# RECON: why placement refuses LL_RR x LL_RR and L_R x LL_RR (any)

Head ffdf49ba00, read-only (a temporary probe test and a debug print in the positions failure branch were used and reverted; nothing committed). Paths under src/junction/; "jp.cpp" = placement/junction_placement.cpp. Spec: `MakeTestJunctionSpec()`, `AnyToAny`, `MakeJunctionHubTopology`.

## 0. Verdict

Cause class: **(c)/(d)**, not (a), not (b). Two adjacent branch in lanes on the S side (ports 1 column apart) that each split toward both main directions. The E-bound leg of the west lane S-bends east (to pass the W-bound leg of the east lane), the E-bound leg of the east lane runs straight; the bridge of each of them must sit exactly over one S-bend of the same crossed edge, and two S-bends of one edge are at least 2 apart. The columns that results from the port spacing are 2 too close in LL_RR x LL_RR (1 too close in L_R x LL_RR). Real (rule-for-rule) conflict, not a single over-strict relation: see section 4.

Variants run (same spec): L_R x L_R and LL_RR x L_R (one S lane): seed. L_R x LL_RR (any) and LL_RR x LL_RR (any): this contradiction. LL_RR x LL_RR keeps-lane: refused earlier ("a crossed lane in edge is not ruled yet"). LL_RR x LL_RR per-direction: a different contradiction (bridge heads hug crossers, edge 14/15), not studied here. L_R x LL_RR has single-lane main groups, so D221 pairing does not change it: it is pure (c).

## 1. The cycles

Both are failures of the first (flat, relations-only, X phase) solve of `solve` (jp.cpp:365 onward; the text is built at jp.cpp:431-455 from `positions.Explain()`, which is the lo chain then the up chain of the failed point, so the printed text lists each link twice, mirrored; the cycle is the 10 (LL_RR) or 6 (L_R) distinct links below). Constraint form: `x[v] >= x[u] + k` (jp.cpp `ge`; `eq` is two of them).

Port slots (JunctionPortSlots order). LL_RR x LL_RR: 0,1 W in (main lane 0,1); 2,3 W out; 4,5 S in (branch lane 0,1); 6,7 S out; 8,9 E in (lane 1,0); 10,11 E out (lane 1,0). L_R x LL_RR: 0 W in; 1 W out; 2,3 S in (branch lane 0,1); 4,5 S out; 6 E in; 7 E out. Node levels: 2r+1 on row r, 2b in band b (band R south of the last row).

### 1.1 LL_RR x LL_RR (positive cycle, weight +2)

Edges in the cycle (kind In/Leg/Out; from -> to; paths = lane paths from slot to slot; program in travel order; `Under(e)` produces no placement element, only Corner/SBend/Over do):
- **e4** In: slot 4 (S in, lane 0, branch) -> node 4 (split of slot 4, level 8 = band 4, south of the last row). Paths 6,7,10,11. Program: empty. Crossed by 6,7,10,11 (not placement relevant, they share the trunk).
- **e5** In: node 4 -> node 5 (split of slot 4, level 4 = band 2). Paths 10,11 (slot 4 to E out 11, 10: the E-bound group, then the lane split at node 5). Program: `SBend(east, no stack item)`, `Under(9)`, `Under(29,c1)`, `Under(29,c0)`, `Under(28,c1)`, `Under(28,c0)`, `Over(3)`. One bridge: edge 5 over edge 3 (`structure.bridges`: edge 5, doubled=false, crossed [3]).
- **e7** In: slot 5 (S in, lane 1, branch) -> node 7 (split of slot 5, level 8). Paths 8,9,12,13. Program empty.
- **e8** In: node 7 -> node 8 (split of slot 5, level 4 = band 2). Paths 12,13 (slot 5 to E out 11, 10). Program: `Under(29,c1)`, `Under(29,c0)`, `Under(28,c1)`, `Under(28,c0)`, `Over(3)`. No S-bend. One bridge: edge 8 over edge 3.
- **e3** In: node 2 (split of slot 1 = W in main lane 1, level 3 = row 1) -> node 3 (split of slot 1, level 4 = band 2). Paths 2,3 (slot 1 to S out 6,7). Program: `Corner(left)`, `SBend(north)`, `Under(5)`, `SBend(north)`, `Under(8)`; it runs east in band 2 with two S-bends, S-bend a under bridge 5 and S-bend b under bridge 8 (the elements are the S-bend a 59->60 and b 61->62; in X an S-bend of a horizontal run has width 0).

Points: 45 = port 4 column, 46 = port 5 column, 17 = node 4, 20 = node 7, 63 = end of e5's S-bend, 64 = e5's near head, 59/60 = e3's S-bend a, 61 = e3's S-bend b first point, 67 = e8's near head.

| # | relation (rule name in jp.cpp) | constraint | jp.cpp |
|---|---|---|---|
| 1 | S port spacing: slot 5 is 1 column east of slot 4 (same group, `column += 1`) | x46 = x45 + 1 | 1310-1316 |
| 2 | chain run e4: port to node 4 is a vertical run, same column | x17 = x45 | 1500-1520 (`along`/`run`) |
| 3 | S-bend size e5: east by at least m = 1 | x63 >= x17 + 1 | 1462-1478 |
| 4 | chain run e5 (S-bend end to its near head: vertical, same column) | x64 = x63 | 1518 |
| 5 | bridge over S-bend e5: bridge column = crossed S-bend first column + VX[side] (side north, VX = 0) | x64 = x59 | 1542-1561 |
| 6 | S-bend size e3 (horizontal S-bend, no X extent) | x60 = x59 | 1473-1477 |
| 7 | chain run e3: S-bend a to S-bend b, at least 2 (`JunctionRunFloor` default) | x61 >= x60 + 2 | 1518, 84 |
| 8 | bridge over S-bend e8: bridge column = S-bend b first column | x67 = x61 | 1542-1561 |
| 9 | chain run e8: node 7 to its near head, vertical | x67 = x20 | 1518 |
| 10 | chain run e7: port to node 7, vertical | x20 = x46 | 1518 |

Sum around the loop 45 -> 17 -> 63 -> 64 -> 59 -> 60 -> 61 -> 67 -> 20 -> 46 -> 45: 0 + 1 + 0 + 0 + 0 + 2 + 0 + 0 + 0 + (-1) = **+2**. In words: x61 = x45 + 1 (bridge 8 stands in port 5's column) but x61 >= x59 + 2 >= (x45 + 1) + 2 = x45 + 3.

Reported text, in the order printed (each link twice): S-bend size (e5) <- chain run (e4) <- S port spacing <- chain run (e7) <- chain run (e8) <- bridge over S-bend (e8) <- chain run (e3) <- S-bend size (e3) <- bridge over S-bend (e5) <- chain run (e5) <- chain run (e5) <- (the same links back to) S-bend size (e5).

### 1.2 L_R x LL_RR (positive cycle, weight +1)

Edges (all Leg kind except none In: with single-lane main groups the S in lane splits straight into a W leg and an E leg):
- **e2** In: slot 2 (S in, lane 0, branch) -> node 2 (split of slot 2, level 4 = band 2 = band R). Paths 3,5. Program empty.
- **e3** In: slot 3 (S in, lane 1, branch) -> node 3 (split of slot 3, level 4). Paths 4,6. Program empty.
- **e10** Leg: node 3 -> node 7 (merge of slot 1 = W out, level 4). Path 4 (slot 3 to W out). Program: `SBend(west)`, `Over(11)`. One single bridge over edge 11.
- **e11** Leg: node 2 -> node 11 (merge of slot 7 = E out, level 2). Path 5 (slot 2 to E out). Program: `SBend(east)`, `Under(10)`, `Under(13,c1)`, `Under(13,c0)`, `SBend(east)`.
(For completeness: e9 node 2 -> node 7, path 3, `SBend(west)`, `SBend(east)`; e12 node 3 -> node 11, path 6, `Under(13,c1)`, `Under(13,c0)`, no bend: the E leg of lane 1 is straight, as in LL_RR.)

Points: 21 = port 2, 22 = port 3, 9 = node 2, 10 = node 3, 38 = end of e10's S-bend, 39 = e10's near head.

| # | relation | constraint |
|---|---|---|
| 1 | S port spacing | x22 = x21 + 1 |
| 2 | chain run e2 | x9 = x21 |
| 3 | chain run e3 | x10 = x22 |
| 4 | S-bend size e10 (west: first >= last + 1) | x10 >= x38 + 1 |
| 5 | chain run e10 (S-bend end to head) | x39 = x38 |
| 6 | bridge over S-bend e10: bridge column = e11's S-bend first column + 1 (side east, VX = 1) | x39 = x9 + 1 |

Loop 21 -> 9 -> 39 -> 38 -> 10 -> 22 -> 21: 0 + 1 + 0 + 1 + 0 + (-1) = **+1**. Reported text (twice each): chain run (e3) <- S-bend size (e10) <- chain run (e10) <- bridge over S-bend (e10) <- chain run (e2) <- S port spacing <- S port spacing <- chain run (e2) <- ... back to chain run (e3).
In words: e10 sits in port 3's column p+1, S-bends west (to p or less) and then its bridge must stand in column p+1, the column e11's S-bend moves into.

## 2. Which placement relations form the cycle

All are in `solve`'s X phase relations (jp.cpp, "chain element" loop and the port block):
- **S port spacing** (jp.cpp ~1310-1316): `column += 1 + (group change ? 1 : 0)`, `eq(0, port, port0, column)`. Branch lanes of one group are 1 apart (D142, D38 default at flat ground). Hard equality.
- **chain run** (jp.cpp ~1496-1535, floors from `JunctionRunFloor` jp.cpp:62-85): for a vertical (Y-heading) run the X part is equality of columns (`run`/`along` with exact); for a horizontal run the least run 2 (default entry of the floor table: signal tile and boundary tile, D217, G1 §16.1).
- **S-bend size** (jp.cpp ~1462-1478): vertical S-bend: last >= first + m (m = 1 columns moved, `ge` in the flat solve); horizontal S-bend: no X extent.
- **bridge over S-bend** (jp.cpp ~1530-1561): the single bridge over a crossed S-bend: `eq(0, heads.first, crossed S-bend.first, VX[side])`, heads one row beyond the S-bend's tiles in Y. Exact equality in X.
- The "bridge over S-bend" relation exists once per bridge, so the two bridges over e3 (LL_RR) pin e3's S-bends a and b to the columns of bridge 5 and bridge 8; the chain run 2 between a and b is relation 7.

Nothing from doubled bridges, row legs, in/out row edges, joiners or prio is in either cycle.

## 3. Classification

- **(a) doubled out edge: not the cause.** No row edge, no doubled bridge, no node of a row is in either cycle (only band In edges and S ports). The programs of the doubled out edge exist (LL_RR: doubled bridges on e19 (leg) and e44 (out edge), e28/e29 doubled; the in/out placement of the out edge, RECON-doubled-row-edges section 2, is untouched by this failure and is not exercised because X fails first). Whether (a) is the next failure cannot be told until (c) is lifted.
- **(b) several bridges or crossings on one band edge (F3): not the cause, a contributor only to LL_RR.** The bridges are single and carry one crossing each (e5 over e3, e8 over e3); no bridge carries two crossings. e3 is crossed by two bridges, each with its own S-bend; that adds the relation-7 chain run (+2) to the LL_RR cycle, but L_R x LL_RR has a single bridge over e11's S-bend and still fails (+1). F3's test (the CX1 dump's e5 with one bridge over e40 and e43) is a different shape.
- **(c)/(d): the cause, and it comes first.** Two adjacent branch in lanes on the S side (slots 4,5 in LL_RR; 2,3 in L_R) both split toward W and E. The W-bound leg of the east lane must cross the E-bound leg of the west lane, so the west lane's E-bound leg S-bends east first (e5 east, e11 east) and the east lane's W-bound leg S-bends west (e10, e9). The east lane's E-bound leg is straight (e8, e12). Each bridge is pinned to the crossed S-bend's column, and the S-bend of the carrier before its own bridge (e10) or the second crossed S-bend on one edge (e3 b after a) needs columns that the 1-column port spacing does not give. The same is true of every pair of adjacent branch in lanes that both split both ways, so it is a property of the S side with 2 lanes, not of the cascade: LL_RR x L_R (one S lane) and L_R x L_R are fine.
  Required spacing from the cycle: LL_RR x LL_RR at least 3 columns between slot 4 and slot 5 (is 1, short by 2); L_R x LL_RR at least 2 (is 1, short by 1), with the programs as they are.

## 4. The rules cited and whether the cycle is real

| relation | rule | note |
|---|---|---|
| S port spacing | D142 (tracks of one direction adjacent, gap 0), D38 (line spacing notation) | D38: "a guideline rather than a mandatory distance ... Spacing may legitimately vary along a line (climbs, coastline, widening before a junction)... on flat ground (M1) the legs use it exactly." The relation is the flat-ground reading: exact at the port. |
| S-bend size | G1 §11.9 (columns "a band node's diverging S-bend moves 1 column per 2 halves"); curve gaps G1 §11.8 | `ge` m, least solution m = 1; not over-strict |
| chain run (vertical, same column) | D217, G1 §16.1 (RW) | a run along the heading keeps the column; not over-strict |
| chain run 2 (e3 a to b) | D217; G1 §11.8 curve gaps (opposite hands at least 2, same-hand consecutive at least min(2TL - 1, max(2, CL))) | the table gives 2; the curve-gap family can only raise it, never lower it |
| bridge over S-bend | G1 §11 "Band crossing (single bridge, F4 of FABLE-G1c2): the carrier passes over the crossed S-bend's 2 tiles in its own column: middle = 2, heads 1 beyond, one D113 piece beyond each head"; band layer rule "a layer holding a band crossing has height = the crossed S-bend's columns moved + 2 K_HEAD + 2 ..., so the single Y-axis bridge spans exactly the S-bend" (G1 §11 band layers); D113 (no split inside a bridge block), D174 (a branch bridge is single) | exact equality is the rule ("spans exactly the S-bend"); it is the smallest-bridge reading, not a necessary one |
| e10/e5 S-bend before its own bridge | structure programs (DESIGN-G1 §10.7), D203(1) | the token order SBend then Over is set by the programs |

Judgement: **a real conflict among the rules as written, not a single relation stricter than its rule.** With the programs fixed (e8, e12 straight; e5, e10 bend before their bridge), exact port spacing 1 (D142/D38 flat ground), the exact bridge-over-S-bend pin (G1 §11) and the run floors 2 (D217) have no solution (cycles +2 and +1 above). The candidates that are looser than their rule, if one has to be named: (i) the exact S port spacing, because D38 calls spacing a guideline that may widen before a junction and D203(1) lets lanes leave their bundle by S-bends inside the box and return to the notation spacing at the ports (D38); the S side's in edges are straight (no Fan, which only exists for W/E ports, R3), so that allowance is not used on the S side. (ii) The exact "spans exactly the S-bend" pin, which is a layout minimum, not a game rule; a bridge may stand on a longer span. Neither is a decision of Pete's today; this recon makes no choice (no hedged padding; margin must come from a rule: a decision or a G1 section ruling that the S in lanes open up, or a program in which the east lane's E leg bends east).

## 5. Evidence trail

- Probe: hidden tag `[probe_pc]` (graph, order, structure, programs dump per topology) and a debug print of the explained constraints in the failure branch (jp.cpp ~431), run with the release build, both reverted. Variants run through `MakeJunctionGraphSeed`.
- Related: RECON-doubled-row-edges section 2 (placement still treats the leg only as doubled; unaffected here), DESIGN-G2 G2.3 (band bridges: not cited by this cycle), D228, D229 (programs of doubled out edges).
