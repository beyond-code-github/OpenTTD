# BRIEF X2f: where do the fewest whole-lane steps go? (probe, Sonnet)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/ng1, branch wt-slots, head {HEAD} (X2e). Release build dir /home/pete/OpenTTD/.claude/ng1/build-rel.
- Implements: wip/SPEC-slots.md §7 X2 and §2 (what Slots decides: steps). X2e showed whole-lane steps (none inside a deck) close every variant with every exact row (mask 31). This step asks the fewest steps that do, and where they sit, so that Slots' lengths-free rule can be designed from evidence.

## Read first
- /home/pete/OpenTTD/.claude/ng1/src/tests/junction/planar/planar_shape_layer.cpp: `ClosureLp` (bits 1-16) and the last TEST_CASE.

## Changes (that test file only)
1. `ClosureLp` gains bit 32: with bits 8 and 16 set, each step's integer n is written as n = p - m with p, m integer in 0..64, and the objective gains 1000 * (p + m) per step (the existing width and height costs stay). `ClosureResult` gains `std::vector<std::string> steps;`: after an optimal solve, one entry per step with n != 0: `<track name> stretch=<k> n=<n> heading=<first piece heading mod 8> from=<what starts the stretch> to=<what ends it>`, where from/to is `port <side>`, `node <index> <split|merge>`, or `cross <other track name> <sep|crosser>` (sep when this track is the separator of a structure listing that crossing, or the copy deck of one), and the number of steps total.
2. In the probe test, for each hub run mask 63 on every shaped variant and print `X2F <hub> closes=<n>/<shaped> steps_min=<a> steps_max=<b> lanes_sum_min=<c> ms_max=<t>` (steps = count of nonzero n; lanes_sum = sum |n|). For the first shaped variant of each hub print every step line: `X2F <hub> <tag> STEP <entry>`. Also print, over all variants of the hub, a tally of steps by (from kind, to kind) pair: `X2F <hub> TALLY <from kind>-><to kind> <count>` (kind = port, split, merge, cross-sep, cross-crosser).

## Run
- Build as before; `cd /home/pete/OpenTTD/.claude/ng1/build-rel && timeout 1200 ./openttd_test "planar probe: closure with Slots' least exact answer on T1-T3 and LL2" > x2f-run.txt 2>&1`; `grep X2F x2f-run.txt > x2f.txt`.
- `/home/pete/OpenTTD/tools/junction/jt fast /home/pete/OpenTTD/.claude/ng1/build-rel '[planarshape]' 10`.

## Do not
- Change any other row or bit or file; fix anything; interpret beyond the tally.

## Stop conditions
- One solve over 30 s: report hub, tag, and stop. (The MILP is a probe, not Slots' method.)

## Gates and commit
- `[planarshape]` fast green.
- Commit: `Add: Junction planar probe: X2f, the fewest whole-lane steps` ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report (short)
- Commit; the content of /home/pete/OpenTTD/.claude/ng1/build-rel/x2f.txt verbatim (if over 150 lines: all summary and TALLY lines, and the STEP lines of T2 and LL2 only); `[planarshape]` result; state.
When you have reported, stop.
