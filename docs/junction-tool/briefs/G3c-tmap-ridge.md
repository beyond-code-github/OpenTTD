# G3c: projection onto terrain, step 2: a ridge climbed by ramps (revision 5 projection, D170a, D173)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Principle
The line router alone handles terrain (heights, ramps, foundations). The routed edges may climb; the fixed parts (main line row 0, the W row, the stubs) stay at plane 0 and are checked before routing (G3b). This step: a ridge across the branch area, climbed by ramps. No tunnels (the spec has no tunnel price yet), no new prices (ramps cost the existing defaults).

## State
- Worktree `/home/pete/OpenTTD/.claude/g3c`, branch `wt-g3c`, head given at launch (feature/junction-tool with G3c0 merged). Create `build` (Debug) and `build-rel` (Release) like the main ones; build `openttd_test` only.
- Tests only through `tools/junction/jt` (read `docs/junction-tool/TESTS.md`): `tools/junction/jt fast build-rel [projection] 5` (Release budget, TESTS.md), `tools/junction/jt fast build [junctionmap] 5`, final `tools/junction/jt gate build-rel`. OVER BUDGET is a stop. Never type tag filters by hand.

## Read first (once each)
`CODINGSTYLE.md`; `ConstructJunctionTMap` (whole) and `FinishJunctionCandidate`'s terrain checks in `src/junction_generator.cpp`; the `[projection]` tests in `src/tests/junction_map.cpp`; `SetRampColumn` and `SetUpRidge` in `src/tests/junction_line_router.cpp` (how a ramp column is described: z_min, z_max, incline; NE rises towards -x, so SE rises towards +y and NW towards -y).

## Change (`src/junction_generator.cpp`)
In the line-router path only, call `FinishJunctionCandidate` with `terrain = nullptr`: the line router has already checked every routed piece against the terrain (heights, ramps, foundations) and the fixed parts are checked at plane 0 before routing (G3b); `FinishJunctionCandidate`'s plane-0 test would reject every ramp. Comment saying so. The old-router path is unchanged.

## Tests (`[projection]`), red first
Helper (static, in the test file): `SetRampRow(terrain, y, z_min, z_max, incline)` setting every tile of row y, like `SetRampColumn`.
1. "a ridge across the branches is climbed": test spec, flat terrain (G3b's helper), flat height H from `SharedFlatT()`, b row = ml_gap + 2. Ridge: row b+2 z 0..1 incline SE; rows b+3 and b+4 flat at 1; row b+5 z 0..1 incline NW. Check first that rows b+2..b+5 are all < H - 3 (the stub row); if not, stop and report. `ConstructJunctionTMap(spec, &terrain, H, true)`: layout built and `clean`. Report height, heights_tried and the log. Red: before the change, no layout (finish rejects the ramps) - record the log line.
2. "the ridge layout builds through the fake poster": the layout's build steps through the existing sequencer and fake poster (as `src/tests/junction_build.cpp` does) post without error. If the poster checks slopes and fails, stop and report.
3. BENCH (Release): micros, micros_flat, micros_route, heights for test 1.

## Do not
No tunnels, no new prices or spec fields, no main-line routing, no change to the router, the old-router path, decisions, skeleton, signal pass, checker, `GenerateJunction`, `junction_organic.*`, savegame, network or command code. No `std::to_string`/`std::printf` in tests; no `pkill`, broad `kill`, bare `git stash`.

## Stop conditions
- Test 1 not clean after the change: report the log, violations (rule, tile, message), and which edge failed at each height.
- OVER BUDGET; anything not covered.

## Gates
0 warnings; the jt runs above pass. One commit on `wt-g3c`: `Feature: Junction T map projected over a ridge`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
References read; commit; red evidence; test 1's outcome; jt lines; BENCH; stops; `git status --porcelain`, `git log -1`.

When you have reported, stop.
