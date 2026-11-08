# S1b2: queue spillback: held trains only at hold points

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD`, branch `feature/junction-tool`, head given in your launch prompt.
- S1b (`briefs/S1b-spillback.md`, commit 3c28377ffc) added `JunctionQueueConflicts`. Its model held a train at **every** signal on a path. That is too strict: a train only stands at a signal if the train ahead of it is held further on, and a queue deeper than the junction's waiting room is a capacity question (the S1a bottleneck), not a geometry one. The generated test junction now fails the `ml_blocked` CHECK (5 conflicts). This step corrects the model, so the suite tells us whether those conflicts are real.

## Read first
- `CODINGSTYLE.md` (in full).
- `src/junction_throughput.{h,cpp}` and `src/tests/junction_throughput.cpp` (whole).
- `briefs/S1b-spillback.md` (the model being corrected).
- `docs/junction-tool/DECISIONS.md` rows D71 and D113 (search for `| D71 |` and `| D113 |`).

## Corrected model (settled; implement exactly this)
- **Hold points of movement m.** A train of m is held only where the train ahead of it would be held. With one train queued (depth 1, the D71a/D113 case), that's:
  - **(a) Path end:** a train held beyond the exit port (unchanged).
  - **(b) Merge signals:** a signal piece on m's path whose next block is used by some movement k that does not use m's block immediately before that signal. "Next block" means the block of the first non-signal piece after the signal on m's path; "block before" means the block of the last non-signal piece before it. If there is no block before (the signal is the path's first piece), it is not a merge signal. These are the signals where m waits for another flow.
  - All other signals are no longer stop positions.
- Everything else is unchanged: the body `[max(0, p - TL), p]`, held blocks, the same-signal exemption, the same-exit exemption for path ends, the `ml_blocked` flag, ordering and dedup.
- **Depth-2 queues and beyond are out of scope.** They are judged by capacity (S1a) and, later, by the rig.

## Code changes
- In `JunctionQueueConflicts`, build the stop list from (a) and (b) only. Add one file-local static helper, `IsMergeSignal`, taking the paths, the per-piece blocks, m and the piece index. Document it in one line.
- Update the `JunctionQueueConflict` and `JunctionQueueConflicts` doc comments to say "hold points (path end and merge signals)".

## Tests
1. **Test 1's passing variant.** The rules-test fixture has no signal on the branch, so the whole branch is one block with the ML's first tiles. A real layout always has a signal after a split (D24). Add one block signal on the branch's first signal-legal piece after the split, facing along the branch, using the same `AddSignal` call style. Its exact position is that of the first piece after the split that `IsLegalSignalPiece` accepts. Then assert no `ml_blocked` conflict. In the failing variant (split inside the bridge's block, no legal piece before the head), keep the existing assertion. If adding the branch signal also removes the failing variant's conflict, stop and report the geometry.
2. **Test 2:** remove the `queued==0 && stop==9P` filter added in S1b. With hold points, the x = 3 signal is no longer a stop (unless it is a merge signal: work out which in a comment). Assert the full conflict list for both tl values.
3. **Test 3:** unchanged.
4. **New test** "Queue spillback: only hold points stop trains": a branch with two signals in a row before its merge, where only the second is a merge signal. A train at the first signal would block the ML, but a train at the merge signal fits. Assert no conflict. Red: under the old model (every signal a stop), it reports a conflict. Write this test first and see it fail against the current code before changing the model.
5. **The generated test junction's `ml_blocked` CHECK stays.**
   - If it passes, report the full remaining conflict list.
   - If it still fails, do not change the generator. Report each `ml_blocked` conflict with: queued movement, stop tile, the merge it waits at (which movement k makes it a merge signal), the distance in tiles from the ML split to that signal, the TL, and the blocked ML movement and block. Then stop after the gates. This would be a D71a finding (the waiting room between the split and the merge signal is shorter than a train).

## Do not
- Change the generator, rules, signal pass, or S1a functions.
- Model queue depth beyond 1.

## Gates
1. Build `build-rel` `openttd_test` and Debug `build` `openttd_test` and `openttd`, with output to files. No errors and no new warnings.
2. `[throughput]` in both builds.
3. Run the full `build-rel/openttd_test` with output to a file.
4. Commit: `Fix: Junction queue spillback holds trains only at hold points`, with the body ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. Commit only your files (the docs in `git status` aren't yours).

## Report
- References read
- Commit: hash and subject
- Red evidence for test 4
- Types and functions added or removed: names only
- The generated junction's conflict list after the change, as above
- Suite results
- Stops, and anything left undone
- State: `git status --porcelain` and `git log -1`

When you have reported, stop.
