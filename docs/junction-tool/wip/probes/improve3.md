# probe-improve3 (D526): remove a route itself, keep its ports, redraw it

Head 850fdf36fe, release build, no solver. Per route (hub route order, then reverse): `RemoveRoute` (erase the route, cascade of untouched tracks / dangling nodes / empty ports as `PlanarRemovePort` does, then `PlanarDeriveTopology` for the dissolve; `HubWithout` and `RecomputeCrossers` copied into the test because they are file-static), then `PlanarAddRouteBetween` from the old entry port (or a new port at its old handle tile) to the old exit port (likewise). Accepted when the cost (travel + bends + 8 per structure) drops, no judge line outside the input's own, and the hub's ports (in, side, end point) and route count are unchanged. Rounds until none improves (20 s cap), then improve2's 10 s first-improvement node drags. Probe code temporary, reverted.

## Table (forward order)
| hub | start | after routes | after polish | route rounds, time | redrawn / accepted / budget-failed / port-rejected | judge on final |
|---|---|---|---|---|---|---|
| 1 loop | 270 | 246 | 228 (12 drag rounds, 10 s cap) | 3, 9.4 s | 13 / 4 / 3 / 1 (5 draw failures in all) | no new line |
| 2 kink | 169 | 168 | 135 (converged, 5.1 s) | 2, 0.7 s | 6 / 1 / 2 / 2 | no new line |
| 3 conn | 137 | 137 | 137 | 1, 0.0 s | 3 / 0 / 0 / 1 | no new line |
| 4 T1 drag | 236 | 196 | 196 (1 round, 10 s cap) | 3, 21.1 s | 16 / 4 / 0 / 0 | no new line |
| 5 clicked T | 332 | 332 | 314 (11 rounds, 10 s cap) | 1, 9.2 s | 6 / 0 / 0 / 0 (all 6 not better) | no new line |

## Reverse order
| hub | after routes | after polish | route rounds, time | redrawn / accepted / budget-failed / port-rejected |
|---|---|---|---|---|
| 1 | 244 | 241 | 2, 6.5 s | 12 / 2 / 0 / 4 |
| 2 | 168 | 135 | 2, 0.7 s | 6 / 1 / 2 / 2 |
| 3 | 137 | 137 | 1, 0.0 s | 3 / 0 / 0 / 1 |
| 4 | 196 | 196 | 2, 14.4 s | 12 / 4 / 0 / 0 |
| 5 | 332 | 314 | 1, 9.2 s | 6 / 0 / 0 / 0 |

Budget failures are never more than a third of the attempts (hub 1 forward 3 of 18, hub 2 2 of 8), so no rerun with a raised `LINK_MAX_ROUTER_CALLS`. Every redraw kept the route count. Hub 5 `t-final3.json` is in the improve1 scratch folder (forward, after the polish); the route pass changed nothing there, so it is improve2's 314 hub by another path.

## Loop verdict (hub 1): gone
In both orders L11 (the 42-travel, 6-bend loop leg) is replaced by a straight leg. Forward: L11 (38,7) -> (21,24), 21 travel 0 bends after the routes (17 after the polish), the routes to exit (23,36) drawn again through it (270 -> 267 -> 266 -> 258 -> 246). Reverse: L11 (44,7) -> (23,28), 25 travel 0 bends. The hub's layout moves a lot (the splits end up far from where they started), but the ports and the judge are the input's own.

## Order sensitivity
Real on the two hubs that move: hub 1 reaches 246 forward and 244 reverse after the routes, but 228 vs 241 after the capped polish (different hubs, so a different number of drag rounds fit in 10 s); hub 4 reaches 196 in both orders by different routes (3 rounds vs 2). Hubs 2, 3, 5 are order-independent (168, 137, 332).

## Against improve1 and improve2
Removing the route itself fixes what improve2 could not: on hubs 1 and 4, where every route shares a port, the pass now runs and takes 24 and 40 off (improve2 took 3 and 0; improve1's whole drag pass took 29 on hub 1, with the loop kept, and 18 on hub 4 to 218). The loop is removed, which neither earlier probe did. Hub 4 at 196 beats improve1's 218 with no drag at all, so route redraw is the stronger move there, and after the routes the 10 s polish found nothing (its first round alone is 10 s). It does nothing on hub 5 (all 6 redraws are not better, 9.2 s, polish 314, as improve2; improve1's 281 stays best) and little on hubs 2 and 3, where the same search-budget (2 on hub 2) and port rejections (2 on hub 2, 1 on hub 3) from improve2 still kill the redraw. The cost is higher than improve2: 6-21 s per hub where a route pass runs, against 0-2 s. On hubs 1 and 4 the route pass is the better first step and the drags are the cheaper second; on hub 5 only the drags help.
