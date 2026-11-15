/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file planar_plan.cpp Test the planar model end to end: seed, placement, render, levels, signals and the checker (DESIGN-planar §5). */

#include "../../../stdafx.h"

#include "../../../3rdparty/catch2/catch.hpp"

#include "../../../junction/junction_route.h"
#include "../../../junction/junction_rules.h"
#include "../../../junction/junction_spec.h"
#include "../../../junction/planar/planar_plan.h"
#include "../../../junction/terrain/junction_terrain.h"

#include "../../../slope_func.h"

#include <algorithm>
#include <chrono>
#include <set>
#include <tuple>
#include <string>

#include "../../../safeguards.h"

namespace {

/** A flat snapshot around a plan's box. */
JunctionTerrain FlatTerrain(int width, int height)
{
	JunctionTerrain t;
	t.origin = {0, 0};
	t.width = width;
	t.height = height;
	t.tiles.assign(static_cast<size_t>(width) * height, JunctionTerrainTile{});
	t.foundation_cost = 250;
	t.build_on_slopes = true;
	return t;
}

/** The ridge fixture: flat at 0, a ridge across the branch area rising over row r (SE), level at 1 over rows r + 1 and r + 2, falling over row r + 3 (NW). */
JunctionTerrain RidgeTerrain(int width, int height, int r)
{
	JunctionTerrain t = FlatTerrain(width, height);
	const auto row = [&](int y, int z_min, int z_max, DiagDirection incline) {
		for (int x = 0; x < width; x++) {
			JunctionTerrainTile &tile = t.tiles[static_cast<size_t>(y) * width + x];
			tile.z_min = static_cast<int8_t>(z_min);
			tile.z_max = static_cast<int8_t>(z_max);
			tile.incline = incline;
			tile.slope = incline == DiagDirection::Invalid ? SLOPE_FLAT : InclinedSlope(incline);
		}
	};
	row(r, 0, 1, DiagDirection::SE);
	row(r + 1, 1, 1, DiagDirection::Invalid);
	row(r + 2, 1, 1, DiagDirection::Invalid);
	row(r + 3, 0, 1, DiagDirection::NW);
	return t;
}

/** The seed, required solved. */
std::shared_ptr<const PlanarSeed> Seed(const std::string &text, const JunctionSpec &spec)
{
	std::shared_ptr<const PlanarSeed> seed = PlanarSeedFor(text, spec);
	INFO("seed: " << seed->why);
	REQUIRE(seed->why.empty());
	return seed;
}

/** Place, and require the layout and a clean checker. */
void RequirePlacedClean(const PlanarSeed &seed, const JunctionSpec &spec, const JunctionTerrain *terrain, TileIndexDiffC offset)
{
	const PlanarPlan plan = PlanPlanarJunction(seed, spec, terrain, offset, false);
	INFO("plan: " << plan.why);
	REQUIRE(plan.layout.has_value());
	INFO(RenderJunctionAscii(*plan.layout));
	const std::vector<RuleViolation> violations = CheckJunctionRules(*plan.layout, spec, PlanarCheckerTopology(seed.topology));
	for (const RuleViolation &v : violations) UNSCOPED_INFO(v.message << " at " << v.tile.x << "," << v.tile.y);
	CHECK(violations.empty());
}

}

TEST_CASE("planar: the T at one lane places on flat ground, checker clean, seed under 5 s", "[junction][planar][slow]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const std::shared_ptr<const PlanarSeed> seed = Seed(PlanarTText(1), spec);
	CHECK(seed->micros < 5'000'000);
	RequirePlacedClean(*seed, spec, nullptr, {0, 0});
}

TEST_CASE("planar: the T at two lanes places on flat ground, checker clean, seed under 5 s", "[junction][planar][slow]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const std::shared_ptr<const PlanarSeed> seed = Seed(PlanarTText(2), spec);
	CHECK(seed->micros < 5'000'000);
	RequirePlacedClean(*seed, spec, nullptr, {0, 0});
}

TEST_CASE("planar: the T at one lane places on the ridge, checker clean", "[junction][planar][slow]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const std::shared_ptr<const PlanarSeed> seed = Seed(PlanarTText(1), spec);
	const int margin = 4;
	const JunctionTerrain ridge = RidgeTerrain(seed->geometry.width + 2 * margin, seed->geometry.height + 2 * margin, margin + seed->geometry.height / 2);
	RequirePlacedClean(*seed, spec, &ridge, {static_cast<int16_t>(margin), static_cast<int16_t>(margin)});
}

TEST_CASE("planar: the two-lane LL² any hub places, checker clean", "[junction][planar][slow]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const std::shared_ptr<const PlanarSeed> seed = Seed(PlanarLL2Text(), spec);
	RequirePlacedClean(*seed, spec, nullptr, {0, 0});
}

TEST_CASE("planar: hover on the T at one lane", "[junction][planar][.bench]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const std::shared_ptr<const PlanarSeed> seed = Seed(PlanarTText(1), spec);
	const JunctionTerrain flat = FlatTerrain(seed->geometry.width + 16, seed->geometry.height + 16);
	constexpr int RUNS = 20;
	const auto start = std::chrono::steady_clock::now();
	for (int i = 0; i < RUNS; i++) {
		const JunctionHoverPlan plan = PlanPlanarHoverNear(*seed, spec, flat, {static_cast<int16_t>(2 + i % 4), 4});
		INFO(plan.why);
		REQUIRE(plan.layout.has_value());
	}
	const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() / RUNS;
	WARN("planar T1 hover: " << ms << " ms per plan");
	CHECK(ms < 100);
}

TEST_CASE("planar: LL² any has the composed hub's lane paths, prio on the fixed lane", "[junction][planar]")
{
	std::string error;
	const PlanarTopology topology = ParsePlanarTopology(PlanarLL2Text(), &error);
	INFO(error);
	REQUIRE(error.empty());
	CHECK(topology.routes.size() == 12);
	CHECK(std::ranges::count_if(topology.routes, [](const PlanarRoute &r) { return r.prio; }) == 6);
	const JunctionTopology checker = PlanarCheckerTopology(topology);
	REQUIRE(checker.groups.size() == 6);
	CHECK(checker.movements.size() == 6);
	/* The hub's lane paths (ComposeJunctionHub on the width-2 T, read from its lane path list): (from slot, to slot, prio). */
	const std::set<std::tuple<int, int, bool>> hub = {
		{0, 11, true}, {0, 6, false}, {0, 7, false}, {1, 11, false}, {1, 10, false}, {1, 6, true},
		{4, 11, false}, {4, 10, false}, {4, 3, true}, {5, 10, true}, {5, 2, false}, {5, 3, false},
		{8, 2, false}, {8, 3, false}, {8, 7, true}, {9, 2, true}, {9, 6, false}, {9, 7, false},
	};
	std::set<std::tuple<int, int, bool>> got;
	for (const JunctionLanePath &p : checker.lane_paths) got.insert({p.from, p.to, p.prio});
	CHECK(checker.lane_paths.size() == 18);
	CHECK(got == hub);
}
