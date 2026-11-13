# RECON: the seam between topology and placement in junction_topology.cpp (RS1)

Facts only, no recommendation. Source: src/junction/topology/junction_topology.cpp (4522 lines) and .h (440), at HEAD of feature/junction-tool. Line numbers are of the .cpp unless marked .h.

## 1. Function map of junction_topology.cpp

| Lines | Size | Item | Stage | Callers (outside the file) |
|---|---|---|---|---|
| 37-174 | 138 | `JunctionPortSlots`, `ExpandJunctionLanes`, `JunctionChordCrossings`, `JunctionSlotRows` | topology | rules.cpp, legacy/nodes.cpp (slots, expand); tests only (chords, slot rows); `JunctionSlotRows` also read by placement |
| 175-248 | 74 | anon `RowItems`, `FindRowItems`, `FanTokenOf`; `JunctionIsIn/Out/Leg/DoubledEdge` | order/programs helpers | tests only; placement calls the four predicates (lines 1275-1278) and `FanTokenOf` (1664); `FindRowItems` also at 963, 1266 |
| 249-486 | 238 | `JunctionTreeBuilder`, `BuildJunctionTrackGraph` | graph | tests only |
| 487-816 | 330 | `OrderJunctionTrackGraph` | order | tool/junction_hover.cpp, tests |
| 817-924 | 108 | `StructureJunctionTrackGraph` | structure | tests only |
| 925-1146 | 222 | `ProgramJunctionTrackGraph` | programs | tests only |
| 1148-1219 | 72 | `JUNCTION_AXIS_TRACKDIR`, `JunctionBendMoves`, `FloorKind`, `JunctionCornerTrack` (decl), `JunctionPrioStepBits`, `JunctionRunFloor` | placement helpers | none; shared use below |
| 1220-3451 | 2232 | `PlaceTrackGraph` (static) | placement | the three `PlaceJunctionTrackGraph` overloads, `MakeJunctionPlacementShared` |
| 3452-3487 | 36 | `PlaceJunctionTrackGraph` x2 (seed, fixed choices) | placement | tool/hover, tests |
| 3462-3487 | 26 | `PlaceJunctionTrackGraph` (facts + terraform) | placement + render + signals + levels glue | tool/hover, tests |
| 3488-3592 | 105 | `MakeJunctionPlacementShared`, `JunctionPlacementNearReject` | placement (hover glue) | tool/hover, 1 test each |
| 3593-3811 | 219 | `RenderJunctionTrackPlacement`, `JunctionTrackTouches`, `JunctionCornerTrack`, `AddJunctionTracks` | render | tool/hover, tests; seed, cut-less paths |
| 3812-4153 | 342 | `JunctionRouteEvents` (3812-3863), `SignalJunctionTrackRoutes` (3864-4153) | signals | tool/hover, legacy/nodes.cpp, tests; `PlaceDragSignals` called from here |
| 4154-4221 | 68 | `JunctionTTopology`, `JunctionLLRRSidelineTopology`, `MakeJunctionHubTopology` | topology | rules, legacy (T); gui (hub); tests |
| 4222-4347 | 126 | `AppendBridgeCompositions/Counts`, `CutJunctionTrackBridges` | bridge cut (structure edit that calls placement) | tests only; called by `MakeJunctionGraphSeed` |
| 4348-4375 | 28 | `MakeJunctionGraphSeed` | pipeline / hover glue | tool/junction_gui.cpp, tests |
| 4376-4522 | 147 | `IsRunPiece`, `ApplyJunctionDetours` | routes edit (D213; uses `JunctionCurveGaps`) | legacy/nodes.cpp, 1 test |

Rough stage totals: topology/graph/order/structure/programs (37-1146, 4154-4221) about 1370 lines; placement (1148-1593 in the table sense: 1148-3592) about 2445; render 219; signals 342; cut + seed + detours 301.

Shared helpers across the placement/render/signals boundary: `JUNCTION_AXIS_TRACKDIR` (placement 1313, 1931, 1943, 2095, 2116, 2142, 2892; render 3629, 3690, 3717), `JunctionBendMoves` (placement 1313, 1931, 2095, 2142, 2916; render 3690, 3710), `JunctionCornerTrack` (placement via `JunctionPrioStepBits` 1172; render 3792; signals 3964), `JunctionPrioStepBits` (placement 2240, 3298 only), `JunctionTrackTouches` (render and signals only). `FloorKind` and `JunctionRunFloor` are placement-only. `JunctionRoutesHeightsOk` is named only in a comment (2084), not called.

### PlaceTrackGraph phases (1220-3451)

Inputs read throughout: topology.groups (slots), paths, graph.edges (44 uses), graph.nodes (10), graph.path_edges (2), order.node_level (11), order.rows (1), order.bands (1), structure.bridges (17), structure.stack (7), programs.edges (20), programs.copies (9); spec.tl, reference_speed, curve_speed, curve_limit, max_bridge_middle, max_tunnel_length, prio_tiles. Never read: graph.crossings, order.edge_crossings, structure.turns. Nothing graph-side is written; all outputs are local state and the result struct.

| Lines | Phase | Reads | Writes (local unless noted) |
|---|---|---|---|
| 1220-1254 | prologue: guards, slots, `row_of`, `end_level`, row-edge tests | topology, paths, graph, order, programs sizes | locals; `error` |
| 1255-1321 | headings, stack items, edge roles, row shape lookup, curve size, prio coverage, bend raster | structure.stack, programs, spec.tl/curve/prio_tiles; `DeriveCurveSpacing` | locals |
| 1322-1442 | point and chain data model: `Elem`, `nth_heads`, `lane_bend`, `Cons`, `FactCons` | none (types) | locals |
| 1443-1614 | relation machinery: `ge/eq`, hug, `BridgeLine`, `RowChange`, `solve` (1499-1593, builds `JunctionPositions`), `along/run` | cons | `vx`, `vy` via `solve` |
| 1615-1681 | `refresh` (bend sizes), notation rows, `fan_item`, `node_item`, `edge_port` | order.node_level, programs | locals |
| 1682-1840 | chains of the row edges (1682), row-change list (1765), symbolic tiles (1807) | graph.edges, programs, structure | `chain`, `row_changes` |
| 1841-2122 | symbolic routes and curves (`sym_routes` 1864), `row_pieces` (2027), node tracks (2087) | graph.path_edges, programs, chain | `sym` routes |
| 2123-2390 | flat solve, bend bits, occupancy family (2151-2321), head hugging, bridge lines, X clearance | chain, sym | `cons`, `hugs` |
| 2391-2692 | `build` part 1: stack rows, ports, row nodes, chains of the other edges (2483-2692) | order, structure, chain | `cons` (Y and X) |
| 2693-2765 | `build` part 2: Y clearance, curve-gap family (`JunctionCurveGaps`, 2720), floors | spec.curve_*, x_floors, y_floors | `cons` |
| 2766-3083 | `build` part 3: terrain facts in X (2766-3070), objective, row-change push (`JunctionPositions` Mark/Undo) | `JunctionFacts`, offset | `cons`, `vx`, `reasons` |
| 3084-3156 | port offset choice, `set_port_rows`, `plan_clearance` | vy | `port_offset`, `vy` |
| 3157-3260 | driver: fixed choices (one Y + one X) or the rounds (up to 5); also fills `JunctionPlacementShared` (make/record) | choices, shared | `choices`, `vx`, `vy`, `shared` |
| 3261-3305 | legality, fan rows, result base fields, prio elements (3288) | vx, vy | result: item_rows, ports, nodes, size, choices, columns, prio |
| 3306-3451 | bridge heads, edge spans (3332-3417), bridges (3418), curves (3438); max span checks | structure.bridges, spec.max_* | result: edges, copies, bridges, curves; `error` |

Hover and test callers of `PlaceJunctionTrackGraph` pass `JunctionPlacementChoices` and `JunctionPlacementShared` through; `MakeJunctionPlacementShared` runs `PlaceTrackGraph` in record mode (phase 3157-3260) and then probes with a dummy flat terrain (3488-3592).

## 2. Data flowing between structure/programs and placement

Writer of each graph-side type, and its readers:

| Type (.h line) | Fields | Written by | Read by placement | Other readers |
|---|---|---|---|---|
| `JunctionTrackGraph` (119) | nodes, edges, path_edges, crossings | `BuildJunctionTrackGraph` | nodes, edges, path_edges (not crossings) | order, structure, programs, render (edges, nodes, path_edges), signals (edges, path_edges), `JunctionRouteEvents` |
| `JunctionTrackOrder` (150) | node_level, rows, edge_crossings, bands | `OrderJunctionTrackGraph` | node_level, rows (count), bands (one use) | structure, programs, predicates; not render or signals |
| `JunctionTrackStructure` (192) | stack, bridges, turns | `StructureJunctionTrackGraph`; `CutJunctionTrackBridges` replaces it | stack, bridges (not turns) | programs, render (bridges, 5 uses), cut |
| `JunctionTrackPrograms` (231) | edges, copies | `ProgramJunctionTrackGraph` | edges, copies | render, signals, `JunctionRouteEvents`, predicates |
| `JunctionSpec` (legacy/junction_generator.h:39) | see list above | caller | 7 fields | render none; signals sd, signal_type, tl, prio_tiles; detours tl, curve_speed, reference_speed |

Placement output `JunctionTrackPlacement` (.h 274) and who writes each field:

| Field | Written by | Read by |
|---|---|---|
| item_rows, ports, nodes, edges, copies, bridges, size, choices, columns, curves, why | `PlaceTrackGraph` | render (bridges, copies, edges, nodes, ports, size), levels (nodes, ports only), hover, tests |
| prio | `PlaceTrackGraph` (3288) and again by the terraform overload through `SignalJunctionTrackRoutes(..., &placement.prio)` (3480 area) | signals (`JunctionPrioSpan` input) |
| terraform, height_cost, track_levels | terraform overload, from `SolveJunctionLevels` | ghost, build check |

Write-backs into graph-side types and cut-making facts:
1. `CutJunctionTrackBridges` (4262) runs `PlaceJunctionTrackGraph` for each candidate cut and returns a new `JunctionTrackStructure` chosen by placement width then height cost. So structure.bridges is rewritten by a placement outcome; the topology folder calls placement.
2. The terraform overload of `PlaceJunctionTrackGraph` (3462-3487) calls `RenderJunctionTrackPlacement`, `SignalJunctionTrackRoutes` and `SolveJunctionLevels`, then stores terraform, height_cost, track_levels and prio into the placement struct. `JunctionTrackPlacement` (a placement-stage type) thus carries level-stage output, and that is why levels.h includes topology.h.
3. `JunctionGraphSeed` (.h 398) bundles graph, structure (after cut), programs, placement and routes in one topology-folder type.
4. `JunctionPlacementChoices` is both input and output (fixed in, settled out in `result.choices`).
5. No placement code mutates graph, order or programs; all of those are passed `const`.

## 3. What levels.h needs from topology.h

levels.h includes `../terrain/junction_facts.h` and `../topology/junction_topology.h`. The signature uses `JunctionTerrain`, `JunctionFacts`, `JunctionTerraform`, `TileIndexDiffC`, `Money` (terrain folder and core), plus from topology.h only:
- `JunctionTrackPlacement`: levels.cpp reads only `placement.nodes` and `placement.ports` (lines 1519-1520), as fixed tiles.
- `std::vector<JunctionTrackRoute>`: reads `route.pieces[j]` (a `JunctionPiece`: tile, on_bridge, trackdir) and `route.tags[j].edge` and `.copy` (1524-1544).
- `JunctionLayout` (root `junction_layout.h`; reads `layout.tracks`, offset and tracks, line 1498; bridge heads from layout.bridges). topology.h only re-exports it.

levels.cpp includes no topology header of its own. `JunctionTrackRoute` and `JunctionTrackPieceTag` use only `JunctionPiece` (junction_route.h, root) and integers, so they could live in junction_route.h. `JunctionTrackPlacement` is the hard one only because it holds levels' own outputs; levels needs nothing from it but two vectors of tiles. So yes: either (a) levels.h takes `nodes` and `ports` as two `std::vector<TileIndexDiffC>` (or a small input struct) and includes only root and terrain headers once the route types move to junction_route.h, or (b) the placement struct moves to a placement/ header that levels.h includes inside its own folder.

## 4. Render and signal stages

- Render (3593-3811) reads placement output (nodes, ports, edges, copies, bridges, size) and graph/structure/programs/paths/topology.groups. It does not read order or spec. It writes `JunctionLayout` and `JunctionTrackRoute`s.
- Signals (3864-4153) read routes (pieces and tags), graph.edges, graph.path_edges, programs (via `JunctionRouteEvents`), topology.groups and movements, spec (sd, signal_type, tl, prio_tiles) and optionally `JunctionPrioSpan` (placement output type). They read no placement coordinates; they work on the rendered routes. They call `PlaceDragSignals` in topology/junction_signal_pass.cpp and write `layout.signals`.
- Both read graph types and placement or route output. `JunctionRouteEvents` (3812) is a graph/programs/routes query used by signals and legacy nodes.
- The CLAUDE.md files place signals in topology/ and render in the pipeline; RECON-folder-layout §3 proposes a separate `signals/` folder (signal_pass only) and `model/` for output types. Neither doc names a home for render.

## 5. Candidate cut lines

Common facts: the `PlaceJunctionTrackGraph` terraform overload, `CutJunctionTrackBridges` and `MakeJunctionGraphSeed` each call across the stages (placement, render, signals, levels), so each candidate must say where those three live. Includes the .cpp now takes: legacy/junction_generator.h (JunctionSpec), junction_rules_internal.h, line/junction_line.h, junction_route.h, chain.h, signal_pass.h, levels.h, positions.h.

### Cut A: placement core moves, glue stays
- Files: `placement/junction_horizontal.cpp` (name free) about 2445 lines (1148-3592 less the terraform overload, which stays); `topology/junction_topology.cpp` about 2080 (graph, order, structure, programs, render, signals, cut, seed, detours, terraform overload).
- To a shared header: `JunctionTrackSpan`, `JunctionPlacementReason`, `JunctionPlacementChoices`, `JunctionPrioSpan`, `JunctionTrackPlacement`, `JunctionPlacementShared` and the three overload declarations go to `placement/junction_placement.h`, which includes topology.h (graph types) for signatures. Axis/bend helpers go to a small header in root or placement/.
- Remaining cross-folder includes: placement -> topology (graph, order, structure, programs, predicates, `JunctionSlotRows`); topology (render, cut, seed, terraform overload) -> placement (placement.h, levels.h); levels.h -> placement.h. So the folder cycle stays, now topology <-> placement at the header of the placement struct, unless levels takes the two tile vectors.
- Hash risks: static helpers shared with render/signals (`JUNCTION_AXIS_TRACKDIR`, `JunctionBendMoves`, `JunctionCornerTrack`) must be moved as one definition, not copied; reason literals and `why` order, `columns`, `curves` and error text are compared by the identity tests; the `JunctionPositions` call order inside `build` must not change.

### Cut B: placement plus render plus routes plus signals leave topology
- Files: `placement/` gets horizontal placement (about 2445), render (219), signals and route events (342), terraform glue, cut (126), seed (28), detours (147): about 3300 lines in two or three files. topology.cpp ends at about 1370 (notation, graph, order, structure, programs, topologies).
- To a shared header: `JunctionTrackRoute` and `JunctionTrackPieceTag` to root `junction_route.h` (self-contained); the placement struct and `JunctionGraphSeed` to a placement header; `JunctionTrackSignal`, `JunctionSignalReason` with the signals file.
- Remaining cross-folder includes: placement -> topology only (graph types); legacy/nodes and tool/hover include both; levels.h includes root headers only. No folder cycle (topology/ stops at programs and `JunctionIs*Edge`; `JunctionSlotRows` and `FindRowItems` stay in topology and are exported).
- Hash risks: the largest move; `MakeJunctionGraphSeed` and cut order must stay identical (cut picks the first tried among equals); `JunctionRouteEvents` is used by legacy nodes, so its header moves with callers updated; signals call `PlaceDragSignals` (stays in topology/ or follows); function overload resolution in tests (PlaceJunctionTrackGraph x3, default arguments) must be kept.

### Cut C: no file move yet; break the include cycle with a smaller levels input
- Files: unchanged sizes (topology.cpp 4522). `JunctionTrackRoute` and `JunctionTrackPieceTag` move to junction_route.h; `SolveJunctionLevels` takes the fixed tiles (placement.nodes, placement.ports) as vectors or a small input struct instead of `JunctionTrackPlacement`.
- Shared header: junction_route.h (root) for the route types; the input struct next to levels.h.
- Remaining cross-folder includes: topology.cpp -> placement/levels.h and positions.h (one direction); levels.h no longer includes topology.h. The big file split is deferred; later cuts A or B then need no further levels change.
- Hash risks: lowest; the call at 3480 passes `placement.nodes` and `placement.ports` unchanged; no logic moves. The call signature change is not a pure move, but the values are the same.

### Relations between the cuts
C removes the levels.h cycle on its own; A is a subset of B (B adds render, signals, cut, seed and detours).
