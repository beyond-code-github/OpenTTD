# UI1: design speed from the picked train; sizing errors; junction window fixes

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD`, branch `feature/junction-tool`, head given in your launch prompt. You are the only writer.
- Decisions: `docs/junction-tool/DECISIONS.md` D35, D45, D47 (bridge and reference speed), D137/D137a, D139, D140 (workflow), D141 (this step's ruling, below).
- Builds `build-rel` and `build`, both ccache. Tests: `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/ptest.sh <binary> "<filter>" 24`; iterate with `"[junction]~[slow]"`; full `"~[.]"` only at the gate.

## Why (measured, settled)
In the engineer's NUTS game (2057) the rail type has no speed limit, so `GetJunctionReferenceSpeed` (`src/junction_game.cpp:94`) returns the fastest buildable engine's speed (D45), over 300 km/h, while the picked train tops out at 196. At reference speed 300 the generator finds no layout at any coverage (probe: `GenerateJunction` err NoLayout for prio 1..24 at TL4 and TL5; at 160 and 196 it finds layouts and `SizeJunctionPrio` gives 14 at TL4, 15 at TL5). `SizeJunctionPrio` then returns 0 and the GUI shows "Prio coverage would exceed 32 tiles", which is false.

**D141 (orchestrator, follows D137 and D47):** with a picked train, the junction's reference speed is `min(rail type speed limit, train max_speed)`; for an unlimited rail type it is the train's max_speed. Bridges must be at least as fast as that speed (D47), not the fastest buildable engine. With no train (station tool, `JunctionTrainParams{}`), behaviour is unchanged.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md` (GUI, strings, naming).
- `src/junction_game.{h,cpp}`: `GetJunctionReferenceSpeed`, `ChooseJunctionBridgeType` and its callers, `MakeJunctionSpec`.
- `src/junction_generator.{h,cpp}`: `SizeJunctionPrio`, `JunctionGenerateError`.
- `src/console_cmds.cpp`: `junction_export`'s sizing call.
- `src/junction_gui.cpp`: `SizedPrio`, `EffectivePrio`, `SizingStatus`, `GetWidgetString` (`WID_BJ_TRAIN`, `WID_BJ_PRIO_LABEL`, `WID_BJ_STATUS`), `DrawWidget`, `UpdateWidgetSize`, the widget layout, and every `SetWidgetDirty(WID_BJ_STATUS)`.
- `src/tests/junction_game.cpp`.

## Changes
1. **Reference speed (D141):** `MakeJunctionSpec` sets `spec.reference_speed = train.max_speed > 0 ? std::min(GetJunctionReferenceSpeed(railtype), train.max_speed) : GetJunctionReferenceSpeed(railtype)`, where for an unlimited rail type the first term is replaced by the train's speed (i.e. when `GetRailTypeInfo(railtype)->max_speed == 0` and a train is given, use `train.max_speed`). Bridges: give `ChooseJunctionBridgeType` a `uint16_t target_speed` parameter used in place of the fastest-engine speed for unlimited rail types (and in place of the rail limit when lower); `MakeJunctionSpec` passes `spec.reference_speed`. Update doc comments, citing D141. Other callers of `ChooseJunctionBridgeType` pass what they used before (keep their behaviour).
2. **Sizing result:** `SizeJunctionPrio` returns `struct JunctionPrioSizing { uint8_t prio_tiles = 0; JunctionGenerateError error = JunctionGenerateError::None; };` (declare in `junction_generator.h`, one-line docs per member): generation failure gives `{0, the generation's error}`, the cap gives `{0, PrioTooLong}`, success `{p, None}`. Update every caller (GUI library job, `junction_export`, tests). The GUI and `junction_export` show the generation error's existing string/name when sizing fails for that reason.
3. **Window:**
   - a. When sizing is not done or failed (`SizedPrio() == 0`), the prio label is empty and the slider is not drawn (the row stays, blank). The status line alone carries the sizing or error message.
   - b. Every place that dirties `WID_BJ_STATUS` because sizing or the library job changed also dirties `WID_BJ_PRIO_LABEL` and `WID_BJ_PRIO_SLIDER`. If `WID_BJ_PRIO_LABEL` is a widget type that draws no background (so old text stays under new), make it the same widget type and colour as `WID_BJ_TRAIN`.
   - c. `WID_BJ_TRAIN`: the prio shown is the shown placement's `prio_tiles` when a placement is shown, else `SizedPrio()` when non-zero; otherwise use a new string `STR_JUNCTION_TRAIN_NO_PRIO` "{RAW_STRING}: {NUM} tile{P "" s}" (name and TL; match `STR_JUNCTION_TRAIN`'s parameter style). Remove the `JunctionPrioLength` call and the `STR_JUNCTION_TRAIN_LONG_PRIO` suffix from the GUI (leave the string in english.txt only if other code uses it; otherwise remove it too).
   - d. `UpdateWidgetSize` for `WID_BJ_TRAIN`: width at least that of `STR_JUNCTION_TRAIN` with a 20-character name and 2-digit numbers (use `GetParamMaxDigits` as the cost row does), so the line is not cut.

## Tests (red first; record each red)
1. `[junction][game]` "MakeJunctionSpec caps the reference speed at the picked train": with a train whose `max_speed` is 100, `spec.reference_speed == 100` (whatever the rail type's limit in the test environment is, as long as it is above 100; if the test environment's rail type limit is below 100, use a lower train speed and say so). With `JunctionTrainParams{}`, `reference_speed == GetJunctionReferenceSpeed(RAILTYPE_RAIL)`. Red: today it ignores the train.
2. `[junction][generator]` "SizeJunctionPrio reports the generation error": `MakeTestJunctionSpec()` with `reference_speed = 300`, `prio_tiles = 0` gives `{0, NoLayout}` (the probe's error). Red: the struct doesn't exist yet: write it first with `SizeJunctionPrio` returning `{0, PrioTooLong}` for every failure, see the error mismatch, then fix.
3. All existing tests pass.

## Manual check
The orchestrator checks the window. Make sure `build/openttd` builds.

## Do not
- Change the timing model, the slider rules (D139), the generator's geometry, or station behaviour.
- Add settings or savegame state.

## Stop conditions
- `ChooseJunctionBridgeType`'s other callers would change behaviour.
- The test environment has no rail type info to test against.

## Gates
1. Build `build-rel` `openttd_test`, Debug `build` `openttd_test` and `openttd`; no errors, no new warnings.
2. `ptest.sh build-rel/openttd_test "~[.]" 24` and `ptest.sh build/openttd_test "[junction]~[slow]" 24` (the Debug "map_export JSON of a hand-built area" abort is known and pre-existing: report, don't fix).
3. Commit: `Fix: Junction design speed comes from the picked train; window prio display`, body ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. Commit only your files.

## Report
- References read
- Commit: hash and subject
- Red evidence per test
- Types, functions, members, strings added or removed: names only
- Suite results and wall times
- Stops, and anything left undone
- State: `git status --porcelain` and `git log -1`

When you have reported, stop.
