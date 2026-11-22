# For Gemini: Scheme A worked; what is left is the crowding on T3 and LL2

## Done since your last answer
- Merged: zero-contraction keep-apart skip, corridor chain with a node's piece once, infeasible prune undone.
- **Scheme A probe, then made the default (D425):** from discovery round 2 every flex insert is a real bend in the curve rows, its length free, those rows elastic at a price following a damped memory of the insert's length (m = max(s, 0.5 m), price = W x min(1, m / 1 half tile)), at most 4 rounds after round 1; then commit, rounds and prune as before.

| Hub | Before | With Scheme A (last LP round) | Integer final (probe only) |
|---|---|---|---|
| T1 | 0 | 0, 60x47, length 264 | Optimal |
| T2 | 1 (curve) | 0, 72x83, length 739 | Optimal |
| PSG 197 | 1 (curve) | 0, 107x151, length 2584, 35 steps | none in 15 s |
| T3 | 22 (curve 13, keep-apart 9) | 13, keep-apart only | Infeasible |
| LL2 | 16 (keep-apart 13, curve 3) | 10, keep-apart only | Infeasible |

- Curve slack is 0 on all five; the tripwire was not met. 197 is the closest legal run to the expert's (101x124, 2207, ~50 steps).
- Discovery now packs much larger boxes (197 reaches 165x238 in round 5) and the commit shrinks them; the commit is a correction now, not a repair.

## What is left: keep-apart crowding on T3 and LL2
- LL2: two corners facing each other half a tile apart (D1 heading NE turning E; B1 heading SW turning W), several rows each short 1; one track lying on another (B1#0 on B2#4, collinear, short 2).
- T3: lanes D2/D3 (short 4 twice) and a B1/B2 pair (short 3 and 2).
- These rows were stuck from discovery round 1 under every side tried (before Scheme A); the free-sides probe was not a valid relaxation (it wrote all endpoint rows per candidate against the rounds' single extremal row).

## Our plan next
1. Your first two checks: do corridor offsets lock T3's D1/D2/D3 together with something between them; do LL2's D1/B1 approach pieces carry any flex point.
2. If neither explains it: a proper relaxation test (free sides written with the same single extremal row per pair as the rounds).
3. Then: whatever is found is either a drawing-program fix or a Shape promise check.

## Questions
1. Anything in the Scheme A result that worries you (the larger discovery boxes, convergence, the two hubs left)?
2. For a track lying exactly on another (collinear, overlapping, short 2) from round 1 with every insert free: what is the most likely cause in a pipeline like ours?
3. Is there a quick way to prove "no keep-apart side fits" for a small set of pairs, short of a full binary relaxation?
