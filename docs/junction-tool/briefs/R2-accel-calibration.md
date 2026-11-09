# R2: does StepJunctionTrain accelerate like the real design train?

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Work **only** in `/home/pete/OpenTTD/.claude/play` (detached HEAD at 128ed37504, R1: the rig builds the design train). Another agent holds `/home/pete/OpenTTD`; don't touch it. Build only in `build-play` (Release, ccache configured or not, either is fine).
- Rig: `tools/ottd/README.md`. Use your own run dir and ports, as R1 did: `OTTD_RIG_DIR=$S/ottd-rig-r2/ OTTD_RIG_PORT=3997 OTTD_RIG_GAME_PORT=3999 OTTD_RIG_BIN=/home/pete/OpenTTD/.claude/play/build-play/openttd` (check the ports are free). Never stop or touch the running rig server on 3977/3979.
- `$S` = `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad`.
- Why: the junction tool's prio sizing (D137) gives 14 tiles where the engineer expects about 7. One suspect is the train model (`src/junction_train_model.{h,cpp}`, `StepJunctionTrain`) accelerating too slowly. This step measures the real train and compares.

## Read first
- `tools/ottd/README.md`, `tools/ottd/gs/main.nut` (`CmdSample`, the chunked reply), `tools/ottd/ottd` (`sample`).
- `src/junction_train_model.{h,cpp}` (whole), and `src/ground_vehicle.cpp` / `src/ground_vehicle.hpp` acceleration (`GetAcceleration`, `UpdateInclination` not needed) to confirm which acceleration model the game uses.

## Changes (commit these)
1. GS command `speed_trace {vehicle, ticks}`: replies `{speeds:[...]}`, the vehicle's `GSVehicle.GetCurrentSpeed` (km/h) at every game tick for `ticks` ticks, starting the tick the command arrives. Same sleep pattern as `CmdSample`.
2. CLI `speed-trace VEHICLE --ticks N`: prints the list, one value per line as `tick speed`. Add both to the README tables.

## Measurement (not committed)
1. `start --nuts`, `company`, a straight 120-tile line on flat ground with a depot at each end (map size: use what `start` gives; if 120 tiles don't fit, the longest straight that does, and say so), `train --design` in the west depot, orders to the east depot.
2. `go`, then immediately `speed-trace <v> --ticks 1500`. Save to `$S/r2_game_trace.txt`. Note the game's `train_acceleration_model` setting (`rcon "setting train_acceleration_model"`).
3. The consist's empty mass: sum `GSEngine.GetWeight` over its parts (report how you got it), and the power and TE the GS reports.
4. Model: a temporary Catch test in `src/tests/` (not committed, deleted afterwards) that steps `StepJunctionTrain` from rest for 1500 ticks with `power_hp` 14194, `max_te` 1090000, `mass_t` = the empty mass from step 3, `air_drag` 25, `max_speed` 196, and writes `tick speed_kmh` per tick to `$S/r2_model_trace.txt`, converting the model's speed unit to km/h the same way the game's `GetDisplaySpeed` does for trains (read it, cite the line).
5. Compare: ticks to reach 40, 80, 120, 160 and 190 km/h in the game and the model, and the ratio. Also the distance in tiles after 100, 200, 400 ticks if the model gives it.

## Do not
- Change the train model or any junction code. Commit nothing from steps 1-5 of the measurement.
- Touch `/home/pete/OpenTTD`, the running rig, or Pete's config/data (read only).

## Stop conditions
- The game trace does not start from 0 km/h (the train left the depot already moving): report the first speeds.
- The model's speed unit or conversion cannot be determined from the code.

## Gates
1. `python3 -m py_compile` on changed Python; the GS loads (the trace works).
2. Commit: `Add: Junction rig speed trace`, body ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. Only the GS, CLI and README.

## Report
- References read
- Commit hash in the play worktree
- Acceleration model setting; empty mass and how obtained; GS power/TE
- The comparison table (ticks to each speed, game vs model, ratio), and tile distances if available
- The model's speed unit and the conversion line cited
- Stops, and anything left undone
- State: `git -C /home/pete/OpenTTD/.claude/play status --porcelain` and `log -1`

When you have reported, stop.
