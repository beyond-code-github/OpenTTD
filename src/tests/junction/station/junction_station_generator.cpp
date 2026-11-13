/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_station_generator.cpp Test functionality from junction_station_generator. */

#include "../../../stdafx.h"

#include "../../../3rdparty/catch2/catch.hpp"

#include "../../../junction/junction_route.h"
#include "../../../junction/station/junction_station_generator.h"
#include "../../../core/format.hpp"
#include "../../../map_func.h"

#include "../../../safeguards.h"

TEST_CASE("GenerateStation: a clean station for 2, 3 and 4 platforms", "[junction][station]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	for (uint8_t n : {2, 3, 4}) {
		const JunctionStationResult result = GenerateStation(spec, n);
		REQUIRE(result.layout.has_value());
		const JunctionLayout &layout = *result.layout;
		REQUIRE(layout.stations.size() == 1);
		const JunctionStationBlock &block = layout.stations.front();
		WARN(fmt::format("station, {} platforms, size {}x{}, block rect x {}..{}, y {}..{}:\n{}", n, layout.size.x, layout.size.y,
				block.origin.x, block.origin.x + block.length - 1, block.origin.y, block.origin.y + block.platforms - 1, RenderJunctionAscii(layout)));
		const auto violations = CheckStationRules(layout, result.ports, spec.tl, spec.sd);
		for (const StationRuleViolation &v : violations) WARN(fmt::format("({}, {}) {}", v.tile.x, v.tile.y, v.message));
		CHECK(violations.empty());
	}
}

TEST_CASE("GenerateStation: deterministic", "[junction][station]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	for (uint8_t n : {2, 3, 4}) {
		const JunctionStationResult a = GenerateStation(spec, n);
		const JunctionStationResult b = GenerateStation(spec, n);
		REQUIRE(a.layout.has_value());
		REQUIRE(b.layout.has_value());
		CHECK(JunctionLayoutToJson(*a.layout) == JunctionLayoutToJson(*b.layout));
	}
}

TEST_CASE("GenerateStation: each platform is exactly TL long", "[junction][station]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	for (uint8_t n : {2, 3, 4}) {
		const JunctionStationResult result = GenerateStation(spec, n);
		REQUIRE(result.layout.has_value());
		REQUIRE(result.layout->stations.size() == 1);
		CHECK(result.layout->stations.front().length == spec.tl);
		CHECK(result.layout->stations.front().platforms == n);
	}
}

TEST_CASE("MakeJunctionBuildSteps: a station layout gives one station step first and no rail on station tiles", "[junction][station]")
{
	Map::Allocate(64, 64);
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionStationResult result = GenerateStation(spec, 3);
	REQUIRE(result.layout.has_value());
	const JunctionLayout &layout = *result.layout;
	REQUIRE(layout.stations.size() == 1);
	const JunctionStationBlock &block = layout.stations.front();
	const TileIndex origin = TileXY(10, 10);

	const std::vector<JunctionBuildStep> steps = MakeJunctionBuildSteps(layout, origin);
	REQUIRE(!steps.empty());
	size_t station_steps = 0;
	for (const JunctionBuildStep &step : steps) {
		if (std::holds_alternative<JunctionStationStep>(step)) station_steps++;
	}
	CHECK(station_steps == 1);
	REQUIRE(std::holds_alternative<JunctionStationStep>(steps.front()));
	const JunctionStationStep &station = std::get<JunctionStationStep>(steps.front());
	CHECK(station.tile == TileXY(10 + block.origin.x, 10 + block.origin.y));
	CHECK(station.axis == block.axis);
	CHECK(station.platforms == 3);
	CHECK(station.length == spec.tl);

	size_t rail_on_station = 0;
	size_t rail_steps = 0;
	for (const JunctionBuildStep &step : steps) {
		const JunctionRailStep *rail = std::get_if<JunctionRailStep>(&step);
		if (rail == nullptr) continue;
		rail_steps++;
		const int x = static_cast<int>(TileX(rail->tile)) - 10;
		const int y = static_cast<int>(TileY(rail->tile)) - 10;
		if (x >= block.origin.x && x < block.origin.x + block.length && y >= block.origin.y && y < block.origin.y + block.platforms) rail_on_station++;
	}
	CHECK(rail_on_station == 0);
	CHECK(rail_steps > 0);
}
