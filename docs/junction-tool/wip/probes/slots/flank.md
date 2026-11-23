
### STEP2C T1

- STEP2C T1 stage A (no keep-apart rows, tidy on, flank rows on): Optimal 6751 ms, flank rows written 62

Per-track breaks:

- STEP2C T1 STOP: no per-track break found

### STEP2B T1

- STEP2B T1 stage A (no keep-apart rows, tidy on, flank rows on): Optimal 6742 ms, flank rows written 62
- STEP2B T1 contracted variant: per-track turns hold, node turns (F6/F8) hold (0 failing line(s)); g_turn_moved 0 -> 0, g_turn_rejected 0 -> 0
- STEP2B T1 stage B listing solve (no rows): Optimal 6 ms

| stage | round | status | ms | pairs fixed | pairs free | binaries | failing pairs | freed |
|---|---|---|---|---|---|---|---|---|
| B | 0 | Optimal | 6 | 0 | 0 | 0 | 0 | - |
- STEP2B T1 stage B: rounds 1 converged true stop '' pairs with rows 0 of 167 total ms 6
- STEP2B T1 polish: Optimal 6 ms length before 110 after 110
- STEP2B T1 judge accept (without interface rows: judge accept); domain accept notes 0 breaks 0

### STEP2B T2

- STEP2B T2 stage A (no keep-apart rows, tidy on, flank rows on): Error 15034 ms, flank rows written 159

### Reading

- T1: 62 flank rows; stage A Optimal in 6.7 s (169 ms without the rows). The step 2c per-track break diagnostic finds no break (its "no per-track break" stop fires). The contracted variant holds per-track turns and node turns (F6/F8), 0 failing lines.
- T1 stage B: listing Optimal, 0 pairs with rows of 167, 1 round, converged; polish Optimal, length 110 to 110; judge accept; domain accept, 0 breaks.
- T2: 159 flank rows; stage A Error at 15 s (2.4 s without the rows). No drawing, so no break classes, listing, loop, polish, judge or domain verdicts.
- `[slotsstep2cdiag]` stopped at T1 and did not reach T2 (its T1 stop is a FAIL when no break exists); it was not rerun on T2 alone (stage A is Error there).
