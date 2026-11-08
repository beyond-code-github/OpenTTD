# Brief 1c step PL: prio length as a multiple of TL (stopgap experiment)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- **Repo:** `/home/pete/OpenTTD`, branch `feature/junction-tool`, head `{HEAD}`. The Release suite passes.
- **Why:** the engineer, watching the rig: "straight away I see that the prios are not long enough for these trains".
  - Today every prio covers 2·TL units (one train length) upstream of its merge.
  - The proper fix is the 1c acceleration model (D31). This step adds a knob, so the rig can compare prio lengths of 1, 2 and 3 TL now.
- **Scratchpad:** `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/pl/`
- **Don't touch the rig server** (a traffic run is in progress there).

## 2. Read first

- `src/junction_generator.h`: `JunctionSpec`.
- `src/junction_generator.cpp`: `MakeTestJunctionSpec`, and `nw_max_x` (about l.375).
- `src/junction_signal_pass.cpp`: the three coverage loops `covered < 2u * ctx.spec.tl` (about l.317, 437 and 476).
- `src/junction_rules.cpp`: the G9 coverage loop (about l.1198).
- `src/junction_game.cpp`: `MakeJunctionSpec`.
- `src/tests/junction_design.cpp`: the env handling of `[.gen]` (`JUNCTION_GEN_*`) and `[.circuit]` (`JUNCTION_CIRCUIT`).

## 3. Changes

1. `JunctionSpec` gets `uint8_t prio_tl = 1; ///< Prio coverage upstream of a merge, in train lengths (stopgap until the 1c model, D31).`
   - `MakeTestJunctionSpec` and `MakeJunctionSpec` leave it at 1. Set it explicitly to 1 where every other field is set, so the struct initialisation stays complete.
2. In the four prio coverage loops (three in the signal pass, one in the checker), the bound `2u * ctx.spec.tl` becomes `2u * ctx.spec.tl * ctx.spec.prio_tl`.
   - Check that each loop is a prio coverage loop and not a bay or waiting-room one. The waiting room (`junction_rules.cpp` l.794), bays (l.798, signal pass l.107) and l.578 stay as they are.
   - If any of the four turns out not to be prio coverage, stop and report.
3. In `BuildJunctionCandidate`, `nw_max_x` uses `spec.tl * spec.prio_tl` instead of `spec.tl`. The N→W merge must sit at least the prio's coverage downstream of the lane merge, so the prio stays on single track (D85).
4. The env `JUNCTION_GEN_PRIO` (a digit, read as the others are, `e[0]-'0'`) sets `spec.prio_tl` in both `[.gen]` and `[.circuit]`.
   - `[.circuit]` may not read the `JUNCTION_GEN_*` variables today. If not, add just this one there, the same way.

## 4. Tests

1. **New:** "a longer prio covers two train lengths". For `MakeTestJunctionSpec()` with `prio_tl = 2`:
   - if `GenerateJunction` gives a layout, `CheckJunctionRules` is empty;
   - and for each ML merge's prio, the distance from the merge to the farthest upstream prio join is ≥ 2·TL tiles − 1.
   - Measure it on the layout: find the joiner merges, then walk the prio row's join tiles. If you can't measure it without a new helper of more than about 20 lines, check only the checker cleanliness and the layout's width growing versus prio 1, and say so.
   - If `GenerateJunction` gives NoLayout at prio 2, the test asserts that and WARNs. Report it: that is the finding, not a stop.
2. **Golden:** `[.golden]` with prio 1, before and after, must be identical (`cmp`).
3. Release suite, all passing.

## 5. Experiment (report the numbers)

1. Run `[.gen]` with `JUNCTION_GEN_PRIO` = 1, 2, 3 (test spec, `JUNCTION_GEN_SKIP` unset). For each, report the "built N candidates" line, the failure histogram, and the final layout's ASCII and size.
2. For each prio value that generates, write a circuit: `JUNCTION_GEN_PRIO=n JUNCTION_CIRCUIT=tools/ottd/layouts/circuit-LR-TL4-prio{n}.json build-rel/openttd_test "[.circuit]"`, for n = 2 and 3.
   - Commit those JSON files.
   - Don't overwrite `circuit-LR-TL4.json`.

## 6. Do not

- touch the rig server or run any `ottd` command
- change bays, the waiting room or any other rule
- change the generator beyond `nw_max_x`

## 7. Stop conditions

- The golden files differ at prio 1.
- One of the four loops isn't prio coverage.
- Anything not covered.

## 8. Gates

1. `cmake --build build-rel --target openttd openttd_test > pl/rel.log 2>&1`, then the Release suite, all passing.
2. `cmake --build build --target openttd openttd_test > pl/dbg.log 2>&1`: no errors or warnings in the files you touched. **Don't run the Debug binary or the rig.**

Commit message: `Add: Junction prio length as a multiple of train length`. Signing is off; end the message with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 9. Report

- commit
- red evidence
- suite totals
- cmp result
- the experiment numbers and ASCII per prio value
- which circuit files were written
- stops
- `git status --porcelain` and `git log -1`

When you have reported, stop.
