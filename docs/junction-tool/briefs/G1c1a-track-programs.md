# G1c(i-a): track programs, the bends and crossings of every graph edge (DESIGN-G1 §10.1, §10.6, §10.7)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (persistent; Debug `build`, Release `build-rel`). First: `git checkout -B wt-g1c1a <HEAD>` (given at launch). You are the only writer there. `tools/junction/jt` builds openttd_test before running.
- G1b made, for a topology: lane paths, a track graph (nodes with levels, edges, crossings), an order (rows, edge_crossings in travel order) and a structure (row stack, bridges, turns). This step adds one pure function that turns those into a symbolic drawing: per graph edge, its bends and crossings in travel order. No lengths, no coordinates, nothing calls it yet.

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ part, doc comments); docs/junction-tool/TESTS.md (the `[topology]` row).
- docs/junction-tool/DESIGN-G1.md §10.1, §10.6, §10.7 (lines 88-125). §10.7 is this step's design; the rules below restate it exactly.
- src/junction_topology.h (whole file, 179 lines).
- src/junction_topology.cpp :350-475 (OrderJunctionTrackGraph: `end_level`, `is_row_edge`, and the sibling key at :449-462) and :527-634 (StructureJunctionTrackGraph, for how it finds rows, stack items and bridges).
- src/tests/junction_topology.cpp :100-185 and :237-320 (the graph, order and structure expectations of both topologies; the helpers P, N, OrderOf, StructureOf).

## Design (settled)
Add to src/junction_topology.h after StructureJunctionTrackGraph, and implement in src/junction_topology.cpp after it:
```cpp
/** One token of a track edge's program (DESIGN-G1 §10.7), in travel order. Runs lie between tokens; lengths, columns and port rows are not part of a program. */
struct JunctionTrackStep {
	static constexpr uint32_t NO_ROW = UINT32_MAX; ///< A bend that ends heading N or S.
	enum class Kind : uint8_t {
		Fan, ///< The S-bend between a port row and a stack row, or none when they coincide (placed later).
		Corner, ///< A same-hand 90-degree pair.
		SBend, ///< An opposite-hand pair: the track moves sideways and keeps its heading.
		Over, ///< This edge's bridge passes over another edge.
		Under, ///< This edge passes under another edge's bridge.
	};
	Kind kind;
	int8_t hand; ///< Corner: the turn, +1 right, -1 left (as JunctionTrackStructure::turns). SBend: its first curve. 0 otherwise.
	uint32_t index; ///< Fan: the stack item at its inner end. Corner, SBend: the stack item it ends on, or NO_ROW. Over: the crossed edge. Under: the edge overhead.
	uint8_t copy; ///< Under: which copy of a doubled bridge is overhead (0 or 1). 0 otherwise.
	bool operator==(const JunctionTrackStep &) const = default;
};

/** The programs of a structured track graph (DESIGN-G1 §10.7). */
struct JunctionTrackPrograms {
	std::vector<std::vector<JunctionTrackStep>> edges; ///< Per graph edge; for a doubled edge its copy 0 (keeps its upstream row through the bridge, bends onto the lane row after it).
	std::vector<std::vector<JunctionTrackStep>> copies; ///< Per graph edge: a doubled edge's copy 1 (bends onto the lane row before the bridge); empty for the others.
};

/**
 * The programs of every edge of a structured track graph (DESIGN-G1 §10.7).
 * @param error Receives the reason when the result is empty (no edges).
 */
JunctionTrackPrograms ProgramJunctionTrackGraph(const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, const JunctionTrackOrder &order, const JunctionTrackStructure &structure, std::string *error);
```
Implement inside the function with lambdas, as the other functions in the file do (no new file-scope helpers, no new struct). Reuse JunctionSlotRows and JunctionPortSlots.

### Definitions (all from the inputs)
- `end_level(end)`: as in OrderJunctionTrackGraph: a node's `level`; a port slot on a main group: 2·row+1 (row from JunctionSlotRows); any other port: 2R+1 (R = number of rows). A **row node** has an odd level; a **band node** an even level. A **row edge**: both ends on the same odd level.
- **Heading of row r**: E if its In slot (the main in-group slot with that row) is on the W side, W if on the E side.
- **Stack items of row r** (indices into structure.stack): `lane` = the item {r, Lane}; `north` = the item of row r directly before `lane` (kind Prio, Copy or PrioAndCopy); `approach` = the item {r, Approach}. Missing item where needed: error "a row without its side tracks is not ruled yet".
- **Row shape**: the row edges of row r must be exactly three: In (main port to row node), leg (row node to row node), Out (row node to main port). Otherwise: error "a lane other than in, one leg, out is not ruled yet". A non-row edge with both ends row nodes: error "an edge between two rows is not ruled yet".
- **Doubled**: a structure.bridges entry with `doubled` and `edge` == e. A row is doubled when its leg is. A doubled bridge on an In or Out row edge: error "a crossed lane in or out edge is not ruled yet".
- **Edge heading** (non-row edges): S when end_level(to) > end_level(from), else N.
- **Sibling order at a band node**: the node's edges other than its trunk (its In edge for a split, its Out edge for a merge) sorted by the sweep's key (:455-459: min over the edge's paths of (far − node.slot + n) % n, far = path.to at a split, path.from at a merge), descending = west first.
- **Straight child** of a band node: of its two siblings, the one whose far end (the end that is not this node) has the smaller end_level; a tie: the west one. The other is **diverging**; its **side hand** is -1 if it is west of the straight child, else +1.
- **S-side slot** of an edge: of its first path, the end slot whose group is not main.

### Programs (edge e; `rows` below mean the rows of e's ends)
1. **In row edge**: `{Fan, 0, north if the row is doubled else lane, 0}`.
2. **Out row edge**: `{Fan, 0, lane, 0}`.
3. **Leg row edge**: not doubled: empty. Doubled: let S = `{SBend, h, lane, 0}` with h = +1 if the row heads E, -1 if W; let O = `{Over, 0, x, 0}` for each x in order.edge_crossings[e]. Copy 0 (in `edges`) = O..., S. Copy 1 (in `copies`) = S, O....
4. **Any other edge**, in this order:
   a. Start: if `from` is a band split and e is its diverging child: if e ends at a row node (a joiner leg) and order.edge_crossings[e] is empty: the joiner Corner (step c) here, and its hand must equal e's side hand, else error "a diverging corner on the wrong side is not ruled yet"; a joiner leg with crossings: error "a diverging joiner that crosses is not ruled yet"; any other edge: `{SBend, side hand, NO_ROW, 0}`.
      If `from` is a row node (an exit leg): `{Corner, h, NO_ROW, 0}`, h = +1 if that row heads E, -1 if W.
   b. Crossings: for each x in order.edge_crossings[e]: if a bridge has `edge` == e: `{Over, 0, x, 0}`. Else the bridge with `edge` == x: if doubled: `{Under, 0, x, c}` twice, c = 0 then 1 when e heads S, 1 then 0 when e heads N; if single: `{SBend, h, NO_ROW, 0}` then `{Under, 0, x, 0}`, h = +1 if e's S-side slot < x's S-side slot, else -1.
   c. If `to` is a row node (a joiner leg), unless step a already placed the Corner: `{Corner, h, approach of that row, 0}`, h = +1 if that row heads E, -1 if W. Then always `{SBend, h', lane of that row, 0}`, h' = -1 if that row heads E, +1 if W.
   d. End: if `to` is a band merge and e is its diverging child: `{SBend, side hand, NO_ROW, 0}`.
5. `copies[e]` is empty for every edge but doubled legs.

## Tests (tag `[topology]`, in src/tests/junction_topology.cpp; record each red)
Add a helper `ProgramsOf(topology)` beside StructureOf, and in the test file short makers: `Fan(i)`, `C(h, i)`, `S(h, i)`, `O(x)`, `U(x, c)` returning JunctionTrackStep, and `NR = JunctionTrackStep::NO_ROW`.
1. "topology: the T's track programs": `edges` and `copies` (12 each) equal exactly:
   - 0 `{Fan(1)}`; 1 `{}`; 2 `{Fan(3)}`; 3 `{}`; 4 `{C(+1,NR), U(5,0), U(5,1), S(-1,NR), U(8,0)}`; 5 `{O(8), O(4), S(-1,4)}`; 6 `{C(-1,NR), S(+1,NR)}`; 7 `{C(-1,5), S(+1,4)}`; 8 `{O(4), U(5,1), U(5,0), C(+1,2), S(-1,1)}`; 9 `{Fan(4)}`; 10 `{}`; 11 `{Fan(1)}`.
   - copies: all empty except 5 `{S(-1,4), O(8), O(4)}`.
   (These are the traced T's hands, RECON-turn-programs §1: P1 RR LR, P5 LL RL, P6 RR LR, P7 LL RL, W's lane S-bends D10/D11.)
2. "topology: LL_RR with an L_R sideline: track programs": `edges` and `copies` (26 each) equal exactly:
   - 0 `{Fan(1)}`; 1 `{Fan(3)}`; 2 `{}`; 3 `{O(23), U(10,1), U(10,0), U(9,1), U(9,0)}`; 4 `{S(-1,NR)}`; 5 `{Fan(9)}`; 6 `{Fan(6)}`; 7 `{}`;
   - 8 `{O(11), O(15), S(+1,4)}`; 9 `{O(3), O(11), O(12), S(-1,7)}`; 10 `{O(13), O(3), O(11), O(12), O(17), S(-1,10)}`;
   - 11 `{C(+1,NR), U(8,0), U(8,1), U(9,0), U(9,1), U(10,0), U(10,1)}`; 12 `{C(+1,NR), U(9,0), U(9,1), U(10,0), U(10,1), S(-1,NR)}`; 13 `{C(-1,NR), U(10,0), U(10,1)}`; 14 `{C(-1,NR), S(+1,NR)}`;
   - 15 `{U(8,1), U(8,0), C(+1,2), S(-1,1)}`; 16 `{C(+1,5), S(-1,4)}`; 17 `{U(10,1), U(10,0), C(-1,8), S(+1,7)}`; 18 `{C(-1,11), S(+1,10)}`;
   - 19 `{Fan(7)}`; 20 `{Fan(10)}`; 21 `{}`; 22 `{S(+1,NR)}`; 23 `{S(-1,NR), U(3,0)}`; 24 `{Fan(4)}`; 25 `{Fan(1)}`.
   - copies: all empty except 8 `{S(+1,4), O(11), O(15)}`, 9 `{S(-1,7), O(3), O(11), O(12)}`, 10 `{S(-1,10), O(13), O(3), O(11), O(12), O(17)}`.
   If your implementation of the rules as written gives anything else, do NOT change the expected lists: STOP and report both (edge by edge).
3. "topology: programs agree with turns and crossings" (both topologies, one loop): (a) for every path, the sum of the Corner hands over `edges[e]` for e in graph.path_edges[path] equals structure.turns[path]; (b) for every edge e, its Over and Under tokens (in `edges[e]`) taken in order and with repeated Under(x) collapsed to one x give exactly order.edge_crossings[e]; (c) a doubled edge's Under count per x is 2 on every crossing edge, a single bridge's 1.
Red for all three: does not compile; then with a stub returning `JunctionTrackPrograms{}`: each fails on its first CHECK/REQUIRE. Record both.
4. Update the `[topology]` row in TESTS.md (test count and time). The gate stays green.

## Do not
- Do not change BuildJunctionTrackGraph, OrderJunctionTrackGraph, StructureJunctionTrackGraph, their outputs or any existing expectation. No lengths, columns, band y orders, band slots, signals or port rows (later steps). Do not call the new function from the template builder, the solve, the generator or the checker.
- Do not add the word "shift" anywhere (a sideways step is an S-bend; "shift" is reserved for the coop shifter).

## Stop conditions
- Test 1 or 2 differs from the expected lists (report both, edge by edge).
- Any rule above is ambiguous for an edge of either topology (report the edge and the rule).
- An error is returned for either topology (report the text and the edge).

## Gates
`tools/junction/jt fast build "[topology]" 5`, `tools/junction/jt fast build-rel "[topology]" 1`, `tools/junction/jt gate build-rel`. Commit: `Add: Junction track programs (DESIGN-G1 §10.7)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
References read; commit; red evidence; types/functions added; suites with times; stops; `git status --porcelain` and `git log -1`. When you have reported, stop.
