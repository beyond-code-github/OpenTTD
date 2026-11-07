/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_router.h Junction route search. */

#ifndef JUNCTION_ROUTER_H
#define JUNCTION_ROUTER_H

#include "economy_type.h"
#include "junction_generator.h"
#include "junction_layout.h"
#include "junction_route.h"
#include "junction_terrain.h"
#include "map_type.h"
#include "track_type.h"

#include <functional>
#include <optional>
#include <vector>

/** One piece a route may start from or end on: an existing piece of the layout. */
struct JunctionRouteEnd {
	TileIndexDiffC tile;
	Trackdir trackdir;
};

/** What to route. */
struct JunctionRouteRequest {
	std::vector<JunctionRouteEnd> starts; ///< The route leaves from one of these (its first move continues from it).
	std::vector<JunctionRouteEnd> goals; ///< The route's last piece is one of these.
	TileIndexDiffC min; ///< Inclusive bounds for every piece and bridge tile the route makes.
	TileIndexDiffC max;
	uint curve_spacing; ///< CL in units (DeriveCurveSpacing).
	uint s_bend_window; ///< 2 * TL in units.
	std::function<bool(uint)> bridge_span_ok; ///< By middle-tile count, as JunctionSpec.
	uint max_bridge_middle; ///< Largest n tried for B<n>.
	JunctionUnitPrices prices;
	std::vector<TileIndexDiffC> crossable; ///< Tiles whose tracks will be lifted onto a bridge: treated as empty ground.
	Money crossable_cost; ///< Added for each piece made on a crossable tile.
	uint max_expansions = 200000;
	const JunctionTerrain *terrain = nullptr; ///< When set, tiles must suit the plane (D78).
	int plane = 0; ///< The level the junction is built on.
};

std::optional<JunctionRoute> FindJunctionRoute(const JunctionLayout &layout, const JunctionRouteRequest &request);

#endif /* JUNCTION_ROUTER_H */
