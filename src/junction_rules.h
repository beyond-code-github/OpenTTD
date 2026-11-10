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

#include <set>
#include <string>
#include <utility>
#include <vector>

enum class JunctionRule : uint8_t { NotImplemented, Connectivity, Geometry, CurveSpeed, Order, Bridges, Signals, WaitingRoom, Deadlock, Prio, Footprint, PrioReach };
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
std::vector<RuleViolation> CheckCurveSpeed(const JunctionCheckContext &ctx);
/** Rule 3 over a piece sequence: the least CurveSpeedLimitOfTrain over every 2*tl-unit window, the ends padded with straight units as CheckCurveSpeed pads a path. */
int JunctionMinCurveSpeed(const std::vector<JunctionPiece> &pieces, uint8_t tl, uint8_t curve_speed);
std::vector<RuleViolation> CheckBridges(const JunctionCheckContext &ctx);
std::vector<RuleViolation> CheckSignals(const JunctionCheckContext &ctx);
std::vector<RuleViolation> CheckWaitingRoom(const JunctionCheckContext &ctx);
std::vector<RuleViolation> CheckDeadlock(const JunctionCheckContext &ctx);
std::vector<RuleViolation> CheckPrio(const JunctionCheckContext &ctx);
std::vector<RuleViolation> CheckFootprint(const JunctionCheckContext &ctx);
std::vector<RuleViolation> CheckPrioReach(const JunctionCheckContext &ctx);

/** Whether the stretch scanned from 'from' in 'step' direction (up to a bridge piece or a switch) has a signal (D117 a). */
bool JunctionStretchHasSignal(const JunctionLayout &layout, const JunctionPath &path, const std::set<size_t> &switches, size_t from, int step);

std::vector<RuleViolation> CheckJunctionRules(const JunctionLayout &layout, const JunctionSpec &spec);
bool JunctionRulesClean(const JunctionLayout &layout, const JunctionSpec &spec);

#endif /* JUNCTION_RULES_H */
