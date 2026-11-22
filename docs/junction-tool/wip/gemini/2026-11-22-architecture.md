# For Gemini: three more measurements are in. Where should the separation choices live?

## Recap
OpenTTD junction tool. Layer stack: Structure (topology), then Shape (headings, 8 directions, 45-degree bends), then Slots (lengths, an LP/MIP in half tiles). The rule is "by construction": no later layer repairs an earlier one. Keep-apart between two walls of a face is pairwise: each pair has up to 4 candidate sides (along either wall's normal, either way round). Today, sides are read from the previous LP round's drawing (we call this D418); that is a heuristic. Your last reply: no octilinear turn-regularity theorem exists, and side choices depend on lengths.

## New results (all measured, logs kept)
1. **T3: the pipeline's side picks are wrong, not the room.** In the crowded faces 75/29/66, an exact drawing (every pair binary, coordinate bound 256) exists. It contradicts 39 of the pipeline's 132 side picks, all of them pairs across a face (not neighbours), mostly "outward" candidates.
2. **"Inward candidates only" is refuted by the expert's real drawing.** On V's PSG 197 (a real, legal, expert-built hub) 247 of 1362 pairs are separated only by a non-inward candidate. Restricting to inward sides raises the slack on V's own geometry from 57.7 to 5247.5. Facing pairs (each wall in the other's inner half-plane by the face walk) still take their side from headings alone, and that holds on V. But they are few.
3. **Exact whole-hub solves don't finish.** Every pair binary over every face, with a tight bound (twice the pipeline drawing's extent, big-M = 4x that). No incumbent in 15 s on any hub: T1 3044 binaries, T2 7060, T3 12324, LL2 14352, 197 56956. Single faces solve in 0.3-3 s.
4. **LL2** (from before): face 33 is provably infeasible. The minimal core is two corner pairs (D1 bends NE to E, B1 bends SW to W, the corners facing each other) plus a copy track closing onto its parent inside that face by a forced diagonal step.

## Options we see
- **(A) Shape decides the across-face choices by a topological rule** (e.g. travel order inside a bundle, your earlier suggestion), so Slots stays a pure LP with no binaries. Risk: the simplest rule (inward-only) already failed on V.
- **(B) Slots decides them with exact solves face by face.** Faces are coupled through shared tracks, so a sequence of face solves is a decomposition with no global guarantee unless it is done properly (Benders, or similar).
- **(C) Keep D418 as a heuristic in our test oracle for now and move on.** The oracle has done its job: it found LL2's upstream gap and showed T3 has none. Design the production Slots question later.
- **Separately for LL2:** a promise clause in Structure/Shape, so no copy closes inside a face that holds facing bends.

Our current lean: (C) now, plus the LL2 clause. Then design (A) for Slots with V's 197 as the answer key.

## Questions
1. Given results 1-3, is (A) realistic at all? Can a topological rule for octilinear side choices match a real expert drawing, or is the length-dependence fundamental (meaning some binaries are irreducible)?
2. If some binaries are irreducible, what is the principled home for them: Shape as variant bits (enumerated or searched), or Slots as a MIP with a better formulation? Is there a known stronger formulation of disjunctive separation than big-M with 4 binaries per pair: indicator constraints, convex-hull (Balas) disjunctions, symmetry breaking, or ordering variables shared across pairs (one "x-order"/"y-order" variable per track pair instead of per wall pair, as in VLSI/floorplanning sequence-pair or constraint-graph compaction)?
3. Pairs whose walls are far apart in the face rarely bind. Is lazy separation the standard answer: add a pair's disjunction only once a drawing violates it (cutting planes / branch-and-cut)? Does that count as "by construction", given that the model is still exact and the final answer is checked against the full row set?
4. Our coupling between faces is shared tracks (a wall's position appears in the two faces it borders). Is there a decomposition with a guarantee here, or is a monolithic MIP with lazy pairs the better bet?
5. Is the LL2 clause ("no copy closes inside a face holding facing bends") a reasonable general rule, or too narrow? What is the general form?
6. Anything we are missing? For example, metro-map layout work (Nöllenburg and Wolff's octilinear MIP) handles exactly this kind of 8-direction separation. How do they keep it tractable?
