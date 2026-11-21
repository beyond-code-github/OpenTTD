# Brief E5-FS-f: a step of length 0 imposes no spacing (SPEC-shape §8.10)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `{HEAD}`. Work and commit only there.
- Spec: `/home/pete/OpenTTD/docs/junction-tool/wip/SPEC-shape.md` §8.10 (main worktree path; your copy is older). Read §8.8 "The rule" for context.

## 2. Read first (once each)
- `CODINGSTYLE.md` (C++ part); §8.10.
- `src/junction/planar/planar_draw.h` (whole); `planar_draw.cpp`: `PlanarCellClearRows` (the line-row loop and its spacing), `clear_walls` (how a line row is written), and the piece lengths (`length[t][i]`, a variable and a coefficient).
- `src/tests/junction/planar/planar_draw.cpp`: "clearance rows go by sides", "side rows refuse a step's end caps", `Shaped197`, "PSG 197 by sides, every face alone and whole".

## 3. Tests (outside-in)
1. **Outer (hidden measurement) `planar draw: PSG 197 zero-length steps, faces and whole`, `[.][planarkeepapart]`.** As "PSG 197 by sides, every face alone and whole", with `{.tidy = false, .only_faces = {f}}` per face (15 s each, the face loop stops at 15 minutes) and `{.tidy = false}` for the whole hub (60 s). Summary: `FSf 197 faces: feasible a, infeasible b, undecided c, skipped d; whole: <status> in <ms> ms`. Then, for each face that is not feasible, an elastic run `{.only_faces = {f}, .elastic = true}` (keep declaration order) and WARN its total slack and its first 15 `stretched` entries (tag, amount). And for the whole hub, if not feasible, one elastic run `{.elastic = true}` (60 s): WARN total slack, families (count, sum), the first 40 stretched tags. Run once after the change and report.
2. **Inner `planar draw: a step of length 0 imposes no spacing`, `[junction][planar][planardraw]`.** Hand cell (walk order, cell on the left): 0 `{0, 0, 6}` top W; 1 `{1, 0, 3}` closing a SE; 2 `{1, 1, 2}` insert E; 3 `{1, 2, 3}` closing b SE; 4 `{2, 0, 2}` bottom E; 5 `{UINT32_MAX, 1, 0}` box N. `insert = {f, f, t, f, f, f}`.
   - `PlanarCellClearRows(cell, insert)` gives exactly (vertex, wall, spacing, room; `-` = `UINT32_MAX`): (0,4,2,-) (1,0,1,2) (1,4,1,2) (2,0,1,2) (2,4,1,2) (3,0,2,-) (4,0,2,-) (4,1,2,-) (4,3,2,-) (5,1,2,-) (5,3,2,-) (5,4,2,-).
   - Evaluate on points with insert length L (vertex k is the end of wall k; wall e starts at `V[(e + 5) % 6]`): `V = {(10,10), (11,11), (11+L,11), (12+L,12), (20,12), (20,10)}`. A line row holds when `d = Dot(V[k] - V[(e + 5) % 6], STEP[(h_e + 6) % 8]) >= spacing`; a row with a room wall also needs `M * (d - spacing) - L >= 0` with M = 4096.
     - L = 0: every row holds.
     - L = 2: exactly the room parts of (1,0), (1,4), (2,0), (2,4) fail.
   - These lists come from a reference computation of §8.10; if yours differ, stop and report the difference.
   - The FS-b and FS-d hand-cell tests must pass unchanged: their cells have no vertex where the insert's spacing exceeds the other wall's.
   - Red: add the member and a version that sets no room and keeps today's spacing; build; record the failures (spacing 2 instead of 1 at vertices 1 and 2, no room); then implement.

## 4. Changes
1. `planar_draw.h`, `PlanarClearRow`: add `uint32_t room = UINT32_MAX; ///< A flex insert at this vertex whose heading needs more spacing than `spacing` (SPEC-shape §8.10): its length must be 0 unless the vertex keeps more than `spacing`; UINT32_MAX for none.` Extend `PlanarCellClearRows`' doc comment by one sentence on §8.10.
2. `planar_draw.cpp`, `PlanarCellClearRows`, the line-row spacing: when exactly one of walls k and k + 1 is an insert (`insert[]`), spacing = `RungSpacing(h_e, h_other)`, and if `RungSpacing(h_e, h_insert)` is larger, `room` = the insert's wall index. Otherwise as today. Dummy rows (spacing 0) get no room.
3. `clear_walls`: for a row with `room != UINT32_MAX`, after writing the line row, write one more row with the same distance terms times M (`COORD_MAX`) minus the insert's length (`length[track][piece]`'s variable times its coefficient), lower bound `M * spacing`, tag `room {c} {k} {e}`.
4. `src/junction/planar/CLAUDE.md`, "Drawing program", clearance bullet: one sentence: "A flex insert's heading does not count in its corner's spacing; where it would need more, a `room` row lets the insert have length only when the corner keeps more than its base spacing (SPEC-shape §8.10)."

## 5. Do not
- Change defaults, the pocket branch, Shape, or other tests.

## 6. Stop conditions
- The lists differ; any existing test fails; anything not covered.

## 7. Gates
- Debug `build` -j 4 (to a file), `tools/junction/jt fast build '[planardraw]'`; Release `build-rel` -j 4, target `openttd_test` only (a GUI client runs from `build-rel/openttd`: do not rebuild or kill it), `JT_JOBS=4 tools/junction/jt gate build-rel` (about 31 s, OVER BUDGET at 30 s known; `FAILED:` lines inside passing cases are not new).
- Commit: `Change: Junction drawing program: a step of length 0 imposes no spacing (SPEC-shape §8.10)`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.
- Then alone: `build-rel/openttd_test "planar draw: PSG 197 zero-length steps\, faces and whole" > /tmp/claude-1000/fsf-197.log 2>&1` (budget 20 minutes).

## 8. Report (short)
- References read; commit; red evidence; members added
- Suite results per gate (count, time)
- 197: the summary line; per non-feasible face its status and elastic stretched entries; the whole hub's status and, if infeasible, its elastic families and top tags
- Stops; state (`git status --porcelain`, `git log -1 --format='%h %s'`)

When you have reported, stop.
