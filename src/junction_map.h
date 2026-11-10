/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_map.h A junction's structure as a map in tile units, and its projection by routing (DESIGN-O4 revision 5). */

#ifndef JUNCTION_MAP_H
#define JUNCTION_MAP_H

#include "economy_type.h"
#include "junction_layout.h"
#include "junction_route.h"
#include "junction_router.h"
#include "map_type.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

/** What a map node is (DESIGN-O4 revision 5). */
enum class JunctionMapNodeKind : uint8_t { Port, Split, Merge };

/** A place on the map: the pieces it may be projected to, in tile units. */
struct JunctionMapNode {
	JunctionMapNodeKind kind;
	std::vector<JunctionRouteEnd> window; ///< The pieces this node may take; one piece when fixed.
	bool plain_only = false; ///< Only window pieces whose tile holds exactly the piece's axis track in the layout so far count (the old PlainRowPieces filter).
};

/** pos(a).x - pos(b).x >= k, between two nodes (difference constraint along x, as the skeleton solver's). */
struct JunctionMapConstraint {
	uint8_t a;
	uint8_t b;
	int k;
};

/** A stretch of track between two nodes. */
struct JunctionMapEdge {
	std::optional<JunctionRoute> fixed; ///< Exact geometry (main line, stubs, doubled lanes): laid as given, nodes ignored.
	uint8_t from = 0; ///< Start node (its window gives the route's starts).
	uint8_t to = 0; ///< End node (its window gives the route's goals).
	std::vector<TileIndexDiffC> fence; ///< Tiles this edge may not use (RouteFencedAndAdd's fence).
	std::vector<TileIndexDiffC> crossable; ///< As JunctionRouteRequest::crossable.
	Money crossable_cost = 0;
};

/** A junction's structure in tile units: nodes, constraints between them, edges in projection order. */
struct JunctionMap {
	std::vector<JunctionMapNode> nodes;
	std::vector<JunctionMapConstraint> constraints;
	std::vector<JunctionMapEdge> edges;
	JunctionRouteRequest base{}; ///< Router settings shared by every edge (box, curve spacing, bridges, prices, terrain, plane).
	JunctionRouteCache *cache = nullptr; ///< When set, router results are shared through it.
};

/** The result of projecting a map. */
struct JunctionMapProjection {
	std::optional<JunctionLayout> layout; ///< Unfinished (not cropped, no signals).
	std::vector<std::optional<JunctionRouteEnd>> placed; ///< Per node, the piece it was projected to.
	std::vector<JunctionRaster> rasters; ///< Per edge, what was laid.
	std::string error; ///< Which edge failed, when it did.
};

/** Lay a map's edges in order: fixed edges as given; routed edges from their start node's window to their end node's window, both filtered by plain_only and by every constraint whose other node is already placed. */
JunctionMapProjection ProjectJunctionMap(const JunctionMap &map, JunctionLayout layout = {});

#endif /* JUNCTION_MAP_H */
