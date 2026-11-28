# probe-pf2: PathFinder on T1 with tight ports and a tidy after

Head 9a7b353f79, release build, no solver. `pathfinder = true`, growth 2.0, budget 60 s, T1 = the rig T1 variant. Measures: box in tiles, area = w x h, cost = travel + bends + 8 per structure (half tiles). Probe code temporary, reverted. Tidy = improve3's route rip-up and redraw (20 s cap, ports kept) then improve2's first-improvement node drags (20 s cap, the "first improvement" polish), accept only lower cost and no new judge line.

## 1. Port margin (ring cw, start 0)
| margin | result | iter | s | box | area | travel | cost | judge |
|---|---|---|---|---|---|---|---|---|
| 4 | fail: search budget (chain L12 L8) | 22 | 60.0 | | | | | |
| 8 | built | 12 | 35.4 | 22 x 110 | 2420 | 644 | 712 | empty |
| 12 | built | 8 | 25.4 | 22 x 112 | 2464 | 652 | 720 | empty |
| 16 | fail: finish, bridge approach track at 5,2 | 3 | 5.1 | | | | | |
| 24 | fail: finish, bridge approach track at 13,1 | 4 | 8.4 | | | | | |
| 40 | built | 7 | 16.0 | 23 x 50 | 1150 | 376 | 434 | empty |

## 2. Port order variety (best two margins: 40 and 8)
| margin | ring | start | result | s |
|---|---|---|---|---|
| 40 | cw | 0, 2 | built, identical to the margin run (1150, cost 434) | 15-16 |
| 40 | cw | 1, 3 | fail: no convergence after 50 iterations, 5 overused tiles | 26 |
| 40 | ccw | 1, 3 | built, identical (1150, cost 434) | 16-19 |
| 40 | ccw | 0, 2 | fail: no convergence (50 iterations) | 26-28 |
| 8 | cw | 0, 2 | built, identical (2420, cost 712) | 47 |
| 8 | cw | 1, 3 | fail: no convergence | 57 |
| 8 | ccw | 1, 3 | built, identical (2420, cost 712) | 46-50 |
| 8 | ccw | 0, 2 | fail: no convergence | 58 |

Order changes only whether it converges (cw even start and ccw odd start build; the other half does not), never the hub: every built result at a margin is the same box and cost.

## 3. Tidy (three smallest legal builds; each distinct build is the only one of its size)
| build | start (box, area, cost) | after routes (20 s cap, used 21 s) | after node polish (20 s) | judge |
|---|---|---|---|---|
| m40 | 23 x 50, 1150, 434 | 23 x 43, 989, 336 (11 redrawn, 6 accepted; travel 376 -> 306, bends 18 -> 6, structures 5 -> 3) | 23 x 43, 989, 334 | no new line |
| m8 | 22 x 110, 2420, 712 | 22 x 110, 2420, 666 | 22 x 110, 2420, 632 | no new line |
| m12 | 22 x 112, 2464, 720 | 22 x 112, 2464, 674 | 22 x 112, 2464, 640 | no new line |

## 4. Reference: the engineer's T1 (`JunctionDragT1Hub()`, no prio rows)
box 23 x 14 = 322 tiles2, travel 196, bends 16, 3 structures, cost 236, judge empty. (The rig `hub.json` on disk is 23 x 15 = 345, travel 226, cost 278 and judges one curve line, so it is not the answer key; the brief's "about 17 x 11" is not what these two files measure.)

## Best result against T1
Best: m40 build, tidied: 23 x 43 = 989 tiles2, cost 334, judge no new line (best.json in the scratch folder). Against the drag T1: area 989 / 322 = 3.07x, cost 334 / 236 = 1.42x (travel 300 vs 196 = 1.53x, bends 10 vs 16, structures 3 vs 3). Width is equal (23); height is 43 against 14.

## Gap
PathFinder plus tidy is 1.4x the engineer's cost but 3.1x his area, and almost all of the area gap is box edge, not node or bridge layout. The tidied hub's tracks all lie between y = 15 and y = 42 half tiles (about 14 tiles, his height) except the two S-side lines, L12 (y 42 to 100) and L7 (y 36 to 100), which are plain straight runs to the S ports at y = 100: the builder places the S ports 40 half tiles beyond the hub's extent, the tidy keeps ports by construction, and so the box stays 50 tiles tall while the work happens in the top 14. Width is already his 23 because the E and W ports sit at x = 46 and x = 0. Smaller margins do not fix it: 8 and 12 build but with 110 tiles height (the S legs then take long detours in the soft pass), 16 and 24 fail Finish on a bridge approach track, and 4 runs out of search budget. The cost gap (98 half tiles) is travel (300 vs 196): the tidy brought bends and structures to his 3 and 10 vs 16, so what remains is the long port legs plus node spacing; the tidy cannot shorten a port leg because it never moves a port. The next lever is therefore a port pull-in after the build (move each port to the hub's own edge) and not more routing; bridge spans are not part of the gap (3 structures each).

# pf3: port pull-in on the tidied T1

Head 9a7b353f79, release build, input = pf2's best.json (23 x 43, cost 334). Temporary `[.][pf3probe]` test in `junction_drag.cpp`, reverted. Pull-in: `PlanarDragTo(hub, end, end + 2 * inward, spec, &why, true, true, &hub, &allowed, true)` per port in hub order, repeated per port while accepted, then a one-tile slide each way; accepted when the cost drops, or stays equal with a smaller box, and no judge line outside the input's own. Route rip-up, node polish and pull-in as in pf2, with the caps 30 / 20 / 20 / 15 s.

| stage | box | area | cost (travel, bends, structs) | judge (new) | moves accepted | s |
|---|---|---|---|---|---|---|
| input | 23 x 43 | 989 | 334 (300, 10, 3) | empty (0) | | |
| pull-in 1 | 23 x 43 | 989 | 276 (244, 8, 3) | empty (0) | pull 28, slide 1 | 1.5 |
| routes | 23 x 43 | 989 | 276 (244, 8, 3) | empty (0) | none (6 redrawn, 0 better) | 12.9 |
| node polish | 23 x 43 | 989 | 270 (234, 12, 3) | empty (0) | node drag 4 | 20.0 |
| pull-in 2 | 23 x 43 | 989 | 270 (234, 12, 3) | empty (0) | none | 0.4 |

Final (final.json in the scratch folder) against the engineer's T1 (23 x 14 = 322, cost 236, travel 196): cost 270 / 236 = 1.14 (was 1.42), area 989 / 322 = 3.07 (unchanged), box 23 x 43 against 23 x 14.

Gap. The pull-in took the cost gap from 98 to 34 half tiles (travel 300 to 234, against his 196) because the S in port L12 walked from y = 100 up to y = 46 (27 pulls), but it did not touch the area: the box is still set by the S out port L7, a straight run from its merge at (27,36) to (27,100) that no pull-in moved. The drag does accept the one-tile pull of L7's end, but relaying L7 along its straight line drops its bridge over L4 (L4 crosses it at about (27,39)), so the rip-up (D488) throws L4 into a 12-point detour north of the hub and adds a structure: +30 cost for the one-tile pull (306 against 276), +20 for six tiles, so the rule rejects it every time. L12's own pull stalls at y = 46 for the same kind of reason (one tile refused, two tiles accepted at +46, area worse because the track then runs wider). So what is left of the gap is one track: with L7 pulled in to the hub (L12 now ends at y = 46, so the tracks span y = 15 to 46) the box would be about 23 x 16 (from the points, not measured), next to his 23 x 14; the node polish and the route pass do not do it because they keep ports. A single-port pull of L7 that keeps the L4 crossing as a bridge (the straight track rising, D488 relay of L7 only, not a rip-up of L4) is the missing move. Beyond that, the cost left is travel 234 against 196 and 12 bends against 16 (node spacing, not bridges: 3 structures each).

# pf4: pf3's pipeline again with the bridge head at a node (D529)

Head 11608e0e63 (D529 in the router), release build, input = pf2's best.json (23 x 43, cost 334). The same temporary probe as pf3 (reverted), same caps 30 / 20 / 20 / 15 s. Final hub: scratchpad pf4/final.json.

| stage | box | area | cost (travel, bends, structs) | judge (new) | moves accepted | s |
|---|---|---|---|---|---|---|
| input | 23 x 43 | 989 | 334 (300, 10, 3) | empty (0) | | |
| pull-in 1 | 23 x 16 | 368 | 218 (186, 8, 3) | empty (0) | pull 57, slide 1 | 1.4 |
| routes | 23 x 16 | 368 | 218 (186, 8, 3) | empty (0) | none (6 redrawn, 0 accepted) | 9.6 |
| node polish | 23 x 16 | 368 | 214 (178, 12, 3) | empty (0) | node drag 3 | 20.0 |
| pull-in 2 | 23 x 16 | 368 | 214 (178, 12, 3) | empty (0) | none | 0.5 |

Against the engineer's T1 (23 x 14 = 322, cost 236, travel 196, bends 16, 3 structures): cost 214 / 236 = 0.91 (pf3: 1.14), area 368 / 322 = 1.14 (pf3: 3.07), box 23 x 16 against 23 x 14. The pull-in now takes L7 in (L7 keeps its bridge over L4), so the box is set by the hub, not the port legs; the travel is 178 against 196 and the bends 12 against 16. What is left is two tiles of height.
