# Brief 1b step PG: parallelogram doubled bridges as a second lane style (D105)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- **Repo:** `/home/pete/OpenTTD`, branch `feature/junction-tool`, head `{HEAD}`. The Release suite passes.
- **Decisions:** D105 (read it), D100, D85 and D103.
- **Today, pass 2 of `BuildJunctionCandidate`** builds one style: the symmetric Y (D100), with lane A on `w_row − 1` and lane B on `b_row`, both mirrored and rejoining the W row.
- **This step adds style 1, the parallelogram, as a sweep option.**
- **Scratchpad:** `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/pg/`
- **Don't run any `tools/ottd` command.**

## 2. Read first

- `src/junction_generator.h`: `JunctionCandidateParams`.
- `src/junction_generator.cpp`:
  - `BuildJunctionCandidate`, all of pass 2 (about l.293–400);
  - the `route_movements` lambda (N→W goals use `PlainRowPieces(l, w_row, ...)`);
  - `GenerateJunction` and `BuildJunctionLibrary` (the sweep order).
- `src/junction_route.h/.cpp`: the move grammar.
- `src/tests/junction_generator.cpp`: "the doubled ML lanes are mirror images", "GenerateJunction reports the hub offset", "doubled bridges are no longer than needed".

## 3. Geometry of style 1

Moves are westbound from `tile(63, w_row)` with `Trackdir::X_NE`. `RFL` shifts one row down, from `w_row` to `b_row`, as lane B's existing program shows.

- **Lane B** keeps today's search for p and q, unchanged:
  - prefix `p·F + "RFL" + q·F + span`;
  - near head at `(h_e, b_row)`, with exactly one plain axis piece between `RFL` and the head.
  - After the span, lane B goes straight on `b_row`.
- **Lane A** is `k·F + span + "F" + "RFL"`. k is the smallest value that puts its near head at `(h_e, w_row)`; its far head must then be at `(h_w, w_row)`. After the `RFL`, lane A is on `b_row`.
  - **The lane merge tile** is the `b_row` tile holding lane A's last half piece (from the `L`) next to lane B's axis piece.
  - Rasterize lane A up to and including its `RFL`. Lane B runs straight on `b_row` past the merge.
- **Merge position:** `merge_x` is the x of the lane merge tile. If lane A's `RFL` doesn't land on a tile where lane B has an axis piece, `return std::nullopt`.
- **`nw_max_x`:** same formula as today, using this `merge_x`.
- **The shift-back (the shared single track):**
  - `x_sb = nw_max_x − 6`;
  - lane B's raster ends with `F`s up to the tile at `x = x_sb + 1` on `b_row`, then `"LFR"` up to `w_row`, then `F`s to `x = 0` on `w_row`.
  - Find the F counts by rasterizing and checking positions, the way lane B's search does today.
  - If `x_sb < 2`, `return std::nullopt`.
- **N→W goals:** `PlainRowPieces(l, b_row, Trackdir::X_NE, x_sb + 3, nw_max_x)` instead of the W row. Pass the row into `route_movements` (a new parameter) rather than duplicating the lambda.
- **`under_spans`:** x in (h_w, h_e) on rows `w_row` and `b_row`.
- **W→S starts** (lane B east of its bridge, `lane_b_max_x`): unchanged.
- **Equal lane units** are checked by the checker's doubled-movement rule (G1). Don't add a separate assert; candidates that fail it are filtered out as usual.

## 4. Changes

1. `JunctionCandidateParams` gets `uint8_t lane_style = 0; ///< 0 = symmetric Y (D100), 1 = parallelogram (D105).`, after `bridge_margin`.
2. Pass 2 branches on `params.lane_style`. Style 0's code is unchanged; style 1 is as in §3.
3. **`GenerateJunction` and `BuildJunctionLibrary`:** the per-height batch adds `lane_style` ∈ {0, 1} as the innermost loop, after the margin. Selection, the early stop and the parallel run are otherwise unchanged.

## 5. Tests; red first

1. **New: "a parallelogram candidate is clean".**
   - For the test spec, `BuildJunctionCandidate` with `lane_style = 1` at some height, crossing cost, slack and margin gives a layout with empty `CheckJunctionRules`.
   - Loop over the sweep space and REQUIRE at least one.
   - Report how many of the sweep's style-1 candidates are clean, against style 0.
   - Red: before the change, the field doesn't exist, which is a compile error (acceptable; say so).
2. **New: "parallelogram bridges are adjacent".** On that layout, the two ML bridges' rows differ by exactly 1.
3. **"the doubled ML lanes are mirror images"** applies to style 0. Keep it testing style 0 explicitly (`lane_style = 0`), or rename it if it uses `GenerateJunction`. Add the analogous check for style 1: the two WIn→WOut paths have equal half-piece counts and equal units.
4. **"GenerateJunction reports the hub offset":** extend its accepted cases so the hub column hitting W track on `w_row` or `b_row` (X track or under a bridge) also passes.
5. Release suite, all passing. `[.golden]` will change; don't compare it.
6. **Report:**
   - which style the test spec's `GenerateJunction` picks, and its ASCII and size;
   - the S1 time in `build-prof` (rebuild it; median of 3);
   - `[.gen]` candidate counts for the test spec, TL3 and TL6 with B=0.

## 6. Regenerate the circuits
`JUNCTION_GEN_PRIO=n JUNCTION_CIRCUIT=tools/ottd/layouts/<file> build-rel/openttd_test "[.circuit]"`:
- n=1: `circuit-LR-TL4.json`
- n=2: `-prio2.json`
- n=3: `-prio3.json`

Commit them, and report which style each picked.

## 7. Do not

- change style 0, the fences, the windows or the scoring
- route lanes with the router (a later step)

## 8. Stop conditions

- No clean style-1 candidate for the test spec. Report the least-bad violations and the ASCII, then stop without committing, and report.
- Anything not covered.

## 9. Gates

1. `cmake --build build-rel --target openttd openttd_test > pg/rel.log 2>&1`, then the Release suite, all passing.
2. `cmake --build build --target openttd openttd_test > pg/dbg.log 2>&1`: no errors or warnings in the files you touched.

Commit message: `Add: Junction parallelogram doubled bridges as a lane style`. Signing is off; end the message with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 10. Report

- commit
- red evidence
- suite totals
- the style counts
- the chosen style, ASCII and size
- the S1 time
- the circuits
- stops
- `git status --porcelain` and `git log -1`

When you have reported, stop.
