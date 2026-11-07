# Brief 1b step E: the Junction window uses the generator

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- **Repo:** `/home/pete/OpenTTD`, branch `feature/junction-tool`, head `{HEAD}`.
- **Plan:** PLAN-1b §7, as amended here.
- **Decisions to read** in `DECISIONS.md`: D45, D48 (house UI style), D64, D65, D66, D67, D68, D69 (no savegame changes, hard rule), D92.
- **Today:** the tool always places the fixed `MakeTestJunctionLayout()`, through `GetPlacementLayout()` in `src/junction_gui.cpp`. Nothing in production builds a `JunctionSpec`. `GenerateJunction(spec)` (`src/junction_generator.h`) is done; it takes about 0.5 s in Release.

## 2. Read first (once, with offset/limit)

- `CODINGSTYLE.md`: naming, braces, doc comments
- **`src/junction_gui.cpp`:**
  - the window class
  - `GetPlacementLayout` (l.238)
  - `ComputePlacement` (l.359)
  - `OnClick`, `OnRealtimeTick`
  - the nested widgets (l.508–528)
- `src/widgets/junction_widget.h`
- **`src/junction_game.h/.cpp`:** `ChooseJunctionBridgeType` (l.43–88), `GameJunctionPricing::PriceBridge` (l.121–134)
- **`src/junction_generator.h`:** `JunctionSpec`, `JunctionUnitPrices`, `ParseJunctionNotation`, `JunctionGenerateError`
- **`src/table/settings/gui_settings.ini`:** the `drag_signals_density` entry (l.544–555) and the templates (l.29–37); also `network_private_settings.ini` l.19 and l.45–52, for the `SDTC_SSTR` template and an entry
- **`src/settings_type.h`:** the GUI settings struct near `drag_signals_density` (l.270)
- **`src/lang/english.txt`:** the `STR_JUNCTION_*` block (l.2863–2882)
- **one existing window with an editbox** (`QueryString` plus `WWT_EDITBOX`) to copy the pattern, e.g. a small one in `src/` that `grep -l "WWT_EDITBOX"` finds; read only its editbox parts

## 3. Changes

### 3.1 Settings (client-only, never in the savegame: D69)
Add these to `gui_settings.ini` and the GUI settings struct in `settings_type.h`, all with `SettingFlag::NotInSave, SettingFlag::NoNetworkSync`:

| Setting | Type | Default | Shown? |
|---|---|---|---|
| `gui.junction_signal_type` | `SDTC_OMANY`-style or `SDTC_VAR` U8 over {0 = one-way block, 1 = one-way path} (`SignalType::Block`, `SignalType::PathOneWay`) | 0 (D68) | yes, in the Settings window next to the existing signal settings |
| `gui.junction_ml_notation` | string (add the `SDTC_SSTR` template line to `gui_settings.ini`'s `[templates]`, copied from `network_private_settings.ini`; length 0 as `connect_to_ip` uses) | `"L_R"` | no |
| `gui.junction_branch_notation` | string | `"L_R"` | no |
| `gui.junction_tl` | U8, min 2, max 12 | 4 | no |

- **`junction_signal_type`:** add its strings (name and help) in the house style, and its place in `settings_gui.cpp`'s page list next to the signal settings. Copy how `drag_signals_density` or `default_signal_type` is listed.
- If a needed template, flag or page mechanism doesn't exist as described, stop.

### 3.2 `junction_game.h/.cpp`

```cpp
/** The speed the junction is designed for: the rail type's limit, or the fastest buildable engine when unlimited (D45, D67). */
uint16_t GetJunctionReferenceSpeed(RailType railtype);
/** Fill a generator spec from the game: settings, rail type, prices and bridges. */
JunctionSpec MakeJunctionSpec(RailType railtype, uint8_t ml_gap, uint8_t branch_gap, uint8_t tl);
```

**`GetJunctionReferenceSpeed`:**
- Factor out the target-speed logic from `ChooseJunctionBridgeType`: the rail type's `max_speed` if non-zero, else the max engine speed over buildable, compatible, non-wagon engines.
- If the result is 0, return 160.
- `ChooseJunctionBridgeType` must call it, and its behaviour must not change.

**`MakeJunctionSpec` fills:**

| Field | Value |
|---|---|
| `ml_gap`, `branch_gap`, `tl` | the arguments |
| `sd` | `_settings_client.gui.drag_signals_density` (D65) |
| `reference_speed` | `GetJunctionReferenceSpeed(railtype)` |
| `curve_speed` | `GetRailTypeInfo(railtype)->curve_speed` |
| `curve_limit` | true unless `_settings_game.vehicle.train_acceleration_model` is the original model (use its enum) |
| `signal_type` | from `gui.junction_signal_type` |
| `bridge_span_ok` | `[railtype, speed](uint n) { auto t = ChooseJunctionBridgeType(railtype, n); return t.has_value() && (GetBridgeSpec(*t)->speed >= speed); }`, where n is the middle-tile count (the same length meaning `ComputePlacement` uses) |
| `prices.rail_piece` | `RailBuildCost(railtype)` |
| `prices.signal` | `_price[Price::BuildSignals]` |
| `prices.bridge(n)` | the body-cost formula from `PriceBridge` for length n+2, using `ChooseJunctionBridgeType(railtype, n)`'s spec, plus `(n+2) * RailBuildCost(railtype)`; if there is no type, a very large Money |

If a value's unit doesn't match the field's doc comment (e.g. bridge `speed` against train speed units), stop and report.

### 3.3 Window (`junction_gui.cpp`, `widgets/junction_widget.h`, `english.txt`)

**Widgets**, added in house style (D48) above the cost line:
- two editboxes, labelled "Mainline" and "Branch", with tooltips explaining the notation (`L_R` = one track each way with one tile between; more underscores widen the gap). Each holds up to 16 chars and starts from its setting.
- a TL control: a `−`/`+` pair either side of a number label ("Train length: {N} tiles"), the house pattern for small integer steppers.

**Generation:**
- A window member `std::optional<JunctionLayout> generated` and `StringID generate_error`.
- `Regenerate()`:
  1. Parse both notations with `ParseJunctionNotation`. Invalid gives `STR_JUNCTION_ERROR_NOTATION`; multi-lane gives `STR_JUNCTION_ERROR_MULTILANE`.
  2. Otherwise run `GenerateJunction(MakeJunctionSpec(railtype, ml.gap, branch.gap, tl))`. `NoLayout` gives `STR_JUNCTION_ERROR_NO_LAYOUT`; `NeedsMlGap` gives `STR_JUNCTION_ERROR_NEEDS_ML_GAP`.
  3. On success, if `_settings_game.vehicle.road_side` is left-hand driving, apply `TransformJunctionLayout(layout, JunctionTransform::Mirror)` (D66).
  4. Store the layout, save the inputs to the settings, set `last_tile = INVALID_TILE` and invalidate the window.
- Call `Regenerate()` in the constructor, on each editbox change (`OnEditboxChanged`), on the TL buttons, and when the rail type changes (if the window can change it).
- **`GetPlacementLayout()`** becomes a window method (or takes the window's layout) that returns `ApplyJunctionOrientation(*generated, rotation, mirrored)`. Its other callers (`Close`, Cancel, the Done branch) use the same method.
- **When `generated` is empty:**
  - hover shows no ghost;
  - the status line shows `generate_error`;
  - a map click does nothing;
  - `SetTileSelectSize(1,1)`.
- `WID_BJ_NAME` now shows the notations ("{ml} × {branch}, train {tl}").

**Strings** (all new, `STR_JUNCTION_*`, plain English, house style):
- the labels and tooltips
- the TL label
- the four errors
- the settings name and help

Keep `MakeTestJunctionLayout()`; tests use it.

## 4. Tests

- **`src/tests/junction_game.cpp`:** `MakeJunctionSpec` reads `sd` from `drag_signals_density`, `signal_type` from the new setting, and `curve_speed` from the rail type. Set the settings in the test, and use a rail type the existing tests can access. If engines or rail types can't be set up in tests, test only the settings-derived fields and say so.
- **`GetJunctionReferenceSpeed`:** for a rail type with `max_speed` set, it returns that value. Only if feasible, as above.
- **Window:** no automated test (GUI). Manual checks below.

## 5. Manual checks (the engineer runs these in step F; you can't drive the GUI headless, so list them as "not run" unless you find a way)

Run the game with `build/openttd`, start a new game, and open rail toolbar → Junction.

1. The window shows both notation boxes (`L_R`), TL 4 and a size/cost. Hover shows the generated doubled-bridge T ghost, about 25×15.
2. Changing TL to 6 regenerates, and the ghost changes. Typing `LR` in Mainline shows the "needs a gap" error, with no ghost.
3. Typing `LL_RR` shows the multi-lane error. Typing `x` shows the notation error.
4. Rotate and mirror still work. Click-build builds the junction, including Entry/Combo/Exit prio signals.
5. With road side set to left in a new game, the built layout is the mirror image.
6. Close and reopen the window: the notations and TL persist. Save and load: there is no savegame difference (D69).

Copy this list into `docs/junction-tool/CHECKS-1b.md` (a new file) as the engineer's checklist.

## 6. Do not

- change the generator, router, signal pass or checker
- add threads (D92)
- add game settings that are saved
- remove `MakeTestJunctionLayout`
- change build sequencing

## 7. Stop conditions

- A setting mechanism differs from §2's description.
- Units don't match.
- The editbox or stepper pattern doesn't exist as described.
- Anything not covered.

## 8. Gates

1. `cmake --build build --target openttd openttd_test > /tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/e_build.log 2>&1`. There must be no errors or warnings in the files you touched.
2. Full suite. The baseline is 212 cases, all passing.

Commit message: `Feature: Junction window generates the junction from notations and train length`. Signing is off; end the message with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 9. Report

- references read
- commit
- red evidence
- settings, functions, widgets and strings added
- suite totals
- manual check results (or why one couldn't be done)
- stops
- `git status --porcelain` and `git log -1`

When you have reported, stop.
