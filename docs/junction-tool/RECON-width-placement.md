# RECON: placing and rendering the T at width 2

Read-only recon at HEAD 847079d05f. Paths under `src/junction/`. Line numbers are at this HEAD.

## Summary
1. Recommendation: second lane item, built as "track k = copy k" of the existing copy machinery. Reject fat track.
2. Why: C1 made a bundle ONE path with `width = 2`, so a track is a (path, copy) route. Routes, tags, programs, placement chains, bridge head pairs and the (edge, copy) joint keys already have that shape.
3. Fat track fails because chains hold one row per element and the rasteriser has no offset operation; a nested 90-degree corner needs a larger bend, not a translation.
4. Steps: 7 (1 mechanical pin, 4 design-bearing, 2 mechanical). Each moves one "not ruled yet" refusal; width 1 stays byte-identical by golden pins.
5. Biggest risk: band edges at width 2 (step 5): no precedent for a column-axis head pair (HeadsPair is row-only) and nested Corner sizes inside the form-choice joint model.
6. Open design call before step 2: what the T's Prio and Approach items become when the join node is a block (they serve the merger, not the edge).

## Q1. Per-edge geometry that width changes, by stage
Nothing outside `BuildJunctionBundleGraph` reads `JunctionTrackEdge::width` or `JunctionTrackNode::block` today (grep, `.width`, `.block`). The bundle graph is built on the lanes=1 topology; callers must use that topology and its paths themselves (`topology/junction_topology.cpp:496-519`, the function returns only the graph).

Order, `OrderJunctionTrackGraph` (`:524`): rows hold edges and nodes as items, bands hold events and lines of edges. No geometry. Width changes nothing. Fat: none. Second lane: none.

Structure, `StructureJunctionTrackGraph` (`:857`):
- Row stack: per row at most one item per side (`side_item`, `:923-935`): Prio, Copy, PrioAndCopy, Approach, plus Lane. `need_copy` is set only when the row leg is crossed (`:921`). Fat: the Lane item would be two rows tall. Second lane: every row of a width-2 graph gets a track-1 item (the Copy kind) whether crossed or not.
- Bridges (`:942`): `doubled = true` for any crossed row edge, `false` for band bridges (`:975`, `:991`). Fat: a bridge widened by width. Second lane: `doubled` follows width, on band bridges too.
- Joiners, prio, approach (`:896-921`): these serve a merge node. With a join block they are the block's footprint, not an edge track. Decision needed (see steps).

Programs, `ProgramJunctionTrackGraph` (`:1007`):
- Row leg (`:1162`) emits `Over` tokens, plus `copies[e]` only when doubled, with `LaneSplit`/`LaneMerge` S-bends. In and out edges: same (`:1184-1198`).
- Band edge (`:1204-1260`): one token list, crossers get two `Under` tokens per doubled bridge (`:1243-1244`).
- `programs.copies` on a band edge is a `NOT_REACHED` (`:1553`).
- Fat: one program per edge. Second lane: `copies[e]` is track 1's program for every width-2 edge, no lane roles.

Arrangement, `ArrangeJunctionTrackGraph`: `unders` are per Under token of a doubled bridge, both copies. Fat: counts stay. Second lane: a width-2 crosser adds its own Under pair per track (four pass-unders under a width-2 bridge).

Placement, `PlaceTrackGraph` (`placement/junction_placement.cpp:87`):
- Stack rows: item i sits exactly one row below item i-1 inside a row (`:2107-2110`, `eq(1, item_pt[i], item_pt[i-1], 1)`), at least one between rows. This is the one place item height is fixed.
- Chain builder (`:806-870`): per (row edge, copy) chain; `chain_copy[e]` is the second chain; `Over` becomes `HeadsPair` only when doubled (`:854`). Band chains (`:~300-420`) are single and refuse a doubled bridge (`:350`).
- Doubled-leg relations (`:2199-2290`): lane pair, exact stagger "copy 1 heads one tile upstream" (D118), floors from lane split/merge. This is the code a width-2 row edge would reuse minus the lane bends.
- Crosser tiles and head hugging (`:437`, `:2008-2050`): `edge_tiles(e)` reads `chain[e]` only (`:971`). A width-2 crosser needs both chains' tiles.
- Occupancy (`:1371-2000`): `related()` (`:1365`) treats copy 0 and copy 1 of one edge as unrelated, so parallel tracks are already checked as unrelated pieces; adjacent parallel is legal (`JunctionUnrelatedTileGap` gives 1, or 2 only for diagonals touching across an edge; `junction_track_geometry.cpp:226`). New need: an EXACT parallel relation (shape, like the stagger) so the tracks stay adjacent and do not drift apart.
- Curve gaps (`:2635-2700`): per route (path, copy), `JunctionCurveGaps(hands, tl, curve_speed, reference_speed)` has no width input. Each track has its own bends, so no change if each track is its own chain. `sym_routes` (`:998`) makes copy 1 follow `chain_copy` only on doubled edges, else it shares `chain[e]` (copy -1). A bundle track needs its own chain on every edge.
- Spans and bridge checks (`:3322-3480`): `heads_of(b, copy)`, `placement.bridges[b][2]`, tunnel/length per copy. Already per copy.
- `JunctionJoint` (`placement/junction_joint.cpp:424-503`): lengths keyed by (edge, copy). `JunctionMilp` and `JunctionPositions` have no edge or copy notion (`junction_milp.cpp`, `junction_positions.cpp`: no match). Unknown beyond that without running the joint on a width-2 graph.
- Ports (`:3299`): one port tile per slot. Track 1 needs its own port tile. See Q5.

Render, `RenderJunctionTrackPlacement` (`placement/junction_render.cpp:33`): per path, copy loop `c < 2` only when some edge is doubled (`:62-63`); copy 1 only on a doubled edge (`:86`, `:99`, `:169`, `:204`). Fat: draw width routes from one program. Second lane: loop over the track count and read each track's own program and spans.

Signals: see Q6.

## Q2. Rows: a second row item, or a two-tall item
The stack item is one Y row by construction: chain elements carry one `item_pt` (`:855-866`, `eq(1, el.first, item_pt[h->index], 0)`), and tiles of an element lie on that row. A two-tall item would still need a second row for track 1's tiles, so it is a second item in all but name.
- Item height is fixed at `:2110` (constant 1 within a row) and by one `StackItem` per row entry (`side_item`, `structure.cpp:923`). No per-kind height.
- The cheap form: a width-2 row edge gets `Lane` plus a track-1 item adjacent (reuse kind `Copy`; today it is Lane's neighbour on the side away from the joiner, `:921`). Heights stay 1. No change to `:2110`.
- A fat 2-tall item would change `:2110` to a per-item height and still need derived tiles for track 1 at every consumer of `chain[e]` (43 uses of `chain[`, 16 of `chain_copy` in `junction_placement.cpp`).
- Conclusion: rows need the second item either way. Rows do not separate the two approaches; bands do.

## Q3. Band edges at width 2
Forms today (`topology/CLAUDE.md`, "A band edge crossed by a single bridge carries two forms"): an S-bend under the bridge, or a square run (Corner, crossing, Corner back); placement picks by one binary per crossing group (`choices.form`).
- S-bend form: two parallel tracks are the same S-bend translated one tile across. Derived: the translated diagonals sit one tile apart, which is the legal Upper|Lower or Left|Right share (`JunctionTracksMayShareTile`, `junction_track_geometry.cpp:214`). So two parallel 45-degree tracks ARE representable with the existing half-tile pieces; each is its own route and `AddRasterToLayout` merges tiles.
- Square form (nested Corners): derived from the nested geometry, the outer track's diagonal is one tile longer than the inner's when the two diagonals share tiles; so the two Corners are not translations of each other. The first Corner's outer track is the second Corner's inner track (hands reverse), so sizes cross over. Each track needs its own Corner element with its own size, tied to the other by exact relations.
- Constants: `JunctionBendRowForms` (`junction_track_geometry.cpp:144`) is per (heading, hand, corner), independent of width: no width parameter, each track's bend uses it as today. `JunctionCurveGaps` (`topology/junction_chain.h:39`) per route: no width parameter. Width enters only as new relations between the two chains.
- Fat track here: one chain at track 0 plus a derived track 1 would need an offset operation per token; S-bend is a translation, Corner is not, so fat fails on the square form.
- Diverge/Converge S-bends at nodes: node points are per node (`node_pt`); a block node (footprint across the trunk) is not a point. Unknown: whether track 1's bend ends on the node tile or the block edge.

## Q4. Bridges: the doubled bridge as the width-2 bridge
Same geometry for a row edge: two parallel bridges on adjacent rows, copy 1's heads one tile upstream (the parallelogram), both spans equal. Reuse by mapping track k to copy k:
- Reused as is: `HeadsPair` per copy (`:854-866`), exact stagger relations (`:2218-2231`), head hugging of crosser tiles (`:2008-2050`, per copy row), `heads_of(b, copy)` (`:3326`), spans per copy (`:3455`), `placement.bridges[b][copy]`, render head lookup (`render.cpp:99`), `CutJunctionTrackBridges` cutting doubled bridges into series of at most 2 crossers (`junction_pipeline.cpp:111`, only `doubled`).
- Dropped: lane split and merge S-bends, their floors (`JunctionRunFloor(Node, LaneSplit)`, `LaneMerge`), the shared lane (copy -1 occupants), the check "a doubled leg without its lane split and merge is not ruled yet" (`:825`).
- Changed: copy 1 runs the whole edge on its own item, so its chain has start tile, bends and end tile like copy 0.
- Band edge bridges: no precedent. A band bridge is one `Heads` element on the column axis (`:3335`); `HeadsPair` is row-only; the doubled-band refusal is `:350`. Width-2 band bridges need a column-axis head pair with the same stagger, and `bridge_lines` (`x_axis` flag, `:445`) both axes.
- Bridge over a width-2 crosser: heads must hug both crosser tracks' tiles on both bridge rows. Today `edge_tiles(x)` is copy 0 only (`:971`).

## Q5. Render
- Render builds each route from per-token spans of the placement (`placement.edges[e][t]`, `placement.copies[e][t]`), not from an offset of a route. One `JunctionTrackRoute` per (path, copy) already exists; `copy` is `uint8_t` (`junction_route.h:71`) and its comment says "1 for the copy-1 route of a doubled leg".
- No sideways offset exists. `RasterizeJunctionRoute` takes a start piece and moves `F L R B n T n`; straight runs and S-bends offset by moving the start, Corners do not (Q3). So offset-at-render is out; track chains in placement are in.
- Needed render changes (second lane): copy loop over the width, not "has a doubled edge" (`:62-63`); `copy1 = c == 1` for every edge (`:86`, `:99`, `:169`, `:204`); a start and end tile per track (placement has one port tile per slot, `placement.cpp:3299`); a node tile per track.
- The node-piece check fails for track 1: each route must pass the node's `placement.nodes[...]` tile (`render.cpp:193`, "a node has no piece on the route"). A block node needs a per-track node tile or a block boundary.
- Unknown: the track-1 port tile. The real topology has one slot per lane at an arm (group lanes adjacent, notation gap between groups); the bundle graph discards them (lanes = 1). The core's ends are block ports, not lane slots. Where recorded: `BuildJunctionBundleGraph` sets `groups.lanes = 1`.

## Q6. Signals and level solve
- `JunctionRouteEvents` (`signals/junction_signals.cpp:47-80`): for every edge with `copies[e]` non-empty, looks up the sibling route of the same path and places lane split/merge switches by S-bend role. With no `LaneSplit` or `LaneMerge` token, `with_role` returns the program size, no piece tag equals it, and no events are added. Safe by accident; make the gate explicit (role present).
- Node boundary events (`:37-45`) read the tag edge change at a node piece; with a block node that boundary changes (block rendering is C6, later).
- Level solve (`placement/junction_levels.cpp`): `copy` is read from tags (`:1539`) and stored (`:49`, `:287`). Groups are unions of tile variables tied by equal pairs (`:635-640`), not of routes. Parallel routes of different paths already meet on adjacent tiles in the T. Unknown: two parallel bridge decks at one level; not read, run to find out.
- Pipeline edits key on (edge, run, copy) (`junction_pipeline.cpp:249-265`): works for track index.
- Checker `CheckJunctionRules`: "ML bridge is not doubled" and `SameLanePath` compare routes by (from slot, to slot) (`junction_rules.cpp:162-166`, `:619-628`). On the bare core there are no lane slots; the checker is an oracle for composed hubs only (DESIGN-modules §6 H1).
- Prio spans (`placement.cpp:3312`) come from the Prio element of a row: join-block territory.

## Q7. Recommendation and steps
Second lane item, track k = copy k. At width 1 nothing changes: `JunctionIsDoubledEdge` (programs.copies non-empty) is the only switch the placement reads, and width 1 leaves `copies` as today.

Do not fix a fat track: it needs a widened constant in every relation family that reads a chain (occupancy, hugging, clearance, curve gaps, spans) plus a corner that is not a translation. A third option inside the chosen one: slave track 1's points to track 0 by exact "shape" relations (like the D118 stagger, `:2218`, `along(..., 1, true)`), so track 1 adds elements for occupancy, hugging, curve gaps and render but no new freedom for the solver. Use it in steps 4 and 5.

Steps (each one commit with its own test; each test moves a "not ruled yet" refusal):
1. Pin width 1 (mechanical). Golden tests: programs, placement size, route pieces of the T (L_R x L_R) and LL_RR x L_R. Test: the goldens pass and `BuildJunctionBundleGraph` at one lane equals the T already (exists). Guards every later step.
2. Structure and programs for width-2 row edges (design-bearing). Track-1 item on every row of a width-2 graph; `bridges.doubled` follows width for crossed row edges; `programs.copies[e]` = track 1 for leg, in and out edges, `Fan` and `Over`, no lane roles; a width-2 band edge refuses with a named message. Decide first: Prio and Approach items of a join-block node (keep as the block's reserved rows, or drop). Test: program golden of the T bundle's row edges and the stack.
3. Programs for width-2 band edges (design-bearing). `copies[e]` for band edges (remove `:1553`), Diverge/Converge, Cross groups per track (S-bend form first), four Unders per crossing of two bundles, arrangement unders per track. Test: program golden of edges 4 and 8 and arrangement.
4. Placement of width-2 row edges (design-bearing). `chain_copy` for every width-2 row edge without lane bends; exact parallel relations; `sym_routes` copy 1 on its own chains; port tile and node tile per track; drop the lane floors for them. Test: placement refuses at the first band edge with the named message after all row chains are built; row heads are staggered; occupancy legal.
5. Placement of width-2 band edges (design-bearing, biggest). Band chains per track; column-axis head pair with the stagger; crosser tiles of both tracks in `edge_tiles`; S-bend form first, then the square form with nested Corner sizes (two sub-commits if needed). Test: the T at width 2 places on flat ground, size recorded, `occupancy_failed` clear, joint loop settles.
6. Render per track (mechanical). Copy loop over width, track-own programs and spans, per-track port and node tile; update the `JunctionTrackRoute::copy` comment. Test: 12 routes (6 paths x 2), every track ends on its ports, no tile shared except legal diagonal pairs, ASCII golden.
7. Signals and levels on the bundle routes (mechanical). Explicit gate on lane roles in `JunctionRouteEvents`; run the level solve and signal pass on the width-2 T on flat ground. Test: both stages return a plan, no lane events, checker not asked.

Generalising two copies to w tracks for LLL is later (H2): `array<JunctionBridge,2>`, `c < 2` loops and `chain_copy` become vectors.

Biggest risk: step 5, band edges. Reasons: no column-axis head pair exists; the square form's nested Corners differ in size per track and sit inside the joint model's per-group form binary (`choices.form`, `choices.run`); and heads hugging two crosser tracks on two bridge rows is new. Mitigation: ship the S-bend form first (a translation), prove the T places, then add the square form.

Unknowns (not guessed): track-1 port tile source; block-node tile per track; whether two parallel decks pass the level solve; joint-model behaviour on a width-2 recording; build and pricing seam (`game/`) not read.
