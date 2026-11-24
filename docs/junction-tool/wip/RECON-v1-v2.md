# RECON: three junction drawing approaches (A pre-SSS generator, B planar mapped composer, C SSS v2)

Read-only recon, 2026-11-25. Facts with sources, no recommendations. Main checkout /home/pete/OpenTTD (feature/junction-tool, HEAD 2e9f3fc494 at read time); C read in worktree .claude/ng2 (wt-c2). Unverified items are marked "not verified".

- A = the pre-SSS "graph pipeline": `src/junction/` topology/, placement/, signals/, block/, `junction_pipeline.cpp`, hover in tool/, plus legacy/ (the generator before it). Sources: `src/junction/CLAUDE.md` "Pipeline"; each folder's CLAUDE.md.
- B = the planar mapped composer and component solver: `src/junction/planar/planar_components*`, `planar_model*`, `planar_layout*`, `planar_slots*`, `planar_gaps*`, `planar_paths*`, `planar_plan*`. Source: `src/junction/planar/CLAUDE.md` (main checkout) sections Channels, Channel paths, Layout program, Back end, Component solver, Gap steps, Slot layer, Line sides.
- C = SSS v2: `planar_structure*`, `planar_regions*`, `planar_shape_layer*`, `planar_draw*`, `planar_domain*`, `planar_hub*`. Sources: ng2 `src/junction/planar/CLAUDE.md` "Architecture", "Structure", "Shape layer", "Track map", "Drawing program", "Drawn hub"; `docs/junction-tool/DESIGN-sss-v2.md`; DECISIONS D361-D461.
- Naming: T1/T2/T3 = the T at one/two/three lanes (`PlanarTText(n)`, planar_topology.h:109). LL2 = PSG 227 pattern at two lanes (`PlanarLL2Text`). A's hubs are "T and LL_RR x L_R" (tool/CLAUDE.md "Generation on hover").

---------------------------------------------------------------------------
## 1. Pipeline of each

### 1A. Pre-SSS generator (A)
Stages in order (src/junction/CLAUDE.md "Pipeline"):
1. Topology (`topology/junction_topology.cpp`, 1916 lines): ports (side, direction, lane count, line, gap) and movements; lanes expanded by rule (`ExpandJunctionLanes`); `lane_paths` with prio flag (D301). Notation `LL_RR` etc. Only S-side branch ports ("N-side and both-side branches are refused as not ruled").
2. Graph (`BuildJunctionTrackGraph`): nodes (splits/merges, two legs each; cascades of two-way nodes), edges, crossings per edge "built by rule from the topology". Splits before merges along every track.
3. Order (`OrderJunctionTrackGraph`): sweep from the branch side north; each band's line of edges before/after each event kept (`band_lines`). Fails when legs not adjacent ("the legs must be adjacent items, else the order fails").
4. Structure (`StructureJunctionTrackGraph`): row stack (lane, copy, prio, approach items), side tracks, bridge/tunnel grouping, turns; every main-line bridge doubled (parallelogram, equal cost), branch bridges single; over/under by fixed rules (joiner over turning leg, etc.; "a through edge off its row in a crossing is refused as not ruled"); series cut at `JUNCTION_BRIDGE_MAX_CROSSERS` = 2.
5. Programs: one token list per graph edge (Fan, Corner, SBend, Over, Under, Tunnel, Above); node shapes (trunk heading, legs, hand) are data from the programs (D235).
6. Placement (`placement/junction_placement.cpp`, 3989 lines; `junction_positions` difference constraints): "the least solution of the graph's relations on the terrain facts"; every relation a rule floor "at least N" with its rule family (curve gaps `JunctionCurveGaps`, R1-R9 families for blocks, `JunctionUnrelatedTileGap`, `JunctionTracksMayShareTile`). Objective: least solution then each row change pushed toward its feature. Rows (Y) come from the structure; X by difference constraints; no MILP in the default path. Optional MILP "joint" (`junction_joint.cpp`, `SolveJunctionJoint`/`SolveJunctionJointLoop`: both axes plus every bend length as one integer program, objective flat build cost, lazy clash rounds recorded at its point) used by the bridge-cut fallback.
7. Levels (`junction_levels.cpp`): per-tile level solve with ramps/foundations/terraform on the rendered, signalled layout (the one stage that reads a rendered layout).
8. Render (`junction_render.cpp`): routes rasterised (`RasterizeJunctionRoute`) to a `JunctionLayout`; routes carry `JunctionTrackPieceTag`.
9. Signals (`signals/`): `SignalJunctionTrackRoutes` via `JunctionSignalNetwork` (filled by `MakeJunctionSignalNetwork`), drag spacing `PlaceDragSignals`, prio element placement (straight pattern, else leaf search up to 20000 leaves).
- Driver `junction_pipeline.cpp` (396 lines): `MakeJunctionGraphSeed` (graph -> order -> structure -> programs -> bridge cut -> placement), `CutJunctionTrackBridges` (search over series cuts; when no cut places, the joint loop on the default cut, lines 132-210), `ApplyJunctionDetours`, terraform entry.
- Hover: `PlanJunctionHoverNear` (tool/): solves at the cursor, then near offsets (at most 8 columns, 4 rows), "feeds the GUI estimate and build"; cache of 32 results; seed per preset cached (D242).
- Block path for wide hubs: `block/junction_block.cpp` (1193 lines) generic generator `BuildJunctionBlock` turns a data table `JunctionBlockProgram` into columns/routes/layout (columns = least solution of difference floors, rule families R1-R9, two-pass for R5/R7); `JunctionJoinBlock227Program` (PSG 227 join) and `JunctionExitBlockProgram` are the two tables; `ComposeJunctionHub` (block/junction_hub.cpp) composes the core's placement with oriented blocks into a `JunctionHub`. Bundle graph (`JunctionGraphWidth` 2) with width-2 T placing "on flat ground with its real blocks (size 86 x 63)" (placement/CLAUDE.md).
- legacy/: `junction_generator`, `junction_nodes`, `junction_skeleton`, `junction_throughput` ("superseded, not extended"; still called by gui/console).

Hand-coded vs derived in A:
- Derived from topology: graph, crossings, order, structure, programs, placement lengths (difference constraints), signals.
- Hand-coded per hub type: structure rules per hub shape ("not ruled" refusals: N-side branches, through edge off its row, bundle wider than 2, row block whose diverging leg goes north, all groups must have the same lane count in a bundle graph); the two block tables (227 join, exit block) with hand-typed segment lists (topology/CLAUDE.md-block/CLAUDE.md: "The exit block (points W, p1, s0 ... )"); the PSG 227 pattern's prio list. The checker "is a test oracle; the search still keeps it in acceptance, because the template does not yet cover every rule" (src/junction/CLAUDE.md Pipeline).

### 1B. Planar mapped composer / component solver (B)
Order (planar/CLAUDE.md; Back end; Component solver):
1. Topology (`planar_topology`): lines, routes, ports in ring order, event order; derived crossings; authored crossing marks (D389, D343/D344).
2. Channels (`BuildPlanarChannels`): group lines into channels by in ports, out ports, depth, crossed channels; sites (fray/lane split/join/lane merge) with straight channel, side (prio route decides, else least turn; ties = variant bits).
3. Channel paths (`BuildPlanarPaths`): per channel piece headings (45 degrees per piece, short way), crossing pieces, over channel (prio over non-prio, else a bit per open crossing); triangle flips (`FlipPlanarTriangles`).
4. Variants: bits o (open crossings), t (ties), f (flips), s (candidate steps) tag `o..t..f..s..`; up to 6 bits all solved, above that local search (one-bit flips, 4 solves at once; LL2 reaches o17t0f1s0 in 48 solves).
5. Gap steps (`planar_gaps`: `ResolvePlanarSites`, `BuildPlanarGapSteps`, `PlanarCandidateSteps`): walk the box ring; consecutive port lines on one side with different channels are a pinned gap, giving fan / close-up steps (rule-derived from channel graph).
6. Slot layer (`BuildPlanarSlots`): one small integer program per variant, no binaries, minimising steps (weight 1) with width tie-break 1e-4; per channel interval the ordered tracks (lines, copies for doubled lines); composer reads lane offsets, fray gaps, join out gaps from it.
7. Composer (`MappedComposer`, planar_components.cpp, built on `planar_model` `PlanarModel::Model`): builds in depth order sites then channels (bundles, straight cores); fray, join, doubled core, n-lane bundles as required pieces; decks wired after; flow-share doubling (`line_share`: full lane doubled over a crossing, half-share sideline single).
8. Solve: one `JunctionMilp` per variant (all integer, half-tile), 5 s per solve limit, 15 s per variant deadline; lazy clash separation (one pair per round, or batched by `PlanarLineSides` sides); corridor ladders (`LadderRungs`, at most 2 passes) from a rough solve; area pass by sequential linearisation. Objective: track length plus one linearisation of outline area, `PLANAR_PULL` 0.001 coordinate pull NW; anchor window removes sliding symmetry (D348).
9. Back end (`planar_plan`): flat seed cached per topology text; `RenderPlanarRoutePrograms` rasterises each `PlanarRouteProgram` through `RasterizeJunctionRoute`; terrain: program run again with terrain facts lazy, warm start; hover fixes the seed's binaries. `PlanarCheckerTopology` gives checker lane paths.
- Hand-coded in B: `SolvePlanarComponents` has a hand composition for `LL2` only (`PlanarLL2Text()`, 4 variants = S fray side x diagonal west fray; "On the test spec the solver finds LL² at 800 tiles (variant -1F1w1, 64x47)"); `PlanarLL2Text` is a fixed PSG 227 pattern whose 18 lane paths equal `ComposeJunctionHub` on the width-2 T; `PlanarHubText` maps the window's hub type to T/LL2/two-lane T. Per-hub inside the "general" composer: the T2/T3 default sites/ties tags, the candidate-step rule (T's B channel), per-hub composer helpers ("The hand Composer keeps T2-only helpers", ROADMAP:13). T2 and T3 and (since D326) every topology except one-lane T go to the general `SolvePlanarMapped`; "one-lane T is refused (it keeps the layout program)" (the older `planar_layout` program, kept for it).
- Derived: everything else from channel graph.
- (Intermediate-step paragraph) B sits between A and C: it replaced A's hand templates with a channel-graph composer and a MILP (D305-D309, D326 "any hub at scale"), then SSS replaced B's generate-and-check composer (D361).

### 1C. SSS v2 (C)
Layers (ng2 CLAUDE.md "Architecture"; D361, D362, D366-D369, D379, D388):
1. Structure (`BuildPlanarStructure(topology, spec, keep)`): enumerates variants (bits: every channel crossing over via `PlanarChannelOver`, ties, open triangles, one separator bit per mover D372); costs each without a track map (runs + runs with a copy, D371a); hands down `keep` cheapest by (cost, tag `o..t..f..m..`) with track map, sites, structures (`PlanarStructure`: separator, kind bridge/tunnel, `limit`, crossers), `side_gap`, `rules` (`PlanarRuleFields`). Oracle `CheckPlanarStructurePromise` P1-P9. T2 16384 variants exhaustive, LL2 65536, T3 by search, all under 0.2 s Release. Track map (`BuildPlanarTrackMap`): expands skeleton into every physical track (rows beside prio lanes, copies over crossings, mirrored crossings D458) as one validated planar map (V - E + F = 2) with sides per stretch. `PlanarLineSides` is the line-level (B) side table, kept.
2. Shape (`ShapePlanarStructureVariant`, `BuildPlanarShape`): one `JunctionMilp` over the lifted heading at every point (5 s limit): port headings, node rule (straight leg = trunk, others turned one eighth toward side, no cusps, D404), crossing rule (separator axis), spine rows (H14), capacity rows per span (bridge/tunnel limit), face winding, dummies for convex cells (D385); flex points (flex C default, D420) marked on diagonals. Oracle `CheckPlanarShapePromise` H1-H14. Headings only, no lengths.
3. Slots / drawing (`planar_draw`: `CheckPlanarDrawable`, pipeline `DrawPlanarPipeline`, D422): one integer program (half tiles) with points/pieces, rule fields (bays, curves D420, node gaps, node lead, end node, cross piece, structure footprint, coverage), clearance per convex cell, pairwise keep-apart (binary side per wall pair), grid parity, corridor family (D459), along mode (D460). D447/D446: stage A without keep-apart picks S-bend offers by length objective; stage B contracted carries keep-apart lazily; D449: a bend pre-program decides S-bends (not built yet; "v1's slot layer is the precedent").
4. Hub hand-off: `PlanarDrawnHub` (planar_hub, D452) points per track, nodes, ports, structures, routes; `CheckPlanarDrawnHub` oracle; `PlanarDomainInputOf` gives the domain input.
5. Heights, signals: not built for C (AUDIT-v1-v2-parity.md: "v2 has no plan-level caller ... no prio_beside, no levels").
- Judge: `CheckPlanarDomain` (planar_domain.h) judges geometry by game tile rules only (D428), independent of every solver; the rasteriser is proven equal to `RasterizeJunctionRoute` by test "planar domain: the rasteriser matches the game's" (tests/junction/planar/planar_domain.cpp:427).
- Hand-coded in C: none per hub in the layers; answer keys are pinned from saves: PSG 197 structure/shape files (tools/junction/topologies/psg197.structure.txt, .shape.txt; D389); T1/T2/T3/LL2 text topologies. Rule fields come from the spec only.

---------------------------------------------------------------------------
## 2. Numbers

### 2A. A
- Hover target: "Generation on hover is under 100 ms for both topologies (T and LL_RR x L_R)"; `[realterrainsweep]` requires it of the plan and the near search over 48 terrain fixtures (tool/CLAUDE.md; TESTS.md line 33: budget 10 s Release, 0.15 s noted for the RT1/RT2 landing). Gate flakes: hover near_max 107.9 ms under load (ROADMAP.md:86, planar side; the same target).
- Sweeps: `[topologysweep]` 44 cells checker clean, no box larger than the old seeds (TESTS.md:46); `[junctiongamesweep]` T planned 32 of 48, LL_RR planned 20 of 48, 0 red tiles (TESTS.md:25); `[hoverplan]` 3.3 s Debug (TESTS.md:47); `[sweep]` hidden tag.
- Size: width-2 T "places on flat ground with its real blocks (size 86 x 63)" = 5418 bbox tiles (placement/CLAUDE.md). Objective: least solution, i.e. floor lengths, no area score.
- Not verified: per-lane-count T sizes for A; not re-run in this recon.

### 2B. B (planar CLAUDE.md "Component solver", STATUS)
| hub | result | time | source |
|---|---|---|---|
| T3 | 1941.25 (o0t1f1s1, 129x45); o0t1f1s0 1960.25; hand T3 earlier 1970 limit | 0.64 s wall full run (earlier note 3.3 s, "T3 3.3 s from 4.1" before pull) | CLAUDE.md Component solver; test limit planar_components.cpp:106 `CheckSolves(T(3), 1970)` |
| T2 | 906.25 (89x34, o0t2f1s1) in CLAUDE.md; STATUS later 862.25 (o1t3f1s0, 94x29) | 0.85 s (1.8 s anchored); test limit 963, 32 variants solved | CLAUDE.md; STATUS.md:186,196; planar_components.cpp:101 |
| LL2 | mapped 1347.25 (o17t0f1s0, 48 solves); hand composition 800 (64x47, variant -1F1w1); test limits 807 (hand) and 1347.25 (mapped, 1..199 solved) | 74 s (anchored, from 92 s) | CLAUDE.md; planar_components.cpp:77,111 |
| T1 (one lane) | 325.25 breaks bay and adjacent-split rules ("known gap, hidden test"); seed 616 s on the `planar_layout` program; the layout program's lazy rounds "11 to 70 s per lazy round, over 10 rounds" | 616 s (open defect) | CLAUDE.md; ROADMAP.md:87; TESTS.md:41 |
- Test timings: T2 mapped lands in box test 2.2-2.3 s Release; three slow cases moved to `[slow]` at 12.4 s (T2 hand), 12.1 s, 10.0 s (LL2 hand) (TESTS.md:58). `tools/junction/pv` runs variants singly: T2 1.3 s Release (TESTS.md:8), timeouts T2 20 s, T3 20 s, LL2 60 s (tools/junction/pv:TIMEOUT).
- Per-solve limits: 5 s per solve, 15 s per variant deadline.
- Objective: track length plus linearised outline area (D315, D329, D336 "outline are clear winners"); measured: width+height adds nothing over track length on T3; area step trims empty corners; the full area loop equals linearisation on T3; `PLANAR_PULL` 0.001: 20-25% faster, finds T3 1941.25 that the run without missed by a tile; anchor T2 2.8 -> 1.8 s, T3 4.1 -> 3.3 s, LL2 92 -> 74 s. Wasted sideways travel: T3 main line 1933.25 138x39 wasted 92, T2 862.25 wasted 36 (STATUS.md:189). Slot layer objectives: T2 `o1t3f1s0` 4.0041, LL2 `o17t0f1s0` 3.0054, a few ms.

### 2C. C
- Structure: all variants of T2, T3, LL2 under 0.2 s Release (CLAUDE.md Structure).
- Shape (keep 16): T2 about 0.72 s, T3 3.9 s, LL2 3.1 s Release (CLAUDE.md Shape layer); earlier LL2 12 s over 16 variants after D404 (STATUS.md:112).
- Drawing: T1 drawn legal end to end: "stage A (incumbent), turns hold, stage B 13 of 280 pairs in 35 ms, polish 136 -> 130, judge accept, domain accept with 0 breaks" (wip/HANDOVER-2026-11-25.md:11); HiGHS stage A on T1 had an incumbent at 1.4 s (:12); T1 "with the turn rules in 0.06 s" CP-SAT, "T2 stage A with the turn rules in 4.6 s on the grid; T2 with EVERY keep-apart row in 92 s" (:14).
- T2 under D456-D459 (wip/probes/d449/t2-d458.md in ng2): HiGHS stage A finds nothing in 15 s (Error at 15042 ms even with incumbent knob); CP-SAT on the curves-off export (1766 vars, 2478 rows, all integer) OPTIMAL 1.74 s; with those lengths locked stage A Optimal 36-48 ms but stage B round 1 Infeasible (53 pairs, 212 binaries, ~147 ms); pair filter 53 -> 1 pair (`sep 33 2 4`, 4047 ms, 54 solves); all-pairs CP-SAT export 14150 vars, 55110 rows UNKNOWN at 120 s; P6 CP-SAT lazy loop reached a WITNESS (0 of 3096 pairs failing, 255 pairs with rows) only at round 26 (7-36 s per round); with curves on that set still fails stage B round 1 (pair `sep 9 0 13`). LP-filter set fails before any keep-apart row (`footprint F1 3 4 2`). So: no T2 drawing reaches the domain judge in the default path as of that file.
- T3/LL2 drawing: no completed drawing recorded; STATUS.md:72 certificates: T3 faces 75, 29, 66 exactly drawable (0.3-3 s, pipeline heuristics at fault); LL2 face 33 provably Infeasible (0.8 s), minimal core D1#11 vs B1#1, B1#4 plus `corridor F1"0 close`.
- PSG 197 (answer key hub): drawing LP "slack 0 by round 2, box 82x93, length 1951 (V 2207)" (STATUS.md:94); whole-hub exact: no incumbent in 15 s.
- Objective: stage A feasibility only (D446, D447); compactness by D445 one-axis-at-a-time polish; length and box prices in the pipeline (insert 0.05/0.1, box 0.01). Engineer on T1: "The curve length stuff seems good now, but the compactness is questionable" (STATUS.md:235). T1 polish 136 -> 130 half-tile length units (not verified units).
- Budget: seed target < 5 s (D297) and hover < 100 ms (memory hover-target); C measured only stage times above, no end-to-end seed time for T2/T3/LL2.

---------------------------------------------------------------------------
## 3. Known defects and why the next approach started

### 3A. A (pre-SSS)
- Over-prescribed geometry: D296 "Width-2 geometry is over-prescribed ... 'we're being overly prescriptive'": D292-D295 not confirmed, direction "generalise rather than add geometry rules".
- Order, occupancy: placement never read the band lines; occupancy orders pairs by a stable sort on flat columns, so row-edge vs band-edge pieces were not ordered off item rows (wip/recon-band-lines-occupancy.md). D305 (after "H1c4's sixth stop") says nodes are every place a line leaves, joins or crosses a channel, replacing D304 (held).
- Not generic: "not ruled" refusals (N-side branch ports, through edge off its row, bundle wider than 2, equal lane counts only in bundle graph); "the width-2 T ... joint loop on it waits for stretching blocks"; "the square crossing of a bundle waits for stretching blocks" (placement/CLAUDE.md).
- Checker stays in acceptance "because the template does not yet cover every rule" (src/junction/CLAUDE.md).
- LL2 and the T came from hand block tables (227 join, exit block); D308 (coop rules below the topology, replace with planarized line/route graph), D326 "Until we can solve for any hub at scale, we've not got the architecture right; per-hub compositions: why was that ever something we did".
- Dead ends before the switch: DESIGN-width R9-R13, G4 static rows "stopped by D276" (ROADMAP.md:161-165).

### 3B. B (planar mapped composer)
- One-lane T: 325.25 solution breaks bay and adjacent-split rules; seed 616 s; refused by component solver (CLAUDE.md; ROADMAP:87; TESTS:41).
- Rules the composer could not express (both are probe results in planar CLAUDE.md "Layout program"): `PLANAR_FACESEPS` makes every T2/T3/LL2 variant infeasible (IIS: a split node's incoming line and its two legs get opposite sides of the leg's far segment, because a side holds only where lines run alongside); `PLANAR_ALIGN` (prio route between opposite sides on the same row) infeasible on every T2/T3 variant and on all but one of 201 LL2 variants, "because no step brings a fanned main line back" (D337 ruled "Leave offset", D316).
- Optional bends in the model: D340 "the whole point of this was to remove the concept of optional bends"; slot probes failed; "a slot step on a diagonal piece is refused" (Slot layer); only fork legs remain optional.
- Generate-and-check: lazy clash loop with direction trials, ladders, and "infeasible with topology sides" outcomes; D361 declares generate-and-check "a defect to design out". `PlanarLL2Text` hand composition (LL2) kept as the only composition; D326 hand compositions are "fixtures, never the product". D360: no fork/merge straight-leg mask solves LL2 (every IIS a stack of G2 fork/merge diagonals against ports 2 apart).
- Side table is line-level: "a track crossing both legs of a split lies on both sides of the trunk (PSG 197: D3 and C2c)" so the B side table disagrees with PSG 197 (CLAUDE.md Line sides).
- Prio never over (doubling not built into layout program); D342, D347/D347b relaxed later in C. Copy switches, doubled bridges: B had them (see section 4).
- Stability: solves time out at 15 s per variant; "a solve that reaches it counts as infeasible" (MILP incumbent issue, memory milp-incumbent).
- Why C started: D361 (engineer: "valid by construction" layers), D362 SSS, D340 shape layer, D350 slot layer; holistic review `wip/REVIEW-architecture-holistic.md` (named in D361; not read here).

### 3C. C (SSS v2)
- T2 end to end blocked at prio joins (HANDOVER-2026-11-25 title); S-bends are "the hard part"; D449: "Slots cannot choose optional S-bends inside one integer program" (flank-lock.md: lazy loop, flank rows, neighbourhood freeing, conflict-driven proposals, partial node rule all fail on T2).
- T2 under D456: stage B infeasible on every locked set; existence with every pair unknown at 120 s; P6 witness only after 26 CP-SAT rounds (t2-d458.md).
- HiGHS solve times: T2 stage A no incumbent at 15 s; CP-SAT licence (D461, not a constraint now); D450 benchmark of MIT CP solvers.
- Compactness: T1 questionable (STATUS:235); high box prices limited by D422's split (STATUS:82); D388 bundle-level direction.
- AUDIT-v1-v2-parity.md (2026-11-25, in main checkout wip/): v2 MISSING: row never drivable (D455 chevron joins, being built), exact coverage env-only, curve spacing and bays off in the probe (X1Fixture:152), grid parity not default, prio element room (v1 planar_layout.cpp:921-957 family 14), doubled bridges (equal arms, copy deck stagger, signal tile before copy split: v1 planar_components.cpp:189-203, 317-324, 479-502), cusp/flank/node_flank env-only, copy switches absent from signal input, no split/merge inside prio coverage row (D108), prio_beside + levels, tunnels (no levels), terrain facts, outline-area objective (D354). IN: ports/side gaps, node legs (stage B), node lead, deck gaps, end runs + end_node, span limit, cross_piece, keep-apart, box, node/port tiles, event order. DROPPED by ruling: prio never over (D347/D347b, D342), port line >= 2 tiles (D412, D434).
- Whole-hub exact is too slow (PSG 197 11k pairs, 45k binaries; STATUS:58); LL2 face 33 undrawable until the promise gains a clause (STATUS:50, 72).
- Domain suite exact on X1, X2, M1, K1, L1, Y1, P1, BR1 (D435); the oracle pins hand cases from the game.

---------------------------------------------------------------------------
## 4. Concept map: A / B / C

"Domain-checked" = a rule with pinned cases in the domain suite (`[planardomain]`, `[planardomainexact]`, `[planardraw]`; tests/junction/planar/planar_domain*.cpp) or an oracle clause (P/H) that C tests. Not verified per row unless stated; the domain suite covers game tile rules (connections, diamond, ends, deck middles, head), not coop rules.

| mechanism | A | B | C | domain-suite rule in C |
|---|---|---|---|---|
| topology, ports, lane paths | `JunctionTopology`, `ExpandJunctionLanes`, lane_paths with prio | `PlanarTopology`, lines/routes/ports, event order | same `PlanarTopology` | n/a (data) |
| grouping tracks | graph edges (one per lane/bundle), width 1 or 2 only | channels (`BuildPlanarChannels`) | channels + track map | oracle P1-P9 |
| sites (split/merge, fray/join) | graph nodes with shapes from programs | sites with straight channel, side, ties | sites + node rule in Shape (straight leg equals trunk) | node rule proof D400 `[planarnode]`; domain "node", "diamond" |
| crossing over/under | fixed rules per class (joiner over turning leg...) | prio over non-prio else open bit; triangle flips | Structure bits (`PlanarChannelOver`), separator per mover; D364 per joiner | domain "under", "decks", "missing crossing" |
| bridge/tunnel grouping, limit | series cut at 2 crossers; bridge cut search; tunnel single only | decks per crossing group, "within the longest middle" | `PlanarStructure` limit, Shape capacity rows (H6, H11), tunnels in Structure | domain "span", "head" |
| doubled main-line bridge | parallelogram, copy 1 heads one tile upstream, one signal tile (D80) | doubled core by flow share `line_share`, copy decks in `planar_model`; D351 parallelogram | copies are tracks in track map (mirrored crossings D458); drawing has no stagger rows | none (stagger, equal arms missing: audit item 7) |
| prio row / element | row stack, prio rows, approach items; element by signal pass | prio row tracks (`Row` unused in slots), element room family 14 | rows beside lanes in track map; coverage row (probe, off by default D384) | none for coverage; D455 never drivable pending |
| lane fan / port close-up (gap steps) | Fan tokens, `port_step` | `planar_gaps` (fan, close-up, candidate step) | port-side frames in Slots (D363: gap steps withdrawn); `side_gap` rows | P8 side_gap |
| S-bends / sideways steps | SBend tokens sized by placement | slot layer (`planar_slots`) integer, composer takes S-bends | flex points (D383, D397), D449 bend pre-program not built | domain "grid", "turn"; flank rows D448 off |
| heading / angle choice | tokens, 45-degree pieces by programs | channel paths (`BuildPlanarPaths`) | Shape heading ILP (lifted headings) | oracle H1-H14; domain "turn" |
| lengths / geometry solve | difference constraints (`JunctionPositions`) least solution | one MILP per variant (half tiles) with lazy clash loop, ladders | one integer drawing program (stages A/B, CP-SAT chooser question) | `[planardraw]` 52 cases |
| non-overlap | R7 occupancy rows (pairs by flat column order) | lazy clashes + ladders from face sides | keep-apart by construction (D381): pairwise binaries, corridor family | domain "connection"; keep-apart read-out judged on fixed points (D428) |
| curve spacing | `JunctionCurveGaps` floors (R8) | curve rows over required pieces (`PieceCurves`) | curves rows on by default (D420); S-bend middles count as travel (breach possible) | not a domain rule (a spec rule) |
| bay (2 TL before merge) | R5 | bay rows (2 TL + 4) | bays rows (D420); off in the Slots probe | none |
| node legs/lead, end node, cross piece | R6 plain axis tile; head floors | adjacent-split rules (breaks on T1) | `node_gaps`, `node_lead`, `end_node`, `cross_piece` (D435 on) | domain suite exact on X1, X2, M1, K1, L1, Y1, P1, BR1 |
| curve of bridge head vs ground end | not modelled | not modelled | pin 5b: deck-side end of a head is no ground end (D456, 01f112979f) | yes |
| grid (tile edge midpoints, parity) | tile grid via column/row integers | integer half-tile model with parity aux | `grid`, `grid_conditional` (probe), D382 | domain "grid" |
| objective | least solution; push toward feature | length + area (D315) + pull | feasibility (stage A); polish length (D445); box/insert prices | none |
| variant choice | cut search + joint fallback | enumerated bits, local search above 6 | Structure ranks by cheap score (D371a), keep top N; Shape ranks | n/a |
| terrain / heights | level solve (`junction_levels`) | terrain facts lazy in same MILP | Heights layer D365, not built | none |
| signals | `signals/` pass on rendered routes | native `JunctionSignalNetwork` | not wired (no plan-level caller) | none |
| hover | `PlanJunctionHoverNear` | `PlanPlanarHoverNear` (fixes seed binaries) | none | none |
| hand-coded hub | exit block, 227 join block tables | LL2 composition, T2-only helpers | answer keys PSG 197 as files | n/a |

---------------------------------------------------------------------------
## 5. Bridge: can A's and B's output be judged by today's `CheckPlanarDomain`?

Domain input type: `PlanarDomainInput` {`tracks` (per track a list of half-tile points, edge midpoints, travel order), `nodes` ({at, in tracks, out tracks}), `structures` ({separator, first, last, tunnel, limit, crossers, ends})}; `CheckPlanarDomain(input)` returns `PlanarDomainVerdict` with breaks per rule ("grid", "turn", "connection", "node", "diamond", "head", "decks", "under", "missing crossing", "span"). Source: ng2 planar_domain.h:55-78. Existing producer: `PlanarDomainInputOf(const PlanarShapeVariant &v, points, notes)` (planar_domain_map.h:27) and `PlanarDomainInputOfHub` (planar_hub); both start from C's track map, not from route programs.

Proof the rasteriser matches: test "planar domain: the rasteriser matches the game's" (planar_domain.cpp:427) runs `RasterizeJunctionRoute` (junction_route.h:31 `JunctionRoute`{start, start_trackdir, moves}; `JunctionRaster`{pieces, bridges, error}) and compares to `RasterPlanarDomainTrack`; helper `TrackFromRoute` (planar_domain.cpp ~46-60) converts a `JunctionRoute` to domain points: first piece's entry edge midpoint, then each piece's exit midpoint (`EdgeMidpoint` half-tile frame: NE (2x, 2y+1), SW (2x+2, 2y+1), NW (2x+1, 2y), SE (2x+1, 2y+2)). That is the nearest existing function for any adapter.

A's output:
- Type: `JunctionLayout` (junction_layout.h:60; track pieces, signals, size) is output only; per-route pieces are `JunctionTrackRoute` {path, copy, pieces (`JunctionPiece`: tile, trackdir, units, on_bridge), tags (`JunctionTrackPieceTag`: edge, token, run, copy)} (junction_route.h:69); bridges as `JunctionBridge` list in `JunctionRaster`; nodes in `JunctionTrackGraph` (nodes with kind, slot), signal network `JunctionSignalNetwork` (switches, merge nodes per route).
- No adapter exists (not found: grep for PlanarDomainInput in src/junction outside planar/ not run here; the domain is only built from `PlanarShapeVariant`/hub in the files above; not verified exhaustively).
- What an adapter would need: (1) per route piece list -> points (use `TrackFromRoute`'s construction, but from `JunctionPiece` lists: entry midpoint of the first, exit midpoint of each; bridge pieces (`on_bridge`) must be split into head, middle and head pieces: the domain wants separator tracks with `structures.first/last/ends`). (2) one domain track per graph edge/copy with consecutive routes' shared prefixes de-duplicated (A routes are per lane path from port to port, so routes overlap on shared edges; the domain needs one track per physical stretch with `nodes` listing in/out tracks). `JunctionTrackPieceTag.edge` and `copy` identify stretches; node tiles are given by `JunctionTrackGraph` nodes or `JunctionSignalNetwork` switches/merges. (3) structures: `JunctionBridge` {span, crossers, tunnel} -> `PlanarDomainStructure` {separator, first, last, crossers, limit, ends}; A's doubled bridges are two bridges; tunnels `ends` 2. (4) block-composed hubs (`ComposeJunctionHub`) already return one route per path plus copy tags in a common frame.
- Rules likely to differ: the domain's end-piece, deck-gap and head pins (D456, D435) vs A's `JunctionTracksMayShareTile`, `JunctionUnrelatedTileGap`; A's checker `CheckJunctionRules` is the oracle A is judged by today (not the domain).

B's output:
- Type: `PlanarComponentLayout` (routes: `PlanarRouteProgram` per route path and copy choice: pieces (tile, heading 0 N .. 7 NW, Ground/Head/Middle, line index in path) plus copy switches; `variant`, `area`, `width`, `height`, `track`, `wasted`, `empty`, `rungs`, `micros`), after `SolvePlanarMapped`. `RenderPlanarRoutePrograms` already converts each program to `JunctionTrackRoute`s via `RasterizeJunctionRoute` (planar_plan.cpp; "turn +1 is the rasteriser's L, a deck is B<middle>").
- Closest feed to the domain: the pieces carry the line index per path, so tracks per topology line can be rebuilt by concatenating pieces by line; Head/Middle markers give the structure; nodes from topology split/merge nodes (CLAUDE.md: "a split's node tile the first piece of its out leg, a merge's the last piece of its in leg"). No such adapter exists in the sources read; not verified.
- B's `PlanarGeometryClashes(topology, geometry)` is B's own clash oracle (family 13 tile scan), not the domain.
- Mismatch: B's solution geometry is the solver's points per strand (half-tile vertices), but its stored output is pieces, not points; points can be recovered through the same entry/exit midpoint rule.

Tests that run A and B today:
- B on T2/T3/LL2: `[planarcomponents][slow]` tests "T2 from its topology text (mapped) at the probe's area" (area <= 963, 32 variants solved), "T3 ... at the hand composition's area" (<= 1970), "LL² ... mapped ..." (<= 1347.25, 1..199 solved) and the hand LL² (<= 807); the one-lane T is hidden-tag documented broken (planar_components.cpp:77-111). Tools: `tools/junction/pv run|diff|save|check <build-dir> T2|T3|LL2` (TESTS.md:8; timeouts T2 20 s, T3 20 s, LL2 60 s). Budgets in TESTS.md: `[planarcomponents]` non-slow 2.3 s Release (5 cases, budget 5 s); `[planarmodel]` 5 s; `[planargaps]` 5 s; [slow] cases not budgeted (12 s, 12 s, 10 s, 2.3 s landed). Run with `tools/junction/jt` (never hand-typed filters).
- Checker clean: older tests "planar: T2 builds from its component layout and passes the checker" ([planar][slow], TESTS.md:58) run the rule checker `CheckJunctionRules` on B's output.
- A: `[topologysweep]`, `[junctiongamesweep]`, `[realterrainsweep]`, `[hoverplan]`, `[topology]` (TESTS.md:21-47); gate only for the sweeps, not per step.
- No test found that runs A or B output through `CheckPlanarDomain` (grep in tests/junction/planar/planar_domain*.cpp shows `RasterizeJunctionRoute` used only for hand cases and the rasteriser-equality test). Not verified beyond that grep.

Sources not read in full (no claims drawn beyond headings): docs/junction-tool/wip/REVIEW-architecture-holistic.md, wip/HANDOVER-2026-11-25.md (read lines 1-33 only), wip/probes/slots/*.md, DESIGN-sss-v2.md, PLAN-sss-v2.md.
