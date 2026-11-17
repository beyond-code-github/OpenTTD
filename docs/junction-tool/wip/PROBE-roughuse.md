# Probe: rough-solve uses (DESIGN-tracks §10 items 1, 2), base 9cf540691c, -j 6, release
Tables: scratchpad roughuse/ (an.out has the full analysis, *.off/warm/bound.txt, *.bound.log). Patch: probe-roughuse.patch.

## Premises that differed from the brief (answered by the coordinator)
- In-model warm start already exists: Model::Solve passes `last` to HiGHS setSolution, so every lazy round, probe solve and area pass of one model is warm in "off" (option b = off). Only the first solve of each new ladder model was cold.
- The first SolveLazy1 objective is track length, not area (area comes from LineariseArea in the second pass).

## Part A: option (a) = seed the ladder model's `last` from the accepted solution (columns equal; no mismatch seen; setSolution returned no error; HiGHS log not enabled, so acceptance beyond "no error" is not shown)
Areas identical to off on all 3 hubs (0 differing).
| hub | off median/max/sum ms | (a) median/max/sum ms |
| T2 (28 solved) | 86 / 873 / 6825 | 118 / 864 / 6724 |
| T3 (27) | 179 / 476 / 6650 | 181 / 579 / 6828 |
| LL2 (189 of 201) | 5613 / 10405 / 1174981 | 4213 / 8566 / 908206 |
T2/T3 differences are noise. LL2: median -25 %, sum -23 %, max -18 % (one run each; machine noise not measured).

## Part B: LP relaxation (integers and parity relaxed, obj = track) per variant
LP track <= rough track: always (0 violations on T2, T3, LL2). LP ms median/max: T2 4.8/32, T3 9.8/68, LL2 9.0/52. The cold rough first solve is median 16 / 31 / 899 ms.
(LP area and W+H are from the LP solution rounded to integers by Model::Solve; not a bound.)
Spearman vs final ladder area, and is the final best variant (ties: best-ranked) in the top 1/3/5/10:
| key | T2 rho; top 1/3/5/10 | T3 rho; top | LL2 rho; top |
| LP track | 0.04; 0 0 0 0 | -0.10; 0 0 0 0 | 0.83; 0 0 0 0 |
| LP area | 0.84; 0 1 1 1 | 0.66; 0 0 1 1 | 0.90; 1 1 1 1 |
| LP W+H | 0.17; 0 0 0 0 | -0.12; 0 0 0 0 | 0.91; 0 0 0 0 |
| rough track | 0.39; 0 0 0 0 | 0.16; 0 0 0 0 | 0.83; 0 0 0 0 |
Best variant's rank: LP area 3 / 4 / 1; LP track 18 / 20 / 36; LP W+H 11.5 / 13.5 / 33; rough track 16 / 18 / 83.
Prune simulation, score = 10*(W+H) + track, LP score as bound, rough integer score as final, ascending bound order (ms saved = off-run ms of the skipped variants minus their LP ms):
T2 skip 18/28, saves 3778 of 6825 ms; T3 skip 18/27, saves 4040 of 6650; LL2 skip 147/189, saves 919533 of 1174981 ms (78 %).
Caveat: the prune uses the rough score as "final", and the final ladder area is not that score; whether the area-best variant survives was not tested.

## Reading
- Warm start (a) is a cheap LL2 win (~23 %) with identical areas.
- LP area ranks the final best variant first on LL2 and in the top 3-4 on T2/T3, but it is not a bound; LP track and W+H alone do not rank well on T2/T3.
