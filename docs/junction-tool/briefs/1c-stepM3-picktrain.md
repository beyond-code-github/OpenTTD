# Brief 1c step M3: pick an actual train in the Junction window (D101)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- **Repo:** `/home/pete/OpenTTD`, branch `feature/junction-tool`, head `{HEAD}`. The Release suite passes.
- **Decisions:** D101 (pick an actual train, loaded mass, design speed = min(train, rail type)), D102 (prio length) and D34 (warn above 2 TL).
- **Exists:** `src/junction_train_model.h/.cpp`, with `JunctionTrainParams`, `JunctionPrioLength` and `JunctionTerminalSpeed`.
- **This step reads a picked train into the window and shows what it implies.** It doesn't change generation yet (that's M4).
- **Scratchpad:** `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/m3/`
- **Don't run any `tools/ottd` command:** a rig run is live.

## 2. Read first

- `src/junction_gui.cpp`: the window class, its widgets, `OnClick` and the TL stepper. Also how it uses `SetObjectToPlace` for the junction placement (HT_RECT or similar), and `OnPlaceObjectAbort` if present.
- `src/widgets/junction_widget.h` and `_nested_build_junction_widgets` in `junction_gui.cpp`.
- `src/depot_gui.cpp` l.820–835 (`SetObjectToPlaceWnd(..., HT_VEHICLE, this)`) and l.936–955 (`OnVehicleSelect`): the clone-tool pattern to copy.
- `src/train.h`: `Train`, `GetMaxWeight`, `GetDisplayMaxSpeed`, `IsFrontEngine`, `First`.
- `src/ground_vehicle.hpp`: `GroundVehicleCache` (`cached_power`, `cached_max_te`, `cached_air_drag`, `cached_total_length`).
- `src/train_cmd.cpp` l.4330 (`Train::GetMaxWeight`).
- `src/rail.h`: `GetRailTypeInfo(...)->max_speed` (0 means unlimited).
- `src/junction_game.h/.cpp`: `MakeJunctionSpec`, and where the window keeps `tl`.

## 3. Changes

### 3.1 Game side (`junction_game.h/.cpp`)

```cpp
/** What the junction tool takes from a picked train (D101). */
struct JunctionPickedTrain {
	JunctionTrainParams loaded; ///< Loaded mass (sum of GetMaxWeight over parts), effective power, TE and air drag from the caches; max_speed = min(train max, rail type max).
	uint8_t tl = 0; ///< Train length in whole tiles, rounded up: ceil(cached_total_length / TILE_SIZE).
	std::string name; ///< The vehicle's name, as GetString(STR_VEHICLE_NAME, id) gives it.
};

/** Read a train for the junction tool; std::nullopt if v is not a train of the local company. */
std::optional<JunctionPickedTrain> ReadJunctionTrain(const Vehicle *v);
```

- Use `v->First()`.
- **`loaded.mass_t`** is the sum over all parts of `GetMaxWeight()`, at least 1.
- **`power_hp`, `max_te` and `air_drag`** come from the first vehicle's `gcache`.
- **`max_speed`** is `min(GetDisplayMaxSpeed(), rail type max)`, using the train's own `railtype`, where max 0 means unlimited. Use whatever unit `cur_speed` uses for trains; check that `vcache.cached_max_speed` is in those units, and say so.
- **`tl`** is the ceiling of `cached_total_length / TILE_SIZE`, clamped to 2..12 (the window's TL range).

### 3.2 GUI (`junction_gui.cpp`, `junction_widget.h`, `english.txt`)

1. **A new button `WID_BJ_PICK_TRAIN`** ("Pick train", tooltip "Click one of your trains to size the junction for it"), on a new row below the TL row.
2. **Click:** `SetObjectToPlaceWnd(SPR_CURSOR_CLONE_TRAIN, PAL_NONE, HT_VEHICLE, this)`.
   - In `OnVehicleSelect`: `ReadJunctionTrain`. If it gives a train:
     - store it in a window member `std::optional<JunctionPickedTrain> picked_train`;
     - set `this->tl` to its `tl` and call `Regenerate()`, as the TL stepper does;
     - restore the junction placement cursor (whatever the window uses on open; find how it starts placement, and call the same thing).
   - Return true.
   - If it isn't a train of the local company, show `STR_JUNCTION_ERROR_NOT_OUR_TRAIN` "Pick one of your own trains" in the status line, and keep the vehicle cursor.
3. **A new info line `WID_BJ_TRAIN`**, below the button. When a train is picked:
   - `STR_JUNCTION_TRAIN` "{STRING}: {NUM} tiles, prio {NUM} tiles", with the name, `tl`, and `JunctionPrioLength(loaded, loaded.max_speed, tl, sd)`, where sd is the drag density setting;
   - when the prio is over 2·tl, append `STR_JUNCTION_TRAIN_LONG_PRIO` " (long)" (D34).
   With no train picked: `STR_JUNCTION_TRAIN_NONE` "No train picked".
4. **The TL stepper** still works and clears `picked_train` when used (the player overrode the length).

### 3.3 Console (debug, `console_cmds.cpp`, inside `#ifdef _DEBUG`, next to `junction_export`)
`junction_train <vehicle id>` prints one line: `junction_train name=... tl=N mass=M power=P te=T drag=D max=S prio=L`. That's `ReadJunctionTrain`, plus `JunctionPrioLength` with sd 2. On a bad id, print an error.

## 4. Tests

- Unit-testing needs game state, so it isn't possible here.
- `JunctionPrioLength` is already tested.
- Check with the rig in the report step, done by the orchestrator, not you.
- Compile both builds. Make sure the Release suite still passes.

## 5. Do not

- change generation, `JunctionSpec` or the prio coverage (that's M4)
- persist the picked train in settings (later)
- run the rig

## 6. Stop conditions

- `HT_VEHICLE` selection can't coexist with the window's placement mode.
- Anything not covered.

## 7. Gates

1. `cmake --build build-rel --target openttd openttd_test > m3/rel.log 2>&1`, then the Release suite, all passing.
2. `cmake --build build --target openttd openttd_test > m3/dbg.log 2>&1`: no errors or warnings in the files you touched.

Commit message: `Feature: Junction window can size the junction for a picked train`. Signing is off; end the message with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 8. Report

- commit
- functions, members and strings added
- the speed units finding
- suite totals
- what the engineer should check in game
- stops
- `git status --porcelain` and `git log -1`

When you have reported, stop.
