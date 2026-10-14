#pragma once
#include "Engine/Game/Build/fingerprint.h"
#include <array>
#include <cstdint>

// The game's light renderer and its fixed tables of lights (Extension/World/light_table_guard.cpp).
//
// The renderer (one object; `renderer` holds the pointer, its functions in the table at
// 0x666fc90) keeps six kinds of light, each in a manager of its own inside it:
//   +0x60 OldPointLight, +0x570 OldSpotLight, +0xa80 PbrSphereLight, +0xf90 PbrSpotLight,
//   +0x14a0 PbrTubeLight, +0x19b0 PbrRectangularLight
// (the names are those the constructor 0x4eab1a0 gives them). A light entity asks the
// renderer for its light (0x4ec5ee0, by the class in byte +8 of what it hands over), and the
// manager's creator takes an ID from its own allocator at manager+0xb8 (0x3e844f0: one
// recycled from the free list, whose head's low 32 bits are at +0xe8 and 0xffffffff when it is
// empty, else a fresh one from the counter at +0xf0, which nothing checks) and inserts the light
// at that ID (0x4ea3d00 and its five siblings). The insert puts it in page ID >> 7 of the
// table the light's selector names: 64 pages of 128 entries (0x304c2d0 makes all 64 when the
// table is made; nothing adds one later), so 8,192 IDs, 0 to 8,191, for each kind. Nothing
// checks that either. IDs 8,192 to 8,319 take the pointer behind the 64 page pointers (the
// table's snapshot, +0x200) for a page and write into that object; from 8,320 on the "page" is
// whatever the fields behind it hold, null in every crash seen: the write at 0x4ea4094 on a
// level with more than 8,192 lights of a kind (2026-10-13).
//
// Not making the light at the insert, or giving the entity no light, is no way out: the entity
// keeps the handle the renderer gave it (ID + 2, the class in its top three bits) and changes
// its light through it without asking whether there is one (0x52d11b0, and the renderer's
// functions for one class, from +0x60 of its table, take the ID from the handle whatever the
// class says), so every one of those would go into the table at that ID or at ID 0xfffffffe.
// The guard is one step before: each kind's entity factory (`factories`, slot +8 of the
// kind's creator, 0xc0-byte entities) gives no entity when the game should not make one (a
// flag on the light, or another realm than the renderer's), before it allocates or registers
// anything, and its caller goes on without one. The guard gives no entity the same way, at the
// same point. With no entity there is no handle and no ID.
//
// The ID is taken later, in the same call (factory, entity's constructor, renderer, creator),
// and other threads may take IDs in between, as may the lights the game asks the renderer for
// elsewhere (0x2924380, 0x2daea40, 0x53086b0 and 0x5ed9a10, not through these factories), so the
// guard leaves the last IDs free
// (Engine/Game/World/light_table_room.h). Everything from a factory to its kind's insert is
// checked byte for byte before anything is hooked (light_tables_sites.inc).
namespace dingosdk::game::build::v20260929::light_tables {
inline constexpr std::uintptr_t renderer = 0x77ef140; // the light renderer, null until made
inline constexpr std::uint32_t allocator = 0xb8;      // a manager's ID allocator
inline constexpr std::uint32_t free_head = 0xe8;      // ... the head of its free list (u64)
inline constexpr std::uint32_t issued = 0xf0;         // ... how many fresh IDs it has given (u32)
inline constexpr std::uint32_t pages = 64, page_entries = 128;

struct Kind {
    const char* name;
    std::uint32_t manager; // in the renderer
    Fingerprint factory;
};
// The six factories begin alike; the sites tell them apart.
inline constexpr std::array<unsigned char, 32> factory_head{
    0x40,0x53,0x56,0x57,0x41,0x56,0x48,0x83,0xec,0x58,0x48,0x8b,0x9a,0x40,0x01,0x00,
    0x00,0x48,0x8b,0xfa,0x4c,0x8b,0xf1,0x48,0x8d,0x54,0x24,0x30,0x48,0x8b,0x4f,0x10};
// By the class the renderer is told, 0 to 5.
inline constexpr Kind kinds[]{
    {"OldPointLight", 0x60, {0x52d3f50, factory_head}},
    {"OldSpotLight", 0x570, {0x52d4480, factory_head}},
    {"PbrSphereLight", 0xa80, {0x52d0e00, factory_head}},
    {"PbrSpotLight", 0xf90, {0x52d1740, factory_head}},
    {"PbrTubeLight", 0x14a0, {0x52d1d40, factory_head}},
    {"PbrRectangularLight", 0x19b0, {0x52d2680, factory_head}},
};

// For each kind: the factory makes its entity, whose constructor asks the renderer (through
// `renderer`) for a light of that class; the renderer hands the class to its manager, whose
// creator takes an ID from manager+0xb8 and inserts the light at it, 128 entries a page; and
// the allocator and the 64 pages themselves.
struct Site {
    std::uint32_t rva;
    std::uint8_t size;
    std::uint8_t bytes[15];
};
inline constexpr Site sites[]{
#include "light_tables_sites.inc"
};
}
