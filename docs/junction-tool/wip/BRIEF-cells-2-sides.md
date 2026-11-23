# BRIEF cells-2 (measurement): cells decide the keep-apart sides; the certified pairwise rows do the rest

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 90 minutes wall. No solves. Measurement code in the hidden `[.][cellsmeasure]` test only; no production change.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- cells-1 (`docs/junction-tool/wip/probes/slots/cells-measure.md`): refinement over contracted lines is sound and cheap (197: 162 dummies, 1 non-monotone), but the cell CLEARANCE rows are too strict everywhere (node wedges, S-bend triangles: places where the game allows closeness and the suite-certified pairwise rows have exemptions).
- The idea: use the cells only to DECIDE each pairwise keep-apart pair's side, and keep the suite-certified pairwise rows (with all their exemptions) as they are. The side a cell decides:
  - same cell: the cell is convex, so wall j lies in wall i's inward half-plane: the candidate "i's inward normal" (and symmetrically j's); it is one of the pairwise candidates (D406: the walls' normals).
  - different cells of one face: some dummy separates them; the candidate along that dummy's normal (from i's side towards j's). This is NOT necessarily one of the pairwise candidates: measure its margin directly (the pair's walls' endpoints projected on the dummy's normal, minus the pair's spacing), and say whether an existing candidate coincides with it (same direction).
- Read first: cells-measure.md; sides-forced.md (how sides-1..5 measured a forced candidate's failures); the pairwise pair loop's candidates (`candidates`, spacing) and `PlanarKeepApartHolds`/`PlanarKeepApartEnd`; the cell structures (`PlanarShapeDummy`, which walls bound each cell) and `CellsAccumulate`.

## 2. Measure (per fixture over legal drawings incl. off-interface; and V's 197 at its points)
For every pairwise keep-apart pair with rows (the judge's read-out, default rules):
1. Classify: same cell / different cells (which dummy separates them; if several, the first on the face walk between them) / not decided (a wall not in a refined face, or a dummy wall itself).
2. The decided candidate's margin on the drawing; a negative margin is a FAILURE (the cell-decided side refuses a legal drawing): count per class; first 5 in full per class and fixture.
3. Coverage: share of pairs decided (same + different cells), versus walk_strict ∪ facing (sides-4).
4. Agreement: for pairs walk_strict or facing also forces, does the cell's candidate equal theirs?

## 3. Output
Section "cells-2" at the top of cells-measure.md: a table (pairs, same-cell, different-cell, undecided, failures per class, coverage vs sides-4) and a three-line reading; commit `Doc: Junction: cells decide keep-apart sides, measured (cells-2)`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. `tools/junction/jt fast build '[planardomain]' 5` must pass.

## 4. Report (short)
References read; commit; the table; first failures per class; the reading; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
