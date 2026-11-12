# P4a: solve failures name the graph elements they hit (DESIGN-G1 §13.4, §13.8 items 5, 9; D213)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

**Source rule (D213):** the elements come from the graph input's tags carried on the template; nothing reads a layout to find them. This is what lets P4b turn a failure into a graph edit.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (Debug `build`, Release `build-rel`). First: `git checkout -B wt-p4a <HEAD>` (given at launch). Never use git stash.
- P1-P3 (merged): template built from the graph input (per-piece JunctionTrackPieceTag), graph solve overload, port-bend leans. JunctionNodeFailure (junction_nodes.h) names template indices only: Straight/Break/Edge a link, Piece an X class, Height a Y class plus JunctionNodeHeightFault (part, tiles), Occupancy a row.

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ part); docs/junction-tool/TESTS.md (`[topology]` row and the table's format); DESIGN-G1.md §13.8 items 5 and 7.
- src/junction_nodes.h: JunctionNodePoint, JunctionNodeTemplate, JunctionNodeHeightFault, JunctionNodeFailure, DiffJunctionNodeTemplates; junction_nodes.cpp: ReadPoints (where points are made from input pieces), every place SolveJunctionNodesWith sets `result.failure`, the occupancy step (AddJunctionNodeOccupancy and how its failure is reported).
- src/tests/junction_topology.cpp: P2b/P3 tests (graph solve on flat facts, flat terrain setup), P1's oracle test; src/tests/junction_nodes.cpp BlockMapTestTile and the comparator test.

## Tests (record each red)
New tag `[topoproj]` (add a TESTS.md row: "graph projection solves on facts (P2b, P3, P4a...)", budget 5 s Debug, measured time). Move the P2b flat-solve tests and P3's two tests from `[topology]` to `[topoproj]` (tag edit only; update the `[topology]` row's count).
1. **Outer, "topoproj: a house on LL_RR's W in-lane run names that run"**: the graph pipeline on LL_RR; flat terrain as in P3's test; block the tile of the W in-lane route's port run 4 tiles in from its port (the first route whose first edge starts at a W in slot; the piece at index 4 of that route, which is a RUN piece of its first edge: assert that). Solve by the graph overload. REQUIRE a failure; its `elements` contains a tag with that route's first edge and token RUN. INFO the failure kind and elements. Red: `elements` empty (record).
2. **"topoproj: a house on a port bend names the bend's token"**: block the tile of the first piece of the same route's in bend (P3's rule: first token piece on the first edge), on terrain where the bend cannot move clear of it (also block every tile of that route's port row from the port to the exit split's node piece except the bend's own tiles; if the failure is then not a Piece or Height failure, report its kind and adjust nothing). REQUIRE `elements` contains a tag with that edge and the bend's token index.
3. **"topoproj: every failure on LL_RR carries an element"**: for each of 20 tiles evenly spread over the solved flat layout's track tiles (deterministic: every k-th tile in sorted order), block it alone and solve: whenever there is a failure, `elements` is non-empty, and every element's edge is a graph edge. Report the kinds seen.
- All existing tests stay green and unedited except the tag moves and the comparator test (VEC/field for the new template member).

## Changes
1. JunctionNodeTemplate gains `std::vector<JunctionTrackPieceTag> point_tags; ///< Per point: the graph element of its input piece (graph input only; empty when traced).` filled in ReadPoints from input.tags. Comparator compares it; P1's oracle test copies it from the graph template into the traced one before diffing (as P3 did for lean).
2. JunctionNodeFailure gains `std::vector<JunctionTrackPieceTag> elements; ///< The graph elements it hit (graph input only; empty when traced): Straight/Break/Edge: the link's run (its from point's tag with token RUN, or the first interior input piece's tag); Piece: the tags of the class's points; Height: the tags of the points whose tiles are in height->tiles (or of the part's points when tiles is empty); Occupancy: the tags of both overlapping elements.` Fill it wherever result.failure is set, from tpl.point_tags (skip when point_tags is empty). For Occupancy, if the step does not know the two elements, extend its report to name the two template points (or classes) it compared — the smallest change that names them; if that needs more than reporting what the step already compares, STOP and report.
3. Nothing else: no repairs, no search changes (P4b).

## Do not
- Do not change any constraint, the snap, the lean, the legacy failures' existing fields, the search, GenerateJunctionNear.

## Stop conditions
- Test 1 gives no failure (the solve routes around the house: report the solved fan position and stop; the test premise is then wrong); Occupancy needs more than reporting; an existing test fails; anything not covered.

## Gates
- `tools/junction/jt fast build '[topology]' 5`; `tools/junction/jt fast build '[topoproj]' 5`; `tools/junction/jt fast build '[nodes]' 5`; `tools/junction/jt gate build-rel` (400 before; report the count). Check whether `jt gate` picks up the new tag (if the gate lists tags explicitly, add it there and say so).
- One commit: `Add: Junction node solve failures name the graph elements they hit`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus the failure kinds and elements for tests 1-3. When you have reported, stop.

## Amendment 1 (orchestrator, after the two test-premise stops; the premises were the brief's error)
- Test 1 becomes "topoproj: a house on LL_RR's W in-lane port run names its edge": block the route's piece 1 (the port run's tile next to the port; it is rigid with the bend, so it cannot be avoided). REQUIRE a failure whose elements contain a tag with the route's first edge. (Piece 8 being avoided by the solve is correct behaviour: record it in the test as a second SECTION: blocking piece 8 solves, and the house tile holds no track.)
- Test 2 is dropped.
- Straight/Break/Edge elements: add `std::vector<JunctionTrackPieceTag> link_tags; ///< Per link: the tag of its first interior input piece, else its from point's tag with token RUN (graph input only).` to JunctionNodeTemplate (filled where links are made; compared by the comparator; copied in P1's oracle test like point_tags), and use it for those failures instead of the from point's tag.
- Piece/Height/Occupancy stay class-coarse for now (P4b refines what it needs).
