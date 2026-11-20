/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file planar_shape_layer.cpp Test the Shape promise oracle on two hand variants, one mutation per promise (SPEC-shape §7 H1). */

#include "../../../stdafx.h"

#include "../../../3rdparty/catch2/catch.hpp"

#include "../../../junction/junction_spec.h"
#include "../../../junction/planar/planar_shape_layer.h"
#include "../../../junction/planar/planar_structure.h"
#include "../../../junction/planar/planar_topology.h"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "../../../3rdparty/fmt/format.h"
#include "../../../junction/placement/junction_milp.h"
#include "../../../junction/planar/planar_model.h"
#include "../../../junction/planar/planar_plan.h"

#include "../../../safeguards.h"

/** A hand case: its topology and spec, and the hand-made shape of Structure's variant 0. */
struct ShapeCase {
	PlanarTopology topology;
	JunctionSpec spec = MakeTestJunctionSpec();
	PlanarShapeVariant v;
};

/**
 * Build Structure's variant 0 of a topology and wrap it with no shape.
 * @param text The topology text.
 * @param tag The tag variant 0 must have.
 * @return The case; its tracks, angles and structures are empty.
 */
static ShapeCase MakeCase(const std::string &text, const std::string &tag)
{
	ShapeCase c;
	std::string error;
	c.topology = ParsePlanarTopology(text, &error);
	REQUIRE(error.empty());
	const PlanarStructureResult r = BuildPlanarStructure(c.topology, c.spec, 1);
	INFO("why: " << r.why);
	REQUIRE(!r.variants.empty());
	REQUIRE(r.variants[0].tag == tag);
	c.v.structure = r.variants[0];
	c.v.cost = 0;
	return c;
}

/** Give the case the hand-made shape: per track index, its pieces and event_piece. */
static void SetTracks(ShapeCase &c, const std::vector<std::pair<std::vector<int>, std::vector<uint32_t>>> &tracks)
{
	REQUIRE(tracks.size() == c.v.structure.map.tracks.size());
	c.v.tracks.clear();
	for (const auto &[pieces, event_piece] : tracks) c.v.tracks.push_back({pieces, event_piece});
}

/** The one-lane T: the program's shape under H11 (D376); the earlier hand shape crossed structure 0 at headings 6 and 3. */
static ShapeCase MakeT1()
{
	ShapeCase c = MakeCase(PlanarTText(1), "o3t0f0m0");
	SetTracks(c, {
		{{2}, {}}, {{6}, {}}, {{2}, {}}, {{6, 5}, {}}, {{0, -1}, {}}, {{4}, {}},
		{{4}, {}}, {{5, 6}, {0, 1}}, {{-1}, {}}, {{0, 1}, {0, 0}},
		{{2}, {}}, {{2, 3}, {0, 0}}, {{6}, {}}, {{2}, {}}, {{6}, {}}, {{2}, {}},
	});
	c.v.angles = {2, 3, 2};
	REQUIRE(c.v.structure.structures.size() == 2);
	REQUIRE(c.v.structure.structures[0].separator == 9);
	REQUIRE(c.v.structure.structures[1].separator == 11);
	c.v.structures = {{0, {2, -1}, {1, -1}, {-1, -1}}, {2, {2, -1}, {1, -1}, {-1, -1}}};
	return c;
}

/** The fray: its tracks are found by name. */
static ShapeCase MakeFray()
{
	ShapeCase c = MakeCase("topology fray\nport W in E: AB1\nport E out E: A1\nport S out S: B1\nnode AB1 -> A1, B1\n", "o0t0f0m0");
	std::vector<std::pair<std::vector<int>, std::vector<uint32_t>>> tracks;
	for (const PlanarTrack &t : c.v.structure.map.tracks) {
		if (t.name == "AB1") {
			tracks.push_back({{2}, {}});
		} else if (t.name == "A1") {
			tracks.push_back({{2}, {}});
		} else if (t.name == "B1") {
			tracks.push_back({{3, 4}, {}});
		} else {
			FAIL("unexpected track " << t.name);
		}
	}
	SetTracks(c, tracks);
	return c;
}

/** @return The index of the fray's track of that name. */
static size_t FrayTrack(const ShapeCase &c, const std::string &name)
{
	for (size_t t = 0; t < c.v.structure.map.tracks.size(); t++) {
		if (c.v.structure.map.tracks[t].name == name) return t;
	}
	FAIL("no track " << name);
	return 0;
}

/** Assert the oracle breaches, and every message starts with the id. */
static void RequireAllOf(const ShapeCase &c, const PlanarShapeVariant &v, const std::string &id)
{
	const std::vector<std::string> messages = CheckPlanarShapePromise(c.topology, c.spec, v);
	INFO(messages.size() << " messages, first: " << (messages.empty() ? "" : messages[0]));
	REQUIRE(!messages.empty());
	for (const std::string &m : messages) {
		INFO(m);
		CHECK(m.starts_with(id + ":"));
	}
}

TEST_CASE("planar shape layer: the oracle accepts the two valid hand variants", "[junction][planar][planarshape]")
{
	for (const ShapeCase &c : {MakeT1(), MakeFray()}) {
		const std::vector<std::string> messages = CheckPlanarShapePromise(c.topology, c.spec, c.v);
		INFO(c.v.structure.tag << ": " << messages.size() << " messages, first: " << (messages.empty() ? "" : messages[0]));
		CHECK(messages.empty());
	}
}

TEST_CASE("planar shape layer: H1 a stretch that turns and turns back", "[junction][planar][planarshape]")
{
	ShapeCase c = MakeFray();
	c.v.tracks[FrayTrack(c, "B1")].pieces = {3, 4, 3, 4};
	RequireAllOf(c, c.v, "H1");
}

TEST_CASE("planar shape layer: H2 a port end off its port's heading", "[junction][planar][planarshape]")
{
	ShapeCase c = MakeFray();
	c.v.tracks[FrayTrack(c, "A1")].pieces = {2, 1};
	RequireAllOf(c, c.v, "H2");
}

TEST_CASE("planar shape layer: H3 a leg that turns two eighths at a node", "[junction][planar][planarshape]")
{
	ShapeCase c = MakeFray();
	c.v.tracks[FrayTrack(c, "B1")].pieces = {4};
	RequireAllOf(c, c.v, "H3");
}

TEST_CASE("planar shape layer: H4 a stored angle that the headings do not give", "[junction][planar][planarshape]")
{
	ShapeCase c = MakeT1();
	c.v.angles[1] = 1;
	RequireAllOf(c, c.v, "H4");
}

TEST_CASE("planar shape layer: H5 a crossing on a diagonal piece of its separator", "[junction][planar][planarshape]")
{
	ShapeCase c = MakeT1();
	c.v.tracks[9].event_piece = {0, 1};
	c.v.angles[2] = 1;
	c.v.structures[0].middles = {3, -1};
	RequireAllOf(c, c.v, "H5");
}

TEST_CASE("planar shape layer: H6 middles that are not the least at the angles", "[junction][planar][planarshape]")
{
	ShapeCase c = MakeT1();
	c.v.structures[0].middles = {5, -1};
	RequireAllOf(c, c.v, "H6");
}

TEST_CASE("planar shape layer: H7 a face that winds twice and a U-turn", "[junction][planar][planarshape]")
{
	ShapeCase c = MakeFray();
	c.v.tracks[FrayTrack(c, "A1")].pieces = {2, 3, 4, 5, 6, 7, 8, 9, 10};
	RequireAllOf(c, c.v, "H7");
}

TEST_CASE("planar shape layer: H8 a row whose pieces differ from its lane's", "[junction][planar][planarshape]")
{
	ShapeCase c = MakeT1();
	c.v.tracks[15].pieces = {2, 3};
	RequireAllOf(c, c.v, "H8");
}

TEST_CASE("planar shape layer: H9 Structure's promise broken under the shape", "[junction][planar][planarshape]")
{
	ShapeCase c = MakeT1();
	c.v.structure.map.conflicts.push_back({0, 0, 0, 0});
	RequireAllOf(c, c.v, "H9");
}

/** Shape the case's variant and assert it is shaped and kept by the oracle, with the cost. */
static void RequireShaped(const ShapeCase &c, int cost)
{
	PlanarShapeVariant shaped;
	std::string why;
	const bool ok = ShapePlanarStructureVariant(c.v.structure, shaped, why);
	INFO("why: " << why);
	REQUIRE(ok);
	const std::vector<std::string> messages = CheckPlanarShapePromise(c.topology, c.spec, shaped);
	INFO(messages.size() << " messages, first: " << (messages.empty() ? "" : messages[0]));
	CHECK(messages.empty());
	CHECK(shaped.cost == cost);
}

TEST_CASE("planar shape layer: the one-lane T is shaped at the least turning", "[junction][planar][planarshape]")
{
	RequireShaped(MakeT1(), 5);
}

TEST_CASE("planar shape layer: the fray is shaped at the least turning", "[junction][planar][planarshape]")
{
	RequireShaped(MakeFray(), 1);
}

TEST_CASE("planar shape layer: H4 a mirrored crossing with another angle than its original", "[junction][planar][planarshape]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	std::string error;
	const PlanarTopology topology = ParsePlanarTopology(PlanarTText(2), &error);
	REQUIRE(error.empty());
	const PlanarStructureResult r = BuildPlanarStructure(topology, spec, 1);
	REQUIRE(!r.variants.empty());
	PlanarShapeVariant shaped;
	std::string why;
	const bool ok = ShapePlanarStructureVariant(r.variants[0], shaped, why);
	INFO("why: " << why);
	REQUIRE(ok);
	CHECK(CheckPlanarShapePromise(topology, spec, shaped).empty());
	const PlanarTrackMap &map = shaped.structure.map;
	uint32_t mirrored = UINT32_MAX;
	for (uint32_t c = 0; c < map.crossings.size() && mirrored == UINT32_MAX; c++) {
		bool listed = false;
		for (const PlanarStructure &s : shaped.structure.structures) listed = listed || std::ranges::find(s.crossings, c) != s.crossings.end();
		if (!listed) mirrored = c;
	}
	REQUIRE(mirrored != UINT32_MAX);
	shaped.angles[mirrored] = shaped.angles[mirrored] == 1 ? 3 : 1;
	const std::vector<std::string> messages = CheckPlanarShapePromise(topology, spec, shaped);
	REQUIRE(!messages.empty());
	for (const std::string &m : messages) {
		INFO(m);
		CHECK(m.starts_with("H4:"));
	}
}

TEST_CASE("planar shape layer: H11 a span whose crossers are not parallel", "[junction][planar][planarshape]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	std::string error;
	const PlanarTopology topology = ParsePlanarTopology(PlanarTText(2), &error);
	REQUIRE(error.empty());
	const PlanarStructureResult r = BuildPlanarStructure(topology, spec, 16);
	REQUIRE(!r.variants.empty());
	bool broken = false;
	for (const PlanarStructureVariant &variant : r.variants) {
		PlanarShapeVariant shaped;
		std::string why;
		const bool ok = ShapePlanarStructureVariant(variant, shaped, why);
		INFO(variant.tag << " why: " << why);
		REQUIRE(ok);
		CHECK(CheckPlanarShapePromise(topology, spec, shaped).empty());
		if (broken) continue;
		for (const PlanarStructure &s : shaped.structure.structures) {
			if (broken || s.crossers.size() < 2) continue;
			const PlanarTrackMap &map = shaped.structure.map;
			const std::vector<uint32_t> &events = map.events[s.crossers[1]];
			const size_t k = static_cast<size_t>(std::ranges::find(events, s.crossings[1]) - events.begin());
			REQUIRE(k < events.size());
			shaped.tracks[s.crossers[1]].pieces[shaped.tracks[s.crossers[1]].event_piece[k]] += 2;
			bool found = false;
			for (const std::string &m : CheckPlanarShapePromise(topology, spec, shaped)) found = found || m.starts_with("H11:");
			CHECK(found);
			broken = true;
		}
	}
	CHECK(broken);
}

/** The closure probe's answer for one shaped variant. */
struct ClosureResult {
	bool closes; ///< The headings close with positive lengths.
	std::string why; ///< The status and, when infeasible, the tags of the conflicting rows.
	double micros; ///< The time of the solve.
	std::vector<std::string> steps; ///< With bits 8 and 16, one entry per whole-lane step with n != 0 after an optimal solve.
	std::vector<std::string> rigid; ///< With bit 64, one entry per non-exempt part that cannot be stretched (t < 0.5) after an optimal solve.
};

/**
 * Ask whether the headings of a shaped variant can close with positive lengths (the lengths-only LP of H2).
 * Per point (x, y); a parallel leg of a node is offset sideways from the node point by a free w >= 0 (the cusp opening Slots owns, SPEC-shape section 3 hand-on (1)); per piece a length of at least 1, split into one part per stretch the piece lies on, so a piece that holds events is cut at them (the reading the orchestrator ruled intended, not the literal sum over a shared piece); the box is [0, W] x [0, Hb].
 * @param v The shaped variant.
 * @param families Which quantities of Slots' least answer to fix: bit 1 one lane at each cusp, bit 2 one tile per deck step, bit 4 the pinned gaps at the ports, bit 8 a free sideways step in every stretch, bit 16 (with 8) whole-lane steps and none inside a deck, bit 32 (with 8 and 16) each step n = p - m with 1000 per unit of p and m, bit 64 (with 1, 2, 4) the homogeneous closure: every constant 0, every part may be 0, and a max-support LP finds the parts that cannot be stretched.
 * @return The answer.
 */
static ClosureResult ClosureLp(const PlanarShapeVariant &v, int families = 0)
{
	const PlanarTrackMap &map = v.structure.map;
	const size_t nt = map.tracks.size();
	JunctionMilp lp;
	std::vector<std::string> tags;
	const auto row = [&](std::vector<std::pair<uint32_t, double>> terms, double lower, double upper, const std::string &tag) {
		std::sort(terms.begin(), terms.end());
		std::vector<std::pair<uint32_t, double>> merged;
		for (const auto &[variable, coefficient] : terms) {
			if (!merged.empty() && merged.back().first == variable) {
				merged.back().second += coefficient;
			} else {
				merged.emplace_back(variable, coefficient);
			}
		}
		lp.AddRow(merged, lower, upper);
		tags.push_back(tag);
	};
	const bool homogeneous = (families & 64) != 0;
	const uint32_t width = lp.AddVariable(homogeneous ? 0 : 1, JunctionMilp::INF, homogeneous ? 0 : 1, false), height = lp.AddVariable(homogeneous ? 0 : 1, JunctionMilp::INF, homogeneous ? 0 : 1, false);
	std::vector<std::vector<std::pair<uint32_t, uint32_t>>> point(nt); ///< Per track and point, its (x, y).
	for (size_t t = 0; t < nt; t++) {
		for (size_t i = 0; i < map.events[t].size() + 2; i++) {
			const uint32_t x = lp.AddVariable(0, JunctionMilp::INF, 0, false), y = lp.AddVariable(0, JunctionMilp::INF, 0, false);
			point[t].emplace_back(x, y);
			row({{x, 1}, {width, -1}}, -JunctionMilp::INF, 0, "box");
			row({{y, 1}, {height, -1}}, -JunctionMilp::INF, 0, "box");
		}
	}
	struct StepVar {
		uint32_t track, stretch, side, p, m;
		int heading;
	};
	std::vector<StepVar> step_vars; ///< One per whole-lane step variable.
	std::set<std::pair<uint32_t, size_t>> in_deck; ///< The (track, stretch) pairs strictly between two crossings of one structure's deck.
	for (const PlanarStructure &st : v.structure.structures) {
		const auto event_of = [&](uint32_t t, uint32_t c) { return static_cast<size_t>(std::ranges::find(map.events[t], c) - map.events[t].begin()); };
		if (!st.crossings.empty()) {
			const size_t first = event_of(st.separator, st.crossings.front()), last = event_of(st.separator, st.crossings.back());
			for (size_t k = 1; k <= last; k++) if (k > first) in_deck.emplace(st.separator, k);
		}
		if (st.copy != UINT32_MAX) {
			for (size_t k = 1; k < map.events[st.copy].size(); k++) in_deck.emplace(st.copy, k);
		}
	}
	struct SupportPart {
		uint32_t track, stretch, piece, part, t;
	};
	std::vector<SupportPart> support; ///< With bit 64, one per non-exempt part.
	for (uint32_t t = 0; t < nt; t++) {
		const PlanarShapeTrack &s = v.tracks[t];
		std::vector<std::vector<uint32_t>> parts(s.pieces.size());
		for (size_t k = 0; k <= map.events[t].size(); k++) {
			const size_t first = k == 0 ? 0 : s.event_piece[k - 1], last = k == map.events[t].size() ? s.pieces.size() - 1 : s.event_piece[k];
			std::vector<std::pair<uint32_t, double>> dx = {{point[t][k + 1].first, 1}, {point[t][k].first, -1}}, dy = {{point[t][k + 1].second, 1}, {point[t][k].second, -1}};
			for (size_t j = first; j <= last; j++) {
				const uint32_t part = lp.AddVariable(0, JunctionMilp::INF, 0, false);
				parts[j].push_back(part);
				if (homogeneous && !in_deck.contains({t, k})) {
					const uint32_t support_t = lp.AddVariable(0, 1, -1, false);
					row({{support_t, 1}, {part, -1}}, -JunctionMilp::INF, 0, "support");
					support.push_back({t, static_cast<uint32_t>(k), static_cast<uint32_t>(j), part, support_t});
				}
				const auto &step = PlanarModel::STEP[((s.pieces[j] % 8) + 8) % 8];
				dx.emplace_back(part, -step[0]);
				dy.emplace_back(part, -step[1]);
			}
			if ((families & 8) && !((families & 16) && in_deck.contains({t, k}))) {
				const int heading = ((s.pieces[first] % 8) + 8) % 8;
				const bool whole = (families & 16) != 0;
					const bool split_pm = whole && (families & 32) != 0;
				const uint32_t side = split_pm ? UINT32_MAX : (whole ? lp.AddVariable(-64, 64, 0, true) : lp.AddVariable(-JunctionMilp::INF, JunctionMilp::INF, 0, false));
				const uint32_t p = split_pm ? lp.AddVariable(0, 64, 1000, true) : UINT32_MAX, m = split_pm ? lp.AddVariable(0, 64, 1000, true) : UINT32_MAX;
				const double lane = whole ? (heading % 2 == 0 ? 2 : 1) : 1;
				const auto &right = PlanarModel::Right(heading);
				if (split_pm) {
					dx.emplace_back(p, -right[0] * lane);
					dx.emplace_back(m, right[0] * lane);
					dy.emplace_back(p, -right[1] * lane);
					dy.emplace_back(m, right[1] * lane);
				} else {
					dx.emplace_back(side, -right[0] * lane);
					dy.emplace_back(side, -right[1] * lane);
				}
				if (whole) step_vars.push_back({t, static_cast<uint32_t>(k), side, p, m, heading});
			}
			row(dx, 0, 0, fmt::format("stretch {} {} x {}", t, k, map.tracks[t].name));
			row(dy, 0, 0, fmt::format("stretch {} {} y {}", t, k, map.tracks[t].name));
		}
		for (size_t j = 0; j < parts.size(); j++) {
			std::vector<std::pair<uint32_t, double>> total;
			for (const uint32_t part : parts[j]) total.emplace_back(part, 1);
			row(total, homogeneous ? 0 : 1, JunctionMilp::INF, fmt::format("piece {} {} {}", t, j, map.tracks[t].name));
		}
	}
	const auto tie = [&](std::pair<uint32_t, uint32_t> a, std::pair<uint32_t, uint32_t> b, const std::string &tag) {
		row({{a.first, 1}, {b.first, -1}}, 0, 0, tag);
		row({{a.second, 1}, {b.second, -1}}, 0, 0, tag);
	};
	/* A parallel leg of a node (not the site's straight leg, at the trunk's heading mod 8) is offset from the node point sideways by w >= 0 lane steps: the cusp opening Slots owns. */
	for (size_t n = 0; n < map.expanded.nodes.size(); n++) {
		const PlanarNode &node = map.expanded.nodes[n];
		const PlanarStructureSite *site = nullptr;
		for (const PlanarStructureSite &candidate : v.structure.sites) {
			if (candidate.node == n) site = &candidate;
		}
		const bool split = node.in.size() == 1 && (node.out.size() != 1 || (site != nullptr && site->split));
		const std::vector<uint32_t> &trunks = split ? node.in : node.out, &legs = split ? node.out : node.in;
		const uint32_t trunk = trunks[0];
		std::string tag = fmt::format("node {} trunk", n);
		for (const uint32_t t : trunks) tag += " " + map.tracks[t].name;
		tag += " legs";
		for (const uint32_t t : legs) tag += " " + map.tracks[t].name;
		const auto reference = split ? point[trunk].back() : point[trunk].front();
		const int h = (((split ? v.tracks[trunk].pieces.back() : v.tracks[trunk].pieces.front()) % 8) + 8) % 8;
		for (const uint32_t t : trunks) tie(reference, split ? point[t].back() : point[t].front(), tag);
		for (const uint32_t leg : legs) {
			const auto leg_point = split ? point[leg].front() : point[leg].back();
			const int heading = (((split ? v.tracks[leg].pieces.front() : v.tracks[leg].pieces.back()) % 8) + 8) % 8;
			const bool fork = site != nullptr && site->straight == UINT32_MAX && legs.size() == 2;
			const bool straight = site != nullptr && leg == site->straight;
			if (straight || heading != h || site == nullptr) {
				tie(reference, leg_point, tag);
				continue;
			}
			const uint32_t stretch = split ? 0 : static_cast<uint32_t>(map.events[leg].size());
			int sigma;
			if (fork) {
				const uint32_t other = legs[0] == leg ? legs[1] : legs[0];
				sigma = map.Side(leg, stretch, other, split ? 0 : static_cast<uint32_t>(map.events[other].size()));
			} else {
				sigma = map.Side(leg, stretch, site->straight, split ? 0 : static_cast<uint32_t>(map.events[site->straight].size()));
				if (sigma == 0) sigma = site->side;
			}
			const uint32_t w = lp.AddVariable(0, JunctionMilp::INF, 0, false);
			if (families & 1) row({{w, 1}}, homogeneous ? 0 : (h % 2 == 0 ? 2 : 1), homogeneous ? 0 : (h % 2 == 0 ? 2 : 1), fmt::format("cusp {} trunk {} leg {}", n, map.tracks[trunk].name, map.tracks[leg].name));
			const auto &step = PlanarModel::STEP[(h + 2) % 8];
			row({{leg_point.first, 1}, {reference.first, -1}, {w, -sigma * step[0]}}, 0, 0, tag);
			row({{leg_point.second, 1}, {reference.second, -1}, {w, -sigma * step[1]}}, 0, 0, tag);
		}
	}
	for (uint32_t c = 0; c < map.crossings.size(); c++) {
		const uint32_t a = map.crossings[c].a, b = map.crossings[c].b;
		const auto at = [&](uint32_t t) { return point[t][static_cast<size_t>(std::ranges::find(map.events[t], c) - map.events[t].begin()) + 1]; };
		tie(at(a), at(b), fmt::format("cross {} {} {}", c, map.tracks[a].name, map.tracks[b].name));
	}
	for (uint32_t t = 0; t < nt; t++) {
		const PlanarLine &line = map.expanded.lines[t];
		for (const bool end : {false, true}) {
			const PlanarEnd &e = end ? line.to : line.from;
			if (!e.port) continue;
			const auto [x, y] = end ? point[t].back() : point[t].front();
			const std::string tag = fmt::format("port {} {}", t, map.tracks[t].name);
			switch (map.expanded.ports[e.index].side) {
				case PlanarSide::W: row({{x, 1}}, 0, 0, tag); break;
				case PlanarSide::E: row({{x, 1}, {width, -1}}, 0, 0, tag); break;
				case PlanarSide::N: row({{y, 1}}, 0, 0, tag); break;
				case PlanarSide::S: row({{y, 1}, {height, -1}}, 0, 0, tag); break;
			}
		}
	}
	if (families & 6) {
		const auto event_index = [&](uint32_t t, uint32_t c) { return static_cast<size_t>(std::ranges::find(map.events[t], c) - map.events[t].begin()); };
		const auto deck_step = [&](uint32_t deck, size_t index, std::pair<uint32_t, uint32_t> from, std::pair<uint32_t, uint32_t> to, const std::string &tag) {
			const int h = ((v.tracks[deck].pieces[v.tracks[deck].event_piece[index]] % 8) + 8) % 8;
			const auto &step = PlanarModel::STEP[h];
			row({{to.first, 1}, {from.first, -1}}, homogeneous ? 0 : 2.0 * step[0], homogeneous ? 0 : 2.0 * step[0], tag);
			row({{to.second, 1}, {from.second, -1}}, homogeneous ? 0 : 2.0 * step[1], homogeneous ? 0 : 2.0 * step[1], tag);
		};
		if (families & 2) for (size_t i = 0; i < v.structure.structures.size(); i++) {
			const PlanarStructure &s = v.structure.structures[i];
			const std::string tag = fmt::format("deck {}", i);
			for (size_t k = 1; k < s.crossings.size(); k++) {
				const size_t a = event_index(s.separator, s.crossings[k - 1]), b = event_index(s.separator, s.crossings[k]);
				deck_step(s.separator, b, point[s.separator][a + 1], point[s.separator][b + 1], tag);
			}
			if (s.copy != UINT32_MAX) {
				for (size_t k = 1; k < map.events[s.copy].size(); k++) deck_step(s.copy, k, point[s.copy][k], point[s.copy][k + 1], tag);
			}
		}
		const std::vector<uint32_t> &ring = map.expanded.ring;
		const auto port_end = [&](uint32_t t) {
			const PlanarLine &line = map.expanded.lines[t];
			return line.from.port ? line.from : line.to;
		};
		if (families & 4) for (size_t i = 1; i < ring.size(); i++) {
			const uint32_t t1 = ring[i - 1], t2 = ring[i];
			const PlanarLine &l1 = map.expanded.lines[t1], &l2 = map.expanded.lines[t2];
			const PlanarEnd e1 = port_end(t1), e2 = port_end(t2);
			const PlanarSide side = map.expanded.ports[e1.index].side;
			if (side != map.expanded.ports[e2.index].side) continue;
			const auto p1 = l1.from.port ? point[t1].front() : point[t1].back(), p2 = l2.from.port ? point[t2].front() : point[t2].back();
			const double d = homogeneous ? 0 : (e1.index == e2.index ? 2 : 2.0 * v.structure.side_gap[static_cast<size_t>(side)]);
			const std::string tag = fmt::format("pin {} {} {} d={}", static_cast<int>(side), map.tracks[t1].name, map.tracks[t2].name, d);
			switch (side) {
				case PlanarSide::N: row({{p2.first, 1}, {p1.first, -1}}, d, d, tag); break;
				case PlanarSide::E: row({{p2.second, 1}, {p1.second, -1}}, d, d, tag); break;
				case PlanarSide::S: row({{p2.first, 1}, {p1.first, -1}}, -d, -d, tag); break;
				case PlanarSide::W: row({{p2.second, 1}, {p1.second, -1}}, -d, -d, tag); break;
			}
		}
	}
	lp.SetTimeLimit(15.0);
	const auto start = std::chrono::steady_clock::now();
	const JunctionMilp::Status status = lp.Solve();
	ClosureResult r;
	r.micros = static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count());
	r.closes = status == JunctionMilp::Status::Optimal;
	r.why = r.closes ? "Optimal" : (status == JunctionMilp::Status::Infeasible ? "Infeasible" : "Error");
	if (status == JunctionMilp::Status::Infeasible) {
		std::set<std::string> unique;
		for (const uint32_t i : lp.InfeasibleRows()) unique.insert(tags[i]);
		for (const std::string &t : unique) r.why += " " + t + ";";
	}
	if (r.closes) {
		const auto node_kind = [&](uint32_t n) {
			const PlanarNode &node = map.expanded.nodes[n];
			const PlanarStructureSite *site = nullptr;
			for (const PlanarStructureSite &candidate : v.structure.sites) {
				if (candidate.node == n) site = &candidate;
			}
			const bool split = node.in.size() == 1 && (node.out.size() != 1 || (site != nullptr && site->split));
			return fmt::format("node {} {}", n, split ? "split" : "merge");
		};
		const auto end_text = [&](uint32_t t, bool end) {
			const PlanarEnd &e = end ? map.expanded.lines[t].to : map.expanded.lines[t].from;
			return e.port ? fmt::format("port {}", "NESW"[static_cast<int>(map.expanded.ports[e.index].side)]) : node_kind(e.index);
		};
		const auto cross_text = [&](uint32_t t, uint32_t c) {
			bool sep = false;
			for (const PlanarStructure &st : v.structure.structures) {
				if (st.copy == t || (st.separator == t && std::ranges::find(st.crossings, c) != st.crossings.end())) sep = true;
			}
			const uint32_t other = map.crossings[c].a == t ? map.crossings[c].b : map.crossings[c].a;
			return fmt::format("cross {} {}", map.tracks[other].name, sep ? "sep" : "crosser");
		};
		for (const StepVar &sv : step_vars) {
			const int n = static_cast<int>(std::lround(sv.side != UINT32_MAX ? lp.Value(sv.side) : lp.Value(sv.p) - lp.Value(sv.m)));
			if (n == 0) continue;
			const size_t events = map.events[sv.track].size();
			const std::string from = sv.stretch == 0 ? end_text(sv.track, false) : cross_text(sv.track, map.events[sv.track][sv.stretch - 1]);
			const std::string to = sv.stretch == events ? end_text(sv.track, true) : cross_text(sv.track, map.events[sv.track][sv.stretch]);
			r.steps.push_back(fmt::format("{} stretch={} n={} heading={} from={} to={}", map.tracks[sv.track].name, sv.stretch, n, sv.heading, from, to));
		}
		for (const SupportPart &sp : support) {
			if (lp.Value(sp.t) >= 0.5) continue;
			const size_t events = map.events[sp.track].size();
			const std::string from = sp.stretch == 0 ? end_text(sp.track, false) : cross_text(sp.track, map.events[sp.track][sp.stretch - 1]);
			const std::string to = sp.stretch == events ? end_text(sp.track, true) : cross_text(sp.track, map.events[sp.track][sp.stretch]);
			r.rigid.push_back(fmt::format("{} stretch={} piece={} heading={} from={} to={}", map.tracks[sp.track].name, sp.stretch, sp.piece, ((v.tracks[sp.track].pieces[sp.piece] % 8) + 8) % 8, from, to));
		}
	}
	return r;
}

TEST_CASE("planar shape layer: t3 first variant shapes", "[junction][planar][planarshape]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	std::string error;
	const PlanarTopology topology = ParsePlanarTopology(PlanarTText(3), &error);
	REQUIRE(error.empty());
	const PlanarStructureResult r = BuildPlanarStructure(topology, spec, 1);
	REQUIRE(!r.variants.empty());
	PlanarShapeVariant shaped;
	std::string why;
	const bool ok = ShapePlanarStructureVariant(r.variants[0], shaped, why);
	INFO("why: " << why);
	REQUIRE(ok);
	const std::vector<std::string> messages = CheckPlanarShapePromise(topology, spec, shaped);
	for (const std::string &message : messages) {
		INFO(message);
		CHECK(message.empty());
	}
	CHECK(messages.empty());
}

TEST_CASE("planar shape layer: probe, the heading program and the closure LP on T2, T3 and LL2", "[junction][planar][planarshape][slow]")
{
	const std::vector<std::pair<std::string, std::string>> hubs = {{"T2", PlanarTText(2)}, {"T3", PlanarTText(3)}, {"LL2", PlanarLL2Text()}};
	for (const auto &[name, text] : hubs) {
		std::string error;
		const PlanarTopology topology = ParsePlanarTopology(text, &error);
		REQUIRE(error.empty());
		const JunctionSpec spec = MakeTestJunctionSpec();
		const PlanarStructureResult r = BuildPlanarStructure(topology, spec, 16);
		size_t shaped = 0, unshaped = 0, closed = 0, open = 0;
		std::map<std::string, size_t> breaches;
		int cost_min = INT32_MAX, cost_max = INT32_MIN;
		double program_max = 0, program_sum = 0, lp_max = 0, lp_sum = 0;
		for (const PlanarStructureVariant &variant : r.variants) {
			PlanarShapeVariant out;
			std::string why;
			const auto start = std::chrono::steady_clock::now();
			const bool ok = ShapePlanarStructureVariant(variant, out, why);
			const double micros = static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count());
			program_max = std::max(program_max, micros);
			program_sum += micros;
			if (!ok) {
				unshaped++;
				std::cout << fmt::format("PROBE {} {} UNSHAPED {}\n", name, variant.tag, why);
				continue;
			}
			shaped++;
			cost_min = std::min(cost_min, out.cost);
			cost_max = std::max(cost_max, out.cost);
			for (const std::string &m : CheckPlanarShapePromise(topology, spec, out)) {
				breaches[m.substr(0, m.find(':'))]++;
				if (!m.starts_with("H6:")) std::cout << fmt::format("PROBE {} {} BREACH {}\n", name, variant.tag, m);
			}
			const ClosureResult closure = ClosureLp(out);
			lp_max = std::max(lp_max, closure.micros);
			lp_sum += closure.micros;
			if (closure.closes) {
				closed++;
			} else {
				open++;
				std::cout << fmt::format("PROBE {} {} OPEN {}\n", name, variant.tag, closure.why);
			}
		}
		const size_t n = r.variants.size();
		std::string breach_text;
		for (const auto &[id, count] : breaches) breach_text += fmt::format(" {}={}", id, count);
		std::cout << fmt::format("PROBE {} variants={} shaped={} unshaped={} breaches:{} closure feasible={} infeasible={} cost min={} max={} program ms max={:.1f} mean={:.1f} lp ms max={:.1f} mean={:.1f}\n",
				name, n, shaped, unshaped, breach_text.empty() ? " none" : breach_text, closed, open, shaped > 0 ? cost_min : -1, shaped > 0 ? cost_max : -1,
				program_max / 1000, n > 0 ? program_sum / 1000 / static_cast<double>(n) : 0.0, lp_max / 1000, shaped > 0 ? lp_sum / 1000 / static_cast<double>(shaped) : 0.0);
		CHECK(shaped + unshaped == n);
	}
}

TEST_CASE("planar shape: BuildPlanarShape on T2, T3 and LL2", "[junction][planar][planarshape][slow]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const std::vector<std::pair<std::string, std::string>> hubs = {{"T2", PlanarTText(2)}, {"T3", PlanarTText(3)}, {"LL2", PlanarLL2Text()}};
	for (const auto &[name, text] : hubs) {
		std::string error;
		const PlanarTopology topology = ParsePlanarTopology(text, &error);
		REQUIRE(error.empty());
		const auto start = std::chrono::steady_clock::now();
		const PlanarShapeResult result = BuildPlanarShape(BuildPlanarStructure(topology, spec, 16));
		const double millis = static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count()) / 1000;
		std::string line = fmt::format("SHAPE {} variants={} unshaped={} {:.1f} ms:", name, result.variants.size(), result.unshaped.size(), millis);
		for (const PlanarShapeVariant &v : result.variants) line += fmt::format(" {}({},{})", v.structure.tag, v.structure.cost, v.cost);
		std::cout << line << "\n";
		for (const auto &[tag, why] : result.unshaped) std::cout << fmt::format("SHAPE {} {} UNSHAPED {}\n", name, tag, why);
		INFO(name << " why: " << result.why);
		REQUIRE(result.variants.size() == 16);
		REQUIRE(result.unshaped.empty());
		for (size_t i = 0; i < result.variants.size(); i++) {
			const PlanarShapeVariant &v = result.variants[i];
			for (const std::string &m : CheckPlanarShapePromise(topology, spec, v)) {
				INFO(name << " " << v.structure.tag << " " << m);
				CHECK(m.empty());
			}
			if (i == 0) continue;
			const PlanarShapeVariant &before = result.variants[i - 1];
			CHECK(std::tie(before.structure.cost, before.cost, before.structure.tag) <= std::tie(v.structure.cost, v.cost, v.structure.tag));
		}
	}
}

TEST_CASE("planar sweep: Structure and Shape over T1-T3, LL2 and LL3a", "[junction][planar][planarshape][.probe]")
{
	static const std::string ll3a = R"(topology LL3a
port W in E: EF1, EF2, EF3
port W out W: WO1=C1+B1, WO2=C2+B2, WO3=C3+B3
port E out E: EO1=E1+D1, EO2=E2+D2, EO3=E3+D3
port E in W: AB1, AB2, AB3
port S in N: CD1, CD2, CD3
port S out S: SO1=A1+F1, SO2=A2+F2, SO3=A3+F3
node EF1, EF2, EF3 -> E1, F1, E2, F2, E3, F3
node AB1, AB2, AB3 -> A1, B1, A2, B2, A3, B3
node CD1, CD2, CD3 -> C1, D1, C2, D2, C3, D3
node E1, D1 -> EO1
node E2, D2 -> EO2
node E3, D3 -> EO3
node C1, B1 -> WO1
node C2, B2 -> WO2
node C3, B3 -> WO3
node A1, F1 -> SO1
node A2, F2 -> SO2
node A3, F3 -> SO3
prio E1, E2, E3, B1, B2, B3, A1, F2, A3
)";
	const std::vector<std::pair<std::string, std::string>> hubs = {{"T1", PlanarTText(1)}, {"T2", PlanarTText(2)}, {"T3", PlanarTText(3)}, {"LL2", PlanarLL2Text()}, {"LL3a", ll3a}};
	const JunctionSpec spec = MakeTestJunctionSpec();
	const auto millis = [](std::chrono::steady_clock::time_point from) { return static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - from).count()) / 1000; };
	for (const auto &[name, text] : hubs) {
		std::string error;
		const PlanarTopology topology = ParsePlanarTopology(text, &error);
		if (!error.empty()) {
			std::cout << fmt::format("SWEEP {} PARSE {}\n", name, error);
			CHECK(name == "LL3a");
			continue;
		}
		const PlanarChannels channels = BuildPlanarChannels(topology);
		auto start = std::chrono::steady_clock::now();
		PlanarStructureResult structure = BuildPlanarStructure(topology, spec, 16);
		const double struct_ms = millis(start);
		const size_t variants = structure.variants.size();
		size_t struct_breaches = 0;
		std::map<std::string, size_t> printed;
		const auto breach = [&](const std::string &tag, const std::string &m, const std::string &oracle) {
			if (printed[oracle]++ < 3) std::cout << fmt::format("SWEEP {} BREACH {} {}\n", name, tag, m);
		};
		for (const PlanarStructureVariant &v : structure.variants) {
			for (const std::string &m : CheckPlanarStructurePromise(topology, spec, v)) {
				struct_breaches++;
				breach(v.tag, m, "structure");
			}
		}
		if (structure.variants.empty()) {
			std::cout << fmt::format("SWEEP {} STRUCTURE_WHY {}\n", name, structure.why);
			for (const auto &[reason, count] : structure.skipped) std::cout << fmt::format("SWEEP {} SKIPPED {} {}\n", name, count, reason);
			std::cout << fmt::format("SWEEP {} evaluated={}\n", name, structure.evaluated);
		}
		start = std::chrono::steady_clock::now();
		const PlanarShapeResult shape = BuildPlanarShape(std::move(structure));
		const double shape_ms = millis(start);
		size_t shape_breaches = 0, closes = 0, c1_bad = 0, c1_pairs = 0, c2_bad = 0, c2_structures = 0;
		size_t c1_printed = 0, c2_printed = 0;
		for (const auto &[tag, why] : shape.unshaped) std::cout << fmt::format("SWEEP {} UNSHAPED {} {}\n", name, tag, why);
		for (const PlanarShapeVariant &v : shape.variants) {
			const std::string &tag = v.structure.tag;
			for (const std::string &m : CheckPlanarShapePromise(topology, spec, v)) {
				shape_breaches++;
				breach(tag, m, "shape");
			}
			if (ClosureLp(v).closes) closes++;
			const PlanarTrackMap &map = v.structure.map;
			/* The heading of track t at crossing c, in eighths. */
			const auto heading = [&](uint32_t t, uint32_t c) {
				const size_t k = static_cast<size_t>(std::ranges::find(map.events[t], c) - map.events[t].begin());
				return ((v.tracks[t].pieces[v.tracks[t].event_piece[k]] % 8) + 8) % 8;
			};
			const auto line_of = [&](uint32_t t) { return t < topology.lines.size() ? t : map.part_of[t]; };
			std::map<std::pair<uint32_t, uint32_t>, std::set<int>> keys;
			for (uint32_t c = 0; c < map.crossings.size(); c++) {
				const std::array<uint32_t, 2> tracks = {map.crossings[c].a, map.crossings[c].b};
				for (size_t i = 0; i < 2; i++) {
					const uint32_t t = tracks[i], other = tracks[1 - i];
					const uint32_t line = line_of(t), other_line = line_of(other);
					if (line == UINT32_MAX || other_line == UINT32_MAX) continue;
					keys[{channels.channel_of_line[line], channels.channel_of_line[other_line]}].insert(heading(t, c));
				}
			}
			for (const auto &[key, headings] : keys) {
				c1_pairs++;
				if (headings.size() <= 1) continue;
				c1_bad++;
				if (c1_printed++ < 5) {
					std::string list;
					for (const int h : headings) list += fmt::format("{} ", h);
					std::cout << fmt::format("SWEEP {} C1 {} ch{} x ch{} headings={}\n", name, tag, key.first, key.second, list);
				}
			}
			for (size_t i = 0; i < v.structure.structures.size(); i++) {
				const PlanarStructure &s = v.structure.structures[i];
				if (s.crossers.size() < 2) continue;
				c2_structures++;
				std::set<int> lines;
				std::string list;
				for (size_t k = 0; k < s.crossers.size(); k++) {
					const int h = heading(s.crossers[k], s.crossings[k]);
					lines.insert(h % 4);
					list += fmt::format("{} ", h);
				}
				if (lines.size() <= 1) continue;
				c2_bad++;
				if (c2_printed++ < 5) std::cout << fmt::format("SWEEP {} C2 {} structure {} sep {} headings={}\n", name, tag, i, s.separator, list);
			}
		}
		std::cout << fmt::format("SWEEP {} variants={} struct_ms={:.1f} struct_breaches={} shaped={} unshaped={} shape_ms={:.1f} shape_breaches={} closes={} c1_bad={} c1_pairs={} c2_bad={} c2_structures={}\n",
				name, variants, struct_ms, struct_breaches, shape.variants.size(), shape.unshaped.size(), shape_ms, shape_breaches, closes, c1_bad, c1_pairs, c2_bad, c2_structures);
		double shape_cost_sum = 0;
		for (const PlanarShapeVariant &v : shape.variants) shape_cost_sum += v.cost;
		std::cout << fmt::format("SWEEP {} shape_cost_sum={}\n", name, shape_cost_sum);
	}
}

TEST_CASE("planar probe: closure with Slots' least exact answer on T1-T3 and LL2", "[junction][planar][planarshape][.probe]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const std::vector<std::pair<std::string, std::string>> hubs = {{"T1", PlanarTText(1)}, {"T2", PlanarTText(2)}, {"T3", PlanarTText(3)}, {"LL2", PlanarLL2Text()}};
	for (const auto &[name, text] : hubs) {
		std::string error;
		const PlanarTopology topology = ParsePlanarTopology(text, &error);
		REQUIRE(error.empty());
		const PlanarShapeResult shape = BuildPlanarShape(BuildPlanarStructure(topology, spec, 16));
		size_t closes_free = 0, closes_exact = 0;
		double ms_max = 0;
		std::vector<std::string> open;
		for (const PlanarShapeVariant &v : shape.variants) {
			const ClosureResult free = ClosureLp(v, 0), exact = ClosureLp(v, 7);
			if (free.closes) closes_free++;
			if (exact.closes) closes_exact++; else open.push_back(fmt::format("X2A {} {} OPEN {}\n", name, v.structure.tag, exact.why));
			ms_max = std::max({ms_max, free.micros / 1000, exact.micros / 1000});
		}
		std::cout << fmt::format("X2A {} shaped={} closes_free={} closes_exact={} lp_ms_max={:.1f}\n", name, shape.variants.size(), closes_free, closes_exact, ms_max);
		for (const std::string &line : open) std::cout << line;
		for (const int mask : {1, 2, 4, 3, 5, 6}) {
			size_t closes = 0;
			for (const PlanarShapeVariant &v : shape.variants) if (ClosureLp(v, mask).closes) closes++;
			std::cout << fmt::format("X2B {} mask={} closes={}/{}\n", name, mask, closes, shape.variants.size());
		}
		if (name != "T1" && name != "LL2" && !shape.variants.empty()) {
			const PlanarShapeVariant &v = shape.variants.front();
			const PlanarTrackMap &map = v.structure.map;
			const ClosureResult c = ClosureLp(v, 4);
			if (!c.closes) std::cout << fmt::format("X2C {} {} mask=4 WHY {}\n", name, v.structure.tag, c.why);
			const char *const side_name = "NESW";
			const char *const heading_name[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
			for (size_t i = 0; i < map.expanded.ring.size(); i++) {
				const uint32_t t = map.expanded.ring[i];
				const PlanarLine &line = map.expanded.lines[t];
				const PlanarEnd e = line.from.port ? line.from : line.to;
				const PlanarPort &port = map.expanded.ports[e.index];
				std::cout << fmt::format("X2C {} RING {} {} side={} port={} {} heading={}\n", name, i, map.tracks[t].name, side_name[static_cast<int>(port.side)], e.index, port.in ? "in" : "out", heading_name[static_cast<int>(port.heading)]);
			}
			for (size_t i = 0; i < map.expanded.ring.size(); i++) {
				const uint32_t t = map.expanded.ring[i];
				std::string pieces;
				for (const int piece : v.tracks[t].pieces) pieces += fmt::format("{},", ((piece % 8) + 8) % 8);
				std::cout << fmt::format("X2C {} TRACK {} pieces={} events={}\n", name, map.tracks[t].name, pieces, map.events[t].size());
			}
		}
		for (const int mask : {8, 12, 9, 10, 15}) {
			size_t closes = 0;
			std::string why;
			for (const PlanarShapeVariant &v : shape.variants) {
				const ClosureResult c = ClosureLp(v, mask);
				if (c.closes) {
					closes++;
				} else if (mask == 15 && why.empty() && (name == "T2" || name == "T3")) {
					why = fmt::format("X2D {} {} mask=15 WHY {}\n", name, v.structure.tag, c.why);
				}
			}
			std::cout << fmt::format("X2D {} mask={} closes={}/{}\n", name, mask, closes, shape.variants.size());
			std::cout << why;
		}
		for (const int mask : {24, 31}) {
			size_t closes = 0;
			double ms_max_e = 0;
			std::string why;
			for (const PlanarShapeVariant &v : shape.variants) {
				const ClosureResult c = ClosureLp(v, mask);
				ms_max_e = std::max(ms_max_e, c.micros / 1000);
				if (c.closes) {
					closes++;
				} else if (mask == 31 && why.empty()) {
					why = fmt::format("X2E {} {} mask=31 WHY {}\n", name, v.structure.tag, c.why);
				}
			}
			std::cout << fmt::format("X2E {} mask={} closes={}/{} ms_max={:.1f}\n", name, mask, closes, shape.variants.size(), ms_max_e);
			std::cout << why;
		}
			{
			size_t closes = 0, steps_min = SIZE_MAX, steps_max = 0, lanes_min = SIZE_MAX;
			double ms_max_f = 0;
			std::map<std::string, size_t> tally;
			const auto number_of = [](const std::string &entry, const std::string &key) { const size_t at = entry.find(key) + key.size(); return entry.substr(at, entry.find(' ', at) - at); };
			const auto kind_of = [](const std::string &text) {
				if (text.starts_with("port")) return std::string("port");
				if (text.starts_with("node")) return text.substr(text.rfind(' ') + 1);
				return std::string("cross-") + text.substr(text.rfind(' ') + 1);
			};
			bool first = true;
			for (const PlanarShapeVariant &v : shape.variants) {
				const ClosureResult c = ClosureLp(v, 63);
				ms_max_f = std::max(ms_max_f, c.micros / 1000);
				if (!c.closes) continue;
				closes++;
				size_t lanes = 0;
				for (const std::string &entry : c.steps) {
						const std::string n_text = number_of(entry, "n=");
						int n_value = 0;
						std::from_chars(n_text.data(), n_text.data() + n_text.size(), n_value);
						lanes += static_cast<size_t>(n_value < 0 ? -n_value : n_value);
					const size_t f = entry.find(" from=") + 6, t = entry.find(" to=");
					tally[kind_of(entry.substr(f, t - f)) + "->" + kind_of(entry.substr(t + 4))]++;
					if (first) std::cout << fmt::format("X2F {} {} STEP {}\n", name, v.structure.tag, entry);
				}
				first = false;
				steps_min = std::min(steps_min, c.steps.size());
				steps_max = std::max(steps_max, c.steps.size());
				lanes_min = std::min(lanes_min, lanes);
			}
			std::cout << fmt::format("X2F {} closes={}/{} steps_min={} steps_max={} lanes_sum_min={} ms_max={:.1f}\n", name, closes, shape.variants.size(), closes == 0 ? 0 : steps_min, steps_max, closes == 0 ? 0 : lanes_min, ms_max_f);
			for (const auto &[pair, count] : tally) std::cout << fmt::format("X2F {} TALLY {} {}\n", name, pair, count);
		}
		{
			size_t rigid_min = SIZE_MAX, rigid_max = 0, parts_total = 0;
			std::map<std::string, size_t> tally;
			bool first = true;
			for (const PlanarShapeVariant &v : shape.variants) {
				const ClosureResult c = ClosureLp(v, 71);
				rigid_min = std::min(rigid_min, c.rigid.size());
				rigid_max = std::max(rigid_max, c.rigid.size());
				std::set<std::string> hit;
				for (const std::string &entry : c.rigid) {
					hit.insert(entry.substr(0, entry.find(' ')));
					if (first) std::cout << fmt::format("X2G {} {} RIGID {}\n", name, v.structure.tag, entry);
				}
				for (const std::string &track : hit) tally[track]++;
				if (first) {
					const PlanarTrackMap &map = v.structure.map;
					std::set<std::pair<uint32_t, size_t>> deck;
					for (const PlanarStructure &st : v.structure.structures) {
						const auto event_of = [&](uint32_t t, uint32_t cr) { return static_cast<size_t>(std::ranges::find(map.events[t], cr) - map.events[t].begin()); };
						if (!st.crossings.empty()) {
							const size_t f = event_of(st.separator, st.crossings.front()), l = event_of(st.separator, st.crossings.back());
							for (size_t k = f + 1; k <= l; k++) deck.emplace(st.separator, k);
						}
						if (st.copy != UINT32_MAX) for (size_t k = 1; k < map.events[st.copy].size(); k++) deck.emplace(st.copy, k);
					}
					for (uint32_t t = 0; t < map.tracks.size(); t++) {
						const PlanarShapeTrack &s = v.tracks[t];
						for (size_t k = 0; k <= map.events[t].size(); k++) {
							const size_t f = k == 0 ? 0 : s.event_piece[k - 1], l = k == map.events[t].size() ? s.pieces.size() - 1 : s.event_piece[k];
							if (!deck.contains({t, k})) parts_total += l - f + 1;
						}
					}
				}
				first = false;
			}
			std::cout << fmt::format("X2G {} variants={} rigid_min={} rigid_max={} parts_total={}\n", name, shape.variants.size(), shape.variants.empty() ? 0 : rigid_min, rigid_max, parts_total);
			for (const auto &[track, count] : tally) std::cout << fmt::format("X2G {} TALLY {} {}\n", name, track, count);
			size_t mask64_max = 0;
			for (const PlanarShapeVariant &v : shape.variants) mask64_max = std::max(mask64_max, ClosureLp(v, 64).rigid.size());
			std::cout << fmt::format("X2G {} mask=64 rigid_max={}\n", name, mask64_max);
		}
		if (name == "T1" && !shape.variants.empty()) {
			const PlanarShapeVariant &v = shape.variants.front();
			for (const int mask : {1, 2, 4}) {
				const ClosureResult c = ClosureLp(v, mask);
				if (!c.closes) std::cout << fmt::format("X2B T1 {} mask={} WHY {}\n", v.structure.tag, mask, c.why);
			}
		}
	}
}
