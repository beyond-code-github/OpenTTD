# RECON C1b: PSG hub census on a whole-map finder

Untracked. Supersedes the finder of C1 (`RECON-corpus-census.md`); the classifier is C1's with the fixes below. Per-hub rows: `D/census2/census2.tsv`, `D = /mnt/c/Users/micro/OneDrive/Documents/OpenTTD-junction-tool/scratchpad/psg`. Result: the new finder finds 1418 hub boxes in 177 saves, but only 43 hubs with a two-way main line classify (21 clean) and 7 are clean axis-aligned tool types. Stations (694) and mesh-large boxes (277) take most of the corpus.

## Method and deviations
- Boxes of each save are stitched. Cores: switch tiles (2+ track bits, not Upper+Lower or Left+Right) and rail bridge heads (tunnel heads are not cores), linked when within 6 tiles along track (BFS over the track graph, a bridge span counts as one step).
- Growth: the analysis box is the core bounding box plus 6 tiles. Cores merge until no core tile lies within 6 tiles of that box border, i.e. core boxes within 12 tiles of each other merge (literal reading). Over 120x120 for the analysis box: `mesh-large`, skipped. A station tile inside the box: `station-in-box`, skipped, as C1.
- Deviation: a core of one tile (one switch tile, ncore 1) is `trivial` and not classified (153 boxes; in a first run they produced 1x1 "hubs" with one lane per edge).
- Directions: the parity system of C1 (flow in or out of each tile-edge point), run once over the whole map instead of a 40x40 window; the sign per connected component is the majority of signal votes within the box plus 20 tiles. Lanes are deduplicated per (border tile, edge), because switch tiles sit on the old tight border.
- Pairing: per border side, lanes are cut into direction groups (same direction, chain gap at most 3 tiles); adjacent groups of opposite direction within 20 tiles are paired greedily by smallest gap into one line port; the gap in tiles between the two groups is recorded (0 = adjacent tracks). A straight component without a switch is added to the hub only when it has a lane within 20 tiles of an opposite-direction hub lane on the same side (34 hubs got one). A port holding a main lane is main as a whole.
- Classifier: C1's, with `1way` prefix, `T`, main line W-E rotation and branch on S. The 3-tile rule is now inside direction groups only; the gap between groups is free. Both ends of a main line must be one port each, else `main-unpaired`. Within-group chain gaps of 1 to 3 are allowed (flag `loose`, column in census2.tsv: 19 of the 43 clean hubs carry it, so a stricter adjacent-only group would drop those).
- Clean = no lane without direction, no bidir lane, no topology contradiction, no signal conflict (3+ votes or over 5%), no depot in the box, classified. Clipping cannot occur by construction.
- Not done: no tool-type check of movements beyond C1's AnyToAny, no check of bridge doubling, no recount of branch layouts.

## Table A: hub types (1418 boxes; C1: 529 candidates)
| type | n | clean | axis | diag | C1 (n / clean) |
|---|---|---|---|---|---|
| station-in-box (skipped) | 694 | 0 | | | not counted (87 in box) |
| mesh-large (skipped) | 277 | 0 | | | not counted |
| trivial (single switch tile) | 153 | 0 | | | not counted |
| unclassified: main-line end holds several ports (one-way or unpaired) | 73 | 0 | | | part of 267 one-way / 57 side lanes |
| unclassified: no branch | 49 | 0 | 36 | 3 | 1 / 0 |
| unclassified: no through movement | 48 | 0 | | | 71 / 0 |
| unclassified: no lanes | 16 | 0 | | | 0 |
| unclassified: lanes on the main sides outside the through line | 13 | 0 | 6 | 0 | 57 / 0 |
| 1way main (not a tool type) | 50 | 21 | 34 | 1 | 17 + 170 flagged / 3 |
| two-way main, one side, ends equal | 23 | 11 | 16 | 0 | 4 / 2 |
| two-way main, ends differ (for example `L_R/L x L`) | 15 | 8 | 6 | 1 | in the 4 above |
| mesh | 6 | 3 | 4 | 0 | 5 / 0 |
| both-sides | 1 | 0 | 1 | 0 | 2 / 0 |

Axis and diag columns count main-line tracks only; the rest is `mixed`. Counted per row, the remaining boxes are neither axis nor diagonal.

## Table B: tool types, clean and axis-aligned (id [x0,y0,x1,y1])
| tool type | classified | clean axis | clean axis examples |
|---|---|---|---|
| T | 0 | 0 | none (no `L_R x L_R` is AnyToAny: every candidate misses a movement, mostly the return direction of the branch) |
| L_R x L_R | 4 | 2 | psg12_23 [747,151,760,166], psg12_25 [543,167,559,180] |
| LL_RR x L_R | 5 | 4 | psg04_34 [174,334,206,378], psg35_38 [840,269,864,295], psg35_46 [113,311,142,338], psg35_106 [118,717,147,741] |
| L_R x LL_RR | 0 | 0 | none |
| LL_RR x LL_RR | 3 | 1 | psg173_14 [73,92,130,133] |
| LLL_RRR x * | 2 | 0 | none (psg09_13, psg167_18 are not clean) |

Also clean axis: psg04_36 `L_R x L_R + L` [208,349,228,362]; psg35_136 `LL_RR x L` [118,880,142,929]; psg35_149 is mixed. psg35_38, 35_46 and 35_106 have the same shape (gap 4 between the LL and RR groups), likely one design copied.

## Direction-group gaps of two-way main lines (tiles between the two groups at a border)
Hubs with two-way main ends (hand RH or LH), 79 line ends in 43 hubs, mesh and both-sides included; in brackets the clean ones (38 ends).
| gap | 0 | 1 | 2-3 | 4-10 | over 10 |
|---|---|---|---|---|---|
| ends, all | 28 | 7 | 9 | 30 | 5 |
| ends, clean | 11 | 5 | 3 | 17 | 2 |

So adjacent tracks (gap 0) and 4-10 tiles (two 2-lane groups kept apart, as in psg35 and psg173_14) are the two common forms; gap 1 and 2-3 are rare. The tool's notation allows 1 or more; gap 0 occurs in 28 of 79 ends and is outside "one or more" if `LR` is not allowed.

## Spot check (10 newly classified hubs, pictures in `D/census2/pics/<id>.txt`, C1 legend)
- agree (5): psg12_23 (`L_R x L_R`, N-S pair, branch pair from W), psg12_25 (W-E pair, branch pair S), psg35_38 (`LL_RR x L_R`, two pairs 4 apart, branch E), psg35_136 (`LL_RR x L`, 2+2 N-S over bridges with gap 1, one-lane branch W), psg04_36 (`L_R x L_R + L`, three diagonal branches, a pair and a single).
- disagree (1): psg41_35 (`L_R/L x L_R`): the W-E pair turns into the S pair; the E end has no through lanes along the line, the label reads an end that is not a line.
- not verifiable by eye (4): psg292_17, psg323_16, psg170_24, psg41_25: big diagonal weaves or the "ends differ" form (one end `L_R`, the other `L`), which is a through line with a missing direction, not a tool type.

Score: 5 agree, 1 disagree, 4 unverifiable; the stop rule (over 3 disagree) did not trigger. Treat "ends differ" (15 hubs, 8 clean) as unconfirmed. The five agreed hubs are all plain pair or 2+2 lines with at most one branch.

## Why the yield stays low
- 694 of 1418 boxes hold a station (coop junctions sit at station throats); 277 merge into more than 120x120 (dense areas); 153 are single switches.
- Of the 294 boxes that reach the classifier, about 123 are one-way funnels (73 with several unpaired ports, 50 `1way`; the return direction is further than 20 tiles away or on a separate line), 48 have no through movement, 49 no branch.
- Sensitivity: merging core boxes within 6 instead of 12 tiles (`MERGE=6`, file `census2_m6.tsv`) gives 8738 boxes, 3677 with stations, 297 mesh-large and 306 two-way classified (148 with ends differ); Table B there: `L_R x L_R` 13 (6 clean axis), `LL_RR x L_R` 7 (3), `L_R x LL_RR` 2 (1), `LL_RR x LL_RR` 5 (2), `LLL_RRR x *` 3 (0), `T` 0. Boxes there overlap neighbours' switches, so it is looser than the brief.

## Files
`D/census2/census2.py` (`python3 -I census2.py [save ...]`, 12 s for all, env `MERGE`, `MARGIN`, `PAIR`, `WITHIN`, `CAP`, `OUT`), `tables.py` (tables from a tsv), `pic.py` (`python3 -I pic.py census2.tsv <id> ...` writes `pics/<id>.txt`), `dbg.py` (lane dump for a box), `census2.tsv`, `census2_m6.tsv`.
