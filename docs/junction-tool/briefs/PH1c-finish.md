# PH1c: finish PH1 on top of CS1

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ph1`, branch `wt-ph1`, with a WIP commit (155b1d88f2) of PH1 (`docs/junction-tool/briefs/PH1-port-heights.md`, read it whole). Its `build` and `build-rel` exist. First merge `feature/junction-tool` (head given at launch, CS1 and CS2 merged: `approach` and `continuation`, edges keep the curve rule across the track they continue from and into) into `wt-ph1`; resolve conflicts keeping both changes.
- Tests only through `tools/junction/jt` with bracketed tags (TESTS.md).

## Steps
1. Rebuild; run PH1's test 4 ("the main line's W end on higher ground is reached by a ramp"). Expected now: clean (CS2 removed the diagnosed cause, scratchpad/DIAG-ph1-test4.md: the merging edge did not see the S-bend after its goal). If it is still unclean, stop and report the violation and the pieces around it.
2. `[projection]` was 5.13 s Release with PH1, over its 5 s budget. Split it: the tests added from G4 onwards that are about the hub's edges (the E1, E2, E3, E3b and PH1 tests, and G4's "an unbuildable hub tile gives no layout") move to a new tag `[projectionedges]` (replace `[projection]` in their tag lists; keep `[junction]`). Measure both with jt (Release, budget 5 s each) and add `[projectionedges]` to TESTS.md with its time.
2b. `[junctionmap]` in Debug was 4.94 s of its 5 s budget before PH1. If PH1 pushes it over, split it the same way: the T-map construction tests that use a terrain or the line router's edges move to a new tag `[junctionmapedges]`; measure both and record them in TESTS.md.
3. Remove the leftover `HUBBLOCKED` print in the G4 test.
4. Finish PH1's report items: red evidence is in the WIP's report (reuse it), jt lines (`[startheight]`, `[approach]`, `[projection]`, `[projectionedges]`, `[tmapnear]`, `[linerouter]`, `[junctionmap]` Debug), gate.
5. Squash the WIP and your changes into one commit on `wt-ph1`: `Change: Junction routes start at their start piece's own height`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>` (a merge commit from step 0 may stay below it).

## Do not
As PH1's brief. No other change.

## Stop conditions
Test 4 unclean; any other test changes outcome; OVER BUDGET after the split; anything not covered.

## Report (short)
Commit; test 4 outcome and height; jt lines with times; stops; `git status --porcelain`, `git log -3 --oneline`.

When you have reported, stop.
