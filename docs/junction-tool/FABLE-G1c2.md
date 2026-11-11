# FABLE-G1c2: review of DESIGN-G1 §11 (G1c(ii): placement and render of the seed) (at 142a074c6f)

Read-only review. Sources: DESIGN-G1 §10.6-§11, src/junction_topology.h, src/tests/junction_topology.cpp:237-400 (orders, bands, stacks, bridges, programs as tested), src/junction_nodes.cpp:140-560, :688-706, :903, RECON-G1c-seed, RECON-turn-programs §1-§4, FABLE-G1c, DECISIONS D80 D110 D113 D115a D118 D145 D148 D172 D192 D203 D205 D206. Arithmetic on the two topologies is mine and marked INFERRED where it rests on the tile pattern of a diagonal (taken from the T's D1/D5: the turn-direction axis moves first, RECON-turn-programs §1 P6.4).

**Verdict.** The skeleton is right: a difference system with a least solution, exact rigid parts, render from the programs. But §11.1 classifies "rigid" wrongly. In the builder, every straight run that ends at a bridge head is rigid (junction_nodes.cpp:207-208), not only the head-to-head span. §11.3/§11.4 then put the loose G on exactly those runs: layer heights, lane split to head, head to lane merge. The builder freezes G there, Lo cannot tighten it, and §11.6(5) (counts only) cannot see it. For the **T**, band R comes out about G rows deeper than the trace, with a frozen 1-row-too-long single bridge. For **LL_RR**, G spreads the crossers under w1's and w2's bridges past nmax. Even at the exact rule minimum, w2's bridge is 11 > nmax 10, and nothing in §11 checks spans. The placement is not yet right for either topology. Fixes: F1-F4 make the T exact; F5 is a structure question for Pete.

## Findings

**F1 [wrong] A run touching a bridge head is rigid (§11.1).** Points sit at the first and last piece on a bridge (:163-170). A link counts as free only if no piece from a to b inclusive is `on_bridge` (:207-208). So the link from the last point before a near head up to that head is rigid, and so is the link from a far head to the next point. Their seed lengths become in-class offsets (RECON-G1c-seed 1a/1b).
- **T, rigid besides the spans:** lane A (31,3)->(29,3); lane B's S-bend at the lane split into its head (30,4) (D110); far ends (23,3)->(22,3) and (24,4)->(22,4); N->E (26,11)->(26,9) and (26,6)->(26,4).
- **LL_RR:** J_e trunk (edge 3) from N2 to its near head is rigid. Its far head to N3 is **one rigid link**: the pieces under w2's and w1's bridges are not points (no direction change, no event; a point is keyed by tile and trackdir, :155-175; INFERRED). That link spans band R layer 3, the w2 triplet, band 3, the w1 triplet and band 2 up to N3. The same holds for every D113 leg of e2, w1 and w2.
- **Fix:** §11.1 should read: free = a straight run with no bridge piece, or a diagonal of 3 or more halves not under a Y-axis bridge. A bridge assembly (switch or bend, the D113 piece, head, middle, head, the D113 piece, the next point) is placed exactly at D113's one piece per leg (rigid freezes, so the minimum is the only right value).

**F2 [wrong] Layer heights G are frozen through the carriers (§11.3).**
- **T:** N->E's whole column from the N split (row 11) to its corner start (row 4) is rigid (F1). The trace's depth follows from that chain: y(corner start) = y_a + k = 4; far head = +2 (one D113 piece, (26,5)); near head = far head + n_B2 + 1 = 9; N split = near head + 2 = 11. So band R depth = y_a + k + 5 + n_B2. In the draft, layer 1 is G = 2·TL + p = 22 rows, so the N split sits about 21 rows lower and that is frozen. The trace's layer 1 is 1 row: (26,10) and (28,10) are each one D113/G6 piece.
- **LL_RR:** band 2 layer 1 and band R layer 3 lie inside J_e's rigid far-head-to-N3 link. Band 2's rule depth is k rows: J_e2's Corner from N3 onto e2a takes 2k+1 >= h halves. With G, w1 and w2 sit about 20 rows too low, and that is frozen.
- **Fix:** every layer height is derived exactly from the bends, D113/D192 pieces and stub it holds. G is allowed only on a layer that no rigid link spans, and none exists in either topology.

**F3 [wrong] The row chain's G between the exit split, the lane split, the heads and the lane merge (§11.4).** These are rules, not loose gaps:
- exit split to lane split = 2 (D110(3), D145 x_sl - x_ls = 2);
- lane split to the near head: copy 0 = 2, copy 1 = 1 (D145, D118);
- far end: copy 0's S-bend straight after its head, copy 1 one plain piece (T: (23,3)|(22,3); (24,4),(23,4)|(22,4)).

With G:
- **T:** the B->S column moves to 31 + G - 2 = 51, so D4 grows from 6 to 46 halves. It is a free diagonal, but m_min and keep_slot read its halves (:427, :501). The D3/lane-B tile sharing near (31,3) also changes flat_bits (RECON-G1c-seed §3). The lane-side legs are rigid (F1), so G is frozen there.
- **LL_RR:** Xe2 = N1 + k, and Xe1 >= N1 + G + 3, so Xe1 and Xe2 sit G+1 apart under w1's and w2's rigid bridges. Likewise X_w1 = N6 - k >= c + G + 1 under w2's bridge. Both spans are far past nmax 10.
- **Fix:** mark these as exact in §11.4. Only the fan zone to the exit split, the lane merge to the joiner merge, and the runs into ports stay loose.

**F4 [wrong] Single bridge span and the crossing layer height (§11.3, §11.4).**
- **T:** "Heads one row beyond the crossed S-bend's rows" puts B2's heads on rows 6 and 10 for D2 (rows 7-9): middle 3 against the trace's 2 (heads (26,9), (26,6)), and B2 is frozen.
- **T:** "Height = columns moved + 2·K_HEAD + 2" = 3 + 2 + 2 = 7 against the trace's 6 ((26,10) to (26,5)).
- **Rule:** the carrier passes over the crossed diagonal's tiles in its own column. That is 2 tiles when the column lies strictly inside the S-bend, as at the least placement (j = 1: T 26 vs 25..28; LL_RR c vs c-1..c+2). So middle = 2, heads at those rows ±1, and the extent along the carrier = 2 + 2 + 2 = 6, whatever the columns moved. The S-bend's last row may share the near head's row (T row 9).
- **Also wrong:** "band R lies below the last item". The carrier's D113 piece shares the last stack row where that row's track is x-disjoint (T (26,5) on row 5; N->W's approach is x 7..19).

**F5 [missing] No span limit. LL_RR's w2 bridge does not fit at the test spec.** Bridge 10 carries w2 over {13, 3, 11, 12, 17}.
- **Rule minimum (INFERRED):**
  - Xe2 = a and Xe1 = a + 3 (D145 on e2: hw0 = Xe1 - 1, x_ls = Xe1 - 3, N1 = Xe1 - 5);
  - c >= a + 4 (slice);
  - X_w1 >= c + 3: the X_w trunk lies east of X_e at c + 2 below the swap, and X_w1 is N10's straight child;
  - J_w1 at j <= a - 1, whose Corner onto w1a puts halves j-1, j-2 on row 9 and j, j-1 on row 10 (the T's D5, mirrored).
- **Result:** w2 heads west (u = +1), so hw0 = j - 3, he0 = X_w1 + 1 and n = X_w1 - j + 3 >= 11 > nmax 10. The D172 window is also exceeded: hi - lo = 10 > D = 7 (RECON-turn-programs Q8 is still open on which bound applies).
- §11.6(3) would pass this. D203 limits a shared bridge "within its span limits", but §9 picks the set without geometry.
- **Fix:** add n <= nmax as an upper-bound arc. A positive cycle means refusal, naming the bridge. Splitting the set is Pete's call (D203). The T is fine: W's n = 5, B2's n = 2.

**F6 [missing] Columns are least only by accident.** Slices give `>= 1`, and the least solution packs a pair of columns only if no other chain pushes the eastern one.
- **T:** N col >= AS + 1 also has a lower bound from N->W (approach start + k_D7 + 1 >= x(N3) + 8). AS wins only because its chain carries two G's (x(N3) + G + 3).
- **What breaks if not:** a bigger N->W bound would widen W's rigid bridges and D2, which is rigid under a Y-axis bridge (:212).
- **Fix:** make it a checked postcondition, not an equality: an equality is infeasible for LL_RR, whose X_w1 needs c + 3. Check that each crossing S-bend moves (items passed + notation offset) columns (T 3, LL_RR 3) and that each bridge's Z width equals its packed width.

**F7 [wrong] G is a hedge (§11.1).** For a free link any length works: constants cancel (1c, 1e) and Lo removes it. What a free run must hold is its signal slot. Rules 4-6 look for the last slot between anchors and **silently skip** when none exists (:523, :533, :548). So the derived lower bound is: each free run holds the one legal piece its rule keys on (keep_slot keeps 1 piece, :491-505). State that, drop 2·TL + p, and never use G on a link F1 calls rigid.

**F8 [missing] Tests.**
- §10.6 promised "the T clean under CheckJunctionRules" for (ii). §11.6 drops it, and the output has no signals; say which step places them.
- §11.6(2) is wrong for D142's parallel diagonals at S = 1: two tracks share a tile. Example: LL_RR's W fans for e1 (2 rows) and e2 (1 row) start at a common column. Test track bits, not tiles.
- §11.6(5) compares counts, so F1-F4 all pass it. Compare rigid links instead (the in-class offsets).

**F9 [wording]**
- §11.4 "x(column) = x(lane split) ± k" should say the **exit** split (FABLE-G1c F8). The doubling's lane split is a different node.
- §11.3 "exit Corner from lane row y" should say "from its upstream item": doubled lanes split on the copy row (T B->S on row 3; LL_RR X_e2, X_w1, X_w2 on rows 3, 6, 9).
- Corner convention: in the T the joiner's last half is (28,2) and the run starts at (29,2), so "approach run start = column + k" is the last half, not the run. Define the corner's end tile once, as RECON-turn-programs' conventions do for nodes.
- h: cite JunctionCurveGaps (min(2TL - 1, max(2, CL)), odd). CL 2 parity is still open (RECON-turn-programs Q2).

## Answers to §11.7

1. **One G?** Yes for truly free links. But §11 applies it to rigid ones: layer heights (F2), the exit split to lane split to head legs and the far legs (F3), single spans (F4). Corners never move under a bridge: their x is exact from the column, and the heads follow Z. G does move Z apart in LL_RR (F3), which grows rigid spans.
2. **Slices.**
   - "Before and after each layer's events" give the right order in both topologies (checked: the T's D7 passes column 25 at rows 10-11, below AS's D2; J_w2's and J_e2's F4 Corners sweep outward).
   - They do not cover a diagonal item's x varying with y, or a column against a side track's x range (approach and copy runs). Add both as checks.
   - Rigid S-bends are least in both topologies only by chain luck (F6).
3. **Span with diagonal crossers.** "Least equal span, stagger 1" is D80 + D118 + D145, and the T confirms it. Write it without s, s'. With u = the upstream step (+1 heading W, -1 heading E):
   - hw0 = min(Z0 ∪ (Z1 - u)) - 1 and he0 = max(Z0 ∪ (Z1 - u)) + 1;
   - copy 1's heads = copy 0's + u.
   - T: Z1 - 1 = {24, 25, 26}, union {24..28}, so heads (23,3), (29,3) and (24,4), (30,4), n = 5. Exact.
   - Then x_ls = he0 + 2u and the lane merge = hw0 - u, by D145.
   - Tie the heads by equality to the extreme crosser tiles, which are known offsets of known columns. Add n <= nmax (F5).
4. **Seed reads (RECON §3).**
   - Joiner row sign: right. The approach is adjacent and south, so the reserved prio is north (:367); the T and LL_RR agree with the stacks.
   - Slots: wrong under G on diagonals (keep_slot :501 reads end - slot and halves); fine once F3 fixes D4's size.
   - flat_bits: needs D145's exact near end (F3).
   - Occupancy: min(2, seed gap) (:903) is unchanged. The T's adjacent AS/NE columns stay at gap 1.
   - AddBoxEdges: right (W at 0, E at one x, S at the S edge; the e1/A rows are the min-y ends).
   - The anchor (:688) and role-based rules 5 and 6 are G1d for LL_RR.
5. **Split (ii)?** Yes. (ii-a) has pure numbers: the T's expected exact coordinates are the trace's for every rigid part (nodes, heads, corners, S-bends, the band R depth of 11), with order-only checks on free runs. LL_RR (ii-a) should expect the F5 refusal until Pete rules.

## Suggested tests
- T (ii-a): B2 heads at rows 9 and 6; W heads at 23/29 (row 3) and 24/30 (row 4); N split and S merge on row 11; lane split 31, B->S split 33; AS col 25, N col 26, S col 28.
- T: each rigid link's length equals the traced template's (non-free links, in-class offsets).
- LL_RR: placement refuses bridge 10 (n 11 > nmax 10) with that text. Separately, D145's near-end constants give Xe1 - Xe2 = 3.
- Both: every crossing S-bend moves its packed column count (3); every span <= nmax; occupancy by track bits.
