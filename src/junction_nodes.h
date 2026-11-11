/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_nodes.h The node model's position template read from a flat layout, and its render (DESIGN-N3). */

#ifndef JUNCTION_NODES_H
#define JUNCTION_NODES_H

#include "junction_generator.h"
#include "junction_layout.h"
#include "junction_positions.h"
#include "junction_rules.h"

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
/** Add the template's diffs and the anchor (fixed to flat[anchor]) to positions over template.flat.size() variables; false when infeasible. */
bool AddJunctionNodeConstraints(const JunctionNodeTemplate &tpl, JunctionPositions &positions);
/** Fix every class in index order to its flat value clamped into [Lo, Up] (difference constraints only: every value in range extends); the resulting values. */
std::vector<int> JunctionNodeSnap(const JunctionNodeTemplate &tpl, JunctionPositions &positions);
/** Draw the paths' track and the bridges at the classes' values (DESIGN-N3 §3) and normalise; no signals and no prio rows. */
JunctionLayout RenderJunctionNodeTracks(const JunctionNodeTemplate &tpl, const std::vector<int> &values);

#endif /* JUNCTION_NODES_H */
