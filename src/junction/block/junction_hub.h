/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_hub.h A hub composed from the core's placement and render and its oriented blocks (DESIGN-blocks §7.3). */

#ifndef JUNCTION_HUB_H
#define JUNCTION_HUB_H

#include "../junction_layout.h"
#include "../junction_route.h"
#include "../placement/junction_placement.h"
#include "../topology/junction_topology.h"

#include <string>
#include <vector>

struct JunctionSpec;

/** A hub composed from the core's placement and render and its oriented blocks (DESIGN-blocks §7.3). */
struct JunctionHub {
	JunctionLayout layout; ///< The core's render layout with every block route's raster added at its anchor.
	JunctionTopology topology; ///< The hub topology (every group at the bundle width) with its explicit lane paths (D301).
	std::vector<JunctionLanePath> paths; ///< topology.lane_paths.
	JunctionTrackGraph graph; ///< The signal graph: every block node, and edges chained through the core tracks.
	JunctionTrackPrograms programs; ///< Sized to the graph's edges; only an edge holding a block's doubled edge carries tokens.
	std::vector<JunctionTrackRoute> routes; ///< Per hub lane path, and per copy through a doubled block edge (copy 0 first), port to port.
	std::vector<JunctionPrioSpan> prio; ///< Every block's prio spans at the hub's tiles.
};

/**
 * Compose the hub (DESIGN-blocks §7.3).
 * @param hub_topology The topology with every group at the bundle width; its lane_paths are filled here.
 * @param topology, paths, graph, order, programs, placement The one-lane topology, its lane paths, and the core's bundle graph, order, programs and placement, as placed.
 * @param core_routes The core render's routes (one per edge and track).
 */
bool ComposeJunctionHub(const JunctionTopology &hub_topology, const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, const JunctionTrackOrder &order, const JunctionTrackPrograms &programs, const JunctionTrackPlacement &placement, const JunctionLayout &core_layout, const std::vector<JunctionTrackRoute> &core_routes, const JunctionSpec &spec, JunctionHub &hub, std::string *error);

#endif /* JUNCTION_HUB_H */
