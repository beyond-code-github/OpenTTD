# G3b: projection onto terrain, step 1: obstacles (revision 5 projection, D170a, D173)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Principle
Projection materialises the map's edges one at a time with the line router, which alone handles the terrain; the box grows when an edge finds no room (D170a). This step: flat terrain with obstacles (Blocked tiles) at plane 0. The four routed edges (A->S, N->A, B->S, N->B) route around obstacles. The fixed parts (A main line row 0, B's lanes, the N and S stubs) are not routed yet: an obstacle on them is reported, not worked around (routing the main lines is a later step). No hills in this step.

## State
- Worktree `/home/pete/OpenTTD/.claude/g3b`, branch `wt-g3b`, head given at launch (feature/junction-tool, G3a merged). Create `build` (Debug) like `/home/pete/OpenTTD/build` and `build-rel` (Release) like `/home/pete/OpenTTD/build-rel`; build `openttd_test` only.
- Tests run ONLY through `tools/junction/jt` (read `docs/junction-tool/TESTS.md` first). Develop with `tools/junction/jt fast build [junctionmap] 5`; final gate `tools/junction/jt gate build-rel`. OVER BUDGET is a stop condition. Never type tag filters by hand; never run `[linerouter]`.

## Read first (once each)
`CODINGSTYLE.md`; `docs/junction-tool/TESTS.md`; `ConstructJunctionTMap` (whole, including the `route_line` lambda) and `FinishJunctionCandidate`'s terrain checks in `src/junction_generator.cpp`; `src/tests/junction_map.cpp`; `MakeNearTestTerrain` in `src/tests/junction_game.cpp` (how a test terrain matches the frame); `JunctionTerrain` in `src/junction_terrain.h`.

## Changes (`src/junction_generator.{h,cpp}`)
1. `JunctionTMapResult` gains `std::vector<TileIndexDiffC> a_to_s_tiles; ///< Test aid: the tiles of A->S's route, frame coordinates, before cropping.` filled from A->S's raster at the built height (line router and old router paths).
2. With `terrain` set, before routing at each height: every tile of the fixed parts laid by `base_layout` and the W row's lane (rows 0 and W, the two stubs) must be `terrain->UsableAt(x, y, 0)`; if not, log `"fixed track on unusable tile x,y"` and return the result with no layout (growing the box cannot help; routing the main lines is a later step). Comment why.
3. The line router path passes `terrain` and plane 0 to every line request (check `route_line` does; if it already does, no change). The flat map that gives the first height (D173) keeps `terrain = nullptr`.
4. Nothing else: heights still grow one row at a time up to the existing cap.

## Tests (`src/tests/junction_map.cpp`, `[junctionmap]`), red first
Use a test terrain helper copied from `MakeNearTestTerrain` (flat, Buildable, z 0, sized to the spec's frame with margin 4) as a static function in this file.
1. "an obstacle on A->S's route is projected around": test spec; flat line-router T gives `a_to_s_tiles`; pick the tile at the middle index of that list that is not on row 0, the W row or the b row; mark it `Blocked`; `ConstructJunctionTMap(spec, &terrain, std::nullopt, true)`: layout built, `clean`, the blocked tile is not in the new `a_to_s_tiles`, and the layout differs from the flat one (tracks or bridges). Report height and heights_tried. Red: with no code change the tile is still used or (if FinishJunctionCandidate rejects it) no layout.
2. "an obstacle on the main line is reported": a Blocked tile on row 0 at x = 3: no layout, `log` contains `fixed track on unusable tile 3,0`.
3. "a field of obstacles grows the box": block every tile of the flat A->S route except rows 0, W, b (test spec): either built and clean at a height > the flat height, or (if no height up to the cap routes) no layout with every height logged; report which, the heights and the log. Information, not a stop, as long as tests 1 and 2 pass.
4. BENCH (Release, report only): micros, micros_flat, micros_route, heights for tests 1 and 3.

## Do not
No hills or slopes, no main-line or lane routing, no change to the router, the old router, decisions, windows, fences, skeleton, signal pass, checker, `GenerateJunction`, `junction_organic.*`, savegame, network or command code. Do not change the default of `line_router`. No `std::to_string`/`std::printf` in tests; no `pkill`, broad `kill`, bare `git stash`.

## Stop conditions
- Test 1 cannot be made clean without a change the brief does not name: report the log, violations (rule, tile, message) and which edge failed at each height.
- `jt fast build [junctionmap] 5` is OVER BUDGET.

## Gates
0 warnings; `jt fast build [junctionmap] 5` passes; `jt gate build-rel` passes. One commit on `wt-g3b`: `Feature: Junction T map projected around obstacles`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
References read; commit; red evidence; members added; the jt lines (time, budget); BENCH lines; test 3's outcome; stops; `git status --porcelain`, `git log -1`.

When you have reported, stop.
