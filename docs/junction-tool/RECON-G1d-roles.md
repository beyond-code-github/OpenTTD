# RECON G1d: where the checker and signal pass assume the T junction

Read-only recon, branch feature/junction-tool. Paths are under src/. "Is-ML" is the repeated expression
`(from==EIn && to==EOut) || (from==WIn && to==WOut)`; it is the same fact everywhere (see the proposal).
T mapping (JunctionTTopology): EIn=g0 (W side in), WOut=g1, NIn=g2 (S side in), SOut=g3, WIn=g4 (E side in), EOut=g5.
Movements {0,5},{0,3},{4,1},{4,3},{2,1},{2,5} = JunctionTMovements() order. In the canonical frame the box edges are
NE=x0 ("W" side), SW=x max ("E" side), SE=y max ("S" side); there is no N (y=0) edge at all.

## 1. Sites (grep hits in the listed files; the other files are counts only)

| # | file:line | function | what the role is used for | generic replacement |
|---|---|---|---|---|
| 1 | junction_rules.h:27 | enum JunctionPortRole | six fixed names | drop; ports identified by slot index |
| 2 | junction_rules.h:30 | JunctionPort.role | port identity | slot index (+ group, lane) |
| 3 | junction_rules.h:37-38 | JunctionPath.from/to | path endpoints as roles | from/to slot index (JunctionLanePath-style) + movement index |
| 4 | junction_rules.h:47,53 | JunctionCheckContext.expected, MakeJunctionCheckContext | the movements that must exist | expected = lane paths (from slot, to slot, movement) |
| 5 | junction_rules.h:52 | FindJunctionPorts decl | T ports | ports from JunctionPortSlots + box geometry |
| 6 | junction_rules.cpp:31 | `using Movement` | (role,role) pair | (slot,slot) or lane-path index |
| 7 | junction_rules.cpp:104-114 | RoleName | message text "EIn to SOut" | slot name (side + lane) |
| 8 | junction_rules.cpp:117 | IsInRole | trace starts at In ports, stops at Out ports | group.in of the slot's group |
| 9 | junction_rules.cpp:154-179 | FindJunctionPorts | see section 2 | see section 2 |
| 10 | junction_rules.cpp:200,210-212 | TraceFrom | exit must be an Out port at same tile+trackdir; records `{from, port.role}` | port lookup by (tile, trackdir) gives slot |
| 11 | junction_rules.cpp:271-277 | MakeJunctionCheckContext | traces from every In port | same, In = group.in |
| 12 | junction_rules.cpp:338-343 | PortTile | violation location "port tile of role" | tile of slot |
| 13 | junction_rules.cpp:351-359,401-402 | CheckConnectivity | missing / ambiguous / unexpected movement keyed by (from,to) | key by lane path (slot pair); count>1 = doubled |
| 14 | junction_rules.cpp:405-412 | CheckConnectivity | "missing port R" for every role in expected | every slot of every expected path |
| 15 | junction_rules.cpp:435 | CheckOrder | is-ML: no two adjacent splits on a mainline path | path.movement is a through movement (both groups main, same line) |
| 16 | junction_rules.cpp:515 | CheckBridges | is-ML: bridge on a mainline path needs a same-movement path avoiding it | same; "same movement" = same movement index, not same from/to |
| 17 | junction_rules.cpp:695,707 | JoinerSignals | is-ML; `joiner.from==ml.from&&to==ml.to` = lane merge, no prio | through movement; joiner.movement == ml.movement |
| 18 | junction_rules.cpp:848 | CheckWaitingRoom | rule 5: paths (EIn|WIn)->SOut need split + merge + waiting room (D83) | movement whose in group is main and out group is not main (an exit) |
| 19 | junction_rules.cpp:883 | CheckWaitingRoom | rule 6: every non-ML path gets bays before its queue merges | not a through movement |
| 20 | junction_rules.cpp:1094,1117,1307 | CheckPrio | is-ML; lane merge = same from/to; 4b lane list = same from/to | through; same movement index |
| 21 | junction_rules.cpp:1544,1565,1588 | CheckPrioReach | same three uses as CheckPrio | same |
| 22 | junction_rules.cpp:1685-1692 | JunctionTMovements | the six T movements | topology.movements expanded to lane paths |
| 23 | junction_rules.cpp:1696,1720 | CheckJunctionRules, JunctionRulesClean | build ctx with T ports and movements | take topology (or ports+expected) as parameter |
| 24 | junction_rules_internal.h:30 | decl JunctionTMovements | as 22 | as 22 |
| 25 | junction_signal_pass.cpp:33-36 | IsMainline | is-ML; PathOrder walks mainlines first (l.42-45); five passes skip non-ML (464,510,542,582) | through movement |
| 26 | junction_signal_pass.cpp:274,287,332,522,548 | ReservePrioJoins/PlaceRequired/FillSignals... | `from/to` equality = lane merge / "same lane group"; not a role constant | same movement index |
| 27 | junction_signal_pass.cpp:617,620 | RemoveRedundantJunctionSignals | ports computed once, ctx rebuilt per round | pass topology/ports in |
| 28 | junction_signal_pass.cpp:642-643 | PlaceJunctionSignals | `trace` lambda = ctx from FindJunctionPorts+JunctionTMovements | same |
| 29 | junction_nodes.h:71-72 | JunctionNodePath.from/to | copy of path roles | slot indices |
| 30 | junction_nodes.cpp:111-114,360,538 | IsMainLinePath | ML-ness: prio merge detection (360, also `is_prio = joiner.from!=path.from||to!=path.to`), rule 6 skip (538) | through movement; same movement index |
| 31 | junction_nodes.cpp:528 | node rule 5 | (EIn|WIn)->SOut waiting room, mirrors CheckWaitingRoom | exit movement (as site 18) |
| 32 | junction_nodes.cpp:658,1516 | TraceJunctionNodeInput, JunctionNodeHeightsOk | T ports/movements for ctx | as 28 |
| 33 | junction_nodes.cpp:688,1668-1670 | BuildJunctionNodeTemplate, NodeAnchorShift | rule 5 anchor: path EIn->SOut; its last piece's X class and first piece's Y class pin the node's x/y. Also :362 `joiner_row = ...tile.y` (T "joiner comes from south") INFERRED | "the branch exit path of the first main in group", or a per-topology anchor movement; INFERRED that only the T has an anchor |
| 34 | junction_throughput.h:32-33,41,70 | JunctionMovementTiming, JunctionTrafficMix, doc | from/to roles, movement-keyed weights, "JunctionTMovements() order" | movement/lane-path index |
| 35 | junction_throughput.cpp:44-47 | IsMlMovement | weight 3 for ML, 1 for branch; JunctionPrioTimings skips non-ML (l.529) | through movement |
| 36 | junction_throughput.cpp:53,82,74 | MakeDefaultJunctionMix, EvaluateJunctionThroughput | iterates JunctionTMovements(); finds path per movement by from/to; nullopt if missing | iterate expected lane paths |
| 37 | junction_throughput.cpp:514 | doc of JunctionPrioTimings | "in JunctionTMovements() order" (ctx built with empty ports/expected, so no coupling) | as 36 |

Sites per file (listed in the table / raw grep hits for the five names plus `JunctionPortRole`):
junction_rules.cpp 17 sites / 31 hits; junction_rules.h 5 / 7; junction_rules_internal.h 1 / 1;
junction_signal_pass.cpp 4 / 5 (+5 from/to equality uses with no role constant); junction_nodes.h 1 / 2;
junction_nodes.cpp 4 / 11 (+ from/to equalities at 360 and 1668-1670 counted there); junction_throughput.h 1 / 4; junction_throughput.cpp 4 / 6.
Uses by file, not read: junction_organic.cpp 10, junction_chain.cpp 2, junction_chain.h 2, junction_generator.cpp 2,
junction_topology.h 1; tests/: junction_rules 86, junction_generator 27, junction_nodes 21, junction_design 6, junction_organic 6,
junction_throughput 5, junction_chain 4, junction_signal_pass 4, junction_topology 2, junction_router 2.

## 2. FindJunctionPorts (junction_rules.cpp:154-179)

Builds ports per box edge via EdgeTiles (l.128-150): for each layout tile on the edge (x==0, x==size.x-1, y==size.y-1) it
collects the tracks that TrackTouches that edge, keyed by the coordinate along the edge. add_edge then:
- requires exactly `tiles.size() == 2` (l.160): one In tile and one Out tile per edge. 2 lanes per main side gives 4 tiles per
  W/E edge, so the T-only count fails and the edge yields no ports (silently: no ports -> "missing port" violations).
- requires every port tile to hold exactly `TrackBits{axis}` (X for W/E edges, Y for the S edge), l.162. Fine for straight ports,
  but any port tile with a corner or a doubled-edge track (bridge ramp heads are not on the boundary, footprint rule) is rejected.
- assigns roles by order along the edge: smaller `along` is the first role (the In on the NE edge, the Out on the SW edge). The
  roles are hard-coded per edge (l.175-177) with fixed trackdirs (X_SW/X_NE/Y_NW/Y_SE). For N side (y=0) there is no edge case:
  EdgeTiles has no N, TrackTouches/AnyTrackTouches have no NW edge, and CheckFootprint's on_boundary (l.1661: x==0, x==size.x-1,
  y==size.y-1) also omits y==0. LLRR sideline needs both N-less and 2-lane sides, so only the lane and the edge-count assumptions bite.
- the topology order IS the along-edge order: JunctionPortSlots lists W north to south, S west to east, E south to north, which is
  exactly FindJunctionPorts's smaller-`along`-first rule per edge, so slot i of an edge maps to the i-th tile along it (E and N
  reversed in lane order inside a group). A topology-driven version needs the edge tile coordinates for 10 slots: 4 on W (2 in, 2 out),
  2 on S, 4 on E; each group's lanes are adjacent (D142). INFERRED: the tile rows of each group are read from the layout's boundary tiles
  in slot order, not from a constant; whether in and out groups of one side can be adjacent rows is not checked here.

## 3. JunctionCheckContext and `expected`

Context = {layout, spec, ports, expected, paths, trace_problems}. MakeJunctionCheckContext traces (TraceFrom, depth-first,
branching at every continuation, handling bridges entered along axis) from each In port and records a JunctionPath{from role,
to role, pieces} when the walk leaves the box at an Out port's tile and trackdir (l.208-213). Anything else adds a trace problem
(dead end, cycle, unexpected exit, path too long). Paths are not paired with movements at trace time; `expected` is only
consulted afterwards, by (from,to) equality: CheckConnectivity counts paths per expected pair (0 = missing, >1 = doubled lanes
checked for common prefix/suffix and equal length, else ambiguous), flags paths whose pair is not in expected, and flags expected
roles with no port. Several paths with the same (from,to) is today a valid "doubled" movement (two routes), so with slot-keyed
paths the checker must separate "two routes for one lane path" from "two lane paths of one movement".
Downstream use of path.from/to: (a) is-ML tests (about 12 sites); (b) `joiner.from==ml.from && joiner.to==ml.to` = lane merge, no prio
(JoinerSignals, CheckPrio, CheckPrioReach, signal pass, nodes); (c) `lane.from==ml.from` = all routes of the same movement
(CheckPrio 4b, signal pass on_every_lane); (d) rule 5 anchor and waiting-room selection by role; (e) violation message names;
(f) throughput looks paths up per expected movement and keeps the movement order; (g) JunctionNodePath copies from/to.

## 4. Checker rules (CheckJunctionRules order)

| rule fn | reads roles? | how |
|---|---|---|
| CheckConnectivity (G1) | yes | expected pairs, unexpected pair, missing ports, PortTile, RoleName |
| CheckGeometry (G2) | no | level crossing from UsedTracks |
| CheckCurveSpeed (3) | no | path pieces only |
| CheckOrder (G4) | yes | is-ML limits the adjacent-split check |
| CheckBridges | yes | "ML bridge is not doubled": is-ML plus same from/to |
| CheckSignals (G6) | indirect | JoinerSignals (is-ML, same from/to); rest is geometry |
| CheckWaitingRoom | yes | rule 5 selects (EIn|WIn)->SOut; rule 6 skips ML |
| CheckDeadlock | no | blocks only |
| CheckPrio | yes | is-ML; lane-merge exclusion; 4b same-movement lanes |
| CheckPrioReach | yes | same three |
| CheckFootprint | no roles, but T geometry | on_boundary omits y==0 (no N side) |

## 5. Smallest generic interface (PROPOSAL, not decided)

Give each port slot `{slot index, group index, lane, tile, trackdir, in}` (tile/trackdir from a topology-aware FindJunctionPorts
that walks each box edge once per group), and each path `{from slot, to slot, movement index, pieces}` where `movement` indexes
`JunctionTopology::movements` (the same index ExpandJunctionLanes puts in JunctionLanePath). The context carries `const
JunctionTopology*` plus the expanded lane paths as `expected`. Then every site is answered without a role: is-ML becomes
`IsThrough(topology, path.movement)` (both groups main, same line; the T has exactly the two EIn->EOut and WIn->WOut movements); "exit"
(rules 5, node anchor candidates) becomes in group main && out group not main; "same movement / lane merge" becomes
`a.movement == b.movement` (not from/to, which now differ per lane); "port tile of role" becomes the tile of the slot; messages print
side+lane; throughput weights, timings and `JunctionTMovements() order` key off the lane-path index. The node anchor (EIn->SOut) stays
the one T-flavoured fact: propose a topology-side `anchor movement` (the first exit from the first main in group). To keep the old
call sites cheap, keep `JunctionTMovements()` and a role alias map for tests via the T topology (g0..g5) until they migrate.
