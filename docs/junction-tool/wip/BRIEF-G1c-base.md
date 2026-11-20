# BRIEF G1c: why G1's base model is infeasible on LL2; clash groups of solved runs (probe, Sonnet)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/ng1, branch wt-slots, head {HEAD} (G1b, 5e4eb6b031). Release `build-rel`.
- G1b result (build-rel/g1b.txt): with all nine lower-bound families off (off=511) LL2 is still infeasible on all 3 variants with no IIS tags; T2 and T3 solve at off=511. X2e (wip/SPEC-slots.md §7) closed every LL2 variant with free whole-lane steps on every stretch. G1's base differs from X2e in: (a) one step place per stretch on a named piece; (b) step travel (the diagonal piece moves along as well as sideways); (c) integer points with parity and axis evenness; (d) cusps as anchored steps of fixed side and size >= 1. G1c finds which.

## Changes (src/tests/junction/planar/planar_slots_probe.cpp only)
1. `SolveG1` gains three more `off` bits: 512 relax every variable to continuous (the LP; parity/evenness auxiliaries included); 1024 drop the parity and evenness rows (items 2's x + y odd, cross-coordinate odd, axis even); 2048 step places on every piece of every stretch not in a span (each piece of the stretch gets its own place, as item 4 builds one; cusp places unchanged); 4096 steps without travel: a step adds `(p - m) * lane * Right(h)` (lane 2 on an axis heading, 1 on a diagonal; exactly `ClosureLp`'s bit 16 term, planar_shape_layer.cpp around line 400) instead of `2 p STEP[h + 1] + 2 m STEP[h - 1]`.
2. After every solved run (any `off`), print the clash groups as G1 does for the full solve: `same_channel=<n> face=<n> noface=<n>` on the G1A line, plus the first 3 hits of each group as `G1C ...` lines with `off=<bits>` added.
3. The test: for LL2 only, per variant, run off = 511, 511|512, 511|1024, 511|2048, 511|4096, 511|1024|2048|4096, and 511|512|1024|2048|4096; print each as a G1A line (`why` up to 300 chars). For T2, first variant only, run off = 1, 2 and 511 again so their clash groups print. Skip T3. Keep the existing full runs (off 0) as they are.
- Commit: `Add: Junction planar probe: G1c, the base model's ablation on LL2`.

## Run
- `cd /home/pete/OpenTTD/.claude/ng1/build-rel && timeout 1800 ./openttd_test "planar probe: G1 joint slots and lengths on T2 T3 LL2" > g1c-run.txt 2>&1; grep '^G1' g1c-run.txt > g1c.txt` (Release, alone).
- Release gate after the commit: `/home/pete/OpenTTD/tools/junction/jt gate /home/pete/OpenTTD/.claude/ng1/build-rel`.

## Do not
- Change any existing row or family; fix an infeasibility; add clash, separation or frame rows; touch any other file.

## Stop conditions
- A STOP throw, or `consistency > 0` on a solved run: report and stop.
- The gate fails in a test you did not touch: rerun it alone once; passes alone: report as a flake and continue; else stop.

## Report (short)
- Commit; gate (count, time); every G1A line for LL2 and T2 verbatim; the G1C lines (at most 3 per group per run); `git status --porcelain`, `git log -1`.
When you have reported, stop.
