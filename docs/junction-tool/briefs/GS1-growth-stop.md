# GS1: the box grows only when the box stopped the search (D170a, D178, D179)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Why
The engineer, on a T map that tried 30 heights: "30 heights???". `ConstructJunctionTMap`'s line-router path grows the box one row per failed height up to the terrain snapshot's edge, whatever stopped the failed edge. When an edge's search failed for reasons that a taller box does not change (houses, terrain, the main line A, the side bounds), every further height fails the same way. The rule: after a routed edge fails, grow only if some refusal of its search depended on the height; otherwise stop at once and say why. One rule for every routed edge (D178).

## State
- Worktree `/home/pete/OpenTTD/.claude/gs1`, branch `wt-gs1`, head given at launch (feature/junction-tool with E1 merged). Create `build` (Debug) and `build-rel` (Release) like the main ones; build `openttd_test` only. If a build fails because a precompiled header is older than a system header, delete that .pch and rebuild.
- Tests only through `tools/junction/jt` (read `docs/junction-tool/TESTS.md`): `tools/junction/jt fast build [refused] 5` (new tag), `tools/junction/jt fast build-rel [projection] 5`, `tools/junction/jt fast build-rel [tmapnear] 5`, `tools/junction/jt fast build [junctionmap] 5`, final `tools/junction/jt gate build-rel`. The two `[slow]` tests named below are run by name once each in Release for the report (`build-rel/openttd_test "<exact name>"`), the only by-name runs allowed. OVER BUDGET is a stop. Never type tag filters by hand.

## Read first (once each)
`CODINGSTYLE.md`; the glossary and "Every part is an edge" in `docs/junction-tool/DESIGN-O4.md`; `JunctionLineRouteRequest` and `JunctionLineRouteResult` in `src/junction_line_router.h`; in `src/junction_line_router.cpp` every place a successor (piece, bridge head, bridge span middle, tunnel portal or bore) is discarded because of its tile (out of `min`..`max`, in `blocked`, clashing with `existing`, terrain not usable, `SpanOk` false) and where the search ends on `max_expansions`; `ConstructJunctionTMap` in `src/junction_generator.cpp` (the line-router path: `route_line`, the failure sites `say("A failed")`, `"A->S failed"`, `"N->A failed"`, `"B->S failed"`, `"N->B failed"`, `column_fence`, `n_to_a_blocked`, `branch_fence`, `last_row`); in `src/tests/junction_map.cpp` the tests "an obstacle on the main line beside the hub gives no layout" and "a hill covered by a town is not crossed without a tunnel price" (both `[slow]`).

## Tests, red first
1. `src/tests/junction_line_router.cpp`, tag `[junction][refused]`: "FindJunctionLineRoute: a failed search reports the tiles that refused it": a one-track straight request on flat terrain with a full column of `Blocked` terrain tiles across the box between start and goal (follow the file's existing terrain-request tests for the setup). No route; `refused` contains every tile of that column that the search reached and no tile it could not reach; `expansion_limit_hit` false. A second request with `max_expansions = 1` on clear terrain: no route, `expansion_limit_hit` true. Red: the members do not exist; write the test against the members first (compiles once they are declared, empty), so the red is an assertion.
2. "an obstacle on the main line beside the hub gives no layout": now `heights_tried == 1`, no layout, the log has one line ending `growth cannot help`. Remove its `[slow]` tag and the comment that says why it was slow. Red: today 33 heights.
3. "a hill covered by a town is not crossed without a tunnel price": now no layout with the log ending in a `growth cannot help` line, and `heights_tried` at most 6 (report the exact value and every log line). Remove its `[slow]` tag and comment if its Release time is under 0.5 s; otherwise keep `[slow]` and report the time. Red: today 30 heights.
4. Every other test keeps its outcome, in particular every `[projection]` test that builds (the field of obstacles still builds at height 15: its failed heights fail in the D172 window check, not in a routed edge).

## Changes
1. `src/junction_line_router.h`, `JunctionLineRouteResult` gains:
   - `std::vector<TileIndexDiffC> refused; ///< When no route is found: each tile (once) where a successor was discarded because of the tile (outside min..max, blocked, existing track, terrain, span). Empty when a route is found.`
   - `bool expansion_limit_hit = false; ///< The search stopped at max_expansions, so the failure says nothing about the tiles.`
2. `src/junction_line_router.cpp`: record the tile at each discard site of the kind listed in "Read first" (a set of packed tile coordinates during the search, sorted into `refused` only when the search ends without a route); set `expansion_limit_hit` where the search stops on `max_expansions`. Discards for rule reasons (curve spacing, S-bend window, movement rules, D113) are not recorded: when the search exhausts its states those states are all inside the box, which is the point. No change to which routes are found (the golden trace test must stay green unchanged).
3. `src/junction_generator.cpp`, `ConstructJunctionTMap`, line-router path:
   a. `route_line` keeps the last request's `refused` and `expansion_limit_hit` in two locals of the enclosing function.
   b. A lambda `growth_can_help(dependent)`: true when `expansion_limit_hit`, or when any refused tile has `y >= ys - 2` (the stubs, their fence tiles, the goals and the last row all move with the height), or is in `dependent`. Doc comment: D170a, the engineer's "30 heights???", and why each class depends on the height.
   c. At each routed edge's failure site, after its existing `say`: if `!growth_can_help(dependent)`, `say("<edge> failed; growth cannot help")` with the edge's name as in the existing line, set `micros`, and return (no layout). `dependent` per site: A: none. A->S: none beyond the rule in b. N->A: the tiles of A->S's raster and `n_to_a_blocked`. B->S and N->B: the tiles of A->S's and N->A's rasters, of both of B's lanes, and the lane part of `branch_fence` (the tiles from the skeleton's `x_hw`/`x_he`); for N->B also B->S's raster. Tiles of A's raster and the rows 1 .. w_row - 1 part of `branch_fence` are not dependent (A does not change with the height).
   d. The old-router path is unchanged. Failures that are not a routed edge (the D172 window, skeleton, lanes, finish, "frame too short", the stub check) keep growing as today.
4. TESTS.md: add `[refused]` with its measured Debug and Release times (budget Debug 5 s); update `[projection]` if its time changes.

## Do not
No change to which routes the router finds, to the old-router path, `GenerateJunction`, the skeleton, signal pass, checker, the GUI, `junction_organic.*`, savegame, network or command code. No growth cap, no new height limit. No `std::to_string`/`std::printf` in tests; no `pkill`, broad `kill`, bare `git stash`.

## Stop conditions
- The golden trace test or any building test changes outcome.
- Test 2 or 3 does not stop early: report the log and, for the last failing edge, the refused tiles that `growth_can_help` called dependent and why.
- OVER BUDGET; anything not covered.

## Gates
0 warnings in both builds; the jt runs above pass. One commit on `wt-gs1`: `Change: Junction T map grows only when the box stopped the search`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
References read; commit; red evidence (each test, the failure seen); members added; tests 2 and 3: heights_tried, log, Release time; jt lines with times; stops; `git status --porcelain`, `git log -1`.

When you have reported, stop.
