# GS1b: a relaxed reachability test skips doomed searches and stops useless growth (D170a, D173, D179)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Why
The engineer, on a T map that tried 30 heights: "30 heights???". The box grows one row per failed height up to the snapshot's edge, and each failed height costs a full line-router search (up to 50k expansions). GS1 tried to judge growth from the tiles a failed search refused; that cannot work (every search touches the stubs, the last row and the box edges). This step uses a relaxation instead, the same idea as D173: a tile-level reachability test that every real route also passes. If the relaxation cannot connect an edge's ends, the line router cannot either, so the search is skipped; and if it cannot connect them even in the tallest box with only the obstacles that never move, no height can work and the growth stops with a "move the cursor" answer.

## State
- Worktree `/home/pete/OpenTTD/.claude/gs1b`, branch `wt-gs1b`, head given at launch (feature/junction-tool with E1 and E2 merged: A and the N/S port edges are routed). Create `build` (Debug) and `build-rel` (Release) like the main ones; build `openttd_test` only. If a build fails because a precompiled header is older than a system header, delete that .pch and rebuild.
- Tests only through `tools/junction/jt` (read `docs/junction-tool/TESTS.md`): `tools/junction/jt fast build [relaxed] 5` (new tag), `tools/junction/jt fast build-rel [projection] 5`, `tools/junction/jt fast build-rel [tmapnear] 5`, `tools/junction/jt fast build [junctionmap] 5`, final `tools/junction/jt gate build-rel`. The `[slow]` test "an obstacle on the main line beside the hub gives no layout" is run once by its exact name in Release for the report, the only by-name run allowed. OVER BUDGET is a stop. Never type tag filters by hand.

## Read first (once each)
`CODINGSTYLE.md`; the glossary in `docs/junction-tool/DESIGN-O4.md`; D173, D175, D179 in `DECISIONS.md`; `JunctionLineRouteRequest` (`starts`, `goals`, `min`, `max`, `blocked`, `existing`, `terrain`, `max_bridge_middle`, `tunnel_price`, `max_tunnel_length`) in `src/junction_line_router.h`; `JunctionTerrain` (`At`, `cls`, `steep`) in `src/junction_terrain.h`; `ConstructJunctionTMap`'s line-router path in `src/junction_generator.cpp` (`route_line`, every routed edge and its failure `say`, the height limit `height_limit`); the tests "a hill covered by a town is not crossed without a tunnel price" and "an obstacle on the main line beside the hub gives no layout" in `src/tests/junction_map.cpp`.

## The relaxation (exact definition)
`bool JunctionRelaxedReachable(const JunctionLineRouteRequest &request)`, declared in `src/junction_line_router.h` with a doc comment saying it over-approximates `FindJunctionLineRoute` (true whenever a route exists), defined in `src/junction_line_router.cpp`:
- Walkable tile: inside `min`..`max`, not in `blocked`, without track of `existing` (any `JunctionTrack` with track bits, or a bridge head), and, with a terrain, `At` non-null, `cls == Buildable` and not `steep`. Heights are ignored (ramps and foundations can join any two of them). Start and goal tiles are walkable whatever lies on them.
- Moves: to any of the four edge-neighbours that is walkable.
- Bridge jumps: along x or y, over 1 .. `max_bridge_middle` middle tiles, each inside the bounds and not `Blocked` on the terrain (existing track and `blocked` tiles may lie under a bridge), landing on a walkable tile.
- Tunnel jumps, only when `tunnel_price` is set and there is a terrain: along x or y, over 1 .. `max_tunnel_length` middle tiles inside the bounds (anything may lie above), landing on a walkable tile.
- Breadth-first from all start tiles; true when a goal tile is reached.
Any rule of the line router this ignores only makes the test pass more often; if you find a way a real route could move that this does not allow, stop and report it.

## Tests, red first
In `src/tests/junction_line_router.cpp`, tag `[junction][relaxed]`:
1. "JunctionRelaxedReachable: a wall of houses cuts the box": a request on a flat test terrain (follow the file's terrain-request tests), a full column of `Blocked` tiles across the box between start and goal: false. The same with one tile of the wall Buildable: true. The same wall with `tunnel_price` set: true. Red: write the test against the declaration with a body returning true, so the red is an assertion.
2. "JunctionRelaxedReachable agrees with the router when it routes": for each request in the file that has an expected route (pick at least five, including one with a bridge), the relaxation is true.
In `src/tests/junction_map.cpp`, `[projection]`:
3. "a hill covered by a town is not crossed without a tunnel price": now `heights_tried == 1`, the log's last line ends `growth cannot help`, and it runs in Release under 0.2 s; drop its `[slow]` tag and the comment that explains it. Red: today it tries every height.
4. "an obstacle on the main line beside the hub gives no layout" stays `[slow]` unless it now stops early; report its heights and time either way. Its cause is a rule (N->A's merge onto A), not a tile, which the relaxation does not see; that is expected, not a stop.
5. Every building test keeps its outcome and its layout; the golden trace test is unchanged (skipped searches are only those that would fail).

## Changes (`src/junction_generator.cpp`, `ConstructJunctionTMap`, line-router path only)
1. In `route_line`, before `FindJunctionLineRoute`: if `!JunctionRelaxedReachable(line_request)`, append the trace line `"- 0 0 relaxed"` and return no route without searching.
2. After each routed edge's failure `say` (A, the port edges, A->S, N->A, B->S, N->B): build the same edge's request once more for the growth test, with `max` raised to the tallest box's last row (`height_limit - 1`), `blocked` reduced to the tiles that do not depend on the height (A's raster tiles are kept in `existing` only; drop every other routed raster, `last_row`, `n_to_a_blocked`, the lane part of `branch_fence`; keep `column_fence`'s rows 0 .. g + 3 and `branch_fence`'s rows 1 .. w_row - 1), and every end that lies on a port edge or a stub replaced by the union of that end over all heights from this one to `height_limit` (for example A->S's goal becomes `(xO, ys_h)` for every such height h). If `JunctionRelaxedReachable` is false for it: `say("<edge> failed; growth cannot help")` with the edge's name as in its failure line, set `micros`, and return (no layout). Write this as one lambda taking the edge's request and a description of which ends move, with a doc comment quoting the engineer ("30 heights???") and stating why the reduced obstacles and the united ends make it a relaxation of every taller box.
3. Old-router path unchanged.
4. TESTS.md: add `[relaxed]` with measured Debug and Release times (budget Debug 5 s); update `[projection]`.

## Do not
No change to `FindJunctionLineRoute`'s search or results, the old-router path, `GenerateJunction`, the skeleton, signal pass, checker, the GUI, `junction_organic.*`, savegame, network or command code. No cap on heights or expansions. No `std::to_string`/`std::printf` in tests; no `pkill`, broad `kill`, bare `git stash`.

## Stop conditions
- Any building test or the golden trace changes.
- Test 3 does not stop at its first height: report, for its failing edge, the reduced request (bounds, the ends) and why the relaxation found a path.
- OVER BUDGET; anything not covered.

## Gates
0 warnings in both builds; the jt runs above pass. One commit on `wt-gs1b`: `Change: Junction T map skips searches and growth that cannot succeed`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
References read; commit; red evidence (each test, the failure seen); functions added; tests 3 and 4: heights_tried, log, Release time; time of one relaxation call on the test spec (Release); jt lines with times; stops; `git status --porcelain`, `git log -1`.

When you have reported, stop.
