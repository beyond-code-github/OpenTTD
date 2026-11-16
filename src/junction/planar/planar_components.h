/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file planar_components.h The planar component solver (DESIGN-components §6): coop components inside bundles, solved as one integer program per variant. */

#ifndef PLANAR_COMPONENTS_H
#define PLANAR_COMPONENTS_H

#include "planar_layout.h"
#include "planar_topology.h"
#include "../../map_type.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

struct JunctionSpec;

/** One piece of a route program: an axis step (2 units) or a diagonal half step (1 unit) of a route path, in tiles of the plan's frame. */
struct PlanarProgramPiece {
	TileIndexDiffC tile;
	uint8_t heading; ///< 0 N, 1 NE, 2 E, ... 7 NW (clockwise, y down).
	PlanarPiece::Kind kind; ///< Ground, or a deck's Head or Middle piece.
	uint8_t line; ///< Index of the piece's line in the route path.
};

/** A route path's program for one choice of copy per doubled stretch. */
struct PlanarRouteProgram {
	uint32_t route; ///< Index into PlanarTopology::routes.
	uint32_t path; ///< Index into that route's paths.
	uint8_t copy; ///< The copy choice (the probe's mask over the path's doubled stretches, in its order).
	std::vector<PlanarProgramPiece> pieces;
	std::vector<std::pair<bool, uint32_t>> copy_switches; ///< (true: copy split at this piece, false: copy merge at this piece), the probe's S and M marks.
};

/** A solved component layout (DESIGN-components §6). */
struct PlanarComponentLayout {
	int width = 0; ///< Box width in tiles.
	int height = 0; ///< Box height in tiles.
	double area = 0; ///< Tiles inside the junction outline.
	std::string variant; ///< The winning variant's tag, as the probe prints it.
	int64_t micros = 0; ///< Wall time of the whole solve.
	std::vector<PlanarRouteProgram> routes;
};

/** Solve the component layout of a T2 or LL2 topology: every variant (in parallel), the least area wins. @return false with why set when the topology has no composition or no variant solves. */
bool SolvePlanarComponents(const PlanarTopology &topology, const JunctionSpec &spec, PlanarComponentLayout &layout, std::string *why);
bool SolvePlanarMapped(const PlanarTopology &topology, const JunctionSpec &spec, PlanarComponentLayout &layout, std::string *why);

#endif /* PLANAR_COMPONENTS_H */
