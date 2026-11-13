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

TEST_CASE("joint: reproduces the prototype on T and LL_RR x L_R", "[junction][milp]")
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

TEST_CASE("joint: an infeasible system names its conflict", "[junction][milp]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	JointInputs in = MakeJointInputs("LL_RR", "LL_RR");

	/* The default cut: every doubled bridge in series of the most crossings, from the start (the first cut the cut search tries). */
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
