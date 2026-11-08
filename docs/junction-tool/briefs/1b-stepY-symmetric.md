# Brief 1b step Y: symmetric doubled ML bridges (D100)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- **Repo:** `/home/pete/OpenTTD`, branch `feature/junction-tool`, head `{HEAD}`. The Release suite passes.
- **Decision:** D100 in `docs/junction-tool/DECISIONS.md`; read it, and D85.
- **Today, in pass 2 of `BuildJunctionCandidate`:**
  - lane A is the straight W row with a bridge from h_e to h_w;
  - lane B leaves the W row with `RFL`, runs on `b_row` (W row + 1) with its own bridge, and comes back with `LFR`.
  - In game every train takes lane A, because lane B's 4 curves cost more.
- **After this step:** lane A is lane B's mirror. It leaves with `LFR` to the row above the W row (`w_row − 1`, the gap row next to the W track) and comes back with `RFL`, from and to the same tiles as lane B.
- **Scratchpad:** `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/y/`
- **Don't run any `tools/ottd` command.**

## 2. Read first

- `src/junction_generator.cpp`, `BuildJunctionCandidate`, from "Pass 2: fixed doubled lanes" to the second `route_movements` call (about l.300–382), plus the `route_movements` lambda and the fences it uses (about l.240–290).
- `src/junction_route.h/.cpp`: the `RasterizeJunctionRoute` move grammar (F, L, R, B<n>).
- `src/tests/junction_generator.cpp`: "BuildJunctionCandidate: the doubled lanes are equal and doubled" (about l.129).

## 3. Changes (`junction_generator.cpp`, pass 2 only)

1. **Keep the search for lane B** (p, q, r, s) exactly as it is.
2. **Lane A becomes lane B's program with every 'L' and 'R' swapped.** 'F' and 'B<n>' are unchanged. Use the same start, `tile(63, w_row)` with `Trackdir::X_NE`.
   - Rasterize it. It must have no error.
   - Its near bridge head must be at (h_e, w_row − 1), its far head at (h_w, w_row − 1), and its last piece at x = 0 on `w_row`. Otherwise `return std::nullopt`.
   - Replace the old straight `lane_a`.
3. `RasterUnits(raster_a) == RasterUnits(raster_b)` stays as a check.
4. **`under_spans`** covers x in (h_w, h_e) on rows `w_row − 1` and `b_row`, instead of `w_row` and `b_row`.
5. **The lane merge tile (`merge_x`)** keeps its definition: the W row tile where lane B's `LFR` puts a half piece. Lane A's mirror lands its half piece on the same tile. Assert that in code: find lane A's corresponding piece the same way, and `return std::nullopt` if the x differs.
6. Nothing else changes: pass 1, `lane_b_axis_x`, `nw_max_x`, the fences, the signal pass and the checker all stay.

## 4. Tests (`src/tests/junction_generator.cpp`); write the new one first and record red

1. **New: "the doubled ML lanes are mirror images".** For the test spec's generated layout, build the check context (`MakeJunctionCheckContext(layout, spec, FindJunctionPorts(layout), JunctionTMovements())`) and take the WIn→WOut paths. There must be exactly 2.
   - For each path, count its non-axis (half) pieces, and the number of times consecutive pieces change direction (turns).
   - Both counts must be equal between the two paths.
   - Red: today lane A has 0 half pieces and lane B has several.
2. **Existing "the doubled lanes are equal and doubled"** stays green. If it encodes lane A as straight, report how before changing it.
3. **Generator outer, determinism and clean-candidate tests** must stay green. If a clean-candidate test fails (no clean layout), report the least-bad violations and ASCII, then stop.
4. **`[.golden]` will change:** this step changes layouts, which is expected. Don't compare it.
5. Run `[.gen]` for the test spec, and also with `JUNCTION_GEN_TL` = 3 and 6 and `JUNCTION_GEN_B=0`. Report the candidate count and the final ASCII for the test spec.
6. Release suite, all passing.

## 5. Regenerate the circuits
`JUNCTION_GEN_PRIO=n JUNCTION_CIRCUIT=tools/ottd/layouts/<file> build-rel/openttd_test "[.circuit]"`:
- n=1: `circuit-LR-TL4.json`
- n=2: `circuit-LR-TL4-prio2.json`
- n=3: `circuit-LR-TL4-prio3.json`

All three must pass. Commit them.

## 6. Do not

- change lane B's search, the fences, the windows or anything outside pass 2
- touch the signal pass or the checker
- run the rig

## 7. Stop conditions

- No clean candidate for the test spec.
- Lane A's mirror rasterizes with an error, or its heads aren't where §3 says, for every p/q/r/s that lane B's search accepts. Report the program.
- Anything not covered.

## 8. Gates

1. `cmake --build build-rel --target openttd openttd_test > y/rel.log 2>&1`, then the Release suite, all passing.
2. `cmake --build build --target openttd openttd_test > y/dbg.log 2>&1`: no errors or warnings in the files you touched.

Commit message: `Change: Junction doubled ML bridges are symmetric`. Signing is off; end the message with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 9. Report

- commit
- red evidence
- suite totals
- the `[.gen]` numbers and the test-spec ASCII
- the circuits regenerated
- stops
- `git status --porcelain` and `git log -1`

When you have reported, stop.
