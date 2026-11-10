# G3c0: the box grows within the terrain, and the crossing window on the line-router T (D170a, D172)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Why
G3b's "field of obstacles" test (A->S's whole flat route blocked) showed two gaps: (1) heights stop at the flat cap `g + 13`, a construction leftover, not a rule (D170a: the box grows until the map fits); (2) A->S detoured far west and the crossing set no longer fit one doubled bridge ("no valid skeleton"), because D172's X_B window is only applied in the parked SoverN code, not on the line-router T.

## State
- Worktree `/home/pete/OpenTTD/.claude/g3c0`, branch `wt-g3c0`, head given at launch (feature/junction-tool, G3b merged). Create `build` (Debug) and `build-rel` (Release) like the main ones; build `openttd_test` only.
- Tests only through `tools/junction/jt` (read `docs/junction-tool/TESTS.md`): `tools/junction/jt fast build [projection] 5`, `tools/junction/jt fast build [junctionmap] 5`, final `tools/junction/jt gate build-rel`. OVER BUDGET is a stop. Never type tag filters by hand.
- Design: `docs/junction-tool/DESIGN-O4.md` section "G1", paragraph "Crossing nodes and their windows"; D170a, D172 in DECISIONS.md (main checkout).

## Read first (once each)
`CODINGSTYLE.md`; `ConstructJunctionTMap` (whole, including `route_line` and the G3b fixed-part check); `src/tests/junction_map.cpp` (the `[projection]` tests and `SharedFlatT`); `JunctionTerrain` in `src/junction_terrain.h`. For the window, the parked reference: `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/g2b/g2c_wip.patch` (read only the lo/hi and `blocked` part; do not apply the patch).

## Changes (`src/junction_generator.cpp`)
1. Height limit: without a terrain, unchanged (`g + 13`). With a terrain, heights grow while the frame fits the snapshot: stop after the largest height whose box (rows 0 .. height - 1, the frame's columns) lies inside `terrain` (every tile `At` non-null). Comment: D170a, and the snapshot is the edge of what projection can see.
2. X_B window (D172), line-router path only: after A->S routes, lo and hi are the smallest and largest x of its pieces on the W row and the b row (bridge spans included). D = nmax - 1 - 2 * k_head (nmax as the skeleton input computes it, k_head = 1); if hi - lo > D, log `"A->S crossing wider than the doubled bridge"` and try the next height. N->A's line request gets as `blocked` every W-row and b-row tile with x < hi - D or x > lo + D. Comment quoting D172. The old-router path is unchanged (its column fence stays its envelope).
3. Nothing else.

## Tests (`[projection]`), red first
1. "a field of obstacles grows the box" (existing test 3): keep its blocking; it must now either build clean at a height above the flat one, or fail with every height up to the terrain limit logged and none logged "no valid skeleton for either stagger" (the window makes the skeleton's input fit by construction). Red: today it logs "no valid skeleton" at 14 and stops at the flat cap.
2. "the crossing window keeps both crossings under one bridge": for test 1's terrain (one blocked tile), every piece of A->S and N->A on the W and b rows lies within an x-interval of width D (use `a_to_s_tiles`; add `std::vector<TileIndexDiffC> n_to_a_tiles; ///< Test aid: the tiles of N->A's route, frame coordinates, before cropping.` filled like `a_to_s_tiles`).
3. The flat line-router T stays identical to the old router's (existing `[junctionmap]` test).
If test 1 makes `[projection]` OVER BUDGET (it routes more heights), stop and report its time and log; do not tag it `[slow]` yourself.

## Do not
No hills, no SoverN, no change to the old-router path, the router, decisions, skeleton, finish, signal pass, checker, `GenerateJunction`, `junction_organic.*`, savegame, network or command code. No `std::to_string`/`std::printf` in tests; no `pkill`, broad `kill`, bare `git stash`.

## Stop conditions
Test 3 (flat identity) breaks; OVER BUDGET; anything not covered.

## Gates
0 warnings; the jt runs above pass. One commit on `wt-g3c0`: `Change: Junction T map box grows within the terrain, crossing window on the line router`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
References read; commit; red evidence; members added; test 1's outcome (built at which height, or the log); jt lines; BENCH (Release) for test 1; stops; `git status --porcelain`, `git log -1`.

When you have reported, stop.
