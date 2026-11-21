# BRIEF E5-LD: is Shape's 197 drawable? Landings read from a clearance-free drawing (probe, test code only)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Budget
45 min wall total. Solve caps as below; no rerun loops. Builds `-j 4`. One heavy job at a time.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `{HEAD}`; Release build `build-rel` (reuse).
- Context: `/home/pete/OpenTTD/docs/junction-tool/wip/CARD-E5-keepapart.md` (last section) and `wip/probes/e5land/NOTES.md` Round 3. Shape's named landings (H12b) make 197 infeasible even without clearance rows. This probe asks only whether SOME landing choice makes 197 drawable: a feasible result proves 197 drawable; an infeasible one proves nothing.

## 2. Read first (once, offset/limit)
- `CODINGSTYLE.md` (C++ part); `src/junction/planar/planar_draw.h`; `src/junction/planar/planar_shape_layer.h` (`PlanarShapeDummy`, `PlanarShapeFaceWalls`, `PlanarCellWall`, `PLANAR_DUMMY_TRACK`); `src/tests/junction/planar/planar_draw.cpp` (the 197 tests: how 197 is loaded and shaped); `PlanarModel::STEP` in `src/junction/planar/planar_model.h`.

## 3. The probe: one hidden test, `[.][planarkeepapart]`, "planar draw: PSG 197 with landings from a drawing" (test code only; no src/junction change)
1. Load and shape 197 as the existing 197 test does (`shaped`).
2. `base = CheckPlanarDrawable(shaped, 30.0, {.clearance = false})`; REQUIRE feasible. `base.points[t][k]` is the start of piece k of track t (k = pieces.size() is the end). The box: width W = the largest x over all points, height H = the largest y (ports lie on the box edges).
3. For each dummy d of `shaped.dummies` (in index order): corner P = `base.points[d.track][d.piece + 1]`, direction s = `STEP[d.heading mod 8]`. Over the walls of its face (the entry of `PlanarShapeFaceWalls(shaped)` with `face == d.face`), find the first wall the ray P + t*s (t > 0) meets:
   - a track wall (track, piece): the segment from `base.points[track][piece]` to `base.points[track][piece + 1]`; skip the two walls whose piece has P as an end point (the corner's own pieces) and any segment of zero length; a hit is a point Q = P + t s on the segment (end points included), t > 0, least t wins; ties: the lower wall index in the walk.
   - a box edge (track == UINT32_MAX, piece = PlanarSide): the line x = 0 (W), x = W (E), y = 0 (N), y = H (S), when the ray meets it with t > 0.
   Exact integer arithmetic: s has components in {-1, 0, 1}; solve the 2x2 system with integers or doubles and an epsilon of 1e-9.
   Set `d.land_track`, `d.land_piece` on a copy `relanded` of `shaped` to the hit wall (box: `UINT32_MAX` and the side). No hit: leave the dummy as it is and count it.
   Count: dummies whose landing changed, kept, no hit.
4. `r = CheckPlanarDrawable(relanded, 120.0)` (clearance and clear rows on). WARN `LD 197: feasible, status, rows, columns, ms, changed, kept, nohit` and, when infeasible, conflict families with counts and the first 20 tags. Also run `CheckPlanarDrawable(relanded, 60.0, {.clearance = true, .clear_rows = false})` (landings only) and WARN the same line with `LD 197 landings only`.
5. Also WARN the number of cells `PlanarShapeCellWalls(relanded).size()` against `PlanarShapeCellWalls(shaped).size()` (a dummy whose new landing does not split its cell is ignored by the cell walk: report the difference, do not fix it).
No CHECK on feasibility (it is the measurement).

## 4. Do not
- Do not change any file under `src/junction/`.
- Do not iterate (no second round of landings from the second drawing).

## 5. Stop conditions
- `base` infeasible; the face of a dummy missing from `PlanarShapeFaceWalls`; over budget.

## 6. Gates and commit
- `tools/junction/jt fast build-rel "[planardraw]" 10` (the hidden test must not run in it).
- One commit: `Add: Junction drawing program probe: PSG 197 with landings from a clearance-free drawing`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 7. Report (short)
References read; Commit; the `LD` lines; cells count; Stops; State (`git status --porcelain`, `git log -1 --oneline`).

When you have reported, stop.
