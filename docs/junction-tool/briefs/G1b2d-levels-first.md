# G1b(ii-d): node levels before crossings (D205), crossings filtered by level, one slot-row rule (DESIGN-G1 §10.6)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (persistent; Debug `build`, Release `build-rel`). First: `git checkout -B wt-g1b2d <HEAD>` (given at launch). You are the only writer there. `tools/junction/jt` builds openttd_test before running.
- src/junction_topology.h/.cpp hold BuildJunctionTrackGraph (G1b ii-a), OrderJunctionTrackGraph (ii-b), StructureJunctionTrackGraph (ii-c). D205 (DECISIONS.md, read that row) moves the exit-side lane merges to band R. Crossings must then follow node levels, so levels move into the graph step. Nothing outside junction_topology.* and its test changes.

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ part); docs/junction-tool/DECISIONS.md row D205 (grep); docs/junction-tool/DESIGN-G1.md §7, §8, §10.6.
- src/junction_topology.h and .cpp (all); src/tests/junction_topology.cpp (all).

## Changes (settled)
1. **One slot-row rule.** Add and declare after JunctionChordCrossings:
   ```cpp
   /**
    * The row of every port slot (DESIGN-G1 §8): the main slots on the W side in slot order are rows 0, 1, ...; every other main slot takes the row of the W-side slot it shares a through path with (both ends main); a non-main slot has -1.
    * @param error Receives the reason when the result is empty.
    */
   std::vector<int> JunctionSlotRows(const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, std::string *error);
   ```
   Same error text as today (`"a main slot without a through partner is not ruled yet"`). Replace the row computations inside OrderJunctionTrackGraph and StructureJunctionTrackGraph with calls to it (no behaviour change there).
2. **Levels in the graph.** JunctionTrackNode gains `int level; ///< 2r+1 on row r; 2b in band b (band b north of row b; band R south of the last row) (DESIGN-G1 §8, D204, D205).` (operator== keeps comparing every member). BuildJunctionTrackGraph:
   - First (before building trees): every non-main slot must be on the S side, else error `"branch ports off the S side are not ruled yet"`; rows = JunctionSlotRows (propagate its error); R = number of W-side main slots.
   - Each node's level when created: main slot -> 2*row+1; non-main slot, merge node -> 2R (D205); non-main slot, split node -> 2R if its paths' far slots lie in more than one group, else 2*(1 + the largest row among that group's slots) (D204). (This is today's OrderJunctionTrackGraph rule plus D205's merge clause.)
   - OrderJunctionTrackGraph's node_level becomes a copy of the nodes' levels (keep the member and its doc); remove its own level computation and its S-side check (now in the graph step).
3. **Crossings filtered by level.** End level of an edge end: node -> its level; main port slot -> 2*row+1; non-main port slot -> 2R+1. An edge is a row edge iff its two end levels are equal and odd; span = [smaller, larger] end level. A candidate (i, j) of rule 5 must also be able to meet: not both row edges; one row edge on level L and the other: the other's span strictly contains L (lo < L < hi); neither a row edge: some even L with max(lo_i, lo_j) <= L <= min(hi_i, hi_j). Everything else in rule 5 stays (disjoint paths, all pairs chord, largest product first, ties by index, cover each pair once).
4. **Sweep in a band, interleaved.** OrderJunctionTrackGraph rule 6 at an even level L becomes: pending = this band's band crossings (sorted). Loop: if some pending (i, j) (first in sorted order) has both edges among the items and adjacent: swap them, record each as crossing the other here, remove it from pending, continue the loop; else expand the leftmost item whose north end is a node at level L (as today) and continue; else stop. Then pending must be empty, else error `"a crossing between edges that are not adjacent is not ruled yet"`. The north end of an edge is the end opposite the one the sweep entered it by (as implemented today).

## Tests (tag `[topology]`; record each red)
Expected values come from a prototype of these rules (orchestrator, checked against the T). If your implementation gives anything different from a list below, do NOT change the list: STOP and report both.
1. Existing T tests: unchanged except the T graph's nodes now carry levels {1, 4, 3, 3, 4, 1} (N0..N5, in the node list). Every other T expectation (edges, path_edges, crossings, order, structure) must stay exactly as it is.
2. LL_RR graph test: nodes carry levels {1, 3, 8, 4, 8, 7, 5, 5, 7, 8, 8, 8, 3, 1}; edges and path_edges unchanged; crossings (11): (3,9), (3,10), (3,23), (8,11), (8,15), (9,11), (9,12), (10,11), (10,12), (10,13), (10,17).
3. LL_RR order test: node_level as in 2; rows: row 0 [N0, N13]; row 1 [N1, e11, e15, N12]; row 2 [N7, e12, e11, e3, N6]; row 3 [N8, e17, e12, e11, e3, e13, N5]; edge_crossings: e3 [23, 10, 9]; e8 [11, 15]; e9 [3, 11, 12]; e10 [13, 3, 11, 12, 17]; e11 [8, 9, 10]; e12 [9, 10]; e13 [10]; e15 [8]; e17 [10]; e23 [3]; every other edge empty.
4. LL_RR structure test: stack and turns unchanged; bridges [{3, {23}, false}, {8, {11, 15}, true}, {9, {3, 11, 12}, true}, {10, {13, 3, 11, 12, 17}, true}].
5. "every chord pair is crossed exactly once": unchanged, must still pass for both.
6. The N-side refusal test: the error now comes from BuildJunctionTrackGraph (empty graph, "branch ports off the S side are not ruled yet"); change that test accordingly (ExpandJunctionLanes still succeeds).
7. New: "JunctionSlotRows": the T gives {0, 1, -1, -1, 1, 0}; LL_RR gives {0, 1, 2, 3, -1, -1, 3, 2, 1, 0}.
Red: update the expectations first and run: record which tests fail and why (levels missing: does not compile; the LL_RR crossings: the old 9-crossing graph). Then implement.

## Do not
- Change nothing outside src/junction_topology.h/.cpp, src/tests/junction_topology.cpp and TESTS.md. No new rules beyond the four changes. Do not touch programs, positions, the template builder or the checker.

## Stop conditions
- Any T expectation would change beyond the node levels (report).
- Any LL_RR list differs from the brief's (report both).
- Anything the rules leave undecided.

## Gates
`tools/junction/jt fast build "[topology]" 5`, `tools/junction/jt fast build-rel "[topology]" 1`, `tools/junction/jt gate build-rel`. Update the `[topology]` row in TESTS.md. Commit: `Change: Junction track graph places nodes before crossings (D205)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
References read; commit; red evidence; types/members/functions added or changed; suites with times; stops; gaps filled; `git status --porcelain` and `git log -1`. When you have reported, stop.
