# Brief E5-FS-d: the pocket rule behind a flag, and its 197 measurement (SPEC-shape §8.9)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `{HEAD}`. Work and commit only there.
- Spec: `docs/junction-tool/wip/SPEC-shape.md` §8.9 (read it from `/home/pete/OpenTTD/docs/junction-tool/wip/SPEC-shape.md`, the main worktree; your worktree's copy is older). §8.8 for context.

## 2. Read first (once each)
- `CODINGSTYLE.md` (C++ part).
- §8.8 "The rule" and all of §8.9 (main worktree path above).
- `src/junction/planar/planar_draw.h` (whole) and `planar_draw.cpp`: `PlanarCellClearRows` and `clear_walls` (the loop over its rows).
- `src/tests/junction/planar/planar_draw.cpp`: tests "clearance rows go by sides", "side rows refuse a step's end caps", "PSG 197 by sides, every face alone and whole".

## 3. Tests (outside-in), `src/tests/junction/planar/planar_draw.cpp`
1. **Outer (hidden measurement) `planar draw: PSG 197 pocket rule, faces and whole`, tags `[.][planarkeepapart]`.** As "PSG 197 by sides, every face alone and whole", but every solve uses `{.pocket = true, .tidy = false}` (plus `.only_faces` per face; keep declaration order in the designated initializers), and the face loop stops at 15 minutes. Summary line: `FSd 197 faces: feasible a, infeasible b, undecided c, skipped d; whole: <status> in <ms> ms`. For every face that is not feasible, WARN the face, status, ms and first 5 conflict tags. Also WARN the faces that were feasible in the FS-b run but are not now (the FS-b run's non-feasible faces were 4, 6, 7, 8, 11-17, 46, 52, 64, 71, 72, 75, 84, 99, 124, 141, 147, 153; with tidy = false, 4, 6, 46, 52, 64, 72, 75, 84, 99, 124, 147, 153 became feasible). Run it once after the change and report.
2. **Inner `planar draw: the pocket rule lets an adjacent side use a step's room`, tags `[junction][planar][planardraw]`.** Hand cell (walk order, the cell on the left): 0 `{0,0,0}` a N; 1 `{0,1,1}` insert NE; 2 `{0,2,0}` b N; 3 `{1,0,5}` top1 SW; 4 `{4,0,5}` top2 SW; 5 `{2,0,4}` left S; 6 `{3,0,2}` bottom E. `insert = {f,t,f,f,f,f,f}`.
   - With `pocket = false` the line rows are exactly (vertex, wall, spacing): (0,3,2) (0,4,2) (0,5,2) (0,6,1) (1,3,2) (1,4,2) (1,5,2) (1,6,1) (2,5,2) (2,6,1) (3,0,1) (3,2,1) (3,5,1) (3,6,1) (4,0,2) (4,2,2) (4,6,1) (5,0,2) (5,2,2) (5,3,2) (5,4,2) (6,3,2) (6,4,2) (6,5,2), and no along rows.
   - With `pocket = true`: the same line rows without (3,0,1), and the along rows (vertex, anchor, heading, spacing) exactly (3,1,0,0) (4,1,0,0) (5,0,4,0).
   - Evaluate on points (vertex k is the end of wall k, wall e starts at `V[(e + 6) % 7]`; a line row holds when `Dot(V[k] - V[(e + 6) % 7], STEP[(h_e + 6) % 8]) >= spacing`, an along row when `Dot(V[vertex] - V[anchor], STEP[heading]) >= spacing`):
     - `good = {(11,16),(15,12),(15,6),(13,8),(9,12),(9,20),(11,20)}`: with pocket off, exactly row (3,0) fails (value -2 < 1); with pocket on, nothing fails.
     - `back = {(11,16),(15,12),(15,6),(13,13),(9,12),(9,20),(11,20)}` (top's inner vertex back beside the step): with pocket on, the along row (3,1) fails (value -1 < 0).
   - The existing FS-b hand-cell tests keep calling the function with the pocket off and stay unchanged.
   - Red: first add the declarations and a pocket branch that does nothing, build, run, record the failures; then implement.
   - These lists come from a reference computation of §8.9. If your function gives different lists, stop and report the difference; do not change the expected lists.

## 4. Changes
1. `planar_draw.h`:
   ```cpp
   /** An along row of a cell (SPEC-shape §8.9): Dot(V[vertex] - V[anchor], STEP[heading]) >= spacing, where V[k] is the corner after wall k. */
   struct PlanarAlongRow {
   	uint32_t vertex; ///< The vertex held.
   	uint32_t anchor; ///< The vertex it is held past: the end of the side's last insert, or the start of its first.
   	int heading; ///< The direction along which it is held: the side's walked heading, or its reverse.
   	int spacing; ///< 0 along an axis heading, 2 along a diagonal one.
   };
   ```
   Change `PlanarCellClearRows` to `std::vector<PlanarClearRow> PlanarCellClearRows(const std::vector<PlanarCellWall> &cell, const std::vector<bool> &insert, bool pocket = false, std::vector<PlanarAlongRow> *along = nullptr);` and extend its doc comment with §8.9's rule in two sentences. With `pocket` false it behaves exactly as today and leaves `along` untouched.
   Add to `PlanarDrawOptions`, after `tidy`: `bool pocket = false; ///< With flex_sides: the pocket rule (SPEC-shape §8.9, a probe): an adjacent side's vertices are held past a side's last (first) insert and keep only the lines beyond it.`
2. `planar_draw.cpp`, `PlanarCellClearRows` with `pocket` true, exactly §8.9:
   - Sides as today. Each side's walls in walk order start at its wall whose predecessor (cyclically) is in another side.
   - Skip when the cell has fewer than 3 sides. For each side S with an insert: h = the walked heading of S's first non-insert wall; m = h even ? 0 : 2; L = S's last insert, F = its first; R = the side of the wall after S's last wall; P = the side of the wall before S's first wall.
   - If every wall of R is a track wall (not `PLANAR_DUMMY_TRACK`, not `UINT32_MAX`): for every vertex k (corner after wall k) with wall k or wall k+1 in R, except k = S's last wall (the corner S/R), and not between two non-track walls, add along row (k, L, h, m). If both walls of k are in R, drop the line rows (k, e) for every wall e of S before L in S's order.
   - Mirror for P, if all its walls are track walls: every vertex with a wall in P, except the corner P/S (k = the wall before S's first wall), gets along row (k, F - 1 cyclic, (h + 4) % 8, m). If both walls of k are in P, drop the line rows (k, e) for every wall e of S after F.
3. `clear_walls`: pass `options.pocket` (only with `options.flex_sides`) and an along vector. For each along row, take the vertex point of `vertex` and of `anchor` the same way the line rows take theirs (from `part[k].b`, else `part[k + 1].a`; skip the row when either has no point), apply the same span-vertex skip to `vertex`, and write `Dot(V[vertex] - V[anchor], STEP[heading]) >= spacing` as one row tagged `along {c} {vertex} {anchor}`.
4. `src/junction/planar/CLAUDE.md`, "Drawing program", the clearance bullet: add one sentence: "`PlanarDrawOptions::pocket` (default false, a probe, SPEC-shape §8.9): an adjacent side's vertices are held past a side's last (first) flex insert along its heading (`along` rows, `PlanarAlongRow`) and keep only the lines beyond it."

## 5. Do not
- Change the default of any option, the FS-b tests, Shape, the landings, or pairwise mode.

## 6. Stop conditions
- Test 2's lists differ from yours. Any existing test fails. Anything not covered.

## 7. Gates
- Debug `build` -j 4 (output to a file), `tools/junction/jt fast build '[planardraw]'`; Release `build-rel` -j 4 (target `openttd_test` only; a GUI client runs from that build dir's `openttd`, do not rebuild or kill it), `JT_JOBS=4 tools/junction/jt gate build-rel` (about 31 s, OVER BUDGET at 30 s known; `FAILED:` lines inside passing cases are not new).
- Commit: `Change: Junction drawing program: the pocket rule behind a flag (SPEC-shape §8.9)`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.
- Then alone: `build-rel/openttd_test "planar draw: PSG 197 pocket rule\, faces and whole" > /tmp/claude-1000/fsd-197.log 2>&1` (budget 17 minutes).

## 8. Report (short)
- References read; commit; red evidence; types and members added
- Suite results per gate (count, time)
- 197: the summary line; faces not feasible (face, status, first tag); faces newly not feasible; the whole hub's status and conflict families with counts
- Stops; state (`git status --porcelain`, `git log -1 --format='%h %s'`)

When you have reported, stop.
