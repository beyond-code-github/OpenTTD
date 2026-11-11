# FABLE: independent review of D201 (topology -> graph -> solve) and RS1 as a graph edit

Read-only review, branch feature/junction-tool at 5c0242233f. Lines are src/junction_nodes.cpp unless a file is named. RECON-template-graph.md was checked against the code; its claims hold where I looked (classes :263-284, link freeness :205-221, slot legality :472-477, crossings :561-630, flat_bits :1021-1037). Additions and corrections are marked **new**.

## 1. Is D201 sound? What the solve takes from the reference positions

Every point's coordinate is `value(cls) + tile - flat[cls]` (h:28-29). Sorted by what the reference really carries:

| use | where | what it carries | gauge only? |
|---|---|---|---|
| diffs as offsets from flat | NodeAddAtLeast :694-700, AddDirected in the builder | constants, invariant under moving a whole class | yes |
| diagonal k = `q.tile.x - p.tile.x + dx*m` | :723-726, again in diagonal_ok :1163-1164 | the p->q displacement must equal the raster of `halves` from p.tile (:711-715). I found no check of this anywhere: a mismatch gives wrong joins and occupancy, not a Template error | yes, but it must be **consistent** |
| late gap k uses `q.tile - p.tile` and the flat unit gap u | :730-741 | constants; u must match the reference lengths | yes, consistent |
| raster interiors from p.tile | :711-715, :745-756, occupancy | gauge plus consistency | yes |
| **snap target**: nearest to `flat[c]` | snap :1042-1058; JunctionNodeSnap :905-909 | **the preferred output**. On flat ground the solve returns the reference: test "keeps the flat T" (tests/junction_nodes.cpp:587-610) requires `TrackKeys(search) == TrackKeys(FlatT)` | **no**: it is the design target |
| anchor fixed to `flat[anchor]` | :687 | absolute frame; NodeAnchorShift re-derives the offset (:1672-1677) | yes |
| flat_bits: tracks per **flat tile** | :1021-1037; `bits_at` uses `.at()` | **which pieces share a tile** (switches). Two pieces whose reference tiles coincide get the union, which changes Switch/Straight facts | no: tile identity |
| slot legality | :476, junction_rules.cpp:682-687 (`TracksOverlap(TracksAt(layout, tile))`, bridge head) | tile identity again | no |
| occupancy order and gaps | :851-890 | ties broken by flat order; `gap = min(2, b.flat_lo - a.flat_hi)` on one flat row (:884-885). **new**: if two elements on one flat row overlap in the reference, gap <= 0 and the chain **allows** an overlap in the output instead of failing | no: left-to-right order on a row |
| crossings | AddCrossings :585-628 | "under this bridge" is decided by the reference tile being strictly between the heads. Something not under a bridge in the reference gets no crossing constraint | no: over/under topology |
| box edges | AddBoxEdges :632-656 | which path ends are on the reference's extreme column/row | no: extremes |
| prio row side | :369, :462 | joiner side from reference rows | no: side |
| path x/y signs | :240-260 (both signs = Template error) | sign of reference displacements | no, but trivially topological |

**Verdict.** The reference is more than a gauge. It carries five topological facts (tile sharing, left-right order on rows, over/under at bridges, box extremes, sides) and the **target shape** (the snap). A reference that has to be consistent, collision-free and the intended compact shape is a drawing in every respect except the stored track bits. D201 is sound if it means "the graph builder outputs the graph **plus** a reference embedding it computes by maths". It is **not** sound if it means "nodes have no positions" (DESIGN-N3 §11, last RS1 bullet: "no positions are chosen by the insert"). With today's solve every inserted node needs a reference tile, and choosing those tiles is a geometric decision.

**Could the solve need no reference?** Yes, at these costs:
- Gauge parts (offsets within a class, a per-diagonal displacement instead of `q.tile - p.tile`): cheap, mechanical.
- Tile identity: explicit shared-tile groups (switch pieces list their co-tracks). Cheap to moderate.
- Crossings, box edges, prio sides: explicit relations from the topology. Moderate.
- Occupancy order per row: an explicit left-to-right order per row, i.e. the planar embedding by another name. Hard, and it rewrites the most fragile part of the solve.
- Snap target: needs a real objective (for example least total link length, the dual of min-cost flow over the difference system). `Lo` is not compact: classes bounded only from one side go to the -1024 bound. This changes outputs and breaks the "keeps the flat T" oracle.

Not worth it now. Keep the reference and make its consistency a checked invariant (§3).

## 2. What a direct builder must compute that ConstructJunctionTMap computes on tiles

**new, important:** the node path only ever uses **one constant drawing**. `JunctionNodeTopologyT()` is a function-local static: `ConstructJunctionTMap(MakeTestJunctionSpec(), nullptr, std::nullopt, true)` (junction_generator.cpp:1936-1940). The game calls it whatever the user's spec (junction_game.cpp:446, :472), and so does SizeJunctionPrio (junction_generator.cpp:1954). So:
- Terrain and first_height play no part in the node path: terrain = nullptr, and first_height defaults to the old router's flat height (junction_generator.cpp:891, :911).
- The line router runs once per process, inside that static, and costs nothing per hover. D201 gains no speed for the T. What it gains is repairs as edits and a route to other hub shapes.

What the builder has to reproduce (junction_generator.cpp:874-1737):
1. **Frame:** hub_x, the ml_gap rows `g`, `w_row = g+1`, `b_row = g+2`, `xO = xI + branch_gap + 1`, STUB = 2 (:917-925).
2. **Port columns:** x_w and x_e come from cropping the old-router flat T (:913-914). junction_game also reads `topo.hub` and `topo.layout->size.x` for the start offset and the blocked-port check (junction_game.cpp:459-461). The builder must provide hub and size.
3. **Box height:** the box grows until the edges route (:1103-1199). On flat ground this is the minimum height at which the router finds routes.
4. **Edge shapes, from router searches:** the S/N port edges, A, and the branch edges. Their curve state is chained across edges (CS1-CS3, :979-1070), they are judged by rule 3, and B crosses A on a bridge. Topologically the builder needs only each edge's turn program (runs, curves, bridges) and the switch order. But because of the snap, it also needs the router's **compact lengths**, or the output differs.
5. **Paths:** today MakeJunctionCheckContext traces them by following the drawn track (:660). The builder states them directly: point sequence, trackdirs, units (half = 1, axis piece or bridge = 2, junction_route.cpp:70).
6. **Not needed:** signals and prio rows. State 0 of the search is track-only (`RenderJunctionNodeTracks(flat_tpl, flat_tpl.flat)`, :1672). Its template, not the template of the finished FlatT, is the right oracle.

**What stays layout-level:** items 4 and 5 (the shapes and lengths the router chooses) and the collision-free embedding. For one fixed T a hand-written or table builder is easy. For general hubs, "by maths" means writing a layout engine, which is what the router is today.

## 3. Migration order (safest first)

1. **Exact template comparator.** Compare in order: points with their reference tiles and trackdirs, links, classes, flat, diffs, joins, pieces, diagonals, late gaps, late crossings, prio rows, and the path units and signs. Order matters for the semantics, not just for a tidy diff:
   - classes are numbered by first point (:271-284);
   - the snap fixes variables in index order (:907, :1337, :1481);
   - occupancy stable-sorts in insertion order (:854).

   The existing test compares counts only (tests/junction_nodes.cpp:600-604). That is not enough.
2. **Split the builder from the tracer** (RECON Option A, which I agree with). NodeBuilder takes an explicit graph input: per path, the pieces with reference tile, trackdir, on_bridge and units; the bridges; and a tile-sharing and slot oracle in place of `ctx.layout`. The four layout queries are UsedTracks, SwitchEventsOf (:147), IsLegal/IsSignalledPiece (:476) and layout.bridges (:217, :327). The tracer becomes one producer of this input. Gate: [nodes] and [nodeoracle] identical, plus the hand-drawn-layout tests (tests/junction_nodes.cpp:89, 124, 193, 276, 388, 628, 699, 1033, 1291) unchanged.
3. **Direct producer for the T,** checked equal to the traced state-0 input by the comparator. It is a single constant, so the check is exact and cheap.
4. **Reference validator, kept as a test oracle and never retired:** render at flat, require no collisions, then require retrace == template. The test at :593-604 already does the render and retrace. This is the only thing that catches an inconsistent reference (§1).
5. **Repairs as edits on the input:** RS1 first, the detour port second. InsertJunctionDetour today relies on layout collision checks (:1575-1580), which the validator must replace.
6. Retire ConstructJunctionTMap from the node path last. Keep it in tests as the oracle; its consumers hub, size and ports (junction_game.cpp:452-486) move to the builder.

**Risks:**
- Renumbering silently changes snap results.
- A reference that is inconsistent or overlapping passes the template and fails, or worse passes, in occupancy (§1, gap <= 0).
- Retiring the tracer removes the only consistency check (step 4 keeps it).
- Choosing the wrong oracle template (finished FlatT vs track-only state 0).

## 4. RS1 ramp as a graph edit: what it must supply, what can go wrong

**What the edit supplies,** for diagonal p->q split into p ..diag1.. a | b ..Y.. c | d ..diag2.. q:
- **Points:** a = last half of diag1, b and c = Y pieces, d = first half of diag2, each with a trackdir. The two curves are opposite hands (an S).
- **Links:** p-a diag, a-b rigid, b-c free Y, c-d rigid, d-q diag.
- **Classes:** one new X class {a,b,c,d}. Rigid links and the Y-free link join X (:268). Two new Y classes, {a,b} and {c,d}. Adding an X class shifts **every** Y index (Y follows X, :279), so diffs, pieces, joins, bridges and points must all be renumbered. Rebuilding through Option A avoids this.
- **Units:** the new pieces' units. Every later point on **every path containing the link** shifts, because links are deduped by (from, to) (:226).
- **Rules (rerun, never patched):** the path's whole curve-gap list (the pair rules change, T2 :437-453); rule 1 for the new links (:421-434); late gaps spanning X runs; prio rule 3 and slot rules 4-6. The axis pieces of the ramp are new slots (:472-477).
- **Joins:** a->b and c->d.
- **Reference tiles:** see the next list.

**What can go wrong:**
- **Reference consistency is forced, not free.** With p and q fixed, a and d lie on the old diagonal line. The ramp's reference length is therefore fixed by the geometry at about 1-3 tiles (RECON §3, which I agree with), and the solve stretches it. "No positions chosen" is not available: the edit must compute these tiles, and diag2's raster must end exactly on q.tile (nothing checks this, §1).
- **Minimum halves.** Each new diagonal needs >= 3 reference halves to stay free (:214), and can shrink to 2 (m_min, :429). A diagonal that is short in the T cannot be split free-free. A rigid side merges classes and pins the ramp, possibly to a Template error ("a free diagonal joins one class", :430). Measure the T's diagonal halves before briefing.
- **Room to stretch.** Stretching the ramp by s rows moves {c,d} by s. diag2 must then shrink by s map steps, unless q's class moves. A diagonal with few halves caps the ramp length.
- **y-sign.** The ramp must run in the path's dir_y. The diagonal's dy already fixes this, so a correctly built edit cannot violate it (:240-260). **new:** RS1 covers Y ramps only. A slope that rises along x needs an X straight (diag -> X run -> diag), which is not designed.
- **Collisions in the reference.** The reference tiles beside the old diagonal must not coincide with or overlap another path's tiles. Otherwise flat_bits unions them (:1023) and occupancy may accept an overlap (:884-888).
- **Bridges.** If the old diagonal had a late crossing (:594-601), the edit's reference decides which piece (diag1, ramp or diag2) is under the bridge, and that is a topology decision. A Y ramp under a Y-axis bridge is a Template error (:603-605).
- **Shared links.** Every path through the link gets the same edit. Units shift per path.
- **Placement.** Nothing forces the ramp onto the failing rows except the snap target. Put the reference ramp at the fault's row (JunctionNodeHeightFault.row, h:155).

## 5. What I think is wrong or missing

1. DESIGN-N3 §11 ("q is just a graph node, it doesn't have an explicit position"; "no positions are chosen by the insert") contradicts the solve as written. Every node needs a consistent reference tile, and the snap treats the reference as the target. Either the edit computes the reference (recommended), or §1's no-reference rewrite is scheduled. That rewrite is the bigger job.
2. D201's "no interim layout" removes a **type**, not a **drawing**. The builder's hard part (the compact, collision-free, ordered embedding) is the router's job today. For the T this is trivial, because the T is a constant (§2). The case for D201 rests on repairs as edits and on future hubs, not on speed or on avoiding geometry.
3. Retiring the tracer, as D201's migration says, should not mean deleting it. Keep render -> retrace -> compare as the test oracle for every builder output and every edit (in the spirit of D193).
4. The planned oracle comparison must be exact and in order. The current test compares counts only.
5. Detours (InsertJunctionDetour) rely on layout collision checks today. Porting them to graph edits needs the same reference validator.

## Recommendation

Adopt D201 in this reading: a graph input of paths of pieces with reference tiles, from which NodeBuilder derives the rules (RECON Option A). The direct T builder and the edits produce that input, including reference tiles computed by geometry. Build in this order:
1. The exact comparator.
2. The builder/tracer split, with no behaviour change.
3. The direct T producer, checked against the oracle.
4. The validator.
5. RS1 as an input edit with a forced short reference ramp.

Before briefing RS1, measure the T's diagonal halves and amend DESIGN-N3 §11's "no positions" sentence.
