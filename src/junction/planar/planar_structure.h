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
#include "planar_paths.h"
#include "planar_regions.h"
#include "planar_topology.h"

#include <cstdint>
#include <map>
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
	size_t evaluated = 0; ///< The variants evaluated (their runs and cost found), including those later skipped or not kept.
	std::map<std::string, size_t> skipped; ///< How many (flip, ties) combinations, variants or track maps were skipped, by reason text.
	bool exhaustive = false; ///< Every variant of the search space was evaluated, else the search was best-improvement over one-bit neighbours.
};

/**
 * The channel that is over at each channel crossing, one bit per crossing in crossing order (D347b: every channel crossing is a choice).
 * Bit clear is the start: the prio channel when exactly one of the two is prio, otherwise channel `a`; bit set is the other one.
 * @param channels The channel graph.
 * @param bits Bit i set: crossing i has the other channel over.
 * @return Per channel crossing, the channel that is over.
 */
std::vector<uint32_t> PlanarChannelOver(const PlanarChannels &channels, uint64_t bits);

/**
 * The structures of a built track map, one per run: kinds bridge, the separator the run's first crossing's track on the run's line, and every event of the separator from the run's first to its last crossing (line crossings and crossings with a row or another structure's copy), with the run's copy when it has one.
 * Shared by BuildPlanarStructure and the tests.
 * @param topology The topology.
 * @param runs The runs the map was built from.
 * @param map The track map built from them.
 * @return The structures, in run order; a run whose crossings the map lacks gives none.
 */
std::vector<PlanarStructure> PlanarBuildStructures(const PlanarTopology &topology, const std::vector<PlanarStructureRun> &runs, const PlanarTrackMap &map);

/**
 * Every split and merge of a built track map with its straight leg and side: a copy or row node's straight leg is the line track, a topology node's follows the resolved site.
 * @param topology The topology.
 * @param channels The (flipped) channel graph the map was built from.
 * @param sites The resolved sites the map was built from.
 * @param map The track map.
 * @return One site per node of the map's expanded topology.
 */
std::vector<PlanarStructureSite> PlanarBuildStructureSites(const PlanarTopology &topology, const PlanarChannels &channels, const std::vector<PlanarResolvedSite> &sites, const PlanarTrackMap &map);

/**
 * The ranked variant list of SPEC-structure §3 and §6.
 * A variant is the over bits (one per channel crossing, PlanarChannelOver), the ties, the triangle flips and a mask of separator movers (bit i: the i-th of PlanarMovers is a separator, D372); its runs come from PlanarCrossingSeparators and PlanarGroupStructures, its cost is runs plus runs with a copy (D371a).
 * When the bits of the four kinds (the movers counted at their largest over the tie combinations) number 16 or fewer every variant is evaluated; otherwise best-improvement over one-bit neighbours from over 0, ties 0, flips 0, movers 0 until none lowers the cost.
 * Tags are `o{o}t{t}f{f}m{m}`. A (flip, ties) whose sites do not resolve, a variant whose separators fail and a variant whose track map fails are skipped and counted by reason.
 * @param topology The topology.
 * @param spec The spec: the bridge and flow limits.
 * @param keep How many variants to hand down at most: the lowest by (cost, tag) whose track map is valid.
 * @return The variants, ranked; why is set only when none survives.
 */
PlanarStructureResult BuildPlanarStructure(const PlanarTopology &topology, const JunctionSpec &spec, size_t keep);

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
