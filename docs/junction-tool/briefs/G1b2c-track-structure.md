# G1b(ii-c): side tracks, the row stack, bridges, turns (DESIGN-G1 §9; D75/D87, D80, D108, D174, D203)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (persistent; Debug `build`, Release `build-rel`). First: `git checkout -B wt-g1b2c <HEAD>` (given at launch). You are the only writer there. `tools/junction/jt` builds openttd_test before running.
- G1b(ii-a) and (ii-b) (merged) added JunctionTrackGraph / BuildJunctionTrackGraph and JunctionTrackOrder / OrderJunctionTrackGraph to src/junction_topology.h/.cpp. This step adds the last combinatorial layer: per lane its side tracks, the north-to-south row stack inside the box, the bridges, and each path's turn. A pure function with its own tests; nothing existing calls it; nothing existing changes.

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ part); docs/junction-tool/DESIGN-G1.md §7-§9.
- src/junction_topology.h (all) and src/junction_topology.cpp (the track graph and order parts); src/tests/junction_topology.cpp (the graph and order tests: their node/edge numbers are used below).

## Design (settled)
Add to junction_topology.h/.cpp after OrderJunctionTrackGraph:
```cpp
/** One track of the row stack inside the box, north to south (DESIGN-G1 §9). */
struct JunctionTrackStackItem {
	enum class Kind : uint8_t {
		Lane, ///< The main-line lane itself.
		Prio, ///< Its prio track (D75/D87: opposite the joiner).
		Copy, ///< Its doubling copy (D80).
		PrioAndCopy, ///< Prio and doubling copy sharing one track (D108 keeps them apart along the lane).
		Approach, ///< The joiner's last stretch beside the lane, on the joiner's side.
	};
	uint8_t row; ///< The lane's row (JunctionTrackOrder::rows index).
	Kind kind;
	bool operator==(const JunctionTrackStackItem &) const = default;
};

/** A bridge: one edge carried over a contiguous set of crossings (D203). */
struct JunctionTrackBridge {
	uint32_t edge; ///< The edge carried over.
	std::vector<uint32_t> crossed; ///< The edges it passes, in the carried edge's travel order.
	bool doubled; ///< A main-line bridge (D80, D188); a branch bridge is single (D174).
	bool operator==(const JunctionTrackBridge &) const = default;
};

/** Side tracks, row stack, bridges and turns of an ordered track graph (DESIGN-G1 §9). */
struct JunctionTrackStructure {
	std::vector<JunctionTrackStackItem> stack; ///< North to south.
	std::vector<JunctionTrackBridge> bridges; ///< Sorted by edge.
	std::vector<int8_t> turns; ///< Per lane path: quarter turns clockwise from in heading to out heading (+1 right, -1 left, 0).
};

/**
 * The structure of an ordered track graph (DESIGN-G1 §9).
 * @param error Receives the reason when the result is empty (no stack).
 */
JunctionTrackStructure StructureJunctionTrackGraph(const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, const JunctionTrackOrder &order, std::string *error);
```
Rules, exactly. "Row edge" and "north"/"south" are as in OrderJunctionTrackGraph (an edge whose two end levels are equal and odd lies on that row). The branch ports are on the S side (OrderJunctionTrackGraph refuses others), so "the joiner's side" is south whenever the joiner comes from the S side; compute it as stated, do not assume it.
1. **Joiner and prio.** For each row r: its joiners are the edges that are not row edges and have an end at a merge node on row r (node_level == 2r+1, split == false). More than one joiner on a row: error `"more than one joiner on a lane is not ruled yet"`. With one joiner: approach side = south if the joiner's other end has a larger level than 2r+1, else north; prio side = the opposite.
2. **Copy.** Row r is crossed iff any of its row edges has a non-empty edge_crossings. A crossed row's copy side = its prio side if it has a prio; else the side away from the branch ports (north).
3. **Stack.** For r = 0 .. R-1: the north side item (Prio, Copy or PrioAndCopy if both are north; Approach if the approach is north), then {r, Lane}, then the south side item the same way. A side with nothing has no item. Prio and approach are never on one side (rule 1).
4. **Bridges.** For each row edge with non-empty edge_crossings: {edge, its edge_crossings, true}. For each pair in graph.crossings where neither edge is a row edge: the joiner edge = the one whose paths all start at a non-main slot (paths[p].from's group not main); exactly one of the two must be (else error `"a branch crossing without one joiner is not ruled yet"`); {joiner edge, {the other}, false}. Sort by edge.
5. **Turns.** In heading from the in slot's side: W -> east, E -> west, S -> north, N -> south. Out heading from the out slot's side: W -> west, E -> east, S -> south, N -> north. Turn = clockwise quarter turns from in to out, in -1..+1 (a reversal, 2, cannot occur from these sides; if it does: error `"a reversing path is not ruled yet"`).

## Tests (tag `[topology]`; record each red)
State every expected list literally. Node and edge numbers are those of the existing track-graph tests.
1. "the T's track structure" (outer): stack [{0,Prio}, {0,Lane}, {0,Approach}, {1,PrioAndCopy}, {1,Lane}, {1,Approach}] (the traced T's rows 0-5: A's prio, A, N->E's approach on the gap row, W's prio and doubling copy on row 3, W, N->W on row 5); bridges [{5, {8, 4}, true}, {8, {4}, false}] (W's doubled bridges over N->E and A->S; N->E single over A->S); turns {0, +1, 0, -1, -1, +1}.
2. "LL_RR with an L_R sideline: track structure" (outer): stack [{0,Prio}, {0,Lane}, {0,Approach}, {1,PrioAndCopy}, {1,Lane}, {1,Approach}, {2,PrioAndCopy}, {2,Lane}, {2,Approach}, {3,PrioAndCopy}, {3,Lane}, {3,Approach}]; bridges [{3, {23}, false}, {8, {11, 15}, true}, {9, {3, 23}, true}, {10, {13, 3, 23, 17}, true}]; turns {0, 0, 0, 0, +1, +1, -1, -1, +1, +1, -1, -1}.
   If your implementation of the rules as written gives anything different in test 1 or 2, do NOT change the expected lists: STOP and report both.
3. "two joiners on one lane are refused": a topology with groups in slot order W in 1 lane main line 0, W out 1 lane main line 0, S in 1 lane line 1, S in 1 lane line 2, E in 1 lane main line 0, E out 1 lane main line 0; movements W in -> E out, E in -> W out, both S ins -> E out. Build graph and order (CHECK both succeed; if either fails, STOP and report its error), then StructureJunctionTrackGraph: empty stack, error "more than one joiner on a lane is not ruled yet".
Red: does not compile (record it: add the tests before the declaration); then with a stub returning an empty structure, tests 1-3 fail (record the messages).

## Do not
- No positions, lengths, sharing of rows, bridge splitting, span limits, tunnels or over/under choices beyond rule 4 (later steps). Do not change BuildJunctionTrackGraph, OrderJunctionTrackGraph or anything they call. Do not call the new function from the template builder, solve, generator or checker.

## Stop conditions
- Test 1 or 2 differs (report both). Test 3's graph or order fails (report).
- Anything the rules above leave undecided.

## Gates
`tools/junction/jt fast build "[topology]" 5`, `tools/junction/jt fast build-rel "[topology]" 1`, `tools/junction/jt gate build-rel`. Update the `[topology]` row in TESTS.md. Commit: `Add: Junction track structure: side tracks, row stack, bridges, turns`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
References read; commit; red evidence; types/functions added; suites with times; stops; any gap you filled; `git status --porcelain` and `git log -1`. When you have reported, stop.
