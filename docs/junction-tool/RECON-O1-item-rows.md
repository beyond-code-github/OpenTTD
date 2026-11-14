# Recon O1: what stands on each stack item row today (read-only; D268)

Head 55f7eefd9a. jp = `src/junction/placement/junction_placement.cpp` at that head (line numbers are of the unmodified file). Probe: `docs/junction-tool/wip/o1-item-rows-probe.patch` (dump hook in `add_occupancy`/`hug_heads` plus a hidden test `[probeo1]` in `junction_topology.cpp`); full dump: `docs/junction-tool/wip/o1-item-rows-dump.txt`. All seed numbers are at the flat solution `add_occupancy` receives on the last record (the flat solve of the relations, no facts, no floors), seeds T, LL_RR x L_R, L_R x LL_RR built as the ARR1 test does. Anything not read or run is marked INFERRED.

## Summary
- 3a: 0 reversals. 637 occupant pairs that have an item (T 92, LL_RR x L_R 396, L_R x LL_RR 149): none has the west item's occupant strictly east of the east item's. 68 consecutive row pairs by extent: 3 not strictly ordered, all one shared tile (gap 0), none reversed.
- 3b: no band-edge crosser lacks an item (0 of 0 in all three). Without an item on item rows: doubled-bridge heads T 4, LL_RR 24, L_R x LL_RR 8 (4 per bridge); the rest of the leg's own pieces T 11, 37, 13 (S-bends, runs); prio footprint tiles 46, 92, 46 (23 per prio element); other rows' port-stub fans (Fan, PortTile, Run) 0, 24, 0. Single-bridge heads (`BE.Heads`) are never on an item row in the seeds.
- 3c: crossers of item rows are covered by Corner / SBend tiles and vertical tiles only (T 6 Corner, 3 VTile; LL_RR 10, 12 SBend, 19; L_R x LL_RR 6 SBend, 9 VTile). The 46 crosser tiles between a doubled bridge's heads at the seed (T 7, LL_RR 29, L_R x LL_RR 10) are all in the Under's candidate set (0 outside): next bend head 20, previous bend tail 8, run 18 (a run tile is any of RunBefore / RunAfter, see 3c).
- 3d: unrelated pairs sharing a tile on an item row: 0, 8, 0. LL_RR: 3 are `BE.SBend` (crosser e8) with `BE.Corner` (e11 at node n7), 5 are two other rows' port fans (`RE.Fan` with `RE.Fan`).
- 5: the rule below (travel position q from graph, order, programs and arrangement) orders 2437 unrelated occupant pairs on item rows the way the seed does (T 369 of 369, LL_RR x L_R 1526 of 1529 with 3 at gap 0, L_R x LL_RR 539 of 539): 0 disagree, 0 ties. 624 pairs involve a port-stub fan of another row, which the rule leaves out (A6's fans).

## 1. Sources: what `add_occupancy` puts on a row (jp 1307-1431)
`Occupant` (jp 1309-1322): row, west end (lp, ldx), east end (rp, rdx), track bits, edge, copy, node, footprint. Ends are a point plus an offset; when recording they are also `JunctionSystemColumn`s (point, c0, c1 * L(bend)). The row y of every occupant is a number: `vy[point]` of its point, or `port_row[slot]` for a port tile and a fan's far end (jp 1181-1237), or for a bend `y + offs[j].y`. Points: `item_pt[i]` (one per stack item) and `node_pt[n]` are made at jp 203-204; occupants use `node_pt` for node tiles and chain-element points (`el.first`, `el.last`, the S-bend and Corner anchors) for pieces. `item_pt` is used only for its row (`vy[item_pt[i]]`), never for a column.

| source (jp) | pieces | west / east end | points |
|---|---|---|---|
| node tile (1373) | one tile on the node's row | (node_pt[n], 0) both | `node_pt` |
| non-row edge chain Tile (1379) | one tile at `el.first`, skipped when it is a node's | (el.first, 0) both | chain point |
| Heads (1381-1384, band edges' single bridges) | two tiles at `el.first` and `el.last`, on their own rows | (p, 0) both | chain points |
| Corner, SBend, Fan (1386-1387; `add_bend` 1346-1371) | one tile per half, offsets from `bend_tiles`, first and last skipped when on a node | (anchor, offs[j].x) both; record form `JunctionBendTileForm` dx0 + dx1 * L | chain anchor |
| vertical tiles between elements (1393-1394) | one Y tile per row strictly between `el.last` and `next.first` | (el.last, 0) both | `el.last` |
| horizontal run between elements (1396) | `add_run` (1343): tiles strictly between two points | (west point, +1) to (east point, -1); empty (right = left - 1) when no tile | the two run end points |
| row-chain Tile (port tile, 1405) and HeadsPair tiles (1212-1215 in `row_pieces`) | port tile; the two head tiles of a doubled bridge, one pair per bridge per copy | (el.first or el.last, 0) | chain points |
| row-chain Run (1406; `row_pieces` 1226-1229) | run between consecutive elements of in edge, leg (both copies), out edge | as `add_run` | chain points |
| row-chain Bend (1407; Fan, SBend of the leg: lane split, lane merge) | `add_bend`, `piece.hand`, `piece.halves`, never skipped | (anchor, offs[j].x) | anchor |
| prio footprint (1415-1431) | per step 0..even_prio: a tile on the Prio item's row (`y_row`) and, when the step has lane bits (`JunctionPrioStepBits`), one on the lane row (`y_lane`); `footprint = +-1` | (prio_el.last, -step * sgn) both | `prio_el.last` |

Copies: a doubled leg's chains give copy 0 and copy 1; the run from the start to the lane split and the run after the lane merge carry copy -1 (shared). `related()` (jp 1434) lets two occupants stand on one tile when the same edge and copy (or either copy -1), a node tile with an incident edge, or the two parts of one prio footprint. A band edge crossing a row, and the heads of a bridge, are unrelated to each other and are separated by the occupancy.

## 2. Item rows vs other rows
- The item rows are exactly the distinct `vy[item_pt[i]]`: 6 of 6, 12 of 12, 6 of 6 stack items give distinct rows at the seeds (the stack is one row per item).
- Non-item rows holding row-chain pieces (RE.*): only the port-stub fans of In and Out edges and their port tile and run: LL_RR rows 7 (ports of e6, e19); L_R x LL_RR rows 3-10 (e0, e20) and 12-19 (e4, e16). The fan of an edge of one row also crosses the stack rows of another row between its port row and its item row (LL_RR: e19, e20 fans on row 2's stack rows 8 and 9; e24/e25 and e0/e1 on row 1). No S-bend of the leg, no copy piece, no Run of a leg stands on a non-item row; the lane split and lane merge S-bends stand on the two item rows (copy row and lane) they join.
- Band-edge chains (BE.*) occupy many non-item rows (the bands). Single-bridge Heads stand on those only.

## 3. At the seeds
Per item row the dump lists each occupant west to east with kind, edge, copy, node, chain index, token and item; all of it is in the dump file.

(a) Order vs `arrangement.row_pairs`. Each occupant maps to an item: node tile to its node; a `BE.*` occupant whose edge is an item of the row to that edge; a `BE.*` of an edge incident to the row's split or merge node to that node; an In-edge piece to the split node; an Out-edge piece to the merge node. Heads, the leg's own pieces, prio footprint and other rows' fans have no item. Result: 0 inversions (pairs of mapped occupants with the west item's occupant strictly east), and 3 consecutive row pairs not strictly ordered by extent, all LL_RR x L_R: node n7's block (band edge e11's Corner) and crosser e8's SBend tile share a tile at columns 27, 28, 29 on rows 10, 11, 12 (gap 0, legal as Upper|Lower).

(b) Occupants with no item, per hub (item rows only):

| kind | T | LL_RR x L_R | L_R x LL_RR |
|---|---|---|---|
| band-edge crosser with no item | 0 | 0 | 0 |
| doubled-bridge heads (RE.HeadTile) | 4 | 24 | 8 |
| leg runs (RE.Run) | 7 | 25 | 9 |
| leg lane split / merge S-bends | 4 | 12 | 4 |
| prio footprint tiles (23 per element) | 46 | 92 | 46 |
| other rows' port-stub fans (Fan 16 + PortTile 4 + Run 4) | 0 | 24 | 0 |

(c) Which element covers each crosser row. Crosser tiles of item rows (every tile of a crosser edge on an item row of a row it crosses): T Corner 6, VTile 3; LL_RR Corner 10, SBend 12, VTile 19; L_R x LL_RR SBend 6, VTile 9; no Heads tile, no Run. Against `arrangement.unders` (every Under token of a doubled bridge, head row of its copy, crosser tiles strictly between the heads): 38 Unders, 46 tiles, all inside the candidates:
- bend tile whose token is the nearest bend after the Under: next bend head, T 4, LL_RR 16 (8 Corner + 8 SBend), L_R x LL_RR 0;
- bend tile whose token is the nearest bend before the Under: previous bend tail, T 2, LL_RR 2, L_R x LL_RR 4;
- vertical tile (`BE.VTile`): a run, T 1, LL_RR 11, L_R x LL_RR 6. The occupant does not say which run (before or after the Under token): a run between two bends is split into runs by the Under tokens, one `VTile` occupant per tile. 6 of these tiles follow a first element (token 0) that is not a bend before the Under (the edge's start tile or fan), still a run.
So a bend head is the commonest cover (20 of 46), then runs (18) and bend tails (8); the vertical tile of "between elements" is never a separate candidate piece, it is RunBefore or RunAfter. INFERRED: the O1 crosser column at a head row for a run is the run's column; for a bend tile P3 generalised, as DESIGN-G4 O1 says.

(d) Pairs of unrelated occupants at gap 0 on item rows: T 0; LL_RR x L_R 8 (SBend with Corner 3, port fan with port fan 5); L_R x LL_RR 0. The 5 fan pairs are two rows' fans overlapping on one tile (A6/O2 scope).

## 4. The hug
- `hugs` is rebuilt every X round (jp 1613). In the doubled-leg build (jp 1757-1788), per bridge b of the leg, a `HugCandidate{west head point, east head point, edge, row, tiles}` takes, for every crosser x in `bridge.crossed` and for both head rows (copy 0's `heads0(b)`, copy 1's `heads1(b)`), every tile of x on that row (`edge_tiles`, symbolic: point + dx, bend tiles dx from the raster). For each tile it adds two relations that keep it strictly between the heads: `ge(z.pt, near_far.first, 1 - dx)` and `ge(near_far.last, z.pt, 1 + dx)` (sides swapped when s < 0): not `order_pin`, static, 2 per tile (T 14, LL_RR 58, L_R x LL_RR 20). A crosser without a tile on the head row returns `build_why = "Z row ..."` (jp 1784): the refusal.
- `hug_heads` (jp 1519-1536), called by `detect` (jp 1592) after `add_occupancy`: per hug, the lowest and highest tile value at the flat solve (`flat[pt] + dx`, offset by `-u` for copy 1's row) and two equalities: `west == low - 1`, `east == high + 1`. Each equality is two relations, all tagged `order_pin` (4 per hug: T 4, LL_RR 24, L_R x LL_RR 8). They pin the heads one tile outside the outermost crosser at the flat order, and the joint model drops them.
- Occupancy of heads and crossers: heads are `RE.HeadTile` occupants of the leg's edge and copy, crossers are unrelated `BE.*` tiles, so `add_occupancy` orders them by flat position and adds one clash row per adjacent pair of occupant groups (gap 1, 2 when they touch): the adjacent head/crosser pairs at the seeds are 4, 24, 8 (2 ends x 2 rows per bridge). Heads and crossers of different bridges are ordered only through the chain of adjacent pairs and the hug's strict-between rows; nested sequences are covered by transitivity of the flat order.

## 5. The static order of a row's occupants (proposed rule)
Define the travel position q of an occupant on a stack row of row r, from the graph, the order, the programs and the arrangement only. Let k be the number of the row's items minus one (order.rows[r] = [first node, crossers, last node]); positions in travel order: the split node s is 0, the crossers 1..k-1 in travel order (order.rows reversed when the row travels west), the merge node m is k. The row's direction is the side of its In edge's port.

| occupant | q |
|---|---|
| In-edge piece (Fan, Run, PortTile) | -1 |
| split node tile, and a band edge of the row's split node | 0 |
| leg: shared run before the lane split | 0.05 |
| leg: lane-split S-bend (role LaneSplit) | 0.1 |
| leg: run before the first bridge | pos(first crosser of bridge 0) - 0.5 |
| bridge head a / b of bridge j | pos(first crosser of j) - 0.4 / pos(last crosser of j) + 0.4 |
| crosser x of the row (any of its tiles) | pos(x) |
| leg: run between bridges j-1 and j | pos(last crosser of j-1) + 0.5 |
| leg: run after the last bridge | k - 0.55 |
| leg: lane-merge S-bend (role LaneMerge) | k - 0.5 |
| leg: shared run after the lane merge | k - 0.4 |
| prio footprint | k - 0.25 |
| merge node tile, and a band edge of the merge node | k |
| Out-edge piece | k + 1 |

Bridge j is the j-th of `structure.bridges` with the leg's edge, `crossed` its crossers; a leg with no bridge has no heads and the leg's run takes q = 0.5. A doubled bridge's two copies lie on two rows (the copy row and the lane); the table applies per row. Within one q, two unrelated pieces can occur only for two band edges of one node (a row node has at most one band edge: split node: In, leg, one band edge; merge node: Out, leg, one band edge), so no tie is expected. INFERRED.

Check against the dump (unrelated pairs of non-empty occupants on item rows; the order is q along the travel direction, the seed's order is by columns): T 369 agree of 369; LL_RR x L_R 1526 agree, 3 at gap 0 (overlap, not orderable by columns), 624 unranked; L_R x LL_RR 539 agree of 539. 0 disagree, 0 ties. The unranked 624 all involve a port-stub fan of another row (Fan, PortTile or Run of an In or Out edge standing on this row's stack rows on its way to its port row). What the rule leaves to O1's author: those fans (DESIGN-G4 A6 and O2); the crosser's column at a head row (O1's piece-choice form); a doubled bridge's two staggered copies on adjacent rows (the table is per row, so the stagger is not a pair of one row).
Caveats: heads are ranked by their order among the heads of the row in column order, which is circular only for the head-to-head order (it cannot fail: heads of one row are ordered by their own bridge index); the shared tile of two band edges at gap 0 (3 cases) is legal today by `JunctionTracksMayShareTile` and must stay at gap 0 in O1's row.
