# Brief: 1a step 2, test junction fixture and build steps

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- Repo `/home/pete/OpenTTD`, branch `feature/junction-tool`, head `{HEAD}`. Work in this checkout.
- This step comes from `docs/junction-tool/PLAN-1a.md`, "Steps" item 2. Step 1 added `src/junction_layout.h` and `src/junction_layout.cpp` with stub functions, plus `src/tests/junction_layout.cpp` with `StubJunctionPricing` and one outer test.
- That outer test, "JunctionLayout outer: test junction on flat clear land prices exactly", is **red by design** and stays red after this step. It goes green in step 3.
- The baseline after step 1 is 96 test cases: 95 pass and that outer test fails.

## 2. Read first, once each

- `CODINGSTYLE.md`
- `src/junction_layout.h` and `src/junction_layout.cpp` (both short)
- `src/tests/junction_layout.cpp` (short)
- `src/map_func.h` lines 505–525 (`AddTileIndexDiffCWrap`, which returns `INVALID_TILE` off the map)

## 3. Tests (write first, see them fail)

Add to `src/tests/junction_layout.cpp`, after the existing outer test:

1. **`TEST_CASE("JunctionLayout: test junction fixture contents")`** checks that `MakeTestJunctionLayout()` returns exactly:
   - `size == {8, 6}` (compare `.x` and `.y`)
   - `tracks.size() == 9`:
     - `tracks[0..7]` are `{offset {x, 2}, TrackBits{Track::X}}` for x = 0..7, in that order
     - `tracks[8]` is `{offset {4, 5}, TrackBits{Track::Y}}`
   - `bridges.size() == 1`, with `start {4, 0}` and `end {4, 4}`
   - `signals.size() == 2`:
     - `signals[0]` is `{offset {1, 2}, Trackdir::X_SW, SignalType::Block, SignalVariant::Electric}`
     - `signals[1]` is `{offset {6, 2}, Trackdir::X_NE, SignalType::Block, SignalVariant::Electric}`

   Expected red: it fails on `size` and `tracks.size()`, because the stub returns `{}`.
2. **`TEST_CASE("JunctionLayout: build steps are rail, then bridges, then signals")`**:
   - `Map::Allocate(64, 64)`, then `steps = MakeJunctionBuildSteps(MakeTestJunctionLayout(), TileXY(10, 10))`.
   - `steps.size() == 12`.
   - `steps[0..8]` hold `JunctionRailStep` (check with `std::holds_alternative`/`std::get`):
     - `steps[i]` for i = 0..7 has tile `TileXY(10 + i, 12)` and track `Track::X`
     - `steps[8]` has tile `TileXY(14, 15)` and track `Track::Y`
   - `steps[9]` is a `JunctionBridgeStep` with start `TileXY(14, 10)` and end `TileXY(14, 14)`.
   - `steps[10]` is a `JunctionSignalStep` with tile `TileXY(11, 12)`, `Trackdir::X_SW`, `SignalType::Block` and `SignalVariant::Electric`.
   - `steps[11]` is the same with tile `TileXY(16, 12)` and `Trackdir::X_NE`.

   Expected red: it fails on `steps.size() == 12` (0).
3. **`TEST_CASE("JunctionLayout: one rail step per track bit")`**:
   - Build a layout with `size {1, 1}` and a single track `{offset {0, 0}, TrackBits{Track::Upper, Track::Lower}}`.
   - Steps at origin `TileXY(5, 5)` are exactly 2 rail steps, both on `TileXY(5, 5)`, with tracks `Track::Upper` and then `Track::Lower`. That's ascending `Track` order, which is the order `for (Track t : bits)` gives.

   Expected red: size 0.
4. **`TEST_CASE("JunctionLayout: steps off the map get INVALID_TILE")`**:
   - `Map::Allocate(64, 64)`, then the test junction at origin `TileXY(60, 60)`.
   - `steps[0]` (offset (0,2)) has tile `TileXY(60, 62)`.
   - `steps[4]` (offset (4,2)) has tile `INVALID_TILE`.
   - `steps[9]` (the bridge) has `start == INVALID_TILE`, since (64,60) is off the map.

   Expected red: size 0, so indexing is unsafe. Guard each case with `REQUIRE(steps.size() == 12)` first, so the red is a clean assertion failure, not a crash.

Run these four tests and record the failing `REQUIRE`/`CHECK` lines.

## 4. Changes (`src/junction_layout.cpp` only)

**`MakeTestJunctionLayout()`** returns the fixture described in test 1, built in that exact order. Update its doc comment to say what the fixture is for: "Fixed layout used until the generator exists (1b); exercises track, a bridge over track, and signals in both directions."

**`MakeJunctionBuildSteps(layout, origin)`**:
1. For each `JunctionTrack` in order, and for each `Track t` in its `tracks` (ascending), push `JunctionRailStep{AddTileIndexDiffCWrap(origin, offset), t}`.
2. Then, for each `JunctionBridge`, push `JunctionBridgeStep{AddTileIndexDiffCWrap(origin, start), AddTileIndexDiffCWrap(origin, end)}`.
3. Then, for each `JunctionSignal`, push `JunctionSignalStep{AddTileIndexDiffCWrap(origin, offset), facing, type, variant}`.

Include `map_func.h`. Remove `[[maybe_unused]]` from the parameters these two functions now use. Leave `TransformJunctionLayout` and `EstimateJunction` as stubs.

## 5. Do not

- Implement `EstimateJunction` or `TransformJunctionLayout`. Those are steps 3 and 4.
- Change the header, the stub pricing class, or the existing outer test.
- Add any other test or file.

## 6. Stop conditions

- A named function, enum value or header doesn't exist as described.
- A test can't be made to fail on an assertion.
- Any existing test other than the known-red outer test fails.
- Anything not covered here.

## 7. Gates

1. Build: `cmake --build build --target openttd_test > .../s2_build.log 2>&1`. No errors, and no warnings in the junction files.
2. Red run before the changes in section 4: `./build/openttd_test "JunctionLayout:*" > .../s2_red.log 2>&1`. Record the failures.
3. After the changes: the same run, with all four new tests passing.
4. Full suite: `./build/openttd_test > .../s2_full.log 2>&1`. Expect 100 test cases, with only the known outer test failing. Report the summary line.
5. Commit only `src/junction_layout.cpp` and `src/tests/junction_layout.cpp`, with this message:

   ```
   Add: Junction test layout and build step ordering

   Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>
   ```

## 8. Report format

- **References read**
- **Commits:** hash and subject
- **Red evidence:** each new test and its failing line
- **Types and members added or removed:** names only, which should be none
- **Suite results:** the full-run summary line, naming any failure other than the known outer test
- **Manual checks:** none
- **Stops, and anything left undone**
- **State:** the output of `git status --porcelain` and `git log -1 --format='%h %s'`

When you have reported, stop.
