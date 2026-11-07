# Brief 1b step A: foundations

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- **Repo:** `/home/pete/OpenTTD`, branch `feature/junction-tool`, head `{HEAD}`.
- **Plan:** `docs/junction-tool/PLAN-1b.md` §1, §3, §5 items 1–2, §8 and the Steps row A.
- **Decisions:** `docs/junction-tool/DECISIONS.md` D36, D38, D60, D65, D67, D72.
- Another agent may be working in a separate git worktree on `tools/ottd/`. Ignore it, and don't touch `tools/`.

## 2. Read first

Read each of these once, with offset and limit:
- `CODINGSTYLE.md`
- `src/junction_layout.h` and the first 120 lines of `src/junction_layout.cpp` (the style and the types you reuse)
- `src/tests/junction_layout.cpp`, the first 60 lines (test style and includes)
- `src/track_func.h`: `TrackdirToExitdir`, `TrackdirReachesTrackdirs`, `TrackdirToTrack`, `TrackToTrackbits`, `IsDiagonalTrackdir`
- `src/direction_func.h`: `ChangeDir`, `DirDifference`
- `src/map_func.h`: `TileIndexDiffCByDiagDir`
- `src/train_cmd.cpp`: `Train::GetCurveSpeedLimit` (the formula you reproduce)

## 3. Tests, outermost first

Make every test red before writing its code. Record each failure message, and make sure it fails on an assertion, not a compile or link error. To get there, create stubs that compile first.

New test files go in `src/tests/` and are registered in `src/tests/CMakeLists.txt`.

**`src/tests/junction_generator.cpp`:**

1. **`"GenerateJunction gives a rule-clean L_R T"`**, tags `[junction][!mayfail]`.
   - Spec from `MakeTestJunctionSpec()`.
   - `REQUIRE(result.layout.has_value())`, then `CHECK(CheckJunctionRules(*result.layout, spec).empty())`.
   - Red: the stub `GenerateJunction` returns no layout, with `JunctionGenerateError::NoLayout`.
   - It stays red and tagged `[!mayfail]`; step D makes it green.
2. **`ParseJunctionNotation`:**
   - `"L_R"` → gap 1
   - `"LR"` → gap 0
   - `"L___R"` → gap 3
   - `"LL_RR"` and `"L_L__R_R"` → `MultiLane`
   - `""`, `"R_L"`, `"L_X_R"`, `"L_"`, `"_R"` → `Invalid`
3. **`CurveSpeedLimitForSpacing(s, curve_speed)`:**
   - s = 1, 2, 3, 4, 6, 8, 10, 12 with curve_speed 0 → 88, 111, 132, 151, 183, 207, 223, 231
   - s = 13 → 231 (clamped)
   - s = 2, curve_speed 1 → 166
   - s = 4, curve_speed 2 → 302
4. **`DeriveCurveSpacing(speed, curve_speed, tl)`:**

   | speed | curve_speed | tl | result |
   |---|---|---|---|
   | 88 | 0 | 10 | 1 |
   | 111 | 0 | 10 | 2 |
   | 160 | 0 | 10 | 5 |
   | 231 | 0 | 10 | 12 |
   | 232 | 0 | 10 | 20 |
   | 160 | 0 | 2 | 4 |
   | 160 | 1 | 10 | 2 |

**`src/tests/junction_route.cpp`:**

5. **`TrackdirToDirection`:** all 12 valid trackdirs, mapped as listed in §4.
6. **The rasterizer:**
   - From `{0,0}` `Trackdir::X_SW`, `"FF"` gives 3 pieces at x = 0, 1, 2, all `X_SW`, 2 units each.
   - `"L"` from `X_SW` gives a second piece whose direction is `ChangeDir(SW, Left45)`, on the next tile, 1 unit.
   - `"R"` likewise with `Right45`.
   - A straight run of half pieces: `"LFFF"` alternates the two half tracks of the same direction (e.g. Upper/Lower), each 1 unit, every piece on a new tile.
   - `"LFR"` returns to `X_SW`: Right_S, Left_S, X_SW. `"LR"` is an error, because from a half piece only F and one of L/R can be reached (parity; see `_exitdir_reaches_trackdirs`).
   - From `{0,0}` `X_SW`, `"B3F"`:
     - pieces at x = 1..5 have `on_bridge = true`
     - a bridge from {1,0} to {5,0}
     - the final piece is at {6,0}, `X_SW`, with `on_bridge = false`
   - Bridge from a half piece (`"LB2"`) → error.
   - An unknown move character → error.
   - A `B` with no number → error.
7. **`AddRasterToLayout` and `NormaliseJunctionLayout`:**
   - Two rasters sharing a tile with different tracks give one `JunctionTrack` whose bits are ORed.
   - Bridge-flagged pieces add no track bits.
   - Normalising a layout with negative offsets shifts everything (tracks, signals, bridges) so the minimum x and y are 0, and sets `size` to the bounding box.
   - Bridge `start` is the head with the smaller coordinate.
8. **`RenderJunctionAscii`** on a hand-made layout gives exactly the expected multi-line string. Cover one tile of each glyph in §4.

## 4. Changes

### New files

- `src/junction_generator.h` and `.cpp`
- `src/junction_rules.h` and `.cpp`
- `src/junction_route.h` and `.cpp`

Register all six in `src/CMakeLists.txt`, next to the existing `junction_*` entries. Follow the existing junction files' header style: copyright block, `/** @file */`, and `safeguards.h` last among the includes in the `.cpp`.

### `junction_route.h` (pure: no map or game state)

```cpp
/** One track piece of a route. Units: an axis piece is 2, a half piece 1, a bridge tile 2 (PLAN-1b §1). */
struct JunctionPiece {
	TileIndexDiffC tile;
	Trackdir trackdir;
	uint8_t units;
	bool on_bridge; ///< A bridge head or span tile; no track is laid here.
};

/** A route program: a start piece and a move string of F, L, R and B<n> (PLAN-1b §3). */
struct JunctionRoute {
	TileIndexDiffC start;
	Trackdir start_trackdir;
	std::string moves;
};

/** The rasterized route, or an error message when the program is invalid. */
struct JunctionRaster {
	std::vector<JunctionPiece> pieces;
	std::vector<JunctionBridge> bridges;
	std::string error; ///< Empty when valid.
};

Direction TrackdirToDirection(Trackdir td);
JunctionRaster RasterizeJunctionRoute(const JunctionRoute &route);
void AddRasterToLayout(JunctionLayout &layout, const JunctionRaster &raster);
void NormaliseJunctionLayout(JunctionLayout &layout);
std::string RenderJunctionAscii(const JunctionLayout &layout);
```

**`TrackdirToDirection`:**
- `X_NE`→NE, `X_SW`→SW, `Y_NW`→NW, `Y_SE`→SE
- `Upper_E`, `Lower_E`→E; `Upper_W`, `Lower_W`→W
- `Left_N`, `Right_N`→N; `Left_S`, `Right_S`→S

**Rasterizer:**
- The start piece is the first piece.
- Every move appends one piece on the tile `cur.tile + TileIndexDiffCByDiagDir(TrackdirToExitdir(cur.trackdir))`.
- Its trackdir is the member of `TrackdirReachesTrackdirs(cur.trackdir)` whose `TrackdirToDirection` equals:
  - `F`: the same direction
  - `L`: `ChangeDir(dir, DirDiff::Left45)`
  - `R`: `ChangeDir(dir, DirDiff::Right45)`

  If no member matches, that's an error.
- Units: 2 if `IsDiagonalTrackdir` (OpenTTD's name for X/Y), else 1.
- **`B<n>`** (n is decimal, 0 or more):
  - The current piece must be X/Y, otherwise it's an error.
  - Append `n + 2` pieces along the same trackdir: the near head, n middle tiles, the far head. Each has `on_bridge = true` and 2 units.
  - Record `JunctionBridge{start, end}` with `start` the head whose x+y is smaller.
  - The next move continues from the far head.
- A layout's tile offsets may go negative until normalised.

**`AddRasterToLayout`:**
- sets `TrackdirToTrack(td)` (`TrackBits` is an `EnumBitSet`: `bits.Set(track)`) for every piece without `on_bridge` into the `JunctionTrack` for that tile, creating it if needed (linear search is fine).
- Appends the raster's bridges, skipping any equal one already present.

**`NormaliseJunctionLayout`:**
- Shifts all offsets (tracks, signals, bridge ends) by the negated minimum x and y over tracks and bridge ends.
- Sets `size` to (max − min + 1) in each axis.

**`RenderJunctionAscii`:**
- Rows are y = 0..size.y−1 and columns x = 0..size.x−1, two characters per tile, with `\n` after each row.
- **First character** (the first rule that applies wins):
  - `H`: a bridge head
  - `=`: a bridge span tile
  - `+`: exactly X|Y
  - `-`: only X
  - `|`: only Y
  - `/`: only Upper and/or Lower
  - `\`: only Left and/or Right
  - `#`: any other non-empty set
  - `.`: empty
- **Second character:**
  - no signal: a space
  - one signal: by `TrackdirToDirection(facing)`: NE `<`, SW `>`, NW `^`, SE `v`, N `n`, S `s`, E `e`, W `w`
  - more than one signal: `*`
- Add a header comment: the grid is x→right, y→down, which is not the isometric screen view.

### `junction_generator.h` (pure)

```cpp
enum class JunctionNotationError : uint8_t { None, Invalid, MultiLane };
struct JunctionNotation { uint8_t gap = 0; JunctionNotationError error = JunctionNotationError::None; };
JunctionNotation ParseJunctionNotation(std::string_view text);

struct JunctionUnitPrices {
	Money rail_piece;
	Money signal;
	std::function<Money(uint)> bridge; ///< By middle-tile count.
};

/** Everything the generator needs; the game fills it (PLAN-1b §5.1). */
struct JunctionSpec {
	uint8_t ml_gap;
	uint8_t branch_gap;
	uint8_t tl; ///< Train length in tiles.
	uint8_t sd; ///< Signal spacing in tiles.
	uint16_t reference_speed; ///< Same units as Train::GetCurveSpeedLimit.
	uint8_t curve_speed; ///< RailTypeInfo::curve_speed.
	bool curve_limit; ///< False under the original acceleration model.
	SignalType signal_type; ///< Block or PathOneWay.
	std::function<bool(uint)> bridge_span_ok; ///< By middle-tile count.
	JunctionUnitPrices prices;
};

enum class JunctionGenerateError : uint8_t { None, InvalidNotation, MultiLane, NoLayout };
struct JunctionGenerateResult {
	std::optional<JunctionLayout> layout;
	JunctionGenerateError error = JunctionGenerateError::None;
};

uint16_t CurveSpeedLimitForSpacing(uint spacing, uint8_t curve_speed);
uint DeriveCurveSpacing(uint16_t speed, uint8_t curve_speed, uint8_t tl);
JunctionSpec MakeTestJunctionSpec();
JunctionGenerateResult GenerateJunction(const JunctionSpec &spec);
```

This is a change from the plan: errors are an enum, not a `StringID`. The window maps them to strings in step E, which keeps the generator pure.

**Notation grammar:**
- The text must match `L+` `_*` `R+` (one or more `L`, zero or more `_`, one or more `R`), otherwise it's `Invalid`.
- Gap = the number of `_`.
- More than one `L` or `R` → `MultiLane`.
- The D38 form `L_L__R_R` doesn't match the grammar, but is `MultiLane` because it contains more than one `L`. Check for "more than one L or R, and only L/_/R characters, starting with L and ending with R" before reporting `Invalid`.

**`CurveSpeedLimitForSpacing`:** reproduces `GetCurveSpeedLimit` exactly. `c = Clamp(s, 1, 12)`; `v = 232 - (13 - c) * (13 - c)`; then `v += (v / 2) * curve_speed`. Integer arithmetic, as in the game.

**`DeriveCurveSpacing`:** the smallest s in 1..12 with `CurveSpeedLimitForSpacing(s, curve_speed) >= speed`. If there's none, use `2 * tl`. Return `std::min(s, 2 * tl)`.

**`MakeTestJunctionSpec`:**
- ml_gap 1, branch_gap 1, tl 4, sd 2
- reference_speed 160, curve_speed 0, curve_limit true
- signal_type `SignalType::Block`
- `bridge_span_ok = [](uint n) { return n >= 1 && n <= 10; }`
- prices: rail_piece 100, signal 50, `bridge = [](uint n) { return Money(500 + 200 * n); }`

**`GenerateJunction` stub:** returns `{std::nullopt, JunctionGenerateError::NoLayout}`.

### `junction_rules.h` (pure)

```cpp
enum class JunctionRule : uint8_t { NotImplemented, Connectivity, Geometry, CurveSpeed, Order, Bridges, Signals, WaitingRoom, Deadlock, Prio, Footprint };
struct RuleViolation { JunctionRule rule; TileIndexDiffC tile; std::string message; };
std::vector<RuleViolation> CheckJunctionRules(const JunctionLayout &layout, const JunctionSpec &spec);
```

**Stub:** returns `{{JunctionRule::NotImplemented, {0, 0}, "rule checker not implemented"}}`.

## 5. Do not

- Don't touch any upstream (non-`junction_*`) source file except the two `CMakeLists.txt` files.
- Don't implement route tracing, rule checks, templates or search. Those are steps B–D.
- Don't add strings, settings or GUI.
- Don't change the existing junction files.
- Don't remove the `[!mayfail]` tag.

## 6. Stop conditions

- A trackdir/direction table that doesn't match the code.
- `TrackdirReachesTrackdirs` giving more than one match for any move, or no F match from some trackdir. (No L or R match from a half piece is normal: that is a rasterizer error, not a stop.)
- The curve formula in `train_cmd.cpp` differing from §4.
- Any premise above that's false.

## 7. Gates

1. Build both targets with `cmake --build build --target openttd openttd_test > /tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/stepA_build.log 2>&1`.
2. Grep the log for `error|warning`. There must be no new warnings in the junction files.
3. Run `build/openttd_test > …/stepA_test.log 2>&1` and report the totals.
4. Expected suite result: everything passes except the `[!mayfail]` outer test, which Catch2 reports as a failure that doesn't fail the run.

Commits:
- one: `Feature: Junction generator foundations - spec, notation, curve spacing, route programs`
- or two, if you split route and spec, in that same style

End each message with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. Signing is off.

## 8. Report

- references read
- commits (hash, subject)
- red evidence per test
- types and members added
- suite totals
- stops or anything left undone
- `git status --porcelain` and `git log -1`

When you have reported, stop.
