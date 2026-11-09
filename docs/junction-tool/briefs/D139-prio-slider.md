# D139: prio length slider in the junction window

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD`, branch `feature/junction-tool`, head given in your launch prompt. You are the only writer here. Another agent works in `/home/pete/OpenTTD/.claude/tm2` (train model); don't touch it.
- Decisions: `docs/junction-tool/DECISIONS.md` D139 (the ruling), D137/D137a (sizing, `prio_strict`, `SizeJunctionPrio`), D140 (workflow).
- Builds: `build-rel` (Release) and `build` (Debug), both with ccache. Tests: `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/ptest.sh <binary> "<filter>" 24`. Iterate with `"[junction]~[slow]"` (about 13 s). Full suites only at the gate.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md` (GUI and naming parts).
- `src/junction_gui.cpp`: the window's widget list (`_nested_build_junction_widgets` or equivalent), `SpecKey`/`MakeKey`/`JunctionGenerateKey`, `EnsureLibrary`, `LibraryUsable`, `StartJob`, the library job's sizing result (`sizing_done` and the sized value), `OnClick`, `OnVehicleSelect`, the TL buttons, `GetWidgetString`, `SizingStatus`.
- `src/widgets/junction_widget.h`.
- `src/slider_func.h` and the volume slider in `src/settings_gui.cpp` (draw ~698, click and drag handling): copy that pattern.
- `src/junction_game.{h,cpp}` (`MakeJunctionSpec`).

## Changes
1. **Seam (unit-tested):** in `junction_game.{h,cpp}`:
   ```cpp
   /** The coverage and strictness for a slider choice (D139). @param choice 0 = the maximum. @param tl Train length. @param sized The strict coverage from SizeJunctionPrio (> 0). @return {prio_tiles, prio_strict}. */
   std::pair<uint8_t, bool> JunctionPrioChoice(uint8_t choice, uint8_t tl, uint8_t sized);
   ```
   Rules: the minimum is `min(tl, sized)`. `choice == 0` or `choice >= sized` gives `{sized, true}`. Otherwise `{max(choice, min(tl, sized)), false}`, except that a result equal to `sized` is `{sized, true}`.
2. **Widgets:** add `WID_BJ_PRIO_LABEL` (text) and `WID_BJ_PRIO_SLIDER` (an empty widget drawn as a slider, as the volume slider) after `WID_BJ_TRAIN` in the enum and in the window layout, each on its own row, same width as the train line. Tooltip on the slider: `STR_JUNCTION_PRIO_SLIDER_TOOLTIP` "Shorter than the maximum accepts occasional main-line holds behind joining trains".
3. **Window state:** `uint8_t prio_choice = 0; ///< Slider choice; 0 = the strict maximum (D139).` Reset it to 0 when a train is picked and when TL changes.
4. **Label:** `STR_JUNCTION_PRIO_LENGTH` "Prio length: {NUM} of {NUM} tiles" with the effective coverage and the sized maximum. Before sizing is done, or when sizing gave 0, the label shows the existing sizing/too-long status string instead and the slider is drawn disabled (no value).
5. **Slider:** range `min(tl, sized)` to `sized`, value = the effective coverage; click and drag set `prio_choice` (dragging to the maximum stores 0). A change calls `Regenerate()` (hover job only).
6. **Generation:** `StartJob` applies `JunctionPrioChoice(prio_choice, tl, sized)` to `job->spec.prio_tiles` and `job->spec.prio_strict` before the frame and terrain snapshot. Add the effective coverage to `JunctionGenerateKey` so a change restarts the job. `LibraryUsable()` returns false while the effective coverage is below the maximum (the library is built strict at the maximum; orchestrator call, recorded under D139).
7. `WID_BJ_TRAIN` keeps showing the shown placement's `prio_tiles` (D137).

## Tests (`[junction][game]`, red first)
1. "JunctionPrioChoice follows the slider rules": `(0,5,14)→{14,true}`, `(20,5,14)→{14,true}`, `(14,5,14)→{14,true}`, `(7,5,14)→{7,false}`, `(3,5,14)→{5,false}`, `(0,5,3)→{3,true}`, `(2,5,3)→{3,true}`. Red: stub returns `{sized, true}` (record which cases fail).

## Manual check
Not required from you; the orchestrator checks the window in the game. Do make sure `build/openttd` builds.

## Do not
- Change the generator, sizing, evaluator, train model or library building.
- Add settings, config entries or savegame state; `prio_choice` is window state only.

## Stop conditions
- The volume slider pattern can't be reused for a window that isn't the settings window.
- The window has no room for two rows without reworking its layout.

## Gates
1. Build `build-rel` `openttd_test`, Debug `build` `openttd_test` and `openttd`; no errors, no new warnings.
2. `ptest.sh build-rel/openttd_test "~[.]" 24` (full Release) and `ptest.sh build/openttd_test "[junction]~[slow]" 24` (Debug fast tier).
3. Commit: `Add: Junction prio length slider`, body ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. Commit only your files.

## Report
- References read
- Commit: hash and subject
- Red evidence
- Types, functions, members, widgets, strings added: names only
- Suite results with wall times
- Stops, and anything left undone
- State: `git status --porcelain` and `git log -1`

When you have reported, stop.
