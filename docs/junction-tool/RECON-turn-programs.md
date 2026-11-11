# RECON turn programs: today's flat T as per-movement programs, and which lengths the rules fix (for N0c2, read-only, 2026-11-11)

Sources: FLAT-T-CHAINS (units, curves), the t2-dump section (a) (tiles; the dump is 41 x 13 tiles, same coordinates as the chains), RECON-edge-rules, DESIGN-nodes 4a, DESIGN-N3 5, DIAG-flat-speed Q1, junction_chain.cpp:25 (JunctionCurveGaps), junction_generator.cpp:88/222 (DeriveCurveSpacing, MakeJunctionFrame), DECISIONS rows named inline. Anything not read from those is marked INFERRED. Test spec: TL 4, p = prio_tiles 14, CL 5, g 1.
Conventions. Grid terms, y down: E = +x, S = +y, SE = (+x,+y) etc. Unit = half a tile; an X/Y piece 2 units, a half piece (diagonal) 1, a bridge tile 2. A node's coordinate is the chain's: a split/merge is at the tile of the first diverging (or last joining) half piece. Ports: A (EIn->EOut) and the W line (WIn->WOut) have 3-tile ports at x 0..2 (west) and x 38..40 (east); the N port is column 26, the S port column 28, each with a 2-tile stub (rows 12 signal tile, 13 boundary) below its switch tile at row 11. Rows: A row 1, prio row of A row 0, gap row 2, W lane A row 3, W lane B row 4. Layout x = working x - 14 (xI 40 -> 26, xO 42 -> 28, MakeJunctionFrame hub_x = 30 + max(0, p - TL) = 40).

## 1. Turn programs (run = direction, pieces, kind, ends; u = units; node in [brackets])
**P1 EIn->SOut (71u), hands R R L R**
1. E, 23 tiles, X run, (0,1)-(22,1), 46u. Port x 0..2.
2. SE, 5 halves, diagonal, (23,1)-(25,3), 5u. [split A->S at (23,1)] (25,3) is under lane-A bridge.
3. S, 3 tiles, Y run, (25,4)-(25,6), 6u. (25,4) under lane-B bridge.
4. SE, 6 halves, diagonal, (25,7)-(28,9), 6u. (26,7),(26,8) under N->E's bridge.
5. S, 4 tiles, Y run, (28,10)-(28,13), 8u. [merge with B->S at (28,11), unit 65] stub (28,12),(28,13).
**P2 EIn->EOut = A (82u)**: 1. E, 41 tiles, X run, (0,1)-(40,1). [split A->S (23,1) u46; prio merge N->E (38,1) u76]. Prio row 0, x 24..38.
**P3 WIn->WOut lane A (82u), hands L R R L**
1. W, 11 tiles, X run, (40,3)-(30,3), 22u. [split B->S (33,3) u14; lane split (31,3) u18].
2. W, bridge span, 7 tiles (heads (29,3),(23,3), middle 5), 14u.
3. SW then W, 2 halves, S-bend (one row), (22,3),(22,4), 2u. [lane merge at (22,4)].
4. W, 15 tiles, X run, (21,4)-(7,4), 30u. [prio merge N->W at (6,4) u68].
5. W, 4 tiles, X run, (6,4)-(3,4), 8u.
6. NW then W, 2 halves, S-bend, (2,4),(2,3), 2u. 7. W, 2 tiles, X run, (1,3),(0,3), 4u. Port.
**P4 WIn->WOut lane B (82u), hands L R R L**
1. W, 9 tiles, X run, (40,3)-(32,3), 18u. [split B->S (33,3); lane split (31,3)].
2. SW then W, 2 halves, S-bend, (31,3),(31,4), 2u; the near head (30,4) is the second curve (D110).
3. W, bridge span, 7 tiles (heads (30,4),(24,4), middle 5), 14u.
4. W, 1 tile (23,4), 2u. 5. W, 16 tiles, X run, (22,4)-(7,4), 32u. [lane merge (22,4) u36; prio merge (6,4) u68]. Then as P3 steps 5-7 (shared tiles).
**P5 WIn->SOut (35u), hands L L R L**
1. W, 7 tiles, X run, (40,3)-(34,3), 14u. 2. SW, 5 halves, diagonal, (33,3)-(31,5), 5u. [split (33,3) u14].
3. S, 3 tiles, Y run, (31,6)-(31,8), 6u. 4. SW, 6 halves, diagonal, (31,9)-(28,11), 6u. [merge (28,11) u30, the 6th half piece].
5. S, 2 tiles, Y run, (28,12),(28,13), 4u (stub).
**P6 NIn->EOut (47u), hands R R L R**
1. N, 4 tiles, Y run, (26,13)-(26,10), 8u. [split N port (26,11) u4; signal tile (26,10)].
2. N, bridge span, 4 tiles (heads (26,9),(26,6), middle 2, single, over P1), 8u.
3. N, 1 tile, Y run, (26,5), 2u. 4. NE, 5 halves, diagonal, (26,4)-(28,2), 5u. (26,4),(27,4) under lane B; (27,3),(28,3) under lane A.
5. E, 9 tiles, X run, (29,2)-(37,2), 18u, on the gap row.
6. NE then E, 2 halves, S-bend, (38,2),(38,1), 2u. [prio merge onto A at (38,1) u42]. 7. E, 2 tiles, X run, (39,1),(40,1), 4u. Port.
**P7 NIn->WOut (57u), hands L L R L R L**
1. N, 2 tiles, Y run, (26,13),(26,12), 4u. [shared split (26,11)].
2. NW, 13 halves, diagonal, (26,11)-(20,5), 13u. No crossing.
3. W, 13 tiles, X run, (19,5)-(7,5), 26u, row 5 (the joiner's row, beside the W line; W prio row is row 3).
4. NW then W, 2 halves, S-bend, (6,5),(6,4), 2u. [prio merge (6,4) u44].
5. W, 3 tiles, X run, (5,4)-(3,4), 6u. 6. S-bend (2,4),(2,3), 2u (shared with P3/P4). 7. W, 2 tiles, X run, 4u. Port.

## 2. Diagonals: hands, length, rule minimum at the spec (CL 5, TL 4)
Rule: JunctionCurveGaps: consecutive same-hand min(2TL-1, max(2,CL)) = 5; consecutive opposite 2; non-consecutive (i,j) min(7, (j-i) CL) = 7 for j-i >= 2 (checked for all pairs below: every non-consecutive gap in the T is >= 8, so only the consecutive rule binds).
| diag | path | hands | units | rule min | at min? | what else fixes it |
|---|---|---|---|---|---|---|
| D1 | P1 (23,1)-(25,3) | R R | 5 | 5 | yes | |
| D2 | P1 (25,7)-(28,9) | L R | 6 | 2 | no | 2 x (S column 28 - leg column 25): the S-bend must also sit under N->E's bridge middle (rows 7,8). INFERRED |
| D3 | P5 (33,3)-(31,5) | L L | 5 | 5 | yes | |
| D4 | P5 (31,9)-(28,11) | R L | 6 | 2 | no | 2 x (31 - 28), same column shift as D2 |
| D5 | P6 (26,4)-(28,2) | R R | 5 | 5 | yes | |
| D6 | P6 (38,2),(38,1) | L R | 2 | 2 | yes | the prio merge sits inside it |
| D7 | P7 (26,11)-(20,5) | L L | 13 | 5 | no | 2 x (rows 11 - 5) + 1: the N split row (= S merge row = depth) and the joiner row 5 = W row + 1. INFERRED |
| D8 | P7 (6,5),(6,4) | R L | 2 | 2 | yes | prio merge inside |
| D9 | P3/P4/P7 (2,4),(2,3) | R L | 2 | 2 | yes | shift-back S-bend, port column x_w + 2 |
| D10 | P3 (22,3),(22,4) | L R | 2 | 2 | yes | lane merge inside |
| D11 | P4 (31,3),(31,4) | L R | 2 | 2 | yes | lane split inside; the head closes it (D110) |
Same-hand 90-degree corners (D1, D3, D5) need an ODD half-piece count (RECON-edge-rules 1, test "LFFL" = 3 halves = 132 km/h), so the geometric minimum is the least odd d >= CL (INFERRED for CL 2, 4, 6), which JunctionCurveGaps does not state. Opposite pairs: D2/D4 are fixed by column shifts, D7 by rows; none is limited by a curve gap.

## 3. X/Y runs: lower bounds
- A split to merge (P2, 15 tiles = 30u = 2p + 2): G9c/D148 floor join - x_s >= p + 1 (x_s = last X tile before the diverging half piece), `JUNCTION_E_PRIO_AFTER_SPLIT` = 2, e_phase 0 at p 14 (D148a). Chain S (23) to M (38) is exactly 15 tiles; RECON-edge-rules / DESIGN-N3 give x_s 36 and join 52 (16): one-tile convention gap, see Q1. At the floor either way.
- A west of the split, 23 tiles: bound only port (3 tiles) + signal before the split (G6); the 23 is slack from the frame (section 4). A east of the join: 2 tiles = port stub (D179: join may be the port's first tile).
- P1 Y leg 3 tiles: curve gaps allow 1 tile (R46 -> L57 needs >= 7, got 11). 3 comes from N->E's bridge position (heads rows 9 and 6). INFERRED.
- P1 last Y (28,10)-(28,13): stub 2 tiles (D115a, G10) plus 1 signal-legal piece before the merge (G6, D113).
- P5 Y leg 3 tiles: curve gaps allow 1 tile (14 -> 25 needs >= 7). It is the free vertical run absorbing depth: rows 3..5 corner, 6..8 leg, 9..11 S-bend = merge row 11 (DESIGN-N3 5 "free Y runs"). G7 bay signals (rules.cpp:836-960, 8u bays) could raise it; INFERRED not binding here (split to merge 16u).
- P6 Y: (26,10) = D113 signal piece between the split and the near head; (26,5) = D113 piece after the far head (1 tile each, minimum). Bridge B2: crossing rows 7,8, heads clear k_head 1 (rows 6, 9), middle 2 (<= nmax 10, D172). Single (D174).
- P6 X run row 2, 9 tiles: curve gaps allow 0; set by columns: diag end 29 = N column + 3, merge at 38 (prio). Free run.
- Lane bridges, n = 5 each, equal (D80, line sync): crossing row 3 = x 25,27,28 (width 4), row 4 = x 25,26,27 (3); heads (23,3),(29,3) and (24,4),(30,4). Skeleton rule hw <= zmin-1, he >= zmax+1, 2 <= he-hw <= nmax+1, D172 window D = nmax-1-2 K_HEAD = 7 (hi - lo <= 7, here 3 and 2). Lane B's near-west head (24,4) = zmin-1 and lane A's east head (29,3) = zmax+1 are both tight; the equal-n + stagger 1 (D118) forces n 5. INFERRED from the dump.
- Signal legs (D113/D118/D145, RECON-edge-rules 2-3): lane split to near head: lane A 1 tile (30,3), lane B 1 piece (31,4); far head to lane merge: lane A (22,3), lane B (23,4); each exactly one legal piece. Lane split (31) to B->S split (33): 2 (D145 x_sl - x_ls = 2).
- Lane merge (22,4) to prio merge (6,4): 16 tiles in the dump; the pinned parallelogram minimum is p + k_w = 15 (D145 test "constants are minimal", RECON-edge-rules 3, JUNCTION_PRIO_AFTER_MERGE 1). Dump has one tile more, INFERRED same off-by-one as above.
- Prio merge (6,4) to shift-back (2,4): 4 tiles = 8u, `JUNCTION_SHIFT_BACK_TILES` 4 (D145, G9c). At minimum.
- P7 X run row 5, 13 tiles: curve gaps allow 0; set by diag end column 19 = N column - 7 (D7) and the join column 6. Free run.
- W east of the B->S split: 7 tiles (40 - 33): port 3 tiles + signal before split; slack from east alignment.

## 4. What fixes the arrangement
Vertical legs west to east: A->S leg x 25 | N column x 26 (P6) | S column x 28 (P1 end, P5 end) | B->S leg x 31.
- S column = N column + 2 (xO = xI + branch_gap 1 + 1, junction_generator.cpp frame; D115a stubs). Frame constants, not rules.
- A->S leg = (A join 38) - 15 + 2 = 25: join 38 = x_e - 2 (port), split = join - 15 (D148), leg = split + (5-1)/2. B->S leg = B->S split 33 - 2 = 31 (corner D3), split 33 = lane split 31 + 2 (D145), lane split 31 = lane-A east head 29 + 2 (D118/D113), head 29 = zmax_A + 1 = 28 + 1. Both legs are 3 columns from the S column (S-bends of 6u, D2 and D4): the symmetry is arithmetic, not a rule. INFERRED.
- West chain (from x_w = 0): shift-back (2,4) = x_w + 2, join 6 (+4), lane merge 22 (+16), far head B (24,4) (+2), so A->S leg >= 25 (must clear lane B's far head, zmin_B - 1 = 24). East chain: leg = x_e - 15. So x_e - 15 >= x_w + 25 -> x_e = x_w + 40: the T's width is exactly tight on this chain (INFERRED arithmetic from the numbers above). Hub offset from x_w: 6 + (p + 2) + 3 + 1 = p + 12 = 26, equal to the frame's xI - x_w only because xI = 30 + (p - TL) (INFERRED, p 14 TL 4 only).
- Crossings in today's T (3): (a) W line (both lanes) over A->S, (b) W line (both lanes) over N->E, (c) N->E over A->S (B2, single). Order: W line > N->E > A->S. A, N->W and B->S cross nothing (they merge with or split from a line on the near side).
- Forced by ports + movements (D189): (a) and (b) exist because the W line lies between A and the S/N ports (line order); (c) exists because N sits west of S while A->S comes from the west and N->E goes to the east: two paths with swapped order cross an odd number of times (INFERRED: once). Node order: splits before merges on each line (A: 23 then 38; W: 33, 31 then 22, 6).
- Choices: over/under for (a), (b), (c) (main line over first, D156/D190; W lane bridges doubled D80; B2 single D174); lane stagger 1 (stagger 0 ties on span, RECON-edge-rules 6); whether A->S passes west of the N column (leg 25 < 26); bridge vs tunnel; doubling follows throughput (D189).

## 5. Required diagonal length by CL (TL 4, cap 2TL-1 = 7; curve spacing per DIAG-flat-speed Q1)
Corner d = least odd d >= max(2, CL), at most 7 (rule value in brackets where it differs). k = (d - 5)/2 = columns and rows added per corner relative to the spec.
| CL (km/h) | 2 (100) | 3 (120) | 4 (140) | 5 (160, spec) | 6 (180) | 7 (196) |
|---|---|---|---|---|---|---|
| D1 (P1), D3 (P5), D5 (P6) each | 3 (2) | 3 | 5 (4) | 5 | 7 (6) | 7 |
| k per corner | -1 | -1 | 0 | 0 | +1 | +1 |
| S-bends D6, D8-D11 | 2 | 2 | 2 | 2 | 2 | 2 |
| D2, D4 (column-fixed) | 6 | 6 | 6 | 6 | 6 | 6 |
| D7 (row-fixed) | 13 | 13 | 13 | 13 | 13 | 13 |
First-order effect per path (INFERRED, to be settled by the solve): P1 and P5 and P6 each add k rows/columns, P2, P3, P4, P7 add none directly. A->S leg moves east by k (split fixed by the join), the crossing set moves with N->E's diagonal end (+k), depth (S/N row) +k through D1 and D5, which D7 absorbs (+2k units) and P5's free Y leg absorbs (+k tiles). At k = +1 the A->S leg (26) coincides with the N column unless the hub moves east (Q6). The sampled pairs (i, i+2) stay satisfied: every non-consecutive gap in the T is >= 8 >= min(7, 2 CL). At CL 2 and 3 the bound check "gap 2 vs d 3" is the parity question Q2.

## 6. Open questions for the orchestrator
1. Which split/join convention does the generator use: the dump has chain S 23 and M 38 (15 tiles), RECON/DESIGN-N3 5 say x_s 36 and join 52 (16); is today's T at the D148 floor or one tile above it?
2. CL 2 (100 km/h): JunctionCurveGaps allows a same-hand gap of 2, a same-hand corner geometrically needs d >= 3 (odd): which does N0c2 emit?
3. Lane merge to prio join is 16 tiles in the dump, the pinned D145 minimum is 15: derive 15 or reproduce 16?
4. Are the east and west chains (width x_e = x_w + 40, hub = x_w + p + 12) the rule N0c2 derives positions from, instead of the frame's xI = 30 + (p - TL)?
5. A->S's Y leg length 3 (S-bend under B2's middle) and P5's Y leg 3 (depth absorber): rule, or searched?
6. At k >= +1 (CL 6, 7) the A->S leg reaches the N column: shift the hub east (T widens by k) or place the A->S leg east of the N column and change the crossing?
7. Crossing assignment as input: confirm W line over both and N->E over A->S as the default order (D156/D174/D190), and the tunnel variant.
8. D172's window D = 7 versus the skeleton bound he - hw <= nmax + 1 for crossing width: which does N0c2 use (here widths 4 and 3)?
9. Do the free runs (A west 23, W east 7, P6 X run 9, P7 X run 13) get minimum-length rules (port 3 + signal) or stay slack outputs of the solve?
10. Does N0c2 model G7 waiting bays (8u) as a lower bound on the P5 and P1 legs, or leave them to the checker?
