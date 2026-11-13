/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_placement.h Horizontal placement of a programmed track graph: the least tiles that meet the rules (DESIGN-G1 §11.8, §14). */

#ifndef JUNCTION_PLACEMENT_H
#define JUNCTION_PLACEMENT_H

#include "../terrain/junction_facts.h"
#include "../junction_layout.h"
#include "../terrain/junction_terraform.h"
#include "../topology/junction_topology.h"

#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

struct JunctionSpec;

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

/** A relation x_v - x_u >= c of one axis of the placement's last rounds, with what c depends on (DESIGN-G3 §G3.3). */
struct JunctionSystemRelation {
	uint8_t axis; ///< 0 X, 1 Y.
	int v, u, c;
	int bend = -1; ///< When >= 0 and not nonlinear: c = a * L(bend) + b.
	int a = 0, b = 0;
	bool nonlinear = false; ///< c depends on lengths non-linearly; held at c.
	bool zero_branch = false; ///< Holds only when the bend's S-fan size is 0 (see solve.py), else only when it is not.
	bool order_pin = false; ///< Pins a head to the outermost crosser by the flat solve's order (the hug); the joint model drops it.
	std::string reason; ///< The why() text with edge, node and row.
};

/** One bend of the placement's chains (a Corner, an S-bend, an S fan or a fan), with how its size is known. */
struct JunctionSystemBend {
	int id; ///< The bend's index in JunctionPlacementSystem::bends; chain elements name it by this.
	int edge; ///< The edge whose chain holds it.
	int copy; ///< 0 the edge's chain, 1 the copy of a doubled row leg.
	int tok; ///< The token of the edge's program it is.
	std::string kind; ///< "Corner", "Fan", "SFan" or "SBend".
	/** The size's source: free (read back from the columns), rows (from a Y difference), corner_rows (joiner Corner, from the rows), port_rows (a W/E fan, from the port rows), fixed_k (a Corner of k), one_row (an item-less horizontal S-bend). */
	std::string source;
	int hand; ///< The bend's hand.
	int hin; ///< The heading the bend starts on.
	int first; ///< The point at its start.
	int last; ///< The point at its end.
	int item_pt; ///< The point of the stack item it ends on, -1 when none.
	int m; ///< Its size at the recording (a Corner's: the row span when the rows are known, else k).
	int floor; ///< The least size the placement allows it.
};

/** One element of a chain, in chain order; consecutive elements of one (edge, copy) form its chain. */
struct JunctionSystemElement {
	int edge; ///< The chain's edge.
	int copy; ///< The chain's copy, as JunctionSystemBend::copy.
	bool tunnel; ///< The chain's edge is carried by a tunnel.
	std::string k; ///< "Tile", "Corner", "SBend", "Heads", "Fan", "HeadsPair" or "Prio".
	int first; ///< The element's first point.
	int last; ///< The element's last point.
	int hin; ///< The heading it starts on.
	int hout; ///< The heading it ends on.
	int bend; ///< The id of its bend, -1 when it is none.
};

/** One bend of a route as the curve-gap counters read it (DESIGN-G3 §G3.3a): its raster quantities q(L) = first + (L - 1) * (second - first) as (q at L = 1, q at L = 2), or (q, q) when its size is constant. */
struct JunctionSystemRouteBend {
	int pt; ///< The point at the bend's first half tile.
	int bend; ///< The JunctionPlacementSystem bend whose length it has, -1 when its halves are constant.
	int hin; ///< The heading it starts on.
	int hout; ///< The heading it leaves on.
	int hand; ///< The bend's hand.
	bool corner; ///< The raster is a Corner's.
	std::array<std::pair<int, int>, 5> quantity; ///< before_dx, before_dy, close_dx, close_dy, close_units.
	std::vector<std::pair<int, int>> curve_units; ///< Per curve, its units from the start of the axis piece before the bend.
};

/** One route (lane path and copy, N side skipped) of the curve-gap counters. */
struct JunctionSystemRoute {
	int start_pt; ///< The route's start point.
	int start_heading; ///< The heading it starts on.
	std::vector<JunctionSystemRouteBend> bends; ///< In travel order.
	std::vector<int8_t> hands; ///< Each curve's hand in travel order (+1 right, -1 left).
};

/** The relation system of the last X and the last Y round of one placement call, with each constant's dependence on a bend's length (DESIGN-G3 §G3.3). */
struct JunctionPlacementSystem {
	int pt_count = 0; ///< The number of placement points.
	int k = 0; ///< The Corner's least size.
	bool rows_known = false; ///< Whether the bends' sizes were taken after the rows were known.
	std::vector<int> item_pt; ///< Per stack item, its point.
	std::vector<int> port_row; ///< Per port slot, its row.
	int port_offset = 0; ///< The port rows' offset.
	int64_t rail_piece = 0; ///< The price of a rail piece.
	int64_t signal = 0; ///< The price of a signal.
	int64_t bridge0 = 0; ///< The price of a bridge of no middle tiles.
	int64_t bridge1 = 0; ///< The price of a bridge of one middle tile.
	int sd = 0; ///< The signal distance, in track pieces.
	std::vector<uint32_t> tokens; ///< Per edge, its program's token count, the fixed path's choices shape.
	std::vector<std::vector<int>> paths; ///< Per lane path, its edges.
	std::vector<JunctionSystemBend> bends; ///< The bends, from the last round whose rows were known (else the last Y round).
	std::vector<JunctionSystemElement> chains; ///< The elements of every chain.
	std::vector<JunctionSystemRelation> relations; ///< The X relations, then the Y relations.
	std::vector<JunctionSystemRoute> routes; ///< The routes of the last X round whose rows were known, for the curve-gap counters; empty when none.
	std::vector<std::array<int, 5>> route_curves; ///< For tests: per route curve (route, curve in travel order, xoff, yoff, seg) of the walk's curve forms.
};

/** One prio element as the placement fixes it (DESIGN-G1 12.4). */
struct JunctionPrioSpan {
	std::vector<std::pair<TileIndexDiffC, TrackBits>> tiles; ///< Its tiles in drawing order (per step upstream of the merge: the main-line tile's corner half on even steps, then the row tile) with the bits drawn on each.
	TileIndexDiffC merge; ///< The joiner merge's tile (step 0 on the main line).
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
	std::vector<JunctionPrioSpan> prio; ///< Per prio element, its tiles and drawn bits (placement frame), from the Prio Elem.
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

/** The placement with its choices fixed (see above), returning the relation system it solved (DESIGN-G3 §G3.3): its last X and Y rounds, with each constant's dependence on a bend's length. Empty when the placement fails before its first solve. */
JunctionPlacementSystem RecordJunctionPlacementSystem(const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, const JunctionTrackOrder &order, const JunctionTrackStructure &structure, const JunctionTrackPrograms &programs, const JunctionSpec &spec, const JunctionPlacementChoices &choices);

/** The stages of the facts placement that do not read the facts or the offset (the Y phase: its clearance relations from the flat solve, and the Y solution), solved once for a near search over many offsets. */
struct JunctionPlacementShared {
	bool ok = false; ///< False when the stages did not solve: the placement then computes them itself (and reports their failure as today).
	std::vector<int> vy; ///< The rows of the Y solve, per point.
	/* The X phase's offset-independent shape, for the early reject of a near offset. */
	bool near_ok = false;
	int near_pts = 0;
	std::vector<std::array<int, 3>> near_groups; ///< (point, column offset, row) of every tile group the facts must allow.
	std::vector<std::array<int, 4>> near_runs; ///< (a, b, row, sense) of every straight run on a row.
	std::vector<std::array<int, 3>> near_cons; ///< (v, u, k): x_v - x_u >= k.
	std::vector<int> near_lo; ///< The least column of every point under the relations alone.
};

/** True when the X solve of the class facts at this offset must fail (a sound bounds propagation over the shared relations, the groups and the runs); false proves nothing. The error text is not produced. */
bool JunctionPlacementNearReject(const JunctionPlacementShared &shared, const JunctionFacts &facts, TileIndexDiffC offset);

/** The shared stages for these inputs (the same as those of the facts overload, with the choices fixed). */
JunctionPlacementShared MakeJunctionPlacementShared(const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, const JunctionTrackOrder &order, const JunctionTrackStructure &structure, const JunctionTrackPrograms &programs, const JunctionSpec &spec, const JunctionPlacementChoices &choices);

/**
 * The placement of a programmed track graph (DESIGN-G1 §11.8, §14.2).
 * @param fixed The choices to solve with (one Y and one X solve), or nullptr for the rounds that find them.
 * @param x_floors (point, least column) pairs for the X phase (see the public overload).
 * @param y_floors (point, least row) pairs for the Y phase (see the public overload).
 * @param facts The target terrain's facts for the X phase of a fixed solve (DESIGN-G1 §14.4), or nullptr.
 * @param offset The working tile of the placement's (0, 0); only with facts.
 * @param heights False: only the class facts (D218): the height steps, joins and element fits are skipped.
 * @param classes_only True (the height solve's X phase): every tile check is JunctionFacts::Buildable, the game's slope rules are the level solve's.
 * @param system When non-null, receives the last X and last Y relation systems of the call (DESIGN-G3 §G3.3); the result is the same either way.
 */
JunctionTrackPlacement PlaceTrackGraph(const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, const JunctionTrackOrder &order, const JunctionTrackStructure &structure, const JunctionTrackPrograms &programs, const JunctionSpec &spec, const JunctionPlacementChoices *fixed, std::string *error, const std::vector<std::pair<int, int>> &x_floors, const std::vector<std::pair<int, int>> &y_floors, const JunctionFacts *facts = nullptr, TileIndexDiffC offset = {0, 0}, bool heights = true, bool classes_only = false, const JunctionPlacementShared *shared = nullptr, JunctionPlacementShared *make_shared = nullptr, JunctionPlacementShared *record = nullptr, JunctionPlacementSystem *system = nullptr);

#endif /* JUNCTION_PLACEMENT_H */
