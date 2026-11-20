# BRIEF X2j: the fewest step places that leave nothing rigid (probe, Sonnet)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/ng1, branch wt-slots, head {HEAD} (X2i). Release build dir /home/pete/OpenTTD/.claude/ng1/build-rel.
- Implements: option E's placement question (wip/REVIEW-slots-lengths.md, "Follow-up: X2g" item 5). X2i: with a free-size step of each sign on every stretch outside a deck (bit 64 max-support, families 71), nothing is rigid on T1, T2, T3, LL2. Option E needs a real placement: each step has one sign. This probe finds, per variant, a small set of (track, stretch, sign) places that still leaves nothing rigid.

## Changes (src/tests/junction/planar/planar_shape_layer.cpp only)
1. In the probe test, per hub and shaped variant: start from X2i's full list of `step_columns` (both signs on every non-exempt stretch). Greedy pruning: for each entry in a fixed order (by track index, then stretch, then sign -1 before +1), remove it, run families 71 with the remaining list; if the result has any rigid entry (part or step), put the entry back; otherwise leave it removed. The result is a minimal set (no single entry can be dropped).
2. For each kept place, classify its context: `port` (the stretch starts or ends at a port), `node` (starts or ends at a node, not a port), `crossing` (between two crossings), `deck-adjacent` (one end is a crossing of a structure the track crosses or separates). Print per hub `X2J <hub> places_min=<a> places_max=<b> lps_max=<number of LP solves for the slowest variant> ms_max=<pruning time of the slowest variant>`, a tally over all variants `X2J <hub> TALLY <context> <count>`, and for the first variant of T1, T2 and LL2 each kept place: `X2J <hub> <tag> PLACE <track name> stretch=<k> sign=<s> heading=<h mod 8> from=<...> to=<...>` (from/to as X2f prints them).

## Run
- Build as before; `cd /home/pete/OpenTTD/.claude/ng1/build-rel && timeout 2400 ./openttd_test "planar probe: closure with Slots' least exact answer on T1-T3 and LL2" > x2j-run.txt 2>&1`; `grep X2J x2j-run.txt > x2j.txt`.
- `/home/pete/OpenTTD/tools/junction/jt fast /home/pete/OpenTTD/.claude/ng1/build-rel '[planarshape]' 10`.

## Do not
- Change `ClosureLp` or any other file; fix anything; interpret beyond the counts and tally.

## Stop conditions
- The run exceeds 2400 s: report what printed and stop.

## Gates and commit
- `[planarshape]` fast green.
- Commit: `Add: Junction planar probe: X2j, the fewest step places` ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report (short)
- Commit; x2j.txt verbatim (if over 120 lines: summaries, TALLY lines, and the T1 and T2 PLACE lines); `[planarshape]` result; state.
When you have reported, stop.
