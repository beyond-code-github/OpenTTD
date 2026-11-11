# G1c(i-b): band layers, the y order of nodes and band crossings inside each band (DESIGN-G1 §10.8)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (persistent; Debug `build`, Release `build-rel`). First: `git checkout -B wt-g1c1b <HEAD>` (given at launch). You are the only writer there. `tools/junction/jt` builds openttd_test before running.
- OrderJunctionTrackGraph sweeps north from the branch side. In each band (even level) it repeatedly swaps an adjacent pending band crossing or expands the leftmost item whose north end is a node at this level. This step records those band events with a layer (their y order), as one new member of JunctionTrackOrder. Nothing else in the order changes.

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ part, doc comments); docs/junction-tool/TESTS.md (the `[topology]` row).
- docs/junction-tool/DESIGN-G1.md §10.8 (the last section of the file).
- src/junction_topology.h :116-137 (JunctionTrackRowItem, JunctionTrackOrder, OrderJunctionTrackGraph).
- src/junction_topology.cpp :350-525 (OrderJunctionTrackGraph; the band loop is :424-467).
- src/tests/junction_topology.cpp :237-280 (the two track order tests; RN/RE helpers above them).

## Design (settled)
1. junction_topology.h, before JunctionTrackOrder:
   ```cpp
   /** A node or a band crossing at its place inside a band (DESIGN-G1 §10.8). */
   struct JunctionTrackBandEvent {
   	bool node; ///< A node (a is its index), else a band crossing (a and b are its edges, as in JunctionTrackGraph::crossings).
   	uint32_t a;
   	uint32_t b; ///< 0 for a node.
   	int layer; ///< 1 is southmost; an event lies north of every event of a lower layer that touched one of its items.
   	bool operator==(const JunctionTrackBandEvent &) const = default;
   };
   ```
   JunctionTrackOrder gains, after `edge_crossings`:
   ```cpp
   std::vector<std::vector<JunctionTrackBandEvent>> bands; ///< Per band b (0 to R), its events in sweep order.
   ```
2. OrderJunctionTrackGraph: `order.bands` has R+1 entries (band b = level / 2). Keep a per-item layer beside `items` (same order, moved with it). At the start of each band every item's layer is 0. In the band loop:
   - a swap of the pair p: layer = 1 + max(layer of both items); push `{false, p->first, p->second, layer}`; both items take that layer.
   - an expansion of item i at node k: layer = 1 + layer of item i; push `{true, k, 0, layer}`; every inserted child takes that layer.
   Nothing else in the sweep changes (same choices, same order, same errors).

## Tests (tag `[topology]`; record each red)
1. In "topology: the T's track order", add: `order.bands` has 3 entries; bands 0 and 1 empty; band 2 == `{{true, 1, 0, 1}, {true, 4, 0, 1}, {false, 4, 8, 2}}`.
2. In "topology: LL_RR with an L_R sideline: track order", add: `order.bands` has 5 entries; bands 0, 1 and 3 empty; band 2 == `{{true, 3, 0, 1}}`; band 4 == `{{true, 2, 0, 1}, {true, 4, 0, 2}, {true, 9, 0, 1}, {false, 3, 23, 2}, {true, 11, 0, 3}, {true, 10, 0, 2}}`.
   If the implementation gives anything else, do NOT change the expected lists: STOP and report both.
Red: does not compile (record it); then with the member added but never filled: both fail. Record both.
3. Update the `[topology]` row in TESTS.md (time). Every existing expectation stays as it is; the gate stays green.

## Do not
- Do not change any existing member, choice or error of the sweep, the graph, the structure or the programs. No lengths, heights or signals. Do not change the stack.
- Do not use the word "shift" (a sideways step is an S-bend).

## Stop conditions
- Test 1 or 2 differs from the expected lists (report both).
- Any existing `[topology]` expectation changes.

## Gates
`tools/junction/jt fast build "[topology]" 5`, `tools/junction/jt fast build-rel "[topology]" 1`, `tools/junction/jt gate build-rel`. Commit: `Add: Junction track order records band layers (DESIGN-G1 §10.8)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
References read; commit; red evidence; types/members added; suites with times; stops; `git status --porcelain` and `git log -1`. When you have reported, stop.
