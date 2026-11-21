# For Gemini: deciding pairwise separation directions from the map

## Where we are
- **Model:** octilinear rail-hub layout. The shape is fixed (every track's headings); lengths are the unknowns on a half-tile grid. Optional steps are bend pairs with length >= 0.
- **Keep-apart:** inside each face, every non-incident pair of walls (straight pieces) is separated along one of up to four directions:
  - the normal of wall i, or the normal of wall j;
  - for parallel walls, the normal or the shared direction;
  - in each case, on one side.
  With each pair's direction fixed this is an LP, and optional steps stay linear.
- **Measured on the expert's real drawing** (piece lengths from the save):
  - pairwise separation with the expert's own directions needs almost no slack: 84 over 19 of 1449 pairs, mostly on tracks we could not pin to the expert;
  - convex-cell refinement with wall-line clearance needs 1900-2100, however its dummy edges land.
  So pairwise it is. The open question is choosing each pair's direction without one binary per pair.
- **What we already know:** the side (left or right) is topological, because the face tells which side of each wall the other lies on. The axis (normal of i, normal of j, or the shared direction) depends on lengths.

## Questions
1. Nöllenburg & Wolff ("Drawing and labeling high-quality metro maps by mixed-integer programming", IEEE TVCG 2011) separate non-incident edges pairwise with direction binaries, restricted to edges sharing a face. Did they, or follow-up work, decide those directions combinatorially (from the embedding or the face walk) rather than by binaries? Or show when that is possible or impossible?
2. Is there a known combinatorial rule that picks the separating axis of two walls of one face from the face walk alone? For example: the normal of the wall that the other "faces" when walking between them, or a rule based on the turn sum between them, in the spirit of turn-regularity. Under what conditions is it guaranteed to admit every drawing (no loss of feasibility)?
3. If no such rule is exact, what is the usual cheap fallback in practice: lazy constraints (add a pair's separation only when violated), or fixing directions from a first relaxed drawing? And what is known about how much optimality that loses?

Short answers with references, please; say "guess" where you are guessing.
