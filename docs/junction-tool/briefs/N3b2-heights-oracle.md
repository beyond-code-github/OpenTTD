# N3b2: a height oracle for laid-out layouts (DESIGN-N3 §4; FABLE §2.1 "lay-out that can still fail")

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Why
N3b's facts check each tile alone. Heights must also connect along each path: the router walks a track height z piece by piece (HeightStep/RampStep, src/junction_line_router.cpp ~190-240). A test-only oracle with the same rules tells us whether node-solved layouts are buildable before we add height constraints to the solve.

## State
Worktree `/home/pete/OpenTTD/.claude/n3b2`, branch `wt-n3b2`, head given at launch. Create `build` (Debug) and `build-rel` (Release) (clang-18, ccache); build `openttd_test` only. Tests only through `tools/junction/jt`: `'[nodes]'` (budget 1 s Release, 5 s Debug); `jt gate build-rel` once before the commit. Output in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/n3b2/`.

## Read first (once each, offset/limit)
`CODINGSTYLE.md`; `src/junction_line_router.cpp` lines 185-262 (HeightStep, RampStep, PieceUsable: the rules); `src/junction_terrain.h` (At, UsableAt); `src/junction_nodes.h`; `src/junction_chain.cpp` lines 55-75 (how paths and pieces are read); `src/tests/junction_nodes.cpp` (N3b tests 1-2, the helpers FlatToWorking, MakeMapTestTerrain).

## Addition to `src/junction_nodes.h` / `.cpp`
```cpp
/**
 * Whether the heights of a layout's traced paths connect on the terrain, with the line router's rules (HeightStep/RampStep): along each path the track has a height z;
 * an axis piece on a Buildable, non-steep tile inclined along its direction is a ramp (z == z_min going up -> z + 1; z == z_max going down -> z - 1), any other piece needs UsableAt(tile, z);
 * on_bridge pieces keep z and are not checked. The first piece of a path starts at z_min if it is a ramp up, z_max if a ramp down, otherwise at z_max.
 * Pieces shared by several paths must get the same z on each.
 * @param offset Maps layout tiles to the terrain's coordinates (working = layout + offset).
 * @param why When set and the result is false, receives "path <i> piece <j> at x,y: <reason>".
 */
bool JunctionNodeHeightsOk(const JunctionLayout &layout, const JunctionSpec &spec, const JunctionTerrain &terrain, TileIndexDiffC offset, std::string *why);
```
Paths: as ExtractJunctionChains reads them (MakeJunctionCheckContext with FindJunctionPorts and JunctionTMovements). Do not change the router; reimplement the two rules in junction_nodes.cpp as file-local functions with a comment naming the router functions they mirror.

## Tests, red first (`src/tests/junction_nodes.cpp`, `[junction][nodes]`). Red: stub returns true; record which CHECKs fail.
1. "heights: a half piece on a foundation beside a ramp does not connect" (inner, hand-built): a short layout from JunctionRoute/RasterizeJunctionRoute/AddRasterToLayout (as N3a test 4) along X on a terrain where a ramp-up tile is directly followed by a tile whose only usable height is the ramp's low height (e.g. flat z 0 after a ramp from 0 to 1), so the piece after the ramp needs z 1 and the tile offers only 0. CHECK false and `why` names that piece. Then make that tile flat z 1: CHECK true. (Red: stub true -> first CHECK fails.)
2. "heights: the line router's route across an incline is accepted" (agreement): one FindJunctionLineRoute call from "FindJunctionLineRoute: track along an incline is always a ramp" (src/tests/junction_line_router.cpp ~1183; reuse its terrain and request), lay its raster into a layout and CHECK JunctionNodeHeightsOk is true with offset (0,0). If the route's layout has no traced paths (no ports), state that and instead walk the raster's pieces directly through the same file-local rule functions in the test (STOP and ask before adding any API for that).
3. Extend N3b's tests 1 and 2 ("the node solve builds the flat T on flat terrain", "the node solve moves the W end off an incline") with CHECK(JunctionNodeHeightsOk(layout, spec, terrain, <their working offset>, &why)), INFO(why).

## Do not
No change to the router, junction_facts, junction_positions, the solve itself (SolveJunctionNodes), the generator or the checker. No `std::to_string`/`std::printf`; no `pkill`, broad `kill`, bare `git stash`.

## Stop conditions
- Test 3's W-end case is false: that is a measurement. Report `why` verbatim and the piece's tile and track kind; leave that CHECK in place as a CHECK (the step then commits with that one test red? NO: change it to a `WARN` with the text "known gap: heights not in the solve (N3b3)" and report it; do not change the solve).
- Test 2's agreement fails: stop and report both rule readings.
- OVER BUDGET; anything not covered.

## Gates
0 warnings; `jt fast build '[nodes]' 5`, `jt fast build-rel '[nodes]' 1`, `jt gate build-rel`. One commit on `wt-n3b2`: `Test: Junction height oracle for node layouts (DESIGN-N3)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
References read; commit; red evidence; added members; test 3's result for both cases (with `why` if false); `[nodes]` times; gate; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
