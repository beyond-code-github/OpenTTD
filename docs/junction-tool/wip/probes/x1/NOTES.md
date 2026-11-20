# X1 probe: route-set line/node rule on PSG 197 (stage X4 question)

Files: lines.py (rule + comparison, `python3 -I lines.py`, 0.5 s), lines-out.txt (full output), dirs.py (direction measurement).
Graph: `core.build()` of the trace (net.build2: signals direct, every unsignalled piece passable both ways; KEEP 16 lanes; core = forward from ins AND backward from outs; 1158 core pieces, tunnel jumps included in N.CS).

## Rule as implemented
Per core piece: route set = {(in lane i, out arm o)} with piece reachable from lane i's edge piece and reaching some out lane of arm o. Line = weakly connected component of core pieces joined by a directed edge whose two ends have equal sets. Node = cluster of "cross edges" (a->b, set(a)!=set(b)), clustered when edges share a piece; arity = distinct lines in/out. Pair (lane, arm) named as the hand routes (A1 = N91 -> arm S, etc.).

## Counts
Extractor: 52 lines, 41 nodes (7 splits 1->2, 26 merges 2->1, 8 plain 1->1 changes, no 3-way). Hand: 100 lines in the Line index (psg197.txt 101 lines), 62 nodes: 38 change the route set, 24 are same-set (choice splits A1, B1, E1c, D12 ... and one same-set join D12f+D12e -> D12h).
Extractor nodes (41) vs hand set-changing (38): signature (in route sets, out route sets) matches for 33 hand nodes; 5 hand nodes unmatched, 8 extractor nodes unmatched.

## Match (hand line -> extractor lines on its route-carrying path between its from/to tiles, node tiles excluded)
- 92 hand lines lie in exactly one extractor line with the same route set; but 22 of the 52 extractor lines each cover several hand lines (e.g. L31 route A1 = hand A1, A1b, A1c, A1d, A1e; L34 D1+D2 = eight hand lines D12..D12h). So (a) one-to-one hand/extractor: only the port lines and a few unique ones; most hand lines are NOT separately recoverable (class U).
- (b) 8 hand lines span two extractor lines (class B): A1d, A2, B1b, B2, BF1, C1, F1b, F2: the route set changes at a node whose tile differs from the hand node's tile (hand puts AB2 at 91,147; the set changes at 91,148/92,148 -> 91,149/92,149; hand AD12b/AD12c nodes at 97,192, set changes at 97,191). Offset of 1 to 3 tiles, plus ladders (below).
- (c) extractor lines with no hand line: 0. The dropped short parallel pairs do not give extra lines: both strands carry the same route set, so they sit inside one line. They appear only as ladder pieces (see M3).

## Mismatch classes (one example each)
- U, same-set splits are invisible: the rule has no node at a choice split (route set unchanged), so hand nodes `A1 -> A1b, A1c`, `D12 -> D12b, D12c`, `E1c`, 24 nodes in all, have no extractor node and their legs merge into one line. Includes the N90 three-way split (A2 -> A2m/A2d, A2m -> A2c/A2b): all legs carry only route A2, so there is no node (extractor L20 = A2, A2b, A2c, A2d). Needs a second rule for choice splits (tunnel/track choice), not derivable from route sets.
- M1, node position: the set change happens at the real divergence, not at the hand's node tile (AB2 above). Hand nodes `B1b, F1b -> BF1`, `BF1, F2 -> BF12`, `C1, C12c -> C12d`, `AD12b, A1d -> AD12c` fail the signature test for this reason: the extractor sees the merge sequence as 2 nodes with different input sets (e.g. [B1]->[B1+F1], [B1+F1]->[B1+F1+F2]) because my cluster rule splits a 3-input cascade differently from the hand's pairing.
- M2, 1->2 split reported as two 1->1: AB2 (A2+B2 -> A2, B2) appears as `L25->L20` and `L25->L28`, two cross edges with no shared piece: the N90 lane runs as two parallel strands before the split. Cluster by line pair, not shared piece, would give 1->2.
- M3, ladders: where parallel strands both carry the same set (the hand's dropped 1-3 tile pairs), the line is a ladder: 8 lines have no start or end piece with no same-line neighbour ("(loop)" in lines-out.txt): the free (two-way) unsignalled pieces form 2-cycles, so from/to tile cannot be taken as a source/sink. Not a route-set error but the line has no unique from/to.
- M4, arity: extractor has no node wider than 2; the hand file has none either (the 3-way split is written as two 1->2), see U.

## Direction measurement (dirs.py, LOCAL=1 vs build2)
The trace's "direction fixes" in this graph are only KEEP (16 lanes) + signals; build2 does not use the save's local direction inference at all. LOCAL=1: all 16 KEEP lanes agree with the raw direction (no lane differs); 5 raw lanes outside KEEP (E190, S116, S128 with directions; E186, S115 undirected) are the halo cut. 1229 box pieces: 448 signal-directed (0 undefined, 0 contradict LOCAL), 781 free (both ways); of these 736 get a LOCAL direction (would restrict them), 45 undefined. So the hand fixes recorded for N91/E166/E167 (ANATOMY) are not reproducible here: no piece-level direction difference against signals; all the openness is in the 781 free pieces. I did not rerun the line rule with the LOCAL directions applied to the free pieces.

## Not done
- No run with LOCAL-restricted free pieces (rule on a one-way graph); the 2-cycle problem (M3) and the node-cluster rule were not iterated (no rerun loop).
- Hand-node tiles were not matched to extractor node tiles individually (only by route-set signature and by the 8 span cases).
- "Hand node tile" is the Line index's from/to tile, which is a piece on either side of the change.
