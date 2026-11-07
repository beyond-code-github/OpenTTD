# Recon: junction placement ghost

Facts gathered for the placement milestone (read-only recon, 2026-11-07, HEAD 938fd8fbff). Paths under `src/`.

## Building

- **Signals:** `CmdBuildSingleSignal(flags, tile, track, sigtype, sigvar, convert_signal, skip_existing_signals, ctrl_pressed, cycle_start, cycle_stop, num_dir_cycle, signals_copy)` (`rail_cmd.cpp:1070`).
  - Path signals are `SignalType::Path` and `SignalType::PathOneWay`, with the variant `SignalVariant::Electric` or `Semaphore`.
  - Direction can be set in two ways:
    - `num_dir_cycle` cycles the sides after a fresh build.
    - `signals_copy` sets the side bits exactly. It must overlap `SignalOnTrack(track)`, and it's the code path `CmdBuildSignalTrack` uses.
  - The track must already exist, on plain rail with no overlapping tracks.
  - Cost is `_price[Price::BuildSignals]` per signal, regardless of rail type or signal type.
- **Bridges:** `CmdBuildBridge(flags, tile_end, tile_start, TransportType::Rail, BridgeType, RailType, INVALID_ROADTYPE)` (`tunnelbridge_cmd.cpp:311`).
  - The heads must be at the same height, and flat heads are fine for rail.
  - Valid types are those `CheckBridgeAvailability(type, len)` accepts, where `len` is the number of middle tiles.
  - Plain track is allowed under a bridge, subject to the height rule `GetTileMaxZ(middle) <= z_head`.
  - Bridges can't cross other bridges.
- **Bridge price:** a `QueryCost` run returns only the clearing and terraforming cost. The GUI adds the rest (`bridge_gui.cpp:422-440`):
  `cost + ((CalcBridgeLenCostFactor(len+2) * _price[Price::BuildBridge] * spec->price) >> 8) + (len+2) * RailBuildCost(rt)`.

## Up-front cost consequence

- Signals can't be test-run before their track exists, so they're priced as count × `Price::BuildSignals`.
- Track is priced by test-running `Commands::BuildRail` on each tile.
- Bridges are priced by the GUI formula above.
- For a flat, clear footprint these parts don't interact, so the sum is exact.

## Drawing

- **There is no existing multi-tile ghost.** Pickers draw previews only inside windows. In the viewport, placement shows only the tile-selection rectangle.
- **Track:** `static void DrawTrackBits(TileInfo *ti, TrackBits track)` (`rail_cmd.cpp:2289`), plus `DrawTrackBitsOverlay` (`:2101`) for NewGRF overlay rail types.
  - It reads the map for the rail type, the ground type, and the reservation.
  - Everything else comes from `ti` (tileh, z, x, y).
  - A ghost needs a refactor that takes the rail type and ground type as parameters. Sprites come from `rti->base_sprites`, or from `GetCustomRailSprite(rti, tile, ...)` for NewGRF types (the tile is only used for resolving).
- **Signals:** `static void DrawSingleSignal(tile, rti, track, condition, image, pos)` (`rail_cmd.cpp:1901`) reads the signal type and variant from the map, and the position mapping lives in `DrawSignals` (`:2436`). A ghost needs a copy that takes the type and variant as parameters. The sprite comes from `GetCustomSignalSprite(rti, tile, type, variant, state)`, with a fallback to the base sprites.
- **Bridges:** drawing was not investigated yet (`tunnelbridge_cmd.cpp`).
- **Hook point:** `DrawTileSelection(const TileInfo *ti)` (`viewport.cpp:1120`) is called during the landscape pass for each tile.
  - `DrawGroundSprite` and `AddSortableSpriteToDraw` can be called from there. Ground sprites attach to the current tile's foundation.
- **Transparency and red:**
  - `AddSortableSpriteToDraw(..., transparent=true)` sets `PALETTE_MODIFIER_TRANSPARENT` and `PALETTE_TO_TRANSPARENT`.
  - For ground sprites, set the bit and palette by hand, as `sprite.cpp:67` does.
  - For red, use a remap palette such as `PALETTE_TO_STRUCT_RED` with the transparent bit set. That gives a transparent remap.

## Input and display

- **Footprint outline:** `HT_RECT` plus `SetTileSelectSize(w, h)` follows the cursor, with the cursor at the top-left corner. Set `_thd.make_square_red` to make the outline red. Rotating swaps w and h, which is the station precedent.
- **Hovered-tile changes:** there's no callback. Poll in `OnRealtimeTick` or `OnMouseLoop` and compare against the last tile. `BuildRailToolbarWindow::OnRealtimeTick` (`rail_gui.cpp:849`) is the precedent.
- **Price by the cursor:** `GuiShowTooltips(parent, GetEncodedString(STR_..., money), TooltipCloseCondition::ExitViewport)` (`misc_gui.cpp:690`). It's recreated at the cursor on every call. `ShowMeasurementTooltips` (`viewport.cpp:2796`) is the precedent, but it's gated on the `measure_tooltip` setting, so call `GuiShowTooltips` directly. Money is formatted with `{CURRENCY_LONG}`.
- **Rotate and mirror keys:** there's no precedent. Add a `HotkeyList` and override `OnHotkey` in the placing window, following `BuildRailToolbarWindow::hotkeys` (`rail_gui.cpp:894`).

## Bridges (second recon)

- **Bridge head:** drawn by `DrawTile_TunnelBridge` (`tunnelbridge_cmd.cpp:1282`, bridge branch ~1442–1540).
  - It reads the bridge type, rail type, direction, snow/desert, reservation, `ti->tileh` and `ti->z` from the map.
  - Ramp sprite: `GetBridgeSpriteTable(type, BRIDGE_PIECE_HEAD)` (`:156`, static), offset by `GetBridgeRampDirectionBaseOffset(dir)`, plus 4 on flat ground, plus `rti->bridge_offset`. It's drawn with `AddSortableSpriteToDraw(..., *ti, {{}, {TILE_SIZE, TILE_SIZE, flat ? 0 : TILE_HEIGHT}, {}})`.
  - The rail surface overlay is drawn only for custom layouts and overlay rail types.
  - The head ends with `DrawBridgeMiddle(ti, blocked_pillars)`.
- **Bridge middle:** `void DrawBridgeMiddle(const TileInfo *ti, BridgePillarFlags blocked_pillars)` (`:1604`, `bridge.h:69`).
  - It reads the bridge ends, axis, type, lengths, deck height and rail type from the map.
  - Sprites come from `GetBridgeSpriteTable(type, CalcBridgePiece(north_len+1, south_len+1))` with 3 entries: floor, roof and pillar.
  - The deck height is `GetBridgePixelHeight(rampsouth)`, and `BRIDGE_Z_START` = 3.
  - Pillars are drawn by `DrawBridgePillars(psid, ti, axis, drawfarpillar, x, y, z)` (`:1086`), which reads only `ti`.
  - Every tile type's draw proc calls `DrawBridgeMiddle` for a tile with a bridge above. There's no viewport-side call.
- **There's no drawer that works without the map.** A ghost needs parameterised copies of the head and middle drawing, taking the bridge type, rail type, direction or axis, deck z, and lengths north and south. It can skip reservation, snow and catenary.
- **Calling those copies from `DrawTileSelection`** works for each head tile and each span tile, because it runs after each tile's own draw proc.
- **Risk:** the viewport visibility check (`viewport.cpp:1294`) uses the real `IsBridgeAbove`, so a ghost deck may be culled near the edges of the view.
- **Height:** on flat ground the deck is 1 level above the heads (`GetBridgeHeight`, `bridge_map.cpp:76`). Tiles under the span must satisfy `GetTileMaxZ(tile) <= z_head` (`CmdBuildBridge` `:498`).

## Picker windows and settings (second recon)

- **Window base class:** `PickerWindow` (`picker_gui.h:322`) always shows class and type lists for NewGRF items. A junction window should derive from `PickerWindowBase` (`window_gui.h:1018`), as `BuildSignalWindow` (`rail_gui.cpp:1522`) and `BuildRailDepotWindow` (`:1776`) do.
- **+/- number control:** `airport_gui.cpp:607-611`. It's `WWT_PUSHARROWBTN` with `ArrowWidgetType::Decrease`/`Increase`, around a `WWT_LABEL`. The handler is at `:525-535`, and the disabled state is set with `SetWidgetDisabledState`.
- **Toggle buttons:** `WWT_PANEL` with `LowerWidget`/`RaiseWidget`, following the station orientation buttons (`rail_gui.cpp:1296-1310`).
- **Opening the window:**
  - In the toolbar's `OnClick`: `bool started = HandlePlacePushButton(...)`, then `if (started) Show...(this)` (`rail_gui.cpp:646-666`).
  - The `WindowDesc` uses a new `WindowClass` with parent class `WindowClass::BuildToolbar` and `WindowDefaultFlag::Construction` (`:1503`).
  - The toolbar's `OnPlaceObjectAbort` (`:821`) must `CloseWindowById` the new class.
- **Persistent client settings:**
  - Add a field to `GUISettings` (`settings_type.h:217`).
  - Add an `[SDTC_VAR]`/`[SDTC_BOOL]` block to `table/settings/gui_settings.ini` with `flags = SettingFlag::NotInSave, SettingFlag::NoNetworkSync`, following `gui.station_numtracks` (`:683`).
  - Strings use `[SDT_SSTR]` (`locale_settings.ini:174`), so the client-setting flags for a string need checking.
- **Signal direction for the build command:** `SignalAlongTrackdir(td)` (`signal_func.h:24`) is the bit for the signal a train travelling along `td` faces. Passing it as `signals_copy` builds a one-way signal facing that way.
