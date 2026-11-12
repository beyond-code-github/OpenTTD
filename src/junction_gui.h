/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */


/** @file junction_gui.h Client-side store and drawing of the junction placement ghost. */

#ifndef JUNCTION_GUI_H
#define JUNCTION_GUI_H

#include "junction_layout.h"
#include "slope_type.h"
#include "bridge_type.h"
#include "rail_type.h"
#include "direction_type.h"

#include <span>

/** Ghost of a bridge head on one tile. */
struct JunctionGhostBridgeHead {
	BridgeType type;
	DiagDirection dir; ///< Direction from this head towards the other head.
};

/** Ghost of a bridge span over one tile. */
struct JunctionGhostBridgeSpan {
	BridgeType type;
	Axis axis;
	int bridge_z; ///< Deck height in pixels.
	uint piece_north; ///< Tiles from this tile to the northern head.
	uint piece_south; ///< Tiles from this tile to the southern head.
};

/** Everything the ghost draws on one tile. */
struct JunctionGhostTile {
	TrackBits tracks{};
	std::vector<JunctionSignalStep> signals;
	std::optional<JunctionGhostBridgeHead> head;
	std::optional<JunctionGhostBridgeSpan> span;
	bool bad = false; ///< Draw in red: the estimate found this tile unbuildable.
	bool terraform = false; ///< Draw in blue: the projection plan changes the ground of this tile.
	bool leveled = false; ///< The tile has a solved level: its sprites are drawn on the terraformed ground below, not the tile's own.
	Slope slope = SLOPE_FLAT; ///< The terraformed ground's slope (when leveled).
	int z = 0; ///< The terraformed ground's lowest corner, in pixels (when leveled).
};

/** Client-side store of the junction placement ghost. */
namespace JunctionGhost {
	void Set(const JunctionLayout &layout, TileIndex origin, int height, RailType railtype, const std::vector<std::optional<BridgeType>> &bridge_types, const JunctionEstimate &estimate, std::span<const TileIndex> terraform_tiles = {}, std::span<const JunctionTerraformStep> steps = {}, std::span<const std::pair<TileIndexDiffC, int8_t>> levels = {});
	void Clear();
	const JunctionGhostTile *Get(TileIndex tile);
	RailType GetRailType();
	void SetPinned(bool pinned);
	bool IsInPinnedFootprint(TileIndex tile);
}

struct TileInfo;
void DrawJunctionGhost(TileInfo *ti);
void DrawJunctionGhostOutline(const TileInfo *ti);

struct Window;
void ShowBuildJunctionWindow(Window *parent, RailType railtype);
void JunctionToolClick(TileIndex tile);
bool IsJunctionTrainPickActive();
void ShowBuildJunctionStationWindow(Window *parent, RailType railtype);
void JunctionStationToolClick(TileIndex tile);
bool IsJunctionStationTrainPickActive();
void RestoreJunctionPlacement();

#endif /* JUNCTION_GUI_H */
