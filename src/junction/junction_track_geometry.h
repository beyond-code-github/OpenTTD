/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_track_geometry.h Axis and bend geometry shared by the placement, render and signal stages (DESIGN-G1 §11.8). */

#ifndef JUNCTION_TRACK_GEOMETRY_H
#define JUNCTION_TRACK_GEOMETRY_H

#include "../direction_type.h"
#include "../track_type.h"

#include <string>
#include <vector>

/** The axis piece of each heading: 0 north, 1 east, 2 south, 3 west. */
constexpr Trackdir JUNCTION_AXIS_TRACKDIR[4] = {Trackdir::Y_NW, Trackdir::X_SW, Trackdir::Y_SE, Trackdir::X_NE};

/** The moves of a bend from its axis piece: the first turn, the halves after the first, the closing turn (DESIGN-G1 §11.8). A hand of +1 is a clockwise turn. */
std::string JunctionBendMoves(int hand, int halves, bool corner);

/** One tile of a bend's raster as an affine form of the bend's length L: a head tile's offset from the bend's first tile is constant; a tail tile's is (dx0 + dx1 L, dy0 + dy1 L), counted by its index from the raster's end. */
struct JunctionBendTileForm {
	bool tail;
	int dx0, dx1, dy0, dy1;
};

/** Per tile of the raster of a bend of length L (an S-bend: 2L halves; a Corner: 2L + 1), its form, read from the rasters at L and L + 1. */
std::vector<JunctionBendTileForm> JunctionBendTileForms(int heading, int hand, int length, bool corner);

/** The rows of an S-bend's sideways column 1 and how they move per column (DESIGN-G3 §G3.4a): lo(t) = lo + s (t - 1), hi(t) = hi + s (t - 1) for t = 1..t0 + t1 L. */
struct JunctionSBendColumns {
	int lo, hi, s, t0, t1;
};

/** The column form of the S-bend of 2L halves starting on the given heading with the given hand, read from the rasters. */
JunctionSBendColumns JunctionSBendColumnForm(int heading, int hand);

/** Whether a track touches the given tile edge. */
bool JunctionTrackTouches(Track track, DiagDirection edge);

/** The track that touches both edges. */
Track JunctionCornerTrack(DiagDirection a, DiagDirection b);

/**
 * Whether two unrelated pieces may stand on one tile: only as the parallel diagonal pair Upper|Lower or Left|Right, never as an at-grade X|Y diamond (D182, D217).
 * @param a The tracks of one piece on the tile.
 * @param b The tracks of the other piece on the tile.
 * @return True when (a | b) is exactly TRACK_BIT_HORZ or TRACK_BIT_VERT and the two share no track.
 */
bool JunctionTracksMayShareTile(TrackBits a, TrackBits b);

#endif /* JUNCTION_TRACK_GEOMETRY_H */
