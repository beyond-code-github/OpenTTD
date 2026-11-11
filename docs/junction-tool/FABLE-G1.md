# FABLE-G1: independent review of DESIGN-G1 (draft at 3a51984978)

Read only. Lines are src/junction_nodes.cpp unless a file is named. Frame used below: the canonical frame of the T (y down, left-hand running: E-bound lanes north, W-bound south; both branch ports on the S edge, branch-in west of branch-out, as FindJunctionPorts assigns them, junction_rules.cpp:174).

## 1. R1-R8

### 1a. Today's T: the rules reproduce the paths and crossings, but not the node order or the rows
- R1, R2, R4 give the T's 6 movements, 7 paths (the W line has 2 copies), and the crossings (a) W over A->S, (b) W over N->E, (c) N->E over A->S. N->W and B->S cross nothing (their chords are nested). Correct.
- **R3 gives no answer on the W line.** Both of its splits (B->S, and the lane split of the doubling) have legs leaving toward S, so "nearest side first" cannot order them. The T's order B->S, then lane split, then bridge, then lane merge, then prio merge has a different source: B->S must not cross A->S or N->E, so it leaves before the crossing group, and the doubling brackets the crossing group (D145 constants). That is an order derived from the crossings, not from sides.
- **R7 cites the wrong decision.** D28 says the prio goes "outside" (on the far side from the other direction). The code puts it opposite the joiner (:459-462; junction_signal_pass.cpp:277, "D75 and D87"; DESIGN-N3 §8). The T's W prio row is row 3, on the *inside*, between W (row 4) and A. So R7's wording matches the code, but its source should be D75/D87, not D28.
- **No rule gives the T's rows.** Inside the box the W line drops one row: the S-bend D9 at x_w+2 (RECON-turn-programs §2) frees row 3 for its prio. N->E runs along the gap row and N->W along row 5. R6 says that row changes happen, but no R says which row each track, copy, prio and joiner approach takes. D189 lists "rows" as derived by rule; G1 has no such rule. Section 1b shows why this is the main gap.

### 1b. LL_RR main line + L_R sideline (D186-D188)
Rows from north to south: e1, e2 (adjacent, D142), gap, w1, w2. Port slots counter-clockwise from the north-west corner: 0 e1in, 1 e2in, 2 w1out, 3 w2out, 4 Bin, 5 Sout, 6 w2in, 7 w1in, 8 e2out, 9 e1out.

**Paths (R1), 12 plus the doubling copies:**
- through T_e1 (0-9), T_e2 (1-8), T_w1 (7-2), T_w2 (6-3);
- exits X_e1 (0-5), X_e2 (1-5), X_w1 (7-5), X_w2 (6-5);
- fan joins J_e1 (4-9), J_e2 (4-8), J_w1 (4-2), J_w2 (4-3).

**Node order per line:**
- e1: s1, then m1.
- e2: s2, the doubling split, bridge, doubling merge, m2.
- w1 (westward): t1, the doubling group, n1.
- w2: t2, one or more doubling groups, n2.
- Branch-in: a direction split, then two fan splits. R3 is silent on this order; "destination before line choice" is the obvious rule to add.
- Exit merge tree: per direction first (D187), then the two directions. R1 does not state the tree.

**Crossings by the chord rule (path pairs, 16):**
- X_e1 with T_e2, T_w1, T_w2, J_e1, J_e2;
- X_e2 with T_w1, T_w2, J_e1, J_e2;
- X_w1 with T_w2;
- J_e1 with T_e2, T_w1, T_w2;
- J_e2 with T_w1, T_w2;
- J_w1 with T_w2.

T_e1, X_w2 and J_w2 cross nothing.

**Physical crossings and bridges (main line over):**
- The lanes are crossed 12 times: e2 by 2 legs, w1 by 4, w2 by 6. Every leg crosses both bridge copies of its lane.
- Leg-on-leg crossings X_e x J_e: 4 single bridges if each crosses as a separate leg, or 1 if both cross on trunks (the X_e merge before the crossing, the J_e fan split after it).
- w2 cannot hold all 6 crossings under one bridge group. X_w1 crosses it just after w1's split (east). J_w1 crosses it just before w1's merge (west), and D108 keeps w1's doubling switches out of that merge's coverage. So w2 has 2-3 groups.
- Main-line bridges: e2 2 + w1 2 + w2 4-6 = **8-10, not the 4 that D188 states.** D188 counted one crossing site, the W-bound pair. The **sibling crossings** (X_e1 x e2, J_e1 x e2, X_w1 x w2, J_w1 x w2) are forced by the chord rule because the lanes are adjacent and there are no shifters (D191). They are a new cost the engineer has not seen. Put this to him as a card: confirm main over here as well, or let the leg take the grade separation.

**Where the rules give no answer or a wrong one:**
- **R4 counts per path pair; it must count per graph edge.**
  - J_e1 crossing e2 west of s2 crosses the trunk shared by T_e2 and X_e2: one physical crossing for two path pairs. Crossing east of s2 needs a second, leg-on-leg crossing (J_e1 x X_e2) below.
  - Both are valid. Which one applies is a node-placement choice that no rule decides, and it changes the bridge count.
  - Copies also multiply crossings: one path pair gives two crossings on a doubled lane.
  - R4's second clause ("shared node, swap sides") is not needed for the fan case. Sibling crossings are ordinary pairs with no shared node, e.g. (0,5) against (1,8).
- **Grade separation is total, not "a diagonal can't".**
  - G2 forbids X+Y level crossings (junction_rules.cpp:417-423).
  - The game accepts any track combination on a tile (rail_cmd.cpp:240-256). Only UPPER+LOWER, LEFT+RIGHT and X+Y share no edge (track_func.h:540-548 lists the overlap cases). So a diagonal or a curve on an axis lane's tile is a switch, not a crossing.
  - Every crossing pair is therefore a bridge or a tunnel.
- **R3 is silent on:**
  - s2 against the X_e1 crossing on e2 (both work);
  - the branch-in split order;
  - the exit merge tree;
  - which fan leg is on the west.
- **R7 is wrong for the inner lanes.** Every joiner comes from the south, so every prio row goes north. That gives e1 north (free), e2 the e1 row (taken), w1 the gap row, and w2 the w1 row (taken).
- **Rows conflict.** With D142 adjacency only three free rows exist next to lanes: north of e1, the gap, and south of w2. The following all compete for them:
  - doubling copies for e2, w1 and w2 (3);
  - prio rows (4);
  - joiner approach rows, like the T's N->E on the gap row (J_e1 has no row between e1 and e2, and J_w1 none between w1 and w2).

  Either the bundle spreads apart inside the box (as the T's W line already does by one row), or prios and approaches use diagonals under bridges. **This is the missing rule, and it is an engineer question, not something to infer.**
- **R8 misses:**
  - D100/D105 equal cost for the two fan legs (J_e1 crosses e2, J_e2 does not, so their curve counts may differ, which no difference constraint can express);
  - D80 equal copies;
  - D108 coverage free of switches;
  - CheckOrder.
- R6 is fine. Parity follows from the turn: same-hand corners are odd and opposite pairs are even (RECON-turn-programs §2 table: D1/D3/D5/D7 odd, D2/D4/D6/D8-D11 even). The diagonal's dx is the variable, so the half count stays a difference constraint.

## 2. OPEN-1 and OPEN-5
**Straight chords (OPEN-1).**
- **Sound for:** which pairs cross. Straight chords in a convex box cross exactly when they interleave.
- **Gives a consistent order** (any straight drawing is realisable). Along each lane it puts every split first, then the crossings, then the merges. That is the order D189 and the no-cross constraints force anyway; for example J_e1 must cross w1 west of t1, or it would cross X_w1. So no geometry is needed to get that order.
- **Not sound as stated:**
  - Nodes collapse to ports, so a leg never crosses a shared trunk. The embedding silently picks the crossing-maximal choice of R4 (4 X_e x J_e bridges, not 1-2).
  - It has no rows. Doubling copies, prio rows and joiner approaches do not exist in it, so it cannot give "the order of elements on a row" (R5's claim).
  - It depends on assumed slot positions and box size, which do not exist yet.
  - The check on the T proves nothing: with three crossings every order is forced.

  Replace it with a combinatorial rule: per lane, splits, then crossings in the x order of the legs' far ports, then merges; per pair of legs, an explicit choice of crossing as legs or on the trunk; rows from a stated row rule (§1b). Test it against a hand-made LL_RR expected structure, not the T.

**Option (b), seed then solve, is unsound unless the seed is already a valid drawing.**
- **Run minima do not close cycles.** Every run at its own minimum disagrees wherever paths share nodes, crossing points or equal copies. The T itself shows this: A's west run is 23 tiles, set by the west chain x_e = x_w + 40, not by its own minimum (RECON-turn-programs §4).
- **The builder trusts the seed for structure:**
  - tile identity (flat_bits, FABLE-topology-graph §1);
  - which crossings get constraints: only those under a bridge *in the seed* (AddCrossings :561-630);
  - box extremes (AddBoxEdges :632-656);
  - prio sides (:459-462);
  - occupancy order (:885).

  A wrong seed therefore yields *missing* constraints, not failures.
- **The solve cannot repair it.** The snap returns the seed (the "keeps the flat T" test). One pass only works if the seed needed no repair.

**Option (a) as written is not a difference system.** Variables per run length with sums along paths give closing equalities over sums, which needs LP/ILP.

**Recommend (a) over node coordinates:**
- the template's own classes, with offsets inside each class taken from the structure (run directions, the raster of a diagonal's half count, S-bends) instead of `pos - flat`;
- crossing windows, box edges and prio sides emitted from the structure;
- occupancy written as per-row order constraints from the stated row order.

Then:
1. Take the least solution (JunctionPositions; a positive cycle is a clear structure error).
2. Render it.
3. Run the G1a rule builder on the render and require a fixed point: same template, solve on flat == render, no collisions.

So (b)'s "build, solve on flat" becomes the **validator** (FABLE-topology-graph §3 step 4), not the generator. Also fix §3's wording: a least solution puts each class at its least *coordinate*, not each run at its minimum *length* (a run whose end is pushed by another chain keeps its slack).

## 3. OPEN-2 and OPEN-3
- **OPEN-2 misreads the rules.**
  - D176 allows "whole bays, **or** the leftover carries no signal". D192 then puts a signal right after every line-choice split and accepts that a train hangs over the split only when every line is red.
  - Together they make a bay leg a plain lower bound: at least n whole bays from the merge signal back (D83 grid), with the leftover at the split end. That is a difference constraint with no discrete k.
  - The engineer question should be: "with D192, is leg >= n bays + leftover acceptable, and is n = 1 for the first LL_RR?" It should not ask about "shortest whole number of bays".
  - The bay legs are J fan legs and X exit legs (D187). Doubling-bridge legs are not, per D192.
- **OPEN-3: most items are not differences to accept.**
  - Q1 (14 vs 15 split/join convention) and Q3 (lane merge 15 vs 16) must be ruled before G1c, or the oracle has nothing to compare against.
  - Q2 (CL 2: JunctionCurveGaps allows 2, a same-hand corner needs an odd count >= 3) is a rule bug.
  - Q6 (at CL 6/7 the A->S leg reaches the N column) is a **structure** change (order or crossing), not a length, and G1c tests 196 km/h.
  - Instead of a list of named exceptions, use two oracles: structural equality with the traced T (paths, node order, crossings, turns, rule kinds per pair), plus the exact fixed-point check of §2 on the generated T.
  - The compact T also moves the hub and size that junction_game.cpp reads (FABLE-topology-graph §3 item 6).

## 4. Steps G0-G1e
- **Missing:**
  - the validator (render, collision check, retrace, compare) as a gate on every generated template in G1c and G1e;
  - an engineer card for rows, sibling-crossing bridges and the bay reading before G1b;
  - a hand-made LL_RR expected structure (§1b) as G1b's second oracle. Without it, LL_RR structure errors surface only in G1e.
- **T-only code that G1 never names:**
  - IsMainLinePath (:111-114);
  - the anchor "rule 5" EIn->SOut (:668-676; ambiguous with two lanes);
  - prio detection by role (:367);
  - the builder's FindJunctionPorts and JunctionTMovements call (:661);
  - IsLaneMerge by role (junction_signal_pass.cpp:270-275);
  - the checker's is_ml tests (junction_rules.cpp:435, 515, 695, 883, 1094, 1544).

  These belong in G1a or G1b (builder) and G1d (checker). OPEN-4 covers only the checker.
- **Too big for one test-first step:**
  - G1b: split it into (i) the topology type, data, R1 and R4 per edge, tested on both topologies; (ii) orders, rows, sides and turns.
  - G1c: split it into (i) reference, render and validator on the T; (ii) solves at 4 speeds and a clean checker.
  - G1d: the checker work is large (CheckWaitingRoom first split/merge, CheckPrio "one prio component" :1154, the ML-bridge rule at :507-522 passing without doubling, first-path consumers, new D176/D186/D188 rules).
- **G1e assumes signals exist.** The signal pass (IsLaneMerge, prio geometry) is T-shaped, and D192 says to read the coop fail-safe material first. Add a step for the LL_RR signal pass before "rule-clean by the checker".

## 5. Other gaps
- **Signals:**
  - not mentioned at all;
  - the D113/D192 signal pieces and where the SD grid aligns across sibling lanes (RECON-llrr-facts §3 Q9) are open.
- **Prio rows for fan joins:**
  - R7 fails for e2 and w2 (§1b);
  - a J_e1 approach between adjacent lanes has no row;
  - the checker's "one joiner per merge" fits, but the prio row placement does not.
- **Box depth:**
  - derived only implicitly. AddBoxEdges reads extremes from the reference, the same circularity as OPEN-5;
  - the structure must state: both branch ports on the S edge at one row, every other element strictly inside.
- **Ports with more than one lane:**
  - FindJunctionPorts returns nothing unless an edge has exactly 2 tiles (junction_rules.cpp:160) carrying only the axis track (:162), and assigns roles by sorted order;
  - JunctionPortRole has one role per direction, so two lanes share EIn->EOut and the ML-bridge rule passes without doubling;
  - ports need lane identity (side, group, lane index), not roles;
  - ParseJunctionNotation also rejects LL_RR (junction_generator.cpp:44-65).
- **Doubling copies as paths:** whether copies are separate paths (the traced T) or one bundle edge (DESIGN-nodes §7) decides the crossing count and the equal-length constraints. G1 should say which.
