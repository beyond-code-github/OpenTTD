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

## G3.6 One rule source (D240), v2 after REVIEW-rule-source
**Why.** The joint solve takes its rules from a recording of the old placement at fixed sizes. That placement writes some rules only after an inner solve succeeds: occupancy, the hug and the X rules need the flat X order and the rows; clearance needs `plan_clearance`'s flat solves. On the failing hubs those inner solves fail, so the rulebook is incomplete. b-7 and G3.4a stopped on exactly that. D240 retires the path instead of patching it.

**Shape: convert `build()` in place (review §6), not a new module.**
- `build()` becomes the rule source. `SystemTag` already carries each static row's (bend, a, b).
- **Inputs:** every bend size, t (G3.4a), the port offset and each fan's sign σ. `refresh` stops reading vy or vx for sizes and hands.
- **No inner solve:** `build()` emits only static rows.
- **Detection:** `add_occupancy`, `hug_heads`, `add_clearance_x` and `plan_clearance`'s side rule become one function, `detect(point)`. It reads a full point (x, y, L, t) and returns the lazy rows that point needs.
- **The rule list:** the record (`RecordJunctionPlacementSystem`) becomes it, with no flat solves.
- **Name:** the module is not called `junction_rules`; that name is the checker's.

**Static rows (the review's classification, §1).** These are:
- stack origin and rows, band gap, the S edge, the S port row, port columns, S port spacing, row nodes, the waiting room;
- the prio element, row items, the lane split run;
- bridge heads, series bridge heads, the bridge over an S-bend (affine in t; L = 1 uses max(m, 2) as today, stated by a row L >= 2 or a fixed t = 1 at L = 1);
- the chain run, the S-bend, Corner and joiner Corner sizes;
- the curve gaps as counters;
- the fan size with its sign as data (F4: σ_e from the seed, row σ_e (port_row - y_item) >= 0; a flip is a structure choice);
- the box row (offset + notation >= 0) and the span caps (|far - near| - 1 <= max_bridge_middle, max_tunnel_length) (F5);
- the hug inequalities for a straight crosser.

**Lazy rows.** These are occupancy, the hug for a bend crosser, and clearance in X and Y.
- **Order (F12, G3.2):** the side comes from the sweep's band order where G3.2 defines it, else from the detection point's walk order, named. Lazy is only *which* pairs share a row.
- **A tie only where the pieces may share (F1):** a tie is a pair that `JunctionTracksMayShareTile` allows. Any other overlap gets a separation row. A kept row's gap may only grow; the key leaves the gap out and the row is updated to the larger gap.
- **Side 0 (F9):** both axes take the side by graph order (the head's edge order against the bridge's crossing order), never "both sides".

**The canonical point (F2).** HiGHS returns an arbitrary optimal vertex, so the loop detects at a fixed point, not at that vertex. After each MILP solve:
1. keep L, t, the port offset and σ;
2. solve Y at those lengths for the least rows (a difference system; the MILP's y is feasible for it);
3. solve X with today's least solution plus the D215 push.
That point is the one the hover itself would place on flat ground, so the seed equals the hover on flat ground, as the T2 test pins. Detection reads it, and the seed outputs it.

**Seed loop and acceptance (F1, F5).**
- Solve the MILP, take the canonical point, detect, add the new rows or tighter gaps, and repeat.
- It stops when detection adds nothing and every detected row holds.
- Acceptance then requires, at the canonical point:
  - legality (`JunctionTracksMayShareTile` over every shared tile);
  - the post-solve checks of today's fixed path (a crossing edge misses its bridge, the Z row, the blocked span, the curve walk);
  - render, signals, and today's rule-checker use.
- An infeasible set names each clash in it, and whether its row fact held at the last feasible point.

**Hover (F3, F6, F7).**
- It reads the static rows plus a fresh detection at the seed's canonical point, not the loop's kept history. So stale rows never reach it.
- It also reads the hug pins as lazy equalities at that point, because the hover's least objective needs them (the joint keeps only the inequalities).
- At fixed L, t and y each row reduces to two X points; a route shaped horizontal, vertical, horizontal is refused by name (review §3).
- Per offset: the X difference system plus the facts, then an X-only detection, with rows added at that offset kept local to it.
- `JunctionPlacementShared` stays as the hover's carrier of the evaluated rows and the near reject (P5b halves near-search time). The facts builder moves to the evaluation, not to the retired set.

**Cut search (F10).** `CutJunctionTrackBridges` runs the seed loop per cut and ranks cuts by D235 cost, not by box. Its time on LL_RR x LL_RR is measured.

**LL_RR x LL_RR (F8).**
- The doubled-row families are written per role, from the node shapes' A/B items; they are no longer leg-shaped (`exit_item`, the first tile).
- Each Heads element comes from its own Over token, not `bridges_of[e][0]`.
- `collect_bridges` covers in and out edges.
- e5 gets a static oracle derived by hand.

**Migration (each step test-first).**
- **R1:** sizes, t, the port offset and σ become inputs of `build()`; `refresh` reads no solution. Identity unchanged.
- **R2:** `build()` emits static rows only, and `detect(point)` holds the lazy families.
  - Oracle: at the seed, static rows plus detect(seed point) equal today's relations, family by family, and at L + 1 per sized bend. The curve-gap comparison reduces the counters to today's per-(v, u) maximum first (F11). The oracle also runs on the three failing hubs where their static families are recorded.
  - Identity unchanged.
- **R3:** the record is the rule list, and the joint loop runs on it with the canonical point, ties as above and the acceptance list. It is measured on the five hubs. Tests:
  - "the loop ends legal";
  - "placement on flat facts is the seed";
  - "fan signs are data";
  - "span cap is a row".
- **R4:** the seed goes live, and the cut search runs on the loop. The re-pins go to Pete case by case.
- **R5:** the hover reads the evaluated rows. Tests: "every hover row has two X points", "a hover plan does not depend on visiting order", and the identity sweeps equal or re-pinned by listed rule.
- **R6:** delete the rounds, `plan_clearance` and the flat solves.
- **In parallel, after R2:** the doubled families per role (F8) and several bridges per band edge (G2c).
