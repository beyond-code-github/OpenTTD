# BRIEF G0: how many free degrees does a variant's geometry really have? (probe, Sonnet)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/ng1, branch wt-slots, head {HEAD} (X2g). Release build dir /home/pete/OpenTTD/.claude/ng1/build-rel.
- Implements: wip/SPEC-slots.md §7 (after X2g): sizing a joint geometry layer (steps and lengths in one solve). X2g found most parts' lengths are fixed by sideways quantities; if so, the real search space is small. This probe counts it.

## Changes (src/tests/junction/planar/planar_shape_layer.cpp only)
1. A static helper `static int ClosureRank(const PlanarShapeVariant &v, int families)` that builds the same equality rows `ClosureLp` builds for `families` (reuse `ClosureLp`'s row building: add a `std::vector<std::vector<std::pair<uint32_t, double>>> *equalities` out-parameter to `ClosureLp`, default nullptr, that receives every row added with lower == upper, and the variable count via a second out-parameter `uint32_t *columns`), then computes the rank of that equality matrix by Gaussian elimination in double with partial pivoting (tolerance 1e-9), returning the rank. Write it in the test file; no library.
2. In the probe test, for each hub and each shaped variant, for families 0, 7 and 31 (31 = exact rows plus whole-lane steps, no deck steps): columns = variable count, rank = equality rank, free = columns - rank. Print per hub `G0 <hub> fam=<f> columns=<min..max> rank=<min..max> free=<min..max> steps_vars=<count of step variables, fam 31 only>` and for the first variant of T2 and LL2 the same numbers on one line each.

## Run
- Build as before; `cd /home/pete/OpenTTD/.claude/ng1/build-rel && timeout 900 ./openttd_test "planar probe: closure with Slots' least exact answer on T1-T3 and LL2" > g0-run.txt 2>&1`; `grep -E "^G0" g0-run.txt`.
- `/home/pete/OpenTTD/tools/junction/jt fast /home/pete/OpenTTD/.claude/ng1/build-rel '[planarshape]' 10`.

## Do not
- Change any row or bit of `ClosureLp`, any other file; interpret beyond the numbers.

## Gates and commit
- `[planarshape]` fast green; the X2A-X2G lines unchanged.
- Commit: `Add: Junction planar probe: G0, the closure's free degrees` ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report (short)
- Commit; the G0 lines verbatim; `[planarshape]` result; state.
When you have reported, stop.
