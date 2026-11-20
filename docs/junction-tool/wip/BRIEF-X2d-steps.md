# BRIEF X2d: does some choice of sideways steps make the exact answer close? (probe, Sonnet)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/ng1, branch wt-slots, head {HEAD} (X2c). Release build dir /home/pete/OpenTTD/.claude/ng1/build-rel.
- Implements: wip/SPEC-slots.md §7 X2. X2a-X2c: with port lanes pinned one lane apart (A3), closure fails on T2 and T3, because the LP has no sideways steps: a port's lanes must fan (S-bends, Slots' steps, D313) before their fray's legs can cross. This step asks whether SOME step choice closes every variant (an existence test for Slots' answer, L9), before Slots is designed to find it.

## Read first
- /home/pete/OpenTTD/.claude/ng1/src/tests/junction/planar/planar_shape_layer.cpp: `ClosureLp` (with its `families` bits) and the last TEST_CASE.
- /home/pete/OpenTTD/.claude/ng1/src/junction/planar/planar_model.h lines 40-70 (`STEP`, `Right`).

## Changes (that test file only)
1. `ClosureLp` gains bit 8 of `families`: **steps**. For every track t and every stretch k of it (the same stretches its `stretch` rows sum), add one free continuous variable `s` (bounds -INF..+INF, cost 0) and put `- s * R[0]` into the stretch's x row and `- s * R[1]` into its y row, where R = `PlanarModel::Right(h)` and h is the heading (mod 8) of the stretch's first piece. (A step is a sideways move inside the stretch; its along-track part is absorbed by the free piece lengths.) Tag nothing new; the stretch rows keep their tags.
2. In the probe test, for each hub print `X2D <hub> mask=<m> closes=<n>/<shaped>` for masks 8, 12 (pins + steps), 9, 10, 15.
3. For T2 and T3, if mask 15 fails on a variant, print the first such `X2D <hub> <tag> mask=15 WHY <why>`.

## Run
- Build as in X2c; `cd /home/pete/OpenTTD/.claude/ng1/build-rel && timeout 900 ./openttd_test "planar probe: closure with Slots' least exact answer on T1-T3 and LL2" > x2d-run.txt 2>&1`; `grep -E "X2D" x2d-run.txt`.
- `/home/pete/OpenTTD/tools/junction/jt fast /home/pete/OpenTTD/.claude/ng1/build-rel '[planarshape]' 10`.

## Do not
- Change the rows of bits 1, 2, 4 or the free rows; any other file; try to fix anything.

## Gates and commit
- `[planarshape]` fast green; X2A, X2B and X2C lines unchanged.
- Commit: `Add: Junction planar probe: X2d, free sideways steps` ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report (short)
- Commit; the X2D lines verbatim (WHY lines condensed to their row-tag kinds and track names if over 20 lines); `[planarshape]` result; state.
When you have reported, stop.
