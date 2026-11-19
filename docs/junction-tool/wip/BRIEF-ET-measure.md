# Brief ET (landing): empty tiles inside the outline, a scorecard measure

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1` (on `wt-tracks`; its build-rel currently holds a probe-st binary). Run `git switch -c wt-empty feature/junction-tool` (at 2890586981). Commit only there; the orchestrator merges it. Build dir `build-rel`, target `openttd_test` only. Finish with `git switch wt-tracks` and rebuild `openttd_test` in build-rel.
- Time box: 60 minutes. One heavy job at a time. Tests via `tools/junction/jt` with the budgets in `docs/junction-tool/TESTS.md`.
- S = `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad`.

## Why
D354 and D357 (read both in `docs/junction-tool/DECISIONS.md`): area is not compactness; each step is judged on a scorecard whose first number is the empty space the outline encloses. This adds it beside `wasted`, exactly like the WS landing did (`git show 5c81542852 --stat` and its diff are the pattern to follow).

## Read first
- `CODINGSTYLE.md` (C++ part).
- `git show 5c81542852` (the WS landing: function, member, both fill sites, test, pv, CLAUDE.md, TESTS.md).
- `src/junction/planar/planar_components.h`: `PlanarProgramPiece`, `PlanarRouteProgram`, `PlanarComponentLayout`.

## Definition
Occupied tiles: the number of distinct tiles `(x, y)` over every piece of every route program (every route, path and copy; every piece kind). Empty tiles: `area - occupied` (a double; the outline area has quarter tiles).

## Tests (outermost first; each red before its code, record the failure message)
1. In `src/tests/junction/planar/planar_components.cpp`, next to the wasted test, `"planar occupied tiles: distinct tiles over every program"` (same tags as the wasted test): program A route 0 copy 0 tiles (0,5)..(9,5); program B route 0 copy 1 tiles (0,5)..(3,5), (4,4), (5,4), (6,4), (7,5)..(9,5). Assert `PlanarOccupiedTiles({A})` is 10 and `PlanarOccupiedTiles({A, B})` is 13.
2. The existing T3 and T2 mapped tests' `INFO` line gains ` empty <e>` after the wasted value, `e` with two decimals (no new assertion).

## Changes
- `planar_components.h`: `int PlanarOccupiedTiles(const std::vector<PlanarRouteProgram> &routes);` with a doc comment stating the definition; member `double empty = 0; ///< Empty tiles inside the outline: area minus PlanarOccupiedTiles over the routes.` after `wasted`.
- `planar_components.cpp`: the function; `layout.empty = layout.area - PlanarOccupiedTiles(layout.routes);` right after each `layout.wasted = ...` line (both sites).
- Test file: the INFO line as above.
- `tools/junction/pv`: the regex takes an optional ` empty (\S+)` after wasted; output line `<tag> <area|-> <WxH|-> <ms|-> <track|-> <wasted|-> <empty|-> <reason|ok>`; update the usage text.
- `src/junction/planar/CLAUDE.md`, "Component solver", beside the wasted fact: "`PlanarComponentLayout::empty` is the outline area minus the distinct tiles holding any piece of any route program (every path and copy); pv prints it after wasted."
- `docs/junction-tool/TESTS.md`: the new test with its budget in the file's format.

## Do not
- Change the solver, objective, composer or any rule. Add another member or type. Touch ports 4077/4079 or start a game client. Use git stash.

## Gates
- The new test (red then green); `jt` fast planar tests (count and time); the T3 and T2 mapped tests once each with output to a file (record the printed area, wasted and empty); `pv run build-rel T2 --out <S>/et-pv-T2.txt` and the same for T3 (record that the empty column is filled, and the best variant's area, track, wasted and empty).
- One commit: `Add: Junction empty tiles inside the outline measured per solve`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
References read; commit hash; red evidence; members added; gate results with counts and times; the T2/T3 values; `git status --porcelain`; `git log -1`. Finish with `git switch wt-tracks` and the rebuild. When you have reported, stop.
