# G3 R3a: the joint loop records at a point and detects there (DESIGN-G3 §G3.6, D240)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Goal
Today the joint loop (test "joint: the joint loop on the hubs", `[.g3loop]`) records its rules through the fixed path at given choices. The lazy families (occupancy, the hug pins, the X and Y clearance) are detected only at the fixed path's own flat solves. When the static relations contradict each other at those choices, the flat solves fail and the families are missing, as with all-floor choices on L_R x L_RR, L_R x LL_RR and LL_RR x LL_RR (RECON-R3-loop.md). The loop then cycles, or finds an infeasible set.

After this step:
- A recording can be given a point (x and y per placement point). It then detects the lazy families at that point, not at its flat solves, and keeps recording when a solve fails.
- The recording returns the point its own Y and X solves found (least rows, then least columns plus the D215 push: the canonical point, §G3.6 F2), when they succeed.
- A function `SolveJunctionJointLoop` runs: record → joint MILP → canonical point → record again, until nothing new is found. The test drives the five hubs through it.

The fixed path without a point is unchanged, so identity does not change.

## State
- Worktree `/home/pete/OpenTTD/.claude/nodes`. First: `git -C /home/pete/OpenTTD/.claude/nodes checkout -B wt-r3a <HEAD given at launch>`. Rebuild `openttd_test` in `build` and `build-rel` (delete stale `.pch` files if the build complains about them). Leave untracked files alone.
- Never touch the other worktrees under `.claude/`.
- Output files go in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/r3a/`.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md`;
- `docs/junction-tool/DESIGN-G3-vectors.md` §G3.6 (lines 95-170);
- `docs/junction-tool/RECON-R3-loop.md`;
- `src/junction/placement/junction_placement.h`: `JunctionSystemClash`, `JunctionPlacementSystem`, `RecordJunctionPlacementSystem`;
- `src/junction/placement/junction_placement.cpp`:
  - `PlaceTrackGraph`'s signature;
  - `record_system`, the lambda that fills `*system` (grep "recorded_axis"), and `solve`;
  - `add_occupancy` (grep "auto add_occupancy") through its `add_relation`;
  - `detect`, the block in `build()` that calls it (grep "R3 hands detect another point");
  - `detect_y`, `plan_clearance`, and the fixed path (grep "One Y solve and one X solve with the choices fixed");
  - `RecordJunctionPlacementSystem`;
- `src/junction/placement/junction_joint.h` and `junction_joint.cpp` (`SolveJunctionJoint`, its result);
- `src/tests/junction/placement/junction_joint.cpp`: the helpers, and "joint: the joint loop on the hubs" (grep `[.g3loop]`).

## Changes
### 1. A point as an input (`junction_placement.h/.cpp`)
- In `junction_placement.h`, after `JunctionBendSizes`:
  ```cpp
  /** A placement point set: per placement point, its column and row (DESIGN-G3 §G3.6 F2, R3). */
  struct JunctionSystemPoint {
  	std::vector<int> x; ///< Per point, its column.
  	std::vector<int> y; ///< Per point, its row.
  	auto operator<=>(const JunctionSystemPoint &) const = default;
  };
  ```
- `JunctionPlacementSystem` gains two members, at its end:
  ```cpp
  JunctionSystemPoint solved; ///< The recording's own solution when its Y and X solves both succeed (least rows, then least columns with the D215 push); empty otherwise.
  std::vector<std::string> failed; ///< The recording's failed solves and builds, in order, e.g. "Y solve: <error>", "X build: <error>".
  ```
- Add a trailing `const JunctionSystemPoint *at = nullptr` to `PlaceTrackGraph` (definition and every call) and to `RecordJunctionPlacementSystem` (header and definition), which passes it on. It is read only when `fixed != nullptr && system != nullptr`. If `at->x.size()` or `at->y.size()` is not `pt_count`, fail with "the given point does not fit the placement points".

### 2. Recording at a point (only when `at` is given)
In the fixed path:
- **Y clearance.** Instead of `plan_clearance(fixed->port_offset)`, call `detect_y(at->x, at->y)`.
- **Y solve.** If the Y `build()` fails, push "Y build: " + the error into `system->failed`, and use `vy = at->y`. If `solve(vy)` fails, push "Y solve: " + the error, and set `vy = at->y`. Then go on to the X phase. `solve` has already recorded the Y system before it solved.
- **X detection.** In `build()`'s block that calls `detect` (comment "R3 hands detect another point"), when `at` is given, call `detect(at->x)` in place of the flat solve and `detect(flat)`, at the same place. `compute_node_tracks()` stays before it.
- **X solve.** If the X `build()` fails, push "X build: " + the error and stop there. Run `solve(vx)` as today. If it fails, push "X solve: " + the error. If both solves succeeded, set `system->solved = {vx, vy}`.
- Each failure is recorded and the function returns as it does today. The point of the change is that `*system` holds both axes whenever both builds succeed.
- The tie rule (§G3.6 F1), only when `at` is given: in `add_occupancy`'s `add_relation`, a pair the point puts on overlapping columns (`flat_left(b) <= flat_right(a)`) is a tie only when `JunctionTracksMayShareTile(a.bits, b.bits)` or either is a footprint, as the existing call decides. Otherwise it gets a separation row, `tie = false`, ordered as the existing loop orders the pair. Without `at`, the existing rule is unchanged.
- Without `at`, nothing in the fixed path changes.

### 3. The loop (`junction_joint.h/.cpp`)
Add:
```cpp
/** The joint loop's result (DESIGN-G3 §G3.6, R3). */
struct JunctionJointLoopResult {
	JunctionPlacementChoices choices; ///< The last joint solution's lengths as choices.
	JunctionSystemPoint point; ///< The last canonical point.
	int rounds = 0;
	std::string outcome; ///< "settled", or the reason it stopped: "infeasible: <conflict chain>", "solver status <n>", "cycle of period <p>", "round limit".
	std::vector<std::string> log; ///< One line per round: lengths changed, clashes kept, new clashes, the recording's failures.
};

/**
 * The joint loop (DESIGN-G3 §G3.6, R3): record the rules at the choices and the point, solve the joint model with every clash kept so far, take the recording's canonical point (or the joint solution's when the recording's solves fail), and repeat until a round keeps no new clash and changes no length.
 * @param choices The first round's choices (a seed's, or all floors).
 * @param max_rounds The round limit.
 */
JunctionJointLoopResult SolveJunctionJointLoop(const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, const JunctionTrackOrder &order, const JunctionTrackStructure &structure, const JunctionTrackPrograms &programs, const JunctionSpec &spec, const JunctionPlacementChoices &choices, int max_rounds = 12);
```
Its body is the test's existing loop, moved and changed as follows:
- **Round 0** records at `choices` with no point.
- **Round k ≥ 1** records at the last joint solution's choices with `at` = the current point.
- **Clashes** are kept across rounds by key, exactly as the test keeps them today (`kept`, `reversed`, `added_in_round`), and the system handed to `SolveJunctionJoint` carries all of them.
- **The point after a round**:
  - If the next round's recording has `solved` non-empty, the point for detection in the round after it is that `solved`.
  - Otherwise it is the joint result's `x`, `y`.
  - So each round records at (new choices, previous point), takes that recording's `solved` as the new point, and the round after detects there.
- **"settled"**: a round whose joint choices equal its input choices, that keeps no new clash, and whose recording's `solved` equals the point it was given.
- **Cycle**: the test's existing cycle rule, with the period.
- **Stops**: an infeasible joint solve gives "infeasible: " + the conflict chain joined by " <- ". Any other non-optimal status gives "solver status <n>". The last round number gives "round limit".
- **log**: each round's line carries the joint status, the objective, the sum of L, the lengths changed, the clashes kept (new and total), the recording's `failed` joined by "; ", and the round's milliseconds.

Move only what the loop needs out of the test into this function. Leave the test's reporting (`ApproximatedGaps`, the "without the bridge-over-S-bend relations" probe) in the test.

## Tests (`src/tests/junction/placement/junction_joint.cpp`)
1. **"joint: a recording at a point detects there"** (`[junction][milp]`, red first).
   - LL_RR x L_R at the seed's choices. Record without a point, and take `solved`; CHECK it is non-empty.
   - Record again with `at` = that point, and CHECK that the two systems' digests (`SystemDigest`) are equal. At its own canonical point the detection is today's: flat solve and canonical point agree on flat ground for this hub. If they do not, STOP and report the first differing relation. Do not loosen the test.
   - Then L_R x L_RR at all-floor choices, with `at` = the joint result's `x`, `y` from a `SolveJunctionJoint` on the no-point recording. CHECK that the recording has X relations (axis 0) and at least one clash, which today's no-point recording lacks (RECON-R3-loop.md §2).
   - Red: add the `at` parameter and ignore it; the second part fails with no clashes. Record the message.
2. **"joint: the loop ends settled or names a reason"** (`[junction][milp][.sweep]`). The five hubs at the existing test's round-0 choices, through `SolveJunctionJointLoop`:
   - T and LL_RR x L_R: CHECK outcome "settled", and that `PlaceJunctionTrackGraph` at the result's choices places.
   - The other three: CHECK the outcome is non-empty, and WARN each hub's outcome, rounds and log.
   - This test reports; it does not require the failing hubs to settle.
3. Change `[.g3loop]` to call `SolveJunctionJointLoop`, keeping its WARN lines and its final `PlaceJunctionTrackGraph` check.

Every other existing test stays exactly as it is.

## Do not
- Change the fixed path's behaviour without `at`, or any relation's value, tag or reason.
- Change `SolveJunctionJoint`'s model.
- Add the t (G3.4a) model, a port-offset variable, or fan signs.
- Touch the hover, the pipeline, the window or `MakeJunctionGraphSeed`.
- Use `pkill`, a broad `kill`, or a bare `git stash`.

## Stop conditions
- Identity, or any existing test, changes.
- Test 1's first part differs.
- Anything not covered.

## Gates
- 0 new warnings;
- `tools/junction/jt fast build '[milp]' 10`;
- `tools/junction/jt gate build-rel`;
- `tools/junction/jt sweep build-rel` (budget 90 s; if the new loop test pushes it over, report the time and STOP);
- `tools/junction/jt identity build-rel`. If a gate is over budget, re-run it once and report both times.

Update `src/junction/placement/CLAUDE.md`, standalone and without D-ids or references: a recording can be given a point, at which it detects occupancy, the hug pins and the clearance, and it returns its own solution; the joint loop records at the joint solution's canonical point until no new clash appears. One commit on wt-r3a: `Feature: Junction joint loop records and detects at a point`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
- references read;
- the commit;
- red evidence;
- types, members and functions added;
- per hub: the outcome, the rounds, and the last log line;
- gate, sweep and identity counts and times;
- stops;
- `git status --porcelain` and `git log -1`.

When you have reported, stop.
