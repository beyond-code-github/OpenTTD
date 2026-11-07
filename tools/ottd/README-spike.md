# Admin-port + GameScript spike

Proves a stdlib-only Python client can drive a running dedicated OpenTTD through the admin port and a GameScript. See `docs/junction-tool/RECON-mcp.md` section 6.

Files: `spike.py` (client), `gs/info.nut` + `gs/main.nut` (GameScript `JunctionSpikeGS`).

## Run it

Pick a scratch dir `$D` (it becomes the game's config and data dir, see "Findings" below).

```sh
D=/tmp/ottd-spike; mkdir -p $D ~/.local/share/openttd/game/JunctionSpikeGS
cp tools/ottd/gs/*.nut ~/.local/share/openttd/game/JunctionSpikeGS/   # the GS must be in a folder the game searches

printf '[version]\nini_version = 8\n[network]\nserver_admin_port = 3977\nallow_insecure_admin_login = true\n[game_scripts]\nJunctionSpikeGS =\n' > $D/openttd.cfg
printf '[network]\nadmin_password = spikepw\n' > $D/secrets.cfg

cd $D && nohup /path/to/openttd -D 127.0.0.1 -c $D/openttd.cfg -g > $D/server.log 2>&1 &
# wait ~6 s, then (twice, to prove per-call connect):
OTTD_SPIKE_DIR=$D python3 tools/ottd/spike.py
OTTD_SPIKE_DIR=$D python3 tools/ottd/spike.py
# stop: kill the openttd pid
```

The spike run here put the GameScript in `~/.local/share/openttd/game/JunctionSpikeGS/` (the only user-dir change). `-D 127.0.0.1` binds game and admin ports to localhost only.

`spike.py` options: `--host`, `--port`, `--password`, `--screenshot-dir`.

## What worked

- Insecure join (`AdminJoin`), rcon `getdate`, rcon `screenshot minimap`, a GS JSON round trip, and `AdminQuit`. Two consecutive runs, each a new connection, both succeeded.
- GS reply example: `{'date': 712246, 'id': 1, 'pong': True}`. The request must first register `Gamescript` at `Automatic` frequency (`AdminUpdateFrequency` packet, type 9, freq bit 1<<6); the spike does this on every call and matches replies by `id`.
- The GS is selected for new games by `[game_scripts] JunctionSpikeGS =` in `openttd.cfg`. `-g` starts a new random game with it. API version `"16"` in `info.nut` was accepted.

## Answers

- **`screenshot minimap` on a dedicated server: works.** It writes `screenshot/screenshot.png` (then `screenshot#1.png`, ...), a 256x256 PNG of a real map. `screenshot heightmap` works too.
- **`screenshot` (normal), `giant`, `big`, `viewport`: fail** on `-D` (server console prints `Screenshot failed!`; no file). They need a viewport. So the open question in RECON section 3 is answered: only minimap and heightmap render headless; for real views a GUI listen server is needed.
- **Screenshot rcon replies are empty.** The success/failure text goes to the server's own console (the log), not back through rcon. A client can only learn the result by watching the `screenshot/` folder, as `spike.py` does. The file name is auto-numbered unless one is given on the command line.
- **Connect latency:** TCP connect plus join to the Welcome packet took 32-51 ms per call (localhost, loopback in WSL2). A whole run (join, 3 rcon, GS ping) is well under a second. Per-call connect is viable.
- The game runs while no client is connected (the date advanced about one day per spike call; no autopause on this dedicated setup).

## Contradictions and surprises vs RECON-mcp.md

- **`-c` changes where `secrets.cfg` (and `private.cfg`, `hs.dat`, ...) live.** With `-c <file>`, the config dir becomes the process's working directory, not the directory of `<file>`. Start the server with `cd $D`, where `$D` also holds `openttd.cfg`, or the game reads `secrets.cfg` from the cwd and not from `-c`'s directory. It also created `ai/ game/ save/ screenshot/ ...` subfolders in the cwd. RECON says nothing about this.
- **`openttd.cfg` needs `[version] ini_version = 8`** (tested). Without it the game treats the config as pre-1.x format, ignores `secrets.cfg`, and the admin port never opens (silently: no log line). The version is read from `openttd.cfg` only. RECON says only "admin_password in secrets.cfg". The game also rewrites all cfg files at exit, so edit them only while the server is stopped.
- **Packet numbering:** `ServerRemoteConsoleCommand` is 120, `ServerGameScript` 124, `ServerRemoteConsoleCommandEnd` 125 (counting the enum in `tcp_admin.h`, `ServerFull = 100`). Wire format is `uint16 length (incl. 3-byte header) | uint8 type | payload`, little endian, strings NUL terminated.
- RECON's "probably `normal` should work headless" is wrong: it fails.
- Nothing contradicted: auth, GS event queue, JSON ping, `getdate`, and multiple admin connections all worked as RECON predicted.
