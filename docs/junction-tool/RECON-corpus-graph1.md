# RECON G1: PSG hubs from the track graph

Untracked. `D = /mnt/c/Users/micro/OneDrive/Documents/OpenTTD-junction-tool/scratchpad/psg`; code and data in `D/graph1/` (`g1.py` builds, `tables1.py` tabulates, `spot.py` draws; output `hubs.jsonl`, `stats.json`, `stats_crossplain.json`). Result: **the spot check failed (5 of 10 hubs cut wrongly, 6 counting one doubtful), so I stopped as ordered.** The numbers below are real but describe a T=16 segmentation that chains hubs. Do not give G2 these hubs as final.

## Method and deviations
- Loader, World (pieces, `across`, signal votes, parity directions) are census2's. Node = tile with 2+ track pieces that is not exactly X+Y, Upper+Lower or Left+Right; X+Y tiles and rail/road level crossings are grade crossings (counted, not nodes). Vertex = (node tile, tile edge). 178 saves, 177k nodes' worth of walks, 3 min on 24 cores.
- Edge = walk from a vertex along plain pieces, through bridges and tunnels via far_end, to a node, a station/waypoint/depot tile (terminal), or an open end. Per edge: length (tiles between the ends, span interiors included), signal count, bridges/tunnels, grade-crossing tiles, crossing count (rail tile under a bridge span, edge tile above a tunnel or under a span), direction.
- Direction: the edge's own signals if present ("sig"), else census2's parity inference with the component sign taken from votes in a 3x3 (then 5x5, 9x9) block of 32-tile cells around the piece ("inf"; deviation: C1b used votes inside the hub box plus 20 tiles). Conflicting signals give "bi".
- Cut rule, deviation: "plain" cannot mean "no crossing" (a 60-tile double line with one bridge crossing then chained a 125x247 "hub"; that was my first run, 16363 hubs). Final rule: cut an edge when its longest crossing-free run is at least T tiles, or when it ends at a station, waypoint or depot. Open-end stubs that qualify are cut but give no port. The crossing-blind variant (cut any edge of length >= T) gives 40194 hubs at T=16 (`stats_crossplain.json`).
- Hub = component of nodes over uncut edges with 2+ nodes. Bounding box = hub nodes, uncut edge tiles and uncut bridge spans. Lanes: for each cut edge end at the hub, the tile where the walk first leaves the box (or the terminal end) and the edge it leaves by; side by SIDE map, as C1b. Ports, bundles and the gap between direction groups are census2's `make_ports`; classification is census2's `classify` unchanged, fed with these lanes.
- Clean = C1b's flags (no lane without direction, no bidir lane, no topology contradiction, no signal conflict) plus no terminal port. Terminal ports are cut edges into stations or depots, so the hub is a station throat. `st_in_bbox` (station or depot tile inside the box) is recorded but does not unset clean (33 clean hubs carry it).
- Through lines per hub: opposite-side port pairs with a movement either way, then a maximum matching per axis.
- Not done: no per-hub tool-type check beyond C1b; no T=10/24 hub records (counts and node sizes only); no by-eye check of more than 10 hubs.

## Hub counts (nodes with 2+ per component; single-node components apart)
| T | hubs | single-node | median nodes | p90 | max | >=100 nodes | >=300 |
|---|---|---|---|---|---|---|---|
| 10 | 46273 | 30144 | 3 | 16 | 900 | 171 | 8 |
| 16 | 29151 | 12766 | 4 | 28 | 1356 | 524 | 54 |
| 24 | 19648 | 7011 | 5 | 40 | 2100 | 754 | 150 |

## Size at T=16 (29151 hubs)
- Nodes: <=2 7970; 3 4652; 4-5 5007; 6-10 4819; 11-20 2858; 21-50 2346; 51-100 986; 101-300 459; >300 54.
- Longest box side: <=5 7252; 6-10 5561; 11-20 7501; 21-40 5226; 41-80 2712; 81-120 579; 121-200 284; >200 36. So 320 hubs exceed C1b's 120 limit (C1b: 277 mesh-large boxes), and the 5 spot-check failures are all in the 6-40 node range, not the giants.
- 11254 hubs hold a bridge, 6250 a tunnel, 4617 a grade crossing. 20684 have terminal ports; 7946 have neither terminal ports nor a station in the box.

## Type table at T=16 (C1b Table A: 1418 boxes, 43 two-way classified, 21 clean, 7 clean axis tool types)
| type | n | clean | axis | diag |
|---|---|---|---|---|
| terminal ports (station or depot throat; not classified for clean) | 20684 | 0 | 7899 | 357 |
| unclassified: no through movement | 3126 | 0 | | |
| 1way main (not a tool type) | 2025 | 1475 | 1538 | 24 |
| unclassified: no branch | 1436 | 0 | 1324 | 3 |
| unclassified: no lanes (closed or open-ended cluster) | 784 | 0 | | |
| unclassified: lanes on the main sides outside the through line | 442 | 0 | 175 | 19 |
| unclassified: main-line end holds several ports | 310 | 0 | | |
| mesh | 168 | 113 | 57 | 35 |
| both-sides | 57 | 3 | 15 | 0 |
| two-way main, ends differ | 51 | 11 | 16 | 0 |
| two-way main, one side, ends equal | 36 | 14 | 14 | 0 |
| unclassified: other | 32 | 0 | 12 | 0 |
- Clean total 1616 (1245 axis). Clean `L_R x L_R`: 7 (3 axis); there is no clean axis `LL_RR` type and no `T`. The two-way tool types remain rare (about 90 two-way hubs in 29151); the gain over C1b is in one-way hubs (1475 clean) and `mesh` (113 clean), which is what the graph segmentation finds that the box finder merged away.

## Multi-line hubs (2+ through lines; 358 hubs without terminal ports)
- Through lines, all hubs: 0 lines 10594; 1 line 16603; 2 lines 1620; 3 lines 206; 4+ 128. Without terminal ports: 0 lines 3915; 1 line 4194; 2 lines 334; 3 lines 21; 4+ 3. Clean: 1503 with 1 line, 113 with 2.
- Port patterns (ports per side; median 14 nodes): `W1 E1 N1 S1` 140 (mostly four one-lane one-way ports `i1|i1|o1|o1`: 70 of the 358, i.e. two single-lane lines crossing or touching); `W1 E1 N2 S1` 14; `W1 E2 N1 S1` 12; `W1 E1 N1 S2` 12; `W2 E1 N1 S1` 11; `W2 E2 N1 S1` 9; `N2 S2` 8; `W2 E2` 6 (two two-way lines side by side). Port strings with a two-way line (`i1_g_o1`) are few.
- The multi-line count is inflated by the failure below: two hubs chained by a short link often show as 2 lines.

## Spot check (10 hubs, 5 former mesh-large; ASCII from `D/graph1/spot.py`, files `pics_<id>.txt` for the first run only)
Rule: the extent is wrong if the picture shows two junctions joined only by a plain link or a crossing corridor.
| hub | origin | verdict |
|---|---|---|
| psg241_h81 (14 nodes) | mesh-large | right: one diagonal staircase |
| psg160_h56 (17) | mesh-large | doubtful: ladder + 10-tile double track + ladder |
| psg261_h20 (32, 36x44) | mesh-large | wrong: 2 to 3 hubs chained by 8 to 10-tile links and bridges |
| psg121_h51 (17) | other | right: one fan |
| psg136_h68 (7) | other | right, but its 7 feeder lines of 12 tiles are included (T=10 would trim them) |
| psg173_h265 (13) | mesh-large | right: one diagonal chain with bridges |
| psg290_h38 (8) | other | wrong: two lobes joined by a 9-tile double line between two crossings |
| psg331_h26 (14, 37x40) | other | wrong: three clusters on a 15-tile corridor |
| psg4_h89 (7) | other | wrong: ladder plus a junction 12 tiles on |
| psg307_h16 (8) | other | wrong: two groups joined by a 10-tile double line |
- 5 wrong, 1 doubtful, 4 right. Cause: T=16 does not separate hubs whose spacing is 8 to 15 tiles, and a crossing in a link resets its plain run, so crossing corridors never cut. T=10 would cut about three of the five (links 10 to 15 tiles) but not the two with crossings in the link. No fix was tried (stop rule).
- Suggestion for G1 re-run (needs your decision): cut at T=10 and also cut a link when each side of it carries 2+ nodes and the link has no node, whatever its crossings, or segment by "cut at every plain stretch longer than the longest gap inside a known clean hub".

## File formats
`D/graph1/hubs.jsonl`, one JSON object per hub (T=16 only, 29151 lines, 240 MB). Coordinates are map tiles (x increases to screen SW, y to SE). Tile edges are NE, SE, SW, NW.
- Scalars: `id` (`psg<save number>_h<k>`), `save`, `bbox` [x0,y0,x1,y1], `size` ("WxH"), `n_nodes`, `n_edges_int`, `n_edges_cut`, `n_bridges`, `n_tunnels` (inside uncut edges), `n_gc` (grade crossings), `open_stubs`, `cut_inside` (cut edges that never leave the box, no lane), `term_ports`, `st_in_bbox`, C1b fields `type`, `clean`, `axis`, `ml`, `branches`, `reasons`, `detail`, plus `through_lines`, `n_ports`, `n_lanes`.
- `nodes`: [x, y, [track names on the tile], is_grade_crossing_too].
- `edges`: objects with `a` [x,y,tile_edge] (a node vertex of this hub or of the neighbour), `b` (same, or null for a stub), `kind` (node, station, wp, depot, open), `tile` (terminal tile or null), `len`, `cut`, `dir` (f = a to b, r = b to a, bi, null) with `dsrc` (sig, inf, none), `nsig` (signals on the edge), `br` ([kind,x,y,far_x,far_y] bridges and tunnels), `gc`, `cross`, `path` (flat x,y list of the plain tiles, at most 80). A cut edge between two hubs appears in both. An edge with `cut` false has all its ends in this hub.
- `lanes`: {id, tile, e (the edge it crosses the box border by), side (W E N S per census2: NE=W, SW=E, NW=N, SE=S), dir (in, out, bidir, null), trk, how (node or the terminal kind)}.
- `ports`: {side, groups [{d: in/out, n lanes, lo, hi positions}], gap (tiles between the two direction groups, null for one group), lanes [ids], term (bool)}.
- `moves`: [in lane id, out lane id] pairs following the one-way direction inside the hub.
