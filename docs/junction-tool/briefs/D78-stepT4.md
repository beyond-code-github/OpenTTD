# Brief D78 step T4: generate at the hovered spot on a worker thread

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- **Repo:** `/home/pete/OpenTTD`, branch `feature/junction-tool`, head `{HEAD}`.
- **What exists already:**
  - T3b: `MakeJunctionTerrain(anchor, rotation, mirrored)` and the plane rule in `EstimateJunction`.
  - T3a: `OrientJunctionOffset`, `OrientJunctionVector`, and `JunctionGenerateResult::hub`.
  - E: the window generates synchronously when its parameters change (D92).
- **Plan:** `docs/junction-tool/PLAN-D78.md`, points 6 and 7 (D93).

## 2. Read first

- `src/junction_gui.cpp` (all; it is about 700 lines)
- `src/junction_game.h/.cpp`: `MakeJunctionSpec`, `MakeJunctionTerrain`, `ChooseJunctionBridgeType`, the helper `GetMaxBuildableEngineSpeed`
- `src/junction_layout.h`: the orientation helpers
- `src/thread.h`: `StartNewThread`

## 3. Changes

### 3.1 Thread-safe spec (`junction_game.cpp`)
`MakeJunctionSpec` must not leave closures that read game state:
- Precompute on the calling (main) thread, for n = 1..10:
  - `ok[n]` (the current `bridge_span_ok` result);
  - `price[n]` (the current `prices.bridge(n)` result).
- The lambdas then capture those `std::array`s by value. For n outside 1..10: not OK, and a very large price.
- Behaviour for n in 1..10 is unchanged. The test spec's `max_bridge_middle` is 10, the same as the generator's.

### 3.2 Window (`junction_gui.cpp`)

**Remove** the synchronous `Regenerate()` generation and the `generated` member. Parameter changes now only:
- validate the notations (setting `generate_error` for invalid or multi-lane);
- save the settings;
- clear the cache;
- reset the hover state.

**New members:**
- **Cache:** an LRU of up to 32 entries. Each maps a key (anchor tile, rotation, effective mirror, ml_gap, branch_gap, tl, signal type, sd) to the result (`JunctionGenerateResult`). A `std::vector` with move-to-front is fine.
- **One pending job:** its key, a `std::thread`, and a `std::shared_ptr<JobState>`. The job struct holds `std::atomic<bool> done`, the result, and the spec and terrain it owns. Name it `JunctionGenerateJob` and define it in `junction_gui.cpp`.
- **Hover rest:** the tile, plus the time (from `std::chrono::steady_clock`) when the hover reached it.

**Effective mirror:** `mirrored XOR (road_side is left)`. Rotation is the window's rotation.

**In `OnRealtimeTick` (hover branch):**
1. When the hover tile changes, record the tile and the time. If the cache has the key, use the cached result at once.
2. If the hover has rested for 300 ms and there is no cache entry:
   - If no job is pending, or the pending job's key differs, then on the main thread build `spec = MakeJunctionSpec(...)` and `terrain = MakeJunctionTerrain(anchor, rotation, effective mirror)`.
   - Start a job with `StartNewThread` that runs `GenerateJunction(spec, &terrain)` and sets `done`.
   - If a job with a different key is still running, let it finish: detach it, and the shared state keeps it alive. Ignore its result.
3. When the pending job is `done`:
   - Join it.
   - Store the result in the cache.
   - If its key matches the current hover, use it.

**Using a result:**
- If it has a layout:
  - `oriented = ApplyJunctionOrientation(*layout, rotation, effective mirror)`;
  - `origin = anchor − OrientJunctionOffset(hub, layout->size, rotation, effective mirror)`, as a TileIndex with bounds checks (off-map gives an error state);
  - run the existing `ComputePlacement`/ghost/estimate path with `oriented` and `origin` (a placement at `origin` rather than at the hover tile).
- If it has no layout: no ghost, and the status shows the error string. Add `STR_JUNCTION_ERROR_NO_LAYOUT_HERE` "No junction fits here".
- **While waiting:** no ghost; the status shows `STR_JUNCTION_STATUS_GENERATING` "Working out a junction for this spot…"; `SetTileSelectSize(1,1)`.

**Build:**
- A map click builds the placement currently shown: its oriented layout and origin.
- With no current result for the clicked tile, do nothing.
- `StartBuild` takes the origin and the oriented layout instead of recomputing from the tile.

**Close:** detach any running job (the shared state keeps it safe) and clear the ghost as today.

### 3.3 Strings
- the two above
- update the window's name line if it mentioned "generated" timing

## 4. Tests

Unit-test only the pure part. In `src/tests/junction_game.cpp`, `MakeJunctionSpec`'s `bridge_span_ok` and `prices.bridge` return the same values before and after the precompute for n = 1..10 on the test setup. If bridges can't be set up, say so and skip.

**Manual checks (for the engineer):** append them to `docs/junction-tool/CHECKS-1b.md` under "D78 T4":
1. Hovering over flat land shows "Working out…", then the ghost within about a second, anchored with the hub at the cursor.
2. Hovering over hills or water shows an adapted layout or "No junction fits here".
3. Moving the mouse around causes no freeze.
4. Rotate and mirror re-generate for the new orientation.
5. The building matches the ghost.
6. Left-hand road side: mirrored correctly.

## 5. Do not

- change the generator, router, checker or signal pass
- touch the savegame or settings beyond removing nothing
- let the worker touch game state (no map, engine, bridge or settings reads in `GenerateJunction`'s call graph)

## 6. Stop conditions

- `StartNewThread` doesn't fit.
- `ComputePlacement` can't take an origin without broader changes.
- Anything not covered.

## 7. Gates

1. `cmake --build build --target openttd openttd_test > /tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/t4_build.log 2>&1`. There must be no errors or warnings in the files you touched.
2. Full suite, all passing.
3. Also build `build-rel` for `openttd` (`cmake --build build-rel --target openttd`), so the engineer can test at Release speed.

Commit message: `Feature: Junction window generates for the hovered spot in the background`. Signing is off; end the message with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 8. Report

- commit
- red evidence
- members and functions
- suite totals
- stops
- `git status --porcelain` and `git log -1`

When you have reported, stop.
