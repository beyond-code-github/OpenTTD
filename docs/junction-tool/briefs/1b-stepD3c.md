# Brief 1b step D3c: early branch swap as a sweep option (D91)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- **Repo:** `/home/pete/OpenTTD`, branch `feature/junction-tool`, head `{HEAD}`.
- **Decisions to read:** D90 (skeleton) and D91 (swap) in `DECISIONS.md`.
- **Code:** `BuildJunctionCandidate` and `GenerateJunction` in `src/junction_generator.cpp`.
- **Diagnostics:** the hidden `[.gen]` test in `src/tests/junction_design.cpp`. It takes `JUNCTION_GEN_TL/SD/G/B` overrides and prints per-candidate violation counts plus a histogram. Use it to diagnose.

## 2. Read first

- `src/junction_generator.cpp` (all)
- `src/junction_router.h`
- the `[.gen]` test

## 3. Changes

1. **`JunctionCandidateParams`:** add `bool swap = false; ///< D91: the branch pair crosses over on the leg, so at the hub inbound is east and outbound west.`
2. **In `BuildJunctionCandidate`, when `swap` is set:**
   - **Hub columns:** S-stub column xS = xI − branch_gap − 1, N-stub column xN = xI. Without swap these are xS = xO and xN = xI.
   - **Legs below the hub.** The hub's stubs end at row yh = yb − LEG, with LEG = 2·(branch_gap+1) + 2·CL_tiles + 4, where CL_tiles = ceil(curve_spacing / 2).
     - The S stub spans rows yh−STUB .. yh.
     - **S leg:** route from the S stub's bottom piece `(xS, yh)` `Y_SE` to the goal `(xO, yb)` `Y_SE`, with bounds rows yh..yb, columns xS−2 .. xO+2.
     - **N leg:** route from `(xI, yb)` `Y_NW`. The start piece is pre-laid as one Y track, as the N stub was. The goal is `(xN, yh)` `Y_NW`. Same bounds. It will bridge over the S leg.
     - The N stub spans rows yh−STUB .. yh, and N→E/N→W start from it as before.
     - If either leg fails, return `std::nullopt`.
   - **Hub routing is unchanged except:**

     | | Without swap | With swap |
     |---|---|---|
     | E→S goal | `(xO, ys)` | the top of the S stub at column xS |
     | W→S goals | S stub side pieces at xO | S stub side pieces at xS |
     | E→S window | x ≤ xI−2 | x ≤ xS−2 |
     | N→E window | x ≥ xO | x ≥ xN+2 |
     | W→S window | x ≥ xO+2 | x ≥ xN+2 |
     | E→S fence | column xI above row g+3 | column xN above row g+3 (it keeps E→S west of N) |

     The branch fence for W→S and N→W stays as it is.
   - **Bridges:** the router may bridge N→W over E→S's stub and W→S over N→E. Nothing extra is needed for that.
3. **`GenerateJunction`:** sweep swap ∈ {false, true} for every (height, crossing_cost, slack). For swap, the height range runs to ml_gap+20+LEG. Early stop and scoring stay unchanged, so the scorer picks.
4. **The `[.gen]` test:** add a `JUNCTION_GEN_SWAP=1` override that sweeps only swap candidates.

## 4. Tests

1. **Swap candidates exist:** for `MakeTestJunctionSpec()`, at least one swap candidate is clean (assert it). Log its params and ASCII.
2. **The existing outer and determinism tests** still pass.
3. **Report the comparison** on the test spec, swap against no swap:
   - clean counts;
   - the best score of each (cost and open rectangle);
   - bounding-box sizes;
   - which one `GenerateJunction` picks.

## 5. Do not

- change the router, the signal pass or the checker
- change the no-swap path's behaviour

## 6. Stop conditions

- **No clean swap candidate:** stop. Report the failing-step table and the histogram, plus the ASCII and violations of the fewest-violation candidate. Use the `[.gen]` style; temporary prints are allowed but must be removed before any commit.
- Anything not covered.

## 7. Gates

1. `cmake --build build --target openttd openttd_test > /tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/d3c_build.log 2>&1`. There must be no errors or warnings in junction files.
2. Full suite. The baseline is 211 cases, all passing. Report the wall time and `GenerateJunction`'s time on the test spec.

Commit message: `Add: Junction generator tries an early branch swap`. Signing is off; end the message with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 8. Report

- commit
- red evidence
- the comparison table
- the swap layout's ASCII
- suite totals and times
- stops
- `git status --porcelain` and `git log -1`

When you have reported, stop.
