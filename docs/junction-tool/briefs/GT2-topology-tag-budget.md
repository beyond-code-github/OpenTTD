# GT2: `[topology]` back under its 5 s Debug budget

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes2 (Debug `build`, Release `build-rel`). First: `git -C /home/pete/OpenTTD/.claude/nodes2 checkout -B wt2-gt2 <HEAD>` (given at launch). Never use git stash. Other agents work in /home/pete/OpenTTD/.claude/nodes (OrderJunctionTrackGraph) and may work in .claude/ng1: never touch them. Use absolute paths.
- `tools/junction/jt fast build '[topology]' 5` runs 70 test cases in 5.1-5.2 s Debug and reports OVER BUDGET (docs/junction-tool/TESTS.md: the per-step fast tag; sweeps belong in `[topologysweep]`, run by the gate).

## Changes
1. Measure each `[topology]` test case's Debug time (`build/openttd_test "[topology]" --durations yes` to a file); list the ten slowest.
2. Move the slowest cases that are sweeps or characterisation loops (many seeds/specs/edits, not a single red-first test of a rule) from `[topology]` to `[topologysweep]` until the tag runs under 4.5 s Debug on a quiet run. A test that is the red-first test of a rule stays in `[topology]`; if it alone is slow, report it instead of moving it.
3. TESTS.md rows for both tags with the new times and which tests moved.

## Gates
- `tools/junction/jt fast build '[topology]' 5` (must pass, report time); `tools/junction/jt gate build-rel` (count unchanged, time).
- One commit: `Codechange: Junction slow topology sweeps move out of the per-step tag`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus the moved tests and times. When you have reported, stop.
