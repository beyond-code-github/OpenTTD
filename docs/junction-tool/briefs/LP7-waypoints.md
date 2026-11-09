# LP7: waypoints and a frozen prefix for the line router (D150)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/lp7`, branch `wt-lp7`, head given in your launch prompt. You are the only writer there. Never touch sibling worktrees or the main worktree `/home/pete/OpenTTD`.
- Build: configure once, `cmake -S . -B build-rel -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-18 -DCMAKE_CXX_COMPILER=clang++-18 -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache > cfg.log 2>&1`, then `cmake --build build-rel --target openttd_test -j 8 > build.log 2>&1`.
- Tests: `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/ptest.sh build-rel/openttd_test "<filter>" 8`. Iterate with `"[linerouter],[line]"` minus slow (`"[linerouter]~[slow]"`) or your own filter; `"[junction]~[slow]"` then `"~[.]"` at the gate.
- Hardware note: this machine's CPU is unstable under heavy all-core load. A lone segfault/SIGILL is most likely that: rerun the case alone and report it.
- Decisions: `/home/pete/OpenTTD/docs/junction-tool/DECISIONS.md` rows D133, D134, D135, D149, D150 (this step's ruling: read it first).

## Why (settled, D150)
The painter is click-driven (D133): the player clicks a start and then waypoints; the reference track (track 0) passes through each clicked tile and the router picks the heading (D134 a). Adding a waypoint re-routes the last two segments together while earlier segments stay frozen (D134 b), so the router needs (1) an ordered list of pass-through tiles and (2) a frozen prefix of moves whose pieces and curve state it continues from.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md`.
- DECISIONS rows D134, D150.
- `src/junction_line.h` (whole) and `src/junction_line.cpp` `RasterizeJunctionLine` (grep it).
- `src/junction_line_router.h` (whole) and `src/junction_line_router.cpp` (whole; LP6 added bridges).
- `src/tests/junction_line_router.cpp`: the helpers at the top, "straight when clear", "curve spacing binds", and the two `[slow]` informed = uninformed cases.

## Tests (red first; record each red)
Add to `src/tests/junction_line_router.cpp`, tag `[junction][linerouter]`, flat ground with no terrain unless stated, L_R gap 1, start heading along an axis as the existing straight tests do, bounds generous enough for the detours (but small enough that "no route" returns quickly).
1. "FindJunctionLineRoute: passes a waypoint": no goals, one waypoint 10 tiles ahead and 4 tiles to one side of the start heading. Route found; `RasterizeJunctionLine` of it gives track 0 a non-bridge piece on the waypoint tile, and track 0's last piece is within the last move's pieces of that tile (the route ends with the move that passes it: assert that removing the last move gives a raster whose track 0 has no piece on the waypoint). Red: before the change, no route (no goal can be reached).
2. "FindJunctionLineRoute: passes waypoints in order": two waypoints, placed so that the second is nearer the start than the first (for example W1 12 tiles ahead, W2 6 tiles ahead and 4 to the side). Track 0's first piece on W1 comes before its first piece on W2.
3. "FindJunctionLineRoute: a waypoint then a goal": one waypoint and one exact goal piece beyond it; the route passes the waypoint and ends on the goal piece.
4. "FindJunctionLineRoute: a frozen prefix carries its curve state": `curve_spacing` set so that a turn needs at least 4 axis pieces of straight run before it (half-units as in "curve spacing binds"), `prefix` a legal move string ending in a turn (for example `"FFFFFL"` then enough to be on an axis heading again if the turn template needs it; check with `RasterizeJunctionLine` that your prefix has no error), and a waypoint placed so that an immediate turn after the prefix would be cheapest. Assert: the returned moves start with the prefix; the first move after the prefix is not a turn. Then the same request without the prefix but starting at the prefix's last pieces would turn at once (assert its first move is a turn) — this shows the state, not the geometry, forbids it. Red: before the change the moves don't start with the prefix.
5. "FindJunctionLineRoute: prefix pieces are the line's own track": a prefix that runs straight, and a waypoint behind the start so the route must come back past the prefix; no piece of the searched part shares a tile with a prefix piece except as an exact parallel pair. Red: the prefix isn't replayed.
6. "FindJunctionLineRoute: a waypoint on water is refused": a `FlatTerrainFor` terrain with the waypoint tile set to `JunctionTileClass::Water` and no bridges enabled: "no route".
7. A `[slow]` case (or an extension of the LP6 one): tests 1-5's requests give the same `cost` with `use_heuristic` true and false. If the costs differ, stop and report (do not change the heuristic).

## Changes
1. `junction_line_router.h`, `JunctionLineRouteRequest`:
   - after `goals`:
     ```cpp
     std::vector<TileIndexDiffC> waypoints; ///< Tiles track 0 passes through, in order, on a piece that is not a bridge middle tile (D134 a, D150). With no goals the route ends with the move that passes the last one.
     std::string prefix; ///< Frozen moves from the start (D134 b, D150): replayed before the search, their pieces are the line's own track and set the curve state; not re-checked against terrain or obstacles.
     ```
   - `goals` doc: "Exact end pieces for track 0; may be empty when there are waypoints (D150)."
   - `JunctionLineRouteResult::route` doc (or the struct's): the route's moves are the prefix followed by the searched moves.
2. `junction_line_router.cpp`:
   - `LineNode` gains `uint8_t passed = 0; ///< Waypoints passed so far (D150).`; `Key` appends it.
   - Root: rasterize `{request.start, request.start_trackdir, request.prefix}` with `RasterizeJunctionLine` (its error is returned as today). The root's `last` is each track's last piece; its states come from the initial state `{cap, 0, true, false}` advanced by `AdvanceState` over each track's pieces after the first (a failure is an error "prefix breaks the curve rules"); `after_bridge` is whether the prefix's last move is a bridge. The start-piece checks (usable, terrain cost, occupy) stay for the first pieces only, as today. The root entry's `added` holds each track's prefix pieces after the first and its `bridges` the prefix's bridges, and the per-expansion path state (`path_used`, `path_bridges`) includes the root entry (today the loop stops before it).
   - Waypoint progress: when a move is made (F/L/R and B), walk track 0's added pieces in order; while `next.passed < waypoints.size()` and a piece that is not a bridge middle tile lies on `waypoints[next.passed]`, increment `next.passed` (one piece may pass several equal consecutive waypoints; keep it simple: check the same piece again after an increment).
   - `IsGoal`: `node.passed == request.waypoints.size()` and (`request.goals.empty()` ? `!request.waypoints.empty()` : the existing exact match). With neither goals nor waypoints, return the error `"a route needs a goal or a waypoint"` from `Run` before searching.
   - `Heuristic` per D150 (4): with `p = node.passed`, `d = chebyshev(last[0], waypoints[p]) + sum of chebyshev(waypoints[k], waypoints[k + 1]) for k = p .. size - 2`, and from the last waypoint (or from last[0] when all are passed) the existing nearest-goal distance when goals are set; times `rail_piece * tracks`, zero when `!use_heuristic`.
   - Route reconstruction: `moves = request.prefix + searched moves`.

## Do not
- Change `RasterizeJunctionLine`, `ExtendJunctionLine(Bridge)`, the single-track router, bridges' rules (D149), heights (LP8) or D143 narrowing (LP5b).
- Re-check prefix pieces against terrain, `blocked` or `existing` (D150 3).
- Add red-tile reporting (D150 5).
- Change existing tests' expectations.

## Stop conditions
- Any existing test fails (beyond a lone hardware flake).
- Test 7's informed and uninformed costs differ.
- Test 4's no-prefix comparison does not turn at once (the test then proves nothing: report the layout instead of adjusting the assertion).

## Gates
1. Build `build-rel` `openttd_test`: no errors, no new warnings.
2. `ptest.sh build-rel/openttd_test "[junction]~[slow]" 8`, then `"~[.]"`.
3. Commit: `Feature: Junction line router passes waypoints and continues a frozen prefix`, body ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. Commit only your files.

## Report
- References read
- Commit: hash and subject
- Red evidence per test
- Types, functions, members, constants added or removed: names only
- Suite results and wall times
- Stops, and anything left undone
- State: `git status --porcelain` and `git log -1`

When you have reported, stop.
