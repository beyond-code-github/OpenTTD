/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file planar_components.cpp Test the planar component solver (DESIGN-components §6). */

#include "../../../stdafx.h"

#include "../../../3rdparty/catch2/catch.hpp"

#include "../../../junction/junction_spec.h"
#include "../../../junction/planar/planar_components.h"
#include "../../../junction/planar/planar_plan.h"
#include "../../../junction/planar/planar_topology.h"

#include <set>
#include <string>
#include <utility>

#include "../../../safeguards.h"

namespace {

/**
 * Solve a topology's components and check what every hub must satisfy.
 * @param text The topology text.
 * @param area_limit The most area (tiles) the solve may need.
 */
void CheckSolves(const std::string &text, double area_limit)
{
	std::string error;
	const PlanarTopology topology = ParsePlanarTopology(text, &error);
	REQUIRE(error.empty());
	const JunctionSpec spec = MakeTestJunctionSpec();
	PlanarComponentLayout layout;
	std::string why;
	REQUIRE(SolvePlanarComponents(topology, spec, layout, &why));
	INFO("variant " << layout.variant << " area " << layout.area << " size " << layout.width << "x" << layout.height << " in " << layout.micros / 1000 << " ms");
	CHECK(layout.area <= area_limit);
	CHECK(layout.width > 0);
	CHECK(layout.height > 0);
	std::set<std::pair<uint32_t, uint32_t>> covered;
	for (const PlanarRouteProgram &program : layout.routes) {
		REQUIRE(program.route < topology.routes.size());
		REQUIRE(program.path < topology.routes[program.route].paths.size());
		covered.emplace(program.route, program.path);
		REQUIRE(!program.pieces.empty());
		CHECK((program.pieces.front().heading % 2) == 0);
	}
	for (uint32_t r = 0; r < topology.routes.size(); r++) {
		for (uint32_t p = 0; p < topology.routes[r].paths.size(); p++) {
			INFO("route " << topology.routes[r].name << " path " << p);
			CHECK(covered.contains({r, p}));
		}
	}
}

} // namespace

TEST_CASE("planar components: LL² solves at the probe's area", "[junction][planarcomponents]")
{
	CheckSolves(PlanarLL2Text(), 807);
}

TEST_CASE("planar components: T2 solves at the probe's area", "[junction][planarcomponents]")
{
	CheckSolves(PlanarTText(2), 963);
}

TEST_CASE("planar components: a topology without a composition is refused", "[junction][planarcomponents]")
{
	std::string error;
	const PlanarTopology topology = ParsePlanarTopology(PlanarTText(1), &error);
	REQUIRE(error.empty());
	PlanarComponentLayout layout;
	std::string why;
	CHECK(!SolvePlanarComponents(topology, MakeTestJunctionSpec(), layout, &why));
	CHECK(!why.empty());
}

TEST_CASE("planar components: the test spec derives the probe's constants", "[junction][planarcomponents]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	CHECK(spec.tl == 4);
	CHECK(DeriveCurveSpacing(spec.reference_speed, spec.curve_speed, spec.tl) == 5);
	CHECK(spec.prio_tiles == 14);
}
