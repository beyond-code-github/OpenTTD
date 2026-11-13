# REVIEW-port-fans: review of D230, briefs/SF1-south-fans.md and RECON-llrr-placement-contradiction (at 04122bcd34)

Read-only review. "jp" = src/junction/placement/junction_placement.cpp, "jt" = src/junction/topology/junction_topology.cpp. Edge numbers for L_R x LL_RR come from the graph builder (jt:279-376: in trees e0-e5, one leg per path e6-e15, out trees e16-e21). They match the recon (e2, e3, e9-e12, nodes 7 and 11) and the SF1 run's text (e6, e7, e14, e18, e19). The SF1 stopped patch was read as evidence. Nothing was built or run, so every runtime claim is INFERRED from code.

## Verdict
The fan is the right kind of fix for the in-side cycle, and the halves hand rule is not lane-specific. It is the difference-constraint form of the rule the W/E fan zones already follow: "lanes S-bend outward monotonically", with the port block between its lanes' inside ends (G1 §10.2). D230 should say it that way. Two of its claims are wrong (F5).

Neither failure the SF1 run hit after the in side is about fans:
- The out-side X cycle and the Y cycle both come from the band-edge chain builder. It starts every In edge heading north, every Out edge south and every band-node Leg north (jp:245). That is wrong for exactly the edge shapes that multi-lane S groups bring: exit cascades and D224 band merges.
- Flipping the out-fan hands hides that bug and would make the two out fans cross.
- The recon's LL_RR +2 cycle partly rests on the same bug.

SF1 as briefed also runs into the "S edge" relation in Y, and it starts each out fan on its merge tile. The order is in Q4. Fix the heading and the reason label (identity-safe). Then make "S edge" per port. Then land SF1 with halves on both sides plus two corrections. Then add a minimax objective, copied from W/E's `best_offset`.

## Findings

**F1 [wrong] Band chains start on the wrong heading for every edge shape that multi-lane S groups add (jp:245).** `cur = In ? north : Out ? south : row node ? row heading : north`. The programs assume otherwise. They put a Corner of the row's hand at a row-node start (jt:1158-1160; G1 §10.2: an exit leg Corners "to heading S"), and their diverge and crossing hands assume travel away from the far end (jt:1086-1089, 1138).
- (a) e1 (In, exit split n0 on row 0 to lane split n1 in band 1). The chain starts north, so `Corner(+1)` turns north to east and its last point lands k rows north of row 0, at y < 0. That is the run's Y failure "Corner size (edge 1) <- ... <- stack origin <- origin" exactly.
- (b) e6 (Leg, n1 to n8, W in to S out lane 0) carries `SBend(+1)`: it diverges at n1, and e7 is the straight leg (jt:1076-1088). Heading south, +1 is west. The chain reads it heading north, so it goes east, which gives x(n8) >= x(n1) + 1 = x(n9) + 1 (e7 is straight: runs and heads exact). That forces S out lane 0's merge east of lane 1's, the opposite of the fans' lane order, and it is the run's out-side cycle (S-bend e6, chain/heads e7, fan e19, spacing, fan e18, run e6). The "east S-bend" at e14's end is the same misreading of +1.
- (c) e17 (Out, band merge n7 to W out joiner merge n6) and e21 (n11 to n10) start south. Their arrival Corner then turns the wrong way, and Y must fail, because n7 lies below row 1 (jt:400-405).
- (d) LL_RR x LL_RR e3 (In from row node 2). The recon's "runs east in band 2" and "horizontal S-bend, no X extent" (its relations 6-7, the +2) come from this bug. The program means Corner east to south, then vertical S-bends.
- Fix shape: `cur` = the row heading when the edge starts at a row node; else south when the edge's paths come from a main slot, north when they come from the S side. A band edge carries only branch paths of one direction. Programs' `heads_south` (jt:1138) needs the same rule, because equal levels (n5 to n8 inside band R) read as north.
- Every edge shape placed today maps to the same `cur` (S port In, S-tree band nodes and legs: north; legs from row nodes: row heading; Out to S ports and S-out band merges: south). So the fix is identity-safe. INFERRED; confirm with `jt identity`.
- Side note: on a tie in level, the straight leg is `sibs[0]` (jt:1087). For a south-heading exit split that is the east leg (e7), but G1 §10.2 says "tie: the west one".

**F2 [collision] "S edge" pins every layer-1 band-R node exactly STUB above the S edge (jp:1285-1292; G1 §11.3 "on one row").** The node behind each S port is layer 1 (jt:563-575, 692). With any non-empty fan the chain needs 2 (the exact port run, jp:1520/1521) + (m - 1) + 2 (fan to node) >= 4 rows from the edge, but "S edge" says exactly 2. That is a Y contradiction for every m >= 1. "On one row" also forbids fans of different sizes within one group. D115a rules the stub below each edge-most split or merge, not one row for all of them. The SF1 brief protects the run table but does not mention this relation, so after F1 it is the next Y failure.
- Fix: the stub stays exact in the chain (port to first element, as W/E's (Port, Fan) = 2 at jp:71). Apply "S edge" only to nodes whose port edge has no fan, or make it >=.
- In T and LL_RR every layer-1 band-R node is adjacent to its port, so the exact stub runs already imply the relation and nothing moves (INFERRED).

**F3 [collision] An S out fan that is its edge's first token starts on the merge node.** jp:263 gives a token at t = 0 the node's point. Only `SBend` gets a new point (jp:267), and the patch builds Fan "as an SBend" without extending that line. e18 and e19 (program `[Fan]`) therefore start their diagonal on n8/n9's merge tile, where the straight leg and the joining S-bend already lie (three pieces). Fix: a Fan at t = 0 from a node gets a new point and the (Node, Fan) run.

**F4 [wrong] Stale reason label.** `why("S port spacing")` (jp:1307) also labels the row-node relations at jp:1314-1320, because no `why()` call sits between them. So a Y chain prints "S port spacing" where it means "exit split or joiner merge on its item row". The Y cycle in the run therefore does not involve the S ports. Fix: `why("row nodes")` before jp:1314.

**F5 [wrong] Two claims in D230.**
- (a) "The least solution sizes the fans" is not true. With fixed halves hands the least solution places the S port block at the largest west-half or middle node, pushed through the exact cross-group spacing (jp:1309). That fan stays empty, and east-half fans absorb the rest. One group's fans are therefore sized by the other group's nodes. L_R x LL_RR in side: e10 S-bends west by at least 1, then its bridge stands at x(n2) + 1 (jp:1552), so x(n3) >= x(n2) + 2. Least solution: n2 = port 2 (empty), and the east fan is at least 1 from the relations alone.
- (b) Toggle "all one way": all-east fans give the S ports no lower bound except the origin. Their only X relations are the spacing and the port runs (jp:1298-1311, 1520-1530), and the box bound is an upper bound. So the least solution puts the S ports at column 0. Only all-west is a usable toggle.

**F6 [missing] S fans do not lean (D212, D215).** The exact port run puts every S fan 2 tiles inside its port, with the free run between fan and node. D215 asks the opposite. The lean is X-only (jp:383) and skips vertical S-bends (jp:657). Fix shape: put the exactness on the node side (fan to node at its floor, so the free run on the port side is the line's approach, D212), or add a Y kind-1 lean. Today's layouts are untouched either way.

**F7 [wrong] The fan X floor in the brief (`ge ... 0`, not `el.m`) lets m shrink between rounds.** Every other free S-bend keeps `ge(..., el.m)` (jp:1484), so its m never decreases and the rounds settle. With fans that can shrink, Y/X can oscillate up to "does not settle" (jp:2110). Fix: floor `el.m`. It starts at 0 for a fan, so an empty fan is still allowed.

**F8 [collision] Fan tokens and row roles.** `JunctionIsInEdge` and `JunctionIsOutEdge` (jt:211-221) define a row's in/out edge as a port end, an odd-level node end, and a Fan token first or last. An S port edge that ends at a row node would become a second in edge of that row (jp:152). That happens for an S lane with a single destination, such as a future movement toggle (D222). It is unreachable with today's hub topologies. Fix: test that the port is a main port, not the token kind.

**Recon errata.**
- §1.1 "Corner(left)" contradicts "runs east". Under jp:245 it is `Corner(+1)`, clockwise (jp:178).
- §1.1 relations 6-7 depend on F1(d). After F1, e3's S-bends are vertical: bridge 8 = b.first + 1 >= bridge 5 + m_a, which still gives a +1 cycle of the same kind as L_R x LL_RR.
- §3 "required spacing" 3 and 2 come from the relations only. Occupancy needs more: e11 must S-bend at least 2 to clear the carrier's column.
- §4 candidate (ii), relaxing the bridge pin, cannot resolve L_R x LL_RR (Q1).

## Answers to the questions

**Q1. The general rule.** Yes in substance, and it already exists on W/E:
- The Fan token's hand is 0 (jt:1120, 1128, 1133). Placement takes the hand from the solved sign (jp:493), and render matches either hand (render:127-128).
- The port rows are numbers, not points (jp:1523-1528, R3). They are chosen by minimising the largest fan (jp:1949-1974; G1 §11.3), with lanes S-bending outward monotonically (G1 §10.2).

What does not transfer is "the solver picks direction" inside the difference system:
- A sign is a disjunction, not a difference constraint.
- With no sign there is no X relation at all between port and node, and the S block falls to column 0 (F5b).

D230's halves rule is the minimal set of difference constraints that both anchors the block and keeps lanes in order: west half port >= node, east half node >= port, so the block lies between its lanes' inside ends. It is one rule for any lane count. A one-lane group is its own middle lane, which is why it gets no fan, not because of a lane-count exception.

- For 2-lane groups the halves rule gives exactly the region that the W/E minimax would search, so keep it, restated that way.
- For 4 or more lanes, add one order relation per adjacent pair in the same group: x(node i) + 1 <= x(node i+1). Same-hand fans keep order if and only if that holds, and fans with crossed hands only when both are empty.
- Keep the out fans on halves, not crossed (F1b).

Simpler resolutions:
- (i) Relaxing the bridge pin does not help. After its west S-bend e10 is at x <= p. e11's S-bend starts at p heading east, so a carrier column must lie in [p+1, p+m11]. That interval is empty for every pin.
- (ii) Token order is fixed. The diverging S-bend is the split itself (G1 §11.8: a split is the tile of its first diverging half piece; jp:263), and D113 wants a signal-legal piece before the head.
- (iii) A real alternative is choosing which leg of an S-in split goes straight. Today it is the leg to the farther row (jt:1086-1088). If each lane's inner leg goes straight (lane 0 east-bound, lane 1 west-bound), e10 bridges e11's S-bend in column p+1 with the ports 1 apart, and lane 1's east-bound leg diverges east by at least m11. That needs no fans and no deeper stub. But it is also a rule by lane half, so it is not more general. It is a candidate for the cost search (D207). The exit side needs room too: e14 S-bends under e7 and again into n8 (INFERRED), which the out fans give.

**Q2. Exact relations that encode a smallest-layout choice, not a rule.**
- "S edge", all layer-1 nodes on one row (jp:1288-1292): must become per port (F2). Breaks nothing placed today (INFERRED).
- "Bridge over S-bend":
  - The Y heads at exactly one row beyond the crossed tiles (jp:1554-1555; G1 §11.3 "spans exactly") can become two floors. On flat ground the least solution sits at the floor, so it is identity-safe. It changes layouts only under facts, and the G1 §11.3 exact band-layer height becomes a floor.
  - The X column (jp:1552) should stay exact. A column anywhere inside the S-bend would tie the rows of the crossed tiles to c - first, a cross-axis relation that the separate X and Y solves cannot express. It also resolves neither cycle (Q1 i).
- Corner of exactly k (jp:1471-1473) and the item-less horizontal S-bend of exactly one row (jp:1487): smallest-shape choices. Leave them until a topology needs more.
- The fan's position at the port (exact run, jp:1520-1521): a lean choice (F6).
- Ruled, so keep: S port spacing (D38 flat ground, D142, D203(1)); the stub (D115a, D206); split to heads 2 and the head stagger (D145, D118); the prio span; item rows (D142).

**Q3. "Prefer empty fans".**
- The least solution cannot express it: it is a longest path, and the objective here is a minimum of |x(node) - x(port)| (sum or max).
- The P3b lean cannot carry it either:
  - it never moves a group that holds a port (jp:400-403), and the S ports are one rigid group;
  - it only increases columns (jp:406);
  - it pushes each change to its extreme, not to a target.
- What works is the W/E precedent: a scan over one variable, the S block's offset, between its least and greatest column with the nodes held, minimising the largest fan (ties: west). Run it inside `solve()` where the D215 lean runs, so the rounds read m from it. Pick one objective for all port fans. The W/E ports already use minimax (fewer rows); "empty fans" counts bends instead, and the two differ for 3 or more lanes.

Free direction in the model:
- X: no relation between port and node, so lane order needs the explicit node relations (Q1) and the block needs an anchor.
- Y: |dx| is already handled (abs at jp:2102; extent m - 1 at jp:1480). But the hand used for tiles, curve gaps and occupancy must be taken from the sign every round, as jp:493 does for W/E.
- Fixed choices (jp:2030-2069) would need a signed m.
- Sign flips can stop the rounds settling.

Fixed halves hands avoid all four. Then the objective only picks a point inside the interval.

**Q4. Risks and staging.** No step moves today's T or LL_RR seed, provided the INFERRED claims hold; each step runs `jt gate` and `jt identity`.
1. **S0:** F1 heading (placement and the program's `heads_south`) plus F4 label. Identity unchanged.
2. **S1:** F2, "S edge" per port. Identity unchanged.
3. **S2:** SF1 with halves on both sides, plus F3 and F7. Do not flip the out hands. Red: L_R x LL_RR. Identity unchanged, since a one-lane group has no Fan token.
4. **S3:** rerun the recon probe on LL_RR x LL_RR (any) and record its next cycle. Do not assume fans suffice.
5. **S4:** minimax block objective. This moves only multi-lane S seeds, which have no identity pins yet. Pin their hashes after it.
6. **Later:** bridge Y floors and the F6 lean, each identity-safe on flat ground.

## Suggested tests
- S0 red: a branch with one S in lane and two S out lanes (pairing any). Before the fix, the error names e1's "Corner size" or e6's "S-bend size". After it, placement gets past both. Unit: for L_R x LL_RR, the span of e6's SBend lies west of n1 and e1's Corner ends south of row 0.
- S1: a layer-1 node behind a fan of m columns lies at least 4 + (m - 1) rows above the S edge. T and LL_RR are byte-identical.
- S2: L_R x LL_RR places. In each S group, nodes keep slot order with x(n i+1) >= x(n i) + 1. Every S fan's first point is exactly 2 inside its port, and the out fans' first points are not the merge tiles. The rounds settle (report the count).
- Report-only: all-east hands put the S ports at column 0 (documents F5b).
