# DESIGN: the length layer, tile routing and group compaction (draft 2026-11-26, D462)

Status: draft for the engineer. Read with FACTS.md (M8-M13, M11, T4a, C1) and D462. This replaces the drawing program's solve (stage A/B, choosers, joint CP). It does not replace Structure, the track map, the domain or the drawn hub.

## 0. Candidate facts about the solution
Principles the length layer is being built on, each with its rulings and its status: **shown** (a test or measurement holds it), **candidate** (ruled, not yet shown), **against** (a measurement contradicts it). Promote a row when its test lands (and add a claim to PAPER-notes); never delete one, mark it against.
- **P1 One judge.** Every pre-signal rule lives in `JudgePlanarDrawnHub`; no solver re-encodes a rule (D465). *Shown:* every builder step and answer key is judged by it (M20).
- **P2 Tiles, not rows.** Hubs are built on tiles by a router whose moves are legal by construction and confirmed by the judge, not drawn by linear rows (M11, D462). *Gap:* the router does not check a track against its own pieces (M34). *Candidate:* the router is sound locally (M20). No full hub has been built under the crossing plan yet (M21, M27-M29).
- **P3 The topology is fixed upstream.** Structure fixes the tracks, their crossings, order and sides (the crossing plan), and the builder only follows it (D468). *Shown:* the router keeps the plan (M20), and since 4450b96d89 also for chains with no listed crossing (M31, M32).
- **P4 Human hubs are a floor and an answer key.** Expert builds are extracted and judged by the same judge; their lengths are sketches, not targets (D389, D393, D474). *Shown on rig T1:* extracted with its prio rows, passes the judge but for the known curve (M20, M22).
- **P5 A prio is applied, not drawn.** The topology declares which routes take prios (D470). Structure carries the declaration and an optional type pin, and the solver picks placement and type (D473). A prio is applied to an existing lane section after the lane is laid, takes in the lane's own bridge when its type does (D471, D472), is calculated in length (D474), works on bent track (D475), and other tracks move for it (D476). *Candidate:* the regular prio is designed (§9.5 3c, prio3 tests parked); the Signals layer has no bridge type yet (M24, L5).
- **P6 Search, not a single pass.** The builder needs lookahead and a way to undo, with no loops (D477). *Against, for the plain forms:* chronological backtracking, forward checking and conflict reordering do not build T1 (M27-M29).
- **P7 Negotiation.** Blocked placements are resolved by rip-up and reroute over a dependency graph, the smallest stretch first, widening one fixed point at a time, with history costs for convergence. The router names the tracks it violates (D478, §9.7). *Candidate:* the soft router is in (ng2 a672fc9716, it names the tracks it violates); the loop over whole chains is in (8720241437), and the pinned block scenarios hold (M32: a corridor block and its cascade resolve as intended). T1 not built yet (M30: slow soft calls, whole-chain cascades). *Against, in the chain model:* six fixes did not build T1, and the tripwire fired (M34). The redesign on vertices and edges is next.
- **P8 Push, don't fit.** Room is made by moving things: along the line (nodes), sideways (routing around), at the edge (ports out) (D472, D476). *Shown at the edge:* Finish pushes ports out for bridges and port bays (ng2 c6613e22be). Along the line and sideways: candidate (P7).

## 1. The problem (D462)
Given one Structure variant, produce its most compact legal drawing, or near it, fast. Legal means the domain (tile rules) **and** curve speed (G3) **and** the coop path rules (L1-L4). Compact means the least area inside the outline (K1).

## 2. Why tiles (M11)
Every rule we have is a tile rule or a rule along a path of tiles, and each is cheap to check exactly. The row-based solve had to translate them into inequalities over continuous points, and every gap in that translation cost weeks (M10). Here the layout **is** tiles: a rule is checked where it lives, by the code that already judges the game (the domain, `JunctionMinCurveSpeed`). Nothing needs an encoding, so there is nothing to keep exact.

## 3. Inputs (kept from today)
- The track map: tracks, nodes, each track's events in order, crossings, structures (separator, crossers, allowed kinds, limit), copies, prio rows, sides per stretch.
- The headings the rules force: port headings, each separator on an axis across its span, node legs one eighth off (D404), crossing angles within their rules. **Not** Shape's free choice between a diagonal and an axis run with an S-bend (T4a): that is decided here.
- Rule fields: tl, cl, bay, coverage, node_leg, sd, deck and tunnel limits, port gaps.

## 4. The model
- A **layout** is the drawn hub on tiles: per track its pieces (tile, trackdir), nodes on switch tiles, structures as head, middles, head on an axis, ports on box edges.
- **Groups** that move as one (M11):
  - a **bundle**: lanes running together, their copies (equal, one tile apart, B5), their prio rows (one lane beside, coverage long, L2); like a PCB differential pair;
  - a **structure**: its deck and every crosser's crossing tile under it (B1, B4).
- **Legality** is one function on a layout: domain plus curve speed plus coop path rules. It is the only judge. Every state the algorithm keeps is legal.

## 4a. The engineer's model (2026-11-26): elastic lines settling into grooves
Engineer: "it's best to start with the turns"; "Nothing is truly fixed, although typically some things like main lines only shift a tiny bit"; "starting with the basic tracks and then kinda 'pushing' things apart where room needs to be made for crossings"; "It's almost more of a physics problem ... Layers of sand with gravity pulling them to the middle of the hub"; "there's a centre line to the hub, and things get pushed depending on what side of the hub they sit on. Imagine each line is elastic and settles into it's groove."
- The hub has a centre line. Gravity pulls every track towards it; a track is pushed outward on its own side of the centre line when room is needed (a crossing's bridge, clearance, curve spacing).
- Each line is elastic (wants to be short) and settles into its groove. Main lines are stiff (shift a little); others are soft.
- Every step is legal (D464): elements are added one at a time, each settled in its groove; when an addition needs room it pushes, and a push is either a track moving sideways out of the way or a track rising (a bridge) to let a line pass under. Nothing is ever illegal, and nothing starts stretched, so no stretch is inherited (M8, M16 came from shrinking a stretched start).
- Lookahead (engineer, 2026-11-26, after the greedy builder sprawled to 23 x 21 on T1): "I use lookahead when placing my tracks though - I'm not super good at it but this is what the good builders do, they know where the gaps need to be in order to place things and then do a small amount of reshuffling where neccesary". A track is placed leaving the gaps later tracks will need; reshuffling is small and local. Version 1 of the builder has no lookahead (each track is the cheapest given only what is placed) and no reshuffle; the reproduce test (BRIEF-length-reproduce1) separates the two: given the engineer's choices, which carry his lookahead, does the router rebuild his hub?
- Being worked through on simple hubs first (worked examples), before any probe. §5 below is the earlier draft (sparse start, then compaction) and is superseded by this once the examples settle it.

## 4b. Worked example T1, built by the engineer in the rig (2026-11-26, left-hand drive, no prios yet)
Read off the rig map (signs "PS - Step 1..3" at (91, 69), (92, 85), (93, 99)); steps side by side, every step a legal layout.
1. One main line and one turn leaving it (split, short diagonal, then straight to the south side).
2. A second turn leaves the same main line 5 tiles further on; the two branch tracks cross each other right away; the one running straight rises on a short bridge, the diagonal one passes under ("the tracks heading straight are the ones to rise").
3. The other main line is added last, two tiles from the first, and rises over the whole crossing cluster as a doubled bridge (split, two equal bridges staggered one tile, merge; D331). A third branch joins it from the south-west; one more straight-rises bridge below.

Result: about 20 x 11 tiles before prios, against the orchestrator's greedy attempt at 47 x 19 (which also wrongly used single main-line bridges).
Lessons for the physics:
- Start from one main line and its turns, not both main lines.
- Elements go close to where they connect; crossings resolve locally (rise, or step aside) in one tight cluster where the turns leave the main line.
- The second main line comes last and rises over the cluster in one doubled structure.
- Greedy insertion that fixes each element where it first fits spreads everything outward (the orchestrator's attempt); nothing should be fixed once placed.
Step 4, prios (the engineer edited step 3 in place): each prio row sits right beside its main line on the side away from the joiners, which is the outer side, so it takes no room from the crossing cluster and pushes nothing sideways. Its cost is along the line: the full coverage must fit upstream of the merge with no split or bridge inside (D108). Engineer, on what gives way: "in both cases the splits push back down the line". So a prio push is along the line, and the upstream splits move back; elastic branches absorb the change.

The pushes, as seen in this example:
- **rise**: a track heading straight lifts over a crossing (a bridge; a full-share main line as a doubled bridge);
- **step aside**: a track moves sideways out of the way;
- **along the line**: a length rule (prio coverage, D108; bays) moves a node along its line, and the tracks attached to that node stretch.

## 4c. Worked example 2: L_R four-way, no balancing (engineer, rig, sign 'P2 Step 1' at (19, 23); built in place)
1. One main line (W-E, row 26) and its first turn: split, short diagonal, south.
2. The second main line (N-S) added; the W-E line rises over it as a doubled bridge (rows 25-26, x 15-17); further south the N-S line rises (doubled, staggered, x 18-19, rows 30-34) over a turn joining the W-E line.
3. A new turn leaves the W-E line at (11, 25) and runs south down x 15; the N-S line rises again (x 16-17, rows 32-36). Everything stays in one cluster (about x 11-21, y 21-42).

Engineer: "I made it work but it's not great. The line coming from the NE should have made its right turn before crossing the core rather than after." The split after the crossing made the turn cross the other line's lanes again: one extra doubled structure.
Lessons:
- The push rules held again: turns leave close together, straight lines rise over them locally, one cluster.
- On a 4-way both main lines rise, each where turns pass under it.
- Quality depends on Structure's event order: split before crossing the core ("split early, merge late, cross in the core", D24). That choice is length-free and belongs to Structure; the physics cannot repair a bad order (as M13 for the variant). Already a rule for generated hubs: D286 ("split by direction early, cross in the core") and D24 (splits before merges); the engineer's hand build broke it, the tool would not.
- No prios here: all lines are equal. Bays were not measured: they "would largely just push splits or merges down the line again" (the along-line push). Left turns not built: "pretty straightforward" (in left-hand drive they cross nothing).


## 4d. The physics rules (draft v1, from worked examples T1 and the four-way; to be checked by hand on T2 before any code)
**Inputs (from Structure, length-free):** the track map with its event order (splits early, D286), for each crossing who rises (Structure's separator; default: the track heading straight rises), flow shares (stiffness: full share is stiff), the coop rule fields.
**Invariant:** the layout is legal after every step (D464): domain, curve speed, coop rules (bays, coverage, D108, signals before bridge heads).
**Insertion order** (engineer, 2026-11-26: "the insertion order seems wrong. Basically I was doing all the right turns first"):
1. The turns that cross the opposing traffic first (right turns in left-hand drive, left turns in right-hand drive): each placed against the line it leaves or joins, settled in its groove, close together. They cross each other in one cluster; where they cross, the track heading straight rises.
2. Then the straight lines, rising over the cluster where they meet it (one structure each, doubled when full share).
3. Then the other turns (left turns in left-hand drive), which cross nothing.
4. Prio rows (on the outer side) and bays last: they push along the line.
Order among the crossing turns (engineer): "There's always symmetry to these types of hub, either radial or regular so I'm guessing right turns can go in any order, but probably either clockwise or anticlockwise. Can't say for sure though". Default: round the hub in one direction; measure clockwise against anticlockwise in the first probe. Symmetry (radial or mirror) is also a lever: one sector settled can seed the others (DESIGN-tracks §14).
(The draft before this correction put the main lines first; the engineer's builds show a main line only as the anchor a right turn leaves from.)
**Pushes (each legal):** rise (a bridge), step aside (sideways), along the line (a node moves along its line, attached tracks stretch). A push takes the smallest legal amount, moves the softest element involved, in the direction away from the core given by the planar sides.
**Re-settle after every insertion:** every element moves inward (towards the cluster) while it stays legal; nothing is fixed once placed (the orchestrator's greedy T1 lacked this and spread outward).
**Open:** how "closest legal position" is searched; what moves a re-settle may use (the compaction probes' cut shifts and rewrites, group moves per bundle and structure); termination; whether insertion order matters beyond 1-5.

## 4e. Probe plan: T1 first, then the L_R four-way (the first real test; engineer)
**Data structures (engineer: "This will need to work with our existing topology stuff and data structures"):** the builder is a layer of the existing pipeline, not a parallel one. Its input is Structure's output for one variant: the topology (`PlanarTopology`), the track map (`PlanarTrackMap`: tracks, nodes, events, crossings, structures with separator and crossers, copies, prio rows, sides) and the rule fields (`PlanarRuleFields`), plus the headings the rules force. Its output is the existing hand-off `PlanarDrawnHub` (D452), with track i = the track map's track i, so Heights, Signals, the render, the domain (`PlanarDomainInputOfHub`) and the rig read it unchanged. The corpus extractor's `hub.json` only loads an engineer's or expert's hub into the same `PlanarDrawnHub` type for judging and comparison; it is not an input to the builder.
Outside-in, each step a brief:
1. **Answer key and judge.** Extract the engineer's T1 from the rig (corpus extractor on a map export, as for PSG 197: topology, structure, shape, lengths, pins) and judge it with our checks (domain, game curve speed, coop rules). Pins the legality function on a real legal hub and exposes any check that disagrees with a hub the engineer built.
2. **A legality function on tile layouts** that takes tracks as polylines plus nodes and structures directly (not via a Shape variant's pieces, which the compaction probes showed is too rigid once tracks change shape).
3. **The groove: a tile router for one track** (state: tile-edge point, heading, the bends inside the train-length window), so the tightest legal path from an anchor to its port is found with curve speed holding by construction. Crossing an existing track only by a rise (the straight-heading track becomes a bridge).
4. **Insertion for T1** in the 4d order (crossing turns round the hub, then straight lines rising, then other turns, then prios and bays), with the three pushes and a re-settle; compared with the engineer's T1, clockwise against anticlockwise.
5. The L_R four-way, against the engineer's example 2 (with its turn-before-core fixed by D286).

## 4f. The groove: routing one track on tiles (design for step 3)
**Input:** the hub so far (a `PlanarDrawnHub`, legal), a start (a point on a tile-edge midpoint with the heading the rules force there: a node leg one eighth off its trunk, D404, or a port's heading) and a goal (a port side with the positions its port spacing allows, D313, or a merge point with its leg heading).
**State:** (point, heading, the bends inside the train-length window: their travel position and direction). A move lays one piece (one step to the next tile-edge midpoint) at the current heading or one eighth either way (a 45-degree bend).
**A move is allowed only if it keeps the hub legal locally:**
- the new piece shares a tile only as the game allows without a switch (the two halves of a diagonal pair: Upper with Lower, Left with Right); never an X|Y diamond, never a bridge head tile;
- curve speed by construction: consecutive bends at least `curve_same` (same direction) or `curve_opposite` (opposite) apart, and the window counts (`curve_window`), the drawing program's curve rules (D420, which hold on V's PSG 197); the full game check (`JudgePlanarDrawnHub`) confirms the finished route;
- crossing an existing track only by a rise: (a) pass under an existing bridge's middles; (b) the existing track rises: it is straight on an axis there, with room for a head on each side clear of switches; it becomes a bridge over the new track (doubled when it is a full-share line's copy pair); (c) the new track rises: a macro-move laying head, middles over the crossed tracks, head, all on one axis. The default between (b) and (c): the track heading straight there rises (engineer).
**Cost:** travel (half tiles), plus a cost per bend and per new structure (Structure counts structures, D371). A* with the octilinear distance to the goal as the estimate.
**Output:** the track's points, and any structure it created or joined. The caller (the builder) checks the whole hub with `JudgePlanarDrawnHub` after inserting it.
**First version leaves out:** pushes (if no route exists, the builder reports it), copies and prio rows (added by the builder, not routed freely).

## 4g. The builder, version 1 (design for step 4)
**Input:** a topology and one Structure variant (track map, sites, structures, rule fields). For the first run: the engineer's own T1 topology as the extractor wrote it (`tools/junction/corpus/out/rigt1/topology.txt`), so the result compares one to one with his `hub.json` (same topology, left-hand drive). **Output:** a `PlanarDrawnHub` with track i = the track map's track i, judged legal by `JudgePlanarDrawnHub` after every step.
**Phases (§4d order):**
0. **Anchor:** the line the first crossing turn leaves (or joins), laid straight between provisional port points on its two sides.
1. **Crossing turns, round the hub** (in ring order from the anchor's in port): each from a node on an existing line to its other end (a port side, or a merge on another existing line). Node placement: candidate points along the existing line, one tile apart, nearest the cluster first; the first candidate whose routed track leaves the hub legal is taken ("closest legal position"). A port end is a goal line (the side's provisional edge at the port's heading, any position along it).
2. **Straight lines:** the remaining port-to-port lines, routed from side to side; where they meet the cluster they rise (the router's rule: the newer straight track rises).
3. **The other turns** (crossing nothing), as phase 1.
4. **Along-line pushes:** where the judge reports a short bay, the node at its start moves back along its line by one tile and the tracks attached to it are re-routed; repeat until legal.
5. **Ports:** every port track is extended straight to the final box edge; port spacing (D313) is reported, not yet enforced.
**Version 1 leaves out, and reports:** doubling (full-share lines over crossings are single bridges), prio rows and coverage, signals before bridge heads, re-settling (measured first: how far version 1 lands from the engineer's T1 tells us which to add).
**Router extension needed:** a goal line (reach x = X or y = Y at a heading, any position along it), step 3c.

## 4h. The hybrid pipeline (engineer, 2026-11-26: "This is starting to sound like it could form part of a hybrid approach with the old solver perhaps? Heights was the one thing that worked really well about the old one")
1. **Structure** (unchanged): variant, track map, event order (D286).
2. **The builder** (§4d, §4g): every discrete choice, legally and fast: node positions, bends, which track rises, port positions.
3. **Length polish** (the drawing program with the builder's choices fixed: sides from the built hub, bends fixed): every length as tight as the rules allow, the whole hub at once. M8: exact and under 0.1 s on T2 with every choice fixed, domain- and curve-legal; it could not make the choices, the builder does.
4. **Heights**: the old level solve (ramps, foundations, terraform) on `RenderPlanarDrawnHub`'s layout, as the planar back end already sends its plans through it (D365's Heights layer).
5. **Signals**: the signal pass on the drawn hub (D452, D455).
To verify when the builder lands: the polish keeps the builder's hub legal and shrinks it; the old level solve accepts the rendered layout unchanged.

## 5. The algorithm (VLSI symbolic layout, then compaction)
1. **A legal sparse start.** Lay the topology out with generous spacing: structures and nodes far apart, tracks routed between them on tiles by a state-space search (state: tile edge, heading, and the bends inside the train-length window, so curve speed holds by construction; G3). Fixed-size elements (bridges, prio rows, bays) keep their size; only the free track between them stretches. Phase 1 needs no quality, only legality (M8 showed choices from a stretched drawing are usable).
2. **Compaction by legal moves.** Repeat until no move shrinks the area:
   - **cut shift**: everything on one side of a cut through free track moves one tile toward the other; each track the cut crosses loses a tile there;
   - **local rewrites**: shorten a run, slide a bend, turn a diagonal into an axis run with an S-bend or back (T4a), drop an S-bend that no longer does anything;
   - **group moves**: a bundle or a structure moves as one.

   Each move is applied and judged by the legality function; a move that breaks anything is undone.
3. **Hand-off:** the drawn hub (D452) to Heights and Signals, unchanged.

## 6. Known risks (to probe in this order, riskiest first within the layer)
1. **Compaction quality.** Does a greedy sequence of legal moves reach B-level density, or do coupled tracks lock (M11)? D445 parked one-axis compaction in the row solve because fixing one axis freezes diagonals. Here a cut that crosses a diagonal needs a rewrite at the crossing (a diagonal cannot lose one tile of x alone), so cut shifts and rewrites must combine. This is the first thing to test.
2. **Speed.** Number of moves times the cost of a legality check. The domain on T2 runs in milliseconds; incremental checks (only the tiles a move touched) may be needed.
3. **The sparse start for any topology:** placing structures and nodes so every track routes.

## 7. First probe (risk 1)
Use T2's existing legal drawing (`t2-p6cf`, 68 x 50.5) as the sparse start. It is legal, stretched, and on variant 0. Build only the legality function on a tile layout and two moves (cut shift in x and y, with the diagonal rewrite at the cut). Run greedy compaction. Measure the area against 69 x 48.5 (rows, bends fixed) and the time. If cut shifts lock immediately, record where and why before adding moves.

## 8. Settled (D463)
- The objective is the outline area (K1).
- Nodes move freely in compaction; their bays and coverage are judged by the legality function after each move.

## 9. The plan after the reproduce test (draft 2026-11-26, for the engineer's review)
**Where we stand.** The tile router is sound locally (FACTS M20): given the engineer's surroundings, it lays his track back, legal and as short. The one judge holds every rule (D465 to D467), and his rig T1 passes it apart from the known curve. What is missing is the search on top: lookahead (leave room for later tracks) and backtracking. On top of that, the builder uses only part of Structure (§9.1).

**Principles.**
- Hubs emerge from the rules (memory: emergent hubs). The judge defines legal, the outline area defines better (D463), and human hubs are a floor. Lookahead, ordering and backtracking are search machinery only; they never restrict the answer.
- Every variant is a hub (D468), enumerated offline. The opinions come from which variants win, and they are written in generic terms so they carry from T1 to LLLL_RRRR and beyond (§9.4).

### 9.1 The builder takes all of Structure, no Shape
Today it reads the tracks, nodes and straight legs. It adds three length-free facts from the track map. Together they are the lookahead's raw material: they say, before any length, what must pass between what.
1. **Crossings exactly as the map lists them.** A routed track crosses exactly the tracks its events list, in their listed order, arriving from the listed side (the D389 marks), and touches no other track. It never crosses a box edge except at its own port. On tiles this is homotopy routing: the track stays in its faces of the planar map, so it can no longer go round the back of a main line (the north loop in study 2).
   - *Router change:* the state carries the index of its next event. A rise or crossing over a track that is not the next event is refused, and so is arrival from the wrong side.
2. **Separators from the map.** At each crossing, the track that rises is Structure's separator, not "the straight one". Crossings in one structure share one bridge. This replaces the router's rise rule for map crossings.
3. **Sides per stretch** follow from 1 for crossing tracks. For tracks that never cross (parallel neighbours), the map's side says which side of the other the track runs on. The router refuses a first piece on the wrong side.
- *Also:* the router gets the curve window of the track it continues across a node (the bends before the node on that route), not none (L11 in M20).
- *Check:* leave one out on the rig T1 stays identical (13 of 15 same points, all at his travel), and no T1 build of any seed crosses a track outside its list.

### 9.2 The exact optimum per variant on small hubs (a spike)
A CP-SAT model of T1 on tiles, offline. It decides every tile, under the same rules the judge checks, and minimises the outline area. It does not scale, and it does not need to. Its purpose is ground truth: the best T1 per variant, which may be one no person would draw, and the yardstick for every scalable method.
- *Model sketch:* per track, a path in the router's state graph (point, heading, curve window), with arc binaries and flow conservation. Rises are macro arcs, as in the router. Every tile allows only the domain's legal piece combinations. Crossings follow §9.1. Each bay is a path-length row from its split. The box is a variable; minimise W x H by an outer loop over W.
- *Spike plan:* first the engineer's variant of T1. Does it find a legal hub? Is it at or below 17 x 11? How long does it take? Time box: one session. Offline solves may take minutes, not the hover's 5 s.
- *Every solution is confirmed by the one judge,* never by the model's own rows (memory: independent check).

### 9.3 The variant study (D468)
1. Enumerate every Structure variant of the hub (`BuildPlanarStructure` without `keep`).
2. Give each one its best legal area: the exact optimum (§9.2) where it runs, the §9.1 builder with search where it does not, and both where both run, to calibrate the builder.
3. Record per variant its decisions in the generic vocabulary (§9.4) and its area.
4. Find the decisions shared by the best variants and missing from the worst.
5. Prune (D469). Per variant only its optimum matters. Variants whose optimum is outside a range of the hub's best are dropped. The rest are the tool's options, for the user to choose from or for the tool to randomise. The §9.4 opinions carry the pruning to hubs too big to enumerate.

The ladder: T1 → L_R four-way → T2 (16 384 variants) → T3 → LL2 → PSG 197 → LLLL_RRRR. Each rung checks whether the previous rung's opinions still predict the winners.

### 9.4 The generic vocabulary (so opinions transfer between hubs)
Opinions are stated about kinds of decision, never about bit indices or line names. A first list, to grow from the studies:
- **Crossing over:** prio over non-prio; full share over half; the line turning less over the line turning more; the line crossing more over the line crossing fewer.
- **Straight leg at a tie site:** the leg turning less; the leg toward the hub's centre line; the leg with the longer run to its port.
- **Triangle flip:** which pair of the three crosses first, named by the lines' roles (prio, turning, straight).
- **Grouping:** crossings of one line sharing a bridge, against separate bridges.
- **Mover separators (D372):** the joiner rises, or the main line does.

Each variant is a vector of these choices. A study reports, per choice, the area distribution with and without it. An opinion is a choice whose presence reliably predicts a smaller area across hubs.

### 9.5 Order of work (each step one brief, one fresh agent, checked against its brief)
1. **Router:** the curve window across a node (§9.1, last bullet).
2. **Router and builder:** crossings exactly as the map lists them, separators from the map, sides (§9.1 1-3).
3. **Prio rows (engineer, 2026-11-26: "We've got a teach it about them sooner or later though right?"):** declare `prio E1, B1` in the rig T1 (D470) and add his rows to the answer key. The builder lays each prio row beside its lane, exactly its coverage long (L2, D384), with its joins. Coverage is one of the three things that set T2's size (K5), so no variant study runs without it. His rows are already in the rig T1 (FACTS M22); E1's spans its doubled bridge in two pieces, which is legal (D471). Sub-steps, one brief each:
   - **3a DONE** (ng2 32a79deb53, BRIEF-length-prio1): the extractor writes the rows into `hub.json` (Row tracks `prio:B1`, `prio:E1a`, `prio:E1b`: the beside run between the two joins) and marks the prio routes, from a `prio` column in `hubs.tsv`.
   - **The model (D472, D473):** a prio is applied to an existing section of the lane upstream of its merge, after the lane is laid; Structure only carries the declaration (D470). The solver picks each prio's placement and style; the topology may pin a style per join. Styles known: a plain row (B1), and a bridge prio over the lane's own bridge (E1: coverage along the lane, bridge included, ground either side for joins and signals, path signals). Where the coverage does not fit, the merge moves downstream (an along-line push, as bays do).
   - **3b, probe (riskiest first):** what the Signals layer and `CheckPrio`/`CheckPrioReach` say about the answer key's two prios as built: does B1 signal and judge clean, and what does E1's bridge prio need (two row pieces, path signals)? Measure each prio section's length along the lane against `rules.coverage`. This settles what "fits" means before the builder lays any row.
   - **3b DONE** (FACTS M23, M24, L5; D474): coverage is calculated, never read off a hub; today's Signals layer has no bridge prio; the coop wiki lists the styles.
   - **3c, the regular prio in the builder** (BRIEF-length-prio3; engineer: "assume the user asks for a regular prio here - we still need to handle that case"). Every style must cope when its natural place is taken, and for the regular prio that means the join or split moves.
     - **Structure** gives the length layer the declaration only: `LoadPlanarStructureVariant(..., rows = false)` leaves out its row expansion (D349's rows belong to the Shape/Slots pipeline), and the routes keep their prio flags (D473).
     - **The fit rule** for a regular prio at merge point M on the trunk chain X (the prio lane is the merge's straight leg; the row lies on the side away from the joiner): the section is X's last `coverage + 4` half tiles before M (the row's beside run is `rules.coverage`, its opening and closing steps 2 along each, D421). It must lie on one axis segment of X, hold no node of X other than M (lane nodes excepted, D24), and touch no bridge (X as separator, or under another chain's span). The beside run's tiles must hold no other track. Candidates that break it are dropped before routing, as `KeepBays` drops bay breakers, so the merge lands farther downstream. A node candidate inside a placed section is dropped too, so a split lands farther upstream.
     - **The row** is a hub track appended after the map's tracks, kind Row, named `prio:<route>`, points `[O, O + opening, M - closing, M]`. Later chains see it as ground track, so nothing crosses it (v1).
     - **The check** is independent of the builder: the signal pass with the builder's rows, then `CheckPrio` and `CheckPrioReach` (the game's pre-signal logic), on two synthetic hubs, one pushing past a split and one past a bridge.
     - **Left out and reported:** a diagonal or bent section, a crossing through a coverage (rows not mirrored, D458), styles other than the regular row.
   - **3c is on hold behind the search (D477).** Finish pushes ports out for bridges and port bays (ng2 c6613e22be, D476). Prio3's tests are in `wip/prio3-tests.patch`; it resumes as step S3, where a row conflict is a restriction like any other.
   - **The search (D477)**, riskiest first:
     - **S1** (BRIEF-length-search-bt1): depth-first search with chronological backtracking over today's placement sequence. Each placement can be resumed from an attempt index, so attempts at a level only increase, and a state signature is kept so no partial state is explored twice: no loops. A 5 s budget. A finished-hub failure resumes the last level. Outer test: the engineer's T1 builds under the crossing plan (M21).
     - **S2**: optimistic lookahead (engineer, 2026-11-27: "lets see how far we can get with our home-grown implementation at any rate"). These are necessary conditions only, checked after each placement: a planned structure's whole span is reserved when its separator is placed (M26), every prio lane still has room for its coverage, every bay is still reachable. Measure the shrinkage of the search on T1 and PortBridge. Conflict-directed backjumping only if S1/S2 measure it as needed.
     - **S3**: the regular prio on the search (prio3 with amendment A, bent prios): coverage and row conflicts become restrictions (along the line: a node beyond a point; step aside: a chain placed after the row), and the bay push becomes one of them.
     - **Parked, S-CP (engineer, 2026-11-27: "there's no reason we can't just do this exact same method with CP-SAT as the implementation engine")**: a time-boxed CP-SAT spike on T1 and PortBridge, run only if the home-grown search (S1-S3) stalls, or as a comparison once it builds. (1) Per chain, a pool of node points and tile routes from today's router. (2) Pair compatibility from the judge's tile rules and the crossing plan (no shared tiles, listed crossings only, from the listed sides), computed, never hand-encoded. (3) CP-SAT picks one candidate per chain, node points consistent, minimising the box; the few many-chain rules (a structure's span over all its crossers, prio coverage along a lane) are constraints over the choices. (4) The full judge on the assembled hub: whatever it rejects becomes a cut, then re-solve with the last answer as hint (CP-SAT has no in-search callback). The pool grows in rounds around the best answer (negotiated routing, M11). It is compared with S1 (built, attempts, time, box). If it wins, CP-SAT is the engine and the router and judge are its oracle. It also stands in for step 4's exact spike (optimal over the pool).
4. **Spike:** the exact T1 optimum on the engineer's variant (§9.2).
5. **Variant enumeration on T1:** the exact optimum per variant if step 4 holds, else the builder. Report it in the §9.4 vocabulary.
6. **The same on the L_R four-way,** then a decision on the scalable search (gap reservation with local backtracking, or negotiated routing, §M11), designed against the study's findings.

### 9.7 Negotiation (D478)
The builder's main loop becomes rip-up and reroute over a dependency graph (PathFinder, M11), replacing chronological backtracking (M27-M29).
- **Elements:** chains (and later prio rows and structures). An element depends on the elements its node ends attach to (a row on its lane, a crosser's passage on its structure). Ripping up an element rips up its dependents; they re-enter the queue.
- **Soft router:** in soft mode the router may take tiles that other tracks make illegal (a shared tile, an unlisted crossing, a crossing from the wrong side or out of order), at a cost per tile times the tile's history. It returns the tracks it violated, so blame comes from the router itself.
- **Local rip-up (engineer: "as a human builder I just trace the lines back and see what needs to move"):** the unit ripped up is the smallest stretch of the blocker holding the conflict, a track between its two fixed points (nodes, ports, structure heads), rerouted between the same points. If it cannot reroute, widen by one fixed point: a node moves along its trunk (its candidates, as today) and the legs attached there reroute. Only then does a whole element and its dependents go.
- **The loop:** take the next queued element. Route it hard. If that fails, route it soft. A soft route with no conflicts is the hard answer. Otherwise the element is placed, its conflicting elements and their dependents are ripped up and queued, and every contested tile's history rises. Reservations (a structure's whole span, a prio's coverage section and row, D476) are elements too, placed with their owner, so "reserve the space and reroute the current line" is the same move.
- **Convergence, no loops (D477):** history costs make repeated fights dearer, so the negotiation settles. An iteration cap and the state signatures are hard stops. The order of the queue is today's `Sequence()`; ripped-up elements go to its front.
- **Pinned scenarios (engineer, 2026-11-27: "pin this with some tests that contrive some block scenarios and then prove that the re-route resolves in the way we expect"):** a test seam lets a build start from a given partial hub (chains pre-placed where the test puts them). Each scenario asserts who is ripped, how often, the final judge, and termination. (1) Order block: B pre-placed across C's only corridor. Only B is ripped, C takes the corridor, B reroutes around, the hub is legal. (2) Cascade: a leg attached to the ripped chain goes with it and comes back after it; nothing else moves. (3) Anchor protection: the only way through crosses the anchor, so the build fails with the named reason and never rips the anchor. (4) Two chains, one corridor: converges within a stated number of rip-ups, the loser detours, no ping-pong (the no-loop guarantee, D477). (5) No conflict: unchanged.
- **Blame from Structure, preferred (engineer, 2026-11-27: "as a human looking at this I know exactly when to rip up vs move around. I just can't do all the permutations quickly in my head. But we also have the structure to help guide us right?").** Structure's track map fixes what a blocked chain C may cross (the crossing plan: a planned crossing is a bridge to build, never a rip) and which side of every other track it runs on. Because the router crosses only planned tracks, C's route already stays inside one face of the planar map. If C has no route, its face is too tight, and the only tracks that can be in the way are that face's walls: a small set named by the map. The blame step rips the cheapest wall (not the anchor; nearest first, history breaking ties) and routes both hard. Going around is the router's job inside the face; ripping is moving a wall outward. No soft search, no guessing. The soft search below is the fallback if face walls prove too coarse (a large face with many walls).
- **The soft cost is a detour length (engineer, 2026-11-27: "I don't know that we want to go through all the things though do we? That will just always be a diagonal/straight line from point to point").** A low conflict cost makes the soft route the straight line, which rips everything on it. A high one finds the fewest blockers only after exploring every long detour (M30: about 0.4 s a call). Set the cost to a detour of k tiles: the route goes round anything it can pass within about k extra tiles and rips only what costs more to avoid, and the search never explores detours longer than k. Start k at one train length; measure time and chains ripped on T1. Measure first where the 0.4 s goes (router or judge). Fallback: blame read off the hard router's most promising refused paths (no second search).
- **Tripwire: patching (engineer, 2026-11-27: "Feels like we're patching over stuff at the moment? Lets just keep an eye on this").** Since M27 each step has fixed the last symptom T1 showed: backtracking, forward checking, reordering, whole-chain negotiation, soft-first, stretch rip-up. The common limit is the builder's unit, the chain, an artefact of placement order. The hub's own units are vertices (nodes, crossings, ports) and the edges between them (§9.8's handles). The stretch rip-up (BRIEF-length-stretch1) is the last change made in the chain model. **If T1 still does not build after it, stop and redesign the builder's state as the planar map**: vertices placed, each edge routed elastically between its vertices, negotiation and the drag tool both acting on vertices and edges. No further chain-level fix before that decision.
- **Bear in mind, not yet (engineer, 2026-11-27: "we're gonna need some kind of heuristic/branch and bound here aren't we to constrain the search space. Don't do it now, just bear it in mind"):** prune the candidates with bounds. A lower bound on the area (or cost) of any completion from a partial state drops a branch that cannot beat the best hub found; an ordering heuristic tries the likely winners first. Take it up when the measured search size calls for it, and compare with the parked CP-SAT engine (S-CP).
- **Steps:** N1, the soft router (`soft_cost`, `history`, `conflicts`), tested on small hand hubs. N2a, the negotiation loop over whole chains (outer test T1). N2-tests, the seam and the pinned scenarios above. N2b, ripping up one stretch, if the measured cascades need it. N3, prio rows and structure spans as reserved elements (prio3 resumes here; bent prios, D475).

### 9.8 The drag tool (engineer, 2026-11-27, not started)
Engineer: "I'd like to be able to 'drag' nodes around in the actual UI and see the effects of the routing in real time. This would give us confidence in the 'push and pull' and elastic track aspects of things which we could then automate into the solve."
- **The primitive is N2b's:** move node N to point p (a candidate along its trunk), then reroute the stretches attached to N between their fixed points: hard first, negotiation (stretch rip-up) only if blocked. The drag tool and the automated push are one primitive with two drivers.
- **The UI** (in the fork's junction tool, `src/junction/tool/` ghost drawing): the built hub as a ghost; grab a split or merge and slide it along its trunk; each move calls the primitive and redraws; the judge's lines (bay, curve, D108) show as red tiles.
- **Interaction model (engineer: "I'd probably 'drag' splits or merges downstream or upstream, then I'd pull the crossings or lines sideways to make room for other lines - and bends would appear or disappear based on what I'm moving around. It will also help us understand what nodes and edges really are"):** three handles, each a vertex or edge of the planar map and each a solver move. (1) A split or merge slides up or downstream along its trunk (a node; the along-line move). (2) A crossing moves sideways or along, its structure with it (a crossing vertex; rise and structure placement). (3) A line is pulled sideways to open room (an edge, the stretch between two events, given a waypoint; step aside). Bends are not handles: each stretch reroutes elastically between its fixed points, so bends appear and vanish. If a drag needs a handle the map lacks (a bridge head, say), the map is missing a vertex. A recorded sequence of drags is a recipe the solver can compare with and later automate. Build order: the shared primitive (reroute a stretch between fixed points, optionally through a sideways waypoint), then node drag, then crossings, then lines.
- **Speed:** a hard route on T1 is about 50 ms, so a move that reroutes 2-3 stretches is 100-200 ms, interactive. Moves that need negotiation will lag, which is itself a measurement.
- **Order:** after N2b (the primitive), before N3 (dragging prio joins with their rows tests D472-D476 directly). The GUI's design (what is grabbed, what is shown, when it reroutes) is settled with the engineer first.

### 9.9 The drag feature: state and primitive (D479, D480)
After the tripwire (M34) the focus is the drag feature (engineer: "lets focus on what we need for the drag feature").
- **State:** a drawn hub's polylines. Vertices are ports (fixed), nodes, and structures (one handle each, D479). Bends are not handles: each edge between two vertices is relaid when a vertex moves, so bends appear and vanish (D480).
- **Drag (D480):** each mouse move proposes the vertex at the tile under the cursor. The layout updates only if that position is valid: every edge attached to the vertex can be laid legally between its end vertices, and the judge accepts the hub. Otherwise the drag shows the last valid layout. No search over positions.
- **Relaying an edge:** between two known end points with known end headings (the neighbouring pieces' headings at each end), the least-bend octilinear shapes are few: straight, one bend, two bends (an S-bend or a dog-leg). Enumerate them in closed form, keep those whose bends hold the curve spacing (also across the ends, against the neighbours' bends) and whose pieces hold their least lengths and touch no other track, and take the one with the fewest bends, then the shortest. The router (A*) is the fallback only when every simple shape is blocked. One evaluation is then microseconds to a millisecond.
- **Already built (D479's version):** `PlanarDragHub` with fixed headings and a clamp along the drag direction (ng2 3f540f554d, 26926c427f). It stays as the fine slide when no heading needs to change; D480's relayer is the main path.
- **The router relays (D483, engineer: "I think the router is the only thing that will work with obstacles and existing track"):** closed-form shapes are dropped; each attached track is routed by the A* groove router around existing track, with the curve windows across its ends, and in the drag tool it may bridge any track (`cross_any`, the straight one rising). The bullets on closed-form shapes and on crossing plans derived from a shape below are superseded.
- **Node orientation (D484):** a moved node turns to face its tracks: its trunk heading is free among the 8 and its legs keep the node rule (straight leg on the trunk, the other one eighth off toward its side), so only the angles that work appear. BRIEF-drag-d8.
- **Straight leg (D485):** which leg of a node runs straight is free (no main-line rule), so a dragged node tries both alignments (the trunk on either leg) with each rotation. BRIEF-drag-d9.
- **Tridents, later (engineer, 2026-11-27: "trident merges and splits are a thing that we need to support at some point"):** a 1-to-3 split or 3-to-1 merge, the straight leg and one leg one eighth off on each side. Known fact (planar CLAUDE.md, D400 node proof): a trident exists only on an axis trunk, so a trident node's orientation candidates are the four axis headings. The topology notation today writes a 3-way split as two 1-to-2 splits (psg197 notes), so supporting it touches the topology (a 1-to-3 node), the drawn hub's nodes, the router's node rules and the drag's orientation candidates.
- **Crossing drags (D482, engineer: "drag a line across another and see it bridge"):** in the drag tool a relaid track may cross other tracks. The crossings come from its best closed-form shape (the tracks it crosses, in order, the side each arrives from, and the riser: the straight one, or the dragged one when both are straight). The track is then routed by the groove router under exactly that plan, so the router builds the bridge (a rise, or a lift of the other track) as hub structures. The one judge decides (span limit, heads, spare middles). Dragging back removes the crossing and its bridge. Next step after D3 (D4). Test: on the two-node hub, the merge dragged west past the turn leg makes the joiner cross it with a bridge; dragged back, the bridge goes.
- **Steps:** D1c the edge relayer (closed-form shapes) and `PlanarDragTo(hub, vertex, target) -> hub or invalid`, tested on the roomy hub: the split dragged up from the main line grows an S-bend each side and dragged back loses them; a target where the curve rules fail is invalid and leaves the hub unchanged. Then D2, the in-game UI on `PlanarDragTo` (its design with the engineer first).

### 9.6 Open (for the engineer)
- Whether step 3's model is tractable. The curve windows are the risk.
- Whether the four-way has few enough variants to enumerate in full.
- Which human hubs to add as answer keys on the ladder.

### 9.10 The port tool: drawing a hub from scratch (D494)
The drag tool is the solver's engine; the port tool exercises its primitives on hubs drawn from nothing. A "Draw hub" mode starts an empty session. A pair of clicks adds a route: the first click is the entry, the second the exit. Two new ports lie on opposite sides along the dominant axis (D494 (1)), and the track is routed by the groove router with `cross_any`, so a crossing makes a bridge. A first click on an existing port's handle splits a new route from that port's line at the cheapest point (D15b), and a second click on an existing exit merges into it (D15c). Dragging works on a drawn hub as on a loaded one. Steps: D15a (two new ports and one track; `PlanarAddRoute`, `JunctionDragSession::AddRoute`, the button), D15b (split), D15c (merge), then pushing.
