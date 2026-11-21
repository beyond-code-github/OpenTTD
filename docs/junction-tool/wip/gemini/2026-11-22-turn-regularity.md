# For Gemini: optional bends, compaction hardness, and turn-regularity

## Where we are
- **Model:** an octilinear rail-hub layout. The shape is fixed (every track's heading list); the lengths are unknowns on a half-tile grid.
- **Optional steps:** bend pairs h -> h+-1 -> h with length >= 0 (a sideways step that may or may not appear).
- **Measured:**
  - Exact keep-apart with optional steps needs one disjunction per wall pair: 38k binaries with few flex points, 113k with the flex the expert's hub needs. No solve in 120 s.
  - Restricting binaries to pairs touching a flex group removes almost nothing, because flex groups touch nearly every wall.
  - Convex cells with wall-line clearance are exact only when no step is used.

## What we recall (please confirm or correct, with references)
1. Patrignani: compaction of orthogonal representations is NP-hard in general.
2. Bridgeman, Di Battista, Didimo, Liotta, Tamassia, Vismara, "Turn-regularity and optimal area drawings of orthogonal representations": when every face is turn-regular (no pair of kitty corners, i.e. two reflex corners facing each other), the dimension-wise compaction is exact via two constraint graphs. It is polynomial and needs no disjunctions.

## Questions
1. Are (1) and (2) right? Is there an octilinear analogue of turn-regularity (kitty corners across 4 axes) with the same polynomial result? References if any.
2. **Our two options.**
   - **(B) Constraint-graph compaction with steps still optional.** Our reading: a step that may be 0 or positive creates or removes kitty corners. So visibility is not fixed, and B is hard exactly where it matters. Is that right?
   - **(A) Decide the steps upstream.** A layer before the lengths solve fixes which flex points step and to which side; the faces are then refined. Does choosing steps so that every face is turn-regular (or can be refined to turn-regular cells by dummy edges) give an exact binary-free lengths solve?
   - What is the standard way to make a face turn-regular? Inserting dummy edges between kitty corners, as in rectangularisation?
3. If we take (A), is there a cheap rule or score for which steps to use (and which side), from the literature on bend placement or "shape with flexible bends"? Or is it fine to take them from the expert's builds as defaults?

Short answers, please, with references; say "guess" where you are guessing.
