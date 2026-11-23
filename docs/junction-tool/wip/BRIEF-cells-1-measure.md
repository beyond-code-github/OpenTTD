# BRIEF cells-1 (measurement): convex cells over contracted lines, judged by the suite

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 90 minutes wall. No solves. Measurement code in a hidden test (`[.][cellsmeasure]`) only; no production change.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- Keep-apart sides (sides-1..5, `docs/junction-tool/wip/probes/slots/sides-forced.md`): pair-by-pair rules decide ~30% of pairs safely on V's 197; 1771 pairs stay choices. Convex cells (D385: Shape refines each face into convex cells with dummy edges; inside a cell every clearance is a linear row) would make sides known by construction. They were retired (D431) because (1) a dummy's landing could depend on lengths (D405: 2 of 104 faces on 197), (2) zero-length flex bends made most corners (D407: 830 of 878 dummies), (3) the cell rows were tuned on hubs (and made TS2 look undrawable, shape-4). Contracted lines (D432) remove (2); the suite can judge (3). This step measures whether cells over contracted variants are sound and how many landing choices remain.
- Read first: DECISIONS D385, D405, D407, D408, D431, D432; `planar_shape_layer.h` ~55-125 (`RefinePlanarShapeCells`, `CheckPlanarShapeCells`, `nonmonotone_dummies`, `landing_fallbacks`, `PlanarShapeDummy`); planar_draw.cpp the cell rows (`clear`, `land`, `room`, `dummy` tags; options `clearance`, `clear_rows`, `pairwise` = false); the domain-suite measurement test (`[.][sidesmeasure]`, how it iterates legal drawings incl. off-interface, contracted variants, V's 197 read).

## 2. Measure, per fixture over its legal drawings (incl. off-interface) and on V's 197 at its points
1. Refine cells on each drawing's CONTRACTED variant (`RefinePlanarShapeCells(cv.v, why)` without `landing_from`). If it refuses a contracted variant, record its `why` (count by message) and skip that drawing.
2. Counts per fixture: dummies, `nonmonotone_dummies` (length-dependent landings, D405), cells, refusals.
3. Judge the cell rows on the drawing's points: build the program with the cell machinery (`clearance = true`, `clear_rows = true`, `pairwise = false`, `program_out`, `build_only`, the default rules otherwise) and check `CheckPlanarProgramOnPoints`; record which rows break (tag families `clear`, `land`, `room`, `dummy`, others). A cell row breaking on a legal drawing is TOO STRICT: count per family and give the first 5 in full (fixture, polyline, tag, the walls/dummy involved). If the dummy landing points are undetermined on fixed points (the checker cannot eliminate them), say so and report what is undetermined; do not invent values.
4. Also: on each fixture's ILLEGAL polylines (domain refuses), do the cell rows (with every other row) refuse? Count too-loose cases (domain refuses, cell program accepts) by family.

## 3. Output
`docs/junction-tool/wip/probes/slots/cells-measure.md`: a table per fixture and 197 (drawings, refused refinements, dummies, nonmonotone, too strict by family, too loose) and a three-line reading; commit `Doc: Junction: convex cells over contracted lines measured (cells-1)`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. `tools/junction/jt fast build '[planardomain]' 5` must pass.

## 4. Report (short)
References read; commit; the table; the first too-strict examples; the reading; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
