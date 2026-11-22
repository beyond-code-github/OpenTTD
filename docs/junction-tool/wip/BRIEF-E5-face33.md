# BRIEF: E5 LL2 face 33, the minimal infeasible core (exact deletion filter, read-out only)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: about 45 minutes in total. Each solve has a 15 s cap. Run the hidden test once (a second time only if the first crashed or printed nothing). No rerun loops, no tuning.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng1` (branch wt-e1), head HEAD_HASH, clean. Release build `/home/pete/OpenTTD/.claude/ng1/build-rel`.
- Previous step 856ca51153 (brief `/home/pete/OpenTTD/docs/junction-tool/wip/BRIEF-E5-certificates.md`): exact single-face solves (`pairwise`, `pairwise_all_faces`, `only_faces`, no `sides_from` so every pair gets binaries, inserts free, `coord_max` 256). LL2 face 33 alone is Infeasible, with no IIS tags (the LP relaxation is feasible, so the integer side choice is what fails). Face 33's walls: [0] D1#8 h1, [1] D1#9 h0, [2] D1#10 h1, [3] D1#11 h2, [4] D1#12 h1, [5] B1"9#0 h6, [6] B1#0 h6, [7] B1#1 h5, [8] B1#2 h6, [9] B1#3 h7, [10] B1#4 h6, [11] F1"0#0 h4.
- This step finds what makes it infeasible: which row family it needs and the minimal set of wall pairs whose separation cannot hold together.

## 2. Read first (once each)
- `git show 856ca51153 -- src/tests/junction/planar/planar_draw.cpp` (the certificate test, its options and `CertWallText`).
- `src/junction/planar/planar_draw.cpp`: the pairwise pair loop's skip rules before the binary branch (search `if (j == i + 1 || (i == 0 && j == n - 1)) continue;`).
- `src/junction/planar/planar_draw.h`: `PlanarDrawOptions` (`bays`, `curves`, `corridor`).

## 3. Changes
### 3.1 One option
- `PlanarDrawOptions`: `std::vector<std::string> skip_pairs;` doc `///< A measurement aid: in pairwise mode these pair tags ("sep <face> <i> <j>") get no separation rows (a deletion filter).` In the pair loop, right after the existing skip rules, `continue` when the pair's tag is in `skip_pairs`.

### 3.2 New hidden test "planar draw: LL2 face 33 infeasible core", tag `[.][planarcert]`
Base options exactly as the certificate test's LL2 {33} case (B 256). `ShapeFirst(PlanarLL2Text(), 1)`.
1. **Base:** solve once; WARN `face33 base: <status> ms <ms>`. If it is not Infeasible, WARN that and stop the test there (do not continue to 2-3).
2. **Families:** solve with (a) `bays = false`, (b) `curves = false`, (c) `corridor = false`, (d) all three false. WARN `face33 without <bays|curves|corridor|all three>: <status> ms <ms>`.
3. **Pair deletion filter (with the base options):** the candidate list is every (i, j) with i < j over face 33's 12 walls that the loop would not skip as adjacent (`j == i + 1` or `i == 0 && j == n - 1`), as tags `sep 33 i j`, in increasing (i, j) order. Let `S` = all of them. For each tag p in order: solve with `skip_pairs` = every candidate not in S, plus p. If the status is Infeasible, remove p from S (it is not needed); otherwise keep p. After the pass, S is a minimal set: WARN `face33 core size <|S|>` and per pair in S: `face33 core sep 33 i j: <wall i text> | <wall j text>` (`CertWallText`). Then solve once with only S kept (everything else skipped) and WARN its status (must be Infeasible) and ms.
4. If any solve reaches its time limit (status not Optimal and not Infeasible), WARN it with the tag; treat it as "not Infeasible" in the filter (keep p) and say so in the report.

## 4. Do not
- No change to any default or behaviour (`skip_pairs` empty by default). No interpretation beyond tabulating.

## 5. Stop conditions
- A gate fails for a reason you did not cause and cannot isolate in one rerun.

## 6. Gates and measurement
- `cd /home/pete/OpenTTD/.claude/ng1 && tools/junction/jt fast build-rel '[planardraw]' 5`; `tools/junction/jt gate build-rel` (30 s). Known flakes: "planar components: T2 mapped lands in its box at the anchor", "joint: a port fan's size follows the rows"; rerun alone and report.
- `build-rel/openttd_test "planar draw: LL2 face 33 infeasible core" > build-rel/face33.log 2>&1`; copy to `docs/junction-tool/wip/probes/e5land/face33-<short head hash before your commit>.log`, same commit.
- One commit: `Add: Junction: deletion filter on LL2 face 33 (E5 certificate)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. No merge or push.

## 7. Report (short, fixed format)
- Commits: hash and subject; files changed; members added.
- Suite results (count and time per gate).
- Base line; the four family lines; the core size, every core line verbatim and the core-only check; any time-limit lines. Join Catch's 80-column wraps first.
- Stops and anything left undone.
- State: `git status --porcelain` and `git log -1 --oneline`.

When you have reported, stop.
