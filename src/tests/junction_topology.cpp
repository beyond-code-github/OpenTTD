/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_topology.cpp Test functionality from junction_topology. */

#include "../stdafx.h"

#include "../3rdparty/catch2/catch.hpp"

#include "../junction_rules.h"
#include "../junction_rules_internal.h"
#include "../junction_topology.h"

#include "../safeguards.h"

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

TEST_CASE("topology: multi-to-multi is refused", "[junction][topology]")
{
	JunctionTopology t = JunctionLLRRSidelineTopology();
	t.groups[3].lanes = 2;
	std::string error;
	CHECK(ExpandJunctionLanes(t, &error).empty());
	CHECK(error == "multi-to-multi movements are not ruled yet");
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

	const std::vector<JunctionTrackNode> nodes = {{true, 0}, {true, 2}, {true, 4}, {false, 1}, {false, 3}, {false, 5}};
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
		{true, 0}, {true, 1}, {true, 4}, {true, 4}, {true, 4}, {true, 6}, {true, 7},
		{false, 2}, {false, 3}, {false, 5}, {false, 5}, {false, 5}, {false, 8}, {false, 9},
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
	const Pairs crossings = {{3, 9}, {3, 10}, {3, 23}, {8, 11}, {8, 15}, {9, 23}, {10, 13}, {10, 17}, {10, 23}};
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

TEST_CASE("topology: a split with three legs is refused", "[junction][topology]")
{
	JunctionTopology t;
	t.groups = {
		{JunctionSide::W, true, 1, 0, true},
		{JunctionSide::S, false, 1, 1, false},
		{JunctionSide::E, false, 1, 0, true},
		{JunctionSide::N, false, 1, 2, false},
	};
	t.movements = {{0, 1}, {0, 2}, {0, 3}};
	std::string error;
	const std::vector<JunctionLanePath> paths = ExpandJunctionLanes(t, &error);
	REQUIRE(paths.size() == 3);
	const JunctionTrackGraph graph = BuildJunctionTrackGraph(t, paths, &error);
	CHECK(graph.nodes.empty());
	CHECK(graph.edges.empty());
	CHECK(error == "more than two legs at one split are not ruled yet");
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
}

TEST_CASE("topology: LL_RR with an L_R sideline: track order", "[junction][topology]")
{
	std::string error;
	const JunctionTrackOrder order = OrderOf(JunctionLLRRSidelineTopology(), &error);
	REQUIRE(!order.rows.empty());
	CHECK(order.node_level == std::vector<int>{1, 3, 8, 4, 8, 7, 5, 5, 7, 8, 8, 4, 3, 1});
	using Row = std::vector<JunctionTrackRowItem>;
	CHECK(order.rows == std::vector<Row>{
		{RN(0), RN(13)},
		{RN(1), RE(11), RE(15), RN(12)},
		{RN(7), RE(23), RE(3), RN(6)},
		{RN(8), RE(17), RE(23), RE(3), RE(13), RN(5)},
	});
	std::vector<std::vector<uint32_t>> crossings(26);
	crossings[3] = {23, 10, 9};
	crossings[8] = {11, 15};
	crossings[9] = {3, 23};
	crossings[10] = {13, 3, 23, 17};
	crossings[11] = {8};
	crossings[13] = {10};
	crossings[15] = {8};
	crossings[17] = {10};
	crossings[23] = {9, 10, 3};
	CHECK(order.edge_crossings == crossings);
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
	const JunctionTrackOrder order = OrderOf(t, &error);
	CHECK(order.rows.empty());
	CHECK(error == "branch ports off the S side are not ruled yet");
}
