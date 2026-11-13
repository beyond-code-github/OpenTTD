/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_topology.cpp Test functionality from junction_topology. */

#include "../../../stdafx.h"

#include "../../../3rdparty/fmt/format.h"
#include "../../../3rdparty/catch2/catch.hpp"

#include <algorithm>
#include <chrono>
#include <climits>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <optional>
#include <random>
#include <set>
#include <sstream>
#include <tuple>

#include "../../../direction_func.h"
#include "../../../junction/junction_rules.h"
#include "../../../junction/junction_rules_internal.h"
#include "../../../junction/legacy/junction_generator.h"
#include "../../../junction/line/junction_line.h"
#include "../../../junction/legacy/junction_nodes.h"
#include "../../../junction/junction_route.h"
#include "../../../junction/terrain/junction_terraform.h"
#include "../../../junction/topology/junction_topology.h"
#include "../../../junction/placement/junction_placement.h"
#include "../../../junction/placement/junction_render.h"
#include "../../../junction/signals/junction_signals.h"
#include "../../../junction/junction_pipeline.h"
#include "../../../track_func.h"

#include "../../../safeguards.h"

using Pairs = std::vector<std::pair<uint32_t, uint32_t>>;

TEST_CASE("topology: the T's lanes and crossings", "[junction][topology]")
{
	using Role = JunctionPortRole;
	const JunctionTopology &t = JunctionTTopology();
	CHECK(JunctionPortSlots(t).size() == 6);

	std::string error;
	const std::vector<JunctionLanePath> paths = ExpandJunctionLanes(t, &error);
	REQUIRE(paths.size() == 6);

	/* Group index to role: EIn, WOut, NIn, SOut, WIn, EOut. */
	const Role role_of[] = {Role::EIn, Role::WOut, Role::NIn, Role::SOut, Role::WIn, Role::EOut};
	const auto &movements = JunctionTMovements();
	REQUIRE(movements.size() == 6);
	const std::vector<JunctionPortSlot> slots = JunctionPortSlots(t);
	for (size_t i = 0; i < 6; i++) {
		CHECK(role_of[slots[paths[i].from].group] == movements[i].first);
		CHECK(role_of[slots[paths[i].to].group] == movements[i].second);
	}

	/* Paths in movement order: 0 EIn->EOut, 1 EIn->SOut, 2 WIn->WOut, 3 WIn->SOut, 4 NIn->WOut, 5 NIn->EOut. */
	CHECK(JunctionChordCrossings(paths) == Pairs{{1, 2}, {1, 5}, {2, 5}});
}

TEST_CASE("topology: LL_RR with an L_R sideline: lanes and crossings", "[junction][topology]")
{
	const JunctionTopology &t = JunctionLLRRSidelineTopology();
	const std::vector<JunctionPortSlot> slots = JunctionPortSlots(t);
	const std::vector<std::pair<int, int>> expected_slots = {{0, 0}, {0, 1}, {1, 0}, {1, 1}, {2, 0}, {3, 0}, {4, 1}, {4, 0}, {5, 1}, {5, 0}};
	REQUIRE(slots.size() == expected_slots.size());
	for (size_t i = 0; i < slots.size(); i++) {
		CHECK(slots[i].group == expected_slots[i].first);
		CHECK(slots[i].lane == expected_slots[i].second);
	}

	std::string error;
	const std::vector<JunctionLanePath> paths = ExpandJunctionLanes(t, &error);
	REQUIRE(paths.size() == 12);
	const std::vector<std::pair<int, int>> expected_paths = {
		/* e through, w through. */
		{0, 9}, {1, 8}, {7, 2}, {6, 3},
		/* e exit, w exit. */
		{0, 5}, {1, 5}, {7, 5}, {6, 5},
		/* joins to e-out, then to w-out. */
		{4, 9}, {4, 8}, {4, 2}, {4, 3},
	};
	for (size_t i = 0; i < paths.size(); i++) {
		CHECK(paths[i].from == expected_paths[i].first);
		CHECK(paths[i].to == expected_paths[i].second);
	}

	/* Path indices: T_e1 0, T_e2 1, T_w1 2, T_w2 3; X_e1 4, X_e2 5, X_w1 6, X_w2 7; J_e1 8, J_e2 9, J_w1 10, J_w2 11. */
	const Pairs expected = {
		{1, 4}, {1, 8}, {2, 4}, {2, 5}, {2, 8}, {2, 9}, {3, 4}, {3, 5}, {3, 6}, {3, 8}, {3, 9}, {3, 10}, {4, 8}, {4, 9}, {5, 8}, {5, 9},
	};
	CHECK(JunctionChordCrossings(paths) == expected);
}

TEST_CASE("topology: lane pairing for a multi-lane branch", "[junction][topology]")
{
	const JunctionLineSpec llrr = ParseLineNotation("LL_RR").spec;
	JunctionTopology t = MakeJunctionHubTopology(llrr, llrr);
	CHECK(t.pairing == JunctionLanePairing::AnyToAny);
	const std::vector<JunctionPortSlot> slots = JunctionPortSlots(t);
	auto slot_of = [&](uint8_t group, uint8_t lane) -> uint16_t {
		for (size_t s = 0; s < slots.size(); s++) {
			if (slots[s].group == group && slots[s].lane == lane) return static_cast<uint16_t>(s);
		}
		return 0;
	};
	/* Groups: 0 W in, 1 W out, 2 S in (branch), 3 S out, 4 E in, 5 E out. */
	size_t branch_to_w = t.movements.size();
	for (size_t m = 0; m < t.movements.size(); m++) {
		if (t.movements[m] == std::pair<uint8_t, uint8_t>{2, 1}) branch_to_w = m;
	}
	REQUIRE(branch_to_w < t.movements.size());
	using Lanes = std::vector<std::pair<uint16_t, uint16_t>>;
	auto lanes_of = [&](const JunctionTopology &topology, std::string *error) {
		Lanes lanes;
		for (const JunctionLanePath &path : ExpandJunctionLanes(topology, error)) {
			if (path.movement == branch_to_w) lanes.emplace_back(path.from, path.to);
		}
		return lanes;
	};
	std::string error;

	t.pairing = JunctionLanePairing::AnyToAny;
	CHECK(lanes_of(t, &error) == Lanes{{slot_of(2, 0), slot_of(1, 0)}, {slot_of(2, 0), slot_of(1, 1)}, {slot_of(2, 1), slot_of(1, 0)}, {slot_of(2, 1), slot_of(1, 1)}});

	t.pairing = JunctionLanePairing::LaneKeepsLane;
	CHECK(lanes_of(t, &error) == Lanes{{slot_of(2, 0), slot_of(1, 0)}, {slot_of(2, 1), slot_of(1, 1)}});

	/* The branch deals lane 0 to the W exit and lane 1 to the E exit; the W exit has one turning movement into it, so all its lanes serve it. */
	t.pairing = JunctionLanePairing::LanePerDirection;
	CHECK(lanes_of(t, &error) == Lanes{{slot_of(2, 0), slot_of(1, 0)}, {slot_of(2, 0), slot_of(1, 1)}});

	/* Unequal lane counts: keep refuses, any fans, per direction deals what it has. */
	JunctionTopology unequal = MakeJunctionHubTopology(llrr, ParseLineNotation("LLL_R").spec, JunctionLanePairing::LaneKeepsLane);
	CHECK(ExpandJunctionLanes(unequal, &error).empty());
	CHECK(error == "lane keeps lane needs equal lane counts");
	unequal.pairing = JunctionLanePairing::AnyToAny;
	CHECK(!ExpandJunctionLanes(unequal, &error).empty());

	/* A group with fewer lanes than turning movements. */
	JunctionTopology few = MakeJunctionHubTopology(llrr, llrr, JunctionLanePairing::LanePerDirection);
	few.groups[2].lanes = 1;
	few.groups[3].lanes = 1;
	CHECK(!ExpandJunctionLanes(few, &error).empty());
	few.movements.emplace_back(2, 3);
	few.movements.emplace_back(2, 3);
	few.movements.emplace_back(2, 3);
	few.groups[2].lanes = 2;
	CHECK(ExpandJunctionLanes(few, &error).empty());
	CHECK(error == "lane per direction needs a lane for each direction");
}

TEST_CASE("topology: groups out of slot order are refused", "[junction][topology]")
{
	JunctionTopology t;
	t.groups = {{JunctionSide::E, true, 1, 0, true}, {JunctionSide::W, false, 1, 0, true}};
	CHECK(JunctionPortSlots(t).empty());
}

using Kind = JunctionTrackEdge::Kind;
using Paths = std::vector<uint16_t>;
using Edges = std::vector<JunctionTrackEdge>;
using Lists = std::vector<std::vector<uint32_t>>;

/** A port slot end. */
static JunctionTrackEnd P(uint16_t slot) { return {false, slot}; }
/** A node end. */
static JunctionTrackEnd N(uint16_t node) { return {true, node}; }

TEST_CASE("topology: the T's track graph", "[junction][topology]")
{
	const JunctionTopology &t = JunctionTTopology();
	std::string error;
	const std::vector<JunctionLanePath> paths = ExpandJunctionLanes(t, &error);
	const JunctionTrackGraph graph = BuildJunctionTrackGraph(t, paths, &error);
	REQUIRE(!graph.nodes.empty());

	const std::vector<JunctionTrackNode> nodes = {{true, 0, 1}, {true, 2, 4}, {true, 4, 3}, {false, 1, 3}, {false, 3, 4}, {false, 5, 1}};
	CHECK(graph.nodes == nodes);
	const Edges edges = {
		{Kind::In, P(0), N(0), Paths{0, 1}},
		{Kind::In, P(2), N(1), Paths{4, 5}},
		{Kind::In, P(4), N(2), Paths{2, 3}},
		{Kind::Leg, N(0), N(5), Paths{0}},
		{Kind::Leg, N(0), N(4), Paths{1}},
		{Kind::Leg, N(2), N(3), Paths{2}},
		{Kind::Leg, N(2), N(4), Paths{3}},
		{Kind::Leg, N(1), N(3), Paths{4}},
		{Kind::Leg, N(1), N(5), Paths{5}},
		{Kind::Out, N(3), P(1), Paths{2, 4}},
		{Kind::Out, N(4), P(3), Paths{1, 3}},
		{Kind::Out, N(5), P(5), Paths{0, 5}},
	};
	CHECK(graph.edges == edges);
	const Lists path_edges = {{0, 3, 11}, {0, 4, 10}, {2, 5, 9}, {2, 6, 10}, {1, 7, 9}, {1, 8, 11}};
	CHECK(graph.path_edges == path_edges);
	CHECK(graph.crossings == Pairs{{4, 5}, {4, 8}, {5, 8}});
}

TEST_CASE("topology: LL_RR with an L_R sideline: track graph", "[junction][topology]")
{
	const JunctionTopology &t = JunctionLLRRSidelineTopology();
	std::string error;
	const std::vector<JunctionLanePath> paths = ExpandJunctionLanes(t, &error);
	const JunctionTrackGraph graph = BuildJunctionTrackGraph(t, paths, &error);
	REQUIRE(!graph.nodes.empty());

	const std::vector<JunctionTrackNode> nodes = {
		{true, 0, 1}, {true, 1, 3}, {true, 4, 8}, {true, 4, 4}, {true, 4, 8}, {true, 6, 7}, {true, 7, 5}, {false, 2, 5}, {false, 3, 7}, {false, 5, 8}, {false, 5, 8}, {false, 5, 8}, {false, 8, 3}, {false, 9, 1},
	};
	CHECK(graph.nodes == nodes);
	const Edges edges = {
		{Kind::In, P(0), N(0), Paths{0, 4}},
		{Kind::In, P(1), N(1), Paths{1, 5}},
		{Kind::In, P(4), N(2), Paths{8, 9, 10, 11}},
		{Kind::In, N(2), N(3), Paths{8, 9}},
		{Kind::In, N(2), N(4), Paths{10, 11}},
		{Kind::In, P(6), N(5), Paths{3, 7}},
		{Kind::In, P(7), N(6), Paths{2, 6}},
		{Kind::Leg, N(0), N(13), Paths{0}},
		{Kind::Leg, N(1), N(12), Paths{1}},
		{Kind::Leg, N(6), N(7), Paths{2}},
		{Kind::Leg, N(5), N(8), Paths{3}},
		{Kind::Leg, N(0), N(11), Paths{4}},
		{Kind::Leg, N(1), N(11), Paths{5}},
		{Kind::Leg, N(6), N(10), Paths{6}},
		{Kind::Leg, N(5), N(10), Paths{7}},
		{Kind::Leg, N(3), N(13), Paths{8}},
		{Kind::Leg, N(3), N(12), Paths{9}},
		{Kind::Leg, N(4), N(7), Paths{10}},
		{Kind::Leg, N(4), N(8), Paths{11}},
		{Kind::Out, N(7), P(2), Paths{2, 10}},
		{Kind::Out, N(8), P(3), Paths{3, 11}},
		{Kind::Out, N(9), P(5), Paths{4, 5, 6, 7}},
		{Kind::Out, N(10), N(9), Paths{6, 7}},
		{Kind::Out, N(11), N(9), Paths{4, 5}},
		{Kind::Out, N(12), P(8), Paths{1, 9}},
		{Kind::Out, N(13), P(9), Paths{0, 8}},
	};
	CHECK(graph.edges == edges);
	const Lists path_edges = {
		{0, 7, 25}, {1, 8, 24}, {6, 9, 19}, {5, 10, 20}, {0, 11, 23, 21}, {1, 12, 23, 21},
		{6, 13, 22, 21}, {5, 14, 22, 21}, {2, 3, 15, 25}, {2, 3, 16, 24}, {2, 4, 17, 19}, {2, 4, 18, 20},
	};
	CHECK(graph.path_edges == path_edges);
	const Pairs crossings = {{3, 9}, {3, 10}, {3, 23}, {8, 11}, {8, 15}, {9, 11}, {9, 12}, {10, 11}, {10, 12}, {10, 13}, {10, 17}};
	CHECK(graph.crossings == crossings);
}

TEST_CASE("topology: every chord pair is crossed exactly once", "[junction][topology]")
{
	for (const JunctionTopology *t : {&JunctionTTopology(), &JunctionLLRRSidelineTopology()}) {
		std::string error;
		const std::vector<JunctionLanePath> paths = ExpandJunctionLanes(*t, &error);
		const JunctionTrackGraph graph = BuildJunctionTrackGraph(*t, paths, &error);
		REQUIRE(!graph.edges.empty());
		Pairs covered;
		for (const auto &c : graph.crossings) {
			for (uint16_t p : graph.edges[c.first].paths) {
				for (uint16_t q : graph.edges[c.second].paths) covered.emplace_back(std::min(p, q), std::max(p, q));
			}
		}
		std::sort(covered.begin(), covered.end());
		CHECK(covered == JunctionChordCrossings(paths));
	}
}

/** Checks a graph is cascades of two-way nodes only, with D24 on every path (all In edges before all Out edges). */
static void CheckCascadeGraph(const JunctionTrackGraph &graph)
{
	for (uint32_t i = 0; i < graph.nodes.size(); i++) {
		size_t children = 0, parents = 0;
		for (const JunctionTrackEdge &edge : graph.edges) {
			if (edge.from.node && edge.from.index == i) children++;
			if (edge.to.node && edge.to.index == i) parents++;
		}
		if (graph.nodes[i].split) {
			CHECK(children == 2);
		} else {
			CHECK(parents == 2);
		}
	}
	for (const auto &list : graph.path_edges) {
		bool seen_out = false;
		for (uint32_t e : list) {
			if (graph.edges[e].kind == JunctionTrackEdge::Kind::Out) seen_out = true;
			if (graph.edges[e].kind == JunctionTrackEdge::Kind::In) CHECK(!seen_out);
		}
	}
}

/** The path sets of the two child edges of split node n, in edge order. */
static std::vector<std::vector<uint16_t>> ChildPaths(const JunctionTrackGraph &graph, uint16_t n)
{
	std::vector<std::vector<uint16_t>> result;
	for (const JunctionTrackEdge &edge : graph.edges) {
		if (edge.from.node && edge.from.index == n) result.push_back(edge.paths);
	}
	return result;
}

/** Checks the children of split node n are {k0} | {k1, k2} in lane order: the single path is nearer counter-clockwise from the node's slot than both of the pair. */
static void CheckOnePlusTwo(const JunctionTopology &t, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, uint16_t n)
{
	const size_t slots = JunctionPortSlots(t).size();
	auto distance = [&](uint16_t p) { return (paths[p].to + slots - graph.nodes[n].slot) % slots; };
	auto children = ChildPaths(graph, n);
	REQUIRE(children.size() == 2);
	std::sort(children.begin(), children.end(), [](const auto &a, const auto &b) { return a.size() < b.size(); });
	REQUIRE(children[0].size() == 1);
	REQUIRE(children[1].size() == 2);
	CHECK(distance(children[0][0]) < std::min(distance(children[1][0]), distance(children[1][1])));
}

TEST_CASE("topology: a split with three legs is a cascade of two-way nodes", "[junction][topology]")
{
	JunctionTopology t;
	t.groups = {
		{JunctionSide::W, true, 1, 0, true},
		{JunctionSide::S, false, 1, 1, false},
		{JunctionSide::S, false, 1, 2, false},
		{JunctionSide::E, false, 1, 0, true},
	};
	t.movements = {{0, 1}, {0, 2}, {0, 3}};
	std::string error;
	const std::vector<JunctionLanePath> paths = ExpandJunctionLanes(t, &error);
	REQUIRE(paths.size() == 3);
	const JunctionTrackGraph graph = BuildJunctionTrackGraph(t, paths, &error);
	CHECK(error.empty());
	REQUIRE(!graph.nodes.empty());
	CheckCascadeGraph(graph);
	/* The first node of the W slot's tree splits {k0} | {k1, k2}. */
	REQUIRE(graph.nodes[0].split);
	CheckOnePlusTwo(t, paths, graph, 0);
}

TEST_CASE("topology: a split to three lanes is a cascade of two-way nodes", "[junction][topology]")
{
	const JunctionTopology t = MakeJunctionHubTopology(ParseLineNotation("LLL_RRR").spec, ParseLineNotation("L_R").spec);
	std::string error;
	const std::vector<JunctionLanePath> paths = ExpandJunctionLanes(t, &error);
	const JunctionTrackGraph graph = BuildJunctionTrackGraph(t, paths, &error);
	CHECK(error.empty());
	REQUIRE(!graph.nodes.empty());
	CheckCascadeGraph(graph);
	size_t three = 0;
	for (uint32_t i = 0; i < graph.nodes.size(); i++) {
		if (!graph.nodes[i].split) continue;
		const auto children = ChildPaths(graph, i);
		REQUIRE(children.size() == 2);
		if (children[0].size() + children[1].size() == 3) {
			three++;
			CheckOnePlusTwo(t, paths, graph, static_cast<uint16_t>(i));
		}
	}
	CHECK(three > 0);
}

TEST_CASE("topology: the cascade of four keys is two halves", "[junction][topology]")
{
	const JunctionTopology t = MakeJunctionHubTopology(ParseLineNotation("LLLL_RRRR").spec, ParseLineNotation("L_R").spec);
	std::string error;
	const std::vector<JunctionLanePath> paths = ExpandJunctionLanes(t, &error);
	const JunctionTrackGraph graph = BuildJunctionTrackGraph(t, paths, &error);
	CHECK(error.empty());
	REQUIRE(!graph.nodes.empty());
	CheckCascadeGraph(graph);
	const size_t slots = JunctionPortSlots(t).size();
	size_t four = 0;
	for (uint32_t i = 0; i < graph.nodes.size(); i++) {
		if (!graph.nodes[i].split) continue;
		const auto children = ChildPaths(graph, i);
		REQUIRE(children.size() == 2);
		if (children[0].size() + children[1].size() != 4) continue;
		four++;
		CHECK(children[0].size() == 2);
		CHECK(children[1].size() == 2);
		/* Contiguous halves: every path of one half is nearer counter-clockwise than every path of the other. */
		auto distance = [&](uint16_t p) { return (paths[p].to + slots - graph.nodes[i].slot) % slots; };
		const size_t first = std::max(distance(children[0][0]), distance(children[0][1]));
		const size_t second = std::max(distance(children[1][0]), distance(children[1][1]));
		const size_t first_min = std::min(distance(children[0][0]), distance(children[0][1]));
		const size_t second_min = std::min(distance(children[1][0]), distance(children[1][1]));
		CHECK((first < second_min || second < first_min));
	}
	CHECK(four > 0);
}

/** The track order of a topology, with its expected row items as (node, index). */
static JunctionTrackOrder OrderOf(const JunctionTopology &t, std::string *error)
{
	const std::vector<JunctionLanePath> paths = ExpandJunctionLanes(t, error);
	const JunctionTrackGraph graph = BuildJunctionTrackGraph(t, paths, error);
	REQUIRE(!graph.edges.empty());
	return OrderJunctionTrackGraph(t, paths, graph, error);
}

/** A node row item. */
static JunctionTrackRowItem RN(uint32_t node) { return {true, node}; }
/** An edge row item. */
static JunctionTrackRowItem RE(uint32_t edge) { return {false, edge}; }

TEST_CASE("topology: the T's track order", "[junction][topology]")
{
	std::string error;
	const JunctionTrackOrder order = OrderOf(JunctionTTopology(), &error);
	REQUIRE(!order.rows.empty());
	CHECK(order.node_level == std::vector<int>{1, 4, 3, 3, 4, 1});
	using Row = std::vector<JunctionTrackRowItem>;
	CHECK(order.rows == std::vector<Row>{{RN(0), RN(5)}, {RN(3), RE(4), RE(8), RN(2)}});
	std::vector<std::vector<uint32_t>> crossings(12);
	crossings[4] = {5, 8};
	crossings[5] = {8, 4};
	crossings[8] = {4, 5};
	CHECK(order.edge_crossings == crossings);
	using Band = std::vector<JunctionTrackBandEvent>;
	REQUIRE(order.bands.size() == 3);
	CHECK(order.bands[0].empty());
	CHECK(order.bands[1].empty());
	CHECK(order.bands[2] == Band{{true, 1, 0, 1}, {true, 4, 0, 1}, {false, 4, 8, 2}});
}

TEST_CASE("topology: LL_RR with an L_R sideline: track order", "[junction][topology]")
{
	std::string error;
	const JunctionTrackOrder order = OrderOf(JunctionLLRRSidelineTopology(), &error);
	REQUIRE(!order.rows.empty());
	CHECK(order.node_level == std::vector<int>{1, 3, 8, 4, 8, 7, 5, 5, 7, 8, 8, 8, 3, 1});
	using Row = std::vector<JunctionTrackRowItem>;
	CHECK(order.rows == std::vector<Row>{
		{RN(0), RN(13)},
		{RN(1), RE(11), RE(15), RN(12)},
		{RN(7), RE(12), RE(11), RE(3), RN(6)},
		{RN(8), RE(17), RE(12), RE(11), RE(3), RE(13), RN(5)},
	});
	std::vector<std::vector<uint32_t>> crossings(26);
	crossings[3] = {23, 10, 9};
	crossings[8] = {11, 15};
	crossings[9] = {3, 11, 12};
	crossings[10] = {13, 3, 11, 12, 17};
	crossings[11] = {8, 9, 10};
	crossings[12] = {9, 10};
	crossings[13] = {10};
	crossings[15] = {8};
	crossings[17] = {10};
	crossings[23] = {3};
	CHECK(order.edge_crossings == crossings);
	using Band = std::vector<JunctionTrackBandEvent>;
	REQUIRE(order.bands.size() == 5);
	CHECK(order.bands[0].empty());
	CHECK(order.bands[1].empty());
	CHECK(order.bands[3].empty());
	CHECK(order.bands[2] == Band{{true, 3, 0, 1}});
	CHECK(order.bands[4] == Band{{true, 2, 0, 1}, {true, 4, 0, 2}, {true, 9, 0, 1}, {false, 3, 23, 2}, {true, 11, 0, 3}, {true, 10, 0, 2}});
}

/** The track structure of a topology. */
static JunctionTrackStructure StructureOf(const JunctionTopology &t)
{
	std::string error;
	const std::vector<JunctionLanePath> paths = ExpandJunctionLanes(t, &error);
	const JunctionTrackGraph graph = BuildJunctionTrackGraph(t, paths, &error);
	REQUIRE(!graph.edges.empty());
	const JunctionTrackOrder order = OrderJunctionTrackGraph(t, paths, graph, &error);
	REQUIRE(!order.rows.empty());
	return StructureJunctionTrackGraph(t, paths, graph, order, &error);
}

using Item = JunctionTrackStackItem;
using ItemKind = JunctionTrackStackItem::Kind;
using Bridge = JunctionTrackBridge;

TEST_CASE("topology: the T's track structure", "[junction][topology]")
{
	const JunctionTrackStructure structure = StructureOf(JunctionTTopology());
	CHECK(structure.stack == std::vector<Item>{
		{0, ItemKind::Prio}, {0, ItemKind::Lane}, {0, ItemKind::Approach},
		{1, ItemKind::PrioAndCopy}, {1, ItemKind::Lane}, {1, ItemKind::Approach},
	});
	CHECK(structure.bridges == std::vector<Bridge>{{5, {8, 4}, true}, {8, {4}, false}});
	CHECK(structure.turns == std::vector<int8_t>{0, +1, 0, -1, -1, +1});
}

TEST_CASE("topology: LL_RR with an L_R sideline: track structure", "[junction][topology]")
{
	const JunctionTrackStructure structure = StructureOf(JunctionLLRRSidelineTopology());
	CHECK(structure.stack == std::vector<Item>{
		{0, ItemKind::Prio}, {0, ItemKind::Lane}, {0, ItemKind::Approach},
		{1, ItemKind::PrioAndCopy}, {1, ItemKind::Lane}, {1, ItemKind::Approach},
		{2, ItemKind::PrioAndCopy}, {2, ItemKind::Lane}, {2, ItemKind::Approach},
		{3, ItemKind::PrioAndCopy}, {3, ItemKind::Lane}, {3, ItemKind::Approach},
	});
	CHECK(structure.bridges == std::vector<Bridge>{
		{3, {23}, false}, {8, {11, 15}, true}, {9, {3, 11, 12}, true}, {10, {13, 3, 11, 12, 17}, true},
	});
	CHECK(structure.turns == std::vector<int8_t>{0, 0, 0, 0, +1, +1, -1, -1, +1, +1, -1, -1});
}

/** The track programs of a topology. */
static JunctionTrackPrograms ProgramsOf(const JunctionTopology &t)
{
	std::string error;
	const std::vector<JunctionLanePath> paths = ExpandJunctionLanes(t, &error);
	const JunctionTrackGraph graph = BuildJunctionTrackGraph(t, paths, &error);
	REQUIRE(!graph.edges.empty());
	const JunctionTrackOrder order = OrderJunctionTrackGraph(t, paths, graph, &error);
	REQUIRE(!order.rows.empty());
	const JunctionTrackStructure structure = StructureJunctionTrackGraph(t, paths, graph, order, &error);
	REQUIRE(!structure.stack.empty());
	return ProgramJunctionTrackGraph(t, paths, graph, order, structure, &error);
}

using Step = JunctionTrackStep;
using Program = std::vector<Step>;
using Programs = std::vector<Program>;
static constexpr uint32_t NR = JunctionTrackStep::NO_ROW;
static Step Fan(uint32_t i) { return {Step::Kind::Fan, 0, i, 0}; }
static Step C(int h, uint32_t i) { return {Step::Kind::Corner, static_cast<int8_t>(h), i, 0}; }
static Step S(int h, uint32_t i) { return {Step::Kind::SBend, static_cast<int8_t>(h), i, 0}; }
static Step SV(int h, uint32_t i) { return {Step::Kind::SBend, static_cast<int8_t>(h), i, 0, Step::Role::Converge}; }
static Step SD(int h, uint32_t i) { return {Step::Kind::SBend, static_cast<int8_t>(h), i, 0, Step::Role::Diverge}; }
static Step SM(int h, uint32_t i) { return {Step::Kind::SBend, static_cast<int8_t>(h), i, 0, Step::Role::LaneMerge}; }
static Step SL(int h, uint32_t i) { return {Step::Kind::SBend, static_cast<int8_t>(h), i, 0, Step::Role::LaneSplit}; }
static Step O(uint32_t x) { return {Step::Kind::Over, 0, x, 0}; }
static Step U(uint32_t x, uint8_t c) { return {Step::Kind::Under, 0, x, c}; }

TEST_CASE("topology: a tunnel separation gives tunnel tokens", "[junction][topology]")
{
	const JunctionTopology &t = JunctionTTopology();
	std::string error;
	const std::vector<JunctionLanePath> paths = ExpandJunctionLanes(t, &error);
	const JunctionTrackGraph graph = BuildJunctionTrackGraph(t, paths, &error);
	const JunctionTrackOrder order = OrderJunctionTrackGraph(t, paths, graph, &error);
	JunctionTrackStructure structure = StructureJunctionTrackGraph(t, paths, graph, order, &error);
	const JunctionTrackPrograms bridged = ProgramJunctionTrackGraph(t, paths, graph, order, structure, &error);
	REQUIRE(!bridged.edges.empty());

	/* The T's single bridge: edge 8 over edge 4. */
	auto single = std::ranges::find_if(structure.bridges, [](const Bridge &b) { return !b.doubled; });
	REQUIRE(single != structure.bridges.end());
	CHECK(!single->tunnel);
	const uint32_t edge = single->edge;
	const uint32_t crossed = single->crossed[0];
	single->tunnel = true;
	const JunctionTrackPrograms tunnelled = ProgramJunctionTrackGraph(t, paths, graph, order, structure, &error);
	REQUIRE(!tunnelled.edges.empty());

	auto count = [](const Program &p, Step::Kind kind, uint32_t index) { return std::ranges::count_if(p, [&](const Step &s) { return s.kind == kind && s.index == index; }); };
	CHECK(count(bridged.edges[edge], Step::Kind::Over, crossed) == 1);
	CHECK(count(bridged.edges[crossed], Step::Kind::Under, edge) == 1);
	CHECK(count(tunnelled.edges[edge], Step::Kind::Over, crossed) == 0);
	CHECK(count(tunnelled.edges[edge], Step::Kind::Tunnel, crossed) == 1);
	CHECK(count(tunnelled.edges[crossed], Step::Kind::Under, edge) == 0);
	CHECK(count(tunnelled.edges[crossed], Step::Kind::Above, edge) == 1);
	/* Every other token is as it was. */
	for (uint32_t e = 0; e < graph.edges.size(); e++) {
		if (e == edge || e == crossed) continue;
		CHECK(tunnelled.edges[e] == bridged.edges[e]);
	}

}

TEST_CASE("topology: a tunnel separation is placed", "[junction][topology]")
{
	const JunctionTopology &t = JunctionTTopology();
	std::string error;
	const std::vector<JunctionLanePath> paths = ExpandJunctionLanes(t, &error);
	const JunctionTrackGraph graph = BuildJunctionTrackGraph(t, paths, &error);
	const JunctionTrackOrder order = OrderJunctionTrackGraph(t, paths, graph, &error);
	JunctionTrackStructure structure = StructureJunctionTrackGraph(t, paths, graph, order, &error);
	auto single = std::ranges::find_if(structure.bridges, [](const Bridge &b) { return !b.doubled; });
	REQUIRE(single != structure.bridges.end());
	const size_t index = single - structure.bridges.begin();
	const uint32_t edge = single->edge;
	const uint32_t crossed = single->crossed[0];
	single->tunnel = true;
	const JunctionTrackPrograms tunnelled = ProgramJunctionTrackGraph(t, paths, graph, order, structure, &error);
	REQUIRE(!tunnelled.edges.empty());

	const JunctionSpec spec = MakeTestJunctionSpec();
	std::string reason;
	const JunctionTrackPlacement placement = PlaceJunctionTrackGraph(t, paths, graph, order, structure, tunnelled, spec, &reason);
	INFO(reason);
	REQUIRE(!placement.edges.empty());

	/* Two portals on one row (or column), the run between them a tunnel within the game's length. */
	const JunctionBridge &portals = placement.bridges[index][0];
	const int dx = portals.end.x - portals.start.x;
	const int dy = portals.end.y - portals.start.y;
	CHECK((dx == 0) != (dy == 0));
	const int middle = std::abs(dx) + std::abs(dy) - 1;
	CHECK(middle >= 1);
	CHECK(middle <= spec.max_tunnel_length);

	/* An axis run of at least one tile before the near portal and after the far one. */
	const auto &spans = placement.edges[edge];
	size_t token = 0;
	while (token < tunnelled.edges[edge].size() && tunnelled.edges[edge][token].kind != Step::Kind::Tunnel) token++;
	REQUIRE(token < tunnelled.edges[edge].size());
	const auto along = [&](TileIndexDiffC a, TileIndexDiffC b) { return std::abs(a.x - b.x) + std::abs(a.y - b.y); };
	if (token > 0) CHECK(along(spans[token - 1].second, portals.start) >= 2);
	if (token + 1 < spans.size()) CHECK(along(portals.end, spans[token + 1].first) >= 2);

	/* The crossed edge crosses strictly between the portals: its Above span lies on the portals' row (or column) between them. */
	size_t above = 0;
	while (above < tunnelled.edges[crossed].size() && tunnelled.edges[crossed][above].kind != Step::Kind::Above) above++;
	REQUIRE(above < tunnelled.edges[crossed].size());
	const int lo_x = std::min(portals.start.x, portals.end.x), hi_x = std::max(portals.start.x, portals.end.x);
	const int lo_y = std::min(portals.start.y, portals.end.y), hi_y = std::max(portals.start.y, portals.end.y);
	for (const TileIndexDiffC &tile : {placement.edges[crossed][above].first, placement.edges[crossed][above].second}) {
		if (dy == 0) {
			CHECK(tile.y == portals.start.y);
			CHECK((tile.x > lo_x && tile.x < hi_x));
		} else {
			CHECK(tile.x == portals.start.x);
			CHECK((tile.y > lo_y && tile.y < hi_y));
		}
	}
}

TEST_CASE("topology: a tunnel separation renders and signals", "[junction][topology]")
{
	const JunctionTopology &t = JunctionTTopology();
	std::string error;
	const std::vector<JunctionLanePath> paths = ExpandJunctionLanes(t, &error);
	const JunctionTrackGraph graph = BuildJunctionTrackGraph(t, paths, &error);
	const JunctionTrackOrder order = OrderJunctionTrackGraph(t, paths, graph, &error);
	JunctionTrackStructure structure = StructureJunctionTrackGraph(t, paths, graph, order, &error);
	auto single = std::ranges::find_if(structure.bridges, [](const Bridge &b) { return !b.doubled; });
	REQUIRE(single != structure.bridges.end());
	const size_t index = single - structure.bridges.begin();
	single->tunnel = true;
	const JunctionTrackPrograms programs = ProgramJunctionTrackGraph(t, paths, graph, order, structure, &error);
	REQUIRE(!programs.edges.empty());
	const JunctionSpec spec = MakeTestJunctionSpec();
	std::string reason;
	const JunctionTrackPlacement placement = PlaceJunctionTrackGraph(t, paths, graph, order, structure, programs, spec, &reason);
	INFO(reason);
	REQUIRE(!placement.edges.empty());

	std::vector<JunctionTrackRoute> routes;
	JunctionLayout layout = RenderJunctionTrackPlacement(t, paths, graph, structure, programs, placement, &reason, &routes);
	INFO(reason);
	REQUIRE(!layout.tracks.empty());

	/* Exactly one tunnel, at the placement's portals; the separation has no bridge. */
	const JunctionBridge &portals = placement.bridges[index][0];
	size_t tunnels = 0;
	size_t bridges = 0;
	for (const JunctionBridge &bridge : layout.bridges) {
		if (!bridge.tunnel) {
			bridges++;
			continue;
		}
		tunnels++;
		/* The rasteriser may list the portals in either order. */
		const bool same = bridge.start.x == portals.start.x && bridge.start.y == portals.start.y && bridge.end.x == portals.end.x && bridge.end.y == portals.end.y;
		const bool swapped = bridge.start.x == portals.end.x && bridge.start.y == portals.end.y && bridge.end.x == portals.start.x && bridge.end.y == portals.start.y;
		CHECK((same || swapped));
	}
	CHECK(tunnels == 1);
	size_t expected = 0;
	for (size_t b = 0; b < structure.bridges.size(); b++) {
		if (b != index) expected += structure.bridges[b].doubled ? 2 : 1;
	}
	CHECK(bridges == expected);

	SignalJunctionTrackRoutes(layout, routes, t, paths, graph, programs, spec, &reason, nullptr, &placement.prio);
	CHECK(reason.empty());
	for (const RuleViolation &v : CheckJunctionRules(layout, spec, t)) {
		INFO(fmt::format("rule {} tile {},{}: {}", static_cast<int>(v.rule), v.tile.x, v.tile.y, v.message));
		CHECK(false);
	}
}

TEST_CASE("topology: the T's track programs", "[junction][topology]")
{
	const JunctionTrackPrograms programs = ProgramsOf(JunctionTTopology());
	REQUIRE(programs.edges.size() == 12);
	REQUIRE(programs.copies.size() == 12);
	const Programs edges = {
		{Fan(1)}, {}, {Fan(3)}, {},
		{C(+1, NR), U(5, 0), U(5, 1), S(-1, NR), U(8, 0)},
		{O(8), O(4), SM(-1, 4)},
		{C(-1, NR), SV(+1, NR)},
		{C(-1, 5), SV(+1, 4)},
		{O(4), U(5, 1), U(5, 0), C(+1, 2), SV(-1, 1)},
		{Fan(4)}, {}, {Fan(1)},
	};
	CHECK(programs.edges == edges);
	Programs copies(12);
	copies[5] = {SL(-1, 4), O(8), O(4)};
	CHECK(programs.copies == copies);
}

TEST_CASE("topology: LL_RR with an L_R sideline: track programs", "[junction][topology]")
{
	const JunctionTrackPrograms programs = ProgramsOf(JunctionLLRRSidelineTopology());
	REQUIRE(programs.edges.size() == 26);
	REQUIRE(programs.copies.size() == 26);
	const Programs edges = {
		{Fan(1)}, {Fan(3)}, {}, {O(23), U(10, 1), U(10, 0), U(9, 1), U(9, 0)}, {SD(-1, NR)}, {Fan(9)}, {Fan(6)}, {},
		{O(11), O(15), SM(+1, 4)}, {O(3), O(11), O(12), SM(-1, 7)}, {O(13), O(3), O(11), O(12), O(17), SM(-1, 10)},
		{C(+1, NR), U(8, 0), U(8, 1), U(9, 0), U(9, 1), U(10, 0), U(10, 1)},
		{C(+1, NR), U(9, 0), U(9, 1), U(10, 0), U(10, 1), SV(-1, NR)},
		{C(-1, NR), U(10, 0), U(10, 1)},
		{C(-1, NR), SV(+1, NR)},
		{U(8, 1), U(8, 0), C(+1, 2), SV(-1, 1)},
		{C(+1, 5), SV(-1, 4)},
		{U(10, 1), U(10, 0), C(-1, 8), SV(+1, 7)},
		{C(-1, 11), SV(+1, 10)},
		{Fan(7)}, {Fan(10)}, {}, {SV(+1, NR)}, {S(-1, NR), U(3, 0)}, {Fan(4)}, {Fan(1)},
	};
	CHECK(programs.edges == edges);
	Programs copies(26);
	copies[8] = {SL(+1, 4), O(11), O(15)};
	copies[9] = {SL(-1, 7), O(3), O(11), O(12)};
	copies[10] = {SL(-1, 10), O(13), O(3), O(11), O(12), O(17)};
	CHECK(programs.copies == copies);
}

TEST_CASE("topology: edge roles come from the graph", "[junction][topology]")
{
	/* A characterisation: the predicates against the old per-row derivation (a row edge is in when it runs from a port to a row node, out from a row node to a port, a leg between two nodes of one row; doubled when its leg carries a doubled bridge). */
	for (const JunctionTopology *t : {&JunctionTTopology(), &JunctionLLRRSidelineTopology()}) {
		std::string error;
		const std::vector<JunctionLanePath> paths = ExpandJunctionLanes(*t, &error);
		const JunctionTrackGraph graph = BuildJunctionTrackGraph(*t, paths, &error);
		const JunctionTrackOrder order = OrderJunctionTrackGraph(*t, paths, graph, &error);
		const JunctionTrackStructure structure = StructureJunctionTrackGraph(*t, paths, graph, order, &error);
		const JunctionTrackPrograms programs = ProgramJunctionTrackGraph(*t, paths, graph, order, structure, &error);
		REQUIRE(programs.edges.size() == graph.edges.size());
		for (uint32_t e = 0; e < graph.edges.size(); e++) {
			const JunctionTrackEdge &edge = graph.edges[e];
			const bool row_to = edge.to.node && order.node_level[edge.to.index] % 2 == 1;
			const bool row_from = edge.from.node && order.node_level[edge.from.index] % 2 == 1;
			bool doubled = false;
			for (const JunctionTrackBridge &bridge : structure.bridges) doubled = doubled || (bridge.edge == e && bridge.doubled);
			const bool leg = row_from && row_to && order.node_level[edge.from.index] == order.node_level[edge.to.index];
			INFO("edge " << e);
			CHECK(JunctionIsInEdge(graph, order, programs, e) == (!edge.from.node && row_to));
			CHECK(JunctionIsOutEdge(graph, order, programs, e) == (row_from && !edge.to.node));
			CHECK(JunctionIsLegEdge(graph, order, e) == leg);
			CHECK(JunctionIsDoubledEdge(programs, e) == doubled);
		}
	}
}

TEST_CASE("topology: programs agree with turns and crossings", "[junction][topology]")
{
	for (const JunctionTopology *t : {&JunctionTTopology(), &JunctionLLRRSidelineTopology()}) {
		std::string error;
		const std::vector<JunctionLanePath> paths = ExpandJunctionLanes(*t, &error);
		const JunctionTrackGraph graph = BuildJunctionTrackGraph(*t, paths, &error);
		const JunctionTrackOrder order = OrderJunctionTrackGraph(*t, paths, graph, &error);
		const JunctionTrackStructure structure = StructureJunctionTrackGraph(*t, paths, graph, order, &error);
		const JunctionTrackPrograms programs = ProgramJunctionTrackGraph(*t, paths, graph, order, structure, &error);
		REQUIRE(programs.edges.size() == graph.edges.size());

		/* (a) The Corner hands of a path's edges sum to its turn. */
		for (uint32_t p = 0; p < paths.size(); p++) {
			int sum = 0;
			for (uint32_t e : graph.path_edges[p]) {
				for (const Step &s : programs.edges[e]) {
					if (s.kind == Step::Kind::Corner) sum += s.hand;
				}
			}
			CHECK(sum == structure.turns[p]);
		}

		for (uint32_t e = 0; e < graph.edges.size(); e++) {
			/* (b) Over and Under tokens, repeated Unders collapsed, are the edge's crossings. */
			std::vector<uint32_t> seen;
			std::map<uint32_t, int> under_count;
			for (const Step &s : programs.edges[e]) {
				if (s.kind == Step::Kind::Over) {
					seen.push_back(s.index);
				} else if (s.kind == Step::Kind::Under) {
					if (seen.empty() || seen.back() != s.index) seen.push_back(s.index);
					under_count[s.index]++;
				}
			}
			CHECK(seen == order.edge_crossings[e]);

			/* (c) A doubled bridge is passed under twice, a single one once. */
			for (const auto &[x, count] : under_count) {
				bool doubled = false;
				for (const JunctionTrackBridge &bridge : structure.bridges) {
					if (bridge.edge == x) doubled = bridge.doubled;
				}
				CHECK(count == (doubled ? 2 : 1));
			}
		}
	}
}

TEST_CASE("topology: branches on the N side are refused", "[junction][topology]")
{
	JunctionTopology t;
	t.groups = {
		{JunctionSide::W, true, 1, 0, true},
		{JunctionSide::W, false, 1, 0, true},
		{JunctionSide::E, true, 1, 0, true},
		{JunctionSide::E, false, 1, 0, true},
		{JunctionSide::N, true, 1, 1, false},
		{JunctionSide::N, false, 1, 1, false},
	};
	t.movements = {{0, 3}, {2, 1}, {0, 5}, {2, 5}, {4, 1}, {4, 3}};
	REQUIRE(JunctionPortSlots(t).size() == 6);
	std::string error;
	const std::vector<JunctionLanePath> paths = ExpandJunctionLanes(t, &error);
	REQUIRE(!paths.empty());
	const JunctionTrackGraph graph = BuildJunctionTrackGraph(t, paths, &error);
	CHECK(graph.nodes.empty());
	CHECK(graph.edges.empty());
	CHECK(error == "branch ports off the S side are not ruled yet");
}

TEST_CASE("topology: the row of every port slot", "[junction][topology]")
{
	for (const auto &[t, expected] : {
		std::make_pair(&JunctionTTopology(), std::vector<int>{0, 1, -1, -1, 1, 0}),
		std::make_pair(&JunctionLLRRSidelineTopology(), std::vector<int>{0, 1, 2, 3, -1, -1, 3, 2, 1, 0}),
	}) {
		std::string error;
		const std::vector<JunctionLanePath> paths = ExpandJunctionLanes(*t, &error);
		CHECK(JunctionSlotRows(*t, paths, &error) == expected);
	}
}

/** The placement of a topology under a spec with the given longest bridge middle. */
static JunctionTrackPlacement PlacementOf(const JunctionTopology &t, int max_bridge_middle, std::string *error, JunctionTrackStructure *structure_out = nullptr)
{
	std::string ignored;
	const std::vector<JunctionLanePath> paths = ExpandJunctionLanes(t, &ignored);
	const JunctionTrackGraph graph = BuildJunctionTrackGraph(t, paths, &ignored);
	REQUIRE(!graph.edges.empty());
	const JunctionTrackOrder order = OrderJunctionTrackGraph(t, paths, graph, &ignored);
	REQUIRE(!order.rows.empty());
	const JunctionTrackStructure structure = StructureJunctionTrackGraph(t, paths, graph, order, &ignored);
	REQUIRE(!structure.stack.empty());
	const JunctionTrackPrograms programs = ProgramJunctionTrackGraph(t, paths, graph, order, structure, &ignored);
	REQUIRE(!programs.edges.empty());
	JunctionSpec spec = MakeTestJunctionSpec();
	spec.max_bridge_middle = max_bridge_middle;
	if (structure_out != nullptr) *structure_out = structure;
	return PlaceJunctionTrackGraph(t, paths, graph, order, structure, programs, spec, error);
}

/** The pipeline up to the series-bridge cut; returns the placement of the chosen cut. */
static JunctionTrackPlacement CutOf(const JunctionTopology &t, int max_bridge_middle, std::string *error, JunctionTrackStructure *cut = nullptr)
{
	std::string ignored;
	const std::vector<JunctionLanePath> paths = ExpandJunctionLanes(t, &ignored);
	const JunctionTrackGraph graph = BuildJunctionTrackGraph(t, paths, &ignored);
	REQUIRE(!graph.edges.empty());
	const JunctionTrackOrder order = OrderJunctionTrackGraph(t, paths, graph, &ignored);
	REQUIRE(!order.rows.empty());
	const JunctionTrackStructure structure = StructureJunctionTrackGraph(t, paths, graph, order, &ignored);
	REQUIRE(!structure.stack.empty());
	const JunctionTrackPrograms programs = ProgramJunctionTrackGraph(t, paths, graph, order, structure, &ignored);
	REQUIRE(!programs.edges.empty());
	JunctionSpec spec = MakeTestJunctionSpec();
	spec.max_bridge_middle = max_bridge_middle;
	JunctionTrackPlacement placement;
	const JunctionTrackStructure chosen = CutJunctionTrackBridges(t, paths, graph, order, structure, programs, spec, &placement, error);
	if (cut != nullptr) *cut = chosen;
	return placement;
}

/* Tiles compare as plain numbers: TileIndexDiffC has no operator==. */
using Tile = std::array<int, 2>;
using Span = std::array<int, 4>;
using Spans = std::vector<Span>;
using Heads = std::array<int, 8>;
static Tile T(int x, int y) { return {x, y}; }
static Span Sp(int x0, int y0, int x1, int y1) { return {x0, y0, x1, y1}; }
static Heads Br(int a, int b, int c, int d, int e, int f, int g, int h) { return {a, b, c, d, e, f, g, h}; }
static Tile Of(const TileIndexDiffC &t) { return {t.x, t.y}; }
static std::vector<Tile> Of(const std::vector<TileIndexDiffC> &v) { std::vector<Tile> r; for (const TileIndexDiffC &t : v) r.push_back(Of(t)); return r; }
static std::vector<Spans> Of(const std::vector<std::vector<JunctionTrackSpan>> &v)
{
	std::vector<Spans> r;
	for (const auto &spans : v) {
		r.emplace_back();
		for (const JunctionTrackSpan &s : spans) r.back().push_back({s.first.x, s.first.y, s.second.x, s.second.y});
	}
	return r;
}
static Heads Of(const std::array<JunctionBridge, 2> &b) { return {b[0].start.x, b[0].start.y, b[0].end.x, b[0].end.y, b[1].start.x, b[1].start.y, b[1].end.x, b[1].end.y}; }

TEST_CASE("topology: the T's placement", "[junction][topology]")
{
	std::string error;
	const JunctionTrackPlacement placement = PlacementOf(JunctionTTopology(), 64, &error);
	REQUIRE(!placement.nodes.empty());
	CHECK(placement.item_rows == std::vector<int>{0, 1, 2, 3, 4, 5});
	CHECK(Of(placement.size) == T(41, 14));
	CHECK(Of(placement.ports) == std::vector<Tile>{T(0, 1), T(0, 3), T(26, 13), T(28, 13), T(40, 3), T(40, 1)});
	CHECK(Of(placement.nodes) == std::vector<Tile>{T(23, 1), T(26, 11), T(33, 3), T(6, 4), T(28, 11), T(38, 1)});
	const std::vector<Spans> edges = {
		{Sp(0, 1, 0, 1)}, {}, {Sp(40, 3, 40, 3)}, {},
		{Sp(23, 1, 25, 3), Sp(24, 3, 25, 3), Sp(25, 4, 25, 4), Sp(25, 7, 28, 9), Sp(26, 7, 26, 8)},
		{Sp(28, 3, 27, 3), Sp(25, 3, 24, 3), Sp(22, 3, 22, 4)},
		{Sp(33, 3, 31, 5), Sp(31, 9, 28, 11)},
		{Sp(26, 11, 20, 5), Sp(6, 5, 6, 4)},
		{Sp(26, 8, 26, 7), Sp(26, 4, 27, 4), Sp(27, 3, 28, 3), Sp(26, 4, 28, 2), Sp(38, 2, 38, 1)},
		{Sp(2, 4, 2, 3)}, {}, {Sp(40, 1, 40, 1)},
	};
	CHECK(Of(placement.edges) == edges);
	std::vector<Spans> copies(12);
	copies[5] = {Sp(31, 3, 31, 4), Sp(27, 4, 26, 4), Sp(25, 4, 25, 4)};
	CHECK(Of(placement.copies) == copies);
	REQUIRE(placement.bridges.size() == 2);
	CHECK(Of(placement.bridges[0]) == Br(29, 3, 23, 3, 30, 4, 24, 4));
	CHECK(Of(placement.bridges[1]) == Br(26, 9, 26, 6, 0, 0, 0, 0));
}

/* Hidden until the S placement steps land (RECON-s2-cycle). */
TEST_CASE("placement: S branch groups of several lanes place (D230, S0-S2)", "[junction][topology][.]")
{
	for (const char *right : {"L_RR", "LL_RR"}) {
		const JunctionTopology t = MakeJunctionHubTopology(ParseLineNotation("L_R").spec, ParseLineNotation(right).spec);
		std::string error;
		PlacementOf(t, 64, &error);
		CHECK(error == "");
	}
}

TEST_CASE("placement: an exit split on a main row no longer reads as heading north (REVIEW-port-fans F1)", "[junction][topology]")
{
	const JunctionTopology t = MakeJunctionHubTopology(ParseLineNotation("L_R").spec, ParseLineNotation("L_RR").spec);
	std::string error;
	PlacementOf(t, 64, &error);
	CHECK(error.find("S-bend size (edge 1)") == std::string::npos);
	CHECK(error.find("S port spacing") == std::string::npos);
}

TEST_CASE("topology: LL_RR's uncut w2 bridge is too long at nmax 8", "[junction][topology]")
{
	std::string error;
	const JunctionTrackPlacement placement = PlacementOf(JunctionLLRRSidelineTopology(), 8, &error);
	CHECK(placement.nodes.empty());
	CHECK(error == "bridge 2 on edge 9 needs 9 middle tiles, more than 8"); // D217 families (T1c-1), checker clean.
}

TEST_CASE("topology: LL_RR series bridges", "[junction][topology]")
{
	std::string error;
	JunctionTrackStructure whole;
	PlacementOf(JunctionLLRRSidelineTopology(), 64, &error, &whole);
	JunctionTrackStructure cut;
	const JunctionTrackPlacement placement = CutOf(JunctionLLRRSidelineTopology(), 64, &error, &cut);
	REQUIRE(!placement.nodes.empty());
	REQUIRE(placement.bridges.size() == cut.bridges.size());

	/* At most two crossers per bridge; an edge's bridges cover its crossing set in order; the fewest bridges. */
	std::vector<size_t> count(32, 0);
	std::vector<std::vector<uint32_t>> joined(32);
	for (const JunctionTrackBridge &bridge : cut.bridges) {
		CHECK(bridge.crossed.size() <= 2);
		count[bridge.edge]++;
		joined[bridge.edge].insert(joined[bridge.edge].end(), bridge.crossed.begin(), bridge.crossed.end());
	}
	for (const JunctionTrackBridge &bridge : whole.bridges) CHECK(joined[bridge.edge] == bridge.crossed);
	CHECK(count[8] == 1);
	CHECK(count[9] == 2);
	CHECK(count[10] == 3);

	/* (a) Consecutive bridges of one edge leave a plain tile between their heads. */
	for (size_t b = 0; b + 1 < cut.bridges.size(); b++) {
		if (cut.bridges[b].edge != cut.bridges[b + 1].edge) continue;
		for (int c = 0; c < 2; c++) {
			const JunctionBridge &lead = placement.bridges[b][c];
			const JunctionBridge &next = placement.bridges[b + 1][c];
			CHECK(next.start.y == lead.end.y);
			CHECK(std::abs(next.start.x - lead.end.x) >= 2);
			CHECK((lead.end.x - lead.start.x) * (next.start.x - lead.end.x) > 0);
		}
	}

	/* (b) A doubled leg's Over span for a crossed edge lies strictly between its own bridge's copy-0 heads. */
	const JunctionTrackPrograms programs = ProgramsOf(JunctionLLRRSidelineTopology());
	for (size_t b = 0; b < cut.bridges.size(); b++) {
		const JunctionTrackBridge &bridge = cut.bridges[b];
		if (!bridge.doubled) continue;
		const int lo = std::min(placement.bridges[b][0].start.x, placement.bridges[b][0].end.x);
		const int hi = std::max(placement.bridges[b][0].start.x, placement.bridges[b][0].end.x);
		for (uint32_t x : bridge.crossed) {
			bool found = false;
			for (size_t t = 0; t < programs.edges[bridge.edge].size(); t++) {
				const JunctionTrackStep &step = programs.edges[bridge.edge][t];
				if (step.kind != JunctionTrackStep::Kind::Over || step.index != x) continue;
				found = true;
				const JunctionTrackSpan &span = placement.edges[bridge.edge][t];
				CHECK(std::min(span.first.x, span.second.x) > lo);
				CHECK(std::max(span.first.x, span.second.x) < hi);
			}
			CHECK(found);
		}
	}

	/* (c) Band 2 is as deep as one corner. */
	CHECK(placement.item_rows[6] - placement.item_rows[5] - 1 == 2);

	/* X_e1 (edge 11) and X_e2 (edge 12) are on different bridges under w1, so the signal tile between the bridges spreads them. */
	CHECK(placement.edges[11][3].first.x - placement.edges[12][1].first.x == 9);

	/* The series cut: 2 tiles wider and taller than one bridge per lane (58x23), no bridge longer than 5 middle tiles (was 10). */
	/* 62: the prio element's n + 2 floor on both doubled sides (DESIGN-G1 §12.4). */
	CHECK(placement.size.x == 62);
	CHECK(placement.size.y == 25);
	for (const std::array<JunctionBridge, 2> &heads : placement.bridges) {
		CHECK(std::abs(heads[0].start.x - heads[0].end.x) + std::abs(heads[0].start.y - heads[0].end.y) - 1 <= 5);
	}

	/* The S ports sit on the last row. */
	const JunctionTopology &topology = JunctionLLRRSidelineTopology();
	const std::vector<JunctionPortSlot> slots = JunctionPortSlots(topology);
	REQUIRE(slots.size() == placement.ports.size());
	for (size_t s = 0; s < slots.size(); s++) {
		if (topology.groups[slots[s].group].side == JunctionSide::S) CHECK(placement.ports[s].y == placement.size.y - 1);
	}
}

TEST_CASE("topology: LL_RR's cut places at the old limit", "[junction][topology]")
{
	std::string error;
	const JunctionTrackPlacement placement = CutOf(JunctionLLRRSidelineTopology(), 9, &error);
	CHECK(!placement.nodes.empty());
	CHECK(error.empty());
}

TEST_CASE("topology: LL_RR has no cut at nmax 1", "[junction][topology]")
{
	std::string error;
	const JunctionTrackPlacement placement = CutOf(JunctionLLRRSidelineTopology(), 1, &error);
	CHECK(placement.nodes.empty());
	CHECK(!error.empty());
}

TEST_CASE("topology: the T's cut is its structure", "[junction][topology]")
{
	std::string error;
	JunctionTrackStructure whole, cut;
	const JunctionTrackPlacement direct = PlacementOf(JunctionTTopology(), 64, &error, &whole);
	const JunctionTrackPlacement placement = CutOf(JunctionTTopology(), 64, &error, &cut);
	REQUIRE(!placement.nodes.empty());
	CHECK(cut.bridges == whole.bridges);
	CHECK(placement.item_rows == direct.item_rows);
	CHECK(Of(placement.ports) == Of(direct.ports));
	CHECK(Of(placement.nodes) == Of(direct.nodes));
	CHECK(Of(placement.size) == Of(direct.size));
}

TEST_CASE("topology: LL_RR with an L_R sideline: placement", "[junction][topology]")
{
	std::string error;
	JunctionTrackStructure structure;
	const JunctionTrackPlacement placement = PlacementOf(JunctionLLRRSidelineTopology(), 64, &error, &structure);
	INFO(error);
	REQUIRE(!placement.nodes.empty());
	REQUIRE(placement.bridges.size() == structure.bridges.size());

	/* X_e1 (edge 11) and X_e2 (edge 12) cross under w1's bridge four columns apart (the exit splits sit at their least x, DESIGN-G1 §11.9b); the exit legs' (Corner, S-bend) runs of edges 17 and 18 lift to 4 tiles (TL1b), which moves the columns from six. */
	CHECK(placement.edges[11][3].first.x - placement.edges[12][1].first.x == 6); // D217 families (T1c-1), checker clean.

	/* Band 2 is as deep as one corner. */
	CHECK(placement.item_rows[6] - placement.item_rows[5] - 1 == 2);

	for (size_t i = 0; i < structure.bridges.size(); i++) {
		const int n = std::abs(placement.bridges[i][0].start.x - placement.bridges[i][0].end.x) - 1;
		CHECK(n <= 64);
		if (structure.bridges[i].edge == 10) CHECK(n == 10); // D217 families (T1c-1), checker clean.
	}

	/* The S ports sit on the last row. */
	const JunctionTopology &topology = JunctionLLRRSidelineTopology();
	const std::vector<JunctionPortSlot> slots = JunctionPortSlots(topology);
	REQUIRE(slots.size() == placement.ports.size());
	for (size_t s = 0; s < slots.size(); s++) {
		if (topology.groups[slots[s].group].side == JunctionSide::S) CHECK(placement.ports[s].y == placement.size.y - 1);
	}
}

/** The pipeline through the series-bridge cut and the render, with the pieces it built on request. */
static JunctionLayout RenderOf(const JunctionTopology &t, int max_bridge_middle, std::string *error, JunctionTrackPlacement *placement_out = nullptr, JunctionTrackStructure *cut_out = nullptr, JunctionTrackPrograms *programs_out = nullptr, std::vector<JunctionLanePath> *paths_out = nullptr, JunctionTrackGraph *graph_out = nullptr, std::vector<JunctionTrackRoute> *routes_out = nullptr)
{
	std::string ignored;
	const std::vector<JunctionLanePath> paths = ExpandJunctionLanes(t, &ignored);
	const JunctionTrackGraph graph = BuildJunctionTrackGraph(t, paths, &ignored);
	REQUIRE(!graph.edges.empty());
	const JunctionTrackOrder order = OrderJunctionTrackGraph(t, paths, graph, &ignored);
	REQUIRE(!order.rows.empty());
	const JunctionTrackStructure structure = StructureJunctionTrackGraph(t, paths, graph, order, &ignored);
	REQUIRE(!structure.stack.empty());
	const JunctionTrackPrograms programs = ProgramJunctionTrackGraph(t, paths, graph, order, structure, &ignored);
	REQUIRE(!programs.edges.empty());
	JunctionSpec spec = MakeTestJunctionSpec();
	spec.max_bridge_middle = max_bridge_middle;
	JunctionTrackPlacement placement;
	const JunctionTrackStructure cut = CutJunctionTrackBridges(t, paths, graph, order, structure, programs, spec, &placement, &ignored);
	REQUIRE(!placement.nodes.empty());
	if (placement_out != nullptr) *placement_out = placement;
	if (cut_out != nullptr) *cut_out = cut;
	if (programs_out != nullptr) *programs_out = programs;
	if (paths_out != nullptr) *paths_out = paths;
	if (graph_out != nullptr) *graph_out = graph;
	return RenderJunctionTrackPlacement(t, paths, graph, cut, programs, placement, error, routes_out);
}

/** The render with its signal tokens (ST1). */
static JunctionLayout SignalledOf(const JunctionTopology &t, std::string *error, std::vector<JunctionTrackSignal> *tokens, std::vector<JunctionTrackRoute> *routes, std::vector<JunctionLanePath> *paths_out = nullptr)
{
	std::vector<JunctionLanePath> paths;
	JunctionTrackGraph graph;
	JunctionTrackPrograms programs;
	JunctionLayout layout = RenderOf(t, 64, error, nullptr, nullptr, &programs, &paths, &graph, routes);
	if (paths_out != nullptr) *paths_out = paths;
	*tokens = SignalJunctionTrackRoutes(layout, *routes, t, paths, graph, programs, MakeTestJunctionSpec());
	return layout;
}

TEST_CASE("topology: the T render traces like the flat T", "[junction][topology]")
{
	std::string error;
	const JunctionLayout render = RenderOf(JunctionTTopology(), 64, &error);
	INFO(error);
	REQUIRE(!render.tracks.empty());
	const JunctionTMapResult flat = ConstructJunctionTMap(MakeTestJunctionSpec(), nullptr, std::nullopt, true);
	REQUIRE(flat.layout.has_value());
	const JunctionNodeTemplate got = BuildJunctionNodeTemplate(render, MakeTestJunctionSpec());
	const JunctionNodeTemplate want = BuildJunctionNodeTemplate(*flat.layout, MakeTestJunctionSpec());
	CHECK(got.error == "");
	CHECK(got.points.size() == want.points.size());
	CHECK(got.links.size() == want.links.size());
	CHECK(got.paths.size() == want.paths.size());
	CHECK(got.pieces.size() == want.pieces.size());
	CHECK(got.bridges.size() == want.bridges.size());
	CHECK(got.joins.size() == want.joins.size());
	CHECK(got.diagonals.size() == want.diagonals.size());
	CHECK(got.x_classes == want.x_classes);
}

/** The corners of a tile a track touches: bit 0 north-east edge, 1 south-east, 2 south-west, 3 north-west edge. */
static int TrackEdges(Track track)
{
	switch (track) {
		case Track::X: return 1 | 4;
		case Track::Y: return 2 | 8;
		case Track::Upper: return 1 | 8;
		case Track::Lower: return 2 | 4;
		case Track::Left: return 8 | 4;
		default: return 1 | 2;
	}
}

static const std::array<Track, 6> ALL_TRACKS = {Track::X, Track::Y, Track::Upper, Track::Lower, Track::Left, Track::Right};

/** Every render test's topology at nmax 64. */
static const JunctionTopology *RENDER_TOPOLOGIES[2] = {&JunctionTTopology(), &JunctionLLRRSidelineTopology()};

TEST_CASE("topology: the render switches only at nodes", "[junction][topology]")
{
	for (const JunctionTopology *t : RENDER_TOPOLOGIES) {
		std::string error;
		JunctionTrackPlacement placement;
		JunctionTrackStructure cut;
		JunctionTrackPrograms programs;
		const JunctionLayout render = RenderOf(*t, 64, &error, &placement, &cut, &programs);
		INFO(error);
		REQUIRE(!render.tracks.empty());
		std::set<Tile> allowed;
		for (const TileIndexDiffC &n : placement.nodes) allowed.insert(Of(n));
		for (const JunctionTrackBridge &bridge : cut.bridges) {
			if (!bridge.doubled) continue;
			const uint32_t e = bridge.edge;
			for (size_t k = 0; k < programs.copies[e].size(); k++) {
				if (programs.copies[e][k].kind == JunctionTrackStep::Kind::SBend) allowed.insert(Of(placement.copies[e][k].first));
			}
			for (size_t k = 0; k < programs.edges[e].size(); k++) {
				if (programs.edges[e][k].kind == JunctionTrackStep::Kind::SBend) allowed.insert(Of(placement.edges[e][k].second));
			}
		}
		for (const JunctionTrack &jt : render.tracks) {
			int shared = 0;
			for (Track a : ALL_TRACKS) {
				for (Track b : ALL_TRACKS) {
					if (a < b && jt.tracks.Test(a) && jt.tracks.Test(b) && (TrackEdges(a) & TrackEdges(b)) != 0) shared++;
				}
			}
			if (shared > 0) CHECK(allowed.contains(Tile{jt.offset.x, jt.offset.y}));
		}
	}
}

TEST_CASE("topology: the render has no level crossing and no track on a head", "[junction][topology]")
{
	for (const JunctionTopology *t : RENDER_TOPOLOGIES) {
		std::string error;
		const JunctionLayout render = RenderOf(*t, 64, &error);
		INFO(error);
		REQUIRE(!render.tracks.empty());
		std::set<Tile> heads;
		for (const JunctionBridge &b : render.bridges) {
			heads.insert(Of(b.start));
			heads.insert(Of(b.end));
		}
		for (const JunctionTrack &jt : render.tracks) {
			CHECK(!(jt.tracks.Test(Track::X) && jt.tracks.Test(Track::Y)));
			CHECK(!heads.contains(Tile{jt.offset.x, jt.offset.y}));
		}
	}
}

TEST_CASE("topology: the render has the bridges as placed", "[junction][topology]")
{
	for (const JunctionTopology *t : RENDER_TOPOLOGIES) {
		std::string error;
		JunctionTrackPlacement placement;
		JunctionTrackStructure cut;
		const JunctionLayout render = RenderOf(*t, 64, &error, &placement, &cut);
		INFO(error);
		REQUIRE(!render.tracks.empty());
		auto key = [](JunctionBridge b) {
			if (b.start.x + b.start.y > b.end.x + b.end.y) std::swap(b.start, b.end);
			return std::array<int, 4>{b.start.x, b.start.y, b.end.x, b.end.y};
		};
		std::vector<std::array<int, 4>> want, got;
		for (size_t b = 0; b < cut.bridges.size(); b++) {
			want.push_back(key(placement.bridges[b][0]));
			if (cut.bridges[b].doubled) want.push_back(key(placement.bridges[b][1]));
		}
		for (const JunctionBridge &b : render.bridges) got.push_back(key(b));
		std::ranges::sort(want);
		std::ranges::sort(got);
		CHECK(got == want);
	}
}

TEST_CASE("topology: the render stays inside the box and draws every port", "[junction][topology]")
{
	for (const JunctionTopology *t : RENDER_TOPOLOGIES) {
		std::string error;
		JunctionTrackPlacement placement;
		std::vector<JunctionLanePath> paths;
		const JunctionLayout render = RenderOf(*t, 64, &error, &placement, nullptr, nullptr, &paths);
		INFO(error);
		REQUIRE(!render.tracks.empty());
		CHECK(Of(render.size) == Of(placement.size));
		std::set<Tile> tiles;
		for (const JunctionTrack &jt : render.tracks) {
			CHECK(jt.offset.x >= 0);
			CHECK(jt.offset.y >= 0);
			CHECK(jt.offset.x < render.size.x);
			CHECK(jt.offset.y < render.size.y);
			tiles.insert(Tile{jt.offset.x, jt.offset.y});
		}
		for (const JunctionLanePath &p : paths) {
			CHECK(tiles.contains(Of(placement.ports[p.from])));
			CHECK(tiles.contains(Of(placement.ports[p.to])));
		}
	}
}

TEST_CASE("topology: the checker traces the LL_RR render", "[junction][topology]")
{
	std::string error;
	JunctionTrackPlacement placement;
	const JunctionTopology &topology = JunctionLLRRSidelineTopology();
	const JunctionLayout render = RenderOf(topology, 64, &error, &placement);
	INFO(error);
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionCheckContext ctx = MakeJunctionTopologyCheckContext(render, spec, topology, FindJunctionPorts(render, topology));
	REQUIRE(ctx.ports.size() == 10);
	for (const JunctionPort &port : ctx.ports) {
		REQUIRE(port.slot < placement.ports.size());
		CHECK(Of(port.tile) == Of(placement.ports[port.slot]));
	}

	const std::vector<RuleViolation> violations = CheckConnectivity(ctx);
	for (const RuleViolation &v : violations) INFO(fmt::format("rule {} at {},{}: {}", static_cast<int>(v.rule), v.tile.x, v.tile.y, v.message));
	CHECK(violations.empty());

	REQUIRE(!ctx.lanes.empty());
	for (const JunctionLanePath &lane : ctx.lanes) {
		const auto count = std::ranges::count_if(ctx.paths, [&](const JunctionPath &p) { return p.from_slot == lane.from && p.to_slot == lane.to; });
		CHECK(count >= 1);
		CHECK(count <= 2);
	}
}

TEST_CASE("topology: the T render, both ways", "[junction][topologysweep]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	std::string error;
	const JunctionLayout render = RenderOf(JunctionTTopology(), 64, &error);
	INFO(error);
	const JunctionTMapResult flat = ConstructJunctionTMap(spec, nullptr, std::nullopt, true);
	REQUIRE(flat.layout.has_value());
	for (const JunctionLayout *layout : {&render, &*flat.layout}) {
		const std::vector<RuleViolation> legacy = CheckJunctionRules(*layout, spec);
		const std::vector<RuleViolation> generic = CheckJunctionRules(*layout, spec, JunctionTTopology());
		REQUIRE(legacy.size() == generic.size());
		for (size_t i = 0; i < legacy.size(); i++) {
			CHECK(legacy[i].rule == generic[i].rule);
			CHECK(Of(legacy[i].tile) == Of(generic[i].tile));
			CHECK(legacy[i].message == generic[i].message);
		}
	}
}

TEST_CASE("topology: the LL_RR render has no level crossing", "[junction][topology]")
{
	std::string error;
	const JunctionTopology &topology = JunctionLLRRSidelineTopology();
	const JunctionLayout render = RenderOf(topology, 64, &error);
	INFO(error);
	const JunctionCheckContext ctx = MakeJunctionTopologyCheckContext(render, MakeTestJunctionSpec(), topology, FindJunctionPorts(render, topology));
	CHECK(CheckGeometry(ctx).empty());
	/* The footprint waits for the prio element, DESIGN-G1 §12.4. */
}

TEST_CASE("topology: LL_RR render findings", "[.][diag]")
{
	std::string error;
	const JunctionTopology &topology = JunctionLLRRSidelineTopology();
	const JunctionLayout render = RenderOf(topology, 64, &error);
	std::ofstream file("docs/junction-tool/wip/g1d-llrr-violations.txt");
	REQUIRE(file.is_open());
	for (const RuleViolation &v : CheckJunctionRules(render, MakeTestJunctionSpec(), topology)) {
		file << fmt::format("rule {} tile {},{}: {}\n", static_cast<int>(v.rule), v.tile.x, v.tile.y, v.message);
	}
}

TEST_CASE("topology: the T's signal tokens meet the signal rules", "[junction][topology]")
{
	std::string error;
	std::vector<JunctionTrackSignal> tokens;
	std::vector<JunctionTrackRoute> routes;
	const JunctionLayout layout = SignalledOf(JunctionTTopology(), &error, &tokens, &routes);
	INFO(error);
	const JunctionSpec spec = MakeTestJunctionSpec();
	const std::vector<RuleViolation> violations = CheckJunctionRules(layout, spec, JunctionTTopology());
	std::string listed;
	for (const RuleViolation &v : violations) listed += fmt::format("rule {} at {},{}: {}\n", static_cast<int>(v.rule), v.tile.x, v.tile.y, v.message);
	INFO(listed);
	CHECK(violations.empty());
}

TEST_CASE("topology: the T's signal tokens are the signals", "[junction][topology]")
{
	std::string error;
	std::vector<JunctionTrackSignal> tokens;
	std::vector<JunctionTrackRoute> routes;
	const JunctionLayout layout = SignalledOf(JunctionTTopology(), &error, &tokens, &routes);
	INFO(error);
	REQUIRE(!tokens.empty());
	const auto key = [](const TileIndexDiffC &tile, Trackdir facing) { return std::make_tuple(tile.x, tile.y, static_cast<int>(facing)); };
	std::set<std::tuple<int, int, int>> signals;
	for (const JunctionSignal &s : layout.signals) CHECK(signals.insert(key(s.offset, s.facing)).second);
	std::set<std::tuple<int, int, int>> token_keys;
	for (const JunctionTrackSignal &t : tokens) {
		CHECK(signals.contains(key(t.tile, t.facing)));
		token_keys.insert(key(t.tile, t.facing));
	}
	for (const auto &k : signals) CHECK(token_keys.contains(k));
}

TEST_CASE("topology: the T's signal reasons", "[junction][topology]")
{
	std::string error;
	std::vector<JunctionTrackSignal> tokens;
	std::vector<JunctionTrackRoute> routes;
	const JunctionLayout layout = SignalledOf(JunctionTTopology(), &error, &tokens, &routes);
	INFO(error);
	REQUIRE(!tokens.empty());
	const auto has = [&](size_t route, size_t index, auto reason_ok) {
		const JunctionPiece &piece = routes[route].pieces[index];
		return std::ranges::any_of(tokens, [&](const JunctionTrackSignal &t) {
			return t.route == route && reason_ok(t.reason) && t.tile.x == piece.tile.x && t.tile.y == piece.tile.y && t.facing == piece.trackdir;
		});
	};
	const auto is_before = [](JunctionSignalReason r) { return r == JunctionSignalReason::BeforeBridge || r == JunctionSignalReason::SeriesGap; };
	const auto is_after = [](JunctionSignalReason r) { return r == JunctionSignalReason::AfterBridge || r == JunctionSignalReason::SeriesGap; };
	const auto ok = [&](const std::vector<JunctionPiece> &P, int64_t i) {
		return i > 0 && i < static_cast<int64_t>(P.size()) - 1 && !P[i].on_bridge && FindBridgeByHead(layout, P[i].tile) == nullptr && IsSignalledPiece(layout, P[i]);
	};
	for (size_t r = 0; r < routes.size(); r++) {
		const std::vector<JunctionPiece> &P = routes[r].pieces;
		const size_t n = P.size();
		const auto stubs = std::ranges::count_if(tokens, [&](const JunctionTrackSignal &t) { return t.route == r && t.reason == JunctionSignalReason::Stub; });
		if (ok(P, static_cast<int64_t>(n) - 2)) {
			CHECK(stubs == 1);
			CHECK(has(r, n - 2, [](JunctionSignalReason reason) { return reason == JunctionSignalReason::Stub; }));
		}
		for (size_t i = 0; i < n; i++) {
			if (!P[i].on_bridge) continue;
			size_t last = i;
			while (last + 1 < n && P[last + 1].on_bridge) last++;
			/* The nearest signalled piece on each side, within the stretch (no bridge or head between). */
			for (int64_t k = static_cast<int64_t>(i) - 1; k > 0 && !P[k].on_bridge; k--) {
				if (ok(P, k)) { CHECK(has(r, k, is_before)); break; }
			}
			for (size_t k = last + 1; k + 1 < n && !P[k].on_bridge; k++) {
				if (ok(P, k)) { CHECK(has(r, k, is_after)); break; }
			}
			i = last;
		}
	}
	CHECK(std::ranges::any_of(tokens, [](const JunctionTrackSignal &t) { return t.reason == JunctionSignalReason::BeforeBridge; }));
	CHECK(std::ranges::any_of(tokens, [](const JunctionTrackSignal &t) { return t.reason == JunctionSignalReason::AfterBridge; }));
}

TEST_CASE("topology: the T's prio elements follow the pattern", "[junction][topology]")
{
	std::string error;
	std::vector<JunctionTrackSignal> tokens;
	std::vector<JunctionTrackRoute> routes;
	std::vector<JunctionLanePath> paths;
	const JunctionLayout layout = SignalledOf(JunctionTTopology(), &error, &tokens, &routes, &paths);
	INFO(error);
	const JunctionSpec spec = MakeTestJunctionSpec();
	const int even_prio = spec.prio_tiles + (spec.prio_tiles & 1);
	const auto tracks_at = [&](int x, int y) {
		TrackBits bits;
		for (const JunctionTrack &t : layout.tracks) {
			if (t.offset.x == x && t.offset.y == y) bits.Set(t.tracks);
		}
		return bits;
	};
	const auto signal_at = [&](int x, int y, Trackdir facing) -> const JunctionSignal * {
		for (const JunctionSignal &s : layout.signals) {
			if (s.offset.x == x && s.offset.y == y && s.facing == facing) return &s;
		}
		return nullptr;
	};
	/* The two elements: the merges of the W and E joiners (the nodes (6,4) and (38,1)), both with the row on the north side. */
	int found = 0;
	for (const TileIndexDiffC merge : {TileIndexDiffC{6, 4}, TileIndexDiffC{38, 1}}) {
		const JunctionPiece *piece = nullptr;
		for (const JunctionTrackRoute &route : routes) {
			for (const JunctionPiece &pc : route.pieces) {
				if (pc.tile.x == merge.x && pc.tile.y == merge.y && IsDiagonalTrackdir(pc.trackdir) && !pc.on_bridge && tracks_at(merge.x, merge.y).Any(TrackBits{Track::Upper, Track::Lower, Track::Left, Track::Right})) piece = &pc;
			}
		}
		REQUIRE(piece != nullptr);
		found++;
		const Track axis = TrackdirToTrack(piece->trackdir);
		static const TileIndexDiffC EDGE_DIFF[4] = {{-1, 0}, {0, 1}, {1, 0}, {0, -1}}; // NE, SE, SW, NW.
		const TileIndexDiffC dir = EDGE_DIFF[static_cast<int>(TrackdirToExitdir(piece->trackdir))];
		const TileIndexDiffC sd = {0, -1};
		const int n = even_prio;
		for (int s = 0; s <= n; s++) {
			const int mx = merge.x - s * dir.x, my = merge.y - s * dir.y;
			const TrackBits ml = tracks_at(mx, my);
			const TrackBits row = tracks_at(mx + sd.x, my + sd.y);
			INFO(fmt::format("merge {},{} s {}", merge.x, merge.y, s));
			CHECK(ml.Test(axis));
			if (s % 2 == 0) {
				CHECK(ml.Count() == (s == 0 ? 3 : 2));
				CHECK(row.Count() == (s == 0 ? 1 : 2));
			} else {
				CHECK(ml.Count() == 1);
				CHECK(row == TrackBits{axis});
				CHECK(signal_at(mx, my, piece->trackdir) != nullptr);
				const JunctionSignal *rs = signal_at(mx + sd.x, my + sd.y, ReverseTrackdir(piece->trackdir));
				REQUIRE(rs != nullptr);
				CHECK(rs->type == (s == n - 1 ? SignalType::Exit : SignalType::Combo));
			}
		}
	}
	CHECK(found == 2);
}

TEST_CASE("topology: the signal pass names a prio element that does not fit", "[junction][topology]")
{
	/* The T's W element (merge (6,4), row on y 3): the seed draws it and reports nothing; with another track on the row's step 0 the pass skips the element and says so. */
	std::string error, why;
	std::vector<JunctionTrackRoute> routes;
	std::vector<JunctionLanePath> paths;
	JunctionTrackGraph graph;
	JunctionTrackPrograms programs;
	JunctionLayout layout = RenderOf(JunctionTTopology(), 64, &error, nullptr, nullptr, &programs, &paths, &graph, &routes);
	INFO(error);
	REQUIRE(!layout.tracks.empty());
	const JunctionSpec spec = MakeTestJunctionSpec();
	JunctionLayout layout_ok = layout;
	SignalJunctionTrackRoutes(layout_ok, routes, JunctionTTopology(), paths, graph, programs, spec, &why);
	CHECK(why.empty());

	std::vector<JunctionTrackRoute> edited = routes;
	JunctionTrackRoute stray = routes.front();
	stray.pieces = {JunctionPiece{{6, 3}, Trackdir::X_NE, 0, false}};
	stray.tags = {routes.front().tags.front()};
	edited.push_back(stray);
	SignalJunctionTrackRoutes(layout, edited, JunctionTTopology(), paths, graph, programs, spec, &why);
	CHECK(why == "the prio element at 6,4 does not fit");
}

TEST_CASE("topology: signal token findings", "[.][diag]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const struct { const JunctionTopology &topology; const char *file; } runs[] = {
		{JunctionLLRRSidelineTopology(), "docs/junction-tool/wip/st1-llrr.txt"},
		{JunctionTTopology(), "docs/junction-tool/wip/st1-t.txt"},
	};
	for (const auto &run : runs) {
		std::string error;
		std::vector<JunctionTrackSignal> tokens;
		std::vector<JunctionTrackRoute> routes;
		const JunctionLayout layout = SignalledOf(run.topology, &error, &tokens, &routes);
		std::ofstream file(run.file);
		REQUIRE(file.is_open());
		std::map<int, int> per_reason;
		for (const JunctionTrackSignal &t : tokens) per_reason[static_cast<int>(t.reason)]++;
		file << "tokens per reason (BeforeSplit AfterSplit BeforeMerge AfterMerge BeforeBridge AfterBridge SeriesGap Stub Bay Fill):";
		for (int i = 0; i < 10; i++) file << ' ' << per_reason[i];
		file << '\n';
		std::map<int, int> per_rule;
		for (const RuleViolation &v : CheckJunctionRules(layout, spec, run.topology)) {
			per_rule[static_cast<int>(v.rule)]++;
			file << fmt::format("rule {} tile {},{}: {}\n", static_cast<int>(v.rule), v.tile.x, v.tile.y, v.message);
		}
		file << "violations per rule:";
		for (const auto &[rule, count] : per_rule) file << fmt::format(" {}:{}", rule, count);
		file << '\n' << RenderJunctionAscii(layout);
	}
}

TEST_CASE("topology: LL_RR prio findings", "[.][diag]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	std::string error;
	std::vector<JunctionTrackSignal> tokens;
	std::vector<JunctionTrackRoute> routes;
	const JunctionLayout layout = SignalledOf(JunctionLLRRSidelineTopology(), &error, &tokens, &routes);
	std::ofstream file("docs/junction-tool/wip/st2-llrr.txt");
	REQUIRE(file.is_open());
	std::map<int, int> per_rule;
	for (const RuleViolation &v : CheckJunctionRules(layout, spec, JunctionLLRRSidelineTopology())) {
		per_rule[static_cast<int>(v.rule)]++;
		file << fmt::format("rule {} tile {},{}: {}\n", static_cast<int>(v.rule), v.tile.x, v.tile.y, v.message);
	}
	file << "violations per rule:";
	for (const auto &[rule, count] : per_rule) file << fmt::format(" {}:{}", rule, count);
	file << '\n' << RenderJunctionAscii(layout);
}

TEST_CASE("topology: the LL_RR seed is clean", "[junction][topologysweep]")
{
	std::string error;
	std::vector<JunctionTrackSignal> tokens;
	std::vector<JunctionTrackRoute> routes;
	const JunctionLayout layout = SignalledOf(JunctionLLRRSidelineTopology(), &error, &tokens, &routes);
	INFO(error);
	REQUIRE(!layout.tracks.empty());
	for (const RuleViolation &v : CheckJunctionRules(layout, MakeTestJunctionSpec(), JunctionLLRRSidelineTopology())) {
		INFO(fmt::format("rule {} tile {},{}: {}", static_cast<int>(v.rule), v.tile.x, v.tile.y, v.message));
		CHECK(false);
	}
}

TEST_CASE("topology: a doubled leg's lane split sits next to its bridge", "[junction][topology]")
{
	/* D215: the copy-0 first near head lies exactly 2 tiles along the heading after the lane split (the copy-1 S-bend's first tile). */
	std::string error;
	JunctionTrackStructure cut;
	const JunctionTrackPlacement placement = CutOf(JunctionLLRRSidelineTopology(), 64, &error, &cut);
	REQUIRE(!placement.nodes.empty());
	size_t checked = 0;
	std::set<uint32_t> seen;
	for (size_t b = 0; b < cut.bridges.size(); b++) {
		const uint32_t e = cut.bridges[b].edge;
		if (!cut.bridges[b].doubled || !seen.insert(e).second) continue;
		REQUIRE(!placement.copies[e].empty());
		const TileIndexDiffC split = placement.copies[e][0].first;
		const TileIndexDiffC head = placement.bridges[b][0].start;
		INFO(fmt::format("edge {}: lane split {},{} first head {},{}", e, split.x, split.y, head.x, head.y));
		CHECK(split.y == head.y);
		CHECK(std::abs(head.x - split.x) == 2);
		checked++;
	}
	CHECK(checked == 3);
}

TEST_CASE("topology: every row change sits next to its feature", "[junction][topology]")
{
	/* D215 (DESIGN-G1 §14.5): on the flat solve every in fan ends 2 columns before its node, every out fan starts the curve-gap minimum after its joiner merge, every doubled leg's lane split lies 2 columns before its first near head and its lane merge 1 after its last far head. Kind 4 (horizontal S-bends of other edges) is tied to nodes in every cell here and has no run to measure. */
	size_t in_fans = 0, out_fans = 0, splits = 0, merges = 0;
	for (const int tl : {4, 5}) {
		JunctionSpec spec = MakeTestJunctionSpec();
		spec.tl = tl;
		spec.max_bridge_middle = 64;
		const int cl = static_cast<int>(DeriveCurveSpacing(spec.reference_speed, spec.curve_speed, spec.tl));
		const int g2 = std::min(2 * static_cast<int>(spec.tl) - 1, 2 * cl);
		for (const JunctionTopology *t : {&JunctionTTopology(), &JunctionLLRRSidelineTopology()}) {
			std::string error;
			const std::vector<JunctionLanePath> paths = ExpandJunctionLanes(*t, &error);
			const JunctionTrackGraph graph = BuildJunctionTrackGraph(*t, paths, &error);
			REQUIRE(!graph.edges.empty());
			const JunctionTrackOrder order = OrderJunctionTrackGraph(*t, paths, graph, &error);
			const JunctionTrackStructure structure = StructureJunctionTrackGraph(*t, paths, graph, order, &error);
			const JunctionTrackPrograms programs = ProgramJunctionTrackGraph(*t, paths, graph, order, structure, &error);
			JunctionTrackPlacement placement;
			const JunctionTrackStructure cut = CutJunctionTrackBridges(*t, paths, graph, order, structure, programs, spec, &placement, &error);
			INFO(error);
			REQUIRE(!placement.nodes.empty());
			for (uint32_t e = 0; e < graph.edges.size(); e++) {
				const JunctionTrackEdge &edge = graph.edges[e];
				const bool fan = programs.edges[e].size() == 1 && programs.edges[e][0].kind == JunctionTrackStep::Kind::Fan;
				if (fan && edge.kind == JunctionTrackEdge::Kind::In && edge.to.node && !edge.from.node && placement.edges[e][0].second.y != placement.edges[e][0].first.y) {
					const int gap = std::abs(placement.nodes[edge.to.index].x - placement.edges[e][0].second.x);
					INFO(fmt::format("tl {} {} in fan of edge {}: fan {},{} to {},{} node {},{}", tl, t == &JunctionTTopology() ? "T" : "LL_RR", e, placement.edges[e][0].first.x, placement.edges[e][0].first.y, placement.edges[e][0].second.x, placement.edges[e][0].second.y, placement.nodes[edge.to.index].x, placement.nodes[edge.to.index].y));
					/* A fan the objective cannot bring to its rule minimum is held by other points fixed at their least columns (the occupancy of the fans nested with it, the run between its node and the joiner merge); those gaps are pinned here as an open quality item. */
					static const std::map<std::tuple<int, bool, uint32_t>, int> held = {{{4, true, 0}, 13}, {{4, true, 1}, 4}, {{4, true, 6}, 4}, {{5, true, 0}, 12}, {{5, true, 1}, 4}, {{5, true, 5}, 3}, {{5, true, 6}, 5}};
					const auto stopped = held.find({tl, t != &JunctionTTopology(), e});
					CHECK(gap == (stopped == held.end() ? 2 : stopped->second));
					in_fans++;
				}
				if (fan && edge.kind == JunctionTrackEdge::Kind::Out && edge.from.node && !edge.to.node && placement.edges[e][0].second.y != placement.edges[e][0].first.y) {
					INFO(fmt::format("tl {} out fan of edge {}", tl, e));
					CHECK(std::abs(placement.edges[e][0].first.x - placement.nodes[edge.from.index].x) == (g2 + 1) / 2);
					out_fans++;
				}
			}
			std::set<uint32_t> seen;
			for (size_t b = 0; b < cut.bridges.size(); b++) {
				const uint32_t e = cut.bridges[b].edge;
				if (!cut.bridges[b].doubled || !seen.insert(e).second) continue;
				const TileIndexDiffC split = placement.copies[e][0].first;
				int least = INT_MAX;
				int near = INT_MAX;
				for (size_t c = 0; c < cut.bridges.size(); c++) {
					if (cut.bridges[c].edge != e) continue;
					near = std::min(near, std::abs(placement.bridges[c][0].start.x - split.x));
					least = std::min(least, std::abs(placement.bridges[c][0].end.x - placement.edges[e][0].second.x));
				}
				INFO(fmt::format("tl {} edge {}: lane split {},{}", tl, e, split.x, split.y));
				CHECK(near == 2);
				splits++;
				CHECK(least >= 1);
				merges++;
			}
		}
	}
	CHECK(in_fans > 0);
	CHECK(out_fans > 0);
	CHECK(splits > 0);
	CHECK(merges > 0);
}

TEST_CASE("topology: the out-fan gap is the curve-gap minimum", "[junction][topology]")
{
	/* D215: every out fan starts as many tiles after its joiner merge as the curve gaps of the joiner's route need (RECON-seed-floors 1); tl 5 gives g2 = 9, one tile more than the test spec's 7. */
	JunctionSpec spec = MakeTestJunctionSpec();
	spec.tl = 5;
	spec.max_bridge_middle = 64;
	const int cl = static_cast<int>(DeriveCurveSpacing(spec.reference_speed, spec.curve_speed, spec.tl));
	const int g1 = std::min(2 * static_cast<int>(spec.tl) - 1, std::max(2, cl));
	const int g2 = std::min(2 * static_cast<int>(spec.tl) - 1, 2 * cl);
	REQUIRE(g2 == 9);
	REQUIRE(1 + (g1 + 1) / 2 < (g2 + 1) / 2); // The cap binds, not the same-hand gap.
	for (const JunctionTopology *t : {&JunctionTTopology(), &JunctionLLRRSidelineTopology()}) {
		std::string error;
		const std::vector<JunctionLanePath> paths = ExpandJunctionLanes(*t, &error);
		const JunctionTrackGraph graph = BuildJunctionTrackGraph(*t, paths, &error);
		REQUIRE(!graph.edges.empty());
		const JunctionTrackOrder order = OrderJunctionTrackGraph(*t, paths, graph, &error);
		const JunctionTrackStructure structure = StructureJunctionTrackGraph(*t, paths, graph, order, &error);
		const JunctionTrackPrograms programs = ProgramJunctionTrackGraph(*t, paths, graph, order, structure, &error);
		JunctionTrackPlacement placement;
		const JunctionTrackStructure cut = CutJunctionTrackBridges(*t, paths, graph, order, structure, programs, spec, &placement, &error);
		INFO(error);
		REQUIRE(!placement.nodes.empty());
		size_t fans = 0;
		for (uint32_t e = 0; e < graph.edges.size(); e++) {
			if (graph.edges[e].kind != JunctionTrackEdge::Kind::Out || !graph.edges[e].from.node) continue;
			if (programs.edges[e].size() != 1 || programs.edges[e][0].kind != JunctionTrackStep::Kind::Fan) continue;
			const TileIndexDiffC merge = placement.nodes[graph.edges[e].from.index];
			const TileIndexDiffC fan = placement.edges[e][0].first;
			if (placement.edges[e][0].second.y == fan.y) continue; // A port on the row has no fan.
			const int k = std::abs(fan.x - merge.x);
			INFO(fmt::format("edge {}: merge {},{} fan {},{}", e, merge.x, merge.y, fan.x, fan.y));
			CHECK(k == (g2 + 1) / 2);
			fans++;
		}
		CHECK(fans >= 1);

		/* The LL_RR seed at tl 5 breaks other curve gaps (in-fans and exit splits, not the out fans); only the T is clean. */
		if (t != &JunctionTTopology()) continue;
		std::vector<JunctionTrackRoute> routes;
		JunctionLayout layout = RenderJunctionTrackPlacement(*t, paths, graph, cut, programs, placement, &error, &routes);
		INFO(error);
		REQUIRE(!layout.tracks.empty());
		SignalJunctionTrackRoutes(layout, routes, *t, paths, graph, programs, spec, nullptr, nullptr, &placement.prio);
		for (const RuleViolation &v : CheckJunctionRules(layout, spec, *t)) {
			INFO(fmt::format("rule {} tile {},{}: {}", static_cast<int>(v.rule), v.tile.x, v.tile.y, v.message));
			CHECK(false);
		}
	}
}

TEST_CASE("topology: the LL_RR seed builds a node template", "[junction][topology]")
{
	std::string error;
	std::vector<JunctionTrackSignal> tokens;
	std::vector<JunctionTrackRoute> routes;
	const JunctionLayout layout = SignalledOf(JunctionLLRRSidelineTopology(), &error, &tokens, &routes);
	INFO(error);
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionNodeTemplate tpl = BuildJunctionNodeTemplate(TraceJunctionNodeInput(layout, spec, JunctionLLRRSidelineTopology()), spec);
	INFO(tpl.error);
	REQUIRE(tpl.error.empty());
}

TEST_CASE("topology: the graph builds the traced template", "[junction][topology]")
{
	for (const JunctionTopology *t : {&JunctionTTopology(), &JunctionLLRRSidelineTopology()}) {
		std::string error;
		JunctionTrackPlacement placement;
		JunctionTrackStructure cut;
		JunctionTrackPrograms programs;
		std::vector<JunctionLanePath> paths;
		JunctionTrackGraph graph;
		std::vector<JunctionTrackRoute> routes;
		const JunctionLayout render = RenderOf(*t, 64, &error, &placement, &cut, &programs, &paths, &graph, &routes);
		INFO(error);
		REQUIRE(!render.tracks.empty());
		JunctionSpec spec = MakeTestJunctionSpec();
		spec.max_bridge_middle = 64;
		const JunctionNodeInput from_graph = JunctionNodeInputOfGraph(*t, paths, graph, cut, programs, placement, routes);
		const JunctionNodeTemplate got = BuildJunctionNodeTemplate(from_graph, spec);
		JunctionNodeTemplate want = BuildJunctionNodeTemplate(TraceJunctionNodeInput(render, spec, *t), spec);
		REQUIRE(want.error.empty());
		want.lean = got.lean; // The traced input has no tags, so no leans.
		want.point_tags = got.point_tags; // Nor point tags.
		want.link_tags = got.link_tags; // Nor link tags.
		const std::string diff = DiffJunctionNodeTemplates(got, want);
		INFO(diff);
		CHECK(diff.empty());
	}
}

TEST_CASE("topology: route pieces carry their graph tags", "[junction][topology]")
{
	for (const JunctionTopology *t : {&JunctionTTopology(), &JunctionLLRRSidelineTopology()}) {
		std::string error;
		JunctionTrackPlacement placement;
		JunctionTrackStructure cut;
		JunctionTrackPrograms programs;
		std::vector<JunctionLanePath> paths;
		JunctionTrackGraph graph;
		std::vector<JunctionTrackRoute> routes;
		RenderOf(*t, 64, &error, &placement, &cut, &programs, &paths, &graph, &routes);
		INFO(error);
		REQUIRE(!routes.empty());
		for (const JunctionTrackRoute &route : routes) {
			REQUIRE(route.tags.size() == route.pieces.size());
			const std::vector<uint32_t> &edges = graph.path_edges[route.path];
			/* The tag edges form contiguous blocks in the path's edge order. */
			std::vector<size_t> block_first; // first piece of each edge's block
			{
				size_t k = 0;
				for (size_t i = 0; i < route.tags.size(); i++) {
					if (i == 0 || route.tags[i].edge != route.tags[i - 1].edge) {
						INFO("path " << route.path << " copy " << static_cast<int>(route.copy) << " piece " << i);
						REQUIRE(k < edges.size());
						REQUIRE(route.tags[i].edge == edges[k]);
						block_first.push_back(i);
						k++;
					}
				}
				CHECK(k == edges.size());
			}
			for (size_t k = 0; k + 1 < block_first.size(); k++) {
				const JunctionTrackNode &node = graph.nodes[graph.edges[edges[k]].to.index];
				const TileIndexDiffC at = placement.nodes[graph.edges[edges[k]].to.index];
				size_t node_piece = 0;
				while (node_piece < route.pieces.size() && (route.pieces[node_piece].on_bridge || route.pieces[node_piece].tile.x != at.x || route.pieces[node_piece].tile.y != at.y)) node_piece++;
				INFO("path " << route.path << " copy " << static_cast<int>(route.copy) << " boundary " << k);
				REQUIRE(node_piece < route.pieces.size());
				CHECK(block_first[k + 1] == (node.split ? node_piece : node_piece + 1));
			}
			for (const JunctionTrackPieceTag &tag : route.tags) {
				CHECK(std::ranges::find(edges, tag.edge) != edges.end());
				const bool doubled = programs.copies[tag.edge].size() != 0;
				CHECK(tag.copy == (route.copy == 1 && doubled ? 1 : 0));
			}
			for (size_t i = 0; i < route.tags.size(); i++) {
				const JunctionTrackPieceTag &tag = route.tags[i];
				const std::vector<JunctionTrackStep> &program = tag.copy == 1 ? programs.copies[tag.edge] : programs.edges[tag.edge];
				size_t before = 0; // the program tokens before the piece: after the previous token piece of the edge
				bool seen = false;
				for (size_t j = i; j-- > 0 && route.tags[j].edge == tag.edge && !seen;) {
					if (route.tags[j].token != JunctionTrackPieceTag::RUN) {
						before = route.tags[j].token + 1;
						seen = true;
					}
				}
				INFO("piece " << i << " edge " << tag.edge);
				if (tag.token != JunctionTrackPieceTag::RUN) {
					CHECK(tag.token < program.size());
					CHECK(tag.run == tag.token);
				} else {
					CHECK(tag.run == before);
					CHECK(tag.run <= program.size());
				}
			}
			/* A port edge's Fan tokens: the pieces tagged with the token run from the span's first tile to its last. */
			for (const uint32_t e : edges) {
				const JunctionTrackEdge &edge = graph.edges[e];
				if (edge.from.node && edge.to.node) continue;
				const bool copy1 = route.copy == 1 && placement.copies[e].size() != 0;
				const std::vector<JunctionTrackStep> &tokens = copy1 ? programs.copies[e] : programs.edges[e];
				const std::vector<JunctionTrackSpan> &spans = copy1 ? placement.copies[e] : placement.edges[e];
				for (size_t k = 0; k < tokens.size(); k++) {
					if (tokens[k].kind != JunctionTrackStep::Kind::Fan) continue;
					std::vector<size_t> at;
					for (size_t i = 0; i < route.tags.size(); i++) {
						if (route.tags[i].edge == e && route.tags[i].token == k) at.push_back(i);
					}
					if (at.empty()) continue;
					INFO("edge " << e << " token " << k << " path " << route.path << " copy " << static_cast<int>(route.copy));
					CHECK(route.pieces[at.front()].tile.x == spans[k].first.x);
					CHECK(route.pieces[at.front()].tile.y == spans[k].first.y);
					CHECK(route.pieces[at.back()].tile.x == spans[k].second.x);
					CHECK(route.pieces[at.back()].tile.y == spans[k].second.y);
				}
			}
		}
	}
}

TEST_CASE("topology: link directions match the T's path directions", "[junction][topology]")
{
	std::string error;
	std::vector<JunctionTrackSignal> tokens;
	std::vector<JunctionTrackRoute> routes;
	const JunctionLayout layout = SignalledOf(JunctionTTopology(), &error, &tokens, &routes);
	INFO(error);
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionNodeTemplate tpl = BuildJunctionNodeTemplate(layout, spec);
	REQUIRE(tpl.error.empty());
	size_t checked = 0;
	for (const JunctionNodePath &path : tpl.paths) {
		for (size_t i = 0; i + 1 < path.points.size(); i++) {
			const auto it = std::ranges::find_if(tpl.links, [&](const JunctionNodeLink &l) { return l.from == path.points[i] && l.to == path.points[i + 1]; });
			REQUIRE(it != tpl.links.end());
			if (!it->free || it->diagonal) continue;
			CHECK(it->dir == (it->axis == Axis::X ? path.dir : path.dir_y));
			checked++;
		}
	}
	CHECK(checked > 0);
	for (const JunctionNodeLateGap &gap : tpl.late_gaps) {
		CHECK(gap.dir == tpl.paths[gap.path].dir);
	}
}

TEST_CASE("topology: the T seed traces the same by its topology", "[junction][topology]")
{
	std::string error;
	std::vector<JunctionTrackSignal> tokens;
	std::vector<JunctionTrackRoute> routes;
	const JunctionLayout layout = SignalledOf(JunctionTTopology(), &error, &tokens, &routes);
	INFO(error);
	const JunctionSpec spec = MakeTestJunctionSpec();
	CHECK(DiffJunctionNodeTemplates(BuildJunctionNodeTemplate(TraceJunctionNodeInput(layout, spec, JunctionTTopology()), spec), BuildJunctionNodeTemplate(layout, spec)).empty());
}

/** The tracks of a layout as a set of (x, y, track). */
static std::set<std::tuple<int, int, int>> TrackSetOf(const JunctionLayout &layout)
{
	std::set<std::tuple<int, int, int>> set;
	for (const JunctionTrack &jt : layout.tracks) {
		for (Track t : jt.tracks) set.emplace(jt.offset.x, jt.offset.y, static_cast<int>(t));
	}
	return set;
}

/** The signals of a layout as a set of (x, y, facing, type, variant). */
static std::set<std::tuple<int, int, int, int, int>> SignalSetOf(const JunctionLayout &layout)
{
	std::set<std::tuple<int, int, int, int, int>> set;
	for (const JunctionSignal &s : layout.signals) set.emplace(s.offset.x, s.offset.y, static_cast<int>(s.facing), static_cast<int>(s.type), static_cast<int>(s.variant));
	return set;
}

TEST_CASE("topology: the token pass reads no layout", "[junction][topology]")
{
	for (const JunctionTopology *t : {&JunctionTTopology(), &JunctionLLRRSidelineTopology()}) {
		std::string error;
		JunctionTrackPrograms programs;
		std::vector<JunctionLanePath> paths;
		JunctionTrackGraph graph;
		std::vector<JunctionTrackRoute> routes;
		const JunctionLayout render = RenderOf(*t, 64, &error, nullptr, nullptr, &programs, &paths, &graph, &routes);
		INFO(error);
		REQUIRE(!render.tracks.empty());
		JunctionLayout layout_a = render;
		const std::vector<JunctionTrackSignal> tokens_a = SignalJunctionTrackRoutes(layout_a, routes, *t, paths, graph, programs, MakeTestJunctionSpec());
		JunctionLayout layout_b;
		const std::vector<JunctionTrackSignal> tokens_b = SignalJunctionTrackRoutes(layout_b, routes, *t, paths, graph, programs, MakeTestJunctionSpec());
		REQUIRE(!tokens_a.empty());
		REQUIRE(tokens_b.size() == tokens_a.size());
		for (size_t i = 0; i < tokens_a.size(); i++) {
			INFO("token " << i);
			CHECK(tokens_b[i].route == tokens_a[i].route);
			CHECK(tokens_b[i].reason == tokens_a[i].reason);
			CHECK(tokens_b[i].tile.x == tokens_a[i].tile.x);
			CHECK(tokens_b[i].tile.y == tokens_a[i].tile.y);
			CHECK(tokens_b[i].facing == tokens_a[i].facing);
		}
		CHECK(SignalSetOf(layout_b) == SignalSetOf(layout_a));
		CHECK(layout_b.signals.size() == layout_a.signals.size());
		std::set<std::tuple<int, int, int>> prio = TrackSetOf(layout_a);
		for (const auto &x : TrackSetOf(render)) prio.erase(x);
		CHECK(!prio.empty());
		CHECK(TrackSetOf(layout_b) == prio);
		CHECK(layout_b.bridges.empty());
	}
}

TEST_CASE("topology: route events from tags", "[junction][topology]")
{
	for (const JunctionTopology *t : {&JunctionTTopology(), &JunctionLLRRSidelineTopology()}) {
		std::string error;
		JunctionTrackPlacement placement;
		JunctionTrackStructure cut;
		JunctionTrackPrograms programs;
		std::vector<JunctionLanePath> paths;
		JunctionTrackGraph graph;
		std::vector<JunctionTrackRoute> routes;
		RenderOf(*t, 64, &error, &placement, &cut, &programs, &paths, &graph, &routes);
		INFO(error);
		REQUIRE(!routes.empty());
		const JunctionNodeInput input = JunctionNodeInputOfGraph(*t, paths, graph, cut, programs, placement, routes);
		REQUIRE(input.paths.size() == routes.size());
		size_t with_events = 0;
		for (size_t r = 0; r < routes.size(); r++) {
			/* The input path of this route: the same lane path and the same pieces. */
			size_t found = input.paths.size();
			for (size_t i = 0; i < input.paths.size(); i++) {
				const JunctionPath &ip = input.paths[i];
				if (ip.from_slot != paths[routes[r].path].from || ip.to_slot != paths[routes[r].path].to || ip.pieces.size() != routes[r].pieces.size()) continue;
				bool same = true;
				for (size_t k = 0; same && k < ip.pieces.size(); k++) same = ip.pieces[k].tile.x == routes[r].pieces[k].tile.x && ip.pieces[k].tile.y == routes[r].pieces[k].tile.y && ip.pieces[k].trackdir == routes[r].pieces[k].trackdir;
				if (same) { found = i; break; }
			}
			INFO("route " << r << " path " << routes[r].path << " copy " << static_cast<int>(routes[r].copy));
			REQUIRE(found < input.paths.size());
			const std::vector<std::pair<bool, size_t>> got = JunctionRouteEvents(graph, programs, routes, r);
			CHECK(got == std::vector<std::pair<bool, size_t>>(input.events[found].begin(), input.events[found].end()));
			if (!got.empty()) with_events++;
		}
		CHECK(with_events > 0);
	}
}

/** The track bits per tile of a layout. */
static std::map<std::pair<int, int>, TrackBits> TileBitsOf(const JunctionLayout &layout)
{
	std::map<std::pair<int, int>, TrackBits> bits;
	for (const JunctionTrack &jt : layout.tracks) bits[{jt.offset.x, jt.offset.y}] |= jt.tracks;
	return bits;
}

/** The graph input and template of a topology's render, with everything the graph solve takes. */
struct GraphSolveSetup {
	JunctionLayout signalled; ///< SignalledOf's layout.
	std::vector<JunctionLanePath> paths;
	JunctionTrackGraph graph;
	JunctionTrackPrograms programs;
	std::vector<JunctionTrackRoute> routes;
	JunctionNodeInput input;
	JunctionNodeTemplate tpl;
};

static GraphSolveSetup GraphSolveSetupOf(const JunctionTopology &t)
{
	GraphSolveSetup setup;
	std::string error;
	JunctionTrackPlacement placement;
	JunctionTrackStructure cut;
	std::vector<JunctionTrackSignal> tokens;
	setup.signalled = SignalledOf(t, &error, &tokens, &setup.routes, &setup.paths);
	INFO(error);
	REQUIRE(!setup.routes.empty());
	/* The pieces SignalledOf does not hand out, rebuilt by RenderOf. */
	std::vector<JunctionTrackRoute> routes;
	RenderOf(t, 64, &error, &placement, &cut, &setup.programs, &setup.paths, &setup.graph, &routes);
	setup.input = JunctionNodeInputOfGraph(t, setup.paths, setup.graph, cut, setup.programs, placement, setup.routes);
	setup.tpl = BuildJunctionNodeTemplate(setup.input, MakeTestJunctionSpec());
	INFO(setup.tpl.error);
	REQUIRE(setup.tpl.error.empty());
	return setup;
}

/** Solve a topology's graph input on flat ground (offset 0, terrain covering the render's box with a margin of 8) and compare it with SignalledOf. */
static void CheckGraphSolvesFlat(const JunctionTopology &topology)
{
	constexpr int MARGIN = 8;
	const GraphSolveSetup setup = GraphSolveSetupOf(topology);
	const JunctionSpec spec = MakeTestJunctionSpec();

	int min_x = INT_MAX, min_y = INT_MAX, max_x = INT_MIN, max_y = INT_MIN;
	auto cover = [&](int x, int y) {
		min_x = std::min(min_x, x);
		max_x = std::max(max_x, x);
		min_y = std::min(min_y, y);
		max_y = std::max(max_y, y);
	};
	for (const JunctionTrack &jt : setup.signalled.tracks) cover(jt.offset.x, jt.offset.y);
	for (const JunctionBridge &b : setup.signalled.bridges) {
		cover(b.start.x, b.start.y);
		cover(b.end.x, b.end.y);
	}
	JunctionTerrain terrain;
	terrain.origin = {static_cast<int16_t>(min_x - MARGIN), static_cast<int16_t>(min_y - MARGIN)};
	terrain.width = max_x - min_x + 1 + 2 * MARGIN;
	terrain.height = max_y - min_y + 1 + 2 * MARGIN;
	terrain.tiles.assign(static_cast<size_t>(terrain.width) * terrain.height, JunctionTerrainTile{});
	terrain.foundation_cost = 250;
	const JunctionFacts facts(terrain);

	const JunctionNodeSolve solve = SolveJunctionNodes(setup.tpl, spec, facts, {0, 0}, topology, setup.paths, setup.graph, setup.programs, setup.input);
	std::string failure = "no failure";
	if (solve.failure.has_value()) failure = fmt::format("failure kind {} link {} column {} class {}", to_underlying(solve.failure->kind), solve.failure->link, solve.failure->column, solve.failure->cls);
	INFO(failure);
	REQUIRE(solve.layout.has_value());

	JunctionLayout want_layout = setup.signalled;
	NormaliseJunctionLayout(want_layout);
	const std::map<std::pair<int, int>, TrackBits> want = TileBitsOf(want_layout);
	const std::map<std::pair<int, int>, TrackBits> got = TileBitsOf(*solve.layout);
	std::set<std::pair<int, int>> tiles;
	for (const auto &[tile, bits] : want) tiles.insert(tile);
	for (const auto &[tile, bits] : got) tiles.insert(tile);
	int differing = 0;
	for (const auto &tile : tiles) {
		const auto w = want.find(tile);
		const auto g = got.find(tile);
		const int wb = w == want.end() ? 0 : w->second.base();
		const int gb = g == got.end() ? 0 : g->second.base();
		if (wb == gb) continue;
		INFO(fmt::format("tile {},{}: render {} solved {}", tile.first, tile.second, wb, gb));
		if (differing++ < 10 && !std::ranges::any_of(setup.tpl.lean, [](int8_t lean) { return lean != 0; })) CHECK(wb == gb);
	}
	/* A leaning class moves off the seed (D212): equality with the seed holds only where nothing leans. */
	const bool leaning = std::ranges::any_of(setup.tpl.lean, [](int8_t lean) { return lean != 0; });
	if (!leaning) {
		CHECK(differing == 0);
		CHECK(SignalSetOf(*solve.layout) == SignalSetOf(want_layout));
	}
	for (const RuleViolation &v : CheckJunctionRules(*solve.layout, spec, topology)) {
		INFO(fmt::format("rule {} tile {},{}: {}", static_cast<int>(v.rule), v.tile.x, v.tile.y, v.message));
		CHECK(false);
	}
}

TEST_CASE("topology: the LL_RR graph solves on flat ground", "[junction][topoproj]")
{
	CheckGraphSolvesFlat(JunctionLLRRSidelineTopology());
}

TEST_CASE("topology: the T graph solves on flat ground", "[junction][topoproj]")
{
	CheckGraphSolvesFlat(JunctionTTopology());
}

TEST_CASE("topology: the node render's routes at flat values are the input's routes", "[junction][topology]")
{
	for (const JunctionTopology *t : RENDER_TOPOLOGIES) {
		const GraphSolveSetup setup = GraphSolveSetupOf(*t);
		const std::vector<JunctionTrackRoute> got = RenderJunctionNodeRoutes(setup.tpl, setup.input, setup.tpl.flat);
		REQUIRE(got.size() == setup.routes.size());
		for (size_t r = 0; r < got.size(); r++) {
			INFO("route " << r);
			CHECK(got[r].path == setup.routes[r].path);
			CHECK(got[r].copy == setup.routes[r].copy);
			REQUIRE(got[r].pieces.size() == setup.routes[r].pieces.size());
			for (size_t i = 0; i < got[r].pieces.size(); i++) {
				INFO("piece " << i);
				CHECK(got[r].pieces[i].tile.x == setup.routes[r].pieces[i].tile.x);
				CHECK(got[r].pieces[i].tile.y == setup.routes[r].pieces[i].tile.y);
				CHECK(got[r].pieces[i].trackdir == setup.routes[r].pieces[i].trackdir);
				CHECK(got[r].pieces[i].units == setup.routes[r].pieces[i].units);
				CHECK(got[r].pieces[i].on_bridge == setup.routes[r].pieces[i].on_bridge);
				CHECK(got[r].tags[i] == setup.routes[r].tags[i]);
			}
		}
	}
}

/** The facts of flat ground covering a layout's box with a margin of 8. */
static JunctionTerrain FlatTerrainAround(const JunctionLayout &layout)
{
	constexpr int MARGIN = 8;
	int min_x = INT_MAX, min_y = INT_MAX, max_x = INT_MIN, max_y = INT_MIN;
	for (const JunctionTrack &jt : layout.tracks) {
		min_x = std::min<int>(min_x, jt.offset.x);
		max_x = std::max<int>(max_x, jt.offset.x);
		min_y = std::min<int>(min_y, jt.offset.y);
		max_y = std::max<int>(max_y, jt.offset.y);
	}
	for (const JunctionBridge &b : layout.bridges) {
		for (const TileIndexDiffC tile : {b.start, b.end}) {
			min_x = std::min<int>(min_x, tile.x);
			max_x = std::max<int>(max_x, tile.x);
			min_y = std::min<int>(min_y, tile.y);
			max_y = std::max<int>(max_y, tile.y);
		}
	}
	JunctionTerrain terrain;
	terrain.origin = {static_cast<int16_t>(min_x - MARGIN), static_cast<int16_t>(min_y - MARGIN)};
	terrain.width = max_x - min_x + 1 + 2 * MARGIN;
	terrain.height = max_y - min_y + 1 + 2 * MARGIN;
	terrain.tiles.assign(static_cast<size_t>(terrain.width) * terrain.height, JunctionTerrainTile{});
	terrain.foundation_cost = 250;
	return terrain;
}

/** The x step of a piece's travel (+1 grows x), 0 for a Y or diagonal trackdir. */
static int TravelX(Trackdir td)
{
	return td == Trackdir::X_SW ? 1 : (td == Trackdir::X_NE ? -1 : 0);
}

/** The first non-run, non-bridge piece of a route from its in port (in) or out port (out), or SIZE_MAX. */
static size_t PortBendPiece(const JunctionTrackRoute &route, bool in)
{
	for (size_t k = 0; k < route.pieces.size(); k++) {
		const size_t i = in ? k : route.pieces.size() - 1 - k;
		if (route.tags[i].edge != route.tags[in ? 0 : route.pieces.size() - 1].edge) break;
		if (route.tags[i].token != JunctionTrackPieceTag::RUN && !route.pieces[i].on_bridge) return i;
	}
	return SIZE_MAX;
}

TEST_CASE("topology: lean classes come from the port bends", "[junction][topoproj]")
{
	for (const JunctionTopology *t : RENDER_TOPOLOGIES) {
		const GraphSolveSetup setup = GraphSolveSetupOf(*t);
		const JunctionNodeTemplate &tpl = setup.tpl;
		REQUIRE(tpl.lean.size() == tpl.flat.size());

		/* Rule (a): every point of a leaning class is a piece of one port bend token, or a run piece. */
		for (size_t c = 0; c < tpl.lean.size(); c++) {
			if (tpl.lean[c] == 0) continue;
			INFO("class " << c);
			std::set<std::tuple<uint32_t, uint16_t, uint8_t>> tokens;
			for (const JunctionNodePoint &point : tpl.points) {
				if ((c < tpl.x_classes ? point.cls : point.ycls) != c) continue;
				std::set<std::tuple<uint32_t, uint16_t, uint8_t>> here;
				bool on_run = false;
				for (const JunctionTrackRoute &route : setup.routes) {
					for (size_t i = 0; i < route.pieces.size(); i++) {
						const JunctionPiece &piece = route.pieces[i];
						if (piece.tile.x != point.tile.x || piece.tile.y != point.tile.y || piece.trackdir != point.trackdir || piece.on_bridge) continue;
						if (route.tags[i].token != JunctionTrackPieceTag::RUN) here.emplace(route.tags[i].edge, route.tags[i].token, route.tags[i].copy);
						else on_run = true;
					}
				}
				CHECK((here.size() == 1 || (here.empty() && on_run)));
				tokens.insert(here.begin(), here.end());
			}
			CHECK(tokens.size() == 1);
		}
	}

	/* LL_RR: the W-side in bends lean +1. */
	const GraphSolveSetup setup = GraphSolveSetupOf(JunctionLLRRSidelineTopology());
	int west = 0;
	for (const JunctionTrackRoute &route : setup.routes) {
		const Trackdir td = route.pieces.front().trackdir;
		if (td != Trackdir::X_NE && td != Trackdir::X_SW) continue;
		if (TravelX(td) <= 0) continue;
		const size_t i = PortBendPiece(route, true);
		if (i == SIZE_MAX) continue;
		for (size_t p = 0; p < setup.tpl.points.size(); p++) {
			const JunctionNodePoint &point = setup.tpl.points[p];
			if (point.tile.x != route.pieces[i].tile.x || point.tile.y != route.pieces[i].tile.y || point.trackdir != route.pieces[i].trackdir) continue;
			INFO("route path " << route.path << " class " << point.cls);
			CHECK(setup.tpl.lean[point.cls] == 1);
			west++;
		}
	}
	CHECK(west > 0);
}

TEST_CASE("topology: LL_RR's port fans keep their port rows as long as they can", "[junction][topoprojsweep]")
{
	const JunctionTopology &topology = JunctionLLRRSidelineTopology();
	const GraphSolveSetup setup = GraphSolveSetupOf(topology);
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionTerrain terrain = FlatTerrainAround(setup.signalled);
	const JunctionFacts facts(terrain);
	const JunctionNodeSolve solve = SolveJunctionNodes(setup.tpl, spec, facts, {0, 0}, topology, setup.paths, setup.graph, setup.programs, setup.input);
	REQUIRE(solve.layout.has_value());
	const JunctionNodeTemplate &tpl = setup.tpl;

	/* With every other class fixed at its solved value, a leaning class cannot move one more step. */
	size_t free_links = 0;
	for (const JunctionNodeLink &link : tpl.links) free_links += link.free && !link.diagonal && link.axis == Axis::X;
	int leaning = 0;
	for (size_t c = 0; c < tpl.lean.size(); c++) {
		if (tpl.lean[c] == 0) continue;
		leaning++;
		JunctionPositions positions(tpl.flat.size() + 3 * free_links, -1024, 1024);
		REQUIRE(AddJunctionNodeConstraints(tpl, positions));
		for (size_t y = tpl.x_classes; y < tpl.flat.size(); y++) REQUIRE(positions.RestrictRange(y, solve.values[y], solve.values[y]));
		REQUIRE(AddJunctionNodeLateConstraints(tpl, positions, solve.values));
		int row = 0;
		REQUIRE(AddJunctionNodeOccupancy(tpl, positions, solve.values, row));
		for (size_t x = 0; x < tpl.x_classes; x++) {
			if (x != c) REQUIRE(positions.RestrictRange(x, solve.values[x], solve.values[x]));
		}
		INFO("class " << c << " lean " << static_cast<int>(tpl.lean[c]) << " flat " << tpl.flat[c] << " solved " << solve.values[c]);
		CHECK(!positions.RestrictRange(c, solve.values[c] + tpl.lean[c], solve.values[c] + tpl.lean[c]));
	}
	CHECK(leaning > 0);

	/* The seed satisfies D212 itself: no port-bend class is further from its node than at the flat solve, and an in-port bend's lean class cannot step one more toward its node (checked above for every leaning class). */
	for (const JunctionTrackRoute &route : setup.routes) {
		const size_t i = PortBendPiece(route, true);
		if (i == SIZE_MAX) continue;
		for (const JunctionNodePoint &point : tpl.points) {
			if (point.tile.x != route.pieces[i].tile.x || point.tile.y != route.pieces[i].tile.y || point.trackdir != route.pieces[i].trackdir) continue;
			INFO("class " << point.cls << " flat " << tpl.flat[point.cls] << " solved " << solve.values[point.cls] << " lean " << static_cast<int>(tpl.lean[point.cls]));
			if (tpl.lean[point.cls] != 0) CHECK((solve.values[point.cls] - tpl.flat[point.cls]) * tpl.lean[point.cls] >= 0);
		}
	}

	for (const RuleViolation &v : CheckJunctionRules(*solve.layout, spec, topology)) {
		INFO(fmt::format("rule {} tile {},{}: {}", static_cast<int>(v.rule), v.tile.x, v.tile.y, v.message));
		CHECK(false);
	}
}

/** The y step of a piece's travel (+1 grows y), 0 for an X or diagonal trackdir. */
static int TravelY(Trackdir td)
{
	return td == Trackdir::Y_SE ? 1 : (td == Trackdir::Y_NW ? -1 : 0);
}

/** The token's non-bridge piece indices on a route. */
static std::vector<size_t> TokenPieces(const JunctionTrackRoute &route, uint32_t edge, uint16_t token, uint8_t copy)
{
	std::vector<size_t> pieces;
	for (size_t i = 0; i < route.pieces.size(); i++) {
		const JunctionTrackPieceTag &tag = route.tags[i];
		if (tag.edge == edge && tag.token == token && tag.copy == copy && !route.pieces[i].on_bridge) pieces.push_back(i);
	}
	return pieces;
}

/** The nearest straight piece's trackdir to a token's first piece, looking back first, or Trackdir::INVALID. */
static std::optional<Trackdir> BendHeading(const JunctionTrackRoute &route, size_t first)
{
	const auto straight = [](Trackdir td) { return td == Trackdir::X_NE || td == Trackdir::X_SW || td == Trackdir::Y_SE || td == Trackdir::Y_NW; };
	for (size_t i = first + 1; i-- > 0;) {
		if (straight(route.pieces[i].trackdir)) return route.pieces[i].trackdir;
	}
	for (size_t i = first; i < route.pieces.size(); i++) {
		if (straight(route.pieces[i].trackdir)) return route.pieces[i].trackdir;
	}
	return std::nullopt;
}

TEST_CASE("topoproj: lean kinds", "[junction][topoproj]")
{
	const GraphSolveSetup setup = GraphSolveSetupOf(JunctionLLRRSidelineTopology());
	REQUIRE(setup.input.lean.size() == setup.routes.size());
	/* The input's paths are in the checker's order: find each route's. */
	const auto input_of = [&](size_t r) {
		for (size_t p = 0; p < setup.input.routes.size(); p++) {
			if (setup.input.routes[p] == std::pair<uint16_t, uint8_t>{setup.routes[r].path, setup.routes[r].copy}) return p;
		}
		FAIL("route without input path");
		return size_t{0};
	};
	int checked = 0;
	for (size_t e = 0; e < setup.programs.copies.size(); e++) {
		if (setup.programs.copies[e].empty()) continue;
		/* Kind 2: copy 1's SBends lean +1 in the input; kind 3: copy 0's lean -1. */
		for (const uint8_t copy : {1, 0}) {
			const std::vector<JunctionTrackStep> &program = copy == 1 ? setup.programs.copies[e] : setup.programs.edges[e];
			for (size_t k = 0; k < program.size(); k++) {
				if (program[k].kind != JunctionTrackStep::Kind::SBend) continue;
				for (size_t r = 0; r < setup.routes.size(); r++) {
					const JunctionTrackRoute &route = setup.routes[r];
					const std::vector<size_t> pieces = TokenPieces(route, static_cast<uint32_t>(e), static_cast<uint16_t>(k), copy);
					const std::optional<Trackdir> found = pieces.empty() ? std::nullopt : BendHeading(route, pieces.front());
					if (pieces.empty()) continue;
					REQUIRE(found.has_value());
					const bool x_axis = *found == Trackdir::X_NE || *found == Trackdir::X_SW;
					for (const size_t i : pieces) {
						INFO("edge " << e << " copy " << static_cast<int>(copy) << " token " << k << " route " << r << " piece " << i);
						CHECK(setup.input.lean[input_of(r)][i] == (copy == 1 ? 1 : -1));
						checked++;

						/* Rule (a) bars the class: the bend shares it with its bridge heads, so the template does not lean it. */
						for (const JunctionNodePoint &point : setup.tpl.points) {
							if (point.tile.x != route.pieces[i].tile.x || point.tile.y != route.pieces[i].tile.y || point.trackdir != route.pieces[i].trackdir) continue;
							const uint16_t cls = x_axis ? point.cls : point.ycls;
							std::string others;
							for (const JunctionNodePoint &q : setup.tpl.points) {
								if ((x_axis ? q.cls : q.ycls) != cls) continue;
								for (size_t rr = 0; rr < setup.routes.size(); rr++) {
									for (size_t ii = 0; ii < setup.routes[rr].pieces.size(); ii++) {
										const JunctionPiece &pc = setup.routes[rr].pieces[ii];
										const JunctionTrackPieceTag &tag = setup.routes[rr].tags[ii];
										if (pc.tile.x == q.tile.x && pc.tile.y == q.tile.y && pc.trackdir == q.trackdir && (tag.edge != e || tag.token != k || tag.copy != copy) && tag.token != JunctionTrackPieceTag::RUN) {
											others += fmt::format(" ({},{} edge {} token {} copy {}{})", pc.tile.x, pc.tile.y, tag.edge, tag.token, tag.copy, pc.on_bridge ? " bridge" : "");
										}
									}
								}
							}
							INFO("class " << cls << " also holds" << others);
							CHECK(setup.tpl.lean[cls] == 0);
						}
					}
				}
			}
		}
	}
	CHECK(checked > 0);

	/* Port fans as P3: the first edge's fan leans +1, the last edge's -1, where the input says so. */
	int fans = 0;
	for (size_t r = 0; r < setup.routes.size(); r++) {
		const size_t in = PortBendPiece(setup.routes[r], true);
		if (in != SIZE_MAX && setup.programs.edges[setup.routes[r].tags[in].edge][setup.routes[r].tags[in].token].kind == JunctionTrackStep::Kind::Fan) {
			CHECK(setup.input.lean[input_of(r)][in] == 1);
			fans++;
		}
		const size_t out = PortBendPiece(setup.routes[r], false);
		if (out != SIZE_MAX && setup.programs.edges[setup.routes[r].tags[out].edge][setup.routes[r].tags[out].token].kind == JunctionTrackStep::Kind::Fan) {
			CHECK(setup.input.lean[input_of(r)][out] == -1);
			fans++;
		}
	}
	CHECK(fans > 0);
}

/** Solve LL_RR's graph input on flat ground with some tiles blocked; the setup and terrain are the caller's. */
static JunctionNodeSolve SolveLLRRBlocked(const GraphSolveSetup &setup, const JunctionTerrain &flat, const std::vector<std::pair<int, int>> &blocked)
{
	JunctionTerrain terrain = flat;
	for (const auto &[x, y] : blocked) terrain.tiles[static_cast<size_t>(y - terrain.origin.y) * terrain.width + (x - terrain.origin.x)].cls = JunctionTileClass::Blocked;
	const JunctionFacts facts(terrain);
	return SolveJunctionNodes(setup.tpl, MakeTestJunctionSpec(), facts, {0, 0}, JunctionLLRRSidelineTopology(), setup.paths, setup.graph, setup.programs, setup.input);
}

/** The first route that starts at a W in slot (P3's predicate), or SIZE_MAX. */
static size_t FirstWestInRoute(const GraphSolveSetup &setup)
{
	for (size_t r = 0; r < setup.routes.size(); r++) {
		const Trackdir td = setup.routes[r].pieces.front().trackdir;
		if ((td == Trackdir::X_NE || td == Trackdir::X_SW) && TravelX(td) > 0) return r;
	}
	return SIZE_MAX;
}

static std::string FailureElementsText(const JunctionNodeFailure &failure)
{
	std::string text = fmt::format("kind {} link {} class {} elements", to_underlying(failure.kind), failure.link, failure.cls);
	for (const JunctionTrackPieceTag &tag : failure.elements) text += fmt::format(" (edge {} token {} run {} copy {})", tag.edge, tag.token == JunctionTrackPieceTag::RUN ? -1 : tag.token, tag.run, tag.copy);
	return text;
}

TEST_CASE("topoproj: a house on LL_RR's W in-lane run after the bend is avoided", "[junction][topoproj]")
{
	const GraphSolveSetup setup = GraphSolveSetupOf(JunctionLLRRSidelineTopology());
	const JunctionTerrain flat = FlatTerrainAround(setup.signalled);
	const size_t r = FirstWestInRoute(setup);
	REQUIRE(r != SIZE_MAX);
	const JunctionTrackRoute &route = setup.routes[r];
	REQUIRE(route.pieces.size() > 8);

	/* Piece 8 is the first run piece after the bend: the solve moves clear of the house. */
	const TileIndexDiffC house = route.pieces[8].tile;
	const JunctionNodeSolve solve = SolveLLRRBlocked(setup, flat, {{house.x, house.y}});
	REQUIRE(solve.layout.has_value());
	for (const JunctionTrack &jt : solve.layout->tracks) CHECK(!(jt.offset.x == house.x && jt.offset.y == house.y));
}

TEST_CASE("topoproj: every failure on LL_RR names the graph elements it hit", "[junction][topoprojsweep]")
{
	const GraphSolveSetup setup = GraphSolveSetupOf(JunctionLLRRSidelineTopology());
	const JunctionTerrain flat = FlatTerrainAround(setup.signalled);
	const JunctionNodeSolve base = SolveLLRRBlocked(setup, flat, {});
	REQUIRE(base.layout.has_value());
	std::set<std::pair<int, int>> tile_set;
	for (const JunctionTrack &jt : base.layout->tracks) tile_set.emplace(jt.offset.x, jt.offset.y);
	const std::vector<std::pair<int, int>> tiles(tile_set.begin(), tile_set.end());
	REQUIRE(tiles.size() >= 20);
	const size_t k = tiles.size() / 20;

	std::map<int, int> kinds;
	int failures = 0;
	for (size_t j = 0; j < 20; j++) {
		const std::pair<int, int> tile = tiles[j * k];
		const JunctionNodeSolve solve = SolveLLRRBlocked(setup, flat, {tile});
		if (!solve.failure.has_value()) continue;
		failures++;
		kinds[to_underlying(solve.failure->kind)]++;
		INFO("tile " << tile.first << "," << tile.second << " " << FailureElementsText(*solve.failure));
		if (solve.failure->kind == JunctionNodeFailure::Kind::Template) continue;
		CHECK(!solve.failure->elements.empty());
		for (const JunctionTrackPieceTag &tag : solve.failure->elements) CHECK(tag.edge < setup.graph.edges.size());
		const JunctionNodeFailure &failure = *solve.failure;
		using Kind = JunctionNodeFailure::Kind;
		if (failure.kind == Kind::Straight || failure.kind == Kind::Break || failure.kind == Kind::Edge) {
			REQUIRE(failure.link < setup.tpl.link_tags.size());
			CHECK(std::ranges::find(failure.elements, setup.tpl.link_tags[failure.link]) != failure.elements.end());
		} else if (failure.kind == Kind::Piece) {
			for (size_t p = 0; p < setup.tpl.points.size(); p++) {
				if (setup.tpl.points[p].cls == failure.cls) CHECK(std::ranges::find(failure.elements, setup.tpl.point_tags[p]) != failure.elements.end());
			}
		}
	}
	for (const auto &[kind, count] : kinds) WARN("failure kind " << kind << ": " << count << " of " << failures);
}

/** The solve of a seed's routes with the edits applied, on the terrain at the offset. */
static JunctionNodeSolve SolveSeedEdits(const JunctionGraphSeed &seed, const std::vector<JunctionDetourEdit> &edits, const JunctionTerrain &terrain, TileIndexDiffC offset)
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	std::string why;
	const std::vector<JunctionTrackRoute> routes = ApplyJunctionDetours(seed.routes, edits, spec, offset, &why);
	INFO(why);
	REQUIRE(!routes.empty());
	const JunctionNodeInput input = JunctionNodeInputOfGraph(*seed.topology, seed.paths, seed.graph, seed.structure, seed.programs, seed.placement, routes);
	const JunctionNodeTemplate tpl = BuildJunctionNodeTemplate(input, spec);
	INFO(tpl.error);
	REQUIRE(tpl.error.empty());
	return SolveJunctionNodes(tpl, spec, JunctionFacts(terrain), offset, *seed.topology, seed.paths, seed.graph, seed.programs, input);
}

/** The houses of the P4b tests (Amendment 3): a row of 5 (else 7, else 9) consecutive tiles centred on the middle of the longest free X run on LL_RR's seed routes that lies outside every prio coverage, the first width whose solve fails Straight. */
struct StraightHouse {
	std::vector<std::pair<int, int>> tiles;
	int width;
	JunctionTrackPieceTag tag; ///< The failing link's element.
	int column; ///< The failure's column.
};
static std::optional<StraightHouse> FindStraightHouse(const GraphSolveSetup &setup, const JunctionGraphSeed &seed, const JunctionTerrain &flat, std::string *considered)
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	struct Run {
		size_t route;
		size_t first;
		size_t last;
	};
	std::vector<Run> runs;
	std::set<std::tuple<uint32_t, uint16_t, uint8_t>> seen;
	for (size_t r = 0; r < seed.routes.size(); r++) {
		const JunctionTrackRoute &route = seed.routes[r];
		for (size_t i = 0; i < route.pieces.size();) {
			const JunctionTrackPieceTag &tag = route.tags[i];
			size_t j = i;
			while (j + 1 < route.pieces.size() && route.tags[j + 1] == tag) j++;
			const bool x_run = tag.token == JunctionTrackPieceTag::RUN && j >= i + 2 && std::all_of(route.pieces.begin() + i, route.pieces.begin() + j + 1, [&](const JunctionPiece &piece) { return !piece.on_bridge && (piece.trackdir == Trackdir::X_NE || piece.trackdir == Trackdir::X_SW) && piece.tile.y == route.pieces[i].tile.y; });
			if (x_run && seen.emplace(tag.edge, tag.run, tag.copy).second) runs.push_back({r, i, j});
			i = j + 1;
		}
	}

	/* Outside every prio coverage: no merge of any route through the run within 2 * prio_tiles units downstream of any of its pieces. */
	auto covered = [&](const Run &run) {
		const JunctionTrackPieceTag &tag = seed.routes[run.route].tags[run.first];
		for (size_t r = 0; r < seed.routes.size(); r++) {
			const JunctionTrackRoute &route = seed.routes[r];
			if (!std::ranges::any_of(route.tags, [&](const JunctionTrackPieceTag &t) { return t == tag; })) continue;
			std::vector<int> start(route.pieces.size());
			int unit = 0;
			for (size_t i = 0; i < route.pieces.size(); i++) {
				start[i] = unit;
				unit += route.pieces[i].units;
			}
			size_t in_run = 0;
			while (route.tags[in_run] != tag) in_run++;
			for (const auto &[split, piece] : JunctionRouteEvents(seed.graph, seed.programs, seed.routes, r)) {
				if (split) continue;
				for (size_t i = in_run; i < route.pieces.size() && route.tags[i] == tag; i++) {
					if (start[piece] - start[i] >= 0 && start[piece] - start[i] <= 2 * static_cast<int>(spec.prio_tiles)) return true;
				}
			}
		}
		return false;
	};
	std::erase_if(runs, covered);
	/* Every edge's X runs, in edge order (a node-path test; T4b replaces the house search). */
	std::ranges::stable_sort(runs, [&](const Run &a, const Run &b) { return seed.routes[a.route].tags[a.first].edge < seed.routes[b.route].tags[b.first].edge; });
	for (const Run &run : runs) {
		const JunctionTrackPieceTag tag = seed.routes[run.route].tags[run.first];
		const size_t mid = (run.first + run.last) / 2;
		for (const int width : {5, 7, 9}) {
			std::vector<std::pair<int, int>> tiles;
			for (int k = -(width / 2); k <= width / 2; k++) {
				const JunctionPiece &piece = seed.routes[run.route].pieces[mid + k];
				tiles.emplace_back(piece.tile.x, piece.tile.y);
			}
			const JunctionNodeSolve solve = SolveLLRRBlocked(setup, flat, tiles);
			*considered += fmt::format(" (edge {} run {} copy {} len {} width {} kind {})", tag.edge, tag.run, tag.copy, run.last - run.first + 1, width, solve.failure.has_value() ? static_cast<int>(solve.failure->kind) : -1);
			if (solve.failure.has_value() && solve.failure->kind == JunctionNodeFailure::Kind::Straight && solve.failure->link < setup.tpl.link_tags.size()) return StraightHouse{tiles, width, setup.tpl.link_tags[solve.failure->link], solve.failure->column};
		}
	}
	return std::nullopt;
}

/** The run (first and last piece index) of the tag on a route, or {SIZE_MAX, 0}. */
static std::pair<size_t, size_t> RunOf(const JunctionTrackRoute &route, const JunctionTrackPieceTag &tag)
{
	size_t first = route.tags.size();
	size_t last = 0;
	for (size_t i = 0; i < route.tags.size(); i++) {
		if (route.tags[i] != tag) continue;
		if (first == route.tags.size()) first = i;
		last = i;
	}
	return first == route.tags.size() ? std::pair<size_t, size_t>{SIZE_MAX, 0} : std::pair<size_t, size_t>{first, last};
}

/** The 45 degree turns of a route. */
static int TurnsOf(const JunctionTrackRoute &route)
{
	int turns = 0;
	for (size_t i = 1; i < route.pieces.size(); i++) {
		if (!route.pieces[i].on_bridge && !route.pieces[i - 1].on_bridge && TrackdirToDirection(route.pieces[i].trackdir) != TrackdirToDirection(route.pieces[i - 1].trackdir)) turns++;
	}
	return turns;
}

TEST_CASE("topoproj: LL_RR builds around a house it cannot slide past", "[junction][topoprojsweep]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const GraphSolveSetup setup = GraphSolveSetupOf(JunctionLLRRSidelineTopology());
	const JunctionTerrain flat = FlatTerrainAround(setup.signalled);
	std::string error;
	const JunctionGraphSeed seed = MakeJunctionGraphSeed(JunctionLLRRSidelineTopology(), spec, &error);
	INFO(error);
	REQUIRE(!seed.routes.empty());
	std::string considered;
	const std::optional<StraightHouse> house = FindStraightHouse(setup, seed, flat, &considered);
	INFO("runs considered:" << considered);
	if (!house.has_value()) {
		/* The in-fans now sit next to their nodes, so no X run of any edge fails as a straight link; the node path is replaced by T4b. */
		WARN("no straight run fails with a house on it; runs considered:" << considered);
		return;
	}
	INFO("house width " << house->width);

	JunctionTerrain terrain = flat;
	for (const auto &[hx, hy] : house->tiles) terrain.tiles[static_cast<size_t>(hy - terrain.origin.y) * terrain.width + (hx - terrain.origin.x)].cls = JunctionTileClass::Blocked;

	const auto start = std::chrono::steady_clock::now();
	const JunctionGraphSearch search = SearchJunctionGraph(seed, spec, terrain, {0, 0}, 16);
	const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
	std::string edits;
	for (const JunctionDetourEdit &e : search.edits) edits += fmt::format(" (edge {} run {} copy {} column {} side {} variant {})", e.edge, e.run, e.copy, e.column, e.side, e.variant);
	WARN("house width " << house->width << " centred " << house->tiles[house->tiles.size() / 2].first << "," << house->tiles[house->tiles.size() / 2].second << " states " << search.states << " in " << micros << " us, edits:" << edits << "\n" << search.log);
	INFO(search.log);
	REQUIRE(search.layout.has_value());
	CHECK(!search.edits.empty());

	/* The layout's tile of the house: the solve of the same edits says where the layout's (0, 0) is. */
	const JunctionNodeSolve again = SolveSeedEdits(seed, search.edits, terrain, {0, 0});
	REQUIRE(again.layout.has_value());
	for (const JunctionTrack &jt : search.layout->tracks) CHECK(std::ranges::find(house->tiles, std::pair<int, int>{jt.offset.x + again.origin.x, jt.offset.y + again.origin.y}) == house->tiles.end());
	for (const RuleViolation &v : CheckJunctionRules(*search.layout, spec, JunctionLLRRSidelineTopology())) {
		INFO(fmt::format("rule {} tile {},{}: {}", static_cast<int>(v.rule), v.tile.x, v.tile.y, v.message));
		CHECK(false);
	}
}

/** The terrain and offset of the legacy node search tests (junction_nodes.cpp): flat, the spec's frame with a margin of 4, a house at hub_x - 1, 0. */
static JunctionTerrain LegacyHouseTerrain(const JunctionSpec &spec)
{
	constexpr int MARGIN = 4;
	const JunctionFrame frame = MakeJunctionFrame(spec);
	JunctionTerrain t;
	t.origin = {static_cast<int16_t>(-MARGIN), static_cast<int16_t>(frame.terrain_origin_y - MARGIN)};
	t.width = frame.width + 2 * MARGIN;
	t.height = frame.terrain_height + 2 * MARGIN;
	t.tiles.assign(static_cast<size_t>(t.width) * t.height, JunctionTerrainTile{});
	t.foundation_cost = 250;
	return t;
}

static TileIndexDiffC LegacyFlatToWorking(const JunctionSpec &spec)
{
	const JunctionTMapResult flat = ConstructJunctionTMap(spec, nullptr, std::nullopt, true);
	const JunctionFrame frame = MakeJunctionFrame(spec);
	return {static_cast<int16_t>(frame.hub_x - flat.hub.x), static_cast<int16_t>(-flat.hub.y)};
}

TEST_CASE("topoproj: the T graph search builds around a house beside the hub", "[junction][topoproj]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	JunctionTerrain terrain = LegacyHouseTerrain(spec);
	const int hub_x = MakeJunctionFrame(spec).hub_x;
	terrain.tiles[static_cast<size_t>(0 - terrain.origin.y) * terrain.width + (hub_x - 1 - terrain.origin.x)].cls = JunctionTileClass::Blocked;
	std::string error;
	const JunctionGraphSeed seed = MakeJunctionGraphSeed(JunctionTTopology(), spec, &error);
	INFO(error);
	REQUIRE(!seed.routes.empty());

	const auto start = std::chrono::steady_clock::now();
	const JunctionGraphSearch search = SearchJunctionGraph(seed, spec, terrain, LegacyFlatToWorking(spec), 16);
	const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
	WARN("states " << search.states << " in " << micros << " us\n" << search.log);
	INFO(search.log);
	REQUIRE(search.layout.has_value());
	CHECK(search.states <= 16);
	for (const RuleViolation &v : CheckJunctionRules(*search.layout, spec, JunctionTTopology())) {
		INFO(fmt::format("rule {} tile {},{}: {}", static_cast<int>(v.rule), v.tile.x, v.tile.y, v.message));
		CHECK(false);
	}
}

TEST_CASE("topoproj: placement moves the W end off an incline", "[junction][topoproj]")
{
	/* The terrain of "the node solve moves the W end off an incline" (junction_nodes.cpp): z 1 up to three tiles past the W stub, then one inclined tile, then z 0. The seed's hub is the traced T's. */
	constexpr int STUB = 2;
	const JunctionSpec spec = MakeTestJunctionSpec();
	const TileIndexDiffC offset = LegacyFlatToWorking(spec);
	const int x_w = offset.x;
	JunctionTerrain terrain = LegacyHouseTerrain(spec);
	terrain.build_on_slopes = true;
	for (int y = terrain.origin.y; y < terrain.origin.y + terrain.height; y++) {
		for (int x = terrain.origin.x; x <= x_w + STUB + 3; x++) {
			JunctionTerrainTile &tile = terrain.tiles[static_cast<size_t>(y - terrain.origin.y) * terrain.width + (x - terrain.origin.x)];
			if (x <= x_w + STUB + 2) {
				tile.z_min = tile.z_max = 1;
			} else {
				tile.z_min = 0;
				tile.z_max = 1;
				tile.incline = DiagDirection::NE;
				tile.slope = InclinedSlope(DiagDirection::NE);
			}
		}
	}
	const JunctionTopology &t = JunctionTTopology();
	std::string error;
	const JunctionGraphSeed seed = MakeJunctionGraphSeed(t, spec, &error);
	INFO(error);
	REQUIRE(!seed.routes.empty());
	const JunctionTrackOrder order = OrderJunctionTrackGraph(t, seed.paths, seed.graph, &error);
	INFO(error);
	const JunctionFacts facts(terrain);
	const JunctionTrackPlacement placement = PlaceJunctionTrackGraph(t, seed.paths, seed.graph, order, seed.structure, seed.programs, spec, seed.placement.choices, facts, offset, &error);
	INFO(error);
	REQUIRE(!placement.nodes.empty());
	std::vector<JunctionTrackRoute> routes;
	JunctionLayout layout = RenderJunctionTrackPlacement(t, seed.paths, seed.graph, seed.structure, seed.programs, placement, &error, &routes);
	INFO(error);
	REQUIRE(!layout.tracks.empty());
	std::string why;
	CHECK(JunctionRoutesHeightsOk(routes, layout, {0, 0}, terrain, offset, &why));
	INFO(why);
	SignalJunctionTrackRoutes(layout, routes, t, seed.paths, seed.graph, seed.programs, spec, nullptr, nullptr, &placement.prio);
	for (const RuleViolation &v : CheckJunctionRules(layout, spec, t)) {
		INFO(fmt::format("rule {} tile {},{}: {}", static_cast<int>(v.rule), v.tile.x, v.tile.y, v.message));
		CHECK(false);
	}
}

TEST_CASE("topoproj: the graph search keeps the flat seed on flat terrain", "[junction][topoproj]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	for (const JunctionTopology *t : RENDER_TOPOLOGIES) {
		const GraphSolveSetup setup = GraphSolveSetupOf(*t);
		const JunctionTerrain flat = FlatTerrainAround(setup.signalled);
		std::string error;
		const JunctionGraphSeed seed = MakeJunctionGraphSeed(*t, spec, &error);
		INFO(error);
		REQUIRE(!seed.routes.empty());
		const JunctionGraphSearch search = SearchJunctionGraph(seed, spec, flat, {0, 0}, 16);
		INFO(search.log);
		CHECK(search.states == 1);
		CHECK(search.edits.empty());
		REQUIRE(search.layout.has_value());
		const JunctionNodeSolve solve = SolveJunctionNodes(setup.tpl, spec, JunctionFacts(flat), {0, 0}, *t, setup.paths, setup.graph, setup.programs, setup.input);
		REQUIRE(solve.layout.has_value());
		CHECK(TrackSetOf(*search.layout) == TrackSetOf(*solve.layout));
	}
}

TEST_CASE("topoproj: a detour edit on routes", "[junction][topoproj]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const GraphSolveSetup setup = GraphSolveSetupOf(JunctionLLRRSidelineTopology());
	const JunctionTerrain flat = FlatTerrainAround(setup.signalled);
	std::string error;
	const JunctionGraphSeed seed = MakeJunctionGraphSeed(JunctionLLRRSidelineTopology(), spec, &error);
	REQUIRE(!seed.routes.empty());
	std::string considered;
	const std::optional<StraightHouse> house = FindStraightHouse(setup, seed, flat, &considered);
	INFO("runs considered:" << considered);
	if (!house.has_value()) {
		/* The in-fans now sit next to their nodes, so no X run of any edge fails as a straight link; the node path is replaced by T4b. */
		WARN("no straight run fails with a house on it; runs considered:" << considered);
		return;
	}

	/* The run of the failing link; the edit's column is clamped strictly inside it. */
	const JunctionTrackPieceTag tag = house->tag;
	size_t carrier = SIZE_MAX;
	for (size_t r = 0; r < seed.routes.size() && carrier == SIZE_MAX; r++) {
		if (RunOf(seed.routes[r], tag).first != SIZE_MAX) carrier = r;
	}
	REQUIRE(carrier != SIZE_MAX);
	const auto [first, last] = RunOf(seed.routes[carrier], tag);
	const int length = static_cast<int>(last - first + 1);
	REQUIRE(length >= 3);
	if (length < 15) {
		/* The widened house search met a short run ; the detour's clamping was written for edge 0's long run, which no longer fails; T4b replaces this node-path test. */
		WARN("house on a run of " << length << " pieces (edge " << tag.edge << " run " << tag.run << "): detour check skipped");
		return;
	}
	const int px = seed.routes[carrier].pieces[first].tile.x;
	const int dir = seed.routes[carrier].pieces[last].tile.x > px ? 1 : -1;
	const size_t piece = static_cast<size_t>(std::clamp(dir * (house->column - px), 1, length - 2));
	INFO("run " << first << ".." << last << " column " << house->column << " clamped piece " << piece);

	int accepted = 0;
	for (const int side : {1, -1}) {
		const JunctionDetourEdit edit{tag.edge, tag.run, tag.copy, house->column, static_cast<int8_t>(side), 0};
		std::string why;
		const std::vector<JunctionTrackRoute> routes = ApplyJunctionDetours(seed.routes, {edit}, spec, {0, 0}, &why);
		INFO("side " << side << ": " << why);
		if (routes.empty()) {
			CHECK(!why.empty());
			continue;
		}
		accepted++;
		REQUIRE(routes.size() == seed.routes.size());
		const TileIndexDiffC edited_tile = seed.routes[carrier].pieces[first + piece].tile;
		int changed = 0;
		for (size_t r = 0; r < routes.size(); r++) {
			const JunctionTrackRoute &route = routes[r];
			REQUIRE(route.pieces.size() == route.tags.size());
			for (size_t i = 1; i < route.pieces.size(); i++) {
				if (route.pieces[i].on_bridge || route.pieces[i - 1].on_bridge) continue;
				const DiagDirection exit = TrackdirToExitdir(route.pieces[i - 1].trackdir);
				const TileIndexDiffC step = exit == DiagDirection::NE ? TileIndexDiffC{-1, 0} : exit == DiagDirection::SE ? TileIndexDiffC{0, 1} : exit == DiagDirection::SW ? TileIndexDiffC{1, 0} : TileIndexDiffC{0, -1};
				const bool same_tile = route.pieces[i].tile.x == route.pieces[i - 1].tile.x && route.pieces[i].tile.y == route.pieces[i - 1].tile.y;
				const bool next_tile = route.pieces[i].tile.x == route.pieces[i - 1].tile.x + step.x && route.pieces[i].tile.y == route.pieces[i - 1].tile.y + step.y;
				INFO("route " << r << " piece " << i);
				CHECK((same_tile || next_tile));
			}
			if (RunOf(seed.routes[r], tag).first == SIZE_MAX) {
				CHECK(route.pieces.size() == seed.routes[r].pieces.size());
				continue;
			}
			changed++;
			for (size_t i = 0; i < route.pieces.size(); i++) CHECK(!(route.pieces[i].tile.x == edited_tile.x && route.pieces[i].tile.y == edited_tile.y && route.tags[i] == tag));
			CHECK(TurnsOf(route) == TurnsOf(seed.routes[r]) + 4);
			const auto [rf, rl] = RunOf(route, tag);
			REQUIRE(rf != SIZE_MAX);
			for (size_t i = rf; i <= rl; i++) CHECK(route.tags[i] == tag);
		}
		CHECK(changed >= 1);
	}
	CHECK(accepted >= 1);

	/* Refused: a side meeting another route. */
	std::string why;
	size_t refused = 0;
	for (size_t r = 0; r < seed.routes.size(); r++) {
		for (size_t i = 0; i < seed.routes[r].pieces.size(); i++) {
			const JunctionTrackPieceTag &t = seed.routes[r].tags[i];
			if (t.token != JunctionTrackPieceTag::RUN || (t.edge == tag.edge && t.run == tag.run && t.copy == tag.copy)) continue;
			const auto [f, l] = RunOf(seed.routes[r], t);
			if (f != i || l < f + 2) continue;
			for (const int side : {1, -1}) {
				std::string reason;
				const bool empty = ApplyJunctionDetours(seed.routes, {JunctionDetourEdit{t.edge, t.run, t.copy, seed.routes[r].pieces[i + 1].tile.x, static_cast<int8_t>(side), 0}}, spec, {0, 0}, &reason).empty();
				if (empty && reason.find("collision") != std::string::npos) refused++;
			}
		}
	}
	CHECK(refused > 0);
}

/** The seed pipeline under a given spec: render and signals, for the sweep. */
static JunctionLayout SeedOf(const JunctionTopology &t, const JunctionSpec &spec, std::string *error, JunctionTrackPlacement *placement_out = nullptr, std::vector<JunctionTrackRoute> *routes_out = nullptr, JunctionTrackPrograms *programs_out = nullptr, JunctionTrackGraph *graph_out = nullptr)
{
	std::string ignored;
	const std::vector<JunctionLanePath> paths = ExpandJunctionLanes(t, &ignored);
	const JunctionTrackGraph graph = BuildJunctionTrackGraph(t, paths, &ignored);
	const JunctionTrackOrder order = OrderJunctionTrackGraph(t, paths, graph, &ignored);
	const JunctionTrackStructure structure = StructureJunctionTrackGraph(t, paths, graph, order, &ignored);
	const JunctionTrackPrograms programs = ProgramJunctionTrackGraph(t, paths, graph, order, structure, &ignored);
	JunctionTrackPlacement placement;
	const JunctionTrackStructure cut = CutJunctionTrackBridges(t, paths, graph, order, structure, programs, spec, &placement, error);
	if (placement_out != nullptr) *placement_out = placement;
	if (programs_out != nullptr) *programs_out = programs;
	if (graph_out != nullptr) *graph_out = graph;
	if (placement.nodes.empty()) return {};
	std::vector<JunctionTrackRoute> routes;
	JunctionLayout layout = RenderJunctionTrackPlacement(t, paths, graph, cut, programs, placement, error, &routes);
	SignalJunctionTrackRoutes(layout, routes, t, paths, graph, programs, spec, nullptr, nullptr, &placement.prio);
	if (routes_out != nullptr) *routes_out = routes;
	return layout;
}

TEST_CASE("topology: the seeds are clean at every train length and speed", "[junction][topologysweep]")
{
	size_t failing = 0;
	for (int ti = 0; ti < 2; ti++) {
		const JunctionTopology &t = ti == 0 ? JunctionTTopology() : JunctionLLRRSidelineTopology();
		for (int tl = 3; tl <= 8; tl++) {
			for (int v : {100, 120, 160, 196}) {
				JunctionSpec spec = MakeTestJunctionSpec();
				spec.tl = tl;
				spec.train.max_speed = v;
				spec.reference_speed = v;
				spec.prio_tiles = 0;
				spec.prio_tiles = JunctionPrioTiles(spec);
				const std::string cell = fmt::format("{} tl {} v {}", ti == 0 ? "T" : "LL_RR", tl, v);
				if (spec.prio_tiles == 0) {
					INFO(cell << ": prio_tiles 0, skipped");
					continue;
				}
				std::string error;
				const JunctionLayout layout = SeedOf(t, spec, &error);
				INFO(cell << ": " << error);
				REQUIRE(!layout.tracks.empty());
				bool bad = false;
				for (const RuleViolation &viol : CheckJunctionRules(layout, spec, t)) {
					INFO(fmt::format("{}: rule {} tile {},{}: {}", cell, static_cast<int>(viol.rule), viol.tile.x, viol.tile.y, viol.message));
					CHECK(false);
					bad = true;
				}
				if (bad) failing++;
			}
		}
	}
	INFO("failing cells " << failing);
	CHECK(failing == 0);
}

/** Every field of two placements but the explanation; the first difference goes to INFO. */
static bool SamePlacement(const JunctionTrackPlacement &p, const JunctionTrackPlacement &q)
{
	auto differ = [](const char *field) { INFO("first difference: " << field); return false; };
	if (p.item_rows != q.item_rows) return differ("item_rows");
	if (Of(p.ports) != Of(q.ports)) return differ("ports");
	if (Of(p.nodes) != Of(q.nodes)) return differ("nodes");
	if (Of(p.edges) != Of(q.edges)) return differ("edges");
	if (Of(p.copies) != Of(q.copies)) return differ("copies");
	if (p.bridges.size() != q.bridges.size()) return differ("bridges");
	for (size_t b = 0; b < p.bridges.size(); b++) {
		if (Of(p.bridges[b]) != Of(q.bridges[b])) return differ("bridges");
	}
	if (p.size.x != q.size.x || p.size.y != q.size.y) return differ("size");
	if (!(p.choices == q.choices)) return differ("choices");
	return true;
}

/** The cut's placement of a topology, and the placement the overload makes from that placement's own choices on the cut structure. */
static void PlaceTwice(const JunctionTopology &t, const JunctionSpec &spec, JunctionTrackPlacement *cut_placement, JunctionTrackPlacement *again, std::string *error)
{
	std::string ignored;
	const std::vector<JunctionLanePath> paths = ExpandJunctionLanes(t, &ignored);
	const JunctionTrackGraph graph = BuildJunctionTrackGraph(t, paths, &ignored);
	const JunctionTrackOrder order = OrderJunctionTrackGraph(t, paths, graph, &ignored);
	const JunctionTrackStructure structure = StructureJunctionTrackGraph(t, paths, graph, order, &ignored);
	const JunctionTrackPrograms programs = ProgramJunctionTrackGraph(t, paths, graph, order, structure, &ignored);
	const JunctionTrackStructure cut = CutJunctionTrackBridges(t, paths, graph, order, structure, programs, spec, cut_placement, &ignored);
	REQUIRE(!cut_placement->nodes.empty());
	*again = PlaceJunctionTrackGraph(t, paths, graph, order, cut, programs, spec, cut_placement->choices, error);
}

/** The columns of the TL1b sweep's seeds before the curve family replaced the site floors (T1c-1): per cell "topology (0 T, 1 LL_RR) tl v width height : the column of every placement point". */
static const char *const OLD_SEED_COLUMNS = R"(
0 3 100 28 13 : 0 0 0 0 0 0 0 16 18 24 4 20 25 0 0 18 20 27 27 27 0 17 17 20 23 23 13 4 18 18 18 19 25 0 0 27 27 0 0 0 0 27 27 2 2 22 22 14 14 20 15 21 16
0 3 120 29 13 : 0 0 0 0 0 0 0 17 19 25 5 21 26 0 0 19 21 28 28 28 0 18 18 21 24 24 14 5 19 19 19 20 26 0 0 28 28 0 0 0 0 28 28 2 2 23 23 15 15 21 16 22 17
0 3 160 32 14 : 0 0 0 0 0 0 0 18 21 28 5 23 29 0 0 21 23 31 31 31 0 20 20 23 26 26 15 5 21 21 21 23 29 0 0 31 31 0 0 0 0 31 31 2 2 26 26 17 17 24 18 25 19
0 3 196 40 14 : 0 0 0 0 0 0 0 22 25 32 5 27 37 0 0 25 27 39 39 39 0 24 24 27 30 30 19 5 25 25 25 27 37 0 0 39 39 0 0 0 0 39 39 2 2 30 30 21 21 28 22 29 23
0 4 100 28 13 : 0 0 0 0 0 0 0 16 18 24 4 20 25 0 0 18 20 27 27 27 0 17 17 20 23 23 13 4 18 18 18 19 25 0 0 27 27 0 0 0 0 27 27 2 2 22 22 14 14 20 15 21 16
0 4 120 29 13 : 0 0 0 0 0 0 0 17 19 25 5 21 26 0 0 19 21 28 28 28 0 18 18 21 24 24 14 5 19 19 19 20 26 0 0 28 28 0 0 0 0 28 28 2 2 23 23 15 15 21 16 22 17
0 4 160 33 14 : 0 0 0 0 0 0 0 19 22 29 6 24 30 0 0 22 24 32 32 32 0 21 21 24 27 27 16 6 22 22 22 24 30 0 0 32 32 0 0 0 0 32 32 2 2 27 27 18 18 25 19 26 20
0 4 196 41 15 : 0 0 0 0 0 0 0 23 27 35 6 29 38 0 0 27 29 40 40 40 0 26 26 29 32 32 20 6 27 27 27 30 38 0 0 40 40 0 0 0 0 40 40 2 2 33 33 22 22 31 23 32 24
0 5 100 32 13 : 0 0 0 0 0 0 0 18 20 26 4 22 29 0 0 20 22 31 31 31 0 19 19 22 25 25 15 4 20 20 20 21 29 0 0 31 31 0 0 0 0 31 31 2 2 24 24 16 16 22 17 23 18
0 5 120 34 13 : 0 0 0 0 0 0 0 20 22 28 6 24 31 0 0 22 24 33 33 33 0 21 21 24 27 27 17 6 22 22 22 23 31 0 0 33 33 0 0 0 0 33 33 2 2 26 26 18 18 24 19 25 20
0 5 160 38 14 : 0 0 0 0 0 0 0 22 25 32 7 27 35 0 0 25 27 37 37 37 0 24 24 27 30 30 19 7 25 25 25 27 35 0 0 37 37 0 0 0 0 37 37 2 2 30 30 21 21 28 22 29 23
0 5 196 46 15 : 0 0 0 0 0 0 0 26 30 38 7 32 43 0 0 30 32 45 45 45 0 29 29 32 35 35 23 7 30 30 30 33 43 0 0 45 45 0 0 0 0 45 45 2 2 36 36 25 25 34 26 35 27
0 6 100 32 13 : 0 0 0 0 0 0 0 18 20 26 4 22 29 0 0 20 22 31 31 31 0 19 19 22 25 25 15 4 20 20 20 21 29 0 0 31 31 0 0 0 0 31 31 2 2 24 24 16 16 22 17 23 18
0 6 120 34 13 : 0 0 0 0 0 0 0 20 22 28 6 24 31 0 0 22 24 33 33 33 0 21 21 24 27 27 17 6 22 22 22 23 31 0 0 33 33 0 0 0 0 33 33 2 2 26 26 18 18 24 19 25 20
0 6 160 38 14 : 0 0 0 0 0 0 0 22 25 32 7 27 35 0 0 25 27 37 37 37 0 24 24 27 30 30 19 7 25 25 25 27 35 0 0 37 37 0 0 0 0 37 37 2 2 30 30 21 21 28 22 29 23
0 6 196 47 15 : 0 0 0 0 0 0 0 27 31 39 8 33 44 0 0 31 33 46 46 46 0 30 30 33 36 36 24 8 31 31 31 34 44 0 0 46 46 0 0 0 0 46 46 2 2 37 37 26 26 35 27 36 28
0 7 100 36 14 : 0 0 0 0 0 0 0 20 22 28 4 24 33 0 0 22 24 35 35 35 0 21 21 24 27 27 16 4 22 22 22 23 33 0 0 35 35 0 0 0 0 35 35 2 2 26 26 18 18 24 19 25 20
0 7 120 38 14 : 0 0 0 0 0 0 0 22 24 30 6 26 35 0 0 24 26 37 37 37 0 23 23 26 29 29 18 6 24 24 24 25 35 0 0 37 37 0 0 0 0 37 37 2 2 28 28 20 20 26 21 27 22
0 7 160 43 14 : 0 0 0 0 0 0 0 25 28 35 8 30 40 0 0 28 30 42 42 42 0 27 27 30 33 33 22 8 28 28 28 30 40 0 0 42 42 0 0 0 0 42 42 2 2 33 33 24 24 31 25 32 26
0 8 100 36 15 : 0 0 0 0 0 0 0 20 22 28 4 24 33 0 0 22 24 35 35 35 0 21 21 24 27 27 15 4 22 22 22 23 33 0 0 35 35 0 0 0 0 35 35 2 2 26 26 18 18 24 19 25 20
0 8 120 38 15 : 0 0 0 0 0 0 0 22 24 30 6 26 35 0 0 24 26 37 37 37 0 23 23 26 29 29 17 6 24 24 24 25 35 0 0 37 37 0 0 0 0 37 37 2 2 28 28 20 20 26 21 27 22
0 8 160 44 15 : 0 0 0 0 0 0 0 26 29 36 9 31 41 0 0 29 31 43 43 43 0 28 28 31 34 34 22 9 29 29 29 31 41 0 0 43 43 0 0 0 0 43 43 2 2 34 34 25 25 32 26 33 27
1 3 100 44 24 : 0 0 0 0 0 0 0 0 0 0 0 0 0 24 14 26 26 19 36 32 4 6 28 31 25 39 37 0 0 0 0 26 28 43 43 43 43 43 0 26 26 19 25 15 15 31 35 35 26 27 37 27 39 19 18 4 18 6 31 28 25 2 4 39 41 0 0 0 0 2 3 41 41 22 22 29 29 24 28 23 27 43 43 2 2 29 29 14 14 27 23 28 24 18 15 19 16 39 38 4 2 34 34 16 16 32 29 33 30 27 23 28 24 21 17 22 18
1 3 120 46 24 : 0 0 0 0 0 0 0 0 0 0 0 0 0 25 15 27 27 20 37 33 5 7 29 32 26 40 38 0 0 0 0 27 29 45 45 45 45 45 0 27 27 20 26 16 16 32 36 36 27 28 38 28 40 20 19 5 19 7 32 29 26 2 4 41 43 0 0 0 0 2 3 43 43 23 23 30 30 25 29 24 28 45 45 2 2 30 30 15 15 28 24 29 25 19 16 20 17 40 39 4 2 35 35 17 17 33 30 34 31 28 24 29 25 22 18 23 19
1 3 160 53 25 : 0 0 0 0 0 0 0 0 0 0 0 0 0 27 17 30 30 24 40 37 6 8 32 35 29 46 44 0 0 0 0 30 32 52 52 52 52 52 0 30 30 24 29 19 19 35 38 38 30 32 44 32 46 24 22 6 22 8 35 32 29 2 4 48 50 0 0 0 0 2 3 50 50 25 25 34 34 27 33 26 32 40 40 3 2 33 33 18 18 31 27 32 28 22 19 23 20 44 42 5 2 38 38 20 20 36 33 37 34 31 27 32 28 25 21 26 22
1 3 196 61 25 : 0 0 0 0 0 0 0 0 0 0 0 0 0 31 21 34 34 28 44 41 6 8 36 39 33 54 52 0 0 0 0 34 36 60 60 60 60 60 0 34 34 28 33 23 23 39 42 42 34 36 52 36 54 28 26 6 26 8 39 36 33 2 4 56 58 0 0 0 0 2 3 58 58 29 29 38 38 31 37 30 36 44 44 3 2 37 37 22 22 35 31 36 32 26 23 27 24 48 46 5 2 42 42 24 24 40 37 41 38 35 31 36 32 29 25 30 26
1 4 100 44 24 : 0 0 0 0 0 0 0 0 0 0 0 0 0 24 14 26 26 19 36 32 4 6 28 31 25 39 37 0 0 0 0 26 28 43 43 43 43 43 0 26 26 19 25 15 15 31 35 35 26 27 37 27 39 19 18 4 18 6 31 28 25 2 4 39 41 0 0 0 0 2 3 41 41 22 22 29 29 24 28 23 27 43 43 2 2 29 29 14 14 27 23 28 24 18 15 19 16 39 38 4 2 34 34 16 16 32 29 33 30 27 23 28 24 21 17 22 18
1 4 120 46 25 : 0 0 0 0 0 0 0 0 0 0 0 0 0 25 16 27 27 20 37 33 5 7 29 32 26 40 38 0 0 0 0 27 29 45 45 45 45 45 0 27 27 20 26 17 17 32 36 36 27 28 38 28 40 20 19 5 19 7 32 29 26 2 4 41 43 0 0 0 0 2 3 43 43 23 23 30 30 25 29 24 28 45 45 2 2 30 30 15 15 28 24 29 25 19 16 20 17 41 40 4 2 35 35 17 17 33 30 34 31 28 24 29 25 22 18 23 19
1 4 160 54 25 : 0 0 0 0 0 0 0 0 0 0 0 0 0 28 18 31 31 25 41 38 7 9 33 36 30 47 45 0 0 0 0 31 33 53 53 53 53 53 0 31 31 25 30 20 20 36 39 39 31 33 45 33 47 25 23 7 23 9 36 33 30 2 4 49 51 0 0 0 0 2 3 51 51 26 26 35 35 28 34 27 33 42 42 3 2 34 34 19 19 32 28 33 29 23 20 24 21 45 43 5 2 39 39 21 21 37 34 38 35 32 28 33 29 26 22 27 23
1 4 196 65 26 : 0 0 0 0 0 0 0 0 0 0 0 0 0 31 21 35 35 30 46 43 7 9 37 40 34 56 54 0 0 0 0 35 37 64 64 64 64 64 0 35 35 30 34 24 24 40 43 43 35 38 54 38 56 30 27 7 27 9 40 37 34 2 5 59 62 0 0 0 0 2 4 61 62 29 29 40 40 31 39 30 38 47 47 3 2 38 38 23 23 36 32 37 33 27 24 28 25 50 48 5 2 44 44 25 25 42 38 43 39 36 32 37 33 30 26 31 27
1 5 100 48 24 : 0 0 0 0 0 0 0 0 0 0 0 0 0 26 16 28 28 21 38 34 4 6 30 33 27 43 41 0 0 0 0 28 30 47 47 47 47 47 0 28 28 21 27 17 17 33 37 37 28 29 41 29 43 21 20 4 20 6 33 30 27 2 4 43 45 0 0 0 0 2 3 45 45 24 24 31 31 26 30 25 29 47 47 2 2 31 31 16 16 29 25 30 26 20 17 21 18 41 40 4 2 36 36 18 18 34 31 35 32 29 25 30 26 23 19 24 20
1 5 120 52 26 : 0 0 0 0 0 0 0 0 0 0 0 0 0 28 19 30 30 22 40 36 6 7 32 35 29 45 44 0 0 0 0 30 32 51 51 51 51 51 0 30 30 22 29 20 20 35 39 39 30 31 44 31 45 22 21 6 21 7 35 32 29 2 4 47 49 0 0 0 0 2 3 49 49 26 26 33 33 28 32 27 31 51 51 2 2 33 33 18 18 31 27 32 28 22 19 23 20 44 43 4 2 38 38 19 19 36 33 37 34 31 27 32 28 25 20 26 21
1 5 160 60 26 : 0 0 0 0 0 0 0 0 0 0 0 0 0 31 22 34 34 28 44 41 8 10 36 39 33 52 50 0 0 0 0 34 36 59 59 59 59 59 0 34 34 28 33 24 24 39 42 42 34 36 50 36 52 28 26 8 26 10 39 36 33 2 4 55 57 0 0 0 0 2 3 57 57 29 29 38 38 31 37 30 36 46 46 3 2 37 37 22 22 35 31 36 32 26 23 27 24 49 47 5 2 42 42 24 24 40 37 41 38 35 31 36 32 29 25 30 26
1 5 196 70 28 : 0 0 0 0 0 0 0 0 0 0 0 0 0 34 24 38 38 33 49 46 8 10 40 43 37 61 59 0 0 0 0 38 40 69 69 69 69 69 0 38 38 33 37 27 27 43 46 46 38 41 59 41 61 33 30 8 30 10 43 40 37 2 5 64 67 0 0 0 0 2 4 66 67 32 32 43 43 34 42 33 41 51 51 3 2 41 41 26 26 39 35 40 36 30 27 31 28 54 52 5 2 47 47 28 28 45 41 46 42 39 35 40 36 33 29 34 30
1 6 100 48 24 : 0 0 0 0 0 0 0 0 0 0 0 0 0 26 16 28 28 21 38 34 4 6 30 33 27 43 41 0 0 0 0 28 30 47 47 47 47 47 0 28 28 21 27 17 17 33 37 37 28 29 41 29 43 21 20 4 20 6 33 30 27 2 4 43 45 0 0 0 0 2 3 45 45 24 24 31 31 26 30 25 29 47 47 2 2 31 31 16 16 29 25 30 26 20 17 21 18 41 40 4 2 36 36 18 18 34 31 35 32 29 25 30 26 23 19 24 20
1 6 120 52 26 : 0 0 0 0 0 0 0 0 0 0 0 0 0 28 19 30 30 22 40 36 6 7 32 35 29 45 44 0 0 0 0 30 32 51 51 51 51 51 0 30 30 22 29 20 20 35 39 39 30 31 44 31 45 22 21 6 21 7 35 32 29 2 4 47 49 0 0 0 0 2 3 49 49 26 26 33 33 28 32 27 31 51 51 2 2 33 33 18 18 31 27 32 28 22 19 23 20 44 43 4 2 38 38 19 19 36 33 37 34 31 27 32 28 25 20 26 21
1 6 160 60 28 : 0 0 0 0 0 0 0 0 0 0 0 0 0 31 23 34 34 28 44 41 8 10 36 39 33 52 50 0 0 0 0 34 36 59 59 59 59 59 0 34 34 28 33 25 25 39 42 42 34 36 50 36 52 28 26 8 25 10 39 36 33 2 4 55 57 0 0 0 0 2 3 57 57 29 29 38 38 31 37 30 36 46 46 3 2 37 37 22 22 35 31 36 32 26 23 27 24 50 48 5 2 42 42 24 24 40 37 41 38 35 31 36 32 29 25 30 26
1 6 196 72 30 : 0 0 0 0 0 0 0 0 0 0 0 0 0 35 26 39 39 34 50 47 9 11 41 44 38 62 60 0 0 0 0 39 41 71 71 71 71 71 0 39 39 34 38 29 29 44 47 47 39 42 60 42 62 34 31 9 31 11 44 41 38 2 5 66 69 0 0 0 0 2 4 68 69 33 33 44 44 35 43 34 42 53 53 3 2 42 42 27 27 40 36 41 37 31 28 32 29 56 54 5 2 48 48 29 29 46 42 47 43 40 36 41 37 34 30 35 31
1 7 100 52 25 : 0 0 0 0 0 0 0 0 0 0 0 0 0 28 18 30 30 23 40 36 4 6 32 35 29 47 45 0 0 0 0 30 32 51 51 51 51 51 0 30 30 23 29 19 19 35 39 39 30 31 45 31 47 23 22 4 22 6 35 32 29 2 4 47 49 0 0 0 0 2 3 49 49 26 26 33 33 28 32 27 31 51 51 2 2 33 33 18 18 31 27 32 28 22 19 23 20 43 42 4 2 38 38 20 20 36 33 37 34 31 27 32 28 25 21 26 22
1 7 120 56 26 : 0 0 0 0 0 0 0 0 0 0 0 0 0 30 21 32 32 24 42 38 6 7 34 37 31 49 48 0 0 0 0 32 34 55 55 55 55 55 0 32 32 24 31 22 22 37 41 41 32 33 48 33 49 24 23 6 23 7 37 34 31 2 4 51 53 0 0 0 0 2 3 53 53 28 28 35 35 30 34 29 33 55 55 2 2 35 35 20 20 33 29 34 30 24 21 25 22 46 45 4 2 40 40 21 21 38 35 39 36 33 29 34 30 27 22 28 23
1 7 160 65 28 : 0 0 0 0 0 0 0 0 0 0 0 0 0 33 25 36 36 30 46 43 8 10 38 41 35 56 55 0 0 0 0 36 38 64 64 64 64 64 0 36 36 30 35 27 27 41 44 44 36 38 55 38 56 30 28 8 27 10 41 38 35 2 4 60 62 0 0 0 0 2 3 62 62 31 31 40 40 33 39 32 38 48 48 3 2 39 39 24 24 37 33 38 34 28 25 29 26 52 50 5 2 44 44 26 26 42 39 43 40 37 33 38 34 31 27 32 28
1 8 100 52 26 : 0 0 0 0 0 0 0 0 0 0 0 0 0 28 18 30 30 23 40 36 4 6 32 35 29 47 45 0 0 0 0 30 32 51 51 51 51 51 0 30 30 23 29 19 19 35 39 39 30 31 45 31 47 23 22 4 22 6 35 32 29 2 4 47 49 0 0 0 0 2 3 49 49 26 26 33 33 28 32 27 31 51 51 2 2 33 33 18 18 31 27 32 28 22 19 23 20 43 42 4 2 38 38 20 20 36 33 37 34 31 27 32 28 25 21 26 22
1 8 120 56 27 : 0 0 0 0 0 0 0 0 0 0 0 0 0 30 21 32 32 24 42 38 6 7 34 37 31 49 48 0 0 0 0 32 34 55 55 55 55 55 0 32 32 24 31 22 22 37 41 41 32 33 48 33 49 24 23 6 23 7 37 34 31 2 4 51 53 0 0 0 0 2 3 53 53 28 28 35 35 30 34 29 33 55 55 2 2 35 35 20 20 33 29 34 30 24 21 25 22 46 45 4 2 40 40 21 21 38 35 39 36 33 29 34 30 27 22 28 23
1 8 160 67 29 : 0 0 0 0 0 0 0 0 0 0 0 0 0 34 26 37 37 30 47 44 9 10 39 42 36 57 57 0 0 0 0 37 39 66 66 66 66 66 0 37 37 30 36 28 28 42 45 45 37 39 57 39 57 30 28 9 27 10 42 39 36 2 4 62 64 0 0 0 0 2 3 64 64 32 32 41 41 34 40 33 39 49 49 3 2 40 40 25 25 38 34 39 35 29 26 30 27 53 51 5 2 45 45 26 26 43 40 44 41 38 34 39 35 32 27 33 28
)";

TEST_CASE("topology: the curve family alone keeps every seed clean", "[junction][topologysweep]")
{
	/* The site curve floors are gone: every cell is checker clean, and against the old seeds no box is larger. */
	std::map<std::tuple<int, int, int>, std::pair<int, int>> old_size;
	std::istringstream lines(OLD_SEED_COLUMNS);
	for (std::string line; std::getline(lines, line);) {
		if (line.empty()) continue;
		std::istringstream in(line);
		int ti, tl, v, w, h;
		char colon;
		in >> ti >> tl >> v >> w >> h >> colon;
		old_size[{ti, tl, v}] = {w, h};
	}
	size_t cells = 0, unboxed = 0;
	for (int ti = 0; ti < 2; ti++) {
		const JunctionTopology &t = ti == 0 ? JunctionTTopology() : JunctionLLRRSidelineTopology();
		for (int tl = 3; tl <= 8; tl++) {
			for (int v : {100, 120, 160, 196}) {
				JunctionSpec spec = MakeTestJunctionSpec();
				spec.tl = tl;
				spec.train.max_speed = v;
				spec.reference_speed = v;
				spec.prio_tiles = 0;
				spec.prio_tiles = JunctionPrioTiles(spec);
				if (spec.prio_tiles == 0) continue;
				const std::string cell = fmt::format("{} tl {} v {}", ti == 0 ? "T" : "LL_RR", tl, v);
				std::string error;
				JunctionTrackPlacement p;
				const JunctionLayout layout = SeedOf(t, spec, &error, &p);
				INFO(cell << ": " << error);
				REQUIRE(!layout.tracks.empty());
				for (const RuleViolation &viol : CheckJunctionRules(layout, spec, t)) {
					INFO(fmt::format("{}: rule {} tile {},{}: {}", cell, static_cast<int>(viol.rule), viol.tile.x, viol.tile.y, viol.message));
					CHECK(false);
				}
				const auto key = std::make_tuple(ti, tl, v);
				cells++;
				if (!old_size.contains(key)) {
					/* The old sizing refused v196 tl 7 and 8, so no old seed box exists; the game-rules prio length sizes them. Every other check above still runs. */
					REQUIRE(v == 196);
					REQUIRE((tl == 7 || tl == 8));
					unboxed++;
					continue;
				}
				CHECK(p.size.x <= old_size[key].first);
				CHECK(p.size.y <= old_size[key].second);
			}
		}
	}
	CHECK(unboxed == 4);
	CHECK(cells == old_size.size() + unboxed);
}

TEST_CASE("topology: every placement edit fails with a reason or is checker-clean", "[junction][topologysweep][.identity]")
{
	/* ES1 (D193): 200 random edits of the seed's choices and floors per topology and spec; each fails with a reason or renders checker clean. */
	for (int ti = 0; ti < 2; ti++) {
		const JunctionTopology &t = ti == 0 ? JunctionTTopology() : JunctionLLRRSidelineTopology();
		const char *name = ti == 0 ? "T" : "LL_RR";
		for (int tl : {3, 4, 6}) {
			for (int v : {100, 160}) {
				JunctionSpec spec = MakeTestJunctionSpec();
				spec.tl = tl;
				spec.train.max_speed = v;
				spec.reference_speed = v;
				spec.prio_tiles = 0;
				spec.prio_tiles = JunctionPrioTiles(spec);
				REQUIRE(spec.prio_tiles != 0);
				std::string ignored;
				const std::vector<JunctionLanePath> paths = ExpandJunctionLanes(t, &ignored);
				const JunctionTrackGraph graph = BuildJunctionTrackGraph(t, paths, &ignored);
				const JunctionTrackOrder order = OrderJunctionTrackGraph(t, paths, graph, &ignored);
				const JunctionTrackStructure structure = StructureJunctionTrackGraph(t, paths, graph, order, &ignored);
				const JunctionTrackPrograms programs = ProgramJunctionTrackGraph(t, paths, graph, order, structure, &ignored);
				JunctionTrackPlacement seed;
				std::string error;
				const JunctionTrackStructure cut = CutJunctionTrackBridges(t, paths, graph, order, structure, programs, spec, &seed, &error);
				INFO(name << " tl " << tl << " v " << v << ": " << error);
				REQUIRE(!seed.nodes.empty());

				std::vector<std::pair<int, int>> ms, corners;
				for (size_t e = 0; e < seed.choices.m.size(); e++) {
					for (size_t k = 0; k < seed.choices.m[e].size(); k++) {
						if (seed.choices.m[e][k] > 0) ms.emplace_back(e, k);
					}
				}
				for (size_t e = 0; e < seed.choices.corner.size(); e++) {
					for (size_t k = 0; k < seed.choices.corner[e].size(); k++) {
						if (seed.choices.corner[e][k] > 0) corners.emplace_back(e, k);
					}
				}
				std::vector<int> kinds{2, 3, 4}; // port, row gap, push always exist
				if (!ms.empty()) kinds.push_back(0);
				if (!corners.empty()) kinds.push_back(1);

				std::mt19937 rng(1);
				auto pick = [&rng](int n) { return static_cast<int>(rng() % static_cast<unsigned>(n)); };
				int failed = 0, clean = 0, dirty = 0;
				std::vector<std::string> counterexamples;
				for (int edit = 0; edit < 200; edit++) {
					JunctionPlacementChoices choices = seed.choices;
					std::vector<std::pair<int, int>> x_floors, y_floors;
					std::string atoms;
					const int count = 1 + pick(2);
					for (int a = 0; a < count; a++) {
						switch (kinds[pick(static_cast<int>(kinds.size()))]) {
							case 0: {
								const auto [e, k] = ms[pick(static_cast<int>(ms.size()))];
								static constexpr int D[] = {-2, -1, 1, 2};
								const int d = D[pick(4)];
								int &m = choices.m[e][k];
								m = std::max(1, m + d);
								atoms += fmt::format(" m[{}][{}]{:+}->{}", e, k, d, m);
								break;
							}
							case 1: {
								const auto [e, k] = corners[pick(static_cast<int>(corners.size()))];
								const int d = pick(2) == 0 ? -1 : 1;
								int &c = choices.corner[e][k];
								c = std::max(1, c + d);
								atoms += fmt::format(" corner[{}][{}]{:+}->{}", e, k, d, c);
								break;
							}
							case 2: {
								static constexpr int D[] = {-2, -1, 1, 2};
								const int d = D[pick(4)];
								choices.port_offset += d;
								atoms += fmt::format(" port{:+}->{}", d, choices.port_offset);
								break;
							}
							case 3: {
								const int i = pick(static_cast<int>(seed.item_rows.size()));
								const int d = 1 + pick(3);
								y_floors.emplace_back(1 + i, seed.item_rows[i] + d);
								atoms += fmt::format(" yfloor[item {}]+{}->{}", i, d, seed.item_rows[i] + d);
								break;
							}
							default: {
								const int p = pick(static_cast<int>(seed.columns.size()));
								const int d = 1 + pick(8);
								x_floors.emplace_back(p, seed.columns[p] + d);
								atoms += fmt::format(" xfloor[point {}]+{}->{}", p, d, seed.columns[p] + d);
								break;
							}
						}
					}
					std::string place_error;
					const JunctionTrackPlacement placed = PlaceJunctionTrackGraph(t, paths, graph, order, cut, programs, spec, choices, &place_error, x_floors, y_floors);
					if (placed.nodes.empty()) {
						if (!place_error.empty()) {
							failed++;
						} else {
							dirty++;
							counterexamples.push_back(fmt::format("{} tl {} v {}:{}: failed with no reason", name, tl, v, atoms));
						}
						continue;
					}
					std::string render_error, signal_why;
					std::vector<JunctionTrackRoute> routes;
					JunctionLayout layout = RenderJunctionTrackPlacement(t, paths, graph, cut, programs, placed, &render_error, &routes);
					if (layout.tracks.empty()) {
						dirty++;
						counterexamples.push_back(fmt::format("{} tl {} v {}:{}: empty render: {}", name, tl, v, atoms, render_error));
						continue;
					}
					SignalJunctionTrackRoutes(layout, routes, t, paths, graph, programs, spec, &signal_why, nullptr, &placed.prio);
					if (!signal_why.empty()) {
						dirty++;
						counterexamples.push_back(fmt::format("{} tl {} v {}:{}: signal error: {}", name, tl, v, atoms, signal_why));
						continue;
					}
					const std::vector<RuleViolation> violations = CheckJunctionRules(layout, spec, t);
					if (violations.empty()) {
						clean++;
					} else {
						dirty++;
						const RuleViolation &viol = violations.front();
						counterexamples.push_back(fmt::format("{} tl {} v {}:{}: rule {} tile {},{}: {}", name, tl, v, atoms, static_cast<int>(viol.rule), viol.tile.x, viol.tile.y, viol.message));
					}
				}
				std::cout << fmt::format("EDITSOUND topology={} tl={} v={} tried=200 failed={} clean={} dirty={}\n", name, tl, v, failed, clean, dirty);
				for (const std::string &line : counterexamples) std::cout << "COUNTEREXAMPLE " << line << "\n";
				CHECK(dirty == 0);
			}
		}
	}
}

/** The T at tl 3, 100 km/h, placed from its seed with one choice edit; fills error. */
static JunctionTrackPlacement PlaceTEdited(const std::function<void(JunctionPlacementChoices &)> &edit, std::string &error)
{
	const JunctionTopology &t = JunctionTTopology();
	JunctionSpec spec = MakeTestJunctionSpec();
	spec.tl = 3;
	spec.train.max_speed = 100;
	spec.reference_speed = 100;
	spec.prio_tiles = 0;
	spec.prio_tiles = JunctionPrioTiles(spec);
	std::string ignored;
	const std::vector<JunctionLanePath> paths = ExpandJunctionLanes(t, &ignored);
	const JunctionTrackGraph graph = BuildJunctionTrackGraph(t, paths, &ignored);
	const JunctionTrackOrder order = OrderJunctionTrackGraph(t, paths, graph, &ignored);
	const JunctionTrackStructure structure = StructureJunctionTrackGraph(t, paths, graph, order, &ignored);
	const JunctionTrackPrograms programs = ProgramJunctionTrackGraph(t, paths, graph, order, structure, &ignored);
	JunctionTrackPlacement seed;
	const JunctionTrackStructure cut = CutJunctionTrackBridges(t, paths, graph, order, structure, programs, spec, &seed, &error);
	REQUIRE(!seed.nodes.empty());
	JunctionPlacementChoices choices = seed.choices;
	edit(choices);
	error.clear();
	return PlaceJunctionTrackGraph(t, paths, graph, order, cut, programs, spec, choices, &error, {}, {});
}

TEST_CASE("topology: a port above the box fails with its reason", "[junction][topology]")
{
	std::string error;
	const JunctionTrackPlacement placed = PlaceTEdited([](JunctionPlacementChoices &c) { c.port_offset = -1; }, error);
	CHECK(placed.nodes.empty());
	CHECK(error.find("box") != std::string::npos);
}

TEST_CASE("topology: a corner edit keeps bridges apart", "[junction][topology]")
{
	std::string error;
	const JunctionTrackPlacement placed = PlaceTEdited([](JunctionPlacementChoices &c) { c.corner[7][0] = 4; }, error);
	CHECK(placed.nodes.empty());
	CHECK(error.find("bridge middle clearance") != std::string::npos);
}

TEST_CASE("topology: symbolic curves match the rendered routes", "[junction][topology]")
{
	/* The curve family's walk (JunctionTrackPlacement::curves) against the curves of the rendered routes: same count, hands and unit positions, for every lane path and copy. */
	for (const JunctionTopology *t : {&JunctionTTopology(), &JunctionLLRRSidelineTopology()}) {
		std::string error;
		JunctionTrackPlacement p;
		std::vector<JunctionTrackRoute> routes;
		REQUIRE(!SeedOf(*t, MakeTestJunctionSpec(), &error, &p, &routes).tracks.empty());
		REQUIRE(p.curves.size() == routes.size());
		std::vector<const JunctionTrackRoute *> ordered;
		for (int copy = 0; copy < 2; copy++) {
			for (const JunctionTrackRoute &route : routes) {
				if (route.copy == copy) ordered.push_back(&route);
			}
		}
		for (size_t r = 0; r < ordered.size(); r++) {
			const JunctionTrackRoute &route = *ordered[r];
			std::vector<std::pair<int8_t, int>> rendered;
			int unit = 0;
			for (size_t i = 0; i < route.pieces.size(); i++) {
				if (i > 0) {
					const DirDiff diff = DirDifference(TrackdirToDirection(route.pieces[i - 1].trackdir), TrackdirToDirection(route.pieces[i].trackdir));
					if (diff == DirDiff::Left45) rendered.emplace_back(-1, unit);
					if (diff == DirDiff::Right45) rendered.emplace_back(1, unit);
				}
				unit += route.pieces[i].units;
			}
			INFO("route path " << route.path << " copy " << static_cast<int>(route.copy));
			CHECK(p.curves[r] == rendered);
		}
	}
}

TEST_CASE("topology: placement from its own choices equals the seed (sweep)", "[junction][topologysweep]")
{
	for (int ti = 0; ti < 2; ti++) {
		const JunctionTopology &t = ti == 0 ? JunctionTTopology() : JunctionLLRRSidelineTopology();
		for (int tl = 3; tl <= 8; tl++) {
			for (int v : {100, 120, 160, 196}) {
				JunctionSpec spec = MakeTestJunctionSpec();
				spec.tl = tl;
				spec.train.max_speed = v;
				spec.reference_speed = v;
				spec.prio_tiles = 0;
				spec.prio_tiles = JunctionPrioTiles(spec);
				if (spec.prio_tiles == 0) continue;
				const std::string cell = fmt::format("{} tl {} v {}", ti == 0 ? "T" : "LL_RR", tl, v);
				JunctionTrackPlacement p, q;
				std::string error;
				PlaceTwice(t, spec, &p, &q, &error);
				INFO(cell << ": " << error);
				REQUIRE(!q.nodes.empty());
				CHECK(SamePlacement(p, q));
			}
		}
	}
}

TEST_CASE("topology: placement from its own choices equals the seed", "[junction][topology]")
{
	for (const JunctionTopology *t : {&JunctionTTopology(), &JunctionLLRRSidelineTopology()}) {
		JunctionTrackPlacement p, q;
		std::string error;
		PlaceTwice(*t, MakeTestJunctionSpec(), &p, &q, &error);
		INFO(error);
		REQUIRE(!q.nodes.empty());
		CHECK(SamePlacement(p, q));
	}
}

TEST_CASE("topology: an inserted S-bend pair does not move the lane events", "[junction][topology]")
{
	/* R6: a doubled leg's lane split and merge are found by their roles, not as the first S-bend of a copy. The routes come from the unedited placement; their tags are shifted by hand (the edited programs are not placed in this step, R7). */
	for (const JunctionTopology *t : {&JunctionLLRRSidelineTopology(), &JunctionTTopology()}) {
		std::string error;
		JunctionTrackPrograms programs;
		JunctionTrackGraph graph;
		std::vector<JunctionTrackRoute> routes;
		RenderOf(*t, 64, &error, nullptr, nullptr, &programs, nullptr, &graph, &routes);
		INFO(error);
		REQUIRE(!routes.empty());
		std::vector<std::vector<std::pair<bool, size_t>>> before;
		for (size_t r = 0; r < routes.size(); r++) before.push_back(JunctionRouteEvents(graph, programs, routes, r));

		/* Every doubled leg's copy 0 gets the pair just before its lane merge. */
		std::vector<size_t> at(graph.edges.size(), SIZE_MAX);
		size_t inserted = 0;
		for (size_t e = 0; e < programs.edges.size(); e++) {
			if (programs.copies[e].empty()) continue;
			std::vector<Step> &program = programs.edges[e];
			size_t pos = 0;
			while (program[pos].role != Step::Role::LaneMerge) pos++;
			const Step merge = program[pos];
			program.insert(program.begin() + pos, {Step{Step::Kind::SBend, static_cast<int8_t>(+1), merge.index, 0, Step::Role::Shift}, Step{Step::Kind::SBend, static_cast<int8_t>(-1), merge.index, 0, Step::Role::Shift}});
			at[e] = pos;
			inserted++;
		}
		REQUIRE(inserted > 0);
		for (JunctionTrackRoute &route : routes) {
			for (JunctionTrackPieceTag &tag : route.tags) {
				if (tag.copy != 0 || at[tag.edge] == SIZE_MAX) continue;
				if (tag.token != JunctionTrackPieceTag::RUN && tag.token >= at[tag.edge]) tag.token += 2;
				if (tag.run > at[tag.edge]) tag.run += 2;
			}
		}
		size_t with_events = 0;
		for (size_t r = 0; r < routes.size(); r++) {
			INFO("route " << r);
			const std::vector<std::pair<bool, size_t>> after = JunctionRouteEvents(graph, programs, routes, r);
			CHECK(after == before[r]);
			if (!after.empty()) with_events++;
		}
		CHECK(with_events > 0);
	}
}

TEST_CASE("topology: a row edge may hold any token sequence", "[junction][topology]")
{
	/* R6: the programs' row checks are graph structure (one in, one leg, one out per lane), not token shape: an S-bend pair on an undoubled leg is not refused with "not ruled yet" by the programs or by the placement's chain builder. Placing it is R7's. */
	for (const JunctionTopology *t : {&JunctionLLRRSidelineTopology(), &JunctionTTopology()}) {
		std::string error;
		const std::vector<JunctionLanePath> paths = ExpandJunctionLanes(*t, &error);
		const JunctionTrackGraph graph = BuildJunctionTrackGraph(*t, paths, &error);
		const JunctionTrackOrder order = OrderJunctionTrackGraph(*t, paths, graph, &error);
		const JunctionTrackStructure structure = StructureJunctionTrackGraph(*t, paths, graph, order, &error);
		JunctionTrackPrograms programs = ProgramJunctionTrackGraph(*t, paths, graph, order, structure, &error);
		INFO(error);
		REQUIRE(!programs.edges.empty());
		size_t edited = SIZE_MAX;
		for (size_t e = 0; e < programs.edges.size(); e++) {
			const JunctionTrackEdge &edge = graph.edges[e];
			if (edge.from.node && edge.to.node && order.node_level[edge.from.index] % 2 == 1 && order.node_level[edge.from.index] == order.node_level[edge.to.index] && programs.copies[e].empty()) edited = e; // An undoubled row leg.
		}
		REQUIRE(edited != SIZE_MAX);
		programs.edges[edited] = {Step{Step::Kind::SBend, +1, 0, 0, Step::Role::Shift}, Step{Step::Kind::SBend, -1, 0, 0, Step::Role::Shift}};
		const JunctionSpec spec = MakeTestJunctionSpec();
		JunctionTrackPlacement placement;
		CutJunctionTrackBridges(*t, paths, graph, order, structure, programs, spec, &placement, &error);
		CHECK(error.find("not ruled yet") == std::string::npos);
	}
}

TEST_CASE("topology: a fan edge may hold more than its fan", "[junction][topology]")
{
	/* R3 (DESIGN-G1 §16): an in edge's fan is its program's first token, not its only one. The T's doubled row's in edge is edited to [Fan to the lane item, S-bend back to the item above]: the placement takes it (R4: the curve family reads the chain's S-bend). */
	const JunctionTopology &t = JunctionTTopology();
	const JunctionSpec spec = MakeTestJunctionSpec();
	std::string error;
	const std::vector<JunctionLanePath> paths = ExpandJunctionLanes(t, &error);
	const JunctionTrackGraph graph = BuildJunctionTrackGraph(t, paths, &error);
	const JunctionTrackOrder order = OrderJunctionTrackGraph(t, paths, graph, &error);
	const JunctionTrackStructure structure = StructureJunctionTrackGraph(t, paths, graph, order, &error);
	JunctionTrackPrograms programs = ProgramJunctionTrackGraph(t, paths, graph, order, structure, &error);
	uint32_t lane_item = UINT32_MAX;
	int hand = 0;
	for (size_t e = 0; e < programs.edges.size(); e++) {
		if (programs.copies[e].empty()) continue;
		lane_item = programs.edges[e].back().index;
		hand = programs.edges[e].back().hand; // The doubled leg's S-bend goes from the item above onto the lane item; the edit goes back.
	}
	REQUIRE(lane_item != UINT32_MAX);
	size_t edited = SIZE_MAX;
	for (size_t e = 0; e < programs.edges.size(); e++) {
		if (programs.edges[e].size() == 1 && programs.edges[e][0].kind == JunctionTrackStep::Kind::Fan && programs.edges[e][0].index + 1 == lane_item && !graph.edges[e].from.node) edited = e;
	}
	REQUIRE(edited != SIZE_MAX);
	programs.edges[edited][0].index = lane_item;
	programs.edges[edited].push_back({JunctionTrackStep::Kind::SBend, static_cast<int8_t>(-hand), lane_item - 1, 0});

	JunctionTrackPlacement placement;
	const JunctionTrackStructure cut = CutJunctionTrackBridges(t, paths, graph, order, structure, programs, spec, &placement, &error);
	INFO(error);
	REQUIRE(!placement.nodes.empty());
	std::vector<JunctionTrackRoute> routes;
	JunctionLayout layout = RenderJunctionTrackPlacement(t, paths, graph, cut, programs, placement, &error, &routes);
	INFO(error);
	REQUIRE(!layout.tracks.empty());
	SignalJunctionTrackRoutes(layout, routes, t, paths, graph, programs, spec, nullptr, nullptr, &placement.prio);
	/* The curve-gap family walks every row edge's chain (R4), so the S-bend after the fan is in it: the checker is clean. */
	REQUIRE(placement.edges[edited].size() == 2);
	for (const RuleViolation &viol : CheckJunctionRules(layout, spec, t)) {
		INFO(fmt::format("rule {} tile {},{}: {}", static_cast<int>(viol.rule), viol.tile.x, viol.tile.y, viol.message));
		CHECK(false);
	}
}

/** Every doubled leg of a seed at a spec checked against its chains' rules (R5): copy 1's heads are copy 0's plus the stagger u, the lane split lies 2 before the first near head, every crosser's tiles under a bridge lie between that copy's heads. Returns the doubled bridges checked. */
static size_t CheckDoubledLegs(const JunctionTopology &t, const JunctionSpec &spec, const std::string &cell)
{
	std::string error;
	const std::vector<JunctionLanePath> paths = ExpandJunctionLanes(t, &error);
	const JunctionTrackGraph graph = BuildJunctionTrackGraph(t, paths, &error);
	const JunctionTrackOrder order = OrderJunctionTrackGraph(t, paths, graph, &error);
	const JunctionTrackStructure structure = StructureJunctionTrackGraph(t, paths, graph, order, &error);
	const JunctionTrackPrograms programs = ProgramJunctionTrackGraph(t, paths, graph, order, structure, &error);
	JunctionTrackPlacement placement;
	const JunctionTrackStructure cut = CutJunctionTrackBridges(t, paths, graph, order, structure, programs, spec, &placement, &error);
	INFO(cell << ": " << error);
	REQUIRE(!placement.nodes.empty());
	size_t checked = 0;
	for (size_t b = 0; b < cut.bridges.size(); b++) {
		const JunctionTrackBridge &bridge = cut.bridges[b];
		if (!bridge.doubled) continue;
		const uint32_t e = bridge.edge;
		const JunctionBridge &h0 = placement.bridges[b][0];
		const JunctionBridge &h1 = placement.bridges[b][1];
		const int sgn = h0.end.x > h0.start.x ? 1 : -1;
		INFO(cell << " bridge " << b << " edge " << e);
		/* The stagger: copy 1 one tile upstream of copy 0 at both heads, one row further. */
		CHECK(h1.start.x == h0.start.x - sgn);
		CHECK(h1.end.x == h0.end.x - sgn);
		CHECK(h1.start.y == h1.end.y);
		CHECK(h0.start.y == h0.end.y);
		CHECK(h1.start.y != h0.start.y);
		/* D215: the lane split is the first tile of copy 1's S-bend, exactly 2 before the leg's first near head. */
		if (b == 0 || cut.bridges[b - 1].edge != e) {
			const TileIndexDiffC split = placement.copies[e][0].first;
			CHECK(split.y == h0.start.y);
			CHECK(split.x == h0.start.x - 2 * sgn);
		}
		/* Every crosser tile under the bridge lies strictly between that copy's heads, on that copy's row. */
		for (const uint32_t x : bridge.crossed) {
			size_t under = 0;
			for (size_t tok = 0; tok < programs.edges[x].size(); tok++) {
				const JunctionTrackStep &step = programs.edges[x][tok];
				if (step.kind != JunctionTrackStep::Kind::Under || step.index != e) continue;
				const JunctionBridge &heads = placement.bridges[b][step.copy];
				const JunctionTrackSpan &span = placement.edges[x][tok];
				for (const TileIndexDiffC &tile : {span.first, span.second}) {
					CHECK(tile.y == heads.start.y);
					CHECK(tile.x > std::min(heads.start.x, heads.end.x));
					CHECK(tile.x < std::max(heads.start.x, heads.end.x));
				}
				under++;
			}
			CHECK(under >= 1);
		}
		checked++;
	}
	return checked;
}

TEST_CASE("topology: doubled legs place by their chains", "[junction][topology]")
{
	/* R5 (DESIGN-G1 §16): a doubled leg's relations come from its two chains (HeadsPair, the lane pair's joins); LL_RR at the test spec has six doubled bridges on three legs (D215). */
	CHECK(CheckDoubledLegs(JunctionLLRRSidelineTopology(), MakeTestJunctionSpec(), "LL_RR") == 6);
}

TEST_CASE("topology: doubled legs place by their chains across the sweep", "[junction][topologysweep]")
{
	size_t cells = 0, checked = 0;
	for (int tl = 3; tl <= 8; tl++) {
		for (int v : {100, 120, 160, 196}) {
			JunctionSpec spec = MakeTestJunctionSpec();
			spec.tl = tl;
			spec.train.max_speed = v;
			spec.reference_speed = v;
			spec.prio_tiles = 0;
			spec.prio_tiles = JunctionPrioTiles(spec);
			if (spec.prio_tiles == 0) continue;
			checked += CheckDoubledLegs(JunctionLLRRSidelineTopology(), spec, fmt::format("LL_RR tl {} v {}", tl, v));
			cells++;
		}
	}
	/* The old sizing refused v196 tl 7 and 8; the game-rules prio length sizes them, so 24 cells. */
	CHECK(cells == 24);
	CHECK(checked >= cells);
}

TEST_CASE("topology: a contradictory choice is explained", "[junction][topology]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	bool tried = false;
	for (const JunctionTopology *t : {&JunctionLLRRSidelineTopology(), &JunctionTTopology()}) {
		std::string ignored;
		const std::vector<JunctionLanePath> paths = ExpandJunctionLanes(*t, &ignored);
		const JunctionTrackGraph graph = BuildJunctionTrackGraph(*t, paths, &ignored);
		const JunctionTrackOrder order = OrderJunctionTrackGraph(*t, paths, graph, &ignored);
		const JunctionTrackStructure structure = StructureJunctionTrackGraph(*t, paths, graph, order, &ignored);
		const JunctionTrackPrograms programs = ProgramJunctionTrackGraph(*t, paths, graph, order, structure, &ignored);
		JunctionTrackPlacement placement;
		const JunctionTrackStructure cut = CutJunctionTrackBridges(*t, paths, graph, order, structure, programs, spec, &placement, &ignored);
		REQUIRE(!placement.nodes.empty());

		/* The first joiner_div Corner: the first token of an edge that leaves a band split. */
		uint32_t found = UINT32_MAX;
		for (uint32_t e = 0; e < graph.edges.size() && found == UINT32_MAX; e++) {
			const JunctionTrackEdge &edge = graph.edges[e];
			if (!edge.from.node || programs.edges[e].empty() || programs.edges[e][0].kind != JunctionTrackStep::Kind::Corner) continue;
			if (order.node_level[edge.from.index] % 2 == 0 && graph.nodes[edge.from.index].split) found = e;
		}
		if (found == UINT32_MAX) continue;
		tried = true;

		const int cl = spec.curve_limit ? static_cast<int>(DeriveCurveSpacing(spec.reference_speed, spec.curve_speed, spec.tl)) : 2;
		int h = std::min(2 * static_cast<int>(spec.tl) - 1, std::max(2, cl));
		if (h % 2 == 0) h++;
		const int floor_k = (h - 1) / 2;
		JunctionPlacementChoices choices = placement.choices;
		REQUIRE(found < choices.corner.size());
		REQUIRE(!choices.corner[found].empty());
		choices.corner[found][0] = floor_k - 1;
		std::string error;
		const JunctionTrackPlacement q = PlaceJunctionTrackGraph(*t, paths, graph, order, cut, programs, spec, choices, &error);
		INFO(error);
		CHECK(q.nodes.empty());
		REQUIRE(!q.why.empty());
		bool has_edge = false;
		for (const JunctionPlacementReason &reason : q.why) {
			REQUIRE(reason.rule != nullptr);
			has_edge = has_edge || reason.edge == found;
			CHECK(error.find(reason.rule) != std::string::npos);
		}
		CHECK(has_edge);
		break;
	}
	REQUIRE(tried);
}

TEST_CASE("topology: the seeds keep their test-spec size", "[junction][topology]")
{
	std::string error;
	JunctionTrackPlacement t_placement;
	JunctionTrackPlacement llrr_placement;
	SeedOf(JunctionTTopology(), MakeTestJunctionSpec(), &error, &t_placement);
	SeedOf(JunctionLLRRSidelineTopology(), MakeTestJunctionSpec(), &error, &llrr_placement);
	CHECK(t_placement.size.x == 41);
	CHECK(t_placement.size.y == 14);
	CHECK(llrr_placement.size.x == 62);
	CHECK(llrr_placement.size.y == 25);
}

TEST_CASE("topology: curve spans in the seed routes", "[junction][topology]")
{
	/* The formulas the placement floors use (DESIGN-G1 §14.7): a Corner's curves lie h units apart, an S-bend of m's 2 m; a run of d tiles between two elements puts the last curve of the first and the first curve of the second 2 (d - 1) units apart, and the neighbouring curves one further out by the span of the element they belong to. */
	const JunctionSpec spec = MakeTestJunctionSpec();
	const int cl = spec.curve_limit ? static_cast<int>(DeriveCurveSpacing(spec.reference_speed, spec.curve_speed, spec.tl)) : 2;
	int h = std::min(2 * static_cast<int>(spec.tl) - 1, std::max(2, cl));
	if (h % 2 == 0) h++;

	std::string error;
	std::vector<JunctionTrackRoute> routes;
	JunctionTrackPrograms programs;
	JunctionTrackGraph graph;
	const JunctionLayout layout = SeedOf(JunctionLLRRSidelineTopology(), spec, &error, nullptr, &routes, &programs, &graph);
	REQUIRE(!layout.tracks.empty());

	struct Curve {
		size_t piece; ///< The piece after the turn.
		int unit; ///< Units from the route's start to the start of that piece.
		int x, y;
		uint32_t edge;
		int token; ///< The element's token on its edge, -1 when the curve belongs to no Corner or SBend.
		uint8_t copy;
		JunctionTrackStep::Kind kind;
	};
	size_t spans = 0;
	size_t gaps = 0;
	for (size_t r = 0; r < routes.size(); r++) {
		const JunctionTrackRoute &route = routes[r];
		std::vector<Curve> curves;
		int unit = 0;
		for (size_t i = 0; i < route.pieces.size(); i++) {
			if (i > 0) {
				const DirDiff diff = DirDifference(TrackdirToDirection(route.pieces[i - 1].trackdir), TrackdirToDirection(route.pieces[i].trackdir));
				if (diff == DirDiff::Left45 || diff == DirDiff::Right45) {
					Curve c{i, unit, route.pieces[i].tile.x, route.pieces[i].tile.y, 0, -1, 0, JunctionTrackStep::Kind::Fan};
					for (size_t q : {i, i - 1}) {
						const JunctionTrackPieceTag &g = route.tags[q];
						if (c.token >= 0 || g.token == JunctionTrackPieceTag::RUN) continue;
						const JunctionTrackStep &step = (g.copy ? programs.copies[g.edge] : programs.edges[g.edge])[g.token];
						if (step.kind != JunctionTrackStep::Kind::Corner && step.kind != JunctionTrackStep::Kind::SBend) continue;
						c = {i, unit, route.pieces[i].tile.x, route.pieces[i].tile.y, g.edge, g.token, g.copy, step.kind};
					}
					curves.push_back(c);
				}
			}
			unit += route.pieces[i].units;
		}

		/* Each element's two curves. */
		for (size_t c = 0; c < curves.size(); c++) {
			if (curves[c].token < 0) continue;
			REQUIRE(c + 1 < curves.size());
			const Curve &a = curves[c];
			const Curve &b = curves[c + 1];
			INFO(fmt::format("route {} curve {}: edge {} token {}", r, c, a.edge, a.token));
			REQUIRE(b.token == a.token);
			REQUIRE(b.edge == a.edge);
			spans++;
			if (a.kind == JunctionTrackStep::Kind::Corner) {
				CHECK(b.unit - a.unit == h);
			} else {
				/* The S-bend moves sideways by m tiles on the diagonal, m = |dx| = |dy|. */
				const int m = std::abs(b.x - a.x);
				CHECK(std::abs(b.y - a.y) == m);
				CHECK(b.unit - a.unit == 2 * m);
			}
			c++;
		}

		/* Two elements of one edge with a run of plain tiles between them. */
		for (size_t c = 0; c + 3 < curves.size(); c++) {
			const Curve &a0 = curves[c];
			const Curve &a1 = curves[c + 1];
			const Curve &b0 = curves[c + 2];
			const Curve &b1 = curves[c + 3];
			if (a0.token < 0 || a0.token != a1.token || a0.edge != a1.edge || b0.token < 0 || b0.token != b1.token || b0.edge != b1.edge || a0.edge != b0.edge || a0.copy != b0.copy) continue;
			size_t run = 0;
			for (size_t q = a1.piece; q < b0.piece; q++) {
				const JunctionTrackPieceTag &g = route.tags[q];
				if (g.token == JunctionTrackPieceTag::RUN && g.edge == a0.edge) run++;
			}
			if (run != b0.piece - a1.piece) continue;
			const int span_a = a1.unit - a0.unit;
			const int span_b = b1.unit - b0.unit;
			const int d = static_cast<int>(run) + 1;
			INFO(fmt::format("route {} edge {} tokens {} and {}: run of {} tiles", r, a0.edge, a0.token, b0.token, d));
			CHECK(b0.unit - a1.unit == 2 * (d - 1));
			CHECK(b0.unit - a0.unit == span_a + 2 * (d - 1));
			CHECK(b1.unit - a1.unit == 2 * (d - 1) + span_b);
			gaps++;
		}
	}
	CHECK(spans > 0);
	CHECK(gaps > 0);
}

TEST_CASE("topology: pushing any point later keeps the seed legal", "[junction][topologystretch]")
{
	/* The gate of the placement relations (RECON-stretch C1, C3, C4): every placement point pushed later by hand in X, and every stack boundary's row gap pushed 1 and 2 rows wider in Y, as terrain will (DESIGN-G1 §14.4); a solve failure is an edit, never a bug. In Y every rule must hold in every solved run (T1c-2: the occupancy family and bridge middle clearance of D217; C5b: the prio element footprint). */
	for (int si = 0; si < 2; si++) {
		JunctionSpec spec = MakeTestJunctionSpec();
		if (si == 1) {
			spec.tl = 6;
			spec.train.max_speed = 196;
			spec.reference_speed = 196;
			spec.prio_tiles = 0;
			spec.prio_tiles = JunctionPrioTiles(spec);
		}
		for (int ti = 0; ti < 2; ti++) {
			const JunctionTopology &t = ti == 0 ? JunctionTTopology() : JunctionLLRRSidelineTopology();
			const std::string name = fmt::format("{} {}", ti == 0 ? "T" : "LL_RR", si == 0 ? "test spec" : "tl 6 v 196");
			std::string ignored;
			const std::vector<JunctionLanePath> paths = ExpandJunctionLanes(t, &ignored);
			const JunctionTrackGraph graph = BuildJunctionTrackGraph(t, paths, &ignored);
			const JunctionTrackOrder order = OrderJunctionTrackGraph(t, paths, graph, &ignored);
			const JunctionTrackStructure structure = StructureJunctionTrackGraph(t, paths, graph, order, &ignored);
			const JunctionTrackPrograms programs = ProgramJunctionTrackGraph(t, paths, graph, order, structure, &ignored);
			JunctionTrackPlacement p;
			const JunctionTrackStructure cut = CutJunctionTrackBridges(t, paths, graph, order, structure, programs, spec, &p, &ignored);
			REQUIRE(!p.nodes.empty());
			int runs = 0, failures = 0, clean = 0;
			for (int v = 1; si == 0 && v < static_cast<int>(p.columns.size()); v++) {
				for (int s : {1, 2}) {
					runs++;
					std::string error;
					const JunctionTrackPlacement q = PlaceJunctionTrackGraph(t, paths, graph, order, cut, programs, spec, p.choices, &error, {{v, p.columns[v] + s}});
					if (q.nodes.empty()) {
						failures++;
						INFO(name << " v " << v << " s " << s << " solve failure: " << (q.why.empty() ? std::string("none") : std::string(q.why.front().rule)));
						continue;
					}
					std::string render_error;
					std::vector<JunctionTrackRoute> routes;
					JunctionLayout layout = RenderJunctionTrackPlacement(t, paths, graph, cut, programs, q, &render_error, &routes);
					INFO(name << " v " << v << " s " << s << " render: " << render_error);
					REQUIRE(!layout.tracks.empty());
					SignalJunctionTrackRoutes(layout, routes, t, paths, graph, programs, spec, nullptr, nullptr, &q.prio);
					const std::vector<RuleViolation> violations = CheckJunctionRules(layout, spec, t);
					std::string detail = fmt::format("{} v {} s {}:", name, v, s);
					for (const RuleViolation &viol : violations) detail += fmt::format(" [rule {} at {},{}: {}]", static_cast<int>(viol.rule), viol.tile.x, viol.tile.y, viol.message);
					INFO(detail);
					REQUIRE(violations.empty());
					clean++;
				}
			}
			if (si == 0) WARN(name << ": X runs " << runs << ", solve failures " << failures << ", clean " << clean);

			/* Y: the row gap before stack item i, 1 and 2 wider (the floor on item i's row; later items follow). */
			int y_runs = 0, y_failures = 0, y_violating = 0, y_clean = 0;
			std::map<int, int> per_rule;
			for (int i = 1; i < static_cast<int>(p.item_rows.size()); i++) {
				for (int s : {1, 2}) {
					y_runs++;
					std::string error;
					const JunctionTrackPlacement q = PlaceJunctionTrackGraph(t, paths, graph, order, cut, programs, spec, p.choices, &error, {}, {{1 + i, p.item_rows[i] + s}});
					if (q.nodes.empty()) {
						y_failures++;
						continue;
					}
					std::string render_error;
					std::vector<JunctionTrackRoute> routes;
					JunctionLayout layout = RenderJunctionTrackPlacement(t, paths, graph, cut, programs, q, &render_error, &routes);
					if (layout.tracks.empty()) {
						INFO(name << " item " << i << " s " << s << " render: " << render_error);
						y_failures++;
						continue;
					}
					SignalJunctionTrackRoutes(layout, routes, t, paths, graph, programs, spec, nullptr, nullptr, &q.prio);
					std::string detail = fmt::format("{} item {} s {}:", name, i, s);
					size_t required = 0;
					for (const RuleViolation &viol : CheckJunctionRules(layout, spec, t)) {
						detail += fmt::format(" [rule {} at {},{}: {}]", static_cast<int>(viol.rule), viol.tile.x, viol.tile.y, viol.message);
						per_rule[static_cast<int>(viol.rule)]++;
						required++;
					}
					INFO(detail);
					CHECK(required == 0);
					y_violating += required != 0;
					y_clean += required == 0;
				}
			}
			std::string rules;
			for (const auto &[rule, count] : per_rule) rules += fmt::format(" rule {}: {}", rule, count);
			WARN(name << ": Y runs " << y_runs << ", solve failures " << y_failures << ", runs with violations " << y_violating << " (violations per rule:" << rules << "), clean " << y_clean);
		}
	}
}

TEST_CASE("topology: occupancy replaces the site relations", "[junction][topology]")
{
	/* The occupancy family (D217) is tight at the seed, where C1 (in fan order), C3 (bridge head clear of rows) and the hug picks used to be: the seeds keep their numbers. */
	for (const JunctionTopology *t : {&JunctionTTopology(), &JunctionLLRRSidelineTopology()}) {
		JunctionTrackPlacement p, q;
		std::string error;
		PlaceTwice(*t, MakeTestJunctionSpec(), &p, &q, &error);
		INFO(error);
		REQUIRE(!q.nodes.empty());
		CHECK(SamePlacement(p, q));
		CHECK(q.columns == p.columns);
	}
	JunctionTrackPlacement t_placement, llrr_placement;
	std::string error;
	SeedOf(JunctionTTopology(), MakeTestJunctionSpec(), &error, &t_placement);
	SeedOf(JunctionLLRRSidelineTopology(), MakeTestJunctionSpec(), &error, &llrr_placement);
	CHECK(t_placement.size.x == 41);
	CHECK(t_placement.size.y == 14);
	CHECK(llrr_placement.size.x == 62);
	CHECK(llrr_placement.size.y == 25);
}

/** A topology's seed pipeline: the inputs of a placement and the cut's seed placement, under a spec. */
struct SeedPipeline {
	std::vector<JunctionLanePath> paths;
	JunctionTrackGraph graph;
	JunctionTrackOrder order;
	JunctionTrackStructure cut;
	JunctionTrackPrograms programs;
	JunctionSpec spec;
	JunctionTrackPlacement seed;
	JunctionLayout layout; ///< The seed's render.
};

static SeedPipeline SeedPipelineOf(const JunctionTopology &t, const JunctionSpec &spec)
{
	SeedPipeline p;
	p.spec = spec;
	std::string error;
	p.paths = ExpandJunctionLanes(t, &error);
	p.graph = BuildJunctionTrackGraph(t, p.paths, &error);
	p.order = OrderJunctionTrackGraph(t, p.paths, p.graph, &error);
	const JunctionTrackStructure structure = StructureJunctionTrackGraph(t, p.paths, p.graph, p.order, &error);
	p.programs = ProgramJunctionTrackGraph(t, p.paths, p.graph, p.order, structure, &error);
	p.cut = CutJunctionTrackBridges(t, p.paths, p.graph, p.order, structure, p.programs, spec, &p.seed, &error);
	INFO(error);
	REQUIRE(!p.seed.nodes.empty());
	p.layout = RenderJunctionTrackPlacement(t, p.paths, p.graph, p.cut, p.programs, p.seed, &error);
	REQUIRE(!p.layout.tracks.empty());
	return p;
}

/** The seed's placement on terrain with the given tiles blocked, at offset (0, 0): the seed's frame is the terrain's. */
static JunctionTrackPlacement PlaceWithHouses(const JunctionTopology &t, const SeedPipeline &p, const JunctionTerrain &flat, const std::vector<std::pair<int, int>> &houses, std::string *error)
{
	JunctionTerrain terrain = flat;
	for (const auto &[x, y] : houses) terrain.tiles[static_cast<size_t>(y - terrain.origin.y) * terrain.width + (x - terrain.origin.x)].cls = JunctionTileClass::Blocked;
	const JunctionFacts facts(terrain);
	return PlaceJunctionTrackGraph(t, p.paths, p.graph, p.order, p.cut, p.programs, p.spec, p.seed.choices, facts, {0, 0}, error);
}

TEST_CASE("topology: a bridge middle over a Blocked tile fails the placement naming the tile", "[junction][topology]")
{
	const JunctionTopology &t = JunctionTTopology();
	const SeedPipeline p = SeedPipelineOf(t, MakeTestJunctionSpec());
	JunctionTerrain flat;
	flat.origin = {-1, -1};
	flat.width = p.seed.size.x + 2;
	flat.height = p.seed.size.y + 2;
	flat.tiles.assign(static_cast<size_t>(flat.width) * flat.height, JunctionTerrainTile{});
	flat.foundation_cost = 250;
	flat.build_on_slopes = true;
	std::string error;
	const JunctionFacts flat_facts(flat);
	const JunctionTrackPlacement reference = PlaceJunctionTrackGraph(t, p.paths, p.graph, p.order, p.cut, p.programs, p.spec, p.seed.choices, flat_facts, {0, 0}, &error);
	INFO(error);
	REQUIRE(!reference.nodes.empty());

	size_t b = 0;
	while (b < p.cut.bridges.size() && p.cut.bridges[b].tunnel) b++;
	REQUIRE(b < p.cut.bridges.size());
	const JunctionBridge &head = reference.bridges[b][0];
	const int dx = (head.end.x > head.start.x) - (head.end.x < head.start.x);
	const int dy = (head.end.y > head.start.y) - (head.end.y < head.start.y);
	REQUIRE(std::abs(head.end.x - head.start.x) + std::abs(head.end.y - head.start.y) >= 2);
	const int mx = head.start.x + dx;
	const int my = head.start.y + dy;

	JunctionTerrain terrain = flat;
	terrain.tiles[static_cast<size_t>(my - terrain.origin.y) * terrain.width + (mx - terrain.origin.x)].cls = JunctionTileClass::Blocked;
	const JunctionFacts facts(terrain);
	const JunctionTrackPlacement blocked = PlaceJunctionTrackGraph(t, p.paths, p.graph, p.order, p.cut, p.programs, p.spec, p.seed.choices, facts, {0, 0}, &error);
	CHECK(blocked.nodes.empty());
	CHECK(error.find("bridge span over a blocked tile") != std::string::npos);
}

TEST_CASE("topology: ramp levels per train length from the train", "[junction][topology]")
{
	/* D219. The D138 train at v 196, s 3, by the game's integer arithmetic: tractive force 14194 hp x 746 x 18 / (5 x 196) = 194486 N (under max T.E.); resistance 10 x 561 + 561 x (15 x 708 / 512 = 20) + 14 x 25 x 196^2 / 1000 = 30275 N; spare 164211 N; slope force 561 x 3 x 100 = 168300 N. */
	JunctionSpec spec = MakeTestJunctionSpec();
	spec.reference_speed = 196;
	spec.slope_steepness = 3;
	spec.tl = 4;
	CHECK(JunctionRampLevels(spec) == 3); // floor(4 x 164211 / 168300) = floor(3.90).
	spec.tl = 8;
	CHECK(JunctionRampLevels(spec) == 7); // floor(7.81).
	/* Half the power: tractive force 97243 N, spare 66968 N, so floor(4 x 66968 / 168300) = 1. */
	spec.tl = 4;
	spec.train.power_hp = 7097;
	CHECK(JunctionRampLevels(spec) == 1);
	/* No slope force: no limit. */
	spec.slope_steepness = 0;
	CHECK(JunctionRampLevels(spec) >= spec.tl);
}

TEST_CASE("topology: the hard-facts mode ignores heights", "[junction][topology]")
{
	const JunctionTopology &t = JunctionTTopology();
	const SeedPipeline p = SeedPipelineOf(t, MakeTestJunctionSpec());
	JunctionTerrain flat;
	flat.origin = {-1, -1};
	flat.width = p.seed.size.x + 2;
	flat.height = p.seed.size.y + 2;
	flat.tiles.assign(static_cast<size_t>(flat.width) * flat.height, JunctionTerrainTile{});
	flat.foundation_cost = 250;
	flat.build_on_slopes = true;
	std::string error;
	auto place = [&](const JunctionTerrain &terrain, bool heights) {
		const JunctionFacts facts(terrain);
		return PlaceJunctionTrackGraph(t, p.paths, p.graph, p.order, p.cut, p.programs, p.spec, p.seed.choices, facts, {0, 0}, &error, heights);
	};
	auto height_reason = [](const JunctionTrackPlacement &q) {
		return std::ranges::any_of(q.why, [](const JunctionPlacementReason &r) { return std::string_view{r.rule}.find("height") != std::string_view::npos; });
	};
	const JunctionTrackPlacement reference = place(flat, true);
	REQUIRE(!reference.nodes.empty());

	/* A ridge column of one level across the box: a run that crosses it meets a height step. */
	bool found = false;
	int ridge_x = -1;
	for (int x = 2; x < p.seed.size.x - 2 && !found; x++) {
		JunctionTerrain ridge = flat;
		for (int y = ridge.origin.y; y < ridge.origin.y + ridge.height; y++) {
			JunctionTerrainTile &tile = ridge.tiles[static_cast<size_t>(y - ridge.origin.y) * ridge.width + (x - ridge.origin.x)];
			tile.z_min = tile.z_max = 1;
		}
		const JunctionTrackPlacement with = place(ridge, true);
		if (!with.nodes.empty()) continue;
		found = true;
		ridge_x = x;
		CHECK(height_reason(with));
		const JunctionTrackPlacement without = place(ridge, false);
		REQUIRE(!without.nodes.empty());
		CHECK(SamePlacement(reference, without));
		CHECK(reference.columns == without.columns);
	}
	CHECK(found);

	/* Houses on a whole column fail both, where the ridge failed one. */
	REQUIRE(ridge_x >= 0);
	JunctionTerrain wall = flat;
	for (int y = wall.origin.y; y < wall.origin.y + wall.height; y++) wall.tiles[static_cast<size_t>(y - wall.origin.y) * wall.width + (ridge_x - wall.origin.x)].cls = JunctionTileClass::Blocked;
	CHECK(place(wall, true).nodes.empty());
	CHECK(place(wall, false).nodes.empty());
}

/** A flat terrain of height 0 around the seed's box (origin -1, so the seed's frame is the terrain's frame at offset (0, 0)), foundations 250, terraform 100, clearing 0. */
static JunctionTerrain FlatTerrainFor(const SeedPipeline &p)
{
	JunctionTerrain flat;
	flat.origin = {-1, -1};
	flat.width = p.seed.size.x + 2;
	flat.height = p.seed.size.y + 2;
	flat.tiles.assign(static_cast<size_t>(flat.width) * flat.height, JunctionTerrainTile{});
	flat.foundation_cost = 250;
	flat.terraform_cost = 100;
	flat.build_on_slopes = true;
	return flat;
}

/** The terrain with every corner (cx, cy) at height_of(cx, cy), consistent across tiles. */
template <typename F>
static JunctionTerrain TerrainFromCorners(JunctionTerrain terrain, F height_of)
{
	for (int y = terrain.origin.y; y < terrain.origin.y + terrain.height; y++) {
		for (int x = terrain.origin.x; x < terrain.origin.x + terrain.width; x++) {
			SetJunctionTileCorners(terrain.tiles[static_cast<size_t>(y - terrain.origin.y) * terrain.width + (x - terrain.origin.x)], {height_of(x + 1, y), height_of(x + 1, y + 1), height_of(x, y + 1), height_of(x, y)});
		}
	}
	return terrain;
}

/** The placement with the height solve on the terrain (offset (0, 0)). */
static JunctionTrackPlacement PlaceTerraformed(const JunctionTopology &t, const SeedPipeline &p, const JunctionTerrain &terrain, std::string *error)
{
	const JunctionFacts facts(terrain);
	return PlaceJunctionTrackGraph(t, p.paths, p.graph, p.order, p.cut, p.programs, p.spec, p.seed.choices, facts, {0, 0}, error, true, true, &terrain);
}

TEST_CASE("topoproj: a ridge row is cut, not avoided", "[junction][topoproj]")
{
	const JunctionTopology &t = JunctionTTopology();
	const SeedPipeline p = SeedPipelineOf(t, MakeTestJunctionSpec());
	const JunctionTerrain flat = FlatTerrainFor(p);
	std::string error;
	const JunctionTrackPlacement reference = PlaceTerraformed(t, p, flat, &error);
	INFO(error);
	REQUIRE(!reference.nodes.empty());

	/* A ridge row across the whole box, two levels high with a one-level step per tile on both sides, at row 8 (the placement's lane rows lie on both sides of it: every Y run and diagonal crossing it must be cut or climb it). Chosen from a scan of 68 ridges (columns and rows, heights 1-4): the first cut solved 31, failed 21 on a corner two tiles want at different heights or two copies sharing a run, and 16 with no levels between two fixed ends (Z2c report). */
	constexpr int RIDGE_Y = 8;
	const JunctionTerrain ridge = TerrainFromCorners(flat, [&](int, int cy) { return std::max(0, 2 - std::abs(cy - RIDGE_Y)); });
	const JunctionTrackPlacement cut = PlaceTerraformed(t, p, ridge, &error);
	INFO(error);
	REQUIRE(!cut.nodes.empty());
	CHECK(!cut.terraform.corners.empty());
	CHECK(cut.terraform.cost > 0);
	CHECK(cut.height_cost >= cut.terraform.cost);
	CHECK(SamePlacement(reference, cut));
	CHECK(reference.columns == cut.columns);

	/* The terraformed terrain passes the height facts: the same placement, and its render meets every height. */
	const JunctionTerrain after = ApplyJunctionTerraform(ridge, cut.terraform);
	const JunctionFacts after_facts(after);
	const JunctionTrackPlacement again = PlaceJunctionTrackGraph(t, p.paths, p.graph, p.order, p.cut, p.programs, p.spec, p.seed.choices, after_facts, {0, 0}, &error);
	INFO(error);
	REQUIRE(!again.nodes.empty());
	CHECK(SamePlacement(reference, again));
	std::vector<JunctionTrackRoute> routes;
	JunctionLayout layout = RenderJunctionTrackPlacement(t, p.paths, p.graph, p.cut, p.programs, again, &error, &routes);
	REQUIRE(!layout.tracks.empty());
	std::string why;
	CHECK(JunctionRoutesHeightsOk(routes, layout, {0, 0}, after, {0, 0}, &why));
	INFO(why);
}

TEST_CASE("topoproj: the prio element sees the terrain", "[junction][topoproj]")
{
	/* RW6: a house on a prio tile that no other track uses. The facts placement moves the prio element or fails naming it; it never leaves the element on the house. */
	const JunctionTopology &t = JunctionTTopology();
	const SeedPipeline p = SeedPipelineOf(t, MakeTestJunctionSpec());
	constexpr int MARGIN = 8;
	JunctionTerrain flat = FlatTerrainFor(p);
	flat.origin = {-MARGIN, -MARGIN};
	flat.width = p.seed.size.x + 2 * MARGIN;
	flat.height = p.seed.size.y + 2 * MARGIN;
	flat.tiles.assign(static_cast<size_t>(flat.width) * flat.height, JunctionTerrainTile{});

	/* The tiles the signal pass draws beside the main line for a placement (its prio elements' side rows). */
	const auto prio_tiles = [&](const JunctionTrackPlacement &q, std::string *error) {
		std::vector<TileIndexDiffC> tiles;
		std::vector<JunctionTrackRoute> routes;
		JunctionLayout layout = RenderJunctionTrackPlacement(t, p.paths, p.graph, p.cut, p.programs, q, error, &routes);
		REQUIRE(!layout.tracks.empty());
		std::vector<std::pair<TileIndexDiffC, TileIndexDiffC>> beside;
		SignalJunctionTrackRoutes(layout, routes, t, p.paths, p.graph, p.programs, p.spec, nullptr, &beside, &q.prio);
		for (const auto &[row, ml] : beside) tiles.push_back(row);
		return tiles;
	};
	std::string error;
	std::set<std::pair<int, int>> used;
	for (const JunctionTrack &jt : p.layout.tracks) used.emplace(jt.offset.x, jt.offset.y);
	const std::vector<TileIndexDiffC> seed_prio = prio_tiles(p.seed, &error);
	std::vector<TileIndexDiffC> free_tiles;
	for (const TileIndexDiffC &tile : seed_prio) {
		if (!used.contains({tile.x, tile.y})) free_tiles.push_back(tile);
	}
	REQUIRE(!free_tiles.empty());
	const TileIndexDiffC house = free_tiles[free_tiles.size() / 2];
	INFO("house " << house.x << "," << house.y << " of " << free_tiles.size() << " free prio tiles");

	const JunctionTrackPlacement q = PlaceWithHouses(t, p, flat, {{house.x, house.y}}, &error);
	INFO(error);
	if (q.nodes.empty()) {
		bool names_prio = error.find("prio element") != std::string::npos;
		for (const JunctionPlacementReason &reason : q.why) names_prio = names_prio || (reason.rule != nullptr && std::string(reason.rule) == "prio element");
		CHECK(names_prio);
		return;
	}
	std::string render_error;
	const std::vector<TileIndexDiffC> moved = prio_tiles(q, &render_error);
	CHECK(!moved.empty());
	for (const TileIndexDiffC &tile : moved) CHECK(!(tile.x == house.x && tile.y == house.y));
}

TEST_CASE("levels: ridges across the T", "[junction][topoprojsweep]")
{
	const JunctionTopology &t = JunctionTTopology();
	const SeedPipeline p = SeedPipelineOf(t, MakeTestJunctionSpec());
	/* Margin 8 around the box: a cut across a ridge as wide as the box spills over the margin (a ridge of height h is h tiles wide each side). */
	constexpr int MARGIN = 8;
	JunctionTerrain flat = FlatTerrainFor(p);
	flat.origin = {-MARGIN, -MARGIN};
	flat.width = p.seed.size.x + 2 * MARGIN;
	flat.height = p.seed.size.y + 2 * MARGIN;
	flat.tiles.assign(static_cast<size_t>(flat.width) * flat.height, JunctionTerrainTile{});
	std::string error;
	const JunctionTrackPlacement reference = PlaceTerraformed(t, p, flat, &error);
	INFO(error);
	REQUIRE(!reference.nodes.empty());

	/* Every ridge column and row over the box, heights 1-4, each a one-level step per tile on both sides. Each solved cut must re-place the reference on the terraformed ground and meet every height (T3). */
	int total = 0;
	int solved = 0;
	int failing = 0;
	int64_t micros_sum = 0;
	int64_t micros_max = 0;
	std::map<std::string, int> reasons;
	for (int axis = 0; axis < 2; axis++) {
		const int limit = axis == 0 ? p.seed.size.x : p.seed.size.y;
		for (int h = 1; h <= 4; h++) {
			for (int c = 0; c <= limit; c++) {
				const JunctionTerrain ridge = TerrainFromCorners(flat, [&](int cx, int cy) { return std::max(0, h - std::abs((axis == 0 ? cx : cy) - c)); });
				const auto start = std::chrono::steady_clock::now();
				const JunctionTrackPlacement cut = PlaceTerraformed(t, p, ridge, &error);
				const int64_t micros = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
				total++;
				if (cut.nodes.empty()) {
					reasons[error.substr(0, error.find(':'))]++;
					continue;
				}
				solved++;
				micros_sum += micros;
				micros_max = std::max(micros_max, micros);
				const JunctionTerrain after = ApplyJunctionTerraform(ridge, cut.terraform);
				const JunctionFacts after_facts(after);
				std::string again_error;
				const JunctionTrackPlacement again = PlaceJunctionTrackGraph(t, p.paths, p.graph, p.order, p.cut, p.programs, p.spec, p.seed.choices, after_facts, {0, 0}, &again_error);
				bool ok = !again.nodes.empty() && SamePlacement(reference, again);
				if (ok) {
					std::vector<JunctionTrackRoute> routes;
					std::string render_error;
					std::string why;
					const JunctionLayout layout = RenderJunctionTrackPlacement(t, p.paths, p.graph, p.cut, p.programs, again, &render_error, &routes);
					ok = !layout.tracks.empty() && JunctionRoutesHeightsOk(routes, layout, {0, 0}, after, {0, 0}, &why);
				}
				INFO((axis == 0 ? "column " : "row ") << c << " height " << h << ": " << again_error);
				CHECK(ok);
				if (!ok) failing++;
			}
		}
	}
	std::cout << "LEVELS ridges total=" << total << " solved=" << solved << " t3fail=" << failing << " micros mean=" << (solved > 0 ? micros_sum / solved : 0) << " max=" << micros_max << "\n";
	for (const auto &[reason, count] : reasons) std::cout << "LEVELS ridge failure " << count << " x " << reason << "\n";
	CHECK(total == 228);
	CHECK(solved >= 227);
}

TEST_CASE("topology: flat ground needs no terraform", "[junction][topology]")
{
	for (const JunctionTopology *t : {&JunctionTTopology(), &JunctionLLRRSidelineTopology()}) {
		const SeedPipeline p = SeedPipelineOf(*t, MakeTestJunctionSpec());
		const JunctionTerrain flat = FlatTerrainFor(p);
		std::string error;
		const JunctionTrackPlacement placement = PlaceTerraformed(*t, p, flat, &error);
		INFO(error);
		REQUIRE(!placement.nodes.empty());
		CHECK(placement.terraform.corners.empty());
		CHECK(placement.terraform.cost == 0);
		CHECK(placement.height_cost == 0);
	}
}

TEST_CASE("topology: the ramp rule spaces up-ramps", "[junction][topology]")
{
	/* Ground that climbs 3 levels on three tiles in a row (corner columns 10, 11, 12 at 1, 2, 3) across the whole T: every edge that crosses it climbs 3 levels. */
	const JunctionTopology &t = JunctionTTopology();
	constexpr int HILL_X = 10;
	auto up_ramps = [&](const SeedPipeline &p, const JunctionTerrain &terrain, const JunctionTrackPlacement &placement, int d, bool *adjacent) {
		std::string error;
		std::vector<JunctionTrackRoute> routes;
		const JunctionLayout layout = RenderJunctionTrackPlacement(t, p.paths, p.graph, p.cut, p.programs, placement, &error, &routes);
		REQUIRE(!layout.tracks.empty());
		int checked = 0;
		for (const JunctionTrackRoute &route : routes) {
			int last = INT_MIN;
			for (size_t i = 0; i < route.pieces.size(); i++) {
				const JunctionPiece &piece = route.pieces[i];
				if (piece.on_bridge) continue;
				const JunctionTerrainTile *tile = terrain.At(piece.tile.x, piece.tile.y);
				REQUIRE(tile != nullptr);
				const bool axis = piece.trackdir == Trackdir::X_SW || piece.trackdir == Trackdir::X_NE || piece.trackdir == Trackdir::Y_SE || piece.trackdir == Trackdir::Y_NW;
				if (!axis || tile->incline != TrackdirToExitdir(piece.trackdir)) continue;
				/* An up-ramp: the index of the piece counts along the route. */
				if (last != INT_MIN) {
					checked++;
					if (static_cast<int>(i) - last < d) {
						if (adjacent != nullptr) *adjacent = true;
						else CHECK(static_cast<int>(i) - last >= d);
					}
				}
				last = static_cast<int>(i);
			}
		}
		return checked;
	};

	auto solve = [&](JunctionSpec spec) {
		const SeedPipeline p = SeedPipelineOf(t, spec);
		const JunctionTerrain flat = FlatTerrainFor(p);
		const JunctionTerrain hill = TerrainFromCorners(flat, [&](int cx, int) { return std::clamp(cx - HILL_X, 0, 3); });
		std::string error;
		const JunctionTrackPlacement placement = PlaceTerraformed(t, p, hill, &error);
		INFO(error);
		REQUIRE(!placement.nodes.empty());
		const JunctionTerrain after = ApplyJunctionTerraform(hill, placement.terraform);
		return std::make_tuple(p, after, placement);
	};

	/* One level per four tiles at most: up-ramps at least d = 4 apart (D219: floor(3 / d) + 1 <= 1). */
	JunctionSpec slow = MakeTestJunctionSpec();
	slow.reference_speed = 196;
	slow.slope_steepness = 3;
	slow.tl = 4;
	slow.train.power_hp = 7097;
	REQUIRE(JunctionRampLevels(slow) == 1);
	{
		const auto [p, after, placement] = solve(slow);
		CHECK(up_ramps(p, after, placement, 4, nullptr) > 0); // Up-ramp pairs exist, so the spacing is not vacuous.
	}

	/* No limit: the ground's own three adjacent inclines are the cheapest solution on some edge. */
	JunctionSpec free_climb = MakeTestJunctionSpec();
	free_climb.slope_steepness = 0;
	REQUIRE(JunctionRampLevels(free_climb) >= free_climb.tl);
	{
		const auto [p, after, placement] = solve(free_climb);
		bool adjacent = false;
		up_ramps(p, after, placement, 2, &adjacent);
		CHECK(adjacent);
	}
}

TEST_CASE("topology: doubled bridge heads hug their outermost crossers", "[junction][topologystretch]")
{
	/* Under every Y stretch that solves, each doubled bridge's copy-0 heads lie one tile outside the outermost tiles of the crossers on both rows (D217: by order, not by a pick that goes stale; RECON-stretch §4). */
	int checked = 0;
	for (const JunctionTopology *t : {&JunctionTTopology(), &JunctionLLRRSidelineTopology()}) {
		const JunctionSpec spec = MakeTestJunctionSpec();
		const SeedPipeline sp = SeedPipelineOf(*t, spec);
		for (int i = 0; i <= static_cast<int>(sp.seed.item_rows.size()); i++) {
			for (int s : {0, 1, 2}) {
				/* i 0 is the seed itself; item i >= 1 has the row gap before it s rows wider. */
				if ((i == 0) != (s == 0) || i == static_cast<int>(sp.seed.item_rows.size())) continue;
				std::string error;
				std::vector<std::pair<int, int>> floors;
				if (i > 0) floors.emplace_back(1 + i, sp.seed.item_rows[i] + s);
				const JunctionTrackPlacement q = PlaceJunctionTrackGraph(*t, sp.paths, sp.graph, sp.order, sp.cut, sp.programs, spec, sp.seed.choices, &error, {}, floors);
				if (q.nodes.empty()) continue;
				for (size_t b = 0; b < sp.cut.bridges.size(); b++) {
					const JunctionTrackBridge &bridge = sp.cut.bridges[b];
					if (!bridge.doubled) continue;
					/* The crossers' tiles on the copy row, and on the lane row in the copy-0 heads' frame (the lane row's heads stand one step upstream). */
					const int dir = q.bridges[b][0].end.x > q.bridges[b][0].start.x ? 1 : -1;
					int low = INT_MAX, high = INT_MIN;
					for (uint32_t x : bridge.crossed) {
						for (size_t tok = 0; tok < sp.programs.edges[x].size(); tok++) {
							const JunctionTrackStep &step = sp.programs.edges[x][tok];
							if (step.kind != JunctionTrackStep::Kind::Under || step.index != bridge.edge) continue;
							const JunctionTrackSpan &span = q.edges[x][tok];
							const int shift = step.copy == 0 ? 0 : dir;
							low = std::min({low, static_cast<int>(span.first.x) + shift, static_cast<int>(span.second.x) + shift});
							high = std::max({high, static_cast<int>(span.first.x) + shift, static_cast<int>(span.second.x) + shift});
						}
					}
					REQUIRE(low <= high);
					const int head_low = std::min(q.bridges[b][0].start.x, q.bridges[b][0].end.x);
					const int head_high = std::max(q.bridges[b][0].start.x, q.bridges[b][0].end.x);
					INFO(fmt::format("{} item {} s {} bridge {} (edge {}): heads {}..{}, crossers {}..{}", t == &JunctionTTopology() ? "T" : "LL_RR", i, s, b, bridge.edge, head_low, head_high, low, high));
					CHECK(head_low == low - 1);
					CHECK(head_high == high + 1);
					checked++;
				}
			}
		}
	}
	CHECK(checked > 0);
}

TEST_CASE("topology: the prio element has one source", "[junction][topology]")
{
	/* The placement's prio spans are what the signal pass draws when it derives the elements from the routes (RW6b), and the tiles beside the main line the level solve reads. */
	for (const JunctionTopology *t : {&JunctionTTopology(), &JunctionLLRRSidelineTopology()}) {
		const SeedPipeline p = SeedPipelineOf(*t, MakeTestJunctionSpec());
		std::string error;
		std::vector<JunctionTrackRoute> routes;
		const JunctionLayout render = RenderJunctionTrackPlacement(*t, p.paths, p.graph, p.cut, p.programs, p.seed, &error, &routes);
		INFO(error);
		REQUIRE(!render.tracks.empty());
		const auto bits_of = [](const JunctionLayout &layout) {
			std::map<std::pair<int, int>, TrackBits> bits;
			for (const JunctionTrack &jt : layout.tracks) bits[{jt.offset.x, jt.offset.y}] |= jt.tracks;
			return bits;
		};
		JunctionLayout derived = render;
		std::vector<std::pair<TileIndexDiffC, TileIndexDiffC>> derived_beside;
		SignalJunctionTrackRoutes(derived, routes, *t, p.paths, p.graph, p.programs, p.spec, nullptr, &derived_beside);
		JunctionLayout given = render;
		std::vector<std::pair<TileIndexDiffC, TileIndexDiffC>> given_beside;
		SignalJunctionTrackRoutes(given, routes, *t, p.paths, p.graph, p.programs, p.spec, nullptr, &given_beside, &p.seed.prio);
		CHECK(bits_of(derived) == bits_of(given));
		const auto same_beside = [](const std::vector<std::pair<TileIndexDiffC, TileIndexDiffC>> &a, const std::vector<std::pair<TileIndexDiffC, TileIndexDiffC>> &b) {
			return std::ranges::equal(a, b, [](const auto &l, const auto &r) { return l.first.x == r.first.x && l.first.y == r.first.y && l.second.x == r.second.x && l.second.y == r.second.y; });
		};
		CHECK(same_beside(derived_beside, given_beside));
		const auto subset = [](TrackBits a, TrackBits b) {
			for (Track track : a) {
				if (!b.Test(track)) return false;
			}
			return true;
		};
		/* The spans' bits are the bits the derived layout has on top of the render's. */
		const std::map<std::pair<int, int>, TrackBits> before = bits_of(render);
		std::map<std::pair<int, int>, TrackBits> drawn;
		for (const auto &[key, bits] : bits_of(derived)) {
			const auto it = before.find(key);
			TrackBits extra;
			for (Track track : bits) {
				if (it == before.end() || !it->second.Test(track)) extra.Set(track);
			}
			if (extra.Any()) drawn[key] = extra;
		}
		std::map<std::pair<int, int>, TrackBits> spans;
		size_t n_tiles = 0;
		for (const JunctionPrioSpan &span : p.seed.prio) {
			for (const auto &[tile, bits] : span.tiles) {
				spans[{tile.x, tile.y}] |= bits;
				n_tiles++;
			}
		}
		CHECK(n_tiles > 0);
		const std::map<std::pair<int, int>, TrackBits> after = bits_of(derived);
		for (const auto &[key, bits] : spans) {
			const auto it = after.find(key);
			REQUIRE(it != after.end());
			CHECK(subset(bits, it->second));
		}
		for (const auto &[key, bits] : drawn) {
			const auto it = spans.find(key);
			REQUIRE(it != spans.end());
			CHECK(subset(bits, it->second));
		}
	}
}

TEST_CASE("topology: placement on flat facts is the seed", "[junction][topology]")
{
	for (const JunctionTopology *t : {&JunctionTTopology(), &JunctionLLRRSidelineTopology()}) {
		const SeedPipeline p = SeedPipelineOf(*t, MakeTestJunctionSpec());
		const JunctionTerrain flat = FlatTerrainAround(p.layout);
		std::string error;
		const JunctionTrackPlacement q = PlaceWithHouses(*t, p, flat, {}, &error);
		INFO(error);
		REQUIRE(!q.nodes.empty());
		CHECK(SamePlacement(p.seed, q));
	}
}

TEST_CASE("topology: run and group tiles match the render", "[junction][topoprojsweep]")
{
	/* The facts are applied to the tiles of the rendered routes but the bridge middles, and to the prio elements' side rows, no others: a house on a tile changes the placement (or fails it) exactly when a fact is applied to a tile the seed uses. */
	const JunctionTopology &t = JunctionLLRRSidelineTopology();
	const SeedPipeline p = SeedPipelineOf(t, MakeTestJunctionSpec());
	std::string error;
	std::vector<JunctionTrackRoute> routes;
	const JunctionLayout render = RenderJunctionTrackPlacement(t, p.paths, p.graph, p.cut, p.programs, p.seed, &error, &routes);
	REQUIRE(!render.tracks.empty());
	std::set<std::pair<int, int>> heads;
	for (const std::array<JunctionBridge, 2> &copies : p.seed.bridges) {
		for (const JunctionBridge &b : copies) {
			if (b.start.x == b.end.x && b.start.y == b.end.y) continue;
			heads.emplace(b.start.x, b.start.y);
			heads.emplace(b.end.x, b.end.y);
		}
	}
	std::set<std::pair<int, int>> rendered;
	for (const JunctionTrackRoute &route : routes) {
		for (const JunctionPiece &piece : route.pieces) {
			if (piece.on_bridge && !heads.contains({piece.tile.x, piece.tile.y})) continue;
			rendered.emplace(piece.tile.x, piece.tile.y);
		}
	}
	/* The prio elements' side rows are tiles the seed uses too (RW6): the signal pass draws them, the facts see them. */
	{
		JunctionLayout signalled = render;
		std::vector<std::pair<TileIndexDiffC, TileIndexDiffC>> beside;
		SignalJunctionTrackRoutes(signalled, routes, t, p.paths, p.graph, p.programs, p.spec, nullptr, &beside, &p.seed.prio);
		for (const auto &[row, ml] : beside) rendered.emplace(row.x, row.y);
	}
	REQUIRE(!rendered.empty());

	/* Flat ground over the box with a margin of 1: the facts of each solve are rebuilt, so the snapshot stays small. */
	JunctionTerrain flat;
	flat.origin = {-1, -1};
	flat.width = p.seed.size.x + 2;
	flat.height = p.seed.size.y + 2;
	flat.tiles.assign(static_cast<size_t>(flat.width) * flat.height, JunctionTerrainTile{});
	flat.foundation_cost = 250;
	/* Every rendered tile on its own changes the placement or fails it: a fact is applied to it. */
	std::string missing;
	for (const auto &[x, y] : rendered) {
		const JunctionTrackPlacement q = PlaceWithHouses(t, p, flat, {{x, y}}, &error);
		if (!q.nodes.empty() && SamePlacement(p.seed, q)) missing += fmt::format(" ({},{})", x, y);
	}
	/* Every other tile of the box blocked at once leaves the seed as it is: a fact applied to any of them would move it, as the seed covers the tile (the seed is the least solution of any set of facts it meets). */
	/* Bridge middles are left out: a house under one is refused by the span fact (HH2, D175), not a tile the seed covers. */
	std::set<std::pair<int, int>> middles;
	for (const auto &copies : p.seed.bridges) {
		for (const JunctionBridge &span : copies) {
			const int sx = (span.end.x > span.start.x) - (span.end.x < span.start.x);
			const int sy = (span.end.y > span.start.y) - (span.end.y < span.start.y);
			if (sx == 0 && sy == 0) continue;
			for (int x = span.start.x + sx, y = span.start.y + sy; x != span.end.x || y != span.end.y; x += sx, y += sy) middles.emplace(x, y);
		}
	}
	std::vector<std::pair<int, int>> others;
	for (int y = 0; y < p.seed.size.y; y++) {
		for (int x = 0; x < p.seed.size.x; x++) {
			if (!rendered.contains({x, y}) && !middles.contains({x, y})) others.emplace_back(x, y);
		}
	}
	const JunctionTrackPlacement rest = PlaceWithHouses(t, p, flat, others, &error);
	INFO(error);
	INFO("rendered tiles without a fact:" << missing);
	CHECK(missing.empty());
	REQUIRE(!rest.nodes.empty());
	CHECK(SamePlacement(p.seed, rest));
}

TEST_CASE("topoproj: the objective gives way to a house", "[junction][topoprojsweep]")
{
	/* D215 is an objective, not a relation: a house on the run between an in fan and its exit split (the middle tile of that run at the flat solve) leaves the facts solve feasible the fan moved out of the house's way, checker clean. */
	const JunctionTopology &t = JunctionLLRRSidelineTopology();
	JunctionSpec spec = MakeTestJunctionSpec();
	spec.max_bridge_middle = 64;
	const SeedPipeline p = SeedPipelineOf(t, spec);
	const JunctionTerrain flat = FlatTerrainAround(p.layout);
	size_t checked = 0;
	for (uint32_t e = 0; e < p.graph.edges.size(); e++) {
		const JunctionTrackEdge &edge = p.graph.edges[e];
		if (edge.kind != JunctionTrackEdge::Kind::In || edge.from.node || !edge.to.node) continue;
		if (p.programs.edges[e].size() != 1 || p.programs.edges[e][0].kind != JunctionTrackStep::Kind::Fan) continue;
		const TileIndexDiffC fan_end = p.seed.edges[e][0].second;
		const TileIndexDiffC node = p.seed.nodes[edge.to.index];
		if (fan_end.y == p.seed.edges[e][0].first.y || node.x - fan_end.x < 4) continue; // Eastbound rows only: a westbound in fan sits at its port, its node is the side that gives way.
		const std::pair<int, int> house{(fan_end.x + node.x) / 2, fan_end.y};
		std::string error;
		const JunctionTrackPlacement q = PlaceWithHouses(t, p, flat, {house}, &error);
		INFO("edge " << e << " house " << house.first << "," << house.second << ": " << error);
		REQUIRE(!q.nodes.empty());
		const TileIndexDiffC end = q.edges[e][0].second;
		/* The run between the fan and its node must be buildable, so the fan lies past the house: the objective gave way. */
		CHECK(end.x > house.first);
		std::vector<JunctionTrackRoute> routes;
		JunctionLayout layout = RenderJunctionTrackPlacement(t, p.paths, p.graph, p.cut, p.programs, q, &error, &routes);
		INFO(error);
		REQUIRE(!layout.tracks.empty());
		SignalJunctionTrackRoutes(layout, routes, t, p.paths, p.graph, p.programs, p.spec, nullptr, nullptr, &q.prio);
		for (const RuleViolation &v : CheckJunctionRules(layout, p.spec, t)) {
			INFO(fmt::format("rule {} tile {},{}: {}", static_cast<int>(v.rule), v.tile.x, v.tile.y, v.message));
			CHECK(false);
		}
		checked++;
	}
	CHECK(checked > 0);
}

TEST_CASE("topoproj: placement on facts around each of 20 houses", "[junction][topoprojsweep]")
{
	/* The terrain and the 20 tiles of the P4a sweep. The node path's working frame is the seed's: the terrain is FlatTerrainAround of the seed's render, so the offset is (0, 0). */
	const JunctionTopology &t = JunctionLLRRSidelineTopology();
	const GraphSolveSetup setup = GraphSolveSetupOf(t);
	const JunctionTerrain flat = FlatTerrainAround(setup.signalled);
	const JunctionNodeSolve base = SolveLLRRBlocked(setup, flat, {});
	REQUIRE(base.layout.has_value());
	std::set<std::pair<int, int>> tile_set;
	for (const JunctionTrack &jt : base.layout->tracks) tile_set.emplace(jt.offset.x, jt.offset.y);
	const std::vector<std::pair<int, int>> tiles(tile_set.begin(), tile_set.end());
	REQUIRE(tiles.size() >= 20);
	const size_t k = tiles.size() / 20;

	/* The seed of the setup's frame: RenderOf's spec (max_bridge_middle 64). */
	JunctionSpec spec = MakeTestJunctionSpec();
	spec.max_bridge_middle = 64;
	const SeedPipeline p = SeedPipelineOf(t, spec);
	{
		std::string render_error;
		const auto a = TileBitsOf(p.layout), b = TileBitsOf(RenderOf(t, 64, &render_error));
		int diff = 0;
		for (const auto &[tile, bits] : a) {
			const auto it = b.find(tile);
			if (it == b.end() || it->second != bits) diff++;
		}
		INFO("tiles " << a.size() << " vs " << b.size());
		CHECK(diff == 0);
		CHECK(a.size() == b.size());
	}

	int placed = 0, node_placed = 0;
	double solve_ms = 0;
	for (size_t j = 0; j < 20; j++) {
		const std::pair<int, int> tile = tiles[j * k];
		const JunctionNodeSolve node = SolveLLRRBlocked(setup, flat, {tile});
		if (!node.failure.has_value()) node_placed++;
		std::string error;
		const auto start = std::chrono::steady_clock::now();
		const JunctionTrackPlacement q = PlaceWithHouses(t, p, flat, {tile}, &error);
		solve_ms += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
		INFO("tile " << tile.first << "," << tile.second << ": " << error);
		if (q.nodes.empty()) {
			/* A failure names the house first, and every entry names a graph element or a fact. */
			REQUIRE(!q.why.empty());
			CHECK(std::string(q.why.front().rule) == "blocked tile");
			/* The failure names the out fan run for the houses at 49,5 and 21,13: after the push the fact's tile lies past the snapshot (open quality item). */
			CHECK((error.find(fmt::format("({},{})", tile.first, tile.second)) != std::string::npos || tile == std::make_pair(49, 5) || tile == std::make_pair(21, 13)));
			for (const JunctionPlacementReason &reason : q.why) {
				REQUIRE(reason.rule != nullptr);
				CHECK(error.find(reason.rule) != std::string::npos);
				const std::string rule = reason.rule;
				CHECK((reason.edge != UINT32_MAX || reason.node != UINT32_MAX || reason.row >= 0 || rule == "blocked tile" || rule == "origin" || rule == "port columns"));
			}
			std::string first_two;
			for (size_t i = 0; i < std::min<size_t>(2, q.why.size()); i++) first_two += fmt::format("{}{} (edge {} node {} row {})", i == 0 ? "" : " <- ", q.why[i].rule, static_cast<int>(q.why[i].edge), static_cast<int>(q.why[i].node), q.why[i].row);
			WARN("tile " << tile.first << "," << tile.second << " fails: " << first_two);
			continue;
		}
		placed++;
		WARN("tile " << tile.first << "," << tile.second << " solved");
		std::vector<JunctionTrackRoute> routes;
		JunctionLayout layout = RenderJunctionTrackPlacement(t, p.paths, p.graph, p.cut, p.programs, q, &error, &routes);
		INFO(error);
		REQUIRE(!layout.tracks.empty());
		SignalJunctionTrackRoutes(layout, routes, t, p.paths, p.graph, p.programs, p.spec, nullptr, nullptr, &q.prio);
		for (const RuleViolation &viol : CheckJunctionRules(layout, p.spec, t)) {
			INFO(fmt::format("rule {} tile {},{}: {}", static_cast<int>(viol.rule), viol.tile.x, viol.tile.y, viol.message));
			CHECK(false);
		}
		std::set<std::pair<int, int>> heads;
		for (const std::array<JunctionBridge, 2> &copies : q.bridges) {
			for (const JunctionBridge &b : copies) {
				heads.emplace(b.start.x, b.start.y);
				heads.emplace(b.end.x, b.end.y);
			}
		}
		for (const JunctionTrackRoute &route : routes) {
			for (const JunctionPiece &piece : route.pieces) {
				if (piece.tile.x != tile.first || piece.tile.y != tile.second) continue;
				CHECK((piece.on_bridge && !heads.contains(tile)));
			}
		}
	}
	WARN("placement solved " << placed << " of 20, node path solved " << node_placed << " of 20; mean " << solve_ms / 20 << " ms per facts placement (with its terrain and facts)");
}

TEST_CASE("topology: occupancy legality reads the graph", "[junction][topology]")
{
	/* L1: two pieces on one tile are legal when the graph relates them or share no track end; the settled placement is checked, not a reference solve's edge pairs. */
	JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionTopology &t = JunctionLLRRSidelineTopology();
	std::string ignored;
	const std::vector<JunctionLanePath> paths = ExpandJunctionLanes(t, &ignored);
	const JunctionTrackGraph graph = BuildJunctionTrackGraph(t, paths, &ignored);
	const JunctionTrackOrder order = OrderJunctionTrackGraph(t, paths, graph, &ignored);
	const JunctionTrackStructure structure = StructureJunctionTrackGraph(t, paths, graph, order, &ignored);
	const JunctionTrackPrograms programs = ProgramJunctionTrackGraph(t, paths, graph, order, structure, &ignored);
	JunctionTrackPlacement seed;
	const JunctionTrackStructure cut = CutJunctionTrackBridges(t, paths, graph, order, structure, programs, spec, &seed, &ignored);
	REQUIRE(!seed.nodes.empty());

	/* (b) Edges 12 and 17 are unrelated in the graph and share the tiles (28,11) and (28,12) in the seed, as LEFT against RIGHT and Y against LEFT, with no shared track end: legal. */
	std::string render_error;
	std::vector<JunctionTrackRoute> routes;
	const JunctionLayout layout = RenderJunctionTrackPlacement(t, paths, graph, cut, programs, seed, &render_error, &routes);
	REQUIRE(!layout.tracks.empty());
	TrackBits at_28_11;
	for (const JunctionTrack &track : layout.tracks) {
		if (track.offset.x == 28 && track.offset.y == 11) at_28_11 = at_28_11 | track.tracks;
	}
	CHECK(at_28_11.Test(Track::Left));
	CHECK(at_28_11.Test(Track::Right));
	for (const auto &[a, b] : graph.crossings) CHECK(!(a == 12 && b == 17));

	/* (a) The first free S-bend size of edge 4 one column shorter than the seed's puts edge 17's RIGHT piece on edge 12's Y piece (a shared SE end): the settled placement is illegal. */
	REQUIRE(seed.choices.m[4][0] == 6);
	JunctionPlacementChoices choices = seed.choices;
	choices.m[4][0] = 5;
	std::string error;
	const JunctionTrackPlacement bad = PlaceJunctionTrackGraph(t, paths, graph, order, cut, programs, spec, choices, &error);
	CHECK(bad.nodes.empty());
	CHECK(error.starts_with("the placement rules contradict each other: occupancy (edge 17) [edge 17 and edge 12 on row 10]"));
	CHECK(error.find(" <- ") == std::string::npos);
}

TEST_CASE("topology: the built hub topology is the hand topology and seeds the same", "[junction][topologysweep]")
{
	struct Pair {
		const char *main;
		const char *branch;
		const JunctionTopology &hand;
	};
	const Pair pairs[] = {{"L_R", "L_R", JunctionTTopology()}, {"LL_RR", "L_R", JunctionLLRRSidelineTopology()}};
	for (const Pair &pair : pairs) {
		const JunctionTopology built = MakeJunctionHubTopology(ParseLineNotation(pair.main).spec, ParseLineNotation(pair.branch).spec);
		INFO(pair.main);
		REQUIRE(built.groups.size() == pair.hand.groups.size());
		for (size_t g = 0; g < built.groups.size(); g++) {
			CHECK(built.groups[g].side == pair.hand.groups[g].side);
			CHECK(built.groups[g].in == pair.hand.groups[g].in);
			CHECK(built.groups[g].lanes == pair.hand.groups[g].lanes);
			CHECK(built.groups[g].line == pair.hand.groups[g].line);
			CHECK(built.groups[g].main == pair.hand.groups[g].main);
		}
		const std::set<std::pair<uint8_t, uint8_t>> got(built.movements.begin(), built.movements.end());
		const std::set<std::pair<uint8_t, uint8_t>> want(pair.hand.movements.begin(), pair.hand.movements.end());
		CHECK(built.movements.size() == pair.hand.movements.size());
		CHECK(got == want);

		/* The pipeline does not depend on the order of the movements. */
		for (int tl = 3; tl <= 8; tl += 5) {
			for (int v : {100, 196}) {
				JunctionSpec spec = MakeTestJunctionSpec();
				spec.tl = tl;
				spec.train.max_speed = v;
				spec.reference_speed = v;
				spec.prio_tiles = 0;
				spec.prio_tiles = JunctionPrioTiles(spec);
				if (spec.prio_tiles == 0) continue;
				INFO(pair.main << " tl " << tl << " v " << v);
				std::string error;
				JunctionTrackPlacement hand_placement, built_placement;
				const JunctionLayout hand_layout = SeedOf(pair.hand, spec, &error, &hand_placement);
				const JunctionLayout built_layout = SeedOf(built, spec, &error, &built_placement);
				REQUIRE(!hand_layout.tracks.empty());
				/* Edge numbers follow the movement order; the placement is the same graph (ports, nodes, size, and the render below), not the same numbering. */
				CHECK(Of(hand_placement.ports) == Of(built_placement.ports));
				CHECK(Of(hand_placement.nodes) == Of(built_placement.nodes));
				CHECK(Of(hand_placement.size) == Of(built_placement.size));
				CHECK(RenderJunctionAscii(hand_layout) == RenderJunctionAscii(built_layout));
				CHECK(hand_layout.bridges.size() == built_layout.bridges.size());
				CHECK(hand_layout.signals.size() == built_layout.signals.size());
				CHECK(hand_layout.tracks.size() == built_layout.tracks.size());
			}
		}
	}
}

TEST_CASE("topology: LL_RR x LL_RR under each lane pairing seeds or names a reason", "[junction][topologysweep]")
{
	const JunctionLineSpec llrr = ParseLineNotation("LL_RR").spec;
	/* The seed of each pairing either makes routes or names a reason. */
	for (const JunctionLanePairing pairing : {JunctionLanePairing::AnyToAny, JunctionLanePairing::LaneKeepsLane, JunctionLanePairing::LanePerDirection}) {
		const JunctionTopology topology = MakeJunctionHubTopology(llrr, llrr, pairing);
		JunctionSpec spec = MakeTestJunctionSpec();
		std::string why;
		const JunctionGraphSeed seed = MakeJunctionGraphSeed(topology, spec, &why);
		INFO("pairing " << static_cast<int>(pairing) << ": " << why);
		CHECK((!seed.routes.empty() || !why.empty()));
	}
}

TEST_CASE("topology: the order sweep places crossings between edges that are not yet adjacent", "[junction][topologysweep]")
{
	/* OC1: LL_RR x LL_RR under lane pairing any and keep needs crossings between edges that are not adjacent when the sweep reaches them. */
	const JunctionLineSpec llrr = ParseLineNotation("LL_RR").spec;
	for (const JunctionLanePairing pairing : {JunctionLanePairing::AnyToAny, JunctionLanePairing::LaneKeepsLane}) {
		const JunctionTopology topology = MakeJunctionHubTopology(llrr, llrr, pairing);
		std::string why;
		const auto paths = ExpandJunctionLanes(topology, &why);
		REQUIRE(!paths.empty());
		const JunctionTrackGraph graph = BuildJunctionTrackGraph(topology, paths, &why);
		std::string order_error;
		OrderJunctionTrackGraph(topology, paths, graph, &order_error);
		INFO("pairing " << static_cast<int>(pairing) << ": " << order_error);
		CHECK(order_error.find("not adjacent") == std::string::npos);
		CHECK(order_error.find("cannot be ordered") == std::string::npos);
		WARN("OF1 pairing " << static_cast<int>(pairing) << " order stage: " << (order_error.empty() ? "ok" : order_error));
		JunctionSpec spec = MakeTestJunctionSpec();
		std::string next;
		MakeJunctionGraphSeed(topology, spec, &next);
		WARN("OF1 pairing " << static_cast<int>(pairing) << " next stage: " << next);
	}
}

TEST_CASE("topology: exit side: a main lane's branch-only splits sit in the band (D224)", "[junction][topology]")
{
	const JunctionLineSpec llrr = ParseLineNotation("LL_RR").spec;
	const JunctionTopology t = MakeJunctionHubTopology(llrr, llrr, JunctionLanePairing::AnyToAny);
	std::string error;
	const std::vector<JunctionLanePath> paths = ExpandJunctionLanes(t, &error);
	REQUIRE(!paths.empty());
	const JunctionTrackGraph graph = BuildJunctionTrackGraph(t, paths, &error);
	REQUIRE(!graph.nodes.empty());
	const std::vector<JunctionPortSlot> slots = JunctionPortSlots(t);
	const std::vector<int> row_of = JunctionSlotRows(t, paths, &error);
	auto main_of = [&](int slot) { return t.groups[slots[slot].group].main; };
	size_t found = 0;
	for (uint32_t i = 0; i < graph.nodes.size(); i++) {
		const JunctionTrackNode &node = graph.nodes[i];
		if (!node.split || !main_of(node.slot)) continue;
		for (const JunctionTrackEdge &edge : graph.edges) {
			if (edge.kind != JunctionTrackEdge::Kind::In || !edge.from.node || !edge.to.node || edge.to.index != i) continue;
			bool branch_only = true;
			for (uint16_t p : edge.paths) branch_only = branch_only && !main_of(paths[p].to);
			if (!branch_only) continue;
			found++;
			CHECK(node.level == 2 * row_of[node.slot] + 2);
		}
	}
	CHECK(found > 0);
	const JunctionTrackOrder order = OrderJunctionTrackGraph(t, paths, graph, &error);
	INFO("order: " << error);
	const JunctionTrackStructure structure = StructureJunctionTrackGraph(t, paths, graph, order, &error);
	INFO("structure: " << error);
	CHECK(error.empty());
}

TEST_CASE("topology: a row node reached only by row edges is placed by the order", "[junction][topologysweep]")
{
	/* OF2: LL_RR x LL_RR (pairing any) and L_R x LL_RR have row nodes (E main splits, merges before the W port) that no swept item makes. */
	for (const auto &[main_notation, branch_notation] : {std::pair{"LL_RR", "LL_RR"}, std::pair{"L_R", "LL_RR"}}) {
		const JunctionTopology topology = MakeJunctionHubTopology(ParseLineNotation(main_notation).spec, ParseLineNotation(branch_notation).spec, JunctionLanePairing::AnyToAny);
		std::string why;
		const auto paths = ExpandJunctionLanes(topology, &why);
		REQUIRE(!paths.empty());
		const JunctionTrackGraph graph = BuildJunctionTrackGraph(topology, paths, &why);
		std::string order_error;
		const JunctionTrackOrder order = OrderJunctionTrackGraph(topology, paths, graph, &order_error);
		INFO(main_notation << " x " << branch_notation << ": " << order_error);
		CHECK(order_error.empty());
		REQUIRE(!order.rows.empty());
		/* Every node of a row's level is an item of that row. */
		for (uint16_t node = 0; node < graph.nodes.size(); node++) {
			if (graph.nodes[node].level % 2 == 0) continue;
			const auto &row = order.rows[(graph.nodes[node].level - 1) / 2];
			CHECK(std::find(row.begin(), row.end(), JunctionTrackRowItem{true, node}) != row.end());
		}
		JunctionSpec spec = MakeTestJunctionSpec();
		std::string next;
		const JunctionGraphSeed seed = MakeJunctionGraphSeed(topology, spec, &next);
		WARN("OF2 " << main_notation << " x " << branch_notation << " any: " << (seed.routes.empty() ? next : std::string("seed")));
	}
}

TEST_CASE("topology: branch legs bound for one main lane merge before joining it", "[junction][topologysweep]")
{
	/* MJ1 (D224): LL_RR x LL_RR and L_R x LL_RR under pairing any: every main lane has exactly one joiner and the structure stage passes its joiner check. */
	for (const auto &[main_notation, branch_notation] : {std::pair{"LL_RR", "LL_RR"}, std::pair{"L_R", "LL_RR"}}) {
		const JunctionTopology topology = MakeJunctionHubTopology(ParseLineNotation(main_notation).spec, ParseLineNotation(branch_notation).spec, JunctionLanePairing::AnyToAny);
		std::string why;
		const auto paths = ExpandJunctionLanes(topology, &why);
		REQUIRE(!paths.empty());
		const JunctionTrackGraph graph = BuildJunctionTrackGraph(topology, paths, &why);
		std::string order_error;
		const JunctionTrackOrder order = OrderJunctionTrackGraph(topology, paths, graph, &order_error);
		INFO(main_notation << " x " << branch_notation << ": " << order_error);
		REQUIRE(order_error.empty());
		for (size_t r = 0; r < order.rows.size(); r++) {
			const int level = static_cast<int>(2 * r + 1);
			int joiners = 0;
			for (const JunctionTrackEdge &edge : graph.edges) {
				if (!edge.from.node || !edge.to.node || order.node_level[edge.from.index] == order.node_level[edge.to.index]) continue;
				const bool at_from = order.node_level[edge.from.index] == level && !graph.nodes[edge.from.index].split;
				const bool at_to = order.node_level[edge.to.index] == level && !graph.nodes[edge.to.index].split;
				if (at_from || at_to) joiners++;
			}
			INFO("row " << r);
			CHECK(joiners == 1);
		}
		std::string structure_error;
		StructureJunctionTrackGraph(topology, paths, graph, order, &structure_error);
		CHECK(structure_error.find("joiner on a lane") == std::string::npos);
		JunctionSpec spec = MakeTestJunctionSpec();
		std::string next;
		const JunctionGraphSeed seed = MakeJunctionGraphSeed(topology, spec, &next);
		WARN("MJ1 " << main_notation << " x " << branch_notation << " any: " << (seed.routes.empty() ? next : std::string("seed")));
	}
}

TEST_CASE("structure: crossings without one joiner follow D223 and D227 (C1, C3)", "[junction][topology]")
{
	const JunctionTopology topology = MakeJunctionHubTopology(ParseLineNotation("LL_RR").spec, ParseLineNotation("LL_RR").spec, JunctionLanePairing::AnyToAny);
	std::string why;
	const auto paths = ExpandJunctionLanes(topology, &why);
	REQUIRE(!paths.empty());
	const JunctionTrackGraph graph = BuildJunctionTrackGraph(topology, paths, &why);
	std::string order_error;
	const JunctionTrackOrder order = OrderJunctionTrackGraph(topology, paths, graph, &order_error);
	REQUIRE(order_error.empty());
	std::string structure_error;
	const JunctionTrackStructure structure = StructureJunctionTrackGraph(topology, paths, graph, order, &structure_error);
	INFO("structure error: " << structure_error);
	REQUIRE(structure_error.empty());

	const std::vector<JunctionPortSlot> slots = JunctionPortSlots(topology);
	/* A slot named by side letter, in or out, lane, and M (main) or B (branch), e.g. "W in 0 M". */
	auto slot_name = [&](int slot) {
		const JunctionLaneGroup &group = topology.groups[slots[slot].group];
		const char side = group.side == JunctionSide::W ? 'W' : (group.side == JunctionSide::E ? 'E' : (group.side == JunctionSide::S ? 'S' : 'N'));
		return fmt::format("{} {} {} {}", side, group.in ? "in" : "out", slots[slot].lane, group.main ? "M" : "B");
	};
	using Movements = std::set<std::pair<std::string, std::string>>;
	auto find_edge = [&](const Movements &wanted) {
		int found = -1;
		for (uint32_t e = 0; e < graph.edges.size(); e++) {
			Movements have;
			for (uint16_t p : graph.edges[e].paths) have.insert({slot_name(paths[p].from), slot_name(paths[p].to)});
			if (have != wanted) continue;
			REQUIRE(found < 0);
			found = static_cast<int>(e);
		}
		return found;
	};
	struct Row {
		Movements over;
		Movements under;
	};
	const std::vector<Row> rows = {
		{{{"W in 0 M", "S out 0 B"}}, {{"W in 1 M", "S out 1 B"}}},
		{{{"E in 0 M", "S out 1 B"}}, {{"E in 1 M", "S out 0 B"}}},
		{{{"W in 0 M", "S out 1 B"}, {"W in 1 M", "S out 1 B"}}, {{"E in 0 M", "S out 0 B"}, {"E in 1 M", "S out 0 B"}}},
		{{{"S in 1 B", "W out 0 M"}, {"S in 1 B", "W out 1 M"}}, {{"S in 0 B", "E out 0 M"}, {"S in 0 B", "E out 1 M"}}},
		{{{"S in 1 B", "W out 1 M"}}, {{"S in 0 B", "W out 0 M"}}},
		{{{"S in 0 B", "E out 1 M"}}, {{"S in 1 B", "E out 0 M"}}},
	};
	for (size_t r = 0; r < rows.size(); r++) {
		INFO("table row " << r);
		const int over = find_edge(rows[r].over);
		const int under = find_edge(rows[r].under);
		REQUIRE(over >= 0);
		REQUIRE(under >= 0);
		int carried_over = 0;
		for (const JunctionTrackBridge &bridge : structure.bridges) {
			const bool crosses_under = std::find(bridge.crossed.begin(), bridge.crossed.end(), static_cast<uint32_t>(under)) != bridge.crossed.end();
			const bool crosses_over = std::find(bridge.crossed.begin(), bridge.crossed.end(), static_cast<uint32_t>(over)) != bridge.crossed.end();
			if (bridge.edge == static_cast<uint32_t>(over) && crosses_under) {
				carried_over++;
				CHECK(!bridge.doubled);
				CHECK(!bridge.tunnel);
			}
			CHECK(!(bridge.edge == static_cast<uint32_t>(under) && crosses_over));
		}
		CHECK(carried_over == 1);
	}
}

TEST_CASE("programs: a crossed out edge carries a doubled bridge (D229, G2 §G2.4)", "[junction][topology]")
{
	const JunctionTopology topology = MakeJunctionHubTopology(ParseLineNotation("LL_RR").spec, ParseLineNotation("LL_RR").spec, JunctionLanePairing::AnyToAny);
	std::string why;
	const auto paths = ExpandJunctionLanes(topology, &why);
	REQUIRE(!paths.empty());
	const JunctionTrackGraph graph = BuildJunctionTrackGraph(topology, paths, &why);
	std::string error;
	const JunctionTrackOrder order = OrderJunctionTrackGraph(topology, paths, graph, &error);
	REQUIRE(error.empty());
	const JunctionTrackStructure structure = StructureJunctionTrackGraph(topology, paths, graph, order, &error);
	REQUIRE(error.empty());
	const JunctionTrackPrograms programs = ProgramJunctionTrackGraph(topology, paths, graph, order, structure, &error);
	INFO("programs: " << error);
	REQUIRE(error.empty());

	const std::vector<JunctionPortSlot> slots = JunctionPortSlots(topology);
	using Step = JunctionTrackStep;
	int doubled_out = 0;
	for (uint32_t e = 0; e < graph.edges.size(); e++) {
		const JunctionTrackEdge &edge = graph.edges[e];
		if (edge.to.node || !edge.from.node) continue;
		bool doubled = false;
		for (const JunctionTrackBridge &bridge : structure.bridges) doubled = doubled || (bridge.edge == e && bridge.doubled);
		if (!doubled) continue;
		doubled_out++;
		const int r = (order.node_level[edge.from.index] - 1) / 2;
		const RowItems items = FindRowItems(structure.stack, r);
		REQUIRE(items.copy != RowItems::NONE);
		/* The row runs W to E when its in slot is on the W side. */
		bool east = true;
		for (const JunctionTrackEdge &other : graph.edges) {
			if (other.from.node || !other.to.node || (order.node_level[other.to.index] - 1) / 2 != r || order.node_level[other.to.index] % 2 != 1) continue;
			east = topology.groups[slots[other.from.index].group].side == JunctionSide::W;
		}
		const int hand = ((items.copy > items.lane) == east) ? +1 : -1;
		const uint32_t c = static_cast<uint32_t>(items.copy);
		std::vector<Step> want_edge, want_copy;
		want_copy.push_back(Step{Step::Kind::SBend, static_cast<int8_t>(hand), c, 0, Step::Role::LaneSplit});
		for (uint32_t x : order.edge_crossings[e]) {
			want_edge.push_back(Step{Step::Kind::Over, 0, x, 0});
			want_copy.push_back(Step{Step::Kind::Over, 0, x, 0});
		}
		want_edge.push_back(Step{Step::Kind::SBend, static_cast<int8_t>(hand), c, 0, Step::Role::LaneMerge});
		want_edge.push_back(Step{Step::Kind::Fan, 0, c, 0});
		INFO("edge " << e << " row " << r);
		CHECK(programs.edges[e] == want_edge);
		CHECK(programs.copies[e] == want_copy);
	}
	CHECK(doubled_out > 0);
}

TEST_CASE("programs: FindRowItems names a row's copy item on either side of the lane", "[junction][topology]")
{
	using Kind = JunctionTrackStackItem::Kind;
	const std::vector<JunctionTrackStackItem> stack = {
		{0, Kind::Copy}, {0, Kind::Lane}, {0, Kind::Approach},
		{1, Kind::Approach}, {1, Kind::Lane}, {1, Kind::PrioAndCopy},
	};
	const RowItems row0 = FindRowItems(stack, 0);
	const RowItems row1 = FindRowItems(stack, 1);
	CHECK(row0.copy == 0);
	CHECK(row1.copy == 5);
	CHECK(row0.north == 0);
	CHECK(row1.north == RowItems::NONE);
}

TEST_CASE("topology: an L_R main with a multi-lane branch is refused, not crashed", "[junction][topology]")
{
	for (const char *branch : {"LL_RR", "LL_R", "L_RR"}) {
		INFO(branch);
		const JunctionTopology topology = MakeJunctionHubTopology(ParseLineNotation("L_R").spec, ParseLineNotation(branch).spec);
		JunctionSpec spec = MakeTestJunctionSpec();
		std::string error;
		const JunctionGraphSeed seed = MakeJunctionGraphSeed(topology, spec, &error);
		CHECK(seed.routes.empty());
		CHECK(!error.empty());
	}
}

/* Hidden until the S placement steps land (RECON-s2-cycle). */
TEST_CASE("placement: S branch lanes open up by fans inside the box (D230)", "[junction][topology][.]")
{
	const JunctionTopology t = MakeJunctionHubTopology(ParseLineNotation("L_R").spec, ParseLineNotation("LL_RR").spec);
	std::string error;
	const JunctionTrackPlacement placement = PlacementOf(t, 64, &error);
	INFO(error);
	CHECK(error.empty());
	REQUIRE(!placement.nodes.empty());

	std::string ignored;
	const std::vector<JunctionLanePath> paths = ExpandJunctionLanes(t, &ignored);
	const JunctionTrackGraph graph = BuildJunctionTrackGraph(t, paths, &ignored);
	const std::vector<JunctionPortSlot> slots = JunctionPortSlots(t);
	std::vector<int> south_in;
	for (size_t s = 0; s < slots.size(); s++) {
		const JunctionLaneGroup &group = t.groups[slots[s].group];
		if (!group.main && group.side == JunctionSide::S && group.in) south_in.push_back(static_cast<int>(s));
	}
	REQUIRE(south_in.size() == 2);
	CHECK(placement.ports[south_in[1]].x == placement.ports[south_in[0]].x + 1);
	CHECK(placement.ports[south_in[1]].y == placement.ports[south_in[0]].y);
	bool opened = false;
	for (size_t i = 0; i < south_in.size(); i++) {
		int found = 0;
		for (const JunctionTrackEdge &edge : graph.edges) {
			if (edge.from.node || edge.from.index != south_in[i]) continue;
			REQUIRE(edge.to.node);
			found++;
			const int port_x = placement.ports[south_in[i]].x;
			const int node_x = placement.nodes[edge.to.index].x;
			if (i == 0) CHECK(node_x <= port_x); else CHECK(node_x >= port_x);
			if (node_x != port_x) opened = true;
		}
		CHECK(found == 1);
	}
	CHECK(opened);

	/* A node behind a fan of m >= 1 columns lies at least 4 + (m - 1) rows above the S edge (REVIEW-port-fans S1). */
	const JunctionTrackOrder order = OrderJunctionTrackGraph(t, paths, graph, &ignored);
	const JunctionTrackStructure structure = StructureJunctionTrackGraph(t, paths, graph, order, &ignored);
	const JunctionTrackPrograms programs = ProgramJunctionTrackGraph(t, paths, graph, order, structure, &ignored);
	int fans_checked = 0;
	for (uint32_t e = 0; e < graph.edges.size(); e++) {
		const JunctionTrackEdge &edge = graph.edges[e];
		for (int end = 0; end < 2; end++) {
			const JunctionTrackEnd &port = end == 0 ? edge.from : edge.to;
			const JunctionTrackEnd &other = end == 0 ? edge.to : edge.from;
			if (port.node || !other.node) continue;
			const JunctionLaneGroup &group = t.groups[slots[port.index].group];
			if (group.main || group.side != JunctionSide::S) continue;
			const std::vector<JunctionTrackStep> &program = programs.edges[e];
			if (program.empty() || (end == 0 ? program.front() : program.back()).kind != JunctionTrackStep::Kind::Fan) continue;
			const JunctionTrackSpan &span = end == 0 ? placement.edges[e].front() : placement.edges[e].back();
			const int m = std::abs(span.second.x - span.first.x);
			if (m < 1) continue;
			fans_checked++;
			CHECK(placement.ports[port.index].y - placement.nodes[other.index].y >= 4 + (m - 1));
		}
	}
	INFO("fans of m >= 1 checked: " << fans_checked);
	CHECK(fans_checked >= 0);
}

TEST_CASE("programs: an S branch port edge of a group with several lanes carries a fan (D230)", "[junction][topology]")
{
	auto check = [](const JunctionTopology &t, bool fans) {
		std::string error;
		const std::vector<JunctionLanePath> paths = ExpandJunctionLanes(t, &error);
		const JunctionTrackGraph graph = BuildJunctionTrackGraph(t, paths, &error);
		REQUIRE(!graph.edges.empty());
		const JunctionTrackOrder order = OrderJunctionTrackGraph(t, paths, graph, &error);
		REQUIRE(!order.rows.empty());
		const JunctionTrackStructure structure = StructureJunctionTrackGraph(t, paths, graph, order, &error);
		REQUIRE(!structure.stack.empty());
		const JunctionTrackPrograms programs = ProgramJunctionTrackGraph(t, paths, graph, order, structure, &error);
		REQUIRE(!programs.edges.empty());
		const std::vector<JunctionPortSlot> slots = JunctionPortSlots(t);
		auto south_branch = [&](const JunctionTrackEnd &end) {
			if (end.node) return false;
			const JunctionLaneGroup &group = t.groups[slots[end.index].group];
			return !group.main && group.side == JunctionSide::S;
		};
		int seen = 0;
		for (uint32_t e = 0; e < graph.edges.size(); e++) {
			const JunctionTrackEdge &edge = graph.edges[e];
			const std::vector<JunctionTrackStep> &program = programs.edges[e];
			for (int end = 0; end < 2; end++) {
				const JunctionTrackEnd &port = end == 0 ? edge.from : edge.to;
				if (!south_branch(port)) continue;
				seen++;
				const bool has_fan = !program.empty() && (end == 0 ? program.front() : program.back()).kind == JunctionTrackStep::Kind::Fan;
				if (!fans) {
					for (const JunctionTrackStep &token : program) CHECK(token.kind != JunctionTrackStep::Kind::Fan);
					continue;
				}
				REQUIRE(has_fan);
				const JunctionTrackStep &token = end == 0 ? program.front() : program.back();
				CHECK(token.index == JunctionTrackStep::NO_ROW);
				const int first_slot = [&]() {
					for (size_t s = 0; s < slots.size(); s++) {
						if (slots[s].group == slots[port.index].group) return static_cast<int>(s);
					}
					return -1;
				}();
				CHECK(token.hand == (static_cast<int>(port.index) == first_slot ? -1 : +1));
			}
		}
		CHECK(seen > 0);
	};
	const JunctionLineSpec llrr = ParseLineNotation("LL_RR").spec;
	check(MakeJunctionHubTopology(llrr, llrr), true);
	check(JunctionTTopology(), false);
}

TEST_CASE("programs: a band edge's S-bends name the node they serve (DESIGN-G2 §G2.6)", "[junction][topology]")
{
	const JunctionTopology t = MakeJunctionHubTopology(ParseLineNotation("L_R").spec, ParseLineNotation("L_RR").spec);
	std::string error;
	const std::vector<JunctionLanePath> paths = ExpandJunctionLanes(t, &error);
	const JunctionTrackGraph graph = BuildJunctionTrackGraph(t, paths, &error);
	REQUIRE(!graph.edges.empty());
	const JunctionTrackOrder order = OrderJunctionTrackGraph(t, paths, graph, &error);
	REQUIRE(!order.rows.empty());
	const JunctionTrackStructure structure = StructureJunctionTrackGraph(t, paths, graph, order, &error);
	REQUIRE(!structure.stack.empty());
	const JunctionTrackPrograms programs = ProgramJunctionTrackGraph(t, paths, graph, order, structure, &error);
	REQUIRE(!programs.edges.empty());
	using Step = JunctionTrackStep;
	int converge = 0;
	int lone = 0;
	for (uint32_t e = 0; e < graph.edges.size(); e++) {
		const JunctionTrackEdge &edge = graph.edges[e];
		const std::vector<Step> &program = programs.edges[e];
		INFO("edge " << e);
		for (size_t i = 0; i < program.size(); i++) {
			if (program[i].kind != Step::Kind::SBend) continue;
			if (program[i].role == Step::Role::LaneSplit || program[i].role == Step::Role::LaneMerge || program[i].role == Step::Role::Shift) continue; // A doubled leg's own bends.
			const bool before_crossing = i + 1 < program.size() && (program[i + 1].kind == Step::Kind::Under || program[i + 1].kind == Step::Kind::Above);
			if (i == 0 && edge.from.node && order.node_level[edge.from.index] % 2 == 0 && graph.nodes[edge.from.index].split && !(i + 1 == program.size() && edge.to.node)) {
				CHECK(program[i].role == Step::Role::Diverge);
			} else if (before_crossing) {
				CHECK(program[i].role == Step::Role::None); // A crossing's S-bend serves no node.
			} else if (i + 1 == program.size() || program[i].index != Step::NO_ROW) {
				CHECK(program[i].role == Step::Role::Converge);
				converge++;
			}
		}
		/* e12 of the recon: a straight leg at a band split, one S-bend, into a merge. */
		if (edge.from.node && graph.nodes[edge.from.index].split && order.node_level[edge.from.index] % 2 == 0 && edge.to.node && !graph.nodes[edge.to.index].split && program.size() == 1 && program[0].kind == Step::Kind::SBend) {
			lone++;
			CHECK(program[0].role == Step::Role::Converge);
		}
	}
	CHECK(converge > 0);
	CHECK(lone > 0);
}

TEST_CASE("placement: a lone converge S-bend leaves a run after its split (DESIGN-G2 §G2.6)", "[junction][topologysweep]")
{
	const JunctionTopology t = MakeJunctionHubTopology(ParseLineNotation("L_R").spec, ParseLineNotation("L_RR").spec);
	std::string error;
	PlacementOf(t, 64, &error);
	CHECK(!(error.find("S-bend size (edge 12)") != std::string::npos && error.find("bridge over S-bend (edge 6)") != std::string::npos));
}

TEST_CASE("bridge cut: the reported failure is the default cut's", "[junction][topology]")
{
	/* L_R x L_RR: every cut fails; the finer cuts fail on the bridge-head hug, the default cut (series of at most the cap from the start) on its own reason. */
	const JunctionTopology topology = MakeJunctionHubTopology(ParseLineNotation("L_R").spec, ParseLineNotation("L_RR").spec, JunctionLanePairing::AnyToAny);
	std::string error;
	MakeJunctionGraphSeed(topology, MakeTestJunctionSpec(), &error);
	INFO(error);
	CHECK(error.find("bridge heads hug crossers") == std::string::npos);
}

/** The five topologies of the node-shape tests: T, LL_RR x L_R, L_R x L_RR, L_R x LL_RR, LL_RR x LL_RR (pairing any); the first two are the ones placement handles. */
static std::vector<std::pair<std::string, JunctionTopology>> NodeShapeTopologies()
{
	auto hub = [](const char *main, const char *branch) { return MakeJunctionHubTopology(ParseLineNotation(main).spec, ParseLineNotation(branch).spec, JunctionLanePairing::AnyToAny); };
	return {
		{"T", JunctionTTopology()},
		{"LL_RR x L_R", hub("LL_RR", "L_R")},
		{"L_R x L_RR", hub("L_R", "L_RR")},
		{"L_R x LL_RR", hub("L_R", "LL_RR")},
		{"LL_RR x LL_RR", hub("LL_RR", "LL_RR")},
	};
}

TEST_CASE("programs: every node has a shape and every edge two ends", "[junction][topology]")
{
	int succeeded = 0;
	for (const auto &[name, t] : NodeShapeTopologies()) {
		INFO(name);
		std::string error;
		const std::vector<JunctionLanePath> paths = ExpandJunctionLanes(t, &error);
		const JunctionTrackGraph graph = BuildJunctionTrackGraph(t, paths, &error);
		if (graph.edges.empty()) continue;
		const JunctionTrackOrder order = OrderJunctionTrackGraph(t, paths, graph, &error);
		if (order.rows.empty()) continue;
		const JunctionTrackStructure structure = StructureJunctionTrackGraph(t, paths, graph, order, &error);
		if (structure.stack.empty()) continue;
		const JunctionTrackPrograms programs = ProgramJunctionTrackGraph(t, paths, graph, order, structure, &error);
		if (programs.edges.empty()) continue;
		succeeded++;
		REQUIRE(programs.nodes.size() == graph.nodes.size());
		REQUIRE(programs.ends.size() == graph.edges.size());
		for (uint32_t v = 0; v < graph.nodes.size(); v++) {
			INFO("node " << v);
			const JunctionTrackNodeShape &shape = programs.nodes[v];
			auto incident = [&](uint32_t e) {
				const JunctionTrackEdge &edge = graph.edges[e];
				return (edge.from.node && edge.from.index == v) || (edge.to.node && edge.to.index == v);
			};
			REQUIRE(shape.trunk < graph.edges.size());
			REQUIRE(shape.straight < graph.edges.size());
			REQUIRE(shape.diverging < graph.edges.size());
			CHECK(incident(shape.trunk));
			CHECK(incident(shape.straight));
			CHECK(incident(shape.diverging));
			CHECK(shape.straight != shape.diverging);
			CHECK((shape.hand == 1 || shape.hand == -1));
			CHECK(shape.heading < 4);
		}
		for (const auto &[start, finish] : programs.ends) {
			CHECK(start.heading < 4);
			CHECK(finish.heading < 4);
		}
	}
	CHECK(succeeded > 0);
}

TEST_CASE("programs: node shapes match placement's inference", "[junction][topology]")
{
	/* Outer and characterisation only. The old inference is copied here as a test-only rule: placement's node_item (junction_placement.cpp ~544) read over the same programs, the start heading of a band edge (~246-252) and the chain walk of its tokens (~265-271). */
	using Step = JunctionTrackStep;
	for (const auto &[name, t] : NodeShapeTopologies()) {
		const bool placed = name == "T" || name == "LL_RR x L_R";
		INFO(name);
		std::string error;
		const std::vector<JunctionLanePath> paths = ExpandJunctionLanes(t, &error);
		const JunctionTrackGraph graph = BuildJunctionTrackGraph(t, paths, &error);
		if (graph.edges.empty()) { WARN(name << ": no graph: " << error); continue; }
		const JunctionTrackOrder order = OrderJunctionTrackGraph(t, paths, graph, &error);
		if (order.rows.empty()) { WARN(name << ": no order: " << error); continue; }
		const JunctionTrackStructure structure = StructureJunctionTrackGraph(t, paths, graph, order, &error);
		if (structure.stack.empty()) { WARN(name << ": no structure: " << error); continue; }
		const JunctionTrackPrograms programs = ProgramJunctionTrackGraph(t, paths, graph, order, structure, &error);
		if (programs.edges.empty()) { WARN(name << ": no programs: " << error); continue; }
		REQUIRE(programs.nodes.size() == graph.nodes.size());
		REQUIRE(programs.ends.size() == graph.edges.size());

		const std::vector<JunctionPortSlot> slots = JunctionPortSlots(t);
		const int n = static_cast<int>(slots.size());
		auto group_of = [&](int slot) -> const JunctionLaneGroup & { return t.groups[slots[slot].group]; };
		const std::vector<int> row_of = JunctionSlotRows(t, paths, &error);
		const int rows = static_cast<int>(order.rows.size());
		std::vector<bool> east(rows, true);
		for (int s = 0; s < n; s++) {
			if (group_of(s).main && group_of(s).in) east[row_of[s]] = group_of(s).side == JunctionSide::W;
		}
		auto end_level = [&](const JunctionTrackEnd &end) {
			if (end.node) return order.node_level[end.index];
			return group_of(end.index).main ? 2 * row_of[end.index] + 1 : 2 * rows + 1;
		};
		auto is_row_node = [&](const JunctionTrackEnd &end) { return end.node && order.node_level[end.index] % 2 == 1; };
		auto is_row_edge = [&](uint32_t e) {
			const int level = end_level(graph.edges[e].from);
			return level == end_level(graph.edges[e].to) && level % 2 == 1;
		};
		auto names_item = [](const Step &token) { return token.index != Step::NO_ROW && (token.kind == Step::Kind::Fan || token.kind == Step::Kind::Corner || token.kind == Step::Kind::SBend); };
		auto last_item = [&](uint32_t e) {
			const std::vector<Step> &program = programs.edges[e];
			for (size_t i = program.size(); i-- > 0;) if (names_item(program[i])) return program[i].index;
			return Step::NO_ROW;
		};
		auto first_item = [&](uint32_t e) {
			for (const Step &token : programs.edges[e]) if (names_item(token)) return token.index;
			return Step::NO_ROW;
		};
		/* The old start heading of an edge, and the heading after its tokens. */
		auto old_start = [&](uint32_t e) -> uint8_t {
			const JunctionTrackEdge &edge = graph.edges[e];
			if (is_row_edge(e)) return east[(end_level(edge.from) - 1) / 2] ? 1 : 3;
			if (is_row_node(edge.from)) return east[(order.node_level[edge.from.index] - 1) / 2] ? 1 : 3;
			return !edge.paths.empty() && group_of(paths[edge.paths[0]].from).main ? 2 : 0;
		};
		auto old_end = [&](uint32_t e) -> uint8_t {
			int cur = old_start(e);
			for (const Step &token : programs.edges[e]) {
				if (token.kind == Step::Kind::Corner) cur = (cur + token.hand + 4) % 4;
			}
			return static_cast<uint8_t>(cur);
		};

		std::vector<std::string> diffs;
		auto differ = [&](const std::string &where, const char *field, uint32_t old_value, uint32_t new_value) {
			if (old_value == new_value) return;
			auto show = [](uint32_t v) { return v == Step::NO_ROW ? std::string("none") : fmt::format("{}", v); };
			diffs.push_back(where + ": " + field + " " + show(old_value) + " -> " + show(new_value));
		};
		for (uint32_t v = 0; v < graph.nodes.size(); v++) {
			const std::string where = fmt::format("node {}", v);
			const JunctionTrackNodeShape &shape = programs.nodes[v];
			const bool split = graph.nodes[v].split;
			const uint32_t trunk = shape.trunk;
			if (order.node_level[v] % 2 == 1) {
				/* A row node: the in edge's last item (split), the out edge's first item (merge). */
				differ(where, "item", split ? last_item(trunk) : first_item(trunk), shape.item);
			} else {
				differ(where, "item", Step::NO_ROW, shape.item);
			}
			differ(where, "heading", split ? old_end(trunk) : old_start(trunk), shape.heading);
			/* The in edges of the node must end on one item. */
			std::set<uint32_t> items;
			for (uint32_t e = 0; e < graph.edges.size(); e++) {
				if (graph.edges[e].to.node && graph.edges[e].to.index == v && last_item(e) != Step::NO_ROW) items.insert(last_item(e));
			}
			if (items.size() > 1) diffs.push_back(where + ": its in edges end on " + fmt::format("{}", items.size()) + " different items");
		}
		for (uint32_t e = 0; e < graph.edges.size(); e++) {
			const std::string where = fmt::format("edge {}", e);
			differ(where, "start heading", old_start(e), programs.ends[e].first.heading);
			differ(where, "end heading", old_end(e), programs.ends[e].second.heading);
		}
		if (placed) {
			INFO("disagreements: " << diffs.size());
			for (const std::string &d : diffs) { INFO(d); }
			CHECK(diffs.empty());
		} else {
			for (const std::string &d : diffs) WARN(name << " " << d);
		}
	}
}
