/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_signals.h Signal tokens resolved on the rendered routes of a placement (DESIGN-G1 §12). */

#ifndef JUNCTION_SIGNALS_H
#define JUNCTION_SIGNALS_H

#include "../placement/junction_placement.h"
#include "../junction_layout.h"
#include "../junction_route.h"
#include "../topology/junction_topology.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

struct JunctionSpec;

/**
 * The switch events of routes[r] from the tags (DESIGN-G1 §13.8 item 4): at each boundary between consecutive edges, the node piece, a split when graph.nodes[node].split; on a doubled edge, the lane split and the lane merge. The lane split tile is the first piece of the copy-1 route's SBend token, the lane merge tile the last piece of the copy-0 route's SBend token (the sibling route of the same lane path and the other copy supplies the one routes[r] lacks); the event is routes[r]'s first non-bridge piece on that tile. Sorted by piece, a split before a merge on one piece.
 */
std::vector<std::pair<bool, size_t>> JunctionRouteEvents(const JunctionTrackGraph &graph, const JunctionTrackPrograms &programs, const std::vector<JunctionTrackRoute> &routes, size_t r);

/** Why a signal stands where it does (DESIGN-G1 §12.2, §12.3). */
enum class JunctionSignalReason : uint8_t { BeforeSplit, AfterSplit, BeforeMerge, AfterMerge, BeforeBridge, AfterBridge, SeriesGap, Stub, Bay, Fill,
	Prio ///< Inside a prio element: its ML signals and its row signals (D211: the prio places its own).
};

/** A signal token: a signal placed by a rule of the graph, on one route (D211). */
struct JunctionTrackSignal {
	uint16_t route; ///< Index into the routes.
	JunctionSignalReason reason;
	TileIndexDiffC tile;
	Trackdir facing;
};

/**
 * Place the signal tokens of a rendered seed (DESIGN-G1 §12.2, §12.3): the anchor signals of every route's splits, merges, bridges and out port, then the whole-train bays before the merges of non-through paths, then drag fill; each drawn into layout.signals once.
 * @param why Receives the first prio element that does not fit (its merge tile), empty when every element was drawn.
 * @param prio_beside Receives, for every tile a prio element draws beside the main line (its side row, steps 0 to n_prio), that tile and the main-line tile next to it in the same column (H3, the level solve).
 * @param prio The placement's prio elements: when given, the pass draws exactly their tiles and bits; nullptr derives the elements from the routes (callers with no placement).
 * @return The tokens, in placement order; a signal shared by several routes or rules gives one token per route and rule that resolved to it.
 */
std::vector<JunctionTrackSignal> SignalJunctionTrackRoutes(JunctionLayout &layout, const std::vector<JunctionTrackRoute> &routes, const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, const JunctionTrackPrograms &programs, const JunctionSpec &spec, std::string *why = nullptr, std::vector<std::pair<TileIndexDiffC, TileIndexDiffC>> *prio_beside = nullptr, const std::vector<JunctionPrioSpan> *prio = nullptr);

#endif /* JUNCTION_SIGNALS_H */
