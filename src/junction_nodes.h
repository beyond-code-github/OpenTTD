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
	uint16_t cls; ///< Its x variable (an X class, < x_classes); x = value(cls) + tile.x - flat[cls].
	uint16_t ycls; ///< Its y variable (a Y class, >= x_classes); y = value(ycls) + tile.y - flat[ycls].
};
/** Two consecutive points on a path; deduplicated over paths. */
struct JunctionNodeLink {
	uint32_t from; ///< Point index, first in travel order.
	uint32_t to;
	bool free; ///< No on_bridge piece from `from` to `to` inclusive, and either same row with X trackdirs or same column with Y trackdirs: its length may change.
	Axis axis; ///< A free link's axis (Axis::X for rigid links).
	bool diagonal; ///< A free diagonal (DESIGN-N3 §6); then `free` is true and `axis` is Axis::X.
};
/** A prio merge's reserved row (DESIGN-N3 §8). */
struct JunctionNodePrioRow {
	uint32_t merge; ///< The merge point on the main-line path.
	int8_t side; ///< +1 or -1: the row of the prio relative to the merge's row (opposite the joiner).
	int8_t dir; ///< The main line's x direction at the merge.
	uint8_t span; ///< Reserved tiles upstream of the merge: prio_tiles, the steps the signal pass lays in the flat T (x_M - p .. x_M).
};
/** A free diagonal: its length changes by two half pieces per map step (DESIGN-N3 §6). */
struct JunctionNodeDiagonal {
	uint32_t link; ///< Index into links.
	int8_t dx; ///< The x of one map step along it (two half pieces).
	int8_t dy; ///< The y of one map step along it.
	uint16_t halves; ///< Flat half pieces from the link's `from` to its `to`, both included.
};
/** A unit rule between two points of a path with a free X run between them: an X difference added after the Y snap (DESIGN-N3 §6). */
struct JunctionNodeLateGap {
	uint32_t path;
	uint32_t p; ///< Point before q on the path.
	uint32_t q;
	int g; ///< Required unit gap.
	int u; ///< Flat unit gap.
	std::vector<uint32_t> diagonals; ///< Indices into JunctionNodeTemplate::diagonals between p and q.
};
/** An X-axis bridge over the interior of a free diagonal (DESIGN-N3 §6). */
struct JunctionNodeLateCrossing {
	uint32_t bridge; ///< Index into bridges.
	uint32_t diagonal; ///< Index into diagonals.
};
/** One traced path of the flat layout as points. */
struct JunctionNodePath {
	JunctionPortRole from;
	JunctionPortRole to;
	std::vector<uint32_t> points; ///< In travel order: every piece of the path that is a point.
	std::vector<uint16_t> units; ///< Per point, the flat unit at which its piece starts along this path.
	int8_t dir; ///< +1 or -1: the x sign of the path's X runs and free diagonals; 0 when it has none.
	int8_t dir_y; ///< +1 (y grows) or -1: the y sign of the path's Y runs and free diagonals; 0 when it has none.
};
/** value(a) - value(b) >= k over variables (two X classes or two Y classes). */
struct JunctionNodeDiff {
	uint16_t a;
	uint16_t b;
	int k;
};
/** A piece of a rigid run (any path piece that is not strictly inside a free link), drawn with its classes. */
struct JunctionNodePiece {
	TileIndexDiffC tile; ///< In the flat layout.
	Track track;
	bool on_bridge;
	uint16_t cls; ///< Its X class.
	uint16_t ycls; ///< Its Y class.
};
/** Two consecutive rigid pieces of one group (same X and Y class) that must meet at one height (DESIGN-N3 §10, kind a); tiles in the flat layout. */
struct JunctionNodeJoin {
	TileIndexDiffC from_tile;
	Trackdir from_td;
	TileIndexDiffC to_tile;
	uint16_t cls;
	uint16_t ycls;
};
/** A flat bridge with the variables of its heads. */
struct JunctionNodeBridge {
	JunctionBridge bridge;
	uint16_t cls; ///< The X class of both heads.
	uint16_t ycls; ///< The Y class of both heads.
};
/** The flat layout's positions as X and Y classes and difference constraints (DESIGN-N3 §1-§2). */
struct JunctionNodeTemplate {
	std::vector<JunctionNodePoint> points;
	std::vector<JunctionNodeLink> links;
	std::vector<JunctionNodePath> paths; ///< In the checker's path order.
	std::vector<JunctionNodePiece> pieces; ///< Deduplicated by (tile, track).
	std::vector<JunctionNodeBridge> bridges; ///< Every flat bridge with the classes of its heads.
	std::vector<JunctionNodeJoin> joins; ///< Deduplicated by from_tile, from_td, to_tile.
	uint32_t open_joins = 0; ///< Consecutive pieces of different classes that are not the two ends of one free link: left to the oracle.
	std::vector<JunctionNodeDiff> diffs; ///< DESIGN-N3 §2 rules 1-4 and 6.
	std::vector<JunctionNodeDiagonal> diagonals; ///< The free diagonal links.
	std::vector<JunctionNodeLateGap> late_gaps; ///< The unit rules that need the Y values.
	std::vector<JunctionNodeLateCrossing> late_crossings; ///< The X-axis bridges over free diagonals.
	std::vector<JunctionNodePrioRow> prio_rows; ///< One per prio merge whose joiner is off the merge's row.
	std::vector<int> flat; ///< Per variable, its flat value: X classes (the flat x of the class's first point, lowest point index) then Y classes (the flat y of the class's first point).
	uint16_t x_classes; ///< The number of X classes; the Y classes follow.
	uint16_t anchor; ///< DESIGN-N3 §2 rule 5: the X class of the last point of EIn->SOut; UINT16_MAX (no constraint, no error) when the layout has no EIn->SOut path.
	uint16_t anchor_y; ///< The Y class of the first point of EIn->SOut (on A); UINT16_MAX when anchor is.
	std::string error; ///< Empty when the template was built.
};
/** Read the template of a flat layout. */
JunctionNodeTemplate BuildJunctionNodeTemplate(const JunctionLayout &flat, const JunctionSpec &spec);
/** Add the template's diffs and the anchors (fixed to flat[anchor] and flat[anchor_y]) to positions; its first tpl.flat.size() variables are the X then Y classes and extra variables are allowed; false when infeasible. */
bool AddJunctionNodeConstraints(const JunctionNodeTemplate &tpl, JunctionPositions &positions);
/** Add the constraints that need the Y values (DESIGN-N3 §6 phase 3): free diagonals' x, late gaps, late crossings. values holds every Y variable's fixed value (X entries ignored); false when infeasible. */
bool AddJunctionNodeLateConstraints(const JunctionNodeTemplate &tpl, JunctionPositions &positions, const std::vector<int> &values);
/**
 * Add the occupancy chain of DESIGN-N3 §7 for the current rows (phase 3, after AddJunctionNodeLateConstraints).
 * @param values Every Y variable's fixed value (X entries ignored).
 * @param[out] row On failure, the layout row (current) where it failed.
 * @return False when two elements overlap in the flat order on one row, or a constraint is infeasible.
 */
bool AddJunctionNodeOccupancy(const JunctionNodeTemplate &tpl, JunctionPositions &positions, const std::vector<int> &values, int &row);
/** Fix the Y variables in index order to their flat values clamped into [Lo, Up], add the late constraints (AddJunctionNodeLateConstraints) and the occupancy chain (AddJunctionNodeOccupancy), then fix the X variables the same way (difference constraints only: every value in range extends); all values in index order, or empty when infeasible. */
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
		Occupancy, ///< Two elements overlap in the flat order on one row, or the occupancy chain is infeasible (DESIGN-N3 §7).
		Height, ///< No row for a Y class meets the facts and heights of its pieces (DESIGN-N3 §10); cls is the Y class.
	};
	Kind kind;
	uint32_t link = UINT32_MAX; ///< Straight/Break/Edge: the X-free link (index into tpl.links).
	int column = 0; ///< Occupancy: the layout row where it failed. Straight/Break/Edge: the working x of the bad tile (Break: the tile before the height break, in +x order; Edge: MinX() - 1 or MaxX() + 1).
	uint16_t cls = UINT16_MAX; ///< Piece: the X class whose pieces (or whose vertical runs) fit nowhere left. Height: the Y class.
};
/** The result of one node solve. */
struct JunctionNodeSolve {
	std::optional<JunctionLayout> layout; ///< Rendered, signalled and normalised (as FinishJunctionCandidate: Normalise, PlaceJunctionSignals, Normalise).
	std::vector<int> values; ///< Per variable (X classes, then Y classes), when solved.
	std::optional<JunctionNodeFailure> failure;
};
/** Solve the template's positions on the facts (DESIGN-N3 section 2 and the facts of section 4) and render; offset maps layout to working coordinates. */
JunctionNodeSolve SolveJunctionNodes(const JunctionNodeTemplate &tpl, const JunctionSpec &spec, const JunctionFacts &facts, TileIndexDiffC offset);

/**
 * Whether the heights of a layout's traced paths connect on the terrain, by the game's own rail foundations (D194): every piece not on a bridge must Fit with its whole tile's tracks,
 * and consecutive such pieces meet iff the exit edge of the first and the entry edge of the second have equal EdgeZ, each computed with its whole tile's tracks.
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
	std::optional<JunctionLayout> layout; ///< Fitting and with connected heights; rule-clean by construction (D193; the [nodeoracle] test checks it).
	TileIndexDiffC offset{}; ///< For the returned layout: working = layout + offset.
	int states = 0; ///< States solved.
	int checked = 0; ///< Times the rule checker ran inside the search (D193: none; it is a test oracle).
	std::string log; ///< One line per state: "state <n> detours <k>: <ok | failure kind link column | template error | heights: <why>>".
};
/** Best-first search over detours from the flat layout (DESIGN-N3 §4): at most max_states states solved. */
JunctionNodeSearch SearchJunctionNodes(const JunctionLayout &flat, const JunctionSpec &spec, const JunctionTerrain &terrain, TileIndexDiffC offset, int max_states);

#endif /* JUNCTION_NODES_H */
