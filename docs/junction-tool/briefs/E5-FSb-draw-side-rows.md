# Brief E5-FS-b: clearance rows by sides in the drawing program (D407 (2), D408)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `{HEAD}` (step FS-a is in: Shape refines cells with flex pairs at size 0, `FLEX_CELLS_AT_ZERO`). Work and commit only there.
- Plan: `docs/junction-tool/wip/CARD-E5-flexstep.md` (step FS-b); rulings D407 (2) and D408 in `docs/junction-tool/DECISIONS.md`; proof `docs/junction-tool/wip/SPEC-shape.md` §8.8 ("The rule", "Rows").

## 2. Read first (once each, with offset/limit)
- `CODINGSTYLE.md` (C++ part).
- `docs/junction-tool/wip/SPEC-shape.md` from "### 8.8" to the end.
- `src/junction/planar/planar_draw.h` (whole, 60 lines).
- `src/junction/planar/planar_draw.cpp` lines ~336-470: the clearance section, `clear_walls` and its call sites (cell mode, and pairwise mode's convex faces).
- `src/junction/planar/planar_model.h`: `STEP`, `RungSpacing`.
- `src/tests/junction/planar/planar_draw.cpp`: helpers at the top, `Shaped197` and the test "PSG 197 clearance per cell, faces alone".
- `src/junction/planar/CLAUDE.md`, section "Drawing program".

## 3. Tests, outside-in, in `src/tests/junction/planar/planar_draw.cpp`
1. **Outer (hidden, measurement): `planar draw: PSG 197 by sides, every face alone and whole`, tags `[.][planarkeepapart]`.** `Shaped197()`; for each face `f.face` of `PlanarShapeFaceWalls(shaped)`: `CheckPlanarDrawable(shaped, 15.0, {.only_faces = {f.face}})`; classify feasible / `status == "Infeasible"` / other (undecided); `WARN` one line per face that is not feasible (face, status, ms, first 5 conflict tags). Stop the face loop when 20 minutes have elapsed and `WARN` how many faces were skipped. Then the whole hub: `CheckPlanarDrawable(shaped, 60.0)`, `WARN` status, rows, ms, conflict families with counts (as the "PSG 197 (Shape's own shape)" test does) and the first 20 tags. Finally `WARN("FS 197 faces: feasible {}, infeasible {}, undecided {}, skipped {}; whole: {} in {:.0f} ms")`. No CHECKs: it is a measurement. Run it once after the change (Release) and report the summary line, the whole-hub conflict families, and the non-feasible faces. Not run before the change (it is not a red/green test).
2. **Inner: `planar draw: clearance rows go by sides`, tags `[junction][planar][planardraw]`.** A hand cell, walls in walk order with the cell on the left (`PlanarCellWall{track, piece, heading}`):
   - 0: `{0, 0, 0}` a, north; 1: `{0, 1, 1}` insert, north-east; 2: `{0, 2, 0}` b, north; 3: `{1, 0, 5}` top, south-west; 4: `{2, 0, 4}` left, south; 5: `{3, 0, 2}` bottom, east. `insert = {false, true, false, false, false, false}`.
   - `PlanarCellClearRows(cell, insert)` must return exactly these (vertex, wall, spacing), in any order: (0,3,2) (0,4,2) (0,5,1) (1,3,2) (1,4,2) (1,5,1) (2,4,2) (2,5,1) (3,0,2) (3,2,2) (3,5,1) (4,0,2) (4,2,2) (4,3,2) (5,3,2) (5,4,2). Compare as sorted vectors of tuples.
   - Assert no row targets wall 1, and none goes from vertex 0 or 1 to wall 0 or 2.
   - With `insert` all false, assert some row targets wall 1 (the old rule).
   - Red: the function does not exist (a compile error is not acceptable red): first add the declaration and a stub definition returning `{}`, build, run, and record the assertion failure.
3. **Inner: `planar draw: side rows refuse a step's end caps`, same tags.** Same cell and rows. Evaluate each row on a list of vertex points `V[0..5]` (vertex k is the end of wall k; wall e starts at `V[(e + 5) % 6]`): value `= Dot(V[k] - V[(e + 5) % 6], STEP[(h_e + 6) % 8])`, held when `>= spacing`. Half tiles, x east, y south.
   - Good drawing `V = {(11,16), (13,14), (13,10), (7,16), (7,20), (11,20)}`: every row holds.
   - Step reaching the next side (b of length 0) `V = {(11,16), (13,14), (13,14), (9,18), (9,20), (11,20)}`: row (1,3) fails (value 0 < 2).
   - Adjacent side's vertex inside the step's pocket `V = {(11,16), (13,14), (13,10), (12,11), (12,20), (11,20)}`: row (3,0) fails (value -1 < 2).
   - Red: with the stub (`{}`) the two refusals fail (no row fails); record it.
   These values were checked by hand from SPEC-shape §8.8; if your function returns a different row set than test 2 lists, stop and report the difference (do not change the expected lists).

## 4. Changes
1. `src/junction/planar/planar_draw.h`, before `CheckPlanarDrawable`:
   ```cpp
   /** One clearance row of a cell (SPEC-shape §8.8): the vertex after wall `vertex` keeps `spacing` from the line of wall `wall`, on the cell's side. */
   struct PlanarClearRow {
   	uint32_t vertex; ///< The vertex: the corner after this wall in the cell's walk.
   	uint32_t wall; ///< The wall whose line it keeps from.
   	int spacing; ///< The least distance along the wall's left normal (`RungSpacing` units); 0 for a dummy.
   };

   /**
    * The clearance rows of one cell, by sides (D407 (2), D408), before the variant's own exemptions (span pieces, shared node points, crossing points).
    * A side is a maximal run of walls joined where two consecutive walls have equal headings or one of them is a flex insert.
    * Each vertex k (between walls k and k + 1) has rows against every wall e that is not k or k + 1, not an insert, not a box edge, and not on the side of wall k or of wall k + 1:
    * a track wall with spacing max(RungSpacing(h_e, h_k), RungSpacing(h_e, h_k+1)), a dummy with spacing 0. A vertex between two walls that are no track (dummies, box edges) has no rows.
    * @param cell The cell's walls in walk order, the cell on the left.
    * @param insert Per wall, whether it is a flex insert.
    * @return The rows.
    */
   std::vector<PlanarClearRow> PlanarCellClearRows(const std::vector<PlanarCellWall> &cell, const std::vector<bool> &insert);
   ```
   And in `PlanarDrawOptions`, after `clear_rows`:
   ```cpp
   bool flex_sides = true; ///< With clearance: rows by sides (D407): flex inserts join their side and are no target, and each vertex keeps 0 from its cell's dummies (D408); false is the rule before D407 (sides by equal headings only, no dummy rows).
   ```
2. `src/junction/planar/planar_draw.cpp`: define `PlanarCellClearRows` (a free function, outside `CheckPlanarDrawable`). "No track" means `track == PLANAR_DUMMY_TRACK || track == UINT32_MAX`. Sides: the same union as today's `side` loop in `clear_walls`, with the join condition `cell[i].heading == cell[j].heading || insert[i] || insert[j]`. Spacing uses the walked headings `cell[k].heading`, `cell[k + 1].heading` (as today).
3. In `clear_walls`: delete its `side` computation and replace the target loop `for (size_t e = 0; ...)` with a loop over `PlanarCellClearRows(cell, insert)`, where `insert[i] = options.flex_sides && cell[i].track < nt && cell[i].piece` is in `v.tracks[cell[i].track].flex`. Keep, unchanged and per row, every other filter of today's loop and vertex code: the vertex point from `part[k]`/`part[next]` (skip when neither is valid), the span-vertex skip, `!part[e].valid`, `is_span(cell[e])`, the shared node point test with `find`. When `!options.flex_sides`, skip a row whose wall is a dummy (the old rule). The row itself is written as today (`terms` from `vertex` and `part[e].a`, `normal = STEP[(h + 6) % 8]` with `h = cell[e].heading`, lower bound = the row's spacing, tag `clear {c} {k} {e}`); a dummy row has lower bound 0.
   - Worked call site (the shape, not new code): `for (const PlanarClearRow &r : PlanarCellClearRows(cell, insert)) { const size_t k = r.vertex, next = (k + 1) % n, e = r.wall; ...today's vertex and filter code...; row(std::move(terms), r.spacing, JunctionMilp::INF, fmt::format("clear {} {} {}", c, k, e)); }`. Computing the vertex point once per vertex or once per row are both fine; pick the per-row form if it keeps the code shorter.
4. `src/junction/planar/CLAUDE.md`, "Drawing program", the E5-2 bullet: replace "Clearance rows never target a dummy (a dummy is no track; the cell's convexity keeps vertices inside it); walls that meet straight (equal walked headings) are one side, and a vertex has no row against its own sides" with: "Clearance rows go by sides (D407, `PlanarCellClearRows`, `PlanarDrawOptions::flex_sides`, default true): a side is a run of walls joined at equal walked headings or at a flex insert; no row targets an insert or a box edge; a vertex has no rows against the sides of its two walls; each vertex keeps 0 from its cell's dummies (D408, a dummy is no track). `flex_sides = false` is the rule before D407." Keep the rest of the bullet.

## 5. Do not
- Do not touch `planar_shape_layer.cpp`, the landings, pairwise separation, or any objective.
- Do not add rows against box edges, across dummies, or for grid parity.
- Do not change the expected lists in tests 2 and 3.

## 6. Stop conditions
- Test 2's row set differs from the list (report the difference).
- Any existing test fails after the change: stop, WIP commit `WIP FS-b`, report test and assertion text.
- Anything this brief does not cover.

## 7. Gates
- Debug build `build` (-j 4, output to `/tmp/claude-1000/fsb-build.log`), then `tools/junction/jt fast build '[planardraw]'` and `tools/junction/jt fast build '[planarshape]'`.
- Release build `build-rel` (-j 4), `JT_JOBS=4 tools/junction/jt gate build-rel`. Known: about 31 s, OVER BUDGET at 30 s is not new; `FAILED:` lines inside passing cases (junction_joint, planar_structure:686, junction_real_terrain:397) are not new.
- Then the hidden measurement (Release), alone, output to a file: `build-rel/openttd_test "planar draw: PSG 197 by sides\, every face alone and whole" > /tmp/claude-1000/fsb-197.log 2>&1` (the comma needs `\,`). Wall budget: 25 minutes; it bounds itself at 20 minutes of faces plus 60 s.
- One commit before the measurement: `Change: Junction drawing program: clearance rows by sides (D407, D408)`, body naming `PlanarCellClearRows`, `flex_sides` and the three tests; end with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. The measurement adds no commit.

## 8. Report (short, this format)
- References read
- Commits: hash and subject
- Red evidence: tests 2 and 3, failure text with the stub
- Types and members added or removed: names only
- Suite results: per gate, count and time; any failure you did not cause
- 197: the summary line, the whole hub's conflict families, the list of non-feasible faces (face, status, first conflict tag)
- Stops, and anything left undone, with the reason
- State: `git status --porcelain` and `git log -1 --format='%h %s'`

When you have reported, stop.
