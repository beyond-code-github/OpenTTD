# S2f: the parallelogram style leaves the sweep; e_slack becomes a pinned constant

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/s2f`, branch `wt-s2f`, head given in your launch prompt. You are the only writer there. Other agents work in sibling worktrees under `.claude/`; never touch them or the main worktree `/home/pete/OpenTTD`.
- Build: configure once, `cmake -S . -B build-rel -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-18 -DCMAKE_CXX_COMPILER=clang++-18 -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache > cfg.log 2>&1`, then `cmake --build build-rel --target openttd_test -j 8 > build.log 2>&1`.
- Tests: `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/ptest.sh build-rel/openttd_test "<filter>" 8` (8 jobs: other agents share the machine). Iterate with `"[junction]~[slow]"` or your own filter; full `"~[.]"` only at the gate.
- Known flake: generator tests occasionally segfault or SIGILL under parallel load (pre-existing, under investigation elsewhere). If one case crashes, rerun that case alone 3 times; report it, don't chase it.
- Plan: `docs/junction-tool/PLAN-S2.md` §3 (E1, Pipeline), §4 (S2f). Decisions in `/home/pete/OpenTTD/docs/junction-tool/DECISIONS.md` (newer than your copy): D145, D146 (this step's ruling), D144, D108, D114.

## Why (settled)
Since S2e the parallelogram style (lane_style 1) takes its positions from the solver and ignores `w_slack`, `bridge_margin`, `gap` and `sb_back`. Yet `GenerateJunctionUnchecked` and `BuildJunctionLibrary` still enumerate it for every w_slack (4) × margin (3) × e_slack (4) combination: 48 identical-or-near-identical builds per height and crossing cost. And `e_slack` is a hedge on the E side (`x_s + cover + 1 + params.e_slack`, line ~428), which PLAN-S2 E1 says becomes a pinned constant k_e.

**D146 (orchestrator, 2026-11-09, reversible):** S2f is scoped to style 1. (1) Style 1 is enumerated once per (height, crossing_cost, plane): its E-side window starts at `x_s + cover + JUNCTION_E_PRIO_AFTER_SPLIT`, a constant pinned by measurement and test like D145. (2) Style 0 keeps its full sweep (e_slack, w_slack, margin, gap, sb_back) until S2g decides its fate (D144 Q2 keeps it; neither test spec builds it clean today). (3) The §3 ladder is not built now; if a spec regresses without it, stop and report.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md`.
- DECISIONS rows D145, D146.
- `src/junction_generator.h` 84-140.
- `src/junction_generator.cpp` 205-225 (the D145 constants), 415-432 (E side, pass 1), 712-750 (`BuildJunctionCandidate`), 836-960 (`GenerateJunctionUnchecked`), 965-1030 (`BuildJunctionLibrary`).
- `src/tests/junction_generator.cpp` 28-70, 1180-1245 (the stagger test and "Parallelogram solver constants are minimal (D145)").

## Step 1: measure (before any code change; record the numbers in your report)
Write a temporary test case tagged `[.s2f]` (deleted before commit) that, for `MakeTestJunctionSpec()` and `PinnedTestSpec()`:
- (a) for each e_slack 0..3: over heights ml_gap+5..ml_gap+20 and both crossing costs with `lane_style = 1`, `w_slack = 0`, counts clean candidates (`BuildJunctionCandidate(..., &clean)`) and records the best `ScoreJunctionLayout` among the clean ones;
- (b) runs `GenerateJunction(spec)` and records `ScoreJunctionLayout(*layout).first`, the cost, `size.x × size.y`, and wall time (3 runs, Release);
- (c) runs `BuildJunctionLibrary(spec)` (for `MakeTestJunctionSpec()` size it first as the existing library tests do) and records the entry count, the best entry's score and wall time.

Let e_min be the smallest e_slack with at least one clean candidate. **Stop and report the table** if: e_min differs between the two specs, or on either spec the best clean score comes from an e_slack above e_min, or no e_slack gives a clean candidate. Otherwise `JUNCTION_E_PRIO_AFTER_SPLIT = 1 + e_min`.

## Tests (red first; record each red)
1. Extend "Parallelogram solver constants are minimal (D145)" (rename it "... (D145, D146)") to a third constant `"k_e"`, set through a new seam `params.k_e_delta = -1`. Drop the test's own e_slack loop (style 1 no longer reads e_slack; use e_slack 0). For k_e, assert `clean_count == 0` and record which rules reject it in an `INFO`/comment, as the k_w comment does (assert the rule set only if it is the same on every rejected candidate; otherwise just the count). Red: first set `JUNCTION_E_PRIO_AFTER_SPLIT` to `2 + e_min` and see `clean_count == 0` fail; record it; then set `1 + e_min`.
2. New `[junction][generator][slow]` "GenerateJunction: the solver sweep is no larger than the old sweep (S2f)": for both specs, `GenerateJunction` gives a layout that is `JunctionRulesClean`, with `ScoreJunctionLayout(...).first` ≤ the Step 1 (b) value and area ≤ the Step 1 (b) area (literal numbers with a comment "measured at <head> before S2f"). This is a guard: it passes before and after; record that it passes at head before your change.

## Changes
1. `junction_generator.cpp`: `static constexpr int JUNCTION_E_PRIO_AFTER_SPLIT = <1 + e_min>;` beside the D145 constants, with a comment naming D108/D146 and the rule(s) that reject one less. On the E-side goal line: `const int e_after = params.lane_style == 1 ? JUNCTION_E_PRIO_AFTER_SPLIT + params.k_e_delta : 1 + params.e_slack;` and use `x_s + cover + e_after`.
2. `junction_generator.h`, `JunctionCandidateParams`: add `int k_e_delta = 0; ///< Test seam: added to the parallelogram style's E-side prio distance (D146).` next to `k_w_delta`. Update the docs of `e_slack`, `w_slack`, `bridge_margin` to say "symmetric style (0) only; the parallelogram style ignores it (D145, D146)".
3. One file-static helper in `junction_generator.cpp`, used by both `GenerateJunctionUnchecked` (per height) and `BuildJunctionLibrary` (per height), replacing their nested loops:
   ```cpp
   /** The candidates of one height (D146): the symmetric style over the full sweep, the parallelogram style once per crossing cost. */
   static void AddJunctionSweepParams(std::vector<JunctionCandidateParams> &out, const JunctionSpec &spec, int height, int plane, JunctionRouteCache *cache)
   ```
   Order: `for (crossing_cost : {0, spec.prices.bridge(3)}) { for e_slack 0..3 { for w_slack 0..3 { for margin 1..3 { style 0 } } } ; style 1 with e_slack 0, w_slack 0 }`. Set `plane` and `route_cache` on every entry, as the two loops do today (the library passes plane 0, as today it leaves it at the default).
4. `BuildJunctionLibrary` doc in the header: replace "slack 0..3" with "the D146 sweep".

## Do not
- Change style 0's behaviour, the solver, the lane builder, the D145 constants, the signal pass or the checker.
- Remove `BuildJunctionCandidateWithGap`'s `gap`/`sb_back` parameters or the style-0 ladder (`attempt`): that is S2g.
- Touch other test cases' own parameter loops unless they fail; if one fails, stop and report.

## Stop conditions
- The Step 1 conditions above.
- Any existing test fails after the change (beyond the known flake).
- The guard test fails (footprint or area larger than measured).

## Gates
1. Build `build-rel` `openttd_test`: no errors, no new warnings.
2. `ptest.sh build-rel/openttd_test "[junction]~[slow]" 8`, then `"~[.]"`.
3. Timing: re-run Step 1 (b) and (c) timings after the change (3 runs each) and report before/after; then delete the `[.s2f]` case.
4. Commit: `Change: Junction parallelogram style built once per height and crossing cost, E-side constant pinned`, body ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. Commit only your files.

## Report
- References read
- Commit: hash and subject
- Step 1 table (a), (b), (c), and e_min
- Red evidence per test
- Types, functions, members, constants added or removed: names only
- Timings before/after
- Suite results and wall times
- Stops, and anything left undone
- State: `git status --porcelain` and `git log -1`

When you have reported, stop.
