# Lights: a level with more lights of one kind than the game's table holds no longer crashes it

> **Not tested in game yet.** Built and unit-tested offline only. This file is the full PR description, kept on the branch for review; drop this commit before sending upstream.

## What

A guard in the style of `shader_permutation_guard` (2.0.9). When the game is about to make a
light while that kind's table is (nearly) full, the light entity is not made, so the light is
not drawn. Without the guard the game writes past the end of the table and crashes. A warning
in `ReSkate.log` names the kind of light and how many were left out (the first 8 of each kind,
then every 1000th):

```
Lights: the level has more PbrSpotLight lights than the game's table of them holds (8,192); this one
is not made and is not drawn, where the game would have crashed (1 left out since the game started).
A light left out stays out until its part of the level is loaded again.
```

Nothing else changes. Below the limit, the six hooked functions call the game's own straight
away after two reads.

## Why

The game keeps each of its six kinds of light (OldPointLight, OldSpotLight, PbrSphereLight,
PbrSpotLight, PbrTubeLight, PbrRectangularLight) in its own manager, with its own ID allocator
and its own tables. Every table is made with exactly 64 pages of 128 entries, 8,192 IDs
(`0x304c2d0` makes all 64 pages, and nothing adds pages later). The allocator (`0x3e844f0`)
hands out fresh IDs from a counter with no capacity check. The insert (`0x4ea3ff0` for
PbrSpotLight, with five siblings) stores at `page[id >> 7][id & 127]` with no bounds check:

- IDs 8,192 to 8,319 take the table's snapshot pointer (`table + 0x200`) for a page and write
  into the snapshot object. That corrupts memory silently.
- From 8,320 on, the "page" is whatever the fields behind it hold. It was null in every crash we
  looked at: an access violation at `Skate.exe+0x4ea4094`.

A custom level reaches this with enough lights of one kind. Our GTA V port has up to 23,056
PbrSpotLights in one time-of-day layer. A level just under the limit can also reach it for a
moment when two of its time-of-day layers are loaded together during a switch (see "What changes
for players"). Evidence from crash dumps (build 20260929): the faulting ID was 8,320 (0x2080) in
two dumps, PbrSpotLight, selector 2. A hook on the insert's entry saw inserts at 8,192 and up,
then the crash.

## How

**Why not at the insert.** Skipping the store, or giving the entity no light, is not safe. The
light entity keeps the handle the renderer gave it (`(class << 29) | (id + 2)`) and later changes
its light through it without checking it. For example, the PbrSpotLight entity's property update
`0x52d11b0` calls the renderer's per-class setters (vtable `0x666fc90`, `+0x60` on), and those
take the ID from the handle whatever the class bits say. They look the light up in
`snapshot[id >> 7][id & 127]` and insert a copy-on-write record at that ID. So a skipped insert is
read back as garbage on the light's next change. A handle of 0 becomes ID `0xfffffffe` of
OldPointLight. A handle with an invalid class becomes ID `0xfffffffe` of the class the setter is
for.

**Where.** Each kind has one entity factory (`0x52d3f50`, `0x52d4480`, `0x52d0e00`, `0x52d1740`,
`0x52d1d40`, `0x52d2680`; slot `+8` of the kind's creator, 0xc0-byte entities). The game's own
factory already returns no entity on two paths: a flag on the light, and a realm other than the
renderer's. Both returns come before it allocates or registers anything. The guard returns null
at that same point, so no entity, handle or ID ever exists for a refused light. The caller is the
game's generic entity creation, through the type's creator. Because the game's own factories
already return null there, it must handle null, but that is inferred, not traced (see "Not tested
yet").

**When.** Just before calling the game's factory, the guard reads the kind's allocator:
`*renderer` (`0x77ef140`), plus the manager offset, plus `0xb8`, with the free-list head at
`+0xe8` and the fresh counter at `+0xf0`. The ID itself is taken a little later in the same call
(factory, entity constructor, renderer `0x4ec5ee0`, the kind's creator, then the allocator).
Other threads, and the lights the game requests elsewhere (`0x2924380`, `0x2daea40`, `0x53086b0`,
`0x5ed9a10`, not through these factories), can take IDs in between. So the last IDs stay free
(`Engine/Game/World/light_table_room.h`):

- While the counter is below 8,192 − 256, every light is made.
- Above that, a light is made only while a recycled ID is waiting on the free list, and never
  once the counter is within 128 of the end. Up to 128 lights made at the same instant therefore
  still land inside the table.

Freed IDs do come back (`0x4ecdda0` pushes the retired ID onto the free list). So after a heavy
layer unloads, new lights of that kind are made again from recycled IDs, even though the counter
is a high-water mark.

**Checks.** Before anything is hooked, every link from each factory to its kind's 128-entry pages
is checked byte for byte (`Engine/Game/Build/20260929/light_tables_sites.inc`, 85 instructions):

- the factory → its entity's constructor;
- the load of the renderer pointer;
- the descriptor that names class 0 to 5;
- the renderer → the manager offset → the creator;
- `manager + 0xb8` → the shared allocator → the insert;
- the insert's table lookup and `shr 7`;
- the allocator's free list and counter;
- the 64 pages of 0x400 bytes.

If any of these differs, there is no guard and a warning is logged. All six hooks are enabled in
one Detours transaction (`hook_queue_enable` / `hook_apply_queued`), or none are.

## What changes for players

- **Below the limit:** nothing.
- **A level with more lights of one kind than the table holds:** the game no longer crashes. It
  makes lights of that kind until about 7,936 to 8,064 IDs are in use, and the rest are not
  drawn. Which lights are left out depends on load order: whatever is created after the table
  fills. That can include stock lights loaded after the level's own.
- **A time-of-day switch that holds two layers for a moment.** ReSkate can load the new
  time-of-day layer before the old one has finished unloading. This is a separate matter, not
  changed here. While both are loaded, both layers' lights count against the same 8,192. If they
  don't fit together, the new layer's lights created during the overlap are refused. The old
  layer's IDs do return to the free list once it unloads. But a refused light does not come back
  by itself: its entity was never made, and nothing retries it. It stays missing until that layer
  is loaded again, for example by switching the time of day away and back, or by reloading the
  level. Before this change, the same switch crashed the game. (This comes from reading the code;
  see "Not tested yet".)
- **Logs:** one warning per refused light for the first 8 of each kind, then every 1000th.

## How tested

- Built with the README's steps (`cmake --preset vs2022-x64`, `cmake --build --preset release`)
  with every `DINGOSDK_BUILD_*_TESTS` option on, then ran `ctest`:
  - 88 of 89 tests pass, including the new `light_tables`.
  - `trainer_tuning_names` fails without a game folder (`Unrecognized native envelope in
    Data\layout.toc`). It fails the same way on main (6c45e96).
  - `dingosdk_app_audio_probe` and `dingosdk_external_player_probe` don't link (unresolved
    `audio_dsp::process`, `spotify_device::*`, `youtube_music::*`), on main too. Both are by-hand
    probes, not tests.
  - `ReSkate.dll` builds with no warnings from the new files.
- New test `light_tables` (`Engine/Game/World/Test/light_table_room_tests.cpp`):
  - the room rule, including a sweep over every counter value: whenever the guard says yes, 128
    more fresh IDs still fit;
  - when given a Skate folder (`DINGOSDK_TEST_GAME_ROOT`), all six factory fingerprints and all
    85 sites, checked against `Skate.exe` on disk. Run against the supported build (Skate.exe SHA-256
    `fbce74d5…88b8f9`, the `supported_build::game_sha256`): all 85 sites and 6 fingerprints
    match. As a check that the test fails when it should, a copy of Skate.exe with one byte changed
    (the PbrSpotLight insert's `shr ecx, 7` made `shr ecx, 8`) fails it, naming 0x4ea405d.
- Static analysis of Skate.exe 20260929 (capstone and Ghidra) for every claim above. A second
  person checked the factory/caller contract independently.

## Not tested yet

- **In game.** No game was run for this change. The plan: a level with about 23,000 PbrSpotLights
  in one time-of-day layer, which crashes at `+0x4ea4094` today. Three checks:
  - it loads with the guard;
  - the log shows the warnings;
  - switching time of day back and forth doesn't crash.
- **That the factory's caller handles null.** This is a strong inference, not traced. The game's
  factories already return null on the realm and flag paths, and do no registration on them. The
  call is virtual through the type's creator, so the caller's RVA isn't pinned statically. The
  in-game run should log the return address once.
- Whether 256 kept back is enough for the lights made outside these factories on every map.
- The `light_tables` contract test only runs when `DINGOSDK_TEST_GAME_ROOT` is set.

🤖 Generated with [Claude Code](https://claude.com/claude-code)
