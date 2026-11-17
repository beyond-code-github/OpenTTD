# L3: warm-start the ladder model from the accepted solution

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Never use git stash, never start a game client, never touch ports 4077/4079 or running openttd processes; build only `openttd_test` in `build-rel`, never `openttd`. Scratch: `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/L3/`.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-planar`, clean at `{HEAD}`. Read `/home/pete/OpenTTD/docs/junction-tool/wip/PROBE-roughuse.md` and the Part A hunk of `/home/pete/OpenTTD/docs/junction-tool/wip/probe-roughuse.patch` (the `PLANAR_WARM` seeding in `SolveMappedVariant`), `Model::Solve` / `last` in `planar_model.h`.

## Change
- In `SolveMappedVariant`, each ladder pass's new model starts from the accepted solution (rough, or the previous pass): set its `last` from that solution when the column counts match, exactly as the probe's Part A, always on (no env knob). Comment: rows only are added (rungs), so the columns match index for index; a mismatch leaves the model cold.
- `src/junction/planar/CLAUDE.md`: one line: each ladder pass starts HiGHS from the accepted solution. No D-refs.
- No new test (speed only; areas must not change): the pv diff is the check.

## Gates
- Build, `jt` gate suite (`/home/pete/OpenTTD/docs/junction-tool/TESTS.md`), T2 and T3 mapped [slow] tests.
- pv before (clean `{HEAD}`) and after on T2, T3 and LL² (201 tags in `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/ll2-tags.txt`), `-j 6`. Stop before committing if any area or reason differs.
- Commit: `Change: Junction ladder passes start from the accepted solution` + blank line + body + `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
Commit; suites; pv (areas identical yes/no, median/max/sum ms before and after per hub); stops; `git status --porcelain`, `git log -1`. Under 12 lines. When you have reported, stop.
