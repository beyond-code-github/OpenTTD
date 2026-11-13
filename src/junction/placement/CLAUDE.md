# Junction tool: placement/ (horizontal placement, render, positions solver, level solve)

- `junction_placement` is the horizontal placement and `junction_render` rasterises it to routes and a layout; `junction_positions` solves difference constraints (`JunctionPositions`); `junction_levels` is the level solve (D216, D218).

## Placement
- Placement is relations plus terrain facts solved together; no flat positions enter the terrain solve (D216, G1 §14.1).
- Every relation is a rule floor "at least N", scaling with train length and speed; each entry cites its rule (G1 §14.3, G1 §16.1).
- Relations are rule families applied everywhere, never per site: every curve-gap pair on every route, and unrelated tracks never share or touch a tile (D217).
- Two unrelated pieces share a tile only as Upper|Lower or Left|Right (D182, D217; JunctionTracksMayShareTile).
- Site floors and per-shape constants exist only as test oracles (D217).
- Every edge is one chain of elements; one floor table keyed by (previous kind, next kind) holds every run constant (G1 §16.1, RW1-RW6).
- Curve gaps: consecutive same-hand bends at least `min(2TL - 1, max(2, CL))` in units along the path, opposite hands at least 2, non-consecutive at least `min(2TL - 1, (j - i) CL)` (`JunctionCurveGaps`, G1 §11.8).
- Curve length CL is derived from the train, not set: the smallest bend spacing (1..12) whose curve speed limit is at least the design speed, capped at 2 TL (`DeriveCurveSpacing`, D67).
- A diagonal half piece counts as half a tile for CL (D36).
- A split or merge is the tile of its first diverging or last joining half piece (G1 §11.8).
- Terrain facts are bounds on the X phase: a straight run on a row is buildable on every tile between its points, heights included (G1 §14.4).
- A mid-row obstacle is never absorbed by positions; it needs a graph edit (G1 §14.4).
- Rows (Y) come from the structure and change only by an edit named by an X failure (G1 §14.4).
- Objective within a graph: the least solution, then each row change pushed toward its feature (D215, G1 §14.5).
- Stub length at a branch port is two tiles: the signal tile then the boundary tile; the stub is one rule constant (D115a, D206).
- The junction footprint ends one tile beyond the outermost signal on each leg; every track of a leg ends flush at the footprint edge (D58).
- Planar signal windows and node footprints: a node has a footprint from the rules (switch tile, required straight runs, prio coverage) and a window along its line (D182).

## Heights
- Height is solved per tile on the rendered routes after placement.
- A tile can be a ramp only when it holds a lone axis track that every route crosses the same way and is not a node, port, head or route end; a ramp climbs or drops at most one level (`SolveTileLevels`).
- A track piece running along a tile's incline is a ramp and is never levelled; a lone diagonal piece gets a levelled foundation only on a slope with three corners raised; on any other slope it gets a halftile foundation (when the corners next to its track corner are lowered) or the special anti-zig-zag foundation (D152b, `GetRailFoundation`).
- Curves and diagonals cannot sit on a ramp tile (G1 §15.4).
- Ramp rule: along every route the level gain over any train length is at most `k_max = floor(tl x S(v) / (m x s x 100))` in the travel direction; descents are free (D219, G1 §15.4).
- The junction tool requires the realistic acceleration model and refuses to generate with the original model, saying why (D219).
- The level solve costs terraform exactly by clamping the ground to the cones of fixed track corners and descends to the cheapest levels (HC1, P4b, P6b).
- The hover plan carries a level for every track tile, prio tiles included (H3).
- A bridge head may sit on the slope rising to its span; the ground under a span stays at or below the heads (IG3).
- The ramp rule counts the climb onto and across bridges: a flat head climbs onto the deck, an inclined head sits at it (IG4).
- Tunnel level plan: portals on the game's raw slope rising into the tunnel, the outer edge at level L, ground over the middle at least L+1, the game's exit search ends at the planned portal (TU4).
- The level solve never asks the game about an impossible slope; it returns a plan or a reason (P3b).
- Each port has its own height, derived from its tile and the piece's direction as the game builds track (D180).
