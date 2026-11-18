# One deadline per variant (the 15 s budget per variant, not per solve)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Never use git stash, never start a game client, never touch ports 4077/4079 or running openttd processes; build only `openttd_test` in `build-rel`. ONE heavy job at a time (known CPU instability). Scratch: `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/deadline/`.

## State
Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-planar`, clean at `{HEAD}`. Read `src/junction/planar/CLAUDE.md`; in `planar_components.cpp`: `BUDGET` (line ~49, 15 s "per variant for the lazy loop"), `SolveLazy1`'s budget check (~2087), `SolveLazy`, `SolveMappedVariant` (its `t0`, the ladder passes calling `SolveLazy(next->M, Clock::now(), ...)` ~2628, the D341 second-shape solve), `Model::Solve`'s MILP time limit (planar_model / junction_milp `time_limit`).
- Today every solve restarts the clock: rough, each ladder pass, and the second shape each get 15 s, so one variant can run ~90 s. The rule (solve budget): a variant not solved in ~15 s is infeasible.

## Change
- One deadline per variant: `SolveMappedVariant` (the outer call, not the recursive per-shape call) fixes `deadline = start + BUDGET`; every `SolveLazy`/`SolveLazy1` inside the variant (both shapes, rough and ladder passes, area pass) checks the same deadline instead of its own `t0`, and each MILP solve's time limit is the remaining time (but no more than its current limit). When the deadline passes: a ladder pass not finished -> the accepted solution stands; a second shape not finished -> the first shape's result stands; a rough solve not finished -> the variant fails with "out of time" as today. Pass the deadline as a `Clock::time_point` parameter replacing the `t0` parameters (no globals, no new type).
- CLAUDE.md: the budget is per variant, shared by every solve in it. No D-refs.

## Test first
`planar components: a variant's solves share one 15 s deadline` ([junction][planarcomponents][slow]): solve LL² variant o1t3f0s0 with `SolvePlanarMappedVariant` and check `layout.micros <= 16e6`. If it already passes before the change (it may, a typical variant takes ~7 s), instead pick the slowest LL² variant from the baseline `/home/pete/OpenTTD/docs/junction-tool/wip/pv-baselines/LL2.txt` (max ms column) and use it; record red (its time before the change) or report that no variant exceeds 16 s.

## Gates
Build; `jt gate`; T2/T3 mapped slow. pv after, default mode, T2, T3, LL² (`--tags /home/pete/OpenTTD/docs/junction-tool/wip/ll2-tags.txt`) one at a time, `pv diff` against `/home/pete/OpenTTD/docs/junction-tool/wip/pv-baselines/{T2,T3,LL2}.txt`: report variants whose area or reason changed (expected: only variants that ran past 15 s), median/max ms. Stop before committing if any best area per hub gets worse.
Commit: `Change: Junction one 15 s deadline per variant, shared by its solves` + body + `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
Commit; red; gate; pv diff summary and ms; stops; `git status --porcelain`, `git log -1`. Under 15 lines. When you have reported, stop.
