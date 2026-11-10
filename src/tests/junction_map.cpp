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
#include "../junction_terrain.h"
#include "../junction_build.h"
#include "../map_func.h"
#include "../track_func.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <set>
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

TEST_CASE("the first height is a lower bound", "[junctionmapedges]")
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

TEST_CASE("a supplied flat map gives the same T", "[junctionmap]")
{
	for (const auto &[name, spec] : MapSpecs()) {
		INFO("spec " << name);
		const JunctionTMapResult flat = ConstructJunctionTMap(spec, nullptr, std::nullopt, false);
		const JunctionTMapResult given = ConstructJunctionTMap(spec, nullptr, std::nullopt, true, &flat);
		const JunctionTMapResult own = ConstructJunctionTMap(spec, nullptr, std::nullopt, true);
		REQUIRE(given.layout.has_value());
		REQUIRE(own.layout.has_value());
		CHECK(given.layout->tracks.size() == own.layout->tracks.size());
		for (size_t i = 0; i < std::min(given.layout->tracks.size(), own.layout->tracks.size()); i++) {
			CHECK(given.layout->tracks[i].offset.x == own.layout->tracks[i].offset.x);
			CHECK(given.layout->tracks[i].offset.y == own.layout->tracks[i].offset.y);
			CHECK(given.layout->tracks[i].tracks == own.layout->tracks[i].tracks);
		}
		CHECK(given.height == own.height);
		CHECK(given.hub.x == own.hub.x);
		CHECK(given.hub.y == own.hub.y);
		CHECK(given.flat_runs == 0);
		CHECK(own.flat_runs == 1);
	}
}

TEST_CASE("the line-router T matches or beats the old-router T", "[junctionmapedges]")
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
		"FF 200 2\n"
		"FF 200 2\n"
		"FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF 4000 40\n"
		"LFFFLFFRFFFFLF 1800 58\n"
		"FB2FLFFFLFFFFFFFFRFL 3000 120\n"
		"FFFFFFFFFF 1000 10\n"
		"RF 250 3\n"
		"FFFFFFFFFFFFFFFFFFFF 2000 20\n"
		"LR 400 8\n"
		"F 100 1\n"
		"FL 250 1\n"
		"B5 0 0\n"
		"B5 0 0\n"
		"RFFFRFFLFFFFFR 1700 83\n"
		"RFFFFFFFFFFFRFFFFFFFFFFFFLFR 3100 817\n"},
		{"pinned",
		"- 0 42338\n"
		"LFFFFFFFFFFFFFLF 1800 123\n"
		"FB2FLFFFFL 1800 50\n"
		"- 0 15565\n"
		"FF 200 2\n"
		"FF 200 2\n"
		"FFFFFFFFFFFFFFFFFFFFFFFF 2400 24\n"
		"LFFFLFFRFFFFLF 1800 58\n"
		"FB2FFLFFFFL 1900 72\n"
		"FFFFF 500 5\n"
		"RF 250 3\n"
		"FFFFFFFFFF 1000 10\n"
		"LR 400 8\n"
		"F 100 1\n"
		"FL 250 1\n"
		"B4 0 0\n"
		"B4 0 0\n"
		"RFFFFFRFFFLFR 1600 55\n"
		"RFFFFFFFRFFLFFFFFR 2100 236\n"},
	};
	/* D173: the line router starts at the flat map's height, so its trace is the tail of the old one: the lines of the built height. */
	constexpr size_t BUILT_HEIGHT_LINES = 15; ///< The S and N port edges, A, A->S, N->A, the eight lane edges of A and B, B->S, N->B (E2 routes the port edges, E3 the lane edges, D178).
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

/** A flat, Buildable terrain at z 0 sized to the spec's frame with a margin of 4 (a copy of MakeNearTestTerrain in junction_game.cpp). */
static JunctionTerrain MakeMapTestTerrain(const JunctionSpec &spec)
{
	constexpr int MARGIN = 4;
	const JunctionFrame frame = MakeJunctionFrame(spec);
	JunctionTerrain t;
	t.origin = {static_cast<int16_t>(-MARGIN), static_cast<int16_t>(frame.terrain_origin_y - MARGIN)};
	t.width = frame.width + 2 * MARGIN;
	t.height = frame.terrain_height + 2 * MARGIN;
	t.tiles.assign(static_cast<size_t>(t.width) * t.height, JunctionTerrainTile{});
	t.foundation_cost = 250;
	return t;
}

static void BlockMapTestTile(JunctionTerrain &terrain, int x, int y)
{
	terrain.tiles[static_cast<size_t>(y - terrain.origin.y) * terrain.width + (x - terrain.origin.x)].cls = JunctionTileClass::Blocked;
}

/** The flat line-router T of the test spec, built once: its height is the first height of every projection test (D173) and its A->S tiles are the routes to block. */
static const JunctionTMapResult &SharedFlatT()
{
	static const JunctionTMapResult flat = ConstructJunctionTMap(MakeTestJunctionSpec(), nullptr, std::nullopt, true);
	return flat;
}

/** The tiles of the flat A->S route that are not on row 0, the W row or the b row. */
static std::vector<TileIndexDiffC> MovableAToSTiles(const JunctionSpec &spec, const JunctionTMapResult &flat)
{
	std::vector<TileIndexDiffC> tiles;
	for (const TileIndexDiffC &t : flat.a_to_s_tiles) {
		if (t.y != 0 && t.y != spec.ml_gap + 1 && t.y != spec.ml_gap + 2) tiles.push_back(t);
	}
	return tiles;
}

static bool MapUsesTile(const std::vector<TileIndexDiffC> &tiles, TileIndexDiffC tile)
{
	return std::ranges::any_of(tiles, [&](const TileIndexDiffC &t) { return t.x == tile.x && t.y == tile.y; });
}

TEST_CASE("an obstacle on A->S's route is projected around", "[junction][projection]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionTMapResult &flat = SharedFlatT();
	REQUIRE(flat.layout.has_value());
	const std::vector<TileIndexDiffC> movable = MovableAToSTiles(spec, flat);
	REQUIRE(!movable.empty());
	const TileIndexDiffC blocked = movable[movable.size() / 2];
	JunctionTerrain terrain = MakeMapTestTerrain(spec);
	BlockMapTestTile(terrain, blocked.x, blocked.y);
	const JunctionTMapResult result = ConstructJunctionTMap(spec, &terrain, SharedFlatT().height, true);
	INFO("blocked " << blocked.x << "," << blocked.y << "\n" << result.log);
	REQUIRE(result.layout.has_value());
	CHECK(result.clean);
	CHECK(!MapUsesTile(result.a_to_s_tiles, blocked));
	bool differs = result.layout->tracks.size() != flat.layout->tracks.size() || result.layout->bridges.size() != flat.layout->bridges.size();
	for (size_t i = 0; !differs && i < flat.layout->tracks.size(); i++) {
		differs = result.layout->tracks[i].offset.x != flat.layout->tracks[i].offset.x || result.layout->tracks[i].offset.y != flat.layout->tracks[i].offset.y || result.layout->tracks[i].tracks != flat.layout->tracks[i].tracks;
	}
	for (size_t i = 0; !differs && i < flat.layout->bridges.size(); i++) {
		const JunctionBridge &a = result.layout->bridges[i];
		const JunctionBridge &b = flat.layout->bridges[i];
		differs = a.start.x != b.start.x || a.start.y != b.start.y || a.end.x != b.end.x || a.end.y != b.end.y;
	}
	CHECK(differs);
	std::cout << "BENCH tmap obstacle height=" << result.height << " heights=" << result.heights_tried << " flat_height=" << flat.height << " micros=" << result.micros << " flat=" << result.micros_flat << " route=" << result.micros_route << "\n";
}

/** Whether two layouts hold the same tracks, bridges and signals, tile for tile. */
static void CheckSameLayout(const JunctionLayout &a, const JunctionLayout &b)
{
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

/** The frame x of A's west port in the flat T (E1, D179): the ports come from the flat T, whose crop leaves the main line content +-2. */
static int FlatWestPortX(const JunctionTMapResult &flat)
{
	return MakeJunctionFrame(MakeTestJunctionSpec()).hub_x - flat.hub.x;
}

TEST_CASE("an obstacle on the main line outside the junction is ignored", "[junction][projectionedges]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionTMapResult &flat = SharedFlatT();
	REQUIRE(flat.layout.has_value());
	REQUIRE(3 < FlatWestPortX(flat));
	JunctionTerrain terrain = MakeMapTestTerrain(spec);
	BlockMapTestTile(terrain, 3, 0);
	const JunctionTMapResult result = ConstructJunctionTMap(spec, &terrain, flat.height, true);
	INFO(result.log);
	REQUIRE(result.layout.has_value());
	CHECK(result.clean);
	CheckSameLayout(*result.layout, *flat.layout);
}

/** The flat T's tile in the frame's working coordinates (E3): the layout is cropped and shifted by the hub offset. */
static TileIndexDiffC FlatFrameTile(const JunctionTMapResult &flat, const JunctionTrack &track)
{
	const int x_w = FlatWestPortX(flat);
	return {static_cast<int16_t>(track.offset.x + x_w), static_cast<int16_t>(track.offset.y - flat.hub.y)};
}

/** Cuts the terrain after `rows` frame rows (from row 0), so a failing map tries only a few heights. */
static void LimitMapTestRows(JunctionTerrain &terrain, int rows)
{
	terrain.height = rows - terrain.origin.y;
	terrain.tiles.resize(static_cast<size_t>(terrain.width) * terrain.height);
}

TEST_CASE("a house on B's main line outside the junction is ignored", "[junction][projectionedges]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionTMapResult &flat = SharedFlatT();
	REQUIRE(flat.layout.has_value());
	const int x_east = FlatWestPortX(flat) + flat.layout->size.x; // One column east of the flat T's last column.
	JunctionTerrain terrain = MakeMapTestTerrain(spec);
	BlockMapTestTile(terrain, x_east, spec.ml_gap + 1);
	const JunctionTMapResult result = ConstructJunctionTMap(spec, &terrain, flat.height, true);
	INFO(result.log);
	REQUIRE(result.layout.has_value());
	CHECK(result.clean);
	CheckSameLayout(*result.layout, *flat.layout);
}

TEST_CASE("a house on B's lane inside the junction is reported by B", "[junction][projectionedges]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionTMapResult &flat = SharedFlatT();
	REQUIRE(flat.layout.has_value());
	const int b_row = spec.ml_gap + 2;
	/* The b row's bridge heads, then the plain X tiles west of the far head that no branch route uses. */
	int far_head_x = INT16_MAX;
	for (const JunctionBridge &bridge : flat.layout->bridges) {
		if (bridge.start.y - flat.hub.y == b_row) far_head_x = std::min<int>(far_head_x, std::min(bridge.start.x, bridge.end.x) + FlatWestPortX(flat));
	}
	REQUIRE(far_head_x != INT16_MAX);
	std::vector<TileIndexDiffC> candidates;
	for (const JunctionTrack &track : flat.layout->tracks) {
		const TileIndexDiffC frame_tile = FlatFrameTile(flat, track);
		if (frame_tile.y != b_row || frame_tile.x >= far_head_x - 2 || track.tracks != TrackBits{Track::X}) continue;
		if (MapUsesTile(flat.a_to_s_tiles, frame_tile) || MapUsesTile(flat.n_to_a_tiles, frame_tile)) continue;
		candidates.push_back(frame_tile);
	}
	REQUIRE(!candidates.empty());
	std::ranges::sort(candidates, [](const TileIndexDiffC &a, const TileIndexDiffC &b) { return a.x < b.x; });
	const TileIndexDiffC blocked = candidates.back(); // The nearest to the far head: a stretch too short to detour around.
	JunctionTerrain terrain = MakeMapTestTerrain(spec);
	BlockMapTestTile(terrain, blocked.x, blocked.y);
	LimitMapTestRows(terrain, flat.height + 3);
	const JunctionTMapResult result = ConstructJunctionTMap(spec, &terrain, flat.height, true);
	INFO("blocked " << blocked.x << "," << blocked.y << "\n" << result.log);
	if (result.layout.has_value()) {
		CHECK(result.clean);
		for (const JunctionTrack &track : result.layout->tracks) {
			if (FlatFrameTile(flat, track).x == blocked.x && FlatFrameTile(flat, track).y == blocked.y) CHECK(track.tracks.None());
		}
	}
	std::istringstream lines(result.log);
	int failed_heights = 0;
	for (std::string line; std::getline(lines, line);) {
		if (line.ends_with(": built")) continue;
		CHECK(line.find("B lane edge failed at") != std::string::npos);
		failed_heights++;
	}
	CHECK(failed_heights > 0);
}

TEST_CASE("a house under B's doubled bridge is reported", "[junction][projectionedges]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionTMapResult &flat = SharedFlatT();
	REQUIRE(flat.layout.has_value());
	const int b_row = spec.ml_gap + 2;
	/* Lane B's bridge: the flat T's bridge on the b row; the second tile after its west head is a middle tile. */
	int west_head_x = INT16_MAX;
	for (const JunctionBridge &bridge : flat.layout->bridges) {
		if (bridge.start.y - flat.hub.y == b_row) west_head_x = std::min<int>(west_head_x, std::min(bridge.start.x, bridge.end.x) + FlatWestPortX(flat));
	}
	REQUIRE(west_head_x != INT16_MAX);
	const TileIndexDiffC blocked{static_cast<int16_t>(west_head_x + 2), static_cast<int16_t>(b_row)};
	JunctionTerrain terrain = MakeMapTestTerrain(spec);
	BlockMapTestTile(terrain, blocked.x, blocked.y);
	LimitMapTestRows(terrain, flat.height + 3);
	const JunctionTMapResult result = ConstructJunctionTMap(spec, &terrain, flat.height, true);
	INFO("blocked " << blocked.x << "," << blocked.y << "\n" << result.log);
	CHECK(result.log.find("B lane edge failed at") != std::string::npos);
	if (result.layout.has_value()) {
		CHECK(result.clean);
		const int x_w = MakeJunctionFrame(spec).hub_x - result.hub.x;
		for (const JunctionBridge &bridge : result.layout->bridges) {
			const int y = bridge.start.y - result.hub.y;
			if (bridge.start.y != bridge.end.y || y != blocked.y) continue;
			const int lo = std::min(bridge.start.x, bridge.end.x) + x_w;
			const int hi = std::max(bridge.start.x, bridge.end.x) + x_w;
			CHECK_FALSE((lo <= blocked.x && blocked.x <= hi));
		}
	}
}

TEST_CASE("a slope under B's straight stretch is ramped or reported", "[junction][projectionedges]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionTMapResult &flat = SharedFlatT();
	REQUIRE(flat.layout.has_value());
	const int x_east = FlatWestPortX(flat) + flat.layout->size.x - 1;
	JunctionTerrain terrain = MakeMapTestTerrain(spec);
	terrain.build_on_slopes = true;
	for (const int x : {x_east - 4, x_east - 3}) {
		JunctionTerrainTile &tile = terrain.tiles[static_cast<size_t>(spec.ml_gap + 1 - terrain.origin.y) * terrain.width + (x - terrain.origin.x)];
		tile.z_min = 0;
		tile.z_max = 1;
		tile.incline = DiagDirection::NE;
	}
	LimitMapTestRows(terrain, flat.height + 3);
	const JunctionTMapResult result = ConstructJunctionTMap(spec, &terrain, flat.height, true);
	INFO(result.log);
	if (result.layout.has_value()) {
		CHECK(result.clean);
	} else {
		CHECK(result.log.find("B lane edge failed at") != std::string::npos);
	}
}

TEST_CASE("a blocked port approach is reported at once", "[junction][projectionedges]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionTMapResult &flat = SharedFlatT();
	REQUIRE(flat.layout.has_value());
	const int x_w = FlatWestPortX(flat);
	JunctionTerrain terrain = MakeMapTestTerrain(spec);
	BlockMapTestTile(terrain, x_w + 1, 0);
	const JunctionTMapResult result = ConstructJunctionTMap(spec, &terrain, flat.height, true);
	INFO(result.log);
	CHECK(!result.layout.has_value());
	CHECK(result.port_blocked);
	std::ostringstream expected;
	expected << "port approach blocked at " << x_w + 1 << ",0";
	CHECK(result.log.find(expected.str()) != std::string::npos);
	CHECK(result.heights_tried == 0);
	CHECK(result.micros_route == 0);
}

/* [slow] until the growth stop: today it tries every height to the snapshot's end (33 heights, about 8 s Release); the growth stop (grow only when the box, not the terrain, stopped the search, DESIGN-O4) must make it fail at the first height and drop [slow]. */
TEST_CASE("an obstacle on the main line beside the hub gives no layout", "[junction][projectionedges][slow]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionTMapResult &flat = SharedFlatT();
	REQUIRE(flat.layout.has_value());
	constexpr int STUB = 2;
	const int x_w = FlatWestPortX(flat);
	const int x_e = x_w + flat.layout->size.x - 1;
	const int xI = MakeJunctionFrame(spec).hub_x;
	const TileIndexDiffC blocked{static_cast<int16_t>(xI - 1), 0};
	REQUIRE(blocked.x > x_w + STUB);
	REQUIRE(blocked.x < x_e - STUB);
	REQUIRE(!MapUsesTile(flat.a_to_s_tiles, blocked));
	REQUIRE(!MapUsesTile(flat.n_to_a_tiles, blocked));
	JunctionTerrain terrain = MakeMapTestTerrain(spec);
	BlockMapTestTile(terrain, blocked.x, blocked.y);
	const JunctionTMapResult result = ConstructJunctionTMap(spec, &terrain, flat.height, true);
	INFO(result.log);
	CHECK(!result.layout.has_value());
	CHECK(result.heights_tried > 0);
	for (int h = flat.height; h < flat.height + result.heights_tried; h++) {
		std::ostringstream a_line;
		a_line << "height " << h << ": A failed\n";
		std::ostringstream n_line;
		n_line << "height " << h << ": N->A failed\n";
		CHECK((result.log.find(a_line.str()) != std::string::npos || result.log.find(n_line.str()) != std::string::npos));
	}
	/* No other failure kind: every log line is one of the two. */
	std::istringstream lines(result.log);
	for (std::string line; std::getline(lines, line);) {
		CHECK((line.ends_with(": A failed") || line.ends_with(": N->A failed")));
	}
}

TEST_CASE("a field of obstacles grows the box", "[junction][projection]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionTMapResult &flat = SharedFlatT();
	REQUIRE(flat.layout.has_value());
	JunctionTerrain terrain = MakeMapTestTerrain(spec);
	/* The S stub (x == xO, y >= ys) is fixed track: leave it, so the fixed-part check does not fire and the growth path runs. */
	const JunctionFrame frame = MakeJunctionFrame(spec);
	const int xO = frame.hub_x + spec.branch_gap + 1;
	const int ys = flat.height - 3;
	/* Only the middle third (by index) of the movable tiles: the whole route made the test too slow, and a third is enough to force a detour. */
	const std::vector<TileIndexDiffC> movable = MovableAToSTiles(spec, flat);
	for (size_t i = movable.size() / 3; i < movable.size() - movable.size() / 3; i++) {
		const TileIndexDiffC &t = movable[i];
		if (t.x == xO && t.y >= ys) continue;
		BlockMapTestTile(terrain, t.x, t.y);
	}
	const JunctionTMapResult result = ConstructJunctionTMap(spec, &terrain, SharedFlatT().height, true);
	INFO(result.log);
	/* D170a: the box grows until the map fits; the crossing window (D172) makes the skeleton's input fit by construction. */
	CHECK(result.log.find("no valid skeleton") == std::string::npos);
	if (result.layout.has_value()) {
		CHECK(result.height > flat.height);
		CHECK(result.clean);
	} else {
		/* Every height up to the terrain's limit was tried: the rows of the snapshot are the box. */
		const int terrain_rows = terrain.origin.y + terrain.height;
		std::ostringstream last_height;
		last_height << "height " << terrain_rows << ": ";
		CHECK(result.log.find(last_height.str()) != std::string::npos);
	}
	std::cout << "BENCH tmap field height=" << result.height << " heights=" << result.heights_tried << " micros=" << result.micros << " flat=" << result.micros_flat << " route=" << result.micros_route << "\n";
}

/** The flat T's frame x of the S port edge (D178): the O column, xO = hub_x + branch_gap + 1. */
static int SPortEdgeX(const JunctionSpec &spec)
{
	return MakeJunctionFrame(spec).hub_x + spec.branch_gap + 1;
}

TEST_CASE("a house on the S port edge grows the box", "[junction][projectionedges]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionTMapResult &flat = SharedFlatT();
	REQUIRE(flat.layout.has_value());
	const int xO = SPortEdgeX(spec);
	const int y = flat.height - 3;
	JunctionTerrain terrain = MakeMapTestTerrain(spec);
	BlockMapTestTile(terrain, xO, y);
	const JunctionTMapResult result = ConstructJunctionTMap(spec, &terrain, flat.height, true);
	INFO(result.log);
	REQUIRE(result.layout.has_value());
	CHECK(result.clean);
	CHECK(result.height > flat.height);
	for (const JunctionTrack &track : result.layout->tracks) {
		if (track.offset.x == xO && track.offset.y == y) CHECK(track.tracks.None());
	}
	std::ostringstream failed;
	failed << "height " << flat.height << ": S port edge failed";
	std::istringstream lines(result.log);
	bool found = false;
	for (std::string line; std::getline(lines, line);) {
		if (line.ends_with(failed.str())) found = true;
	}
	CHECK(found);
}

TEST_CASE("a slope under the S port edge is ramped", "[junction][projectionedges]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionTMapResult &flat = SharedFlatT();
	REQUIRE(flat.layout.has_value());
	const int xO = SPortEdgeX(spec);
	const int ys = flat.height - 3;
	JunctionTerrain terrain = MakeMapTestTerrain(spec);
	terrain.build_on_slopes = true;
	for (const int y : {ys, ys + 1}) {
		JunctionTerrainTile &tile = terrain.tiles[static_cast<size_t>(y - terrain.origin.y) * terrain.width + (xO - terrain.origin.x)];
		tile.z_min = 0;
		tile.z_max = 1;
		tile.incline = DiagDirection::NW;
	}
	const JunctionTMapResult result = ConstructJunctionTMap(spec, &terrain, flat.height, true);
	INFO(result.log);
	REQUIRE(result.layout.has_value());
	CHECK(result.clean);
	/* D178: the stub is a routed port edge, not fixed track that is checked on its own. */
	CHECK(result.log.find("stub on unusable tile") == std::string::npos);
}

TEST_CASE("the crossing window keeps both crossings under one bridge", "[junction][projection]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionTMapResult &flat = SharedFlatT();
	REQUIRE(flat.layout.has_value());
	const std::vector<TileIndexDiffC> movable = MovableAToSTiles(spec, flat);
	REQUIRE(!movable.empty());
	const TileIndexDiffC blocked = movable[movable.size() / 2];
	JunctionTerrain terrain = MakeMapTestTerrain(spec);
	BlockMapTestTile(terrain, blocked.x, blocked.y);
	const JunctionTMapResult result = ConstructJunctionTMap(spec, &terrain, flat.height, true);
	INFO(result.log);
	REQUIRE(result.layout.has_value());
	int nmax = 0;
	for (int n = 10; n >= 1 && nmax == 0; n--) {
		if (spec.bridge_span_ok(n)) nmax = n;
	}
	const int window = nmax - 1 - 2;
	int lo = std::numeric_limits<int>::max();
	int hi = std::numeric_limits<int>::min();
	for (const auto *tiles : {&result.a_to_s_tiles, &result.n_to_a_tiles}) {
		for (const TileIndexDiffC &t : *tiles) {
			if (t.y != spec.ml_gap + 1 && t.y != spec.ml_gap + 2) continue;
			lo = std::min<int>(lo, t.x);
			hi = std::max<int>(hi, t.x);
		}
	}
	REQUIRE(lo <= hi);
	CHECK(hi - lo <= window);
}

/** Set every tile of row y of the terrain: heights and incline (like SetRampColumn in the line router tests). */
static void SetRampRow(JunctionTerrain &terrain, int y, int z_min, int z_max, DiagDirection incline)
{
	for (int x = terrain.origin.x; x < terrain.origin.x + terrain.width; x++) {
		JunctionTerrainTile &tile = terrain.tiles[static_cast<size_t>(y - terrain.origin.y) * terrain.width + (x - terrain.origin.x)];
		tile.z_min = static_cast<int8_t>(z_min);
		tile.z_max = static_cast<int8_t>(z_max);
		tile.incline = incline;
	}
}

/** The terrain of the ridge tests: flat at 0, with a ridge across the branch area that rises over row R = b + 6, is level at 1 over rows R + 1 and R + 2, and falls over row R + 3 (SE rises towards +y, NW towards -y). */
static JunctionTerrain MakeRidgeTerrain(const JunctionSpec &spec)
{
	const int ridge = spec.ml_gap + 2 + 6;
	JunctionTerrain terrain = MakeMapTestTerrain(spec);
	terrain.build_on_slopes = true;
	SetRampRow(terrain, ridge, 0, 1, DiagDirection::SE);
	SetRampRow(terrain, ridge + 1, 1, 1, DiagDirection::Invalid);
	SetRampRow(terrain, ridge + 2, 1, 1, DiagDirection::Invalid);
	SetRampRow(terrain, ridge + 3, 0, 1, DiagDirection::NW);
	return terrain;
}

/** The terrain cost of a layout by the SC1 rule, written out plainly: each tile with track or a bridge head once, its clearing, and a foundation when it is sloped and not a ramp. */
static Money ExpectedTerrainCost(const JunctionLayout &layout, const JunctionTerrain &terrain, int ox, int oy)
{
	std::map<std::pair<int, int>, TrackBits> tiles;
	for (const JunctionTrack &jt : layout.tracks) {
		if (jt.tracks.None()) continue;
		TrackBits &bits = tiles[{jt.offset.x + ox, jt.offset.y + oy}];
		for (const Track track : jt.tracks) bits.Set(track);
	}
	for (const JunctionBridge &bridge : layout.bridges) {
		tiles[{bridge.start.x + ox, bridge.start.y + oy}];
		tiles[{bridge.end.x + ox, bridge.end.y + oy}];
	}
	Money expected = 0;
	for (const auto &[pos, bits] : tiles) {
		const JunctionTerrainTile *tile = terrain.At(pos.first, pos.second);
		REQUIRE(tile != nullptr);
		expected += tile->clear_cost;
		if (tile->z_min == tile->z_max) continue;
		bool ramp = false;
		if (tile->incline == DiagDirection::NE || tile->incline == DiagDirection::SW) ramp = bits == TrackBits{Track::X};
		if (tile->incline == DiagDirection::SE || tile->incline == DiagDirection::NW) ramp = bits == TrackBits{Track::Y};
		if (!ramp) expected += terrain.foundation_cost;
	}
	return expected;
}

TEST_CASE("the T map reports its terrain cost", "[junction][projectionedges]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionTMapResult &flat = SharedFlatT();
	REQUIRE(flat.layout.has_value());
	JunctionTerrain terrain = MakeMapTestTerrain(spec);
	for (JunctionTerrainTile &tile : terrain.tiles) tile.clear_cost = 30;
	const JunctionTMapResult result = ConstructJunctionTMap(spec, &terrain, flat.height, true);
	REQUIRE(result.layout.has_value());
	std::set<std::pair<int, int>> tiles;
	for (const JunctionTrack &jt : result.layout->tracks) {
		if (!jt.tracks.None()) tiles.insert({jt.offset.x, jt.offset.y});
	}
	for (const JunctionBridge &bridge : result.layout->bridges) {
		tiles.insert({bridge.start.x, bridge.start.y});
		tiles.insert({bridge.end.x, bridge.end.y});
	}
	CHECK(result.terrain_cost == Money(30) * static_cast<int64_t>(tiles.size()));
}

TEST_CASE("a foundation is charged, a ramp is not", "[junction][projectionedges]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionTMapResult &flat = SharedFlatT();
	REQUIRE(flat.layout.has_value());
	JunctionTerrain terrain = MakeRidgeTerrain(spec);
	terrain.foundation_cost = 250;
	for (JunctionTerrainTile &tile : terrain.tiles) tile.clear_cost = 30;
	const JunctionTMapResult result = ConstructJunctionTMap(spec, &terrain, flat.height, true);
	REQUIRE(result.layout.has_value());
	const JunctionFrame frame = MakeJunctionFrame(spec);
	const Money expected = ExpectedTerrainCost(*result.layout, terrain, frame.hub_x - result.hub.x, -result.hub.y);
	CHECK(result.terrain_cost == expected);
}

TEST_CASE("a ridge across the branches is climbed", "[junction][projection]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionTMapResult &flat = SharedFlatT();
	REQUIRE(flat.layout.has_value());
	const JunctionTerrain terrain = MakeRidgeTerrain(spec);
	const JunctionTMapResult result = ConstructJunctionTMap(spec, &terrain, flat.height, true);
	INFO(result.log);
	std::cout << "RIDGE flat_height=" << flat.height << " built=" << (result.layout.has_value() ? 1 : 0) << " clean=" << (result.clean ? 1 : 0) << " height=" << result.height << " heights=" << result.heights_tried << "\n" << result.log;
	REQUIRE(result.layout.has_value());
	CHECK(result.clean);
}

TEST_CASE("the ridge layout builds through the fake poster", "[junction][projection]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionTMapResult &flat = SharedFlatT();
	REQUIRE(flat.layout.has_value());
	const JunctionTerrain terrain = MakeRidgeTerrain(spec);
	const JunctionTMapResult result = ConstructJunctionTMap(spec, &terrain, flat.height, true);
	REQUIRE(result.layout.has_value());
	Map::Allocate(256, 256);
	std::vector<JunctionBuildStep> steps = MakeJunctionBuildSteps(*result.layout, TileXY(8, 8));
	const size_t expected = steps.size();
	REQUIRE(expected > 0);
	JunctionSequencer seq;
	/** Poster that counts every posted step. */
	class CountingPoster : public JunctionStepPoster {
	public:
		JunctionSequencer &seq;
		size_t posted = 0;
		explicit CountingPoster(JunctionSequencer &seq) : seq(seq) {}
		bool Post([[maybe_unused]] const JunctionBuildStep &step) override
		{
			this->posted++;
			this->seq.OnStepResult(true);
			return true;
		}
	} poster(seq);
	seq.Start(std::move(steps));
	for (size_t i = 0; i < expected + 5 && seq.GetState() == JunctionBuildState::Building; i++) seq.Tick(poster);
	CHECK(seq.GetState() == JunctionBuildState::Done);
	CHECK(poster.posted == expected);
}

/** The G3c ridge with houses on it: the two flat top rows Blocked across the terrain's width. */
static JunctionTerrain MakeTownHillTerrain(const JunctionSpec &spec)
{
	const int ridge = spec.ml_gap + 2 + 6;
	JunctionTerrain terrain = MakeRidgeTerrain(spec);
	for (const int y : {ridge + 1, ridge + 2}) {
		for (int x = terrain.origin.x; x < terrain.origin.x + terrain.width; x++) BlockMapTestTile(terrain, x, y);
	}
	return terrain;
}

TEST_CASE("a hill covered by a town is not crossed without a tunnel price", "[junction][projection]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionTerrain terrain = MakeTownHillTerrain(spec);
	/* Neither ramps nor bridges pass the town: the S port edge could route in a taller box, A->S cannot, so the third height stops the search (GS1b). */
	const JunctionTMapResult untunnelled = ConstructJunctionTMap(spec, &terrain, SharedFlatT().height, true);
	INFO(untunnelled.log);
	CHECK_FALSE(untunnelled.layout.has_value());
	CHECK(untunnelled.heights_tried <= 3);
	CHECK(untunnelled.log.ends_with("growth cannot help\n"));
}

TEST_CASE("a hill covered by a town is tunnelled", "[junction][projection]")
{
	JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionTMapResult &flat = SharedFlatT();
	REQUIRE(flat.layout.has_value());
	const int ridge = spec.ml_gap + 2 + 6;
	const JunctionTerrain terrain = MakeTownHillTerrain(spec);

	spec.prices.tunnel = [](uint n) { return Money(600 + 300 * n); };
	const JunctionTMapResult result = ConstructJunctionTMap(spec, &terrain, flat.height, true);
	INFO(result.log);
	REQUIRE(result.layout.has_value());
	CHECK(result.clean);
	/* The layout is normalised: its rows are the frame's plus dy, which the doubled main-line bridges (on frame rows W = ml_gap + 1 and b = ml_gap + 2, the layout's horizontal bridges) give. */
	int dy = std::numeric_limits<int>::max();
	for (const JunctionBridge &bridge : result.layout->bridges) {
		if (bridge.start.y == bridge.end.y) dy = std::min<int>(dy, bridge.start.y);
	}
	REQUIRE(dy != std::numeric_limits<int>::max());
	dy -= spec.ml_gap + 1;
	size_t tunnels = 0;
	for (const JunctionBridge &bridge : result.layout->bridges) {
		const int lo = std::min(bridge.start.y, bridge.end.y) - dy;
		const int hi = std::max(bridge.start.y, bridge.end.y) - dy;
		if (bridge.tunnel) {
			tunnels++;
			CHECK(lo == ridge);
			CHECK(hi == ridge + 3);
		} else {
			/* A bridge's middle tiles are those strictly between its heads (all on one row when it runs along x). */
			const int first = lo == hi ? lo : lo + 1;
			const int last = lo == hi ? lo : hi - 1;
			const bool over_town = first <= ridge + 2 && last >= ridge + 1;
			CHECK_FALSE(over_town);
		}
	}
	CHECK(tunnels >= 1);
}

TEST_CASE("BENCH the ridge map", "[projection]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionTMapResult &flat = SharedFlatT();
	REQUIRE(flat.layout.has_value());
	const JunctionTerrain terrain = MakeRidgeTerrain(spec);
	const JunctionTMapResult result = ConstructJunctionTMap(spec, &terrain, flat.height, true);
	REQUIRE(result.layout.has_value());
	std::cout << "BENCH tmap ridge height=" << result.height << " heights=" << result.heights_tried << " micros=" << result.micros << " flat=" << result.micros_flat << " route=" << result.micros_route << "\n";
}

TEST_CASE("BENCH the tunnelled hill map", "[projection]")
{
	JunctionSpec spec = MakeTestJunctionSpec();
	spec.prices.tunnel = [](uint n) { return Money(600 + 300 * n); };
	const JunctionTMapResult &flat = SharedFlatT();
	REQUIRE(flat.layout.has_value());
	const int ridge = spec.ml_gap + 2 + 6;
	JunctionTerrain terrain = MakeRidgeTerrain(spec);
	for (const int y : {ridge + 1, ridge + 2}) {
		for (int x = terrain.origin.x; x < terrain.origin.x + terrain.width; x++) BlockMapTestTile(terrain, x, y);
	}
	const JunctionTMapResult result = ConstructJunctionTMap(spec, &terrain, flat.height, true);
	REQUIRE(result.layout.has_value());
	std::cout << "BENCH tmap tunnel height=" << result.height << " heights=" << result.heights_tried << " micros=" << result.micros << " flat=" << result.micros_flat << " route=" << result.micros_route << "\n";
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

TEST_CASE("the T map builds at the hub's ground height", "[junction][projection]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	JunctionTerrain terrain = MakeMapTestTerrain(spec);
	terrain.build_on_slopes = true;
	for (JunctionTerrainTile &tile : terrain.tiles) {
		tile.z_min = 2;
		tile.z_max = 2;
	}
	const JunctionTMapResult result = ConstructJunctionTMap(spec, &terrain, std::nullopt, true);
	const JunctionTMapResult &flat = SharedFlatT();
	INFO(result.log);
	REQUIRE(result.layout.has_value());
	REQUIRE(flat.layout.has_value());
	CHECK(result.clean);
	const JunctionLayout &a = *result.layout;
	const JunctionLayout &b = *flat.layout;
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

TEST_CASE("an unbuildable hub tile gives no layout", "[junction][projectionedges]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	JunctionTerrain terrain = MakeMapTestTerrain(spec);
	BlockMapTestTile(terrain, MakeJunctionFrame(spec).hub_x, 0);
	/* PH1: the hub tile is a tile of A like any other, so A's routing fails on it (no "hub tile not buildable" return any more); the rows are limited to keep the failing search short. */
	LimitMapTestRows(terrain, SharedFlatT().height + 3);
	const JunctionTMapResult result = ConstructJunctionTMap(spec, &terrain, SharedFlatT().height, true);
	INFO(result.log);
	CHECK(!result.layout.has_value());
}

TEST_CASE("the T map reports the hub offset", "[junction][projection]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionGenerateResult generated = GenerateJunction(spec, nullptr);
	const JunctionTMapResult &flat = SharedFlatT();
	CHECK(flat.hub.x == generated.hub.x);
	CHECK(flat.hub.y == generated.hub.y);
}

// Known limit (PH1c): the lanes' and merges' positions come from the flat T's skeleton, so a ramp inside the junction's footprint has nowhere to go; needs node windows instead of fixed positions. [slow]: tries every height (no growth stop for rule-caused failures).
TEST_CASE("a height change inside the junction's W end is not built yet", "[junction][projectionedges][slow]")
{
	constexpr int STUB = 2;
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionTMapResult &flat = SharedFlatT();
	REQUIRE(flat.layout.has_value());
	const int x_w = FlatWestPortX(flat);
	JunctionTerrain terrain = MakeMapTestTerrain(spec);
	terrain.build_on_slopes = true;
	for (int y = terrain.origin.y; y < terrain.origin.y + terrain.height; y++) {
		for (int x = terrain.origin.x; x <= x_w + STUB + 3; x++) {
			JunctionTerrainTile &tile = terrain.tiles[static_cast<size_t>(y - terrain.origin.y) * terrain.width + (x - terrain.origin.x)];
			if (x <= x_w + STUB + 2) {
				tile.z_min = tile.z_max = 1;
			} else {
				tile.z_min = 0;
				tile.z_max = 1;
				tile.incline = DiagDirection::NE;
			}
		}
	}
	const JunctionTMapResult result = ConstructJunctionTMap(spec, &terrain, flat.height, true);
	INFO(result.log);
	CHECK(!result.layout.has_value());
	CHECK(result.log.find("N->B failed") != std::string::npos);
	CHECK(result.log.find("port approach") == std::string::npos);
}

TEST_CASE("the hub on a slope", "[junction][projectionedges]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	JunctionTerrain terrain = MakeMapTestTerrain(spec);
	terrain.build_on_slopes = true;
	for (JunctionTerrainTile &tile : terrain.tiles) tile.z_min = tile.z_max = 1;
	const int xI = MakeJunctionFrame(spec).hub_x;
	terrain.tiles[static_cast<size_t>(0 - terrain.origin.y) * terrain.width + (xI - terrain.origin.x)].z_min = 0;
	const JunctionTMapResult result = ConstructJunctionTMap(spec, &terrain, std::nullopt, true);
	INFO(result.log);
	REQUIRE(result.layout.has_value());
	CHECK(result.clean);
}
