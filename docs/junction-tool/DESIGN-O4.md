# O4: a junction from the constraints (design, revision 3: every movement routed, splits and merges emerge)

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
| O4b | `GenerateJunctionOrganic` for the T on flat ground: steps 1-4. Outer tests: rule-clean on MakeTestJunctionSpec and PinnedTestSpec; the Pinned TL6 branch_gap 0 case builds; throughput targets met; compactness reported beside today's generator (190 occupied / 53 slivers). |
| O4c | On terrain (after O3): rule-clean on a hill, across a river, under a ridge. |
| O4d | Behind `GenerateJunction`/`GenerateJunctionNear` (GUI, library, rig) in place of the template; old path deleted. |

## Reused
Line router, checker, signal pass and prio placement, throughput evaluator and prio timing (S1, D137), pricing, build sequencer, compactness measure (C1), ghost.

## Later (not in O4)
- Throughput confirming or loosening the step-2 proxies (D108, D148): the route to hubs better than coop.
- Shared or optional prios beyond D159; attach mode; 4-way hubs (more movements, same search).
- Open from the audit: the doubled-bridge move's interaction with a coverage (D80 note), D148a's phase with bridge blocks.
