# RECON: why placement refuses L_R x L_RR, L_R x LL_RR and LL_RR x LL_RR after S2 and the S-bend roles

Worktree .claude/nodes, branch wt-s3r at eec2db81e3. Probe (all reverted, `git diff` empty): debug output to a file in `solve`'s failure branch, a longest-path cycle search over the explained constraints, a per-round dump of `vx`/`vy`, and a hidden test calling `MakeJunctionGraphSeed` for the three topologies with `JunctionLanePairing::AnyToAny` and `MakeTestJunctionSpec()`. "jp" = placement/junction_placement.cpp at HEAD (line numbers of the clean file). Points are placement points (pt), edges/nodes the graph builder's numbering. Weights: `v[a] >= v[b] + k`.

## 0. Verdicts (one line each)

1. **L_R x L_RR: the reported error is a red herring.** `CutJunctionTrackBridges` (junction_pipeline.cpp:110-193) tries every series cut and reports only the last cut's error. The natural cut ({e6,e5},{e9}) does not fail with a contradiction; it fails with "the placement does not settle" (jp:2141), a diverging X/Y feedback with gain exactly 2. The reported cycle belongs to the two finer cuts, which cannot work for crossers that touch. Class **(d)** for the real cause (a missing fan/band-growth objective, G2.5 S4), with a **(c)** reading for the reported cycle.
2. **L_R x LL_RR:** class **(c)/(b) borderline**: a Converge S-bend pinned to a merge, directly followed by a Corner pinned to the same merge node, with the curve-gap rule between them. Not a hand or program error. Same cycle in all 5 cuts and in the uncut `PlacementOf`.
3. **LL_RR x LL_RR:** class **(b)**: `node_item(out_edge[r])` (jp:544, used at jp:1334) reads the first item-naming token of a doubled out edge, which is the LaneMerge S-bend's end item (the Copy item), but G2.4 says the joiner merge sits on the Lane item (A). Same cycle in all 4628 cuts.

Shared cause: **no single root cause**. 2 and 3 are the same family (a node pinned to two things by two different readings of an edge's tokens, where the program is right by G2.4/G2.6 and placement pins both ends), 1 is a different mechanism. Details below.

## 1. L_R x L_RR (main L_R, branch L_RR)

Slots: 0 W in, 1 W out, 2 S in (one lane), 3 S out lane 0, 4 S out lane 1, 5 E in, 6 E out. Nodes (level): n0 (1) W-in split, n1 (2) W-row split, n2 (4) S-in split, n3 (3) E-row split, n4 (4), n5 (3) merge, n6 (4), n7 (4), n8 (1). The graph has 17 edges. Edges in play (tokens: kinds Fan, Corner, SBend, Over, Under; roles D=Diverge, C=Converge, LS/LM = LaneSplit/LaneMerge):
- e10: n3 to n5, path 5->1 (E in to W out), a row edge, **doubled**, heading west (hin 3). Program: Over(e6), Over(e5), Over(e9), SBend(h-1, LM, i4). Crossers per edge: e6 and e5 leave n1 (pt 8); e9 leaves n2 (pt 9).
- e5: n1 to n6, path 0->3, SBend(h+1, D) then Under(e10) twice. m = x(n1) - x(pt 29).
- e6: n1 to n7, Under, Under, Over(e11). e9: n2 to n8 (S in to E out), Under, Under, Over(e1), Corner, SBend(C).
- e8: n2 to n5, path 3 (S in to W out), Corner(h-1, i5), SBend(h+1, C, i4). e13: n5 to port 1, Fan(i4). e14: n6 to port 3 (S out lane 0), Fan, hand -1.

### 1a. What `MakeJunctionGraphSeed` reports

`bridge heads hug crossers (edge 10, row 1) <- lane split run (edge 10, node 3, row 1) <- ...`. Phase **X** (phase=0, rows_known=1, round 0). It is the last of **three cut attempts** on edge 10's crossers {e6, e5, e9}: compositions of 3 are tried as 2 bridges ((2,1) then (1,2)) and then 3 bridges ((1,1,1)). Probe log order: attempt (2,1) printed SETTLE lines only (no contradiction); (1,2) failed in X; (1,1,1) failed in X with the same cycle. The reported text is `last_error` of the last attempt.

| # | relation (jp reason) | constraint | weight | jp |
|---|---|---|---|---|
| 1 | bridge heads hug crossers, bridge B east head 56 on the east crosser tile pt 8 - 1, plus 1 | v56 >= v8 + 0 | 0 | 1212-1214 (hug_heads), 1427-1450 candidates |
| 2 | bridge heads hug crossers, bridge A west head 55 hugs the west tile of e6's two tiles (pt 8 + 0, pt 8 - 1), minus 1 | v8 >= v55 + 1 | 1 | same |
| 3 | lane split run, series bridges: far head of A to near head of B at least the Heads-Heads floor | v55 >= v56 + 2 | 2 | 1413 (the `b > 0` along; reason set at 1405) |

Loop v56 -> v8 -> v55 -> v56: 0 + 1 + 2 = **+3**, no solution. Crossers: e6 is a straight leg at column x8; e5 leaves n1 by an S-bend (west), so on row 1 its tiles are pt 8 - 1 .. pt 8 - 4. They are column-adjacent and share the tile pt 8 - 1 (e6's copy 1 tile). Series bridges need at least 2 plain columns between heads (the same floor as chain run Heads-Heads); touching crossers cannot be split between two bridges. Attempt (1,2) = {e6},{e5,e9} and (1,1,1) = {e6},{e5},{e9} both separate e6 from e5; the cut that keeps them together, (2,1) = {e6,e5},{e9}, is the natural one.

### 1b. The real failure: attempt (2,1) does not settle

The hidden test "placement: S branch groups of several lanes place" reports exactly this for L_R x L_RR ("the placement does not settle", jp:2141) through `PlacementOf`, which uses the uncut structure; the cut (2,1) behaves the same. m of the S-bends that run the full box width, per round (vx/vy dumps):

| round | e5 tok0 (n1 to n6 S-bend) | e11 tok1 | e14 fan (n6 to port 3) |
|---|---|---|---|
| start | 9 | 9 | 8 |
| 0 | 55 | 55 | 49 |
| 1 | 143 | 143 | 137 |
| 2 | 319 | 319 | 313 |
| 3 | 671 | 671 | 665 |
| 4 | 1375 | 1375 | 1369 |

From round 1 on, m' = 2m + 33 exactly (143 = 2*55 + 33, 319 = 2*143 + 33, 671 = 2*319 + 33). The loop:
- **X** (round 0): n6 (pt 13) lies at column 0 (the least solution), port 3 (pt 19) at column 49. So the fan e14 spans m = 49 columns and the S-bend e5 (n1 at 55 to pt 29 at 0) spans m = 55. No relation holds n6 near its port (the Y-only rule at jp "node behind a fan of m columns lies at least 4 + (m-1) rows above the S edge" bounds Y, not X).
- **Y**: a 45-degree S-bend of m columns is m - 1 rows (jp:1503, 1496 for fans). e5's S-bend and e14's fan stack in series from n1 (y 19) to the S edge (y 141): 19 + 54 + 14 + 48 + 6 = 141.
- **X** again: e8 is the S-in joiner; its Corner starts at the band split n2 (`joiner_div`, jp:276, size from the rows, jp:1552-1556). The Corner is a diagonal of (y(n2) - y(row)) columns. Explained chain for x(n8)... (EXPLAIN of pt 8 in round 1): 0 (port 1) + 2 (chain run e13) + 8 (fan size e13) + 4 (curve gap e13) + **121** (curve gap edge 8: v9 >= v33 + 121, with y9 = 139 and the row at 18) + the e1 S-bend (m 9, bridge over S-bend e9: v34 ~ v26) = 143. So the box width follows the box height one-for-one, and the height follows the width with 2 diagonals in series.

Gain 2 is why it diverges rather than converging to the first-pass m.

### 1c. Classification (topology 1)

- (a) no: hands and tokens agree (e5/e11 hands as in RECON-s2-cycle; no sign error found).
- (b) no: the hug tiles are exactly e6's and e5's tiles on the row.
- Reported cycle: **(c)** two correct rules (crossers packed exactly under a hugging bridge; series bridges leave 2 plain columns) truly conflict for crossers whose tiles touch; the cut search is meant to find the alternative, and it does (the (2,1) cut passes that check) but then dies in the settle loop.
- Real cause: **(d)** the fan/band-growth objective of DESIGN-G2 §G2.5 ("the fan objective (REVIEW-port-fans S4): keep each band's growth minimal") has no code. placement/CLAUDE.md states the only objective as "the least solution, then each row change pushed toward its feature": the least solution drives n6 to column 0, a 49-column fan. Inference (not tested): with n6 held near port 3, e5 and e14 would be a few columns, the X/Y gain would drop far below 1, and the (2,1) cut would settle.

## 2. L_R x LL_RR

Slots: 0 W in, 1 W out, 2,3 S in, 4,5 S out, 6 E in, 7 E out. 12 nodes. Edges in the cycle:
- e9: n2 to n7, path 3 (S in 2 to W out). Program: SBend(h-1, D), SBend(h+1, C, i4)... chain: tok0 f9 l40 m5, tok1 f41 l14 m6 (pt 14 = n7, `last = stop` pinned by role Converge, jp:274). Heading north (hin 0).
- e17: n7 to n6, paths 3, 4 (merged joiner lanes). Program: Corner(h-1, i5), SBend(h+1, C, i4). Corner tok0: first = start = pt 14 (jp:273, the exception at jp:277 covers only SBend and Fan).
- n7 is a merge (level 4); n6 the row merge (level 3).

Phase **Y**, round 0 (phase=1, rows_known=1), identical cycle in all 5 cut attempts ({e7,e6}/{e12,e11}; the three 3-bridge cuts; the 4-bridge cut) and in `PlacementOf` (uncut):

| # | relation (jp reason) | constraint | weight | jp |
|---|---|---|---|---|
| 1 | S-bend size (edge 9): S-bend tok1 is exactly m - 1 = 5 rows, m = 6 from the first X pass | v14 >= v41 - 5 (and v41 >= v14 + 5) | -5 / +5 | 1503 (and the X side 1507) |
| 2 | curve gap (edge 17): curve 3 (2nd curve of e9's S-bend, hand 0, anchored pt 41, yoff 12) to curve 4 (e17's Corner, hand 0, anchored pt 14, yoff 0): min_gap 5, delta -12, sg -1, so `y41 - y14 >= 9` | v41 >= v14 + 9 | 9 | 1625-1641 |

Loop: +5 against 9, weight total 4. Phase Y, not X; `m` is fixed from X, so the extent is exact and the round loop cannot raise it from the Y side (the settle loop only lowers/raises m from X, jp:2125-2138).

Why: a Y offset of 12 units is 2m (the second curve lies one row past pt 14, because `last` of an S-bend is the tile before its closing curve; within a chain this is absorbed by the chain-run floor between elements, e.g. e9 tok0 to tok1 gets 2 more rows). Here there is no chain run: the S-bend ends on n7 (pinned by role, G2.6) and the Corner starts on n7 (first = start, jp:273); the run between them is exactly 0, so the pair needs the S-bend to span 9 rows (m >= 10) to satisfy the gap rule with nothing between. m comes out of X as 6. INFERRED: the Corner after a merge at jp:273 should get a free first as an S-bend or Fan does (jp:277), or the curve gap rule must allow a run (a FloorKind Node to Corner floor) between the merge and the Corner; the decision is not in the design docs I read.

Classification: not (a); (b) in the weak sense that the Corner at t == 0 from a **merge** is pinned like a Corner from a band split (`joiner_div` is the band-split case only, jp:276); (c) in the strong sense: R1 "a merge is the tile of its last converging half piece" (placement/CLAUDE.md) pins the S-bend, R2 the Corner has no run before it, R3 same-hand curves are at least `min(2TL-1, max(2, CL))` apart (CL = 5 units here); the three together need an S-bend of at least 10 columns. The rule that would have to give is R2 (a straight run between merge and Corner), which is what the CLAUDE.md line "A row change sits next to the feature it serves, and the free run lies on its other side" already allows for row changes.

Hidden test "placement: S branch lanes open up by fans inside the box (D230)" builds exactly this and fails with this cycle; "placement: S branch groups of several lanes place" fails first on L_RR ("does not settle", section 1b) and then on LL_RR with this cycle (the test runs both).

## 3. LL_RR x LL_RR

27 nodes, 12 ports. Stack items (printed): row 0 items 0 (Prio), 1 (Lane), 2 (Approach); row 1 items 3 (PrioAndCopy), **4 (Lane)**, 5 (Approach); rows 2 and 3 likewise. pt numbering: item i is pt i + 1.

Edges: e45: n25 to n24, paths 11 (4->10), 13 (5->10): the joiner lanes' merged out edge. Program: Corner(h+1, i5) then SBend(h-1, C, i4). Chain: tok0 f38 l123, tok1 **f124 l37 (last = n24's pt 37, pinned by Converge)**. n24 is the row-1 joiner merge (level 3, merge). e44: n24 to port 10 (E out): **doubled out edge**, program Over(e14), Over(e15), SBend(h-1, LaneMerge, **i3**), Fan(i3). e19: n2 to n24, the lane edge into n24: Over, Over, SBend(h+1, LaneMerge, i4).

Phase **Y**, round 0, first Y solve; identical cycle in all **4628** cut attempts (so independent of the cut):

| # | relation (jp reason) | constraint | weight | jp |
|---|---|---|---|---|
| 1 | row nodes: the joiner merge n24 (pt 37) lies on the item named by `node_item(out_edge[1])` = item 3 (pt 4) | v4 >= v37 + 0 (and the reverse) | 0 | 1327, 1334 |
| 2 | S-bend size (edge 45): the S-bend's last (pt 37) lies on its token's item 4 (pt 5) | v37 >= v5 + 0 (and the reverse) | 0 | 1513 |
| 3 | stack rows (row 1): item 4 at least one row below item 3 | v5 >= v4 + 1 | 1 | 1287 |

Loop v4 -> v37 -> v5 -> v4: total **+1**.

The mismatch: `node_item(e)` (jp:544-570) for an edge that starts at a node returns the **first** token that names an item. For e44 that is the LaneMerge S-bend, whose index is item 3, the Copy item B of DESIGN-G2 §G2.4 ("Out edge: A is Lane (the joiner merge sits on the lane), B is the row's Copy item; the out edge's Fan runs from the Copy item to the port. Programs: `edges[e]` = `[Over..., SBend(copy, LaneMerge), Fan(copy)]`"). The node is on A = Lane = item 4, which is what e45's Converge S-bend, e19's LaneMerge (i4) and the comment at jp:1334 ("the lane item, at the joiner merge") all say. Placement reads B. The CLAUDE.md line "A row's in or out edge meets its node on the row of the stack item named by its nearest Fan, Corner or S-bend token" is the rule `node_item` implements, and for a doubled out edge it contradicts G2.4. The program is right by G2.4 (also checked by the program test at junction_topology.cpp:4723).

Classification: **(b)**, a relation built from a misread item (a token's index read as the item the *node* sits on when it is the item the S-bend *ends* on). Not (a), not a true conflict.

## 4. Do the three share a root cause?

- 2 and 3 are the same family: placement pins one node or point from two different readings of an edge's tokens (3: node item vs S-bend item; 2: merge point vs Corner start), each by exact equality. Both are exposed by the Converge pin and G2.6 roles but neither is introduced by them: the Converge pin existed (jp:274); in 2 only the zero-run next to it is new, in 3 only the doubled out edge (G2.4) is new.
- 1 is a different mechanism (a diverging X/Y loop and a cut-enumeration error that hides it).
- None is a hand error. None involves the S-bend roles themselves (the roles are right in every dump).

## 5. Open questions

1. Topology 1, cut (2,1): confirm that holding n6 near port 3 (a fan objective) is enough; I only showed the loop and its gain, not a fix.
2. Topology 1: `CutJunctionTrackBridges` reports the last error; should it prefer the failure of the first (fewest-bridge) cut? Not a placement matter.
3. Topology 2: where should the run between a merge and a following Corner come from (free `first` at jp:273/277, or a Node-to-Corner floor)? Decided facts in the design docs do not say.
4. Topology 3: after fixing the item, LL_RR x LL_RR may hit topology 1's or 2's cause next (it has joiner merges followed by Corners, e.g. e45). Not tested.
5. Whether topology 2's pair (3,4) is really an off-by-one of `last` against the rasterised S-bend (yoff 12 vs extent 5 rows) or deliberate; the rest of the model is consistent with "deliberate", the chain-run floors absorb it.

## 6. State

Probe reverted; `git diff` is empty; `git status --porcelain` shows only the pre-existing untracked wip files (and this report, which is untracked).
