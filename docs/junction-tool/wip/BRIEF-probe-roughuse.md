# Probe: warm start and variant pruning by bound (DESIGN-tracks §10 items 1, 2)

Throwaway probe: nothing committed. Time box: 60 minutes; at the box, report what you have. Never use git stash, never start a game client, never touch ports 4077/4079 or running openttd processes; build only `openttd_test` in `build-rel`, never `openttd`. Outputs under `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/roughuse/`; code read with offset/limit; command output to files. If anything does not fit the code as described, stop and report; do not invent another rule.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-planar`, clean at `{HEAD}` (ladders landed: `SolveMappedVariant` does a rough `SolveLazy`, then up to two ladder passes). Release `build-rel`.
- Read: `/home/pete/OpenTTD/docs/junction-tool/DESIGN-tracks.md` §10; in `src/junction/planar/planar_components.cpp` `SolveMappedVariant`, `SolveLazy`, `SolveLazy1`, `SolvePlanarMapped`; in `planar_model.{h,cpp}` how the MILP is handed to HiGHS (`Solve` or similar).
- Cost today (pv, -j 6): median ms T2 92, T3 185, LL² 5590 (max 10.4 s); before ladders LL² 2495.

## Part A: warm start (env `PLANAR_WARM=1`)
Hand the ladder solve's HiGHS the previous solution (rough, or the previous pass) as a starting point (`Highs::setSolution` with the column values; the ladder model is the same variant's model plus rung rows, so columns should match index for index: check `num_col` equal and stop if not). Also each lazy round within `SolveLazy1` and the area pass may reuse the previous round's solution the same way; try (a) ladder pass only, (b) every re-solve. Report whether HiGHS accepts the start (its log line / return status) and the ms.

## Part B: bound per variant (env `PLANAR_BOUND=1`, measurement only)
For every variant, solve the rough model's LP relaxation (integers relaxed; same objective as the first `SolveLazy1` solve) and record its objective as area (same units as `out.area`) and its ms. Do not prune; just record bound vs final ladder area per variant.

## Runs
- T2, T3 full sets, LL² on the 201 tags in `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/ll2-tags.txt` (use `/home/pete/OpenTTD/tools/junction/pv run`, `-j 6`, with env set; pv passes the environment).
- A: per hub, median and max ms for off / (a) / (b); areas must be identical to off (list any differing).
- B: per hub, LP ms median/max; for each variant bound vs final area; is bound <= final always (list violations)? How many variants would be pruned if solved in ascending bound order, skipping any whose bound >= the best final found so far (simulate offline from the table), and the total ms saved vs solving all.

## Report (findings under 35 lines in `/home/pete/OpenTTD/docs/junction-tool/wip/PROBE-roughuse.md`)
Then `git diff > /home/pete/OpenTTD/docs/junction-tool/wip/probe-roughuse.patch`, `git checkout -- .`, `git status --porcelain` empty, rebuild `openttd_test`. Report in under 12 lines. When you have reported, stop.
