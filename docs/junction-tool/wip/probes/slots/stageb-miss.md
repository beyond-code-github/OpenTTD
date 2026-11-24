# T1 stage B miss: the judge and the domain refuse the polished drawing

Source: hidden test `[stagebmiss]` (planar_domain_encoding.cpp), from `t1-stagea-rules.drawing.txt`, run twice with identical lines (times masked). Final drawing saved as `tools/junction/topologies/t1-stageb-rules.drawing.txt` (points per track); pin: `[junction][contraction][!shouldfail]`.

## Stage B run
- Lazy loop: 2 rounds, converged, 8 of 198 pairs have rows, length 150.
- Polish (`sp`: tidy on, `only_pairs = b.list`, 3 s): Optimal, 150 -> 134.
- Judge on the polished points: refuse `[keep apart]` (also without interface rows). Domain: refuse, 1 break.

## Domain break
- `[connection]` at tile (7,12): tracks CD1 (#4) and AF1 (#5), "4 track ends meet at (15, 24) with no node".
- CD1 piece 2 starts at (15,24) and AF1 piece 1 starts at (15,24): the two tracks share the point (15,24) mid-track. Both are walls of face 5 (the same face pair as the judge's four failing pairs below); they are not at a listed node.

## Judge keep-apart refusal
Four pairs of face 5, all between CD1 and AF1, all failing in every candidate (best margin per pair):

| pair | pieces CD1/AF1 | best candidate | margin |
|---|---|---|---|
| sep 5 3 7 | 2/1 | s (1,0) or (0,1), spacing 2 | -2 |
| sep 5 3 8 | 2/0 | s (1,0), spacing 1 | -1 |
| sep 5 4 7 | 1/1 | s (1,1) spacing 2 / s (-1,0) spacing 1 | -2 / -1 |
| sep 5 4 8 | 1/0 | s (1,1) or (-1,1), spacing 2 | -2 |

Rows as the judge writes them, e.g. sep 5 3 7 candidate 0 (s (1,0), spacing 2): `s.(q-p) >= 2` for p in {CD1#2 (15,24), CD1#3 (15,22)}, q in {AF1#1 (15,24), AF1#2 (15,30)}; the row p=CD1#2, q=AF1#1 has q-p = (0,0): margin -2 (the same point). The other three pairs fail on the same coincidence of CD1#2 = AF1#1 = (15,24).

## Did stage B's detector see it?
- The pairs are not skipped: the lazy loop's own read-out of the same four tags has `skipped ''`, 4 candidates, same tags as the judge's. Same pairing rule as the judge; no skip reason, no pairing difference.
- They hold on the lazy loop's last drawing (before the polish): `PlanarKeepApartHolds` true for all four. So the lazy loop was right: the drawing it converged on is clean, and these pairs were never violated, hence never listed (list = 8 pairs, none of them these).
- The polish then solved with rows only for those 8 listed pairs (`sp.only_pairs = b.list`, `only_pairs_on`): the four pairs have no rows in it. The polish moves CD1 and AF1 (length 150 -> 134) until they touch at (15,24), which nothing forbids in its program.
- Neither a detector miss nor a rule mismatch: stage B's rows were never wrong; the polish drops the pairs that held by chance.

## Verdict (one sentence)
The polish is wrong by its own contract: it is a re-solve of the converged drawing that keeps only the 8 pairs the lazy loop had to list, so it is not drawing-preserving for the other 190 pairs, and it is run after, not inside, the lazy loop that would catch the pairs it breaks (stage B's drawing is clean, the polished one is not).
