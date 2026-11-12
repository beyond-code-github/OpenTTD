# RG1: the LL_RR seed as a rig test circuit (milestone: LL_RR on flat ground in the rig)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (persistent; Debug `build`, Release `build-rel`). First: `git checkout -B wt-rg1 <HEAD>` (given at launch). You are the only writer there. Never use git stash.
- The new pipeline makes a checker-clean LL_RR seed: ExpandJunctionLanes, BuildJunctionTrackGraph, OrderJunctionTrackGraph, StructureJunctionTrackGraph, ProgramJunctionTrackGraph, CutJunctionTrackBridges, RenderJunctionTrackPlacement (with routes), SignalJunctionTrackRoutes (see SignalledOf in src/tests/junction_topology.cpp). The rig (`tools/ottd/ottd junction-check FILE --find-clear --trains N`) builds a circuit JSON: the junction, turnaround loops joining each out port back to an in port, a depot, then wandering trains and a deadlock verdict. Today only the T has a circuit: TEST_CASE "Junction test circuit" `[.circuit]` in src/tests/junction_design.cpp, T roles hard-coded.

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ part).
- src/tests/junction_design.cpp :60-100 (CircuitDepot, CircuitExtras, WriteLayoutJson) and :208-390 (the T circuit test, whole).
- src/tests/junction_topology.cpp: RenderOf and SignalledOf.
- src/junction_rules.h (FindJunctionPorts(layout, topology), JunctionPort.slot); src/junction_topology.h JunctionPortSlots, JunctionLaneGroup.
- tools/ottd/ottd: `setup_circuit` and `cmd_junction_check` (grep them; read ~120 lines) to see which JSON fields the rig uses.

## Changes (src/tests/junction_design.cpp only)
1. **Extract** from the T circuit test, unchanged in behaviour, two static helpers placed above it, and make the T test call them:
   - `static CircuitDepot AddCircuitDepot(JunctionLayout &layout, const JunctionRaster &loop)`: the depot block ("The depot: on A, ...") on the given loop; FAIL as today when there is no site.
   - `static void AddCircuitLoopSignals(JunctionLayout &layout, const std::vector<JunctionRaster> &rasters, const JunctionSpec &spec)`: the "One-way block signals along each loop" block.
   The T test's output (ASCII, depot, waypoints, JSON) must stay byte-identical: run it with JUNCTION_TMAP=1 JUNCTION_CIRCUIT=<file> before and after and diff the two JSON files (report the diff is empty).
2. **Add** `TEST_CASE("Junction LL_RR test circuit", "[.circuitllrr]")`:
   - spec = MakeTestJunctionSpec() with max_bridge_middle = 10; the pipeline as SignalledOf does it (copy the calls; the test files do not share helpers), on JunctionLLRRSidelineTopology(). REQUIRE the layout is non-empty and `CheckJunctionRules(layout, spec, topology)` is empty (print each violation first).
   - ports = FindJunctionPorts(layout, topology) (REQUIRE 10). For each side W, E, S: its in ports and its out ports (group.in of the port's slot's group). Sort the in ports and the out ports each by their distance along the box edge to the nearest port of the other kind on that side (nearest first). Pair the i-th in with the i-th out: loop i runs from out port to in port (the T's loop request: start = out tile and trackdir, goal = in tile and trackdir), DEPTH 32, the same region per side as the T test's W, E, S loops; route the pairs nearest first (inner loops first), adding each raster to the layout before routing the next. FAIL naming the side and pair when a loop has no route.
   - AddCircuitLoopSignals over all loop rasters; AddCircuitDepot on the outermost W loop (the last W loop routed).
   - No waypoints. extras.box = {0, 0, w, h}, normalised as the T test does; print the ASCII and the depot; write JSON when JUNCTION_CIRCUIT is set (WriteLayoutJson(layout, path, depots, &extras)).
3. Write the circuit to tools/ottd/layouts/circuit-llrr-TL4.json (Release build: `JUNCTION_CIRCUIT=$PWD/tools/ottd/layouts/circuit-llrr-TL4.json build-rel/openttd_test "[.circuitllrr]"`), and commit it with the code.

## Tests
- The new test is hidden (`[.circuitllrr]`); it passes when run directly. The T circuit JSON is unchanged (diff above).
- `tools/junction/jt gate .claude/nodes/build-rel` green (count unchanged from before).

## Do not
- Do not run the rig, start or stop servers, or touch tools/ottd code.
- Do not change anything outside src/tests/junction_design.cpp and the new JSON file.

## Stop conditions
- The LL_RR seed has checker violations; a loop has no route (report which, with the ASCII so far to a file you name); the T JSON differs.
- Anything not covered here.

## Gates and commit
- One commit: `Add: Junction LL_RR rig test circuit`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus the circuit's size, loop count and the JSON path. When you have reported, stop.
