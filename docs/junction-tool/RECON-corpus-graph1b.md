# RECON G1b: PSG hubs, repaired segmentation

Untracked. `D = /mnt/c/Users/micro/OneDrive/Documents/OpenTTD-junction-tool/scratchpad/psg`; code and data in `D/graph1b/` (`g1b.py` builds with env `RULE=A|B|C T=<n>`; `an.py` counts; `spot2.py` + `show.py` make and print the pictures; `tables1.py` as G1). Everything else (graph, direction, lanes, classifier) is G1's, unchanged.

## Variants
- **A**: G1 rule at T=10 (cut when the longest crossing-free run >= T, or at a terminal).
- **B**: a bridge or tunnel over or under the link no longer resets the run; only an at-grade crossing (X+Y, rail/road level crossing) does. T=10 and T=16.
- **C**: B at T=10, plus link cut: take the long (>= 6 tiles, nodeless by construction) uncut edges; group them into bundles between the same two sub-components; cut a bundle if removing it splits the hub and both sides keep 2+ nodes (applied one bundle at a time on the current graph).

## Counts (178 saves; hubs = components with 2+ nodes)
| variant | hubs | single-node | median nodes | p90 | 100+ nodes | side >120 | side >200 | max nodes |
|---|---|---|---|---|---|---|---|---|
| A10 | 46273 | 30144 | 3 | 16 | 171 | 60 | 5 | 900 |
| B10 | 61957 | 45573 | 3 | 11 | 26 | 30 | 4 | 893 |
| B16 | 39297 | 19003 | 4 | 21 | 248 | 111 | 10 | 903 |
| C10 | 88133 | 45573 | 3 | 7 | 15 | 7 | 0 | 891 |

Nodes per hub (<=2 / 3 / 4-5 / 6-10 / 11-20 / 21-50 / 51-100 / 101-300 / >300): A10 14704/8698/7917/7475/4020/2696/595/160/8; B10 21398/12446/11258/10327/4557/1736/210/22/3; B16 10922/6842/6843/6764/3966/2930/786/232/12; C10 38217/22002/14900/9750/2504/661/85/13/1.
(A10 reproduces G1's T=10 count, 46273.)

## Spot check (20 hubs per variant, one sample: `anchors.json`)
Sample: the 6 G1 ids by their centre node, plus 14 random non-terminal hubs of B10 with 8+ nodes and side <= 70 (the cap keeps the pictures readable; giants are judged by size, as wrong). Each variant is judged on the hub holding the anchor node (2 to 3 anchors that are single-node in a variant fall back to the nearest hub, which is judged instead). Wrong = two junctions joined only by a plain link or crossing corridor (under-cut), or one junction split in two (over-cut). Judged by eye from ASCII, so approximate.
| variant | wrong of 20 | under-cut | over-cut |
|---|---|---|---|
| A10 | 15 | 15 | 0 |
| B10 | 9 | 9 | 0 |
| B16 | 15 | 15 | 0 |
| C10 | 0 | 0 | 0 |
Per-id verdicts: `D/graph1b/spot_scores.md`. All 6 G1 failures are fixed by C10 (psg261_h20, psg290_h38 and psg331_h26 become 2 to 8 node hubs; psg4_h89, psg307_h16, psg160_h56 split at their 10 to 15 tile links). B alone fixes 3 of 6. The remaining B10 errors are exactly bundles of nodeless double track of 6 to 9 tiles, or a corridor with an at-grade crossing.
- Over-cut check beyond the 20: 4 random C10 splits at a 6-tile link, shown inside their B10 hub, were all two separate junction groups (tunnel link between ladders; 6-tile gap between a ladder and a cluster). No over-cut seen. New cuts lengths: 6, 7, 8, 9 tiles in similar numbers (C10 cut edges 6:7.7k, 7:9.2k, 8:8.1k, 9:7.1k), so the 6-tile floor is the knob to move if Pete wants fewer, larger hubs.
- C10 has many tiny hubs (38217 with 2 nodes: crossovers); most are single crossovers or ladder rungs.

## Chosen: C10
Fewest wrong (0 of 20, bar 3). Data: `D/graph1b/hubs.jsonl` (88133 hubs, G1's format and fields; = `hubs_C10.jsonl`), `stats.json`. The other variants: `hubs_{A10,B10,B16}.jsonl`, `stats_*.json`.

## Type table, C10 (G1 §3 grouping; `tables1b.txt`)
| type | n | clean | axis | diag |
|---|---|---|---|---|
| terminal ports (station or depot throat) | 35536 | 0 | 19161 | 742 |
| 1way main (not a tool type) | 20868 | 14812 | 14795 | 230 |
| unclassified: no through movement | 19998 | 0 | 0 | 0 |
| unclassified: no branch | 5165 | 0 | 3594 | 214 |
| mesh | 2676 | 2360 | 916 | 831 |
| unclassified: lanes on main sides outside the through line | 1429 | 0 | 815 | 72 |
| two-way main, ends differ | 631 | 203 | 215 | 42 |
| unclassified: no lanes | 626 | 0 | 0 | 0 |
| unclassified: main-line end holds several ports | 592 | 0 | 0 | 0 |
| both-sides | 289 | 36 | 88 | 31 |
| unclassified: other | 241 | 0 | 123 | 15 |
| two-way main, one side, ends equal | 82 | 17 | 49 | 5 |
Clean total 17428 (11989 axis).

Supported types (clean, non-terminal; axis = clean and axis-aligned):
| type | all | clean | clean axis |
|---|---|---|---|
| `T` | 0 | 0 | 0 |
| `L_R x L_R` | 20 | 1 (mixed axis) | 0 |
| `LL_RR x L_R` | 0 | 0 | 0 |
| `L_R x LL_RR` | 0 | 0 | 0 |
| `LL_RR x LL_RR` | 0 | 0 | 0 |
No clean axis hub of any of the five types exists at C10 (list of ids is empty: `clean_axis_ids.json`). The 14 axis-aligned `L_R x L_R` are all dirty: 'topo-contradiction' with terminal ports (a station or depot beside the junction). Other variants: A10 2 clean axis `L_R x L_R`, B10 1, B16 2 (plus 2 dirty `LL_RR x L_R`); G1's T=16 had 3. So the clean two-way tool types stay rare under any segmentation; the large clean populations are 1way (14812), mesh (2360), two-way main with differing ends (203, e.g. `L_R/L x L`, 91 mixed axis).
