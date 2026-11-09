/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_line_router.h Router for a multi-track line at one plane (LP4, LP5). */

#ifndef JUNCTION_LINE_ROUTER_H
#define JUNCTION_LINE_ROUTER_H

#include "economy_type.h"
#include "junction_layout.h"
#include "junction_line.h"
#include "junction_router.h"
#include "junction_terrain.h"

#include <optional>
#include <string>
#include <vector>

/** A request to route a line at one plane (LP4, LP5). */
struct JunctionLineRouteRequest {
	JunctionLineSpec line;
	TileIndexDiffC start; ///< Track 0's first piece.
	Trackdir start_trackdir;
	std::vector<JunctionRouteEnd> goals; ///< Exact end pieces for track 0.
	TileIndexDiffC min; ///< Every piece of every track lies inside, inclusive.
	TileIndexDiffC max;
	std::vector<TileIndexDiffC> blocked; ///< Tiles no track may use.
	const JunctionTerrain *terrain = nullptr; ///< When set, every piece's tile must be usable at plane, and its CostAt is paid once per tile (D147).
	int plane = 0; ///< The level the line is built on; only used with a terrain.
	const JunctionLayout *existing = nullptr; ///< When set, its tracks and bridge heads are obstacles (D147).
	uint curve_spacing = 0; ///< As JunctionRouteRequest: half-units of straight run needed before a turn.
	uint s_bend_window = 0; ///< As JunctionRouteRequest.
	Money rail_piece = 1; ///< Per piece, summed over all tracks.
	Money turn_cost = 0; ///< Once per line turn (D135 c).
	uint max_expansions = 200000;
	bool use_heuristic = true; ///< Test seam: false makes the search uninformed (h = 0).
};

struct JunctionLineRouteResult {
	std::optional<JunctionLineRoute> route;
	std::string error;
	Money cost = 0; ///< The route's total cost (pieces, turns, terrain).
};

JunctionLineRouteResult FindJunctionLineRoute(const JunctionLineRouteRequest &request);

#endif /* JUNCTION_LINE_ROUTER_H */
