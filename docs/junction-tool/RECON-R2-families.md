# R2 recon: relation families of build() (junction_placement.cpp, commit 29c9be26bf)

Line numbers are in src/junction/placement/junction_placement.cpp. build() = 1438-2210. Rows are emitted by
ge/eq(axis, ...) which DROP a row whose axis != phase (line 1385 region, ge at ~333), so "axis" below is the
axis the row lands on; the code runs in both phases and filters. Phase flags: x_phase = (phase==0).
Helpers `along`/`run` (588-601) read only the static tables VX/VY. `refresh()` (603-640) writes el.m / el.offs /
el.hand of every bend from vy, port_row, fan_m, so every row that reads el.m or el.offs reads a solution
INDIRECTLY (rows from vy, port offset from best_offset, fan_m from set_port_rows).

Tag column: SystemTag{bend,a,b,nonlinear,zero_branch,order_pin}; `-` = default tag (no tag, c is constant).
Class column: S static, L lazy, "S?" = §G3.6 lists it static but it reads solution state, "U" = unclear.

| # | why text | lines | axis / phase condition | reads (solution state) | tag | class |
|---|----------|-------|------------------------|------------------------|-----|-------|
| 1 | stack origin | 1451-1452 | Y, always | none | - | S |
| 2 | stack rows | 1453-1456 | Y, always | none (structure.stack) | - | S |
| 3 | band gap | 1457-1465 | Y, always | none | - | S |
| 4 | S edge | 1467-1472 | Y, always | none (JunctionRunFloor) | - | S |
| 5 | S port row | 1474-1477 | Y, always | none | - | S |
| 6 | port columns | 1480-1485 | X, always | none | - | S |
| 7 | S port spacing | 1487-1491 | X, always | none | - | S |
| 8 | row nodes | 1493-1500 | Y, always | none | - | S |
| 9 | waiting room | 1503-1524 | Y, !x_phase | none | - | S |
| 10 | prio element (RW6, own tile pins) | 1536-1542 | Y eq x2, X eq + along; both phases, only if edge ends in Prio | none (even_prio is an input) | - | S |
| 11 | row items (doubled leg) | 1553-1569 | Y, both phases, doubled row edges | none | - | S |
| 12 | lane split run (floors, stagger, head spacing) | 1572-1580 | X, `if (!x_phase) continue` at 1570, doubled legs | none | - | S |
| 13 | series bridge heads (+ lane merge run) | 1582-1586 | X, x_phase, doubled legs | none | - | S |
| 14 | prio element (floor after lane merge) | 1587-1590 | X, x_phase, doubled leg with Prio | none | - | S |
| 15 | bridge heads hug crossers (static ineqs) | 1594-1625 (rows 1608-1614) | X, x_phase, doubled legs; the whole crosser loop is skipped `if (!rows_known) break` (1599); fails build with "Z row" if no crosser tile on the head row (1617) | vy[heads0/heads1(b).first] (1603) via the row match z.y != y; edge_tiles(x) (788-814) reads vy, port_row, el.offs of the CROSSER's bends; z.dx = bend tile offset (from el.m via el.offs) | {nullptr,0,0,z.bend,false}: nonlinear iff the crosser tile is a bend tile | S? (straight crosser) / L (bend crosser) -- see (a) |
| 16 | fan size, non-empty | 1630-1637, 1640 | X (along, el.m-1) + Y (eq item row, 1640); fans of non-doubled edges, both phases | el.m (= fan_m[e], from set_port_rows: port_row, vy item row, port offset); el.hand/offs not read here | {&el,1,-1,false,false} | S? (m is the input; today it is derived) |
| 17 | fan size, empty (m==0) | 1630-1634 | X eq x2 (fan ends on its port column) | el.m == 0 branch (zero-size) | {&el,0,0,false,zero_branch=true} | S (branch on input m=0) |
| 18 | joiner_div Corner floor | 1646-1647 | Y ge(first,last,k), both phases | none (k) | - | S |
| 19 | joiner_div Corner choice | 1648-1654 | Y eq(first,last,fixed->corner[e][tok]), `fixed != nullptr` | fixed->corner | {&el,1,0,...} | S (input: corner size) |
| 20 | joiner_div Corner X size | 1655-1660 | X, x_phase; kp = fixed ? fixed->corner : (rows_known ? abs(vy[first]-vy[last]) : k) | vy[el.first], vy[el.last] (1656) when fixed == nullptr && rows_known | {&el,VX[hin]+VX[hout],0,..} | S? -- reads vy (see a) |
| 21 | Corner size (non joiner_div) | 1662-1666 | X + Y, both phases | none (k) | - | S |
| 22 | S-bend size: fan at S port (el.fan, hin 0/2) | 1669-1678 | Y eq (m==0: 0, else VY*(m-1)); X: fixed ? eq : ge (last-first, m) | el.m (from refresh: rows or fan_m); fixed != nullptr picks eq vs ge on X | m==0: {&el,0,0,..,zero_branch}; else {&el,VY[hin],-VY[hin]} for Y, {&el,1,0} for X | S? (m input) |
| 23 | S-bend size: vertical-entry S-bend (hin 0/2, not fan) | 1679-1686 | same as 22 with no zero branch | el.m (= abs(vy[item]-vy[first]) via refresh, 624-625) | {&el,VY[hin],-VY[hin]} Y; {&el,1,0} X | S? |
| 24 | S-bend size: horizontal S-bend | 1687-1691 | Y eq(VY[side]) only if el.index==NO_ROW; X eq 0; both phases | none | - | S |
| 25 | (item pin of Corner/S-bend/Fan, same why as its element) | 1667, 1692, 1640 | Y eq(last, item_pt) | none | - | S |
| 26 | bridge heads | 1696-1698 | X or Y by VX[hin] (eq last==first on the cross axis) | none | - | S |
| 27 | fan size (empty S fan joins port tile) | 1705-1711 | X + Y eq, both phases | el.m == 0 via next.m (1704 condition) | {fan el,0,0,..,zero_branch} | S (branch on input m=0) |
| 28 | prio element (floor on first tile) | 1729-1731 | X/Y along (heading), floor, when before_prio | none | - | S |
| 29 | chain run | 1732-1752 | X/Y along or run; exact flag from structure | none | - | S |
| 30 | bridge over S-bend | 1756-1786 | X eq(first, crossed->first, VX[side]); Y two eq (rows hi+1/lo-1); both phases | crossed->m only via bend_tiles(2*max(m,2)) for lo/hi (1762) and for the m = 2..64 constancy scan (1773, only when system != nullptr) | constant_rows ? {crossed,0,north?hi+1:lo-1} (b of 2nd row set 1783) : {crossed,0,0,nonlinear} | S (affine in t when constant_rows; L=1 uses max(m,2)) |
| 31 | y floor | 1789-1792 | Y, !x_phase && !floors_off | y_floors argument | - | S (floors_off drops it) |
| 32 | bridge middle clearance Y (clear_y entries) | 1793-1800 | Y, !x_phase && !skip_clearance | clear_y, built by plan_clearance from flat_x/flat_y (2239-2283) | pad_tags(false) = default tag | L (clearance Y) |
| 33 | curve gap | 1806-1875 (emit 1868-1873) | Y (vertical pairs) if !x_phase, X (other pairs) if x_phase; condition (!x_phase \|\| rows_known) && spec.curve_limit; returns false on a same-anchor gap or sym_routes failure | sym_routes (841-1000) reads vy (857, 878-883, 896), port_row (857, 878), el.offs.size() (896), el.m, rows_known; routes then feed JunctionCurveGaps; per-(v,u) max of ceil(need/2) | {nullptr,0,0,nonlinear=true,false} | S (as counters, F11); today reads vy/port_row/offs -- see (a) |
| 34 | occupancy | add_occupancy 1157-1363; called 1882 and (legality only) 2391 | X, x_phase && rows_known && !relations_only && flat_solve ok | flat (order) AND vy (row of every occupant, 1228, 1236, 1253, 1274, row_pieces), node_bits, el.offs; legality=true path uses vx and adds nothing | pad_tags(true) = {nullptr,0,0,nonlinear,false} | L (occupancy) |
| 35 | bridge heads hug crossers (order pin) | hug_heads 1365-1383 | X, same block as 34, `hug_heads(flat)` is NOT gated by skip_clearance; hugs non-empty only if row 15 ran | flat[tile.first] + dx over hug.tiles (hug filled at 1620 from vy / el.offs) | {nullptr,0,0,true,false,order_pin=true}; eq => 2 rows each, 2 eq per hug | L (bend-crosser hug; the order pin) |
| 36 | bridge middle clearance X | add_clearance_x 1416-1436 | X, same block, `!skip_clearance`; only Y-axis bridges (line.x_axis false) | vy[line.a], vy[line.b], vy[head.ypt] (row strictness) and flat[head.xpt]-flat[line.line] (side; side 0 skipped = no row, F9) | default | L (clearance X) |
| 37 | x floor | 1887-1890 | X, x_phase && !floors_off | x_floors argument | - | S |
| 38 | facts (blocked tile / height join; not a cons row) | 1892-2196 | X, x_phase && facts != nullptr && !relations_only; pushes fact_cons (domains/run bans), not cons | vy (2011, 2107-2165, el.offs, group tiles); least/most from cons (not the solution) | none | U: not in §G3.6's lists; it is an evaluation row (the hover's "facts"). Needs vy (rows) and a bound pass over cons |
| 39 | box bound | 2198-2208 | X: xe >= p for all p; Y: se >= p (not x_phase split) | none | - | S (§G3.6 box row is a different thing: offset+notation>=0, see below) |

Other rows named by §G3.6 that do not appear as a `why` in build():
- box row (port offset + notation >= 0): checked in the fixed path at 2297-2303 (fails with reason "box"), not a relation.
- span caps (|far-near|-1 <= max_bridge_middle / max_tunnel_length): not in build(); not present as rows today.
- fan sign sigma: refresh() 614 sets el.hand from sign(y_item - y_port) (a solution read; F4 turns it into an input + row).

## (a) Solution reads inside families §G3.6 calls static (R2 must remove or make inputs)

1. Row 15 "bridge heads hug crossers" static ineqs: `vy[near_far.first]` (1603); `edge_tiles(x)` (788-814) reads vy[...] (793-809) and port_row[el.slot] (793, 799) of the crosser; `z.dx` and the bend tiles come from `el.offs` (805, 811), i.e. el.m which comes from vy/port_row/fan_m in refresh(). `if (!rows_known) break` (1599) makes the family disappear in the first pass. `return false` "Z row" (1617) depends on vy. Straight crossers: dx = 0 but the tile row set (z.y == y) still needs vy. S-class only if rows are an input (y per crosser tile).
2. Row 20 joiner_div Corner X size: `vy[el.first]`, `vy[el.last]` (1656), unless fixed != nullptr (then fixed->corner). Same value as row 19's `fixed->corner[e][el.tok]` (1652, input).
3. Rows 16, 22, 23 (fan size, S-bend size, S fan): read `el.m` directly (1631, 1636, 1672, 1674, 1678, 1682, 1684, 1687-1689 no) and `el.m == 0` branches (1632, 1672, 1704). el.m is set by refresh(): fans from fan_m (set_port_rows 2227-2231 = abs(port_row - vy[item])), row-edge S-bends and arrival S-bends from abs(vy[item_pt]-vy[first]) (621-625), Corner offs from vy (618-619). R1 turns these into inputs.
4. Row 30 bridge over S-bend: reads crossed->m (1762 via max(m,2); 1773-1782 scan only when system != nullptr), crossed->hin/hand (hand from refresh 614 for fans only, not for this S-bend). tag.b/a already encode affine in m.
5. Row 33 curve gaps: sym_routes (841-1000) reads vy (857 start_y for S-side ports, 878-883, 896), port_row (857, 878-879), el.offs.size() as halves (896), and `rows_known`. Y phase also reads numeric_y rows. JunctionCurveGaps needs the halves and the hands of curves (hand from refresh). The per-(v,u) maximum at 1860-1866 depends on numeric halves.
6. `fixed != nullptr` branches (select eq vs ge): rows 19, 22, 23 (1648, 1676, 1684). With fixed == nullptr the X S-bend rows are ge(m) and m is the previous round's |vx[last]-vx[first]| (rounds, 2354-2374) -- a read of vx across rounds.
7. `if (!rows_known)` gates: 1599 (row 15), 1656 (row 20), 1808 (row 33: X phase only), 1877 (block 34-36). `rows_known` is set from the first X pass (2351-2355: el.m from vx).

## (b) Where add_occupancy, hug_heads, add_clearance_x get their order, and what detect(point) needs

Common: all three run in ONE block, 1877-1886, immediately after the static rows are in `cons`:
`compute_node_tracks(); flat_solve(flat);` flat_solve (1130-1144) = least solution (Lo) of `cons` as a difference system, no facts, no floors, JunctionPositions with bound = 1 + sum of positive k. If flat_solve fails (relations contradict) the lazy families are silently omitted (line 1880 `if`). Rows come from `vy` (rows of every point: "row" is vy, never a flat Y). add_occupancy also takes the flag `legality` (false here, true at 2391 where it uses `vx` and only checks JunctionTracksMayShareTile, no cons).

- add_occupancy(flat, false): order per row y = vy[...]: occupants collected per row from chain[e] and row_pieces(r) (1011-1080, which read vy and port_row and el.offs), prio footprints, node tiles. Each row: stable_sort by (flat_left, flat_right, -footprint) with flat_left = flat[lp]+ldx (1195-1196, 1291-1294). Equal flat extents form a tie group (1311-1319); neighbours in the sorted order are linked: `tie` (equal extents or overlap = ties, two cons) or a separation (b.lp - a.rp >= 1 or 2 if touch, one con). Related occupants (same edge/copy, node with its edges) are skipped. Rows (cons) emitted: 1346-1358: `{b.lp, a.rp, apart-b.ldx+a.rdx}` pairs, c from pad_tags(true) nonlinear. Also pushes recorded_clashes with row keys.
  Needs for `point`: x per point (flat[lp], flat[rp]) incl. the offsets ldx/rdx of each occupant (these come from bend tiles of el.offs: needs each bend's L (halves = 2L or 2L+1 for corner), hand, hin), y per point (vy: which row each occupant stands on, incl. fan ends on port_row / item rows, vertical Y-runs one occupant per y between el.last and next.first), node_bits (compute_node_tracks, reads el.offs), and L per bend. t (bridge-over-S-bend column offset) affects only static row 30 / crossed S-bend m: no direct need in detect other than via the bend m of the crossed S-bend (it is in L). The sort key is a function of (x,y,L) only -- a canonical point makes it deterministic.
- hug_heads(flat): order from `hugs` (one HugCandidate per doubled-leg bridge, filled during row 15 with (pt, dx) of every crosser tile on heads0/heads1 rows: dx = z.dx for the copy-0 pass, z.dx - u for the copy-1 pass, u = -s). Picks `low` = min and `high` = max of flat[pt]+dx over hug.tiles (1370-1371). Emits `eq(0, west, low.pt, low.dx-1)` and `eq(0, east, high.pt, high.dx+1)`: 4 relations, order_pin tag. Needs: flat x of each crosser tile point, its dx (bend L of the crosser; straight dx = 0), and which tiles are on heads rows (y per tile: rows). No t needed. hugs is built (and cleared at 1447) only in the X phase.
- add_clearance_x(flat): order from collect_bridges (1385-1414): lines (Y-axis bridges: line = column variable el.first; x_axis ones from doubled-leg HeadsPairs) and heads (xpt, ypt). For every Y-axis line and every head of another bridge with vy[head.ypt] strictly between vy[line.a] and vy[line.b]: side = flat[head.xpt] - flat[line.line]; side 0 -> NO row (F9 asks for graph-order side instead); else one row `{head.xpt,line.line,1}` or `{line.line,head.xpt,1}`. Needs: x of head.xpt and line.line (flat), y of line.a, line.b, head.ypt (vy). No L or t. collect_bridges uses only `bridges_of[e][0]` and `nth_heads` (F8: covers one bridge per heads element; in/out edges not covered).
- plan_clearance side rule (the Y clearance, 2239-2283): not in build() but feeds row 32. It runs build()+flat_solve for Y (phase=1), sets vy = flat_y, set_port_rows(offset or best_offset()), phase=0, refresh(true), build()+flat_solve for X with floors_off, relations_only, skip_clearance set (so the lazy block 1877 and facts and floors are skipped), then for each X-axis line (line.x_axis) and head with flat_x[head.xpt] strictly between flat_x[line.a], flat_x[line.b]: side = flat_y[head.ypt] - flat_y[line.line]; side 0 -> BOTH directions (two rows); else one `{v,u,1}` entry in clear_y. Needs: x of head.xpt/line.a/line.b and y of head.ypt/line.line. No L or t beyond what the point holds.
- Summary of `point` contents: x[pt_count] and y[pt_count] (both from one consistent state: rows come from Y, columns from X), the port offset (port_row, hence port ends of fans and S-port tiles), L (size m) per bend with hand/hin/offs derived, and t (for the S-bend-crossing bridge's column offset; only static). Note vy and `refresh()` state are a SHARED mutable (build() reads el.offs set by the last refresh, plan_clearance mutates and restores vx/vy/port_row/fan_m at 2252, 2264-2271).

## (c) Families emitted only after an inner solve succeeds

- Rows 34, 35, 36 (occupancy, hug, clearance X): only if `flat_solve(flat)` succeeds (1881) AND x_phase && rows_known && !relations_only. add_occupancy additionally returns false (placement failure) on an illegal sharing in legality mode; in the non-legality call it only returns false from add_relation (always true currently, 1362) -- effectively never.
- Row 36 and 35 also need !skip_clearance (36) / hugs non-empty (35).
- Row 32 (clearance Y): clear_y is filled by plan_clearance only if its two inner builds + flat_solves succeed (`ok` at 2250-2257; `if (!ok) return` 2262). Rows are then added only when !x_phase && !skip_clearance. plan_clearance is called in the fixed path at 2312 (when no shared Y) and each round at 2344; shared Y (make_shared / shared->vy) skips it entirely: the shared Y was solved with its own clear_y (2307-2312).
- Row 38 facts: fact_cons only when facts != nullptr && !relations_only; the least/most bounds (1897-1910 and loop 2170-2188) come from cons, not from a solve, but node_checks() (heights) and `pending` jobs read them. No inner solve.
- Row 33 curve gap depends on sym_routes() succeeding (else build() returns false with build_why) but no solve.
- Row 15: `return false` "Z row" if a crosser has no tile on the head row (no solve).
- Rounds (2331-2378): vx from a first X pass without rows (rows_known=false) gives el.m of free_m bends; every later build depends on that (rows 22, 23, 20 via rows_known).
