# SB1b: series bridges with the cut chosen by placement (D209; DESIGN-G1 §11.9, §11.9a)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (persistent; Debug `build`, Release `build-rel`). First: `git checkout -B wt-sb1 <HEAD>` (given at launch), then `git apply docs/junction-tool/wip/sb1-attempt.patch`. You are the only writer there.
- The patch is the previous agent's SB1 (briefs/SB1-series-bridges.md): placement handles several doubled bridges per lane, with one plain tile between consecutive heads; structure cut each crossing set greedily into pairs. That greedy cut has no placement for LL_RR (it separates J_e's trunk, edge 3, from X_e1, edge 11, under w2; they are forced into adjacent columns). §11.9a moves the cut out of the structure: placement tries the cuts.

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ part); docs/junction-tool/TESTS.md (`[topology]` row).
- docs/junction-tool/DESIGN-G1.md §11.8, §11.9, §11.9a (the end of the file).
- briefs/SB1-series-bridges.md (the placement design the patch implements; its points 1-9 stand).
- The patch itself, then src/junction_topology.h :150-240 and the patched StructureJunctionTrackGraph "Bridges." block and PlaceJunctionTrackGraph's signature.
- src/tests/junction_topology.cpp: PlacementOf, the structure tests, the placement and refusal tests (as patched).

## Changes
1. **Revert the structure cut** from the patch: StructureJunctionTrackGraph again pushes one `{e, order.edge_crossings[e], true}` per crossed lane leg (the original code). Keep JUNCTION_BRIDGE_MAX_CROSSERS in the header; change JunctionTrackBridge's comment to `/** A bridge: one edge carried over a contiguous set of crossings (D203); CutJunctionTrackBridges cuts a lane's set into bridges of at most JUNCTION_BRIDGE_MAX_CROSSERS (D209). */`. Keep the patch's `bridges` member comment.
2. **Keep all placement changes** of the patch unchanged.
3. **Add** in junction_topology.h after PlaceJunctionTrackGraph, implemented after it:
```cpp
/**
 * The series-bridge cut (D209, DESIGN-G1 §11.9a): every doubled bridge cut into consecutive bridges of at most JUNCTION_BRIDGE_MAX_CROSSERS crossings, choosing among the cuts with the fewest bridges whose placement succeeds the one with the least box width, then height, then the first tried.
 * @param[out] placement Receives the chosen cut's placement; empty (no nodes) when no cut places.
 * @param error Receives the last cut's placement error when no cut places.
 * @return The structure with the chosen cut (the input structure when none places).
 */
JunctionTrackStructure CutJunctionTrackBridges(const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, const JunctionTrackOrder &order, const JunctionTrackStructure &structure, const JunctionTrackPrograms &programs, const JunctionSpec &spec, JunctionTrackPlacement *placement, std::string *error);
```
   Enumeration, exactly:
   - Let D = the doubled bridges of `structure` in structure order, n_i = crossed.size(). Single bridges are kept as they are.
   - For each bridge i, its count c_i ranges from ceil(n_i / MAX) to n_i. For each total B from sum of minima to sum of n_i (ascending): for each tuple (c_0, c_1, ...) with that sum, in ascending lexicographic order; for each combination of compositions (bridge i's crossing list cut into c_i consecutive parts, each 1..MAX, compositions in descending lexicographic order of part sizes, e.g. 5 into 3: (2,2,1), (2,1,2), (1,2,2)), in odometer order with the last bridge varying fastest: build the cut structure (each doubled bridge replaced in place by its parts, in order, all doubled; then the existing stable sort by edge) and call PlaceJunctionTrackGraph.
   - Keep the best success of this B: least placement.size.x, then least size.y, then the first. If B has any success, return it (no larger B is tried). If no B succeeds, return the input structure, an empty placement and the last error.
   - Programs are not recomputed (they do not depend on the cut, §11.9).

## Tests (tag `[topology]`; record each red)
Add a helper `CutOf(topology, nmax, &error, JunctionTrackStructure *cut = nullptr)` next to PlacementOf: the same pipeline, then CutJunctionTrackBridges; returns the placement.
1. **LL_RR structure:** restore the original expectation (`{3,{23},false}, {8,{11,15},true}, {9,{3,11,12},true}, {10,{13,3,11,12,17},true}`); the patch changed it.
2. **LL_RR cut (outer, new TEST_CASE "topology: LL_RR series bridges")** at nmax 64: placement non-empty; every bridge has at most 2 crossers; per edge, its bridges' crossed lists concatenated equal the original crossing set; edge 8 has 1 bridge, edge 9 has 2, edge 10 has 3 (the fewest count; if the chosen cut has more, STOP and report every cut tried at the fewest count with its error). Then the patch's placement checks (a) heads gap >= 2 on each copy, in travel order; (b) every Over span strictly inside its own bridge's copy-0 heads; (c) band 2 depth 2, S ports on the last row. X_e1 - X_e2: measure and pin the value, which must be >= 4 (if less, STOP). Red: the function does not exist (record the first assertion failure after a stub that returns the input structure and PlaceJunctionTrackGraph's result on it).
3. **The old limit:** CutOf(LL_RR, 9) succeeds (with the stub: fails, "bridge 3 on edge 10 needs 10 middle tiles, more than 9").
4. **Direct placement still refuses an uncut set:** PlacementOf(LL_RR, 9) (no cut) fails with exactly `"bridge 3 on edge 10 needs 10 middle tiles, more than 9"`. Replace the patch's refusal test with this one.
5. **No cut fits:** CutOf(LL_RR, 1) returns no nodes and a non-empty error.
6. **The T:** CutOf(T, 64) gives the same structure bridges as StructureJunctionTrackGraph and the same placement (item_rows, ports, nodes, size) as PlacementOf(T, 64).
- The T, programs and other placement tests stay unchanged and green; if one fails, STOP.

## Do not
- Do not change programs, order, stack, the placement rules from the patch, or any constant.
- Do not prune the enumeration, cache placements, or add any other choice rule.
- Do not touch the render brief's files or anything outside junction_topology.{h,cpp}, its test file and TESTS.md.

## Stop conditions
- No cut at the fewest count places LL_RR (report each cut and its error).
- An unchanged test fails.
- Anything not covered here.

## Gates
- `tools/junction/jt fast .claude/nodes/build '[topology]' 5` green; `tools/junction/jt gate .claude/nodes/build-rel` green (371 before; report the count).
- One commit: `Add: Junction series bridges, cut chosen by placement (D209, DESIGN-G1 §11.9a)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. TESTS.md `[topology]` row: add "SB1" and the new test count; report the `[topology]` time.

## Report
The standard report, plus from the LL_RR cut at 64: the chosen cut (per bridge: edge, crossed, copy-0 heads, middle), how many cuts were placed and how many failed at the fewest count, the box size (was 58x23), X_e1 - X_e2, and the time of one CutOf(LL_RR, 64) in the Release build. When you have reported, stop.
