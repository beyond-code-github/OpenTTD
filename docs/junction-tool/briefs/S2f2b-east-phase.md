# S2f2b: the parallelogram style's E-side window phase (D148a), finishing S2f2

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/s2f`, branch `wt-s2f`, head ddb5db8789, with the **uncommitted S2f2 edits** of `briefs/S2f2-east-window.md` (constant `JUNCTION_E_PRIO_AFTER_SPLIT = 2`, seam `k_e_delta`, style 1 once per crossing cost, the D145 test renamed "(D145, D148)" with k_e). You build on them and commit them together with this step. Never stash, reset or checkout. You are the only writer there; never touch sibling worktrees or the main worktree `/home/pete/OpenTTD`.
- Build: `cmake --build build-rel --target openttd_test -j 8 > build.log 2>&1`.
- Tests: `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/ptest.sh build-rel/openttd_test "<filter>" 8`. Iterate with your own filter; `"[junction]~[slow]"` then `"~[.]"` at the gate.
- Hardware note: this machine's CPU is unstable under heavy all-core load. A lone segfault/SIGILL is most likely that: rerun the case alone and report it.
- Decisions: `/home/pete/OpenTTD/docs/junction-tool/DECISIONS.md` rows D145, D146, D148, D148a (this step's ruling: read it first).

## Why (settled, D148a)
With the S2f2 edits, three tests fail: "SizeJunctionPrio sizes the test spec" (0, expected 14), "SizeJunctionPrio: a slower train needs more coverage" (d138 == 0), "JunctionRulesClean agrees with CheckJunctionRules on the sweep" (no unclean candidate). S2f-m2 measured why: at odd prio_tiles >= 5 the D148 start is rejected by PrioReach (the prio pass cannot place non-adjacent joins), and one tile later is clean; `SizeJunctionPrio` walks prio upwards and hits an odd value with no clean candidate. D148a: for style 1, `e_slack` becomes that phase, swept 0..1.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md`.
- DECISIONS rows D148, D148a.
- `git diff` in the worktree (the S2f2 edits, ~90 lines).
- `src/junction_generator.cpp` 425-435 (the N->E goal line) and `AddJunctionSweepParams` (grep it).
- `src/tests/junction_generator.cpp`: "SizeJunctionPrio sizes the test spec" (67), "SizeJunctionPrio: a slower train needs more coverage" (92), "JunctionRulesClean agrees with CheckJunctionRules on the sweep" (135), "Parallelogram solver constants are minimal (D145, D148)" (grep).

## Tests (red first; record each red)
1. New `[junction][generator]` "Parallelogram E-side window phase (D148a)": `MakeTestJunctionSpec()` with `prio_tiles = 9`; over heights ml_gap+5..ml_gap+20 and both crossing costs (`0`, `spec.prices.bridge(3)`), `lane_style = 1`, `w_slack = 0`: with `e_slack = 0` no candidate is clean and the rejecting rules (from `CheckJunctionRules` of the built, unclean candidates) include `JunctionRule::PrioReach`; with `e_slack = 1` at least one is clean. Red: with only the S2f2 edits (style 1 ignores e_slack) the `e_slack = 1` half fails.
2. "JunctionRulesClean agrees with CheckJunctionRules on the sweep": for `style == 1` also build each candidate with `params.k_e_delta = -1` (D148 measured that start as rejected at the test spec's prio), so the test has unclean candidates again. No other change to that test.
3. The two SizeJunctionPrio tests must pass unchanged.
4. The S2f guard test must pass unchanged (193 / 574 Make, 120 / 350 Pinned).

## Changes
1. `junction_generator.cpp`, the N->E goal line: `const int e_after = params.lane_style == 1 ? JUNCTION_E_PRIO_AFTER_SPLIT + params.e_slack + params.k_e_delta : 1 + params.e_slack;`. Extend the constant's doc comment with: "One more tile (e_slack 1) when the prio pass can't place non-adjacent joins over the coverage (G9b, D148a)."
2. `AddJunctionSweepParams`: style 1 is added for `e_slack` 0 and 1 per crossing cost (w_slack 0), outside the style-0 loops. Doc: "... the parallelogram style once per crossing cost and E-side phase (D146, D148, D148a)."
3. `junction_generator.h`, the `e_slack` doc: "Extra tiles kept between the E->S split's prio coverage and where N->E may join (symmetric style 0); for the parallelogram style the E-side phase, 0 or 1 (D148a)."

## Do not
- Change style 0, the solver, the lane builder, the D145 constants, the checker, the signal pass or `SizeJunctionPrio`.
- Change the expectations of any test other than test 2's loop.

## Stop conditions
- A SizeJunctionPrio test or the guard test fails, or `GenerateJunction` gives a different score on either spec than ddb5db8789 (193/30400 Make, 120/19000 Pinned; Pinned best 120/19150 is also acceptable per D148, report which).
- Test 1's `e_slack = 0` half has a clean candidate.
- Any other existing test fails (beyond a lone hardware flake).

## Gates
1. Build `build-rel` `openttd_test`: no errors, no new warnings.
2. `ptest.sh build-rel/openttd_test "[junction]~[slow]" 8`, then `"~[.]"`.
3. Timing: `GenerateJunction` and `BuildJunctionLibrary` on Make and Pinned, 3 runs each (temporary `[.s2f2b]` case, deleted before commit). Earlier figures: S2f head Make library ~7.5 s; S2f2 edits ~3.97 s.
4. One commit with the S2f2 edits and this step: `Change: Junction parallelogram E-side window derived from the prio rules`, body naming D148 and D148a, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. Commit only source and test files.

## Report
- References read
- Commit: hash and subject
- Red evidence per test
- Types, functions, members, constants added or removed: names only
- Scores, library entry counts and timings
- Suite results and wall times
- Stops, and anything left undone
- State: `git status --porcelain` and `git log -1`

When you have reported, stop.
