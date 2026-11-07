# Brief D78 step T6: search near the cursor when the spot doesn't fit

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- **Repo:** `/home/pete/OpenTTD`, branch `feature/junction-tool`, head `{HEAD}`. The suite has 230 cases, all passing.
- **D78 T1–T5 are done.** On the hilly rig seed, only 3 of 12 random spots had a layout (`docs/junction-tool/PLAN-D78.md`, Progress). Lever (a) is to try nearby anchors.

## 2. Read first

- `src/junction_layout.h/.cpp`: `OrientJunctionVector`, `OrientJunctionOffset`
- `src/junction_terrain.h`
- `src/junction_game.h/.cpp`: `MakeJunctionTerrain`
- `src/junction_generator.h`: `GenerateJunction`
- `src/junction_gui.cpp`: the job struct, `StartJob`, `PollJob`, `ShowResult`
- `src/console_cmds.cpp`: `junction_export`
- `tools/ottd/ottd`: `junction-here`, `junction-scan`

## 3. Changes

### 3.1 `junction_layout.h/.cpp`

```cpp
/** Inverse of OrientJunctionVector: rotate back (dx,dy)->(-dy,dx) 'rotation' times, then mirror (dx,dy)->(-dx,dy) if set. */
TileIndexDiffC InverseOrientJunctionVector(TileIndexDiffC v, uint8_t rotation, bool mirrored);
```

### 3.2 `junction_game.h/.cpp`
- `MakeJunctionTerrain` gets a trailing `int margin = 0`. The grid becomes origin (−margin, −2 − margin), width 64 + 2·margin, height 43 + 2·margin. Everything else is unchanged.
- New pure function, declared in `junction_game.h` (it reads no game state):

  ```cpp
  /** Result of a search for a fitting spot near the anchor. */
  struct JunctionNearResult {
  	JunctionGenerateResult result;
  	TileIndexDiffC offset{}; ///< Map offset from the requested anchor to the anchor that fits.
  };
  /**
   * Try the anchor, then nearby anchors, using one snapshot taken with a margin of at least the largest offset.
   * Trying map offset d uses the same snapshot with origin shifted by -InverseOrientJunctionVector(d, rotation, mirrored).
   * Stops at the first layout found, or when cancel becomes true.
   */
  JunctionNearResult GenerateJunctionNear(const JunctionSpec &spec, const JunctionTerrain &terrain, uint8_t rotation, bool mirrored, const std::atomic<bool> &cancel);
  ```

- **Offsets tried, in order:**
  1. (0,0);
  2. the 8 at distance 2: (±2, 0), (0, ±2), (±2, ±2);
  3. the 8 at distance 4.

  That is 17 tries at most. The terrain needs a margin of at least 4.
- Copy the terrain once per try, with only `origin` changed (a struct copy is fine).
- Check `cancel` before each try.
- If nothing fits, return the (0,0) try's result with offset (0,0).

### 3.3 GUI (`junction_gui.cpp`)
- The job builds the snapshot with margin 4 and runs `GenerateJunctionNear`. The job gets `std::atomic<bool> cancel`, set to true when it is detached (the hover moved on, or the window closed).
- `ShowResult` uses anchor + offset as the anchor for placement and origin.
- When offset ≠ (0,0), the status line shows `STR_JUNCTION_STATUS_NEARBY` "Nearest fit shown, {NUM} tiles from the cursor" (the Chebyshev distance).
- The cache key stays the requested anchor, so the cached result keeps its offset.

### 3.4 Console and rig
- `junction_export` gains an optional trailing argument `near` (0 or 1, default 0). With 1 it uses margin 4 and `GenerateJunctionNear` (with a false cancel). The JSON gains `"offset":[dx,dy]`.
- `junction-here` and `junction-scan` gain `--near`. The scan prints the offset for each try.

### 3.5 Run
1. `ottd start --hilly`, then `ottd company`.
2. `ottd junction-scan --count 12 --seed 1`. It should repeat the T5 numbers: 3 generated, 3 built.
3. `ottd junction-scan --count 12 --seed 1 --near`.
4. Report both totals. Every generated layout must build with 0 failures.
5. Print the join command when starting.

## 4. Tests

1. **`InverseOrientJunctionVector(OrientJunctionVector(v))` equals v** for 4 sample vectors × 8 orientations.
2. **`GenerateJunctionNear` on a synthetic terrain** (flat z0 with margin 4, origin and size as `MakeJunctionTerrain` would give):
   - **Clear spot:** unblocked terrain gives offset (0,0) and a layout equal to `GenerateJunction(spec, &terrain)` for the same terrain without the margin. If equality is awkward because of the origin, compare tracks only.
   - **Blocked spot:** Blocked tiles in a 12×12 block covering the working box x 24..35, y 0..11 under rotation 0 give a non-zero offset and a layout. If even distance 4 can't escape that block, use a 6×6 block x 27..32, y 2..7, and say so.
   - **Cancel:** with `cancel` already true, it returns immediately with the (0,0) result or none. Check that it does not try all 17: time it under 2 s in Debug with a terrain that has no fit anywhere (all Blocked).

## 5. Do not

- change the generator, router or checker
- try more than 17 offsets

## 6. Stop conditions

- A generated layout fails to build in the scan. Report and stop.
- Anything not covered.

## 7. Gates

1. `cmake --build build --target openttd openttd_test > /tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/t6_build.log 2>&1`. There must be no errors or warnings in the files you touched.
2. Full suite, all passing.
3. The rig run.
4. `cmake --build build-rel --target openttd`.

Commit message: `Feature: Junction tool finds the nearest spot that fits`. Signing is off; end the message with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 8. Report

- commit
- red evidence
- functions
- suite totals
- both scan outputs
- stops
- `git status --porcelain` and `git log -1`

When you have reported, stop.
