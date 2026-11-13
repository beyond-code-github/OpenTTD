/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_levels.h The height solve of a junction placement: one level per track tile, solved jointly (DESIGN-G1 §15.1-§15.3; D218, D219). */

#ifndef JUNCTION_LEVELS_H
#define JUNCTION_LEVELS_H

#include "junction_facts.h"
#include "junction_topology.h"

#include <string>
#include <vector>

/**
 * Choose the level and shape of every track tile of a rendered placement, and the terraform that makes the ground fit.
 * One level per track tile is solved jointly (DESIGN-G1 §15.3, D218, D219): continuity along every route (a tile that is not a lone axis track continues flat, a ramp tile moves one level), the ramp rule L(t + tl) - L(t) <= k per route, a crosser under a bridge middle at or below both heads, a corner shared by two track tiles one height, track corners within distance d at most d apart, then a coordinate descent on the cost. The targets of every tile whose corners must move are merged (a tile on a foundation takes any corner from its ground up to its level) and completed by PropagateJunctionTerraform with every corner of every track tile fixed.
 * @param terrain The snapshot (canonical frame; the placement's tile (x, y) is the snapshot's (x + offset)).
 * @param facts The facts of terrain.
 * @param offset The snapshot tile of the placement's (0, 0).
 * @param ramp_levels JunctionRampLevels(spec).
 * @param train_length Tiles of the train (spec.tl).
 * @param placement The placement (ports and nodes).
 * @param layout The render of placement with the signal pass's prio elements drawn (tracks and bridges).
 * @param routes The routes of that render.
 * @param prio_beside The tiles of the prio elements the signal pass draws, each with the main-line tile beside it (layout is the signalled layout): each is flat and at that tile's level.
 * @param[out] plan The terraform plan.
 * @param[out] cost The total cost: the plan's, plus foundations and clearing of the tiles it does not touch.
 * @param[out] levels The level of every track tile (placement frame; a ramp tile's entry level in the direction of travel).
 * @param[out] why The reason on failure ("terraform conflict ..." when two tiles or a propagation want a corner at different heights).
 * @param[in,out] incremental_check Tests only: when set, the descent also computes every incremental cost in full and counts (first) the comparisons and (second) the differences.
 * @return Whether the levels were solved.
 */
bool SolveJunctionLevels(const JunctionTerrain &terrain, const JunctionFacts &facts, TileIndexDiffC offset, int ramp_levels, int train_length, const JunctionTrackPlacement &placement, const JunctionLayout &layout, const std::vector<JunctionTrackRoute> &routes, const std::vector<std::pair<TileIndexDiffC, TileIndexDiffC>> &prio_beside, JunctionTerraform &plan, Money &cost, std::vector<std::pair<TileIndexDiffC, int8_t>> &levels, std::string *why, std::pair<long, long> *incremental_check = nullptr);

#endif /* JUNCTION_LEVELS_H */
