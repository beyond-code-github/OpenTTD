## T2 offers 1 d443 off, stage A (no keep-apart rows, tidy on): status Optimal ms 2356

node turns (F6/F8) on the contracted drawing: FAIL, 1 failing line(s)

### node 10 point (26,17) rule F8 trunk ED1 starting h2 legs: D1 arriving h2 ; E1 arriving h2

track ED1 (starting: first 4 pieces), point at node in stage A (26,17)
- uncontracted v (piece: heading, stage A length, kind): [0: h2 len -0 flex flank] [1: h1 len -0 flex middle] [2: h2 len 0 flex flank] [3: h3 len 0 flex middle]
- contracted plain (piece: heading, length): [0: h2 len 2]

track D1 (arriving: last 4 pieces), point at node in stage A (26,17)
- uncontracted v (piece: heading, stage A length, kind): [10: h1 len 2 flex middle] [11: h2 len -0 flex flank] [12: h3 len -0 flex middle] [13: h2 len -0 flex middle]
- contracted plain (piece: heading, length): [2: h1 len -0] [3: h2 len 20] [4: h1 len 2] [5: h2 len -0]

track E1 (arriving: last 4 pieces), point at node in stage A (26,17)
- uncontracted v (piece: heading, stage A length, kind): [0: h2 len 6 fixed]
- contracted plain (piece: heading, length): [0: h2 len 6]

## stage B listing solve (no keep-apart rows): status Infeasible ms 24
conflict: [piece CD1 0] [piece CD2 0] [piece AF1 0] [piece A2 2] [piece A2 3] [piece A2 4] [piece A2 5] [piece B1 5] [piece B1 6] [piece B1 7] [piece C2 0] [piece C2 1] [piece C2 4] [piece D1 0] [piece D1 1] [piece D1 2] [piece D2 0] [piece D2 1] [piece D2 2] [piece E2 0] [piece E2 1] [piece F1 3] [piece F1 4] [piece F1 5] [piece F2 2] [piece F2 3] [piece F2 4] [port S gap CD2] [port S gap AF2] [port S gap AF1] [node 2] [node 3] [node 5] [node 6] [node 7] [cross 5] [cross 6] [cross 11] [cross 14] [cross 15] (49 tags in all)

## Reading

- One node fails: node 10 (trunk ED1 starting h2; legs D1 and E1 arriving), rule F8: D1 and E1 both read heading h2 at (26,17).
- E1 is one fixed piece (h2, length 6): unchanged by contraction.
- D1's contracted last piece (plain 5, h2) is the zero-length (-0) flex flank (v 11) that absorbed the removed middle (v 12-13, h3 then h2, both length 0). A flex flank, not a fixed piece.
- The piece before it (plain 4, h1, length 2) is the kept insert's second piece (v 10, a flex middle); the drawing really arrives on h1, and h1 is within one eighth of the trunk's h2 and differs from E1's h2.
- `NodeTurnsHold` reads `pieces.back()` whatever its length, so it takes the zero-length flank's h2 instead of the last nonzero piece's h1; only whole zero-length tracks are transparent, not zero-length end pieces.
- In the uncontracted v the last piece (v 13, h2) is a zero-length middle, so the same read gives h2 there too (not run through `NodeTurnsHold`).
- Stage B's listing solve is Infeasible in 24 ms (49 conflict tags, the first 40 listed above).
