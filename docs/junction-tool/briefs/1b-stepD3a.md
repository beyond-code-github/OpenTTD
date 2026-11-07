# Brief 1b step D3a: build one generator candidate (`BuildJunctionCandidate`)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- **Repo:** `/home/pete/OpenTTD`, branch `feature/junction-tool`, head `{HEAD}`.
- **Plan:** `docs/junction-tool/GENERATOR-1b.md`, pipeline steps 1–4.
- **Decisions to read** in `DECISIONS.md`: D58, D80, D85, D87.
- **Building blocks, all done:**
  - `FindJunctionRoute` (`junction_router.h`)
  - `RasterizeJunctionRoute`, `AddRasterToLayout`, `NormaliseJunctionLayout` (`junction_route.h`)
  - `PlaceJunctionSignals` (`junction_signal_pass.h`)
  - `CheckJunctionRules`

## 2. Read first (once, with offset/limit)

- `src/junction_router.h` (all) and the D1 tests' `MakeRequest` helper in `src/tests/junction_router.cpp`
- `src/junction_route.h`
- `src/junction_signal_pass.h`
- `src/junction_generator.h` and `.cpp`
- `docs/junction-tool/layouts/reference-LR-TL4-doubled.txt`: what a good result looks like

## 3. Change: `junction_generator.h/.cpp`

```cpp
/** The swept inputs of one candidate. */
struct JunctionCandidateParams {
	uint8_t height; ///< H: the branch edge is row H-1; the E track is row 0.
	Money crossing_cost; ///< crossable_cost when a route crosses the W track.
	uint8_t joiner_slack; ///< Extra tiles kept between the lane merge and where N->W may join.
};

std::optional<JunctionLayout> BuildJunctionCandidate(const JunctionSpec &spec, const JunctionCandidateParams &params);
```

It works in the canonical frame (PLAN-1b §1), with these fixed values:
- the working area is x 0..63;
- g = `spec.ml_gap`, so the E row is 0, the W row is g+1 and lane B's row is g+2;
- xI = 30 and xO = xI + `spec.branch_gap` + 1;
- yb = height − 1. Return `std::nullopt` if yb < g+4.

**Common request fields:**
- bounds (0,0)–(63,yb);
- curve_spacing = `spec.curve_limit ? DeriveCurveSpacing(spec.reference_speed, spec.curve_speed, spec.tl) : 1`;
- s_bend_window = 2·tl;
- `spec.bridge_span_ok`, max_bridge_middle 10, and `spec.prices`.

"Plain" below means a tile whose only track is the row's X.

### Pass 1: find the crossing zone
1. **Layout:** the E row as route (0,0) `X_SW` F×63, the W row as route (63,g+1) `X_NE` F×63, plus the single track (xI,yb) Y.
2. **Route, adding each raster as it is found.** If any route fails, return `std::nullopt`.

   | Route | Starts | Goals | Crossable |
   |---|---|---|---|
   | E→S | the E-row pieces x 2..61 | `{(xO,yb) Y_SE}` | the W-row tiles, at `params.crossing_cost` |
   | W→S | the plain W-row pieces x 2..61 | E→S's `Y_SE` pieces on column xO | — |
   | N→E | `{(xI,yb) Y_NW}` | the plain E-row pieces x 2..61 | the W-row tiles, at `params.crossing_cost` |
   | N→W | N→E's `Y_NW` pieces on column xI with y < yb | the plain W-row pieces x 2..61 | — |

3. **Zone:** C = the x of every W-row tile holding any track other than X.
   - If C is empty, skip pass 2. The pass-1 layout is the result.
   - Otherwise the zone is [min C − 1, max C + 1], with heads hW = min C − 2 and hE = max C + 2, and span n = hE − hW − 1.
   - If `bridge_span_ok(n)` is false, return `std::nullopt`.

### Pass 2: fixed doubled lanes (D80/D85)
1. **Layout:** the E row as before, plus the track (xI,yb) Y.
2. **Lane A:** the route from (63,g+1) `X_NE`: F up to the tile hE+1, then `B<n>` (heads hE and hW), then F to x 0.
3. **Lane B:** the route from (63,g+1) `X_NE`: `F×p` `RFL` `F×q` `B<n>` `F×r` `LFR` `F×s`.
   - Choose p and q so that lane B's near head is (hE, g+2), and exactly one plain axis piece of lane B lies between the end of `RFL` and the head.
   - Choose r so that exactly one plain axis piece lies between the far head and the start of `LFR`. The route reaches x 0.
   - Find p, q, r and s by rasterizing and checking the piece positions. Do not do hand arithmetic.
   - Both lanes must come out equal in units.
4. **Lane merge tile M:** the W-row tile where lane B's `LFR` puts a half piece.
5. **Re-route the four movements** in the same order as pass 1, with no crossable tiles. Branch routes may only cross the lane rows under the spans, where the tiles are empty. Changes from pass 1:

   | Route | Change |
   |---|---|
   | W→S | starts are the plain pieces of lane A and lane B |
   | N→W | goals are the plain W-row pieces with x ≤ M.x − 1 − `params.joiner_slack` |

   If any route fails, return `std::nullopt`.

### Finish
1. **Crop:** content = every tile holding track other than a plain E-row or W-row X, plus bridge heads. Keep the E and W rows (and lane A's pieces) only for x in [content min x − 2, content max x + 2].
2. `NormaliseJunctionLayout`, then `PlaceJunctionSignals(layout, spec)`, then `NormaliseJunctionLayout` again (the pass adds prio tiles at row −1). Return the layout.

## 4. Tests (`src/tests/junction_generator.cpp`)

Outermost first, with red evidence against a stub that returns `std::nullopt`.

1. **Some candidate is clean:**
   - Over height g+4..g+16 (g = 1), crossing_cost ∈ {0, `spec.prices.bridge(3)`} and joiner_slack 0..3, at least one `BuildJunctionCandidate(MakeTestJunctionSpec(), params)` result has `CheckJunctionRules(...)` empty.
   - Log the first clean one's params and `RenderJunctionAscii` with `INFO`/`WARN`, so it shows in your report.
2. **Every candidate built is structurally sound:** for the same sweep, every non-null result has no `JunctionRule::Connectivity` and no `JunctionRule::Geometry` violation.
3. **Lanes:** for the first clean candidate, the two W→W paths have equal units, and the W row has no undoubled bridge (no G5 "not doubled" message).

Leave the outer `[!mayfail]` test as it is; D3b wires `GenerateJunction`.

## 5. Do not

- change the router, the signal pass or the checker
- add scoring or early stopping (that is D3b)
- tune costs or add parameters beyond `JunctionCandidateParams`

## 6. Stop conditions

- **Test 1 finds no clean candidate:** stop. Report a histogram of the violation messages (by rule group and message text, counted over all non-null candidates), plus the ASCII and the full violation list of the candidate with the fewest violations.
- **A route fails in every candidate:** report which one.
- Anything not covered.

## 7. Gates

1. `cmake --build build --target openttd openttd_test > /tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/d3a_build.log 2>&1`. There must be no errors or warnings in junction files.
2. Full suite. The baseline is D2's count; everything passes except the `[!mayfail]` outer test.

Commit message: `Add: Junction generator builds one candidate from routes, lanes and signals`. Signing is off; end the message with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 8. Report

- commit
- red evidence
- functions added
- suite totals
- the first clean candidate's params and ASCII, or the stop data
- stops
- `git status --porcelain` and `git log -1`

When you have reported, stop.
