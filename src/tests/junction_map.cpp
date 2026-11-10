/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_map.cpp Test functionality from the constructed T map (DESIGN-O4 "G1"). */

#include "../stdafx.h"

#include "../3rdparty/catch2/catch.hpp"

#include "../junction_compact.h"
#include "../junction_generator.h"
#include "../junction_route.h"
#include "../junction_rules.h"
#include "../track_func.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

#include "../safeguards.h"

/** The test spec at its old coverage of one train length and without the ML delay check (a copy of PinnedTestSpec in junction_generator.cpp). */
static JunctionSpec PinnedMapSpec()
{
	JunctionSpec spec = MakeTestJunctionSpec();
	spec.prio_tiles = spec.tl;
	spec.prio_strict = false;
	return spec;
}

/** The specs under test, by name. */
static std::vector<std::pair<std::string, JunctionSpec>> MapSpecs()
{
	return {{"test", MakeTestJunctionSpec()}, {"pinned", PinnedMapSpec()}};
}

/** Print what a failed map needs to be diagnosed. */
static void PrintMapFailure(const std::string &name, const JunctionTMapResult &result)
{
	std::cout << "spec " << name << " height " << result.height << " stagger " << static_cast<int>(result.stagger) << " phase " << static_cast<int>(result.e_phase) << "\n";
	for (const RuleViolation &v : result.violations) {
		std::cout << "  rule " << static_cast<int>(v.rule) << " tile " << v.tile.x << "," << v.tile.y << ": " << v.message << "\n";
	}
	std::cout << result.log;
	if (result.layout.has_value()) std::cout << RenderJunctionAscii(*result.layout) << "\n";
}

TEST_CASE("the constructed T is rule-clean", "[junctionmap]")
{
	for (const auto &[name, spec] : MapSpecs()) {
		const JunctionTMapResult result = ConstructJunctionTMap(spec);
		if (!result.clean || result.layouts_built != 1) PrintMapFailure(name, result);
		CHECK(result.clean);
		CHECK(result.layouts_built == 1);
	}
}

TEST_CASE("the T map is built once per height", "[junctionmap]")
{
	for (const auto &[name, spec] : MapSpecs()) {
		const JunctionTMapResult result = ConstructJunctionTMap(spec);
		CHECK(result.layouts_built <= result.heights_tried);
		CHECK(result.layouts_built == 1);
		CHECK(result.layout.has_value());
	}
}

TEST_CASE("the E-side phase follows D148a", "[junctionmap]")
{
	const std::vector<std::pair<int, int>> expected{{1, 0}, {3, 0}, {4, 0}, {5, 1}, {7, 1}, {9, 1}, {14, 0}, {15, 1}};
	for (const auto &[prio_tiles, phase] : expected) {
		INFO("prio_tiles " << prio_tiles);
		CHECK(JunctionEWindowPhase(prio_tiles) == phase);
	}
}

TEST_CASE("the doubled bridge spans the branch crossings", "[junctionmap]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionTMapResult result = ConstructJunctionTMap(spec);
	REQUIRE(result.layout.has_value());
	const JunctionLayout &layout = *result.layout;

	/* The doubled main-line bridges: the two horizontal bridges on adjacent rows. */
	std::vector<JunctionBridge> mains;
	for (const JunctionBridge &bridge : layout.bridges) {
		if (bridge.start.y == bridge.end.y) mains.push_back(bridge);
	}
	REQUIRE(mains.size() >= 2);
	size_t crossings = 0;
	for (const JunctionBridge &bridge : mains) {
		const int lo = std::min(bridge.start.x, bridge.end.x);
		const int hi = std::max(bridge.start.x, bridge.end.x);
		/* A branch piece on the bridge's row: a tile with a vertical track under or beside the heads. */
		for (const JunctionTrack &jt : layout.tracks) {
			if (jt.offset.y != bridge.start.y || !jt.tracks.Test(Track::Y)) continue;
			crossings++;
			INFO("bridge row " << bridge.start.y << " heads " << lo << " " << hi << " branch piece at x " << jt.offset.x);
			CHECK(jt.offset.x > lo);
			CHECK(jt.offset.x < hi);
		}
	}
	CHECK(crossings > 0);
}

TEST_CASE("BENCH the constructed T", "[junctionmap]")
{
	for (const auto &[name, spec] : MapSpecs()) {
		const JunctionTMapResult result = ConstructJunctionTMap(spec);
		REQUIRE(result.layout.has_value());
		const JunctionCompactness compact = MeasureJunctionCompactness(*result.layout);
		std::cout << "BENCH tmap spec=" << name << " height=" << result.height << " micros=" << result.micros << " occupied=" << compact.occupied << " slivers=" << compact.slivers << " clean=" << (result.clean ? 1 : 0) << "\n";
	}
}

TEST_CASE("BENCH the old generator, for comparison", "[.bench]")
{
	for (const auto &[name, spec] : MapSpecs()) {
		const auto start = std::chrono::steady_clock::now();
		const JunctionGenerateResult result = GenerateJunction(spec);
		const int64_t micros = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
		REQUIRE(result.layout.has_value());
		const JunctionCompactness compact = MeasureJunctionCompactness(*result.layout);
		std::cout << "BENCH old spec=" << name << " micros=" << micros << " occupied=" << compact.occupied << " slivers=" << compact.slivers << "\n";
	}
}
