/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file planar_domain_encoding.cpp Judge our keep-apart rules on fixed points against the domain on X1's small configurations (D428). */

#include "../../../stdafx.h"

#include "../../../3rdparty/catch2/catch.hpp"

#include "../../../junction/junction_spec.h"
#include "../../../junction/planar/planar_domain.h"
#include "../../../junction/planar/planar_domain_map.h"
#include "../../../junction/planar/planar_draw.h"
#include "../../../junction/planar/planar_shape_layer.h"
#include "../../../junction/planar/planar_structure.h"
#include "../../../junction/planar/planar_topology.h"

#include <bit>
#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <climits>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <functional>
#include <iostream>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

#include "../../../safeguards.h"

namespace {

using Point = std::array<int, 2>;
using Points = std::vector<Point>;

/** X1's variant with B1's shape text and crossing piece as given, loaded; n_at is the N port's declared line positions (tiles; the base's C1 and B1 ends); the flex pairs are not placed. */
bool LoadEncVariant(const std::string &b1_shape, const std::string &b1_at, PlanarShapeVariant &v, std::string &why, std::vector<std::string> *promise = nullptr, const std::string &n_at = "3 5")
{
	const std::string topo = "topology X1\nport W in E: AC1\nport S in N: BD1\nport N out N @ " + n_at + ": C1, B1\nport E out E @ 2 4: A1, D1\nnode AC1 -> C1, A1\nnode BD1 -> B1, D1\ncross A1: B1<\ncross B1: A1>\n";
	std::string error;
	const PlanarTopology t = ParsePlanarTopology(topo, &error);
	REQUIRE(error.empty());
	const JunctionSpec spec = MakeTestJunctionSpec();
	std::vector<std::string> report;
	const PlanarStructureVariant s = LoadPlanarStructureVariant(t, "tunnel T1 A1: B1\nstraight AC1: A1\nstraight BD1: B1\n", spec, report, &error);
	REQUIRE(error.empty());
	if (!LoadPlanarShapeVariant(t, s, "shape AC1: 2\nshape A1: 2\nshape C1: 1 0\nshape BD1: 0\nshape B1: " + b1_shape + "\nshape D1: 1 2\nat A1: B1@0\nat B1: " + b1_at + "\n", v, why)) return false;
	if (promise != nullptr) *promise = CheckPlanarShapePromise(t, spec, v);
	return true;
}

/** The loaded X1 variant with its flex pairs placed. */
PlanarShapeVariant LoadX1Enc()
{
	PlanarShapeVariant v;
	std::string why;
	REQUIRE(LoadEncVariant("0", "A1@0", v, why));
	REQUIRE(PlacePlanarShapeFlex(v, why));
	return v;
}

constexpr int STEPS[8][2] = {{0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}};

/** The program's point bounds (>= 0) are a normalisation, legality does not depend on where a drawing sits: a geometry reaching a negative coordinate is translated by an even (dx, dy), parity kept, so its least coordinate on each such axis is 2 or 3. */
void NormalisePoints(std::vector<std::vector<std::array<int, 2>>> &points)
{
	int least[2] = {INT_MAX, INT_MAX};
	for (const auto &track : points) {
		for (const auto &p : track) {
			for (int a = 0; a < 2; a++) least[a] = std::min(least[a], p[a]);
		}
	}
	int shift[2] = {0, 0};
	for (int a = 0; a < 2; a++) {
		if (least[a] < 0 && least[a] != INT_MAX) shift[a] = 2 * ((2 - least[a] + 1) / 2);
	}
	if (shift[0] == 0 && shift[1] == 0) return;
	for (auto &track : points) {
		for (auto &p : track) {
			p[0] += shift[0];
			p[1] += shift[1];
		}
	}
}

/** A geometry: per track name, piece index to length. */
using Geometry = std::map<std::string, std::map<uint32_t, int>>;

/** X1 with flex: the variant, its track indices and headings, and the base geometry (the brief's section 5). */
struct X1Fixture {
	PlanarShapeVariant v;
	uint32_t n = 0;
	std::map<std::string, uint32_t> idx;
	std::vector<std::string> names; ///< Per track, its key: the line's name, made unique by "#<index>" for a copy or part.
	std::map<std::string, std::vector<int>> heads;
	std::map<std::string, Point> starts;
	Geometry base;
	PlanarDrawOptions options; ///< The section 5 options.

	X1Fixture() : X1Fixture(LoadX1Enc())
	{
		REQUIRE(n == 6);
		SetUp({
			{"AC1", {2, 1, 2, 3, 2}},
			{"A1", {2, 1, 2, 3, 2, 1, 2, 3, 2}},
			{"C1", {1, 0, 1, 2, 1, 0, -1, 0, 1, 0}},
			{"BD1", {0, -1, 0, 1, 0}},
			{"B1", {0, -1, 0, 1, 0, -1, 0, 1, 0}},
			{"D1", {1, 0, 1, 2, 1, 2, 1, 2, 3, 2}},
		}, {
			{"AC1", {{0, 4}}},
			{"A1", {{4, 12}, {8, 4}}},
			{"C1", {{0, 3}, {9, 2}}},
			{"BD1", {{0, 2}}},
			{"B1", {{4, 6}, {8, 4}}},
			{"D1", {{0, 1}, {9, 8}}},
		});
	}

	/** A fixture on a loaded variant; its headings and base are set by SetUp. */
	explicit X1Fixture(PlanarShapeVariant variant) : v(std::move(variant))
	{
		n = static_cast<uint32_t>(v.structure.map.expanded.lines.size());
		for (uint32_t t = 0; t < n; t++) {
			/* A copy or a part shares its line's name: later ones get "#<track index>" so every track has its own key. */
			std::string name = v.structure.map.expanded.lines[t].name;
			if (idx.count(name) != 0) name += "#" + std::format("{}", t);
			names.push_back(name);
			idx[name] = t;
		}
		starts = {{"AC1", {0, 5}}, {"A1", {4, 5}}, {"C1", {4, 5}}, {"BD1", {11, 12}}, {"B1", {11, 10}}, {"D1", {11, 10}}};
		options.clearance = options.clear_rows = options.pairwise = options.pairwise_all_faces = true;
		options.strict_span = options.strict_zero = options.crossing_flanks = true;
		options.node_gaps = true;
		options.bays = options.curves = false;
	}

	/** The headings (checked against the variant's pieces) and the base geometry. */
	void SetUp(const std::map<std::string, std::vector<int>> &h, const Geometry &g)
	{
		heads = h;
		for (const auto &[name, hs] : heads) {
			REQUIRE(idx.count(name) == 1);
			REQUIRE(v.tracks[idx[name]].pieces == hs);
		}
		base = g;
	}

	/**
	 * The start of every track under a geometry, the geometry connected at its nodes: a track that starts at a node starts at the node's point plus the base's offset there (0 for an ordinary track, the opening step for a beside track); an in-port track keeps its base start.
	 * A node's point is the end of its single track on the other side (a split's trunk; a merge's trunk start: the end of its first non-beside leg in the node's `in` order).
	 * @param connected Set to false when a merge leg does not end at the node point plus its base offset (the geometry is disconnected).
	 */
	std::map<std::string, Point> StartsOf(const Geometry &g, bool *connected = nullptr) const
	{
		const auto &map = v.structure.map;
		const auto end_from = [&](uint32_t t, const Geometry &geometry, Point cur) {
			const std::string &name = Name(t);
			const auto it = geometry.find(name);
			const std::vector<int> &h = heads.at(name);
			if (it != geometry.end()) {
				for (const auto &[k, len] : it->second) {
					const int hh = ((h[k] % 8) + 8) % 8;
					cur[0] += len * STEPS[hh][0];
					cur[1] += len * STEPS[hh][1];
				}
			}
			return cur;
		};
		const uint32_t nodes = static_cast<uint32_t>(map.expanded.nodes.size());
		/* The defining track of a node (none when a side is empty) and the base node point. */
		std::vector<std::optional<uint32_t>> defining(nodes);
		std::vector<Point> base_point(nodes);
		for (uint32_t m = 0; m < nodes; m++) {
			std::vector<uint32_t> ending, starting;
			for (uint32_t t = 0; t < n; t++) {
				const PlanarLine &line = map.expanded.lines[t];
				if (!line.to.port && line.to.index == m) ending.push_back(t);
				if (!line.from.port && line.from.index == m) starting.push_back(t);
			}
			if (ending.empty() || starting.empty()) continue;
			uint32_t d = ending[0];
			if (ending.size() > 1) {
				for (const uint32_t t : map.expanded.nodes[m].in) {
					if (map.part_of[t] != UINT32_MAX) { d = t; break; }
				}
			}
			defining[m] = d;
			base_point[m] = end_from(d, base, starts.at(Name(d)));
		}
		std::map<std::string, Point> result;
		std::vector<bool> known(n, false), resolved(nodes, false);
		std::vector<Point> point(nodes);
		for (uint32_t t = 0; t < n; t++) {
			const PlanarLine &line = map.expanded.lines[t];
			if (line.from.port || !defining[line.from.index]) {
				result[Name(t)] = starts.at(Name(t));
				known[t] = true;
			}
		}
		for (bool progress = true; progress;) {
			progress = false;
			for (uint32_t m = 0; m < nodes; m++) {
				if (resolved[m] || !defining[m] || !known[*defining[m]]) continue;
				resolved[m] = true;
				progress = true;
				point[m] = end_from(*defining[m], g, result.at(Name(*defining[m])));
				for (uint32_t t = 0; t < n; t++) {
					const PlanarLine &line = map.expanded.lines[t];
					if (line.from.port || line.from.index != m) continue;
					const Point &base_start = starts.at(Name(t));
					result[Name(t)] = {point[m][0] + base_start[0] - base_point[m][0], point[m][1] + base_start[1] - base_point[m][1]};
					known[t] = true;
				}
			}
		}
		REQUIRE(std::ranges::all_of(known, [](bool k) { return k; })); // A cycle of nodes: no track defines its start.
		if (connected != nullptr) {
			*connected = true;
			for (uint32_t t = 0; t < n; t++) {
				const PlanarLine &line = map.expanded.lines[t];
				if (line.to.port || !defining[line.to.index] || *defining[line.to.index] == t) continue;
				const uint32_t m = line.to.index;
				const Point base_end = end_from(t, base, starts.at(Name(t)));
				const Point end = end_from(t, g, result.at(Name(t)));
				if (end[0] != point[m][0] + base_end[0] - base_point[m][0] || end[1] != point[m][1] + base_end[1] - base_point[m][1]) *connected = false;
			}
		}
		return result;
	}

	/** The points of a geometry (unlisted pieces have length 0). */
	std::vector<Points> PointsOf(const Geometry &g) const
	{
		std::vector<Points> points(n);
		const std::map<std::string, Point> start_of = StartsOf(g);
		for (const auto &[name, h] : heads) {
			Points &p = points[idx.at(name)];
			Point cur = start_of.at(name);
			p.push_back(cur);
			const auto it = g.find(name);
			for (uint32_t k = 0; k < h.size(); k++) {
				int len = 0;
				if (it != g.end()) {
					const auto l = it->second.find(k);
					if (l != it->second.end()) len = l->second;
				}
				const int hh = ((h[k] % 8) + 8) % 8;
				cur[0] += len * STEPS[hh][0];
				cur[1] += len * STEPS[hh][1];
				p.push_back(cur);
			}
		}
		NormalisePoints(points);
		return points;
	}

	/** The read-out of the keep-apart pairs, built once and not solved. */
	PlanarDrawResult ReadOut(bool strict) const
	{
		PlanarDrawOptions o = options;
		if (!strict) o.strict_span = o.strict_zero = o.crossing_flanks = false;
		o.keep_apart_out = true;
		o.build_only = true;
		return CheckPlanarDrawable(v, 5.0, o);
	}

	/** The read-out of the whole program, built once and not solved (the strict rules off, no node lead). */
	PlanarDrawResult ProgramOut() const
	{
		PlanarDrawOptions o = options;
		o.strict_span = o.strict_zero = o.crossing_flanks = false;
		o.keep_apart_out = true;
		o.program_out = true;
		o.build_only = true;
		return CheckPlanarDrawable(v, 5.0, o);
	}

	const std::string &Name(uint32_t t) const { return names[t]; }
};

/** X2's loaded variant, the B1 shape text that loaded, and the log of what was refused. */
struct X2Load {
	PlanarShapeVariant v;
	std::string shape; ///< B1's shape text that loaded.
	std::string log; ///< The refusals before it.
};

/** X2: B1 crosses A1's tunnel on a 45-degree piece, `0 1 0`, or its mirror `0 -1 0` when that is refused (D414). */
X2Load LoadX2Enc()
{
	X2Load out;
	for (const std::string shape : {"0 1 0", "0 -1 0"}) {
		PlanarShapeVariant v;
		std::string why;
		std::vector<std::string> promise;
		if (!LoadEncVariant(shape, "A1@1", v, why, &promise, "3 7")) {
			out.log += "shape B1: " + shape + " refused: " + why + "\n";
			continue;
		}
		const bool crossing = std::ranges::any_of(promise, [](const std::string &m) { return m.find("cross") != std::string::npos; });
		if (crossing) {
			for (const std::string &m : promise) out.log += "shape B1: " + shape + " promise: " + m + "\n";
			continue;
		}
		out.v = std::move(v);
		out.shape = shape;
		return out;
	}
	INFO(out.log);
	FAIL("neither B1 shape loaded");
	return out;
}

/** X2 with flex: B1 bends at its crossing with A1's tunnel (the brief's section 3). */
struct X2Fixture : X1Fixture {
	std::string shape; ///< B1's shape text that loaded.
	std::string log; ///< What was refused first.
	uint32_t b1_event = 0; ///< B1's event piece after flex.
	uint32_t a1_event = 0; ///< A1's event piece after flex.

	X2Fixture() : X2Fixture(LoadX2Enc()) {}

private:
	explicit X2Fixture(X2Load l) : X1Fixture(std::move(l.v)), shape(l.shape), log(l.log)
	{
		std::string why;
		REQUIRE(PlacePlanarShapeFlex(v, why));
		std::map<std::string, std::vector<int>> h;
		for (const auto &[name, t] : idx) h[name] = v.tracks[t].pieces;
		/* The other tracks are X1's. */
		REQUIRE(h["AC1"] == std::vector<int>({2, 1, 2, 3, 2}));
		REQUIRE(h["A1"] == std::vector<int>({2, 1, 2, 3, 2, 1, 2, 3, 2}));
		REQUIRE(h["C1"] == std::vector<int>({1, 0, 1, 2, 1, 0, -1, 0, 1, 0}));
		REQUIRE(h["BD1"] == std::vector<int>({0, -1, 0, 1, 0}));
		REQUIRE(h["D1"] == std::vector<int>({1, 0, 1, 2, 1, 2, 1, 2, 3, 2}));
		const auto &map = v.structure.map;
		auto event_of = [&](const std::string &name) {
			const uint32_t t = idx.at(name);
			REQUIRE(map.crossings.size() == 1);
			const auto &ev = map.events[t];
			const size_t pos = static_cast<size_t>(std::ranges::find(ev, 0u) - ev.begin());
			REQUIRE(pos < ev.size());
			return v.tracks[t].event_piece[pos];
		};
		b1_event = event_of("B1");
		a1_event = event_of("A1");
		/* B1: N 2, the crossing run (NE or NW) 4 on the event piece, N 4; each run on the first piece of its heading at or after the previous run's piece. */
		const std::vector<int> &b = h["B1"];
		const int diag = shape == "0 1 0" ? 1 : 7;
		const std::pair<int, int> runs[3] = {{0, 2}, {diag, 4}, {0, 4}};
		Geometry g = {
			{"AC1", {{0, 4}}},
			{"A1", {{4, 12}, {8, 4}}},
			{"C1", {{0, 3}, {9, 2}}},
			{"BD1", {{0, 2}}},
			{"D1", {{0, 1}, {9, 8}}},
		};
		uint32_t prev = 0;
		for (int r = 0; r < 3; r++) {
			uint32_t k = prev;
			if (r == 1) {
				k = b1_event;
			} else {
				while (k < b.size() && ((b[k] % 8) + 8) % 8 != runs[r].first) k++;
				if (r == 2) {
					for (uint32_t j = k; j < b.size(); j++) {
						if (((b[j] % 8) + 8) % 8 == runs[r].first) k = j;
					}
				}
			}
			REQUIRE(k < b.size());
			REQUIRE(((b[k] % 8) + 8) % 8 == runs[r].first);
			REQUIRE(k >= prev);
			if (r == 0) REQUIRE(k <= b1_event);
			g["B1"][k] = runs[r].second;
			prev = k;
		}
		SetUp(h, g);
	}
};

/** A variant from topology, structure and shape texts (no crossings), flex placed. */
PlanarShapeVariant LoadTextEnc(const std::string &topo, const std::string &structure, const std::string &shape)
{
	std::string error;
	const PlanarTopology t = ParsePlanarTopology(topo, &error);
	INFO(error);
	REQUIRE(error.empty());
	const JunctionSpec spec = MakeTestJunctionSpec();
	std::vector<std::string> report;
	const PlanarStructureVariant s = LoadPlanarStructureVariant(t, structure, spec, report, &error);
	INFO(error);
	REQUIRE(error.empty());
	PlanarShapeVariant v;
	std::string why;
	const bool loaded = LoadPlanarShapeVariant(t, s, shape, v, why);
	INFO(why);
	REQUIRE(loaded);
	REQUIRE(PlacePlanarShapeFlex(v, why));
	return v;
}

/** One track's base lengths: each run (heading, length) on the first piece of its heading at or after the previous run's piece, the last run on the last piece of its heading; the pieces used are appended to holders. */
std::map<uint32_t, int> PlaceRuns(const std::vector<int> &b, const std::vector<std::pair<int, int>> &runs, std::vector<uint32_t> &holders)
{
	std::map<uint32_t, int> out;
	uint32_t prev = 0;
	for (size_t r = 0; r < runs.size(); r++) {
		uint32_t k = prev;
		while (k < b.size() && ((b[k] % 8) + 8) % 8 != runs[r].first) k++;
		if (r + 1 == runs.size()) {
			for (uint32_t j = k; j < b.size(); j++) {
				if (((b[j] % 8) + 8) % 8 == runs[r].first) k = j;
			}
		}
		REQUIRE(k < b.size());
		out[k] = runs[r].second;
		holders.push_back(k);
		prev = k;
	}
	return out;
}

/** M1: a merge whose legs come from splits (domain-15). */
struct M1Fixture : X1Fixture {
	std::map<std::string, std::vector<uint32_t>> holders; ///< Per track, the pieces that hold its base runs.

	M1Fixture() : M1Fixture(LoadTextEnc(
		"topology M1\nport W in E: AC1\nport S in N: BD1\nport N out N: C1\nport E out E @ 2 5: AB1, D1\nnode AC1 -> C1, A1\nnode BD1 -> B1, D1\nnode A1, B1 -> AB1\n",
		"straight AC1: A1\nstraight BD1: B1\nstraight AB1: A1\n",
		"shape AC1: 2\nshape C1: 1 0\nshape A1: 2\nshape BD1: 0\nshape B1: 0 1\nshape D1: 1 2\nshape AB1: 2\n")) {}

	explicit M1Fixture(PlanarShapeVariant variant) : X1Fixture(std::move(variant))
	{
		REQUIRE(n == 7);
		starts = {{"AC1", {0, 5}}, {"C1", {4, 5}}, {"A1", {4, 5}}, {"BD1", {13, 14}}, {"B1", {13, 12}}, {"D1", {13, 12}}, {"AB1", {16, 5}}};
		const std::map<std::string, std::vector<std::pair<int, int>>> runs = {
			{"AC1", {{2, 4}}}, {"C1", {{1, 3}, {0, 2}}}, {"A1", {{2, 12}}}, {"BD1", {{0, 2}}}, {"B1", {{0, 4}, {1, 3}}}, {"D1", {{1, 1}, {2, 10}}}, {"AB1", {{2, 8}}},
		};
		std::map<std::string, std::vector<int>> h;
		Geometry g;
		for (const auto &[name, t] : idx) {
			h[name] = v.tracks[t].pieces;
			g[name] = PlaceRuns(h[name], runs.at(name), holders[name]);
		}
		SetUp(h, g);
	}
};

/** K1: a cascade and a trident (domain-15). */
struct K1Fixture : X1Fixture {
	std::map<std::string, std::vector<uint32_t>> holders; ///< Per track, the pieces that hold its base runs.

	K1Fixture() : K1Fixture(LoadTextEnc(
		"topology K1\nport W in E: ABC1\nport N out N: A1\nport E out E: B1\nport S out S: C1\nnode ABC1 -> A1, BC1\nnode BC1 -> B1, C1\n",
		"straight ABC1: BC1\nstraight BC1: B1\n",
		"shape ABC1: 2\nshape A1: 1 0\nshape BC1: 2\nshape B1: 2\nshape C1: 3 4\n")) {}

	explicit K1Fixture(PlanarShapeVariant variant) : X1Fixture(std::move(variant))
	{
		REQUIRE(n == 5);
		starts = {{"ABC1", {0, 9}}, {"A1", {4, 9}}, {"BC1", {4, 9}}, {"B1", {8, 9}}, {"C1", {8, 9}}};
		const std::map<std::string, std::vector<std::pair<int, int>>> runs = {
			{"ABC1", {{2, 4}}}, {"A1", {{1, 3}, {0, 6}}}, {"BC1", {{2, 4}}}, {"B1", {{2, 12}}}, {"C1", {{3, 3}, {4, 6}}},
		};
		std::map<std::string, std::vector<int>> h;
		Geometry g;
		for (const auto &[name, t] : idx) {
			h[name] = v.tracks[t].pieces;
			g[name] = PlaceRuns(h[name], runs.at(name), holders[name]);
		}
		SetUp(h, g);
	}
};

/** A fixture on Shape's own heading solve (D428 batch 2a): the variant is `ShapePlanarStructureVariant`'s, the starts and base geometry are one drawing by the program on the grid (the rule set span+lead+endnode+crosspiece+grid plus the corridor family, tidy, integer). */
struct DrawnFixture : X1Fixture {
	std::string shape_log; ///< Shape's headings per track, one line each.
	std::string draw_log; ///< The base drawing's status and size.
	PlanarDrawResult drawing; ///< The base drawing.
	std::string draw_why; ///< Why the base drawing is not legal, empty when it is.

	DrawnFixture(const std::string &topo, const std::string &structure, int least_extra = 0) : DrawnFixture(Shaped(topo, structure), least_extra) {}

private:
	static PlanarShapeVariant Shaped(const std::string &topo, const std::string &structure)
	{
		std::string error;
		const PlanarTopology t = ParsePlanarTopology(topo, &error);
		INFO(error);
		REQUIRE(error.empty());
		const JunctionSpec spec = MakeTestJunctionSpec();
		std::vector<std::string> report;
		const PlanarStructureVariant s = LoadPlanarStructureVariant(t, structure, spec, report, &error);
		INFO(error);
		REQUIRE(error.empty());
		PlanarShapeVariant v;
		std::string why;
		const bool shaped = ShapePlanarStructureVariant(s, v, why);
		INFO("Shape refused: " << why);
		REQUIRE(shaped);
		return v;
	}

	DrawnFixture(PlanarShapeVariant variant, int least_extra) : X1Fixture(std::move(variant))
	{
		options.corridor = true;
		options.end_node = options.cross_piece = options.grid = true;
		options.strict_zero = options.crossing_flanks = false;
		options.node_lead = true;
		options.tidy = true;
		options.elastic = false;
		std::map<std::string, std::vector<int>> h;
		std::ostringstream shape;
		for (uint32_t t = 0; t < n; t++) {
			h[Name(t)] = v.tracks[t].pieces;
			shape << "  " << Name(t) << ":";
			for (const int x : v.tracks[t].pieces) shape << " " << x;
			shape << "; flex middles:";
			for (const uint32_t m : v.tracks[t].flex) shape << " " << m;
			shape << "\n";
		}
		shape_log = shape.str();
		/* The base is drawn on the flex-stripped variant (every flex pair erased with its trailing flank, as the pipeline's final_contract does with nothing kept), so every piece has a least length and the grid seeds every axis run. */
		PlanarShapeVariant stripped = v;
		std::vector<std::vector<uint32_t>> kept_of(n); ///< Per track, the shaped index of each stripped piece's first piece.
		std::vector<std::vector<uint32_t>> stripped_of(n); ///< Per track, the stripped piece each shaped piece belongs to.
		for (uint32_t t = 0; t < n; t++) {
			PlanarShapeTrack &track = stripped.tracks[t];
			std::vector<uint32_t> middles = track.flex;
			std::ranges::sort(middles);
			std::vector<bool> erased(track.pieces.size(), false);
			for (const uint32_t m : middles) erased[m] = erased[m + 1] = true;
			for (uint32_t i = 0; i < erased.size(); i++) {
				if (!erased[i]) kept_of[t].push_back(i);
				stripped_of[t].push_back(static_cast<uint32_t>(kept_of[t].size()) - 1);
			}
			for (size_t j = middles.size(); j-- > 0;) {
				const uint32_t m = middles[j];
				track.pieces.erase(track.pieces.begin() + m, track.pieces.begin() + m + 2);
				for (uint32_t &e : track.event_piece) {
					if (e > m) e -= 2;
				}
			}
			track.flex.clear();
			track.steps.clear();
		}
		std::string refine_why;
		REQUIRE(RefinePlanarShapeCells(stripped, refine_why));
		PlanarDrawOptions base_options = options;
		base_options.least_extra = least_extra;
		drawing = CheckPlanarDrawable(stripped, 15.0, base_options);
		std::ostringstream log;
		log << "status " << drawing.status << ", feasible " << drawing.feasible << ", rows " << drawing.rows << ", columns " << drawing.columns << ", " << drawing.ms << " ms";
		if (drawing.status != "Optimal") {
			log << "; conflicts:";
			for (const std::string &c : drawing.conflict) log << " [" << c << "]";
		}
		draw_log = log.str();
		INFO("base drawing (flex-stripped variant): " << draw_log);
		INFO("Shape's headings:\n" << shape_log);
		REQUIRE(drawing.status == "Optimal");
		REQUIRE(drawing.points.size() == n);
		/* The base onto the shaped variant: a stripped piece's length goes to its first piece in the shaped list, or to the piece its crossing sits on; the inserts stay 0. */
		Geometry g;
		for (uint32_t t = 0; t < n; t++) {
			const auto &p = drawing.points[t];
			REQUIRE(p.size() == kept_of[t].size() + 1);
			starts[Name(t)] = p[0];
			/* Per stripped piece, its event pieces (ascending) with the distance along the drawn run at which the crossing's other track meets it. */
			std::map<uint32_t, std::map<uint32_t, int>> at_event;
			const auto &events = v.structure.map.events[t];
			for (size_t pos = 0; pos < events.size(); pos++) {
				const uint32_t e = v.tracks[t].event_piece[pos];
				const uint32_t k = stripped_of[t][e];
				const auto &cr = v.structure.map.crossings[events[pos]];
				const uint32_t o = cr.a == t ? cr.b : cr.a;
				const auto &oe = v.structure.map.events[o];
				const size_t opos = static_cast<size_t>(std::ranges::find(oe, events[pos]) - oe.begin());
				REQUIRE(opos < oe.size());
				const uint32_t ok = stripped_of[o][v.tracks[o].event_piece[opos]];
				const Point &p0 = drawing.points[t][k], &p1 = drawing.points[t][k + 1], &r0 = drawing.points[o][ok], &r1 = drawing.points[o][ok + 1];
				const int len = std::max(std::abs(p1[0] - p0[0]), std::abs(p1[1] - p0[1]));
				const Point dp = {p1[0] - p0[0], p1[1] - p0[1]}, dr = {r1[0] - r0[0], r1[1] - r0[1]};
				const int den = dp[0] * dr[1] - dp[1] * dr[0];
				int dist = 0;
				if (den != 0) {
					int num = (r0[0] - p0[0]) * dr[1] - (r0[1] - p0[1]) * dr[0];
					int d = den;
					if (d < 0) { d = -d; num = -num; }
					dist = std::clamp(len * num / d, 0, len);
				}
				auto &m = at_event[k];
				auto it = m.emplace(e, dist);
				if (!it.second) it.first->second = std::min(it.first->second, dist);
			}
			for (size_t k = 0; k + 1 < p.size(); k++) {
				const int len = std::max(std::abs(p[k + 1][0] - p[k][0]), std::abs(p[k + 1][1] - p[k][1]));
				if (len == 0) continue;
				const auto ev = at_event.find(static_cast<uint32_t>(k));
				if (ev == at_event.end()) {
					g[Name(t)][kept_of[t][k]] = len;
					continue;
				}
				/* Several event pieces on one drawn run (two crossings separated by a flex group): the run is cut midway between consecutive crossings, each event piece takes its share and the pieces between stay 0. */
				std::vector<std::pair<uint32_t, int>> at(ev->second.begin(), ev->second.end());
				int from = 0;
				for (size_t j = 0; j < at.size(); j++) {
					if (j > 0) REQUIRE(at[j - 1].second <= at[j].second);
					const int to = j + 1 < at.size() ? (at[j].second + at[j + 1].second) / 2 : len;
					if (to > from) g[Name(t)][at[j].first] = to - from;
					from = std::max(from, to);
				}
			}
		}
		SetUp(h, g);
		/* The drawing's end points are the base polyline's. */
		const std::vector<Points> base = PointsOf(g);
		for (uint32_t t = 0; t < n; t++) REQUIRE(base[t].back() == drawing.points[t].back());
	}
};

/** The least over a candidate's rows of s.(q - p) - spacing on the points. */
int WorstSlack(const PlanarKeepApartPair::Candidate &c, const std::vector<Points> &points)
{
	int worst = INT_MAX;
	for (const PlanarKeepApartPair::Row &r : c.rows) {
		const Point p = PlanarKeepApartEnd(r, false, points);
		const Point q = PlanarKeepApartEnd(r, true, points);
		worst = std::min(worst, c.s[0] * (q[0] - p[0]) + c.s[1] * (q[1] - p[1]) - c.spacing);
	}
	return worst;
}

/** The pairs that fail on the points, grouped as the report's strict key wants. */
std::string FailingText(const X1Fixture &f, const std::vector<PlanarKeepApartPair> &pairs, const std::vector<Points> &points, std::set<std::string> &kept_set)
{
	std::ostringstream out;
	for (const PlanarKeepApartPair &p : pairs) {
		if (PlanarKeepApartHolds(p, points)) continue;
		kept_set.insert(p.kept.empty() ? "plain" : p.kept);
		out << "      " << p.tag << " tracks " << f.Name(p.tracks[0]) << "/" << f.Name(p.tracks[1]) << " pieces " << p.pieces[0] << "/" << p.pieces[1] << " kept [" << (p.kept.empty() ? "plain" : p.kept) << "] candidates:";
		for (const PlanarKeepApartPair::Candidate &c : p.candidates) out << " s(" << c.s[0] << "," << c.s[1] << ")+" << c.spacing << " worst slack " << WorstSlack(c, points) << ";";
		out << "\n";
	}
	return out.str();
}

std::string Join(const std::set<std::string> &s)
{
	std::string out;
	for (const std::string &x : s) out += (out.empty() ? "" : " | ") + x;
	return out;
}

/** A polyline on one track: runs of (heading mod 8, length), zero pieces dropped, equal neighbouring headings merged. */
using Runs = std::vector<std::pair<int, int>>;
using Lengths = std::vector<int>;

Runs PolylineRuns(const std::vector<int> &heads, const Lengths &lengths)
{
	Runs runs;
	for (size_t k = 0; k < heads.size(); k++) {
		if (lengths[k] == 0) continue;
		const int h = ((heads[k] % 8) + 8) % 8;
		if (!runs.empty() && runs.back().first == h) {
			runs.back().second += lengths[k];
		} else {
			runs.emplace_back(h, lengths[k]);
		}
	}
	return runs;
}

/** Every piece-length assignment on the pieces whose polyline is exactly runs (at most cap; capped says it stopped there). With flex (the flex middles), the search is flank-first: a run takes the earliest regular piece it can, flex middles only when the order needs them (D432). */
std::vector<Lengths> Representations(const std::vector<int> &pieces, const Runs &runs, size_t cap, bool *capped = nullptr, const std::vector<uint32_t> *flex = nullptr)
{
	std::vector<Lengths> out;
	if (capped != nullptr) *capped = false;
	Lengths cur(pieces.size(), 0);
	auto walk = [&](auto &&self, size_t k, size_t r, int left) -> void {
		if (out.size() >= cap) {
			if (capped != nullptr) *capped = true;
			return;
		}
		if (k == pieces.size()) {
			if (runs.empty() || (r == runs.size() - 1 && left == 0)) out.push_back(cur);
			return;
		}
		const int h = ((pieces[k] % 8) + 8) % 8;
		const int unit = h % 2 == 0 ? 2 : 1;
		const bool middle = flex == nullptr || std::ranges::find(*flex, static_cast<uint32_t>(k)) != flex->end();
		/* Stay in the current run (any amount from 0), or move on to the next run once this one is used up (at least one unit). */
		if (!runs.empty() && runs[r].first == h) {
			for (int i = 0; i <= left / unit; i++) {
				const int a = (middle ? i : left / unit - i) * unit;
				cur[k] = a;
				self(self, k + 1, r, left - a);
			}
			cur[k] = 0;
		} else {
			self(self, k + 1, r, left);
		}
		if (!runs.empty() && left == 0 && r + 1 < runs.size() && runs[r + 1].first == h) {
			const int top = runs[r + 1].second / unit;
			for (int i = 1; i <= top; i++) {
				const int a = (middle ? i : top + 1 - i) * unit;
				cur[k] = a;
				self(self, k + 1, r + 1, runs[r + 1].second - a);
			}
			cur[k] = 0;
		}
	};
	if (runs.empty()) {
		out.push_back(cur);
	} else {
		walk(walk, 0, 0, runs[0].second);
	}
	return out;
}

struct Example {
	std::string varied;
	std::string detail;
};

struct Group {
	uint32_t count = 0; ///< Polylines.
	uint32_t geometries = 0;
	std::vector<Example> examples;
};

struct Tally {
	std::array<uint32_t, 8> counts{}; ///< Polylines: both accept, both reject, too strict, too loose; then off grid geometries, then other shape, then off interface, then loose off interface (the domain refuses, the full rows refuse, the rows without the interface accept).
	std::array<uint32_t, 8> geoms{}; ///< The enumerated geometries behind each.
	std::set<std::string> seen; ///< The polylines counted.
	std::map<std::string, Group> strict;
	std::map<std::string, Group> loose;
	std::map<std::string, Group> loose_off; ///< Loose off interface (report only), by domain break rules.
	std::vector<Example> other; ///< The first other shape polylines.
	std::vector<Example> off_interface; ///< The first off interface polylines (the domain accepts them, the full rows reject them, and the read-out without the interface rows, the ports and the box, accepts them); the examples list the full rows' failing rows.
};

uint32_t g_corridor_rows_seen = 0; ///< The corridor rows over the on-grid contracted variants judged in the last report.
uint32_t g_corridor_rows_base = 0; ///< The corridor rows in the base polyline's read-out.
uint32_t g_lead_rows_seen = 0; ///< The node lead rows over the contracted variants judged in the last report.
std::map<std::string, uint32_t> g_lead_by_family; ///< The same, per configuration family (the name up to its first space).

std::string FamilyOf(const std::string &tag)
{
	return tag.substr(0, tag.find(' '));
}

const char *const CLASS_NAMES[8] = {"both accept", "both reject", "too strict", "too loose", "off grid (geometries)", "other shape", "off interface", "loose off interface"};

/** A polyline's class and its example text, from the first geometry that had it. */
struct PolyClass {
	int c = 0;
	std::string key;
	std::string detail;
};


/** A crossing's two assigned pieces: track a's piece pa and track b's piece pb meet (D428). */
struct Conformity {
	uint32_t a, b, pa, pb;
};

int Cross2(const Point &o, const Point &p, const Point &q)
{
	return (p[0] - o[0]) * (q[1] - o[1]) - (p[1] - o[1]) * (q[0] - o[0]);
}

/** Whether the closed segments p0p1 and q0q1 meet in exactly one point. */
bool SegmentsMeetInOnePoint(const Point &p0, const Point &p1, const Point &q0, const Point &q1)
{
	const bool pz = p0 == p1, qz = q0 == q1;
	auto on = [](const Point &x, const Point &a, const Point &b) {
		return Cross2(a, b, x) == 0 && std::min(a[0], b[0]) <= x[0] && x[0] <= std::max(a[0], b[0]) && std::min(a[1], b[1]) <= x[1] && x[1] <= std::max(a[1], b[1]);
	};
	if (pz && qz) return p0 == q0;
	if (pz) return on(p0, q0, q1);
	if (qz) return on(q0, p0, p1);
	const Point dp = {p1[0] - p0[0], p1[1] - p0[1]}, dq = {q1[0] - q0[0], q1[1] - q0[1]};
	const int d = dp[0] * dq[1] - dp[1] * dq[0];
	if (d != 0) {
		const int w[2] = {q0[0] - p0[0], q0[1] - p0[1]};
		int t = w[0] * dq[1] - w[1] * dq[0];
		int u = w[0] * dp[1] - w[1] * dp[0];
		int dd = d;
		if (dd < 0) {
			dd = -dd;
			t = -t;
			u = -u;
		}
		return t >= 0 && t <= dd && u >= 0 && u <= dd;
	}
	if (Cross2(p0, p1, q0) != 0) return false;
	/* Collinear: the intervals along p's direction share exactly one point. */
	auto at = [&](const Point &x) { return dp[0] * x[0] + dp[1] * x[1]; };
	const int pa = at(p0), pb = at(p1), qa = at(q0), qb = at(q1);
	const int lo = std::max(std::min(pa, pb), std::min(qa, qb)), hi = std::min(std::max(pa, pb), std::max(qa, qb));
	return lo == hi;
}

bool ConformityHolds(const Conformity &c, const std::vector<Points> &cur)
{
	const Points &a = cur[c.a], &b = cur[c.b];
	/* A crossing sits on a piece with length (cross_piece): a zero-length assigned piece crosses at its neighbours' angle, another shape. */
	if (a[c.pa] == a[c.pa + 1] || b[c.pb] == b[c.pb + 1]) return false;
	return SegmentsMeetInOnePoint(a[c.pa], a[c.pa + 1], b[c.pb], b[c.pb + 1]);
}

/** The map's crossings with their assigned pieces. */
std::vector<Conformity> ConformitiesOf(const X1Fixture &f)
{
	std::vector<Conformity> out;
	const auto &map = f.v.structure.map;
	for (uint32_t c = 0; c < map.crossings.size(); c++) {
		const uint32_t t[2] = {map.crossings[c].a, map.crossings[c].b};
		uint32_t piece[2];
		for (int i = 0; i < 2; i++) {
			const auto &ev = map.events[t[i]];
			const size_t pos = static_cast<size_t>(std::ranges::find(ev, c) - ev.begin());
			REQUIRE(pos < ev.size());
			piece[i] = f.v.tracks[t[i]].event_piece[pos];
		}
		out.push_back({t[0], t[1], piece[0], piece[1]});
	}
	return out;
}

/** REQUIRE a fixture's base on the grid, domain-legal and conformant. */
void RequireBaseLegal(const X1Fixture &f)
{
	const std::vector<Points> base = f.PointsOf(f.base);
	for (const Points &p : base) REQUIRE(RasterPlanarDomainTrack(p).rule.empty());
	std::vector<std::string> notes;
	const PlanarDomainVerdict d = CheckPlanarDomain(PlanarDomainInputOf(f.v, base, notes));
	for (const std::string &m : notes) WARN("base note: " << m);
	for (const PlanarDomainBreak &b : d.breaks) WARN("base break: " << b.rule << " tile (" << b.tile.x << "," << b.tile.y << ") " << b.why);
	REQUIRE(notes.empty());
	REQUIRE(d.legal);
	for (const Conformity &c : ConformitiesOf(f)) REQUIRE(ConformityHolds(c, base));
}

uint32_t g_turn_moved = 0; ///< Contracted writings that took a later representation because the first turned over one eighth.
uint32_t g_turn_rejected = 0; ///< Contracted writings with no representation within one eighth per turn (the rows reject them).
uint32_t g_node_turn_rejected = 0; ///< Polylines the rows reject only because a trunk and a leg turn over one eighth at a node (F6).

/** One polyline's contracted writing (D432): the variant with only the pieces that have length, and its points (the polyline's corners). */
struct Contracted {
	PlanarShapeVariant v;
	std::vector<Points> points;
};

/** A judged drawing: the behaviour the suite specifies for Slots (accept exactly what the game allows; name the rule that refuses). */
struct RowsVerdict {
	bool accept = false;
	std::set<std::string> rules; ///< When refused: the suite's rule names that refuse (see RULE_NAMES), each once.
};

/** What the suite judges: one implementation of Slots' rows on fixed points. `interface_rows` false: without the port and box rows (the off-interface counterfactual). */
using RowsJudge = std::function<RowsVerdict(const X1Fixture &f, const Contracted &cv, bool interface_rows)>;

/** The drawing program (CheckPlanarDrawable read-outs, keep-apart pairs, lead rows) as a RowsJudge, under a rule-set string as RunEncoding parses it (the empty string: the PLANAR_DOMAIN_RULES measurement variable, else "all"). */
RowsJudge DrawingProgramJudge(const std::string &rules);

/** The suite's rule names by the drawing program's tag family (the tag up to its first space; for `bound <g>` and `integer <g>` the two words); a family not here is reported as `other <family>`. The turn checks (`turn`, `node turn`) are the suite's own and have no tag. */
const std::map<std::string, std::string> RULE_NAMES = {
	{"sep", "keep apart"}, {"sepdir", "keep apart"}, {"sephard", "keep apart"},
	{"lead", "lead"}, {"along", "along"},
	{"end", "end run"},
	{"cross", "crossing piece"},
	{"grid", "grid"}, {"integer half", "grid"}, {"bound half", "grid"},
	{"port", "port"}, {"portzero", "port"},
	{"box", "box"}, {"bound box", "box"}, {"integer box", "box"},
	{"corridor", "corridor"},
	{"leg", "node gap"}, {"leg zero", "node gap"},
	{"deck", "deck"}, {"node", "node"}, {"order", "port order"}, {"span", "span"},
	{"piece", "length"}, {"flex", "length"}, {"bound axis", "length"}, {"bound diagonal", "length"},
};

/** The suite's rule name of a drawing program row tag. */
std::string RuleOf(const std::string &tag)
{
	size_t end = tag.find(' ');
	const std::string first = tag.substr(0, end);
	if ((first == "bound" || first == "integer") && end != std::string::npos) end = tag.find(' ', end + 1);
	const std::string family = tag.substr(0, end);
	const auto it = RULE_NAMES.find(family);
	return it == RULE_NAMES.end() ? "other " + family : it->second;
}

/**
 * F6 and F8: at every node, each leg's heading at the node and the trunk's differ by at most one eighth (the contracted turn rule, F3, through nodes; F6), and no two legs of the node share a heading (the node shape, D400; F8).
 * When why is not null, each failing node (the first rule it breaks) pushes one line instead of returning at once: "node N point (x,y) rule R trunk NAME arriving|starting hH|zero legs: NAME arriving|starting hH ..." (rule F8, F6 or beside; trunk zero: it took the zero-trunk recursion; the legs are those the rule names), and the function returns false if any line was pushed.
 */
bool NodeTurnsHold(const X1Fixture &f, const Contracted &cv, std::vector<std::string> *why = nullptr)
{
	const auto &map = f.v.structure.map;
	struct Site {
		std::vector<uint32_t> ending, starting;
		Point point{};
	};
	/* A track's end point at a node: its last point when arriving, else its first. */
	const auto end_point = [&](uint32_t t, bool arriving) { return arriving ? cv.points[t].back() : cv.points[t].front(); };
	/* A zero-length track (all its points equal) has no heading and is transparent. */
	const auto zero = [&](uint32_t t) { return std::ranges::all_of(cv.points[t], [&](const Point &p) { return p == cv.points[t].front(); }); };
	const auto site_of = [&](uint32_t n) {
		Site site;
		for (uint32_t t = 0; t < f.n; t++) {
			const PlanarLine &line = map.expanded.lines[t];
			if (!line.to.port && line.to.index == n) site.ending.push_back(t);
			if (!line.from.port && line.from.index == n) site.starting.push_back(t);
		}
		REQUIRE((site.ending.size() <= 1 || site.starting.size() <= 1));
		if (site.ending.empty() || site.starting.empty()) return site;
		/* The node point is the end point most tracks share (the trunk's on a tie). */
		const bool trunk_ends = site.ending.size() == 1 && site.starting.size() > 1;
		site.point = end_point(trunk_ends ? site.ending[0] : site.starting[0], trunk_ends);
		size_t best = 0;
		const auto count = [&](const Point &p) {
			size_t c = 0;
			for (const uint32_t t : site.ending) c += end_point(t, true) == p ? 1 : 0;
			for (const uint32_t t : site.starting) c += end_point(t, false) == p ? 1 : 0;
			return c;
		};
		for (const uint32_t t : site.ending) {
			if (count(end_point(t, true)) > best) { best = count(end_point(t, true)); site.point = end_point(t, true); }
		}
		for (const uint32_t t : site.starting) {
			if (count(end_point(t, false)) > best) { best = count(end_point(t, false)); site.point = end_point(t, false); }
		}
		return site;
	};
	/* A track's heading at the node n: its end piece's, or for a beside track (its end point not the node point) the heading of the 2-step offset. */
	const auto heading = [&](uint32_t n, uint32_t t, bool arriving) {
		const Site site = site_of(n);
		const Point p = end_point(t, arriving);
		if (p == site.point) {
			const std::vector<int> &pieces = cv.v.tracks[t].pieces;
			return ((arriving ? pieces.back() : pieces.front()) % 8 + 8) % 8;
		}
		for (int k = 0; k < 8; k++) {
			const Point d = arriving ? Point{site.point[0] - p[0], site.point[1] - p[1]} : Point{p[0] - site.point[0], p[1] - site.point[1]};
			if (d[0] == 2 * STEPS[k][0] && d[1] == 2 * STEPS[k][1]) return k;
		}
		return -1; // Beside the node point at no 2 * STEP offset: refused below.
	};
	/* The heading of a trunk at node n; a zero-length trunk takes the heading of the trunk of the node at its other end (split: the track arriving there; merge: the track departing it), recursively; none when that end is a port. */
	const std::function<std::optional<int>(uint32_t, uint32_t, bool, size_t)> trunk_heading = [&](uint32_t n, uint32_t t, bool arriving, size_t depth) -> std::optional<int> {
		if (!zero(t)) return heading(n, t, arriving);
		const PlanarEnd &other = arriving ? map.expanded.lines[t].from : map.expanded.lines[t].to;
		if (other.port || depth > map.expanded.nodes.size()) return std::nullopt;
		const Site m = site_of(other.index);
		if (m.ending.size() == 1 && m.starting.size() > 1 && m.ending[0] != t) return trunk_heading(other.index, m.ending[0], true, depth + 1);
		if (m.starting.size() == 1 && m.ending.size() > 1 && m.starting[0] != t) return trunk_heading(other.index, m.starting[0], false, depth + 1);
		return std::nullopt;
	};
	const auto track_name = [&](uint32_t t) { return map.tracks[t].name; };
	bool all_hold = true;
	for (uint32_t n = 0; n < map.expanded.nodes.size(); n++) {
		const Site site = site_of(n);
		if (site.ending.empty() || site.starting.empty()) continue;
		const bool trunk_ends = site.ending.size() == 1 && site.starting.size() > 1;
		const uint32_t trunk = trunk_ends ? site.ending[0] : site.starting[0];
		const auto leg_text = [&](uint32_t t, bool arriving) { return std::format("{} {} h{}", track_name(t), arriving ? "arriving" : "starting", heading(n, t, arriving)); };
		const auto line_of = [&](const char *rule, const std::string &legs, std::optional<int> trunk_h) {
			const std::string trunk_text = zero(trunk) ? std::format("{} {} zero", track_name(trunk), trunk_ends ? "arriving" : "starting") : leg_text(trunk, trunk_ends);
			return std::format("node {} point ({},{}) rule {} trunk {}{} legs: {}", n, site.point[0], site.point[1], rule, trunk_text, zero(trunk) ? std::format(" (recursion heading {})", trunk_h ? std::format("{}", *trunk_h) : "none") : "", legs);
		};
		/* The first rule this node breaks, as its line; empty when it holds. */
		const auto check = [&]() -> std::string {
			/* F8: the legs' headings at the node are pairwise distinct (a zero leg is skipped). */
			std::map<int, std::string> leg_headings;
			for (const bool arriving : {true, false}) {
				for (const uint32_t t : arriving ? site.ending : site.starting) {
					if (t == trunk || zero(t)) continue;
					const int h = heading(n, t, arriving);
					if (h < 0) continue;
					const auto [it, fresh] = leg_headings.emplace(h, leg_text(t, arriving));
					if (!fresh) return line_of("F8", it->second + " ; " + leg_text(t, arriving), std::nullopt);
				}
			}
			const std::optional<int> trunk_h = trunk_heading(n, trunk, trunk_ends, 0);
			if (!trunk_h) return "";
			if (*trunk_h < 0) return line_of("beside", leg_text(trunk, trunk_ends), trunk_h);
			for (const bool arriving : {true, false}) {
				for (const uint32_t t : arriving ? site.ending : site.starting) {
					if (t == trunk || zero(t)) continue;
					const int h = heading(n, t, arriving);
					if (h < 0) return line_of("beside", leg_text(t, arriving), trunk_h);
					const int d = ((*trunk_h - h) % 8 + 8) % 8;
					if (std::min(d, 8 - d) > 1) return line_of("F6", leg_text(t, arriving), trunk_h);
				}
			}
			return "";
		};
		const std::string line = check();
		if (line.empty()) continue;
		if (why == nullptr) return false;
		why->push_back(line);
		all_hold = false;
	}
	return all_hold;
}

/**
 * The contracted variant of a polyline (D432): per track one representation of its runs on the variant's pieces, the pieces with length kept in order and equal consecutive headings merged, flex empty; steps are the contracted pieces that came from a flex middle (S-bend inserts).
 * Each event piece is remapped to the contracted piece holding the crossing with Shape's heading (mod 8).
 * @return The variant and its points; none when the runs have no representation or a crossing has no such piece (other shape).
 */
std::optional<Contracted> ContractedVariant(const X1Fixture &f, const Geometry &g)
{
	Contracted out;
	out.v = f.v;
	out.points.assign(f.n, {});
	const std::map<std::string, Point> start_of = f.StartsOf(g);
	for (const auto &[name, h] : f.heads) {
		const uint32_t t = f.idx.at(name);
		Lengths lens(h.size(), 0);
		const auto it = g.find(name);
		if (it != g.end()) {
			for (const auto &[k, len] : it->second) lens[k] = len;
		}
		/* Slots keeps the flank between two used S-bend inserts: consecutive contracted pieces are at most one eighth apart (lifted headings). Flank-first: the next representation (cap 50) before giving up; when none holds, the first stays and the rows reject it (its turn is over one eighth). */
		const std::vector<Lengths> reps = Representations(h, PolylineRuns(h, lens), 50, nullptr, &f.v.tracks[t].flex);
		if (reps.empty()) return std::nullopt;
		PlanarShapeTrack &tr = out.v.tracks[t];
		std::vector<int> kept;
		const auto write = [&](const Lengths &rep) {
			tr.pieces.clear();
			tr.flex.clear();
			tr.steps.clear();
			kept.clear();
			for (size_t k = 0; k < h.size(); k++) {
				if (rep[k] <= 0) continue;
				const bool middle = std::ranges::find(f.v.tracks[t].flex, static_cast<uint32_t>(k)) != f.v.tracks[t].flex.end();
				if (!tr.pieces.empty() && tr.pieces.back() == h[k]) {
					kept.back() += rep[k];
					if (middle && std::ranges::find(tr.steps, static_cast<uint32_t>(tr.pieces.size() - 1)) == tr.steps.end()) tr.steps.push_back(static_cast<uint32_t>(tr.pieces.size() - 1));
				} else {
					if (middle) tr.steps.push_back(static_cast<uint32_t>(tr.pieces.size()));
					tr.pieces.push_back(h[k]);
					kept.push_back(rep[k]);
				}
			}
			return std::ranges::adjacent_find(tr.pieces, [](int a, int b) { return std::abs(a - b) > 1; }) == tr.pieces.end();
		};
		size_t chosen = reps.size();
		for (size_t r = 0; r < reps.size() && chosen == reps.size(); r++) {
			if (write(reps[r])) chosen = r;
		}
		if (chosen == reps.size()) {
			write(reps[0]);
			g_turn_rejected++;
		} else if (chosen > 0) {
			g_turn_moved++;
		}
		if (tr.pieces.empty()) {
			/* A track with no run (D433): one piece of length 0, its first heading. */
			tr.pieces.push_back(h[0]);
			kept.push_back(0);
		}
		Point cur = start_of.at(name);
		out.points[t].push_back(cur);
		for (size_t k = 0; k < kept.size(); k++) {
			const int hh = ((tr.pieces[k] % 8) + 8) % 8;
			cur[0] += kept[k] * STEPS[hh][0];
			cur[1] += kept[k] * STEPS[hh][1];
			out.points[t].push_back(cur);
		}
	}
	NormalisePoints(out.points);
	const auto &map = f.v.structure.map;
	for (uint32_t c = 0; c < map.crossings.size(); c++) {
		const uint32_t t[2] = {map.crossings[c].a, map.crossings[c].b};
		size_t pos[2];
		int heading[2];
		for (int i = 0; i < 2; i++) {
			const auto &ev = map.events[t[i]];
			pos[i] = static_cast<size_t>(std::ranges::find(ev, c) - ev.begin());
			REQUIRE(pos[i] < ev.size());
			heading[i] = ((f.v.tracks[t[i]].pieces[f.v.tracks[t[i]].event_piece[pos[i]]] % 8) + 8) % 8;
		}
		const auto head_of = [&](uint32_t track, size_t piece) { return ((out.v.tracks[track].pieces[piece] % 8) + 8) % 8; };
		bool found = false;
		for (size_t i = 0; i < out.v.tracks[t[0]].pieces.size() && !found; i++) {
			if (head_of(t[0], i) != heading[0]) continue;
			for (size_t j = 0; j < out.v.tracks[t[1]].pieces.size() && !found; j++) {
				if (head_of(t[1], j) != heading[1]) continue;
				if (!SegmentsMeetInOnePoint(out.points[t[0]][i], out.points[t[0]][i + 1], out.points[t[1]][j], out.points[t[1]][j + 1])) continue;
				out.v.tracks[t[0]].event_piece[pos[0]] = static_cast<uint32_t>(i);
				out.v.tracks[t[1]].event_piece[pos[1]] = static_cast<uint32_t>(j);
				found = true;
			}
		}
		if (!found) return std::nullopt;
	}
	return out;
}

/** One configuration: the axes (track, piece, values) enumerated together. */
struct Config {
	std::string name;
	std::vector<std::tuple<std::string, uint32_t, std::vector<int>>> axes;
	std::vector<std::pair<std::string, uint32_t>> absorbers = {}; ///< Absorbing pieces (track, piece): the piece takes the one length that puts the track's end where the base has it; a geometry with no such length (it would move that end) is skipped.
};

/** The length of an absorbing piece that puts its track's end at the base's, given the end with that piece at 0; none when no length does. */
std::optional<int> AbsorbLength(const X1Fixture &f, const std::vector<Points> &base_points, const std::string &name, uint32_t k, const Point &end_without)
{
	const Point &target = base_points[f.idx.at(name)].back();
	const int hh = ((f.heads.at(name)[k] % 8) + 8) % 8;
	const int len = STEPS[hh][0] != 0 ? (target[0] - end_without[0]) * STEPS[hh][0] : (target[1] - end_without[1]) * STEPS[hh][1];
	if (len < 0 || end_without[0] + len * STEPS[hh][0] != target[0] || end_without[1] + len * STEPS[hh][1] != target[1]) return std::nullopt;
	return len;
}

/**
 * Set a geometry's absorbing pieces from its other pieces; false when one has no length.
 * A track with two absorbers (x and y both fixed): the 2x2 system for their two lengths; false when it is singular, a length is negative or not an integer, or an axis absorber's length is odd (off the grid).
 */
bool AbsorbGeometry(const X1Fixture &f, const std::vector<Points> &base_points, const Config &cfg, Geometry &g)
{
	std::map<std::string, std::vector<uint32_t>> by_track;
	for (const auto &[name, k] : cfg.absorbers) by_track[name].push_back(k);
	for (const auto &[name, ks] : by_track) {
		if (ks.size() != 2) continue;
		const std::vector<int> &h = f.heads.at(name);
		Point cur = f.starts.at(name);
		const auto it = g.find(name);
		for (uint32_t j = 0; j < h.size(); j++) {
			if (j == ks[0] || j == ks[1]) continue;
			const int hh = ((h[j] % 8) + 8) % 8;
			int len = 0;
			if (it != g.end()) {
				const auto l = it->second.find(j);
				if (l != it->second.end()) len = l->second;
			}
			cur[0] += len * STEPS[hh][0];
			cur[1] += len * STEPS[hh][1];
		}
		const Point &target = base_points[f.idx.at(name)].back();
		const int dx = target[0] - cur[0], dy = target[1] - cur[1];
		const int h1 = ((h[ks[0]] % 8) + 8) % 8, h2 = ((h[ks[1]] % 8) + 8) % 8;
		const int det = STEPS[h1][0] * STEPS[h2][1] - STEPS[h1][1] * STEPS[h2][0];
		if (det == 0) return false;
		const int na = dx * STEPS[h2][1] - dy * STEPS[h2][0], nb = STEPS[h1][0] * dy - STEPS[h1][1] * dx;
		if (na % det != 0 || nb % det != 0) return false;
		const int a = na / det, b = nb / det;
		if (a < 0 || b < 0) return false;
		if ((h1 % 2 == 0 && a % 2 != 0) || (h2 % 2 == 0 && b % 2 != 0)) return false;
		g[name][ks[0]] = a;
		g[name][ks[1]] = b;
	}
	for (const auto &[name, k] : cfg.absorbers) {
		if (by_track[name].size() == 2) continue;
		const std::vector<int> &h = f.heads.at(name);
		Point cur = f.starts.at(name);
		const auto it = g.find(name);
		for (uint32_t j = 0; j < h.size(); j++) {
			const int hh = ((h[j] % 8) + 8) % 8;
			int len = 0;
			if (it != g.end() && j != k) {
				const auto l = it->second.find(j);
				if (l != it->second.end()) len = l->second;
			}
			cur[0] += len * STEPS[hh][0];
			cur[1] += len * STEPS[hh][1];
		}
		const std::optional<int> len = AbsorbLength(f, base_points, name, k, cur);
		if (!len) return false;
		g[name][k] = *len;
	}
	return true;
}

/** Whether a configuration name is a neighbourhood one (`node<i> <track>`, `cross<i> <track>`). */
bool IsNeighbourhood(const std::string &name)
{
	return name.starts_with("node") || name.starts_with("cross");
}

/**
 * The generic neighbourhood configurations (domain-21): for each event of the base (each node point, each crossing) and each track touching it, the k pieces of that track nearest the event are varied (flex or step inserts {0,1,2} diagonal or {0,2,4} axis; other pieces base length plus {-2..2} diagonal or {-4,-2,0,2,4} axis, clipped at 0); two outside pieces absorb so the track's far end (x and y) stays at the base's: the nearest with the event piece's heading and the nearest whose heading is not parallel to it (not equal mod 4); their lengths solve a 2x2 system per geometry. One configuration per (event, track) named `<event> <track>`, plus one per crossing varying both its tracks (`cross<i> <a>+<b>`). A (event, track) lacking either absorber outside its window is skipped.
 */
std::vector<Config> NeighbourhoodConfigs(const X1Fixture &f, uint32_t k)
{
	const auto &map = f.v.structure.map;
	/** The window of the k pieces nearest the piece `at` (ties upstream first), and the absorbing piece; false when there is none. */
	const auto make = [&](uint32_t t, uint32_t at, Config &out) {
		const std::string &name = f.Name(t);
		const std::vector<int> &h = f.heads.at(name);
		const uint32_t n = static_cast<uint32_t>(h.size());
		std::vector<uint32_t> order;
		for (uint32_t d = 0; d < n && order.size() < k; d++) {
			if (d == 0) {
				order.push_back(at);
				continue;
			}
			if (at >= d) order.push_back(at - d);
			if (at + d < n && order.size() < k) order.push_back(at + d);
		}
		order.resize(std::min<size_t>(order.size(), k));
		std::ranges::sort(order);
		const uint32_t lo = order.front(), hi = order.back();
		const int heading = ((h[at] % 8) + 8) % 8;
		/* Two absorbers: the nearest outside piece with the event piece's heading, and the nearest whose heading is not parallel to it (not equal mod 4). */
		std::optional<uint32_t> absorber, cross_absorber;
		for (uint32_t d = 1; d < n; d++) {
			for (const int side : {1, -1}) {
				if (side == 1 ? hi + d >= n : lo < d) continue;
				const uint32_t q = side == 1 ? hi + d : lo - d;
				const int hq = ((h[q] % 8) + 8) % 8;
				if (!absorber && hq == heading) absorber = q;
				if (!cross_absorber && hq % 4 != heading % 4) cross_absorber = q;
			}
		}
		if (!absorber || !cross_absorber) return false;
		const auto &tr = f.v.tracks[t];
		const auto base_it = f.base.find(name);
		for (const uint32_t p : order) {
			const bool axis = (((h[p] % 8) + 8) % 8) % 2 == 0;
			std::vector<int> vals;
			if (std::ranges::find(tr.flex, p) != tr.flex.end() || std::ranges::find(tr.steps, p) != tr.steps.end()) {
				vals = axis ? std::vector<int>{0, 2, 4} : std::vector<int>{0, 1, 2};
			} else {
				int base = 0;
				if (base_it != f.base.end()) {
					const auto l = base_it->second.find(p);
					if (l != base_it->second.end()) base = l->second;
				}
				for (const int d : axis ? std::vector<int>{-4, -2, 0, 2, 4} : std::vector<int>{-2, -1, 0, 1, 2}) {
					if (base + d >= 0) vals.push_back(base + d);
				}
			}
			out.axes.emplace_back(name, p, vals);
		}
		out.absorbers.emplace_back(name, *absorber);
		out.absorbers.emplace_back(name, *cross_absorber);
		return true;
	};
	std::vector<Config> configs;
	uint32_t skipped = 0;
	const auto &lines = map.expanded.lines;
	for (uint32_t node = 0; node < map.expanded.nodes.size(); node++) {
		for (uint32_t t = 0; t < f.n; t++) {
			const uint32_t last = static_cast<uint32_t>(f.heads.at(f.Name(t)).size()) - 1;
			for (const bool start : {true, false}) {
				const PlanarEnd &end = start ? lines[t].from : lines[t].to;
				if (end.port || end.index != node) continue;
				Config c{std::format("node{} {}", node, f.Name(t)), {}};
				if (make(t, start ? 0 : last, c)) configs.push_back(c); else skipped++;
			}
		}
	}
	for (uint32_t x = 0; x < map.crossings.size(); x++) {
		const uint32_t tracks[2] = {map.crossings[x].a, map.crossings[x].b};
		Config both{std::format("cross{} {}+{}", x, f.Name(tracks[0]), f.Name(tracks[1])), {}};
		bool both_ok = true;
		for (const uint32_t t : tracks) {
			const auto &ev = map.events[t];
			const size_t pos = static_cast<size_t>(std::ranges::find(ev, x) - ev.begin());
			REQUIRE(pos < ev.size());
			const uint32_t at = f.v.tracks[t].event_piece[pos];
			Config c{std::format("cross{} {}", x, f.Name(t)), {}};
			if (make(t, at, c)) configs.push_back(c); else skipped++;
			both_ok = make(t, at, both) && both_ok;
		}
		if (both_ok) configs.push_back(both); else skipped++;
	}
	if (skipped != 0) WARN("neighbourhood configurations skipped (no pair of absorbing pieces outside the window): " << skipped);
	return configs;
}

/** Per track one entry: the travel of a polyline's points (the sum over consecutive points of max(|dx|, |dy|)), as PlanarDrawOptions::port_zero_from reads it. */
std::vector<std::vector<double>> PortZeroFrom(const std::vector<Points> &points)
{
	std::vector<std::vector<double>> from;
	for (const Points &p : points) {
		double travel = 0;
		for (size_t i = 0; i + 1 < p.size(); i++) travel += std::max(std::abs(p[i + 1][0] - p[i][0]), std::abs(p[i + 1][1] - p[i][1]));
		from.push_back({travel});
	}
	return from;
}

/** The read-outs on one contracted variant. */
struct Readouts {
	PlanarDrawResult keep; ///< The keep-apart pairs.
	PlanarDrawResult program; ///< The whole program (strict rules off).
	std::vector<PlanarPointRow> lead; ///< The node lead rows (none when the rule set has no lead).
};

Readouts g_last_readouts; ///< The drawing program's read-outs of the last DrawingProgramJudge call: the report's diagnostics only, the behaviour is RowsVerdict.

/** D438: the along mode of the polyline's own drawing, as the judge reads it: per track and event, 1 when the track crosses a tunnel there, its piece after the crossing piece has the separator's heading mod 4 and starts on the separator's line. */
std::vector<std::vector<double>> AlongFrom(const Contracted &cv)
{
	const auto &map = cv.v.structure.map;
	std::vector<std::vector<double>> out(cv.v.tracks.size());
	for (size_t t = 0; t < out.size(); t++) out[t].assign(map.events[t].size(), 0.0);
	for (const PlanarStructure &st : cv.v.structure.structures) {
		if (st.kinds != 2 || st.crossings.empty() || st.separator >= out.size()) continue;
		const PlanarShapeTrack &sep = cv.v.tracks[st.separator];
		if (sep.event_piece.size() != map.events[st.separator].size()) continue;
		for (size_t k = 0; k < map.events[st.separator].size(); k++) {
			if (std::ranges::find(st.crossings, map.events[st.separator][k]) == st.crossings.end()) continue;
			const uint32_t sp = sep.event_piece[k];
			const int axis = ((sep.pieces[sp] % 8) + 8) % 8;
			const auto &s0 = cv.points[st.separator][sp];
			const int *step = STEPS[axis];
			for (const uint32_t crosser : st.crossers) {
				const PlanarShapeTrack &tr = cv.v.tracks[crosser];
				if (tr.event_piece.size() != map.events[crosser].size()) continue;
				for (size_t e = 0; e < map.events[crosser].size(); e++) {
					if (map.events[crosser][e] != map.events[st.separator][k]) continue;
					const uint32_t cp = tr.event_piece[e];
					if (cp + 1 >= tr.pieces.size() || ((tr.pieces[cp + 1] % 8) + 8) % 8 % 4 != axis % 4) continue;
					const auto &p = cv.points[crosser][cp + 1];
					const long across = static_cast<long>(step[1]) * (static_cast<long>(p[0]) - s0[0]) - static_cast<long>(step[0]) * (static_cast<long>(p[1]) - s0[1]);
					if (across == 0) out[crosser][e] = 1.0;
				}
			}
		}
	}
	return out;
}

/**
 * The options the judge's keep-apart read-out solves with, for a rule set (as DrawingProgramJudge reads it: "span+lead+endnode+crosspiece+grid+portzero" and its prefixes), without the references (port_zero_from, node_gap_from, along_from: per drawing, set by the caller) and without the read-out flags (program_out, build_only, keep_apart_out).
 * D444 lazy-2: a Slots solve uses exactly these.
 */
PlanarDrawOptions SlotsOptions(const X1Fixture &f, const std::string &rule_set)
{
	std::string rules = rule_set;
	const bool portzero = rules.ends_with("+portzero");
	if (portzero) rules.resize(rules.size() - 9);
	const bool grid = rules.ends_with("+grid");
	if (grid) rules.resize(rules.size() - 5);
	const bool crosspiece = rules.ends_with("+crosspiece");
	if (crosspiece) rules.resize(rules.size() - 11);
	const bool endnode = rules.ends_with("+endnode");
	if (endnode) rules.resize(rules.size() - 8);
	const bool lead = rules.ends_with("+lead");
	if (lead) rules.resize(rules.size() - 5);
	PlanarDrawOptions o = f.options;
	o.corridor = true; // D421: copies and prio rows need the corridor family
	o.node_lead = lead;
	o.end_node = endnode;
	o.cross_piece = crosspiece;
	o.grid = grid;
	o.port_zero = portzero;
	if (rules != "all") o.strict_span = o.strict_zero = o.crossing_flanks = false;
	if (rules == "span" || rules == "zero") {
		o.strict_span = rules == "span";
		o.strict_zero = rules == "zero";
	}
	return o;
}

RowsJudge DrawingProgramJudge(const std::string &rule_set)
{
	/* PLANAR_DOMAIN_RULES (a measurement): plain, span, zero or all (default) strict keep-apart rules. */
	const char *rules_env = rule_set.empty() ? std::getenv("PLANAR_DOMAIN_RULES") : nullptr;
	std::string rules = !rule_set.empty() ? rule_set : rules_env == nullptr ? "all" : rules_env;
	const std::string full_rules = rules;
	const bool portzero = rules.ends_with("+portzero");
	if (portzero) rules.resize(rules.size() - 9);
	const bool grid = rules.ends_with("+grid");
	if (grid) rules.resize(rules.size() - 5);
	const bool crosspiece = rules.ends_with("+crosspiece");
	if (crosspiece) rules.resize(rules.size() - 11);
	const bool endnode = rules.ends_with("+endnode");
	if (endnode) rules.resize(rules.size() - 8);
	const bool lead = rules.ends_with("+lead");
	if (lead) rules.resize(rules.size() - 5);

	return [=](const X1Fixture &f, const Contracted &cv, bool interface_rows) {
		const std::vector<std::vector<double>> port_zero_from = PortZeroFrom(cv.points);
		Readouts ro;
		/* The read-outs get the variant with steps cleared (production's steps cannot yet hold a step at a track's end); only the lead rows read them. */
		PlanarShapeVariant plain = cv.v;
		for (PlanarShapeTrack &track : plain.tracks) track.steps.clear();
		const std::vector<std::vector<double>> along_from = AlongFrom(cv);
		PlanarDrawOptions o = SlotsOptions(f, full_rules);
		o.along_from = &along_from;
		o.port_zero_from = &port_zero_from;
		o.node_gap_from = &port_zero_from;
		o.keep_apart_out = true;
		o.build_only = true;
		if (!interface_rows) o.skip_rows = {"port ", "box"};
		ro.keep = CheckPlanarDrawable(plain, 5.0, o);
		PlanarDrawOptions p = o;
		p.strict_span = p.strict_zero = p.crossing_flanks = false;
		p.program_out = true;
		p.build_only = true;
		if (!interface_rows) p.skip_rows = {"port ", "box"};
		ro.program = CheckPlanarDrawable(plain, 5.0, p);
		/* Contraction merges equal headings, so crossings of one structure on different pieces of its separator mean the separator bends inside its span: the game refuses that (the span rule), the program says so as unsupported (D436). */
		const auto is_span_bend = [](const std::string &status) { return status.starts_with("unsupported: structure ") && status.ends_with("'s crossings sit on different pieces of its separator"); };
		if (is_span_bend(ro.keep.status) || is_span_bend(ro.program.status)) {
			RowsVerdict refused;
			refused.rules.insert("span");
			refused.accept = false;
			g_last_readouts = Readouts();
			return refused;
		}
		REQUIRE(ro.keep.status == "built");
		REQUIRE(ro.program.status == "built");
		if (lead) ro.lead = PlanarNodeLeadRows(cv.v);

		RowsVerdict verdict;
		const bool per_track = std::ranges::all_of(cv.v.tracks, [](const PlanarShapeTrack &tr) { return std::ranges::adjacent_find(tr.pieces, [](int a, int b) { return std::abs(a - b) > 1; }) == tr.pieces.end(); });
		const bool node_turns = NodeTurnsHold(f, cv);
		if (!per_track) verdict.rules.insert("turn");
		if (!node_turns) verdict.rules.insert("node turn");
		for (const PlanarKeepApartPair &pair : ro.keep.keep_apart) {
			if (!PlanarKeepApartHolds(pair, cv.points)) verdict.rules.insert(RuleOf(pair.tag));
		}
		for (const PlanarPointRow &row : ro.lead) {
			if (!PlanarPointRowHolds(row, cv.points)) verdict.rules.insert(RuleOf(row.tag));
		}
		for (const std::string &tag : CheckPlanarProgramOnPoints(ro.program.program, cv.points).broken) verdict.rules.insert(RuleOf(tag));
		verdict.accept = verdict.rules.empty();
		if (per_track && !node_turns && verdict.rules.size() == 1) g_node_turn_rejected++;
		g_last_readouts = std::move(ro);
		return verdict;
	};
}

/** Sides measurement (D431): per keep-apart side rule, the pairs it forces on the legal drawings and the pairs where the forced candidate fails. */
struct SideRule {
	size_t forced = 0;
	size_t failed = 0;
	std::vector<std::string> first;
};
struct SideStats {
	size_t polylines = 0;
	size_t off_interface = 0; ///< Of the polylines, those the domain accepts and only the judge without interface rows accepts.
	size_t pairs = 0; ///< Pairs with rows.
	size_t two_way = 0; ///< Pairs whose inward mask holds two candidates (not forced).
	SideRule walk, strict, facing, both, inward;
	/* sides-5: undecided pairs (neither walk_strict nor facing forces a side), those a third wall implies, and the margins. */
	size_t decided = 0, implied = 0, undecided = 0, implied_failed = 0;
	std::vector<std::string> implied_first;
	struct Ident {
		std::array<uint32_t, 2> tracks, pieces;
		bool conflict = false;
		bool seen_und = false, seen_still = false;
		int least_und = INT_MAX, least_still = INT_MAX;
	};
	std::map<std::string, Ident> idents; ///< By tag.
};
SideStats *g_sides_stats = nullptr;

/** The least over a candidate's rows of s . (q - p) - spacing on the points. */
int CandidateMargin(const PlanarKeepApartPair::Candidate &c, const std::vector<std::vector<std::array<int, 2>>> &points)
{
	int margin = INT_MAX;
	for (const PlanarKeepApartPair::Row &r : c.rows) {
		const std::array<int, 2> p = PlanarKeepApartEnd(r, false, points);
		const std::array<int, 2> q = PlanarKeepApartEnd(r, true, points);
		margin = std::min(margin, c.s[0] * (q[0] - p[0]) + c.s[1] * (q[1] - p[1]) - c.spacing);
	}
	return margin;
}

/** Add one drawing's pairs to the stats. */
void SidesAccumulate(SideStats &st, const X1Fixture &f, const std::string &pkey, const std::vector<PlanarKeepApartPair> &pairs, const std::vector<std::vector<std::array<int, 2>>> &points)
{
	st.polylines++;
	for (const PlanarKeepApartPair &pair : pairs) {
		if (!pair.skipped.empty() || pair.candidates.empty()) continue;
		REQUIRE(pair.candidates.size() == 4);
		st.pairs++;
		const auto holds = [&](int c) {
			PlanarKeepApartPair one = pair;
			one.candidates = {pair.candidates[c]};
			return PlanarKeepApartHolds(one, points);
		};
		const auto note = [&](SideRule &r, int c, bool ok) {
			r.forced++;
			if (ok) return;
			r.failed++;
			if (r.first.size() < 5) r.first.push_back(std::format("{} | {} | walls {}/{} pieces {}/{} | candidate {} | margin {}", pair.tag, pkey, pair.tracks[0] < f.n ? f.Name(pair.tracks[0]) : "box", pair.tracks[1] < f.n ? f.Name(pair.tracks[1]) : "box", pair.pieces[0], pair.pieces[1], c, CandidateMargin(pair.candidates[c], points)));
		};
		const auto with_tag = [&](SideRule &r, int c) { note(r, c, holds(c)); };
		bool both_ok = true;
		bool both_forced = false;
		for (const auto &[rule, c] : {std::pair<SideRule *, int>{&st.walk, pair.walk}, {&st.strict, pair.walk_strict}, {&st.facing, pair.facing}}) {
			if (c < 0) continue;
			with_tag(*rule, c);
			if (rule != &st.walk) {
				both_forced = true;
				both_ok = both_ok && holds(c);
			}
		}
		if (both_forced) {
			st.both.forced++;
			if (!both_ok) {
				st.both.failed++;
				if (st.both.first.size() < 5) st.both.first.push_back(std::format("{} | {} | strict {} facing {}", pair.tag, pkey, pair.walk_strict, pair.facing));
			}
		}
		const int bits = std::popcount(pair.inward);
		bool any = false;
		for (int c = 0; c < 4; c++) any = any || ((pair.inward >> c) & 1) != 0 && holds(c);
		if (bits == 1) {
			st.inward.forced++;
			if (!any) {
				st.inward.failed++;
				if (st.inward.first.size() < 5) st.inward.first.push_back(std::format("{} | {} | inward mask {}", pair.tag, pkey, pair.inward));
			}
		} else {
			st.two_way++;
			if (!any) {
				st.inward.failed++;
				if (st.inward.first.size() < 5) st.inward.first.push_back(std::format("{} | {} | inward mask {} (two-way)", pair.tag, pkey, pair.inward));
			}
		}
	}

	/* sides-5: decided = walk_strict or facing forces a side; implied = a third wall k has both (i,k) and (k,j) forced (or implied) on the same direction s. */
	struct Rel {
		int face;
		std::array<int, 2> s;
		int before, after;
		int spacing;
	};
	struct PairId {
		int face, i, j;
	};
	std::vector<Rel> rels;
	std::vector<std::pair<const PlanarKeepApartPair *, PairId>> und;
	for (const PlanarKeepApartPair &pair : pairs) {
		if (!pair.skipped.empty() || pair.candidates.empty()) continue;
		PairId id{};
		{
			std::istringstream tag_in(pair.tag.substr(4));
			tag_in >> id.face >> id.i >> id.j;
			REQUIRE(!tag_in.fail());
		}
		const int c = pair.walk_strict >= 0 ? pair.walk_strict : pair.facing;
		if (c >= 0) {
			st.decided++;
			const PlanarKeepApartPair::Candidate &cand = pair.candidates[c];
			rels.push_back({id.face, cand.s, (c & 1) == 0 ? id.i : id.j, (c & 1) == 0 ? id.j : id.i, cand.spacing});
		} else {
			und.emplace_back(&pair, id);
		}
	}
	st.undecided += und.size();
	std::vector<bool> is_implied(und.size(), false);
	const auto margin_of = [&](const PlanarKeepApartPair &pair) {
		int best = INT_MIN;
		for (const PlanarKeepApartPair::Candidate &c : pair.candidates) best = std::max(best, std::min(CandidateMargin(c, points), 1000));
		return best;
	};
	for (bool changed = true; changed;) {
		changed = false;
		for (size_t u = 0; u < und.size(); u++) {
			if (is_implied[u]) continue;
			const PlanarKeepApartPair &pair = *und[u].first;
			const PairId id = und[u].second;
			for (int axis = 0; axis < 2 && !is_implied[u]; axis++) {
				const PlanarKeepApartPair::Candidate &c0 = pair.candidates[axis * 2];
				for (int side = 0; side < 2 && !is_implied[u]; side++) {
					const int a = side == 0 ? id.i : id.j, b = side == 0 ? id.j : id.i;
					for (const Rel &r1 : rels) {
						if (r1.face != id.face || r1.s != c0.s || r1.before != a) continue;
						for (const Rel &r2 : rels) {
							if (r2.face != id.face || r2.s != c0.s || r2.before != r1.after || r2.after != b) continue;
							if (c0.spacing > r1.spacing + r2.spacing) continue;
							is_implied[u] = true;
							const int via = r1.after;
							rels.push_back({id.face, c0.s, a, b, c0.spacing}); // r1 and r2 are invalid from here.
							const int cand = axis * 2 + side;
							st.implied++;
							PlanarKeepApartPair one = pair;
							one.candidates = {pair.candidates[cand]};
							if (!PlanarKeepApartHolds(one, points)) {
								st.implied_failed++;
								if (st.implied_first.size() < 5) st.implied_first.push_back(std::format("{} | {} | via wall {} | candidate {} | margin {}", pair.tag, pkey, via, cand, CandidateMargin(pair.candidates[cand], points)));
							}
							changed = true;
							goto next_pair;
						}
					}
				}
			}
		next_pair:;
		}
	}
	/* Identity-level margins: every pair of the read-out (by tag), its least best-candidate margin while undecided / still undecided. */
	for (size_t u = 0; u < und.size(); u++) {
		const PlanarKeepApartPair &pair = *und[u].first;
		SideStats::Ident &e = st.idents.try_emplace(pair.tag, SideStats::Ident{pair.tracks, pair.pieces}).first->second;
		if (e.tracks != pair.tracks || e.pieces != pair.pieces) e.conflict = true;
		const int m = margin_of(pair);
		e.seen_und = true;
		e.least_und = std::min(e.least_und, m);
		if (!is_implied[u]) {
			e.seen_still = true;
			e.least_still = std::min(e.least_still, m);
		}
	}
}

/** The stats as report text lines. */
std::string SidesText(const std::string &name, const SideStats &st)
{
	std::ostringstream o;
	const auto pct = [&](size_t n) { return st.pairs == 0 ? 0.0 : 100.0 * static_cast<double>(n) / static_cast<double>(st.pairs); };
	o << std::format("{}: polylines {} (off interface {}) pair-instances {} (inward two-way {})\n", name, st.polylines, st.off_interface, st.pairs, st.two_way);
	for (const auto &[rule, r] : {std::pair<const char *, const SideRule *>{"walk", &st.walk}, {"walk_strict", &st.strict}, {"facing", &st.facing}, {"strict+facing", &st.both}, {"inward(forced=1 cand)", &st.inward}}) {
		o << std::format("  {}: forced {} ({:.1f}%) failed {}\n", rule, r->forced, pct(r->forced), r->failed);
		for (const std::string &line : r->first) o << "    " << line << "\n";
	}
	o << std::format("  sides-5 instances: pairs {} decided {} implied {} still {} implied-failures {}\n", st.pairs, st.decided, st.implied, st.undecided - st.implied, st.implied_failed);
	for (const std::string &line : st.implied_first) o << "    " << line << "\n";
	size_t und = 0, still = 0, conflict = 0;
	std::array<size_t, 4> und_ge{}, still_ge{};
	for (const auto &[tag, e] : st.idents) {
		if (e.conflict) {
			conflict++;
			continue;
		}
		if (e.seen_und) {
			und++;
			for (size_t k = 0; k < 4; k++) und_ge[k] += e.least_und >= (k == 0 ? 0 : 1 << k) ? 1 : 0;
		}
		if (e.seen_still) {
			still++;
			for (size_t k = 0; k < 4; k++) still_ge[k] += e.least_still >= (k == 0 ? 0 : 1 << k) ? 1 : 0;
		}
	}
	const auto share = [](size_t n, size_t d) { return d == 0 ? 0.0 : 100.0 * static_cast<double>(n) / static_cast<double>(d); };
	o << std::format("  sides-5 identities: undecided {} (tag-conflict skipped {}): least margin >=0 {:.1f}% >=2 {:.1f}% >=4 {:.1f}% >=8 {:.1f}%\n", und, conflict, share(und_ge[0], und), share(und_ge[1], und), share(und_ge[2], und), share(und_ge[3], und));
	o << std::format("  sides-5 identities: still undecided {}: least margin >=0 {:.1f}% >=2 {:.1f}% >=4 {:.1f}% >=8 {:.1f}%\n", still, share(still_ge[0], still), share(still_ge[1], still), share(still_ge[2], still), share(still_ge[3], still));
	return o.str();
}

/** cells-2 measurement: the cells decide each keep-apart pair's side. Per class: counts and failures (a negative margin of the decided candidate). */
struct Cells2 {
	size_t drawings = 0, refused = 0;
	size_t pairs = 0, same = 0, diff = 0, undecided = 0;
	size_t und_wall = 0, und_nosep = 0, und_ambiguous = 0; ///< Undecided: a wall not in the face's cells, no single separating dummy (a landing wall is in both sides), a wall key twice in the face.
	size_t same_two = 0; ///< Same-cell pairs whose two inward candidates differ.
	size_t same_fail_i = 0, same_fail_j = 0, same_fail_both = 0; ///< Margin of i's inward candidate / j's inward candidate negative; both negative.
	size_t diff_fail = 0, diff_fail_all = 0, diff_multi = 0, diff_coincide = 0; ///< First separating dummy's margin negative; every separating dummy's negative; pairs with several separating dummies; candidate coincides with an existing one.
	size_t diff_fail_new = 0, diff_new = 0; ///< Of the pairs whose dummy direction is no existing candidate: count and failures.
	size_t sides4 = 0, both_decided = 0, cells_only = 0, sides4_only = 0, neither = 0; ///< Coverage: sides-4 (walk_strict or facing forces) versus cells.
	size_t agree = 0, disagree = 0, agree_i = 0, agree_j = 0; ///< Pairs both force: the cell's candidate equals the forced one (same cell: either inward); of the agreeing same-cell ones, equal to i's / j's.
	size_t forced_same = 0, forced_diff = 0, disagree_same = 0, disagree_diff = 0;
	std::vector<std::string> first_same, first_diff, first_disagree;
};

/** cells-1 measurement (D385 revisited over contracted lines): per fixture, the convex cells RefinePlanarShapeCells makes on a drawing's contracted variant and the cell rows judged on the drawing's points. */
struct CellStats {
	size_t drawings = 0, legal = 0, illegal = 0, off_interface = 0;
	size_t refused = 0, refused_legal = 0; ///< Refinements refused (all drawings, legal ones).
	std::map<std::string, size_t> refusals; ///< By message.
	size_t refined = 0, dummies = 0, nonmonotone = 0, cells = 0, drawings_nonmono = 0, max_dummies = 0, fallbacks = 0;
	size_t unconvex = 0; ///< Refined variants for which CheckPlanarShapeCells reports a breach.
	std::map<std::string, size_t> unsupported; ///< Cell programs that did not build, by status.
	size_t built = 0, built_legal = 0, built_illegal = 0;
	size_t strict = 0; ///< Legal drawings with a broken row.
	std::map<std::string, size_t> strict_by_family; ///< Legal drawings with a broken row of the family.
	std::vector<std::string> strict_first;
	std::map<std::string, size_t> strict_signature; ///< Legal drawings by their set of broken families.
	size_t pin_native = 0, pin_solved = 0, pin_box = 0, pin_parallel = 0; ///< Dummies whose length and landing parameter were solved here, landing on a box side, with a ray parallel to its landing wall.
	size_t base_legal_accept = 0; ///< Legal drawings the same program without cells accepts (a check of that counterfactual).
	size_t keep_only = 0, keep_only_refused = 0; ///< Illegal drawings that the same program without cells or keep-apart rows (every other rule) accepts, so only a keep-apart rule can refuse them; of them those the cell program refuses.
	std::map<std::string, size_t> undetermined; ///< Built drawings with a row of the family skipped because a variable stayed unknown.
	size_t refuse_illegal = 0; ///< Illegal drawings the cell program refuses.
	std::map<std::string, size_t> refuse_illegal_by_family;
	size_t loose = 0, loose_pairwise_too = 0, loose_off_grid = 0; ///< Illegal drawings the cell program accepts; of them those the pairwise judge also accepts; of them off the grid.
	std::map<std::string, size_t> loose_by_rule; ///< By the domain rule that refuses (once per drawing).
	double ms = 0;
	Cells2 c2;
};
CellStats *g_cells_stats = nullptr;
std::string g_cells_name;

/** The family of a program row tag: the tag up to its first space, `bound`/`integer` with their group. */
std::string CellFamilyOf(const std::string &tag)
{
	size_t end = tag.find(' ');
	const std::string first = tag.substr(0, end);
	if ((first == "bound" || first == "integer") && end != std::string::npos) end = tag.find(' ', end + 1);
	return tag.substr(0, end);
}

/** A cell wall as text. */
std::string CellWallText(const PlanarShapeVariant &v, const PlanarCellWall &w)
{
	const auto &map = v.structure.map;
	if (w.track == UINT32_MAX) return std::format("box {} h{}", w.piece, w.heading);
	if (w.track == PLANAR_DUMMY_TRACK) {
		const PlanarShapeDummy &d = v.dummies[w.piece];
		const std::string land = d.land_track == UINT32_MAX ? std::format("box {}", d.land_piece) : d.land_track == PLANAR_DUMMY_TRACK ? std::format("dummy {}", d.land_piece) : std::format("{} p{}", map.tracks[d.land_track].name, d.land_piece);
		return std::format("dummy {} (from {} bend after p{}, heading {}, lands on {}) walked h{}", w.piece, map.tracks[d.track].name, d.piece, d.heading, land, w.heading);
	}
	return std::format("{} p{} h{}", map.tracks[w.track].name, w.piece, w.heading);
}

/** cells-2: per keep-apart pair of a legal drawing, whether the convex cells decide its side and whether the decided candidate holds. */
void CellsSidesAccumulate(Cells2 &c2, const X1Fixture &f, const std::string &pkey, const Contracted &cv, const std::vector<PlanarKeepApartPair> &pairs)
{
	c2.drawings++;
	PlanarShapeVariant vc = cv.v;
	std::string why;
	if (!RefinePlanarShapeCells(vc, why)) {
		c2.refused++;
		return;
	}
	const std::vector<std::vector<PlanarCellWall>> cells = PlanarShapeCellWalls(vc);
	const std::vector<uint32_t> cell_faces = PlanarShapeCellFaces(vc);
	const std::vector<PlanarFaceWalls> face_walls = PlanarShapeFaceWalls(vc);
	std::map<uint32_t, const PlanarFaceWalls *> face_of;
	for (const PlanarFaceWalls &fw : face_walls) face_of[fw.face] = &fw;
	/* The cell tree: per dummy, the two cells that carry it as a wall. */
	std::vector<std::vector<size_t>> dummy_cells(vc.dummies.size());
	for (size_t c = 0; c < cells.size(); c++) {
		for (const PlanarCellWall &w : cells[c]) {
			if (w.track == PLANAR_DUMMY_TRACK && w.piece < dummy_cells.size()) dummy_cells[w.piece].push_back(c);
		}
	}
	const auto &pts = cv.points;
	/* A candidate's index for "from wall a along s" (a = 0 wall i, 1 wall j): the candidate with that direction and from-wall, or the equivalent (from the other wall along -s); -1 when none. */
	const auto find_candidate = [&](const PlanarKeepApartPair &pair, int a, const std::array<int, 2> &s) {
		for (int c = 0; c < 4; c++) {
			const PlanarKeepApartPair::Candidate &cand = pair.candidates[c];
			const int from = c & 1;
			if (from == a && cand.s == s) return c;
			if (from != a && cand.s == std::array<int, 2>{-s[0], -s[1]}) return c;
		}
		return -1;
	};
	/* The margin of direction s from wall a: an existing candidate's own, else the rows of the from-a candidate (axis 0) at the larger of the two axes' spacings. */
	const auto margin_of = [&](const PlanarKeepApartPair &pair, int a, const std::array<int, 2> &s, int &cand_out, int &spacing_out) {
		cand_out = find_candidate(pair, a, s);
		if (cand_out >= 0) {
			spacing_out = pair.candidates[cand_out].spacing;
			return CandidateMargin(pair.candidates[cand_out], pts);
		}
		spacing_out = std::max(pair.candidates[0].spacing, pair.candidates[2].spacing);
		int m = INT_MAX;
		for (const PlanarKeepApartPair::Row &r : pair.candidates[a == 0 ? 0 : 1].rows) {
			const std::array<int, 2> p = PlanarKeepApartEnd(r, false, pts), q = PlanarKeepApartEnd(r, true, pts);
			m = std::min(m, s[0] * (q[0] - p[0]) + s[1] * (q[1] - p[1]) - spacing_out);
		}
		return m;
	};
	const auto wall_text = [&](const PlanarCellWall &w) { return CellWallText(vc, w); };
	for (const PlanarKeepApartPair &pair : pairs) {
		if (!pair.skipped.empty() || pair.candidates.empty()) continue;
		REQUIRE(pair.candidates.size() == 4);
		c2.pairs++;
		uint32_t face = 0;
		size_t wi = 0, wj = 0;
		{
			std::istringstream tag_in(pair.tag.substr(4));
			tag_in >> face >> wi >> wj;
			REQUIRE(!tag_in.fail());
		}
		const int forced = pair.walk_strict >= 0 ? pair.walk_strict : pair.facing;
		if (forced >= 0) c2.sides4++;
		bool decided = false, agree_known = false, agrees = false, agrees_i = false, is_same = false;
		const auto finish = [&]() {
			if (decided) {
				(forced >= 0 ? c2.both_decided : c2.cells_only)++;
				if (forced >= 0) {
					(is_same ? c2.forced_same : c2.forced_diff)++;
					if (agrees) {
						c2.agree++;
						if (is_same) (agrees_i ? c2.agree_i : c2.agree_j)++;
					} else {
						c2.disagree++;
						(is_same ? c2.disagree_same : c2.disagree_diff)++;
						if (c2.first_disagree.size() < 5) c2.first_disagree.push_back(std::format("{} | {} | {} | forced candidate {}", pair.tag, pkey, is_same ? "same cell" : "different cells", forced));
					}
				}
			} else {
				c2.undecided++;
				(forced >= 0 ? c2.sides4_only : c2.neither)++;
			}
			(void)agree_known;
		};
		const auto fit = face_of.find(face);
		if (fit == face_of.end() || wi >= fit->second->walls.size() || wj >= fit->second->walls.size()) {
			c2.und_wall++;
			finish();
			continue;
		}
		const std::vector<PlanarCellWall> &fw = fit->second->walls;
		const auto same_wall = [](const PlanarCellWall &a, const PlanarCellWall &b) { return a.track == b.track && a.piece == b.piece && a.heading == b.heading; };
		size_t keys_i = 0, keys_j = 0;
		for (const PlanarCellWall &w : fw) {
			keys_i += same_wall(w, fw[wi]) ? 1 : 0;
			keys_j += same_wall(w, fw[wj]) ? 1 : 0;
		}
		if (keys_i != 1 || keys_j != 1) {
			c2.und_ambiguous++;
			finish();
			continue;
		}
		std::vector<size_t> face_cells, ci, cj;
		for (size_t c = 0; c < cells.size(); c++) {
			if (cell_faces[c] != face) continue;
			face_cells.push_back(c);
			for (const PlanarCellWall &w : cells[c]) {
				if (same_wall(w, fw[wi]) && std::ranges::find(ci, c) == ci.end()) ci.push_back(c);
				if (same_wall(w, fw[wj]) && std::ranges::find(cj, c) == cj.end()) cj.push_back(c);
			}
		}
		if (ci.empty() || cj.empty()) {
			c2.und_wall++;
			finish();
			continue;
		}
		const int hi = fw[wi].heading, hj = fw[wj].heading;
		const std::string walls_text = std::format("walls {} / {} (heading {} / {})", wall_text(fw[wi]), wall_text(fw[wj]), hi, hj);
		const bool common = std::ranges::any_of(ci, [&](size_t c) { return std::ranges::find(cj, c) != cj.end(); });
		if (common) {
			c2.same++;
			decided = true;
			is_same = true;
			const std::array<int, 2> si = {STEPS[(hi + 6) % 8][0], STEPS[(hi + 6) % 8][1]}, sj = {STEPS[(hj + 6) % 8][0], STEPS[(hj + 6) % 8][1]};
			int cand_i = 0, cand_j = 0, spacing = 0;
			const int mi = margin_of(pair, 0, si, cand_i, spacing);
			const int mj = margin_of(pair, 1, sj, cand_j, spacing);
			REQUIRE(cand_i >= 0);
			REQUIRE(cand_j >= 0);
			if (cand_i != cand_j) c2.same_two++;
			const bool fi = mi < 0, fj = mj < 0;
			if (fi) c2.same_fail_i++;
			if (fj) c2.same_fail_j++;
			if (fi && fj) {
				c2.same_fail_both++;
				if (c2.first_same.size() < 5) c2.first_same.push_back(std::format("{} | {} | {} | i-inward c{} margin {}, j-inward c{} margin {}", pair.tag, pkey, walls_text, cand_i, mi, cand_j, mj));
			}
			agrees = forced >= 0 && (forced == cand_i || forced == cand_j);
			agrees_i = forced == cand_i;
			finish();
			continue;
		}
		/* Different cells: the dummies whose removal from the cell tree puts every cell of wall i on one side and every cell of wall j on the other. */
		std::map<size_t, int> dist;
		{
			std::vector<size_t> queue = ci;
			for (const size_t c : ci) dist[c] = 0;
			for (size_t head = 0; head < queue.size(); head++) {
				const size_t c = queue[head];
				for (const PlanarCellWall &w : cells[c]) {
					if (w.track != PLANAR_DUMMY_TRACK) continue;
					for (const size_t o : dummy_cells[w.piece]) {
						if (o != c && !dist.contains(o)) {
							dist[o] = dist[c] + 1;
							queue.push_back(o);
						}
					}
				}
			}
		}
		struct Sep {
			size_t dummy;
			size_t side_cell; ///< The dummy's cell on wall i's side.
			int near;
		};
		std::vector<Sep> seps;
		for (size_t d = 0; d < vc.dummies.size(); d++) {
			if (vc.dummies[d].face != face || dummy_cells[d].size() != 2) continue;
			/* Component of a cell with the edge d removed: flood from it. */
			const auto component = [&](size_t start) {
				std::set<size_t> seen = {start};
				std::vector<size_t> queue = {start};
				for (size_t head = 0; head < queue.size(); head++) {
					for (const PlanarCellWall &w : cells[queue[head]]) {
						if (w.track != PLANAR_DUMMY_TRACK || w.piece == d) continue;
						for (const size_t o : dummy_cells[w.piece]) {
							if (seen.insert(o).second) queue.push_back(o);
						}
					}
				}
				return seen;
			};
			const std::set<size_t> a = component(dummy_cells[d][0]);
			const bool i_all_a = std::ranges::all_of(ci, [&](size_t c) { return a.contains(c); }), i_none_a = std::ranges::none_of(ci, [&](size_t c) { return a.contains(c); });
			const bool j_all_a = std::ranges::all_of(cj, [&](size_t c) { return a.contains(c); }), j_none_a = std::ranges::none_of(cj, [&](size_t c) { return a.contains(c); });
			if (!((i_all_a && j_none_a) || (i_none_a && j_all_a))) continue;
			const size_t side = i_all_a ? dummy_cells[d][0] : dummy_cells[d][1];
			seps.push_back({d, side, std::min(dist.contains(dummy_cells[d][0]) ? dist[dummy_cells[d][0]] : INT_MAX, dist.contains(dummy_cells[d][1]) ? dist[dummy_cells[d][1]] : INT_MAX)});
		}
		if (seps.empty()) {
			c2.und_nosep++;
			finish();
			continue;
		}
		std::ranges::stable_sort(seps, [](const Sep &x, const Sep &y) { return x.near < y.near; });
		c2.diff++;
		decided = true;
		if (seps.size() > 1) c2.diff_multi++;
		bool all_fail = true;
		int first_margin = 0, first_cand = -1, first_spacing = 0;
		std::array<int, 2> first_s{};
		for (size_t k = 0; k < seps.size(); k++) {
			int h = -1;
			for (const PlanarCellWall &w : cells[seps[k].side_cell]) {
				if (w.track == PLANAR_DUMMY_TRACK && w.piece == seps[k].dummy) h = w.heading;
			}
			REQUIRE(h >= 0);
			const std::array<int, 2> s = {STEPS[(h + 2) % 8][0], STEPS[(h + 2) % 8][1]};
			int cand = -1, spacing = 0;
			const int m = margin_of(pair, 0, s, cand, spacing);
			all_fail = all_fail && m < 0;
			if (k == 0) {
				first_margin = m;
				first_cand = cand;
				first_spacing = spacing;
				first_s = s;
			}
		}
		if (first_cand >= 0) c2.diff_coincide++;
		else c2.diff_new++;
		if (all_fail) c2.diff_fail_all++;
		if (first_margin < 0) {
			c2.diff_fail++;
			if (first_cand < 0) c2.diff_fail_new++;
			if (c2.first_diff.size() < 5) c2.first_diff.push_back(std::format("{} | {} | {} | dummy {} ({}) s({},{}) spacing {} margin {} {}", pair.tag, pkey, walls_text, seps[0].dummy, wall_text({PLANAR_DUMMY_TRACK, static_cast<uint32_t>(seps[0].dummy), 0}), first_s[0], first_s[1], first_spacing, first_margin, first_cand >= 0 ? std::format("(= candidate {})", first_cand) : "(no existing candidate)"));
		}
		agrees = forced >= 0 && first_cand == forced;
		agrees_i = true;
		finish();
	}
	(void)f;
}

/** cells-2 stats as a report line block. */
std::string Cells2Text(const std::string &name, const Cells2 &c)
{
	std::ostringstream o;
	o << std::format("cells-2 {}: drawings {} (refinement refused {}) pairs {} same {} diff {} undecided {} (wall {}, no separating dummy {}, ambiguous {})\n", name, c.drawings, c.refused, c.pairs, c.same, c.diff, c.undecided, c.und_wall, c.und_nosep, c.und_ambiguous);
	o << std::format("  same-cell: two distinct inward candidates {}; i-inward fails {}, j-inward fails {}, neither holds {}\n", c.same_two, c.same_fail_i, c.same_fail_j, c.same_fail_both);
	o << std::format("  different-cell: several separating dummies {}; coincides with an existing candidate {}, new direction {} (fails {}); first dummy fails {}; every separating dummy fails {}\n", c.diff_multi, c.diff_coincide, c.diff_new, c.diff_fail_new, c.diff_fail, c.diff_fail_all);
	o << std::format("  coverage: sides-4 forces {} ({:.1f}%); cells decide {} ({:.1f}%); both {}, cells only {}, sides-4 only {}, neither {}\n", c.sides4, c.pairs == 0 ? 0.0 : 100.0 * static_cast<double>(c.sides4) / static_cast<double>(c.pairs), c.same + c.diff, c.pairs == 0 ? 0.0 : 100.0 * static_cast<double>(c.same + c.diff) / static_cast<double>(c.pairs), c.both_decided, c.cells_only, c.sides4_only, c.neither);
	o << std::format("  agreement (forced and decided {}): agree {} (same-cell {} of which i-inward {}, j-inward {}), disagree {} (same-cell {}, different-cell {})\n", c.both_decided, c.agree, c.forced_same - c.disagree_same, c.agree_i, c.agree_j, c.disagree, c.disagree_same, c.disagree_diff);
	for (const std::string &l : c.first_same) o << "  first same-cell failure: " << l << "\n";
	for (const std::string &l : c.first_diff) o << "  first different-cell failure: " << l << "\n";
	for (const std::string &l : c.first_disagree) o << "  first disagreement: " << l << "\n";
	return o.str();
}

/** Judge the cell program on one drawing's contracted variant. `legal` is the domain's verdict of the drawing, `off_interface` that the judge for it leaves out the port and box rows. */
void CellsAccumulate(CellStats &st, const X1Fixture &f, const std::string &pkey, const Contracted &cv, const PlanarDomainVerdict &domain, bool off_interface, bool pairwise_accept, bool on_grid)
{
	const auto t0 = std::chrono::steady_clock::now();
	const bool legal = domain.legal;
	st.drawings++;
	(legal ? st.legal : st.illegal)++;
	if (off_interface) st.off_interface++;
	PlanarShapeVariant vc = cv.v;
	std::string why;
	if (!RefinePlanarShapeCells(vc, why)) {
		st.refused++;
		if (legal) st.refused_legal++;
		std::string key = why;
		for (char &c : key) if (c >= '0' && c <= '9') c = '#';
		st.refusals[key]++;
		st.ms += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
		return;
	}
	st.refined++;
	st.dummies += vc.dummies.size();
	st.nonmonotone += vc.nonmonotone_dummies;
	st.fallbacks += vc.landing_fallbacks;
	if (vc.nonmonotone_dummies > 0) st.drawings_nonmono++;
	st.max_dummies = std::max(st.max_dummies, vc.dummies.size());
	const std::vector<std::vector<PlanarCellWall>> cells = PlanarShapeCellWalls(vc);
	st.cells += cells.size();
	if (const std::vector<std::string> h12 = CheckPlanarShapeCells(vc); !h12.empty()) {
		st.unconvex++;
		if (g_cells_name == "197") {
			std::string text = std::format("197 H12 breaches: {}\n", h12.size());
			for (size_t i = 0; i < h12.size() && i < 5; i++) text += "    " + h12[i] + "\n";
			st.strict_first.push_back(text);
		}
	}

	/* The cell program: the judge's options with the cell machinery (clearance, clear rows, no pairwise), the default rule set. */
	const std::vector<std::vector<double>> port_zero_from = PortZeroFrom(cv.points);
	const std::vector<std::vector<double>> along_from = AlongFrom(cv);
	PlanarShapeVariant plain = vc;
	for (PlanarShapeTrack &track : plain.tracks) track.steps.clear();
	PlanarDrawOptions o = f.options;
	o.along_from = &along_from;
	o.node_lead = true;
	o.end_node = true;
	o.cross_piece = true;
	o.grid = true;
	o.port_zero = true;
	o.port_zero_from = &port_zero_from;
	o.node_gap_from = &port_zero_from;
	o.strict_span = o.strict_zero = o.crossing_flanks = false;
	o.clearance = true;
	o.clear_rows = true;
	o.pairwise = false;
	o.pairwise_all_faces = false;
	o.keep_apart_out = true;
	o.program_out = true;
	o.build_only = true;
	if (off_interface) o.skip_rows = {"port ", "box"};
	const PlanarDrawResult r = CheckPlanarDrawable(plain, 5.0, o);
	const auto is_span_bend = [](const std::string &status) { return status.starts_with("unsupported: structure ") && status.ends_with("'s crossings sit on different pieces of its separator"); };
	std::set<std::string> broken_families;
	std::vector<std::string> broken_tags;
	if (is_span_bend(r.status)) {
		broken_families.insert("span");
	} else if (r.status != "built") {
		std::string key = r.status;
		for (char &c : key) if (c >= '0' && c <= '9') c = '#';
		st.unsupported[key]++;
		st.ms += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
		return;
	}
	st.built++;
	(legal ? st.built_legal : st.built_illegal)++;
	if (!is_span_bend(r.status)) {
		/* The checker eliminates a variable only from an equality row with one unknown or two rows on the same two unknowns, so a dummy whose ray and landing wall are both diagonal or both off the axes stays undetermined (four rows on its end, its length and the landing parameter). Here the variables the points fix are propagated through the program's equality rows, then each dummy's `land` equalities are solved as a small linear system; a unique solution is written back as equality rows tagged `pinned`. No value is chosen: it is the rows' own solution. A dummy whose equalities leave an unknown (a box side the points do not fix) is left to the checker. */
		PlanarProgram prog = r.program;
		prog.plan.reset();
		{
			const size_t nv = prog.lower.size();
			std::vector<double> val(nv, 0);
			std::vector<bool> known(nv, false);
			for (size_t t = 0; t < prog.point.size() && t < cv.points.size(); t++) {
				for (size_t i = 0; i < prog.point[t].size() && i < cv.points[t].size(); i++) {
					for (int axis = 0; axis < 2; axis++) {
						val[prog.point[t][i][axis]] = cv.points[t][i][axis];
						known[prog.point[t][i][axis]] = true;
					}
				}
			}
			const auto propagate = [&]() {
				for (bool changed = true; changed;) {
					changed = false;
					for (const PlanarProgram::Row &row : prog.rows) {
						if (row.lower != row.upper) continue;
						size_t unknown = 0;
						uint32_t which = 0;
						double coef = 0, rest = 0;
						for (const auto &[v, c] : row.terms) {
							if (c == 0) continue;
							if (known[v]) {
								rest += c * val[v];
							} else {
								unknown++;
								which = v;
								coef += c;
							}
						}
						if (unknown == 0 || coef == 0) continue;
						bool same = true;
						for (const auto &[v, c] : row.terms) same = same && (c == 0 || known[v] || v == which);
						if (!same) continue;
						val[which] = (row.lower - rest) / coef;
						known[which] = true;
						changed = true;
					}
				}
			};
			propagate();
			std::set<size_t> solved, native;
			for (int pass = 0; pass < 3; pass++) {
				for (size_t d = 0; d < vc.dummies.size(); d++) {
					const std::string tag = std::format("land {}", d);
					std::vector<const PlanarProgram::Row *> eqs;
					std::vector<uint32_t> unk;
					for (const PlanarProgram::Row &row : prog.rows) {
						if (row.tag != tag || row.lower != row.upper) continue;
						eqs.push_back(&row);
						for (const auto &[v, c] : row.terms) {
							if (c != 0 && !known[v] && std::ranges::find(unk, v) == unk.end()) unk.push_back(v);
						}
					}
					if (unk.empty()) {
						if (pass == 0) native.insert(d);
						continue;
					}
					/* Gauss-Jordan on [A | b], unknowns in unk. */
					std::vector<std::vector<double>> m;
					for (const PlanarProgram::Row *row : eqs) {
						std::vector<double> line(unk.size() + 1, 0.0);
						double rest = 0;
						for (const auto &[v, c] : row->terms) {
							if (c == 0) continue;
							if (known[v]) {
								rest += c * val[v];
							} else {
								line[static_cast<size_t>(std::ranges::find(unk, v) - unk.begin())] += c;
							}
						}
						line.back() = row->lower - rest;
						m.push_back(std::move(line));
					}
					size_t rank = 0;
					std::vector<size_t> pivot_col;
					for (size_t col = 0; col < unk.size() && rank < m.size(); col++) {
						size_t best = rank;
						for (size_t i = rank; i < m.size(); i++) {
							if (std::abs(m[i][col]) > std::abs(m[best][col])) best = i;
						}
						if (std::abs(m[best][col]) < 1e-9) continue;
						std::swap(m[rank], m[best]);
						const double pv = m[rank][col];
						for (double &x : m[rank]) x /= pv;
						for (size_t i = 0; i < m.size(); i++) {
							if (i == rank || std::abs(m[i][col]) < 1e-12) continue;
							const double fct = m[i][col];
							for (size_t k = 0; k < m[i].size(); k++) m[i][k] -= fct * m[rank][k];
						}
						pivot_col.push_back(col);
						rank++;
					}
					if (rank != unk.size()) continue;
					bool consistent = true;
					for (size_t i = rank; i < m.size(); i++) consistent = consistent && std::abs(m[i].back()) < 1e-6;
					if (!consistent) continue;
					for (size_t i = 0; i < rank; i++) {
						const uint32_t var = unk[pivot_col[i]];
						val[var] = m[i].back();
						known[var] = true;
						prog.rows.push_back({{{var, 1.0}}, m[i].back(), m[i].back(), std::format("pinned {}", d)});
					}
					solved.insert(d);
					propagate();
				}
			}
			for (size_t d = 0; d < vc.dummies.size(); d++) {
				if (native.contains(d)) {
					st.pin_native++;
				} else if (solved.contains(d)) {
					st.pin_solved++;
				} else if (vc.dummies[d].land_track == UINT32_MAX) {
					st.pin_box++;
				} else {
					st.pin_parallel++;
				}
			}
		}
		const PlanarProgramCheck check = CheckPlanarProgramOnPoints(prog, cv.points);
		for (const std::string &u : check.undetermined) {
			st.undetermined[CellFamilyOf(u)]++;
		}
		for (const std::string &b : check.broken) {
			broken_families.insert(CellFamilyOf(b));
			broken_tags.push_back(b);
		}
		for (const PlanarPointRow &row : PlanarNodeLeadRows(cv.v)) {
			if (!PlanarPointRowHolds(row, cv.points)) broken_families.insert("lead");
		}
		const bool per_track = std::ranges::all_of(cv.v.tracks, [](const PlanarShapeTrack &tr) { return std::ranges::adjacent_find(tr.pieces, [](int a, int b) { return std::abs(a - b) > 1; }) == tr.pieces.end(); });
		if (!per_track) broken_families.insert("turn");
		if (!NodeTurnsHold(f, cv)) broken_families.insert("node turn");
	}
	if (legal || g_cells_name == "197") {
		if (!broken_families.empty()) {
			if (legal) st.strict++;
			{
				std::string sig;
				for (const std::string &fam : broken_families) sig += (sig.empty() ? "" : "+") + fam;
				st.strict_signature[sig]++;
			}
			for (const std::string &fam : broken_families) st.strict_by_family[fam]++;
			if (st.strict_first.size() < 5 || g_cells_name == "197") {
				std::string text = std::format("{} polyline {}\n", g_cells_name, pkey);
				text += std::format("    {} cell program broken rows (families):", broken_tags.size());
				for (const std::string &fam : broken_families) text += " " + fam;
				text += "\n";
				for (size_t i = 0; i < broken_tags.size() && i < (g_cells_name == "197" ? 400 : 3); i++) {
					if (g_cells_name == "197" && (broken_tags[i].starts_with("bound") || broken_tags[i].starts_with("integer"))) {
						text += "    row " + broken_tags[i] + "\n";
						continue;
					}
					const std::string &tag = broken_tags[i];
					text += "    row " + tag + "\n";
					std::istringstream ts(tag);
					std::string family;
					std::vector<size_t> n;
					ts >> family;
					for (size_t x; ts >> x;) n.push_back(x);
					const size_t c = n.size() == 3 ? n[0] : 0, k = n.size() == 3 ? n[1] : 0, e = n.size() == 3 ? n[2] : 0, d = n.empty() ? 0 : n[0];
					if (n.size() == 3 && (family == "clear" || family == "room" || family == "along")) {
						if (c < cells.size() && k < cells[c].size() && e < cells[c].size()) {
							const auto &cell = cells[c];
							text += std::format("      cell {} ({} walls): vertex after wall {}: {}; next wall {}; target wall {}: {}\n", c, cell.size(), k, CellWallText(vc, cell[k]), CellWallText(vc, cell[(k + 1) % cell.size()]), e, CellWallText(vc, cell[e]));
						}
					} else if (n.size() == 1 && family == "land" && d < vc.dummies.size()) {
						const PlanarShapeDummy &dm = vc.dummies[d];
						const std::string land = dm.land_track == UINT32_MAX ? std::format("box {}", dm.land_piece) : dm.land_track == PLANAR_DUMMY_TRACK ? std::format("dummy {}", dm.land_piece) : std::format("{} p{}", vc.structure.map.tracks[dm.land_track].name, dm.land_piece);
						text += std::format("      dummy {} from {} bend after p{}, heading {}, lands on {}\n", d, vc.structure.map.tracks[dm.track].name, dm.piece, dm.heading, land);
					}
				}
				st.strict_first.push_back(text);
			}
		}
	}
	{
		PlanarDrawOptions o2 = o;
		o2.clearance = false;
		o2.clear_rows = false;
		const PlanarDrawResult b = CheckPlanarDrawable(plain, 5.0, o2);
		bool base_accept = b.status == "built" || is_span_bend(b.status) ? !is_span_bend(b.status) : false;
		if (base_accept) base_accept = CheckPlanarProgramOnPoints(b.program, cv.points).broken.empty();
		if (base_accept) {
			for (const PlanarPointRow &row : PlanarNodeLeadRows(cv.v)) base_accept = base_accept && PlanarPointRowHolds(row, cv.points);
			base_accept = base_accept && NodeTurnsHold(f, cv) && std::ranges::all_of(cv.v.tracks, [](const PlanarShapeTrack &tr) { return std::ranges::adjacent_find(tr.pieces, [](int a, int b) { return std::abs(a - b) > 1; }) == tr.pieces.end(); });
		}
		if (base_accept && legal) st.base_legal_accept++;
		if (base_accept && !legal) {
			st.keep_only++;
			if (!broken_families.empty()) st.keep_only_refused++;
		}
	}
	if (!legal && broken_families.empty()) {
		st.loose++;
		if (pairwise_accept) st.loose_pairwise_too++;
		if (!on_grid) st.loose_off_grid++;
		std::set<std::string> rules;
		for (const PlanarDomainBreak &b : domain.breaks) rules.insert(b.rule);
		for (const std::string &rule : rules) st.loose_by_rule[rule]++;
	} else if (!legal) {
		st.refuse_illegal++;
		for (const std::string &fam : broken_families) st.refuse_illegal_by_family[fam]++;
	}
	st.ms += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

std::string CellsText(const std::string &name, const CellStats &st)
{
	std::ostringstream o;
	const auto map_text = [](const std::map<std::string, size_t> &m) {
		std::string s;
		for (const auto &[k, n] : m) s += std::format(" [{}] {};", k, n);
		return s.empty() ? std::string(" none") : s;
	};
	o << std::format("{}: drawings {} (legal {} of them off interface {}, illegal {}); {:.1f} s\n", name, st.drawings, st.legal, st.off_interface, st.illegal, st.ms / 1000);
	o << std::format("  refinement refused {} (legal {}):{}\n", st.refused, st.refused_legal, map_text(st.refusals));
	o << std::format("  refined {}: dummies {} (max {} per drawing), nonmonotone {} (drawings with one {}), cells {}, landing_fallbacks {}, H12 breaches {} drawings\n", st.refined, st.dummies, st.max_dummies, st.nonmonotone, st.drawings_nonmono, st.cells, st.fallbacks, st.unconvex);
	o << std::format("  cell program built {} (legal {}, illegal {}), not built:{}\n", st.built, st.built_legal, st.built_illegal, map_text(st.unsupported));
	o << std::format("  too strict: {} of {} built legal drawings; by family:{}\n", st.strict, st.built_legal, map_text(st.strict_by_family));
	o << std::format("  dummies (built drawings): determined by the checker alone {}, solved here {}, left undetermined: landing on a box side {}, landing on a track or dummy {}\n", st.pin_native, st.pin_solved, st.pin_box, st.pin_parallel);
	o << std::format("  too strict by set of broken families:{}\n", map_text(st.strict_signature));
	o << std::format("  undetermined rows (drawings per family):{}\n", map_text(st.undetermined));
	o << std::format("  counterfactual check: legal drawings the program without cells accepts {} of {}\n", st.base_legal_accept, st.built_legal);
	o << std::format("  illegal drawings every other rule accepts (only a keep-apart rule can refuse them): {}; the cell program refuses {}\n", st.keep_only, st.keep_only_refused);
	o << std::format("  illegal drawings refused by the cell program {} (families:{}); too loose {} (also accepted by the pairwise judge {}, off grid {}); by domain rule:{}\n", st.refuse_illegal, map_text(st.refuse_illegal_by_family), st.loose, st.loose_pairwise_too, st.loose_off_grid, map_text(st.loose_by_rule));
	for (const std::string &t : st.strict_first) o << "  first too strict: " << t;
	o << Cells2Text(name, st.c2);
	return o.str();
}

/** Judge a polyline's contracted writing against the domain on a fixture's configurations and write the report. */
void RunEncoding(const X1Fixture &f, const char *report_path, const std::string &title, const std::vector<Config> &configs, const std::string &header, const RowsJudge &judge, int *base_verdict = nullptr, std::array<uint32_t, 8> *totals = nullptr)
{
	REQUIRE((report_path != nullptr || base_verdict != nullptr || totals != nullptr));

	const auto t0 = std::chrono::steady_clock::now();
	std::map<std::string, Tally> per;
	std::map<std::string, double> cfg_wall;
	Tally total;
	Tally section[2]; ///< The hand-named configurations (0) and the neighbourhood ones (1).
	std::map<std::string, PolyClass> classes;

	auto polyline_key = [&](const Geometry &g) {
		std::string pkey;
		for (const auto &[name, h] : f.heads) {
			Lengths l(h.size(), 0);
			const auto it = g.find(name);
			if (it != g.end()) {
				for (const auto &[k, len] : it->second) l[k] = len;
			}
			pkey += name + ":";
			std::ostringstream run_text;
			for (const auto &[hh, len] : PolylineRuns(h, l)) run_text << hh << "*" << len << ",";
			pkey += run_text.str() + ";";
		}
		return pkey;
	};
	/** The number of the program read-out's corridor rows. */
	auto corridor_rows = [](const Readouts &ro) {
		return static_cast<uint32_t>(std::ranges::count_if(ro.program.program.rows, [](const PlanarProgram::Row &r) { return r.tag.starts_with("corridor"); }));
	};
	if (base_verdict != nullptr) {
		/* Only the base polyline: 0 the rows accept it, 1 reject, 2 other shape. */
		const std::optional<Contracted> cv = ContractedVariant(f, f.base);
		if (!cv) {
			*base_verdict = 2;
			return;
		}
		*base_verdict = judge(f, *cv, true).accept ? 0 : 1;
		return;
	}
	g_lead_rows_seen = 0;
	g_corridor_rows_seen = 0;
	g_corridor_rows_base = 0;
	g_turn_moved = 0;
	g_turn_rejected = 0;
	g_node_turn_rejected = 0;
	g_lead_by_family.clear();
	{
		const std::optional<Contracted> cv = ContractedVariant(f, f.base);
		const bool base_accept = cv && judge(f, *cv, true).accept;
		if (cv) g_corridor_rows_base = corridor_rows(g_last_readouts);
		WARN("base polyline: " << (!cv ? "other shape" : base_accept ? "accept" : "reject") << ", corridor rows in its read-out: " << g_corridor_rows_base);
	}

	const auto Num = [](size_t n) { std::ostringstream o; o << n; return o.str(); };
	/** Where a contracted polyline first leaves the grid: the first track (by index) whose raster says so, and its first corner off an edge midpoint, else its first axis piece off the centre line. */
	auto first_off_grid = [&](const Contracted &cv) {
		for (uint32_t t = 0; t < f.n; t++) {
			if (RasterPlanarDomainTrack(cv.points[t]).rule.empty()) continue;
			const Points &p = cv.points[t];
			for (size_t i = 0; i < p.size(); i++) {
				if ((((p[i][0] + p[i][1]) % 2) + 2) % 2 == 0) return f.Name(t) + " corner " + Num(i);
			}
			for (size_t k = 0; k + 1 < p.size(); k++) {
				const int hh = ((cv.v.tracks[t].pieces[k] % 8) + 8) % 8;
				if (hh % 2 != 0 || p[k] == p[k + 1]) continue;
				const int across = hh == 2 || hh == 6 ? p[k][1] : p[k][0];
				if ((across % 2 + 2) % 2 == 0) return f.Name(t) + " piece " + Num(k) + " off the centre line";
			}
			return f.Name(t) + " (raster: " + RasterPlanarDomainTrack(p).rule + ")";
		}
		return std::string("unlocated");
	};
	struct EnumCount {
		uint32_t blind = 0; ///< The blind enumeration's geometries.
		uint32_t blind_on = 0; ///< Of them on the grid.
		uint32_t built = 0; ///< The by-construction enumeration's geometries.
		uint32_t built_off = 0; ///< Of them the raster calls off grid (a defect of the generator).
	};
	std::map<std::string, EnumCount> enum_counts;

	const std::vector<Points> base_points = f.PointsOf(f.base);
	/** Set a geometry's absorbing pieces from its other pieces; false when one has no length. */
	auto absorb = [&](const Config &cfg, Geometry &g) { return AbsorbGeometry(f, base_points, cfg, g); };
	std::map<std::string, uint32_t> skipped; ///< Per configuration, the blind geometries skipped because an absorbing piece had no length.
	std::map<std::string, uint32_t> disconnected; ///< Per configuration, the geometries skipped because a merge leg does not end at its node (the geometry is not connected).

	/** Judge one geometry into the tallies (the class is cached per polyline). */
	auto judge_one = [&](const Config &cfg, Tally &tally, const Geometry &g, const std::string &varied, const std::vector<Points> &points, bool on_grid) {
		enum { BOTH_ACCEPT, BOTH_REJECT, STRICT, LOOSE, OFF_GRID, OTHER_SHAPE, OFF_INTERFACE, LOOSE_OFF } c;
		std::string pkey;
		const PolyClass *pc = nullptr;
		{
			pkey = polyline_key(g);
			const auto known = classes.find(pkey);
			if (known == classes.end()) {
				PolyClass nc;
				const std::optional<Contracted> cv = ContractedVariant(f, g);
				if (!cv) {
					nc.c = OTHER_SHAPE;
					nc.detail = "      polyline " + pkey + "\n";
				} else {
					std::vector<std::string> notes;
					/* The domain's verdict is of this polyline's own points, on or off the grid (an off-grid polyline is rejected by its raster). */
					const PlanarDomainVerdict d = CheckPlanarDomain(PlanarDomainInputOf(f.v, points, notes));
					const RowsVerdict verdict = judge(f, *cv, true);
					const Readouts ro = g_last_readouts;
					if (on_grid) {
						g_lead_rows_seen += static_cast<uint32_t>(ro.lead.size());
						g_lead_by_family[FamilyOf(cfg.name)] += static_cast<uint32_t>(ro.lead.size());
						g_corridor_rows_seen += corridor_rows(ro);
					}
					const bool accept = verdict.accept;
					/* Off interface: the domain accepts, the full rows reject, and the same read-out without the interface rows (the ports and the box) accepts. */
					const std::vector<std::string> broken = CheckPlanarProgramOnPoints(ro.program.program, cv->points).broken;
					const bool only_port = d.legal && !accept && judge(f, *cv, false).accept;
					if (d.legal && only_port) {
						nc.c = OFF_INTERFACE;
						/* g_last_readouts is still the read-out of the judge without interface rows. */
						if (g_sides_stats != nullptr) {
							g_sides_stats->off_interface++;
							SidesAccumulate(*g_sides_stats, f, pkey, g_last_readouts.keep.keep_apart, cv->points);
						}
						if (g_cells_stats != nullptr) CellsSidesAccumulate(g_cells_stats->c2, f, pkey, *cv, g_last_readouts.keep.keep_apart);
					} else if (d.legal) {
						nc.c = accept ? BOTH_ACCEPT : STRICT;
					} else {
						/* Loose off interface: the domain refuses, the full rows refuse, and the rows without the port and box rows accept (report only). */
						nc.c = accept ? LOOSE : judge(f, *cv, false).accept ? LOOSE_OFF : BOTH_REJECT;
					}
					if (nc.c == BOTH_ACCEPT && g_sides_stats != nullptr) SidesAccumulate(*g_sides_stats, f, pkey, ro.keep.keep_apart, cv->points);
					if (nc.c == BOTH_ACCEPT && g_cells_stats != nullptr) CellsSidesAccumulate(g_cells_stats->c2, f, pkey, *cv, ro.keep.keep_apart);
					if (g_cells_stats != nullptr) CellsAccumulate(*g_cells_stats, f, pkey, *cv, d, nc.c == OFF_INTERFACE, accept, on_grid);
					if (nc.c == STRICT) {
						/* A too-strict polyline is on the grid by definition. */
						for (const Points &p : points) REQUIRE(RasterPlanarDomainTrack(p).rule.empty());
						std::set<std::string> kept;
						nc.detail = "      polyline " + pkey + "\n" + FailingText(f, ro.keep.keep_apart, cv->points, kept);
						for (const PlanarPointRow &r : ro.lead) {
							if (PlanarPointRowHolds(r, cv->points)) continue;
							kept.insert("lead");
							nc.detail += "      " + r.tag + " fails\n";
						}
						for (const std::string &b : CheckPlanarProgramOnPoints(ro.program.program, cv->points).broken) {
							kept.insert(FamilyOf(b));
							nc.detail += "      program row broken: " + b + "\n";
						}
						nc.key = Join(verdict.rules);
							nc.detail += "      rules refused: " + Join(verdict.rules) + "\n";
					} else if (nc.c == OFF_INTERFACE) {
						std::ostringstream out;
						out << "      polyline " << pkey << "\n      contracted pieces (heading x length):";
						for (uint32_t t = 0; t < f.n; t++) {
							out << " " << f.Name(t) << "[";
							for (size_t k = 0; k < cv->v.tracks[t].pieces.size(); k++) {
								const Point &a = cv->points[t][k], &b = cv->points[t][k + 1];
								out << (k == 0 ? "" : ",") << cv->v.tracks[t].pieces[k] << "x" << std::max(std::abs(b[0] - a[0]), std::abs(b[1] - a[1]));
							}
							out << "]";
						}
						out << "\n";
						for (const std::string &b : broken) out << "      port row broken: " << b << "\n";
						nc.detail = out.str();
					} else if (nc.c == LOOSE || nc.c == LOOSE_OFF) {
						std::set<std::string> broke;
						std::ostringstream out;
						out << "      polyline " << pkey << "\n      contracted pieces (heading x length):";
						for (uint32_t t = 0; t < f.n; t++) {
							out << " " << f.Name(t) << "[";
							for (size_t k = 0; k < cv->v.tracks[t].pieces.size(); k++) {
								const Point &a = cv->points[t][k], &b = cv->points[t][k + 1];
								out << (k == 0 ? "" : ",") << cv->v.tracks[t].pieces[k] << "x" << std::max(std::abs(b[0] - a[0]), std::abs(b[1] - a[1]));
							}
							out << "]";
						}
						out << "\n";
						if (!on_grid) {
							const std::string where = first_off_grid(*cv);
							out << "      off grid: first at " << where << "\n";
							broke.insert("off grid: " + where);
						}
						for (const PlanarDomainBreak &b : d.breaks) {
							broke.insert(b.rule);
							std::string names;
							for (const uint32_t t : b.tracks) names += (names.empty() ? "" : ",") + f.Name(t);
							out << "      break " << b.rule << " tracks " << names << " tile (" << b.tile.x << "," << b.tile.y << ") " << b.why << "\n";
							for (const PlanarKeepApartPair &p : ro.keep.keep_apart) {
								const bool pa = std::ranges::count(b.tracks, p.tracks[0]) > 0, pb = std::ranges::count(b.tracks, p.tracks[1]) > 0;
								if (!pa || !pb) continue;
								out << "        " << p.tag << " " << f.Name(p.tracks[0]) << "/" << f.Name(p.tracks[1]) << " pieces " << p.pieces[0] << "/" << p.pieces[1] << ": " << (p.skipped.empty() ? "held" : p.skipped) << "\n";
							}
						}
						nc.detail = out.str();
						nc.key = Join(broke);
					}
				}
				pc = &(classes[pkey] = nc);
			} else {
				pc = &known->second;
			}
			c = static_cast<decltype(c)>(pc->c);
		}
		for (Tally *x : {&tally, &total, &section[IsNeighbourhood(cfg.name) ? 1 : 0]}) {
			x->geoms[c]++;
			if (c == OFF_GRID) {
				x->counts[c]++;
				continue;
			}
			if (c == OTHER_SHAPE || c == OFF_INTERFACE) {
				if (x->seen.insert(pkey).second) {
					x->counts[c]++;
					std::vector<Example> &list = c == OTHER_SHAPE ? x->other : x->off_interface;
					if (list.size() < (c == OTHER_SHAPE ? 3U : 1U)) list.push_back({varied, pc->detail});
				}
				continue;
			}
			Group *grp = c == STRICT ? &x->strict[pc->key] : c == LOOSE ? &x->loose[pc->key] : c == LOOSE_OFF ? &x->loose_off[pc->key] : nullptr;
			if (grp != nullptr) grp->geometries++;
			if (!x->seen.insert(pkey).second) continue;
			x->counts[c]++;
			if (grp != nullptr && grp->count++ < (c == LOOSE_OFF ? 1U : 3U)) grp->examples.push_back({varied, pc->detail});
		}
	};

	/** The on-grid walk of one track (the by-construction mode): from the start, each piece offers its candidate lengths (the axis values when varied, else the base length) and keeps those that end on an edge midpoint (x + y odd) and, for an axis piece with length, on its centre line (east-west on odd y, north-south on odd x). Returns the chosen lengths of the varied pieces. */
	auto walk_track = [&](const Config &cfg, const std::string &name, const std::map<uint32_t, std::vector<int>> &varied) {
		std::set<uint32_t> absorbing;
		for (const auto &[track, piece] : cfg.absorbers) {
			if (track == name) absorbing.insert(piece);
		}
		std::vector<std::map<uint32_t, int>> out;
		const std::vector<int> &h = f.heads.at(name);
		const auto base_it = f.base.find(name);
		std::map<uint32_t, int> chosen;
		Points path;
		auto rec = [&](auto &&self, uint32_t k, Point cur) -> void {
			if (k == h.size()) {
				if (!absorbing.empty() && path.back() != base_points[f.idx.at(name)].back()) return;
				/* The raster's turn rule: no turn over one eighth, no two consecutive pieces on one tile. */
				if (RasterPlanarDomainTrack(path).rule.empty()) out.push_back(chosen);
				return;
			}
			std::vector<int> cands;
			const auto v = varied.find(k);
			if (v != varied.end()) {
				cands = v->second;
			} else if (absorbing.count(k) != 0) {
				/* The absorbing piece offers every length up to the base's plus 32; the end filter below keeps the one that puts the track's end at the base's. */
				const auto bt = f.base.find(name);
				const auto bl = bt == f.base.end() ? std::map<uint32_t, int>::const_iterator() : bt->second.find(k);
				const int top = (bt != f.base.end() && bl != bt->second.end() ? bl->second : 0) + 32;
				for (int c = 0; c <= top; c++) cands.push_back(c);
			} else {
				int len = 0;
				if (base_it != f.base.end()) {
					const auto l = base_it->second.find(k);
					if (l != base_it->second.end()) len = l->second;
				}
				cands = {len};
			}
			const int hh = ((h[k] % 8) + 8) % 8;
			for (const int len : cands) {
				Point next = {cur[0] + len * STEPS[hh][0], cur[1] + len * STEPS[hh][1]};
				if (len > 0) {
					if ((((next[0] + next[1]) % 2) + 2) % 2 != 1) continue;
					if (hh % 2 == 0) {
						const int across = (hh == 2 || hh == 6) ? cur[1] : cur[0];
						if ((across % 2 + 2) % 2 != 1) continue;
					}
				}
				if (v != varied.end() || absorbing.count(k) != 0) chosen[k] = len;
				path.push_back(next);
				self(self, k + 1, next);
				path.pop_back();
			}
			if (v != varied.end() || absorbing.count(k) != 0) chosen.erase(k);
		};
		path.push_back(f.starts.at(name));
		rec(rec, 0, f.starts.at(name));
		return out;
	};

	for (const Config &cfg : configs) {
		const auto c0 = std::chrono::steady_clock::now();
		Tally &tally = per[cfg.name];
		EnumCount &ec = enum_counts[cfg.name];
		/* Blind enumeration: independent length lists; its off-grid geometries are the off-grid part (judged with +grid, else counted). */
		std::vector<size_t> pos(cfg.axes.size(), 0);
		while (true) {
			Geometry g = f.base;
			std::ostringstream varied;
			for (size_t a = 0; a < cfg.axes.size(); a++) {
				const auto &[name, piece, vals] = cfg.axes[a];
				g[name][piece] = vals[pos[a]];
				varied << (a == 0 ? "" : " ") << name << "." << piece << "=" << vals[pos[a]];
			}
			if (!absorb(cfg, g)) {
				skipped[cfg.name]++;
			} else if (bool connected = true; f.StartsOf(g, &connected), !connected) {
				disconnected[cfg.name]++;
			} else {
				const std::vector<Points> points = f.PointsOf(g);
				bool on_grid = true;
				for (const Points &p : points) on_grid = on_grid && RasterPlanarDomainTrack(p).rule.empty();
				ec.blind++;
				if (on_grid) {
					ec.blind_on++;
				} else {
					judge_one(cfg, tally, g, varied.str(), points, false);
				}
			}
			size_t a = 0;
			while (a < cfg.axes.size()) {
				if (++pos[a] < std::get<2>(cfg.axes[a]).size()) break;
				pos[a++] = 0;
			}
			if (a == cfg.axes.size()) break;
		}
		/* By construction: per varied track the on-grid walks, then their product. */
		std::vector<std::string> names;
		std::map<std::string, std::map<uint32_t, std::vector<int>>> varied_of;
		for (const auto &[name, piece, vals] : cfg.axes) {
			if (varied_of.find(name) == varied_of.end()) names.push_back(name);
			varied_of[name][piece] = vals;
		}
		std::vector<std::vector<std::map<uint32_t, int>>> walks;
		bool any_empty = false;
		for (const std::string &name : names) {
			walks.push_back(walk_track(cfg, name, varied_of[name]));
			any_empty = any_empty || walks.back().empty();
		}
		if (!any_empty) {
			std::vector<size_t> wp(names.size(), 0);
			while (true) {
				Geometry g = f.base;
				for (size_t i = 0; i < names.size(); i++) {
					for (const auto &[k, len] : walks[i][wp[i]]) g[names[i]][k] = len;
				}
				std::ostringstream varied;
				for (size_t a = 0; a < cfg.axes.size(); a++) {
					const auto &[name, piece, vals] = cfg.axes[a];
					varied << (a == 0 ? "" : " ") << name << "." << piece << "=" << g[name][piece];
				}
				bool connected = true;
				f.StartsOf(g, &connected);
				if (!connected) {
					disconnected[cfg.name]++;
				} else {
					const std::vector<Points> points = f.PointsOf(g);
					bool on_grid = true;
					for (const Points &p : points) on_grid = on_grid && RasterPlanarDomainTrack(p).rule.empty();
					ec.built++;
					if (!on_grid) ec.built_off++;
					/* A geometry the raster refuses (its turn rules go beyond the walk's grid parity) is in the blind off-grid stream; it is not judged twice. */
					if (on_grid) judge_one(cfg, tally, g, varied.str(), points, true);
				}
				size_t i = 0;
				while (i < names.size()) {
					if (++wp[i] < walks[i].size()) break;
					wp[i++] = 0;
				}
				if (i == names.size()) break;
			}
		}
		cfg_wall[cfg.name] = std::chrono::duration<double>(std::chrono::steady_clock::now() - c0).count();
	}
	const double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

	std::ostringstream out;
	out << "Encoding report on " << title << ": our contracted rows (judged on fixed points, no solve) against the domain, one writing per polyline (D432)\n" << header;
	out << "node lead rows over the judged contracted variants: " << g_lead_rows_seen << "\n";
	out << "corridor rows over the judged on-grid contracted variants: " << g_corridor_rows_seen << "; in the base polyline's read-out: " << g_corridor_rows_base << "\n";
	out << "Slots keeps the flank between two used S-bend inserts (consecutive contracted pieces at most one eighth apart); contracted writings that took a later representation: " << g_turn_moved << ", with none within one eighth (rows reject): " << g_turn_rejected << "; through nodes (F6), rows reject: " << g_node_turn_rejected << "\n";
	for (const Config &cfg : configs) g_lead_by_family.try_emplace(FamilyOf(cfg.name), 0);
	for (const auto &[family, rows] : g_lead_by_family) out << "node lead rows, configuration " << family << ": " << rows << "\n";
	out << "enumeration wall " << wall << " s\n\n";
	auto write = [&](const std::string &title, const Tally &t) {
		out << "== " << title << " ==\n";
		for (int c = 0; c < 8; c++) out << "  " << CLASS_NAMES[c] << ": " << t.counts[c] << (c == 4 ? "" : " polylines") << " (" << t.geoms[c] << " geometries)\n";
		for (const Example &e : t.other) out << "    other shape, varied: " << e.varied << "\n" << e.detail;
		for (const Example &e : t.off_interface) out << "    off interface, varied: " << e.varied << "\n" << e.detail;
		for (int kind = 0; kind < 3; kind++) {
			const auto &groups = kind == 0 ? t.strict : kind == 1 ? t.loose : t.loose_off;
			out << (kind == 0 ? " too strict, by the kept values of the failing pairs:\n" : kind == 1 ? " too loose, by domain break rules:\n" : " loose off interface (report only), by domain break rules:\n");
			for (const auto &[key, grp] : groups) {
				out << "  [" << key << "] polylines " << grp.count << " (geometries " << grp.geometries << ")\n";
				for (const Example &e : grp.examples) out << "    varied: " << e.varied << "\n" << e.detail;
			}
		}
		out << "\n";
	};
	for (const Config &cfg : configs) {
		const EnumCount &ec = enum_counts[cfg.name];
		out << "wall " << cfg.name << ": " << cfg_wall[cfg.name] << " s\n";
		out << "enumeration " << cfg.name << ": blind " << ec.blind << " geometries (" << ec.blind_on << " on the grid, " << ec.blind - ec.blind_on << " off; " << skipped[cfg.name] << " more skipped, an absorbing piece had no length; " << disconnected[cfg.name] << " disconnected, skipped); by construction " << ec.built << " geometries (" << ec.built_off << " of them the raster refuses and are not judged here: its turn rules)\n";
		write(cfg.name, per[cfg.name]);
	}
	write("total hand-named configurations", section[0]);
	write("total neighbourhood configurations", section[1]);
	write("total", total);

	out << "== base (strict rules on): failing pairs ==\n";
	{
		const std::vector<Points> base = f.PointsOf(f.base);
		std::set<std::string> kept;
		out << FailingText(f, f.ReadOut(true).keep_apart, base, kept);
	}
	if (report_path != nullptr) {
		std::ofstream file(report_path);
		REQUIRE(file.good());
		file << out.str();
	}
	if (totals != nullptr) *totals = total.counts;
	for (const Config &cfg : configs) {
		const Tally &t = per[cfg.name];
		WARN(cfg.name << " (polylines): both accept " << t.counts[0] << ", both reject " << t.counts[1] << ", too strict " << t.counts[2] << ", too loose " << t.counts[3] << ", off grid geometries " << t.counts[4] << ", other shape " << t.counts[5] << " (geometries " << t.geoms[5] << "), off interface " << t.counts[6] << " (geometries " << t.geoms[6] << "), loose off interface " << t.counts[7] << " (geometries " << t.geoms[7] << "), wall " << cfg_wall[cfg.name] << " s");
	}
	WARN("enumeration wall " << wall << " s");
}
} // namespace

namespace {

/** The default rule set (D435) with the port rows' branch (D434): the suffixes RunEncoding reads. */
const char *const DEFAULT_RULES = "span+lead+endnode+crosspiece+grid+portzero";

/** What the judge and the domain say of one polyline: rows is 0 accept, 1 reject, 2 other shape (no contracted writing); rules are the judge's refusing rules. */
struct Verdicts {
	int rows = 1;
	bool domain = false;
	std::set<std::string> rules;
};

/** The verdicts on a geometry of a fixture under a judge. */
Verdicts VerdictsOf(const X1Fixture &f, const Geometry &g, const RowsJudge &judge)
{
	Verdicts r;
	const std::optional<Contracted> cv = ContractedVariant(f, g);
	if (!cv) {
		r.rows = 2;
	} else {
		const RowsVerdict v = judge(f, *cv, true);
		r.rows = v.accept ? 0 : 1;
		r.rules = v.rules;
	}
	std::vector<std::string> notes;
	r.domain = CheckPlanarDomain(PlanarDomainInputOf(f.v, f.PointsOf(g), notes)).legal;
	return r;
}

} // namespace

namespace {

/**
 * The rows of a polyline's whole program (strict rules off, grid on) that its contracted points break, the D412 port lines and the node-to-node tracks without an event taking their zero-or-ordinary branch from the polyline's own travel (the port_zero_from and node_gap_from of a reference drawing that is this polyline, or `reference_travel` for one track). `port_zero` off leaves every in-port line free (D412), `node_gaps` off drops the node gap rows.
 */
std::vector<std::string> BrokenOnOwnBranches(const X1Fixture &f, const Geometry &g, bool port_zero, bool node_gaps, const std::pair<std::string, double> *reference_travel = nullptr)
{
	const std::optional<Contracted> cv = ContractedVariant(f, g);
	REQUIRE(cv.has_value());
	PlanarShapeVariant plain = cv->v;
	for (PlanarShapeTrack &track : plain.tracks) track.steps.clear();
	std::vector<std::vector<double>> from = PortZeroFrom(cv->points);
	if (reference_travel != nullptr) from[f.idx.at(reference_travel->first)] = {reference_travel->second};
	PlanarDrawOptions o = f.options;
	o.strict_span = o.strict_zero = o.crossing_flanks = false;
	o.keep_apart_out = true;
	o.program_out = true;
	o.build_only = true;
	o.grid = true;
	o.port_zero = port_zero;
	o.port_zero_from = &from;
	o.node_gaps = node_gaps;
	o.node_gap_from = &from;
	const PlanarDrawResult r = CheckPlanarDrawable(plain, 5.0, o);
	REQUIRE(r.status == "built");
	return CheckPlanarProgramOnPoints(r.program, cv->points).broken;
}

} // namespace

TEST_CASE("planar domain: the lead rule binds on the sibling's heading (F1)", "[junction][planar][planardomain]")
{
	/* M1's too-strict example: A1 arrives at the merge on an S-bend toward its sibling with no flank, but its heading differs from B1's there (a Y): the lead rule does not refuse it. */
	const M1Fixture f;
	Geometry g = f.base;
	g["A1"] = {{0, 6}, {1, 2}, {2, 2}, {3, 2}, {4, 0}};
	const Verdicts v = VerdictsOf(f, g, DrawingProgramJudge(DEFAULT_RULES));
	REQUIRE(v.rows != 2);
	INFO("refused by " << Join(v.rules));
	CHECK_FALSE(v.rules.contains("lead"));
}

TEST_CASE("planar domain: a contracted line turns one eighth at a time (F3)", "[junction][planar][planardomain]")
{
	/* M1's turn example: NE then SE with the flank between them removed at length 0: the rows must not accept it. */
	const M1Fixture f;
	Geometry g = f.base;
	g["A1"] = {{0, 10}, {1, 1}, {2, 0}, {3, 1}, {4, 0}};
	const Verdicts v = VerdictsOf(f, g, DrawingProgramJudge(DEFAULT_RULES));
	REQUIRE(v.rows != 2);
	CHECK(v.rows == 1);
	CHECK(v.rules.contains("turn"));
	const Config c{"turn", {{"A1", 0, {10}}, {"A1", 1, {1}}, {"A1", 2, {0}}, {"A1", 3, {1}}, {"A1", 4, {0}}}};
	std::filesystem::path path = std::filesystem::temp_directory_path() / "planar_domain_turn.txt";
	RunEncoding(f, path.string().c_str(), "M1 turn", {c}, "", DrawingProgramJudge("span+lead+endnode+crosspiece+grid"));
	std::ifstream in(path);
	const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	std::filesystem::remove(path);
	CHECK(text.find("  too loose: 0 polylines") != std::string::npos);
}

TEST_CASE("planar domain: a node-to-out-port line keeps its least lengths (F2)", "[junction][planar][planardomain]")
{
	/* X1's first F2 example: C1 = one 2-long east run (heading 0), A1 and the rest as base. C1 leaves a node for an out port, so D412 does not free its pieces: its run is on the grid, and its off-centre-line points are refused. */
	const X1Fixture f;
	Geometry g = f.base;
	g["C1"] = {{9, 2}};
	const Verdicts v = VerdictsOf(f, g, DrawingProgramJudge(DEFAULT_RULES));
	INFO("refused by " << Join(v.rules));
	CHECK(v.rows == 1);
	CHECK_FALSE(v.domain);
	CHECK(v.rules.contains("grid"));
}


TEST_CASE("planar domain: a D412 port line off its centre line is rejected under port_zero (F5, D434)", "[junction][planar][planardomain]")
{
	/* F5: X1's in-port line AC1 = diagonal 1, east 2, diagonal 1 (its east run off the centre line). D412 frees every piece of it, so without port_zero the program's rows accept it (see the drawing program's own test); under port_zero the polyline's own travel (4) makes AC1 an ordinary line and its run is refused, as the domain refuses it. */
	const X1Fixture f;
	Geometry g = f.base;
	g["AC1"] = {{1, 1}, {2, 2}, {3, 1}};
	const Verdicts with = VerdictsOf(f, g, DrawingProgramJudge(DEFAULT_RULES));
	INFO("refused by " << Join(with.rules));
	CHECK(with.rows == 1);
	CHECK_FALSE(with.domain);
	CHECK(with.rules.contains("grid"));
}


TEST_CASE("planar domain: a port row broken under grid is blamed on the port row, not the grid link (domain-27)", "[junction][planar][planardomain]")
{
	/* K1's first [grid] too-strict example: A1:1*1,0*6,;ABC1:2*4,;B1:2*12,;BC1:2*4,;C1:3*1,4*6,; (A1 ends short of the box edge, so it is off interface). */
	const K1Fixture f;
	Geometry g = f.base;
	g["A1"] = {{0, 1}, {9, 6}};
	g["C1"] = {{0, 1}, {9, 6}};
	const std::optional<Contracted> cv = ContractedVariant(f, g);
	REQUIRE(cv.has_value());
	const RowsJudge judge = DrawingProgramJudge(DEFAULT_RULES);
	const RowsVerdict full = judge(f, *cv, true);
	INFO("refused by " << Join(full.rules));
	CHECK_FALSE(full.accept);
	CHECK(full.rules.contains("port"));
	CHECK_FALSE(full.rules.contains("grid"));
	CHECK(judge(f, *cv, false).accept);
}


TEST_CASE("planar domain: the grid rows reject an off-grid drawing", "[junction][planar][planardomain]")
{
	/* X1's base shifted by one half tile in x on every track: every corner is off the grid; the grid rule refuses it and the same rules without the grid accept it. */
	X1Fixture f;
	for (auto &[name, start] : f.starts) start[0] += 1;
	f.options.skip_rows = {"port "}; /* The shift also moves the ports off their box edges; this test judges the grid rows only. */
	const std::vector<Points> points = f.PointsOf(f.base);
	bool all_off = true;
	for (const Points &p : points) all_off = all_off && !RasterPlanarDomainTrack(p).rule.empty();
	REQUIRE(all_off);
	const std::optional<Contracted> cv = ContractedVariant(f, f.base);
	REQUIRE(cv.has_value());
	const RowsVerdict on = DrawingProgramJudge("span+lead+endnode+crosspiece+grid")(f, *cv, true);
	INFO("refused by " << Join(on.rules));
	CHECK_FALSE(on.accept);
	CHECK(on.rules.contains("grid"));
	CHECK(DrawingProgramJudge("span+lead+endnode+crosspiece")(f, *cv, true).accept);
}


TEST_CASE("planar domain: grid points lie on edge midpoints (F4)", "[junction][planar][planardomain]")
{
	/* X1's base with every point of every track shifted by (1, 0): every point's x + y parity flips, so no point is on an edge midpoint (F4, D428): the domain and the grid rule refuse it. */
	X1Fixture f;
	for (auto &[name, start] : f.starts) start[0] += 1;
	const Verdicts v = VerdictsOf(f, f.base, DrawingProgramJudge(DEFAULT_RULES));
	INFO("refused by " << Join(v.rules));
	CHECK(v.rows == 1);
	CHECK_FALSE(v.domain);
	CHECK(v.rules.contains("grid"));
}

TEST_CASE("planar domain: a too-strict polyline is on the grid", "[junction][planar][planardomain]")
{
	/* X1's B1 run north on x = 10 (0*2,1*2,0*2,7*3,0*8 from (11,10)): the raster calls it off the grid, so under +grid the domain rejects it and it is never too strict. */
	const X1Fixture f;
	Geometry g = f.base;
	g["B1"] = {{0, 2}, {3, 2}, {4, 2}, {5, 3}, {8, 8}};
	bool off = false;
	for (const Points &p : f.PointsOf(g)) off = off || !RasterPlanarDomainTrack(p).rule.empty();
	REQUIRE(off);
	const Config c{"one", {{"B1", 0, {2}}, {"B1", 3, {2}}, {"B1", 4, {2}}, {"B1", 5, {3}}, {"B1", 8, {8}}}};
	const std::string path = (std::filesystem::temp_directory_path() / "planar_domain_grid_strict.txt").string();
	RunEncoding(f, path.c_str(), "X1 one", {c}, "", DrawingProgramJudge("span+lead+endnode+crosspiece+grid"));
	std::ifstream in(path);
	const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	std::filesystem::remove(path);
	CHECK(text.find("  too strict: 0 polylines") != std::string::npos);
	CHECK(text.find("  both reject: 1 polylines") != std::string::npos);
}

namespace {

std::vector<Config> X1Configs(const X1Fixture &f)
{
	std::vector<Config> configs = {
		{"S1", {{"B1", 2, {0, 2, 4}}, {"B1", 3, {0, 1, 2, 3, 4}}, {"B1", 4, {2, 4, 6, 8, 10, 12, 14}}, {"B1", 5, {0, 1, 2, 3, 4}}, {"B1", 6, {4}}}},
		{"S2", {{"A1", 2, {0, 2, 4}}, {"A1", 3, {0, 1, 2, 3, 4}}, {"A1", 4, {2, 4, 6, 8, 10, 12, 14, 16}}, {"A1", 5, {0, 1, 2, 3, 4}}, {"A1", 6, {4}}}},
		{"N1", {{"C1", 0, {0, 1, 2, 3}}, {"C1", 1, {0, 2, 4}}, {"C1", 2, {0, 1, 2, 3}}, {"A1", 0, {0, 2, 4}}, {"A1", 1, {0, 1, 2, 3}}}},
	};
	for (const Config &c : NeighbourhoodConfigs(f, 3)) configs.push_back(c);
	return configs;
}

} // namespace

TEST_CASE("planar domain: our keep-apart against the domain on X1", "[.][planardomainenc]")
{
	const char *report_path = std::getenv("PLANAR_DOMAIN_REPORT");
	REQUIRE(report_path != nullptr);
	const X1Fixture f;
	const std::vector<Config> configs = X1Configs(f);
	RunEncoding(f, report_path, "X1", configs, "", DrawingProgramJudge(""));
}

TEST_CASE("planar domain: X1 is exact under the default rule set", "[junction][planar][planardomainexact]")
{
	const X1Fixture f;
	std::array<uint32_t, 8> totals{};
	RunEncoding(f, nullptr, "X1", X1Configs(f), "", DrawingProgramJudge(DEFAULT_RULES), nullptr, &totals);
	INFO("X1: too strict " << totals[2] << ", too loose " << totals[3]);
	CHECK(totals[2] == 0);
	CHECK(totals[3] == 0);
}

TEST_CASE("planar domain: keep-apart separates a face's wall segments, cut at crossings (D431)", "[junction][planar][planardomain]")
{
	/* The sides-2 hand check: X1's polyline A1 3*2,2*8,1*2,2*4 with B1 0*10. Face 4's walls A1 piece 1 and B1 piece 0 are cut at the crossing (11,7); a wall ends there, not at the piece's far end (11,0). */
	const X1Fixture f;
	Geometry g = f.base;
	g["A1"] = {{3, 2}, {4, 8}, {5, 2}, {6, 4}};
	const Verdicts v = VerdictsOf(f, g, DrawingProgramJudge(DEFAULT_RULES));
	REQUIRE(v.rows != 2);
	INFO("refused by " << Join(v.rules));
	CHECK(v.domain);
	CHECK(v.rows == 0);
	const std::optional<Contracted> cv = ContractedVariant(f, g);
	REQUIRE(cv.has_value());
	bool found = false;
	uint32_t cut_rows = 0;
	for (const PlanarKeepApartPair &pair : g_last_readouts.keep.keep_apart) {
		CHECK(PlanarKeepApartHolds(pair, cv->points));
		for (const PlanarKeepApartPair::Candidate &c : pair.candidates) {
			for (const PlanarKeepApartPair::Row &r : c.rows) cut_rows += r.p_cross[0] != UINT32_MAX || r.q_cross[0] != UINT32_MAX;
		}
		if (pair.tag != "sep 4 3 6") continue;
		found = true;
		REQUIRE(pair.walk_strict >= 0);
		/* The side the strict walk forces holds on the cut walls. */
		CHECK(CandidateMargin(pair.candidates[pair.walk_strict], cv->points) >= 0);
	}
	CHECK(found);
	CHECK(cut_rows > 0);
}

TEST_CASE("planar domain: X2 loads and its base is legal", "[junction][planar][planardomain]")
{
	const X2Fixture f;
	INFO(f.log);
	RequireBaseLegal(f);
	const std::vector<Points> base = f.PointsOf(f.base);

	/* The base polyline, judged as the report judges it with the rule set span+lead+endnode+crosspiece: its one contracted writing passes. */
	int verdict = 1;
	const Config none{"base", {}};
	RunEncoding(f, nullptr, "X2", {none}, "", DrawingProgramJudge("span+lead+endnode+crosspiece"), &verdict);
	CHECK(verdict == 0);
}

TEST_CASE("planar domain: X2's crosser bending at its crossing is accepted under the default rules (D414, D435)", "[junction][planar][planardomain]")
{
	const X2Fixture f;
	INFO(f.log);
	RequireBaseLegal(f);
	const Verdicts v = VerdictsOf(f, f.base, DrawingProgramJudge(DEFAULT_RULES));
	INFO("refused by " << Join(v.rules));
	CHECK(v.rows == 0);
	CHECK(v.domain);
}

namespace {

std::vector<Config> X2Configs(const X2Fixture &f)
{
	const std::vector<int> &b1 = f.heads.at("B1");
	const uint32_t last = static_cast<uint32_t>(b1.size()) - 1;
	/* C1: B1's event piece length, each piece within two of it (not the last), the last piece fixed at 4. */
	Config c1{"X2 C1", {{"B1", f.b1_event, {1, 2, 3, 4, 5, 6}}}};
	for (uint32_t k = f.b1_event >= 2 ? f.b1_event - 2 : 0; k <= f.b1_event + 2 && k < last; k++) {
		if (k == f.b1_event) continue;
		const bool axis = ((b1[k] % 8) + 8) % 8 % 2 == 0;
		c1.axes.emplace_back("B1", k, axis ? std::vector<int>{0, 2, 4} : std::vector<int>{0, 1, 2, 3, 4});
	}
	c1.axes.emplace_back("B1", last, std::vector<int>{4});
	/* C2: A1's event piece length, A1's pieces before it in {0, 2, 4}. */
	Config c2{"X2 C2", {}};
	for (uint32_t k = 0; k < f.a1_event; k++) c2.axes.emplace_back("A1", k, std::vector<int>{0, 2, 4});
	c2.axes.emplace_back("A1", f.a1_event, std::vector<int>{8, 10, 12, 14});
	std::vector<Config> configs = {c1, c2};
	for (const Config &c : NeighbourhoodConfigs(f, 3)) configs.push_back(c);
	return configs;
}

} // namespace

TEST_CASE("planar domain: our keep-apart against the domain on X2", "[.][planardomainenc]")
{
	const char *report_path = std::getenv("PLANAR_DOMAIN_REPORT_X2");
	REQUIRE(report_path != nullptr);
	const X2Fixture f;
	RequireBaseLegal(f);
	const std::vector<int> &b1 = f.heads.at("B1");
	const uint32_t last = static_cast<uint32_t>(b1.size()) - 1;
	std::ostringstream header;
	header << "B1 shape text: " << f.shape << "\n" << (f.log.empty() ? "" : "refused first:\n" + f.log);
	header << "B1 headings after flex:";
	for (const int h : b1) header << " " << h;
	header << "\nB1 flex middles:";
	for (const uint32_t m : f.v.tracks[f.idx.at("B1")].flex) header << " " << m;
	header << "\nB1 event piece after flex: " << f.b1_event << "\nA1 event piece after flex: " << f.a1_event << "\nB1's last piece (" << last << ") is fixed at 4\n";

	const std::vector<Config> configs = X2Configs(f);
	RunEncoding(f, report_path, "X2", configs, header.str(), DrawingProgramJudge(""));
}

TEST_CASE("planar domain: X2 is exact under the default rule set", "[junction][planar][planardomainexact]")
{
	const X2Fixture f;
	RequireBaseLegal(f);
	std::array<uint32_t, 8> totals{};
	RunEncoding(f, nullptr, "X2", X2Configs(f), "", DrawingProgramJudge(DEFAULT_RULES), nullptr, &totals);
	INFO("X2: too strict " << totals[2] << ", too loose " << totals[3]);
	CHECK(totals[2] == 0);
	CHECK(totals[3] == 0);
}

TEST_CASE("planar domain: a line may run along a tunnel over its middles (D438, F9)", "[junction][planar][planardomain]")
{
	/* X2's B1 crosses A1's tunnel on its diagonal (D414: the crossing piece ends on A1's line), then runs east ALONG the tunnel over a middle tile (E2) and leaves it on a diagonal at a middle tile: the judge and the domain accept. The same family with E4 (B1's diagonal then starts at the portal tile) is refused by both. A plain long diagonal crossing (not in along mode) is accepted by both. The polylines move B1's far end, so the interface rows (the ports and the box) are off: they are not what the test is about. */
	const X2Fixture f;
	const RowsJudge judge = DrawingProgramJudge(DEFAULT_RULES);
	const auto with = [&](const Runs &runs) {
		const std::vector<Lengths> reps = Representations(f.heads.at("B1"), runs, 50, nullptr, &f.v.tracks[f.idx.at("B1")].flex);
		REQUIRE_FALSE(reps.empty());
		Geometry g = f.base;
		g["B1"].clear();
		for (size_t k = 0; k < reps[0].size(); k++) {
			if (reps[0][k] != 0) g["B1"][static_cast<uint32_t>(k)] = reps[0][k];
		}
		Verdicts r;
		std::vector<std::string> notes;
		const std::optional<Contracted> cv = ContractedVariant(f, g);
		REQUIRE(cv.has_value());
		const RowsVerdict v = judge(f, *cv, false);
		r.rows = v.accept ? 0 : 1;
		r.rules = v.rules;
		r.domain = CheckPlanarDomain(PlanarDomainInputOf(f.v, f.PointsOf(g), notes)).legal;
		return r;
	};
	const Verdicts along = with({{0, 2}, {1, 3}, {2, 2}, {1, 5}, {0, 4}});
	INFO("refused by " << Join(along.rules));
	CHECK(along.rows == 0);
	CHECK(along.domain);
	const Verdicts portal = with({{0, 2}, {1, 3}, {2, 4}, {1, 1}, {0, 4}});
	INFO("refused by " << Join(portal.rules));
	CHECK(portal.rows == 1);
	CHECK(portal.rules.contains("along"));
	CHECK_FALSE(portal.domain);
	const Verdicts plain = with({{0, 2}, {1, 6}, {0, 4}});
	INFO("refused by " << Join(plain.rules));
	CHECK(plain.rows == 0);
	CHECK(plain.domain);
}

namespace {

/** Axes for a track's pieces in [lo, hi]: a run holder takes the values listed for it (none: left at base), every other piece is an insert, {0, 2} on an axis heading and {0, 1, 2} otherwise. */
void AddAxes(Config &c, const X1Fixture &f, const std::string &name, const std::vector<uint32_t> &holders, const std::vector<std::vector<int>> &holder_values, uint32_t lo, uint32_t hi)
{
	const std::vector<int> &h = f.heads.at(name);
	for (uint32_t k = 0; k < h.size(); k++) {
		const auto it = std::ranges::find(holders, k);
		if (it != holders.end()) {
			const std::vector<int> &vals = holder_values[static_cast<size_t>(it - holders.begin())];
			if (!vals.empty()) c.axes.emplace_back(name, k, vals);
			continue;
		}
		if (k < lo || k > hi) continue;
		const bool axis = ((h[k] % 8) + 8) % 8 % 2 == 0;
		c.axes.emplace_back(name, k, axis ? std::vector<int>{0, 2} : std::vector<int>{0, 1, 2});
	}
}

/** The report header: per track its headings after flex, flex middles and run holders. */
std::string FixtureHeader(const X1Fixture &f, const std::map<std::string, std::vector<uint32_t>> &holders)
{
	std::ostringstream out;
	for (const auto &[name, h] : f.heads) {
		out << name << " headings after flex:";
		for (const int x : h) out << " " << x;
		out << "; flex middles:";
		for (const uint32_t m : f.v.tracks[f.idx.at(name)].flex) out << " " << m;
		out << "; run holders:";
		for (const uint32_t k : holders.at(name)) out << " " << k;
		out << "\n";
	}
	return out.str();
}

} // namespace

TEST_CASE("planar domain: M1 loads and its base is legal", "[junction][planar][planardomain]")
{
	const M1Fixture f;
	RequireBaseLegal(f);
	int verdict = 1;
	const Config none{"base", {}};
	RunEncoding(f, nullptr, "M1", {none}, "", DrawingProgramJudge("span+lead+endnode+crosspiece"), &verdict);
	CHECK(verdict == 0);
}

TEST_CASE("planar domain: K1 loads and its base is legal", "[junction][planar][planardomain]")
{
	const K1Fixture f;
	RequireBaseLegal(f);
	int verdict = 1;
	const Config none{"base", {}};
	RunEncoding(f, nullptr, "K1", {none}, "", DrawingProgramJudge("span+lead+endnode+crosspiece"), &verdict);
	CHECK(verdict == 0);
}

namespace {

std::vector<Config> M1Configs(const M1Fixture &f)
{
	/* M1e (A1) and M1f (B1): S-bend pairs just before the merge, one configuration per candidate; the first piece (A1) or the earlier NE flank (B1) absorbs the rest so the track still ends at the merge point (16,5). Candidates that are off the grid are kept: the on-grid generator and the off-grid judging sort them. */
	std::vector<Config> configs;
	const std::vector<int> &ah = f.heads.at("A1"), &bh = f.heads.at("B1");
	{
		std::ostringstream hs;
		for (const int x : ah) hs << x << " ";
		hs << "| ";
		for (const int x : bh) hs << x << " ";
		INFO("A1 | B1 headings: " << hs.str());
		REQUIRE(ah == std::vector<int>({2, 1, 2, 3, 2}));
		REQUIRE(bh.size() == 10);
		REQUIRE((bh[5] == 1 && bh[6] == 0 && bh[7] == 1 && bh[8] == 2 && bh[9] == 1));
	}
	const auto add = [&](const std::string &track, const std::map<uint32_t, int> &lengths, const std::string &label) {
		Geometry g = f.base;
		g[track] = lengths;
		const std::vector<Points> q = f.PointsOf(g);
		REQUIRE(q[f.idx.at(track)].back() == Point({16, 5}));
		Config c{label, {}};
		for (const auto &[k, len] : lengths) c.axes.emplace_back(track, k, std::vector<int>{len});
		configs.push_back(c);
	};
	for (const int d : {1, 2}) {
		for (const int p2 : {0, 2, 4}) {
			for (const int p4 : {0, 2, 4}) {
				const int rest = 12 - 2 * d - p2 - p4;
				if (rest < 0) continue;
				std::ostringstream cname;
				cname << "M1e A1 d" << d << " p2=" << p2 << " p4=" << p4;
				add("A1", {{0, rest}, {1, d}, {2, p2}, {3, d}, {4, p4}}, cname.str());
			}
		}
	}
	for (const int d : {2, 4}) {
		for (const int p7 : {0, 1, 2, 3}) {
			for (const int p9 : {0, 1, 2, 3}) {
				const int n5 = 3 - d - p7 - p9;
				if (n5 < 0) continue;
				std::ostringstream cname;
				cname << "M1f B1 d" << d << " p7=" << p7 << " p9=" << p9;
				add("B1", {{0, 4}, {5, n5}, {6, d}, {7, p7}, {8, d}, {9, p9}}, cname.str());
			}
		}
	}
	for (const Config &c : NeighbourhoodConfigs(f, 3)) configs.push_back(c);
	return configs;
}

} // namespace

TEST_CASE("planar domain: our keep-apart against the domain on M1", "[.][planardomainenc]")
{
	const char *report_path = std::getenv("PLANAR_DOMAIN_REPORT_M1");
	REQUIRE(report_path != nullptr);
	const M1Fixture f;
	RequireBaseLegal(f);
	const std::vector<Config> configs = M1Configs(f);
	RunEncoding(f, report_path, "M1", configs, FixtureHeader(f, f.holders), DrawingProgramJudge(""));
}

TEST_CASE("planar domain: M1 is exact under the default rule set", "[junction][planar][planardomainexact]")
{
	const M1Fixture f;
	RequireBaseLegal(f);
	std::array<uint32_t, 8> totals{};
	RunEncoding(f, nullptr, "M1", M1Configs(f), "", DrawingProgramJudge(DEFAULT_RULES), nullptr, &totals);
	INFO("M1: too strict " << totals[2] << ", too loose " << totals[3]);
	CHECK(totals[2] == 0);
	CHECK(totals[3] == 0);
}

namespace {

std::vector<Config> K1Configs(const K1Fixture &f)
{
	/* K1a: BC1's total length in {0, 2, 4, 6} (0 is the trident); A1's NE run and C1's SE run in 1..3; the inserts of A1 and C1 within their first three pieces. node_gaps is on (D424). */
	Config c{"K1a", {}};
	c.axes.emplace_back("BC1", f.holders.at("BC1")[0], std::vector<int>{0, 2, 4, 6});
	AddAxes(c, f, "A1", f.holders.at("A1"), {{1, 2, 3}, {}}, 0, 2);
	AddAxes(c, f, "C1", f.holders.at("C1"), {{1, 2, 3}, {}}, 0, 2);
	REQUIRE(f.options.node_gaps);
	std::vector<Config> configs = {c};
	for (const Config &n : NeighbourhoodConfigs(f, 3)) configs.push_back(n);
	return configs;
}

} // namespace

TEST_CASE("planar domain: our keep-apart against the domain on K1", "[.][planardomainenc]")
{
	const char *report_path = std::getenv("PLANAR_DOMAIN_REPORT_K1");
	REQUIRE(report_path != nullptr);
	const K1Fixture f;
	RequireBaseLegal(f);
	const std::vector<Config> configs = K1Configs(f);
	RunEncoding(f, report_path, "K1", configs, "node_gaps: on (D424)\n" + FixtureHeader(f, f.holders), DrawingProgramJudge(""));
}

TEST_CASE("planar domain: K1 is exact under the default rule set", "[junction][planar][planardomainexact]")
{
	const K1Fixture f;
	RequireBaseLegal(f);
	std::array<uint32_t, 8> totals{};
	RunEncoding(f, nullptr, "K1", K1Configs(f), "", DrawingProgramJudge(DEFAULT_RULES), nullptr, &totals);
	INFO("K1: too strict " << totals[2] << ", too loose " << totals[3]);
	CHECK(totals[2] == 0);
	CHECK(totals[3] == 0);
}

TEST_CASE("planar domain: a connected trident is accepted (D433, F7)", "[junction][planar][planardomain]")
{
	/* F7: K1 with BC1 = 0 and B1, C1 starting at the split node (4, 9): B1 runs east 16, C1 SE 7 then S 2. The domain accepts it and so do the rows: the judge gives the program the polyline's own travel as node_gap_from, so BC1 takes the zero branch (leg zero) and its pieces are free (D433). The base is this trident, so the node offsets are 0. On the tile grid no axis node-to-node track lies strictly between 0 and node_leg, so the `node gap` refusal is unreachable there for axis legs. */
	K1Fixture f;
	f.starts["B1"] = {4, 9};
	f.starts["C1"] = {4, 9};
	f.base["BC1"] = {};
	Geometry g = f.base;
	g["B1"] = {{4, 16}};
	g["C1"] = {{0, 7}, {9, 2}};
	const Verdicts v = VerdictsOf(f, g, DrawingProgramJudge(DEFAULT_RULES));
	INFO("refused by " << Join(v.rules));
	REQUIRE(v.rows != 2);
	CHECK(v.domain);
	CHECK(v.rows == 0);
}

TEST_CASE("planar domain: walls across a zero trident are not kept apart (D433, F7)", "[junction][planar][planardomain]")
{
	/* A zero track's walls are points (D433, D434): BC1 = 0 sits between A1's and C1's walls in the face, and keep-apart neither separates it nor treats the walls either side of it as apart. The domain-35 first example: ABC1 and C1 lie across the trident from each other. */
	K1Fixture f;
	f.starts["B1"] = {4, 9};
	f.starts["C1"] = {4, 9};
	f.base["BC1"] = {};
	Geometry g = f.base;
	g["B1"] = {{4, 16}};
	g["A1"] = {{0, 1}, {1, 0}, {2, 2}, {9, 6}};
	g["C1"] = {{0, 1}, {1, 0}, {2, 0}, {9, 6}};
	const Verdicts v = VerdictsOf(f, g, DrawingProgramJudge(DEFAULT_RULES));
	INFO("refused by " << Join(v.rules));
	REQUIRE(v.rows != 2);
	CHECK(v.domain);
	CHECK(v.rows == 0);
}

TEST_CASE("planar domain: the smallest non-zero trident gap is one tile (D424, F7)", "[junction][planar][planardomain]")
{
	/* BC1 = 2 half tiles (node_leg): B1 and C1 start at its end (6, 9), connected. */
	K1Fixture f;
	f.starts["B1"] = {6, 9};
	f.starts["C1"] = {6, 9};
	f.base["BC1"] = {{0, 2}};
	Geometry g = f.base;
	g["B1"] = {{4, 16}};
	g["C1"] = {{0, 7}, {9, 2}};
	const Verdicts v = VerdictsOf(f, g, DrawingProgramJudge(DEFAULT_RULES));
	INFO("refused by " << Join(v.rules));
	REQUIRE(v.rows != 2);
	CHECK(v.domain);
	CHECK(v.rows == 0);
}

namespace {

/** L1: two lanes from W, the left lanes turning north across the right lanes' straight run (domain-19). */
struct L1Fixture : DrawnFixture {
	L1Fixture() : DrawnFixture(
		"topology L1\nport W in E: AB1, AB2\nport N out N: A1, A2\nport E out E: B1, B2\nnode AB1 -> A1, B1\nnode AB2 -> A2, B2\ncross B1: A2<\ncross A2: B1>\n",
		"tunnel T1 B1: A2\nstraight AB1: B1\nstraight AB2: B2\n")
	{
		REQUIRE(n == 6);
		REQUIRE(heads.at("A1") == std::vector<int>({1, 0, 1, 2, 1, 0, -1, 0, 1, 0}));
		REQUIRE(heads.at("A2") == std::vector<int>({1, 0, 1, 2, 1, 0, -1, 0, 1, 0, -1, 0, 1, 0}));
		REQUIRE(v.tracks[idx.at("A2")].event_piece.size() == 1);
	}

	/** The header: Shape's headings, the base drawing, the base polyline's runs. */
	std::string Header() const
	{
		std::ostringstream out;
		out << "Shape's headings (the heading solve of ShapePlanarStructureVariant):\n" << shape_log << "base drawing (the flex-stripped variant, CheckPlanarDrawable 15 s, grid, tidy, integer, corridor, lead rows): " << draw_log << "\n";
		const std::vector<Points> base_points = PointsOf(base);
		for (uint32_t t = 0; t < n; t++) {
			out << "base " << Name(t) << ": start (" << base_points[t].front()[0] << "," << base_points[t].front()[1] << ") lengths";
			for (uint32_t k = 0; k < heads.at(Name(t)).size(); k++) {
				const auto it = base.find(Name(t));
				const auto l = it == base.end() ? std::map<uint32_t, int>::const_iterator() : it->second.find(k);
				out << " " << (it != base.end() && l != it->second.end() ? l->second : 0);
			}
			out << "\n";
		}
		return out.str();
	}
};

} // namespace

TEST_CASE("planar domain: L1 loads and its base is legal", "[junction][planar][planardomain]")
{
	const L1Fixture f;
	INFO(f.shape_log << f.draw_log);
	RequireBaseLegal(f);
	int verdict = 1;
	const Config none{"base", {}, {}};
	RunEncoding(f, nullptr, "L1", {none}, "", DrawingProgramJudge("span+lead+endnode+crosspiece+grid"), &verdict);
	CHECK(verdict == 0);
}

namespace {

std::vector<Config> L1Configs(const L1Fixture &f)
{
	/* An insert takes {0, 1, 2}, an axis heading's {0, 2}. */
	const auto insert_values = [](int heading) { return (((heading % 8) + 8) % 8) % 2 == 0 ? std::vector<int>{0, 2} : std::vector<int>{0, 1, 2}; };
	/* L1a: every flex insert of A1 and A2; the piece holding the last run's base length (the run into the N port) absorbs, so the port end stays where the base has it. */
	Config a{"L1a", {}, {}};
	for (const std::string name : {"A1", "A2"}) {
		const uint32_t t = f.idx.at(name);
		for (const uint32_t m : f.v.tracks[t].flex) a.axes.emplace_back(name, m, insert_values(f.heads.at(name)[m]));
		/* The last piece holding a base length (the N run's: Shape's flank pieces are 0 in the base). */
		uint32_t holder = 0;
		for (const auto &[k, len] : f.base.at(name)) holder = std::max(holder, k);
		a.absorbers.emplace_back(name, holder);
	}
	/* L1b: A2's pieces within two of its crossing piece in {0..4}, an axis heading's even; B1 at base. */
	Config b{"L1b", {}, {}};
	{
		const std::vector<int> &h = f.heads.at("A2");
		const uint32_t at = f.v.tracks[f.idx.at("A2")].event_piece[0];
		for (uint32_t k = at >= 2 ? at - 2 : 0; k <= at + 2 && k < h.size(); k++) {
			b.axes.emplace_back("A2", k, (((h[k] % 8) + 8) % 8) % 2 == 0 ? std::vector<int>{0, 2, 4} : std::vector<int>{0, 1, 2, 3, 4});
		}
	}
	std::vector<Config> configs = {a, b};
	for (const Config &c : NeighbourhoodConfigs(f, 3)) configs.push_back(c);
	return configs;
}

} // namespace

TEST_CASE("planar domain: our keep-apart against the domain on L1", "[.][planardomainenc]")
{
	const char *report_path = std::getenv("PLANAR_DOMAIN_REPORT_L1");
	REQUIRE(report_path != nullptr);
	const L1Fixture f;
	RequireBaseLegal(f);
	const std::vector<Config> configs = L1Configs(f);
	RunEncoding(f, report_path, "L1", configs, f.Header(), DrawingProgramJudge(""));
}

TEST_CASE("planar domain: L1 is exact under the default rule set", "[junction][planar][planardomainexact]")
{
	const L1Fixture f;
	RequireBaseLegal(f);
	std::array<uint32_t, 8> totals{};
	RunEncoding(f, nullptr, "L1", L1Configs(f), "", DrawingProgramJudge(DEFAULT_RULES), nullptr, &totals);
	INFO("L1: too strict " << totals[2] << ", too loose " << totals[3]);
	CHECK(totals[2] == 0);
	CHECK(totals[3] == 0);
}

namespace {

/** Y1: two lanes from W and S, the W lane's A1 crossing under B1 through a tunnel with a copy beside it, merging into AD1 (domain-20). */
struct Y1Fixture : DrawnFixture {
	Y1Fixture() : DrawnFixture(
		"topology Y1\nport W in E: AC1\nport S in N: BD1\nport N out N: C1, B1\nport E out E: AD1\nnode AC1 -> C1, A1\nnode BD1 -> B1, D1\nnode A1, D1 -> AD1\ncross A1: B1<\ncross B1: A1>\n",
		"tunnel T1 A1: B1\ncopy A1 over T1\nstraight AC1: A1\nstraight BD1: B1\nstraight AD1: A1\n", 2)
	{
	}

	/** The header: Shape's headings, the base drawing, the base polyline's runs. */
	std::string Header() const
	{
		std::ostringstream out;
		out << "Shape's headings (the heading solve of ShapePlanarStructureVariant):\n" << shape_log << "base drawing (the flex-stripped variant, CheckPlanarDrawable 15 s, grid, tidy, integer, corridor, lead rows): " << draw_log << "\n";
		const std::vector<Points> base_points = PointsOf(base);
		for (uint32_t t = 0; t < n; t++) {
			out << "base " << Name(t) << ": start (" << base_points[t].front()[0] << "," << base_points[t].front()[1] << ") lengths";
			for (uint32_t k = 0; k < heads.at(Name(t)).size(); k++) {
				const auto it = base.find(Name(t));
				const auto l = it == base.end() ? std::map<uint32_t, int>::const_iterator() : it->second.find(k);
				out << " " << (it != base.end() && l != it->second.end() ? l->second : 0);
			}
			out << "\n";
		}
		return out.str();
	}
};

} // namespace

TEST_CASE("planar domain: Y1 loads and its base is legal", "[junction][planar][planardomain]")
{
	const Y1Fixture f;
	INFO(f.shape_log << f.draw_log);
	RequireBaseLegal(f);
	int verdict = 1;
	const Config none{"base", {}, {}};
	RunEncoding(f, nullptr, "Y1", {none}, "", DrawingProgramJudge("span+lead+endnode+crosspiece+grid"), &verdict);
	CHECK(verdict == 0);
}

TEST_CASE("planar domain: a trunk turning two eighths from a beside leg at its node is refused (F6)", "[junction][planar][planardomain]")
{
	/* Y1's one too-loose polyline (domain-24 report): A1#8 leaves the node at (20, 9) heading NE (1) while the copy A1#9 closes into it heading SE (3), because contraction removed Shape's zero first piece. */
	const Y1Fixture f;
	Geometry g = f.base;
	g["A1#8"] = {{0, 0}, {1, 2}, {2, 2}, {3, 2}, {4, 0}};
	const std::optional<Contracted> cv = ContractedVariant(f, g);
	REQUIRE(cv.has_value());
	CHECK(cv->v.tracks[f.idx.at("A1#8")].pieces == std::vector<int>({1, 2, 3}));
	const RowsJudge judge = DrawingProgramJudge(DEFAULT_RULES);
	CHECK(judge(f, *cv, true).rules.contains("node turn"));
	/* The base drawing's contracted writing turns at most one eighth at every node. */
	const std::optional<Contracted> base = ContractedVariant(f, f.base);
	REQUIRE(base.has_value());
	CHECK_FALSE(judge(f, *base, true).rules.contains("node turn"));
}

namespace {

std::vector<Config> Y1Configs(const Y1Fixture &f)
{
	const std::vector<int> runs = {2, 4, 6, 8, 10};
	/* Y1a: B1's N run before its crossing (piece 4) moves the crossing along the span, B1's last piece absorbing so the port end stays; A1's last run before the copy's split node (piece 4) in {2..10}, A1's first run absorbing so the split stays put. The copy is rigid (a corridor). */
	Config a{"Y1a", {}, {}};
	a.axes.emplace_back("B1", 4, std::vector<int>{2, 4, 6, 8, 10, 12});
	a.absorbers.emplace_back("B1", static_cast<uint32_t>(f.heads.at("B1").size() - 1));
	a.axes.emplace_back("A1", 4, runs);
	a.absorbers.emplace_back("A1", 0);
	/* Y1b: the mirror at the merge: AD1's first run in {2..10}, its last piece absorbing; A1#8's last run in {2..10}, its first run absorbing. */
	Config b{"Y1b", {}, {}};
	b.axes.emplace_back("AD1", 0, runs);
	b.absorbers.emplace_back("AD1", static_cast<uint32_t>(f.heads.at("AD1").size() - 1));
	b.axes.emplace_back("A1#8", 4, runs);
	b.absorbers.emplace_back("A1#8", 0);
	std::vector<Config> configs = {a, b};
	for (const Config &c : NeighbourhoodConfigs(f, 3)) configs.push_back(c);
	return configs;
}

} // namespace

TEST_CASE("planar domain: our keep-apart against the domain on Y1", "[.][planardomainenc]")
{
	const char *report_path = std::getenv("PLANAR_DOMAIN_REPORT_Y1");
	REQUIRE(report_path != nullptr);
	const Y1Fixture f;
	RequireBaseLegal(f);
	const std::vector<Config> configs = Y1Configs(f);
	RunEncoding(f, report_path, "Y1", configs, f.Header(), DrawingProgramJudge(""));
}

TEST_CASE("planar domain: Y1 is exact under the default rule set", "[junction][planar][planardomainexact]")
{
	const Y1Fixture f;
	RequireBaseLegal(f);
	std::array<uint32_t, 8> totals{};
	RunEncoding(f, nullptr, "Y1", Y1Configs(f), "", DrawingProgramJudge(DEFAULT_RULES), nullptr, &totals);
	INFO("Y1: too strict " << totals[2] << ", too loose " << totals[3]);
	CHECK(totals[2] == 0);
	CHECK(totals[3] == 0);
}

namespace {

/** P1: a prio row, the beside track of a prio join (domain-23). */
struct P1Fixture : DrawnFixture {
	P1Fixture() : DrawnFixture(
		"topology P1\nport W in E: A1\nport S in N: C1\nport E out E: AC1\nnode A1, C1 -> AC1\nprio A1\n",
		"straight AC1: A1\n", 2)
	{
	}

	/** The header: the expanded tracks, Shape's headings, the base drawing, the base polyline's runs. */
	std::string Header() const
	{
		std::ostringstream out;
		const PlanarTopology &x = v.structure.map.expanded;
		out << "expanded tracks:\n";
		for (uint32_t t = 0; t < n; t++) {
			const PlanarLine &l = x.lines[t];
			const auto end = [&](const PlanarEnd &e) { std::ostringstream o; o << (e.port ? "port " : "node ") << e.index; return o.str(); };
			out << "  " << Name(t) << ": from " << end(l.from) << " to " << end(l.to) << (v.structure.map.part_of[t] == UINT32_MAX ? ", beside" : ", not beside") << "\n";
		}
		out << "Shape's headings (the heading solve of ShapePlanarStructureVariant):\n" << shape_log << "base drawing (the flex-stripped variant, CheckPlanarDrawable 15 s, grid, tidy, integer, corridor, lead rows): " << draw_log << "\n";
		const std::vector<Points> base_points = PointsOf(base);
		for (uint32_t t = 0; t < n; t++) {
			out << "base " << Name(t) << ": start (" << base_points[t].front()[0] << "," << base_points[t].front()[1] << ") lengths";
			for (uint32_t k = 0; k < heads.at(Name(t)).size(); k++) {
				const auto it = base.find(Name(t));
				const auto l = it == base.end() ? std::map<uint32_t, int>::const_iterator() : it->second.find(k);
				out << " " << (it != base.end() && l != it->second.end() ? l->second : 0);
			}
			out << "\n";
		}
		return out.str();
	}
};

} // namespace

TEST_CASE("planar domain: P1's base turns at most one eighth at its node (F6)", "[junction][planar][planardomain]")
{
	/* C1 has total length 0 in P1's base: its writing has no heading, it is transparent. */
	const P1Fixture f;
	const std::optional<Contracted> base = ContractedVariant(f, f.base);
	REQUIRE(base.has_value());
	CHECK_FALSE(DrawingProgramJudge(DEFAULT_RULES)(f, *base, true).rules.contains("node turn"));
}

TEST_CASE("planar domain: sibling legs arriving on one heading are refused (F8)", "[junction][planar][planardomain]")
{
	/* F8: on P1 the lane A1#3 is 32 half tiles, the row A1#4 28 and C1's heading-2 piece 8 is 22: the geometry is connected and C1 runs ON the lane A1#3 along y = 3 into their merge (node 0). The domain refuses (`connection`: the track ends meet with no node). Both legs of the merge arrive with heading 2 after contraction, and their walls are consecutive in the face walk, so keep-apart never pairs them: the node shape (D400) refuses it, legs of one node keep distinct headings. */
	const P1Fixture f;
	Geometry g = f.base;
	g["A1#3"] = {{0, 32}};
	g["A1#4"] = {{0, 28}};
	g["C1"] = {{8, 22}};
	const std::optional<Contracted> cv = ContractedVariant(f, g);
	REQUIRE(cv.has_value());
	const Verdicts v = VerdictsOf(f, g, DrawingProgramJudge(DEFAULT_RULES));
	INFO("refused by " << Join(v.rules));
	CHECK_FALSE(v.domain);
	CHECK(v.rows == 1);
	CHECK(v.rules.contains("node turn"));
	/* The base C1's last contracted piece is one eighth off A1#3's; judge and domain accept. */
	const Verdicts base = VerdictsOf(f, f.base, DrawingProgramJudge(DEFAULT_RULES));
	CHECK(base.domain);
	CHECK(base.rows == 0);
}

TEST_CASE("planar domain: P1 loads, has a prio row and its base is legal", "[junction][planar][planardomain]")
{
	const P1Fixture f;
	INFO(f.shape_log << f.draw_log);
	CHECK(std::ranges::count(f.v.structure.map.part_of, UINT32_MAX) >= 1);
	CHECK(f.drawing.status == "Optimal");
	RequireBaseLegal(f);
	int verdict = 1;
	const Config none{"base", {}, {}};
	RunEncoding(f, nullptr, "P1", {none}, "", DrawingProgramJudge("span+lead+endnode+crosspiece+grid+portzero"), &verdict);
	CHECK(verdict == 0);
}

namespace {

std::vector<Config> P1Configs(const P1Fixture &f)
{
	std::vector<Config> configs;
	for (const Config &c : NeighbourhoodConfigs(f, 3)) configs.push_back(c);
	return configs;
}

} // namespace

TEST_CASE("planar domain: our keep-apart against the domain on P1", "[.][planardomainenc]")
{
	const char *report_path = std::getenv("PLANAR_DOMAIN_REPORT_P1");
	REQUIRE(report_path != nullptr);
	const P1Fixture f;
	RequireBaseLegal(f);
	const std::vector<Config> configs = P1Configs(f);
	RunEncoding(f, report_path, "P1", configs, f.Header(), DrawingProgramJudge(""));
}

TEST_CASE("planar domain: P1 is exact under the default rule set", "[junction][planar][planardomainexact]")
{
	const P1Fixture f;
	RequireBaseLegal(f);
	std::array<uint32_t, 8> totals{};
	RunEncoding(f, nullptr, "P1", P1Configs(f), "", DrawingProgramJudge(DEFAULT_RULES), nullptr, &totals);
	INFO("P1: too strict " << totals[2] << ", too loose " << totals[3]);
	CHECK(totals[2] == 0);
	CHECK(totals[3] == 0);
}

namespace {

/** P1 with the lane A1#3 `travel` half tiles from the row's split node to its merge node and the row A1#4 four shorter; C1's S port moves with the merge. */
Verdicts P1PrioTravel(int travel)
{
	P1Fixture f;
	Geometry g = f.base;
	g["A1#3"] = {{0, travel}};
	g["A1#4"] = {{0, travel - 4}};
	f.starts["C1"][0] = travel;
	return VerdictsOf(f, g, DrawingProgramJudge(DEFAULT_RULES));
}

} // namespace

TEST_CASE("planar domain: a prio row exactly its coverage long is accepted (D384)", "[junction][planar][planardomain]")
{
	/* The travel is measured along A1#3's points between the row's split node and merge node (one piece); the coverage is rules.coverage = 2 prio_tiles + 4. D410 positions are differences only, so a single-line side has no absolute pin (finding): C1's start is moved in the test instead. */
	const P1Fixture f;
	const Verdicts v = P1PrioTravel(f.v.structure.rules.coverage);
	INFO("refused by " << Join(v.rules));
	CHECK(v.domain);
	CHECK(v.rows == 0);
}

TEST_CASE("planar domain: a prio row longer than its coverage is refused (D384)", "[junction][planar][planardomain][!shouldfail]")
{
	/* D384: the drawing program has no coverage row yet (STATUS), so the judge accepts; the test flips to unexpected pass when an implementation adds `prio length`. The domain has no prio semantics and accepts. Travel = coverage + 2. D410 positions are differences only, so a single-line side has no absolute pin (finding). */
	const P1Fixture f;
	const Verdicts v = P1PrioTravel(f.v.structure.rules.coverage + 2);
	CHECK(v.domain);
	CHECK(v.rows == 1);
	CHECK(v.rules.contains("prio length"));
}

namespace {

/** BR1: X1's topology with its one structure a bridge instead of a tunnel (domain-31). */
struct BridgeFixture : DrawnFixture {
	BridgeFixture() : DrawnFixture(
		"topology X1\nport W in E: AC1\nport S in N: BD1\nport N out N @ 3 5: C1, B1\nport E out E @ 2 4: A1, D1\nnode AC1 -> C1, A1\nnode BD1 -> B1, D1\ncross A1: B1<\ncross B1: A1>\n",
		"bridge B1 A1: B1\nstraight AC1: A1\nstraight BD1: B1\n", 2)
	{
	}

	/** The header: Shape's headings, the base drawing, the base polyline's runs. */
	std::string Header() const
	{
		std::ostringstream out;
		out << "Shape's headings (the heading solve of ShapePlanarStructureVariant):\n" << shape_log << "base drawing (the flex-stripped variant, CheckPlanarDrawable 15 s, grid, tidy, integer, corridor, lead rows): " << draw_log << "\n";
		const std::vector<Points> base_points = PointsOf(base);
		for (uint32_t t = 0; t < n; t++) {
			out << "base " << Name(t) << ": start (" << base_points[t].front()[0] << "," << base_points[t].front()[1] << ") lengths";
			for (uint32_t k = 0; k < heads.at(Name(t)).size(); k++) {
				const auto it = base.find(Name(t));
				const auto l = it == base.end() ? std::map<uint32_t, int>::const_iterator() : it->second.find(k);
				out << " " << (it != base.end() && l != it->second.end() ? l->second : 0);
			}
			out << "\n";
		}
		return out.str();
	}
};

} // namespace

TEST_CASE("planar domain: BR1 loads, its structure is a bridge and its base is legal", "[junction][planar][planardomain]")
{
	const BridgeFixture f;
	INFO(f.shape_log << f.draw_log);
	REQUIRE(f.v.structure.structures.size() == 1);
	CHECK(f.v.structure.structures[0].kinds == 1);
	RequireBaseLegal(f);
	std::vector<std::string> notes;
	const PlanarDomainInput in = PlanarDomainInputOf(f.v, f.PointsOf(f.base), notes);
	REQUIRE(in.structures.size() == 1);
	CHECK_FALSE(in.structures[0].tunnel);
	CHECK(in.structures[0].ends == 1);
	int verdict = 1;
	const Config none{"base", {}, {}};
	RunEncoding(f, nullptr, "BR1", {none}, "", DrawingProgramJudge("span+lead+endnode+crosspiece+grid+portzero"), &verdict);
	CHECK(verdict == 0);
}

namespace {

std::vector<Config> BR1Configs(const BridgeFixture &f)
{
	std::vector<Config> configs;
	for (const Config &c : NeighbourhoodConfigs(f, 3)) configs.push_back(c);
	return configs;
}

} // namespace

TEST_CASE("planar domain: our keep-apart against the domain on BR1", "[.][planardomainenc]")
{
	const char *report_path = std::getenv("PLANAR_DOMAIN_REPORT_BR1");
	REQUIRE(report_path != nullptr);
	const BridgeFixture f;
	RequireBaseLegal(f);
	const std::vector<Config> configs = BR1Configs(f);
	RunEncoding(f, report_path, "BR1", configs, f.Header(), DrawingProgramJudge(""));
}

TEST_CASE("planar domain: BR1 is exact under the default rule set", "[junction][planar][planardomainexact]")
{
	const BridgeFixture f;
	RequireBaseLegal(f);
	std::array<uint32_t, 8> totals{};
	RunEncoding(f, nullptr, "BR1", BR1Configs(f), "", DrawingProgramJudge(DEFAULT_RULES), nullptr, &totals);
	INFO("BR1: too strict " << totals[2] << ", too loose " << totals[3]);
	CHECK(totals[2] == 0);
	CHECK(totals[3] == 0);
}

namespace {

/** TT1 (domain-43): a configuration, by topology and structure text. */
struct StackFixture : DrawnFixture {
	StackFixture(const std::string &topo, const std::string &structure) : DrawnFixture(topo, structure, 2) {}

	/** The header: Shape's headings, the base drawing, the base polyline's runs. */
	std::string Header() const
	{
		std::ostringstream out;
		out << "Shape's headings (the heading solve of ShapePlanarStructureVariant):\n" << shape_log << "base drawing (the flex-stripped variant, CheckPlanarDrawable 15 s, grid, tidy, integer, corridor, lead rows): " << draw_log << "\n";
		const std::vector<Points> base_points = PointsOf(base);
		for (uint32_t t = 0; t < n; t++) {
			out << "base " << Name(t) << ": start (" << base_points[t].front()[0] << "," << base_points[t].front()[1] << ") lengths";
			for (uint32_t k = 0; k < heads.at(Name(t)).size(); k++) {
				const auto it = base.find(Name(t));
				const auto l = it == base.end() ? std::map<uint32_t, int>::const_iterator() : it->second.find(k);
				out << " " << (it != base.end() && l != it->second.end() ? l->second : 0);
			}
			out << "\n";
		}
		return out.str();
	}
};

/** TT1: X1's topology, A1 and B1 each the other's tunnel separator (D390 (2)). */
struct TT1Fixture : StackFixture {
	TT1Fixture() : StackFixture("topology X1\nport W in E: AC1\nport S in N: BD1\nport N out N @ 3 5: C1, B1\nport E out E @ 2 4: A1, D1\nnode AC1 -> C1, A1\nnode BD1 -> B1, D1\ncross A1: B1<\ncross B1: A1>\n", "tunnel T1 A1: B1\ntunnel T2 B1: A1\nstraight AC1: A1\nstraight BD1: B1\n") {}
};

std::vector<Config> StackConfigs(const StackFixture &f)
{
	std::vector<Config> configs;
	for (const Config &c : NeighbourhoodConfigs(f, 3)) configs.push_back(c);
	return configs;
}

} // namespace

TEST_CASE("planar domain: TT1 loads, two tunnels cross and its base is legal", "[junction][planar][planardomain]")
{
	const TT1Fixture f;
	INFO(f.shape_log << f.draw_log);
	REQUIRE(f.v.structure.structures.size() == 2);
	RequireBaseLegal(f);
	std::vector<std::string> notes;
	const PlanarDomainInput in = PlanarDomainInputOf(f.v, f.PointsOf(f.base), notes);
	REQUIRE(in.structures.size() == 2);
	CHECK(in.structures[0].tunnel);
	CHECK(in.structures[1].tunnel);
}

TEST_CASE("planar domain: our keep-apart against the domain on TT1", "[.][planardomainenc]")
{
	const char *report_path = std::getenv("PLANAR_DOMAIN_REPORT_TT1");
	REQUIRE(report_path != nullptr);
	const TT1Fixture f;
	RequireBaseLegal(f);
	RunEncoding(f, report_path, "TT1", StackConfigs(f), f.Header(), DrawingProgramJudge(""));
}

TEST_CASE("planar domain: TT1 is exact under the default rule set", "[junction][planar][planardomainexact]")
{
	const TT1Fixture f;
	RequireBaseLegal(f);
	std::array<uint32_t, 8> totals{};
	RunEncoding(f, nullptr, "TT1", StackConfigs(f), "", DrawingProgramJudge(DEFAULT_RULES), nullptr, &totals);
	INFO("TT1: too strict " << totals[2] << ", too loose " << totals[3]);
	CHECK(totals[2] == 0);
	CHECK(totals[3] == 0);
}

namespace {

/** TS2 (domain-47): L1 widened to three lanes; W and E ports four tiles per lane (the default spacing is infeasible, see ts2-diag.md), N at its default. */
struct TS2Fixture : StackFixture {
	TS2Fixture() : StackFixture("topology TS2\nport W in E @ 2 6 10: AB1, AB2, AB3\nport N out N: A1, A2, A3\nport E out E @ 2 6 10: B1, B2, B3\nnode AB1 -> A1, B1\nnode AB2 -> A2, B2\nnode AB3 -> A3, B3\ncross B1: A2<, A3<\ncross B2: A3<\ncross A2: B1>\ncross A3: B2>, B1>\n", "tunnel T1 B1: A2, A3\ntunnel T2 B2: A3\nstraight AB1: B1\nstraight AB2: B2\nstraight AB3: B3\n") {}
};

} // namespace

TEST_CASE("planar domain: TS2 loads, its tunnels have crossers and its base is legal", "[junction][planar][planardomain]")
{
	const TS2Fixture f;
	INFO(f.shape_log << f.draw_log);
	REQUIRE(f.v.structure.structures.size() == 2);
	RequireBaseLegal(f);
	std::vector<std::string> notes;
	const PlanarDomainInput in = PlanarDomainInputOf(f.v, f.PointsOf(f.base), notes);
	REQUIRE(in.structures.size() == 2);
	CHECK(in.structures[0].tunnel);
	CHECK(in.structures[1].tunnel);
}

TEST_CASE("planar domain: our keep-apart against the domain on TS2", "[.][planardomainenc]")
{
	const char *report_path = std::getenv("PLANAR_DOMAIN_REPORT_TS2");
	REQUIRE(report_path != nullptr);
	const TS2Fixture f;
	RequireBaseLegal(f);
	RunEncoding(f, report_path, "TS2", StackConfigs(f), f.Header(), DrawingProgramJudge(""));
}

TEST_CASE("planar domain: TS2 is exact under the default rule set", "[junction][planar][planardomainexact]")
{
	const TS2Fixture f;
	RequireBaseLegal(f);
	std::array<uint32_t, 8> totals{};
	RunEncoding(f, nullptr, "TS2", StackConfigs(f), "", DrawingProgramJudge(DEFAULT_RULES), nullptr, &totals);
	INFO("TS2: too strict " << totals[2] << ", too loose " << totals[3]);
	CHECK(totals[2] == 0);
	CHECK(totals[3] == 0);
}

TEST_CASE("planar domain: a bend inside a tunnel's span is a span refusal by the judge and the domain (D436)", "[junction][planar][planardomain]")
{
	/* B1, the separator of T1, steps sideways (an S-bend, pieces 5 to 7) between its crossings with A2 and A3, so after contraction the two crossings sit on different pieces of the separator. Shape's headings changed with the tunnel end run (D437: 1 piece), so the pieces are 4 to 8 now, not 0 to 4. */
	const TS2Fixture f;
	Geometry g = f.base;
	g["B1"] = {{0, 2}, {4, 4}, {5, 1}, {6, 2}, {7, 1}};
	const std::optional<Contracted> cv = ContractedVariant(f, g);
	REQUIRE(cv.has_value());
	const RowsVerdict verdict = DrawingProgramJudge(DEFAULT_RULES)(f, *cv, true);
	CHECK_FALSE(verdict.accept);
	CHECK(verdict.rules == std::set<std::string>{"span"});
	std::vector<std::string> notes;
	const PlanarDomainVerdict d = CheckPlanarDomain(PlanarDomainInputOf(f.v, f.PointsOf(g), notes));
	CHECK_FALSE(d.legal);
}

namespace {

/** REQUIRE that no port rule refuses a fixture's base polyline. */
void RequireBasePortRows(const char *name, const X1Fixture &f)
{
	const std::optional<Contracted> cv = ContractedVariant(f, f.base);
	REQUIRE(cv.has_value());
	const RowsVerdict v = DrawingProgramJudge(DEFAULT_RULES)(f, *cv, true);
	INFO(name << " base is refused by " << Join(v.rules));
	CHECK_FALSE(v.rules.contains("port"));
}

} // namespace

TEST_CASE("planar domain: every fixture's base keeps its port rows", "[junction][planar][planardomain]")
{
	RequireBasePortRows("X1", X1Fixture());
	RequireBasePortRows("X2", X2Fixture());
	RequireBasePortRows("M1", M1Fixture());
	RequireBasePortRows("K1", K1Fixture());
	RequireBasePortRows("L1", L1Fixture());
	RequireBasePortRows("Y1", Y1Fixture());
	RequireBasePortRows("P1", P1Fixture());
}

TEST_CASE("planar domain: two port lines on one point are rejected by the port rows", "[junction][planar][planardomain]")
{
	/* L1's N port side has two out lines, A1 and A2, each ending on the box edge heading north. One axis piece parallel to the side (heading east or west) moves its line's end along the edge only: on either line, the piece whose new length is non-negative puts that line's end on the other's. */
	const L1Fixture f;
	const std::vector<Points> base = f.PointsOf(f.base);
	Geometry g = f.base;
	bool moved = false;
	for (const std::string &mover : {"A1", "A2"}) {
		const uint32_t t = f.idx.at(mover), other = f.idx.at(mover == "A1" ? "A2" : "A1");
		const int delta = base[other].back()[0] - base[t].back()[0];
		const std::vector<int> &h = f.heads.at(mover);
		for (uint32_t k = 0; k < h.size() && !moved; k++) {
			const int hh = ((h[k] % 8) + 8) % 8;
			if (hh != 2 && hh != 6) continue;
			const auto it = f.base.at(mover).find(k);
			const int length = (it == f.base.at(mover).end() ? 0 : it->second) + (hh == 2 ? delta : -delta);
			if (length < 0) continue;
			g[mover][k] = length;
			moved = true;
		}
		if (moved) break;
	}
	REQUIRE(moved);
	CHECK(f.PointsOf(g)[f.idx.at("A2")].back() == f.PointsOf(g)[f.idx.at("A1")].back());
	const std::optional<Contracted> cv = ContractedVariant(f, g);
	REQUIRE(cv.has_value());
	const RowsVerdict v = DrawingProgramJudge(DEFAULT_RULES)(f, *cv, true);
	INFO("refused by " << Join(v.rules));
	CHECK_FALSE(v.accept);
	CHECK(v.rules.contains("port"));
}

TEST_CASE("planar domain: an L1 polyline off its N edge is off interface, not too strict", "[junction][planar][planardomain]")
{
	/* The first L1 off-interface example of the domain-29 report: A2 ends short of the N box edge (its port rows fail), and the rest of its rows hold, so the read-out without the interface rows accepts it. */
	const L1Fixture f;
	RequireBaseLegal(f);
	const Config one{"one", {{"A2", 7, {0}}, {"A2", 8, {0}}, {"A2", 9, {2}}, {"A2", 10, {1}}, {"A2", 11, {0}}}, {}};
	Geometry g = f.base;
	for (const auto &[name, piece, vals] : one.axes) g[name][piece] = vals[0];
	const RowsJudge judge = DrawingProgramJudge("span+lead+endnode+crosspiece+grid+portzero");
	const std::optional<Contracted> cv = ContractedVariant(f, g);
	REQUIRE(cv.has_value());
	const RowsVerdict full = judge(f, *cv, true);
	INFO("refused by " << Join(full.rules));
	CHECK_FALSE(full.accept);
	CHECK(full.rules.contains("port"));
	CHECK(judge(f, *cv, false).accept);
	CHECK(VerdictsOf(f, g, judge).domain);
	const std::string path = (std::filesystem::temp_directory_path() / "planar_domain_offinterface_l1.txt").string();
	RunEncoding(f, path.c_str(), "L1", {one}, "", DrawingProgramJudge("span+lead+endnode+crosspiece+grid+portzero"));
	std::ifstream in(path);
	const std::string report((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	std::filesystem::remove(path);
	INFO(report);
	CHECK(report.find("A2:1*1,0*2,7*1,;") != std::string::npos);
	CHECK(report.find("  too strict: 0 polylines") != std::string::npos);
	CHECK(report.find("  off interface: 1 polylines") != std::string::npos);
}

TEST_CASE("planar domain: an outward S-bend at a node is legal (D430)", "[junction][planar][planardomain]")
{
	/* X1's A1 leaves its split on an SE step at once (away from C1, which leaves NE) and returns NE beyond the crossing's end runs: both the rows and the domain accept. The same step toward C1 (NE at once, with the flank and the SE return) is the toward case: its lead row fails and both reject (F1's geometry does not fit: it has no bend back). */
	const X1Fixture f;
	Geometry away = f.base;
	away["A1"] = {{3, 2}, {4, 10}, {5, 2}, {8, 2}};
	const RowsJudge judge = DrawingProgramJudge(DEFAULT_RULES);
	const Verdicts a = VerdictsOf(f, away, judge);
	CHECK(a.rows == 0);
	CHECK(a.domain);
	Geometry toward = f.base;
	toward["A1"] = {{1, 2}, {2, 2}, {3, 2}, {4, 6}, {8, 4}};
	const Verdicts t = VerdictsOf(f, toward, judge);
	CHECK(t.rows == 1);
	CHECK_FALSE(t.domain);
	CHECK(t.rules.contains("lead"));
}

TEST_CASE("planar domain: a bridge's one-piece end run fits where a tunnel's two do not (BR1)", "[junction][planar][planardomain]")
{
	/* BR1's B1 crosses A1 at its third tile (x = 5, by a NW step before and a NE step after): the bridge head is the second tile, just off the split's switch tile. The same polyline on the structure as a tunnel with the flat-ground ramp of D155 (before D437: kind tunnel, end run set to 2) puts its portal on the switch tile: the rows' `end 0 node` row breaks and the domain rejects. */
	const BridgeFixture f;
	Geometry g = f.base;
	g["B1"] = {{0, 2}, {1, 2}, {4, 6}, {7, 2}, {8, 4}};
	const Verdicts bridge = VerdictsOf(f, g, DrawingProgramJudge(DEFAULT_RULES));
	CHECK(bridge.rows == 0);
	CHECK(bridge.domain);
	BridgeFixture tunnel = f;
	tunnel.v.structure.structures[0].kinds = 2;
	tunnel.v.structure.structures[0].limit[0] = -1;
	tunnel.v.structure.structures[0].limit[1] = 10;
	tunnel.v.structures[0].end_run[1] = 2;
	const Verdicts as_tunnel = VerdictsOf(tunnel, g, DrawingProgramJudge(DEFAULT_RULES));
	CHECK(as_tunnel.rows == 1);
	CHECK_FALSE(as_tunnel.domain);
	CHECK(as_tunnel.rules.contains("end run"));
}

TEST_CASE("planar domain: a zero D412 port line holds under port_zero and a one-half-tile reference makes it ordinary (D434)", "[junction][planar][planardomain]")
{
	/* BR1's base has AC1 at total length 0 with its node on the box edge. Its rows accept under port_zero (the line is 0 where the reference travels below one half tile). With a reference travel of one half tile AC1 is an ordinary line: its pieces have least lengths, the zero line breaks them. With port_zero off D412 frees every piece, so nothing breaks. X1's and K1's ports sit away from their nodes, so only BR1 can show the zero line itself. */
	const BridgeFixture f;
	const std::vector<Points> points = f.PointsOf(f.base);
	const uint32_t ac1 = f.idx.at("AC1");
	REQUIRE(std::ranges::all_of(points[ac1], [&](const Point &p) { return p == points[ac1].front(); }));
	const Verdicts v = VerdictsOf(f, f.base, DrawingProgramJudge(DEFAULT_RULES));
	CHECK(v.rows == 0);
	CHECK(v.domain);
	/* With port_zero off D412 frees every piece, so the rows accept it as well. */
	CHECK(VerdictsOf(f, f.base, DrawingProgramJudge("span+lead+endnode+crosspiece+grid")).rows == 0);
}

TEST_CASE("planar domain: a copy one lane further from its parent breaks the corridor (Y1)", "[junction][planar][planardomain]")
{
	/* Y1's copy A1#9 runs one lane (2 half tiles) north of its parent. Moved one more lane north over its whole length it no longer opens and closes at its split nodes and the domain rejects it. The base is accepted by both. */
	const Y1Fixture f;
	const RowsJudge judge = DrawingProgramJudge(DEFAULT_RULES);
	const Verdicts base = VerdictsOf(f, f.base, judge);
	CHECK(base.rows == 0);
	CHECK(base.domain);
	Y1Fixture moved = f;
	moved.starts["A1#9"][1] -= 2;
	const Verdicts out = VerdictsOf(moved, f.base, judge);
	INFO("refused by " << Join(out.rules));
	CHECK(out.rows == 1);
	CHECK(out.rules.contains("corridor"));
	CHECK_FALSE(out.domain);
}

/* The drawing program's own tests (an implementation of RowsJudge), not the behaviour. */

TEST_CASE("planar draw: keep-apart pairs hold on given points", "[junction][planar][planardraw]")
{
	PlanarKeepApartPair skipped;
	skipped.skipped = "span";
	CHECK(PlanarKeepApartHolds(skipped, {}));
	CHECK(PlanarKeepApartHolds(skipped, {{{0, 0}}, {{0, 0}}}));

	PlanarKeepApartPair pair;
	pair.candidates.push_back({{1, 0}, 2, {{{0, 0}, {1, 0}}}});
	pair.candidates.push_back({{-1, 0}, 2, {{{0, 0}, {1, 0}}}});
	/* Track 1's point 0 is 2 right of track 0's point 0, then 1 right, then 2 left. */
	CHECK(PlanarKeepApartHolds(pair, {{{0, 0}}, {{2, 0}}}));
	CHECK_FALSE(PlanarKeepApartHolds(pair, {{{0, 0}}, {{1, 0}}}));
	CHECK(PlanarKeepApartHolds(pair, {{{0, 0}}, {{-2, 0}}}));
}

TEST_CASE("planar draw: node lead rows on X1", "[junction][planar][planardraw]")
{
	const X1Fixture f;
	const std::vector<PlanarPointRow> rows = PlanarNodeLeadRows(f.v);
	/*
	 * X1's sites: node 0 splits AC1 into straight A1 (side -1, the other leg C1 leaves on the left) and C1; node 1 splits BD1 into straight B1 (side +1) and D1. Its tracks have no merge (their ends are ports).
	 * Sibling side: A1 -1, C1 +1, B1 +1, D1 -1. Flex middles: A1 1 3 5 7; C1 1 3 6 8; B1 1 3 5 7; D1 1 3 6 8, so the leading walk is m = 1, 3 (C1, D1) or 1, 3, 5, 7 (A1, B1).
	 * Insert side d = sign(pieces[m] - pieces[m - 1]); a row for every m with d on the sibling side: from the sites, A1 m=1 (1 - 2 = -1); C1 m=3 (2 - 1); B1 m=3 (1 - 0); D1 m=1 (0 - 1); the walk of A1 and B1 beyond m=3 is checked against the pieces below.
	 */
	REQUIRE(f.v.structure.sites.size() == 2);
	std::map<std::string, int> sib = {{"A1", 0}, {"C1", 0}, {"B1", 0}, {"D1", 0}};
	for (const PlanarStructureSite &site : f.v.structure.sites) {
		const PlanarNode &node = f.v.structure.map.expanded.nodes[site.node];
		REQUIRE(node.out.size() == 2);
		REQUIRE(std::ranges::count(node.out, site.straight) == 1);
		for (const uint32_t leg : node.out) sib[f.v.structure.map.expanded.lines[leg].name] = leg == site.straight ? site.side : -site.side;
	}
	CHECK(sib == std::map<std::string, int>{{"A1", -1}, {"C1", 1}, {"B1", 1}, {"D1", -1}});
	std::set<std::string> want;
	for (const auto &[name, side] : sib) {
		const std::vector<int> &h = f.v.tracks[f.idx.at(name)].pieces;
		uint32_t k = 0;
		for (const uint32_t m : f.v.tracks[f.idx.at(name)].flex) {
			if (m != 1 + 2 * k) break;
			k++;
			const int d = h[m] > h[m - 1] ? 1 : (h[m] < h[m - 1] ? -1 : 0);
			if (d == side) want.insert((std::ostringstream{} << "lead " << name << " start " << m).str());
		}
	}
	CHECK(want.size() >= 4);
	CHECK(want.contains("lead A1 start 1"));
	CHECK(want.contains("lead C1 start 3"));
	CHECK(want.contains("lead B1 start 3"));
	CHECK(want.contains("lead D1 start 1"));
	REQUIRE(rows.size() == want.size());
	std::map<std::string, const PlanarPointRow *> by;
	for (const PlanarPointRow &r : rows) by[r.tag] = &r;
	for (const std::string &tag : want) {
		INFO(tag);
		REQUIRE(by.count(tag) == 1);
		const PlanarPointRow &r = *by[tag];
		CHECK(r.least == 0);
		CHECK(r.terms.size() == 3);
	}
	/* The base puts the runs of A1 and B1 on piece 4 (pieces 0 to 3 at 0): the rows of their first toward insert (m = 1 for A1) have no insert length, so they hold. */
	const std::vector<Points> base = f.PointsOf(f.base);
	for (const PlanarPointRow &r : rows) {
		INFO(r.tag);
		CHECK(PlanarPointRowHolds(r, base));
	}
	/* The same polyline with the runs on piece 0: every row holds. */
	Geometry led = f.base;
	led["A1"][0] = 12;
	led["A1"][4] = 0;
	led["B1"][0] = 6;
	led["B1"][4] = 0;
	const std::vector<Points> moved = f.PointsOf(led);
	for (const PlanarPointRow &r : rows) {
		INFO(r.tag);
		CHECK(PlanarPointRowHolds(r, moved));
	}
	/* One of the 18 polylines rejected before: A1 with the run at piece 4 (pieces 0 to 2 at 0, piece 3 = 2), the toward insert m = 1 at 0. Its row holds. */
	Geometry out = f.base;
	out["A1"] = {{3, 2}, {4, 12}, {8, 4}};
	CHECK(PlanarPointRowHolds(*by["lead A1 start 1"], f.PointsOf(out)));
	/* A used toward insert on the node: A1 piece 0 = 0 and piece 1 = 1 fails the row. */
	Geometry used = f.base;
	used["A1"] = {{1, 1}, {4, 12}};
	CHECK_FALSE(PlanarPointRowHolds(*by["lead A1 start 1"], f.PointsOf(used)));
	/* With a piece-0 run in front of it, it holds. */
	used["A1"] = {{0, 1}, {1, 1}, {4, 12}};
	CHECK(PlanarPointRowHolds(*by["lead A1 start 1"], f.PointsOf(used)));
}

TEST_CASE("planar draw: node lead rows on contracted lines", "[junction][planar][planardraw]")
{
	const X1Fixture f;
	/* S1's first too-loose polyline: B1 = 1x2, 0x10 contracted. B1's first piece is its toward insert (steps), the node-side flank is absent: the row fails. */
	Geometry s1 = f.base;
	s1["B1"] = {{3, 2}, {4, 2}, {6, 4}, {8, 4}};
	const std::optional<Contracted> cv = ContractedVariant(f, s1);
	REQUIRE(cv.has_value());
	const PlanarShapeTrack &b1 = cv->v.tracks[f.idx.at("B1")];
	CHECK(b1.pieces == std::vector<int>({1, 0}));
	CHECK(b1.steps == std::vector<uint32_t>({0}));
	const std::vector<PlanarPointRow> rows = PlanarNodeLeadRows(cv->v);
	std::map<std::string, const PlanarPointRow *> by;
	for (const PlanarPointRow &r : rows) by[r.tag] = &r;
	REQUIRE(by.count("lead B1 start 0") == 1);
	CHECK_FALSE(PlanarPointRowHolds(*by["lead B1 start 0"], cv->points));
	/* X1's base contracted: every lead row holds. */
	const std::optional<Contracted> base = ContractedVariant(f, f.base);
	REQUIRE(base.has_value());
	for (const PlanarPointRow &r : PlanarNodeLeadRows(base->v)) {
		INFO(r.tag);
		CHECK(PlanarPointRowHolds(r, base->points));
	}
	/* The D430 outward case: A1 leaves the split on its SE insert (a step away from its sibling): no lead row fails. */
	Geometry away = f.base;
	away["A1"] = {{3, 2}, {4, 12}, {8, 4}};
	const std::optional<Contracted> av = ContractedVariant(f, away);
	REQUIRE(av.has_value());
	CHECK(av->v.tracks[f.idx.at("A1")].steps == std::vector<uint32_t>({0}));
	for (const PlanarPointRow &r : PlanarNodeLeadRows(av->v)) {
		INFO(r.tag);
		CHECK(PlanarPointRowHolds(r, av->points));
	}
}

TEST_CASE("planar draw: least_extra raises every non-free piece's least length", "[junction][planar][planardraw]")
{
	/* The F2 test's read-out: X1's base contracted (no flex), so its pieces have least lengths. */
	const X1Fixture f;
	const std::optional<Contracted> cv = ContractedVariant(f, f.base);
	REQUIRE(cv.has_value());
	PlanarShapeVariant plain = cv->v;
	for (PlanarShapeTrack &track : plain.tracks) track.steps.clear();
	PlanarProgram programs[2];
	for (const int extra : {0, 2}) {
		PlanarDrawOptions o = f.options;
		o.strict_span = o.strict_zero = o.crossing_flanks = false;
		o.keep_apart_out = true;
		o.program_out = true;
		o.build_only = true;
		o.grid = true;
		o.least_extra = extra;
		const PlanarDrawResult r = CheckPlanarDrawable(plain, 5.0, o);
		REQUIRE(r.status == "built");
		programs[extra / 2] = r.program;
	}
	REQUIRE(programs[0].lower.size() == programs[1].lower.size());
	size_t raised = 0, free = 0;
	for (size_t i = 0; i < programs[0].lower.size(); i++) {
		if (programs[0].group[i] != "axis" && programs[0].group[i] != "diagonal") continue;
		if (programs[0].lower[i] == 1) {
			CHECK(programs[1].lower[i] == 3);
			raised++;
		} else if (programs[0].lower[i] == 0) {
			CHECK(programs[1].lower[i] == 0);
			free++;
		}
	}
	CHECK(raised > 0);
	CHECK(free > 0);
}

TEST_CASE("planar draw: port_zero holds a zero D412 line at 0 and frees an ordinary one never (D434)", "[junction][planar][planardraw]")
{
	const X1Fixture f;
	const std::optional<Contracted> cv = ContractedVariant(f, f.base);
	REQUIRE(cv.has_value());
	PlanarShapeVariant plain = cv->v;
	for (PlanarShapeTrack &track : plain.tracks) track.steps.clear();
	const uint32_t ac1 = f.idx.at("AC1");
	for (const double travel : {0.0, 4.0}) {
		std::vector<std::vector<double>> from = PortZeroFrom(cv->points);
		from[ac1] = {travel};
		PlanarDrawOptions o = f.options;
		o.strict_span = o.strict_zero = o.crossing_flanks = false;
		o.program_out = true;
		o.build_only = true;
		o.grid = true;
		o.port_zero = true;
		o.port_zero_from = &from;
		const PlanarDrawResult r = CheckPlanarDrawable(plain, 5.0, o);
		REQUIRE(r.status == "built");
		const PlanarProgram &pr = r.program;
		const bool row = std::ranges::any_of(pr.rows, [](const PlanarProgram::Row &w) { return w.tag == "portzero AC1"; });
		/* AC1's length variables: the variables with a coefficient in its `piece AC1 <i>` rows that are lengths (group axis or diagonal). */
		std::set<uint32_t> lengths;
		for (const PlanarProgram::Row &w : pr.rows) {
			if (!w.tag.starts_with("piece AC1 ")) continue;
			for (const auto &[var, c] : w.terms) {
				if (pr.group[var] == "axis" || pr.group[var] == "diagonal") lengths.insert(var);
			}
		}
		REQUIRE(lengths.size() == plain.tracks[ac1].pieces.size());
		INFO("travel " << travel);
		if (travel == 0) {
			CHECK(row);
			for (const uint32_t v : lengths) CHECK(pr.lower[v] == 0);
		} else {
			CHECK(!row);
			for (const uint32_t v : lengths) CHECK(pr.lower[v] >= 1);
		}
	}
}

TEST_CASE("planar draw: keep-apart on fixed points agrees with the program on X1", "[junction][planar][planardraw]")
{
	const X1Fixture f;
	const std::vector<Points> base = f.PointsOf(f.base);

	/* 1: with the strict rules on, the read-out is built and some pair fails on the base geometry. */
	const PlanarDrawResult strict = f.ReadOut(true);
	REQUIRE(strict.status == "built");
	REQUIRE(!strict.keep_apart.empty());
	CHECK(std::ranges::any_of(strict.keep_apart, [&](const PlanarKeepApartPair &p) { return !PlanarKeepApartHolds(p, base); }));

	/* 2: with them off, every pair holds. */
	const PlanarDrawResult plain = f.ReadOut(false);
	REQUIRE(plain.status == "built");
	REQUIRE(!plain.keep_apart.empty());
	for (const PlanarKeepApartPair &p : plain.keep_apart) {
		INFO(p.tag);
		CHECK(PlanarKeepApartHolds(p, base));
	}

	/* 3: the program with the points fixed says the same. */
	PlanarDrawOptions o = f.options;
	o.fix_points = &base;
	o.fix_window = 0;
	o.tidy = false;
	CHECK(CheckPlanarDrawable(f.v, 5.0, o).status == "Infeasible");
	o.strict_span = o.strict_zero = o.crossing_flanks = false;
	CHECK(CheckPlanarDrawable(f.v, 5.0, o).status == "Optimal");
}

TEST_CASE("planar draw: the program on fixed points agrees with the program on X1", "[junction][planar][planardraw]")
{
	const X1Fixture f;
	const PlanarDrawResult out = f.ProgramOut();
	REQUIRE(out.status == "built");
	REQUIRE(!out.program.rows.empty());
	REQUIRE(out.program.lower.size() == out.columns);

	/* The base: the program with these points fixed was Optimal, so no row breaks. */
	const PlanarProgramCheck base = CheckPlanarProgramOnPoints(out.program, f.PointsOf(f.base));
	std::string undetermined;
	for (const std::string &u : base.undetermined) undetermined += " " + u;
	WARN("base undetermined:" << undetermined);
	CHECK(base.broken.empty());
	for (const std::string &b : base.broken) WARN("base broken: " << b);
	for (const std::string &u : base.undetermined) CHECK((u == "sep" || u == "sepdir" || u == "box" || u == "lane" || u == "land" || u == "clear" || u == "room" || u == "along"));

	/* A1's run split over pieces 4 and 8 (8 each, the same end point): the structure's end run breaks (`end 0`); the crossing keeps rules.cross = 1, so no `cross` row breaks. */
	Geometry split = f.base;
	split["A1"] = {{4, 8}, {8, 8}};
	const std::vector<Points> split_points = f.PointsOf(split);
	const PlanarProgramCheck check = CheckPlanarProgramOnPoints(out.program, split_points);
	CHECK(check.broken == std::vector<std::string>{"end 0"});

	/* The program itself, with the points fixed and the strict rules off, agrees. */
	PlanarDrawOptions o = f.options;
	o.fix_points = &split_points;
	o.fix_window = 0;
	o.tidy = false;
	o.strict_span = o.strict_zero = o.crossing_flanks = false;
	const PlanarDrawResult solved = CheckPlanarDrawable(f.v, 5.0, o);
	CHECK(solved.status == "Infeasible");
	CHECK(std::ranges::count(solved.conflict, std::string("end 0")) == 1);
}

TEST_CASE("planar draw: the program checked by track sets", "[junction][planar][planardraw]")
{
	const X1Fixture f;
	const PlanarDrawResult out = f.ProgramOut();
	REQUIRE(out.status == "built");
	const std::vector<Points> base = f.PointsOf(f.base);
	Geometry split = f.base;
	split["A1"] = {{4, 8}, {8, 8}};
	const std::vector<Points> split_points = f.PointsOf(split);

	/* With B1 alone placed, the rows whose sets hold A1 are not evaluated: `end 0` stays quiet on the split geometry. */
	std::vector<bool> only_b(f.n, false);
	only_b[f.idx.at("B1")] = true;
	CHECK(CheckPlanarProgramOnPoints(out.program, split_points, only_b).broken.empty());
	CHECK(CheckPlanarProgramOnPoints(out.program, base, only_b).broken.empty());
	/* `end 0` needs A1 and B1: with A1 alone it is still not evaluated, with both it breaks. */
	std::vector<bool> only_a(f.n, false);
	only_a[f.idx.at("A1")] = true;
	CHECK(CheckPlanarProgramOnPoints(out.program, split_points, only_a).broken.empty());
	std::vector<bool> a_and_b = only_a;
	a_and_b[f.idx.at("B1")] = true;
	CHECK(CheckPlanarProgramOnPoints(out.program, split_points, a_and_b).broken == std::vector<std::string>{"end 0"});

	/* With every track placed the two forms agree. */
	const std::vector<bool> all(f.n, true);
	for (const std::vector<Points> *pts : {&base, &split_points}) {
		CHECK(CheckPlanarProgramOnPoints(out.program, *pts, all).broken == CheckPlanarProgramOnPoints(out.program, *pts).broken);
	}
	CHECK(CheckPlanarProgramOnPoints(out.program, base, all).broken.empty());
	CHECK(CheckPlanarProgramOnPoints(out.program, split_points, all).broken == std::vector<std::string>{"end 0"});
}

TEST_CASE("planar draw: end runs stay off a node's switch tile", "[junction][planar][planardraw]")
{
	X1Fixture f;
	/* The tunnel's end run is its portal (1 piece, D437), so the row needs the crossing at least 4 half tiles from the split's switch tile (2 * 1 + 2). B1 steps NW 4 half tiles at once: its crossing with A1 is 3 half tiles from the node's tile, one half tile closer than the nearest placement the row allows (x = 8, travel 4); only `end 0 node` breaks. */
	Geometry g = f.base;
	g["B1"] = {{1, 4}, {4, 2}, {7, 4}};
	const std::vector<Points> points = f.PointsOf(g);
	const auto has = [](const PlanarProgramCheck &c, const std::string &tag) { return std::ranges::find(c.broken, tag) != c.broken.end(); };

	/* Red: without the option no `end 0 node` row exists, so it cannot break. */
	f.options.end_node = false;
	const PlanarDrawResult plain = f.ProgramOut();
	REQUIRE(plain.status == "built");
	CHECK(!has(CheckPlanarProgramOnPoints(plain.program, points), "end 0 node"));
	for (const PlanarProgram::Row &r : plain.program.rows) CHECK(r.tag != "end 0 node");

	f.options.end_node = true;
	const PlanarDrawResult out = f.ProgramOut();
	REQUIRE(out.status == "built");
	CHECK(has(CheckPlanarProgramOnPoints(out.program, points), "end 0 node"));
	/* The base holds. */
	CHECK(CheckPlanarProgramOnPoints(out.program, f.PointsOf(f.base)).broken.empty());
}

TEST_CASE("planar draw: end runs count a diagonal crosser's footprint", "[junction][planar][planardraw]")
{
	X2Fixture f;
	f.options.end_node = true;
	f.options.cross_piece = true;
	/* The first too-loose example's witness (encoding-x2-span_lead_endnode_crosspiece.txt, group [connection]): B1 crosses A1 diagonally at (16, 5), its pieces occupy A1's tiles 7 and 8; the tunnel's last end run (the portal alone, 1 piece, D437) would need tile 9 and A1 here is cut to piece 4 of length 14 (it ends at tile 8 of its 9), one tile short of the end run (found by search: 15 holds, 14 breaks `end 0`). */
	const std::map<std::string, std::vector<int>> witness = {
		{"AC1", {0, 0, 0, 0, 4}}, {"BD1", {0, 0, 0, 0, 2}}, {"C1", {0, 0, 0, 0, 0, 0, 0, 0, 3, 2}},
		{"B1", {0, 0, 0, 0, 2, 0, 0, 1, 2, 2, 0, 0, 0, 0, 0, 0, 0, 1, 4}}, {"A1", {0, 0, 0, 0, 14, 0, 0, 0, 0}}, {"D1", {0, 0, 0, 0, 0, 0, 1, 0, 0, 8}},
	};
	Geometry g;
	for (const auto &[name, lens] : witness) {
		for (size_t k = 0; k < lens.size(); k++) {
			if (lens[k] != 0) g[name][static_cast<uint32_t>(k)] = lens[k];
		}
	}
	const std::vector<Points> points = f.PointsOf(g);
	const PlanarDrawResult out = f.ProgramOut();
	REQUIRE(out.status == "built");
	const PlanarProgramCheck check = CheckPlanarProgramOnPoints(out.program, points);
	CHECK(std::ranges::find(check.broken, "end 0") != check.broken.end());
}

TEST_CASE("planar draw: a crossing sits on a piece with length", "[junction][planar][planardraw]")
{
	X1Fixture f;
	/* The last too-loose example's witness (encoding-x1-span_lead_endnode.txt, S1, group [head]): B1's crossing piece (4) has length 0. */
	const std::map<std::string, std::vector<int>> witness = {
		{"AC1", {0, 0, 0, 0, 4}}, {"BD1", {0, 0, 0, 0, 2}}, {"C1", {0, 0, 0, 0, 0, 0, 0, 0, 3, 2}},
		{"B1", {4, 1, 0, 0, 0, 1, 0, 0, 8}}, {"A1", {0, 0, 0, 0, 10, 0, 0, 0, 6}}, {"D1", {0, 0, 0, 0, 0, 0, 1, 0, 0, 8}},
	};
	Geometry g;
	for (const auto &[name, lens] : witness) {
		for (size_t k = 0; k < lens.size(); k++) {
			if (lens[k] != 0) g[name][static_cast<uint32_t>(k)] = lens[k];
		}
	}
	const std::vector<Points> points = f.PointsOf(g);
	const auto has = [](const PlanarProgramCheck &c, const std::string &tag) { return std::ranges::find(c.broken, tag) != c.broken.end(); };

	/* Red: without the option no `cross 0 piece` row exists, so it cannot break. */
	f.options.cross_piece = false;
	const PlanarDrawResult plain = f.ProgramOut();
	REQUIRE(plain.status == "built");
	CHECK(!has(CheckPlanarProgramOnPoints(plain.program, points), "cross 0 piece"));
	for (const PlanarProgram::Row &r : plain.program.rows) CHECK(r.tag != "cross 0 piece");

	f.options.cross_piece = true;
	const PlanarDrawResult out = f.ProgramOut();
	REQUIRE(out.status == "built");
	CHECK(has(CheckPlanarProgramOnPoints(out.program, points), "cross 0 piece"));
	/* The base holds. */
	CHECK(CheckPlanarProgramOnPoints(out.program, f.PointsOf(f.base)).broken.empty());
}

TEST_CASE("planar domain: representations of a polyline", "[junction][planar][planardraw]")
{
	const std::vector<int> pieces = {2, 1, 2, 3, 2};
	const std::vector<Lengths> a = Representations(pieces, {{2, 4}}, 1000);
	CHECK(a.size() == 6);
	for (const Lengths &l : a) CHECK(PolylineRuns(pieces, l) == Runs{{2, 4}});
	const Runs b = {{2, 2}, {3, 1}, {2, 2}};
	const std::vector<Lengths> r = Representations(pieces, b, 1000);
	CHECK(r.size() == 2);
	for (const Lengths &l : r) {
		CHECK(PolylineRuns(pieces, l) == b);
		CHECK(l[1] == 0);
		CHECK(l[3] == 1);
		CHECK(l[4] == 2);
	}
	bool capped = false;
	CHECK(Representations(pieces, {{2, 4}}, 3, &capped).size() == 3);
	CHECK(capped);
}

TEST_CASE("planar domain: a polyline has one contracted writing", "[junction][planar][planardraw]")
{
	const X1Fixture f;
	const std::optional<Contracted> base = ContractedVariant(f, f.base);
	REQUIRE(base.has_value());
	/* Every kept piece has length, no flex or steps remain, and the runs merge: A1 is one run of 12 then one of 4 (heading 2). */
	for (uint32_t t = 0; t < f.n; t++) {
		CHECK(base->v.tracks[t].flex.empty());
		CHECK(base->v.tracks[t].steps.empty());
		CHECK(base->points[t].size() == base->v.tracks[t].pieces.size() + 1);
		for (size_t k = 0; k < base->v.tracks[t].pieces.size(); k++) CHECK(base->points[t][k] != base->points[t][k + 1]);
		for (size_t k = 1; k < base->v.tracks[t].pieces.size(); k++) CHECK(base->v.tracks[t].pieces[k] != base->v.tracks[t].pieces[k - 1]);
	}
	CHECK(base->v.tracks[f.idx.at("A1")].pieces == std::vector<int>({2}));
	/* The crossing sits on a piece of the same heading holding the meeting point. */
	for (const Conformity &c : ConformitiesOf(f)) {
		const std::vector<Points> &pts = base->points;
		const uint32_t pa = base->v.tracks[c.a].event_piece[0], pb = base->v.tracks[c.b].event_piece[0];
		CHECK(SegmentsMeetInOnePoint(pts[c.a][pa], pts[c.a][pa + 1], pts[c.b][pb], pts[c.b][pb + 1]));
	}
	/* Runs with no representation on the pieces are another shape. */
	Geometry g = f.base;
	g["B1"] = {{2, 4}, {3, 2}, {4, 2}, {5, 2}, {6, 4}, {8, 4}};
	CHECK(!ContractedVariant(f, g).has_value());
}

TEST_CASE("planar draw: grid points lie on edge midpoints", "[junction][planar][planardraw]")
{
	/* X1's base with every point of every track shifted by (1, 0): every point's x + y parity flips, so a point is on an edge midpoint no longer and a `grid point` row breaks in the program check with grid on (F4, D428). */
	X1Fixture f;
	for (auto &[name, start] : f.starts) start[0] += 1;
	const std::optional<Contracted> cv = ContractedVariant(f, f.base);
	REQUIRE(cv.has_value());
	PlanarShapeVariant plain = cv->v;
	for (PlanarShapeTrack &track : plain.tracks) track.steps.clear();
	PlanarDrawOptions o = f.options;
	o.strict_span = o.strict_zero = o.crossing_flanks = false;
	o.keep_apart_out = true;
	o.program_out = true;
	o.build_only = true;
	o.grid = true;
	const PlanarDrawResult out = CheckPlanarDrawable(plain, 5.0, o);
	/* The program has a `grid point <track> <k>` row for the points, and the shifted base violates it (x + y even); the program check breaks, through the row's h, which no other row fixes. */
	size_t grid_rows = 0, violated = 0;
	for (const PlanarProgram::Row &r : out.program.rows) {
		if (!r.tag.starts_with("grid point ")) continue;
		grid_rows++;
		std::istringstream in(r.tag.substr(11));
		std::string track;
		size_t k = 0;
		in >> track >> k;
		const Point &p = cv->points.at(f.idx.at(track)).at(k);
		if (((p[0] + p[1]) % 2) == 0) violated++;
	}
	const std::vector<std::string> broken = CheckPlanarProgramOnPoints(out.program, cv->points).broken;
	INFO("grid point rows " << grid_rows << ", violated " << violated << ", broken " << broken.size() << ", first " << (broken.empty() ? std::string("-") : broken[0]));
	CHECK(grid_rows > 0);
	CHECK(violated == grid_rows);
	CHECK(!broken.empty());
}

TEST_CASE("planar domain: neighbourhood configurations cover X1's events", "[junction][planar][planardraw]")
{
	const X1Fixture f;
	const auto &map = f.v.structure.map;
	size_t expected = 3 * map.crossings.size();
	for (uint32_t node = 0; node < map.expanded.nodes.size(); node++) {
		for (const auto &line : map.expanded.lines) {
			expected += (!line.from.port && line.from.index == node ? 1 : 0) + (!line.to.port && line.to.index == node ? 1 : 0);
		}
	}
	const std::vector<Config> configs = NeighbourhoodConfigs(f, 3);
	CHECK(expected > 0);
	REQUIRE(configs.size() == expected);
	const std::vector<Points> base_points = f.PointsOf(f.base);
	for (const Config &c : configs) {
		INFO(c.name);
		CHECK(IsNeighbourhood(c.name));
		/* Two absorbers per varied track, of non-parallel headings, none inside the window. */
		const size_t tracks = c.name.find('+') != std::string::npos ? 2 : 1;
		REQUIRE(c.absorbers.size() == 2 * tracks);
		for (size_t i = 0; i < c.absorbers.size(); i += 2) {
			CHECK(c.absorbers[i].first == c.absorbers[i + 1].first);
			const std::vector<int> &hs = f.heads.at(c.absorbers[i].first);
			CHECK(((hs[c.absorbers[i].second] % 8) + 8) % 4 != ((hs[c.absorbers[i + 1].second] % 8) + 8) % 4);
		}
		/* Every axis is within k, none is an absorber, and each axis's base-delta-0 value keeps the track's end: the base values absorb to the base lengths. */
		CHECK(c.axes.size() <= 3 * tracks);
		for (const auto &[name, piece, vals] : c.axes) {
			for (const auto &[track, absorber] : c.absorbers) CHECK((track != name || absorber != piece));
		}
		/* First values, last values and the base values: wherever the absorbers have a length, the far ends are the base's. */
		for (int mode = 0; mode < 3; mode++) {
			Geometry g = f.base;
			for (const auto &[name, piece, vals] : c.axes) {
				if (mode == 0) g[name][piece] = vals.front();
				if (mode == 1) g[name][piece] = vals.back();
			}
			if (mode == 2) {
				REQUIRE(AbsorbGeometry(f, base_points, c, g));
				CHECK(f.PointsOf(g) == base_points);
			} else if (!AbsorbGeometry(f, base_points, c, g)) {
				continue;
			}
			const std::vector<Points> points = f.PointsOf(g);
			for (const auto &[track, absorber] : c.absorbers) CHECK(points[f.idx.at(track)].back() == base_points[f.idx.at(track)].back());
		}
	}
}

TEST_CASE("planar domain: the box width and height are group box, grid halves group half", "[junction][planar][planardraw]")
{
	const X1Fixture f;
	PlanarDrawOptions o = f.options;
	o.strict_span = o.strict_zero = o.crossing_flanks = false;
	o.keep_apart_out = true;
	o.program_out = true;
	o.build_only = true;
	o.grid = true;
	const PlanarDrawResult r = CheckPlanarDrawable(f.v, 5.0, o);
	REQUIRE(r.status == "built");
	const auto &group = r.program.group;
	CHECK(std::ranges::count(group, "box") == 2);
	CHECK(std::ranges::count(group, "half") >= 1);
	/* The box rows name the box variables. */
	size_t box_terms = 0;
	for (const PlanarProgram::Row &row : r.program.rows) {
		if (row.tag != "box" && !row.tag.starts_with("box ")) continue;
		for (const auto &term : row.terms) box_terms += group[term.first] == "box";
	}
	CHECK(box_terms > 0);
}

TEST_CASE("planar draw: the program's lead rows bind on the sibling's heading (F1)", "[junction][planar][planardraw]")
{
	/* M1's too-strict example: A1 arrives at the merge on an S-bend toward its sibling with no flank, but its heading differs from B1's there (a Y): no lead row fails. */
	const M1Fixture f;
	Geometry g = f.base;
	g["A1"] = {{0, 6}, {1, 2}, {2, 2}, {3, 2}, {4, 0}};
	const std::optional<Contracted> cv = ContractedVariant(f, g);
	REQUIRE(cv.has_value());
	for (const PlanarPointRow &r : PlanarNodeLeadRows(cv->v)) {
		INFO(r.tag);
		CHECK(PlanarPointRowHolds(r, cv->points));
	}
}

TEST_CASE("planar draw: a node-to-out-port line keeps its least lengths (F2)", "[junction][planar][planardraw]")
{
	/* X1's first F2 too-loose example: C1 = one 2-long east run (heading 0), A1 and the rest as base. C1 leaves a node for an out port, so D412 does not free its pieces: its run is on the grid rows, and its off-centre-line points break an integer row. */
	const X1Fixture f;
	Geometry g = f.base;
	g["C1"] = {{9, 2}};
	const std::optional<Contracted> cv = ContractedVariant(f, g);
	REQUIRE(cv.has_value());
	PlanarShapeVariant plain = cv->v;
	for (PlanarShapeTrack &track : plain.tracks) track.steps.clear();
	PlanarDrawOptions o = f.options;
	o.strict_span = o.strict_zero = o.crossing_flanks = false;
	o.keep_apart_out = true;
	o.program_out = true;
	o.build_only = true;
	o.grid = true;
	const PlanarDrawResult r = CheckPlanarDrawable(plain, 5.0, o);
	const std::vector<std::string> broken = CheckPlanarProgramOnPoints(r.program, cv->points).broken;
	bool integer = false;
	for (const std::string &b : broken) integer = integer || b.starts_with("integer");
	INFO("broken " << broken.size() << ", first " << (broken.empty() ? std::string("-") : broken[0]));
	CHECK(integer);
}

TEST_CASE("planar draw: a D412 port line off its centre line is rejected under port_zero (F5, D434)", "[junction][planar][planardraw]")
{
	/* F5: X1's in-port line AC1 = diagonal 1, east 2, diagonal 1 (its east run off the centre line). D412 frees every piece of it, so without port_zero nothing breaks; under port_zero the polyline's own travel (4) makes AC1 an ordinary line and its run is on the grid rows. */
	const X1Fixture f;
	Geometry g = f.base;
	g["AC1"] = {{1, 1}, {2, 2}, {3, 1}};
	const std::optional<Contracted> cv = ContractedVariant(f, g);
	REQUIRE(cv.has_value());
	PlanarShapeVariant plain = cv->v;
	for (PlanarShapeTrack &track : plain.tracks) track.steps.clear();
	const std::vector<std::vector<double>> from = PortZeroFrom(cv->points);
	for (const bool on : {false, true}) {
		PlanarDrawOptions o = f.options;
		o.strict_span = o.strict_zero = o.crossing_flanks = false;
		o.keep_apart_out = true;
		o.program_out = true;
		o.build_only = true;
		o.grid = true;
		o.port_zero = on;
		o.port_zero_from = &from;
		const std::vector<std::string> broken = CheckPlanarProgramOnPoints(CheckPlanarDrawable(plain, 5.0, o).program, cv->points).broken;
		bool integer = false;
		for (const std::string &b : broken) integer = integer || b.starts_with("integer");
		INFO("port_zero " << on << ", broken " << broken.size() << ", first " << (broken.empty() ? std::string("-") : broken[0]));
		if (on) {
			CHECK(integer);
		} else {
			CHECK(broken.empty());
		}
	}
}

TEST_CASE("planar draw: a port row broken under grid is blamed on the port row, not the grid link (domain-27)", "[junction][planar][planardraw]")
{
	/* K1's first [grid] too-strict example: A1:1*1,0*6,;ABC1:2*4,;B1:2*12,;BC1:2*4,;C1:3*1,4*6,; (A1 ends short of the box edge, so it is off interface). */
	const K1Fixture f;
	Geometry g = f.base;
	g["A1"] = {{0, 1}, {9, 6}};
	g["C1"] = {{0, 1}, {9, 6}};
	const std::optional<Contracted> cv = ContractedVariant(f, g);
	REQUIRE(cv.has_value());
	PlanarShapeVariant plain = cv->v;
	for (PlanarShapeTrack &track : plain.tracks) track.steps.clear();
	const std::vector<std::vector<double>> from = PortZeroFrom(cv->points);
	PlanarDrawOptions o = f.options;
	o.strict_span = o.strict_zero = o.crossing_flanks = false;
	o.keep_apart_out = true;
	o.program_out = true;
	o.build_only = true;
	o.grid = true;
	o.port_zero = true;
	o.port_zero_from = &from;
	const std::vector<std::string> broken = CheckPlanarProgramOnPoints(CheckPlanarDrawable(plain, 5.0, o).program, cv->points).broken;
	bool port = false, link = false;
	std::string all;
	for (const std::string &b : broken) {
		port = port || (b.starts_with("port ") && b.find(" edge") != std::string::npos);
		link = link || b.starts_with("grid link");
		all += b + "; ";
	}
	INFO("broken: " << all);
	CHECK(port);
	CHECK(!link);
}

TEST_CASE("planar draw: the program's grid rows reject an off-grid drawing", "[junction][planar][planardraw]")
{
	/* X1's base shifted by one half tile in x on every track: every corner is off the grid. */
	X1Fixture f;
	for (auto &[name, start] : f.starts) start[0] += 1;
	f.options.skip_rows = {"port "}; /* The shift also moves the ports off their box edges; this test judges the grid rows only. */
	const std::vector<Points> points = f.PointsOf(f.base);
	bool all_off = true;
	for (const Points &p : points) all_off = all_off && !RasterPlanarDomainTrack(p).rule.empty();
	REQUIRE(all_off);
	/* The program read-out of the contracted writing (every piece has length, so the grid rows are written): with grid on it breaks an integer or grid row; with grid off it breaks none. */
	const std::optional<Contracted> cv = ContractedVariant(f, f.base);
	REQUIRE(cv.has_value());
	PlanarShapeVariant plain = cv->v;
	for (PlanarShapeTrack &track : plain.tracks) track.steps.clear();
	const auto program_of = [&](bool grid) {
		PlanarDrawOptions o = f.options;
		o.strict_span = o.strict_zero = o.crossing_flanks = false;
		o.keep_apart_out = true;
		o.program_out = true;
		o.build_only = true;
		o.grid = grid;
		return CheckPlanarDrawable(plain, 5.0, o);
	};
	const PlanarDrawResult on_out = program_of(true);
	const std::vector<std::string> broken_on = CheckPlanarProgramOnPoints(on_out.program, cv->points).broken;
	bool grid_row = false;
	for (const std::string &b : broken_on) grid_row = grid_row || b.starts_with("integer") || b.starts_with("grid");
	INFO("grid forced " << on_out.grid_forced << ", broken with grid " << broken_on.size() << ", first " << (broken_on.empty() ? std::string("-") : broken_on[0]));
	CHECK(grid_row);
	CHECK(CheckPlanarProgramOnPoints(program_of(false).program, cv->points).broken.empty());
}

TEST_CASE("planar draw: a tunnel's `end 0 node` row breaks where a bridge's holds (BR1)", "[junction][planar][planardraw]")
{
	/* BR1's B1 crosses A1 at its third tile (x = 5, by a NW step before and a NE step after): the bridge head is the second tile, just off the split's switch tile. The same polyline on the structure as a tunnel with the flat-ground ramp of D155 (before D437: kind tunnel, end run set to 2) puts its portal on the switch tile: the rows' `end 0 node` row breaks and the domain rejects. */
	const BridgeFixture f;
	Geometry g = f.base;
	g["B1"] = {{0, 2}, {1, 2}, {4, 6}, {7, 2}, {8, 4}};
	BridgeFixture tunnel = f;
	tunnel.v.structure.structures[0].kinds = 2;
	tunnel.v.structure.structures[0].limit[0] = -1;
	tunnel.v.structure.structures[0].limit[1] = 10;
	tunnel.v.structures[0].end_run[1] = 2;
	const std::vector<std::string> broken = BrokenOnOwnBranches(tunnel, g, true, true);
	CHECK(std::ranges::find(broken, "end 0 node") != broken.end());
	CHECK(BrokenOnOwnBranches(f, g, true, true).empty());
}

TEST_CASE("planar draw: a one-half-tile reference makes a zero D412 port line ordinary in the program (D434)", "[junction][planar][planardraw]")
{
	/* BR1's base has AC1 at total length 0 with its node on the box edge. Its rows accept under port_zero (the line is 0 where the reference travels below one half tile). With a reference travel of one half tile AC1 is an ordinary line: its pieces have least lengths, the zero line breaks them. With port_zero off D412 frees every piece, so nothing breaks. X1's and K1's ports sit away from their nodes, so only BR1 can show the zero line itself. */
	const BridgeFixture f;
	const std::vector<Points> points = f.PointsOf(f.base);
	const uint32_t ac1 = f.idx.at("AC1");
	REQUIRE(std::ranges::all_of(points[ac1], [&](const Point &p) { return p == points[ac1].front(); }));
	const std::pair<std::string, double> one{"AC1", 1.0};
	const std::vector<std::string> ordinary = BrokenOnOwnBranches(f, f.base, true, true, &one);
	CHECK(std::ranges::any_of(ordinary, [](const std::string &b) { return b.starts_with("bound "); }));
	CHECK(BrokenOnOwnBranches(f, f.base, false, true, &one).empty());
}

TEST_CASE("planar draw: the corridor family is what breaks a copy one lane further from its parent (Y1)", "[junction][planar][planardraw]")
{
	/* Y1's copy A1#9 runs one lane (2 half tiles) north of its parent. Moved one more lane north over its whole length it no longer opens and closes at its split nodes: the corridor rows break and the domain rejects, and with the corridor family off no corridor row says so. The base is accepted by both. The rows' verdict is the program's rows only (the suite's node turn check does not take a beside track two lanes out). */
	const Y1Fixture f;
	const auto is_corridor = [](const std::string &b) { return b.starts_with("corridor "); };
	CHECK_FALSE(std::ranges::any_of(BrokenOnOwnBranches(f, f.base, true, true), is_corridor));
	Y1Fixture moved = f;
	moved.starts["A1#9"][1] -= 2;
	std::vector<std::string> notes;
	CHECK_FALSE(CheckPlanarDomain(PlanarDomainInputOf(moved.v, moved.PointsOf(f.base), notes)).legal);
	CHECK(std::ranges::any_of(BrokenOnOwnBranches(moved, f.base, true, true), is_corridor));
	moved.options.corridor = false;
	CHECK_FALSE(std::ranges::any_of(BrokenOnOwnBranches(moved, f.base, true, true), is_corridor));
}

namespace {

/** The extractor's PSG 197 text file, empty when missing. */
std::string Read197File(const std::string &name)
{
	std::ifstream file(std::string(JUNCTION_TOPOLOGY_DIR) + "/../corpus/out/psg197/" + name);
	std::stringstream buffer;
	buffer << file.rdbuf();
	return buffer.str();
}

} // namespace


TEST_CASE("planar sides: how many keep-apart sides the layout forces, and whether they are always right (D431)", "[.][sidesmeasure]")
{
	const char *path_env = std::getenv("PLANAR_SIDES_REPORT");
	REQUIRE(path_env != nullptr);
	std::string text;
	const auto run = [&](const std::string &name, const X1Fixture &f, const std::vector<Config> &configs) {
		SideStats st;
		g_sides_stats = &st;
		std::array<uint32_t, 8> totals{};
		RunEncoding(f, nullptr, name, configs, "", DrawingProgramJudge(DEFAULT_RULES), nullptr, &totals);
		g_sides_stats = nullptr;
		text += SidesText(name, st);
		std::cout << SidesText(name, st) << std::flush;
	};
	{ const X1Fixture f; run("X1", f, X1Configs(f)); }
	{ const X2Fixture f; RequireBaseLegal(f); run("X2", f, X2Configs(f)); }
	{ const M1Fixture f; RequireBaseLegal(f); run("M1", f, M1Configs(f)); }
	{ const K1Fixture f; RequireBaseLegal(f); run("K1", f, K1Configs(f)); }
	{ const L1Fixture f; RequireBaseLegal(f); run("L1", f, L1Configs(f)); }
	{ const Y1Fixture f; RequireBaseLegal(f); run("Y1", f, Y1Configs(f)); }
	{ const P1Fixture f; RequireBaseLegal(f); run("P1", f, P1Configs(f)); }
	{ const BridgeFixture f; RequireBaseLegal(f); run("BR1", f, BR1Configs(f)); }
	{ const TT1Fixture f; RequireBaseLegal(f); run("TT1", f, StackConfigs(f)); }
	{ const TS2Fixture f; RequireBaseLegal(f); run("TS2", f, StackConfigs(f)); }
	std::ofstream(path_env) << text;
}

TEST_CASE("planar cells: convex cells over contracted variants, judged by the suite (cells-1, D385 revisited)", "[.][cellsmeasure]")
{
	const char *path_env = std::getenv("PLANAR_CELLS_REPORT");
	REQUIRE(path_env != nullptr);
	std::string text;
	const auto run = [&](const std::string &name, const X1Fixture &f, const std::vector<Config> &configs) {
		CellStats st;
		std::array<uint32_t, 8> totals{};
		g_cells_stats = &st;
		g_cells_name = name;
		RunEncoding(f, nullptr, name, configs, "", DrawingProgramJudge(DEFAULT_RULES), nullptr, &totals);
		g_cells_stats = nullptr;
		text += CellsText(name, st);
		std::cout << CellsText(name, st) << std::flush;
	};
	{ const X1Fixture f; run("X1", f, X1Configs(f)); }
	{ const X2Fixture f; RequireBaseLegal(f); run("X2", f, X2Configs(f)); }
	{ const M1Fixture f; RequireBaseLegal(f); run("M1", f, M1Configs(f)); }
	{ const K1Fixture f; RequireBaseLegal(f); run("K1", f, K1Configs(f)); }
	{ const L1Fixture f; RequireBaseLegal(f); run("L1", f, L1Configs(f)); }
	{ const Y1Fixture f; RequireBaseLegal(f); run("Y1", f, Y1Configs(f)); }
	{ const P1Fixture f; RequireBaseLegal(f); run("P1", f, P1Configs(f)); }
	{ const BridgeFixture f; RequireBaseLegal(f); run("BR1", f, BR1Configs(f)); }
	{ const TT1Fixture f; RequireBaseLegal(f); run("TT1", f, StackConfigs(f)); }
	{ const TS2Fixture f; RequireBaseLegal(f); run("TS2", f, StackConfigs(f)); }
	std::ofstream(path_env) << text;
}

TEST_CASE("planar domain: V's PSG 197 judged by the behaviour (answer key)", "[.][planardomain197]")
{
	const char *report_env = std::getenv("PLANAR_DOMAIN_REPORT_197");
	const std::filesystem::path report_path = report_env != nullptr ? report_env : "docs/junction-tool/wip/probes/domain/answerkey-197.txt";
	std::ostringstream out;

	/* 1. Load 197 (topology, structure, shape, flex), no cell refinement. */
	std::string error, why;
	const PlanarTopology topology = ParsePlanarTopology(Read197File("topology.txt"), &error);
	REQUIRE(error.empty());
	const JunctionSpec spec = MakeTestJunctionSpec();
	std::vector<std::string> report;
	const PlanarStructureVariant structure = LoadPlanarStructureVariant(topology, Read197File("structure.txt"), spec, report, &error);
	REQUIRE(error.empty());
	PlanarShapeVariant shaped;
	REQUIRE(LoadPlanarShapeVariant(topology, structure, Read197File("shape.txt"), shaped, why));
	const PlanarTrackMap &map = shaped.structure.map;
	const size_t nt = shaped.tracks.size();
	const PlanarDrawPins pins = ParsePlanarDrawPins(shaped, Read197File("pins.txt"));

	/* 2. V's points per track: start pin, then V's piece lengths along the raw headings. */
	std::map<std::string, std::vector<int>> lengths;
	{
		std::istringstream in(Read197File("lengths.txt"));
		std::string text;
		while (std::getline(in, text)) {
			if (!text.starts_with("lengths ")) continue;
			const size_t colon = text.find(':');
			std::istringstream ls(text.substr(colon + 1));
			std::vector<int> &l = lengths[text.substr(8, colon - 8)];
			int x;
			while (ls >> x) l.push_back(x);
		}
	}
	/* The raw shape: shape.txt's headings and event pieces, each stretch that steps.txt lists replaced by its raw heading list (consecutive stretches share their event piece, kept once). */
	std::map<std::pair<std::string, size_t>, std::vector<int>> step_lists;
	{
		std::istringstream in(Read197File("steps.txt"));
		std::string text;
		while (std::getline(in, text)) {
			if (!text.starts_with("step ")) continue;
			const size_t hash = text.find(" # ");
			std::istringstream ls(text.substr(5, hash == std::string::npos ? std::string::npos : hash - 5));
			std::string name;
			size_t k;
			char colon;
			ls >> name >> k >> colon;
			std::vector<int> &h = step_lists[{name, k}];
			int x;
			while (ls >> x) h.push_back(x);
		}
	}
	PlanarShapeVariant raw = shaped;
	std::map<size_t, std::string> unmapped; ///< Track to reason.
	for (size_t t = 0; t < nt; t++) {
		const std::string &name = map.tracks[t].name;
		const PlanarShapeTrack &track = shaped.tracks[t];
		std::string reason;
		if (map.tracks[t].kind != PlanarTrack::Kind::Line) reason = "not a line";
		else if (!lengths.contains(name)) reason = "no lengths.txt entry";
		if (reason.empty()) {
			const std::vector<int> &h = track.pieces;
			const std::vector<uint32_t> &ev = track.event_piece;
			std::vector<int> pieces;
			std::vector<uint32_t> events;
			for (size_t k = 0; k <= ev.size(); k++) {
				const size_t first = k == 0 ? 0 : ev[k - 1], last = k == ev.size() ? h.size() - 1 : ev[k];
				std::vector<int> list;
				const auto it = step_lists.find({name, k});
				if (it != step_lists.end()) {
					list = it->second;
					const int offset = h[first] - list[0];
					for (int &x : list) x += offset;
				} else {
					list.assign(h.begin() + first, h.begin() + last + 1);
				}
				if (k > 0) list.erase(list.begin());
				pieces.insert(pieces.end(), list.begin(), list.end());
				if (k < ev.size()) events.push_back(static_cast<uint32_t>(pieces.size() - 1));
			}
			raw.tracks[t].pieces = pieces;
			raw.tracks[t].event_piece = events;
			raw.tracks[t].flex.clear();
			raw.tracks[t].steps.clear();
			if (pieces.size() != lengths[name].size()) reason = std::format("raw heading list has {} pieces, lengths.txt {}", pieces.size(), lengths[name].size());
		}
		if (reason.empty() && std::ranges::none_of(pins.pins, [&](const PlanarDrawPins::Pin &p) { return !p.cross && p.track == t && p.start; })) reason = "no start pin";
		if (!reason.empty()) unmapped[t] = reason;
	}
	out << "tracks " << nt << " mapped " << nt - unmapped.size() << " unmapped " << unmapped.size() << "\n";
	for (const auto &[t, reason] : unmapped) out << "unmapped " << t << " " << map.tracks[t].name << ": " << reason << "\n";
	const auto finish = [&]() {
		std::filesystem::create_directories(report_path.parent_path());
		std::ofstream(report_path) << out.str();
		INFO(out.str());
	};
	if (!unmapped.empty()) {
		finish();
		FAIL("tracks not carried by the raw variant");
	}

	/* The box's low corner moves to 0 (the existing 197 test's rule): a W or N port's end pins hold the edge, else the least pin. */
	std::array<int, 2> low = {INT_MAX, INT_MAX};
	std::array<bool, 2> edge_port = {false, false};
	for (size_t p = 0; p < map.expanded.ports.size(); p++) {
		const PlanarSide side = map.expanded.ports[p].side;
		if (side != PlanarSide::W && side != PlanarSide::N) continue;
		const int axis = side == PlanarSide::W ? 0 : 1;
		for (const uint32_t line : map.expanded.ports[p].lines) {
			const PlanarEnd &from = map.expanded.lines[line].from;
			const bool at_start = from.port && from.index == p;
			for (const PlanarDrawPins::Pin &pin : pins.pins) {
				if (pin.cross || pin.track != line || pin.start != at_start) continue;
				low[axis] = std::min(low[axis], pin.at[axis]);
				edge_port[axis] = true;
			}
		}
	}
	for (int axis = 0; axis < 2; axis++) {
		if (edge_port[axis]) continue;
		for (const PlanarDrawPins::Pin &pin : pins.pins) low[axis] = std::min(low[axis], pin.at[axis]);
	}
	for (int &c : low) c -= c & 1; // An even shift: the grid's parity (edge midpoints) is kept.
	out << "box low corner, rounded down to even (" << low[0] << ", " << low[1] << ")\n";

	/* The fixture on the raw variant: V's headings, start pins and lengths. */
	X1Fixture f(raw);
	std::map<std::string, std::vector<int>> heads;
	Geometry base;
	for (uint32_t t = 0; t < f.n; t++) {
		const std::string &key = f.Name(t);
		heads[key] = raw.tracks[t].pieces;
		const std::vector<int> &l = lengths[map.tracks[t].name];
		for (uint32_t k = 0; k < l.size(); k++) base[key][k] = l[k];
		const auto it = std::ranges::find_if(pins.pins, [&](const PlanarDrawPins::Pin &p) { return !p.cross && p.track == t && p.start; });
		f.starts[key] = {it->at[0] - low[0], it->at[1] - low[1]};
	}
	f.SetUp(heads, base);
	bool connected = false;
	f.StartsOf(f.base, &connected);
	const std::vector<Points> points = f.PointsOf(f.base);
	size_t end_mismatch = 0;
	for (const PlanarDrawPins::Pin &pin : pins.pins) {
		if (pin.cross || pin.start) continue;
		if (points[pin.track].back() != Point{pin.at[0] - low[0], pin.at[1] - low[1]}) end_mismatch++;
	}
	size_t raw_piece_count = 0;
	for (const PlanarShapeTrack &tr : raw.tracks) raw_piece_count += tr.pieces.size();
	out << "geometry connected at the nodes: " << (connected ? "yes" : "NO") << "; end pins not met: " << end_mismatch << "\n";

	/* 4. The domain on V's points. */
	std::vector<std::string> notes;
	/* The answer key reads Shape's end runs directly: a tunnel's end run is its portal alone (1 piece), as on V's build (D437). */
	const PlanarDomainVerdict domain = CheckPlanarDomain(PlanarDomainInputOf(f.v, points, notes));
	out << "domain on V's points: " << (domain.legal ? "legal" : "NOT legal") << ", " << domain.breaks.size() << " breaks, " << notes.size() << " notes\n";
	for (const std::string &m : notes) out << "  note: " << m << "\n";
	std::map<std::string, size_t> domain_by_rule;
	for (const PlanarDomainBreak &b : domain.breaks) domain_by_rule[b.rule]++;
	for (const auto &[rule, count] : domain_by_rule) out << "  domain rule " << rule << ": " << count << "\n";
	if (const char *dump_env = std::getenv("PLANAR_DOMAIN_DUMP_197"); dump_env != nullptr) {
		/* Diagnostic dump (domain-39): our rasterised tiles and every break, as JSON (shifted frame; the box low corner is reported). */
		std::ostringstream js;
		js << "{\"low\":[" << low[0] << "," << low[1] << "],\"pieces\":[";
		bool first = true;
		for (size_t t = 0; t < points.size(); t++) {
			const PlanarDomainRaster r = RasterPlanarDomainTrack(points[t]);
			for (size_t k = 0; k < r.pieces.size(); k++) {
				const PlanarDomainPiece &p = r.pieces[k];
				js << (first ? "" : ",") << "{\"track\":\"" << f.Name(static_cast<uint32_t>(t)) << "\",\"k\":" << k << ",\"x\":" << p.tile.x << ",\"y\":" << p.tile.y << ",\"piece\":" << static_cast<int>(p.track) << "}";
				first = false;
			}
		}
		js << "],\"breaks\":[";
		for (size_t i = 0; i < domain.breaks.size(); i++) {
			const PlanarDomainBreak &b = domain.breaks[i];
			js << (i == 0 ? "" : ",") << "{\"rule\":\"" << b.rule << "\",\"x\":" << b.tile.x << ",\"y\":" << b.tile.y << ",\"tracks\":[";
			for (size_t j = 0; j < b.tracks.size(); j++) js << (j == 0 ? "" : ",") << "\"" << f.Name(b.tracks[j]) << "\"";
			std::string why = b.why;
			for (char &c : why) if (c == '"' || c == '\\') c = '\'';
			js << "],\"why\":\"" << why << "\"}";
		}
		js << "]}\n";
		std::ofstream(dump_env) << js.str();
	}
	for (size_t i = 0; i < domain.breaks.size() && i < 20; i++) {
		const PlanarDomainBreak &b = domain.breaks[i];
		out << "  break " << b.rule << " tile (" << b.tile.x << "," << b.tile.y << ") tracks";
		for (const uint32_t t : b.tracks) out << " " << f.Name(t);
		out << ": " << b.why << "\n";
	}

	/* The contracted variant and the judge. */
	const std::optional<Contracted> cv = ContractedVariant(f, f.base);
	if (!cv) {
		out << "ContractedVariant: none (a run has no representation or a crossing no piece with Shape's heading)\n";
		finish();
		FAIL("no contracted variant");
	}
	size_t contracted_pieces = 0;
	for (const PlanarShapeTrack &tr : cv->v.tracks) contracted_pieces += tr.pieces.size();
	out << "counts: tracks " << nt << ", raw pieces " << raw_piece_count << ", contracted pieces " << contracted_pieces << ", unmapped 0; contraction moved " << g_turn_moved << ", turn-rejected " << g_turn_rejected << "\n";

	const RowsJudge judge = DrawingProgramJudge(DEFAULT_RULES);
	const RowsVerdict full = judge(f, *cv, true);
	const Readouts ro = g_last_readouts;
	if (const char *sides_env = std::getenv("PLANAR_SIDES_197"); sides_env != nullptr) {
		SideStats st;
		SidesAccumulate(st, f, "V's 197", ro.keep.keep_apart, cv->points);
		std::ofstream(sides_env) << std::format("domain legal {} judge accepts {}\n", domain.legal, full.accept) << SidesText("197", st);
	}
	if (const char *cells_env = std::getenv("PLANAR_CELLS_197"); cells_env != nullptr) {
		CellStats st;
		g_cells_name = "197";
		CellsAccumulate(st, f, "V's 197", *cv, domain, false, full.accept, true);
		CellsSidesAccumulate(st.c2, f, "V's 197", *cv, ro.keep.keep_apart);
		std::ofstream(cells_env) << std::format("domain legal {} judge accepts {}\n", domain.legal, full.accept) << CellsText("197", st);
	}
	struct Refusal {
		size_t count = 0;
		std::vector<std::string> first;
	};
	std::map<std::string, Refusal> refusals;
	std::map<std::string, size_t> written;
	const auto name_list = [&](const std::vector<uint32_t> &ts) {
		std::string s;
		for (const uint32_t t : ts) s += (s.empty() ? "" : ",") + f.Name(t);
		return s;
	};
	const auto refuse = [&](const std::string &rule, const std::string &row) {
		Refusal &r = refusals[rule];
		r.count++;
		r.first.push_back(row);
	};
	for (const PlanarKeepApartPair &pair : ro.keep.keep_apart) {
		if (pair.skipped.empty()) written[RuleOf(pair.tag)]++;
		if (!PlanarKeepApartHolds(pair, cv->points)) refuse(RuleOf(pair.tag), pair.tag + " [tracks " + name_list({pair.tracks[0], pair.tracks[1]}) + "]");
	}
	for (const PlanarPointRow &row : ro.lead) {
		written[RuleOf(row.tag)]++;
		if (!PlanarPointRowHolds(row, cv->points)) {
			std::vector<uint32_t> ts;
			for (const auto &term : row.terms) ts.push_back(term.point[0]);
			std::ranges::sort(ts);
			ts.erase(std::unique(ts.begin(), ts.end()), ts.end());
			refuse(RuleOf(row.tag), row.tag + " [tracks " + name_list(ts) + "]");
		}
	}
	for (const PlanarProgram::Row &row : ro.program.program.rows) written[RuleOf(row.tag)]++;
	for (const std::string &tag : CheckPlanarProgramOnPoints(ro.program.program, cv->points).broken) refuse(RuleOf(tag), tag);
	/* Diagnostic (domain-41): every refusing row on V's points with its terms and values, for the triage (wip/probes/domain/triage-197-judge.md). Points are in half tiles, the box low corner moved to 0; game tile = point / 2 + (78, 137). */
	const auto judge_rows = [&](const Contracted &c, const Readouts &r, const std::string &label) {
		/* Diagnostic (domain-41): every refusing row on V's points with its terms and values, for the triage (wip/probes/domain/triage-197-judge.md). Points are in half tiles, the box low corner moved to 0; game tile = point / 2 + (78, 137). */
		std::ostringstream jo;
		jo << "== " << label << "\n";
		const auto pt = [&](const std::array<uint32_t, 2> &tp) {
			const auto &q = c.points[tp[0]][tp[1]];
			return std::format("{}#{}({},{})", f.Name(tp[0]), tp[1], q[0], q[1]);
		};
		for (const PlanarKeepApartPair &pair : r.keep.keep_apart) {
			if (PlanarKeepApartHolds(pair, c.points)) continue;
			jo << pair.tag << " tracks " << f.Name(pair.tracks[0]) << "," << f.Name(pair.tracks[1]) << " pieces " << pair.pieces[0] << "," << pair.pieces[1] << " skipped '" << pair.skipped << "'\n";
			for (const auto &cand : pair.candidates) {
				jo << "  cand s(" << cand.s[0] << "," << cand.s[1] << ") spacing " << cand.spacing << ":";
				for (const auto &r : cand.rows) {
					const auto a = PlanarKeepApartEnd(r, false, c.points), b = PlanarKeepApartEnd(r, true, c.points);
					jo << " [" << pt(r.q) << "-" << pt(r.p) << " -> " << cand.s[0] * (b[0] - a[0]) + cand.s[1] * (b[1] - a[1]) << "]";
				}
				jo << "\n";
			}
		}
		for (const PlanarPointRow &row : r.lead) {
			if (PlanarPointRowHolds(row, c.points)) continue;
			long sum = 0;
			jo << row.tag << ":";
			for (const auto &term : row.terms) {
				const auto &q = c.points[term.point[0]][term.point[1]];
				sum += term.c[0] * q[0] + term.c[1] * q[1];
				jo << " c(" << term.c[0] << "," << term.c[1] << ")*" << pt(term.point);
			}
			jo << " sum " << sum << " least " << row.least << "\n";
		}
		/* The program on V's points: solve the determined variables (equality rows with one unknown, or two rows sharing the same two unknowns), then evaluate each row of a refusing tag. */
		const PlanarProgram &prog = r.program.program;
		const size_t nv = prog.lower.size();
		std::vector<double> val(nv, 0);
		std::vector<bool> known(nv, false);
		for (size_t t = 0; t < prog.point.size() && t < c.points.size(); t++) {
			for (size_t i = 0; i < prog.point[t].size() && i < c.points[t].size(); i++) {
				for (int axis = 0; axis < 2; axis++) {
					val[prog.point[t][i][axis]] = c.points[t][i][axis];
					known[prog.point[t][i][axis]] = true;
				}
			}
		}
		for (bool changed = true; changed;) {
			changed = false;
			std::map<std::pair<uint32_t, uint32_t>, std::vector<size_t>> pairs;
			for (size_t ri = 0; ri < prog.rows.size(); ri++) {
				const auto &r = prog.rows[ri];
				if (r.lower != r.upper) continue;
				std::vector<uint32_t> unk;
				for (const auto &[x, c] : r.terms) if (!known[x] && c != 0 && std::ranges::find(unk, x) == unk.end()) unk.push_back(x);
				if (unk.size() == 1) {
					double rest = r.lower, coef = 0;
					for (const auto &[x, c] : r.terms) { if (x == unk[0]) coef += c; else rest -= c * val[x]; }
					val[unk[0]] = rest / coef;
					known[unk[0]] = true;
					changed = true;
				} else if (unk.size() == 2) {
					std::ranges::sort(unk);
					pairs[{unk[0], unk[1]}].push_back(ri);
				}
			}
			if (changed) continue;
			for (const auto &[ab, rs] : pairs) {
				if (rs.size() < 2) continue;
				double c1[2] = {0, 0}, c2[2] = {0, 0}, r1 = prog.rows[rs[0]].lower, r2 = prog.rows[rs[1]].lower;
				for (const auto &[x, c] : prog.rows[rs[0]].terms) { if (x == ab.first) c1[0] += c; else if (x == ab.second) c1[1] += c; else r1 -= c * val[x]; }
				for (const auto &[x, c] : prog.rows[rs[1]].terms) { if (x == ab.first) c2[0] += c; else if (x == ab.second) c2[1] += c; else r2 -= c * val[x]; }
				const double det = c1[0] * c2[1] - c2[0] * c1[1];
				if (std::abs(det) < 1e-9) continue;
				val[ab.first] = (r1 * c2[1] - r2 * c1[1]) / det;
				val[ab.second] = (c1[0] * r2 - c2[0] * r1) / det;
				known[ab.first] = known[ab.second] = true;
				changed = true;
				break;
			}
		}
		const std::set<std::string> broken = [&]() {
			const auto b = CheckPlanarProgramOnPoints(prog, c.points).broken;
			return std::set<std::string>(b.begin(), b.end());
		}();
		for (const PlanarProgram::Row &r : prog.rows) {
			if (!broken.contains(r.tag)) continue;
			double sum = 0;
			bool all = true;
			std::string terms;
			for (const auto &[x, c] : r.terms) {
				all = all && known[x];
				sum += c * val[x];
				terms += std::format(" {}*{}[{}{}]", c, val[x], prog.group[x], known[x] ? "" : " UNKNOWN");
			}
			if (all && sum >= r.lower - 1e-6 && sum <= r.upper + 1e-6) continue;
			jo << r.tag << ":" << terms << " sum " << sum << " in [" << r.lower << ", " << r.upper << "]\n";
			if (r.tag.starts_with("deck ") || r.tag.starts_with("order ")) {
				/* The crossings whose parameters are the row's variables (the `cross <c>` rows that hold them, through the grid link). */
				for (const auto &[x, coef] : r.terms) {
					jo << "    var " << x << " in:";
					for (const auto &other : prog.rows) {
						if (!other.tag.starts_with("cross ") && !other.tag.starts_with("grid link")) continue;
						if (!std::ranges::any_of(other.terms, [&](const auto &t) { return t.first == x; })) continue;
						jo << " {" << other.tag;
						if (other.tag.starts_with("cross ") && other.tag.find("piece") == std::string::npos) {
							uint32_t cr = UINT32_MAX;
							std::from_chars(other.tag.data() + 6, other.tag.data() + other.tag.size(), cr);
							if (cr < c.v.structure.map.crossings.size()) jo << " = " << f.Name(c.v.structure.map.crossings[cr].a) << "x" << f.Name(c.v.structure.map.crossings[cr].b);
						}
						jo << "}";
					}
					jo << "\n";
				}
			}
		}
		for (size_t x = 0; x < nv; x++) {
			if (!known[x]) continue;
			const bool low = val[x] < prog.lower[x] - 1e-6 || val[x] > prog.upper[x] + 1e-6;
			const bool frac = prog.integer[x] && std::abs(val[x] - std::round(val[x])) > 1e-6;
			if (!low && !frac) continue;
			if (broken.contains((low ? "bound " : "integer ") + prog.group[x]) || broken.contains("integer " + prog.group[x]) || broken.contains("bound " + prog.group[x])) {
				jo << "var " << x << " group " << prog.group[x] << " value " << val[x] << (low ? " out of bounds" : " not integer") << " [" << prog.lower[x] << ", " << prog.upper[x] << "]; in rows:";
				for (const auto &r : prog.rows) if (std::ranges::any_of(r.terms, [&](const auto &t) { return t.first == x; })) jo << " {" << r.tag << "}";
				jo << "\n";
			}
		}
		for (const uint32_t n : {47u, 61u}) {
			jo << "node " << n << " lines";
			for (uint32_t t = 0; t < c.v.structure.map.expanded.lines.size(); t++) {
				const PlanarLine &ln = c.v.structure.map.expanded.lines[t];
				if ((!ln.from.port && ln.from.index == n) || (!ln.to.port && ln.to.index == n)) jo << " " << f.Name(t) << ((!ln.from.port && ln.from.index == n) ? "(from)" : "(to)");
			}
			jo << "\n";
		}
		for (size_t k = 0; k < c.v.structure.structures.size(); k++) {
			const PlanarStructure &st = c.v.structure.structures[k];
			const std::string deck = std::format("deck {}", k), end = std::format("end {}", k);
			if (!broken.contains(deck) && !broken.contains(end) && !broken.contains(end + " node")) continue;
			jo << "structure " << k << " separator " << f.Name(st.separator) << " kinds " << static_cast<int>(st.kinds) << " limit " << st.limit[0] << "," << st.limit[1] << " end_run " << c.v.structures[k].end_run[0] << "," << c.v.structures[k].end_run[1] << " crossers";
			for (const uint32_t c : st.crossers) jo << " " << f.Name(c);
			jo << "; separator points";
			for (const auto &q : c.points[st.separator]) jo << " (" << q[0] << "," << q[1] << ")";
			jo << "\n";
		}
		return jo.str();
	};
	if (const char *judge_env = std::getenv("PLANAR_DOMAIN_JUDGE_197"); judge_env != nullptr) {
		/* The answer key reads Shape's end runs directly (D437): the judge's and the key's end runs are the same, no counterfactual. */
		const std::string text = judge_rows(*cv, ro, "judge (Shape's end runs)");
		std::ofstream(judge_env) << text;
	}
	out << "judge (default rules, interface rows): " << (full.accept ? "ACCEPT" : "REFUSE") << "\n";
	for (const std::string &rule : full.rules) out << "  rule named: " << rule << "\n";
	for (const auto &[rule, r] : refusals) {
		out << "  refusing rule " << rule << ": " << r.count << " rows\n";
		for (const std::string &row : r.first) out << "    " << row << "\n";
	}
	out << "rows written per rule (V's places that exercise it: keep-apart pairs with rows, lead rows, program rows by tag family):\n";
	for (const auto &[rule, count] : written) out << "  " << rule << ": " << count << "\n";
	const RowsVerdict off = judge(f, *cv, false);
	out << "judge off-interface counterfactual (no port and box rows): " << (off.accept ? "ACCEPT" : "REFUSE") << "\n";
	for (const std::string &rule : off.rules) out << "  rule named: " << rule << "\n";

	finish();
	CHECK(true);
}

namespace {

/** TS2 diagnosis (domain-46): the base drawing program on TS2 with the options and deletion filter from the environment. */
std::string EnvText(const char *name, const char *fallback)
{
	const char *value = std::getenv(name);
	return value == nullptr ? fallback : value;
}

} // namespace

TEST_CASE("planar domain: TS2 base diagnosis", "[.][ts2diag]")
{
	const std::string ports = EnvText("TS2_PORTS", "");
	const std::string topo = "topology TS2\nport W in E" + (EnvText("TS2_W", "").empty() ? "" : " @ " + EnvText("TS2_W", "")) + ": AB1, AB2, AB3\nport N out N" + (ports.empty() ? "" : " @ " + EnvText("TS2_N", "")) + ": A1, A2, A3\nport E out E" + (ports.empty() ? "" : " @ " + EnvText("TS2_E", "")) + ": B1, B2, B3\nnode AB1 -> A1, B1\nnode AB2 -> A2, B2\nnode AB3 -> A3, B3\ncross B1: A2<, A3<\ncross B2: A3<\ncross A2: B1>\ncross A3: B2>, B1>\n";
	const std::string structure = "tunnel T1 B1: A2, A3\ntunnel T2 B2: A3\nstraight AB1: B1\nstraight AB2: B2\nstraight AB3: B3\n";
	std::string error;
	const PlanarTopology t = ParsePlanarTopology(topo, &error);
	REQUIRE(error.empty());
	const JunctionSpec spec = MakeTestJunctionSpec();
	std::vector<std::string> report;
	const PlanarStructureVariant s = LoadPlanarStructureVariant(t, structure, spec, report, &error);
	REQUIRE(error.empty());
	PlanarShapeVariant v;
	std::string why;
	REQUIRE(ShapePlanarStructureVariant(s, v, why));
	PlanarShapeVariant stripped = v;
	for (PlanarShapeTrack &track : stripped.tracks) {
		std::vector<uint32_t> middles = track.flex;
		std::ranges::sort(middles);
		for (size_t j = middles.size(); j-- > 0;) {
			const uint32_t m = middles[j];
			track.pieces.erase(track.pieces.begin() + m, track.pieces.begin() + m + 2);
			for (uint32_t &e : track.event_piece) {
				if (e > m) e -= 2;
			}
		}
		track.flex.clear();
		track.steps.clear();
	}
	std::string refine_why;
	REQUIRE(RefinePlanarShapeCells(stripped, refine_why));
	PlanarDrawOptions o;
	o.corridor = true;
	o.end_node = o.cross_piece = o.grid = true;
	o.strict_zero = o.crossing_flanks = false;
	o.node_lead = true;
	o.tidy = true;
	o.elastic = false;
	o.least_extra = EnvText("TS2_EXTRA", "2")[0] - '0';
	const std::string off = EnvText("TS2_OFF", "");
	if (off.find("grid") != std::string::npos) o.grid = false;
	if (off.find("end_node") != std::string::npos) o.end_node = false;
	if (off.find("cross_piece") != std::string::npos) o.cross_piece = false;
	if (off.find("node_lead") != std::string::npos) o.node_lead = false;
	std::istringstream skips(EnvText("TS2_SKIP", ""));
	for (std::string tok; std::getline(skips, tok, '|');) o.skip_rows.push_back(tok);
	if (std::getenv("TS2_DUMP") != nullptr) {
		const auto wall_name = [&](const PlanarCellWall &w) {
			if (w.track == UINT32_MAX) return std::format("box{}", w.piece);
			if (w.track == PLANAR_DUMMY_TRACK) return std::format("D{}", w.piece);
			return std::format("{}.{}", stripped.structure.map.tracks[w.track].name, w.piece);
		};
		for (size_t d = 0; d < stripped.dummies.size(); d++) {
			const PlanarShapeDummy &dm = stripped.dummies[d];
			std::cout << std::format("DUMMY {} face {} from {}.{} heading {} lands {}\n", d, dm.face, stripped.structure.map.tracks[dm.track].name, dm.piece, dm.heading, wall_name({dm.land_track, dm.land_piece, 0}));
		}
		const auto cells = PlanarShapeCellWalls(stripped);
		for (size_t c = 0; c < cells.size(); c++) {
			std::string text = std::format("CELL {}:", c);
			for (const PlanarCellWall &w : cells[c]) text += std::format(" {}({})", wall_name(w), w.heading);
			std::cout << text << "\n";
		}
		for (const PlanarShapeTrack &track : stripped.tracks) {
			std::string text = "PIECES";
			for (const int x : track.pieces) text += std::format(" {}", x);
			std::cout << text << "\n";
		}
	}
	const PlanarDrawResult r = CheckPlanarDrawable(stripped, 15.0, o);
	std::string line = std::format("TS2DIAG status {} ms {} conflicts:", r.status, static_cast<int>(r.ms));
	for (const std::string &c : r.conflict) line += " [" + c + "]";
	std::cout << line << "\n";
}

/** Shape-2 probe, steps 2 and 3: TS2's Shape variant and the 45-degree variant, drawn at the default port spacing. */
TEST_CASE("planar shape: TS2 at 45 degrees", "[.][shapeprobe]")
{
	const std::string topo = "topology TS2\nport W in E: AB1, AB2, AB3\nport N out N: A1, A2, A3\nport E out E: B1, B2, B3\nnode AB1 -> A1, B1\nnode AB2 -> A2, B2\nnode AB3 -> A3, B3\ncross B1: A2<, A3<\ncross B2: A3<\ncross A2: B1>\ncross A3: B2>, B1>\n";
	const std::string structure = "tunnel T1 B1: A2, A3\ntunnel T2 B2: A3\nstraight AB1: B1\nstraight AB2: B2\nstraight AB3: B3\n";
	std::string error;
	const PlanarTopology t = ParsePlanarTopology(topo, &error);
	REQUIRE(error.empty());
	const JunctionSpec spec = MakeTestJunctionSpec();
	std::vector<std::string> report;
	const PlanarStructureVariant s = LoadPlanarStructureVariant(t, structure, spec, report, &error);
	REQUIRE(error.empty());
	PlanarShapeVariant shaped;
	std::string why;
	REQUIRE(ShapePlanarStructureVariant(s, shaped, why));
	/* The base variant, flex-stripped as DrawnFixture does. */
	PlanarShapeVariant base = shaped;
	for (PlanarShapeTrack &track : base.tracks) {
		std::vector<uint32_t> middles = track.flex;
		std::ranges::sort(middles);
		for (size_t j = middles.size(); j-- > 0;) {
			const uint32_t m = middles[j];
			track.pieces.erase(track.pieces.begin() + m, track.pieces.begin() + m + 2);
			for (uint32_t &e : track.event_piece) {
				if (e > m) e -= 2;
			}
		}
		track.flex.clear();
		track.steps.clear();
	}
	const auto describe = [&](const char *label, const PlanarShapeVariant &x) {
		const PlanarTrackMap &map = x.structure.map;
		std::cout << "VARIANT " << label << "\n";
		for (uint32_t k = 0; k < x.tracks.size(); k++) {
			std::string line = std::format("  {}:", map.tracks[k].name);
			for (const int h : x.tracks[k].pieces) line += std::format(" {}", h);
			line += "; crossings:";
			for (size_t e = 0; e < map.events[k].size(); e++) {
				const auto &cr = map.crossings[map.events[k][e]];
				line += std::format(" {}@{}", map.tracks[cr.a == k ? cr.b : cr.a].name, x.tracks[k].event_piece[e]);
			}
			std::cout << line << "\n";
		}
		for (size_t c = 0; c < map.crossings.size(); c++) std::cout << std::format("  crossing {} x {}: angle {} eighths\n", map.tracks[map.crossings[c].a].name, map.tracks[map.crossings[c].b].name, static_cast<int>(x.angles[c]));
	};
	describe("base (Shape, flex stripped)", base);
	/* The 45-degree variant: A2 and A3 cross at heading 1 and turn to 0 after their last crossing. */
	PlanarShapeVariant alt;
	const std::string text = "shape AB1: 2\nshape AB2: 2\nshape AB3: 2\nshape A1: 1 0\nshape A2: 1 0\nshape A3: 1 0\nshape B1: 2\nshape B2: 2\nshape B3: 2\nat A2: B1@0\nat A3: B2@0, B1@0\nat B1: A2@0, A3@0\nat B2: A3@0\n";
	const bool loaded = LoadPlanarShapeVariant(t, s, text, alt, why);
	INFO(why);
	REQUIRE(loaded);
	describe("45 degrees", alt);
	std::cout << "ALT cost " << alt.cost << "\n";
	std::string refine_why;
	for (const std::string &m : CheckPlanarShapePromise(t, spec, alt)) std::cout << "ALT promise (before cells): " << m << "\n";
	const bool refined = RefinePlanarShapeCells(alt, refine_why);
	std::cout << std::format("ALT cells refined {} {} dummies {}\n", refined, refine_why, alt.dummies.size());
	for (const std::string &m : CheckPlanarShapePromise(t, spec, alt)) std::cout << "ALT promise: " << m << "\n";
	std::string base_why;
	const bool base_refined = RefinePlanarShapeCells(base, base_why);
	std::cout << std::format("BASE cells refined {} {} dummies {}\n", base_refined, base_why, base.dummies.size());
	for (const std::string &m : CheckPlanarShapePromise(t, spec, base)) std::cout << "BASE promise: " << m << "\n";
	PlanarDrawOptions o;
	o.corridor = true;
	o.end_node = o.cross_piece = o.grid = true;
	o.strict_zero = o.crossing_flanks = false;
	o.node_lead = true;
	o.tidy = true;
	o.elastic = false;
	o.least_extra = 0;
	const auto draw = [&](const char *label, const PlanarShapeVariant &x) {
		const PlanarDrawResult r = CheckPlanarDrawable(x, 15.0, o);
		std::string line = std::format("DRAW {} status {} ms {} rows {} columns {}", label, r.status, static_cast<int>(r.ms), r.rows, r.columns);
		if (r.status == "Optimal") {
			for (uint32_t k = 0; k < r.points.size(); k++) {
				line += std::format("\n  {}:", x.structure.map.tracks[k].name);
				for (const Point &p : r.points[k]) line += std::format(" ({},{})", p[0], p[1]);
			}
		}
		std::cout << line << "\n";
	};
	draw("base", base);
	draw("45 degrees", alt);
	/* Step 4: the conflict of the 45-degree variant, by a deletion filter over the rows of the first conflict (one solve per row). */
	int solves = 0;
	PlanarDrawResult first = CheckPlanarDrawable(alt, 15.0, o);
	solves++;
	std::vector<std::string> kept = first.conflict, dropped;
	std::cout << "CONFLICT first (" << kept.size() << "):";
	for (const std::string &c : kept) std::cout << " [" << c << "]";
	std::cout << "\n";
	for (const std::string &row : first.conflict) {
		if (solves >= 30) break;
		PlanarDrawOptions f = o;
		f.skip_rows = dropped;
		f.skip_rows.push_back(row);
		const PlanarDrawResult r = CheckPlanarDrawable(alt, 15.0, f);
		solves++;
		std::cout << std::format("FILTER skip [{}] -> {} {} ms\n", row, r.status, static_cast<int>(r.ms));
		if (r.status == "Infeasible") dropped.push_back(row);
	}
	std::cout << "MINIMAL after " << solves << " solves:";
	for (const std::string &c : first.conflict) {
		if (std::ranges::find(dropped, c) == dropped.end()) std::cout << " [" << c << "]";
	}
	std::cout << "\n";
	/* The same variant with the four optional rules off (the ts2-diag #7 setting). */
	PlanarDrawOptions bare = o;
	bare.grid = bare.end_node = bare.cross_piece = bare.node_lead = false;
	const PlanarDrawResult b = CheckPlanarDrawable(alt, 15.0, bare);
	std::cout << std::format("DRAW 45 degrees, four optional rules off: status {} ms {}\n", b.status, static_cast<int>(b.ms));
	const PlanarDrawResult b0 = CheckPlanarDrawable(base, 15.0, bare);
	std::cout << std::format("DRAW base, four optional rules off: status {} ms {}\n", b0.status, static_cast<int>(b0.ms));
}

/** Shape-3 probe: TS2 at the default spacing with flex offered on every stretch (the lattice clause off). */
TEST_CASE("planar shape: TS2 with lattice flex", "[.][shapeprobe]")
{
	const std::string topo = "topology TS2\nport W in E: AB1, AB2, AB3\nport N out N: A1, A2, A3\nport E out E: B1, B2, B3\nnode AB1 -> A1, B1\nnode AB2 -> A2, B2\nnode AB3 -> A3, B3\ncross B1: A2<, A3<\ncross B2: A3<\ncross A2: B1>\ncross A3: B2>, B1>\n";
	const std::string structure = "tunnel T1 B1: A2, A3\ntunnel T2 B2: A3\nstraight AB1: B1\nstraight AB2: B2\nstraight AB3: B3\n";
	std::string error;
	const PlanarTopology t = ParsePlanarTopology(topo, &error);
	REQUIRE(error.empty());
	const JunctionSpec spec = MakeTestJunctionSpec();
	std::vector<std::string> report;
	const PlanarStructureVariant s = LoadPlanarStructureVariant(t, structure, spec, report, &error);
	REQUIRE(error.empty());
	std::string why;
	PlanarShapeVariant base;
	REQUIRE(ShapePlanarStructureVariant(s, base, why));
	for (PlanarShapeTrack &track : base.tracks) {
		std::vector<uint32_t> middles = track.flex;
		std::ranges::sort(middles);
		for (size_t j = middles.size(); j-- > 0;) {
			const uint32_t m = middles[j];
			track.pieces.erase(track.pieces.begin() + m, track.pieces.begin() + m + 2);
			for (uint32_t &e : track.event_piece) {
				if (e > m) e -= 2;
			}
		}
		track.flex.clear();
		track.steps.clear();
	}
	PlanarShapeVariant alt;
	const std::string text = "shape AB1: 2\nshape AB2: 2\nshape AB3: 2\nshape A1: 1 0\nshape A2: 1 0\nshape A3: 1 0\nshape B1: 2\nshape B2: 2\nshape B3: 2\nat A2: B1@0\nat A3: B2@0, B1@0\nat B1: A2@0, A3@0\nat B2: A3@0\n";
	REQUIRE(LoadPlanarShapeVariant(t, s, text, alt, why));
	PlanarDrawOptions o;
	o.corridor = true;
	o.end_node = o.cross_piece = o.grid = true;
	o.strict_zero = o.crossing_flanks = false;
	o.node_lead = true;
	o.tidy = true;
	o.elastic = false;
	o.least_extra = 0;
	const auto prepare = [&](PlanarShapeVariant &x, bool lattice) {
		REQUIRE(PlacePlanarShapeFlex(x, why, true, lattice));
		std::string cells_why;
		const bool refined = RefinePlanarShapeCells(x, cells_why);
		size_t flex = 0;
		for (const PlanarShapeTrack &tr : x.tracks) flex += tr.flex.size();
		std::cout << std::format("FLEX lattice {} placed {} refined {} {} dummies {}\n", lattice, flex, refined, cells_why, x.dummies.size());
		for (uint32_t k = 0; k < x.tracks.size(); k++) {
			if (x.tracks[k].flex.empty()) continue;
			std::string line = std::format("  {} flex at", x.structure.map.tracks[k].name);
			for (const uint32_t m : x.tracks[k].flex) line += std::format(" {}", m);
			std::cout << line << "\n";
		}
		return refined;
	};
	const auto draw = [&](const char *label, const PlanarShapeVariant &x, const PlanarDrawOptions &opt) {
		const PlanarDrawResult r = CheckPlanarDrawable(x, 15.0, opt);
		size_t used = 0;
		std::string line = std::format("DRAW {} status {} ms {} rows {} columns {}", label, r.status, static_cast<int>(r.ms), r.rows, r.columns);
		if (r.status == "Optimal") {
			for (uint32_t k = 0; k < x.tracks.size(); k++) {
				for (const uint32_t m : x.tracks[k].flex) {
					if (r.lengths[k][m] > 1e-6) {
						used++;
						line += std::format("\n  insert {} piece {} heading {} length {} half tiles", x.structure.map.tracks[k].name, m, x.tracks[k].pieces[m], r.lengths[k][m]);
					}
				}
				line += std::format("\n  {}:", x.structure.map.tracks[k].name);
				for (const Point &p : r.points[k]) line += std::format(" ({},{})", p[0], p[1]);
			}
		}
		std::cout << line << "\nUSED " << label << " " << used << "\n";
		return r;
	};
	for (const bool lattice : {false, true}) {
		PlanarShapeVariant b = base, a = alt;
		if (prepare(b, lattice)) draw(lattice ? "base lattice flex" : "base flex C", b, o);
		if (prepare(a, lattice)) draw(lattice ? "45 degrees lattice flex" : "45 degrees flex C", a, o);
	}
	/* Step 3: the conflicts with the lattice clause off; at most 30 solves in all (the 90-degree first conflict, the 45-degree deletion filter). */
	int solves = 0;
	for (const bool is_alt : {false, true}) {
		PlanarShapeVariant x = is_alt ? alt : base;
		REQUIRE(prepare(x, true));
		const PlanarDrawResult first = CheckPlanarDrawable(x, 15.0, o);
		solves++;
		std::cout << (is_alt ? "CONFLICT 45 first (" : "CONFLICT 90 first (") << first.conflict.size() << "):";
		for (const std::string &c : first.conflict) std::cout << " [" << c << "]";
		std::cout << "\n";
		if (!is_alt) continue;
		std::vector<std::string> dropped;
		for (const std::string &row : first.conflict) {
			if (solves >= 30) break;
			PlanarDrawOptions f = o;
			f.skip_rows = dropped;
			f.skip_rows.push_back(row);
			const PlanarDrawResult r = CheckPlanarDrawable(x, 15.0, f);
			solves++;
			std::cout << std::format("FILTER skip [{}] -> {} {} ms\n", row, r.status, static_cast<int>(r.ms));
			if (r.status == "Infeasible") dropped.push_back(row);
		}
		std::cout << "MINIMAL 45 after " << solves << " solves:";
		for (const std::string &c : first.conflict) {
			if (std::ranges::find(dropped, c) == dropped.end()) std::cout << " [" << c << "]";
		}
		std::cout << "\n";
	}
}

/** Shape-4 probe: TS2 with flex placed, no cell refinement, keep-apart only through the pairwise rows. */
TEST_CASE("planar shape: TS2 with only suite-judged rows", "[.][shapeprobe]")
{
	const auto topo_of = [](int n) {
		std::string at;
		if (n > 0) at = std::format(" @ 1 {} {}", 1 + n, 1 + 2 * n);
		return "topology TS2\nport W in E: AB1, AB2, AB3\nport N out N" + at + ": A1, A2, A3\nport E out E: B1, B2, B3\nnode AB1 -> A1, B1\nnode AB2 -> A2, B2\nnode AB3 -> A3, B3\ncross B1: A2<, A3<\ncross B2: A3<\ncross A2: B1>\ncross A3: B2>, B1>\n";
	};
	const std::string structure = "tunnel T1 B1: A2, A3\ntunnel T2 B2: A3\nstraight AB1: B1\nstraight AB2: B2\nstraight AB3: B3\n";
	const std::string text = "shape AB1: 2\nshape AB2: 2\nshape AB3: 2\nshape A1: 1 0\nshape A2: 1 0\nshape A3: 1 0\nshape B1: 2\nshape B2: 2\nshape B3: 2\nat A2: B1@0\nat A3: B2@0, B1@0\nat B1: A2@0, A3@0\nat B2: A3@0\n";
	const JunctionSpec spec = MakeTestJunctionSpec();
	PlanarDrawOptions o;
	o.corridor = true;
	o.end_node = o.cross_piece = o.grid = true;
	o.strict_zero = o.crossing_flanks = false;
	o.node_lead = true;
	o.tidy = true;
	o.elastic = false;
	o.least_extra = 0;
	o.pairwise = o.pairwise_all_faces = true;
	/* Per spacing: the topology, both variants flex-stripped then flex placed (lattice clause as production), no cells. */
	const auto build = [&](int n, PlanarTopology &t, PlanarStructureVariant &s, PlanarShapeVariant &base, PlanarShapeVariant &alt) {
		std::string error, why;
		t = ParsePlanarTopology(topo_of(n), &error);
		REQUIRE(error.empty());
		std::vector<std::string> report;
		s = LoadPlanarStructureVariant(t, structure, spec, report, &error);
		REQUIRE(error.empty());
		REQUIRE(ShapePlanarStructureVariant(s, base, why));
		for (PlanarShapeTrack &track : base.tracks) {
			std::vector<uint32_t> middles = track.flex;
			std::ranges::sort(middles);
			for (size_t j = middles.size(); j-- > 0;) {
				const uint32_t m = middles[j];
				track.pieces.erase(track.pieces.begin() + m, track.pieces.begin() + m + 2);
				for (uint32_t &e : track.event_piece) {
					if (e > m) e -= 2;
				}
			}
			track.flex.clear();
			track.steps.clear();
		}
		REQUIRE(LoadPlanarShapeVariant(t, s, text, alt, why));
		REQUIRE(PlacePlanarShapeFlex(base, why, true));
		REQUIRE(PlacePlanarShapeFlex(alt, why, true));
	};
	const auto flex_count = [](const PlanarShapeVariant &x) {
		size_t f = 0;
		for (const PlanarShapeTrack &tr : x.tracks) f += tr.flex.size();
		return f;
	};
	const auto draw = [&](const std::string &label, const PlanarShapeVariant &x, const PlanarDrawOptions &opt) {
		const PlanarDrawResult r = CheckPlanarDrawable(x, 15.0, opt);
		size_t used = 0;
		std::string line = std::format("DRAW {} status {} ms {} rows {} columns {}", label, r.status, static_cast<int>(r.ms), r.rows, r.columns);
		if (r.status == "Optimal") {
			for (uint32_t k = 0; k < x.tracks.size(); k++) {
				for (const uint32_t m : x.tracks[k].flex) {
					if (r.lengths[k][m] > 1e-6) used++;
				}
				line += std::format("\n  {}:", x.structure.map.tracks[k].name);
				for (const Point &p : r.points[k]) line += std::format(" ({},{})", p[0], p[1]);
			}
			std::vector<std::string> notes;
			const PlanarDomainVerdict d = CheckPlanarDomain(PlanarDomainInputOf(x, r.points, notes));
			line += std::format("\n  DOMAIN legal {} notes {}", d.legal, notes.size());
			for (const std::string &m : notes) line += "\n    note: " + m;
			for (const PlanarDomainBreak &b : d.breaks) line += std::format("\n    break: {} tile ({},{}) {}", b.rule, b.tile.x, b.tile.y, b.why);
			/* The same drawing moved two half tiles right and down (a tile of margin; parity kept). */
			std::vector<std::vector<std::array<int, 2>>> moved = r.points;
			for (auto &tr : moved) {
				for (auto &q : tr) {
					q[0] += 2;
					q[1] += 2;
				}
			}
			std::vector<std::string> notes2;
			const PlanarDomainVerdict d2 = CheckPlanarDomain(PlanarDomainInputOf(x, moved, notes2));
			line += std::format("\n  DOMAIN moved (+2,+2) legal {} notes {} breaks {}", d2.legal, notes2.size(), d2.breaks.size());
			for (const PlanarDomainBreak &b : d2.breaks) line += std::format("\n    break: {} tile ({},{}) {}", b.rule, b.tile.x, b.tile.y, b.why);
		}
		std::cout << line << "\nUSED " << label << " " << used << "\n";
		return r;
	};
	PlanarTopology t;
	PlanarStructureVariant s;
	PlanarShapeVariant base, alt;
	build(0, t, s, base, alt);
	std::cout << std::format("FLEX placed base {} alt {}\n", flex_count(base), flex_count(alt));
	/* Step 1: the tag families of the program as written. */
	for (const bool is_alt : {false, true}) {
		PlanarDrawOptions b = o;
		b.program_out = b.build_only = true;
		const PlanarDrawResult r = CheckPlanarDrawable(is_alt ? alt : base, 15.0, b);
		std::map<std::string, size_t> families;
		for (const PlanarProgram::Row &row : r.program.rows) families[row.tag.substr(0, row.tag.find(' '))]++;
		std::string line = std::format("FAMILIES {} status {} rows {} dummies {}:", is_alt ? "45" : "90", r.status, r.program.rows.size(), (is_alt ? alt : base).dummies.size());
		for (const auto &[f, c] : families) line += std::format(" {}={}", f, c);
		std::cout << line << "\n";
	}
	/* Step 2. */
	const PlanarDrawResult r90 = draw("90 default", base, o);
	const PlanarDrawResult r45 = draw("45 default", alt, o);
	if (r90.status == "Optimal" || r45.status == "Optimal") return;
	/* Step 3: the smallest spacing at 45 degrees. */
	for (const int n : {1, 2, 3}) {
		PlanarShapeVariant b2, a2;
		PlanarTopology t2;
		PlanarStructureVariant s2;
		build(n, t2, s2, b2, a2);
		const PlanarDrawResult r = draw(std::format("45 spacing {}", n), a2, o);
		if (r.status == "Optimal") break;
	}
	/* The conflict at the default spacing, by family deletion (at most 20 solves). */
	for (const bool is_alt : {false, true}) {
		const PlanarShapeVariant &x = is_alt ? alt : base;
		const PlanarDrawResult first = CheckPlanarDrawable(x, 15.0, o);
		std::vector<std::string> families;
		std::cout << (is_alt ? "CONFLICT 45 first (" : "CONFLICT 90 first (") << first.conflict.size() << "):";
		for (const std::string &c : first.conflict) {
			std::cout << " [" << c << "]";
			const std::string f = c.substr(0, c.find(' '));
			if (std::ranges::find(families, f) == families.end()) families.push_back(f);
		}
		std::cout << "\n";
		int solves = 0;
		std::vector<std::string> dropped;
		for (const std::string &f : families) {
			if (solves >= 20) break;
			PlanarDrawOptions fo = o;
			fo.skip_rows = dropped;
			fo.skip_rows.push_back(f + " ");
			const PlanarDrawResult r = CheckPlanarDrawable(x, 15.0, fo);
			solves++;
			std::cout << std::format("FAMFILTER {} skip family [{}] -> {} {} ms\n", is_alt ? "45" : "90", f, r.status, static_cast<int>(r.ms));
			if (r.status == "Infeasible") dropped.push_back(f + " ");
		}
		std::cout << "FAMILIES NEEDED " << (is_alt ? "45" : "90") << " after " << solves << " solves:";
		for (const std::string &f : families) {
			if (std::ranges::find(dropped, f + " ") == dropped.end()) std::cout << " [" << f << "]";
		}
		std::cout << "\n";
	}
}

TEST_CASE("planar domain: a curve may come right up to a tunnel's portal (D437)", "[junction][planar][planardomain]")
{
	/* TT1's B1 (the separator of T2) crosses A1 at its third tile (x = 5, by a NW step before and a NE step after), the same polyline as BR1's: the first piece outside the crossing's run is the diagonal. A tunnel's end run is the portal alone (one piece, D437; the ground outside it is lower), so a diagonal starting one piece from the crossing's middle fits: the judge and the domain accept. The same curve one tile closer to the crossing (the diagonal starting at the portal piece itself, end run 0) is refused by both. */
	const TT1Fixture f;
	REQUIRE(f.v.structures[1].end_run[1] == 1);
	const RowsJudge judge = DrawingProgramJudge(DEFAULT_RULES);
	Geometry g = f.base;
	g["B1"] = {{0, 2}, {1, 2}, {4, 6}, {7, 2}, {8, 4}};
	const Verdicts fits = VerdictsOf(f, g, judge);
	INFO("refused by " << Join(fits.rules));
	CHECK(fits.rows == 0);
	CHECK(fits.domain);
	Geometry closer = f.base;
	closer["B1"] = {{0, 2}, {1, 2}, {4, 4}, {7, 2}, {8, 6}};
	const Verdicts too_close = VerdictsOf(f, closer, judge);
	CHECK(too_close.rows == 1);
	CHECK(too_close.rules.contains("end run"));
	CHECK_FALSE(too_close.domain);
}

TEST_CASE("planar slots: two-stage solve on Shape's variant (D441, D442)", "[.][slotsmeasure]")
{
	/* PLANAR_SLOTS_HUB selects one hub (T1, T2, T3, 197); unset runs all in turn. */
	const char *hub_env = std::getenv("PLANAR_SLOTS_HUB");
	const JunctionSpec spec = MakeTestJunctionSpec();
	const auto strip = [](PlanarShapeVariant &v) {
		for (PlanarShapeTrack &track : v.tracks) {
			std::vector<uint32_t> middles = track.flex;
			std::ranges::sort(middles);
			for (size_t j = middles.size(); j-- > 0;) {
				const uint32_t m = middles[j];
				track.pieces.erase(track.pieces.begin() + m, track.pieces.begin() + m + 2);
				for (uint32_t &e : track.event_piece) {
					if (e > m) e -= 2;
				}
			}
			track.flex.clear();
		}
	};
	std::vector<std::pair<std::string, PlanarShapeVariant>> hubs;
	for (const std::string name : {"T1", "T2", "T3", "197"}) {
		if (hub_env != nullptr && name != hub_env) continue;
		std::string error, why;
		PlanarShapeVariant v;
		if (name == "197") {
			const PlanarTopology topology = ParsePlanarTopology(Read197File("topology.txt"), &error);
			REQUIRE(error.empty());
			std::vector<std::string> report;
			const PlanarStructureVariant structure = LoadPlanarStructureVariant(topology, Read197File("structure.txt"), spec, report, &error);
			REQUIRE(error.empty());
			REQUIRE(LoadPlanarShapeVariant(topology, structure, Read197File("shape.txt"), v, why));
		} else {
			const PlanarTopology topology = ParsePlanarTopology(PlanarTText(name[1] - '0'), &error);
			REQUIRE(error.empty());
			const PlanarStructureResult r = BuildPlanarStructure(topology, spec, 1);
			REQUIRE(!r.variants.empty());
			REQUIRE(ShapePlanarStructureVariant(r.variants[0], v, why));
			strip(v);
		}
		hubs.emplace_back(name, std::move(v));
	}
	struct Config { int offers; bool d443; };
	for (const auto &[name, base] : hubs) {
		for (const Config cfg : {Config{3, true}, Config{1, false}}) {
			std::string why;
			PlanarShapeVariant v = base;
			REQUIRE(PlacePlanarShapeFlex(v, why, true, cfg.d443, cfg.offers));
			size_t pairs = 0;
			for (const PlanarShapeTrack &t : v.tracks) pairs += t.flex.size();
			const std::string tag = std::format("SLOTS {} offers {} d443 {}", name, cfg.offers, cfg.d443 ? "on" : "off");
			/* The suite's rules (DEFAULT_RULES), the program's options as the judge builds them. */
			PlanarDrawOptions o;
			o.corridor = true;
			o.clearance = o.clear_rows = o.pairwise = o.pairwise_all_faces = true;
			o.strict_span = true;
			o.strict_zero = o.crossing_flanks = false;
			o.bays = o.curves = false;
			o.node_lead = o.end_node = o.cross_piece = true;
			o.tidy = true;
			o.elastic = false;
			o.least_extra = 0;
			std::cout << std::format("{}: flex pairs {}\n", tag, pairs) << std::flush;

			/* Stage 1: the uncontracted program, geometry continuous, no grid, no node gaps (no reference yet). */
			PlanarDrawOptions s1 = o;
			s1.grid = false;
			s1.node_gaps = false;
			s1.continuous = {"point", "axis", "diagonal", "param", "cross"};
			const PlanarDrawResult r1 = CheckPlanarDrawable(v, 15.0, s1);
			size_t used = 0, kept_count = 0;
			if (r1.status == "Optimal") {
				for (uint32_t t = 0; t < v.tracks.size(); t++) {
					for (const uint32_t m : v.tracks[t].flex) {
						if (r1.lengths[t][m] > 1e-6) used++;
						if (r1.lengths[t][m] >= 0.5) kept_count++;
					}
				}
			}
			std::cout << std::format("{} STAGE1 status {} ms {} rows {} columns {} binaries {} pairs {} used {} kept {}\n", tag, r1.status, static_cast<int>(r1.ms), r1.rows, r1.columns, r1.binaries, r1.pairs, used, kept_count) << std::flush;
			if (r1.status != "Optimal") continue;

			/* The contracted variant (final_contract's mechanism), the lengths and points mapped onto it. */
			PlanarShapeVariant vc = v;
			auto points = r1.points;
			auto lengths = r1.lengths;
			for (size_t t = 0; t < vc.tracks.size(); t++) {
				PlanarShapeTrack &track = vc.tracks[t];
				std::vector<uint32_t> middles = track.flex;
				std::ranges::sort(middles);
				std::vector<bool> kept;
				for (const uint32_t m : middles) kept.push_back(r1.lengths[t][m] >= 0.5);
				for (size_t j = middles.size(); j-- > 0;) {
					if (kept[j]) continue;
					const uint32_t m = middles[j];
					track.pieces.erase(track.pieces.begin() + m, track.pieces.begin() + m + 2);
					for (uint32_t &e : track.event_piece) {
						if (e > m) e -= 2;
					}
					points[t].erase(points[t].begin() + m, points[t].begin() + m + 2);
					lengths[t][m - 1] += lengths[t][m] + lengths[t][m + 1];
					lengths[t].erase(lengths[t].begin() + m, lengths[t].begin() + m + 2);
				}
				track.steps.clear();
				uint32_t removed = 0;
				for (size_t j = 0; j < middles.size(); j++) {
					if (kept[j]) track.steps.push_back(middles[j] - 2 * removed);
					else removed++;
				}
				track.flex.clear();
			}
			int biggest = 0;
			for (const auto &track : points) {
				for (const auto &p : track) biggest = std::max({biggest, std::abs(p[0]), std::abs(p[1])});
			}
			/* Stage 2: integer with grid and node gaps; a: the sides stage 1 chose (as final_contract); b: sides free. */
			for (const bool sides : {true, false}) {
				PlanarDrawOptions s2 = o;
				s2.grid = true;
				s2.node_gaps = true;
				s2.node_gap_from = &lengths;
				if (sides) s2.sides_from = &points;
				s2.coord_max = std::max(64, (2 * biggest + 15) / 16 * 16);
				const PlanarDrawResult r2 = CheckPlanarDrawable(vc, 15.0, s2);
				std::string verdict;
				if (r2.status == "Optimal") {
					X1Fixture f(vc);
					Contracted cv;
					cv.v = vc;
					cv.points = r2.points;
					const RowsVerdict jv = DrawingProgramJudge(DEFAULT_RULES)(f, cv, true);
					verdict = std::format(" judge {}", jv.accept ? "accept" : "refuse");
					for (const std::string &rule : jv.rules) verdict += " [" + rule + "]";
					std::vector<std::string> notes;
					const PlanarDomainVerdict d = CheckPlanarDomain(PlanarDomainInputOf(vc, r2.points, notes));
					std::set<std::string> breaks;
					for (const PlanarDomainBreak &b : d.breaks) breaks.insert(b.rule);
					verdict += std::format(" domain {} notes {} breaks {}", d.legal ? "accept" : "refuse", notes.size(), d.breaks.size());
					for (const std::string &rule : breaks) verdict += " [" + rule + "]";
				}
				std::cout << std::format("{} STAGE2{} status {} ms {} rows {} columns {} binaries {} steps {}{}\n", tag, sides ? "a(sides from stage 1)" : "b(sides free)", r2.status, static_cast<int>(r2.ms), r2.rows, r2.columns, r2.binaries, kept_count, verdict) << std::flush;
				if (r2.status == "Optimal" && sides) break;
			}
		}
	}
}

namespace {

/** D444 lazy trial: one run of rounds over a variant. Each round solves with separation rows only for `list`, then adds the pairs that fail on its points. */
struct LazyRun {
	std::vector<std::string> list; ///< The pair tags with rows at the end.
	bool converged = false;
	std::string stop; ///< Why the run ended when it did not converge.
	int rounds = 0;
	double ms = 0;
	size_t all_pairs = 0;
	PlanarDrawResult last; ///< The last Optimal round.
	std::vector<PlanarKeepApartPair> pairs; ///< The last Optimal round's pairs.
};

std::string LazyRoundTag(const std::string &tag, const char *stage, int round) { return std::format("{} {} round {}", tag, stage, round); }

/** D444 lazy-3: the minimal conflict of an Infeasible solve by deletion: over the row families of the first conflict, then, within the last family still needed, over its tags; at most 30 solves in all. */
void LazyConflict(const PlanarShapeVariant &v, const PlanarDrawOptions &base, const std::string &tag, const char *stage, std::ostream &md)
{
	const PlanarDrawResult first = CheckPlanarDrawable(v, 15.0, base);
	std::vector<std::string> families;
	std::map<std::string, std::vector<std::string>> tags;
	for (const std::string &c : first.conflict) {
		const std::string f = c.substr(0, c.find(' '));
		if (std::ranges::find(families, f) == families.end()) families.push_back(f);
		tags[f].push_back(c);
	}
	std::string out = std::format("- {} {} conflict: first solve {} (conflict rows {}, families {})\n", tag, stage, first.status, first.conflict.size(), families.size());
	for (const std::string &c : first.conflict) out += " [" + c + "]";
	out += "\n";
	{
		const auto &mp = v.structure.map;
		for (const std::string &c : first.conflict) {
			if (!c.starts_with("cross ")) continue;
			size_t k = 0;
			std::from_chars(c.data() + 6, c.data() + c.size(), k);
			if (k < mp.crossings.size()) out += std::format("  - {} is the crossing of {} and {}\n", c, mp.tracks[mp.crossings[k].a].name, mp.tracks[mp.crossings[k].b].name);
		}
	}
	int solves = 1;
	std::vector<std::string> dropped;
	for (const std::string &f : families) {
		if (solves >= 30) break;
		PlanarDrawOptions fo = base;
		fo.skip_rows = dropped;
		fo.skip_rows.push_back(f + " ");
		const PlanarDrawResult r = CheckPlanarDrawable(v, 15.0, fo);
		solves++;
		out += std::format("  - drop family [{}]: {} {} ms\n", f, r.status, static_cast<int>(r.ms));
		if (r.status == "Infeasible") dropped.push_back(f + " ");
	}
	std::vector<std::string> needed;
	for (const std::string &f : families) {
		if (std::ranges::find(dropped, f + " ") == dropped.end()) needed.push_back(f);
	}
	out += "  - families needed:";
	for (const std::string &f : needed) out += " [" + f + "]";
	out += "\n";
	if (!needed.empty()) {
		const std::string &last = needed.back();
		std::vector<std::string> dropped_tags;
		for (const std::string &t : tags[last]) {
			if (solves >= 30) break;
			PlanarDrawOptions fo = base;
			fo.skip_rows = dropped;
			for (const std::string &d : dropped_tags) fo.skip_rows.push_back(d);
			fo.skip_rows.push_back(t);
			const PlanarDrawResult r = CheckPlanarDrawable(v, 15.0, fo);
			solves++;
			out += std::format("  - drop tag [{}]: {} {} ms\n", t, r.status, static_cast<int>(r.ms));
			if (r.status == "Infeasible") dropped_tags.push_back(t);
		}
		out += std::format("  - tags needed in [{}] after {} solves:", last, solves);
		for (const std::string &t : tags[last]) {
			if (std::ranges::find(dropped_tags, t) == dropped_tags.end()) out += " [" + t + "]";
		}
		out += "\n";
	}
	md << out << std::flush;
	std::cout << out << std::flush;
}

/** D444 lazy-4: the candidate (axis * 2 + side, the order the pair lists them) with the largest margin on the points: the least over its rows of s . (q - p) - spacing. */
int LazyBestCandidate(const PlanarKeepApartPair &p, const std::vector<std::vector<std::array<int, 2>>> &points)
{
	int best = 0, best_margin = INT_MIN;
	for (size_t c = 0; c < p.candidates.size(); c++) {
		int margin = INT_MAX;
		for (const PlanarKeepApartPair::Row &r : p.candidates[c].rows) {
			const std::array<int, 2> a = PlanarKeepApartEnd(r, false, points), b = PlanarKeepApartEnd(r, true, points);
			margin = std::min(margin, p.candidates[c].s[0] * (b[0] - a[0]) + p.candidates[c].s[1] * (b[1] - a[1]) - p.candidates[c].spacing);
		}
		if (margin > best_margin) {
			best_margin = margin;
			best = static_cast<int>(c);
		}
	}
	return best;
}

/** Slots step 1b: the best candidate's margin on the points (the largest over candidates of the least over its rows of s . (b - a) - spacing). */
int LazyBestMargin(const PlanarKeepApartPair &p, const std::vector<std::vector<std::array<int, 2>>> &points)
{
	int best_margin = INT_MIN;
	for (const PlanarKeepApartPair::Candidate &cand : p.candidates) {
		int margin = INT_MAX;
		for (const PlanarKeepApartPair::Row &r : cand.rows) {
			const std::array<int, 2> a = PlanarKeepApartEnd(r, false, points), b = PlanarKeepApartEnd(r, true, points);
			margin = std::min(margin, cand.s[0] * (b[0] - a[0]) + cand.s[1] * (b[1] - a[1]) - cand.spacing);
		}
		best_margin = std::max(best_margin, margin);
	}
	return best_margin;
}

/**
 * D444 lazy-4: rounds as lazy-1, but a pair that fails on a round's points is added with its candidate FIXED to the one with the largest margin on those points (one plain row, no binary); a round with fixed pairs that is Infeasible frees the fixed pairs its conflict names (binaries, as lazy-1) and solves again. `list` seeds the free pairs.
 * D446 (slots step 1): with `fix_sides == false` a round's failing pairs join `free_list` (side binaries) and no pair is ever fixed, so the Infeasible branch never frees any. With `tight_bound == true` every round after the first Optimal round sets coord_max to max(64, (2 * biggest + 15) / 16 * 16) over the last Optimal points; a tight round that is not Optimal is solved once more with coord_max doubled (both rows written, the second as stage "<stage>+", both counted).
 * Slots step 1b: with `near_margin > 0` a pair without rows also joins the failing pairs when its best candidate's margin on the round's points is below `near_margin`.
 */
LazyRun LazyLoop(const PlanarShapeVariant &v, const PlanarDrawOptions &base, std::vector<std::string> list, const std::string &tag, const char *stage_name, std::ostream &md, bool fix_sides = true, bool tight_bound = false, int near_margin = 0)
{
	std::string stage_text = stage_name;
	const char *stage = stage_text.c_str();
	LazyRun run;
	std::vector<std::string> free_list = std::move(list);
	std::vector<std::pair<std::string, int>> fixed;
	const auto all_with_rows = [&]() {
		std::vector<std::string> names = free_list;
		for (const auto &[t, c] : fixed) names.push_back(t);
		return names;
	};
	for (int round = 0; round < 12; round++) {
		PlanarDrawOptions o = base;
		o.keep_apart_out = true;
		o.only_pairs_on = true;
		o.only_pairs = free_list;
		o.fixed_pairs = fixed;
		stage_text = stage_name;
		stage = stage_text.c_str();
		PlanarDrawResult r;
		bool tight_round = false;
		if (tight_bound && run.last.status == "Optimal") {
			int biggest = 0;
			for (const auto &track : run.last.points) {
				for (const auto &p : track) biggest = std::max({biggest, std::abs(p[0]), std::abs(p[1])});
			}
			o.coord_max = std::max(64, (2 * biggest + 15) / 16 * 16);
			tight_round = true;
		}
		r = CheckPlanarDrawable(v, 15.0, o);
		run.rounds++;
		run.ms += r.ms;
		if (tight_round && r.status != "Optimal") {
			const std::string row = std::format("| {} | {} | {} | {} | {} | {} | {} | - | - |", stage, round, r.status, static_cast<int>(r.ms), fixed.size(), free_list.size(), r.binaries);
			md << row << "\n" << std::flush;
			std::cout << LazyRoundTag(tag, stage, round) << ": " << row << "\n" << std::flush;
			stage_text = std::string(stage_name) + "+";
			stage = stage_text.c_str();
			o.coord_max *= 2;
			r = CheckPlanarDrawable(v, 15.0, o);
			run.rounds++;
			run.ms += r.ms;
		}
		const size_t n_fixed = fixed.size(), n_free = free_list.size();
		size_t all = 0;
		std::vector<const PlanarKeepApartPair *> failing;
		for (const PlanarKeepApartPair &p : r.keep_apart) {
			if (!p.skipped.empty() || p.candidates.empty()) continue;
			all++;
		}
		run.all_pairs = all;
		if (r.status == "Optimal") {
			const std::vector<std::string> have = all_with_rows();
			for (const PlanarKeepApartPair &p : r.keep_apart) {
				if (!p.skipped.empty() || p.candidates.empty()) continue;
				if (std::ranges::find(have, p.tag) == have.end() && (!PlanarKeepApartHolds(p, r.points) || (near_margin > 0 && LazyBestMargin(p, r.points) < near_margin))) failing.push_back(&p);
			}
			run.last = r;
			run.pairs = r.keep_apart;
		}
		size_t freed = 0;
		if (r.status == "Infeasible" && !fixed.empty()) {
			for (const std::string &c : r.conflict) {
				const auto it = std::ranges::find_if(fixed, [&](const std::pair<std::string, int> &f) { return f.first == c; });
				if (it == fixed.end()) continue;
				free_list.push_back(it->first);
				fixed.erase(it);
				freed++;
			}
		}
		const std::string line = std::format("| {} | {} | {} | {} | {} | {} | {} | {} | {} |", stage, round, r.status, static_cast<int>(r.ms), n_fixed, n_free, r.binaries, r.status == "Optimal" ? std::format("{}", failing.size()) : "-", r.status == "Infeasible" ? std::format("{}", freed) : "-");
		md << line << "\n" << std::flush;
		std::cout << LazyRoundTag(tag, stage, round) << ": " << line << "\n" << std::flush;
		if (r.status == "Infeasible" && freed == 0) {
			std::string fam;
			for (const std::string &c : r.conflict) fam += " [" + c + "]";
			const std::string note = std::format("- {} {} round {} conflict rows {}:{}\n", tag, stage, round, r.conflict.size(), fam.size() > 600 ? fam.substr(0, 600) + " ..." : fam);
			md << note << std::flush;
			std::cout << note << std::flush;
		}
		if (r.status == "Infeasible" && freed > 0) continue;
		if (r.status != "Optimal") {
			run.stop = r.status;
			run.list = all_with_rows();
			if (r.status == "Infeasible") LazyConflict(v, o, tag, stage, md);
			return run;
		}
		if (failing.empty()) {
			run.converged = true;
			run.list = all_with_rows();
			return run;
		}
		for (const PlanarKeepApartPair *p : failing) {
			if (fix_sides) fixed.emplace_back(p->tag, LazyBestCandidate(*p, r.points));
			else free_list.push_back(p->tag);
		}
	}
	run.stop = "12 rounds";
	run.list = all_with_rows();
	return run;
}

}  // namespace

TEST_CASE("planar slots: lazy keep-apart rows on T1 and T2 (D444)", "[.][lazymeasure]")
{
	const char *hub_env = std::getenv("PLANAR_SLOTS_HUB");
	const char *cfg_env = std::getenv("PLANAR_LAZY_CFG"); ///< "1" offers 1 d443 off only; "3" offers 3 d443 on only; unset both.
	const JunctionSpec spec = MakeTestJunctionSpec();
	std::ofstream md(std::getenv("PLANAR_LAZY_OUT") != nullptr ? std::getenv("PLANAR_LAZY_OUT") : "lazy-rounds.md", std::ios::app);
	for (const std::string name : {"T1", "T2"}) {
		if (hub_env != nullptr && name != hub_env) continue;
		std::string error, why;
		const PlanarTopology topology = ParsePlanarTopology(PlanarTText(name[1] - '0'), &error);
		REQUIRE(error.empty());
		const PlanarStructureResult sr = BuildPlanarStructure(topology, spec, 1);
		REQUIRE(!sr.variants.empty());
		PlanarShapeVariant base;
		REQUIRE(ShapePlanarStructureVariant(sr.variants[0], base, why));
		for (PlanarShapeTrack &track : base.tracks) {
			std::vector<uint32_t> middles = track.flex;
			std::ranges::sort(middles);
			for (size_t j = middles.size(); j-- > 0;) {
				const uint32_t m = middles[j];
				track.pieces.erase(track.pieces.begin() + m, track.pieces.begin() + m + 2);
				for (uint32_t &e : track.event_piece) {
					if (e > m) e -= 2;
				}
			}
			track.flex.clear();
		}
		struct Config { int offers; bool d443; };
		for (const Config cfg : {Config{1, false}, Config{3, true}, Config{3, false}, Config{1, true}}) {
			/* Unset: the two lazy-1 configs; "1", "3": by offers among those two; "1on", "3off", ...: one config by offers and D443. */
			const bool is_default = (cfg.offers == 1 && !cfg.d443) || (cfg.offers == 3 && cfg.d443);
			if (cfg_env == nullptr ? !is_default : std::string(cfg_env) != std::format("{}", cfg.offers) && std::string(cfg_env) != std::format("{}{}", cfg.offers, cfg.d443 ? "on" : "off")) continue;
			if (cfg_env != nullptr && std::string(cfg_env).size() == 1 && !is_default) continue;
			PlanarShapeVariant v = base;
			REQUIRE(PlacePlanarShapeFlex(v, why, true, cfg.d443, cfg.offers));
			size_t flex_pairs = 0;
			for (const PlanarShapeTrack &t : v.tracks) flex_pairs += t.flex.size();
			const std::string tag = std::format("LAZY {} offers {} d443 {}", name, cfg.offers, cfg.d443 ? "on" : "off");
			md << std::format("\n### {} (flex pairs {})\n\n| stage | round | status | ms | pairs fixed | pairs free | binaries | failing pairs | freed |\n|---|---|---|---|---|---|---|---|---|\n", tag, flex_pairs) << std::flush;
			/* Stage A: uncontracted, integer, SlotsOptions' grid, inserts may be 0, no reference (no node gaps). */
			const X1Fixture fa(v);
			PlanarDrawOptions sa = SlotsOptions(fa, DEFAULT_RULES);
			sa.node_gaps = false;
			const LazyRun a = LazyLoop(v, sa, {}, tag, "A", md);
			std::string summary = std::format("- {} stage A: rounds {} converged {} stop '{}' pairs with rows {} of {} total ms {}\n", tag, a.rounds, a.converged, a.stop, a.list.size(), a.all_pairs, static_cast<int>(a.ms));
			if (a.last.status != "Optimal") {
				md << "\n" << summary << "- no stage B (stage A never Optimal)\n" << std::flush;
				continue;
			}

			/* Stage B: contract from stage A's last Optimal drawing (slots-1's mechanism). */
			PlanarShapeVariant vc = v;
			auto points = a.last.points;
			auto lengths = a.last.lengths;
			std::set<std::string> seed_keys;
			std::vector<std::vector<int>> mapped(vc.tracks.size());
			for (size_t t = 0; t < vc.tracks.size(); t++) {
				PlanarShapeTrack &track = vc.tracks[t];
				std::vector<uint32_t> middles = track.flex;
				std::ranges::sort(middles);
				std::vector<bool> kept;
				for (const uint32_t m : middles) kept.push_back(a.last.lengths[t][m] >= 0.5);
				std::set<uint32_t> removed;
				for (size_t j = 0; j < middles.size(); j++) {
					if (!kept[j]) removed.insert(middles[j]);
				}
				int out = 0;
				mapped[t].resize(track.pieces.size());
				for (uint32_t p = 0; p < track.pieces.size(); p++) {
					if (removed.contains(p) || (p > 0 && removed.contains(p - 1))) mapped[t][p] = out - 1;
					else mapped[t][p] = out++;
				}
				for (size_t j = middles.size(); j-- > 0;) {
					if (kept[j]) continue;
					const uint32_t m = middles[j];
					track.pieces.erase(track.pieces.begin() + m, track.pieces.begin() + m + 2);
					for (uint32_t &e : track.event_piece) {
						if (e > m) e -= 2;
					}
					points[t].erase(points[t].begin() + m, points[t].begin() + m + 2);
					lengths[t][m - 1] += lengths[t][m] + lengths[t][m + 1];
					lengths[t].erase(lengths[t].begin() + m, lengths[t].begin() + m + 2);
				}
				track.steps.clear();
				uint32_t gone = 0;
				for (size_t j = 0; j < middles.size(); j++) {
					if (kept[j]) track.steps.push_back(middles[j] - 2 * gone);
					else gone++;
				}
				track.flex.clear();
			}
			const auto key_of = [](uint32_t t0, uint32_t p0, uint32_t t1, uint32_t p1) {
				if (std::make_pair(t0, p0) > std::make_pair(t1, p1)) { std::swap(t0, t1); std::swap(p0, p1); }
				return std::format("{}.{}-{}.{}", t0, p0, t1, p1);
			};
			for (const PlanarKeepApartPair &p : a.pairs) {
				if (std::ranges::find(a.list, p.tag) == a.list.end()) continue;
				seed_keys.insert(key_of(p.tracks[0], static_cast<uint32_t>(mapped[p.tracks[0]][p.pieces[0]]), p.tracks[1], static_cast<uint32_t>(mapped[p.tracks[1]][p.pieces[1]])));
			}
			int biggest = 0;
			for (const auto &track : points) {
				for (const auto &p : track) biggest = std::max({biggest, std::abs(p[0]), std::abs(p[1])});
			}
			/* D444 lazy-2: stage B solves on what the judge reads (steps cleared) with the judge's options (SlotsOptions) and the references the judge would read from stage A's contracted drawing; coord_max is the solve's bound, not a rule. */
			PlanarShapeVariant plain = vc;
			for (PlanarShapeTrack &track : plain.tracks) track.steps.clear();
			Contracted a_cv;
			a_cv.v = plain;
			a_cv.points = points;
			const std::vector<std::vector<double>> ref_port_zero = PortZeroFrom(points);
			const std::vector<std::vector<double>> ref_along = AlongFrom(a_cv);
			bool turns_ok = std::ranges::all_of(plain.tracks, [](const PlanarShapeTrack &tr) { return std::ranges::adjacent_find(tr.pieces, [](int x, int y) { return std::abs(x - y) > 1; }) == tr.pieces.end(); });
			for (size_t t = 0; t < plain.tracks.size(); t++) {
				const auto &ps = plain.tracks[t].pieces;
				const auto bad = std::ranges::adjacent_find(ps, [](int x, int y) { return std::abs(x - y) > 1; });
				if (bad != ps.end()) md << std::format("- turn check: track {} pieces {} and {}: headings {} then {}\n", t, bad - ps.begin(), bad - ps.begin() + 1, *bad, *(bad + 1));
			}
			X1Fixture fb(plain);
			const bool node_ok = NodeTurnsHold(fb, a_cv);
			const std::string turn_line = std::format("- stage B contracted variant: per-track turns {}, node turns (F6/F8) {}\n", turns_ok ? "hold" : "FAIL", node_ok ? "hold" : "FAIL");
			md << turn_line << std::flush;
			std::cout << turn_line << std::flush;
			PlanarDrawOptions sb = SlotsOptions(fb, DEFAULT_RULES);
			sb.port_zero_from = &ref_port_zero;
			sb.node_gap_from = &ref_port_zero;
			sb.along_from = &ref_along;
			sb.coord_max = std::max(64, (2 * biggest + 15) / 16 * 16);
			/* The listing solve: no rows, to read stage B's tags; the seed is stage A's pairs mapped by (track, piece). */
			PlanarDrawOptions listing = sb;
			listing.keep_apart_out = true;
			listing.only_pairs_on = true;
			const PlanarDrawResult lr = CheckPlanarDrawable(plain, 15.0, listing);
			std::vector<std::string> seed;
			for (const PlanarKeepApartPair &p : lr.keep_apart) {
				if (!p.skipped.empty() || p.candidates.empty()) continue;
				if (seed_keys.contains(key_of(p.tracks[0], p.pieces[0], p.tracks[1], p.pieces[1]))) seed.push_back(p.tag);
			}
			summary += std::format("- stage B seed: {} of stage A's {} pairs map to stage B tags (listing solve status {}, {} ms, not counted)\n", seed.size(), a.list.size(), lr.status, static_cast<int>(lr.ms));
			if (lr.status == "Infeasible") LazyConflict(plain, listing, tag, "B (no keep-apart rows)", md);
			const LazyRun b = LazyLoop(plain, sb, seed, tag, "B", md);
			summary += std::format("- {} stage B: rounds {} converged {} stop '{}' pairs with rows {} of {} total ms {}\n", tag, b.rounds, b.converged, b.stop, b.list.size(), b.all_pairs, static_cast<int>(b.ms));
			if (b.converged) {
				const X1Fixture &f = fb;
				Contracted cv;
				cv.v = plain;
				cv.points = b.last.points;
				const RowsVerdict jv = DrawingProgramJudge(DEFAULT_RULES)(f, cv, true);
				std::string verdict = std::format("judge {}", jv.accept ? "accept" : "refuse");
				for (const std::string &rule : jv.rules) verdict += " [" + rule + "]";
				const RowsVerdict jn = DrawingProgramJudge(DEFAULT_RULES)(f, cv, false);
				verdict += std::format(" (without interface rows: judge {}", jn.accept ? "accept" : "refuse");
				for (const std::string &rule : jn.rules) verdict += " [" + rule + "]";
				verdict += ")";
				std::vector<std::string> notes;
				const PlanarDomainVerdict d = CheckPlanarDomain(PlanarDomainInputOf(vc, b.last.points, notes));
				std::set<std::string> breaks;
				for (const PlanarDomainBreak &br : d.breaks) breaks.insert(br.rule);
				verdict += std::format("; domain {} notes {} breaks {}", d.legal ? "accept" : "refuse", notes.size(), d.breaks.size());
				for (const std::string &rule : breaks) verdict += " [" + rule + "]";
				summary += "- " + verdict + "\n";
				for (const std::string &tag : CheckPlanarProgramOnPoints(g_last_readouts.program.program, cv.points).broken) summary += "  - judge program row broken: " + tag + "\n";
				for (const PlanarKeepApartPair &kp : g_last_readouts.keep.keep_apart) {
					if (!PlanarKeepApartHolds(kp, cv.points)) summary += "  - judge keep-apart broken: " + kp.tag + "\n";
				}
				std::set<std::string> shown;
				for (const PlanarDomainBreak &br : d.breaks) {
					if (!shown.insert(br.rule).second) continue;
					std::string trs;
					for (const uint32_t t : br.tracks) trs += std::format(" {}", f.Name(t));
					summary += std::format("  - first domain break [{}] tracks{} tile ({}, {}): {}\n", br.rule, trs, br.tile.x, br.tile.y, br.why);
				}
				const auto &mp = vc.structure.map;
				for (uint32_t n = 0; n < mp.expanded.nodes.size(); n++) {
					std::string line = std::format("  - node {}:", n);
					for (uint32_t t = 0; t < f.n; t++) {
						const PlanarLine &ln = mp.expanded.lines[t];
						if (!ln.to.port && ln.to.index == n) line += std::format(" {}@end({},{}) h{}", f.Name(t), cv.points[t].back()[0], cv.points[t].back()[1], vc.tracks[t].pieces.back());
						if (!ln.from.port && ln.from.index == n) line += std::format(" {}@start({},{}) h{}", f.Name(t), cv.points[t].front()[0], cv.points[t].front()[1], vc.tracks[t].pieces.front());
					}
					summary += line + "\n";
				}
			}
			/* Control: stage B with every pair's rows (slots-1's stage 2 with sides free), judged the same way. */
			{
				const PlanarDrawResult fr = CheckPlanarDrawable(plain, 15.0, sb);
				std::string verdict = std::format("- control, stage B with all rows: status {} ms {} binaries {}", fr.status, static_cast<int>(fr.ms), fr.binaries);
				if (fr.status == "Optimal") {
					const X1Fixture &f = fb;
					Contracted cv;
					cv.v = plain;
					cv.points = fr.points;
					const RowsVerdict jv = DrawingProgramJudge(DEFAULT_RULES)(f, cv, true);
					verdict += std::format(" judge {}", jv.accept ? "accept" : "refuse");
					for (const std::string &rule : jv.rules) verdict += " [" + rule + "]";
				}
				summary += verdict + "\n";
			}
			md << "\n" << summary << std::flush;
			std::cout << summary << std::flush;
		}
	}
}

/** D444 lazy-5: why T2 (offers 1, D443 off) stage A is Infeasible with every keep-apart row: elastic on "sep" only, LP then integer; the stretched pairs with their walls and the domain's breaks near them. */
TEST_CASE("planar slots: T2 stage A elastic keep-apart diagnosis (lazy-5)", "[.][lazydiag]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	std::ofstream md(std::getenv("PLANAR_DIAG_OUT") != nullptr ? std::getenv("PLANAR_DIAG_OUT") : "diag.md", std::ios::app);
	std::string error, why;
	const PlanarTopology topology = ParsePlanarTopology(PlanarTText(2), &error);
	REQUIRE(error.empty());
	const PlanarStructureResult sr = BuildPlanarStructure(topology, spec, 1);
	REQUIRE(!sr.variants.empty());
	PlanarShapeVariant base;
	REQUIRE(ShapePlanarStructureVariant(sr.variants[0], base, why));
	for (PlanarShapeTrack &track : base.tracks) {
		std::vector<uint32_t> middles = track.flex;
		std::ranges::sort(middles);
		for (size_t j = middles.size(); j-- > 0;) {
			const uint32_t m = middles[j];
			track.pieces.erase(track.pieces.begin() + m, track.pieces.begin() + m + 2);
			for (uint32_t &e : track.event_piece) {
				if (e > m) e -= 2;
			}
		}
		track.flex.clear();
	}
	PlanarShapeVariant v = base;
	REQUIRE(PlacePlanarShapeFlex(v, why, true, false, 1));
	const X1Fixture fa(v);
	const auto &mp = v.structure.map;
	for (const bool integer : {false, true}) {
		PlanarDrawOptions o = SlotsOptions(fa, DEFAULT_RULES);
		o.node_gaps = false;
		o.keep_apart_out = true;
		o.elastic = true;
		o.elastic_families = {"sep"};
		o.elastic_integer = integer;
		const PlanarDrawResult r = CheckPlanarDrawable(v, 15.0, o);
		std::string out = std::format("\n## elastic {} (sep only)\n\nstatus {} ms {} binaries {} total slack {} stretched rows {}\n", integer ? "integer" : "continuous LP", r.status, static_cast<int>(r.ms), r.binaries, r.slack, r.stretched.size());
		if (r.status == "Optimal") {
			const auto where = [&](uint32_t t, uint32_t piece) {
				std::string s = std::format("{} piece {} h{}", fa.Name(t), piece, v.tracks[t].pieces[piece]);
				const auto &ev = mp.events[t];
				for (size_t k = 0; k < ev.size(); k++) {
					if (v.tracks[t].event_piece[k] == piece) s += std::format(" [crossing {} {}x{}]", ev[k], fa.Name(mp.crossings[ev[k]].a), fa.Name(mp.crossings[ev[k]].b));
				}
				const PlanarLine &ln = mp.expanded.lines[t];
				if (piece == 0) s += std::format(" [starts {} {}]", ln.from.port ? "port" : "node", ln.from.index);
				if (piece + 1 == v.tracks[t].pieces.size()) s += std::format(" [ends {} {}]", ln.to.port ? "port" : "node", ln.to.index);
				std::string near;
				for (size_t k = 0; k < ev.size(); k++) {
					if (v.tracks[t].event_piece[k] + 2 >= piece && piece + 2 >= v.tracks[t].event_piece[k]) near += std::format(" {}", ev[k]);
				}
				if (!near.empty()) s += " near crossings" + near;
				const auto &pts = r.points[t];
				s += std::format(" pts ({},{})->({},{})", pts[piece][0], pts[piece][1], pts[piece + 1][0], pts[piece + 1][1]);
				return s;
			};
			std::vector<std::string> notes;
			const PlanarDomainVerdict d = CheckPlanarDomain(PlanarDomainInputOf(v, r.points, notes));
			out += std::format("domain on the whole elastic drawing: {} notes {} breaks {}\n", d.legal ? "accept" : "refuse", notes.size(), d.breaks.size());
			std::map<std::string, int> by_rule;
			for (const PlanarDomainBreak &br : d.breaks) by_rule[br.rule]++;
			for (const auto &[rule, n] : by_rule) out += std::format("  - rule [{}] x{}\n", rule, n);
			int shown = 0;
			for (const auto &[tag, slack] : r.stretched) {
				out += std::format("\n- stretched [{}] slack {}\n", tag, slack);
				if (shown++ >= 10) continue;
				for (const PlanarKeepApartPair &p : r.keep_apart) {
					if (p.tag != tag) continue;
					out += std::format("  - wall i: {}\n  - wall j: {}\n", where(p.tracks[0], p.pieces[0]), where(p.tracks[1], p.pieces[1]));
					for (size_t c = 0; c < p.candidates.size(); c++) {
						int margin = INT_MAX;
						for (const PlanarKeepApartPair::Row &row : p.candidates[c].rows) {
							const std::array<int, 2> a = PlanarKeepApartEnd(row, false, r.points), b = PlanarKeepApartEnd(row, true, r.points);
							margin = std::min(margin, p.candidates[c].s[0] * (b[0] - a[0]) + p.candidates[c].s[1] * (b[1] - a[1]) - p.candidates[c].spacing);
						}
						out += std::format("  - candidate {} s ({},{}) spacing {} rows {} margin on the drawing {}\n", c, p.candidates[c].s[0], p.candidates[c].s[1], p.candidates[c].spacing, p.candidates[c].rows.size(), margin);
					}
					for (const PlanarDomainBreak &br : d.breaks) {
						if (std::ranges::find(br.tracks, p.tracks[0]) == br.tracks.end() && std::ranges::find(br.tracks, p.tracks[1]) == br.tracks.end()) continue;
						std::string trs;
						for (const uint32_t t : br.tracks) trs += std::format(" {}", fa.Name(t));
						out += std::format("  - domain break [{}] tracks{} tile ({}, {}): {}\n", br.rule, trs, br.tile.x, br.tile.y, br.why);
					}
				}
			}
		}
		md << out << std::flush;
		std::cout << out << std::flush;
	}
	/* Controls (not elastic, one solve each, 15 s): all rows with grid on (lazy-4's Infeasible), and with grid off. */
	for (const bool grid : {true, false}) {
		PlanarDrawOptions o = SlotsOptions(fa, DEFAULT_RULES);
		o.node_gaps = false;
		o.grid = grid;
		const PlanarDrawResult r = CheckPlanarDrawable(v, 15.0, o);
		const std::string out = std::format("\n## control: all keep-apart rows, hard, grid {}\n\nstatus {} ms {} binaries {}\n", grid ? "on" : "off", r.status, static_cast<int>(r.ms), r.binaries);
		md << out << std::flush;
		std::cout << out << std::flush;
	}
}

/** Slots step 0: T2 stage A round 1 cheap checks: which of coord_max, tidy, a length start, a fixed side or grid moves round 1 from Error to a result. */
TEST_CASE("planar slots: T2 stage A round 1 cheap checks (step 0)", "[.][slotsstep0]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	std::ofstream md(std::getenv("PLANAR_STEP0_OUT") != nullptr ? std::getenv("PLANAR_STEP0_OUT") : "slots-step0.md", std::ios::app);
	std::string error, why;
	const PlanarTopology topology = ParsePlanarTopology(PlanarTText(2), &error);
	REQUIRE(error.empty());
	const PlanarStructureResult sr = BuildPlanarStructure(topology, spec, 1);
	REQUIRE(!sr.variants.empty());
	PlanarShapeVariant base;
	REQUIRE(ShapePlanarStructureVariant(sr.variants[0], base, why));
	for (PlanarShapeTrack &track : base.tracks) {
		std::vector<uint32_t> middles = track.flex;
		std::ranges::sort(middles);
		for (size_t j = middles.size(); j-- > 0;) {
			const uint32_t m = middles[j];
			track.pieces.erase(track.pieces.begin() + m, track.pieces.begin() + m + 2);
			for (uint32_t &e : track.event_piece) {
				if (e > m) e -= 2;
			}
		}
		track.flex.clear();
	}
	PlanarShapeVariant v = base;
	REQUIRE(PlacePlanarShapeFlex(v, why, true, false, 1));
	const X1Fixture fa(v);
	PlanarDrawOptions sa = SlotsOptions(fa, DEFAULT_RULES);
	sa.node_gaps = false;

	PlanarDrawOptions o0 = sa;
	o0.keep_apart_out = true;
	o0.only_pairs_on = true;
	const PlanarDrawResult d0 = CheckPlanarDrawable(v, 15.0, o0);
	REQUIRE(d0.status == "Optimal");
	const auto failing_of = [&](const PlanarDrawResult &r, const std::vector<std::string> &have) {
		std::vector<const PlanarKeepApartPair *> failing;
		for (const PlanarKeepApartPair &p : r.keep_apart) {
			if (!p.skipped.empty() || p.candidates.empty()) continue;
			if (std::ranges::find(have, p.tag) == have.end() && !PlanarKeepApartHolds(p, r.points)) failing.push_back(&p);
		}
		return failing;
	};
	const std::vector<const PlanarKeepApartPair *> f0 = failing_of(d0, {});
	std::vector<std::string> tags;
	for (const PlanarKeepApartPair *p : f0) tags.push_back(p->tag);
	int biggest = 0;
	for (const auto &track : d0.points) {
		for (const auto &p : track) biggest = std::max({biggest, std::abs(p[0]), std::abs(p[1])});
	}
	const int tight = std::max(64, (2 * biggest + 15) / 16 * 16);
	const std::string head = std::format("\n### T2 stage A round 1 (offers 1, D443 off)\n\nround 0: {} ms {}, failing pairs {}; biggest {}; tight {}\n\n| run | change | status | ms | binaries | start_values | failing without rows |\n|---|---|---|---|---|---|---|\n", d0.status, static_cast<int>(d0.ms), f0.size(), biggest, tight);
	md << head << std::flush;
	std::cout << head << std::flush;
	REQUIRE(f0.size() >= 40);
	REQUIRE(f0.size() <= 60);

	std::vector<std::pair<std::string, int>> fixed;
	for (const PlanarKeepApartPair *p : f0) fixed.emplace_back(p->tag, LazyBestCandidate(*p, d0.points));
	const char *names[] = {"none", "coord_max tight", "tidy off", "tight + tidy off", "R3 + start_lengths", "tight, fixed sides, no binaries", "R5 + grid off", "R3 + grid off"};
	for (int run = 0; run < 8; run++) {
		PlanarDrawOptions o = sa;
		o.keep_apart_out = true;
		o.only_pairs_on = true;
		o.only_pairs = tags;
		if (run == 1 || run == 3 || run == 4 || run == 5 || run == 6 || run == 7) o.coord_max = tight;
		if (run == 2 || run == 3 || run == 4 || run == 7) o.tidy = false;
		if (run == 4) o.start_lengths = &d0.lengths;
		if (run == 5 || run == 6) {
			o.only_pairs.clear();
			o.fixed_pairs = fixed;
		}
		if (run == 6 || run == 7) o.grid = false;
		const PlanarDrawResult r = CheckPlanarDrawable(v, 15.0, o);
		std::string fail = "-";
		if (r.status == "Optimal") {
			std::vector<std::string> have = tags;
			for (const auto &[t, c] : fixed) {
				if (run == 5 || run == 6) have.push_back(t);
			}
			fail = std::format("{}", failing_of(r, have).size());
		}
		const std::string row = std::format("| R{} | {} | {} | {} | {} | {} | {} |\n", run, names[run], r.status, static_cast<int>(r.ms), r.binaries, run == 4 ? std::format("{}", r.start_values) : "-", fail);
		md << row << std::flush;
		std::cout << row << std::flush;
	}
}

/** Slots step 1: stage A's lazy loop under D446 (free side binaries, tight bound, feasibility objective) on T1 and T2, then a 3 s length polish from its last drawing. */
TEST_CASE("planar slots: stage A lazy loop under D446 on T1 and T2 (step 1)", "[.][slotsstep1]")
{
	const char *hub_env = std::getenv("PLANAR_SLOTS_HUB");
	const JunctionSpec spec = MakeTestJunctionSpec();
	std::ofstream md(std::getenv("PLANAR_STEP1_OUT") != nullptr ? std::getenv("PLANAR_STEP1_OUT") : "slots-step1.md", std::ios::app);
	for (const std::string name : {"T1", "T2"}) {
		if (hub_env != nullptr && name != hub_env) continue;
		std::string error, why;
		const PlanarTopology topology = ParsePlanarTopology(PlanarTText(name[1] - '0'), &error);
		REQUIRE(error.empty());
		const PlanarStructureResult sr = BuildPlanarStructure(topology, spec, 1);
		REQUIRE(!sr.variants.empty());
		PlanarShapeVariant base;
		REQUIRE(ShapePlanarStructureVariant(sr.variants[0], base, why));
		for (PlanarShapeTrack &track : base.tracks) {
			std::vector<uint32_t> middles = track.flex;
			std::ranges::sort(middles);
			for (size_t j = middles.size(); j-- > 0;) {
				const uint32_t m = middles[j];
				track.pieces.erase(track.pieces.begin() + m, track.pieces.begin() + m + 2);
				for (uint32_t &e : track.event_piece) {
					if (e > m) e -= 2;
				}
			}
			track.flex.clear();
		}
		PlanarShapeVariant v = base;
		REQUIRE(PlacePlanarShapeFlex(v, why, true, false, 1));
		const X1Fixture fa(v);
		PlanarDrawOptions sa = SlotsOptions(fa, DEFAULT_RULES);
		sa.node_gaps = false;
		sa.tidy = false;
		const char *mode_env = std::getenv("PLANAR_STEP1_MODE");
		const std::string mode = mode_env != nullptr ? mode_env : "";
		if (mode == "tidy" || mode == "tidynear") sa.tidy = true;
		const int near_margin = (mode == "near" || mode == "tidynear") ? 4 : 0;
		const std::string tag = mode.empty() ? std::format("STEP1 {}", name) : std::format("STEP1 {} {}", name, mode);
		md << std::format("\n### {}\n\n| stage | round | status | ms | pairs fixed | pairs free | binaries | failing pairs | freed |\n|---|---|---|---|---|---|---|---|---|\n", tag) << std::flush;
		const LazyRun a = LazyLoop(v, sa, {}, tag, "A", md, false, true, near_margin);
		std::string summary = std::format("\n- {} stage A: rounds {} converged {} stop '{}' pairs with rows {} of {} total ms {}\n", tag, a.rounds, a.converged, a.stop, a.list.size(), a.all_pairs, static_cast<int>(a.ms));
		if (a.converged) {
			int biggest = 0;
			for (const auto &track : a.last.points) {
				for (const auto &p : track) biggest = std::max({biggest, std::abs(p[0]), std::abs(p[1])});
			}
			PlanarDrawOptions sp = sa;
			sp.tidy = true;
			sp.keep_apart_out = true;
			sp.only_pairs_on = true;
			sp.only_pairs = a.list;
			sp.coord_max = std::max(64, (2 * biggest + 15) / 16 * 16);
			sp.start_points = &a.last.points;
			const PlanarDrawResult pr = CheckPlanarDrawable(v, 3.0, sp);
			const auto total = [](const std::vector<std::vector<double>> &l) {
				double sum = 0;
				for (const auto &t : l) {
					for (const double x : t) sum += x;
				}
				return sum;
			};
			size_t fail = 0;
			if (pr.status == "Optimal") {
				for (const PlanarKeepApartPair &p : pr.keep_apart) {
					if (!p.skipped.empty() || p.candidates.empty()) continue;
					if (std::ranges::find(a.list, p.tag) == a.list.end() && !PlanarKeepApartHolds(p, pr.points)) fail++;
				}
			}
			summary += std::format("- {} polish: {} {} ms start_values {} length before {} after {} failing without rows {}\n", tag, pr.status, static_cast<int>(pr.ms), pr.start_values, total(a.last.lengths), pr.status == "Optimal" ? std::format("{}", total(pr.lengths)) : "-", pr.status == "Optimal" ? std::format("{}", fail) : "-");
		}
		md << summary << std::flush;
		std::cout << summary << std::flush;
	}
}

/** Slots step 2a (diagnosis): where T2's contracted writing (D447 stage A, then the lazy-3 contraction) breaks the node turns F6/F8; the failing nodes with the pieces at them before and after contraction, and the listing solve of stage B. */
TEST_CASE("planar slots: T2 contracted node turns (step 2a)", "[.][slotsstep2diag]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	std::ostringstream out;
	std::string error, why;
	const PlanarTopology topology = ParsePlanarTopology(PlanarTText(2), &error);
	REQUIRE(error.empty());
	const PlanarStructureResult sr = BuildPlanarStructure(topology, spec, 1);
	REQUIRE(!sr.variants.empty());
	PlanarShapeVariant base;
	REQUIRE(ShapePlanarStructureVariant(sr.variants[0], base, why));
	for (PlanarShapeTrack &track : base.tracks) {
		std::vector<uint32_t> middles = track.flex;
		std::ranges::sort(middles);
		for (size_t j = middles.size(); j-- > 0;) {
			const uint32_t m = middles[j];
			track.pieces.erase(track.pieces.begin() + m, track.pieces.begin() + m + 2);
			for (uint32_t &e : track.event_piece) {
				if (e > m) e -= 2;
			}
		}
		track.flex.clear();
	}
	PlanarShapeVariant v = base;
	REQUIRE(PlacePlanarShapeFlex(v, why, true, false, 1));

	/* Stage A (D447): no keep-apart rows, tidy on. */
	const X1Fixture fa(v);
	PlanarDrawOptions sa = SlotsOptions(fa, DEFAULT_RULES);
	sa.node_gaps = false;
	sa.keep_apart_out = true;
	sa.only_pairs_on = true;
	const PlanarDrawResult a = CheckPlanarDrawable(v, 15.0, sa);
	out << std::format("## T2 offers 1 d443 off, stage A (no keep-apart rows, tidy on): status {} ms {}\n\n", a.status, static_cast<int>(a.ms));
	REQUIRE(a.status == "Optimal");

	/* The contraction, as the lazy test's stage B does it. */
	PlanarShapeVariant vc = v;
	auto points = a.points;
	auto lengths = a.lengths;
	for (size_t t = 0; t < vc.tracks.size(); t++) {
		PlanarShapeTrack &track = vc.tracks[t];
		std::vector<uint32_t> middles = track.flex;
		std::ranges::sort(middles);
		std::vector<bool> kept;
		for (const uint32_t m : middles) kept.push_back(a.lengths[t][m] >= 0.5);
		for (size_t j = middles.size(); j-- > 0;) {
			if (kept[j]) continue;
			const uint32_t m = middles[j];
			track.pieces.erase(track.pieces.begin() + m, track.pieces.begin() + m + 2);
			for (uint32_t &e : track.event_piece) {
				if (e > m) e -= 2;
			}
			points[t].erase(points[t].begin() + m, points[t].begin() + m + 2);
			lengths[t][m - 1] += lengths[t][m] + lengths[t][m + 1];
			lengths[t].erase(lengths[t].begin() + m, lengths[t].begin() + m + 2);
		}
		track.steps.clear();
		uint32_t gone = 0;
		for (size_t j = 0; j < middles.size(); j++) {
			if (kept[j]) track.steps.push_back(middles[j] - 2 * gone);
			else gone++;
		}
		track.flex.clear();
	}
	int biggest = 0;
	for (const auto &track : points) {
		for (const auto &p : track) biggest = std::max({biggest, std::abs(p[0]), std::abs(p[1])});
	}
	PlanarShapeVariant plain = vc;
	for (PlanarShapeTrack &track : plain.tracks) track.steps.clear();
	Contracted a_cv;
	a_cv.v = plain;
	a_cv.points = points;
	const std::vector<std::vector<double>> ref_port_zero = PortZeroFrom(points);
	const std::vector<std::vector<double>> ref_along = AlongFrom(a_cv);
	X1Fixture fb(plain);
	std::vector<std::string> lines;
	const bool node_ok = NodeTurnsHold(fb, a_cv, &lines);
	out << std::format("node turns (F6/F8) on the contracted drawing: {}, {} failing line(s)\n\n", node_ok ? "hold" : "FAIL", lines.size());

	const auto &map = v.structure.map;
	std::map<std::string, uint32_t> by_name;
	for (uint32_t t = 0; t < map.tracks.size(); t++) by_name[map.tracks[t].name] = t;
	const auto pt = [](const Point &p) { return std::format("({},{})", p[0], p[1]); };
	for (const std::string &line : lines) {
		out << "### " << line << "\n\n";
		/* Each track the line names, once: "NAME arriving|starting hH|zero". */
		std::set<std::pair<uint32_t, bool>> seen;
		static const std::regex token(R"((\S+) (arriving|starting) (h-?[0-9]+|zero))");
		for (std::sregex_iterator it(line.begin(), line.end(), token), end; it != end; ++it) {
			const auto found = by_name.find((*it)[1].str());
			REQUIRE(found != by_name.end());
			const uint32_t t = found->second;
			const bool arriving = (*it)[2].str() == "arriving";
			if (!seen.insert({t, arriving}).second) continue;
			const auto first4 = [&](size_t n) { return arriving ? (n > 4 ? n - 4 : 0) : 0; };
			const auto last4 = [&](size_t n) { return arriving ? n : std::min<size_t>(n, 4); };
			out << std::format("track {} ({}), point at node in stage A {}\n", map.tracks[t].name, arriving ? "arriving: last 4 pieces" : "starting: first 4 pieces", pt(arriving ? a.points[t].back() : a.points[t].front()));
			out << "- uncontracted v (piece: heading, stage A length, kind):";
			const std::vector<uint32_t> &flex = v.tracks[t].flex;
			for (size_t p = first4(v.tracks[t].pieces.size()); p < last4(v.tracks[t].pieces.size()); p++) {
				std::string kind = "fixed";
				for (const uint32_t m : flex) {
					if (p == m || p == m + 1) kind = "flex middle";
					else if (p + 1 == m || p == m + 2) kind = "flex flank";
				}
				out << std::format(" [{}: h{} len {} {}]", p, v.tracks[t].pieces[p], a.lengths[t][p], kind);
			}
			out << "\n- contracted plain (piece: heading, length):";
			for (size_t p = first4(plain.tracks[t].pieces.size()); p < last4(plain.tracks[t].pieces.size()); p++) out << std::format(" [{}: h{} len {}]", p, plain.tracks[t].pieces[p], lengths[t][p]);
			out << "\n\n";
		}
	}

	/* Stage B's listing solve, as the lazy test does it. */
	PlanarDrawOptions sb = SlotsOptions(fb, DEFAULT_RULES);
	sb.port_zero_from = &ref_port_zero;
	sb.node_gap_from = &ref_port_zero;
	sb.along_from = &ref_along;
	sb.coord_max = std::max(64, (2 * biggest + 15) / 16 * 16);
	PlanarDrawOptions listing = sb;
	listing.keep_apart_out = true;
	listing.only_pairs_on = true;
	const PlanarDrawResult lr = CheckPlanarDrawable(plain, 15.0, listing);
	out << std::format("## stage B listing solve (no keep-apart rows): status {} ms {}\n", lr.status, static_cast<int>(lr.ms));
	if (lr.status == "Infeasible") {
		out << "conflict:";
		for (size_t i = 0; i < lr.conflict.size() && i < 40; i++) out << " [" << lr.conflict[i] << "]";
		out << std::format(" ({} tags in all)\n", lr.conflict.size());
	}
	std::ofstream md(std::getenv("PLANAR_STEP2A_OUT") != nullptr ? std::getenv("PLANAR_STEP2A_OUT") : "slots-step2a.md", std::ios::app);
	md << out.str() << std::flush;
	std::cout << out.str() << std::flush;
}

/** Slots step 2b (measurement): stage B from the suite's own contraction (ContractedVariant) of stage A's drawing, on T1 and T2. */
TEST_CASE("planar slots: stage B from ContractedVariant of stage A on T1 and T2 (step 2b)", "[.][slotsstep2]")
{
	const char *hub_env = std::getenv("PLANAR_SLOTS_HUB");
	const JunctionSpec spec = MakeTestJunctionSpec();
	std::ofstream md(std::getenv("PLANAR_STEP2B_OUT") != nullptr ? std::getenv("PLANAR_STEP2B_OUT") : "slots-step2b.md", std::ios::app);
	for (const std::string name : {"T1", "T2"}) {
		if (hub_env != nullptr && name != hub_env) continue;
		std::ostringstream out;
		const auto flush = [&]() {
			md << out.str() << std::flush;
			std::cout << out.str() << std::flush;
			out.str("");
		};
		std::string error, why;
		const PlanarTopology topology = ParsePlanarTopology(PlanarTText(name[1] - '0'), &error);
		REQUIRE(error.empty());
		const PlanarStructureResult sr = BuildPlanarStructure(topology, spec, 1);
		REQUIRE(!sr.variants.empty());
		PlanarShapeVariant base;
		REQUIRE(ShapePlanarStructureVariant(sr.variants[0], base, why));
		for (PlanarShapeTrack &track : base.tracks) {
			std::vector<uint32_t> middles = track.flex;
			std::ranges::sort(middles);
			for (size_t j = middles.size(); j-- > 0;) {
				const uint32_t m = middles[j];
				track.pieces.erase(track.pieces.begin() + m, track.pieces.begin() + m + 2);
				for (uint32_t &e : track.event_piece) {
					if (e > m) e -= 2;
				}
			}
			track.flex.clear();
		}
		PlanarShapeVariant v = base;
		REQUIRE(PlacePlanarShapeFlex(v, why, true, false, 1));
		const std::string tag = std::format("STEP2B {}", name);
		out << std::format("\n### {}\n\n", tag);

		/* Stage A: no keep-apart rows, tidy on (D447). */
		X1Fixture fa(v);
		PlanarDrawOptions sa = SlotsOptions(fa, DEFAULT_RULES);
		sa.node_gaps = false;
		sa.keep_apart_out = true;
		sa.only_pairs_on = true;
		const PlanarDrawResult a = CheckPlanarDrawable(v, 15.0, sa);
		out << std::format("- {} stage A (no keep-apart rows, tidy on): {} {} ms\n", tag, a.status, static_cast<int>(a.ms));
		flush();
		REQUIRE(a.status == "Optimal");

		/* The geometry of stage A's integer lengths. */
		Geometry g;
		for (uint32_t t = 0; t < fa.n; t++) {
			for (uint32_t k = 0; k < a.lengths[t].size(); k++) {
				REQUIRE(std::abs(a.lengths[t][k] - std::round(a.lengths[t][k])) < 1e-6);
				g[fa.Name(t)][k] = static_cast<int>(std::lround(a.lengths[t][k]));
			}
		}
		/* The fixture reads the drawing: V's headings, the drawing's start points and lengths (as the answer-key test does). */
		{
			std::map<std::string, std::vector<int>> heads;
			for (uint32_t t = 0; t < fa.n; t++) {
				heads[fa.Name(t)] = v.tracks[t].pieces;
				fa.starts[fa.Name(t)] = a.points[t].front();
			}
			fa.SetUp(heads, g);
		}
		const std::optional<Contracted> cv = ContractedVariant(fa, g);
		if (!cv.has_value()) {
			out << std::format("- {}: ContractedVariant has no value (other shape); no stage B\n", tag);
			flush();
			continue;
		}
		/* The contraction does not move the drawing. */
		{
			const Point off{a.points[0][0][0] - cv->points[0][0][0], a.points[0][0][1] - cv->points[0][0][1]};
			for (uint32_t t = 0; t < fa.n; t++) {
				for (size_t i = 0; i < cv->points[t].size(); i++) {
					const Point q{cv->points[t][i][0] + off[0], cv->points[t][i][1] + off[1]};
					if (std::ranges::find(a.points[t], q) == a.points[t].end()) {
						out << std::format("- {} STOP: contracted point not in stage A: track {} index {} contracted ({},{}) translated ({},{})\n", tag, fa.Name(t), i, cv->points[t][i][0], cv->points[t][i][1], q[0], q[1]);
						flush();
						FAIL("contraction moves the drawing");
					}
				}
			}
		}
		PlanarShapeVariant plain = cv->v;
		for (PlanarShapeTrack &track : plain.tracks) track.steps.clear();
		bool turns_ok = true;
		for (size_t t = 0; t < plain.tracks.size(); t++) {
			const auto &ps = plain.tracks[t].pieces;
			const auto bad = std::ranges::adjacent_find(ps, [](int x, int y) { return std::abs(x - y) > 1; });
			if (bad != ps.end()) {
				turns_ok = false;
				out << std::format("- turn check: track {} pieces {} and {}: headings {} then {}\n", t, bad - ps.begin(), bad - ps.begin() + 1, *bad, *(bad + 1));
			}
		}
		Contracted plain_cv;
		plain_cv.v = plain;
		plain_cv.points = cv->points;
		X1Fixture fb(plain);
		std::vector<std::string> lines;
		const uint32_t moved0 = g_turn_moved, rejected0 = g_turn_rejected;
		const bool node_ok = NodeTurnsHold(fb, plain_cv, &lines);
		out << std::format("- {} contracted variant: per-track turns {}, node turns (F6/F8) {} ({} failing line(s)); g_turn_moved {} -> {}, g_turn_rejected {} -> {}\n", tag, turns_ok ? "hold" : "FAIL", node_ok ? "hold" : "FAIL", lines.size(), moved0, g_turn_moved, rejected0, g_turn_rejected);
		for (const std::string &line : lines) out << "  - " << line << "\n";
		flush();

		/* Stage B on the contracted variant. */
		const std::vector<std::vector<double>> ref_port_zero = PortZeroFrom(cv->points);
		const std::vector<std::vector<double>> ref_along = AlongFrom(plain_cv);
		int biggest = 0;
		for (const auto &track : cv->points) {
			for (const auto &p : track) biggest = std::max({biggest, std::abs(p[0]), std::abs(p[1])});
		}
		PlanarDrawOptions sb = SlotsOptions(fb, DEFAULT_RULES);
		sb.port_zero_from = &ref_port_zero;
		sb.node_gap_from = &ref_port_zero;
		sb.along_from = &ref_along;
		sb.coord_max = std::max(64, (2 * biggest + 15) / 16 * 16);
		sb.tidy = false;
		PlanarDrawOptions listing = sb;
		listing.keep_apart_out = true;
		listing.only_pairs_on = true;
		const PlanarDrawResult lr = CheckPlanarDrawable(plain, 15.0, listing);
		out << std::format("- {} stage B listing solve (no rows): {} {} ms\n", tag, lr.status, static_cast<int>(lr.ms));
		flush();
		if (lr.status == "Infeasible") {
			LazyConflict(plain, listing, tag, "B (no keep-apart rows)", md);
			continue;
		}
		md << std::format("\n| stage | round | status | ms | pairs fixed | pairs free | binaries | failing pairs | freed |\n|---|---|---|---|---|---|---|---|---|\n") << std::flush;
		const LazyRun b = LazyLoop(plain, sb, {}, tag, "B", md, false, true);
		out << std::format("- {} stage B: rounds {} converged {} stop '{}' pairs with rows {} of {} total ms {}\n", tag, b.rounds, b.converged, b.stop, b.list.size(), b.all_pairs, static_cast<int>(b.ms));
		if (!b.converged) {
			flush();
			continue;
		}
		int biggest_b = 0;
		for (const auto &track : b.last.points) {
			for (const auto &p : track) biggest_b = std::max({biggest_b, std::abs(p[0]), std::abs(p[1])});
		}
		PlanarDrawOptions sp = sb;
		sp.tidy = true;
		sp.keep_apart_out = true;
		sp.only_pairs_on = true;
		sp.only_pairs = b.list;
		sp.coord_max = std::max(64, (2 * biggest_b + 15) / 16 * 16);
		sp.start_points = &b.last.points;
		const PlanarDrawResult pr = CheckPlanarDrawable(plain, 3.0, sp);
		const auto total = [](const std::vector<std::vector<double>> &l) {
			double sum = 0;
			for (const auto &t : l) {
				for (const double x : t) sum += x;
			}
			return sum;
		};
		out << std::format("- {} polish: {} {} ms length before {} after {}\n", tag, pr.status, static_cast<int>(pr.ms), total(b.last.lengths), pr.status == "Optimal" ? std::format("{}", total(pr.lengths)) : "-");
		Contracted jc;
		jc.v = plain;
		jc.points = pr.status == "Optimal" ? pr.points : b.last.points;
		const RowsVerdict jv = DrawingProgramJudge(DEFAULT_RULES)(fb, jc, true);
		std::string verdict = std::format("judge {}", jv.accept ? "accept" : "refuse");
		for (const std::string &rule : jv.rules) verdict += " [" + rule + "]";
		const RowsVerdict jn = DrawingProgramJudge(DEFAULT_RULES)(fb, jc, false);
		verdict += std::format(" (without interface rows: judge {}", jn.accept ? "accept" : "refuse");
		for (const std::string &rule : jn.rules) verdict += " [" + rule + "]";
		verdict += ")";
		std::vector<std::string> notes;
		const PlanarDomainVerdict d = CheckPlanarDomain(PlanarDomainInputOf(cv->v, jc.points, notes));
		std::set<std::string> breaks;
		for (const PlanarDomainBreak &br : d.breaks) breaks.insert(br.rule);
		verdict += std::format("; domain {} notes {} breaks {}", d.legal ? "accept" : "refuse", notes.size(), d.breaks.size());
		for (const std::string &rule : breaks) verdict += " [" + rule + "]";
		out << "- " << tag << " " << verdict << "\n";
		flush();
	}
}

TEST_CASE("planar slots: stage A turn breaks classified (step 2c)", "[.][slotsstep2cdiag]")
{
	const char *hub_env = std::getenv("PLANAR_SLOTS_HUB");
	const JunctionSpec spec = MakeTestJunctionSpec();
	std::ofstream md(std::getenv("PLANAR_STEP2C_OUT") != nullptr ? std::getenv("PLANAR_STEP2C_OUT") : "slots-step2c.md", std::ios::app);
	for (const std::string name : {"T1", "T2"}) {
		if (hub_env != nullptr && name != hub_env) continue;
		std::ostringstream out;
		const auto flush = [&]() {
			md << out.str() << std::flush;
			std::cout << out.str() << std::flush;
			out.str("");
		};
		std::string error, why;
		const PlanarTopology topology = ParsePlanarTopology(PlanarTText(name[1] - '0'), &error);
		REQUIRE(error.empty());
		const PlanarStructureResult sr = BuildPlanarStructure(topology, spec, 1);
		REQUIRE(!sr.variants.empty());
		PlanarShapeVariant base;
		REQUIRE(ShapePlanarStructureVariant(sr.variants[0], base, why));
		for (PlanarShapeTrack &track : base.tracks) {
			std::vector<uint32_t> middles = track.flex;
			std::ranges::sort(middles);
			for (size_t j = middles.size(); j-- > 0;) {
				const uint32_t m = middles[j];
				track.pieces.erase(track.pieces.begin() + m, track.pieces.begin() + m + 2);
				for (uint32_t &e : track.event_piece) {
					if (e > m) e -= 2;
				}
			}
			track.flex.clear();
		}
		PlanarShapeVariant v = base;
		REQUIRE(PlacePlanarShapeFlex(v, why, true, false, 1));
		const std::string tag = std::format("STEP2C {}", name);
		out << std::format("\n### {}\n\n", tag);

		/* Stage A: no keep-apart rows, tidy on (D447). */
		X1Fixture fa(v);
		PlanarDrawOptions sa = SlotsOptions(fa, DEFAULT_RULES);
		sa.node_gaps = false;
		sa.keep_apart_out = true;
		sa.only_pairs_on = true;
		const PlanarDrawResult a = CheckPlanarDrawable(v, 15.0, sa);
		out << std::format("- {} stage A (no keep-apart rows, tidy on): {} {} ms\n", tag, a.status, static_cast<int>(a.ms));
		flush();
		REQUIRE(a.status == "Optimal");

		const auto &map = v.structure.map;
		/* Per track, per piece: the length of stage A's drawing, and the piece's role in v. */
		std::vector<std::vector<int>> len(fa.n);
		std::vector<std::vector<std::string>> role(fa.n);
		for (uint32_t t = 0; t < fa.n; t++) {
			const PlanarShapeTrack &tr = v.tracks[t];
			REQUIRE(a.points[t].size() == tr.pieces.size() + 1);
			for (size_t k = 0; k < tr.pieces.size(); k++) {
				const Point &p = a.points[t][k], &q = a.points[t][k + 1];
				len[t].push_back(std::max(std::abs(q[0] - p[0]), std::abs(q[1] - p[1])));
				const auto is_mid = [&](size_t m) { return std::ranges::find(tr.flex, static_cast<uint32_t>(m)) != tr.flex.end(); };
				role[t].push_back(is_mid(k) ? "middle" : ((k > 0 && is_mid(k - 1)) || is_mid(k + 1)) ? "flank" : "fixed");
			}
		}
		const auto desc = [&](uint32_t t, size_t k) { return std::format("{}[{} h{} {}]", k, role[t][k], v.tracks[t].pieces[k], len[t][k]); };
		const auto heading_of = [&](uint32_t t, size_t k) { return ((v.tracks[t].pieces[k] % 8) + 8) % 8; };

		/* 1. Per-track breaks. */
		std::map<char, int> track_count;
		out << "\nPer-track breaks:\n\n";
		for (uint32_t t = 0; t < fa.n; t++) {
			const PlanarShapeTrack &tr = v.tracks[t];
			int prev = -1;
			for (size_t j = 0; j < tr.pieces.size(); j++) {
				if (len[t][j] == 0) continue;
				if (prev >= 0) {
					const int d = std::abs(heading_of(t, prev) - heading_of(t, j));
					if (std::min(d, 8 - d) > 1) {
						std::string zeros;
						int flanks = 0;
						bool fixed = false, crossing = false;
						std::vector<size_t> flank_at;
						for (size_t k = prev + 1; k < j; k++) {
							zeros += std::format(" {}[h{} {}]", k, tr.pieces[k], role[t][k]);
							if (role[t][k] == "flank") { flanks++; flank_at.push_back(k); }
							if (role[t][k] == "fixed") fixed = true;
						}
						for (size_t k = prev; k <= j; k++) {
							if (std::ranges::find(tr.event_piece, static_cast<uint32_t>(k)) != tr.event_piece.end()) crossing = true;
						}
						const auto is_mid = [&](size_t m) { return std::ranges::find(tr.flex, static_cast<uint32_t>(m)) != tr.flex.end(); };
						char cls = 'C';
						std::string note;
						if (flanks == 1 && flank_at[0] > 0 && is_mid(flank_at[0] - 1) && is_mid(flank_at[0] + 1)) {
							cls = 'A';
						} else if (fixed) {
							cls = 'B';
						} else {
							note = std::format(" ({} flank(s) in the run, none or not between two middles)", flanks);
						}
						track_count[cls]++;
						out << std::format("- track {} (idx {}): i {} j {}; zero run:{}; crossing on i..j (event_piece): {}; class {}{}\n", map.tracks[t].name, t, desc(t, prev), desc(t, j), zeros.empty() ? " none" : zeros, crossing ? "yes" : "no", cls, note);
					}
				}
				prev = static_cast<int>(j);
			}
		}
		if (track_count.empty()) {
			out << std::format("- {} STOP: no per-track break found\n", tag);
			flush();
			FAIL("no per-track break");
		}

		/* 2. Node breaks, on a contracted fixture with zero-length end pieces read through. */
		Contracted ec;
		ec.v = v;
		ec.points.assign(fa.n, {});
		for (uint32_t t = 0; t < fa.n; t++) {
			size_t lo = 0, hi = len[t].size();
			while (lo + 1 < hi && len[t][lo] == 0) lo++;
			while (hi - 1 > lo && len[t][hi - 1] == 0) hi--;
			PlanarShapeTrack &tr = ec.v.tracks[t];
			tr.pieces.assign(v.tracks[t].pieces.begin() + lo, v.tracks[t].pieces.begin() + hi);
			ec.points[t].assign(a.points[t].begin() + lo, a.points[t].begin() + hi + 1);
		}
		X1Fixture fe(ec.v);
		std::vector<std::string> lines;
		const bool node_ok = NodeTurnsHold(fe, ec, &lines);
		out << std::format("\nNode breaks (ends read through): node turns {} ({} failing line(s))\n\n", node_ok ? "hold" : "FAIL", lines.size());
		std::map<std::string, int> node_count;
		for (const std::string &line : lines) {
			out << "- " << line << "\n";
			std::string rule = line.substr(line.find("rule ") + 5);
			rule = rule.substr(0, rule.find(' '));
			node_count["rule " + rule]++;
			std::string legs = line.substr(line.find("legs: ") + 6);
			size_t pos = 0;
			while (pos <= legs.size()) {
				size_t e = legs.find(" ; ", pos);
				if (e == std::string::npos) e = legs.size();
				const std::string leg = legs.substr(pos, e - pos);
				pos = e + 3;
				std::istringstream is(leg);
				std::string lname, dir;
				is >> lname >> dir;
				const bool arriving = dir == "arriving";
				uint32_t t = fa.n;
				for (uint32_t k = 0; k < fa.n; k++) {
					if (map.tracks[k].name == lname) { t = k; break; }
				}
				REQUIRE(t < fa.n);
				const size_t n = v.tracks[t].pieces.size();
				std::string between;
				int nz = -1;
				for (size_t s = 0; s < n; s++) {
					const size_t k = arriving ? n - 1 - s : s;
					if (len[t][k] != 0) { nz = static_cast<int>(k); break; }
					between += std::format(" {}[h{} {}]", k, v.tracks[t].pieces[k], role[t][k]);
				}
				const size_t endk = arriving ? n - 1 : 0;
				std::string cls;
				if (nz < 0) {
					cls = "N3 (leg all zero)";
				} else if (role[t][nz] == "middle") {
					cls = "N1";
				} else if (role[t][nz] == "fixed") {
					cls = "N2";
				} else {
					cls = "N3 (non-zero end piece is a flank)";
				}
				node_count[cls.substr(0, 2)]++;
				out << std::format("  - leg {}: first non-zero {}; zeros to node:{}; Shape end piece {} h{}; class {}\n", leg, nz < 0 ? std::string("none") : desc(t, nz), between.empty() ? " none" : between, endk, v.tracks[t].pieces[endk], cls);
			}
		}
		out << std::format("\nCounts {}: per-track", tag);
		for (const auto &[c, k] : track_count) out << std::format(" {}={}", c, k);
		out << "; node";
		for (const auto &[c, k] : node_count) out << std::format(" [{}]={}", c, k);
		out << "\n";
		flush();
	}
}
