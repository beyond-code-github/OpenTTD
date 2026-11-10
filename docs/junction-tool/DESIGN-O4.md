# O4: a junction from the constraints

## Revision 5: the theory (2026-11-10, D170; supersedes revision 4's core search)
Engineer: "I thought we were solving the layout in a vector map first and then projecting it on to the terrain later"; agreed: negotiation is the technique that projects the map. Every step is checked against this page, not against the last failure.

**Pipeline.** 1 map layer picks the structure -> 2 projection makes it fit the terrain (negotiation, more room, terraforming last) -> 3 checker and throughput judge confirm.

**1. The vector map (topology, no tiles).** A small graph per junction:
- Nodes: ports; splits; merges; crossings. A crossing only says that two lines cross (engineer: "it should just say 'crosses' not above or below"), with a tile envelope (where along each line, the straight approach and span range any realisation needs). Which line changes level, and how (bridge, tunnel, the D80 doubling when it is a main line, terraforming last), is projection's choice by terrain and cost; D156 (lift the main line) and D167's over/under are projection preferences, not map content.
- Edges: stretches of track between nodes, each with a track count (lanes later) and a minimum length.
- Per line: the order of its splits, merges and crossings.
- Mandatory signals (engineer: "Signals that absolutely must exist go into the map - this way we accurately represent the topology"): every signal a rule requires (the prio's signals, the signals bounding a split's and a merge's blocks, those delimiting the waiting room, the signal-legal piece before a bridge head) is a node on its edge, with kind, direction and offset in tiles. Blocks are therefore defined on the map, so the block rules (split and merge not in one block, waiting room, D113) are checked exactly there. Projection puts each on its routed piece; the signal pass only adds the optional spacing signals and no longer has to recognise a prio's geometry (replaces O4s). The T1 brief lists the mandatory signals from junction_rules.cpp.
- Prio (D158, D159; engineer: "Prio is reserved in the vector map yes, but not on the terrain"): for a merge that carries one, the stretch of the protected line before the merge is a two-track edge (the line plus its prio track, side fixed by the map), at least the coverage long, ending at the merge. Projection routes it like any edge, as one two-track token, so it may bend ("prios can bend, it's just two tracks side by side") and nothing else can take the prio track. A bent prio is a negotiation outcome and should be rare (engineer); it needs nothing special, because its signals come from the map (below).
- Placement in tile units (engineer: "topology must be defined in tile units"): each node has a tile interval along its line, each edge a minimum length in tiles, plus the branch position p. Projection picks the exact tile inside the interval.
- Straightness is a concretion matter, not topology (engineer): the map never assumes a straight main line (D160); every line, main line included, is edges like any other, and projection decides its shape.

Rules are constraints on the map, mostly lengths along edges and orders along lines: waiting room (G7) and bays (D83) as edge minimum lengths; a signal-legal piece between a split and a bridge head (D113); no split inside a coverage (D108); a split and a merge not in one block; a merge with a prio has its prio as an edge of the map (below).
Traffic on the map (engineer: "This also means that we can bring back the traffic management model"): with signals on the map its blocks are known, and edge minimum lengths give each block's shortest length, so block times at design speed from the mix and design train (D159, the throughput evaluator) are computed per structure before projection. The map layer uses them to decide which merges need a prio (D159), to reject structures that miss a throughput target, and to rank candidates; after projection the same model reruns on the real lengths as the judge (step 3).
Solved by enumeration best first: candidate structures (crossing choices x orders x coarse placements) ranked by an admissible bound (edge minimum lengths + required bridges), cheapest tried first. Small discrete search, target a few ms.

**2. Projection (negotiation).** Engineer: "we know the terrain up front, we know what rail types are valid at each stage, we know the rules for curves, everything else falls out of the map. Materialise the different movements one at a time, negotiate where necessary." So: edges are materialised one at a time by the line router, which alone handles terrain, valid pieces (slopes, bridges, tunnels) and curve rules; the map's distance constraints become each edge's start and goal windows relative to the nodes already placed; conflicts are negotiated. No separate terrain pass or constraint re-solve. Several maps may be projected and the best score kept (every map is valid; this is optimisation, not search for a valid one). Given one structure:
- Fixed by the map, not on the terrain: crossing over/under and the prio edges. Nothing is reserved on tiles before routing.
- Then every edge is routed by the line router (heights, bridges, tunnels, rule state from O4c1), all edges equal, PathFinder prices (D168), a small round limit (measured). Shared track is one edge routed once, so splits and merges are where the map put them, not emergent.
- Split and merge spots are chosen by the router, not fixed by the map: an edge leaving a split starts from a set of pieces (O4a), every piece of the trunk that the map's constraints allow (inside the node's interval, outside coverages, room for the D113 piece and the waiting room); an edge joining a merge ends on the legal pieces of the trunk downstream. The router takes the cheapest.
- Negotiation only packs. The map is MADE to fit (engineer: "it is MADE to fit either by routing or terraforming (last resort)"): when an edge finds no route or negotiation does not converge, projection gives itself room (the box grows a step) and routes again; terraforming is the last resort (with terrain, later). A map that cannot be projected at all is a missing map constraint, fixed in the map layer, not a reason to try another candidate. Never fixed by tuning prices or adding feedback.

**3. Finish and judge.** Existing pipeline: crop, normalise, signal pass, full checker (safety net), prio timing gate and throughput (D119), score: compactness then cost (D153).

**Time.** One generation < 100 ms: map search a few ms; projection about 12 edges x a few rounds x 1-8 ms; best-first stops at the first structure that projects cleanly, or after a few to compare scores.

**Carried over:** line router and rule state (O4c1), forced-pair computation and over/under enumeration (D167, now in the map layer), PathFinder prices, hug and clean-up (D166 a-b), main-line turn cost (D165), finishing pipeline, checker, throughput evaluator. **Dropped:** emergent splits and merges (switch rule at tile level), per-round masks derived from main-line routes, routing main lines first, the feedback loop.

**Steps.** T1 map layer for the flat T: types, enumeration, bound, rules as constraints; tests: the old template's structure is among the candidates, a structure breaking each rule is rejected, order follows the bound. T2 projection of one structure, then GenerateJunctionOrganic over T1's candidates: rule-clean on the test spec, the pinned spec and TL6 branch_gap 0, generation time reported against 100 ms, compactness against 243. Then: the D80 lift node, LL_RR (lanes as edges, crossings per group), terrain.

## G1: constructing the T's map (2026-11-10; engineer: "maps don't have candidates", "stick to it this time")
The map is constructed, not searched: decisions come from rules, positions from difference constraints, one layout is built, the checker runs once to confirm. A failure is a missing constraint in the construction (D170a), never a reason to build another.
Decisions for the flat single-track T (L_R), each with its rule:
- The main line B goes over both branch crossings (D156; with ml_gap rows between the main lines a branch bridge over B has no room): a D80 doubled bridge, parallelogram style (D105, D145), positions from the skeleton solver.
- N->A goes over A->S (D3d). The opposite choice is a later step (open one choice at a time).
- Skeleton stagger 0 or 1 (D118): the one with the smaller bridge span from the solver, by arithmetic.
- E-side window phase (D148a): 1 when prio_tiles is odd and >= 5, else 0.
- Crossing B's row costs nothing (B goes over), so there is no crossing-cost choice; style 0's bridge margin and w_slack do not apply.
Construction order (dependencies, each edge routed once, the earlier edges as obstacles): A line and stubs fixed; A->S (split window row 0, x 2..xI-2) and N->A (merge window from x_sA + prio_tiles + 2 + phase, D148) route first; where they cross B's rows gives the skeleton's crossing set; the skeleton places B's lanes, the B->S split and the N->B window; B->S and N->B route last. Height: the smallest that routes (the box grows until the map fits, D170a); branch position and width from the frame (MakeJunctionFrame).
Crossing nodes and their windows (2026-11-10, D172; from G2b's SoverN stop). A crossing is a node of the map with a tile envelope, and the envelope is applied as a window to every edge that passes the node, in materialisation order, relative to the crossers already placed (revision 5: "the map's distance constraints become each edge's windows relative to the nodes already placed"). Never as a cost patch on one edge.
- X_B, both branch edges crossing B (A->S and N->A each cross B's W and b rows): realised by one D80 doubled bridge, so every crossing piece on those rows must lie strictly between the lanes' heads. The largest span has nmax middle tiles and each head needs k_head (1) clear tile beside the crossings, so all crossing pieces fit in an x-interval of width D = nmax - 1 - 2 * k_head. Window for an edge: x in [hi - D, lo + D], lo and hi the smallest and largest x of the pieces already placed on the W and b rows by earlier crossers; the first crosser has no window (its own pieces are checked against D after it routes). Outside the window the W and b rows are blocked for that edge.
- X3, A->S crossing N->A: in NoverS the column fence (A->S off column xI at rows 0..g+3, ys-1, ys-2) is its envelope; in SoverN A->S passes column xI at rows g+2..ys-1 only on a bridge span, so N->A runs up the column under it.
- NoverS keeps the old router and its column fence (which already bounds X_B); the X_B window applies to both realisations once the T map's edges move to the line router.
Every part is an edge (D178, D160; orchestrator design 2026-11-11, from the engineer's "The projection should work equally for any part of the hub, on shared reusable logic"). G1 laid A's main line (row 0), B's lanes (W and b rows, D80 doubled across X_B) and the two stubs as fixed straight track; projection inherited that and grew one check per part (G3b fixed-part check, stub check, D177 plane check, the withdrawn G4b). These parts become edges of the map, projected by the one route_line like the branch edges; the per-part checks are deleted. On flat ground every one of them routes straight, so the flat T is unchanged (D173 identity test).
- Ports: the given ends (place tool: where each track crosses the box edge). Each edge that ends at a port ends with the port's straight approach (D115a: STUB straight pieces), a goal property of the request shared by every port edge (A's two ends, B's two ends, S and N), not a separately laid stub.
- Order (dependency, unchanged in spirit): A (E port to W port); A->S from A's plain pieces in its split window; N->A to A's plain pieces in its merge window (windows indexed along A's route, not by row-0 x); the X_B node from the crossers' pieces (D172 window); B across X_B as a two-track line (the D80 doubled section: the line router's multi-track lines bridge together with equal lengths, D149) between B's lane split and shift-back, from the skeleton's positions; B's single-track ends to its ports; B->S and N->B.
- Plane: each port's height is the ground at the port; D177's hub plane is where A starts.
- Routing order in general (engineer 2026-11-11: "do we need to route the lowest ones first to ensure that bridges get created properly? That may mean that we need to split movements up into their flat parts and their bridging/tunneling parts ... one line may have a tunnel and a bridge at different parts"). A routed edge is an obstacle the later ones respect; the line router takes a later edge over earlier track (bridge) or under it (tunnel). So at each crossing the edge that stays on the surface is routed first and the one that changes level second (for a bridge the higher goes second, for a tunnel the lower). The T's whole-edge order satisfies every crossing (A->S, N->A over it, B's doubled bridge over both, then B->S, N->B). In general a pair of lines may cross twice the other way round, so edges are split at their crossing nodes (D172) into segments, each pinned at its ends by the node's window; each crossing orders its two segments; the segments are routed in topological order. Who changes level at a crossing is the realisation choice (D171); where the rules do not force it (D174: the main line bridges doubled) projection tries both and keeps the cheaper. Needed from E3 and LL_RR on.
- Crossing order, plainest first (engineer 2026-11-11: "a line router cannot create a space where there isn't one, and we don't want bridges and tunnels to be longer than neccesary"; then "if you just routed the flat ground tracks first then the bridge or tunnel would know exactly how wide it needs to be"). All surface segments are routed first, then the level-changing segments (bridges, tunnels), each of which then sees exactly what it spans and takes the shortest legal crossing. D172's window keeps the crossed surface tracks together (short spans); the level-changing segment picks a crossing spot within the window where heads or portals land. Reserving a crossing's footprint before the surface segments is a fallback, added only if a measured case loses landing room.
Steps: E1 A as an edge (split/merge windows along its route), E2 port approaches as a request property (stubs deleted), E3 B as an edge with its doubled section as a two-track line. Each step deletes the matching check and keeps the flat identity.

Height from the edges (orchestrator proposal, 2026-11-10, from FINDINGS-P1; not yet a decision). The box grows one row at a time and every height below the first that fits costs a failed search (91% of the line router's T-map route time: A->S fails at g+10, B->S at g+11, for both specs). The derived first height (g+10) uses only the column fence and N->A's bridge; it misses A->S's descent after crossing column xI (to column xO heading +y under curve spacing) and the room B->S needs to merge onto A->S. Two ways to remove the failed searches, both inside revision 5: (a) tile-unit edge minimums on the map (each edge's shortest legal shape from the curve rules, so the first height is exact); (b) the S port's row as a goal window for A->S (the router picks the row, as it picks split and merge pieces), the frame height then following from it. (b) needs the column fence's rows relative to the chosen row, which a static fence cannot express. Measure after P2 before choosing.
Replaces the old sweep (about 100 candidates per height) and pass 1's reroute. Later: the other third-crossing choice, symmetric lanes, mandatory signals and throughput on the map (G3), the line router and terrain.

## Revision 4 (kept for reference)
Dated 2026-11-10, D168; superseded revision 3's search steps 2-3.
From the run digest, RECONCILE-O4.md and the second Fable steer. Kept from revision 3: every movement routed by the line router, splits and merges emerge from sharing, negotiation packs, forced crossings decided by branch and bound (D167), hug and clean-up (D166 a-b), main-line turn cost (D165), main line may bridge (D164).
Changed:
1. **Rules are part of the routing, not feedback.** The line router gains a small rule state machine per movement (shared with a sibling / private / merged; private units since the split; signal-legal pieces since a split or a far head). Waiting room (G7), split before a bridge (D113) and split-and-merge in one block become router constraints. Prio and coverage depend on another movement's route, so they are per-round masks from the main lines' current routes: no split inside a coverage window (D108), merges only where the main line runs straight for the coverage with its prio row free; a chosen merge reserves its prio row. The feedback loop is deleted; the checker confirms.
2. **PathFinder prices**: extra = (U + h) x (1 + pf x k) - U for a piece clashing with k others; pf from 0.5, x1.5 per round; h += U per round in clash. Main lines are routed first each round.
3. **Frame**: room for prio rows (main-line rows not on the box edge; no crop inside a prio window).
4. **Time on every generation**, against 100 ms. Branch position searched by bound later; forced crossings branched lazily later; multi-lane crossings decided per group (Fable steer 4).
Steps: O4c1 router rule state machine (brief O4c1); O4c2 organic search on it (feedback loop deleted). X1 (CP-SAT) serves as an offline lower bound on occupied tiles.

## Revision 3 (kept for reference)

Principle (D152-D153): the code holds rules, costs and a search; no shapes. Layout = whatever answer meets the rules and the throughput targets and scores best (D119: compactness, then cost).
Revision 3 (D160, engineer 2026-11-10) replaces revision 2's anchor layer: the main line is not assumed straight (a D80 doubled bridge shifts a lane; terrain may need ramps, a tunnel or a detour), so every movement, the main-line through movements included, is routed the same way, and splits and merges emerge where movements share track. Kept from revision 2 and the audit (D157, D158, D159): tokens, negotiated routing, throughput as judge and source of the prio decision, per-track-bit resources, route memo, minimum lengths.

## Inputs
`JunctionSpec` (notation, ml_gap, branch_gap, tl, design train -> prio_tiles, prices), terrain snapshot, anchor and orientation, a search box (grown a step when nothing fits), a traffic mix (`JunctionTrafficMix`; `MakeDefaultJunctionMix()` until the GUI has a setting, D119).

## Given (not searched)
Still a place tool: the cursor and rotation fix where each track crosses the box edge (the main-line tracks on the E and W edges, rows 0 and ml_gap + 1; the branch tracks on the S edge, branch_gap apart, at a position p searched over a few values). Attach to an existing line is a later mode that only changes where these come from.
Which merges carry a prio (D159): main-line merges always; a branch merge when a layout-free estimate (block times at design speed from the mix, design train and TL) says the side with priority misses its target without one. Decided once, before the search.

## Movements (a T: six)
E->W and W->E (main line through), E->S, W->S, S->E, S->W. Each is routed by the one line router (O1, O1b, O2, O4a) from its edge track to its edge track (lane bundles per notation later).

## Sharing: where splits and merges come from
Resource = track bit at a level (surface, bridge level, tunnel depth); a tile holds several bits that don't cross (audit 6; RECON-organic §6).
- Two movements may share a track bit only at the same level, in the same direction, and when they share an edge track: E->W and E->S share the E entry track until one leaves it (a split); E->S and W->S share from where they meet to the S exit (a merge). Sharing is free (it saves tiles).
- Opposite directions never share; nothing crosses at grade; two movements that part don't rejoin (except as lanes of one movement).
- A split is where a shared stretch ends, a merge where one begins. Nothing places them.

## Crossings and the main line on bridges
A movement crossing another goes over (bridge, LP6), under (tunnel, O2), or the other goes over it. The main-line doubled bridge (D80: split, two bridges side by side with equal path lengths, plain merge) is one router move for a main-line movement: a rule unit like a prio, not a stored shape. D156: money and compactness decide.

## The search
1. **Simultaneous routing, no prios.** Negotiated congestion (PathFinder, McMurchie & Ebeling): every round every movement is rerouted against present cost (other movements on a bit, unless they may share it) and history cost (how often a bit was contested); costs rise each round; converged when nothing conflicts; give up after K rounds (measured). Routes are memoised by (movement, ends, obstacle hash).
2. **Rule feedback on emergent points.** After each round, the rules about where splits and merges may be are checked along the shared paths (cheap, one-dimensional along each path): D108 no split inside a coverage, D148/D148a merge windows and phase, G7 waiting room, D83 bays, signal spacing, D113 a signal-legal piece between a split and a bridge head. A violation becomes a penalty on the tiles where the offending split or merge happened, which moves it next round. Minimum lengths (waiting room, bays) are also router conditions on a movement (O4n).
3. **Prios added.** For each merge that carries one, a prio token (two tracks side by side, at least the coverage long, following the line it protects, D158) is placed beside the merge where it came out; negotiation reroutes whatever is in its way. Needs the signal pass to accept joins off a single row (today `junction_signal_pass.cpp:377-380` requires the merge's row).
4. **Judge and score** (complete layouts only): signal pass and full checker (safety net); `EvaluateJunctionThroughput` (main-line delay zero, branch capacity at least its target, D119); compactness (occupied + slivers, D153), then cost. Branch and bound over p and restarts on occupied tiles with an admissible bound (sum of per-movement minimum lengths), not the box span (audit 5).
Per rule, count rejections in step 4; a rule that rejects often moves into step 2 or the router.

Expected work: 6 movements x a few rounds x a route each (memoised) x a few p. O5 measures it against 100 ms; the first thing measured is one route's cost (O4r).

## Steps
| Step | What |
|---|---|
| O4a | Merged: the line router takes a set of start pieces. |
| O2 | Merged: tunnels in the router. |
| O4r | Measure one route's cost: single-track and two-track routes across a junction-sized box, flat and hilly, with and without bridges and tunnels and obstacles. Decides whether 6 movements x rounds fit 100 ms. |
| O4n1 | Line router: present/history cost per (track bit, level) from the request; sharing allowed for bits of the same direction and group (the negotiation hook). |
| O4n2 | Line router: minimum path length for a routed token (prios, waiting room, bays). |
| O4s | Signal pass: prio joins off a single row (prios that bend with the line). |
| O4b1 | Search step 1 alone: the six movements of a T on flat ground negotiated (ports, resources, costs, rounds), layout out; converges, connectivity/geometry/bridges clean, timing. Brief: briefs/O4b1-negotiated-movements.md. |
| O4b2 | Steps 2-4: rule feedback (prio rows reserved, coverage switch penalties, minimum lengths), old finishing pipeline, prio timing gate, score over p (briefs/O4b2-rule-clean-organic.md). Main-line movements don't bridge yet (D163). |
| O4b3 | The D80 doubled-bridge move for main-line movements; D159 branch prios; throughput judge. |
| O4b | `GenerateJunctionOrganic` for the T on flat ground: steps 1-4. Outer tests: rule-clean on MakeTestJunctionSpec and PinnedTestSpec; the Pinned TL6 branch_gap 0 case builds; throughput targets met; compactness reported beside today's generator (190 occupied / 53 slivers). |
| O4c | On terrain (after O3): rule-clean on a hill, across a river, under a ridge. |
| O4d | Behind `GenerateJunction`/`GenerateJunctionNear` (GUI, library, rig) in place of the template; old path deleted. |

## Reused
Line router, checker, signal pass and prio placement, throughput evaluator and prio timing (S1, D137), pricing, build sequencer, compactness measure (C1), ghost.

## Later (not in O4)
- Throughput confirming or loosening the step-2 proxies (D108, D148): the route to hubs better than coop.
- Shared or optional prios beyond D159; attach mode; 4-way hubs (more movements, same search).
- Open from the audit: the doubled-bridge move's interaction with a coverage (D80 note), D148a's phase with bridge blocks.
