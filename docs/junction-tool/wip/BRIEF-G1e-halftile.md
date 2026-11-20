# BRIEF G1e: print the half tile that blocks LL2 (probe, Sonnet)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/ng1, branch wt-slots, head {HEAD} (G1d, bb247cd870). Release `build-rel`.
- G1d (build-rel/g1d.txt): on LL2 o0t0f1m1 at off=511 the grid rows of A2a and B2"13 alone, with the E-side pins EO1-EO2 and EO2-AB2, are infeasible. G1e prints the geometry of the parity-free solution so the orchestrator can see which point lands off the grid.

## Changes (src/tests/junction/planar/planar_slots_probe.cpp only)
- A test `"planar probe: G1e the geometry around LL2's grid conflict"` (`[junction][planar][planarshape][.probe]`), LL2 first variant, one solve at off = 511|1024 (no lower bounds, no grid rows; integer).
- For the tracks EO1, EO2, AB1, AB2, A2a, B2"13 and every track that shares a node with A2a or B2"13 (one level: the other tracks at their start and end nodes), print one line per vertex of its solved vertex list (the list G1 builds for the geometry): `G1E <track> v<i> kind=<start|bend|step|event|end> x=<x> y=<y> x%2=<0|1> y%2=<0|1> (x+y)%2=<0|1> next_heading=<h> next_len=<units>`, with `node=<index>` on start and end vertices and `cross=<other track>` on events.
- After them, one line per G1 grid rule this solution breaks on those tracks: `G1E BREAK <track> v<i> rule=<start x+y odd | axis cross-coordinate odd | axis even>`.
- No commit gate beyond the build and this one test; then the Release gate.
- Commit: `Add: Junction planar probe: G1e, the geometry around LL2's grid conflict`.

## Run
- `cd /home/pete/OpenTTD/.claude/ng1/build-rel && timeout 300 ./openttd_test "planar probe: G1e the geometry around LL2's grid conflict" > g1e-run.txt 2>&1; grep '^G1E' g1e-run.txt > g1e.txt` (Release, alone); then `/home/pete/OpenTTD/tools/junction/jt gate /home/pete/OpenTTD/.claude/ng1/build-rel` (once, alone).

## Do not
- Change any existing row or parameter; fix anything; interpret beyond the printed lines; touch any other file.

## Stop conditions
- The solve at 511|1024 is not optimal (G1c found it optimal): report and stop.

## Report (short)
- Commit; gate (count, time; name any failing test and whether it passes alone); `g1e.txt` verbatim; `git status --porcelain`, `git log -1`.
When you have reported, stop.
