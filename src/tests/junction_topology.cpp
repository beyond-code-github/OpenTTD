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
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <tuple>

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

TEST_CASE("topology: LL_RR's uncut w2 bridge is too long at nmax 9", "[junction][topology]")
{
	std::string error;
	const JunctionTrackPlacement placement = PlacementOf(JunctionLLRRSidelineTopology(), 9, &error);
	CHECK(placement.nodes.empty());
	CHECK(error == "bridge 3 on edge 10 needs 10 middle tiles, more than 9");
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

	/* X_e1 (edge 11) and X_e2 (edge 12) cross under w1's bridge six columns apart (the exit splits sit at their least x, DESIGN-G1 §11.9b). */
	CHECK(placement.edges[11][3].first.x - placement.edges[12][1].first.x == 6);

	/* Band 2 is as deep as one corner. */
	CHECK(placement.item_rows[6] - placement.item_rows[5] - 1 == 2);

	for (size_t i = 0; i < structure.bridges.size(); i++) {
		const int n = std::abs(placement.bridges[i][0].start.x - placement.bridges[i][0].end.x) - 1;
		CHECK(n <= 64);
		if (structure.bridges[i].edge == 10) CHECK(n == 10);
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
	JunctionLayout layout = RenderOf(t, 64, error, nullptr, nullptr, nullptr, &paths, nullptr, routes);
	if (paths_out != nullptr) *paths_out = paths;
	*tokens = SignalJunctionTrackRoutes(layout, *routes, t, paths, MakeTestJunctionSpec());
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
