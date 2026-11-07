/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_game.cpp Game-side pricing for the junction tool. */

#include "stdafx.h"

#include "junction_game.h"
#include "command_func.h"
#include "rail_cmd.h"
#include "tunnelbridge_cmd.h"
#include "bridge.h"
#include "tunnelbridge.h"
#include "rail.h"
#include "economy_func.h"
#include "company_func.h"
#include "tile_map.h"
#include "slope_type.h"
#include "table/strings.h"

#include "safeguards.h"

/**
 * Choose the bridge type for a junction bridge.
 * With a speed-limited rail type, this is the cheapest available bridge at least as fast as the rail type.
 * With an unlimited rail type, it is the fastest available bridge, then the cheapest, then the lowest index.
 * @param railtype The rail type the bridge carries.
 * @param length Number of middle tiles of the bridge.
 * @return The chosen bridge type, or std::nullopt if no available bridge qualifies.
 */
std::optional<BridgeType> ChooseJunctionBridgeType(RailType railtype, uint length)
{
	const uint16_t rail_speed = GetRailTypeInfo(railtype)->max_speed;

	std::optional<BridgeType> best;
	for (BridgeType type = 0; type < MAX_BRIDGES; type++) {
		if (CheckBridgeAvailability(type, length).Failed()) continue;

		const BridgeSpec *spec = GetBridgeSpec(type);
		if (rail_speed != 0 && spec->speed < rail_speed) continue;

		if (!best.has_value()) {
			best = type;
			continue;
		}

		const BridgeSpec *best_spec = GetBridgeSpec(*best);
		if (rail_speed != 0) {
			if (spec->price < best_spec->price) best = type;
		} else if (spec->speed > best_spec->speed || (spec->speed == best_spec->speed && spec->price < best_spec->price)) {
			best = type;
		}
	}
	return best;
}

/**
 * Get the height of a flat tile.
 * @param tile The tile to check.
 * @return The height of the tile, or std::nullopt if it is off-map or not flat.
 */
std::optional<int> GameJunctionPricing::GetFlatHeight(TileIndex tile) const
{
	if (!IsValidTile(tile)) return std::nullopt;

	auto [slope, z] = GetTileSlopeZ(tile);
	if (slope == SLOPE_FLAT) return z;
	return std::nullopt;
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
