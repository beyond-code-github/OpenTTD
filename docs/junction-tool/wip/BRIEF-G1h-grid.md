# BRIEF G1h: the next grid conflict on LL2 and T3, named and printed (probe, Sonnet)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/ng1, branch wt-slots, head {HEAD} (G1g, a7502e1e76). Release `build-rel`.
- After G1g's grid fixes (step pieces on the grid), LL2 at off=511 is infeasible again and T3 o7t... at off=511 fails on 1 of 3 variants. G1d's deletion filter and G1e's geometry print found the last grid conflict (node 18, now handled by D380 stubs). G1h repeats them on the new model.

## Changes (src/tests/junction/planar/planar_slots_probe.cpp only)
- In test "planar probe: G1d the grid rows that block LL2": run the existing deletion filter (grid rows per track, off=511) on LL2's first variant AND on the T3 variant of G1g's top 3 that fails at off=511; print `G1D <hub> <tag> minimal=<n> tracks=<names>` and the `G1D TRACK` lines as now. The grid rows of a track now include its step-piece rows (`parity step`, `parity step vertex`): the set filter covers them with the track.
- Then for each minimal set, one solve at off = 511|1024 (no grid rows) with `print_geometry` on the tracks of the set plus every track sharing a node or a crossing with them, and a `G1E BREAK` line for every grid rule the solution breaks on those tracks (the audit's grid checks, as G1g wrote them, restricted to those tracks).
- Commit: `Add: Junction planar probe: G1h, the grid conflicts after G1g, named`.

## Run (Release, one at a time)
- `cd /home/pete/OpenTTD/.claude/ng1/build-rel && timeout 900 ./openttd_test "planar probe: G1d the grid rows that block LL2" > g1h-run.txt 2>&1; grep '^G1[DE]' g1h-run.txt > g1h.txt`
- `/home/pete/OpenTTD/tools/junction/jt gate /home/pete/OpenTTD/.claude/ng1/build-rel`

## Do not
- Change any row; fix anything; run the long G1 test; touch any other file.

## Stop conditions
- The full set is feasible for a hub (no conflict to name): report it and continue with the other hub.

## Report (short)
- Commit; gate (count, time); `g1h.txt` verbatim if under 150 lines, else the G1D lines and every BREAK line with the 3 G1E vertex lines before and after it; `git status --porcelain`, `git log -1`.
When you have reported, stop.
