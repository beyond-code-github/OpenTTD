# Brief D96 step P1: profile junction generation

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- **Repo:** `/home/pete/OpenTTD`, branch `feature/junction-tool`, head `{HEAD}`.
- **This is a measurement step: no source changes and no commits.** `git status --porcelain` must be empty at the end. The new build directory `build-prof/` is untracked and ignored like the other build dirs. Check `git status` after configuring it; if it shows up, stop and report.
- **Why:** D96 (`docs/junction-tool/DECISIONS.md`). Hover should update in near-real time, and today one `GenerateJunction` takes about 0.5 s in Release. We need to know where the time goes before anyone optimises.
- **Scratchpad for all output:** `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/p1/`

## 2. Read first (skim; you need only the names of the stages)

- `src/junction_generator.cpp`:
  - `GenerateJunction`: the sweep over height × crossing cost × slack × plane, with the early stop;
  - `BuildJunctionCandidate`: pass 1 routing, lane doubling, pass 2 routing, crop and normalise, `PlaceJunctionSignals`, and the checker calls, if any;
  - `ScoreJunctionLayout`.
- `src/junction_router.cpp`: `FindJunctionRoute`, with its open set, closed set and `Successors`.
- `src/junction_game.cpp`: `GenerateJunctionNear`.
- The test names in `src/tests/junction_generator.cpp` and `src/tests/junction_game.cpp`.

## 3. Steps

1. **Configure and build:**
   ```
   cmake -S . -B build-prof -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_CXX_FLAGS="-fno-omit-frame-pointer" > p1/cfg.log 2>&1
   cmake --build build-prof --target openttd_test -j 24 > p1/build.log 2>&1
   ```
   Copy any other configure options `build-rel/CMakeCache.txt` needs (for example, if it disables some optional dependency). Don't add new options.
2. **Wall-clock time** each scenario below (run `build-prof/openttd_test "<exact test name>"` under `/usr/bin/time -v` or `time`; take 3 runs and report the median).
   - **S1:** `GenerateJunction gives a rule-clean L_R T` (the test spec, flat, no terrain).
   - **S2:** `GenerateJunction: a long train with a narrow branch still fits` (TL6, branch gap 0).
   - **S3:** `GenerateJunction avoids a block of houses` (terrain).
   - **S4:** `GenerateJunctionNear: a blocked spot gives a nearby fit` (several failing tries).
3. **Profile** each scenario: `perf record -g --call-graph fp -o p1/sN.data -- build-prof/openttd_test "<name>"`. Then:
   - **Self time:** `perf report -i p1/sN.data --stdio --no-children --percent-limit 2 > p1/sN_self.txt`.
   - **Inclusive time:** `perf report -i p1/sN.data --stdio --children --percent-limit 3 -g none > p1/sN_incl.txt`.
   - If `--call-graph fp` gives broken stacks (most samples under `[unknown]`), retry with `--call-graph dwarf` and say so.
4. **Counts** (no code changes): for S1, how many candidates does the sweep build before its early stop? Use the `[.gen]` test's histogram output (`build-prof/openttd_test "[.gen]"`), which already prints a candidate count, or the `[.gensweep]` output if that's clearer.

## 4. Do not

- edit any source, test or CMake file
- run the full suite
- optimise anything, even an obvious hot spot. Name it in the report instead.

## 5. Stop conditions

- `perf` can't record (permissions). Report the error.
- `build-prof` doesn't build.
- Anything not covered.

## 6. Report (short)

- S1–S4 median wall times.
- For each scenario, the inclusive time broken down by stage: router (`FindJunctionRoute` and below), `BuildJunctionCandidate` apart from the router, `PlaceJunctionSignals`, the checker (`CheckJunctionRules` and the like), `ScoreJunctionLayout`, layout copies and normalisation, and terrain lookups. Give percentages.
- The top 10 self-time functions for S1 and S4.
- Within the router: the split between heap/priority-queue operations, the closed-set or visited lookups (hash map or similar), `Successors`, and allocation (`malloc`/`new`).
- The S1 candidate count.
- Your reading of the 3 biggest optimisation levers, each one sentence with its evidence (a number from the profile). Label each as a hypothesis.
- `git status --porcelain` (must be empty) and `git log -1`.

When you have reported, stop.
