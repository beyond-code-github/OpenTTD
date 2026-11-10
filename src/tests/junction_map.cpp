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

TEST_CASE("an obstacle on the main line is reported", "[junction][projection]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	JunctionTerrain terrain = MakeMapTestTerrain(spec);
	BlockMapTestTile(terrain, 3, 0);
	const JunctionTMapResult result = ConstructJunctionTMap(spec, &terrain, SharedFlatT().height, true);
	INFO(result.log);
	CHECK(!result.layout.has_value());
	CHECK(result.log.find("fixed track on unusable tile 3,0") != std::string::npos);
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
	std::cout << "FIELD flat_height=" << flat.height << " built=" << (result.layout.has_value() ? 1 : 0) << " clean=" << (result.clean ? 1 : 0) << " height=" << result.height << " heights=" << result.heights_tried << "\n" << result.log;
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

TEST_CASE("a hill covered by a town is tunnelled", "[junction][projection]")
{
	JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionTMapResult &flat = SharedFlatT();
	REQUIRE(flat.layout.has_value());
	const int ridge = spec.ml_gap + 2 + 6;
	JunctionTerrain terrain = MakeRidgeTerrain(spec);
	/* Houses on the hill: the two flat top rows are Blocked across the terrain's width. */
	for (const int y : {ridge + 1, ridge + 2}) {
		for (int x = terrain.origin.x; x < terrain.origin.x + terrain.width; x++) BlockMapTestTile(terrain, x, y);
	}

	/* Without a tunnel price the hill cannot be crossed: neither ramps nor bridges pass the town. */
	const JunctionTMapResult untunnelled = ConstructJunctionTMap(spec, &terrain, flat.height, true);
	std::cout << "TUNNEL unset built=" << (untunnelled.layout.has_value() ? 1 : 0) << "\n" << untunnelled.log;
	CHECK_FALSE(untunnelled.layout.has_value());

	spec.prices.tunnel = [](uint n) { return Money(600 + 300 * n); };
	const JunctionTMapResult result = ConstructJunctionTMap(spec, &terrain, flat.height, true);
	INFO(result.log);
	std::cout << "TUNNEL height=" << result.height << " heights=" << result.heights_tried << " built=" << (result.layout.has_value() ? 1 : 0) << " clean=" << (result.clean ? 1 : 0) << "\n" << result.log;
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
		std::cout << "TUNNEL piece " << (bridge.tunnel ? "tunnel" : "bridge") << " " << bridge.start.x << "," << bridge.start.y << " -> " << bridge.end.x << "," << bridge.end.y << " z=" << static_cast<int>(bridge.z) << "\n";
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
