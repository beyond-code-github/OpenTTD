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
#include <utility>
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
	std::array<int, 2> copy_middles = {-1, -1}; ///< The least middle tiles of the copy's deck per kind at its chosen angles (-1 for a kind not allowed, or no copy).
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
	std::vector<std::pair<std::string, std::string>> unshaped; ///< Variants handed down that got no shape: each is a bug (SPEC-shape H10). Tag and why.
};

/**
 * The parent chain of a beside track (H8): from the straight leg of its start node's site, at each end node the out track that is a part of its line, to the track ending where it ends.
 * @param map The track map.
 * @param sites The variant's sites.
 * @param b The Copy or Row track.
 * @param chain Receives the tracks in order.
 * @return False when there is no (unique) chain.
 */
bool ParentChain(const PlanarTrackMap &map, const std::vector<PlanarStructureSite> &sites, uint32_t b, std::vector<uint32_t> &chain);

/**
 * The promise H1 to H9 and H11 of SPEC-shape §4 as a test oracle (D367).
 * The topology and spec serve H9 (P1 to P8) and H6's limits; the oracle is a test tool, not a layer.
 * @param topology The topology the variant was made from.
 * @param spec The spec: the bridge and tunnel limits and the flow bound.
 * @param v The variant.
 * @return One message per breach, each starting with its promise id ("H3: ..."); empty when the promise is kept.
 */
std::vector<std::string> CheckPlanarShapePromise(const PlanarTopology &topology, const JunctionSpec &spec, const PlanarShapeVariant &v);

/**
 * Shape one Structure variant: the heading program of SPEC-shape §2 and §2.1 (every rule of SPEC-shape §2.1).
 * One integer program over the lifted heading at every point of every track (its ends and its events): port headings; the node rule at every site; the crossing rule with the arrival side, the separator on an axis and the crossing angle in 1..3 eighths; one axis across each structure; beside tracks equal to their parent; every face winding once; no route turning more than 3 eighths from its entry heading; per structure and kind the capacity rows (n crossers plus one tile per run of diagonal crossers at one angle, at most the structure's limit). A stretch is generated monotone from its end headings. It minimises the turning of the Line tracks (no angle preference).
 * @param v Structure's variant.
 * @param out Receives the shaped variant when the result is true: the pieces, event pieces, crossing angles, structure axes, least middles and end runs, and the turning cost.
 * @param why Receives "no shape: <status>" when the program has no optimal solution, with the tags of the infeasible rows for an infeasible one.
 * @return Whether the variant was shaped.
 */
bool ShapePlanarStructureVariant(const PlanarStructureVariant &v, PlanarShapeVariant &out, std::string &why);

/**
 * Shape every variant Structure handed down (SPEC-shape §6).
 * @param structure Structure's output.
 * @return The shaped variants ranked by (Structure cost, Shape cost, Structure tag); the variants with no shape in `unshaped`; why is set only when none shaped.
 */
PlanarShapeResult BuildPlanarShape(PlanarStructureResult structure);

#endif /* PLANAR_SHAPE_LAYER_H */
