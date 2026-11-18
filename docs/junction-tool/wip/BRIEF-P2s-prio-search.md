# P2s: prio joins and signals by search, judged by the checker (D345)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Never use git stash, never start a game client, never touch ports 4077/4079 or running openttd processes (rig + engineer's client running); build only `openttd_test` in `build-rel`, `-j 8`; one test run at a time (known CPU instability). Scratch: `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/P2s/`.

## State
Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-planar`, clean at `{HEAD}`. Read: `/home/pete/OpenTTD/CODINGSTYLE.md`; `/home/pete/OpenTTD/docs/junction-tool/DECISIONS.md` rows D75, D79, D81, D107, D342, D345; DESIGN-tracks §13; `src/junction/signals/junction_signals.cpp` (the prio block ~100-200: today's pattern: join at even s, ML signal + reversed row signal (Combo, last Exit) at odd s, s = 1..n_prio pieces back from the merge M); `CheckPrio` / `CheckPrioReach` in `src/junction/junction_rules.cpp`; the previous attempt `/home/pete/OpenTTD/docs/junction-tool/wip/P2-change-wip.patch` (reuse its units walk upstream from the merge and its row tile per piece: side tile beside an axis piece, inner corner tile with the parallel corner track beside a 45° piece turning toward the row; and its two signal-pass tests, the straight row-step one and the bent one). Do not reuse its hand rules for where joins and signals go.

## Design (settled)
- Walk: the ML pieces upstream from the merge piece, in order, until the walk has covered at least the units today's straight prio covers (2 per axis piece, 1 per 45° piece) and may extend further (a join that cannot be placed earlier is placed later; more coverage is allowed).
- Elements, in walk order: J0 = the merge-tile join (fixed, as today), then alternately a signal pair S (ML signal of `spec.signal_type` on the ML piece + reversed row signal beside it, Combo; the last pair's row signal Exit, as today) and a join J (h1 on the ML piece, axis+h2 on its row tile, as today), ending with a join. Join candidates: axis ML pieces whose row tile is free; signal candidates: plain axis ML pieces (no switch, not on a bridge) whose row tile is free. Row tiles between elements get the row track (axis beside axis pieces, corner track beside a bend piece).
- Search: depth-first over element positions in walk order, fewest elements first (start from today's count n_prio/2 joins and grow by one pair up to twice that), positions in increasing walk order, each element strictly after the previous; the FIRST candidate tried at each depth is today's pattern position (so a straight prio reproduces today's layout exactly). At each leaf, draw the candidate into a scratch copy of the tracks/signals and run `CheckPrio` and `CheckPrioReach` restricted to this prio (if they cannot be restricted, run them whole and accept when no violation names this prio's tiles); accept the first leaf with no violation. Cap: 20000 leaves per prio; on cap or exhaustion set `why` = "the prio element at X,Y does not fit" as today.
- If calling the checker from the signal pass is not possible without a dependency the code forbids (check the folder CLAUDE.md files), stop and report.

## Tests first
- Apply the previous attempt's two signal-pass tests (straight row-step: green today; bent: red today with "the prio element at 3,1 does not fit" / missing prio). Record red. Add: a straight prio test asserting the result is byte-identical to today's pattern (tracks and signals) for prio_tiles 2..8.

## Gates
Build; `jt gate` (report time and load); `[block]`, `[hub]`, signal-pass tests; mapped T2/T3 checker tests; pv default T2 and T3 against `/home/pete/OpenTTD/docs/junction-tool/wip/pv-baselines/` identical (stop if a reason changes). Report the search's leaves per prio (max) on T2/T3.
- `src/junction/CLAUDE.md`: the fact (prio placement by search judged by CheckPrio/G9b; straight pattern first). No D-refs.
Commit: `Change: Junction prio joins and signals placed by search, judged by the checker` + body + `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
Commit; red evidence; suites, gate time; pv identical; max leaves; stops; `git status --porcelain`, `git log -1`. Under 15 lines. When you have reported, stop.
