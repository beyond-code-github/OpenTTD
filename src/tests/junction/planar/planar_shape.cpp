/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file planar_shape.cpp Test the shape program: its shapes pass the shape check and turn less than the default. */

#include "../../../stdafx.h"

#include "../../../3rdparty/catch2/catch.hpp"

#include "../../../junction/junction_spec.h"
#include "../../../junction/planar/planar_components.h"
#include "../../../junction/planar/planar_paths.h"
#include "../../../junction/planar/planar_plan.h"
#include "../../../junction/planar/planar_shape.h"
#include "../../../junction/planar/planar_topology.h"

#include <chrono>
#include <string>
#include <vector>

#include "../../../safeguards.h"

/** What a loop over a hub's variants found. */
struct ShapeLoop {
	int variants = 0;
	int built = 0;
	int default_turns = 0;
	int program_turns = 0;
	double max_ms = 0;
	double total_ms = 0;
};

/** Run the program on every over choice and flip of a hub whose default paths build; check what the tests require of each. */
static ShapeLoop LoopVariants(const std::string &text)
{
	ShapeLoop loop;
	std::string error;
	const PlanarTopology topology = ParsePlanarTopology(text, &error);
	REQUIRE(error.empty());
	const PlanarChannels base = BuildPlanarChannels(topology);
	const size_t open = PlanarOpenCrossings(base).size(), triangles = PlanarTriangles(base).size();
	for (uint64_t flips = 0; flips < (uint64_t{1} << triangles); flips++) {
		const PlanarChannels channels = FlipPlanarTriangles(base, flips);
		for (uint64_t choice = 0; choice < (uint64_t{1} << open); choice++) {
			loop.variants++;
			const PlanarPaths def = BuildPlanarPaths(topology, channels, choice);
			if (!def.why.empty()) continue;
			loop.built++;
			int turns = -1;
			std::string why;
			const auto t0 = std::chrono::steady_clock::now();
			const std::vector<PlanarChannelPath> shape = SolvePlanarShape(topology, channels, def.over, &turns, &why);
			const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
			loop.max_ms = std::max(loop.max_ms, ms);
			loop.total_ms += ms;
			INFO("flips " << flips << " choice " << choice << ": " << why);
			REQUIRE(!shape.empty());
			CHECK(CheckPlanarShape(topology, channels, def.over, shape).empty());
			CHECK(turns <= PlanarShapeTurns(def.paths));
			CHECK(turns == PlanarShapeTurns(shape));
			loop.default_turns += PlanarShapeTurns(def.paths);
			loop.program_turns += turns;
		}
	}
	return loop;
}

TEST_CASE("planar shape: every variant's program shape passes the shape check", "[junction][planarshape]")
{
	for (const std::string &text : {PlanarTText(2), PlanarTText(3)}) {
		const ShapeLoop loop = LoopVariants(text);
		CHECK(loop.built > 0);
	}
}

TEST_CASE("planar shape: the program never turns more than the default", "[junction][planarshape]")
{
	for (const std::string &text : {PlanarTText(2), PlanarTText(3)}) {
		const ShapeLoop loop = LoopVariants(text);
		CHECK(loop.program_turns <= loop.default_turns);
		WARN("variants " << loop.variants << " built " << loop.built << " default turns " << loop.default_turns << " program turns " << loop.program_turns << " max ms " << loop.max_ms);
	}
}

TEST_CASE("planar shape: LL2 program shapes pass the shape check and never turn more than the default", "[junction][planarshape][slow]")
{
	const ShapeLoop loop = LoopVariants(PlanarLL2Text());
	CHECK(loop.built > 0);
	CHECK(loop.program_turns <= loop.default_turns);
	WARN("LL2 variants " << loop.variants << " built " << loop.built << " default turns " << loop.default_turns << " program turns " << loop.program_turns << " max ms " << loop.max_ms);
}

TEST_CASE("planar shape: the program turns less than the default on T2", "[junction][planarshape]")
{
	std::string error;
	const PlanarTopology topology = ParsePlanarTopology(PlanarTText(2), &error);
	REQUIRE(error.empty());
	const JunctionSpec spec = MakeTestJunctionSpec();
	std::string why;
	const std::vector<PlanarChannelPath> def = PlanarDefaultShape(topology, spec, "o0t2f1s1", &why);
	INFO(why);
	REQUIRE(!def.empty());
	const PlanarChannels channels = FlipPlanarTriangles(BuildPlanarChannels(topology), 1);
	const PlanarPaths built = BuildPlanarPaths(topology, channels, 0);
	REQUIRE(built.why.empty());
	int turns = -1;
	const std::vector<PlanarChannelPath> shape = SolvePlanarShape(topology, channels, built.over, &turns, &why);
	INFO(why);
	REQUIRE(!shape.empty());
	WARN("T2 o0t2f1s1 default turns " << PlanarShapeTurns(def) << " program turns " << turns);
	CHECK(turns < PlanarShapeTurns(def));
}

TEST_CASE("planar shape: the T2 loop takes under a second", "[junction][planarshape]")
{
	const ShapeLoop loop = LoopVariants(PlanarTText(2));
	WARN("T2 loop " << loop.total_ms << " ms over " << loop.built << " variants");
	CHECK(loop.total_ms < 1000);
}
