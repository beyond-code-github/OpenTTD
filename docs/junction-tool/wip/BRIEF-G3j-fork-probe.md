# Brief G3j (probe on probe-g3e): a fork keeps one straight leg, read from the slot layer

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1` (on `wt-tracks`). Run `git switch probe-g3e` (at d5040c6f73). Commit only there. Build dir `build-rel`, target `openttd_test` only. Finish with `git switch wt-tracks` and rebuild `openttd_test` in build-rel.
- Time box: 90 minutes. One heavy job at a time. Each run under `timeout -s KILL 1800`.
- S = `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad`.
- Read first (main worktree `/home/pete/OpenTTD/docs/junction-tool/`): STATUS.md first bullet; `wip/BRIEF-G3i-f0-probe.md`; DECISIONS D340.

## Why
Every LL² tag is base infeasible in the track composer (`<S>/g3i-LL2-o0t1f0s1.txt`): the E2 lane split is a fork (no straight leg), so `BuildPlanarLineHeadings` rule 2 gives both legs a forced diagonal (left -1, right +1), and the merges after them add two more, four one-way spreads in series between EO ports pinned 2 apart. `MappedComposer` lets one fork leg run straight (optional diagonals, the solver picks). D340 forbids optional bends, so the straight leg must be decided before the solve. The slot layer (`BuildPlanarSlots`) may already decide it.

## Step 1: dump, then decide (stop if the data is not there)
For LL² `o0t1f0s1`, print every fork node (a split whose `node_legs` has no straight leg): its in line, its legs, the in line's slot in its channel's last interval, each leg's slot in its channel's first interval (in the slot layer's frame: use the links or the channel offsets as `NetStep` in `src/tests/junction/planar/planar_slots.cpp` does), and any `PlanarSlotLink` whose `from` is the in line. Into `<S>/g3j-forks.txt`.
- If exactly one leg per fork has a link from the in line with step 0, or sits on the in line's slot: go on with step 2 using that leg.
- Otherwise: stop and report the dump (do not invent a choice).

## Step 2: the rule (behind `PLANAR_FORKSLOT=1`, in `BuildPlanarLineHeadings`' caller only)
In `TrackComposer`, before the line loop: for each fork node with a slot-straight leg as found in step 1, that leg's working heading list is its base list (no leading diagonal: undo rule 2 for it, shifting its crossing pieces back), and the other leg keeps rule 2's diagonal. The merges are unchanged. Add the same as a check to the probe test's output (which forks got a straight leg).

## Measure
1. LL² 12 tags: `PLANAR_COMPOSER=tracks PLANAR_SLOTBEND=1 PLANAR_FORKSLOT=1 tools/junction/pv run build-rel LL2 --tags <S>/ll2-12.txt --out <S>/g3j-pv-LL2.txt`: ok / failed by reason, best (area, size, track, wasted, empty, ms), largest ms; beside `<S>/lo1-pv-LL2-after.txt`.
2. T2, T3 with the same variables: any variant changed against `<S>/g3h-pv-T2.txt` / `-T3.txt` (T2 and T3 have no forks, so expect none; list any).
3. If LL² is still infeasible: `PLANAR_IIS=1 PLANAR_SHAPE=default` on `o0t1f0s1` via the probe test (`JUNCTION_PROBE_HUB=LL2`), groups classified as in G3g.

## Do not
- Add a variant bit or an optional piece. Change the slot layer, the map, G2's rules for anything but forks, the solve or `MappedComposer`. Touch ports 4077/4079 or start a game client. Use git stash.

## Commit and report
- One commit on probe-g3e: `Probe: Junction a fork keeps the leg the slot layer keeps straight (PLANAR_FORKSLOT)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>` (or, on the step-1 stop, no commit).
- Report: references read; commit hash; the fork dump; measures 1-3; any stop; `git status --porcelain`; `git log -1`. Finish with `git switch wt-tracks` and the rebuild. When you have reported, stop.
