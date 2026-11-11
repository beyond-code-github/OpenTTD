# G1b(i): the topology type, two topologies as data, lane expansion and chord crossings (DESIGN-G1 §1, §2 R1/R4, §6; D186-D188, D202)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (persistent; Debug `build`, Release `build-rel`). First: `git checkout -B wt-g1b1 <HEAD>` (given at launch). You are the only writer there. `tools/junction/jt` builds openttd_test before running (new test files need a CMake re-run: `cmake build` / `cmake build-rel` if jt does not pick them up).
- The node graph will be generated from a topology for any junction type (D201, D202). This step adds only the topology description and the first two structure rules, as pure functions with their own tests. Nothing existing calls them yet; nothing existing changes.

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ part, file header and doc comment style); docs/junction-tool/TESTS.md (how a new tag gets a row); DESIGN-G1.md §1, §2, §6; FABLE-G1.md §1b (the LL_RR walk-through: the source of test 2's expected lists).
- src/junction_rules.h (JunctionPortRole) and src/junction_rules.cpp JunctionTMovements (the T's six movements and their order).
- An existing small junction source/test pair for file layout and the copyright header (e.g. src/junction_facts.h/.cpp, src/tests/junction_facts.cpp), and src/CMakeLists.txt / src/tests/CMakeLists.txt (where junction_* files are listed).

## Design (settled)
New files src/junction_topology.h, src/junction_topology.cpp, src/tests/junction_topology.cpp, registered in both CMakeLists beside the other junction files.
```cpp
/** A side of the junction's box in the canonical frame (the main line runs along X, D152). */
enum class JunctionSide : uint8_t { N, E, S, W };

/** One direction of one line where it meets the box (DESIGN-G1 §1). */
struct JunctionLaneGroup {
	JunctionSide side;
	bool in; ///< Trains enter the box here.
	uint8_t lanes; ///< Adjacent same-direction tracks (D142), at least 1.
	uint8_t line; ///< Groups of one line share this id.
	bool main; ///< Part of the main line (D60: main line and branch take their own notation).
};

/** A junction type: its lane groups and movements (D189). */
struct JunctionTopology {
	std::vector<JunctionLaneGroup> groups; ///< In slot order (JunctionPortSlots).
	std::vector<std::pair<uint8_t, uint8_t>> movements; ///< (in group, out group), indices into groups.
};

/** One lane of a group at the box: a port slot. */
struct JunctionPortSlot {
	uint8_t group;
	uint8_t lane; ///< 0 is the northmost lane on the W and E sides, the westmost on the N and S sides.
};

/** The port slots counter-clockwise from the north-west corner: W side north to south, S side west to east, E side south to north, N side east to west; so on the E and N sides a group's lanes appear in reverse lane order. Groups must be listed in that order; otherwise empty. */
std::vector<JunctionPortSlot> JunctionPortSlots(const JunctionTopology &topology);

/** One lane-level path (DESIGN-G1 R1): from an in slot to an out slot. */
struct JunctionLanePath {
	uint16_t from; ///< Index into JunctionPortSlots.
	uint16_t to;
	uint16_t movement; ///< Index into JunctionTopology::movements.
};

/**
 * The lane-level paths of a topology (DESIGN-G1 R1; D186, D187): per movement in order, lanes ascending (from lane, then to lane).
 * Through movement (both groups main, same line): lane i to lane i (equal lane counts, else error). Otherwise one side has one lane: every pair (an exit from every lane, D187; a fan onto every lane, D186).
 * Both sides with more than one lane and not a through movement: error "multi-to-multi movements are not ruled yet" (D186: later).
 * @param error Receives the reason when the result is empty.
 */
std::vector<JunctionLanePath> ExpandJunctionLanes(const JunctionTopology &topology, std::string *error);

/**
 * The pairs of lane paths that must cross (DESIGN-G1 R4, chord rule): two paths with no slot in common cross iff exactly one end of one lies strictly between the two ends of the other in slot order.
 * Pairs (i, j) with i < j, sorted. Paths sharing a slot are not paired here (they share a trunk; per-edge crossings are a later step).
 */
std::vector<std::pair<uint32_t, uint32_t>> JunctionChordCrossings(const std::vector<JunctionLanePath> &paths);

/** The T of today's node model (L_R main line, L_R branch on the S side), groups in slot order: W side EIn then WOut; S side NIn then SOut; E side WIn then EOut. Movements in JunctionTMovements order. */
const JunctionTopology &JunctionTTopology();

/** An LL_RR main line with an L_R sideline on the S side (D186-D188, D202): W side e-in (2 lanes) then w-out (2); S side branch-in (1) then branch-out (1); E side w-in (2) then e-out (2). Movements: e through, w through, e exit, w exit, join to e-out, join to w-out. */
const JunctionTopology &JunctionLLRRSidelineTopology();
```
Main line groups: main = true, line 0; branch groups: main = false, line 1.

## Tests (tag `[topology]`; record each red)
1. "the T's lanes and crossings": `JunctionPortSlots(JunctionTTopology())` has 6 slots; ExpandJunctionLanes gives 6 paths whose (from group, to group), mapped by a table in the test (EIn, WOut, NIn, SOut, WIn, EOut by group index), equal JunctionTMovements in order; JunctionChordCrossings gives exactly the 3 pairs (WIn->WOut, EIn->SOut), (WIn->WOut, NIn->EOut), (NIn->EOut, EIn->SOut) (RECON-turn-programs §4; the traced T's three bridges). Red: does not compile, then with stubs returning empty: fails.
2. "LL_RR with an L_R sideline: lanes and crossings" (FABLE-G1 §1b): slots in order e1in, e2in, w1out, w2out, Bin, Sout, w2in, w1in, e2out, e1out (as (group, lane)); 12 paths: through (0,9), (1,8), (7,2), (6,3); exits (0,5), (1,5), (7,5), (6,5); joins (4,9), (4,8), (4,2), (4,3) (as slot pairs, in the order ExpandJunctionLanes documents: state them in that order in the test); 16 crossing pairs: X_e1 with T_e2, T_w1, T_w2, J_e1, J_e2; X_e2 with T_w1, T_w2, J_e1, J_e2; X_w1 with T_w2; J_e1 with T_e2, T_w1, T_w2; J_e2 with T_w1, T_w2; J_w1 with T_w2 (T = through, X = exit, J = join; e1 = lane 0). If your implementation of the rule as documented gives a different set, do NOT change the expected list: STOP and report both sets.
3. "multi-to-multi is refused": a topology with a 2-lane exit into a 2-lane branch out: empty with the error text.
4. "groups out of slot order are refused": JunctionPortSlots of a topology listing an E group before a W group: empty.
5. Add a `[topology]` row to TESTS.md with its time. The gate stays green.

## Do not
- Do not call these from the template builder, the solve, the generator or the checker. Do not touch JunctionPortRole or JunctionTMovements. No node order, rows, bridges or doubling (later steps).

## Stop conditions
- Test 2's crossings differ from the expected list (report both).
- The T's crossings differ from the three pairs (report).

## Gates
`tools/junction/jt fast build "[topology]" 5`, `tools/junction/jt fast build-rel "[topology]" 1`, `tools/junction/jt gate build-rel`. Commit: `Add: Junction topology, lane expansion and chord crossings`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
References read; commit; red evidence; types/functions added; suites with times; stops; `git status --porcelain` and `git log -1`. When you have reported, stop.
