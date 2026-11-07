/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_layout.cpp Test functionality from junction_layout. */

#include "../stdafx.h"

#include "../3rdparty/catch2/catch.hpp"

#include "../junction_layout.h"
#include "../map_func.h"
#include "../table/strings.h"

#include <map>
#include <set>

#include "../safeguards.h"

/** Pricing stub with configurable failures. */
class StubJunctionPricing : public JunctionPricing {
public:
	std::set<TileIndex> not_flat;
	std::map<TileIndex, int> heights;
	std::map<TileIndex, StringID> rail_fails;
	bool bridge_fails = false;
	Money money = 1000000;

	std::optional<int> GetFlatHeight(TileIndex tile) const override
	{
		if (tile == INVALID_TILE || this->not_flat.contains(tile)) return std::nullopt;
		auto it = this->heights.find(tile);
		return it != this->heights.end() ? it->second : 1;
	}

	CommandCost TestRail(TileIndex tile, [[maybe_unused]] Track track) const override
	{
		auto it = this->rail_fails.find(tile);
		if (it != this->rail_fails.end()) return CommandCost(it->second);
		return CommandCost(ExpensesType::Construction, 100);
	}

	CommandCost PriceBridge([[maybe_unused]] TileIndex start, [[maybe_unused]] TileIndex end) const override
	{
		if (this->bridge_fails) return CommandCost(STR_ERROR_CAN_T_BUILD_BRIDGE_HERE);
		return CommandCost(ExpensesType::Construction, 1000);
	}

	Money SignalCost() const override { return 50; }
	Money AvailableMoney() const override { return this->money; }
};

TEST_CASE("JunctionLayout outer: test junction on flat clear land prices exactly")
{
	Map::Allocate(64, 64);

	StubJunctionPricing pricing;
	JunctionEstimate estimate = EstimateJunction(MakeTestJunctionLayout(), TileXY(10, 10), pricing);

	/* 2000 = 9 rail pieces * 100 + 1 bridge * 1000 + 2 signals * 50. */
	CHECK(estimate.total == 2000);
	CHECK(estimate.bad_tiles.empty());
	CHECK(estimate.affordable);
}
