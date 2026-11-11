/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_nodes.h The node model's position template read from a flat layout, and its render (DESIGN-N3). */

#ifndef JUNCTION_NODES_H
#define JUNCTION_NODES_H

#include "junction_facts.h"
#include "junction_generator.h"
#include "junction_layout.h"
#include "junction_positions.h"
#include "junction_rules.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

/** A piece the node model positions (DESIGN-N3 §1): the first or last piece of a run on some path, a switch piece, or the first or last on_bridge piece of a bridge on a path. */
struct JunctionNodePoint {
	TileIndexDiffC tile; ///< In the flat layout.
	Trackdir trackdir;
	uint16_t cls; ///< Its x class; x = value(cls) + tile.x - flat[cls].
};
/** Two consecutive points on a path; deduplicated over paths. */
struct JunctionNodeLink {
	uint32_t from; ///< Point index, first in travel order.
	uint32_t to;
	bool free; ///< Same row, travel along X, no on_bridge piece from `from` to `to` inclusive: its length may change.
};
/** One traced path of the flat layout as points. */
struct JunctionNodePath {
	JunctionPortRole from;
	JunctionPortRole to;
	std::vector<uint32_t> points; ///< In travel order: every piece of the path that is a point.
	std::vector<uint16_t> units; ///< Per point, the flat unit at which its piece starts along this path.
	int8_t dir; ///< +1 or -1: the x direction of travel of the path's free links; 0 when it has none.
};
/** value(a) - value(b) >= k over classes. */
struct JunctionNodeDiff {
	uint16_t a;
	uint16_t b;
	int k;
};
/** A piece of a rigid run (any path piece that is not strictly inside a free link), drawn with its class. */
struct JunctionNodePiece {
	TileIndexDiffC tile; ///< In the flat layout.
	Track track;
	bool on_bridge;
	uint16_t cls;
};
/** The flat layout's positions as classes and difference constraints (DESIGN-N3 §1-§2). */
struct JunctionNodeTemplate {
	std::vector<JunctionNodePoint> points;
	std::vector<JunctionNodeLink> links;
	std::vector<JunctionNodePath> paths; ///< In the checker's path order.
	std::vector<JunctionNodePiece> pieces; ///< Deduplicated by (tile, track).
	std::vector<std::pair<JunctionBridge, uint16_t>> bridges; ///< Every flat bridge with the class of its heads.
	std::vector<JunctionNodeDiff> diffs; ///< DESIGN-N3 §2 rules 1-4 and 6.
	std::vector<int> flat; ///< Per class, its flat value: the flat x of the class's first point (lowest point index).
	uint16_t anchor; ///< DESIGN-N3 §2 rule 5; UINT16_MAX (no constraint, no error) when the layout has no EIn->SOut path.
	std::string error; ///< Empty when the template was built.
};
/** Read the template of a flat layout. */
JunctionNodeTemplate BuildJunctionNodeTemplate(const JunctionLayout &flat, const JunctionSpec &spec);
/** Add the template's diffs and the anchor (fixed to flat[anchor]) to positions; its first tpl.flat.size() variables are the classes and extra variables are allowed; false when infeasible. */
bool AddJunctionNodeConstraints(const JunctionNodeTemplate &tpl, JunctionPositions &positions);
/** Fix every class in index order to its flat value clamped into [Lo, Up] (difference constraints only: every value in range extends); the resulting values. */
std::vector<int> JunctionNodeSnap(const JunctionNodeTemplate &tpl, JunctionPositions &positions);
/** Draw the paths' track and the bridges at the classes' values (DESIGN-N3 §3) and normalise; no signals and no prio rows. */
JunctionLayout RenderJunctionNodeTracks(const JunctionNodeTemplate &tpl, const std::vector<int> &values);

/** Whether track bits fit a working tile by its facts: two or more tracks -> Switch; one X or Y track -> Straight along that axis; one diagonal (half) track -> Half. */
bool JunctionNodeTrackFits(const JunctionFacts &facts, int x, int y, TrackBits bits);

/** What made a node solve infeasible: the first constraint, in the order of SolveJunctionNodes, that could not be added. */
struct JunctionNodeFailure {
	enum class Kind : uint8_t {
		Template,
		Piece,
		Straight,
		Break,
		Edge, ///< The interior of a free link cannot stay inside the snapshot.
	};
	Kind kind;
	uint32_t link = UINT32_MAX; ///< Straight/Break/Edge: the free link (index into tpl.links).
	int column = 0; ///< Straight/Break/Edge: the working x of the bad tile (Break: the tile before the height break, in +x order; Edge: MinX() - 1 or MaxX() + 1).
	uint16_t cls = UINT16_MAX; ///< Piece: the class whose pieces fit nowhere left.
};
/** The result of one node solve. */
struct JunctionNodeSolve {
	std::optional<JunctionLayout> layout; ///< Rendered, signalled and normalised (as FinishJunctionCandidate: Normalise, PlaceJunctionSignals, Normalise).
	std::vector<int> values; ///< Per class, when solved.
	std::optional<JunctionNodeFailure> failure;
};
/** Solve the template's positions on the facts (DESIGN-N3 section 2 and the facts of section 4) and render; offset maps layout to working coordinates. */
JunctionNodeSolve SolveJunctionNodes(const JunctionNodeTemplate &tpl, const JunctionSpec &spec, const JunctionFacts &facts, TileIndexDiffC offset);

/**
 * Whether the heights of a layout's traced paths connect on the terrain, with the line router's rules (HeightStep/RampStep): along each path the track has a height z;
 * an axis piece on a Buildable, non-steep tile inclined along its direction is a ramp (z == z_min going up -> z + 1; z == z_max going down -> z - 1), any other piece needs UsableAt(tile, z);
 * on_bridge pieces keep z and are not checked. The first piece of a path starts at z_min if it is a ramp up, z_max if a ramp down, otherwise at z_max.
 * Pieces shared by several paths must get the same z on each.
 * @param offset Maps layout tiles to the terrain's coordinates (working = layout + offset).
 * @param why When set and the result is false, receives "path <i> piece <j> at x,y: <reason>".
 */
bool JunctionNodeHeightsOk(const JunctionLayout &layout, const JunctionSpec &spec, const JunctionTerrain &terrain, TileIndexDiffC offset, std::string *why);

/**
 * Replace the interior of a free link of a state layout by a detour around one column (DESIGN-N3 §4).
 * @param layout The state layout (not normalised by this function).
 * @param tpl Its template.
 * @param link A free link of tpl.
 * @param column The layout x the detour's shifted run must cover.
 * @param side +1 or -1: the row the shifted run is on, relative to the link's row.
 * @param spec The spec the child layout's template is built with.
 * @param why On failure, receives the reason.
 * @return The new layout, or std::nullopt.
 */
std::optional<JunctionLayout> InsertJunctionDetour(const JunctionLayout &layout, const JunctionNodeTemplate &tpl, uint32_t link, int column, int side, const JunctionSpec &spec, std::string *why);

/** The result of a structure search. */
struct JunctionNodeSearch {
	std::optional<JunctionLayout> layout; ///< Rule-clean, fitting and with connected heights.
	int states = 0; ///< States solved.
	std::string log; ///< One line per state: "state <n> detours <k>: <ok | failure kind link column | template error | rules: <first violation> | heights: <why>>".
};
/** Best-first search over detours from the flat layout (DESIGN-N3 §4): at most max_states states solved. */
JunctionNodeSearch SearchJunctionNodes(const JunctionLayout &flat, const JunctionSpec &spec, const JunctionTerrain &terrain, TileIndexDiffC offset, int max_states);

#endif /* JUNCTION_NODES_H */
