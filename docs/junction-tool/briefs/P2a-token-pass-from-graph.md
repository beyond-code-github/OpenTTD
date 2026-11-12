# P2a: the signal-token pass reads the graph and its routes, never a layout (DESIGN-G1 §13.8 item 4; D213)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

**Source rule:** after this step SignalJunctionTrackRoutes takes everything it reads from the graph and the tagged routes; the JunctionLayout parameter is write-only (it receives the signals and the prio element's tracks). This is what lets the solve (P2b) signal a solved junction from its solved routes.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (Debug `build`, Release `build-rel`). First: `git checkout -B wt-p2a <HEAD>` (given at launch). Never use git stash.
- P1/P1b (merged): routes carry per-piece tags (JunctionTrackPieceTag: edge, token or RUN, run, copy), each edge's pieces ending at its node (a merge's node piece belongs to the in-edge; a split's node piece to the out-edge); JunctionNodeInputOfGraph builds the template input from the graph.
- RECON-graph-input.md Q9 (read it) lists every layout read in SignalJunctionTrackRoutes (junction_topology.cpp) and its graph replacement, plus PlaceDragSignals (junction_signal_pass.cpp:157) reading the layout.

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ part); docs/junction-tool/TESTS.md (`[topology]` row); RECON-graph-input.md Q4, Q5, Q9.
- src/junction_topology.h :245-300 (JunctionTrackPieceTag, JunctionTrackRoute, signal tokens, SignalJunctionTrackRoutes); junction_topology.cpp SignalJunctionTrackRoutes (whole) and JunctionNodeInputOfGraph's events code (junction_nodes.cpp).
- src/junction_signal_pass.{h,cpp} PlaceDragSignals (:157-200) and its two other callers (junction_signal_pass.cpp:208, junction_line.cpp:392).
- src/tests/junction_topology.cpp: SignalledOf and the ST1/ST2 tests.

## Tests (tag `[topology]`; record each red)
1. **Outer, "topology: the token pass reads no layout"**: for the T and LL_RR: tokens_a and layout_a = today's call on the drawn render (as SignalledOf); tokens_b from the new signature on an EMPTY JunctionLayout (no tracks, no signals, no bridges). REQUIRE tokens_b == tokens_a (every field, same order), and layout_b's signals equal layout_a's signals as sets, and layout_b's tracks equal (layout_a's tracks minus the drawn render's tracks), i.e. exactly the prio element's tracks. Red: with the new parameters added but the body unchanged, the empty layout gives different tokens (record).
2. **"topology: route events from tags"**: for both topologies, JunctionRouteEvents(graph, programs, route) equals, per route, the events JunctionNodeInputOfGraph computed for that route (match by lane path and copy). Red: stub returns {}.
- All existing ST1/ST2/CS1/P1 and "seed is clean" tests stay green (update their calls to the new signature: the only allowed edit; SignalledOf passes graph and programs).

## Changes
1. **Route events** (junction_topology.h after JunctionTrackRoute; .cpp):
```cpp
/**
 * The switch events of a route from its tags (DESIGN-G1 §13.8 item 4): at each boundary between consecutive edges, the node piece, a split when graph.nodes[node].split; on a doubled edge, the lane split (copy 1's SBend token's first piece) and the lane merge (copy 0's SBend token's last piece). Sorted by piece, a split before a merge on one piece.
 */
std::vector<std::pair<bool, size_t>> JunctionRouteEvents(const JunctionTrackGraph &graph, const JunctionTrackPrograms &programs, const JunctionTrackRoute &route);
```
   (Use the same pair type as SwitchEvent in junction_rules_internal.h if it is that pair: `using SwitchEvent`; then return std::vector<SwitchEvent>.) JunctionNodeInputOfGraph uses it instead of its own events code (P1's oracle test must stay green).
2. **SignalJunctionTrackRoutes** gains `const JunctionTrackGraph &graph, const JunctionTrackPrograms &programs` after `paths`, and inside:
   - every SwitchEventsOf call -> JunctionRouteEvents(graph, programs, routes[r]);
   - the joiner (tile coincidence) -> the graph rule of RECON Q9: the merge node n at the route's merge event; the joiner is the first route in route order whose lane path has an edge e with graph.edges[e].to == n and another movement; k = that route's node piece index for n;
   - every read of layout tracks (JunctionTracksAt etc.) -> a local per-tile TrackBits map built at the start from all routes' non-bridge pieces, updated whenever the pass draws prio-element tracks;
   - bridge heads -> a local set of tiles: the first and last on_bridge piece of each maximal on_bridge run of every route;
   - signals read back (IsSignalledPiece, "no signal on the track") -> a local set of (tile, trackdir) the pass maintains as it places signals;
   - ok()/IsLegalSignalPiece -> RECON Q5's rule over those three locals plus the pass's existing index conditions;
   - layout: only push_back of signals and setting prio tracks (write-only).
3. **PlaceDragSignals overload** (junction_signal_pass.h/.cpp), the walk unchanged, its three layout uses as callbacks:
```cpp
/** PlaceDragSignals over callbacks: `signalled` and `legal` answer for a piece, `place` puts a signal on it (the token pass's own state, no layout). */
void PlaceDragSignals(const std::vector<JunctionPiece> &pieces, uint8_t sd, const std::function<bool(const JunctionPiece &)> &signalled, const std::function<bool(const JunctionPiece &)> &legal, const std::function<void(const JunctionPiece &)> &place);
```
   The layout version calls it with lambdas over the layout (behaviour unchanged for its other callers). The token pass calls the overload with its locals.

## Do not
- Do not change any token rule, order, reason or the prio element pattern; the checker; the old signal pass; JunctionNodeInputOfGraph's results.

## Stop conditions
- Test 1 differs (report the first differing token and which read it came from); an existing test fails; anything not covered.

## Gates
- `tools/junction/jt fast build '[topology]' 5`; `tools/junction/jt fast build '[nodes]' 5`; `tools/junction/jt gate build-rel` (report the count).
- One commit: `Change: Junction signal-token pass reads the graph and its routes, not a layout`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. TESTS.md `[topology]` row: add "P2a" and the count.

## Report
The standard report. When you have reported, stop.

## Amendment 1 (orchestrator, after the lane-event stop)
Option 1, with the sibling route: the doubled leg's lane split and lane merge are switches both copies pass, located by the copy that bends there. Signature:
```cpp
std::vector<SwitchEvent> JunctionRouteEvents(const JunctionTrackGraph &graph, const JunctionTrackPrograms &programs, const std::vector<JunctionTrackRoute> &routes, size_t r);
```
For routes[r], on each doubled edge e of its lane path: the sibling route s = the route with the same lane path and the other copy. Lane split tile = the first piece tagged (edge e, copy 1, the SBend token of programs.copies[e]) in whichever of r/s is the copy-1 route; lane merge tile = the last piece tagged (edge e, copy 0, the SBend token of programs.edges[e]) in the copy-0 route. The event is routes[r]'s first non-bridge piece on that tile (split: true; merge: false). Node events as the brief says. Routes are graph output (tagged), so this is not a layout read. Callers pass the whole route vector. Test 2 compares per route as before. Also update the fourth caller, src/tests/junction_design.cpp:438.
