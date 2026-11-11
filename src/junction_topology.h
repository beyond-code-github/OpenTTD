/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_topology.h The topology of a junction type, its lane-level paths and the chord crossing rule (DESIGN-G1 §1, §2). */

#ifndef JUNCTION_TOPOLOGY_H
#define JUNCTION_TOPOLOGY_H

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

/** A side of the junction's box in the canonical frame (the main line runs along X, D152). */
enum class JunctionSide : uint8_t { N, E, S, W };

/** One direction of one line where it meets the box (DESIGN-G1 §1). */
struct JunctionLaneGroup {
	JunctionSide side;
	bool in; ///< Trains enter the box here.
	uint8_t lanes; ///< Adjacent same-direction tracks (D142), at least 1.
	uint8_t line; ///< Groups of one line share this id.
	bool main; ///< Part of the main line (D60: main line and branch take their own notation).
};

/** A junction type: its lane groups and movements (D189). */
struct JunctionTopology {
	std::vector<JunctionLaneGroup> groups; ///< In slot order (JunctionPortSlots).
	std::vector<std::pair<uint8_t, uint8_t>> movements; ///< (in group, out group), indices into groups.
};

/** One lane of a group at the box: a port slot. */
struct JunctionPortSlot {
	uint8_t group;
	uint8_t lane; ///< 0 is the northmost lane on the W and E sides, the westmost on the N and S sides.
};

/** The port slots counter-clockwise from the north-west corner: W side north to south, S side west to east, E side south to north, N side east to west; so on the E and N sides a group's lanes appear in reverse lane order. Groups must be listed in that order; otherwise empty. */
std::vector<JunctionPortSlot> JunctionPortSlots(const JunctionTopology &topology);

/** One lane-level path (DESIGN-G1 R1): from an in slot to an out slot. */
struct JunctionLanePath {
	uint16_t from; ///< Index into JunctionPortSlots.
	uint16_t to;
	uint16_t movement; ///< Index into JunctionTopology::movements.
};

/**
 * The lane-level paths of a topology (DESIGN-G1 R1; D186, D187): per movement in order, lanes ascending (from lane, then to lane).
 * Through movement (both groups main, same line): lane i to lane i (equal lane counts, else error). Otherwise one side has one lane: every pair (an exit from every lane, D187; a fan onto every lane, D186).
 * Both sides with more than one lane and not a through movement: error "multi-to-multi movements are not ruled yet" (D186: later).
 * @param error Receives the reason when the result is empty.
 */
std::vector<JunctionLanePath> ExpandJunctionLanes(const JunctionTopology &topology, std::string *error);

/**
 * The pairs of lane paths that must cross (DESIGN-G1 R4, chord rule): two paths with no slot in common cross iff exactly one end of one lies strictly between the two ends of the other in slot order.
 * Pairs (i, j) with i < j, sorted. Paths sharing a slot are not paired here (they share a trunk; per-edge crossings are a later step).
 */
std::vector<std::pair<uint32_t, uint32_t>> JunctionChordCrossings(const std::vector<JunctionLanePath> &paths);

/** One end of a track-graph edge: a port slot or a node. */
struct JunctionTrackEnd {
	bool node; ///< index is into JunctionTrackGraph::nodes, else into JunctionPortSlots.
	uint16_t index;
	bool operator==(const JunctionTrackEnd &) const = default;
};

/** A switch of the track graph: where an in slot's paths split, or where an out slot's paths merge (DESIGN-G1 §7; D187, D204). */
struct JunctionTrackNode {
	bool split; ///< In an in slot's split tree; else in an out slot's merge tree.
	uint16_t slot; ///< The port slot whose tree holds it.
	bool operator==(const JunctionTrackNode &) const = default;
};

/** A run of track between two ends in the direction of travel, carried by a set of lane paths. */
struct JunctionTrackEdge {
	enum class Kind : uint8_t {
		In, ///< In an in slot's split tree.
		Leg, ///< One path's own run, from its last split (or in port) to its first merge (or out port).
		Out, ///< In an out slot's merge tree.
	};
	Kind kind;
	JunctionTrackEnd from;
	JunctionTrackEnd to;
	std::vector<uint16_t> paths; ///< Indices into the lane paths, ascending.
	bool operator==(const JunctionTrackEdge &) const = default;
};

/** The track graph of a topology (DESIGN-G1 §7): switches, edges, each path's edges, and which edges cross. */
struct JunctionTrackGraph {
	std::vector<JunctionTrackNode> nodes;
	std::vector<JunctionTrackEdge> edges;
	std::vector<std::vector<uint32_t>> path_edges; ///< Per lane path, its edges in travel order.
	std::vector<std::pair<uint32_t, uint32_t>> crossings; ///< Edge pairs (i < j) that cross, sorted.
};

/**
 * The track graph of a topology's lane paths (DESIGN-G1 §7).
 * @param paths ExpandJunctionLanes(topology).
 * @param error Receives the reason when the result is empty (no nodes, no edges).
 */
JunctionTrackGraph BuildJunctionTrackGraph(const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, std::string *error);

/** A node, or an edge crossing the lane, at its place along a main-lane row. */
struct JunctionTrackRowItem {
	bool node; ///< index is into JunctionTrackGraph::nodes, else into its edges (an edge crossing this row).
	uint32_t index;
	bool operator==(const JunctionTrackRowItem &) const = default;
};

/** The order of a track graph (DESIGN-G1 §8). */
struct JunctionTrackOrder {
	std::vector<int> node_level; ///< Per node: 2r+1 on row r; 2b in band b (band b lies north of row b; band R south of the last row).
	std::vector<std::vector<JunctionTrackRowItem>> rows; ///< Per row, north to south: its nodes and the edges crossing it, west to east.
	std::vector<std::vector<uint32_t>> edge_crossings; ///< Per edge: the edges crossing it, in travel order.
};

/**
 * The order of a track graph by a sweep from the branch side (DESIGN-G1 §8).
 * @param paths ExpandJunctionLanes(topology).
 * @param graph BuildJunctionTrackGraph(topology, paths).
 * @param error Receives the reason when the result is empty (no rows).
 */
JunctionTrackOrder OrderJunctionTrackGraph(const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, std::string *error);

/** The T of today's node model (L_R main line, L_R branch on the S side), groups in slot order: W side EIn then WOut; S side NIn then SOut; E side WIn then EOut. Movements in JunctionTMovements order. */
const JunctionTopology &JunctionTTopology();

/** An LL_RR main line with an L_R sideline on the S side (D186-D188, D202): W side e-in (2 lanes) then w-out (2); S side branch-in (1) then branch-out (1); E side w-in (2) then e-out (2). Movements: e through, w through, e exit, w exit, join to e-out, join to w-out. */
const JunctionTopology &JunctionLLRRSidelineTopology();

#endif /* JUNCTION_TOPOLOGY_H */
