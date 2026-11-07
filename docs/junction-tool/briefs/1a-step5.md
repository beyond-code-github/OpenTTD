# Brief: 1a step 5, game-side junction pricing

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- **Work only in the worktree `/home/pete/OpenTTD-wt/step5`, on branch `junction/1a-step5`, head `{HEAD}`.**
  - It has its own configured build directory, `build/` (Ninja, clang-18), already built.
  - Don't touch `/home/pete/OpenTTD` or any other worktree. Other agents are working there.
- Plan: `/home/pete/OpenTTD/docs/junction-tool/PLAN-1a.md`, "Game side" (the `GameJunctionPricing` part only) and "Steps" item 5. Where this brief differs from the plan, this brief wins.
- `src/junction_layout.h` declares the abstract `JunctionPricing`. In this worktree, `EstimateJunction` and the transforms are still stubs; that's fine, this step doesn't use them.
- The baseline in this worktree is 100 test cases: 99 pass, plus the known-red outer test "JunctionLayout outer: test junction on flat clear land prices exactly". It stays red here; it's fixed on another branch.
- Rebase-friendliness matters (D42): this step adds new files only, plus two CMake list lines.

## 2. Read first, once each (really read them)

- `CODINGSTYLE.md`, in full.
- `src/junction_layout.h`.
- `src/bridge_gui.cpp` lines 380–445: the bridge price calculation and the `QueryCost` pattern to copy.
- `src/bridge.h` lines 15–75, and `src/tunnelbridge.h` lines 20–35 (`GetTunnelBridgeLength`).
- `src/rail.h` around lines 230–245 (`RailTypeInfo::max_speed`) and around line 448 (`RailBuildCost`).
- `src/tile_map.h` lines 25–70 and 260–290 (`TileHeight`, `SetTileHeight`, `GetTileSlopeZ`, `IsValidTile`).
- `src/tests/junction_layout.cpp`, the first 40 lines, for test style only.

## 3. Tests (write first, see them fail)

Create `src/tests/junction_game.cpp` (licence header, `@file`, includes as in `junction_layout.cpp` tests, `safeguards.h` last) and register it in `src/tests/CMakeLists.txt` after `history_func.cpp` and before `junction_layout.cpp`. Each test calls `Map::Allocate(64, 64)` and constructs `GameJunctionPricing pricing(RAILTYPE_RAIL);`:

1. **`"GameJunctionPricing: flat tile reports its height"`:** `pricing.GetFlatHeight(TileXY(10, 10)) == 0`. A fresh map is flat at height 0.
2. **`"GameJunctionPricing: raised corner is not flat"`:** after `SetTileHeight(TileXY(11, 11), 1)`, `pricing.GetFlatHeight(TileXY(10, 10)) == std::nullopt`. That raises the S corner of (10,10).
3. **`"GameJunctionPricing: raised flat tile reports its height"`:** set the heights of (20,20), (21,20), (20,21) and (21,21) to 2. Then `pricing.GetFlatHeight(TileXY(20, 20)) == 2`.
4. **`"GameJunctionPricing: off-map tile has no height"`:** `pricing.GetFlatHeight(INVALID_TILE) == std::nullopt`.

**Red procedure:** first write the class with every method stubbed (`GetFlatHeight` returns `std::nullopt`; the others return `CommandCost()` or 0). Build, and run `./build/openttd_test "GameJunctionPricing:*"`. Tests 1 and 3 must fail on their assertions. Tests 2 and 4 may pass against the stub; that's expected. Record the failures, then implement.

If `Map::Allocate` alone isn't enough for `GetTileSlopeZ` to work (a crash or assert), stop and report. Don't add other game initialisation.

## 4. Changes

### `src/junction_game.h` (new; guard `JUNCTION_GAME_H`)

```cpp
#include "junction_layout.h"
#include "rail_type.h"
#include "bridge_type.h"

/** Cheapest available bridge type at least as fast as the rail type, for a bridge with 'length' middle tiles. */
std::optional<BridgeType> ChooseJunctionBridgeType(RailType railtype, uint length);

/** Junction pricing against the real map, commands and prices, for the local company. */
class GameJunctionPricing : public JunctionPricing {
public:
	explicit GameJunctionPricing(RailType railtype) : railtype(railtype) {}
	std::optional<int> GetFlatHeight(TileIndex tile) const override;
	CommandCost TestRail(TileIndex tile, Track track) const override;
	CommandCost PriceBridge(TileIndex start, TileIndex end) const override;
	Money SignalCost() const override;
	Money AvailableMoney() const override;

private:
	RailType railtype; ///< Rail type the junction is built in.
};
```

Give each method a doc comment in the .cpp, matching house style.

### `src/junction_game.cpp` (new)

Use the licence header, `@file`, `stdafx.h` first and `safeguards.h` last. Include exactly what's needed. Likely: `junction_game.h`, `command_func.h`, `rail_cmd.h`, `tunnelbridge_cmd.h`, `bridge.h`, `tunnelbridge.h`, `rail.h`, `economy_func.h`, `company_func.h`, `tile_map.h`, `slope_type.h`, `table/strings.h`.

- **`GetFlatHeight`:** if `!IsValidTile(tile)`, return `std::nullopt`. Otherwise `auto [slope, z] = GetTileSlopeZ(tile);`, and return `z` if `slope == SLOPE_FLAT`, else `std::nullopt`.
- **`TestRail`:** `return Command<Commands::BuildRail>::Do(CommandFlagsToDCFlags(GetCommandFlags<Commands::BuildRail>()) | DoCommandFlag::QueryCost, tile, this->railtype, track, false);`
- **`ChooseJunctionBridgeType(railtype, length)`:**
  1. Loop over `type` from 0 to `MAX_BRIDGES - 1`. Skip any type where `CheckBridgeAvailability(type, length).Failed()`.
  2. Let `rail_speed = GetRailTypeInfo(railtype)->max_speed`.
  3. If `rail_speed != 0`: among the types whose `GetBridgeSpec(type)->speed >= rail_speed`, return the one with the lowest `spec->price`. On a price tie, take the lowest type index.
  4. If `rail_speed == 0` (unlimited): return the available type with the highest `speed`. On a speed tie, take the lowest `price`, then the lowest index.
  5. If no type qualifies, return `std::nullopt`.
- **`PriceBridge(start, end)`:**
  1. Set `length = GetTunnelBridgeLength(start, end)` and `type = ChooseJunctionBridgeType(this->railtype, length)`. If there's no type, return `CommandCost(STR_ERROR_CAN_T_BUILD_BRIDGE_HERE)`.
  2. Run `CommandCost ret = Command<Commands::BuildBridge>::Do(CommandFlagsToDCFlags(GetCommandFlags<Commands::BuildBridge>()) | DoCommandFlag::QueryCost, end, start, TransportType::Rail, *type, this->railtype, INVALID_ROADTYPE);`. Note the argument order: end, then start, as in `bridge_gui.cpp`. If it failed, return `ret`.
  3. Add the bridge window's formula, exactly as in `bridge_gui.cpp`: `((int64_t)CalcBridgeLenCostFactor(length + 2) * _price[Price::BuildBridge] * spec->price) >> 8`, plus `(length + 2) * RailBuildCost(this->railtype)`.
  4. Return `CommandCost(ExpensesType::Construction, total)`.

  If `bridge_gui.cpp`'s formula differs in any detail from this description, copy `bridge_gui.cpp` exactly, and report the difference.
- **`SignalCost`:** `return _price[Price::BuildSignals];`
- **`AvailableMoney`:** `return GetAvailableMoney(_local_company);`

### `src/CMakeLists.txt`

Add `junction_game.cpp` and `junction_game.h` to the main `add_files(` list in alphabetical order, just before `junction_layout.cpp`.

## 5. Do not

- Touch any existing source file other than the two `CMakeLists.txt` files.
- Add `JunctionBuilder`, any GUI or drawing code, or anything from steps 6–9.
- Try to unit-test `TestRail`, `PriceBridge`, `ChooseJunctionBridgeType`, `SignalCost` or `AvailableMoney`. They need a running game, and step 9's in-game check covers them.
- Change git config, or work outside the worktree.

## 6. Stop conditions

- A named function, constant or header doesn't exist as described (for example `IsValidTile`, `CommandFlagsToDCFlags`, `GetAvailableMoney`, `Price::BuildSignals`).
- `GetTileSlopeZ` needs more than `Map::Allocate` in tests.
- A test can't be made red as described, or an existing test (other than the known-red outer test) fails.
- Anything not covered here.

## 7. Gates

Logs go to `.../s5_*.log`; grep them.

1. `cmake --build build --target openttd_test openttd`, with no errors and no warnings in the new files. The game target must also link, since `junction_game.cpp` is in the game library.
2. Red run recorded, then the green run: all four new tests pass.
3. Full suite: 104 test cases, 103 passing, and the known outer test failing.
4. Commit only the four new or changed files on `junction/1a-step5`, with this message:

   ```
   Add: Game-side junction pricing

   Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>
   ```

## 8. Report format

- **References read**
- **Commits:** hash and subject
- **Red evidence:** each new test and its failure line, or "passed against stub (expected)"
- **Types and members added or removed:** names only
- **Suite results:** the summary line
- **Manual checks:** none
- **Stops, and anything left undone,** plus any `bridge_gui.cpp` formula difference
- **State:** `git status --porcelain` and `git log -1 --format='%h %s'`, both run in the worktree

When you have reported, stop.
