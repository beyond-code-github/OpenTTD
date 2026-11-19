# Brief RR (rig tool): lines in colour with PURR Universal Rainbow Rails

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Repo `/home/pete/OpenTTD` (main checkout), files under `tools/ottd/` only (Python CLI `ottd`, `ottd_core/`, GameScript under `tools/ottd/gs/`). Commit on the current branch of the main checkout only the files you change, message `Add: Junction rig rail types per track (PURR rainbow rails)` ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. Do not touch any `.claude/` worktree.
- Time box: 90 minutes.
- Your test server: run dir `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/ottd-rr`, ports `OTTD_RIG_PORT=4177 OTTD_RIG_GAME_PORT=4179`, binary `OTTD_RIG_BIN=/home/pete/OpenTTD/.claude/ng1/build-rel/openttd`. Never touch ports 4077/4079 (the orchestrator's rig) and never start a GUI client. Stop your server at the end.

## Why
The engineer compares solver layouts with hand edits in the game; telling which track is which line needs colour. PURR Universal Rainbow Rails (NewGRF, `/home/pete/.local/share/openttd/newgrf/psg/56420202-PURR_Universal_Rainbow_Rails-0.0.7.tar`) adds rail types in many colours.

## Read first
- `tools/ottd/README.md`; `ottd_core/server.py` (`start`, the `--nuts` NewGRF path: `nuts_newgrf_line`, how the `[newgrf]` line with grfid and md5 is written); the `build-layout` command path in `ottd` / `ottd_core` and the GameScript command that builds a layout (how it picks the rail type today: "first available rail type").

## Changes
1. `ottd start --purr`: loads PURR 0.0.7 like `--nuts` loads NUTS (same `[newgrf]` line mechanism; grfid and md5 read from the tar as `nuts_newgrf_line` does). `--purr` and `--nuts` may be combined.
2. GameScript: a command `rail_types` replying with the available rail types in `ScriptRailTypeList` order: `[{id, name}]`; `ottd rail-types` prints `id name` per line.
3. Layout JSON: an optional integer `"rail_type"` on each `tracks` entry and each `bridges` entry (a rail type id). `build-layout` builds an entry with a `rail_type` with that type (the GS sets the current rail type before each such build and restores the default after); entries without it build as today. Document the field in the README's Layout JSON section.
4. `tools/ottd/colour-lines.py LAYOUT.json LINES.txt OUT.json [--types ID,ID,...]`: LINES.txt holds one piece per line, `x y heading kind line` (x, y tile coordinates in the layout's frame; line a name like `B1`, `E2"`, `prio:E1`). Each distinct line base (the name without a trailing copy mark `"`, `"<digits>` or `'`) gets a rail type id: from `--types` in order of first appearance, else ids 0, 1, 2, ... in order of first appearance; a copy keeps its line's type; `prio:` rows get the type of their lane. A track tile takes the type of the first line listed on it; a bridge takes the type of the line on its start tile. Writes OUT.json (the layout with `rail_type` fields) and prints a legend `line type_id` per line base, plus the count of tiles where two lines of different types share a tile (they take the first).

## Test (on your own server)
- `ottd start --purr` then `ottd rail-types`: list printed, more than 4 types (record the list in your report).
- Build `/home/pete/OpenTTD/docs/junction-tool/wip/t3-1796-o1t3f1s0.json` with every track entry given `rail_type` = the second type id (write that JSON with a throwaway one-liner): `built=N failed=0`; check via `ottd gs` or a map export (`ottd rcon "map_export x0 y0 x1 y1 <path>"`, field `rail_type`) that the tiles have that type.
- `colour-lines.py` on a three-line hand-made LINES.txt and a three-tile layout: the OUT.json fields and legend as specified.

## Do not
- Change the junction C++ code or anything outside `tools/ottd/`. Touch ports 4077/4079 or start a GUI client. Use git stash.

## Report
Changes, the rail type list, test results, commit hash, `git status --porcelain` for the files you touched. When you have reported, stop.
