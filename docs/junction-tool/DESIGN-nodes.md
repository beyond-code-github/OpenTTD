# DESIGN-nodes: splits, merges and crossings as placed nodes (D182)

Status: DRAFT for the engineer's review. Nothing is briefed until it is agreed.
Inputs: RECON-nodes.md (inventory, file:line), DIAG-house.md, DIAG-ph1-limit.md, D170, D172, D178, D182.

## 1. The problem in one paragraph
Today only B's nodes are solved (SolveJunctionSkeletonW), and even they are solved from a crossing set measured on routes already made. A's split (A->S) and merge (N->A) have no variable anywhere: they are "any plain piece of A's routed raster in a range" (RECON N1, N2), and the merge's range starts from where A->S happened to leave A. So when terrain bends A, the nodes move or vanish and later edges fail for reasons that come from earlier ones (DIAG-house, NG1/NG1b's drifting merge). RECON §3 lists 14 such route-on-route dependencies; most of the patches of the last two days (CS1/CS2 chaining, D172's after-the-fact width check, NW1's retry, D181) are answers to one of them.

## 2. Two graphs
1. **Track graph (directed, acyclic).** Nodes: ports, splits, merges, crossings. Edges: track segments in travel direction. Every movement is a path in-port -> out-port; a junction has no loops. A crossing is a node two movements pass at different levels without connecting. Routing works here: one A* route per edge, between fixed node footprints.
2. **Placement constraints (cyclic, solved together).** Variables: node positions along their lines. Rules between nodes are difference constraints (x_i - x_j >= k or <= k), e.g. merge - split >= prio_tiles + 2 + phase; crossing columns within D of each other; bridge heads clear of the crossing. Two-sided constraints make cycles; that is normal. A shortest-path pass (what SolveJunctionSkeletonW does) solves them in microseconds; a negative cycle means "no layout here", known before any routing.

## 3. Nodes (v1)
Line A (row 0, +x) and the crossing node are new; B's are the skeleton's today.

| var | node | footprint (tiles that must be usable for these pieces) | window |
|---|---|---|---|
| AS | A->S split on A | row 0: [AS-1, AS] straight X (signal-legal piece before, switch tile; G6, D117a) | 2 .. XL - m_AS |
| NA | N->A merge on A | row 0: [NA-1-prio_tiles, NA+1] straight X (coverage, merge tile, plain goal piece; D108, D148) and the prio row y = -1 over [NA-1-prio_tiles, NA] (signal_pass side rule) | NA - AS >= prio_tiles + 2 + e_phase (today's 1406 constant, D148a); NA >= xO; NA <= frame.width - 3 |
| XL, XR | X_B: the columns where A->S and N->A cross B's two rows | none of its own (B bridges over, D156) | XR - XL <= D = nmax - 3 (D172); XL >= AS + m_AS; NA >= XR + m_NA |
| HE/HW, LS, SL, LM, NW, SB | B's nodes (skeleton, unchanged) | heads: head tiles; NW: N->B's prio coverage on the b row and its prio row; others their switch tiles | as today, with zmin/zmax replaced by XL/XR |
| ports | x_w, x_e, stubs | D179 approaches (unchanged) | D179a widening (unchanged) |

m_AS, m_NA: the least x-run for A->S (N->A) between its node on row 0 and B's W row: the edge drops (climbs) g+1 rows; from the rules a diagonal moves one x per row, plus the curve spacing before and after. Derived once from the rules, and must reproduce the flat T (in the flat T: AS 36, XL 38, g 1, so m_AS <= 2).

Not a node in v1: X3 (N->A over A->S) stays emergent as today (RECON N12). See card 1.

## 4. Projection, step by step (per depth)
1. **Solve** all node variables together (skeleton extended with AS, NA, XL, XR). Infeasible: no layout at this depth or any other (the constraints do not depend on depth) -> "move the cursor".
2. **Footprints on the terrain.** For each node whose footprint has an unusable tile, add one learned constraint pushing it just past the obstacle, AWAY from its compact extreme (the solver pushes every variable to an extreme for compactness, D144, so the only room is the other way). Re-solve. A push only ever moves a node away, so the rounds are bounded by the window widths. Counted and timed.
3. **Route the edges between fixed footprints.** A becomes three segments: W port -> AS footprint, AS footprint -> NA footprint (free: may detour), NA footprint -> E port. A->S: AS footprint -> S stub, B's rows blocked outside [XL, XR]. N->A: N stub -> NA footprint, same window. B's edges as today from the skeleton. No edge reads another edge's route for its ends any more: PlainRasterPieces for A->S/N->A, x_s read-back, the zone read-back and D172's after-check all go.
4. **Segment failure -> learned separation.** If an edge between two placed nodes fails (e.g. A's AS->NA segment cannot fit its detour), add "separation of those two nodes >= current + 1", re-solve, route again. Monotone and bounded by the windows. This is the one feedback loop; card 2.
5. **Depth grows only for edges whose ends depend on depth** (the stub ends of A->S, N->A, B->S, N->B). A failure on a depth-independent edge (A's segments, anything between row-0 and B-row nodes) never grows the box: that is the structural answer to "30 heights???".
6. Finish with the terrain passed (prio rows checked; today finish gets nullptr, RECON §3 last but one).

## 5. Worked by hand
**House at (39, 0)**, xI 40, prio_tiles 14, flat AS 36, XL..XR 38..39, flat NA 52.
- Solve: AS 36, NA 52 (36 + 14 + 2), X_B 38..39 as flat.
- Footprints: NA's row-0 run [37, 53] holds the house -> learned NA - 1 - 14 >= 40 -> NA >= 55. Re-solve: NA 55. AS [35, 36] clear.
- Route A: W -> 36 straight; 36 -> 40 must pass the house at 39: a drop to row 1 and back in 4 tiles is too tight for the main line's curves (expected; the router decides) -> learned NA - AS >= 20, re-solve NA 56 ... until the detour fits (likely 2-5 rounds, each a short segment route). N->A goes from the N stub up to NA; its prio row y = -1 over [NA-15, NA] is checked.
- Expected result: built at the first depth, clean, merge east of the house. Compare NG1b: there the merge drifted back to x 50 because nothing held it.
**Unbuildable hub tile (40, 0)**: the same with the obstacle one tile east: NA >= 56 from the footprint push.
**Flat ground**: no push, no learned constraint: must give the flat T exactly (identity test).

## 6. Why this is not D169 again, and what could go wrong
- D169 modelled every tile (183k variables on 44x16; no layout in 120 s). Here: about 12 integers along lines, difference constraints, microseconds; the crossing and over/under choices are already fixed by the map (D167, D170). Routing stays A* per edge.
- O4c2 routed main lines first and found no legal merge; here nodes are placed before any edge.
- SoverN's per-edge envelopes were whack-a-mole; here each footprint comes from its node type and the rules, the same for every node.
- D170 already asked for this ("solving the layout in a vector map first and then projecting it"); D182 restores it.
- Risk: the feedback in steps 2 and 4 is a loop (D168 deleted the old rule loop because it wandered). Guard: pushes and separations only grow, so rounds are bounded by the windows; every round is counted and timed; a test asserts the round count on the house case.

## 7. Cards for the engineer
**Card 1: X3 (N->A over A->S) as a node in v1?**
Context: N->A crosses A->S between the hub column and B; today it is emergent with a column fence on A->S (RECON N12). No case has failed on it yet.
Options: (a) emergent in v1, a node when a case needs it; (b) a node now (two more variables, its own D172-style window).
Recommendation: (a), simplicity rule. Prior art: D171, D172. Reversible: yes.

**Card 2: learned separation on segment failure in v1?**
Context: the house case needs it: after the footprint push, A's detour between AS and NA does not fit in 4 tiles.
Options: (a) yes, +1 per failure, bounded, counted; (b) derive the needed run from the obstacle and the curve rules up front (tile-unit edge minimums, STATUS step 3 option a), no loop; (c) report failure.
Recommendation: (a) now, (b) later replacing it when the minimums exist (also needed for D176's whole-bay legs). Reversible: yes.

**Card 3: footprints on slopes.**
Context: a footprint tile is checked with the router's own per-piece test (flat, foundation, ramp along the axis, D152, D180). Is a switch tile or a prio coverage run on a ramp acceptable?
Options: (a) whatever the router accepts for that piece (switches on foundations, coverage may ramp); (b) switch tiles flat or foundation only.
Recommendation: (a), the game allows both. Reversible: yes.

## 8. Steps (each one agent, test-first; tags and budgets from TESTS.md)
N0. Skeleton gains AS, NA, XL, XR and their constraints; on flat ground the T map takes A->S's start and N->A's goal from them and must give the identical flat T (outer test: identity + golden trace). B's zmin/zmax from XL/XR.
N1. Footprint push (step 2). Outer test: house case gives NA >= 55 in the log before routing.
N2. A routed as three segments between footprints; A->S and N->A between fixed ends; retire PlainRasterPieces/x_s/zone read-backs and D172's after-check.
N3. Learned separation (step 4, card 2). Outer test: house and hub-tile cases build clean at the first depth; round counts asserted.
N4. Depth grows only for depth-dependent edges (step 5); finish with terrain (step 6).
After N3: re-measure PERF2 row e (40 s today).
