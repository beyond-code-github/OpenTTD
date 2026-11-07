# Milestone 1a: junction placement

The player opens the new **Junction** tool on the rail toolbar. A small window opens. A ghost of the selected junction follows the mouse, drawn semi-transparent in the toolbar's rail type, using real track, signal and bridge sprites. The price floats next to the cursor. `.` rotates the junction and `,` mirrors it. Clicking builds it with ordinary commands. If a build fails, the window says how far it got.

1a is driven by one fixed **test junction**. The generator that produces real junctions arrives in 1b (D16, D26), and it plugs into the same junction description.

Decisions: D1–D16, D35 and D37 in `DECISIONS.md`. Research: `RECON-placement.md`.

## Scope

**In:**
- the junction description, with rotate and mirror
- the build-step list and the up-front price (D9)
- flat-ground validity (D11)
- the ghost: track, one-way signals and rail bridges, in rail-type sprites (D13)
- the price tooltip by the cursor (D14)
- rotate and mirror hotkeys (D15)
- the window, the toolbar button and the build sequencer with failure handling (D7)

**Out:**
- generators and generator parameters (CL, TL, the line notation, handedness): 1b
- prios and train physics: 1b and 1c
- terrain fitting
- catenary in the ghost
- path reservations in the ghost
- connecting to existing track (endpoint mode)

## Design

### Files

| File | Contents |
|---|---|
| `src/junction_layout.h`, `src/junction_layout.cpp` | Pure: the description types, transforms, build steps, the estimate, the `JunctionPricing` interface, and `MakeTestJunctionLayout()` |
| `src/junction_game.cpp` | `GameJunctionPricing` (real map, commands and prices) and `JunctionBuilder` (the sequencer) |
| `src/junction_gui.h`, `src/junction_gui.cpp` | Placement state, the ghost store and its drawing hook, the window, the tooltip, and the toolbar glue |
| `src/widgets/junction_widget.h` | `BuildJunctionWidgets` enum |
| `src/tests/junction_layout.cpp` | Catch2 tests with `StubJunctionPricing` |
| Edits | `rail_cmd.cpp`, `rail.h`, `tunnelbridge_cmd.cpp`, `bridge.h`, `viewport.cpp`, `rail_gui.cpp`, `widgets/rail_widget.h`, `window_type.h`, `lang/english.txt`, `src/CMakeLists.txt`, `src/tests/CMakeLists.txt` |

### Junction description (`junction_layout.h`)

Offsets are relative to the layout's north corner. The north corner is the tile with the smallest x and y, at (0,0), and every offset has x and y ≥ 0.

```cpp
struct JunctionTrack {
	TileIndexDiffC offset;
	TrackBits tracks;
};

/** A one-way signal; trains travelling along 'facing' see it (SignalAlongTrackdir). */
struct JunctionSignal {
	TileIndexDiffC offset;
	Trackdir facing;
	SignalType type;
	SignalVariant variant;
};

/** A rail bridge between two heads on the same row or column; the bridge type is chosen at placement (D35). */
struct JunctionBridge {
	TileIndexDiffC start;
	TileIndexDiffC end;
};

struct JunctionLayout {
	TileIndexDiffC size; ///< Bounding box in tiles.
	std::vector<JunctionTrack> tracks;
	std::vector<JunctionSignal> signals;
	std::vector<JunctionBridge> bridges;
};

enum class JunctionTransform : uint8_t { Rotate, Mirror };

JunctionLayout TransformJunctionLayout(const JunctionLayout &layout, JunctionTransform transform);
JunctionLayout MakeTestJunctionLayout();
```

**Transforms** are pure, and every transform keeps all offsets ≥ 0.

- **Rotate** turns the layout a quarter turn so that DiagDirection NE→SE→SW→NW→NE.
  - Offsets: (x, y) → (y, size.x − 1 − x), and the size becomes (size.y, size.x).
  - Directions: a DiagDirection d maps to `ChangeDiagDir(d, DiagDirDiff::Right90)` (`direction_func.h:159`).
- **Mirror** flips across the y axis, swapping NE and SW.
  - Offsets: (x, y) → (size.x − 1 − x, y).
  - Directions: NE↔SW, while SE and NW stay put.
- **Tracks and trackdirs are transformed through their edges.**
  - A track joins two tile edges: X = NE+SW, Y = NW+SE, Upper = NE+NW, Lower = SE+SW, Left = SW+NW, Right = NE+SE.
  - To transform a track, transform both edges, then find the track joining the new pair. That lookup is a static table in `junction_layout.cpp`.
  - To transform a trackdir, transform its exit edge (`TrackdirToExitdir`) and its track, then rebuild it with `TrackExitdirToTrackdir(new_track, new_exitdir)`.
- **Bridges** transform their start and end offsets. Afterwards, swap them if needed so that `start` is the more northern head (smaller x+y).

**`MakeTestJunctionLayout()`** returns a 8×6 fixture that exercises every kind of piece:
- `Track::X` on (0..7, 2)
- a bridge from (4, 0) to (4, 4), whose span (4, 1..3) passes over the track at (4, 2)
- `Track::Y` on (4, 5), leading off the southern head
- a one-way block signal facing `Trackdir::X_SW` at (1, 2)
- a one-way block signal facing `Trackdir::X_NE` at (6, 2)

### Build steps and estimate

```cpp
struct JunctionRailStep { TileIndex tile; Track track; };
struct JunctionBridgeStep { TileIndex start; TileIndex end; };
struct JunctionSignalStep { TileIndex tile; Trackdir facing; SignalType type; SignalVariant variant; };
using JunctionBuildStep = std::variant<JunctionRailStep, JunctionBridgeStep, JunctionSignalStep>;

/** Steps in build order: every rail piece, then every bridge, then every signal (signals need their track). */
std::vector<JunctionBuildStep> MakeJunctionBuildSteps(const JunctionLayout &layout, TileIndex origin);

/** Everything the estimate needs from the game. House pattern: abstract base, test subclass (D6). */
class JunctionPricing {
public:
	virtual ~JunctionPricing() = default;
	/** Tile is on the map, flat, and its height. std::nullopt if off-map or not flat. */
	virtual std::optional<int> GetFlatHeight(TileIndex tile) const = 0;
	/** Test-only run of building one rail piece (cost includes clearing). */
	virtual CommandCost TestRail(TileIndex tile, Track track) const = 0;
	/** Cheapest available bridge type at least as fast as the rail type (D35) and its full price, or an error. */
	virtual CommandCost PriceBridge(TileIndex start, TileIndex end) const = 0;
	virtual Money SignalCost() const = 0;
	virtual Money AvailableMoney() const = 0;
};

struct JunctionEstimate {
	Money total = 0;
	std::vector<TileIndex> bad_tiles; ///< Tiles that are off-map, not flat, at the wrong height, or whose step failed.
	StringID first_error = INVALID_STRING_ID;
	bool affordable = false; ///< bad_tiles empty and total <= AvailableMoney().
};

JunctionEstimate EstimateJunction(const JunctionLayout &layout, TileIndex origin, const JunctionPricing &pricing);
```

**How `EstimateJunction` works:**
1. **Flatness.** The occupied tiles are every track tile, both bridge heads and every span tile. Each must have `GetFlatHeight` equal to the origin tile's height. If the origin tile isn't flat, it uses the first occupied tile that is.
2. **Rail.** It sums `TestRail` for every rail step.
3. **Bridges.** It sums `PriceBridge` for every bridge.
4. **Signals.** It adds the number of signals × `SignalCost()`.
5. A failed step adds its tile(s) to `bad_tiles` and records the first error. Failures don't stop the estimate, so every bad tile gets shown.

**`origin` is the tile under the cursor.** It becomes the layout's (0,0), just as the station tool puts the cursor at the selection's top-left. An origin whose layout runs off the map counts as bad tiles, not as a crash. Use `AddTileIndexDiffCWrap`, `map_func.h:519`, which returns `INVALID_TILE` off-map.

### Game side (`junction_game.cpp`)

**`GameJunctionPricing(RailType rt)`:**
- `GetFlatHeight`: `GetTileSlopeZ`. It returns the height only if the slope is `SLOPE_FLAT`.
- `TestRail`: `Command<Commands::BuildRail>::Do(CommandFlagsToDCFlags(GetCommandFlags<Commands::BuildRail>()) | DoCommandFlag::QueryCost, tile, rt, track, false)`. That's the pattern at `bridge_gui.cpp:396`.
- `PriceBridge`:
  1. Set `len` = the number of middle tiles.
  2. Pick the cheapest bridge type with `CheckBridgeAvailability(type, len).Succeeded()` and `GetBridgeSpec(type)->speed >= GetRailTypeInfo(rt)->max_speed`. If `max_speed` is 0, meaning unlimited, take the fastest available type. If none qualifies, return `CommandCost(STR_ERROR_CAN_T_BUILD_BRIDGE_HERE)`.
  3. Run a `QueryCost` test of `Commands::BuildBridge` with that type, for validity and the clearing cost.
  4. Add the bridge window's formula: `((int64_t)CalcBridgeLenCostFactor(len + 2) * _price[Price::BuildBridge] * spec->price) >> 8` plus `(len + 2) * RailBuildCost(rt)` (`bridge_gui.cpp:422-440`).
  5. Remember the chosen type for the build.
- `SignalCost`: `_price[Price::BuildSignals]`.
- `AvailableMoney`: `GetAvailableMoney(_local_company)`.

**`JunctionBuilder`:**
- It's a static holding the steps, the rail type, the chosen bridge types, the next index and a state of Idle, Building or Failed.
- **`Start`** re-runs the estimate. If the result isn't affordable it refuses, and the window refreshes. Otherwise it posts step 0.
- **Posting by step type:**
  - Rail: `Command<Commands::BuildRail>::Post(STR_ERROR_CAN_T_BUILD_RAILROAD_TRACK, CcJunctionStep, tile, rt, track, false)`
  - Bridge: `Command<Commands::BuildBridge>::Post(STR_ERROR_CAN_T_BUILD_BRIDGE_HERE, CcJunctionStep, end, start, TransportType::Rail, type, rt, INVALID_ROADTYPE)`
  - Signal: `Command<Commands::BuildSignal>::Post(STR_ERROR_CAN_T_BUILD_SIGNALS_HERE, CcJunctionStep, tile, TrackdirToTrack(facing), type, variant, false, false, false, SignalType::Block, SignalType::Block, 0, SignalAlongTrackdir(facing))`
- **`CcJunctionStep(Commands, const CommandCost &result, TileIndex)`** posts the next step on success.
  - On failure, it sets the state to Failed and keeps the index. The ghost shows the remaining steps in red, and the window shows "Built N of M steps" with **Retry**, which re-estimates and resumes from the failed step, and **Cancel**.
  - `Post` shows the game's own error message.
- **Error string names** must be checked against `lang/english.txt`. If one doesn't exist, use the string the existing GUI uses for that command.

### Drawing refactors (Codechange, no visible change to real tiles)

- **Track** (`rail_cmd.cpp`). Split `DrawTrackBits` (`:2289`) and `DrawTrackBitsOverlay` (`:2101`) so the sprite work is a new function:
  `void DrawRailTrackSprites(TileInfo *ti, TrackBits track, RailType rt, RailGroundType rgt, PaletteID ghost_pal)`
  - It takes the rail type and ground type as parameters instead of reading them.
  - `ghost_pal == PAL_NONE` means normal drawing. Otherwise every sprite gets `PALETTE_MODIFIER_TRANSPARENT` and `ghost_pal`.
  - It skips reservation overlays when `ghost_pal != PAL_NONE`.
  - `DrawTrackBits` and `DrawTrackBitsOverlay` become thin wrappers that read the map and call it with `PAL_NONE`. Declare it in `rail.h` beside `DrawTrainDepotSprite`.
- **Signals** (`rail_cmd.cpp`).
  - Move the bit-to-position mapping out of `DrawSignals` (`:2436`) into `static std::pair<SignalOffsets, uint> GetSignalImageAndPos(Track track, uint signal_bit)`.
  - Add `void DrawRailSignalGhost(TileIndex tile, RailType rt, Trackdir facing, SignalType type, SignalVariant variant, PaletteID ghost_pal)`. It works out the sprite as `DrawSingleSignal` does (`:1901`), but from its parameters, in `SignalState::Green`.
  - `DrawSignals` uses the shared helper.
- **Bridges** (`tunnelbridge_cmd.cpp`). Add:
  - `void DrawBridgeHeadGhost(TileInfo *ti, BridgeType type, RailType rt, DiagDirection dir, PaletteID ghost_pal)`
  - `void DrawBridgeMiddleGhost(const TileInfo *ti, BridgeType type, RailType rt, Axis axis, int bridge_z, uint north_len, uint south_len, PaletteID ghost_pal)`

  Both are copies of the sprite logic in `DrawTile_TunnelBridge`'s head branch and in `DrawBridgeMiddle` (`:1604`), taking parameters instead of reading the map. They don't draw reservation, snow or catenary. Where the copied logic can be shared through a helper without changing the real drawing, share it. Declare both in `bridge.h`.
- **Ghost palettes.** A valid ghost uses `PALETTE_TO_TRANSPARENT`. An invalid one uses `PALETTE_TO_STRUCT_RED` with the transparent bit set, which gives a transparent remap (`gfx.cpp:1007-1010`).

### Ghost store and viewport hook (`junction_gui.cpp`, `viewport.cpp`)

**The store** is `JunctionGhost`, a static `std::unordered_map<TileIndex, JunctionGhostTile>`.
- `JunctionGhostTile` is `{TrackBits tracks; std::vector<JunctionSignalStep> signals; std::optional<BridgeHeadGhost> head; std::optional<BridgeSpanGhost> span; bool bad;}`.
- `JunctionGhost::Set(const JunctionLayout &, TileIndex origin, const JunctionEstimate &, RailType, bridge types)` marks every old and new tile dirty with `MarkTileDirtyByTile`. Span tiles use their bridge level offset.
- `JunctionGhost::Clear()` and `JunctionGhost::Get(TileIndex)` complete the interface.

**The hook:** `void DrawJunctionGhost(const TileInfo *ti)`, called at the end of `DrawTileSelection` (`viewport.cpp:1120`). For a ghost tile:
- If the tile isn't flat, it draws the same red selection outline the terraform tools show for an invalid tile (`PALETTE_SEL_TILE_RED`, as `DrawTileSelectionRect` does), with `PALETTE_MODIFIER_TRANSPARENT` set so it has the ghost's opacity (D40), and nothing else.
- Otherwise it draws the track, then the bridge head or span, then the signals. The palette is red if `bad` is set, and otherwise transparent.

**Culling:** the visibility check at `viewport.cpp:1294` uses the real `IsBridgeAbove`. Where it decides a tile's vertical extent, also allow for a ghost span on that tile, so a ghost deck isn't clipped. If that can't be done without changing behaviour for real tiles, stop and report.

### Window, tool and hover (`junction_gui.cpp`, `rail_gui.cpp`)

- **Toolbar button.** `WID_RAT_BUILD_JUNCTION` is added after `WID_RAT_BUILD_TUNNEL` in `RailToolbarWidgets`.
  - It follows the nested-widget pattern at `rail_gui.cpp:931`, using `SPR_IMG_AUTORAIL` as a placeholder icon, with tooltip `STR_RAIL_TOOLBAR_TOOLTIP_BUILD_JUNCTION`.
  - It gets hotkey `"junction"` with no default key, the autorail cursor of `_cur_railtype`, and highlight style `HT_RECT`.
  - In `OnClick`: `if (started) ShowBuildJunctionWindow(this);`.
  - In `OnPlaceObject`: `JunctionToolClick(tile)`.
  - In `OnPlaceObjectAbort`: `CloseWindowById(WindowClass::BuildJunction, TransportType::Rail)` and `JunctionGhost::Clear()`.
- **Window.** `WindowClass::BuildJunction` is new in `window_type.h`. `BuildJunctionWindow : PickerWindowBase` has:
  - a caption
  - a one-line junction name (only the test junction for now)
  - **Rotate** and **Mirror** push buttons
  - a status line, for "Built N of M steps"
  - **Retry** and **Cancel**, shown only when the build has failed

  Its `HotkeyList` `"buildjunction"` has `Hotkey(WKC_PERIOD, "rotate", ...)` and `Hotkey(WKC_COMMA, "mirror", ...)`. Neither key is used elsewhere; the letter keys are taken, for example `R` is remove and `Z` is centre-zoom.
- **Hover.** `BuildJunctionWindow::OnRealtimeTick` compares the tile under the cursor (`_thd.pos` as a tile, or `GetTileBelowCursor()`) with the last one. When either the tile or the transform changes, it:
  1. Transforms the base layout by the current rotation and mirror state.
  2. Calls `SetTileSelectSize(size.x, size.y)`.
  3. Runs the estimate with `GameJunctionPricing(_cur_railtype)`.
  4. Calls `JunctionGhost::Set`.
  5. Sets `_thd.make_square_red = !estimate.affordable`.
  6. Shows the price by calling `GuiShowTooltips(this, text, TooltipCloseCondition::ExitViewport)`, where the text is one of:
     - `STR_JUNCTION_TOOLTIP_COST` (`{BLACK}Cost: {CURRENCY_LONG}`)
     - `STR_JUNCTION_TOOLTIP_UNAFFORDABLE` (`{RED}Cost: {CURRENCY_LONG} (you have {CURRENCY_LONG})`)
     - `STR_JUNCTION_TOOLTIP_INVALID` (`{RED}Can't build here: {STRING}`)

  While the builder is in Building or Failed state, the hover updates are paused.
- **Click.** `JunctionToolClick(tile)` starts the builder for the current origin and transformed layout. It does nothing while a build is in progress.

## Steps

Each step is one Sonnet agent and one commit. Tests come outermost first, and each must be seen to fail on its assertion before the code exists. `StubJunctionPricing` is used throughout:
- defaults: flat at height 1 everywhere on the map, rail 100 per piece, bridge 1000 each, signal 50, money 1,000,000
- per-tile overrides: `SetNotFlat`, `SetHeight`, `SetRailFails(tile, StringID)`, `SetBridgeFails`
- tests call `Map::Allocate(64, 64)`

1. **API skeleton and the outer test.**
   - Add `junction_layout.h` as specified, with `junction_layout.cpp` returning defaults: the input layout unchanged, empty steps, an empty estimate, and an empty layout from `MakeTestJunctionLayout`.
   - Add the test file and register it with CMake.
   - Outer test, **"test junction on flat clear land prices exactly"**: the origin is (10,10), and the expected total is 8+1 rail pieces × 100 + 1 bridge × 1000 + 2 signals × 50 = 2000. It must be affordable with no bad tiles. Red.
2. **Test junction and build steps.**
   - Inner tests:
     - `MakeTestJunctionLayout` contains the pieces listed above
     - `MakeJunctionBuildSteps` orders all rail steps before bridges and bridges before signals, with tiles = origin + offset
     - an off-map origin gives `INVALID_TILE` steps that the estimate will treat as bad
   - Implement both functions.
3. **Estimate.** This makes the step-1 outer test green.
   - Further outer tests:
     - **one non-flat tile**: that tile is in `bad_tiles` and the result isn't affordable
     - **height mismatch**
     - **rail step fails**: the error is recorded and the other tiles are still checked
     - **bridge fails**
     - **too expensive**: money 1999 is unaffordable, and 2000 is affordable
     - **layout partly off-map**
   - Implement `EstimateJunction`.
4. **Transforms.**
   - Outer tests:
     - rotating the test junction 4 times returns the original exactly, element by element
     - mirroring twice returns the original
     - a rotated test junction still prices at 2000 on flat land
     - the size swaps on rotation
   - Inner tests:
     - the edge table: every track maps to a valid track under rotation and mirroring
     - known cases: rotating `Track::X` gives `Track::Y`; rotating `Trackdir::X_NE` gives `Trackdir::Y_SE`; mirroring `Track::Upper` gives `Track::Left`; mirroring `Trackdir::X_NE` gives `Trackdir::X_SW`
     - bridge start/end ordering after a transform
   - Implement the transforms.
5. **Game pricing.**
   - Add `GameJunctionPricing` as specified.
   - Characterisation tests on a `Map::Allocate` map: `GetFlatHeight` reports a flat tile's height, `std::nullopt` for a raised corner (`SetTileHeight`), and `std::nullopt` for an off-map tile.
   - `TestRail`, `PriceBridge` and the money lookup need a running game and aren't unit-tested. They're checked in step 9.
6. **Drawing refactors.**
   - The track, signal and bridge ghost functions and helpers, as specified.
   - Build `openttd`. No unit tests. Manual check (step 9): existing track, signals and bridges look exactly as before.
7. **Ghost store and viewport hook.**
   - Inner tests: `JunctionGhost::Set`, `Get` and `Clear` contents for the test junction at an origin, including span tiles and the `bad` flag from an estimate.
   - Then add the hook, the culling allowance, and the dirty marking.
8. **Window, tool, hover and tooltip,** plus the toolbar wiring, strings and hotkeys. No unit tests beyond building cleanly.
9. **Builder and failure handling.**
   - Inner test: `MakeJunctionBuildSteps` resume semantics, i.e. resuming from index k posts steps k…n.
   - Add `JunctionBuilder`, `CcJunctionStep`, Retry and Cancel.
   - Then the manual check pass, which the engineer runs from the checklist below.

Gates for every step: `cmake --build build --target openttd_test` passes and the full `./build/openttd_test` run is green. The baseline is 95 test cases, and the count rises only by the new tests. Steps 6–9 also build `openttd`. All code follows `CODINGSTYLE.md`. Commit messages follow the repo style (`Add:` / `Codechange:`) and end with the attribution trailer.

## Manual checks (engineer, in game)

1. Existing track, signals, bridges and NewGRF rail types draw exactly as before (step 6 refactor).
2. Selecting the tool shows the window. The ghost follows the mouse, with track, bridge and signals in the current rail type, semi-transparent.
3. The ghost and the outline turn red over a hill, water, a house or the map edge. The tooltip says why.
4. The price by the cursor matches the money actually spent after building. The tooltip goes red and building is refused when money is short.
5. `.` rotates and `,` mirrors. Every orientation builds correctly, and the signals face the right way.
6. The ghost bridge deck isn't clipped when scrolled near the edge of the view.
7. Rail, electric, monorail and maglev all build in that type, and the bridge type chosen is fast enough.
8. Force a failure (e.g. build something on a ghost tile between estimate and click, or drop money with the cheat window). The build stops, the remaining tiles go red, and the window shows "Built N of M" with Retry and Cancel. Retry completes the build.
9. In multiplayer (a local server plus a client), building works and the other client sees the result. There are no desyncs.

## Risks

- **The drawing refactors touch core rendering.** A mistake shows up visually, not in tests. That's why they're a separate step with a dedicated manual check.
- **Ghost bridge culling** may need a small viewport change. The step stops if that would alter how real tiles are drawn.
- **`QueryCost` bridge prices** use the bridge window's formula, not the command's own. If the two disagree in practice, check 4 will show it, and the estimate switches to a full test run.
