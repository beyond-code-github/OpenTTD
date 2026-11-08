# S1a: throughput evaluator, part 1: running times, block occupation and the bottleneck

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD`, branch `feature/junction-tool`, head given in your launch prompt.
- Plan: `docs/junction-tool/PLAN-sim-driven.md` §3.1 (L1, blocking-time evaluator), step S1. Decisions D119 (strictly zero ML delay, YAPF only, default mix) and D102 (prio length from the train).
- This step is the first of several: S1b (queue spillback), S1c (prio ML delay), S1d (YAPF lane balance) and S1e (rig calibration) follow, each as its own brief. Do none of them here.

## Read first (once each, with offset/limit)
- `CODINGSTYLE.md` (in full; required).
- `docs/junction-tool/PLAN-sim-driven.md` §3.1.
- `src/junction_train_model.h` and `.cpp`: `JunctionTrainParams`, `JunctionTrainState`, `StepJunctionTrain`, `JunctionTerminalSpeed`, `JunctionProgressPerStraightTile`, `JunctionProgressPerCornerPiece`.
- `src/junction_route.h` (`JunctionPiece`: tile, trackdir, units, on_bridge).
- `src/junction_rules.h` (`JunctionPath`, `JunctionCheckContext`, `MakeJunctionCheckContext`, `FindJunctionPorts`) and `src/junction_rules_internal.h` (`JunctionTMovements`, `ComputeBlocks`, `BlockOfPiece`, `IsSignalledPiece`).
- `src/junction_layout.h` (`JunctionSignal`, `JunctionLayout`).
- `src/tests/junction_rules.cpp` lines 1-80 (the `Build` and `AddSignal` helpers) and the test "CheckSignals: no split in a bridge's block" (around line 1371), as an example of hand-made layouts.
- `src/tests/junction_generator.cpp`: how the test spec is made (`MakeTestJunctionSpec` or equivalent) and how S1 "GenerateJunction gives a rule-clean L_R T" gets its layout.
- `src/CMakeLists.txt` and `src/tests/CMakeLists.txt`: where the junction files are registered.

## The model (settled; implement exactly this)
- **Design speed:** `v = JunctionTerminalSpeed(train)`. Every train runs through the junction at constant speed v; nothing stops in this step.
- **Path length:** each `JunctionPath` piece adds progress. A piece with `units == 2` (straight) or `on_bridge` adds `JunctionProgressPerStraightTile()`; a diagonal piece (`units == 1`) adds `JunctionProgressPerCornerPiece()`. Before coding, confirm from `junction_route.h`/`.cpp` that this is what `units` and `on_bridge` mean (in particular that bridge span tiles appear as `on_bridge` pieces). If not, stop and report.
- **Time at constant speed:** the ticks to cover a progress distance at speed v come from stepping `StepJunctionTrain` from a state with `speed = v`, until `distance` reaches the target. Add one function for this (below). It is exact game physics, with no closed-form approximation.
- **Block entry and exit along a path:** walk the path's pieces in order, using `BlockOfPiece` (with `ComputeBlocks(layout)`) per piece.
  - A maximal run of consecutive pieces in the same block is one occupation of that block.
  - The occupation's entry position is the progress at the start of its first piece; the exit position is the progress at the end of its last piece. The first block starts at progress 0 (the train arrives from the line outside).
  - Pieces with no block (signal pieces in our block model) belong to no occupation.
- **Blocking interval (original braking):** for an occupation with entry position s_in and exit position s_out, the block is held from `t(s_in)` to `t(s_out + TL_progress)`. Here `t(s)` is the time the head reaches progress s, and `TL_progress = tl * JunctionProgressPerStraightTile()` is the train's length. Under the original braking model a train only needs a signal green as it reaches it, so there is no approach time before s_in. Do not add margins.
- **Headway matrix:** for movements i and j, `H[i][j] = max(0, max over blocks b used by both of (end_i(b) - start_j(b)))`, with each movement's times measured from its own arrival at its path start. When they share no block, `H[i][j] = 0`. When a movement uses a block more than once, use the earliest start and the latest end for that movement and block.
- **Utilisation and the bottleneck:**
  - With mix weights `w_m` (normalised to sum 1), each block's load is `sum over m of w_m * (end_m(b) - start_m(b))`, in ticks per train of the mix.
  - The bottleneck is the block with the largest load.
  - Throughput is `1000.0 / max_load` trains per 1000 ticks. Report the bottleneck block, its load, and the movements using it.

## New files and API
`src/junction_throughput.h` / `.cpp` (register them next to the other junction files in `src/CMakeLists.txt`), and `src/tests/junction_throughput.cpp` (register it in `src/tests/CMakeLists.txt`).

```cpp
/** A block a movement holds, in ticks from the train's arrival at the path start (PLAN-sim-driven §3.1). */
struct JunctionBlockOccupation {
	size_t block;
	uint32_t start; ///< The head reaches the block.
	uint32_t end; ///< The tail leaves the block.
};

/** One movement through the junction at design speed. */
struct JunctionMovementTiming {
	JunctionPortRole from;
	JunctionPortRole to;
	uint64_t length; ///< Path progress.
	uint32_t ticks; ///< Head from path start to path end.
	std::vector<JunctionBlockOccupation> occupations; ///< In path order.
};

/** Relative traffic per movement; weights need not sum to 1. */
struct JunctionTrafficMix {
	std::vector<std::pair<std::pair<JunctionPortRole, JunctionPortRole>, double>> weights;
};

/** What the evaluator reports. */
struct JunctionThroughput {
	uint16_t design_speed;
	std::vector<JunctionMovementTiming> movements; ///< In JunctionTMovements() order.
	std::vector<std::vector<uint32_t>> headway; ///< headway[i][j]: the least ticks between a train of movement i and a following train of movement j.
	size_t bottleneck_block;
	double bottleneck_load; ///< Ticks per train of the mix.
	std::vector<size_t> bottleneck_movements; ///< Indices into movements.
	double trains_per_1000_ticks;
};

uint32_t JunctionTicksAtSpeed(const JunctionTrainParams &train, uint16_t speed, uint64_t distance);
JunctionTrafficMix MakeDefaultJunctionMix();
std::optional<JunctionThroughput> EvaluateJunctionThroughput(const JunctionLayout &layout, const JunctionSpec &spec, const JunctionTrainParams &train, uint8_t tl, const JunctionTrafficMix &mix);
```
- `MakeDefaultJunctionMix`: weight 3 for each ML movement (ML through, both directions) and 1 for each branch movement, over `JunctionTMovements()`. Identify ML movements by role the way the existing rules code does. This is an orchestrator default, recorded as reversible.
- `EvaluateJunctionThroughput` returns `std::nullopt` when the context's paths don't cover every movement in `JunctionTMovements()`, or when the layout has a signal type other than block, entry, exit or combo (PBS is a later step).
- Paths come from `MakeJunctionCheckContext(layout, spec, FindJunctionPorts(layout), JunctionTMovements())`, the same way the rules use it. Copy the exact call from `CheckJunctionRules`.

## Tests (write first, outermost first, see each fail for its stated reason)
In `src/tests/junction_throughput.cpp`, tag `[junction][throughput]`. Copy the `Build`/`AddSignal` helpers from `tests/junction_rules.cpp` as file-local statics (do not move them into a shared header).

1. **Outer:** "EvaluateJunctionThroughput on the generated test junction". Generate the S1 test junction (the same spec and call as the S1 test). Use a train fixture: `JunctionTrainParams{power_hp 4000, max_te 300000, mass_t 500, air_drag 0, max_speed}` with max_speed = the test spec's speed if there is one, else 160 km/h converted to cur_speed units as the train-model tests do. Use tl = the spec's TL. Assert:
   - a result is returned, with one movement per `JunctionTMovements()` entry;
   - each movement has at least one occupation, with starts non-decreasing and every `end > start`;
   - `trains_per_1000_ticks > 0`, and the bottleneck movements include at least one ML movement.
   Red: the function stub returns `std::nullopt`.
2. "JunctionTicksAtSpeed matches stepping": the same as stepping by hand for 10 tiles at a speed the fixture can hold. Red: the stub returns 0.
3. "Occupation on a straight signalled line": a hand-made straight line of 12 tiles on row 0 (`Build({{{0,0}, Trackdir::X_SW, "FFFFFFFFFFFF"}})`) with block signals at x = 3 and x = 7. Work out the block boundaries by hand in a comment, then assert each occupation's start and end exactly, using `JunctionTicksAtSpeed` for the expected times (including the +TL tail). Call the internal occupation helper (below) on this single path, since a straight line has no ports or movements. Red: the stub returns an empty vector.
4. "Headway from shared blocks": two hand-built `JunctionMovementTiming` values with known occupations. Assert `H` from the internal helper, for these cases:
   - a shared block;
   - no shared block (0);
   - negative clamped to 0.
   Red: the stub returns zeros.
5. "Bottleneck load with a mix": three hand-built timings and a mix. Assert the bottleneck block, its load (exact arithmetic), and `trains_per_1000_ticks`. Red: the stub.

For tests 3-5, expose three helpers in the header (still used by `EvaluateJunctionThroughput`):
```cpp
std::vector<JunctionBlockOccupation> JunctionPathOccupations(const JunctionLayout &layout, const std::vector<JunctionPiece> &pieces, const JunctionTrainParams &train, uint16_t speed, uint8_t tl);
std::vector<std::vector<uint32_t>> JunctionHeadways(const std::vector<JunctionMovementTiming> &movements);
void JunctionFindBottleneck(const std::vector<JunctionMovementTiming> &movements, const std::vector<double> &weights, JunctionThroughput &out);
```

## Do not
- Model stops, queues, prio holding, pathfinder choice, PBS or realistic braking (later briefs).
- Change any existing file except the two CMakeLists.
- Add GUI, console commands or settings.

## Stop conditions
- `units`/`on_bridge` don't mean what the model assumes.
- `BlockOfPiece`/`ComputeBlocks` don't give a block per piece the way the model needs.
- The test spec has no train speed, and the train-model tests give no cur_speed conversion to copy.
- Anything would need savegame or game-state changes.

## Gates
1. Build `build-rel` `openttd_test` and Debug `build` `openttd_test` and `openttd`, with output to files. No errors and no new warnings.
2. `./openttd_test "[throughput]"` passes in both builds.
3. Run the full `build-rel/openttd_test` directly, with output to a file. Report failures.
4. Commit: `Add: Junction throughput evaluator, block occupation and bottleneck`, with the body ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. Commit only your files and the CMakeLists.

## Report
- References read
- Commit: hash and subject
- Red evidence: each test, and its failure message before the implementation
- Types, functions and members added: names only
- For test 1: the actual numbers (design speed, per-movement ticks, bottleneck block and load, trains per 1000 ticks)
- Suite results
- Stops, and anything left undone, with the reason
- State: `git status --porcelain` and `git log -1`

When you have reported, stop.
