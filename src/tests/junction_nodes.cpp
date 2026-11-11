/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_nodes.cpp Test functionality from junction_nodes. */

#include "../stdafx.h"
#include "../3rdparty/fmt/format.h"

#include "../3rdparty/catch2/catch.hpp"

#include "../junction_chain.h"
#include "../junction_generator.h"
#include "../junction_nodes.h"
#include "../junction_positions.h"
#include "../junction_route.h"
#include "../junction_facts.h"
#include "../junction_rules.h"
#include "../junction_rules_internal.h"
#include "../junction_signal_pass.h"

#include <algorithm>
#include <tuple>
#include <vector>

#include "../safeguards.h"

/** The flat line-router T of the test spec, built once. */
static const JunctionTMapResult &FlatT()
{
	static const JunctionTMapResult flat = ConstructJunctionTMap(MakeTestJunctionSpec(), nullptr, std::nullopt, true);
	return flat;
}

using TrackKey = std::tuple<int, int, uint>;
using SignalKey = std::tuple<int, int, uint, uint, uint>;
using BridgeKey = std::tuple<int, int, int, int, bool>;

static std::vector<TrackKey> TrackKeys(const JunctionLayout &layout)
{
	std::vector<TrackKey> keys;
	for (const JunctionTrack &jt : layout.tracks) keys.emplace_back(jt.offset.x, jt.offset.y, static_cast<uint>(jt.tracks.base()));
	std::ranges::sort(keys);
	return keys;
}

static std::vector<SignalKey> SignalKeys(const JunctionLayout &layout)
{
	std::vector<SignalKey> keys;
	for (const JunctionSignal &s : layout.signals) keys.emplace_back(s.offset.x, s.offset.y, static_cast<uint>(to_underlying(s.facing)), static_cast<uint>(to_underlying(s.type)), static_cast<uint>(to_underlying(s.variant)));
	std::ranges::sort(keys);
	return keys;
}

static std::vector<BridgeKey> BridgeKeys(const JunctionLayout &layout)
{
	std::vector<BridgeKey> keys;
	for (const JunctionBridge &b : layout.bridges) keys.emplace_back(b.start.x, b.start.y, b.end.x, b.end.y, b.tunnel);
	std::ranges::sort(keys);
	return keys;
}

/** Render the template at the values, place the signals as the generator does, and normalise. */
static JunctionLayout RenderWithSignals(const JunctionNodeTemplate &tpl, const std::vector<int> &values, const JunctionSpec &spec)
{
	JunctionLayout layout = RenderJunctionNodeTracks(tpl, values);
	PlaceJunctionSignals(layout, spec);
	NormaliseJunctionLayout(layout);
	return layout;
}

TEST_CASE("a flat T renders back from its template", "[junction][nodes]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	REQUIRE(FlatT().layout.has_value());
	const JunctionLayout &flat = *FlatT().layout;

	const JunctionNodeTemplate tpl = BuildJunctionNodeTemplate(flat, spec);
	INFO(tpl.error);
	REQUIRE(tpl.error.empty());
	const JunctionLayout layout = RenderWithSignals(tpl, tpl.flat, spec);

	CHECK(TrackKeys(layout) == TrackKeys(flat));
	CHECK(SignalKeys(layout) == SignalKeys(flat));
	CHECK(BridgeKeys(layout) == BridgeKeys(flat));
}

/** The index of the traced path from a to b. */
static size_t PathIndex(const JunctionNodeTemplate &tpl, JunctionPortRole from, JunctionPortRole to)
{
	for (size_t i = 0; i < tpl.paths.size(); i++) {
		if (tpl.paths[i].from == from && tpl.paths[i].to == to) return i;
	}
	FAIL("no such path");
	return 0;
}

/** The point of the path at a tile. */
static uint32_t PointAt(const JunctionNodeTemplate &tpl, size_t path, TileIndexDiffC tile)
{
	for (uint32_t p : tpl.paths[path].points) {
		if (tpl.points[p].tile.x == tile.x && tpl.points[p].tile.y == tile.y) return p;
	}
	FAIL("no point at the tile");
	return 0;
}

TEST_CASE("a longer main line between split and merge is still clean", "[junction][nodes]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	REQUIRE(FlatT().layout.has_value());
	const JunctionLayout &flat = *FlatT().layout;
	const JunctionNodeTemplate tpl = BuildJunctionNodeTemplate(flat, spec);
	REQUIRE(tpl.error.empty());

	JunctionPositions positions(tpl.flat.size(), -1024, 1024);
	REQUIRE(AddJunctionNodeConstraints(tpl, positions));

	const size_t path = PathIndex(tpl, JunctionPortRole::EIn, JunctionPortRole::EOut);
	const std::vector<JunctionChain> chains = ExtractJunctionChains(flat, spec);
	REQUIRE(chains.size() == tpl.paths.size());
	const JunctionChain &chain = chains[path];
	const auto split = std::ranges::find_if(chain.switches, [](const JunctionChainSwitch &s) { return s.split; });
	const auto merge = std::ranges::find_if(chain.switches, [](const JunctionChainSwitch &s) { return s.prio; });
	REQUIRE(split != chain.switches.end());
	REQUIRE(merge != chain.switches.end());
	const JunctionNodePoint &s = tpl.points[PointAt(tpl, path, split->tile)];
	const JunctionNodePoint &m = tpl.points[PointAt(tpl, path, merge->tile)];
	REQUIRE(s.cls != m.cls);

	/* d * (x_M - x_S) >= d * F + 2 with x = value + tile.x - flat[cls]. */
	const int d = tpl.paths[path].dir;
	REQUIRE(d != 0);
	const int f = m.tile.x - s.tile.x;
	const int off_m = m.tile.x - tpl.flat[m.cls];
	const int off_s = s.tile.x - tpl.flat[s.cls];
	if (d > 0) {
		REQUIRE(positions.AddDifference(m.cls, s.cls, f + 2 - (off_m - off_s)));
	} else {
		REQUIRE(positions.AddDifference(s.cls, m.cls, -f + 2 + (off_m - off_s)));
	}

	const std::vector<int> values = JunctionNodeSnap(tpl, positions);
	REQUIRE(values.size() == tpl.flat.size());
	const JunctionLayout layout = RenderWithSignals(tpl, values, spec);
	const std::vector<RuleViolation> violations = CheckJunctionRules(layout, spec);
	std::string text;
	for (const RuleViolation &v : violations) text += fmt::format("rule {} at {},{}: {}\n", to_underlying(v.rule), v.tile.x, v.tile.y, v.message);
	INFO(text);
	CHECK(violations.empty());
	CHECK(layout.size.x == flat.size.x + 2);
}

TEST_CASE("the flat T's positions meet its template", "[junction][nodes]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	REQUIRE(FlatT().layout.has_value());
	const JunctionNodeTemplate tpl = BuildJunctionNodeTemplate(*FlatT().layout, spec);
	REQUIRE(tpl.error.empty());

	JunctionPositions positions(tpl.flat.size(), -1024, 1024);
	REQUIRE(AddJunctionNodeConstraints(tpl, positions));
	for (size_t c = 0; c < tpl.flat.size(); c++) {
		INFO("class " << c);
		CHECK(positions.Lo(c) <= tpl.flat[c]);
		CHECK(tpl.flat[c] <= positions.Up(c));
	}
	CHECK(JunctionNodeSnap(tpl, positions) == tpl.flat);
}

TEST_CASE("the template of a hand-built S-bend", "[junction][nodes]")
{
	JunctionLayout layout;
	for (const JunctionRoute &route : {JunctionRoute{{0, 0}, Trackdir::X_SW, "FFFLFRFFFF"}, JunctionRoute{{9, 3}, Trackdir::X_NE, "FFFFFFFFF"}}) {
		const JunctionRaster raster = RasterizeJunctionRoute(route);
		REQUIRE(raster.error.empty());
		AddRasterToLayout(layout, raster);
	}
	NormaliseJunctionLayout(layout);

	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionNodeTemplate tpl = BuildJunctionNodeTemplate(layout, spec);
	INFO(tpl.error);
	REQUIRE(tpl.error.empty());

	const std::vector<JunctionChain> chains = ExtractJunctionChains(layout, spec);
	REQUIRE(chains.size() == tpl.paths.size());
	const auto bent = std::ranges::find_if(chains, [](const JunctionChain &c) { return c.curves.size() == 2; });
	REQUIRE(bent != chains.end());
	const JunctionNodePath &path = tpl.paths[static_cast<size_t>(bent - chains.begin())];

	std::vector<uint16_t> classes;
	for (uint32_t p : path.points) classes.push_back(tpl.points[p].cls);
	REQUIRE(classes.size() == 6);
	CHECK(classes[1] == classes[2]);
	CHECK(classes[1] == classes[3]);
	CHECK(classes[1] == classes[4]);
	CHECK(classes[0] != classes[1]);
	CHECK(classes[5] != classes[1]);
	CHECK(classes[0] != classes[5]);

	size_t free_links = 0;
	for (size_t k = 0; k + 1 < path.points.size(); k++) {
		const auto link = std::ranges::find_if(tpl.links, [&](const JunctionNodeLink &l) { return l.from == path.points[k] && l.to == path.points[k + 1]; });
		REQUIRE(link != tpl.links.end());
		free_links += link->free ? 1 : 0;
	}
	CHECK(free_links == 2);
	CHECK(path.dir == 1);

	CHECK(TrackKeys(RenderJunctionNodeTracks(tpl, tpl.flat)) == TrackKeys(layout));
}

/** A flat, Buildable terrain at z 0 sized to the spec's frame with a margin of 4 (a copy of MakeMapTestTerrain in junction_map.cpp). */
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

/** The offset that maps the flat T's layout to working coordinates. */
static TileIndexDiffC FlatToWorking(const JunctionSpec &spec)
{
	const JunctionFrame frame = MakeJunctionFrame(spec);
	return {static_cast<int16_t>(frame.hub_x - FlatT().hub.x), static_cast<int16_t>(-FlatT().hub.y)};
}

static std::string ViolationText(const std::vector<RuleViolation> &violations)
{
	std::string text;
	for (const RuleViolation &v : violations) text += fmt::format("rule {} at {},{}: {}\n", to_underlying(v.rule), v.tile.x, v.tile.y, v.message);
	return text;
}

/** The failure of a solve as text. */
static std::string FailureText(const JunctionNodeSolve &solve)
{
	if (!solve.failure.has_value()) return "no failure";
	return fmt::format("failure kind {} link {} column {} class {}", to_underlying(solve.failure->kind), solve.failure->link, solve.failure->column, solve.failure->cls);
}

TEST_CASE("the node solve builds the flat T on flat terrain", "[junction][nodes]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	REQUIRE(FlatT().layout.has_value());
	const JunctionLayout &flat = *FlatT().layout;
	const JunctionNodeTemplate tpl = BuildJunctionNodeTemplate(flat, spec);
	REQUIRE(tpl.error.empty());
	const JunctionTerrain terrain = MakeMapTestTerrain(spec);
	const JunctionFacts facts(terrain);
	const TileIndexDiffC offset = FlatToWorking(spec);

	const JunctionNodeSolve solve = SolveJunctionNodes(tpl, spec, facts, offset);
	REQUIRE(solve.layout.has_value());
	CHECK(solve.values == tpl.flat);
	CHECK(TrackKeys(*solve.layout) == TrackKeys(flat));
	CHECK(SignalKeys(*solve.layout) == SignalKeys(flat));
	CHECK(BridgeKeys(*solve.layout) == BridgeKeys(flat));
	for (const JunctionTrack &jt : flat.tracks) {
		INFO("tile " << jt.offset.x << "," << jt.offset.y);
		CHECK(JunctionNodeTrackFits(facts, jt.offset.x + offset.x, jt.offset.y + offset.y, jt.tracks));
	}
}

TEST_CASE("the node solve moves the W end off an incline", "[junction][nodes]")
{
	constexpr int STUB = 2;
	const JunctionSpec spec = MakeTestJunctionSpec();
	REQUIRE(FlatT().layout.has_value());
	const JunctionNodeTemplate tpl = BuildJunctionNodeTemplate(*FlatT().layout, spec);
	REQUIRE(tpl.error.empty());
	const int x_w = MakeJunctionFrame(spec).hub_x - FlatT().hub.x;
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
	const JunctionFacts facts(terrain);
	const TileIndexDiffC offset = FlatToWorking(spec);

	const JunctionNodeSolve solve = SolveJunctionNodes(tpl, spec, facts, offset);
	INFO(FailureText(solve));
	REQUIRE(solve.layout.has_value());
	const JunctionLayout &layout = *solve.layout;
	const std::vector<RuleViolation> violations = CheckJunctionRules(layout, spec);
	INFO(ViolationText(violations));
	CHECK(violations.empty());

	/* The anchor class is fixed, so its piece's working tile is its flat tile plus the offset; its layout tile gives the render's normalisation shift. */
	const JunctionCheckContext ctx = MakeJunctionCheckContext(layout, spec, FindJunctionPorts(layout), JunctionTMovements());
	const size_t path = PathIndex(tpl, JunctionPortRole::EIn, JunctionPortRole::SOut);
	const TileIndexDiffC anchor_flat = tpl.points[tpl.paths[path].points.back()].tile;
	const auto ctx_path = std::ranges::find_if(ctx.paths, [](const JunctionPath &p) { return p.from == JunctionPortRole::EIn && p.to == JunctionPortRole::SOut; });
	REQUIRE(ctx_path != ctx.paths.end());
	const TileIndexDiffC anchor_layout = ctx_path->pieces.back().tile;
	const int dx = anchor_flat.x + offset.x - anchor_layout.x;
	const int dy = anchor_flat.y + offset.y - anchor_layout.y;
	for (const JunctionTrack &jt : layout.tracks) {
		const bool head = std::ranges::any_of(layout.bridges, [&](const JunctionBridge &b) { return (b.start.x == jt.offset.x && b.start.y == jt.offset.y) || (b.end.x == jt.offset.x && b.end.y == jt.offset.y); });
		if (head) continue;
		INFO("layout tile " << jt.offset.x << "," << jt.offset.y << " bits " << static_cast<uint>(jt.tracks.base()));
		CHECK(JunctionNodeTrackFits(facts, jt.offset.x + dx, jt.offset.y + dy, jt.tracks));
	}
	for (size_t c = 0; c < tpl.flat.size(); c++) {
		if (solve.values[c] != tpl.flat[c]) WARN("class " << c << ": " << tpl.flat[c] << " -> " << solve.values[c]);
	}
}

TEST_CASE("a house beside the hub fails on A's run", "[junction][nodes]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	REQUIRE(FlatT().layout.has_value());
	const JunctionNodeTemplate tpl = BuildJunctionNodeTemplate(*FlatT().layout, spec);
	REQUIRE(tpl.error.empty());
	const JunctionFrame frame = MakeJunctionFrame(spec);
	JunctionTerrain terrain = MakeMapTestTerrain(spec);
	BlockMapTestTile(terrain, frame.hub_x - 1, 0);
	const JunctionFacts facts(terrain);

	const JunctionNodeSolve solve = SolveJunctionNodes(tpl, spec, facts, FlatToWorking(spec));
	CHECK(!solve.layout.has_value());
	REQUIRE(solve.failure.has_value());
	CHECK(solve.failure->kind == JunctionNodeFailure::Kind::Straight);
	CHECK(solve.failure->column == frame.hub_x - 1);
	REQUIRE(solve.failure->link < tpl.links.size());
	const JunctionNodeLink &link = tpl.links[solve.failure->link];
	const JunctionNodePath &path = tpl.paths[PathIndex(tpl, JunctionPortRole::EIn, JunctionPortRole::EOut)];
	bool on_path = false;
	for (size_t k = 0; k + 1 < path.points.size(); k++) on_path = on_path || (path.points[k] == link.from && path.points[k + 1] == link.to);
	CHECK(on_path);
}

TEST_CASE("facts move the end of a free run", "[junction][nodes]")
{
	JunctionLayout layout;
	for (const JunctionRoute &route : {JunctionRoute{{0, 0}, Trackdir::X_SW, "FFFLFRFFFF"}, JunctionRoute{{9, 3}, Trackdir::X_NE, "FFFFFFFFF"}}) {
		const JunctionRaster raster = RasterizeJunctionRoute(route);
		REQUIRE(raster.error.empty());
		AddRasterToLayout(layout, raster);
	}
	NormaliseJunctionLayout(layout);
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionNodeTemplate tpl = BuildJunctionNodeTemplate(layout, spec);
	REQUIRE(tpl.error.empty());

	const auto chains = ExtractJunctionChains(layout, spec);
	REQUIRE(chains.size() == tpl.paths.size());
	const auto bent = std::ranges::find_if(chains, [](const JunctionChain &c) { return c.curves.size() == 2; });
	REQUIRE(bent != chains.end());
	const JunctionNodePoint &end = tpl.points[tpl.paths[static_cast<size_t>(bent - chains.begin())].points.back()];

	JunctionTerrain terrain;
	terrain.origin = {0, 0};
	terrain.width = 16;
	terrain.height = 6;
	terrain.tiles.assign(static_cast<size_t>(terrain.width) * terrain.height, JunctionTerrainTile{});
	const int blocked_x = end.tile.x - 2;
	BlockMapTestTile(terrain, blocked_x, end.tile.y);
	const JunctionFacts facts(terrain);

	const JunctionNodeSolve solve = SolveJunctionNodes(tpl, spec, facts, {0, 0});
	INFO(FailureText(solve));
	REQUIRE(solve.layout.has_value());
	CHECK(solve.values[end.cls] == blocked_x - 1);
	for (const JunctionTrack &jt : solve.layout->tracks) CHECK(!(jt.offset.x == blocked_x && jt.offset.y == end.tile.y));
}

TEST_CASE("JunctionNodeTrackFits by kind", "[junction][nodes]")
{
	JunctionTerrain terrain;
	terrain.origin = {0, 0};
	terrain.width = 4;
	terrain.height = 1;
	terrain.build_on_slopes = false; /* Otherwise the ramp tile counts as level at its top and Y, half and switch pieces would fit on it. */
	terrain.tiles.assign(4, JunctionTerrainTile{});
	terrain.tiles[1].z_max = 1;
	terrain.tiles[1].incline = DiagDirection::SW;
	terrain.tiles[2].cls = JunctionTileClass::Blocked;
	const JunctionFacts facts(terrain);

	CHECK(facts.MinX() == 0);
	CHECK(facts.MaxX() == 3);

	const TrackBits x{Track::X};
	const TrackBits y{Track::Y};
	const TrackBits half{Track::Upper};
	const TrackBits two{Track::X, Track::Upper};
	CHECK(JunctionNodeTrackFits(facts, 0, 0, x));
	CHECK(JunctionNodeTrackFits(facts, 1, 0, x));
	CHECK(!JunctionNodeTrackFits(facts, 1, 0, y));
	for (const TrackBits bits : {x, y, half, two}) CHECK(!JunctionNodeTrackFits(facts, 2, 0, bits));
	CHECK(JunctionNodeTrackFits(facts, 0, 0, half));
	CHECK(JunctionNodeTrackFits(facts, 0, 0, two));
	CHECK(!JunctionNodeTrackFits(facts, 1, 0, half));
	CHECK(!JunctionNodeTrackFits(facts, 1, 0, two));
}
