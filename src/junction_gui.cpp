/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */


/** @file junction_gui.cpp Client-side store and drawing of the junction placement ghost. */

#include "stdafx.h"

#include "junction_gui.h"
#include "junction_ghost_draw.h"
#include "map_func.h"
#include "slope_func.h"
#include "tile_cmd.h"
#include "viewport_func.h"

#include "table/sprites.h"

#include <unordered_map>

/** The ghost tiles, by tile. */
static std::unordered_map<TileIndex, JunctionGhostTile> _junction_ghost_tiles;
/** Rail type of the ghost. */
static RailType _junction_ghost_railtype = RAILTYPE_RAIL;

/** Mark every stored tile dirty, so it is redrawn. */
static void MarkJunctionGhostDirty()
{
	for (const auto &[tile, ghost] : _junction_ghost_tiles) {
		MarkTileDirtyByTile(tile);
		/* The bridge deck is one level above the ground. */
		if (ghost.span.has_value()) MarkTileDirtyByTile(tile, 1);
	}
}

/**
 * Store a junction ghost, replacing any earlier one.
 * @param layout Layout, already transformed.
 * @param origin North corner of the junction.
 * @param height Ground height of the junction in height levels.
 * @param railtype Rail type to draw.
 * @param bridge_types Chosen bridge type per layout bridge, in order; std::nullopt where none qualifies.
 * @param estimate Estimate of the junction; its bad tiles are drawn red.
 */
void JunctionGhost::Set(const JunctionLayout &layout, TileIndex origin, int height, RailType railtype, const std::vector<std::optional<BridgeType>> &bridge_types, const JunctionEstimate &estimate)
{
	JunctionGhost::Clear();
	_junction_ghost_railtype = railtype;

	for (const JunctionTrack &track : layout.tracks) {
		TileIndex tile = AddTileIndexDiffCWrap(origin, track.offset);
		if (tile == INVALID_TILE) continue;
		_junction_ghost_tiles[tile].tracks.Set(track.tracks);
	}

	for (const JunctionSignal &signal : layout.signals) {
		TileIndex tile = AddTileIndexDiffCWrap(origin, signal.offset);
		if (tile == INVALID_TILE) continue;
		_junction_ghost_tiles[tile].signals.push_back({tile, signal.facing, signal.type, signal.variant});
	}

	for (size_t i = 0; i < layout.bridges.size(); i++) {
		TileIndex start = AddTileIndexDiffCWrap(origin, layout.bridges[i].start);
		TileIndex end = AddTileIndexDiffCWrap(origin, layout.bridges[i].end);
		if (start == INVALID_TILE || end == INVALID_TILE) continue;

		std::optional<BridgeType> type = i < bridge_types.size() ? bridge_types[i] : std::nullopt;
		if (!type.has_value()) {
			_junction_ghost_tiles[start].bad = true;
			_junction_ghost_tiles[end].bad = true;
			continue;
		}

		Axis axis = TileX(start) == TileX(end) ? Axis::Y : Axis::X;
		_junction_ghost_tiles[start].head = JunctionGhostBridgeHead{*type, axis == Axis::Y ? DiagDirection::SE : DiagDirection::SW};
		_junction_ghost_tiles[end].head = JunctionGhostBridgeHead{*type, axis == Axis::Y ? DiagDirection::NW : DiagDirection::NE};

		/* Tiles strictly between the heads. */
		uint len = (axis == Axis::Y ? Delta(TileY(start), TileY(end)) : Delta(TileX(start), TileX(end))) - 1;
		for (uint d = 1; d <= len; d++) {
			TileIndex tile = axis == Axis::Y ? TileXY(TileX(start), TileY(start) + d) : TileXY(TileX(start) + d, TileY(start));
			_junction_ghost_tiles[tile].span = JunctionGhostBridgeSpan{*type, axis, static_cast<int>((height + 1) * TILE_HEIGHT), d, len + 1 - d};
		}
	}

	for (TileIndex tile : estimate.bad_tiles) {
		if (tile == INVALID_TILE) continue;
		_junction_ghost_tiles[tile].bad = true;
	}

	MarkJunctionGhostDirty();
}

/** Empty the ghost store, and mark the old tiles dirty. */
void JunctionGhost::Clear()
{
	MarkJunctionGhostDirty();
	_junction_ghost_tiles.clear();
}

/**
 * Get the ghost on a tile.
 * @param tile Tile to look up.
 * @return The ghost, or nullptr if the tile has none.
 */
const JunctionGhostTile *JunctionGhost::Get(TileIndex tile)
{
	auto it = _junction_ghost_tiles.find(tile);
	return it == _junction_ghost_tiles.end() ? nullptr : &it->second;
}

/**
 * Get the rail type of the ghost.
 * @return The rail type given to Set().
 */
RailType JunctionGhost::GetRailType()
{
	return _junction_ghost_railtype;
}

/**
 * Draw the junction ghost on one tile, if it has any.
 * @param ti Tile that is being drawn.
 */
void DrawJunctionGhost(const TileInfo *ti)
{
	const JunctionGhostTile *ghost = JunctionGhost::Get(ti->tile);
	if (ghost == nullptr) return;

	/* Ghost tiles that are not flat can't be built on; outline them in red. */
	if (ti->tileh != SLOPE_FLAT) {
		SetViewportGhostPalette(PALETTE_SEL_TILE_RED);
		DrawTileSelectionRect(ti, PALETTE_SEL_TILE_RED);
		SetViewportGhostPalette(PAL_NONE);
		return;
	}

	SetViewportGhostPalette(ghost->bad ? PALETTE_TO_STRUCT_RED : PALETTE_TO_TRANSPARENT);
	TileInfo copy = *ti;
	RailType railtype = JunctionGhost::GetRailType();
	if (ghost->tracks.Any()) DrawRailTrackGhost(&copy, ghost->tracks, railtype);
	if (ghost->head.has_value()) DrawBridgeHeadGhost(&copy, ghost->head->type, railtype, ghost->head->dir);
	if (ghost->span.has_value()) DrawBridgeMiddleGhost(ti, ghost->span->type, railtype, ghost->span->axis, ghost->span->bridge_z, ghost->span->piece_north, ghost->span->piece_south);
	for (const JunctionSignalStep &signal : ghost->signals) {
		DrawRailSignalGhost(ti->tile, railtype, signal.facing, signal.type, signal.variant);
	}
	SetViewportGhostPalette(PAL_NONE);
}

#include "safeguards.h"
