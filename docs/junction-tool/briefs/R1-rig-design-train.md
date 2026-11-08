# R1: the rig builds the design train (NUTS Mastermind consist)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Work **only** in the worktree `/home/pete/OpenTTD/.claude/play`. Another agent is building in `/home/pete/OpenTTD`; don't touch it.
- First: `git -C /home/pete/OpenTTD/.claude/play checkout --detach <head given in your launch prompt>`. Commit on detached HEAD and report the hash; the orchestrator cherry-picks it. Build nothing unless a gate says so (no C++ changes in this step).
- Decision D138 (`docs/junction-tool/DECISIONS.md`): the design train is NUTS 0.8.4 (grfid `48410101`), **2× "Mastermind (Electric)" (front engine, then a second engine) + 6× "3rd gen Universal Wagon"**, 14,194 hp, 1,090 kN, max speed 122 mph, Mastermind built 2057.
- A rig server (pid from `pgrep -af openttd`, run dir `.../scratchpad/ottd-rig/`, ports 3977 admin / 3979 game) is running and must stay running. Use your own run dir and ports.
- Scratchpad: `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad` (below `$S`).

## Read first
- `tools/ottd/README.md`, `tools/ottd/ottd` (the `start` and `train` commands), `tools/ottd/ottd_core/server.py`, `tools/ottd/gs/main.nut` (`CmdBuildTrain`, `SetRailType`).
- The NUTS tar: `~/.local/share/openttd/content_download/newgrf/` (find the 0.8.4 file; copy, never modify, anything under `~/.local/share/openttd`).

## Changes
1. **Ports:** `server.py` reads `OTTD_RIG_PORT` (admin, default 3977) and `OTTD_RIG_GAME_PORT` (game, default 3979, written as `server_port` in `[network]`). Defaults keep today's behaviour.
2. **`start --nuts`:** the written config loads NUTS 0.8.4 in `[newgrf]` (grfid and md5 taken from the file, the format OpenTTD itself writes), and sets the start year to 2057 (`[game_creation] starting_year`). Without `--nuts`, the config is unchanged.
3. **GS `build_train`:** optional fields `engine_name` (string), `engines` (count, default 1), `wagon_name` (string). A name selects the buildable engine whose `GSEngine.GetName` equals it. `engines` > 1 builds that many of the engine and attaches each extra one directly behind the front (`GSVehicle.MoveWagon`). With `engine_name`, the rail type chosen is one the engine `CanRunOnRail` (build the line and depot on it: extend `SetRailType` only as far as needed, and report what you changed). Existing fields behave as today.
4. **CLI `train ... --design`:** sends `engine_name "Mastermind (Electric)"`, `engines 2`, `wagon_name "3rd gen Universal Wagon"`, `wagons 6`. Document `--nuts`, `--design` and the two env vars in the README table.

## Manual run (required)
With `OTTD_RIG_DIR=$S/ottd-rig-r1/ OTTD_RIG_PORT=3987 OTTD_RIG_GAME_PORT=3989 OTTD_RIG_BIN=/home/pete/OpenTTD/.claude/play/build-play/openttd` (the default binary is in the main worktree, which the other agent relinks) (check those ports are free first): `start --nuts`, `company`, a 20-tile line, two depots, `train ... --design`, orders, `go`, `watch` 30 s, then `stop` (it stops only your server: confirm the original rig pid still runs afterwards). Report: the train's vehicle id, its part names in order (GS vehicle/engine names), GS-readable power, max tractive effort, max speed, and the top speed `watch` saw.

## Do not
- Stop, restart or connect to the running rig server; don't use ports 3977/3979.
- Touch C++ sources, `/home/pete/OpenTTD`, or anything under `~/.config/openttd` / `~/.local/share/openttd` except reading.
- Add consist presets beyond `--design`.

## Stop conditions
- NUTS fails to load in the dedicated server (report the log lines).
- No rail type both Mastermind can run on and the GS can build.
- The names above don't match any buildable engine in 2057 (report the buildable rail engine and wagon names the GS sees).

## Gates
1. `python3 -m py_compile` on the changed Python files.
2. The manual run above succeeds.
3. Commit: `Add: Junction rig builds the NUTS design train`, body ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
- References read
- Commit hash in the play worktree
- What changed (names only)
- Manual run results (as listed above)
- Stops, and anything left undone
- State: `git -C /home/pete/OpenTTD/.claude/play status --porcelain` and `log -1`

When you have reported, stop.
