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

/** The axis piece of each heading: 0 north, 1 east, 2 south, 3 west. */
constexpr Trackdir JUNCTION_AXIS_TRACKDIR[4] = {Trackdir::Y_NW, Trackdir::X_SW, Trackdir::Y_SE, Trackdir::X_NE};

/** The moves of a bend from its axis piece: the first turn, the halves after the first, the closing turn (DESIGN-G1 §11.8). A hand of +1 is a clockwise turn. */
std::string JunctionBendMoves(int hand, int halves, bool corner);

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
