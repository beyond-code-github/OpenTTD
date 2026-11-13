/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_track_geometry.cpp Axis and bend geometry shared by the placement, render and signal stages. */

#include "../stdafx.h"

#include "junction_track_geometry.h"

#include "../track_func.h"
#include "../direction_func.h"

#include "../safeguards.h"

/** The moves of a bend from its axis piece: the first turn, the halves after the first, the closing turn (DESIGN-G1 §11.8). A hand of +1 is a clockwise turn. */
std::string JunctionBendMoves(int hand, int halves, bool corner)
{
	std::string moves(1, hand > 0 ? 'L' : 'R');
	moves.append(halves - 1, 'F');
	moves += ((corner ? hand : -hand) > 0) ? 'L' : 'R';
	return moves;
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
