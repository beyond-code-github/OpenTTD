# Brief G3g (probe, diagnosis): what makes T3 infeasible and what still binds T2's height, in the track composer

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, currently ON `probe-g3e` at 98ef7c11c0 (built). Stay on probe-g3e; commit only there. Build dir `build-rel`, target `openttd_test` only. Finish with `git switch wt-tracks` and rebuild `openttd_test` in build-rel.
- Time box: 90 minutes. One heavy job at a time. Each run under `timeout -s KILL 900`.
- S = `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad`.
- Read first (main worktree `/home/pete/OpenTTD/docs/junction-tool/`): STATUS.md's first bullet under "Where things are" (the chain so far), `wip/BRIEF-G3f-size-probe.md`, `wip/BRIEF-G3e-slotbend-probe.md`.

## Why
The track composer (`PLANAR_COMPOSER=tracks PLANAR_SLOTBEND=1`) solves T2 at best 1918 (96x46) against `MappedComposer`'s 862 (94x29), and no T3 variant. The last fix came from capping the height (`PLANAR_MAXH=<half-tiles>`, e57e1aa14c) and reading the IIS: a rule had left a line with one diagonal heading, tying x to y. We need the same reading for what binds now, and for T3.

## Changes (probe test only)
- The probe test `"planar probe: track composer slot bends"` reads `JUNCTION_PROBE_HUB` (`T2` default, or `T3`: `PlanarTText(3)`) and `JUNCTION_PROBE_TAG` (default `o1t3f1s0`) for its solve (the step-1 dump stays T2 o1t3f1s0), and prints the solve line with the hub and tag.

## Measure
1. **T3:** with `PLANAR_IIS=1 PLANAR_SHAPE=default`, tags `o0t1f1s1` and `o0t1f1s0` (MappedComposer's best two), into `<S>/g3g-T3-<tag>.txt`. For each: the IIS rows grouped by strand chain, and for each group which composer rule made its rows (heading list from `BuildPlanarLineHeadings`, a slot bend (line, D, rule 1/2), a port step (D359), a copy (doubled piece), a deck (per run), the shared finish (port order, box, bay, deck max, prio row)). Print the heading lists of the lines involved (from the strands hook `PlanarTrackComposerStrands`) beside it.
2. **T2:** tag `o1t3f1s0`, `PLANAR_MAXH` = 80, then 70, then 60 (half-tiles), each with `PLANAR_IIS=1 PLANAR_SHAPE=default`, into `<S>/g3g-T2-maxh<N>.txt`: the same grouping for the first cap that is infeasible, with the heading lists of the lines involved.
3. For every group in 1 and 2, say in one line whether it is (a) a line whose heading list cannot give independent x and y spans between two fixed points (name the line and list), (b) a ruled row (name which), or (c) something else (describe).

## Do not
- Change any composer row, rule, heading rule or the solve. Touch ports 4077/4079 or start a game client. Use git stash.

## Commit and report
- One commit on probe-g3e: `Probe: Junction track composer probe test takes a hub and tag`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Report: references read; commit hash; measures 1-3 (groups, rules, heading lists, classification); any stop; `git status --porcelain`; `git log -1`. Finish with `git switch wt-tracks` and the rebuild. When you have reported, stop.
