# Brief: 1a steps 3+4, estimate and transforms

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

These are two small pure-logic steps grouped into one agent (D41). Do **part A** completely, including its commit, before starting **part B**. Each part is test-first: write its tests, run them, and record the red failures before writing the code.

## 1. State

- Repo `/home/pete/OpenTTD` (the main checkout), branch `feature/junction-tool`, head `{HEAD}`.
- Plan: `docs/junction-tool/PLAN-1a.md`, "Steps" items 3 and 4. Where this brief and the plan differ, this brief wins.
- Steps 1–2 are done:
  - `src/junction_layout.h` and `src/junction_layout.cpp` exist.
  - `MakeTestJunctionLayout` and `MakeJunctionBuildSteps` are implemented.
  - `TransformJunctionLayout` and `EstimateJunction` are stubs.
- The tests in `src/tests/junction_layout.cpp` include `StubJunctionPricing` and the known-red outer test "JunctionLayout outer: test junction on flat clear land prices exactly", which this brief turns green.
- The baseline is 100 test cases: 99 pass, plus that outer test.
- Other agents are working in separate worktrees on other files at the same time. Touch only the files named here.

## 2. Read first, once each (really read them)

- `CODINGSTYLE.md`, in full. It's required: the last agent skipped it.
- `src/junction_layout.h`, `src/junction_layout.cpp` and `src/tests/junction_layout.cpp`.
- `src/track_func.h`: the doc comments and signatures of `TrackdirToExitdir`, `TrackdirToTrack` and `TrackExitdirToTrackdir` (around lines 230–370).
- `src/direction_func.h` around line 159 (`ChangeDiagDir`), and `src/direction_type.h` lines 80–125 (`DiagDirection`, `DiagDirDiff`).

## Part A: `EstimateJunction` (plan step 3)

### A1. Rules for `EstimateJunction(layout, origin, pricing)`

1. **Occupied tiles**, in this order, each translated with `AddTileIndexDiffCWrap(origin, offset)`:
   - every `JunctionTrack` tile
   - for each bridge: the start head, the end head, then every span tile strictly between them (they share an x or a y)
2. **Off-map.** If any occupied or step tile is `INVALID_TILE`, add `INVALID_TILE` to `bad_tiles` **once**. If `first_error` is still unset, set it to `STR_ERROR_TOO_CLOSE_TO_EDGE_OF_MAP`. Skip pricing for anything involving an invalid tile.
3. **Reference height:** `GetFlatHeight` of the first valid occupied tile, in the order of rule 1, that returns a value. If none does, every valid occupied tile is bad.
4. **Flatness.** Every valid occupied tile whose `GetFlatHeight` is `std::nullopt`, or differs from the reference height, is bad. If `first_error` is unset, use `STR_ERROR_FLAT_LAND_REQUIRED`.
5. **Pricing**, using the steps from `MakeJunctionBuildSteps(layout, origin)`:
   - **Each rail step with a valid tile:** call `pricing.TestRail`. On success, add its cost to `total`. On failure, mark the tile bad, and set `first_error = result.GetErrorMessage()` if it's unset.
   - **Each bridge step with two valid heads:** call `pricing.PriceBridge`. On success, add its cost. On failure, mark both heads bad and set `first_error` as above.
   - **Each signal step with a valid tile:** add `pricing.SignalCost()`.
6. **`bad_tiles`** holds each tile once, in the order first found. `INVALID_TILE` counts as a tile here, so it also appears only once.
7. **`affordable`** is `bad_tiles.empty() && total <= pricing.AvailableMoney()`.

Check `CommandCost::GetErrorMessage()` and `GetCost()` in `src/command_type.h`. If they're named differently, use the existing accessors and say so in the report.

### A2. Tests (add to `src/tests/junction_layout.cpp`, write first, see them red)

Every test calls `Map::Allocate(64, 64)` and uses the test junction at origin `TileXY(10, 10)` unless stated. The occupied tiles are:
- tracks at (10..17, 12) and (14, 15)
- bridge heads at (14, 10) and (14, 14)
- spans at (14, 11), (14, 12) and (14, 13)

Name each test `"JunctionLayout outer: ..."`:

1. **"one non-flat tile is bad"**:
   - Setup: `pricing.not_flat.insert(TileXY(12, 12))`.
   - Expect: `bad_tiles == {TileXY(12, 12)}`, `!affordable`, `first_error == STR_ERROR_FLAT_LAND_REQUIRED`, and `total == 2000`. The stub's rail pricing doesn't depend on flatness, so this isolates the flatness rule.
2. **"tile at another height is bad"**:
   - Setup: `pricing.heights[TileXY(16, 12)] = 2`.
   - Expect: `bad_tiles == {TileXY(16, 12)}`, `!affordable`, and `first_error == STR_ERROR_FLAT_LAND_REQUIRED`.
3. **"a failed rail step marks its tile and the rest are still priced"**:
   - Setup: `pricing.rail_fails[TileXY(11, 12)] = STR_ERROR_CAN_T_BUILD_RAILROAD_TRACK`.
   - Expect: `bad_tiles == {TileXY(11, 12)}`, `first_error == STR_ERROR_CAN_T_BUILD_RAILROAD_TRACK`, `total == 1900`, and `!affordable`.
4. **"a failed bridge marks both heads"**:
   - Setup: `pricing.bridge_fails = true`.
   - Expect: `bad_tiles == {TileXY(14, 10), TileXY(14, 14)}`, `total == 1000` (900 rail + 100 signals), and `!affordable`.
5. **"affordability is total against money"**:
   - With `money = 1999`: `!affordable` and `bad_tiles.empty()`.
   - With `money = 2000`: `affordable`.
6. **"layout partly off the map"**:
   - Setup: origin `TileXY(60, 60)`.
   - Expect: `bad_tiles` contains `INVALID_TILE` exactly once, `first_error == STR_ERROR_TOO_CLOSE_TO_EDGE_OF_MAP`, and `!affordable`.

Compare `bad_tiles` to an expected `std::vector<TileIndex>` built in the test.

**Red:** run `./build/openttd_test "JunctionLayout outer*"`. All six new tests and the existing outer test must fail on assertions. Record the lines.

### A3. Implement and commit

- Implement `EstimateJunction` in `src/junction_layout.cpp` per A1, with a helper `static void MarkBad(JunctionEstimate &estimate, TileIndex tile)` that adds the tile only if it's not already present.
- Include `table/strings.h` if needed for the string IDs.
- Gates: build with no warnings in the junction files. All seven outer tests pass. The full suite has 106 test cases, all passing.
- Commit only `src/junction_layout.cpp` and `src/tests/junction_layout.cpp`, with this message:

  ```
  Add: Junction placement estimate

  Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>
  ```

## Part B: transforms (plan step 4)

### B1. Header additions (`src/junction_layout.h`)

Add exactly these two declarations after `TransformJunctionLayout`, each with a one-line doc comment:

```cpp
Track TransformTrack(Track track, JunctionTransform transform);
Trackdir TransformTrackdir(Trackdir trackdir, JunctionTransform transform);
```

### B2. Rules

- **DiagDirection transform:**
  - Rotate: `ChangeDiagDir(d, DiagDirDiff::Right90)`, so NE→SE→SW→NW→NE.
  - Mirror: NE↔SW, while SE and NW stay put.
- **Track edges** (a static table in the .cpp): X = {NE, SW}, Y = {NW, SE}, Upper = {NE, NW}, Lower = {SE, SW}, Left = {SW, NW}, Right = {NE, SE}.
- **`TransformTrack`:** transform both edges, then return the track whose edge pair equals the new pair (order-insensitive).
- **`TransformTrackdir`:** `TrackExitdirToTrackdir(TransformTrack(TrackdirToTrack(td)), transformed TrackdirToExitdir(td))`.
- **`TransformJunctionLayout`:**
  - Rotate: an offset (x, y) becomes (y, size.x − 1 − x), and the size becomes (size.y, size.x).
  - Mirror: an offset (x, y) becomes (size.x − 1 − x, y), and the size is unchanged.
  - Track bits: transform each bit with `TransformTrack`.
  - Signals: transform the offset and `facing`. Type and variant are unchanged.
  - Bridges: transform both offsets, then swap them if needed so that `start` has the smaller x + y.
  - Element order in each vector is preserved.

### B3. Tests (write first, see them red)

1. **Inner tests**, named `"JunctionLayout: ..."`:
   - **"every track transforms to a valid track":** for each `Track` from X to Right, under both transforms, `IsValidTrack(result)`.
   - **"known track and trackdir cases":** rotating `Track::X` gives `Track::Y`; rotating `Trackdir::X_NE` gives `Trackdir::Y_SE`; mirroring `Track::Upper` gives `Track::Left`; mirroring `Trackdir::X_NE` gives `Trackdir::X_SW`; mirroring `Track::Y` gives `Track::Y`.
2. **Outer tests**, named `"JunctionLayout outer: ..."`:
   - **"rotating four times is the identity":** apply Rotate four times to the test junction; the result equals the original field by field.
   - **"mirroring twice is the identity".**
   - **"rotation swaps the size":** a rotated test junction has size {6, 8}.
   - **"a rotated junction still prices exactly":** the test junction rotated once, at origin `TileXY(10, 10)` with default stub pricing, has `total == 2000`, `bad_tiles.empty()` and `affordable`.
   - **"bridge start stays the northern head":** after one rotation and after one mirror, the bridge's `start.x + start.y <= end.x + end.y`.

   For field-by-field comparison, add a test-file-local helper, `static bool SameLayout(const JunctionLayout &a, const JunctionLayout &b)`, that compares the size and every element's fields. Don't add `operator==` to the header.

**Red:** the stubs return their input unchanged, so the identity tests may pass before you write any code. That's expected and acceptable for those two only. Record which tests are red: "known cases", "swaps the size" and "bridge start" must be red. The new transform functions don't exist yet, so stub them first to return their input, see the reds, then implement.

### B4. Implement and commit

- Implement in `src/junction_layout.cpp`.
- Gates: build with no warnings in the junction files. The full suite has 113 test cases, all passing.
- Commit only `src/junction_layout.h`, `src/junction_layout.cpp` and `src/tests/junction_layout.cpp`, with this message:

  ```
  Add: Rotate and mirror for junction layouts

  Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>
  ```

## 5. Do not

- Touch any file other than the three named.
- Add `operator==` or other members to the header beyond B1.
- Change `StubJunctionPricing` or the existing tests, except to add the new ones.
- Build the `openttd` target. Only `openttd_test` is needed.

## 6. Stop conditions

- A named function or string doesn't exist as described.
- A test that this brief says must be red passes before the code exists.
- An existing test fails.
- Expected numbers don't match. Report the actual values and don't adjust the expectations.
- Anything not covered here.

## 7. Logs

Write build and test output to `.../s34_*.log` and grep them. Don't paste whole logs.

## 8. Report format

- **References read**
- **Commits:** hash and subject for each (two expected)
- **Red evidence:** each new test and its failure line, per part
- **Types and members added or removed:** names only
- **Suite results:** the summary line after each part
- **Manual checks:** none
- **Stops, and anything left undone**
- **State:** the output of `git status --porcelain` and `git log -2 --format='%h %s'`

When you have reported, stop.
