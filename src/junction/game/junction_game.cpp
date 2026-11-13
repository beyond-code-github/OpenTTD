/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_game.cpp Game-side pricing for the junction tool. */

#include "../../stdafx.h"

#include "junction_game.h"
#include "../legacy/junction_generator.h"
#include "../junction_rules.h"
#include "../legacy/junction_nodes.h"
#include "../../settings_type.h"
#include "../../command_func.h"
#include "../../rail_cmd.h"
#include "../../station_cmd.h"
#include "../../newgrf_station.h"
#include "../../landscape_cmd.h"
#include "../../tunnelbridge_cmd.h"
#include "../../terraform_cmd.h"
#include "../../bridge.h"
#include "../../tunnelbridge.h"
#include "../../signal_func.h"
#include "../../track_func.h"
#include "../../rail.h"
#include "../../engine_base.h"
#include "../../engine_func.h"
#include "../../economy_func.h"
#include "../../company_func.h"
#include "../../tile_map.h"
#include "../../debug.h"
#include "../../rail_map.h"
#include "../../road_map.h"
#include "../../slope_type.h"
#include "../../slope_func.h"
#include "../../map_func.h"
#include "../../tile_type.h"
#include "../../train.h"
#include "../../strings_func.h"
#include "../../table/strings.h"

#include <array>

#include "../../safeguards.h"

std::optional<JunctionPickedTrain> ReadJunctionTrain(const Vehicle *v)
{
	if (v == nullptr || v->type != VehicleType::Train || v->owner != _local_company) return std::nullopt;
	const Train *first = Train::From(v)->First();

	JunctionPickedTrain picked;
	uint32_t mass = 0;
	for (const Train *part = first; part != nullptr; part = part->Next()) mass += static_cast<const Vehicle *>(part)->GetMaxWeight();
	picked.loaded.mass_t = std::max<uint32_t>(mass, 1);
	picked.loaded.power_hp = first->gcache.cached_power;
	picked.loaded.max_te = first->gcache.cached_max_te;
	picked.loaded.air_drag = first->gcache.cached_air_drag;

	/* GetDisplayMaxSpeed() is vcache.cached_max_speed, the same unit as cur_speed; so is the rail type's max_speed. */
	uint32_t speed = first->GetDisplayMaxSpeed();
	const uint16_t rail_max = GetRailTypeInfo(GetRailType(first->tile))->max_speed;
	if (rail_max != 0) speed = std::min<uint32_t>(speed, rail_max);
	picked.loaded.max_speed = static_cast<uint16_t>(speed);

	picked.tl = static_cast<uint8_t>(Clamp<uint32_t>(CeilDiv(first->gcache.cached_total_length, TILE_SIZE), 2, 12));
	picked.name = GetString(STR_VEHICLE_NAME, first->index);
	for (const Train *part = first; part != nullptr; part = part->Next()) picked.engines.push_back(part->engine_type);
	return picked;
}

/**
 * Get the top speed of the fastest train engine the local company can build that runs on a rail type.
 * @param railtype The rail type.
 * @return The speed, or 0 if there is no such engine.
 */
static uint GetMaxBuildableEngineSpeed(RailType railtype)
{
	uint target = 0;
	for (const Engine *e : Engine::IterateType(VehicleType::Train)) {
		if (!IsEngineBuildable(e->index, VehicleType::Train, _local_company)) continue;

		const RailVehicleInfo &rvi = e->VehInfo<RailVehicleInfo>();
		if (rvi.railveh_type == RailVehicleType::Wagon) continue;
		if (!IsCompatibleRail(rvi.railtypes, railtype)) continue;

		target = std::max<uint>(target, rvi.max_speed);
	}
	return target;
}

/**
 * Get the speed the junction is designed for: the rail type's limit, or the fastest buildable engine when unlimited (D45, D67).
 * @param railtype The rail type.
 * @return The speed in train speed units; 160 if there is neither a limit nor an engine.
 */
uint16_t GetJunctionReferenceSpeed(RailType railtype)
{
	uint speed = GetRailTypeInfo(railtype)->max_speed;
	if (speed == 0) speed = GetMaxBuildableEngineSpeed(railtype);
	return speed == 0 ? 160 : static_cast<uint16_t>(speed);
}

/**
 * Choose the bridge type for a junction bridge.
 * With a speed-limited rail type, this is the cheapest available bridge at least as fast as the rail type (D35).
 * With an unlimited rail type, the target speed is the given one (D141), or without one that of the fastest buildable train engine that runs on the rail type (D45).
 * A given target speed below the rail type's limit replaces the limit (D141).
 * The choice is then the cheapest available bridge at least that fast, then the lowest index.
 * If no bridge is that fast, it is the fastest available bridge, then the cheapest, then the lowest index.
 * Without any such engine, it is the cheapest available bridge, then the lowest index.
 * @param railtype The rail type the bridge carries.
 * @param length Number of middle tiles of the bridge.
 * @param target_speed The speed the bridge must reach (D141); 0 for the rail type's limit, or the fastest buildable engine when unlimited.
 * @return The chosen bridge type, or std::nullopt if no available bridge qualifies.
 */
std::optional<BridgeType> ChooseJunctionBridgeType(RailType railtype, uint length, uint16_t target_speed)
{
	uint16_t rail_speed = GetRailTypeInfo(railtype)->max_speed;
	if (target_speed != 0 && rail_speed != 0) rail_speed = std::min(rail_speed, target_speed);

	if (rail_speed == 0) {
		const uint target = target_speed != 0 ? target_speed : GetMaxBuildableEngineSpeed(railtype);

		std::optional<BridgeType> cheapest; // Cheapest bridge fast enough, or cheapest of all without a target.
		std::optional<BridgeType> fastest; // Fallback: fastest bridge, if none is fast enough.
		for (BridgeType type = 0; type < MAX_BRIDGES; type++) {
			if (CheckBridgeAvailability(type, length).Failed()) continue;

			const BridgeSpec *spec = GetBridgeSpec(type);
			if (target == 0 || spec->speed >= target) {
				if (!cheapest.has_value() || spec->price < GetBridgeSpec(*cheapest)->price) cheapest = type;
			}
			if (!fastest.has_value()) {
				fastest = type;
			} else {
				const BridgeSpec *fastest_spec = GetBridgeSpec(*fastest);
				if (spec->speed > fastest_spec->speed || (spec->speed == fastest_spec->speed && spec->price < fastest_spec->price)) fastest = type;
			}
		}
		return cheapest.has_value() ? cheapest : fastest;
	}

	std::optional<BridgeType> best;
	for (BridgeType type = 0; type < MAX_BRIDGES; type++) {
		if (CheckBridgeAvailability(type, length).Failed()) continue;

		const BridgeSpec *spec = GetBridgeSpec(type);
		if (spec->speed < rail_speed) continue;

		if (!best.has_value() || spec->price < GetBridgeSpec(*best)->price) best = type;
	}
	return best;
}

/**
 * Get the slope facts of a tile.
 * @param tile The tile to check.
 * @return The slope facts, or std::nullopt if the tile is off-map.
 */
std::optional<JunctionTileSlope> GameJunctionPricing::GetTileSlope(TileIndex tile) const
{
	if (!IsValidTile(tile)) return std::nullopt;

	auto [slope, z] = GetTileSlopeZ(tile);
	return JunctionTileSlope{z, GetTileMaxZ(tile), IsSteepSlope(slope), _settings_game.construction.build_on_slopes};
}

/**
 * Test-only run of building one rail piece, including clearing costs.
 * @param tile The tile to build on.
 * @param track The track piece to build.
 * @return The cost of the command, or its error.
 */
CommandCost GameJunctionPricing::TestRail(TileIndex tile, Track track) const
{
	return Command<Commands::BuildRail>::Do(CommandFlagsToDCFlags(GetCommandFlags<Commands::BuildRail>()) | DoCommandFlag::QueryCost, tile, this->railtype, track, false);
}

/**
 * Test-only run of building a block of station tiles as a new station, with the default station class and type.
 * @param tile North corner of the block.
 * @param axis Platform direction.
 * @param platforms Number of platforms.
 * @param length Platform length in tiles.
 * @return The cost of the command, or its error.
 */
CommandCost GameJunctionPricing::TestStation(TileIndex tile, Axis axis, uint8_t platforms, uint8_t length) const
{
	return Command<Commands::BuildRailStation>::Do(CommandFlagsToDCFlags(GetCommandFlags<Commands::BuildRailStation>()) | DoCommandFlag::QueryCost, tile, this->railtype, axis, platforms, length, STAT_CLASS_DFLT, 0, StationID::Invalid(), false);
}

/**
 * Full price of the cheapest suitable bridge between two bridge heads, as the bridge window shows it.
 * @param start One bridge head.
 * @param end The other bridge head.
 * @return The price, or the error that prevents building the bridge.
 */
CommandCost GameJunctionPricing::PriceBridge(TileIndex start, TileIndex end) const
{
	const uint length = GetTunnelBridgeLength(start, end);
	std::optional<BridgeType> type = ChooseJunctionBridgeType(this->railtype, length);
	if (!type.has_value()) return CommandCost(STR_ERROR_CAN_T_BUILD_BRIDGE_HERE);

	CommandCost ret = Command<Commands::BuildBridge>::Do(CommandFlagsToDCFlags(GetCommandFlags<Commands::BuildBridge>()) | DoCommandFlag::QueryCost, end, start, TransportType::Rail, *type, this->railtype, INVALID_ROADTYPE);
	if (ret.Failed()) return ret;

	/* The query excludes the bridge itself; add it as the bridge window does. */
	return CommandCost(ExpensesType::Construction, ret.GetCost() + this->BridgePrice(start, end));
}

/**
 * The price of one track piece as CmdBuildSingleRail charges it, without any terrain check or clearing: the piece and, when the ground needs one, the foundation.
 * @param track The piece (the price does not depend on it).
 * @param foundation Whether the tile's planned ground needs a foundation for the piece.
 * @return The price.
 */
Money GameJunctionPricing::RailPrice([[maybe_unused]] Track track, bool foundation) const
{
	return RailBuildCost(this->railtype) + (foundation ? _price[Price::BuildFoundation] : Money{});
}

/**
 * The price of the bridge as CmdBuildBridge and the bridge window charge it, without the command's terrain checks and without clearing or foundations of its heads and span: the bridge itself and the rail on its heads and span.
 * @param start One bridge head.
 * @param end The other bridge head.
 * @return The price; nothing when no bridge type qualifies.
 */
Money GameJunctionPricing::BridgePrice(TileIndex start, TileIndex end) const
{
	const uint length = GetTunnelBridgeLength(start, end);
	const std::optional<BridgeType> type = ChooseJunctionBridgeType(this->railtype, length);
	if (!type.has_value()) return Money{};
	const BridgeSpec *spec = GetBridgeSpec(*type);
	return (((int64_t)CalcBridgeLenCostFactor(length + 2) * _price[Price::BuildBridge] * spec->price) >> 8) + (length + 2) * RailBuildCost(this->railtype);
}

/**
 * Test-only run of building a rail tunnel, as the tunnel build command would do it.
 * @param start The portal the tunnel is built from.
 * @param end The portal the tunnel must end at.
 * @return The cost of the command, or its error; an error too when the tunnel would end elsewhere.
 */
CommandCost GameJunctionPricing::PriceTunnel(TileIndex start, TileIndex end) const
{
	CommandCost ret = Command<Commands::BuildTunnel>::Do(CommandFlagsToDCFlags(GetCommandFlags<Commands::BuildTunnel>()) | DoCommandFlag::QueryCost, start, TransportType::Rail, this->railtype, INVALID_ROADTYPE);
	if (ret.Failed()) return ret;
	if (_build_tunnel_endtile != end) return CommandCost(STR_ERROR_CAN_T_BUILD_TUNNEL_HERE);
	return ret;
}

/**
 * Test-only run of clearing a tile, as TestClear does.
 * @param tile The tile to clear.
 * @return The cost of the command, or its error.
 */
static CommandCost QueryJunctionClear(TileIndex tile)
{
	return Command<Commands::LandscapeClear>::Do(CommandFlagsToDCFlags(GetCommandFlags<Commands::LandscapeClear>()) | DoCommandFlag::QueryCost, tile);
}

/**
 * Test-only run of clearing a tile.
 * @param tile The tile to clear.
 * @return The cost of the command, or its error.
 */
CommandCost GameJunctionPricing::TestClear(TileIndex tile) const
{
	return QueryJunctionClear(tile);
}

/**
 * The canonical uphill direction of an inclined slope under a junction orientation (D152).
 * @param slope The tile's slope.
 * @param rotation Quarter turns of the orientation.
 * @param mirrored Whether the orientation is mirrored.
 * @return The direction in the canonical frame the slope rises in, or DiagDirection::Invalid when the slope isn't inclined.
 */
DiagDirection JunctionCanonicalIncline(Slope slope, uint8_t rotation, bool mirrored)
{
	const DiagDirection uphill = GetInclinedSlopeDirection(slope);
	if (uphill == DiagDirection::Invalid) return DiagDirection::Invalid;
	const TileIndexDiffC canonical = InverseOrientJunctionVector(TileIndexDiffCByDiagDir(uphill), rotation, mirrored);
	for (DiagDirection dir = DiagDirection::Begin; dir != DiagDirection::End; dir++) {
		const TileIndexDiffC step = TileIndexDiffCByDiagDir(dir);
		if (step.x == canonical.x && step.y == canonical.y) return dir;
	}
	NOT_REACHED();
}

/**
 * The slope of a tile under a junction orientation (D152).
 * @param slope The tile's slope.
 * @param rotation Quarter turns of the orientation.
 * @param mirrored Whether the orientation is mirrored.
 * @return The slope in the canonical frame: each raised corner moved with the frame, the steep bit kept.
 */
Slope JunctionCanonicalSlope(Slope slope, uint8_t rotation, bool mirrored)
{
	/* Corner positions relative to the tile centre in map x and y: N (-1,-1), E (-1,1), S (1,1), W (1,-1). */
	static constexpr TileIndexDiffC CORNER_OFFSET[] = {{1, -1}, {1, 1}, {-1, 1}, {-1, -1}}; // Indexed by Corner: W, S, E, N.
	Slope result = slope & SLOPE_STEEP;
	for (const Corner corner : {Corner::W, Corner::S, Corner::E, Corner::N}) {
		if ((slope & SlopeWithOneCornerRaised(corner)) == SLOPE_FLAT) continue;
		const TileIndexDiffC canonical = InverseOrientJunctionVector(CORNER_OFFSET[to_underlying(corner)], rotation, mirrored);
		for (const Corner target : {Corner::W, Corner::S, Corner::E, Corner::N}) {
			const TileIndexDiffC position = CORNER_OFFSET[to_underlying(target)];
			if (position.x == canonical.x && position.y == canonical.y) result |= SlopeWithOneCornerRaised(target);
		}
	}
	return result;
}

/**
 * Fill one snapshot cell from a map tile: height, steepness, incline and class.
 * @param cell The cell, already marked Blocked; left Blocked for tiles the generator cannot use.
 * @param tile The map tile.
 * @param rotation Quarter turns of the orientation, for the canonical incline.
 * @param mirrored Whether the orientation is mirrored, for the canonical incline.
 */
static void FillJunctionTerrainCell(JunctionTerrainTile &cell, TileIndex tile, uint8_t rotation, bool mirrored)
{
	auto [slope, z] = GetTileSlopeZ(tile);
	cell.z_min = static_cast<int8_t>(z);
	cell.z_max = static_cast<int8_t>(GetTileMaxZ(tile));
	cell.steep = IsSteepSlope(slope);
	cell.incline = JunctionCanonicalIncline(slope, rotation, mirrored);
	cell.slope = JunctionCanonicalSlope(slope, rotation, mirrored);

	switch (GetTileType(tile)) {
		case TileType::Clear:
		case TileType::Trees: {
			CommandCost cost = QueryJunctionClear(tile);
			if (cost.Failed()) break;
			cell.cls = JunctionTileClass::Buildable;
			cell.clear_cost = cost.GetCost();
			break;
		}
		case TileType::Water:
			cell.cls = JunctionTileClass::Water;
			break;
		case TileType::Railway:
			if (IsPlainRail(tile)) cell.cls = JunctionTileClass::Spannable;
			break;
		case TileType::Road:
			if (IsNormalRoad(tile) || IsLevelCrossing(tile)) cell.cls = JunctionTileClass::Spannable;
			break;
		default:
			break;
	}
}

/**
 * Set the game settings a terrain snapshot carries.
 * @param terrain The snapshot.
 */
static void SetJunctionTerrainSettings(JunctionTerrain &terrain)
{
	terrain.foundation_cost = _price[Price::BuildFoundation];
	terrain.terraform_cost = _price[Price::Terraform];
	terrain.build_on_slopes = _settings_game.construction.build_on_slopes;
	terrain.max_bridge_height = _settings_game.construction.max_bridge_height;
	terrain.max_height = _settings_game.construction.map_height_limit;
}

/**
 * Snapshot the map around an anchor for the generator, in its working frame (D93).
 * Working cell (x, y) is map tile anchor + OrientJunctionVector({x - frame.hub_x, y}, rotation, mirrored); the grid covers the frame.
 * @param anchor The anchor tile (working cell (frame.hub_x, 0)).
 * @param rotation Quarter turns of the orientation.
 * @param mirrored Whether the orientation is mirrored.
 * @param frame The working frame.
 * @param margin Extra cells on every side of the grid.
 * @return The snapshot.
 */
JunctionTerrain MakeJunctionTerrain(TileIndex anchor, uint8_t rotation, bool mirrored, const JunctionFrame &frame, int margin)
{
	JunctionTerrain terrain;
	terrain.origin = {static_cast<int16_t>(-margin), static_cast<int16_t>(frame.terrain_origin_y - margin)};
	terrain.width = frame.width + 2 * margin;
	terrain.height = frame.terrain_height + 2 * margin;
	terrain.tiles.resize(static_cast<size_t>(terrain.width) * terrain.height);
	SetJunctionTerrainSettings(terrain);

	for (int gy = 0; gy < terrain.height; gy++) {
		for (int gx = 0; gx < terrain.width; gx++) {
			JunctionTerrainTile &cell = terrain.tiles[static_cast<size_t>(gy) * terrain.width + gx];
			cell.cls = JunctionTileClass::Blocked;

			const TileIndexDiffC v = OrientJunctionVector({static_cast<int16_t>(gx + terrain.origin.x - frame.hub_x), static_cast<int16_t>(gy + terrain.origin.y)}, rotation, mirrored);
			const int x = static_cast<int>(TileX(anchor)) + v.x;
			const int y = static_cast<int>(TileY(anchor)) + v.y;
			if (x < 0 || y < 0 || x >= static_cast<int>(Map::SizeX()) || y >= static_cast<int>(Map::SizeY())) continue;
			const TileIndex tile = TileXY(x, y);
			if (!IsValidTile(tile)) continue;

			FillJunctionTerrainCell(cell, tile, rotation, mirrored);
		}
	}
	return terrain;
}

/**
 * Snapshot a map-axis rectangle for the line painter (LP9): cell (x, y) is map tile TileXY(x, y), orientation rotation 0, not mirrored.
 * The rectangle spans both corners (in any order) plus margin cells on every side; cells off the map are Blocked.
 * @param a One corner tile.
 * @param b The other corner tile.
 * @param margin Extra cells on every side.
 * @return The snapshot, origin at the rectangle's smallest map x and y (which may be negative).
 */
JunctionTerrain MakeJunctionTerrainRect(TileIndex a, TileIndex b, int margin)
{
	const int min_x = static_cast<int>(std::min(TileX(a), TileX(b))) - margin;
	const int min_y = static_cast<int>(std::min(TileY(a), TileY(b))) - margin;
	const int max_x = static_cast<int>(std::max(TileX(a), TileX(b))) + margin;
	const int max_y = static_cast<int>(std::max(TileY(a), TileY(b))) + margin;

	JunctionTerrain terrain;
	terrain.origin = {static_cast<int16_t>(min_x), static_cast<int16_t>(min_y)};
	terrain.width = max_x - min_x + 1;
	terrain.height = max_y - min_y + 1;
	terrain.tiles.resize(static_cast<size_t>(terrain.width) * terrain.height);
	SetJunctionTerrainSettings(terrain);

	for (int gy = 0; gy < terrain.height; gy++) {
		for (int gx = 0; gx < terrain.width; gx++) {
			JunctionTerrainTile &cell = terrain.tiles[static_cast<size_t>(gy) * terrain.width + gx];
			cell.cls = JunctionTileClass::Blocked;

			const int x = min_x + gx;
			const int y = min_y + gy;
			if (x < 0 || y < 0 || x >= static_cast<int>(Map::SizeX()) || y >= static_cast<int>(Map::SizeY())) continue;
			const TileIndex tile = TileXY(x, y);
			if (!IsValidTile(tile)) continue;

			FillJunctionTerrainCell(cell, tile, 0, false);
		}
	}
	return terrain;
}

/** @return The map offsets tried around the anchor, nearest first: rings at 2, 4, 6 and 8 tiles (D195: the fallback when no structure clears an obstacle). */
static std::vector<TileIndexDiffC> JunctionNearOffsets()
{
	static constexpr int16_t DISTANCES[] = {2, 4, 6, 8};
	std::vector<TileIndexDiffC> offsets{{0, 0}};
	for (int16_t d : DISTANCES) {
		for (int16_t dy = -d; dy <= d; dy += d) {
			for (int16_t dx = -d; dx <= d; dx += d) {
				if (dx != 0 || dy != 0) offsets.push_back({dx, dy});
			}
		}
	}
	return offsets;
}

/**
 * Try the anchor, then nearby anchors, on one snapshot, running the node search at each (D182).
 * The search starts from the node model's topology (JunctionNodeTopologyT, D189) and solves lengths and detours against the shifted terrain.
 * @param spec The junction spec.
 * @param terrain The snapshot, taken with a margin of at least 4.
 * @param rotation Quarter turns of the orientation.
 * @param mirrored Whether the orientation is mirrored.
 * @param cancel Set to stop the search.
 * @return The first layout found and its map offset from the anchor; else the result at the anchor.
 */
JunctionNearResult GenerateJunctionNear(const JunctionSpec &spec, const JunctionTerrain &terrain, uint8_t rotation, bool mirrored, const std::atomic<bool> &cancel)
{
	JunctionNearResult first;
	bool have_first = false;
	const std::vector<TileIndexDiffC> offsets = JunctionNearOffsets();
	const JunctionTMapResult &topo = JunctionNodeTopologyT();
	const JunctionFrame frame = MakeJunctionFrame(spec);
	for (const TileIndexDiffC &d : offsets) {
		if (cancel) break;
		JunctionNearResult attempt{{}, d};
		attempt.result.prio_tiles = spec.prio_tiles;
		if (!topo.layout.has_value()) {
			attempt.result.error = JunctionGenerateError::NoLayout;
			attempt.result.hub = topo.hub;
		} else {
			const TileIndexDiffC c = InverseOrientJunctionVector(d, rotation, mirrored);
			JunctionTerrain shifted = terrain;
			shifted.origin = {static_cast<int16_t>(terrain.origin.x - c.x), static_cast<int16_t>(terrain.origin.y - c.y)};
			const TileIndexDiffC start_offset = {static_cast<int16_t>(frame.hub_x - topo.hub.x), static_cast<int16_t>(-topo.hub.y)};
			/* D179: a blocked port approach is reported without a search. The ports are the topology's columns on row 0. */
			if (JunctionBlockedPortApproach(shifted, start_offset.x, start_offset.x + topo.layout->size.x - 1).has_value()) {
				Debug(Facility::Misc, Severity::Debug2, "junction node search at offset {},{}: port approach blocked", d.x, d.y);
				attempt.result.error = JunctionGenerateError::PortBlocked;
				attempt.result.hub = topo.hub;
				if (!have_first) {
					first = std::move(attempt);
					have_first = true;
				}
				continue;
			}
			const auto time_start = std::chrono::steady_clock::now();
			const JunctionNodeSearch search = SearchJunctionNodes(*topo.layout, spec, shifted, start_offset, 16);
			const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - time_start).count();
			const std::string_view log = search.log;
			const size_t last_start = log.size() > 1 ? log.rfind('\n', log.size() - 2) : std::string_view::npos;
			Debug(Facility::Misc, Severity::Debug2, "junction node search at offset {},{}: {} states, {} us, {}", d.x, d.y, search.states, micros, log.substr(last_start == std::string_view::npos ? 0 : last_start + 1));
			if (search.layout.has_value()) {
				attempt.result.layout = search.layout;
				attempt.result.error = JunctionGenerateError::None;
				attempt.result.hub = {static_cast<int16_t>(frame.hub_x - search.offset.x), static_cast<int16_t>(0 - search.offset.y)};
				/* SC1: the score carries the terrain cost, like the library fit's, so the two compare. */
				attempt.result.score = ScoreJunctionLayout(*search.layout, spec);
				attempt.result.score.second += JunctionTMapTerrainCost(*search.layout, shifted, search.offset.x, search.offset.y);
			} else {
				attempt.result.error = JunctionGenerateError::NoLayout;
				attempt.result.hub = topo.hub;
			}
		}
		if (attempt.result.layout.has_value()) return attempt;
		if (!have_first) {
			first = std::move(attempt);
			have_first = true;
		}
	}
	return first;
}

/**
 * The instant tier (D96): fit the library at the anchor, then at the nearby offsets.
 * @param library The candidate library of the spec.
 * @param terrain The snapshot, taken with a margin of at least 4.
 * @param rotation Quarter turns of the orientation.
 * @param mirrored Whether the orientation is mirrored.
 * @return The first fit as a result with its map offset; else an empty NoLayout result at offset (0,0).
 */
JunctionNearResult FitJunctionLibraryNear(const std::vector<JunctionLibraryEntry> &library, const JunctionTerrain &terrain, uint8_t rotation, bool mirrored)
{
	for (const TileIndexDiffC &d : JunctionNearOffsets()) {
		const TileIndexDiffC c = InverseOrientJunctionVector(d, rotation, mirrored);
		JunctionTerrain shifted = terrain;
		shifted.origin = {static_cast<int16_t>(terrain.origin.x - c.x), static_cast<int16_t>(terrain.origin.y - c.y)};
		const std::optional<JunctionLibraryFit> fit = FitJunctionLibrary(library, shifted);
		if (!fit.has_value()) continue;
		const JunctionLibraryEntry &entry = library[fit->entry];
		JunctionNearResult found;
		found.result.layout = entry.layout;
		found.result.error = JunctionGenerateError::None;
		found.result.hub = entry.hub;
		found.result.score = {entry.score.first, entry.score.second + fit->terrain_cost};
		found.result.prio_tiles = entry.prio_tiles;
		found.offset = d;
		return found;
	}
	JunctionNearResult none;
	none.result.error = JunctionGenerateError::NoLayout;
	return none;
}

/**
 * Price of one signal.
 * @return The current price for building a signal.
 */
Money GameJunctionPricing::SignalCost() const
{
	return _price[Price::BuildSignals];
}

/**
 * Money the local company can spend.
 * @return The available money of the local company.
 */
Money GameJunctionPricing::AvailableMoney() const
{
	return GetAvailableMoney(_local_company);
}

/**
 * Post one junction build step as a command.
 * @param step The step to post.
 * @return The result of posting; false also if no bridge type qualifies.
 */
bool GameJunctionStepPoster::Post(const JunctionBuildStep &step)
{
	return std::visit([this]<typename T>(const T &s) -> bool {
		if constexpr (std::is_same_v<T, JunctionRailStep>) {
			return Command<Commands::BuildRail>::Post(STR_ERROR_CAN_T_BUILD_RAILROAD_TRACK, CcJunctionStep, s.tile, this->railtype, s.track, false);
		} else if constexpr (std::is_same_v<T, JunctionStationStep>) {
			/* The same parameters as the station window uses for a new station: default class and type, no join, not adjacent. */
			return Command<Commands::BuildRailStation>::Post(STR_ERROR_CAN_T_BUILD_RAILROAD_STATION, CcJunctionStep, s.tile, this->railtype, s.axis, s.platforms, s.length, STAT_CLASS_DFLT, 0, StationID::Invalid(), false);
		} else if constexpr (std::is_same_v<T, JunctionBridgeStep>) {
			if (s.tunnel) return Command<Commands::BuildTunnel>::Post(STR_ERROR_CAN_T_BUILD_TUNNEL_HERE, CcJunctionStep, s.start, TransportType::Rail, this->railtype, INVALID_ROADTYPE);
			uint length = GetTunnelBridgeLength(s.start, s.end);
			auto type = ChooseJunctionBridgeType(this->railtype, length);
			if (!type.has_value()) return false;
			return Command<Commands::BuildBridge>::Post(STR_ERROR_CAN_T_BUILD_BRIDGE_HERE, CcJunctionStep, s.end, s.start, TransportType::Rail, *type, this->railtype, INVALID_ROADTYPE);
		} else if constexpr (std::is_same_v<T, JunctionTerraformStep>) {
			return Command<Commands::TerraformLand>::Post(s.raise ? STR_ERROR_CAN_T_RAISE_LAND_HERE : STR_ERROR_CAN_T_LOWER_LAND_HERE, CcJunctionTerraformStep, s.tile, SlopeWithOneCornerRaised(s.corner), s.raise);
		} else {
			return Command<Commands::BuildSignal>::Post(STR_ERROR_CAN_T_BUILD_SIGNALS_HERE, CcJunctionStep, s.tile, TrackdirToTrack(s.facing), s.type, s.variant, false, false, false, SignalType::Block, SignalType::Block, 0, SignalAlongTrackdir(s.facing));
		}
	}, step);
}

/**
 * Get the single client-side junction build sequencer.
 * @return The sequencer.
 */
JunctionSequencer &GetJunctionSequencer()
{
	static JunctionSequencer sequencer;
	return sequencer;
}

/**
 * Command callback of every junction build step; reports the result to the sequencer.
 * @param cmd The command that was executed.
 * @param result The result of the command.
 * @param tile The tile the command was executed on.
 */
void CcJunctionStep([[maybe_unused]] Commands cmd, const CommandCost &result, [[maybe_unused]] TileIndex tile)
{
	if (result.Succeeded()) GetJunctionSequencer().spent += result.GetCost();
	GetJunctionSequencer().OnStepResult(result.Succeeded());
}

/**
 * Command callback of every junction terraform step; reports the result to the sequencer.
 * @param cmd The command that was executed.
 * @param result The result of the command.
 * @param cost The cost of the command.
 * @param tile The tile the command was executed on.
 */
void CcJunctionTerraformStep([[maybe_unused]] Commands cmd, const CommandCost &result, Money cost, [[maybe_unused]] TileIndex tile)
{
	if (result.Succeeded()) GetJunctionSequencer().spent += cost;
	GetJunctionSequencer().OnStepResult(result.Succeeded());
}

/**
 * Fill a generator spec from the game: settings, rail type, prices and bridges.
 * @param railtype Rail type of the junction.
 * @param ml_gap Gap of the mainline notation.
 * @param branch_gap Gap of the branch notation.
 * @param tl Train length in tiles.
 * @param train The loaded design train (D137); the generator sizes the prio coverage from it, and its speed caps the reference speed (D141).
 * @return The spec; prio_tiles is 0.
 */
JunctionSpec MakeJunctionSpec(RailType railtype, uint8_t ml_gap, uint8_t branch_gap, uint8_t tl, const JunctionTrainParams &train)
{
	/* D141: the picked train's speed caps the rail type's limit; an unlimited rail type uses the train's speed. */
	uint16_t speed = GetJunctionReferenceSpeed(railtype);
	if (train.max_speed > 0) speed = GetRailTypeInfo(railtype)->max_speed == 0 ? train.max_speed : std::min(speed, train.max_speed);

	JunctionSpec spec{};
	spec.ml_gap = ml_gap;
	spec.branch_gap = branch_gap;
	spec.tl = tl;
	spec.train = train;
	spec.sd = _settings_client.gui.drag_signals_density;
	spec.reference_speed = speed;
	spec.curve_speed = GetRailTypeInfo(railtype)->curve_speed;
	spec.curve_limit = _settings_game.vehicle.train_acceleration_model != AccelerationModel::Original;
	spec.slope_steepness = _settings_game.vehicle.train_slope_steepness;
	spec.signal_type = _settings_client.gui.junction_signal_type == 0 ? SignalType::Block : SignalType::PathOneWay;
	/* Precompute on this thread: the generator runs on a worker thread, which must not read game state. */
	spec.max_bridge_middle = std::max<uint>(1, _settings_game.construction.max_bridge_length);
	const uint max_middle = spec.max_bridge_middle;
	std::vector<bool> span_ok(max_middle + 1, false);
	std::vector<Money> bridge_price(max_middle + 1, Money(std::numeric_limits<int64_t>::max() / 4));
	for (uint n = 1; n <= max_middle; n++) {
		std::optional<BridgeType> type = ChooseJunctionBridgeType(railtype, n, spec.reference_speed);
		if (!type.has_value()) continue;
		const BridgeSpec *bridge = GetBridgeSpec(*type);
		/* D94: when no bridge reaches the design speed, the chooser's fastest bridge is used and trains slow on it. */
		span_ok[n] = true;
		bridge_price[n] = Money(((int64_t)CalcBridgeLenCostFactor(n + 2) * _price[Price::BuildBridge] * bridge->price) >> 8) + (n + 2) * RailBuildCost(railtype);
	}
	spec.bridge_span_ok = [span_ok, max_middle](uint n) {
		return n >= 1 && n <= max_middle && span_ok[n];
	};
	spec.prices.rail_piece = RailBuildCost(railtype);
	spec.prices.signal = _price[Price::BuildSignals];
	spec.prices.bridge = [bridge_price, max_middle](uint n) {
		if (n < 1 || n > max_middle) return Money(std::numeric_limits<int64_t>::max() / 4);
		return bridge_price[n];
	};
	spec.max_tunnel_length = _settings_game.construction.max_tunnel_length;
	std::vector<Money> tunnel_price;
	for (uint n = 0; n <= spec.max_tunnel_length; n++) tunnel_price.push_back(JunctionTunnelPrice(n, _price[Price::BuildTunnel], RailBuildCost(railtype)));
	spec.prices.tunnel = [tunnel_price](uint n) {
		if (n >= tunnel_price.size()) return Money(std::numeric_limits<int64_t>::max() / 4);
		return tunnel_price[n];
	};
	return spec;
}

Money JunctionTunnelPrice(uint middle, Money build_tunnel, Money rail_piece)
{
	/* Mirrors CmdBuildTunnel (tunnelbridge_cmd.cpp), without the clearing of the portals. */
	int tiles_coef = 3;
	uint tiles_bump = 25;
	Money cost = 0;
	for (uint tiles = 1; tiles <= middle; tiles++) {
		if (tiles == tiles_bump) {
			tiles_coef++;
			tiles_bump *= 2;
		}
		cost += build_tunnel;
		cost += cost.base() >> tiles_coef;
	}
	/* The entrance, the exit, and the rail in the portals and the middle tiles. */
	cost += build_tunnel;
	cost += build_tunnel;
	return cost + (middle + 2) * rail_piece;
}

/**
 * The coverage and strictness for a slider choice (D139).
 * @param choice The slider choice; 0 is the maximum.
 * @param tl Train length in tiles.
 * @param sized The prio length M from JunctionPrioTiles (> 0).
 * @return {prio_tiles, prio_strict}.
 */
std::pair<uint8_t, bool> JunctionPrioChoice(uint8_t choice, uint8_t tl, uint8_t sized)
{
	const uint8_t minimum = std::min(tl, sized);
	if (choice == 0 || choice >= sized) return {sized, true};
	const uint8_t tiles = std::max(choice, minimum);
	return {tiles, tiles == sized};
}
