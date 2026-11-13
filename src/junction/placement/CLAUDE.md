# Junction tool: placement/ (horizontal placement, render, positions solver, level solve)

- `junction_placement` is the horizontal placement and `junction_render` rasterises it to routes and a layout; `junction_positions` solves difference constraints (`JunctionPositions`); `junction_levels` is the level solve.

## Placement
- Placement is relations plus terrain facts solved together; no flat positions enter the terrain solve.
- Every relation is a rule floor "at least N", scaling with train length and speed; each entry cites its rule.
- Relations are rule families applied everywhere, never per site: every curve-gap pair on every route, and unrelated tracks never share or touch a tile.
- Two unrelated pieces share a tile only as Upper|Lower or Left|Right (`JunctionTracksMayShareTile`).
- Site floors and per-shape constants exist only as test oracles.
- Every edge is one chain of elements; one floor table keyed by (previous kind, next kind) holds every run constant.
- A band edge's chain starts on its row's heading when it leaves a row node, else south when it carries exit paths and north when it carries joiner paths.
- A band chain's start heading, which of its bends begins or ends exactly on a node's point, and a row node's stack item are read from the programs' edge ends and node shapes; placement infers none of them from token order.
- Only a node's serving bend shares its point: a Diverge S-bend or a Corner leaving a split, a Converge S-bend joining a merge; any other bend next to a node has a run of at least its floor between them.
- Curve gaps: consecutive same-hand bends at least `min(2TL - 1, max(2, CL))` in units along the path, opposite hands at least 2, non-consecutive at least `min(2TL - 1, (j - i) CL)` (`JunctionCurveGaps`).
- Curve length CL is derived from the train, not set: the smallest bend spacing (1..12) whose curve speed limit is at least the design speed, capped at 2 TL (`DeriveCurveSpacing`).
- A diagonal half piece counts as half a tile for CL.
- A split or merge is the tile of its first diverging or last joining half piece.
- Terrain facts are bounds on the X phase: a straight run on a row is buildable on every tile between its points, heights included.
- A mid-row obstacle is never absorbed by positions; it needs a graph edit.
- Rows (Y) come from the structure and change only by an edit named by an X failure.
- Objective within a graph: the least solution, then each row change pushed toward its feature.
- Stub length at a branch port is two tiles: the signal tile then the boundary tile; the stub is one rule constant.
- An S branch port's fan is a vertical S-bend sized by the solve, 0 columns allowed (then it lies on the port's run); the ports keep the notation spacing.
- Every layer-1 node of band R lies at least a port stub above the S edge.
- A row node lies on the stack item of its node shape (the last item token of its trunk for a split, of its straight leg for a merge); bridge tokens name edges, not items.
- A Fan, Corner or S-bend sized to 0 columns is no bend: it adds no pieces, occupancy or bend bits.
- A single bridge over an S-bend may cross it at any sideways column the S-bend's diagonal allows; the joint model chooses that column.
- The junction footprint ends one tile beyond the outermost signal on each leg; every track of a leg ends flush at the footprint edge.
- Planar signal windows and node footprints: a node has a footprint from the rules (the split or merge tile, required straight runs, prio coverage) and a window along its line.

## Heights
- Height is solved per tile on the rendered routes after placement.
- A tile can be a ramp only when it holds a lone axis track that every route crosses the same way and is not a node, port, head or route end; a ramp climbs or drops at most one level (`SolveTileLevels`).
- A track piece running along a tile's incline is a ramp and is never levelled; a lone diagonal piece gets a levelled foundation only on a slope with three corners raised; on any other slope it gets a halftile foundation (when the corners next to its track corner are lowered) or the special anti-zig-zag foundation (`GetRailFoundation`).
- Curves and diagonals cannot sit on a ramp tile.
- Ramp rule: along every route the level gain over any train length is at most `k_max = floor(tl x S(v) / (m x s x 100))` in the travel direction; descents are free.
- The junction tool requires the realistic acceleration model and refuses to generate with the original model, saying why.
- The level solve costs terraform exactly by clamping the ground to the cones of fixed track corners and descends to the cheapest levels.
- Every corner a plan ends at lies in [0, map height limit], as the game's terraform allows; the corner cone carries the bound (`terraform_cmd.cpp`).
- The hover plan carries a level for every track tile, prio tiles included.
- A bridge head may sit on the slope rising to its span; the ground under a span stays at or below the heads.
- The ramp rule counts the climb onto and across bridges: a flat head climbs onto the deck, an inclined head sits at it.
- Tunnel level plan: portals on the game's raw slope rising into the tunnel, the outer edge at level L, ground over the middle at least L+1, the game's exit search ends at the planned portal.
- The level solve never asks the game about an impossible slope; it returns a plan or a reason.
- Each port has its own height, derived from its tile and the piece's direction as the game builds track.
- Linear and integer programs go through `JunctionMilp`, the one seam to the solver (HiGHS); it solves quietly, exactly and deterministically (one thread, zero gap, fixed seed).
- Placement can record its last X and Y relation systems with each constant's dependence on a bend's length (`RecordJunctionPlacementSystem`); `SolveJunctionJoint` solves both axes and every bend length together as one integer program whose objective is the flat build cost.
- A recording can be given a point (a column and a row per placement point). It then detects occupancy, the hug pins and the clearance at that point instead of at its own flat solves, keeps recording when one of its solves fails (naming the failure), and returns its own solution (least rows, then least columns with the push). `SolveJunctionJointLoop` records at the joint solution's canonical point and solves again until a round keeps no new clash and changes no length.
- The joint model sizes a W/E port fan from the rows: its length is a variable tied to the span from its stack item to its port row, the side as data (from the fan's hand and whether it starts on its port); a fan recorded empty only bounds the length below by that span. The loop records at the previous recording's own solution when it has one, else at the joint solution only when the system it came from had X relations, else at no point.
- The joint model chooses the order of an occupancy pair whose order came from a joint solution (either track may come first, at its own gap); a tie, and a pair kept from a solved placement, keep their recorded order and offset.
