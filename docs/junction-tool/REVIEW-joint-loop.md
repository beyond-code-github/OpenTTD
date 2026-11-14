# REVIEW-joint-loop: review of the joint loop and its rule source (at aad3c5f03e)

Read-only review against aad3c5f03e. Runs were made in worktree `.claude/ng1` (branch wt-review at aad3c5f03e, Release `build-rel`), using throwaway tests that have been reverted (ng1 is clean). Logs are in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/review/` (g3loop.log, dump.log, dump2.log, probe.log, probe2.log, count.log; the test sources are kept next to them as *_test.cpp).

Abbreviations:
- jp = src/junction/placement/junction_placement.cpp
- jj = src/junction/placement/junction_joint.cpp
- jm = src/junction/placement/junction_milp.cpp
- topo = src/junction/topology/junction_topology.cpp
- pipe = src/junction/junction_pipeline.cpp
- E1-E5 = the runs in "Evidence".

INFERRED marks a conclusion I reasoned to but did not read in the code or run.

## Verdict

**The joint MILP is the right core. The loop around it (record, then MILP, then the canonical point, then record again) is the wrong direction. More family-by-family fixes will not make it converge.** Three facts carry this.

1. **The loop adds rows that hold only for the arrangement seen at one point.** A loop of this kind ends at a correct answer only if every row it adds holds for every legal layout. The model is then a relaxation: a fixed point is optimal, and "infeasible" really means infeasible.
   - These rows fail that test:
     - occupancy: its west/east order, and the fact that both pieces stand on one tile row;
     - the hug: which crosser tiles lie on the head row;
     - clearance: its side and its row strictness;
     - the fans' empty or sized branch.
   - When such rows are kept, they cut off legal layouts (an infeasible model whose IIS is empty or misleading).
   - When they are derived again each round, the iteration can go back and forth (cycles).
   - The logs show both. The loop also detects at a point that mixes the previous round's columns with this round's rows and lengths (F2).
2. **The information that would make those rows static already exists, and placement throws it away.**
   - The order sweep emits every row's west-to-east order, every band's events with layers, and every edge's crossing order (topology.h:150-155). Placement reads only the row count and band R's layer-1 nodes (jp:120, 1539-1541).
   - The band chains drop the Under tokens (jp:294-296), so no relation ties a crosser to the rows it passes.
   - DESIGN-G3 itself says the three tile-pattern rules are linear "once ... an order (the sweep, G3.2)" is fixed (DESIGN-G3-vectors.md:37). G3.2 was step 4 of G3.5 (line 90) and was never built.
   - D236 chose lazy clashes first, with the sweep's order as the fallback "if a hub cycles or a pair's order flips" (DESIGN-G3-vectors.md:63). Both have now happened: T cycles under R3d (STATUS line 2), and D249 had to make clash orders binaries.
   - D249 also lets placement choose the order of two tracks. D216 lists order changes ("rows, crossings, node order") among the graph edits.
3. **Not every failure belongs to the loop.**
   - I replaced the recorded hug with a static one and imposed the programs' token order in Y (E3). LL_RR x LL_RR round 0 is still infeasible, through the same cycle as RECON-llrr-round0b.
   - The cause is a row budget. A band edge moves sideways only by a 45-degree S-bend, so e1 needs at least 26 rows in band 1, and the fixed port rows forbid that.
   - R3d removed the conflict only by moving the port offset from 25 to 50 (STATUS line 3), which doubles the band.
   - This is a program rule generalised from the T (F5). It is the engineer's call.

**What replaces it (Q6): arrangement first, then one static model, no loop.**
- Emit the arrangement as data; most of it already exists:
  - each row's west-to-east occupants;
  - each band's slices with their west-to-east items;
  - per crossing, the piece of the crosser that passes under each bridge copy.
- Write occupancy, the hug and clearance as static rows over that arrangement, affine in the lengths. The tile forms (P2) and P3 already exist.
- Keep only a few local discrete choices as explicit binaries:
  - the crosser's piece under each row-bridge copy (the T's own seed uses Corner tiles there, F4);
  - the flanking S-bend (D248);
  - empty or sized fans;
  - the port offset (D252), as an integer.
- Solve one MILP per bridge cut.
- An infeasible model is then exact, and its IIS names structure and program rules. That is the "fix is a program edit" route G3.3 always intended (DESIGN-G3-vectors.md:32).
- The hover keeps a fixed-length difference system and loses the fresh detection.

**What stays and what goes.**
- Stays:
  - the joint model and its flat-cost objective (D235), behind JunctionMilp/HiGHS (D237);
  - the affine tags and tile forms (R1, P2), and t over the diagonal (G3.4a-2);
  - port fans tied to the rows (R3b), and the offset variable from R3d (D252) without its loop;
  - band series (G2c-1), and TT3's rule check of every seed.
- Goes:
  - SolveJunctionJointLoop, the recording at a point, the kept clashes and D249's order binaries;
  - detect, detect_y, plan_clearance and the flat-solve order;
  - the all-floor start, and the D250 fallback.

## Findings

**F1 [wrong] The lazy rows are not valid inequalities, so the loop is neither a relaxation nor a restriction.**
- Evidence:
  - A clash is built per tile row, `by_row[y]` (jp:1239-1250), between neighbours in the flat x order (jp:1350, 1403-1421). The joint's clash row has no y term (jj:300-334), so the "same row" fact is held.
  - Clashes are kept forever (jj:507-515). One detected at a recording's own point is a fixed-order hard row (`choose = point_joint`, jj:510, 546).
  - The hug reads which crosser tiles have `z.y == vy[head]` (jp:1676-1690), and the build refuses with "Z row" when none does (jp:1692).
  - Clearance takes its side from the point (jp:1488, 2337).
- Counterexamples:
  - L_R x L_RR (E1): rounds 0-2 keep no clash (no X solve; points "none", "none", "joint"). Round 3 keeps 100 clashes, all chosen, at the round-2 joint point, and the joint is infeasible.
  - T under R3d: objective 26600, then 27550, then 53850, a cycle of period 2 (STATUS line 2).
- Fix: no fix exists inside the loop. Either model each such row as its full disjunction (Q2 (i), too large), or remove its dependence on a point by fixing the arrangement (Q6).

**F2 [wrong] The loop detects at a point solved at the previous lengths.**
- Evidence:
  - Round k records at C_k, the previous joint's choices (jj:497, 544), with the point set to the previous recording's own solution, which was solved at C_(k-1) (jj:547-548).
  - Detection then uses `at->x` (jp:1964-1965), rows from this round's Y solve (jp:2418) or from `at->y` when that solve fails (jp:2416-2421), and tiles refreshed at C_k (jp:2448).
  - So three states are mixed until the choices stop changing, and every clash kept meanwhile stays (jj:507-512).
  - D246 ruled this point. REVIEW-rule-source F2 asked for detection at the canonical point of the joint's own lengths.
- Fix: none needed under Q6. If the loop were kept, it should detect at canonical(C_k).

**F3 [missing] Placement ignores the arrangement the sweep already emits.**
- Evidence: see the verdict, fact 2.
- Counterexample (E2): LL_RR x LL_RR's all-floor recording has "bridge heads hug crossers (edge 28, row 2)" reading two tiles on row 2: edge 17's S-bend token 4 (point 84, a bend tile) and edge 14's bridge heads (point 80).
  - Edge 17's program is Under(28) x2, Under(29) x2, SBend, Under(14), SBend(Converge).
  - The sweep puts the e14/e17 swap in band 4 at layer 4, south of row 3 (row 3 west to east: ... e16 e17 e14 e15 ...).
  - So the Y solve put that swap two rows north of where the order says it is, and the hug recorded it there.
- Fix: Q6, step 2.

**F4 [missing] No rule says on which piece a crosser passes under a row bridge.**
- Evidence:
  - DESIGN-G1 §10.7 F6 says: "a bend's tail may overlap a row it is listed before or after (F6); (ii) decides" (DESIGN-G1.md:118). Nothing since has decided it except the flat solve.
  - The T's own seed passes under e7's doubled bridge on Corner tiles: e6's Corner starts on row 4 (copy 1's head row), and e3's on row 3 (E4).
  - LL_RR x L_R's e8 passes under e15/e16 on its Corner and S-bend: 14 of 87 strict-order rows are broken at its seed (E4).
- Consequence: a static hug needs a choice per (crosser, bridge copy) of the piece under the head row: the run, or the tail or head of the adjacent bend.
  - A 45-degree piece's column at a given row is affine in its first point and in the row: P3 generalised to Corners, test first.
  - LL_RR x LL_RR has 40 such crossings (E5).
- Fix: Q6, step 1.

**F5 [wrong] Band edges move sideways only by 45-degree S-bends, so big hubs run out of rows.**
- Evidence:
  - In the band programs (topo:1191-1245), a Corner appears only when an edge leaves a row node or when a joiner arrives. Every other sideways move is an SBend (Diverge, crossing, Converge).
  - A free S-bend's rows are its columns minus one (jp:1754-1763; link jj:344-350).
  - With a static hug and token order, LL_RR x LL_RR stays infeasible through e1's S-bend (L2 ≥ 26), e47's bridge over it, the e19 and e44 bridges with row 1's 14-tile prio between them, stack rows and band gap (E3; RECON-llrr-round0b §1). Under R3b the fan pins y4 at port row 26 (R3b made the fan's length a variable tied to the rows), so it cannot reach the 35 rows the cycle needs (RECON-llrr-round0b).
- Consequence: R3d made this feasible only by moving the offset from 25 to 50 (STATUS).
- Fix (engineer's decision, with the slack figures from Q6 step 3):
  - a horizontal band run (Corner, run, Corner), so that e47 crosses e1 square under a short bridge;
  - or taller bands (D252).
  - INFERRED: coop hubs cross perpendicular bundles with short bridges (RECON-junctionary-crossings:10); the corpus should confirm.

**F6 [wrong] A kept clash keeps its first gap.**
- jj:507-512 skips a key that is already kept. G3.6 F1 says "a kept row's gap may only grow ... updated to the larger gap" (DESIGN-G3-vectors.md:118); that is not implemented. This goes away with the loop.

**F7 [wrong] The conflict chain is the IIS of the LP relaxation, and it is not irreducible.**
- jm:116-123 calls `getIis` on `BuildLp(false)` with `iis_strategy` 2 (FromLp).
- With BIG = 1e5 (jj:26), any infeasibility that needs a binary or integrality is invisible: the LP is feasible, so the IIS is empty (R3d round 1).
- When the IIS is non-empty it is a true infeasible subset of the MIP, but it may hold redundant rows (RECON-llrr-round0b: 21 of 26 relations were singly redundant).
- HiGHS 1.16 has `kIisStrategyIrreducible` = 4 (HConst.h:325).
- Fix: Q5.

**F8 [wrong] The lane split run is an exact relation, which contradicts D216.**
- jp:1647 uses `along(..., true)` under the comment "The lane split sits next to its bridge (D215)". D216 says D215 "is an objective on graph variables ..., not an exact relation".
- It is not in today's cycles, which use it as ≥. It removes slack the static model will want.
- Fix: make it a floor, and keep D215 as the push.

**F9 [missing] The tractability levers are unused.**
- BIG = 1e5 (jj:26), while the coordinates stay below the box. Points have no upper bound (jj:184-185, INF), so no tight M can be derived.
- Every coordinate is an integer variable (jj:45, 184-185). Yet at fixed integer L, t, z and O, the rest is a difference system plus counters, whose vertices are integral (INFERRED).
- R3d's round 0 took 10.7 s (STATUS).
- Fix: bound x and y by the box (Σ of positive constants, as at jp:518-519); derive M per row from the bounds; make x and y continuous, and take the integral point from the canonical re-solve.

**F10 [missing] The seed spends its time in the cut search, not in the joint.**
- Every cut runs the rounds before the loop fallback (pipe:111-220).
- E1: L_R x LL_RR's start takes 16.5 s in total, against 0.24 + 0.25 s for its two loop rounds. LL_RR x LL_RR takes 16.9 s, of which 0.1 s is the joint. LLL_RRR x LL_RR takes more than 100 s (RECON-G2c).
- Fix: one model per cut, ranked by cost (§G3.6 F10). Later, make the series breaks binaries (D228: one bridge or several is "a cost choice").

## Q1. Convergence

**In general, no.**
- The loop is a fixed-point iteration: (C, P) goes to MILP(static(C) ∪ kept ∪ detect(C, P)), which gives (C', P').
- It terminates because keys are finite and the clash rows only grow (G3.3a b-6). But only the clashes are kept. Every other point-dependent family is recorded afresh each round (record_system, jp:426-514), so the rows as a whole are not monotone, and the map can cycle.
- It is correct only if every row added is valid for all legal layouts. That holds for a cutting-plane loop with valid cuts, and not here: F1 and F2 give counterexamples of both failure modes.
- The general condition for it to work is this: each family that depends on the point must be either
  - (a) modelled as its exact disjunction inside the MILP, or
  - (b) made independent of the point by fixing the arrangement.

**The families of build() (lines in jp), classified.**

| family | lines | class |
|---|---|---|
| stack origin, stack rows, band gap, S edge, S port row | 1524-1550 | static (constants) |
| port columns, S port spacing, row nodes, waiting room | 1554-1602 | static |
| prio element (pins, coverage, floors) | 1610-1616, 1660-1663, 1801-1803 | static (D233 sizes it) |
| row items, lane split run, stagger, series bridge heads | 1627-1658 | static (1647 exact: F8) |
| Corner size (fixed_k), joiner_div Corner floor, bridge heads, chain run, x/y floor, box bound | 1720-1771, 1805-1817, 1873-1875, 1972-1975, 2285-2294 | static |
| joiner_div Corner choice and X size | 1722-1733 | affine in L, tagged |
| S-bend size (vertical, free) | 1754-1763 (+ links jj:339-351) | affine in L, tagged |
| fan size, sized (W/E port fans) | 1709-1714 (+ jj:359-369) | affine in L; side σ from the recorded hand; recorded empty → only \|port row - y\| ≤ L (jj:360-363): stale sign |
| bridge over S-bend | 1820-1870 | affine in t and L (P3); which S-bend: branch (D248, record-only alternative, jj:287-296) |
| curve gaps | 1892-1958 recorded; jj:91-175 counters | affine as counters; which curves exist per route follows the bends' emptiness and the rows (sym_routes) |
| hug inequalities | 1665-1696 | depends on rows: which crosser tiles lie on the head row; "Z row" refusal |
| hug pins | 1427-1443 | order (flat x outermost); the joint drops them (jj:263); the hover needs them |
| bridge middle clearance X / Y | 1478-1497 / 2326-2347, 1877-1884 | depends on rows and columns (strictness) and on order (side; side 0 = both in Y) |
| occupancy | 1216-1424 | depends on rows (by_row[y]) and on order (flat sort, ties, groups) |
| fan empty, empty S fan on its port, S fan, rows S-bend zero | 1705-1708, 1778-1784, 1743-1747; jj:203-228 | branch (z with BIG) |
| a size-0 bend's tiles in a clash; a vanished bend's curves | jj:319-322, 162-171 | branch |
| terrain facts, D215 push | 1977-2283, 534-583 | hover / objective, not rules |

Most rows are static or affine. On LL_RR x L_R's seed recording, 743 of 1000 relations are neither occupancy (117), hug (82) nor Y clearance (58) (RECON-R3-loop §2). The point-dependent rows are exactly the families an arrangement fixes. Once it is fixed:
- occupancy pairs and sides are fixed;
- the hug reads named pieces;
- clearance sides are fixed.

What then remains are the branch families (fans), which are true binaries.

## Q2. One complete model

**(i) With the arrangement left free (every order, branch and row fact a disjunction).**
- Non-overlap between two pieces needs four cases (west, east, north, south), so 2 binaries per candidate pair.
- R3d's round 1 on LL_RR x LL_RR already kept 529 pairs (STATUS). Points grow from 215 to 535 for LLL_RRR x LLL_RRR (E5).
- That gives about 1000 binaries on LL_RR x LL_RR and about 6000 on LLL_RRR x LLL_RRR (pairs scale with points²; INFERRED).
- With BIG-M rows, the LP bound is useless. Two-dimensional compaction is NP-hard even with the shape fixed (INFERRED from the literature).
- R3d's round 0, which had no clash binaries at all, already took 10.7 s (STATUS).
- Not tractable for the sweep, and the D242 seed cache does not save it.

**(ii) With the arrangement fixed by the sweep (Q6).** Binaries only for local choices (counts from E5):

| hub | piece under row bridge (≤ 2 per Under token) | single bridges (t, w) | zero-size bends (≤ bends) | total binaries (≤) |
|---|---|---|---|---|
| LL_RR x LL_RR | 40 tokens → 80 | 9 | 61 | ~150 |
| LLL_RRR x LL_RR | 96 → 192 | 14 | 90 | ~300 |
| LLL_RRR x LLL_RRR | 144 → 288 | 33 | 135 | ~450 |

- Rows: today's static rows, about 1500 / 2600 / 4200 (E5), plus the arrangement's neighbour rows, which are linear in the pieces (INFERRED: about 2 per row occupant).
- Measured reference: one loop round with 98-135 fixed clashes and no order binaries takes 130-250 ms, recording included (E1).
- With a tight M, continuous x and y and about 150 binaries, I expect under 1 s on LL_RR x LL_RR and a few seconds on LLL_RRR x LLL_RRR (INFERRED; step 2 measures it).
- HiGHS has semi-integer columns (HConst.h:110 `kSemiInteger`), which model "L = 0 or L ≥ floor" without z where only the size matters. Highs.h has no indicator-constraint API (grep), so big-M stays for the conditional rows.

## Q3. Other architectures

| option | complete? | deterministic? | seed time | hover |
|---|---|---|---|---|
| Lazy rows in one branch and bound (callbacks) | Only if the rows are valid disjunctions, so it is (i) again. And HiGHS 1.16 cannot add rows from a callback: the input holds only interrupt and user solution (HighsCallbackStruct.h:56-61, HighsCallback.h:64-79); the lazy callback type is declared (HConst.h:258) but has no input. It needs another solver (revises D237). | yes, one thread | better than re-solving | unchanged |
| Re-solve from scratch with valid lazy disjunctions | yes; this is the correct form of today's loop | yes | rounds × a growing MILP of size (i) | unchanged |
| Y first (rows, stack, lengths), then X at fixed rows | Only with valid feedback cuts. An X positive cycle at fixed rows gives Σ(a L + b) ≤ 0, which is valid only if no row in it depends on y; without the arrangement, the hug and occupancy rows do. Diagonals couple both axes through L (RECON-llrr-round0b's cycle crosses axes only through L2), so the master must carry every L anyway. | yes | many master rounds | X stays a difference system |
| Structure choices by an outside search, the rest a difference system or LP | Only if it enumerates. The cut search is this pattern, and it does not finish in 100 s on LLL_RRR x LL_RR (RECON-G2c). Choices fixed by rule plus a few in-model binaries is Q6. | yes | explodes if it enumerates | fixed-length system |
| Constructive generator for regular hubs | No (regular hubs only); it is per-shape code, against D202 and D217 | yes | fastest | unchanged |
| **Arrangement first, one static MILP (Q6)** | Complete for the ruled arrangement (D207: structure by rule, only lengths minimised). A different arrangement is a graph edit (D213, D216). | yes; no point input | one MILP per cut | static rows at the seed's choices; no detection; REVIEW-rule-source F3 and F7 become moot |

The hover stays a fixed-length difference system under every option. Only Q6 also removes its stale or fresh-detection problem.

## Q4. Is the model right at the root?

The binding rules of the RECON cycles, judged one by one:

- **The hug** (RECON-llrr-round0 R6/R7; round0b rows 175 and 467).
  - The inequality is a true rule: a bridge spans its crossers.
  - The equality to the outermost crosser is a cost preference, and the joint already drops it.
  - The staleness comes from F3 and F4 (which tile is on the head row), not from the rule.
  - E3 shows round 0 still fails with a static hug, so in round0b the hug is not the cause. The probe applies the programs' strict token order, which is stricter than F6 (that is the 3 broken rows at the T's seed in E4). RECON-llrr-round0b's "drop the hug and it is feasible" removes the span rule itself.
- **"A single bridge passes over the S-bend it crosses"** (DESIGN-G1 F2, §10.7).
  - For two vertical band tracks, one must move sideways under the other, so a sideways piece is geometrically needed.
  - That the piece is a 45-degree S-bend comes from the T (topo:1215-1219).
  - D248's "after" alternative is, on edge 17, the Converge S-bend that serves n23 (E2): one S-bend doing two jobs, chosen by the solver.
  - With F5's horizontal run, the crossing becomes square and the whole t, w machinery shrinks.
- **The lane split runs** (R4/R5 in RECON-llrr-round0; rows 452-457 in round0b).
  - The floors are true rules: D113 (a signal-legal piece between split and head), D209/D211 (a signal tile between series heads), D145 constants.
  - The exactness at jp:1647 (F8) and the exact stagger, a style choice (D105 "the shape is free", D145 "a discrete solver choice (0 or +1)"), are T-shaped pins.
  - They are not what binds in these cycles.
- **The prio element's width** (14 tiles, round0b rows 149/150) is a true rule (D233).
- **The stack rows, band gap and port fan cap** are true structure. The cap is binding only because the offset is fixed (D252 frees it) and sideways moves cost rows (F5).
- **L_R x L_RR's round-0 "joiner_div Corner choice <- stack rows"** (RECON-R3-loop §2) is an artefact of the all-floor start. The recording pins the Corner span with an equality (jp:1722-1727) that the joint does not keep.

In summary:
- Two rules are T generalisations that do not scale: the S-bend as the only sideways move, and the piece under a row bridge being left to the flat solve.
- Two are T shapes pinned exact: the lane split lean and the stagger.
- The rest are true rules.

## Q5. Diagnosis tooling

1. **Now, in jm:119:** set `iis_strategy` to 2 + 4 (FromLp + Irreducible; HConst.h:321-331). An LP IIS is then exact for the rows without binaries. Report "MIP infeasible, LP relaxation feasible: the conflict needs binaries" instead of printing an empty chain.
2. **An elastic mode (the main tool for the engineer).**
   - Add a slack s_g ≥ 0 per rule group (reason and edge), with a large cost, and solve once.
   - The result names the groups that must give, and by how much. For example, round0b's cycle would show "port row cap short by 9 rows" (0 ≥ 9 in RECON-llrr-round0b).
   - It is one deterministic MILP and needs no new library feature.
3. **A deletion filter on the MIP, offline** (a `jt` diagnostic tag, never in seeds).
   - Use QuickXplain over rule groups: about k log(n/k) feasibility solves with zero cost, where k is the IIS size (10-30) and n is about 1500.
   - At 0.1 s per solve on LL_RR x LL_RR (E1), that is about 300 solves, under a minute (INFERRED).
4. **Under Q6, at fixed binaries,** the system is a difference system in x and y with L as variables, and JunctionPositions' `Explain` already gives exact positive cycles at fixed L.

## Q6. Recommendation: arrangement first, one static model

**Step 1: the arrangement as data, with tests first (outside-in).**
- Build a `JunctionTrackArrangement` from the order, the structure and the programs:
  - per row: its occupants west to east (order.rows plus each chain's token order);
  - per band: slices between layer boundaries with their items west to east;
  - per (crosser, row-bridge copy): the piece under the head row.
- Tests:
  - (a) "the seeds obey the arrangement": T, LL_RR x L_R and L_R x LL_RR at their seeds. E4's strict form is the red baseline: 3 of 17, 14 of 87 and 4 of 35 rows broken. The piece choice must turn it green.
  - (b) "band slices are a strict layer order or a stated partial order": decided by what the three seeds satisfy.

**Step 2: static families and one solve, with the loop deleted.**
- Occupancy: separation between the arrangement's neighbours, with tile forms affine in L (P2).
- Hug: per piece, affine (P3 extended to Corners, test first).
- Clearance: sides from the arrangement.
- The piece choices, w and z as binaries; the offset O (D252) as an integer.
- Tight M; x and y continuous; the canonical point (least Y, then least X plus push) as the output.
- Tests:
  - T and LL_RR x L_R reproduce 27300 and 86350, or list each re-pin (D235);
  - L_R x LL_RR still seeds and is rule-checked;
  - "the joint has no point input" (API);
  - "two solves give identical rows" (determinism);
  - legality and CheckJunctionRules on every seed;
  - per-hub solve-time budgets in TESTS.md.

**Step 3: exact diagnosis, and the program question to the engineer.**
- Q5 items 1 and 2, run on L_R x L_RR and LL_RR x LL_RR.
- Bring the engineer the slack figures and the two options for F5: a horizontal band run, or taller bands.
- Tests:
  - "an infeasible hub names an irreducible set" (removing any one named group makes it feasible);
  - "elastic mode names the rule and the shortfall".

**Then:** the cut search on the single solve, ranked by cost, with the series breaks as binaries later (F10, D228); then the LLL hubs.

**Delete:**
- SolveJunctionJointLoop and its tests, `JunctionSystemPoint` and the at-mode (jp:110-114, 2407-2463);
- the kept-clash keys, and the clash rows and order binaries (jj:300-334);
- detect, detect_y, plan_clearance and the flat-solve detection;
- JunctionFloorChoices as a start; the D250 fallback (pipe:200-217);
- after step 2, the rounds (R6).

**Reused or retired from today's merged work:**
- Reused:
  - R1 (sizes as inputs), and R2's static and lazy split (the lazy half becomes static);
  - G3.4a-2 (t, P3), and G3.4b (w; re-justify against the band layer);
  - R3b (fans tied to rows), G2c-1, and TT3's rule check;
  - R3d's O variable without the loop.
- Retired: R3a, R3c, R4a.

**Decisions this revises:**
- D236: the sweep's order, its own fallback, becomes the plan.
- D240: in "Occupancy pairs come from the joint solution (lazy, kept)", the pairs now come from the arrangement.
- D243: the hug becomes static per piece.
- D246 is retired.
- D249 is retired: order is the graph's (D213, D216).
- D250 becomes one solve per cut.
- D248 is re-derived from the layers.
- D252 is kept.
- D217: the rule family is unchanged; only the source of the pairs changes.
- For the engineer: F5 (a program rule), and F8 against D145 and D216.

## Evidence (runs in ng1 at aad3c5f03e, Release)

- **E1: `openttd_test "joint: the joint loop on the hubs"`** (36.7 s; g3loop.log).
  - T and LL_RR x L_R settle in 2 rounds (14 and 131-139 ms per round).
  - L_R x LL_RR settles: 239 and 249 ms per round, but 16.5 s in total.
  - L_R x L_RR: objectives 29100, 30050, then 61350. Round 3 keeps 100 clashes, all chosen, and is infeasible; its failed recording lists a Y and an X cycle.
  - LL_RR x LL_RR: round 0 infeasible (96 ms), 16.9 s in total; without the bridge-over-S-bend rows, status 0.
- **E2: dump of the order, programs and all-floor hug rows** (dump.log).
  - LL_RR x LL_RR: row 3 west to east is n16 e20 e22 e5 e8 e16 e17 e14 e15 e30 e31 n10.
  - Band 4 holds [x14/17 L4].
  - Edge 17's program is Under(28) x2, Under(29) x2, SBend, Under(14), SBend(r5).
  - The hug (edge 28, row 2) reads x84 (e17 SBend, nl) and x80 (e14 Heads).
  - The recording has X 795 and Y 747 relations.
- **E3: static order probe** (probe.log): the recorded hug is replaced by "run column strictly inside the heads", the token order is imposed in Y, optionally with the row order.
  - LL_RR x LL_RR stays infeasible with the same members (e1 S-bend, e47 over S-bend, e19/e44, stack rows, band gap).
  - L_R x LL_RR keeps 63900; L_R x L_RR goes from 29100 to 31100.
  - LL_RR x L_R becomes infeasible: the strict order is too strict (F4).
- **E4: those static rows evaluated at the seeds** (probe2.log).
  - Broken: T 3 of 17 (e6's Corner on rows 3-4, e3's on row 3), LL_RR x L_R 14 of 87, L_R x LL_RR 4 of 35.
- **E5: sizes** (count.log). Points, bends and relations of the all-floor recording, and Under tokens of doubled bridges:

| hub | points | bends | relations | Under tokens (doubled bridges) |
|---|---|---|---|---|
| LL_RR x LL_RR | 215 | 61 | 1542 | 40 |
| LLL_RRR x LL_RR | 353 | 90 | 2639 | 96 |
| LLL_RRR x LLL_RRR | 535 | 135 | 4219 | 144 |

- **HiGHS 1.16** (`~/.local/highs/include/highs`):
  - lazy callback type declared but with no input (HConst.h:258; HighsCallbackStruct.h:56-61);
  - IIS strategies at HConst.h:321-331; `kSemiInteger` at HConst.h:110.

## Suggested tests
1. "the seeds obey the arrangement" (T, LL_RR x L_R, L_R x LL_RR; red baseline E4).
2. "a static solve reproduces T and LL_RR x L_R" (27300 / 86350, or listed re-pins).
3. "the joint model takes no point" and "two solves give identical rows and points".
4. "every seed is legal and CheckJunctionRules-clean" (keep TT3's).
5. "an infeasible hub names an irreducible set" (delete-one check) and "elastic mode names the shortfall" (LL_RR x LL_RR: port row cap).
6. "LL_RR x LL_RR e1 with a horizontal band run fits the fixed port rows": a probe for F5 before asking the engineer.
7. Solve-time budget per hub in TESTS.md: LL_RR x LL_RR ≤ 1 s and LLL_RRR x LLL_RRR ≤ 10 s Release (to be set from step 2's measurements, D251 style).
