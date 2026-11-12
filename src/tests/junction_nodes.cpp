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
#include "../junction_line.h"
#include "../junction_line_router.h"
#include "../junction_nodes.h"
#include "../junction_positions.h"
#include "../junction_route.h"
#include "../junction_facts.h"
#include "../junction_rules.h"
#include "../junction_rules_internal.h"
#include "../junction_signal_pass.h"
#include "../rail.h"
#include "../slope_func.h"
#include "../track_func.h"

#include <algorithm>
#include <chrono>
#include <climits>
#include <optional>
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
	std::string text = fmt::format("failure kind {} link {} column {} class {}", to_underlying(solve.failure->kind), solve.failure->link, solve.failure->column, solve.failure->cls);
	if (solve.failure->height.has_value()) {
		const JunctionNodeHeightFault &fault = *solve.failure->height;
		text += fmt::format(" height part {} index {} row {} x {} tiles", to_underlying(fault.part), fault.index, fault.row, fault.x);
		for (const TileIndexDiffC &tile : fault.tiles) text += fmt::format(" ({},{})", tile.x, tile.y);
	}
	return text;
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
	std::string why;
	CHECK(JunctionNodeHeightsOk(*solve.layout, spec, terrain, offset, &why));
	INFO(why);
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
				tile.slope = InclinedSlope(DiagDirection::NE);
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
	std::string why;
	CHECK(JunctionNodeHeightsOk(layout, spec, terrain, {static_cast<int16_t>(dx), static_cast<int16_t>(dy)}, &why));
	INFO(why);
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
	terrain.tiles[1].slope = InclinedSlope(DiagDirection::SW);
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

/** A flat terrain at z 0 over 6 x 2 tiles from (0, 0). */
static JunctionTerrain MakeStripTerrain()
{
	JunctionTerrain terrain;
	terrain.origin = {0, 0};
	terrain.width = 6;
	terrain.height = 2;
	terrain.tiles.assign(12, JunctionTerrainTile{});
	terrain.build_on_slopes = true;
	return terrain;
}

/** Set the heights and incline of one column of a strip terrain. */
static void SetStripColumn(JunctionTerrain &terrain, int x, int z_min, int z_max, DiagDirection incline)
{
	for (int y = 0; y < terrain.height; y++) {
		JunctionTerrainTile &tile = terrain.tiles[static_cast<size_t>(y) * terrain.width + x];
		tile.z_min = static_cast<int8_t>(z_min);
		tile.z_max = static_cast<int8_t>(z_max);
		tile.incline = incline;
		tile.slope = incline == DiagDirection::Invalid ? SLOPE_FLAT : InclinedSlope(incline);
	}
}

TEST_CASE("heights: a half piece on a foundation beside a ramp does not connect", "[junction][nodes]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	JunctionLayout layout;
	AddRasterToLayout(layout, RasterizeJunctionRoute({{0, 0}, Trackdir::X_SW, "FFFFF"}));
	AddRasterToLayout(layout, RasterizeJunctionRoute({{5, 1}, Trackdir::X_NE, "FFFFF"}));
	NormaliseJunctionLayout(layout);

	/* Level at 0, a ramp up (towards SW) over x = 2, then the ground at 0: the piece after the ramp needs height 1. */
	JunctionTerrain terrain = MakeStripTerrain();
	SetStripColumn(terrain, 2, 0, 1, DiagDirection::SW);
	std::string why;
	CHECK_FALSE(JunctionNodeHeightsOk(layout, spec, terrain, {0, 0}, &why));
	INFO(why);
	CHECK(why.find("piece 3 at 3,0") != std::string::npos);

	/* The ground at 1 after the ramp: it connects. */
	for (int x = 3; x < 6; x++) SetStripColumn(terrain, x, 1, 1, DiagDirection::Invalid);
	why.clear();
	CHECK(JunctionNodeHeightsOk(layout, spec, terrain, {0, 0}, &why));
	INFO(why);
}

TEST_CASE("heights: the line router's route across an incline is accepted", "[junction][nodes]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	JunctionLineRouteRequest request;
	request.line = {2, 1, 1};
	request.start = {40, 20};
	request.start_trackdir = Trackdir::X_NE;
	request.goals = {{{20, 20}, Trackdir::X_NE}};
	request.min = {10, 5};
	request.max = {50, 40};
	request.curve_spacing = 4;
	request.rail_piece = 1;
	request.slope_piece = 100;
	request.plane = 1;
	std::vector<int> rows;
	for (const JunctionRaster &raster : RasterizeJunctionLine(request.line, {request.start, request.start_trackdir, ""})) rows.push_back(raster.pieces[0].tile.y);
	request.min.y = static_cast<int16_t>(*std::ranges::min_element(rows));
	request.max.y = static_cast<int16_t>(*std::ranges::max_element(rows));

	JunctionTerrain terrain;
	terrain.origin = request.min;
	terrain.width = request.max.x - request.min.x + 1;
	terrain.height = request.max.y - request.min.y + 1;
	JunctionTerrainTile ground;
	ground.z_min = ground.z_max = 1;
	ground.clear_cost = 5;
	terrain.tiles.assign(static_cast<size_t>(terrain.width) * terrain.height, ground);
	terrain.build_on_slopes = true;
	terrain.foundation_cost = 1;
	for (const auto &[x, incline] : {std::pair{33, DiagDirection::SW}, std::pair{32, DiagDirection::NE}}) {
		for (int y = terrain.origin.y; y < terrain.origin.y + terrain.height; y++) {
			JunctionTerrainTile &tile = terrain.tiles[static_cast<size_t>(y - terrain.origin.y) * terrain.width + (x - terrain.origin.x)];
			tile.z_min = 0;
			tile.z_max = 1;
			tile.incline = incline;
			tile.slope = incline == DiagDirection::Invalid ? SLOPE_FLAT : InclinedSlope(incline);
		}
	}
	request.terrain = &terrain;
	const JunctionLineRouteResult result = FindJunctionLineRoute(request);
	REQUIRE(result.route.has_value());

	/* The raster is laid at the router's coordinates, then normalised: the offset maps the layout back. */
	JunctionLayout layout;
	for (const JunctionRaster &raster : RasterizeJunctionLine(request.line, *result.route)) AddRasterToLayout(layout, raster);
	int min_x = INT16_MAX;
	int min_y = INT16_MAX;
	for (const JunctionTrack &jt : layout.tracks) {
		min_x = std::min<int>(min_x, jt.offset.x);
		min_y = std::min<int>(min_y, jt.offset.y);
	}
	NormaliseJunctionLayout(layout);
	const JunctionCheckContext ctx = MakeJunctionCheckContext(layout, spec, FindJunctionPorts(layout), JunctionTMovements());
	INFO("traced paths " << ctx.paths.size());
	REQUIRE_FALSE(ctx.paths.empty());
	std::string why;
	CHECK(JunctionNodeHeightsOk(layout, spec, terrain, {static_cast<int16_t>(min_x), static_cast<int16_t>(min_y)}, &why));
	INFO(why);
}

/** Search from the flat T on the given terrain, WARN the states, the log and the time. */
static JunctionNodeSearch SearchFlatT(const JunctionSpec &spec, const JunctionTerrain &terrain)
{
	const auto start = std::chrono::steady_clock::now();
	const JunctionNodeSearch search = SearchJunctionNodes(*FlatT().layout, spec, terrain, FlatToWorking(spec), 16);
	const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
	WARN("states " << search.states << " in " << micros << " us\n" << search.log);
	return search;
}

TEST_CASE("the node search builds around a house beside the hub", "[junction][nodes]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	REQUIRE(FlatT().layout.has_value());
	JunctionTerrain terrain = MakeMapTestTerrain(spec);
	BlockMapTestTile(terrain, MakeJunctionFrame(spec).hub_x - 1, 0);

	const JunctionNodeSearch search = SearchFlatT(spec, terrain);
	INFO(search.log);
	REQUIRE(search.layout.has_value());
	CHECK(search.states <= 16);
}

TEST_CASE("the node search builds around an unbuildable hub tile", "[junction][nodes]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	REQUIRE(FlatT().layout.has_value());
	JunctionTerrain terrain = MakeMapTestTerrain(spec);
	BlockMapTestTile(terrain, MakeJunctionFrame(spec).hub_x, 0);

	const JunctionNodeSearch search = SearchFlatT(spec, terrain);
	INFO(search.log);
	REQUIRE(search.layout.has_value());
	CHECK(search.states <= 16);
}

TEST_CASE("the node search keeps the flat T on flat terrain", "[junction][nodes]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	REQUIRE(FlatT().layout.has_value());

	/* The track-only start state has the template of the finished flat layout. */
	const JunctionNodeTemplate finished = BuildJunctionNodeTemplate(*FlatT().layout, spec);
	REQUIRE(finished.error.empty());
	const JunctionNodeTemplate track_only = BuildJunctionNodeTemplate(RenderJunctionNodeTracks(finished, finished.flat), spec);
	REQUIRE(track_only.error.empty());
	auto free_links = [](const JunctionNodeTemplate &tpl) { return std::ranges::count_if(tpl.links, [](const JunctionNodeLink &l) { return l.free; }); };
	INFO("finished: points " << finished.points.size() << " links " << finished.links.size() << " free " << free_links(finished) << " classes " << finished.flat.size() << " diffs " << finished.diffs.size());
	INFO("track-only: points " << track_only.points.size() << " links " << track_only.links.size() << " free " << free_links(track_only) << " classes " << track_only.flat.size() << " diffs " << track_only.diffs.size());
	REQUIRE(track_only.points.size() == finished.points.size());
	REQUIRE(track_only.links.size() == finished.links.size());
	REQUIRE(free_links(track_only) == free_links(finished));
	REQUIRE(track_only.flat.size() == finished.flat.size());
	REQUIRE(track_only.diffs.size() == finished.diffs.size());

	const JunctionNodeSearch search = SearchFlatT(spec, MakeMapTestTerrain(spec));
	INFO(search.log);
	CHECK(search.states == 1);
	REQUIRE(search.layout.has_value());
	CHECK(TrackKeys(*search.layout) == TrackKeys(*FlatT().layout));
}

/** A straight X line of 15 tiles from (0, row_a) and a second one back from (14, row_b), normalised (the first line ends on `row`); the template and the link of the first line. */
struct DetourFixture {
	JunctionLayout layout;
	JunctionNodeTemplate tpl;
	uint32_t link = UINT32_MAX;
	int row = 0; ///< The row of the first line.

	DetourFixture(int row_a, int row_b)
	{
		for (const JunctionRoute &route : {JunctionRoute{{0, static_cast<int16_t>(row_a)}, Trackdir::X_SW, "FFFFFFFFFFFFFF"}, JunctionRoute{{14, static_cast<int16_t>(row_b)}, Trackdir::X_NE, "FFFFFFFFFFFFFF"}}) {
			const JunctionRaster raster = RasterizeJunctionRoute(route);
			REQUIRE(raster.error.empty());
			AddRasterToLayout(layout, raster);
		}
		NormaliseJunctionLayout(layout);
		tpl = BuildJunctionNodeTemplate(layout, MakeTestJunctionSpec());
		REQUIRE(tpl.error.empty());
		row = row_a - std::min(row_a, row_b);
		for (uint32_t i = 0; i < tpl.links.size(); i++) {
			if (tpl.links[i].free && tpl.points[tpl.links[i].from].tile.y == row && tpl.points[tpl.links[i].from].tile.x == 0) link = i;
		}
		REQUIRE(link != UINT32_MAX);
	}
};

static bool HasTrackAt(const JunctionLayout &layout, int x, int y)
{
	return std::ranges::any_of(layout.tracks, [&](const JunctionTrack &jt) { return jt.offset.x == x && jt.offset.y == y && jt.tracks.Any(); });
}

TEST_CASE("a detour around a column of the hand-built line", "[junction][nodes]")
{
	const DetourFixture fixture(2, 5);
	const int row = fixture.row;
	const JunctionSpec spec = MakeTestJunctionSpec();

	std::string why;
	const std::optional<JunctionLayout> up = InsertJunctionDetour(fixture.layout, fixture.tpl, fixture.link, 7, -1, spec, &why);
	INFO(why);
	REQUIRE(up.has_value());
	CHECK(!HasTrackAt(*up, 7, row));
	CHECK(HasTrackAt(*up, 7, row - 1));
	JunctionLayout normalised = *up;
	NormaliseJunctionLayout(normalised);
	const std::vector<JunctionChain> chains = ExtractJunctionChains(normalised, spec);
	CHECK(std::ranges::any_of(chains, [](const JunctionChain &c) { return c.curves.size() == 4; }));

	why.clear();
	const std::optional<JunctionLayout> down = InsertJunctionDetour(fixture.layout, fixture.tpl, fixture.link, 7, 1, spec, &why);
	INFO(why);
	REQUIRE(down.has_value());
	CHECK(!HasTrackAt(*down, 7, row));
	CHECK(HasTrackAt(*down, 7, row + 1));

	why.clear();
	CHECK(!InsertJunctionDetour(fixture.layout, fixture.tpl, fixture.link, 0, -1, spec, &why).has_value());
	CHECK(why.find("column outside the link") != std::string::npos);
}

TEST_CASE("a detour collides with a parallel line", "[junction][nodes]")
{
	const DetourFixture fixture(2, 3);
	const JunctionSpec spec = MakeTestJunctionSpec();
	std::string why;
	CHECK(!InsertJunctionDetour(fixture.layout, fixture.tpl, fixture.link, 7, 1, spec, &why).has_value());
	CHECK(why.find("collision") != std::string::npos);
}

/** The first Y-free link on the path from a to b (index into tpl.links), or UINT32_MAX. */
static uint32_t FirstYFreeLink(const JunctionNodeTemplate &tpl, JunctionPortRole from, JunctionPortRole to)
{
	const JunctionNodePath &path = tpl.paths[PathIndex(tpl, from, to)];
	for (size_t k = 0; k + 1 < path.points.size(); k++) {
		for (uint32_t i = 0; i < tpl.links.size(); i++) {
			const JunctionNodeLink &l = tpl.links[i];
			if (l.from == path.points[k] && l.to == path.points[k + 1] && l.free && l.axis == Axis::Y) return i;
		}
	}
	return UINT32_MAX;
}

TEST_CASE("a deeper box keeps the flat T clean", "[junction][nodes]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	REQUIRE(FlatT().layout.has_value());
	const JunctionLayout &flat = *FlatT().layout;
	const JunctionNodeTemplate tpl = BuildJunctionNodeTemplate(flat, spec);
	INFO(tpl.error);
	REQUIRE(tpl.error.empty());

	JunctionPositions positions(tpl.flat.size(), -1024, 1024);
	REQUIRE(AddJunctionNodeConstraints(tpl, positions));

	/* y_S - y_A >= (flat y_S - flat y_A) + 2 with y = value + tile.y - flat[ycls]. */
	const JunctionNodePath &path = tpl.paths[PathIndex(tpl, JunctionPortRole::EIn, JunctionPortRole::SOut)];
	const JunctionNodePoint &a = tpl.points[path.points.front()];
	const JunctionNodePoint &s = tpl.points[path.points.back()];
	const int off_a = a.tile.y - tpl.flat[a.ycls];
	const int off_s = s.tile.y - tpl.flat[s.ycls];
	const int f = s.tile.y - a.tile.y;
	REQUIRE(positions.AddDifference(s.ycls, a.ycls, f + 2 - (off_s - off_a)));

	const std::vector<int> values = JunctionNodeSnap(tpl, positions);
	REQUIRE(values.size() == tpl.flat.size());
	const JunctionLayout layout = RenderWithSignals(tpl, values, spec);
	const std::vector<RuleViolation> violations = CheckJunctionRules(layout, spec);
	INFO(ViolationText(violations));
	CHECK(violations.empty());
	CHECK(layout.size.y == flat.size.y + 2);
}

TEST_CASE("the flat T's vertical runs are free", "[junction][nodes]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	REQUIRE(FlatT().layout.has_value());
	const JunctionNodeTemplate tpl = BuildJunctionNodeTemplate(*FlatT().layout, spec);
	INFO(tpl.error);
	REQUIRE(tpl.error.empty());

	size_t x_free = 0;
	size_t y_free = 0;
	for (const JunctionNodeLink &link : tpl.links) {
		if (!link.free) continue;
		if (link.axis == Axis::X) x_free++;
		if (link.axis == Axis::Y) {
			y_free++;
			CHECK(tpl.points[link.from].cls == tpl.points[link.to].cls);
			CHECK(tpl.points[link.from].tile.x == tpl.points[link.to].tile.x);
		}
	}
	CHECK(y_free >= 3);

	const JunctionNodePath &path = tpl.paths[PathIndex(tpl, JunctionPortRole::EIn, JunctionPortRole::SOut)];
	CHECK(tpl.anchor_y == tpl.points[path.points.front()].ycls);

	WARN("points " << tpl.points.size() << ", links " << tpl.links.size() << ", X-free " << x_free << ", Y-free " << y_free << ", X classes " << tpl.x_classes << ", Y classes " << (tpl.flat.size() - tpl.x_classes) << ", diffs " << tpl.diffs.size());
}

TEST_CASE("a house on a vertical leg fails on its class", "[junction][nodes]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	REQUIRE(FlatT().layout.has_value());
	const JunctionNodeTemplate tpl = BuildJunctionNodeTemplate(*FlatT().layout, spec);
	REQUIRE(tpl.error.empty());

	const uint32_t leg = FirstYFreeLink(tpl, JunctionPortRole::EIn, JunctionPortRole::SOut);
	REQUIRE(leg != UINT32_MAX);
	const JunctionNodePoint &from = tpl.points[tpl.links[leg].from];
	const JunctionNodePoint &to = tpl.points[tpl.links[leg].to];
	const int lo = std::min<int>(from.tile.y, to.tile.y);
	const int hi = std::max<int>(from.tile.y, to.tile.y);
	REQUIRE(hi - lo >= 2);

	const TileIndexDiffC offset = FlatToWorking(spec);
	JunctionTerrain terrain = MakeMapTestTerrain(spec);
	BlockMapTestTile(terrain, from.tile.x + offset.x, (lo + hi) / 2 + offset.y);
	const JunctionFacts facts(terrain);

	const JunctionNodeSolve solve = SolveJunctionNodes(tpl, spec, facts, offset);
	CHECK(!solve.layout.has_value());
	INFO(FailureText(solve));
	REQUIRE(solve.failure.has_value());
	/* The column check now runs in the Y snap (DESIGN-N3 §10), so the house is found there: the Y class that fixes last of the column's two ends. */
	CHECK(solve.failure->kind == JunctionNodeFailure::Kind::Height);
	CHECK(solve.failure->cls == std::max(from.ycls, to.ycls));
	REQUIRE(solve.failure->height.has_value());
	const JunctionNodeHeightFault &fault = *solve.failure->height;
	CHECK(fault.part == JunctionNodeHeightFault::Part::Column);
	CHECK(fault.index == leg);
	CHECK(std::ranges::any_of(fault.tiles, [&](const TileIndexDiffC &tile) { return tile.x == from.tile.x + offset.x && tile.y == (lo + hi) / 2 + offset.y; }));
}

TEST_CASE("a longer E->S vertical leg", "[junction][nodes]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	REQUIRE(FlatT().layout.has_value());
	const JunctionNodeTemplate tpl = BuildJunctionNodeTemplate(*FlatT().layout, spec);
	REQUIRE(tpl.error.empty());

	const uint32_t leg = FirstYFreeLink(tpl, JunctionPortRole::EIn, JunctionPortRole::SOut);
	REQUIRE(leg != UINT32_MAX);
	const JunctionNodePoint &p = tpl.points[tpl.links[leg].from];
	const JunctionNodePoint &q = tpl.points[tpl.links[leg].to];

	const JunctionNodePath &path = tpl.paths[PathIndex(tpl, JunctionPortRole::EIn, JunctionPortRole::SOut)];
	for (size_t k = 0; k + 1 < path.points.size(); k++) {
		const JunctionNodePoint &from = tpl.points[path.points[k]];
		const JunctionNodePoint &to = tpl.points[path.points[k + 1]];
		const auto link = std::ranges::find_if(tpl.links, [&](const JunctionNodeLink &l) { return l.from == path.points[k] && l.to == path.points[k + 1]; });
		WARN("E->S link " << k << ": " << from.tile.x << "," << from.tile.y << " (x " << from.cls << ", y " << from.ycls << ") to " << to.tile.x << "," << to.tile.y << " (x " << to.cls << ", y " << to.ycls << ") " << (link->free ? (link->axis == Axis::X ? "free X" : "free Y") : "rigid"));
	}

	JunctionPositions positions(tpl.flat.size(), -1024, 1024);
	REQUIRE(AddJunctionNodeConstraints(tpl, positions));
	const int off_p = p.tile.y - tpl.flat[p.ycls];
	const int off_q = q.tile.y - tpl.flat[q.ycls];
	const bool added = positions.AddDifference(q.ycls, p.ycls, (q.tile.y - p.tile.y) + 1 - (off_q - off_p));
	WARN("longer leg: p " << p.tile.x << "," << p.tile.y << " y class " << p.ycls << ", q " << q.tile.x << "," << q.tile.y << " y class " << q.ycls << ", AddDifference " << (added ? "ok" : "failed"));
	if (!added) return;
	const std::vector<int> values = JunctionNodeSnap(tpl, positions);
	WARN("longer leg: snap " << (values.size() == tpl.flat.size() ? "ok" : "failed"));
	if (values.size() != tpl.flat.size()) return;
	const JunctionLayout layout = RenderWithSignals(tpl, values, spec);
	WARN("longer leg: rule violations " << CheckJunctionRules(layout, spec).size() << "\n" << ViolationText(CheckJunctionRules(layout, spec)));
}

/** The halves of a free diagonal at the solved values: its flat halves plus two per map step (DESIGN-N3 §6). */
static int DiagonalHalves(const JunctionNodeTemplate &tpl, const std::vector<int> &values, size_t diagonal)
{
	const JunctionNodeDiagonal &d = tpl.diagonals[diagonal];
	const JunctionNodeLink &link = tpl.links[d.link];
	const int shift_p = values[tpl.points[link.from].ycls] - tpl.flat[tpl.points[link.from].ycls];
	const int shift_q = values[tpl.points[link.to].ycls] - tpl.flat[tpl.points[link.to].ycls];
	return d.halves + 2 * d.dy * (shift_q - shift_p);
}

/**
 * The spec junction_export builds for `ml 1 branch 1 tl 4 train=14194,1090000,561,25,<speed>` without prio=: the command, the speed, the loaded train and the sized prio.
 * MakeJunctionSpec also reads the game (an unlimited rail type, curve_speed 0, realistic acceleration, block signals, signal density 2 as in the rig's configuration), which the test spec carries.
 * The prio coverage is SizeJunctionPrio's result, measured once per speed: sizing takes about 7 s for the six speeds (Release), far over the [nodes] budget.
 */
static JunctionSpec MakeExportSpec(uint16_t speed)
{
	JunctionSpec spec = MakeTestJunctionSpec();
	spec.train.max_speed = speed;
	spec.reference_speed = speed;
	spec.prio_tiles = speed == 100 ? 9 : speed == 120 ? 8 : speed == 140 ? 10 : speed == 160 ? 11 : speed == 180 ? 12 : 14;
	return spec;
}

/** Solve the test-spec topology at one design speed on flat land and check it with the rule checker. */
static void CheckNodeSolveAtSpeed(uint16_t speed)
{
	const JunctionSpec spec = MakeExportSpec(speed);
	INFO("speed " << speed << " prio_tiles " << static_cast<int>(spec.prio_tiles));
	const JunctionNodeTemplate tpl = BuildJunctionNodeTemplate(*FlatT().layout, spec);
	INFO("template: " << tpl.error);
	CHECK(tpl.error.empty());
	if (!tpl.error.empty()) return;

	const JunctionTerrain terrain = MakeMapTestTerrain(spec);
	const JunctionFacts facts(terrain);
	const JunctionNodeSolve solve = SolveJunctionNodes(tpl, spec, facts, FlatToWorking(spec));
	INFO(FailureText(solve));
	CHECK(solve.layout.has_value());
	if (!solve.layout.has_value()) return;

	const std::vector<RuleViolation> violations = CheckJunctionRules(*solve.layout, spec);
	INFO(ViolationText(violations));
	CHECK(violations.empty());
	std::string halves;
	for (size_t i = 0; i < tpl.diagonals.size(); i++) halves += fmt::format(" {}", DiagonalHalves(tpl, solve.values, i));
	WARN("speed " << speed << " prio_tiles " << static_cast<int>(spec.prio_tiles) << " halves" << halves << " size " << solve.layout->size.x << "x" << solve.layout->size.y);
}

TEST_CASE("the node solve builds the T at 196 km/h on flat land", "[junction][nodes]")
{
	REQUIRE(FlatT().layout.has_value());
	CheckNodeSolveAtSpeed(196);
}

TEST_CASE("the node solve builds the T at 100 to 196 km/h on flat land", "[junction][nodespeed]")
{
	REQUIRE(FlatT().layout.has_value());
	for (const uint16_t speed : {100, 120, 140, 160, 180, 196}) CheckNodeSolveAtSpeed(speed);
}

TEST_CASE("the node search does not run the rule checker", "[junction][nodes]")
{
	const JunctionSpec spec = MakeExportSpec(196);
	REQUIRE(FlatT().layout.has_value());
	const JunctionTerrain terrain = MakeMapTestTerrain(spec);

	const JunctionNodeSearch search = SearchFlatT(spec, terrain);
	REQUIRE(search.layout.has_value());
	CHECK(search.log.find("rules:") == std::string::npos);
	CHECK(search.checked == 0);
}

TEST_CASE("a lower N split stretches N->W's diagonal", "[junction][nodes]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	REQUIRE(FlatT().layout.has_value());
	const JunctionNodeTemplate tpl = BuildJunctionNodeTemplate(*FlatT().layout, spec);
	REQUIRE(tpl.error.empty());

	/* The shared split: the first point after the start that NIn->WOut has in common with NIn->EOut. */
	const JunctionNodePath &to_w = tpl.paths[PathIndex(tpl, JunctionPortRole::NIn, JunctionPortRole::WOut)];
	const JunctionNodePath &to_e = tpl.paths[PathIndex(tpl, JunctionPortRole::NIn, JunctionPortRole::EOut)];
	uint32_t split = UINT32_MAX;
	for (size_t k = 1; k < to_w.points.size() && split == UINT32_MAX; k++) {
		if (std::ranges::find(to_e.points, to_w.points[k]) != to_e.points.end()) split = to_w.points[k];
	}
	REQUIRE(split != UINT32_MAX);
	const JunctionNodePoint &s = tpl.points[split];
	const JunctionNodePoint &a = tpl.points[tpl.paths[PathIndex(tpl, JunctionPortRole::EIn, JunctionPortRole::SOut)].points.front()];

	JunctionPositions positions(tpl.flat.size(), -1024, 1024);
	REQUIRE(AddJunctionNodeConstraints(tpl, positions));
	const int off_s = s.tile.y - tpl.flat[s.ycls];
	const int off_a = a.tile.y - tpl.flat[a.ycls];
	REQUIRE(positions.AddDifference(s.ycls, a.ycls, (s.tile.y - a.tile.y) + 1 - (off_s - off_a)));

	const std::vector<int> values = JunctionNodeSnap(tpl, positions);
	REQUIRE(values.size() == tpl.flat.size());
	const JunctionLayout layout = RenderWithSignals(tpl, values, spec);
	const std::vector<RuleViolation> violations = CheckJunctionRules(layout, spec);
	INFO(ViolationText(violations));
	CHECK(violations.empty());

	/* N->W's diagonal follows the split on its Y class and is the only free diagonal of the path. */
	size_t found = 0;
	for (size_t i = 0; i < tpl.diagonals.size(); i++) {
		const JunctionNodeLink &link = tpl.links[tpl.diagonals[i].link];
		const bool on_path = std::ranges::find(to_w.points, link.from) != to_w.points.end() && std::ranges::find(to_w.points, link.to) != to_w.points.end();
		if (!on_path) continue;
		found++;
		CHECK(tpl.points[link.from].ycls == s.ycls);
		CHECK(DiagonalHalves(tpl, values, i) == tpl.diagonals[i].halves + 2);
	}
	CHECK(found == 1);
}

TEST_CASE("the flat T's free diagonals", "[junction][nodes]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	REQUIRE(FlatT().layout.has_value());
	const JunctionNodeTemplate tpl = BuildJunctionNodeTemplate(*FlatT().layout, spec);
	INFO(tpl.error);
	REQUIRE(tpl.error.empty());

	/* RECON D1, D3, D4, D5, D7. */
	std::vector<int> halves;
	for (const JunctionNodeDiagonal &d : tpl.diagonals) {
		CHECK(tpl.links[d.link].free);
		CHECK(tpl.links[d.link].diagonal);
		halves.push_back(d.halves);
	}
	std::ranges::sort(halves);
	CHECK(halves == std::vector<int>{5, 5, 5, 6, 13});

	/* D2, EIn->SOut's second diagonal (under N->E's bridge), is rigid: only D1 of that path is free. */
	const JunctionNodePath &path = tpl.paths[PathIndex(tpl, JunctionPortRole::EIn, JunctionPortRole::SOut)];
	size_t free_on_path = 0;
	for (size_t k = 0; k + 1 < path.points.size(); k++) {
		const auto link = std::ranges::find_if(tpl.links, [&](const JunctionNodeLink &l) { return l.from == path.points[k] && l.to == path.points[k + 1]; });
		REQUIRE(link != tpl.links.end());
		free_on_path += link->diagonal ? 1 : 0;
	}
	CHECK(free_on_path == 1);
}

/** The Y variables of the two lines of a DetourFixture: the first line's (its row is fixture.row) and the second's. */
static std::pair<uint16_t, uint16_t> FixtureLineRows(const DetourFixture &fixture)
{
	uint16_t y_a = UINT16_MAX;
	uint16_t y_b = UINT16_MAX;
	for (const JunctionNodePoint &point : fixture.tpl.points) {
		if (point.tile.x != 0 && point.tile.x != 14) continue;
		(point.tile.y == fixture.row ? y_a : y_b) = point.ycls;
	}
	REQUIRE(y_a != UINT16_MAX);
	REQUIRE(y_b != UINT16_MAX);
	return {y_a, y_b};
}

TEST_CASE("two lines forced onto one row collide", "[junction][nodes]")
{
	const DetourFixture fixture(2, 4);
	const JunctionNodeTemplate &tpl = fixture.tpl;
	JunctionPositions positions(tpl.flat.size(), -1024, 1024);
	REQUIRE(AddJunctionNodeConstraints(tpl, positions));
	const auto [y_a, y_b] = FixtureLineRows(fixture);
	REQUIRE(positions.AddDifference(y_b, y_a, 0));
	REQUIRE(positions.AddDifference(y_a, y_b, 0));
	CHECK(JunctionNodeSnap(tpl, positions).empty());
}

TEST_CASE("two lines forced onto adjacent rows stay apart", "[junction][nodes]")
{
	const DetourFixture fixture(2, 4);
	const JunctionNodeTemplate &tpl = fixture.tpl;
	JunctionPositions positions(tpl.flat.size(), -1024, 1024);
	REQUIRE(AddJunctionNodeConstraints(tpl, positions));
	const auto [y_a, y_b] = FixtureLineRows(fixture);
	REQUIRE(positions.AddDifference(y_b, y_a, 1));
	REQUIRE(positions.AddDifference(y_a, y_b, -1));
	const std::vector<int> values = JunctionNodeSnap(tpl, positions);
	REQUIRE(values.size() == tpl.flat.size());
	const JunctionLayout layout = RenderJunctionNodeTracks(tpl, values);
	size_t tiles = 0;
	for (const JunctionTrack &track : layout.tracks) tiles += track.tracks.Any() ? 1 : 0;
	CHECK(tiles == 30);
}

TEST_CASE("a short waiting room is lengthened", "[junction][nodes]")
{
	JunctionSpec spec = MakeTestJunctionSpec();
	spec.tl = 8;
	REQUIRE(FlatT().layout.has_value());
	const JunctionNodeTemplate tpl = BuildJunctionNodeTemplate(*FlatT().layout, spec);
	INFO("template: " << tpl.error);
	REQUIRE(tpl.error.empty());

	const JunctionTerrain terrain = MakeMapTestTerrain(spec);
	const JunctionFacts facts(terrain);
	const JunctionNodeSolve solve = SolveJunctionNodes(tpl, spec, facts, FlatToWorking(spec));
	INFO(FailureText(solve));
	REQUIRE(solve.layout.has_value());

	const std::vector<RuleViolation> violations = CheckJunctionRules(*solve.layout, spec);
	INFO(ViolationText(violations));
	CHECK(std::ranges::none_of(violations, [](const RuleViolation &v) { return v.rule == JunctionRule::WaitingRoom; }));
}

TEST_CASE("slot keeping keeps an interior piece", "[junction][nodes]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	REQUIRE(FlatT().layout.has_value());
	const JunctionLayout &flat = *FlatT().layout;
	const JunctionNodeTemplate tpl = BuildJunctionNodeTemplate(flat, spec);
	REQUIRE(tpl.error.empty());
	const JunctionCheckContext ctx = MakeJunctionCheckContext(flat, spec, FindJunctionPorts(flat), JunctionTMovements());
	const auto used = UsedTracks(ctx);

	/* The first free X run that holds the last slot of a stretch between two switches or bridge pieces. */
	for (size_t pi = 0; pi < ctx.paths.size(); pi++) {
		const std::vector<JunctionPiece> &pieces = ctx.paths[pi].pieces;
		const std::vector<SwitchEvent> events = SwitchEventsOf(flat, used, ctx.paths[pi]);
		const auto is_switch = [&](size_t i) { return std::ranges::any_of(events, [&](const SwitchEvent &e) { return e.second == i; }); };
		std::vector<size_t> anchors;
		for (size_t i = 0; i < pieces.size(); i++) {
			if (pieces[i].on_bridge || is_switch(i)) anchors.push_back(i);
		}
		for (size_t k = 0; k + 1 < anchors.size(); k++) {
			size_t slot = SIZE_MAX;
			for (size_t i = anchors[k + 1]; i-- > anchors[k] + 1;) {
				if (!is_switch(i) && (IsLegalSignalPiece(flat, pieces[i]) || IsSignalledPiece(flat, pieces[i]))) {
					slot = i;
					break;
				}
			}
			if (slot == SIZE_MAX) continue;
			const JunctionNodePath &path = tpl.paths[pi];
			for (size_t j = 0; j + 1 < path.points.size(); j++) {
				const JunctionNodePoint &p = tpl.points[path.points[j]];
				const JunctionNodePoint &q = tpl.points[path.points[j + 1]];
				const auto link = std::ranges::find_if(tpl.links, [&](const JunctionNodeLink &l) { return l.from == path.points[j] && l.to == path.points[j + 1]; });
				const int x = pieces[slot].tile.x;
				if (!link->free || link->diagonal || link->axis != Axis::X || p.tile.y != pieces[slot].tile.y || x <= std::min(p.tile.x, q.tile.x) || x >= std::max(p.tile.x, q.tile.x)) continue;

				WARN("slot " << x << "," << pieces[slot].tile.y << " of path " << pi << " in the run " << p.tile.x << " to " << q.tile.x);
				JunctionPositions positions(tpl.flat.size(), -1024, 1024);
				REQUIRE(AddJunctionNodeConstraints(tpl, positions));
				/* The run shortened to one tile step: d * (x_q - x_p) <= 1. */
				const int off_p = p.tile.x - tpl.flat[p.cls];
				const int off_q = q.tile.x - tpl.flat[q.cls];
				const bool added = path.dir > 0 ? positions.AddDifference(p.cls, q.cls, off_q - off_p - 1) : positions.AddDifference(q.cls, p.cls, off_p - off_q - 1);
				CHECK_FALSE(added);
				return;
			}
		}
	}
	FAIL("no free X run holds a last slot");
}

/** The flat x range of a prio row: its merge's x and span tiles upstream of it. */
static std::pair<int, int> PrioRowRange(const JunctionNodeTemplate &tpl, const JunctionNodePrioRow &prio)
{
	const int x = tpl.points[prio.merge].tile.x;
	const int far = x - prio.span * prio.dir;
	return {std::min(x, far), std::max(x, far)};
}

TEST_CASE("the flat T reserves its two prio rows", "[junction][nodes]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	REQUIRE(FlatT().layout.has_value());
	const JunctionNodeTemplate tpl = BuildJunctionNodeTemplate(*FlatT().layout, spec);
	REQUIRE(tpl.error.empty());
	REQUIRE(tpl.prio_rows.size() == 2);
	for (const JunctionNodePrioRow &prio : tpl.prio_rows) {
		const JunctionNodePoint &merge = tpl.points[prio.merge];
		INFO("merge " << merge.tile.x << "," << merge.tile.y << " side " << static_cast<int>(prio.side) << " dir " << static_cast<int>(prio.dir));
		CHECK(prio.side == -1);
		const auto [lo, hi] = PrioRowRange(tpl, prio);
		if (merge.tile.y + prio.side == 0) {
			CHECK(lo == 24); // The signal pass lays x 24..38 in the flat T.
			CHECK(hi == 38);
		} else {
			CHECK(merge.tile.y + prio.side == 3);
			CHECK(lo == 6);
			CHECK(hi == 20);
		}
	}
}

TEST_CASE("a free diagonal's interior tiles fit the facts", "[junction][nodes]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	REQUIRE(FlatT().layout.has_value());
	const JunctionNodeTemplate tpl = BuildJunctionNodeTemplate(*FlatT().layout, spec);
	REQUIRE(tpl.error.empty());
	REQUIRE(!tpl.diagonals.empty());
	const JunctionNodeDiagonal &diag = tpl.diagonals[0];
	const JunctionNodePoint &p = tpl.points[tpl.links[diag.link].from];
	const JunctionRaster raster = RasterizeJunctionRoute({p.tile, p.trackdir, std::string(static_cast<size_t>(diag.halves - 1), 'F')});
	REQUIRE(raster.error.empty());
	REQUIRE(raster.pieces.size() >= 3);
	const TileIndexDiffC offset = FlatToWorking(spec);
	const int blocked_x = raster.pieces[1].tile.x + offset.x;
	const int blocked_y = raster.pieces[1].tile.y + offset.y;

	JunctionTerrain terrain = MakeMapTestTerrain(spec);
	BlockMapTestTile(terrain, blocked_x, blocked_y);
	const JunctionFacts facts(terrain);

	const JunctionNodeSolve solve = SolveJunctionNodes(tpl, spec, facts, offset);
	INFO(FailureText(solve));
	if (solve.layout.has_value()) {
		for (const JunctionTrack &jt : solve.layout->tracks) {
			INFO("blocked working tile " << blocked_x << "," << blocked_y);
			CHECK(!(jt.offset.x + offset.x == blocked_x && jt.offset.y + offset.y == blocked_y));
		}
	}
}

TEST_CASE("the oracle sits a split on an incline level", "[junction][nodes]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	REQUIRE(FlatT().layout.has_value());
	const JunctionLayout &layout = *FlatT().layout;
	const TileIndexDiffC offset = FlatToWorking(spec);

	/* A split tile of the flat T: the X straight plus a diagonal. */
	const auto split = std::ranges::find_if(layout.tracks, [](const JunctionTrack &jt) { return jt.tracks.Test(Track::X) && jt.tracks.Count() >= 2; });
	REQUIRE(split != layout.tracks.end());
	const TrackBits bits = split->tracks;

	/* Flat land at the top height 1, with the split tile one step lower on one side. */
	JunctionTerrain terrain = MakeMapTestTerrain(spec);
	terrain.build_on_slopes = true;
	for (JunctionTerrainTile &tile : terrain.tiles) tile.z_min = tile.z_max = 1;
	JunctionTerrainTile &tile = terrain.tiles[static_cast<size_t>(split->offset.y + offset.y - terrain.origin.y) * terrain.width + (split->offset.x + offset.x - terrain.origin.x)];
	tile.z_min = 0;
	std::string why;

	/* Inclined along y the X straight lies across the slope: with a diagonal on the high side, the game builds the tile on a leveled foundation at the top height. */
	const auto incline_where = [&](bool valid, std::initializer_list<DiagDirection> inclines) {
		for (const DiagDirection incline : inclines) {
			if ((GetRailFoundation(InclinedSlope(incline), bits) != Foundation::Invalid) != valid) continue;
			tile.incline = incline;
			tile.slope = InclinedSlope(incline);
			return true;
		}
		return false;
	};
	REQUIRE(incline_where(true, {DiagDirection::NW, DiagDirection::SE}));
	CHECK(JunctionNodeHeightsOk(layout, spec, terrain, offset, &why));
	INFO(why);

	/* Inclined along x the X straight runs up the slope, and cannot share its tile with a diagonal: the game refuses it. */
	REQUIRE(incline_where(false, {DiagDirection::NE, DiagDirection::SW}));
	why.clear();
	CHECK_FALSE(JunctionNodeHeightsOk(layout, spec, terrain, offset, &why));
	CHECK(why.find("the game cannot build") != std::string::npos);
}

/** The layout offset of a solved layout as SearchJunctionNodes computes it: the state's offset minus where the anchor pieces moved. */
static TileIndexDiffC SolvedLayoutOffset(const JunctionNodeTemplate &tpl, const JunctionLayout &layout, const JunctionSpec &spec, TileIndexDiffC state_offset)
{
	const auto anchor = std::ranges::find_if(tpl.paths, [](const JunctionNodePath &path) { return path.from == JunctionPortRole::EIn && path.to == JunctionPortRole::SOut; });
	const JunctionCheckContext ctx = MakeJunctionCheckContext(layout, spec, FindJunctionPorts(layout), JunctionTMovements());
	const auto ctx_path = std::ranges::find_if(ctx.paths, [](const JunctionPath &path) { return path.from == JunctionPortRole::EIn && path.to == JunctionPortRole::SOut; });
	REQUIRE(anchor != tpl.paths.end());
	REQUIRE(ctx_path != ctx.paths.end());
	const int shift_x = ctx_path->pieces.back().tile.x - tpl.points[anchor->points.back()].tile.x;
	const int shift_y = ctx_path->pieces.front().tile.y - tpl.points[anchor->points.front()].tile.y;
	return {static_cast<int16_t>(state_offset.x - shift_x), static_cast<int16_t>(state_offset.y - shift_y)};
}

TEST_CASE("a free diagonal over a ramp row is a Height failure", "[junction][nodes]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	REQUIRE(FlatT().layout.has_value());
	const JunctionNodeTemplate tpl = BuildJunctionNodeTemplate(*FlatT().layout, spec);
	REQUIRE(tpl.error.empty());
	JunctionTerrain terrain = MakeMapTestTerrain(spec);
	terrain.build_on_slopes = true;

	/* The ridge test's four rows. */
	const int ridge = spec.ml_gap + 2 + 6;
	const auto set_row = [&](int y, int z_min, int z_max, DiagDirection incline) {
		for (int x = terrain.origin.x; x < terrain.origin.x + terrain.width; x++) {
			JunctionTerrainTile &tile = terrain.tiles[static_cast<size_t>(y - terrain.origin.y) * terrain.width + (x - terrain.origin.x)];
			tile.z_min = static_cast<int8_t>(z_min);
			tile.z_max = static_cast<int8_t>(z_max);
			tile.incline = incline;
			tile.slope = incline == DiagDirection::Invalid ? SLOPE_FLAT : InclinedSlope(incline);
		}
	};
	set_row(ridge, 0, 1, DiagDirection::SE);
	set_row(ridge + 1, 1, 1, DiagDirection::Invalid);
	set_row(ridge + 2, 1, 1, DiagDirection::Invalid);
	set_row(ridge + 3, 0, 1, DiagDirection::NW);
	const JunctionFacts facts(terrain);
	const TileIndexDiffC offset = FlatToWorking(spec);

	const JunctionNodeSolve solve = SolveJunctionNodes(tpl, spec, facts, offset);
	INFO(FailureText(solve));
	CHECK(!solve.layout.has_value());
	REQUIRE(solve.failure.has_value());
	CHECK(solve.failure->kind == JunctionNodeFailure::Kind::Height);
	CHECK(std::ranges::any_of(tpl.diagonals, [&](const JunctionNodeDiagonal &diag) { return tpl.points[tpl.links[diag.link].from].ycls == solve.failure->cls; }));
	REQUIRE(solve.failure->height.has_value());
	const JunctionNodeHeightFault &fault = *solve.failure->height;
	CHECK(fault.part == JunctionNodeHeightFault::Part::Diagonal);
	REQUIRE(fault.index < tpl.diagonals.size());
	const JunctionNodeLink &link = tpl.links[tpl.diagonals[fault.index].link];
	CHECK((tpl.points[link.from].ycls == solve.failure->cls || tpl.points[link.to].ycls == solve.failure->cls));
	REQUIRE(!fault.tiles.empty());
	/* A join that does not meet reports both its tiles, so a flat partner of a ridge-row tile is reported too. */
	const auto on_ridge = [&](const TileIndexDiffC &tile) { return tile.y >= ridge && tile.y <= ridge + 3; };
	CHECK(std::ranges::all_of(fault.tiles, [&](const TileIndexDiffC &tile) {
		return on_ridge(tile) || std::ranges::any_of(fault.tiles, [&](const TileIndexDiffC &other) { return on_ridge(other) && std::abs(other.x - tile.x) + std::abs(other.y - tile.y) == 1; });
	}));
	CHECK(std::ranges::any_of(fault.tiles, [&](const TileIndexDiffC &tile) { return tile.y == ridge || tile.y == ridge + 3; }));
}

TEST_CASE("a Height failure's tiles fail at its row and x", "[junction][nodes]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	REQUIRE(FlatT().layout.has_value());
	const JunctionNodeTemplate tpl = BuildJunctionNodeTemplate(*FlatT().layout, spec);
	REQUIRE(tpl.error.empty());
	JunctionTerrain terrain = MakeMapTestTerrain(spec);
	terrain.build_on_slopes = true;

	const int ridge = spec.ml_gap + 2 + 6;
	const auto set_row = [&](int y, int z_min, int z_max, DiagDirection incline) {
		for (int x = terrain.origin.x; x < terrain.origin.x + terrain.width; x++) {
			JunctionTerrainTile &tile = terrain.tiles[static_cast<size_t>(y - terrain.origin.y) * terrain.width + (x - terrain.origin.x)];
			tile.z_min = static_cast<int8_t>(z_min);
			tile.z_max = static_cast<int8_t>(z_max);
			tile.incline = incline;
			tile.slope = incline == DiagDirection::Invalid ? SLOPE_FLAT : InclinedSlope(incline);
		}
	};
	set_row(ridge, 0, 1, DiagDirection::SE);
	set_row(ridge + 1, 1, 1, DiagDirection::Invalid);
	set_row(ridge + 2, 1, 1, DiagDirection::Invalid);
	set_row(ridge + 3, 0, 1, DiagDirection::NW);
	const JunctionFacts facts(terrain);

	const JunctionNodeSolve solve = SolveJunctionNodes(tpl, spec, facts, FlatToWorking(spec));
	INFO(FailureText(solve));
	REQUIRE(solve.failure.has_value());
	REQUIRE(solve.failure->height.has_value());
	REQUIRE(!solve.failure->height->tiles.empty());

	/* A cheap consistency check; the solve's own collector is the authority. */
	const auto &tiles = solve.failure->height->tiles;
	const auto on_ramp = [&](const TileIndexDiffC &tile) {
		const JunctionTerrainTile &t = terrain.tiles[static_cast<size_t>(tile.y - terrain.origin.y) * terrain.width + (tile.x - terrain.origin.x)];
		return t.z_min != t.z_max;
	};
	for (const TileIndexDiffC &tile : tiles) {
		const bool unfit = !facts.Fits(tile.x, tile.y, TrackBits{Track::X}) && !facts.Fits(tile.x, tile.y, TrackBits{Track::Y});
		const bool partner = std::ranges::any_of(tiles, [&](const TileIndexDiffC &other) { return on_ramp(other) && std::abs(other.x - tile.x) + std::abs(other.y - tile.y) == 1; });
		CHECK((unfit || on_ramp(tile) || partner));
	}
}

TEST_CASE("a split whose foundation does not meet its run moves", "[junction][nodes]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	REQUIRE(FlatT().layout.has_value());
	const JunctionLayout &flat = *FlatT().layout;
	const JunctionNodeTemplate tpl = BuildJunctionNodeTemplate(flat, spec);
	REQUIRE(tpl.error.empty());
	const TileIndexDiffC offset = FlatToWorking(spec);

	/* The first free X link whose first point sits on a tile with two or more tracks. */
	std::optional<JunctionNodePoint> p;
	TrackBits bits;
	for (const JunctionNodeLink &link : tpl.links) {
		if (!link.free || link.diagonal || link.axis != Axis::X) continue;
		const JunctionNodePoint &from = tpl.points[link.from];
		const auto tile = std::ranges::find_if(flat.tracks, [&](const JunctionTrack &jt) { return jt.offset.x == from.tile.x && jt.offset.y == from.tile.y; });
		if (tile == flat.tracks.end() || tile->tracks.Count() < 2) continue;
		p = from;
		bits = tile->tracks;
		break;
	}
	REQUIRE(p.has_value());
	const int px = p->tile.x + offset.x;
	const int py = p->tile.y + offset.y;
	const DiagDirection edge = TrackdirToExitdir(p->trackdir);
	const TileIndexDiffC step = TileIndexDiffCByDiagDir(edge);

	/* A slope on which the game builds p's tracks but at the edge toward the run is not at the flat run's height. */
	JunctionTerrain terrain = MakeMapTestTerrain(spec);
	terrain.build_on_slopes = true;
	JunctionTerrainTile &tile = terrain.tiles[static_cast<size_t>(py - terrain.origin.y) * terrain.width + (px - terrain.origin.x)];
	bool found = false;
	for (int s = 1; s <= 15 && !found; s++) {
		tile.slope = static_cast<Slope>(s);
		tile.z_min = 0;
		tile.z_max = 1;
		const JunctionFacts facts(terrain);
		const int z_here = facts.EdgeZ(px, py, bits, edge);
		const int z_run = facts.EdgeZ(px + step.x, py + step.y, TrackBits{Track::X}, ReverseDiagDir(edge));
		found = facts.Fits(px, py, bits) && z_here != INT_MIN && z_run != INT_MIN && z_here != z_run;
	}
	REQUIRE(found);

	const JunctionFacts facts(terrain);
	const JunctionNodeSolve solve = SolveJunctionNodes(tpl, spec, facts, offset);
	INFO(FailureText(solve));
	if (solve.layout.has_value()) {
		std::string why;
		CHECK(JunctionNodeHeightsOk(*solve.layout, spec, terrain, SolvedLayoutOffset(tpl, *solve.layout, spec, offset), &why));
		INFO(why);
	}
}

TEST_CASE("a blocked prio row moves its merge", "[junction][nodes]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	REQUIRE(FlatT().layout.has_value());
	const JunctionNodeTemplate tpl = BuildJunctionNodeTemplate(*FlatT().layout, spec);
	REQUIRE(tpl.error.empty());
	REQUIRE(!tpl.prio_rows.empty());
	const TileIndexDiffC offset = FlatToWorking(spec);

	/* A tile inside the first merge's reserved prio row at the flat solution: three tiles upstream of the merge. */
	const JunctionNodePrioRow &prio = tpl.prio_rows[0];
	const JunctionNodePoint &merge = tpl.points[prio.merge];
	const int bx = merge.tile.x - 3 * prio.dir + offset.x;
	const int by = merge.tile.y + prio.side + offset.y;
	JunctionTerrain terrain = MakeMapTestTerrain(spec);
	BlockMapTestTile(terrain, bx, by);
	const JunctionFacts facts(terrain);

	const JunctionNodeSolve solve = SolveJunctionNodes(tpl, spec, facts, offset);
	INFO(FailureText(solve));
	if (solve.layout.has_value()) {
		const TileIndexDiffC placed = SolvedLayoutOffset(tpl, *solve.layout, spec, offset);
		for (const JunctionTrack &jt : solve.layout->tracks) {
			CHECK_FALSE((jt.offset.x + placed.x == bx && jt.offset.y + placed.y == by));
		}
	}
}

/** A different value of the same type: toggled, next enum value, +1, or an appended char. */
template <typename T>
static void ChangeNodeValue(T &value)
{
	if constexpr (std::is_enum_v<T>) {
		value = static_cast<T>(to_underlying(value) + 1);
	} else if constexpr (std::is_same_v<T, bool>) {
		value = !value;
	} else if constexpr (std::is_same_v<T, std::string>) {
		value += 'x';
	} else {
		value = static_cast<T>(value + 1);
	}
}

/** Check that `mutated` differs from `tpl` with a diff starting with `prefix`. */
static void CheckNodeDiff(const JunctionNodeTemplate &tpl, const JunctionNodeTemplate &mutated, const std::string &prefix)
{
	const std::string diff = DiffJunctionNodeTemplates(tpl, mutated);
	INFO(prefix << " -> " << diff);
	CHECK(!diff.empty());
	CHECK(diff.starts_with(prefix));
}

/** Check a template vector member by size (one element removed, or one default element added when it is empty in `tpl`). */
template <typename V>
static void CheckNodeSize(const JunctionNodeTemplate &tpl, V JunctionNodeTemplate::*member, const char *name)
{
	JunctionNodeTemplate copy = tpl;
	if ((copy.*member).empty()) {
		(copy.*member).emplace_back();
	} else {
		(copy.*member).pop_back();
	}
	CheckNodeDiff(tpl, copy, fmt::format("{}: size", name));
}

/** Check each listed field of the first element of a member: `fields` pairs a field name with a mutation of the element. */
template <typename V, typename... Fs>
static void CheckNodeFields(const JunctionNodeTemplate &tpl, V JunctionNodeTemplate::*member, const char *name, std::pair<const char *, Fs>... fields)
{
	CheckNodeSize(tpl, member, name);
	if ((tpl.*member).empty()) {
		WARN("empty in FlatT, size only: " << name);
		return;
	}
	const auto one = [&](const char *field, auto mutate) {
		JunctionNodeTemplate copy = tpl;
		mutate((copy.*member).front());
		CheckNodeDiff(tpl, copy, fmt::format("{}[0].{}", name, field));
	};
	(one(fields.first, fields.second), ...);
}

/** A field mutation, written `FIELD(tile)` for a plain field or `FIELD2(bridge, tunnel)` for a nested one. */
#define FIELD(f) std::pair{#f, [](auto &e) { ChangeNodeValue(e.f); }}
#define FIELD2(f, g) std::pair{#f "." #g, [](auto &e) { ChangeNodeValue(e.f.g); }}
#define VEC(f) std::pair{#f, [](auto &e) { e.f.pop_back(); }}

TEST_CASE("the template comparator finds every member", "[junction][nodes]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	REQUIRE(FlatT().layout.has_value());
	const JunctionNodeTemplate tpl = BuildJunctionNodeTemplate(*FlatT().layout, spec);
	REQUIRE(tpl.error.empty());
	CHECK(DiffJunctionNodeTemplates(tpl, tpl).empty());

	using N = JunctionNodeTemplate;
	CheckNodeFields(tpl, &N::points, "points", FIELD2(tile, x), FIELD2(tile, y), FIELD(trackdir), FIELD(cls), FIELD(ycls));
	CheckNodeFields(tpl, &N::links, "links", FIELD(from), FIELD(to), FIELD(free), FIELD(axis), FIELD(diagonal));
	CheckNodeFields(tpl, &N::paths, "paths", FIELD(from), FIELD(to), VEC(points), VEC(units), FIELD(dir), FIELD(dir_y));
	CheckNodeFields(tpl, &N::pieces, "pieces", FIELD2(tile, x), FIELD2(tile, y), FIELD(track), FIELD(on_bridge), FIELD(cls), FIELD(ycls));
	CheckNodeFields(tpl, &N::bridges, "bridges", FIELD2(bridge, start.x), FIELD2(bridge, start.y), FIELD2(bridge, end.x), FIELD2(bridge, end.y), FIELD2(bridge, tunnel), FIELD2(bridge, z), FIELD(cls), FIELD(ycls));
	CheckNodeFields(tpl, &N::joins, "joins", FIELD2(from_tile, x), FIELD2(from_tile, y), FIELD(from_td), FIELD2(to_tile, x), FIELD2(to_tile, y), FIELD(cls), FIELD(ycls));
	CheckNodeFields(tpl, &N::diffs, "diffs", FIELD(a), FIELD(b), FIELD(k));
	CheckNodeFields(tpl, &N::diagonals, "diagonals", FIELD(link), FIELD(dx), FIELD(dy), FIELD(halves));
	CheckNodeFields(tpl, &N::late_gaps, "late_gaps", FIELD(path), FIELD(p), FIELD(q), FIELD(g), FIELD(u), VEC(diagonals), FIELD(dir), VEC(y_links));
	CheckNodeFields(tpl, &N::late_crossings, "late_crossings", FIELD(bridge), FIELD(diagonal));
	CheckNodeFields(tpl, &N::prio_rows, "prio_rows", FIELD(merge), FIELD(side), FIELD(dir), FIELD(span));
	CheckNodeSize(tpl, &N::flat, "flat");
	CheckNodeSize(tpl, &N::lean, "lean");

	const auto scalar = [&](const char *name, auto mutate) {
		N copy = tpl;
		mutate(copy);
		CheckNodeDiff(tpl, copy, name);
	};
	scalar("open_joins", [](N &t) { ChangeNodeValue(t.open_joins); });
	scalar("flat[0]", [](N &t) { ChangeNodeValue(t.flat.front()); });
	scalar("lean[0]", [](N &t) { ChangeNodeValue(t.lean.front()); });
	scalar("x_classes", [](N &t) { ChangeNodeValue(t.x_classes); });
	scalar("anchor", [](N &t) { ChangeNodeValue(t.anchor); });
	scalar("anchor_y", [](N &t) { ChangeNodeValue(t.anchor_y); });
	scalar("error", [](N &t) { ChangeNodeValue(t.error); });
}

TEST_CASE("the traced T is a fixed point", "[junction][nodes]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	REQUIRE(FlatT().layout.has_value());
	const JunctionNodeTemplate tpl = BuildJunctionNodeTemplate(*FlatT().layout, spec);
	REQUIRE(tpl.error.empty());
	const JunctionTerrain terrain = MakeMapTestTerrain(spec);
	const JunctionFacts facts(terrain);
	const JunctionNodeSolve solve = SolveJunctionNodes(tpl, spec, facts, FlatToWorking(spec));
	REQUIRE(solve.layout.has_value());
	const JunctionNodeTemplate rebuilt = BuildJunctionNodeTemplate(*solve.layout, spec);
	const std::string diff = DiffJunctionNodeTemplates(tpl, rebuilt);
	INFO(diff);
	CHECK(diff == "");
}

TEST_CASE("the template from a traced input equals the template from its layout", "[junction][nodes]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const auto same = [&](const JunctionLayout &layout) {
		const std::string diff = DiffJunctionNodeTemplates(BuildJunctionNodeTemplate(layout, spec), BuildJunctionNodeTemplate(TraceJunctionNodeInput(layout, spec), spec));
		INFO(diff);
		CHECK(diff.empty());
	};
	REQUIRE(FlatT().layout.has_value());
	same(*FlatT().layout);

	/* The layouts of "a detour around a column of the hand-built line": up and down. */
	const DetourFixture fixture(2, 5);
	const JunctionSpec test_spec = MakeTestJunctionSpec();
	std::string why;
	const std::optional<JunctionLayout> up = InsertJunctionDetour(fixture.layout, fixture.tpl, fixture.link, 7, -1, test_spec, &why);
	const std::optional<JunctionLayout> down = InsertJunctionDetour(fixture.layout, fixture.tpl, fixture.link, 7, 1, test_spec, &why);
	REQUIRE(up.has_value());
	REQUIRE(down.has_value());
	same(*up);
	same(*down);
}

TEST_CASE("the template input holds no layout", "[junction][nodes]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	REQUIRE(FlatT().layout.has_value());
	const JunctionNodeTemplate expected = BuildJunctionNodeTemplate(*FlatT().layout, spec);
	std::optional<JunctionNodeInput> input;
	{
		const JunctionLayout local = *FlatT().layout;
		input = TraceJunctionNodeInput(local, spec);
	}
	const std::string diff = DiffJunctionNodeTemplates(expected, BuildJunctionNodeTemplate(*input, spec));
	INFO(diff);
	CHECK(diff.empty());
}
