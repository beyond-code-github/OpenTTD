# Brief SPEED-pass: get generation fast again, with identical output

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- **Repo:** `/home/pete/OpenTTD`, branch `feature/junction-tool`, head `{HEAD}`. The Release suite passes, but it takes 930 s (it was 210 s a few commits ago).
  - `GenerateJunction` on the test spec takes about 3.3 s or more in `build-prof` (it was 0.085 s after D96 P4).
  - Causes, so far as known:
    - separate e/w slack (×4);
    - the tight/loose lane variant rebuild, with `CheckJunctionRules` run inside `BuildJunctionCandidate`;
    - the style-1 shift-back loop over 6 offsets, each with its own build and rule check;
    - G9b (prio reach), which simulates OpenTTD's signal logic for every covered tile.
- **Output must be identical.**
  - Capture the `[.golden]` output (env `JUNCTION_GOLDEN`) before any change; it must match byte for byte at the end.
  - Also capture the library from `BuildJunctionLibrary` for the test spec (`JunctionLayoutToJson` of every entry, plus its hub and score) to a file before and after, and `cmp`.
- **Scratchpad:** `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/speed/`
- **Don't run any `tools/ottd` command.**

## 2. Step 1: measure (no code changes)

1. Rebuild `build-prof` (RelWithDebInfo, `-fno-omit-frame-pointer`, already configured).
2. Time the S1 test ("GenerateJunction gives a rule-clean L_R T") as the median of 3.
3. Profile it with `perf record -g --call-graph fp`. perf can't symbolise on this machine, so use the existing helper approach: the scripts in `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/p1tool/` (`an.py`, `an2.py`, which use `perf script` plus `llvm-symbolizer-18`).
4. Report inclusive % for:
   - `BuildJunctionCandidate`, excluding the rule checks;
   - `CheckJunctionRules`, split per rule function, with `CheckPrioReach` (G9b) and its simulator called out;
   - the router;
   - `PlaceJunctionSignals`.
   Also report how many times `CheckJunctionRules` and `BuildJunctionCandidate` are called per `GenerateJunction`.

## 3. Step 2: changes, in this order, measuring after each

1. **Fail fast.** Add `bool JunctionRulesClean(const JunctionLayout &, const JunctionSpec &)` in `junction_rules.h/.cpp`.
   - It runs the same checks as `CheckJunctionRules`, ordered cheapest first, with G9b (`CheckPrioReach`) last.
   - It returns false at the first violation.
   - Use it everywhere a caller only tests `CheckJunctionRules(...).empty()`: the generator sweep, the tight/loose fallback, the shift-back loop and the library. `CheckJunctionRules` itself is unchanged (the tests and the diagnostics use it).
   - A unit test asserts that `JunctionRulesClean` and `CheckJunctionRules(...).empty()` agree on every candidate of the test spec's sweep.
2. **No duplicate checks.** If `BuildJunctionCandidate` already proved a candidate clean (tight/loose or shift-back), the sweep must not check it again. Pass the result out (an out-param `bool *clean`, defaulting to nullptr) rather than re-running.
3. **Only if** G9b is still more than 30% of the time after 1 and 2: memoise the settled-signal simulation per layout within one `CheckPrioReach` call. The block segmentation is computed once and only the occupied position varies. If that needs more than a local refactor of the simulator, stop and report.

## 4. Tests and gates

1. Golden and library `cmp` identical, before and after.
2. The S1 time after each change, and finally. The target is under 0.5 s; report what you get, even if it misses.
3. The Release suite wall time, finally. Run it in the background and wait.
4. Release suite, all passing.
5. `cmake --build build --target openttd openttd_test > speed/dbg.log 2>&1`: no errors or warnings in the files you touched. Build only.

Commit message: `Codechange: Junction generation checks candidates fail-fast and once`. Signing is off; end the message with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 5. Do not

- change any generated output (golden and library identical)
- change rules, sweep ranges or the scoring

## 6. Stop conditions

- The golden or library outputs differ.
- Anything not covered.

## 7. Report

- commit
- the profile table (before)
- times after each change
- the call counts
- the suite wall time
- the cmp results
- stops
- `git status --porcelain` and `git log -1`

When you have reported, stop.
