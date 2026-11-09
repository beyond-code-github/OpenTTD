/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_line_router.cpp Test functionality from junction_line_router. */

#include "../stdafx.h"

#include "../3rdparty/catch2/catch.hpp"

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
