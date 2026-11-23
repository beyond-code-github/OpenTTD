
### STEP2B T1

- STEP2B T1 stage A (no keep-apart rows, tidy on): Optimal 169 ms
- turn check: track 4 pieces 1 and 2: headings 1 then -1
- turn check: track 5 pieces 0 and 1: headings 5 then 3
- turn check: track 11 pieces 1 and 2: headings 2 then 4
- STEP2B T1 contracted variant: per-track turns FAIL, node turns (F6/F8) FAIL (4 failing line(s)); g_turn_moved 0 -> 0, g_turn_rejected 3 -> 3
  - node 1 point (8,13) rule F6 trunk CD1 arriving h7 legs: D1 starting h1
  - node 3 point (11,14) rule F8 trunk AF1 starting h5 legs: A1 arriving h4 ; F1 arriving h4
  - node 4 point (2,9) rule F8 trunk CB1 starting h6 legs: C1 arriving h6 ; B1 arriving h6
  - node 5 point (12,3) rule F8 trunk ED1 starting h2 legs: D1 arriving h2 ; E1 arriving h2
- STEP2B T1 stage B listing solve (no rows): Infeasible 0 ms
- STEP2B T1 B (no keep-apart rows) conflict: first solve Infeasible (conflict rows 1, families 1)
 [piece A1 1]
  - drop family [piece]: Infeasible 0 ms
  - families needed:

### STEP2B T2

- STEP2B T2 stage A (no keep-apart rows, tidy on): Optimal 2392 ms
- turn check: track 13 pieces 2 and 3: headings 7 then 5
- turn check: track 18 pieces 1 and 2: headings 0 then 2
- turn check: track 19 pieces 1 and 2: headings 0 then 2
- turn check: track 22 pieces 1 and 2: headings 2 then 4
- STEP2B T2 contracted variant: per-track turns FAIL, node turns (F6/F8) FAIL (7 failing line(s)); g_turn_moved 0 -> 0, g_turn_rejected 7 -> 7
  - node 0 point (15,22) rule F6 trunk AB1 arriving h7 legs: A1 starting h5
  - node 1 point (18,21) rule F6 trunk AB2 arriving h7 legs: A2 starting h5
  - node 7 point (7,22) rule F8 trunk AF2 starting h4 legs: A2 arriving h5 ; F2 arriving h5
  - node 9 point (5,8) rule F8 trunk CB2 starting h7 legs: C2 arriving h7 ; B2 arriving h7
  - node 11 point (21,22) rule F8 trunk ED2 starting h3 legs: D2 arriving h3 ; E2 arriving h3
  - node 13 point (7,14) rule F6 trunk B1 arriving h6 legs: prio:B1 starting h0
  - node 14 point (20,17) rule F6 trunk E1 arriving h3 legs: prio:E1 starting h1
- STEP2B T2 stage B listing solve (no rows): Infeasible 1 ms
- STEP2B T2 B (no keep-apart rows) conflict: first solve Infeasible (conflict rows 1, families 1)
 [piece AF1 0]
  - drop family [piece]: Infeasible 0 ms
  - families needed:

## Reading

- Stage A (no keep-apart rows, tidy on) is Optimal on both hubs (T1 169 ms, T2 2392 ms); lengths are integer; the contraction does not move the drawing (point check passes).
- T1: ContractedVariant exists but 3 tracks have no representation within one eighth per turn (g_turn_rejected 3): per-track turns FAIL (tracks 4, 5, 11); node turns FAIL, 4 lines (node 1 F6; nodes 3, 4, 5 F8); stage B listing Infeasible, 0 ms, one conflict row [piece A1 1]; no loop, no polish, no judge or domain verdict.
- T2: ContractedVariant exists, 7 tracks rejected (g_turn_rejected 7): per-track turns FAIL (tracks 13, 18, 19, 22); node turns FAIL, 7 lines (F6 at nodes 0, 1, 13, 14; F8 at nodes 7, 9, 11); stage B listing Infeasible, 1 ms, one conflict row [piece AF1 0]; no loop, no polish, no judge or domain verdict.
- The conflict families reduce to a single "piece" row on each hub (drop-family keeps it infeasible; no needed family named).
- Both hubs fail before NodeTurnsHold: the stage A drawing has pieces whose contracted writing turns over one eighth, so the contracted variant is not a legal polyline shape.
