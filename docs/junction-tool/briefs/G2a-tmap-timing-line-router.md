# G2a: the T map's time by phase, and its edges on the line router (DESIGN-O4 "G1", revision 5 projection)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Principle (unchanged from G1)
One map, edges materialised one at a time, checked once. No sweep, no best-of, no retry with other parameters. Unclean = missing constraint: stop and report.

## State
- Worktree `/home/pete/OpenTTD/.claude/g2`, branch `wt-g2`, head `e5d6918c6c` (feature, G1 merged as `e7a918c44d`). Create `build` configured like `/home/pete/OpenTTD/build` (Debug); build `openttd_test` only.
- Measured after G1 (Release): test spec 58 ms, pinned 30 ms, one map each, against 100 ms. G2 will project several maps, so per-map time must drop.

## Test speed
`[junctionmap]` stays under 5 s in Debug. Develop against it only; the wider gate once at the end.

## Read first (once each)
- `CODINGSTYLE.md`.
- `src/junction_generator.cpp`: `ConstructJunctionTMap` and `FinishJunctionCandidate` (whole), and `RouteAndAdd` / `RouteFencedAndAdd`.
- `src/junction_line_router.h` (whole) and in `src/junction_line_router.cpp` how `starts`, `goals`, `blocked`, `existing`, `curve_spacing`, `s_bend_window`, bridges and the result's route/raster are used. `src/junction_line.h` (`JunctionLineSpec`, `JunctionLineRoute`).
- `src/tests/junction_map.cpp`.

## Part 1: time by phase (commit 1)
Add to `JunctionTMapResult`:
```cpp
int64_t micros_route = 0;    ///< All router calls, every height tried.
int64_t micros_skeleton = 0; ///< Crossing set, both skeleton solves, lanes.
int64_t micros_finish = 0;   ///< FinishJunctionCandidate (crop, normalise, signal pass).
int64_t micros_check = 0;    ///< JunctionRulesClean (and CheckJunctionRules when unclean).
std::vector<int64_t> micros_per_height; ///< Wall time of each height tried, in order.
```
Measure with `std::chrono::steady_clock`. The BENCH line gains `route=<us> skeleton=<us> finish=<us> check=<us> heights=<n>`. Test: "the T map reports its time by phase": for the test spec, the four phases are each >= 0, their sum <= micros, `micros_per_height.size() == heights_tried`.
Commit: `Change: Junction T map reports its time by phase`.

## Part 2: edges on the line router (commit 2)
Replace each of the four routed edges' router call in `ConstructJunctionTMap` (A->S, N->A, B->S, N->B) with `FindJunctionLineRoute`, keeping every decision the same:
- `line` = one track (`JunctionLineSpec{1, 0, 1}`), the request's `starts` and `goals` = the G1 windows (same pieces), `blocked` = the G1 fence tiles plus, for B->S and N->B, the G1 `under_spans`, `existing` = the layout so far (its track and heads are obstacles), `min`/`max` = G1's box, `curve_spacing` and `s_bend_window` = G1's base values, `rail_piece` = spec rail_piece, `turn_cost` = G1's base turn_cost, `bridge_span_ok` / `bridge_price` / `max_bridge_middle` = G1's (bridge price from `spec.prices.bridge`), `terrain` and `plane` as G1. No `resource_cost`, no `rules`, no `prio_window`, no tunnels (`tunnel_price` unset).
- The B row is not laid when A->S and N->A route (as in G1 the old router's crossable W row was free): confirm by reading G1; if the W row IS laid at that point, stop and report.
- Add the route to the layout from the line route's raster (find how the organic code or the line router tests turn a `JunctionLineRoute` into layout track and bridges; use that existing function; if none exists, stop and report).
- Keep the old router path available behind a parameter for comparison only in tests: add `bool line_router = true` as the last parameter of `ConstructJunctionTMap`.
Tests:
- Test 1 ("the constructed T is rule-clean") now runs with the line router (default) and must stay clean for both specs. Red: write the parameter and a test "the line-router T matches or beats the old-router T": for both specs, line-router occupied + slivers <= old-router occupied + slivers, both clean. Run it before Part 2's code exists (parameter ignored = identical, so make the red step a check that `micros_route` with `line_router = true` is less than with `false`; it fails while both use the old router).
- BENCH prints both routers' lines.
Commit: `Change: Junction T map edges routed by the line router`.

## Do not
- No sweep, no alternatives, no change to decisions, windows, fences, skeleton, finish, signal pass or checker.
- Do not touch `GenerateJunction`, `BuildJunctionCandidateWithGap`, `junction_organic.*`, savegame, network or command code.
- No `std::to_string`/`std::printf` in tests; no `pkill`, broad `kill` or bare `git stash`.

## Stop conditions
- With the line router, a spec is not rule-clean: stop before commit 2 (commit 1 stays), report violations (rule, tile, message), the log and an ASCII render of both routers' layouts.
- The line router cannot express a G1 request (for example a start window the line router's `starts` cannot take): stop and report which field.
- `[junctionmap]` over 5 s.

## Gates
0 warnings; `[junctionmap]` green under 5 s; once at the end `"[junction]~[slow]~[mapexport]"` green (report time; `map_export JSON of a hand-built area` fails before this step, D-noted in STATUS). Two commits on `wt-g2` with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
References read; commits; red evidence; members added; suite results with times; BENCH lines (Debug) for both routers with the phase split; stops; `git status --porcelain`, `git log -2`.

When you have reported, stop.
