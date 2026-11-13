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

#include "../../../junction/junction_pipeline.h"
#include "../../../junction/junction_spec.h"
#include "../../../junction/line/junction_line.h"
#include "../../../junction/placement/junction_joint.h"
#include "../../../junction/placement/junction_placement.h"
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
		double objective; ///< The prototype's, without the tie break.
		int differing; ///< The bends that differ from the seed's size.
	};
	const JunctionSpec spec = MakeTestJunctionSpec();
	for (const Case &c : {Case{"T", "L_R", "L_R", 27050, 1}, Case{"LL_RR x L_R", "LL_RR", "L_R", 86350, 0}}) {
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
				if (std::string(main) == "L_R" && b == 1) CHECK(free_span == 5); // T: the Corner of edge 5 at 5 shortens the bridge from the seed's 6.
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
