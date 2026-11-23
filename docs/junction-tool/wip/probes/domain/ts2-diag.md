# TS2 base infeasibility diagnosed (domain-46)

Head 8efcc87edd on wt-e1. TS2 = L1 widened to three lanes: `tunnel T1 B1: A2, A3`, `tunnel T2 B2: A3`, straights `AB<k>: B<k>`, no `@` positions.
Harness: hidden test `planar domain: TS2 base diagnosis` `[.][ts2diag]` (environment: `TS2_EXTRA`, `TS2_OFF`, `TS2_SKIP`, `TS2_W/N/E`, `TS2_PORTS`, `TS2_DUMP`). It rebuilds the DrawnFixture base (flex-stripped variant, RefinePlanarShapeCells, `CheckPlanarDrawable` 15 s, corridor, integer, tidy). Release build; every solve was 3-5 ms (the program is infeasible at presolve), 40 solves in all.
Base options: corridor, end_node, cross_piece, node_lead, grid, tidy, integer (the DrawnFixture options). "all off" = grid, end_node, cross_piece, node_lead all false.

## Solves (options, status)

| # | options | status |
|---|---|---|
| 1 | base, `least_extra` 2 | Infeasible |
| 2 | base, `least_extra` 0 | Infeasible |
| 3 | extra 0, no grid | Infeasible |
| 4 | extra 0, no end_node | Infeasible |
| 5 | extra 0, no cross_piece | Infeasible |
| 6 | extra 0, no node_lead | Infeasible |
| 7 | extra 0, all four off | Infeasible |
| 8-14 | family filter on #7 (below) | see below |
| 15 | #7 minus `node `, `cross `, `curve ` | Infeasible (13-row conflict) |
| 16-28 | tag filter on #15 (below) | see below |
| 29 | the minimal set's own run | Infeasible, exactly the 9 rows |
| 30 | step 5: W and E `@ 2 4 6` (gap 2 tiles), N `@ 3 5 7`, extra 2, base options | Infeasible |
| 31 | E only `@ 2 4 6`, N `@ 3 5 7` (the commit's probe) | Infeasible |
| 32 | W and E `@ 2 5 8` (gap 3 tiles), extra 2, base options | Infeasible |
| 33 | W and E gap 3, extra 0, all four off, `node cross curve` dropped | Optimal |
| 34 | W and E gap 3, extra 0, all four off, nothing dropped | Infeasible (19 rows incl. node, cross, curve A2/A3, `clear 1 2 1`, `clear 2 2 1`) |
| 35 | as 34, `curve ` dropped | Optimal |
| 36 | as 34, `cross ` dropped | Optimal |
| 37 | W and E `@ 2 6 10` (gap 4 tiles), extra 0, all four off | Optimal |
| 38 | W and E gap 4, base options, extra 0 | Optimal |
| 39 | W and E gap 3, base options, extra 0 | Infeasible |
| 40 | W and E gap 4, base options, extra 2 | Optimal |

Family filter on #7 (drop one family at a time, keep dropped if still Infeasible): `piece ` Optimal (needed), `port ` Optimal (needed), `node ` Infeasible (dropped), `cross ` Infeasible (dropped), `curve ` Infeasible (dropped), `clear ` Optimal (needed), `land ` Optimal (needed). The needed families are `piece`, `port`, `clear`, `land`; node, cross and curve rows are not in the base conflict.

Tag filter inside them (conflict of #15: `piece AB1 0`, `piece AB2 0`, `piece B1 0`, `piece B2 0`, `piece B3 0`, `port E gap B2`, `port E gap B3`, `port W gap AB3`, `land 2`, `clear 1 3 0`, `clear 2 1 4`, `clear 2 3 0`, `clear 9 1 0`): `piece AB1 0`, `piece B1 0`, `port E gap B2`, `clear 1 3 0` dropped (still Infeasible); every other row's removal makes it Optimal.

## The minimal conflict (9 rows, all optional rules off, extra 0)

`piece AB2 0`, `piece B2 0`, `piece B3 0`, `port E gap B3`, `port W gap AB3`, `land 2`, `clear 2 1 4`, `clear 2 3 0`, `clear 9 1 0`.

Cells (TS2_DUMP): dummy 2 is in face 7, from A3's first bend (A3.0 to A3.1) heading south, landing on B3.0. Cell 9 = {A3.0 back, B3.0, D2}, cell 2 = {AB3.0, A3.0, A3.1, B2.0 back, AB2.0 back, W edge}.

## Plain reading

- AB2/B2 and AB3/B3 are straight lines (one piece each, heading east), so the W port gap between AB2 and AB3 and the E port gap between B2 and B3 are the same number, and the minimum is one tile (2 half tiles; `port ... gap` is an equation, not a lower bound). The lane 3 line sits exactly one tile below lane 2 along its whole run.
- A3 leaves lane 3's node on the diagonal (A3.0), then turns north (A3.1) and crosses B2 at right angles, so its bend must lie between lane 3 and lane 2. `clear 2 1 4`: that bend must keep the clearance from the lane 2 line. `clear 9 1 0` and `land 2`: the dummy dropped from the bend onto B3 lands on lane 3 at least a clearance from A3's own diagonal (the right-angle foot of a 45 degree triangle whose legs are the diagonal's rise), and `clear 2 3 0` keeps lane 2's node a clearance from lane 3's line.
- So the rise of A3's diagonal plus the clearance of its bend from lane 2 have to fit in the one tile between lane 3 and lane 2, and they do not. The diagonal needs more rise than one tile less the bend's clearance. This is the geometry; nothing about the tunnels or the second crosser enters it.
- It is a lane-spacing conflict of the ports, not of the tunnels: widening the gap fixes it. The commit's probe (`E @ 2 4 6`, `N @ 3 5 7`) widened only E; W stays 1 tile, and the straight lines tie the two gaps, so that probe contradicted itself (#31).

## Step 5: widening the port spacing

W and E widened together (the straights tie them):
- gap 2 tiles (`@ 2 4 6`): still Infeasible (#30), the same persisting families.
- gap 3 tiles (`@ 2 5 8`): with all optional rules off, base options minus nothing dropped Infeasible (#34); it is Optimal when `curve ` rows are dropped (#35) or when `cross ` rows are dropped (#36), so a second, smaller conflict needs node, cross and curve rows together (`clear 1 2 1`, `clear 2 2 1`, `curve A2`, `curve A3` in its list): A2's and A3's turns (the curve spacing between their two bends, and the crossing pieces) at that gap. With the base options and extra 0 it is Infeasible (#39).
- gap 4 tiles (`@ 2 6 10`): Optimal with all optional rules off (#37), with the base options at extra 0 (#38), and with the base options at extra 2 (#40).

So the answer to step 5 is yes, but one tile wider per lane (2 tiles) is not enough: the base is feasible at a 4-tile lane gap (W and E both), infeasible at 1, 2 and 3.
Not tried: 3 tiles on W with a different E gap (the straights make that infeasible by construction), gaps between 3 and 4 half-steps (positions are whole tiles), and whether the geometry rule behind the 9-row core is too strict or is the real game limit (this brief fixes no rule).
