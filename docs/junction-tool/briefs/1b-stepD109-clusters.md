# Brief 1b step D109: one doubled section per crossing cluster

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- **Repo:** `/home/pete/OpenTTD`, branch `feature/junction-tool`, head `{HEAD}`. The Release suite passes.
- **Decisions:** D109 (read it), D108, D100, D103 and D105.
- **The problem:**
  - With D108 the E→S split must sit at least the prio coverage + sd + 1 tiles upstream of the N→E merge.
  - At `prio_tl` 2 and 3, the W-row crossing zone (E→S and N→E pieces on the W row) spans too far for one doubled-bridge section (`spec.bridge_span_ok(n)` fails, or h_w/h_e fall out of range), so no candidate builds.
  - No existing test generates at prio ≥ 2, so nothing was hidden. The new test in §4 is the guard.
- **Scratchpad:** `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/d109/`
- **Don't run any `tools/ottd` command.**

## 2. Read first

- `src/junction_generator.cpp`, `BuildJunctionCandidate`:
  - the zone collection (pass 1);
  - h_w, h_e and n;
  - pass 2's style 0 lane construction (lane B's p/q/r/s search, lane A as its mirror, `merge_x`, `under_spans`, `lane_b_axis_x`, `lane_b_max_x`, `nw_max_for`);
  - style 1.
- `src/tests/junction_generator.cpp`: the `[.d109]` tests and the lane tests.

## 3. Changes (`BuildJunctionCandidate`, pass 2, style 0 only)

1. **Clusters.**
   - Sort the zone's x values (distinct) into runs: start a new cluster where the gap between consecutive x values exceeds `GAP = 2·bridge_margin + 8` tiles. That leaves room for one section's lane merge and the next one's lane split.
   - Each cluster c has `h_w_c = min_c − margin` and `h_e_c = max_c + margin`.
   - **If there is exactly one cluster and its span is OK, behave exactly as today:** byte-identical results, guarded by test 2 below.
   - **Otherwise,** if any cluster's span fails `bridge_span_ok`, or two consecutive sections' heads are closer than `GAP − 2·margin` apart (overlap), `return std::nullopt`.
   - Also try a single combined section first. If that span is OK, use it, even with several clusters; split only when the single span fails. That keeps today's results wherever they work, and splits only when needed.
2. **Multi-section lane B** (westbound from `tile(63, w_row)`, sections in order east to west):
   - `p·F + "RFL" + q·F + span_1 + "F" + "LFR"`, then for each further section `t_c·F + "RFL" + q_c·F + span_c + "F" + "LFR"`, and finally `s·F` to x = 0.
   - Find each section's counts with the same rasterize-and-check method as today: near head at `(h_e_c, b_row)`, exactly one plain axis piece between the `RFL` and the head, and far head at `(h_w_c, b_row)`.
   - Lane A is the full program's mirror (L and R swapped), as today, and must have its heads at `(h_e_c, w_row−1)` and `(h_w_c, w_row−1)`.
3. **Derived values:**
   - `merge_x` is the **westmost** section's lane merge, which is what the N→W window needs.
   - `lane_b_axis_x` and `lane_b_max_x` (the W→S start) come from the **eastmost** section, as today.
   - `under_spans` covers every section's span tiles on both lane rows.
4. **Style 1 (parallelogram)** with more than one cluster: `return std::nullopt` when the single combined span fails. Multi-section parallelograms are out of scope.

## 4. Tests

1. Check that "GenerateJunction: a longer prio covers two train lengths" (prio 2) is green both before and after. Report what it does at head.
2. **Golden at prio 1:** run `[.golden]` before and after, and `cmp`. They must be identical, because single sections are unchanged.
3. **New: "long prios use one doubled section per crossing cluster".**
   - For the test spec with `prio_tl` = 3, `GenerateJunction` gives a clean layout.
   - Count its ML bridges. With two sections there are 4: 2 lanes × 2 sections. Assert ≥ 2. Report the count, and every bridge's middle-tile length.
4. **`[.gen]` counts** at `JUNCTION_GEN_PRIO` 1, 2, 3 and 5 (5 is about the solver's 21 tiles at TL4). Report them, and the final ASCII and size at prio 3 and 5.
   - If prio 5 builds 0, report the failing step, the same way the D3d diagnosis did (temporary prints, then removed). That's a finding, not a stop.
5. Release suite, all passing.

## 5. Regenerate the circuits
`JUNCTION_GEN_PRIO=n JUNCTION_CIRCUIT=tools/ottd/layouts/<file> build-rel/openttd_test "[.circuit]"`:
- n=1: `circuit-LR-TL4.json`
- n=2: `-prio2.json`
- n=3: `-prio3.json`

Add n=5: `circuit-LR-TL4-prio5.json`, if prio 5 generates. Commit them.

## 6. Do not

- change single-section behaviour (golden identical)
- implement multi-section parallelograms
- change fences, windows, prio placement or the checker

## 7. Stop conditions

- The golden files differ.
- Prio 2 or 3 still gives no clean candidate.
- Anything not covered.

## 8. Gates

1. `cmake --build build-rel --target openttd openttd_test > d109/rel.log 2>&1`, then the Release suite, all passing.
2. `cmake --build build --target openttd openttd_test > d109/dbg.log 2>&1`: no errors or warnings in the files you touched. Build only.

Commit message: `Change: Junction doubled bridges come one section per crossing cluster`. Signing is off; end the message with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 9. Report

- commit
- the cmp result
- the prio 2 test's behaviour
- `[.gen]` counts
- the prio 3 and 5 layouts and the bridge counts and lengths
- suite totals
- stops
- `git status --porcelain` and `git log -1`

When you have reported, stop.
