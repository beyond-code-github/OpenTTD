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
