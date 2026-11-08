# Brief 1c step M2: prio length solver (D102)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- **Repo:** `/home/pete/OpenTTD`, branch `feature/junction-tool`, head `{HEAD}`. The Release suite passes.
- **Decisions:** D101 and D102 in `docs/junction-tool/DECISIONS.md` (read both), and D34 and D98.
- **Exists:** `src/junction_train_model.h/.cpp`, which has `JunctionTrainParams`, `JunctionTrainState`, `StepJunctionTrain`, `JunctionTicksToCover` and the progress constants. It was merged as 8a4d67eb26.
- **Scratchpad:** `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/m2/`
- **Don't run any `tools/ottd` command:** a rig run is live.

## 2. Read first

- `src/junction_train_model.h/.cpp`, all of it.
- `src/tests/junction_train_model.cpp`, all of it.
- `src/vehicle_base.h`: `GetAdvanceSpeed`.

## 3. Changes

### 3.1 `junction_train_model.h/.cpp`

```cpp
/**
 * Shortest prio coverage, in whole tiles of straight track, so that an ML train arriving at design_speed never comes within one block of a joiner that starts from rest at the merge signal (D102).
 * @param joiner The joining train, loaded (D101).
 * @param design_speed ML speed in cur_speed units: min(train max, rail type max) (D101).
 * @param tl Train length in tiles.
 * @param sd Signal spacing in tiles (one block).
 * @return L = ceil(max over t of (ML distance − joiner distance), in tiles) + tl + sd.
 */
uint32_t JunctionPrioLength(const JunctionTrainParams &joiner, uint16_t design_speed, uint8_t tl, uint8_t sd);
```

- Step the joiner from rest with `StepJunctionTrain`. Its `max_speed` is `min(joiner.max_speed, design_speed)`.
- The ML train advances at constant `design_speed`. Its progress per tick is `GetAdvanceSpeed(design_speed)`; reuse the same expression the model uses.
- Track `deficit = ml_progress − joiner_distance` each tick, keeping the maximum.
- Stop when the joiner's speed ≥ the design speed (the deficit can't grow after that), or after 1,000,000 ticks.
- Convert the maximum deficit to tiles with `JunctionProgressPerStraightTile()`, rounding up. Then add tl + sd.

### 3.2 Test tags
Add the tag `[junction]` to the five existing train-model test cases, so they run with the junction filter.

## 4. Tests (in `src/tests/junction_train_model.cpp`); red first

The params, as in the existing tests: {power 1000 hp, max_te 200000, mass 200 t, air_drag 50, max_speed 160}.

1. **Monotonic in mass:** `JunctionPrioLength` with mass 400 > mass 200 > mass 100, at design 160, tl 4, sd 2.
2. **Monotonic in design speed:** at mass 200, design 160 > design 100 > design 60.
3. **Floor:** a train that reaches design speed almost at once (power 100000 hp, max_te 10000000, mass 50) gives exactly tl + sd + (0 or 1).
4. **A hand-check, to report:** compute and print (WARN) `JunctionPrioLength` for the params above at design 160, tl 4, sd 2. Report the number. No assertion beyond being ≥ tl + sd.
5. **Red:** a link error before the function exists is acceptable red; say so.

## 5. Do not

- wire it into the generator or the GUI (later steps)
- model curves, slopes or the original acceleration model here (later)

## 6. Stop conditions

- Anything not covered.

## 7. Gates

1. `cmake --build build-rel --target openttd openttd_test > m2/rel.log 2>&1`, then the Release suite, all passing.
2. `cmake --build build --target openttd openttd_test > m2/dbg.log 2>&1`: no errors or warnings in the files you touched.

Commit message: `Add: Junction prio length solver`. Signing is off; end the message with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 8. Report

- commit
- red evidence
- suite totals
- the hand-check number
- stops
- `git status --porcelain` and `git log -1`

When you have reported, stop.
