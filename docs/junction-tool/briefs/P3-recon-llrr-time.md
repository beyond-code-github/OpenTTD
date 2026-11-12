# P3 (recon by experiment): why an LL_RR placement on real terrain costs ~70-150 ms per offset (LR1's report)

Recon by experiment: change code in your worktree to measure, commit nothing; save experiments as a patch in /tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/p3.patch and leave the worktree clean. Keep your context small: output to files, report only what matters. Never use git stash. Never start a game client or touch the engineer's game or rig servers. Mark anything not measured or read as INFERRED.

## State
- Worktree /home/pete/OpenTTD/.claude/ng1 (Debug `build`, Release `build-rel`). First: `git -C /home/pete/OpenTTD/.claude/ng1 checkout -B wt-p3 <HEAD>` (given at launch). Other agents work in /home/pete/OpenTTD/.claude/nodes and .claude/nodes2: never touch them. Use absolute paths.
- LR1 (9927400a71): the hover builds the topology from the notations; `[realterrainsweep]` "hover plan on real terrain, LL_RR": HOVERPLAN_LLRR 7/48 planned, ms_max 227; HOVERNEAR_LLRR 20 planned, ms_mean 484, ms_max 9606 (temperate-mountainous-5: 128 offsets, 8812 ms in PlaceJunctionTrackGraph incl. the level solve; failing offsets ~150 ms each). The T: ~0.3 ms per failing placement, ~2-3.5 ms level solve (LS1/HC1/P2). The 100 ms hover target (D151).

## Experiments (Release, LL_RR built topology, the 48 fixtures, centred offset and the near offsets)
1. Per stage time of the facts placement for LL_RR (shared flat solve/Y via P2's JunctionPlacementShared: is it used for LL_RR?; X solve; facts block; settled legality; level solve; anything else), for failing and passing offsets; compare with the T. Name the hot spot(s) with file:line and call counts.
2. If the X solve or the facts block dominates: why (size of the system, number of rounds, explanation building, failure path doing extra work such as building reasons eagerly); if the level solve dominates: sizes (tiles, corners, descent sweeps) and where the time goes (as LS1 did for the T).
3. Try the one or two identity-preserving speed-ups the profile points at (same plans and same failure classes on all 48 x 153 offsets: hash as LEVELHASH does); report their effect on HOVERPLAN_LLRR and HOVERNEAR_LLRR mean/max.
4. What remains over 100 ms and what it would take (e.g. a cheaper early reject, a coarser near search for big boxes, the time budget D197 might allow): list, with numbers.

## Output
docs/junction-tool/RECON-llrr-time.md (untracked, ~60 lines) and the patch path.

## Report
10 lines: the hot spots, the speed-ups and their numbers, and what remains. `git -C /home/pete/OpenTTD/.claude/ng1 status --porcelain` (clean). When you have reported, stop.
