# Brief: 1a step 9g, opaque invalid ghost and pinned footprint

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- Repo `/home/pete/OpenTTD` (main checkout), branch `feature/junction-tool`, head `{HEAD}`. You are the only agent here. A copy of the game (`build/openttd-step9f`) may be running. Don't touch it.
- **Decision D53** in `docs/junction-tool/DECISIONS.md`. The engineer found two problems:
  1. **Invalid ghosts are drawn with `PALETTE_TO_STRUCT_RED` plus the transparent bit,** which hides the track and signals and leaves only the bridge heads visible. This happens after a failed build, and over water.
  2. **After a failure, the game's cursor selection rectangle keeps following the mouse,** while the ghost stays at the build position.
- Baseline: `{BASELINE}`, all passing.
- Rebase-friendliness (D42): no upstream file changes, unless section 6 allows one and you've stopped to ask first.

## 2. Read first, once each

- `CODINGSTYLE.md` (no need to quote a rule).
- `src/junction_gui.h` and `src/junction_gui.cpp`: the `JunctionGhost` store, `DrawJunctionGhost`, and `BuildJunctionWindow::OnRealtimeTick`, `StartBuild` and `Close`.
- `src/tests/junction_gui.cpp`.
- `src/viewport.cpp`: `SetTileSelectSize` (~2622) and `UpdateTileSelection` (~2735–2770). Also `src/tilehighlight_type.h` (`TileHighlightData`). **The goal here:** find how a placement window can stop the cursor's `HT_RECT` rectangle being drawn, without leaving placement mode and without editing upstream files. For example, does `SetTileSelectSize(0, 0)` draw nothing? Or is there an existing `_thd` field or call that suppresses drawing? Report what you find and why it works.

## 3. Tests (write first, see them fail)

Add to `src/tests/junction_gui.cpp`:

1. **`"JunctionGhost: pinned footprint covers the layout's bounding box"`:**
   - `Set(...)` the test junction at `TileXY(10, 10)` (size 8×6), then `JunctionGhost::SetPinned(true)`.
   - Expect `IsInPinnedFootprint(TileXY(10, 10))`, `IsInPinnedFootprint(TileXY(17, 15))`, `!IsInPinnedFootprint(TileXY(18, 10))` and `!IsInPinnedFootprint(TileXY(10, 16))`.
   - After `SetPinned(false)`, `!IsInPinnedFootprint(TileXY(10, 10))`.
2. **`"JunctionGhost: clear unpins"`:** after `SetPinned(true)` and `Clear()`, `!IsInPinnedFootprint(TileXY(10, 10))`.

**Red:** declare the two functions with stub bodies (return false, or do nothing), build, run `./build/openttd_test "JunctionGhost*"`, and record the failures.

## 4. Changes

1. **Ghost store (`junction_gui.h` and `.cpp`):**
   - `Set` also records the footprint: the origin and `layout.size`, kept as a file-static `TileArea` (see `tilearea_type.h`).
   - Add `void SetPinned(bool pinned);` and `bool IsInPinnedFootprint(TileIndex tile);` to the `JunctionGhost` namespace, both documented. `IsInPinnedFootprint` is true only when pinned and the tile is inside the footprint `TileArea`.
   - `Clear()` unpins. `SetPinned` marks every footprint tile dirty.
2. **`DrawJunctionGhost(ti)`:**
   - **Remove the palette override entirely.** Ghost sprites are always drawn with normal palettes; delete the `SetViewportGhostPalette(...)` calls in this function.
   - **The order becomes:**
     1. Look up `ghost = Get(ti->tile)`.
     2. If there's a ghost entry and `ti->tileh != SLOPE_FLAT`: draw the red outline only (as now, but **without** the palette override: `DrawTileSelectionRect(ti, PALETTE_SEL_TILE_RED)`), then return.
     3. If there's a ghost entry: draw its track, bridge and signals opaque. If `ghost->bad`, also draw `DrawTileSelectionRect(ti, PALETTE_SEL_TILE_RED)`.
     4. Otherwise, or in addition when there's no `bad` outline: if `IsInPinnedFootprint(ti->tile)` and no red outline was drawn for this tile, draw `DrawTileSelectionRect(ti, PAL_NONE)` (white).
   - Keep the early return for tiles that are neither ghost tiles nor inside a pinned footprint.
3. **`BuildJunctionWindow`:**
   - **When a build starts** (`StartBuild` calls `Start`): call `JunctionGhost::SetPinned(true)`, and hide the cursor rectangle using the mechanism you found in section 2.
   - **When the state returns to `Idle`** (build done, or Cancel), and in `Close`: call `JunctionGhost::SetPinned(false)` and restore the cursor rectangle (`SetTileSelectSize(size.x, size.y)` of the placement layout, as the hover code already does).
4. **`SetViewportGhostPalette`** stays in `viewport.cpp`; it's simply no longer called by `junction_gui.cpp`. Don't remove the upstream seam.

Commit message: `Fix: Draw invalid junction ghosts opaque with a red outline, and pin the footprint while building`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 5. Do not

- Edit upstream files (`viewport.cpp` and so on). If hiding the cursor rectangle can't be done without one, stop.
- Change the sequencer, the estimate, the build steps, or the hover pricing.

## 6. Stop conditions

- There's no way to hide the cursor's `HT_RECT` rectangle while staying in placement mode, without an upstream edit. Report the options you found and stop before implementing item 3's hiding. Items 1 and 2 may still be committed.
- Anything not covered here.

## 7. Gates

1. Build `openttd` and `openttd_test` with no errors and no warnings.
2. The red run is recorded. The full suite is `{BASELINE}` plus 2, all passing.
3. One commit, touching only `junction_gui.h`, `junction_gui.cpp` and `tests/junction_gui.cpp`.

Logs go to `.../s9g_*.log`.

## 8. Report format

- **References read,** plus the cursor-rectangle mechanism you used and why it works
- **Commits**
- **Red evidence**
- **Types and members added or removed**
- **Suite results**
- **Manual checks for the engineer:**
  - over water or a building, the ghost is fully visible, with red outlines on the bad tiles
  - on slopes, red outlines only
  - after a paused (failed) build, the outline stays at the build position, red on the remaining tiles, and the cursor rectangle is gone
  - Retry completes the build, and placement then resumes following the cursor
  - Cancel does the same
- **Stops, and anything left undone**
- **State:** `git status --porcelain` and `git log -1 --format='%h %s'`

When you have reported, stop.
