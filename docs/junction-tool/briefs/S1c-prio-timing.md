# S1c: throughput evaluator, part 3: prio timing on the real layout (ML delay)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD`, branch `feature/junction-tool`, head given in your launch prompt. You are the only writer in this worktree.
- Follows S1a/S1b/S1b2: `src/junction_throughput.{h,cpp}`, tests in `src/tests/junction_throughput.cpp`.
- Decisions: D119 (ML delay strictly zero), D101 (design train, loaded), D102 (`JunctionPrioLength`), D107 (G9b prio reach).

## Why
`JunctionPrioLength` (D102) sizes the prio coverage assuming the joiner starts from rest **at the merge**, on straight track, with blocks every `sd` tiles. On a real layout, the joiner's Entry signal sits some pieces before the merge, the shared track after the merge has the layout's own signals, and corner pieces have their own progress. This step simulates the worst case on the real layout and reports, per prio merge, whether the ML train is delayed and by how much margin.

## Read first (once each, with offset/limit)
- `CODINGSTYLE.md` (in full).
- `src/junction_throughput.{h,cpp}` and `src/tests/junction_throughput.cpp` (whole).
- `src/junction_train_model.h` and, in `.cpp`, `StepJunctionTrain` and `JunctionPrioLength` (the analytic model this step checks).
- `src/junction_rules.cpp` `CheckPrioReach` (around line 1483): how the ML movement, the merge piece, the joiner, the joiner's Entry signal piece and the coverage walk are found. Copy that identification exactly.
- `src/junction_rules_internal.h`: `BlockOfPiece`, `IsSignalledPiece`, `SwitchEventsOf`.

## The model (settled; implement exactly this)
For each prio merge that `CheckPrioReach` would examine (ML movement m, merge piece index on m, joiner movement k, joiner Entry signal piece index e on k):
- **Coordinates.** Progress along each path, with S1a's per-piece progress (`PieceProgress`). Beyond the path's last piece, both paths continue as virtual straight track with a block signal every `sd` tiles (`sd` = `spec.sd`), starting `sd` tiles after the exit port. After the merge, k and m run on the same pieces; map k's progress to m's by the offset at the merge piece.
- **Start (t = 0).** The joiner is at rest with its head at the start of its Entry signal piece e (it has just got green). The ML train's head is at the coverage start: m's progress at the merge piece minus the coverage length `prio_tl * tl` straight tiles, clamped to 0. This is the worst case: the ML train is just outside the coverage, so the Entry did not see it. The ML train runs at constant `design_speed`.
- **Step.** Each tick: advance the joiner with `StepJunctionTrain(train, state)` (the loaded design train, flat straight track; curves are not modelled in this step) and the ML train by its constant speed (same progress per tick as `JunctionTicksAtSpeed` implies).
- **Joiner body.** `[max(0, p_k - TL_progress), p_k]` on k's path; it occupies every block (`BlockOfPiece`, signal pieces excluded) of k's pieces overlapping that range, and the virtual blocks it overlaps beyond the exit.
- **Delay.** The ML train is delayed if, in some tick, its head crosses the start of a signal piece on m's path facing its travel (or a virtual signal) while the block beyond that signal is occupied by the joiner body. Record the first such event.
- **Margin.** If never delayed: the minimum over ticks and over m's signals crossed by the ML head of (ticks until the joiner body leaves the block beyond that signal) negated, i.e. how many ticks earlier the joiner cleared it than the ML head arrived. Report the smallest such clearance in ticks (`margin_ticks`, ≥ 0).
- **End.** Stop when the joiner's speed reaches `design_speed` and its tail is beyond the merge, or after 20000 ticks (then report `timed_out`).

## API (`src/junction_throughput.h`)
```cpp
/** Worst-case timing of one prio merge on the real layout (PLAN-sim-driven §3.1, ML delay; D119). */
struct JunctionPrioTiming {
	size_t ml; ///< Index of the ML movement.
	size_t joiner; ///< Index of the joining movement.
	TileIndexDiffC merge_tile; ///< Layout offset of the merge piece.
	uint32_t entry_to_merge; ///< Progress from the joiner's Entry signal piece start to the merge piece start.
	bool ml_delayed; ///< The ML train reaches a signal whose next block the joiner still holds.
	uint32_t margin_ticks; ///< When not delayed: the smallest clearance in ticks. 0 when delayed.
	bool timed_out; ///< The run hit the 20000-tick cap.
};
```
- Add `std::vector<JunctionPrioTiming> prio_timings;` to `JunctionThroughput` after `queue_conflicts`, filled by `EvaluateJunctionThroughput`.
- Add the helper:
```cpp
std::vector<JunctionPrioTiming> JunctionPrioTimings(const JunctionLayout &layout, const JunctionSpec &spec, const std::vector<JunctionPath> &paths, const JunctionTrainParams &train, uint16_t design_speed, uint8_t tl);
```
Order the result by (ml, merge piece index on ml). File-local statics as needed; document each in one line.

## Tests (write first, see each fail for its stated reason; tag `[junction][throughput]`)
1. **Outer:** in "EvaluateJunctionThroughput on the generated test junction", `CHECK` that `prio_timings` is non-empty and that no entry has `ml_delayed` or `timed_out`. Red: the stub returns empty, so the non-empty check fails.
   - If it fails on the current generator for `ml_delayed`, do not change the generator. Report each delayed prio: ML and joiner movement names, merge tile, `entry_to_merge`, coverage length, and the signal where the ML train was stopped. Then stop after the gates. That is a D119 finding.
2. "Prio timing: coverage from JunctionPrioLength is enough at the merge". A hand-made straight ML with a joiner whose Entry signal is on the piece immediately before the merge (`entry_to_merge` = one piece), coverage `prio_tl * tl` set from `JunctionPrioLength` rounded up to whole TL. Assert `ml_delayed` false. Red: the stub.
3. "Prio timing: a distant Entry signal delays the ML". Same as test 2, but the joiner's Entry signal is 6 straight tiles before the merge, coverage unchanged. Work out in a comment, from the joiner's acceleration, why this delays the ML (or, if your arithmetic says it does not, increase the distance to the smallest whole number of tiles that does and say so). Assert `ml_delayed` true. Red: the stub returns an entry with `ml_delayed` false (make the stub return one dummy entry for this red step).
4. "Prio timing: margin grows with coverage": test 2's fixture with coverage one TL longer gives a larger `margin_ticks` than test 2. Red: stub.

## Do not
- Change the generator, the rules, the signal pass, S1a/S1b functions, or `JunctionPrioLength`.
- Model curves, slopes, bridge speed limits, realistic braking or path signals.

## Stop conditions
- `CheckPrioReach`'s identification cannot be reused without changing `junction_rules.cpp`.
- A prio merge whose joiner has no Entry signal piece.

## Gates
1. Build `build-rel` `openttd_test` and Debug `build` `openttd_test` and `openttd`, with output to files. No errors and no new warnings.
2. `[throughput]` in both builds.
3. Run the full `build-rel/openttd_test` with output to a file.
4. Commit: `Add: Junction throughput evaluator, prio timing`, with the body ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. Commit only your files.

## Report
- References read
- Commit: hash and subject
- Red evidence per test
- Types, functions and members added: names only
- The generated junction's full `prio_timings` list (movement names, merge tile, `entry_to_merge`, delayed, margin_ticks)
- Suite results
- Stops, and anything left undone
- State: `git status --porcelain` and `git log -1`

When you have reported, stop.
