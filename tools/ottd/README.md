# ottd: OpenTTD test rig CLI

Drives a headless dedicated OpenTTD through the admin port and the `JunctionTestGS` GameScript. Python 3 stdlib only. Output is terse text; any failure prints `FAIL: ...` to stderr and exits 1.

```sh
O=tools/ottd/ottd
$O start; $O company
$O build-line 10 20 30 20; $O depot 9 20 SW; $O depot 31 20 NE
$O train 9 20 --wagons 2; $O orders 0 9 20 31 20; $O go 0
$O watch 0 --seconds 20; $O stop
```

## Commands

| Command | Example | Notes |
|---|---|---|
| `start [--save PATH] [--hilly] [--nuts]` | `ottd start --save /x/junction.sav` | new 64x64 flat game (fixed seed 3, env `OTTD_RIG_SEED`), or load a save; waits for the admin port. `--nuts` loads the NUTS 0.8.4 newgrf (grfid and md5 computed from the tar in `~/.local/share/openttd/content_download/newgrf/`) and starts in 2057. Env `OTTD_RIG_PORT` (admin, default 3977) and `OTTD_RIG_GAME_PORT` (game, default 3979) pick the ports |
| `stop` | `ottd stop` | SIGTERM via `server.pid` |
| `status` | `ottd status` | date, companies, money |
| `rcon "<cmd>"` | `ottd rcon "getdate"` | prints the console output |
| `gs '<json>'` | `ottd gs '{"cmd":"ping"}'` | `id` is added for you; prints the compact JSON reply |
| `company [--id N]` | `ottd company --id 0` | tells the GS which company to act as; without `--id`, starts `IdleAI` if there is no company and uses it. `--id` needs the company to exist already (e.g. after `start --save`) |
| `build-line X1 Y1 X2 Y2` | `ottd build-line 10 20 30 20` | straight rail, first available rail type, inclusive tiles |
| `depot X Y DIR` | `ottd depot 9 20 SW` | DIR = way the entrance faces: NE x-1, SE y+1, SW x+1, NW y-1 |
| `train DEPOT_X DEPOT_Y [--engine ID] [--wagons N] [--design]` | `ottd train 9 20 --wagons 2` | fastest buildable engine by default; prints the vehicle id. `--design` builds the design train (needs `start --nuts`): 2x "Mastermind (Electric)" + 6x "3rd gen Universal Wagon (Rail)" |
| `orders VEHICLE X Y X Y` | `ottd orders 0 9 20 31 20` | two go-to-depot orders |
| `go VEHICLE` | `ottd go 0` | starts the train if not running |
| `build-layout FILE X Y` | `ottd build-layout layouts/test-1a.json 20 20` | builds a layout JSON with its north corner at tile X,Y: tracks, then bridges (fastest rail bridge for the length), then signals. Prints `built=N failed=M` and one line per failure (layout-relative x,y); exits 1 on failures |
| `sample V [V...] --ticks N` | `ottd sample 0 --ticks 600` | per-game-tick speed sampling inside the GS: per vehicle n, min, max, avg (integer km/h), `stops` (transitions to speed 0), then `drops` (speed fell 10 km/h or more within 8 ticks; one entry per event, tile where it began) |
| `speed-trace VEHICLE --ticks N` | `ottd speed-trace 0 --ticks 1500` | the vehicle's speed (km/h) at every game tick for N ticks starting when the command arrives; one `tick speed` line per tick |
| `junction-check FILE [--trains N] [--ticks T] [--find-clear \| X Y]` | `ottd junction-check layouts/circuit-LR-TL4.json --find-clear` | sets the loan to max (`loan_max`), builds FILE, builds its `depots` (layout JSON field `[{x,y,dir}]`, relative to the origin), buys N trains (default 6, 3 wagons) in the first depot, starting one about 300 ticks after the previous, no orders, samples T ticks (default 7400), prints one line per train, then `OK` or `DEADLOCK?` (every train at speed 0 on the same tile in two `vehicles` snapshots 1000 ticks apart). Exit 0 OK, 2 DEADLOCK?, 1 build failure |
| `junction-here X Y [--rot N] [--mirror] [--ml M] [--branch B] [--tl N] [--near] [--instant] [--prio TILES]` | `ottd junction-here 20 20 --prio 7` | generates a junction at tile X,Y through the rcon `junction_export` (always with the D138 design train) and builds it; `--prio` sets the coverage by hand, non-strict (D137a), otherwise it is sized; prints ok, error or failed |
| `junction-scan --count N [--seed S] [--near] [--instant]` | `ottd junction-scan --count 20` | runs `junction-here` at N random spots and prints totals: generated, no layout, built ok, build failures, rig errors |
| `traffic FILE [--trains N] [--wagons W] [--warmup DAYS] [--ticks T] [--every K] [--find-clear \| X Y]` | `ottd traffic layouts/circuit-LR-TL4.json --find-clear --trains 12` | builds the circuit as `junction-check` does (N trains default 8, W wagons default 3), waits DAYS (default 20) so the trains spread out, then samples every K ticks (default 4) for T ticks (default 20000) with the GS `occupancy` command. Prints an ASCII heatmap of the layout box (layout-relative; digit 1-9 = stopped samples scaled to the busiest tile, `+` slow only (under 80 km/h), `#` track or bridge head, `=` bridge middle), the top 10 stopped tiles, stop events per train, writes the raw reply and parameters to `RUNDIR/traffic-<time>.json`, then the deadlock verdict (`OK` or `DEADLOCK?`, exit 0 or 2) |
| `vehicles` | `ottd vehicles` | `id @x,y speed km/h state` per train |
| `watch VEHICLE --seconds N` | `ottd watch 0 --seconds 20` | samples about every 0.5 s; min/max/avg speed, whether it hit 0 |

GameScript commands (`ottd gs`): `occupancy {vehicles:[ids],ticks:T,every?:4,slow?:80}` (reply `{samples,stopped:[[x,y,n]],slow:[[x,y,n]],stops:[[id,count]]}`), `speed_trace {vehicle,ticks}` (reply `{speeds:[km/h per tick]}`), `ping`, `loan_max` (company mode: sets the loan to the maximum, replies `{balance}`), `company_set {company}`, `build_rail {x1,y1,x2,y2}`, `build_depot {x,y,dir}`, `build_train {depot_x,depot_y,engine?,wagons?,engine_name?,engines?,wagon_name?}`, `orders_depots {vehicle,x1,y1,x2,y2}`, `start {vehicle}`, `vehicles`, `build_layout {origin_x,origin_y,layout}` (reply `{built,failed:[{what,x,y,error}]}`), `sample {vehicles:[ids],ticks:N}` (reply `{vehicles:[{id,n,min,max,avg,stops,drops:[{tile_x,tile_y,from,to}]}]}`). The CLI splits a large layout into several `build_layout` requests (about 1200 bytes each, tracks then bridges then signals), since one admin packet cannot carry a big layout. Replies over 1400 bytes arrive as `{"id","chunk","of","data"}` parts; `ottd_core.admin` reassembles them.

## Layout JSON

Mirrors `JunctionLayout` in `src/junction/junction_layout.h`. Coordinates are relative to the layout's north corner. Fixture: `layouts/test-1a.json` (`MakeTestJunctionLayout`).

```json
{"size":[8,6],
 "tracks":[{"x":0,"y":2,"bits":["X"]}],
 "bridges":[{"start":[4,0],"end":[4,4]}],
 "signals":[{"x":1,"y":2,"facing":"X_SW","type":"block"}]}
```

- Track names: `X`, `Y`, `Upper`, `Lower`, `Left`, `Right`.
- `facing`: an OpenTTD trackdir, `X_NE`, `Y_SE`, `Upper_E`, `Lower_E`, `Left_S`, `Right_S`, `X_SW`, `Y_NW`, `Upper_W`, `Lower_W`, `Left_N`, `Right_N`. A train travelling along `facing` sees the signal (`SignalAlongTrackdir`). Every signal is one-way, in that direction.
- `type`: `block`, `entry`, `exit`, `combo`, `path`, `path_oneway`.
- Signals are placed via `ScriptRail.BuildSignal(tile, front, type)`, where `front` is the neighbour the train comes from. The GS refuses (and reports) a signal whose track is not the first present track of that front row, and checks after building that the engine reads back the same one-way signal.

## Saves

`ottd rcon "save NAME"` writes `<run dir>/save/NAME.sav`. Resume with `ottd start --save <run dir>/save/NAME.sav`; the GameScript restarts with the game (ping answers), company 0 still exists, so run `ottd company --id 0` (not plain `ottd company`).

## Flat ground

New games are generated flat by the `openttd.cfg` that `start` writes: `difficulty.terrain_type = 5` (Custom) with `game_creation.custom_terrain_type = 1` (peak height 1), `tgen_smoothness = 0` (very smooth), `amount_of_rivers = 0`, `variety = 0`, `difficulty.quantity_sea_lakes = 4` (custom) with `custom_sea_level = 1` (1 percent, the minimum), `water_borders = 0`, `water_border_presets = 0`. All land is then height 1 and sloped tiles occur only on coasts. Seed 3 (the default): the only sloped tiles are the outer map-edge ring and 6 tiles at (61..62, 58..62). Another `OTTD_RIG_SEED` is not guaranteed flat; settings cannot rule out a coast.

## Layout

- `ottd`: the CLI. `ottd_core/admin.py` (admin client), `server.py` (lifecycle), `format.py` (output).
- `gs/`: `JunctionTestGS`, copied to `~/.local/share/openttd/game/JunctionTestGS/` on every `start`.
- `ai/IdleAI/`: copied to `~/.local/share/openttd/ai/IdleAI/` on every `start`. It sleeps forever; it exists only to give the game a company (`start_ai IdleAI`), because a dedicated server has none. The GS then builds as that company with `ScriptCompanyMode`.
- Run dir (default `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/ottd-rig/`, env `OTTD_RIG_DIR`): `openttd.cfg`, `secrets.cfg` (both rewritten on each start), `server.pid`, `server.log`, plus the game's own `save/ screenshot/ ...`.
- Game binary: `/home/pete/OpenTTD/build/openttd-spike` (env `OTTD_RIG_BIN`). Admin port 3977 on 127.0.0.1, password `rigpw`.

## Gotchas (from README-spike.md and this build)

- `-c <file>` makes the process cwd the config dir, so the server runs with cwd = run dir.
- `openttd.cfg` needs `[version] ini_version = 8`, or `secrets.cfg` is ignored and the admin port silently never opens.
- The game rewrites its cfg files on exit: edit only while stopped.
- Screenshots: only `screenshot minimap` and `heightmap` work headless. The rcon reply is empty; look in `<rundir>/screenshot/`.
- The game runs with no client connected; the date moves about a day per second of wall time.
- Dedicated servers need `[ai] ai_in_multiplayer = true` for `start_ai` (written by `start`).
- GS company ids are 0-based; `rcon companies` prints 1-based. The GS forgets the company on restart: run `ottd company` after every `start`.
- A fixed seed is used, but not every seed has clear land: `build-line 10 20 30 20` works on seed 3 (seeds 2 and 5 hit obstacles). Pick lines accordingly.
- GS DoCommands each take at least a tick, so large builds are slow and block later requests.
- Train `watch` starts at speed 0 while accelerating, so `stopped=yes` is normal at the start.

## Test circuit

Hidden test `[.circuit]` in `src/tests/junction/legacy/junction_design.cpp`: `JUNCTION_CIRCUIT=tools/ottd/layouts/circuit-LR-TL4.json build/openttd_test "[.circuit]"`. It takes `GenerateJunction(MakeTestJunctionSpec())`, routes a turnaround loop beyond each of the three leg ends (boxes include the edge row or column of the port), puts one-way block signals along the loops, and adds a depot beside the west loop (a half-piece track on its outermost axis piece, depot on the free neighbour). `start` sets `[difficulty] max_loan = 2000000000` so the IdleAI company can pay for it.
