# X1c probe: switch-node rule with route-oriented directions, PSG 197

Files: nodes.py (`python3 -I nodes.py`, ~1 s, no args needed), nodes-out.txt (nodes, lines, hand match; the EXTRAS block is printed to stdout only). Copy of x1b/nodes.py with the direction block replaced; imports the same net.py and x1/lines.py.

## Rule as implemented
1. Signal-directed pieces (sig1) keep their direction. Every other piece starts passable both ways (LOCAL not used).
2. Rounds: core lanes as x1b (KEEP). Forward reach from the 8 in lanes, backward from the 8 out lanes, over the current graph (edge a->b needs a's exit edge and b's entry-opposite edge both in the current exit sets). For each undecided free piece: not in fwd and bwd reach -> off-route (no exits). Else try each direction alone (rest of graph as current); valid = piece is forward-reachable from an in lane and backward-reachable from an out lane. Exactly one valid -> fixed; none -> off-route. Repeat until no change (3 rounds: 771, 10, 0 changes).
3. Result: 708 free pieces fixed, 73 off-route, 0 ambiguous (both directions valid at the end). Then x1b's rule unchanged: node = piece with >=2 successors or predecessors; lines = components of non-node pieces; fray/lane and join/lane by route-set comparison of legs.

## Counts
- Core 1152 pieces (x1: 1158, x1b: 1079). All 16 routes carried. 0 two-cycles, 0 ambiguous pieces (so no ambiguous tiles in the core or elsewhere), 73 off-route pieces.
- 109 lines (hand 100, x1b 89), all with a uniform route set. 68 node pieces (hand 62, x1b 54): splits 25 lane (24 1->2, 1 1->3 at 92,158Y) + 8 fray (7 1->2, 1 0->2 at 128,173X); merges 30 join + 4 lane; 1 cross lane/lane 2->2 (109,171Lo, D3 over D3). 67 node clusters (one adjacent pair).
- Hand: 62 nodes = 8 fray, 23 lane splits, 30 joins, 1 lane merge.

## Match vs hand
- Lines: 89 one-to-one; 2 more with a unique candidate shared by two hand lines (D12b/D12g -> L92); 4 with two candidates (A1d,A1e -> L58/L59; C1,C1b -> L74/L80, parallel strands of one route set); 5 hand lines with no ext line (CD2, D3, D12d, D12e, D12f); 15 ext lines match no hand line (L67 68 69 73 76 77 81 82 84 85 86 90 95 97 98).
- Nodes (tile <=1 + kind + fray/lane): 62 of 62 matched, 0 kind-only, 0 unmatched. 61 of 68 ext node pieces used. 7 ext node pieces match no hand node: 100,182Up split, 101,181Up merge, 101,183Up split, 102,182Up merge, 102,183Lo split, 103,182Lo merge (a D1+D2 lane-pair ladder x=100..103 y=181..183, all lane/lane) and 109,171Lo (cross, D3).
- The 7 x1b-unmatched nodes, all matched now: B1 98,155Ri; BF1 100,165Le (join); D12 106,178Up / D12b 105,179Up / D12g 104,179Lo (D12 ladder: the three hand nodes land on three ext pieces, ext pieces shared by neighbouring hand nodes within 1 tile); D12f+D12e 97,185Lo; AD12 96,187Up.
- Caveat: tile matching is loose (<=1 tile); in the D12 ladder hand nodes D12/D12b/D12g share candidate pieces, and hand lines D12d, D12e, D12f have no ext line, so the ladder wiring differs from the hand file even though each hand node has a nearby node of its kind. Hand D12 ladder = D12 -> D12b,D12c; D12b -> D12f,D12g; D12g -> D12d,D12e; the extractor shows a different set of lane pairs around x=100..103 y=181..183.
- x1b-lost tiles back in the core: 98,159 99,160 100,161 103,170 (x1b lost six tiles vs x1: 98,159 99,159 99,160 100,160 100,161 103,170; the core is still 6 pieces short of x1 (1152 vs 1158)).)

## Route rule vs LOCAL
- Of 708 free pieces fixed by routes, 510 agree with the save's LOCAL direction, 198 disagree (all in the core), 0 had LOCAL undefined. Disagreeing tiles are spread over the whole core (x=79..127, e.g. 100,158..100,168, 102,160..102,169, 108,166..109,169, 112,167, 116,167, 126,166). Per-tile list: run nodes.py (EXTRAS block). The hand file matches all 62 nodes only with the route rule, so wherever the two differ at the 7 nodes the hand follows the route rule; LOCAL follows the save's inferred direction, which conflicts with the hand strands at x=98..100 y=156..164 and x=96..105 y=178..190.
- Mismatch classes now: LOSS none; SHARED (hand nodes <=1 tile apart land on the same ext piece, 14 pieces shared); EXT-ONLY (7 pieces above); hand-line no-match 5 (D12d/e/f, D3, CD2: CD2 starts at port E173, whose piece 128,173X is itself the 0->2 fray, so its line has no body).
