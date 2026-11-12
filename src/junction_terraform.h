/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_terraform.h A terraform plan on a terrain snapshot for the junction generator (D218). */

#ifndef JUNCTION_TERRAFORM_H
#define JUNCTION_TERRAFORM_H

#include "economy_type.h"
#include "junction_terrain.h"

#include <array>
#include <map>
#include <set>
#include <string>
#include <utility>

/** A terraform plan (D218): target heights of tile corners in the snapshot's canonical frame; corner (x, y) is the north corner of tile (x, y), i.e. shared by tiles (x-1..x, y-1..y). */
struct JunctionTerraform {
	std::map<std::pair<int, int>, int> corners; ///< Corner -> target height (only corners that change).
	Money cost = 0; ///< Set by PropagateJunctionTerraform.
};

/** Complete the plan as CmdTerraformLand would: every corner orthogonally next to a changed corner is moved so neighbouring corners differ by at most one, repeatedly; fails with the reason (tile, class) when a corner of a tile that cannot be terraformed (not Buildable) or outside the snapshot would move. A corner in fixed (corners that must keep their target or current height) that a propagation would move fails with the reason "terraform conflict" naming the corner and tile. Sets cost (corner-level moves x terraform_cost + clear_cost of each Buildable tile whose corners change). */
bool PropagateJunctionTerraform(const JunctionTerrain &terrain, JunctionTerraform &plan, std::string *why, const std::set<std::pair<int, int>> *fixed = nullptr);

/** The terrain with the plan applied: z_min, z_max, slope, steep and incline recomputed for every tile with a changed corner. */
JunctionTerrain ApplyJunctionTerraform(const JunctionTerrain &terrain, const JunctionTerraform &plan);

/** A tile's four corner heights (canonical corner order W, S, E, N as slope_type.h names them), and the tile back from them. */
std::array<int, 4> JunctionTileCorners(const JunctionTerrainTile &tile);
void SetJunctionTileCorners(JunctionTerrainTile &tile, const std::array<int, 4> &corners);

#endif /* JUNCTION_TERRAFORM_H */
