# Probe: ladders on every face run, rung spacing from the clash rule (2026-11-17, HEAD 05e0159b09 + probe-ladders3 + this)

Result: every run against every other run plus the 1/2 spacing removes all first-pass round-0 clashes on T2 (80 to 0) and LL2 (4740 to 0), T3 91 to 6. No area worse anywhere; rough rows violated: 0 on every hub.
Code: wip/probe-ladders4.patch (on probe-ladders3.patch; PLANAR_LADDER=3). Raw: scratchpad `ladders4/` (summary.txt, T2/ T3/ LL2/, parse.py).

## Changes
- Face runs: PlanarCorridor.runs (planar_regions) = maximal cyclic runs of F or B darts in walk order, ring edges ignored. CorrWall.runs; RungsFrom loops every ordered pair (X,Y), target side from Y (F run: left, B run: right). One-run faces skipped as before.
- RungSpacing(target_d, own_d): 1 when the target is axis and the own segment is diagonal or perpendicular, else 2 (comment holds the DIAG findings). A point shared by two own segments (same position, same target) is one row with the smaller constant.

## Per hub (round 0 = first lazy pass, as in ladders3; area-pass round 0 is the second lazy pass)
| hub | solved rough/ladder | r0 rough to ladder (variants at 0) | area-pass r0 | area b/s/w | ladder ms med/max |
| T2 | 28/28 of 32 | 80 to 0 (18 to 28) | 112 to 12 | 0/28/0 | 38/233 |
| T3 | 27/27 of 32 | 91 to 6 (20 to 25) | 200 to 12 | 0/27/0 | 69/174 |
| LL2 (201 tags) | 189/189 | 4740 to 0 (0 to 189) | 254 to 0 | 16/173/0 | 1368/2831 |
- Rows violated on the rough solve: 0 on all 244 solved variants. Rough infeasible at HEAD (no ladder): T2 4, T3 5, LL2 12 (same as ladders3).
- Area better (LL2, all f0): o1t5f0s1/s3 -39; o3t{0,1,4,5,6,7}f0s1/s3 -154; o3t3f0s1/s3 -183. No worse.
- Surviving first-pass pairs: T3 o1t2f1s0 and o1t2f1s1: AF3#1 vs D3#0, AF3#1 vs D3#1, AF3#2 vs D3#0 (D3#0 and AF3#0 lie in a skipped one-wall box face 15; AF3#1/#2 are in no ladder face). T2: none.
- Surviving area-pass pairs: T2 and T3 o1t{0,1,2,3}f1s1: A2#1/A2#2 vs B1"#0/#1 (T3: A3#1/#2 vs B1"#0/#1), 3 pairs per variant, first-pass r0 0 there.
