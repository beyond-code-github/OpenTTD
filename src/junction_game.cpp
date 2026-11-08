/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_game.cpp Game-side pricing for the junction tool. */

#include "stdafx.h"

#include "junction_game.h"
#include "junction_generator.h"
#include "settings_type.h"
#include "command_func.h"
#include "rail_cmd.h"
#include "landscape_cmd.h"
#include "tunnelbridge_cmd.h"
#include "bridge.h"
#include "tunnelbridge.h"
#include "signal_func.h"
#include "track_func.h"
#include "rail.h"
#include "engine_base.h"
#include "engine_func.h"
#include "economy_func.h"
#include "company_func.h"
#include "tile_map.h"
#include "rail_map.h"
#include "road_map.h"
#include "slope_type.h"
#include "slope_func.h"
#include "map_func.h"
#include "tile_type.h"
#include "train.h"
#include "strings_func.h"
#include "table/strings.h"

#include <array>

#include "safeguards.h"

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
 * With an unlimited rail type, the target speed is that of the fastest buildable train engine that runs on the rail type (D45).
 * The choice is then the cheapest available bridge at least that fast, then the lowest index.
 * If no bridge is that fast, it is the fastest available bridge, then the cheapest, then the lowest index.
 * Without any such engine, it is the cheapest available bridge, then the lowest index.
 * @param railtype The rail type the bridge carries.
 * @param length Number of middle tiles of the bridge.
 * @return The chosen bridge type, or std::nullopt if no available bridge qualifies.
 */
std::optional<BridgeType> ChooseJunctionBridgeType(RailType railtype, uint length)
{
	const uint16_t rail_speed = GetRailTypeInfo(railtype)->max_speed;

	if (rail_speed == 0) {
		const uint target = GetMaxBuildableEngineSpeed(railtype);

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
	const BridgeSpec *spec = GetBridgeSpec(*type);
	const Money total = ret.GetCost() + (((int64_t)CalcBridgeLenCostFactor(length + 2) * _price[Price::BuildBridge] * spec->price) >> 8) + (length + 2) * RailBuildCost(this->railtype);
	return CommandCost(ExpensesType::Construction, total);
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
	terrain.foundation_cost = _price[Price::BuildFoundation];
	terrain.build_on_slopes = _settings_game.construction.build_on_slopes;
	terrain.max_bridge_height = _settings_game.construction.max_bridge_height;

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

			auto [slope, z] = GetTileSlopeZ(tile);
			cell.z_min = static_cast<int8_t>(z);
			cell.z_max = static_cast<int8_t>(GetTileMaxZ(tile));
			cell.steep = IsSteepSlope(slope);

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
	}
	return terrain;
}

/** @return The map offsets tried around the anchor, nearest first. */
static std::vector<TileIndexDiffC> JunctionNearOffsets()
{
	static constexpr int16_t DISTANCES[] = {2, 4};
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
 * Try the anchor, then nearby anchors, on one snapshot.
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
	for (const TileIndexDiffC &d : JunctionNearOffsets()) {
		if (cancel) break;
		const TileIndexDiffC c = InverseOrientJunctionVector(d, rotation, mirrored);
		JunctionTerrain shifted = terrain;
		shifted.origin = {static_cast<int16_t>(terrain.origin.x - c.x), static_cast<int16_t>(terrain.origin.y - c.y)};
		JunctionNearResult attempt{GenerateJunction(spec, &shifted), d};
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
		found.result.score = {entry.score.first + fit->terrain_cost, entry.score.second};
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
		} else if constexpr (std::is_same_v<T, JunctionBridgeStep>) {
			uint length = GetTunnelBridgeLength(s.start, s.end);
			auto type = ChooseJunctionBridgeType(this->railtype, length);
			if (!type.has_value()) return false;
			return Command<Commands::BuildBridge>::Post(STR_ERROR_CAN_T_BUILD_BRIDGE_HERE, CcJunctionStep, s.end, s.start, TransportType::Rail, *type, this->railtype, INVALID_ROADTYPE);
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
	GetJunctionSequencer().OnStepResult(result.Succeeded());
}

/**
 * Fill a generator spec from the game: settings, rail type, prices and bridges.
 * @param railtype Rail type of the junction.
 * @param ml_gap Gap of the mainline notation.
 * @param branch_gap Gap of the branch notation.
 * @param tl Train length in tiles.
 * @return The spec.
 */
JunctionSpec MakeJunctionSpec(RailType railtype, uint8_t ml_gap, uint8_t branch_gap, uint8_t tl)
{
	const uint16_t speed = GetJunctionReferenceSpeed(railtype);

	JunctionSpec spec{};
	spec.ml_gap = ml_gap;
	spec.branch_gap = branch_gap;
	spec.tl = tl;
	spec.prio_tl = 1;
	spec.sd = _settings_client.gui.drag_signals_density;
	spec.reference_speed = speed;
	spec.curve_speed = GetRailTypeInfo(railtype)->curve_speed;
	spec.curve_limit = _settings_game.vehicle.train_acceleration_model != AccelerationModel::Original;
	spec.signal_type = _settings_client.gui.junction_signal_type == 0 ? SignalType::Block : SignalType::PathOneWay;
	/* Precompute on this thread: the generator runs on a worker thread, which must not read game state. */
	std::array<bool, 11> span_ok{};
	std::array<Money, 11> bridge_price{};
	bridge_price.fill(Money(std::numeric_limits<int64_t>::max() / 4));
	for (uint n = 1; n <= 10; n++) {
		std::optional<BridgeType> type = ChooseJunctionBridgeType(railtype, n);
		if (!type.has_value()) continue;
		const BridgeSpec *bridge = GetBridgeSpec(*type);
		/* D94: when no bridge reaches the design speed, the chooser's fastest bridge is used and trains slow on it. */
		span_ok[n] = true;
		bridge_price[n] = Money(((int64_t)CalcBridgeLenCostFactor(n + 2) * _price[Price::BuildBridge] * bridge->price) >> 8) + (n + 2) * RailBuildCost(railtype);
	}
	spec.bridge_span_ok = [span_ok](uint n) {
		return n >= 1 && n <= 10 && span_ok[n];
	};
	spec.prices.rail_piece = RailBuildCost(railtype);
	spec.prices.signal = _price[Price::BuildSignals];
	spec.prices.bridge = [bridge_price](uint n) {
		if (n < 1 || n > 10) return Money(std::numeric_limits<int64_t>::max() / 4);
		return bridge_price[n];
	};
	return spec;
}
