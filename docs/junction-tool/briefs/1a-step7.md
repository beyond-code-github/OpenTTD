# Brief: 1a step 7, ghost store and viewport hook

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- Repo `/home/pete/OpenTTD` (the main checkout), branch `feature/junction-tool`, head `{HEAD}`. No other agent is working in this checkout during this step.
- Plan: `docs/junction-tool/PLAN-1a.md`, "Ghost store and viewport hook" and "Steps" item 7. This brief is authoritative where they differ. In particular, **there is no culling change in this step**; manual check 6 will show whether one is needed (D42).
- Already on this branch:
  - `src/junction_layout.h`/`.cpp`: the layout, transforms, build steps and estimate.
  - `src/junction_game.h`/`.cpp`: `GameJunctionPricing` and `ChooseJunctionBridgeType`.
  - `src/junction_ghost_draw.h`/`.cpp`, from step 6:
    - `SetViewportGhostPalette` (declared in `viewport_func.h`)
    - `DrawRailTrackGhost(TileInfo *, TrackBits, RailType)`
    - `DrawRailSignalGhost(TileIndex, RailType, Trackdir, SignalType, SignalVariant)`
    - `DrawBridgeHeadGhost(TileInfo *, BridgeType, RailType, DiagDirection)`
    - `DrawBridgeMiddleGhost(const TileInfo *, BridgeType, RailType, Axis, int bridge_z, uint piece_north, uint piece_south)`
- Baseline: the full suite `{BASELINE}`, all passing.
- Rebase-friendliness (D42): upstream edits are limited to what section 4 lists.

## 2. Read first, once each (really read them)

- `CODINGSTYLE.md`, in full.
- `src/junction_layout.h`, `src/junction_ghost_draw.h` and `src/junction_game.h`.
- `src/viewport.cpp`: lines 910–940 (`DrawTileSelectionRect`) and 1115–1180 (`DrawTileSelection`). Also `src/viewport_func.h` lines 80–100 (`MarkTileDirtyByTile`).
- `src/tunnelbridge_cmd.cpp`: in `CmdBuildBridge`, how the direction of each head is set (search for `DiagdirBetweenTileXY` or similar, and `MakeRailBridgeRamp`). Also `src/bridge_map.cpp` `GetBridgeHeight`.
- `src/tests/junction_layout.cpp`, the first 40 lines, for style.

## 3. Tests (write first, see them fail)

Create `src/tests/junction_gui.cpp` and register it in `src/tests/CMakeLists.txt` alphabetically, after `junction_game.cpp`. Every test calls `Map::Allocate(64, 64)`, then `JunctionGhost::Clear()`, then works with the test junction at origin `TileXY(10, 10)`, ground height 1, `RAILTYPE_RAIL`, bridge types `{BridgeType{0}}`, and an empty `JunctionEstimate{}`, unless stated otherwise:

1. **`"JunctionGhost: test junction tiles"`:**
   - `Get(TileXY(10, 12))->tracks == TrackBits{Track::X}`
   - `Get(TileXY(14, 15))->tracks == TrackBits{Track::Y}`
   - `Get(TileXY(14, 12))` has track X and a span with `axis == Axis::Y`, `bridge_z == 2 * TILE_HEIGHT`, `piece_north == 2`, `piece_south == 2` and `type == 0`
   - `Get(TileXY(14, 11))` has a span with `piece_north == 1` and `piece_south == 3`, and an empty `tracks`
   - `Get(TileXY(14, 10))` has a head with `dir == DiagDirection::SE`
   - `Get(TileXY(14, 14))` has a head with `dir == DiagDirection::NW`
   - `Get(TileXY(11, 12))->signals` has one entry with facing `Trackdir::X_SW`
   - `Get(TileXY(16, 12))->signals` has one entry with facing `Trackdir::X_NE`
   - `Get(TileXY(9, 9)) == nullptr`
   - no tile has `bad` set
   - `GetRailType() == RAILTYPE_RAIL`
2. **`"JunctionGhost: bad tiles from the estimate"`:** with `estimate.bad_tiles = {TileXY(12, 12), INVALID_TILE}`, `Get(TileXY(12, 12))->bad` is true, `Get(TileXY(13, 12))->bad` is false, and nothing crashes.
3. **`"JunctionGhost: bridge without a type"`:** with bridge types `{std::nullopt}`, both head tiles exist with `bad == true` and no `head`. The span tiles have no `span`, and (14,12) still has its track.
4. **`"JunctionGhost: clear empties the store"`:** after `Set(...)` and then `Clear()`, `Get(TileXY(10, 12)) == nullptr`.

**Red procedure:** add the header and a `.cpp` whose functions are stubs (`Get` returns `nullptr`, `Set` does nothing). Build, run `./build/openttd_test "JunctionGhost:*"`, and record the failures. Guard pointer checks with `REQUIRE(ptr != nullptr)`, so the red is a clean assertion failure, not a crash. Test 4 may pass against the stub; that's acceptable.

If `MarkTileDirtyByTile` or `MarkBridgeDirty` crashes or asserts in the test environment (no viewports), stop and report.

## 4. Changes

### `src/junction_gui.h` (new, guard `JUNCTION_GUI_H`)

```cpp
#include "junction_layout.h"
#include "bridge_type.h"
#include "rail_type.h"
#include "direction_type.h"

/** Ghost of a bridge head on one tile. */
struct JunctionGhostBridgeHead {
	BridgeType type;
	DiagDirection dir; ///< Direction from this head towards the other head.
};

/** Ghost of a bridge span over one tile. */
struct JunctionGhostBridgeSpan {
	BridgeType type;
	Axis axis;
	int bridge_z; ///< Deck height in pixels.
	uint piece_north; ///< Tiles from this tile to the northern head.
	uint piece_south; ///< Tiles from this tile to the southern head.
};

/** Everything the ghost draws on one tile. */
struct JunctionGhostTile {
	TrackBits tracks{};
	std::vector<JunctionSignalStep> signals;
	std::optional<JunctionGhostBridgeHead> head;
	std::optional<JunctionGhostBridgeSpan> span;
	bool bad = false; ///< Draw in red: the estimate found this tile unbuildable.
};

/** Client-side store of the junction placement ghost. */
namespace JunctionGhost {
	void Set(const JunctionLayout &layout, TileIndex origin, int height, RailType railtype, const std::vector<std::optional<BridgeType>> &bridge_types, const JunctionEstimate &estimate);
	void Clear();
	const JunctionGhostTile *Get(TileIndex tile);
	RailType GetRailType();
}

struct TileInfo;
void DrawJunctionGhost(const TileInfo *ti);
```

Each function gets a doc comment in the .cpp. `height` is the junction's ground height in height levels. `bridge_types` gives the chosen type per layout bridge, in order, with `std::nullopt` where none qualifies.

### `src/junction_gui.cpp` (new)

Use the licence header, `@file`, `stdafx.h` first and `safeguards.h` last. It holds file-static storage: `std::unordered_map<TileIndex, JunctionGhostTile>` and a `RailType`.

**`Set`:**
1. Call `Clear()`. This marks the old tiles dirty.
2. Store `railtype`.
3. For each `JunctionTrack`: the tile is `AddTileIndexDiffCWrap(origin, offset)`. Skip it if invalid. Otherwise OR in the tracks.
4. For each `JunctionSignal`: if the tile is valid, append a `JunctionSignalStep{tile, facing, type, variant}`.
5. For each bridge `i`, with `S = start` and `E = end` translated (skip the bridge if either is invalid):
   - Take the type from `bridge_types[i]`, or `std::nullopt` if `i` is out of range.
   - If there's no type: set `bad` on S and E (creating entries as needed) and add no head or span.
   - Otherwise:
     - `axis = TileX(S) == TileX(E) ? Axis::Y : Axis::X`
     - S's head `dir` is SE for `Axis::Y` and SW for `Axis::X`. E's `dir` is the reverse. Confirm this matches how `CmdBuildBridge` sets head directions. If it doesn't, stop.
     - For each tile strictly between S and E, at distance `d` from S and `len + 1 - d` from E (where `len` is the number of middle tiles), add a span: `{type, axis, (height + 1) * TILE_HEIGHT, d, len + 1 - d}`.
6. For each tile in `estimate.bad_tiles` other than `INVALID_TILE`: set `bad` (creating an entry if needed).
7. Mark every tile in the store dirty: `MarkTileDirtyByTile(tile)`, plus `MarkTileDirtyByTile(tile, 1)` for span tiles, since the deck is one level above the ground.

**`Clear`:** mark every stored tile dirty (as in rule 7), then empty the store.

**`Get`:** a pointer into the map, or `nullptr`.

**`DrawJunctionGhost(const TileInfo *ti)`:**
1. `ghost = Get(ti->tile)`. If it's `nullptr`, return.
2. If `ti->tileh != SLOPE_FLAT`: call `SetViewportGhostPalette(PALETTE_SEL_TILE_RED)`, then `DrawTileSelectionRect(ti, PALETTE_SEL_TILE_RED)`, then `SetViewportGhostPalette(PAL_NONE)`, then return. This is D40: the terraform tool's red outline at ghost opacity.
3. Otherwise call `SetViewportGhostPalette(ghost->bad ? PALETTE_TO_STRUCT_RED : PALETTE_TO_TRANSPARENT)`, and make a local copy `TileInfo copy = *ti;` for the functions taking a non-const pointer. Then:
   - if `ghost->tracks.Any()`: `DrawRailTrackGhost(&copy, ghost->tracks, GetRailType())`
   - if `head`: `DrawBridgeHeadGhost(&copy, head->type, GetRailType(), head->dir)`
   - if `span`: `DrawBridgeMiddleGhost(ti, span->type, GetRailType(), span->axis, span->bridge_z, span->piece_north, span->piece_south)`
   - for each signal: `DrawRailSignalGhost(ti->tile, GetRailType(), s.facing, s.type, s.variant)`
   - finally `SetViewportGhostPalette(PAL_NONE)`.

### Upstream edits (`src/viewport.cpp`), split across the two commits

- **Commit 1** (`Codechange: Make DrawTileSelectionRect available to the junction ghost`): remove `static` from `DrawTileSelectionRect`, and declare it in `src/junction_ghost_draw.h` as `void DrawTileSelectionRect(const TileInfo *ti, PaletteID pal); // Defined in viewport.cpp.`. Nothing else. It must build.
- **Commit 2** (`Add: Junction placement ghost store and viewport hook`), together with the new files:
  - Add `#include "junction_gui.h"` to `viewport.cpp`'s include block (keep the alphabetical position if the block is sorted).
  - Add `DrawJunctionGhost(ti);` as the **last statement** of `DrawTileSelection`. If the function has early `return`s that would skip it, stop and report rather than restructuring.

  The hook draws nothing while the store is empty.

### CMake

Add `junction_gui.cpp` and `junction_gui.h` to `src/CMakeLists.txt` alphabetically, after `junction_ghost_draw.h`.

## 5. Do not

- Add the window, toolbar button, hover or tooltip code. That's step 8.
- Change viewport culling.
- Touch upstream files other than `viewport.cpp` and the two `CMakeLists.txt` files.
- Unit-test `DrawJunctionGhost`. It's drawing, checked in game.

## 6. Stop conditions

- A named function or declaration doesn't exist as described.
- The bridge head direction convention differs from rule 5.
- The dirty-marking functions crash in tests.
- `DrawTileSelection` has early returns before its end.
- Anything not covered here.

## 7. Gates

Logs go to `.../s7_*.log`.

1. Build `openttd_test` and `openttd` with no errors and no warnings.
2. The red run is recorded, then all four new tests pass.
3. The full suite is `{BASELINE}` plus 4, all passing.
4. Two commits, as described under "Upstream edits": commit 1 is `viewport.cpp` and `junction_ghost_draw.h` only; commit 2 is the new files, CMake, tests and the `viewport.cpp` hook. Each ends with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.
5. Report `git diff --stat {HEAD}..HEAD -- src/viewport.cpp`.

## 8. Report format

- **References read**
- **Commits**
- **Red evidence**
- **Types and members added or removed**
- **Upstream diff size**
- **Suite results**
- **Manual checks:** none (the engineer checks the ghost in game after step 8)
- **Stops, and anything left undone**
- **State:** `git status --porcelain` and `git log -2 --format='%h %s'`

When you have reported, stop.
