# PSG 197 gaps: what psg197.txt and psg197-decisions.md cannot express, or simplify

Check method: the C++ parser was not run (no code run). `ParsePlanarTopology`'s routes, paths, must-cross rule and greedy crossing choice were re-implemented in a scratch script from `planar_topology.cpp` and run on psg197.txt: 101 lines, 62 node statements, 16 routes, 48 paths, 352 must-cross path pairs, 47 crossings derived, 8 path pairs left uncovered. Every gap below is read from that run or from the trace tiles.

## 1. The notation refuses the hub (8 must-cross path pairs have no line pair)
- Evidence: path pairs of six route pairs: C1 x C2, C1 x C3, C2 x C3 (in lanes E174, E173, E169 to N), D1 x D2, D1 x D3, D2 x D3 (to S). For example C1 to NO2 (E174 to N85) and C2 to NO1 (E173 to N84) share no line on their separate tunnel tracks, their ring ends interleave, so they must cross; but every line pair that could carry that crossing also carries other path pairs that need not cross (the lines carry choice alternatives of the same routes, and merged streams). `ParsePlanarTopology` would return "routes C1 and C2 must cross but no two of their separate lines can".
- Per-lane-pair routes (37 routes, one per in lane and out lane) give the same 8 uncovered pairs, so the refusal is not caused by choice routes.
- The hub's actual crossings are tunnels under tracks (psg197-decisions.md): 112 separator x crosser line pairs from tile evidence, only 10 equal to a pair the notation derives (47 derived). The derived set covers each must-cross path pair once, by greedy; the tiles cross the same path pairs on different line pairs.
- Model needs: a crossing between lines that carry several paths of one route (a choice route's alternatives crossing another route's), or crossings authored per path pair, or authored beside the topology (Structure sidecar giving the line pairs).

## 2. Crossings the notation cannot hold
- Same-route crossings (7 line pairs): the two lines carry a common route, so a crossing would cross a route with itself (a choice route's alternative tracks, or a merged stream over its own source): E23 x NO1, C12 x C23, E2 x E2b, F12 x BF1, D12c x D23c, D23e x D123, D12d x D12e. The notation drops any line pair whose path pair has equal routes.
- A port line over a structure: E23 crosses tunnel (85,152) to (85,149) (middle tiles 85,151 and 85,150) under NO1, the N84 out port line. Port lines never cross in the notation.
- Crosser pieces on a node tile are assigned to the line ending there (approximate, affects at most the line name, not the tile).

## 3. Choice by tunnel availability
- The notation has choice routes (one route, several paths) but not the mechanism: 25 of 50 tunnel entrances carry an exit pre-signal 1-3 tiles before the head, fed by an entry signal on the line before the split; 22 later-stage entrances carry block signals; 3 path signals. The tunnel table lists each entrance's signal. The choice is made at the first stage only.
- Model needs: a flag on a split saying its legs are chosen by pre-signal (free leg wins), so Signals places exit pre-signals on the leg entrances and an entry signal before the split; and the throughput reading "a bundle of N tunnels is an N-times bridge".

## 4. Lane counts that change inside the hub (3 -> 2 -> 3)
- The hub merges three lanes of an in arm into fewer tracks after the direction split, then splits again by out lane (anatomy reading 3). In psg197.txt this is cascaded 2 -> 1 merges and 1 -> 2 splits; lines carry route sets (D123 carries D1+D2+D3 on one line). A line has no track count, so a line carrying three routes is not told apart from one carrying one; and a merge is all-or-none for a route, so "two tracks carry three routes" is not stated.
- Model needs: a lane count per line (the number of parallel tracks), and merge/split nodes with a track count per leg.

## 5. Tunnels in series on one line, free lengths landing at their joins
- Lines carry up to three tunnels in series with different lengths and no node between: A2b (92,161) 12 middle tiles, (92,176) 2, (92,181) 1; A2c (93,161) 12, (94,176) 7, (94,187) 2; A1d (97,158) 13, (97,174) 6, (97,183) 3; A1e (98,158), (98,174), (98,183) the same; F3 (102,168), (95,169), (88,171); the full list is in the line index. Each tunnel is its own structure with its own crossers; the notation has crossings per line pair, not a structure per stretch of a line, and no statement that a tunnel's exit lands where the next join needs it.
- Middle tiles reach 13 (the x 95-97 group, y 158 to 172) and 12 (the x 91-93 group, y 161 to 174); head to head 14, the engineer's "15 tiles counting both heads".
- Model needs: tunnel length a free variable per structure bounded only by `max_tunnel_length`, and the Lengths layer letting an exit land at its join (SPEC-structure group limits do cover the bound).

## 6. Two tunnels feeding one out lane (the suspected error, reading 7)
- Expressed as a split with both legs to S90: line pair A1d and A1e (tunnels (96,158) and (97,158), then (97,174) and (98,174), then (97,183) and (98,183)) are two paths of route A1 to SO1; route A1 has three paths (one to S89 through A1c, two to S90). Representable, but nothing marks it as a duplicate lane; N91 reaches no S88.
- Model needs: a check (rules or throughput) that flags a route with two paths to one exit lane that share no line.

## 7. Direction of travel is a superset
- The trace's reach table is a superset (unsignalled track passes both ways). Orientation of the piece graph for psg197.txt: an edge is forward when it is a shortest-path step from some in lane; 1180 of 1185 piece pairs oriented that way, 4 conflicts and 1 unoriented. The 4 conflicts are the pairs 100,165l with 100,164y and 100,164r, and 102,166r with 101,166x and 101,166l. Reading taken: both are real merges of different in lanes (100,164y carries N91, 100,164r carries S81, path N91 to E167 vs S81 to E167 in paths_all.txt; 101,166x is S82's track). Other reading: events.txt lists them as "via 2" copies of one lane (M 100,164, M 101,166). The tiles support the merge reading.
- The five missing in/out pairs (N90>E167, N91>E166, N91>S88, E169>S90, S83>E164) are structural and appear as missing paths.

## 8. Short parallel pairs (copies not over a structure) and stubs, dropped
- 24 core pieces in six groups lie off the oriented graph: (a) 100,160r 100,161u 98,159x 99,159r 99,160l on B1b (N91 to E, the path-signal cluster at 98-100,158-162); (b) 110,171r 110,172u 109,172l on D3, over the middle tiles of tunnels (111,171) and (111,172), so a copy of D3 over group G18 (recorded in the decisions file); (c) 100,182d 100,183l 101,181d 101,182u 102,181r on D12d; (d) 100,180d 100,181u 101,180r 99,181d 99,182l on D12f; (e) 102,184l 103,183d 103,184u 104,182r 104,183u on D12e; (f) 103,170r on C3. Groups (c) to (e) sit in the E to S tail (99-104,180-184), group (f) on E169 to N at 103,170-171, neither over a structure.
- Not in the topology because the notation has no copy and these are not over a crossing (except (b)). The trace's "5 doublings" and the events.txt "par" forks are these and my lane splits (114,172; 120,174; 116-117,173), which are not copies: their legs end at different merges.
- Model needs: a copy authored per line over a stretch (D351), including a short pair that forks and rejoins within 1-3 tiles outside any structure (a capacity feature, no crossing).

## 9. Prio is per route, the hub's is per merge
- Six merges have one entry-gated leg: 86,151r (C23 gated, CE123c holds), 98,171l (C2b gated), 107,173x (D2b gated), 100,174d (C2 gated), 110,176x (D2 gated), 94,195u (A2c gated). The other 25 merges are block/block (plain). Routes on the holding legs: C1, D1, D3, A1, E1, E2, E3 (never gated) are `prio`.
- C2 (gated at 86,151r, 98,171l, 100,174d, holds at 86,151r on its other path), C3 (holds at 98,171l, gated at 86,151r) and D2 (gated at 107,173x, 110,176x; holds at 94,195u) both hold and yield, so they are not `prio` and lose their holds; A2 yields at 94,195u only but is not marked.
- Model needs: prio per (route, merge) or per (path, merge), not per route.

## 10. Retrofit path-signal clusters
- Path signals (12, two path_oneway) form three clusters: N91 to E167 (98-100,158-162), E173/E174 to S90 (99-101,181-190), E174 at 109,174-175 with two-way combo/exit pieces at 102,174-175. They are lines and nodes in psg197.txt like the rest; their signal type (path, two-way pieces) is not in the notation.

## 11. Frame, ports and sizes
- The save's sides are kept (N, E, S arms); picture names, not the engineer's inferred names (anatomy says the engineer's "right" is probably the S arm and "bottom left" the E arm).
- E173's direction split is on the edge cell (128,173): its port line CD2 has length 0; the layout program wants at least 4 units of port line. E174's split is 5 tiles in, N90's 11, N91's 13.
- Port lines of some lanes enter the hub inside the box edge (E164 is the y 163 line after an S-bend, E173 shifts to y 172); slots are not part of the topology.

## 12. Readings with two choices
- 3-way split of N90 to S at 92,158y (S88, S89, S90 legs leave the same tile): written as a split of S90 first, then S89 and S88 (line A2m carries the rest); the other order (S88 first) is as good. The tile shows no order.
- Left to right order of the legs at each node is read from the tiles around it (side of travel); no node fell back to piece order (one node, 105,179u, needed a longer look: its legs are a one-piece leg to 104,179d and a six-piece leg to 101,181u).
- Cascade merge order, e.g. the 3 -> 1 into D123: written as D12c with D23c at 95,180d as the tiles show; the engineer's "3 -> 2" reading is the same merges seen by track count (gap 4).
- Choice scope: a route's paths are every walk through the lines carrying it; one tunnel track per out lane is assumed (the trace reaches 37 lane pairs, a superset).
