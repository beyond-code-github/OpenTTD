# P5 (recon by experiment): the LL_RR near search on fixtures where every offset fails in placement (~0.7 ms x 153 = 100-120 ms)

Recon by experiment: change code in your worktree to measure, commit nothing; save experiments as a patch in /tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/p5.patch and leave the worktree clean. Keep your context small. Never use git stash. Never start a game client or touch the engineer's game or rig servers. Mark anything not measured as INFERRED.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes2 (Debug `build`, Release `build-rel`). First: `git -C /home/pete/OpenTTD/.claude/nodes2 checkout -B wt2-p5 <HEAD>` (given at launch). Other agents work in /home/pete/OpenTTD/.claude/nodes (order sweep) and .claude/ng1 (tunnel placement): never touch them. Use absolute paths.
- After P4b (d104e683a7): HOVERNEAR_LLRR mean 93 ms, max 574 ms; 16 of the 20 LL_RR fixtures still over 100 ms never reach a level solve: every one of their 153 offsets fails in the facts placement at ~0.7 ms (P3/P4: build 0.3-0.5 ms, X solve 0.3-0.45 ms; P2's JunctionPlacementShared already shares the flat solve and Y). Failures are mostly blocked tiles (water, houses) under ports and rows (N1/E5 for the T).

## Experiments (Release, LL_RR built topology, the 48 fixtures x 153 offsets)
1. For the failing offsets: the first reason classes (blocked tile, height step, contradiction...) and at what point in the facts placement the failure becomes certain (before the X solve? after which group?).
2. A sound early reject that is cheap: e.g. a precomputed per-offset check of the tiles that are fixed relative to the offset regardless of the X solve (port tiles, row tiles whose columns the shared Y/flat solve fixes within a range: a tile blocked for EVERY column in a point's range rejects); or reusing P2's shared results more (facts block prefiltering by row). Prove soundness from the code or show no reject where the full placement succeeds over all 7344 offsets (a counterexample = report).
3. Identity-preserving speed-ups of the failing path (reasons formatted lazily, the facts block's allocations, building the X system incrementally from the shared Y).
4. Numbers with the best combination: HOVERNEAR_LLRR mean/max, fixtures over 100 ms; T HOVERNEAR unchanged or better; both identity hashes unchanged (`tools/junction/jt identity build-rel`).

## Output
docs/junction-tool/RECON-llrr-placement-fail.md (untracked, ~50 lines) and the patch path.

## Report
10 lines: failure classes, the early reject (soundness, reject rate, cost), speed-ups, numbers, what remains. `git -C /home/pete/OpenTTD/.claude/nodes2 status --porcelain` (clean). When you have reported, stop.
