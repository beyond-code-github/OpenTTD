# RECON G1c: one-way main hubs, loop or split?

Untracked. `D = /mnt/c/Users/micro/OneDrive/Documents/OpenTTD-junction-tool/scratchpad/psg`; code and data in `D/graph1c/`. Input: C10 (G1b), terminal-port hubs excluded, type starting "1way".

## Method
- `g1c.py` rebuilds each save with g1b's own code (RULE=C T=10: graph, walks, signals, bridges/tunnels by far end, linkcut), so hub ids are g1b's own. Needs `PYTHONHASHSEED=0` and no `-I`; hash order shifts linkcut bundles, so a rerun gives 20781 hubs vs 20868 in `graph1b/hubs.jsonl` (0.4%). Hub ids in `graph1c` are from the seeded run and may differ from `graph1b/hubs.jsonl` ids.
- Main lanes: the through pair with most moves (as census2), plus whole ports. Walk starts at each main out-lane's hub node, node to node along g1b's walks. Edges whose signal/inferred direction runs against the walk are blocked. At a node it goes straight on if it can, else the one allowed exit.
- Loop: the walk arrives at a node-edge that is one of the same hub's main in-lanes, within 3000 tiles and 50 hubs passed. Other ends: terminal-station/-open/-depot/-wp, dead-end node, against-signal, cycle-elsewhere (enters a cycle that misses the hub), reenter-other-lane, bound.
- Split: outside the hub box, within 20 tiles (Chebyshev), a run of 10+ consecutive single-piece straight tiles on the same axis whose signal direction is the opposite travel direction. Distance is box to nearest such tile. "Paired hub" = another 1way hub, same axis, opposite travel, boxes within 20. Limitation: runs made of node tiles (ladders) do not count, so the distance can overstate when the opposite line sits inside a neighbouring ladder (seen in psg134_h177: 20 reported, ladders 5 tiles away).
- Walks were run for all sizes (`oneway_all.jsonl`); the question's set is 8+ nodes (`oneway.jsonl`, `sum.txt`).

## Size (1way main; term_ports 0; graph1b/hubs.jsonl; all / clean / axis / clean axis)
| nodes | all | clean | axis | clean axis |
|---|---|---|---|---|
| 2 | 10797 | 9302 | 8466 | 7425 |
| 3-7 | 9367 | 5384 | 5955 | 3654 |
| 8-15 | 611 | 111 | 310 | 64 |
| 16+ | 93 | 15 | 64 | 12 |
Total 20868 / 14812 clean. At 8+: 704 hubs, 370 axis-aligned (my rerun: 701, 370; 73 clean axis). So 99% of 1way hubs are 2 to 7 nodes (ladder rungs, crossovers); only 704 are the 8+ the question asks about.

## Classes, 8+ nodes (701)
| class | n | clean | axis |
|---|---|---|---|
| loop | 4 | 0 | 2 |
| both (loop and split) | 6 | 0 | 1 |
| split | 507 | 75 | 262 |
| neither | 184 | 49 | 104 |
- Loops are 10 hubs in 7 saves (134, 136x2, 13, 145x3 on one ring, 182, 184, 302-by-walk). Loop length 500 / 1004 / 1296 / 1325 / 1630 tiles (min/q1/median/q3/max, 10 hubs), 10 / 10 / 14 / 14 / 21 hubs passed. They are single big rectangular rings (see spot check), not local balloon loops.
- Split (513 with a split, loop or not): distance to opposite line min/q1/med/q3/max 1 / 3 / 7 / 10 / 20 tiles. 148 have the opposite run inside another hub's box; 312 (61%) have a paired 1way hub (opposite travel, same axis) within 20, pair distance 0 / 4 / 8 / 12 / 20.
- Neither (184) by walk end (hubs can have several walks): terminal-station 86 (+ mixes), cycle-elsewhere 20, terminal-open 16+13, dead-end node 11, terminal-depot 7, against-signal 5, wp 2. About half of the 8+ hubs have a main out-lane that goes to a station.
- Most 1way main lines of every size end at a station (about 13.3k of the small hubs' walk ends, 374 split-hub walks at 8+): feeders, not circulation. Small hubs (2-7 nodes, `oneway_all.jsonl`): split 12297, neither 7646, loop 52, both 85, so loops are as rare there.

## Per save (`persave.tsv`, 149 saves with a 1way hub of 8+ nodes)
135 saves have a split hub, only 7 have a loop hub, and those 7 also have split hubs. Loop share of (loop+split) is 0 in all but 7 saves (0.14, 0.33 and so on in 134/136; psg145 has 3 loop hubs on one ring beside 12 split ones). No save is "built as one-way loops" in this data: loops are at most a single ring line per save. Split dominates everywhere (median save: 8 split hubs, 0 loop).
Caveat: 8+ nodes only means large hubs; a loop passing small ladders would show up for them too, and 137 small hubs loop, in the same few saves.

## Spot check (10 hubs, by eye, `pic_*.txt`)
Loops (picture: hub neighbourhood with one-way arrows and the walk path as `*`, plus a coarse map of the whole path, 1 char = 2 to 6 tiles): psg134_h170, psg136_h34, psg13_h2, psg145_h52, psg184_h110. All five show a closed rectangular ring of 340 to 480 tiles side, with the hub (H) on it and the ring coming back into the hub from the far side: 5 agree. psg184_h110 also has side branches that end elsewhere (a loop plus stubs, as the walk list says).
Splits: psg153_h30 (opposite `v` column 9 tiles east: agree), psg326_h31 (opposite `<` lines 1 to 5 tiles away, paired hub 4 tiles: agree), psg134_h177 (class agree, opposite lines are westbound ladders 5 tiles away; reported 20 is the plain-run artefact: agree with distance caveat), psg162_h78 (eastbound lines 10 tiles north, paired hub 16: agree), psg222_h62 (opposite `>` lines directly above, 1 tile, paired hub 1: agree).
Score: 10 of 10 agree on class, 1 distance overstated. Walk judged reliable. Not checked by eye: the many cycle-elsewhere and dead-end results, and loop continuity over its full length (only the closed ring shape, from the coarse map).

## Data
`D/graph1c/`: `g1c.py` (run: `PYTHONHASHSEED=0 NP=14 python3 g1c.py`; `MINN=2 OUT=oneway_all.jsonl` for all sizes; `CAP=1` writes pictures for ids in `spot_ids.json`), `oneway.jsonl` (8+ detail rows: id, class fields loop/loop_tiles/loop_hubs, split/split_dist/split_hub/pair_hub/pair_dist, walk list, why), `oneway_all.jsonl`, `sum.py`/`sum.txt`, `persave.tsv`, `pic_*.txt`, `spot_ids.json`. Runtime about 1 minute for all 178 saves.
