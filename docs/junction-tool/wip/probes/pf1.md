# probe-pf1: a PathFinder negotiation on T1 (D528, HEAD 850fdf36fe + the flag)

Hidden test `[pathfinderprobe]`, Release, `budget_seconds` 170, T1 (rigt1).

## Version 1 (crossing plan, chain must place)

| run | present start | growth | overused per iteration | s per iteration | outcome |
|---|---|---|---|---|---|
| 1 | 0.5 | 1.5 | none (iteration 0 did not finish) | 0 | not built: negotiation: chain L6 has no admissible attempt; last: no route from (62, 65) to (112, 65) within 140 half tiles with bridges of at most 10 middle tiles |
| 2 | 0.5 | 2.0 | same | 0 | same |
| 3 | 1.0 | 1.5 | same | 0 | same |

The soft router relaxes only refusals caused by another track's pieces; the crossing plan and the bridge limit stay hard, so with L9 and L12 L8 laid over the others L6 has no route.

## Version 2 (D528 amended: no crossing plan, `cross_any`; an unplaceable chain stays unplaced and is retried), dependents stop kept

Iteration 0 leaves 2 (L6 soft over L9 at tile (33, 33), L11 unplaced); iteration 1 stops: L2 L3 L5 L13 L15 has placed dependents (L6, L11). Identical in all three runs.

## Version 3 (a rip takes the chain's placed dependents; they are placed again in the same iteration)

| run | present start | growth | overused per iteration (iteration 0 first) | s per iteration | outcome |
|---|---|---|---|---|---|
| 1 | 0.5 | 1.5 | 2 2 4 8 5 5 5 4 5 0 | 1.53 | built after 9 iterations; box 23 x 50 tiles, travel 376 half tiles, judge empty |
| 2 | 0.5 | 2.0 | 2 2 4 8 5 4 5 0 | 1.72 | built after 7 iterations; box 23 x 50 tiles, travel 376 half tiles, judge empty |
| 3 | 1.0 | 1.5 | 2 2 2 2 2 0 | 2.54 | converged after 5 iterations but the finished hub is not legal: rules: bridges: no approach track outside the head at 5,2 and at 4,2 |

Iteration 0 (all runs): the anchor and L2 L3 L5 L13 L15, L4 L10, L9 and L12 L8 place with no conflicts; L6 places soft over L9, the overused tile is (33, 33) (domain: head tracks L6, L9 at that tile); L11 stays unplaced: "no admissible attempt; last: domain: head tracks L6,L9 tile 33,33" (0 conflicts in its attempts), so the count is 1 tile + 1 unplaced chain.

Does it converge on T1? Yes: with the crossing plan dropped and dependents ripped, runs 1 and 2 reach no conflict with every chain placed in 9 and 7 iterations (about 14 and 12 s of wall time) and the finished hub judges clean; run 3 converges too but its finished hub breaks a bridge approach rule that the iteration's soft judge (partial hub) does not see, so the build ends with that why. What limits it: the count does not fall steadily (it rises to 8 at iteration 3 and holds around 5 before dropping to 0 in one step), and chains drop in and out of placement (L2 L3 L5 L13 L15 was unplaced in iterations 4 and 6 for "bridge: structure 0 (L12) has 2 spare middles"), so the history, not the present price, does the separating; every iteration costs 1.5 to 2.5 s.
