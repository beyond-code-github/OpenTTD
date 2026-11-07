# Brief 1b step D3b: `GenerateJunction`, sweep and scoring

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- **Repo:** `/home/pete/OpenTTD`, branch `feature/junction-tool`, head `{HEAD}`.
- **Plan:** `docs/junction-tool/GENERATOR-1b.md` steps 5–6. Decisions D62, D76, D90.
- **Done:** `BuildJunctionCandidate(spec, params)` in `src/junction_generator.cpp`. The hidden `[.gen]` test in `src/tests/junction_design.cpp` shows that for `MakeTestJunctionSpec()` 4 of 128 candidates are clean (h12, joiner_slack 2–3).

## 2. Read first

- `src/junction_generator.h` and `.cpp` (all)
- `src/tests/junction_generator.cpp`
- the `[.gen]` test

## 3. Changes (`junction_generator.cpp`, tests)

### `GenerateJunction(spec)`
1. **Sweep:**
   - height from `ml_gap+5` to `ml_gap+20`, ascending;
   - for each height, crossing_cost in {0, `prices.bridge(3)`} and joiner_slack 0..3;
   - build each candidate and keep it if `CheckJunctionRules` is empty.
2. **Early stop:** once a height has given a clean candidate, also sweep the next 2 heights, then stop.
3. **Score** every clean candidate. Pick the lowest cost, then the largest open rectangle, then the smaller bounding-box area, then sweep order (first wins). Use integer arithmetic only.
   - **Cost:** `prices.rail_piece` × (the number of track bits over all tiles) + `prices.signal` × (the number of signals) + Σ `prices.bridge(middle tiles)` over the bridges.
   - **Open rectangle (D76):** the largest axis-aligned rectangle of empty tiles inside the layout's bounding box. An empty tile has no track and is not under a bridge span or head. Compute it with the standard histogram method.
4. **Result:** the best layout with `JunctionGenerateError::None`. If no candidate is clean, return `NoLayout`.

### Tests
1. **Outer test:** remove `[!mayfail]` from "GenerateJunction gives a rule-clean L_R T". It must pass.
2. **Deterministic:** two calls on the test spec give identical layouts (compare tracks, bridges and signals).
3. **Scoring unit:** for two hand-built small layouts that differ only in an extra track bit, the scorer prefers the cheaper one. Expose the score only as a function declared in `junction_generator.h`:

   ```cpp
   /** Generator score of a layout: cost, then open space (D76). Lower is better. */
   std::pair<Money, int> ScoreJunctionLayout(const JunctionLayout &layout, const JunctionSpec &spec); // second = -open rectangle area
   ```

   Ties on the pair go to the smaller bounding-box area inside `GenerateJunction`, not in this function.
4. **Spec sweep, hidden tag `[.gensweep]`:**
   - Over TL {3,4,6} × SD {2,4} × ml_gap {0,1,3} × branch_gap {0,1,3}, run `GenerateJunction`.
   - Print one line per spec: the params, either "clean WxH" or "NoLayout", and the time taken in ms.
   - Assert that every layout returned is checker-clean.
   - Run it once for your report.
5. **Rig export:** extend the `[.gen]` test so that when `JUNCTION_JSON` is set, it writes `GenerateJunction(MakeTestJunctionSpec())`'s layout as JSON.
   - Use the JSON writer already in `junction_design.cpp`, moved into a static helper `WriteLayoutJson(const JunctionLayout &, const char *path)` that both tests use.
   - With `JUNCTION_MIRROR` set, mirror it first, as the design test does.
   - Write `tools/ottd/layouts/generated-LR-TL4.json` and `generated-LR-TL4-lefthand.json`.

## 4. Do not

- change `BuildJunctionCandidate`, the router, the signal pass or the checker
- add threads
- tune the sweep beyond the ranges here

## 5. Stop conditions

- The outer test is not clean after your changes.
- Anything not covered.

Report the `[.gensweep]` table; it decides the next step.

## 6. Gates

1. `cmake --build build --target openttd openttd_test > /tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/d3b_build.log 2>&1`. There must be no errors or warnings in junction files.
2. Full suite. The baseline is 208 cases (207 + the outer test); after this step every case passes. Report the full-suite wall time and `GenerateJunction`'s time for the test spec.

Commit message: `Add: Junction generator sweeps candidates and scores them`. Signing is off; end the message with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 7. Report

- commit
- red evidence
- functions added
- suite totals and times
- the `[.gensweep]` table
- the generated test-spec layout's ASCII
- stops
- `git status --porcelain` and `git log -1`

When you have reported, stop.
