# RECON: the seed's out-fan and doubled-leg floors (SF1a, D212, D215)

Method: env-switched constants in a scratch copy of `src/junction_topology.cpp` plus a hidden diag test (reverted; patch kept in the session scratchpad only). Pipeline = ExpandJunctionLanes .. CutJunctionTrackBridges .. RenderJunctionTrackPlacement .. SignalJunctionTrackRoutes, test spec (tl 4, cl 5, prio_tiles 14, even_prio 14), `CheckJunctionRules`. Release build. Line numbers are HEAD 33e835099a `src/junction_topology.cpp`. Units: a straight tile is 2 units; "c0..c3" are the curves of an out route in order. Everything below is MEASURED unless marked INFERRED, PROPOSED or UNEXPLAINED.

## 1. Out-fan distance k (`:1368 along(heading, rp.fof, njm, k, true)`)
| k | T | LL_RR | violations |
|---|---|-------|------------|
| 4 (now) | 41x14 | 62x25 | 0 / 0 |
| 3 | 40x14 | 60x25 | T 1, LL_RR 4: rule 3 CurveSpeed "132 below 160" |
| 2 | 39x14 | 58x25 | T 1, LL_RR 4: rule 3 "111 below 160" |
| 1 | placement OK, render fails "negative or off-line run" (T path 4 edge 9, LL_RR path 8 edge 25) | same | k=1 puts c2 on c1 |

Every out row is clean at k = 4 and no lower, and every one binds at k = 3 (T: edge 9; LL_RR: edges 19, 20, 24, 25; the other out edges have no fan). The bound is JunctionCurveGaps' pairwise gap via G3 (`junction_chain.cpp:22-40`, rule 3), on the joiner route: c0,c1 = the joiner merge's S-bend (adjacent, opposite hands, 2 units apart, c1 on the merge node), c2,c3 = the fan (c3 = c2 + 2 m_out). c2 = c1 + 2(k-1).
| out edge (joiner route) | hands c0 c1 c2 c3 | fan moves | at k=3 | binds |
|---|---|---|---|---|
| T e9 (path 4; m 1) | R L R L | north, 1 row | c0-c2 6<7, c1-c3 6<7 | cl: 2cl cap 7 |
| LL_RR e25 (path 8; m 3, E) | L R R L | south, 3 rows (toward the joiner's side; the merge step went north) | c0-c2 6<7, c1-c2 4<5 (same hand) | both |
| LL_RR e24 (path 9; m 1, E) | L R R L | south, 1 row (toward the joiner's side) | c0-c2 6<7, c1-c2 4<5, c1-c3 6<7 | all three |
| LL_RR e19 (path 10; m 2, W) | R L R L | north, 2 rows (continues the merge step, away from the joiner's side) | c0-c2 6<7 | cap 7 |
| LL_RR e20 (path 11; m 4, W) | R L R L | north, 4 rows (away from the joiner's side) | c0-c2 6<7 | cap 7 |
(Violation tiles at k=3: LL_RR 55,1  57,4  3,9  5,12; T 2,4: the fan's first piece.) The prio row is the merge row (path 0 on E1, path 1 on E2); the joiner arrives from the south on all four.
Gaps used (spec tl 4, cl 5): g1 = min(2tl-1, max(2,cl)) = 5 (adjacent same hand), g2 = min(2tl-1, 2cl) = 7 (every non-adjacent pair), opposite adjacent 2. These are the very g1, g2 of the in-fan block (`:1283-1284`).
PROPOSED: k_out = max( ceil(g2/2), same_hand(c1,c2) ? 1 + ceil(g1/2) : 2 ). Check: ceil(7/2) = 4 and 1 + ceil(5/2) = 4, so k = 4 on every row. The (c1,c3) pair never binds past (c0,c2) (2(k-1)+2m >= g2 whenever 2k >= g2, m >= 1). The floor 2 is "c2 at least one tile after c1" (k=1 fails the render, though the placement accepts it: UNEXPLAINED by the placement rules, only by the render). The constant 4 equals the derived minimum for these constants, so it is the rule minimum today, and would be too small only for another tl or cl (INFERRED, not run).

## 2. Doubled-leg lane split (`:1303 nh0[0] >= lane_split+2`, `:1304 eq(sb1_last, lane_split)`, `:1305 nh1[0] >= sb1_last+1`, `:1307 nh0 = nh1+1 exact`)
Seed distances (tiles, along travel; copy 1's S-bend piece to the first heads), LL_RR:
| leg | heading | S-bend x | nh1 | nh0 | to nex (min 2, `:1301`) |
|---|---|---|---|---|---|
| e8 | +x (E) | 24 | 31: 7 (min 1) | 32: 8 (min 2) | 2 (at min) |
| e9 | -x (W) | 38 | 37: 1 (min 1) | 36: 2 (min 2) | 4 (slack 2) |
| e10 | -x (W) | 43 | 42: 1 | 41: 2 | 2 |
T e5 (-x): S-bend 31, heads 30 / 29, nex 33: all at min. So the engineer's finding holds on exactly one leg, LL_RR e8: the lane split sits at nex+2 and the heads 6 tiles later. Cause: the placement is a least-x solve. For heading +x the lower bound `lane_split >= nex+2` is the only one and the heads are pushed east by the crossers' tiles (`:1336-1340` ge near/far); for heading -x the same bounds read the other way round and the split is pulled to the heads.
Experiment, `nh0[0]` exact (`SF_LSX`) with d = 2: CutJunctionTrackBridges finds the same cut as now (doubled bridges e8:2 e9:2,1 e10:1,2,2; 7 tuples tried at the least total, 1 places, the other 6 fail "the placement rules contradict each other"); T 41x14, LL_RR 62x25, 0 violations; e8's lane split moves to x=30 (heads 31). d = 3: same box, 0 violations. d = 4: LL_RR 62x26. d = 5: 63x28. d = 6: 64x30. d = 1 and d = 0: every cut fails "contradict" (T and LL_RR): the floor 2 is not the constant at `:1303`, it is implied by `:1304`+`:1305`+`:1307` (nh0 = nh1+1, nh1 >= sb1_last+1), so `:1303` is redundant. Making `:1305` exact too (alone or with `:1303`): identical result (62x25, clean).
PROPOSED: both `:1303` and `:1305` exact. d_split = 2 for copy 0 (1 for copy 1) = one tile for the copy-1 S-bend's last piece plus the nh0-nh1 stagger. UNEXPLAINED: why the stagger `:1307` is 1 (no rule constant; the heads' tiles hug the crossers, D80/D118/D145).

## 3. Doubled-leg lane merge (`:1311 sb0_first >= fh0[g-1]+1`, `:1312 eq(lane_merge, sb0_first)`, `:1313 lane_merge >= fh1[g-1]+2`, `:1315 njm >= lane_merge + even_prio+2`)
Seed (LL_RR): all three legs already at every minimum: last copy-0 head to S-bend 1 tile, last copy-1 head to S-bend 2 tiles, S-bend to njm 16 = even_prio + 2 (e8 x 39 -> N12 55, e9 23 -> N7 7, e10 25 -> N8 9). Exact `:1313` (`SF_LMX`) d = 2: same cut, 41x14 / 62x25, 0 violations (no change). d = 3: T 42x14, LL_RR 64x25; d = 4: 43x14, 66x25 (both clean). d = 1, 0: every cut fails "contradict": `:1313` is again implied (`:1308 fh0 = fh1+1` + `:1311`). Nothing to fix for the merge; the njm side is the prio element (RECON-prio-element §3-4), exact in the seed.

## 4. Together (out-fan k=4, lane split exact, lane merge exact)
T 41x14 -> 41x14, LL_RR 62x25 -> 62x25, 0 violations on both; Release pipeline (cut, render, signals, no checker): T 0.5 ms, LL_RR 2.0 ms (before: 0.5, 2.1). The box does not change because k=4 is already the clean minimum and the exact floors only move e8's lane split east into slack.

## 5. Every row-change token, LL_RR (seed at HEAD, base coordinates; node tiles N from the placement)
Kinds as briefs/P3b (1 fan, 2 lane split, 3 lane merge, 4 other S-bend). "pin" = the token's last point IS the node point (`:1051` el.last = stop; distance 0 by construction). `d_min` as `:1281-1295`.
| token | kind | feature | constraint (file:line) | seed vs min | least-x explanation |
|---|---|---|---|---|---|
| e0 in fan | 1 +t | exit split N0 (32,1) | `:1294 nex-fil >= d_min` (=4: c0 L, exit corner R) | fan end (4,1): 28 vs 4, 24 late | heading +x: fif = port+2 (`:1275`) is the only lower bound, nex is pushed east by other rules |
| e1 in fan | 1 +t | N1 (22,3) | `:1294`, d_min 4 | (3,3): 19 vs 4, 15 late | same |
| e5 in fan | 1 +t | N5 (45,11) | `:1294`, d_min 2 (c0, c2 both L) | (47,11): 2 vs 2, at min | heading -x: nex lowest, fan lower-bounded on it |
| e6 in fan | 1 +t | N6 (42,8) | `:1294`, d_min 4 | (46,8): 4 vs 4, at min | same |
| e19 e20 e24 e25 out fans | 1 -t | joiner merge N7 (7,9), N8 (9,12), N12 (55,4), N13 (53,1) | `:1368` exact k=4; port `:1370` exact 2 | 4 vs 4 each (clean minimum, section 1), port 2 | exact, no x-dependence |
| e8 lane split (c1 t0) | 2 +t | heads (31,4) | `:1303-1305` | 7 vs 1 (nh0 8 vs 2), 6 early | heading +x (section 2) |
| e9 e10 lane split | 2 +t | heads | same | 1 vs 1, 2 vs 2 | heading -x: pulled to the heads |
| e8 e9 e10 lane merge (c0 last) | 3 -t | last heads | `:1311` `:1313` `:1315` | 1 vs 1, 2 vs 2, 16 vs 16 | both sides at min on all three |
| e12 t5 | 4 +t | N11 (34,16) | pin `:1051` | 0 (S-bend ends at (34,16)) | n/a, exact |
| e14 t1 | 4 +t | N10 (40,17) | pin | 0 ((43,15)-(40,17)) | n/a |
| e15 t3 | 4 +t | N13 (53,1) | pin | 0 ((53,2)-(53,1)) | n/a |
| e16 t1 | 4 +t | N12 (55,4) | pin | 0 | n/a |
| e17 t3 | 4 +t | N7 (7,9) | pin | 0 ((7,10)-(7,9)) | n/a |
| e18 t1 | 4 +t | N8 (9,12) | pin | 0 ((9,13)-(9,12)) | n/a |
| e23 t0 | 4 -t | from-node N11 (34,16) | `:1054` free first, `:1412-1418` run dist 2 not exact | first piece (34,18): 2 vs 2, at min | heading +y: least-y puts it at the lower bound, next to the node |
| e22 t0 | no lean | between N10 (40,17) and N9 (37,22) | end pinned `:1051`; start by the geometry (free m) | start (40,20): 3 from N10 vs 2, 1 slack | not a least-x effect: set by the pinned end and m |
| e4 t0 | no lean | between N2 (35,22) and N4 (29,15) | start = node (band split `:1050`); end `:1418` dist 2 | 0 and 2 (at min both) | n/a |
So in the seed, D215 already holds for: all kind-4 tokens that precede their to-node (pinned, exact, rigid in the template), e23 (at its lower bound), all out fans, all lane merges, e9/e10 lane splits, the E-in fans e5/e6. It fails only for the W-in fans e0, e1 (kind 1 +t, 24 and 15 tiles away) and e8's lane split (6 tiles), the three heading +x cases of a lower-bound-only constraint; the cure for each is an exact relation to its feature (nex-fil = d_min; lane_split = nh-2), proven clean for e8 above (not run for e0/e1: INFERRED).

## Cleanup
`git checkout -- src`; worktree clean (only the untracked docs/junction-tool/wip/ that was there at the start).
