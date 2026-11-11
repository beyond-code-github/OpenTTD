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
#include "../junction_generator.h"
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
	CHECK(Of(placement.size) == T(40, 14));
	CHECK(Of(placement.ports) == std::vector<Tile>{T(0, 1), T(0, 3), T(25, 13), T(27, 13), T(39, 3), T(39, 1)});
	CHECK(Of(placement.nodes) == std::vector<Tile>{T(22, 1), T(25, 11), T(32, 3), T(6, 4), T(27, 11), T(37, 1)});
	const std::vector<Spans> edges = {
		{Sp(0, 1, 0, 1)}, {}, {Sp(39, 3, 39, 3)}, {},
		{Sp(22, 1, 24, 3), Sp(23, 3, 24, 3), Sp(24, 4, 24, 4), Sp(24, 7, 27, 9), Sp(25, 7, 25, 8)},
		{Sp(27, 3, 26, 3), Sp(24, 3, 23, 3), Sp(21, 3, 21, 4)},
		{Sp(32, 3, 30, 5), Sp(30, 9, 27, 11)},
		{Sp(25, 11, 19, 5), Sp(6, 5, 6, 4)},
		{Sp(25, 8, 25, 7), Sp(25, 4, 26, 4), Sp(26, 3, 27, 3), Sp(25, 4, 27, 2), Sp(37, 2, 37, 1)},
		{Sp(2, 4, 2, 3)}, {}, {Sp(39, 1, 39, 1)},
	};
	CHECK(Of(placement.edges) == edges);
	std::vector<Spans> copies(12);
	copies[5] = {Sp(30, 3, 30, 4), Sp(26, 4, 25, 4), Sp(24, 4, 24, 4)};
	CHECK(Of(placement.copies) == copies);
	REQUIRE(placement.bridges.size() == 2);
	CHECK(Of(placement.bridges[0]) == Br(28, 3, 22, 3, 29, 4, 23, 4));
	CHECK(Of(placement.bridges[1]) == Br(25, 9, 25, 6, 0, 0, 0, 0));
}

TEST_CASE("topology: LL_RR's w2 bridge is too long at nmax 9", "[junction][topology]")
{
	std::string error;
	const JunctionTrackPlacement placement = PlacementOf(JunctionLLRRSidelineTopology(), 9, &error);
	CHECK(placement.nodes.empty());
	CHECK(error == "bridge 10 needs 10 middle tiles, more than 9");
}

TEST_CASE("topology: LL_RR with an L_R sideline: placement", "[junction][topology]")
{
	std::string error;
	JunctionTrackStructure structure;
	const JunctionTrackPlacement placement = PlacementOf(JunctionLLRRSidelineTopology(), 64, &error, &structure);
	REQUIRE(!placement.nodes.empty());
	REQUIRE(placement.bridges.size() == structure.bridges.size());

	/* X_e1 (edge 11) and X_e2 (edge 12) cross under w1's bridge four columns apart. */
	CHECK(placement.edges[11][3].first.x - placement.edges[12][1].first.x == 4);

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
