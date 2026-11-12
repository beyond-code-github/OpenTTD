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

#include "safeguards.h"

JunctionHoverPlan PlanJunctionHover(const JunctionGraphSeed &seed, const JunctionSpec &spec, const JunctionTerrain &terrain, TileIndexDiffC offset)
{
	JunctionHoverPlan plan;
	const JunctionTopology &topology = *seed.topology;
	std::string error;
	const JunctionTrackOrder order = OrderJunctionTrackGraph(topology, seed.paths, seed.graph, &error);
	if (!error.empty()) {
		plan.why = error;
		return plan;
	}
	const JunctionFacts facts(terrain);
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
	return plan;
}
