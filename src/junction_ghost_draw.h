/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_ghost_draw.h Drawing of the junction placement ghost, using the game's own sprite code. */

#ifndef JUNCTION_GHOST_DRAW_H
#define JUNCTION_GHOST_DRAW_H

#include "bridge_type.h"
#include "direction_type.h"
#include "gfx_type.h"
#include "rail_type.h"
#include "signal_type.h"
#include "tile_type.h"
#include "track_type.h"

#include <span>

struct TileInfo;

void DrawRailTrackGhost(TileInfo *ti, TrackBits track, RailType rt);
void DrawRailSignalGhost(TileIndex tile, RailType rt, Trackdir facing, SignalType type, SignalVariant variant);
void DrawBridgeHeadGhost(TileInfo *ti, BridgeType type, RailType rt, DiagDirection dir);
void DrawBridgeMiddleGhost(const TileInfo *ti, BridgeType type, RailType rt, Axis axis, int bridge_z, uint piece_north, uint piece_south);

/* Bridge sprite helpers defined in tunnelbridge_cmd.cpp */
std::span<const PalSpriteID> GetBridgeSpriteTable(BridgeType bridge_type, BridgePieces piece);
uint8_t GetBridgeRampDirectionBaseOffset(DiagDirection diagdir);
uint8_t GetBridgeMiddleAxisBaseOffset(Axis axis);
void DrawBridgePillars(const PalSpriteID &psid, const TileInfo *ti, Axis axis, bool drawfarpillar, int x, int y, int z_bridge);
BridgePieces CalcBridgePiece(uint north, uint south);
bool BridgeHasCustomSpriteTable(BridgeType bridge_type, BridgePieces piece);

#endif /* JUNCTION_GHOST_DRAW_H */
