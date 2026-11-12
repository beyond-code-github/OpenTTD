# P2b: the solve renders tagged routes and signals them by tokens; LL_RR solves on flat ground (DESIGN-G1 §13.3, §13.8 items 4, 9)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

**Source rule (D213):** the solved junction's routes are produced from the template and its graph input (the template's paths keep their lane path and copy); nothing traces a layout. The layout is output only.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (Debug `build`, Release `build-rel`). First: `git checkout -B wt-p2b <HEAD>` (given at launch). Never use git stash.
- P1/P1b: JunctionNodeInputOfGraph (junction_nodes.cpp) builds the input from the graph with per-piece tags; P2a: SignalJunctionTrackRoutes(layout, routes, topology, paths, graph, programs, spec) reads only the graph and the tagged routes (layout write-only). SolveJunctionNodes (4 arguments) renders tracks with RenderJunctionNodeTracks and signals them with the legacy T pass PlaceJunctionSignals.
- Reference only (do not merge or cherry-pick): branch wt-np3 commit 33686f7496 has a stopped attempt whose LL_RR flat-terrain test setup you may copy (its routes-from-layout tracing is the thing this step must NOT do).

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ part); docs/junction-tool/TESTS.md (`[topology]` row); DESIGN-G1.md §13.8 (end).
- src/junction_nodes.h (JunctionNodeInput, JunctionNodePiece, JunctionNodeLink, JunctionNodeDiagonal, JunctionNodePath, RenderJunctionNodeTracks, SolveJunctionNodes); junction_nodes.cpp RenderJunctionNodeTracks (whole), NodeDiagonalRaster, NodeDiagonalSteps, SolveJunctionNodes (whole), JunctionNodeInputOfGraph (whole; its path sort).
- src/junction_topology.h JunctionTrackPieceTag, JunctionTrackRoute, SignalJunctionTrackRoutes.
- src/tests/junction_topology.cpp: RenderOf, SignalledOf, P1's tests; `git show 33686f7496 -- src/tests/junction_topology.cpp` for the flat terrain/facts setup only.

## Tests (tag `[topology]`; record each red)
1. **Outer, "topology: the LL_RR graph solves on flat ground"**: from RenderOf's outputs build the graph input and its template; flat facts covering the box with an 8-tile margin; `SolveJunctionNodes(tpl, spec, facts, {0, 0}, topology, paths, graph, programs, input)`. REQUIRE no failure; the solved layout's tracks per tile equal SignalledOf's layout's tracks (prio element included); its signals equal SignalledOf's as a set (tile, trackdir, type); `CheckJunctionRules(layout, spec, topology)` is empty (INFO each). Red: the overload first calls the legacy 4-argument solve; record the failure.
2. **The same for the T** ("topology: the T graph solves on flat ground").
3. **Inner, "topology: the node render's routes at flat values are the input's routes"**: RenderJunctionNodeRoutes(tpl, input, tpl.flat) gives, for both topologies, routes equal to RenderOf's routes (path, copy, pieces, tags), in RenderOf's order. Red: stub returns {}.
- All existing tests stay green and unedited.

## Changes (junction_nodes.{h,cpp})
1. **JunctionNodeInput** gains `std::vector<std::pair<uint16_t, uint8_t>> routes; ///< Per path: its lane path and copy (graph input only; empty when traced).` JunctionNodeInputOfGraph fills it.
2. **Routes of a solved template:**
```cpp
/**
 * The routes of the template's paths at the given values (DESIGN-G1 §13.3): rigid pieces shifted by their classes, free links drawn at their solved length (X/Y runs, free diagonals by NodeDiagonalRaster), every piece keeping the tag of the input piece it renders. In the graph's route order (lane path, then copy).
 * @param input The graph input the template was built from (its tags and routes).
 */
std::vector<JunctionTrackRoute> RenderJunctionNodeRoutes(const JunctionNodeTemplate &tpl, const JunctionNodeInput &input, const std::vector<int> &values);
```
   Per template path i (= input path i): walk its points; between consecutive points, a rigid link's pieces are the input path's pieces between them, each shifted by its JunctionNodePiece's classes (find by tile and track); a free X/Y link's interior is one piece per tile between the solved ends with the input's trackdir along it; a free diagonal's interior is NodeDiagonalRaster's interior at the solved halves, translated as RenderJunctionNodeTracks does. Bridge (on_bridge) pieces: shifted by the bridge's classes as RenderJunctionNodeTracks shifts tpl.bridges. Tags: a shifted piece keeps its input tag; a generated interior piece takes the tag of the link's first interior input piece (a link with no interior input piece: the tag of its `from` piece with token RUN). Route path/copy from input.routes[i]; sort the result by (path, copy). If a piece cannot be classed, STOP.
3. **Solve overload:**
```cpp
/** SolveJunctionNodes for a graph input (D213): its routes rendered from the template and signalled by SignalJunctionTrackRoutes (DESIGN-G1 §12) instead of the T pass. */
JunctionNodeSolve SolveJunctionNodes(const JunctionNodeTemplate &tpl, const JunctionSpec &spec, const JunctionFacts &facts, TileIndexDiffC offset, const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, const JunctionTrackPrograms &programs, const JunctionNodeInput &input);
```
   Move the 4-argument body into a file-static implementation; at "6. Render." the legacy path keeps PlaceJunctionSignals; the overload renders RenderJunctionNodeTracks as today, then `SignalJunctionTrackRoutes(layout, RenderJunctionNodeRoutes(tpl, input, result.values), topology, paths, graph, programs, spec)`, then NormaliseJunctionLayout. (Offsets: translate as RenderJunctionNodeTracks' output is translated today; if the overload needs a translation the 4-argument path does not do, STOP.)

## Do not
- Do not trace any layout; do not change the template, the builder, the 4-argument solve's behaviour, the search, GenerateJunctionNear, the token rules.

## Stop conditions
- Test 1 or 2: solve failure (report the JunctionNodeFailure), tracks or signals differ (first 10 differing tiles), or checker violations (each, plus RenderJunctionAscii to docs/junction-tool/wip/p2b-llrr.txt). Do not fix them.
- An existing test fails; anything not covered.

## Gates
- `tools/junction/jt fast build '[topology]' 5`; `tools/junction/jt fast build '[nodes]' 5`; `tools/junction/jt gate build-rel` (report the count).
- One commit: `Add: Junction node solve renders tagged routes and signals them by tokens`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. TESTS.md `[topology]` row: add "P2b" and the count.

## Report
The standard report, plus Release time of the LL_RR solve overload (median of 5). When you have reported, stop.
