/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_router.cpp Test functionality from junction_router. */

#include "../stdafx.h"

#include "../3rdparty/catch2/catch.hpp"

#include "../direction_func.h"
#include "../junction_generator.h"
#include "../junction_route.h"
#include "../junction_router.h"
#include "../junction_rules.h"
#include "../junction_terrain.h"
#include "../track_func.h"

#include "../safeguards.h"

using Role = JunctionPortRole;

/** A request with the test spec's grammar and prices; the caller sets ends and bounds. */
static JunctionRouteRequest MakeRequest(TileIndexDiffC min, TileIndexDiffC max)
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	JunctionRouteRequest request{};
	request.min = min;
	request.max = max;
	request.curve_spacing = DeriveCurveSpacing(spec.reference_speed, spec.curve_speed, spec.tl);
	request.s_bend_window = 2u * spec.tl;
	request.bridge_span_ok = spec.bridge_span_ok;
	request.max_bridge_middle = 10;
	request.prices = spec.prices;
	request.crossable_cost = 0;
	return request;
}

static JunctionLayout WallLayout(int x, int y0, int y1)
{
	JunctionLayout layout;
	for (int y = y0; y <= y1; y++) layout.tracks.push_back({{static_cast<int16_t>(x), static_cast<int16_t>(y)}, TrackBits{Track::Y}});
	return layout;
}

static JunctionLayout RowLayout(int x0, int x1)
{
	JunctionLayout layout;
	for (int x = x0; x <= x1; x++) layout.tracks.push_back({{static_cast<int16_t>(x), 0}, TrackBits{Track::X}});
	return layout;
}

/** Check one route from X_SW at the start of a path of its own, with the given reference speed. */
static std::vector<RuleViolation> CurveViolations(const char *moves, uint16_t reference_speed)
{
	JunctionSpec spec = MakeTestJunctionSpec();
	spec.reference_speed = reference_speed;
	const JunctionRaster raster = RasterizeJunctionRoute({{0, 0}, Trackdir::X_SW, moves});
	REQUIRE(raster.error.empty());
	JunctionLayout layout;
	AddRasterToLayout(layout, raster);
	NormaliseJunctionLayout(layout);
	const JunctionPiece &last = raster.pieces.back();
	const JunctionCheckContext ctx = MakeJunctionCheckContext(layout, spec,
			{{Role::EIn, {0, 0}, Trackdir::X_SW}, {Role::EOut, last.tile, last.trackdir}}, {{Role::EIn, Role::EOut}});
	REQUIRE(ctx.paths.size() == 1);
	return CheckCurveSpeed(ctx);
}

static bool IsTurn(Trackdir a, Trackdir b)
{
	return TrackdirToDirection(a) != TrackdirToDirection(b);
}

/** The turns of a raster as (index of the piece after the turn, hand, units since the previous turn or -1). */
struct TurnInfo {
	size_t index;
	bool left;
	int gap;
};

static std::vector<TurnInfo> Turns(const JunctionRaster &raster)
{
	std::vector<TurnInfo> turns;
	int units = 0;
	bool any = false;
	for (size_t i = 1; i < raster.pieces.size(); i++) {
		const JunctionPiece &prev = raster.pieces[i - 1];
		const JunctionPiece &cur = raster.pieces[i];
		units += prev.units;
		if (!cur.on_bridge && !prev.on_bridge && IsTurn(prev.trackdir, cur.trackdir)) {
			const bool left = ChangeDir(TrackdirToDirection(prev.trackdir), DirDiff::Left45) == TrackdirToDirection(cur.trackdir);
			turns.push_back({i, left, any ? units : -1});
			units = 0;
			any = true;
		}
	}
	return turns;
}

static bool HasBridge(const std::string &moves)
{
	return moves.find('B') != std::string::npos;
}

TEST_CASE("FindJunctionRoute: straight", "[junction][router]")
{
	JunctionRouteRequest request = MakeRequest({0, 0}, {12, 12});
	request.starts = {{{0, 0}, Trackdir::X_SW}};
	request.goals = {{{6, 0}, Trackdir::X_SW}};
	const auto route = FindJunctionRoute({}, request);
	REQUIRE(route.has_value());
	CHECK(route->moves == "FFFFFF");
}

TEST_CASE("FindJunctionRoute: turn", "[junction][router]")
{
	JunctionRouteRequest request = MakeRequest({0, 0}, {12, 12});
	request.starts = {{{0, 0}, Trackdir::X_SW}};
	request.goals = {{{6, 6}, Trackdir::Y_SE}};
	const auto route = FindJunctionRoute({}, request);
	REQUIRE(route.has_value());
	INFO(route->moves);
	const JunctionRaster raster = RasterizeJunctionRoute(*route);
	REQUIRE(raster.error.empty());
	CHECK(raster.pieces.back().tile.x == 6);
	CHECK(raster.pieces.back().tile.y == 6);
	CHECK(raster.pieces.back().trackdir == Trackdir::Y_SE);
	CHECK(CurveViolations(route->moves.c_str(), 160).empty());
	const auto turns = Turns(raster);
	for (size_t i = 1; i < turns.size(); i++) {
		if (turns[i].left == turns[i - 1].left) CHECK(turns[i].gap >= static_cast<int>(request.curve_spacing));
	}
}

TEST_CASE("FindJunctionRoute: obstacle", "[junction][router]")
{
	const JunctionLayout wall = WallLayout(5, 0, 12);
	JunctionRouteRequest request = MakeRequest({0, 0}, {12, 12});
	request.starts = {{{0, 6}, Trackdir::X_SW}};
	request.goals = {{{10, 6}, Trackdir::X_SW}};
	auto route = FindJunctionRoute(wall, request);
	REQUIRE(route.has_value());
	INFO(route->moves);
	CHECK(HasBridge(route->moves));
	CHECK(RasterizeJunctionRoute(*route).error.empty());

	request = MakeRequest({0, 0}, {12, 20});
	request.starts = {{{0, 6}, Trackdir::X_SW}};
	request.goals = {{{10, 6}, Trackdir::X_SW}};
	request.prices.bridge = [](uint) { return Money(1000000000); };
	route = FindJunctionRoute(wall, request);
	REQUIRE(route.has_value());
	INFO(route->moves);
	CHECK_FALSE(HasBridge(route->moves));
}

TEST_CASE("FindJunctionRoute: the track beyond a far head runs in line", "[junction][router]")
{
	/* The start leaves no room to shift rows before the wall. Goal (8,7) is reachable only by turning off the far head, so it must have no route; goal (12,7) is reachable in line. */
	const JunctionLayout wall = WallLayout(5, 0, 12);
	auto check_in_line = [](const JunctionRoute &route) {
		const JunctionRaster raster = RasterizeJunctionRoute(route);
		REQUIRE(raster.error.empty());
		for (size_t i = 0; i + 1 < raster.pieces.size(); i++) {
			if (raster.pieces[i].on_bridge && !raster.pieces[i + 1].on_bridge) CHECK(raster.pieces[i + 1].trackdir == raster.pieces[i].trackdir);
		}
	};
	JunctionRouteRequest request = MakeRequest({0, 0}, {14, 12});
	request.starts = {{{3, 6}, Trackdir::X_SW}};
	request.goals = {{{8, 7}, Trackdir::X_SW}};
	auto route = FindJunctionRoute(wall, request);
	if (route.has_value()) {
		INFO(route->moves);
		check_in_line(*route);
	}

	request.goals = {{{12, 7}, Trackdir::X_SW}};
	route = FindJunctionRoute(wall, request);
	REQUIRE(route.has_value());
	INFO(route->moves);
	check_in_line(*route);
}

TEST_CASE("FindJunctionRoute: crossable", "[junction][router]")
{
	const JunctionLayout wall = WallLayout(5, 0, 12);
	JunctionRouteRequest request = MakeRequest({0, 0}, {12, 12});
	request.starts = {{{0, 6}, Trackdir::X_SW}};
	request.goals = {{{10, 6}, Trackdir::X_SW}};
	for (int y = 0; y <= 12; y++) request.crossable.push_back({5, static_cast<int16_t>(y)});
	const auto route = FindJunctionRoute(wall, request);
	REQUIRE(route.has_value());
	INFO(route->moves);
	CHECK_FALSE(HasBridge(route->moves));
	const JunctionRaster raster = RasterizeJunctionRoute(*route);
	REQUIRE(raster.error.empty());
	CHECK(std::ranges::any_of(raster.pieces, [](const JunctionPiece &p) { return p.tile.x == 5 && !p.on_bridge; }));
}

TEST_CASE("FindJunctionRoute: split start", "[junction][router]")
{
	const JunctionLayout row = RowLayout(0, 10);
	JunctionRouteRequest request = MakeRequest({0, 0}, {12, 12});
	request.starts = {{{3, 0}, Trackdir::X_SW}};
	request.goals = {{{8, 6}, Trackdir::Y_SE}};
	const auto route = FindJunctionRoute(row, request);
	REQUIRE(route.has_value());
	INFO(route->moves);
	REQUIRE_FALSE(route->moves.empty());
	CHECK((route->moves[0] == 'L' || route->moves[0] == 'R'));
	const JunctionRaster raster = RasterizeJunctionRoute(*route);
	REQUIRE(raster.error.empty());
	REQUIRE(raster.pieces.size() >= 2);
	const JunctionPiece &first = raster.pieces[1];
	CHECK(std::ranges::any_of(row.tracks, [&](const JunctionTrack &jt) {
		return jt.offset.x == first.tile.x && jt.offset.y == first.tile.y && jt.tracks.Test(Track::X);
	}));
}

TEST_CASE("FindJunctionRoute: merge goal", "[junction][router]")
{
	const JunctionLayout row = RowLayout(0, 20);
	JunctionRouteRequest request = MakeRequest({0, 0}, {20, 10});
	request.starts = {{{2, 8}, Trackdir::Y_NW}};
	for (int x = 0; x <= 20; x++) request.goals.push_back({{static_cast<int16_t>(x), 0}, Trackdir::X_SW});
	const auto route = FindJunctionRoute(row, request);
	REQUIRE(route.has_value());
	INFO(route->moves);
	const JunctionRaster raster = RasterizeJunctionRoute(*route);
	REQUIRE(raster.error.empty());
	REQUIRE(raster.pieces.size() >= 2);
	CHECK(raster.pieces.back().tile.y == 0);
	const JunctionPiece &second_last = raster.pieces[raster.pieces.size() - 2];
	CHECK(std::ranges::any_of(row.tracks, [&](const JunctionTrack &jt) {
		return jt.offset.x == second_last.tile.x && jt.offset.y == second_last.tile.y && jt.tracks.Test(Track::X);
	}));
}

TEST_CASE("FindJunctionRoute: unreachable", "[junction][router]")
{
	/* A goal at (6,6) ringed by Y tracks on the four sides' tiles. */
	JunctionLayout ring;
	for (int d = -1; d <= 1; d++) {
		for (int e = -1; e <= 1; e++) {
			if (d == 0 && e == 0) continue;
			ring.tracks.push_back({{static_cast<int16_t>(6 + d), static_cast<int16_t>(6 + e)}, TrackBits{Track::Y}});
		}
	}
	JunctionRouteRequest request = MakeRequest({0, 0}, {12, 12});
	request.starts = {{{0, 6}, Trackdir::X_SW}};
	request.goals = {{{6, 6}, Trackdir::X_SW}};
	request.bridge_span_ok = [](uint) { return false; };
	CHECK_FALSE(FindJunctionRoute(ring, request).has_value());
}

TEST_CASE("FindJunctionRoute: deterministic", "[junction][router]")
{
	JunctionRouteRequest request = MakeRequest({0, 0}, {12, 12});
	request.starts = {{{0, 0}, Trackdir::X_SW}};
	request.goals = {{{6, 6}, Trackdir::Y_SE}};
	const auto a = FindJunctionRoute({}, request);
	const auto b = FindJunctionRoute({}, request);
	REQUIRE(a.has_value());
	REQUIRE(b.has_value());
	CHECK(a->moves == b->moves);
	CHECK(a->start.x == b->start.x);
	CHECK(a->start.y == b->start.y);
}

TEST_CASE("FindJunctionRoute: S-bend grammar", "[junction][router]")
{
	JunctionRouteRequest request = MakeRequest({0, 0}, {12, 12});
	request.starts = {{{0, 0}, Trackdir::X_SW}};
	request.goals = {{{12, 1}, Trackdir::X_SW}};
	const auto route = FindJunctionRoute({}, request);
	REQUIRE(route.has_value());
	INFO(route->moves);
	const JunctionRaster raster = RasterizeJunctionRoute(*route);
	REQUIRE(raster.error.empty());
	const auto turns = Turns(raster);
	size_t tight = 0;
	for (size_t i = 1; i < turns.size(); i++) {
		if (turns[i].left == turns[i - 1].left || turns[i].gap >= static_cast<int>(request.curve_spacing)) continue;
		tight++;
		if (i + 1 < turns.size()) CHECK(turns[i + 1].gap >= static_cast<int>(request.s_bend_window));
	}
	CHECK(tight == 1);
}

/** A flat Buildable grid at height z covering the bounds, with foundation_cost 250. */
static JunctionTerrain FlatTerrain(TileIndexDiffC min, TileIndexDiffC max, int8_t z)
{
	JunctionTerrain terrain;
	terrain.origin = min;
	terrain.width = max.x - min.x + 1;
	terrain.height = max.y - min.y + 1;
	JunctionTerrainTile tile;
	tile.z_min = tile.z_max = z;
	terrain.tiles.assign(static_cast<size_t>(terrain.width) * terrain.height, tile);
	terrain.foundation_cost = 250;
	return terrain;
}

static JunctionTerrainTile &TerrainTileAt(JunctionTerrain &terrain, int x, int y)
{
	return terrain.tiles[static_cast<size_t>(y - terrain.origin.y) * terrain.width + (x - terrain.origin.x)];
}

/** Whether any non-bridge piece of the route lies in the given inclusive tile rectangle. */
static bool RouteTouches(const JunctionRoute &route, int x0, int y0, int x1, int y1)
{
	for (const JunctionPiece &piece : RasterizeJunctionRoute(route).pieces) {
		if (piece.on_bridge) continue;
		if (piece.tile.x >= x0 && piece.tile.x <= x1 && piece.tile.y >= y0 && piece.tile.y <= y1) return true;
	}
	return false;
}

TEST_CASE("FindJunctionRoute: a hill at the plane's level is detoured", "[junction][router][terrain]")
{
	JunctionTerrain terrain = FlatTerrain({0, 0}, {12, 12}, 0);
	for (int x = 4; x <= 6; x++) {
		for (int y = 5; y <= 7; y++) TerrainTileAt(terrain, x, y).z_min = TerrainTileAt(terrain, x, y).z_max = 2;
	}
	JunctionRouteRequest request = MakeRequest({0, 0}, {12, 12});
	request.starts = {{{0, 6}, Trackdir::X_SW}};
	request.goals = {{{10, 6}, Trackdir::X_SW}};
	request.terrain = &terrain;
	request.plane = 0;
	const auto route = FindJunctionRoute({}, request);
	REQUIRE(route.has_value());
	INFO(route->moves);
	CHECK_FALSE(HasBridge(route->moves));
	CHECK_FALSE(RouteTouches(*route, 4, 5, 6, 7));
	CHECK(route->moves != "FFFFFFFFFF");
}

TEST_CASE("FindJunctionRoute: a leveled foundation is paid for", "[junction][router][terrain]")
{
	JunctionTerrain terrain = FlatTerrain({0, 0}, {12, 12}, 1);
	JunctionTerrainTile &slope = TerrainTileAt(terrain, 5, 6);
	slope.z_min = 0;
	slope.z_max = 1;
	JunctionRouteRequest request = MakeRequest({0, 6}, {12, 6});
	request.starts = {{{0, 6}, Trackdir::X_SW}};
	request.goals = {{{10, 6}, Trackdir::X_SW}};
	request.terrain = &terrain;
	request.plane = 1;

	/* A single row leaves no detour: the leveled tile is usable at the plane. */
	auto route = FindJunctionRoute({}, request);
	REQUIRE(route.has_value());
	INFO(route->moves);
	CHECK(route->moves == "FFFFFFFFFF");
	CHECK(RouteTouches(*route, 5, 6, 5, 6));

	/* With room to detour, 2 extra pieces (200) are cheaper than the 250 foundation, so the route avoids the tile: the foundation is charged. */
	request.min = {0, 0};
	request.max = {12, 12};
	route = FindJunctionRoute({}, request);
	REQUIRE(route.has_value());
	INFO(route->moves);
	CHECK_FALSE(RouteTouches(*route, 5, 6, 5, 6));

	terrain.foundation_cost = 1000000000;
	route = FindJunctionRoute({}, request);
	REQUIRE(route.has_value());
	CHECK_FALSE(RouteTouches(*route, 5, 6, 5, 6));

	terrain.build_on_slopes = false;
	terrain.foundation_cost = 250;
	route = FindJunctionRoute({}, request);
	REQUIRE(route.has_value());
	CHECK_FALSE(RouteTouches(*route, 5, 6, 5, 6));
}

TEST_CASE("FindJunctionRoute: houses block and are not spanned", "[junction][router][terrain]")
{
	JunctionTerrain terrain = FlatTerrain({0, 0}, {12, 12}, 0);
	for (int y = 0; y <= 12; y++) TerrainTileAt(terrain, 5, y).cls = JunctionTileClass::Blocked;
	JunctionRouteRequest request = MakeRequest({0, 0}, {12, 12});
	request.starts = {{{0, 6}, Trackdir::X_SW}};
	request.goals = {{{10, 6}, Trackdir::X_SW}};
	request.terrain = &terrain;
	request.plane = 0;
	CHECK_FALSE(FindJunctionRoute({}, request).has_value());

	/* A gap in the column gives a detour that avoids every Blocked tile. */
	TerrainTileAt(terrain, 5, 4).cls = JunctionTileClass::Buildable;
	const auto route = FindJunctionRoute({}, request);
	REQUIRE(route.has_value());
	INFO(route->moves);
	CHECK_FALSE(HasBridge(route->moves));
	CHECK_FALSE(RouteTouches(*route, 5, 0, 5, 3));
	CHECK_FALSE(RouteTouches(*route, 5, 5, 5, 12));
}

TEST_CASE("FindJunctionRoute: water is crossed only by a span", "[junction][router][terrain]")
{
	JunctionTerrain terrain = FlatTerrain({0, 0}, {12, 12}, 0);
	for (int y = 0; y <= 12; y++) TerrainTileAt(terrain, 5, y).cls = JunctionTileClass::Water;
	JunctionRouteRequest request = MakeRequest({0, 0}, {12, 12});
	request.starts = {{{0, 6}, Trackdir::X_SW}};
	request.goals = {{{10, 6}, Trackdir::X_SW}};
	request.terrain = &terrain;
	request.plane = 0;
	auto route = FindJunctionRoute({}, request);
	REQUIRE(route.has_value());
	INFO(route->moves);
	CHECK(HasBridge(route->moves));
	CHECK_FALSE(RouteTouches(*route, 5, 0, 5, 12));

	request.bridge_span_ok = [](uint) { return false; };
	CHECK_FALSE(FindJunctionRoute({}, request).has_value());
}
