# TM2: the train model advances twice per game tick, as the game does

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- You run in your own git worktree (the Agent tool created it from `feature/junction-tool` at d9f96f8c05). Work only there. Another agent holds `/home/pete/OpenTTD` with uncommitted work: never touch it. Your branch is merged by the orchestrator afterwards.
- Build: configure a Release build dir inside your worktree: `CCACHE_BASEDIR=<your worktree> cmake -S . -B build-rel -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache`, then `cmake --build build-rel --target openttd_test`. Release only; no Debug build in this step.
- Tests: run with the parallel runner `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/ptest.sh <binary> "<filter>" 24` (prints failing cases and wall time).

## Why (settled)
`Train::Tick` (`src/train_cmd.cpp:4213-4215`) calls `TrainLocoHandler` twice per game tick, and each call runs `UpdateSpeed()` then advances (`train_cmd.cpp:4119-4148`). `StepJunctionTrain` (`src/junction_train_model.cpp:29`) does one UpdateSpeed + advance and counts it as one tick, so every tick count from the model is twice the game's. Rig evidence (R2): the Mastermind consist from 61 to 190 km/h took 64 game ticks; the model needs 128 single steps, 64 double steps. Coverage in tiles is unaffected (both trains scale alike); tick counts halve.

## Read first
- `CODINGSTYLE.md`.
- `src/junction_train_model.{h,cpp}` (whole), `src/tests/junction_train_model.cpp` (whole).
- `src/train_cmd.cpp` 4029-4150 and 4203-4216.

## Change
- Move the body of `StepJunctionTrain`, minus its tick increment, into a file-local `static void StepJunctionLocoHandler(const JunctionTrainParams &params, JunctionTrainState &state)` documented as one `TrainLocoHandler` pass (UpdateSpeed then advance), citing `train_cmd.cpp:4119`.
- `StepJunctionTrain` calls it twice, then `state.ticks++`. Update its doc comment (header and source) to say one game tick is two passes, citing `train_cmd.cpp:4213-4215`.
- Nothing else in the model changes. If the tick increment is not a single statement you can lift out, stop and report.

## Tests (`[junction][train]`)
1. **New, red first:** "StepJunctionTrain: one tick is two loco-handler passes". With the D138 train (`power_hp` 14194, `max_te` 1090000, `mass_t` 280, `air_drag` 25, `max_speed` 196): starting from speed 61, subspeed 0, the number of `StepJunctionTrain` calls until speed ≥ 190 is 64 (the rig's measurement, R2). Red: today's code gives 128 (record the output).
2. Every existing test: run `[junction]` and the full suite. A test that asserts a specific tick count, interval or margin from the model will change. **Do not edit it.** List each one (name, old and new value) and stop after the gates. Equalities between two model calls (e.g. `JunctionTicksAtSpeed(...) == state.ticks`) must keep passing.

## Do not
- Change `JunctionPrioLength`, the throughput evaluator, or any tile-valued result. If a tile-valued result changes (for example a test asserting `JunctionPrioLength` or a coverage in tiles), stop and report it: that would contradict the scaling argument.
- Build Debug, touch `/home/pete/OpenTTD`, or the play worktree.

## Gates
1. Release `openttd_test` builds with no errors and no new warnings.
2. `[junction]` and the full suite through `ptest.sh`.
3. Commit in your worktree: `Fix: Junction train model steps twice per game tick`, body ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. Commit even if step 2 lists changed tick assertions, with those tests left failing, and say so.

## Report
- References read
- Worktree path, branch name, commit hash and subject
- Red evidence for test 1
- Functions added or removed: names only
- Tests whose tick values changed (name, old, new); any tile-valued change
- Suite results and wall times
- Stops, and anything left undone
- State: `git status --porcelain` and `git log -1` in your worktree

When you have reported, stop.
