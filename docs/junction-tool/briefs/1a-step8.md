# Brief: 1a step 8, junction tool window, hover ghost and price tooltip

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- Repo `/home/pete/OpenTTD` (the main checkout), branch `feature/junction-tool`, head `{HEAD}`. You are the only agent in this checkout.
- Plan: `docs/junction-tool/PLAN-1a.md`, "Window, tool and hover" and "Steps" item 8. This brief is authoritative where they differ.
- **Already on the branch:**
  - `src/junction_layout.h`/`.cpp`: the layout, transforms, build steps and estimate.
  - `src/junction_game.h`/`.cpp`: `GameJunctionPricing(RailType)` and `ChooseJunctionBridgeType(RailType, uint length)`.
  - `src/junction_ghost_draw.*`: the drawing seams.
  - `src/junction_gui.h`/`.cpp`: the `JunctionGhost` store (`Set`, `Clear`, `Get`, `GetRailType`) and `DrawJunctionGhost`, already hooked into the viewport.
- **Baseline:** `{BASELINE}`, all passing.
- **Rebase-friendliness (D42):** upstream edits are small and listed. The new window lives in `junction_gui.cpp`.
- **Step 9** adds building (click to build, the sequencer, Retry and Cancel). In this step a click does nothing.

## 2. Read first, once each

- `CODINGSTYLE.md`, **in full.** Your report must quote one specific rule from it, verbatim, as evidence you read it.
- `src/junction_layout.h`, `src/junction_gui.h`, `src/junction_game.h`.
- `src/rail_gui.cpp`:
  - 440–700: the toolbar class, its widget list near 469, `GetCursorForWidget`, `GetHighLightStyleForWidget`, `OnClick`, `OnPlaceObject`
  - 821–860: `OnPlaceObjectAbort`, `OnRealtimeTick`
  - 890–960: the hotkeys and the nested widgets
  - 1776–1855: `BuildRailDepotWindow`, `_nested_build_depot_widgets`, `_build_depot_desc` and `ShowBuildTrainDepotPicker`. **These are the pattern for the new window.**
- `src/widgets/rail_widget.h` and `src/widgets/depot_widget.h`. Also how `BuildRailDepotWidgets` are declared (search `WID_BRAD_`).
- `src/window_type.h`: the `WindowClass` enum around `BuildDepot`.
- `src/window_gui.h` around `PickerWindowBase` (≈1018), `OnHotkey` and `OnRealtimeTick`. Also `src/hotkeys.h` (`Hotkey`, `HotkeyList`).
- How a window's own `HotkeyList` is attached and dispatched. See `BuildRailStationWindow::hotkeys` and its `WindowDesc` in `rail_gui.cpp` (~1440–1510), and `HandleKeypress` in `window.cpp` (~2715). Confirm that a non-focused construction window's hotkeys receive key presses. If they don't, stop.
- `src/misc_gui.cpp` around `GuiShowTooltips` (≈690), and `src/viewport.cpp` `SetTileSelectSize` (≈2622) and `GetTileBelowCursor` (≈458).
- `src/lang/english.txt`: the `STR_RAIL_TOOLBAR_TOOLTIP_*` block (≈2841) and the `STR_BUILD_DEPOT_TRAIN_*` strings, for the caption and button text style.

## 3. Tests (write first, see them fail)

Add to `src/tests/junction_layout.cpp`:

1. **`"JunctionLayout: orientation applies mirror then rotations"`** tests `ApplyJunctionOrientation(layout, rotation, mirrored)` on the test junction:
   - `(0, false)` gives the same layout (use `SameLayout`)
   - `(1, false)` gives one `Rotate`
   - `(3, true)` gives `Rotate(Rotate(Rotate(Mirror(layout))))`
   - `(4, false)` gives the same layout as `(0, false)`
2. **`"JunctionLayout outer: estimate reports the ground height"`:**
   - default stub: `estimate.height == 1`
   - with `pricing.heights` set to 3 for **every** occupied tile of the test junction at origin (10,10): `estimate.height == 3`, and the estimate is affordable

**Red:** add the declaration and a stub returning `layout`, and add the field with its default of 0. Build and run, and record the failures. Test 1's `(1, false)` and `(3, true)` cases fail; `(0, false)` passes. Test 2 fails on `height == 1`.

## 4. Changes

### Commit 1: `Add: Junction orientation helper and estimate ground height`

- In `src/junction_layout.h`:
  - Add `int height = 0; ///< Ground height of the junction in height levels (the reference height).` to `JunctionEstimate`.
  - Declare `JunctionLayout ApplyJunctionOrientation(const JunctionLayout &layout, uint8_t rotation, bool mirrored);`, documented as "Mirror (if set), then rotate 'rotation % 4' quarter turns."
- In `src/junction_layout.cpp`:
  - Implement it with `TransformJunctionLayout`.
  - In `EstimateJunction`, set `estimate.height` to the reference height found by rule 3, or leave it 0 if there's none.
- Commit `src/junction_layout.h`, `src/junction_layout.cpp` and `src/tests/junction_layout.cpp`.

### Commit 2: `Add: Junction tool on the rail toolbar with placement ghost and price tooltip`

**`src/widgets/junction_widget.h`** (new, guard `WIDGETS_JUNCTION_WIDGET_H`, styled like the other widget headers):
```cpp
/** Widgets of the #BuildJunctionWindow class. */
enum BuildJunctionWidgets : WidgetID {
	WID_BJ_NAME,   ///< Name of the selected junction.
	WID_BJ_ROTATE, ///< Rotate the junction.
	WID_BJ_MIRROR, ///< Mirror the junction.
};
```

**`src/window_type.h`:** add `BuildJunction` to `WindowClass`, right after `BuildDepot`, with the comment `///< Build junction; %Window numbers: TransportType::Rail`, following its neighbours' comment style. If `WindowClass` values are persisted anywhere by number, rather than by the name in `WindowDesc`, stop.

**`src/widgets/rail_widget.h`:** append `WID_RAT_BUILD_JUNCTION, ///< Build a pre-built junction.` as the **last** entry of `RailToolbarWidgets`, after `WID_RAT_CONVERT_RAIL`.

**`src/rail_gui.cpp`.** Grep for `WID_RAT_CONVERT_RAIL` and `WID_RAT_BUILD_TUNNEL`, and list every site in your report. Then:
- **Widget list near line 469** (if it's the list of tool buttons): add `WID_RAT_BUILD_JUNCTION`.
- **`GetCursorForWidget`:** `case WID_RAT_BUILD_JUNCTION: return GetRailTypeInfo(_cur_railtype)->cursor.autorail;`
- **`GetHighLightStyleForWidget`:** `case WID_RAT_BUILD_JUNCTION: return HT_RECT;`
- **`OnClick`:** `case WID_RAT_BUILD_JUNCTION: if (started) ShowBuildJunctionWindow(this); break;`
- **`OnPlaceObject`:** `case WID_RAT_BUILD_JUNCTION: JunctionToolClick(tile); break;`
- **`OnPlaceObjectAbort`:** add `CloseWindowById(WindowClass::BuildJunction, TransportType::Rail);` next to the others.
- **Nested toolbar widgets:** a new `NWidget(WWT_IMGBTN, Colours::DarkGreen, WID_RAT_BUILD_JUNCTION), SetFill(0, 1), SetToolbarMinimalSize(1), SetSpriteTip(SPR_IMG_AUTORAIL, STR_RAIL_TOOLBAR_TOOLTIP_BUILD_JUNCTION),`, placed **after** the convert-rail button.
- **Hotkeys:** `Hotkey(WKC_NONE, "junction", WID_RAT_BUILD_JUNCTION),` at the end of the rail toolbar list. Check that `WKC_NONE`, or whatever this tree uses for "no default key", exists. If not, stop.
- `#include "junction_gui.h"`.
- If any other `switch` or list over toolbar widgets would need the new widget (for example ctrl-removal or `UpdateRemoveWidgetStatus`), **don't add it there**. List it in the report.

**`src/lang/english.txt`.** Add these strings, keeping the column alignment of their neighbours:
- With the rail toolbar tooltips: `STR_RAIL_TOOLBAR_TOOLTIP_BUILD_JUNCTION :{BLACK}Build a pre-built junction. Press . to rotate and , to mirror`
- A new block near the depot picker strings:
  - `STR_JUNCTION_CAPTION :{WHITE}Junction`
  - `STR_JUNCTION_NAME_TEST :{BLACK}Test junction`
  - `STR_JUNCTION_ROTATE :{BLACK}Rotate`
  - `STR_JUNCTION_ROTATE_TOOLTIP :{BLACK}Rotate the junction a quarter turn (hotkey .)`
  - `STR_JUNCTION_MIRROR :{BLACK}Mirror`
  - `STR_JUNCTION_MIRROR_TOOLTIP :{BLACK}Mirror the junction (hotkey ,)`
  - `STR_JUNCTION_TOOLTIP_COST :{BLACK}Cost: {CURRENCY_LONG}`
  - `STR_JUNCTION_TOOLTIP_UNAFFORDABLE :{RED}Cost: {CURRENCY_LONG} (you have {CURRENCY_LONG})`
  - `STR_JUNCTION_TOOLTIP_INVALID :{RED}Can't build here: {STRING}`

**`src/junction_gui.h`:** add these declarations, documented in the .cpp:
- `void ShowBuildJunctionWindow(Window *parent);`
- `void JunctionToolClick(TileIndex tile);`

Forward-declare `struct Window;`.

**`src/junction_gui.cpp`:**
- **File-static placement state:** `static uint8_t _junction_rotation = 0;` and `static bool _junction_mirrored = false;`
- **`static JunctionLayout GetPlacementLayout()`:** `return ApplyJunctionOrientation(MakeTestJunctionLayout(), _junction_rotation, _junction_mirrored);`
- **`JunctionToolClick(TileIndex)`:** an empty body, with the comment "Building is added in step 9 of PLAN-1a."
- **`struct BuildJunctionWindow : public PickerWindowBase`,** modelled on `BuildRailDepotWindow`:
  - **Constructor:** `InitNested(TransportType::Rail)`, then set `this->last_tile = INVALID_TILE`.
  - **Members:** `TileIndex last_tile; ///< Tile the ghost was last placed at.`
  - **`OnClick`:**
    - `WID_BJ_ROTATE`: `_junction_rotation = (_junction_rotation + 1) % 4;`, then `this->last_tile = INVALID_TILE; SndClickBeep();`.
    - `WID_BJ_MIRROR`: toggle `_junction_mirrored`, then the same.
  - **`OnHotkey(int hotkey)`:** `JHK_ROTATE` → `OnClick({}, WID_BJ_ROTATE, 1)`, and `JHK_MIRROR` → the same with `WID_BJ_MIRROR`. Return `EventState::Handled` for those, and otherwise defer to the base.
  - **`OnRealtimeTick(uint)`:**
    1. `Point pt = GetTileBelowCursor();`. If `pt.x == -1`, call `JunctionGhost::Clear()`, set `this->last_tile = INVALID_TILE`, and return.
    2. `TileIndex tile = TileVirtXY(pt.x, pt.y);`. If `tile == this->last_tile`, return.
    3. Set `this->last_tile = tile`.
    4. Get `layout = GetPlacementLayout()`, then call `SetTileSelectSize(layout.size.x, layout.size.y)`.
    5. Run the estimate: `GameJunctionPricing pricing(_cur_railtype); JunctionEstimate estimate = EstimateJunction(layout, tile, pricing);`
    6. Build the bridge types as `std::vector<std::optional<BridgeType>>`, one per layout bridge, with `ChooseJunctionBridgeType(_cur_railtype, length)`, where `length = abs(end.x - start.x) + abs(end.y - start.y) - 1`.
    7. Call `JunctionGhost::Set(layout, tile, estimate.height, _cur_railtype, bridge_types, estimate);`
    8. Set `_thd.make_square_red = !estimate.affordable;`
    9. Show the tooltip: `GuiShowTooltips(this, text, TooltipCloseCondition::ExitViewport)`, where `text` is:
       - `GetEncodedString(STR_JUNCTION_TOOLTIP_INVALID, estimate.first_error)` if `estimate.bad_tiles` isn't empty
       - otherwise `GetEncodedString(STR_JUNCTION_TOOLTIP_UNAFFORDABLE, estimate.total, pricing.AvailableMoney())` if it isn't affordable
       - otherwise `GetEncodedString(STR_JUNCTION_TOOLTIP_COST, estimate.total)`
  - **`Close(int data)` override:** call `JunctionGhost::Clear()`, set `_thd.make_square_red = false`, then call `PickerWindowBase::Close(data)`. Check `PickerWindowBase::Close`'s exact signature and match it.
  - **Hotkeys:** `static inline HotkeyList hotkeys{"buildjunction", { Hotkey(WKC_PERIOD, "rotate", JHK_ROTATE), Hotkey(WKC_COMMA, "mirror", JHK_MIRROR) }};`, with `enum JunctionHotkeys : int { JHK_ROTATE, JHK_MIRROR };` at file scope.
- **Nested widgets** (`_nested_build_junction_widgets`), like the depot picker's:
  - a caption row with a close box and `STR_JUNCTION_CAPTION`
  - a panel holding a vertical container with `WWT_LABEL WID_BJ_NAME` (`STR_JUNCTION_NAME_TEST`)
  - then a horizontal row with two `WWT_PUSHTXTBTN` buttons, `WID_BJ_ROTATE` and `WID_BJ_MIRROR`, using their strings and tooltips
  - padding and spacing as in the depot picker
- **`WindowDesc _build_junction_desc(WindowPosition::Automatic, "build_junction", 0, 0, WindowClass::BuildJunction, WindowClass::BuildToolbar, WindowDefaultFlag::Construction, _nested_build_junction_widgets, &BuildJunctionWindow::hotkeys);`**. Match the argument order to the `WindowDesc` constructor in this tree.
- **`ShowBuildJunctionWindow(Window *parent)`:** `new BuildJunctionWindow(_build_junction_desc, parent);`

Add `src/widgets/junction_widget.h` to the widgets file list in CMake, if one exists (check `src/widgets/CMakeLists.txt`).

## 5. Do not

- Implement building, Retry or Cancel. That's step 9.
- Unit-test the window or the tooltip.
- Reorder existing enum entries, widgets or strings.
- Change `PLAN-1a.md` or other docs.

## 6. Stop conditions

- Window hotkeys don't reach a non-focused construction window.
- `WindowClass` is persisted by number.
- A named API (`GetTileBelowCursor`, `TileVirtXY`, `GuiShowTooltips`, `GetEncodedString`, `SetTileSelectSize`, `WKC_PERIOD`, `WKC_COMMA`, `WKC_NONE`) differs in this tree.
- The toolbar needs the new widget in places beyond those listed, in a way that would break if left out. Report it rather than guess.
- Anything not covered here.

## 7. Gates

Logs go to `.../s8_*.log`.

1. Build `openttd_test` and `openttd` with no errors and no warnings. The `strgen` step must also report no string warnings.
2. The red run is recorded. After commit 1, the full suite is `{BASELINE}` plus 2, all passing.
3. After commit 2, the full suite passes again.
4. Report `git diff --stat {HEAD}..HEAD -- src/rail_gui.cpp src/window_type.h src/widgets/rail_widget.h src/lang/english.txt`.
5. **A launch smoke test:** run `./build/openttd -h > .../s8_help.log 2>&1` to make sure the binary starts. Don't launch the GUI.

Each commit ends with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 8. Report format

- **References read,** including one CODINGSTYLE.md rule quoted verbatim
- **Commits**
- **Red evidence**
- **Types and members added or removed**
- **Toolbar sites found and changed,** plus any not changed
- **Upstream diff size**
- **Suite results**
- **Manual checks for the engineer:**
  - the toolbar button appears
  - the window opens
  - the ghost follows the mouse in the current rail type
  - red over hills, water and buildings, with the reason in the tooltip
  - the price tooltip follows the cursor
  - `.` and `,` rotate and mirror, as do the buttons
  - closing the window or pressing Esc clears the ghost
  - the ghost bridge deck isn't clipped near the view edges
- **Stops, and anything left undone**
- **State:** `git status --porcelain` and `git log -2 --format='%h %s'`

When you have reported, stop.
