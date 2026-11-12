# RECON N1: how big is snap-and-negotiate on real terrain (DESIGN-G1 §17)
Branch wt2-n1 from dfd404be4c, Release `build-rel`, T topology, the 29 of 48 `[realterrainsweep]` fixtures that PlanJunctionHover fails (19 plan). Experiment patch (not committed): /tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/n1-experiment.patch (hooks in junction_topology.cpp: waiver set, X-phase range probe, floors on the facts overload; hidden `[.n1]` tests in junction_real_terrain.cpp). Raw outputs n1_e1b/e2/e3/e4/e5.txt beside it. Anything marked INFERRED was not measured.

## What the fixtures are
Snapshot 80x40, the seed box 41x14 centred in it, ports on the box boundary (W/E rows 1 and 3, S cols 26 and 28). Terraform cost 187 per corner, foundation 187; the cheapest node tile costs 15 (clear cost only) almost everywhere.

## E1 failing items
- Every one of the 29 failures is an X-phase contradiction of the "blocked tile" facts (class != Buildable: water, houses, rail/road), found by propagation. None fails in the level solve, render or signals. First reason: EDGE 28, NODE 1, OTHER 0. (NODE = a fact tagged with a node; 18 of 85 items. Only 12 of those have the node tile itself unbuildable over its whole range; the rest are the element at the node, a bend half beside it.)
- Items iterated (waive the first fact's item, re-solve, up to 6 rounds, until the X solve passes): 1 item: 5 fixtures, 2: 8, 3: 6, 4: 4, 5: 2, 6 or more (cap, still failing): 4. Mean 2.9, median 3; 85 items in all.
- Which items: e0 row 0 (the in leg's row run from the W port) 20 times, e9 row 1 (the out leg's row run to the W port) 17, e8 6, e5 row 1 6, e7 5, e4 4, node items 15, others 1-3 each. The two port-side row runs dominate (37 of 85).
- Caution: after the X solve passes, 25 of 25 non-capped waived runs then stop at "tile cannot be built on" (the level solve refuses the waived blocked tile), so items-to-X-pass counts what negotiation must change, not what is then plannable.

## E2 node spots (probe: relations only, box inside the snapshot)
- Every node's legal range is 21 columns (flat column .. +20): the box's NW corner is fixed, so a node only slides east, and the room is the snapshot's slack (80 - 41 = 39, halved by the centring; INFERRED: a real hover snapshot's size sets this number).
- Of the 174 node-ranges in the failing fixtures, 19 have no hard-fact-free node tile (0 spots). Failing NODE items: median 0 free columns (12 of 18 have none, in 4 fixtures whose whole node rows lie in water); the other 6 have 8, 9 or 21 free columns (their blocker is the bend at the node, not the node tile).
- Cost of the best 3 spots: identical (15 each, plain clear cost) wherever the ground is flat; the node-tile cost does not rank the spots. Ranking would need the cost of the whole element and its neighbours.

## E3 edge shapes (geometry only, flat positions as the ends: INFERRED proxy for "current ends", since the failing solve has no solution to read ends from)
- Per failing edge item the tracer enumerates every Shift pair (size 1, both sides, every start and end inside each straight run; the middle run at least 2 tiles, from the curve gaps at tl 4) and S-bend m +/- 1 re-traces, checks hard facts (Buildable) and the tiles of the other flat edges.
- Of 71 edge items, 56 have a flat-position tile that is blocked. A single Shift pair that covers all of them survives for 1 (and is free of other tracks), an S-bend m +/- 1 re-trace for 1; either: 2. Without the "covers every blocked tile" test 37 of 71 had a surviving shift, which is the trap: a shift that avoids one obstacle but not the next is not a candidate.
- Obstacle test independent of positions (E5): the first blocked tile of each item, its contiguous stretch along the row, lateral room on either side at 1, 2 or 3 rows. Row-run stretch widths: median 9, 1 to 82 (16 of 48 are 1 to 5 tiles wide, 16 are 51 to 82, i.e. the whole box is across water). Only 3 of the 29 fixtures have every item a row run with lateral room (arctic-hilly-1, -11, -15); a 2- or 3-row shift adds none.
- Trace time: median 0.15 us per edge, mean 0.5 us, max 8 us (a run with 214 combinations before the cover filter: 47 us).

## E4 negotiation with the full solve (placement + levels + render + signals), best first
- R7 (placing a Shift pair) is not merged, so the edits the machinery can place today are: node spots (x floors, best 3 per failing node), S-bend m +/- 1, joiner_div corner +/- 1, port offset +/- 1, row gap +1 at each of the 5 stack boundaries (y floors; hack on the facts overload). Order: node spots, then m/corner/port, then row gaps; singles first, then pairs of different targets.
- 20 full solves per fixture: 2 of 29 become plannable (arctic-hilly-1 by port offset +1 after 1 solve, arctic-hilly-15 by port offset +2 after 13 solves): 21 of 48 against 19. Uncapped over all singles and pairs (14 to 26 edits, 94 to 331 assignments, with row gap +2 and port offsets +/- 3 added): still 2. The cheapest assignment passes 1 of 2 times (arctic-hilly-1); INFERRED for the rest.
- Cost per fixture: a failing full solve ends in the X phase and costs about 0.25 ms; a passing one 9 ms. 20 solves: mean 7 ms, max 18 ms (arctic-hilly-15, 13 solves ending in a pass). Each failure's first reason is the same blocked-tile chain; no assignment failed in a different stage.
- What the realizable edits cannot do: the other 27 fixtures keep the blocked row run of the in or out leg, which only a Shift, a node move to another row, a longer edge detour or the frame can change.

## E5 what remains (after E4's 2)
- Port tile itself on a blocked tile at the flat offset: 19 of 29 (ports sit on the box boundary; only the box position, D197's cursor offset, moves them).
- Same choices, box moved by up to 8 columns / 4 rows: 12 of 29 plan (radii 2,2,2,3,3,3,3,4,4,7,7,8), among them 7 of the 19 with a blocked port tile, and all 3 shiftable-row-run fixtures.
- Groups: frame shift plans it 12; no plan within radius 8 and a port tile blocked 12 (water across the whole 41x14 box and wider: 51 to 82 tile wide stretches; these need a bridge or tunnel or are simply not buildable); 5 with nothing but obstacles in the interior (a blocked stretch 1 to 11 tiles wide on the in or out leg's row with no lateral room, or a column or bend obstacle).
- Not tried: node move to another row, a tunnel edit, detours of whole legs, Shift pairs on the in/out legs placed by R7 (the geometry says 3 candidates exist, all of which a frame shift of 2 to 7 also fixes).

## Verdict
Snap-and-negotiate as §17 describes it (node spots, Shift pairs, m +/-) is small and cheap (about 3 failing items per fixture; 1-21 node spots; microsecond traces; tens of ms for 20 full solves) but buys little on these fixtures: at most 3 more fixtures plannable by Shift (INFERRED, R7 absent) and 2 measured by port offset, against 12 by moving the box. The failures are mostly water under the in and out legs and the ports, not a shape problem.
