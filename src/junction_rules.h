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

#include <string>
#include <vector>

enum class JunctionRule : uint8_t { NotImplemented, Connectivity, Geometry, CurveSpeed, Order, Bridges, Signals, WaitingRoom, Deadlock, Prio, Footprint };
struct RuleViolation { JunctionRule rule; TileIndexDiffC tile; std::string message; };
std::vector<RuleViolation> CheckJunctionRules(const JunctionLayout &layout, const JunctionSpec &spec);

#endif /* JUNCTION_RULES_H */
