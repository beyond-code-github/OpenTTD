# LP5: line router on terrain at one plane, around existing track, with an admissible heuristic

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/lp5`, branch `wt-lp5`, head given in your launch prompt. You are the only writer there. Other agents work in sibling worktrees under `.claude/`; never touch them or the main worktree `/home/pete/OpenTTD`.
- Build: configure once, `cmake -S . -B build-rel -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-18 -DCMAKE_CXX_COMPILER=clang++-18 -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache > cfg.log 2>&1`, then `cmake --build build-rel --target openttd_test -j 8 > build.log 2>&1`.
- Tests: `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/ptest.sh build-rel/openttd_test "<filter>" 8`. Iterate with `"[linerouter]"`; `"[junction]~[slow]"` and full `"~[.]"` only at the gate.
- Known flake: generator tests occasionally segfault or SIGILL under parallel load (pre-existing, investigated elsewhere). If a generator case crashes, rerun it alone 3 times; report it, don't chase it.
- Plan: `docs/junction-tool/RECON-linepainter.md` §E (LP5; LP6 bridges and LP8 heights are later steps). Decisions in `/home/pete/OpenTTD/docs/junction-tool/DECISIONS.md`: D133–D136, D142, D143, D147 (this step's ruling).

## Why (settled)
LP4 routes a whole line on flat empty ground. Before the painter can be used on a map, the router must respect the terrain snapshot (water, houses, other property, clearing and foundation cost) at one level and the company's existing track. Also, LP4's heuristic is `rail_piece × tracks × chebyshev(track 0, goal)`. On a turn where track 0 is the outer track, the inner tracks need fewer pieces than track 0, so the heuristic can overestimate and A* can return a costlier route than the cheapest.

**D147 (orchestrator, 2026-11-09, reversible):** (1) LP5 routes at a single plane, as the junction router does (D78): every piece's tile must satisfy `JunctionTerrain::UsableAt(x, y, plane)`, and each tile's `CostAt` is paid once per tile the line occupies, however many of its tracks share it. Heights and slopes between planes are LP8; bridges are LP6. (2) An existing layout's tracks are obstacles with the line's own rule: a new piece may share a tile with an existing track only as an exact parallel pair (the `Occupy` rule); a bridge head tile is blocked. (3) The heuristic is one track's distance: `rail_piece × chebyshev(track 0, nearest goal)`, which is admissible because track 0 alone needs at least that many pieces. (4) D143 narrowing is a separate step (LP5b) with its own geometry design; not here.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md`.
- DECISIONS rows D143, D147.
- `src/junction_line_router.{h,cpp}` (all; 45 + 252 lines).
- `src/junction_terrain.h` 15-80 (`JunctionTileClass`, `JunctionTerrain::UsableAt`, `CostAt`).
- `src/junction_layout.h` 20-62 (`JunctionTrack`, `JunctionBridge`, `JunctionLayout`).
- `src/tests/junction_line_router.cpp` (all).
- For a terrain fixture to copy: grep `JunctionTerrain` in `src/tests/junction_router.cpp` and read the nearest helper that builds one.

## Tests (red first; record each red), in `src/tests/junction_line_router.cpp`, tag `[junction][linerouter]`
Add the new request members (Change 1) first, unused, so the tests compile; each must then fail on an assertion.
1. "FindJunctionLineRoute: detours around a house": L_R, straight east-west corridor as in "straight when clear", a terrain covering the corridor with one `Blocked` tile on track 1's straight path (not on track 0's). The route exists, no piece of any track is on that tile, and its cost is above the straight route's. Red: the straight route goes through it.
2. "FindJunctionLineRoute: refused over water": as 1 but the bounds are one tile wide on each side of the straight line so no detour fits, and the tile is `Water`. Result: no route, error `"no route"`. Red: a route is returned.
3. "FindJunctionLineRoute: pays terrain cost once per tile": L_R straight, a terrain where every tile has `clear_cost = 10` and `rail_piece = 1`. The cost (add `Money cost` to `JunctionLineRouteResult` if it does not exist; see Change 3) equals pieces × 1 + distinct occupied tiles × 10. Red: cost without the terrain term.
4. "FindJunctionLineRoute: existing track is an obstacle": an existing `JunctionLayout` with a `Track::Y` piece crossing track 0's straight path, and bounds wide enough to detour. The route avoids that tile. Then the same with an existing piece that is an exact parallel pair partner of track 0 on one tile (same tile, the parallel track of track 0's piece): the straight route is allowed. Red: the crossing case goes straight through.
5. "FindJunctionLineRoute: the route is as cheap as an uninformed search": add the test seam `bool use_heuristic = true; ///< Test seam: false makes the search uninformed (h = 0).` For L_R and LL_RR, for every goal piece in a box (goals at x = start.x − 6 .. start.x − 14, y = start.y − 4 .. start.y + 4, every trackdir the line can end on there), with `turn_cost = 3`: when both searches find a route, the costs are equal. Red: find at least one case where the LP4 heuristic gives the higher cost and record it (start, goal, both costs). **If no case in that box is red, do not change the heuristic**: keep the test as a guard, and report "no red found".

## Changes
1. `junction_line_router.h`, `JunctionLineRouteRequest`, after `blocked`:
   ```cpp
   const JunctionTerrain *terrain = nullptr; ///< When set, every piece's tile must be usable at plane, and its CostAt is paid once per tile (D147).
   int plane = 0; ///< The level the line is built on; only used with a terrain.
   const JunctionLayout *existing = nullptr; ///< When set, its tracks and bridge heads are obstacles (D147).
   ```
   and the `use_heuristic` seam (Test 5). Include `junction_terrain.h` and `junction_layout.h` as needed. Update the file's `@file` line and the struct doc from "on flat ground" to "at one plane (LP4, LP5)".
2. `LineRouter`:
   - Constructor: seed a member `std::unordered_map<uint64_t, TrackBits> existing_bits` from `request.existing->tracks` (offset → tracks) and add both head tiles of every `request.existing->bridges` entry to `blocked`.
   - Piece validity (in both places that test `InBounds`/`blocked` today: the start pieces and the expansion loop): additionally reject when `request.terrain != nullptr && !request.terrain->UsableAt(x, y, request.plane)`, and when combining the piece's track with `existing_bits` at its tile fails the `Occupy` rule (same track already present, or `TracksOverlap`). Do this with one private member `bool PieceUsable(const JunctionPiece &piece) const` used in both places.
   - Cost: when `request.terrain` is set, add `request.terrain->CostAt(x, y, request.plane)` for a piece whose tile is not yet in `used` before that piece is recorded (so once per tile per path; the path's earlier tiles are already in `path_used`).
   - Heuristic: `request.use_heuristic ? request.rail_piece * best : Money{0}` (D147 (3)); doc comment citing D147. Only if Test 5 found a red case.
3. `JunctionLineRouteResult`: add `Money cost = 0; ///< The route's total cost (pieces, turns, terrain).` if it is not there, and set it from the goal node's g.

## Do not
- Add slopes, plane changes, foundations beyond `CostAt`, bridges, cursor goals or narrowing (LP5b, LP6, LP7, LP8).
- Change `ExtendJunctionLine`, `RasterizeJunctionLine`, the curve grammar, or the single-track router.
- Change the search key or the closed-set logic.

## Stop conditions
- `JunctionTerrain` or `JunctionLayout` lacks something the brief names.
- An existing `[linerouter]` test changes result.
- Test 5's uninformed search exceeds `max_expansions` on the box (report how many cases; don't raise the limit).

## Gates
1. Build `build-rel` `openttd_test`: no errors, no new warnings.
2. `ptest.sh build-rel/openttd_test "[linerouter]" 8`, `"[junction]~[slow]"`, then `"~[.]"`.
3. Report the expansion count (add a temporary print, removed before commit) of the existing "L_R around a block" case before and after the heuristic change.
4. Commit: `Feature: Junction line router on terrain at one plane, around existing track`, body ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. Commit only your files.

## Report
- References read
- Commit: hash and subject
- Red evidence per test (Test 5: the red case, or "no red found")
- Types, functions, members added or removed: names only
- Expansion counts before/after
- Suite results and wall times
- Stops, and anything left undone
- State: `git status --porcelain` and `git log -1`

When you have reported, stop.
