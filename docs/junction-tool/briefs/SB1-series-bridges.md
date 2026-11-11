# SB1: series bridges, a lane's crossings cut into bridges of at most two crossers (D209; DESIGN-G1 §11.9)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (persistent; Debug `build`, Release `build-rel`). First: `git checkout -B wt-sb1 <HEAD>` (given at launch). You are the only writer there. `tools/junction/jt` builds openttd_test before running.
- Today StructureJunctionTrackGraph makes one doubled bridge per crossed lane leg over all its crossings; LL_RR's w2 leg (edge 10) gets one bridge over 5 crossers, 10 middle tiles. D209 (engineer) caps a bridge's crossers and puts the rest on further bridges in series on the same lane, with one plain tile (the signal slot) between consecutive heads. Programs do not change. Placement learns several bridges per lane.

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ part); docs/junction-tool/TESTS.md (the `[topology]` row).
- docs/junction-tool/DESIGN-G1.md §11.8 and §11.9 (the end of the file); DECISIONS.md D208, D209.
- src/junction_topology.h :154-230 (stack item, JunctionTrackBridge, structure, steps, programs, placement).
- src/junction_topology.cpp: StructureJunctionTrackGraph's "Bridges." block (:626-643); PlaceJunctionTrackGraph (:880 to its end, ~:1600). Every use of `bridge_of`, `doubled_row`, `nh0`, `fh0`, `nh1`, `fh1` is in that function (lines 950-952, 1038-1044, 1136-1142, 1230-1300, 1485-1600).
- src/tests/junction_topology.cpp: the two structure tests (:300-332), PlacementOf, the T and LL_RR placement tests and the LL_RR refusal test (:540-600).

## Design (settled)
### Structure
In junction_topology.h, just above `struct JunctionTrackBridge`:
```cpp
/** The most crossings one bridge passes over; a longer crossing set goes on further bridges in series (D209, provisional until the D208 rig experiment). */
static constexpr size_t JUNCTION_BRIDGE_MAX_CROSSERS = 2;
```
Update JunctionTrackBridge's class comment to: `/** A bridge: one edge carried over a contiguous set of at most JUNCTION_BRIDGE_MAX_CROSSERS crossings (D203, D209). */`, and JunctionTrackStructure::bridges' comment to `///< Sorted by edge; an edge's bridges in its travel order.`

In the "Bridges." block, replace the one push per crossed row edge by: cut `order.edge_crossings[e]` (already in travel order) into consecutive chunks of at most JUNCTION_BRIDGE_MAX_CROSSERS from its start (5 give 2, 2, 1), one `{e, chunk, true}` per chunk, in order. The branch-bridge loop and the stable sort stay as they are (the sort is stable, so an edge's chunks keep their order).

### Programs
No change. Do not edit ProgramJunctionTrackGraph. Its `doubled_bridge` and `bridge_over` lambdas already scan every bridge.

### Placement
1. `bridge_of` (:950) becomes `std::vector<std::vector<int>> bridges_of(edge_count)`: per edge, its structure bridge indices in order (push_back while looping b ascending). `doubled_row(r)`: `!bridges_of[leg_edge[r]].empty() && structure.bridges[bridges_of[leg_edge[r]][0]].doubled`.
2. The band case (:1038): `if (bridges_of[e].size() != 1 || structure.bridges[bridges_of[e][0]].doubled || ...crossed.size() != 1)` with the same error text; `b = bridges_of[e][0]`.
3. RowPts: `nh0, fh0, nh1, fh1` become `std::vector<int>`, one point each per bridge of the row's leg (size `bridges_of[leg_edge[r]].size()` when the row is doubled, else empty); create them with new_pt where the scalars were created. (If leg_edge or doubled_row is not yet known at :1139, move the RowPts creation after it; nothing else moves.)
4. Y phase (:1232-1235): the four `eq(1, ...)` lines apply to every k.
5. X phase, the doubled chain (:1254-1263), with g bridges, k = 0..g-1:
   - `along(heading, rp.nh0[0], rp.lane_split, 2, true)`; `along(heading, rp.nh1[0], rp.sb1_last, 1, true)`.
   - for every k: `along(heading, rp.nh0[k], rp.nh1[k], 1, true)` and `along(heading, rp.fh0[k], rp.fh1[k], 1, true)` (copy 1 = copy 0 + u per bridge).
   - for every k > 0: `along(heading, rp.nh0[k], rp.fh0[k - 1], 2, false)` (one plain tile between consecutive heads: the signal slot). This is the only new constraint.
   - after the bridges: `along(heading, rp.sb0_first, rp.fh0[g - 1], 1, true)`; `along(heading, rp.lane_merge, rp.fh1[g - 1], 2, true)`; the rest unchanged.
6. The hug block (:1267-1300) runs once per bridge k of the row, over that bridge's `crossed` only, with `near`/`far` = `nh0[k]`/`fh0[k]` (pass 0) and `nh1[k]`/`fh1[k]` (pass 1), and its own `have/low/high` reset per bridge; the two final `eq` lines use bridge k's heads.
7. `heads_of` takes a structure bridge index `b` (not an edge): doubled → the row r with `leg_edge[r] == structure.bridges[b].edge`, k = the position of b in `bridges_of[that edge]`, return the k-th heads of the copy; single → the edge's chain Heads element as today.
8. Token spans: a doubled leg's Over token for crossed edge x uses the bridge of that edge whose `crossed` holds x; a single bridge's Over uses `bridges_of[e][0]`; an Under token beneath edge x uses the bridge of x whose `crossed` holds this edge (doubled or not). If no such bridge exists, keep the existing "a crossing edge misses its bridge" failure.
9. The bridges loop at the end iterates structure indices b; the refusal text becomes `fmt::format("bridge {} on edge {} needs {} middle tiles, more than {}", b, bridge.edge, middle, spec.max_bridge_middle)`.

## Tests (tag `[topology]`; record each red)
1. **LL_RR structure** (edit the existing expectation): `{3, {23}, false}, {8, {11, 15}, true}, {9, {3, 11}, true}, {9, {12}, true}, {10, {13, 3}, true}, {10, {11, 12}, true}, {10, {17}, true}`. Red: the old single bridges.
2. **T structure, T programs, LL_RR programs, T placement:** unchanged, must stay green untouched (the T's W bridge has 2 crossers; programs do not change). Do not edit their expectations; if one fails, STOP.
3. **LL_RR placement at nmax 9** (new TEST_CASE "topology: LL_RR series bridges fit the old limit"): `PlacementOf(LL_RR, 9, ...)` succeeds (red today: "bridge 10 needs 10 middle tiles, more than 9"). Then, at nmax 64 (in the existing LL_RR placement test, replacing the `edge == 10` / `n == 10` check):
   - (a) for each pair of consecutive structure bridges b, b+1 with the same edge, on each copy c in {0,1}: the far head of b and the near head of b+1 differ by at least 2 along the carrier's travel (x for row bridges), i.e. `abs(placement.bridges[b+1][c].start.x - placement.bridges[b][c].end.x) >= 2`, both on the same y, and in travel order (b+1 further along than b);
   - (b) for every doubled bridge b and every x in its crossed: the leg's copy-0 Over span for x (`placement.edges[edge][t]` where programs.edges[edge][t] is Over(x)) has both tiles strictly between bridge b's copy-0 heads in x;
   - (c) band 2 depth and S ports on the last row stay unchanged (if one fails, STOP). X_e1 - X_e2 may grow: under w1, X_e1 (11) and X_e2 (12) are now on different bridges, so the one-tile gap can push them apart; set the check to the measured value, which must be >= 4, and report it.
4. **Refusal** (edit the existing refusal test): place at 64, find the largest copy-0 middle M and its bridge b and edge; place again at M - 1 and CHECK the error equals `fmt::format("bridge {} on edge {} needs {} middle tiles, more than {}", b, edge, M, M - 1)` and no nodes. (If several bridges share M, b is the first: the loop's first failure.) Red: the old message format.

## Do not
- Do not change programs, the order, the stack, the branch-bridge rule, or any constant other than adding JUNCTION_BRIDGE_MAX_CROSSERS.
- Do not add a middle-tile cap of your own; the only tile limit is spec.max_bridge_middle.
- Do not place signals; the plain tile is only kept free by the constraint.
- Do not touch the render, the template builder, or junction_nodes.

## Stop conditions
- Any T test or program test changes.
- The X phase has no solution for LL_RR (Bellman-Ford does not settle), or a hug constraint fails for a group: report the bridge, its crossers and the tiles.
- Band 2 depth or the S-port check changes, or X_e1 - X_e2 drops below 4.
- Anything in this design that does not fit the code as it stands (e.g. a fifth use of bridge_of not listed).

## Gates
- `tools/junction/jt fast .claude/nodes/build '[topology]' 5` green; then `tools/junction/jt gate .claude/nodes/build-rel` green (371 before; report the new count).
- One commit: `Add: Junction series bridges (D209, DESIGN-G1 §11.9)`, ending with the attribution line `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.
- TESTS.md `[topology]` row: add "SB1" to the list and bump the test count by one.

## Report
The standard report (references read, commits, red evidence per test, types and members added or removed, suite results, stops, state), plus, measured from the LL_RR placement at 64: the box size (was 58x23); per structure bridge: edge, crossed, copy-0 heads and middle; the X_e1 - X_e2 value. When you have reported, stop.
