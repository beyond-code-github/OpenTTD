# Z2d (recon by experiment): a joint level solve over the whole junction (DESIGN-G1 §15.3; D218; Z2c's coupling failures)

Recon by experiment: you may change code in your worktree to measure, but commit nothing and leave the worktree clean (`git checkout -- src`, delete scratch files; save your experiment as a patch in the scratchpad). Keep your context small: send command output to files and report only what matters. Never use git stash. Mark anything not measured or read as INFERRED.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes2 (Debug `build`, Release `build-rel`, both built). First: `git checkout -B wt2-z2d <HEAD>` (given at launch). Another agent works in .claude/nodes: never touch it.
- Z2c (merged, src/junction_levels.{h,cpp}): levels per edge by DP between FIXED ends (node, port, head tiles at their own cheapest flat level), terraform plan merged afterwards, propagation with fixed track corners. On the 18 fixtures the hard-facts placement solves (`[realterrainsweep]`), it finds levels for 1: 8 "no levels" (fixed ends further apart in level than the edge can climb), 9 "terraform conflict" (about 5 shared corners between adjacent edges/rows, about 4 the two copies of a doubled edge sharing a run). Synthetic ridges on the T: 31 of 68 solved. D218 says heights never fail, they cost; the coupling is the gap.

## The model to try (PROPOSED by the orchestrator; measure it)
- **Variables:** one level per track TILE (all routes through a tile share it: shared runs, node tiles, doubled copies' shared tiles), in a JunctionPositions (levels as positions, range [lowest ground - 3, highest ground + 3]).
- **Continuity per route, consecutive tiles a -> b:** if a's piece is an axis piece along its travel axis (a possible ramp tile): `-1 <= L_b - L_a <= 1`; else `L_b == L_a`. A tile that is a ramp on one route and anything else on another is flat: then `L_b == L_a` for that route (only single-route axis tiles may ramp).
- **Ramp rule (D219):** per route, for tiles t and t + tl along it: `L_(t+tl) - L_t <= k` (k = JunctionRampLevels), in travel direction (differences; across nodes too).
- **Bridges:** crosser tiles under a middle: `L_crosser <= L_head` (both heads, each).
- **Ports:** variables like the rest (no fixed ends).
- **Feasibility:** propagation; an infeasible system names its chain (reasons on the trail).
- **Cost and choice:** start from each tile's cheapest level within [Lo, Up] (its own ground), then coordinate descent: repeatedly, for each tile (or each group of tiles tied by `==`), move it to the level in its current [Lo, Up] (others fixed: Mark, RestrictRange others, read Lo/Up, Undo) minimising its tile cost (Z2c's per-tile cost table: flat / foundation / ramp / corner moves) plus the cost of the corners it shares with neighbouring track tiles at their current levels; stop when no move improves or after 20 rounds. Then build the plan as Z2c does (fixed corners) and propagate.
- Shared corners between two track tiles at different levels: allowed only if one of them is a ramp or a foundation makes it fit (the per-tile table); otherwise add the cost of the corner move (it becomes a terraform target).

## Experiments (Release)
1. Implement the model as a scratch replacement of Z2c's per-edge DP behind a compile-time switch; same inputs and outputs (plan, cost).
2. On the 18 hard-solved fixtures and Z2c's 68 synthetic ridges (find them in its test or rebuild them: ridge columns and rows, heights 1-4, on the T): solved / infeasible (with the reason chain's first entries) / terraform conflict; for every solved one: the terraformed terrain passes T3's heights (SamePlacement + JunctionRoutesHeightsOk) — any that fails is a finding (tile, edge, why).
3. Cost vs Z2c where both solve; rounds to converge; Release micros mean/max.
4. What still fails and why, grouped; a PROPOSED next fix per group.

## Output
docs/junction-tool/RECON-joint-levels.md (untracked, ~80 lines) and the patch path.

## Report
The conclusions in 10 lines, solved counts (fixtures and ridges) vs Z2c, timing, `git status --porcelain` (clean), `git log -1`. When you have reported, stop.
