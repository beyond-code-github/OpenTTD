# RECON: where a template block generator meets the pipeline (HEAD 13f7073742)

## Summary
1. Recommended seam: (a) routes. The template emits `JunctionRoute` move strings per lane path plus a minimal graph and tags; rasterise with `RasterizeJunctionRoute`, signal with `SignalJunctionTrackRoutes`, check and build from the `JunctionLayout`.
2. Render (b) is not reusable for a block: it needs a solved `JunctionTrackPlacement` whose spans match the program tokens one to one, and the programs, structure and order assume main W/E rows.
3. Signals read only graph edges/nodes/path_edges, topology groups, paths and the route tags; they do not read the placement (except optional prio spans), so (a) feeds them cleanly.
4. Biggest blocker: the checker's topology. `ExpandJunctionLanes` makes every lane pair of a movement and refuses unequal main-main lane counts; a 4-in 2-out block with partial reach needs single-lane groups (one per track) to be expressible. `FindJunctionPorts` also hardcodes the W/E/S edges and accepts only axis-only tiles on the edge.
5. Second risk: prio. Signal prio code assumes a main-line row with a side row on NW/SE and an empty neighbour; in a 4-parallel-track block that room may not exist. Level solve and build are fine (they take layout/routes).

## 1. Render input
`RenderJunctionTrackPlacement(topology, paths, graph, structure, programs, placement, error, routes)` (`placement/junction_render.h:25`).
- topology: only `groups[slot.group].side` through `JunctionPortSlots` (`junction_render.cpp:41,65`). N side is refused (`:66`). Start heading comes from the side: W east, E west, S north (`:67`).
- paths (`JunctionLanePath`): `.from`, `.to` (`:65,164`). `graph.path_edges.size()` must equal `paths.size()` (`:40`).
- graph: `path_edges[p]`, `edges[e].from/.to` (node flag + index), `nodes[i].split` (`:61,176-193`).
- structure: `bridges[b]`: `edge`, `doubled`, `crossed`, `tunnel` (`:47-52,95-111`).
- programs: `edges[e]`, `copies[e]`; per token `kind`, `hand`, `role`, `index` (`:87,93,118,131`).
- placement: `nodes`, `ports`, `edges[e][t]` spans (first/last tile per token), `copies`, `bridges[b][copy]` heads, `size` (`:40,88,99,116,164`).
- Not read by render: `order`, `item_rows`, `choices`, terraform, `prio`.
Could a template fill these directly? In principle yes: a hand-built graph with 2 nodes per split and a program per edge, and spans computed from the template's tile table. But:
- Every token needs a span, and each bend must match exactly one (hand, halves) raster (`:144`), so spans must be derived from bend geometry the template would have to compute anyway.
- `graph`, `structure` and `programs` have shapes made by the topology/order/structure builders for rows (`FindRowItems`, `JunctionIsInEdge`, stack items). A block has no stack. unknown: whether `JunctionTrackGraph`/`JunctionTrackStructure` can be built by hand without `order`; I did not read `junction_graph` constructors in full.
- The placement struct is a plain data struct (`junction_placement.h:272`), not entangled with the solver for render. The entanglement is in producing it (`PlaceJunctionTrackGraph`, row/column relations).
Smallest set a template must fill for render: `paths`, `graph` (nodes, edges, path_edges), `structure.bridges`, `programs.edges` (copies empty), `placement` (nodes, ports, edges spans, bridges, size). That is five structures to stay consistent; (a) needs fewer.

## 2. Routes
- `JunctionTrackRoute` (`junction_route.h`): `path` (index into lane paths), `copy` (0 or 1), `pieces` (`JunctionPiece`: tile, trackdir, units, on_bridge), `tags` (one `JunctionTrackPieceTag` per piece).
- `JunctionTrackPieceTag`: `edge` (graph edge), `token` (index into the edge's program, or `RUN` = UINT16_MAX), `run` (tokens before this piece), `copy`.
- Rasteriser: `RasterizeJunctionRoute(JunctionRoute{start, start_trackdir, moves})` gives pieces and bridges; `AddRasterToLayout(layout, raster)` then `NormaliseJunctionLayout(layout)` makes the `JunctionLayout` (`junction_route.h`; used in render `:171,201`).
- Moves are a string: `F` straight, `L`/`R` 45-degree turns, `Bn` bridge of middle n, `Tn` tunnel (`junction_render.cpp:107,111`).
- Yes, a template can emit `JunctionRoute`s directly and get a correct track-only layout; no placement is needed for that step. Tags are extra: only signals and the level solve read them.
- Tags tie routes to the graph: signals use `tags[i].edge` and `graph.nodes[..].split` to find splits/merges (`junction_signals.cpp:42-44,151`). So a template must also give a graph whose edges and nodes match its pieces, or tag pieces itself.

## 3. Signals
`SignalJunctionTrackRoutes(layout, routes, topology, paths, graph, programs, spec, why, prio_beside, prio)` (`signals/junction_signals.h:50`).
Really read:
- routes: `pieces` (tracks, bridge heads `:95-103`), `tags[].edge` (`:42,151`), `path` (`:140`).
- topology: `movements`, `groups[].main` (`:140,306`). Main-main paths get prio and through treatment; others get bays.
- paths: `.movement` (`:140,155`).
- graph: `path_edges`, `edges[].to` (node flag, index), `nodes[].split` (`:44,48,151-157`).
- programs: `copies[e].empty()`, roles LaneSplit/LaneMerge only for doubled edges (`:49,63-64`); a block with no doubled legs needs only correctly sized empty `copies`.
- spec: `prio_tiles`, `sd`, `tl`, `signal_type` (`:137,233,267,304,354`).
- Not read: placement, order, structure.
Prio: phase 0 (`:136-240`) walks each main-main route's merge events, finds a joiner route of another movement ending at the same node, checks the tiles beside the main line are free (`:193-207`, "the prio element at x,y does not fit"), then draws.
- With `prio != nullptr` it draws only the span whose `merge` equals the merge tile (`:212-217`); with nullptr it derives the element from the routes and `spec.prio_tiles` (`:219-`).
- `JunctionPrioSpan` = `tiles` (tile, bits in drawing order) + `merge` tile (`junction_placement.h:266`). The solver builds it from a `Prio` Elem at `junction_placement.cpp:3300-3320` using `JunctionPrioStepBits(step, side, exit_edge)`, side NW or SE of the lane.
- A template can provide spans directly with the same helper, or pass nullptr. Either way the prio row must lie beside the fixed track on a free side; the 227 join has choice tracks on one side and the next bundle on the other, so room is `unknown` until tried.
- A joiner also needs `paths[r2].movement` different from the main route's (`:155`), so each track pair must be its own movement.

## 4. Checker
`CheckJunctionRules(layout, spec, topology)` (`junction_rules.cpp:1805`). It reads only layout (tracks, signals, bridges) and the topology; it traces paths from the layout.
From the topology it uses:
- `JunctionPortSlots` (groups in W, S, E, N order; `junction_topology.cpp:34`).
- `FindJunctionPorts`: for edges NE (W side), SW (E side), SE (S side) the layout's edge tiles must number exactly the slots of that side and hold axis-only tracks, else no ports for that edge (`junction_rules.cpp:205-235`; the "mismatches give no ports" test at `tests/junction/junction_rules.cpp:108`). Tiles are on x=0 and x=size.x-1 (W/E) and y=size.y-1 (S).
- `ExpandJunctionLanes`: the lane pairs that must exist (`CheckConnectivity` `:455-475`: missing, ambiguous, and "unexpected movement" when a traced path has no expected lane pair `:505-508`).
- `groups[].main` through `IsThroughPath`/`IsExitPath` (`:151-162`): main-main is treated as main line for prio, signals, order, deadlock.
Can a 4-in (W) 2-out (E) straight block be described?
- One W group of 4 and one E group of 2, both main, same line: refused. Quote: `"a through movement needs equal lane counts"` (`junction_topology.cpp:75`). The checker calls it with a null error, so the result is empty lanes and no connectivity expectation at all (silent).
- Same groups with different `line`: `in.main && out.main && in.line == out.line` false, so the AnyToAny branch gives all 4x2 = 8 pairs (`:117-121`). Wrong for the 227 reach (fA reaches only L0).
- Workable: one single-lane group per track (4 W groups, 2 E groups), movements fA->L0, cA->L0, cA->L1, cB->L0, cB->L1, fB->L1. Single-lane groups give exactly one path per movement (`:117-121`). `lanes` of each group is 1, so slot order is the group order W north to south (E reversed). I see no code that forbids several groups on one side (`JunctionPortSlots` loops groups). Not run; `unknown` until a test confirms.
- Main-line rules: setting all groups `main` makes every path a main-line path (prio, order, deadlock rules apply to all). Setting some non-main turns them into exit paths with bay rules. Which is right for a block is a design question, not in the code.
- The row-based topology builder (`junction_topology.cpp:150-170`) is not used by the checker but would refuse it: `"a main slot without a through partner is not ruled yet"` (line 166). That matters only for route (b).
- The block needs a straight line: no S port needed, so the S-edge condition passes (no S slots, zero tiles).

## 5. Levels and build
- Level solve `SolveJunctionLevels(terrain, facts, offset, ramp_levels, tl, nodes, ports, layout, routes, prio_beside, plan, cost, levels, why)` (`placement/junction_levels.h:61`). It reads `layout.tracks`/`bridges` (`junction_levels.cpp:372,563,1493,1497`), and per route `pieces` and `tags[j].edge/.copy` (`:1522-1539`). It also needs `nodes` and `ports` tile lists and `prio_beside` from the signal pass.
- So a template must supply node tiles (its split/merge tiles), port tiles, routes with tags, and prio_beside (output of signals).
- Terraform, pricing and build: `MakeJunctionBuildSteps(layout, origin)` and `EstimateJunction`/`EstimateJunctionTerraformed(layout, origin, pricing, height_cost, steps)` take a `JunctionLayout` (`junction_layout.h:141-148`). Terraform plan and `track_levels` come from the level solve (`placement.terraform`, `height_cost`).
- Flat ground: a template layout goes straight into build and pricing. On sloped ground it needs the level solve above, which needs routes+tags and node/port tiles.
- The driver `junction_pipeline.cpp:36-58` shows the exact order: place, render with routes, signal with `&placement.prio`, level solve.

## 6. Existing hand-built layouts
- Helper: `static JunctionLayout Build(std::initializer_list<JunctionRoute>)` in `src/tests/junction/junction_rules.cpp:30-39`; it rasterises route move strings, `AddRasterToLayout`, `NormaliseJunctionLayout`. There is no ASCII-to-layout parser; `RenderJunctionAscii` only prints.
- Companion helpers there: `Port(role, x, y, td)` (`:41`), `AddSignal(layout, x, y, td, type)` (`:56`), `Has(violations, rule, x, y, text)` (`:48`).
- Example: `TEST_CASE("CheckConnectivity: a straight line passes", "[junction][rules]")` (`:164`): `Build({{{0,0}, Trackdir::X_SW, "FFFF"}})`, `MakeJunctionCheckContext` with two ports, `CHECK(CheckConnectivity(ctx).empty())`.
- Also `MakeTestJunctionLayout()` (`junction_layout.h:140`) as a canned layout. unknown: other users of it; I did not search further.

## 7. Recommendation
(a) Routes
- Template emits a `JunctionRoute` per lane path (and bridge `B1` for the one crossing) from a fixed tile table; difference constraints give the run lengths.
- Smallest surface: rasteriser exists and is tested; layout falls out in two calls.
- Signals still need a small graph (nodes, edges, path_edges) and tags per piece, so a tag builder is the one new piece.
- Keeps "stages read the graph": the template's graph is the source; the layout is its output only.
- Prio and node/port tiles for the level solve are template outputs too.
(b) Graph + programs + placement into render
- Reuses render, but render matches each bend against spans with a unique (hand, halves) rule and needs consistent `structure`, `programs`, `placement`.
- The graph/programs shapes come from row-based builders; a block has none, so hand-building them is the same work as (a) plus more invariants.
- Placement struct must be filled without the solver; every token needs a span.
- Gains little: render is a thin loop over the raster that (a) calls anyway.
- Pro: one render path for hub core and blocks, and the same tags by construction.
(c) Layout tiles directly
- Easiest to write and the checker/build accept it at once.
- No routes, no tags: signals and the level solve cannot run; signals would be drawn by the template.
- Breaks the project rules: "Stages read the graph, never a rendered layout" and "Hand-made layouts are checker fixtures only; layout design belongs to the generator."
- Fine only as a test fixture, never as the product seam.
Recommended: (a). First test (B1, red first): a join block 227 built from templates: two `JunctionRoute`s for the choice legs and fixed tracks on flat ground, a hand-made `JunctionTopology` with 4 single-lane W groups and 2 single-lane E groups, movements as in section 4; assert `CheckConnectivity` reports exactly the six expected movements (no missing or unexpected) via `FindJunctionPorts`. Then step two: add the graph and tags and run `SignalJunctionTrackRoutes`; assert signal and prio violations are empty or list which rule needs a block-specific form.
