# O4r: measure one route's cost (DESIGN-O4 revision 3, step O4r)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/o4r`, branch `wt-o4r`, head given in your launch prompt. You are the only writer there; never touch sibling worktrees or the main worktree. Never use `git stash`.
- Build: configure once, `cmake -S . -B build-rel -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-18 -DCMAKE_CXX_COMPILER=clang++-18 -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache > cfg.log 2>&1`, then `cmake --build build-rel --target openttd_test -j 8 > build.log 2>&1`.
- Tests: `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/ptest.sh build-rel/openttd_test "<filter>" 8`. The benchmark itself runs alone, single process: `build-rel/openttd_test "[.bench]" > bench.log 2>&1` (the machine is unstable under all-core load; never run it in parallel with anything).

## Why
The organic junction search (DESIGN-O4 revision 3) routes six movements per round over a few rounds with the line router. Whether that fits 100 ms per generation depends on one route's cost, which nobody has measured for the height-aware router with bridges and tunnels. This step only measures; it changes no behaviour.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md`.
- `src/junction_line_router.h` (the request).
- `src/tests/junction_line_router.cpp`: `BaseRequest`, `FlatTerrainFor`, `TerrainTileOf`, `EnableBridges`, `SetRampColumn`, `SetUpRamps`, `SetUpTunnelRidge`, and one existing test that uses `existing`.

## The change
Add one Catch2 test at the end of `src/tests/junction_line_router.cpp`: `TEST_CASE("FindJunctionLineRoute: route cost benchmark", "[.bench][junction][linerouter]")` (the `[.bench]` tag hides it from normal runs). Helpers it needs are `static` in the same file. Each case builds its request once, then times `FindJunctionLineRoute` with `std::chrono::steady_clock` over 1 warm-up and 31 timed runs, and prints one line per case with `std::printf`:
`BENCH <name> tracks=<n> found=<0|1> cost=<cost> median_us=<median> max_us=<max>`
Also `CHECK` that each case marked "found" below finds a route (so a broken setup is visible).

Box for all cases: min {0, 0}, max {43, 19} (a 44 x 20 junction box). `curve_spacing = 4`, `turn_cost = 3`, `rail_piece = 1`. Terrain from `FlatTerrainFor(request, 5)` unless stated.
Cases (single track = `JunctionLineSpec{1, 1, 1}`; two-track = `{2, 1, 1}`):
1. `flat_straight_1`: single track from {43, 5} heading X_NE to goal {0, 5} X_NE. Found.
2. `flat_turn_1`: single track from {43, 5} heading X_NE to a goal on the S edge row 19 heading south-west along Y (use the Y trackdir that leaves the box at y = 19, at x = 20). Found.
3. `flat_straight_2`, `flat_turn_2`: as 1 and 2 with two tracks. Found.
4. `obstacle_turn_1`: as 2, with `existing` = a layout holding straight X track on rows 8 and 10 across the whole box (the main line), and bridges enabled (`EnableBridges`) and tunnels enabled (`tunnel_price` as `SetUpTunnelRidge` sets it): the route must cross both rows. Found (by bridge).
5. `hills_turn_1`: as 2 with bridges and tunnels enabled and a terrain with heights: columns x = 30..33 set as `SetUpRidge` sets x = 30..33 (shifted to x = 30..33 in this box, same profile), and columns x = 10..13 set as a valley as `SetUpValley` does (same profile, shifted), `slope_piece = 11`, `foundation_cost = 7`, `build_on_slopes = false`. Found.
6. `hills_obstacle_turn_2`: case 5 with two tracks and case 4's `existing`. Report found as seen (no CHECK).
7. `no_route_1`: as 2 with `blocked` = every tile of row 12: no route; this times a full failed search up to `max_expansions`.

## Do not
- Change any router, generator, or non-test file.
- Add timing code to the router.
- Run the benchmark in parallel with builds or other tests.

## Stop conditions
- A case marked "found" finds no route with the setup as written (report which and the error text; do not reshape the case beyond fixing an obvious coordinate slip, and say what you changed).
- Any existing test fails.

## Gates
1. Build: no errors, no new warnings.
2. `ptest.sh build-rel/openttd_test "[linerouter]~[slow]" 8`.
3. Run the benchmark alone three times; keep all three outputs in the report.
4. Commit: `Add: Junction line router route cost benchmark`, body ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. Commit only the test file.

## Report
References read; commit; the three benchmark outputs verbatim (BENCH lines only); any case you adjusted and how; suite results; stops; `git status --porcelain` and `git log -1`. When you have reported, stop.
