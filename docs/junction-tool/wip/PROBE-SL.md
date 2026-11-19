# PROBE-SL: face slack caps on T2 (o1t3f1s0), throwaway

Raw outputs: /tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/sl-*.txt. Branch probe-sl (from probe-cm).

The test solves the variant twice. Run A (first "slack" line) is a wider-area solve; run B (second line) is the probe-area solve whose result is the reported area 932.25 75x31. All caps below were tested on both; the cap table gives run B unless noted.

## Face table (no env)
Run B (slack 536.5, 198 rungs):
| face | slack | rungs | strands |
|---|---|---|---|
| 26 | 178 | 25 | B2,D1,E2,F1 |
| 22 | 116 | 23 | B1,C2,D1,F2 |
| 14 | 59 | 19 | A2,B1,B1",D2,F1 |
| 2 | 35.5 | 8 | B2,EF2 |
| 4 | 26 | 10 | B1,C2,CB1,CB2 |
| 5 | 23 | 12 | D1,E2,ED1,ED2 |
| 23 | 20.5 | 14 | B1,B2,C2 |
| 24 | 20 | 12 | B1,C1,C2,D1 |
Run A (slack 902.5, 227 rungs): face 22 207, 2 151, 14 125, 26 116.5, 24 79, 23 68.5, 6 40, 5 27.

## Whole-sum cap (PLANAR_SLACKCAP)
| cap | run A | run B |
|---|---|---|
| 500 | ladder pass fails (rough kept, "slack - rungs 0") | ladder solves: slack 498.5, rungs 195, area 932.25 75x31 (unchanged), track 749.5 |
| 460 | fails (rough kept) | fails (rough kept); final area 932.25 75x31 is the rough result |
| 420, 380, 340 | fails | fails, same |
Run B with cap 500: face 26 178, 22 101, 14 59, 2 35.5 (others as before): the cap was met by taking 15 off face 22 and a few small ones; area did not move.
The solve outcome is the first feasible/infeasible edge between 460 and 500 for run B; the floor is therefore about 480 +- 20 tiles total, against 536.5 free, i.e. the rules force most of the slack.

## Per-face caps (PLANAR_SLACKFACE, run B; run A in brackets)
| face | slack | x0.8 | x0.6 | x0.4 |
|---|---|---|---|---|
| 26 | 178 | cap 142.4 fails (A solved: its face 26 slack is 116.5) | 106.8 fails | 71.2 fails |
| 22 | 116 | 92.8 fails | 69.6 fails | 46.4 fails |
| 14 | 59 | 47.2 fails | 35.4 fails | 23.6 fails |
So each of the three largest faces fails already at 0.8 of its slack: these three faces' slack is forced, not slack from the objective.
Every failure keeps the rough result (area 932.25 75x31).

## IIS
Not obtained. The IIS print does run for a ladder-pass failure (many "solve ... no" lines follow, deletion filter over the core at about 165 ms per LP), but the run is killed by the brief's 30 s timeout before any "IIS" line is printed (sl-cap460-iis.txt, 0 IIS lines). sl-iis-all.txt is empty; sl-iis-f*.txt were not made. A longer timeout (not used, brief fixes 30 s) is needed.
