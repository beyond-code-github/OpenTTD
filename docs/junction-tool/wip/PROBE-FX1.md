# PROBE FX1 stage 1: the red count

Branch probe-fx1, commit 387f0ac913. Env for all runs: PLANAR_COMPOSER=tracks PLANAR_SLOTBEND=1, FORKMASK/MERGEMASK unset. R0 = baseline, R1 = + PLANAR_NODELEGS=1 PLANAR_NOPORTSTEPS=1. Files in scratchpad/fx1/ (R0-H.txt, R1-H.txt, iis-H-tag.txt, dry.txt, ll2tags.txt).

## Tally (VERIFIED from R0-H.txt / R1-H.txt)

| hub | run | total | built | base infeasible | other failures | solved (best area, tag) |
|---|---|---|---|---|---|---|
| T2 | R0 | 32 | 32 | 8 | 2 "infeasible with topology sides" | 22 (920.25, o0t0f1s0) |
| T2 | R1 | 32 | 32 | 32 | 0 | 0 |
| T3 | R0 | 32 | 32 | 8 | 10 "infeasible with topology sides" | 14 (1929.25, o0t0f1s0) |
| T3 | R1 | 32 | 32 | 32 | 0 | 0 |
| LL2 | R0 | 8192 (dry) | 7168 | 7168 | 0 | 0 |
| LL2 | R1 | 7168 (the R0 built list) | 7168 | 7168 | 0 | 0 |

LL2 dry (dry.txt, VERIFIED): 8192 variants, 7168 built, 512 "no piece of the channel of B1 suits its crossing", 512 "... of F1 ...". The R1 run reuses the R0 built list; the built set under the R1 switches was not re-listed (INFERRED unchanged).
Best area = smallest area among solved. "Best" for LL2: none solved in either run.

## IIS samples (R1, first three base-infeasible per hub in tag order; first block of rows, up to the first "port order")
The T2/T3 outputs hold more blocks than the first (a second solve; T2 o0t0f1s0 and T3 o0t0f1s0 have 17 rows, one block). Only the first block is listed.

- T2 o0t0f0s0 (and o0t0f0s1, identical rows): seg AB2#0-2, B2#0, B2"9#0, D1#1-3, E1#0, E2#2, E2"12#0, ED1#0; cross B2"9#1>D1#1, E2#2>F1#0, E2"12#1>D1#4; deck D1#1 (x3); deck-neighbours; port order. Related (INFERRED from names): lines AB2, B2 and its split leg B2"9, D1, E1/E2 with E2"12, ED1; crossings between B2"9 and D1 and between E2/E2"12 and F1/D1; deck spacing on D1#1; the A/E port order.
- T2 o0t0f1s0: seg AB2#0-2, B2#0, B2"8#0, D2#1-4, ED2#0-2; tie@L3207 (D2#4 to E2#5.end); cross B2"8#1>D2#1 (two rows: L>=1 and the y relation); port order (AB2.start vs ED2#2.end = 4). Related (INFERRED): AB2/B2 split leg B2"8 crossing D2 at D2#1, the tie of D2 to E2, and the A to E port order.
- T3 o0t0f0s0 (and o0t0f0s1, identical rows): seg CD3#0, D3#0, F3#1-4, AF3#0; tie@L3207 (A3#3.end to F3#4.end); cross D3#1>F3#1 (two rows); port order (CD3.start vs AF3#0.end = 4); deck-neighbours (D3#0.L >= 2). Related (INFERRED): lines CD3, D3, F3, AF3; the D3 x F3 crossing and the tie of A3 to F3; C/D to A/F port order.
- T3 o0t0f1s0: seg AB3#0-2, B3#0, B3"23#0, D3#1-4, ED3#0-2; tie@L3207 (D3#4 to E3#8.end); cross B3"23#1>D3#1 (two rows); port order (AB3.start vs ED3#2.end = 4). Same shape as T2 o0t0f1s0 (INFERRED).
- LL2 o0t0f0s0, s1, s2 (identical rows, VERIFIED by file): seg B1a#0-3, B1b#0-1, CBj1#0-1, CBj2#0-1, WO1#0, WO2#0; tie@L3207 x3 (C2a#2/CBj1 to B1a#3; C2b#2/CBj2 to B1b#1; C1#2/WO1 to CBj1#1); port order (WO1#0.end vs WO2#0.end = 2). Related (INFERRED): the B1 split legs B1a/B1b, the two CBj bridge lines, the WO1/WO2 exits and the equal-end ties joining them; no cross or deck row in the set.

Common to every sample (VERIFIED): the IIS contains a "port order" row; every T-hub IIS also has a "tie@L3207" or "cross ... @L804/L3348" row on the y-chain of one line pair.
