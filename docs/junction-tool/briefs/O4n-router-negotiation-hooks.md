# O4n: line router hooks for negotiated routing, minimum length, and the remaining hot spots (DESIGN-O4 revision 3: O4n1 + O4n2)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/o4n`, branch `wt-o4n`, head in your launch prompt. You are the only writer there; never touch sibling worktrees or the main worktree. Never use `git stash`.
- Build: configure once, `cmake -S . -B build-rel -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-18 -DCMAKE_CXX_COMPILER=clang++-18 -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache > cfg.log 2>&1`, then `cmake --build build-rel --target openttd_test -j 8 > build.log 2>&1`.
- Tests: `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/ptest.sh build-rel/openttd_test "<filter>" 8`. The benchmark runs alone, never beside a build or tests: `build-rel/openttd_test "FindJunctionLineRoute: route cost benchmark" > bench.log 2>&1` (its BENCH lines print cost and expansions).
- Design: `docs/junction-tool/DESIGN-O4.md` (revision 3; this is steps O4n1 and O4n2), decisions D160, D161, D162.

## Why
The organic junction search routes six movements per round with the line router under negotiated congestion (PathFinder): each round every movement is rerouted against an extra cost per track bit and level that the search computes from the other movements (present + history; zero where two movements may share a bit). The router needs that cost as an input. Waiting room (G7: 2 x TL units between split and the last signal before the merge), bays (D83) and prio coverage are minimum lengths on a routed token, so the router needs a minimum length. The router also still spends ~2 us per expansion (O4p findings), and a junction generation routes dozens of times.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md`; DESIGN-O4.md sections "Sharing" and "The search"; D160-D162.
- `src/junction_line_router.h` and `src/junction_line_router.cpp` (whole: you change them).
- `src/junction_layout.h` `JunctionPiece` (its `units`), `src/junction_rules.cpp` `CheckWaitingRoom` (~803-840: how units are summed).
- In `src/tests/junction_line_router.cpp`: `BaseRequest`, `FlatTerrainFor`, `CheckRoute`, `EnableBridges`, `SetUpRiver`, the benchmark helpers (`BenchCase`, `SetUpBenchCase`), one "as cheap as an uninformed search" test.

## Tests (red first; record each red), tag `[junction][linerouter]`
1. "FindJunctionLineRoute: a resource cost steers the route": single track (`JunctionLineSpec{1, 1, 1}`), straight 20-tile request on flat ground, `resource_cost` returning 100 for Surface pieces on the start row for x in the middle third and 0 elsewhere: the route leaves the row there, and `result.cost` equals the request's piece, turn and terrain costs plus the sum of `resource_cost` over every piece of the returned route (recompute it in the test from the rasterized route). Red: route stays on the row.
2. "FindJunctionLineRoute: a resource cost sees the direction": as 1 but dear only for the trackdir the route travels (X_NE), 0 for the opposite trackdir on the same tiles: the NE route detours; the same request mirrored (start and goal swapped, heading SW) stays straight. Red: the NE route stays straight.
3. "FindJunctionLineRoute: a resource cost on a bridge span": a river that forces a bridge (as `SetUpRiver`), `resource_cost` returning 7 for BridgeSpan pieces: route found with a `B` move, and `result.cost` is 7 x (middle pieces of all tracks) more than with no `resource_cost`. Red: the costs are equal.
4. "FindJunctionLineRoute: a minimum length lengthens the route": single track straight request of 10 tiles on flat ground with `min_units` = the units of 16 straight tiles: route found whose track-0 units sum to at least `min_units`. Red: the straight 10-tile route.
5. `[slow]` "FindJunctionLineRoute: minimum length routes are as cheap as an uninformed search": test 4's request and two more `min_units` values (12 and 20 tiles' worth): `informed.cost <= uninformed.cost` (D161).

## Changes
1. `junction_line_router.h`:
   ```cpp
   /** Where a routed piece sits, for negotiated resource costs (DESIGN-O4 revision 3). */
   enum class JunctionResourceLevel : uint8_t {
   	Surface, ///< Track on the ground: plain, ramp, bridge head or tunnel portal.
   	BridgeSpan, ///< A bridge's middle piece.
   	TunnelBore, ///< A tunnel's middle piece.
   };
   ```
   Request members:
   - `std::function<Money(const JunctionPiece &piece, int8_t z, JunctionResourceLevel level)> resource_cost; ///< Extra cost per piece of every track, from the negotiated search (DESIGN-O4 revision 3); must be >= 0; unset means 0. z is the track's height (bridge spans: the track's height at the heads; tunnel bores: the tunnel's floor).`
   - `uint min_units = 0; ///< Track 0 needs at least this many units (JunctionPiece::units, as CheckWaitingRoom counts them) from its first piece, prefix included, before a goal counts (waiting room, bays, prio coverage).`
2. `junction_line_router.cpp`:
   - `resource_cost` is added for every piece of every track wherever the router adds that piece's cost: start pieces (root cost), prefix pieces if they are costed today (if they are not, leave them uncosted and say so), F/L/R pieces, bridge heads and middles, tunnel portals and middles. The heuristic is unchanged (costs >= 0 keep it admissible).
   - `LineNode` gains `uint16_t units = 0; ///< Track 0's units so far, capped at min_units.`, updated with track 0's added pieces, part of `Key`; `IsGoal` also requires `units >= request.min_units`.
3. Hot spots (behaviour-identical: costs and expansions on every benchmark case must not change; check before and after with the benchmark):
   - `path_used`: a flat grid member over the request's bounds (`std::vector<TrackBits>`, one per tile), filled per expansion by walking the parent chain as today, the touched indices recorded and cleared after the expansion; the overlay and `Occupy`/`IsUsed` read it instead of the map. Pieces outside the bounds (if any can occur, e.g. none) must keep today's behaviour: check how the map handles them and match it.
   - `blocked`: a flat grid of `bool` over the bounds instead of `std::set`.
   - `tree`: a `std::deque<LineTreeEntry>` so the popped node is read by `const` reference instead of copied; the key is built once per push (a single lookup-or-insert).

## Do not
- Implement the negotiation loop, movements, sharing rules or prios (O4b does that); change the heuristic, the key's existing fields, legality, `max_expansions` or the benchmark's cases.
- Change other files than the router's `.h`, `.cpp` and its test file.

## Stop conditions
- After change 3, any benchmark case's cost or expansions differ from before it.
- A `[slow]` case has informed > uninformed.
- Any existing test fails (beyond a lone hardware flake: rerun alone and report).

## Gates
1. Build: no errors, no new warnings.
2. `ptest.sh build-rel/openttd_test "[linerouter]~[slow]" 8`, `"[linerouter][slow]"` (6 jobs), `"[junction]~[slow]"`.
3. The benchmark alone: once before change 3, three times after. Report all BENCH lines.
4. Commit: `Feature: Junction line router resource costs and minimum length for negotiated routing`, body ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. One commit.

## Report
References read; commit; red evidence per test; names added; whether prefix pieces are costed; the out-of-bounds finding for the grid; benchmark lines (before change 3 and three after); findings (remaining hot spots); suite results; stops; `git status --porcelain` and `git log -1`. When you have reported, stop.
