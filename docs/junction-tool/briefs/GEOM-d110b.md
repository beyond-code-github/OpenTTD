# Brief GEOM-d110b: SL split first with a signal gap, and a tight shift-back (D110 parts 3 and 4)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- **Repo:** `/home/pete/OpenTTD`, branch `feature/junction-tool`, head `{HEAD}`. The Release suite passes.
- **Decision:** D110 in `docs/junction-tool/DECISIONS.md`, **parts 3 and 4 only**; read the corrected text carefully. Also the memory principle: margins come from the rules and the checker, never hedging.
- **Words:**
  - "SL split" is where W→S leaves the W mainline.
  - "Lane split" is where the W mainline divides into the two doubled-bridge lanes.
  - In the canonical frame the W mainline runs westbound (−x), so "upstream" means larger x.
- **Today:**
  - W→S may start on the W row (`PlainRowPieces(l, w_row, X_NE, xO+2, ...)`, about l.288) **and** from lane B after the lane split (about l.289).
  - The parallelogram style (lane style 1) puts its shift-back at a fixed `x_sb = nw_max_x − 6` (about l.356), with N→W goals starting at `x_sb + 3`.
- **Scratchpad:** `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/geom2/`
- **Don't run any `tools/ottd` command.**

## 2. Read first

- `src/junction_generator.cpp`, `BuildJunctionCandidate`:
  - the `route_movements` lambda;
  - pass 2's lane construction for both styles;
  - where the lane split x is known;
  - `x_sb`;
  - the N→W goals (`nw_min_x`).
- `src/junction_rules.cpp`: an existing function that walks ML paths' switch events (`CheckGeometry` or `CheckOrder`), and `SwitchEventsOf`.

## 3. Changes

1. **SL first, part 3, generator.** In pass 2 (lanes fixed), let `x_ls` be the W-row x of the lane split, the tile where the lanes diverge from the single W track.
   - The W→S starts become exactly one piece: the W-row piece at `x = x_ls + 2`, heading `X_NE`. That's the SL split, with one plain tile at `x_ls + 1` between it and the lane split, where a signal goes.
   - Remove the lane B starts (l.289) in pass 2.
   - Pass 1 (no lanes) is unchanged.
   - **If `x_ls + 2` isn't a plain W-row piece** (for example it's off the frame, or it's a bridge tile), `return std::nullopt`.
2. **Part 3, checker.** In the chosen ML-walking function, for each ML path, take consecutive split events (`SwitchEventsOf`, splits only; merges are handled separately and are out of scope). If they sit on **adjacent** tiles along the path, with no plain tile between them, add a violation: `"adjacent splits: make one three-way split or leave one signal tile"`.
   - A three-way split on one tile is a single event, so it passes naturally.
   - Gaps of 1 or more tiles aren't flagged by this rule.
3. **Shift-back, part 4 (style 1 only).**
   - Replace the fixed `x_sb = nw_max_x − 6` with a loop over `x_sb` from `nw_max_x − 1` down to `nw_max_x − 6`.
   - For each value, build as today (the shift-back program, the N→W goals from `x_sb + 1` instead of `x_sb + 3`, the routes and the rest of the candidate). Keep the first `x_sb` whose candidate is fully clean under `CheckJunctionRules`.
   - Wrap only the part of `BuildJunctionCandidate` that depends on `x_sb`, as a small lambda or loop. Don't restructure the rest.
   - If none is clean, return what the last attempt returned (nullopt or the unclean layout), as before.

## 4. Tests; red first

1. **Checker:** a hand-built ML with two splits on adjacent tiles is flagged, and the same with one plain tile between is not. Use the `R`/`BuildRoutes` style or an existing fixture.
   - Red: before the rule, no such violation.
2. **The test spec after the change:**
   - `CheckJunctionRules` is empty;
   - on the WIn→WOut path there is exactly one plain tile between the SL split and the lane split. Assert it.
   - Report the ASCII before and after (`[.gen]`, the final block) and the size. The width test (`size.x <= 32`) must still hold; if it fails, stop and report.
3. **Style 1:** report which `x_sb` offset the test spec's style 1 candidates pick, as a histogram over the sweep, by running `[.gen]` or a small loop in a test.
4. **`[.gen]` counts** at prio 1. If 0, stop.
5. Release suite, all passing. `[.golden]` will change.

## 5. Regenerate the circuit
`JUNCTION_GEN_PRIO=1 JUNCTION_CIRCUIT=tools/ottd/layouts/circuit-LR-TL4.json build-rel/openttd_test "[.circuit]"`. Commit it.

## 6. Do not

- touch the prio shape (D111 is a later step), the E side, or the frame
- change lane head offsets (done in 86e223962d)

## 7. Stop conditions

- No clean candidate at prio 1.
- The test spec gets wider than 32.
- Anything not covered.

## 8. Gates

1. `cmake --build build-rel --target openttd openttd_test > geom2/rel.log 2>&1`, then the Release suite, all passing.
2. `cmake --build build --target openttd openttd_test > geom2/dbg.log 2>&1`: no errors or warnings in the files you touched. Build only.

Commit message: `Change: Junction SL splits first with a signal gap, shift-back tight to the SL merge`. Signing is off; end the message with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. Use "split", never "switch", in user-visible text.

## 9. Report

- commit
- red evidence
- the ASCII and size, before and after
- the `x_sb` histogram
- `[.gen]` counts
- suite totals
- stops
- `git status --porcelain` and `git log -1`

When you have reported, stop.
