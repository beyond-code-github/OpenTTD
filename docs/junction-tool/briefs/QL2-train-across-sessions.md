# QL2: the design train and prio remembered across sessions; "spent vs quoted" after a build (engineer, 2026-11-13)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree and HEAD: given at launch (`<WT>`, `<branch>`, `<HEAD>`): `git -C <WT> checkout -B <branch> <HEAD>`. Never use git stash. Never touch other worktrees, any game client or the rig servers. Use absolute paths.
- QL1 (8a10aa1006) remembers the picked train as a VehicleID and the prio slider in file statics: within one game session only. Engineer: "It would be good if the picked train was remembered across sessions ideally". A VehicleID means nothing in another save, so what persists is the design train's NUMBERS: JunctionPickedTrain (src/junction_game.h: `loaded` JunctionTrainParams {power_hp, max_te, mass_t, air_drag, max_speed}, `tl`, `name`), which is all the spec needs (MakeJunctionSpec reads JunctionTrainParams and tl), plus the engine IDs of its parts for the image.
- Client settings already hold `gui.junction_tl` (src/table/settings/gui_settings.ini ~:584): the pattern for new private client settings saved in openttd.cfg.
- Engineer also OK'd: after a build finishes, show "spent £X, quoted £Y" (the build sequencer knows both; find where the build completes and where the quote (estimate total incl. height cost) is held).

## Read first (once each)
CODINGSTYLE.md (both parts); src/table/settings/gui_settings.ini (junction_tl and its neighbours), src/settings_type.h GUISettings junction fields; src/junction_game.{h,cpp} ReadJunctionTrain; src/junction_gui.cpp the remembered-train statics (QL1), the window constructor, the train label/image drawing, the build sequencer's completion path; src/lang/english.txt junction strings.

## Changes
1. Client settings (gui_settings.ini, GUISettings): `junction_train_power`, `junction_train_te`, `junction_train_mass`, `junction_train_air_drag`, `junction_train_speed`, `junction_train_tl`, `junction_train_name` (string), `junction_train_engines` (string: the parts' engine IDs as a comma list), `junction_prio` (uint8; 0 = default). Flags as junction_tl's (not network synced, not in saves).
2. On pick: write them all. On prio change: write junction_prio. On window open: if the session's remembered vehicle is valid (QL1 rule), use it as today; else if the settings hold a train (mass > 0), use the stored numbers as the picked train (name shown as stored, marked e.g. "(saved)" only if an existing string allows: no new string otherwise), image drawn from the stored engine IDs where those engines exist in this game (draw each engine's purchase-list sprite left to right, as the engine preview draws; skip unknown IDs; empty if none), and the prio from junction_prio.
3. After a build completes (all steps done), the cursor tooltip / status shows the spent amount and the quote, using one new string `STR_JUNCTION_STATUS_SPENT :{BLACK}Spent {CURRENCY_LONG}, quoted {CURRENCY_LONG}`, until the next hover changes it; also `Debug(misc, 1, ...)` the two numbers.
4. TESTS.md row if a test is added.

## Tests
- A `[game]` unit test only if the settings round-trip (numbers -> JunctionPickedTrain) is a free function; otherwise none and say so.
- Manual checklist for the engineer.

## Gates
- The `openttd` target builds Debug and Release with no new warnings; `tools/junction/jt gate build-rel` (count, time).
- One commit: `Feature: Junction design train and prio persist in the client config; spent vs quoted after a build`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus the manual checklist. When you have reported, stop.
