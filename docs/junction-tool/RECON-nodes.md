# RECON nodes: splits, merges and crossings of the line-router T (2026-11-11, read-only, for D182)

All line numbers are src/junction_generator.cpp unless a file is named. Everything is read from code or decisions unless marked INFERRED.
Naming. A = row 0, travels +x (X_SW) from the W port x_w to the E port x_e. B = the doubled main line on the W row (w_row = g+1) and b row (b_row = g+2), travels -x (X_NE). xI = hub_x (N column), xO = xI+branch_gap+1 (S column). Box rows 0..yb, yb = height-1, ys = yb-STUB (STUB = 2, line 898). NE is -x in OpenTTD tile coordinates, so "upstream" on B is larger x.
Entry point: `ConstructJunctionTMap(..., line_router=true)` line 864. The flat T (old router, line_router=false) runs first and only supplies the ports and the first height (902-904, D173, D179).

## 1. Node table

Key to the "today" column: FLAT = fixed arithmetic from frame constants; SKEL = solved by SolveJunctionSkeletonW from the crossing set of two routed rasters; PLAIN = inferred from the plain pieces (`IsPlainX`, tile holds exactly X, line 162-170) of an already routed raster; WIN = D172 window.

| node | on lines | how its position is decided TODAY | footprint by the rules | window it may move in today | edges starting/ending at it |
|---|---|---|---|---|---|
| N1. A->S split | A (+x), A->S | PLAIN. `request.starts = PlainRasterPieces(pass1, *a_raster, X_SW, 2, xI-2)` (1351); the router picks the cheapest start. The split tile itself is not a variable anywhere; `x_s = a_to_s->pieces.front().tile.x` is read back after routing (1405, 1406). | Switch tile; one signal-legal plain piece before it (G6 "no signal before a split", signal on its own tile, D117a switch isolation, junction_rules.cpp:777-780). Not adjacent to another split on the ML (G4, rules.cpp:440). Outside N->A's prio coverage: x_join - x_s >= prio_tiles+1 (D108 G9c, D148). Curve rule 3 over approach (CS1) plus route plus continuation (CS3, 1051-1058). If a bridge follows: a signal-legal piece between split and head (D113). No prio on a split. | x in [2, xI-2] on A's plain pieces, and not before A's own start x_w. Width about xI-3 tiles. Also A->S must cross xI only at rows g+4..ys-3 (column_fence, 1343-1347). | A->S starts here (and A continues). |
| N2. N->A merge | A, N->A | PLAIN. `request.goals = PlainRasterPieces(pass1, *a_raster, X_SW, n_to_a_min_x, min(width-3, x_e))` (1407); `n_to_a_min_x = max(xO, x_s + prio_tiles + JUNCTION_E_PRIO_AFTER_SPLIT(2, line 245) + e_phase)` (1406). The goal is the plain tile one past the merge tile (D148). | Merge switch tile; goal piece plain after it (D115a/D148); signal after the merge on A (G6 "no signal after a merge"); joiner's nearest signal before the merge is Entry (G9 6, rules.cpp:1115-1128). Prio (D85, D159, always for an ML merge): coverage = 2*prio_tiles units back along A from the merge, no split of A inside it (G9c, rules.cpp:1298-1316) plus one segment (D108); one join in each block over TL, first join on the merge tile (D75, G9 3/4). Prio row: the side opposite the joiner's approach (signal_pass.cpp:277-302, D75/D87). N->A arrives from larger y, so the row is y = -1, ABOVE A (INFERRED from `offset`/`side` at :295-300). Prio is built after routing (PlaceJunctionSignals, finish 1692), never routed. | x >= x_s + prio_tiles + 2 + e_phase (e_phase = 1 when prio_tiles odd and >= 5, D148a, line 822) up to min(width-3, x_e). Also bounded by the router and by the X_B window (row 10 below). | N->A ends here; N->A also starts at the N port edge (N4). |
| N3. B->S split | B (W row), B->S | SKEL. `sl_x = pos.x_sl + 1`; `request.starts = PlainRowPieces(fixed, w_row, X_NE, sl_x, sl_x)` (1641-1643); fails with "B->S has no start" if sl_x < xO+2 or the piece is not plain. x_sl = x_ls + 2 exactly (SL pushed Down). | Switch tile x_sl, then one plain signal tile, then the lane split: x_sl - x_ls >= 2 (k_split, D110 adjacent splits, G4 rules.cpp:440; D145 "x_sl - x_ls = 2"). | One tile: none. Start piece fixed by the skeleton; x_sl follows x_ls which follows the crossing set's max (x_he >= zmax+1). Must satisfy x_sl+1 >= xO+2 (checked after, 1644). | B->S starts at x_sl+1. |
| N4. S port merge (A->S and B->S join the S stub) | S port edge x=xO | FLAT. A->S goal is the single piece (xO, ys) Y_SE (`goal_s`, 1288, used 1355). B->S goals = `stub_pieces(fixed, s_port_raster, xO, Y_SE, ys)` (1648): stub pieces of rows ys..yb-1 whose tile is exactly Y. Whether (xO,ys) still counts as plain after A->S arrives is INFERRED to be no, leaving row ys+1 only. | D115a: below a branch's edge-most split/merge exactly two tiles (signal tile, boundary tile), STUB=2; boundary row carries no switch/signal (G10, rules.cpp:1624). Signal after the merge (G6). G7 waiting room on each connection: split to last signal before the merge holds a train (rules.cpp:836). | A->S: none. B->S: rows ys..yb-1 (up to 2 tiles). | A->S ends; B->S ends. |
| N5. N port split (N->A and N->B leave the N stub) | N port edge x=xI | FLAT. N->A start `start_n` = (xI, ys) Y_NW (1289); N->B starts = `stub_pieces(fixed, n_port_raster, xI, Y_NW, ys+1)` (1656), i.e. only (xI, ys+1) at STUB=2. | As N4 mirrored: a split needs a signal-legal piece before it and the boundary tile after (D115a). | None. | N->A and N->B start. |
| N6. Lane split on B | B lanes | SKEL, then template. x_ls from skeleton (SV_LS Down). The lanes are rasterised from `pos` by BuildParallelogramLanes (258, 630, 1485); lane B's start of edge = last piece shared with lane A (`shared`, 1534-1541). | Shared split tile, plain pieces, head: x_ls - x_heA >= 2, x_ls - x_heB >= 1 (k_entry = {2,1}, D145; D118 stagger; D113 one signal-legal piece per leg). D85: second lane on the outer side. D108: no lane split inside a prio coverage. | None once pos is solved. | Lane B edge starts at the last shared piece; lane A continues. |
| N7. Bridge heads, near (x_he) and far (x_hw), per lane | B lanes, spans | SKEL: x_he >= zmax+k_head (Down), x_hw <= zmin-k_head (Up), 2 <= x_he - x_hw <= nmax+1; zmin/zmax from the crossing pieces of A->S and N->A on w_row (lane A) and b_row (lane B). | D80 doubled, equal lengths (D118 stagger 0 or 1), k_head = 1 clear tile beside the crossings, G5 head rules (approach track outside, no signal on head, rules.cpp:470-505), span ok (`bridge_span_ok`). One signal before the near head and after the far head (G6 "Every bridge is its own block", rules.cpp:790-810). | Spans are fixed prefixes `B<n>` in the span edges (1571-1575); heads are exactly the skeleton's. | Span edges between head nodes (route_edge kind 2, 1593-1617). |
| N8. Lane merge on B | B lanes | SKEL x_lm (Up), x_hw - x_lm >= 2 (lane A) / >= 3 (lane B) (k_exit, D145). Template lane A ends at (x_lm, b_row). But the routed edge differs: lane A's last surface edge is routed to goals `PlainRasterPieces(fixed, routed_b raster, X_NE, landing.x, landing.x+1)` on b_row (1604-1612), landing = template's last lane-A piece. | Plain merge, no prio (D80/D85); upstream of the joiner merge by cover+k_w (N9). D108: no lane merge within a prio coverage. G8 no block with split and merge. | 2 tiles, x_lm..x_lm-? (INFERRED: landing.x and landing.x+1 on lane B's routed b-row pieces). | Lane A's last surface edge ends; lane B's through edge passes. |
| N9. N->B merge (joiner onto B's b row) | B (b row), N->B | SKEL window. `request.goals = PlainRowPieces(fixed, b_row, X_NE, pos.x_sb+1, pos.x_nw)` (1657). x_nw Up = min(xI-2, x_lm - cover - k_w) (NorthWestCap, PrioCoverage). k_w = JUNCTION_PRIO_AFTER_MERGE = 1 (236, 1462). | x_lm - x_nw >= prio_tiles + 1 (D85, G9c; pinned by test "Parallelogram solver constants are minimal", D145). Prio: single track after the lane merge, row on the side opposite the joiner = w_row (INFERRED as for N2). Coverage reaches back to the lane merge's neighbourhood but not over it (D108). Signals as N2. x_nw <= xI-2. | x in [x_sb+1, x_nw]: k_sb = 4+d tiles wide, d = 0..tl (NW1 retries, 1677-1684). | N->B ends. |
| N10. Shift-back on B (b row to W row, S-bend) | B | SKEL x_sb = x_nw - k_sb, k_sb = JUNCTION_SHIFT_BACK_TILES (4, line 242) + d; x_sb >= x_sb_min = 2. Template lane B's `LFR` is built from pos (258). The lane-B surface edge across it is routed between template nodes. | At least tl straight tiles between merge and S-bend so CS2's S-bend window (2*tl units, a straight tile = 2 units) clears (comment 1673-1676). Rule 3. | d = 0..shift_back_limit = tl (1437). | Lane B edge from the shift-back to the W port. |
| N11. X_B crossing (A->S and N->A cross B's w_row and b_row) | A->S, N->A, B lanes | WIN + SKEL. Windows from routed rasters: A->S is the first crosser, unbounded, checked after: `hi - lo > window` gives "A->S crossing wider than the doubled bridge" (1388-1393). N->A gets rows w_row and b_row blocked outside [hi - window, lo + window] (1394-1398), window = nmax-1-2*K_HEAD (K_HEAD = 1, D172). The skeleton then puts heads around the actual pieces (zone, 1424-1432). | Doubled B bridge, middle length nmax at most (largest n in 1..10 with bridge_span_ok), crossing pieces strictly between heads with k_head = 1 clear on each side (D172, D145). B goes over (D156). | Width D = nmax-3 tiles for all crossing pieces together. | A->S, N->A pass; B's span edges span it. |
| N12. X3 crossing (N->A over A->S) | A->S, N->A | Emergent. No node. A->S routed first (1358); column_fence blocks A->S at xI rows 0..g+3, ys-1, ys-2 (1343-1347), then N->A is routed against A->S as existing track and the router bridges it if needed. | Crossing envelope per D171/D172 "X3" (DESIGN-O4); bridge heads: approach track, span ok, no signal on head (G5). | None today (only the fence on A->S). | A->S and N->A. |
| N13. Port approaches (A's two ends, S and N stubs) | ports | FLAT. x_w, x_e from the flat T (902-904); widened by D179a (1700-1718). | D179: port tile plus STUB straight pieces usable (`ports_blocked` checks x_w, x_w+1, x_w+2, x_e-2, x_e-1, x_e, line ~1137-1146). D115a STUB=2. G10 footprint (no switch/signal/head on the boundary). | Ports only widen, within the frame. | A starts/ends; port edges. |

Not nodes today but listed in case: the S-bend pieces of lane edges (JunctionLaneNodes, 344-365) are nodes of each lane's chain only in the sense of edge boundaries (axis piece at every straight/diagonal change, piece before and after bridge heads). The signal-pass-only items (prio joins, prio signals) have no node in the T at all.

## 2. Routing order today (line-router path, per height)

0. Height loop from `first` (flat T's height) to `height_limit` (1150-1156); `routed_rasters.clear()` (1160). `last_row` = the boundary row blocked except xI, xO (1161-1163). Port check `ports_blocked()` first (1139-1148).
1. S port edge: start (xO, ys) Y_SE, goal (xO, yb) Y_SE (1296-1307).
2. N port edge: start (xI, yb) Y_NW, goal (xI, ys) Y_NW (1308-1318).
3. A: start (x_w, 0) X_SW, goal (x_e, 0) X_SW, band rows 0..g (1325-1337).
4. A->S: starts = plain pieces of A, x in [2, xI-2]; goal (xO, ys) Y_SE; blocked = column_fence; w_row bridge heads barred (cost 100x100 rail piece); `cross_b_once` (1351-1360).
5. D172 window computed from A->S pieces on w_row/b_row (1374-1403); fail "A->S crossing wider..." goes to the next height.
6. N->A: start (xI, ys) Y_NW; goals = plain pieces of A in [n_to_a_min_x, min(width-3, x_e)]; blocked = n_to_a_blocked; w_row heads barred (1405-1412).
7. Crossing set `zone`, `zone_b` from the pieces of A->S and N->A on w_row and b_row (1424-1436). Empty zone is "no crossing of the W row".
8. attempt(d), d = 0..tl: skeleton for stagger 0 and 1, smaller span wins (1441-1480); lanes template (1485); layout rebuilt from A, S/N port rasters, A->S, N->A (1500-1504).
9. Lane A surface edges except the last (1596-1600), each start and goal = template pieces at the lane's nodes, band rows w_row..b_row between the nodes' x, blocked = rows_between (1560-1582).
10. Lane B surface edges (1601-1604).
11. Lane A's last surface edge to merge goals on lane B's routed b-row pieces at x_lm.. +1 (1604-1612).
12. Span edges of both lanes with the template's fixed prefix `B<n>` (1613-1617).
13. B->S: start plain w_row piece x_sl+1; goals S stub plain pieces; blocked = branch_fence (rows_between plus the bridged-under tiles) (1641-1654).
14. N->B: starts N stub plain pieces from ys+1; goals plain b_row pieces in [x_sb+1, x_nw]; blocked = branch_fence (1655-1671). On failure with d < tl: shift_back_retry, back to 8 with d+1 (1664-1665, 1677-1684).
15. FinishJunctionCandidate with terrain = nullptr (1692): crop, normalise, PlaceJunctionSignals (prio rows, signals), normalise (380-460).
16. Ports check: if the crop reaches past x_w..x_e, widen the ports and redo the same height (1700-1718).
17. Rule check on the built layout (1722-1726).

## 3. Where a later edge depends on the route (not just the ends) of an earlier one

- CS1 approach chaining: an edge starting on routed track inherits the curve state from the pieces before its start piece on the newest raster that holds it, and chains backwards to further rasters up to max(curve_spacing, s_bend_window) pieces (`chained_approach`, 980-1006; lookups precomputed per request 1043-1046).
- CS2 continuation chaining: a route ending on routed track must keep the curve rule over the pieces that already continue from its goal, chaining across rasters (`chained_continuation`, 1009-1034). DIAG-ph1-limit: this refused goal (17,3) because the next piece after it was the lane's W-end curve.
- CS3: the whole joined route is judged by rule 3 (`boundary_ok`, 1051-1058).
- A->S's starts and N->A's goals are both read from A's routed raster (`PlainRasterPieces`, 162-170; used 1351, 1407, 1608). DIAG-house: a house beside the hub forced A onto row 1 for x=38..53 and N->A was left with 2 plain goals instead of 3.
- N->A's goal window starts from the routed A->S's first piece x_s (1405-1406).
- D172 window: N->A's blocked rows come from the pieces A->S actually used on w_row/b_row (1374-1398). Failing the width check abandons the height (1388-1393).
- The skeleton's crossing set zone/zone_b is the union of the routed pieces of A->S and N->A on B's rows (1424-1432); it fixes x_he, x_hw, and through them x_ls, x_sl, x_lm, x_nw, x_sb.
- Lane merge goals are plain pieces of lane B's routed surface raster (1604-1612); lane A's last edge waits for lane B's (1601-1604).
- Span edges are routed last and take the template's bridge length as a fixed prefix (1571-1575); their heads are checked against the terrain (E3b, 951).
- B->S goals/starts: S stub pieces of the routed port raster that are still plain (`stub_pieces`, 1296-1306 comment, 1648); starts need a plain B piece at x_sl+1 on the routed lane A (1643).
- N->B goals: plain b-row pieces of the routed lane B edges in [x_sb+1, x_nw] (1657). DIAG-ph1-limit: the window straddled an incline column, goals 17..20 each refused by a different router rule.
- NW1 shift-back retry: N->B's failure re-solves the skeleton with k_sb + d and reruns everything after the skeleton; `routed_rasters` is cut back to its length before the skeleton so CS1/CS2 do not see the discarded attempt (1677-1684).
- Ports widening: after finish, the cropped layout's extent decides new ports, A is rerouted from the wider ports (1700-1718, D179a).
- Prio pass: prio side and row are decided from the routed joiner's piece before the merge (signal_pass.cpp:277-302), coverage walk from the routed merge (:343, :476, :515, :554), after all routing; the terrain is not consulted for the prio rows in the line-router path (finish gets terrain = nullptr, 1692; the usability test of "prio rows exist only now" at 454-460 runs only when terrain is non-null).
- Growth stop: `growth_cannot_help` tests relaxed reachability of S port, N port, A, A->S and N->A only; B's edges get only the per-height skip (comment 1190-1210, relaxed test 1210-1240).

## 4. What SolveJunctionSkeletonW already solves (junction_skeleton.cpp:132-186)

Variables (SkeletonVar, :124-137): HE_A, HE_B (near heads), HW_A, HW_B (far heads), LS (lane split), SL (B->S split tile), LM (lane merge), NW (N->B join), SB (shift-back). All on B's W/b rows, x only; the origin is index 0 fixed at 0.
Constraints (x[a]-x[b] >= k):
- AdjacentSplits: SL - LS >= k_split (2).
- BridgeEntry: LS - HE_l >= k_entry[l] ({2,1}).
- Span: HE_l - HW_l >= 2 and HE_l - HW_l <= nmax+1.
- HeadClear: HW_l <= zmin[l] - k_head; HE_l >= zmax[l] + k_head (the crossing set of that lane's row).
- BridgeExit: HW_l - LM >= k_exit[l] ({2,3}).
- EqualLength: HE_B - HE_A = stagger and HW_B - HW_A = stagger.
- PrioCoverage: LM - NW >= cover + k_w.
- NorthWestCap: NW <= x_cap (xI-2).
- ShiftBack: NW - SB >= k_sb; SB >= x_sb_min (2).
- extra: caller's learned constraints (unused by the generator).
Push order: HE_A, HE_B Down; HW_A, HW_B Up; LS Down; SL Down; LM Up; NW Up; SB Up (each fixed one at a time at its extreme, D144).
Inputs the caller fills: zmin/zmax per lane (from routed A->S and N->A), nmax, cover = prio_tiles, x_cap, stagger, k_w, k_sb.

So it is already a node placement for all of B's nodes N3, N6, N7, N8, N9, N10, given the crossing set.

What it would need to also place A's split (N1) and merge (N2). All INFERRED:
- Two new variables, AS (A->S split) and NA (N->A merge), on A's row (x increases with travel on A).
- Constraints it could take straight from existing rules: NA - AS >= prio_tiles + 2 + e_phase (D148/D148a, today a plain constant at 1406); AS <= xI-2 and AS >= 2 (today the PlainRasterPieces window, 1351); NA >= xO.
- The crossing set is not an input there but a function of the two nodes and the routes between them and B's rows. Today zmin/zmax are read back from routed A->S and N->A. For the skeleton to place A's nodes first it would need the crossing columns as variables tied to AS and NA (A->S's descent from A to B's rows is a diagonal run whose length depends on the curve rules; N->A's likewise), or the crossing columns would become nodes themselves (X_B, N11) with their own window D = nmax-3 and a footprint of the two edges' approach geometry.
- Direction of coupling: today B depends on A's nodes (via zone). The skeleton as written has the crossing set as constants, so placing AS and NA jointly with B means making zmin/zmax variables with constraints from the branch edges' geometry (per-piece diagonals, curve spacing, column fence at xI). No such constraint exists in junction_skeleton.cpp.
- The X3 crossing (N->A over A->S) and the N4/N5 port nodes (S merge, N split) have no variables in the skeleton at all.
- Constants differ between sides: W side k_w = 1 (merge tile then coverage) against E side 2 (the goal piece is the plain tile after the merge, D148). The phase e_phase (D148a) exists only on the E side: it depends on the prio pass refusing adjacent joins (signal_pass.cpp:414), "deriving it in the generator would need the signal pass's ML blocks before routing" (D148a).

## Surprising facts (also in the handback)
- A's split and merge have no variable anywhere: x_s is read back from the routed A->S (1405) and the N->A window starts from it.
- B's nodes are already solved as nodes (skeleton), but from a crossing set measured on routed A->S and N->A, so B's nodes depend on A's routes.
- The S merge and N split are nodes of the port edges with 1-2 candidate pieces each, set by STUB=2 (D115a); nothing in the generator names them.
- The prio rows (N2 on row -1 above A; N9 on the W row beside b row, INFERRED) are not routed and, in the line-router path, never terrain-checked: finish is called with terrain nullptr (1692).
- X_B's window D = nmax-3 is applied only to N->A; A->S is checked after routing and fails the whole height if wider (1388-1393).
