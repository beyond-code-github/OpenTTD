# RECON: per-row and per-kind code that keeps DESIGN-G1 §16.1 unmet (RC16)
Read in src/junction_topology.cpp (all line numbers there; feature/junction-tool at 7f5bc41e06), .h, DESIGN-G1 §10/§11/§14/§16, REVIEW-opus Q1(d)/Q3. Nothing built or run. Anything not read is marked INFERRED. clangd not used (grep + reads).

## 1. Inventory (30 sites: 7 stack-kind, 12 row-role, 6 prio, 5 token-position/role-tag)
Stack-kind (branch on Lane/Copy/Approach/Prio/PrioAndCopy, or on "item before Lane that is not Approach"):
- K1 :585-637 StructureJunctionTrackGraph: per row, joiners (>1 refused :608) and "crossed" give prio/approach/copy per side, then Kind by if-chain. The source of the kinds; stands in for: one item per (edge needing a side track).
- K2 :709-718 (program) and K3 :997-1008 (Place, a verbatim duplicate): lane_item, approach_item, north_item (= item before Lane unless Approach). Stands in for: "the item a token ends on".
- K4 :819-832 leg tokens: LaneMerge/LaneSplit SBend index = lane_item; in-edge Fan index = north_item when the row is doubled, else lane_item. Stands in for: the exit item / lane item of the leg's chain.
- K5 :841 joiner Corner index = approach_item; :886 arrival SBend index = lane_item (the joiner's S-bend).
- K6 :1475-1476 HeadsPair index: copy 0 = north_item, copy 1 = lane_item (the row each copy's heads lie on).
- K7 :2183, :2232-2241 exit_item = doubled ? north_item : lane_item; bend1/bend0 first/last pinned to north_item/lane_item. :1975 prio row = lane item row +-1.
Row-role (in_edge/leg_edge/out_edge/doubled_row/is_in_out_edge):
- R1 :725-745 program: counts exactly one in, one leg, one out per row, else "not ruled yet"; :762-768 row_doubled only on the leg.
- R2 :983, :1012-1030 Place: in/leg/out arrays, doubled_row lambda (bridge on leg_edge is doubled).
- R3 :1357-1362 fan_item/edge_port: Fan = first token of an in edge, last of an out edge (token position).
- R4 :1380-1385 row_pts: per-row point block (fif/fil/fof/fol, lane_split, sb1_last, sb0_first, lane_merge, heads) allocated so point ids stay; R5 left this alive.
- R5 :1403-1500 chain build splits three ways: in/out (Fan + SBends only, anything else fails :1424), undoubled leg (two Tiles), doubled leg (Tile, HeadsPair.., SBend, Tile + copy chain).
- R6 :1495-1527 row_changes by role (Fan on in/out, lane merge/split on doubled leg).
- R7 :1775-1808 row_pieces(r): walk(in_edge), leg (single or the hand-built 2-copy run/head/bend list), walk(out_edge).
- R8 :1959-1963 prio footprint reads out_edge[r].from as the joiner merge and leg_edge[r] to find main paths.
- R9 :2092-2105 doubled-leg bridge lines by row.
- R10 :2181-2187 Y relations: node_pt[in.to] on exit_item, node_pt[out.from] on lane_item. Both are already the Fan/Corner token indexes (K4); the relation is re-derived from the role.
- R11 :2227-2263 doubled-leg block: row items pinned (Y), lane split/head/lane merge runs, the stagger (D118) and the prio floor on njm.
- R12 :2354-2366 floor_kind: Node/PrioNode by "row edge, not in/out" (undoubled leg) and by Tile position.
(Row heading east[r] at :1610, :1843, :2723, :3017 reads a per-row bool, not a chain relation; benign: the heading is a property of the row's path. Not counted.)
Token position / role tag:
- T1 :1114 joiner_div = Corner at token 0 from a band split. T2 :1327 Fan only at t==0 (in) or last (out). T3 :1442-1443, :3393-3394, :3017: with_role search for LaneSplit/LaneMerge by Role tag (a tag, general enough; R7 Shift adds Role::Shift). T4 :1763 Fan item read from el.index. T5 :1334/:2314-2322 joiner Corner size from rows (already general).
Prio (P): P1 :1038 even_prio; P2 :930-931 floors (Node->PrioNode = even_prio+1, LaneMerge->PrioNode = even_prio+2); P3 :2263, :2366 apply them on njm; P4 :1957-1981 occupancy footprint; P5 :3467-3567 signal pass draws it; P6 :3138-3142 prio_beside ties for levels.

## 2. Approach items
- Graph: an Approach item is not a graph edge or token. It is the stack row the joiner (branch) edge's Corner lands on: edge from a side-band split to a row merge node n has program [Corner(index=approach_item) (joiner_div, :841/:849), Under/Over.., SBend(index=lane_item) (:886)]. The Corner turns the joiner from vertical to the row heading ON the approach row (a Corner's Y is eq to its item, :2330); the SBend then steps from the approach row to the lane row and ends at the merge node tile (:1099 el.last = stop).
- Why one row: the arrival SBend is the K5 token on a NON-row edge. In refresh :1338 only `is_row_edge(e)` gets m = |vy[item] - vy[first]|; a joiner SBend keeps Elem::m = 1 (default :1080) and the Y relation is hard-coded `eq(1, el.last, el.first, VY[side])` (:2342, horizontal case). With the Corner's last on the approach item (:2330) and the SBend's last on the lane item (:2345), an item between them gives |dy|=2 vs 1: contradiction (R7b). X side `eq(0, last, first, 0)` (:2343) is already size-free.
- General relation: size the joiner's arrival SBend like the row-edge SBend and the Fan: m = |vy[item_pt[index]] - vy[first]|, offs = bend_tiles(hin, hand, 2m) (the :1338-1340 branch with the `is_row_edge` guard dropped for horizontal hin), and the Y relation becomes the pair of item pins only (:2330, :2345) with no fixed 1 (:2342 deleted for horizontal). Fan sizing (:1325-1331, :2303-2308 `m - 1` across) and the lane split/merge (:2232-2241, sized by item rows via the same refresh) are the cited precedents. Downstream readers take size from el.offs (:1546, :1638, :1822, :1931, :2693), so INFERRED no further change. The signal pass finds the S-bend by token, not by size (INFERRED, JunctionRouteEvents not read).
- Seeds: unchanged when approach adjoins lane (m = 1 = today).

## 3. The prio element
- Graph element: none. DESIGN-G1 §16.1 itself says "the prio element is a node floor, not an Elem": a floor on the joiner merge node (P2/P3) plus a Y-adjacency via the stack. Its tiles are decided in two places that do not talk to each other: (a) placement occupancy :1957-1981: for each row whose merge tile carries the main axis and a side half piece (node_bits nw/se), row y = vy[lane_item] +- 1 chosen by which side the JOINER's half piece lies (NOT by the Prio item), X span njm..njm-+even_prio, two Occupants keyed to the node; (b) the signal pass :3517-3567: geometry re-derived from the joiner route's last tile offset (`offset = from.y - M.y`) and the tracks already drawn, draws rows ml(s)/row(s), refuses with "does not fit" if any row tile is occupied (:3529) or the far end touches (:3531) (rule 10: Prio item left empty, no signals).
- Prio item row vs element row: coincide only because the stack puts Prio adjacent to Lane (:1975 +-1). Nothing ties them, and facts do not see the tiles: the terrain facts/clearance code (2380-3420) has no prio branch; only the levels solve sees them afterwards through prio_beside (:3138-3142; junction_levels.cpp :653-660, :376-390 tie "4" equal level to the main-line tile beside). INFERRED: placement facts (blocked tile, height step) can pass while the element lands on a bad tile; the levels solve then fails or terraforms.
- What it takes to be an Elem on the Prio item's row: a Prio Elem (K::Prio) on the leg/out chain with index = Prio item: Y: eq(1, first/last, item_pt[index]); X: last = njm, first = njm -+ even_prio by `along` (replaces the Node->PrioNode and LaneMerge->PrioNode floors with a Prio->PrioNode run entry in JunctionRunFloor); occupancy emits its Occupants from the Elem instead of node_bits (:1957-1981 deleted); facts get it as a Straight group on its row like any run (:2723-2740 row_pieces Tiles/Runs); the signal pass reads the Elem's tiles (via placement) instead of deriving them, and the fit test becomes a placement fact. A shared PrioAndCopy item then needs the Prio Elem and the copy's heads kept apart on one row: D108 gives the separation in the doubled block today; as an Elem it is two relations on one row (INFERRED).

## 4. Replacement and order (each by identity: SamePlacement over the 44 [topologysweep] cells + circuit file byte-identical, §16.2)
1. K2/K3 one shared helper (item lookups); no behaviour change. Identical.
2. K5/K6/K4 tokens carry their items (Over tokens of a doubled leg get index = copy row item; Fan/LaneSplit already do); K7/R10/R11 read token indexes not north_item/lane_item. Identical.
3. Approach span: refresh :1338 and :2342 as in section 2. Identical while adjacent; unlocks R7b. First step with new capability.
4. R2/R1/R3 roles from graph/tokens (Fan token present = in/out; leg = chain with copies): replace in/leg/out arrays and doubled_row by `!programs.copies[e].empty()`. Identical.
5. R5/R6/R7/R9/R11/R12 one chain-driven path for all row edges (tokens in order: Fan, SBend, Heads, Over, role tags), then R4 row_pts removed. Point ids change (new_pt order); identical only if the D215 objective and every tie-break key on (edge, token, copy) (§16.2) and not on ids: INFERRED risk; if a tie depends on ids this step changes seeds and needs a sweep diff.
6. K1 stack from per-edge needs (joiner edge -> Approach, crossing -> Copy, merge -> Prio) instead of per-row if-chain; multi-joiner rows still refused today, so identical for all current seeds.
7. Prio Elem (section 3), P1-P6 replaced. Flat seeds identical (row = lane +-1, same X span; INFERRED). Seeds change only where placement facts newly reject/shift a prio tile (terrain cells, [realterrainsweep]); that is the intended gain. Signal-pass change must keep `fits` identical on flat cells.
Seed-changing: step 5 (risk), step 7 (terrain only). Steps 1-4, 6 identical. Steps 1-3 need no other step; 7 needs 2 and 4.
