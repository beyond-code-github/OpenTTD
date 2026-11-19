/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_signals.h Signal tokens resolved on the rendered routes of a placement (DESIGN-G1 §12). */

#ifndef JUNCTION_SIGNALS_H
#define JUNCTION_SIGNALS_H

#include "../junction_layout.h"
#include "../junction_route.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

struct JunctionSpec;

/** One prio element as a placement fixes it (DESIGN-G1 12.4). */
struct JunctionPrioSpan {
	std::vector<std::pair<TileIndexDiffC, TrackBits>> tiles; ///< Its tiles in drawing order (per step upstream of the merge: the main-line tile's corner half on even steps, then the row tile) with the bits drawn on each.
	TileIndexDiffC merge; ///< The joiner merge's tile (step 0 on the main line).
};

/** One route as the signal pass reads it, whatever model made it. */
struct JunctionSignalRoute {
	bool main = false; ///< A main line: its merges carry the prio element and it gets no waiting bays.
	uint32_t group = 0; ///< A prio's joiner is a route of another group.
	std::vector<std::pair<bool, size_t>> switches; ///< Its splits and merges: (split, the switch tile's piece index), sorted by piece, a split before a merge on one piece.
	std::vector<std::pair<size_t, uint32_t>> merge_nodes; ///< For each merge switch where a joiner can arrive: (its piece index, the merge node).
	std::vector<std::pair<uint32_t, size_t>> arrivals; ///< Per node the route enters: (node, the index of its last piece before the node).
};

/** The routes' switch structure, as the signal pass reads it (parallel to the track routes). */
struct JunctionSignalNetwork {
	std::vector<JunctionSignalRoute> routes;
};

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
 * Place the signal tokens of rendered routes (DESIGN-G1 §12.2, §12.3): the anchor signals of every route's splits, merges, bridges and out port, then the whole-train bays before the merges of non-through paths, then drag fill; each drawn into layout.signals once.
 * @param why Receives the first prio element that does not fit (its merge tile), empty when every element was drawn.
 * @param prio_beside Receives, for every tile a prio element draws beside the main line (its side row, steps 0 to n_prio), that tile and the main-line tile next to it in the same column (H3, the level solve).
 * @param prio The placement's prio elements: when given, the pass draws exactly their tiles and bits; nullptr derives the elements from the routes (callers with no placement).
 * @return The tokens, in placement order; a signal shared by several routes or rules gives one token per route and rule that resolved to it.
 */
std::vector<JunctionTrackSignal> SignalJunctionTrackRoutes(JunctionLayout &layout, const std::vector<JunctionTrackRoute> &routes, const JunctionSignalNetwork &network, const JunctionSpec &spec, std::string *why = nullptr, std::vector<std::pair<TileIndexDiffC, TileIndexDiffC>> *prio_beside = nullptr, const std::vector<JunctionPrioSpan> *prio = nullptr);

#endif /* JUNCTION_SIGNALS_H */
