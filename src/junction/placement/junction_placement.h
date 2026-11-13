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
 */
JunctionTrackPlacement PlaceTrackGraph(const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, const JunctionTrackOrder &order, const JunctionTrackStructure &structure, const JunctionTrackPrograms &programs, const JunctionSpec &spec, const JunctionPlacementChoices *fixed, std::string *error, const std::vector<std::pair<int, int>> &x_floors, const std::vector<std::pair<int, int>> &y_floors, const JunctionFacts *facts = nullptr, TileIndexDiffC offset = {0, 0}, bool heights = true, bool classes_only = false, const JunctionPlacementShared *shared = nullptr, JunctionPlacementShared *make_shared = nullptr, JunctionPlacementShared *record = nullptr);

#endif /* JUNCTION_PLACEMENT_H */
