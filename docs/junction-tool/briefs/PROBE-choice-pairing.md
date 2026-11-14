# PROBE: place LL² with PSG 227's lane pairing (REPRESENT-psg227 changes 1 and 5; D283, D287)

You execute this brief and nothing else. It is a probe on a probe branch: nothing is merged. Make exactly the changes listed. Do not add any type, member, file or mechanism the brief does not name. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. Keep your context small: send command output to files and report only what failed.

## Goal
The engineer wants to see tonight how far our pipeline gets with mfb's PSG 227 graph: the 18 lane paths of his pattern on our LL² T, our placement (D287: our prio element, our parallelograms, our spacing). Report each stage the seed reaches, and the first failure with its message.

## State
- Worktree `/home/pete/OpenTTD/.claude/nodes`: `git -C /home/pete/OpenTTD/.claude/nodes checkout -B wt-choice-probe <HEAD given at launch>`. Leave its untracked files alone. Release `build-rel` exists; build only `openttd_test` (`cmake --build build-rel --target openttd_test -j 14 > <out>/build.log 2>&1`).
- Output dir: `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/choice-probe/`.
- Never use git stash; never start a game client; never touch other worktrees or ports 4077/4079.

## Read first (offset/limit)
- `docs/junction-tool/REPRESENT-psg227.md` Q1 and the change list; DECISIONS D283, D287 (grep `| D28`).
- `src/junction/topology/junction_topology.h` 38-90 (pairing enum, `ExpandJunctionLanes` comment), `junction_topology.cpp` 40-125 (`ExpandJunctionLanes`), 1765-1790 (`MakeJunctionHubTopology`).
- `src/junction/junction_pipeline.h` (`MakeJunctionGraphSeed`) and one test that calls it: `src/tests/junction/junction_layout.cpp` around 755-830.

## Changes
1. `JunctionLanePairing` gains `ChoicePerLine` (doc comment: "Per (in group, out group) one lane is fixed to one out lane and one lane chooses either out lane (PSG 227, D283); needs two lanes on both groups; through movements follow it too.").
2. In `ExpandJunctionLanes`, when `topology.pairing == ChoicePerLine` and both groups of a movement have more than one lane, through movements included (the through branch at the top of the loop is skipped for this pairing):
   - both groups must have exactly 2 lanes, else `fail("choice per line needs two lanes")`;
   - lane 0 of the in group is the fixed lane, lane 1 the choice lane;
   - the fixed lane goes to out lane `i`, where `i` is the index (0 or 1) of movement m among the movements into the same out group, in movement order;
   - paths pushed in this order: (in lane 0, out lane i), (in lane 1, out lane 0), (in lane 1, out lane 1).
   - Any other pairing: unchanged.
3. `JUNCTION_BRIDGE_MAX_CROSSERS` = 4 (probe only).
4. A probe test file `src/tests/junction/placement/junction_probe_choice.cpp`, registered in `src/tests/CMakeLists.txt` like the other placement tests, one TEST_CASE tagged `[.probe]`:
   - topology `MakeJunctionHubTopology(LL_RR, LL_RR, JunctionLanePairing::ChoicePerLine)` (parse with `ParseLineNotation("LL_RR")`), spec `MakeTestJunctionSpec()`;
   - asserts `ExpandJunctionLanes` gives 18 paths and every out slot has exactly 3 source paths (red before change 2: fewer or more paths);
   - writes to the output dir: the 18 paths (from slot, to slot, movement); the track graph (`BuildJunctionTrackGraph`): nodes, edges with their paths, crossings; and for each out slot the merge cascade (which legs merge first);
   - calls `MakeJunctionGraphSeed(topology, spec, &error)` and writes the error, or on success: the layout's ASCII (the existing ASCII writer the layout tests use), `CheckJunctionRules` violations by rule, and the layout JSON (grep `junction_export` or the JSON writer the tests use), plus where each prio element landed (which lane).
   Run with `timeout 600 build-rel/openttd_test "[.probe]" > <out>/run.log 2>&1`.
5. If the seed fails at a refusal named in REPRESENT's change list 6 or 7 (`junction_topology.cpp` around 877, 946, 957, 963), record it, then make a probe-only change that skips that one refusal, rebuild and rerun; repeat for each such refusal. Any other failure (a crash, an assert, a placement or joint failure, a timeout): stop there and report it with its message and the stage reached. Do not fix placement.

## Output
- `/home/pete/OpenTTD/docs/junction-tool/PROBE-choice-pairing.md` (main tree, uncommitted): the 18 paths, the graph summary (node and edge counts, crossings, the merge cascades per out slot and whether the two choice legs merge first), each run's stage reached and failure, the refusals skipped, and if it placed: size, checker violations by rule, prio lanes, ASCII and JSON paths.
- Probe patch: `git -C /home/pete/OpenTTD/.claude/nodes diff <launch HEAD> > /home/pete/OpenTTD/docs/junction-tool/wip/choice-probe.patch` (include the new file), then a WIP commit on the branch (`git -c commit.gpgsign=false commit`) so the worktree is clean.

## Budget
Stop after 60 minutes of work and report what finished.

## Report
Stages reached per run, the first non-refusal failure, any placement's checker summary and file paths, `git status --porcelain` and `git log -1`. When you have reported, stop.
