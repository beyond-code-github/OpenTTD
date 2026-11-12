# HC0 (recon by experiment): the level solve's objective from exact corners instead of ADJ/FLEX/SLOPE2 (REVIEW Q4; DESIGN-G1 §15; HM1's finding)

Recon by experiment: change code in your worktree to measure, commit nothing; save the experiment as a patch in /tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/hc0.patch and leave the worktree clean (`git -C <wt> checkout -- src`). Keep your context small: output to files, report only what matters. Never use git stash. Mark anything not measured or read as INFERRED.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (Debug `build`, Release `build-rel`). First: `git -C /home/pete/OpenTTD/.claude/nodes checkout -B wt-hc0 <HEAD>` (given at launch). Another agent works in /home/pete/OpenTTD/.claude/nodes2: never touch it. Use absolute paths.
- The level solve (src/junction_levels.cpp) picks a level and shape per track tile by greedy start + coordinate descent; its cost model approximates the terraform "spill" (corners next to a changed corner that must follow, the game's |corner difference| <= 1 rule) with fitted terms ADJ, FLEX and SLOPE2 (RECON-joint-levels picked them by fixture count). The real terraform is computed afterwards by PropagateJunctionTerraform.
- HM1 (399aa08be0) made the levelled foundation the game's, judged on the tile's GROUND slope; it found that a neighbour's terraform can raise a foundation tile's corners and change what the game builds there (T3 catches it afterwards).

## The exact model (orchestrator's derivation: verify it)
For fixed track-corner targets t_k (from the tiles' levels and shapes) and immovable corners u (ground g_u), the cheapest finished terraform is, per free corner c, `clamp(g_c, L_c, U_c)` with L_c = max over fixed corners f of (h_f - d(c,f)), U_c = min of (h_f + d(c,f)) (d = Manhattan corner distance; two L1 distance-transform passes, as LS1's cone). Ground is 1-Lipschitz in the game and so are L and U, so the clamp is 1-Lipschitz (a legal terrain) and is pointwise the least move, hence optimal for sum |c - g|; it is infeasible iff some L_c > U_c (or a fixed corner violates the cone). Foundation tiles leave their corners free between ground and level (their corners are not fixed targets, but the final slope must still give the game's foundation: check after).

## Experiments (Release, 48 fixtures x 153 offsets as LEVELHASH)
1. Verify the derivation: for every plan the current solve makes, compute the clamp terrain for its own tile levels/shapes: is it equal to PropagateJunctionTerraform's result (corners and cost)? If not, how they differ (counts, examples).
2. Exact objective: in the descent, replace ADJ/FLEX/SLOPE2 (and whatever spill approximation exists) by the clamp cost of the full assignment (recompute per evaluated move; then try an incremental update limited to the corners whose L/U change, if the full one is slow). Report: plans, mean/max cost vs current (same offsets), offsets gained/lost, ms per solve (mean/max), and how many plans now fail T3 (the foundation-after-terraform effect) under each model.
3. Foundation after terraform: in the exact model, require for each foundation tile that the clamp terrain's slope still gives GetRailFoundation == Leveled at that level (reject the option or the assignment otherwise): how many plans change.
4. Whether ADJ/FLEX/SLOPE2 can be deleted outright with the exact objective (identical or better plans, no T3 failures, time within 2x of today's ~2 ms).

## Output
docs/junction-tool/RECON-exact-corners.md (untracked, ~60 lines) and the patch path.

## Report
10 lines: derivation verified or not, exact vs fitted (plans, cost, time), T3 failures before/after, and a recommendation for the implementation step (what to delete, what to add, expected numbers). `git -C /home/pete/OpenTTD/.claude/nodes status --porcelain` (clean). When you have reported, stop.
