/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_track_geometry.cpp Test functionality from junction_track_geometry. */

#include "../../stdafx.h"

#include "../../3rdparty/catch2/catch.hpp"

#include "../../direction_func.h"
#include "../../junction/junction_route.h"
#include "../../junction/junction_track_geometry.h"

#include "../../safeguards.h"

TEST_CASE("unrelated pieces share a tile only as a parallel diagonal pair", "[junction][geometry]")
{
	CHECK(JunctionTracksMayShareTile(TrackBits{Track::Upper}, TrackBits{Track::Lower}));
	CHECK(JunctionTracksMayShareTile(TrackBits{Track::Lower}, TrackBits{Track::Upper}));
	CHECK(JunctionTracksMayShareTile(TrackBits{Track::Left}, TrackBits{Track::Right}));
	CHECK(JunctionTracksMayShareTile(TrackBits{Track::Right}, TrackBits{Track::Left}));
	CHECK_FALSE(JunctionTracksMayShareTile(TrackBits{Track::X}, TrackBits{Track::Y}));
	CHECK_FALSE(JunctionTracksMayShareTile(TrackBits{Track::Y}, TrackBits{Track::X}));
	CHECK_FALSE(JunctionTracksMayShareTile(TrackBits{Track::X}, TrackBits{Track::Upper}));
	CHECK_FALSE(JunctionTracksMayShareTile(TrackBits{Track::Upper}, TrackBits{Track::Left}));
	CHECK_FALSE(JunctionTracksMayShareTile(TrackBits{Track::X}, TrackBits{Track::X}));
}

/** The raster quantities of a bend that placement reads, by name. */
struct BendRasterQuantities {
	std::vector<std::pair<std::string, int>> values; ///< Scalar quantities, in a fixed order.
	std::vector<int> curve_units; ///< Units from the start of the axis piece before the bend, per curve.
};

/**
 * Read a bend raster as placement does: offsets of the axis piece before and the closing piece from the first half tile, the units up to the close, and the units at each curve.
 * @param raster The valid raster of a bend.
 * @return The quantities.
 */
static BendRasterQuantities ReadBendRaster(const JunctionRaster &raster)
{
	BendRasterQuantities q;
	int close_units = 0;
	for (size_t i = 0; i < raster.pieces.size(); i++) {
		if (i > 0) {
			const DirDiff diff = DirDifference(TrackdirToDirection(raster.pieces[i - 1].trackdir), TrackdirToDirection(raster.pieces[i].trackdir));
			if (diff == DirDiff::Left45 || diff == DirDiff::Right45) q.curve_units.push_back(close_units);
		}
		if (i + 1 < raster.pieces.size()) close_units += raster.pieces[i].units;
	}
	q.values.emplace_back("before_dx", raster.pieces[0].tile.x - raster.pieces[1].tile.x);
	q.values.emplace_back("before_dy", raster.pieces[0].tile.y - raster.pieces[1].tile.y);
	q.values.emplace_back("close_dx", raster.pieces.back().tile.x - raster.pieces[1].tile.x);
	q.values.emplace_back("close_dy", raster.pieces.back().tile.y - raster.pieces[1].tile.y);
	q.values.emplace_back("close_units", close_units);
	q.values.emplace_back("curve_count", static_cast<int>(q.curve_units.size()));
	return q;
}

TEST_CASE("bend rasters are affine in their length", "[junction][geometry]")
{
	for (bool corner : {false, true}) {
		for (int heading = 0; heading < 4; heading++) {
			for (int hand : {-1, 1}) {
				std::vector<BendRasterQuantities> by_length; ///< Index L - 1.
				for (int length = 1; length <= 64; length++) {
					const int halves = corner ? 2 * length + 1 : 2 * length;
					const JunctionRaster raster = RasterizeJunctionRoute({{0, 0}, JUNCTION_AXIS_TRACKDIR[heading], JunctionBendMoves(hand, halves, corner)});
					REQUIRE(raster.error.empty());
					REQUIRE(static_cast<int>(raster.pieces.size()) == halves + 2);
					by_length.push_back(ReadBendRaster(raster));
				}
				INFO("corner " << corner << " heading " << heading << " hand " << hand);
				for (int length = 3; length <= 64; length++) {
					const BendRasterQuantities &q = by_length[length - 1];
					const BendRasterQuantities &q1 = by_length[0];
					const BendRasterQuantities &q2 = by_length[1];
					INFO("L " << length);
					for (size_t i = 0; i < q.values.size(); i++) {
						INFO("quantity " << q.values[i].first);
						CHECK(q.values[i].second == q1.values[i].second + (length - 1) * (q2.values[i].second - q1.values[i].second));
					}
					REQUIRE(q.curve_units.size() == q1.curve_units.size());
					for (size_t i = 0; i < q.curve_units.size(); i++) {
						INFO("quantity curve_units " << i);
						CHECK(q.curve_units[i] == q1.curve_units[i] + (length - 1) * (q2.curve_units[i] - q1.curve_units[i]));
					}
				}
			}
		}
	}
}
