# O4x findings: why the uninformed line search returns a costlier route

Investigation only. All instrumentation was temporary and is reverted. Source: `wip-o4h2` (095dcd9d7b), Release build.

## 1. A failing case
`LL_RR`, start (40,20) X_NE, goal tile (34,18), trackdir 3 (Lower_E), `turn_cost` 3 (BaseRequest of the test).
- informed (heuristic on): cost 251, moves `RFFFRFRFFFRFRFFFRFFFFFRFFFFFRFRFFF` (34 moves)
- uninformed (h = 0): cost 259, moves `RFFFRFRFFFRFRFFFRFFFFFFFRFFFRFRFFFFF` (36 moves)

Many more fail, all LL_RR (the test's first failing ones: goal 34,19 td 2: 255 vs 263; 33,19 td 0: 270 vs 278; 33,19 td 3: 259 vs 267; 33,20 td 2: 263 vs 271). Always informed < uninformed. The run was cut by a timeout after six cases, so no total count.

## 2. The informed route is legal
Two checks, both passed: `CheckRoute` on it (its `RasterizeJunctionLine` result has no raster error, no track used twice, no `TracksOverlap`, inside min/max, ends on the goal), and a request with `prefix = informed moves` and `use_heuristic = false` was accepted by the router (route found, no "prefix breaks the curve rules"). Informed g = 251 is therefore a legal route the uninformed search should have found at cost <= 251.

## 3. Where the uninformed search lost it
The instrumentation ran the uninformed search with the informed moves as target and caught the first push of a prefix of it that `best` rejected (`junction_line_router.cpp:427`, `it->second <= entry.g.base()`).

Rejected node: moves `RFFFRFRFFFRFRFFFRFFFFFRFFFFF` (28 moves), g = 201.
Holder of the same key: moves `FRFFFRFRFFFRFRFFFRFFFFFFRFFF` (28 moves), g = 201 (a tie, the earlier push wins).

Every key field is identical: track 0 last (44,17) trackdir 13; all four tracks u=4 hand=2 prev_gap_ok=true pair=false; last pieces (44,17,13) (43,18,12) (42,19,13) (41,20,12); z all 0; after_bridge false; passed 0. Equal members of `LineNode` (`last`, `states`, `z`, `after_bridge`, `passed`): all identical. Equal g.

The only difference is the tiles the path has occupied (`path_used`, built in `Run` at the expansion, lines 462-479, from the path's `added` pieces): 149 tiles for the rejected node, 155 for the holder, and they differ in many tiles, e.g. the holder has (38,20) = X only (bits 1), (41,20) = 16, (43,18) = 16, while the rejected node has (38,20) = 8,... (the Upper/Lower/Left/Right tracks of its sweep), (42,20) = 48, (44,18) = 48. The two paths are shifted by one tile where they curved, so they leave different tiles used behind them.

Why it matters: the informed route's remaining 6 moves (`RFRFFF`) take track 3 over (38,20) as an Upper_E piece (trackdir 2), then (37,20) trackdir 3 and (37,21) trackdir 2. Replaying the informed continuation on the holder's tiles (rasterising the holder's moves and testing each later informed piece): track 3 piece 36 at (38,20) trackdir 2 overlaps the holder's X track there (`TracksOverlap`), and pieces 37 and 38 at (37,20) and (37,21) repeat tracks the holder already holds. So the holder can never complete that continuation, while the rejected node can. The holder then went on and reached the goal only by a longer route (259).

## 4. The missing state
`path_used`, the set of tracks (TrackBits per tile) the line itself has already built on the path. It is not in `Key` (`junction_line_router.cpp:252-261`) and not in `LineNode` (lines 37-43); it is rebuilt only from the tree at expansion (462-479).

It is read by:
- legality: `Occupy(used, piece)` at `junction_line_router.cpp:504` (F/L/R moves), `:561` and `:624` (tunnel/bridge heads), and `used.contains(packed)` at `:556` and `:619`; `Occupy` itself (`:327-333`) fails on a repeated track or `TracksOverlap`.
- cost: `if (!used.contains(PackTile(...))) cost += TerrainCost(...)` at `:501` (a tile the line already holds is charged no terrain cost again; also `:400` for the start pieces).

(No terrain in this failing case, so here it is legality only.) Bridge history (`path_bridges`, `BetweenBridgeHeads`, `UnderCrossingTunnel`) is also outside the key but was not involved in this case.

## 5. Cost of adding it to the key
Temporary key = existing key plus the sorted `path_used` (tile and bits), computed for every pushed node. Benchmark cases of `route cost benchmark` (8 cases, `max_expansions` 200000):

| | informed expansions (sum) | uninformed expansions (sum) |
|---|---|---|
| current key | 52,026 | 180,825 |
| key + path_used | 1,067,049 | 1,600,008 |

With the current key every case finishes (no_route_1 reports "no route" in 32,645 both ways). With the extended key only `flat_straight_1` (43, same as before) and `flat_turn_1` (1,829 vs 904) and `hills_turn_1` (65,172 vs 2,125) finish informed; the other five informed runs and every uninformed run (8 of 8, including flat_straight_1: 200,001 vs 40,656) hit the 200,001 search limit, so the sums above are floored by that limit and the true cost is larger. Exact `path_used` in the key removes the pruning almost entirely: it is not practical as is. Costs of the finished informed cases are unchanged (263, 233, 255).

Not done (outside scope): an approximation of the used set (for instance only the tiles within reach of the goal) or a dominance test instead of an equal key.
