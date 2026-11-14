# DESIGN-G4: the static placement model over the arrangement (at ea79529c24)

Design pass for step 2 of REVIEW-joint-loop Q6 (D254). Read-only on code. Probes ran in worktree ng1 (branch wt-g4 at ea79529c24, Release `build-rel`), were reverted (ng1 clean), and are kept as `g4_probes.patch` with their outputs `p1_out.txt` .. `p5b.log` in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/g4/`.

Abbreviations: jp = `src/junction/placement/junction_placement.cpp`, jj = `.../placement/junction_joint.cpp`, ph = `.../placement/junction_placement.h`, topo = `src/junction/topology/junction_topology.cpp`, pipe = `src/junction/junction_pipeline.cpp`, hover = `src/junction/tool/junction_hover.cpp`. INFERRED marks a conclusion I reasoned to but did not read or run. LL² = LL_RR x LL_RR, LLL³ = LLL_RRR x LLL_RRR.

## Summary

**The answer to the hard question.** Which tiles share a row is fixed by three different things, and the model treats each with its own static family:
1. **Item rows are tied.** Every stack item is one row variable; every piece on it (runs, heads, node tiles, prio footprint, the column of each band edge crossing it) is on that row in every layout. Occupancy there is the arrangement's row order with plain separation rows, no binary. The hug is a special case of it.
2. **W/E fans** (row-edge S-bends between port rows and item rows) are separated at the rows of their four ends, with the side of each fan (σ) a binary.
3. **Bands** are where rows are free. Band edges are y-monotone chains of pieces. Each bend moves toward the partner of the event it serves (the split's sibling, the merge's other leg, the crossing partner), and that partner is the immediate neighbour on that side. So between two neighbouring band edges the per-row gap is smallest at a shared event (the node or crossing shape), at a boundary event of either edge, or between their run columns. Every such check is a conjunction of linear rows, using each edge's column at that row (P3 generalised). The only true disjunction is the y-order of two *incomparable* events whose items are neighbours. That order is one binary per such pair.

**What the measurements say** (§3 has the detail):
- **(a) Whole-extent separation cuts all three seeds.** At the seeds, 4 / 26 / 14 row-sharing piece pairs are ordered on every shared row but have overlapping x-extents (P1).
- **(b) One binary per pair is exact but too large.** Per piece pair that the band order does not prove disjoint: 1074 binaries on LL² and 5708 on LLL³. Per slice (segment pair): 138 and 576, and with extent separation inside a slice it cuts the seeds as (a) does (P2).
- **(c) Parallel bends are not a family of their own.** They are the gap-0 entry of the gap table. At the LL_RR x L_R seed, e8's Converge and e11's joiner Corner lie on one diagonal and share tiles as half pieces (P1).
- **(d) Tied rows are adopted for item rows and rejected for bands.** "One row per band layer" is broken at 2 of 3 seeds, and the layers are not strictly ordered at LL_RR x L_R (P3).
- **(e) Recommended: profile rows plus event-order binaries.** The binaries are 21 (LL²) and 139 (LLL³) neighbouring incomparable event pairs (P4), plus the piece-at-row choices, the zero-size bends and the fan sides. That is about 100 binaries on LL² and about 400 on LLL³ (INFERRED totals, §5).

**The arrangement must also be extended.** Its band neighbour pairs must cover *every* linear extension of D255(2)'s order, not only the sweep's line states. Pairs that occur only in other extensions: T 1, LL_RR x L_R 4, L_R x LL_RR 4, LL² 14, LLL³ 49. LL_RR x L_R's seed realises one of them: x3/23 lies south of n4, so e4 and e23 are neighbours there, although they never share a sweep state (P3, P4). Replaying 400 random extensions per band never broke adjacency at a swap (0 invalid, P4).

**REVIEW F9 is half wrong: x and y must stay integer.** With continuous coordinates, today's joint finds half-tile optima: T 27300 → 27200, LL_RR x L_R 86350 → 85850, L_R x LL_RR 71050 → 70150 (P5). The curve counters are integer, so 2h·Δ can use half tiles. A box of 300 with M = 1000 leaves every objective unchanged and the seed times at 10-94 ms (P5b). Tight M stays; continuous x and y is dropped.

**D256 (square crossings).** The programs emit Corner(hand), Under/Above, Corner(-hand) where today they emit SBend, Under. Placement replaces "bridge over S-bend" with "bridge over a horizontal run", which is static and needs no t or w. Every curve rule reaches the Corners through the existing counters.

**Hover.** It is still a fixed-length difference system: Y once, then X per offset. It reads the seed's L, O, σ, z, event-order and piece choices. It has no detection.

**Sub-steps:** 2a1 square crossing; 2a2 retire over-S-bend; 2b1 ARR3 (arrangement gains); 2b2a item-row and fan families (fold F8); 2b2b band families; 2b3 one MILP per cut; 2b4 hover and deletion; 2b5 delete the rounds.

**Decision cards (§9):** DC1 "When the largest seeds are slow"; DC2 "Consecutive square crossings on one band edge".

## 0. Evidence (ng1 at ea79529c24, Release; throwaway, reverted)

- **P1, occupancy census at the seeds** (`p1_out.txt`). Unrelated rendered pieces that share a tile row: T 96, LL_RR x L_R 583, L_R x LL_RR 330.
  - Extent-separated: 92%, 95% and 96%.
  - Staggered (ordered on every shared row, x-extents overlap): 4, 26 and 14.
  - No pair is reversed between rows. The only "inconsistent" pairs are a crosser between its two heads (the hug), and parallel pieces sharing tiles as halves (fans e0/e1, e19/e20; e8/e11).
- **P2, sizes from graph, order and programs, with D256 simulated** (`p2_out.txt`).
  - Square crossings: T 1, LL_RR x L_R 1, L_R x LL_RR 3 (matches D256), LL² 9, LLL³ 33.
  - Unders of doubled bridges (candidates): T 4 (14), LL_RR x L_R 20 (60), L_R x LL_RR 8 (22), LL² 40 (112), LLL³ 144 (434).
  - Band edge x item-row crossings: LL² 60, LLL³ 216.
  - Consecutive same-hand single crossings on one edge: LL² 1, LLL_RRR x LL_RR 2, LLL³ 10.
- **P3, band events against level rules at the seeds** (`p3_out.txt`).
  - Rule L1, every event of a layer on one row: broken in 0/2, 1/4 and 3/5 layers (T, LL_RR x L_R, L_R x LL_RR).
  - Rule L2, layers strictly ordered: broken at LL_RR x L_R, where n11 (layer 3, y16) lies south of n4 (layer 2, y15).
  - Rule L3, the sweep order as north-or-equal: broken 0/2, 2/5 and 3/9.
- **P4, band adjacency over linear extensions** (`p4_out.txt`). Neighbouring incomparable event pairs: T 1, LL_RR x L_R 7, L_R x LL_RR 4, L_R x L_RR 2, LL² 21, LLL_RRR x LL_RR 83, LLL³ 139. Extension-only neighbour pairs as in the summary.
- **P5 / P5b, today's joint at the seeds' choices** (`p5.log`, `p5b.log`).
  - Seed times: T 10-16 ms, LL_RR x L_R 89-120 ms, L_R x LL_RR 68-281 ms.
  - LL² at all floors: infeasible in 94-119 ms. Without the over-S-bend rows (no occupancy is recorded there): optimal 224750 in 0.67-0.95 s (integer coordinates).
  - The continuous runs are in the summary.

## 1. Inputs

Graph, order, structure, programs (with D256), and the arrangement (ARR1, th:282-294).

**What ARR3 must add** (2b1). It is computed in `ArrangeJunctionTrackGraph` (topo:1332) from the graph, the order and the programs only:
- **(A1) Event replacements.** For each band, its start state and, per event, the contiguous block it removes and the block it inserts. The sweep already makes them: the swap at topo:594-595, the gather at 665-690, the expansion at 693-720. Emitting them directly replaces ARR2's per-state lines.
- **(A2) Neighbour pairs over all linear extensions** of the band's event order (D255(2) transitively closed).
  - Each pair is (band, west edge, east edge, phase relative to their own crossing).
  - It carries the events where the pair's coexistence begins and ends.
  - It is flagged *extension-only* when the sweep's own order never makes it.
  - Enumeration: downsets memoised by their alive set. Oracle: equal to brute force over all linear extensions on the three seeds' bands. If LLL³ is slow, a 2-SAT test per pair. INFERRED: bands of up to about 30 events are fine.
- **(A3) Neighbouring incomparable event pairs**, one event-order binary each (P4 counts).
- **(A4) Segments per band edge.** Its boundary events in travel order: band nodes, band crossings, row crossings (Under pairs of doubled bridges), node and port ends. Each bend in a segment gets its role toward its event: it approaches the event, or it leaves it, and its partner is a stated edge.
- **(A5) Item-row occupant sequences, west to east.** These are ARR1's row pairs refined to pieces. A node contributes its block: the node tile plus the row-edge pieces on that item next to it, in chain order. A crossing edge contributes its column at that row. Heads bound their crossers, and the prio footprint is one interval.
  - This settles D255's open items. A band edge incident to the row's nodes is part of the node's block (ARR0 B). A port stub of another row belongs to that row's fan (A6) (ARR0 A).
- **(A6) Row crossings and fans.** Per (band edge, row it crosses): the item rows of that row, and the D255(3) candidates extended from the head rows to every item row of the row (approach, prio, copy). Per W/E side: the fan pairs of adjacent rows.

## 2. Every family of today's build(), mapped

| family (jp lines) | G4 |
|---|---|
| stack origin and rows, band gap, S edge, S port row (1523-1550); port columns, S spacing (1552-1565); row nodes (1567-1575); waiting room (1577-1602) | kept, static |
| prio element pins, coverage, floors (1609-1616, 1660-1663, 1799-1803) | kept, static |
| prio footprint in occupancy (1323-1338, footprint first at 1349-1350) | rewritten: one interval occupant of its item row and the lane row in A5's order |
| doubled leg: row items, stagger, series heads, lane merge (1617-1658) | kept; lane split run 1647 exact → floor (F8 patch, folded in 2b2a); the stagger stays exact (D118, topology CLAUDE.md) |
| hug inequalities (1665-1696) and the "Z row" refusal (1692) | rewritten: item-row order (O1) with the crosser's column at the head row from the piece choice; the refusal is deleted |
| hug pins at the flat order (1427-1443) | deleted from the MILP (already dropped, jj:263); in the hover, static equalities to the first and last crosser of the row order at the seed's piece choices (§6) |
| fan size, W/E (1700-1714; link jj:359-369) | rewritten: L = σ(O + n - y_item), σ a binary per fan (with O, R3d not merged: jj:446 copies the recorded offset), empty-fan rows conditional on z |
| Corner fixed_k; joiner_div Corner (1715-1739; jj:352-354) | kept |
| S-bend sizes, vertical free and rows-sized (1740-1768; jj:339-358) | kept, links unchanged |
| chain runs and floors (1773-1818) | kept |
| bridge over S-bend (1820-1870; jj:234-255, 287-296) | deleted for every band crossing (D256); replaced by "bridge over a horizontal run" (§4) |
| curve gaps (1892-1959; counters jj:91-175) | kept as counters, built from the programs' routes; the zero-size relaxation M becomes min_gap (§5) |
| occupancy (1216-1424) | rewritten: O1 item rows, O2 fans, O3 band profile rows (§3); the flat sort, ties and groups go |
| clearance X (1478-1497) | rewritten: a single bridge's deck counts as an occupant for other bridges' heads in O3, side from A2 |
| clearance Y (detect_y 2326-2347, rows 1877-1884) | rewritten: a band edge's single-bridge head lies in its band, so its row side against a doubled bridge on an item row is the band's side (static, INFERRED implied by band gap; test in 2b2b); "side 0, both sides" (2338-2342) goes |
| detect, plan_clearance, flat_solve (1500-1505, 2350-2377, 1188-1202), at-mode (110-114, 1961-1970, 2400-2463) | deleted |
| terrain facts (1977-2283), box bound (2285-2294), D215 push (534-583) | kept: hover facts, box, canonical point |
| final legality (2528-2532) | test oracle only; acceptance keeps CheckJunctionRules |
| port fans tied to the rows (R3b), port offset (D252), series bridges (G2c-1) | R3b and G2c-1 are static as above; O is an integer variable (from the parked R3d patch, without its loop) |

## 3. Occupancy

**Options against the brief's criteria** (binaries on LL² / LLL³; seed: does it stay feasible):

| option | seeds | binaries | time |
|---|---|---|---|
| (a) whole piece extents | cuts all three (staggered pairs 4 / 26 / 14, P1) | 0 | about today's |
| (b) per piece pair, x-separated or y-disjoint | keeps (exact) | 1074 / 5708 (P2) | INFERRED: minutes or worse (REVIEW Q2(i) scale) |
| (b) per slice (segment pair), extents | cuts the seeds as (a) | 138 / 576 | INFERRED: seconds |
| (c) parallel same-shape groups | not a separation model: fans differ in L (e0 [17..19]x[1..4], e1 [17..18]x[3..5]); folded into the gap table | - | - |
| (d) tied rows | item rows: keeps (adopted as O1); band layers as levels: cuts LL_RR x L_R and L_R x LL_RR (P3) | 0 in bands | about today's |
| **(e) profile rows + event order** | keeps (INFERRED from P1: no pair reverses between rows; oracle in 2b2) | 21 / 139 event pairs, plus §5's | INFERRED: 1-10 s on LL², 10 s or more on LLL³ |

**O1, item rows.**
- On each stack item row, the occupants of A5 west to east are separated by `east(a) + g(a, b) <= west(b)`.
- Each end is affine in a point, L and the item row: a tile form (P2), a run end, or a crosser's column at the row.
- The crosser's column comes from the piece choice of A6: a run gives its column; a bend tail or head gives P3 generalised, `x_first + s (y_item - y_first) + c(L)`. It is linear, and conditional on the choice.
- The hug is these rows with the heads as occupants and the crossers strictly between them. The cost pulls the heads tight.

**O2, W/E fans.** Adjacent fans of one side are evaluated at the rows of their four ends (port rows O + n, item rows). Each fan's column there is its diagonal line or its end column, as in O3. Their order is fixed by σ and the notation. Count: at most 2 per row and side.

**O3, bands.** For each neighbour pair (a west of b) of A2, conditional on the event-order binaries that make the pair coexist:
- (i) **Run order.** `c_a + g <= c_b` for each pair of their segments that coexist. Here c is the segment's run column, or for a run-less segment, the bend end facing the other edge.
- (ii) **Boundary rows.** At each boundary event of a or b inside their common range, `A(rho) + g <= B(rho)`.
  - A is the run column, plus the line through rho of every bend of a that approaches a partner other than b. Such a line extended beyond the bend's far end moves away from b, so the row is harmless where the bend does not reach. B is the mirror.
  - Each is a conjunction of linear rows. Rows that mix x and y are new in the relation record (ph:48-60 is single-axis); they get y terms.
- (iii) **Shared events.** The node shape and the crossing shape are fixed tile offsets, or P2 forms for an S-bend.
- **Why this is complete** (proof sketch, INFERRED; the oracle is CheckJunctionRules on every solved layout plus 2b2's seed check):
  - A bend moves toward its event's partner, and at the event's row that partner is the immediate neighbour on that side (merge legs and crossing partners are adjacent items, topo:589, 622-631). So facing a non-partner, a segment never goes past its run column, except for bends approaching a third edge p.
  - p appears between a and b only through an event of b (its removal or arrival touches a neighbour, which is why P4's replays were all valid). That event is a boundary in (ii).
  - Two bends approaching a shared event move monotonically toward it, so their gap is smallest at the event's shape (T: e3's crossing bend and e8's Converge share row 9 at n4).
  - Worked example, T after D256: pair (e5 west, e3 east) north of x3/6. At y(x3/6), e5's joiner Corner column `x(n1) - 1 - (y(n1) - y)` plus g must be at most e3's column after the crossing (seed: about 21 against 25). The order is decided, because x3/6 touches e6, which n1 touched.
- **The gap table g.** It covers (piece kind, half, side) pairs and is built from `JunctionTracksMayShareTile` and the edge-reach test today's touch gap uses (jp:1359). It is 0 for complementary halves, which is the case of parallel diagonals sharing tiles. Test first in 2b2a.

## 4. The square crossing (D256)

**Programs.** At topo:1215-1217, `SBend(hand)` and `Under/Above(x)` become `Corner(hand)`, `Under/Above(x)`, `Corner(-hand)`. The hand is the same S-side slot rule. Edge ends and node shapes are unchanged: the Corners serve no node. (amended, D260: the programs emit both forms, `SBend, Corner, Under, Corner`, all role Cross; edge ends and node shapes stay unchanged only once topo:1266 and jp:282 stop reading position, §11.1.)
- Topology CLAUDE.md: "the edge under a single bridge crosses square" replaces DESIGN-G1 §10.7 F2. (amended, D260: "by an S-bend or square, as chosen; consecutive same-hand square crossings share one run".)

**Placement chain.** It already handles this: Corner elements of fixed k (jp:1734-1738), a horizontal run between them through `run()` (jp:1817), and the Under token adds no element (jp:294-296). The family "bridge over a horizontal run" is static and has no binary (amended, D260: it is guarded by the form binary q, §11.2). For the Heads element of edge e over x, with x's Corner1 and Corner2 around its Under token, and the run eastward (mirrored for westward):
- `x(Corner1.last) + 1 <= x(heads) <= x(Corner2.first) - 1`: the bridge's column is strictly inside the run, so the bridge passes over plain track.
- `y(head_north) + 1 <= y_run <= y(head_south) - 1`, with `y_run = y(Corner1.last) = y(Corner2.first)`: the run's row is strictly between the heads.
- The heads against the Corners' tiles use the gap table at this shared event (O3 iii).
- The run's floor is the chain run (JunctionRunFloor default 2), which gives exactly one run tile under the bridge. The curve rules (JunctionCurveGaps, CL from the train, no 90-degree bend) reach both Corners through the counters, so the run takes whatever length they need (D256).
- The level solve sees axis track under the deck (no new ramp rule). Render finds the run's tiles beneath the heads as today (jp:2669-2683).

**What retires:** (amended, D260: nothing below retires except the "no crossed sbend" refusal; t, w, the over-S-bend rows and their record are the S-bend form, §11.6.)
- G3.4a-2's t: `choices.over` (ph:41), `JunctionSystemOver` (ph:152-162), T (jj:237-255).
- D248's before/after: `choices.over_after` (ph:42), w (jj:242, 246-252, 287-296).
- The over-S-bend rows and the "no crossed sbend" refusal: jp:1820-1870.
- The placement CLAUDE.md lines on a single bridge over an S-bend.
- `JunctionSBendColumnForm` stays only as P3 for profile rows, generalised to Corners (test first).

**Re-pins (D235, listed in 2a1):** T e3 under e6; LL_RR x L_R e23 under e3; L_R x LL_RR's three crossings. (amended, D260: none; the square form does not seed at any of these five crossings and the S-bend form stays, §11.0.) Without the over-S-bend rows, today's joint gives T 26050 and LL_RR x L_R 86200 (P5). The square form's numbers are measured, not predicted.

## 5. Variables, binaries, tight M

**Variables:**
- x and y are integer (P5: continuous coordinates admit half-tile optima), with box bounds `0 <= x <= X`, `0 <= y <= Y`.
- X and Y are the per-axis least-solution box of jp:517-519, evaluated at L_max: the canonical point is a least solution, so it lies inside. The cost argument that the MILP optimum lies inside it too is INFERRED, so the test is "box-bounded equals unbounded" on the seeds; P5b already found box 300 neutral.
- L is integer, with its floor and L_max (the box side it spans).
- O is integer, with lower bound `-min(notation)` (the box's first row).
- The binaries:

| binaries | LL² | LLL³ |
|---|---|---|
| event order (A3) | 21 | 139 |
| piece at an item row (A6; "run after" never seen, D255 toggle) | about 48 (INFERRED: Under candidates beyond the run, 32, x 60/40 item rows per head row) | about 220 (146 x 216/144) |
| zero-size z (port fans, rows S-bends, S fans) | 24 (P5) | about 35 (INFERRED: 23 tokens x 1.5 for copies) |
| fan side σ | 8 | 12 |
| total | about 101 | about 406 |

The seeds: T about 1 + 9 + 8 + 4; LL_RR x L_R about 7 + 30 + 18 + 8; L_R x LL_RR about 4 + 12 + 12 + 4 (INFERRED).

**Rows.** Today's static rows (E5: 1542 / 4219), plus O1 (about 15 per item row), plus O3 (about 8 per neighbour pair): about 2600 on LL² and about 7600 on LLL³ (INFERRED).

**Tight M.** One helper computes it, never a constant BIG: `M = rhs - min over the box of the row's terms = rhs + sum over terms of |coef| x (ub - lb)`. Per family:
- Event order, `y(E1) <= y(E2)` by δ: M = Y.
- O3 rows conditional on coexistence: M = g + X + |s| Y (the y terms of a line).
- Piece-at-row y rows (the item row within the bend's rows): M = Y + L_max. The x rows: M = g + X + Y.
- Zero-size: `L <= L_max z`, `L >= z`. The curve-gap counter relaxed by `min_gap (1 - z)`: M = min_gap is exact, because units along a route never decrease (jj:91-175). Today it is 1e5 (jj:26, 162-171).
- Fan σ: the two rows of `L = σ(O + n - y_item)`: M = 2Y. Empty fan on its port: M = X.
- D256 and every unconditional row: none.

## 6. The hover

- **It stays the fixed-length difference system:** Y once (MakeJunctionPlacementShared, jp:2759-2791), then X per offset with the terrain facts and the near reject, unchanged (hover:32, 85, 106).
- **It reads `seed.placement.choices`** with: L (m, corner and the sized bends), O, σ, z, the event-order binaries and the piece choices.
- **Every row reduces to two points of one axis:**
  - At fixed binaries every row is single-axis, except O3's lines and O1's crosser columns, whose y terms become constants once Y is solved.
  - Two families become equalities so that flat ground reproduces the seed (G3.6 F2):
    - the hug: west head = first crosser's chosen column - 1, east head = last crosser's + 1;
    - a single bridge's heads: the run row -/+ the seed's offset.
- **It gets no detection.** add_occupancy, hug_heads, add_clearance_x, detect_y and the flat solve are gone. The legality check (jp:2528-2532) is test-only.
- **Budget:** the X system grows from about 1000 to about 2500 rows on LL²; INFERRED within 100 ms. [hoverplan] measures it in 2b4.

## 7. What is deleted, in order

1. **2a2.** The over-S-bend family and its record, joint and choices parts (§4).
2. **2b3.** The D250 fallback (pipe:200-216) and `JunctionFloorChoices` as a start (jj:574-592). The cut search runs one MILP per cut, ranked by cost (§G3.6 F10).
3. **2b4.**
   - `SolveJunctionJointLoop` (jj:486-572, joint.h:40-54) and its tests (junction_joint.cpp tests "a recording at a point detects there", "a clash's order...", "the loop ends settled...", "the joint loop on the hubs", the two over-S-bend sweeps).
   - `JunctionSystemPoint` (ph:87-92) and the at-mode.
   - Kept clashes and D249's order binaries: `JunctionSystemClash` (ph:136-150), jj:300-334, the record's clash parts (jp:442-446, 484-492, 1358-1366).
   - detect, hug_heads, add_clearance_x, add_occupancy's detection, detect_y, plan_clearance and flat_solve.
4. **2b5.** The rounds (jp:2471-2526), best_offset (2299-2314), and measure reading vy for sizes (634-664). Sizes, O and σ are inputs (G3.6 R6).

Retired rulings: D236, D240's "pairs from the joint solution", D243, D246, D248, D249, D250 (as D254 lists). D252 is kept, made real.

## 8. Sub-steps (outside-in, each with its red test)

Identity today: the joint objective at the seed's recording is T 27300 and LL_RR x L_R 86350 (tests/junction/placement/junction_joint.cpp:55-98). D235: every change is listed with its rule.

- **2a1, square crossing.** (amended, D260: 2a1 and 2a2 below are replaced by §11.6.)
  - Programs (topo:1215-1217) and the run family (§4), beside the old family for crossings that still have an S-bend: none after this step.
  - Red tests:
    - "a single-bridge band crossing is Corner, Under, Corner" (T, LL_RR x L_R, L_R x LL_RR);
    - "the bridge's column is strictly inside the run and the run's row strictly between the heads";
    - "LL² gets past 'no crossed sbend'" (the F5 probe).
  - Re-pins: T 1, LL_RR x L_R 1, L_R x LL_RR 3 crossings, each cell and objective listed. CheckJunctionRules clean.
- **2a2, retire t, w and overs.** Identity equal to 2a1. (amended, D260: t, w and overs stay; see §11.6.)
- **2b1, ARR3** (A1-A6). Red tests:
  - "every unrelated row-sharing pair at the three seeds is a row pair, a band neighbour pair, or separated through an edge present on that row" (P1 as the oracle);
  - "LL_RR x L_R emits the extension-only pair (e4, e23)";
  - "downset enumeration equals brute force on the seeds' bands".
  - LL² and LLL³ counts are logged; [topology] stays in budget. Identity unchanged (data only).
- **2b2a, item rows and fans in the rule source.**
  - O1 (with the hug and the prio footprint), O2, clearance, the gap table, F8 folded in.
  - Red tests:
    - "every O1/O2 row holds at the three seeds with the choices read off the seed";
    - "the gap table agrees with JunctionTracksMayShareTile on every kind pair";
    - "a Corner's column at a row is affine (P3 for Corners)".
  - Re-pin: F8 (the LL_RR level hash and the joint digests, STATUS).
- **2b2b, band families.** O3, the event-order rows, the D256 shape rows. Red tests:
  - "every O3 row holds at the three seeds with the event order read off the seed";
  - "dropping any O3 row lets CheckJunctionRules find an occupancy breach on some seed or LL²" (each row is needed).
- **2b3, one MILP per cut** (`SolveJunctionJoint` rebuilt on the rule list: integer x and y, the box, the M helper, O; spans of the static choices).
  - Red tests:
    - "T and LL_RR x L_R reproduce the 2a1/2b2a objectives or list each cell";
    - "L_R x LL_RR seeds and is CheckJunctionRules-clean";
    - "LL² seeds or names an irreducible set";
    - "the solve takes no point" (API);
    - "two solves give identical rows and points";
    - "box-bounded equals unbounded";
    - "the canonical point's flat cost equals the MILP objective".
  - Budgets in TESTS.md, set from the measurements (D251 style).
  - Expected re-pin from O: R3d measured 27100 / 85050 before D256.
- **2b4, hover and deletions** (§6, §7.3).
  - Red tests: "every hover row has two points of one axis"; "the hover on flat facts equals the seed".
  - The identity sweeps are re-pinned by listed rule; [hoverplan] stays in budget.
- **2b5, delete the rounds.** Identity equal to 2b4.

## 9. Decision cards

**DC1: When the largest seeds are slow.**
- **Context.** §5 puts LLL³ at about 400 binaries. D253 accepts seconds, and the seed is cached (D242). The time is unknown until 2b3 measures it (INFERRED: 10 s or more). The cut search multiplies it by the number of cuts.
- **Options:**
  - (1) optimal only, any time, measured and budgeted per hub;
  - (2) a deterministic work limit (a node budget at one thread and a fixed seed), which keeps the best layout found: deterministic, but not proven least cost;
  - (3) a band rule that removes the event-order binaries: layers as levels. P3 shows it would cut the LL_RR x L_R and L_R x LL_RR seeds, so layouts get taller bands.
- **Recommendation.** (1) until 2b3's numbers. Then (2) only for hubs over a stated budget. Never (3) without a corpus case.
- **Reversible:** yes.

**DC2: Consecutive square crossings on one band edge.**
- **Context.** D256 is worded per crossing. Where one band edge passes under two single bridges in a row, it gives Corner, Under, Corner, Corner, Under, Corner: a jog back to vertical between the bridges. Counts: LL² 1, LLL_RRR x LL_RR 2, LLL³ 10. All are the same hand, so one run could pass under both (P2).
- **Options:**
  - (1) as worded: a Corner pair per crossing, each run sized by the curve rules;
  - (2) one run under consecutive same-hand crossings: Corner, Under, Under, Corner, with every bridge's column strictly inside the run and the run's row strictly between each bridge's heads. That means fewer curves and rows, and it is closer to the coop habit of crossing a bundle with short bridges (RECON-junctionary-crossings, INFERRED).
- **Recommendation:** (2). It does not affect T, LL_RR x L_R or L_R x LL_RR.
- **Reversible:** yes (a program rule).

## 10. Risks (not decisions; each has a test above)

- **O3's completeness rests on a proof sketch.** The guard is CheckJunctionRules on every solved layout plus 2b2b's necessity test; a breach names a missing row family.
- **The canonical point may cost more than the MILP's point.** The least solution does not minimise runs in general. 2b3's cost test finds it. The remedy is more lengths as choices; the hover then keeps less freedom.
- **The downset enumeration on LLL³.** The fallback is a 2-SAT pair test (A2).

## 11. Amendment: the crossing form is a choice (D260)

At c23bae8d69 plus the 2a1 WIP (ng1, wt-2a1). The probes ran on top of the WIP in Release and were reverted: ng1's `git diff` equals `scratchpad/2a1/wip-stopped.patch` again. The probe patch (`probe_full_on_wip.patch`) and the outputs (`p2_small.txt`, `p3_ll2_group.txt`, `p4_ll2_each.txt`, `p5_ll2_loop.txt`, `p6_counts.txt`, `p6_why.txt`) are in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/g4a/`. The probe chose the form per crossing (0 S-bend, 1 square) inside the programs and fixed the token-0 misread (§11.1) so that the square form could be measured at all.

**Summary.**
- Programs emit both forms per crossing: `SBend(h)`, `Corner(h)`, `Under/Above(x)`, `Corner(-h)`, all with a new role Cross. `choices.form` picks one form per D258 group; the other form's bends have size 0 (no bend).
- Edge ends and joiner_div read token positions today. A square Corner at token 0 of a band split's straight leg is misread as the split's serving bend. Measured on L_R x LL_RR e11; LL² has three more such Corners (e5, e20, e26).
- The square run's length r is data for the hover. Otherwise a run that heads against the route's rows gives the route two horizontal headings, and its curve gaps are no longer two-point rows (measured: LL² crossings 5 and 6).
- **Measured.** T seeds with the S-bend form (27300, clean). No square choice seeds on T, LL_RR x L_R or L_R x LL_RR, so the seeds keep the S-bend form and their joint objectives (27300, 86350, 71050). LL² round 0 is infeasible all-S and all-square; with the choice it is Optimal at 268600 (form 100100000).
- One binary per D258 group. Binaries: T 25, LL_RR x L_R 64, L_R x LL_RR 38, LL² about 125, LLL³ about 489.
- Sub-steps: 2a1 both forms as data, then 2a2 the joint chooses. t, w and the over-S-bend family stay. No decision cards.

### 11.0 Evidence
- **Crossings, in emission order.** T: e4 under e8 (`JunctionTTopology`; e3 under e6 in the L_R x L_R hub). LL_RR x L_R: e23 under e3. L_R x LL_RR: e1 under e21, e11 under e10, e14 under e7. LL²: e1/e47, e3/e5, e3/e8 (the D258 group), e5/e9, e17/e14, e20/e23, e26/e25, e32/e31, e39/e43 (edge under bridge).
- **T, both T topologies.** S-bend form: seeds 41x14, CheckJunctionRules clean, joint at the seed 27300. Square: no seed ("chain run (e10) <- chain run (e4) <- Corner size (e4) <- bridge over run (e8, node 4) <- chain run (e8) <- chain run (e1) <- S port spacing"), and the D250 loop is infeasible too. This is geometry: the WIP's rows put the bridge column at least k + 1 = 3 columns from e4's port column, on the run's side (k = 2), and e8's port is 1-2 columns away.
- **LL_RR x L_R.** S-bend form: seeds 62x25, joint 86350. Square: no seed, and the round-0 joint is infeasible too ("S port spacing <- chain run (e2) <- chain run (e3) <- bridge over run (e3, node 23) <- chain run (e21) <- Corner size (e23)"): the T's geometry again.
- **L_R x LL_RR.**
  - 000: seeds 61x44, clean, joint 71050 (16 s).
  - The 7 other forms: no seed. The fixed path fails with "occupancy (e6) [e6 and e12 on row 20 or 26]" or "bridge middle clearance (e13) [the head of e7]", and the loop is infeasible.
  - The round-0 joint is feasible at all 8 forms, least 57400 at 100 against 63900 at 000. It is a relaxation, with occupancy only at its own flat point.
- **LL² round 0** (all floors, default cut).
  - All S: "S-bend size (e1) <- ... prio element (e19)". All square: infeasible (WIP red4).
  - Group-level enumeration, 256 forms. 192 do not record: crossing 5 or 6 square gives "path 6 (12) has two horizontal headings" (jp:992-1001).
  - Of the other 64, 24 are Optimal. Best 268600 at 100100000 (e1 and e5 square, the rest S-bend), 0.4-10 s per solve.
  - Per crossing, 512 forms: 48 Optimal, the same best 268600. The best mixed group is 269950.
  - The D250 loop from 100100000 is infeasible at round 1 ("X solve: ... bridge over S-bend").
- **Counts** (`p6_counts.txt`). "Refused" is alone-square crossings refused for two horizontal headings; it is a lower bound, because a Y failure can come first.

| hub | crossings | D258 groups | Corner at token 0 of a split's straight leg | S-bend after the Under (D248 w) | doubled Unders next to a crossing | refused |
|---|---|---|---|---|---|---|
| T | 1 | 1 | 0 | 0 | 2 | 0 |
| LL_RR x L_R | 1 | 1 | 0 | 0 | 0 | 0 |
| L_R x LL_RR | 3 | 3 | 1 | 1 | 2 | 0 |
| LL² | 9 | 8 | 3 | 4 | 12 | 2 |
| LLL_RRR x LL_RR | 14 | 12 | 5 | 7 | 18 | >= 4 |
| LLL³ | 33 | 23 | 12 | 18 | 42 | >= 11 |

- **A robustness bug found on the way.** When the X build refuses, the recording returns `pt_count` 0, measured on LL² at form 000001000. `SolveJunctionJoint` then crashed (SIGSEGV), INFERRED at `x[0]` (jj:187). The recording must fail loudly and name the refusal.

### 11.1 Programs
- **The tokens.** For each single-bridge crossing x of band edge e, with hand h from topo:1216's rule, the program emits `SBend(h, Cross)`, `Corner(h, Cross)`, `Under/Above(x)`, `Corner(-h, Cross)`. Cross is a new `Step::Role`, and it replaces role None on a crossing's S-bend.
  - A D258 group is these quadruples in sequence. In its square form, the inner Corner pairs and the inner S-bends are zero-size at one point of the run.
  - The inner Corners keep every S-bend on a vertical heading in both forms, so one chain with form-independent headings exists. An S-bend token on the run would be a horizontal S-bend, one row (jp:1765).
- **What stays well defined.** The token count does not depend on the form, so every choice stays per token. Edge ends and node shapes are unchanged, because Cross tokens serve no node.
  - That holds only once the readers stop using position. Today topo:1266 names `program.front()` as a split's end whenever it is a Corner, and jp:282 sets joiner_div for any Corner at t == 0 from a band split.
  - Counterexample: L_R x LL_RR e11 leaves band split n2 by its straight leg. Square, it fails "a bridge over a run that is not a run" (measured). LL² has three more such Corners (e5, e20, e26, counted); their failure is INFERRED.
  - Rule: an end token is a Diverge S-bend or a Corner that is not Cross, and joiner_div means "the start end names this Corner". This is the placement CLAUDE.md fact "infers none of them from token order".
- **The arrangement (ARR1, ARR3).** In both forms, the Cross tokens are the bends of the crossing event, and their partner is x (A4).
  - D255(3)'s "previous bend's tail" and "next bend's head" mean the previous or next *present* bend, so a doubled Under next to a crossing has one candidate per form.
  - Counterexample: on L_R x LL_RR e11, the token before the two doubled Unders is `Corner(-h)`, which has size 0 at the S seed.
- **Data.** These are read by the fixed path, the render, the route walk, the arrangement and the hover:
  - `choices.form[e][tok]` at a group's first Cross token: 0 S-bend, 1 square; empty means all S-bend, today's layouts;
  - `choices.run[e][tok]`: the square run's length r, read off the seed;
  - t and w as today (`choices.over`, `over_after`).
  - The render skips the Cross tokens of the unchosen form. Today it skips only an empty Fan (render:118).

### 11.2 Placement families under the binary
Each group g has one binary q_g (1 square). There is one chain over all tokens. The M values come from §5's helper.
- **Sizes.**
  - The S-bend uses the S fan's zero-size pattern (jj:205-211): `floor (1 - q) <= L_S <= L_max (1 - q)`, with the y link's z = 1 - q.
  - The outer Corners of a group have L = k q, and its inner Corners L = 0 in both forms (§11.3). Their size rows `x(last) - x(first) = s k q` are exact without M.
  - A bend of the unchosen form has length 0 and is no bend, so it adds no pieces, bits, occupancy or curves.
- **Runs.**
  - Same-line equalities are stated only between neighbours in the superset chain. They hold in both forms.
  - Each floor is stated between consecutive *present* elements, as `along(b.first - a.last) >= floor(a, b) * ind`, with ind = q or 1 - q. This is exact without M, because bands are y-monotone: at ind = 0 the row is already implied.
  - A run whose heading comes only from absent Corners is 0. That is the E/W run between zero Corners in the S-bend form (`|dx| <= X q`), and the vertical stubs between zero inner Corners of a shared run (`|dy| <= Y (1 - q)`).
- **The S-bend family** (jp:1820-1870, unchanged rows): t, and D248's w where an S-bend follows the Under. Each row is relaxed by M q, which replaces BIG (jj:240-255, 289-296).
  - "Before" and "after" mean the nearest S-bends of the S-bend form. jp:1830's `tok ± 1` test must skip the Cross Corners, which now sit between the S-bend and the Under.
- **The square family** (the WIP's rows): `x(C.last) + 1 <= x(heads) <= x(C'.first) - 1` (mirrored for runs heading west) and `y(north) + 1 <= y(C.last) <= y(south) - 1`, each relaxed by M (1 - q). In a group, the bridges keep their token order along the run.
- **Curve gaps, exact.**
  - The counters (jj:91-175) take any heading, and a zero-size bend has 0 units (jj:99-106).
  - JunctionCurveGaps depends on the number of curves between two curves. The S-bend form has 2 curves per crossing, the square 4 per group.
  - So each pair gets one row per combination of the forms of the groups on or between its two curves, while `min(2TL - 1, (c + 1) CL)` still varies. Each row is relaxed by min_gap times the Hamming distance from q. M = min_gap is exact, because units never decrease along a route.
  - At the test spec (TL 4, CL 5) only consecutive pairs vary. A pair spanning a crossing needs 7 in both forms.
  - Today's relaxation (jj:162-171) still counts vanished curves in j - i. That is sound but loose, so it is not reused here.

### 11.3 D258 runs under the choice
- **Recommendation: one binary per group.** Either every crossing of the group S-bends, or one run passes under all of them.
- **Why:**
  - D258 makes the group's single run *the* square form.
  - A mixed group is either a jog between two square crossings, which D258 rules out, or a square-plus-S-bend group, which no ruling covers.
  - Per crossing needs a share term per adjacent pair (`s = q_i AND q_i+1`, inner Corners k (q_i - s)).
  - Measured on LL² round 0: per crossing finds no better optimum (268600 both ways, best mixed 269950).
- **Toggle candidate:** per crossing. The tokens are the same; only the model changes. Groups: LL² 8 from 9 crossings, LLL³ 23 from 33.

### 11.4 The band model (§3)
- **Events.** The crossing is one swap event of (e, x) in both forms. The band order (D255(2)), A2's neighbour pairs and A3's event-order binaries do not depend on the form: the sweep never reads it.
- **Event y.** Each crossing has one integer variable rho. Square: rho = y(C.last), the run's row. S-bend form: rho = y(north head) + 1, the diagonal's north row at column t (the over rows put the heads one row beyond the diagonal, jp:1862-1866). These are two equalities with M = Y, and in both forms the north head < rho < the south head.
- **Inside a bend (ARR2).** In the S-bend form, e's S-bend straddles rho.
  - Above rho it approaches x, its partner.
  - Below rho it moves toward e's new neighbour beyond x. It ends on e's next run column, which O3(i) bounds against that neighbour.
  - At rho, O3(ii) already takes "the line through rho of every bend approaching a partner other than b", which is P3 at rho, affine in t and L.
  - Segments are cut at rows, not at tokens. ARR2's per-token slices are not used.
  - Square: the run is a horizontal piece on row rho, and its far end lies within e's next run column. O3(iii) gets one crossing shape per form, each guarded by q.
- **Binary count.**
  - Event order: unchanged.
  - New: q per group; w per S-bend-form site; one more piece candidate per doubled Under next to a crossing (`pi <= q` or `pi <= 1 - q`).
  - t, rho and r are integers.
- **Totals** (§5's own parts as INFERRED there):

| | §5 | + q | + w | + piece | total |
|---|---|---|---|---|---|
| T | 22 | 1 | 0 | 2 | 25 |
| LL_RR x L_R | 63 | 1 | 0 | 0 | 64 |
| L_R x LL_RR | 32 | 3 | 1 | 2 | 38 |
| LL² | ~101 | 8 | 4 | 12 | ~125 |
| LLL³ | ~406 | 23 | 18 | 42 | ~489 |

### 11.5 Fixed path and hover
- **Both read the form, t, w and r as data.** At a fixed form every guarded row is a constant row, and the zero-size elements are points tied by equalities.
- **Why r is data.**
  - The square run is an axis heading. When it heads against the route's rows, the route has two horizontal headings. jp:992-1001 refuses that (measured counts in §11.0), and a curve-gap pair across both stretches is a three-point row, which breaks §6's "two points of one axis".
  - With r fixed, Corner + run + Corner is a fixed span, so the route keeps one horizontal sense. sym_routes counts the span's units as a constant, and a pair separated only by vertical runs and fixed spans falls in the Y phase.
  - In the MILP r stays free, because the counters handle any heading.
- **The rounds' default** (until 2b5) is the S-bend form everywhere, with r at the floor 2 when square is given. The joint chooses by cost. D260's toggle candidate ("square only where the S-bend form is infeasible") stays a toggle.
- **Expansion (D257).** The form and r per crossing are small persisted data. A frozen crossing keeps its form, and a new crossing chooses one.

### 11.6 Revised sub-steps (replace §8's 2a1 and 2a2)
- **2a1, both forms in the programs; the form as data.**
  - Changes:
    - the tokens and role Cross (§11.1), and the end and joiner_div rule;
    - `choices.form` and `choices.run`;
    - the superset chain with zero-size unchosen bends, present-pair floors and zero runs (§11.2);
    - both families: the WIP's run family, and the over-S-bend family with before/after skipping Cross Corners;
    - render, route walk and ARR1 skip absent Cross tokens; sym_routes treats a square crossing as a fixed span;
    - the joint holds an absent Cross S-bend at 0;
    - the recording names an X-build refusal.
  - Red tests:
    - "programs: a single-bridge crossing carries both forms" (T, LL_RR x L_R, L_R x LL_RR, the LL² group);
    - "edge ends and joiner_div never name a Cross token" (L_R x LL_RR e11; LL² e5, e20, e26);
    - "LL² at form 100100000 records round 0 without failure and its joint is Optimal at 268600" (red, INFERRED: "a bridge over a run that is not a run" at e5);
    - "LL² crossing 5 alone square records" (red: "path 6 has two horizontal headings");
    - "ARR1 at the S seeds names no absent piece" (T e4, L_R x LL_RR e11).
  - Re-pins: layouts, level hashes and objectives none (the default is the S-bend form). Recording sizes and digests grow by the zero-size Cross elements; re-pin them under that rule (D235).
  - Restore the two over-S-bend tests that the WIP deleted.
- **2a2, the joint chooses the form.**
  - Changes: q per group; relations tagged (group, form) and guarded with the M helper, which also replaces BIG on the t and w rows; z = 1 - q and L = k q; the curve-gap combination rows; `choices.form` and `choices.run` out.
  - Caveat: the recorded clashes cover only the recorded form's pieces. A flip is checked by the loop's next recording, until 2b2b gives both forms static occupancy.
  - Why now: the per-form enumeration is an oracle for the guarded rows before 2b3 rebuilds the model.
  - Red tests:
    - "at every enumerated form with q fixed, the joint equals that form's recording" (exact rows);
    - "LL² round 0 with q free is Optimal at no more than 268600" (red: infeasible today);
    - "T and LL_RR x L_R keep the S-bend form at 27300 and 86350" (INFERRED: rules cut their square form, not occupancy).
  - Re-pins: none expected. L_R x LL_RR's joint at its seed is not pinned. Its round 0 preferred e1 square (57400 against 63900), so list it if it moves.
  - Retires: only the "no crossed sbend" refusal (jp:1833, unreachable once every crossing carries its S-bend) and BIG on the over rows. t, w, `JunctionSystemOver`, `choices.over` and `choices.over_after`, the over rows and `JunctionSBendColumnForm` stay. §7.1 is void, and §7.3 keeps the two over-S-bend tests.
- **Order:** 2a1 -> 2a2 -> 2b1 (ARR3 gains Cross bends, rho and per-form piece candidates) -> 2b2a -> 2b2b (both crossing shapes guarded; "every O3 row holds at the seeds and at LL² 100100000") -> 2b3 (q, t, w and r in the static MILP; "LL² seeds or names an irreducible set") -> 2b4 (the hover reads form, t, w and r, with r pinned) -> 2b5.

### 11.7 Decision cards
None is the engineer's. Recommendations, reversible and toggle candidates under D227:
- one form binary per D258 group (toggle: per crossing);
- r as hover data (forced by §6's two-point rows);
- the rounds default to the S-bend form.

## 12. Amendment: O3 over A4 segments (D273, D275; RECON-O3)

RECON-O3 measured §3's O3 at the seeds: every band-edge clash off the item rows is between neighbour pairs; (i) read with one run column per segment fails where a segment holds two runs or none, and holds everywhere with the column taken at the rows where the pair coexists; (ii) holds everywhere. This section fixes how 2b2b2 states O3 on the data 2b2b1a and 2b2b1b record.

### 12.1 Cuts and their rows
- A band edge's cuts (A4, `JunctionBandEdge`): Start, End, Crossing, Row, Free. Their rows: a band node end or crossing, its event row Y(E) (2b2b1b: a node's point row, a crossing's rho); a row-node end or a Row cut, an item row (outside every band, so ordered statically against every band event); an S port end, beyond band R; a Free cut, the free bend's two point rows (its near end bounds the segment before it, its far end the segment after).
- A segment of a in band B covers the rows between its two cuts' rows. With Free cuts every segment holds at most one vertical run (2b2b1a: LL² 4 free cuts).

### 12.2 Coexistence as order literals
- Two segments sa of a and sb of b in band B coexist when `lo(sa) <= hi(sb)` and `lo(sb) <= hi(sa)` (rows grow south; lo is the north cut's row). Each comparison between two cut rows is either fixed (same edge in travel order; an item-row or port cut against a band event; two events ordered by the layer closure of `band_pairs`) or a literal of an order binary: an A3 event order (`event_orders`) for two events, or a new free order for a free bend's end row against an event of the neighbour (made only where fixed order does not decide it).
- A comparison fixed false drops the pair; fixed true drops the literal. An O3 row holds under the conjunction of its literals (`JunctionStaticRow::orders` and the new free-order guards). Ties are allowed both ways (a row shared by two cuts is a coexisting row).

### 12.3 The rows (a west of b; g from `JunctionUnrelatedTileGap` of the two pieces' bits)
- **(i) Runs.** For each coexisting segment pair where both hold a vertical run: `x(run a) + g <= x(run b)`. Where one holds no run, its column is each of its bends' row forms (`JunctionBendRowForms`) evaluated at each of the pair's coexistence bound rows (the later of the two lo rows, the earlier of the two hi rows; which cut binds is one more literal where not fixed).
- **(ii) Boundary rows.** At each cut row R of a or b that lies inside the other's segment (literals as above): `A(R) + g <= B(R)`, with A(R) the run column of a's segment covering R and the row form at R of every bend of that segment whose partners do not include b (Free bends included); B the mirror. A cut of a itself has a's segments on both sides; each gives its own rows.
- **(iii) The pair's own crossing.** A pair that swaps at its own crossing coexists up to rho on each side as two phases (A2 lists both); no row at rho itself: the crossing families (over rows, run rows) hold it.
- Reason strings begin "O3 band ".

### 12.4 Oracles
- Every O3 row whose guards are as recorded holds at T, LL_RR x L_R and L_R x LL_RR.
- Coverage: every recorded clash at the seeds between pieces of two band edges on a non-item row (RECON-O3 Q1 class D bb: 15 / 31 / 118) names a pair that has an O3 row in that band.
- Counts of rows, literals and free orders are logged for LL² and both 3-lane hubs (arrangement and recording at the joint point; the rows need not hold there, D274).

### 12.5 Left to 2b2b3 and 2b2b4
Band pieces against W/E fans, nodes, row pieces and prio tiles in band rows (RECON-O3 finding 3), the square form's shapes, and clearance.
