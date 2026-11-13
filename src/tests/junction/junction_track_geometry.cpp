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

#include <map>
#include <set>

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

TEST_CASE("bend tile forms predict the raster", "[junction][geometry]")
{
	auto offsets = [](int heading, int hand, int length, bool corner) {
		const int halves = corner ? 2 * length + 1 : 2 * length;
		const JunctionRaster raster = RasterizeJunctionRoute({{0, 0}, JUNCTION_AXIS_TRACKDIR[heading], JunctionBendMoves(hand, halves, corner)});
		REQUIRE(raster.error.empty());
		REQUIRE(static_cast<int>(raster.pieces.size()) == halves + 2);
		std::vector<std::pair<int, int>> result;
		for (int i = 1; i <= halves; i++) result.emplace_back(raster.pieces[i].tile.x - raster.pieces[1].tile.x, raster.pieces[i].tile.y - raster.pieces[1].tile.y);
		return result;
	};
	for (bool corner : {false, true}) {
		for (int heading = 0; heading < 4; heading++) {
			for (int hand : {-1, 1}) {
				std::vector<std::vector<std::pair<int, int>>> raster; ///< Index L - 1.
				for (int length = 1; length <= 64; length++) raster.push_back(offsets(heading, hand, length, corner));
				for (int length = 1; length <= 63; length++) {
					const std::vector<JunctionBendTileForm> forms = JunctionBendTileForms(heading, hand, length, corner);
					INFO("corner " << corner << " heading " << heading << " hand " << hand << " L " << length);
					REQUIRE(forms.size() == raster[length - 1].size());
					const int n = static_cast<int>(forms.size());
					for (int j = 0; j < n; j++) {
						for (int later = length + 1; later <= 64; later++) {
							const std::vector<std::pair<int, int>> &at = raster[later - 1];
							const int index = forms[j].tail ? static_cast<int>(at.size()) - 1 - (n - 1 - j) : j;
							INFO("L' " << later << " tile " << j);
							REQUIRE(index >= 0);
							CHECK(forms[j].dx0 + forms[j].dx1 * later == at[index].first);
							CHECK(forms[j].dy0 + forms[j].dy1 * later == at[index].second);
						}
					}
				}
			}
		}
	}
}

TEST_CASE("S-bend columns are affine in the column", "[junction][geometry]")
{
	static constexpr int VX[] = {0, 1, 0, -1};
	static constexpr int VY[] = {-1, 0, 1, 0};
	/** The rows of one column of a raster: least, greatest, and whether they are contiguous. */
	struct Column {
		int lo = 1000, hi = -1000;
		std::set<int> rows;
		bool contiguous() const { return !this->rows.empty() && static_cast<int>(this->rows.size()) == this->hi - this->lo + 1; }
	};
	for (int heading = 0; heading < 4; heading++) {
		for (int hand : {-1, 1}) {
			const int side = (heading + hand + 4) % 4;
			std::vector<std::map<int, Column>> by_length(65); ///< Index L.
			for (int length = 2; length <= 64; length++) {
				const JunctionRaster raster = RasterizeJunctionRoute({{0, 0}, JUNCTION_AXIS_TRACKDIR[heading], JunctionBendMoves(hand, 2 * length, false)});
				REQUIRE(raster.error.empty());
				REQUIRE(static_cast<int>(raster.pieces.size()) == 2 * length + 2);
				for (int i = 1; i <= 2 * length; i++) {
					const int x = raster.pieces[i].tile.x - raster.pieces[1].tile.x;
					const int y = raster.pieces[i].tile.y - raster.pieces[1].tile.y;
					Column &c = by_length[length][x * VX[side] + y * VY[side]];
					c.lo = std::min(c.lo, y);
					c.hi = std::max(c.hi, y);
					c.rows.insert(y);
				}
			}
			const std::map<int, Column> &big = by_length[64];
			REQUIRE(big.count(1) != 0);
			REQUIRE(big.count(2) != 0);
			const int s = big.at(2).lo - big.at(1).lo;
			const int lo1 = big.at(1).lo;
			const int hi1 = big.at(1).hi;
			/* T(L): the largest t with columns 1..t contiguous and on the diagonal. */
			std::vector<int> reach(65, 0);
			for (int length = 2; length <= 64; length++) {
				int t = 1;
				while (true) {
					const auto found = by_length[length].find(t);
					if (found == by_length[length].end()) break;
					const Column &c = found->second;
					if (!c.contiguous() || c.lo != lo1 + s * (t - 1) || c.hi != hi1 + s * (t - 1)) break;
					t++;
				}
				reach[length] = t - 1;
			}
			const int t1 = reach[64] - reach[63];
			const int t0 = reach[64] - 64 * t1;
			INFO("TABLE heading " << heading << " hand " << hand << " s " << s << " T0 " << t0 << " T1 " << t1 << " lo1 " << lo1 << " hi1 " << hi1);
			for (int length = 2; length <= 64; length++) {
				INFO("L " << length);
				CHECK(reach[length] == t0 + t1 * length);
				CHECK(by_length[length].at(1).lo == lo1);
				CHECK(by_length[length].at(1).hi == hi1);
			}
			const JunctionSBendColumns form = JunctionSBendColumnForm(heading, hand);
			CHECK(form.lo == lo1);
			CHECK(form.hi == hi1);
			CHECK(form.s == s);
			CHECK(form.t0 == t0);
			CHECK(form.t1 == t1);
		}
	}
}
