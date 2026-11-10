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
#include <optional>
#include <sstream>
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

TEST_CASE("the T map reports its time by phase", "[junctionmap]")
{
	const JunctionTMapResult result = ConstructJunctionTMap(MakeTestJunctionSpec());
	CHECK(result.micros_route >= 0);
	CHECK(result.micros_skeleton >= 0);
	CHECK(result.micros_finish >= 0);
	CHECK(result.micros_check >= 0);
	CHECK(result.micros_route + result.micros_skeleton + result.micros_finish + result.micros_check <= result.micros);
	CHECK(static_cast<int>(result.micros_per_height.size()) == result.heights_tried);
}

TEST_CASE("the first height is a lower bound", "[junctionmap]")
{
	for (const auto &[name, spec] : MapSpecs()) {
		const int bound = JunctionTMapFirstHeight(spec);
		CHECK(bound == spec.ml_gap + 10);
		const JunctionTMapResult below = ConstructJunctionTMap(spec, nullptr, bound - 1);
		std::ostringstream expected;
		expected << "height " << bound - 1 << ": A->S failed";
		INFO(below.log);
		CHECK(below.log.find(expected.str()) != std::string::npos);
		const JunctionTMapResult from_bound = ConstructJunctionTMap(spec);
		const JunctionTMapResult from_old = ConstructJunctionTMap(spec, nullptr, spec.ml_gap + 5);
		REQUIRE(from_bound.layout.has_value());
		CHECK(from_bound.height == from_old.height);
		std::cout << "BENCH first height spec=" << name << " before: heights=" << from_old.heights_tried << " micros=" << from_old.micros << " after: heights=" << from_bound.heights_tried << " micros=" << from_bound.micros << "\n";
	}
}

TEST_CASE("the line-router T matches or beats the old-router T", "[junctionmap]")
{
	for (const auto &[name, spec] : MapSpecs()) {
		const JunctionTMapResult line = ConstructJunctionTMap(spec, nullptr, std::nullopt, true);
		const JunctionTMapResult old = ConstructJunctionTMap(spec, nullptr, std::nullopt, false);
		if (!line.clean || !old.clean) {
			PrintMapFailure(name + " line router", line);
			PrintMapFailure(name + " old router", old);
		}
		REQUIRE(line.layout.has_value());
		REQUIRE(old.layout.has_value());
		CHECK(line.clean);
		CHECK(old.clean);
		const JunctionCompactness line_compact = MeasureJunctionCompactness(*line.layout);
		const JunctionCompactness old_compact = MeasureJunctionCompactness(*old.layout);
		INFO("spec " << name << " line router " << line_compact.occupied << "+" << line_compact.slivers << " old router " << old_compact.occupied << "+" << old_compact.slivers);
		CHECK(line_compact.occupied + line_compact.slivers <= old_compact.occupied + old_compact.slivers);
		std::cout << "BENCH tmap route compare spec=" << name << " line=" << line.micros_route << " old=" << old.micros_route << "\n";
	}
}

TEST_CASE("line router results are unchanged by the speed work", "[linerouter]")
{
	/* Each request's moves, cost and expansions, recorded before the speed work (P2). */
	const std::vector<std::pair<std::string, std::string>> expected = {
		{"test",
		"- 0 65626\n"
		"LFFFFFFFFFFFFFLF 1800 123\n"
		"FB2LFFFLFFFFFFRFL 2700 77\n"
		"- 0 31886\n"
		"LFFFLFFRFFFFLF 1800 58\n"
		"FB2FLFFFLFFFFFFFFRFL 3000 172\n"
		"RFFFRFFLFFFFFR 1700 83\n"
		"RFFFFFFFFFFFRFFFFFFFFFFFFLFR 3100 817\n"},
		{"pinned",
		"- 0 42338\n"
		"LFFFFFFFFFFFFFLF 1800 123\n"
		"FB2FLFFFFL 1800 50\n"
		"- 0 15565\n"
		"LFFFLFFRFFFFLF 1800 58\n"
		"FB2FFLFFFFL 1900 72\n"
		"RFFFFFRFFFLFR 1600 55\n"
		"RFFFFFFFRFFLFFFFFR 2100 236\n"},
	};
	/* D173: the line router starts at the flat map's height, so its trace is the tail of the old one: the lines of the built height. */
	constexpr size_t BUILT_HEIGHT_LINES = 4; ///< A->S, N->A, B->S, N->B.
	for (const auto &[name, trace] : expected) {
		size_t cut = trace.size();
		for (size_t i = 0; i < BUILT_HEIGHT_LINES; i++) cut = trace.rfind('\n', cut - 2) + 1;
		for (const auto &[spec_name, spec] : MapSpecs()) {
			if (spec_name != name) continue;
			CHECK(ConstructJunctionTMap(spec, nullptr, std::nullopt, true).line_router_trace == trace.substr(cut));
		}
	}
}

TEST_CASE("the line-router T starts at the flat height", "[junctionmap]")
{
	for (const auto &[name, spec] : MapSpecs()) {
		const JunctionTMapResult old = ConstructJunctionTMap(spec, nullptr, std::nullopt, false);
		const JunctionTMapResult line = ConstructJunctionTMap(spec, nullptr, std::nullopt, true);
		REQUIRE(old.layout.has_value());
		REQUIRE(line.layout.has_value());
		INFO("spec " << name);
		CHECK(line.heights_tried == 1);
		CHECK(line.height == old.height);
		CHECK(line.clean);
		CHECK(line.micros_flat > 0);
		CHECK(line.micros_flat <= line.micros);
		CHECK(old.micros_flat == 0);
		const JunctionLayout &a = *line.layout;
		const JunctionLayout &b = *old.layout;
		REQUIRE(a.tracks.size() == b.tracks.size());
		for (size_t i = 0; i < a.tracks.size(); i++) {
			CHECK(a.tracks[i].offset.x == b.tracks[i].offset.x);
			CHECK(a.tracks[i].offset.y == b.tracks[i].offset.y);
			CHECK(a.tracks[i].tracks == b.tracks[i].tracks);
		}
		REQUIRE(a.bridges.size() == b.bridges.size());
		for (size_t i = 0; i < a.bridges.size(); i++) {
			CHECK(a.bridges[i].start.x == b.bridges[i].start.x);
			CHECK(a.bridges[i].start.y == b.bridges[i].start.y);
			CHECK(a.bridges[i].end.x == b.bridges[i].end.x);
			CHECK(a.bridges[i].end.y == b.bridges[i].end.y);
			CHECK(a.bridges[i].tunnel == b.bridges[i].tunnel);
		}
		REQUIRE(a.signals.size() == b.signals.size());
		for (size_t i = 0; i < a.signals.size(); i++) {
			CHECK(a.signals[i].offset.x == b.signals[i].offset.x);
			CHECK(a.signals[i].offset.y == b.signals[i].offset.y);
			CHECK(a.signals[i].facing == b.signals[i].facing);
			CHECK(a.signals[i].type == b.signals[i].type);
			CHECK(a.signals[i].variant == b.signals[i].variant);
		}
	}
}

TEST_CASE("BENCH the constructed T", "[junctionmap]")
{
	for (const auto &[name, spec] : MapSpecs()) {
		for (const bool line_router : {true, false}) {
			const JunctionTMapResult result = ConstructJunctionTMap(spec, nullptr, std::nullopt, line_router);
			REQUIRE(result.layout.has_value());
			const JunctionCompactness compact = MeasureJunctionCompactness(*result.layout);
			std::cout << "BENCH tmap router=" << (line_router ? "line" : "old") << " spec=" << name << " height=" << result.height << " micros=" << result.micros << " occupied=" << compact.occupied << " slivers=" << compact.slivers << " clean=" << (result.clean ? 1 : 0) << " flat=" << result.micros_flat << " route=" << result.micros_route << " skeleton=" << result.micros_skeleton << " finish=" << result.micros_finish << " check=" << result.micros_check << " heights=" << result.heights_tried << "\n";
		}
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
