# For Gemini: convex cells when bends are optional

## Setup
- We draw an OpenTTD rail hub as an LP/MIP on a half-tile grid. The shape is fixed: per track a heading list (8 directions), lengths unknown. An axis piece is 2k half tiles; a diagonal piece moves (+-1, +-1) per unit.
- **Optional steps (flex points):** on some pieces at heading h we insert bend pairs h -> h-1 -> h and h -> h+1 -> h. Each insert has length >= 0, so a sideways step (S-bend) appears or not. No binaries.
- **Keep-apart, today (cells):**
  - Each face between tracks is refined into convex cells by dummy edges from reflex bends. The refinement assumes every insert has length 0.
  - In a convex cell each vertex keeps a spacing from the infinite line of every other wall (a linear row along that wall's normal).
  - Rules already in place: no row targets an insert, walls joined at an insert count as one side, and a vertex has no rows against its own sides.
- **Keep-apart, alternative (pairwise):** per face, each pair of non-adjacent walls is kept apart along one of four (axis, side) directions, with one binary per choice. With the sides fixed it is an LP.

## What we measured on the expert's hub (pins within 4 tiles of the expert's points; elastic slack)
- **Few flex points (344):**
  - Cells: slack 208, the largest clearance row 3.
  - Pairwise, with sides re-picked to a fixed point: 102.
- **More flex points (646), needed to follow the expert's steps:**
  - Cells: slack 948.
  - Pairwise: 66.
- **Where the cell slack goes:** 316 of 380 stretched clearance rows (512 of 634 slack) have an insert at the vertex and a target wall that is the piece next to another insert.
- **Our reading:** once a step is used, the cell is no longer convex. The line of the wall beyond a step is offset from the line before it, so a vertex is held off a line it never faces.

## Questions
1. **Is there a binary-free way to keep convex-cell clearance exact when bends are optional?** For example:
   - refine cells so each flex group sits on a cell boundary (a dummy edge from each insert bend);
   - clear against the segment only over its span (projection), not the whole line;
   - something from the compaction literature (topology-shape-metrics with bends that may vanish; "flexible" or "optional" bends in orthogonal and octilinear compaction).
   Name the technique if one exists.
2. If not, which is cheaper in practice:
   - (a) one binary per flex group (used or not), switching between two cell refinements;
   - (b) pairwise separation only in faces that hold flex groups, cells elsewhere?
   Pairwise in every face was about 38,700 binaries and did not solve in 120 s. Faces with flex are most faces in this hub.
3. Is our reading of the mechanism right, or is there a simpler cause we are missing?

Short answers, please; say "guess" where you are guessing.
