/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_levels.h The height solve of a junction placement: a level per tile on every edge by a shortest path (DESIGN-G1 §15.1-§15.3; D218, D219). */

#ifndef JUNCTION_LEVELS_H
#define JUNCTION_LEVELS_H

#include "junction_facts.h"
#include "junction_topology.h"

#include <string>
#include <vector>

/**
 * Choose the level and shape of every track tile of a rendered placement, and the terraform that makes the ground fit.
 * Node tiles, port tiles and bridge heads sit flat at their cheapest level alone; between two such tiles an edge's tiles are solved alone by a shortest path over (tile, exit level, tiles since the last up-ramp) with ramp tiles (axis pieces along their axis, one level) and the ramp rule (D219); a crosser under a bridge middle stays at or below the bridge's lower head. The targets of every tile whose corners must move are merged and completed by PropagateJunctionTerraform with every corner of every track tile fixed.
 * @param terrain The snapshot (canonical frame; the placement's tile (x, y) is the snapshot's (x + offset)).
 * @param facts The facts of terrain.
 * @param offset The snapshot tile of the placement's (0, 0).
 * @param ramp_levels JunctionRampLevels(spec).
 * @param train_length Tiles of the train (spec.tl).
 * @param placement The placement (ports and nodes).
 * @param layout The render of placement (tracks and bridges).
 * @param routes The routes of that render.
 * @param[out] plan The terraform plan.
 * @param[out] cost The total cost: the plan's, plus foundations and clearing of the tiles it does not touch.
 * @param[out] why The reason on failure ("terraform conflict ..." when two tiles or a propagation want a corner at different heights).
 * @return Whether the levels were solved.
 */
bool SolveJunctionLevels(const JunctionTerrain &terrain, const JunctionFacts &facts, TileIndexDiffC offset, int ramp_levels, int train_length, const JunctionTrackPlacement &placement, const JunctionLayout &layout, const std::vector<JunctionTrackRoute> &routes, JunctionTerraform &plan, Money &cost, std::string *why);

#endif /* JUNCTION_LEVELS_H */
