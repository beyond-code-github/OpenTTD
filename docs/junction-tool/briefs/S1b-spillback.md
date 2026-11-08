# S1b: throughput evaluator, part 2: queue spillback conflicts

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD`, branch `feature/junction-tool`, head given in your launch prompt.
- Follows S1a (`briefs/S1a-occupation.md`, commit dfc42f13f9): `src/junction_throughput.{h,cpp}`, tests in `src/tests/junction_throughput.cpp`.
- Plan: `PLAN-sim-driven.md` §3.1. Decisions: D113 (no split in a bridge's block), D71a/D83 (whole-train bays), D119 (ML delay strictly zero).

## Why
Trains back up when a destination is busy. A train held at a red signal with its tail inside a block that another movement needs blocks that movement, even though the other movement isn't going to the busy destination. This is the engineer's D113 observation: "a train in the last waiting bay heading bottom-left will block a train heading top-right". Today the checker enforces a proxy rule for it. The evaluator should detect it directly.

## Read first (once each, with offset/limit)
- `CODINGSTYLE.md`.
- `src/junction_throughput.h` and `.cpp` (S1a, whole).
- `src/junction_layout.h` (`JunctionSignal`: offset, facing, type).
- `src/junction_rules_internal.h` (`ComputeBlocks`, `BlockOfPiece`, `IsSignalledPiece`).
- `src/tests/junction_throughput.cpp` (whole) and, in `src/tests/junction_rules.cpp`, the test "CheckSignals: no split in a bridge's block" (around line 1371): its hand-made ML-with-bridge-and-branch layout is the fixture for test 1 below.

## The model (settled; implement exactly this)
- **Stop positions of movement m:** every piece on m's path that carries a signal facing m's direction of travel (a `JunctionSignal` on that piece's tile whose `facing` equals the piece's trackdir), plus the path's end (a train held at the first signal beyond the exit port, outside the layout). The stop position is the progress at the start of that signal piece, or the path's full length for the path end.
- **Body of a held train:** a train of movement m held with its head at stop position p covers path progress `[max(0, p - TL_progress), p]`, with `TL_progress = tl * JunctionProgressPerStraightTile()` and the same per-piece progress as S1a. The blocks it holds are the blocks (`BlockOfPiece`) of every path piece overlapping that range, signal pieces excluded.
- **Conflict:** for each other movement k (k ≠ m) whose path uses one of those blocks, there is a conflict (m, p, k, block), unless k's path also contains m's stop signal piece (the same tile and trackdir). In that case k is queued behind the same signal anyway. For the path-end stop, k is exempt when k has the same `to` port as m.
- **ML blocked:** a conflict whose blocked movement k is an ML movement (identify ML movements by role, as `MakeDefaultJunctionMix` does). Per D119 an ML movement must never be blocked by another movement's queue, so `ml_blocked` conflicts are failures. Others are reported for information.

## API changes (`src/junction_throughput.h`)
```cpp
/** A held train of one movement blocking another movement (PLAN-sim-driven §3.1, queue spillback). */
struct JunctionQueueConflict {
	size_t queued; ///< Index of the held train's movement.
	uint64_t stop; ///< Progress along the queued movement's path where its head is held.
	bool at_exit; ///< Held beyond the exit port rather than at a signal inside the junction.
	size_t blocked; ///< Index of the movement that is blocked.
	size_t block; ///< The block they share.
	bool ml_blocked; ///< The blocked movement is an ML movement.
};
```
- Add `std::vector<JunctionQueueConflict> queue_conflicts;` to `JunctionThroughput` (after `trains_per_1000_ticks`), filled by `EvaluateJunctionThroughput` for the six movements.
- Add the helper used by it and by the tests:
```cpp
std::vector<JunctionQueueConflict> JunctionQueueConflicts(const JunctionLayout &layout, const std::vector<JunctionPath> &paths, const std::vector<bool> &is_ml, uint8_t tl);
```
  `paths[i]` and `is_ml[i]` are per movement. Report conflicts in order of (queued, stop, blocked, block), with duplicates of the same (queued, stop, blocked) on several blocks kept once, using the lowest block id.

## Tests (write first, see each fail for its stated reason; tag `[junction][throughput]`)
1. **Outer:** "Queue spillback: a split in a bridge's block blocks the ML". Build the bridge-block fixture from the rules test: the ML on row 0, `"FFB1FFFF"` from (0,0) X_SW, with a branch leaving just before the bridge head and no signal-legal piece between the split and the head. Copy the branch geometry and signal positions from the rules test's failing case. Make two `JunctionPath`s:
   - ML: the ML route's raster pieces, `is_ml` true.
   - Branch: the branch route's pieces, including the shared pieces up to the split, `is_ml` false.
   Assert at least one conflict whose `blocked` movement is the ML path (`ml_blocked` true), with `queued` = the branch movement whose held train's body lies in the block containing the split and the bridge. Work out which held position causes it in a comment before asserting. Then the rules test's passing variant (with the signal-legal piece and its signal) gives no `ml_blocked` conflict.
   - Red: the stub returns empty.
   - If the fixture as described can't produce an ML-blocking held train (for example the branch's held positions all lie past the split), stop and report the geometry, rather than inventing a different fixture.
2. "Queue spillback: a whole-train bay holds its train". A straight line with signals at x = 3 and x = 9, and a second path sharing the line up to a split at x = 2 (built like test 1). With tl such that a train held at x = 9 fits inside [3, 9], there's no conflict. With a larger tl, whose tail reaches back past x = 3 into the split's block, there's a conflict. Red: the stub.
3. "Queue spillback: same-signal followers are exempt": two paths that share the stop signal give no conflict between them at that stop. Red: for this test's red step, make the stub return one dummy conflict, so the test fails because a conflict is reported.
4. **Generated test junction:** in the existing outer test "EvaluateJunctionThroughput on the generated test junction", add `CHECK` that no `queue_conflicts` entry has `ml_blocked`.
   - If this fails on the current generator, do not change the generator. Leave the CHECK, record every ml_blocked conflict (queued and blocked movement names, stop tile, block) in the report, and stop after the gates. That would be a real finding for the orchestrator.

## Do not
- Change the generator, the rules or the signal pass.
- Model time-dependent queues (how long trains wait); this is a static "can a held train block it" check.
- Add PBS or realistic braking handling.

## Stop conditions
- Test 1's fixture can't express the D113 case as described.
- `JunctionSignal.facing` doesn't relate to piece trackdirs the way the model assumes.

## Gates
1. Build `build-rel` `openttd_test` and Debug `build` `openttd_test` and `openttd`, with output to files. No errors and no new warnings.
2. `./openttd_test "[throughput]"` in both builds.
3. Run the full `build-rel/openttd_test` directly, with output to a file.
4. Commit: `Add: Junction throughput evaluator, queue spillback conflicts`, with the body ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. Commit only your files.

## Report
- References read
- Commit: hash and subject
- Red evidence per test
- Types, functions and members added: names only
- The generated test junction's full `queue_conflicts` list (movement names, stop tile, block, ml_blocked)
- Suite results
- Stops, and anything left undone
- State: `git status --porcelain` and `git log -1`

When you have reported, stop.
