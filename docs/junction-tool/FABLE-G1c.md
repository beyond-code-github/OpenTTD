# FABLE-G1c: review of DESIGN-G1 §10 (the seed as path programs)

(Orchestrator note, 2026-11-12: this review predates the wording rule; its "Shift" means an S-bend, not a coop shifter.)

Read-only review at 940e1df2ea. Sources: DESIGN-G1 §7-§10, src/tests/junction_topology.cpp (graphs, orders, stacks as tested), RECON-G1c-seed, RECON-turn-programs §1-§3, DECISIONS D75 D80 D108 D110 D113 D115a D118 D142 D145 D172 D174 D176 D186-D204, src/junction_nodes.cpp.

**Verdict.** The grammar is right and most of §10.2 follows the T. Applied literally it does **not** give a drawable LL_RR seed, and it reproduces the T only because §10.3 reads two bends off the trace. Five rules are missing or wrong: band rows (F1), band-crossing Shift (F2), y order in band R (F3), Corner length (F4), doubling form (F5). (b') is sound only after four template fixes, not two (§3).

LL_RR recap (from the tests): N0/N1 e1/e2 exit splits; N2 branch-in direction split; N3 J_e line-choice split (band 2); N4 J_w split (band R); N5/N6 w2/w1 exit splits; N7/N8 w1/w2 merges; N9 S merge; N10 X_w merge (band R); N11 X_e merge (band 2); N12/N13 e2/e1 merges. Edge 3 = J_e trunk, 23 = X_e trunk. Rows: e2 [N1, X_e1, J_e1, N12]; w1 [N7, E23, E3, N6]; w2 [N8, J_w1, E23, E3, X_w1, N5]. Stack: e1p 0, e1 1, e1a 2, e2pc 3, e2 4, e2a 5, w1pc 6, w1 7, w1a 8, w2pc 9, w2 10, w2a 11.

## 1. §10.2 applied literally

**F1 [collision] Band 2 has no rows.** N3 and N11 sit at level 4 (band 2, between e2 and w1), but the §9 stack puts e2a (5) next to w1pc (6), and the lanes are adjacent tile rows (Shift = 2 halves per row). The bends do not fit:
- J_e2 diverges at N3 with a Corner R onto e2a. A minimum Corner of 5 halves rises two rows above its split tile (the T's D5: halves (26,4),(27,4),(27,3),(28,3),(28,2)). So N3's split tile lands on row 7, which is **w1's lane**.
- X_e2's Corner R off e2 (row 4) reaches row 6. Its Shift into X_e1's column (N11 inside the Shift) then needs rows 7-8, which is **w1** again.
- More rows are needed on top of that. D192 wants a signal piece straight after N3 on each leg. The exit-leg bays are vertical here: template rule 5 (junction_nodes.cpp:528) and rule 6 (:538) need 2·TL units from N0 (or N1) to N11's last slot, and D176/D187 need whole-train legs. So band 2 must be about TL rows deep.

The T never shows this, because its band nodes are all in band R, which has free depth down to the stub. The §10.4 claim "the row orders keep every Y Run clear (checked by hand)" misses it.
- **Fix:** the stack gets a band slot between two lane triplets whenever band b < R holds nodes. Its height comes out of step (ii), the larger of the bend geometry and the bay.
- **Cost of the fix:** the band slot adds its height g to every w row, so fans and box depth grow (Q2(1)).
- **Alternative, for Pete:** put N11 in band R (merge by the port). X_e1 and X_e2 would then each pass under w1's and w2's bridges, one more crossing column per bridge and no new bridges, and their bays would use band R's free depth. D204 ("lane legs merge first", the mirror of the branch-in side) currently places N11 in band 2; grep D187/D204 before raising it.

**F2 [no answer; §10.4 wrong] Nothing makes the band crossing.** Two Y Runs cannot cross.
- **T:** P1's D2 Shift (column 25 to 28, rows 7-9) under N->E's bridge (column 26) is what realises crossing AS x NE. No §10.2 rule produces that Shift; §10.3's "matches" reads it off the trace.
- **LL_RR:** §10.4 has X_e going "Y ... under J_e's single bridge ... straight through the S merge". But X_e is west of J_e on w1 and w2 (E23 before E3), and P5 is east of P4. X_e must therefore make a Shift east under J_e's bridge.
- **Rule:** in a band crossing, the path that goes under makes a column Shift beneath the joiner's bridge, from its sweep position before the swap to its position after it.

**F3 [no answer] y order inside band R.** N2 diverges the J_w trunk with a Shift west ("into its own column"). That NW diagonal crosses X_e's column c_X, so X_e's crossing Shift (F2) must lie wholly above it; otherwise we get X_e x J_w trunk, which is not in graph.crossings. The two diagonals are parallel and never meet once they are ordered.
- X_w1's column must lie east of P5's column, or X_e's Shift crosses X_w1.
- After the J_w trunk's Shift L R comes J_w2's Corner L L at N4. The non-consecutive curve gap min(7, 2·CL) units needs a Y run between them.

Programs carry no lengths, so G1c(i) cannot express any of this. Add a per-band y order (nodes, bends and bridges, like §8's row items), or state that (ii) derives it.

**F4 [wrong] "Bends at rule minimum" fails for a Corner from a band node to an approach row.** Its halves are 2·Δrows + 1, fixed by rows (RECON-turn-programs §2: D7 is 13 halves "by rows").
- **T:** N->E needs a D113 piece, a single bridge over P1's D2 Shift and another D113 piece, all between the N split and row 5, so the split sits at least about 6 rows below row 5.
- **T, D7 at its minimum (5 halves):** the N split would be at row 7 and its half at (25,6) would sit on P1's column 25. That makes N->W x A->S, a crossing that is not in the graph.
- **LL_RR:** J_w2's Corner at N4 and J_e2's at N3 behave the same way.
- **Fix:** such Corners take their length from the rows. In the template this is harmless (free diagonals: m ≥ m_min, :501; late x = flat + dx·steps, :740-745).

**F5 [wrong vs D145/D118/D110] The doubling form.** §10.2's form keeps the lane copy straight and gives the side copy two Shifts. That is not D145's measured parallelogram.
- In that form the side copy is B-like at the near end (x_ls − x_he = 1) and A-like at the far end (2). The lane copy is A-like near (2) and B-like far (3). So the spans differ by 2 tiles, unless extra plain pieces are added; D110 objected to that ("extra tile after the split").
- D145 also says style 0 "has no clean candidate ... constants not measured".
- **Rule it as the parallelogram now:** a crossed lane runs on its copy row upstream of its doubling and on its lane row downstream. Each copy then has one Shift and the D118 stagger applies. The copy row is the prio side and the prio is downstream, so D108 keeps them apart in x.
- **T:** this is exactly the trace (W on row 3 east, row 4 west). It removes §10.3's named difference and both E-side Shifts, so the T oracle becomes exact.
- **LL_RR:** e2 runs on 3 west / 4 east, w1 on 6 east / 7 west, w2 on 9 east / 10 west. X_e2, X_w1 and X_w2 then leave from the copy row, as the T's B->S split does (row 3). D145's x_sl − x_ls = 2 shares a tile as the T's (31,4) does.
- "Lazy fan" is therefore not a cost step: it is this rule.

**F6 [rule wording] Bends pass under bridges, not just Y Runs.**
- **Joiner Corners:** J_e1's Corner onto e1a lies on rows 4,3 under e2's two bridges, as the T's D5 lies under W. J_w1's Corner lies on rows 10,9 under w2's.
- **Exit Corners:** X_e1's last half is on row 3 under e2's copy bridge (the T's D1); X_w1's is on row 9 under w2's.
- "The crossed Y Run passes under" should read "Y Run or bend". AddCrossings already handles diagonals (:588-600).
- **Span check:** w2's bridge covers J_w1's Corner (2 tiles), X_e, J_e and X_w1's Corner. That is about 5-6 tiles against the D172 window of 7 at nmax 10. It is tight; check it at every speed in (iv).

**F7 [missing from the grammar] Signals and stubs.** None of these is an event in §10.1, so (ii) must place them by rule:
- D113 pieces: N2 to J_e's near head; lane split to heads; far heads to lane merge.
- D192 pieces after N3 and N4.
- D115a stubs of exactly 2 tiles under N2 and under N9, with both switches on one row (box).

Add a `signal` event, or list these in (ii).

**F8 [wording]** "Exit leg leaving a lane split" means the exit split (N0, N1, N5, N6). That clashes with the doubling's lane split. "Joiner ...: Y Run north": J_e2 and J_w2 have no Y Run, because they corner at their split.

**What does work literally:**
- T_e1 is one Run.
- Corner hands match structure.turns.
- Prio is north on every lane and every approach is south.
- Approaches are x-disjoint from the crossing Y Runs. J_e2's approach starts east of column a, X_e2 and X_e1 lie west of it, and J_w2's approach is west of J_w1.
- D108 reduces to lengths: every crossing of a lane is inside its one doubled bridge, which must clear prio coverage plus one segment.
- X_w1 runs straight into N10 and X_w2 Shifts in; on the T, P5 and P6 come out as stated.

## 2. §10.5 open points

**(1) Fans are drawable with one added rule: every Shift of one fan starts at a common column.**
- **Why it works:** Shifts are monotone outward, so with a common start each adjacent pair's row gap changes monotonically from its stack gap to its port gap, which is at least 1. Parallel diagonals at S=1 are legal (D142).
- **Without the rule** the order is undefined. Example: on the W side w1 (7→6) must leave row 7 before w2 (10→7) lands on it.
- **No fan Shift meets a side track.** Each side track lies inside its side's span of first and last node or bridge: prio by D108 (the coverage ends at its merge), a copy between its lane split and lane merge, an approach between the joiner's bend (under a crossing) and its merge. The fan zone is outside that span by definition.
- **The largest Shift is not 3.** It grows with F1's band height g: w2's Shift is about 3 + g with the §9 rows, and less with F5, because the crossed lanes' upstream rows move to the copy rows.

**(2) "Farther child continues straight"** is consistent at all six LL_RR band nodes (N2, N3, N4, N9, N10, N11) and at the T's two (the N split and the S merge). It is not general:
- The diverging child's Corner hand must agree with its sweep side: a west child turns left/W, an east child turns right/E. If the farther child is on the wrong side, the near child's Corner cuts across the straight Y. Rule: straight = the farther child, unless that forces this cut; then the near child goes straight and the farther child Shifts.
- For a BBH, "farther" means farther from the node's own port edge. That stays refused until N-side sweeps exist.

**(3) Refusing a doubled trunk is fine for now.**
- **Both topologies are safe:** in each, every lane bridge is on a leg (LL_RR edges 8, 9, 10; the T's edge 5), and edge 3 is a single branch bridge (D174).
- **Put the refusal where `doubled` is set** (StructureJunctionTrackGraph, §9), with its own error text and one negative test, not in G1c.

## 3. Is (b') sound?

Yes in outline, but RECON-G1c-seed lists more than the two fixes in §10.
- **(a) Bridges are rigid** (heads in one class, :341; crossing pieces equal to the head line, 1j/1k). The columns under one bridge must therefore be drawn at minimum spacing, not "generous". Generous X runs between crossers become bridge span, and Lo keeps it.
- **(b) Lo of the early system is not a layout.** Late diagonals (:740-745), late gaps (:748-757) and occupancy depend on the Y values. Use Snap's phase order (:918-934) with Lo in place of the clamp-to-flat: Y = Lo, then late constraints, then occupancy, then X = Lo.
- **(c) Do not just free the anchor.** Any class with only upper bounds then drops to the domain floor (-1024). Pin the box's north-west corner instead (the min-x port class and the min-y port class), and assert Lo(c) > -1024 for every class.
- **(d) More seed choices that get frozen into the reference:**
  - **Occupancy gap** = min(2, flat gap) on one flat row (:903). A generous seed gets 2 where the T has 1 (OC1). The reference grows and the fixed point hides it; make it a rule (touch → 2, else 1).
  - **Notation spacing at the ports (D38/D142)** is held only by the snap to flat. I found no template rule for it: AddBoxEdges (:630) ties only extreme ends. So Lo should collapse L_R's "_" (the T's W port row 3 → 2) and the S columns 26/28. It needs a rule before Lo can serve as the reference.
  - **Also seed-read:** the joiner-row sign (:367); the slot set (input.slots, keep_slot halves :501); flat_bits; AddCrossings membership; box ends.
  - **Role-based code:** IsMainLinePath (:111), rule 5's EIn/WIn→SOut (:528), rule 6 (:538), and the EIn->SOut anchor (:688). LL_RR has two EIn->SOut paths (X_e1 and X_e2), so LL_RR's template is G1d work.
- **(e)** D80 equal copies as a rule, as §10 says. With F5's stagger it is equal spans.
- **(f)** The fixed point should compare the templates modulo `flat`, and also compare the seed's AddCrossings sets with structure.bridges.

## 4. Steps and outer tests

**The split is right in shape; four changes:**
- (i) also outputs each band's y order (F3) and the band slots (F1).
- (ii)'s outer test must be geometric: no tile conflicts, AddCrossings sets == structure.bridges, ports aligned, and the T render clean under CheckJunctionRules at the test spec (D193 oracle).
- (iii) starts with the template fixes from §3 (a)-(e), then Lo and the fixed point on the T.
- LL_RR stops after (ii) until G1d. (iv) is T-only.

All are test-first-able: (i) is pure data.

**G1c(i) outer test, T:** the 6 paths plus W's second copy as literal programs.
- **Hands (from RECON-turn-programs §1):** P1 RRLR, P2 none, P3/P4 LRRL, P5 LLRL, P6 RRLR, P7 LLRLRL. These are exact with F5; under §10.2 as drafted P3/P4 carry the named extra Shifts.
- **Events, e.g. P1:** port, split, Corner R, under W-copy bridge, under W bridge, Shift under N->E's bridge, merge, port.
- **Invariants:**
  - Σ Corner quarter-turns = structure.turns.
  - Over/under events per edge = order.edge_crossings.
  - Node events = graph.path_edges.
  - Every bridge event set = structure.bridges.

**G1c(i) outer test, LL_RR:** 12 paths plus 3 second copies (e2, w1, w2) as literals, with the same invariants.
- **Literal programs to pin, at least:**
  - X_e1: E port, N0, Corner R, under the e2 copy and lane bridges, N11 (straight), under w1 x2, under w2 x2, Shift E under J_e's bridge, N9 (straight), port.
  - J_e1: N2 (straight), over X_e, under w2 and w1 x2, N3, signal, Corner R under e2, Run e1a, Shift into N13.
  - J_w2: Shift W at N2, N4, Corner L onto w2a, Shift into N8.
  - T_w2 with its doubling over four crossings.
- **Band y orders:** band 2 = N11/N3 above w1pc with their bends. Band R must put X_e's Shift above the J_w trunk's Shift.
- **Negative tests:** a crossed-trunk topology is refused; the N-side refusal is kept.
