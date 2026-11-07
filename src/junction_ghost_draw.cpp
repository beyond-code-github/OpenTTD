/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_ghost_draw.cpp Drawing of the junction placement ghost, using the game's own sprite code. */

#include "stdafx.h"

#include "junction_ghost_draw.h"
#include "bridge.h"
#include "newgrf_railtype.h"
#include "rail.h"
#include "slope_func.h"
#include "tile_cmd.h"
#include "transparency.h"
#include "viewport_func.h"

#include "safeguards.h"

static constexpr int GHOST_BRIDGE_Z_START = 3; ///< Mirrors BRIDGE_Z_START in tunnelbridge_cmd.cpp; keep in sync when rebasing.

/**
 * Draw a rail bridge head on a flat tile for the junction placement ghost, without reading the tile's map data.
 * This mirrors the bridge head branch of DrawTile_TunnelBridge() in tunnelbridge_cmd.cpp, for a flat tile, and must be kept in sync with it when rebasing.
 * It skips the foundation, ground, reservation, catenary and the bridge middle.
 * @param ti Tile to draw on (must be flat).
 * @param type Bridge type.
 * @param rt Rail type of the bridge.
 * @param dir Direction the bridge leaves the head in.
 */
void DrawBridgeHeadGhost(TileInfo *ti, BridgeType type, RailType rt, DiagDirection dir)
{
	bool is_custom_layout = false; // Set if rail/road bridge uses a custom layout.

	uint base_offset = GetBridgeRampDirectionBaseOffset(dir);
	std::span<const PalSpriteID> psid;
	if (ti->tileh == SLOPE_FLAT) base_offset += 4; // sloped bridge head
	base_offset += GetRailTypeInfo(rt)->bridge_offset;
	psid = GetBridgeSpriteTable(type, BRIDGE_PIECE_HEAD);
	is_custom_layout = BridgeHasCustomSpriteTable(type, BRIDGE_PIECE_HEAD);
	psid = psid.subspan(base_offset, 1);

	/* Draw Trambits and PBS Reservation as SpriteCombine */
	StartSpriteCombine();

	/* HACK set the height of the BB of a sloped ramp to 1 so a vehicle on
	 * it doesn't disappear behind it
	 */
	/* Bridge heads are drawn solid no matter how invisibility/transparency is set */
	AddSortableSpriteToDraw(psid[0].sprite, psid[0].pal, *ti, {{}, {TILE_SIZE, TILE_SIZE, static_cast<uint8_t>(ti->tileh == SLOPE_FLAT ? 0 : TILE_HEIGHT)}, {}});

	const RailTypeInfo *rti = GetRailTypeInfo(rt);
	if (is_custom_layout || rti->UsesOverlay()) {
		SpriteID surface = rti->UsesOverlay() ? GetCustomRailSprite(rti, INVALID_TILE, RailSpriteType::Bridge) : rti->base_sprites.bridge_deck;
		if (surface != 0) {
			if (HasBridgeFlatRamp(ti->tileh, DiagDirToAxis(dir))) {
				AddSortableSpriteToDraw(surface + ((DiagDirToAxis(dir) == Axis::X) ? RailTrackBridgeOffset::X : RailTrackBridgeOffset::Y), PAL_NONE, *ti, {{0, 0, TILE_HEIGHT}, {TILE_SIZE, TILE_SIZE, 0}, {}});
			} else {
				AddSortableSpriteToDraw(surface + RailTrackBridgeOffset::Slope + to_underlying(dir), PAL_NONE, *ti, {{}, {TILE_SIZE, TILE_SIZE, TILE_HEIGHT}, {}});
			}
		}
	}

	EndSpriteCombine();
}

/**
 * Draw the middle bits of a rail bridge for the junction placement ghost, without reading the map data.
 * This mirrors DrawBridgeMiddle() in tunnelbridge_cmd.cpp from the point after its map reads, and must be kept in sync with it when rebasing.
 * Transparency comes from the ghost palette override, so sprites are not made transparent here. It skips reservation and catenary, and always draws pillars.
 * @param ti Tile information of the tile to draw it on.
 * @param type Bridge type.
 * @param rt Rail type of the bridge.
 * @param axis Axis of the bridge.
 * @param bridge_z Pixel height of the bridge.
 * @param piece_north Distance to the northern bridge head plus one, as passed to CalcBridgePiece() by the real code.
 * @param piece_south Distance to the southern bridge head plus one, as passed to CalcBridgePiece() by the real code.
 */
void DrawBridgeMiddleGhost(const TileInfo *ti, BridgeType type, RailType rt, Axis axis, int bridge_z, uint piece_north, uint piece_south)
{
	bool is_custom_layout; // Set if rail/road bridge uses a custom layout.

	uint base_offset = GetBridgeMiddleAxisBaseOffset(axis);
	std::span<const PalSpriteID> psid;
	bool drawfarpillar;
	BridgePieces bridge_piece = CalcBridgePiece(piece_north, piece_south);
	drawfarpillar = !HasBit(GetBridgeSpec(type)->flags, 0);
	base_offset += GetRailTypeInfo(rt)->bridge_offset;
	psid = GetBridgeSpriteTable(type, bridge_piece);
	is_custom_layout = BridgeHasCustomSpriteTable(type, bridge_piece);
	psid = psid.subspan(base_offset, 3);

	int x = ti->x;
	int y = ti->y;
	int z = bridge_z - GHOST_BRIDGE_Z_START;

	/* Add a bounding box that separates the bridge from things below it. */
	AddSortableSpriteToDraw(SPR_EMPTY_BOUNDING_BOX, PAL_NONE, x, y, bridge_z - TILE_HEIGHT + BB_Z_SEPARATOR, {{}, {TILE_SIZE, TILE_SIZE, 1}, {}});

	/* Draw Trambits as SpriteCombine */
	StartSpriteCombine();

	/* Draw floor and far part of bridge*/
	if (!IsInvisibilitySet(TransparencyOption::Bridges)) {
		if (axis == Axis::X) {
			AddSortableSpriteToDraw(psid[0].sprite, psid[0].pal, x, y, z, {{0, 0, GHOST_BRIDGE_Z_START}, {TILE_SIZE, 1, 40}, {0, 0, -GHOST_BRIDGE_Z_START}}, false);
		} else {
			AddSortableSpriteToDraw(psid[0].sprite, psid[0].pal, x, y, z, {{0, 0, GHOST_BRIDGE_Z_START}, {1, TILE_SIZE, 40}, {0, 0, -GHOST_BRIDGE_Z_START}}, false);
		}
	}

	const RailTypeInfo *rti = GetRailTypeInfo(rt);
	if ((is_custom_layout || rti->UsesOverlay()) && !IsInvisibilitySet(TransparencyOption::Bridges)) {
		SpriteID surface = rti->UsesOverlay() ? GetCustomRailSprite(rti, INVALID_TILE, RailSpriteType::Bridge, TileContext::OnBridge) : rti->base_sprites.bridge_deck;
		if (surface != 0) {
			AddSortableSpriteToDraw(surface + to_underlying(axis), PAL_NONE, x, y, bridge_z, {{}, {TILE_SIZE, TILE_SIZE, 0}, {}}, false);
		}
	}

	EndSpriteCombine();

	/* draw roof, the component of the bridge which is logically between the vehicle and the camera */
	if (!IsInvisibilitySet(TransparencyOption::Bridges)) {
		if (axis == Axis::X) {
			y += 12;
			if (psid[1].sprite & SPRITE_MASK) AddSortableSpriteToDraw(psid[1].sprite, psid[1].pal, x, y, z, {{0, 3, GHOST_BRIDGE_Z_START}, {TILE_SIZE, 1, 40}, {0, -3, -GHOST_BRIDGE_Z_START}}, false);
		} else {
			x += 12;
			if (psid[1].sprite & SPRITE_MASK) AddSortableSpriteToDraw(psid[1].sprite, psid[1].pal, x, y, z, {{3, 0, GHOST_BRIDGE_Z_START}, {1, TILE_SIZE, 40}, {-3, 0, -GHOST_BRIDGE_Z_START}}, false);
		}
	}

	/* Do not draw anything more if bridges are invisible */
	if (IsInvisibilitySet(TransparencyOption::Bridges)) return;

	DrawBridgePillars(psid[2], ti, axis, drawfarpillar, x, y, z);
}
