
### STEP1 T1

| stage | round | status | ms | pairs fixed | pairs free | binaries | failing pairs | freed |
|---|---|---|---|---|---|---|---|---|
| A | 0 | Optimal | 22 | 0 | 0 | 0 | 22 | - |
| A | 1 | Optimal | 154 | 0 | 22 | 88 | 1 | - |
| A | 2 | Optimal | 155 | 0 | 23 | 92 | 0 | - |

- STEP1 T1 stage A: rounds 3 converged true stop '' pairs with rows 23 of 811 total ms 331
- STEP1 T1 polish: Optimal 136 ms start_values 236 length before 134 after 79.00000000002723 failing without rows 0

### STEP1 T2

| stage | round | status | ms | pairs fixed | pairs free | binaries | failing pairs | freed |
|---|---|---|---|---|---|---|---|---|
| A | 0 | Optimal | 388 | 0 | 0 | 0 | 79 | - |
| A | 1 | Optimal | 1744 | 0 | 79 | 316 | 36 | - |
| A | 2 | Optimal | 823 | 0 | 115 | 460 | 18 | - |
| A | 3 | Optimal | 2720 | 0 | 133 | 532 | 18 | - |
| A | 4 | Optimal | 5063 | 0 | 151 | 604 | 18 | - |
| A | 5 | Optimal | 3953 | 0 | 169 | 676 | 19 | - |
| A | 6 | Optimal | 8859 | 0 | 188 | 752 | 17 | - |
| A | 7 | Error | 15111 | 0 | 205 | 820 | - | - |
| A+ | 7 | Error | 16930 | 0 | 205 | 820 | - | - |

- STEP1 T2 stage A: rounds 9 converged false stop 'Error' pairs with rows 205 of 1945 total ms 55595

### Reading

- T1 converged in 3 rounds, 331 ms, 23 of 811 pairs with rows; the 3 s polish was Optimal in 136 ms, length 134 to 79, 0 failing pairs without rows.
- T2 did not converge: rounds 0-6 Optimal (388, 1744, 823, 2720, 5063, 3953, 8859 ms), round 7 Error at 15 s with 205 pairs free (820 binaries), and its doubled-bound retry (A+) Error at 15 s; 9 solves, 55.6 s.
- T2 failing pairs per round: 79, 36, 18, 18, 18, 19, 17; the free set grew 79, 115, 133, 151, 169, 188, 205 and the failing count stopped falling after round 2.
- T2 round 0 had 79 failing pairs (step 0 measured 51 with the default tidy; this run has tidy = false in `sa` as the brief specifies).
- No polish was run for T2 (not converged).

### STEP1 T2 tidy

| stage | round | status | ms | pairs fixed | pairs free | binaries | failing pairs | freed |
|---|---|---|---|---|---|---|---|---|
| A | 0 | Optimal | 2403 | 0 | 0 | 0 | 51 | - |
| A | 1 | Optimal | 4405 | 0 | 51 | 204 | 39 | - |
| A | 2 | Error | 15011 | 0 | 90 | 360 | - | - |
| A+ | 2 | Error | 15009 | 0 | 90 | 360 | - | - |

- STEP1 T2 tidy stage A: rounds 4 converged false stop 'Error' pairs with rows 90 of 1945 total ms 36829

### STEP1 T2 near

| stage | round | status | ms | pairs fixed | pairs free | binaries | failing pairs | freed |
|---|---|---|---|---|---|---|---|---|
| A | 0 | Optimal | 389 | 0 | 0 | 0 | 1010 | - |
| A | 1 | Error | 15026 | 0 | 1010 | 4040 | - | - |
| A+ | 1 | Error | 15200 | 0 | 1010 | 4040 | - | - |

- STEP1 T2 near stage A: rounds 3 converged false stop 'Error' pairs with rows 1010 of 1945 total ms 30616

### STEP1 T2 tidynear

| stage | round | status | ms | pairs fixed | pairs free | binaries | failing pairs | freed |
|---|---|---|---|---|---|---|---|---|
| A | 0 | Optimal | 2392 | 0 | 0 | 0 | 1124 | - |
| A | 1 | Error | 15115 | 0 | 1124 | 4496 | - | - |
| A+ | 1 | Error | 15025 | 0 | 1124 | 4496 | - | - |

- STEP1 T2 tidynear stage A: rounds 3 converged false stop 'Error' pairs with rows 1124 of 1945 total ms 32533

#### Reading (step 1b)

- tidy: not converged, 4 rounds (A 0, 1, 2, 2+; rounds 2 and 2+ Error at the 15 s cap), last pairs with rows 90 of 1945, 36829 ms, no polish. Failing pairs fell 51 then 39 on rounds 0 and 1.
- near (margin 4): not converged, 3 rounds, round 0 added 1010 pairs, round 1 (4040 binaries) Error at cap, last pairs with rows 1010, 30616 ms, no polish.
- tidynear: not converged, 3 rounds, round 0 added 1124 pairs, round 1 (4496 binaries) Error at cap, last pairs with rows 1124, 32533 ms, no polish.
- Round 0 was Optimal in all three modes; the near modes add over half of the 1945 pairs at margin 4, so no mode reached a round 2 solve with a usable size.
