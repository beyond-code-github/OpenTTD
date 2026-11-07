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

#include <algorithm>
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

TEST_CASE("JunctionLayout: test junction fixture contents")
{
	JunctionLayout layout = MakeTestJunctionLayout();

	CHECK(layout.size.x == 8);
	CHECK(layout.size.y == 6);

	REQUIRE(layout.tracks.size() == 9);
	for (int x = 0; x < 8; x++) {
		CHECK(layout.tracks[x].offset.x == x);
		CHECK(layout.tracks[x].offset.y == 2);
		CHECK(layout.tracks[x].tracks == TrackBits{Track::X});
	}
	CHECK(layout.tracks[8].offset.x == 4);
	CHECK(layout.tracks[8].offset.y == 5);
	CHECK(layout.tracks[8].tracks == TrackBits{Track::Y});

	REQUIRE(layout.bridges.size() == 1);
	CHECK(layout.bridges[0].start.x == 4);
	CHECK(layout.bridges[0].start.y == 0);
	CHECK(layout.bridges[0].end.x == 4);
	CHECK(layout.bridges[0].end.y == 4);

	REQUIRE(layout.signals.size() == 2);
	CHECK(layout.signals[0].offset.x == 1);
	CHECK(layout.signals[0].offset.y == 2);
	CHECK(layout.signals[0].facing == Trackdir::X_SW);
	CHECK(layout.signals[0].type == SignalType::Block);
	CHECK(layout.signals[0].variant == SignalVariant::Electric);
	CHECK(layout.signals[1].offset.x == 6);
	CHECK(layout.signals[1].offset.y == 2);
	CHECK(layout.signals[1].facing == Trackdir::X_NE);
	CHECK(layout.signals[1].type == SignalType::Block);
	CHECK(layout.signals[1].variant == SignalVariant::Electric);
}

TEST_CASE("JunctionLayout: build steps are rail, then bridges, then signals")
{
	Map::Allocate(64, 64);

	std::vector<JunctionBuildStep> steps = MakeJunctionBuildSteps(MakeTestJunctionLayout(), TileXY(10, 10));
	REQUIRE(steps.size() == 12);

	for (int i = 0; i < 8; i++) {
		REQUIRE(std::holds_alternative<JunctionRailStep>(steps[i]));
		const JunctionRailStep &rail = std::get<JunctionRailStep>(steps[i]);
		CHECK(rail.tile == TileXY(10 + i, 12));
		CHECK(rail.track == Track::X);
	}
	REQUIRE(std::holds_alternative<JunctionRailStep>(steps[8]));
	const JunctionRailStep &rail = std::get<JunctionRailStep>(steps[8]);
	CHECK(rail.tile == TileXY(14, 15));
	CHECK(rail.track == Track::Y);

	REQUIRE(std::holds_alternative<JunctionBridgeStep>(steps[9]));
	const JunctionBridgeStep &bridge = std::get<JunctionBridgeStep>(steps[9]);
	CHECK(bridge.start == TileXY(14, 10));
	CHECK(bridge.end == TileXY(14, 14));

	REQUIRE(std::holds_alternative<JunctionSignalStep>(steps[10]));
	const JunctionSignalStep &signal0 = std::get<JunctionSignalStep>(steps[10]);
	CHECK(signal0.tile == TileXY(11, 12));
	CHECK(signal0.facing == Trackdir::X_SW);
	CHECK(signal0.type == SignalType::Block);
	CHECK(signal0.variant == SignalVariant::Electric);

	REQUIRE(std::holds_alternative<JunctionSignalStep>(steps[11]));
	const JunctionSignalStep &signal1 = std::get<JunctionSignalStep>(steps[11]);
	CHECK(signal1.tile == TileXY(16, 12));
	CHECK(signal1.facing == Trackdir::X_NE);
	CHECK(signal1.type == SignalType::Block);
	CHECK(signal1.variant == SignalVariant::Electric);
}

TEST_CASE("JunctionLayout: one rail step per track bit")
{
	Map::Allocate(64, 64);

	JunctionLayout layout;
	layout.size = {1, 1};
	layout.tracks.push_back({{0, 0}, TrackBits{Track::Upper, Track::Lower}});

	std::vector<JunctionBuildStep> steps = MakeJunctionBuildSteps(layout, TileXY(5, 5));
	REQUIRE(steps.size() == 2);

	REQUIRE(std::holds_alternative<JunctionRailStep>(steps[0]));
	CHECK(std::get<JunctionRailStep>(steps[0]).tile == TileXY(5, 5));
	CHECK(std::get<JunctionRailStep>(steps[0]).track == Track::Upper);

	REQUIRE(std::holds_alternative<JunctionRailStep>(steps[1]));
	CHECK(std::get<JunctionRailStep>(steps[1]).tile == TileXY(5, 5));
	CHECK(std::get<JunctionRailStep>(steps[1]).track == Track::Lower);
}

TEST_CASE("JunctionLayout: steps off the map get INVALID_TILE")
{
	Map::Allocate(64, 64);

	std::vector<JunctionBuildStep> steps = MakeJunctionBuildSteps(MakeTestJunctionLayout(), TileXY(60, 60));
	REQUIRE(steps.size() == 12);

	REQUIRE(std::holds_alternative<JunctionRailStep>(steps[0]));
	CHECK(std::get<JunctionRailStep>(steps[0]).tile == TileXY(60, 62));
	REQUIRE(std::holds_alternative<JunctionRailStep>(steps[4]));
	CHECK(std::get<JunctionRailStep>(steps[4]).tile == INVALID_TILE);
	REQUIRE(std::holds_alternative<JunctionBridgeStep>(steps[9]));
	CHECK(std::get<JunctionBridgeStep>(steps[9]).start == INVALID_TILE);
}

TEST_CASE("JunctionLayout outer: one non-flat tile is bad")
{
	Map::Allocate(64, 64);

	StubJunctionPricing pricing;
	pricing.not_flat.insert(TileXY(12, 12));
	JunctionEstimate estimate = EstimateJunction(MakeTestJunctionLayout(), TileXY(10, 10), pricing);

	CHECK(estimate.bad_tiles == std::vector<TileIndex>{TileXY(12, 12)});
	CHECK(!estimate.affordable);
	CHECK(estimate.first_error == STR_ERROR_FLAT_LAND_REQUIRED);
	CHECK(estimate.total == 2000);
}

TEST_CASE("JunctionLayout outer: tile at another height is bad")
{
	Map::Allocate(64, 64);

	StubJunctionPricing pricing;
	pricing.heights[TileXY(16, 12)] = 2;
	JunctionEstimate estimate = EstimateJunction(MakeTestJunctionLayout(), TileXY(10, 10), pricing);

	CHECK(estimate.bad_tiles == std::vector<TileIndex>{TileXY(16, 12)});
	CHECK(!estimate.affordable);
	CHECK(estimate.first_error == STR_ERROR_FLAT_LAND_REQUIRED);
}

TEST_CASE("JunctionLayout outer: a failed rail step marks its tile and the rest are still priced")
{
	Map::Allocate(64, 64);

	StubJunctionPricing pricing;
	pricing.rail_fails[TileXY(11, 12)] = STR_ERROR_CAN_T_BUILD_RAILROAD_TRACK;
	JunctionEstimate estimate = EstimateJunction(MakeTestJunctionLayout(), TileXY(10, 10), pricing);

	CHECK(estimate.bad_tiles == std::vector<TileIndex>{TileXY(11, 12)});
	CHECK(estimate.first_error == STR_ERROR_CAN_T_BUILD_RAILROAD_TRACK);
	CHECK(estimate.total == 1900);
	CHECK(!estimate.affordable);
}

TEST_CASE("JunctionLayout outer: a failed bridge marks both heads")
{
	Map::Allocate(64, 64);

	StubJunctionPricing pricing;
	pricing.bridge_fails = true;
	JunctionEstimate estimate = EstimateJunction(MakeTestJunctionLayout(), TileXY(10, 10), pricing);

	CHECK(estimate.bad_tiles == std::vector<TileIndex>{TileXY(14, 10), TileXY(14, 14)});
	CHECK(estimate.total == 1000);
	CHECK(!estimate.affordable);
}

TEST_CASE("JunctionLayout outer: affordability is total against money")
{
	Map::Allocate(64, 64);

	StubJunctionPricing pricing;
	pricing.money = 1999;
	JunctionEstimate estimate = EstimateJunction(MakeTestJunctionLayout(), TileXY(10, 10), pricing);
	CHECK(!estimate.affordable);
	CHECK(estimate.bad_tiles.empty());

	pricing.money = 2000;
	estimate = EstimateJunction(MakeTestJunctionLayout(), TileXY(10, 10), pricing);
	CHECK(estimate.affordable);
}

TEST_CASE("JunctionLayout outer: layout partly off the map")
{
	Map::Allocate(64, 64);

	StubJunctionPricing pricing;
	JunctionEstimate estimate = EstimateJunction(MakeTestJunctionLayout(), TileXY(60, 60), pricing);

	CHECK(std::count(estimate.bad_tiles.begin(), estimate.bad_tiles.end(), INVALID_TILE) == 1);
	CHECK(estimate.first_error == STR_ERROR_TOO_CLOSE_TO_EDGE_OF_MAP);
	CHECK(!estimate.affordable);
}
