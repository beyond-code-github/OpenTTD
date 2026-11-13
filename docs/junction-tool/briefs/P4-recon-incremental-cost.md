# P4 (recon by experiment): an incremental exact_cost for the level solve's descent (LL_RR near search still up to ~1.5 s; P3b)

Recon by experiment: change code in your worktree to measure, commit nothing; save the experiment as a patch in /tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/p4.patch and leave the worktree clean. Keep your context small. Never use git stash. Never start a game client or touch the engineer's game or rig servers. Mark anything not measured as INFERRED.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes2 (Debug `build`, Release `build-rel`). First: `git -C /home/pete/OpenTTD/.claude/nodes2 checkout -B wt2-p4 <HEAD>` (given at launch). Other agents work in /home/pete/OpenTTD/.claude/nodes and .claude/ng1: never touch them. Use absolute paths.
- P3/P3b (docs/junction-tool/RECON-llrr-time.md; merged 2133c850a7): the LL_RR level solve (532 tiles, 92 groups, 3321 cone corners) spends 95% in the coordinate descent calling exact_cost (src/junction_levels.cpp) ~580 times per solve; P3b made the raised-vs-lowered pair loop prefiltered and cached the held levels' cost: ~9.5 ms planned / ~11.5 ms failing per solve; HOVERNEAR_LLRR mean 131 ms, max 1463 ms (temperate-mountainous-5: 79 level solves); the T near max ~74 ms. Identity hashes in `[identity]`: T LEVELHASH (planned 2919, hash 16256945156918441187) and the LL_RR hash (planned 1180, hash 11818222337964832513).

## Experiments
1. Profile exact_cost now (after P3b): where its time goes per call (cone propagation, clamp, foundation checks, violation counting), with file:line.
2. Design and try an incremental exact_cost: when one group's level changes, only the corners whose L/U bounds can change (the cone of that group's corners, bounded by the distance where the change dies out) are recomputed, and the total cost/violations are updated by difference. State the invariant that makes it exact (identical to the full recompute) and check it with an assert against the full computation on every call during the sweeps (then remove the assert for timing).
3. Numbers: both identity hashes unchanged; per-solve ms (T, LL_RR planned/failing); HOVERNEAR and HOVERNEAR_LLRR mean/max; how many LL_RR fixtures remain over 100 ms and why.
4. If incremental is not exact or not faster, say so and why; list the next lever (cap on level solves per near search, D197-style time budget: a decision for the engineer, as a card).

## Output
docs/junction-tool/RECON-incremental-cost.md (untracked, ~50 lines) and the patch path.

## Report
10 lines: the profile, whether the incremental cost is exact, the speed-up, the hover numbers, what remains. `git -C /home/pete/OpenTTD/.claude/nodes2 status --porcelain` (clean). When you have reported, stop.
