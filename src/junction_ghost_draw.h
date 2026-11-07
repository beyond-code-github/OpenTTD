/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_ghost_draw.h Drawing of the junction placement ghost, using the game's own sprite code. */

#ifndef JUNCTION_GHOST_DRAW_H
#define JUNCTION_GHOST_DRAW_H

#include "rail_type.h"
#include "track_type.h"

struct TileInfo;

void DrawRailTrackGhost(TileInfo *ti, TrackBits track, RailType rt);

#endif /* JUNCTION_GHOST_DRAW_H */
