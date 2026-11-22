/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file planar_domain.h The domain: whether a track geometry is legal in the game, independent of every solver (CARD-domain-rules, D428). */

#ifndef PLANAR_DOMAIN_H
#define PLANAR_DOMAIN_H

#include "../../map_type.h"
#include "../../track_type.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

/** One piece of a track: a step between two edge midpoints of one tile (half tiles, x east, y south). */
struct PlanarDomainPiece {
	TileIndexDiffC tile;
	Track track;
	std::array<int, 2> from;
	std::array<int, 2> to;
	int heading; ///< Eighths clockwise from north: 0 (0,-1), 1 (1,-1), 2 (1,0), ... 7 (-1,-1).
};

/** A track's pieces, or why its points are not a legal track. */
struct PlanarDomainRaster {
	std::vector<PlanarDomainPiece> pieces;
	std::string rule; ///< Empty when legal; else "grid" or "turn".
	std::string why;
};

/** A split or merge: the tracks that end at its point and those that start there. */
struct PlanarDomainNode {
	std::array<int, 2> at; ///< An edge midpoint, half tiles.
	std::vector<uint32_t> in; ///< Tracks whose last point is `at`.
	std::vector<uint32_t> out; ///< Tracks whose first point is `at`.
};

/** A bridge or tunnel on its separator: heads at two piece indices of the separator's raster, middles between. */
struct PlanarDomainStructure {
	uint32_t separator;
	uint32_t first; ///< The near head's piece index.
	uint32_t last; ///< The far head's piece index.
	bool tunnel = false;
	int limit = -1; ///< The most middles allowed, -1 for no limit.
	std::vector<uint32_t> crossers; ///< The tracks allowed (and required) under its middles.
};

struct PlanarDomainInput {
	std::vector<std::vector<std::array<int, 2>>> tracks; ///< Per track its points in travel order; consecutive equal points allowed (dropped).
	std::vector<PlanarDomainNode> nodes;
	std::vector<PlanarDomainStructure> structures;
};

/** One way the geometry breaks the game's rules. */
struct PlanarDomainBreak {
	std::string rule; ///< "grid", "turn", "connection", "node", "diamond", "head", "decks", "under", "missing crossing", "span".
	std::vector<uint32_t> tracks; ///< Ascending, each once.
	TileIndexDiffC tile;
	std::string why;
};

struct PlanarDomainVerdict {
	bool legal = true;
	std::vector<PlanarDomainBreak> breaks; ///< Every break, one per (rule, tracks, tile).
};

/** A track's pieces from its points (section 3 frame). */
PlanarDomainRaster RasterPlanarDomainTrack(const std::vector<std::array<int, 2>> &points);

/** Whether the geometry is legal in the game, and every break. */
PlanarDomainVerdict CheckPlanarDomain(const PlanarDomainInput &input);

#endif /* PLANAR_DOMAIN_H */
