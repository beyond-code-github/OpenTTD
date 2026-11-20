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

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
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
	std::array<int, 2> limit; ///< The most middle tiles each kind may span (0 bridge, 1 tunnel), -1 for a kind not allowed: bridge min(max_bridge_middle, and with a copy 2 * sd + tl - 2 (D228a)); tunnel min(max_tunnel_length, and with a copy the same D228a bound).
};

/** The lower bounds and exact values Slots reads, handed down by Structure (D366, SPEC-slots-lengths §8.1, E2); half tiles unless noted. */
struct PlanarRuleFields {
	int tl = 0; ///< The train length in tiles (spec.tl).
	int cl = 0; ///< The curve spacing in half tiles, DeriveCurveSpacing(reference_speed, curve_speed, tl).
	int curve_same = 0; ///< The least distance between consecutive curves of the same hand: min(2 tl - 1, max(2, cl)).
	int curve_opposite = 0; ///< The least distance between consecutive curves of opposite hands: 2.
	std::array<int, 2> curve_window{}; ///< The least distance between curves k = 2 and k = 3 apart: min(2 tl - 1, 2 cl) and min(2 tl - 1, 3 cl).
	int bay = 0; ///< The bay length: 2 tl + 4.
	int coverage = 0; ///< The exact prio coverage: 2 prio_tiles + 4 (D384).
	int node_leg = 0; ///< The least node leg: 2.
	int cross = 0; ///< The crossing length: 1.
	int deck_gap = 0; ///< The least gap between decks: 2.
	int sd = 0; ///< The signal spacing in tiles (spec.sd), from which Lengths derives the landing (D375).

	bool operator==(const PlanarRuleFields &) const = default;
};

/** One variant of Structure's choices with its track map. */
struct PlanarStructureVariant {
	std::string tag; ///< The variant's name.
	PlanarTrackMap map; ///< The variant's track map.
	std::vector<PlanarStructureSite> sites; ///< Every split and merge of the map with its straight leg.
	std::vector<PlanarStructure> structures; ///< The crossings grouped into structures.
	std::array<int, 4> side_gap{}; ///< Per box side (index PlanarSide: N, E, S, W), the slot distance between the last line of one port and the first line of the next port on that side: gap + 1 (ml_gap on E and W, branch_gap on N and S; D377). Slots reads it, never the spec.
	PlanarRuleFields rules{}; ///< The spec's rule fields for Slots (PlanarRuleFieldsOf); P9 checks them.
	std::vector<uint32_t> channel_of_line; ///< Per topology line, its channel (BuildPlanarChannels): Shape's lattice rule reads it (D397).
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
 * The slot distance between consecutive ports on each box side: gap + 1, ml_gap on E and W, branch_gap on N and S (D377).
 * @param spec The spec.
 * @return Per side in PlanarSide order (N, E, S, W), the slot distance.
 */
std::array<int, 4> PlanarSideGaps(const JunctionSpec &spec);

/**
 * The rule fields Slots reads instead of the spec (D366, SPEC-slots-lengths §8.1).
 * @param spec The spec.
 * @return The fields, by the formulas of PlanarRuleFields.
 */
PlanarRuleFields PlanarRuleFieldsOf(const JunctionSpec &spec);

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
 * @param spec The spec: the bridge and tunnel limits and the flow bound, which fill each structure's limit.
 * @return The structures, in run order; a run whose crossings the map lacks gives none.
 */
std::vector<PlanarStructure> PlanarBuildStructures(const PlanarTopology &topology, const std::vector<PlanarStructureRun> &runs, const PlanarTrackMap &map, const JunctionSpec &spec);

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
 * A Structure output written down (D389): load pinned decisions into one variant, through the same builders BuildPlanarStructure uses.
 * The text has one statement per line, `#` starts a comment:
 * - `tunnel T<n> <separator line>: <crosser line>, ...`: one structure of kind tunnel; its crossers in travel order along the separator.
 * - `group G<nn>: T<n>, ...`: parallel tunnels (information for Shape v2; parsed, ids checked, not stored).
 * - `copy <line> over T<n>, ...`: a copy of `<line>` over those tunnels.
 * - `straight <trunk line>: <leg line>` or `straight <trunk line>: none`: at the node whose trunk (a split's in line, a merge's out line) is that line, the straight leg, or none (a fork).
 * @param topology The topology; its crossings must be authored (`crossings_authored`).
 * @param text The decisions.
 * @param spec The spec: the tunnel and flow limits, the side gaps.
 * @param report Receives one line per decision the model cannot take ("gap: ...") and one per straight leg that differs from Structure's own rule ("differs: ...").
 * @param error Receives why the text is malformed; the result is then empty.
 * @return The variant, tagged with the topology's name.
 */
PlanarStructureVariant LoadPlanarStructureVariant(const PlanarTopology &topology, std::string_view text, const JunctionSpec &spec, std::vector<std::string> &report, std::string *error);

/**
 * The promise P1 to P9 of SPEC-structure §4 as a test oracle (D367).
 * Copy and row tracks are the map's own additions: their crossings follow by rule from their parent structure, so P3 counts only crossings between two line tracks.
 * P3 allows a crossing in two structures exactly when each of its two tracks is the separator of one of them.
 * P5 checks the bridge and tunnel length limits and, for a structure with a copy, the D228a flow bound (crossers + 2 <= 2 * sd + tl).
 * P6 also requires each copy track to be the copy of exactly one structure.
 * P8 checks the variant's side gaps equal the spec's (D377).
 * P9 the rule fields (E2).
 * P4 also allows two tunnel structures to cross (at different levels, D390 (2)). On a written-down hub (authored crossings, D394) P3's maximality is not checked and P7 accepts a choice split (legs carrying the same routes) after a merge (D391).
 * @param topology The topology the variant was made from.
 * @param spec The spec: the bridge and tunnel limits.
 * @param v The variant.
 * @return One message per breach, each starting with its promise id ("P3: ..."); empty when the promise is kept.
 */
std::vector<std::string> CheckPlanarStructurePromise(const PlanarTopology &topology, const JunctionSpec &spec, const PlanarStructureVariant &v);

#endif /* PLANAR_STRUCTURE_H */
