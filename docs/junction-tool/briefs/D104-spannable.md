# Brief D104: bridges may span existing rail and road

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- **Repo:** `/home/pete/OpenTTD`, branch `feature/junction-tool`, head `{HEAD}`. The Release suite passes.
- **Decision:** D104 in `docs/junction-tool/DECISIONS.md`.
- **Scratchpad:** `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/d104/`
- **Don't run any `tools/ottd` command.**

## 2. Read first

- `src/junction_terrain.h`: `JunctionTileClass`, `UsableAt`, `CostAt`, `SpanOk`.
- `src/junction_game.cpp`: `MakeJunctionTerrain`, the tile-type switch (about l.248–262).
- `src/rail_map.h`: `IsPlainRail`.
- `src/road_map.h`: `IsNormalRoad`, or the equivalent "plain road, not depot or crossing" test. Choose the function that means a road tile without a depot; level crossings are rail-plus-road and may also be spanned.
- Grep `JunctionTileClass::Water` everywhere, and check each use.

## 3. Changes

1. `JunctionTileClass` gets `Spannable, ///< Plain rail or road of any owner: no track on it, but a bridge span may pass over it (D104).`, after `Water`. Update `Blocked`'s comment to drop "own rail (1st cut)".
2. In `MakeJunctionTerrain`'s switch:
   - `TileType::Railway` with `IsPlainRail(tile)` → `Spannable`;
   - `TileType::Road` that is a normal road or a level crossing (not a depot) → `Spannable`.
3. For every other place that handles `Water` specially (from your grep), decide whether `Spannable` needs the same handling. It does wherever the code means "not buildable but may be bridged". If a place's meaning is unclear, stop and report it.
4. `SpanOk` already rejects only `Blocked`, so it needs no change. Confirm that.

## 4. Tests (`src/tests/junction_generator.cpp` or `junction_game.cpp`, beside the terrain tests); red first

1. **New: "a bridge may span existing track".**
   - Take the flat terrain fixture and mark a 1-tile-wide strip `Spannable` across the doubled bridges' span region, but not under the heads.
   - Choose the strip from the flat layout: under the middle of one doubled bridge, in working coordinates, crossing both lane rows and the W row between them. Use the flat result's hub to map coordinates, as the other terrain tests do.
   - `GenerateJunction(spec, &terrain)` must give a layout.
   - Red: with the same strip marked `Blocked` it gives NoLayout or a different layout. Use `Blocked` in the red run, since `Spannable` doesn't exist yet: record that the Blocked strip prevents the original layout, then switch to `Spannable` and show it fits.
2. **New: "track may not use a Spannable tile":** `UsableAt` is false on a `Spannable` tile, and `SpanOk` is true at a plane above it.
3. Release suite, all passing.

## 5. Gates

1. `cmake --build build-rel --target openttd openttd_test > d104/rel.log 2>&1`, then the Release suite, all passing.
2. `cmake --build build --target openttd openttd_test > d104/dbg.log 2>&1`: no errors or warnings in the files you touched.

Commit message: `Change: Junction bridges may span existing rail and road`. Signing is off; end the message with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 6. Report

- commit
- red evidence
- each Water use you found, and what you did with it
- suite totals
- stops
- `git status --porcelain` and `git log -1`

When you have reported, stop.
