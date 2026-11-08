# Brief GEOM-d110: tight split and bridge geometry at the doubled-bridge entry

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- **Repo:** `/home/pete/OpenTTD`, branch `feature/junction-tool`, head `{HEAD}`. The Release suite passes.
- **Decision:** D110 in `docs/junction-tool/DECISIONS.md` (read it). Also the principle in memory: margins come from the rules and the checker, never hedging.
- **Scratchpad:** `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/geom/`
- **Don't run any `tools/ottd` command.** Another agent works in a separate worktree; ignore it.

## 2. Read first

- `src/junction_generator.cpp`:
  - `nw_max_for` (about l.325);
  - the W→S start pieces (about l.288, `PlainRowPieces(l, w_row, Trackdir::X_NE, xO + 2, ...)`, plus the lane B starts after it);
  - lane B's search with `plain_axis != 1` (about l.371);
  - lane A for style 1 (parallelogram), with its own "F" before the span;
  - where the lane split x (the tile where lanes A and B diverge from the W track) is known in pass 2.
- `src/junction_rules.cpp`: how other ML rules walk paths, and `SwitchEventsOf`.

## 3. Changes

1. **W side prio:** `nw_max_for` becomes `std::min(xI − 2, merge − static_cast<int>(spec.tl * spec.prio_tl) − params.joiner_slack)`, removing the `− 1`.
2. **Lane head offset:**
   - Lane B's search accepts `plain_axis == 0` as well as `1`, trying 0 first.
   - Lane A (style 0) is the mirror, so it follows automatically.
   - For style 1, lane A's program `k·F + span + "F" + "RFL"` also tries without the `"F"` after the span first, and the lane B shift-back the same way.
   - Keep the first variant that rasterizes with its heads in place. Choose by trying 0 first in the same loop; don't add a sweep parameter.
3. **W→S start positions:**
   - In pass 2, let `x_ls` be the lane split's x on the W row.
   - W→S starts on the W row are limited to the two positions east of the lane split: the piece at `x_ls + 1` (an adjacent split) and the piece at `x_ls + 2` (one tile between, for a signal).
   - Keep the existing lane B starts, so W→S may still leave from lane B.
   - In pass 1, where there are no lanes yet, starts are unchanged.
4. **Checker rule:**
   - In `junction_rules.cpp`, add a check to an existing ML-geometry function (`CheckGeometry` or `CheckOrder`; pick the one that already walks ML paths' switch events, and say which).
   - For each ML path (EIn→EOut, WIn→WOut, every lane), look at consecutive `SwitchEventsOf` events that are both splits, or both merges. Count the plain tiles strictly between their tiles along the path.
   - If that count is in `[2, spec.sd]`, add a violation (rule `Geometry` or `Order`, whichever function it lives in) `"splits too close: join them or leave one signal tile"` at the downstream split.

## 4. Tests; red first

1. **New checker test:** a hand-built ML with two splits 2 plain tiles apart (SD 2) is flagged. 0 apart and 1 apart are not flagged. Use the `R(...)`/`BuildRoutes` style from the signal-pass tests, or an existing ML fixture.
   - Red: before the rule exists, the 2-apart case has no such violation.
2. **The test spec's layout after the change:**
   - `CheckJunctionRules` is empty;
   - report the W-side numbers: the bridge head to W prio gap, the lane split to near-head plain tiles, and the W→S split to lane split tiles;
   - report the ASCII before and after (`[.gen]`, the final block).
3. **`[.gen]` counts at prio 1.** If 0, stop and report the least-bad violations.
4. Release suite, all passing. `[.golden]` will change. The width test from TUNE (`size.x <= 32`) must still hold.

## 5. Regenerate the circuit
`JUNCTION_GEN_PRIO=1 JUNCTION_CIRCUIT=tools/ottd/layouts/circuit-LR-TL4.json build-rel/openttd_test "[.circuit]"`. Commit it.

## 6. Do not

- change the prio shape, the signal pass, or E-side windows
- add sweep parameters

## 7. Stop conditions

- No clean candidate at prio 1.
- Anything not covered.

## 8. Gates

1. `cmake --build build-rel --target openttd openttd_test > geom/rel.log 2>&1`, then the Release suite, all passing.
2. `cmake --build build --target openttd openttd_test > geom/dbg.log 2>&1`: no errors or warnings in the files you touched. Build only.

Commit message: `Change: Junction doubled-bridge entry uses tight split and head spacing`. Signing is off; end the message with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. Use "split", never "switch", in user-visible text.

## 9. Report

- commit
- red evidence
- which checker function the rule went into
- the W-side numbers and ASCII, before and after
- `[.gen]` counts
- suite totals
- stops
- `git status --porcelain` and `git log -1`

When you have reported, stop.
