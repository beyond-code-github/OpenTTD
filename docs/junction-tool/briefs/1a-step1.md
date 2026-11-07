# Brief: 1a step 1, API skeleton and outer test

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- Repo `/home/pete/OpenTTD`, branch `feature/junction-tool`, head `{HEAD}`. Work directly in this checkout; there is no separate worktree.
- This step comes from `docs/junction-tool/PLAN-1a.md`, section "Steps", item 1. The types are specified in its "Junction description" and "Build steps and estimate" sections.
- The build directory `build/` is already configured (Ninja, clang-18). Don't reconfigure it.
- The baseline is `./build/openttd_test`: all tests pass, 95 test cases.

## 2. Read first, once each

- `CODINGSTYLE.md`: tabs, brace style, doc comments, `this->` use.
- `docs/junction-tool/PLAN-1a.md`, only the sections "Junction description" and "Build steps and estimate" (use offset and limit).
- `src/tests/tilearea.cpp`, the first 60 lines: the test file style (licence header, `@file`, includes, `#include "../safeguards.h"` last, `Map::Allocate`).
- `src/map_type.h`, lines 25–40 (`TileIndexDiffC`), and `src/command_type.h`, lines 40–60 (the `CommandCost` constructors).

## 3. Tests (write first, see them fail)

Create `src/tests/junction_layout.cpp` containing:

1. **`StubJunctionPricing`**, a class deriving from `JunctionPricing`, defined in the test file. Its public members:
   - `std::set<TileIndex> not_flat;`
   - `std::map<TileIndex, int> heights;`
   - `std::map<TileIndex, StringID> rail_fails;`
   - `bool bridge_fails = false;`
   - `Money money = 1000000;`

   Its overrides:
   - `GetFlatHeight(tile)`: `std::nullopt` if `tile == INVALID_TILE` or `tile` is in `not_flat`; otherwise the value from `heights` if present, else `1`.
   - `TestRail(tile, track)`: `CommandCost(rail_fails[tile])` if `tile` is in `rail_fails`, else `CommandCost(ExpensesType::Construction, 100)`. Look it up with `find`; don't insert with `operator[]` in a const function.
   - `PriceBridge(start, end)`: `CommandCost(STR_ERROR_CAN_T_BUILD_BRIDGE_HERE)` if `bridge_fails`, else `CommandCost(ExpensesType::Construction, 1000)`.
   - `SignalCost()`: `50`.
   - `AvailableMoney()`: `this->money`.

   Include `"../table/strings.h"` for the string ID if needed. If `STR_ERROR_CAN_T_BUILD_BRIDGE_HERE` isn't reachable from the test file, stop and report.
2. **`TEST_CASE("JunctionLayout outer: test junction on flat clear land prices exactly")`**:
   - `Map::Allocate(64, 64);`
   - Build a `StubJunctionPricing pricing;` with its defaults.
   - Call `EstimateJunction(MakeTestJunctionLayout(), TileXY(10, 10), pricing)`.
   - `CHECK(estimate.total == 2000);`
   - `CHECK(estimate.bad_tiles.empty());`
   - `CHECK(estimate.affordable);`
   - Add a comment explaining the 2000: 9 rail pieces × 100 + 1 bridge × 1000 + 2 signals × 50.

Register `junction_layout.cpp` in `src/tests/CMakeLists.txt` in alphabetical order, after `history_func.cpp`.

**Expected red:** the build succeeds, and the test fails on `estimate.total == 2000` (0 == 2000) and `estimate.affordable`. It must not fail on a compile error. Record the failure lines from the test output.

## 4. Changes

### `src/junction_layout.h` (new)

Use the licence header and `@file` doc comment like the other files, and the include guard `JUNCTION_LAYOUT_H`. Include what's needed: `map_type.h`, `track_type.h`, `signal_type.h`, `command_type.h`, `economy_type.h`, `strings_type.h`, `tile_type.h`, and `<optional>`, `<variant>`, `<vector>`. Declare exactly the following, with a one-line doc comment on each type and function:

```cpp
struct JunctionTrack {
	TileIndexDiffC offset;
	TrackBits tracks;
};

/** A one-way signal; trains travelling along 'facing' see it (SignalAlongTrackdir). */
struct JunctionSignal {
	TileIndexDiffC offset;
	Trackdir facing;
	SignalType type;
	SignalVariant variant;
};

/** A rail bridge between two heads on the same row or column; the bridge type is chosen at placement. */
struct JunctionBridge {
	TileIndexDiffC start;
	TileIndexDiffC end;
};

/** A junction relative to its north corner (0,0); all offsets have x and y >= 0. */
struct JunctionLayout {
	TileIndexDiffC size; ///< Bounding box in tiles.
	std::vector<JunctionTrack> tracks;
	std::vector<JunctionSignal> signals;
	std::vector<JunctionBridge> bridges;
};

enum class JunctionTransform : uint8_t { Rotate, Mirror };

struct JunctionRailStep { TileIndex tile; Track track; };
struct JunctionBridgeStep { TileIndex start; TileIndex end; };
struct JunctionSignalStep { TileIndex tile; Trackdir facing; SignalType type; SignalVariant variant; };
using JunctionBuildStep = std::variant<JunctionRailStep, JunctionBridgeStep, JunctionSignalStep>;

/** Everything the estimate needs from the game; tests provide a stub. */
class JunctionPricing {
public:
	virtual ~JunctionPricing() = default;
	/** Height of a flat tile on the map, or std::nullopt if off-map or not flat. */
	virtual std::optional<int> GetFlatHeight(TileIndex tile) const = 0;
	/** Test-only run of building one rail piece; the cost includes clearing. */
	virtual CommandCost TestRail(TileIndex tile, Track track) const = 0;
	/** Full price of the cheapest suitable bridge type between two heads, or an error. */
	virtual CommandCost PriceBridge(TileIndex start, TileIndex end) const = 0;
	/** Price of one signal. */
	virtual Money SignalCost() const = 0;
	/** Money the local company can spend. */
	virtual Money AvailableMoney() const = 0;
};

/** Up-front price and validity of placing a junction. */
struct JunctionEstimate {
	Money total = 0;
	std::vector<TileIndex> bad_tiles; ///< Tiles that are off-map, not flat, at the wrong height, or whose step failed.
	StringID first_error = INVALID_STRING_ID;
	bool affordable = false; ///< bad_tiles is empty and total <= AvailableMoney().
};

JunctionLayout TransformJunctionLayout(const JunctionLayout &layout, JunctionTransform transform);
JunctionLayout MakeTestJunctionLayout();
std::vector<JunctionBuildStep> MakeJunctionBuildSteps(const JunctionLayout &layout, TileIndex origin);
JunctionEstimate EstimateJunction(const JunctionLayout &layout, TileIndex origin, const JunctionPricing &pricing);
```

If `strings_type.h` doesn't define `StringID` and `INVALID_STRING_ID`, find the header that does, use it, and name it in the report.

### `src/junction_layout.cpp` (new)

Use the licence header and `@file`, then `#include "stdafx.h"`, `#include "junction_layout.h"`, and `#include "safeguards.h"` last. Each function is a stub with a doc comment:
- `TransformJunctionLayout` returns `layout` unchanged.
- `MakeTestJunctionLayout` returns `{}`.
- `MakeJunctionBuildSteps` returns `{}`.
- `EstimateJunction` returns `{}`.

Mark unused parameters `[[maybe_unused]]` so there are no warnings.

### `src/CMakeLists.txt`

Add `junction_layout.cpp` and `junction_layout.h` to the main `add_files(` list in alphabetical order. They go after `intro_gui.cpp` (line ~252) and before `landscape.cpp`.

## 5. Do not

- Implement any function beyond the stubs. That's steps 2–4.
- Add the game-side pricing, the GUI, drawing, or any other file.
- Add more tests than the one above.
- Change any existing file other than the two `CMakeLists.txt` files.
- Reconfigure CMake or change build settings.

## 6. Stop conditions

- A named header, type or string doesn't exist as described.
- The test can't be made to fail on its assertion, for example because it fails to compile for a reason you can't fix within this brief.
- An existing test fails.
- Anything not covered here.

## 7. Gates

1. `cmake --build build --target openttd_test > /tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/s1_build.log 2>&1`, then grep the log for `error:` and `warning:`. There must be no errors and no warnings in the new files.
2. Red run: `./build/openttd_test "JunctionLayout outer*" > .../s1_red.log 2>&1`. Record the failing `CHECK` lines.
3. This step ends red by design, because the outer test stays red until step 3. Also run the full suite, `./build/openttd_test > .../s1_full.log 2>&1`. Expect 96 test cases: 95 passing and only the new one failing. Report the summary line.
4. Commit only the four touched or new files, with this message:

   ```
   Add: Junction layout API skeleton and outer pricing test

   Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>
   ```

   Signing is disabled for this repo. Don't change any git config.

## 8. Report format

- **References read**
- **Commits:** hash and subject
- **Red evidence:** the test name and the failing `CHECK` lines
- **Types and members added or removed:** names only
- **Suite results:** the full-run summary line, naming any failure other than the new test
- **Manual checks:** none expected
- **Stops, and anything left undone,** with the reason
- **State:** the output of `git status --porcelain` and `git log -1 --format='%h %s'`

When you have reported, stop.
