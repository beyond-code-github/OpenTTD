/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_station_generator.h Station generator (D106). */

#ifndef JUNCTION_STATION_GENERATOR_H
#define JUNCTION_STATION_GENERATOR_H

#include "../legacy/junction_generator.h"
#include "../junction_layout.h"
#include "junction_station_rules.h"

#include <optional>

/** Result of generating a station (D106 v1: ro-ro drop, one incoming direction). */
struct JunctionStationResult {
	std::optional<JunctionLayout> layout; ///< Normalised; trains travel toward +x.
	JunctionStationPorts ports; ///< In and out, in the normalised layout's coordinates.
};

/** Generate a ro-ro drop station with the given number of platforms (2..8), each exactly spec.tl long. */
JunctionStationResult GenerateStation(const JunctionSpec &spec, uint8_t platforms);

#endif /* JUNCTION_STATION_GENERATOR_H */
