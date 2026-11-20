# BRIEF G1g: audit G1's rows against its own solved geometry, fix the rows that disagree (probe, Sonnet)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/ng1, branch wt-slots, head {HEAD} (G1f, c36e6ce1b7). Release `build-rel`.
- The model is specified in /home/pete/OpenTTD/docs/junction-tool/wip/SPEC-slots-lengths.md §8 and in /home/pete/OpenTTD/docs/junction-tool/wip/BRIEF-G1-joint.md items 1-13, with D380's stubs (BRIEF-G1f-stubs.md item 1). Read those once.
- G1f's T2 geometry (build-rel/g1f-geo.txt, T2 block, off=1, solved Optimal with the grid rows on) breaks the model: B1 starts at node 13 (38,47) heading 0 (north) with x = 38 even (a north piece must have x odd); prio:B1's cusp at node 13 is a heading-7 piece of 1 unit (a cusp is at least one lane: 2 units). So some rows are not written where the spec says, or the vertex walk misreports. G1g finds and fixes it.

## Changes (src/tests/junction/planar/planar_slots_probe.cpp only)
1. **Audit:** a function `static std::vector<std::string> AuditG1(...)` (file-local; arguments as you need from inside `SolveG1`, called after the walk on every solved run) that checks the walked geometry, per family whose rows are on in that run's `off`:
   - grid (unless 1024): every start, end, bend and step vertex has x + y odd; every axis segment's cross coordinate (x for headings 0/4, y for 2/6) is odd along it; every axis segment between two bends (not split at events) has even length;
   - steps: each step segment has heading h +- 1 of its piece and length 2 (p + m) as solved; each cusp step has length >= 2 and lies on the map's side;
   - stubs: each leg's end at its node is the node point plus stub * STEP[trunk];
   - beside (unless 1): every beside row's dot equals 2 s;
   - span clear (unless 2), deck gap (unless 4), crossers (unless 16), node legs (unless 32): each bound as written in BRIEF-G1-joint items 7-9, measured on the geometry.
   Each breach: `G1G AUDIT <hub> <tag> off=<bits> family=<name> <track> v<i> <detail>`.
2. Run the audit on every solved run of the test "planar probe: G1 joint slots and lengths on T2 T3 LL2" and of the G1e test. Record the first output (the red evidence: the breaches found).
3. **Fix:** for each family with breaches, find why its rows do not hold on the solution (a row not written, written on the wrong variable or point, wrong sign or coefficient) and correct the row-writing code so that it matches the brief items above. Do not change what a row means; only make the code write what the brief says. Rerun until the audit prints nothing on every solved run. Name each fix (family, cause, line) in the report.
- Commit: `Fix: Junction planar probe: G1g, rows audited against the solved geometry`.

## Run (Release, one at a time)
- `cd /home/pete/OpenTTD/.claude/ng1/build-rel && timeout 1800 ./openttd_test "planar probe: G1 joint slots and lengths on T2 T3 LL2" > g1g-run.txt 2>&1; grep '^G1' g1g-run.txt > g1g.txt`
- `timeout 300 ./openttd_test "planar probe: G1e the geometry around LL2's grid conflict" > g1g-geo-run.txt 2>&1; grep '^G1' g1g-geo-run.txt > g1g-geo.txt`
- `/home/pete/OpenTTD/tools/junction/jt gate /home/pete/OpenTTD/.claude/ng1/build-rel`

## Do not
- Change the meaning of any row or add a row family; add clash, separation or frame rows; weaken or drop a row to silence the audit (that is a breach); touch any other file.

## Stop conditions
- A breach whose fix would change what the brief says a row means (the brief is wrong, not the code): report the case and stop.
- The audit itself disagrees with the brief's wording on what to check: stop and ask.

## Report (short)
- Commit; the first audit output (counts per hub and family, and the first 3 lines per family); each fix (family, cause, line); after the fixes, the `G1 ... SUMMARY` lines, every `G1 <hub> <tag>` line and every G1A line (why trimmed to 200 chars); the T2 block's BESIDE lines from g1g-geo.txt; gate (count, time); `git status --porcelain`, `git log -1`.
When you have reported, stop.
