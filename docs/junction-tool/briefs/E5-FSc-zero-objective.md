# Brief E5-FS-c: the drawing program without its tidy objective (measurement)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `{HEAD}`. Work and commit only there.
- Plan: `docs/junction-tool/wip/CARD-E5-flexstep.md` (step FS-c). The drawing program is a feasibility oracle; its objective (sum of piece lengths) only tidies the point (CLAUDE.md "Drawing program"). Single 197 faces hit the 15 s limit with it; this step measures them with a zero objective.

## 2. Read first
- `src/junction/planar/planar_draw.h` (whole).
- `src/junction/planar/planar_draw.cpp` lines 85-165 (the `var` helper and the piece lengths, where the length variables get cost 1).
- `src/tests/junction/planar/planar_draw.cpp`: the hidden test "PSG 197 by sides, every face alone and whole" and `Shaped197`.

## 3. Tests
1. Inner, `[junction][planar][planardraw]`: `planar draw: no objective still decides`. On `ShapeFirst(PlanarTText(1), 1)` with `{.clearance = false, .tidy = false}`: feasible, and the result's `points` are non-empty. Red: does not compile until the member exists, so first add the member with no effect, build, and check the test passes trivially; that is acceptable here (this step is a measurement switch, there is no failing behaviour to show). Say so in the report.
2. Hidden measurement, `[.][planarkeepapart]`: `planar draw: PSG 197 undecided faces without the objective`. `Shaped197()`; for each face in {4, 6, 46, 52, 64, 71, 72, 75, 84, 99, 124, 141, 147, 153}: run `CheckPlanarDrawable(shaped, 15.0, {.only_faces = {f}, .tidy = false})` and `WARN` face, status, ms, first 5 conflict tags. Then the whole hub with `{.tidy = false}` at 60 s: `WARN` status, ms, conflict families with counts. Then `WARN` a summary: `FSc faces: feasible a, infeasible b, undecided c; whole: <status> in <ms> ms`.

## 4. Changes
1. `PlanarDrawOptions`, after `flex_sides`: `bool tidy = true; ///< The objective: true minimises the sum of piece lengths (a tidy point); false sets no objective (feasibility only, a measurement).`
2. In `CheckPlanarDrawable`, the piece length variables' cost is `options.tidy ? 1 : 0` instead of `1`. No other cost changes.
3. `src/junction/planar/CLAUDE.md`, "Drawing program", first bullet: after "the objective only tidies the point (the sum of piece lengths)" add "; `PlanarDrawOptions::tidy = false` sets no objective".

## 5. Do not
- Change any row, the time limits, or other tests.

## 6. Stop conditions
- Any existing test fails. Anything not covered.

## 7. Gates
- Debug `build` -j 4, `tools/junction/jt fast build '[planardraw]'`; Release `build-rel` -j 4, `JT_JOBS=4 tools/junction/jt gate build-rel` (about 31 s, OVER BUDGET at 30 s is known; `FAILED:` lines inside passing cases in junction_joint, planar_structure:686, junction_real_terrain:397 and :465 are not new).
- Commit: `Change: Junction drawing program: an option without the tidy objective`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.
- Then, alone, Release: `build-rel/openttd_test "planar draw: PSG 197 undecided faces without the objective" > /tmp/claude-1000/fsc-197.log 2>&1` (budget about 6 minutes).

## 8. Report (short)
- References read; commits; red note; members added
- Suite results per gate (count, time)
- 197: the summary line, each face's status and ms, the whole hub's status and conflict families
- Stops; state (`git status --porcelain`, `git log -1 --format='%h %s'`)

When you have reported, stop.
