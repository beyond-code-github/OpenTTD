# Brief G3f (probe, diagnosis): why the track composer's T2 is 3x the area, and why T3 is infeasible

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1` (on `wt-tracks`). Run `git switch probe-g3e` (at e824c9891b). Commit only there. Build dir `build-rel`, target `openttd_test` only. Finish with `git switch wt-tracks` and rebuild `openttd_test` in build-rel.
- Time box: 90 minutes. One heavy job at a time. Each run under `timeout -s KILL 900`.
- S = `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad`.
- Read first (main worktree, `/home/pete/OpenTTD/docs/junction-tool/wip/`): `BRIEF-G3e-slotbend-probe.md` (what probe-g3e holds) and skim `BRIEF-G3b-composer.md`, `BRIEF-G3b2-steps-decks.md`, `BRIEF-G3c-copies.md`.

## Why
With `PLANAR_COMPOSER=tracks PLANAR_SLOTBEND=1`, T2 o1t3f1s0 solves at area 2621.25, 96x65, track 916, wasted 218, empty 1718 (MappedComposer: 862.25, 94x29, track 800, wasted 36, empty 159); T3 is base infeasible on all 32 variants. We need to see the layout and find which rows make it tall, and what blocks T3. Diagnosis only: no rule changes.

## Changes (probe-only, all behind environment variables, in `planar_components.cpp` and the probe test)
1. `PLANAR_DUMPSEGS=<path>`: at the end of `SolveComposedVariant`, for the accepted solution, append to that file per strand, per segment: index, heading, solved length, least length (the variable's lower bound), and whether it is optional. One header line per variant tag.
2. `PLANAR_NOCURVES=1`: `FinishPlanarModel` skips its `Composer::RouteCurves` call (measurement only).
3. The probe test `"planar probe: track composer slot bends"` additionally writes the solved layout's ASCII (`RenderJunctionAscii` on the layout the plan renders from the programs: find how an existing mapped T2 test or `planar_plan` turns a `PlanarComponentLayout` into a `JunctionLayout` and do the same) to the path in `JUNCTION_PROBE_ASCII` when set.

## Measure
1. T2 o1t3f1s0, tracks + slot bends: ASCII into `<S>/g3f-T2-tracks.txt`, segments into `<S>/g3f-T2-segs.txt`. The same ASCII for MappedComposer (no `PLANAR_COMPOSER`) into `<S>/g3f-T2-mapped.txt`.
2. Same variant with `PLANAR_NOCURVES=1` (tracks + slot bends): area, size, track, wasted, empty.
3. From the segments: list the 15 segments with the largest `solved - least`, by strand, heading and length, and say which run vertically (N/S, diagonals with a N/S part) on the tall side.
4. Look at the ASCII: describe in plain words where the empty area is (which lines run far apart, where, and which lines go out of their way), at most 10 lines.
5. T3 o0t1f1s1, tracks + slot bends, `PLANAR_IIS=1` into `<S>/g3f-T3-iis.txt`: the IIS rows grouped by strand, and for each group your reading of which composer rule or slot bend it comes from (name the S-bend if one is involved: line, D, rule 1 or 2).

## Do not
- Change any row, bound or rule, or the composer's behaviour without the new variables. Touch ports 4077/4079 or start a game client. Use git stash.

## Commit and report
- One commit on probe-g3e: `Probe: Junction track composer size diagnosis (PLANAR_DUMPSEGS, PLANAR_NOCURVES)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Report: references read; commit hash; measures 1-5 (paste both ASCII files' first 70 lines into the report only if each is at most 100 columns wide; otherwise give their paths); any stop; `git status --porcelain`; `git log -1`. Finish with `git switch wt-tracks` and the rebuild. When you have reported, stop.
