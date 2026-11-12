/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_real_terrain.cpp Tests for terrain snapshots dumped from generated maps. */

#include "../stdafx.h"

#include "../3rdparty/catch2/catch.hpp"
#include "../junction_facts.h"
#include "../junction_generator.h"
#include "../junction_hover.h"
#include "../junction_nodes.h"
#include "../junction_rules.h"
#include "../junction_terraform.h"
#include "../junction_terrain_json.h"
#include "../junction_topology.h"

#include <algorithm>
#include <set>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <numeric>
#include <sstream>

#include "../safeguards.h"

/** Compare two snapshots member by member. */
static void CheckTerrainEqual(const JunctionTerrain &a, const JunctionTerrain &b)
{
	CHECK(a.origin.x == b.origin.x);
	CHECK(a.origin.y == b.origin.y);
	CHECK(a.width == b.width);
	CHECK(a.height == b.height);
	CHECK(a.foundation_cost == b.foundation_cost);
	CHECK(a.terraform_cost == b.terraform_cost);
	CHECK(a.build_on_slopes == b.build_on_slopes);
	CHECK(a.max_bridge_height == b.max_bridge_height);
	REQUIRE(a.tiles.size() == b.tiles.size());
	for (size_t i = 0; i < a.tiles.size(); i++) {
		const JunctionTerrainTile &x = a.tiles[i];
		const JunctionTerrainTile &y = b.tiles[i];
		const bool same = x.z_min == y.z_min && x.z_max == y.z_max && x.steep == y.steep && x.incline == y.incline && x.slope == y.slope && x.cls == y.cls && x.clear_cost == y.clear_cost;
		INFO("tile " << i);
		CHECK(same);
	}
}

TEST_CASE("real terrain: json round trip", "[junction][realterrain]")
{
	JunctionTerrain t;
	t.origin = {-3, 7};
	t.width = 5;
	t.height = 4;
	t.foundation_cost = 4321;
	t.terraform_cost = 9876;
	t.build_on_slopes = false;
	t.max_bridge_height = 9;
	const JunctionTileClass classes[] = {JunctionTileClass::Buildable, JunctionTileClass::Water, JunctionTileClass::Spannable, JunctionTileClass::Blocked};
	const DiagDirection inclines[] = {DiagDirection::NE, DiagDirection::SE, DiagDirection::SW, DiagDirection::NW, DiagDirection::Invalid};
	const Slope slopes[] = {SLOPE_FLAT, SLOPE_W, SLOPE_NE, SLOPE_STEEP_S, SLOPE_ELEVATED};
	for (int i = 0; i < t.width * t.height; i++) {
		JunctionTerrainTile tile;
		tile.z_min = static_cast<int8_t>(i - 5);
		tile.z_max = static_cast<int8_t>(i + 3);
		tile.steep = (i % 3) == 1;
		tile.incline = inclines[i % 5];
		tile.slope = slopes[(i + 2) % 5];
		tile.cls = classes[i % 4];
		tile.clear_cost = 1000000007LL * (i + 1);
		t.tiles.push_back(tile);
	}
	std::string error;
	const std::optional<JunctionTerrain> back = JunctionTerrainFromJson(JunctionTerrainToJson(t), &error);
	INFO(error);
	REQUIRE(back.has_value());
	CheckTerrainEqual(t, *back);

	CHECK(!JunctionTerrainFromJson("{\"origin\":[0]}", &error).has_value());
	CHECK(!error.empty());
}

TEST_CASE("placement on real terrain", "[junction][realterrainsweep]")
{
	namespace fs = std::filesystem;
	std::vector<fs::path> files;
	for (const fs::directory_entry &entry : fs::directory_iterator(JUNCTION_TERRAIN_DIR)) {
		if (entry.path().extension() == ".json") files.push_back(entry.path());
	}
	std::ranges::sort(files);
	INFO("fixture directory " << JUNCTION_TERRAIN_DIR);
	REQUIRE(!files.empty());

	const JunctionSpec spec = MakeTestJunctionSpec();
	std::string error;
	const JunctionTopology &t = JunctionTTopology();
	const JunctionGraphSeed seed = MakeJunctionGraphSeed(t, spec, &error);
	INFO(error);
	REQUIRE(!seed.routes.empty());
	const JunctionTrackOrder order = OrderJunctionTrackGraph(t, seed.paths, seed.graph, &error);
	INFO(error);
	const JunctionLayout seed_layout = RenderJunctionTrackPlacement(t, seed.paths, seed.graph, seed.structure, seed.programs, seed.placement, &error);
	REQUIRE(!seed_layout.tracks.empty());

	int solved = 0, failed = 0;
	int64_t micros = 0;
	std::map<std::string, int> failure_reasons;
	std::map<int, int> rule_counts;
	int heights = 0;
	for (const fs::path &file : files) {
		std::ifstream in(file);
		std::stringstream text;
		text << in.rdbuf();
		std::string load_error;
		const std::optional<JunctionTerrain> terrain = JunctionTerrainFromJson(text.str(), &load_error);
		INFO(file.filename().string() << ": " << load_error);
		REQUIRE(terrain.has_value());

		/* The seed's box centred in the snapshot. */
		const TileIndexDiffC offset = {static_cast<int16_t>(terrain->origin.x + (terrain->width - seed_layout.size.x) / 2), static_cast<int16_t>(terrain->origin.y + (terrain->height - seed_layout.size.y) / 2)};
		const JunctionFacts facts(*terrain);
		std::string place_error;
		const auto start = std::chrono::steady_clock::now();
		const JunctionTrackPlacement placement = PlaceJunctionTrackGraph(t, seed.paths, seed.graph, order, seed.structure, seed.programs, spec, seed.placement.choices, facts, offset, &place_error);
		micros += std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
		if (placement.nodes.empty()) {
			failed++;
			/* The kind is the explanation's first rule: the text before its first " (" or " [". */
			std::string kind = place_error.substr(0, place_error.find('\n'));
			kind = kind.substr(0, kind.find(" ["));
			kind = kind.substr(0, kind.find(" ("));
			failure_reasons[kind]++;
			continue;
		}
		solved++;
		std::vector<JunctionTrackRoute> routes;
		std::string render_error;
		JunctionLayout layout = RenderJunctionTrackPlacement(t, seed.paths, seed.graph, seed.structure, seed.programs, placement, &render_error, &routes);
		INFO(file.filename().string() << " " << render_error);
		REQUIRE(!layout.tracks.empty());
		std::string why;
		const bool ok = JunctionRoutesHeightsOk(routes, layout, {0, 0}, *terrain, offset, &why);
		INFO(file.filename().string() << " heights: " << why);
		CHECK(ok);
		if (!ok) heights++;
		SignalJunctionTrackRoutes(layout, routes, t, seed.paths, seed.graph, seed.programs, spec);
		for (const RuleViolation &violation : CheckJunctionRules(layout, spec, t)) {
			INFO(file.filename().string() << " rule " << to_underlying(violation.rule) << " at " << violation.tile.x << "," << violation.tile.y << ": " << violation.message);
			CHECK(false);
			rule_counts[to_underlying(violation.rule)]++;
		}
	}
	std::cout << "REALTERRAIN fixtures=" << files.size() << " solved=" << solved << " failed=" << failed << " micros=" << micros << "\n";
	for (const auto &[reason, count] : failure_reasons) std::cout << "REALTERRAIN failure " << count << " x " << reason << "\n";
	std::cout << "REALTERRAIN heights=" << heights << "\n";
	for (const auto &[rule, count] : rule_counts) std::cout << "REALTERRAIN rule " << rule << " x " << count << "\n";
	CHECK(solved + failed == static_cast<int>(files.size()));

	/* The hard-facts mode (D218): class facts only. Every solved placement renders and passes the rules; the heights are the Z solve's, not checked here. */
	int hard_solved = 0, hard_failed = 0;
	for (const fs::path &file : files) {
		std::ifstream in(file);
		std::stringstream text;
		text << in.rdbuf();
		std::string load_error;
		const std::optional<JunctionTerrain> terrain = JunctionTerrainFromJson(text.str(), &load_error);
		REQUIRE(terrain.has_value());
		const TileIndexDiffC offset = {static_cast<int16_t>(terrain->origin.x + (terrain->width - seed_layout.size.x) / 2), static_cast<int16_t>(terrain->origin.y + (terrain->height - seed_layout.size.y) / 2)};
		const JunctionFacts facts(*terrain);
		std::string place_error;
		const JunctionTrackPlacement placement = PlaceJunctionTrackGraph(t, seed.paths, seed.graph, order, seed.structure, seed.programs, spec, seed.placement.choices, facts, offset, &place_error, false);
		if (placement.nodes.empty()) {
			hard_failed++;
			continue;
		}
		hard_solved++;
		std::vector<JunctionTrackRoute> routes;
		std::string render_error;
		JunctionLayout layout = RenderJunctionTrackPlacement(t, seed.paths, seed.graph, seed.structure, seed.programs, placement, &render_error, &routes);
		INFO(file.filename().string() << " " << render_error);
		REQUIRE(!layout.tracks.empty());
		SignalJunctionTrackRoutes(layout, routes, t, seed.paths, seed.graph, seed.programs, spec);
		for (const RuleViolation &violation : CheckJunctionRules(layout, spec, t)) {
			INFO(file.filename().string() << " rule " << to_underlying(violation.rule) << " at " << violation.tile.x << "," << violation.tile.y << ": " << violation.message);
			CHECK(false);
		}
	}
	std::cout << "REALTERRAIN hard solved=" << hard_solved << " failed=" << hard_failed << "\n";

	/* The height solve (D218, DESIGN-G1 §15): on every hard-facts placement, levels per edge, a terraform plan and its cost. The terraformed terrain must pass the height facts: the same placement on it with heights, and every route of its render meets the ground. */
	int tf_solved = 0, tf_failed = 0, tf_conflicts = 0;
	std::vector<int64_t> costs;
	std::vector<int64_t> solve_micros;
	std::map<std::string, int> tf_reasons;
	for (const fs::path &file : files) {
		std::ifstream in(file);
		std::stringstream text;
		text << in.rdbuf();
		std::string load_error;
		const std::optional<JunctionTerrain> terrain = JunctionTerrainFromJson(text.str(), &load_error);
		REQUIRE(terrain.has_value());
		const TileIndexDiffC offset = {static_cast<int16_t>(terrain->origin.x + (terrain->width - seed_layout.size.x) / 2), static_cast<int16_t>(terrain->origin.y + (terrain->height - seed_layout.size.y) / 2)};
		const JunctionFacts facts(*terrain);
		std::string place_error;
		auto clock = [] { return std::chrono::steady_clock::now(); };
		const auto hard_start = clock();
		const JunctionTrackPlacement hard = PlaceJunctionTrackGraph(t, seed.paths, seed.graph, order, seed.structure, seed.programs, spec, seed.placement.choices, facts, offset, &place_error, false);
		const auto hard_end = clock();
		if (hard.nodes.empty()) continue;
		const JunctionTrackPlacement solved_placement = PlaceJunctionTrackGraph(t, seed.paths, seed.graph, order, seed.structure, seed.programs, spec, seed.placement.choices, facts, offset, &place_error, true, true, &*terrain);
		const auto solve_end = clock();
		INFO(file.filename().string() << ": " << place_error);
		/* Time of the solve: the second placement less the first (X is solved twice), render and levels included. */
		solve_micros.push_back(std::chrono::duration_cast<std::chrono::microseconds>((solve_end - hard_end) - (hard_end - hard_start)).count());
		if (solved_placement.nodes.empty()) {
			tf_failed++;
			if (place_error.find("terraform conflict") != std::string::npos) tf_conflicts++;
			tf_reasons[place_error.substr(0, place_error.find(':'))]++;
			std::cout << "REALTERRAIN terraform fail " << file.filename().string() << ": " << place_error << "\n";
			continue;
		}
		tf_solved++;
		costs.push_back(solved_placement.height_cost.base());
		const JunctionTerrain after = ApplyJunctionTerraform(*terrain, solved_placement.terraform);
		const JunctionFacts after_facts(after);
		std::string again_error;
		const JunctionTrackPlacement again = PlaceJunctionTrackGraph(t, seed.paths, seed.graph, order, seed.structure, seed.programs, spec, seed.placement.choices, after_facts, offset, &again_error);
		INFO(file.filename().string() << " terraformed terrain: " << again_error);
		REQUIRE(!again.nodes.empty());
		/* The terraformed ground re-places what the terraform-mode placement placed (that placement may differ from the hard-facts one). */
		CHECK(again.item_rows == solved_placement.item_rows);
		CHECK(again.columns == solved_placement.columns);
		CHECK(again.size.x == solved_placement.size.x);
		CHECK(again.size.y == solved_placement.size.y);
		std::vector<JunctionTrackRoute> routes;
		std::string render_error;
		JunctionLayout layout = RenderJunctionTrackPlacement(t, seed.paths, seed.graph, seed.structure, seed.programs, again, &render_error, &routes);
		REQUIRE(!layout.tracks.empty());
		std::string why;
		const bool ok = JunctionRoutesHeightsOk(routes, layout, {0, 0}, after, offset, &why);
		INFO(file.filename().string() << " heights after terraform: " << why);
		CHECK(ok);
	}
	/* Z2e: the joint level solve (RECON-joint-levels) solved 17 of the 18 hard-solved fixtures at the game's prices; H3 levels the prio elements' tiles too (flat, at the main-line tile's level), which 4 more fixtures (arctic-hilly-13, arctic-hilly-3, temperate-hilly-4, temperate-mountainous-1) cannot take. */
	CHECK(tf_solved >= 18);
	std::ranges::sort(costs);
	const int64_t cost_sum = std::accumulate(costs.begin(), costs.end(), int64_t(0));
	std::cout << "REALTERRAIN terraform solved=" << tf_solved << " failed=" << tf_failed << " cost_mean=" << (costs.empty() ? int64_t(0) : cost_sum / static_cast<int64_t>(costs.size())) << "\n";
	std::cout << "REALTERRAIN terraform conflicts=" << tf_conflicts << " cost min=" << (costs.empty() ? int64_t(0) : costs.front()) << " median=" << (costs.empty() ? int64_t(0) : costs[costs.size() / 2]) << " max=" << (costs.empty() ? int64_t(0) : costs.back()) << "\n";
	if (!solve_micros.empty()) std::cout << "REALTERRAIN terraform solve micros mean=" << std::accumulate(solve_micros.begin(), solve_micros.end(), int64_t(0)) / static_cast<int64_t>(solve_micros.size()) << " max=" << *std::ranges::max_element(solve_micros) << "\n";
	for (const auto &[reason, count] : tf_reasons) std::cout << "REALTERRAIN terraform failure " << count << " x " << reason << "\n";
}

TEST_CASE("hover plan on real terrain", "[junction][realterrainsweep]")
{
	namespace fs = std::filesystem;
	std::vector<fs::path> files;
	for (const fs::directory_entry &entry : fs::directory_iterator(JUNCTION_TERRAIN_DIR)) {
		if (entry.path().extension() == ".json") files.push_back(entry.path());
	}
	std::ranges::sort(files);
	REQUIRE(!files.empty());

	const JunctionSpec spec = MakeTestJunctionSpec();
	std::string error;
	const JunctionTopology &t = JunctionTTopology();
	const JunctionGraphSeed seed = MakeJunctionGraphSeed(t, spec, &error);
	INFO(error);
	REQUIRE(!seed.routes.empty());
	const JunctionLayout seed_layout = RenderJunctionTrackPlacement(t, seed.paths, seed.graph, seed.structure, seed.programs, seed.placement, &error);
	REQUIRE(!seed_layout.tracks.empty());

	int planned = 0, failed = 0;
	std::vector<double> millis;
	for (const fs::path &file : files) {
		std::ifstream in(file);
		std::stringstream text;
		text << in.rdbuf();
		std::string load_error;
		const std::optional<JunctionTerrain> terrain = JunctionTerrainFromJson(text.str(), &load_error);
		REQUIRE(terrain.has_value());
		const TileIndexDiffC offset = {static_cast<int16_t>(terrain->origin.x + (terrain->width - seed_layout.size.x) / 2), static_cast<int16_t>(terrain->origin.y + (terrain->height - seed_layout.size.y) / 2)};

		const auto start = std::chrono::steady_clock::now();
		const JunctionHoverPlan plan = PlanJunctionHover(seed, spec, *terrain, offset);
		millis.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
		INFO(file.filename().string() << ": " << plan.why);
		if (!plan.layout.has_value()) {
			failed++;
			CHECK(!plan.why.empty());
			continue;
		}
		planned++;
		CHECK(!plan.layout->signals.empty());
		/* The terraformed ground passes the height facts: the placement's routes meet it (Z2e's check). */
		const JunctionTerrain after = ApplyJunctionTerraform(*terrain, plan.terraform);
		const JunctionFacts facts(*terrain);
		const JunctionTrackOrder order = OrderJunctionTrackGraph(t, seed.paths, seed.graph, &error);
		std::string again_error;
		const JunctionTrackPlacement again = PlaceJunctionTrackGraph(t, seed.paths, seed.graph, order, seed.structure, seed.programs, spec, seed.placement.choices, facts, offset, &again_error, true, true, &*terrain);
		INFO("placement: " << again_error);
		REQUIRE(!again.nodes.empty());
		CHECK(again.height_cost == plan.height_cost);
		std::vector<JunctionTrackRoute> routes;
		std::string render_error;
		const JunctionLayout layout = RenderJunctionTrackPlacement(t, seed.paths, seed.graph, seed.structure, seed.programs, again, &render_error, &routes);
		/* The routes checked are those of the plan's layout (the plan's has the signal pass on top). */
		CHECK(layout.size.x == plan.layout->size.x);
		CHECK(layout.size.y == plan.layout->size.y);
		std::string why;
		CHECK(JunctionRoutesHeightsOk(routes, layout, {0, 0}, after, offset, &why));
		INFO(why);
		for (const RuleViolation &violation : CheckJunctionRules(*plan.layout, spec, t)) {
			INFO("rule " << to_underlying(violation.rule) << " at " << violation.tile.x << "," << violation.tile.y << ": " << violation.message);
			CHECK(false);
		}
	}
	const double mean = millis.empty() ? 0.0 : std::accumulate(millis.begin(), millis.end(), 0.0) / static_cast<double>(millis.size());
	const double max = millis.empty() ? 0.0 : *std::ranges::max_element(millis);
	std::cout << "HOVERPLAN fixtures=" << files.size() << " planned=" << planned << " failed=" << failed << " ms_mean=" << mean << " ms_max=" << max << "\n";
	CHECK(planned >= 19);
	REQUIRE(max < 100.0);
}
