# G5: tunnels in the T map's projection (D155, D174, D175)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Why
D175: towns and industries are passed around or under, never bridged. The line router already tunnels (D155) when its request has `tunnel_price` and a terrain, but the spec has no tunnel price, so the T map's edges never tunnel. The four routed edges are branch (SL) lines: D174 says SL tunnels are single at basic throughput, so no doubling is needed here (main lines are not routed yet).

## State
- Worktree `/home/pete/OpenTTD/.claude/g5`, branch `wt-g5`, head given at launch (feature/junction-tool). Create `build` (Debug) and `build-rel` (Release) like the main ones; build `openttd_test` only.
- Tests only through `tools/junction/jt` (read `docs/junction-tool/TESTS.md`): `tools/junction/jt fast build-rel [projection] 5`, `tools/junction/jt fast build [tunnelprice] 5` (new tag), `tools/junction/jt fast build [junctionmap] 5`, final `tools/junction/jt gate build-rel`. OVER BUDGET is a stop. Never type tag filters by hand.

## Read first (once each)
`CODINGSTYLE.md`; D155, D174, D175 in `docs/junction-tool/DECISIONS.md`; `JunctionUnitPrices` and `JunctionSpec` in `src/junction_generator.h`; `MakeTestJunctionSpec` and `ConstructJunctionTMap`'s `route_line` lambda in `src/junction_generator.cpp`; `tunnel_price` and `max_tunnel_length` in `src/junction_line_router.h`; `MakeJunctionSpec` in `src/junction_game.cpp` (how the bridge price table is precomputed on the main thread); `CmdBuildTunnel`'s cost in `src/tunnelbridge_cmd.cpp` (the loop from "Tile shift coefficient" to the "Pay for the rail/road in the tunnel" switch); the G3c ridge test and its helpers in `src/tests/junction_map.cpp`.

## Tests, red first
In a new tag `[junction][tunnelprice]`, in `src/tests/junction_game.cpp`:
1. "JunctionTunnelPrice follows the tunnel command's cost": for build_tunnel = 1000 and rail = 100, the values for middle = 0, 1, 2, 24, 25 and 26, each hand-computed from CmdBuildTunnel's loop (write the arithmetic for 0 and 1 in a comment; 25 crosses the first `tiles_bump`). Red: the function does not exist; write the test against its declaration and a body returning 0 first, so the red is an assertion.
2. "MakeJunctionSpec: the tunnel price and length come from the game": `spec.prices.tunnel` is set and equals `JunctionTunnelPrice(n, _price[Price::BuildTunnel], RailBuildCost(railtype))` for n = 0, 3; `spec.max_tunnel_length == _settings_game.construction.max_tunnel_length`. Follow how the neighbouring MakeJunctionSpec tests set up the rail type and prices. Red: `tunnel` unset.

In `src/tests/junction_map.cpp`, `[projection]`:
3. "a hill covered by a town is tunnelled": the test spec with `spec.prices.tunnel = [](uint n) { return Money(600 + 300 * n); }`; the G3c ridge terrain (same rows, heights, inclines, `build_on_slopes = true`) with the two flat top rows also `Blocked` on every tile of the terrain's width (houses on the hill). `ConstructJunctionTMap(spec, &terrain, H, true)` as in the G3c test: layout built and `clean`; at least one `bridges` entry has `tunnel == true`; every tunnel's two ends lie on the ridge's two incline rows; no bridge (non-tunnel) has a middle tile on the two blocked rows. Report height, heights_tried, the log and the tunnels. Red: the same call with `spec.prices.tunnel` unset builds no layout (record the log); keep that as a second check in the test (unset: no layout).
4. The G3c ridge test (no tunnel price) is unchanged: it still climbs by ramps.
5. BENCH (Release): micros, micros_flat, micros_route, heights for test 3.
Report each tag's time; add `[tunnelprice]` to the TESTS.md table (measured Debug and Release, budget Debug 5 s).

## Changes
1. `src/junction_generator.h`:
   - `JunctionUnitPrices` gains `std::function<Money(uint)> tunnel; ///< One tunnel by middle-tile count, as CmdBuildTunnel charges it without clearing the portals; unset: no tunnels (D155).`
   - `JunctionSpec` gains `uint16_t max_tunnel_length = 64; ///< The game's max_tunnel_length, in middle tiles.`
2. `src/junction_generator.cpp`, `route_line`: when `spec.prices.tunnel` is set, `line_request.tunnel_price = spec.prices.tunnel; line_request.max_tunnel_length = spec.max_tunnel_length;` with a comment citing D175 and D174 (SL tunnels single). Nothing else in the generator; `MakeTestJunctionSpec` stays without a tunnel price (existing tests unchanged).
3. `src/junction_game.{h,cpp}`:
   - `Money JunctionTunnelPrice(uint middle, Money build_tunnel, Money rail_piece);` declared in the header with a doc comment: CmdBuildTunnel's cost for a rail tunnel of `middle` middle tiles, without clearing: the per-middle-tile loop (price plus the shifted multiplier, with `tiles_coef` and `tiles_bump` as there), the entrance price, the exit price, and `(middle + 2) * rail_piece`. Mirror the command's arithmetic exactly, with a comment naming it.
   - `MakeJunctionSpec`: precompute a `std::vector<Money>` of `JunctionTunnelPrice(n, _price[Price::BuildTunnel], RailBuildCost(railtype))` for n = 0 .. max_tunnel_length on this thread (as the bridge table is); `spec.prices.tunnel` returns the entry, or the same "impossible" value the bridge closure uses past the end; `spec.max_tunnel_length` from `_settings_game.construction.max_tunnel_length`.

## Do not
No tunnel doubling, no main-line routing, no change to the router, the old-router path, `GenerateJunction`, decisions, skeleton, signal pass, checker, the GUI, `junction_organic.*`, savegame, network or command code. Do not set a tunnel price in `MakeTestJunctionSpec`. No `std::to_string`/`std::printf` in tests; no `pkill`, broad `kill`, bare `git stash`.

## Stop conditions
- Test 3 not clean: report the log per height, violations (rule, tile, message) and the tunnels; do not change the checker or the signal pass.
- An existing test changes outcome (name it and its failure).
- OVER BUDGET; anything not covered.

## Gates
0 warnings in both builds; the jt runs above pass. One commit on `wt-g5`: `Feature: Junction T map tunnels under blocked hills`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
References read; commit; red evidence (each test, the failure seen); members and functions added; test 3's outcome; jt lines with times; BENCH; stops; `git status --porcelain`, `git log -1`.

When you have reported, stop.
