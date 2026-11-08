# Brief 1b step S1: signal distance as drag-signalling (D97)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- **Repo:** `/home/pete/OpenTTD`, branch `feature/junction-tool`, head `{HEAD}`. The Release suite passes (`build-rel/openttd_test`, about 22 s).
- **Decision:** D97 in `docs/junction-tool/DECISIONS.md`. Read it. The signal pass's phase 3 (`FillSignals`) spreads signals evenly with gaps of at most 2·SD units, so it gives a 3-tile pitch at SD 4. The engineer wants the pitch that drag-signalling gives.
- **Scratchpad:** `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/s1/` (create it).

## 2. Read first

- `src/rail_cmd.cpp` l.1277–1460, `CmdSignalTrackHelper`: the minimise-gaps branch (l.1366–1393) and the counter increments. That is the behaviour to copy. Note:
  - logical length is 192 (`TILE_AXIAL_DISTANCE`) for an axis piece and 128 (`TILE_CORNER_DISTANCE`) for a diagonal half piece;
  - density = SD × 192.
- `src/junction_signal_pass.cpp`, all of it, especially `FillSignals` (l.153–190) and `PlaceBaySignals` (for how it walks backward and skips illegal pieces).
- `src/junction_rules.cpp` l.732–746 (rule 4, spacing).
- `src/junction_route.h`, `JunctionPiece`. `units` is 2 for an axis piece **or a bridge piece**, and 1 for a diagonal half.
- `src/tests/junction_signal_pass.cpp`: the helpers `R`, `BuildRoutes`, `Frame` and `FindSignal`, and the test "fill keeps gaps within twice the spacing" (l.167).

## 3. Definitions

- **`weight(piece)`** = 192 if `piece.units == 2`, else 128.
- **`legal(k)`**: the same test `FillSignals` uses today, `k != 0 && k + 1 != pieces.size() && IsLegalSignalPiece(layout, pieces[k])`.
- **Anchors:** the same as today. A piece is an anchor if it is the path's first piece, its last piece, or a signalled piece.
- **Distance from `last` to `k` (k < last, walking upstream):** `dist(k) = Σ weight(pieces[m])` for m = k+1 .. last.

## 4. Changes

### 4.1 `FillSignals` (rewrite its body; keep its name and signature)
- For each path in `PathOrder(ctx)`, for each pair of consecutive anchors a < b, walk from b upstream. With `density = spec.sd * 192`, `last = b`, and `suitable` empty:

  ```
  for k = b-1 down to a+1:
      if dist(k) > density and suitable has a value:      // overshoot: use the last good spot
          place at suitable; last = suitable; clear suitable
      if dist(k) == density:
          if legal(k): place at k; last = k; clear suitable
      else if dist(k) < density and legal(k):
          suitable = k
  after the loop: if dist(a) > density and suitable has a value: place at suitable
  ```

- `dist` is relative to the current `last`. Keep a running sum, and recompute it when `last` moves.
- **Read this carefully: it mirrors rail_cmd.cpp.** After an overshoot placement, the same k is tested against the new `last` in the same iteration.
- In rail_cmd the "suitable" test happens in the else branch of `==`, including when dist > density and nothing suitable existed. Copy that. If that branch doesn't map cleanly onto this pseudocode, follow rail_cmd's structure and report the difference.
- Use `PlaceSignal(layout, pieces[k], spec.signal_type)` as today.
- Update the doc comment: `/** Phase 3: fill the gaps between anchors as drag-signalling would (D97), walking upstream from each downstream anchor. */`.

### 4.2 Checker rule 4 (`junction_rules.cpp`)
- `d` accumulates `weight(piece)` instead of `piece.units`.
- The bound is `ctx.spec.sd * 192u` instead of `2u * ctx.spec.sd`.
- Everything else stays.
- Put `weight` as a `static` function in each of the two .cpp files, or in `junction_rules_internal.h` as an inline function `JunctionSignalWeight(const JunctionPiece &)`. **Use the header**: one definition.

## 5. Tests (`src/tests/junction_signal_pass.cpp`); write them first and record red

1. **Replace the test "fill keeps gaps within twice the spacing"** with "fill pitch matches drag-signalling on a straight". This is the 24-tile E row of `Frame(5, false, false)`, the EIn→EOut path.
   - At SD 2 (the test spec), the fill signals are at exactly x = 21, 19, 17, …, 1, and no other piece on that path is signalled. Use `FindSignal` for each, and count the signalled pieces.
   - Then at `spec.sd = 4`: signals at exactly x = 19, 15, 11, 7, 3 and nowhere else on the path.
   - The trackdir of the E row is the route's (`Trackdir::X_SW`).
   - If phase 1 places required signals on that row, adjust the expectation to what drag gives with those as anchors, and explain in a comment.
   - Red: the old fill puts signals elsewhere, or a different number.
2. **"fill on a diagonal uses 128 per half piece":** build a path containing a diagonal run of at least 12 half pieces between two anchors. For example, give `Frame` a `row0` program that dips: some F, an R or L, a run of diagonal F, then the reverse turns and F back to x = 23 on row 0. Work out a program that rasterises without error (`BuildRoutes` REQUIREs that).
   - At SD 2 (density 384), consecutive fill signals inside the diagonal run are exactly 3 half pieces apart (3 × 128 = 384).
   - Red: the old fill gives 4.
   - If you can't get a dip to rasterise within 3 tries, stop and report the programs you tried.
3. **Checker:** a hand-built case where a straight gap of SD+1 axis pieces with a legal piece inside is flagged ("signals too far apart"), and SD pieces is not. Use the E row with signals placed by hand via `PlaceSignal`-equivalent code already in the test file. If there is none, add signals with `layout.signals.push_back` exactly as other tests do.
4. **Existing tests:** run the Release suite.
   - Generator tests (clean candidates, golden, deterministic) may change results, because the fill moved.
   - **`[.golden]` is expected to change:** don't update any golden file; it is only compared inside optimisation steps.
   - If a test that asserts `CheckJunctionRules(...).empty()` fails, report which and stop.
   - Test-spec ASCII changes are fine.

## 6. Do not

- add fixed-distance mode (it's deferred)
- change phases 0, 1, 2, 3b or 4
- change any other checker rule

## 7. Stop conditions

- A clean-layout test fails.
- The rail_cmd behaviour can't be expressed with the anchors as defined.
- Anything not covered.

## 8. Gates

1. `cmake --build build-rel --target openttd openttd_test > s1/build.log 2>&1`, then `build-rel/openttd_test > s1/suite.log 2>&1`: all passing.
2. `cmake --build build --target openttd openttd_test > s1/dbg.log 2>&1`: no errors or warnings in the files you touched. Don't run the Debug suite.
3. Print the `[.gen]` best layout's ASCII for the test spec (`build-rel/openttd_test "[.gen]"`, with `JUNCTION_GEN_SKIP` unset; only the final `GenerateJunction` block) at SD 2 and with `JUNCTION_GEN_SD=4`.

Commit message: `Change: Junction signal fill matches drag-signalling`. Signing is off; end the message with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 9. Report

- commit
- red evidence per test
- suite totals
- the two ASCII blocks
- any test expectations you changed, with before and after
- stops
- `git status --porcelain` and `git log -1`

When you have reported, stop.
