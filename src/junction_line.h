/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_line.h Multi-track line geometry for the line painter (D112). */

#ifndef JUNCTION_LINE_H
#define JUNCTION_LINE_H

#include "junction_route.h"

#include <string>
#include <vector>

/** A multi-track line: N parallel tracks, `gap` tiles apart between neighbours (0 means adjacent). */
struct JunctionLineSpec {
	uint8_t tracks = 2;
	uint8_t gap = 1;
};

/**
 * A line program: a start (the first piece of the leftmost track, track 0) and moves, as JunctionRoute, applied to the whole line.
 * The line extends to the right of the start heading, so the other tracks are to the right of track 0.
 */
struct JunctionLineRoute {
	TileIndexDiffC start;
	Trackdir start_trackdir;
	std::string moves; ///< F, L, R only (no bridges in this step).
};

/** Rasterize a line program into one raster per track, outermost-left first; error text in the first raster when the line can't make a move. */
std::vector<JunctionRaster> RasterizeJunctionLine(const JunctionLineSpec &line, const JunctionLineRoute &route);

#endif /* JUNCTION_LINE_H */
