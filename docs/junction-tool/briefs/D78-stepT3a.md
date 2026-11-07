# Brief D78 step T3a: orientation mapping, hub offset, prio terrain check

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- **Repo:** `/home/pete/OpenTTD`, branch `feature/junction-tool`, head `{HEAD}`. It includes T1/T2: `JunctionTerrain`, router terrain, and `GenerateJunction(spec, terrain)`.
- **Plan:** `docs/junction-tool/PLAN-D78.md` (D93), point 6 (anchor = hub point, working coordinate (30,0)).

## 2. Read first

- `src/junction_layout.h/.cpp`: `TransformOffset` (l.86), `TransformJunctionLayout`, `ApplyJunctionOrientation`
- `src/junction_generator.h/.cpp`: `BuildJunctionCandidate` (the crop, the two `NormaliseJunctionLayout` calls, `PlaceJunctionSignals`, the terrain check after the crop), `GenerateJunction`, `JunctionGenerateResult`
- `src/junction_route.cpp`: `NormaliseJunctionLayout` (how it shifts)
- `src/junction_terrain.h`

## 3. Changes

### 3.1 `junction_layout.h/.cpp`

```cpp
/** Where a layout offset lands after ApplyJunctionOrientation(layout of the given size, rotation, mirrored). */
TileIndexDiffC OrientJunctionOffset(TileIndexDiffC offset, TileIndexDiffC size, uint8_t rotation, bool mirrored);
/** The map offset of a vector in the canonical frame, under an orientation: mirror (dx,dy)->(-dx,dy) if set, then rotation times (dx,dy)->(dy,-dx). */
TileIndexDiffC OrientJunctionVector(TileIndexDiffC v, uint8_t rotation, bool mirrored);
```

`OrientJunctionOffset` applies `TransformOffset` exactly as `ApplyJunctionOrientation` does, updating the size after each step.

### 3.2 Generator
1. Add `TileIndexDiffC hub{};` to `JunctionGenerateResult`: the offset, in the returned layout, of working coordinate (30, 0). Document it.
2. `BuildJunctionCandidate` gets another optional out-parameter, `TileIndexDiffC *hub = nullptr`, filled with the same thing for the returned candidate.
   - Record the shift each `NormaliseJunctionLayout` applies (the min x/y before the call) and sum them.
   - Put it after `terrain_cost`.
3. **Prio terrain check.** When `terrain` is set, after `PlaceJunctionSignals` and the second normalise, map every track tile of the final layout back to working coordinates (subtract the summed shift). Each must pass `terrain->UsableAt(x, y, plane)`, otherwise return `std::nullopt`. This now includes the prio rows. Bridge checks stay where they are.
4. `GenerateJunction` copies the winner's hub into the result.

## 4. Tests

1. **`junction_layout` tests:**
   - For a small asymmetric layout (3 tracks at distinct offsets, size 5×3) and all 8 (rotation, mirrored) combinations, `OrientJunctionOffset(offset, size, r, m)` equals the offset the same track has in `ApplyJunctionOrientation(layout, r, m)`.
   - For the same cases, `OrientJunctionOffset(a) − OrientJunctionOffset(b)` equals `OrientJunctionVector(a − b)` for every pair of the layout's offsets.
2. **Generator:**
   - `GenerateJunction(MakeTestJunctionSpec())` returns a hub inside the layout's size, and the layout's tile at the hub holds `Track::X`, because working (30,0) is on the E track.
   - The tile at (hub.x, hub.y + g + 1) also holds `Track::X` or is under a bridge, because it is the W track's row at the same x.
   - With a flat terrain whose prio-row line (working row −1) is Blocked on x 0..63, the result is `NoLayout` or a layout with no track on that row. Assert that no track sits on (hub.y − 1) rows within the prio's x range, or `NoLayout`.

## 5. Do not

- change the router, the checker, the signal pass or the game side
- change `JunctionLayout`

## 6. Stop conditions

- `NormaliseJunctionLayout`'s shift can't be determined from outside without changing it.
- Anything not covered.

## 7. Gates

1. `cmake --build build --target openttd openttd_test > /tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/t3a_build.log 2>&1`. There must be no errors or warnings in junction files.
2. Full suite. The baseline is reported at launch; everything passes.

Commit message: `Add: Junction orientation helpers and generator hub offset`. Signing is off; end the message with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 8. Report

- commit
- red evidence
- functions added
- suite totals
- stops
- `git status --porcelain` and `git log -1`

When you have reported, stop.
