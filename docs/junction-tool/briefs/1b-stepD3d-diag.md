# Brief 1b step D3d-diag: why ml_gap 1 with branch_gap 0 gives no layout at TL3 and TL6

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- **Repo:** `/home/pete/OpenTTD`, branch `feature/junction-tool`, head `{HEAD}`. The suite has 234 cases, all passing.
- **This is a diagnosis step. Nothing is committed.** You add temporary prints, run, record the findings, then revert every change (`git status --porcelain` must be empty at the end).
- **The symptom:**
  - `GenerateJunction` returns NoLayout for ML `L_R` (ml_gap 1) with branch `LR` (branch_gap 0) at TL3 and TL6.
  - At TL4 that spec works. Check this in step 3.1 and report what you see.
  - The other specs in the `[.gensweep]` test fail too. Read that test for its list.

## 2. Read first

- `docs/junction-tool/GENERATOR-1b.md` (the D90 skeleton) and `DECISIONS.md` D87 and D90.
- `src/junction_generator.cpp`, all of it, especially `BuildJunctionCandidate`: its pass 1, the crossing set C, the lane doubling, pass 2, the crop, the signal pass, and its usability checks.
- `src/tests/junction_design.cpp`: the `[.gen]` test and its env variables (`JUNCTION_GEN_TL`, `JUNCTION_GEN_SD`, `JUNCTION_GEN_G`, `JUNCTION_GEN_B`, `JUNCTION_GEN_SKIP`).
- `src/tests/junction_generator.cpp`: the `[.gensweep]` test.

## 3. Steps

1. **Build** `openttd_test` in `build/` (output to a file). Run `[.gen]` for G=1 B=0 at TL3, TL4 and TL6 (SD as the test defaults). Confirm which fail.
2. **Instrument** `BuildJunctionCandidate` with temporary `fmt::print(stderr, ...)` lines (`#include "../core/format.hpp"` if needed). Do not use `std::to_string` or `atoi`. Every `return std::nullopt` (or other failure return) should print a short tag naming the step, plus the key numbers: the height, crossing_cost, slack, which movement failed to route, its start, goals and bounds, and C/hW/hE where known.
3. **Run TL3, then TL6,** for G1 B0. For each sweep candidate, collect the failure tag. Make a histogram of tags per TL, and pick the candidate that got furthest (the latest step reached).
4. **For that furthest candidate,** dump enough to explain the failure:
   - **A route failure:** the request bounds, the fences, and which tiles block the goal (print the partial layout as ASCII if the test already has a helper for it).
   - **A checker failure:** the rule ids and the tiles.
   - **A signal pass failure:** which phase.
5. **Compare with TL4 G1 B0,** which succeeds: at which numeric step do the two diverge? For example, does a stub or window length scale with TL so that it overlaps at TL3 and leaves no room at TL6?
6. **Revert** all source changes.

## 4. Do not

- fix anything, or change any test expectation
- commit
- run the full suite more than once (it's not needed; the head is green)

## 5. Stop conditions

- The symptom isn't reproduced (G1 B0 works at TL3 or TL6). Report what you ran.
- Anything not covered.

## 6. Report (short; findings go in the report, not in a file)

- the commands run, and which TLs fail
- the failure-tag histogram for TL3 and TL6
- the furthest candidate's parameters and failure, with the evidence: bounds, blocking tiles, ASCII up to about 30 lines each
- your reading of the root cause in 3–5 sentences, naming the code line(s) that cause it. Label it as a hypothesis where it is one.
- `git status --porcelain` (must be empty) and `git log -1`

When you have reported, stop.
