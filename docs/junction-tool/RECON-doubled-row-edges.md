# RECON: doubled bridges on a row's in and out edges (D229)

Head e51785c81b, read-only. Paths are under src/junction/ unless stated. "jt.cpp" = topology/junction_topology.cpp, "jp.cpp" = placement/junction_placement.cpp.

## 1. Programs

Leg program (jt.cpp:1110-1116), only when `row_doubled[r]` and `e == leg_of[r]`:
- `programs.edges[e]` = copy 0 = `[Over x..., SBend(lane, LaneMerge)]`: copy 0 keeps the row it arrives on (the `north` item, the Copy/PrioAndCopy track) through the bridge and bends onto the Lane item after it.
- `programs.copies[e]` = copy 1 = `[SBend(lane, LaneSplit), Over x...]`: copy 1 bends onto the Lane item before the bridge and crosses on it.
- `Over.index` is the crossed edge, `SBend.index` the stack item it ends on (topology.h:200-225). A token's `copy` field is only for `Under`.
- Both copies are listed from the same `order.edge_crossings[e]`; `structure.bridges` has one `{e, crossed, doubled=true}` per crossed row edge (jt.cpp:~920), so bridges are already made for in/out edges. Only the programs fail.

In edge fan (jt.cpp:1117-1120): the in edge ends at the row's first node (the exit split). With a doubled leg that node and the leg start lie on `north`, so the in edge's Fan ends on `north`; the lane item is only reached by copy 1's split S-bend. An undoubled row fans to `lane`. The out edge always fans from `lane` (jt.cpp:1121-1123), because the joiner merge lies on the lane item.

Stack items (Structure, jt.cpp:~880-905): per row `[north-side item?] Lane [south-side item?]`, kinds Lane, Prio, Copy, PrioAndCopy, Approach.
- `joiners` = count of non-row edges meeting the row's non-split nodes; more than 1 fails.
- `crossed` (jt.cpp:~893) = ANY row edge at this level with crossings: `is_row_edge(e) && end_level(from)==level && !edge_crossings[e].empty()`. So in and out edge crossings ALREADY make `need_copy`. Nothing in the stack counts per edge.
- Joiner: `need_approach` on the joiner's side, `need_prio` on the other. `need_copy[joiners==1 ? (approach_south?0:1) : 0]`; same side as prio gives PrioAndCopy.
- One Copy item per row, however many edges are crossed.
- `FindRowItems` (jt.cpp:178-190): `north` = the item directly before Lane unless it is an Approach. HAZARD: with a joiner whose approach is north, the Copy/PrioAndCopy lands SOUTH of Lane, `north == NONE`, and today's leg code fails "a row without its side tracks" only on the in edge fan (`row_items[r].north`); the leg itself uses `lane` only. A second doubled edge needs the Copy item on whichever side it is, so `RowItems` needs a `copy` field (the Copy/PrioAndCopy item, either side), not `north`.

Gate that refuses (jt.cpp:1080-1087): `if (e != leg_of[r]) fail("a crossed lane in or out edge is not ruled yet")`, then `row_doubled[r] = true`. `row_doubled` is a per-row bool that drives the in edge's Fan item, so it conflates "row has a Copy item" with "leg is doubled".

## 2. Downstream consumers and leg-only assumptions

Placement (jp.cpp):
- `IsInEdge`/`IsOutEdge` (jt.cpp:215-225) need first (in) or last (out) token to be a Fan. A doubled in edge `[Fan, Over..., merge]` still passes; a doubled out edge must end with its Fan (`[... , Fan]`).
- `fan_item` (jp.cpp:529) and `node_item` (jp.cpp:535-547): `node_item` returns the first/last token with `index != NO_ROW`, and `Over.index` is a crossed edge. For an in edge ending in an Over, or an out edge starting with an Over, it returns an edge number as a stack item. Must skip Over/Tunnel/Under/Above (take only Fan/SBend/Corner). Used at jp.cpp:608, 1315-1319, 1374.
- Chain builder (jp.cpp:553-632): handles Fan, SBend, Over(doubled) per chain; already generic for `chain_copy[e]` on any row edge; it requires `LaneMerge` in `programs.edges[e]` and `LaneSplit` in `programs.copies[e]` (jp.cpp:567-568), so each doubled edge needs exactly that role pair. Over's row item: `row_item` else `node_item(in_edge[r])` (jp.cpp:608): leg-only (an in edge starting with Over has no earlier fan; a doubled out edge's first Over sits on the joiner-merge row).
- Prio element is appended to the LEG's copy-0 chain only (jp.cpp:616-629) and every prio use reads `chain[leg_edge[r]].back()` (jp.cpp:1098, 1899, 2156). Not tied to the copy, but the leg always exists, so unaffected.
- Row relations for a doubled edge (jp.cpp:1362-1440, "A doubled leg"): hard-wired to the leg: `bend1.first` on `exit_item = node_item(in_edge[r])` (the exit split row = where the leg starts), `nex = elems.front().first` (a node tile at the start), `along(... FloorKind::Node, LaneSplit)`, last head to merge to `FloorKind::Prio` of the prio element (`prio_el`), and `eq(1, bend0.first/last ...)`. Assumes: a node tile at BOTH ends of the edge (a port tile or a Fan at one end of an in/out edge breaks `nex`), exactly one lane split first then heads then merge, and the prio element after the merge. These floors need an in-edge variant (Fan then LaneSplit; merge then the node) and an out-edge variant (node then LaneSplit; merge then Fan).
- `collect_bridges` (jp.cpp:1203-1232): doubled bridge lines/heads registered only `for r: IsDoubledEdge(leg_edge[r])`; in/out edges' HeadsPair elements are never registered (single-bridge branch above it skips all row edges at jp.cpp:1208). Must loop in, leg, out.
- `row_pieces` (jp.cpp:893-1000) already walks in, leg, out, with shared run before split and after merge ("copy -1") and `n_chains = IsDoubledEdge(e) ? 2 : 1`; check its split/merge lookups per edge (`find_role`) but the loop is per edge, so it looks general.
- `sym_routes` (jp.cpp:726-770): `doubled_edge(e)` is generic per edge; a route has two copies if any edge is doubled, copy 1 follows `copies[e]` of every doubled edge. Fine for several doubled edges per path (copy 0 stays copy 0 everywhere).
- Lane-change leaning pass (jp.cpp:632-660): iterates `IsDoubledEdge(e) ? 2 : 1` copies per edge; `if (!programs.copies[e].empty()) continue;` at jp.cpp:654 is leg-style.
- Result spans (jp.cpp:2218-2240): the doubled branch treats every token as SBend or Over, so a Fan in a doubled in/out edge falls into the Over branch (`bridge_over(e, step.index)` fails "a crossing edge misses its bridge"). Needs a Fan case (as the undoubled path below it).
- Row prio-element footprint uses `row_items[r].lane` and prio item order only.

Render (junction_render.cpp): `doubled_leg(e)` = any doubled bridge on e (name misleading, generic per edge; lines 46, 86, 99, 168, 203). `placement.copies[e]` must hold one span per `copies[e]` token (line 90), so a Fan in a copy needs a span (see result spans above). Heads come from `placement.bridges[b][c]`. Otherwise generic.

Signals (signals/junction_signals.cpp:47-80): per doubled edge, ONE LaneSplit token in `copies[e]` and ONE LaneMerge token in `edges[e]` (first by role). Generic per edge, works for several doubled edges per path; requires the role pair on every doubled edge and a LaneSplit/LaneMerge tile of that edge to be on its copy route.

Levels (placement/junction_levels.cpp), pipeline, tool: no use of `programs.copies` (grep). They work on rendered routes and the layout.

Legacy (legacy/junction_nodes.cpp:735-745) reads copies but is superseded.

Tests helpers: topology test files read `programs.copies` per edge generally (see section 4).

## 3. Minimal change set

Common: replace the single `row_doubled[r]` with two facts: `leg_doubled[r]` (leg crossed; keeps today's meaning for the in edge fan item `north`) and a per-edge "doubled" predicate `doubled_bridge(e)`. `RowItems` gains `copy` (the Copy/PrioAndCopy item of the row, either side of Lane, or NONE), set in `FindRowItems`; use it instead of `north` for the second line of any doubled bridge. Remove the `e != leg_of[r]` gate.

Which copy keeps which row (current convention): copy 0 = the upstream row through the bridge, copy 1 = bends onto the other line before it; LaneSplit lives in `copies[e]`, LaneMerge in `edges[e]` (placement and signals find them there). Keep this role location for in/out edges so no consumer changes: each doubled edge gets exactly one LaneSplit (copy 1) and one LaneMerge (copy 0).

(a) Doubled out edge (joiner merge on `lane` -> port): both copies start on `lane` (the merge node's row) and end on `lane` (Fan to the port), so only one of them needs to leave the lane for the Copy item. Shape: one copy `[SBend(copy item, LaneSplit), Over..., SBend(lane, LaneMerge), Fan(lane)]`, the other `[Over..., Fan(lane)]` (the head stagger D118/D145 then says which copy that is). This breaks two leg assumptions: the role pair is no longer split one per copy (jp.cpp:567-568 requires LaneMerge in `edges[e]` and LaneSplit in `copies[e]`; signals jp 47-65 look the same way; topology test 3225 searches `edges[e]` for LaneMerge), and `copies[e][0]` is no longer the split. Cheapest consistent choice: the bending copy is always `edges[e]` (copy 0) for in/out edges, and `has_role`/`with_role` look in both programs of the edge. A role-model change is Pete's call (brief rule: add no mechanism the brief does not name). Placement needs: `node_item` skipping Over; an out-edge variant of the doubled-edge relations (node tile -> floor(Node,LaneSplit) -> heads -> floor(Heads,LaneMerge) -> Fan, no prio element on this edge); a Fan case in the doubled branch of the result spans; `collect_bridges` looping over the out edge.
(b) Doubled in edge (port -> exit split on `exit_item`): mirror: `[Fan(lane), SBend split, Over..., SBend merge]` per copy; but the Fan item and exit_item: if the leg is also doubled the exit split sits on `north`/Copy (existing), so the in edge's final merge S-bend must land on `north`; if the leg is not doubled the node is on `lane`. Program: decide Fan item = `lane` always for a doubled in edge; final SBend(node row item). `RowItems.copy` replaces `north` for the fan. Placement: `exit_item` (jp.cpp:1315, 1374) must be `node_item(in_edge)` = the last SBend's item, not a Fan item; floors Fan -> LaneSplit; heads -> LaneMerge -> Node. Same `collect_bridges`, spans and `node_item` fixes.
(c) Two doubled edges on one row (leg + out, or in + leg): one Copy item (`RowItems.copy`) shared; the two bridges lie at different x so the same y row is reused, nothing to add to the stack. Programs: each edge builds its own pair (no shared state; today's `row_doubled` fan rule only matters for the in edge when the leg is doubled). Placement: relations are per edge and use `copy`'s item_pt; the row-wide checks (`Prio` footprint, lane-change leaning at jp.cpp:632-660, `row_pieces`) must tolerate two doubled edges per row (they iterate edges, so mostly yes). The curve-gap and tile-sharing families (D217) are rule families over all routes, so the two bridges are covered. D229 note: a band edge carries one bridge (F3, jp.cpp:271); row edges may carry the series bridges already (jp.cpp:1414 loop).
Signals/render: no change expected, they are per edge; verify the Shift test and `JunctionRouteEvents` with two doubled edges on one path.

## 4. Tests pinning leg-only behaviour (src/tests/junction/topology/junction_topology.cpp)
- No test asserts the string "a crossed lane in or out edge is not ruled yet".
- 3218-3232 (Shift insertion): for every edge with copies, finds `LaneMerge` in `programs.edges[e]` and inserts before it; every doubled edge must have LaneMerge in copy 0.
- 3255-3275 (R6): looks for "an undoubled row leg"; unaffected unless a topology gains doubled in/out.
- 3278-3300 (R3, "a fan edge may hold more than its fan"): identifies the doubled row's in edge by `Fan.index + 1 == lane_item` (the in edge fans to `north`, one before lane); pins the in-edge-fans-to-north rule of a doubled leg (whose last token S-bends onto the lane).
- 3381-3397: `CheckDoubledLegs(...) == 6` for LL_RR sideline at the test spec; fails if the sideline gains doubled in/out bridges.
- 700-726, 920 (hand-built `copies[]` expectations of the T and LL_RR sideline) and 1564-1619, 1775-1804, 2273-2276, 3353 read `placement.copies[e][0].first` as the lane split: assume copies[e][0] is the LaneSplit SBend (true for a leg; an in or out edge variant whose copy 1 starts with a Fan or Over breaks them).
- 4518 (OC1) only exercises order, not programs. LL_RR x LL_RR programs currently fail at the gate.
