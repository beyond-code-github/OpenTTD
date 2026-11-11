# H1: hover generation runs the node search (D182, D185a, D189, D193; the in-game step)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
Persistent worktree `/home/pete/OpenTTD/.claude/nodes` (detached; `build` Debug and `build-rel` Release already built: do NOT reconfigure or delete them). First: `git checkout -B wt-h1 <head given at launch>`; build `openttd_test` incrementally in both. Also build the game target `openttd` in `build-rel` once at the end (it must compile: this step changes GUI code). Tests only through `tools/junction/jt`; tags below; `jt gate build-rel` once before the commit. Output in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/h1/`. Leave the worktree in place.

## Read first (once each, offset/limit)
`CODINGSTYLE.md`; `src/junction_game.h` 75-100 and `src/junction_game.cpp` 380-470 (JunctionNearOffsets, GenerateJunctionNear); `src/junction_gui.cpp` lines 295-330, 410-430, 660-725 (the P3b flat map: flat, flat_key, flat_map, FlatKey, the worker job); `src/console_cmds.cpp` 400-430 (the export caller); `src/junction_generator.cpp` 1935-1965 (SizeJunctionPrio's function-local static topology T) and the code that computes JunctionTMapResult::terrain_cost (grep terrain_cost in junction_generator.cpp / junction_map*.cpp: find the helper that sums CostAt over track tiles and bridge heads); `src/junction_nodes.h` (SearchJunctionNodes, JunctionNodeSearch); `src/junction_nodes.cpp` SearchJunctionNodes; `src/tests/junction_game.cpp` 380-680 (the GenerateJunctionNear tests and their terrain helpers); `src/tests/junction_nodes.cpp` the search tests (how the offset is formed: FlatToWorking with MakeJunctionFrame(spec)).

## Changes
1. `src/junction_generator.h/.cpp`: `/** The L_R topology source of the node model: the test spec's flat T, built once (D189: read for its structure only; lengths come from the solve). */ const JunctionTMapResult &JunctionNodeTopologyT();` (function-local static, as SizeJunctionPrio has now); SizeJunctionPrio uses it instead of its own static.
2. `JunctionNodeSearch` gains `TileIndexDiffC offset{}; ///< For the returned layout: working = layout + offset.` set by SearchJunctionNodes for the accepted state (its state offset minus the render's normalisation shift, the same mapping its acceptance uses for heights).
3. GenerateJunctionNear: drop the `flat` parameter (declaration, doc, both callers). Per offset d (unchanged loop and shifted terrain): topo = JunctionNodeTopologyT() (no layout -> NoLayout result as now); offset = (MakeJunctionFrame(spec).hub_x - topo.hub.x, -topo.hub.y); search = SearchJunctionNodes(*topo.layout, spec, shifted, offset, 16). Layout found: result.layout = search.layout; error None; hub = (frame.hub_x - search.offset.x, 0 - search.offset.y); score = ScoreJunctionLayout(layout, spec) with score.second += the terrain cost of the layout on `shifted` at search.offset, computed by the existing helper you found (reuse it; if it is not callable for a finished layout, STOP and report); prio_tiles = spec.prio_tiles. Else NoLayout. Debug(Misc, Debug2) one line per offset: offset, states, the search time in us, and the search log's last line. Update the function's doc comment (node search, D182).
4. `src/junction_gui.cpp`: remove the P3b flat map (the job's `flat`, the window's `flat_key`/`flat_map`, FlatKey if only used for them, and their uses) — only if nothing else uses them; otherwise STOP and report the other use.
5. `src/console_cmds.cpp`: the call without the flat argument.

## Tests (`src/tests/junction_game.cpp`)
Rulings: "GenerateJunctionNear with a supplied flat map gives the same result" and "GenerateJunctionNear runs the flat map once" are obsolete by design: delete them. "builds the projected T map over a ridge": keep its terrain; replace the identity with the old T map by: offset (0,0), error None, layout present, CheckJunctionRules(layout, spec) empty (INFO the violations), and print the BENCH line as now. "score includes the terrain cost": keep; it must pass with the new cost. Add (outer, `[junction][tmapnear]`) "GenerateJunctionNear builds on flat land at 196 km/h": flat MakeNearTestTerrain, spec = the test spec with train.max_speed = reference_speed = 196 and prio_tiles = SizeJunctionPrio(that spec).prio_tiles; error None, offset (0,0), rules clean. Red: run the new test and the rewritten ridge test before the change; record.
The [slow]/[.slow] GenerateJunctionNear tests: run each once by name after the change (state the command); report pass/fail; do not change them unless they assert the old T map's identity (then apply the ridge ruling).

## Do not
No change to the node model's rules, the checker, the old generator (ConstructJunctionTMap stays: it is the topology source), the library tier (FitJunctionLibraryNear, BuildJunctionLibrary). No `std::to_string`/`std::printf`; no `pkill`, broad `kill`, bare `git stash`.

## Stop conditions
The ridge or 196 km/h test finds no layout (report the Debug lines / search log verbatim: a measurement); the terrain-cost helper unusable; another use of the GUI flat map; OVER BUDGET for [tmapnear] (5 s Debug, measured in Release per TESTS.md: report both); anything not covered.

## Gates
0 warnings (both test builds and `openttd` in build-rel); `jt fast build '[nodes]' 5`, `jt fast build-rel '[tmapnear]' 5`, `jt gate build-rel`. One commit on `wt-h1`: `Feature: Junction hover generation runs the node search (D182)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
References read; commit; red evidence; members added/removed; the ridge and 196 tests' results with their GenerateJunctionNear times; the [slow] tests' results; tag times; gate; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
