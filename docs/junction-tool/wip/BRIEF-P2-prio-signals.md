# P2: prio signals follow the main line piece by piece (D342)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Never use git stash, never start a game client, never touch ports 4077/4079 or running openttd processes (a rig and the engineer's client are running); build only `openttd_test` in `build-rel`. Moderate load (known CPU instability; another agent may run a spike in its own worktree): build `-j 8`, one test run at a time. Scratch: `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/P2/`.

## State
Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-planar`, clean at `{HEAD}` (P1 landed: CheckPrio per piece; tests "prio: coverage through a bend passes when the prio reach holds" and "prio: the row may leave the main line between joins" in src/tests/junction/junction_rules.cpp). Read: `/home/pete/OpenTTD/CODINGSTYLE.md`; DECISIONS D97, D107, D111, D342 (in `/home/pete/OpenTTD/docs/junction-tool/DECISIONS.md`); DESIGN-tracks §13; `wip/RECON-prio-straight.md` (item 2); `src/junction/signals/junction_signals.cpp` (the prio pattern ~136-175: `ml(s) = M - s*dir`, `row(s) = ml(s) + side`, same trackdir for n_prio pieces) and its callers (planar_plan.cpp ~328, the block/hub path junction_block.cpp ~1103, junction_signal_pass.cpp ~286-440); the signal pass tests (src/tests/junction/... junction_signal_pass.cpp ~91, ~267) and P1's two new layouts.

## Test first
- New test beside the signal pass tests: take P1's bend layout without its prio signals (tracks and joins only), run the signal placement the mapped path uses (the same entry planar_plan.cpp calls), then assert `CheckPrio` and `CheckPrioReach` give no violation on the result. Red today: the fixed straight pattern puts prio signals off the bent ML or row (record the violations).
- Same for P1's row-step layout if the signal pass places prio signals on it today with violations; if it already passes, say so.

## Change
- In the prio pattern of `junction_signals.cpp`, replace the straight-line positions with a walk along the ML path's pieces upstream from the merge (the coverage), and the row as the piece-by-piece side tile (the row tile beside each ML piece on the side opposite the joiner, as CheckPrio now identifies joins); signals on the same positions relative to that walk as today's pattern places them on a straight prio (so straight prios come out identical). If the pattern needs a rule for where a prio signal goes on a bend that today's straight pattern does not define, stop and report the cases.
- Straight prios must be byte-identical: the existing signal tests and the mapped T2/T3 checker tests guard it.
- `src/junction/CLAUDE.md` (or signals CLAUDE.md if present): one fact line. No D-refs.

## Gates
Build; `jt gate` (report time; it is near its 30 s budget, and over it under parallel load: report the load you saw); `[block]`, `[hub]`, signal pass tests; mapped T2/T3 checker tests. pv default mode T2 and T3 against `/home/pete/OpenTTD/docs/junction-tool/wip/pv-baselines/` must be identical (signals do not change areas; a changed reason is a stop).
Commit: `Change: Junction prio signals follow the main line piece by piece` + body + `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
Commit; red evidence; suites, gate time; pv identical yes/no; stops; `git status --porcelain`, `git log -1`. Under 15 lines. When you have reported, stop.
