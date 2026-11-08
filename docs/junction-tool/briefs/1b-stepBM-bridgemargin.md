# Brief 1b step BM: sweep the doubled-bridge head margin (D103)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- **Repo:** `/home/pete/OpenTTD`, branch `feature/junction-tool`, head `{HEAD}`. The Release suite passes.
- **Decision:** D103 (1) in `docs/junction-tool/DECISIONS.md`; read it, and D90 and D100.
- **Today:** in `BuildJunctionCandidate`, `h_w = min(zone) − 3` and `h_e = max(zone) + 3`. The engineer sees the bridges as longer than needed, and long bridges cost throughput.
- **Scratchpad:** `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/bm/`
- **Don't run any `tools/ottd` command:** a rig run is live.

## 2. Read first

- `src/junction_generator.h`: `JunctionCandidateParams`.
- `src/junction_generator.cpp`: `BuildJunctionCandidate` (the zone, h_w and h_e), `GenerateJunction` (the sweep, its parallel per-height batches and its early stop) and `BuildJunctionLibrary`.
- `src/tests/junction_generator.cpp`: the tests that construct `JunctionCandidateParams` directly. Grep for it.

## 3. Changes

1. `JunctionCandidateParams` gets `uint8_t bridge_margin = 3; ///< Bridge heads sit this many tiles beyond the crossing set (D103).`
2. `BuildJunctionCandidate`: `h_w = min(zone) − params.bridge_margin`, and `h_e = max(zone) + params.bridge_margin`.
3. **`GenerateJunction` and `BuildJunctionLibrary`:**
   - The per-height batch becomes 2 crossing costs × 4 slacks × 3 margins (1, 2, 3) = 24 params.
   - Order: crossing cost outer, then slack, then margin, inner, in the order 1, 2, 3.
   - Selection, early stop and the parallel run are unchanged otherwise. Results are still walked in this order.
4. Any test that builds params by aggregate initialisation keeps compiling. The default of 3 keeps its old meaning.

## 4. Tests

1. **New: "doubled bridges are no longer than needed".**
   - For the test spec, `GenerateJunction` gives a layout whose doubled ML bridges have a span (middle-tile count) ≤ the span with `bridge_margin` 3. Find that by building the same winning height, crossing cost and slack with margin 3 via `BuildJunctionCandidate` if that is easy; otherwise compare against the committed golden's bridge length.
   - Also report both spans.
   - Red: today they're equal, because margin 3 is the only option. If your test can't go red this way, make it assert that the chosen layout's bridges have middle count ≤ max(C) − min(C) + 2·2 − 1 (margin ≤ 2). Say which form you used.
2. Generator outer, determinism, clean-candidate and library-determinism tests stay green. `[.golden]` will change; don't compare it.
3. **Timing:** report the S1 time in `build-prof` (rebuild it first): `build-prof/openttd_test "GenerateJunction gives a rule-clean L_R T"`, median of 3. The P4 baseline was 85 ms.
4. Release suite, all passing.

## 5. Regenerate the circuits
`JUNCTION_GEN_PRIO=n JUNCTION_CIRCUIT=tools/ottd/layouts/<file> build-rel/openttd_test "[.circuit]"`:
- n=1: `circuit-LR-TL4.json`
- n=2: `-prio2.json`
- n=3: `-prio3.json`

Commit them, and report each circuit's bridge spans.

## 6. Do not

- change the zone computation, the lanes' shape, the fences or the scoring
- add triple bridges (a 1c follow-on)

## 7. Stop conditions

- No clean candidate for the test spec.
- Anything not covered.

## 8. Gates

1. `cmake --build build-rel --target openttd openttd_test > bm/rel.log 2>&1`, then the Release suite, all passing.
2. `cmake --build build --target openttd openttd_test > bm/dbg.log 2>&1`: no errors or warnings in the files you touched.

Commit message: `Change: Junction generator sweeps the bridge head margin`. Signing is off; end the message with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 9. Report

- commit
- red evidence
- suite totals
- spans before and after for the test spec, and per circuit
- the S1 time
- stops
- `git status --porcelain` and `git log -1`

When you have reported, stop.
