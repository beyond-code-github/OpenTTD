# X6/X7 probe: structure and shape from the extractor's lines and crossings, PSG 197

Files: x6.py (`python3 -I x6.py`, ~15 s; execs x5/cross.py, which execs x1c/nodes.py), x6-out.txt (report), cross-out.txt and ext-cross.txt (x5's outputs, rewritten here by the exec). Matching to the hand files is x1c/x5's (`assign`: hand line -> ext line; A1d=L58, A1e=L59, C1=L74, C1b=L80; D12b and D12g share L92).

## X6 rules as implemented
1. Tunnel = a jump segment of an ext line (head tile to head tile, travel direction); its separator is that line. Crossers = the lines of the x5 crossing pairs (one hit per pair) whose hit lies on that jump segment (ground over tunnel, node-over-tunnel by the trunk rule, tunnel under tunnel from both sides), ordered by arc length along the separator in travel order. Hand tunnel Tn is matched by its entrance tile; hand names go to ext lines through `assign`.
2. Groups = tunnels with a crosser, same heading and length, entrance tiles adjacent across the axis (side by side), closed transitively. The "lines of one channel" clause was not needed (not implemented): the geometric rule alone reproduces the hand groups.
3. Straight leg per split/merge node piece: trunk = the in line (split) or out line (merge); trunk heading = the heading of its last (first) segment off node pieces, or the port heading if it has none; leg = the leg whose first (last) off-node segment keeps that heading; no leg keeps it -> `none` (fork). Node pieces whose trunk/leg is another node piece are undetermined; cross nodes skipped.

## X6 results
- Ext: 50 tunnels, all with a crosser, 141 memberships (hand 50 statements incl. T49, 136 memberships), 0 ties in crosser order.
- Tunnels: 42 of 50 identical (separator, crosser set, order). Separator equal 49; crosser set equal 42; order differs 0.
- Groups: ext 18, hand 18, identical 18.
- Nodes: ext 68 node pieces: 65 straight/none decided, 2 undetermined (104,179 and 105,179, node pieces adjacent to node pieces: the D12 ladder), 1 cross (109,171, skipped). Hand 61 nodes (60 two-leg + the 3-way A2): 55 matched to an ext node, 55 of 55 with the same straight leg (forks included, A2 3-way included); 6 hand nodes have no ext node.

## X6 mismatch classes
- Tunnel under tunnel counted twice (4 tunnels, 7 listed with D3): ext reads F2b x A1d and F2 x A1d (T18, T21) and F3 x A2b and F3b x A2b (T29, T31) as jump x jump (x5 marks `c`); the hand has them as ground over A1d's and A2b's tunnel only (the pair sits in A1d's/A2b's statement, not also in F2b's). Ext-only crossers: T18 A1d, T21 A1d, T29 A2b, T31 A2b.
- D3 (cross node 109,171): T30 hand D3 -> ext L98; T32 hand D3 -> ext L97 + L95 (one extra membership).
- T49: hand separator D12d has no ext line; ext L69 (D1+D2 ladder, one tunnel, crosser L68).
- 6 hand nodes with no ext node: CD3 split and D23 merge (hand line D3), D12b, D12g, D12h splits/merge and AD12b merge (D12 tail: D12d/e/f lines absent). The 7 ladder ext nodes (100..103,181..183) and 97,185, 98,191, 107,173, 118,170 have no hand node.

## X7 rules as implemented
- Each ext line's segment list = x5's `segs_of` plus the adjacent single node piece's segment at each end (the hand walk runs node tile to node tile). Headings in eighths clockwise from north per segment, runs of equal heading = pieces, lifted. Crossing piece = the run holding the crossing's segment (crossings in arc order). Port out-lines get the port heading appended if it differs by one eighth (none needed more). Split into projection, `at` pieces (projection indices) and steps exactly as split_shape.py.
- Ext: 109 lines, 351 raw pieces, 246 crossings, 51 steps. Hand: 100 lines, 244 crossing entries, 49 steps.

## X7 results (through the matching)
- 95 of 100 hand lines have an ext line; 94 identical projections, 90 identical steps, 89 both.
- Crossing pieces: 234 of 234 common pairs equal.

## X7 mismatch classes
- Hand line with no ext line (5): CD2, D3, D12d, D12e, D12f (as X6).
- Projection differs (1): D12g, hand [6], ext [5] (D12g is a hand chord sharing ext L92 with D12b).
- Steps differ (5): C2 (hand step in stretch 1, ext stretch 2: ext has one more crossing, the D3 split); C23 (ext has an extra step 2: -1 0 -1, hand none); EF1 (hand 0 1 0, ext 0 1 0 -1 0 -1 0), EF2 (hand 0 1 0, ext 0 1 0 -1 0) and F3c (hand 0, ext 0 -1 0): ext has more headings at the node end. Cause not isolated in this probe (candidate: the ext line's last piece or the adjacent node piece lies beyond where the hand walk stops, or the ports' chains are longer); not examined further.
