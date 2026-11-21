# BRIEF E5-CF: four fixes to the drawing program's clearance rows per cell

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Budget
60 min wall total. Each solve has its cap (below); no rerun loops. Builds `-j 4`, JT_JOBS=4. One heavy job at a time.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `{HEAD}`. Release build `/home/pete/OpenTTD/.claude/ng1/build-rel` (reuse).
- Evidence: `/home/pete/OpenTTD/docs/junction-tool/wip/probes/e5land/NOTES.md`, section "Round 3" (fixes 1-4 found with temporary env probes; the probe diff is `wip/probes/e5land/round3-probe.patch`, for reference only, do not apply it).

## 2. Read first (once each, offset/limit)
- `CODINGSTYLE.md` (C++ part).
- `src/junction/planar/planar_draw.cpp` (all; the crossings block near lines 198-228, `clear_walls`, the pairwise block).
- `src/junction/planar/planar_draw.h` (all).
- `src/junction/planar/planar_shape_layer.h` lines 110-145 and `.cpp` `PlanarShapeCellWalls`, `PlanarShapeFaceWalls`.
- `src/tests/junction/planar/planar_draw.cpp` (the 197 tests).
- `src/junction/planar/CLAUDE.md`, "Drawing program".

## 3. Tests (red first; record each red)
T-outer (hidden `[.][planarkeepapart]`): "planar draw: PSG 197 clearance per cell, faces alone". Load and shape 197 as the existing 197 test does. For each face f in {0, 50, 99, 120, 55, 77, 92, 143}: `CheckPlanarDrawable(shaped, 30.0, {.only_faces = {f}})` (clearance and clear rows on, pairwise off); WARN `KA cells face <f>: feasible, status, rows, ms` plus the first 10 conflict tags; `CHECK(r.feasible)`. Red expected after step 4a (only_faces honoured in cell mode, no fix yet): every face Infeasible in well under a second (probe saw about 100 ms).
Also WARN-only (no CHECK) for face 4: its conflict tags (informational: before fix 3 they named `clear 44 10 1`, a vertex between a dummy and the box).

T-inner (fast, `[junction][planar][planardraw]`): "planar draw: cell faces match the cells": on `ShapeFirst(PlanarTText(1), 1)`, `PlanarShapeCellFaces(c.v)` has the size of `PlanarShapeCellWalls(c.v)`, is non-decreasing, and every face index in it appears in `PlanarShapeFaceWalls(c.v)`'s faces. Red: stub returns `{}`.

## 4. Changes
### 4a. Face filter in cell mode
- `planar_shape_layer.h/.cpp`: `std::vector<uint32_t> PlanarShapeCellFaces(const PlanarShapeVariant &v);` with doc comment "Per cell of PlanarShapeCellWalls, in the same order, the face it lies in." Same loop as `PlanarShapeCellWalls`, pushing `face.face` once per cell.
- `PlanarDrawOptions::only_faces` now applies in cell mode too: when non-empty, dummies whose `face` is not listed get no variables and no `land` rows, and cells whose face (from `PlanarShapeCellFaces`) is not listed get no `clear` rows. Update its doc comment ("With pairwise" goes; one sentence for both modes).
- A dummy of a listed face that lands on a dummy of an unlisted face: stop condition (report it; the probe saw none).

### 4b. The four fixes, all inside `clear_walls` except 4's crossing points (both modes use `clear_walls`, so pairwise mode's convex faces get them too)
1. **No dummy as target:** a wall with `track == PLANAR_DUMMY_TRACK` is never the wall `e` of a row (a dummy is not track; the cell's convexity keeps vertices inside it). Vertices on dummies keep their rows against track walls.
2. **Straight corners join sides:** number the cell's walls into sides: consecutive walls whose walked headings are equal (turn 0) are one side. A vertex (between walls k and next) gets no row against a wall `e` whose side is the side of k or of next. Worked case, cell 645 of 197: walls [3] F3b.20 walked h6 and [4] dummy544 h6 are one side; vertex 4 (the landing point, between [4] and [5]) has no row against [3].
3. **Virtual vertices:** a vertex whose two walls are both not track (`track >= nt`: box edges and dummies) gets no rows (a dummy's landing on the box or on a dummy is no track point).
4. **Crossing corners:** per crossing c a point variable pair `cross_point[c]` with two equality rows tagged `cross <c>`: `cross_point[c] = point[t_a][piece_of[c][0]] + param[c][0] * STEP[h]` (the same expression the crossing rows equate). In `clear_walls`, when walls k and next are pieces of two different tracks that cross each other on exactly those pieces (some crossing c with `{t_a, t_b}` equal to the two tracks and `piece_of[c]` equal to the two pieces, matched per side), that corner is `cross_point[c]`: set `part[k].b` and `part[next].a` to it (the wall in this cell is only the part up to the crossing), and the vertex of that corner is it. Walls whose pieces meet at a node are unchanged.

### 4c. Docs
- `planar_draw.h`: `CheckPlanarDrawable`'s comment names the clearance rules in one sentence (no dummy targets, straight corners join sides, no virtual vertices, crossing corners at the crossing point).
- `src/junction/planar/CLAUDE.md`, the E5-2 bullet: the same four facts, as present-tense rules.

## 5. Measurement (after green; Release; output under `/tmp/claude-1000/cf-*.txt`)
Run T-outer and the existing hidden "planar draw: PSG 197 (Shape's own shape)" once each (`build-rel/openttd_test "<name>"`, commas in names escaped `\,`). Report every `KA` line and the 197 lines (status, rows, ms, conflict families).

## 6. Do not
- Do not change Shape, H12b, the landing rows, the pairwise rows, or the objective.
- Do not add exemptions beyond the four rules; an infeasible face is data.

## 7. Stop conditions
- T-outer stays red for a face after all four fixes: report its conflict tags; do not fix.
- A fast tag over budget; a premise false (e.g. a crossing corner whose two pieces are not the crossing's).

## 8. Gates
- `tools/junction/jt fast build-rel "[planardraw]" 10`, `tools/junction/jt fast build-rel "[planarshape]" 10`, then `tools/junction/jt gate build-rel` once (about 31 s at 4 jobs is the known baseline; report count and time).
- One commit: `Fix: Junction drawing program: clearance rows per cell skip dummies, straight sides and virtual vertices, and use crossing points`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 9. Report (short)
References read; Commits; Red evidence; Types and members added (names only); Suite results; Measurements (`KA` and 197 lines); Stops; State (`git status --porcelain`, `git log -1 --oneline`).

When you have reported, stop.
