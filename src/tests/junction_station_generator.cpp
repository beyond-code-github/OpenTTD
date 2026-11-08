/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_station_generator.cpp Test functionality from junction_station_generator. */

#include "../stdafx.h"

#include "../3rdparty/catch2/catch.hpp"

#include "../junction_route.h"
#include "../junction_station_generator.h"
#include "../core/format.hpp"

#include "../safeguards.h"

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
