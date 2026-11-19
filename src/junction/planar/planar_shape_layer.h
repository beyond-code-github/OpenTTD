/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file planar_shape_layer.h The Shape layer's hand-off: headings, crossing angles and structure axes, and the promise oracle (SPEC-shape §6, DESIGN-tracks §17.2). */

#ifndef PLANAR_SHAPE_LAYER_H
#define PLANAR_SHAPE_LAYER_H

#include "../junction_spec.h"
#include "planar_structure.h"
#include "planar_topology.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

/** One track's shape: its heading list and the piece each crossing sits on (CLAUDE Terms: Shape). */
struct PlanarShapeTrack {
	std::vector<int> pieces; ///< One lifted heading per piece (eighths clockwise from north, not reduced mod 8); consecutive pieces are one eighth apart. The heading at a point is the heading of the piece it sits on.
	std::vector<uint32_t> event_piece; ///< Per entry of the map's events of this track, the piece that crossing sits on. Stretch k runs from piece event_piece[k-1] (0 for k = 0) to event_piece[k] (the last piece for the last stretch).
};

/** What Shape derives for one structure. */
struct PlanarShapeStructure {
	uint8_t axis; ///< The axis heading (eighths, even) its separator holds from the first to the last crossing.
	std::array<int, 2> middles; ///< The least middle tiles per kind at the chosen angles (index 0 bridge, 1 tunnel; -1 for a kind the structure does not allow).
	std::array<int, 2> end_run; ///< The straight end-run tiles per kind (bridge: the head; tunnel: portal and ramp; -1 for a kind the structure does not allow).
};

/** One variant of Shape's output: Structure's variant refined with headings. */
struct PlanarShapeVariant {
	PlanarStructureVariant structure; ///< Structure's variant, unchanged (map, sites, structures, kinds, cost).
	std::vector<PlanarShapeTrack> tracks; ///< Per track of the map, beside tracks included, its shape.
	std::vector<uint8_t> angles; ///< Per crossing of the map, its angle in eighths (1 to 3).
	std::vector<PlanarShapeStructure> structures; ///< Per structure of Structure's variant, in the same order.
	int cost; ///< Shape's cost: the turning (sum of |turn| over stretches, D340).
};

/** Shape's output: the variants ranked by Structure cost, then Shape cost (D373). */
struct PlanarShapeResult {
	std::vector<PlanarShapeVariant> variants; ///< Ranked: Structure cost, then Shape cost.
	std::string why; ///< Empty, or why no variant exists.
};

/**
 * The promise H1 to H9 of SPEC-shape §4 as a test oracle (D367).
 * The topology and spec serve H9 (P1 to P7) and H6's limits; the oracle is a test tool, not a layer.
 * @param topology The topology the variant was made from.
 * @param spec The spec: the bridge and tunnel limits and the flow bound.
 * @param v The variant.
 * @return One message per breach, each starting with its promise id ("H3: ..."); empty when the promise is kept.
 */
std::vector<std::string> CheckPlanarShapePromise(const PlanarTopology &topology, const JunctionSpec &spec, const PlanarShapeVariant &v);

#endif /* PLANAR_SHAPE_LAYER_H */
