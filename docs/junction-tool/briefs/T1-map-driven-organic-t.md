# T1: the map-driven organic T, end to end (DESIGN-O4 revision 5, D170)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/o4c2`, branch `wt-o4c2`, head `03fffdac7e` (the stopped O4c2 work, a WIP commit on top of `4ab6b010b9`). Build dir: use the worktree's existing build directory (`ls -d build*`).
- Plan: `docs/junction-tool/DESIGN-O4.md` "Revision 5: the theory" (read it in the main checkout, `/home/pete/OpenTTD/docs/junction-tool/DESIGN-O4.md`, lines 1-31; the worktree copy is older).
- Why: O4c2 stopped because the main lines, routed first, took every legal merge spot: the prio row was a mask derived after the fact. In revision 5 the prio is part of the map: the main line carries its prio track with it as a two-track stretch ending at the merge, so nothing else can take it. Projection failure (no route, no convergence) ends the candidate and the next candidate is tried.

## Read first (once each)
- `CODINGSTYLE.md`.
- Main checkout `docs/junction-tool/DESIGN-O4.md` lines 1-31; `docs/junction-tool/briefs/O4c2-organic-rules-in-routing.md` (the step you build on).
- `src/junction_line_router.h` whole; in `src/junction_line_router.cpp` the rule-state code (`RulesStart`, `RulesStep`, `RulesHeadAllowed`, `RulesAfterHead`, `LineNode`) and how `min_units`, `goals` and `resource_cost` are applied.
- `src/junction_organic.{h,cpp}` and `src/tests/junction_organic.cpp` whole (the WIP).
- `src/junction_signal_pass.cpp` ~260-300 (`GeometryOfPrio`) for which side the prio row lies on.

## What to build

### 1. Router: a prio window carried by the line (`junction_line_router.{h,cpp}`)
Add to `JunctionLineRouteRequest`:
```cpp
/** A prio carried by this line (DESIGN-O4 revision 5): its last `units` units before the end are straight axis pieces with the parallel piece on `side` also the line's own track. */
struct JunctionPrioWindow {
	uint units = 0;        ///< Prio coverage plus one segment, in JunctionPiece::units (as CheckWaitingRoom counts).
	int8_t side = 0;       ///< +1 or -1: the companion tile is at tile + side along the axis perpendicular to travel.
	int end_lo = 0;        ///< The window's last piece lies at an along-travel coordinate (x for X-axis travel, y for Y) in [end_lo, end_hi].
	int end_hi = 0;
};
std::optional<JunctionPrioWindow> prio_window;
```
And to `JunctionLineRouteResult`: `size_t prio_end = 0; ///< Index in route pieces of the window's last piece, when prio_window was set.`

Semantics, single-track requests only (error "prio window needs one track" otherwise):
- The route ends at a goal as today, but between the start and the goal it contains exactly one window: a run of consecutive surface pieces on the same axis, all straight (no turn inside the run), no bridge or tunnel piece, whose units sum to at least `units`, whose last piece's along-travel coordinate is in `[end_lo, end_hi]`.
- For every piece of the window, the companion piece (same track, tile offset by `side` perpendicular to travel) must be inside `min..max`, not blocked, not used by the line, and is paid as the line's own: `rail_piece` plus `resource_cost(companion, z, Surface)`. It is occupied by the line for the rest of the route (the route may not reuse it).
- Implement as router state like the O4c1 rule state: phase Before / In (units counted, straight run) / After. Entering In is allowed from any surface straight piece; a turn or a non-surface piece in phase In before `units` is reached resets to Before; leaving In after `units` is reached, with the last In piece's coordinate in range, moves to After; a goal counts only in phase After. Include the phase and the In-units (capped at `units`) in the prune key.
- `JunctionLineRoute` gets the companion pieces: add `std::vector<JunctionPiece> companion; ///< The prio window's parallel pieces (DESIGN-O4 revision 5).` to `JunctionLineRoute` in `junction_line.h`, and the raster/layout conversion the organic code uses must lay them as track (they are the prio row: unused by any movement, which is what the checker's prio component expects).

Tests (in `src/tests/junction_line_router.cpp`, red first):
- "a prio window carries its companion track": flat 30x6 box, straight line along x from (0,2) to (29,2), window units = 8 units-worth of pieces, side +1, range [10,20]: route found; the companion pieces are 1 tile +y of a straight run whose last piece's x is in [10,20]; no companion outside the run. Red: compile error is not acceptable red, so first add the members with the window ignored and see the assertion fail.
- "a prio window is never on a curve": the only straight run long enough lies outside the range; with the range forcing it onto the curve, no route ("no route" error).
- "a companion tile that is blocked moves the window": block the +y tiles of x 10..15; the window ends at x >= 16 + its length - 1 or the route fails if out of range.
- The existing router suites and the `[.bench]` cases unchanged (window unset).

### 2. The map for the T (`junction_organic.{h,cpp}`)
```cpp
/** One candidate structure of the T (DESIGN-O4 revision 5): the forced crossings' over/under, and where each prio window may end. */
struct JunctionTMap {
	std::vector<std::pair<int, int>> over; ///< Forced pairs (over, under), as JunctionNegotiationPrices::over.
	struct Window { int line; int joiner; int8_t side; int end_lo; int end_hi; }; ///< Movement indices (JunctionTMovements() order).
	std::vector<Window> windows; ///< One per merge into a main line.
	int bound = 0; ///< Admissible lower bound on occupied tiles, for ordering.
};
/** The candidate structures of the T for a box and ports, cheapest bound first. */
std::vector<JunctionTMap> JunctionTMaps(const JunctionSpec &spec, const std::vector<JunctionPort> &ports, int width, int height);
```
- `over`: the 2^k over/under assignments of `JunctionForcedPairs`, main-line-over first (as the WIP's case order).
- Windows: one per merge into a main line (N->W into W->W's line, N->E into E->E's line). `side` is the side away from the branch edge (the O4c2 brief's -y, as `GeometryOfPrio`: opposite the joiner's approach). `units` comes from the spec: `(prio_tiles + sd + 1)` tiles converted to units the same way the WIP computed W; pass it at projection, not in the map. End ranges: split the along-travel span the window end can take inside the box (from `units` tiles in from the main line's entry port to the box's last column before its exit port) into chunks of 6 tiles; each chunk is a candidate range. The N->W and N->E chunks combine as a cross product.
- `bound`: sum over the six movements of the Manhattan tile distance between its ports, with each main line's window chunk adding nothing, plus for each joiner the Manhattan distance from its port to the nearest end of its window chunk instead of to its Out port when larger. Sort ascending by bound, ties by `over` order then chunk order.
- Rules on the map: drop a candidate whose windows overlap a forced crossing's unavoidable span is out of scope; do not add map rule checks in this step (the router and checker enforce the rules). Signals and throughput on the map are later steps: do not add them.

### 3. Projection: `NegotiateJunctionMovements` takes a map
- Add `const JunctionTMap *map = nullptr;` to `JunctionNegotiationPrices`. When set, `over` comes from `map->over` and each main-line movement that carries a window is routed with `prio_window` = {units, side, end_lo, end_hi} for its window.
- The joiner (N->W, N->E) has `goals` = the piece after `prio_end` on its line's current route (the merge piece), instead of the `may_merge` mask; delete the `may_merge` mask and the side-tile reservation (the companion pieces now occupy those tiles as the main line's own resources, so other movements conflict with them through the existing pricing). Keep `may_split` (D108 coverage: no split of that main line inside its window plus one segment), waiting room and D113 as in the WIP.
- Round order stays: main lines first, then the turning movements.
- A movement with no route in a round: the negotiation ends with `error` set (as now). That ends this candidate.
- Add `bool apply_rules = true;` to `JunctionNegotiationPrices`. When false, no movement rules, no masks and no windows (the O4b1 negotiation as merged). The three O4b1 tests that now fail ("a T on flat ground converges", "movements from one port share their first pieces", "crossings go over or under") pass `apply_rules = false`.

### 4. Candidates: `BuildOrganicT`
- Box: the ports move one row in from the top so the E line's prio row (on -y) fits: add `int top = 0` as the last parameter of `MakeOrganicTPorts` (all ports' y + top), and `BuildOrganicT` calls it with `top = 1` and `height + 1`.
- Replace the crossing-case loop with: for each map of `JunctionTMaps`, in order, negotiate; finish and judge as the WIP does (crop, signal pass, checker, prio timing gate); stop at the first rule-clean one. Record in `cases` one line per candidate tried: bound, over pairs, window chunks, outcome (rounds and "clean", or the error, or the first checker violation's rule and message).
- Stop after 24 candidates at one p (record it).
- The crop margin keeps the companion rows.

## Tests (outermost first, `src/tests/junction_organic.cpp`)
1. "the map-driven organic T is rule-clean": `GenerateJunctionOrganic(MakeTestJunctionSpec(), ml_turn_cost = rail_piece x tl as the WIP tests use)` gives a layout; `JunctionRulesClean` true. Red: on the WIP head this fails (O4c2 handback: unclean at every p).
2. Same for `PinnedTestSpec` and the TL6 branch_gap 0 narrow spec (the WIP's tests 2 and 3).
3. "the prio row travels with the main line": in the clean test-spec layout, for each main-line merge, the prio component the checker finds lies on the companion pieces (assert via `CheckPrio` empty and that the companion tiles carry track no movement uses).
4. "maps are ordered by bound": `JunctionTMaps` on the test spec at p 20 returns at least 8 maps, bounds non-decreasing, the first has main-line-over for every forced pair.
5. BENCH: for each spec and each p tried, print one line `BENCH spec=<name> p=<p> candidates=<n> rounds=<r> micros=<us> occupied=<n> slivers=<n> clean=<0|1>` and the winning `cases` block (to the test output file, not your report).

## Do not
- Do not add map-level rule checks, signals on the map, throughput on the map, the D80 lift, terrain, or multi-lane.
- Do not let the prio window bend (straight only in this step; the engineer expects bent prios to be rare).
- Do not add fallbacks for a no-route (it ends the candidate), price tuning, or feedback loops.
- Do not change savegame, network or command code.
- Do not use `std::to_string` or `std::printf` in tests (poisoned); use `fmt::format` or `std::cout`.
- Do not use `pkill` or broad `kill`.

## Stop conditions
- Test 1 is not clean at any p after the 24-candidate cap: stop, commit nothing, report the `cases` blocks of the two best p (bound order) and the checker's first violation per candidate.
- Any merged suite (router, rules, signal pass, generator) fails for a reason you did not cause, or you cannot make the companion pieces lay as an unused prio row without changing the checker.
- The window semantics above turn out ambiguous for a case you meet (for example a window crossing the box's crop margin).

## Gates
- Build `openttd_test` with 0 warnings. Run `[junction]`-tagged suites (or the whole test binary if tags don't separate them) to a file; all green.
- Generation time per p is reported, not gated, in this step.
- Squash to one commit on `wt-o4c2` on top of `4ab6b010b9` (replacing the WIP commit `03fffdac7e`): `Feature: Junction organic T projected from a map of its structure`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short, this format)
- References read
- Commit: hash and subject
- Red evidence: each new test and the failure seen before the code
- Types and members added or removed: names only
- Suite results
- BENCH lines (all of them) and, per spec, the winning p, candidates tried, rounds, micros, occupied + slivers
- Stops and anything left undone
- State: `git status --porcelain` and `git log -1`

When you have reported, stop.

## Amendment 1 (engineer ruling D170a, sent while running)
Replaces section 4's candidate loop and the first stop condition. The map is MADE to fit, never rejected:
- `BuildOrganicT` projects the FIRST map of `JunctionTMaps` only (bound order; ties main-line-over first).
- When its negotiation fails (no route, or not converged in max_rounds) or the finished layout is not rule-clean, grow the box and project the same map again: width + 4 and height + 2 per step (ports recomputed with `MakeOrganicTPorts` for the new size, the map's window ranges recomputed by calling `JunctionTMaps` on the new box and taking the map with the same `over` and the same chunk indices), up to 4 steps. Record each attempt in `cases` (box, outcome).
- If it still fails after 4 steps: stop, commit nothing, report the `cases` blocks (that is a missing map constraint for the orchestrator).
- `JunctionTMaps` and test 4 stay as written. The 24-candidate cap is gone.

## Amendment 2 (from the T1 diagnosis, scratchpad t1/DIAG.md)
Four causes found at 44x17; fix each, test-first. Box growth stays as amendment 1 but should rarely be needed.
1. **Stale sibling (rule 7).** A turning movement's rules are satisfied against its sibling's route at the time it was routed; the sibling is rerouted later (or cleaned up) and the merge/split moves. Fix: a movement is stale when any movement sharing its In port or its Out port has a different route than when it was last routed; at the end of each round (and after the clean-up pass) reroute every stale movement, repeating until none is stale (at most 3 sweeps, then the round counts as unconverged). Converged means no conflicts and nothing stale. This generalises your joiner refresh.
2. **Counting gap of 2 units (rule 7).** CheckWaitingRoom counts from the piece after the split to the last signal before the merge (the piece before the merge piece). The router counts from the first private piece to the merge. Fix in the organic code: wait_units = required units + the units of the two end pieces the checker does not count (compute them, do not hard-code 2, using the same units function), OR change the router count to the checker's span; choose the organic-side fix (no router change). 
   Agreement test (red first): "router waiting room agrees with the checker": the organic T at 44x17, p 16 (test spec), finished layout: for every turning movement, CheckWaitingRoom reports no violation. Also a unit-level agreement test: a two-movement split/merge on a flat box with wait_units set to the checker's minimum: the finished layout passes CheckWaitingRoom; with one unit fewer required by the checker (spec tl reduced so the minimum is one less than the route's), it still passes; and a route forced exactly at the minimum passes.
3. **Bridge heads next to switches (rule 6).** (a) A turning movement split one tile past the far head of the main line's bridge: may_split must also be false on the first `plain_before_head` + 1 shared pieces after any far head (bridge or tunnel) of the shared route, and on the last `plain_before_head` + 1 shared pieces before a near head. (b) The W main line's near head with nothing signal-legal between it and the previous signal: in the prio window state, after the window's last piece the route needs the merge piece plus `plain_after_merge = 1` surface non-head piece before any head; add `uint plain_after = 1` to JunctionPrioWindow. Agreement test: "bridges keep a signal-legal piece from every split and merge": in the finished 44x17 layouts at p 20, 22, 24, CheckSignals reports no "split in a bridge's block" and no "too far apart".
4. **Companion laid as track (dead end, missing prio, stray track).** The checker expects the prio pieces the signal pass lays (PlacePrio: half pieces at the merge tile, axis plus half beside joins), not a plain row. Fix: companion pieces are NOT laid as track. They stay resources during negotiation (the main line's own, as now) and are left empty in the layout; the signal pass's existing prio placement lays the prio on them. If the signal pass does not put its prio row on exactly those tiles (wrong side or row), stop and report which side/row it chose versus the companion.
5. **Search limit.** Turning-movement requests get max_expansions 1,000,000 (the main lines keep 200,000). Report expansions per call in the BENCH line as max_expansions_seen.
Gates, commit and report as in the brief; add DIAG-style one-liners for any remaining rejection.
