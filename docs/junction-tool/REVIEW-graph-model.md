# REVIEW-graph-model: review of the graph pipeline's representation and placement solver (at b4a995fadb)

Read-only. "jp" = src/junction/placement/junction_placement.cpp, "jt" = src/junction/topology/junction_topology.cpp, "h" = src/junction/topology/junction_topology.h, "pipe" = src/junction/junction_pipeline.cpp. Nothing was built or run. A runtime claim that no recon measured is marked INFERRED.

## Verdict
There are two faults, not one, so the engineer is half right.

The node graph itself (nodes, edges, paths, crossings, levels) is sound, and no stop traces back to it. What is missing is a layer between programs and placement. A node has no geometry of its own: h:97-102 holds only split, slot and level. Its point, its straight and diverging legs, its heading and its stack item are stored implicitly in the tokens of its edges. Placement re-infers them from token kind, position, role and index (jp:246-277, 544-558, 988-989). That inference accounts for 9 of the 17 causes behind the 10 placement stops (classes A, B, C and G below).

The other fault is the solver. The seed sizes every diagonal outside any constraint system: it reads |dx| off one axis's least solution and feeds it to the other axis as an exact constant (jp:2102-2142, 1503, 1482). That is a perturbation method inherited from the traced flat T (DESIGN-N3 §1, §6), now used for synthesis. Once S-bend and fan sizes are free, it neither minimises them nor converges (RECON-s3-probe §1b: m' = 2m + 33). It also forces exact pins that no rule asks for (classes D and E).

Fixing only the representation would not have placed L_R x L_RR. Fixing only the solver would not have stopped the token misreads. I recommend three changes:
1. The programs emit node shapes and edge ends as data.
2. The order sweep emits a left-to-right order inside each band.
3. The seed gets its sizes from one joint solve over both axes, in which a diagonal is an ordinary length (the engineer's "diagonals should not be special"). The hover stays as it is.

All three keep T and LL_RR x L_R identical, provided the joint solve only chooses `JunctionPlacementChoices`.

## Q1. The placement-side stops, classified
The classes:
- **A:** which token end is a node's point is inferred (jp:273-277).
- **B:** a node's stack item is read from an edge's tokens; `index` is an item or an edge depending on the token kind (h:226; jp:544-558).
- **C:** an edge's start heading is derived twice, in the programs (jt:1152-1157) and in placement (jp:246-252).
- **D:** an exact relation encodes the smallest shape, not a rule.
- **E:** diagonal sizes are chosen outside any system (the least solution plus rounds).
- **F:** the search or the report hides the cause.
- **G:** the left-to-right order of tracks in a band is read from the flat least solution (jp:1137, 1646-1654).

| Stop | Topology | What failed | Class | Evidence |
|---|---|---|---|---|
| R7b (parked) | T, LL_RR with an extra item between Lane and Approach | the item-less Approach S-bend is exactly one row (jp:1510); the prio element is drawn beside the merge | D | STATUS 2026-11-13 |
| TS1 (parked) | LL_RR x LL_RR, per direction | a cycle through e3's own bridge and e20's S-bend | D (INFERRED: the bridge pin) | STATUS "2026-11-13 later" |
| Night 3 | L_R x LL_RR, LL_RR x LL_RR | X cycles of +1/+2: exact S port spacing, exact bridge column, chain runs | D; C for LL_RR's e3 | RECON-llrr-placement-contradiction §1; REVIEW-port-fans F1(d) |
| SF1 | L_R x LL_RR | band chain heading; "S edge" exact; out fan starting on the merge tile; stale label; fan m allowed to shrink | C, D, A, F, E | REVIEW-port-fans F1-F4, F7 |
| S2 | L_R x L_RR, L_R x LL_RR | a lone S-bend pinned to both nodes: a +5 Y cycle | A | RECON-s2-cycle §2, §4 |
| S2c | LL_RR x LL_RR | ASan: an Over token's edge index read as a stack item | B | STATUS night 6 |
| Probe after S2c | L_R x L_RR | the reported "bridge heads hug crossers" comes from the last cut tried; the real failure is "does not settle", gain 2 | F, E | RECON-s3-probe §1 |
| Probe after S2c | L_R x LL_RR | a Corner leaving a merge shares the merge's point, and so does the Converge S-bend: a zero run against a curve gap | A | RECON-s3-probe §2 |
| Probe after S2c | LL_RR x LL_RR | `node_item` reads the Copy item of a doubled out edge | B | RECON-s3-probe §3 |
| S3a | LL_RR x LL_RR, L_R x LL_RR | assert `halves >= 1`; then occupancy e6/e12 | B (INFERRED, below); G (INFERRED) | STATUS night 7; wip/s3a-stopped.patch |

Tally of causes: A 3, B 3, C 2, D 4, E 2, F 2, G 1. G2b, CX1 and the "crossed lane in edge" refusal are topology-side and not counted here.

**Common cause.** Every stop is placement deciding something that no earlier stage states, and deciding it by local inference:
- node geometry and items (A, B, C);
- the band order (G);
- the diagonal sizes and the objective (D, E).

Each fix so far tightened one inference: G2.6 roles, the S0 heading rule, S2c's `names_item`, D234. The next topology then met the next inference.

**The S3a crash (INFERRED, not run).** The sequence I expect:
1. With D234(1), LL_RR x LL_RR passes Y and reaches X with the rows known.
2. The doubled-leg block runs for every doubled row edge (jp:1377). So the doubled out edge e44 gets the leg's items.
3. Both of e44's S-bends start on `node_item(in_edge[r])`. Row 1's leg e19 is doubled, so that is the north item 3. Both end on their token's item c, which is also 3 (stack in RECON-s3-probe §3). Each S-bend therefore spans 0 rows.
4. The row S-bend walk does not guard 0 (jp:783). The raster call at jp:810 then asserts in `JunctionBendMoves` (junction_track_geometry.cpp:22).

This is DB1b, DESIGN-G2 §G2.4's A/B items for out edges, not yet placed. `collect_bridges` also skips out-edge bridges (jp:1235-1247).

## Q2. Representation
What placement needs, and where each thing lives today:
- **Node geometry: no record.**
  - The programs choose the straight leg (`diverges`, jt:1082-1091) and keep it only as the absence of a Diverge token.
  - Placement re-infers it from point sharing (jp:988-989) and decides point sharing from token kind and role (jp:273-277).
  - Corner and Fan tokens carry no role.
- **Node footprint: placement/CLAUDE.md says a node has one; the code has none.** It is spread over:
  - the leg chain's Prio element (jp:628-640);
  - the waiting-room relation (jp:1338-1361);
  - floor-table pairs (jp:77-83);
  - node bits (jp:964-1001).
- **Which bend serves which node: three rule sets.** S-bend roles (G2.6, h:221-222), the positional rules, and now D234(2).
- **A node's row and item.** `node_item` reads the last or first item-naming token of an incident edge (jp:544-558). For a doubled out edge the first such token is on the Copy item (RECON-s3-probe §3). `index` is overloaded (h:226), which caused the S2c ASan error.
- **Edge headings: derived twice** (jt:1152-1157, jp:246-252). SF1-F1 was the two derivations disagreeing.
- **Row and band sizes: no variables.**
  - A row's items are exactly adjacent (jp:1288).
  - A band is "at least 1" between rows (jp:1296-1297).
  - An S group has exact spacing (jp:1321-1325).
  - DESIGN-G2 §G2.5's sizes have no variable at all.
  - W/E fans are sized by a scan outside the solver (`best_offset`, jp:1981-2005); S fans by the least solution (jp:1494-1501). That is two models of one taper, and REVIEW-port-fans F5 is the result.
- **Diagonal size: six mechanisms for one shape** (two 45-degree curves around a diagonal). `m` is a mutable field (jp:217), not a solver variable.
  - Vertical S-bend: m read from X (jp:2109, 2133), exact in Y (jp:1503).
  - Band horizontal S-bend: sized from the rows, X extent 0 (jp:513-514, 1511).
  - Row S-bend: sized from the rows (jp:511-512).
  - Corner: fixed k (jp:1486-1488).
  - Joiner Corner: span from the rows, exact in X (jp:1474-1484).
  - W/E fan: sized from the port rows (jp:498). S fan: sized from X, 0 allowed.
- **Left-to-right order inside a band: not represented.** `JunctionTrackOrder::bands` has layers, not columns (h:140-155). Occupancy takes its order from the least solution (jp:1137, 1646-1654), which is not a designed order: RECON-s3-probe §1b has n6 at column 0.
- **Three readings of one token.** There are two chain builders (band jp:240-297, row jp:562-642) with two fan kinds (`K::SBend` with `fan`, and `K::Fan`). Render then re-derives each bend's size by matching rasters to its span ends (junction_render.cpp:127-143).

**The engineer's hypothesis.** The graph is not the root cause; the program layer is. A token string per edge is the right representation for an edge's interior. It is the wrong one for its ends, because a node's geometry is shared by three edges and owned by none. Making node shapes explicit removes A, B and C by construction. It does nothing for D or E.

## Q3. Solver
**With sizes fixed: keep it.** A difference system per axis with the least solution is right, and the hover already runs exactly that:
- it places with the seed's fixed choices (junction_hover.cpp:32, 85; jp:2061-2100);
- it shares the Y solve across offsets (jp:2076-2080).

**For the seed it is not adequate**, for four reasons:
1. **The least solution minimises coordinates, not diagonal extents.** In L_R x L_RR nothing pushes n6 east, so it sits at column 0 and both e5 and e14 span the box (RECON-s3-probe §1b). That comes from the objective, not from a rule.
2. **The couplings run both ways, so a loop through both needs rounds.**
   - An S-bend couples X to Y: m comes from vx and its rows are exactly m - 1.
   - A joiner Corner couples Y to X: its span comes from vy and its columns are exact (jp:1482-1483).
   - m never shrinks (jp:1500, 1507, `ge ... el.m`), so the rounds are a monotone ascent. They settle only if a fixed point lies above the start and the readback does not overshoot it.
   - With the overshoot of reason 1 the loop gain is 2 (two full-width diagonals stacked in Y, one Corner back to X), hence m' = 2m + 33.
3. **Y cannot repair a too-small m.** In Y the S-bend's extent is exact, so a Y cycle that needs a larger m has no fix from the Y side (RECON-s3-probe §2, +4).
4. **The split axes force pins.** A bridge over a diagonal must stand on the diagonal's first column (jp:1582-1585), because "anywhere along it" would tie a column to a row (REVIEW-port-fans Q2). That pin is one of the links of the night-3 cycle.

**Is it still a difference system under some reparametrisation? No.**
- Track runs in four line directions (x, y, x + y, x - y). A difference system splits into independent blocks for only two linear forms.
- Rotating to u = x + y, w = x - y makes a 45-degree run a single equality (w constant). But it turns an axis run into two equal differences in u and w: the same coupling, moved.

**What the joint system is.** With the hands fixed (they are, by the programs and D230), it is today's two constraint lists plus two link rows per free diagonal:

x_last - x_first = s L,   y_last - y_first = s_v (L - 1).

That is a linear program. A diagonal's column has entries in both axes' rows, so the matrix is not a network matrix and the LP does not guarantee an integer optimum (INFERRED; measure it). With integer tiles it is a small ILP.

**Budget.** The seed is made once per spec on the main thread (junction_gui.cpp:1201-1209), so a joint solve does not fall under the per-offset 100 ms; the hover keeps its per-axis solve. The seed's real cost multiplier is the cut search: one placement per cut (pipe:132-184), and 4628 cuts for LL_RR x LL_RR (RECON-s3-probe §0.3).

**An interim without a new solver.** Inside the seed rounds, choose diagonal sizes in X by a minimum-cost tension:
- the objective is the hand-signed diagonal extents, each weighted by the row it costs, plus the width;
- it is the dual of min-cost flow, so it stays a network LP with integer optima.

It removes the overshoot of reason 1, but not the class of reason 2.

### The engineer's direction: every piece is a direction times a length
**1. Does it remove the coupling? No, but it removes the iteration.** A 45-degree piece moves both coordinates, so its length appears in both axes' closure equations. What goes is the readback, the alternation and the six special sizing paths, because every length is an unknown of one system. Class E goes with them.

What remains shows up differently. A loop whose diagonals add the same amount to both axes is feasible only if its constants close. Example (INFERRED, the RECON-s3-probe §1b shape): the joiner Corner e8 spans the band depth, while e5 and e14 consume that depth and move sideways by about as much. A joint solve reports such a shape as infeasible, with a certificate, instead of diverging. It does not make the shape place. That problem moves to the programs (for example, a run before the Corner, or the split one band higher), which is where it belongs.

It does nothing for A, B, C and G. It needs them done first, because a segment model takes every node's directions and the band order as input.

**2. What kind of problem.** With directions and the band order fixed, it is a linear program over lengths with coefficients ±1:
- With axis runs only, it is orthogonal compaction (Tamassia): two independent min-cost flows, which is today's per-axis system.
- With diagonals it is an ILP; the matrix is not totally unimodular in general.
- Optional bends add binaries: a length of 0 means no curves, so the curve-gap rows apply only when the bend exists.
- Occupancy is linear only with the order fixed.
- Terrain domains (blocked tiles) are not linear; they stay in the hover's propagator.

Size and speed:
- LL_RR x L_R has 121 points and 446 relations (RECON-llrr-placement-fail §1), which gives about 250 variables and 900 rows. LL_RR x LL_RR is perhaps three to four times that (INFERRED).
- A sparse LP/MIP solver takes milliseconds to tens of milliseconds at this size. A hand-written dense simplex would be far slower (INFERRED).
- The tree has no LP solver today (CMakeLists.txt `find_package` list).

**3. How it relates to the current model.** It is the standard topology, shape, metrics split:

| Stage | Today | Note |
|---|---|---|
| Topology | the graph | |
| Embedding | order and structure | the band order is missing |
| Shape | the programs | a Corner, S-bend or Fan is two 45-degree turns around one diagonal segment; the runs between tokens are axis segments; Over is a straight span |
| Metrics | placement | |

The Elem chains with first and last points are the point form of the same system, and lengths and points give the same LP. What matters is one system for both axes, not the choice of unknowns. In practice: keep the points and relations, and add one length per bend. With every length fixed, it splits exactly into today's two systems.

Path-sum rules then become plain sums of lengths: curve gaps across X and Y runs, prio coverage, the waiting room, and the later ramp rule. Today they need the split into an X form and a Y form (jp:720, 1619-1620).

**4. Can T and LL_RR x L_R stay identical? Yes, under one condition.** The joint solve must only choose `JunctionPlacementChoices` (m, corner spans and port offset; junction_placement.h:38-43), and positions must still come from the fixed path, which is what the hover and the `[identity]` sweeps consume.
- Identity then reduces to the joint solve choosing today's choices for both topologies. That holds if each settled size today is the least the relations allow; test that first.
- If the joint optimum is smaller, the seeds change by a rule and need a re-pin, as HH2 did. That is the engineer's call.

## Q4. The smallest change that removes the classes
1. **Node shapes and edge ends as program output (removes A, B, C).**
   - Per node: kind; stack item or band; heading in and out; the straight edge; the diverging edge with its hand; its footprint parts.
   - Per edge end: its heading, and the token (or none) whose end is the node's point.
   - Split `index` into an item field and an edge field.
   - Placement reads these. `node_item`, jp:273-277, jp:246-252 and the inference in `compute_node_tracks` go, and one chain builder serves rows and bands.
   - Migration: compute the old inference and the new data side by side. They must agree on every node and edge end of T and LL_RR x L_R, which gives identity by construction. Their disagreements on the three failing topologies are the A/B/C bugs, listed in one run instead of one per stop.
   - Cost: moderate. The programs already compute all of it (jt:1082-1091, 1152-1161).
2. **The band order (removes G).**
   - The order sweep emits the left-to-right order of edges and nodes per band slice.
   - Occupancy and the bridge-head hug use it instead of the least solution's order.
   - Test: it equals the flat order on T and LL_RR x L_R at every row.
   - It is needed before step 3, because a joint optimum is not a least solution and would otherwise change today's flat-order relations.
3. **Seed sizes from one joint solve (removes E and the forced part of D).**
   - The seed outputs `JunctionPlacementChoices`. Every bend length is a variable, at least its rule floor.
   - One declared objective replaces the least-x readback, `best_offset` and the planned S4 scan, and the rounds go.
   - A bridge over a diagonal may stand anywhere along it (link rows). The hover is unchanged.
   - Interim, if the engineer rejects a solver dependency: minimum-cost tension in X inside the rounds.
4. **Relax the smallest-shape pins to floors once step 3 exists:**
   - the item-less horizontal S-bend of one row (jp:1510);
   - the bridge's exact rows (jp:1584-1585);
   - the Corner of exactly k (jp:1486-1488).
5. **Reporting.** Land D234(3), the default cut's error, on its own. Try further cuts only when the default cut fails on a hug or series relation.

**What gets simpler.** Placement loses its inference code, the rounds, the fan scan and five of the six sizing paths, and every relation reads stated data.

**What it costs.**
- The programs emit more.
- A solver dependency, or a hand-written LP/ILP.
- A joint failure explains itself as a combination of an X cycle and a Y cycle, which is harder to print than one chain.

**Test first:**
- (a) The side-by-side oracle of step 1 on T and LL_RR x L_R.
- (b) Whether today's choices for T and LL_RR x L_R are minimal. Place through the fixed path with each m lowered by one; each must contradict.
- (c) An offline prototype. Dump the constraint lists and the link rows for the five topologies and solve them as an LP. Record feasibility, fractional lengths and time.

**On the held work:**
- Land D234(3) alone.
- Do not land D234(1) and (2) as more inference rules; they belong in step 1, as stated data.
- Do not build S4 as a scan; it would be a third objective mechanism.

## Q5. Other fundamentals
- **A perturbation solver used for synthesis.** Placement was built as offsets from a flat layout (DESIGN-N3 §1), and §6 already says "the Y snap does not see X needs". It now synthesises from scratch. The flat T used to supply the orders, hands, shared points and sizes. The graph pipeline removed it, and the least solution now stands in for all four.
- **No declared objective.** There are four objectives in four places: D207 (the least solution), D215 (lean toward the feature), DESIGN-G2 §G2.5 (minimal band growth) and `best_offset` (minimax fan).
- **Zero-size bends are guarded at each call site** (jp:500, 517, 778, 792, 974, 1058), not where the bend is defined. jp:783 and jp:2284 (`offs.back()`) have no guard (INFERRED reachable).
- **A horizontal band or joiner S-bend has X extent 0 in the relations** (jp:1511), while its raster spans columns (jp:514-517). With 2 or more rows, the next element's run floor starts inside the S-bend's tiles, and tiles of the same edge are never checked against each other (jp:1130). This is latent (INFERRED): today's S-bends move 1 row.
- **The cut search** enumerates around a solver that fails identically in every cut (RECON-s3-probe §0.3) and reports the last error (pipe:164, 192).

## Ranked recommendations
1. Step 1: node shapes and edge ends as data, with the side-by-side oracle.
2. Step 2: the band order from the order sweep.
3. Step 3: seed sizes from one joint solve. Before it, the offline LP prototype and the minimality test.
4. D234(3) now; hold D234(1)/(2) and S4.
5. The step 4 relaxations, then DB1b on the new data.

## Open questions for the engineer
1. What does the seed minimise: width, height, area, track length or bend count? A joint solve needs one named, because D207's "least solution" stops being an objective once diagonals are free.
2. May the fork take an LP/MIP library (for example HiGHS, MIT licence)? Or must the solver be hand-written, or stay as rounds with minimum-cost tension as an interim?
3. If the joint solve gives a smaller seed for T or LL_RR x L_R, is a re-pin acceptable?
4. Is the band order a rule (from the sweep) or a choice for the cost search?
5. When a shape cannot close, which edit comes first: a run before a Corner (a new program), or the node one band higher?
