# probe-llrr3c: the 3-way LL_RR under PathFinder with D532 (1) (HEAD c09105a083, 2026-11-28)

Hidden tests `[llrr3build]` (one per variant, `MeasureLlrr3`), Release, `-j 4`, PathFinder, present 0.5 x 1.5, `budget_seconds` 240. Candidate counts came from a temporary print in `PlaceSoft` (removed); outputs in scratchpad `llrr3/v0..v3.out`, `v0b.err`.

## Table (overused count per iteration, iteration 0 first; identical in all four variants)
| variant | outcome | per iteration | s per iteration |
|---|---|---|---|
| 0 | search budget (chain ELFM1 EL1 L1 LjE EO2, iteration 26) | 519 23 19 116 19 87 19 194 19 399 21 21 21 21 21 21 21 21 21 21 21 21 21 21 21 21 21 | about 9 (240 s / 26; `pf_seconds_per_iteration` is not set on a budget stop) |
| 1 | search budget (chain E1) | same | about 9 |
| 2 | search budget (chain AGBH1 BH1 H1 HjW WO2) | same, 28 entries | about 9 |
| 3 | search budget (chain AGBH1 BH1 H1 HjW WO2) | same | about 9 |

Not built in any variant: no judge, box, travel, measure or JSON (`built.json` not written).

## Unplaced in the last completed iteration (every variant)
| chain | last why | start candidates | end candidates before / after KeepBays |
|---|---|---|---|
| AGBH1 BH1 H1 HjW WO2 | no admissible attempt (0 conflicts); `domain: connection tracks ELFM1,L2 tile 115,30` (variant 3: tile 108,30) | 1 | port end (none) |
| CJDK1 CJ1 C1 CjW | no admissible attempt (0 conflicts); the same line | 1 | 406 / 380 (122 / 96 in some iterations) |

## What blocks
The count settles at 21 from iteration 10 on and never moves: the same two chains stay unplaced with no conflict of their own and every attempt refused by the one judge line `domain: connection tracks ELFM1,L2 tile T,30`, a contact between two other chains (ELFM1 EL1 L1 LjE EO2 and L2, which sit along row 30 and drift one tile east per iteration: 99, 108, 115). That is the standing clash D532 (2) addressed (the judge refuses every attempt for a fault the state already holds); D532 (2) is parked, so it is still the block. D532 (1) moved the former block (AGBH1 refused on the anchor's ray, llrr3b) to this one: AGBH1 now has an attempt that routes, and the judge refuses it for the foreign contact. Each iteration costs about 9 s, so 240 s buys about 26 iterations; the build never reaches a state where ELFM1 and L2 stop touching.
