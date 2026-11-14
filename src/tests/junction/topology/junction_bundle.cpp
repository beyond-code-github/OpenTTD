/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_bundle.cpp Test the bundle graph's structure and programs (width 2). */

#include "../../../stdafx.h"

#include "../../../3rdparty/catch2/catch.hpp"

#include <string>
#include <vector>

#include "../../../junction/topology/junction_topology.h"

using Item = JunctionTrackStackItem;
using ItemKind = JunctionTrackStackItem::Kind;
using Bridge = JunctionTrackBridge;
using Step = JunctionTrackStep;
using Program = std::vector<Step>;
using Programs = std::vector<Program>;
static constexpr uint32_t NR = JunctionTrackStep::NO_ROW;
static Step Fan(uint32_t i) { return {Step::Kind::Fan, 0, i, 0}; }
static Step C(int h, uint32_t i) { return {Step::Kind::Corner, static_cast<int8_t>(h), i, 0}; }
static Step XS(int h) { return {Step::Kind::SBend, static_cast<int8_t>(h), NR, 0, Step::Role::Cross}; }
static Step XC(int h) { return {Step::Kind::Corner, static_cast<int8_t>(h), NR, 0, Step::Role::Cross}; }
static Step O(uint32_t x) { return {Step::Kind::Over, 0, x, 0}; }
static Step U(uint32_t x, uint8_t c) { return {Step::Kind::Under, 0, x, c}; }

/** The T's one-lane paths, and its bundle graph with every group `lanes` wide, in the numbering of the T's track graph. */
struct BundleT {
	const JunctionTopology &topology = JunctionTTopology();
	std::vector<JunctionLanePath> paths;
	JunctionTrackGraph graph;
	JunctionTrackOrder order;
	std::string error;

	explicit BundleT(uint8_t lanes)
	{
		JunctionTopology wide = JunctionTTopology();
		for (JunctionLaneGroup &group : wide.groups) group.lanes = lanes;
		graph = BuildJunctionBundleGraph(wide, &error);
		REQUIRE(!graph.edges.empty());
		paths = ExpandJunctionLanes(topology, &error);
		order = OrderJunctionTrackGraph(topology, paths, graph, &error);
		REQUIRE(!order.rows.empty());
	}
};

TEST_CASE("bundle: a bundle wider than two tracks is refused", "[junction][bundle]")
{
	BundleT t(3);
	std::string error;
	const JunctionTrackStructure structure = StructureJunctionTrackGraph(t.topology, t.paths, t.graph, t.order, &error);
	CHECK(error == "a bundle wider than two tracks is not ruled yet");
	CHECK(structure.stack.empty());
}

TEST_CASE("bundle: the T's structure two wide", "[junction][bundle]")
{
	BundleT t(2);
	const JunctionTrackStructure structure = StructureJunctionTrackGraph(t.topology, t.paths, t.graph, t.order, &t.error);
	CHECK(structure.stack == std::vector<Item>{
		{0, ItemKind::Lane, 0}, {0, ItemKind::Copy, 1}, {0, ItemKind::BlockSide, 0}, {0, ItemKind::BlockSide, 1},
		{1, ItemKind::Copy, 1}, {1, ItemKind::Lane, 0}, {1, ItemKind::BlockSide, 1}, {1, ItemKind::BlockSide, 0},
	});
	CHECK(structure.bridges == std::vector<Bridge>{{5, {8, 4}, true}, {8, {4}, false}});
}

TEST_CASE("bundle: the T's programs two wide", "[junction][bundle]")
{
	BundleT t(2);
	const JunctionTrackStructure structure = StructureJunctionTrackGraph(t.topology, t.paths, t.graph, t.order, &t.error);
	REQUIRE(!structure.stack.empty());
	const JunctionTrackPrograms programs = ProgramJunctionTrackGraph(t.topology, t.paths, t.graph, t.order, structure, &t.error);
	REQUIRE(programs.edges.size() == 12);
	REQUIRE(programs.copies.size() == 12);
	const Programs edges = {
		{Fan(0)}, {}, {Fan(5)}, {},
		{C(+1, NR), U(5, 1), U(5, 0), XS(-1), XC(-1), U(8, 0), XC(+1)},
		{O(8), O(4)},
		{C(-1, NR)},
		{C(-1, 7)},
		{O(4), U(5, 0), U(5, 1), C(+1, 2)},
		{Fan(5)}, {}, {Fan(0)},
	};
	CHECK(programs.edges == edges);
	Programs copies(12);
	copies[0] = {Fan(1)};
	copies[2] = {Fan(4)};
	copies[5] = {O(8), O(4)};
	copies[9] = {Fan(4)};
	copies[11] = {Fan(1)};
	CHECK(programs.copies == copies);
	REQUIRE(programs.ends.size() == 12);
	for (size_t e = 0; e < t.graph.edges.size(); e++) {
		if (t.graph.edges[e].from.node) CHECK(programs.ends[e].first.token == JunctionTrackEdgeEnd::NO_TOKEN);
		if (t.graph.edges[e].to.node) CHECK(programs.ends[e].second.token == JunctionTrackEdgeEnd::NO_TOKEN);
	}
	REQUIRE(programs.nodes.size() == 6);
	const uint32_t items[6] = {0, NR, 5, 5, NR, 0};
	for (size_t v = 0; v < 6; v++) CHECK(programs.nodes[v].item == items[v]);
}
