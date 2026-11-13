# TT1: measure test times on a quiet machine and bring tags back under budget (TESTS.md)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Goal
The wide gate measured 103-121 s against its 120 s budget (target under 90 s) on a loaded machine. `[topology]` measured 4.7-4.8 s against its 5 s Debug budget. This step:
- measures per-test durations;
- moves the slowest per-step tests into their existing sweep tags, as TESTS.md's rule says ("otherwise tag the test `[slow]`", and the `...sweep` tags);
- updates TESTS.md's table.

It changes tags only, never a test body.

## State
- Worktree `/home/pete/OpenTTD/.claude/nodes2`. First: `git -C /home/pete/OpenTTD/.claude/nodes2 checkout -B wt-tt1 <HEAD given at launch>`. Rebuild `openttd_test` in `build` (Debug) and `build-rel` first. Leave untracked files alone.
- Another agent builds and tests in `.claude/nodes` at the same time, so the machine is not quiet. Before each measured run, record `uptime`'s load average in the output file. Run each measurement twice, and use the smaller time.
- Output files go in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/tt1/`.

## Read first
- `docs/junction-tool/TESTS.md` (all);
- `tools/junction/jt` (all; it logs `--durations yes`).

## Part A: measure
1. `tools/junction/jt gate build-rel`. From its log, list every test case over 1.0 s with its tags and duration, slowest first.
2. `tools/junction/jt fast build <tag> <budget>` for every per-step tag in TESTS.md's table that has a Debug budget. Record its time and any test over 0.5 s.

## Part B: per-step tags over 80 % of their budget
For each per-step tag whose Debug time is over 80 % of its budget:
- move its slowest tests, one at a time, to the matching gate-only sweep tag until the tag is under 80 %. The matching tag:
  - `[topology]` → `[topologysweep]`;
  - `[topoproj]` → `[topoprojsweep]`;
  - `[linerouter]` → `[slow]`;
  - `[junctionmap]` → `[junctionmapedges]`;
  - any other tag: STOP and report it.
- A test moved this way keeps its `[junction]` tag and its other tags, and loses only the per-step tag.

## Part C: the wide gate
Do not move tests out of the wide gate. Report the 15 slowest gate cases (name, tags, Release seconds) and the sum of all cases over 1 s. The orchestrator decides from that list.

## Changes
- Test tags in `src/tests/junction/**` (Part B only).
- `docs/junction-tool/TESTS.md`:
  - update the measured columns of the tags you ran, with the date 2026-11-13 and "TT1";
  - for each moved test, its name and old and new tag in that tag's row;
  - the gate row's new time.

## Do not
- Change any test body, CHECK, fixture or budget.
- Add tags that TESTS.md does not list.
- Touch `src/junction/`.
- Use `pkill`, a broad `kill`, or a bare `git stash`.

## Stop conditions
- A test fails.
- A tag over budget has no matching sweep tag in Part B.
- Anything not covered.

## Gates
- `tools/junction/jt gate build-rel` after the moves: same case count as before, all pass;
- every per-step tag you touched, through `jt fast`, under budget.

One commit on wt-tt1: `Codechange: Junction tests, slow per-step tests moved to their sweeps`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. When no test moved, commit only TESTS.md, as `Doc: Junction test times (TT1)`.

## Report (short)
- the per-step table (tag, time, budget, load);
- the tests moved;
- Part C's list and sum;
- gate count and time, before and after;
- stops;
- `git status --porcelain` and `git log -1`.

When you have reported, stop.
