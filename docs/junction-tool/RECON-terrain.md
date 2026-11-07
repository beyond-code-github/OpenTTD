# Recon: terrain for place-time generation (D78)

Read-only recon, 2026-11-08, at head 5fb6207f22. All paths are under `src/`. Heights are in height levels (TILE_HEIGHT = 8 world units).

## Rail on slopes

`CmdBuildSingleRail` (rail_cmd.cpp:425) calls `CheckRailSlope(tileh, bits, existing, tile)` (:391), which uses `GetRailFoundation(Slope, TrackBits)` (:311) and the tables `_valid_tracks_without_foundation` and `_valid_tracks_on_leveled_foundation`.

| Ground and track | Foundation | Cost |
|---|---|---|
| Flat, any track | none | 0 |
| A track along the contour of a one-corner or ridge slope | none | 0 |
| Leveled bits (a 3-corner or inclined slope with any track, or a 1-corner slope with its upper track or half pair) | leveled | `Price::BuildFoundation` (250 base, flat per tile) |
| A single X or Y track on a 1-corner slope | inclined (sloped track) | +250 |
| A half track on a slope | halftile or special | +250 |
| Steep slopes | only X/Y inclined, or a single higher-corner halftile | |
| Anything else | invalid | |

- Foundations need `construction.build_on_slopes`.
- Adding a track that changes a tile's foundation charges the 250 again.
- Use `ApplyFoundationToSlope(f, tileh)` to get the z delta after the foundation.

## Bridges (tunnelbridge_cmd.cpp)

**Heads:**
- `GetBridgeFoundation(tileh, axis)` (:111) gives no foundation on flat ground, or on a ridge whose contour runs along the axis. Otherwise it is inclined (one highest corner) or flattening.
- `CheckBridgeSlope` (:209) requires the post-foundation slope to be flat or the valid inclined slope for that head:
  - north head: NE (X) or NW (Y);
  - south head: SW (X) or SE (Y).
- The post-foundation heights of the two heads must be equal (BRIDGEHEADS_NOT_SAME_HEIGHT, :381).
- Each head is cleared with `LandscapeClear`.

**Span tiles:**
- Each needs `GetTileMaxZ(t) <= z_head` (the deck is at z_head+1).
- Each needs `z_head < GetTileZ(t) + construction.max_bridge_height` (default 12).
- No bridge may already be above.
- `CheckBuildAbove` decides what may be underneath:

| Under the span | Rule |
|---|---|
| Plain rail, road, plain water, coast, canal | OK |
| Depots, waypoints, locks | extra height needed |
| Houses, trees, clear land, industries (no handler) | auto-cleared and charged |

`PriceBridge` adds the body cost itself, because QueryCost doesn't include it.

## Clearing (landscape.cpp:688 `CmdLandscapeClear`)

| What is on the tile | Cost or rule |
|---|---|
| Grass | 0 for density 0; rough 40; rocks 200; fields 500 |
| Trees | 20 per tree (1–4), x4 for rainforest; town rating drops |
| Houses, through BuildRail/BuildBridge (Auto set) | always fail (BUILDING_MUST_BE_DEMOLISHED) |
| Houses, through a direct `LandscapeClear` (`TestClear`, no Auto) | allowed if `CanDeleteHouse` and the town rating permits; cost is `GetRemovalCost` (ClearHouse 1600 base) |
| Sea, canal, river | ClearWater 10000 / ClearCanal 5000, but BuildRail (NoWater) fails on flat water |
| Coast, and the low side of a 1-corner slope | buildable |
| Water under a bridge span | fine |
| Another company's tiles | fail |
| Some objects (HQ, transmitter) | unclearable |

- **Own existing plain rail (:437–470):** BuildRail adds the track bits if the rail type is compatible, the combination is valid and no train is on it, with no clearing cost. Re-adding a bit that already exists fails ("already built").
- **Router consequence:** treat own tiles as occupied-but-joinable.
- **Gotcha:** be consistent between the Auto path (BuildRail/BuildBridge, where houses fail) and the non-Auto path (`TestClear`, where houses may be allowed).

## Queries (tile_map.h:268–291)

| Function | Returns |
|---|---|
| `GetTileSlopeZ(t)` | (slope without halftile bits, min corner z) |
| `GetTileZ` | min corner z, in levels |
| `GetTileMaxZ` | max corner z, in levels |
| `IsTileFlat(t, &h)` | whether all 4 corners are equal (ignores foundations and contents) |

Steep means the SLOPE_STEEP bit (max − min = 2). Neighbours of edge tiles are clamped to the map.

## The existing seam

- **Interface:** `JunctionPricing` (junction_layout.h:66–81), implemented by `GameJunctionPricing` (junction_game.cpp).

  | Method | What it does |
  |---|---|
  | `GetFlatHeight` | flat-only; the only terrain query |
  | `TestRail` | a real BuildRail QueryCost: slope, foundation, clearing, ownership, houses fail |
  | `PriceBridge` | a BuildBridge QueryCost plus the body cost |
  | `TestClear` | LandscapeClear, no Auto |
  | `SignalCost`, `AvailableMoney` | |

- **`EstimateJunction`** (junction_layout.cpp:195–296) requires every occupied tile to be flat at one reference height (FLAT_LAND_REQUIRED). That is the hard assumption to replace. Terrain otherwise enters only indirectly, through `TestRail`/`PriceBridge`.

## Terraforming

| Command | Notes |
|---|---|
| `CmdTerraformLand(flags, tile, corners, up)` (terraform_cmd.cpp:172) | test-safe; 250 per corner, plus clearing; `terraform_limit` applies |
| `CmdLevelLand(flags, tile, start, diagonal, mode)` (:326) | levels an area to the start tile's height; NoTest; a QueryCost run works but may overcount; ALREADY_LEVELLED when there is nothing to do |

## A seam for D78 could expose, per tile

- slope and z;
- the rail verdict and foundation cost for a TrackBits set (through `TestRail`, or `GetRailFoundation` plus the tables);
- the clear cost and verdict, consistent with Auto;
- bridge-head validity and post-foundation z, plus span clearance (`GetTileMaxZ(span) <= head_z` and `max_bridge_height`);
- own-track joinability.
