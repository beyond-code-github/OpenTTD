# G1b(ii-a): the track graph — split and merge trees, legs, crossings per edge (DESIGN-G1 §7; D187, D204)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (persistent; Debug `build`, Release `build-rel`). First: `git checkout -B wt-g1b2a <HEAD>` (given at launch). You are the only writer there. `tools/junction/jt` builds openttd_test before running.
- G1b(i) (merged) added src/junction_topology.h/.cpp: JunctionTopology, JunctionPortSlots, ExpandJunctionLanes, JunctionChordCrossings, JunctionTTopology, JunctionLLRRSidelineTopology. This step adds the track graph built from a topology's lane paths. Pure functions with their own tests; nothing existing calls them; nothing existing changes.

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ part); docs/junction-tool/DESIGN-G1.md §7.
- src/junction_topology.h (all) and src/junction_topology.cpp (all, 125 lines); src/tests/junction_topology.cpp (all, 94 lines: the existing tests and how slots/paths are stated).

## Design (settled)
Add to junction_topology.h/.cpp (after JunctionChordCrossings):
```cpp
/** One end of a track-graph edge: a port slot or a node. */
struct JunctionTrackEnd {
	bool node; ///< index is into JunctionTrackGraph::nodes, else into JunctionPortSlots.
	uint16_t index;
	bool operator==(const JunctionTrackEnd &) const = default;
};

/** A switch of the track graph: where an in slot's paths split, or where an out slot's paths merge (DESIGN-G1 §7; D187, D204). */
struct JunctionTrackNode {
	bool split; ///< In an in slot's split tree; else in an out slot's merge tree.
	uint16_t slot; ///< The port slot whose tree holds it.
	bool operator==(const JunctionTrackNode &) const = default;
};

/** A run of track between two ends in the direction of travel, carried by a set of lane paths. */
struct JunctionTrackEdge {
	enum class Kind : uint8_t {
		In, ///< In an in slot's split tree.
		Leg, ///< One path's own run, from its last split (or in port) to its first merge (or out port).
		Out, ///< In an out slot's merge tree.
	};
	Kind kind;
	JunctionTrackEnd from;
	JunctionTrackEnd to;
	std::vector<uint16_t> paths; ///< Indices into the lane paths, ascending.
	bool operator==(const JunctionTrackEdge &) const = default;
};

/** The track graph of a topology (DESIGN-G1 §7): switches, edges, each path's edges, and which edges cross. */
struct JunctionTrackGraph {
	std::vector<JunctionTrackNode> nodes;
	std::vector<JunctionTrackEdge> edges;
	std::vector<std::vector<uint32_t>> path_edges; ///< Per lane path, its edges in travel order.
	std::vector<std::pair<uint32_t, uint32_t>> crossings; ///< Edge pairs (i < j) that cross, sorted.
};

/**
 * The track graph of a topology's lane paths (DESIGN-G1 §7).
 * @param paths ExpandJunctionLanes(topology).
 * @param error Receives the reason when the result is empty (no nodes, no edges).
 */
JunctionTrackGraph BuildJunctionTrackGraph(const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, std::string *error);
```
Rules, exactly (n = number of slots; "far slot" of a path = its `to` in a split tree, its `from` in a merge tree):
1. **Split trees.** For each slot s ascending, S = the paths with `from == s`, in path index order. If |S| <= 1, no tree. Else Partition(S, level 0, parent = port s):
   - key at level 0 = the far slot's group (JunctionPortSlots[far].group); at level 1 = the far slot.
   - Keys are ordered by the least (far - s + n) % n among their paths (counter-clockwise from s).
   - One key: at level 0, Partition the same set at level 1 with the same parent. (At level 1 a key holds exactly one path.)
   - Two keys: append a node {split = true, slot = s}; append an edge {In, from = parent, to = the node, paths = the set}; then for each key in order: a one-path subset's in-leaf is this node; a larger subset: Partition(subset, the same level, parent = this node).
   - More than two keys: error `"more than two legs at one split are not ruled yet"` (empty result).
   A path whose in slot has one path has the port as its in-leaf.
2. **Merge trees.** Then for each slot s ascending, the same with `to == s` and the far slot = `from`: nodes {split = false, slot = s}, edges of kind Out oriented in travel direction (from = the node, to = the parent), error `"more than two legs at one merge are not ruled yet"`. Out-leaf as for in-leaf.
3. **Edge order.** All In edges in creation order, then one Leg per path in path order {Leg, from = in-leaf, to = out-leaf, paths = {p}}, then all Out edges in creation order. Nodes: In nodes in creation order, then Out nodes in creation order.
4. **path_edges[p]**: the In edges whose paths contain p (ascending index), its Leg, then the Out edges whose paths contain p in DESCENDING index order (leaf to port).
5. **Crossings.** chord = JunctionChordCrossings(paths). Candidates: every edge pair (i < j) whose path sets are disjoint and every (p in i, q in j) is a chord pair. Sort by |paths_i| * |paths_j| descending, then (i, j) ascending. Take a candidate when none of its path pairs is already covered; mark them covered. Return the taken pairs sorted. (Leg x Leg covers anything left, so every chord pair is covered exactly once.)

Worked example (LL_RR, branch-in slot 4): S = Je1(4->9), Je2(4->8), Jw1(4->2), Jw2(4->3). Level 0 keys: group e-out (least distance (8-4)%10 = 4), group w-out ((2-4+10)%10 = 8): e-out first. Node 2 {split, 4}; edge e2 {In, port 4 -> node 2, {8,9,10,11}}. Key e-out: subset {Je1, Je2}: Partition level 0 has one key -> level 1: keys slot 8 (4) and slot 9 (5): node 3; edge e3 {In, node 2 -> node 3, {8,9}}; Je2's and Je1's in-leaf = node 3. Key w-out: node 4, edge e4 {In, node 2 -> node 4, {10,11}}.

## Tests (tag `[topology]`; record each red)
State every expected list literally in the test (no loops computing expectations).
1. "the T's track graph" (outer): paths = ExpandJunctionLanes(JunctionTTopology()); path order A(0->5), AS(0->3), W(4->1), WS(4->3), NW(2->1), NE(2->5).
   - nodes: {split,0}, {split,2}, {split,4}, {merge,1}, {merge,3}, {merge,5}.
   - edges (kind, from, to, paths; P = port slot, N = node):
     e0 In P0->N0 {0,1}; e1 In P2->N1 {4,5}; e2 In P4->N2 {2,3};
     e3 Leg N0->N5 {0}; e4 Leg N0->N4 {1}; e5 Leg N2->N3 {2}; e6 Leg N2->N4 {3}; e7 Leg N1->N3 {4}; e8 Leg N1->N5 {5};
     e9 Out N3->P1 {2,4}; e10 Out N4->P3 {1,3}; e11 Out N5->P5 {0,5}.
   - path_edges: A [0,3,11], AS [0,4,10], W [2,5,9], WS [2,6,10], NW [1,7,9], NE [1,8,11].
   - crossings: (4,5), (4,8), (5,8) (the traced T's three: W x A->S, A->S x N->E, W x N->E).
2. "LL_RR with an L_R sideline: track graph" (outer): path order Te1(0->9), Te2(1->8), Tw1(7->2), Tw2(6->3), Xe1(0->5), Xe2(1->5), Xw1(7->5), Xw2(6->5), Je1(4->9), Je2(4->8), Jw1(4->2), Jw2(4->3) (indices 0..11).
   - nodes: N0 {split,0}, N1 {split,1}, N2 {split,4}, N3 {split,4}, N4 {split,4}, N5 {split,6}, N6 {split,7}, N7 {merge,2}, N8 {merge,3}, N9 {merge,5}, N10 {merge,5}, N11 {merge,5}, N12 {merge,8}, N13 {merge,9}.
   - edges:
     e0 In P0->N0 {0,4}; e1 In P1->N1 {1,5}; e2 In P4->N2 {8,9,10,11}; e3 In N2->N3 {8,9}; e4 In N2->N4 {10,11}; e5 In P6->N5 {3,7}; e6 In P7->N6 {2,6};
     e7 Leg N0->N13 {0}; e8 Leg N1->N12 {1}; e9 Leg N6->N7 {2}; e10 Leg N5->N8 {3}; e11 Leg N0->N11 {4}; e12 Leg N1->N11 {5}; e13 Leg N6->N10 {6}; e14 Leg N5->N10 {7}; e15 Leg N3->N13 {8}; e16 Leg N3->N12 {9}; e17 Leg N4->N7 {10}; e18 Leg N4->N8 {11};
     e19 Out N7->P2 {2,10}; e20 Out N8->P3 {3,11}; e21 Out N9->P5 {4,5,6,7}; e22 Out N10->N9 {6,7}; e23 Out N11->N9 {4,5}; e24 Out N12->P8 {1,9}; e25 Out N13->P9 {0,8}.
   - path_edges: Te1 [0,7,25], Te2 [1,8,24], Tw1 [6,9,19], Tw2 [5,10,20], Xe1 [0,11,23,21], Xe2 [1,12,23,21], Xw1 [6,13,22,21], Xw2 [5,14,22,21], Je1 [2,3,15,25], Je2 [2,3,16,24], Jw1 [2,4,17,19], Jw2 [2,4,18,20].
   - crossings (9): (3,9), (3,10), (3,23), (8,11), (8,15), (9,23), (10,13), (10,17), (10,23).
   If your implementation of the rules as written gives anything different, do NOT change the expected lists: STOP and report both.
3. "every chord pair is crossed exactly once": for both topologies, expand each crossing (i, j) into its path pairs (min, max); the multiset equals JunctionChordCrossings(paths) with no duplicates.
4. "a split with three legs is refused": groups (in slot order) W in 1 lane line 0 main, S out 1 lane line 1, E out 1 lane line 0 main, N out 1 lane line 2; movements W->S, W->E, W->N: empty result, error "more than two legs at one split are not ruled yet".
Red: does not compile; then with a stub returning an empty graph, tests 1-3 fail on their first CHECK (record the messages).

## Do not
- No node order along edges, sides, rows, turns, bridges, doubling or positions (later steps). Do not call these from the template builder, solve, generator or checker. Do not change ExpandJunctionLanes or JunctionChordCrossings.

## Stop conditions
- Test 1 or 2 differs from the expected lists (report both).
- Anything the rules above leave undecided.

## Gates
`tools/junction/jt fast build "[topology]" 5`, `tools/junction/jt fast build-rel "[topology]" 1`, `tools/junction/jt gate build-rel`. Update the `[topology]` time in TESTS.md. Commit: `Add: Junction track graph with crossings per edge`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
References read; commit; red evidence; types/functions added; suites with times; stops; `git status --porcelain` and `git log -1`. When you have reported, stop.
