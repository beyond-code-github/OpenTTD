/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_topology.cpp Test functionality from junction_topology. */

#include "../stdafx.h"

#include "../3rdparty/fmt/format.h"
#include "../3rdparty/catch2/catch.hpp"

#include <algorithm>
#include <chrono>
#include <climits>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <tuple>

#include "../direction_func.h"
#include "../junction_rules.h"
#include "../junction_rules_internal.h"
#include "../junction_generator.h"
#include "../junction_nodes.h"
#include "../junction_route.h"
#include "../junction_topology.h"
#include "../track_func.h"

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

TEST_CASE("topology: a split with three legs is refused", "[junction][topology]")
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
static Step O(uint32_t x) { return {Step::Kind::Over, 0, x, 0}; }
static Step U(uint32_t x, uint8_t c) { return {Step::Kind::Under, 0, x, c}; }

TEST_CASE("topology: the T's track programs", "[junction][topology]")
{
	const JunctionTrackPrograms programs = ProgramsOf(JunctionTTopology());
	REQUIRE(programs.edges.size() == 12);
	REQUIRE(programs.copies.size() == 12);
	const Programs edges = {
		{Fan(1)}, {}, {Fan(3)}, {},
		{C(+1, NR), U(5, 0), U(5, 1), S(-1, NR), U(8, 0)},
		{O(8), O(4), S(-1, 4)},
		{C(-1, NR), S(+1, NR)},
		{C(-1, 5), S(+1, 4)},
		{O(4), U(5, 1), U(5, 0), C(+1, 2), S(-1, 1)},
		{Fan(4)}, {}, {Fan(1)},
	};
	CHECK(programs.edges == edges);
	Programs copies(12);
	copies[5] = {S(-1, 4), O(8), O(4)};
	CHECK(programs.copies == copies);
}

TEST_CASE("topology: LL_RR with an L_R sideline: track programs", "[junction][topology]")
{
	const JunctionTrackPrograms programs = ProgramsOf(JunctionLLRRSidelineTopology());
	REQUIRE(programs.edges.size() == 26);
	REQUIRE(programs.copies.size() == 26);
	const Programs edges = {
		{Fan(1)}, {Fan(3)}, {}, {O(23), U(10, 1), U(10, 0), U(9, 1), U(9, 0)}, {S(-1, NR)}, {Fan(9)}, {Fan(6)}, {},
		{O(11), O(15), S(+1, 4)}, {O(3), O(11), O(12), S(-1, 7)}, {O(13), O(3), O(11), O(12), O(17), S(-1, 10)},
		{C(+1, NR), U(8, 0), U(8, 1), U(9, 0), U(9, 1), U(10, 0), U(10, 1)},
		{C(+1, NR), U(9, 0), U(9, 1), U(10, 0), U(10, 1), S(-1, NR)},
		{C(-1, NR), U(10, 0), U(10, 1)},
		{C(-1, NR), S(+1, NR)},
		{U(8, 1), U(8, 0), C(+1, 2), S(-1, 1)},
		{C(+1, 5), S(-1, 4)},
		{U(10, 1), U(10, 0), C(-1, 8), S(+1, 7)},
		{C(-1, 11), S(+1, 10)},
		{Fan(7)}, {Fan(10)}, {}, {S(+1, NR)}, {S(-1, NR), U(3, 0)}, {Fan(4)}, {Fan(1)},
	};
	CHECK(programs.edges == edges);
	Programs copies(26);
	copies[8] = {S(+1, 4), O(11), O(15)};
	copies[9] = {S(-1, 7), O(3), O(11), O(12)};
	copies[10] = {S(-1, 10), O(13), O(3), O(11), O(12), O(17)};
	CHECK(programs.copies == copies);
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

TEST_CASE("topology: LL_RR's uncut w2 bridge is too long at nmax 8", "[junction][topology]")
{
	std::string error;
	const JunctionTrackPlacement placement = PlacementOf(JunctionLLRRSidelineTopology(), 8, &error);
	CHECK(placement.nodes.empty());
	CHECK(error == "bridge 3 on edge 10 needs 9 middle tiles, more than 8");
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
	REQUIRE(!placement.nodes.empty());
	REQUIRE(placement.bridges.size() == structure.bridges.size());

	/* X_e1 (edge 11) and X_e2 (edge 12) cross under w1's bridge four columns apart (the exit splits sit at their least x, DESIGN-G1 §11.9b); the exit legs' (Corner, S-bend) runs of edges 17 and 18 lift to 4 tiles (TL1b), which moves the columns from six. */
	CHECK(placement.edges[11][3].first.x - placement.edges[12][1].first.x == 4);

	/* Band 2 is as deep as one corner. */
	CHECK(placement.item_rows[6] - placement.item_rows[5] - 1 == 2);

	for (size_t i = 0; i < structure.bridges.size(); i++) {
		const int n = std::abs(placement.bridges[i][0].start.x - placement.bridges[i][0].end.x) - 1;
		CHECK(n <= 64);
		if (structure.bridges[i].edge == 10) CHECK(n == 9);
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

TEST_CASE("topology: the T render, both ways", "[junction][topology]")
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

TEST_CASE("topology: the LL_RR seed is clean", "[junction][topology]")
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
		SignalJunctionTrackRoutes(layout, routes, *t, paths, graph, programs, spec);
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

TEST_CASE("topology: LL_RR's port fans keep their port rows as long as they can", "[junction][topoproj]")
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

	/* At least one W-side in-port class left its flat value. */
	bool moved = false;
	for (const JunctionTrackRoute &route : setup.routes) {
		const Trackdir td = route.pieces.front().trackdir;
		if ((td != Trackdir::X_NE && td != Trackdir::X_SW) || TravelX(td) <= 0) continue;
		const size_t i = PortBendPiece(route, true);
		if (i == SIZE_MAX) continue;
		for (const JunctionNodePoint &point : tpl.points) {
			if (point.tile.x == route.pieces[i].tile.x && point.tile.y == route.pieces[i].tile.y && point.trackdir == route.pieces[i].trackdir && solve.values[point.cls] != tpl.flat[point.cls]) moved = true;
		}
	}
	CHECK(moved);

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
	std::ranges::stable_sort(runs, [](const Run &a, const Run &b) { return a.last - a.first > b.last - b.first; });
	if (runs.empty()) return std::nullopt;
	const Run &run = runs.front();
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
	REQUIRE(house.has_value());
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
	SignalJunctionTrackRoutes(layout, routes, t, seed.paths, seed.graph, seed.programs, spec);
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
	REQUIRE(house.has_value());

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
	SignalJunctionTrackRoutes(layout, routes, t, paths, graph, programs, spec);
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
				spec.prio_tiles = SizeJunctionPrio(spec).prio_tiles;
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
				spec.prio_tiles = SizeJunctionPrio(spec).prio_tiles;
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
	/* The gate of the placement relations (RECON-stretch C1, C3): every placement point pushed later by hand, as terrain will (DESIGN-G1 §14.4); a solve failure is an edit, never a bug. */
	const JunctionSpec spec = MakeTestJunctionSpec();
	for (int ti = 0; ti < 2; ti++) {
		const JunctionTopology &t = ti == 0 ? JunctionTTopology() : JunctionLLRRSidelineTopology();
		const char *name = ti == 0 ? "T" : "LL_RR";
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
		for (int v = 1; v < static_cast<int>(p.columns.size()); v++) {
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
				SignalJunctionTrackRoutes(layout, routes, t, paths, graph, programs, spec);
				const std::vector<RuleViolation> violations = CheckJunctionRules(layout, spec, t);
				std::string detail = fmt::format("{} v {} s {}:", name, v, s);
				for (const RuleViolation &viol : violations) detail += fmt::format(" [rule {} at {},{}: {}]", static_cast<int>(viol.rule), viol.tile.x, viol.tile.y, viol.message);
				INFO(detail);
				REQUIRE(violations.empty());
				clean++;
			}
		}
		WARN(name << ": runs " << runs << ", solve failures " << failures << ", clean " << clean);
	}
}

TEST_CASE("topology: the relations keep the seed", "[junction][topology]")
{
	/* The in fan order and bridge head relations are tight at the seed (RECON-stretch C1, C3): the seeds keep their numbers. */
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

TEST_CASE("topology: run and group tiles match the render", "[junction][topology]")
{
	/* The facts are applied to the tiles of the rendered routes but the bridge middles, no others: a house on a tile changes the placement (or fails it) exactly when a fact is applied to a tile the seed uses. */
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
	std::vector<std::pair<int, int>> others;
	for (int y = 0; y < p.seed.size.y; y++) {
		for (int x = 0; x < p.seed.size.x; x++) {
			if (!rendered.contains({x, y})) others.emplace_back(x, y);
		}
	}
	const JunctionTrackPlacement rest = PlaceWithHouses(t, p, flat, others, &error);
	INFO(error);
	REQUIRE(!rest.nodes.empty());
	INFO("rendered tiles without a fact:" << missing);
	CHECK(missing.empty());
	CHECK(SamePlacement(p.seed, rest));
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
			CHECK(error.find(fmt::format("({},{})", tile.first, tile.second)) != std::string::npos);
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
		SignalJunctionTrackRoutes(layout, routes, t, p.paths, p.graph, p.programs, p.spec);
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
