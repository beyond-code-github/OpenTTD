# RECON-foundations: rail on sloped tiles, from the game's code (2026-11-11)

Read: rail_cmd.cpp GetRailFoundation :311, _valid_tracks_without_foundation :261, _valid_tracks_on_leveled_foundation :283, CheckRailSlope :391, CmdBuildSingleRail :425; junction_terrain.h JunctionTerrainTile. Why: HF1's tile rule ("a split tile sits at z_max") was too loose; engineer: "the levelling foundation only works when placing the diagonal tile first. If a straight slope is already present then the action fails" and "there are other repercussions for foundation tiles, for example potentially needing terraforming on neighbouring tiles to continue the line".

## What the code says
- Validity is decided on the union of existing and new tracks (CheckRailSlope: GetRailFoundation(tileh, new | existing)); the code is order-independent. Adding a track that changes the foundation only costs a foundation. (The engineer's in-game observation of an order effect is not explained by this code path: open point, ask.)
- Inclined tile (two adjacent corners raised), e.g. SLOPE_NE:
  - the straight running up the slope is valid only without a foundation (a ramp) and only alone: it is not in the leveled set, so any combination with it is Invalid;
  - the straight across the slope and the two diagonals on the high side are valid on a leveled foundation (tile level at z_max), alone or combined (a split/merge across the slope is possible, at z_max);
  - the two diagonals on the low side are invalid on that tile in any combination.
- One corner raised / three corners raised / steep slopes: halftile and "anti-zig-zag" special foundations for single diagonals, leveled for others where the leveled set allows; steep: inclined foundations for a lone straight, SteepLower/SteepBoth/halftile for diagonals (table in GetRailFoundation).
- A foundation lifts the track to z_max (leveled; halftile/special lift the track's half). The track's neighbours along the line must meet it at that height: a lower flat neighbour needs a ramp (a lone straight up an incline) or terraforming.

## Consequences for the node model
- Tile facts must be per track set, not per piece kind: valid(tile, bits) = GetRailFoundation(slope, bits) != Invalid and (foundation None or build_on_slopes); the edge height of each track end = the height of that edge on the slope after ApplyFoundationToSlope. That is the game's own derivation (DESIGN-nodes §2 already named GetRailFoundation).
- The terrain snapshot keeps z_min, z_max, steep and incline only; one- and three-corner slopes cannot be told apart for the tables. It needs the canonical Slope per tile (rotated with the frame, D152), or the bits rotated back to map orientation before calling the game.
- The join rule of DESIGN-N3 §10 (consecutive tiles meet at one height) is unchanged; only the tile heights come from the game's tables. A failed join names both tiles: the terraform repair (TF1) can level either (lower the foundation tile, raise the neighbour), priced with corners and ranked with moving the split/merge.
