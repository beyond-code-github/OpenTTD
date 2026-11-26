# DESIGN: the length layer, tile routing and group compaction (draft 2026-11-26, D462)

Status: draft for the engineer. Read with FACTS.md (M8-M13, M11, T4a, C1) and D462. This replaces the drawing program's solve (stage A/B, choosers, joint CP). It does not replace Structure, the track map, the domain or the drawn hub.

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
   - **3a** (BRIEF-length-prio1): the extractor writes the rows into `hub.json` (Row tracks `prio:B1`, `prio:E1a`, `prio:E1b`: the beside run between the two joins) and marks the prio routes, from a `prio` column in `hubs.tsv`. The topology's `prio` line waits for 3c.
   - **3b**: Structure's row expansion per D471: a row beside its lane back across the lane's own doubled bridge, in pieces, so the map's rows match the answer key's by name.
   - **3c**: the judge measures coverage along the lane (bridge included, D471) and D108 with the D471 exemption; then `prio B1, E1` in the rig T1 topology and the builder lays each row piece exactly as long as its share of the coverage.
4. **Spike:** the exact T1 optimum on the engineer's variant (§9.2).
5. **Variant enumeration on T1:** the exact optimum per variant if step 4 holds, else the builder. Report it in the §9.4 vocabulary.
6. **The same on the L_R four-way,** then a decision on the scalable search (gap reservation with local backtracking, or negotiated routing, §M11), designed against the study's findings.

### 9.6 Open (for the engineer)
- Whether step 3's model is tractable. The curve windows are the risk.
- Whether the four-way has few enough variants to enumerate in full.
- Which human hubs to add as answer keys on the ladder.
