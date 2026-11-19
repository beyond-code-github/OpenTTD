/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file planar_structure.h The Structure layer's hand-off: structures, sites and the promise oracle (SPEC-structure §6, DESIGN-tracks §17.1). */

#ifndef PLANAR_STRUCTURE_H
#define PLANAR_STRUCTURE_H

#include "../junction_spec.h"
#include "planar_regions.h"
#include "planar_topology.h"

#include <cstdint>
#include <string>
#include <vector>

/** A split or merge of the track map, its straight leg (a track) and the side the other leg leaves or arrives on. */
struct PlanarStructureSite {
	uint32_t node; ///< The node, an index into the track map's expanded topology's nodes.
	bool split; ///< A split, else a merge.
	uint32_t straight; ///< The straight leg, a track of the map; UINT32_MAX when the legs tie (a fork).
	int8_t side; ///< Where the other leg leaves or arrives: +1 right of travel, -1 left, 0 at a fork.
};

/** One bridge or tunnel: a run of consecutive crossings on one line, its separator, with no node and no other crossing between them (D369). */
struct PlanarStructure {
	uint32_t separator; ///< The track that carries the structure and runs straight across its span.
	std::vector<uint32_t> crossings; ///< Indices into the map's crossings, in travel order along the separator.
	std::vector<uint32_t> crossers; ///< The tracks the separator spans, in the same order as the crossings.
	uint8_t kinds; ///< The allowed kinds as a bit set: 1 bridge, 2 tunnel.
	uint32_t copy; ///< The copy track beside the separator over the span, or UINT32_MAX.
};

/** One variant of Structure's choices with its track map. */
struct PlanarStructureVariant {
	std::string tag; ///< The variant's name.
	PlanarTrackMap map; ///< The variant's track map.
	std::vector<PlanarStructureSite> sites; ///< Every split and merge of the map with its straight leg.
	std::vector<PlanarStructure> structures; ///< The crossings grouped into structures.
	int cost; ///< The cheap score for ranking: structures plus copies (D371a).
};

/** Structure's output: the variants ranked by cost, ties by tag. */
struct PlanarStructureResult {
	std::vector<PlanarStructureVariant> variants; ///< Ranked by cost, ties by tag.
	std::string why; ///< Empty, or why no variant exists.
};

/**
 * The promise P1 to P7 of SPEC-structure §4 as a test oracle (D367).
 * Copy and row tracks are the map's own additions: their crossings follow by rule from their parent structure, so P3 counts only crossings between two line tracks.
 * P3 allows a crossing in two structures exactly when each of its two tracks is the separator of one of them.
 * P5 checks the bridge and tunnel length limits and, for a structure with a copy, the D228a flow bound (crossers + 2 <= 2 * sd + tl).
 * P6 also requires each copy track to be the copy of exactly one structure.
 * @param topology The topology the variant was made from.
 * @param spec The spec: the bridge and tunnel limits.
 * @param v The variant.
 * @return One message per breach, each starting with its promise id ("P3: ..."); empty when the promise is kept.
 */
std::vector<std::string> CheckPlanarStructurePromise(const PlanarTopology &topology, const JunctionSpec &spec, const PlanarStructureVariant &v);

#endif /* PLANAR_STRUCTURE_H */
