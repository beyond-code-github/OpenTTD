# Brief WS (landing): wasted sideways travel, a third measure beside area and track

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1` (on `wt-tracks`). Run `git switch -c wt-wasted feature/junction-tool` (feature/junction-tool is at 7866c2e913; its source equals wt-tracks). Commit only there; the orchestrator merges it. Build dir `build-rel` (target `openttd_test` only).
- Time box: 60 minutes. One heavy job at a time. Run tests via `tools/junction/jt` with the budgets in `docs/junction-tool/TESTS.md`.

## Why
D354: area is not compactness. The engineer's eye keeps finding detours (a line climbing and coming back) that area and track barely see. On T3 every layout wastes about 150 tiles of sideways travel (measured from line dumps with a script); one probe cut area while making the top lane's detour worse. Every solve now reports the waste.

## Read first
- `CODINGSTYLE.md` (C++ part).
- `src/junction/planar/planar_components.h`: `PlanarProgramPiece`, `PlanarRouteProgram`, `PlanarComponentLayout`.
- `src/junction/planar/planar_components.cpp` ~2850 and ~2965 (where `layout.track` and `layout.routes` are filled).
- `src/tests/junction/planar/planar_components.cpp` line ~48 (the `INFO("variant ...` line) and the test style there.
- `tools/junction/pv` (the `INFO` regex and the output columns) and `docs/junction-tool/TESTS.md`.

## Definition
For one route program: the pieces' tiles in order, p_0..p_m. Net displacement (dx, dy) = p_m - p_0. The lateral axis is y when |dx| >= |dy|, else x. Waste = sum over i of |lat(p_{i+1}) - lat(p_i)| - |lat(p_m) - lat(p_0)| (tiles, an integer, never negative). The layout's waste is the sum over its route programs with `copy == 0` and `path == 0` (one program per route).

## Tests (outermost first; each red before its code, record the failure message)
1. In `src/tests/junction/planar/planar_components.cpp`, test `"planar wasted sideways: straight, a step and an up-and-back"` (tag as the file's fast pure tests): three hand-made `PlanarRouteProgram`s with `copy == 0`, `path == 0`, routes 0, 1, 2:
   - straight east, tiles (0,5)..(9,5): waste 0;
   - a net step: (0,5)..(4,5), (5,4), (6,3), (7,3)..(9,3): waste 0;
   - up and back: (0,5)..(3,5), (4,4), (5,3), (6,3), (7,4), (8,5), (9,5): waste 4.
   Assert `PlanarWastedSideways` gives 0, 0, 4 for each alone and 4 for all three; and that a program with `copy == 1` is not counted.
2. The existing T3 and T2 mapped tests' `INFO` line gains ` wasted <n>` after the track (no new assertion).

## Changes
- `planar_components.h`: free function `int PlanarWastedSideways(const std::vector<PlanarRouteProgram> &routes);` with a doc comment stating the definition above; member `int wasted = 0; ///< Wasted sideways travel in tiles (PlanarWastedSideways over the routes).` in `PlanarComponentLayout` after `track`.
- `planar_components.cpp`: the function; set `layout.wasted = PlanarWastedSideways(layout.routes);` right after `layout.routes = ...` at both sites.
- Test file: the INFO line as above.
- `tools/junction/pv`: the regex takes an optional ` wasted (\S+)` after the track; output line `<tag> <area|-> <WxH|-> <ms|-> <track|-> <wasted|-> <reason|ok>`; update the usage text in the file's header comment.
- `src/junction/planar/CLAUDE.md`, "Component solver": one fact: "`PlanarComponentLayout::wasted` is the wasted sideways travel of one program per route (copy 0, path 0): per program the sum of sideways tile steps minus the net sideways move, sideways being the axis across the program's net displacement; pv prints it after the track."
- `docs/junction-tool/TESTS.md`: add the new test with its budget per the file's format.

## Do not
- Change the solver, objective, composer or any rule. Add a type or another member. Touch ports 4077/4079 or start a game client. Use git stash.

## Gates
- The new test (red then green); `jt` fast planar tests; the T3 and T2 mapped tests run once each with output to a file (record the `wasted` value printed: expected about 150 on T3 o1t3f1s0 with no env, at area 1796.25); `pv run build-rel T2` once with output to a file (record that the wasted column is filled).
- One commit: `Add: Junction wasted sideways travel measured per solve`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
References read; commit hash; red evidence (the failure message); members added; gate results with counts and times; the T3 and T2 wasted values; `git status --porcelain`; `git log -1`. Finish with `git switch wt-tracks`. When you have reported, stop.
