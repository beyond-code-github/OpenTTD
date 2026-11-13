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

## G3.4 Relax smallest-shape pins to floors (after G3.3)
The item-less horizontal S-bend of one row, the bridge's exact rows, the Corner of exactly k: each becomes a floor. A bridge over a diagonal may stand anywhere along it.

## G3.5 Order of work
0. Land D234(3) alone: the bridge-cut search reports the default cut's failure (from wip/s3a-stopped.patch, part C).
1. Prototype (offline, docs/junction-tool/tools/, Python + HiGHS): a hidden test dumps each topology's points, relations (both axes), bends with hands and floors, and cost terms as JSON; the script builds the ILP, solves it, records feasibility, integrality of the LP relaxation, the objective, the choices and time. Topologies: T, LL_RR x L_R, L_R x L_RR, L_R x LL_RR, LL_RR x LL_RR. Check: T and LL_RR x L_R give today's choices (or smaller: list for re-pin).
2. Minimality test: today's choices for T and LL_RR x L_R, each m lowered by one, must contradict.
3. G3.1 node shapes, with the side-by-side oracle.
4. G3.2 band order.
5. G3.3 in the fork (library decided from the prototype: HiGHS via vcpkg, or a hand-written LP if the relaxation is integral).
6. G3.4.
Held: D234(1)(2) (become G3.1 data), S4 (becomes the G3.3 objective), DB1b and G2c (re-briefed on G3).
