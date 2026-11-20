# BRIEF G1f: node stubs (D380), rerun G1, and T2's beside-versus-clear geometry (probe, Sonnet)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/ng1, branch wt-slots, head {HEAD} (G1e, c2083e7d63). Release `build-rel`.
- Read DECISIONS.md row D380 (/home/pete/OpenTTD/docs/junction-tool/DECISIONS.md, grep `| D380`): two axis legs of a node on a diagonal trunk cannot share its point; each leg of every node gets a trunk-heading stub of 0 or 1 unit, chosen by the solve. G1e showed it at LL2 node 18 (A2 SW -> A2a S, A2b W) and node 20.

## Changes (src/tests/junction/planar/planar_slots_probe.cpp only)
1. **Stubs:** for every node and every leg of it (out tracks of a split, in tracks of a merge, cusp legs included), one integer `stub` in [0, 1], tag `stub <node> <leg>`. The leg's end at the node is tied to the node point plus `stub * STEP[trunk heading]` (split: the leg's start point = node point + stub * STEP[h]; merge: the leg's end point = node point - stub * STEP[h], h the trunk's heading mod 8 at the node, i.e. the out track's first piece at a merge, the in track's last at a split) instead of to the node point itself. The leg's travel (node-leg row, bays, curves, coverage) counts the stub as part of the leg. The grid rows of the leg's first (last) piece read its start (end) point after the stub. Cost 1 per unit of stub (a tie-break against stubs that are not needed). Write it into the geometry as one extra segment of the trunk heading at the start (end) of the leg when stub = 1. `off` bit 8192 removes the stubs (stub fixed to 0).
2. **Beside residuals:** with `print_geometry`, also print for every beside row family-1 would write: `G1E BESIDE <beside track> piece=<j> need=<2s> got=<dot value in this solution>`.
3. **The test** "planar probe: G1 joint slots and lengths on T2 T3 LL2" runs as now (full solve, then the ablation for unsolved variants), with stubs on.
4. **The T2 geometry** in test "planar probe: G1e the geometry around LL2's grid conflict": add a second block for T2's first variant at off=1 (beside rows off, everything else on), printing G1E lines for the tracks B1, prio:B1, C2, E2, prio:E2, D1 and the tracks sharing a node with prio:B1 or prio:E2, then the BESIDE lines for prio:B1 and prio:E2. Keep the LL2 block (now with stubs; its BREAK lines should be gone).
- Commit: `Add: Junction planar probe: G1f, node stubs (D380) and T2's beside geometry`.

## Run (Release, one at a time)
- `cd /home/pete/OpenTTD/.claude/ng1/build-rel && timeout 1800 ./openttd_test "planar probe: G1 joint slots and lengths on T2 T3 LL2" > g1f-run.txt 2>&1; grep '^G1' g1f-run.txt > g1f.txt`
- `timeout 300 ./openttd_test "planar probe: G1e the geometry around LL2's grid conflict" > g1f-geo-run.txt 2>&1; grep '^G1E' g1f-geo-run.txt > g1f-geo.txt`
- `/home/pete/OpenTTD/tools/junction/jt gate /home/pete/OpenTTD/.claude/ng1/build-rel`

## Do not
- Change any other row; add clash, separation or frame rows; fix any infeasibility or clash; touch any other file.

## Stop conditions
- `consistency > 0` on a solved run (the stub must not break the vertex walk): report the first and stop.
- A node whose trunk heading is ambiguous (more than one trunk with different headings at the node): report it and stop.

## Report (short)
- Commit; gate (count, time); the `G1 ... SUMMARY` lines, every `G1 <hub> <tag>` line and every G1A line (why trimmed to 200 chars); `g1f-geo.txt` verbatim if under 120 lines, else its T2 block and any LL2 BREAK lines; `git status --porcelain`, `git log -1`.
When you have reported, stop.
