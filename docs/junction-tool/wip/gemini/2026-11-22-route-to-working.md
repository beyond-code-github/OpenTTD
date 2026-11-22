# For Gemini: the route from "drawable" to "built" for any hub. Are we missing a blocker, and how should the side choice and the grid be done?

## Where we are (all measured today)
Layer stack: Structure (topology, copies, rows, structures) -> Shape (8-direction headings, flex points = optional S-bend pairs) -> Slots (every length, an LP/MIP in half tiles). By construction: no later layer repairs an earlier one. The engineer's priority now: get every hub working with today's compactness. Don't chase the expert's (V's) compactness, and don't loosen the model.

The drawing program (our E5 oracle) is in effect a Slots prototype. Its pipeline: a seed, LP rounds that read each keep-apart pair's side from the previous drawing (a heuristic), commit and prune optional flex, then an integer final.

| Hub | Pipeline (top 8 Structure variants) | Integer final on the tile grid |
|---|---|---|
| T1 (small T) | 8/8 clean (LP slack 0) | Optimal, 0.24 s |
| T2 | 8/8 clean | Optimal, 0.84 s |
| PSG 197 (V's real hub, 101 tracks) | LP clean | no answer in 15 s (also none off-grid in 5 s) |
| T3 | 0/8 (slack 8-16) | - |
| LL2 | 0/8 (slack 6-10) | - |

- **T3:** its crowded faces are exactly drawable (every pair a binary, coordinate bound 256: 0.3-3 s per face). The pipeline's side picks are wrong in 39/132 pairs there, all corner pairs across a face.
- **LL2:** face 33 is provably undrawable in this shape. A copy closes onto its parent inside a face where two bends face each other (your generalised clause from last time: no widening event inside a pinched face).
- **Our keep-apart rows hold on V's real drawing** wherever V can be pinned.
- **Whole-hub exact** (every pair binary, all faces) gave no incumbent in 15 s on any hub.
- **197 drew as an integer drawing before,** with V's own S-bends as fixed bends (no optional flex): 2 LP rounds plus a 6.4 s integer solve. With today's optional flex (about 28.5k pairs) the LP rounds still reach slack 0, but the integer final does not finish.

## The plan we are considering
Promote the drawing program into Slots v2 (no rewrite), each step judged on the five hubs:
1. **Side choices by construction:** a pair whose side follows from headings gets a fixed side; pairs in convex faces get none; every other pair (corner pairs where neither side is certain) gets binaries. The pipeline's drawing goes in only as a MIP start, so correctness never depends on it. Tight per-pair big-M; possibly your track-pair ordering variables and whole straight runs instead of pieces.
2. **The LL2 clause** in Structure/Shape.
3. **Whole tiles:** grid rows (each axis run of positive length has odd coordinates across it; forced parities propagated) in the one integer solve.
4. **The missing rule families:** prio coverage exact, node legs to events, clearance to the box edge.
5. **The hand-off to Heights and Signals.**
Seed budget: 5 s per variant (hover is separate).

## Questions
1. Is step 1's model sound as "by construction"? The binaries are chosen from the shape alone, before any solve, and a MIP start comes from a heuristic drawing. Any trap, e.g. a pair we call "decided by headings" that in fact is not?
2. Speed: 197 fails the integer final at 15 s with sides fixed, but solved in 6.4 s when V's S-bends were fixed bends with no optional flex. So optional flex looks like the cause. What is the right way to hand optional S-bends to the integer solve? We plan: commit the bends the LP used, remove the rest, then the integer solve with grid rows. Is there a better formulation? Our candidates: drop flex inserts the LP left at 0 before the integer solve; solve the integer final face-block by face-block with the shared tracks fixed (a decomposition, which we'd rather avoid); better branching priorities (sides before lengths); a feasibility-only objective.
3. Grid: T1 and T2 found tile-grid drawings in under a second. Is there a known reason grid integrality would blow up with size (lattice or parity constraints coupling across the whole hub), or is it usually local? Should the parity choices move upstream (Shape decides each run's parity) to keep Slots an easy MIP?
4. Is there a blocker we have not listed between "an integer tile-grid drawing" and "a built hub"? For example, the tile rasterisation of 45-degree pieces, tunnel and bridge lengths, or signal spacing feeding back into lengths.
5. Ordering: we plan side choices (T3) -> LL2 clause -> 197 speed and grid -> missing rules -> hand-off. Would you reorder, given that the missing rules could break hubs that draw today?
