# Brief D96 step P5: candidate library and per-hover fit test (instant tier)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- **Repo:** `/home/pete/OpenTTD`, branch `feature/junction-tool`, head `{HEAD}`. The Release suite (`build-rel/openttd_test`, about 22 s) passes.
- **Plan:** `docs/junction-tool/PLAN-D96.md`, step P5; decision D96.
- **The instant tier:**
  - when the spec changes, build a library of every rule-clean flat candidate, once;
  - on each hover step, test each library entry against the terrain snapshot using tile lookups only (no routing), and take the cheapest fit.
- **This step adds pure functions and tests only. No GUI** (that's P6).
- **Scratchpad:** `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/p5/`

## 2. Read first

- `src/junction_generator.h`, all of it.
- `src/junction_generator.cpp`:
  - `BuildJunctionCandidate`, from the `if (terrain != nullptr)` block before the normalise lambda to the end of the function: how terrain is checked and costed, and how `hub` is computed;
  - `GenerateJunction`, all of it: the plane choice and the sweep.
- `src/junction_terrain.h`: `UsableAt`, `CostAt`, `SpanOk`.
- `src/tests/junction_generator.cpp`: `MakeFlatTerrain`, `SetTerrainRect`, `SameLayout`, and the test "GenerateJunction avoids a block of houses".

## 3. Coordinates (read carefully)

- A layout returned with hub h has its tile t at working coordinate (t.x − h.x + 30, t.y − h.y + 0).
- The terrain is indexed by working coordinates.

## 4. Changes

### 4.1 Factor out the plane choice (pure refactor)
In `junction_generator.cpp`, move the plane-choice code at the top of `GenerateJunction` (the `counts` loop over x 26..34, y −4..4, then sort, truncate to 3) into:

```cpp
/** The candidate planes for a terrain: the most common z_max values of Buildable tiles around the hub, at most 3. */
std::vector<int> JunctionTerrainPlanes(const JunctionTerrain &terrain);
```

Declare it in `junction_generator.h`. `GenerateJunction` calls it, and still returns NoLayout when it is empty.

### 4.2 Library (`junction_generator.h/.cpp`)

```cpp
/** One rule-clean flat candidate (D96 instant tier). */
struct JunctionLibraryEntry {
	JunctionLayout layout;
	TileIndexDiffC hub{}; ///< As JunctionGenerateResult::hub.
	std::pair<Money, int> score; ///< ScoreJunctionLayout on flat ground.
	int area = 0; ///< layout.size.x * layout.size.y.
};

/**
 * Every rule-clean flat candidate of the sweep, without the early stop: heights ml_gap+5 .. ml_gap+20, both crossing costs, slack 0..3.
 * Identical layouts (same JunctionLayoutToJson) are kept once, the first one found. Order is sweep order.
 */
std::vector<JunctionLibraryEntry> BuildJunctionLibrary(const JunctionSpec &spec);

/** The result of fitting the library to a terrain. */
struct JunctionLibraryFit {
	size_t entry = 0; ///< Index into the library.
	int plane = 0;
	Money terrain_cost = 0;
};

/**
 * Fit each library entry to the terrain at each of JunctionTerrainPlanes(terrain), with tile lookups only.
 * A fit needs every tile holding track and every bridge head UsableAt the plane, and every bridge middle tile SpanOk.
 * Terrain cost is the sum of CostAt over the tiles holding track or a bridge head (each tile once).
 * Best = lowest (score.first + terrain_cost, score.second, area), ties to the earlier entry, then the earlier plane.
 * @return The best fit, or std::nullopt when nothing fits (also when the spec gives NeedsMlGap, i.e. an empty library).
 */
std::optional<JunctionLibraryFit> FitJunctionLibrary(const std::vector<JunctionLibraryEntry> &library, const JunctionTerrain &terrain);
```

- `BuildJunctionLibrary` returns empty when `spec.ml_gap == 0`.
- Each entry is built with `BuildJunctionCandidate(spec, params, nullptr, nullptr, &hub)`, then filtered with `CheckJunctionRules(...).empty()`.
- In `FitJunctionLibrary`, the bridge-middle walk is the same as in `BuildJunctionCandidate`. Map every layout tile to working coordinates as in §3 before calling the terrain.

## 5. Tests (`src/tests/junction_generator.cpp`); write them first, red first (a link error or an assertion are both acceptable red for brand-new functions; say which)

1. **"FitJunctionLibrary on flat terrain is at least as good as GenerateJunction":**
   - library = `BuildJunctionLibrary(MakeTestJunctionSpec())`; it must not be empty;
   - fit on `MakeFlatTerrain()`;
   - the fitted entry's `score.first` + terrain_cost ≤ `ScoreJunctionLayout(gen)` + 0, where gen = `GenerateJunction(spec, &flat)`. (On flat ground at z 0 the terrain cost is the clear cost, 0 for the flat fixture. If not, compare like with like and report.)
2. **"FitJunctionLibrary avoids blocked tiles":** flat terrain with `SetTerrainRect(t, 18, 27, 4, 10, Blocked)`.
   - Either nullopt, with a WARN, or the fitted entry's tiles, mapped per §3, are all outside the rectangle (x 18..27, y 4..10). Check every track tile and bridge head.
   - Also, a second terrain blocks every tile: the fit returns nullopt.
3. **"FitJunctionLibrary picks a plane":** `MakeFlatTerrain(2)` (z 2 everywhere) gives a fit with plane 2.
4. **"JunctionTerrainPlanes":** on `MakeFlatTerrain(1)` it returns {1}.
5. **Timing, reported and not asserted:**
   - in one of the tests above, time `BuildJunctionLibrary` and 100 calls of `FitJunctionLibrary` on the flat terrain with `std::chrono::steady_clock`, and print both through `WARN`;
   - report the Release numbers: the library size, the build ms, and the fit µs per call.

**Golden:** run `[.golden]` (`JUNCTION_GOLDEN=p5/before.txt` before the 4.1 refactor, `p5/after.txt` after) and `cmp` them. They must be identical: 4.1 is a pure refactor.

## 6. Do not

- touch the GUI, the game side, the router, the signal pass or the checker
- add threading
- change `GenerateJunction`'s results (golden identical)

## 7. Stop conditions

- The golden files differ.
- Test 1 fails: the library's best is worse than `GenerateJunction`'s. Report both scores.
- Anything not covered.

## 8. Gates

1. `cmake --build build-rel --target openttd openttd_test > p5/build.log 2>&1`, then the full Release suite, all passing.
2. `cmake --build build --target openttd openttd_test > p5/dbg.log 2>&1`: no errors or warnings in the files you touched. Don't run the Debug suite.

Commit message: `Add: Junction candidate library and per-hover fit test`. Signing is off; end the message with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 9. Report

- commit
- red evidence
- types and functions added
- suite totals
- cmp result
- library size, build ms, fit µs per call (Release)
- stops
- `git status --porcelain` and `git log -1`

When you have reported, stop.
