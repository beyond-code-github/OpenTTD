/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_topology.h The topology of a junction type, its lane-level paths and the chord crossing rule (DESIGN-G1 §1, §2). */

#ifndef JUNCTION_TOPOLOGY_H
#define JUNCTION_TOPOLOGY_H

#include "junction_facts.h"
#include "junction_layout.h"
#include "junction_route.h"
#include "junction_terraform.h"

#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

struct JunctionSpec;

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
		Fan, ///< The S-bend between a port row and a stack row, or none when they coincide (placed later).
		Corner, ///< A same-hand 90-degree pair.
		SBend, ///< An opposite-hand pair: the track moves sideways and keeps its heading.
		Over, ///< This edge's bridge passes over another edge.
		Under, ///< This edge passes under another edge's bridge.
	};
	enum class Role : uint8_t {
		None,
		LaneSplit, ///< SBend: a doubled leg's copy 1 bends onto the lane row.
		LaneMerge, ///< SBend: a doubled leg's copy 0 bends onto the lane row.
		Shift, ///< SBend: a pair that moves a line sideways and back (R7 inserts them).
	};
	Kind kind;
	int8_t hand; ///< Corner: the turn, +1 right, -1 left (as JunctionTrackStructure::turns). SBend: its first curve. 0 otherwise.
	uint32_t index; ///< Fan: the stack item at its inner end. Corner, SBend: the stack item it ends on, or NO_ROW. Over: the crossed edge. Under: the edge overhead.
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

/** The roles of a row's edges, from the graph and the programs (DESIGN-G1 §16.4 RW3). */
bool JunctionIsInEdge(const JunctionTrackGraph &graph, const JunctionTrackOrder &order, const JunctionTrackPrograms &programs, uint32_t e); ///< An edge from a port to a row node whose program starts with a Fan.
bool JunctionIsOutEdge(const JunctionTrackGraph &graph, const JunctionTrackOrder &order, const JunctionTrackPrograms &programs, uint32_t e); ///< An edge from a row node to a port whose program ends with a Fan.
bool JunctionIsLegEdge(const JunctionTrackGraph &graph, const JunctionTrackOrder &order, uint32_t e); ///< An edge between two nodes of one row.
bool JunctionIsDoubledEdge(const JunctionTrackPrograms &programs, uint32_t e); ///< An edge with a copy 1.

/** A token's tiles: its first and last tile in the edge's travel order (DESIGN-G1 §11.8). */
using JunctionTrackSpan = std::pair<TileIndexDiffC, TileIndexDiffC>;

/** Why a placement relation exists (DESIGN-G1 §14.4): the rule and the graph elements it serves. */
struct JunctionPlacementReason {
	const char *rule = nullptr; ///< A literal naming the floor or block, as the code's comment there names it (e.g. "out fan k_out", "chain run", "waiting room").
	uint32_t edge = UINT32_MAX;
	uint32_t node = UINT32_MAX;
	int32_t row = -1; ///< The row chain's index, -1 when none.
};

/** The values placement does not derive from its relations (DESIGN-G1 §14.2): fixed within a search state, changed only by an edit. */
struct JunctionPlacementChoices {
	std::vector<std::vector<int>> m; ///< Per edge, per token of JunctionTrackPrograms::edges: a vertical S-bend's size, 0 for other tokens.
	std::vector<std::vector<int>> corner; ///< Per edge, per token: a joiner_div Corner's row span, 0 for other tokens.
	int port_offset = 0; ///< The port rows' offset.
	bool operator==(const JunctionPlacementChoices &) const = default;
};

/** The seed placement of a programmed track graph (DESIGN-G1 §11.8): tiles only, x east, y south, (0, 0) the box's north-west corner. */
struct JunctionTrackPlacement {
	std::vector<int> item_rows; ///< Per stack item, its tile row.
	std::vector<TileIndexDiffC> ports; ///< Per port slot, its boundary tile.
	std::vector<TileIndexDiffC> nodes; ///< Per graph node: a split's first diverging half piece, a merge's last joining half piece.
	std::vector<std::vector<JunctionTrackSpan>> edges; ///< Per edge, per token of JunctionTrackPrograms::edges.
	std::vector<std::vector<JunctionTrackSpan>> copies; ///< Per edge, per token of JunctionTrackPrograms::copies.
	std::vector<std::array<JunctionBridge, 2>> bridges; ///< Per structure bridge: copy 0's and copy 1's heads, start = the head the carrier reaches first; a single bridge's copy 1 is {{0, 0}, {0, 0}}.
	TileIndexDiffC size; ///< The box.
	JunctionPlacementChoices choices; ///< The choices this placement was solved with.
	std::vector<JunctionPlacementReason> why; ///< On failure: the explanation chain of the contradiction.
	std::vector<int> columns; ///< The column of every placement point (x_floors refer to these ids).
	JunctionTerraform terraform; ///< The height solve's plan (D218), empty on flat ground.
	Money height_cost = 0; ///< Terraform, foundations and clearing of the chosen levels.
	std::vector<std::pair<TileIndexDiffC, int8_t>> track_levels; ///< The level solve's track level per track tile (placement frame), for the ghost and the build check.
	std::vector<std::vector<std::pair<int8_t, int>>> curves; ///< Per lane path and copy (paths first, then copies in path order): each curve's hand (+1 right, -1 left) and unit position at the solved columns, from the curve family's walk (for tests).
};

/**
 * The seed placement (DESIGN-G1 §11.8): the least tiles that meet the rules, Y first then X.
 * @param error Receives the reason when the result is empty (no nodes), e.g. a bridge longer than spec.max_bridge_middle.
 */
JunctionTrackPlacement PlaceJunctionTrackGraph(const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, const JunctionTrackOrder &order, const JunctionTrackStructure &structure, const JunctionTrackPrograms &programs, const JunctionSpec &spec, std::string *error);

/**
 * The placement with its choices fixed (DESIGN-G1 §14.2): one Y solve and one X solve, no rounds.
 * @param choices The S-bend sizes, Corner spans and port offset to solve with, e.g. those of an earlier placement.
 * @param error Receives the reason when the result is empty; JunctionTrackPlacement::why then names the relations of a contradiction.
 * @param x_floors (point, least column) pairs added as lower bounds in the X phase, reason "x floor"; a test seam until terrain facts (DESIGN-G1 §14.4). Points are the placement's internal ids.
 * @param y_floors (point, least row) pairs added as lower bounds in the Y phase, reason "y floor"; a test seam until the row-gap edit (DESIGN-G1 §14.6). Point 1 + i is stack item i.
 */
JunctionTrackPlacement PlaceJunctionTrackGraph(const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, const JunctionTrackOrder &order, const JunctionTrackStructure &structure, const JunctionTrackPrograms &programs, const JunctionSpec &spec, const JunctionPlacementChoices &choices, std::string *error, const std::vector<std::pair<int, int>> &x_floors = {}, const std::vector<std::pair<int, int>> &y_floors = {});

/**
 * The fixed-choices placement with the target terrain's facts in the X phase (DESIGN-G1 §14.4, D216): every group of tiles (a bend, a switch tile, a bridge head, a vertical run's tile) is restricted to the columns where its tile is buildable, and a straight run on a row must be buildable on every tile it covers. Rows stay as the structure gives them.
 * @param facts The target terrain's facts.
 * @param offset The working tile of the placement's (0, 0), as the node solve's offset.
 * @param error Receives the reason when the result is empty; JunctionTrackPlacement::why then names the relations of a contradiction, a fact's tile in the error text.
 * @param heights False (D218, the hard-facts mode): only the class facts apply (blocked, water, not spannable, outside the snapshot); height steps, joins and the fits of bends and nodes are skipped.
 * @param terraform True (D218): X uses the class facts only (JunctionFacts::Buildable, forces heights = false) and then the height solve (DESIGN-G1 §15.3) chooses a level per tile on every edge and sets terraform and height_cost; on failure the placement is empty and error says why ("terraform conflict" when two tiles or a propagation want a corner at different heights).
 * @param terrain The snapshot the facts were made from; required (non-null) when terraform is true.
 */
JunctionTrackPlacement PlaceJunctionTrackGraph(const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, const JunctionTrackOrder &order, const JunctionTrackStructure &structure, const JunctionTrackPrograms &programs, const JunctionSpec &spec, const JunctionPlacementChoices &choices, const JunctionFacts &facts, TileIndexDiffC offset, std::string *error, bool heights = true, bool terraform = false, const JunctionTerrain *terrain = nullptr);

/**
 * The series-bridge cut (D209, DESIGN-G1 §11.9a): every doubled bridge cut into consecutive bridges of at most JUNCTION_BRIDGE_MAX_CROSSERS crossings, choosing among the cuts with the fewest bridges whose placement succeeds the one with the least box width, then height, then the first tried.
 * @param[out] placement Receives the chosen cut's placement; empty (no nodes) when no cut places.
 * @param error Receives the last cut's placement error when no cut places.
 * @return The structure with the chosen cut (the input structure when none places).
 */
JunctionTrackStructure CutJunctionTrackBridges(const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, const JunctionTrackOrder &order, const JunctionTrackStructure &structure, const JunctionTrackPrograms &programs, const JunctionSpec &spec, JunctionTrackPlacement *placement, std::string *error);

/** Which graph element a route piece renders (DESIGN-G1 §13.2). */
struct JunctionTrackPieceTag {
	static constexpr uint16_t RUN = UINT16_MAX; ///< `token` of a piece on a run between tokens.
	uint32_t edge; ///< Graph edge.
	uint16_t token; ///< Index into the edge's program (JunctionTrackPrograms::edges or ::copies by `copy`), or RUN.
	uint16_t run; ///< The number of the edge's tokens before this piece (run k lies between tokens k - 1 and k).
	uint8_t copy;
	bool operator==(const JunctionTrackPieceTag &) const = default;
};

/** One rendered route: a lane path's pieces from its in port to its out port, for one copy (DESIGN-G1 §11.10). */
struct JunctionTrackRoute {
	uint16_t path; ///< Index into the lane paths.
	uint8_t copy; ///< 1 for the copy-1 route of a path through a doubled leg, else 0.
	std::vector<JunctionPiece> pieces;
	std::vector<JunctionTrackPieceTag> tags; ///< Per piece.
};

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
 * The track-only layout of a seed placement (DESIGN-G1 §11.10): one route per lane path and copy, rasterized and merged.
 * @param error Receives the reason when the result is empty (no tracks).
 */
JunctionLayout RenderJunctionTrackPlacement(const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, const JunctionTrackStructure &structure, const JunctionTrackPrograms &programs, const JunctionTrackPlacement &placement, std::string *error, std::vector<JunctionTrackRoute> *routes = nullptr);

/**
 * Place the signal tokens of a rendered seed (DESIGN-G1 §12.2, §12.3): the anchor signals of every route's splits, merges, bridges and out port, then the whole-train bays before the merges of non-through paths, then drag fill; each drawn into layout.signals once.
 * @param why Receives the first prio element that does not fit (its merge tile), empty when every element was drawn.
 * @param prio_beside Receives, for every tile a prio element draws beside the main line (its side row, steps 0 to n_prio), that tile and the main-line tile next to it in the same column (H3, the level solve).
 * @return The tokens, in placement order; a signal shared by several routes or rules gives one token per route and rule that resolved to it.
 */
std::vector<JunctionTrackSignal> SignalJunctionTrackRoutes(JunctionLayout &layout, const std::vector<JunctionTrackRoute> &routes, const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, const JunctionTrackPrograms &programs, const JunctionSpec &spec, std::string *why = nullptr, std::vector<std::pair<TileIndexDiffC, TileIndexDiffC>> *prio_beside = nullptr);
/** The graph pipeline's products for one topology and spec (DESIGN-G1 §13): the seed a projection search starts from. */
struct JunctionGraphSeed {
	const JunctionTopology *topology;
	std::vector<JunctionLanePath> paths;
	JunctionTrackGraph graph;
	JunctionTrackStructure structure; ///< After the bridge cut.
	JunctionTrackPrograms programs;
	JunctionTrackPlacement placement;
	std::vector<JunctionTrackRoute> routes;
};
/** Run the graph pipeline (expand, graph, order, structure, programs, cut, render routes); empty routes and the reason in error on failure. */
JunctionGraphSeed MakeJunctionGraphSeed(const JunctionTopology &topology, const JunctionSpec &spec, std::string *error);

/** A local detour as a graph edit (DESIGN-G1 §13.5): an out-and-back S-bend on one run of one edge, around the failing tile's column, to side +1 (right of travel) or -1. Lengths are recomputed when applied; the column is clamped strictly inside the seed's run. */
struct JunctionDetourEdit {
	uint32_t edge;
	uint16_t run;
	uint8_t copy;
	int32_t column; ///< The failing tile's working x (the house is fixed in the world).
	int8_t side;
	uint16_t variant; ///< How many of the candidates that pass ApplyJunctionDetours' rules, in its scan order, are skipped (the search tries them in order until a child template builds).
	auto operator<=>(const JunctionDetourEdit &) const = default;
};
/** The routes with every edit applied to every route carrying its (edge, copy) run (D213: realised on routes, not a layout; offset maps the template frame to working, working = frame + offset); empty with the reason in why when an edit is refused (not an X run, piece not strictly inside, a shifted run of one piece, a curve gap of the edited route that does not hold, no S-bend to that side, a new tile meeting another route or a bridge head). */
std::vector<JunctionTrackRoute> ApplyJunctionDetours(const std::vector<JunctionTrackRoute> &routes, const std::vector<JunctionDetourEdit> &edits, const JunctionSpec &spec, TileIndexDiffC offset, std::string *why);

/** The T of today's node model (L_R main line, L_R branch on the S side), groups in slot order: W side EIn then WOut; S side NIn then SOut; E side WIn then EOut. Movements in JunctionTMovements order. */
const JunctionTopology &JunctionTTopology();

/** An LL_RR main line with an L_R sideline on the S side (D186-D188, D202): W side e-in (2 lanes) then w-out (2); S side branch-in (1) then branch-out (1); E side w-in (2) then e-out (2). Movements: e through, w through, e exit, w exit, join to e-out, join to w-out. */
const JunctionTopology &JunctionLLRRSidelineTopology();

#endif /* JUNCTION_TOPOLOGY_H */
