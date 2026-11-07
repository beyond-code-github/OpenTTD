# Brief: 1a step 9d, don't count clearing twice under bridges

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- Repo `/home/pete/OpenTTD` (main checkout), branch `feature/junction-tool`, head `{HEAD}`. You are the only agent here. A copy of the game (`build/openttd-step9c`) may be running. Don't touch it.
- **Decision D50** in `docs/junction-tool/DECISIONS.md`. The estimate is £428 too high, because a tile that has track **and** lies under a bridge span has its clearing cost (e.g. trees) charged twice: once by `TestRail` and once by `PriceBridge`. `CmdBuildBridge` clears span tiles that have no build-above handler (`tunnelbridge_cmd.cpp:288-294`). The real build places the rail first, so it pays once.
- Baseline: `{BASELINE}`, all passing.

## 2. Read first, once each

- `CODINGSTYLE.md` (no need to quote a rule).
- `src/junction_layout.h` (`JunctionPricing`, `EstimateJunction`) and `src/junction_layout.cpp` (`EstimateJunction`).
- `src/junction_game.h` and `src/junction_game.cpp` (`GameJunctionPricing`).
- `src/tests/junction_layout.cpp` (`StubJunctionPricing` and the estimate tests).
- `src/landscape_cmd.h`, for the `Commands::LandscapeClear` signature.

## 3. Tests (write first, see them fail)

1. **Extend `StubJunctionPricing`** with `std::map<TileIndex, Money> clear_costs;` (the default is 0 for tiles not in the map) and an override of the new `TestClear`, which returns `CommandCost(ExpensesType::Construction, cost)`.
2. **The new outer test, `"JunctionLayout outer: clearing under a bridge over track is counted once"`:**
   - Setup: the test junction at origin `TileXY(10, 10)`, with `pricing.clear_costs[TileXY(14, 12)] = 428`.
   - Expect: `estimate.total == 2000 - 428` (i.e. 1572), the estimate is affordable, and `bad_tiles` is empty.
   - Explanation: the stub's `TestRail` and `PriceBridge` don't model clearing, so subtracting 428 shows the overlap rule is applied once. Comment this in the test.
3. **A second new test, `"JunctionLayout: clearing elsewhere is not subtracted"`:** with `clear_costs[TileXY(10, 12)] = 428` (track but no bridge over it) and `clear_costs[TileXY(14, 11)] = 300` (bridge span but no track), expect `total == 2000`.

**Red:** add the interface method and a stub `GameJunctionPricing::TestClear` that returns `CommandCost()`. Build and run, and record the failures. Test 2 must fail; test 3 may pass.

## 4. Changes (one commit)

- **`JunctionPricing`:** add
  ```cpp
  /** Test-only run of clearing a tile; its cost is what building anything on the tile would pay to clear it first. */
  virtual CommandCost TestClear(TileIndex tile) const = 0;
  ```
- **`GameJunctionPricing::TestClear`:** `return Command<Commands::LandscapeClear>::Do(CommandFlagsToDCFlags(GetCommandFlags<Commands::LandscapeClear>()) | DoCommandFlag::QueryCost, tile);`. Include `landscape_cmd.h`. Declare it in `junction_game.h`.
- **`EstimateJunction`,** after pricing (the existing rule 5): for each **valid** tile that appears both as a track tile of the layout **and** as a span tile of a bridge (strictly between the heads):
  - only if that tile isn't already in `bad_tiles`, and the bridge and that tile's rail steps succeeded
  - call `CommandCost c = pricing.TestClear(tile);`, and if `c.Succeeded()`, subtract `c.GetCost()` from `total`

  Count each tile once, even if it has several track bits. Update the rule list in the function's doc comment, adding "6. Clearing under a bridge over track is counted once (D50)".

Commit message: `Fix: Junction estimate counted clearing twice for track under a bridge`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 5. Do not

- Change any other estimate rule, the build order, or the GUI.
- Touch upstream files.

## 6. Stop conditions

- The `Commands::LandscapeClear` signature differs from `(DoCommandFlags, TileIndex)`.
- Anything not covered here.

## 7. Gates

1. Build `openttd` and `openttd_test` with no errors and no warnings.
2. The red run is recorded. The full suite is `{BASELINE}` plus 2, all passing.
3. One commit touching only `junction_layout.h/.cpp`, `junction_game.h/.cpp` and `tests/junction_layout.cpp`.

Logs go to `.../s9d_*.log`.

## 8. Report format

- **References read**
- **Commits**
- **Red evidence**
- **Types and members added or removed**
- **Suite results**
- **Manual checks for the engineer:** pause, read the window cost, build the test junction over trees, and the money change equals the cost to the pound
- **Stops, and anything left undone**
- **State:** `git status --porcelain` and `git log -1 --format='%h %s'`

When you have reported, stop.
