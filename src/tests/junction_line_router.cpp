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
#include "../track_func.h"

#include <algorithm>
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
					if (informed.route.has_value() && uninformed.route.has_value()) CHECK(informed.cost == uninformed.cost);
				}
			}
		}
	}
}
