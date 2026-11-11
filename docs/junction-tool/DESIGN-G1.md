# DESIGN-G1: the node graph from a topology (D189, D201, D202) — DRAFT

Status: orchestrator draft, 2026-11-11, for review (engineer; Fable). Inputs: D186-D192, D201, D202; DESIGN-nodes rev 3 §2-§4; DESIGN-N3 §1-§8; RECON-turn-programs; RECON-template-graph; FABLE-topology-graph; RECON-llrr-facts (§3 questions 1, 2, 4, 5, 6 are ruled).

## 0. Goal and shape
topology -> **structure** (combinatorial: paths as typed runs and nodes, crossings, orders) -> **reference** (positions computed by maths: the least solution of the structure's constraints) -> **template** (today's JunctionNodeTemplate, built by today's rule builder from the reference) -> solve. No routed or constructed layout anywhere on this path. Outer tests from the start: the T (oracle: today's traced template) and LL_RR main line + L_R sideline (D202).

Why a reference at all (FABLE-topology-graph Q1): the solve writes every constraint as a difference from a reference, prefers it in the Y snap, and reads which pieces share a tile, the order of elements on each row, and what passes under each bridge from it. G1 makes that reference a computed output, never a drawing.

## 0b. No lost progress (engineer, 2026-11-11: "I just want to make absolutely sure we're not going to end up losing progress here")
- G1 replaces only the template's SOURCE (topology instead of the traced flat T). The solve (two-phase, facts from GetRailFoundation, height joins, prio rows, occupancy), the search and detours, HN1's failure naming, the oracle and every existing test stay as they are.
- Every G step keeps the full gate green and [nodeoracle] at heights=0 fits=0 with accepted >= its last count; a step that would edit an existing expectation stops for a ruling.
- The hover path switches to the generated template only after G1c shows it structurally equal to the traced T and at least as good on the oracle sweep; the traced template stays as a permanent test oracle.

## 1. Input: JunctionTopology
- **Frame**: the box; the main line runs along X (D152 canonical frame). Sides N, E, S, W.
- **Ports**: per side, an ordered list of lane groups. A lane group = one direction of one line: side, direction (in/out), lane count k (adjacent lanes, gap 0, D142), its line, and the gap to the next group on that side (the underscores, D38/D142).
- **Movements**: (in group -> out group). Lanes are expanded by rule (§2 R1), not listed.
- **Crossing assignment**: for each crossing pair (derived, §2 R4), over or under; default: main line over (D156/D190).
- Spec constants as today (TL, CL, prio, SD, bridge limits).
- The T today: A (W->E main direction), the W line (E->W, two lanes inside the hub, D80 doubling), N in, S out; movements as JunctionTMovements. LL_RR + L_R: main line LL (E-bound) and RR (W-bound), sideline L (in) and R (out) on the S side.

## 2. Structure rules (each with its source)
- **R1 Lane expansion.** Through movement main->main: lane i -> lane i, straight (RECON-llrr-facts §3). Exit main->branch: every lane of the direction splits toward the branch (D187); the legs merge (plain lane merge, no prio) before the branch port. Join branch->main: the branch track splits into one leg per main lane (1-to-2 fan, D186), each leg joins its lane with its own prio. A line-choice split has a signal right after it (D192). No shifters (D191).
- **R2 Bridge doubling.** A main-line lane carried over a crossing gets a doubling fan (split, two equal bridges, merge; D80), per lane (D188: four bridges for LL_RR). A non-main bridge is single (D174).
- **R3 Node order along a line.** Splits before merges (D189). Among the splits of one lane: by the side their legs leave towards, nearest first (INFERRED from the T: A splits to S before nothing else; W splits to S then the lane split; to confirm on LL_RR).
- **R4 Crossings.** Two paths that share no node cross iff their end ports interleave around the box boundary (chord rule on the port slot order); exactly one crossing per interleaved pair (minimal). Paths that share a node (split/merge) and swap sides after it also cross once (INFERRED; check on LL_RR's fan legs, where inner-lane legs must pass the outer lane: a diagonal cannot flat-cross an axis track in the game, so it is a bridge).
- **R5 Order of crossings along a path, and of elements on a row.** From a straight-chord embedding of the ports on the box (positions from the port order and the spec gaps): the order of intersections along each chord. Replaces "flat x order" in occupancy (DESIGN-N3 §7) for generated templates. OPEN-1 below.
- **R6 Turns.** Per path, the heading at its in-port and out-port gives the net turn: 0 (through, S-bends only), 90 left/right (a corner: two same-hand 45-degree curves, diagonal of odd half count >= the curve rule, RECON-turn-programs §2/§5). Row changes along a line (to a lane row, to a fan leg's row, past a bridge) are opposite pairs (S-bends, DESIGN-nodes §4b).
- **R7 Sides.** A split's leg leaves toward its destination side. Prio rows: at main-line merges, on the side away from the joining leg where space permits (D28 and its "wherever space and cost permits" ruling).
- **R8 Lengths are constraints, never values.** Curve gaps (N0a), prio coverage (D108/D148), D113 signal pieces, whole-train bays D176 (OPEN-2), D145 lane constants, bridge spans and D172 windows, port stubs D115a, port approaches D179, box edges. All are today's template rule kinds; G1 emits them from the structure.

## 3. Reference positions
- The structure's constraints over X and Y classes are difference constraints, so their least solution (every class at its lower bound, JunctionPositions after AddJunctionNodeConstraints; min-closed, DESIGN-N3 §7) is a consistent assignment by construction: the reference. Lengths not bound by any rule take their minimum (RECON-turn-programs §3: "slack from the frame" disappears).
- From the reference the builder renders each path as a piece sequence (run directions and lengths are known) and hands it to the rule builder (split from the tracer, step G1b), which produces the template exactly as today. The reference is the template's `flat`.
- Consequence: the generated T is more compact than today's flat T where today's has frame slack (A west of the split 23 tiles, etc.). The oracle comparison is therefore structural (§5), not tile-equal.

## 4. Open points (orchestrator; to settle before briefing, engineer only where marked)
- **OPEN-1 Reference order without positions.** R5 needs an embedding before any position exists (occupancy order, crossings' order along a path). Proposal: the straight-chord embedding (ports placed at their slot order with the spec gaps, chords as straight lines), orders read from it, then the least solution. Check on the T that it reproduces the traced orders (W line over A->S and N->E; N->E over A->S; RECON-turn-programs §4).
- **OPEN-2 Whole-train bays (D176) — ruled, not open.** D176: the number of whole bays per waiting leg is a throughput decision, given as an edge minimum; on multi-line legs the leg is whole bays or the leftover carries no signal. D192: a signal straight after every line-choice split. So k is an input (default 1 until the traffic model), and with k fixed the leg from the post-split signal to the merge signal is exactly k bays: an equality, i.e. two difference constraints. Remaining check (orchestrator, not engineer): half-piece parity (D88) can make an exact multiple unplaceable; measure on LL_RR.
- **OPEN-3 Today's T vs rules.** RECON-turn-programs §6 lists 10 places where today's T is not at a rule minimum or is set by frame constants (one-tile split/join convention, lane merge 16 vs 15, leg lengths 3 set by bridge positions, k >= +1 at CL 6/7 reaching the N column). The generated T resolves each by rule; the oracle test must accept these differences explicitly, each named.
- **OPEN-5 The reference is circular as written.** §3 takes the least solution of the template's constraints, but today's rule builder writes those constraints relative to a reference (units from flat tiles). Two ways out: (a) a run-length system: variables = each run's length (and each diagonal's half count), the same rules written over run lengths along each path, solved for least lengths; then render. (b) Seed and solve: render every run at its own minimum (a consistent piece sequence if the orders of OPEN-1 hold), build the template, solve it on flat facts, and take the solved values as the reference (one pass; the solve already guarantees every rule). (b) reuses everything; its risk is a seed whose occupancy order is wrong. Orchestrator leans (b), with OPEN-1's embedding giving the seed's row orders.
- **OPEN-4 Ports in the rules and checker.** JunctionPortRole (EIn..SOut), JunctionTMovements and FindJunctionPorts (2 tiles per edge) are T-only; the checker (test oracle) must read ports and movements from the topology for LL_RR. Size: RECON-llrr-facts §2.4 (CheckWaitingRoom, CheckPrio, ML-bridge rule, consumers taking the first path per movement).

## 5. Steps (each test-first; Fable's migration order, FABLE-topology-graph Q3)
- **G0** exact template comparator (order, class numbering up to renaming, every rule kind) + test: traced T == traced T rendered from its own flat solve.
- **G1a** split the rule builder from the tracer: it takes paths (piece sequences) + bridges + slot queries; the tracer becomes one producer. Behaviour unchanged (G0 proves it).
- **G1b** JunctionTopology type + the T's and LL_RR's topologies as data; structure rules R1-R7 -> structure; test: the T's structure (paths, node order, crossings, turns) equals the traced T's.
- **G1c** reference (§3) + render to piece sequences + rule builder; test: generated T template structurally equal to the traced one, and solves to a rule-clean T on flat at the test spec and at 140/160/196 km/h.
- **G1d** ports/movements from the topology in the checker (OPEN-4).
- **G1e** LL_RR + L_R: generated, solved on flat, rule-clean by the checker; then the near test and the oracle sweep on it.
- Then RS1 as a graph edit (DESIGN-N3 §11) on generated templates.
