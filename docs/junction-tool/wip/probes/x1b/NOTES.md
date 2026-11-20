# X1b probe: switch-node rule with local directions, PSG 197

Files: nodes.py (`python3 -I nodes.py [x,y ...]`, <2 s; extra args dump pieces at tiles), nodes-out.txt (nodes, lines, hand match), x1core.py/x1core.json (the x1 core, for the diff). Reuses x1/lines.py (bfs, analyse, route names) and the trace's net.py with LOCAL=1.

## Rule as implemented
1. Directions: signal-directed pieces (sig1) as x1; each free piece takes the save's local direction (N.ed) when defined (736), else stays passable both ways (45, listed below). S/P graph rebuilt; core = forward from the 8 in lanes AND backward from the out lanes (KEEP lanes as x1); route set per piece as x1 = {(in lane, out arm)}.
2. Node = a core piece with >=2 successors (split) or >=2 predecessors (merge) in the directed core graph (tunnel/bridge jumps are edges). Line = connected component of the non-node core pieces (maximal chain between nodes, lane edges, dead ends).
3. Split: fray if its successor pieces' route sets differ, else lane split. Merge: join if predecessor sets differ, else lane merge. All 89 lines have one uniform route set (checked).

## Counts
- Core shrinks 1158 -> 1079 pieces (79 lost, 62 tiles, none gained). All 16 hand routes (A1..F3) are still carried. 0 two-cycles, 0 undirected pieces in the core.
- 89 lines (hand 100); 54 node pieces (hand 62 nodes): splits 25 = 7 fray 1->2, 17 lane 1->2, 1 lane 1->3 (92,158Y, the N90 three-way, one tile); 1 fray 0->2 (128,173X: the E173 in-lane piece itself splits D2/C2); merges 27 = 26 join 2->1, 1 lane 2->1; 1 cross 2->2 lane/lane (109,171Lo, D3 over D3). Adjacent node pieces: none (54 clusters of 1).
- Hand: 62 nodes = 8 fray splits, 23 lane splits, 30 joins, 1 lane merge.

## Match vs hand (tile within 1 tile + kind; lines by route set + both end tiles within 1 tile of a boundary)
- Lines: 74 hand lines one-to-one; 4 hand lines have two candidate ext lines (A1d, A1e both -> ext L57/L58; C1, C1b -> L65/L68: two parallel strands of one route set); 22 hand lines have no ext line; 11 ext lines match no hand line.
- Nodes: 53 hand nodes matched by tile+kind+fray/lane (53 of 54 ext pieces used); 2 more match tile+kind but not fray/lane; 7 hand nodes with no ext node; 1 ext node with no hand node.
- N90 three-way: one ext piece 92,158Y (1->3 lane split, all A2); hand A2 and A2m both match it.

## Mismatch classes
- LOSS (22 hand lines, 7 hand nodes): the local direction removes 79 pieces that hand lines use. No-match hand lines: A1c AD12 AD12b AD12c B1 B1b B1c BF1 BF12 CD2 D12 D12b..D12h D3 EO3 SO1 SO2. Unmatched hand nodes: B1 (98,155), BF1 join (100,165), D12 (106,178), D12b (105,179), D12g (104,179), D12f+D12e merge (97,185), AD12 join (96,187); all on lost strands (e.g. 105,178Lo, 104,179, 97,185 are not in the core; 106,178Up has only one core successor). Lost columns: x=98..100 y=156..164, x=96..105 y=178..190, 103,170. The hand D12 ladder (lane splits D12 -> D12b/c -> D12f/g -> D12d/e -> D12h) and the B1/BF1 strand run against the inferred direction in the save, or the inference picks the wrong way there. Needs a look before LOCAL is trusted on those strands.
- FL (2): 97,192Up is a lane merge in the extractor (D12d+A1e and AD12b+A1d merge strands of equal sets in the surviving core) where the hand has joins.
- SHARED (4 hand nodes pairs, 10 ext pieces matched by two hand nodes): hand nodes that sit 1 tile apart (A1/A1b at 95,154+96,155; C1b,C2/C1,C12c; E1b,C23b/C12,CE123b; B2c,F12c/BF12b,F3; D12d,A1e/AD12b,A1d) land on the same ext piece or its neighbour; the 3-way is the same effect.
- EXT-ONLY (1): 109,171Lo, a 2->2 lane/lane crossing of two D3 strands (the hand has none; a dropped short pair or a crossing in the D3 tunnel).
- Dropped short parallel pairs: they show as lane splits/merges in the extractor on surviving strands only (17 lane splits vs 23 hand); no ext lane node lacks a hand node except the cross above, so the pairs are not separately listed.
- Undefined (45 pieces by tile): 100,137 101,137 101,138 102,138 103,138 103,139 104,139 104,140 105,137 105,140 105,141 106,137 106,141 107,137 107,138 107,139 108,137 108,138 108,139 108,140 108,141 108,174 108,175 109,138 109,141 110,138 115,196 115,197 115,198 116,137 116,138 118,196 119,137 119,138 128,185 128,186 98,159 99,159 99,160 100,160 100,161 (the last five were in the x1 core). None is in the core now. Five (98,159 99,159 99,160 100,160 100,161) were in the x1 core: they sit on the lost x=98..100 strand (B1b/BF1 region).
