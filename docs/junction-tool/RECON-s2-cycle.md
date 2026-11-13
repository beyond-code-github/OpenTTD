# RECON: why placement still refuses L_R x L_RR and L_R x LL_RR with the S2 change

Worktree .claude/nodes (wt-s2), uncommitted S2 diff (saved as s2-stopped.patch). Probe: a hidden test dumping graph, programs, bridges, plus a debug print of the explained constraints in `solve`'s failure branch; both reverted (git diff == saved patch, checked with cmp). "jp" = placement/junction_placement.cpp (line numbers of the patched file), "jt" = topology/junction_topology.cpp. Edges/nodes are the graph builder's numbering.

## 0. Verdict

Class **(d)**, a new one: **placement misreads the single S-bend token of the straight-at-the-split leg as a split-diverge, so the leg's S-bend is rigid between the two nodes**. Not (a) (hands are right), not (b) (the program is right by jt's own rules), not (c) (the bridge pin is not what binds). The failure is in the **Y phase of the first round, after the flat X solve** (phase=1, rows_known=1, fixed=0), not in the flat X solve. REVIEW-port-fans F1-F7 are not involved (the fans are on out edges after the merges; no fan is in either cycle).

## 1. Graph, programs (L_R x L_RR; the L_R x LL_RR twin in brackets)

Slots: 0 W in, 1 W out, 2 S in (one lane), 3,4 S out lanes 0,1, 5 E in, 6 E out [LL_RR: 2,3 S in, 4,5 S out, 6 E in, 7 E out]. Levels: n1 = 2 (band 1), n4 = 4 (band R), n6, n7 = 4. [n5 = 4, n8, n9 = 4.]

Edges in the cycle (heading south, "west" = hand +1 when heading south):
- e11 [e14]: E-in split n4 [n5] to S out lane 0 merge n6 [n8]. Paths 5->3 [6->4]. Program: SBend(+1), SBend(+1), Under(e6) [Under(e7)], SBend(+1). The E in leg to the west lane: three west S-bends, the middle one under the bridge.
- e12 [e15]: n4 [n5] to S out lane 1 merge n7 [n9]. Paths 5->4 [6->5]. Program: **one** SBend(+1), nothing else. The E in leg to the east lane.
- e6 [e7]: W-in exit split n1 to n7 [n9]. Paths 0->4 [0->5]. Program: Under, Under, **Over(e11)** [Over(e14)]: a straight leg heading south that bridges e11's middle S-bend; bridge over structure.bridges: edge 6 [7], single, crossed [11] [[14]].
Other edges (e5/e9/e10 ... the W side) are not in the cycle. Dump: /tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/probe.err.

## 2. The cycle (Y phase; x[] = a row; "S-bend m" = columns moved from the first X solve)

Points (L_R x L_RR): 11 = n4, 14 = n7, 8 = n1, 39 = e11 S-bend 0 end, 40/41 = e11 S-bend 1 (under the bridge), 30/31 = e6's bridge heads. Weights are rows, constraint `y[v] >= y[u] + k`.

| # | relation (jp reason) | constraint | jp |
|---|---|---|---|
| 1 | S-bend size e12: m = 2 from X, extent m-1 exact, **first = n4, last = n7** | y14 = y11 + 1 | 273, 1488-1500 |
| 2 | S-bend size e11 tok0 (m = 1, first = n4) | y39 = y11 | same |
| 3 | chain run e11, S-bend 0 to S-bend 1 (floor 2, D217) | y40 >= y39 + 2 | 1543 |
| 4 | bridge over S-bend: head 1 = crossed.first + 2 | y31 = y40 + 2 | 1567-1581 |
| 5 | chain run e6: far head to n7 (floor 2) | y14 >= y31 + 2 | 1543 |

Loop y11 -> y39 -> y40 -> y31 -> y14 -> y11: 0 + 2 + 2 + 2 - 1 = **+5** (y14 >= y11 + 6 against y14 = y11 + 1). L_R x LL_RR: identical shape, nodes n5 (11/12), n9, e14, e15, e7, points 47-49, 38/39: S-bend e15 y16 = y12 + 1, runs 2 + 2 + 2: **+5**. Reported text lists each link twice (mirrored).

## 3. What is impossible, and why fans do not absorb it

The W-in leg to S out lane 1 (e6 [e7], straight, column c-2) and the E-in leg to lane 0 (e11 [e14]) must cross; e6 bridges e11's second S-bend. That S-bend lies at least S-bend 0 (1 row) + run 2 below the split n4, and the bridge heads another 1 + 2 further, so e6 ends at least 6 rows below n4. But e6 ends at n7, and n7 is fixed exactly 1 row below n4 by e12's S-bend, which jp makes the whole stretch n4 to n7 (jp:272-273: first = the split's point, last = the merge's point). So n7 is boxed in from above (the bridge needs 6 rows) and pinned from n4 (1 row).

Fans are on e14/e15 [e18/e19] (merge to port), after n6/n7 in the travel direction and in the other direction of the pinned span; they change X between merge and port, never the Y distance n4 to n7, and neither fan is in the loop.

## 4. Classification and evidence

- (a) hand/sign: **no.** e11/e12 S-bends are hand +1 heading south = west, and ports 3,4 [4,5] and merges lie west of the E-in split; e6's bridge column etc. agree. S0's heading rule holds (`cur` south for exit paths, jp:245-250).
- (b) program order: **no.** At n4 [n5], jt:1086-1090 `diverges`: the two legs tie in level (both merges at level 4), the straight leg is `sibs[0]` = the one to the farther slot = e12 [e15]; e11 [e14] diverges west (its SBend(+1) first). At the merge n7 [n9], the straight leg is the one from the lower level (e6 [e7], n1 level 2), so e12 [e15] diverges at the merge: jt:1213 end rule `d.`. So e12's single SBend is the **merge diverge at the end of a straight run from n4**, and e11's third SBend is its merge diverge for n6. The program says "straight from n4, then S-bend into n7".
- (d) the cause: jp:272-273 sets `first = start` for token 0 of an edge from a node and `last = stop` for the last SBend into a merge. For a one-token SBend that is both, so the S-bend spans exactly n4 to n7 with no run before it (the exception `t == 0 && !from_band_split -> new_pt()` at jp:276 only frees the first for non-band-split sources). Placement cannot tell a split-diverge token from a merge-diverge token because the program does not say which end a single SBend belongs to; it assumes the start.
- (c) bridge pin: **not binding.** Relaxing it cannot fix a +5 cycle whose edge #1 is the rigid S-bend (the pin supplies 2 of the 5).
- Experiment (temporary, reverted): in jp:272 give a band-split edge's lone SBend token into a merge a fresh `first` (so a free run n4 to bend start). Result: the +5 cycles vanish for both topologies, and L_R x LL_RR reaches a different Y failure ("curve gap (edge 17) <- S-bend size (edge 9)", curve gap 9 vs S-bend extent 5, +4: the W-out joiner/e9 side, not this fan or crossing); L_R x L_RR threw `basic_string::_M_replace_aux` from my crude hack (not analysed; INFERRED an artifact of the hack, since the hack gave no reason text). So this is the first, not the last, barrier; the others are INFERRED unrelated to the fans.

## 5. Smallest rule change

Make a band edge's SBend token carry which end it serves: placement (not the program) reads "an SBend whose edge starts on a band split and ends on a merge, and which is the only SBend token, is the merge diverge unless `diverges(e, from)` says otherwise", equivalently the program gains a token role or a Fan-like end marker. The least change that needs no new decision: in jp, take the straight leg at the split from the same tie rule as jt (`sibs[0]` on a level tie) instead of inferring from token position, then the first point of a straight leg's S-bend is a new point with the exact node-to-bend run (D217). It rests on G1 §11.8 ("a split is the tile of its first diverging half piece"; a straight leg at a split has no diverging piece at the split) and D204/D187 (a merge is the tile of its last converging piece). Pete's call whether it is a placement fix or a program token change (D203 token order stays). Doubts: INFERRED that the fix is identity-safe (every identity topology's one-token band legs would need the same check); not run.
