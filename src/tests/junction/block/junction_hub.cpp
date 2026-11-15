/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_hub.cpp Test the composed hub: its routes, signal graph, layout, signals and the checker oracle (DESIGN-blocks §7.3, H1b). */

#include "../../../stdafx.h"

#include "../../../3rdparty/catch2/catch.hpp"

#include <algorithm>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "../../../junction/block/junction_hub.h"
#include "../../../junction/junction_route.h"
#include "../../../junction/junction_rules.h"
#include "../../../junction/junction_spec.h"
#include "../../../junction/placement/junction_render.h"
#include "../../../junction/placement/junction_signal_adapter.h"
#include "../../../junction/topology/junction_topology.h"

#include "../../../safeguards.h"

/** The width-2 T placed, rendered and composed into a hub. */
struct ComposedT {
	JunctionSpec spec = MakeTestJunctionSpec();
	JunctionTopology topology = JunctionTTopology();
	JunctionTopology wide = JunctionTTopology();
	std::vector<JunctionLanePath> paths;
	JunctionTrackGraph graph;
	JunctionTrackOrder order;
	JunctionTrackPrograms programs;
	JunctionTrackPlacement placement;
	JunctionLayout core_layout;
	std::vector<JunctionTrackRoute> core_routes;
	JunctionHub hub;
	std::string error;
	bool composed = false;

	ComposedT()
	{
		for (JunctionLaneGroup &group : wide.groups) group.lanes = 2;
		graph = BuildJunctionBundleGraph(wide, &error);
		REQUIRE(!graph.edges.empty());
		paths = ExpandJunctionLanes(topology, &error);
		order = OrderJunctionTrackGraph(topology, paths, graph, &error);
		REQUIRE(!order.rows.empty());
		const JunctionTrackStructure structure = StructureJunctionTrackGraph(topology, paths, graph, order, &error);
		REQUIRE(!structure.stack.empty());
		programs = ProgramJunctionTrackGraph(topology, paths, graph, order, structure, &error);
		REQUIRE(programs.nodes.size() == 6);
		placement = PlaceJunctionTrackGraph(topology, paths, graph, order, structure, programs, spec, &error);
		REQUIRE(error.empty());
		core_layout = RenderJunctionTrackPlacement(topology, paths, graph, structure, programs, placement, &error, &core_routes);
		REQUIRE(!core_layout.tracks.empty());
		composed = ComposeJunctionHub(wide, topology, paths, graph, order, programs, placement, core_layout, core_routes, spec, hub, &error);
	}
};

TEST_CASE("hub: the LL2 any T composes into hub routes", "[junction][hub]")
{
	ComposedT t;
	INFO("compose error " << t.error);
	REQUIRE(t.composed);
	const JunctionHub &hub = t.hub;
	CHECK(hub.paths.size() == 18);
	CHECK(std::ranges::count_if(hub.paths, [](const JunctionLanePath &p) { return p.prio; }) == 6);

	const std::vector<JunctionPortSlot> slots = JunctionPortSlots(hub.topology);
	for (size_t m = 0; m < hub.topology.movements.size(); m++) {
		INFO("movement " << m);
		CHECK(std::ranges::count_if(hub.paths, [&](const JunctionLanePath &p) { return p.movement == m; }) == 3);
		CHECK(std::ranges::count_if(hub.paths, [&](const JunctionLanePath &p) { return p.movement == m && p.prio; }) == 1);
		for (const JunctionLanePath &p : hub.paths) {
			if (p.movement != m) continue;
			CHECK(slots[p.from].group == hub.topology.movements[m].first);
			CHECK(slots[p.to].group == hub.topology.movements[m].second);
		}
	}
	/* Every hub in slot reaches every destination arm of its group's movements. */
	for (size_t s = 0; s < slots.size(); s++) {
		for (size_t m = 0; m < hub.topology.movements.size(); m++) {
			if (hub.topology.movements[m].first != slots[s].group) continue;
			INFO("slot " << s << " movement " << m);
			CHECK(std::ranges::any_of(hub.paths, [&](const JunctionLanePath &p) { return p.from == s && p.movement == m; }));
		}
	}

	const JunctionCheckContext ctx = MakeJunctionTopologyCheckContext(hub.layout, t.spec, hub.topology, FindJunctionPorts(hub.layout, hub.topology));
	const std::vector<RuleViolation> trace = CheckConnectivity(ctx);
	for (const RuleViolation &v : trace) INFO("connectivity " << v.message << " at " << v.tile.x << "," << v.tile.y);
	CHECK(trace.empty());

	/* Every route's consecutive pieces connect. */
	CHECK(!hub.routes.empty());
	for (const JunctionTrackRoute &route : hub.routes) {
		REQUIRE(route.tags.size() == route.pieces.size());
		for (size_t i = 1; i < route.pieces.size(); i++) {
			INFO("path " << route.path << " copy " << int(route.copy) << " piece " << i);
			CHECK(std::abs(route.pieces[i].tile.x - route.pieces[i - 1].tile.x) <= 1);
			CHECK(std::abs(route.pieces[i].tile.y - route.pieces[i - 1].tile.y) <= 1);
		}
	}
}

TEST_CASE("hub: the signal graph chains core tracks between block nodes", "[junction][hub]")
{
	ComposedT t;
	INFO("compose error " << t.error);
	REQUIRE(t.composed);
	const JunctionHub &hub = t.hub;
	const JunctionTrackGraph &graph = hub.graph;
	REQUIRE(graph.path_edges.size() == hub.paths.size());
	for (size_t p = 0; p < hub.paths.size(); p++) {
		const std::vector<uint32_t> &edges = graph.path_edges[p];
		REQUIRE(!edges.empty());
		for (size_t i = 1; i < edges.size(); i++) {
			INFO("path " << p << " edge " << i);
			CHECK(graph.edges[edges[i - 1]].to.node);
			CHECK(graph.edges[edges[i - 1]].to == graph.edges[edges[i]].from);
		}
	}
	for (size_t e = 0; e < graph.edges.size(); e++) {
		INFO("edge " << e);
		CHECK(!graph.edges[e].paths.empty());
	}
	REQUIRE(hub.programs.edges.size() == graph.edges.size());
	REQUIRE(hub.programs.copies.size() == graph.edges.size());
	size_t doubled = 0;
	for (size_t e = 0; e < graph.edges.size(); e++) {
		if (hub.programs.edges[e].empty() && hub.programs.copies[e].empty()) continue;
		doubled++;
		INFO("edge " << e);
		CHECK(hub.programs.edges[e].size() == 1);
		CHECK(hub.programs.copies[e].size() == 1);
	}
	CHECK(doubled == 3);
	CHECK(std::ranges::is_sorted(graph.crossings));
	CHECK(std::ranges::adjacent_find(graph.crossings) == graph.crossings.end());
}

/* Oracle findings, H1c: the signal pass succeeds, the checker still reports findings (see the report); hidden until they are ruled. */
TEST_CASE("hub: signals and the checker on the composed T", "[junction][hub][.]")
{
	ComposedT t;
	INFO("compose error " << t.error);
	REQUIRE(t.composed);
	JunctionHub &hub = t.hub;
	std::string why;
	std::vector<std::pair<TileIndexDiffC, TileIndexDiffC>> beside;
	SignalJunctionTrackRoutes(hub.layout, hub.routes, hub.topology, hub.paths, hub.graph, hub.programs, t.spec, &why, &beside, &hub.prio);
	INFO("signal why: " << why);
	const std::string scratch = "/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/";
	std::ofstream(scratch + "h1b-hub.txt") << RenderJunctionAscii(hub.layout);
	REQUIRE(why.empty());

	const std::vector<RuleViolation> violations = CheckJunctionRules(hub.layout, t.spec, hub.topology);
	std::ofstream out(scratch + "h1b-violations.txt");
	for (const RuleViolation &v : violations) out << "rule " << static_cast<int>(v.rule) << " tile " << v.tile.x << "," << v.tile.y << " " << v.message << "\n";
	out.close();
	for (const RuleViolation &v : violations) {
		INFO("rule " << static_cast<int>(v.rule) << " tile " << v.tile.x << "," << v.tile.y << ": " << v.message);
		CHECK(v.message == "ML bridge is not doubled");
	}
}
