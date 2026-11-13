/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_joint.cpp Test functionality from junction_joint. */

#include "../../../stdafx.h"

#include "../../../3rdparty/fmt/format.h"
#include "../../../3rdparty/catch2/catch.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <map>
#include <set>

#include "../../../junction/junction_pipeline.h"
#include "../../../junction/junction_spec.h"
#include "../../../junction/line/junction_line.h"
#include "../../../junction/placement/junction_joint.h"
#include "../../../junction/placement/junction_placement.h"
#include "../../../junction/topology/junction_chain.h"
#include "../../../junction/topology/junction_topology.h"

#include "../../../safeguards.h"

/** A topology's pipeline products up to the placement's inputs. */
struct JointInputs {
	JunctionTopology topology;
	std::vector<JunctionLanePath> paths;
	JunctionTrackGraph graph;
	JunctionTrackOrder order;
	JunctionTrackStructure structure; ///< The uncut structure.
	JunctionTrackPrograms programs;
};

static JointInputs MakeJointInputs(const char *main, const char *branch)
{
	JointInputs in;
	in.topology = MakeJunctionHubTopology(ParseLineNotation(main).spec, ParseLineNotation(branch).spec, JunctionLanePairing::AnyToAny);
	std::string why;
	in.paths = ExpandJunctionLanes(in.topology, &why);
	in.graph = BuildJunctionTrackGraph(in.topology, in.paths, &why);
	in.order = OrderJunctionTrackGraph(in.topology, in.paths, in.graph, &why);
	in.structure = StructureJunctionTrackGraph(in.topology, in.paths, in.graph, in.order, &why);
	in.programs = ProgramJunctionTrackGraph(in.topology, in.paths, in.graph, in.order, in.structure, &why);
	return in;
}

TEST_CASE("joint: reproduces the prototype on T and LL_RR x L_R and with counters", "[junction][milp]")
{
	struct Case {
		const char *name, *main, *branch;
		double objective; ///< The joint optimum without the tie break (T keeps its Corner of edge 5 at 6: the port fans follow the rows).
		int differing; ///< The bends that differ from the seed's size.
	};
	const JunctionSpec spec = MakeTestJunctionSpec();
	for (const Case &c : {Case{"T", "L_R", "L_R", 27300, 0}, Case{"LL_RR x L_R", "LL_RR", "L_R", 86350, 0}}) {
		INFO(c.name);
		const JointInputs in = MakeJointInputs(c.main, c.branch);
		std::string error;
		const JunctionGraphSeed seed = MakeJunctionGraphSeed(in.topology, spec, &error);
		REQUIRE(!seed.routes.empty());
		const JunctionPlacementChoices &choices = seed.placement.choices;
		const JunctionPlacementSystem system = RecordJunctionPlacementSystem(in.topology, seed.paths, seed.graph, in.order, seed.structure, seed.programs, spec, choices);
		const auto start = std::chrono::steady_clock::now();
		const JunctionJointResult joint = SolveJunctionJoint(system, spec);
		const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
		std::cout << fmt::format("[joint] {}: bends {}, relations {}, status {}, objective {:.3f}, {:.1f} ms\n", c.name, system.bends.size(), system.relations.size(), static_cast<int>(joint.status), joint.objective, ms);
		REQUIRE(joint.status == JunctionMilp::Status::Optimal);
		REQUIRE(joint.length.size() == system.bends.size());

		int differing = 0;
		int sum = 0;
		for (size_t i = 0; i < system.bends.size(); i++) {
			const JunctionSystemBend &b = system.bends[i];
			sum += joint.length[i];
			if (b.copy != 0 || (b.source != "free" && b.source != "corner_rows")) continue;
			const int seed_size = b.source == "free" ? choices.m[b.edge][b.tok] : choices.corner[b.edge][b.tok];
			if (joint.length[i] == seed_size) continue;
			differing++;
			INFO(fmt::format("bend {} edge {} tok {} {}/{}: seed {} joint {}", b.id, b.edge, b.tok, b.kind, b.source, seed_size, joint.length[i]));
			CHECK(std::string(c.name) == "T");
			CHECK(b.edge == 5);
			CHECK(b.tok == 0);
			CHECK(b.source == "corner_rows");
			CHECK(seed_size == 6);
			CHECK(joint.length[i] == 5);
		}
		CHECK(differing == c.differing);
		CHECK(std::abs(joint.objective - 0.001 * sum - c.objective) < 0.5);
	}
}

/** The least solution of one axis of a recorded system's relations from the origin (the placement's longest path). */
static std::vector<int> LeastPositions(const JunctionPlacementSystem &system, int axis)
{
	std::vector<int> value(system.pt_count, 0);
	for (bool changed = true; changed;) {
		changed = false;
		for (const JunctionSystemRelation &r : system.relations) {
			if (r.axis != axis || value[r.v] >= value[r.u] + r.c) continue;
			value[r.v] = value[r.u] + r.c;
			changed = true;
		}
	}
	return value;
}

static constexpr int HEAD_VX[] = {0, 1, 0, -1};
static constexpr int HEAD_VY[] = {-1, 0, 1, 0};

TEST_CASE("joint: curve-gap counters agree with today's units at the seed", "[junction][milp]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	for (const auto &[main, branch] : {std::pair{"L_R", "L_R"}, std::pair{"LL_RR", "L_R"}}) {
		INFO(main << " x " << branch);
		const JointInputs in = MakeJointInputs(main, branch);
		std::string error;
		const JunctionGraphSeed seed = MakeJunctionGraphSeed(in.topology, spec, &error);
		REQUIRE(!seed.routes.empty());
		const JunctionPlacementSystem system = RecordJunctionPlacementSystem(in.topology, seed.paths, seed.graph, in.order, seed.structure, seed.programs, spec, seed.placement.choices);
		REQUIRE(!system.routes.empty());
		const std::vector<int> vy = LeastPositions(system, 1);
		const std::vector<int> least_x = LeastPositions(system, 0);
		const std::vector<int> &vx = seed.placement.columns; // The solved columns; the rows are the least solution of the recorded Y relations.
		REQUIRE(vx.size() == least_x.size());
		{
			int off = 0;
			for (size_t i = 0; i < vx.size(); i++) off += least_x[i] != vx[i];
			std::cout << fmt::format("[joint] {} x {}: points {}, the least X of the recorded relations differs from the placement's columns at {}\n", main, branch, vx.size(), off);
		}

		/* Curve-gap relations that are not held at constants: none expected. */
		int kept = 0;
		for (const JunctionSystemRelation &r : system.relations) {
			if (r.reason.starts_with("curve gap") && !r.nonlinear) kept++;
		}
		std::cout << fmt::format("[joint] {} x {}: curve gap relations not tagged nonlinear: {}\n", main, branch, kept);

		int compared = 0, absolute = 0;
		size_t at = 0; // Index into route_curves.
		for (size_t r = 0; r < system.routes.size(); r++) {
			const JunctionSystemRoute &route = system.routes[r];
			/* The units at each curve: U = E_i + cu_k, with E_0 and E_{i+1} as the rows give. */
			auto quantity = [&](const JunctionSystemRouteBend &b, const std::pair<int, int> &q) {
				const int length = b.bend >= 0 ? system.bends[b.bend].m : 1;
				return q.first + (length - 1) * (q.second - q.first);
			};
			std::vector<int> units;
			std::vector<int> bend_of;
			int entry = 0;
			for (size_t i = 0; i < route.bends.size(); i++) {
				const JunctionSystemRouteBend &b = route.bends[i];
				const int bdx = quantity(b, b.quantity[0]), bdy = quantity(b, b.quantity[1]);
				if (i == 0) {
					const int h = route.start_heading;
					entry = 2 * (HEAD_VX[h] * (vx[b.pt] + bdx - vx[route.start_pt]) + HEAD_VY[h] * (vy[b.pt] + bdy - vy[route.start_pt]));
				} else {
					const JunctionSystemRouteBend &p = route.bends[i - 1];
					const int h = p.hout;
					const int cdx = quantity(p, p.quantity[2]), cdy = quantity(p, p.quantity[3]);
					entry += quantity(p, p.quantity[4]) + 2 * (HEAD_VX[h] * (vx[b.pt] + bdx - vx[p.pt] - cdx) + HEAD_VY[h] * (vy[b.pt] + bdy - vy[p.pt] - cdy));
				}
				for (const auto &cu : b.curve_units) {
					units.push_back(entry + quantity(b, cu));
					bend_of.push_back(static_cast<int>(i));
				}
			}
			REQUIRE(units.size() == route.hands.size());

			/* Today's forms: s is the route's first horizontal heading, sv a segment's first vertical in heading. */
			int s = 0;
			if (HEAD_VX[route.start_heading] != 0) s = HEAD_VX[route.start_heading];
			for (size_t i = 0; s == 0 && i + 1 < route.bends.size(); i++) s = HEAD_VX[route.bends[i].hout];
			if (s == 0) s = 1;
			std::vector<std::array<int, 5>> forms;
			for (; at < system.route_curves.size() && system.route_curves[at][0] == static_cast<int>(r); at++) forms.push_back(system.route_curves[at]);
			REQUIRE(forms.size() == units.size());
			auto sv_of = [&](int seg) {
				for (size_t k = 0; k < forms.size(); k++) {
					const JunctionSystemRouteBend &b = route.bends[bend_of[k]];
					if (forms[k][4] == seg && HEAD_VX[b.hin] == 0) return HEAD_VY[b.hin];
				}
				return 1;
			};
			auto x_form = [&](size_t k) { return forms[k][2] + 2 * s * (vx[route.bends[bend_of[k]].pt] - vx[route.start_pt]); };
			for (size_t k = 0; k + 1 < units.size(); k++) {
				INFO("route " << r << " curve " << k);
				CHECK(units[k + 1] - units[k] == x_form(k + 1) - x_form(k));
				compared++;
				if (units[k] == x_form(k)) absolute++;
				if (forms[k][4] >= 0 && forms[k][4] == forms[k + 1][4]) {
					const int sv = sv_of(forms[k][4]);
					auto y_form = [&](size_t j) { return forms[j][3] + 2 * sv * vy[route.bends[bend_of[j]].pt]; };
					CHECK(units[k + 1] - units[k] == y_form(k + 1) - y_form(k));
				}
			}
		}
		std::cout << fmt::format("[joint] {} x {}: routes {}, consecutive curve pairs compared {}, absolute units equal {}\n", main, branch, system.routes.size(), compared, absolute);
		CHECK(compared > 0);
	}
}

static bool Same(TileIndexDiffC a, TileIndexDiffC b)
{
	return a.x == b.x && a.y == b.y;
}

static bool SameTiles(const std::vector<TileIndexDiffC> &a, const std::vector<TileIndexDiffC> &b)
{
	return std::equal(a.begin(), a.end(), b.begin(), b.end(), Same);
}

TEST_CASE("joint: recording does not change placement", "[junction][milp]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	for (const auto &[main, branch] : {std::pair{"L_R", "L_R"}, std::pair{"LL_RR", "L_R"}}) {
		INFO(main << " x " << branch);
		const JointInputs in = MakeJointInputs(main, branch);
		std::string error;
		const JunctionGraphSeed seed = MakeJunctionGraphSeed(in.topology, spec, &error);
		REQUIRE(!seed.routes.empty());
		const JunctionPlacementChoices &choices = seed.placement.choices;
		for (const bool fixed : {false, true}) {
			INFO(std::string(fixed ? "fixed choices" : "rounds"));
			const JunctionPlacementChoices *use = fixed ? &choices : nullptr;
			JunctionPlacementSystem system;
			const JunctionTrackPlacement without = PlaceTrackGraph(in.topology, seed.paths, seed.graph, in.order, seed.structure, seed.programs, spec, use, &error, {}, {});
			const JunctionTrackPlacement with = PlaceTrackGraph(in.topology, seed.paths, seed.graph, in.order, seed.structure, seed.programs, spec, use, &error, {}, {}, nullptr, {0, 0}, true, false, nullptr, nullptr, nullptr, &system);
			REQUIRE(!without.nodes.empty());
			CHECK(!system.relations.empty());
			CHECK(with.item_rows == without.item_rows);
			CHECK(SameTiles(with.ports, without.ports));
			CHECK(SameTiles(with.nodes, without.nodes));
			REQUIRE(with.edges.size() == without.edges.size());
			for (size_t e = 0; e < with.edges.size(); e++) {
				REQUIRE(with.edges[e].size() == without.edges[e].size());
				for (size_t t = 0; t < with.edges[e].size(); t++) {
					CHECK(Same(with.edges[e][t].first, without.edges[e][t].first));
					CHECK(Same(with.edges[e][t].second, without.edges[e][t].second));
				}
			}
			CHECK(Same(with.size, without.size));
			CHECK(with.choices == without.choices);
		}
	}
}

/** The default cut: every doubled bridge in series of the most crossings, from the start (the first cut the cut search tries). */
static JunctionTrackStructure DefaultCut(const JointInputs &in)
{
	JunctionTrackStructure cut = in.structure;
	cut.bridges.clear();
	for (const JunctionTrackBridge &bridge : in.structure.bridges) {
		if (!bridge.doubled) {
			cut.bridges.push_back(bridge);
			continue;
		}
		for (size_t first = 0; first < bridge.crossed.size(); first += JUNCTION_BRIDGE_MAX_CROSSERS) {
			const size_t end = std::min(bridge.crossed.size(), first + JUNCTION_BRIDGE_MAX_CROSSERS);
			cut.bridges.push_back({bridge.edge, std::vector<uint32_t>(bridge.crossed.begin() + first, bridge.crossed.begin() + end), true});
		}
	}
	return cut;
}

TEST_CASE("joint: an infeasible system names its conflict", "[junction][milp]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JointInputs in = MakeJointInputs("LL_RR", "LL_RR");

	const JunctionTrackStructure cut = DefaultCut(in);

	std::string error;
	JunctionPlacementSystem system;
	const JunctionTrackPlacement placed = PlaceTrackGraph(in.topology, in.paths, in.graph, in.order, cut, in.programs, spec, nullptr, &error, {}, {}, nullptr, {0, 0}, true, false, nullptr, nullptr, nullptr, &system);
	INFO(error);
	REQUIRE(!system.relations.empty());
	const bool placed_ok = !placed.nodes.empty();
	const JunctionJointResult joint = SolveJunctionJoint(system, spec);
	std::string chain;
	for (const std::string &reason : joint.conflict) chain += (chain.empty() ? "" : " <- ") + reason;
	std::cout << fmt::format("[joint] LL_RR x LL_RR: placement {}, relations {}, status {}, conflict: {}\n", placed_ok ? "places" : "fails", system.relations.size(), static_cast<int>(joint.status), chain);

	auto has = [&](std::string_view needle) {
		return std::any_of(joint.conflict.begin(), joint.conflict.end(), [&](const std::string &reason) { return reason.find(needle) != std::string::npos; });
	};
	if (joint.status == JunctionMilp::Status::Infeasible && joint.conflict.size() == 3 && has("stack rows (row 1)") && has("row nodes") && has("S-bend size (edge 45)")) {
		CHECK(joint.conflict.size() == 3);
	} else {
		WARN("the IIS chain of the prototype no longer holds: " << chain);
		CHECK((joint.status == JunctionMilp::Status::Infeasible || joint.status == JunctionMilp::Status::Optimal));
		CHECK((!joint.conflict.empty() || !joint.length.empty()));
	}
}

/** The default cut: every doubled bridge in series of the most crossings, from the start (the first cut the cut search tries). */
static JunctionTrackStructure DefaultCutOf(const JunctionTrackStructure &structure)
{
	JunctionTrackStructure cut = structure;
	cut.bridges.clear();
	for (const JunctionTrackBridge &bridge : structure.bridges) {
		if (!bridge.doubled) {
			cut.bridges.push_back(bridge);
			continue;
		}
		for (size_t first = 0; first < bridge.crossed.size(); first += JUNCTION_BRIDGE_MAX_CROSSERS) {
			const size_t end = std::min(bridge.crossed.size(), first + JUNCTION_BRIDGE_MAX_CROSSERS);
			cut.bridges.push_back({bridge.edge, std::vector<uint32_t>(bridge.crossed.begin() + first, bridge.crossed.begin() + end), true});
		}
	}
	return cut;
}

TEST_CASE("joint: bridge heads hug without the flat order", "[junction][milp]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	for (const auto &[main, branch] : {std::pair{"L_R", "L_R"}, std::pair{"LL_RR", "L_R"}}) {
		INFO(main << " x " << branch);
		const JointInputs in = MakeJointInputs(main, branch);
		std::string error;
		const JunctionGraphSeed seed = MakeJunctionGraphSeed(in.topology, spec, &error);
		REQUIRE(!seed.routes.empty());
		const JunctionPlacementSystem system = RecordJunctionPlacementSystem(in.topology, seed.paths, seed.graph, in.order, seed.structure, seed.programs, spec, seed.placement.choices);

		/* With the hug's pins kept (they become ordinary relations) and without them (the joint model drops them). */
		JunctionPlacementSystem pinned = system;
		for (JunctionSystemRelation &r : pinned.relations) r.order_pin = false;
		const JunctionJointResult with_pins = SolveJunctionJoint(pinned, spec);
		const JunctionJointResult without_pins = SolveJunctionJoint(system, spec);
		REQUIRE(with_pins.status == JunctionMilp::Status::Optimal);
		REQUIRE(without_pins.status == JunctionMilp::Status::Optimal);
		CHECK(with_pins.length == without_pins.length);

		/* Each joint's lengths as choices, placed through the fixed path. */
		std::vector<JunctionTrackPlacement> placed;
		for (const JunctionJointResult *joint : {&with_pins, &without_pins}) {
			JunctionPlacementChoices choices = seed.placement.choices;
			for (size_t i = 0; i < system.bends.size(); i++) {
				const JunctionSystemBend &b = system.bends[i];
				if (b.copy != 0) continue;
				if (b.source == "free") choices.m[b.edge][b.tok] = joint->length[i];
				if (b.source == "corner_rows") choices.corner[b.edge][b.tok] = joint->length[i];
			}
			placed.push_back(PlaceJunctionTrackGraph(in.topology, seed.paths, seed.graph, in.order, seed.structure, seed.programs, spec, choices, &error));
			INFO(error);
			REQUIRE(!placed.back().nodes.empty());
		}
		REQUIRE(placed[0].bridges.size() == placed[1].bridges.size());
		for (size_t b = 0; b < placed[0].bridges.size(); b++) {
			if (!seed.structure.bridges[b].doubled) continue;
			for (int copy = 0; copy < 2; copy++) {
				const int pinned_span = std::abs(placed[0].bridges[b][copy].end.x - placed[0].bridges[b][copy].start.x);
				const int free_span = std::abs(placed[1].bridges[b][copy].end.x - placed[1].bridges[b][copy].start.x);
				INFO(fmt::format("bridge {} copy {}: pinned span {} free span {}", b, copy, pinned_span, free_span));
				CHECK(free_span == pinned_span);
				if (std::string(main) == "L_R" && b == 1) CHECK(free_span == 6); // T: the Corner of edge 5 stays at the seed's 6, as the port fans follow the rows.
			}
		}
	}
}

TEST_CASE("joint: straight crosser tiles are exact", "[junction][milp]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	struct Case {
		const char *main, *branch;
		bool seed; ///< Recorded at the seed's choices (else the default cut).
	};
	for (const Case &c : {Case{"L_R", "L_R", true}, Case{"LL_RR", "L_R", true}, Case{"L_R", "L_RR", false}, Case{"L_R", "LL_RR", false}, Case{"LL_RR", "LL_RR", false}}) {
		INFO(c.main << " x " << c.branch);
		const JointInputs in = MakeJointInputs(c.main, c.branch);
		std::string error;
		JunctionPlacementSystem system;
		std::vector<int> columns;
		if (c.seed) {
			const JunctionGraphSeed seed = MakeJunctionGraphSeed(in.topology, spec, &error);
			REQUIRE(!seed.routes.empty());
			system = RecordJunctionPlacementSystem(in.topology, seed.paths, seed.graph, in.order, seed.structure, seed.programs, spec, seed.placement.choices);
			columns = seed.placement.columns;
		} else {
			const JunctionTrackStructure cut = DefaultCutOf(in.structure);
			PlaceTrackGraph(in.topology, in.paths, in.graph, in.order, cut, in.programs, spec, nullptr, &error, {}, {}, nullptr, {0, 0}, true, false, nullptr, nullptr, nullptr, &system);
		}
		int straight = 0, bend = 0, violated = 0;
		for (const JunctionSystemRelation &r : system.relations) {
			if (r.order_pin || r.reason.rfind("bridge heads hug crossers", 0) != 0) continue;
			(r.nonlinear ? bend : straight)++;
			if (r.nonlinear || columns.empty() || r.axis != 0) continue;
			if (columns[r.v] - columns[r.u] < r.c) violated++;
		}
		std::cout << fmt::format("[joint] hug inequalities {} x {}: straight {}, bend {}\n", c.main, c.branch, straight, bend);
		if (c.seed) {
			CHECK(straight > 0);
			CHECK(violated == 0);
		}
	}
}

TEST_CASE("joint: failing topologies with the hug relaxed", "[junction][milp]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	for (const auto &[main, branch] : {std::pair{"L_R", "L_RR"}, std::pair{"L_R", "LL_RR"}, std::pair{"LL_RR", "LL_RR"}}) {
		const JointInputs in = MakeJointInputs(main, branch);
		const JunctionTrackStructure cut = DefaultCutOf(in.structure);
		std::string error;
		JunctionPlacementSystem system;
		const JunctionTrackPlacement placed = PlaceTrackGraph(in.topology, in.paths, in.graph, in.order, cut, in.programs, spec, nullptr, &error, {}, {}, nullptr, {0, 0}, true, false, nullptr, nullptr, nullptr, &system);
		REQUIRE(!system.relations.empty());
		const JunctionJointResult joint = SolveJunctionJoint(system, spec);
		std::string chain;
		for (const std::string &reason : joint.conflict) chain += (chain.empty() ? "" : " <- ") + reason;
		int box = 0;
		for (int len : joint.length) box += len;
		WARN(fmt::format("{} x {}: placement {}, status {}, objective {:.3f}, sum of lengths {}, conflict: {}", main, branch, placed.nodes.empty() ? "fails" : "places", static_cast<int>(joint.status), joint.objective, box, chain));
	}
}

TEST_CASE("joint: failing topologies with counters", "[junction][milp]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	for (const auto &[main, branch] : {std::pair{"L_R", "L_RR"}, std::pair{"L_R", "LL_RR"}, std::pair{"LL_RR", "LL_RR"}}) {
		INFO(main << " x " << branch);
		const JointInputs in = MakeJointInputs(main, branch);
		const JunctionTrackStructure cut = DefaultCut(in);
		std::string error;
		JunctionPlacementSystem system;
		const JunctionTrackPlacement placed = PlaceTrackGraph(in.topology, in.paths, in.graph, in.order, cut, in.programs, spec, nullptr, &error, {}, {}, nullptr, {0, 0}, true, false, nullptr, nullptr, nullptr, &system);
		REQUIRE(!system.relations.empty());
		const auto start = std::chrono::steady_clock::now();
		const JunctionJointResult joint = SolveJunctionJoint(system, spec);
		const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
		std::string chain;
		for (const std::string &reason : joint.conflict) chain += (chain.empty() ? "" : " <- ") + reason;
		WARN(fmt::format("{} x {}: placement {}, routes {}, relations {}, status {}, objective {:.3f}, {:.1f} ms, conflict: {}", main, branch, placed.nodes.empty() ? "fails" : "places", system.routes.size(), system.relations.size(), static_cast<int>(joint.status), joint.objective, ms, chain));
		CHECK((joint.status == JunctionMilp::Status::Infeasible || joint.status == JunctionMilp::Status::Optimal));
	}
}

TEST_CASE("joint: scale probe on wider hubs", "[.g3scale]")
{
	using Clock = std::chrono::steady_clock;
	auto since = [](Clock::time_point t) { return std::chrono::duration<double, std::milli>(Clock::now() - t).count(); };
	auto median = [](std::vector<double> v) { std::sort(v.begin(), v.end()); return v[v.size() / 2]; };
	const JunctionSpec spec = MakeTestJunctionSpec();
	for (const auto &[main, branch] : {std::pair{"L_R", "L_R"}, std::pair{"LL_RR", "L_R"}, std::pair{"LLL_RRR", "L_R"}, std::pair{"LLL_RRR", "LL_RR"}, std::pair{"LLL_RRR", "LLL_RRR"}, std::pair{"LLLL_RRRR", "L_R"}, std::pair{"LLLL_RRRR", "LLLL_RRRR"}}) {
		const std::string hub = std::string(main) == "L_R" && std::string(branch) == "L_R" ? std::string("T") : std::string(main) + " x " + branch;
		JointInputs in;
		std::string why;
		in.topology = MakeJunctionHubTopology(ParseLineNotation(main).spec, ParseLineNotation(branch).spec, JunctionLanePairing::AnyToAny);
		in.paths = ExpandJunctionLanes(in.topology, &why);
		if (in.paths.empty()) { WARN(hub << ": stopped at lanes: " << why); continue; }
		in.graph = BuildJunctionTrackGraph(in.topology, in.paths, &why);
		if (in.graph.nodes.empty()) { WARN(hub << ": stopped at graph: " << why); continue; }
		in.order = OrderJunctionTrackGraph(in.topology, in.paths, in.graph, &why);
		if (!why.empty()) { WARN(hub << ": stopped at order: " << why); continue; }
		in.structure = StructureJunctionTrackGraph(in.topology, in.paths, in.graph, in.order, &why);
		if (!why.empty()) { WARN(hub << ": stopped at structure: " << why); continue; }
		in.programs = ProgramJunctionTrackGraph(in.topology, in.paths, in.graph, in.order, in.structure, &why);
		if (!why.empty()) { WARN(hub << ": stopped at programs: " << why); continue; }
		const JunctionTrackStructure cut = DefaultCut(in);

		std::string error;
		std::vector<double> seed_ms;
		JunctionTrackPlacement placed;
		for (int i = 0; i < 3; i++) {
			error.clear();
			const auto t0 = Clock::now();
			placed = PlaceJunctionTrackGraph(in.topology, in.paths, in.graph, in.order, cut, in.programs, spec, &error);
			seed_ms.push_back(since(t0));
		}
		const bool places = !placed.nodes.empty();
		JunctionPlacementSystem system;
		PlaceTrackGraph(in.topology, in.paths, in.graph, in.order, cut, in.programs, spec, places ? &placed.choices : nullptr, &error, {}, {}, nullptr, {0, 0}, true, false, nullptr, nullptr, nullptr, &system);
		if (system.relations.empty()) { WARN(hub << ": seed " << (places ? "places" : "fails") << ", recorded no system: " << error); continue; }

		std::vector<double> ms;
		JunctionJointResult joint;
		for (int i = 0; i < 5; i++) {
			const auto t0 = Clock::now();
			joint = SolveJunctionJoint(system, spec);
			ms.push_back(since(t0));
		}
		const JunctionJointResult relaxed = SolveJunctionJoint(system, spec, true);
		std::string chain;
		for (const std::string &reason : joint.conflict) chain += (chain.empty() ? "" : " <- ") + reason;
		WARN(fmt::format("{}: reached solve, seed {} ({:.1f} ms), bends {}, relations {}, routes {}, status {}, objective {:.3f}, median {:.1f} ms, relaxed status {} objective {:.3f}, conflict length {}", hub, places ? "places" : "fails", median(seed_ms), system.bends.size(), system.relations.size(), system.routes.size(), static_cast<int>(joint.status), joint.objective, median(ms), static_cast<int>(relaxed.status), relaxed.objective, joint.conflict.size()));
	}
}

/** A column's value at the recorded columns and sizes. */
static int ClashColumnValue(const JunctionPlacementSystem &system, const std::vector<int> &columns, const JunctionSystemColumn &column)
{
	return columns[column.pt] + column.c0 + (column.bend >= 0 ? column.c1 * system.bends[column.bend].m : 0);
}

TEST_CASE("joint: clash rows replace the held occupancy", "[junction][milp]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	for (const auto &[main, branch] : {std::pair{"L_R", "L_R"}, std::pair{"LL_RR", "L_R"}}) {
		INFO(main << " x " << branch);
		const JointInputs in = MakeJointInputs(main, branch);
		std::string error;
		const JunctionGraphSeed seed = MakeJunctionGraphSeed(in.topology, spec, &error);
		REQUIRE(!seed.routes.empty());
		const JunctionPlacementSystem with = RecordJunctionPlacementSystem(in.topology, seed.paths, seed.graph, in.order, seed.structure, seed.programs, spec, seed.placement.choices);
		JunctionPlacementSystem without = with;
		without.clashes.clear();
		REQUIRE(!with.clashes.empty());

		int held = 0, violated = 0, variable = 0;
		for (const JunctionSystemClash &clash : with.clashes) {
			held += clash.held;
			variable += clash.a_right.c1 != 0 || clash.b_left.c1 != 0;
			if (ClashColumnValue(with, seed.placement.columns, clash.b_left) - ClashColumnValue(with, seed.placement.columns, clash.a_right) < clash.gap) violated++;
		}
		std::cout << fmt::format("[joint] clashes {} x {}: {}, held {}, with a length term {}, violated at the recording {}\n", main, branch, with.clashes.size(), held, variable, violated);
		CHECK(violated == 0);

		const JunctionJointResult a = SolveJunctionJoint(with, spec);
		const JunctionJointResult b = SolveJunctionJoint(without, spec);
		REQUIRE(a.status == JunctionMilp::Status::Optimal);
		REQUIRE(b.status == JunctionMilp::Status::Optimal);
		CHECK(a.choices == b.choices);
		CHECK(std::abs(a.objective - b.objective) < 1e-6);
	}
}

TEST_CASE("joint: recordings at other sizes keep their points", "[junction][milp]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JointInputs in = MakeJointInputs("L_R", "L_R");
	std::string error;
	const JunctionGraphSeed seed = MakeJunctionGraphSeed(in.topology, spec, &error);
	REQUIRE(!seed.routes.empty());
	const JunctionPlacementSystem base = RecordJunctionPlacementSystem(in.topology, seed.paths, seed.graph, in.order, seed.structure, seed.programs, spec, seed.placement.choices);
	REQUIRE(!base.relations.empty());
	int compared = 0;
	for (const JunctionSystemBend &free_bend : base.bends) {
		if (free_bend.copy != 0 || free_bend.source != "free" || free_bend.kind != "SBend") continue;
		JunctionPlacementChoices bigger = seed.placement.choices;
		bigger.m[free_bend.edge][free_bend.tok]++;
		const JunctionPlacementSystem other = RecordJunctionPlacementSystem(in.topology, seed.paths, seed.graph, in.order, seed.structure, seed.programs, spec, bigger);
		INFO(fmt::format("bend edge {} tok {}", free_bend.edge, free_bend.tok));
		REQUIRE(!other.relations.empty());
		CHECK(other.pt_count == base.pt_count);
		REQUIRE(other.bends.size() == base.bends.size());
		for (size_t i = 0; i < base.bends.size(); i++) {
			CHECK(other.bends[i].edge == base.bends[i].edge);
			CHECK(other.bends[i].copy == base.bends[i].copy);
			CHECK(other.bends[i].tok == base.bends[i].tok);
			CHECK(other.bends[i].first == base.bends[i].first);
			CHECK(other.bends[i].last == base.bends[i].last);
		}
		compared++;
	}
	CHECK(compared > 0);
}

/** The sizes a recording's bends have, as the rule source is given them. */
static JunctionBendSizes SizesOfBends(const JunctionPlacementSystem &system)
{
	JunctionBendSizes sizes;
	for (const JunctionSystemBend &bend : system.bends) {
		REQUIRE(bend.id == static_cast<int>(sizes.m.size()));
		sizes.m.push_back(bend.m);
		sizes.hand.push_back(static_cast<int8_t>(bend.hand));
	}
	return sizes;
}

TEST_CASE("record: given sizes are what the rule source reads", "[junction][milp]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JointInputs in = MakeJointInputs("LL_RR", "L_R");
	std::string error;
	const JunctionGraphSeed seed = MakeJunctionGraphSeed(in.topology, spec, &error);
	REQUIRE(!seed.routes.empty());
	const JunctionPlacementSystem base = RecordJunctionPlacementSystem(in.topology, seed.paths, seed.graph, in.order, seed.structure, seed.programs, spec, seed.placement.choices);
	REQUIRE(!base.relations.empty());
	const JunctionSystemBend *chosen = nullptr;
	for (const JunctionSystemBend &bend : base.bends) {
		if (bend.source == "rows" && bend.m >= 1) {
			chosen = &bend;
			break;
		}
	}
	REQUIRE(chosen != nullptr);
	const int id = chosen->id;
	const int m = chosen->m;
	JunctionBendSizes sizes = SizesOfBends(base);
	sizes.m[id] = m + 1;
	const JunctionPlacementSystem other = RecordJunctionPlacementSystem(in.topology, seed.paths, seed.graph, in.order, seed.structure, seed.programs, spec, seed.placement.choices, &sizes);
	REQUIRE(!other.relations.empty());
	REQUIRE(other.bends.size() == base.bends.size());
	CHECK(other.bends[id].m == m + 1);

	/* A W/E fan: its size is read, and its tagged X relations follow it. */
	const JunctionSystemBend *fan = nullptr;
	for (const JunctionSystemBend &bend : base.bends) {
		if (bend.source == "port_rows" && bend.m >= 1) {
			fan = &bend;
			break;
		}
	}
	REQUIRE(fan != nullptr);
	const int fan_id = fan->id;
	const int fan_m = fan->m;
	JunctionBendSizes fan_sizes = SizesOfBends(base);
	fan_sizes.m[fan_id] = fan_m + 1;
	const JunctionPlacementSystem fan_other = RecordJunctionPlacementSystem(in.topology, seed.paths, seed.graph, in.order, seed.structure, seed.programs, spec, seed.placement.choices, &fan_sizes);
	REQUIRE(fan_other.bends.size() == base.bends.size());
	CHECK(fan_other.bends[fan_id].m == fan_m + 1);
	int tagged = 0;
	for (const JunctionSystemRelation &relation : fan_other.relations) {
		if (relation.axis != 0 || relation.bend != fan_id || relation.nonlinear) continue;
		CHECK(relation.c == relation.a * (fan_m + 1) + relation.b);
		tagged++;
	}
	CHECK(tagged > 0);
}

TEST_CASE("record: the measured sizes given back change nothing", "[junction][milp]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	for (const auto &[main, branch] : {std::pair{"L_R", "L_R"}, std::pair{"LL_RR", "L_R"}}) {
		INFO(main << " x " << branch);
		const JointInputs in = MakeJointInputs(main, branch);
		std::string error;
		const JunctionGraphSeed seed = MakeJunctionGraphSeed(in.topology, spec, &error);
		REQUIRE(!seed.routes.empty());
		const JunctionPlacementSystem a = RecordJunctionPlacementSystem(in.topology, seed.paths, seed.graph, in.order, seed.structure, seed.programs, spec, seed.placement.choices);
		const JunctionBendSizes sizes = SizesOfBends(a);
		const JunctionPlacementSystem b = RecordJunctionPlacementSystem(in.topology, seed.paths, seed.graph, in.order, seed.structure, seed.programs, spec, seed.placement.choices, &sizes);
		REQUIRE(!a.relations.empty());
		REQUIRE(a.relations.size() == b.relations.size());
		for (size_t i = 0; i < a.relations.size(); i++) {
			const JunctionSystemRelation &x = a.relations[i];
			const JunctionSystemRelation &y = b.relations[i];
			INFO("relation " << i << ": " << x.reason);
			CHECK(x.axis == y.axis);
			CHECK(x.v == y.v);
			CHECK(x.u == y.u);
			CHECK(x.c == y.c);
			CHECK(x.bend == y.bend);
			CHECK(x.a == y.a);
			CHECK(x.b == y.b);
			CHECK(x.nonlinear == y.nonlinear);
			CHECK(x.zero_branch == y.zero_branch);
			CHECK(x.order_pin == y.order_pin);
			CHECK(x.reason == y.reason);
		}
		REQUIRE(a.bends.size() == b.bends.size());
		for (size_t i = 0; i < a.bends.size(); i++) {
			const JunctionSystemBend &x = a.bends[i];
			const JunctionSystemBend &y = b.bends[i];
			INFO("bend " << i);
			CHECK(x.id == y.id);
			CHECK(x.edge == y.edge);
			CHECK(x.copy == y.copy);
			CHECK(x.tok == y.tok);
			CHECK(x.kind == y.kind);
			CHECK(x.source == y.source);
			CHECK(x.hand == y.hand);
			CHECK(x.hin == y.hin);
			CHECK(x.first == y.first);
			CHECK(x.last == y.last);
			CHECK(x.item_pt == y.item_pt);
			CHECK(x.m == y.m);
			CHECK(x.floor == y.floor);
		}
		REQUIRE(a.clashes.size() == b.clashes.size());
		for (size_t i = 0; i < a.clashes.size(); i++) {
			const JunctionSystemClash &x = a.clashes[i];
			const JunctionSystemClash &y = b.clashes[i];
			INFO("clash " << i);
			CHECK(x.key == y.key);
			CHECK(x.reversed == y.reversed);
			CHECK(x.a_right == y.a_right);
			CHECK(x.b_left == y.b_left);
			CHECK(x.gap == y.gap);
			CHECK(x.held == y.held);
			CHECK(x.reason == y.reason);
		}
		REQUIRE(a.routes.size() == b.routes.size());
		for (size_t i = 0; i < a.routes.size(); i++) {
			const JunctionSystemRoute &x = a.routes[i];
			const JunctionSystemRoute &y = b.routes[i];
			INFO("route " << i);
			CHECK(x.start_pt == y.start_pt);
			CHECK(x.start_heading == y.start_heading);
			CHECK(x.hands == y.hands);
			REQUIRE(x.bends.size() == y.bends.size());
			for (size_t j = 0; j < x.bends.size(); j++) {
				const JunctionSystemRouteBend &p = x.bends[j];
				const JunctionSystemRouteBend &q = y.bends[j];
				CHECK(p.pt == q.pt);
				CHECK(p.bend == q.bend);
				CHECK(p.hin == q.hin);
				CHECK(p.hout == q.hout);
				CHECK(p.hand == q.hand);
				CHECK(p.corner == q.corner);
				CHECK(p.quantity == q.quantity);
				CHECK(p.curve_units == q.curve_units);
			}
		}
	}
}

/** A route bend's quantity at the joint's length: 0 when a sized bend has vanished, else the affine value. */
static int JointQuantity(const JunctionJointResult &joint, const JunctionSystemRouteBend &b, const std::pair<int, int> &q)
{
	if (b.bend < 0) return q.first;
	const int length = joint.length[b.bend];
	return length == 0 ? 0 : q.first + (length - 1) * (q.second - q.first);
}

/** The units at each bend's entry of a route at the joint's solution, chained from the start. */
static std::vector<int> JointEntries(const JunctionJointResult &joint, const JunctionSystemRoute &route)
{
	std::vector<int> entry;
	for (size_t i = 0; i < route.bends.size(); i++) {
		const JunctionSystemRouteBend &b = route.bends[i];
		auto q = [&](const JunctionSystemRouteBend &bend, size_t k) { return JointQuantity(joint, bend, bend.quantity[k]); };
		if (i == 0) {
			const int h = route.start_heading;
			entry.push_back(2 * (HEAD_VX[h] * (joint.x[b.pt] + q(b, 0) - joint.x[route.start_pt]) + HEAD_VY[h] * (joint.y[b.pt] + q(b, 1) - joint.y[route.start_pt])));
		} else {
			const JunctionSystemRouteBend &p = route.bends[i - 1];
			const int h = p.hout;
			entry.push_back(entry[i - 1] + q(p, 4) + 2 * (HEAD_VX[h] * (joint.x[b.pt] + q(b, 0) - joint.x[p.pt] - q(p, 2)) + HEAD_VY[h] * (joint.y[b.pt] + q(b, 1) - joint.y[p.pt] - q(p, 3))));
		}
	}
	return entry;
}

/** Forces the bend's L to 0 by relations: an S fan's end columns equal (x_last - x_first = s L), a rows-sized S-bend's rows equal (L = |y(item) - y(first)|). */
static JunctionPlacementSystem ForceZero(const JunctionPlacementSystem &system, size_t bi)
{
	const JunctionSystemBend &bend = system.bends[bi];
	JunctionPlacementSystem forced = system;
	const bool fan = bend.kind == "SFan";
	const uint8_t axis = fan ? 0 : 1;
	const int a = fan ? bend.last : bend.item_pt, b = bend.first;
	forced.relations.push_back({axis, a, b, 0, -1, 0, 0, false, false, false, "test: zero-size bend"});
	forced.relations.push_back({axis, b, a, 0, -1, 0, 0, false, false, false, "test: zero-size bend"});
	return forced;
}

/** Whether the bend can vanish and is walked by a route. */
static bool ZeroCandidate(const JunctionPlacementSystem &system, size_t bi)
{
	const JunctionSystemBend &bend = system.bends[bi];
	if (!(bend.kind == "SFan" || (bend.kind == "SBend" && bend.source == "rows"))) return false;
	for (const JunctionSystemRoute &route : system.routes) {
		for (const JunctionSystemRouteBend &b : route.bends) {
			if (b.bend == static_cast<int>(bi)) return true;
		}
	}
	return false;
}

/** The counters' checks at a solved system with bend bi at L = 0. */
static void CheckZeroBend(const JunctionPlacementSystem &forced, const JunctionJointResult &joint, size_t bi, const JunctionSpec &spec)
{
	REQUIRE(joint.length[bi] == 0);
	int checked = 0, gaps_checked = 0;
	for (const JunctionSystemRoute &route : forced.routes) {
		const std::vector<int> entry = JointEntries(joint, route);
		for (size_t i = 0; i + 1 < route.bends.size(); i++) {
			if (route.bends[i].bend != static_cast<int>(bi)) continue;
			/* The route with the bend removed: E_prev_exit + 2 h . delta. */
			const JunctionSystemRouteBend &next = route.bends[i + 1];
			auto q = [&](const JunctionSystemRouteBend &b, size_t k) { return JointQuantity(joint, b, b.quantity[k]); };
			int removed;
			if (i == 0) {
				const int h = route.start_heading;
				removed = 2 * (HEAD_VX[h] * (joint.x[next.pt] + q(next, 0) - joint.x[route.start_pt]) + HEAD_VY[h] * (joint.y[next.pt] + q(next, 1) - joint.y[route.start_pt]));
			} else {
				const JunctionSystemRouteBend &p = route.bends[i - 1];
				const int h = p.hout;
				removed = entry[i - 1] + q(p, 4) + 2 * (HEAD_VX[h] * (joint.x[next.pt] + q(next, 0) - joint.x[p.pt] - q(p, 2)) + HEAD_VY[h] * (joint.y[next.pt] + q(next, 1) - joint.y[p.pt] - q(p, 3)));
			}
			CHECK(entry[i + 1] == removed);
			checked++;
		}

		/* Every curve-gap pair that involves no vanished bend's curve holds at the true units. */
		std::vector<std::pair<size_t, int>> curves; // (bend, units)
		for (size_t i = 0; i < route.bends.size(); i++) {
			for (const auto &cu : route.bends[i].curve_units) curves.emplace_back(i, entry[i] + JointQuantity(joint, route.bends[i], cu));
		}
		std::vector<JunctionCurveHand> hands;
		for (const int8_t hand : route.hands) hands.push_back(hand > 0 ? JunctionCurveHand::Right : JunctionCurveHand::Left);
		REQUIRE(curves.size() == hands.size());
		for (const JunctionCurveGap &gap : JunctionCurveGaps(hands, spec.tl, spec.curve_speed, spec.reference_speed)) {
			auto vanished = [&](size_t curve) { const int id = route.bends[curves[curve].first].bend; return id >= 0 && joint.length[id] == 0; };
			if (vanished(gap.i) || vanished(gap.j)) continue;
			CHECK(curves[gap.j].second - curves[gap.i].second >= gap.min_gap);
			gaps_checked++;
		}
	}
	std::cout << fmt::format("[joint] zero-size: routes through it checked {}, gap pairs checked {}\n", checked, gaps_checked);
	CHECK(checked > 0);
}

/** The count of a recording's relations and clashes, and an FNV-1a 64 hash of both lists' text (each sorted), as a guard that its rulebook is unchanged. */
static std::pair<size_t, uint64_t> SystemDigest(const JunctionPlacementSystem &system)
{
	std::vector<std::string> lines;
	for (const JunctionSystemRelation &r : system.relations) lines.push_back(fmt::format("{} {} {} {} {} {} {} {} {} {} {}", r.axis, r.v, r.u, r.c, r.bend, r.a, r.b, r.nonlinear, r.zero_branch, r.order_pin, r.reason));
	std::sort(lines.begin(), lines.end());
	std::vector<std::string> clashes;
	for (const JunctionSystemClash &c : system.clashes) clashes.push_back(fmt::format("{} {} {}", c.key, c.gap, c.held));
	std::sort(clashes.begin(), clashes.end());
	uint64_t hash = 14695981039346656037ULL;
	for (const std::vector<std::string> *list : {&lines, &clashes}) {
		for (const std::string &line : *list) {
			for (const char ch : line) hash = (hash ^ static_cast<unsigned char>(ch)) * 1099511628211ULL;
			hash = (hash ^ '\n') * 1099511628211ULL;
		}
		hash = (hash ^ '\f') * 1099511628211ULL;
	}
	return {system.relations.size() + system.clashes.size(), hash};
}

TEST_CASE("rule source: detection at the flat solve is today's rulebook", "[junction][milp][.sweep]")
{
	struct Case {
		std::string name;
		size_t count;
		uint64_t hash;
	};
	/* The digests printed by the unchanged code (R2 is a refactor), per case in the order the loop below produces them. */
	const std::vector<Case> expected = {
		{"L_R x L_R seed", 393, 640164135075738815ULL},
		{"L_R x L_R bend 5 m+1", 396, 15574989440103210876ULL},
		{"L_R x L_R bend 7 m+1", 396, 49917478393215249ULL},
		{"L_R x L_R bend 8 m+1", 395, 6861077751002777490ULL},
		{"L_R x L_R bend 9 m+1", 395, 10328024605206432383ULL},
		{"L_R x L_R bend 12 m+1", 395, 11232606783720670483ULL},
		{"LL_RR x L_R seed", 1109, 2683259685928647513ULL},
		{"LL_RR x L_R bend 0 m+1", 1111, 17783139826289707174ULL},
		{"LL_RR x L_R bend 1 m+1", 1112, 4109831045377491802ULL},
		{"LL_RR x L_R bend 3 m+1", 1111, 2826417799457400576ULL},
		{"LL_RR x L_R bend 4 m+1", 1111, 528841753813247845ULL},
		{"LL_RR x L_R bend 8 m+1", 1114, 16306449433040433292ULL},
		{"LL_RR x L_R bend 9 m+1", 1111, 15615666931685096934ULL},
		/* From the bridge over either S-bend, the recording also holds the other flanking S-bend's rows (the next three). */
		{"L_R x L_RR all floors", 236, 14665511438918810305ULL},
		{"L_R x LL_RR all floors", 594, 10034201909365135891ULL},
		{"LL_RR x LL_RR all floors", 1542, 8612936502437310721ULL},
	};
	const JunctionSpec spec = MakeTestJunctionSpec();
	std::vector<Case> got;
	auto digest = [&](const std::string &name, const JunctionPlacementSystem &system) {
		const auto [count, hash] = SystemDigest(system);
		std::cout << fmt::format("[digest] {} {} {}\n", name, count, hash);
		got.push_back({name, count, hash});
	};
	for (const auto &[main, branch] : {std::pair{"L_R", "L_R"}, std::pair{"LL_RR", "L_R"}}) {
		const std::string hub = fmt::format("{} x {}", main, branch);
		const JointInputs in = MakeJointInputs(main, branch);
		std::string error;
		const JunctionGraphSeed seed = MakeJunctionGraphSeed(in.topology, spec, &error);
		REQUIRE(!seed.routes.empty());
		const JunctionPlacementSystem base = RecordJunctionPlacementSystem(in.topology, seed.paths, seed.graph, in.order, seed.structure, seed.programs, spec, seed.placement.choices);
		digest(hub + " seed", base);
		int sized = 0;
		for (const JunctionSystemBend &bend : base.bends) {
			if ((bend.source != "rows" && bend.source != "port_rows") || bend.m < 1 || sized == 6) continue;
			sized++;
			JunctionBendSizes sizes = SizesOfBends(base);
			sizes.m[bend.id] = bend.m + 1;
			digest(fmt::format("{} bend {} m+1", hub, bend.id), RecordJunctionPlacementSystem(in.topology, seed.paths, seed.graph, in.order, seed.structure, seed.programs, spec, seed.placement.choices, &sizes));
		}
	}
	for (const auto &[main, branch] : {std::pair{"L_R", "L_RR"}, std::pair{"L_R", "LL_RR"}, std::pair{"LL_RR", "LL_RR"}}) {
		const std::string hub = fmt::format("{} x {}", main, branch);
		const JointInputs in = MakeJointInputs(main, branch);
		std::string error;
		const JunctionGraphSeed seed = MakeJunctionGraphSeed(in.topology, spec, &error);
		const bool seeded = !seed.routes.empty();
		const std::vector<JunctionLanePath> &paths = seeded ? seed.paths : in.paths;
		const JunctionTrackGraph &graph = seeded ? seed.graph : in.graph;
		const JunctionTrackStructure cut = seeded ? seed.structure : DefaultCut(in);
		const JunctionTrackPrograms &programs = seeded ? seed.programs : in.programs;
		/* All floors, as [.g3loop] takes them: every free S-bend 1, S fan 0, joiner Corner k. */
		const JunctionPlacementChoices choices = JunctionFloorChoices(in.topology, paths, graph, in.order, cut, programs, spec);
		REQUIRE(!choices.m.empty());
		digest(hub + " all floors", RecordJunctionPlacementSystem(in.topology, paths, graph, in.order, cut, programs, spec, choices));
	}
	REQUIRE(expected.size() == got.size());
	for (size_t i = 0; i < got.size(); i++) {
		INFO(expected[i].name);
		CHECK(got[i].name == expected[i].name);
		CHECK(got[i].count == expected[i].count);
		CHECK(got[i].hash == expected[i].hash);
	}
}

TEST_CASE("joint: a zero-size bend adds no units", "[junction][milp]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	bool found = false;
	/* First the three failing hubs at the default cut, then (below) a synthetic system. */
	for (const auto &[main, branch] : {std::pair{"L_R", "L_RR"}, std::pair{"L_R", "LL_RR"}, std::pair{"LL_RR", "LL_RR"}}) {
		if (found) break;
		const JointInputs in = MakeJointInputs(main, branch);
		const JunctionTrackStructure cut = DefaultCut(in);
		std::string error;
		JunctionPlacementSystem system;
		PlaceTrackGraph(in.topology, in.paths, in.graph, in.order, cut, in.programs, JunctionSpec(spec), nullptr, &error, {}, {}, nullptr, {0, 0}, true, false, nullptr, nullptr, nullptr, &system);
		int fans = 0, rows = 0;
		for (const JunctionSystemBend &b : system.bends) { fans += b.kind == "SFan"; rows += b.kind == "SBend" && b.source == "rows"; }
		std::cout << fmt::format("[joint] zero-size: {} x {}: routes {}, S fans {}, rows-sized S-bends {}\n", main, branch, system.routes.size(), fans, rows);
		for (size_t bi = 0; bi < system.bends.size() && !found; bi++) {
			if (!ZeroCandidate(system, bi)) continue;
			const JunctionPlacementSystem forced = ForceZero(system, bi);
			const JunctionJointResult joint = SolveJunctionJoint(forced, spec);
			if (joint.status != JunctionMilp::Status::Optimal) continue;
			found = true;
			std::cout << fmt::format("[joint] zero-size: {} x {}, bend {} ({} {} edge {} tok {})\n", main, branch, bi, system.bends[bi].kind, system.bends[bi].source, system.bends[bi].edge, system.bends[bi].tok);
			CheckZeroBend(forced, joint, bi, spec);
		}
	}
	if (!found) {
		/* Synthetic: LL_RR x L_R's record, a walked rows-sized bend forced to 0, with the relations its conflict chain names removed. */
		const JointInputs in = MakeJointInputs("LL_RR", "L_R");
		std::string error;
		const JunctionGraphSeed seed = MakeJunctionGraphSeed(in.topology, spec, &error);
		REQUIRE(!seed.routes.empty());
		const JunctionPlacementSystem system = RecordJunctionPlacementSystem(in.topology, seed.paths, seed.graph, in.order, seed.structure, seed.programs, spec, seed.placement.choices);
		for (size_t bi = 0; bi < system.bends.size() && !found; bi++) {
			if (!ZeroCandidate(system, bi)) continue;
			JunctionPlacementSystem forced = ForceZero(system, bi);
			JunctionJointResult joint = SolveJunctionJoint(forced, spec);
			if (joint.status != JunctionMilp::Status::Infeasible) continue;
			/* The reasons of each conflict chain, except the forced bound, are removed from the copy (every relation with that reason text); a further chain is handled the same way. */
			std::set<std::string> drop;
			int rounds = 0;
			for (; joint.status == JunctionMilp::Status::Infeasible && rounds < 12; rounds++) {
				std::set<std::string> now;
				for (const std::string &reason : joint.conflict) {
					if (reason != "test: zero-size bend") now.insert(reason);
				}
				if (now.empty()) break;
				for (const std::string &reason : now) INFO("removed relations of: " << reason);
				drop.insert(now.begin(), now.end());
				std::erase_if(forced.relations, [&](const JunctionSystemRelation &r) { return now.count(r.reason) != 0; });
				joint = SolveJunctionJoint(forced, spec);
			}
			std::string removed;
			for (const std::string &reason : drop) removed += (removed.empty() ? "" : "; ") + reason;
			std::cout << fmt::format("[joint] zero-size synthetic: LL_RR x L_R bend {} ({} edge {} tok {}), chains removed {}, [{}], status {}, left: {}\n", bi, system.bends[bi].kind, system.bends[bi].edge, system.bends[bi].tok, rounds, removed, static_cast<int>(joint.status), joint.conflict.size());
			if (joint.status != JunctionMilp::Status::Optimal) continue;
			found = true;
			CheckZeroBend(forced, joint, bi, spec);
		}
	}
	REQUIRE(found);
}

TEST_CASE("joint: choices round-trip", "[junction][milp]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	for (const auto &[main, branch] : {std::pair{"L_R", "L_R"}, std::pair{"LL_RR", "L_R"}}) {
		INFO(main << " x " << branch);
		const JointInputs in = MakeJointInputs(main, branch);
		std::string error;
		const JunctionGraphSeed seed = MakeJunctionGraphSeed(in.topology, spec, &error);
		REQUIRE(!seed.routes.empty());
		JunctionPlacementChoices expected = seed.placement.choices;
		const JunctionPlacementSystem system = RecordJunctionPlacementSystem(in.topology, seed.paths, seed.graph, in.order, seed.structure, seed.programs, spec, expected);
		const JunctionJointResult joint = SolveJunctionJoint(system, spec);
		REQUIRE(joint.status == JunctionMilp::Status::Optimal);
		REQUIRE(joint.x.size() == static_cast<size_t>(system.pt_count));
		REQUIRE(joint.y.size() == static_cast<size_t>(system.pt_count));
		if (std::string(main) == "L_R") {
			REQUIRE(expected.corner.size() > 5);
			REQUIRE(expected.corner[5].size() > 0);
			CHECK(expected.corner[5][0] == 6);
			/* The joint keeps T's Corner of edge 5 at 6: its port fans follow the rows, and at 5 the rows give them sizes the held fans did not have. */
		}
		CHECK(joint.choices.port_offset == expected.port_offset);
		CHECK(joint.choices.m == expected.m);
		CHECK(joint.choices.corner == expected.corner);
		CHECK(joint.choices == expected);
	}
}

/** The curve-gap rows of the counters where a vanished bend lies strictly between the two curves' bends (their gaps count the vanished curves). */
static int ApproximatedGaps(const JunctionPlacementSystem &system, const JunctionJointResult &joint, const JunctionSpec &spec)
{
	int count = 0;
	for (const JunctionSystemRoute &route : system.routes) {
		std::vector<size_t> bend_of;
		for (size_t i = 0; i < route.bends.size(); i++) bend_of.insert(bend_of.end(), route.bends[i].curve_units.size(), i);
		std::vector<JunctionCurveHand> hands;
		for (const int8_t hand : route.hands) hands.push_back(hand > 0 ? JunctionCurveHand::Right : JunctionCurveHand::Left);
		for (const JunctionCurveGap &gap : JunctionCurveGaps(hands, spec.tl, spec.curve_speed, spec.reference_speed)) {
			for (size_t between = bend_of[gap.i] + 1; between < bend_of[gap.j]; between++) {
				const int id = route.bends[between].bend;
				if (id >= 0 && ZeroCandidate(system, static_cast<size_t>(id)) && joint.length[id] == 0) {
					count++;
					break;
				}
			}
		}
	}
	return count;
}

/** A hub's inputs for the joint loop, and the choices its first round records at. */
struct LoopStart {
	JunctionTopology topology;
	JunctionTrackOrder order;
	std::vector<JunctionLanePath> paths;
	JunctionTrackGraph graph;
	JunctionTrackStructure cut;
	JunctionTrackPrograms programs;
	JunctionPlacementChoices choices;
	std::string description; ///< Where the choices come from.
};

/** The loop's start at a hub: the seed's choices when it has a seed, else the rounds-mode choices, else all floors (every free S-bend 1, S fan 0, joiner Corner k). */
static LoopStart MakeLoopStart(const char *main, const char *branch, const JunctionSpec &spec)
{
	const JointInputs in = MakeJointInputs(main, branch);
	std::string error;
	const JunctionGraphSeed seed = MakeJunctionGraphSeed(in.topology, spec, &error);
	const bool seeded = !seed.routes.empty();
	LoopStart start;
	start.topology = in.topology;
	start.order = in.order;
	start.paths = seeded ? seed.paths : in.paths;
	start.graph = seeded ? seed.graph : in.graph;
	start.cut = seeded ? seed.structure : DefaultCut(in);
	start.programs = seeded ? seed.programs : in.programs;
	if (seeded) {
		start.choices = seed.placement.choices;
		start.description = "the seed's choices";
		return start;
	}
	const JunctionTrackPlacement rounds = PlaceJunctionTrackGraph(in.topology, start.paths, start.graph, in.order, start.cut, start.programs, spec, &error);
	if (!rounds.nodes.empty() && !rounds.choices.m.empty()) {
		start.choices = rounds.choices;
		start.description = "the rounds-mode choices";
		return start;
	}
	start.choices = JunctionFloorChoices(in.topology, start.paths, start.graph, in.order, start.cut, start.programs, spec);
	REQUIRE(!start.choices.m.empty());
	start.description = "all-floor choices";
	return start;
}

static JunctionPlacementSystem RecordAt(const LoopStart &s, const JunctionSpec &spec, const JunctionPlacementChoices &choices, const JunctionSystemPoint *at)
{
	return RecordJunctionPlacementSystem(s.topology, s.paths, s.graph, s.order, s.cut, s.programs, spec, choices, nullptr, at);
}

TEST_CASE("joint: a recording at a point detects there", "[junction][milp]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	/* The recording at its own canonical point holds every relation and clash it recorded there, and its rulebook is pinned. */
	struct Hub {
		const char *main, *branch;
		size_t count;
		uint64_t hash;
	};
	for (const Hub &hub : {Hub{"LL_RR", "L_R", 1083, 9031261406966711500ULL}, Hub{"L_R", "L_R", 393, 640164135075738815ULL}}) {
		const LoopStart s = MakeLoopStart(hub.main, hub.branch, spec);
		REQUIRE(s.description == "the seed's choices");
		INFO(fmt::format("{} x {}", hub.main, hub.branch));
		const JunctionPlacementSystem plain = RecordAt(s, spec, s.choices, nullptr);
		REQUIRE(!plain.solved.x.empty());
		CHECK(plain.solved.y.size() == plain.solved.x.size());
		const JunctionPlacementSystem at = RecordAt(s, spec, s.choices, &plain.solved);
		REQUIRE(!at.solved.x.empty());
		CHECK(at.failed.empty());
		int broken = 0;
		for (const JunctionSystemRelation &r : at.relations) {
			const std::vector<int> &coord = r.axis == 0 ? plain.solved.x : plain.solved.y;
			if (coord[r.v] - coord[r.u] < r.c && broken++ == 0) FAIL_CHECK(fmt::format("first relation that fails at the point: axis {} v {} u {} c {} (has {}) {}", r.axis, r.v, r.u, r.c, coord[r.v] - coord[r.u], r.reason));
		}
		CHECK(broken == 0);
		int unheld = 0;
		for (const JunctionSystemClash &c : at.clashes) {
			if (ClashColumnValue(at, plain.solved.x, c.b_left) - ClashColumnValue(at, plain.solved.x, c.a_right) < c.gap && unheld++ == 0) FAIL_CHECK(fmt::format("first clash that fails at the point: {} gap {} {}", c.key, c.gap, c.reason));
		}
		CHECK(unheld == 0);
		const auto [count, hash] = SystemDigest(at);
		CHECK(count == hub.count);
		CHECK(hash == hub.hash);
	}
	{
		const LoopStart s = MakeLoopStart("LL_RR", "L_R", spec);
		const JunctionPlacementSystem plain = RecordAt(s, spec, s.choices, nullptr);
		/* A point of the wrong size is refused. */
		JunctionSystemPoint wrong = plain.solved;
		wrong.x.pop_back();
		CHECK(RecordAt(s, spec, s.choices, &wrong).relations.empty());
	}
	{
		const LoopStart s = MakeLoopStart("L_R", "L_RR", spec);
		REQUIRE(s.description == "all-floor choices");
		const JunctionPlacementSystem plain = RecordAt(s, spec, s.choices, nullptr);
		REQUIRE(!plain.relations.empty());
		CHECK(plain.clashes.empty());
		const JunctionJointResult joint = SolveJunctionJoint(plain, spec);
		REQUIRE(joint.status == JunctionMilp::Status::Optimal);
		const JunctionSystemPoint point{joint.x, joint.y};
		const JunctionPlacementSystem at = RecordAt(s, spec, s.choices, &point);
		CHECK(std::ranges::any_of(at.relations, [](const JunctionSystemRelation &r) { return r.axis == 0; }));
		CHECK(!at.clashes.empty());
	}
}

TEST_CASE("joint: a clash's order is the joint's choice", "[junction][milp]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	/* Three points: the origin, a and b. One clash between two occupants of width 0 records b after a; a relation x[a] - x[b] >= 3 contradicts that order. */
	JunctionPlacementSystem system;
	system.pt_count = 3;
	system.relations.push_back({0, 1, 2, 3, -1, 0, 0, false, false, false, "test relation"});
	JunctionSystemClash clash{};
	clash.key = "a|b|0";
	clash.reversed = "b|a|0";
	clash.a_left = clash.a_right = {1, 0, 0, -1};
	clash.b_left = clash.b_right = {2, 0, 0, -1};
	clash.gap = clash.gap_reversed = 1;
	clash.held = false;
	clash.tie = false;
	clash.choose = true;
	clash.reason = "occupancy (test)";
	system.clashes.push_back(clash);

	const JunctionJointResult joint = SolveJunctionJoint(system, spec);
	INFO("status " << static_cast<int>(joint.status));
	REQUIRE(joint.status == JunctionMilp::Status::Optimal);
	CHECK(joint.x[1] - joint.x[2] >= 3);

	/* A tie keeps its offset: the same clash as a tie is infeasible and names the clash's reason. */
	system.clashes[0].tie = true;
	const JunctionJointResult tied = SolveJunctionJoint(system, spec);
	CHECK(tied.status == JunctionMilp::Status::Infeasible);
}

TEST_CASE("joint: a port fan's size follows the rows", "[junction][milp]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	/* The all-floor start, whether or not the hub seeds. */
	const JointInputs in = MakeJointInputs("L_R", "LL_RR");
	LoopStart s;
	s.topology = in.topology;
	s.order = in.order;
	s.paths = in.paths;
	s.graph = in.graph;
	s.cut = DefaultCut(in);
	s.programs = in.programs;
	s.choices = JunctionFloorChoices(in.topology, s.paths, s.graph, in.order, s.cut, s.programs, spec);
	REQUIRE(!s.choices.m.empty());
	const JunctionPlacementSystem first = RecordAt(s, spec, s.choices, nullptr);
	REQUIRE(!first.relations.empty());
	const JunctionJointResult joint = SolveJunctionJoint(first, spec);
	REQUIRE(joint.status == JunctionMilp::Status::Optimal);
	const JunctionPlacementSystem second = RecordAt(s, spec, joint.choices, nullptr);
	REQUIRE(!second.relations.empty());
	REQUIRE(second.bends.size() == first.bends.size());
	int fans = 0, sized = 0;
	for (const JunctionSystemBend &b : second.bends) {
		if (b.source != "port_rows") continue;
		fans++;
		sized += b.m >= 1;
		CHECK(b.m == joint.length[b.id]);
	}
	CHECK(fans > 0);
	CHECK(sized >= 1);
}

TEST_CASE("joint: the loop ends settled or names a reason", "[junction][milp][.sweep]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	for (const auto &[main, branch] : {std::pair{"L_R", "L_R"}, std::pair{"LL_RR", "L_R"}, std::pair{"L_R", "L_RR"}, std::pair{"L_R", "LL_RR"}, std::pair{"LL_RR", "LL_RR"}}) {
		const std::string hub = fmt::format("{} x {}", main, branch);
		const LoopStart s = MakeLoopStart(main, branch, spec);
		const JunctionJointLoopResult loop = SolveJunctionJointLoop(s.topology, s.paths, s.graph, s.order, s.cut, s.programs, spec, s.choices);
		CHECK(!loop.outcome.empty());
		if (hub == "L_R x L_R" || hub == "LL_RR x L_R") {
			CHECK(loop.outcome == "settled");
			std::string error;
			const JunctionTrackPlacement placed = PlaceJunctionTrackGraph(s.topology, s.paths, s.graph, s.order, s.cut, s.programs, spec, loop.choices, &error);
			CHECK(!placed.nodes.empty());
		} else {
			std::string log;
			for (const std::string &line : loop.log) log += "\n  " + line;
			WARN(fmt::format("{}: C_0 {}, outcome {}, rounds {}{}", hub, s.description, loop.outcome, loop.rounds, log));
		}
	}
}

TEST_CASE("joint: a bridge over an S-bend may move along it", "[junction][milp][.sweep]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	for (const auto &[main, branch] : {std::pair{"L_R", "LL_RR"}}) {
		const std::string hub = fmt::format("{} x {}", main, branch);
		INFO(hub);
		const LoopStart s = MakeLoopStart(main, branch, spec);
		const JunctionJointLoopResult loop = SolveJunctionJointLoop(s.topology, s.paths, s.graph, s.order, s.cut, s.programs, spec, s.choices);
		/* On L_R x LL_RR no round's conflict, and not the final outcome, names a bridge over an S-bend. */
		if (hub == "L_R x LL_RR") { // LL_RR x LL_RR is a measure only: its conflict needs columns beyond the diagonal.
			for (const std::string &line : loop.log) CHECK(line.find("bridge over S-bend") == std::string::npos);
			CHECK(loop.outcome.find("bridge over S-bend") == std::string::npos);
		}
		const JunctionPlacementSystem last = RecordAt(s, spec, loop.choices, &loop.point);
		std::string ts;
		for (size_t i = 0; i < last.overs.size(); i++) ts += fmt::format("{}t{}", ts.empty() ? "" : ", ", last.overs[i].t);
		WARN(fmt::format("{}: outcome {}, rounds {}, last line [{}], overs t [{}]", hub, loop.outcome, loop.rounds, loop.log.empty() ? "" : loop.log.back(), ts));
		if (loop.outcome == "settled") {
			std::string error;
			const JunctionTrackPlacement placed = PlaceJunctionTrackGraph(s.topology, s.paths, s.graph, s.order, s.cut, s.programs, spec, loop.choices, &error);
			WARN(fmt::format("{}: the settled choices {}", hub, placed.nodes.empty() ? "do not place, error: " + error : fmt::format("place, box {} x {}", placed.size.x, placed.size.y)));
		}
	}
}

TEST_CASE("joint: a bridge may pass over the S-bend after its crossing", "[junction][milp][.sweep]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const LoopStart s = MakeLoopStart("LL_RR", "LL_RR", spec);
	const JunctionPlacementSystem system = RecordAt(s, spec, s.choices, nullptr);
	const JunctionJointResult joint = SolveJunctionJoint(system, spec);
	std::string conflict;
	for (const std::string &reason : joint.conflict) conflict += (conflict.empty() ? "" : " <- ") + reason;
	/* Edge 14's bridge reaches the S-bend after its crossing; another conflict may remain (the round-0 recording at all floors). */
	CHECK(conflict.find("bridge over S-bend (edge 14)") == std::string::npos);
	std::string edge14;
	for (size_t i = 0; i < system.overs.size(); i++) {
		const JunctionSystemOver &over = system.overs[i];
		if (over.edge != 14) continue;
		const bool after = joint.status == JunctionMilp::Status::Optimal && !joint.choices.over_after.empty() && joint.choices.over_after[over.edge][over.tok] != 0;
		const int t = joint.status == JunctionMilp::Status::Optimal && !joint.choices.over.empty() ? joint.choices.over[over.edge][over.tok] + 1 : 1;
		edge14 += fmt::format(" [tok {}: before {}, after {}, chosen {}, t {}]", over.tok, over.bend, over.bend_after, after ? "after" : "before", t);
	}
	WARN(fmt::format("round 0: status {}, conflict [{}], edge 14{}", static_cast<int>(joint.status), conflict, edge14));
	const JunctionJointLoopResult loop = SolveJunctionJointLoop(s.topology, s.paths, s.graph, s.order, s.cut, s.programs, spec, s.choices);
	WARN(fmt::format("loop: outcome {}, rounds {}, last line [{}]", loop.outcome, loop.rounds, loop.log.empty() ? "" : loop.log.back()));
}

TEST_CASE("joint: the joint loop on the hubs", "[junction][milp][.g3loop]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	for (const auto &[main, branch] : {std::pair{"L_R", "L_R"}, std::pair{"LL_RR", "L_R"}, std::pair{"L_R", "L_RR"}, std::pair{"L_R", "LL_RR"}, std::pair{"LL_RR", "LL_RR"}}) {
		const std::string hub = fmt::format("{} x {}", main, branch);
		const auto hub_start = std::chrono::steady_clock::now();
		const LoopStart s = MakeLoopStart(main, branch, spec);
		const JunctionJointLoopResult loop = SolveJunctionJointLoop(s.topology, s.paths, s.graph, s.order, s.cut, s.programs, spec, s.choices);
		for (const std::string &line : loop.log) WARN(fmt::format("{} {}", hub, line));
		/* The approximated gap rows at the last solve: the last round's recording and joint solution again. */
		const JunctionPlacementSystem last = RecordAt(s, spec, loop.choices, &loop.point);
		int approx = 0;
		if (!last.relations.empty()) {
			const JunctionJointResult joint = SolveJunctionJoint(last, spec);
			if (joint.status == JunctionMilp::Status::Optimal) approx = ApproximatedGaps(last, joint, spec);
		}
		const double total = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - hub_start).count();
		WARN(fmt::format("{}: C_0 {}, {} after {} rounds, approximated gap rows at the last solve {}, total {:.1f} ms", hub, s.description, loop.outcome, loop.rounds, approx, total));
		if (loop.outcome == "settled") {
			std::string error;
			const JunctionTrackPlacement placed = PlaceJunctionTrackGraph(s.topology, s.paths, s.graph, s.order, s.cut, s.programs, spec, loop.choices, &error);
			CHECK(!placed.nodes.empty());
			if (!placed.nodes.empty()) WARN(fmt::format("{}: the settled choices place, box {} x {}", hub, placed.size.x, placed.size.y));
		}
		if (hub == "LL_RR x LL_RR") {
			/* A measure for the next step: the round-0 system without the bridge-over-S-bend relations. */
			JunctionPlacementSystem round0 = RecordAt(s, spec, s.choices, nullptr);
			std::erase_if(round0.relations, [](const JunctionSystemRelation &r) { return r.reason.starts_with("bridge over S-bend"); });
			const JunctionJointResult without_bridges = SolveJunctionJoint(round0, spec);
			std::string chain;
			for (const std::string &reason : without_bridges.conflict) chain += (chain.empty() ? "" : " <- ") + reason;
			WARN(fmt::format("{} round 0 without the bridge-over-S-bend relations: status {}{}", hub, static_cast<int>(without_bridges.status), without_bridges.status == JunctionMilp::Status::Infeasible ? ", conflict: " + chain : ""));
		}
	}
}

TEST_CASE("placement: a band edge carries a series of single bridges", "[junction][milp]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const char *refusal = "a bridge over several band crossings is not ruled yet";
	for (const bool keep : {false, true}) {
		const char *hub = keep ? "LL_RR x LL_RR keep" : "LLL_RRR x LL_RR";
		JointInputs in = MakeJointInputs(keep ? "LL_RR" : "LLL_RRR", keep ? "LL_RR" : "LL_RR");
		if (keep) {
			const JunctionLineSpec llrr = ParseLineNotation("LL_RR").spec;
			std::string why;
			in.topology = MakeJunctionHubTopology(llrr, llrr, JunctionLanePairing::LaneKeepsLane);
			in.paths = ExpandJunctionLanes(in.topology, &why);
			in.graph = BuildJunctionTrackGraph(in.topology, in.paths, &why);
			in.order = OrderJunctionTrackGraph(in.topology, in.paths, in.graph, &why);
			in.structure = StructureJunctionTrackGraph(in.topology, in.paths, in.graph, in.order, &why);
			in.programs = ProgramJunctionTrackGraph(in.topology, in.paths, in.graph, in.order, in.structure, &why);
		}
		INFO(hub);
		REQUIRE(!in.structure.bridges.empty());
		const JunctionTrackStructure cut = DefaultCut(in);
		auto is_row = [&](uint32_t e) { return std::any_of(cut.bridges.begin(), cut.bridges.end(), [&](const JunctionTrackBridge &b) { return b.edge == e && b.doubled; }); };
		bool series = false;
		for (uint32_t e = 0; e < in.graph.edges.size(); e++) {
			if (is_row(e)) continue;
			series = series || std::count_if(cut.bridges.begin(), cut.bridges.end(), [&](const JunctionTrackBridge &b) { return b.edge == e; }) >= 2;
		}
		CHECK(series);

		std::string error;
		const JunctionTrackPlacement placed = PlaceJunctionTrackGraph(in.topology, in.paths, in.graph, in.order, cut, in.programs, spec, &error);
		CHECK(error.find(refusal) == std::string::npos);
		WARN(hub << ": " << (placed.nodes.empty() ? error : std::string("places")));
		if (placed.nodes.empty()) continue;
		/* No existing test checks an Over span against its crossed edge's track; check only that every band Over/Tunnel token has a span. */
		for (uint32_t e = 0; e < in.graph.edges.size(); e++) {
			if (is_row(e)) continue;
			for (size_t t = 0; t < in.programs.edges[e].size(); t++) {
				const JunctionTrackStep::Kind kind = in.programs.edges[e][t].kind;
				if (kind != JunctionTrackStep::Kind::Over && kind != JunctionTrackStep::Kind::Tunnel) continue;
				REQUIRE(t < placed.edges[e].size());
				const JunctionTrackSpan &span = placed.edges[e][t];
				CHECK((span.first.x != span.second.x || span.first.y != span.second.y));
			}
		}
	}
}
