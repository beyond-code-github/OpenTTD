# X5 probe: crossings, order and marks from the extractor's lines on PSG 197

Files: cross.py (`python3 -I cross.py`, ~10 s; execs x1c/nodes.py with its nodes-out.txt write disabled), cross-out.txt (report), ext-cross.txt (each ext line's crossings in travel order with marks; `n` = trunk-rule node crossing, `c` = tunnel under tunnel, no suffix = ground over tunnel). Debug blocks: env DBG, DBG2, DBG3.

## Rules as implemented
1. Geometry as e1/order.py: each x1c line (109, all simple chains) is a chain of pieces; a piece is the straight segment between two of its mid-edge points (entry shared with the previous piece, exit shared with the next; the first/last piece use the neighbouring node piece, a port line falls back to the far end); a tunnel is a jump head-centre to head-centre. Each line's polyline is displaced sideways by 1e-6*(line id + 1) (order.py's displacement), so a track running along a tunnel axis and leaving it at a curve gives a crossing at the departure.
2. Crossing = proper interior intersection of two segments of different lines. Kinds: (a) ground x ground (level), (b) ground x tunnel jump, (c) jump x jump (D390 (2)), (d) bridge over line: 0 bridges in 197, not implemented. Hits within 1e-3 of an endpoint both lines share (legs of one node meeting at its edge) are dropped (32 such hits).
3. D390 (1): a node piece on a tunnel's middle tile is attached to its trunk line only (split: in line, its end; merge: out line, its start) as one extra ground segment, so a leg line never records that crossing. Cross nodes get no trunk (1 such node, 109,171 D3 over D3).
4. One crossing per line pair: the first hit along the lower-numbered line (1 pair has 2 hits: ext L79 x L97).
5. Order: arc length along the line's displaced polyline in travel (route) order. Marks: marks.py's rule, dot(dM, left(dL)) < 0 gives `>`, left(d) = (d.y, -d.x), directions are the two crossing segments' travel directions.
6. Matching to hand: x1c's candidate lists; the four hand lines with two candidates (A1d,A1e -> L58,L59; C1,C1b -> L74,L80) get the injective assignment maximising common pairs and marks (A1d=L58, A1e=L59, C1=L74, C1b=L80). A hand pair is common when its two assigned ext lines cross.

## Counts
- 50 tunnel jumps (= the 50 tunnels), 0 bridges. Level crossings (a): 0, at nodes too: 0.
- 123 extractor crossing pairs: 102 ground over tunnel, 3 node piece over tunnel (trunk rule), 18 tunnel under tunnel. Hand: 122 pairs (112 tile pairs after the trunk rule, 14 tunnel-under-tunnel in the E1 count; 54 statements).
- Six node pieces sit on tunnel middle tiles: 81,168 / 87,164 / 97,177 give an `n` crossing; 100,165 and 97,185 have the trunk pair already crossed by the trunk's own ground piece (type b), and their two leg lines have no crossing with that tunnel, which is D390 (1); 109,171 (cross node) is over L72's tunnel and its leg L98 crosses L72.
- Common 117, only in hand 5, only in extractor 6.
- Order: all 37 hand lines with >= 2 common pairs have the identical order of their common partners (0 differ; 13 lines have < 2 common pairs; 4 hand lines have no ext line: D12d, D12e, D12f, D3).
- Marks: 117 of 117 common pairs equal in both directions.

## Mismatch classes (11 pairs)
- D12 tail (strand assignment A, e1 NOTES): hand D12d, D12e, D12f have no ext line (x1c). Ext has the D1+D2 ladder lines L67 (99,182 -> 98,185), L68 (102,184 -> 98,185), L69 (100,183 -> 98,190, one tunnel, = T49). Hand-only: A1e x D12e, A1e x D12f, D12d x D12f. Ext-only: L59(A1e) x L67 at (98.5,185.0), L59 x L68 at (98.5,185.5), L68 x L69 at (100.5,185.5). So A1e's two crossings and the one crossing of D12d's tunnel T49 by a D12e/D12f strand are all present, at the points e1 gave ((100.5,185.5) for T49); only the line naming differs (the ladder pairs are separate lines in the extractor, and the hand file merges them into D12e/D12f). The three hand-only pairs are the three ext-only pairs 1:1 in position.
- D3 (109,171 cross node, D3 over D3, a D3 strand pair through one cross node): hand D3 has no ext line; ext D3 is three lines L95, L97, L98. Hand-only: C2 x D3, C2b x D3. Ext-only: L79(C2) x L95 at (109.0,172.5), L79 x L97 at (110.0,172.5), L72(C2b) x L98 at (110.0,171.5). One hand pair becomes two ext pairs for C2 (L79 crosses both D3 strands; L79 x L97 has a second hit); the hand file's simplification of the cross node into one line is the cause.
- No pair is lost for any other reason: no LOSS, no order inversion, no mark flip, no level crossing, and D12 ladder dropped D1+D2 pair crossings (hand note: dropped short parallel pairs) show only as the extra L67/L68/L69 pairs above (the ladder lines carry route set D1+D2, none of them crosses anything other than A1e and T49).

## Caveats
- Common-pair order/marks are computed with the same rule on a geometry that follows the same polyline rules as the hand run, so the agreement tests the extractor's lines and walks, not the rule.
- Order compared on common pairs only; hand lines with one common pair carry no order information.
