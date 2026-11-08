# D137: a design train is required; the picked train sizes the prio

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD`, branch `feature/junction-tool`, head given in your launch prompt. You are the only writer here. (Another agent works in `/home/pete/OpenTTD/.claude/play`; don't touch it.)
- Decisions: `docs/junction-tool/DECISIONS.md` D137 (the ruling), D138 (the design train), D119 (ML delay strictly zero), D101 (loaded design train).
- Today `JunctionSpec::prio_tl` (train lengths) is fixed at 1 by `MakeJunctionSpec` (`src/junction_game.cpp:444`) and `MakeTestJunctionSpec` (`src/junction_generator.cpp:98`), so every prio covers `tl` tiles. S1c2 (`JunctionPrioTimings`, `src/junction_throughput.cpp`) shows that delays the ML on the generated test junction: the outer test "EvaluateJunctionThroughput on the generated test junction" fails on `ml_delayed`. That failing test is your outer red test.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md`.
- `src/junction_generator.h` (`JunctionSpec`, `JunctionGenerateResult`, `JunctionLibraryEntry`, `GenerateJunction`), and in `src/junction_generator.cpp`: `MakeTestJunctionSpec`, `MakeJunctionFrame`, `GenerateJunction`, `BuildJunctionLibrary`, the library fit that builds a `JunctionGenerateResult`, and every `prio_tl` use.
- `src/junction_throughput.{h,cpp}`: `JunctionPrioTiming`, `JunctionPrioTimings`, `SimulatePrioTiming`, `EvaluateJunctionThroughput` (how it builds paths and picks `design_speed`).
- `prio_tl` uses in `src/junction_signal_pass.cpp` and `src/junction_rules.cpp`.
- `src/junction_game.{h,cpp}`: `MakeJunctionSpec`, `ReadJunctionTrain`, `JunctionPickedTrain`.
- `src/junction_gui.cpp`: `JunctionGenerateKey`, `SpecKey`/`MakeKey`, `EnsureLibrary`, `StartJob`, the TL buttons, `OnVehicleSelect`, `GetWidgetString` (`WID_BJ_TRAIN`, `WID_BJ_STATUS`), and where `shown_layout` is set.
- `src/tests/junction_throughput.cpp` (`FixtureTrain`, the outer test).

## Changes

### 1. Spec: coverage in tiles, and the train
In `JunctionSpec`:
- Replace `uint8_t prio_tl = 1;` with `uint8_t prio_tiles = 0; ///< Prio coverage upstream of a merge, in straight tiles; set by GenerateJunction's sizing (D137).`
- Add `JunctionTrainParams train{}; ///< The loaded design train (D101, D137); required.` (include `junction_train_model.h`).

Every use of the coverage becomes tiles:
- `spec.tl * spec.prio_tl` (generator lines ~309, ~335) becomes `spec.prio_tiles`.
- `2u * ctx.spec.tl * ctx.spec.prio_tl` (signal pass and rules; these count half-tile units) becomes `2u * ctx.spec.prio_tiles`.
- `MakeJunctionFrame`: `dC = std::max(0, spec.prio_tiles - spec.tl)` (the frame stays as today when the coverage is at most one TL).
- `SimulatePrioTiming`: `coverage = spec.prio_tiles * JunctionProgressPerStraightTile()`.
- Update each touched doc comment that says "train lengths" or "prio of 1".

### 2. Timing: report the delaying distance
Add to `JunctionPrioTiming`, after `worst_start`: `uint32_t worst_distance; ///< When delayed: progress from worst_start to the merge piece start (on m's path). 0 otherwise.` Fill it in `SimulatePrioTiming`.

### 3. Sizing in `GenerateJunction` (the only public generator entry)
- Move today's body of `GenerateJunction` unchanged into a file-local `static JunctionGenerateResult GenerateJunctionCovered(const JunctionSpec &spec, const JunctionTerrain *terrain)` (one-line doc).
- New `GenerateJunction`: copy the spec, set `prio_tiles = 1`, then loop:
  1. `result = GenerateJunctionCovered(copy, terrain)`. No layout: return it.
  2. Timings: `JunctionPrioTimings(*result.layout, copy, paths, copy.train, design_speed, copy.tl)` with `paths` and `design_speed` obtained exactly as `EvaluateJunctionThroughput` obtains them (call the same functions; don't re-implement).
  3. None delayed (and none `timed_out`): set `result.prio_tiles = copy.prio_tiles` and return it.
  4. Else `next = max over delayed of (worst_distance / JunctionProgressPerStraightTile() + 1)`; `copy.prio_tiles = max(copy.prio_tiles + 1, next)`. If it exceeds `MAX_PRIO_TILES` (file-local `static constexpr uint8_t MAX_PRIO_TILES = 32;`, documented as the search bound), return a result with no layout and a new `JunctionGenerateError::PrioTooLong` (doc it; give it the GUI's error string handling the other errors get, with a new string `STR_JUNCTION_ERROR_PRIO_TOO_LONG` "Prio coverage would exceed 32 tiles for this train" in `src/lang/english.txt` next to the other junction errors).
- A `timed_out` timing counts as delayed for step 4, with `worst_distance` 0 (so the coverage steps by one).
- Add `uint8_t prio_tiles = 0; ///< The prio coverage the sizing chose, in tiles.` to `JunctionGenerateResult` and to `JunctionLibraryEntry`; fill the entry from its result in `BuildJunctionLibrary`, and copy it into the `JunctionGenerateResult` the library fit builds. If the fit does not build a `JunctionGenerateResult` from an entry, stop and report.

### 4. Game and GUI: the train is required
- `MakeJunctionSpec(RailType railtype, uint8_t ml_gap, uint8_t branch_gap, uint8_t tl, const JunctionTrainParams &train)`: sets `spec.train = train`; remove `spec.prio_tl = 1`. Update its doc.
- GUI: `EnsureLibrary` and `StartJob` start nothing while `picked_train` is empty; `WID_BJ_STATUS` then shows a new string `STR_JUNCTION_STATUS_PICK_TRAIN` "Pick a design train first". Pass `picked_train->loaded` to `MakeJunctionSpec`.
- Add the picked train's `loaded` params to `JunctionGenerateKey` so a new pick regenerates (give `JunctionTrainParams` a defaulted `operator==` if the key needs it).
- TL buttons: stop clearing `picked_train` (orchestrator call, recorded under D137: TL stays adjustable, the train is kept).
- `WID_BJ_TRAIN`: the prio value shown is the shown placement's `prio_tiles` when a placement is shown (add `uint8_t shown_prio_tiles = 0;` next to `shown_layout`, set where `shown_layout` is set); otherwise `JunctionPrioLength` as today.

### 5. Tests
- `MakeTestJunctionSpec`: `spec.train` = the D138 train: `power_hp = 14194`, `max_te = 1090000`, `mass_t = 561`, `air_drag = 25`, `max_speed = 196`; remove `prio_tl`. (air_drag 25: `ground_vehicle.cpp:46-57` with the default engine air drag: 2048 / 196 = 10, 10 parts, 10 + 3·10·10/20 = 25.) Keep `tl = 4`.
- `FixtureTrain` stays for the hand-made prio tests (their arithmetic is for it); set those fixtures' `prio_tiles` to the tile value their `prio_tl` gave (`prio_tl * tl`).

## Tests (`[junction]`; red first, record each red output)
1. **Outer (already red):** "EvaluateJunctionThroughput on the generated test junction" must pass with no change to its assertions, using `spec.train` as its train where it now passes a train. Also `CHECK(result.prio_tiles > 0)`.
2. **New:** "GenerateJunction sizes the prio from the design train" (`[junction][generator]`): the test spec with `spec.train` = D138 and with `spec.train` = `FixtureTrain()` (move or copy the helper so both test files can use it, without changing its values); both give a layout; the fixture train's `prio_tiles` is larger than D138's; for each, `JunctionPrioTimings` on the result has no `ml_delayed`. Red: do steps 1, 2 and the new `JunctionGenerateResult::prio_tiles` member first, with `GenerateJunction` doing a single pass at `prio_tiles = tl` (today's coverage); run this test and record the assertion that fails (expected: the no-delay check for FixtureTrain, or the "larger" check). Then write the loop.
3. Every other existing test passes. If a test that asserts a specific layout changes because coverage is no longer `tl`, stop and report it (name, old and new values); do not edit its expectation.

## Do not
- Change the S1c2 timing model, `JunctionPrioLength`, the rules' logic beyond the unit change, STUB, or the test spec's `tl`.
- Add a default or made-up train anywhere in game code.
- Touch savegame or network code.

## Stop conditions
- Generation time per `GenerateJunction` call on the test spec more than 4x today's (measure the outer test's time before and after; report both).
- Any layout-pinning test changes (test 3 above).
- `EvaluateJunctionThroughput`'s path/speed setup cannot be called from the generator without changing its signature.

## Gates
1. Build `build-rel` `openttd_test`, Debug `build` `openttd_test` and `openttd`, output to files. No errors, no new warnings.
2. Full `build-rel/openttd_test` and `build/openttd_test "[junction]"`, output to files.
3. Commit: `Fix: Junction prio coverage is sized from the picked train`, body ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. Commit only your files.

## Report
- References read
- Commit: hash and subject
- Red evidence per test
- Types, functions, members, strings added or removed: names only
- `prio_tiles` chosen for the test spec with D138 and with FixtureTrain; iterations the loop took for each
- Outer test time before and after
- Suite results
- Manual checks: none required (the orchestrator checks the GUI)
- Stops, and anything left undone
- State: `git status --porcelain` and `git log -1`

When you have reported, stop.
