# BRIEF X2h: do free-size steps at known places make every part stretchable? (probe, Sonnet)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/ng1, branch wt-slots, head {HEAD} (after G0 if it has committed; read `git log -3 --oneline` and work on top of whatever is there, never revert another commit). Release build dir /home/pete/OpenTTD/.claude/ng1/build-rel.
- Implements: wip/REVIEW-slots-lengths.md "## Follow-up: X2g" item 5 (option E's falsifier X2h). Read that section first. Option E: Slots fixes each step's existence, side and order; Lengths sizes it (at least 1 lane) with every length. X2g found most parts rigid with exact step sizes; E predicts that sign-fixed steps of FREE size, placed where X2f's fewest-steps answer puts them, make N empty.

## Changes (src/tests/junction/planar/planar_shape_layer.cpp only)
1. `ClosureLp` gains an optional parameter `const std::vector<std::tuple<uint32_t, uint32_t, int>> *step_columns = nullptr` (track, stretch, sign +1/-1). With bit 64 set and `step_columns` given: for each entry add one free column s >= 0 to that stretch's x and y rows with coefficient `-sign * s * Right(h)` (h: the stretch's first piece heading mod 8, as bit 8 does), and include s in the max-support objective like a part (its own t <= s, 0 <= t <= 1; a step that cannot be positive is reported as rigid with the prefix `step`).
2. To get the places: X2f's mask-63 solve already gives each nonzero step (track, stretch, n). Expose them from `ClosureResult` as a vector of (track index, stretch, n) alongside the existing `steps` strings (add the member `std::vector<std::tuple<uint32_t, uint32_t, int>> step_places;`).
3. In the probe test, per hub and shaped variant: run mask 63 (as X2f), take its `step_places` with sign = sign of n, then run families 71 with those as `step_columns`. Print per hub `X2H <hub> variants=<n> rigid_min=<a> rigid_max=<b> (X2g had <x2g max>)` and for the first variant of T1 and T2 every rigid entry `X2H <hub> <tag> RIGID <entry>`.

## Run
- Build as before; `cd /home/pete/OpenTTD/.claude/ng1/build-rel && timeout 1200 ./openttd_test "planar probe: closure with Slots' least exact answer on T1-T3 and LL2" > x2h-run.txt 2>&1`; `grep X2H x2h-run.txt > x2h.txt`.
- `/home/pete/OpenTTD/tools/junction/jt fast /home/pete/OpenTTD/.claude/ng1/build-rel '[planarshape]' 10`.

## Do not
- Change any other bit's rows or any other file; fix anything; interpret beyond the counts.

## Gates and commit
- `[planarshape]` fast green; earlier probe lines unchanged.
- Commit: `Add: Junction planar probe: X2h, free-size steps at X2f's places` ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report (short)
- Commit; x2h.txt verbatim (if over 100 lines, the summaries and the T1 RIGID lines); `[planarshape]` result; state.
When you have reported, stop.
