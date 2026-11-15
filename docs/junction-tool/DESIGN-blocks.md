# DESIGN-blocks: block templates (B2, the PSG 227 join block first)

Orchestrator design, 2026-11-15. It implements DESIGN-modules §4.1 and §6 B1-B2 on the REVIEW-width-overprescribed Q4 interface: per-track ports, floors between them, pieces as no-touch occupants, stretch by floors. D193 (valid by construction), D282/D283/D287 (prio on the fixed track; our prio element and parallelograms), D296 (no invented geometry), D297 (seed under 5 s).

## 1. Idea
A block is a **block program**: data, not code per shape.
- **Points.** Column variables along the block's heading. Columns are solved as the least solution of difference constraints (`JunctionPositions`), so the solve is cheap.
- **Lines.** Each track runs on a line across the heading. Lines are template data: integers, where 0 is the outermost line on the left of travel.
- **Edges.** Each graph edge is one track from a start (port or node) to an end (node or port). It is a list of segments:
  - `Run(to point)`: straight along the current line to the point's column;
  - `SBend(lines)`: a sideways step of a signed number of lines (+ is right of travel). It starts at the current column, and its width is measured from its raster;
  - `Bridge(to point)`: the near head is at the current column and the far head at the point.
  - An edge ending at a node ties the node's point exactly to the column where its last segment ends.
- **Paths.** Each lane path is a list of edges. The route of a path is its edges' moves joined (D295 one route per track).
- **Rule families** turn the program into floors. They apply to every program, never to one site:
  - port stub;
  - signal room around splits, merges and heads;
  - the bridge-over-diagonal hug;
  - queue bays;
  - no touching per line;
  - curve gaps per path;
  - prio room.

  A floor exists only because a rule asks for it. If the checker run finds a violation, the template is missing a rule (D193), and we never add padding to make it pass.
- Every quantity a floor needs (an S-bend's width, which columns it covers on each line, its path units, its bend positions) is **measured from the raster of the segment's moves**. Nothing is typed in as a per-shape constant.

The same machinery later carries the keep and balanced-no-prio joins, the exit block (B3) and width 3. Each is a new program table, not new code.

## 2. The PSG 227 join block (two bundles in, two lanes out)
The block heads east. Lines run top to bottom (0 is the north side):

| line | west (in) | east (out) |
|---|---|---|
| 0 | | prio row of L0 |
| 1 | fA (bundle A, fixed) | L0 |
| 2 | cA (bundle A, choice) | |
| 3, 4 | free (cA1 and cB0 run here) | |
| 5 | cB (bundle B, choice) | |
| 6 | fB (bundle B, fixed) | L1 |
| 7 | | prio row of L1 |

Graph (§4.1): splits sA (cA) and sB (cB); merges mC0 = cA0 + cB0, mC1 = cA1 + cB1, mL0 = fA + c0 (prio on fA), mL1 = fB + c1 (prio on fB); one crossing, cA1 over cB0. Edges, with their segments:
- fA: Run(mL0). It is straight on line 1. L0: Run(E).
- cA In: Run(sA). cA0: Run(mC0) on line 2.
- cA1: SBend(+1), Run(hN), Bridge(hF), Run(d1), SBend(+2) ends at mC1. Line 3 carries the bridge; the descent runs 3 to 5.
- cB In: Run(sB). cB1: Run(mC1) on line 5.
- cB0: SBend(-1), Run(b0), SBend(-2) ends at mC0. It runs 5 to 4, then along 4, then 4 to 2. Its diagonal crosses line 3 under cA1's span.
- c0 (mC0 to mL0): Run(e0), SBend(-1) ends at mL0. c1 (mC1 to mL1): Run(e1), SBend(+1) ends at mL1.
- fB, L1: the mirror of fA and L0.

Paths and reach (D283):
- fA→L0: fA, L0.
- cA→L0: cA In, cA0, c0, L0.
- cA→L1: cA In, cA1, c1, L1.
- cB→L0: cB In, cB0, c0, L0.
- cB→L1: cB In, cB1, c1, L1.
- fB→L1: fB, L1.

So each fixed track reaches its own lane, and each choice track reaches both lanes.

The bundle gap at the west ports is 2 lines (lines 3 and 4). That is a floor the core must meet, by Q4 point 2. The out lanes sit on lines 1 and 6. Bringing them together for the arm is composition's fan (H1), not part of the block.

### 2.1 Revision after B2b (D298, D299): a doubled crossing
The B2b checker run found that cA1's bridge must be doubled: a choice leg carries main-line traffic (D80, D174, D188 → D299). The revised table:

| line | west (in) | east (out) |
|---|---|---|
| 0 | | prio row of L0 |
| 1 | fA | L0 |
| 2 | cA | |
| 3, 4 | cA1's two decks (copy 0 on 3, copy 1 on 4) | |
| 5 | cB0's bay run | |
| 6 | cB | |
| 7 | fB | L1 |
| 8 | | prio row of L1 |

- **Doubled edge.** `JunctionBlockEdge::copy` holds copy 1's segments; it is empty for a single edge. An S-bend may carry a role, LaneSplit or LaneMerge, as `JunctionTrackStep::Role` does. In the graph's programs, a doubled edge gets one SBend token: on copy 1 with role LaneSplit, and on copy 0 with role LaneMerge. The bend pieces are tagged with that token, so `JunctionRouteEvents` finds the lane split and the lane merge exactly as it does for render's doubled legs. Every path through a doubled edge has two routes, copy 0 and copy 1, each running port to port.
- **cA1** runs from sA to mC1.
  - Copy 0: SBend(+1), Run(ls), Run(hN0), Bridge(hF0, crosses cB0), Run(lm'), SBend(+1, LaneMerge) ends at lm, Run(d1), SBend(+2) ends at mC1.
  - Copy 1: SBend(+1), Run(ls), SBend(+1, LaneSplit), Run(hN1), Bridge(hF1, crosses cB0), Run(d1), SBend(+2) ends at mC1.
  - Both copies have the same bends, so their lengths are equal (D80 line sync). A test asserts it.
- **cB0**: SBend(-1) (6 to 5), Run(b0), SBend(-3) (5 to 2) ends at mC0. Its diagonal passes under both decks. Each deck's heads hug the diagonal on that deck's own line (R4, applied per deck), so the parallelogram comes from the hug and is not a separate rule.
- **cA0**: Run(mC0) on line 2. **cB1**: Run(mC1) on line 6. **c0**: Run(e0), SBend(-1) to mL0 on line 1. **c1**: Run(e1), SBend(+1) to mL1 on line 7.
- **R2 and R3, stated generally.** Between a split tile and the next near head on a track, there is at least one axis tile that no other track shares. The same holds between a far head and the next merge tile or bend. This covers the lane split and lane merge (placement's LaneSplit/Heads floors, D145).
- **Topology.** It lists the prio movements fA→L0 and fB→L1 (D298). The checker then asks for a prio only at mL0 and mL1.

## 3. Rule families (the floors)
Units are the rasteriser's piece units: an axis tile is 2 and a diagonal half piece is 1, as the checker counts them. TL is `spec.tl` and P is `spec.prio_tiles` rounded up to even.

| # | Family | Floor | Rule |
|---|---|---|---|
| R1 | Port stub | Every track's first node or bend is at least 2 columns inside the in edge. Every out track runs at least 2 columns from its last node or bend to the out edge E | D206, D115a, `JunctionRunFloor` Port/Node 2 |
| R2 | Split to near head | At least one plain axis tile on the head's line between a split's bend and the near head | signals: "Between a split and the next bridge head there is a signal-legal piece" |
| R3 | Far head to merge | At least one plain axis tile between the far head and the next bend | signals: "likewise a merge after a far head" |
| R4 | Bridge over a diagonal | The crossed segment's pieces on the bridge's line lie strictly between the heads, and none touches a head tile (`JunctionUnrelatedTileGap` of the head's axis piece and the nearest diagonal piece) | placement: "a single bridge … crosses the edge's S-bend at a column its diagonal allows"; D217 |
| R5 | Queue bay | On every non-through path into a merge, the units from the piece after its upstream split to the last signal-legal piece before the merge are at least 2 TL. A piece is not signal-legal when another track stands on its tile | checker WaitingRoom (D83, D88) |
| R6 | Merge signal room | At least one plain axis tile between a merge tile and the next bend on the merged track | signals AfterMerge/BeforeMerge |
| R7 | No touching per line | On each line, unrelated occupants in their template order (west to east) are at least `JunctionUnrelatedTileGap` apart. Related means the two tracks of one node, at that node | placement no-touch family, D217 |
| R8 | Curve gaps | `JunctionCurveGaps` over each path's bends. A gap between two bends of one fixed shape must already hold (assert); any other gap is a floor between points through the units affine in columns | D93, `JunctionCurveGaps` |
| R9 | Prio room | The fixed track has no node and no bend from mL - P - 1 to mL. The prio row line (0 or 7) holds nothing else. mL - P is at least 2 (outside the in stub) | signals prio rules; checker "no split of a main-line lane inside a prio's coverage" |

R5 does not apply to the through paths fA→L0 and fB→L1. The template's topology makes them the through movements (§4).

The least solution is the block at its tightest. The stretch variables (split spacing, prio length, bridge length) are the same points, and floors only bound them below, so composition can lengthen any of them without breaking a rule.

## 4. Outputs (the seam, RECON-template-seam (a))
`JunctionBlock`:
- `topology`: single-lane groups. W: fA, cA, cB, fB, all main, on lines 0, 2, 3, 1. E: L1, L0 in slot order (south to north on the E side), on lines 1 and 0, all main. Movements: the six paths. fA→L0 and fB→L1 are same-line through movements, and the other four are turning movements.
- `paths`, `graph` (nodes, edges, path_edges; crossings = {cA1, cB0}), `programs` (`edges` and `copies` sized to the edges and empty, `nodes` and `ends` empty): enough for the signal pass (RECON-template-seam §3).
- `routes`: one per path, from the west port tile to the east port tile. Tags per piece: the piece's edge, token `RUN`, run 0, copy 0.
- `layout`: the routes rasterised and normalised.
- `prio`: two `JunctionPrioSpan`s at mL0 and mL1, built with the placement's step-bits pattern (its prio side is away from the joiner: north of fA, south of fB).
- `in_ports`, `out_ports`: per track, the tile, the trackdir and the order across.
- `points`: the solved columns, by name, for tests.

## 5. Steps
- **B2a. Geometry.** The block program types, the generic generator (measure, floors R1-R4 and R6-R8, solve, emit moves) and the 227 table. The routes, layout and ports as output.
  - Tests: connectivity through the checker's `CheckConnectivity` with the block topology (exactly the six movements); per-family invariants on the solved points; an ASCII dump to the scratchpad.
  - R5 and R9 floors are in, because they set lengths. Signals are not run yet.
- **B2b. Signals and the oracle.** The graph, tags, topology and prio spans. Run `SignalJunctionTrackRoutes`, then `CheckJunctionRules` on the block.
  - Acceptance: zero violations.
  - Each violation is reported as a missing rule family, and is not patched.
- **B3. Exit block.** A new table: 2 splits and 1 crossing, the mirror image.
- **H1.** Composition with the bundle T.

## 6. Open (orchestrator, decide at B2b's result)
- Whether a diagonal piece under a span is signal-legal. R5 counts only to the last legal piece, so the floor holds either way. B2b will show which.
- Whether `SignalJunctionTrackRoutes` puts a prio at mC0 and mC1 (choice-leg merges, D282: no prio). The template passes only the two spans. If the pass insists on drawing more, that is a signals change for B2b.
