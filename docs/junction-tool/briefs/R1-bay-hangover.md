# R1: no train hangs back over the upstream split (D83, D88; engineer 2026-11-10)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## The rule (engineer: bays must be "either exact multiples, or signalled such that a train won't 'hang over' the edge and block traffic going in the other direction")
D88 walks back from the merge signal and puts bay boundary k at the first signal-legal piece at least a bay (B = 2*TL units) back from boundary k-1, stopping when that piece is at or beyond the upstream split. The stretch between the upstream split and the nearest boundary can then be shorter than B; a path signal there lets a train in and it stands with its tail over the split. New rule: a boundary is accepted only if the stretch from the piece after the upstream split up to and including it is at least B units; otherwise the walk stops (one bay fewer). A signal on the path within that short stretch (after the split, before the nearest accepted boundary, closer than B to the split) is a violation: `"signal within a train length after the upstream split"`.

## State
- Worktree `/home/pete/OpenTTD/.claude/r1`, branch `wt-r1`, head given at launch (feature/junction-tool). Create `build-rel` (Release) like `/home/pete/OpenTTD/build-rel`; build `openttd_test` only. (Release because `[rules]` takes 17 s in Debug.)
- Tests only through `tools/junction/jt` (read `docs/junction-tool/TESTS.md`): develop with `tools/junction/jt fast build-rel [rules] 5`; also `tools/junction/jt fast build-rel [signals] 5` and `tools/junction/jt fast build-rel [junctionmap] 5`; final `tools/junction/jt gate build-rel`. OVER BUDGET is a stop. Never type tag filters by hand.

## Read first (once each)
`CODINGSTYLE.md`; `CheckWaitingRoom` in `src/junction_rules.cpp` (about 803-900); D83 and D88 in `docs/junction-tool/DECISIONS.md` (main checkout); `src/tests/junction_rules.cpp` from 1383 ("whole-train bays before a queue merge") and its helper `BayViolations`; in `src/junction_signal_pass.cpp` where bay signals are placed (search "bay"), and the test "PlaceJunctionSignals: a queue longer than two bays gets bay signals".

## Steps
1. Characterise (red): in "whole-train bays before a queue merge", add a check that the case `BayViolations(plain, {1, 5, 9})` reports `"signal within a train length after the upstream split"` at piece 1. First confirm from the helper which piece is the upstream split for `plain`; if piece 1 is NOT within B units after it, stop and report (the gap may not exist). Record the failure.
2. Checker: implement the rule above in `CheckWaitingRoom`'s walk. Update the existing expectations in that test case that the rule changes, each with a one-line comment citing this brief: `{1, 5, 9}` now violates (step 1); `{5, 9}` becomes clean (no "no signal at bay boundary 2"). Change no other expectation; if any other existing rules test changes outcome, stop and report which.
3. Signal pass: if it places bay signals by its own walk, apply the same acceptance rule there (no bay signal closer than B after the upstream split). If it shares the checker's walk, nothing to do. Test: in the signal pass bay test (or a new case beside it), the pass places no signal within B units after the upstream split.
4. Generated layouts: `[junctionmap]`, `[projection]` and the gate must stay clean. If a generated layout becomes unclean, stop and report the layout's violations (rule, tile, message); do not change the generator.
Add `[rules]` to `docs/junction-tool/TESTS.md` (Debug measured 17.5 s with two 8.8 s tests, Release 2.2 s; budget: Release 5 s until those two are split).

## Do not
No other rule, no generator or router change, no change to D83/D88 wording in DECISIONS (the orchestrator records it). No savegame, network or command code. No `std::to_string`/`std::printf` in tests; no `pkill`, broad `kill`, bare `git stash`.

## Stop conditions
As above, plus OVER BUDGET.

## Gates
0 warnings; the jt runs above pass; one commit on `wt-r1`: `Fix: Junction waiting bays leave no train hanging over the upstream split`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
References read; commit; red evidence; expectations changed (each); signal pass changed or shared; jt lines; stops; `git status --porcelain`, `git log -1`.

When you have reported, stop.
