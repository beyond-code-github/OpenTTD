# DESIGN-width: the bundle T at width 2 (steps 2-5 of RECON-width-placement)

Design at HEAD 009126b418, read-only. Paths are under `src/junction/` unless noted. Line numbers are at this HEAD.
Builds on RECON-width-placement.md (read it first) under D193, D281-D291. Track k = copy k (D291); no lane split or merge roles at width 2.

## Summary
1. Step 2: structure and track-0 programs at width 2: a row gets Lane, Copy and two BlockSide items (no Prio, no Approach, D291); row edges get track 1 programs; block nodes share no token.
2. Step 3: band edges get track 1 programs (track 0's tokens, BlockSide item of track 1); `JunctionEdgeTracks`; the arrangement reads track 0.
3. Steps 4a-4b: block geometry functions (footprint, side, attach); placement reserves each block's footprint, ties every track end to an attach point, expands the ports per track, places track 0.
4. Step 4c: track 1 of every row edge: own chain, the D118 stagger, heads hug both crosser tracks.
5. Step 5a: track 1 of every band edge as an exact translation of track 0; band bridge pairs on the column axis; S-bend form only. The T places two wide.
6. Steps 5b-5c: the square form at width 2 (nested Corners as translations); the joint loop on the bundle T.
7. Engineer questions: 2 (nested Corner form; a bridge pair over an S-bend that leans downstream). Neither blocks steps 2-5a.
8. Biggest risk: step 5a (band pairs over S-bends): its translation and lean derivations are pinned by tests first; if a pin fails, stop.

## Conventions used by every step
- Headings 0 N, 1 E, 2 S, 3 W; `V(h)` = (VX, VY) as placement's tables; `R(h)` = right of travel = `V((h + 1) % 4)`: R(N) = E, R(E) = S, R(S) = W, R(W) = N.
- Track order: track k + 1 lies one line to the right of track k, seen along travel, on every edge of a bundle. Corners and S-bends keep it.
- "Bundle graph" = `JunctionGraphWidth(graph) == 2`. Every new branch is under it. Width 1 runs today's code, line for line.
- Block node = `node.block != JunctionBlockKind::Plain`. In a bundle graph every node is a block (else refused, step 2).
- Tests: new file `src/tests/junction/topology/junction_bundle.cpp` (add to that folder's `CMakeLists.txt`), tag `[junction][bundle]`. New fast tag `[bundle]`, Debug budget 5 s; step 2 adds its TESTS.md row, measured with `jt fast`.
- Every step runs: `jt fast build '[bundle]' 5` and `jt fast build '[topology]' 5` (Debug); `jt gate build-rel`, `jt sweep build-rel`, `jt identity build-rel` (Release). The identity gate is a gate: SamePlacement over the [topologysweep] cells, LEVELHASH, HOVERPLAN, HOVERNEAR and every existing T and LL_RR golden unchanged.
- The width-2 T in tests (orchestrator, 2026-11-15, after step 2's stop: the hub-built T numbers edges differently and puts the band south of both rows): `JunctionTTopology()` with every group's lanes set to 2, through `BuildJunctionBundleGraph`; order, structure, programs and placement take the one-lane `JunctionTTopology()` and its `ExpandJunctionLanes` paths (the bundle graph's paths are those). Every worked number below uses this numbering. Spec: `MakeTestJunctionSpec()` with `exit_block_along = join_block_along = 4` (a placeholder; step 4a).
- The T's graph (test "the T's track graph"): nodes N0 split row 0, N1 split band 2 (S in), N2 split row 1, N3 merge row 1, N4 merge band 2 (S out), N5 merge row 0. Edges 0-2 In, 3-8 Legs, 9-11 Out. Row 0 runs east, row 1 west. Bridges {5 over {8, 4} doubled}, {8 over {4} single}.

## Step 2. Structure and programs for width 2 (track 0 everywhere, track 1 on row edges)

### 2.1 Data
`topology/junction_topology.h`:
```cpp
/** The widest edge of a track graph: 1 for every graph but a bundle graph of more than one lane (D289). */
uint8_t JunctionGraphWidth(const JunctionTrackGraph &graph);

// JunctionTrackStackItem::Kind, after Approach:
BlockSide, ///< A line of a block node's footprint beside the lanes (D291): where a track of the diverging bundle leaves an exit block or reaches a join block; south of the lanes.
// JunctionTrackStackItem, after kind:
uint8_t track = 0; ///< In a bundle graph, the bundle track the item carries (Lane 0, Copy 1, BlockSide its diverging bundle's track); 0 otherwise.
// RowItems, after copy:
std::array<size_t, 2> block_side = {NONE, NONE}; ///< In a bundle graph, the BlockSide item of each track of the diverging bundle; NONE otherwise.
```
- `FindRowItems` sets `block_side[stack[i].track] = i` for a BlockSide item. Readers: programs (2.2) and placement (4b).
- `JunctionGraphWidth` is the max of `edge.width`. Readers: structure, programs, arrangement, placement.

### 2.2 Algorithm
`StructureJunctionTrackGraph` (`topology/junction_topology.cpp:863`):
- Before the row loop: `width = JunctionGraphWidth(graph)`. Width > 2: fail "a bundle wider than two tracks is not ruled yet". Width 2 with a Plain node: fail "a bundle edge at a plain node is not ruled yet".
- Add `east[r]` exactly as the programs compute it (`:1038-1041`).
- In the row loop, after the joiner and crossing counts (kept, so "more than one joiner" still refuses), when width == 2:
  - For every non-row edge with an end at a node of level 2r+1: if its other end's level is below 2r+1, fail "a row block whose diverging leg goes north is not ruled yet".
  - Push four items and `continue`; `first = east[r] ? 0 : 1`:
```cpp
const JunctionTrackStackItem lanes[2] = {{row, Kind::Lane, 0}, {row, Kind::Copy, 1}};
const JunctionTrackStackItem sides[2] = {{row, Kind::BlockSide, 0}, {row, Kind::BlockSide, 1}};
stack.push_back(lanes[first]); stack.push_back(lanes[1 - first]);
stack.push_back(sides[first]); stack.push_back(sides[1 - first]);
```
- Bridges and turns: unchanged (a crossed row edge's bridge is `doubled`, a band bridge single, as today).

The T at width 2, stack north to south (item: row, kind, track):
`0: 0 Lane 0, 1: 0 Copy 1, 2: 0 BlockSide 0, 3: 0 BlockSide 1, 4: 1 Copy 1, 5: 1 Lane 0, 6: 1 BlockSide 1, 7: 1 BlockSide 0`.
Row 0 runs east, so track 1 is south; row 1 runs west, so track 1 is north.

`ProgramJunctionTrackGraph` (`:1007`), with `bundle = JunctionGraphWidth(graph) == 2`:
- Row edges (`:1160`), new first branch when bundle; `item[0] = row_items[r].lane`, `item[1] = row_items[r].copy` (NONE: fail "a row without its side tracks is not ruled yet"):
```cpp
for (int c = 0; c < 2; c++) {
	std::vector<Step> &out = c == 0 ? program : programs.copies[e];
	if (!edge.from.node) out.push_back(step(StepKind::Fan, 0, item[c]));
	for (uint32_t x : crossings) out.push_back(step(StepKind::Over, 0, x));
	if (!edge.to.node) out.push_back(step(StepKind::Fan, 0, item[c]));
}
continue;
```
- Band edges (`:1206`), track 0 only in this step, bundle branch replacing parts a, c, d:
  - a. Start: from a row node, the Corner as today (`:1233-1236`). From a band node: no token (no Diverge, no Corner).
  - b. Crossings: as today, with the Under order generalised: `north = bundle && !east[row of x] ? 1 : 0`; the pair is `Under(x, heads_south ? north : 1 - north)`, then the other. At width 1 `north` is 0: today's order.
  - c. Arrival on a row node: `Corner(east[r] ? +1 : -1, row_items[r].block_side[track])`, no Converge. When the edge leaves a band split it diverges from and has no crossings, keep the check `corner.hand != side_hand` (same message).
  - d. End at a band node: no token.
- Edge ends (`:1298-1309`): a block end never names a token. `if (edge.from.node) { if (plain(from) && split && ...) first.token = 0; }`, likewise for the end. Ports unchanged.
- Node shapes (`:1335-1345`): a row block's item is `row_items[r].lane` (set after today's line 1345). Band shapes unchanged.

The T at width 2 (track 0 = `edges`, track 1 = `copies`; NR = NO_ROW):
```
e0 {Fan(0)} / {Fan(1)}          e1 {} / -              e2 {Fan(5)} / {Fan(4)}       e3 {} / {}
e4 {C(+1,NR), U(5,1), U(5,0), XS(-1), XC(-1), U(8,0), XC(+1)} / -
e5 {O(8), O(4)} / {O(8), O(4)}  e6 {C(-1,NR)} / -       e7 {C(-1,7)} / -
e8 {O(4), U(5,0), U(5,1), C(+1,2)} / -                  e9 {Fan(5)} / {Fan(4)}       e10 {} / -    e11 {Fan(0)} / {Fan(1)}
```
"-": empty until step 3. Width 1 for comparison: e5 `{O(8), O(4), SM(-1,4)}` / `{SL(-1,4), O(8), O(4)}`.
Every edge end at a node has `token == NO_TOKEN`. Node shape items: N0 0, N5 0, N2 5, N3 5, band nodes NO_ROW.

### 2.3 Width 1 stays identical
- Every new branch is under `width == 2`. Width-1 graphs (and the bundle graph at one lane) have Plain nodes and width 1.
- The new item member defaults to 0, so `operator==` on today's stacks is unchanged; `block_side` stays NONE.
- The Under order reduces to today's (north = 0). The ends and shapes changes are guarded by "block".

### 2.4 Tests
1. "bundle: the T's structure two wide" [junction][bundle]: `structure.stack` equals the 8 items above; bridges `{{5,{8,4},true},{8,{4},false}}`. Red: the stack is today's T stack (Prio, Lane, Approach, PrioAndCopy, Lane, Approach).
2. "bundle: the T's programs two wide": `programs.edges` as above; `copies` of 0, 2, 3, 5, 9, 11 as above, of band edges empty; every node end NO_TOKEN; shape items as above. Red: `copies[5] == {SL(-1,4), O(8), O(4)}`.
3. "bundle: a bundle wider than two tracks is refused": LLL_RRR x LLL_RRR, structure error equals the message. Red: structure succeeds.
4. Gate: identity, gate, sweep; existing "the T's track structure/programs" and the LL_RR goldens unchanged.
Declare the enum value and members first, so each red is an assertion.

### 2.5 Do not, and stop
- Do not touch BuildJunctionBundleGraph, placement, or any width-1 branch. No LaneSplit or LaneMerge at width 2.
- Stop if a width-1 golden or the identity gate moves, or `[topology]` goes over budget.

### 2.6 Split
Fits one hour. If not: 2a structure (tests 1, 3), 2b programs (test 2).

## Step 3. Band edges' track 1 programs

### 3.1 Data
`topology/junction_topology.h`:
```cpp
/** The tracks an edge's placement and render carry: 2 for every edge of a bundle graph (D291: track k is copy k), 2 for a doubled edge of a width-1 graph, else 1. */
uint8_t JunctionEdgeTracks(const JunctionTrackGraph &graph, const JunctionTrackPrograms &programs, uint32_t e);
```
Doc comment changes: `JunctionTrackPrograms::copies` "a doubled edge's copy 1, or track 1 of every edge of a bundle graph"; `JunctionTrackStep::copy` "Under: which copy (track) of a doubled bridge is overhead".

### 3.2 Algorithm
- `ProgramJunctionTrackGraph`, band part: run parts a-d for `track` 0 and, when bundle, 1, writing to `programs.edges[e]` then `programs.copies[e]`. Only part c differs: `block_side[track]`.
- `ArrangeJunctionTrackGraph` (`:1553`): `if (!programs.copies[e].empty() && graph.edges[e].width == 1) NOT_REACHED();`. The arrangement of a bundle band edge is its track 0's (track 1 is its translation, step 5). Doc line on the struct.
- `JunctionEdgeTracks`: `graph.edges[e].width == 2 ? 2 : (JunctionIsDoubledEdge(programs, e) ? 2 : 1)`.

The T: `copies[4] == edges[4]`; `copies[6] == {C(-1,NR)}`; `copies[7] == {C(-1,6)}`; `copies[8] == {O(4), U(5,0), U(5,1), C(+1,3)}`; `copies[1]`, `copies[10]` empty.

### 3.3 Width 1
The track loop runs once at width 1. The NOT_REACHED keeps its width-1 meaning. `JunctionEdgeTracks` equals today's doubled test at width 1.

### 3.4 Tests
1. "bundle: band track 1 is track 0 with its own items": the four copies above; for every band edge, the tokens equal track 0's except a Corner's index (BlockSide track 0 to track 1). Red: copies empty.
2. "bundle: the T's arrangement reads track 0": `ArrangeJunctionTrackGraph` returns; its unders are e4 tokens 1, 2 (copies 1, 0) and e8 tokens 1, 2 (copies 0, 1). Red: the process aborts at NOT_REACHED (jt reports the case failed).
3. "bundle: edge tracks": all 12 edges give 2 on the width-2 T; on the width-1 T only edge 5 gives 2. Red: a stub returning `JunctionIsDoubledEdge` gives 1 for edge 0.
4. Gate as step 2.

### 3.5 Do not, and stop
Do not build arrangement entries per track. Stop if any other NOT_REACHED fires on the bundle T (report the line).

### 3.6 Split
Under an hour; no split.

## Step 4a. Block geometry (pure functions)

### 4a.1 Data
`junction_spec.h`, `JunctionSpec`:
```cpp
uint8_t exit_block_along = 0; ///< Placeholder until the block templates (D289, D291): every exit block's length along its trunk, in tiles; 0 refuses a bundle placement. Set by tests.
uint8_t join_block_along = 0; ///< Placeholder until the block templates (D289, D291): every join block's length along its trunk, in tiles; 0 refuses a bundle placement. Set by tests.
```
`topology/junction_topology.h`:
```cpp
/** A block node's footprint (D289, D291): its length along its trunk's heading from its anchor (the upstream end, on the trunk's track 0), and its width in lines across it. */
struct JunctionBlockFootprint {
	uint8_t along; ///< Tiles along the trunk.
	uint8_t across; ///< Lines across the trunk: the trunk's tracks and the diverging bundle's.
	bool operator==(const JunctionBlockFootprint &) const = default;
};

/** Where one track of an edge meets a block (D291), from the block's anchor: along the trunk's travel and across it in lines to the right of travel. */
struct JunctionBlockAttach {
	int along; ///< -1: the tile just upstream of the footprint; footprint.along: just downstream.
	int across; ///< 0..width-1 the trunk's and the straight leg's tracks; the diverging leg's on its side (JunctionBlockSide).
	bool operator==(const JunctionBlockAttach &) const = default;
};

/** The footprint of a block kind: the placeholder until the templates; Plain is a 1 x 1 block (placement does not read it), Exit and Join are {spec's along, 2 x width}. */
JunctionBlockFootprint JunctionBlockFootprintOf(JunctionBlockKind kind, uint8_t width, const JunctionSpec &spec);

/** The side of a block node's diverging leg, seen along its trunk's travel: +1 right, -1 left. A row node's and a band split's is its shape's hand; a band merge's is the opposite (its converging leg's first curve turns toward the trunk). */
int8_t JunctionBlockSide(const JunctionTrackGraph &graph, const JunctionTrackOrder &order, const JunctionTrackPrograms &programs, uint32_t node);

/** The attach of track k of edge e at block node v (D291): a split's trunk at along -1, its legs at footprint.along; a merge's legs at -1, its trunk at footprint.along; across k on the trunk's lines, side > 0 ? width + k : k - width for the diverging leg. */
JunctionBlockAttach JunctionBlockAttachOf(const JunctionTrackGraph &graph, const JunctionTrackPrograms &programs, uint32_t v, int8_t side, JunctionBlockFootprint footprint, uint32_t e, uint8_t track);
```
Readers: placement (4b). Writers: tests set the spec fields; the template generator replaces the placeholder later.

### 4a.2 Algorithm
Pure functions, rules in the doc comments. The trunk, straight and diverging legs come from `programs.nodes[v]`.
The T, along 4: sides N0 +1, N5 +1, N2 -1, N3 -1, N1 -1, N4 -1. Attach (along, across) per track k:
```
N0 split: e0 (-1, k)  e3 (4, k)  e4 (4, 2+k)        N2 split: e2 (-1, k)  e5 (4, k)  e6 (4, k-2)
N5 merge: e3 (-1, k)  e8 (-1, 2+k)  e11 (4, k)      N3 merge: e5 (-1, k)  e7 (-1, k-2)  e9 (4, k)
N1 split: e1 (-1, k)  e8 (4, k)  e7 (4, k-2)        N4 merge: e4 (-1, k)  e6 (-1, k-2)  e10 (4, k)
```
Tile = anchor + along V(h) + across R(h), h = the trunk heading. N0 (h E): e4 track 0 at anchor + (4, 2), the BlockSide 0 row. N2 (h W): e6 track 0 at anchor + (-4, +2), the BlockSide 0 row of row 1.

### 4a.3 Width 1
New members default to 0; nothing calls the functions at width 1.

### 4a.4 Tests
"bundle: block geometry": the footprints ({4, 4} for Exit and Join at width 2, {1, 1} Plain), the six sides and the 36 attaches above. Red: stubs return zeros.

### 4a.5 Do not, and stop
No placement change. Stop if a side differs from the list: the band merge rule is derived from `SV(+1)` on e6 at width 1 (the S-bend relations, `placement.cpp:2428-2437`: e6 starts east of N4).

### 4a.6 Split
Small; can share a commit with 4b if the agent prefers (two commits recommended).

## Step 4b. Placement with blocks, track 0 only

### 4b.1 Data
In `PlaceTrackGraph` (`placement/junction_placement.cpp:87`), local, all created only when bundle, right after `se_pt` (`:207`), in this order:
```cpp
std::vector<int> block_end_pt(graph.nodes.size(), -1); ///< Per block node: its footprint's downstream end on trunk track 0 (along - 1 from node_pt).
std::vector<std::array<std::array<int, 2>, 2>> attach_pt(edge_count, {{{-1, -1}, {-1, -1}}}); ///< Per edge, per end (0 start, 1 end), per track: the point of its tile at a block node; -1 elsewhere.
std::vector<int> track_port_pt(n, -1); ///< Per port slot: track 1's port point.
std::vector<int> port_step(n, 0); ///< Per port slot: R(heading at the port) along the port's line, +1 or -1 (rows on W/E, columns on S).
```
`Elem` gains `bool attach = false; ///< A Tile at a block's attach point: the edge's own first or last tile, outside the footprint.` and `int track = 0; ///< The track of a port Tile or Fan (its port row is the track's).`
A helper `port_row_of(const Elem &el) = port_row[el.slot] + el.track * port_step[el.slot]` replaces `port_row[el.slot]` at `:548, :736, :949, :955, :1035, :1242, :1247, :1797, :3291`.
`JunctionTrackPlacement` (`placement/junction_placement.h`):
```cpp
std::vector<std::vector<TileIndexDiffC>> track_ports; ///< In a bundle graph, per port slot, per track: its boundary tile (track 0's is ports[s]); empty at width 1.
std::vector<std::array<std::vector<TileIndexDiffC>, 2>> attach; ///< In a bundle graph, per edge, per end (start, end), per track: its tile at a block node; empty at a port end and at width 1.
std::vector<TileIndexDiffC> block_ends; ///< In a bundle graph, per node, its footprint's downstream end on trunk track 0 (nodes[v] is the upstream end, the anchor); empty at width 1.
```
Readers: render (step 6), level solve (step 7), tests.

### 4b.2 Algorithm
- Refuse first: a bundle with `JunctionBlockFootprintOf(...).along == 0`: "a block without its footprint is not ruled yet"; `across != 4`: "a block other than two bundles across is not ruled yet".
- Notation (`:786-798`): each slot takes `w` rows (w = graph width): `notation[s] = port_step[s] > 0 ? row : row + w - 1; row += w`, the group gap as today. `port_step[s] = R(h).y` of the port heading (W in: E; W out: W; E in: W; E out: E). S spacing (`:2142-2147`): slot i's first column `c_i`, `c_{i+1} = c_i + w + gap`; track 0 at `c_i` when `port_step > 0`, else `c_i + w - 1`; `port_step = R(h).x` (S in: N; S out: S). At w = 1 both reduce to today's.
- Ports (`:2134-2147`): `track_port_pt` gets the column relation of its slot (W: 0; E: `xe_pt`; S: `port_pt[s] + port_step`, row `se_pt`). Its row on W/E is the number `port_row[s] + port_step[s]`, set with the ports at `:3285`.
- Block relations, why("block attach", e, v), both phases, exact, for every edge end z at a block v and track k < 2:
  `eq(0, attach_pt[e][z][k], node_pt[v], a VX[h] + c RX[h])`, `eq(1, ..., a VY[h] + c RY[h])`, (a, c) from `JunctionBlockAttachOf`, h = `programs.nodes[v].heading`.
  why("block footprint", UINT32_MAX, v): `eq` both axes `block_end_pt[v] = node_pt[v] + (along - 1) V(h)`.
- Band gap and S edge (`:2113-2128`): for a block node apply each relation to `node_pt` and to `block_end_pt`.
- Band chains (`:282-283`, `:291-294`, `:361-364`): `start`/`stop` at a block are `attach_pt[e][z][0]`; the begin and finish Tiles get `attach = true`. Ends name no token, so no bend takes the attach point (`:341-342` give new points).
- Row chains (`:830`, `:871`): a block end is `tile_elem(attach_pt[e][z][c], -1)` with `attach = true`. When bundle, build `c < 1` only (4c raises it) and skip the role refusal (`:820-826`).
- HeadsPair in the general chain loop (`:2186+`), reached only by bundle edges: Y `eq(1, first, item_pt[index], 0)`, `eq(1, last, ...)`; `floor_kind` returns `FloorKind::Heads` for `HeadsPair`. Its index (`:865`) when bundle and no fan or S-bend before: `c == 0 ? lane : copy`.
- The doubled-leg block (`:2199`) gets `&& !bundle`. A new bundle block after it, X phase, for a row edge with bridges: the hug of `:2247-2278` over the built chains (pass c reads `nth_heads` of chain c), crosser tiles from `edge_tiles_all(x)` = `edge_tiles_of(chain[x])` plus `edge_tiles_of(chain_copy[x])` when not empty.
- Occupancy (`:1372`): skip the node occupant of a block (`:1465`); per block, per line c of its 4 lines, one footprint occupant over along tiles: rows for a row block (row = anchor row + c RY, columns anchor .. anchor + (along - 1) VX, bits X, footprint VX[h]); for a band block one occupant per row a (columns from the smallest to the largest line, bits Y, footprint +1). Node v, edge UINT32_MAX: `related()` already relates them to the node's edges and to each other. Row walk (`:1241`): add an attach Tile like a port tile (`el.slot >= 0 || el.attach`).
- `compute_node_tracks` (`:1282`): skip block nodes.
- `two_chains(e) = !chain_copy[e].empty()` replaces `IsDoubledEdge(e)` at `:894, :1209, :2045` and `doubled_edge(e)` in `sym_routes` (`:1015-1030`); the band branch of `sym_routes` (`:1047`) reads `c == 1 && two_chains(e) ? chain_copy[e] : chain[e]`. A bundle path has 2 routes only when every edge has two chains. Route c starts at track c's port point and row.
- Output (`:3297+`): fill `track_ports`, `attach`, `block_ends`. Then, when bundle, fail "the placement of a second track is not ruled yet" (after the legality pass at `:3280`, so the message proves that track 0 and the blocks placed legally).

### 4b.3 Width 1
- No new point is created at width 1, so every point id, `columns` and `x_floors` stay. `w = 1` gives today's notation and S spacing.
- `two_chains` equals today's tests: at width 1 `chain_copy[e]` is non-empty exactly for a doubled row edge (`:827`).
- The HeadsPair floor and loop handling are unreachable at width 1: every HeadsPair chain takes the doubled-leg block and `continue`s (`:2279`).
- The new fields stay empty.

### 4b.4 Tests
1. Outer, "bundle: the T places its blocks and track 0": `PlaceJunctionTrackGraph` on the width-2 T; error equals "the placement of a second track is not ruled yet". Red: "a doubled leg without its lane split and merge is not ruled yet".
2. "bundle: a block without its footprint is refused": along 0 in the spec. Red: test 1's red message.
3. Gate: identity (SamePlacement, LEVELHASH, HOVERPLAN, HOVERNEAR), gate, sweep; "the T's placement" golden (`tests/.../junction_topology.cpp:953`) unchanged.

### 4b.5 Do not, and stop
- Do not build track 1 chains. Do not change the floor table's entries or the doubled-leg (lane roles) relations. Do not move the Prio element code.
- Stop if the identity gate moves; if a failure other than the expected one appears (report the why chain); if `[bundle]` goes over budget.

### 4b.6 Split
About an hour. If larger: 4b1 ports per track and notation (identity only), 4b2 blocks and attach.

## Step 4c. Track 1 of row edges

### 4c.1 Data
No new members. Reason strings: "bridge pair stagger".

### 4c.2 Algorithm
- Row chains: build `c < 2` for bundle edges, from `programs.copies[e]`. Port Tiles and Fans of track 1 carry `track = 1` and `track_port_pt` / port row; attach Tiles `attach_pt[e][z][1]`; HeadsPair index Copy (from 4b's rule).
- General chain loop: for a bundle row edge run the element and run relations for chain 0, then chain 1 (wrap `elems` in a `c` loop; the bridge-over-S-bend part reads only Heads, so it does nothing here).
- Fans: why("track pair", e): `eq(0, fan1.first, fan0.first, 0)`, `eq(0, fan1.last, fan0.last, 0)`. Their rows come from ports and items; equal sizes because port rows and items keep the same order.
- Bundle bridge block: why("bridge pair stagger", e, UINT32_MAX, r): per bridge b, `along(heading, heads0(b).first, heads1(b).first, 1, true)`, the same for `last` (track 1 one tile upstream, D287, D118). The hug now runs both passes (heads0 on track 0's row, heads1 on track 1's row, shifted by -u).
- Output (`:3380-3405`): for a bundle row edge fill `result.copies[e]` per token like track 0; in the doubled branch handle a Fan token (its element's span).
- The final refusal narrows to band edges: "the placement of a band edge's second track is not ruled yet".

Edge 5's pair (h W, s = -1, u = +1; A, Z attach tiles; f, l the heads), all at width 2:
```
y(f0) = y(l0) = y(item 5)        y(f1) = y(l1) = y(item 4) = y(item 5) - 1
x(A0) = x(A1) = x(N2) - 4         x(Z0) = x(Z1) = x(N3) + 1               (block attach)
x(Ak) - x(fk) >= 2                x(lk) - x(Zk) >= 2                      (chain run, Node-Heads, Heads-PrioNode)
x(f1) - x(f0) = 1                 x(l1) - x(l0) = 1                       (stagger: track 1 one tile east, upstream)
per crosser tile z on row y(item 5): x(f0) - (x(z) + dz) >= 1, (x(z) + dz) - x(l0) >= 1
per crosser tile z on row y(item 4): the same with f1, l1                 (hug; tiles of both tracks of 4 and 8)
```

### 4c.3 Width 1
Only bundle edges take the new code; chain_copy and its relations at width 1 are today's.

### 4c.4 Tests
1. Outer: test 4b.1's expected message becomes "the placement of a band edge's second track is not ruled yet". Red: "the placement of a second track is not ruled yet".
2. Gate as 4b.

### 4c.5 Do not, and stop
No relation for band track 1. Stop on any other failure (report the why chain).

### 4c.6 Split
Under an hour.

## Step 5a. Track 1 of band edges, S-bend form; the T places

### 5a.1 Data
No new types. Reason strings: "track pair", "bridge pair stagger". Doc change: `JunctionPlacementSystem` bend and element `copy` "1: the copy of a doubled row edge, or track 1 of a bundle edge".

### 5a.2 Algorithm
- Band chain builder (`:277-365`): for a bundle edge build chain 1 after chain 0 from `programs.copies[e]` (new points in chain order). A Cross token of chain 1 joins chain 0's group (`group_index.at({e, head})`, no new `recorded_form_groups` entry).
- Translation relations, X and Y, exact, why("track pair", e), per present element i of chain 1 that is a Corner, S-bend, Fan or Heads, against element i of chain 0: `first1 = first0 + D`, `last1 = last0 + D`, h = the element's start heading (`fhin` for a form bend), eta its hand:
  - S-bend, Fan: D = R(h).
  - Corner: D = R(h) - eta V(h). (Nested Corners as translations: the outer track turns one tile later, the inner one earlier. Equal sizes; see Q-W1.)
  - Heads: D = R(h) - V(h) (one line right, one tile upstream: D287).
  A form bend's relations carry its group and form (`form_group_now`, `form_now`, `record_only_now` as `:2366-2370`).
- Absent bends of chain 1 keep their own size-0 relations; they get no translation relation.
- Bridge over S-bend (`:2489-2602`) runs for chain 0's Heads only. For a bundle crosser the head rows widen to both crosser tracks: with `dX = -R(h_X).x * VX[side]` and `d = form.s * dX`, the first head's constant gains `max(0, d)` (north) or `min(0, d)` (south), the last head's the other. Then the lean check: `form.s * R(h_B).x * VX[side] == -VY[h_B]`, else `build_why = "a bridge pair over an S-bend that leans downstream is not ruled yet"` (Q-W2).
- Square form at width 2: when the choices give form 1 to a bundle group, `build_why = "a square crossing of a bundle is not ruled yet"` (5b removes it).
- Kind-4 row changes (`:911`): `if (!bundle && !programs.copies[e].empty()) continue;` so track 0 keeps its push; track 1 follows by translation.
- Occupancy: band chains of chain 1 with copy 1 (`:1468-1492`, loop both chains).
- `collect_bridges` (`:2034`): Heads of chain 1 too. `heads_of(b, 1)` for a single bridge of a bundle edge reads chain 1's Heads; the span and length loop (`:3455`) runs two copies for it; `placement.bridges[b][1]` is filled.
- `tiles_under` (`:3349`) reads both chains of the crosser.
- Fixed and measured sizes (`:3130, :3227, :3250`): chain 1's free S-bends take chain 0's token choice (`fixed->m[e][tok]`) and are measured like chain 0's.
- Remove the band refusal of 4c.

The T, track 1 = track 0 + D per bend token (form 0):
```
e4: t0 Corner (h E, +1): (-1, +1)   t3 XS (h S): (-1, 0)   t4, t6 XC: absent, no relation
e6: t0 Corner (h W, -1): (-1, -1)   e7: t0 Corner (h N, -1): (+1, -1)
e8: t0 Heads (h N): (+1, +1)        t3 Corner (h N, +1): (+1, +1)
```
Bridge 1 (8 over 4): `bridges[1][1] = bridges[1][0] + (+1, +1)`. Bridge 0 (row 1): `bridges[0][1] = bridges[0][0] + (+1, -1)`.
e8 over e4's S-bend: `side` E, `form.s = +1` (pinned by test 2), `dX = +1`, `d = +1`: e8's heads hug rows lo - 1 and hi + 2; the lean check gives `1 * 1 * 1 == 1`, upstream.

### 5a.3 Width 1
Chain 1 of a band edge exists only in a bundle; group creation, the kind-4 condition and `heads_of` reduce to today's at width 1.

### 5a.4 Tests
1. Outer, "bundle: the T places two wide on flat ground": `PlaceJunctionTrackGraph` succeeds. It checks: every bend span of `copies[e]` equals the `edges[e]` span plus D (table above); the two bridge pairs as above; `attach` tiles equal anchor plus the 4a offsets; `block_ends`; the size, pinned on first green and recorded in STATUS. Red: "the placement of a band edge's second track is not ruled yet".
2. "bundle: the S-bend column form leans as derived": `JunctionSBendColumnForm(2, -1).s == 1`. A pin of the derivation; it is green before the code, and if it is red, stop.
3. "bundle: a square crossing of a bundle is refused": choices from test 1 with `form[4][3] = 1`. Red: test 1's red.
4. Gate as 4b.

### 5a.5 Do not, and stop
- No square form. No new free variable for track 1. No relation that reads a rendered layout.
- Stop if test 2 fails; if occupancy refuses a translated pair (the legality derivation is then wrong: report the tiles); if the lean check fires on the T.

### 5a.6 Split
The largest step (about an hour and a half). Split if needed: 5a1 chains, translations, occupancy and routes with the bridge 8 pair, test 1 without the size pin; 5a2 the S-bend widening, lean check, refusal and pin.

## Step 5b. Square form at width 2
- 5b.1 Data: none.
- 5b.2 Algorithm: drop the 5a refusal. Bridge over run (`:2503-2546`): run the relation set for every pair (bridge chain i, crosser chain j), with chain i's Heads and chain j's Corners. With the Corner rule of 5a, a square group's two Corners share one D = R(h_run) + eta V(h_run), so the runs of both tracks have equal length (derived: R(h) - eta V(h) = R(h + eta) + eta V(h + eta)).
- 5b.3 Width 1: the pair loop is one pair at width 1.
- 5b.4 Tests: "bundle: the T places with a square crossing" (5a.3's choices) succeeds; e4's two Corner spans of track 1 are track 0's plus D; the run lengths are equal. Red: "a square crossing of a bundle is not ruled yet". Gate.
- 5b.5 Stop if the occupancy refuses the nested Corners (report; Q-W1 option b is the fallback, not to be chosen by the agent).
- 5b.6 Under an hour.

## Step 5c. The joint loop on the bundle T
- 5c.1 Data: none expected.
- 5c.2 Algorithm: none expected. The recording already walks both chains (`:536`). Lengths are per (bend, copy), tied by the recorded translation rows. A form is per group, shared by both tracks. `SolveJunctionJoint` reads choices from copy 0 (`junction_joint.cpp:503, :538, :669`) and counts signals per (path, track) (`:461-482`).
- 5c.3 Width 1: no code change.
- 5c.4 Tests: "bundle: the joint loop settles on the bundle T": `SolveJunctionJointLoop` from 5a's choices gives outcome "settled", and its choices place. If it is green without code, commit the test alone. If it fails, stop and report the outcome line (it names the relations).
- 5c.5 Stop on any change needed in `JunctionMilp`.
- 5c.6 Small.

## What steps 6-7 need from this design
- Render (6): routes per (path, track); track c's tokens and spans (`programs.copies`, `placement.copies` for every bundle edge); `placement.bridges[b][c]` for every bridge of a bundle edge; start and end at `track_ports`; a route stops and resumes at the block's `attach` tiles; no node-piece check at a block; tags `copy = track`; fix the `JunctionTrackRoute::copy` comment. Open: until the block templates (B1), how a route crosses a block (one route per edge run, or wait for templates). This is the orchestrator's call at step 6.
- Signals and levels (7): `JunctionRouteEvents` gated on a LaneSplit token being present; `SolveJunctionLevels` takes the ports of every track (`track_ports`) and the blocks' footprints as reserved tiles.

## Unknowns resolved
- Track 1's port tile: the slot's tracks are adjacent (the notation's gap 0), groups one tile apart (the notation's `_`), and track 1 is one line to the right of travel (step 4b). Source: the one-lane topology's groups times the graph width.
- Node tile per track for a block: none. A block has an anchor (node_pt, upstream end on trunk track 0), a footprint, and one attach tile per track per edge end outside it (`JunctionBlockAttachOf`). No edge shares a tile with a block.
- Two parallel decks through the level solve: the doubled row bridge is already two parallel decks on adjacent rows; it is built in game for the T and LL_RR, and the solve keys tiles, not bridges. A band pair is the same on the column axis. Step 7's test confirms it.
- Joint model: as 5c. `JunctionMilp` has no edge or copy notion and needs none.
- Build and pricing seam: `game/` reads only the `JunctionLayout` (no placement type is read there; grep). Width adds tracks and bridges to the same layout. No change.
- Prio and Approach at a join block: none (D291). The block's footprint is reserved as footprint occupants over its 4 lines, and its BlockSide items are the rows its diverging bundle uses.

## Questions for the engineer

**Q-W1. How a bundle's two tracks take a 90-degree Corner.**
Context: two parallel tracks turning together; the inner track's Corner has size k (the curve minimum).
Options: (a) loose: the outer Corner is the inner one shifted (one line out, one tile on). Same size; its diagonal is a full diagonal step from the inner one. A pure translation: no new variables, legal by derivation (no shared or touching tile). Costs one tile per Corner across the bundle. (b) tight: the outer Corner is one larger and starts level with the inner one; the diagonals share tiles (Upper|Lower). One tile smaller; a second size per Corner and a size cross-over in square groups.
Recommendation: (a) now; (b) later as a compaction toggle if footprint matters.
Reversible: yes.

**Q-W2. A bridge pair over an S-bend that leans downstream.**
Context: our parallelogram puts track 1's heads one tile upstream (D287, D118). Over an S-bend form crossing, the diagonal decides where the second deck must be, and it can be one tile downstream.
Options: (a) refuse that S-bend form (the square form is still offered; the T does not need it); (b) mirror the parallelogram there (track 1's heads one tile downstream). That is a different-looking bridge pair.
Recommendation: (a) until a hub needs it.
Reversible: yes.
