# S2f-m2: measure what rejects the D148 E-side window at short coverage (no code change, no commit)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/s2f`, branch `wt-s2f`, head ddb5db8789, with **uncommitted S2f2 edits that must survive** (a copy is in `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/s2f2-edits.patch`). You are the only writer there. Never touch sibling worktrees or the main worktree `/home/pete/OpenTTD`. Never stash, reset, checkout or commit.
- Build: `build-rel` is configured: `cmake --build build-rel --target openttd_test -j 8 > m2-build.log 2>&1`.
- Run: `build-rel/openttd_test "[.s2fm2]" > m2-out.txt 2>&1`.
- Hardware note: this machine's CPU is unstable under heavy all-core load. If a run segfaults or SIGILLs, rerun it; report how often.
- Decisions: `/home/pete/OpenTTD/docs/junction-tool/DECISIONS.md` rows D108, D137, D145, D148.

## Why
The S2f2 edits make the parallelogram style's (lane_style 1) N->E goal window start at `x_s + prio_tiles + 2 + k_e_delta` (D148; `src/junction_generator.cpp`, grep `JUNCTION_E_PRIO_AFTER_SPLIT`). Before, it started at `x_s + prio_tiles + 1 + e_slack` with e_slack swept 0..3, so old e_slack s equals k_e_delta s - 1. At the test spec's prio 14 the new window gives the same layouts. But `SizeJunctionPrio` (grep it) starts at prio 1 and walks up through `GenerateJunctionUnchecked`, and with the new window it now fails ("SizeJunctionPrio sizes the test spec" returns 0, expected 14). So at short coverage something other than G9c/G9b needs a later window. We need to know which rule, and how its need depends on prio_tiles.

## Read first (once each, offset/limit)
- DECISIONS rows D108, D148.
- `src/junction_generator.cpp`: `SizeJunctionPrio`, `GenerateJunctionUnchecked` (what it returns when no candidate is clean), `AddJunctionSweepParams`, the E side of pass 1 (grep `JUNCTION_E_PRIO_AFTER_SPLIT`).
- `src/junction_generator.h`: `JunctionCandidateParams`.
- `src/tests/junction_generator.cpp` 28-70 (`PinnedTestSpec`) and the test "Parallelogram solver constants are minimal (D145, D148)" (how it collects rejecting rules).
- `src/junction_rules.h`: the `JunctionRule` enum and `CheckJunctionRules`.

## Do
1. Record `git diff --stat` to `m2-before.txt`.
2. Add one temporary test case tagged `[.s2fm2]` at the end of `src/tests/junction_generator.cpp`. It prints (`std::cout`) and asserts nothing. For each spec S in {`MakeTestJunctionSpec()`, `PinnedTestSpec()`} and each prio p in 1..15 (set `S.prio_tiles = p`):
   a. `GenerateJunctionUnchecked(S, nullptr)`: whether it gives a layout, and its error.
   b. For k_e_delta 0..4, over heights ml_gap+5..ml_gap+20 and both crossing costs (as `AddJunctionSweepParams`), with `lane_style = 1`, `e_slack = 0`, `w_slack = 0`, `params.k_e_delta = d`: the number built, the number clean (`BuildJunctionCandidate(..., &clean)`).
   c. At k_e_delta 0: for every candidate that builds but is not clean, the set of `JunctionRule`s in its `CheckJunctionRules` violations; print a histogram rule -> number of candidates.
   d. For p in {1, 5, 10} at k_e_delta 0: the full text (rule, message, tile) of the first three violations of the lowest-height built candidate, and `RenderJunctionAscii` of that layout.
3. Build, run, collect.
4. Delete the test case. Confirm `git diff --stat` equals `m2-before.txt` and `git diff` equals the saved patch (`diff <(git diff) <patch path>` empty). Report that state.

## Report
- One table per spec: p, (a) layout yes/no + error, then per k_e_delta the clean/built counts, and the least clean k_e_delta.
- The histograms from (c), one line per p.
- (d) texts and renders (renders may be put in `m2-renders.txt` in the worktree and named instead of pasted, if long).
- One paragraph, your reading only: which rule rejects the window at short coverage, and whether the least clean k_e_delta follows prio_tiles (say how), ml_gap/tl, or neither. Don't propose a fix.
- The final state check from Do 4.

## Do not
- Change any non-test source, the S2f2 edits, or commit anything.
- Change the specs beyond prio_tiles.

When you have reported, stop.
