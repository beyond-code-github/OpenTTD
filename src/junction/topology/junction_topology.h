/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_topology.h The topology of a junction type, its lane-level paths and the chord crossing rule (DESIGN-G1 §1, §2). */

#ifndef JUNCTION_TOPOLOGY_H
#define JUNCTION_TOPOLOGY_H

#include "../terrain/junction_facts.h"
#include "../junction_layout.h"
#include "../junction_route.h"
#include "../terrain/junction_terraform.h"

#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

struct JunctionSpec;
struct JunctionLineSpec;

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

/** How the lanes of a turning movement connect where both groups have more than one lane (D221); single-lane groups and through movements are not affected. */
enum class JunctionLanePairing : uint8_t {
	AnyToAny, ///< Every in lane to every out lane (D221 A, the default): each lane splits by direction, then by lane (D204, D187).
	LaneKeepsLane, ///< In lane k to out lane k; needs equal lane counts (D221 B).
	LanePerDirection, ///< A group's lanes are dealt round-robin to its turning movements, each fanning to the other group's lanes (D221 C).
};

/** A junction type: its lane groups and movements (D189). */
struct JunctionTopology {
	std::vector<JunctionLaneGroup> groups; ///< In slot order (JunctionPortSlots).
	std::vector<std::pair<uint8_t, uint8_t>> movements; ///< (in group, out group), indices into groups.
	JunctionLanePairing pairing = JunctionLanePairing::AnyToAny; ///< Lane pairing of multi-to-multi turning movements (D221).
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
 * Both sides with more than one lane and not a through movement: by the topology's pairing (D221). AnyToAny: every pair. LaneKeepsLane: lane k to lane k, unequal lane counts are the error "lane keeps lane needs equal lane counts". LanePerDirection: a group with several turning movements deals its lanes round-robin to them in movement order (lane k serves its (k mod n)-th turning movement), the dealt lanes of a movement connect to all lanes (or the dealt lanes) of the other group; a group with fewer lanes than turning movements is the error "lane per direction needs a lane for each direction".
 * @param error Receives the reason when the result is empty.
 */
std::vector<JunctionLanePath> ExpandJunctionLanes(const JunctionTopology &topology, std::string *error);

/**
 * The pairs of lane paths that must cross (DESIGN-G1 R4, chord rule): two paths with no slot in common cross iff exactly one end of one lies strictly between the two ends of the other in slot order.
 * Pairs (i, j) with i < j, sorted. Paths sharing a slot are not paired here (they share a trunk; per-edge crossings are a later step).
 */
std::vector<std::pair<uint32_t, uint32_t>> JunctionChordCrossings(const std::vector<JunctionLanePath> &paths);

/**
 * The row of every port slot (DESIGN-G1 §8): the main slots on the W side in slot order are rows 0, 1, ...; every other main slot takes the row of the W-side slot it shares a through path with (both ends main); a non-main slot has -1.
 * @param error Receives the reason when the result is empty.
 */
std::vector<int> JunctionSlotRows(const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, std::string *error);

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
	int level; ///< 2r+1 on row r; 2b in band b (band b north of row b; band R south of the last row) (DESIGN-G1 §8, D204, D205).
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

/** A node or a band crossing at its place inside a band (DESIGN-G1 §10.8). */
struct JunctionTrackBandEvent {
	bool node; ///< A node (a is its index), else a band crossing (a and b are its edges, as in JunctionTrackGraph::crossings).
	uint32_t a;
	uint32_t b; ///< 0 for a node.
	int layer; ///< 1 is southmost; an event lies north of every event of a lower layer that touched one of its items.
	bool operator==(const JunctionTrackBandEvent &) const = default;
};

/** The order of a track graph (DESIGN-G1 §8). */
struct JunctionTrackOrder {
	std::vector<int> node_level; ///< Per node: 2r+1 on row r; 2b in band b (band b lies north of row b; band R south of the last row).
	std::vector<std::vector<JunctionTrackRowItem>> rows; ///< Per row, north to south: its nodes and the edges crossing it, west to east.
	std::vector<std::vector<uint32_t>> edge_crossings; ///< Per edge: the edges crossing it, in travel order.
	std::vector<std::vector<JunctionTrackBandEvent>> bands; ///< Per band b (0 to R), its events in sweep order.
};

/**
 * The order of a track graph by a sweep from the branch side (DESIGN-G1 §8).
 * @param paths ExpandJunctionLanes(topology).
 * @param graph BuildJunctionTrackGraph(topology, paths).
 * @param error Receives the reason when the result is empty (no rows).
 */
JunctionTrackOrder OrderJunctionTrackGraph(const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, std::string *error);

/** One track of the row stack inside the box, north to south (DESIGN-G1 §9). */
struct JunctionTrackStackItem {
	enum class Kind : uint8_t {
		Lane, ///< The main-line lane itself.
		Prio, ///< Its prio track (D75/D87: opposite the joiner).
		Copy, ///< Its doubling copy (D80).
		PrioAndCopy, ///< Prio and doubling copy sharing one track (D108 keeps them apart along the lane).
		Approach, ///< The joiner's last stretch beside the lane, on the joiner's side.
	};
	uint8_t row; ///< The lane's row (JunctionTrackOrder::rows index).
	Kind kind;
	bool operator==(const JunctionTrackStackItem &) const = default;
};

/** The most crossings one bridge passes over; a longer crossing set goes on further bridges in series (D209, provisional until the D208 rig experiment). */
static constexpr size_t JUNCTION_BRIDGE_MAX_CROSSERS = 2;

/** A bridge: one edge carried over a contiguous set of crossings (D203); CutJunctionTrackBridges cuts a lane's set into bridges of at most JUNCTION_BRIDGE_MAX_CROSSERS (D209). */
struct JunctionTrackBridge {
	uint32_t edge; ///< The edge carried over.
	std::vector<uint32_t> crossed; ///< The edges it passes, in the carried edge's travel order.
	bool doubled; ///< A main-line bridge (D80, D188); a branch bridge is single (D174).
	bool tunnel = false; ///< The edge passes under the crossed edges in a tunnel instead of over them on a bridge (D220); only on a single (not doubled) separation.
	bool operator==(const JunctionTrackBridge &) const = default;
};

/** Side tracks, row stack, bridges and turns of an ordered track graph (DESIGN-G1 §9). */
struct JunctionTrackStructure {
	std::vector<JunctionTrackStackItem> stack; ///< North to south.
	std::vector<JunctionTrackBridge> bridges; ///< Sorted by edge; an edge's bridges in its travel order.
	std::vector<int8_t> turns; ///< Per lane path: quarter turns clockwise from in heading to out heading (+1 right, -1 left, 0).
};

/**
 * The structure of an ordered track graph (DESIGN-G1 §9).
 * @param error Receives the reason when the result is empty (no stack).
 */
JunctionTrackStructure StructureJunctionTrackGraph(const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, const JunctionTrackOrder &order, std::string *error);

/** One token of a track edge's program (DESIGN-G1 §10.7), in travel order. Runs lie between tokens; lengths, columns and port rows are not part of a program. */
struct JunctionTrackStep {
	static constexpr uint32_t NO_ROW = UINT32_MAX; ///< A bend that ends heading N or S.
	enum class Kind : uint8_t {
		Fan, ///< The S-bend between a port row and a stack row on a row edge, or none when they coincide; on an S branch port edge, a vertical S-bend sized by placement, 0 allowed (D230).
		Corner, ///< A same-hand 90-degree pair.
		SBend, ///< An opposite-hand pair: the track moves sideways and keeps its heading.
		Over, ///< This edge's bridge passes over another edge.
		Under, ///< This edge passes under another edge's bridge.
		Tunnel, ///< This edge's tunnel passes under another edge (D220); where a bridge's edge has Over.
		Above, ///< This edge passes over another edge's tunnel (D220); where a crossed edge has Under.
	};
	enum class Role : uint8_t {
		None,
		LaneSplit, ///< SBend: a doubled leg's copy 1 bends onto the lane row.
		LaneMerge, ///< SBend: a doubled leg's copy 0 bends onto the lane row.
		Shift, ///< SBend: a pair that moves a line sideways and back (R7 inserts them).
		Diverge, ///< SBend: leaves the split the edge starts at; starts on its point (DESIGN-G2 §G2.6).
		Converge, ///< SBend: joins the merge the edge ends at (a joiner's arrival included); ends on its point (DESIGN-G2 §G2.6).
	};
	Kind kind;
	int8_t hand; ///< Corner: the turn, +1 right, -1 left (as JunctionTrackStructure::turns). SBend: its first curve. 0 otherwise.
	uint32_t index; ///< Fan: the stack item at its inner end, or NO_ROW at an S branch port. Corner, SBend: the stack item it ends on, or NO_ROW. Over, Tunnel: the crossed edge. Under, Above: the edge overhead (Above: the edge in the tunnel).
	uint8_t copy; ///< Under: which copy of a doubled bridge is overhead (0 or 1). 0 otherwise.
	Role role = Role::None; ///< SBend: what it serves (D215 feature; R7 inserts Shift pairs).
	bool operator==(const JunctionTrackStep &) const = default;
};

/** The programs of a structured track graph (DESIGN-G1 §10.7). */
struct JunctionTrackPrograms {
	std::vector<std::vector<JunctionTrackStep>> edges; ///< Per graph edge; for a doubled edge its copy 0 (keeps its upstream row through the bridge, bends onto the lane row after it).
	std::vector<std::vector<JunctionTrackStep>> copies; ///< Per graph edge: a doubled edge's copy 1 (bends onto the lane row before the bridge); empty for the others.
};

/**
 * The programs of every edge of a structured track graph (DESIGN-G1 §10.7).
 * @param error Receives the reason when the result is empty (no edges).
 */
JunctionTrackPrograms ProgramJunctionTrackGraph(const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, const JunctionTrackOrder &order, const JunctionTrackStructure &structure, std::string *error);

/** The stack items a row's tokens end on. */
struct RowItems {
	size_t lane; ///< The lane's item.
	size_t approach; ///< The joiner's approach item, or NONE.
	size_t north; ///< The item before the lane unless that is the approach, or NONE.
	size_t copy; ///< The row's Copy or PrioAndCopy item, on either side of the lane, or NONE.
	static constexpr size_t NONE = JunctionTrackStep::NO_ROW; ///< Not in the stack.
};

/**
 * The lane, approach and north items of a row (the item before Lane that is not Approach is north).
 * @param stack The track structure's stack.
 * @param row The row.
 * @return Its items.
 */
RowItems FindRowItems(const std::vector<JunctionTrackStackItem> &stack, int row);

/**
 * The first or last Fan token of an edge's program, found by kind (R3).
 * @param programs The programs.
 * @param e The edge.
 * @param first True for the first Fan (an in edge's), false for the last (an out edge's).
 * @return Its index in the program, or the program's size when it has none.
 */
size_t FanTokenOf(const JunctionTrackPrograms &programs, uint32_t e, bool first);

/** The roles of a row's edges, from the graph and the programs (DESIGN-G1 §16.4 RW3). */
bool JunctionIsInEdge(const JunctionTrackGraph &graph, const JunctionTrackOrder &order, const JunctionTrackPrograms &programs, uint32_t e); ///< An edge from a port to a row node whose program starts with a Fan.
bool JunctionIsOutEdge(const JunctionTrackGraph &graph, const JunctionTrackOrder &order, const JunctionTrackPrograms &programs, uint32_t e); ///< An edge from a row node to a port whose program ends with a Fan.
bool JunctionIsLegEdge(const JunctionTrackGraph &graph, const JunctionTrackOrder &order, uint32_t e); ///< An edge between two nodes of one row.
bool JunctionIsDoubledEdge(const JunctionTrackPrograms &programs, uint32_t e); ///< An edge with a copy 1.

/** The T of today's node model (L_R main line, L_R branch on the S side), groups in slot order: W side EIn then WOut; S side NIn then SOut; E side WIn then EOut. Movements in JunctionTMovements order. */
const JunctionTopology &JunctionTTopology();

/** An LL_RR main line with an L_R sideline on the S side (D186-D188, D202): W side e-in (2 lanes) then w-out (2); S side branch-in (1) then branch-out (1); E side w-in (2) then e-out (2). Movements: e through, w through, e exit, w exit, join to e-out, join to w-out. */
const JunctionTopology &JunctionLLRRSidelineTopology();

/**
 * Build the hub topology of a main line and a branch line (D189): the main line enters on the W side and leaves on the E side, the branch on the S side.
 * Its left group is the one heading into the box at W (and out of it at E and, as the branch's own direction, in at S); the right group the other way.
 * Movements are every in group to every out group on another side, in group order, each in group's outs in slot order.
 * @param main The main line (D142 groups).
 * @param branch The branch line.
 * @param pairing The lane pairing of multi-to-multi turning movements (D221), stored on the topology.
 * @return The topology; equals JunctionTTopology() as groups and as a set of movements for L_R and L_R, and JunctionLLRRSidelineTopology() for LL_RR and L_R.
 */
JunctionTopology MakeJunctionHubTopology(const JunctionLineSpec &main, const JunctionLineSpec &branch, JunctionLanePairing pairing = JunctionLanePairing::AnyToAny);

#endif /* JUNCTION_TOPOLOGY_H */
