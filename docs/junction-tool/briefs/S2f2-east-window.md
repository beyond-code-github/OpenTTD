# S2f2: the parallelogram style's E-side window from G9c (D148)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/s2f`, branch `wt-s2f`, head given in your launch prompt (it contains S2f). You are the only writer there. Never touch sibling worktrees or the main worktree `/home/pete/OpenTTD`.
- Build: `build-rel` is configured (clang-18, ccache): `cmake --build build-rel --target openttd_test -j 8 > build.log 2>&1`.
- Tests: `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/ptest.sh build-rel/openttd_test "<filter>" 8`. Iterate with your own filter; `"[junction]~[slow]"` then `"~[.]"` at the gate.
- Hardware note: this machine's CPU is unstable under heavy all-core load. A lone segfault/SIGILL is most likely that: rerun the case alone and report it.
- Decisions: `/home/pete/OpenTTD/docs/junction-tool/DECISIONS.md` rows D145, D146, D148 (this step's ruling: read it first).

## Why (settled, D148)
G9c (with G9b): the E→S split must lie outside the N→E join's prio coverage, which reaches `prio_tiles` tiles back from the join; so the join is at least `prio_tiles + 1` past the split. The router's goal piece is the plain tile one past the merge tile, so the N→E goal window starts at `x_s + prio_tiles + 2`. Today it starts at `x_s + cover + 1 + params.e_slack` and e_slack is swept 0..3 for style 1. With the derived window, style 1 needs no e_slack.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md` (naming, comments).
- DECISIONS rows D145, D148.
- `src/junction_generator.h` 84-100 (`JunctionCandidateParams`).
- `src/junction_generator.cpp` 205-225 (the D145 constants), 420-430 (the N→E goal line), and `AddJunctionSweepParams` (grep it).
- `src/tests/junction_generator.cpp`: "Parallelogram solver constants are minimal (D145)" (grep it) and "GenerateJunction: the solver sweep is no larger than the old sweep (S2f)".

## Tests (red first; record each red)
1. Extend "Parallelogram solver constants are minimal (D145)" (rename it "... (D145, D148)") with a third constant `"k_e"`, set through a new seam `params.k_e_delta = -1`. Drop that test's own e_slack loop (use e_slack 0 for all three constants, since style 1 will not read it). For k_e assert `clean_count == 0` and that the rejecting rules include `JunctionRule::Prio` and `JunctionRule::PrioReach` (D148 measured G9c and G9b rejecting 18 of 18). Red: first set `JUNCTION_E_PRIO_AFTER_SPLIT = 3`, see `clean_count == 0` fail for k_e (k_e − 1 = 2 is clean); record it; then set it to 2 and see it pass.
2. The S2f guard test must still pass unchanged (193 tiles / 574 area on Make, 120 / 350 on Pinned).

## Changes
1. `junction_generator.cpp`, beside the D145 constants:
   ```cpp
   /** D148: the N->E goal window starts this far past the E->S split, beyond the prio coverage: one tile so the split lies outside the join's coverage (G9c, G9b), one because the goal piece is the plain tile after the merge tile. */
   static constexpr int JUNCTION_E_PRIO_AFTER_SPLIT = 2;
   ```
   On the N→E goal line: `const int e_after = params.lane_style == 1 ? JUNCTION_E_PRIO_AFTER_SPLIT + params.k_e_delta : 1 + params.e_slack;` and use `x_s + cover + e_after` in place of `x_s + cover + 1 + params.e_slack`.
2. `junction_generator.h`, `JunctionCandidateParams`: add `int k_e_delta = 0; ///< Test seam: added to the parallelogram style's E-side window start (D148).` after `k_sb_delta`. The `e_slack` doc says "symmetric style (0) only; the parallelogram style uses JUNCTION_E_PRIO_AFTER_SPLIT (D148)".
3. `AddJunctionSweepParams`: style 1 is added once per crossing cost (outside the e_slack loop), with e_slack 0 and w_slack 0. Its doc comment: "... the parallelogram style once per crossing cost (D146, D148)".

## Do not
- Change style 0, the solver, the lane builder, the D145 constants, the checker or the signal pass.
- Change the guard test's numbers.

## Stop conditions
- The guard test fails, or `GenerateJunction` on either spec gives a different score than before your change (record the before score with a temporary print).
- k_e − 1 is clean on Make, or its rejecting rules don't include Prio and PrioReach.
- Any other existing test fails (beyond a lone hardware flake).

## Gates
1. Build `build-rel` `openttd_test`: no errors, no new warnings.
2. `ptest.sh build-rel/openttd_test "[junction]~[slow]" 8`, then `"~[.]"`.
3. Timing: `GenerateJunction` and `BuildJunctionLibrary` on Make and Pinned, 3 runs each, before and after (temporary `[.s2f2]` case, deleted before commit).
4. Commit: `Change: Junction parallelogram E-side window derived from the prio rule`, body ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. Commit only your files.

## Report
- References read
- Commit: hash and subject
- Red evidence per test
- Types, functions, members, constants added or removed: names only
- Scores and timings before/after
- Suite results and wall times
- Stops, and anything left undone
- State: `git status --porcelain` and `git log -1`

When you have reported, stop.
