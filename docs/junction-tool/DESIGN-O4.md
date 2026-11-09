# O4: a junction from the constraints (design, revision 2: tokens and negotiation)

Principle (D152-D153): the code holds rules, costs and a search; no shapes. Layout = whatever answer meets the rules and the throughput targets and scores best (D119: compactness, then cost).
Revision 2 (D157-D159, engineer 2026-11-10) replaces revision 1's backtracking over movements with a token layer and negotiated routing; the inputs, the given main lines, the crossing options and the score are unchanged.

## Inputs
`JunctionSpec` (notation, ml_gap, branch_gap, tl, design train -> prio_tiles, prices), terrain snapshot, anchor and orientation, a search box (grown a step when nothing fits), a traffic mix (`JunctionTrafficMix`; `MakeDefaultJunctionMix()` until the GUI has a setting, D119).

## Given (not searched)
Still a place tool: the cursor and rotation fix these, nothing needs to exist yet (attach to an existing line is a later mode that only changes where they come from).
- The two main-line tracks: straight through the box at the cursor (rows 0 and ml_gap + 1), routed straight across the box by the line router on terrain (ramps, bridges, tunnels allowed).
- The branch: its two tracks leave the box through the S edge, branch_gap apart; its position p along the edge is a search variable (a few values around the anchor).
- Which merges carry a prio (D159): main-line merges always; a branch merge when the throughput estimate says the side with priority would otherwise miss its target.

## Tokens (layout units, D157)
A token is a rule unit with a set of legal placements. No token stores a shape.
- **Anchors** (on the given main-line tracks; a placement is a position x):
  - split: a movement leaves the track;
  - merge: a movement joins the track;
  - lift: a doubled main-line bridge (D80) over a span: split before, two bridges, plain merge after.
- **Routed tokens** (a placement is a route by the one line router):
  - movement: EIn->SOut, WIn->SOut, NIn->EOut, NIn->WOut (lane bundles per notation later), from its split to its merge;
  - prio bundle (D158): two tracks side by side ending at a prio merge, at least the coverage long along its path (the design train's prio_tiles, D137); may bend and may cross by bridge or tunnel;
  - branch trunk: the branch tracks from the S edge to their first split or merge, where that isn't already part of a movement.

## Layer 1: anchors (exact, one-dimensional)
On a straight main line every placement rule between anchors is a relation between positions x, so the legal anchor sets are enumerated exactly, before any routing:
- a merge into track T lies at least prio_tiles + 2 past any split on T (D148), at the phase PrioReach needs (D148a);
- no split, lane split or lane merge inside a prio's coverage plus one segment (D108, G9c);
- signal spacing between consecutive anchors; a signal-legal piece between a split and a bridge head (D113);
- a lift's split, bridges and merge stay clear of every coverage except as D80 allows (the coverage sees each bridge block through a join on a plain piece at its end, `junction_signal_pass.cpp`).
Anchor sets are ordered by an estimate of the occupied tiles they imply (the span between the outermost anchors on each track), smallest first. Expected size: a few positions per anchor on a ~40-tile line.

## Layer 2: routed tokens, negotiated (PathFinder, McMurchie & Ebeling)
For one anchor set, every routed token is routed between its fixed ends by the line router (O1, O1b, O2, O4a) with an extra cost per (tile, level) resource:
- resources: surface tile, bridge level, tunnel depth (RECON-organic §6: per tile, surface track + one tunnel per depth + at most one bridge span; bridges never cross bridges);
- present cost: how many other tokens use the resource now; history cost: how often it was contested in earlier rounds;
- each round reroutes every token that touches a contested resource; present and history costs rise each round;
- converged when no resource is shared; give up after K rounds (K measured) and take the next anchor set.
Rules measured along a routed track (a coverage on the branch trunk, signal spacing on a movement) are enforced in the router as state, the way D148a moved a rule into the windows.
Crossing a main line, three ways, cost decides (D156: no extra penalty; the lift is tried first because it is usually best):
1. the token bridges over the main line (LP6);
2. it tunnels under (O2);
3. the main line is lifted: main-line tiles are crossable at grade at the lift's share of a doubled bridge (today's crossable_cost); a crossing at grade makes a lift anchor over that span part of the anchor set; if no legal lift fits there, the price pushes the token to 1 or 2.

## Layer 3: judge and score (complete layouts only)
1. Signal pass and the full checker (the safety net for rules not yet in layers 1-2).
2. `EvaluateJunctionThroughput` with the traffic mix: main-line delay zero, branch capacity at least its target (D119). A branch merge built without a prio that misses its target is marked as needing one and the search reruns (D159).
3. Score compactness (occupied + slivers, D153), then cost. Branch and bound on occupied tiles (the only part that only grows).
Per rule and per layer, count rejections; a rule that rejects often in layer 3 moves into layer 1 or 2 so it prunes earlier.

Expected work: anchor sets in tens, each 4-6 routed tokens x a few rounds x a few ms per route; the first converged sets bound the rest. O5 measures it against 100 ms.

## Steps
| Step | What |
|---|---|
| O4a | Merged: the line router takes a set of start pieces. |
| O4m | Measure layer 1 alone: enumerate legal anchor sets on MakeTestJunctionSpec, PinnedTestSpec and the Pinned TL6 branch_gap 0 case; count them and confirm the TL6 case has at least one. No routing. |
| O4n | Line router: an extra cost per (tile, level) resource, read from the request (the negotiation hook); a minimum path length for a routed token (prio bundles). |
| O4b | `GenerateJunctionOrganic` for the T on flat ground: layers 1-3. Outer tests: rule-clean on MakeTestJunctionSpec and PinnedTestSpec; the Pinned TL6 branch_gap 0 case builds; throughput targets met; compactness reported beside today's generator (190 occupied / 53 slivers). |
| O4c | On terrain (after O2/O3): rule-clean on a hill, across a river, under a ridge. |
| O4d | Behind `GenerateJunction`/`GenerateJunctionNear` (GUI, library, rig) in place of the template; old path deleted. |

## Reused
Line router (O1, O1b, O2, O4a), checker, signal pass and prio placement, throughput evaluator and prio timing (S1, D137), pricing, build sequencer, compactness measure (C1), ghost.

## Later (not in O4)
- Throughput confirming or loosening the layer-1 proxies (D108, D148): a tighter layout the rules forbid but the evaluator shows is fine. The route to hubs better than coop.
- Shared or optional prios beyond D159; attach mode; 4-way hubs (more anchors per line, more movements, same layers).
