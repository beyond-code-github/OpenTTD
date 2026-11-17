# Spike 3: shape program with the crossing rules (findings)

Code: spike-shapeflow3.patch (on spike-shapeflow2.patch, on 72c03f4196), `PLANAR_SHAPEFLOW=3`. Raw: /tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/shapeflow3 (T2/T3/LL2 files, all/ = full T2, ll2/ = LL2 tags, sum.py = tables).
Model: LP2 (tied crossing corners x in [1,3], vertex turns and ring fixed) as a MIP, plus per crossing and per line an integer heading H in [0,7] = h0 (rough, line start) + sum b over stretches up to the crossing + within-line turns (all 0) - 8k. Rules: over heading even (u: H = 2u); tie x = +-(Hp - Hq) mod 8 from one corner. "Not parallel" (`suits`: h%4 differ) is implied by x in [1,3], so no binaries were needed. Objective sum|b|. HiGHS, 15 s limit (never reached).

## Consistency
- Every rough layout satisfies the model: over heading odd 0, parallel 0, tie mismatches 0, heading formula mismatches 0 (T2, T3, LL2, all 28 T2, all 189 solved LL2 tags). Within-line turns are all 0.
- `suits` applies only to channel crossings. 6 of T2's 18 line crossings (T3 18 of 45, LL2 3 of 18) are in no channel crossing (inside node groups: A2xB1, B1xC2, C2xD1 ...). For these only the tie applies; no over rule exists in BuildPlanarPaths. Not covered by the brief; left as it is.

## Runs (LP1 / LP2 / LP3 vs rough sum|b|, MIP ms)
- T2 o0t2f1s1: 8 / 4 / **8** vs 12, 40 ms. 9 stretches differ (C2#1/#2, D1#1/#3/#5, F1#1/#3, F2#0/#2); 9 of 36 line-crossing headings differ: D1 0 -> 1 (c3, c7), D1 0 -> 2 (c13, c14), F1/F2 4 -> 3 (c5, c6, c9, c10), C2 7 -> 0 (c2, unmapped).
- T3 o1t2f1s1: 12 / 6 / **6** vs 12, 34 ms. 16 stretches differ; 21 of 90 headings (D 0 -> 1, F 4 -> 3, C 7 -> 0 at c6, c7, c14 unmapped).
- LL2 o1t3f0s0: 15 / 10 / **16** vs 30, 31 ms. 31 stretches differ; 15 of 36 headings (F1, F2, E2b 2 -> 4 at c3, c4, c7, c8, c15; D1, D2 0 -> 1; F1 4 -> 5, 3 -> 4, E2 2 -> 1).
- Full T2 (28 variants): rough 8-16, LP3 4-8 (28 below rough, 0 equal, LP3 = LP2 in 12), MIP <= 40 ms.
- LL2 (189 solved of 201 tags; 12 have no output): rough 28-34, LP3 16-19 (189 below rough), MIP <= 60 ms.

## Answer
Under the crossing rules as `suits` writes them, today's turning is not minimal: in every run LP3 is below the rough (T2 8 vs 12, T3 6 vs 12, LL2 16 vs 30), and no variant has LP3 = rough. For T2 o0t2f1s1 LP3's 8 equals LP1's 8 but is a different shape (LP1's was not buildable under the rules). The saving sits at channel crossings: D over F with D axis (D at 0 -> 2 at c13, c14) and F 4 -> 3 and the B/F, B/D under-crossings, and, in LL2, the fork-leg lines F1/F2/E2b/D1/D2.
Caveats: (1) LP3 gives each unmapped crossing only the tie (T2's C2 7 -> 0 at c2 is one); the fraction of the saving that depends on them is not measured. (2) The headings are only what the rule needs: whether BuildPlanarPaths/composer can build these shapes (turn placement within a stretch, jogs, the prio perpendicular preference, area) is not tested, and the earlier probe found the piece rule fixes where a turn sits.
