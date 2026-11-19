# PROBE-SH: box caps on T2 o1t3f1s0 (commit 8ea8a411c3dbb6447b6b22d3a94075ccc457d999 on probe-sh)

Units: half-tiles; size = dx/2+1 by dy/2+1 tiles. Each run prints two 'box' lines (default shape, then program shape); the accepted one is the line whose width matches the reported size.
Caveat: M.Y0 is not pinned (no N port), so the printed baseline dy is 4146 (Y0 sits at its lower bound 0). The real extent is 60 (size 31). A cap on Y-Y0 still binds (Y0 slides up), so dy steps start at 60. With a dy cap the printed dy becomes tight.
The cap also changes which shape wins: under a width cap the default shape (dx 136) takes over from the program shape.

## 1 Baseline: 932.25, 75x31, 3518 ms, dx0=148, dy0=60 (printed 4146)

## 2 PLANAR_MAXDX (rows exempt: no)
| cap | area | size | ms |
|---|---|---|---|
| 146 | 944.25 | 74x32 | 1198 |
| 144 | 956.25 | 69x34 | 891 |
| 142 | 956.25 | 69x34 | 890 |
| 140 | 956.25 | 69x34 | 892 |
| 138 | 956.25 | 69x34 | 741 |
| 136 | 956.25 | 69x34 | 748 |
| 134 | 967.25 | 68x34 | 731 |
| 132 | 943.25 | 67x34 | 699 |
| 130 | no variant solves: a clash is not separable (infeasible, not budget) | | |

IIS at 130 (sh-iis-dx.txt): only "integer-only (LP feasible)": no rows named. Note the explain covers only the last of the four separation options tried for the first clash.

## 3 PLANAR_MAXDY
| cap | area | size | ms |
|---|---|---|---|
| 60 | 932.25 | 75x31 | 1589 |
| 58 | no variant solves: base infeasible (before any separation) | | |

IIS at 58 (sh-iis-dy.txt, 45 rows; one extra "integer-only" line from a second explain):
- seg: 17
- cross: 10
- deck: 8
- box: 2
- bundle offset: 2
- flex: 2
- L689: 1
- cap: 1
- integer-only (LP feasible): 1
- prio row offset: 1
- strands: AF1, B1, B1", B2, B2", D1, D2, E1, E2, F1, prio:E1

## 4 PLANAR_ROWFREE=1
Baseline: 834.25, 67x31, 1555 ms, dx0=132 (printed dy 4146; real 60).
| cap | area | size | ms |
|---|---|---|---|
| 130 | 889.25 | 64x34 | 290 |
| 128 | 889.25 | 64x34 | 282 |
| 126 | 889.25 | 64x34 | 269 |
| 124 | 900.25 | 63x34 | 213 |
| 122 | no variant solves: base infeasible | | |

IIS at 122 (sh-iis-dx-free.txt, 83 rows; one extra "integer-only" line):
- seg: 42
- cross: 11
- deck: 9
- L689: 3
- bundle offset: 3
- tie: 3
- box: 2
- flex: 2
- port order: 2
- cap: 1
- integer-only (LP feasible): 1
- prio coverage: 1
- prio row offset: 1
- side rule D2#4 AB2#0: 1
- side rule D2#4 AB2#1: 1
- strands: AB2, B1, B1", B2, B2", CB2, D1, D2, E1, E2, ED1, ED2, F1, F2, prio:E2


Files: /tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/sh-*.txt
