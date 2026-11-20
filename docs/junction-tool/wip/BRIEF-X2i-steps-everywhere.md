# BRIEF X2i: what stays rigid however many free-size steps Slots places? (probe, Sonnet)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/ng1, branch wt-slots, head {HEAD} (X2h). Release build dir /home/pete/OpenTTD/.claude/ng1/build-rel.
- Implements: the falsifier of option E (wip/REVIEW-slots-lengths.md, "Follow-up: X2g", item 5). X2h placed free-size steps only where X2f's fewest-steps answer put them, and rigid parts remained (T1 4, T2 49). This step gives Slots the most freedom E allows: a free-size step of each sign on EVERY stretch outside a deck. What stays rigid then is rigid under any step placement.

## Changes (src/tests/junction/planar/planar_shape_layer.cpp only)
1. In the probe test, per hub and shaped variant, build `step_columns` with two entries (sign +1 and -1) for every stretch of every track EXCEPT the stretches bit 16 already exempts (a separator strictly inside its span; a copy between its first and last event); run families 71 with them (bit 64's max-support, as X2h).
2. Print per hub `X2I <hub> variants=<n> rigid_min=<a> rigid_max=<b> (X2h had <x>, X2g had <y>)`, and for the first variant of T1, T2 and LL2 every rigid entry that is a part (skip `step` entries; count them instead: `X2I <hub> <tag> rigid_steps=<n>`) as `X2I <hub> <tag> RIGID <entry>`.

## Run
- Build as before; `cd /home/pete/OpenTTD/.claude/ng1/build-rel && timeout 1200 ./openttd_test "planar probe: closure with Slots' least exact answer on T1-T3 and LL2" > x2i-run.txt 2>&1`; `grep X2I x2i-run.txt > x2i.txt`.
- `/home/pete/OpenTTD/tools/junction/jt fast /home/pete/OpenTTD/.claude/ng1/build-rel '[planarshape]' 10`.

## Do not
- Change `ClosureLp` or any other file; fix anything; interpret beyond the counts.

## Gates and commit
- `[planarshape]` fast green.
- Commit: `Add: Junction planar probe: X2i, free-size steps on every stretch` ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report (short)
- Commit; x2i.txt verbatim (if over 120 lines: summaries, rigid_steps lines, and the T1 and T2 RIGID lines); `[planarshape]` result; state.
When you have reported, stop.
