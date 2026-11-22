# RECON: the domain's rules against the game's code (2026-11-23)

Read-only check of `CheckPlanarDomain`'s rules against OpenTTD's source (TB = src/tunnelbridge_cmd.cpp; line numbers approximate). Asked by the engineer: "Can we double check these rules against the actual game mechanics?"

| # | Domain rule | Verdict | Evidence | pinned in game |
|---|---|---|---|---|
| 1 | Pieces on edge-adjacent tiles that both touch the shared edge are connected; otherwise not | game | pathfinder/follow_track.hpp Follow(): `new_td_bits &= DiagdirReachesTrackdirs(exitdir)`; track_func.h 450-480 | `domain pin 1 (rule 1)` |
| 2 | Upper/Lower and Left/Right are the only same-tile pairs that touch no common edge; every other pair is a switch | game | track_func.h 540 `TracksOverlap`; rail_cmd.cpp 240-260 `CheckTrackCombination` allows any combination | not pinned |
| 2b | No X/Y at-grade diamond | **design (D182)** | the game builds X/Y on one tile; trains cannot turn between them (`TrackdirCrossesTrackdirs`) | `domain pin 3 (rule 2b)` |
| 3 | A bridge or tunnel head tile holds nothing else; its side edges connect nothing | game | TB 1851-1858 `GetTileTrackStatus_TunnelBridge` answers only through its mouth; rail on a head is refused via `ClearTile_TunnelBridge` (TB 1025-1031) | `domain pin 4 (rule 3)` |
| 4 | Plain rail of any direction may run under a bridge middle; flat heads give a deck one level up | game | `GetBridgeHeight` bridge_map.cpp 76-82; `CheckBuildAbove_Rail` rail_cmd.cpp 3233 accepts plain rail | `domain pin 5 (rule 4)` |
| 5 | Two bridge middles may not share a tile | game | TB ~508 `if (IsBridgeAbove(tile)) ... MUST_DEMOLISH_BRIDGE_FIRST` | `domain pin 6 (rule 5)` |
| 6 | A tunnel needs 2 exclusive pieces per end on flat ground | game, with terraform | `CmdBuildTunnel` (TB 641-770): the portal must be an inclined slope; the tunnel runs at the portal's low height to the first tile at that height. Under a track kept at h, each portal has its two outer corners lowered to h-1. The portal is the only structure tile per end, but the lowering inclines the next tile out (the ramp), which can then carry only the axis track along its slope (rail_cmd.cpp 261 `_valid_tracks_without_foundation`). So the separator owns portal + ramp, and no other track fits on either at that level. Shortest tunnel: adjacent portals are legal. | `domain pin 7 (rule 6)` |
| 7 | Tunnel middles may share a tile with another tunnel's middle (heights decide); a bridge may pass over a tunnel middle | game, partly | `IsTunnelInWayDir` (tunnel_map.cpp 48-62, TB 711) refuses a perpendicular tunnel at the same depth only; nothing stops a bridge over a tunnel | not pinned |
| 8 | Surface rail may cross a tunnel's middle tiles | game | `CmdBuildSingleRail` has no tunnel check; lowering land above a tunnel is refused (terraform_cmd.cpp ~250) | not pinned |
| 9 | At most 45 degrees between consecutive pieces | **design, matching a default setting** | follow_track.hpp ~168 `Rail90DegTurnDisallowed`; `pf.forbid_90_deg` defaults true; rail types may override | not pinned |
| 10 | A switch (several tracks from one edge midpoint) is legal | game | `CheckTrackCombination` accepts it | `domain pin 2 (rule 10)` |

Consequences for the domain:
- Its "legal" mixes game facts with two design rulings (2b, 9) and the planar model's own rule that only listed crossers pass under a deck ("under"). Reports should say which kind each break is.
- Same-depth perpendicular tunnels are illegal in the game; the domain's "tunnels decide by heights" is right only with different depths, which Heights must guarantee.
- Pinned: each hand case is built in the game with its own commands on a flat map (`[planardomaingame]`, junction_game.cpp); the game's BuildRail needs the Auto flag a real dispatch gives it, else a direct call demolishes a bridge head instead of refusing (pin 4).
