# Brief ST step 5: the Station window (ghost, cost, build)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- **You run in your own git worktree,** based on `feature/junction-tool` at `{HEAD}`.
  - Build Release `openttd_test` and Release `openttd` in a build dir inside the worktree (Ninja, clang-18).
  - Commit on your branch.
  - Don't run any `tools/ottd` command.
- **Read:**
  - `docs/junction-tool/PLAN-stations.md` (ST5), D106 and D101;
  - `src/junction_station_generator.h` (`GenerateStation`);
  - `src/junction_station_rules.h`;
  - `src/junction_gui.cpp`, all of it: the Junction window is the model to copy (notation editbox, TL stepper, Pick train, rotate/mirror, ghost, cost, tooltip, build sequencer, `RestoreJunctionPlacement` pattern);
  - `src/junction_game.h/.cpp` (`EstimateJunction`, `MakeJunctionBuildSteps`, `JunctionBuildStep` variants, `JunctionSequencer`);
  - `src/rail_gui.cpp` (how `WID_RAT_BUILD_JUNCTION` opens the Junction window).
- **Scratchpad:** `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/st5/`

## 2. Changes

1. **Build steps:**
   - `JunctionBuildStep` gains a station variant, `JunctionStationStep { TileIndex tile; Axis axis; uint8_t platforms; uint8_t length; }`.
   - `MakeJunctionBuildSteps` emits one per `layout.stations` block, first, before rail. It skips rail steps whose tile is inside a station block, because station tiles carry their own track.
   - The poster posts `Commands::BuildRailStation` with the same parameters the station GUI uses for a new station of that size. Find them in `station_gui.cpp` / `rail_gui.cpp` `PlaceRail_Station`. Use the default station class and type, and a new station (no join).
   - `EstimateJunction` prices the station block the same way (a test-mode command), and marks bad tiles.
2. **The ghost:** `JunctionGhost::Set` takes station blocks into account. Draw each station tile's platform track with the ghost's existing rail drawing (as plain track of the block's axis) and outline it like other tiles. A distinct station sprite isn't needed in v1; say so in a comment.
3. **The window:**
   - **Toolbar:** a new rail-toolbar button, `WID_RAT_BUILD_JUNCTION_STATION`, with tooltip "Generate a station". Reuse an existing station sprite for its icon, and add it next to the Junction button.
   - **Opening:** it opens `ShowBuildJunctionStationWindow`, a new window class `BuildJunctionStationWindow` in a new file `src/junction_station_gui.cpp`, with widget enum `WID_BJS_*` in `src/widgets/junction_station_widget.h`. Copy the Junction window's structure.
   - **Controls:**
     - a platforms stepper (2..8, a client setting `gui.junction_station_platforms`, default 3, NotInSave and NoNetworkSync like `junction_tl`);
     - the TL stepper (shared setting `gui.junction_tl`);
     - Pick train (reuse `ReadJunctionTrain`; the TL comes from the train);
     - rotate and mirror (reuse `_junction_rotation` / `_junction_mirrored`, or the window's own copies, matching the Junction window);
     - a cost line and a status line.
   - **No line notation in v1:** a single incoming direction.
   - **Generation:** `GenerateStation(MakeJunctionSpec(railtype, 1, 1, tl), platforms)`, synchronously, on spec change. It's fast, so no worker thread is needed.
   - **Hover:** orient the layout with `ApplyJunctionOrientation`. The hovered tile is the station layout's in-port tile, so the incoming stub starts under the cursor. Compute the origin from the oriented in-port position, then show the ghost, the cost and the tooltip as the Junction window does.
   - **Click** builds through the same sequencer. The pick-train flow needs the same toolbar-abort guard as the Junction window: reuse `IsJunctionTrainPickActive` and generalise it if needed, or add an equivalent for this window. Say which.
   - **Strings:** add them to `english.txt`.
4. Register the new files in `src/CMakeLists.txt`.

## 3. Tests

- **Unit:** `MakeJunctionBuildSteps` on a generated station layout emits exactly one station step first and no rail steps on station tiles. Red first: before the change it emits rail steps there.
- The full Release suite, all passing.
- The GUI isn't unit-testable. List what the engineer should check:
  1. open the tool;
  2. the ghost follows the cursor;
  3. change platforms;
  4. rotate;
  5. click to build;
  6. pick a train.

## 4. Do not

- touch the junction generator, the junction checker or junction behaviour (except the shared build-step code above)
- add terminus, pickup or multi-line support

## 5. Stop conditions

- `CmdBuildRailStation`'s parameters can't be determined.
- Sharing the build step breaks a junction test.
- Anything not covered.

## 6. Gates

1. Release build of `openttd` and `openttd_test` in the worktree, then the full suite, all passing.
2. Commit: `Feature: Station tool generates and builds ro-ro drop stations`. Signing is off; end the message with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 7. Report

- the branch and commit
- red evidence
- the files added and changed
- suite totals
- the manual check list
- stops
- `git status --porcelain` and `git log -1`, both in the worktree

When you have reported, stop.
