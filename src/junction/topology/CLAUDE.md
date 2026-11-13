# Junction tool: topology/ (topology, graph, order, structure, programs)

- `junction_chain` holds the curve gap constraints (G1 §11.8).

## Notation and topology
- A line is written with `L` (one lane of one direction), `R` (the other direction) and `_` (one empty tile): LL_RR has two lanes each way with a one-tile gap (D38).
- Tracks of one direction are adjacent (gap 0); the underscores set the only gap between direction groups (D142).
- The notation is a default spacing; spacing may vary along a line (D38).
- Main line and branch each take their own notation (D60).
- A topology holds ports per side as lane groups (side, direction, lane count, line, gap to the next group) and movements (in group to out group) (G1 §1).
- Lanes are expanded by rule, not listed; unequal lane counts are allowed (D126, D135).
- Branch ports are on the S side only; N-side and both-side branches are refused as not ruled (G1 §8).
- Lane pairing for multi-to-multi movements is a topology option: any (default), keep, or per direction; it applies only where both groups of a turning movement have more than one lane (D221).
- Driving side follows the game's road-side setting; Mirror flips a placement (D66).
- Terms: game compass (N up), right-hand running; say "split" for any point where tracks diverge or join, "S-bend" for a sideways step, "shift" only for the coop shifter (D108).

## Graph, order and structure
- Splits come before merges along every track; outgoing lines split before incoming lines merge (D24, D189).
- A line splits by direction first, then by lane (line choice); a lane group's lane legs merge before the port on the exit side, and a joining branch splits by direction then by lane next to the main line it feeds (D187, D204, D205).
- Two legs of one branch bound for one main lane merge with each other first and join as one joiner, with one prio element per lane (D224).
- Every lane of a direction splits toward the branch; no shifters in LL_RR (D187, D191).
- A switch has two legs; a lane bound for more than two keys splits as a cascade of two-way nodes over contiguous halves of the ordered key list (1 + 2, 2 + 2, ...); merges mirror it (D228, DESIGN-G2 §G2.1).
- Crossings are counted per graph edge; two paths cross when their end ports interleave around the box; every chord pair is crossed exactly once (G1 §6, G1 §7).
- Over and under: a crossed row edge (a lane along its own row) carries one doubled bridge over all its crossings (D156, D190, G1 §9).
- A crossing of two non-row edges needs exactly one joiner (a path from a branch slot): the joiner goes over on a single bridge; zero or two joiners are refused as not ruled (G1 §9).
- Branch-on-branch crossings sit in the outermost band beside the branch ports (G1 §8).
- One bridge or tunnel may straddle several crossings of one path within its span limits, except that a band edge carries at most one bridge, over one crossing; placement refuses more (F3); the default is one bridge per crossed row edge, cut into series bridges at `JUNCTION_BRIDGE_MAX_CROSSERS` (2) crossers per bridge (D203, D209, G1 §13).
- Rows: lanes spread inside the box by S-bends to open rows for prio rows, doubling copies and joiner approaches, and return to the notation spacing at the ports (D203, D38).
- A prio sits opposite the joiner; the approach is on the joiner's side; a crossed lane's doubling copy shares the prio's row or sits away from the branch (G1 §9).
- A row change sits next to the feature it serves, and the free run lies on its other side (D212, D215).
- Exit lane legs of a direction merge in the outer band beside the branch port (D205, D210).

## Bridges and tunnels
- Every main-line bridge is doubled, whatever it crosses (D80, D174).
- A bridge carrying branch traffic is single (D174).
- A single branch bridge that carries a waiting bay counts as part of the bay (D174).
- A doubled bridge is a symmetric or parallelogram arrangement with equal pathfinder cost on both lanes; the shape is free (D100, D105).
- In the parallelogram, copy 1's heads sit one tile upstream of copy 0's, both spans equal, one signal tile between each split and its head (D118, D145).
- A bridge's crossers are packed exactly; heads hug the outermost crossers (G1 §11.8).
- A crossing set longer than the cap splits into consecutive bridges in series with one plain signalled tile between them; copies stay parallel with a signal each (D209).
- The series cap is `JUNCTION_BRIDGE_MAX_CROSSERS` = 2 crossers per bridge (D209).
- The longest bridge middle is the game's `max_bridge_length` setting, and a span is allowed when `ChooseJunctionBridgeType` finds a bridge for it (D208, `MakeJunctionSpec`).
- A placement whose doubled bridge needs a middle above that is refused naming the bridge (D208).
- Bridge type: the cheapest bridge at least as fast as the design speed; if no bridge reaches it on an unlimited rail type, the fastest bridge is used and trains slow on it (D35, D94, D141).
- A tunnel is a wormhole like a bridge: one block, no signals inside, surface track may cross the tiles between portals (D154).
- A tunnel is a `JunctionBridge` with `tunnel = true`; build steps, estimate pricing, layout transform and JSON, ASCII (`O` portals), the rig script, the line router, and in the graph pipeline the bridge structure, placement, render and the level solve branch on it (D154, D220).
- In the graph pipeline a `JunctionTrackBridge` has `tunnel`, valid on a single (not doubled) separation only; tokens Tunnel and Above mirror Over and Under; a tunnel longer than `max_tunnel_length` is refused naming it (D220, TU1-TU3).
- A crossing may be a tunnel or a bridge; tunnels have no span-height limit and suit wide topologies (D220).
- Tunnel portals are on Buildable, non-steep, free inclines; entry rises along travel with the track at its low edge; the exit is the first tile with the same low edge inclined the other way (D155).
- Tunnels cross other tunnels only at different depths; a tunnel carries a floor level `z` (D155).
- Tunnels may pass under towns and industries; bridges never span blocked tiles (D175).
- Bridges may span existing plain rail and plain road of any owner (D104).
- Tunnel placement, render, signals and level solve exist for a single separation; the hover never chooses a tunnel: only a test sets a crossing to tunnel (TU1-TU4).
