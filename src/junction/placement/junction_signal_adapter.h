/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_signal_adapter.h The old pipeline's graph, programs and topology read into the signal pass's network. */

#ifndef JUNCTION_SIGNAL_ADAPTER_H
#define JUNCTION_SIGNAL_ADAPTER_H

#include "junction_placement.h"
#include "../signals/junction_signals.h"
#include "../topology/junction_topology.h"

/**
 * The switch events of routes[r] from the tags (DESIGN-G1 §13.8 item 4): at each boundary between consecutive edges, the node piece, a split when graph.nodes[node].split; on a doubled edge, the lane split and the lane merge. The lane split tile is the first piece of the copy-1 route's SBend token, the lane merge tile the last piece of the copy-0 route's SBend token (the sibling route of the same lane path and the other copy supplies the one routes[r] lacks); the event is routes[r]'s first non-bridge piece on that tile. Sorted by piece, a split before a merge on one piece.
 */
std::vector<std::pair<bool, size_t>> JunctionRouteEvents(const JunctionTrackGraph &graph, const JunctionTrackPrograms &programs, const std::vector<JunctionTrackRoute> &routes, size_t r);

/** The signal network of rendered graph routes: main = both ends of the route's movement are main groups, group = its movement, switches from JunctionRouteEvents, merge nodes at edge boundaries ending at a node, arrivals per path edge ending at a node. */
JunctionSignalNetwork MakeJunctionSignalNetwork(const std::vector<JunctionTrackRoute> &routes, const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, const JunctionTrackPrograms &programs);

/** SignalJunctionTrackRoutes on the network of MakeJunctionSignalNetwork. */
std::vector<JunctionTrackSignal> SignalJunctionTrackRoutes(JunctionLayout &layout, const std::vector<JunctionTrackRoute> &routes, const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, const JunctionTrackPrograms &programs, const JunctionSpec &spec, std::string *why = nullptr, std::vector<std::pair<TileIndexDiffC, TileIndexDiffC>> *prio_beside = nullptr, const std::vector<JunctionPrioSpan> *prio = nullptr);

#endif /* JUNCTION_SIGNAL_ADAPTER_H */
