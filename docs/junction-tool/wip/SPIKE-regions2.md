# Spike 2: clash sides from faces and from the ring (2026-11-17)

Follow-up to SPIKE-regions.md. Code: `spike-regions2.patch` (applies to a clean tree; includes the first spike's hooks plus up-front logging) and `spike-regions2-test.cpp` (hidden test `[.regions]`, env `REGIONS_HUB=T2|LL2|T3`; LL2 and T2 run with `PLANAR_ONLY=<tag>` so the variant is fixed; T3 ran with no override). Raw output was in the scratchpad `regions2/`.

## Method
- Lazy: every separation the loop accepted, cumulative. Verified by the run: the first spike's list was already cumulative (it carries through rounds and the area refinement pass), the counts are the same as spike 1 (4, 7).
- Up-front: every call of the `sep` lambda in MappedComposer::Build (fan / joiner rules), logged as segment pairs with its direction f. These are rows (segment pairs), not distinct line pairs. Self/copy (same base line) and parallel-f rows are skipped.
- Same-face: as spike 1 (a face that holds a stretch of p and a stretch of q, outer face excluded).
- Ring: for a pair with no common face, over all route paths through p and through q: if they share a line, or any topology crossing joins a line of one and a line of the other, undecided; else p is left of q iff both ends of p's path lie on the clockwise ring arc from q's entry end to q's exit end.
- Convention check (verified): the ring rule was also run on every same-face pair it can decide, against the face's side: 23/23 (T2 up-front), 21/21 (LL2), 44/44 (T3) agree. So "clockwise arc from q's entry to exit is on q's left" holds as stated, no flip needed.

## Results (rows)
| hub (variant) | kind | total | same-face agree / contradict | ring agree / contradict | undecided |
|---|---|---|---|---|---|
| T2 o0t2f1s1 (area 906.25, F=32) | lazy | 4 | 0 / 0 | 4 / 0 | 0 |
| | up-front | 51 (3 skipped) | 23 / 0 | 25 / 0 | 0 |
| LL2 o49t0f1s0 (area 1654.25, F=38) | lazy | 7 | 5 / 0 | 0 / 0 | 2 |
| | up-front | 33 (4 skipped) | 21 / 0 | 8 / 0 | 0 |
| T3 o0t1f1s1 (area 1941.25, F=65) | lazy | 2 | 0 / 0 | 2 / 0 | 0 |
| | up-front | 171 (6 skipped) | 44 / 0 | 121 / 0 | 0 |

Contradictions: none, on any hub, lazy or up-front, by faces or by ring.

## Undecided
- LL2 lazy, 2 rows: D1a[2] | E1[3] and D1a[5] | E1[3] (D1a travels heading 1, E1 heading 2, f=(0,-1), solver kept D1a on E1's right). Reason: the routes through D1a and E1 share line EO1 (both merge into EO1), so the ring rule does not apply; they share no face. The first spike's other 5 LL2 lazy rows are face-decided (agree); the ring rule cannot decide those 5 (routes share EO2, SO1, SO2, WO1 x2), which is why no ring check exists for them.
- No other undecided row on any hub (up-front: 0 on all three).

## What it says
- Verified: every decided separation is the side the planar map implies, on 3 hubs, 1 variant each, 0 contradictions: 253 decided rows (lazy 11 of 13, up-front 242 of 255 logged rows; the other 13 up-front rows are 13 skipped self/copy/parallel, the 2 lazy ones undecided).
- Verified: the 4 T2 lazy rows spike 1 called "not adjacent" (C1/C2 against F2) are decided by the ring and agree with the solver; T3's 2 lazy rows likewise.
- Not decidable by the ring: pairs whose routes merge into one line (the LL2 D1a|E1 pair); these need the node (merge) order, not the ring. Not tested here.
- Caveats: one variant per hub; sides only, not that the stack order/distances are feasible; the ring rule trusts the first-path ends as in spike 1; "decided" counts are segment rows, not line pairs.
