# RECON fidelity: recording vs joint model, L_R x L_RR, L_R x LL_RR, LL_RR x LL_RR (at 37cad61e2f)
Probe: temporary test in worktree ng1 (reverted), raw output /tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/fid/out.txt (relations matched by (axis,v,u,reason)).

## 0. The premise does not reproduce; what actually fails
- Round 0's own Y solve fails at the all-floor C (not round 1's), with exactly the quoted chain: "joiner_div Corner choice (edge 8) <- chain run (edge 2) <- S port row <- S edge (node 6) <- ... stack rows (row 1)". Cause: choices.corner[8] = k = 2 is pinned by eq(1,first,last,fixed->corner) (line ~1693) while the stack rows need span 12. Expected, the MILP then sets L(bend 6) = 12.
- So round 0's recording is the *failed* Y round: solved = {} (r0.solved.x.size()==0), no X relations (X 0 / Y 232), no routes, no clashes (0). The joint at round 0 therefore has NO X rows, NO curve-gap counters, NO occupancy clashes.
- Round 1 at C' (point = joint x,y): Y solve succeeds (failed[] lists only an X solve). Y: 232 matched relations, 0 mismatches, 0 only-in-one. X: 361 relations, all new; X solve fails ("bridge heads hug crossers (edge 10,row 1) <- lane split run (edge 10,node 3) <- chain run (edge 9) <- bridge over S-bend (edge 9) <- S-bend size (edge 1)...").
- L_R x LL_RR: round 0 Y also fails at C ("chain run (edge 11) <- S-bend size (edge 9) ... bridge over S-bend (edge 10)"); X recorded 297/Y 291, clashes 0, routes 11. Round 1: X solve fails ("S port spacing <- chain run (edge 2) <- S-bend size (edge 2/9/3/10) <- bridge heads (edge 10)").
- LL_RR x LL_RR: the joint at round 0 is already Infeasible (loop stops, rounds 1): conflict starts "Corner size (edge 3) <- chain run (edge 3) <- bridge over S-bend (edge 5) <- bridge heads (edge 14) ... lane split run/hug crossers (edges 28,29) ... bridge over S-bend (edge 43)". No C', no round 1 to compare.
- The loop's "infeasible" outcome for L_R hubs is the JOINT at round 1 (status 1, kept 93/104 clashes), not the recording's failure; the recording failure is in X.

## 1-2. Mismatches (L_R x L_RR: none; L_R x LL_RR: 15) and one-sided relations
L_R x L_RR: 0 mismatches. Only-in-one: Y none; X 306 only in r1 (X absent at round 0): box bound 70, occupancy 64, chain run 40, S-bend size 20, lane split run 16, hug crossers 16, prio element 11, curve gap 20, port columns 8, fan size 11, Corner size 6, series bridge heads 4, bridge over S-bend 4, bridge heads 4, S port spacing 4, joiner_div Corner choice 2.
L_R x LL_RR (c predicted from tag vs c' recorded at C'):
- curve gap, nonlinear (bend -1, a 0 b 0, nl 1): X 5 (e.g. v9 u55 e17 c-5 c'-6; v41 u55 c0 c'-1; v64 u45 e21 c-13 c'-19; v87 u45 e20 c-13 c'-19), Y 6 (e.g. v8 u30 e6 c4 c'10; v12 u11 e14 c6 c'5; v30 u7 e1 c6 c'4; v41 u53 c4 c'5; v52 u11 e15 c6 c'4). Untagged-nonlinear by design; benign ONLY IF replaced by the counters (see 3).
- fan size (edge 16,row 1), bend 19 (port_rows Fan), a 1 b -1: X v85 u86 c0 c'6 (pred 0), v86 u85 c0 c'-6. L=1 (joint's held length) but r1 recomputed the fan as m=7: c' = 7-1. THE tagged mismatch: the bend's L' in the model is not the L the recording sees.
Only in one (L_R x LL_RR):
- ONLY r0: "fan size" (edges 2,3,4 row 1) X 6 + Y 4: empty-fan branch (zero_branch, SFan m 0 -> L' 1; fan 4 is port_rows 0 -> 6).
- ONLY r1 Y 25: bridge middle clearance (edge 13) 16, chain run (edges 2,3) 4, curve gap (edges 9,10,11,17,21) 7 (all start at v32/v34 = the ends of SFans 3,4 that went 0 -> 1).
- ONLY r1 X 112: occupancy 81, curve gap 16, chain run 5, hug crossers 4 (+ COUNT 1 vs 2 on 4 rows, edge 13 row 1), fan size 2, bridge middle clearance 2.
L_R x L_RR bends: r0 m vs L' (r1.m): port_rows Fans 0 -> L' 0 but r1.m 1,10,11,1 (bends 0,3,16,19); Corner 6 (corner_rows) 2 -> 12 = r1; "rows" S-bends 7,9,10,11: 1/0 -> 1 = r1; free: equal. L_R x LL_RR: port_rows Fans 5 (0 -> r1 6) and 19 (1 -> r1 7); bends 0,24 (9,9) equal; everything else r1.m == L'.

## 3. Why each family's constant moves without a tag
All lines in src/junction/placement/junction_placement.cpp.
- fan size (1673-1680): Fan el.m is port_rows = |y(port row) - y(item row)| at the point; tag {&el,1,-1} IS present, but the MILP holds that L at the r0 constant (no y link). Empty branch (el.m==0) is zero_branch; with no Z variable for a port_rows bend (Z only for free SFan) the joint falls to the else-branch (c, hard row) and forces the fan empty and on its port. Wrong exactly when the Y solution gives the port row a nonzero span.
- joiner_div Corner choice (1693): c = fixed->corner[e][tok], tag a1 b0 on corner_rows bend: faithful (L = choice). The failure is round 0's k pin vs rows (see 0), not fidelity.
- S-bend size (1712-1740): tagged (VY, -VY) / (1,0) on el.m for free and rows bends: faithful; the SFan variant is zero_branch/sized-branch via Z: faithful. Fan at S port (el.fan, hin 0/2) m==0 has no rows.
- chain run / "S port spacing" / "port columns" / "S port row" / "stack rows" / "row nodes" (1494-1537): constants (item counts, slots, structure), no bend reads. "S port row" ties port_pt to se_pt; no size dependence. Contradiction cycles through them are made of tagged rows.
- curve gap (1894-1913): relation constant = ceil((min_gap - delta)/2), delta from route curve offsets (xoff/yoff of bend rasters at the recorded m and the rows): tag nonlinear (bend -1). Skipped by the joint only when system.routes is non-empty (joint.cpp: `relation.nonlinear && !routes.empty() && starts_with("curve gap")`), replaced by AddCurveGapCounters. In hub 1 round 0 routes = 0 (X never recorded) so Y/X curve gaps would be held at c; in hub 2 routes = 11 and they are replaced. Curve-gap rows exist per curve: a bend of size 0 has none, so the set depends on SFan/fan emptiness (the 7 new Y rows).
- hug crossers (1640-1668): z.y == vy[heads] picks which crossed tiles; reads vy (point rows) and tile dx; set (1 vs 2 per row) changes with the rows; X-only, order_pin rows dropped by the joint.
- bridge middle clearance (1450-1466 X, 1836-1842 Y): side = flat[head.xpt]-flat[line.line] from the flat X solution, head row strictly inside the bridge's rows (vy): present only when X was solved, absent in r0.
- occupancy / box bound / lane split run / prio element: X-phase families, absent from round-0 Y-only recording (hub 1).

## 4. MILP variables per bend source (junction_joint.cpp 190-215, 272-300, 374)
- free: L variable (SFan: with z, L>=z, L<=Mz; S-bend floor 1) + x link row (and y link unless tagged_y). Choice m written back.
- corner_rows (joiner Corner): L var (lb k), link y(first)-y(last)=L. Choice corner written back.
- rows (item-pinned horizontal S-bend): L var + z (can be 0), link y(item)-y(first)=-sv L. Not written back (read from Y at the point).
- CONSTANT (Var(b.m,b.m)): port_rows (W/E Fan: size = port-row span, a pure function of y, no link row), fixed_k (non-joiner Corner, k), one_row (item-less horizontal S-bend, 1 row). fixed_k/one_row are genuinely constant; port_rows is NOT: it is a y difference left unlinked and frozen at the recorded m (0 in round 0 because the Y solve failed, so rows were unknown; r1 gives 1..11). This is the one bend family read by tagged rows (fan size, a1 b-1; zero_branch) whose size the MILP does not control. Consequence: C' (and the solved x/y) can place port rows with a span while the model assumed an empty fan on its port column; round-1's X then holds a fan the joint never saw.

## Verdict
(1) Biggest gap: round 0's recording is the failed Y round, so the joint has no X relations, routes, clashes at round 0 (hub 1); C' is optimal for Y only and X rejects it at round 1 (hug crossers / S port spacing cycles). (2) port_rows Fans frozen as constants, no link to y (hub 2 mismatch 2 tagged rows + the zero_branch fan 4 and emptiness-dependent row sets). (3) curve-gap/clearance/hug row sets depend on emptiness and rows: handled by counters only where routes exist. Tags are otherwise sound: every other tagged pair matched exactly.
