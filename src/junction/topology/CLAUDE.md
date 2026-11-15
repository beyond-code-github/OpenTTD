# Junction tool: topology/ (topology, graph, order, structure, programs)

- `junction_chain` holds the curve gap constraints.

## Notation and topology
- A line is written with `L` (one lane of one direction), `R` (the other direction) and `_` (one empty tile): LL_RR has two lanes each way with a one-tile gap.
- Tracks of one direction are adjacent (gap 0); the underscores set the only gap between direction groups.
- The notation is a default spacing; spacing may vary along a line.
- Main line and branch each take their own notation.
- A topology holds ports per side as lane groups (side, direction, lane count, line, gap to the next group) and movements (in group to out group).
- Lanes are expanded by rule, not listed; unequal lane counts are allowed.
- `prio_movements` (D282, D298) lists the movements whose paths hold priority at their merges; empty means every main-to-main path does. A pattern block lists only the movements its prio elements serve.
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
- A bundle graph (D289) is the track graph of the topology with every group as one lane: each movement is one bundle, every edge as wide as the lane count, split nodes exit blocks and merge nodes join blocks; all groups need the same lane count.
- Crossings are counted per graph edge; two paths cross when their end ports interleave around the box; every chord pair is crossed exactly once.
- Over and under: a crossed row edge (a lane along its own row) carries one doubled bridge over all its crossings.
- A crossed out edge (the joiner merged before the crossing) carries its doubled bridge like the leg with the items swapped: copy 1 S-bends from the lane to the row's copy item before the bridge, copy 0 after it, and the fan leaves from the copy item.
- A crossed in edge carries its doubled bridge as the mirror: its fan arrives on whichever of the lane and copy items the lane's first node does not sit on, copy 1 S-bends to the node's item before the bridge, copy 0 after it; an in edge's bridge does not double the leg.
- A crossing of two non-row edges: a joiner goes over a turning leg; of two joiners the one whose near end lies farther from the main line goes over; of two turning legs the one from the far side of the main line goes over; each on a single bridge. A through edge goes over: on its row it is the row edge's doubled bridge; a through edge off its row in a crossing is refused as not ruled.
- A diverging joiner that crosses starts with a Diverge S-bend, then crosses and arrives as any joiner; only a diverging joiner with no crossings starts with a Corner.
- Two joiners crossing at the same main-line distance: the one from the lower (west) branch in slot goes over.
- Branch-on-branch crossings sit in the outermost band beside the branch ports.
- One bridge or tunnel may straddle several crossings of one path within its span limits, except that a band edge carries one single bridge per crossing, in series along it, and placement refuses a single bridge over several band crossings; the default is one bridge per crossed row edge, cut into series bridges at `JUNCTION_BRIDGE_MAX_CROSSERS` (2) crossers per bridge.
- The programs emit each node's shape (its stack item, trunk heading, trunk, straight and diverging legs with the diverging hand) and each edge's two ends (heading, and the token whose end is the node's point, if any).
- The arrangement is built from the order and the programs: each row's consecutive items west to east, each band's touching events of a higher layer strictly north, and per Under token of a doubled bridge the pieces that may pass under it (the runs either side, the previous bend's tail, the next bend's head).
- The sweep records each band's line of edges before and after every event. The arrangement lists each event's replacement block (the contiguous edges it takes out of the line and the ones it puts in), every pair of edges that are neighbours in some order of the band's events the layers allow (marked when only another order than the sweep's makes them neighbours), and every unordered pair of events whose edges can be neighbours.
- The arrangement cuts each band edge at its ends, its crossings with band edges, its passes under a row's doubled bridge and its free bends (bends serving no end or crossing); the segments between cuts lie in one band or on a row's item rows; each bend names the cut it serves, whether it approaches it, and the edges it moves toward.
- Rows: lanes spread inside the box by S-bends to open rows for prio rows, doubling copies and joiner approaches, and return to the notation spacing at the ports.
- An S branch port edge starts (in) or ends (out) with a Fan token, on in and out ports alike: the west half of its group opens west, the east half east, the middle lane of an odd group (a one-lane group) has none, so the port block lies between its lanes' inside ends.
- A prio sits opposite the joiner; the approach is on the joiner's side; a crossed lane's doubling copy shares the prio's row or sits away from the branch.
- A row change sits next to the feature it serves, and the free run lies on its other side.
- A band edge's S-bend names the node it serves: Diverge (the split it leaves), Converge (the merge it joins, a joiner's arrival included).
- A band edge crossed by a single bridge carries two forms of the crossing, an S-bend under the bridge and a square run (a Corner, the crossings, a Corner back); consecutive crossings of one hand share one run; the placement's choices pick the form; edge ends and node shapes never name a crossing's bends.
- Exit lane legs of a direction merge in the outer band beside the branch port.

- A bundle graph (`JunctionGraphWidth` 2) has, per row, the stack Lane, Copy and two BlockSide items (the order follows the row's heading, so track 1 lies to the right of travel), no Prio and no Approach; `RowItems::block_side` names the BlockSide item of each track.
- At width 2 a row edge carries track 1 as its copy program, the same Fan and Over tokens as track 0, with no lane split or merge.
- A band edge of a bundle graph names no token at a block node: its start from a band node and its end at a band node are empty, and its arrival on a row node is a Corner to the BlockSide item of track 0 without a Converge.
- A band edge of a bundle graph carries track 1 in `programs.copies[e]`: track 0's tokens, each arrival Corner on the row's BlockSide item of its own track (track 1's Corner names `block_side[1]`). `ArrangeJunctionTrackGraph` reads track 0 only (track 1 is its translation); a copy on a width-1 edge other than a doubled one stays `NOT_REACHED`. `JunctionEdgeTracks` gives 2 for every edge of a bundle graph and for a doubled edge, else 1.
- A block node's footprint (D289, D291) is its length along its trunk's heading from its anchor (the trunk's track 0 at the upstream end) and its width across in lines: Plain is 1 x 1, Exit and Join are the spec's `exit_block_along` / `join_block_along` (placeholders until the templates, 0 refuses a bundle placement) by 2 x width.
- A block's diverging leg lies on its side (`JunctionBlockSide`), seen along the trunk's travel: a row node's and a band split's is the shape's hand, a band merge's the opposite. An edge's track k attaches at along -1 (upstream of the footprint) or footprint.along (downstream): a split's trunk at -1 and its legs downstream, a merge's legs at -1 and its trunk downstream; across k for the trunk and the straight leg, width + k (side right) or k - width (side left) for the diverging leg (`JunctionBlockAttachOf`).
- A block end never names a token; a bundle wider than two tracks, a plain node in a bundle graph, and a row block whose diverging leg goes north are refused as not ruled.

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
