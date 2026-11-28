# probe-improve1 (D526): a local improvement pass with the drag primitives

Head 850fdf36fe. Release build, one machine, no solver. Pass = first improvement per round (moves (a) node drags, 8 directions x 1,2 tiles, `PlanarDragTo` with rip on and `original` = the current hub; (b) `PlanarReattachNode` to every `PlanarIsLineCandidate` point within 3 tiles; (c) `PlanarDragHeadTo` +-1), cost = travel + bends per track + 8 per structure, legal = no judge line outside the input hub's own. The test code was temporary and is reverted (a copy sits at scratchpad/improve1/probeimprove-test-copy.cpp). The 60 s cap is checked between rounds, so one round can overrun it.

## First improvement, 60 s cap
| hub | cost before -> after | rounds | accepted drag / reattach / head | total | note |
|---|---|---|---|---|---|
| 1 loop (after.json) | 270 -> 241 | 13 (converged) | 11 / 1 / 0 | 48.5 s | L11 not removed |
| 2 kink | 169 -> 146 | 12 (converged) | 11 / 0 / 0 | 2.3 s | |
| 3 conn | 137 -> 137 | 1 | 0 / 0 / 0 | 0.0 s | no nodes, no reattach, no bridge head move legal |
| 4 T1 drag hub | 236 -> 218 | 8 (converged) | 7 / 0 / 0 | 63.5 s | last round 14.6 s |
| 5 clicked T | 332 -> 284 at the cap (22 rounds, 75 s); converged 281 | 25 | 23 / 1 / 0 | 93 s converged | |

Round time: 0.05 s when the winning move is early in the list, 3-19 s when it is late or the round finds nothing (a no-move round costs the whole list: 6-15 s on hubs 1, 4, 5). Moves tried over a run: 600-770 drags, 50-130 reattaches. New judge lines on every final hub: none (also in best mode).

## Best improvement per round (hubs 1 and 5), 60 s cap
| hub | first (60 s) | best | rounds | accepted drag / reattach / head | total |
|---|---|---|---|---|---|
| 1 | 270 -> 241 (converged) | 270 -> 239 (converged) | 7 | 5 / 1 / 0 | 57.6 s |
| 5 | 332 -> 284 | 332 -> 304 (cut at the cap) | 5 | 4 / 1 / 0 | 73.7 s |

Best improvement takes bigger bites per round (reattach 270 -> 256 on hub 1 at once) but a round is 7-18 s, so per second it is no better than first improvement; first improvement creeps one node 1 tile per round at 0.05-1 s while the move is early in the list.

## Loop verdict (hub 1)
No. L11 (the loop leg from the split at (19,34) round to the merge at (23,24), 42 travel, 6 bends) is the same in the start and in every final hub (first and best). What the pass improved: the splits/merges on the other legs slid west and north (L1..L8 re-shaped; e.g. reattach of the split (20,5) to (14,11) gave -7). Direct drags of the two L11 nodes (not part of the pass): merge (23,24) to (23,30) cost 270 (no change), to (21,34), (23,36), (19,30) refused (no route for L10 or L6), to (21,36) refused (L10 runs over L6); split (19,34) to (21,28) cost 277 (worse), to (23,22) or (21,24) refused. So the loop leg is the same L11 whichever way the nodes move one to six tiles; removing it needs the merge to move to below the split (about 12 tiles south and past L6) and L6 to re-route, a compound move no single drag expresses. No travel for a replacement track exists: none is produced.

## The clicked T (hub 5)
Clicks (my order: B first, because on an empty hub a first click E->S gives the sides of the dominant axis of the click, not E and S; A..F then follow). The frame shifts by (0,-10) after the first click so the least tile is (0,0); tiles below are as clicked in the original frame (frame y = orig y - 10).
- B E->W: new E in at (40,10), new W out at (0,10).
- A E->S: existing E in, new S out at (26,30).
- C S->W: new S in at (14,30), existing W out.
- D S->E: existing S in, new E out at (40,22).
- E W->E: new W in at (0,16), existing E out.
- F W->S: existing W in, existing S out.
All six were accepted, judge empty on the result. Start cost 332 (tracks L1..L12). After the pass: 281 (-15 %), 25 rounds, 93 s. Files: t-start.json, t-final.json (converged), t-final-60s.json (the capped run) in scratchpad/improve1. What moved: first the E split slid from x 40 down-left in 1-tile steps (8 rounds, -2 each), then the W-side nodes up the diagonal, one reattach (34,25) -> (28,27) (-5), and a bottom node 2 tiles north. The long tracks L2 (78 -> 42) and L8 (48 -> 27) shortened, L7 lengthened (13 -> 57) as the cost trades. The result is not near-optimal by eye: nodes ended along one diagonal line, the layout is still the first click's topology (which splits sit on which line is never changed except by one reattach).

## What the pass can and cannot fix
It reliably removes slack that a node's slide along its own legs can recover: 5-15 % cost on every hub with nodes (hubs 1, 2, 4, 5), with no new judge line, and cheaply when the winning move is early (hub 2 whole pass 2.3 s). It cannot change topology-in-space: the loop leg stayed, hub 3 (no nodes) had nothing to move, and the clicked T stopped at 281 with its nodes in a strung-out diagonal because every remaining single move (1-2 tile drag, reattach within 3 tiles, head +-1) is uphill, and the compound moves that would help (move a merge past another node, swap which line a split sits on, remove a loop) are not single moves. It is also slow for the hover target: a no-improvement round is 6-15 s on T-sized hubs, a full pass 50-95 s, 1-2 orders over a seed budget (< 5 s); only a pass that stops after a few rounds or orders moves by promise would fit. Bridge-head moves never won on any hub (0 accepted of about 100 tried). Reattach won 1 of the 13-25 rounds, worth -5 to -7 when it did. Per D526: the pass is a polish for the engineer's clicks, not a route to near-optimal.
