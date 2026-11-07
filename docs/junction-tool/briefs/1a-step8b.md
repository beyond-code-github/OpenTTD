# Brief: 1a step 8b, opaque ghost and bridge type for unlimited rail

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- Repo `/home/pete/OpenTTD` (main checkout), branch `feature/junction-tool`, head `{HEAD}`. You are the only agent here.
- **Why this step exists.** It comes from the engineer's in-game check of step 8 and two rulings, both in `docs/junction-tool/DECISIONS.md`:
  - **D44:** the ghost must render in normal colours at full opacity. `PALETTE_TO_TRANSPARENT` gives the darkening "glass" effect, which made it hard to see. Invalid tiles keep the red transparent remap.
  - **D45:** all default rail types have `max_speed == 0`, so `ChooseJunctionBridgeType` currently picks the fastest, and most expensive, bridge. It must instead pick the cheapest bridge fast enough for the fastest engine available.
- **Baseline:** `{BASELINE}`, all passing.

## 2. Read first, once each

- `CODINGSTYLE.md` in full. Quote one rule verbatim in your report. It must be a different rule from "Own members of classes should always be referenced using this->".
- `src/junction_gui.cpp`: `DrawJunctionGhost` only.
- `src/junction_game.cpp`: `ChooseJunctionBridgeType` only.
- `src/engine_base.h` lines 190–210 (`IterateType`) and 260–275. Also `src/engine_func.h` (`IsEngineBuildable`) and `src/engine_type.h` lines 60–100 (`RailVehicleInfo`: `railtypes`, `max_speed`, `railveh_type`).
- `src/rail.h` around line 383 (`IsCompatibleRail(RailTypes, RailType)`).

## 3. Tests

Neither change has a unit-test seam. `DrawJunctionGhost` is drawing, and `ChooseJunctionBridgeType` reads the engine pool, the bridge specs and the game date, none of which the test environment sets up. So there are **no new tests.** The existing suite must stay green, and the engineer checks both changes in game.

## 4. Changes (one commit)

1. **`DrawJunctionGhost`:** where it currently calls `SetViewportGhostPalette(ghost->bad ? PALETTE_TO_STRUCT_RED : PALETTE_TO_TRANSPARENT)`, change it to `SetViewportGhostPalette(ghost->bad ? PALETTE_TO_STRUCT_RED : PAL_NONE)`. `PAL_NONE` turns the override off, so valid ghosts draw with their normal palettes. Leave the non-flat red outline branch unchanged. Update the function's doc comment to say valid ghosts are drawn opaque in normal colours (D44).
2. **`ChooseJunctionBridgeType(railtype, length)`:** replace only the `rail_speed == 0` branch:
   - Let `target = 0`. For each `const Engine *e : Engine::IterateType(VehicleType::Train)`:
     - skip if `!IsEngineBuildable(e->index, VehicleType::Train, _local_company)`
     - let `const RailVehicleInfo &rvi = e->VehInfo<RailVehicleInfo>()`; skip if `rvi.railveh_type == RAILVEH_WAGON` (use the actual enum name in this tree)
     - skip if `!IsCompatibleRail(rvi.railtypes, railtype)`
     - otherwise `target = std::max<uint>(target, rvi.max_speed)`

     Both the engine and bridge speed units are "1 km-ish/h". Check this in the comments for `RailVehicleInfo::max_speed` and `BridgeSpec::speed`, and stop if they differ.
   - If `target == 0`, return the available type with the lowest `price`, breaking ties by the lowest index.
   - Otherwise, return the cheapest available type with `speed >= target`, breaking ties by the lowest index. If none qualifies, return the available type with the highest `speed`, then the lowest price, then the lowest index.
   - Leave the `rail_speed != 0` branch unchanged.
   - Update the doc comment of `ChooseJunctionBridgeType`, both in the `.cpp` and in `junction_game.h`, to describe both rules (D35, D45).
   - Add any includes needed (`engine_base.h`, `engine_func.h`, `company_func.h`, `rail.h`).

Commit message: `Change: Draw the junction ghost opaque and pick bridges by available train speed`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 5. Do not

- Change the red outline, the invalid palette, the estimate, or `PriceBridge`'s formula.
- Touch upstream files.
- Add tests, settings or UI.

## 6. Stop conditions

- A named API differs: `IterateType`, `IsEngineBuildable`, `VehInfo<RailVehicleInfo>`, the wagon enum, or `IsCompatibleRail(RailTypes, RailType)`.
- The speed units differ.
- Anything not covered here.

## 7. Gates

Logs go to `.../s8b_*.log`.

1. Build `openttd` and `openttd_test` with no errors and no warnings.
2. The full suite is `{BASELINE}`, all passing.
3. One commit touching only `src/junction_gui.cpp`, `src/junction_game.cpp` and `src/junction_game.h`.

## 8. Report format

- **References read,** with the CODINGSTYLE.md rule quoted
- **Commits**
- **Red evidence:** "none: no unit-test seam (named in brief)"
- **Types and members added or removed**
- **Suite results**
- **Manual checks for the engineer:**
  - the ghost is opaque and in full colour
  - invalid tiles are still red
  - the price is lower than before, and the bridge cost matches the price the game's own bridge window shows for that bridge type and length
- **Stops, and anything left undone**
- **State:** `git status --porcelain` and `git log -1 --format='%h %s'`

When you have reported, stop.
