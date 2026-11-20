# BRIEF X2g: which parts cannot be stretched once Slots' exact set is fixed? (probe, Sonnet)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/ng1, branch wt-slots, head {HEAD} (X2f). Release build dir /home/pete/OpenTTD/.claude/ng1/build-rel.
- Implements: wip/REVIEW-slots-lengths.md "Suggested tests" 1 (X2g, the falsifier of option D) for SPEC-slots §5 Q7. Read that section and the review's F2 first.

## The idea (from the review)
By Stiemke's lemma, the lengths LP closes for every lower bound exactly when the homogeneous closure (every exact quantity set to 0) has a solution with every free part strictly positive. The parts that are 0 in every homogeneous solution form N, "the parts that cannot be stretched". A max-support LP finds them: maximise the sum of t_i with t_i <= part_i, 0 <= t_i <= 1; parts with t_i < 1 at the optimum are in N.

## Changes (src/tests/junction/planar/planar_shape_layer.cpp only)
1. `ClosureLp` gains bit 64 (used with bits 1, 2, 4; not with 8, 16, 32):
   - Homogeneous: every constant 0: cusp w fixed to 0 (bit 1), each deck row's right-hand side 0 (bit 2), each pin row's d = 0 (bit 4); no steps. The width and height variables keep lower bound 0 instead of 1; points keep lower bound 0 is fine (they are free in sign? no: keep as they are). Every part's lower bound becomes 0 (instead of the piece total >= 1).
   - Exempt from the objective: the parts of a structure's separator strictly between its first and last crossing, and of a copy between its first and last event (they are exact by rule).
   - For every other part, add t (0 <= t <= 1, objective +1 maximised, i.e. cost -1) and the row t - part <= 0.
   - Remove the width and height objective terms under bit 64.
   - `ClosureResult` gains `std::vector<std::string> rigid;`: after an optimal solve, one entry per non-exempt part with t < 0.5: `<track name> stretch=<k> piece=<j> heading=<h mod 8> from=<...> to=<...>` (from/to as X2f prints them).
2. In the probe test, for each hub run families 64|7 (= 71) on every shaped variant; print `X2G <hub> variants=<n> rigid_min=<a> rigid_max=<b> parts_total=<c>` (c: non-exempt parts of the first variant), then for the first variant every `X2G <hub> <tag> RIGID <entry>`, and over all variants a tally `X2G <hub> TALLY <track name> <count of variants where any of its parts is rigid>`. Also run 64 alone (no exact families; expected rigid 0 everywhere, which reproduces X2a's closes_free) and print `X2G <hub> mask=64 rigid_max=<n>`.

## Run
- Build as before; `cd /home/pete/OpenTTD/.claude/ng1/build-rel && timeout 900 ./openttd_test "planar probe: closure with Slots' least exact answer on T1-T3 and LL2" > x2g-run.txt 2>&1`; `grep X2G x2g-run.txt > x2g.txt`.
- `/home/pete/OpenTTD/tools/junction/jt fast /home/pete/OpenTTD/.claude/ng1/build-rel '[planarshape]' 10`.

## Do not
- Change any other bit's rows or any other file; fix anything; interpret beyond the tally.

## Stop conditions
- mask 64 alone gives rigid > 0 on any variant (it must reproduce X2a's free closure): report the first such variant's RIGID lines and stop.

## Gates and commit
- `[planarshape]` fast green.
- Commit: `Add: Junction planar probe: X2g, the parts that cannot be stretched` ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report (short)
- Commit; /home/pete/OpenTTD/.claude/ng1/build-rel/x2g.txt verbatim (if over 150 lines: summaries, mask=64 lines, TALLY lines, and the T1 and T2 RIGID lines); `[planarshape]` result; state.
When you have reported, stop.
