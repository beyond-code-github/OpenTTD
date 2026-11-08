# Brief D96 step P4: parallel candidate sweep (same results)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- **Repo:** `/home/pete/OpenTTD`, branch `feature/junction-tool`, head `{HEAD}`. The Release suite passes.
- **Plan:** `docs/junction-tool/PLAN-D96.md`, step P4.
- **The sweep today:**
  - `GenerateJunction` builds candidates one by one: for each plane, for each height (with an early stop 2 heights after the first clean one), 2 crossing costs × 4 slacks.
  - `BuildJunctionLibrary` builds every candidate, with no early stop.
  - Each `BuildJunctionCandidate` plus `CheckJunctionRules` is independent and pure: it reads the spec, the params and the terrain.
- **This step runs those candidates on threads. The selection must stay identical.** Check that with `[.golden]`, plus a library equality test.
- **Scratchpad:** `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/p4/`
- **Don't run any `tools/ottd` command:** a rig run is live.

## 2. Read first

- `src/junction_generator.cpp`: `GenerateJunction` and `BuildJunctionLibrary`.
- `src/thread.h`: `StartNewThread`, and how OpenTTD names threads. Use `std::thread` directly only if `StartNewThread` doesn't fit a join-all pattern. Say which you used and why.
- The spec's `std::function` members (`bridge_span_ok`, `prices.bridge`): confirm they only read captured data. `MakeJunctionSpec` precomputes arrays (T4), and `MakeTestJunctionSpec` uses pure lambdas. If either mutates shared state, stop.

## 3. Changes (`junction_generator.cpp` only)

1. **A file-static helper:**

   ```cpp
   /** Run fn(i) for i in [0, n) on up to max_threads threads (including the caller); returns when all are done. */
   static void JunctionParallelFor(size_t n, const std::function<void(size_t)> &fn);
   ```

   - It uses `std::max(1u, std::min(8u, std::thread::hardware_concurrency()))` threads.
   - An atomic next-index counter hands out work.
   - The caller thread works too.
   - Join all threads before returning.
2. **`GenerateJunction`:**
   - For each plane, and for each height in the existing loop, fill a `std::vector` of the 8 params in today's order (crossing cost outer, slack inner).
   - Run them in parallel into a results vector: an optional layout, terrain_cost, hub, and the `CheckJunctionRules(...).empty()` result.
   - Then walk the results **in the original order**, applying today's selection code unchanged: `clean_here`, the score, the area tie-break, `best`.
   - The early-stop logic over heights is unchanged. Heights stay sequential.
3. **`BuildJunctionLibrary`:**
   - Build all params in sweep order and run them in parallel.
   - Then walk the results in order, applying today's filtering and dedupe unchanged.

## 4. Tests

1. **Golden:** `[.golden]` before and after, `cmp` identical.
2. **New test "BuildJunctionLibrary is deterministic":**
   - build the library twice for the test spec;
   - same size;
   - each entry's `JunctionLayoutToJson` is equal, along with its hub, score and area.
3. **Release suite,** all passing.
4. **Thread sanitizer** (optional; skip if it takes more than 15 minutes): configure `build-tsan` with `-DCMAKE_CXX_FLAGS="-fsanitize=thread -g" -DCMAKE_BUILD_TYPE=RelWithDebInfo` (Ninja, clang-18, as `build-prof`). Build `openttd_test` and run `"[junction]"`. Report any data race. If it is skipped, say so.

## 5. Gates

1. The golden cmp is identical.
2. Release suite, all passing.
3. `cmake --build build --target openttd openttd_test > p4/dbg.log 2>&1`: no errors or warnings in the files you touched.
4. Timings, before and after, in `build-prof` (rebuild it first; median of 3):
   - S1 `"GenerateJunction gives a rule-clean L_R T"`;
   - S3 `"GenerateJunction avoids a block of houses"`;
   - the library build: the P5 test "FitJunctionLibrary on flat terrain is at least as good as GenerateJunction", which WARNs its build ms. Report that number.

Commit message: `Codechange: Junction candidate sweep runs in parallel`. Signing is off; end the message with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 6. Do not

- change the selection, the scoring, the early stop or the order of evaluation of the results
- parallelise across heights or planes
- touch the router, the signal pass or the checker

## 7. Stop conditions

- The golden files differ, or the library determinism test fails.
- The spec's functions are not thread-safe.
- A data race is reported.
- Anything not covered.

## 8. Report

- commit
- cmp result
- suite totals
- timings, before and after
- the tsan result
- `git status --porcelain` and `git log -1`

When you have reported, stop.
