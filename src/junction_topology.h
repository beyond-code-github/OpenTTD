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

/** The T of today's node model (L_R main line, L_R branch on the S side), groups in slot order: W side EIn then WOut; S side NIn then SOut; E side WIn then EOut. Movements in JunctionTMovements order. */
const JunctionTopology &JunctionTTopology();

/** An LL_RR main line with an L_R sideline on the S side (D186-D188, D202): W side e-in (2 lanes) then w-out (2); S side branch-in (1) then branch-out (1); E side w-in (2) then e-out (2). Movements: e through, w through, e exit, w exit, join to e-out, join to w-out. */
const JunctionTopology &JunctionLLRRSidelineTopology();

#endif /* JUNCTION_TOPOLOGY_H */
