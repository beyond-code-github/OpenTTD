/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file planar_plan.h The planar model's back end (DESIGN-planar §3, §4): flat seed, terrain and hover solves, routes, levels, signals and the checker's topology. */

#ifndef PLANAR_PLAN_H
#define PLANAR_PLAN_H

#include "planar_layout.h"
#include "planar_topology.h"
#include "../junction_layout.h"
#include "../junction_route.h"
#include "../signals/junction_signals.h"
#include "../terrain/junction_terraform.h"
#include "../terrain/junction_terrain.h"
#include "../tool/junction_hover.h"
#include "../topology/junction_topology.h"

#include <atomic>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

/** The flat seed of a (topology, spec): the program solved on flat ground with its lazy rows (DESIGN-planar §3). */
struct PlanarSeed {
	std::string text; ///< The topology as authored.
	PlanarTopology topology;
	PlanarSolveState state;
	PlanarGeometry geometry;
	std::string why; ///< Empty when solved.
	int bound = 0; ///< The box bound S it solved at.
	int64_t micros = 0; ///< The seed's solve time.
};

/** The seed of a topology text and spec, solved once and cached (per text and the spec's rule fields). */
std::shared_ptr<const PlanarSeed> PlanarSeedFor(const std::string &text, const JunctionSpec &spec);

/** The rendered routes of a geometry, one per route path (copy 0), with the signal pass's network and the level solve's node and port tiles. */
struct PlanarRender {
	std::vector<JunctionTrackRoute> routes;
	JunctionLayout layout; ///< Tracks and bridges only.
	JunctionSignalNetwork network;
	std::vector<TileIndexDiffC> node_tiles; ///< The split and merge tiles.
	std::vector<TileIndexDiffC> port_tiles; ///< The port boundary tiles.
	std::string why; ///< Empty when rendered.
};

/** Render a solved geometry: each route path to a JunctionTrackRoute through RasterizeJunctionRoute (DESIGN-planar §2a, route conversion). */
PlanarRender RenderPlanarGeometry(const PlanarTopology &topology, const PlanarGeometry &geometry);

/** A placed plan. */
struct PlanarPlan {
	std::optional<JunctionLayout> layout; ///< Signalled, in the plan's frame; nullopt on failure.
	std::vector<JunctionTrackRoute> routes;
	JunctionTerraform terraform;
	Money height_cost = 0;
	std::vector<std::pair<TileIndexDiffC, int8_t>> track_levels;
	std::string why;
};

/**
 * Place a seed: on flat ground (terrain nullptr) its geometry; on terrain the program again with terrain facts as lazy rows, its binaries warm-started from the seed, or fixed (hover); then render, signals with the prio element, and the level solve on terrain.
 * @param offset The snapshot tile of the plan's (0, 0).
 */
PlanarPlan PlanPlanarJunction(const PlanarSeed &seed, const JunctionSpec &spec, const JunctionTerrain *terrain, TileIndexDiffC offset, bool hover);

/** The hover entry of the planar model, the shape of PlanJunctionHoverNear: the seed's binaries fixed at the cursor, then the near offsets in the same order. */
JunctionHoverPlan PlanPlanarHoverNear(const PlanarSeed &seed, const JunctionSpec &spec, const JunctionTerrain &terrain, TileIndexDiffC offset, const std::atomic<bool> *cancel = nullptr);

/** The checker's topology of a planar topology: ports as lane groups in slot order, routes' paths as lane paths with their prio flags (D301). */
JunctionTopology PlanarCheckerTopology(const PlanarTopology &topology);

/** The authored topology of a window hub type: the T for one lane each way on both lines, LL² (PlanarLL2Text) for two with any pairing, the two-lane T for two with another pairing; empty for any other. */
std::string PlanarHubText(uint8_t ml_lanes, uint8_t branch_lanes, JunctionLanePairing pairing);

/** The two-lane LL² any hub (the PSG 227 pattern) in the engineer's notation; its routes are the composed hub's lane paths, prio on the fixed lane (D301). */
std::string PlanarLL2Text();

#endif /* PLANAR_PLAN_H */
