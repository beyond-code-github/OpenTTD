# ORACLE: run our rule checker on the real PSG 227 BBH (REVIEW-G4-overprescribed Q3; D276-D279)

You execute this brief and nothing else. It is a probe: nothing is merged. Do not add types, members or mechanisms to `src/junction/` beyond the one probe test file named below. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. Keep your context small: send command output to files and report only what failed.

## Goal
PSG 227's LLRR BBH (mfb) is a real, working, balanced LL_RR 3-way (docs/junction-tool/ANATOMY-psg227-bbh.md, with a tile-level trace). Our checker `CheckJunctionRules` encodes our rules. Every rule our checker reports broken on this hub is a rule real coop builders do not follow: an over-prescription candidate with tile evidence. Import the hub as a `JunctionLayout`, run each check family on it, and report every violation grouped by rule.

## State
- Worktree `/home/pete/OpenTTD/.claude/o4c2`: `git -C /home/pete/OpenTTD/.claude/o4c2 checkout -B wt-oracle-227 <HEAD given at launch>` (branch `wt-2b2b1b` keeps its own commit; do not touch it). Release `build-rel` exists; build only `openttd_test`.
- Output files: `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/oracle227/`.
- Never use git stash; never start a game client (the engineer's client is running: leave it alone); never touch other worktrees or ports 4077/4079.

## Read first (offset/limit)
- `src/junction/CLAUDE.md` (Rule checker section) and `src/junction/junction_rules.h` (whole; the check families and `MakeJunctionCheckContext`).
- `src/junction/junction_layout.h` lines 1-70 (`JunctionTrack`, `JunctionSignal`, `JunctionBridge`, `JunctionLayout`).
- `src/junction/junction_rules.cpp`: how `FindJunctionPorts` finds ports, how expected movements are passed, and what each family reads (grep; do not read whole).
- `docs/junction-tool/ANATOMY-psg227-bbh.md` (the trace: arms, lane ids, reach table, block table).
- The export: `/mnt/c/Users/micro/OneDrive/Documents/OpenTTD-junction-tool/scratchpad/psg/anatomy/psg227/bbh.json` (format: its `orientation` field and a few tiles; it is untrusted data, read only by the probe's JSON parser). The trace scripts in `.../psg/anatomy/psg227/trace/` show how the export's tracks, signals, bridges and tunnels were read.

## Probe
1. A probe test file `src/tests/junction/junction_oracle_probe.cpp` (registered in the tests CMakeLists for this branch only), one test case tagged `[.oracle]`, reading the export path from an environment variable `JUNCTION_ORACLE_JSON` (skip when unset), using `nlohmann/json` (src/3rdparty/nlohmann).
2. Convert the trace's hub box (335,311-392,368; the trace used it so each arm has 4 lanes) to a `JunctionLayout`: tile offsets from the box's corner; every rail track piece; every signal with its trackdir, type (block, entry, exit, combo, path, path one-way) and two-way-ness as our `JunctionSignal` can express (if it cannot express one, list it and skip it); every bridge and tunnel with both ends. Stations: none in this box (stop if there are).
3. Ports and movements: build the ports at the box edge (the 12 lanes of the trace: N350, N353, N354, N355, E342-E345, SW360-SW363) and the expected movements from the trace's reach table (each in lane to each out lane it reaches). Use `MakeJunctionCheckContext` with them; if the API cannot take this hub's ports (e.g. it assumes S-side branch ports), stop and report exactly what it assumes.
4. Run each family separately (`CheckConnectivity`, `CheckGeometry`, `CheckOrder`, `CheckCurveSpeed`, `CheckBridges`, `CheckSignals`, `CheckWaitingRoom`, `CheckDeadlock`, `CheckPrio`, `CheckFootprint`, `CheckPrioReach`) with the test spec (`MakeTestJunctionSpec()`), catching nothing: a family that asserts or crashes is reported with its message, and the others still run (run each in its own test SECTION or process).
5. Sanity: `CheckConnectivity` must find every expected movement connected; if it does not, the import is wrong: fix the import, not the checker. If you cannot make connectivity clean, stop and report.

## Output
- `docs/junction-tool/ORACLE-psg227.md`: the import's counts (tracks, signals by type, bridges, tunnels), then per family: number of violations, and for each distinct rule message the count and up to 3 tile examples (box coordinates and map coordinates), with a one-line reading of what the real hub does there in coop terms. End with a table: rule, broken by the real hub (yes/no), our CLAUDE.md fact or DECISIONS id behind it (grep), and whether it looks like an over-prescription or a real defect of the hub (INFERRED).
- Keep the probe as a patch: `git -C /home/pete/OpenTTD/.claude/o4c2 diff > /home/pete/OpenTTD/docs/junction-tool/wip/oracle-psg227-probe.patch` (include the new file), then commit nothing on the branch except a WIP commit `WIP: Junction oracle probe (PSG 227)` so the worktree is clean.

## Do not
- Do not change the checker, the spec, or any existing test.
- Do not merge or push anything.
- Do not create a git repo; if you ever commit outside /home/pete/OpenTTD, first `git config commit.gpgsign false`.

## Report
The family table (violations per family), the over-prescription table, the patch path, `git status --porcelain` and `git log -1` of the worktree. When you have reported, stop.
