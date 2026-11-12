# ST1: signal tokens and run rules on the rendered routes (D211; DESIGN-G1 §12.1-12.3, §12.5)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (persistent; Debug `build`, Release `build-rel`). First: `git checkout -B wt-st1 <HEAD>` (given at launch). You are the only writer there. `tools/junction/jt` builds openttd_test before running.
- G1d is merged: the checker can be built for any topology with `MakeJunctionTopologyCheckContext(layout, spec, topology, FindJunctionPorts(layout, topology))`. RenderJunctionTrackPlacement draws tracks and bridges only. This step adds the signals, by rule, as tokens with a reason (D211). The prio element (§12.4) is the next step, not this one.

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ part); docs/junction-tool/TESTS.md (`[topology]` row).
- docs/junction-tool/DESIGN-G1.md §12 (the end of the file); DECISIONS.md rows D113, D192, D206, D209, D211 (grep `^| D113 ` etc.).
- src/junction_topology.h :150-end; src/junction_topology.cpp RenderJunctionTrackPlacement (from its definition to its end, ~:1639-1768).
- src/junction_rules_internal.h (whole, short): SwitchEventsOf, IsLegalSignalPiece, IsSignalledPiece, JunctionSignalWeight, FindBridgeByHead.
- src/junction_signal_pass.cpp :51-160 (PlaceSignal, NearestSignalPiece, PlaceRequiredSignals, PlaceBaySignals) and PlaceDragSignals (:157-200); src/junction_signal_pass.h.
- src/junction_rules.cpp CheckSignals and CheckWaitingRoom (grep the function names; ~:720-940).
- src/junction_route.h :20-45 (JunctionPiece, JunctionRaster); src/junction_layout.h JunctionSignal.
- src/tests/junction_topology.cpp: RenderOf and the render and G1d tests (from "the T render traces like the flat T" to the end).
- docs/junction-tool/wip/diag-T-render-oldpass.txt (the old signal pass on the same T render: what "clean but for the prio" looks like).

## Design (settled)
### Types (junction_topology.h, after RenderJunctionTrackPlacement's declaration)
```cpp
/** One rendered route: a lane path's pieces from its in port to its out port, for one copy (DESIGN-G1 §11.10). */
struct JunctionTrackRoute {
	uint16_t path; ///< Index into the lane paths.
	uint8_t copy; ///< 1 for the copy-1 route of a path through a doubled leg, else 0.
	std::vector<JunctionPiece> pieces;
};

/** Why a signal stands where it does (DESIGN-G1 §12.2, §12.3). */
enum class JunctionSignalReason : uint8_t { BeforeSplit, AfterSplit, BeforeMerge, AfterMerge, BeforeBridge, AfterBridge, SeriesGap, Stub, Bay, Fill };

/** A signal token: a signal placed by a rule of the graph, on one route (D211). */
struct JunctionTrackSignal {
	uint16_t route; ///< Index into the routes.
	JunctionSignalReason reason;
	TileIndexDiffC tile;
	Trackdir facing;
};
```
### Render
RenderJunctionTrackPlacement gains a last parameter `std::vector<JunctionTrackRoute> *routes = nullptr`: when not null it is cleared and, after each successful AddRasterToLayout, receives `{p, c, raster.pieces}`. Nothing else in the render changes.

### The function (declared after the types, implemented after RenderJunctionTrackPlacement)
```cpp
/**
 * Place the signal tokens of a rendered seed (DESIGN-G1 §12.2, §12.3): the anchor signals of every route's splits, merges, bridges and out port, then the whole-train bays before the merges of non-through paths, then drag fill; each drawn into layout.signals once.
 * @return The tokens, in placement order; a signal shared by several routes or rules gives one token per route and rule that resolved to it.
 */
std::vector<JunctionTrackSignal> SignalJunctionTrackRoutes(JunctionLayout &layout, const std::vector<JunctionTrackRoute> &routes, const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionSpec &spec);
```
Definitions used below, for one route r with pieces P (n = P.size()):
- `used` = every track of the layout (a map tile -> TrackBits built from layout.tracks; the render has no unused track yet).
- Events: SwitchEventsOf(layout, used, a JunctionPath whose pieces are P) gives splits and merges by piece index; bridges are maximal runs of pieces with on_bridge (first index a, last index b).
- `ok(i)`: 0 < i < n - 1, !P[i].on_bridge, FindBridgeByHead(layout, P[i].tile) == nullptr, i is not a switch index, and (IsLegalSignalPiece(layout, P[i]) or IsSignalledPiece(layout, P[i])).
- `put(i, reason)`: if !IsSignalledPiece(layout, P[i]), push `{P[i].tile, P[i].trackdir, spec.signal_type, SignalVariant::Electric}` to layout.signals; always push the token `{r, reason, P[i].tile, P[i].trackdir}`.
- Stretch bounds of an event: the previous event's last index and the next event's first index (a switch is one index; a bridge spans a..b; the route's ends are -1 and n).

Phase 1, anchors, routes in order, events in route order:
- split at i: BeforeSplit = the largest ok index in (prev, i); AfterSplit = the smallest ok index in (i, next).
- merge at i: BeforeMerge = the largest ok in (prev, i); AfterMerge = the smallest ok in (i, next).
- bridge a..b: BeforeBridge = the largest ok in (prev, a); AfterBridge = the smallest ok in (b, next). When the neighbouring event on that side is also a bridge, the reason is SeriesGap instead.
- Stub: if ok(n - 2), put(n - 2, Stub).
- An anchor with no ok index is skipped (§12.1: the checker judges).
Phase 2, bays, every route whose path's movement is not a through movement (through = both groups of topology.movements[paths[route.path].movement] are main): exactly PlaceBaySignals's walk (:108-155) on P, with IsLegalSignalPiece/IsSignalledPiece as there and `put(cand, Bay)` instead of PlaceSignal.
Phase 3, fill, every route: remember layout.signals.size(), call PlaceDragSignals(layout, P, spec.sd, spec.signal_type), and push a Fill token for each signal it appended (on this route).
Includes: junction_rules_internal.h and junction_signal_pass.h in junction_topology.cpp.

## Tests (tag `[topology]`; record each red)
Add a helper `SignalledOf(topology, &error, &tokens, &routes)` next to RenderOf: RenderOf at nmax 64 with the routes out-parameter, then SignalJunctionTrackRoutes with MakeTestJunctionSpec(); returns the layout.
1. **Outer, "topology: the T's signal tokens meet the signal rules":** ctx = MakeJunctionTopologyCheckContext(layout, spec, JunctionTTopology(), FindJunctionPorts(layout, JunctionTTopology())); CheckConnectivity, CheckGeometry, CheckBridges, CheckFootprint, CheckSignals, CheckWaitingRoom and CheckDeadlock are each empty (INFO the violations). Red: a stub returning {} without signals (CheckSignals reports missing signals; record the first message).
2. **Tokens are the signals:** every token's (tile, facing) is in layout.signals, every layout signal is some token's, and no two layout signals share tile and facing. Red with the stub: no tokens.
3. **Reasons on the T:** every out route (all routes) has exactly one Stub token, on its piece n - 2; every route with a bridge has, for each of its bridges, a BeforeBridge or SeriesGap token on a piece before the near head and an AfterBridge or SeriesGap token after the far head, unless no ok piece exists there (assert existence only when the stretch has an ok piece). Red with the stub.
4. **LL_RR findings (hidden `[.][diag]`, not committed):** the same pipeline on LL_RR with the G1d checker; write every violation of CheckJunctionRules(layout, spec, JunctionLLRRSidelineTopology()) and RenderJunctionAscii(layout) to docs/junction-tool/wip/st1-llrr.txt; also the T's full CheckJunctionRules to wip/st1-t.txt (Prio and PrioReach violations are expected there).
- Every existing test stays green and unedited. If one fails, STOP.

## Do not
- Do not build the prio, change any signal type to Entry/Combo/Exit, or remove a redundant signal (§12.4, next step).
- Do not change placement, programs, the old signal pass, the checker, or junction_nodes.
- Do not call PlaceJunctionSignals or MakeJunctionCheckContext's legacy form from the new function.

## Stop conditions
- Test 1 is not clean on the T: report each violation with RenderJunctionAscii of the T to a file you name, and which phase placed (or failed to place) the signal at that tile. Do not adapt a rule to make it pass.
- An existing test fails.
- Anything not covered here.

## Gates
- `tools/junction/jt fast .claude/nodes/build '[topology]' 5` and `tools/junction/jt fast .claude/nodes/build '[rules]' 10` green; then `tools/junction/jt gate .claude/nodes/build-rel` green (report the count, and the count before).
- One commit: `Add: Junction signal tokens on the seed render (D211, DESIGN-G1 §12)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. TESTS.md `[topology]` row: add "ST1" and the new test count.

## Report
The standard report, plus: token counts per reason for the T and LL_RR, the two findings files' paths, and LL_RR's violation counts per rule. When you have reported, stop.
