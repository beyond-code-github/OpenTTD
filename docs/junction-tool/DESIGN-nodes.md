# DESIGN-nodes: the map as a chain of typed edges, placed before projection (D182)

Status: DRAFT revision 2 (2026-11-11, after the engineer's review of revision 1). Nothing is briefed until it is agreed.
Inputs: RECON-nodes.md (inventory of today's nodes with file:line), DIAG-house.md, DIAG-ph1-limit.md, D170, D172, D178, D180, D182.

## 1. The problem
Today only B's nodes are solved (SolveJunctionSkeletonW), and from a crossing set measured on routes already made. A's split (A->S) and merge (N->A) have no variable: they are "any plain piece of A's routed raster in a range" (RECON N1, N2). Edges are routed one by one in a fixed order and frozen; RECON §3 lists 14 places where a later edge depends on an earlier edge's route. Each patch of the last two days (CS1/CS2 curve-state chaining, D172's after-check, NW1's shift-back retry, D181/D181a negotiation) answered one of those dependencies after the fact. DIAG-house: a house beside the hub sent A onto row 1 across N->A's merge window; growing the box could not help, so 33 depths x 17 nearby spots = 40 s per hover.

## 2. The model (engineer's rulings in this review)
- **Directed track graph.** Nodes: ports, splits, merges, crossings, and the boundaries between edges. Edges in travel direction. Each movement is a path in-port -> out-port; no cycles.
- **Each line is a chain of typed edges; each edge type encodes its own length rules** (engineer: "an edge encodes the curve length rules. So for example after a curve edge completes, the next straight edge must be at least N tiles before the next node (but optionally more). Same for s-bends"). Types: straight, curve (45 deg), S-bend (one-row shift), diagonal run, ramp, bridge, tunnel. Nodes with extra requirements: split, merge (prio coverage straight before it, prio row beside it), crossing (D172 span window).
- **No detour pairs.** An S-bend shifts the line; it may stay shifted through later edges and nodes and shift back later or never, as cost decides (engineer: "we may not rejoin at all - we may opt to stay in the shifted position to account for cost or a later obstacle"). A merge may sit on row 1 if A is on row 1 there.
- **Obstacles add structure to the map so it is always consistent** (engineer: "we actually add nodes/edges to the map when we encounter obstacles so that the map is always consistent"). An obstacle is a condition: the straight edge covering that column is not on that row; which S-bend satisfies it is a choice by cost.
- **No "segment failed between two nodes" outcome** (engineer: "There should be no 'segment failed between two nodes' possibility"). A consistent map lays out without failure; infeasibility is found in the solve ("move the cursor"). A router failure on a laid-out edge is a model bug caught by tests, never a runtime retry.
- **Heights.** On the surface, track height is not a choice: the game derives it from slope and track bits (GetRailFoundation, rail_cmd.cpp:311). So surface heights are per-tile facts known before the solve (curve on an incline needs a foundation; a switch tile flat or level foundation; a piece on an incline runs along it; neighbouring tiles connect only at matching edge heights). They enter the solve like obstacles. Free heights exist only at bridges and tunnels, which are therefore map edges with head/portal nodes, decided in the map, never invented by the router. Terraforming (D78, later) makes surface heights free and is out of scope.

## 3. The solve: two layers
1. **Discrete layer (structure).** Per line: the sequence of edge types and the row of each straight run (where S-bends, ramps, bridges, tunnels sit). Start from the flat T's structure; add an S-bend / ramp / bridge / tunnel edge only when a per-tile fact forces it; rank candidate structures by a cost bound.
2. **Continuous layer (positions).** For a fixed structure, node positions along their lines are integers under difference constraints (x_i - x_j >= k / <= k): each edge type's minimum length (optionally more), node requirements (merge - split >= prio_tiles + 2 + phase; crossing columns within D = nmax - 3; heads clear of the crossing), and per-tile facts ("an edge of type T may not cover tile t") resolved to bounds for that structure. Solved by a shortest-path pass (SolveJunctionSkeletonW already does this for B's nodes, microseconds); a negative cycle = this structure is infeasible.
3. **Lay out** each edge from its type and fixed ends; heights follow the tiles; bridges/tunnels as placed.
4. **Box depth** is a variable of the solve (the stub nodes' row), not an outer loop over depths.

## 4. Edge-type rule table (to derive exactly, each with its source)
straight (min N after a curve or S-bend: rule 3 / DeriveCurveSpacing), curve, S-bend (S-bend window 2*tl units to the next, CS2), diagonal (>= rows changed), ramp (straight along an incline, D152), bridge (span limits, head rules G5, D80 doubling, D172), tunnel (max length, portal rules, D174/D175), switch requirements (signal-legal piece before a split / after a merge, D115a, D117a, G6), prio coverage (prio_tiles straight before an ML merge, D108, D148, D148a phase), waiting room / whole-train bays (G7, D176). First step test: the flat T rebuilt as a chain of typed edges must be identical to today's.

## 5. Worked case: house at (39, 0)
xI 40, prio_tiles 14, flat AS (A->S split) 36, X_B 38..39, flat NA (N->A merge) 52. The straight A edge covering x = 39 may not be on row 0 -> the discrete layer inserts an S-bend on A before 39 (A to row 1). Cost then decides: shift back to row 0 after the house (another S-bend, at least N straight tiles later; NA's prio coverage then starts after it) or stay on row 1 and merge there (the prio row is then row 0, which must be usable over the coverage). The positions solve places AS, the S-bend(s) and NA together; nothing drifts.

## 6. Why this is not D169 again
D169 modelled every tile (183k variables; no layout in 120 s). Here: per structure about a dozen integers per line, difference constraints, microseconds; crossings and over/under fixed by the map (D167, D170). O4c2 routed main lines first and found no legal merge: here nodes are placed before anything is laid out. SoverN's per-edge envelopes: here each constraint comes from an edge or node type, the same everywhere.

## 7. Open (for the engineer, after the performance review)
- Discrete search: fewest added edges first, by cost bound; branching only on infeasibility.
- Obstacle grouping (bounding boxes vs outlines) for the discrete layer.
- v1 scope: A and B lanes and stubs (line edges), footprints, ramps; branch edges (A->S, N->A as chains) next; bridges/tunnels as map edges.

## 8. Scale targets (D124, D151)
Hover generation < 100 ms (D151). Ultimate goal: fast human building and LLM-driven building of networks at the scale of a 512x1024 map with ~24k rail tiles, ~900 bridges, ~8.8k signals; thousands of deterministic generation calls; LL_RR and multi-line junctions (D176) next.
