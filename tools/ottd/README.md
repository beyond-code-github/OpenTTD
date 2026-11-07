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
| `start [--save PATH]` | `ottd start --save /x/junction.sav` | new 64x64 flat game (fixed seed 3, env `OTTD_RIG_SEED`), or load a save; waits for the admin port |
| `stop` | `ottd stop` | SIGTERM via `server.pid` |
| `status` | `ottd status` | date, companies, money |
| `rcon "<cmd>"` | `ottd rcon "getdate"` | prints the console output |
| `gs '<json>'` | `ottd gs '{"cmd":"ping"}'` | `id` is added for you; prints the compact JSON reply |
| `company` | `ottd company` | starts `IdleAI` if there is no company, then tells the GS which id to act as |
| `build-line X1 Y1 X2 Y2` | `ottd build-line 10 20 30 20` | straight rail, first available rail type, inclusive tiles |
| `depot X Y DIR` | `ottd depot 9 20 SW` | DIR = way the entrance faces: NE x-1, SE y+1, SW x+1, NW y-1 |
| `train DEPOT_X DEPOT_Y [--engine ID] [--wagons N]` | `ottd train 9 20 --wagons 2` | fastest buildable engine by default; prints the vehicle id |
| `orders VEHICLE X Y X Y` | `ottd orders 0 9 20 31 20` | two go-to-depot orders |
| `go VEHICLE` | `ottd go 0` | starts the train if not running |
| `vehicles` | `ottd vehicles` | `id @x,y speed km/h state` per train |
| `watch VEHICLE --seconds N` | `ottd watch 0 --seconds 20` | samples about every 0.5 s; min/max/avg speed, whether it hit 0 |

GameScript commands (`ottd gs`): `ping`, `company_set {company}`, `build_rail {x1,y1,x2,y2}`, `build_depot {x,y,dir}`, `build_train {depot_x,depot_y,engine?,wagons?}`, `orders_depots {vehicle,x1,y1,x2,y2}`, `start {vehicle}`, `vehicles`. Replies over 1400 bytes arrive as `{"id","chunk","of","data"}` parts; `ottd_core.admin` reassembles them.

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
