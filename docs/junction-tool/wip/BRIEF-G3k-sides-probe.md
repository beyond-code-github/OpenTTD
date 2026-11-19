# Brief G3k (probe, diagnosis): the track composer's "infeasible with topology sides" failures

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1` (on `wt-tracks`). Run `git switch probe-g3e` (at d5040c6f73). Commit only there (probe-only prints, behind an environment variable). Build dir `build-rel`, target `openttd_test` only. Finish with `git switch wt-tracks` and rebuild `openttd_test` in build-rel.
- Time box: 75 minutes. One heavy job at a time. Each run under `timeout -s KILL 900`.
- S = `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad`.
- Read first (main worktree `/home/pete/OpenTTD/docs/junction-tool/`): STATUS.md first bullet; `wip/BRIEF-G4p-stretch-sides.md` (how `seg_side` is filled and used); `wip/BRIEF-G3i-f0-probe.md` and its finding on T3 o0t1f0s1 (in STATUS).

## Why
With `PLANAR_COMPOSER=tracks PLANAR_SLOTBEND=1`, T2 has 2 and T3 10 variants failing "infeasible with topology sides" (`<S>/g3h-pv-T2.txt`, `-T3.txt`). G3i saw one (T3 o0t1f0s1): a clash between ED2#4 (flat) and ED3#3 (diagonal), both lines of one out channel near its port, decided by a side that leaves the diagonal no span. We need to know, per failing clash, which side source decided it (the map's `seg_side`, or the whole-line `PlanarLineSides::Side` fallback) and whether that side is right.

## Change (probe only)
- `PLANAR_SIDEDUMP=<path>` in `SolveLazy1`: for each clash a round decides by sides, append one line: the two segment keys, which source gave the side (`seg` or `line`), the side value, the direction chosen, and whether the batch or the single trial was feasible. Nothing else changes.

## Measure
1. For T2 `o1t0f0s0` and T3 `o0t1f0s1` and two more T3 variants failing this way (pick from `<S>/g3h-pv-T3.txt`), via the probe test (`JUNCTION_PROBE_HUB`, `JUNCTION_PROBE_TAG`, `PLANAR_SHAPE=default`): the dump into `<S>/g3k-<hub>-<tag>.txt`, and the clashes in the failing round.
2. For each failing clash: draw the two lines' relative position as the track map says it (the map's `Side` for their stretches) and as the solution before the round has them; say whether the side used is (i) the map's and right, (ii) the map's and wrong for these segments (e.g. a stretch side applied to a segment where the lines have not yet reached the face that orders them), (iii) the whole-line fallback (seg side 0) and wrong, (iv) other.
3. Count, over the variants measured, how many failing clashes fall in each class.

## Do not
- Change the separation, the side rules, `seg_side`, the composer or the solve. Touch ports 4077/4079 or start a game client. Use git stash.

## Commit and report
- One commit on probe-g3e: `Probe: Junction side source dump in the lazy loop (PLANAR_SIDEDUMP)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Report: references read; commit hash; measures 1-3; any stop; `git status --porcelain`; `git log -1`. Finish with `git switch wt-tracks` and the rebuild. When you have reported, stop.
