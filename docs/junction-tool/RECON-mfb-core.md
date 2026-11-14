# RECON: mfb's core (PSG 227) as bundles (2026-11-15, read-only)

1. **Crossings:** 9 of the 15 bundle pairs cross, every one by bridge or tunnel (no level crossing between bundles). They are exactly the pairs whose lane chords interleave (checked by script); no other pair crosses. Only a x f crosses twice (bridge 372 over, tunnel under: both a over f).
2. **Through pair:** NO, they do not cross each other. N and SW are the straight pair at the core (both on the Y axis): b (N>SW) and e (SW>N) run side by side. By box edge (E and SW opposite) the pair is d, f: also no crossing. The middle crossing is b over {a, f} and b over c, plus c over f: b>c>f, the same triangle as our T (5 over {8,4}, 8 over {4}).
3. **Widths:** every bundle has 2 tracks (fixed, choice) after its exit. Crossing width: b 4 (x365-368), c 2 then 4 (x360-363), e 2 then 3, f 2, a 1+1 apart, d 2 then 3. Only b and c cross as 4-bridge stacks; f's 2 tracks pass under three stacks as an adjacent pair.
4. **Does not fit "bundle core":** (a) a's two tracks run apart (fixed passes under b, choice goes round the NE and crosses nothing). (b) 6 of the 9 crossing pairs are exit-block or join-block crossings (same input arm: a-b, c-d, e-f; same output arm: a-f, b-d, c-e), only b-c, b-f, c-f are arm-forced core crossings. (c) the choice split of c sits mid-core (after its under-crossing of b, before stack A); f's after all of its crossings.
5. **Stack A is two stacks:** e over f (2 bridges x358-359) and c over f (4 bridges x360-363); B1 = b over {a-fixed, f} (4 bridges); B2 = b over c (4). All 19 bridges and the tunnel assigned (Q2).
6. **Core graph:** §6. Source: `paths_all.txt` (18 paths) plus the piece graph (`net.py`/`core.py`, read via importlib, no writes) for the second tracks of doublings; scripts and raw outputs in the scratchpad `mfb-core/{scripts,out}` (`br.txt`, `order.txt`, `seg.txt`, `map.txt`).

Naming: a=N>E, b=N>SW, c=E>N, d=E>SW, e=SW>N, f=SW>E (SW = the W lanes in `paths_all.txt`). Lanes: fixed N354 E344 SW360, choice N355 E345 SW361. "Exit" = after the direction split, "line split" = the choice track's FORK, "join" = first MERGE that takes another arm's tracks. Method: bundle = pieces reachable from the in arm and reaching the destination arm and from no other arm, in the 335,311-392,368 box (a 66, b 90, c 77, d 50, e 47, f 92 pieces). Over/under = which bundle owns the bridge heads (the export has no deck heights, only terrain heights). Level crossings: only X over Y on one tile counts; 7 tiles have it, each has a second piece that is on no in-to-out path (INFERRED unrelated/dead track), none is between two bundles.

## Q1. Core stretch, tracks, adjacency (per bundle)
Tiles are x,y. "to split" = end of the choice track's core part; the fixed track continues to its join.

| Bundle | First tiles (exit) | Last tile: choice line split / fixed join | Tracks and adjacency |
|---|---|---|---|
| a N>E | N355 362,325; N354 364,330 | 370,331 / E342 stage 2 at 378,339 | 2, **apart**: fixed runs x364 down to y334 then east along y335 (365-371); choice runs the NE diagonal (362,325 to 369,331, 5+ tiles from the fixed). After the split 3 tracks (370,332 / 371,332 and the fixed). No doubling. |
| b N>SW | N355 362,325; N354 364,330 | 367,331 / SW362 join 367,356-357 | **4 adjacent** at x365-368, y332-346: fixed N354 doubles at 365,330 (x365, x366), choice N355 splits at 367,331 (x367 to SW362, x368 to SW363). Rejoin of the fixed pair 366,346-347; the choice legs swap in a 4-in 2-out cross 367-368,346-347; then 3 tracks (x366, x367, x369). |
| c E>N | E344 378,344; E345 373,345 (after the cross 372-373,345-346) | 361,340 / N353 join 360,328-329 | 2 **adjacent** (y341 = E344, y342 = E345, x365-373, 9 tiles), then fan to **4 adjacent** at x360-363: E344 doubles at 363,340 (x362, x363), E345 splits at 361,340 (x360 to N350, x361 to N353). Rejoin of the E344 pair 362,330-331, joins 360,328-329 and 358,330. |
| d E>SW | E344 378,344; E345 373,346 | 370,348 / SW363 join 370,358-359 | 2 adjacent (diagonal pair 378-372,344-348), then 3 at x368 (E345 leg to SW362), x370 (E345 leg to SW363), x371 (E344 fixed); x368 and x370 straddle x369 where bridge 369 is. Fixed is not doubled. |
| e SW>N | W360 357,339 (x357); W361 363,353 | 359,342 / N350 join 356,328-329 | 2, **apart** until 359,342: fixed on x356-357 (y339-343), choice on the SE diagonal 364,353 to 360,343 (3-7 tiles away). After the split 3 **adjacent** columns x357 (fixed), x358, x359 over y333-338. Fixed has a 3-tile local doubling at 357,334-336 (two-way pieces), no bridge. |
| f SW>E | W360 357,338; W361 363,353 | 368,337 / E343 join 377-378,342-343 | 2 **adjacent** (y336 = W360, y337 = W361, x358-367, 10 tiles; approach is a diagonal pair 363,348 to 357,343). After the split: leg to E342 through the tunnel (y338), leg to E343 by x370 (370,338-341) round to 376,343. Fixed doubles at 375,340 / 374-376,340-342 (rejoin 376,342). |

No two tracks of a bundle merge before a crossing ("cross as one" occurs nowhere in the 18 paths: f's two tracks pass under A and B1 as two separate tracks on y336 and y337, c's two cross as four). The ANATOMY clarification (SW>E join before the core) is not in this data (that was rule (e), failed). "Doubling" (a fixed track splitting into two parallel tracks) occurs in b (365,330), c (363,340), e (357,334, local), f (375,340, after the crossings).

## Q2. Crossings between bundles (every bridge and the tunnel)
| Over | Under | Where | Form | Bridges (heads) |
|---|---|---|---|---|
| e (W361 choice legs) | f (both tracks, y336-337) | x358-359, y335-340 | **stack of 2** parallel bridges, heads staggered | 358,336-340; 359,335-339 |
| e (W361>N353 leg) | c (E345>N350 leg, 359,331-332) | 359,330-333, N join block | single bridge | 359,330-333 |
| c (E345 legs x360, x361; E344 pair x362, x363) | f (both tracks, y336-337) | x360-363, y334-338 | **stack of 4** (all of c's tracks over both of f's) | 360,334-338; 361,334-338; 362,334-338; 363,334-338 |
| b (N354 pair x365, x366; N355 legs x367, x368) | a-fixed (y334-335) and f (y336-337): 3 tracks under all four | x365-368, y333-338 | **stack of 4** (B1); spans 4,4,3,3, heads y333/333/334/334 | 365,333-338; 366,333-338; 367,334-338; 368,334-338 |
| b (same four tracks) | c (E344 y341, E345 y342) | x365-368, y340-344 | **stack of 4** (B2), span 3 | 365,340-344; 366,340-344; 367,340-344; 368,340-344 |
| b (N355>W363 leg) | d (E345>W362 leg, 369,349-350) | 369,348-351, SW join block | single bridge | 369,348-351 |
| a (N355>E343 leg) | a-fixed, a choice E342 leg (372,335-337), f (W360>E343 at 372,337-338) and the tunnel below 372,338 | 372,334-339, E join block | single bridge | 372,334-339 |
| f (W361>E342 leg, in the tunnel) | f-fixed (372-373,338) and a-fixed (373-374,338) above | 371,338-375,338, E join block | tunnel (3 tiles under) | tunnel 371,338-375,338 |
| E-arm trunk (E345's two pre-split tracks, heads on trunk pieces, no bundle) | d (E344's SW branch, 376-378,345-346) | 375-379,345-346 | doubled bridge (parallelogram, D pair), span 2 | 375,346-378,346; 376,345-379,345 |

Count: 4+2+1 (c, e) + 4+4 (b) + 1 + 1 (369, 372) + 2 (D pair) = 19 bridges, plus the tunnel. Nothing unassigned. The 2 D-pair bridges belong to the E arm's exit block (E344's SW branch dives under the two peeled E345 tracks); 359,330-333, 369, 372 and the tunnel are inside a join block (they sit after the line splits of both tracks they carry), as is the rest of the E join block interleave of a and f at x370-377, y335-343 (a Left and f Right pieces share tiles 371-374,335-341, parallel diagonals, no crossing).
Bridge 365 and 363 have no shortest path over them (0 in REPRESENT §4): they carry the second copy of a doubled fixed track. Stack A of REPRESENT (6 bridges x358-363) is two stacks: e's pair and c's four.
Bridge over tunnel: bridge 372 deck is above tile 372,338 where the tunnel runs below: a over f, same pair as the tunnel's own crossing.

## Q3. Order of crossings along each bundle (exit to join)
- a: fixed: under b (B1, y335, x365-368) -> under its own choice leg (372) -> over f's tunnel (374,338). Choice legs: E343 leg over {a-fixed, E342 leg, f} at 372; E342 leg under 372. So a: b, then (a x a, a over f) in the join block.
- b: over {a, f} (B1) -> over c (B2) -> N355>W363 leg over d (369). Order a,f / c / d.
- c: (exit block: E345 trunk over d at the D pair) -> under b (B2, 4 bridges, x368 to x365) -> over f (A, x360-363) -> E345>N350 leg under e (359). Order: b, f, e.
- d: (exit block: under the E trunk) -> E345>W362 leg under b (369). Order: b only, in the join block.
- e: over f (x358-359) -> W361>N353 leg over c (359,330-333). e-fixed crosses nothing. Order: f, c.
- f: under e (x358-359) -> under c (A) -> under b (B1, with a) -> E join block: under a's 372 and a-fixed above the tunnel. Order: e, c, b, then a.
Total events along the longest tracks: f 4 stacks (e, c, b, a), c 3 (b, f, e), b 3 (a+f, c, d), e 2, a 2, d 1 (plus exit blocks).

## Q4. Compare with our single-track T (RECON-bundle-copies Q5)
- Our T: 6 legs, 3 crossings {4,5} {4,8} {5,8}: the triangle of mutually interleaving chords, 5 over {8,4}, 8 over {4}.
- mfb at arm-group level (12 lane ports in cyclic order N350 N353 N354 N355 E342 E343 E344 E345 W363 W362 W361 W360; bundle chord = in group to out group): the forced (interleaving) pairs are exactly b-c, b-f, c-f. Same triangle. Its order: b over c (B2), b over f (B1), c over f (A): b > c > f. Map to the T: b = leg 5, c = leg 8, f = leg 4.
- At lane level 6 more pairs interleave on a single lane-chord pair (a-b 2, a-f 1, b-d 1, c-d 2, c-e 1, e-f 2 chord pairs) and all 6 cross in mfb (a-b in B1, c-d at the D pair, e-f at x358-359, a-f at 372/tunnel, b-d at 369, c-e at 359). Non-interleaving pairs (a-c, a-d, a-e, b-e, d-e, d-f) never cross. So 9 pairs cross, none that need not, none by more than one stack except a-f (bridge 372 over f's tunnel and a-fixed over the tunnel: one pair, two structures, same direction).
- Not in the T core: the 6 extra crossings are same-arm pairs (a-b, c-d, e-f: exit block, DESIGN §4.1 "l0b x l1a") and same-destination pairs (a-f, b-d, c-e: join block, DESIGN §4.1 "cA1 x cB0"). In the single-track T they do not exist (one track per bundle). So the arm-forced core of mfb = our T core with each leg 2 wide (b and c made 4 wide by doubling the fixed track and splitting the choice track before the crossing).
- Through pair: at the core N arm enters along the Y axis (x362-364, y325-330, heading S) and the SW arm leaves/enters along the Y axis (x363-364, y346-353, heading N), the E arm along the X axis (y344-345): so the straight pair is N<->SW = b and e, E is the T's branch. b and e do not cross; b runs x365-368, e x356-364, west of it. The engineer's "straight through lines cross in the middle" holds only in the sense that b (through) crosses the E traffic (a-fixed, f, c) in the middle (B1, B2); e (the other through line) crosses f and c at the west edge (y330-340, x358-359), not in the middle. By box edges (REPRESENT §0: E and SW opposite, N the branch) the through pair is d and f: no crossing either (d runs south x368-371 y347-358, f north y336-337). INFERRED: the engineer means the first reading.

## Q5. Where the choice tracks split, per bundle (relative to the crossings)
- a: split 370,331, **before all of its crossings** (its choice track crosses nothing before it); its crossings (372, tunnel) come after, with a-fixed already under b at B1.
- b: split 367,331 (N355) and doubling at 365,330 (N354), **before all crossings** (B1, B2, 369); a second, lane-swapping cross at 367-368,346-347 comes **after B1 and B2**, before bridge 369.
- c: E345 split 361,340 and E344 doubling 363,340: **between** B2 (c under b, as 2 tracks) and A (c over f, as 4 tracks), before the crossing at 359.
- d: split 370,348, **no crossing before it**; bridge 369 after it.
- e: split 359,342, **before all** crossings (f, c).
- f: split 368,337, **after all three stacks** (e, c, b: f runs as 2 tracks under them), before the tunnel/372 crossings.
So choice tracks split before the crossings in b, e (and a), after them in f, between in c, never in d.

## 6. mfb's core as our core graph
Legs = bundles, ports at the core edge are the exit (first tiles above) and the join (last). "Sides" use the box edges (N top, E right, SW left; ports x/y as in Q1).

| Leg | Width (core part) | From exit port | To join port | Crossings on it |
|---|---|---|---|---|
| b N>SW | 4 (2 fixed adjacent + 2 choice adjacent) | N arm, tiles 362-364,325-330 | SW, 366-370,347-358 (then joins 367,356 / 370,358) | over {a,f} stack of 4, over c stack of 4, over d (single) |
| c E>N | 2, then 4 at x360-363 | E arm 373-378,344-345 | N, 358-360,328-331 | under b (stack of 4), over f (stack of 4), under e (single) |
| f SW>E | 2 | SW arm 357-358,338 and 363,353 | E, 375-378,338-343 | under e (2), under c (4), under b (4 with a), E join: under a (372), tunnel under a/f |
| a N>E | 1 + 1, apart | N arm 362,325 and 364,330 | E, 377-379,338-339 | a-fixed under b (B1); join block: a over f (372), a-fixed over the tunnel |
| d E>SW | 2, then 3 | E arm 373-378,344-346 | SW, 367-371,356-358 | under the E trunk (exit D pair), under b (369) |
| e SW>N | 2 apart, then 3 adjacent | SW arm 357,338-339 and 363,353 | N, 356-358,328-331 | over f (2 bridges), over c (single) |

Crossings (pair, over, form):
1. b over c: stack of 4 parallel bridges (B2), x365-368, y340-344. Core, arm-forced.
2. b over f (with a-fixed): stack of 4 (B1), x365-368, y333-338. Core, arm-forced; the a-fixed track rides the same stack (3 tracks under 4 bridges).
3. c over f: stack of 4 (A), x360-363, y334-338. Core, arm-forced.
4. e over f: stack of 2, x358-359. SW exit-type (lane-forced).
5. e over c: single, 359,330-333. N join block.
6. b over d: single, 369,348-351. SW join block.
7. a over f: single bridge 372,334-339 over tile 372,338 and the tunnel; and the tunnel 371-375,338 under a-fixed and f-fixed. E join block.
8. a-fixed under b: part of crossing 2.
9. E trunk over d: doubled bridge D pair, 375-379,345-346. E exit block.

```
 N arm (down, x362-364)              . = bundle tracks; = / || = deck above
 e out <----+  c out<--+   [b: 4 tracks x365-368]  a-choice ----+ (NE round, 370,331)
 N350/353   |  359:e over c   |  |  |  |                          \
 y330   e-fixed x357   [c 4]  |  |  |  |  a-fixed x364->y335 ===>  372: a over tunnel/f
 y334-338  e2 over f  A: c4 over f   B1: b4 over (a-fixed y335, f y336-337)
 y336-337  f ================================================> split 368,337 -> E342 (tunnel)
                                                                    \-> E343 (x370)
 y340-344  c <=== c y341-342 (2) ============= under B2: b4 over c
 y345-348  d (E arm) ---- under D pair (E trunk over d) ----> d 3 tracks down x368-371
 y349-358  b legs x366,367,369 (369: b over d at 349-350)    joins SW362/SW363 (367,356 / 370,358)

 crossing graph:  b > c > f (core triangle, = our T 5>8>4)
                  b > a(fixed), b > d, e > f, e > c, a > f, E-trunk > d
 pairs that never cross: a-c a-d a-e b-e d-e d-f
```
Gaps: second tracks of doublings come from the piece graph, not from paths_all.txt (only shortest paths are listed there). Which of b's x365 and c's x363 bridges carries a real track (0 shortest paths over them) is INFERRED to be the second copy of the doubled fixed track from the anatomy's doubling list. Deck height is unknown (export has terrain height only); over = bridge owner.
