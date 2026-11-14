/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_track_geometry.cpp Axis and bend geometry shared by the placement, render and signal stages. */

#include "../stdafx.h"

#include "junction_track_geometry.h"
#include "junction_route.h"

#include "../track_func.h"
#include "../direction_func.h"

#include <algorithm>
#include <climits>
#include <map>
#include <set>

#include "../safeguards.h"

/** The moves of a bend from its axis piece: the first turn, the halves after the first, the closing turn (DESIGN-G1 §11.8). A hand of +1 is a clockwise turn. */
std::string JunctionBendMoves(int hand, int halves, bool corner)
{
	assert(halves >= 1);
	std::string moves(1, hand > 0 ? 'L' : 'R');
	moves.append(halves - 1, 'F');
	moves += ((corner ? hand : -hand) > 0) ? 'L' : 'R';
	return moves;
}

/**
 * The offsets of the tiles of a bend's raster from its first tile, one per half piece.
 * @param heading The heading of the axis piece before the bend.
 * @param hand The bend's hand.
 * @param length The bend's length L.
 * @param corner Whether the bend is a Corner.
 * @return The offsets, in raster order.
 */
static std::vector<std::pair<int, int>> BendRasterOffsets(int heading, int hand, int length, bool corner)
{
	const int halves = corner ? 2 * length + 1 : 2 * length;
	const JunctionRaster raster = RasterizeJunctionRoute({{0, 0}, JUNCTION_AXIS_TRACKDIR[heading], JunctionBendMoves(hand, halves, corner)});
	std::vector<std::pair<int, int>> offsets;
	if (!raster.error.empty() || static_cast<int>(raster.pieces.size()) != halves + 2) return offsets;
	for (int i = 1; i <= halves; i++) offsets.emplace_back(raster.pieces[i].tile.x - raster.pieces[1].tile.x, raster.pieces[i].tile.y - raster.pieces[1].tile.y);
	return offsets;
}

/** Per tile of the raster of a bend of length L (an S-bend: 2L halves; a Corner: 2L + 1), its form, read from the rasters at L and L + 1. */
std::vector<JunctionBendTileForm> JunctionBendTileForms(int heading, int hand, int length, bool corner)
{
	const std::vector<std::pair<int, int>> now = BendRasterOffsets(heading, hand, length, corner);
	const std::vector<std::pair<int, int>> next = BendRasterOffsets(heading, hand, length + 1, corner);
	std::vector<JunctionBendTileForm> forms;
	if (now.empty() || next.empty()) return forms;
	const int n = static_cast<int>(now.size());
	const int n_next = static_cast<int>(next.size());
	for (int j = 0; j < n; j++) {
		if (j < n_next && next[j] == now[j]) {
			forms.push_back({false, now[j].first, 0, now[j].second, 0});
			continue;
		}
		const int i = n - 1 - j;
		const std::pair<int, int> &moved = next[n_next - 1 - i];
		const int dx = moved.first - now[j].first;
		const int dy = moved.second - now[j].second;
		forms.push_back({true, now[j].first - length * dx, dx, now[j].second - length * dy, dy});
	}
	return forms;
}

/** The column form of the bend of 2L halves (an S-bend) or 2L + 1 (a Corner) starting on the given heading with the given hand, read from the rasters. */
static JunctionSBendColumns BendColumnForm(int heading, int hand, bool corner)
{
	static constexpr int VX[] = {0, 1, 0, -1};
	static constexpr int VY[] = {-1, 0, 1, 0};
	const int side = (heading + hand + 4) % 4;

	/** The rows of one column: least, greatest and the number of distinct rows. */
	struct Column {
		int lo = INT_MAX, hi = INT_MIN;
		std::set<int> rows;
	};
	auto columns_at = [&](int length) {
		std::map<int, Column> columns;
		for (const std::pair<int, int> &tile : BendRasterOffsets(heading, hand, length, corner)) {
			Column &column = columns[tile.first * VX[side] + tile.second * VY[side]];
			column.lo = std::min(column.lo, tile.second);
			column.hi = std::max(column.hi, tile.second);
			column.rows.insert(tile.second);
		}
		return columns;
	};
	const std::map<int, Column> big = columns_at(64);
	if (big.count(1) == 0 || big.count(2) == 0) return {0, 0, 0, 0, 0};
	JunctionSBendColumns form{big.at(1).lo, big.at(1).hi, big.at(2).lo - big.at(1).lo, 0, 0};

	/* T(L): the largest t with columns 1..t contiguous and on the diagonal. */
	auto reach = [&](int length) {
		const std::map<int, Column> columns = columns_at(length);
		int t = 1;
		for (auto found = columns.find(t); found != columns.end(); found = columns.find(++t)) {
			const Column &column = found->second;
			if (static_cast<int>(column.rows.size()) != column.hi - column.lo + 1 || column.lo != form.lo + form.s * (t - 1) || column.hi != form.hi + form.s * (t - 1)) break;
		}
		return t - 1;
	};
	form.t1 = reach(64) - reach(63);
	form.t0 = reach(64) - 64 * form.t1;
	return form;
}

/** The column form of the S-bend of 2L halves starting on the given heading with the given hand, read from the rasters. */
JunctionSBendColumns JunctionSBendColumnForm(int heading, int hand)
{
	return BendColumnForm(heading, hand, false);
}

/** The column form of the Corner of 2L + 1 halves starting on the given heading with the given hand, read from the rasters, as JunctionSBendColumnForm's. */
JunctionSBendColumns JunctionCornerColumnForm(int heading, int hand)
{
	return BendColumnForm(heading, hand, true);
}

/** The tracks on each tile of a bend's raster, by offset from its first tile. */
static std::map<std::pair<int, int>, TrackBits> BendRasterTracks(int heading, int hand, int length, bool corner)
{
	const int halves = corner ? 2 * length + 1 : 2 * length;
	const JunctionRaster raster = RasterizeJunctionRoute({{0, 0}, JUNCTION_AXIS_TRACKDIR[heading], JunctionBendMoves(hand, halves, corner)});
	std::map<std::pair<int, int>, TrackBits> tiles;
	if (!raster.error.empty() || static_cast<int>(raster.pieces.size()) != halves + 2) return tiles;
	for (int i = 1; i <= halves; i++) {
		TrackBits &bits = tiles[{raster.pieces[i].tile.x - raster.pieces[1].tile.x, raster.pieces[i].tile.y - raster.pieces[1].tile.y}];
		bits = bits | TrackBits{TrackdirToTrack(raster.pieces[i].trackdir)};
	}
	return tiles;
}

/** The rows of the bend of length L (an S-bend: 2L halves; a Corner: 2L + 1) starting on the given heading with the given hand, read from the rasters. */
JunctionBendRows JunctionBendRowForms(int heading, int hand, bool corner)
{
	static constexpr int VX[] = {0, 1, 0, -1};
	static constexpr int VY[] = {-1, 0, 1, 0};
	const int side = (heading + hand + 4) % 4;
	const bool vertical = VX[heading] == 0;
	const int d = vertical ? VY[heading] : VY[side];
	const int slope = vertical ? VX[side] : VX[heading];

	/** A run of consecutive rows with one set of tile keys. */
	struct Segment {
		int lo, hi;
		std::vector<std::pair<int, TrackBits>> keys;
	};
	auto segments_at = [&](int length) {
		std::map<int, std::vector<std::pair<int, TrackBits>>> by_row;
		for (const auto &[offset, bits] : BendRasterTracks(heading, hand, length, corner)) {
			const int r = d * offset.second;
			by_row[r].emplace_back(offset.first - slope * r, bits);
		}
		std::vector<Segment> segments;
		for (auto &[r, keys] : by_row) {
			std::ranges::sort(keys, [](const auto &a, const auto &b) { return a.first < b.first; });
			if (!segments.empty() && segments.back().hi + 1 == r && segments.back().keys == keys) {
				segments.back().hi = r;
			} else {
				segments.push_back({r, r, keys});
			}
		}
		return segments;
	};
	const std::vector<Segment> big = segments_at(64);
	const std::vector<Segment> small = segments_at(63);
	if (big.empty() || big.size() != small.size()) return {0, {}};
	JunctionBendRows rows{d, {}};
	for (size_t s = 0; s < big.size(); s++) {
		if (big[s].keys != small[s].keys) return {0, {}};
		JunctionBendRowSegment segment;
		segment.lo1 = big[s].lo - small[s].lo;
		segment.lo0 = big[s].lo - 64 * segment.lo1;
		segment.hi1 = big[s].hi - small[s].hi;
		segment.hi0 = big[s].hi - 64 * segment.hi1;
		for (const auto &[c0, bits] : big[s].keys) segment.tiles.push_back({c0, slope, bits});
		rows.segments.push_back(std::move(segment));
	}
	return rows;
}

/** Whether a track touches the given tile edge. */
bool JunctionTrackTouches(Track track, DiagDirection edge)
{
	const Trackdir td = TrackToTrackdir(track);
	return TrackdirToExitdir(td) == edge || TrackdirToExitdir(ReverseTrackdir(td)) == edge;
}

/** The track that touches both edges. */
Track JunctionCornerTrack(DiagDirection a, DiagDirection b)
{
	for (Track track : {Track::Upper, Track::Lower, Track::Left, Track::Right}) {
		if (JunctionTrackTouches(track, a) && JunctionTrackTouches(track, b)) return track;
	}
	NOT_REACHED();
}

/**
 * Whether two unrelated pieces may stand on one tile: only as the parallel diagonal pair Upper|Lower or Left|Right, never as an at-grade X|Y diamond (D182, D217).
 * @param a The tracks of one piece on the tile.
 * @param b The tracks of the other piece on the tile.
 * @return True when (a | b) is exactly TRACK_BIT_HORZ or TRACK_BIT_VERT and the two share no track.
 */
bool JunctionTracksMayShareTile(TrackBits a, TrackBits b)
{
	const TrackBits both = a | b;
	return (both == TRACK_BIT_HORZ || both == TRACK_BIT_VERT) && (a & b).None();
}

/**
 * The gap between two unrelated pieces on one row.
 * @param low The tracks of the piece on the lower column.
 * @param high The tracks of the piece on the higher column.
 * @return Whether they may share a tile, and their least column difference when they do not.
 */
JunctionTileGap JunctionUnrelatedTileGap(TrackBits low, TrackBits high)
{
	return {JunctionTracksMayShareTile(low, high), (low.Any(DiagdirReachesTracks(ReverseDiagDir(DiagDirection::SW))) && high.Any(DiagdirReachesTracks(ReverseDiagDir(DiagDirection::NE)))) ? 2 : 1};
}
