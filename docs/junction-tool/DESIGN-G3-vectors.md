# DESIGN-G3: placement as vectors (D235)

Source: REVIEW-graph-model (classes A-G), RECON-s3-probe, RECON-yapf-compute. Rulings: D235 (vectors, cost objective, re-pin case by case), D233, D227, D230, D231.

## G3.0 The idea
- A hub is vectors. A node fixes the directions where tracks part or join; an edge is a chain of pieces, each a direction (one of the 8 the game allows) times a length; axis runs and 45-degree diagonals are the same kind of piece.
- Where two paths meet (split to merge, port to port, rows stacked beside each other), their vector sums agree: a linear equality. Rules (curve gaps, runs, bridge spans, prio coverage, waiting room) are lower bounds on lengths or on sums of lengths.
- Positions follow from the sums. Placement infers nothing from token order.
- Unchanged: the graph (nodes, edges, paths, crossings, levels); the programs as the shape of an edge's interior; the hover's per-axis difference solve with the seed's sizes fixed; the level solve; render; signals.

## G3.1 Node shapes as data (removes A, B, C)
The programs emit, per node:
- kind (split, merge); its stack item (row) or band;
- trunk heading in and out;
- the straight leg and the diverging leg with its hand;
- the token that serves the node (the bend whose end is the node's point), or none;
- its footprint parts (split or merge tile, required straight runs, prio coverage).

Per edge end: its heading and the token, or none, whose end is the node's point.
`JunctionTrackStep::index` splits into an item field and an edge field.
Placement reads these; `node_item`, the positional first/last rules and the second heading derivation go; one chain builder serves rows and bands.
Migration: the old inference and the new data side by side; they agree on every node and edge end of T and LL_RR x L_R (identity by construction); their disagreements on the failing topologies are listed in one run.

## G3.2 Band order (removes G)
The order sweep emits the west-to-east order of edges and nodes per band slice. Occupancy and the bridge-head hug use it, never the least solution's order. Test: equal to today's flat order on T and LL_RR x L_R at every row. Band order is a rule from the sweep, not a cost-search choice.

## G3.3 One joint solve for the seed's sizes (removes E, the forced part of D)
- Unknowns: today's points per axis plus one length per bend (Fan, Corner, S-bend). With all lengths fixed it splits exactly into today's two difference systems.
- Link rows per diagonal: `x_last - x_first = s L`, `y_last - y_first = s_v (L - 1)` (signs from the hands, which the programs fix).
- Objective: the flat build cost at game prices: track pieces (axis and diagonal at their prices), bridge span tiles x type price plus heads, tunnels, signals by drag spacing. Ties broken deterministically (lowest length vector in edge order). Speed is never traded: curve and ramp rules stay hard constraints. Terrain costs stay with the hover's exact pricing.
- Output: `JunctionPlacementChoices` (m, corner spans, port offsets) only; positions still come from the fixed path the hover and identity sweeps consume.
- Infeasible: the solver's irreducible infeasible set, printed as today's "contradict each other" chain over both axes. The fix for a shape that cannot close is a program edit: first a run before the Corner, then the node one band higher.
- Problem class: ILP (the matrix is not a network matrix). Size: about 250 variables and 900 rows for LL_RR x L_R. Runs once per seed and per bridge cut, never per hover offset.
- Pathfinder compute (RECON-yapf-compute) is not part of this objective: lengths barely change it. It is a measure for choosing between structures (cascades, merge before or after a crossing, parallel equal-cost alternatives), candidate for the later structure cost search.

## G3.3a The three tile-pattern rules as length sums (draft for Pete)
Today these three rules are why the prototype could not decide L_R x L_RR and L_R x LL_RR: they read bend rasters and numeric rows, so they are non-linear in m (G3P RESULT, "Non-linear"). In vector form each one is linear once two things are fixed: every heading (the programs fix them, G3.1) and an order (the sweep, G3.2). The ILP then solves where things go for a fixed arrangement. Choosing the arrangement stays with the order and structure stages.

**Premise P1 (test first in G3.3b).** Every bend's raster is affine in its length: for each (kind, heading, hand), the units, the offset of each curve from the first point, and the last point are `a + b L` for L = 1..64. That is one table per kind, checked against `RasterizeJunctionRoute`. If any of them is not affine (for example from rounding at odd L), that bend gets a parity variable or a floor, and comes back to Pete.

**1. Curve gaps (DESIGN-nodes 4a, `JunctionCurveGaps` unchanged).**
- Each route (lane path and copy) gets one unit counter `u_k` per curve k, in travel order.
- Between consecutive curves: `u_{k+1} - u_k` = the bend's own units up to the curve, `a + b L` (P1), plus each run between them, `2 h·(p_next.first - p_prev.last)`, where `h` is the run's heading (a unit vector, fixed). That is one equality row per step.
- Each pair from `JunctionCurveGaps` becomes `u_j - u_i >= min_gap`.
- What goes away:
  - The X form and the Y form become one form. A route may turn through both axes, since both are in the same system.
  - The `ceil(need / 2)` becomes `2Δ >= need` with integer Δ.
  - The "largest pair per (v, u)" merge is no longer needed.
- Size: about one row per curve per route plus one per pair. That is about the 52 to 88 relations we have today, plus the counters.

**2. Bridge heads hug crossers (D80, D118, D217).**
- A crosser is the piece of the crossed edge that crosses the bridge's row. Which piece that is is topological: its Under or Above token.
- Its column at row `y_r` is `x_first + s (y_r - y_first)`, with `s` = 0 for a vertical run and ±1 for a diagonal, fixed by the hand. With both points as variables, that is linear. Its tiles on that row are a constant offset table, from the raster.
- The hug becomes inequalities: `west head <= every crosser tile - 1` and `east head >= every crosser tile + 1`. The cost objective (span tiles × the bridge price) then pulls both heads tight, so the hug is the cheapest choice, not an equality to a max. That removes the "outermost by the flat solve" choice.

**3. Occupancy (unrelated tracks never share or touch a tile, D217).**
- Two pieces are each a segment with a fixed direction, so "B is clear of A on the side n" is linear: for each end of A and each end of B, `(b - a)·n >= g(dirA, dirB, n)`.
- `g` is a constant from `JunctionTracksMayShareTile`. It is tabulated once per direction pair by rasterising two pieces side by side, and checked by a test. Upper|Lower and Left|Right pairs get the smaller gap.
- On a row, the occupants are known by topology (row items, and the crossing pieces as in 2), and their order is the sweep's (G3.2). Neighbours in that order get the rows above, with n = the row's heading.
- Between rows, in a band slice, the west-to-east order of edges comes from the sweep. A pair needs separating only where the two edges are at the same height, and which pieces are at the same height is the open part. Today the flat solve's order decides it, and that is class G. Two ways:
  - (a) The sweep also emits, per band slice, the south-to-north order of the bends' ends. Everything is then linear, and the arrangement is fully topological. This is more sweep work, and a wrong order shows as an infeasible set that names the pair.
  - (b) Lazy clash constraints (constraint rows of the solver, not track rows): solve, check occupancy on the result, add the violated pairs with the order the solution shows, and solve again. HiGHS re-solves warm in milliseconds. This is today's flat-order idea, but it converges, because rows only ever get added, never swapped.
  - **Recommendation and D236: (b) first.** It needs no new sweep output, and it shows how many rounds real hubs take. (a) is the fallback if a hub cycles or a pair's order flips.
  - **Measured (b-5):** re-reading the held occupancy constants every round cycles with period 2 on L_R x L_RR and L_R x LL_RR. Each round's rows replace the last round's, so (b) must keep its rows.
  - **The b-6 form.**
    - **Tile forms (premise P2, test first).** Every tile of a bend's raster at length L is either a head tile, whose offset from the bend's first tile is constant, or a tail tile, whose offset is `(dx0 + dx1 L, dy0 + dy1 L)` counted by its index from the raster's end. The form is read from the rasters at L and L + 1, and checked against L' = L + 1..64.
    - **Columns.** So every occupant end's column is `x[pt] + c0 + c1 L(bend)`. That holds for a straight tile, a run's end (±1), a node, a prio footprint tile and a bend's tile.
    - **Clash rows.** A clash is the pair the recording emits today (neighbours on a row in the flat order). As a row it is `b.left - a.right >= gap`, with the gap the recorded one (1, 2 when the two touch, or the recorded apart for a tie). The row fact, that both stand on one row, is held: it is the lazy part.
    - **Accumulation.** A clash is keyed by its two occupants' forms and its tie flag. The loop keeps every clash it has seen. A key seen again, or seen with a and b reversed, keeps its first form. Rows are only ever added, so the loop ends.
    - **Zero-size bends.** A clash on a tile of a bend with a zero branch gets `- BIG (1 - z)`.
    - **What it does not do.** A kept clash whose two occupants later stand on different rows is stricter than the game needs. The fixed path at the fixed point is the check.

**What stays non-linear, and why that is fine.** The W/E fan size is `|port row - item row|` with port rows from the `best_offset` scan. Make the port row a variable: `best_offset` becomes the objective's job (a short fan costs less), and the fan length is then a difference. That is linear with the sign fixed by the side.

## G3.4 Relax smallest-shape pins to floors (after G3.3)
The item-less horizontal S-bend of one row, the bridge's exact rows, the Corner of exactly k: each becomes a floor. A bridge over a diagonal may stand anywhere along it.

**G3.4a, the bridge over an S-bend (first, from the b-6 measure: LL_RR x LL_RR is infeasible with the exact pins and feasible without them).**
- Today a single bridge over an S-bend stands in the column next to the S-bend's start (t = 1), with its heads just outside the bend's tiles in that column.
- **The choice.** t, the bend's sideways column the bridge stands in, counted from the start, becomes a choice: `over[edge][token]` of the bridge's Over token. It is stored as t - 1, so an empty or 0 choice is today's t = 1.
- **Premise P3 (test first).** For every (heading, hand) and L = 2..64, the rows of the bend's tiles in column t satisfy `lo(t) = lo(1) + s (t - 1)` and `hi(t) = hi(1) + s (t - 1)`, for t = 1..T(L) with T(L) = T0 + T1 L. The rows are contiguous within a column. The test reads s, T0 and T1 from the rasters and checks them; if no such form exists, the premise fails and comes back.
- **Fixed path.** The bridge's column is `x_first + side t`, and its heads are at `lo(t) - 1` and `hi(t) + 1` (north) or mirrored, all at the chosen t.
- **Joint model.** There is one integer variable t per such bridge, with 1 <= t <= T0 + T1 L(bend). The X relation's constant is `side t`, and the Y relations' constants are affine in t. The tie break adds EPS t, so an equal-cost solution keeps today's t = 1.

## G3.5 Order of work
0. Land D234(3) alone: the bridge-cut search reports the default cut's failure (from wip/s3a-stopped.patch, part C).
1. Prototype (offline, docs/junction-tool/tools/, Python + HiGHS): a hidden test dumps each topology's points, relations (both axes), bends with hands and floors, and cost terms as JSON; the script builds the ILP, solves it, records feasibility, integrality of the LP relaxation, the objective, the choices and time. Topologies: T, LL_RR x L_R, L_R x L_RR, L_R x LL_RR, LL_RR x LL_RR. Check: T and LL_RR x L_R give today's choices (or smaller: list for re-pin).
2. Minimality test: today's choices for T and LL_RR x L_R, each m lowered by one, must contradict.
3. G3.1 node shapes, with the side-by-side oracle.
4. G3.2 band order.
5. G3.3 in the fork (library decided from the prototype: HiGHS via vcpkg, or a hand-written LP if the relaxation is integral).
6. G3.4.
Held: D234(1)(2) (become G3.1 data), S4 (becomes the G3.3 objective), DB1b and G2c (re-briefed on G3).
