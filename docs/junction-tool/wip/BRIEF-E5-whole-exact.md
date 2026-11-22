# BRIEF: E5 whole-hub exact keep-apart with a tight bound (measurement, read-out only)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: about 40 minutes in total. Each exact solve has a 15 s cap (the solve budget; do not raise it). Run the hidden test once. No rerun loops, no tuning.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng1` (branch wt-e1), head HEAD_HASH, clean. Release build `/home/pete/OpenTTD/.claude/ng1/build-rel`.
- Context: the certificate test ("planar draw: crowding certificates on T3 and LL2") solved single faces exactly (every keep-apart pair binary, inserts free, `coord_max` 256) in 0.3-3 s. Earlier whole-hub exact solves stalled, all with the loose bound (4096, big-M 16384). This step measures whole hubs exactly with a tight bound taken from each hub's own pipeline drawing, so the bound cannot cut off a solution the pipeline already has room for.

## 2. Read first (once each)
- The certificate test in `src/tests/junction/planar/planar_draw.cpp` (its exact options, `CertWallText`), and the hub pipeline test (its hub list and decode).
- `src/junction/planar/planar_draw.h`: `coord_max`, `only_faces`, `PlanarDrawResult` (`status`, `ms`, `binaries`, `points`).

## 3. Change: one new hidden test "planar draw: whole-hub exact keep-apart", tag `[.][planarcert]`
No production code change.
- Hubs: T1 (`PlanarTText(1)`), T2, T3, LL2 (`PlanarLL2Text()`), and PSG 197 as the 197 pipeline test builds it (same case, same shape; copy how that test builds its variant).
- Per hub:
  1. `DrawPlanarPipeline` with defaults (`final_seconds = 5`), only to read its last round's points: let `P` = the largest coordinate over all points of `r.last_round.points`; `B` = 2 * P rounded up to a multiple of 16.
  2. Exact solve: the certificate test's options with `only_faces` empty (every face) and `coord_max = B`, 15 s.
  3. WARN `whole <hub> B <B>: <status> ms <ms> binaries <binaries>`. On Infeasible, WARN the conflict tags if any (up to 40), decoded for sep tags as the certificate test does.
  4. On Optimal, WARN the drawing's box if derivable (max minus min of the points per axis) and the sum of piece lengths if the result has it.

## 4. Do not
- No production change, no default change, no raised time limit, no interpretation.

## 5. Stop conditions
- The 197 variant cannot be built in this test the way its pipeline test builds it: skip 197 and say so (no stop).
- A gate fails for a reason you did not cause and cannot isolate in one rerun.

## 6. Gates and measurement
- `cd /home/pete/OpenTTD/.claude/ng1 && tools/junction/jt fast build-rel '[planardraw]' 5`.
- `build-rel/openttd_test "planar draw: whole-hub exact keep-apart" > build-rel/whole-exact.log 2>&1`; copy to `docs/junction-tool/wip/probes/e5land/whole-exact-<short head hash before your commit>.log`, same commit.
- One commit: `Add: Junction: whole-hub exact keep-apart with a tight bound, a measurement (E5)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. No merge or push.

## 7. Report (short, fixed format)
- Commits: hash and subject; files changed.
- Suite result.
- One line per hub: B, status, ms, binaries; for Optimal the box and length; for Infeasible the conflict lines.
- Stops and anything left undone. State: `git status --porcelain` and `git log -1 --oneline`.

When you have reported, stop.
