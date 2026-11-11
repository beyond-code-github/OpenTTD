# G1c(ii-a): seed placement, the tile of every node, bend, crossing and bridge head (DESIGN-G1 §11, §11.8)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (persistent; Debug `build`, Release `build-rel`). First: `git checkout -B wt-g1c2a <HEAD>` (given at launch). You are the only writer there. `tools/junction/jt` builds openttd_test before running.
- G1b/G1c(i) give, for a topology: lane paths, a track graph, an order (rows, edge_crossings, band layers), a structure (row stack, bridges, turns) and programs (per edge, a token list of Fan, Corner, SBend, Over, Under). This step places them on tiles: one pure function, numbers only (no layout, no render, no signals). Nothing calls it yet. The render is the next step.
- This is the most rule-dense step so far. Every constant below names its rule. If a rule below does not decide a value for some edge of either topology, STOP and report the edge and what is missing; do not invent a constant.

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ part); docs/junction-tool/TESTS.md (the `[topology]` row).
- docs/junction-tool/DESIGN-G1.md §10.7, §10.8, §11.8 (the end of the file). FABLE-G1c2.md (the review §11.8 answers; its F1-F4 explain why nothing here is loose).
- src/junction_topology.h (whole) and src/junction_topology.cpp from ProgramJunctionTrackGraph to the end of that function.
- src/junction_route.h (JunctionRoute, JunctionRaster, RasterizeJunctionRoute: the canonical raster of a move string F/L/R) and src/junction_chain.cpp:25 (JunctionCurveGaps).
- src/junction_generator.h :39-55 (JunctionSpec: tl, prio_tiles, reference_speed, curve_speed, max_bridge_middle) and junction_generator.cpp :88 (DeriveCurveSpacing: CL from the spec) and :238-247 (JUNCTION_SHIFT_BACK_TILES).
- src/tests/junction_topology.cpp: ProgramsOf and the two program tests.
- docs/junction-tool/RECON-turn-programs.md §1 (the traced T's tiles: the expected values below are those tiles shifted, see test 1).

## Design (settled)
Add to junction_topology.h after ProgramJunctionTrackGraph (include "junction_layout.h" if JunctionBridge/TileIndexDiffC need it), implement in junction_topology.cpp after it:
```cpp
/** A token's tiles: its first and last tile in the edge's travel order (DESIGN-G1 §11.8). */
using JunctionTrackSpan = std::pair<TileIndexDiffC, TileIndexDiffC>;

/** The seed placement of a programmed track graph (DESIGN-G1 §11.8): tiles only, x east, y south, (0, 0) the box's north-west corner. */
struct JunctionTrackPlacement {
	std::vector<int> item_rows; ///< Per stack item, its tile row.
	std::vector<TileIndexDiffC> ports; ///< Per port slot, its boundary tile.
	std::vector<TileIndexDiffC> nodes; ///< Per graph node: a split's first diverging half piece, a merge's last joining half piece.
	std::vector<std::vector<JunctionTrackSpan>> edges; ///< Per edge, per token of JunctionTrackPrograms::edges.
	std::vector<std::vector<JunctionTrackSpan>> copies; ///< Per edge, per token of JunctionTrackPrograms::copies.
	std::vector<std::array<JunctionBridge, 2>> bridges; ///< Per structure bridge: copy 0's and copy 1's heads, start = the head the carrier reaches first; a single bridge's copy 1 is {{0, 0}, {0, 0}}.
	TileIndexDiffC size; ///< The box.
};

/**
 * The seed placement (DESIGN-G1 §11.8): the least tiles that meet the rules, Y first then X.
 * @param error Receives the reason when the result is empty (no nodes), e.g. a bridge longer than spec.max_bridge_middle.
 */
JunctionTrackPlacement PlaceJunctionTrackGraph(const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, const JunctionTrackOrder &order, const JunctionTrackStructure &structure, const JunctionTrackPrograms &programs, const JunctionSpec &spec, std::string *error);
```
Implement inside the function with lambdas and local structs (none at file scope). Solve each phase as a system of difference constraints `v - u >= k` (equalities as two), least solution by longest paths from a source fixed at 0 (Bellman-Ford; a positive cycle is an error "the placement rules contradict each other"). Phase Y first (rows), then phase X with the rows known. Where a Y constraint needs a column count from X (an SBend's size), start with the count the slices give (below), solve Y, solve X, and repeat while any SBend's column count changed (at most 4 rounds, else error "the placement does not settle").

### Shapes (exact, from the raster)
- CL = DeriveCurveSpacing(spec.reference_speed, spec.curve_speed, spec.tl) when spec.curve_limit, else 2. h = the least odd number >= min(2·TL - 1, max(2, CL)) (JunctionCurveGaps' consecutive same-hand gap, made odd); k = (h - 1) / 2. A Corner has h half pieces and moves k columns and k rows. A diverging joiner's Corner (it starts at its band split, §10.7 4a) has 2·k' + 1 halves with k' = its split row - its approach row, and k' >= k, else error "a corner from a band node is too short".
- An SBend moving m lines has 2m half pieces; it moves m lines sideways and m - 1 along its heading.
- The tiles of a bend: rasterize it with RasterizeJunctionRoute from its first half piece (moves: "R" or "L", then "F" per further half, as NodeDiagonalRaster at junction_nodes.cpp:730 does) and read every half piece's tile. Use these offsets for everything below that needs a bend's tile on a given row or column; do not derive them by hand.
- A Fan is an SBend between the port row and its stack item's row; m = |difference|; m = 0 means no bend: its span is (port tile, port tile).

### Phase Y
- Stack items on consecutive rows within a lane's triplet (=1). Between triplets >= 1. Item 0 on row 0.
- A band b < R with events lies between row b-1's last item and row b's first item; its depth is whatever its chains need (no variable of its own).
- Band R: its layer-1 nodes share one row (D115a: both switches on one row); the S edge row = that row + STUB, STUB = 2 (D206); S ports sit on the S edge row.
- Each non-row edge, in travel order: a run between two tokens or between a node and a token has length >= 1 tile along its heading; exactly where a rule says so:
  - single bridge carrier: one plain piece between each head and its neighbouring bend or node (D113); its middle = 2, the crossed SBend's two tiles in the carrier's column; its heads one row beyond (FABLE-G1c2 F4);
  - an exit Corner starts at its exit split on the split's row (the upstream item: the copy row on a doubled row, §10.6 F5);
  - a straight joiner's Corner ends on its approach item's row; a joiner's final SBend ends on its lane item's row at the merge tile (D75);
  - a straight child entering a band merge has one plain piece above the merge tile (G6, D113: T (28,10));
  - a vertical edge's tile on another edge's row is allowed only where that row's tracks do not cover its x (checked after phase X: error "a column meets a side track").
- Port rows on the W and E sides: the notation rows of the main line, one per lane plus one per "_" (D38/D142; the T's L_R: rows o, o+2), offset o chosen to minimise the largest Fan m over both sides, ties to the smaller o.

### Phase X (rows known)
- W ports at x = 0, E ports at one x (the box's last column), S port columns by the branch notation: in-port then out-port, out = in + 2 for L_R (the "_" column).
- Slices (§10.8): in each row's item list (order.rows) and in each band layer's item list before and after its events, consecutive vertical items are >= 1 column apart (D142). A crossed SBend under a single bridge starts in the column next to the carrier (exact). A straight child keeps its parent's column; a diverging SBend child's column is free (>= its slice neighbours).
- Exit Corner: column = exit split x + k (heading E) or - k (heading W). Joiner Corner: its last half is k columns past its column toward its heading; the approach run continues from the next tile.
- Row chain of a doubled row, u = +1 heading W, -1 heading E (the upstream step); Z0, Z1 = every crosser's tiles on the copy row and on the lane row (from the shapes); exact:
  - copy 0 heads: hw0 = min(Z0 ∪ (Z1 - u)) - 1, he0 = max(Z0 ∪ (Z1 - u)) + 1 (D80, D118, D145); copy 1's heads = copy 0's + u; n = |he0 - hw0| - 1; n > spec.max_bridge_middle: error "bridge <edge> needs <n> middle tiles, more than <max>" (D208);
  - lane split = copy 0's near head + 2u; exit split = lane split + 2u (D110(3), D145);
  - copy 1's SBend onto the lane row: first half on the lane split tile, ending on its near head's row; copy 0's SBend: first half on the tile after its far head (far head - u), ending on the lane row; that last tile is the lane merge; copy 1 has one plain piece between its far head and the lane merge.
- Free row runs (>= floors): fan end to exit split >= 2 (a plain piece and the split's signal slot); lane merge to joiner merge >= spec.prio_tiles + 1 (D145 p + k_w, k_w = 1); undoubled row: exit split to joiner merge >= spec.prio_tiles + 1 (D148); joiner merge to the Out Fan's first half = JUNCTION_SHIFT_BACK_TILES (D145) when the Fan has m > 0, else joiner merge to port >= 2; Out Fan's last half to port = 2 (signal tile + boundary, D58/G10, as the stub); In side: port to In Fan's first half >= 2.
- Joiner approach run: from the tile after its Corner to its final SBend's first half >= 1.
- The box: size = (largest x + 1, S edge row + 1).

### Token spans
Corner, SBend, Fan: first and last half piece. Under (doubled): this edge's tiles on that copy's row, in travel order. Under (single): this edge's tiles in the carrier's column. Over: the crossing edge's tiles under this bridge (on this copy's row, or in this carrier's column), in this edge's travel order.

## Tests (tag `[topology]`; record each red)
Helper `PlacementOf(topology, spec)` beside ProgramsOf. Spec: MakeTestJunctionSpec() (CL 5, TL 4 so k = 2; prio_tiles 14), with max_bridge_middle set per test.
1. "topology: the T's placement" (max_bridge_middle 64). The traced T (RECON-turn-programs §1) shifted one column west east of its lane merge to prio merge run (the trace has 16 there; the rule floor p + 1 = 15 is one less; everything else is the trace's shape), so expect exactly:
   - item_rows {0,1,2,3,4,5}; size {40, 14}; ports {(0,1), (0,3), (25,13), (27,13), (39,3), (39,1)}.
   - nodes {(22,1), (25,11), (32,3), (6,4), (27,11), (37,1)}.
   - edges: 0 {((0,1),(0,1))}; 1 {}; 2 {((39,3),(39,3))}; 3 {}; 4 {((22,1),(24,3)), ((24,3),(24,3)), ((24,4),(24,4)), ((24,7),(27,9)), ((25,7),(25,8))}; 5 {((27,3),(26,3)), ((24,3),(24,3)), ((21,3),(21,4))}; 6 {((32,3),(30,5)), ((30,9),(27,11))}; 7 {((25,11),(19,5)), ((6,5),(6,4))}; 8 {((25,8),(25,7)), ((25,4),(26,4)), ((26,3),(27,3)), ((25,4),(27,2)), ((37,2),(37,1))}; 9 {((2,4),(2,3))}; 10 {}; 11 {((39,1),(39,1))}.
   - copies: 5 {((30,3),(30,4)), ((26,4),(25,4)), ((24,4),(24,4))}, the others empty.
   - bridges: 0 {{(28,3),(22,3)}, {(29,4),(23,4)}}; 1 {{(25,9),(25,6)}, {{0,0},{0,0}}}.
   If you get a different tile anywhere, do NOT change the expectation: STOP and report the differing entries with the rule that produced yours.
2. "topology: LL_RR's w2 bridge is too long at nmax 10": max_bridge_middle 10: empty result, error starting "bridge 10 needs". Report the n you got.
3. "topology: LL_RR with an L_R sideline: placement" (max_bridge_middle 64): non-empty, and: X_e1's column - X_e2's column == 3 (their Under spans under w1's bridge); band 2's depth (rows strictly between item 5's row and item 6's row) == k = 2; every bridge's n <= 64; for every doubled bridge, its crossers' columns on the copy row are consecutive integers where they are straight; S ports on the last row. Report the full placement (nodes, bridges, size) in your report for the orchestrator.
Red for each: does not compile; then a stub returning `JunctionTrackPlacement{}`: each fails on its first CHECK/REQUIRE.
4. Update the `[topology]` row in TESTS.md. The gate stays green.

## Do not
- No render, no JunctionLayout, no signals, no template, no call from the generator or the solve. Do not change any earlier function or expectation. No constant that the brief does not name; no slack added "to be safe".
- Do not use the word "shift" for a sideways step (S-bend).

## Stop conditions
- Test 1 differs (report entries and rules). A rule here leaves a value undecided for an edge of either topology (report). Phase Y or X has a positive cycle on either topology. Test 3's checks fail (report which, with the numbers).

## Gates
`tools/junction/jt fast build "[topology]" 5`, `tools/junction/jt fast build-rel "[topology]" 1`, `tools/junction/jt gate build-rel`. Commit: `Add: Junction seed placement (DESIGN-G1 §11.8)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
References read; commit; red evidence; types/functions added; the LL_RR placement (nodes, bridges with n, size); suites with times; stops; `git status --porcelain` and `git log -1`. When you have reported, stop.
