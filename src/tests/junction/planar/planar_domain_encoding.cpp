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
		const Point &p = points[r.p[0]][r.p[1]];
		const Point &q = points[r.q[0]][r.q[1]];
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

/** F6 and F8: at every node, each leg's heading at the node and the trunk's differ by at most one eighth (the contracted turn rule, F3, through nodes; F6), and no two legs of the node share a heading (the node shape, D400; F8). */
bool NodeTurnsHold(const X1Fixture &f, const Contracted &cv)
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
	for (uint32_t n = 0; n < map.expanded.nodes.size(); n++) {
		const Site site = site_of(n);
		if (site.ending.empty() || site.starting.empty()) continue;
		const bool trunk_ends = site.ending.size() == 1 && site.starting.size() > 1;
		const uint32_t trunk = trunk_ends ? site.ending[0] : site.starting[0];
		/* F8: the legs' headings at the node are pairwise distinct (a zero leg is skipped). */
		std::set<int> leg_headings;
		for (const bool arriving : {true, false}) {
			for (const uint32_t t : arriving ? site.ending : site.starting) {
				if (t == trunk || zero(t)) continue;
				const int h = heading(n, t, arriving);
				if (h >= 0 && !leg_headings.insert(h).second) return false;
			}
		}
		const std::optional<int> trunk_h = trunk_heading(n, trunk, trunk_ends, 0);
		if (!trunk_h) continue;
		if (*trunk_h < 0) return false;
		for (const bool arriving : {true, false}) {
			for (const uint32_t t : arriving ? site.ending : site.starting) {
				if (t == trunk || zero(t)) continue;
				const int h = heading(n, t, arriving);
				if (h < 0) return false;
				const int d = ((*trunk_h - h) % 8 + 8) % 8;
				if (std::min(d, 8 - d) > 1) return false;
			}
		}
	}
	return true;
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

RowsJudge DrawingProgramJudge(const std::string &rule_set)
{
	/* PLANAR_DOMAIN_RULES (a measurement): plain, span, zero or all (default) strict keep-apart rules. */
	const char *rules_env = rule_set.empty() ? std::getenv("PLANAR_DOMAIN_RULES") : nullptr;
	std::string rules = !rule_set.empty() ? rule_set : rules_env == nullptr ? "all" : rules_env;
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
		PlanarDrawOptions base_options = f.options;
		base_options.along_from = &along_from;
		base_options.node_lead = lead;
		base_options.end_node = endnode;
		base_options.cross_piece = crosspiece;
		base_options.grid = grid;
		PlanarDrawOptions o = base_options;
		o.port_zero = portzero;
		o.port_zero_from = &port_zero_from;
		o.node_gap_from = &port_zero_from;
		if (rules != "all") o.strict_span = o.strict_zero = o.crossing_flanks = false;
		if (rules == "span" || rules == "zero") {
			o.strict_span = rules == "span";
			o.strict_zero = rules == "zero";
		}
		o.keep_apart_out = true;
		o.build_only = true;
		if (!interface_rows) o.skip_rows = {"port ", "box"};
		ro.keep = CheckPlanarDrawable(plain, 5.0, o);
		PlanarDrawOptions p = base_options;
		p.port_zero = portzero;
		p.port_zero_from = &port_zero_from;
		p.node_gap_from = &port_zero_from;
		p.strict_span = p.strict_zero = p.crossing_flanks = false;
		p.keep_apart_out = true;
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
					} else if (d.legal) {
						nc.c = accept ? BOTH_ACCEPT : STRICT;
					} else {
						/* Loose off interface: the domain refuses, the full rows refuse, and the rows without the port and box rows accept (report only). */
						nc.c = accept ? LOOSE : judge(f, *cv, false).accept ? LOOSE_OFF : BOTH_REJECT;
					}
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
					const auto &a = c.points[r.p[0]][r.p[1]], &b = c.points[r.q[0]][r.q[1]];
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
