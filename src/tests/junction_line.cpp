/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_line.cpp Test functionality from junction_line. */

#include "../stdafx.h"

#include "../3rdparty/catch2/catch.hpp"

#include "../junction_line.h"
#include "../direction_func.h"
#include "../track_func.h"

#include <map>
#include <string>

#include "../safeguards.h"

/** Perpendicular spacing between neighbouring tracks of a diagonal line, in diagonals (x + y or x - y), by gap. */
static const int DIAGONAL_SPACING[] = {1, 3, 4, 6};

static const Trackdir ALL_START_TRACKDIRS[] = {
	Trackdir::X_NE, Trackdir::X_SW, Trackdir::Y_NW, Trackdir::Y_SE,
	Trackdir::Upper_E, Trackdir::Lower_E, Trackdir::Upper_W, Trackdir::Lower_W,
	Trackdir::Left_N, Trackdir::Right_N, Trackdir::Left_S, Trackdir::Right_S,
};

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

static std::string Ascii(const std::vector<JunctionRaster> &rasters)
{
	JunctionLayout layout;
	for (const JunctionRaster &raster : rasters) AddRasterToLayout(layout, raster);
	NormaliseJunctionLayout(layout);
	return RenderJunctionAscii(layout);
}

/** Check one line: valid rasters, tracks never overlap, all tracks end parallel at the right spacing. */
static void CheckLine(const JunctionLineSpec &spec, Trackdir start, const char *moves, bool show)
{
	const std::vector<JunctionRaster> rasters = RasterizeJunctionLine(spec, {{20, 20}, start, moves});
	INFO("tracks " << int{spec.tracks} << " gap " << int{spec.gap} << " start " << static_cast<int>(start) << " moves " << moves);
	REQUIRE(rasters.size() == spec.tracks);
	for (const JunctionRaster &raster : rasters) {
		INFO(raster.error);
		REQUIRE(raster.error.empty());
		REQUIRE_FALSE(raster.pieces.empty());
	}
	if (show) WARN("tracks " << int{spec.tracks} << " gap " << int{spec.gap} << " moves " << moves << "\n" << Ascii(rasters));

	/* No tile is used by two tracks except as a parallel pair, and no piece twice. */
	std::map<std::pair<int, int>, TrackBits> tiles;
	for (const JunctionRaster &raster : rasters) {
		for (const JunctionPiece &piece : raster.pieces) {
			TrackBits &bits = tiles[{piece.tile.x, piece.tile.y}];
			const Track track = TrackdirToTrack(piece.trackdir);
			REQUIRE_FALSE(bits.Test(track));
			bits.Set(track);
			REQUIRE_FALSE(TracksOverlap(bits));
		}
	}

	/* Same final trackdir, equal progress along the heading, and the right distance across. */
	const JunctionPiece &first_end = rasters[0].pieces.back();
	const Direction heading = TrackdirToDirection(first_end.trackdir);
	const bool diagonal = !IsDiagonalTrackdir(first_end.trackdir); // OpenTTD calls X and Y "diagonal"; here it means the half-tile pieces.
	for (size_t i = 1; i < rasters.size(); i++) {
		const JunctionPiece &end = rasters[i].pieces.back();
		const JunctionPiece &prev = rasters[i - 1].pieces.back();
		CHECK(TrackdirToDirection(end.trackdir) == heading);
		const auto [px, py] = ExitPoint(prev);
		const auto [ex, ey] = ExitPoint(end);
		const int dx = ex - px;
		const int dy = ey - py;
		/* Along the heading the progress is equal; across it the distance is the spacing. */
		if (!diagonal) {
			const bool along_x = heading == Direction::NE || heading == Direction::SW;
			CHECK((along_x ? dx : dy) == 0);
			CHECK(std::abs(along_x ? dy : dx) == 2 * (spec.gap + 1));
		} else {
			const bool sum_is_along = heading == Direction::N || heading == Direction::S; // Along (-1,-1) or (1,1) the progress is x + y.
			const int sum = dx + dy;
			const int diff = dx - dy;
			CHECK((sum_is_along ? sum : diff) == 0);
			CHECK(std::abs(sum_is_along ? diff : sum) == 2 * DIAGONAL_SPACING[spec.gap]);
		}
	}
}

TEST_CASE("JunctionLine: straight", "[junction][line]")
{
	for (uint8_t gap = 0; gap < 3; gap++) {
		const JunctionLineSpec spec{2, gap};
		const std::vector<JunctionRaster> rasters = RasterizeJunctionLine(spec, {{20, 20}, Trackdir::X_NE, "FFFF"});
		REQUIRE(rasters.size() == 2);
		REQUIRE(rasters[0].error.empty());
		REQUIRE(rasters[1].error.empty());
		REQUIRE(rasters[0].pieces.size() == 5);
		REQUIRE(rasters[1].pieces.size() == 5);
		for (size_t i = 0; i < 5; i++) {
			CHECK(rasters[0].pieces[i].trackdir == Trackdir::X_NE);
			CHECK(rasters[1].pieces[i].trackdir == Trackdir::X_NE);
			CHECK(rasters[1].pieces[i].tile.x == rasters[0].pieces[i].tile.x);
			CHECK(rasters[1].pieces[i].tile.y - rasters[0].pieces[i].tile.y == gap + 1);
		}
		WARN("straight gap " << int{gap} << "\n" << Ascii(rasters));
	}

	/* Three tracks, other axis: leftmost first, so the right of a SE heading is the SW side. */
	const std::vector<JunctionRaster> rasters = RasterizeJunctionLine({3, 1}, {{20, 20}, Trackdir::Y_SE, "FFF"});
	REQUIRE(rasters.size() == 3);
	for (size_t i = 0; i < 3; i++) {
		REQUIRE(rasters[i].error.empty());
		CHECK(rasters[i].pieces.back().trackdir == Trackdir::Y_SE);
		CHECK(rasters[i].pieces[0].tile.x == 20 + static_cast<int>(i) * 2);
	}
}

TEST_CASE("JunctionLine: straight in all directions", "[junction][line]")
{
	for (Trackdir start : ALL_START_TRACKDIRS) {
		for (uint8_t gap = 0; gap < 4; gap++) CheckLine({2, gap}, start, "FFFFF", false);
		CheckLine({3, 1}, start, "FFFFF", false);
	}
}

TEST_CASE("JunctionLine: one left turn", "[junction][line]")
{
	for (uint8_t gap = 0; gap < 3; gap++) {
		CheckLine({2, gap}, Trackdir::X_NE, "FFLFFF", true);
	}

	/* Per-track piece counts of a bare turn: the outer track runs longer, by the table in junction_line.cpp. */
	const size_t expected_outer[] = {3, 4, 5};
	for (uint8_t gap = 0; gap < 3; gap++) {
		const std::vector<JunctionRaster> rasters = RasterizeJunctionLine({2, gap}, {{20, 20}, Trackdir::X_NE, "L"});
		REQUIRE(rasters.size() == 2);
		CHECK(rasters[0].pieces.size() == 2);
		CHECK(rasters[1].pieces.size() == expected_outer[gap]);
	}
}

TEST_CASE("JunctionLine: one right turn", "[junction][line]")
{
	for (uint8_t gap = 0; gap < 3; gap++) {
		CheckLine({2, gap}, Trackdir::X_NE, "FFRFFF", true);
	}
}

TEST_CASE("JunctionLine: S-bends and quarter turns", "[junction][line]")
{
	for (uint8_t gap = 0; gap < 3; gap++) {
		CheckLine({2, gap}, Trackdir::X_NE, "FFLFFRFF", true);
		CheckLine({2, gap}, Trackdir::X_NE, "FFRFFLFF", true);
		CheckLine({2, gap}, Trackdir::X_NE, "FLRF", true);
		CheckLine({2, gap}, Trackdir::X_NE, "FFLFLFF", true);
		CheckLine({2, gap}, Trackdir::X_NE, "FFRFRFF", true);
	}
}

TEST_CASE("JunctionLine: every start and turn combination", "[junction][line]")
{
	const char *programs[] = {"L", "R", "FLF", "FRF", "FLFF", "FFRF", "LL", "RR", "LLL", "RRR", "LR", "RL", "FLFRF", "FRFLF", "LFLF", "RFRF", "LLLL", "RRRR", "LLLLL", "RRRRR"};
	for (Trackdir start : ALL_START_TRACKDIRS) {
		for (uint8_t gap = 0; gap < 4; gap++) {
			for (const char *program : programs) CheckLine({2, gap}, start, program, false);
		}
		for (const char *program : programs) CheckLine({3, 1}, start, program, false);
		for (const char *program : programs) CheckLine({3, 0}, start, program, false);
	}
}

TEST_CASE("JunctionLine: errors", "[junction][line]")
{
	const std::vector<JunctionRaster> rasters = RasterizeJunctionLine({2, 1}, {{20, 20}, Trackdir::X_NE, "FB3"});
	REQUIRE(rasters.size() == 2);
	CHECK_FALSE(rasters[0].error.empty());
}
