# BRIEF X2b: which of X2a's exact families breaks closure (probe, Sonnet)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/ng1, branch wt-slots, head {HEAD} (X2a, c63cec9015). Release build dir /home/pete/OpenTTD/.claude/ng1/build-rel.
- Implements: wip/SPEC-slots.md §7 X2 (diagnosis of X2a). X2a found closure fails on 0/64 variants with all three exact families (A1 cusps, A2 decks, A3 pins) together. This step finds which family, alone or in pairs, breaks it. A probe; no product code.

## Read first
- /home/pete/OpenTTD/docs/junction-tool/wip/BRIEF-X2a-closure-exact.md (what A1-A3 are).
- /home/pete/OpenTTD/.claude/ng1/src/tests/junction/planar/planar_shape_layer.cpp: `ClosureLp` and the X2a probe test (last TEST_CASE).

## Changes (src/tests/junction/planar/planar_shape_layer.cpp only)
1. `ClosureLp`'s `bool exact` becomes `int families` (default 0): bit 1 adds the A1 rows, bit 2 the A2 rows, bit 4 the A3 rows; the existing call sites pass 0 where they passed false; the X2a test passes 7 where it passed true.
2. In the X2a probe test, after its current output, for each hub and each mask in {1, 2, 4, 3, 5, 6} run `ClosureLp(v, mask)` on every shaped variant and print `X2B <hub> mask=<m> closes=<n>/<shaped>`.
3. For T1 only, for the first shaped variant, print for each mask in {1, 2, 4} that fails: `X2B T1 <tag> mask=<m> WHY <why>`.

## Run
- Build: `cmake --build /home/pete/OpenTTD/.claude/ng1/build-rel --target openttd_test -j16 > /home/pete/OpenTTD/.claude/ng1/build-rel/x2b-build.log 2>&1`; grep `error`.
- `cd /home/pete/OpenTTD/.claude/ng1/build-rel && timeout 900 ./openttd_test "planar probe: closure with Slots' least exact answer on T1-T3 and LL2" > x2b-run.txt 2>&1`; `grep -E "X2A .*shaped|X2B" x2b-run.txt`.
- `/home/pete/OpenTTD/tools/junction/jt fast /home/pete/OpenTTD/.claude/ng1/build-rel '[planarshape]' 10`.

## Do not
- Change any row of A1-A3, the free rows, or any other test or file; try to fix anything.

## Gates and commit
- `[planarshape]` fast green; X2A summary lines identical to X2a's (closes_free 16, closes_exact 0 per hub).
- One commit on wt-slots: `Add: Junction planar probe: X2a's exact families one by one (X2b)` ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report (short)
- Commit: hash and subject
- The grep output verbatim
- `[planarshape]` result
- Stops; state (`git status --porcelain`, `git log -1 --oneline`)
When you have reported, stop.
