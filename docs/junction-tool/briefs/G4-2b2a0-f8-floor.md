# G4 2b2a0: the run from the lane split to the heads is a floor (F8; DESIGN-G4 §2, D268)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Goal
In the doubled-leg block, the run from the lane split to the first heads is an exact relation today. The static model needs it as a rule floor; the objective's push keeps the lane split next to its bridge. This step makes that one relation a floor and re-pins every value that moves, each under this rule.

## State
- Worktree `/home/pete/OpenTTD/.claude/nodes`: `git -C /home/pete/OpenTTD/.claude/nodes checkout -B wt-2b2a0 <HEAD given at launch>`. Leave its untracked files alone. `build-rel` (Release) exists; delete stale `.pch` files if the build complains.
- Output files go in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/2b2a0/`.
- Another agent works in `.claude/ng1` on `junction_track_geometry` and the occupancy touch rule; do not touch those.

## Read first (once each)
- `CODINGSTYLE.md` (C++ part);
- `/home/pete/OpenTTD/docs/junction-tool/wip/f8-lane-split-floor.patch` (the whole change: one comment and one `true` → `false`);
- `docs/junction-tool/TESTS.md` lines 1-12 (the runner and its budgets);
- `src/junction/placement/CLAUDE.md`.

## Change
1. Apply the patch: `git -C /home/pete/OpenTTD/.claude/nodes apply /home/pete/OpenTTD/docs/junction-tool/wip/f8-lane-split-floor.patch`. If it does not apply cleanly, apply the same two edits by hand at the `why("lane split run", ...)` block in `src/junction/placement/junction_placement.cpp` and say so.
2. `src/junction/placement/CLAUDE.md`, under Placement: add "In a doubled leg, the runs from the exit split to the lane split, from the lane split to the first heads, and from the last heads to the lane merge are rule floors; the objective's push keeps the lane split next to its bridge; copy 1's heads sit exactly one tile nearer than copy 0's."

## Tests
No new test. Run the suites, then re-pin.
1. Build `build-rel` (`openttd_test`), output to a file.
2. Run `tools/junction/jt gate build-rel`, `tools/junction/jt sweep build-rel` and `tools/junction/jt identity build-rel` (each with a Bash timeout of at least 400000 ms), outputs to files.
3. For each failing assertion: if it is a pinned value (a level hash, a digest, an objective, a cell count, a planned count) that moved, re-pin it to the new value. Add a short comment by the pin: "F8: the lane split to heads run is a floor". Record old → new for your report. Re-run until all three pass.

## Do not
- Change any other relation, floor or test logic. Change only pinned constants.
- Raise any budget or move a test between tags.
- Touch any worktree but `nodes`; `.claude/nodes2` is reserved for the engineer's game.
- Use `pkill`, a broad `kill`, or any form of `git stash`.

## Stop conditions
- A hub that seeded before no longer seeds, or a seed fails `CheckJunctionRules`.
- A failure that is not a moved pinned value (an error, a crash, a changed refusal text, a count of failures).
- A planned count in the level-hash sweeps goes down (a lost plan).
- A suite over its budget (gate 30 s, sweep 60 s, identity 45 s): report the times, do not optimise.

## Commit
One commit on wt-2b2a0: `Change: Junction lane split to heads run is a floor`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
- references read; the commit;
- every re-pin: test name, old → new;
- gate, sweep and identity: cases passed and time each;
- stops; `git status --porcelain` and `git log -1`.

When you have reported, stop.
