/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_hover.cpp The hover plan: the projection pipeline on a terrain snapshot, with terraform (H1). */

#include "stdafx.h"

#include "junction_hover.h"
#include "junction_facts.h"
#include "junction_generator.h"

#include <algorithm>
#include <cstdlib>

#include "safeguards.h"

/** Plan at one offset with the order and facts made by the caller (they do not depend on the offset). */
static JunctionHoverPlan PlanJunctionHoverAt(const JunctionGraphSeed &seed, const JunctionSpec &spec, const JunctionTerrain &terrain, const JunctionTrackOrder &order, const JunctionFacts &facts, TileIndexDiffC offset)
{
	JunctionHoverPlan plan;
	const JunctionTopology &topology = *seed.topology;
	std::string error;
	const JunctionTrackPlacement placement = PlaceJunctionTrackGraph(topology, seed.paths, seed.graph, order, seed.structure, seed.programs, spec, seed.placement.choices, facts, offset, &error, true, true, &terrain);
	if (placement.nodes.empty()) {
		plan.why = error.empty() ? "no placement" : error;
		return plan;
	}
	std::vector<JunctionTrackRoute> routes;
	JunctionLayout layout = RenderJunctionTrackPlacement(topology, seed.paths, seed.graph, seed.structure, seed.programs, placement, &error, &routes);
	if (layout.tracks.empty()) {
		plan.why = error.empty() ? "empty render" : error;
		return plan;
	}
	std::string signal_why;
	SignalJunctionTrackRoutes(layout, routes, topology, seed.paths, seed.graph, seed.programs, spec, &signal_why);
	if (!signal_why.empty()) {
		plan.why = signal_why;
		return plan;
	}
	plan.layout = std::move(layout);
	plan.terraform = placement.terraform;
	plan.height_cost = placement.height_cost;
	plan.track_levels = placement.track_levels;
	return plan;
}

JunctionHoverPlan PlanJunctionHover(const JunctionGraphSeed &seed, const JunctionSpec &spec, const JunctionTerrain &terrain, TileIndexDiffC offset)
{
	std::string error;
	const JunctionTrackOrder order = OrderJunctionTrackGraph(*seed.topology, seed.paths, seed.graph, &error);
	if (!error.empty()) {
		JunctionHoverPlan plan;
		plan.why = error;
		return plan;
	}
	const JunctionFacts facts(terrain);
	return PlanJunctionHoverAt(seed, spec, terrain, order, facts, offset);
}

JunctionHoverPlan PlanJunctionHoverNear(const JunctionGraphSeed &seed, const JunctionSpec &spec, const JunctionTerrain &terrain, TileIndexDiffC offset)
{
	std::string error;
	const JunctionTrackOrder order = OrderJunctionTrackGraph(*seed.topology, seed.paths, seed.graph, &error);
	if (!error.empty()) {
		JunctionHoverPlan plan;
		plan.why = error;
		return plan;
	}
	const JunctionFacts facts(terrain);
	JunctionHoverPlan cursor = PlanJunctionHoverAt(seed, spec, terrain, order, facts, offset);
	if (cursor.layout.has_value()) return cursor;

	std::vector<TileIndexDiffC> moves;
	for (int y = -JUNCTION_HOVER_NEAR_ROWS; y <= JUNCTION_HOVER_NEAR_ROWS; y++) {
		for (int x = -JUNCTION_HOVER_NEAR_COLUMNS; x <= JUNCTION_HOVER_NEAR_COLUMNS; x++) {
			if (x != 0 || y != 0) moves.push_back({static_cast<int16_t>(x), static_cast<int16_t>(y)});
		}
	}
	std::ranges::sort(moves, [](const TileIndexDiffC &a, const TileIndexDiffC &b) {
		const int da = std::abs(a.x) + std::abs(a.y);
		const int db = std::abs(b.x) + std::abs(b.y);
		if (da != db) return da < db;
		if (std::abs(a.y) != std::abs(b.y)) return std::abs(a.y) < std::abs(b.y);
		if (a.x != b.x) return a.x < b.x;
		return a.y < b.y;
	});
	for (const TileIndexDiffC &move : moves) {
			JunctionHoverPlan plan = PlanJunctionHoverAt(seed, spec, terrain, order, facts, {static_cast<int16_t>(offset.x + move.x), static_cast<int16_t>(offset.y + move.y)});
		if (!plan.layout.has_value()) continue;
		plan.moved = move;
		return plan;
	}
	return cursor;
}
