
### T2 stage A round 1 (offers 1, D443 off)

round 0: Optimal ms 2366, failing pairs 51; biggest 28; tight 64

| run | change | status | ms | binaries | start_values | failing without rows |
|---|---|---|---|---|---|---|
| R0 | none | Error | 15009 | 204 | - | - |
| R1 | coord_max tight | Optimal | 4342 | 204 | - | 39 |
| R2 | tidy off | Optimal | 4534 | 204 | - | 46 |
| R3 | tight + tidy off | Optimal | 582 | 204 | - | 69 |
| R4 | R3 + start_lengths | Optimal | 584 | 204 | 240 | 69 |
| R5 | tight, fixed sides, no binaries | Infeasible | 30 | 0 | - | - |
| R6 | R5 + grid off | Infeasible | 26 | 0 | - | - |
| R7 | R3 + grid off | Optimal | 860 | 204 | - | 102 |

Reading:
- R0 (default coord_max 4096) is Error at the 15 s cap, 204 binaries, as lazy-3.
- `coord_max = 64` alone (R1) moves round 1 to Optimal in 4342 ms; `tidy = false` alone (R2) to Optimal in 4534 ms.
- Both together (R3) give Optimal in 582 ms; a length start (R4) gives 584 ms (240 variables started), no change.
- Fixed sides without binaries (R5, R6) are Infeasible in about 30 ms, with grid on or off.
- R3 with grid off (R7) is Optimal in 860 ms. Pairs failing without rows: 39 (R1), 46 (R2), 69 (R3, R4), 102 (R7); round 0 had 51.
