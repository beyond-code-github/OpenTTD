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
#include <chrono>
#include <cstdint>
#include <iostream>
#include <map>
#include <set>
#include <string>
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

/** The one-lane T: its tracks are the dump's, track by track. */
static ShapeCase MakeT1()
{
	ShapeCase c = MakeCase(PlanarTText(1), "o3t0f0m0");
	SetTracks(c, {
		{{2}, {}}, {{6}, {}}, {{2}, {}}, {{6}, {}}, {{0}, {}}, {{4}, {}},
		{{5, 4}, {}}, {{6}, {0, 0}}, {{0, -1}, {}}, {{0, 1}, {0, 0}},
		{{2}, {}}, {{3, 4}, {0, 1}}, {{6}, {}}, {{2}, {}}, {{6}, {}}, {{2}, {}},
	});
	c.v.angles = {2, 2, 3};
	REQUIRE(c.v.structure.structures.size() == 2);
	REQUIRE(c.v.structure.structures[0].separator == 9);
	REQUIRE(c.v.structure.structures[1].separator == 11);
	c.v.structures = {{0, {3, -1}, {1, -1}}, {4, {1, -1}, {1, -1}}};
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
	c.v.angles[2] = 2;
	c.v.structures[0].middles = {2, -1};
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
	RequireShaped(MakeT1(), 4);
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

/** The closure probe's answer for one shaped variant. */
struct ClosureResult {
	bool closes; ///< The headings close with positive lengths.
	std::string why; ///< The status and, when infeasible, the tags of the conflicting rows.
	double micros; ///< The time of the solve.
};

/**
 * Ask whether the headings of a shaped variant can close with positive lengths (the lengths-only LP of H2).
 * Per point (x, y); per piece a length of at least 1, split into one part per stretch the piece lies on, so a piece that holds events is cut at them (the reading the orchestrator ruled intended, not the literal sum over a shared piece); the box is [0, W] x [0, Hb].
 * @param v The shaped variant.
 * @return The answer.
 */
static ClosureResult ClosureLp(const PlanarShapeVariant &v)
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
	const uint32_t width = lp.AddVariable(1, JunctionMilp::INF, 1, false), height = lp.AddVariable(1, JunctionMilp::INF, 1, false);
	std::vector<std::vector<std::pair<uint32_t, uint32_t>>> point(nt); ///< Per track and point, its (x, y).
	for (size_t t = 0; t < nt; t++) {
		for (size_t i = 0; i < map.events[t].size() + 2; i++) {
			const uint32_t x = lp.AddVariable(0, JunctionMilp::INF, 0, false), y = lp.AddVariable(0, JunctionMilp::INF, 0, false);
			point[t].emplace_back(x, y);
			row({{x, 1}, {width, -1}}, -JunctionMilp::INF, 0, "box");
			row({{y, 1}, {height, -1}}, -JunctionMilp::INF, 0, "box");
		}
	}
	for (uint32_t t = 0; t < nt; t++) {
		const PlanarShapeTrack &s = v.tracks[t];
		std::vector<std::vector<uint32_t>> parts(s.pieces.size());
		for (size_t k = 0; k <= map.events[t].size(); k++) {
			const size_t first = k == 0 ? 0 : s.event_piece[k - 1], last = k == map.events[t].size() ? s.pieces.size() - 1 : s.event_piece[k];
			std::vector<std::pair<uint32_t, double>> dx = {{point[t][k + 1].first, 1}, {point[t][k].first, -1}}, dy = {{point[t][k + 1].second, 1}, {point[t][k].second, -1}};
			for (size_t j = first; j <= last; j++) {
				const uint32_t part = lp.AddVariable(0, JunctionMilp::INF, 0, false);
				parts[j].push_back(part);
				const auto &step = PlanarModel::STEP[((s.pieces[j] % 8) + 8) % 8];
				dx.emplace_back(part, -step[0]);
				dy.emplace_back(part, -step[1]);
			}
			row(dx, 0, 0, fmt::format("stretch {} {} x", t, k));
			row(dy, 0, 0, fmt::format("stretch {} {} y", t, k));
		}
		for (size_t j = 0; j < parts.size(); j++) {
			std::vector<std::pair<uint32_t, double>> total;
			for (const uint32_t part : parts[j]) total.emplace_back(part, 1);
			row(total, 1, JunctionMilp::INF, fmt::format("piece {} {}", t, j));
		}
	}
	const auto tie = [&](std::pair<uint32_t, uint32_t> a, std::pair<uint32_t, uint32_t> b, const std::string &tag) {
		row({{a.first, 1}, {b.first, -1}}, 0, 0, tag);
		row({{a.second, 1}, {b.second, -1}}, 0, 0, tag);
	};
	for (size_t n = 0; n < map.expanded.nodes.size(); n++) {
		const PlanarNode &node = map.expanded.nodes[n];
		const std::string tag = fmt::format("node {}", n);
		const auto reference = point[node.in[0]].back();
		for (const uint32_t t : node.in) tie(reference, point[t].back(), tag);
		for (const uint32_t t : node.out) tie(reference, point[t].front(), tag);
	}
	for (uint32_t c = 0; c < map.crossings.size(); c++) {
		const uint32_t a = map.crossings[c].a, b = map.crossings[c].b;
		const auto at = [&](uint32_t t) { return point[t][static_cast<size_t>(std::ranges::find(map.events[t], c) - map.events[t].begin()) + 1]; };
		tie(at(a), at(b), fmt::format("cross {}", c));
	}
	for (uint32_t t = 0; t < nt; t++) {
		const PlanarLine &line = map.expanded.lines[t];
		for (const bool end : {false, true}) {
			const PlanarEnd &e = end ? line.to : line.from;
			if (!e.port) continue;
			const auto [x, y] = end ? point[t].back() : point[t].front();
			const std::string tag = fmt::format("port {}", t);
			switch (map.expanded.ports[e.index].side) {
				case PlanarSide::W: row({{x, 1}}, 0, 0, tag); break;
				case PlanarSide::E: row({{x, 1}, {width, -1}}, 0, 0, tag); break;
				case PlanarSide::N: row({{y, 1}}, 0, 0, tag); break;
				case PlanarSide::S: row({{y, 1}, {height, -1}}, 0, 0, tag); break;
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
	/* Capacity (H6) is the capacity rows' job, which the heading program does not have yet (H3 of the roadmap): only H6 messages may remain. */
	const std::vector<std::string> messages = CheckPlanarShapePromise(topology, spec, shaped);
	for (const std::string &message : messages) {
		INFO(message);
		CHECK(message.starts_with("H6"));
	}
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
