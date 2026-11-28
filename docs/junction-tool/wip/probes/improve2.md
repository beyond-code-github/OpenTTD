# probe-improve2 (D526): rip up and redraw whole routes

Head 850fdf36fe, release build, no solver. Per route: remove the exit port (else the entry port) when its track carries only that route, redraw with `PlanarAddRouteBetween` from the old port ends (existing port if it survived, else a new port at the old handle tile). Accepted when the cost (travel + bends + 8 per structure) drops, no judge line outside the input's own, and the hub's ports (in, side, end point) are unchanged. Rounds until none improves, 20 s cap; then improve1's first-improvement node drags (moves (a) only), 10 s cap. The probe code was temporary and is reverted.

## Table (forward order; reverse in the next table)
| hub | start | after routes | after polish | route rounds, time | redrawn / accepted / skipped / port-rejected | draw failed | judge on final |
|---|---|---|---|---|---|---|---|
| 1 loop | 270 | 267 | 265 (263 in an earlier run: the 10 s cap cuts the polish, timing-dependent) | 2, 1.7 s | 4 / 1 / 4 (2 per round) / 0 | 4 (no split point: search budget) | no new line |
| 2 kink | 169 | 168 | 135 | 2, 0.7 s | 6 / 1 / 0 / 2 | 2 (search budget) | no new line |
| 3 conn | 137 | 137 | 137 | 1, 0.0 s | 3 / 0 / 0 / 1 | 0 | no new line |
| 4 T1 drag | 236 | 236 | 231 | 1, 0.0 s | 0 / 0 / 6 / 0 | 0 | no new line |
| 5 clicked T | 332 | 332 | 314 (10 s cap, still falling) | 1, 0.0 s | 0 / 0 / 6 / 0 | 0 | no new line |

Polish: hub 2 21 rounds 5.1 s (converged); hub 1 2 rounds (capped); hub 4 3 rounds (capped; a no-move round is 6-15 s); hub 5 11 rounds (capped). Hub 3 has no nodes.

## Reverse order
| hub | after routes | after polish |
|---|---|---|
| 1 | 267 | 265 |
| 2 | 168 | 135 |
| 3 | 137 | 137 |
| 4 | 236 | 231 |
| 5 | 332 | 314 |

## What the route pass did
- Hub 1: one accepted redraw, entry (50,3) to exit (0,5), 270 -> 267 (L7/L8/L4 reshaped, L4 now bends once). Of the 11 routes the pass can touch 2 per round (both entries and exits shared: (50,1) and (50,3) to exit (23,36)); two redraws (to exits (17,36) and (19,36)) die with "no split point: search budget" (the 400-router-call limit); the others are no better.
- Hub 2: one accepted redraw, -1. Two redraws failed on the search budget (entry (46,5) to exit (19,32)). Two were port-rejected: the redrawn exit lands at (23,30) where the old port point was (23,32).
- Hub 3: one port-rejected (an exit (37,36) became (37,34); an entry from side W (0,1) became side N (1,0)), two redraws no better.
- Hubs 4 and 5: every route shares its entry port or exit port with another route (3 entries, 3 exits, 6 routes), so the rule skips all six: the pass does nothing on them.
- Port rejection: a redraw from a new port at a handle tile gives the tile edge midpoint, which differs from the engineer's port point when that was off-midpoint or the heading differs.

## Loop verdict (hub 1)
Not gone. L11 (split (19,34) -> (24,39) (30,39) (39,30) (39,24) (34,19) (28,19) -> merge (23,24), 42 travel, 6 bends) is identical in the start and in every final hub, forward and reverse. The routes that use it end at exit (23,36), which is shared by two entries ((50,1) and (50,3)) and so both are skipped by the rule; no replacement leg was drawn.

## Order sensitivity
None on the routes: forward and reverse reach the same costs after the route pass on all five hubs (267, 168, 137, 236, 332). After the polish hub 1 (265 vs 265 here; 263 in an earlier forward run) differs only by the timing of the 10 s cap; hubs 2, 4, 5 are the same.

## Against improve1
Redrawing routes recovers far less than node drags where it applies at all: 1-3 cost on hubs 1 and 2 (improve1's passes took 29 and 23), 0 on hubs 3, 4, 5, and on the two hubs that matter most (T hubs) it cannot run, since every route shares an end port with another. The cost of a redraw is small (0-1.7 s per hub) but a redraw is bounded by the 400-call search budget and by the port check, which each killed 2-4 attempts on hubs 1-3. The polish after the routes reached the same level as improve1's whole pass on hub 2 (135 here vs 146) within 5 s, because the 1-cost route change opened more drags, a single case, not a rule; on hubs 1, 4, 5 the 10 s cap left it short of improve1's converged values (263-265 vs 241, 231 vs 218, 314 vs 281) since the first rounds are the slow ones. The loop is not removed by either pass. Whole-route redraw as specified is not a route to near-optimal; what it would need is to drop the "ports unshared" condition (redraw a route through the shared trunk) and a larger search budget, neither tested.

Files: scratchpad/improve2 (run-h*.txt, h*-final2.json, -rev variants), scratchpad/improve1/t-final2.json (hub 5, forward, after the polish).
