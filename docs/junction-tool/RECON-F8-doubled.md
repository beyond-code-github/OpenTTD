# F8 recon: doubled bridges off the leg (commit 29c9be26bf, read-only)

jp = src/junction/placement/junction_placement.cpp. jr = junction_render.cpp. sg = src/junction/signals/junction_signals.cpp.
Terms: is_row_edge(e) (jp:115) is TRUE for in, leg and out edges alike (level == level, odd). in_edge/leg_edge/out_edge[r] set at jp:146-161
(exactly one each per row, else "a lane other than in, one leg, out is not ruled yet"). IsDoubledEdge(e) = !programs.copies[e].empty()
(topology.cpp:229), doubled_edge(e) (jp:836) = any bridges_of[e] is doubled. Both are edge-general by definition.
Leg-shaped means: only correct when the doubled edge is leg_edge[r] (node-to-node, copy chain starts and ends at a node, has LaneSplit
and LaneMerge in the copy 1 / copy 0 programs).
Program items for an in edge (from F8 evidence): copies[e5] = SBend(LaneSplit) from A = Lane item to B = Copy item (D241). Out edge = mirror
(fan leaves from the copy item; copy 1 S-bends lane -> copy item BEFORE the bridge, copy 0 after it). I did not read the programs builder
to confirm which roles the out edge's edges[e]/copies[e] carry (open question 1).

## 1. Chain builder (elements per edge)
- jp:656-740 row-edge chain builder, loop `c < (doubled?2:1)`, program = edges[e] / copies[e]. Role-general: builds Tile, Fan, SBend,
  HeadsPair per Over token via bridges_of[e] lookup by crossed[0]==step.index (jp:705). OK for any row edge, EXCEPT:
  - jp:670-676 LEG-SHAPED: refuses a doubled edge unless programs.edges[e] has a LaneMerge SBend AND programs.copies[e] has a LaneSplit
    SBend. An in edge has only LaneSplit in copies (A->B) and an out edge only (presumably) LaneMerge: both refused today
    ("a doubled leg without its lane split and merge"). Needs: require roles per edge kind (in: copy 1 has the S-bend A->B, no merge;
    out: mirror) or drop the refusal and key on the S-bend's own role.
  - jp:715 LEG-SHAPED (soft): HeadsPair el.index with no fan/S-bend before it = programs.nodes[in_edge[r].to].item (B, the exit item). Right
    for a leg (starts at the node). For an out edge whose Over comes before any S-bend/fan the start is the joiner node's item; for an in
    edge the row before the first token is the fan's item: the chain tokens (fan first) set row_item, so OK once the fan precedes. Check
    that an out edge's copy 0 (bridge after the S-bend) and copy 1 (S-bend then bridge) get row_item from their own S-bend: yes via row_item.
  - jp:723-737 prio element appended only when c==0 && leg_edge[r]==e: correct (leg only), stays.
- jp:262-285 (undoubled-edge chain builder, `is_row_edge(e)` continue above it): Over/Tunnel -> Heads; refuses doubled (jp:275), refuses
  several bridges. Not reached for row edges. OK to leave (crossing non-row edges only), but its `bridges_of[e][0]` is the F8 "first bridge" bug
  for non-row edges (jp:276, 445, 1392): LEG-SHAPED w.r.t. one bridge per edge. Needs: Heads el.index/cross -> bridge of its own Over token.

## 2. Bend sizes / row changes / occupant inputs
- jp:604-608 refresh loops chain[e] and chain_copy[e] for every e: OK.
- jp:744-757 row_changes: kinds 1-4. Fan kind 1 OK (any edge). LaneMerge kind 3 uses elems[i-1].last: OK. LANE SPLIT kind 2 (jp:754-755)
  LEG-SHAPED: leans to chain[e][1].first (copy 0's first element after start) with the travel. For an in edge's copy 1 the A->B S-bend
  follows the fan, copy 0's [1] is the fan, not a heads/bend after a node. Needs: lean toward the element after the node's own tile in
  the edge's own chain, per role (A->B at an in edge: toward the node item B side).
- jp:790-800 / 1264-1275 / 2417-2423: chain[leg_edge[r]].back() Prio element and row_items[r].lane. OK (prio is the leg's, D75/D87),
  unchanged by F8.
- jp:836-838, 858-875 sym_routes: `doubled_edge(e)` picks copies[e] tokens for c==1, bends per token; role-general. OK.
  (jp:864 uses programs.copies[e] only when doubled_edge, else edges[e]: an in/out edge with copies but whose bridge is single? not
  possible: copies exist only for doubled.)
- jp:1018-1090 row_pieces: walk(in_edge), walk(leg_edge), walk(out_edge) already in the list; per edge n_chains = IsDoubledEdge(e)?2:1
  (jp:1036). BUT the copy-sharing rule jp:1044-1045,1058-1066 LEG-SHAPED: `split = find_role(1,LaneSplit)`, `merge = find_role(0,LaneMerge)`;
  a == split->first for c==0, copy -1 for c==1 at i==0 (shared run from the start tile to the split) and merge shares at the end
  (jp:1065-1070). For an in edge: the start tile is a PORT (el.slot>=0) not a node; copy 1's S-bend A->B starts after the fan; the shared
  part is the fan up to the split (copy -1), the run after that differs. For an out edge the shared part is after the merge/up to the port.
  Needs: shared run delimited by A (the split) for the in edge and by the merge for the out edge; both chains share fan (in) / tail (out).
- jp:1173, 1816: loops over chain[e], chain_copy[e] for ids: OK.
- jp:1706-1740 (floors per element): `in_out` = IsInEdge||IsOutEdge; floor_kind for in/out returns Port/Node/PrioNode by slot/to.node and
  never reads copies. LEG-SHAPED in the sense that in/out run `elems = chain[e]` only: the doubled in/out gets copy 1's floors
  nowhere (this loop reads `elems` = chain[e]; the doubled branch at jp:1544 `continue`s before it, with is_row_edge(e)&&doubled_edge(e)).

## 3. The doubled relations block (the F8 core)
- jp:1544-1625 `if (is_row_edge(e) && doubled_edge(e))`: LEG-SHAPED throughout:
  - jp:1547 g = count of HeadsPair in elems; nex = elems.front().first (jp:1549): the chain's first tile is the NODE (leg). For in edge it
    is a port tile, for out edge a node/prio tile. Needs: the tile where the lane split sits, per role.
  - jp:1550-1551 bend0 = lane_bend(LaneMerge) in chain[e], bend1 = lane_bend(LaneSplit) in chain_copy[e]. lane_bend (jp:231-237) falls back
    to elems.front() when absent (silent). In edge: only bend1 (A->B); out edge: only a merge. Needs: optional ends, per role.
  - jp:1556-1557, 1565 exit_item = programs.nodes[in_edge[r].to].item; eq bend1.first = item_pt[exit_item]; bend0.first likewise. LEG-SHAPED:
    this is the leg's start item B. For an in edge copy 1's S-bend must start on A (the lane item, = fan item), end on B (Copy item
    = bend1.index, eq jp:1558 is own token item: ok). For the leg, bend1.first on exit_item hence "today it gets size 0 on B" (F8).
    Needs: first = item of the step's A, last = its own B; both from the S-bend step (step.index is the end item; start item = the previous
    row_item/fan item in the same chain), not from the in edge's node.
  - jp:1573 along(heading, bend1.first, nex, JunctionRunFloor(Node, LaneSplit)): Node->LaneSplit run floor from the chain's first tile;
    LEG-SHAPED (floor kinds: in edge's start is Port->Fan, out edge ends at PrioNode). Needs: in edge: Fan->LaneSplit floor, run from the fan;
    out edge: LaneMerge->PrioNode/Port tail.
  - jp:1574-1576: heads0(0).first after bend1.first (floor LaneSplit,Heads=2) and heads1(0) = bend1.last + 1 stagger: in-edge-shaped only if
    the bridge follows the S-bend; for an in edge the bridge may sit BEFORE the S-bend on copy 0 only (copy 0 has no bend at an in edge).
    Needs: sequence of tokens in each chain (not fixed split -> heads -> merge): derive the along() chain from the token order per copy.
  - jp:1578-1585: stagger heads0/heads1 per bridge, bend0.first after heads0(g-1).last, bend0.last after heads1(g-1).last: assumes both
    bends exist and sit on the ends of the bridge span; LEG-SHAPED.
  - jp:1588-1589: prio_el at the end: only the leg has it, correct.
  - jp:1592-1620 hug_heads: bridges_of[e][b] (b = k-th) with heads0(b)/heads1(b) via nth_heads (jp:225). Uses the k-th bridge by travel order;
    role-general if HeadsPair elements are built per Over token (they are, jp:705). BUT is `bridges_of[e][b]` index == travel order? Not
    checked (open question 3). Crossers rows: `edge_tiles(x)` rows y == vy[near_far.first]: role-general.
- jp:1497-1501 (ties of in.to/out.from nodes to their items): OK, in/out by name; exit_item here is the NODE's own item. OK.

## 4. Clearance / collect_bridges
- jp:1385-1415 collect_bridges: (a) non-row edges: `bridges_of[e][0]` per Heads element, Y-axis bridge lines (jp:1392) LEG-SHAPED w.r.t. several
  bridges per edge (not rows); (b) jp:1400-1411 doubled bridges only for leg_edge[r] (`if (!IsDoubledEdge(leg_edge[r])) continue`) LEG-SHAPED:
  skips in_edge[r], out_edge[r]. Needs: loop rows x {in_edge, leg_edge, out_edge}; line for copy c's heads = item_pt[h->index]
  (h->index = row item at el.index: B or A for an in edge, per copy 0/1; copy 1 on A... mirror for out edge). nth_heads(chain, k) pairs
  chain/chain_copy by k, so per-edge OK.
- Consumers: jp:1419 add_clearance_x, jp:2243 (collect_bridges again in the heights/clearance path). Both inherit the omission.

## 5. Result emission
- jp:2430-2449 heads_of(b, copy): doubled branch finds k via bridges_of[edge] and nth_heads(copy chain, k): role-general (the chain is the
  bridge's own edge). OK. Undoubled branch returns the first Heads of chain[edge] (F8: not per-token; jp:2446-2447 LEG-SHAPED).
- jp:2478-2501 result.edges/copies for a doubled edge: `is_row_edge(e) && doubled_edge(e)`; s from end_level(from) row; each copy's S-bend span from
  its chain's own SBend by tok; bridge_over(e, step.index) by crossed edge, heads_of(over, copy); tiles_under(step.index, heads, vy[heads.near]).
  OK role-general (no exit_item, no first tile). Note `else` branch handles any non-SBend token as Over: a Fan token on an in edge's copy 0
  (programs.edges[e] of an in edge starts with a Fan step, jp:699) would hit the Over branch and call bridge_over(e, step.index) = fan item
  index as a crossed edge: LEG-SHAPED: needs Fan and Corner tokens emitted like jp:2503-2509 (kind Fan: el.tok == t).
- jp:2519-2520 `heads_of(bridges_of[e][0], 0)`: Over token of an undoubled edge (single bridge): OK by guard (no multi).
- jp:2531-2537 Under/Above: bridge_over(x, e) with heads_of(bx, step.copy): bx is edge x's bridge over e; for a doubled x uses
  vy[heads.near] (row) lookup; role-general.
- jp:2546-2570 bridges output: copy loop and span max_bridge_middle: role-general; DOES NOT assume the leg except via heads_of.

## 6. jr (render) and sg (signals)
- jr:46-47 doubled_leg(e) = any bridge with edge==e && doubled: edge-general despite the name. jr:62-63, 86-88, 99-111, 168, 201-203 pick
  copies[e]/placement.copies[e] for copy 1 and placement.bridges[b][doubled_leg(e)?c:0]. OK for in/out IF placement.copies[e] holds a span
  per copies[e] token (jp:2478-2501 does). Route copy c = the lane/copy of the doubled edge, but a path through an in edge's doubled bridge
  only has copy-1 route when has_doubled(edges) -> same rule: OK.
- sg:47-80 switch events for each edge with copies[e] non-empty: LEG-SHAPED: needs BOTH a LaneSplit SBend in copies[e] (token search
  with_role, else program.size()) and a LaneMerge in edges[e]; the missing token gives size() so no tag matches and no event is emitted
  (silent miss, no refusal). An in edge has only the split (A->B, split at the S-bend's first tile), an out edge only the merge.
  Needs: emit the split event alone for the in edge and the merge alone for the out edge (and keep both for the leg). The sibling
  route search (jp: sg:50-53) is role-independent. Signals/placement may also place signals on the node pieces: not read further.

## 7. What an in edge / out edge doubled bridge needs, per LEG-SHAPED site (A = fan item/lane item, B = node's item for in; mirror for out)
- In edge (fan ends on A or goes A->B on copy 1; node on B): copy 1 chain = Tile(port), Fan, SBend(split A->B), HeadsPair*, Tile(node);
  copy 0 chain = Tile(port), Fan(A), HeadsPair*, Tile(node)?? (copy 0 runs on A, node's item is B, so the in edge's copy 0 must reach the
  node on B by the program's own S-bend: open question 2).
  jp:670 refusal, jp:1556-1557 exit_item -> A (S-bend first item = previous row item / fan item), jp:1549 nex -> chain's own first tile after
  the fan, jp:1573/1583-1585 floors from token order (Fan->LaneSplit; no LaneMerge), jp:754-755 lean, jp:1040-1070 share the fan run (copy -1)
  up to A's split only, jp:1400 collect_bridges rows loop (in_edge), jp:2480-2501 emit Fan tokens, sg:47-80 split-only.
- Out edge (mirror, node = joiner merge on B, fan on A at the port): copy 1 S-bends lane -> copy item BEFORE the bridge, copy 0 after it,
  fan leaves from the copy item (jp:1497-1501 eq out.from to item_pt[nodes[out.from].item] = node's own item, ok). Same sites with the
  mirror: exit_item -> the out node's item; no LaneSplit refusal; the shared run is the tail after the merge toward the port.

## 8. Tests that touch doubled legs (grep 'doubled'; I ran nothing)
- src/tests/junction/topology/junction_topology.cpp: :734-754 predicates vs old derivation incl. JunctionIsDoubledEdge; :795-801 doubled bridge
  passed under twice; :992-1000 "a doubled leg's Over span for a crossed edge lies strictly between its own bridge's copy-0 heads"; :1020
  prio element n+2 floor on both doubled sides; :1186-1242 (bridges' heads/cut, copy-1 heads keys); :1574 "a doubled leg's lane split sits next
  to its bridge"; :1640; :1797-1798 route tag.copy == 1 only on doubled edge; :3238-3284 R6 lane split/merge found by role; :3310-3324 R3 T's
  doubled row's in edge edited to [Fan, S-bend ...]; :3352-3368 R5 "every doubled leg of a seed at a spec checked against its chains' rules".
- src/tests/junction/placement/junction_joint.cpp: :55-63 T and LL_RR x L_R (doubled leg) reproduces prototype; :121, :223 LL_RR x L_R; :255-261,
  :303-309 default cut with doubled bridges in series; :276-289 LL_RR x LL_RR prints "places/fails, relations N" only (no assertion: the F8 target;
  report-only). src/tests/junction/placement has no other 'doubled' hits (junction_milp.cpp, junction_positions.cpp: none).
- Other: src/tests/junction/signals/junction_signal_pass.cpp:91 (doubled reference), game/junction_map.cpp (:92, :481, :614, :884), terrain/
  junction_terraform.cpp:496 (single bridge), legacy/junction_generator.cpp and junction_rules.cpp (old layout checkers).
- No test covers a doubled bridge on an in or out edge (e5): topology tests iterate legs only (`seen.insert(e)` pattern at topology:1585,1640).

## 9. Open questions (not resolved by reading)
1. What roles do an out edge's edges[e] and copies[e] carry in the programs builder? (jp:670 and sg:47-80 depend on it.)
2. For an in edge's doubled bridge, which chain carries the node-side S-bend (does copy 0 end on B by its own S-bend)? jp:1550 vs 1551 need it.
3. Is bridges_of[e] sorted by travel order, so bridges_of[e][b] matches nth_heads(chain, b)? (jp:1593, jp:2443 assume it.)
4. Do Under/Above tokens on a crosser of an in/out edge's doubled bridge carry step.copy right (jp:2531)?
