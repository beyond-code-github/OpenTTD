# NOTE: one-axis-at-a-time compaction for Slots (D444's principled alternative)

## Why
The suite certified WHAT keep-apart must hold; sides-1..5 and cells-1/2 showed that WHICH axis carries a pair's spacing cannot be fixed from Shape's layout (convexity fixes the half-plane, not the axis), so a one-shot 2D solve carries a disjunction per pair. Classic compaction (topology-shape-metrics) avoids this by moving one axis at a time while the other is frozen.

## The idea, in our octilinear setting
- Freeze every y coordinate. Then:
  - a vertical piece's length is fixed (its y-extent);
  - a diagonal piece's length is fixed too (|dx| = |dy| and dy is frozen);
  - only horizontal pieces' lengths are free.
- Two walls whose y-ranges overlap (they "see" each other horizontally) must keep their left-to-right order; their spacing is a single row along x. Walls whose y-ranges do not overlap are already apart in y (frozen, checked). So every keep-apart row is ONE linear inequality: no binary, no disjunction.
- The x-step is then an LP (in fact difference constraints: a longest-path problem), polynomial, with the suite's exemptions applied per pair as today.
- Then freeze x and do the same in y (vertical pieces free; diagonals again fixed by the frozen x... note: a diagonal's length is fixed by whichever axis is frozen).
- Alternate until no step shrinks the drawing.

## What it needs
1. A first legal drawing: compaction only tightens. Candidates: (a) a generous drawing by construction (Shape's layout stretched: every piece long, faces wide; a simple LP with large spacing, which has no tight pairs); (b) lazy-1's result.
2. Visibility per frozen axis: which wall pairs overlap in the frozen coordinate (a sweep; cheap).
3. The order of overlapping pairs: read from the current drawing (it is legal, so its order is valid and stays valid: each step keeps the order, so each step is legal by construction).
4. The suite's rules as rows along one axis: end runs, crossings on pieces, node gaps, corridors, port gaps, grid parity. Diagonals couple the axes only through their fixed lengths in each step, so these stay linear.

## What it gives and costs
- Each step: legal by construction, an LP, fast (no binaries at all).
- The result is a local optimum (classic 1D compaction is a heuristic; exact 2D compaction is NP-hard). Compactness is secondary to drawability (G3), so this is the right trade.
- The order of walls never changes after the first drawing: a bad first drawing can lock in a poor shape. Mitigation: a better first drawing, or a few lazy 2D rounds before compaction.
- Diagonal pieces: frozen by whichever axis is frozen, so they never change length during compaction unless both axes move. A diagonal that should shrink needs a step that moves both (e.g. a 45-degree sweep step) or the lazy 2D solve. To check on T2.

## First experiment (after lazy-1)
T1 and T2: a generous first drawing by construction (or lazy-1's), then alternating x/y steps; per step: status, ms, bounding box; check every step's drawing with the judge and the domain; compare final size with lazy-1's and with V-like compactness where known.
