# O4h: a tighter admissible heuristic for the line router (DESIGN-O4 revision 3, after O4r)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/o4h`, branch `wt-o4h`, head given in your launch prompt. You are the only writer there; never touch sibling worktrees or the main worktree. Never use `git stash`.
- Build: configure once, `cmake -S . -B build-rel -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-18 -DCMAKE_CXX_COMPILER=clang++-18 -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache > cfg.log 2>&1`, then `cmake --build build-rel --target openttd_test -j 8 > build.log 2>&1`.
- Tests: `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/ptest.sh build-rel/openttd_test "<filter>" 8`. The benchmark runs alone, never beside a build or other tests: `build-rel/openttd_test "FindJunctionLineRoute: route cost benchmark" > bench.log 2>&1`.

## Why (measured, O4r)
The benchmark `FindJunctionLineRoute: route cost benchmark` (end of `src/tests/junction_line_router.cpp`) shows one route costing 26-153 ms; a 44-tile straight route on flat ground with clearing cost 5 takes 150 ms (the same as a failed search that exhausts `max_expansions`). With clearing cost 0 the same route takes 0.15 ms. Cause: `LineRouter::Heuristic` counts only `rail_piece` per tile of Chebyshev distance, so it ignores terrain cost, turn cost and bridge/tunnel prices, and the search degenerates towards uninformed. The organic junction routes six movements per round over several rounds, so a route must cost well under 1 ms.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md`.
- `src/junction_line_router.h` (request and result) and `src/junction_line_router.cpp` (`Heuristic`, the successor loop with its `'F'/'L'/'R'` moves and turn cost, the bridge and tunnel successors with their prices, `TerrainCost`/`CostAt` use, every `return` of the search).
- `src/junction_terrain.h` (`CostAt`).
- The existing `[slow]` "as cheap as an uninformed search" tests and the benchmark in `src/tests/junction_line_router.cpp`.

## Tests (red first; record each red)
In `src/tests/junction_line_router.cpp`, tag `[junction][linerouter]` unless stated:
1. "FindJunctionLineRoute: a straight route on costly ground expands little": the benchmark's `flat_straight_1` request (single track, 44 x 20 box, `FlatTerrainFor(request, 5)`): route found and `result.expansions <= 200`. Red: before the heuristic change, record the expansions seen (expected near `max_expansions` or "search limit").
2. "FindJunctionLineRoute: a turn costs at least its turns": a request whose start heading differs from the goal heading by 90 degrees on clear flat ground (`FlatTerrainFor(request, 0)`), `turn_cost = 50`, `rail_piece = 1`: route found and `result.expansions` lower than with the heuristic's turn term disabled. Implement this comparison only if the turn term is a separate expression you can test through the public request; otherwise assert `result.expansions <= 400` and record the before value as red.
3. `[slow]` "FindJunctionLineRoute: costly routes are as cheap as an uninformed search": the benchmark cases `flat_straight_1`, `flat_turn_1`, `obstacle_turn_1`, `hills_turn_1` (reuse the benchmark's setup by moving it into `static` helpers shared by the benchmark and this test; behaviour of the benchmark unchanged): cost with `use_heuristic` true equals cost with false. If any differs, stop.

## Changes
1. `junction_line_router.h`: `JunctionLineRouteResult` gains `uint expansions = 0; ///< Nodes expanded by the search (O4r, O4h).`, set on every return of the search.
2. `junction_line_router.cpp`, computed once per request before the search (members of `LineRouter`):
   - `step_floor`: the least cost any one track can pay per tile of distance: the minimum of
     - `rail_piece` + the least terrain cost any tile in bounds can charge (`0` without a terrain; with a terrain, the minimum over in-bounds tiles of what the router's terrain cost would charge on that tile at any height the router can stand at there; if that is not a single readable value, use the minimum `clear_cost` and say so in the report),
     - for each n in 0..`max_bridge_middle` when bridges are enabled: `bridge_price(n) / (n + 2)` (integer division rounding down; heads' terrain not added),
     - for each n in 0..`max_tunnel_length` when tunnels are enabled: `tunnel_price(n) / (n + 2)` (rounding down).
   - `Heuristic` returns `step_floor * distance * tracks` (distance as today) plus `turn_cost * turns`, where `turns` is the least number of `'L'`/`'R'` moves that turn track 0's current heading into the nearest goal's heading (each such move turns by 45 degrees: verify from `ExtendJunctionLine`; if a move can turn by more, use the true per-move maximum), taking the minimum over goals of the summed bound (distance part + turn part) rather than mixing goals; with no goals, the turn term is 0.
   - Bridge and tunnel successors are unchanged; the request comments' admissibility notes (`bridge_price`, `tunnel_price`) are updated to say the floor now covers them.

## Do not
- Change costs, moves, legality, the key, the open list, `max_expansions`, the generator or other routers.
- Optimise anything else (for example the per-successor copy of the path's used tracks): report it as a finding if you see it, with the benchmark line that suggests it.

## Stop conditions
- Test 3 or any existing "as cheap as an uninformed search" test finds a cost difference.
- Any existing test fails (beyond a lone hardware flake: rerun alone and report).

## Gates
1. Build: no errors, no new warnings.
2. `ptest.sh build-rel/openttd_test "[linerouter]~[slow]" 8`, then `"[linerouter][slow]"`, then `"[junction]~[slow]"`.
3. The benchmark alone, three times; report the BENCH lines of all three beside O4r's numbers (in the brief's Why).
4. Commit: `Change: Junction line router heuristic counts terrain, turns, bridges and tunnels`, body ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
References read; commit; red evidence per test (expansions before); the `step_floor` terrain choice; benchmark lines (three runs); findings (other hot spots seen); suite results; stops; `git status --porcelain` and `git log -1`. When you have reported, stop.
