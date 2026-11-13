/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_facts.h Per-tile terrain facts of a junction snapshot for the node model (D183). */

#ifndef JUNCTION_FACTS_H
#define JUNCTION_FACTS_H

#include "../../direction_type.h"
#include "junction_terrain.h"
#include "../../track_type.h"

#include <array>
#include <cstdint>
#include <vector>

/** Which per-tile fact a NextBad()/PrevBad() query looks for. */
enum class JunctionFactKind : uint8_t { Straight, Switch, Half };

/** Per-tile facts of a terrain snapshot for the node model, with O(1) queries along rows and columns (DESIGN-nodes §2, FABLE-nodes-perf §1.2). Coordinates are the snapshot's canonical ones. */
class JunctionFacts {
public:
	explicit JunctionFacts(const JunctionTerrain &terrain);
	/** Whether an axis piece along the axis (travelling +x for Axis::X, +y for Axis::Y) can sit on the tile; entry/exit receive the pixel z (TILE_HEIGHT per height level) of its track at the entry and exit edges, from the game's foundation (D194). */
	bool Straight(int x, int y, Axis axis, int &entry, int &exit) const;
	bool Switch(int x, int y) const; ///< A switch tile (two tracks): UsableAt at z_max.
	bool Half(int x, int y) const; ///< A half (corner) piece: UsableAt at z_max.
	bool Buildable(int x, int y) const; ///< The tile's class is Buildable (D218: hard facts only; heights are the level solve's).
	bool Fits(int x, int y, TrackBits bits) const; ///< Whether the game can build exactly these tracks on the tile (buildable class as Switch/Half test it today, GetRailFoundation(slope, bits) != Invalid, and a foundation only with build_on_slopes).
	int EdgeZ(int x, int y, TrackBits bits, DiagDirection edge) const; ///< The pixel z of the tile's track surface at the midpoint of the given edge with these tracks built: z_min * TILE_HEIGHT + ApplyPixelFoundationToSlope(GetRailFoundation(slope, bits), s) + GetPartialPixelZ(mx, my, s); INT_MIN when !Fits.
	/** Whether straight axis pieces cover every tile from a to b (inclusive, either order) on row y (Axis::X) or column x (Axis::Y) and connect at matching heights. O(1). */
	bool StraightRun(Axis axis, int line, int a, int b) const;
	bool Joins(Axis axis, int line, int pos) const; ///< Whether straight pieces at pos and pos + 1 on the line meet at the same height (one straight run).
	/** The first position >= from on the line where the fact fails (Straight for kind Straight on that axis, Switch, Half), or INT_MAX if none. O(1). */
	int NextBad(JunctionFactKind kind, Axis axis, int line, int from) const;
	/** The last position <= from where it fails, or INT_MIN. O(1). */
	int PrevBad(JunctionFactKind kind, Axis axis, int line, int from) const;
	int MinX() const { return this->origin_x; } ///< Working x of the snapshot's first column.
	int MaxX() const { return this->origin_x + this->width - 1; } ///< Working x of its last column.

private:
	bool Cell(Axis axis, int line, int pos, size_t &index) const;

	int origin_x; ///< Canonical x of grid cell (0,0).
	int origin_y; ///< Canonical y of grid cell (0,0).
	int width;
	int height;
	std::array<std::vector<int16_t>, 2> entry; ///< Per axis and cell, the entry pixel z of a straight piece.
	std::array<std::vector<int16_t>, 2> exit; ///< Per axis and cell, the exit pixel z of a straight piece.
	std::array<std::vector<bool>, 2> straight; ///< Per axis and cell, whether a straight piece can sit there.
	bool build_on_slopes; ///< Whether the game allows rail on a slope that needs a foundation.
	std::vector<Slope> slope; ///< Per cell, its canonical slope.
	std::vector<int8_t> z_min; ///< Per cell, its lowest corner height.
	std::vector<bool> buildable; ///< Per cell, whether its class is Buildable.
	std::vector<bool> sw; ///< Per cell, whether a switch can sit there.
	std::vector<bool> half; ///< Per cell, whether a half piece can sit there.
	std::array<std::vector<int>, 2> run_id; ///< Per axis and cell, the id of the connected straight run it belongs to.
	std::array<std::vector<int>, 6> next_bad; ///< Per kind * 2 + axis and cell, the first bad position at or after it on its line.
	std::array<std::vector<int>, 6> prev_bad; ///< Per kind * 2 + axis and cell, the last bad position at or before it on its line.
};

#endif /* JUNCTION_FACTS_H */
