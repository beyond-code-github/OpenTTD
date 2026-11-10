# PH1: every edge starts at its start piece's own height (D177 retired, D178, D179)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Why
The engineer: "each port having its own height is pretty important". Today `ConstructJunctionTMap` takes one plane, the hub tile's ground height (D177), and every line-router request starts there. A main-line end on higher or lower ground than the hub therefore fails, and once A ramps, an edge that starts on A's piece (A->S's split) starts at the wrong height. A piece's height on its tile follows from the tile and the piece's direction (flat at its height; a foundation at z_max; along an incline a ramp entered at z_min going up or z_max going down, D152), which the line router already models with `PieceUsable`. So with a terrain each start's height is derived, not given. Goals already work: a route ends at whatever height its goal piece is usable at.

## State
- Worktree `/home/pete/OpenTTD/.claude/ph1`, branch `wt-ph1`, head given at launch (feature/junction-tool with E1, E2, E3, E3b merged: every part of the hub is routed; the fixed-part check is gone on the line-router path). A first attempt's work is in `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/ph1_wip.patch` (made before E3): reuse its router change and tests where they still apply; its stand-in W-row check is obsolete (that check no longer exists) and must not come back. Create `build` (Debug) and `build-rel` (Release) like the main ones; build `openttd_test` only. If a build fails because a precompiled header is older than a system header, delete that .pch and rebuild.
- Tests only through `tools/junction/jt` (read `docs/junction-tool/TESTS.md`): `tools/junction/jt fast build [startheight] 5` (new tag), `tools/junction/jt fast build-rel [projection] 5`, `tools/junction/jt fast build-rel [tmapnear] 5`, `tools/junction/jt fast build [junctionmap] 5`, final `tools/junction/jt gate build-rel`. OVER BUDGET is a stop. Never type tag filters by hand.

## Read first (once each)
`CODINGSTYLE.md`; D152, D177, D178, D179 in `docs/junction-tool/DECISIONS.md`; `JunctionLineRouteRequest::plane` and `terrain` in `src/junction_line_router.h`; in `src/junction_line_router.cpp` how the root nodes are built from `starts` (around `root.z.push_back(request.plane)`) and `PieceUsable` (what z before a piece makes it usable and what z after it gives); `JunctionTerrain` in `src/junction_terrain.h`; `ConstructJunctionTMap`'s line-router path (D177's hub plane and "hub tile not buildable", the D179 port-approach check, `route_line`, A, the port edges); the `[projection]` tests that mention the hub plane or z = 2.

## Tests, red first
In `src/tests/junction_line_router.cpp`, tag `[junction][startheight]`:
1. "FindJunctionLineRoute starts at the start tile's height": a straight one-track request on a terrain all flat at z 3, `plane = 0`: routes. Red: today no route (the start piece is unusable at 0).
2. "FindJunctionLineRoute starts a ramp at its low end": the start tile inclined rising along the travel direction (z 0..1), the rest flat at z 1 ahead, `plane = 7`: routes, and the route's first piece is the ramp. Red: today no route.
3. Without a terrain, `plane` is still used (an existing test covers it: name it).
In `src/tests/junction_map.cpp`, `[projection]`:
4. "the main line's W end on higher ground is reached by a ramp": the test terrain flat at z 0 with every tile of column x <= x_w + STUB + 2 at z 1 (flat), and column x_w + STUB + 3 an incline from 0 to 1 rising towards -x (x_w from the flat T as in the E1 tests), `build_on_slopes = true`. Layout built and `clean`; A ramps. Red: today no layout (record the log).
5. "the hub on a slope": the test terrain flat at z 1 with the hub tile (xI, 0) a non-steep slope with z_max 1 (a foundation tile): built and clean, as before. Today it passes (D177 used z_max); keep it as a guard.
6. Every existing test keeps its outcome and layout; the golden trace is unchanged.

## Changes
1. `src/junction_line_router.cpp`: when `request.terrain` is set, each start's root height is derived instead of `request.plane`: try z = the start tile's `z_min` and `z_max` (in that order) and take the first for which the start piece is usable with `PieceUsable` as the root uses it; if neither is, that start is skipped as unusable (as an unusable start is today). Without a terrain, `request.plane` as today. Update `plane`'s doc comment in the header: "Track 0's start height without a terrain; with a terrain each start's height is derived from its tile (PH1)."
   If the root code shows that a start's height is needed somewhere else before the first piece (the heuristic, prefix moves), apply the same derived value there; if it is needed somewhere the brief cannot see, stop and report.
2. `src/junction_generator.cpp`, `ConstructJunctionTMap`, line-router path:
   a. D177's hub plane: delete it and its "hub tile not buildable" return; the hub tile is a tile of A like any other (A's routing and the D179 port approaches cover it). `base.plane` stays 0 (used only without a terrain).
   b. The D179 port-approach check: each approach tile must be usable at the height derived as in 1 for a straight piece along row 0 (write it as a small lambda using `JunctionTerrain::UsableAt` at `z_max` for flat or foundation tiles and accepting an incline along the row; comment that it mirrors the router's derivation). Keep the log text `port approach blocked at x,0`.
   c. Update the comments that cite D177.
   d. The existing test "an unbuildable hub tile gives no layout" (G4) changes: its blocked hub tile is now on A's route, so expect no layout with a log line from A or the port approaches (whichever applies; report the log) or a layout that avoids it, clean. One-line comment citing PH1. This is the only existing test whose expectation may change.
3. `docs/junction-tool/DECISIONS.md` is not yours to edit; the orchestrator records D177's retirement.

## Do not
No change to which routes are found when the start height was already right (golden trace unchanged), to the old router, the old-router path, `GenerateJunction`, `FitJunctionLibrary*`, the skeleton, signal pass, checker, the GUI, `junction_organic.*`, savegame, network or command code. No `std::to_string`/`std::printf` in tests; no `pkill`, broad `kill`, bare `git stash`.

## Known from the first attempt
- With the first attempt, test 4 built at the flat height but was unclean: checker rule 3 "curve speed limit 111 below 160" at layout tile 2,4, near A's ramp and the routed merge on the west side. Find the cause before changing anything: print the layout around that tile and the pieces of the edges there, and report which edge's curve it is and why the line router accepted it (does the router count curve spacing across a ramp the way the checker does?). If the fix is in the router or the checker, stop and report it; do not change either.
- E2's test "a slope under the S port edge is ramped" built at 16, not at the flat height, because its port edge started at the single plane; after this change report at which height it builds.
- E3's test 3 (slope under B's E stretch) failed because starts used the hub plane; after this change report its outcome.

## Stop conditions
- The golden trace, the flat identity or any existing test other than the one named in 2d changes outcome.
- Test 4 not clean: report the log per height and violations.
- OVER BUDGET; anything not covered.

## Gates
0 warnings in both builds; the jt runs above pass. One commit on `wt-ph1`: `Change: Junction routes start at their start piece's own height`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
References read; commit; red evidence (each test, the failure seen); test 4 outcome with height and log; the changed G4 test's outcome; jt lines with times; stops; `git status --porcelain`, `git log -1`.

When you have reported, stop.
