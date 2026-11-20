# BRIEF X2e: X2d with whole-lane steps and no step inside a deck (probe, Sonnet)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/ng1, branch wt-slots, head {HEAD} (X2d). Release build dir /home/pete/OpenTTD/.claude/ng1/build-rel.
- Implements: wip/SPEC-slots.md §7 X2 and §2.2 (Decks: no step inside a deck; slots are whole lanes). X2d showed free real steps on every stretch close every variant; this step makes the steps what Slots may really choose.

## Read first
- /home/pete/OpenTTD/.claude/ng1/src/tests/junction/planar/planar_shape_layer.cpp: `ClosureLp` (bit 8) and the last TEST_CASE.

## Changes (that test file only)
1. `ClosureLp` gains bit 16 of `families`, which modifies bit 8's step variables:
   - **Whole lanes:** each step `s` becomes `lane * n` with n an integer variable (bounds -64..64): lane = 2 when the stretch's first piece heading is axis (even), 1 when diagonal. (So the LP becomes a MILP; keep `JunctionMilp`'s integer flag on n only.)
   - **No step inside a deck:** no step variable on the stretches of a structure's separator between its first and last crossing (stretches k with 1 <= k <= index of its last crossing event on the separator and k > index of its first crossing event, i.e. strictly between two of the structure's crossings), nor on the copy's stretches between its first and last event.
2. In the probe test, print `X2E <hub> mask=<m> closes=<n>/<shaped> ms_max=<t>` for masks 24 (steps whole, no deck steps) and 31 (all exact rows + those steps). For each hub, the first variant failing mask 31 prints `X2E <hub> <tag> mask=31 WHY <why>`.

## Run
- Build as before; `cd /home/pete/OpenTTD/.claude/ng1/build-rel && timeout 900 ./openttd_test "planar probe: closure with Slots' least exact answer on T1-T3 and LL2" > x2e-run.txt 2>&1`; `grep -E "X2E" x2e-run.txt`.
- `/home/pete/OpenTTD/tools/junction/jt fast /home/pete/OpenTTD/.claude/ng1/build-rel '[planarshape]' 10`.

## Do not
- Change any other row or bit, any other file; fix anything.

## Stop conditions
- A single MILP over 15 s (the solve budget): report the hub, tag and mask, and stop.

## Gates and commit
- `[planarshape]` fast green.
- Commit: `Add: Junction planar probe: X2e, whole-lane steps and no deck steps` ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report (short)
- Commit; the X2E lines verbatim (WHY lines condensed to row-tag kinds and track names if long); `[planarshape]` result; state.
When you have reported, stop.
