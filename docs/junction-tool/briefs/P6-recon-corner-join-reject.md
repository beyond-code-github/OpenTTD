# P6 (recon by experiment): the last slow LL_RR hovers: offsets that place but whose corners cannot be joined by one terraform (~6 ms each)

Recon by experiment: change code in your worktree to measure, commit nothing; save experiments as a patch in /tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/p6.patch and leave the worktree clean. Keep your context small. Never use git stash. Never start a game client or touch the engineer's game or rig servers. Mark anything not measured as INFERRED.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes2 (Debug `build`, Release `build-rel`). First: `git -C /home/pete/OpenTTD/.claude/nodes2 checkout -B wt2-p6 <HEAD>` (given at launch). Other agents work in /home/pete/OpenTTD/.claude/nodes (S-bend model) and .claude/ng1 (tunnel levels): never touch them. Use absolute paths.
- After P5b (724daf0a63): HOVERNEAR_LLRR mean 35 ms, max 541 ms; 3 fixtures over 100 ms (arctic-hilly-13 156 ms, temperate-mountainous-3 426 ms, temperate-mountainous-5 535 ms: 23, 70, 79 full placements). P5 (docs/junction-tool/RECON-llrr-placement-fail.md): 571 LL_RR offsets pass the X solve but fail later, 502 of them "corners ... cannot be joined by one terraform" at ~3-6 ms each (the level solve runs to find out). LS1's cone reject already catches some infeasible cases before the corner constraints.

## Experiments (Release, LL_RR and T, 48 fixtures x 153 offsets)
1. For the "cannot be joined" failures: at what point in the level solve is the failure certain (after the cone? after the route constraints? only after the descent)? Which corners conflict (fixed by track vs immovable ground, a bridge span bound, a tunnel bound)?
2. A sound early check before or at the start of the level solve (e.g. the pair rule on the fixed-corner set: two fixed targets closer than their height difference; the cone with the track's forced flat groups' level ranges intersected): prove it sound or show 0 rejects where the solve plans over all offsets; reject rate and cost.
3. Numbers with it: HOVERNEAR_LLRR mean/max and fixtures over 100 ms; T unchanged; both identity hashes unchanged.
4. If nothing sound and cheap exists, say so and list what remains (it may be the time-budget card for the engineer).

## Output
docs/junction-tool/RECON-corner-join-reject.md (in the main repo /home/pete/OpenTTD/docs/junction-tool/, untracked, ~40 lines) and the patch path.

## Report
10 lines. `git -C /home/pete/OpenTTD/.claude/nodes2 status --porcelain` (clean). When you have reported, stop.
