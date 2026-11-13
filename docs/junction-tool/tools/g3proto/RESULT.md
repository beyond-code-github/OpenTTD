# G3P result: offline joint solve (DESIGN-G3 §G3.3, §G3.5 step 1)

At 54bd85d75f, `MakeTestJunctionSpec()`, pairing any. Dumps: `<name>.json` from the hidden test `[.g3dump]` of `docs/junction-tool/wip/g3p-dump-hook.patch` (not committed). Model: `solve.py` (HiGHS 1.16). Run: `python solve.py <this dir> <out dir>`.

## What was dumped
- One placement call per topology. T and LL_RR x L_R: the seed's cut, placed again; the choices match the seed's. The three failing ones: the default cut (the first call of the cut search).
- The last X and the last Y system of that call: every relation `x_v - x_u >= c` with its reason, and its m tag (`c = a m + b` of a bend, or a non-linear site). Also the bends (kind, hand, heading, first and last point, m, floor, size source), the chains and the lane paths.
- Failing calls: L_R x L_RR stops after round 4 ("does not settle"), with m values that have diverged (e5 671, e14 665). L_R x LL_RR and LL_RR x LL_RR fail in the Y solve of round 0, so their X system is the first pass, before the rows are known. That pass has no occupancy, hug or X curve-gap relations.

## Model
- Integer x and y per point, all at least 0, origin (0, 0). One integer L per bend, with these floors:
  - free S-bend: 1;
  - S fan: 0, plus a binary z (L >= z, L <= M z);
  - joiner Corner: k;
  - rows-sized horizontal S-bend: 0.
- Every dumped relation is a row; where it is tagged, `a m + b` becomes `a L + b`. An S fan's sized-branch rows apply when z = 1; its zero-size rows apply when z = 0 (big-M).
- Link rows:
  - Free S-bends and S fans: `x_last - x_first = VX[side] L`, with `side = (hin + hand + 4) % 4` (junction_placement.cpp:1492, the "S-bend size" block, 1494-1507). The Y link `y_last - y_first = VY[hin] (L - 1)` (1496, 1503) is already dumped as a tagged equality, so it is not added again. The dumped X relation is `>=` in the rounds, so the X link is added.
  - Joiner Corner: `y_first - y_last = L`. The size is `|vy first - vy last|` (refresh, 1482), and the floor `ge(first, last, k)` fixes the sign.
  - Rows-sized horizontal S-bend: `y_item - y_first = VY[side] L` (refresh 512/514). Where the rows are known, this sign holds on every bend of T and LL_RR x L_R.
- Objective, in money:
  - rail_piece per run tile, as the absolute difference of the points along the element's heading;
  - rail_piece x 2L per bend (half pieces);
  - bridge `(price(1) - price(0))` per span tile;
  - signal x n per lane path and copy, with `sd n >= len - (sd - 1)` (n = floor(len / sd), n integer, len in track pieces off bridges);
  - plus 0.001 x sum L.
  Constants are dropped (Corner +1, bridge heads and price(0), run end tiles), so the objective is exact up to a constant per topology.

## Per topology

| topology | today | vars | rows | ILP | ILP ms | LP ms | LP integral (L) | objective | box |
|---|---|---|---|---|---|---|---|---|---|
| T | places, 41 x 14 | 154 | 429 | optimal | 15 | 1.4 | yes | 27050 (today's sizes: 27300) | 41 x 14 |
| LL_RR x L_R | places, 62 x 25 | 362 | 1171 | optimal | 26 | 2.5 | yes | 86350 (today's sizes: 86350) | 62 x 25 |
| L_R x L_RR | does not settle | 216 | 5407 | optimal | 72 | 6.1 | no | 1907654 | 1428 x 1410 |
| L_R x LL_RR | contradiction (curve gap e17, S-bend e9) | 266 | 678 | optimal | 101 | 2.0 | no | 53300 | 26 x 47 |
| LL_RR x LL_RR | contradiction (S-bend e45, stack rows, row nodes) | 647 | 1670 | infeasible | 6 | 1.8 | (infeasible) | - | - |

- The rows include the cost rows (two per run or span) and the signal rows. The dumped relations number 359, 993, 5297, 536 and 1314.
- "LP integral" means every L (and z) is integral at the LP optimum; the points are fractional in every LP.
- The box is the least east and south edge with the ILP's lengths fixed: a second LP, used for the report only.

## Bends that differ (T and LL_RR x L_R)
- T: one bend. The joiner Corner on edge 5, token 0, is 6 today and 5 in the ILP. The objective is 27300 at today's sizes and 27050 at the ILP's: 250 less, with the other choices the same. Through the fixed path (`[.g3apply]`, `PlaceJunctionTrackGraph` with these choices), it places and renders with a box of **42** x 14, one column wider than today's 41.
- LL_RR x L_R: none. Every free S-bend and joiner Corner equals today's settled value. Through the fixed path it places and renders at 62 x 25, as today.

## Failing topologies
- **L_R x L_RR:** feasible, but at the diverged round-4 scale. The relations held at the dump carry m of about 671:
  - occupancy, e.g. `x9 - x29 >= 683`;
  - curve gaps, e.g. `x9 - x33 >= 1353`.
  With them, the ILP sizes grow further: e5 691, e14 fan 685, Corner e8 1385, box 1428 x 1410. The fixed path refutes it: joiner_div Corner choice (edge 8) <- chain run (edge 2) <- S port row <- chain run (edge 14) <- S-bend size (edge 14) <- curve gap (edge 14) <- curve gap (edge 11) <- row nodes <- stack rows (row 1). This dump cannot test whether the joint solve closes the gain-2 loop: the non-linear constants would need a dump at round 0.
- **L_R x LL_RR:** feasible, box 26 x 47. Its X system has no row-dependent relations (first pass), and its Y curve gaps are held at the dumped m. The fixed path refutes it with today's cycle: S-bend size (edge 9) <- curve gap (edge 17) <- curve gap (edge 17) <- S-bend size (edge 9). The held curve-gap constant is what made the model feasible: e9 has L 9 and 10 against 5 and 6 in the dump, and the real curve gap grows with them.
- **LL_RR x LL_RR:** infeasible. The IIS (LP relaxation, HiGHS `iis_strategy` 2) has 3 rows, in Y only and with no length in it:
  `Y stack rows (row 1): y5 - y4 >= 1 <- Y row nodes: y4 - y37 >= 0 <- Y S-bend size (edge 45): y37 - y5 >= 0`.
  This is the node_item reading of RECON-s3-probe §3 (class b). No size can repair it.

## IIS chains
- LL_RR x LL_RR: the chain above. No other model is infeasible: every ILP that is infeasible also has an infeasible LP.

## Non-linear m dependencies (listed, held at the dumped constant)
- **Curve gap, Y form and X form.**
  - The unit offsets come from the bend rasters (2m halves).
  - The relation is `ceil(need / 2)`, and the largest pair per (v, u) is kept.
  - The X form also reads the rows (vertical runs and the port rows).
  - Rows per topology: T 6 X + 2 Y; LL_RR x L_R 42 + 10; L_R x L_RR 24 + 21; L_R x LL_RR 27 Y; LL_RR x LL_RR 88 Y.
- **Occupancy.**
  - The occupants' tile extents (ldx, rdx) come from the bend rasters and the rows.
  - The order of a pair comes from the flat solve.
  - Rows: T 30, LL_RR x L_R 117, L_R x L_RR 4807.
- **Bridge heads hug crossers.**
  - The crosser tile offsets come from the bend rasters and the rows.
  - The outermost tile is chosen by the flat solve.
  - Rows: T 18, LL_RR x L_R 82, L_R x L_RR 24.
- **Zero-size branches.** Fan or S fan m == 0 swaps the relation set: the fan lies on its port, or it has no rows. For S fans this is modelled exactly by the binary z, over the rows the dump holds of each branch. W/E fans stay at their dumped size.
- **Bridge over S-bend.** The rows of the bend's first column come from the raster of max(m, 2). The hook checked m = 2..64: the result is constant in every dump, so it is tagged `a = 0`.

## Left out, with the reason
- **W/E fans (K::Fan): held at the dumped size.** The size is `|port row - item row|`. The port rows are numbers set by the `best_offset` scan, not points, and the dump has no port-offset variable.
- **Corners of k (not joiners) and the item-less horizontal S-bend: held at k and at 1.** Today they are pins; G3.4 relaxes them later.
- **Rows-sized horizontal S-bends: linked to their rows.** Their columns enter only through the non-linear sites. In the objective, their element has X extent 0, so the run through them also counts their columns: the cost counts those columns twice.
- **The D215 "next to feature" push.** It is the X solve's post-objective, a pin after the least solution, not a relation. The cost objective replaces it.
- **Tunnels.** The spec has no tunnel price, and no dump has a tunnel.
- **Objective constants** (see Model).
- **The non-linear sites above.** They are held at the dumped constants. That is why L_R x LL_RR and L_R x L_RR come out feasible in the model while the fixed path refutes them.
