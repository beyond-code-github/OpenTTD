/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_terrain.h Terrain snapshot for the junction generator. */

#ifndef JUNCTION_TERRAIN_H
#define JUNCTION_TERRAIN_H

#include "../../direction_type.h"
#include "../../economy_type.h"
#include "../../map_type.h"
#include "../../slope_type.h"

#include <vector>

/** What occupies a tile, as far as the generator cares. */
enum class JunctionTileClass : uint8_t {
	Buildable, ///< Clear land, rough, rocks, trees, fields: buildable at a cost.
	Water, ///< Sea, canal, river: only under a bridge span.
	Spannable, ///< Plain rail or road of any owner: no track on it, but a bridge span may pass over it (D104).
	Blocked, ///< Houses, other companies' property, unclearable objects.
};

/** One tile of the snapshot. */
struct JunctionTerrainTile {
	int8_t z_min = 0; ///< Lowest corner height.
	int8_t z_max = 0; ///< Highest corner height.
	bool steep = false;
	DiagDirection incline = DiagDirection::Invalid; ///< The canonical direction an inclined slope rises in, Invalid otherwise (D152).
	Slope slope = SLOPE_FLAT; ///< The tile's slope in the canonical frame (corners rotated and mirrored with it, D152), steep bit kept.
	JunctionTileClass cls = JunctionTileClass::Buildable;
	Money clear_cost = 0;
};

/** The canonical uphill direction of an inclined slope under a junction orientation, or DiagDirection::Invalid when the slope isn't inclined (D152). */
DiagDirection JunctionCanonicalIncline(Slope slope, uint8_t rotation, bool mirrored);

/** The slope of a tile under a junction orientation: its corners rotated and mirrored into the canonical frame, steep bit kept (D152). */
Slope JunctionCanonicalSlope(Slope slope, uint8_t rotation, bool mirrored);

/** A terrain snapshot in the generator's canonical frame. Tiles outside the grid are Blocked. */
struct JunctionTerrain {
	TileIndexDiffC origin; ///< Canonical coordinate of grid cell (0,0).
	int width = 0;
	int height = 0;
	std::vector<JunctionTerrainTile> tiles; ///< Row-major, width * height.
	Money foundation_cost = 0; ///< Price of a leveled foundation (BuildFoundation).
	Money terraform_cost = 0; ///< Price of moving one corner one level (PR_TERRAFORM).
	bool build_on_slopes = true;
	int max_bridge_height = 12;
	int max_height = 255; ///< The highest corner height the game allows (map_height_limit); the lowest is 0.

	/** The tile at canonical (x, y), or nullptr outside the grid. */
	const JunctionTerrainTile *At(int x, int y) const
	{
		const int gx = x - origin.x;
		const int gy = y - origin.y;
		if (gx < 0 || gy < 0 || gx >= this->width || gy >= this->height) return nullptr;
		return &this->tiles[static_cast<size_t>(gy) * this->width + gx];
	}

	/** Whether rail can sit at level plane on the tile: flat at plane, or a non-steep slope whose top is plane and build_on_slopes. */
	bool UsableAt(int x, int y, int plane) const
	{
		const JunctionTerrainTile *t = this->At(x, y);
		if (t == nullptr || t->cls != JunctionTileClass::Buildable || t->steep) return false;
		if (t->z_min == plane && t->z_max == plane) return true;
		return this->build_on_slopes && t->z_max == plane && t->z_min < plane;
	}

	/** Clear plus foundation cost of building at plane; only meaningful when UsableAt. */
	Money CostAt(int x, int y, int plane) const
	{
		const JunctionTerrainTile *t = this->At(x, y);
		if (t == nullptr) return 0;
		return t->clear_cost + (t->z_min < plane ? this->foundation_cost : Money(0));
	}

	/** Whether a bridge span at deck level plane+1 may pass over the tile. */
	bool SpanOk(int x, int y, int plane) const
	{
		const JunctionTerrainTile *t = this->At(x, y);
		return t != nullptr && t->cls != JunctionTileClass::Blocked && t->z_max <= plane && plane < t->z_min + this->max_bridge_height;
	}
};

#endif /* JUNCTION_TERRAIN_H */
