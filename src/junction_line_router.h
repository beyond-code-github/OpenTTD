/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_line_router.h Router for a multi-track line on flat ground (LP4). */

#ifndef JUNCTION_LINE_ROUTER_H
#define JUNCTION_LINE_ROUTER_H

#include "economy_type.h"
#include "junction_line.h"
#include "junction_router.h"

#include <optional>
#include <string>
#include <vector>

/** A request to route a line on flat ground (LP4). */
struct JunctionLineRouteRequest {
	JunctionLineSpec line;
	TileIndexDiffC start; ///< Track 0's first piece.
	Trackdir start_trackdir;
	std::vector<JunctionRouteEnd> goals; ///< Exact end pieces for track 0.
	TileIndexDiffC min; ///< Every piece of every track lies inside, inclusive.
	TileIndexDiffC max;
	std::vector<TileIndexDiffC> blocked; ///< Tiles no track may use.
	uint curve_spacing = 0; ///< As JunctionRouteRequest: half-units of straight run needed before a turn.
	uint s_bend_window = 0; ///< As JunctionRouteRequest.
	Money rail_piece = 1; ///< Per piece, summed over all tracks.
	Money turn_cost = 0; ///< Once per line turn (D135 c).
	uint max_expansions = 200000;
};

struct JunctionLineRouteResult {
	std::optional<JunctionLineRoute> route;
	std::string error;
};

JunctionLineRouteResult FindJunctionLineRoute(const JunctionLineRouteRequest &request);

#endif /* JUNCTION_LINE_ROUTER_H */
