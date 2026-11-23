
### STEP2B T2

- STEP2B T2 lock reference (no flank rows): Optimal 2358 ms
- STEP2B T2 stage A (no keep-apart rows, tidy on, K 64, flank rows on): Infeasible 77 ms, flank rows written 47
- STEP2B T2 conflict rows 101: [piece ED1 0] [piece ED1 1] [piece ED1 2] [piece ED1 3] [piece ED1 4] [piece AB1 0] [piece AB1 4] [piece CD1 0] [piece CD1 1] [piece CD1 2] [piece CD1 3] [piece CD1 4] [piece CD2 0] [piece CD2 1] [piece CD2 2] [piece CD2 3] [piece CD2 4] [piece AF1 0] [piece AF1 1] [piece AF1 2] [piece AF1 3] [piece AF1 4] [piece A2 8] [piece A2 9] [piece A2 10] [piece A2 11] [piece A2 12] [piece A2 13] [piece B1 0] [piece B1 1] [piece B1 2] [piece B1 3] [piece B1 4] [piece B1 17] [piece B1 18] [piece B1 19] [piece B1 20] [piece B1 21] [piece C2 4] [piece C2 5] [piece C2 6] [piece D1 0] [piece D1 1] [piece D1 2] [piece D1 3] [piece D1 4] [piece D1 5] [piece D1 6] [piece D1 7] [piece D1 8] [piece D1 13] [piece D2 0] [piece D2 1] [piece D2 2] [piece D2 3] [piece D2 4] [piece D2 5] [piece D2 6] [piece D2 7] [piece E2 3] [piece F1 9] [piece F1 13] [piece F1 14] [piece F1 15] [piece F1 16] [piece F1 17] [piece F1 18] [piece F1 19] [piece F1 20] [piece F1 21] [piece F2 3] [piece F2 8] [port E gap ED2] [port E gap AB2] [port E gap AB1] [port S gap CD2] [port S gap AF2] [port S gap AF1] [node 0] [node 2] [node 3] [node 6] [node 10] [flank A2 7 before] [flank A2 7 after] [flank B1 18 after] [flank D1 1 before] [flank D1 1 after] [flank D2 1 before] [flank D2 1 after] [flank F2 11 before] [flank F2 11 after] [cross 6] [cross 11] [cross 13] [cross 14] [cross 18] [deck 2] [end 2] [deck 3] [end 3]

## Local freeing (2026-11-24 late)
(a) used inserts locked >= 1, every unused insert free (curve_flex_free):
- STEP2B T2 stage A (no keep-apart rows, tidy on, K 64, flank rows on): Error 15024 ms, flank rows written 158
(b) as the lock, but D1:1, D2:1, A2:7, B1:18, F2:11 set to 0 in the reference (held at 0, not free: the knob cannot free single inserts):
- STEP2B T2 stage A (no keep-apart rows, tidy on, K 64, flank rows on): Infeasible 67 ms, flank rows written 37
- STEP2B T2 conflict rows 68: [piece CD1 0] [piece CD1 1] [piece CD1 2] [piece CD1 3] [piece CD1 4] [piece CD2 0] [piece CD2 1] [piece CD2 2] [piece CD2 3] [piece CD2 4] [piece AF1 0] [piece AF1 1] [piece AF1 2] [piece AF1 3] [piece AF1 4] [piece AF2 4] [piece A2 4] [piece A2 5] [piece A2 6] [piece A2 7] [piece A2 8] [piece A2 9] [piece A2 10] [piece A2 11] [piece A2 12] [piece A2 13] [piece D1 0] [piece D1 1] [piece D1 2] [piece D1 3] [piece D1 4] [piece D1 5] [piece D1 6] [piece D1 7] [piece D2 0] [piece D2 1] [piece D2 2] [piece D2 3] [piece D2 4] [piece D2 5] [piece D2 6] [piece D2 7] [piece D2 8] [piece F1 17] [piece F1 18] [piece F1 19] [piece F1 20] [piece F1 21] [piece F2 8] [piece F2 9] [piece F2 10] [piece F2 11] [piece F2 12] [port S gap CD2] [port S gap AF2] [port S gap AF1] [node 2] [node 3] [node 6] [node 7] [cross 1] [cross 11] [cross 14] [cross 16] [end 0 node] [deck 2] [end 3] [end 3 node]

Reading: (b) has no flank rows in its conflict: those five S-bends are required (ports S, crossings 1/11/14/16, deck 2, end runs). With them used and the other S-bends as the no-flank drawing chose, their flanks do not fit (lock: Infeasible 76 ms). Freeing every unused insert (a) gives no answer in 15 s. Open: whether any S-bend choice fits (needs a per-insert free knob or an offline local model).

## Neighbourhood repair (flex_free_tracks; lock from the no-flank drawing, inserts on the named tracks free)
- STEP2B T2 stage A (no keep-apart rows, tidy on, K 64, flank rows on): Infeasible 133 ms, flank rows written 86
- STEP2B T2 conflict rows 0:
- STEP2B T2 stage A (no keep-apart rows, tidy on, K 64, flank rows on): Error 15021 ms, flank rows written 121
- STEP2B T2 stage A (no keep-apart rows, tidy on, K 64, flank rows on): Error 15075 ms, flank rows written 90

n1 = D1,D2,A2,B1,F2: Infeasible 133 ms, empty conflict (proved by branching): no S-bend choice on those five tracks fits with the others as chosen. n3 = n1 + CD1,CD2 (the trunks): Error 15 s. n2 = all 13 tracks of the lock conflict: Error 15 s. The neighbourhood does not stay small: a fix needs S-bend changes on several further tracks, and the search is hard from 7 free tracks on.

## CP-SAT existence check (2026-11-24, tools/junction/planar_cpsat.py, OR-Tools 9.15, 4 workers; export with grid off, tidy off, every variable integer)
- control, T2 stage A without flank rows (866 variables, 1415 rows): OPTIMAL (feasible) 2.10 s (HiGHS 0.6 s).
- T2 stage A with the 159 flank rows (1574 rows): OPTIMAL (feasible) 28.53 s (HiGHS: no incumbent in 15 s).

Reading: a turn-legal S-bend choice EXISTS for T2 (under rows that are, if anything, slightly strict). The trouble is search, not an upstream Shape gap. Grid parity not included (grid off).
- WITH THE GRID ON (the real use case; parity-substituted export, 1146 variables): control without flank rows OPTIMAL 0.11 s; with the 159 flank rows (1854 rows) OPTIMAL (feasible) 4.60 s. HiGHS on the same model: Error at 15 s (21.9 s).

## HiGHS options, and CP-SAT's S-bend choice locked (2026-11-24)
- JUNCTION_MILP_OPTS mip_heuristic_effort 0.5 / 1.0 (grid on, flank rows, tidy off): Error 21.9 s both.
- CP-SAT (grid on, flank rows) feasible 5.1 s; its piece lengths dumped; stage A in HiGHS with CP-SAT's inserts locked (PLANAR_STAGEA_LOCK_FILE): Optimal 1538 ms (73 flank rows apply). THE CHOICE OF S-BENDS IS THE DIFFICULTY.
- STEP2B T2 contracted variant: per-track turns hold, node turns (F6/F8) FAIL (2 failing line(s)); g_turn_moved 0 -> 0, g_turn_rejected 0 -> 0
  - node 10 point (32,19) rule F8 trunk ED1 starting h2 legs: D1 arriving h2 ; E1 arriving h2
  - node 14 point (26,19) rule F6 trunk E1 arriving h3 legs: prio:E1 starting h1
- STEP2B T2 stage B listing solve (no rows): Optimal 20 ms
- STEP2B T2 stage B: rounds 3 converged false stop 'Infeasible' pairs with rows 55 of 1202 total ms 1887
| B | 0 | Optimal | 21 | 0 | 0 | 0 | 55 | - |
| B | 1 | Infeasible | 648 | 0 | 55 | 220 | - | - |
| B+ | 1 | Infeasible | 1217 | 0 | 55 | 220 | - | 0 |
- Flank-rule gaps: node 14 F6 is a beside leg (prio:E1; beside tracks are skipped by the rule); node 10 F8 (D1 and E1 arriving h2) not yet explained.
- Stage B with that S-bend choice: keep-apart Infeasible at 55 pairs (proved by branching): an S-bend choice made without keep-apart does not leave room for the spacing.

## Conflict-driven S-bends from zero (PLANAR_STAGEA_IIS, 2026-11-24)
- STEP2B T2 IIS round 0: Infeasible 72 ms, conflict rows 192, tracks named 20, newly freed 20, free tracks 20
- STEP2B T2 IIS round 1: Error 15028 ms, conflict rows 0, tracks named 0, newly freed 0, free tracks 20
- STEP2B T2 stage A (no keep-apart rows, tidy on, K 64, flank rows on): Error 15026 ms, flank rows written 130

Reading: the first conflict (every insert held at 0) is 192 rows naming 20 tracks, nearly the whole hub; freeing their inserts is the full free search again (Error at 15 s, empty conflict). The LP conflict does not localise the missing S-bends on T2, so conflict-driven proposals do not get past round 1, before keep-apart is even reached.

## Legal T2 by CP-SAT, and node flanks (2026-11-24)
- T2 stage A with EVERY keep-apart row (7780 side binaries), flank rows, grid on, coord_max 128: CP-SAT feasible in 92 s (rerun 189 s); with coord 4096 and the objective: unknown at 180 s. HiGHS with CP-SAT's S-bend choice locked: Error 15 s (the side binaries); HiGHS with CP-SAT's full solution as start (start_all): Optimal 1.0 s.
- The independent turn check refuses that drawing: node 10 F8 (D1, E1 arrive h2), node 11 F6 (prio:E2 h4 vs ED2 h2); stage B listing Infeasible. Cause: a leg's node-side piece (Shape's one-eighth node turn) may be 0; the flank rows cover inserts only, and beside legs not at all.
- node_flank (probe option): every track end at a node keeps its end piece >= its least length (V's 197: 36 of 36). T1 stage A (flank + node_flank, no keep-apart): HiGHS Error 15 s; CP-SAT OPTIMAL (feasible) in 0.06 s. T2: HiGHS Error 15 s.

Reading: on this problem class HiGHS is the bottleneck (T1: 0.06 s CP-SAT vs no incumbent in 15 s).

## Incumbents, node flanks, T1 legal, T2 node 10 (2026-11-24)
- JunctionMilp returned Error on a time limit even with a feasible incumbent: every 'no incumbent in 15 s' claim for a tidy-on run was unproven (T1 with flank + node flanks: HiGHS had a drawing at 1.4 s, 72 by 9.4 s, gap 11% at 15 s). Probe knob JUNCTION_MILP_INCUMBENT returns the incumbent. Tidy-off Errors (T2 flank 21.8 s, step 1) are real: there the first incumbent is optimal.
- HiGHS mip_heuristic_run_feasibility_jump=true: no help on T1 (one root attempt, fails). CP-SAT's T1 first solution is its fj worker (43 ms); 1 worker: nothing in 60 s. Chuffed (free search) 21.7 s; Gecode nothing in 60 s (D450 benchmark, tools/junction/planar_fzn.py).
- node_flank fixed: a node-end flank keeps its least length, a node-end insert is held at 0. T1 (flank + node_flank, incumbent): stage A 15 s (incumbent), turns hold, stage B 13 of 280 pairs 35 ms, polish 136 -> 130, judge accept, domain accept (0 breaks).
- T2 (CP-SAT start): node 10 F8 persists: D1's Shape headings end '... 2 3 2' (last piece a flank at h2, length 2), E1 and trunk ED1 h2: Shape brings both legs in parallel. Upstream question: Shape's node rule vs F8 at node 10.

## Shape's node waiver at prio joins (2026-11-24, BRIEF-shape-node-waiver.md, stopped, reverted)
- Red test (not kept): a non-straight Line leg at the trunk heading (a cusp) on every kept variant: T2 node 8 B1, node 9 B2, node 10 D1 (16 variants each); T3 node 12 B1, 13 B2, 14 B3, 17 D3 (16 each), 16 D2 (1); LL2 none. Every one is a prio join (a Row at the node).
- Forcing those Line legs to turn (d = 1) makes the heading program infeasible on T1, T2 (0 variants shaped) and others: Shape needs the parallel joiner (its last stretch is monotone from its last crossing).
- Reading: in the game a prio joiner turns in at the merge tile (the checker's streamlined pair of half pieces); in our terms an S-bend insert flush against the node, USED. node_flank held exactly that insert at 0. Rule for the bend pre-program (node shapes, D449): at a node where Shape gives a Line leg the trunk heading, that leg's node-end S-bend toward the trunk is forced used; elsewhere node-end inserts stay 0 and node-side flanks keep their length.
