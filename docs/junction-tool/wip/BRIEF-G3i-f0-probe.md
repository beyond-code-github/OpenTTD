# Brief G3i (probe, diagnosis): why unflipped (f0) variants and LL² are infeasible in the track composer

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, ON `probe-g3e` at 27c44a5fad (built). Stay on probe-g3e; commit only there. Build dir `build-rel`, target `openttd_test` only. Finish with `git switch wt-tracks` and rebuild `openttd_test` in build-rel.
- Time box: 90 minutes. One heavy job at a time. Each run under `timeout -s KILL 900`.
- S = `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad`.
- Read first (main worktree `/home/pete/OpenTTD/docs/junction-tool/`): STATUS.md's first bullet under "Where things are"; `wip/BRIEF-G3g-binding-probe.md` (the method and the classification).

## Why
With `PLANAR_COMPOSER=tracks PLANAR_SLOTBEND=1`: T2 22/32 (best 920), T3 14/32 (best 1929); all failures "base infeasible" are f0 variants, and all 12 LL² tags (`<S>/ll2-12.txt`, all f0) are base infeasible (`<S>/g3h-pv-LL2.txt`); `MappedComposer` solves LL² (best 2162.25). On T2 o1t3f0s0 the IIS is F2's deck over D2's first piece (just after the CD fray) with the CD and AF ports pinned 4 half-tiles apart (`<S>/orc-f0.txt`).

## Changes (probe test only)
- `JUNCTION_PROBE_HUB=LL2` takes `PlanarLL2Text()` in the probe test.

## Measure (each with `PLANAR_IIS=1 PLANAR_SHAPE=default`, output to `<S>/g3i-<hub>-<tag>.txt`; print the heading lists of the strands in the IIS, as G3g did)
1. T2 `o1t2f0s0` (MappedComposer solves it at 1117.25) and T3 `o0t1f0s1` (or the T3 f0 variant MappedComposer solves best, from `<S>/g3a-after-T3.txt`).
2. LL² `o0t1f0s1` (MappedComposer's best) and `o0t0f0s0`.
3. For each: the IIS groups by strand chain, the composer rule behind each group, and the class (a) heading list without independent spans, (b) a ruled row (which), (c) other. Then, for each (a) or (c), compare with how `MappedComposer` builds the same lines on that variant (read its build code for the site or channel involved; name the recipe piece that gives it room, e.g. an optional piece, a jog, a placement of the crossing on another piece) in one or two lines.

## Do not
- Change any composer row, rule, heading rule, the map or the solve. Touch ports 4077/4079 or start a game client. Use git stash.

## Commit and report
- One commit on probe-g3e: `Probe: Junction track composer probe test takes LL2`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Report: references read; commit hash; measures 1-3; any stop; `git status --porcelain`; `git log -1`. Finish with `git switch wt-tracks` and the rebuild. When you have reported, stop.
