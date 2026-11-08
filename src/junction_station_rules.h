/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_station_rules.h Station rule checker (D106). */

#ifndef JUNCTION_STATION_RULES_H
#define JUNCTION_STATION_RULES_H

#include "junction_layout.h"
#include "junction_router.h"

#include <string>
#include <vector>

/** Where trains enter and leave a station layout. */
struct JunctionStationPorts {
	JunctionRouteEnd in; ///< First piece of the in stub, heading into the layout.
	JunctionRouteEnd out; ///< Last piece of the out stub, heading out of the layout.
};

/** Rule ids for station checks (ST2). */
enum class JunctionStationRule : uint8_t { Platforms, Connectivity, Signals, Braking };

/** One broken station rule. */
struct StationRuleViolation {
	JunctionStationRule rule;
	TileIndexDiffC tile;
	std::string message;
};

/**
 * Check a station layout against the station rules.
 * @param layout The layout, with one station block.
 * @param ports Where trains enter and leave.
 * @param tl Train length in tiles; also the platform length.
 * @param sd Signal distance in tiles.
 * @return The violations; empty when the layout is clean.
 */
std::vector<StationRuleViolation> CheckStationRules(const JunctionLayout &layout, const JunctionStationPorts &ports, uint8_t tl, uint8_t sd);

#endif /* JUNCTION_STATION_RULES_H */
