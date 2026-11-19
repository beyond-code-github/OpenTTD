/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file planar_layout.h The planar layout program (DESIGN-planar §2a): an octilinear integer program over the lines' chains, solved through JunctionMilp. */

#ifndef PLANAR_LAYOUT_H
#define PLANAR_LAYOUT_H

#include "planar_topology.h"
#include "../../map_type.h"
#include "../../track_type.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

struct JunctionSpec;
class JunctionFacts;

/** A deck: the over line's bridge from the near head of one crossing to the far head of the same or a later crossing (a merged deck). */
struct PlanarDeck {
	uint32_t line;
	uint32_t first; ///< Chain position of its first crossing vertex.
	uint32_t last; ///< Chain position of its last crossing vertex.
	int back; ///< Units from the first crossing vertex back to the near head's outer edge.
	int front; ///< Units from the last crossing vertex to the far head's outer edge.
};

/** A solved layout: every line's chain of vertices in half-tile units (tile (x, y) covers [2x, 2x + 2] x [2y, 2y + 2]). */
struct PlanarGeometry {
	int width = 0; ///< Box width in tiles.
	int height = 0; ///< Box height in tiles.
	std::vector<std::array<int, 2>> points; ///< Per vertex, (X, Y).
	std::vector<std::vector<uint32_t>> chains; ///< Per line, its vertices in travel order.
	std::vector<std::vector<uint8_t>> headings; ///< Per line, per segment (between consecutive chain vertices), its PlanarHeading; a zero-length segment keeps the previous one.
	std::vector<bool> a_over; ///< Per crossing, whether its line a is over (D307).
	std::vector<PlanarDeck> decks;
	std::vector<uint32_t> node_vertex; ///< Per node, its vertex.
};

/** One piece of a rasterised line: a unit step (an axis step is 2 units, one piece; a diagonal step 1 unit, one half piece). */
struct PlanarPiece {
	TileIndexDiffC tile;
	TrackBits bits;
	std::array<int, 2> from; ///< The step's start point.
	std::array<int, 2> to; ///< Its end point.
	uint8_t heading;
	uint32_t segment; ///< Index of the segment in the line's chain.
	enum class Kind : uint8_t { Ground, Head, Middle } kind = Kind::Ground;
};

/** Rasterise every line of a geometry: per line, its pieces in travel order, deck pieces marked. */
std::vector<std::vector<PlanarPiece>> RasterPlanarLines(const PlanarGeometry &geometry);

/** A lazily added row family, kept so later solves of the same (topology, spec) replay it in order. */
struct PlanarLazyRow {
	enum class Kind : uint8_t { Clash, Curve, Obstacle, Prio } kind;
	uint32_t line_a = 0, segment_a = 0; ///< Clash: the first segment (line, index in its chain); Prio: line_a is the merge node of the element; Obstacle: the segment.
	uint32_t line_b = 0, segment_b = 0; ///< Clash and Prio: the other segment.
	int gap = 0; ///< Curve: the least distance.
	uint8_t rule = 0; ///< Clash and Prio: the clash rule kind of the pair.
	uint8_t heading_a = 0; ///< Clash, Prio and Obstacle: the first segment's heading when found (the rows hold while it keeps it).
	uint8_t heading_b = 0; ///< Clash and Prio: the other segment's heading when found.
	int8_t sector = -1; ///< Clash, Prio and Obstacle: a fixed sector (axis * 2 + sign), or -1 for a free one (binaries).
	TileIndexDiffC tile{}; ///< Obstacle: the tile, plan frame.
	std::vector<std::pair<uint32_t, uint32_t>> span; ///< Curve: the (line, segment) list from curve a to curve b.
	std::vector<std::pair<uint32_t, uint8_t>> curves; ///< Curve: (turn index, hand: 0 left, 1 right) of the curves from a to b.
};

/** What a solve keeps for the next one of the same (topology, spec). */
struct PlanarSolveState {
	std::vector<PlanarLazyRow> lazy;
	std::vector<double> values; ///< The solution, by variable.
	std::vector<uint32_t> binaries; ///< The binary variables (directions, over choices, sectors, merged decks).
	int solves = 0; ///< HiGHS solves the lazy loop took.
};

/** How to solve. */
struct PlanarSolveOptions {
	int bound = 64; ///< The box bound S in tiles: every big-M is 2S (lengths) or 4S (sectors).
	const JunctionFacts *facts = nullptr; ///< The terrain's facts (snapshot frame), nullptr on flat ground.
	TileIndexDiffC offset{}; ///< The snapshot tile of the plan's (0, 0).
	enum class Mode : uint8_t {
		Free, ///< Every binary free; a given state's values are only a warm start.
		Fixed, ///< The given state's binaries fixed by bounds (hover).
	} mode = Mode::Free;
	int max_rounds = 40; ///< The lazy loop's limit.
};

/**
 * Solve the layout program of a topology (DESIGN-planar §2a) with its lazy loop: clashes, curve gaps, prio element room and terrain facts are added as found and the program solved again until none is found.
 * @param base The state of an earlier solve of the same (topology, spec): its lazy rows are replayed, its values warm-start (Free) or fix (Fixed) the binaries; nullptr for none.
 * @param[out] state This solve's state.
 * @param[out] geometry The solved layout.
 * @param why Receives the reason on failure.
 * @return Whether a layout was found.
 */
bool SolvePlanarLayout(const PlanarTopology &topology, const JunctionSpec &spec, const PlanarSolveOptions &options, const PlanarSolveState *base, PlanarSolveState &state, PlanarGeometry &geometry, std::string *why);

#endif /* PLANAR_LAYOUT_H */
