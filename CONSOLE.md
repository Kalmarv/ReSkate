# Console commands

**~** (grave/tilde) opens the console. This page lists what it takes. The descriptions are the ones the
console shows, from the code that registers each command (`Extension/*/…commands.cpp`).

## Finding your way

- `help` shows how the console works. `help <command>` shows a command's syntax, its current value,
  the values it takes, and whether `reset` restores it.
- `help <word>` lists the commands that start with that word: `help perf`, `help mp`, `help trainer`,
  `help grindexit`.
- `commands [prefix]` lists every command and variable that starts with the prefix, with its current
  value, for example `commands perf` or `commands WorldRender.`.
- **Tab** completes names and arguments, and shows each suggestion's description.

A **variable** has a value. Enter its name by itself to read it, or follow it with a value to set it:
`noclip 1`, `flyspeed 60`, `tod night`. Variables that are on or off also take `toggle`. In the tables,
`[value]` after a variable's name is the value it takes.

`reset <command>` puts a variable back where it has a restore (`help <command>` says so); `reset
movement`, `reset features`, `reset engine` and `reset all` restore a group of them at once.

Some commands do something else in multiplayer. On a dedicated server, a player whose role allows it
changes the map (`load`), the time of day (`tod`), the world layers (`world …`) and the parks (`park …`)
for everyone. In a lobby the host's parks and layers are used.

## Console

| Command | What it does |
|---|---|
| `help [command]` | Show command syntax, values, restrictions, and availability |
| `commands [prefix]` | List commands and variables, optionally filtered by prefix |
| `clear` | Clear console scrollback |
| `history` | Show recent commands |
| `echo [text]` | Print text to the console |
| `status` | Show current game state |
| `reset <command>` | Restore a variable or a group of session overrides |
| `reset movement` (or `reset debug`), `reset features`, `reset engine`, `reset all` | Restore the selected runtime overrides |
| `log` (or `log status`) | Show logging level and output path |
| `loglevel [trace\|debug\|info\|warning\|error\|critical\|off]` (or `log level`) | Set the shared logging level |
| `log flush` | Flush the log file |
| `ui scale [scale]` | Scale the ReSkate menu and its text |
| `mods apply [reload]` | Apply Mods/ and mods.json now; takes effect at the next level load, or straight away with 'reload' |
| `items [verb]` | Skater item grids: `items fav` (favorite the highlighted item), `items search <text>`, `items filter all\|favorites\|mods\|game`, `items status`; `items peek` and `items poke` are research aids |

## Skater and camera

| Command | What it does |
|---|---|
| `noclip [0\|1]` | Fly the local skater through the world |
| `nobail [0\|1]` | Prevent wipeouts; also activated automatically during noclip |
| `freecam [0\|1]` | Move the camera independently of the skater |
| `firstperson [0\|1]` | See through the skater's eyes |
| `hideui [0\|1]` | Hide the game's interface |
| `flyspeed [speed]` | Free-flight speed in world units per second |
| `freecamfov [degrees]` | Freecam field of view in degrees (40-120; 0 = the game's own) |
| `forwardvelocity [1-300]` | Velocity added along the skater's forward direction |
| `upvelocity [1-25]` | Velocity added along world up |
| `forwardboost` | Add the configured velocity along the skater's forward direction |
| `freecam_controller <0\|1>` | Block input and use controller for Freecam |
| `tp_to_freecam` | Teleport to freecam position and disable freecam |
| `bind <name> <button_or_key>` | Bind a controller combo or keyboard key; 0 clears the binding. Names: `noclip`, `freecam`, `freecamcontroller`, `tptofreecam`, `forwardvelocity`, `upvelocity`, `offboardupvelocity`, `voteyes`, `voteno`, `firstperson`, `hidehud`, `voicechat`, `tod`, `nobail`, `challenges`, `nametags`, `nametagdots` |

The movement variables also answer to `debug.<name>` (`debug.noclip`, `debug.camera_speed`, …).

## Trainer

Physics tuning, presets, markers and teleports. More in [Extension/Trainer/README.md](Extension/Trainer/README.md).

| Command | What it does |
|---|---|
| `trainer status` | What the trainer is doing |
| `trainer open [tune\|presets\|practice\|map\|realistic\|fun\|everything]` | Open the menu on the trainer page |
| `trainer set <value id> <number>` | Set a physics tuning value live |
| `trainer freeze <value id> 0\|1` | Keep a value through presets and resets |
| `trainer reset <value id>\|all` | Put a value (or all) back to stock |
| `trainer find <words>` | List tuning values whose id contains the words |
| `trainer preset apply\|remove\|save\|delete <name>` | Apply, remove, save or delete a preset |
| `trainer dial <multiplier> <preset name>` | Turn a built-in preset up or down: 1 is the game's own |
| `trainer slot <1-5>` | Select a marker slot |
| `trainer marker save\|go\|clear [slot]` | Save, go to or clear a marker |
| `trainer tp <x> <y> <z>` | Teleport the skater |
| `trainer waypoint [info]` | Teleport to the waypoint placed on the pause map, onto the surface there |
| `trainer ground <x> <z>` | Teleport to the topmost surface at a map position |
| `trainer spot <number>` | Go to one of the map author's spots |
| `trainer option …` | Trainer options: `hud`, `hud_jump`, `auto_return`, `pad`, `log` 0\|1; `return_delay <seconds>`; `hippy_height`, `nocomply_height`, `boneless_height`, `offboard_height`, `flip_speed <x>` |
| `trainer profile set <preset>\|clear` | The preset this map applies on load |
| `trainer jumps` | The last measured jump |
| `trainer where` | The skater's position, heading and speed |
| `trainer states` | Physics states the skater has been in (diagnostic) |
| `trainer classes [find]` | The game's tuning classes the trainer found in memory (diagnostic) |
| `trainer refresh` | Refresh the skater's cached copy of the tuning (diagnostic) |
| `trainer dump` | Write every tuning value to a text file |
| `trainer selftest` | Check the trainer against the running game |

## Gameplay

| Command | What it does |
|---|---|
| `activities [0\|1]` | Enable offline activity features |
| `fasttravel [0\|1]` | Enable fast travel features |
| `progression enabled [0\|1]` | Enable offline progression systems |
| `developermenus [0\|1]` | Enable the available developer menus |
| `boardwear [0\|1]` | Enable the shipped board wear graph |
| `boardwear reset` | Clear accumulated board wear |
| `playercollision [0\|1]` | Allow the game's own party collision option |
| `hallofmeat [0\|1]` | Show the bones a bail hurt, bruised yellow and broken red, and its Meat score ([more](Extension/HallOfMeat/README.md)) |
| `roadrash [0\|1]` | Bails leave marks on your skater that build up: dust, scrapes, bruises, wounds (needs the Road Rash item pack) |
| `roadrashblood [0\|1]` | Fresh blood on Road Rash's worst wounds; off, they are dry and scabbed |
| `roadrashtattoos [0\|1]` | Road Rash's wounds take the place of your own tattoos and moles until you heal; off, a limb that wears a tattoo gets no wounds |
| `roadrash heal` | Take Road Rash's marks off and start counting bails again |
| `roadrash bails <count>` | Count as if you had bailed this often, to look at a stage (10, 14 and 20 are the steps) |
| `boardbreak [0\|1]` | A hard slam can snap your board at the nose, in the middle or at the tail (needs one of the Board Break deck packs) |
| `boardbreak deck [name\|number\|next]` | Which deck every board has; with nothing after it, the decks there are |
| `boardbreak snap [nose\|mid\|tail]` | Break your board now, to look at a break; it stays broken until `boardbreak mend` or a respawn |
| `boardbreak mend` | Make a broken board whole again |
| `grindexit exits on\|off\|status` | Grind exit triggers: LT/RT pressed while grinding pick the side you pop out to (kept) ([more](Extension/Skater/GRIND_EXITS.md)) |
| `grindexit stance goofy\|regular` | Your skater's stance, for toe/heel sides (kept) |
| `grindexit sides toeheel\|travel` | LT toe / RT heel, or LT left / RT right of travel (kept) |
| `grindexit strength <10-100>` | How far to the side a trigger exit pops, percent of the full exit (kept; 100 is the game's own) |
| `grindexit guard on\|off` | Hide a trigger used to pick the side from the game until released (no accidental grab) |
| `grindexit leanguard on\|off` | The left stick's left/right reaches the game at the pop while a trigger picks the side |
| `grindexit prewindow <seconds>` | Seconds a trigger may be pressed before the grind connects and still count (1) |
| `grindexit sidespeed <m/s>` | Sideways speed of a chosen exit when the game had none (m/s, game's own 1.8) |
| `ai spawn [count] [physics] [brain]` | Experimental: spawn native AI skaters beside your skater; results are logged |
| `ai brain <0\|1>` | Start (1) or pause (0) the brain tree of every AI skater |
| `ai clear` | Remove every spawned AI skater |
| `ai status` | Log skater sources, the AI blueprint and each AI skater's state |

The feature switches also answer to `feature.<name>` (`feature.fast_travel`, …), and the `ai` commands to
`ai.<name>`.

## World

| Command | What it does |
|---|---|
| `levels` | List destinations and their one-based indices |
| `load <destination\|index> [start\|-]` | Load a destination with the base root supplied automatically; Tab offers the start points |
| `tod [default\|morning\|noon\|afternoon\|evening\|night\|weatherday\|weathernight\|next]` | Time of day on the current map |
| `world <layer> [default\|on\|off]` | One for each world layer of the installed levels, with its description; `commands world` lists them |
| `world defaults <map>` | Restore authored world layers for a map |
| `park random` | Load a random layout in every park slot |
| `park random-on-launch [0\|1]` | Randomize local parks on the next game launch |
| `park construction\|historic\|financial [layout\|empty]` | Choose the layout for that park slot |
| `traffic [-1\|0\|1\|2\|3]` | Traffic density: -1 default, 0 off, 1 light, 2 normal, 3 busy |
| `pedestrians [-1\|0\|1\|2\|3]` | Pedestrian density: -1 default, 0 off, 1 light, 2 normal, 3 busy |
| `fog [-1\|0\|1]` | Fog: -1 default, 0 off, 1 on |
| `fog_distance [value]` | Fog visibility multiplier; 0.1 to 5, or -1 for default |
| `sky_brightness [value]` | Sky brightness multiplier; 0 to 3, or -1 for default |
| `clouds [value]` | Cloud opacity multiplier; 0 to 2, or -1 for default |
| `wind_strength [value]` | Wind strength; 0 to 30, or -1 for default |
| `wind_direction [value]` | Wind direction in degrees; 0 to 360, or -1 for default |
| `Fog.<field>`, `Sky.<field>`, `Wind.<field>` `[value\|default]` | The level's atmosphere settings one by one (214 of them); `commands Fog.` lists them |
| `Fog.reset`, `Sky.reset`, `Wind.reset` | Restore this group's authored values |
| `environment reset_environment` | Restore all authored atmosphere settings |

The world controls also answer to `environment <name>` (`environment fog`, …).

## Graphics

| Command | What it does |
|---|---|
| `filmgrain [-1\|0\|1]` | Film grain effect |
| `vignette [-1\|0\|1]` | Darkening around the screen edges |
| `chromaticaberration [-1\|0\|1]` | Color fringing at screen edges |
| `graphics reset` | Restore all authored graphics effects |

-1 is the game's own choice. They also answer to `graphics film_grain`, `graphics vignette` and
`graphics chromatic_aberration`.

## Progression

| Command | What it does |
|---|---|
| `challenges [0\|1]` | Show or hide challenges while keeping saved progress |
| `maxranks [0\|1]` | Automatically maximize district ranks |
| `progression busstop <stop> <0\|1\|2>` | Save bus stop state: 0 hidden, 1 visible, 2 unlocked |
| `progression district <district> <rank>` | Save a district rank and disable automatic max ranks |
| `ripscore <score> <cap> <level>` | Save RIP score, score cap, and level |
| `mission complete <id>`, `mission reset <id>` | Save, or reset, mission completion |
| `cardname [name]` | Set the name on your player card; multiplayer always uses your Steam name |

## Objects and the park editor

| Command | What it does |
|---|---|
| `objects enabled [0\|1]` (or `objectpersistence`) | Persist placed objects between sessions |
| `objects clear <map>` | Clear placed objects from the specified map |
| `objects delete <map> <row_token>` | Delete one placed object using its current row token |
| `objects tp <map> <row_token>` | Teleport to a placed object using its current row token |
| `parkeditor [0\|1]` | Open the fullscreen park editor |
| `editor <operation> <map> <generation> <revision> …` | What the park editor's buttons send: `place`, `move`, `delete`, `new`, `undo`, `redo`, `mod-open`, `mod-load`, `mod-save`, `mod-create`, `mod-details`, `mod-convert`, `mods-refresh` |

## Multiplayer

Each `mp` command also answers to `multiplayer.<name>`.

| Command | What it does |
|---|---|
| `mp host [code\|public] [players]` | Host a Steam lobby for 2-32 players |
| `mp join <join_code>` | Join using the host's complete session code |
| `mp browse` | Refresh the public Steam lobby browser |
| `mp join-lobby <lobby_id>` | Join a lobby selected in the server browser |
| `mp stop` | Disconnect and remove the remote skater |
| `mp status` | Show multiplayer connection and animation status |
| `mp chat <message>` | Send a text message to everyone in the session (T opens the chat box) |
| `mp server <command>` | Send a command to the dedicated server you are an admin of (try: `mp server help`) |
| `mp vote <yes\|no\|1\|2…>` | Answer the vote a dedicated server is running (yes, no), or its poll (1, 2...) |
| `mp party <…>` | Your party in a session: invite\|join\|kick\|promote <player>, accept\|decline [player], leave, open, close, status |
| `mp tp <player\|x y z>` | Teleport to a player (name start) or to x y z |
| `mp tpall` | Host or server admin: teleport everyone to you |
| `mp tphere <player>` | Host or server admin: teleport one player to you |
| `mp nametags <on\|off\|toggle>` | Show or hide player nametags |
| `mp nametags-friends <on\|off\|toggle>` | Only your Steam friends have nametags |
| `mp nametag-dots <on\|off\|toggle>` | Show far and off-screen players as dots |
| `mp nametag-distance <metres>` | How far away a player's name still shows, in metres (10-500); past it they are a dot |
| `mp player-distance <metres>` | How far away another player still gets a skater, in metres (50-1000; 1000: every player). Past it: a nametag or dot |
| `mp chat-bubbles <on\|off\|toggle>` | Show or hide chat bubbles above skaters |
| `mp chat-bubbles-own <on\|off\|toggle>` | Also show your own chat messages above your skater |
| `mp chat-bubbles-distance <metres>` | How far away a player may be and still show a chat bubble, in metres (5-500) |
| `mp chat-bubbles-duration <seconds>` | How many seconds a chat bubble stays before it fades (1-30) |
| `mp chat-bubbles-history <lines>` | How many recent messages stack above a skater (1-8) |
| `mp voice-chat <on\|off\|toggle>` | Your own voice chat |
| `mp direct-connections <on\|off\|toggle>` | Connect straight to servers that offer it, not through Steam's relays |
| `mp score-check <on\|off\|toggle>` | Host: keep players whose mods change scoring or physics out of throwdowns and coop challenges (on by default) |
| `mp objects-solid <SteamID64\|server> <on\|off>` | Make a player's placed objects solid for you, or ghosts you pass through |
| `mp map-markers [on\|off]` | Named, selectable markers for your party members on the pause map (default); off: the game's plain player dots |
| `mp collision [on\|off]` | Player collision: show whether other players are solid for your skater, or switch ReSkate's collision capsules on or off |
| `mp puppet-savings [saving] [on\|off]` | Far players' skaters: show the savings, or switch one (driven-placement, ecs-retry, far-components) on or off |
| `mp effects` | Skater effects shared with other players: what has been captured and played |
| `mp retry` | Retry remote skater creation after an error |
| `mp echo`, `mp test`, `mp pose-dump <seconds>` | Tests and research: a delayed remote skater locally, a Steam socket-pair round trip, a recording of your own poses |
| `throwdown-hud [reskate\|game]` | The S.K.A.T.E. throwdown HUD: ReSkate's (default) or the game's own debug text |
| `throwdown-offboard [on\|off]` | S.K.A.T.E.: wait for your turn on foot, off the board (default on) |
| `challenge-invite [on\|off]` | Coop challenges: starting a challenge invites party members near you, and you join theirs (default on) |
| `throwdown …`, `challenge …` | Throwdown and coop challenge labs (research aids) |

A dedicated server has commands of its own for its admins; see [Server/README.txt](Server/README.txt).

## Music

More in [MUSIC-SETUP.md](MUSIC-SETUP.md).

| Command | What it does |
|---|---|
| `music` | The music sources (Spotify, YouTube Music, local files, radio, external audio): what is on, what is playing, and the Spotify account |
| `music setup` | Open the page in your browser that sets the music sources up |
| `music spotify client <id>` | The client id of your Spotify application (its redirect address: http://127.0.0.1:47824/callback) |
| `music spotify login` | Sign in to Spotify in your browser, for your playlists in the music screen |
| `music spotify logout` | Forget the Spotify sign-in |
| `music through [0-2]` | The music sources' sound through the game: 0 off, 1 in place of songs, 2 in place of every stereo sound file |
| `music gain [percent]` | How loud the music sources are through the game, in percent (100 as it is, up to 1600) |

## Performance and diagnostics

| Command | What it does |
|---|---|
| `textures` | Streamed textures in use, and the most the game can hold |
| `perf` | Show or hide the performance HUD |
| `perf hud [on\|off]` | Performance HUD: client update, frame time, ReSkate's cost, CPU per thread |
| `perf status` | Client update and frame timing, ReSkate's zones, CPU per thread |
| `perf window [on\|off]` | The profiler window (shown while the menu or console is open) |
| `perf sample [seconds] [client\|present\|all\|<thread id>] [rate]` | Sample stacks: the client update thread (default, 10 s at 1000 Hz), present, all busy threads, or a thread id |
| `perf stop` | Stop the running sample early (it still reports) |
| `perf report` | The last sample's busiest functions and its report files |
| `perf memory [mark\|trace\|stop\|big]` | The game's memory by allocation; `mark` remembers it and the next `perf memory` says what was added since; `trace` also says who asked for the blocks, until `stop`; `big` lists the largest blocks |
| `perf tweaks` | What each engine tweak is doing |
| `perf gi [updates per second]` | Throttle Enlighten GI's per-frame update to this rate (0 = every frame, stock; try 60) |
| `perf meshtree [on\|off]` | Cull meshes through the octree the game ships off (MeshCullTreeEnabled) |
| `perf jobwake [microseconds]` | Idle job workers wait this long where a push wakes them cheaply (0 = stock) |
| `perf mainsleep [on\|off]` | Pace the main loop on a high-resolution timer with a 0.25 ms spin instead of a 1 ms one (on by default) |
| `perf dirtyskip [on\|off]` | Skip the per-section spinlock that clears dirty bits when none are set (on by default) |
| `perf jobspin [microseconds]` | How long idle engine job threads poll for work before sleeping (ReSkate 50 us, stock 250 us) |
| `perf uipointer [on\|off]` | Skip the UI's per-frame mouse hit-test while the game shows no cursor (on by default) |
| `popup test` | Show the message a kicked player sees, to try answering it |
| `ui dump [all\|roots\|0x<schema>\|text] [delay_seconds]` | Write the live native UI model tree to the game's logs folder (research aid) |

None of the perf settings are saved.

## Engine settings

The console also reaches the game's own settings, by name:

- **The gameplay settings** that the Gameplay switches above are made of, as `0|1` variables:
  `ForceAllActivitiesEnabled`, `EnableQuestSystem`, `FastTravelPointsEnabled`, `EnablePlayerProgression`,
  `EnableDevOnlyMenu`, `BoardWearEnabled`, `PlayerVsPlayerCollisionEnabled` and the others in
  [gameplay_settings_internal.h](Extension/Settings/gameplay_settings_internal.h). Each also answers to
  its settings class and name, for example `DingoActivitySettings.EnableQuestSystem`.
- **The settings in the game's own settings registry** (some 5,900 in this build), as variables named
  `<group>.<setting>`: `WorldRender.…`, `PostProcess.…`, `Render.…`, `TextureStreaming.…`,
  `PerfOverlay.…` and so on. They have no description of their own; `help <name>` gives the type and
  the current value, and `commands TextureStreaming.` lists a group. A change lasts for the session, and
  `reset <name>` or `reset engine` undoes it. When a multiplayer session starts, changes to settings
  that change how the game plays are put back.

  The console is built before the game has registered all of its settings. ReSkate keeps the names
  it has seen in `%LOCALAPPDATA%\ReSkate\cache\<build>\engine-settings.txt`, so a setting the game
  registers late is a command from the next launch.
