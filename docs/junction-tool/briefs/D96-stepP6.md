# Brief D96 step P6: two-tier hover in the Junction window

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- **Repo:** `/home/pete/OpenTTD`, branch `feature/junction-tool`, head `{HEAD}`. The Release suite passes (`build-rel/openttd_test`).
- **Plan:** `docs/junction-tool/PLAN-D96.md` P6, decision D96. The engineer: "I'm envisaging you move it around and it updates in near-real time".
- **What exists:**
  - P5 added `BuildJunctionLibrary` (about 0.7 s in Release, once per spec) and `FitJunctionLibrary` (about 64 µs per call).
  - The window currently shows nothing until the hover has rested 300 ms and the full `GenerateJunctionNear` (0.5 s up to many seconds) finishes.
- **After this step:**
  - **Instant tier:** on every hover tile change, the window fits the library at the cursor (and at nearby offsets) and draws at once.
  - **Refine tier:** the full search still runs after the 300 ms rest, and its job returns the better of the full result and the library fit.
- **Scratchpad:** `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/p6/`

## 2. Read first

- `src/junction_gui.cpp` l.240–530 and `OnRealtimeTick` (about l.700–790): the key, the job, the cache, `ShowHover`, `ShowResult`, `StartJob`, `PollJob`, `Regenerate`.
- `src/junction_game.h/.cpp`: `JunctionNearResult`, `GenerateJunctionNear` (l.251), `MakeJunctionTerrain`, `MakeJunctionSpec`.
- `src/junction_generator.h`: `JunctionGenerateResult`, `JunctionLibraryEntry`, `BuildJunctionLibrary`, `JunctionLibraryFit`, `FitJunctionLibrary`, `ScoreJunctionLayout`.
- `src/console_cmds.cpp`: `junction_export`, and `tools/ottd/ottd`: `junction_here`, `cmd_junction_scan`.

## 3. Changes

### 3.1 Generator (`junction_generator.h/.cpp`)
- `JunctionGenerateResult` gets `std::pair<Money, int> score{}; ///< The chosen layout's score, terrain cost included.`
- `GenerateJunction` sets it to the `best_score` it already keeps.
- Golden must stay identical (the JSON doesn't include the score).

### 3.2 Game side (`junction_game.h/.cpp`)
1. Move the offset list of `GenerateJunctionNear` into `static std::vector<TileIndexDiffC> JunctionNearOffsets()`, in the same .cpp, used by both functions below. Same order.
2. Add:

   ```cpp
   /**
    * The instant tier (D96): fit the library at the anchor, then at the nearby offsets, using the same shifted snapshots as GenerateJunctionNear.
    * Stops at the first offset with a fit.
    * @return The fitted entry's layout as a result (error None, hub and score from the entry, terrain cost added to score.first), with its offset;
    *         or an empty result with error NoLayout and offset (0,0) when nothing fits.
    */
   JunctionNearResult FitJunctionLibraryNear(const std::vector<JunctionLibraryEntry> &library, const JunctionTerrain &terrain, uint8_t rotation, bool mirrored);
   ```

   - With an empty library, the error is `NeedsMlGap` when `ml_gap == 0`. The function doesn't know the spec, so the GUI handles that case. Here just return NoLayout.

### 3.3 GUI (`junction_gui.cpp`)
1. **Library state.**
   - Add `struct JunctionLibraryJob { std::atomic<bool> done{false}; JunctionSpec spec; std::vector<JunctionLibraryEntry> library; };`, next to `JunctionGenerateJob`.
   - Window members:
     - `std::shared_ptr<JunctionLibraryJob> library_job; ///< Library for the current spec, or being built.`
     - `std::thread library_thread;`
     - `std::optional<JunctionGenerateKey> library_key; ///< MakeKey of the library, with anchor INVALID_TILE and rotation 0, mirrored false.`
   - Add a helper, `JunctionGenerateKey SpecKey() const`: `MakeKey(INVALID_TILE)` with rotation 0 and mirrored false. The library is flat, so it doesn't depend on orientation.
2. **`EnsureLibrary()`**, called at the top of the hover part of `OnRealtimeTick` (after `PollJob()`):
   - If `library_key != SpecKey()`, detach any running library thread (as `DetachJob` does), then start a new job on a thread named `"ottd:junctionlib"` with `spec = MakeJunctionSpec(railtype, ml_gap, branch_gap, tl)`, and set `library_key`.
   - When a job completes (`done`), join it.
   - The library is usable when `library_job && library_job->done`.
   - Also start it in `Regenerate()`, via `EnsureLibrary()`.
   - The cache clear after a finished build (map changed) doesn't need a new library: the library is flat.
3. **Instant tier in `ShowHover()`:**
   - If the refine cache has the key, show it (unchanged).
   - Otherwise, if the library is usable:
     - `ml_gap == 0`: `ShowNoPlacement(STR_JUNCTION_ERROR_NEEDS_ML_GAP)`;
     - otherwise: `terrain = MakeJunctionTerrain(hover_tile, rotation, effective mirror, 4)`, `r = FitJunctionLibraryNear(library, terrain, rotation, mirror)`. If r has a layout, `ShowResult(r)`; otherwise `ShowNoPlacement(STR_JUNCTION_STATUS_GENERATING)`, since the refine tier may still find one.
   - Otherwise (no library yet), `ShowNoPlacement(STR_JUNCTION_STATUS_GENERATING)` as today.
   - `ShowHover` is called on tile change (already) and from `PollJob` (already). Also call it once when the library first becomes usable while hovering.
4. **Refine job** (`StartJob`):
   - The job also gets a copy of the library (a `std::shared_ptr<const std::vector<JunctionLibraryEntry>>` member `library` in `JunctionGenerateJob`, which may be null). Make `JunctionLibraryJob::library` shareable with the smallest change: store it as `std::shared_ptr<const std::vector<JunctionLibraryEntry>>` from the start.
   - In the worker, after `full = GenerateJunctionNear(...)`: if a library is present and `!cancel`, compute `fit = FitJunctionLibraryNear(*library, terrain, rotation, mirror)`.
   - Then choose, in this order:
     1. whichever has a layout;
     2. the smaller Chebyshev offset distance;
     3. the lower `(score.first, score.second, area)`, where area = layout->size.x * layout->size.y;
     4. on a full tie, `fit`, so the ghost doesn't flicker.
   - Store the chosen one in `job->result`.
5. Hover rest, cancel, cache size and keys stay unchanged.
6. **Time the instant tier:**
   - in `ShowHover`, measure `MakeJunctionTerrain` plus `FitJunctionLibraryNear` with `std::chrono::steady_clock`;
   - print it with `Debug(misc, 3, "junction instant tier: snapshot {} us, fit {} us", ...)`, using the existing `Debug` macro (`debug.h`).

### 3.4 Console and rig
- `junction_export`'s last argument `near` accepts 2: build the library from the spec, use the margin-4 snapshot, and run `FitJunctionLibraryNear`.
  - The JSON is as for near 1, plus `"tier":"instant"`.
  - The printed line adds `snapshot_us=` and `fit_us=`.
  - Update the help text.
- `ottd`: `junction-here` and `junction-scan` take `--instant`, which passes 2 (and implies near).

### 3.5 Run (the engineer may watch; print the join command)
1. Rebuild `build/openttd` (Debug, for the rig's server).
2. `ottd start --hilly`, then `ottd company`.
3. `ottd junction-scan --count 12 --seed 1 --instant`. Every generated layout must build with 0 failures.
4. Report the totals, and for comparison the T6 numbers already measured with `--near`: 5 generated, 5 built.

## 4. Tests

1. **`FitJunctionLibraryNear`** (in `src/tests/junction_game.cpp`, next to the GenerateJunctionNear tests, using their terrain helpers):
   - **Clear terrain** (margin 4): offset (0,0), and the layout equals the library entry chosen by `FitJunctionLibrary` on the unshifted terrain.
   - **The 6×6 block** used by the GenerateJunctionNear blocked-spot test: report whether it gives a fit, and at which offset. Assert only that a returned layout's tiles are all Buildable in the shifted terrain.
   - **All Blocked:** NoLayout, offset (0,0).
2. **`GenerateJunction` sets `score`:** for the test spec, `result.score == ScoreJunctionLayout(*result.layout, spec)` on flat ground with no terrain.
3. **GUI:** this is not unit-testable. The rig run in 3.5 covers the console path. The engineer checks the window by hand. List in your report what they should look for:
   - the ghost appears immediately on moving;
   - it may change after a 300 ms rest;
   - no ghost where nothing fits.

## 5. Do not

- change the hover rest time, the cache size or the near offsets
- change the router, signal pass or checker
- add a thread pool (that's P4)

## 6. Stop conditions

- A generated layout fails to build in the scan.
- `MakeJunctionTerrain` must run on the main thread (it reads the map), so it runs in `ShowHover` there. If you find it can't run there, stop.
- Anything not covered.

## 7. Gates

1. `cmake --build build-rel --target openttd openttd_test > p6/rel.log 2>&1`, then the Release suite, all passing.
2. `cmake --build build --target openttd openttd_test > p6/dbg.log 2>&1`: no errors or warnings in the files you touched.
3. Golden: `[.golden]` before and after, identical (`cmp`).
4. The rig run.

Commit message: `Feature: Junction ghost follows the cursor using a candidate library`. Signing is off; end the message with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 8. Report

- commit
- red evidence
- types, members and functions added
- suite totals
- cmp result
- scan output and totals
- snapshot µs and fit µs (from the console line, Debug server)
- the manual checks for the engineer
- stops
- `git status --porcelain` and `git log -1`

When you have reported, stop.
