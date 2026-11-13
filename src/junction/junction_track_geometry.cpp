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
