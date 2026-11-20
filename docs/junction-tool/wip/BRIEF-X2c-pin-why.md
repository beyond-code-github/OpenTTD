# BRIEF X2c: the conflicting rows when pins alone break closure (probe, Sonnet)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/ng1, branch wt-slots, head {HEAD} (X2b, 1d1e0d2cca). Release build dir /home/pete/OpenTTD/.claude/ng1/build-rel.
- Implements: wip/SPEC-slots.md §7 X2 (diagnosis). X2b: the pin rows alone (mask 4) close 0/16 on T2 and T3, 16/16 on T1. We need to read the conflict.

## Read first
- /home/pete/OpenTTD/.claude/ng1/src/tests/junction/planar/planar_shape_layer.cpp: `ClosureLp` and the last TEST_CASE.

## Changes (that test file only)
1. In `ClosureLp`, make each pin row's tag name both tracks: `pin <side> <name of t1> <name of t2> d=<d>` (track names from `map.tracks[t].name`), and each `stretch`, `piece`, `node`, `cusp`, `cross`, `port` tag append the track's name (for `node` and `cusp`: the node index plus the names of its trunk and legs).
2. In the probe test, for T2 and T3, for the first shaped variant, print `X2C <hub> <tag> mask=4 WHY <why>` when mask 4 fails. Also print once per hub the port-side layout: for each `map.expanded.ring` entry in order: `X2C <hub> RING <index> <track name> side=<N|E|S|W> port=<port index> <in|out> heading=<port heading>`.
3. Print, for the same variant, each pinned port-line track's heading list: `X2C <hub> TRACK <name> pieces=<list> events=<n>`.

## Run
- Build as in X2b; run `cd /home/pete/OpenTTD/.claude/ng1/build-rel && timeout 900 ./openttd_test "planar probe: closure with Slots' least exact answer on T1-T3 and LL2" > x2c-run.txt 2>&1`; `grep X2C x2c-run.txt > x2c.txt`.
- `/home/pete/OpenTTD/tools/junction/jt fast /home/pete/OpenTTD/.claude/ng1/build-rel '[planarshape]' 10`.

## Do not
- Change any row, its bounds, or any other file; try to fix anything; interpret beyond reporting.

## Gates and commit
- `[planarshape]` fast green; the X2A and X2B lines unchanged.
- Commit: `Add: Junction planar probe: X2c names the pin conflict` ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report (short)
- Commit; the full content of /home/pete/OpenTTD/.claude/ng1/build-rel/x2c.txt verbatim (it is short; if over 150 lines, the T2 lines only); `[planarshape]` result; state.
When you have reported, stop.
