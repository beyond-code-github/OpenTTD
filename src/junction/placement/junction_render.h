/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_render.h Render of a placement to routes and a track-only layout (DESIGN-G1 §11.10). */

#ifndef JUNCTION_RENDER_H
#define JUNCTION_RENDER_H

#include "junction_placement.h"
#include "../junction_layout.h"
#include "../junction_route.h"
#include "../topology/junction_topology.h"

#include <string>
#include <vector>

/**
 * The track-only layout of a seed placement (DESIGN-G1 §11.10): one route per lane path and copy, rasterized and merged.
 * @param error Receives the reason when the result is empty (no tracks).
 */
JunctionLayout RenderJunctionTrackPlacement(const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, const JunctionTrackStructure &structure, const JunctionTrackPrograms &programs, const JunctionTrackPlacement &placement, std::string *error, std::vector<JunctionTrackRoute> *routes = nullptr);

/** Add tracks to a tile of the layout. */
void AddJunctionTracks(JunctionLayout &layout, TileIndexDiffC tile, TrackBits tracks);

#endif /* JUNCTION_RENDER_H */
