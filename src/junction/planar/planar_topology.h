/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file planar_topology.h The planar junction topology (DESIGN-planar §1): ports, lines, routes, nodes, derived crossings and event order. */

#ifndef PLANAR_TOPOLOGY_H
#define PLANAR_TOPOLOGY_H

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/** A side of the box in the canonical frame (x east, y south; the branch on S). */
enum class PlanarSide : uint8_t { N, E, S, W };

/** A compass heading in the canonical frame, clockwise from north (y grows to the south). */
enum class PlanarHeading : uint8_t { N, NE, E, SE, S, SW, W, NW };

/** One end of a line: a port or a node. */
struct PlanarEnd {
	bool port = false; ///< A port, else a node.
	uint32_t index = UINT32_MAX; ///< Into PlanarTopology::ports or ::nodes.
};

/** A line: one physical track between two ends, labelled with the routes it carries (D306). */
struct PlanarLine {
	std::string name; ///< As authored, e.g. "CB1".
	std::vector<uint32_t> routes; ///< Into PlanarTopology::routes, ascending.
	PlanarEnd from; ///< Where trains enter the line.
	PlanarEnd to; ///< Where trains leave it.
};

/** One (side, direction) group of lines at the box: its lines left to right across travel. */
struct PlanarPort {
	PlanarSide side;
	bool in; ///< Trains enter the box here.
	PlanarHeading heading; ///< The indicative travel direction at the port.
	std::vector<uint32_t> lines; ///< Left to right across travel.
};

/** A node: a 1 -> 2 split or a 2 -> 1 merge; legs left to right across travel. */
struct PlanarNode {
	std::vector<uint32_t> in; ///< Lines arriving.
	std::vector<uint32_t> out; ///< Lines leaving.
	uint32_t group; ///< The authored node statement it came from.
	bool IsSplit() const { return this->out.size() == 2; }
};

/** A route: a train's path from an entry port line to one or more exit port lines (a lane path, D301). */
struct PlanarRoute {
	std::string name; ///< Letter and lane, e.g. "B1".
	bool prio = false; ///< D301: holds priority at its merges.
	std::vector<std::vector<uint32_t>> paths; ///< Every line sequence from its entry port line to an exit port line; more than one for a choice route.
};

/** Two lines that cross (a level relation, D307). */
struct PlanarCrossing {
	uint32_t a; ///< The line with the lower index.
	uint32_t b;
};

/** A topology as authored, with what follows from it (DESIGN-planar §1). */
struct PlanarTopology {
	std::string name;
	std::vector<PlanarPort> ports; ///< In authored order.
	std::vector<PlanarLine> lines;
	std::vector<PlanarRoute> routes;
	std::vector<PlanarNode> nodes;
	std::vector<PlanarCrossing> crossings; ///< Derived: sorted by (a, b).
	std::vector<std::vector<uint32_t>> events; ///< Derived: per line, its crossings (indices) in travel order.
	std::vector<uint32_t> ring; ///< Derived: the port lines clockwise around the box from the north-west corner.
};

/**
 * Parse a topology in the engineer's notation and derive its crossings, ring and event order.
 * Format, one statement per line ('#' starts a comment):
 * - `topology <name>`
 * - `port <N|E|S|W> <in|out> <heading>: <line>, <line>, ...` the lines left to right across travel; a side's ports in reading order (N and S west to east, W and E north to south).
 * - `node <line>, ... -> <line>, ...` a group of 1 -> 2 splits (fewer in than out) or 2 -> 1 merges (more in than out); legs keep their order.
 * - `prio <route>, ...` the routes that hold priority at their merges (D301).
 * A line's name is capital letters, a lane number and an optional lower-case suffix: `CB1` carries routes C1 and B1, `C1a` carries C1.
 * @param text The topology text.
 * @param error Receives the reason when the result is empty (no lines).
 * @return The topology.
 */
PlanarTopology ParsePlanarTopology(std::string_view text, std::string *error);

/** The T (main line W-E, branch S, the engineer's drawing turned to the canonical frame) at the given lane count, as topology text. */
std::string PlanarTText(int lanes);

/** The index of a line by name, or UINT32_MAX. */
uint32_t PlanarLineByName(const PlanarTopology &topology, std::string_view name);

/** The crossings as "X×Y" name pairs (the lower name first), sorted, for messages and tests. */
std::vector<std::string> PlanarCrossingNames(const PlanarTopology &topology);

#endif /* PLANAR_TOPOLOGY_H */
