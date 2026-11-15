/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_block.h Junction block programs: data tables for a block's tracks, and the generic generator that solves them (DESIGN-blocks). */

#ifndef JUNCTION_BLOCK_H
#define JUNCTION_BLOCK_H

#include "../../direction_type.h"
#include "../../map_type.h"
#include "../junction_layout.h"
#include "../junction_route.h"
#include "../placement/junction_placement.h"
#include "../topology/junction_topology.h"

#include <cstdint>
#include <string>
#include <vector>

struct JunctionSpec;

/** The kinds of segment of a block edge. */
enum class JunctionBlockSegmentKind : uint8_t { Run, SBend, Bridge };

/** One segment of an edge's track. */
struct JunctionBlockSegment {
	JunctionBlockSegmentKind kind;
	int lines = 0; ///< SBend: signed lines, + is right of travel.
	uint16_t point = 0; ///< Run, Bridge: the point it goes to (Bridge: the far head).
	uint16_t crosses = UINT16_MAX; ///< Bridge: the edge whose S-bend passes under it.
	JunctionTrackStep::Role role = JunctionTrackStep::Role::None; ///< SBend only: LaneSplit or LaneMerge of a doubled edge.
};

/** The end of an edge: a node, or an in port (from) or an out port (to). */
struct JunctionBlockEnd {
	bool node;
	uint16_t index;
};

/** A node of the block's graph. */
struct JunctionBlockNode {
	bool split;
	uint16_t point;
};

/** One track of the block from one end to another. */
struct JunctionBlockEdge {
	JunctionBlockEnd from;
	JunctionBlockEnd to;
	int line; ///< The line the edge starts on.
	std::vector<JunctionBlockSegment> segments;
	std::vector<JunctionBlockSegment> copy; ///< Copy 1's segments of a doubled edge (D80); empty for a single edge.
};

/** The prio element at a merge node, on its row line. */
struct JunctionBlockPrio {
	uint16_t node;
	int line;
};

/** A block program: points, lines, nodes, edges and paths. */
struct JunctionBlockProgram {
	std::vector<std::string> points; ///< Point names; points[0] is "W".
	uint16_t east; ///< The point E.
	std::vector<int> in_lines; ///< Per in port, its line, in across order.
	std::vector<int> out_lines; ///< Per out port, its line.
	std::vector<JunctionBlockNode> nodes;
	std::vector<JunctionBlockEdge> edges;
	std::vector<std::vector<uint16_t>> paths; ///< Per lane path, its edges in travel order.
	std::vector<bool> through; ///< Per path: a through path (no queue bay, R5).
	std::vector<JunctionBlockPrio> prios;
	JunctionTopology topology; ///< The topology of the block's lane paths.
};

/** A port of the solved block, in layout coordinates. */
struct JunctionBlockPort {
	TileIndexDiffC tile;
	Trackdir trackdir;
};

/** A solved block: routes, layout and ports share one frame, x the column and y the line. */
struct JunctionBlock {
	std::vector<int> columns; ///< Per point, its solved column.
	std::vector<JunctionRoute> routes; ///< Per path, and per copy of a path through a doubled edge (copy 0 first), in block coordinates: x is the column, y the line.
	JunctionLayout layout; ///< The routes rasterised and normalised.
	std::vector<JunctionBlockPort> in_ports, out_ports;
	JunctionTopology topology; ///< The program's topology.
	std::vector<JunctionLanePath> paths; ///< ExpandJunctionLanes of the topology.
	JunctionTrackGraph graph; ///< The program's nodes and edges as a track graph.
	JunctionTrackPrograms programs; ///< Edges and copies sized to the graph's edges; a doubled edge has one SBend token per copy, every other list is empty.
	std::vector<JunctionTrackRoute> track_routes; ///< Per route (as `routes`), its pieces in the layout frame with the program edge that emitted each.
	std::vector<JunctionPrioSpan> prio; ///< Per program prio, its tiles and drawn bits.
};

JunctionBlockProgram JunctionJoinBlock227Program();
bool BuildJunctionBlock(const JunctionBlockProgram &program, const JunctionSpec &spec, JunctionBlock &block, std::string *error);
JunctionTopology JunctionJoinBlock227Topology();
JunctionBlockProgram JunctionExitBlockProgram();
JunctionTopology JunctionExitBlockTopology();

#endif /* JUNCTION_BLOCK_H */
