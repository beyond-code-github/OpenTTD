# Brief TUNE-footprint: tightest safe joiner positions (drop the fixed margins)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- **Repo:** `/home/pete/OpenTTD`, branch `feature/junction-tool`, head `{HEAD}`. The Release suite passes.
- **The engineer:** "The build template is still leaving a large gap between the end of the bridge and the prio".
- **Cause:** two fixed margins were added as worst-case guesses:
  - W side, in `nw_max_for` (`junction_generator.cpp` about l.325): `− static_cast<int>(spec.sd + 1)` (D107 3b);
  - E side, in the N→E goal window: `x_s + cover + spec.sd + 2` (D108).
  The checker now proves prio safety directly (G9b prio reach; G9c no split inside prio coverage), so these margins can go. Let the checker reject unsafe positions, and let the score pick the tightest safe layout.
- **Scratchpad:** `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/tune/`
- **Don't run any `tools/ottd` command.**

## 2. Changes (`junction_generator.cpp` only)

1. **W side:** in `nw_max_for`, remove the `− static_cast<int>(spec.sd + 1)` term. Keep everything else: `xI − 2`, `merge − 1`, the coverage and `joiner_slack`.
2. **E side:** the N→E goal minimum becomes `std::max(xO, x_s + cover + 1 + params.joiner_slack)`, replacing `x_s + cover + spec.sd + 2`. The slack sweep 0..3 now also loosens the E side.
3. Nothing else changes: the slack range, the scoring, the checker and the signal pass all stay.

## 3. Tests

1. **Before you change anything,** record the test spec's `GenerateJunction` size and its layout ASCII (`build-rel/openttd_test "[.gen]"`, final block) in `tune/before.txt`.
2. **After the change:**
   - `GenerateJunction(MakeTestJunctionSpec())` must be fully rule-clean (`CheckJunctionRules` empty, which includes G9b and G9c);
   - its width must be ≤ the width before. Add this as a test with the before-width as a literal, and a comment citing this brief.
   - Report both sizes and both ASCII blocks.
3. **Measure the gap** on both layouts, before and after: the number of tiles on the W row between the westmost ML bridge's far head and the W prio's easternmost row tile, and the same on the E side between the E→S split and the E prio's westmost row tile. Report them.
4. **`[.gen]` counts** at prio 1 and 2. Report them.
5. Release suite, all passing. `[.golden]` will change; don't compare it. Tests asserting specific sizes may need updating; report each one you change, with before and after.

## 4. Regenerate the circuits
`JUNCTION_GEN_PRIO=1 JUNCTION_CIRCUIT=tools/ottd/layouts/circuit-LR-TL4.json build-rel/openttd_test "[.circuit]"`. Commit it.

## 5. Do not

- change the checker, the signal pass, the slack range or the lane construction

## 6. Stop conditions

- The test spec gets no clean candidate.
- The layout gets wider.
- Anything not covered.

## 7. Gates

1. `cmake --build build-rel --target openttd openttd_test > tune/rel.log 2>&1`, then the Release suite, all passing.
2. `cmake --build build --target openttd openttd_test > tune/dbg.log 2>&1`: no errors or warnings in the files you touched. Build only.

Commit message: `Change: Junction joiners sit as close as the prio rules allow`. Signing is off; end the message with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 8. Report

- commit
- sizes, ASCII and gaps, before and after
- `[.gen]` counts
- tests changed
- suite totals
- stops
- `git status --porcelain` and `git log -1`

When you have reported, stop.
