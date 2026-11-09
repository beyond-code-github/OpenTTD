# S2f: the parallelogram style is built once per height, crossing cost and e_slack

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/s2f`, branch `wt-s2f`, head given in your launch prompt. You are the only writer there. Other agents work in sibling worktrees under `.claude/`; never touch them or the main worktree `/home/pete/OpenTTD`.
- Build: configure once, `cmake -S . -B build-rel -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-18 -DCMAKE_CXX_COMPILER=clang++-18 -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache > cfg.log 2>&1`, then `cmake --build build-rel --target openttd_test -j 8 > build.log 2>&1`.
- Tests: `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/ptest.sh build-rel/openttd_test "<filter>" 8` (8 jobs: other agents share the machine). Iterate with `"[junction]~[slow]"` or your own filter; full `"~[.]"` only at the gate.
- Known flake: generator tests occasionally segfault or SIGILL under parallel load (pre-existing, under investigation elsewhere). If one case crashes, rerun that case alone 3 times; report it, don't chase it.
- Plan: `docs/junction-tool/PLAN-S2.md` §3 (E1, Pipeline), §4 (S2f). Decisions in `/home/pete/OpenTTD/docs/junction-tool/DECISIONS.md` (newer than your copy): D145, D146 (this step's ruling), D144, D108, D114.

## Why (settled)
Since S2e the parallelogram style (lane_style 1) takes its positions from the solver and ignores `w_slack`, `bridge_margin`, `gap` and `sb_back`. Yet `GenerateJunctionUnchecked` and `BuildJunctionLibrary` still enumerate it for every w_slack (4) × margin (3) combination: 12 identical builds per (height, crossing cost, e_slack).

**D146 (orchestrator, 2026-11-09, amended after the first S2f stop; reversible):** (1) Style 1 is enumerated once per (height, crossing_cost, e_slack, plane), with w_slack 0 and the default margin. (2) e_slack stays swept for style 1 until its rule is derived: the first S2f measured its minimum clean value as 1 on `MakeTestJunctionSpec()` and 0 on `PinnedTestSpec()` (and the morning probe needed 2), so it is not a constant; step S2f-m finds which rule sets it. (3) Style 0 keeps its full sweep until S2g. (4) The PLAN-S2 §3 ladder is not built now.

Measured at bf09d84efb by the first S2f run (Release; use these, don't re-measure before the change):

| | Make | Pinned |
|---|---|---|
| GenerateJunction score | 193/30400 | 120/19000 |
| size | 41x14 = 574 | 25x14 = 350 |
| time | ~1820 ms | ~325 ms |
| BuildJunctionLibrary entries / best | 27 / 193/30400 | 122 / 120/19000 |
| library time | ~10000 ms | ~4820 ms |

## Read first (once each, offset/limit)
- `CODINGSTYLE.md`.
- DECISIONS rows D145, D146.
- `src/junction_generator.h` 84-140.
- `src/junction_generator.cpp` 712-750 (`BuildJunctionCandidate`), 836-960 (`GenerateJunctionUnchecked`), 965-1030 (`BuildJunctionLibrary`).
- `src/tests/junction_generator.cpp` 28-70.

## Tests
1. New `[junction][generator][slow]` "GenerateJunction: the solver sweep is no larger than the old sweep (S2f)": for both specs, `GenerateJunction` gives a layout that is `JunctionRulesClean`, with `ScoreJunctionLayout(...).first` and area at most the table above (193 / 574 for Make, 120 / 350 for Pinned; comment "measured at bf09d84efb before S2f"). For Make, also `BuildJunctionLibrary` (sized as the existing library tests size it) has a best entry score ≤ 193 tiles. This is a guard: run it at head before your change and record that it passes, then after.
2. All existing tests pass. Library entry counts may drop (duplicates gone); if an existing test asserts an exact entry count and fails, stop and report.

## Changes
1. One file-static helper in `junction_generator.cpp`, used by both `GenerateJunctionUnchecked` (per height) and `BuildJunctionLibrary` (per height), replacing their nested loops:
   ```cpp
   /** The candidates of one height (D146): the symmetric style over the full sweep, the parallelogram style once per crossing cost and e_slack. */
   static void AddJunctionSweepParams(std::vector<JunctionCandidateParams> &out, const JunctionSpec &spec, int height, int plane, JunctionRouteCache *cache)
   ```
   Order: `for (crossing_cost : {0, spec.prices.bridge(3)}) { for e_slack 0..3 { for w_slack 0..3 { for margin 1..3 { style 0 } } style 1 with w_slack 0 } }`. Set `plane` and `route_cache` on every entry, as the two loops do today (the library passes plane 0, as it leaves it at the default today).
2. `junction_generator.h`: docs of `w_slack` and `bridge_margin` say "symmetric style (0) only; the parallelogram style ignores it (D145, D146)". `BuildJunctionLibrary` doc: replace "slack 0..3" with "the D146 sweep".

## Do not
- Change style 0's behaviour, the E side (`e_slack` stays as it is), the solver, the lane builder, the D145 constants, the signal pass or the checker.
- Remove `BuildJunctionCandidateWithGap`'s `gap`/`sb_back` parameters or the style-0 ladder (`attempt`): that is S2g.
- Touch other test cases' own parameter loops unless they fail; if one fails, stop and report.

## Stop conditions
- Any existing test fails after the change (beyond the known flake).
- The guard test fails (footprint or area larger than measured).

## Gates
1. Build `build-rel` `openttd_test`: no errors, no new warnings.
2. `ptest.sh build-rel/openttd_test "[junction]~[slow]" 8`, then `"~[.]"`.
3. Timing: GenerateJunction and BuildJunctionLibrary on both specs after the change (3 runs each, via a temporary `[.s2f]` case deleted before commit); report against the table.
4. Commit: `Change: Junction parallelogram style built once per height, crossing cost and E-side slack`, body ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. Commit only your files.

## Report
- References read
- Commit: hash and subject
- Red evidence per test
- Types, functions, members, constants added or removed: names only
- Timings before/after
- Suite results and wall times
- Stops, and anything left undone
- State: `git status --porcelain` and `git log -1`

When you have reported, stop.
