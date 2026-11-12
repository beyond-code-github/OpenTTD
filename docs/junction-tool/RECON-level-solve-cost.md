# RECON: why the level solve costs ~7 ms, and how to cut the level solves per hover (P1)

Release build, 48 fixtures x all 153 near offsets (+/-8 columns, +/-4 rows incl. the cursor spot) = 7344 placements. 4159 fail in the placement (4144 blocked-tile, 12 S-bend size, 3 chain run: 0.3-0.5 ms each, before the level solve); 3185 reach SolveJunctionLevels: 2690 plan, 495 fail (89 "cannot be built on", 406 in PropagateJunctionTerraform). Experiment patch: scratchpad p1-experiments.patch (on top of F1's patch; the worktree is back at F1's diff).

## 1. Profile (baseline, per solve, mean)
| phase | plans (2690) | fails in propagation (406) |
|---|---|---|
| visits + tiles (SolveJunctionLevels 469-515) | 0.04 ms | 0.04 |
| constraint build (junction_levels.cpp 149-297) | 1.18 | 1.19 |
| greedy start (322-343) | 0.18 | 0.19 |
| coordinate descent (390-432) | 6.86 | 10.96 |
| final choices, demand merge | 0.02 + 0.19 | 0.02 + 0.20 |
| PropagateJunctionTerraform (554) | 0.04 | fails inside it |
| total | 8.5 | 12.6 |
About 190 tiles, 45 groups, 2.2 rounds (2.7 on fails), 97 group visits, 233 local_cost calls, 2370 Evaluate calls, ONE propagation per solve.
Hot spot (perf, whole sweep, self time): 72% in JunctionPositions SetLo/SetUp/Propagate/Undo, 6% SolveTileLevels itself (local_cost + Evaluate). The caller is the descent's "held" test, junction_levels.cpp:402-407: per group visit it RestrictRange()s every outside neighbour to its current level (each call propagates through the difference constraints), reads Lo/Up of the group, undoes. ~70 us per group visit. Evaluate/local_cost/propagation are NOT the cost.

## 2. Failing solves: when failure is certain, and sound early rejects
| class (of 495) | n | certain when | sound cheap check |
|---|---|---|---|
| tile "cannot be built on" | 89 | at solve entry (0.04-0.07 ms already) | already cheap |
| corner touches Blocked / Rail or road / Water / outside | 293 | only at PropagateJunctionTerraform, after the descent | partly, see below |
| terraform conflict (fixed corner would move) | 113 | same | none found (depends on final levels of foundation tiles and ground chains) |
The sound check ("cone"): a corner a terraform cannot move (a tile around it is not Buildable or outside the snapshot) ends at its ground height g_u; a successful PropagateJunctionTerraform ends with every moved corner within 1 of all neighbours, and unmoved corners are within 1 by the ground, so every corner c ends in [max_u(g_u - d(c,u)), min_u(g_u + d(c,u))] (d = Manhattan). That is two passes of an L1 distance transform over the snapshot corners (0.03 ms). Per track tile: an option (shape, level) that is not a foundation and moves a corner outside its cone cannot be in any successful plan, so the tile's allowed levels shrink to those with some surviving option (rampable tiles try flat, up and down; a prio tile with no direction takes its main-line tile's). Reject when a tile has none (17 solves), or when the shrunk ranges cannot meet under the route constraints (flat tiles equal, ramp tiles one level, ramp rule; restricted in JunctionPositions, then undone) (78 solves). Placed before the corner constraints (the 1 ms part): the reject costs 0.25 ms instead of 2-12 ms. Cost of the check 0.043 ms mean, 0.23 max.
Reject rate: 95 of 406 propagation failures (23%), 3.0% of the 3185 solves; 0 counterexamples (a reject where the full solve plans) over all 3185. Soundness argument above (needs ground corners adjacent differ <= 1: true of every slope encoding in JunctionTileCorners).
The other 311 unmovable failures are NOT infeasible per tile: each tile alone has a legal option, the cost-optimal levels just pick an illegal one. A variant that makes Evaluate refuse cone-illegal options (greedy start falls back when no flat level is legal; not identical) keeps all 2690 plans and turns 61 more offsets into plans, but HOVERNEAR planned stays 31 and ms_max the same: not pursued (INFERRED: would matter if a fixture's only plan is one of those offsets).

## 3. Speed-ups with identical results
(a) Descent held range without JunctionPositions: all constraints are x_a - x_b >= k with k <= 0 (checked: 0 positive k in the sweep), so with the neighbours held the group's range is max/min over its direct constraints to outside neighbours (per-variable lists, two int loops). Verified equal to the Positions result on 314422 group visits (0 differ). Solve 8.5 -> 2.4 ms (plans), 12.6 -> 2.4 (fails).
(b) Constraint reasons stored as numbers, formatted only in explain() (the fmt strings built for every candidate constraint were 0.2 ms): constraints 1.19 -> 0.98 ms; solve 2.4 -> 2.07 ms.
Identity: all 7344 offsets, signature (cost, levels, terraform corners) hash and result equal to the baseline (0 differences). Remaining solve cost: constraint build 0.98 ms (47%), descent 0.65, start 0.17, merge 0.16.

## 4. HOVERNEAR (F1's near search, real placement, all F1 checks pass: heights, rules, same moved offsets)
| | planned | ms_mean | ms_max |
|---|---|---|---|
| baseline | 31 | 49 | 435-448 |
| (a)+(b) identical speed-ups | 31 | 31.5 | 183 |
| + cone pre-check | 31 | 28.4-29.3 | 89-92 (3 runs) |
Slowest 5: temperate-hilly-15 90.6 (150 placements 71 ms, 50 solves 17.5 ms), temperate-mountainous-6 65.0, temperate-mountainous-9 63.9, temperate-hilly-7 62.5, temperate-mountainous-0 61.0.
What remains: the placement, not the level solve. A fixture with no plan costs 153 placement attempts x 0.4 ms = 60-65 ms (mountainous: all blocked-tile failures), and hilly-15 is 100 failed placements + 50 solves; with the level solve now 2 ms the limit of 100 ms holds with ~10% margin on hilly-15 only because its 50 solves are cheap. Over 100 ms: none in the 48 fixtures; the margin is the placement count times 0.4 ms (render+signals are also run twice per planned offset: PlaceJunctionTrackGraph renders, PlanJunctionHoverAt renders again: 0.4 ms, INFERRED not measured separately).
