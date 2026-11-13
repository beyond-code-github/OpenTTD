/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_pipeline.h The graph pipeline's driver: bridge cut, seed, detours and the terraform entry point (DESIGN-G1 §13). */

#ifndef JUNCTION_PIPELINE_H
#define JUNCTION_PIPELINE_H

#include "placement/junction_placement.h"
#include "terrain/junction_facts.h"
#include "terrain/junction_terraform.h"
#include "topology/junction_topology.h"
#include "junction_route.h"

#include <string>
#include <utility>
#include <vector>

struct JunctionSpec;

/**
 * The fixed-choices placement with the target terrain's facts in the X phase (DESIGN-G1 §14.4, D216): every group of tiles (a bend, a switch tile, a bridge head, a vertical run's tile) is restricted to the columns where its tile is buildable, and a straight run on a row must be buildable on every tile it covers. Rows stay as the structure gives them.
 * @param facts The target terrain's facts.
 * @param offset The working tile of the placement's (0, 0), as the node solve's offset.
 * @param error Receives the reason when the result is empty; JunctionTrackPlacement::why then names the relations of a contradiction, a fact's tile in the error text.
 * @param heights False (D218, the hard-facts mode): only the class facts apply (blocked, water, not spannable, outside the snapshot); height steps, joins and the fits of bends and nodes are skipped.
 * @param terraform True (D218): X uses the class facts only (JunctionFacts::Buildable, forces heights = false) and then the height solve (DESIGN-G1 §15.3) chooses a level per tile on every edge and sets terraform and height_cost; on failure the placement is empty and error says why ("terraform conflict" when two tiles or a propagation want a corner at different heights).
 * @param terrain The snapshot the facts were made from; required (non-null) when terraform is true.
 * @param shared The offset-independent stages made once by MakeJunctionPlacementShared for these same inputs; null (or not ok) computes them here. The result is the same either way.
 * @param incremental_check Tests only, passed on to SolveJunctionLevels.
 */
JunctionTrackPlacement PlaceJunctionTrackGraph(const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, const JunctionTrackOrder &order, const JunctionTrackStructure &structure, const JunctionTrackPrograms &programs, const JunctionSpec &spec, const JunctionPlacementChoices &choices, const JunctionFacts &facts, TileIndexDiffC offset, std::string *error, bool heights = true, bool terraform = false, const JunctionTerrain *terrain = nullptr, const JunctionPlacementShared *shared = nullptr, std::pair<long, long> *incremental_check = nullptr);

/**
 * The series-bridge cut (D209, DESIGN-G1 §11.9a): every doubled bridge cut into consecutive bridges of at most JUNCTION_BRIDGE_MAX_CROSSERS crossings, choosing among the cuts with the fewest bridges whose placement succeeds the one with the least box width, then height, then the first tried.
 * @param[out] placement Receives the chosen cut's placement; empty (no nodes) when no cut places.
 * @param error Receives the last cut's placement error when no cut places.
 * @return The structure with the chosen cut (the input structure when none places).
 */
JunctionTrackStructure CutJunctionTrackBridges(const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, const JunctionTrackOrder &order, const JunctionTrackStructure &structure, const JunctionTrackPrograms &programs, const JunctionSpec &spec, JunctionTrackPlacement *placement, std::string *error);

/** The graph pipeline's products for one topology and spec (DESIGN-G1 §13): the seed a projection search starts from. */
struct JunctionGraphSeed {
	const JunctionTopology *topology;
	std::vector<JunctionLanePath> paths;
	JunctionTrackGraph graph;
	JunctionTrackStructure structure; ///< After the bridge cut.
	JunctionTrackPrograms programs;
	JunctionTrackPlacement placement;
	std::vector<JunctionTrackRoute> routes;
};
/** Run the graph pipeline (expand, graph, order, structure, programs, cut, render routes); empty routes and the reason in error on failure. */
JunctionGraphSeed MakeJunctionGraphSeed(const JunctionTopology &topology, const JunctionSpec &spec, std::string *error);

/** A local detour as a graph edit (DESIGN-G1 §13.5): an out-and-back S-bend on one run of one edge, around the failing tile's column, to side +1 (right of travel) or -1. Lengths are recomputed when applied; the column is clamped strictly inside the seed's run. */
struct JunctionDetourEdit {
	uint32_t edge;
	uint16_t run;
	uint8_t copy;
	int32_t column; ///< The failing tile's working x (the house is fixed in the world).
	int8_t side;
	uint16_t variant; ///< How many of the candidates that pass ApplyJunctionDetours' rules, in its scan order, are skipped (the search tries them in order until a child template builds).
	auto operator<=>(const JunctionDetourEdit &) const = default;
};
/** The routes with every edit applied to every route carrying its (edge, copy) run (D213: realised on routes, not a layout; offset maps the template frame to working, working = frame + offset); empty with the reason in why when an edit is refused (not an X run, piece not strictly inside, a shifted run of one piece, a curve gap of the edited route that does not hold, no S-bend to that side, a new tile meeting another route or a bridge head). */
std::vector<JunctionTrackRoute> ApplyJunctionDetours(const std::vector<JunctionTrackRoute> &routes, const std::vector<JunctionDetourEdit> &edits, const JunctionSpec &spec, TileIndexDiffC offset, std::string *why);

#endif /* JUNCTION_PIPELINE_H */
