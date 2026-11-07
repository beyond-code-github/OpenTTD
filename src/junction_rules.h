/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_rules.h Junction rule checker. */

#ifndef JUNCTION_RULES_H
#define JUNCTION_RULES_H

#include "junction_generator.h"
#include "junction_layout.h"
#include "junction_route.h"
#include "track_type.h"

#include <string>
#include <utility>
#include <vector>

enum class JunctionRule : uint8_t { NotImplemented, Connectivity, Geometry, CurveSpeed, Order, Bridges, Signals, WaitingRoom, Deadlock, Prio, Footprint };
struct RuleViolation { JunctionRule rule; TileIndexDiffC tile; std::string message; };

/** The six places where the mainlines and the branch meet the edge of the junction (PLAN-1b §1). */
enum class JunctionPortRole : uint8_t { EIn, EOut, WIn, WOut, NIn, SOut };
/** One port: a track on the bounding box boundary that leaves through the outer edge. */
struct JunctionPort {
	JunctionPortRole role;
	TileIndexDiffC tile;
	Trackdir trackdir; ///< Travel direction on the port tile (inward for In, outward for Out).
};

/** One traced route through the junction. */
struct JunctionPath {
	JunctionPortRole from;
	JunctionPortRole to;
	std::vector<JunctionPiece> pieces;
};

/** Everything the rule groups work on. */
struct JunctionCheckContext {
	const JunctionLayout &layout;
	const JunctionSpec &spec;
	std::vector<JunctionPort> ports;
	std::vector<std::pair<JunctionPortRole, JunctionPortRole>> expected; ///< Movements that must exist.
	std::vector<JunctionPath> paths; ///< Filled by TraceJunctionPaths.
	std::vector<RuleViolation> trace_problems; ///< Dead ends, cycles, unexpected exits.
};

std::vector<JunctionPort> FindJunctionPorts(const JunctionLayout &layout);
JunctionCheckContext MakeJunctionCheckContext(const JunctionLayout &layout, const JunctionSpec &spec, std::vector<JunctionPort> ports, std::vector<std::pair<JunctionPortRole, JunctionPortRole>> expected);

std::vector<RuleViolation> CheckConnectivity(const JunctionCheckContext &ctx);
std::vector<RuleViolation> CheckGeometry(const JunctionCheckContext &ctx);
std::vector<RuleViolation> CheckOrder(const JunctionCheckContext &ctx);
std::vector<RuleViolation> CheckBridges(const JunctionCheckContext &ctx);

std::vector<RuleViolation> CheckJunctionRules(const JunctionLayout &layout, const JunctionSpec &spec);

#endif /* JUNCTION_RULES_H */
