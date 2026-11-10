/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_line_router.cpp Test functionality from junction_line_router. */

#include "../stdafx.h"

#include "../3rdparty/catch2/catch.hpp"

#include "../junction_layout.h"
#include "../junction_line_router.h"
#include "../direction_func.h"
#include "../map_func.h"
#include "../slope_func.h"
#include "../track_func.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <map>
#include <set>
#include <string>

#include "../safeguards.h"

/** The exit point of a piece, in half-tile units: the middle of the edge the piece leaves through. */
static std::pair<int, int> ExitPoint(const JunctionPiece &piece)
{
	const int x = piece.tile.x;
	const int y = piece.tile.y;
	switch (TrackdirToExitdir(piece.trackdir)) {
		case DiagDirection::NE: return {2 * x, 2 * y + 1};
		case DiagDirection::SE: return {2 * x + 1, 2 * y + 2};
		case DiagDirection::SW: return {2 * x + 2, 2 * y + 1};
		case DiagDirection::NW: return {2 * x + 1, 2 * y};
		default: NOT_REACHED();
	}
}

/** A request for a {2, 1, 1} line heading along -x from (40, 20) to (20, 20), in a field with room to go round. */
static JunctionLineRouteRequest BaseRequest()
{
	JunctionLineRouteRequest request;
	request.line = {2, 1, 1};
	request.start = {40, 20};
	request.start_trackdir = Trackdir::X_NE;
	request.goals = {{{20, 20}, Trackdir::X_NE}};
	request.min = {10, 5};
	request.max = {50, 40};
	request.curve_spacing = 4;
	request.turn_cost = 3;
	return request;
}

static std::vector<JunctionRaster> CheckRoute(const JunctionLineRouteRequest &request, const JunctionLineRouteResult &result)
{
	INFO(result.error);
	REQUIRE(result.route.has_value());
	const std::vector<JunctionRaster> rasters = RasterizeJunctionLine(request.line, *result.route);
	REQUIRE(rasters.size() == request.line.tracks);
	for (const JunctionRaster &raster : rasters) {
		INFO(raster.error);
		REQUIRE(raster.error.empty());
	}

	std::set<std::pair<int, int>> blocked;
	for (const TileIndexDiffC &tile : request.blocked) blocked.emplace(tile.x, tile.y);
	std::map<std::pair<int, int>, TrackBits> tiles;
	for (const JunctionRaster &raster : rasters) {
		for (const JunctionPiece &piece : raster.pieces) {
			CHECK_FALSE(blocked.contains({piece.tile.x, piece.tile.y}));
			CHECK(piece.tile.x >= request.min.x);
			CHECK(piece.tile.y >= request.min.y);
			CHECK(piece.tile.x <= request.max.x);
			CHECK(piece.tile.y <= request.max.y);
			TrackBits &bits = tiles[{piece.tile.x, piece.tile.y}];
			const Track track = TrackdirToTrack(piece.trackdir);
			REQUIRE_FALSE(bits.Test(track));
			bits.Set(track);
			REQUIRE_FALSE(TracksOverlap(bits));
		}
	}
	const JunctionPiece &end = rasters[0].pieces.back();
	CHECK(std::ranges::any_of(request.goals, [&](const JunctionRouteEnd &goal) {
		return goal.tile.x == end.tile.x && goal.tile.y == end.tile.y && goal.trackdir == end.trackdir;
	}));
	return rasters;
}

/** Whether every turn of a track follows the curve rule with spacing and window both equal to spacing: a full run before a turn, or the second turn of an opposite pair whose first turn had a full run before it. */
static bool TurnsSpaced(const JunctionRaster &raster, uint spacing)
{
	uint units = spacing; // The start counts as a full run.
	int prev_hand = 0;
	bool prev_full = true;
	bool pair = false;
	for (size_t i = 1; i < raster.pieces.size(); i++) {
		const Direction before = TrackdirToDirection(raster.pieces[i - 1].trackdir);
		const Direction after = TrackdirToDirection(raster.pieces[i].trackdir);
		if (after != before) {
			const int hand = after == ChangeDir(before, DirDiff::Left45) ? 1 : 2;
			bool allowed = units >= spacing;
			bool next_pair = false;
			if (!allowed && !pair && prev_hand != 0 && prev_hand != hand && prev_full) allowed = next_pair = true;
			if (!allowed) return false;
			prev_full = units >= spacing;
			prev_hand = hand;
			pair = next_pair;
			units = 0;
		}
		units += IsDiagonalTrackdir(raster.pieces[i].trackdir) ? 2 : 1;
	}
	return true;
}

TEST_CASE("FindJunctionLineRoute: L_R around a block", "[junction][linerouter]")
{
	JunctionLineRouteRequest request = BaseRequest();
	for (int x = 29; x <= 31; x++) {
		for (int y = 20; y <= 22; y++) request.blocked.push_back({static_cast<int16_t>(x), static_cast<int16_t>(y)});
	}
	const JunctionLineRouteResult result = FindJunctionLineRoute(request);
	CheckRoute(request, result);
	REQUIRE(result.route.has_value());
	CHECK(result.route->moves.find('L') != std::string::npos);
	CHECK(result.route->moves.find('R') != std::string::npos);
}

TEST_CASE("FindJunctionLineRoute: straight when clear", "[junction][linerouter]")
{
	const JunctionLineRouteRequest request = BaseRequest();
	const JunctionLineRouteResult result = FindJunctionLineRoute(request);
	CheckRoute(request, result);
	REQUIRE(result.route.has_value());
	CHECK(result.route->moves == std::string(20, 'F'));
}

TEST_CASE("FindJunctionLineRoute: picks the better of several starts", "[junction][linerouter]")
{
	JunctionLineRouteRequest request = BaseRequest();
	request.starts = {{{40, 20}, Trackdir::X_NE}, {{30, 20}, Trackdir::X_NE}};
	const JunctionLineRouteResult result = FindJunctionLineRoute(request);
	CheckRoute(request, result);
	REQUIRE(result.route.has_value());
	CHECK(result.route->start.x == 30);
	CHECK(result.route->start.y == 20);

	JunctionLineRouteRequest single = BaseRequest();
	single.start = {30, 20};
	const JunctionLineRouteResult from_near = FindJunctionLineRoute(single);
	REQUIRE(from_near.route.has_value());
	CHECK(result.cost == from_near.cost);
	CHECK(result.route->moves == from_near.route->moves);
}

TEST_CASE("FindJunctionLineRoute: one start in the set equals the single start", "[junction][linerouter]")
{
	const JunctionLineRouteRequest plain = BaseRequest();
	const JunctionLineRouteResult expected = FindJunctionLineRoute(plain);
	JunctionLineRouteRequest request = BaseRequest();
	request.starts = {{request.start, request.start_trackdir}};
	request.start = {0, 0};
	const JunctionLineRouteResult result = FindJunctionLineRoute(request);
	REQUIRE(result.route.has_value());
	REQUIRE(expected.route.has_value());
	CHECK(result.route->moves == expected.route->moves);
	CHECK(result.route->start.x == expected.route->start.x);
	CHECK(result.route->start.y == expected.route->start.y);
	CHECK(result.cost == expected.cost);
}

TEST_CASE("FindJunctionLineRoute: starts with a prefix is an error", "[junction][linerouter]")
{
	JunctionLineRouteRequest request = BaseRequest();
	request.starts = {{{40, 20}, Trackdir::X_NE}, {{30, 20}, Trackdir::X_NE}};
	request.prefix = "F";
	const JunctionLineRouteResult result = FindJunctionLineRoute(request);
	CHECK(!result.route.has_value());
	CHECK(result.error == "a prefix needs a single start");
}

TEST_CASE("FindJunctionLineRoute: curve spacing binds", "[junction][linerouter]")
{
	/* A 90 degree turn is two same-hand turns close together; with room for only 3 tiles of diagonal run, spaced turns need a longer way round. */
	JunctionLineRouteRequest request = BaseRequest();
	request.line = {2, 0, 1};
	request.goals = {{{30, 17}, Trackdir::Y_NW}};
	request.min = {10, 17};
	request.curve_spacing = 0;

	/* Without a rule the turns may be close together. */
	const JunctionLineRouteResult loose = FindJunctionLineRoute(request);
	const std::vector<JunctionRaster> loose_rasters = CheckRoute(request, loose);
	CHECK_FALSE(std::ranges::all_of(loose_rasters, [](const JunctionRaster &raster) { return TurnsSpaced(raster, 8); }));

	/* With 8 units between turns the short way is closed; a longer way round keeps every track's turns spaced. */
	request.curve_spacing = 8;
	request.s_bend_window = 8;
	const JunctionLineRouteResult tight = FindJunctionLineRoute(request);
	const std::vector<JunctionRaster> rasters = CheckRoute(request, tight);
	REQUIRE(loose.route.has_value());
	REQUIRE(tight.route.has_value());
	CHECK(tight.route->moves.size() > loose.route->moves.size());
	for (const JunctionRaster &raster : rasters) CHECK(TurnsSpaced(raster, 8));
}

TEST_CASE("FindJunctionLineRoute: LL_RR keeps groups", "[junction][linerouter]")
{
	JunctionLineRouteRequest request = BaseRequest();
	request.line = ParseLineNotation("LL_RR").spec;
	request.goals = {{{28, 10}, Trackdir::Left_N}, {{28, 10}, Trackdir::Right_N}};
	const JunctionLineRouteResult result = FindJunctionLineRoute(request);
	const std::vector<JunctionRaster> rasters = CheckRoute(request, result);
	REQUIRE(result.route.has_value());
	CHECK(std::ranges::count_if(result.route->moves, [](char c) { return c != 'F'; }) == 1);
	

	const int cumulative[] = {0, 1, 4, 5};
	const auto [px, py] = ExitPoint(rasters[0].pieces.back());
	for (size_t i = 1; i < 4; i++) {
		const auto [ex, ey] = ExitPoint(rasters[i].pieces.back());
		const int dx = ex - px;
		const int dy = ey - py;
		CHECK(dx + dy == 0); // Heading N is along x - y: the offset is across it.
		CHECK(std::abs(dx - dy) == 2 * cumulative[i]);
	}
}

TEST_CASE("ExtendJunctionLine reproduces RasterizeJunctionLine", "[junction][linerouter]")
{
	const Trackdir starts[] = {Trackdir::X_NE, Trackdir::Y_SE, Trackdir::Upper_E, Trackdir::Left_N, Trackdir::Right_S, Trackdir::Lower_W};
	const char *programs[] = {"L", "R", "FLF", "FRF", "LL", "RR", "LR", "RL", "FLFRF", "LFLF", "LLLL", "RRRRR"};
	const JunctionLineSpec specs[] = {{2, 0, 1}, {2, 1, 1}, {3, 1, 1}, {4, 1, 2}};
	for (const JunctionLineSpec &spec : specs) {
		for (Trackdir start : starts) {
			for (const char *program : programs) {
				INFO("tracks " << int{spec.tracks} << " gap " << int{spec.gap} << " start " << static_cast<int>(start) << " moves " << program);
				const std::vector<JunctionRaster> expected = RasterizeJunctionLine(spec, {{20, 20}, start, program});
				std::vector<JunctionRaster> stepped = RasterizeJunctionLine(spec, {{20, 20}, start, ""});
				REQUIRE(stepped.size() == spec.tracks);
				for (const char *move = program; *move != '\0'; move++) {
					std::vector<JunctionPiece> last;
					for (const JunctionRaster &raster : stepped) last.push_back(raster.pieces.back());
					std::string error;
					const std::vector<std::vector<JunctionPiece>> extension = ExtendJunctionLine(spec, last, *move, error);
					REQUIRE(extension.size() == spec.tracks);
					for (size_t i = 0; i < spec.tracks; i++) stepped[i].pieces.insert(stepped[i].pieces.end(), extension[i].begin(), extension[i].end());
				}
				REQUIRE(expected.size() == stepped.size());
				for (size_t i = 0; i < expected.size(); i++) {
					REQUIRE(expected[i].pieces.size() == stepped[i].pieces.size());
					for (size_t p = 0; p < expected[i].pieces.size(); p++) {
						CHECK(expected[i].pieces[p].tile.x == stepped[i].pieces[p].tile.x);
						CHECK(expected[i].pieces[p].tile.y == stepped[i].pieces[p].tile.y);
						CHECK(expected[i].pieces[p].trackdir == stepped[i].pieces[p].trackdir);
					}
				}
			}
		}
	}
}

TEST_CASE("FindJunctionLineRoute: no route", "[junction][linerouter]")
{
	JunctionLineRouteRequest request = BaseRequest();
	for (int x = 19; x <= 21; x++) {
		for (int y = 19; y <= 21; y++) {
			if (x != 20 || y != 20) request.blocked.push_back({static_cast<int16_t>(x), static_cast<int16_t>(y)});
		}
	}
	const JunctionLineRouteResult result = FindJunctionLineRoute(request);
	CHECK_FALSE(result.route.has_value());
	CHECK(result.error == "no route");
}

/** A flat Buildable terrain at plane 0 covering the request's bounds, with the given clearing cost per tile. */
static JunctionTerrain FlatTerrainFor(const JunctionLineRouteRequest &request, Money clear_cost)
{
	JunctionTerrain terrain;
	terrain.origin = request.min;
	terrain.width = request.max.x - request.min.x + 1;
	terrain.height = request.max.y - request.min.y + 1;
	JunctionTerrainTile tile;
	tile.clear_cost = clear_cost;
	terrain.tiles.assign(static_cast<size_t>(terrain.width) * terrain.height, tile);
	return terrain;
}

static JunctionTerrainTile &TerrainTileOf(JunctionTerrain &terrain, int x, int y)
{
	return terrain.tiles[static_cast<size_t>(y - terrain.origin.y) * terrain.width + (x - terrain.origin.x)];
}

/** Whether any piece of any track of the route lies on the tile. */
static bool RouteUsesTile(const JunctionLineRouteRequest &request, const JunctionLineRouteResult &result, int x, int y)
{
	for (const JunctionRaster &raster : RasterizeJunctionLine(request.line, *result.route)) {
		for (const JunctionPiece &piece : raster.pieces) {
			if (piece.tile.x == x && piece.tile.y == y) return true;
		}
	}
	return false;
}

TEST_CASE("FindJunctionLineRoute: detours around a house", "[junction][linerouter]")
{
	JunctionLineRouteRequest request = BaseRequest();
	const JunctionLineRouteResult straight = FindJunctionLineRoute(request);
	REQUIRE(straight.route.has_value());
	REQUIRE(straight.route->moves == std::string(20, 'F'));

	/* A tile on track 1's straight path that track 0 does not use. */
	const std::vector<JunctionRaster> rasters = RasterizeJunctionLine(request.line, *straight.route);
	const JunctionPiece &middle = rasters[1].pieces[rasters[1].pieces.size() / 2];
	REQUIRE(middle.tile.y != 20);

	JunctionTerrain terrain = FlatTerrainFor(request, 0);
	TerrainTileOf(terrain, middle.tile.x, middle.tile.y).cls = JunctionTileClass::Blocked;
	request.terrain = &terrain;
	const JunctionLineRouteResult result = FindJunctionLineRoute(request);
	CheckRoute(request, result);
	REQUIRE(result.route.has_value());
	CHECK_FALSE(RouteUsesTile(request, result, middle.tile.x, middle.tile.y));
	CHECK(result.cost > straight.cost);
}

TEST_CASE("FindJunctionLineRoute: refused over water", "[junction][linerouter]")
{
	JunctionLineRouteRequest request = BaseRequest();
	const JunctionLineRouteResult straight = FindJunctionLineRoute(request);
	REQUIRE(straight.route.has_value());
	const std::vector<JunctionRaster> rasters = RasterizeJunctionLine(request.line, *straight.route);
	const JunctionPiece &middle = rasters[1].pieces[rasters[1].pieces.size() / 2];

	/* The bounds hug the line: exactly the rows of its two tracks, so no sideways shift fits. */
	int min_y = INT32_MAX;
	int max_y = INT32_MIN;
	for (const JunctionRaster &raster : rasters) {
		for (const JunctionPiece &piece : raster.pieces) {
			min_y = std::min<int>(min_y, piece.tile.y);
			max_y = std::max<int>(max_y, piece.tile.y);
		}
	}
	request.min.y = static_cast<int16_t>(min_y);
	request.max.y = static_cast<int16_t>(max_y);

	JunctionTerrain terrain = FlatTerrainFor(request, 0);
	TerrainTileOf(terrain, middle.tile.x, middle.tile.y).cls = JunctionTileClass::Water;
	request.terrain = &terrain;
	const JunctionLineRouteResult result = FindJunctionLineRoute(request);
	CHECK_FALSE(result.route.has_value());
	CHECK(result.error == "no route");
}

TEST_CASE("FindJunctionLineRoute: pays terrain cost once per tile", "[junction][linerouter]")
{
	JunctionLineRouteRequest request = BaseRequest();
	request.rail_piece = 1;
	request.turn_cost = 0;
	JunctionTerrain terrain = FlatTerrainFor(request, 10);
	request.terrain = &terrain;
	const JunctionLineRouteResult result = FindJunctionLineRoute(request);
	CheckRoute(request, result);
	REQUIRE(result.route.has_value());
	CHECK(result.route->moves == std::string(20, 'F'));

	/* The start pieces are the line's first tiles: their terrain is paid, their rail is not. */
	const std::vector<JunctionRaster> rasters = RasterizeJunctionLine(request.line, *result.route);
	std::set<std::pair<int, int>> tiles;
	int added_pieces = 0;
	for (const JunctionRaster &raster : rasters) {
		added_pieces += static_cast<int>(raster.pieces.size()) - 1;
		for (const JunctionPiece &piece : raster.pieces) tiles.emplace(piece.tile.x, piece.tile.y);
	}
	CHECK(result.cost == added_pieces * 1 + static_cast<int>(tiles.size()) * 10);
}

TEST_CASE("FindJunctionLineRoute: existing track is an obstacle", "[junction][linerouter]")
{
	JunctionLineRouteRequest request = BaseRequest();
	const JunctionLineRouteResult straight = FindJunctionLineRoute(request);
	REQUIRE(straight.route.has_value());
	const std::vector<JunctionRaster> rasters = RasterizeJunctionLine(request.line, *straight.route);
	const JunctionPiece &middle = rasters[0].pieces[rasters[0].pieces.size() / 2];

	/* A crossing track on track 0's path. */
	JunctionLayout crossing;
	crossing.tracks.push_back({{middle.tile.x, middle.tile.y}, TrackBits{Track::Y}});
	request.existing = &crossing;
	const JunctionLineRouteResult around = FindJunctionLineRoute(request);
	CheckRoute(request, around);
	REQUIRE(around.route.has_value());
	CHECK_FALSE(RouteUsesTile(request, around, middle.tile.x, middle.tile.y));
	CHECK(around.cost > straight.cost);
}

TEST_CASE("FindJunctionLineRoute: existing track allows an exact parallel pair", "[junction][linerouter]")
{
	/* A line with no gap has tiles in its turns where two of its tracks run as an exact parallel pair. */
	JunctionLineRouteRequest request = BaseRequest();
	request.line = {2, 0, 1};
	request.goals = {{{24, 30}, Trackdir::X_NE}};
	const JunctionLineRouteResult free_route = FindJunctionLineRoute(request);
	REQUIRE(free_route.route.has_value());
	const std::vector<JunctionRaster> rasters = RasterizeJunctionLine(request.line, *free_route.route);
	std::map<std::pair<int, int>, TrackBits> bits;
	for (const JunctionRaster &raster : rasters) {
		for (const JunctionPiece &piece : raster.pieces) bits[{piece.tile.x, piece.tile.y}].Set(TrackdirToTrack(piece.trackdir));
	}
	/* A piece of track 0 on an Upper, Lower, Left or Right track whose tile holds no other track of the line: its parallel partner is free there. */
	const JunctionPiece *half = nullptr;
	for (const JunctionPiece &piece : rasters[0].pieces) {
		if (!IsDiagonalTrackdir(piece.trackdir) && bits[{piece.tile.x, piece.tile.y}].Count() == 1) {
			half = &piece;
			break;
		}
	}
	REQUIRE(half != nullptr);
	const Track track = TrackdirToTrack(half->trackdir);
	const Track partner = track == Track::Upper ? Track::Lower : track == Track::Lower ? Track::Upper : track == Track::Left ? Track::Right : Track::Left;

	/* The partner is already built there: the same route still stands. */
	JunctionLayout layout;
	layout.tracks.push_back({{half->tile.x, half->tile.y}, TrackBits{partner}});
	request.existing = &layout;
	const JunctionLineRouteResult result = FindJunctionLineRoute(request);
	CheckRoute(request, result);
	REQUIRE(result.route.has_value());
	CHECK(result.route->moves == free_route.route->moves);
	CHECK(result.cost == free_route.cost);
}

TEST_CASE("FindJunctionLineRoute: the route is as cheap as an uninformed search", "[junction][linerouter][slow]")
{
	const char *lines[] = {"L_R", "LL_RR"};
	for (const char *notation : lines) {
		JunctionLineRouteRequest request = BaseRequest();
		request.line = ParseLineNotation(notation).spec;
		request.turn_cost = 3;
		for (int dx = 6; dx <= 14; dx++) {
			for (int dy = -4; dy <= 4; dy++) {
				for (Trackdir trackdir : {Trackdir::X_NE, Trackdir::Y_NW, Trackdir::Left_N, Trackdir::Right_N, Trackdir::Upper_E, Trackdir::Lower_E}) {
					request.goals = {{{static_cast<int16_t>(request.start.x - dx), static_cast<int16_t>(request.start.y + dy)}, trackdir}};
					request.use_heuristic = true;
					const JunctionLineRouteResult informed = FindJunctionLineRoute(request);
					request.use_heuristic = false;
					const JunctionLineRouteResult uninformed = FindJunctionLineRoute(request);
					INFO(notation << " goal " << request.goals[0].tile.x << "," << request.goals[0].tile.y << " trackdir " << static_cast<int>(trackdir) << " informed " << informed.route.has_value() << " cost " << informed.cost << " uninformed " << uninformed.route.has_value() << " cost " << uninformed.cost);
					REQUIRE(uninformed.error != "search limit");
					/* D161: pruning is approximate for lines whose tracks can collide; an overestimating heuristic shows as a costlier informed route. */
					if (informed.route.has_value() && uninformed.route.has_value()) CHECK(informed.cost <= uninformed.cost);
				}
			}
		}
		/* Several starts (DESIGN-O4). */
		for (int dy = -4; dy <= 4; dy++) {
			request.starts = {{request.start, Trackdir::X_NE}, {{static_cast<int16_t>(request.start.x - 6), static_cast<int16_t>(request.start.y + dy)}, Trackdir::X_NE}};
			request.goals = {{{20, 20}, Trackdir::X_NE}};
			request.use_heuristic = true;
			const JunctionLineRouteResult informed = FindJunctionLineRoute(request);
			request.use_heuristic = false;
			const JunctionLineRouteResult uninformed = FindJunctionLineRoute(request);
			INFO(notation << " starts dy " << dy << " informed " << informed.route.has_value() << " cost " << informed.cost << " uninformed " << uninformed.route.has_value() << " cost " << uninformed.cost);
			REQUIRE(uninformed.error != "search limit");
			CHECK(informed.route.has_value() == uninformed.route.has_value());
			if (informed.route.has_value() && uninformed.route.has_value()) CHECK(informed.cost <= uninformed.cost);
		}
		request.starts.clear();
	}
}

/** A line request with bridges on, its terrain and its existing layout, set up in place (the request points into the struct). */
struct LineBridgeCase {
	JunctionLineRouteRequest request;
	JunctionTerrain terrain;
	JunctionLayout layout;
};

static void EnableBridges(JunctionLineRouteRequest &request)
{
	request.rail_piece = 1;
	request.bridge_price = [](uint n) { return Money(10 * (n + 2)); };
	request.bridge_span_ok = [](uint n) { return n <= 6; };
	request.max_bridge_middle = 8;
}

/** The rows of the tracks of the line when it runs straight. */
static std::vector<int> TrackRows(const JunctionLineRouteRequest &request)
{
	std::vector<int> rows;
	for (const JunctionRaster &raster : RasterizeJunctionLine(request.line, {request.start, request.start_trackdir, ""})) rows.push_back(raster.pieces[0].tile.y);
	return rows;
}

/** A river of the given width across the whole field from x = 30 towards larger x, with the bounds hugging the line's rows when asked; track 1's row has one more water tile at x = 29 when uneven. */
static void SetUpRiver(LineBridgeCase &c, const char *notation, int width, bool uneven, bool hug)
{
	c.request = BaseRequest();
	c.request.line = ParseLineNotation(notation).spec;
	EnableBridges(c.request);
	const std::vector<int> rows = TrackRows(c.request);
	if (hug) {
		c.request.min.y = static_cast<int16_t>(*std::min_element(rows.begin(), rows.end()));
		c.request.max.y = static_cast<int16_t>(*std::max_element(rows.begin(), rows.end()));
	}
	c.terrain = FlatTerrainFor(c.request, 0);
	for (int y = c.request.min.y; y <= c.request.max.y; y++) {
		for (int x = 30; x < 30 + width; x++) TerrainTileOf(c.terrain, x, y).cls = JunctionTileClass::Water;
	}
	if (uneven) TerrainTileOf(c.terrain, 29, rows[1]).cls = JunctionTileClass::Water;
	c.request.terrain = &c.terrain;
}

/** A straight track along y crossing the line's path at x = 30, over the whole field. */
static void SetUpCrossing(LineBridgeCase &c, const char *notation)
{
	c.request = BaseRequest();
	c.request.line = ParseLineNotation(notation).spec;
	EnableBridges(c.request);
	const std::vector<int> rows = TrackRows(c.request);
	c.request.min.y = static_cast<int16_t>(*std::min_element(rows.begin(), rows.end()));
	c.request.max.y = static_cast<int16_t>(*std::max_element(rows.begin(), rows.end()));
	for (int y = c.request.min.y; y <= c.request.max.y; y++) c.layout.tracks.push_back({{30, static_cast<int16_t>(y)}, TrackBits{Track::Y}});
	c.request.existing = &c.layout;
}

static void CheckRiverBridges(const LineBridgeCase &c, const JunctionLineRouteResult &result, size_t tracks)
{
	REQUIRE(result.route.has_value());
	const std::vector<JunctionRaster> rasters = RasterizeJunctionLine(c.request.line, *result.route);
	REQUIRE(rasters.size() == tracks);
	for (const JunctionRaster &raster : rasters) {
		INFO(raster.error);
		REQUIRE(raster.error.empty());
		REQUIRE(raster.bridges.size() == 1);
		for (const JunctionPiece &piece : raster.pieces) {
			if (piece.on_bridge) continue;
			CHECK(c.terrain.At(piece.tile.x, piece.tile.y)->cls != JunctionTileClass::Water);
		}
		/* The heads lie side by side with the first track's. */
		CHECK(raster.bridges[0].start.y == raster.bridges[0].end.y);
		CHECK(raster.bridges[0].start.x == rasters[0].bridges[0].start.x);
		CHECK(raster.bridges[0].end.x == rasters[0].bridges[0].end.x);
	}
}

TEST_CASE("FindJunctionLineRoute: L_R bridges a river", "[junction][linerouter]")
{
	LineBridgeCase c;
	SetUpRiver(c, "L_R", 3, false, false);
	const JunctionLineRouteResult result = FindJunctionLineRoute(c.request);
	INFO(result.error);
	REQUIRE(result.route.has_value());
	CHECK(result.route->moves.find("B3") != std::string::npos);
	CheckRiverBridges(c, result, 2);
}

TEST_CASE("FindJunctionLineRoute: LL_RR bridges a river", "[junction][linerouter]")
{
	LineBridgeCase c;
	SetUpRiver(c, "LL_RR", 3, false, false);
	const JunctionLineRouteResult result = FindJunctionLineRoute(c.request);
	INFO(result.error);
	REQUIRE(result.route.has_value());
	CHECK(result.route->moves.find("B3") != std::string::npos);
	CheckRiverBridges(c, result, 4);
}

TEST_CASE("FindJunctionLineRoute: bridges over existing track", "[junction][linerouter]")
{
	LineBridgeCase c;
	SetUpCrossing(c, "L_R");
	const JunctionLineRouteResult result = FindJunctionLineRoute(c.request);
	INFO(result.error);
	REQUIRE(result.route.has_value());
	const std::vector<JunctionRaster> rasters = RasterizeJunctionLine(c.request.line, *result.route);
	for (const JunctionRaster &raster : rasters) {
		REQUIRE(raster.error.empty());
		REQUIRE(raster.bridges.size() == 1);
		CHECK(std::min(raster.bridges[0].start.x, raster.bridges[0].end.x) < 30);
		CHECK(std::max(raster.bridges[0].start.x, raster.bridges[0].end.x) > 30);
		for (const JunctionPiece &piece : raster.pieces) {
			if (piece.tile.x == 30) CHECK(piece.on_bridge);
		}
		for (const TileIndexDiffC &head : {raster.bridges[0].start, raster.bridges[0].end}) CHECK(head.x != 30);
	}
}

/** A one-track request heading along -x from (40, 20) whose goal lies one row to the right after a bridge built straight from an S-bend (D110): the template RF + B3 puts its heads at x = 38 and 34 over existing track at x = 35..37; closing the S-bend with an axis piece (RFL + B<n>) would put the near head on that track. */
static void SetUpSBendBridge(LineBridgeCase &c, std::string &moves)
{
	c.request = BaseRequest();
	c.request.line = JunctionLineSpec{1, 0, 1};
	EnableBridges(c.request);
	c.request.bridge_span_ok = [](uint n) { return n <= 3; };
	moves = "RFB3F";
	const JunctionRaster expected = RasterizeJunctionRoute({{40, 20}, Trackdir::X_NE, moves});
	REQUIRE(expected.error.empty());
	c.request.goals = {{expected.pieces.back().tile, expected.pieces.back().trackdir}};
	for (int y = c.request.min.y; y <= c.request.max.y; y++) {
		for (int x = 35; x <= 37; x++) c.layout.tracks.push_back({{static_cast<int16_t>(x), static_cast<int16_t>(y)}, TrackBits{Track::Y}});
	}
	c.request.existing = &c.layout;
}

TEST_CASE("FindJunctionLineRoute: closes an S-bend with a bridge head (D110)", "[junction][d110bridge]")
{
	LineBridgeCase c;
	std::string moves;
	SetUpSBendBridge(c, moves);
	const JunctionLineRouteResult result = FindJunctionLineRoute(c.request);
	INFO(result.error);
	REQUIRE(result.route.has_value());

	const JunctionRaster expected = RasterizeJunctionRoute({{40, 20}, Trackdir::X_NE, moves});
	const std::vector<JunctionRaster> rasters = RasterizeJunctionLine(c.request.line, *result.route);
	REQUIRE(rasters.size() == 1);
	INFO(rasters[0].error);
	REQUIRE(rasters[0].error.empty());
	REQUIRE(rasters[0].pieces.size() == expected.pieces.size());
	for (size_t i = 0; i < expected.pieces.size(); i++) {
		CHECK(rasters[0].pieces[i].tile.x == expected.pieces[i].tile.x);
		CHECK(rasters[0].pieces[i].tile.y == expected.pieces[i].tile.y);
		CHECK(rasters[0].pieces[i].trackdir == expected.pieces[i].trackdir);
		CHECK(rasters[0].pieces[i].on_bridge == expected.pieces[i].on_bridge);
	}
	REQUIRE(rasters[0].bridges.size() == 1);
	REQUIRE(expected.bridges.size() == 1);
	CHECK(rasters[0].bridges[0].start.x == expected.bridges[0].start.x);
	CHECK(rasters[0].bridges[0].start.y == expected.bridges[0].start.y);
	CHECK(rasters[0].bridges[0].end.x == expected.bridges[0].end.x);
	CHECK(rasters[0].bridges[0].end.y == expected.bridges[0].end.y);
}

TEST_CASE("ExtendJunctionLineBridge: a multi-track line bridges from axis pieces only (D149)", "[junction][d110bridge]")
{
	const JunctionRaster half = RasterizeJunctionRoute({{40, 20}, Trackdir::X_NE, "RF"});
	REQUIRE(half.error.empty());
	std::vector<JunctionPiece> last;
	last.push_back(half.pieces.back());
	last.push_back(half.pieces.back());
	std::string error;
	std::vector<JunctionBridge> bridges;
	CHECK(ExtendJunctionLineBridge(ParseLineNotation("L_R").spec, last, 3, bridges, error).empty());
	CHECK_FALSE(error.empty());

	/* One track takes the same piece. */
	last.resize(1);
	error.clear();
	CHECK_FALSE(ExtendJunctionLineBridge(JunctionLineSpec{1, 0, 1}, last, 3, bridges, error).empty());
	CHECK(error.empty());
}

TEST_CASE("FindJunctionLineRoute: all tracks share the bridge length", "[junction][linerouter]")
{
	LineBridgeCase c;
	SetUpRiver(c, "L_R", 3, true, true);
	const JunctionLineRouteResult result = FindJunctionLineRoute(c.request);
	INFO(result.error);
	REQUIRE(result.route.has_value());
	CHECK(result.route->moves.find("B4") != std::string::npos);
	CHECK(result.route->moves.find("B3") == std::string::npos);
	CheckRiverBridges(c, result, 2);
}

TEST_CASE("FindJunctionLineRoute: a bridge longer than the span limit is refused", "[junction][linerouter]")
{
	LineBridgeCase c;
	SetUpRiver(c, "L_R", 7, false, false);
	const JunctionLineRouteResult result = FindJunctionLineRoute(c.request);
	CHECK_FALSE(result.route.has_value());
	CHECK(result.error == "no route");
}

TEST_CASE("RasterizeJunctionLine: a bridge move", "[junction][linerouter]")
{
	const JunctionLineSpec line = ParseLineNotation("L_R").spec;
	const std::vector<JunctionRaster> rasters = RasterizeJunctionLine(line, {{40, 20}, Trackdir::X_NE, "FFB2F"});
	REQUIRE(rasters.size() == 2);
	for (const JunctionRaster &raster : rasters) {
		INFO(raster.error);
		REQUIRE(raster.error.empty());
		CHECK(std::count_if(raster.pieces.begin(), raster.pieces.end(), [](const JunctionPiece &piece) { return piece.on_bridge; }) == 4);
		CHECK(raster.bridges.size() == 1);
	}

	CHECK_FALSE(RasterizeJunctionLine(line, {{40, 20}, Trackdir::X_NE, "FLB2"})[0].error.empty());
	CHECK_FALSE(RasterizeJunctionLine(line, {{40, 20}, Trackdir::X_NE, "FB"})[0].error.empty());
}

TEST_CASE("FindJunctionLineRoute: bridge routes are as cheap as an uninformed search", "[junction][linerouter][slow]")
{
	for (int which = 0; which < 4; which++) {
		LineBridgeCase c;
		switch (which) {
			case 0: SetUpRiver(c, "L_R", 3, false, false); break;
			case 1: SetUpRiver(c, "LL_RR", 3, false, false); break;
			case 2: SetUpCrossing(c, "L_R"); break;
			default: SetUpRiver(c, "L_R", 3, true, true); break;
		}
		c.request.use_heuristic = true;
		const JunctionLineRouteResult informed = FindJunctionLineRoute(c.request);
		c.request.use_heuristic = false;
		const JunctionLineRouteResult uninformed = FindJunctionLineRoute(c.request);
		INFO("case " << which << " informed " << informed.route.has_value() << " cost " << informed.cost << " uninformed " << uninformed.route.has_value() << " cost " << uninformed.cost);
		REQUIRE(informed.route.has_value());
		REQUIRE(uninformed.route.has_value());
		/* D161: pruning is approximate for lines whose tracks can collide; an overestimating heuristic shows as a costlier informed route. */
		CHECK(informed.cost <= uninformed.cost);
	}
}

/** Whether track 0 of the line program has a piece that is not a bridge piece on the tile. */
static bool Track0Passes(const JunctionLineRouteRequest &request, const std::string &moves, const TileIndexDiffC &tile)
{
	const std::vector<JunctionRaster> rasters = RasterizeJunctionLine(request.line, {request.start, request.start_trackdir, moves});
	return std::ranges::any_of(rasters[0].pieces, [&](const JunctionPiece &piece) {
		return !piece.on_bridge && piece.tile.x == tile.x && piece.tile.y == tile.y;
	});
}

/** The index of track 0's first piece on a tile, or SIZE_MAX. */
static size_t FirstPieceOn(const std::vector<JunctionRaster> &rasters, const TileIndexDiffC &tile)
{
	for (size_t i = 0; i < rasters[0].pieces.size(); i++) {
		if (rasters[0].pieces[i].tile.x == tile.x && rasters[0].pieces[i].tile.y == tile.y) return i;
	}
	return SIZE_MAX;
}

/** The waypoint test requests: 0 one waypoint, 1 two waypoints out of order of distance, 2 a waypoint then a goal, 3 a frozen prefix ending in a turn (an S-bend after a short run, so no turn is allowed for 12 units; the waypoint is on the diagonal from its end), 4 a frozen straight prefix and a waypoint behind the start. */
static JunctionLineRouteRequest WaypointRequest(int which)
{
	JunctionLineRouteRequest request = BaseRequest();
	request.goals.clear();
	switch (which) {
		case 0:
			request.waypoints = {{30, 16}};
			break;
		case 1:
			request.waypoints = {{28, 20}, {34, 16}};
			break;
		case 2:
			request.waypoints = {{30, 16}};
			request.goals = {{{20, 16}, Trackdir::X_NE}};
			break;
		case 3: {
			request.curve_spacing = 8;
			request.s_bend_window = 12;
			request.prefix = "RFFFL";
			const std::vector<JunctionRaster> rasters = RasterizeJunctionLine(request.line, {request.start, request.start_trackdir, request.prefix});
			const TileIndexDiffC end = rasters[0].pieces.back().tile;
			request.waypoints = {{static_cast<int16_t>(end.x - 6), static_cast<int16_t>(end.y - 6)}};
			break;
		}
		default:
			request.prefix = "FFFFFF";
			request.waypoints = {{44, 20}};
			break;
	}
	return request;
}

TEST_CASE("FindJunctionLineRoute: passes a waypoint", "[junction][linerouter]")
{
	const JunctionLineRouteRequest request = WaypointRequest(0);
	const JunctionLineRouteResult result = FindJunctionLineRoute(request);
	INFO(result.error);
	REQUIRE(result.route.has_value());
	const std::string &moves = result.route->moves;
	for (const JunctionRaster &raster : RasterizeJunctionLine(request.line, *result.route)) CHECK(raster.error.empty());
	CHECK(Track0Passes(request, moves, request.waypoints[0]));
	REQUIRE_FALSE(moves.empty());
	CHECK_FALSE(Track0Passes(request, moves.substr(0, moves.size() - 1), request.waypoints[0]));
}

TEST_CASE("FindJunctionLineRoute: passes waypoints in order", "[junction][linerouter]")
{
	const JunctionLineRouteRequest request = WaypointRequest(1);
	const JunctionLineRouteResult result = FindJunctionLineRoute(request);
	INFO(result.error);
	REQUIRE(result.route.has_value());
	const std::vector<JunctionRaster> rasters = RasterizeJunctionLine(request.line, *result.route);
	const size_t first = FirstPieceOn(rasters, request.waypoints[0]);
	const size_t second = FirstPieceOn(rasters, request.waypoints[1]);
	REQUIRE(first != SIZE_MAX);
	REQUIRE(second != SIZE_MAX);
	CHECK(first < second);
}

TEST_CASE("FindJunctionLineRoute: a waypoint then a goal", "[junction][linerouter]")
{
	const JunctionLineRouteRequest request = WaypointRequest(2);
	const JunctionLineRouteResult result = FindJunctionLineRoute(request);
	CheckRoute(request, result);
	REQUIRE(result.route.has_value());
	CHECK(Track0Passes(request, result.route->moves, request.waypoints[0]));
}

TEST_CASE("FindJunctionLineRoute: a frozen prefix carries its curve state", "[junction][linerouter]")
{
	const JunctionLineRouteRequest request = WaypointRequest(3);
	const std::vector<JunctionRaster> prefix_rasters = RasterizeJunctionLine(request.line, {request.start, request.start_trackdir, request.prefix});
	for (const JunctionRaster &raster : prefix_rasters) REQUIRE(raster.error.empty());

	const JunctionLineRouteResult result = FindJunctionLineRoute(request);
	INFO(result.error);
	REQUIRE(result.route.has_value());
	const std::string &moves = result.route->moves;
	REQUIRE(moves.size() > request.prefix.size());
	CHECK(moves.substr(0, request.prefix.size()) == request.prefix);
	const char after = moves[request.prefix.size()];
	CHECK((after != 'L' && after != 'R'));

	/* From the prefix's last pieces with no history, the same waypoint is reached by turning at once. */
	JunctionLineRouteRequest fresh = request;
	fresh.prefix.clear();
	fresh.start = prefix_rasters[0].pieces.back().tile;
	fresh.start_trackdir = prefix_rasters[0].pieces.back().trackdir;
	const JunctionLineRouteResult turned = FindJunctionLineRoute(fresh);
	INFO(turned.error);
	REQUIRE(turned.route.has_value());
	INFO("fresh moves " << turned.route->moves);
	REQUIRE_FALSE(turned.route->moves.empty());
	CHECK((turned.route->moves[0] == 'L' || turned.route->moves[0] == 'R'));
}

TEST_CASE("FindJunctionLineRoute: an approach's S-bend counts against the next one", "[junction][approach]")
{
	/* The prefix request: an S-bend, a straight run, then the waypoint (CS1). */
	const JunctionLineRouteRequest request = WaypointRequest(3);
	const std::vector<JunctionRaster> prefix_rasters = RasterizeJunctionLine(request.line, {request.start, request.start_trackdir, request.prefix});
	REQUIRE(prefix_rasters[0].error.empty());
	const std::vector<JunctionPiece> &pieces = prefix_rasters[0].pieces;
	const JunctionLineRouteResult with_prefix = FindJunctionLineRoute(request);
	INFO(with_prefix.error);
	REQUIRE(with_prefix.route.has_value());
	const std::string searched = with_prefix.route->moves.substr(request.prefix.size());

	/* The same request from the prefix's last piece, the prefix being the approach instead. */
	JunctionLineRouteRequest continued = request;
	continued.prefix.clear();
	continued.start = pieces.back().tile;
	continued.start_trackdir = pieces.back().trackdir;
	const JunctionLineRouteResult fresh = FindJunctionLineRoute(continued);
	REQUIRE(fresh.route.has_value());
	REQUIRE_FALSE(fresh.route->moves.empty());
	CHECK((fresh.route->moves[0] == 'L' || fresh.route->moves[0] == 'R'));

	continued.approach = [&](const JunctionRouteEnd &) { return std::vector<JunctionPiece>(pieces.begin(), pieces.end() - 1); };
	const JunctionLineRouteResult inherited = FindJunctionLineRoute(continued);
	INFO(inherited.error);
	REQUIRE(inherited.route.has_value());
	CHECK(inherited.route->moves == searched);
	CHECK((inherited.route->moves[0] != 'L' && inherited.route->moves[0] != 'R'));
}

TEST_CASE("FindJunctionLineRoute: an empty approach changes nothing", "[junction][approach]")
{
	std::vector<JunctionLineRouteRequest> requests = {BaseRequest(), WaypointRequest(0), WaypointRequest(1), WaypointRequest(2)};
	JunctionLineRouteRequest blocked = BaseRequest();
	for (int x = 29; x <= 31; x++) {
		for (int y = 20; y <= 22; y++) blocked.blocked.push_back({static_cast<int16_t>(x), static_cast<int16_t>(y)});
	}
	requests.push_back(blocked);
	for (const JunctionLineRouteRequest &request : requests) {
		const JunctionLineRouteResult plain = FindJunctionLineRoute(request);
		JunctionLineRouteRequest with = request;
		with.approach = [](const JunctionRouteEnd &) { return std::vector<JunctionPiece>{}; };
		const JunctionLineRouteResult empty = FindJunctionLineRoute(with);
		REQUIRE(plain.route.has_value());
		REQUIRE(empty.route.has_value());
		CHECK(empty.route->moves == plain.route->moves);
		CHECK(empty.cost == plain.cost);
		CHECK(empty.expansions == plain.expansions);
	}
}

TEST_CASE("FindJunctionLineRoute: prefix pieces are the line's own track", "[junction][linerouter]")
{
	const JunctionLineRouteRequest request = WaypointRequest(4);
	const JunctionLineRouteResult result = FindJunctionLineRoute(request);
	INFO(result.error);
	REQUIRE(result.route.has_value());
	const std::string &moves = result.route->moves;
	REQUIRE(moves.size() > request.prefix.size());
	CHECK(moves.substr(0, request.prefix.size()) == request.prefix);

	const std::vector<JunctionRaster> prefix_rasters = RasterizeJunctionLine(request.line, {request.start, request.start_trackdir, request.prefix});
	const std::vector<JunctionRaster> rasters = RasterizeJunctionLine(request.line, *result.route);
	std::map<std::pair<int, int>, TrackBits> tiles;
	for (const JunctionRaster &raster : prefix_rasters) {
		for (const JunctionPiece &piece : raster.pieces) tiles[{piece.tile.x, piece.tile.y}].Set(TrackdirToTrack(piece.trackdir));
	}
	for (size_t t = 0; t < rasters.size(); t++) {
		for (size_t i = prefix_rasters[t].pieces.size(); i < rasters[t].pieces.size(); i++) {
			const JunctionPiece &piece = rasters[t].pieces[i];
			TrackBits &bits = tiles[{piece.tile.x, piece.tile.y}];
			const Track track = TrackdirToTrack(piece.trackdir);
			CHECK_FALSE(bits.Test(track));
			bits.Set(track);
			CHECK_FALSE(TracksOverlap(bits));
		}
	}
}

TEST_CASE("FindJunctionLineRoute: a waypoint on water is refused", "[junction][linerouter]")
{
	JunctionLineRouteRequest request = WaypointRequest(0);
	request.min = {22, 10};
	request.max = {45, 30};
	JunctionTerrain terrain = FlatTerrainFor(request, 0);
	TerrainTileOf(terrain, request.waypoints[0].x, request.waypoints[0].y).cls = JunctionTileClass::Water;
	request.terrain = &terrain;
	const JunctionLineRouteResult result = FindJunctionLineRoute(request);
	CHECK_FALSE(result.route.has_value());
	CHECK(result.error == "no route");
}

TEST_CASE("FindJunctionLineRoute: waypoint routes are as cheap as an uninformed search", "[junction][linerouter][slow]")
{
	for (int which = 0; which < 5; which++) {
		JunctionLineRouteRequest request = WaypointRequest(which);
		request.use_heuristic = true;
		const JunctionLineRouteResult informed = FindJunctionLineRoute(request);
		request.use_heuristic = false;
		const JunctionLineRouteResult uninformed = FindJunctionLineRoute(request);
		INFO("case " << which << " informed " << informed.route.has_value() << " cost " << informed.cost << " uninformed " << uninformed.route.has_value() << " cost " << uninformed.cost);
		REQUIRE(informed.route.has_value());
		REQUIRE(uninformed.route.has_value());
		/* D161: pruning is approximate for lines whose tracks can collide; an overestimating heuristic shows as a costlier informed route. */
		CHECK(informed.cost <= uninformed.cost);
	}
}

/** A line request with ramps: the terrain has a hand-set height profile along x, and the request's heights follow it (D152). */
struct LineRampCase {
	JunctionLineRouteRequest request;
	JunctionTerrain terrain;
};

/** Set every tile of a column of the terrain: heights and incline. */
static void SetRampColumn(JunctionTerrain &terrain, int x, int z_min, int z_max, DiagDirection incline)
{
	for (int y = terrain.origin.y; y < terrain.origin.y + terrain.height; y++) {
		JunctionTerrainTile &tile = TerrainTileOf(terrain, x, y);
		tile.z_min = static_cast<int8_t>(z_min);
		tile.z_max = static_cast<int8_t>(z_max);
		tile.incline = incline;
	}
}

/** The common ramp setup: bounds hugging the line's rows (when asked), terrain at one height outside the columns set by the caller, prices that tell the parts apart. */
static void SetUpRamps(LineRampCase &c, int plane, bool hug)
{
	c.request = BaseRequest();
	c.request.rail_piece = 1;
	c.request.turn_cost = 0;
	c.request.slope_piece = 11;
	c.request.plane = plane;
	if (hug) {
		const std::vector<int> rows = TrackRows(c.request);
		c.request.min.y = static_cast<int16_t>(*std::min_element(rows.begin(), rows.end()));
		c.request.max.y = static_cast<int16_t>(*std::max_element(rows.begin(), rows.end()));
	}
	c.terrain = FlatTerrainFor(c.request, 5);
	c.terrain.build_on_slopes = false;
	c.terrain.foundation_cost = 7;
	for (JunctionTerrainTile &tile : c.terrain.tiles) tile.z_min = tile.z_max = static_cast<int8_t>(plane);
	c.request.terrain = &c.terrain;
}

/** A ridge across the field: from x = 40 the ground is at 0, rises over x = 33, is level at 1 over x = 32 and 31, and falls over x = 30. */
static void SetUpRidge(LineRampCase &c)
{
	SetUpRamps(c, 0, true);
	SetRampColumn(c.terrain, 33, 0, 1, DiagDirection::NE);
	SetRampColumn(c.terrain, 32, 1, 1, DiagDirection::Invalid);
	SetRampColumn(c.terrain, 31, 1, 1, DiagDirection::Invalid);
	SetRampColumn(c.terrain, 30, 0, 1, DiagDirection::SW);
}

/** A valley across the field: the ground is at 1, falls over x = 33, is at 0 over x = 32 to 30, and rises over x = 29. */
static void SetUpValley(LineRampCase &c)
{
	SetUpRamps(c, 1, true);
	SetRampColumn(c.terrain, 33, 0, 1, DiagDirection::SW);
	for (int x = 30; x <= 32; x++) SetRampColumn(c.terrain, x, 0, 0, DiagDirection::Invalid);
	SetRampColumn(c.terrain, 29, 0, 1, DiagDirection::NE);
}

/** The cost of the straight route over a profile of two ramp tiles: 21 tiles and 20 rail pieces per track, the ramps paying clear cost and slope_piece. */
static Money RampRouteCost(const LineRampCase &c)
{
	return Money(c.request.line.tracks) * (21 * 5 + 20 + 2 * 11);
}

TEST_CASE("FindJunctionLineRoute: climbs over a ridge", "[junction][linerouter]")
{
	LineRampCase c;
	SetUpRidge(c);
	const JunctionLineRouteResult result = FindJunctionLineRoute(c.request);
	CheckRoute(c.request, result);
	REQUIRE(result.route.has_value());
	CHECK(result.route->moves == std::string(20, 'F'));
	CHECK(result.cost == RampRouteCost(c));
}

TEST_CASE("FindJunctionLineRoute: descends into a valley", "[junction][linerouter]")
{
	LineRampCase c;
	SetUpRamps(c, 1, true);
	SetRampColumn(c.terrain, 33, 0, 1, DiagDirection::SW);
	SetRampColumn(c.terrain, 32, 0, 0, DiagDirection::Invalid);
	SetRampColumn(c.terrain, 31, 0, 0, DiagDirection::Invalid);
	SetRampColumn(c.terrain, 30, 0, 1, DiagDirection::NE);
	const JunctionLineRouteResult result = FindJunctionLineRoute(c.request);
	CheckRoute(c.request, result);
	REQUIRE(result.route.has_value());
	CHECK(result.route->moves == std::string(20, 'F'));
	CHECK(result.cost == RampRouteCost(c));
}

/** The valley of "descends into a valley", on a game that allows building on slopes (D152a). */
static void SetUpDescentOnSlopes(LineRampCase &c)
{
	SetUpRamps(c, 1, true);
	c.terrain.build_on_slopes = true;
	SetRampColumn(c.terrain, 33, 0, 1, DiagDirection::SW);
	SetRampColumn(c.terrain, 32, 0, 0, DiagDirection::Invalid);
	SetRampColumn(c.terrain, 31, 0, 0, DiagDirection::Invalid);
	SetRampColumn(c.terrain, 30, 0, 1, DiagDirection::NE);
}

TEST_CASE("FindJunctionLineRoute: descends with build_on_slopes", "[junction][linerouter]")
{
	LineRampCase c;
	SetUpDescentOnSlopes(c);
	const JunctionLineRouteResult result = FindJunctionLineRoute(c.request);
	CheckRoute(c.request, result);
	REQUIRE(result.route.has_value());
	CHECK(result.route->moves == std::string(20, 'F'));
	CHECK(result.cost == RampRouteCost(c));
}

/** Two slope tiles, down along the track then up again, with the ground at 1 on both sides: the game puts no level foundation under track along an incline, so the track ramps down and up however cheap a foundation is (D152b). */
TEST_CASE("FindJunctionLineRoute: track along an incline is always a ramp", "[junction][linerouter]")
{
	LineRampCase c;
	SetUpRamps(c, 1, true);
	c.terrain.build_on_slopes = true;
	SetRampColumn(c.terrain, 33, 0, 1, DiagDirection::SW);
	SetRampColumn(c.terrain, 32, 0, 1, DiagDirection::NE);
	c.terrain.foundation_cost = 1;
	c.request.slope_piece = 100;
	const JunctionLineRouteResult result = FindJunctionLineRoute(c.request);
	CheckRoute(c.request, result);
	REQUIRE(result.route.has_value());
	CHECK(result.cost == Money(int64_t{c.request.line.tracks} * (21 * 5 + 20 + 2 * 100)));
}

TEST_CASE("FindJunctionLineRoute: no turn on a ramp", "[junction][linerouter]")
{
	LineRampCase c;
	SetUpRamps(c, 0, false);
	c.request.goals = {{{20, 16}, Trackdir::X_NE}};
	SetRampColumn(c.terrain, 30, 0, 1, DiagDirection::NE);
	SetRampColumn(c.terrain, 29, 1, 1, DiagDirection::Invalid);
	SetRampColumn(c.terrain, 28, 0, 1, DiagDirection::SW);
	const JunctionLineRouteResult result = FindJunctionLineRoute(c.request);
	CheckRoute(c.request, result);
	REQUIRE(result.route.has_value());
	CHECK(result.route->moves.find_first_of("LR") != std::string::npos);
	for (const JunctionRaster &raster : RasterizeJunctionLine(c.request.line, *result.route)) {
		for (size_t i = 1; i < raster.pieces.size(); i++) {
			const JunctionPiece &piece = raster.pieces[i];
			if (c.terrain.At(piece.tile.x, piece.tile.y)->incline == DiagDirection::Invalid) continue;
			CHECK(IsDiagonalTrackdir(piece.trackdir));
			CHECK(TrackdirToDirection(piece.trackdir) == TrackdirToDirection(raster.pieces[i - 1].trackdir));
		}
	}
}

TEST_CASE("FindJunctionLineRoute: a bridge or slopes by price", "[junction][linerouter]")
{
	for (Money slope_piece : {Money(0), Money(1000)}) {
		LineRampCase c;
		SetUpValley(c);
		EnableBridges(c.request);
		c.request.slope_piece = slope_piece;
		const JunctionLineRouteResult result = FindJunctionLineRoute(c.request);
		INFO("slope_piece " << slope_piece);
		CheckRoute(c.request, result);
		REQUIRE(result.route.has_value());
		CHECK((result.route->moves.find('B') != std::string::npos) == (slope_piece == 1000));
	}
}

TEST_CASE("JunctionCanonicalIncline: rotations", "[junction][linerouter]")
{
	for (Slope slope : {SLOPE_NE, SLOPE_SE, SLOPE_SW, SLOPE_NW}) {
		for (uint8_t rotation = 0; rotation < 4; rotation++) {
			for (bool mirrored : {false, true}) {
				const TileIndexDiffC canonical = InverseOrientJunctionVector(TileIndexDiffCByDiagDir(GetInclinedSlopeDirection(slope)), rotation, mirrored);
				const DiagDirection result = JunctionCanonicalIncline(slope, rotation, mirrored);
				INFO("slope " << static_cast<int>(slope) << " rotation " << static_cast<int>(rotation) << " mirrored " << mirrored);
				REQUIRE(result != DiagDirection::Invalid);
				CHECK(TileIndexDiffCByDiagDir(result).x == canonical.x);
				CHECK(TileIndexDiffCByDiagDir(result).y == canonical.y);
			}
		}
	}
	for (Slope slope : {SLOPE_FLAT, SLOPE_N, SLOPE_EW, SLOPE_STEEP_N, SLOPE_NWS}) {
		CHECK(JunctionCanonicalIncline(slope, 1, true) == DiagDirection::Invalid);
	}
}

TEST_CASE("FindJunctionLineRoute: ramp routes are as cheap as an uninformed search", "[junction][linerouter][slow]")
{
	for (int which = 0; which < 5; which++) {
		LineRampCase c;
		switch (which) {
			case 0: SetUpRidge(c); break;
			case 1: SetUpValley(c); break;
			case 2:
				SetUpRamps(c, 0, false);
				c.request.goals = {{{20, 16}, Trackdir::X_NE}};
				SetRampColumn(c.terrain, 30, 0, 1, DiagDirection::NE);
				SetRampColumn(c.terrain, 29, 1, 1, DiagDirection::Invalid);
				SetRampColumn(c.terrain, 28, 0, 1, DiagDirection::SW);
				break;
			case 3:
				SetUpValley(c);
				EnableBridges(c.request);
				c.request.slope_piece = 1000;
				break;
			default:
				SetUpDescentOnSlopes(c);
				break;
		}
		c.request.use_heuristic = true;
		const JunctionLineRouteResult informed = FindJunctionLineRoute(c.request);
		c.request.use_heuristic = false;
		const JunctionLineRouteResult uninformed = FindJunctionLineRoute(c.request);
		INFO("case " << which << " informed " << informed.route.has_value() << " cost " << informed.cost << " uninformed " << uninformed.route.has_value() << " cost " << uninformed.cost);
		REQUIRE(informed.route.has_value());
		REQUIRE(uninformed.route.has_value());
		/* D161: pruning is approximate for lines whose tracks can collide; an overestimating heuristic shows as a costlier informed route. */
		CHECK(informed.cost <= uninformed.cost);
	}
}

/** A ridge across the field for tunnels, on ground at `base`: the entry portal at x = 33 rises towards the line's travel, the exit portal at x = 26 falls away, and the six tiles between them are level three higher, so that the only way over is a tunnel at the starting height. */
static void SetUpTunnelRidge(LineRampCase &c, int base)
{
	SetUpRamps(c, base, true);
	c.request.slope_piece = 1000;
	c.request.tunnel_price = [](uint n) { return Money(10 * (n + 2)); };
	SetRampColumn(c.terrain, 33, base, base + 1, DiagDirection::NE);
	for (int x = 27; x <= 32; x++) SetRampColumn(c.terrain, x, base + 3, base + 3, DiagDirection::Invalid);
	SetRampColumn(c.terrain, 26, base, base + 1, DiagDirection::SW);
}

/** Add a perpendicular tunnel under x = 29 at the given floor height, its portals just outside the line's rows. */
static void AddCrossingTunnel(LineRampCase &c, JunctionLayout &layout, int z)
{
	JunctionBridge tunnel{{29, static_cast<int16_t>(c.request.min.y - 1)}, {29, static_cast<int16_t>(c.request.max.y + 1)}, true, static_cast<int8_t>(z)};
	layout.bridges.push_back(tunnel);
	c.request.existing = &layout;
}

/** The number of T moves in a route's program. */
static size_t TunnelMoves(const JunctionLineRouteResult &result)
{
	return result.route.has_value() ? static_cast<size_t>(std::count(result.route->moves.begin(), result.route->moves.end(), 'T')) : 0;
}

TEST_CASE("FindJunctionLineRoute: tunnels through a ridge", "[junction][linerouter]")
{
	LineRampCase c;
	SetUpTunnelRidge(c, 0);
	const JunctionLineRouteResult result = FindJunctionLineRoute(c.request);
	INFO(result.error);
	REQUIRE(result.route.has_value());
	CHECK(TunnelMoves(result) == 1);
	const std::vector<JunctionRaster> rasters = RasterizeJunctionLine(c.request.line, *result.route);
	REQUIRE(rasters.size() == 2);
	for (const JunctionRaster &raster : rasters) {
		INFO(raster.error);
		REQUIRE(raster.error.empty());
		REQUIRE(raster.bridges.size() == 1);
		CHECK(raster.bridges[0].tunnel);
		CHECK(raster.bridges[0].z == 0);
		CHECK(raster.bridges[0].start.x == rasters[0].bridges[0].start.x);
		CHECK(raster.bridges[0].end.x == rasters[0].bridges[0].end.x);
	}
}

TEST_CASE("FindJunctionLineRoute: tunnels cross at different depths", "[junction][linerouter]")
{
	LineRampCase c;
	JunctionLayout layout;
	SetUpTunnelRidge(c, 1);
	AddCrossingTunnel(c, layout, 0);
	const JunctionLineRouteResult result = FindJunctionLineRoute(c.request);
	INFO(result.error);
	REQUIRE(result.route.has_value());
	CHECK(TunnelMoves(result) == 1);
}

TEST_CASE("FindJunctionLineRoute: tunnels at the same depth don't cross", "[junction][linerouter]")
{
	LineRampCase c;
	JunctionLayout layout;
	SetUpTunnelRidge(c, 0);
	AddCrossingTunnel(c, layout, 0);
	const JunctionLineRouteResult result = FindJunctionLineRoute(c.request);
	INFO(result.error);
	CHECK(TunnelMoves(result) == 0);
	CHECK_FALSE(result.route.has_value()); // The ridge can't be climbed.
}

TEST_CASE("FindJunctionLineRoute: a tunnel longer than the limit is refused", "[junction][linerouter]")
{
	LineRampCase c;
	SetUpTunnelRidge(c, 0);
	c.request.max_tunnel_length = 5;
	const JunctionLineRouteResult result = FindJunctionLineRoute(c.request);
	CHECK(TunnelMoves(result) == 0);
}

TEST_CASE("RasterizeJunctionLine: a tunnel move", "[junction][linerouter]")
{
	const JunctionLineSpec line = ParseLineNotation("L_R").spec;
	const std::vector<JunctionRaster> rasters = RasterizeJunctionLine(line, {{40, 20}, Trackdir::X_NE, "FT3F"});
	REQUIRE(rasters.size() == 2);
	for (const JunctionRaster &raster : rasters) {
		INFO(raster.error);
		REQUIRE(raster.error.empty());
		REQUIRE(raster.bridges.size() == 1);
		CHECK(raster.bridges[0].tunnel);
		CHECK(std::abs(raster.bridges[0].start.x - raster.bridges[0].end.x) == 4);
	}

	CHECK_FALSE(RasterizeJunctionLine(line, {{40, 20}, Trackdir::X_NE, "FLT2"})[0].error.empty());
}

TEST_CASE("FindJunctionLineRoute: tunnel routes are as cheap as an uninformed search", "[junction][linerouter][slow]")
{
	for (int which = 0; which < 4; which++) {
		LineRampCase c;
		JunctionLayout layout;
		switch (which) {
			case 0: SetUpTunnelRidge(c, 0); break;
			case 1:
				SetUpTunnelRidge(c, 1);
				AddCrossingTunnel(c, layout, 0);
				break;
			case 2:
				SetUpTunnelRidge(c, 0);
				AddCrossingTunnel(c, layout, 0);
				break;
			default:
				SetUpTunnelRidge(c, 0);
				c.request.max_tunnel_length = 5;
				break;
		}
		c.request.use_heuristic = true;
		const JunctionLineRouteResult informed = FindJunctionLineRoute(c.request);
		c.request.use_heuristic = false;
		const JunctionLineRouteResult uninformed = FindJunctionLineRoute(c.request);
		INFO("case " << which << " informed " << informed.route.has_value() << " cost " << informed.cost << " uninformed " << uninformed.route.has_value() << " cost " << uninformed.cost);
		REQUIRE(informed.route.has_value() == uninformed.route.has_value());
		CHECK(informed.route.has_value() == (which < 2)); // The ridge can't be climbed, so the other cases have no route.
		/* D161: pruning is approximate for lines whose tracks can collide; an overestimating heuristic shows as a costlier informed route. */
		CHECK(informed.cost <= uninformed.cost);
	}
}

/** The tracks of the main line of the benchmark: straight X track on rows 8 and 10 across the whole 44 x 20 box. */
static JunctionLayout BenchMainLine()
{
	JunctionLayout layout;
	for (int y : {8, 10}) {
		for (int x = 0; x <= 43; x++) layout.tracks.push_back({{static_cast<int16_t>(x), static_cast<int16_t>(y)}, TrackBits{Track::X}});
	}
	return layout;
}

/** Time FindJunctionLineRoute on a request (1 warm-up, 31 timed runs) and print one BENCH line; returns whether a route was found. */
static bool BenchRoute(const char *name, const JunctionLineRouteRequest &request)
{
	std::vector<long long> times;
	JunctionLineRouteResult result = FindJunctionLineRoute(request);
	for (int i = 0; i < 31; i++) {
		const auto begin = std::chrono::steady_clock::now();
		result = FindJunctionLineRoute(request);
		const auto end = std::chrono::steady_clock::now();
		times.push_back(std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count());
	}
	std::sort(times.begin(), times.end());
	std::cout << "BENCH " << name << " tracks=" << static_cast<int>(request.line.tracks) << " found=" << (result.route.has_value() ? 1 : 0) << " cost=" << result.cost << " median_us=" << times[times.size() / 2] << " max_us=" << times.back() << " expansions=" << result.expansions << "\n";
	return result.route.has_value();
}

/** A benchmark request in the 44 x 20 box: from {43, 5} heading X_NE to {0, 5} X_NE, or with `turn` to {20, 19} heading Y_SE. */
static JunctionLineRouteRequest BenchRequest(uint8_t tracks, bool turn)
{
	JunctionLineRouteRequest request = BaseRequest();
	request.line = {tracks, 1, 1};
	request.start = {43, 5};
	request.start_trackdir = Trackdir::X_NE;
	request.goals = {turn ? JunctionRouteEnd{{20, 19}, Trackdir::Y_SE} : JunctionRouteEnd{{0, 5}, Trackdir::X_NE}};
	request.min = {0, 0};
	request.max = {43, 19};
	request.curve_spacing = 4;
	request.turn_cost = 3;
	request.rail_piece = 1;
	return request;
}

/** One benchmark request with the terrain and layout it points at, set up in place. */
struct BenchCase {
	std::string name;
	JunctionLineRouteRequest request;
	JunctionTerrain terrain;
	JunctionLayout main_line;
	bool expect_route = true;
};

/** The benchmark cases in order; the names are those SetUpBenchCase knows. */
static const char *const _bench_case_names[] = {"flat_straight_1", "flat_turn_1", "flat_straight_2", "flat_turn_2", "obstacle_turn_1", "hills_turn_1", "hills_obstacle_turn_2", "no_route_1"};

/** Set up a benchmark case by name (see _bench_case_names). */
static void SetUpBenchCase(BenchCase &c, const std::string &name)
{
	c.name = name;
	LineRampCase tunnel_setup;
	SetUpTunnelRidge(tunnel_setup, 0); // Only for the tunnel price.

	if (name.starts_with("flat_")) {
		const bool turn = name.starts_with("flat_turn_");
		const uint8_t tracks = static_cast<uint8_t>(name.back() - '0');
		c.request = BenchRequest(tracks, turn);
		c.terrain = FlatTerrainFor(c.request, 5);
		c.request.terrain = &c.terrain;
	} else if (name == "obstacle_turn_1") {
		c.main_line = BenchMainLine();
		c.request = BenchRequest(1, true);
		c.terrain = FlatTerrainFor(c.request, 5);
		c.request.terrain = &c.terrain;
		c.request.existing = &c.main_line;
		EnableBridges(c.request);
		c.request.tunnel_price = tunnel_setup.request.tunnel_price;
	} else if (name == "hills_turn_1" || name == "hills_obstacle_turn_2") {
		/* The hills: the ridge as SetUpRidge sets it, the valley as SetUpValley sets it over four columns (fall, two level, rise). */
		const uint8_t tracks = static_cast<uint8_t>(name.back() - '0');
		c.request = BenchRequest(tracks, true);
		c.terrain = FlatTerrainFor(c.request, 5);
		c.terrain.build_on_slopes = false;
		c.terrain.foundation_cost = 7;
		for (JunctionTerrainTile &tile : c.terrain.tiles) tile.z_min = tile.z_max = 0;
		SetRampColumn(c.terrain, 33, 0, 1, DiagDirection::NE);
		SetRampColumn(c.terrain, 32, 1, 1, DiagDirection::Invalid);
		SetRampColumn(c.terrain, 31, 1, 1, DiagDirection::Invalid);
		SetRampColumn(c.terrain, 30, 0, 1, DiagDirection::SW);
		SetRampColumn(c.terrain, 13, 0, 1, DiagDirection::SW);
		SetRampColumn(c.terrain, 12, 0, 0, DiagDirection::Invalid);
		SetRampColumn(c.terrain, 11, 0, 0, DiagDirection::Invalid);
		SetRampColumn(c.terrain, 10, 0, 1, DiagDirection::NE);
		c.request.terrain = &c.terrain;
		c.request.slope_piece = 11;
		EnableBridges(c.request);
		c.request.tunnel_price = tunnel_setup.request.tunnel_price;
		if (tracks == 2) {
			c.main_line = BenchMainLine();
			c.request.existing = &c.main_line;
			c.expect_route = false; // Reported, not checked.
		}
	} else {
		c.request = BenchRequest(1, true);
		c.terrain = FlatTerrainFor(c.request, 5);
		c.request.terrain = &c.terrain;
		for (int x = 0; x <= 43; x++) c.request.blocked.push_back({static_cast<int16_t>(x), 12});
		c.expect_route = false;
	}
}

TEST_CASE("FindJunctionLineRoute: route cost benchmark", "[.bench][junction][linerouter]")
{
	for (const char *name : _bench_case_names) {
		BenchCase c;
		SetUpBenchCase(c, name);
		const bool found = BenchRoute(c.name.c_str(), c.request);
		if (c.expect_route) CHECK(found);
	}
}

TEST_CASE("FindJunctionLineRoute: a straight route on costly ground expands little", "[junction][linerouter]")
{
	BenchCase c;
	SetUpBenchCase(c, "flat_straight_1");
	const JunctionLineRouteResult result = FindJunctionLineRoute(c.request);
	INFO("expansions " << result.expansions << " error " << result.error);
	REQUIRE(result.route.has_value());
	CHECK(result.expansions <= 200);
}

TEST_CASE("FindJunctionLineRoute: a turn costs at least its turns", "[junction][linerouter]")
{
	JunctionLineRouteRequest request = BaseRequest();
	request.line = {1, 1, 1};
	request.start = {40, 20};
	request.start_trackdir = Trackdir::X_NE;
	request.goals = {{{25, 30}, Trackdir::Y_SE}};
	request.turn_cost = 50;
	request.rail_piece = 1;
	const JunctionTerrain terrain = FlatTerrainFor(request, 0);
	request.terrain = &terrain;
	const JunctionLineRouteResult result = FindJunctionLineRoute(request);
	INFO("expansions " << result.expansions << " error " << result.error);
	REQUIRE(result.route.has_value());
	CHECK(result.expansions <= 400);
}

TEST_CASE("FindJunctionLineRoute: costly routes are as cheap as an uninformed search", "[junction][linerouter][slow]")
{
	for (const char *name : {"flat_straight_1", "flat_turn_1", "obstacle_turn_1", "hills_turn_1", "flat_straight_2", "flat_turn_2", "hills_obstacle_turn_2"}) {
		BenchCase c;
		SetUpBenchCase(c, name);
		c.request.use_heuristic = true;
		const JunctionLineRouteResult informed = FindJunctionLineRoute(c.request);
		c.request.use_heuristic = false;
		const JunctionLineRouteResult uninformed = FindJunctionLineRoute(c.request);
		INFO(name << " informed " << informed.route.has_value() << " cost " << informed.cost << " uninformed " << uninformed.route.has_value() << " cost " << uninformed.cost);
		REQUIRE(informed.route.has_value() == uninformed.route.has_value());
		/* D161: pruning is approximate for lines whose tracks can collide; an overestimating heuristic shows as a costlier informed route. */
		CHECK(informed.cost <= uninformed.cost);
	}
}

/** The summed units of track 0 of a route. */
static uint Track0Units(const JunctionLineRouteRequest &request, const JunctionLineRouteResult &result)
{
	uint units = 0;
	const std::vector<JunctionRaster> rasters = RasterizeJunctionLine(request.line, *result.route);
	for (const JunctionPiece &piece : rasters[0].pieces) units += piece.units;
	return units;
}

/** A single track straight request on flat ground from {40, 20} heading X_NE to the given x on row 20, with the piece, turn and terrain costs set. */
static JunctionLineRouteRequest ResourceRequest(int goal_x)
{
	JunctionLineRouteRequest request = BaseRequest();
	request.line = {1, 1, 1};
	request.goals = {{{static_cast<int16_t>(goal_x), 20}, Trackdir::X_NE}};
	request.rail_piece = 1;
	request.turn_cost = 3;
	return request;
}

/** The cost a route of a ResourceRequest must have without any resource cost: a piece cost for every piece but the first, a turn cost per L or R, and the clear cost once per tile. */
static Money PlainRouteCost(const JunctionLineRouteRequest &request, const JunctionLineRouteResult &result, Money clear_cost)
{
	const std::vector<JunctionRaster> rasters = RasterizeJunctionLine(request.line, *result.route);
	const std::vector<JunctionPiece> &pieces = rasters[0].pieces;
	std::set<std::pair<int, int>> tiles;
	for (const JunctionPiece &piece : pieces) tiles.emplace(piece.tile.x, piece.tile.y);
	Money cost = Money{request.rail_piece.base() * static_cast<int64_t>(pieces.size() - 1) + clear_cost.base() * static_cast<int64_t>(tiles.size())};
	for (char move : result.route->moves) {
		if (move == 'L' || move == 'R') cost += request.turn_cost;
	}
	return cost;
}

TEST_CASE("FindJunctionLineRoute: a resource cost steers the route", "[junction][linerouter]")
{
	JunctionLineRouteRequest request = ResourceRequest(20);
	const JunctionTerrain terrain = FlatTerrainFor(request, 2);
	request.terrain = &terrain;
	auto dear = [](const JunctionPiece &piece) { return piece.tile.y == 20 && piece.tile.x >= 27 && piece.tile.x <= 33; };
	request.resource_cost = [&](const JunctionPiece &piece, int8_t, JunctionResourceLevel level) { return Money(level == JunctionResourceLevel::Surface && dear(piece) ? 100 : 0); };
	const JunctionLineRouteResult result = FindJunctionLineRoute(request);
	INFO(result.error << " moves " << (result.route.has_value() ? result.route->moves : ""));
	REQUIRE(result.route.has_value());
	Money resource = 0;
	const std::vector<JunctionRaster> rasters = RasterizeJunctionLine(request.line, *result.route);
	for (const JunctionPiece &piece : rasters[0].pieces) {
		CHECK_FALSE(dear(piece));
		resource += request.resource_cost(piece, 0, JunctionResourceLevel::Surface);
	}
	CHECK(result.cost.base() == PlainRouteCost(request, result, 2).base() + resource.base());
}

TEST_CASE("FindJunctionLineRoute: a resource cost sees the direction", "[junction][linerouter]")
{
	JunctionLineRouteRequest request = ResourceRequest(20);
	const JunctionTerrain terrain = FlatTerrainFor(request, 2);
	request.terrain = &terrain;
	request.resource_cost = [](const JunctionPiece &piece, int8_t, JunctionResourceLevel) {
		return Money(piece.trackdir == Trackdir::X_NE && piece.tile.y == 20 && piece.tile.x >= 27 && piece.tile.x <= 33 ? 100 : 0);
	};
	const JunctionLineRouteResult north_east = FindJunctionLineRoute(request);
	REQUIRE(north_east.route.has_value());
	const std::vector<JunctionRaster> rasters = RasterizeJunctionLine(request.line, *north_east.route);
	for (const JunctionPiece &piece : rasters[0].pieces) {
		CHECK(request.resource_cost(piece, 0, JunctionResourceLevel::Surface).base() == 0);
	}
	CHECK(north_east.route->moves.find_first_of("LR") != std::string::npos);

	/* The same ground travelled the other way. */
	request.start = {20, 20};
	request.start_trackdir = Trackdir::X_SW;
	request.goals = {{{40, 20}, Trackdir::X_SW}};
	const JunctionLineRouteResult south_west = FindJunctionLineRoute(request);
	REQUIRE(south_west.route.has_value());
	CHECK(south_west.route->moves.find_first_of("LR") == std::string::npos);
	CHECK(south_west.cost.base() == PlainRouteCost(request, south_west, 2).base());
}

TEST_CASE("FindJunctionLineRoute: a resource cost on a bridge span", "[junction][linerouter]")
{
	LineBridgeCase c;
	SetUpRiver(c, "L_R", 3, false, false);
	const JunctionLineRouteResult plain = FindJunctionLineRoute(c.request);
	REQUIRE(plain.route.has_value());
	CHECK(plain.route->moves.find('B') != std::string::npos);

	c.request.resource_cost = [](const JunctionPiece &, int8_t, JunctionResourceLevel level) { return Money(level == JunctionResourceLevel::BridgeSpan ? 7 : 0); };
	const JunctionLineRouteResult dear = FindJunctionLineRoute(c.request);
	REQUIRE(dear.route.has_value());
	CHECK(dear.route->moves.find('B') != std::string::npos);
	int64_t middles = 0;
	const std::vector<JunctionRaster> rasters = RasterizeJunctionLine(c.request.line, *dear.route);
	for (const JunctionRaster &raster : rasters) {
		for (const JunctionBridge &bridge : raster.bridges) middles += std::abs(bridge.end.x - bridge.start.x) + std::abs(bridge.end.y - bridge.start.y) - 1;
	}
	CHECK(middles > 0);
	CHECK(dear.cost.base() == plain.cost.base() + 7 * middles);
}

TEST_CASE("FindJunctionLineRoute: a minimum length lengthens the route", "[junction][linerouter]")
{
	JunctionLineRouteRequest request = ResourceRequest(31);
	const JunctionLineRouteResult plain = FindJunctionLineRoute(request);
	REQUIRE(plain.route.has_value());
	CHECK(Track0Units(request, plain) < 32);

	request.min_units = 32;
	const JunctionLineRouteResult longer = FindJunctionLineRoute(request);
	INFO(longer.error);
	REQUIRE(longer.route.has_value());
	CHECK(Track0Units(request, longer) >= 32);
}

TEST_CASE("FindJunctionLineRoute: minimum length routes are as cheap as an uninformed search", "[junction][linerouter][slow]")
{
	for (uint tiles : {12u, 16u, 20u}) {
		JunctionLineRouteRequest request = ResourceRequest(31);
		request.min_units = 2 * tiles;
		request.max_expansions = 3000000; // An uninformed search with a minimum length needs far more than the default.
		request.use_heuristic = true;
		const JunctionLineRouteResult informed = FindJunctionLineRoute(request);
		request.use_heuristic = false;
		const JunctionLineRouteResult uninformed = FindJunctionLineRoute(request);
		INFO(tiles << " informed " << informed.route.has_value() << " cost " << informed.cost << " uninformed " << uninformed.route.has_value() << " cost " << uninformed.cost);
		REQUIRE(uninformed.error != "search limit");
		REQUIRE(informed.route.has_value() == uninformed.route.has_value());
		CHECK(informed.cost <= uninformed.cost);
	}
}

/** A request with movement rules: its terrain, when it has one, is the member's. */
struct RuleCase {
	JunctionLineRouteRequest request;
	JunctionTerrain terrain;
};

/** Track 0's pieces of a route, rasterized. */
static std::vector<JunctionPiece> RoutePieces(const JunctionLineRouteRequest &request, const JunctionLineRouteResult &result)
{
	REQUIRE(result.route.has_value());
	return RasterizeJunctionLine(request.line, *result.route)[0].pieces;
}

/** A single track request on flat ground from {40, 20} heading X_NE, to be given a goal and rules. */
static JunctionLineRouteRequest RuleRequest()
{
	JunctionLineRouteRequest request = BaseRequest();
	request.line = {1, 1, 1};
	request.rail_piece = 1;
	request.turn_cost = 3;
	return request;
}

/** The straight row the movement starts on (the siblings' route), tiles x from 40 down to 28. */
static bool OnStartRow(const JunctionPiece &piece)
{
	return piece.tile.y == 20 && piece.tile.x >= 28 && piece.tile.x <= 40 && piece.trackdir == Trackdir::X_NE;
}

/** The target row the goal lies on, tiles x from 32 down to 20 at y = 26. */
static bool OnTargetRow(const JunctionPiece &piece)
{
	return piece.tile.y == 26 && piece.tile.x >= 20 && piece.tile.x <= 32 && piece.trackdir == Trackdir::X_NE;
}

static void SetUpSplitCase(RuleCase &c)
{
	c.request = RuleRequest();
	c.request.goals = {{{30, 26}, Trackdir::Y_SE}};
	/* Ground on the start row costs more, so a route that is free to split does so at once. */
	c.terrain = FlatTerrainFor(c.request, 0);
	for (int x = c.request.min.x; x <= c.request.max.x; x++) TerrainTileOf(c.terrain, x, 20).clear_cost = 5;
	c.request.terrain = &c.terrain;
	c.request.rules.emplace();
	c.request.rules->shares_from = OnStartRow;
	c.request.rules->may_split = [](const JunctionPiece &piece) { return std::abs(piece.tile.x - 40) + std::abs(piece.tile.y - 20) >= 6; };
}

static void SetUpWaitCase(RuleCase &c)
{
	c.request = RuleRequest();
	c.request.goals = {{{20, 26}, Trackdir::X_NE}};
	c.request.rules.emplace();
	c.request.rules->shares_from = OnStartRow;
	c.request.rules->shares_to = OnTargetRow;
	c.request.rules->wait_units = 30;
}

static void SetUpBridgeCase(RuleCase &c)
{
	c.request = RuleRequest();
	EnableBridges(c.request);
	c.terrain = FlatTerrainFor(c.request, 0);
	for (int y = c.request.min.y; y <= c.request.max.y; y++) {
		for (int x = 30; x < 33; x++) TerrainTileOf(c.terrain, x, y).cls = JunctionTileClass::Water;
	}
	c.request.terrain = &c.terrain;
	c.request.rules.emplace();
	c.request.rules->shares_from = [](const JunctionPiece &piece) { return !piece.on_bridge && piece.tile.y == 20 && piece.tile.x >= 34 && piece.tile.x <= 40 && piece.trackdir == Trackdir::X_NE; };
	c.request.rules->plain_before_head = 1;
}

TEST_CASE("FindJunctionLineRoute: a movement splits only where allowed", "[junction][linerouter]")
{
	RuleCase c;
	SetUpSplitCase(c);
	const JunctionLineRouteResult result = FindJunctionLineRoute(c.request);
	INFO(result.error);
	const std::vector<JunctionPiece> pieces = RoutePieces(c.request, result);
	const auto split = std::ranges::find_if(pieces, [](const JunctionPiece &piece) { return !OnStartRow(piece); });
	REQUIRE(split != pieces.end());
	CHECK(std::abs(split->tile.x - 40) + std::abs(split->tile.y - 20) >= 6);
}

TEST_CASE("FindJunctionLineRoute: waiting room before the merge", "[junction][linerouter]")
{
	RuleCase c;
	SetUpWaitCase(c);
	const JunctionLineRouteResult result = FindJunctionLineRoute(c.request);
	INFO(result.error);
	const std::vector<JunctionPiece> pieces = RoutePieces(c.request, result);
	uint private_units = 0;
	int phase = 0;
	for (const JunctionPiece &piece : pieces) {
		if (phase == 0 && !OnStartRow(piece)) {
			phase = 1;
			private_units += piece.units;
		} else if (phase == 1) {
			if (OnTargetRow(piece)) {
				phase = 2;
			} else {
				private_units += piece.units;
			}
		}
	}
	CHECK(phase == 2);
	CHECK(private_units >= c.request.rules->wait_units);
}

TEST_CASE("FindJunctionLineRoute: no rejoining and no parting", "[junction][linerouter]")
{
	/* The cheapest path runs straight along row 20; the shared track has a gap there, so it would split and come back. */
	{
		auto shared = [](const JunctionPiece &piece) { return piece.tile.y == 20 && ((piece.tile.x >= 36 && piece.tile.x <= 40) || (piece.tile.x >= 22 && piece.tile.x <= 28)) && piece.trackdir == Trackdir::X_NE; };
		RuleCase c;
		c.request = RuleRequest();
		c.request.goals = {{{20, 20}, Trackdir::X_NE}};
		c.request.rules.emplace();
		c.request.rules->shares_from = shared;
		const JunctionLineRouteResult result = FindJunctionLineRoute(c.request);
		INFO(result.error);
		const std::vector<JunctionPiece> pieces = RoutePieces(c.request, result);
		bool split = false;
		for (const JunctionPiece &piece : pieces) {
			if (!shared(piece)) split = true;
			if (split) CHECK_FALSE(shared(piece));
		}
	}

	/* The sibling's route to the Out port runs through the straight row, but the goal lies beyond it: a merge could not be left. */
	{
		auto merged = [](const JunctionPiece &piece) { return piece.tile.y == 20 && piece.tile.x >= 26 && piece.tile.x <= 30 && piece.trackdir == Trackdir::X_NE; };
		RuleCase c;
		c.request = RuleRequest();
		c.request.goals = {{{20, 20}, Trackdir::X_NE}};
		c.request.rules.emplace();
		c.request.rules->shares_from = [](const JunctionPiece &piece) { return piece.tile.y == 20 && piece.tile.x >= 36 && piece.tile.x <= 40 && piece.trackdir == Trackdir::X_NE; };
		c.request.rules->shares_to = merged;
		const JunctionLineRouteResult result = FindJunctionLineRoute(c.request);
		INFO(result.error);
		const std::vector<JunctionPiece> pieces = RoutePieces(c.request, result);
		bool merge = false;
		for (const JunctionPiece &piece : pieces) {
			if (merged(piece)) merge = true;
			if (merge) CHECK(merged(piece));
		}
	}
}

TEST_CASE("FindJunctionLineRoute: a signal-legal piece before a bridge after a split", "[junction][linerouter]")
{
	RuleCase c;
	SetUpBridgeCase(c);
	const JunctionLineRouteResult result = FindJunctionLineRoute(c.request);
	INFO(result.error);
	const std::vector<JunctionPiece> pieces = RoutePieces(c.request, result);
	const auto shared = c.request.rules->shares_from;
	const auto split = std::ranges::find_if(pieces, [&](const JunctionPiece &piece) { return !shared(piece); });
	REQUIRE(split != pieces.end());
	const auto head = std::ranges::find_if(split, pieces.end(), [](const JunctionPiece &piece) { return piece.on_bridge; });
	REQUIRE(head != pieces.end());
	CHECK(std::ranges::any_of(split, head, [](const JunctionPiece &piece) { return !piece.on_bridge; }));
}

TEST_CASE("FindJunctionLineRoute: rules need one track", "[junction][linerouter]")
{
	JunctionLineRouteRequest request = BaseRequest();
	request.rules.emplace();
	JunctionLineRouteResult result = FindJunctionLineRoute(request);
	CHECK_FALSE(result.route.has_value());
	CHECK(result.error == "movement rules need one track and no prefix");

	request = RuleRequest();
	request.goals = {{{20, 20}, Trackdir::X_NE}};
	request.prefix = "F";
	request.rules.emplace();
	result = FindJunctionLineRoute(request);
	CHECK_FALSE(result.route.has_value());
	CHECK(result.error == "movement rules need one track and no prefix");
}

TEST_CASE("FindJunctionLineRoute: no rules is unchanged", "[junction][linerouter]")
{
	/* The cost and expansions of the single-track benchmark cases before the rules existed. E3a: the D110 bridge move adds successors; route and cost unchanged (obstacle_turn_1 was 3787 before). */
	struct Expected {
		const char *name;
		bool found;
		int64_t cost;
		uint expansions;
	};
	for (const Expected &expected : {Expected{"flat_straight_1", true, 263, 43}, Expected{"flat_turn_1", true, 233, 904}, Expected{"obstacle_turn_1", true, 293, 3801}, Expected{"hills_turn_1", true, 255, 2125}, Expected{"no_route_1", false, 0, 32645}}) {
		BenchCase c;
		SetUpBenchCase(c, expected.name);
		REQUIRE_FALSE(c.request.rules.has_value());
		const JunctionLineRouteResult result = FindJunctionLineRoute(c.request);
		INFO(expected.name);
		CHECK(result.route.has_value() == expected.found);
		CHECK(result.cost.base() == expected.cost);
		CHECK(result.expansions == expected.expansions);
	}
}

TEST_CASE("FindJunctionLineRoute: rule routes are as cheap as an uninformed search", "[junction][linerouter][slow]")
{
	for (int which = 0; which < 3; which++) {
		RuleCase c;
		if (which == 0) SetUpSplitCase(c);
		if (which == 1) SetUpWaitCase(c);
		if (which == 2) SetUpBridgeCase(c);
		c.request.max_expansions = 3000000;
		c.request.use_heuristic = true;
		const JunctionLineRouteResult informed = FindJunctionLineRoute(c.request);
		c.request.use_heuristic = false;
		const JunctionLineRouteResult uninformed = FindJunctionLineRoute(c.request);
		INFO(which << " informed " << informed.route.has_value() << " cost " << informed.cost << " uninformed " << uninformed.route.has_value() << " cost " << uninformed.cost);
		REQUIRE(uninformed.error != "search limit");
		REQUIRE(informed.route.has_value() == uninformed.route.has_value());
		CHECK(informed.cost <= uninformed.cost);
	}
}

/** A request for one track heading along -x from (35, 20) to (20, 20) in a flat box. */
static JunctionLineRouteRequest SingleTrackRequest()
{
	JunctionLineRouteRequest request = BaseRequest();
	request.line = {1, 0, 1};
	request.start = {35, 20};
	return request;
}

TEST_CASE("FindJunctionLineRoute: a route may start on an identical existing piece", "[junction][linerouter]")
{
	JunctionLayout existing;
	existing.tracks.push_back({{35, 20}, TrackBits{Track::X}});
	JunctionLineRouteRequest request = SingleTrackRequest();
	request.existing = &existing;
	const JunctionLineRouteResult result = FindJunctionLineRoute(request);
	INFO(result.error);
	REQUIRE(result.route.has_value());
	CHECK(result.route->start.x == 35);
	CHECK(result.route->moves == std::string(15, 'F'));

	JunctionLayout different;
	different.tracks.push_back({{35, 20}, TrackBits{Track::Y}});
	request.existing = &different;
	CHECK_FALSE(FindJunctionLineRoute(request).route.has_value());
}

TEST_CASE("FindJunctionLineRoute: a route may end on an identical existing piece", "[junction][linerouter]")
{
	JunctionLayout existing;
	existing.tracks.push_back({{20, 20}, TrackBits{Track::X}});
	JunctionLineRouteRequest request = SingleTrackRequest();
	request.existing = &existing;
	const JunctionLineRouteResult result = FindJunctionLineRoute(request);
	INFO(result.error);
	REQUIRE(result.route.has_value());
	CHECK(result.route->moves == std::string(15, 'F'));

	JunctionLayout different;
	different.tracks.push_back({{20, 20}, TrackBits{Track::Y}});
	request.existing = &different;
	CHECK_FALSE(FindJunctionLineRoute(request).route.has_value());
}

/** An existing straight X row at y = 20 over the given x range. */
static JunctionLayout ExistingRow(int min_x, int max_x)
{
	JunctionLayout row;
	for (int x = min_x; x <= max_x; x++) row.tracks.push_back({{static_cast<int16_t>(x), 20}, TrackBits{Track::X}});
	return row;
}

TEST_CASE("FindJunctionLineRoute: a route may split off identical existing track with a curve", "[junction][linerouter]")
{
	const JunctionLayout existing = ExistingRow(30, 40);
	JunctionLineRouteRequest request = SingleTrackRequest();
	request.goals = {{{34, 15}, Trackdir::Y_NW}};
	request.existing = &existing;
	const JunctionLineRouteResult result = FindJunctionLineRoute(request);
	INFO(result.error);
	REQUIRE(result.route.has_value());
	CHECK(result.route->start.x == 35);
	REQUIRE_FALSE(result.route->moves.empty());
	CHECK(result.route->moves[0] != 'F');
}

TEST_CASE("FindJunctionLineRoute: a route may merge onto existing track with a curve", "[junction][linerouter]")
{
	const JunctionLayout existing = ExistingRow(10, 24);
	JunctionLineRouteRequest request = SingleTrackRequest();
	request.start = {22, 28};
	request.start_trackdir = Trackdir::Y_NW;
	/* A window of the row's pieces, as the T map's goals are: the merge tile (22, 20) is a goal tile. */
	request.goals = {{{22, 20}, Trackdir::X_NE}, {{21, 20}, Trackdir::X_NE}, {{20, 20}, Trackdir::X_NE}};
	request.existing = &existing;
	const JunctionLineRouteResult result = FindJunctionLineRoute(request);
	INFO(result.error);
	REQUIRE(result.route.has_value());
	size_t curves_on_row = 0;
	for (const JunctionRaster &raster : RasterizeJunctionLine(request.line, *result.route)) {
		for (const JunctionPiece &piece : raster.pieces) {
			if (piece.tile.y == 20 && piece.tile.x >= 10 && piece.tile.x <= 24 && TrackdirToTrack(piece.trackdir) != Track::X) curves_on_row++;
		}
	}
	CHECK(curves_on_row > 0);

	/* A clashing piece that no goal piece follows is still rejected: crossing the row is not a merge, so the route goes round its end. */
	request.goals = {{{22, 12}, Trackdir::Y_NW}};
	const JunctionLineRouteResult round = FindJunctionLineRoute(request);
	INFO(round.error);
	REQUIRE(round.route.has_value());
	for (const JunctionRaster &raster : RasterizeJunctionLine(request.line, *round.route)) {
		for (const JunctionPiece &piece : raster.pieces) CHECK_FALSE((piece.tile.y == 20 && piece.tile.x >= 10 && piece.tile.x <= 24));
	}
}

/** A one-track request that replays the prefix B3 from an axis start and ends on the far head, over a flat terrain. */
struct CheckedPrefixCase {
	JunctionLineRouteRequest request;
	JunctionTerrain terrain;
	std::vector<JunctionPiece> pieces; ///< The prefix's pieces: the start, the near head, three middle tiles, the far head.
};

static void SetUpCheckedPrefix(CheckedPrefixCase &c)
{
	c.request = SingleTrackRequest();
	c.request.prefix = "B3";
	const std::vector<JunctionRaster> rasters = RasterizeJunctionLine(c.request.line, {c.request.start, c.request.start_trackdir, c.request.prefix});
	REQUIRE(rasters[0].error.empty());
	c.pieces = rasters[0].pieces;
	REQUIRE(c.pieces.size() == 6);
	c.request.goals = {{c.pieces.back().tile, c.pieces.back().trackdir}};
	c.terrain = FlatTerrainFor(c.request, 0);
	c.request.terrain = &c.terrain;
}

TEST_CASE("a checked prefix bridge over a house is refused", "[junction][checkedprefix]")
{
	CheckedPrefixCase c;
	SetUpCheckedPrefix(c);
	TerrainTileOf(c.terrain, c.pieces[3].tile.x, c.pieces[3].tile.y).cls = JunctionTileClass::Blocked;

	c.request.check_prefix = true;
	CHECK_FALSE(FindJunctionLineRoute(c.request).route.has_value());
	c.request.check_prefix = false;
	const JunctionLineRouteResult frozen = FindJunctionLineRoute(c.request);
	INFO(frozen.error);
	REQUIRE(frozen.route.has_value());
	CHECK(frozen.route->moves == "B3");
}

TEST_CASE("a checked prefix bridge head on a house is refused", "[junction][checkedprefix]")
{
	CheckedPrefixCase c;
	SetUpCheckedPrefix(c);
	TerrainTileOf(c.terrain, c.pieces[5].tile.x, c.pieces[5].tile.y).cls = JunctionTileClass::Blocked;

	c.request.check_prefix = true;
	CHECK_FALSE(FindJunctionLineRoute(c.request).route.has_value());
}

TEST_CASE("a checked prefix that is legal routes as before", "[junction][checkedprefix]")
{
	CheckedPrefixCase c;
	SetUpCheckedPrefix(c);

	c.request.check_prefix = false;
	const JunctionLineRouteResult before = FindJunctionLineRoute(c.request);
	REQUIRE(before.route.has_value());
	c.request.check_prefix = true;
	const JunctionLineRouteResult checked = FindJunctionLineRoute(c.request);
	INFO(checked.error);
	REQUIRE(checked.route.has_value());
	CHECK(checked.route->moves == before.route->moves);
	CHECK(checked.cost == before.cost);
}
