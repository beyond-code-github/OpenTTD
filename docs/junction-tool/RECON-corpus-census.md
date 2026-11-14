# RECON C1: PSG hub corpus census

Untracked. Result: the classifier works, but only 28 of 529 candidates map into the tool's vocabulary and 5 are clean. The candidates are mostly not tool-shaped hubs (see "Why the yield is low"). Per-hub rows: `D/census/census.tsv`, `D = /mnt/c/Users/micro/OneDrive/Documents/OpenTTD-junction-tool/scratchpad/psg`.

## Method as built
- `census.py` reuses `find_hubs.py` (copied as `hubs_lib.py`, patched only to return the cluster's pieces). It re-runs the finder on each box with the stored window, so the cluster is the finder's (88 hubs do not reproduce the finder's bundle count and carry the reason `cluster-mismatch`).
- Pieces are (tile, track). Lane direction: a parity system over piece ends (all pieces on one side of a tile-edge point flow into it, the other side out of it; bridge and tunnel heads join their far head), then the signals vote the sign per connected component. A lane crossing the window border is in or out from that sign. Tracks with signals both ways are `bidir`.
- Movements: BFS from each in-lane along the inferred direction to out-lanes (bridges by `far_end`).
- Lines: the main line is the opposite side pair (W/E or N/S) with the most through movements; the lanes on it with a through movement are the main lanes. All other lanes form branches, grouped per side (gap at most 3 tiles). Notation reads the lanes in the picture frame (north group first as `L`), gap normalised to one `_` in the label (raw gap in `ml_raw`). The picture frame (x right, y down) is a mirror of the game screen, so hand is reported as the real one (`RH`/`LH`) and unequal groups (`LL_R` against `L_RR`) read mirrored against the tool's frame.
- Type labels as in the brief, plus: `1way` prefix (both main-line ends have one direction group, so the hub is not a two-way line), `T` = `L_R x L_R` that is AnyToAny and axis-aligned. Rotation to W-E is by quarter turns (180 degrees chosen to put a one-sided branch on S, so "N only" cannot occur: it is the same hub rotated).
- Clean = every lane has a direction, no bidir lane, no topology contradiction, no signal conflict (3+ conflicting signals, or over 5% of votes), not clipped, no station or depot in the box, cluster reproduced, classified.
- Deviations: (1) not widened from `D/maps`: clipped means a switch tile lies within 6 tiles outside the window along a border lane; widening would add bundles past the finder's cap of 5 and change what a hub is. 456 hubs are clipped this way. (2) A main line needs its direction groups within 3 tiles of each other, else it is `main-groups`: without this, two separate 2-lane lines 6 to 17 tiles apart read as `LL_RR` (found in the spot check). (3) `station-in-box` looks at the whole box (window plus 6 tiles), as briefed.

## Table A: hub types (529 candidates)
Axis/diag columns are for the main-line tracks at the border; `mixed` has both X/Y and diagonal pieces.

| type | n | clean | axis | diag | mixed | examples |
|---|---|---|---|---|---|---|
| unclassified: main line has no two direction groups (mostly one-way through lines, 170 hubs flagged `1way`) | 267 | 0 | 62 | 1 | 204 | psg173_1, psg173_3 |
| unclassified: a branch's lanes are not two groups | 105 | 0 | 37 | 4 | 64 | psg186_1, psg191_1 |
| unclassified: no through movement between opposite sides | 71 | 0 | 0 | 0 | 0 | psg204_2, psg204_3 |
| unclassified: lanes on the main-line sides outside the through line | 57 | 0 | 29 | 2 | 26 | psg173_2, psg204_1 |
| unclassified: no branch | 1 | 0 | 0 | 0 | 1 | psg172_3 |
| mesh (more than one through line) | 5 | 0 | 2 | 0 | 3 | psg112_3, psg146_3 |
| both-sides | 2 | 0 | 1 | 0 | 1 | psg130_1, psg191_2 (1way) |
| two-way main, one side | 4 | 2 | 4 | 0 | 0 | psg41_3, psg134_1, psg224_1, psg300_3 |
| 1way main, one side (not a tool type) | 17 | 3 | 4 | 0 | 13 | psg182_3, psg234_3 |

Two-way classified: `LL_RR x L_R` psg41_3 (clean); `LL_RR x L + L_RR` psg134_1 (clean); `L_R/LL_RR x LL + LL_R` psg224_1; `L_R x L + L` psg300_3; `L_R x L_R` both-sides psg130_1.

## Table B: tool types (clean, axis-aligned)
| tool type | candidates | clean axis examples |
|---|---|---|
| T | 0 | none |
| L_R x L_R | 0 | none |
| LL_RR x L_R | 1 | psg41_3 |
| L_R x LL_RR | 0 | none |
| LL_RR x LL_RR | 0 | none (psg41_2 and psg286_1 read so before the gap rule; both have main lines whose directions sit 4+ tiles apart) |
| LLL_RRR x * | 0 | none |

Other clean: psg134_1 (`LL_RR x L + L_RR`, two branches on one side, axis).

## Branch layout (252 hubs have at least one branch bundle)
- one branch: 134; several branches on one side: 78; branches on both sides: 40 (counting `both-sides` labels from the branch lists, including unclassified hubs).
- Branches on N after rotation: 40, all in the both-sides group (a one-sided branch always rotates to S).
- 3 or more lanes per direction: 82 hubs have a main line with 3+ lanes in a direction; 131 hubs have a branch with 3+ lanes.
- Hand of the 58 two-way main lines: 50 `RH`, 8 `LH`.

## Why the yield is low
- 170 hubs are one-way through lines (one direction group per end): the cluster holds one direction only, or the return direction runs as a separate, unconnected line. 71 more have no through movement at all.
- 456 are clipped: another split or merge lies within 6 tiles beyond the window along a border lane (the 40x40 window cuts the hub).
- 229 contain bidirectional or inconsistent track (the parity system contradicts itself) and 201 have signal conflicts. These are not wrong labels; a single contradicting signal is frequent on big hubs, so I only raised the flag past 3 votes or 5%.
- 108 have a depot and 87 a station in the box.
- To get a usable census, the finder, not the classifier, needs to change: pair both directions of a line into one cluster, and grow the window until no switch lies within 6 tiles of the border.

## Spot check (10 classified hubs, label against the `.txt` picture)
1. psg41_3 `LL_RR x L_R`: agree (two N-S pairs of two lanes 2 apart, diagonal L_R branch).
2. psg41_2 `LL_RR x LL_RR`: agree on the main line; branch lane count not checkable by eye (the E bundle is a long single column). Now unclassified by the gap rule.
3. psg134_1 `LL_RR x L + L_RR`: agree (4-lane N-S line, west branches of 1 and 3 lanes).
4. psg300_3 `L_R x L + L`: agree (L_R X-axis main line, two single-lane diagonal branches from the north, 15 tiles apart).
5. psg326_2 `L_R x L`: agree, with doubt (main L_R is clear; the single out-only branch lane is not).
6. psg130_1 `both-sides`: agree (W-E L_R line with N and S pairs joining it; no N-S through).
7. psg224_1 `L_R/LL_RR x LL + LL_R`: agree (ends differ, branches south), low confidence on lane counts.
8. psg274_3 `LL_RR x LL_R`: disagree: the two W lane pairs are 6 tiles apart, two separate lines. Fixed by the gap rule (now unclassified).
9. psg263_1 `LL_RR x L_RR`: disagree: the W bundles are 17 tiles apart. Fixed by the gap rule.
10. psg182_3 `1way L/LLLL x L + LLLL`: disagree: a one-way label on a diagonal mixed mesh is not a meaningful tool type; it is now prefixed `1way` and not counted as a tool type.

Score: 7 agree, 3 disagree (2 fixed by the rule above, 1 by the `1way` prefix). The remaining 18 classified hubs were not re-checked after the gap rule. The classifier is dependable only for the few shapes above; a larger census needs the finder change first.

## Files
- `D/census/census.py` (run `python3 -I census.py [id ...]`, about 8 s for all; `CDEBUG=1` prints contradictions), `D/census/hubs_lib.py` (patched copy of the finder), `D/census/census.tsv` (one row per hub: id, type, clean, axis, hand, ml, ml_raw, branches, any2any, missing, mesh, through, n_ports, n_lanes, ports, reasons, detail).
