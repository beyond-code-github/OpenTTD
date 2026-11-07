/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_route.h Junction route programs, rasterizing and ASCII rendering. */

#ifndef JUNCTION_ROUTE_H
#define JUNCTION_ROUTE_H

#include "direction_type.h"
#include "junction_layout.h"
#include "map_type.h"
#include "track_type.h"

#include <string>
#include <vector>

/** One track piece of a route. Units: an axis piece is 2, a half piece 1, a bridge tile 2 (PLAN-1b §1). */
struct JunctionPiece {
	TileIndexDiffC tile;
	Trackdir trackdir;
	uint8_t units;
	bool on_bridge; ///< A bridge head or span tile; no track is laid here.
};

/** A route program: a start piece and a move string of F, L, R and B<n> (PLAN-1b §3). */
struct JunctionRoute {
	TileIndexDiffC start;
	Trackdir start_trackdir;
	std::string moves;
};

/** The rasterized route, or an error message when the program is invalid. */
struct JunctionRaster {
	std::vector<JunctionPiece> pieces;
	std::vector<JunctionBridge> bridges;
	std::string error; ///< Empty when valid.
};

Direction TrackdirToDirection(Trackdir td);
/**
 * Find the next trackdir for a move.
 * @param current The current trackdir.
 * @param diff The turn: Same, Left45 or Right45.
 * @param[out] next The next trackdir, when found.
 * @return Whether exactly one reachable trackdir has the wanted direction.
 */
bool JunctionNextTrackdir(Trackdir current, DirDiff diff, Trackdir &next);
JunctionRaster RasterizeJunctionRoute(const JunctionRoute &route);
void AddRasterToLayout(JunctionLayout &layout, const JunctionRaster &raster);
void NormaliseJunctionLayout(JunctionLayout &layout);
std::string RenderJunctionAscii(const JunctionLayout &layout);

#endif /* JUNCTION_ROUTE_H */
