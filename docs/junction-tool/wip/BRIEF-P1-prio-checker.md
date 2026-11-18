# P1: CheckPrio follows the ML piece by piece (D342)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Never use git stash, never start a game client, never touch ports 4077/4079 or running openttd processes; build only `openttd_test` in `build-rel`, never `openttd`. ONE heavy job at a time (known CPU instability). Scratch: `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/P1/`.

## State
Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-planar`, clean at `{HEAD}`. Read: `/home/pete/OpenTTD/CODINGSTYLE.md`; `/home/pete/OpenTTD/docs/junction-tool/DECISIONS.md` rows D75, D79, D81, D107, D108, D111, D342; `DESIGN-tracks.md` §13; `wip/RECON-prio-straight.md`; `src/junction/CLAUDE.md`; `src/junction/junction_rules.cpp` `CheckPrio` (~1213-1500, clause 1 at ~1331-1403) and `CheckPrioReach`; the CheckPrio and CheckPrioReach tests in `src/tests/junction/junction_rules.cpp` (~1148-1524) and their fixtures.

## Tests first
1. New test in `src/tests/junction/junction_rules.cpp`: `prio: coverage through a bend passes when the prio reach holds` (tag as the neighbouring prio tests): build, from the existing prio fixture's helpers, a layout whose ML coverage runs straight from the merge, turns 90° (two 45° steps, as the layout pieces allow) and continues straight; joins (D75 streamlined) on axis pieces of both legs; the prio row follows on the side opposite the joiner (it bends with the ML). Assert `CheckPrioReach` has no violation (precondition: if your layout fails G9b, fix the layout, not the rule) and `CheckPrio` has no violation. Red today: CheckPrio reports "prio tile is not in the single row beside the ML" or "not a streamlined join" for the second leg. Record the messages.
2. New test: `prio: the row may leave the main line between joins`: straight coverage, two joins, the row between them steps one tile further out and back (a gap a track could use). G9b clean; CheckPrio clean after the change; red today ("single row"). If the row's step cannot be built with the layout pieces while G9b holds, stop and report.
3. Existing tests that assert the dropped clauses ("prio tile is not in the single row beside the ML", "prio tile holds tracks other than the axis and a join"): list them; for each, if its layout passes G9b, change its expectation to no CheckPrio violation for that clause (D342) and say so in the test comment; if it fails G9b, keep it as a G9b test. Report the list with what you did.

## Change (junction_rules.cpp CheckPrio clause 1 only)
- A join is recognised on any coverage ML tile whose piece is axis, using THAT piece's track and exit edge (not the merge piece's `axis`); the h1/h2 pattern, the merge-tile joiner half and the stub-on-merge-join rule unchanged.
- Remove the row-tile clauses (single row beside the ML; row tile holds only axis or a join). Keep: a coverage ML tile carrying prio track that is not a streamlined join is a violation (both messages unchanged).
- Keep clause 4 and G9c unchanged. Update comments to state D342's rule (G9b judges the row; shape rules only for joins).
- `src/junction/CLAUDE.md`: replace the prio-row shape fact with D342's (no D-refs).

## Gates
Build; `jt gate` (report time; rerun once if a test fails and report both); the `[block]`/`[hub]` prio-span tests and the mapped T2/T3 checker tests (`planar: T2/T3 builds from its mapped component layout and passes the checker`). No pv run needed (checker only); state it.
Commit: `Change: Junction prio check follows the main line piece by piece` + body + `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
Commit; red evidence; the list from test 3; suites; stops; `git status --porcelain`, `git log -1`. Under 20 lines. When you have reported, stop.
