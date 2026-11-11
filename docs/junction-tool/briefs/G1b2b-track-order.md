# G1b(ii-b): the order of a track graph — node levels, row orders, crossings along each edge (DESIGN-G1 §8; D189, D204)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (persistent; Debug `build`, Release `build-rel`). First: `git checkout -B wt-g1b2b <HEAD>` (given at launch). You are the only writer there. `tools/junction/jt` builds openttd_test before running.
- G1b(ii-a) (merged) added JunctionTrackGraph / BuildJunctionTrackGraph to src/junction_topology.h/.cpp. This step adds the order of that graph: where each node sits north to south, the west-to-east order on each main-lane row, and the crossings along each edge in travel order. A pure function with its own tests; nothing existing calls it; nothing existing changes.

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ part); docs/junction-tool/DESIGN-G1.md §7-§8.
- src/junction_topology.h (all) and src/junction_topology.cpp (all); src/tests/junction_topology.cpp (the track-graph tests: they list both graphs' nodes and edges, which this brief's expectations index).

## Design (settled)
Add to junction_topology.h/.cpp after BuildJunctionTrackGraph:
```cpp
/** A node, or an edge crossing the lane, at its place along a main-lane row. */
struct JunctionTrackRowItem {
	bool node; ///< index is into JunctionTrackGraph::nodes, else into its edges (an edge crossing this row).
	uint32_t index;
	bool operator==(const JunctionTrackRowItem &) const = default;
};

/** The order of a track graph (DESIGN-G1 §8). */
struct JunctionTrackOrder {
	std::vector<int> node_level; ///< Per node: 2r+1 on row r; 2b in band b (band b lies north of row b; band R south of the last row).
	std::vector<std::vector<JunctionTrackRowItem>> rows; ///< Per row, north to south: its nodes and the edges crossing it, west to east.
	std::vector<std::vector<uint32_t>> edge_crossings; ///< Per edge: the edges crossing it, in travel order.
};

/**
 * The order of a track graph by a sweep from the branch side (DESIGN-G1 §8).
 * @param paths ExpandJunctionLanes(topology).
 * @param graph BuildJunctionTrackGraph(topology, paths).
 * @param error Receives the reason when the result is empty (no rows).
 */
JunctionTrackOrder OrderJunctionTrackGraph(const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, std::string *error);
```
Rules, exactly (slots = JunctionPortSlots(topology), n = slots.size()):
1. **Branch side.** Every slot whose group is not main must be on the S side; else error `"branch ports off the S side are not ruled yet"`.
2. **Rows.** The main slots on the W side, in slot order, are rows 0, 1, ... (R = their count). Every other main slot takes the row of the W-side slot it shares a path with where both ends are main (a through path); none: error `"a main slot without a through partner is not ruled yet"`. A row is eastbound when its W-side slot's group is `in`, else westbound.
3. **Levels.** Row r: 2r+1. Band b: 2b (b = 0..R). An end of an edge: a port slot that is main -> its row's level; a non-main (S) port -> 2R+1; a node -> node_level. node_level of a node: its slot is main -> its row's level; else take the far slots of the node's paths (the paths of the edge between the node and its parent... simpler and equivalent: of the edge with the most paths that has an end at this node): if they lie in more than one group -> 2R; else -> 2 * (1 + the largest row among that one group's slots).
4. **Row edges and crossing edges.** An edge whose two end levels are equal and odd is a row edge (it lies on that row). Every other edge's "north end" is the end with the smaller level, its "south end" the larger.
5. **Band crossings.** For each (i, j) in graph.crossings where neither edge is a row edge: its band level is the largest even L with L between the north and south end levels (inclusive) of both edges.
6. **Sweep.** items = for each S-side slot in slot order: the edge with that port slot as an end (exactly one: its trunk, or its only path's leg). For L = 2R down to 0:
   - L even (a band): repeat until none: take the leftmost item whose north end is a node at level L; replace it, in place, by that node's other edges (every edge with an end at the node, except the item), ordered west to east by DESCENDING key, key(edge) = the least (far(p) - node.slot + n) % n over its paths p, where far(p) = paths[p].to for a split node, paths[p].from for a merge node. Then for each band crossing at L, in sorted (i, j) order: the two edges must be adjacent items (else error `"a crossing between edges that are not adjacent is not ruled yet"`): swap them, and record each as crossing the other at this point of the sweep.
   - L odd (row r = (L-1)/2): walk items west to east: an item whose north end is a node at level L: append {true, that node} to rows[r] and remove the item; any other item: append {false, item} to rows[r] (it crosses row r here).
   After L = 0: items must be empty; else error `"the order does not realise the crossings"`.
7. **edge_crossings.**
   - Row edge on row r: the {false, e} items of rows[r] strictly between the positions of its two ends (a port end on the W side = before the first item, on the E side = after the last), in travel order (eastbound: west to east; westbound: east to west). Each such e also records this row edge as crossed at the sweep point where it crossed row r.
   - Crossing edge: what it crossed, in sweep order (band swaps and rows as met), reversed when the edge travels from its north end to its south end (its `from` end is the north end).
8. **Check.** The set of pairs (min, max) over all edge_crossings equals graph.crossings; else error `"the order does not realise the crossings"`.

Worked example (LL_RR, band 8): items start [e2 (P4->N2), e21 (N9->P5)]. e2's north end N2 is at level 8: N2's other edges e3 (J_e: least key (8-4)%10 = 4) and e4 (J_w: 8): descending -> [e4, e3, e21]. e4's north end N4 (level 8) -> Jw1 e17 key 8, Jw2 e18 key 9 -> [e18, e17, e3, e21]. e3's north end N3 is level 4: skip. e21's north end N9 (8): X_e e23 key 5, X_w e22 key 1 -> [e18, e17, e3, e23, e22]. e22 -> N10: Xw1 e13 key 2, Xw2 e14 key 1 -> [e18, e17, e3, e23, e13, e14]. Band crossing (3, 23) at level 8: adjacent, swap -> [e18, e17, e23, e3, e13, e14]. Row 3 (level 7): e18 ends at N8 -> node; e17, e23, e3, e13 cross; e14 ends at N5 -> node: rows[3] = [N8, e17, e23, e3, e13, N5].

## Tests (tag `[topology]`; record each red)
State every expected list literally. Node and edge numbers are those of the track-graph tests.
1. "the T's track order" (outer): node_level {1, 4, 3, 3, 4, 1}; rows: row 0 [N0, N5]; row 1 [N3, e4, e8, N2]; edge_crossings: e4 [5, 8], e5 [8, 4], e8 [4, 5], every other edge empty. (Matches the traced T: along W, westbound, N->E is crossed before A->S; A->S crosses W, then N->E south of W.)
2. "LL_RR with an L_R sideline: track order" (outer): node_level {1, 3, 8, 4, 8, 7, 5, 5, 7, 8, 8, 4, 3, 1}; rows: row 0 [N0, N13]; row 1 [N1, e11, e15, N12]; row 2 [N7, e23, e3, N6]; row 3 [N8, e17, e23, e3, e13, N5]; edge_crossings: e3 [23, 10, 9]; e8 [11, 15]; e9 [3, 23]; e10 [13, 3, 23, 17]; e11 [8]; e13 [10]; e15 [8]; e17 [10]; e23 [9, 10, 3]; every other edge empty.
   If your implementation of the rules as written gives anything different in test 1 or 2, do NOT change the expected lists: STOP and report both.
3. "branches on the N side are refused": a topology with groups in slot order W in 1 lane line 0 main, W out 1 lane line 0 main, E in 1 lane line 0 main, E out 1 lane line 0 main, N in 1 lane line 1, N out 1 lane line 1 (check JunctionPortSlots accepts that order: W, S, E, N; if it does not, STOP and report), movements as the T's mirrored (main throughs, an exit from each main in to N out, N in to each main out): ExpandJunctionLanes and BuildJunctionTrackGraph succeed (CHECK), OrderJunctionTrackGraph gives no rows and error "branch ports off the S side are not ruled yet".
Red: does not compile; then with a stub returning an empty order, tests 1-2 fail on their first CHECK (record the messages).

## Do not
- No rows inside the box beyond the lanes (prio rows, doubling copies, spreading), no bridges, doubling, turns or positions (later steps). Do not change BuildJunctionTrackGraph or anything it calls. Do not call the new function from the template builder, solve, generator or checker.

## Stop conditions
- Test 1 or 2 differs (report both). Test 3's topology not accepted by JunctionPortSlots or BuildJunctionTrackGraph (report).
- Anything the rules above leave undecided.

## Gates
`tools/junction/jt fast build "[topology]" 5`, `tools/junction/jt fast build-rel "[topology]" 1`, `tools/junction/jt gate build-rel`. Update the `[topology]` time in TESTS.md. Commit: `Add: Junction track order by a sweep from the branch side`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
References read; commit; red evidence; types/functions added; suites with times; stops; `git status --porcelain` and `git log -1`. When you have reported, stop.
