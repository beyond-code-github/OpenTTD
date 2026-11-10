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

#include <functional>
#include <optional>
#include <string>
#include <vector>

/** Where a routed piece sits, for negotiated resource costs (DESIGN-O4 revision 3). */
enum class JunctionResourceLevel : uint8_t {
	Surface, ///< Track on the ground: plain or ramp.
	Head, ///< A bridge head or tunnel portal.
	BridgeSpan, ///< A bridge's middle piece.
	TunnelBore, ///< A tunnel's middle piece.
};

/** Where a movement may split from and merge into its siblings' current routes (DESIGN-O4 revision 4, D168). Only for single-track requests. */
struct JunctionMovementRules {
	std::function<bool(const JunctionPiece &piece)> shares_from; ///< The piece (same tile and trackdir) lies on the current route of a movement with the same In port.
	std::function<bool(const JunctionPiece &piece)> shares_to; ///< The piece lies on the current route of a movement with the same Out port.
	std::function<bool(const JunctionPiece &piece)> may_split; ///< The first private piece after shared track may be this one (coverage mask, D108); unset means always.
	std::function<bool(const JunctionPiece &piece)> may_merge; ///< The first piece on a sibling's route after private track may be this one (prio mask); unset means always.
	uint wait_units = 0; ///< Private units (JunctionPiece::units) needed between the split and the merge (G7).
	uint plain_before_head = 0; ///< Signal-legal private pieces needed between the split and a bridge or tunnel head, and between a far head and the merge (D113).
};

/** A prio carried by this line (DESIGN-O4 revision 5): its last `units` units before the end are straight axis pieces with the parallel piece on `side` also the line's own track. */
struct JunctionPrioWindow {
	uint units = 0; ///< Prio coverage plus one segment, in JunctionPiece::units (as CheckWaitingRoom counts).
	int8_t side = 0; ///< +1 or -1: the companion tile is at tile + side along the axis perpendicular to travel.
	int end_lo = 0; ///< The window's last piece lies at an along-travel coordinate (x for X-axis travel, y for Y) in [end_lo, end_hi].
	int end_hi = 0;
};

/** A request to route a line at one plane (LP4, LP5). */
struct JunctionLineRouteRequest {
	JunctionLineSpec line;
	TileIndexDiffC start; ///< Track 0's first piece.
	Trackdir start_trackdir;
	std::vector<JunctionRouteEnd> starts; ///< When not empty, the route may start at any of these (track 0's first piece and trackdir) and `start`/`start_trackdir` are ignored (DESIGN-O4).
	std::vector<JunctionRouteEnd> goals; ///< Exact end pieces for track 0; may be empty when there are waypoints (D150).
	std::vector<TileIndexDiffC> waypoints; ///< Tiles track 0 passes through, in order, on a piece that is not a bridge middle tile (D134 a, D150). With no goals the route ends with the move that passes the last one.
	std::string prefix; ///< Frozen moves from the start (D134 b, D150): replayed before the search, their pieces are the line's own track and set the curve state; not re-checked against terrain or obstacles.
	TileIndexDiffC min; ///< Every piece of every track lies inside, inclusive.
	TileIndexDiffC max;
	std::vector<TileIndexDiffC> blocked; ///< Tiles no track or bridge may use.
	const JunctionTerrain *terrain = nullptr; ///< When set, every piece's tile must be usable at plane, and its CostAt is paid once per tile (D147).
	int plane = 0; ///< Track 0's start height; every track starts at it (D152).
	const JunctionLayout *existing = nullptr; ///< When set, its tracks and bridge heads are obstacles (D147).
	uint curve_spacing = 0; ///< As JunctionRouteRequest: half-units of straight run needed before a turn.
	uint s_bend_window = 0; ///< As JunctionRouteRequest.
	Money rail_piece = 1; ///< Per piece, summed over all tracks.
	Money turn_cost = 0; ///< Once per line turn (D135 c).
	Money slope_piece = 0; ///< Added for each ramp piece, per track (D95, D152).
	std::function<bool(uint)> bridge_span_ok; ///< By middle-tile count; bridges are tried only when this and bridge_price are set and max_bridge_middle > 0 (D149).
	std::function<Money(uint)> bridge_price; ///< Per track and bridge, by middle-tile count. The heuristic's floor covers any price (O4h).
	uint max_bridge_middle = 0; ///< Largest n tried for B<n>.
	std::function<Money(uint)> tunnel_price; ///< Per track and tunnel, by middle-tile count; tunnels are tried only when set (D155). The heuristic's floor covers any price (O4h).
	uint max_tunnel_length = 64; ///< Largest middle-tile count (the game's max_tunnel_length).
	std::function<Money(const JunctionPiece &piece, int8_t z, JunctionResourceLevel level)> resource_cost; ///< Extra cost per piece of every track, from the negotiated search (DESIGN-O4 revision 3); must be >= 0; unset means 0. z is the track's height (bridge spans: the track's height at the heads; tunnel bores: the tunnel's floor).
	uint min_units = 0; ///< Track 0 needs at least this many units (JunctionPiece::units, as CheckWaitingRoom counts them) from its first piece, prefix included, before a goal counts (waiting room, bays, prio coverage).
	std::optional<JunctionMovementRules> rules; ///< When set, the route obeys them (DESIGN-O4 revision 4).
	std::optional<JunctionPrioWindow> prio_window; ///< When set, the route carries one prio window before its goal (DESIGN-O4 revision 5); single-track requests only.
	uint max_expansions = 200000;
	bool use_heuristic = true; ///< Test seam: false makes the search uninformed (h = 0).
};

struct JunctionLineRouteResult {
	std::optional<JunctionLineRoute> route; ///< Its moves are the request's prefix followed by the searched moves (D150).
	std::string error;
	Money cost = 0; ///< The route's total cost (pieces, turns, terrain).
	uint expansions = 0; ///< Nodes expanded by the search (O4r, O4h).
	size_t prio_end = 0; ///< Index in route pieces of the window's last piece, when prio_window was set.
};

JunctionLineRouteResult FindJunctionLineRoute(const JunctionLineRouteRequest &request);

#endif /* JUNCTION_LINE_ROUTER_H */
