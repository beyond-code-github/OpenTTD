# RECON P3: why an LL_RR placement on real terrain costs 70-150 ms per offset

Release, 48 fixtures, LL_RR built from the notations (hub topology), shared stages used. Patch: scratchpad/p3.patch (speed-ups + the NOT_REACHED guard; apply on af3a03a004). Profile harness (timers, per-stage test `[p3profile]`, P3ALL=1 for all 153 offsets): scratchpad/p3-profile-harness.patch (diff vs af3a03a004, includes the speed-ups).

## 1. Stage times (measured, before the speed-ups; 7344 offsets = 48 x 153, 1180 plan, 6164 fail; the near-search order and stop at the first plan gave 4844 offsets, same picture)
- JunctionPlacementShared is used for LL_RR (fixed choices path, junction_topology.cpp:~2938): Y solve is shared, flat Y/X plan_clearance skipped.
- The placement itself (PlaceTrackGraph, no level solve) is cheap: 0.86-0.9 ms per offset, planned or failed alike (build 0.33-0.5 ms of which the facts block 0.12-0.29; X solve 0.28-0.45; settled legality 0.06-0.1). The T's ~0.3 ms holds in kind; LL_RR is 3x bigger. 5593 of the 6164 failures are "placement rules contradict each other" at ~0.75 ms.
- The hot spot is the level solve (SolveJunctionLevels -> SolveTileLevels, junction_levels.cpp): 40 ms per call on planned offsets (1180 calls, 47 s of 53 s), 82 ms per call on the 571 failing offsets that reach it (502 "terraform conflict" at ~210 ms each in the first run, 69 "cannot reach the ground" 3 ms each: the cone filter rejects early). Not 2-3.5 ms as for the T: LL_RR has n = 532 tiles (T: far fewer), 92 equality groups, 3321 cone corners (the whole 80x40 snapshot), 2.9 descent rounds, ~580 exact_cost calls per solve.
- Inside it: the coordinate descent (junction_levels.cpp ~l.886, 94-96% of the solve) = exact_cost calls (0.064 ms each planned, 0.16 failing); in exact_cost the raised-x-lowered pair loop ("a raised target against a lowered one", ~l.766) was 80% (35 s of 44 s). The rest of the facts/relations/solve/render+signal are 2-3% each (render+signal 1.8 ms per plan).

## 2. Why
The pair loop was O(|t_set|^2) over the ~2000 touched target corners per evaluation with a branch per pair, although a pair only counts when raised target - lowered target >= 2. And `best_cost = exact_cost(level)` was recomputed for every group although the levels had not changed since the last evaluation (45% of all calls). Failing offsets pay it because the terraform conflict is only known after the descent ends.

## 3. Speed-ups (identity: hash over ALL 7344 LL_RR offsets of plan/no plan, why text, height_cost, track_levels, terraform corners and cost = 5813095187505070117 before and after each step; T LEVELHASH 16256945156918441187 unchanged, jt identity passes)
1. Pair loop over the two lists with the max-raised/min-lowered prefilter: level solve 40 -> 15 ms (planned), 82 -> 21 ms (failing).
2. Cache the cost of the held levels until a group moves (provably the same value; exact_cost is a pure function of the levels): 15 -> 9.5 ms, 21 -> 11.5 ms; exact_cost calls 687k -> 420k.
3. Lazy notes for the blocked-tile and height-step reasons of row runs (formatted only when a failure is reported; why text identical): failing placement 0.76 -> 0.67 ms.
Results (LL_RR sweep, same box, load ~8): HOVERPLAN_LLRR ms_mean 16.7 -> 4.5, ms_max 208 -> 28; HOVERNEAR_LLRR ms_mean 470 -> 135, ms_max 10404 -> 1552. T: HOVERPLAN max 4 ms, HOVERNEAR max 73-85 ms (unchanged logic; measured only after).
Tried and dropped: a ring scan instead of the pair list (quad block 4.5 -> 3.4 s, 15% of the descent, not worth the code); dropping the group note formatting too would give 0.67 -> 0.62 ms per failing placement (not done).

## 4. What remains over 100 ms (near search, per fixture, final build)
- 25 of 48 fixtures take > 100 ms (36 > 30 ms). Two classes:
  a. 28 no-plan fixtures x 153 offsets x ~0.7-0.9 ms = 110-160 ms each: the placement alone, no level solve. To get under 100 ms the placement must reach <= 0.6 ms per failing offset (lazy group notes 0.62; the facts rows cached across offsets of one fixture; fewer rules built before the first contradiction) or the near window must shrink for big boxes (LL_RR 62x25 in 80x40 snapshots: most offsets are outside the terrain; a bounds reject before any solve would make them ~free: INFERRED, not measured), or the near search gets the D197 time budget.
  b. Fixtures with many terraform conflicts: temperate-mountainous-5 1532 ms (128 offsets, 79 level solves ~19 ms), temperate-mountainous-3 942 ms (71 offsets, 67 solves), arctic-hilly-13 443 ms (26 offsets, 23 solves), arctic-hilly-2 288 ms. A level solve costs 10-20 ms: cap the solves per near search (e.g. 4-5 = 100 ms), or run the descent on one round, or incremental exact_cost (only the moved group's tiles and cones; ~10x, not tried).
- Time budget (D197 style) is the only way to a hard 100 ms for class b; speed-ups alone leave 1.5 s worst case.

## 5. Found: crash in the level solve (fixed in the patch)
At arctic-hilly-4 offset (137,109) (near search move (-6,-1)) exact_cost builds a foundation tile whose terraformed corners spread >= 2 levels over two raised corners (slope 22 = STEEP|E|S); GetRailFoundation / GetBridgeHeadGround then reach GetHighestSlopeCorner -> NOT_REACHED (FatalError aborts the game). Reachable by hover on any terrain; the T sweep does not hit it. The patch counts it as a violation (viol++, like the existing foundation-test violations), which only changes the offsets that aborted before.
