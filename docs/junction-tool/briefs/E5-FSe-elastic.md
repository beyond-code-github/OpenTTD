# Brief E5-FS-e: an elastic mode for the drawing program (D409 (2))

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `{HEAD}`. Work and commit only there.
- Ruling D409 (2) in `/home/pete/OpenTTD/docs/junction-tool/DECISIONS.md` (main worktree; the last row): an elastic mode that reports which rows had to stretch, replacing the LP-relaxation IIS tags for diagnosis.

## 2. Read first (once each)
- `CODINGSTYLE.md` (C++ part).
- `src/junction/planar/planar_draw.h` (whole); `planar_draw.cpp` lines 85-115 (`var`, `row`, `eq`), the piece-length costs (`tidy`), and the solve and result section (around `milp.Solve()` to the end of `CheckPlanarDrawable`).
- `src/junction/placement/junction_milp.h` (`AddVariable`'s `integer` flag).
- `src/tests/junction/planar/planar_draw.cpp`: `ShapeFirst`, "two ports on a side with no gap are a clearance conflict", `Shaped197`, "PSG 197 by sides, every face alone and whole".

## 3. Tests (outside-in)
1. **Outer (hidden measurement) `planar draw: PSG 197 elastic, whole`, `[.][planarkeepapart]`.** `Shaped197()`; `CheckPlanarDrawable(shaped, 120.0, {.elastic = true})` (designated initializers in declaration order). WARN: status, ms, total slack (`r.slack`), the number of stretched rows; per family (tag up to the first space) the count and the slack sum, sorted by sum; the first 40 entries of `r.stretched` (tag and amount). Then, for tags starting `clear ` or `along `, the cell index is the second word: map it to its face with `PlanarShapeCellFaces(shaped)` and WARN per face the slack sum and count, sorted by sum, at most 30 faces. Run it once after the change (Release) and report.
2. **Inner `planar draw: elastic mode names the stretched rows`, `[junction][planar][planardraw]`.** `ShapeFirst(PlanarTText(1), 1)` with `side_gap = {0, 0, 0, 0}` (as in the no-gap test): `{.elastic = true}` gives `feasible`, `slack > 0`, and some `stretched` tag starting with `clear `, `port ` or `box`. On the unchanged T1 with `{.clearance = false, .elastic = true}`: `feasible` and `slack == 0` (within 1e-6) and `stretched` empty.
   - Red: first add the members with no effect, build, run, and record the failure (status Infeasible, no stretched rows).

## 4. Changes
1. `planar_draw.h`:
   - `PlanarDrawOptions`, after `pocket`: `bool elastic = false; ///< Diagnosis (D409): every row except the piece and box rows gets a slack >= 0 per finite bound with cost 1, the objective is the total slack (piece lengths cost 0), and the result lists the rows that stretched.`
   - `PlanarDrawResult`, after `conflict`: `double slack = 0; ///< With elastic: the total slack of the solution.` and `std::vector<std::pair<std::string, double>> stretched; ///< With elastic: per row tag whose slack is above 1e-6, the slack, largest first (a tag that names several rows sums them).`
2. `planar_draw.cpp`:
   - In `row`: when `options.elastic` and the tag's family (text up to the first space, or the whole tag) is neither `piece` nor `box`, add a continuous slack variable (`milp.AddVariable(0, JunctionMilp::INF, 1, false)`) with coefficient +1 when `lower` is finite and one with coefficient -1 when `upper` is finite (an equality gets both). Keep, beside the program, per slack variable the row's tag. `eq` goes through `row`, so it needs no change.
   - Piece length cost: `options.tidy && !options.elastic ? 1 : 0`.
   - After an Optimal solve with `options.elastic`: sum each slack variable's value into `result.slack` and per tag; fill `result.stretched` with the tags above 1e-6, sorted by amount descending, then by tag.
   - Nothing else changes; without `elastic` the program is identical.
3. `src/junction/planar/CLAUDE.md`, "Drawing program": add one bullet: "Elastic mode (D409, `PlanarDrawOptions::elastic`): every row but the piece and box rows gets a slack per finite bound, cost 1, the objective is the total slack, and `PlanarDrawResult::stretched` lists the rows that stretched (tag, amount); it is the diagnosis for an infeasible drawing, not the IIS tags (LP relaxation)."

## 5. Do not
- Use `InfeasibleRows` for anything new; change other options' defaults; touch Shape.

## 6. Stop conditions
- Any existing test fails. Anything not covered.

## 7. Gates
- Debug `build` -j 4 (to a file), `tools/junction/jt fast build '[planardraw]'`; Release `build-rel` -j 4, target `openttd_test` only (a GUI client runs from `build-rel/openttd`: do not rebuild or kill it), `JT_JOBS=4 tools/junction/jt gate build-rel` (about 31 s, OVER BUDGET at 30 s known; `FAILED:` lines inside passing cases are not new).
- Commit: `Change: Junction drawing program: elastic mode for diagnosis (D409)`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.
- Then alone: `build-rel/openttd_test "planar draw: PSG 197 elastic\, whole" > /tmp/claude-1000/fse-197.log 2>&1` (budget 5 minutes).

## 8. Report (short)
- References read; commit; red evidence; members added
- Suite results per gate (count, time)
- 197 elastic: status, ms, total slack, families (count, sum), the top 40 stretched tags, the faces by slack
- Stops; state (`git status --porcelain`, `git log -1 --format='%h %s'`)

When you have reported, stop.
