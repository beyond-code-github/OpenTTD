# Draft: endpoint link tool (superseded as M1 by D11; kept for the endpoint milestone)

The player picks the new **Link** tool on the rail toolbar. They click one open track end, then another. The game finds the cheapest single-track route between the two ends over the existing terrain and shows it as a highlight. A small window then shows the exact cost against the money available. If the player confirms, the game builds the route with ordinary build commands.

This milestone covers the whole pipeline (tag, solve, preview, cost, build) for the simplest case. Multi-track links, junction cores, terraforming and the junctionary all build on it later.

Decisions this plan rests on are in `DECISIONS.md` (D1–D9). The open decisions D10–D13 are listed at the end. Steps that depend on them are marked.

## Scope

**In:**
- One track per tile. The route uses plain rail on tiles that are `TileType::Clear` or `TileType::Trees`.
- Foundations, as the game applies them when `build_on_slopes` is on.
- The rail type of the open rail toolbar.
- The 90-degree turn rule.
- An exact cost shown up front, and the sequential build with failure handling.

**Out (later milestones):**
- Terraforming (D10)
- Bridges and tunnels
- Crossing roads, rail or water
- Demolition
- Signals
- Multiple parallel tracks
- Junction cores and the junctionary
- Ghost sprites drawn in the rail type's own graphics (D13)

## Design

### Files

| File | Contents |
|---|---|
| `src/rail_link.h` | Types, the two interfaces, and the declarations of the pure functions |
| `src/rail_link.cpp` | Solver, edge-height model, end detection, estimate. Pure: nothing here reads global game state |
| `src/rail_link_game.cpp` | `MapLinkTerrain` and `GameLinkRules` (real map and settings), the command test-runner, and the build sequencer |
| `src/rail_link_gui.cpp` | Preview store, the confirm window, and the tool glue called from `rail_gui.cpp` |
| `src/widgets/rail_link_widget.h` | Widget enum for the confirm window |
| `src/tests/rail_link.cpp` | Catch2 tests with `StubLinkTerrain` and `StubLinkRules` |

Register the game files in `src/CMakeLists.txt` (alphabetical `add_files`) and the test file in `src/tests/CMakeLists.txt` (`add_test_files`). Tests call `Map::Allocate(64, 64)` so `TileXY` and `TileOffsByDiagDir` work. Terrain comes from the stub, not the map.

### Types (`src/rail_link.h`)

```cpp
/** An open end of existing track: the tile, and the trackdir that leaves it through the open edge. */
struct RailLinkEnd {
	TileIndex tile;
	Trackdir exit_trackdir;
	RailType railtype;
};

/** One piece of track to build. */
struct RailLinkStep {
	TileIndex tile;
	Track track;
};

/** Why a solve or an end lookup failed. */
enum class RailLinkError : uint8_t {
	None,
	NoRail,         ///< Clicked tile has no rail.
	NoOpenEnd,      ///< Clicked tile's track is connected at every end.
	AmbiguousEnd,   ///< Clicked tile has more than one open end.
	RailTypeMismatch, ///< An end's rail type can't join the tool's rail type (D12).
	NoRoute,        ///< Search exhausted without reaching the target.
	SearchLimit,    ///< Search hit the node limit.
};

struct RailLinkEndResult {
	std::optional<RailLinkEnd> end;
	RailLinkError error = RailLinkError::None;
};

struct RailLinkPlan {
	std::vector<RailLinkStep> steps; ///< In route order from 'from' to 'to'.
	RailLinkError error = RailLinkError::None;
};

/** Up-front cost of a plan, from test-only runs of each build command. */
struct RailLinkEstimate {
	Money total = 0;
	std::vector<size_t> failed_steps; ///< Indices into RailLinkPlan::steps whose test run failed.
	StringID first_error = INVALID_STRING_ID;
	bool affordable = false; ///< No failed steps and total <= available money.
};
```

### Interfaces (D6, D8)

They follow the house pattern of an abstract base with pure virtual members and a test subclass (`FontCache` / `MockFontCache`).

```cpp
/** What the link solver may know about the map. */
class LinkTerrain {
public:
	virtual ~LinkTerrain() = default;
	/** Tile is inside the map and not void. */
	virtual bool IsValidTile(TileIndex tile) const = 0;
	/** Slope and base height (in height levels, as GetTileSlopeZ). */
	virtual std::tuple<Slope, int> GetTileSlopeZ(TileIndex tile) const = 0;
	/** New plain rail may be built here without clearing anything but trees or rough land. */
	virtual bool IsBuildable(TileIndex tile) const = 0;
	/** Rail tracks on the tile that a link can connect to; empty if none. */
	virtual TrackBits GetRailTracks(TileIndex tile) const = 0;
	/** Rail type of the tile's track; INVALID_RAILTYPE if none. */
	virtual RailType GetRailType(TileIndex tile) const = 0;
};

/** Rail-type and setting rules the solver needs. Penalties shape the route; they are not displayed costs. */
class LinkRules {
public:
	virtual ~LinkRules() = default;
	virtual Money TrackCost(RailType rt) const = 0;
	virtual Money FoundationCost() const = 0;
	virtual Money CurvePenalty(RailType rt) const = 0;
	virtual Money SlopePenalty(RailType rt) const = 0;
	virtual bool Disallow90Deg(RailType rt) const = 0;
	virtual bool BuildOnSlopes() const = 0;
	/** May a link of rail type 'link' join existing track of rail type 'existing'? (D12) */
	virtual bool CanJoin(RailType link, RailType existing) const = 0;
};
```

### Pure functions (`src/rail_link.h`)

```cpp
RailLinkEndResult FindRailLinkEnd(const LinkTerrain &terrain, const LinkRules &rules, TileIndex tile, RailType railtype);
std::optional<int> GetRailLinkEdgeHeight(Slope slope, int z, TrackBits tile_tracks, DiagDirection edge, bool build_on_slopes);
RailLinkPlan SolveRailLink(const LinkTerrain &terrain, const LinkRules &rules, const RailLinkEnd &from, const RailLinkEnd &to, RailType railtype);
RailLinkEstimate EstimateRailLink(const RailLinkPlan &plan, const std::function<CommandCost(const RailLinkStep &)> &test_run, Money available);
```

### Edge-height model (`GetRailLinkEdgeHeight`)

Every rail track touches its tile's edges at their midpoints. Two consecutive route tiles join if the track surface heights at their shared edge midpoint match. The function:
1. Computes `f = GetRailFoundation(slope, tile_tracks)`. It returns `std::nullopt` if `f == Foundation::Invalid`, or if `f != Foundation::None` and `!build_on_slopes`.
2. Copies `slope`, then calls `ApplyFoundationToSlope(f, applied)`. That returns a height-level increment `dz`.
3. Calls `GetSlopePixelZOnEdge(applied, edge, (z + dz) * TILE_HEIGHT)`, which returns `{z1, z2}`.
4. Returns `z1 + z2`. This is twice the midpoint height, which avoids halving.

The route is continuous when the exit-edge value of tile *n* equals the entry-edge value of tile *n+1*. The entry edge for tile *n+1* is `ReverseDiagDir` of tile *n*'s exit edge. This is the plan's central assumption about the game's geometry. Step 2 pins it on known slopes, and step 10's in-game check confirms it.

### Search (`SolveRailLink`)

- **Node:** `(TileIndex tile, Trackdir td)`. The node also stores its g-cost, its parent index (into a `std::vector` of nodes) and its exit-edge height. Height isn't part of the state, because without terraforming it's fixed by the terrain.
- **Start:** the tile beyond `from`, i.e. `from.tile + TileOffsByDiagDir(TrackdirToExitdir(from.exit_trackdir))`. The candidate trackdirs there are `TrackdirReachesTrackdirs(from.exit_trackdir)`. Entry continuity is checked against `from`'s exit-edge height. That height is computed with `from.tile`'s own `GetRailTracks` bits, because the foundation depends on every track on the tile.
- **Expansion from node (t, td):**
  1. Set `exit = TrackdirToExitdir(td)` and `next = t + TileOffsByDiagDir(exit)`.
  2. Set `cands = TrackdirReachesTrackdirs(td)`. If `rules.Disallow90Deg(railtype)`, remove every trackdir whose track is in `TrackCrossesTracks(TrackdirToTrack(td))`. This is the same pattern as `train_cmd.cpp:3001`.
  3. Reject `next` if it is invalid, not buildable, outside the search box, or already on this node's path. Check the path by walking the parents, so each tile is used at most once.
  4. Reject a candidate whose entry-edge height doesn't match the node's exit-edge height, or whose `GetRailLinkEdgeHeight` returns `std::nullopt`.
- **Goal:** expanding (t, td) reaches `to` when `next == to.tile` and `ReverseTrackdir(to.exit_trackdir)` is in `cands`, and the heights match against `to.tile`'s own track bits.
- **Step cost:**
  - `TrackCost(railtype)`
  - plus `FoundationCost()` if the tile's foundation `!= Foundation::None`
  - plus `CurvePenalty(railtype)` if `TrackdirDirection(td_new) != TrackdirDirection(td_prev)`
  - plus `SlopePenalty(railtype)` if the entry and exit edge heights differ
  - `TrackdirDirection` is a static table in `rail_link.cpp`: X_NE→NE, Y_SE→SE, Upper_E/Lower_E→E, Left_S/Right_S→S, X_SW→SW, Y_NW→NW, Upper_W/Lower_W→W, Left_N/Right_N→N. This makes a straight diagonal run (alternating Upper_E/Lower_E) count as no curve.
- **Heuristic:** the Manhattan tile distance to `to.tile` × `TrackCost(railtype)`. It's admissible because each step moves exactly one tile in x or y and costs at least `TrackCost`.
- **Open list:** `std::priority_queue` on f-cost. The closed set is a `std::unordered_set` keyed on `(tile, td)`.
- **Limits:** the search box is the bounding box of the two ends, grown by 16 tiles (`RAIL_LINK_SEARCH_MARGIN`). The node limit is 100000 (`RAIL_LINK_NODE_LIMIT`), and hitting it gives `RailLinkError::SearchLimit`.
- **Result:** steps are reconstructed from the goal node back through the parents, then reversed. Each step is `{tile, TrackdirToTrack(td)}`.
- **Optimality:** walking back through the parents and the closed set make the result near-optimal rather than guaranteed optimal. That's accepted for M1.

### End detection (`FindRailLinkEnd`)

1. If `terrain.GetRailTracks(tile)` is empty, the result is `NoRail`.
2. For each track on the tile, and each of its two trackdirs `td`:
   - Set `exit = TrackdirToExitdir(td)` and `n = tile + TileOffsByDiagDir(exit)`.
   - The end is open if `n` is valid and `(terrain.GetRailTracks(n) & DiagdirReachesTracks(exit))` is empty.
3. With no open ends, the result is `NoOpenEnd`. With more than one, it's `AmbiguousEnd`. If `!rules.CanJoin(railtype, terrain.GetRailType(tile))`, it's `RailTypeMismatch`. Otherwise the result is `{tile, td, terrain.GetRailType(tile)}`.

### Cost up front (D9) and the build (D7)

- **The estimate is exact for M1.** Every step is a single `Commands::BuildRail` on a distinct tile, and nothing is terraformed, so the test-only runs don't depend on each other. Their sum is the real cost, including tree clearing and foundations.
- **The test-runner** lives in `rail_link_game.cpp` and follows the pattern at `bridge_gui.cpp:396`:
  `Command<Commands::BuildRail>::Do(CommandFlagsToDCFlags(GetCommandFlags<Commands::BuildRail>()) | DoCommandFlag::QueryCost, step.tile, railtype, step.track, false)`
- **`EstimateRailLink`** runs `test_run` on every step, without stopping at the first failure, so all bad tiles can be shown. It sums the successful costs, records the indices of failed steps and the first error message, and sets `affordable = failed_steps.empty() && total <= available`.
- **Available money** is `GetAvailableMoney(_local_company)` (`company_func.h:28`).
- **The confirm window** shows the cost and the money available. Build is disabled unless the plan is affordable. Failed steps are drawn red in the preview.
- **The build sequencer** is `RailLinkBuilder`, held in a static in `rail_link_game.cpp`:
  - When Build is pressed, it re-estimates. If the plan is no longer affordable, it stops and refreshes the window.
  - Otherwise it posts step 0 with `Command<Commands::BuildRail>::Post(STR_ERROR_CAN_T_BUILD_RAILROAD_TRACK, CcRailLinkStep, tile, railtype, track, false)`.
  - `CcRailLinkStep(Commands, const CommandCost &result, TileIndex tile)` posts the next step on success.
  - On failure it stops. The steps already built are left in place, the failed step and every step after it are marked red, and the window shows "Built N of M pieces" with Retry and Cancel. Retry re-solves from the current ends.
  - `Post` already shows the game's own error message.
  - Each step costs one network round trip in multiplayer. That's accepted for M1.

### Preview (`rail_link_gui.cpp` and `viewport.cpp`)

- **`RailLinkPreview`** is a client-side `std::unordered_map<TileIndex, RailLinkPreviewTile>`, where `RailLinkPreviewTile` is `{TrackBits tracks; bool failed;}`. It has `Set(plan, failed_steps)` and `Clear()`. Both call `MarkTileDirtyByTile` on every tile they add or remove.
- **The hook** goes in `DrawTileSelection` (`viewport.cpp:1120`). If the tile is in the preview, it calls the existing autorail drawing (`DrawAutorailSelection`, `viewport.cpp:977`) once per track. Tracks map to styles like this: X→`HT_DIR_X`, Y→`HT_DIR_Y`, Upper→`HT_DIR_HU`, Lower→`HT_DIR_HL`, Left→`HT_DIR_VL`, Right→`HT_DIR_VR`. A failed tile uses the red palette that function already supports.
- **Rail-type sprites** are deferred (D13).

### Tool (`rail_gui.cpp`)

- **Widget:** add `WID_RAT_LINK` to `RailToolbarWidgets` (`widgets/rail_link_widget.h` is for the confirm window only).
- **The button** follows the nested-widget pattern at `rail_gui.cpp:931`. For M1 it reuses the `SPR_IMG_AUTORAIL` sprite (it needs a proper icon later) and has a new tooltip `STR_RAIL_TOOLBAR_TOOLTIP_BUILD_LINK`.
- **Toolbar switches:**
  - Hotkey `"link"` with no default key.
  - Cursor: the autorail cursor of `_cur_railtype`.
  - `GetHighLightStyleForWidget`: `HT_RECT`.
- **`OnPlaceObject`, case `WID_RAT_LINK`:** this calls `RailLinkToolClick(tile, _cur_railtype)` in `rail_link_gui.cpp`.
  - The first click resolves `from`. On error it shows the matching error string.
  - The second click resolves `to`, solves, estimates, sets the preview and opens the confirm window.
- **`OnPlaceObjectAbort`:** this calls `RailLinkToolAbort()`, which clears the preview and the pending end and closes the confirm window.
- **Strings** go in `lang/english.txt`, following the `STR_RAIL_TOOLBAR_TOOLTIP_*` pattern and its alignment. There's one string per `RailLinkError`, plus the window strings.

## Steps

Each step is one Sonnet agent and one commit. Within each step the tests are written outermost first. Every test must be seen to fail on its assertion before its code exists.

**Outer tests** drive `SolveRailLink` or `FindRailLinkEnd` through `StubLinkTerrain` and `StubLinkRules`. They are named `RailLink outer: ...`.
- `StubLinkTerrain` defaults to flat, z=0, buildable and no rail. Tests override tiles with `SetSlopeZ`, `SetUnbuildable` and `SetRail(tile, bits, rt)`.
- `StubLinkRules` defaults to track 100, foundation 50, curve 0, slope 0, 90-degree turns allowed, build on slopes on, and `CanJoin` as exact match. Every value is a settable public member.

0. **Baseline.**
   - Configure `build/` as a Debug build (`cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug`), build `openttd_test` and the game, and run the tests.
   - Record the number of passing tests and the build time in `STATUS.md`. `build/` is already ignored by git (`/build*`).
   - No code changes.
1. **API skeleton and the first outer test.**
   - Add `rail_link.h` exactly as above. In `rail_link.cpp`, every function returns a default value: an empty plan with `NoRoute`, `{std::nullopt, NoRail}`, `std::nullopt`, and an empty estimate.
   - Add the test file with both stubs and register everything in CMake.
   - Outer test: **straight link on flat land**. A has `Track::X` at (10,20) with open end `X_SW`. B has `Track::X` at (15,20) with open end `X_NE`. Expect no error and steps `{(11..14,20), X}` in order. It's red on the step-count assertion.
2. **Edge-height model.**
   - Inner tests on `GetRailLinkEdgeHeight`:
     - flat z=0: every edge returns 0
     - flat z=2: every edge returns `4*TILE_HEIGHT`
     - X on `SLOPE_NE` (inclined along X): the NE and SW edges differ by `2*TILE_HEIGHT`
     - Y on `SLOPE_NE`: needs a leveled foundation, so every edge is level at the raised height
     - any foundation slope with `build_on_slopes` off: `std::nullopt`
     - an invalid combination (`TRACK_BIT_CROSS` on a steep slope): `std::nullopt`
   - Implement the function as specified.
3. **Search core.** This step makes the step-1 outer test green.
   - Further outer tests:
     - **detours round an unbuildable tile**: block (12,20) and expect a route that avoids it and still joins B
     - **reports NoRoute** when B is walled off
     - **reports SearchLimit** with the node limit lowered through a test-only overload (`SolveRailLink(..., size_t node_limit)`, which the main function calls with `RAIL_LINK_NODE_LIMIT`)
     - **diagonal link**: A and B offset diagonally; expect alternating Upper/Lower or Left/Right tracks
   - Inner tests:
     - `TrackdirDirection` table: one assertion per trackdir
     - path-uniqueness walk: a hand-built node chain reports a tile on it as used
   - The function under test is declared in `rail_link.cpp` and exposed only through `rail_link.h` under a `namespace RailLinkDetail` block for tests.
4. **Rail-type rules in the search.**
   - Outer tests:
     - **90-degree turn refused when Disallow90Deg**: terrain that forces a 90-degree turn gives `NoRoute` when the flag is on and a route when it's off
     - **curve penalty prefers the straighter route**: two equal-length routes, one with fewer curves
     - **rules are asked about the tool's rail type**: `StubLinkRules` records every `RailType` passed to it; after solving with `RAILTYPE_MONO`, every recorded value is `RAILTYPE_MONO`
5. **End detection** (`FindRailLinkEnd`), *depends on D12*.
   - Outer tests:
     - a single open end is found with the right trackdir
     - a lone isolated piece of track has two open ends, so `AmbiguousEnd`
     - track connected at both ends gives `NoOpenEnd`
     - a tile with no rail gives `NoRail`
     - a different rail type gives `RailTypeMismatch`
6. **Slopes and foundations in the search.**
   - Outer tests:
     - **follows an incline**: A at z=0 and B at z=1 joined by a tile on `SLOPE_NE`; the route goes over it
     - **refuses a height step**: a tile at z=1 between two z=0 tiles with no slope between them gives `NoRoute` or a detour
     - **slope penalty prefers the flat detour** when `SlopePenalty` is high
     - **foundation cost counted**: two routes, one needing foundations, and the cheaper one is chosen
7. **Estimate.**
   - Inner tests on `EstimateRailLink`, using a fake `test_run`:
     - all steps succeed: the total is the sum and it's affordable
     - one step fails: its index is recorded, its error is recorded, and it's not affordable
     - the total is above `available`: not affordable
     - the total equals `available`: affordable
8. **Game-side terrain and rules.**
   - `MapLinkTerrain` uses the real map:
     - `IsValidTile` → `IsValidTile(tile) && !IsTileType(tile, TileType::Void)`
     - `GetTileSlopeZ` → `::GetTileSlopeZ`
     - `IsBuildable` → `IsTileType(tile, TileType::Clear) || IsTileType(tile, TileType::Trees)`
     - `GetRailTracks`, by tile type:
       - plain rail (`IsPlainRailTile`): `GetTrackBits`
       - rail depot (`IsRailDepotTile`): the track for `DiagDirToAxis(GetRailDepotDirection)`
       - rail station (`HasStationRail`): `GetRailStationTrack`
       - rail tunnel or bridge head (`GetTunnelBridgeTransportType == TransportType::Rail`): `DiagDirToDiagTrack(GetTunnelBridgeDirection)`
       - anything else: empty
     - `GetRailType` → `::GetRailType` when `GetRailTracks` is non-empty
   - `GameLinkRules`:
     - `TrackCost` → `RailBuildCost`
     - `FoundationCost` → `_price[Price::BuildFoundation]`
     - `CurvePenalty` → `RailBuildCost(rt) / 2`
     - `SlopePenalty` → `RailBuildCost(rt) * 2`
     - `Disallow90Deg` → `Rail90DegTurnDisallowed(rt, rt)`
     - `BuildOnSlopes` → `_settings_game.construction.build_on_slopes`
     - `CanJoin` per D12
   - Characterisation tests on a `Map::Allocate` map: a fresh map is flat, valid and buildable; `MakeRailNormal(tile, OWNER_NONE, TrackBits{Track::X}, RAILTYPE_RAIL)` is reported with its tracks and rail type; a raised corner (`SetTileHeight`) is reported with the expected slope. Also add the test-runner and `RailLinkBuilder`. These aren't unit-tested, and step 10 checks them in game.
9. **Preview store and viewport hook.**
   - Inner tests: `RailLinkPreview::Set` and `Clear` contents. Expose a `RailLinkPreview::Get(tile)` lookup and test that. Don't test the dirty marking.
   - Then add the viewport hook.
   - Manual check (step 10): highlights appear and disappear correctly on flat and sloped tiles.
10. **Tool, confirm window and wiring.** *Depends on D11.*
   - Add the toolbar button, strings, the click flow, and the confirm window: cost, available money, Build disabled when unaffordable, the Retry/Cancel failure state.
   - Manual checks, run in game and reported:
     - a flat straight link
     - a diagonal link
     - an incline link, where the highlight lines up with the ground and the built track matches the highlight (this confirms the edge-height model)
     - an unaffordable plan, where Build is disabled
     - a mid-build failure, by placing an obstacle on a route tile from a second client or using the cheat window to drop money between estimate and build; the window shows "Built N of M" and red tiles
     - each rail type in a temperate game (rail, electric, monorail, maglev), where the built track has the tool's rail type

Gates for every step: `cmake --build build --target openttd_test` and the full `openttd_test` run must be green, the new tests must be in the run, and code must follow `CODINGSTYLE.md`. Steps 8–10 also build the `openttd` target. Commit messages follow the repo's style (`Add:`, `Codechange:` etc., see `git log`) and end with the attribution trailer.

## Open decisions (for the engineer)

**D10: Terraforming in M1?**
```
Context: M1 routes over the land as it is, using foundations. A route over a ridge with no
         slope in the right direction has to detour or fails.
Options:
  A. Leave terraforming out of M1. The solver stays simple (height isn't part of the search
     state), and the cost estimate is exact. Terraforming becomes M2.
  B. Include it now. Height joins the search state, corner changes propagate to neighbouring
     tiles, the estimate becomes order-dependent (terraform then rail), and M1 roughly doubles.
Recommendation: A.
Reversible: yes. The interfaces don't change, and M2 adds a height-changing move.
```

**D11: How does the player confirm?**
```
Context: After the second click the route is highlighted. The player needs to see the cost
         and choose to build.
Options:
  A. A small window: cost, money available, Build (disabled if unaffordable) and Cancel;
     later Retry. Clear, and it gives the cost a home.
  B. Click again to build, Esc to cancel, cost in the tooltip or a news line. Faster, but the
     cost is easy to miss, which works against D9.
Recommendation: A.
Reversible: yes.
```

**D12: Joining track of a different rail type**
```
Context: Electric link tool, clicked end is normal rail. The game itself lets you join them,
         but trains may not be able to run across the join.
Options:
  A. Exact match only. Simple, and no surprise unpowered sections.
  B. Allow if IsCompatibleRail(link, existing). Electric can join normal rail; a normal
     link can't join electric (only electric trains run on electric rail anyway).
  C. Allow anything; the player's call.
Recommendation: A for M1.
Reversible: yes. It's one function, CanJoin.
```

**D13: Preview graphics**
```
Context: D8 says the preview should look like the selected rail type.
Options:
  A. M1 uses the existing autorail highlight sprites (rail-type neutral, slope-aware, already
     support a red "invalid" palette). Real rail-type ghost sprites in a later milestone.
  B. Draw real rail-type sprites semi-transparent now. Needs the rail sprite tables and
     foundation drawing: a sizeable piece of its own.
Recommendation: A.
Reversible: yes.
```
