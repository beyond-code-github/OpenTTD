/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_hover.h The hover plan: the projection pipeline on a terrain snapshot, with terraform (H1). */

#ifndef JUNCTION_HOVER_H
#define JUNCTION_HOVER_H

#include "economy_type.h"
#include "junction_layout.h"
#include "junction_terraform.h"
#include "junction_topology.h"

#include <optional>
#include <string>

/** The result of planning a junction on a snapshot (canonical frame). */
struct JunctionHoverPlan {
	std::optional<JunctionLayout> layout; ///< The signalled layout in the canonical frame, nullopt when no placement was found.
	JunctionTerraform terraform; ///< The terraform plan the layout needs.
	Money height_cost = 0; ///< The placement's height cost (terraform, foundations and clearing).
	std::vector<std::pair<TileIndexDiffC, int8_t>> track_levels; ///< The level of every track tile of layout (the layout's frame, as the placement's).
	std::string why; ///< The reason when there is no layout.
};

/**
 * Plan a junction on a terrain snapshot: facts placement of the seed's choices with terraform, render, signals.
 * Reads the seed and the spec only; touches no global state, so threads may share one seed.
 * @param seed The graph seed of a topology and spec.
 * @param spec The junction spec the seed was made for.
 * @param terrain The snapshot, canonical frame.
 * @param offset The snapshot tile of the placement's tile (0, 0).
 * @return The plan.
 */
JunctionHoverPlan PlanJunctionHover(const JunctionGraphSeed &seed, const JunctionSpec &spec, const JunctionTerrain &terrain, TileIndexDiffC offset);

#endif /* JUNCTION_HOVER_H */
