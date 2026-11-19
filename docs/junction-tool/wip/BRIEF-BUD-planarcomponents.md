# Brief BUD (landing, tests only): the `[planarcomponents]` fast tag back inside its budget

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1` (on `wt-tracks`). Run `git switch -c wt-budget feature/junction-tool` (at the hash in your launch prompt). Commit only there; the orchestrator merges it. Build dir `build-rel`, target `openttd_test` only. Finish with `git switch wt-tracks` and rebuild `openttd_test` in build-rel.
- Time box: 45 minutes. One heavy job at a time.
- S = `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad`.

## Why
`tools/junction/jt fast build-rel '[planarcomponents]'` takes 6-14 s against its 5 s Release budget (`docs/junction-tool/TESTS.md` rows for `[planarcomponents]`). The rule (`src/tests/junction/CLAUDE.md`): a fast tag stays inside its budget, or its slow tests are tagged `[slow]`.

## Steps
1. Build, then time each `[planarcomponents]` test case on its own (`build-rel/openttd_test "<name>" --durations yes`, three runs each, output to `<S>/bud-times.txt`): the 8 cases in `src/tests/junction/planar/planar_components.cpp` and `planar_plan.cpp` that carry the tag without `[slow]` (grep `TEST_CASE.*planarcomponents`).
2. Add `[slow]` to the fewest cases, slowest first, so that the remaining cases sum (worst of three runs) to at most 4 s. Change nothing else in the tests.
3. `docs/junction-tool/TESTS.md`: one row for this change in the table's format (the cases moved to `[slow]` with their times, the tag's new time).
4. Gate: `tools/junction/jt fast build-rel '[planarcomponents]'` inside 5 s (run twice, report both); `tools/junction/jt fast build-rel '[planar]'` (count, time).

## Do not
- Change any test's content, any source file, or any other tag. Touch ports 4077/4079 or start a game client. Use git stash.

## Commit and report
- One commit: `Codechange: Junction slow planar component solves out of the fast tag`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Report: references read; commit hash; the per-case times; the cases moved; gate results; `git status --porcelain`; `git log -1`. Finish with `git switch wt-tracks` and the rebuild. When you have reported, stop.
