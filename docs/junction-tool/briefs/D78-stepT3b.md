# Brief D78 step T3b: map terrain snapshot and the plane rule in the estimate

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- **Repo:** `/home/pete/OpenTTD`, branch `feature/junction-tool`, head `{HEAD}`. It includes T3a: `OrientJunctionVector`, `OrientJunctionOffset`, and `JunctionGenerateResult::hub`.
- **Plan:** `docs/junction-tool/PLAN-D78.md`, points 1, 2, 4 and 8. **Facts:** `docs/junction-tool/RECON-terrain.md` (read it all).

## 2. Read first

- `src/junction_terrain.h`
- `src/junction_game.h/.cpp`: `GameJunctionPricing` (`GetFlatHeight`, `TestClear`)
- `src/junction_layout.h`: the `JunctionPricing` interface and `EstimateJunction`
- `src/junction_layout.cpp`: `EstimateJunction` (l.195–296)
- `src/tests/junction_game.cpp`: how it sets up a map
- `src/tests/junction_layout.cpp`: its fake pricing

## 3. Changes

### 3.1 `junction_game.h/.cpp`

```cpp
/** Snapshot the map around an anchor for the generator, in its working frame (D93): cell (cx, cy) is map tile anchor + OrientJunctionVector({cx - 30, cy}, rotation, mirrored). */
JunctionTerrain MakeJunctionTerrain(TileIndex anchor, uint8_t rotation, bool mirrored);
```

- **Grid:** origin (0, −2), width 64, height 43 (working x 0..63, y −2..40).
- **Each cell:**
  - Off-map, or a void tile: Blocked.
  - `z_min`/`z_max` from `GetTileSlopeZ` and `GetTileMaxZ`; `steep` from `IsSteepSlope`.
  - Class by tile type:

    | Tile type | Class |
    |---|---|
    | Clear | Buildable |
    | Trees | Buildable |
    | Water (sea, canal, river, and coast) | Water |
    | Anything else (house, industry, object, station, road, railway, tunnel/bridge) | Blocked |

  - `clear_cost` for Buildable tiles: the cost from the same `LandscapeClear` QueryCost call that `GameJunctionPricing::TestClear` makes. Reuse it: construct a `GameJunctionPricing` (it needs a rail type; take it as a parameter if needed, and say so) or factor the call into a static helper. If the call fails, mark the tile Blocked.
- **Terrain-wide fields:**
  - `foundation_cost` = `_price[Price::BuildFoundation]`
  - `build_on_slopes` = `_settings_game.construction.build_on_slopes`
  - `max_bridge_height` = `_settings_game.construction.max_bridge_height`

### 3.2 The `EstimateJunction` plane rule (`junction_layout.h/.cpp`, `junction_game.cpp`)
1. **`JunctionPricing`:** replace `GetFlatHeight(tile)` with:

   ```cpp
   /** Slope facts of a tile, or nullopt off the map. */
   virtual std::optional<JunctionTileSlope> GetTileSlope(TileIndex tile) const = 0;
   ```

   with `struct JunctionTileSlope { int z_min; int z_max; bool steep; bool build_on_slopes; };` in `junction_layout.h`. Update `GameJunctionPricing` and every fake in the tests.
2. **`EstimateJunction`:**
   - H = `z_max` of the first occupied track tile (in the existing iteration order).
   - Every occupied track tile and bridge head must be non-steep with `z_max == H`, and either flat (`z_min == z_max`) or `build_on_slopes`.
   - Failures go into `bad_tiles` with the existing `STR_ERROR_FLAT_LAND_REQUIRED`.
   - `height` = H.
   - Span tiles stay as they are (`PriceBridge` validates them).
3. **Where `GetFlatHeight` had other callers** (the ghost height, for instance), keep their behaviour, using `z_min == z_max ? z : nullopt` from the new call.

## 4. Tests

1. **`junction_game`:** on the test map (as its existing tests do), with a raised flat tile and a one-corner-raised tile, `MakeJunctionTerrain` gives the right `z_min`/`z_max`/`steep` at the cells that map to them, for rotation 0 unmirrored and for rotation 1 mirrored. Check the class of a clear tile (Buildable). Add water or a house only if the existing test map makes that easy; say which you covered.
2. **`junction_layout`** (fake pricing):
   - a layout on tiles flat at 0 is accepted (as before);
   - one tile with z_min 0 and z_max 1 when every other tile is flat at 1 is accepted, with `height` = 1;
   - the same with build_on_slopes off is rejected at that tile;
   - a steep tile is rejected;
   - mixed planes (a tile flat at 0 while H = 1) are rejected.

## 5. Do not

- change the generator, router, checker or GUI
- change the savegame (D69)

## 6. Stop conditions

- A named game API differs (`IsSteepSlope`, the tile-type enum names, `max_bridge_height`).
- `GetFlatHeight`'s callers can't keep their behaviour.
- Anything not covered.

## 7. Gates

1. `cmake --build build --target openttd openttd_test > /tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/t3b_build.log 2>&1`. There must be no errors or warnings in the files you touched.
2. Full suite, all passing.

Commit message: `Add: Junction terrain snapshot from the map, plane rule in the estimate`. Signing is off; end the message with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 8. Report

- commit
- red evidence
- functions added and changed
- suite totals
- stops
- `git status --porcelain` and `git log -1`

When you have reported, stop.
