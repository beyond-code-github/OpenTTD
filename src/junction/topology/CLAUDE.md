# Junction tool: topology/ (topology, graph, order, structure, programs)

- `junction_chain` holds the curve gap constraints.

## Notation and topology
- A line is written with `L` (one lane of one direction), `R` (the other direction) and `_` (one empty tile): LL_RR has two lanes each way with a one-tile gap.
- Tracks of one direction are adjacent (gap 0); the underscores set the only gap between direction groups.
- The notation is a default spacing; spacing may vary along a line.
- Main line and branch each take their own notation.
- A topology holds ports per side as lane groups (side, direction, lane count, line, gap to the next group) and movements (in group to out group).
- Lanes are expanded by rule, not listed; unequal lane counts are allowed.
- Branch ports are on the S side only; N-side and both-side branches are refused as not ruled.
- Lane pairing for multi-to-multi movements is a topology option: any (default), keep, or per direction; it applies only where both groups of a turning movement have more than one lane.
- Driving side follows the game's road-side setting; Mirror flips a placement.
- Terms: game compass (N up), right-hand running; a split is a node where tracks diverge, a merge a node where they join (both topological); a merger is a coop pattern built from merges, prios and signals, and a prio is one merger style; "switch" is never used (the checker's "split inside prio coverage" covers both splits and merges); "S-bend" is a sideways step, "shift" only the coop shifter.

## Graph, order and structure
- Splits come before merges along every track; outgoing lines split before incoming lines merge.
- A line splits by direction first, then by lane (line choice); a lane group's lane legs merge before the port on the exit side, and a joining branch splits by direction then by lane next to the main line it feeds.
- Two legs of one branch bound for one main lane merge with each other first and join as one joiner, with one prio element per lane; on the exit side a main lane's splits among branch lanes sit in the band below its row, so each main row carries one in, one leg and one out.
- Every lane of a direction splits toward the branch; no shifters in LL_RR.
- A split or merge node has two legs; a lane bound for more than two keys splits as a cascade of two-way nodes over contiguous halves of the ordered key list (1 + 2, 2 + 2, ...); merges mirror it.
- The order sweep (from the branch side north) gathers a band node it reaches from its legs into one item: a merge's legs into its out edge, a split's legs into its in edge; the legs must be adjacent items, else the order fails.
- Crossings are counted per graph edge; two paths cross when their end ports interleave around the box; every chord pair is crossed exactly once.
- Over and under: a crossed row edge (a lane along its own row) carries one doubled bridge over all its crossings.
- A crossed out edge (the joiner merged before the crossing) carries its doubled bridge like the leg with the items swapped: copy 1 S-bends from the lane to the row's copy item before the bridge, copy 0 after it, and the fan leaves from the copy item; a crossed in edge is refused as not ruled.
- A crossing of two non-row edges: a joiner goes over a turning leg; of two joiners the one whose near end lies farther from the main line goes over; of two turning legs the one from the far side of the main line goes over; each on a single bridge. A through edge goes over: on its row it is the row edge's doubled bridge; a through edge off its row in a crossing is refused as not ruled.
- Branch-on-branch crossings sit in the outermost band beside the branch ports.
- One bridge or tunnel may straddle several crossings of one path within its span limits, except that a band edge carries at most one bridge, over one crossing; placement refuses more; the default is one bridge per crossed row edge, cut into series bridges at `JUNCTION_BRIDGE_MAX_CROSSERS` (2) crossers per bridge.
- Rows: lanes spread inside the box by S-bends to open rows for prio rows, doubling copies and joiner approaches, and return to the notation spacing at the ports.
- An S branch port edge starts (in) or ends (out) with a Fan token, on in and out ports alike: the west half of its group opens west, the east half east, the middle lane of an odd group (a one-lane group) has none, so the port block lies between its lanes' inside ends.
- A prio sits opposite the joiner; the approach is on the joiner's side; a crossed lane's doubling copy shares the prio's row or sits away from the branch.
- A row change sits next to the feature it serves, and the free run lies on its other side.
- A band edge's S-bend names the node it serves: Diverge (the split it leaves), Converge (the merge it joins, a joiner's arrival included), None (a crossing's).
- Exit lane legs of a direction merge in the outer band beside the branch port.

## Bridges and tunnels
- Every main-line bridge is doubled, whatever it crosses.
- A bridge carrying branch traffic is single.
- A single branch bridge that carries a waiting bay counts as part of the bay.
- A doubled bridge is a symmetric or parallelogram arrangement with equal pathfinder cost on both lanes; the shape is free.
- In the parallelogram, copy 1's heads sit one tile upstream of copy 0's, both spans equal, one signal tile between each split and its head.
- A bridge's crossers are packed exactly; heads hug the outermost crossers.
- A crossing set longer than the cap splits into consecutive bridges in series with one plain signalled tile between them; copies stay parallel with a signal each.
- The series cap is `JUNCTION_BRIDGE_MAX_CROSSERS` = 2 crossers per bridge.
- The longest bridge middle is the game's `max_bridge_length` setting, and a span is allowed when `ChooseJunctionBridgeType` finds a bridge for it (`MakeJunctionSpec`).
- A placement whose doubled bridge needs a middle above that is refused naming the bridge.
- Bridge type: the cheapest bridge at least as fast as the design speed; if no bridge reaches it on an unlimited rail type, the fastest bridge is used and trains slow on it.
- A tunnel is a wormhole like a bridge: one block, no signals inside, surface track may cross the tiles between portals.
- A tunnel is a `JunctionBridge` with `tunnel = true`; build steps, estimate pricing, layout transform and JSON, ASCII (`O` portals), the rig script, the line router, and in the graph pipeline the bridge structure, placement, render and the level solve branch on it.
- In the graph pipeline a `JunctionTrackBridge` has `tunnel`, valid on a single (not doubled) separation only; tokens Tunnel and Above mirror Over and Under; a tunnel longer than `max_tunnel_length` is refused naming it.
- A crossing may be a tunnel or a bridge; tunnels have no span-height limit and suit wide topologies.
- Tunnel portals are on Buildable, non-steep, free inclines; entry rises along travel with the track at its low edge; the exit is the first tile with the same low edge inclined the other way.
- Tunnels cross other tunnels only at different depths; a tunnel carries a floor level `z`.
- Tunnels may pass under towns and industries; bridges never span blocked tiles.
- Bridges may span existing plain rail and plain road of any owner.
- Tunnel placement, render, signals and level solve exist for a single separation; the hover never chooses a tunnel: only a test sets a crossing to tunnel.
