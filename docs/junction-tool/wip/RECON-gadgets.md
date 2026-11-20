# RECON-gadgets: what exists for the E3 gadget catalogue (read-only recon, 2026-11-21)

Facts only. Paths are under `src/junction/planar/` unless stated. Units: half tiles (ht); an axis piece is 2 ht, a diagonal half piece 1 ht. Headings: `h` is the bundle's heading, `Turn(h,+1)` is one eighth turn right of travel (`Right(h)` is the sideways unit). `AddSeg(name, heading, lo, hi, cost, optional)` (planar_model.h:381) makes a segment whose length is a solver variable in [lo, hi]; `lo = hi` is fixed. Axis lengths are forced even (parity). No gadget code exists as such (grep "gadget" in src: none); today's composer is `MappedComposer` (planar_components.cpp:750-1932), driven per site by `MappedComposer::Build` (:1374). The older `Composer` (:140-692, `SolvePlanarComponents` :2985) is the hand 2-lane T/LL2 composition, kept for oracles (D330).

## 1. Primitives as built today

Dispatch (Build, :1374-1703): a site is composed as an n-lane `Fray`/`Join` when its kind is Fray/Join and it has exactly two `others` channels; otherwise per node: a `Fork` for each split node, a one-lane `Join` for each merge node (`PlanarSitePerNode` :693: LaneSplit, LaneMerge, or `others.size() != 2`). Site kinds are in planar_channels.h:38-44 (Fray = legs to different out ports; LaneSplit = same out ports; Join = merges from different in ports; LaneMerge = same in ports). Sites are built in depth order; channels after the site that starts them; crossings wired at the end (:1706-1752).

Segments marked **fixed** have lo = hi; **lb** is a lower bound with the solver free above it; **var** is lo 0 (or free) and solver-sized.

### 1.1 Fork (one-lane lane split; used for LaneSplit sites, per node) :907
| Piece | Heading | Length | Notes |
|---|---|---|---|
| left leg | h-1 (diagonal) | optional, lb 2, may vanish (binary), cost 1 | `AddSeg(..., opt=true)` |
| right leg | h+1 (diagonal) | optional, lb 2, may vanish | |
Both start at the in line's end point; `Node({in,left,right})` (strands may touch). One row: the legs' ends are at least 2 ht (one lane) apart across travel ("fork gap"). Lanes: 1 in, 2 out. Ports: the legs' ends; the next piece is each leg's channel bundle (`Bundle`/core). Which leg runs straight is the solver's (a leg's diagonal may vanish). Fails if the node has a straight leg inside a mixed site (":1654 build_nodes": "a split with a straight leg in a mixed site is not composed yet"). Implements D330 (split = fray of one lane), DESIGN-components C2 (no signal-room rows before or after: none in the code).

### 1.2 n-lane fray (Fray site, `build_fray` :1455, `Fray` :968; lanes far..near, far = away from diverging side sigma)
For each in lane x (all lanes):
| Piece | Heading | Length |
|---|---|---|
| port stub | h | lb 4 (STUB), var above |
| prio only: climb | h-1 diagonal | lb 2 required, cost 1 (not optional in code; D355 asks it optional) |
| fan stretch | h | var, lo 0, cost 1.5 (FANW; the "spread" cost) |
Then per lane k a node `{in_k, S_k, V_k}` at x_k's end; S = straight leg, V = diverging leg.
- Split-point gaps: across-travel distance between consecutive lanes' split points is **exact** `2 * (slot(post, k) - slot(post, k-1))` (from `PlanarSlots`, :1470-1474).
- `S_0`: h, lb 2. `V_k`: Turn(h, sigma) diagonal, lb 2 (all k).
- `S_j`, j >= 1, cross every farther `V_i` (i < j), the order swap C8 inside the fray:
  - non-prio: S_j: h lb 2, then a deck segment h lb 2 with `Cross(d0, V, head 4)` for each farther V; `Deck(d0, xs, ., ., n_min 2, tail_min 2)`; heads are 4 ht along the deck from the crossing (so the near head ends >= 4 before and the far head starts >= 4 after the crossing; n >= 2, tail >= 2).
  - prio: `Double(S_j, h, -sigma, "'", first 4)` (the parallelogram, see 1.7), then S_j h lb 2, `PairDecks` over the farther Vs.
- Ports: V_k and S_k end where the next channel's `Bundle` (:1109) takes them over; in lanes meet the in port stub (`PlanarPortIn` :735, start pinned to the box edge).
- Rules: D311 (fray composed from lane splits plus swaps), D330, D355 (split gaps are lower bounds: not yet in code; the gaps are exact equalities today), D349/D384 do not apply to frays. Fan separation rows ("fan rule" :1757): a lane right of another keeps its stub, fan and what follows right of and ahead of the other's, per in port.

### 1.3 One-lane join (merge node; `Join` :1010 with n = 1, called from `build_nodes` :1654 with `lo0 = 2`)
| Piece | Heading | Length |
|---|---|---|
| straight S | h | lb 2 (prio: lb 2P+2 where P = prio coverage in tiles) |
| joiner J, run | h | lb 2 |
| joiner landing J, diagonal | h - sigma (toward S) | lb `lo0` (2 per-node; 6 in the n-lane site join); free above (hi = lo only when n == 2) |
| node | `Tie(End(J), End(S))`: the two end at one point; `Node({S,J,O})` |
| out O | h | lb 2 (cost 1.5 FANW); optional exit step Turn(h, exit_step) lb 2 (step from `PlanarMergeExitStep` / `PlanarInnerMergeExitStep`, planar_gaps.h:41-44); then h lb 4 (STUB) |
Prio (`prio = LinePrio(straight)`): S is lb 2P+2 long and a **prio row** `{S, merge point, h, -sigma}` is recorded (see 1.5). Lanes: 2 in, 1 out. Ports: S and J come from their channels' bundle ends (headings must equal h or the build fails, :1639, :1675); O hands to the out channel or the out port. The tie sides come from `ResolvePlanarSites`; a one-lane merge with unresolved straight leg takes the variant's tie bit (`variant.ties`, :1665). Rule: D75 (the prio join's trailing 45-degree pair is NOT in the composer: the composer lays the joiner's diagonal onto S and the prio row as an unused strand; the pair h1/h2 of unused pieces is placed by the Signals pass, signals/junction_signal_pass.cpp `PlacePrio` :310, `GeometryOfPrio` :285, which puts the pair on the side opposite the joiner's approach (D75, D87)), D374 (no heading rule at prio merges; checker's "axis piece" leftover in junction_rules.cpp, see CLAUDE.md).

### 1.4 n-lane join (Join site, `build_join` :1618, `Join` :1010 n >= 2; lanes far..near, far = away from joining side sigma)
- `S_0`: h, lb 2 (prio lb 2P+2). Each `J_k`: h lb 2, then diagonal h-sigma, lb 6 (k = 0: `lo0`), lb 4 for prio k > 0; **fixed** (hi = lo) only when n == 2.
- `Tie(End(J_0), End(S_0))`. Lane j >= 1: S_j crosses the nearer joiners J_k (k < j):
  - non-prio: S_j deck segment h lb 2, `Cross(d0, J_k, head 4)` for each k < j, `Deck(..., n_min 2, tail_min 4)`.
  - prio: for each joiner crossed before this lane's coverage, one `Double` (copy toward sigma, first 2) plus h lb 2 and a **fixed** 2 ht step back (-sigma), joiners crossed P tiles apart (D322); then a last `Double(S_j,..., "''")`, h lb 2P+2, `PairDecks` over J_{j-1}. Each joiner then ties to its S end.
- Out lanes: node `{S_k, J_k, O_k}`; prio: row for every S_k, and for j >= 1 O_j: h lb 2 then a step Turn(h,-sigma) with lo 2, hi 2j (a staggered walk-out of the rows).
- Exit: each O: h lb 2, step (lb 2) when `exit_step != 0`, h lb 4 (STUB). Exit step to the left with n > 1 fails (":1017 an exit step to the left is not composed"). Rows: O lanes keep an exact across-travel gap `2 * (slot(pre, k+1) - slot(pre, k))`; steps ordered by lane so they do not cross (:1066-1074).
- Rules: D311 (join from lane merges plus prio row plus crossing), D330 (a site with more than two inputs is a chain of two-input merges in the topology's order), D349 (rows are lines of their lanes), D322.

### 1.5 Prio row (C5), `FinishPlanarModel` :1219 "prio rows" (:1313)
For each `M.prio_rows` entry: an extra strand `prio:<S>` starting at the merge point minus 2P ht along h and offset one lane (2 ht) toward `side = -sigma`; one segment h with **fixed** length 2P ht (`AddSeg(nm, h, 2P, 2P, 0)`), cost 0; kept inside the box, `Tie`d to nothing else, so it is "unrelated to all" and any clash rule applies to it as to a track (a tile shared on opposite diagonals is the only legal overlap, see section 2). Fixed length: D384 (exactly the coverage, planar_components.cpp matches the equality). Lanes: +1 beside the prio lane. The row does **not** bend with its lane in the composer (D349/D384's "row follows its lane through bends" is not built; PlanarSlotKind::Row exists in planar_slots.h:19 only as a slot track).
No-node/no-bend-on-fixed-track rule (DESIGN-blocks R9) is not a row in this composer; the prio-reach judge is `CheckPrioReach` (section 3).

### 1.6 Bundle run and slot steps (C1)
`Bundle` (:1109) lays, per channel heading piece and per lane, `AddSeg(l, h, 0, ., cost)` (var length, parallel offset); lanes keep offsets by rows ("bundle offset": each lane at least `2 * slot distance` from the lane before, both ends of the piece). `SlotBend` (:894): between pieces with different slot offsets, a lane inserts `AddSeg(l, h, 0)` then Turn(h, +-1) of **fixed** `2 * |delta|` ht (an S-bend of whole lanes); fails on a diagonal heading ("a slot step on a diagonal piece is not composed").
`Flex` (:1082) for straight cores: per lane an optional-by-length jog (Turn(h, jog) diagonal, lb 2, all lanes equal length, only when a candidate step is chosen: `PlanarCandidateStep`, planar_gaps.h:29), with last-piece offset rows. This is not D397's flex point (that lives in the Shape layer, planar_shape_layer.cpp, not in the composer).

### 1.7 Doubling (C6): `Double` :937 and the bundle form :1109
`Double(l, h, sg, suffix, first)`: lane seg h lb `first`; starts a copy strand (`StartCopy` :927, `Node({lane, copy})`); lane deck d0 h lb 2; lane step Turn(h, sg) **fixed** 2 ht; copy step Turn(h, sg) **fixed** 2 ht; copy deck d1 h lb 2; `Tie(End(copy), End(lane))` (the lane ends on its copy's line; "parallelogram", no gap line); `M.copies.push_back({copy, lane, d0.i, d0.i+2})` so the route program offers the lane or its copy (ExportRoutes :2363 emits one program per copy choice, <= 256).
`PairDecks` (:952): both decks cross the same crossers with `Cross(., ., head 4)`; `Deck(d0, ., n_exact 4, tail_exact 0)`, `Deck(d1, ., n_exact 0, tail_exact 4)`; equal bridges `f0-n0 == f1-n1`; copy 1's near head one tile (2 ht) upstream of copy 0's (D118, D145). In the bundle form each lane then runs lb 2, steps back Turn(h, -sg) **fixed** 2, SlotBend to the tail offset. Copies sides: the left lane copies left, the others right (:1160; cores use D312: far lane outward). Restrictions in code: a turning channel with doubled lanes only for two full-share lanes (:1488); no doubled bridge on a diagonal piece; a straight core with doubled and single lanes is not composed (:1547). Doubling applies per lane with `line_share >= 1.0` (planar_channels.h:72); half-share lanes cross single (D299).

### 1.8 Crossing with its deck, straight core and under crossings (C7)
- Core (`build_core` :1508): a straight (single-heading) channel: `Flex` lead, SlotBend to the interval's offsets, then per lane either a single deck segment (h lb 2) or `Double` plus step-back, a run, with decks wired later (`wire_decks` :1560): per over crossing in travel order `Cross(d0, under_v, head 3 if doubled else 4)`; `Deck(d0, xs)`; consecutive decks of a lane at least 2 ht apart (`n0 - prev_f0 >= 2`); with a copy, first copy-0 deck near head **exact** 4 and last tail **exact** 0 (mirrored on copy 1), equal bridges, copy 1 one tile upstream.
- Bundled channels over crossings: `deck_groups` (:1712): per over lane and piece one deck over all under lanes of all crossings in the group (merged decks, D203(2)); doubled lanes use the 4/0 and 0/4 exact heads.
- Deck rules in the model (`Cross`, `Deck` planar_model.h:473, :498): crossing parameter b >= 1 and under.L - b >= 1; `a - n >= head` and `f - a >= head`; n, f even; global "deck max" `f - n <= 2*(maxmid+2)` ht (:1337).
- Under crossings: an under track is at ground where crossed: when a piece is both over (its deck) and under, the crossing lies outside its own deck by >= 3 ht on the side its channel's crossing order gives (`under-deck` :1741; disabled by env `PLANAR_NOUNDER`).
- Rule refs: D307 (channel crossing replaces |A| x |B| line crossings), D203(2), D156/D164.

### 1.9 Close-up, fan steps at ports, port rules, bays
- Port in: start pinned to the box edge (`PlanarPortIn` :735). Port out: end pinned to the box edge (:1219). Port order per side in reading order: exact 2 ht between lanes of one port, 4 between ports ("port order" :1262).
- Fan step: `PlanarGapStep.fan` true: a step on the in channel's fray fan, away from its neighbour across a pinned gap; close-up: `fan` false, a step on the out channel's close-up after its join, toward its neighbour (planar_gaps.h:19-26, `BuildPlanarGapSteps`). In the model only the close-up is composed: `Join`'s exit step uses `gap_steps` with `!gs.fan` (:1646). The fan step is a Turn on the fray's fan stretch via the `fan rule` rows; no separate piece.
- Bays (R5): a non-prio route runs at least `2 TL + 4` ht from its last split (or port) to each 2-in merge, as one row over the summed lengths ("bay length" :1372).
- Curves: `Composer::RouteCurves` (:634) adds the curve-gap rows for every route (DESIGN-pieces §4).
- Side rule (:1789): a joiner arriving from the side of a neighbouring in port keeps its last two segments on its own side of that port.

## 2. Tile sharing inside primitives

| Where | What allows it | Used by |
|---|---|---|
| `JunctionTracksMayShareTile(a, b)` junction_track_geometry.cpp:214 (decl .h:69) | two unrelated pieces on one tile only when `a|b` is exactly `TRACK_BIT_HORZ` or `TRACK_BIT_VERT` and `a & b` is empty: the parallel diagonal pair Upper|Lower or Left|Right, never an X|Y diamond (D182, D217) | `PiecesClash` planar_layout.cpp:109 (ground pieces only); placement/junction_placement.cpp:2063, :2092 (occupancy legality, D217); `JunctionUnrelatedTileGap` junction_track_geometry.cpp:225 |
| `PiecesClash` planar_layout.cpp:104 | same tile: clash unless kind CLASH_GROUND and may-share; edge-neighbour tiles: clash when both pieces touch the shared edge (touching rule); `CLASH_TILE` (two deck middles) is same tile only; heads (`CLASH_EXCLUSIVE`) clash with anything on the tile or touching | `PlanarGeometryClashes` |
| `KindOf` planar_layout.cpp:116 | head with anything -> EXCLUSIVE; middle with middle -> TILE; middle with ground -> no rule (a deck middle over ground is free); else GROUND | `PlanarGeometryClashes` |
| `at_shared_node` (lambda in `PlanarGeometryClashes` :1124) | pairs of pieces of two lines incident to one node where both pieces touch that node's point are exempt (this is the join half pieces D342/D374 and the fork/fray/join point) | `PlanarGeometryClashes` only |
| `Clashes` planar_components.cpp:1984 (the composer's own check, `ok_pair` :2011) | two pieces on one tile are fine when they are opposite diagonals of one tile (NW|SE or NE|SW: the D353 prio-row diagonal halves, the parallel diagonals of a bundle on one tile); pieces of strands in `Model::related` (a `Node(...)` call: fork, fray and join node members, `StartCopy`) may meet; two pieces sharing an end point of unrelated strands clash; deck middles over ground are skipped (`inside && !is_head`), heads count as track | `SolveLazy` lazy clash loop (:2142); not by the route-program renderer |
| `CheckPrio` junction_rules.cpp:1213 (G9) | an "unused track component" is the prio; D353's allowance (a prio row on the opposite diagonal half of a main-line coverage tile) is stated as a ruling and narrowed in the checker only to tiles where the prio track touches the main line piece (DECISIONS D353; the matching loosening of the checker's "axis piece" clause is a to-do under D374) | rules checker |

Names with no code hit: "trident" (D110) has no function or constant (grep "trident": none). D110's tight split/bridge entry is built by exact deck heads (`Deck(..., n_exact 4 / 0)`, section 1.7), by `Fork`/`Join` Node ties, and, in the legacy generator, by `junction_generator.cpp:214, :387, :483` (gap 0 or 1 tile). D353 diagonal halves: only the `ok_pair` allowance above and `JunctionTracksMayShareTile`'s Upper|Lower, Left|Right case. Deck middles over ground: `KindOf` (CLASH_KINDS), `Clashes` (:1999-2004), `PiecesClash` is not called for it.

## 3. Verification tools that exist

| Function | Signature (file:line) | What it does |
|---|---|---|
| `RasterizeJunctionRoute` | `JunctionRaster RasterizeJunctionRoute(const JunctionRoute &route)` junction_route.h:53 | route program (start tile, start trackdir, move string F L R B<n>) to `JunctionPiece`s (tile, trackdir, units, on_bridge) and bridges; sets `error` when invalid (a bridge without heads, etc.). Does not check clashes between routes. `RenderJunctionAscii(layout)` (:55) prints tiles. |
| `RenderPlanarRoutePrograms` | `PlanarRender RenderPlanarRoutePrograms(const PlanarTopology&, const PlanarComponentLayout&)` planar_plan.h:60 (impl planar_plan.cpp:192) | each `PlanarRouteProgram` (pieces with tile, heading, Kind Ground/Head/Middle, line index) to moves (F/L/R, a deck head to far head is `B<middle>`), through `RasterizeJunctionRoute`; fails with a message when the first piece is not an axis piece, a deck has no far head, the raster's piece count or a tile differs from the program's. Adds each raster to `out.layout`, builds the signal pass routes (prio = route.prio), `node_tiles`, `port_tiles`. Output `PlanarRender::layout` is a `JunctionLayout` (tracks and bridges only). |
| `RenderPlanarGeometry` | `PlanarRender RenderPlanarGeometry(const PlanarTopology&, const PlanarGeometry&)` planar_plan.h:57 | the same for a solved `PlanarGeometry` (chains of points). |
| `RasterPlanarLines` | `std::vector<std::vector<PlanarPiece>> RasterPlanarLines(const PlanarGeometry&)` planar_layout.h:63 | per line, its pieces (tile, bits, heading, segment, Kind), decks marked Head/Middle from `geometry.decks`. |
| `PlanarGeometryClashes` | `std::vector<PlanarClashHit> PlanarGeometryClashes(const PlanarTopology&, const PlanarGeometry&)` planar_layout.h:74 | the layout program's tile clash scan on a given geometry: every pair of pieces of different lines on one tile or on edge-neighbouring tiles, judged by `KindOf` and `PiecesClash`, pairs touching a shared node's point left out; one hit per (line, segment) pair. Used by tests: planar_shape_layer.cpp:145, :152 and planar_slots_probe.cpp:2324 (src/tests/junction/planar/). Takes a geometry, not route programs. |
| Composer clash scan | `ClashList Clashes(const Model&, const GeoSet&)` planar_components.cpp:1984 (anonymous namespace) | the composer's clash rule on a solved model (section 2); not exported (no header). |
| `CheckJunctionRules` | `std::vector<RuleViolation> CheckJunctionRules(const JunctionLayout&, const JunctionSpec&, const JunctionTopology&)` junction_rules.h:84 (impl junction_rules.cpp:1857) | runs G1 `CheckConnectivity` (exactly the expected movements, one path each, no dead ends/cycles/other exits), G3 `CheckCurveSpeed` (when `spec.curve_limit`), G2/G10 `CheckGeometry` (no X|Y level crossing, bounding box, edges free of signals/switches/heads/unused track), `CheckOrder` (G4: merge not followed by split; D110 adjacent splits), `CheckBridges` (G5), `CheckSignals` (G6), `CheckWaitingRoom` (G7), `CheckDeadlock` (G8), `CheckPrio` (G9), `CheckPrioReach` (G9b), `CheckFootprint`. `JunctionRulesClean` (:1886) is the early-exit form. Needs a signalled layout and a `JunctionTopology`; `PlanarCheckerTopology(const PlanarTopology&)` (planar_plan.h:78) makes it from a planar topology. A gadget test would need signals added first (PlanPlanarJunction does this, planar_plan.h:67). |
| `CheckPrio` | `std::vector<RuleViolation> CheckPrio(const JunctionCheckContext&)` junction_rules.h:76, impl :1213 | G9: every ML merge has exactly one prio component (unused track bits, one node per tile, joined across edges), its shape and signals (farthest signal Exit, others Combo, facing upstream, between two segments, counts per segment). |
| `CheckPrioReach` | junction_rules.h:77, impl :1688 | G9b (D107): a one-tile train anywhere in the prio coverage settles the joiner's Entry signal red under OpenTTD's real pre-signal logic. |
| `Model::Solve`/`ExplainInfeasible` | planar_model.h:540 area | solve with extra rows; IIS print for infeasible rows (a per-gadget feasibility test can call it on a model built by `MappedComposer`). |
| Other | `SolvePlanarMappedVariant` (:3176), `SolvePlanarMapped` (:3042), `ExportRoutes` (:2363), `PlanarWastedSideways` (:2958), `PlanarOccupiedTiles` (:2976) | build, solve and export route programs for a topology text (`PlanarComponentLayout`); the existing test suite for the composer is src/tests/junction/planar/planar_components.cpp. |

## 4. What PSG 197 needs (counts from text; a throwaway parser in the scratchpad classified nodes by the in-port and out-port sets reachable up and down each line)

Source: tools/junction/topologies/psg197.txt (133 lines), psg197.structure.txt (132 lines). A "line" in these texts is one track; no line is a multi-lane bundle. The text has no n-lane group nodes: every `node` statement has one or two in lines and two or one out lines. The bundle (channel) lane count per site requires `BuildPlanarChannels` (running code) and was not determined.

**Node statements: 62** (31 splits, 31 merges).
- 1 -> 2 split, legs to different out ports (frays): **8**. (Includes the 3-way splits written as two chained 1->2s.)
- 1 -> 2 split, both legs reach the same out ports (lane splits): **23**.
- 2 -> 1 merge from entries with disjoint in-port sets: **9**; from entries with partly overlapping in-port sets (a lane that already carries two movements merges with one of them or a third): **10**; from the same in-port set (lane merges): **12**. Total merges with unequal entry sets: 19.
- Any other arity: **0**.
- Prio routes: **none** (psg197.txt header: the six entry-gated merges are plain merges, wip/probes/e1/gated-merges-out.txt).
- Ports: N (2 in, 2 out), E (3 in, 3 out), S (3 in, 3 out): 16 port lines.

**Tunnels** (psg197.structure.txt): **50** `tunnel` lines (T1..T50), **18** `group` lines: 15 groups of 2 tunnels, 3 groups of 3 (G02: T2,T3,T4; G05: T8,T9,T10; G19: T33,T34,T35); 39 tunnels grouped, **11 not in a group** (T1, T7, T12, T16, T17, T22, T40, T41, T44, T49, T50). A `copy D3 over T30, T32 (left out, D392)` comment line. Group semantics: "parallel tunnels (information for Shape v2; parsed, ids checked, not stored)" (planar_structure.h:141). `straight` lines: 62.

**Crossings** (`cross` statements): 244 directed marks over 122 distinct line pairs. Pairs whose two lines have identical in-port and out-port sets, i.e. candidates for crossings between two lines of one channel (an approximation of the channel rule, which also needs equal depth and crossings): **13**: B1b x B2b, B1b x B2c, C12 x C23, D123 x D23e, D12c x D23c, D12d x D12f, E1 x E2, E12 x E3b, E1b x E2, E1b x E3, E1b x E3b, E1c x E2b, E2 x E2b. Whether these are in one channel was not verified against `PlanarChannels`.

**The T family and LL2** (texts: `PlanarTText(lanes)` planar_topology.cpp:546, `PlanarLL2Text()` planar_plan.cpp:438; group statements expand to one node per in lane, planar_topology.cpp:176-182). No `cross`, `tunnel` or `group` statements in them (crossings are derived).
| Topology | Nodes (after expansion) | Fray splits (different ports) | Lane splits | Merges, different entries | Merges, overlapping entries | Other arity | Prio |
|---|---|---|---|---|---|---|---|
| T1 (`PlanarTText(1)`) | 6 | 3 (1 lane each) | 0 | 3 | 0 | 0 | B1, E1 |
| T2 (`PlanarTText(2)`) | 12 | 6 (3 sites of a 2-lane fray) | 0 | 6 (3 sites of a 2-lane join) | 0 | 0 | B1,B2,E1,E2 |
| T3 (`PlanarTText(3)`) | 18 | 9 (3 sites of 3 lanes) | 0 | 9 (3 sites of 3 lanes) | 0 | 0 | B1-3, E1-3 |
| LL2 (`PlanarLL2Text()`) | 24 | 6 (3 entry frays of 2 lanes: authored as `node EF1, EF2 -> E1, F1, E2, F2`) | 6 (E2, D1, C2, B1, A2, F1: one lane each) | 6 (EDj1, EDj2, CBj1, CBj2, AFj1, AFj2) | 6 (exit joins `E1, EDj1 -> EO1`, `EDj2, D2 -> EO2` and their W and S twins) | 0 | E1, F2, A1, B2, C1, D2 |
Each T site is an n-lane fray or join over single lines (n = lanes); in LL2 the 12 merge nodes and 6 lane splits are per-node in the composer (`PlanarSitePerNode`), the entry frays are 2-lane sites. LL2's exit join takes three channels (DESIGN-components §9) so it is composed per node, not as an n-lane join. Crossings in T2, T3, LL2 are derived by `PlanarTopology::Derive`; the counts were not determined without running code.

## 5. Gaps noticed

DESIGN-components catalogue entries with no code or only partial code:
- C2 lane split: catalogue says signal room before and after the S-bend (R2, R6); `Fork` has optional diagonals (lb 2) and a one-lane fork gap but no signal-room rows (:907). D110's "exactly one plain tile with a signal between splits" is a rule of the legacy generator and the rules checker only.
- C3 fray: n-lane fray exists; per-lane fan step and fray-with-lane-parting (a channel whose lanes part ways: DESIGN-components §9 ch6 = {E1, E2}) is covered only by per-node `Fork`s; a split with a straight leg in a mixed site fails (:1660). The split gaps are exact equalities in `Fray`, not lower bounds (D355 not landed); the prio climb diagonal is required, not optional (D355).
- C4 join: 3-into-1 and 2+2 cascade exist only as per-node chains of one-lane joins; the n-lane join's crossing of joiners is composed for 2 inputs only with fixed landing lengths (n == 2); a left exit step with n > 1 fails.
- C5 prio row: exists as a fixed-length unused strand beside S; the row does not bend with its lane (D349, D384 say it may); no row for a prio on a diagonal main line (D342/D353/D374); no checker-side loosening yet (D374 note).
- C6 doubling: only on axis pieces; bundled turning channels only with two full-share lanes; mixed doubled/single core fails.
- C7 crossing stack: square/S-bend forms are not distinguished in code; partial crossings (DESIGN-sss-v2 §7 question 2) are not composed (a crossing is whole channel by whole channel, `PlanarChannelCrossing`, planar_channels.h:33).
- C8 order swap: exists only inside the n-lane fray (V crossing nearer S under a deck); no stand-alone order swap and no 2x2 diagonal cross (D311 (4): later).
- C9 arm end: only the port pin and stub (`PlanarPortIn`, port order rows); no leg line-profile notation.
- Tunnels as a second layer (DESIGN-sss-v2 §7 question 4): the composer has no tunnel segment type at all (`Deck` only); PSG 197's 50 tunnels exist only in structure.txt (parsed, `group` ids "checked, not stored", planar_structure.h:141).
- D397 flex points live in planar_shape_layer.cpp, not in any gadget.

Code with no catalogue entry:
- Optional candidate step on straight cores (`Flex`, `PlanarCandidateSteps`), the exit-step and close-up (`PlanarGapStep`, `PlanarMergeExitStep`, `PlanarInnerMergeExitStep`), the bay row (R5), the "under-deck" outside-the-deck rule, the "fan rule" and "side rule" ladders and ladder faces (`LadderFace`, :721), `ExportRoutes` copy choices (<= 256 per route).
- The legacy `Composer` (:140) with hand `Fray`/`Join` (:284, :342, two-lane form, `dform`), kept as an oracle.

Not determined: the channel-graph lane counts and site kinds at PSG 197's nodes (needs `BuildPlanarChannels` run on psg197.txt; the "different ports" / "overlapping" labels above are my classification by reachable in/out port sets, not `PlanarSiteKind`); T2/T3/LL2 crossing counts; whether the 13 same-port-set crossings are inside one channel.
