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
#include <chrono>
#include <climits>
#include <cstdlib>
#include <fstream>
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

/** X1's variant with B1's shape text and crossing piece as given, loaded; the flex pairs are not placed. */
bool LoadEncVariant(const std::string &b1_shape, const std::string &b1_at, PlanarShapeVariant &v, std::string &why, std::vector<std::string> *promise = nullptr)
{
	const std::string topo = "topology X1\nport W in E: AC1\nport S in N: BD1\nport N out N: C1, B1\nport E out E: A1, D1\nnode AC1 -> C1, A1\nnode BD1 -> B1, D1\ncross A1: B1<\ncross B1: A1>\n";
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

/** A geometry: per track name, piece index to length. */
using Geometry = std::map<std::string, std::map<uint32_t, int>>;

/** X1 with flex: the variant, its track indices and headings, and the base geometry (the brief's section 5). */
struct X1Fixture {
	PlanarShapeVariant v;
	uint32_t n = 0;
	std::map<std::string, uint32_t> idx;
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
		for (uint32_t t = 0; t < n; t++) idx[v.structure.map.expanded.lines[t].name] = t;
		starts = {{"AC1", {0, 5}}, {"A1", {4, 5}}, {"C1", {4, 5}}, {"BD1", {11, 12}}, {"B1", {11, 10}}, {"D1", {11, 10}}};
		options.clearance = options.clear_rows = options.pairwise = options.pairwise_all_faces = true;
		options.strict_span = options.strict_zero = options.crossing_flanks = true;
		options.node_gaps = true;
		options.bays = options.curves = false;
		options.skip_rows = {"port "};
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

	/** The points of a geometry (unlisted pieces have length 0). */
	std::vector<Points> PointsOf(const Geometry &g) const
	{
		std::vector<Points> points(n);
		for (const auto &[name, h] : heads) {
			Points &p = points[idx.at(name)];
			Point cur = starts.at(name);
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

	const std::string &Name(uint32_t t) const { return v.structure.map.expanded.lines[t].name; }
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
		if (!LoadEncVariant(shape, "A1@1", v, why, &promise)) {
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
		"topology M1\nport W in E: AC1\nport S in N: BD1\nport N out N: C1\nport E out E: AB1, D1\nnode AC1 -> C1, A1\nnode BD1 -> B1, D1\nnode A1, B1 -> AB1\n",
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
	std::array<uint32_t, 6> counts{}; ///< Polylines: both accept, both reject, too strict, too loose; then off grid geometries, then other shape.
	std::array<uint32_t, 6> geoms{}; ///< The enumerated geometries behind each.
	std::set<std::string> seen; ///< The polylines counted.
	std::map<std::string, Group> strict;
	std::map<std::string, Group> loose;
	std::vector<Example> other; ///< The first other shape polylines.
};

uint32_t g_lead_rows_seen = 0; ///< The node lead rows over the contracted variants judged in the last report.

std::string FamilyOf(const std::string &tag)
{
	return tag.substr(0, tag.find(' '));
}

const char *const CLASS_NAMES[6] = {"both accept", "both reject", "too strict", "too loose", "off grid (geometries)", "other shape"};

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

/** One polyline's contracted writing (D432): the variant with only the pieces that have length, and its points (the polyline's corners). */
struct Contracted {
	PlanarShapeVariant v;
	std::vector<Points> points;
};

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
	for (const auto &[name, h] : f.heads) {
		const uint32_t t = f.idx.at(name);
		Lengths lens(h.size(), 0);
		const auto it = g.find(name);
		if (it != g.end()) {
			for (const auto &[k, len] : it->second) lens[k] = len;
		}
		const std::vector<Lengths> reps = Representations(h, PolylineRuns(h, lens), 1, nullptr, &f.v.tracks[t].flex);
		if (reps.empty()) return std::nullopt;
		PlanarShapeTrack &tr = out.v.tracks[t];
		tr.pieces.clear();
		tr.flex.clear();
		tr.steps.clear();
		std::vector<int> kept;
		for (size_t k = 0; k < h.size(); k++) {
			if (reps[0][k] <= 0) continue;
			const bool middle = std::ranges::find(f.v.tracks[t].flex, static_cast<uint32_t>(k)) != f.v.tracks[t].flex.end();
			if (!tr.pieces.empty() && tr.pieces.back() == h[k]) {
				kept.back() += reps[0][k];
				if (middle && std::ranges::find(tr.steps, static_cast<uint32_t>(tr.pieces.size() - 1)) == tr.steps.end()) tr.steps.push_back(static_cast<uint32_t>(tr.pieces.size() - 1));
			} else {
				if (middle) tr.steps.push_back(static_cast<uint32_t>(tr.pieces.size()));
				tr.pieces.push_back(h[k]);
				kept.push_back(reps[0][k]);
			}
		}
		Point cur = f.starts.at(name);
		out.points[t].push_back(cur);
		for (size_t k = 0; k < kept.size(); k++) {
			const int hh = ((tr.pieces[k] % 8) + 8) % 8;
			cur[0] += kept[k] * STEPS[hh][0];
			cur[1] += kept[k] * STEPS[hh][1];
			out.points[t].push_back(cur);
		}
	}
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
};

/** The read-outs on one contracted variant. */
struct Readouts {
	PlanarDrawResult keep; ///< The keep-apart pairs.
	PlanarDrawResult program; ///< The whole program (strict rules off).
	std::vector<PlanarPointRow> lead; ///< The node lead rows (none when the rule set has no lead).
};

/** Judge our keep-apart rules against the domain on a fixture's configurations and write the report. */
void RunEncoding(const X1Fixture &f, const char *report_path, const std::string &title, const std::vector<Config> &configs, const std::string &header, const char *rules_override = nullptr, int *base_verdict = nullptr)
{
	REQUIRE((report_path != nullptr || base_verdict != nullptr));

	const auto t0 = std::chrono::steady_clock::now();
	std::map<std::string, Tally> per;
	std::map<std::string, double> cfg_wall;
	Tally total;
	std::map<std::string, PolyClass> classes;

	/* PLANAR_DOMAIN_RULES (a measurement): plain, span, zero or all (default) strict keep-apart rules. */
	const char *rules_env = rules_override != nullptr ? rules_override : std::getenv("PLANAR_DOMAIN_RULES");
	std::string rules = rules_env == nullptr ? "all" : rules_env;
	const bool crosspiece = rules.ends_with("+crosspiece");
	if (crosspiece) rules.resize(rules.size() - 11);
	const bool endnode = rules.ends_with("+endnode");
	if (endnode) rules.resize(rules.size() - 8);
	const bool lead = rules.ends_with("+lead");
	if (lead) rules.resize(rules.size() - 5);
	X1Fixture fx = f;
	fx.options.end_node = endnode;
	fx.options.cross_piece = crosspiece;

	/* The read-out of a contracted variant: no flex, so Refine is not needed unless the read-out says so. */
	auto readouts = [&](const PlanarShapeVariant &contracted) {
		Readouts r;
		/* The read-outs get the variant with steps cleared (production's steps cannot yet hold a step at a track's end); only the lead rows read them. */
		PlanarShapeVariant cv = contracted;
		for (PlanarShapeTrack &track : cv.tracks) track.steps.clear();
		PlanarDrawOptions o = fx.options;
		if (rules != "all") o.strict_span = o.strict_zero = o.crossing_flanks = false;
		if (rules == "span" || rules == "zero") {
			o.strict_span = rules == "span";
			o.strict_zero = rules == "zero";
		}
		o.keep_apart_out = true;
		o.build_only = true;
		r.keep = CheckPlanarDrawable(cv, 5.0, o);
		PlanarDrawOptions p = fx.options;
		p.strict_span = p.strict_zero = p.crossing_flanks = false;
		p.keep_apart_out = true;
		p.program_out = true;
		p.build_only = true;
		r.program = CheckPlanarDrawable(cv, 5.0, p);
		REQUIRE(r.keep.status == "built");
		REQUIRE(r.program.status == "built");
		if (lead) r.lead = PlanarNodeLeadRows(contracted);
		return r;
	};
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
	/** Whether the contracted rows accept the polyline's points: every keep-apart pair, every lead row, and the program breaks nothing. */
	auto accepts = [&](const Contracted &cv, const Readouts &ro) {
		return std::ranges::all_of(ro.keep.keep_apart, [&](const PlanarKeepApartPair &p) { return PlanarKeepApartHolds(p, cv.points); })
			&& std::ranges::all_of(ro.lead, [&](const PlanarPointRow &r) { return PlanarPointRowHolds(r, cv.points); })
			&& CheckPlanarProgramOnPoints(ro.program.program, cv.points).broken.empty();
	};
	if (base_verdict != nullptr) {
		/* Only the base polyline: 0 the rows accept it, 1 reject, 2 other shape. */
		const std::optional<Contracted> cv = ContractedVariant(f, f.base);
		if (!cv) {
			*base_verdict = 2;
			return;
		}
		*base_verdict = accepts(*cv, readouts(cv->v)) ? 0 : 1;
		return;
	}
	g_lead_rows_seen = 0;
	{
		const std::optional<Contracted> cv = ContractedVariant(f, f.base);
		WARN("base polyline: " << (!cv ? "other shape" : accepts(*cv, readouts(cv->v)) ? "accept" : "reject"));
	}

	for (const Config &cfg : configs) {
		const auto c0 = std::chrono::steady_clock::now();
		Tally &tally = per[cfg.name];
		std::vector<size_t> pos(cfg.axes.size(), 0);
		while (true) {
			Geometry g = f.base;
			std::ostringstream varied;
			for (size_t a = 0; a < cfg.axes.size(); a++) {
				const auto &[name, piece, vals] = cfg.axes[a];
				g[name][piece] = vals[pos[a]];
				varied << (a == 0 ? "" : " ") << name << "." << piece << "=" << vals[pos[a]];
			}
			const std::vector<Points> points = f.PointsOf(g);
			enum { BOTH_ACCEPT, BOTH_REJECT, STRICT, LOOSE, OFF_GRID, OTHER_SHAPE } c;
			bool on_grid = true;
			for (const Points &p : points) on_grid = on_grid && RasterPlanarDomainTrack(p).rule.empty();
			std::string pkey;
			const PolyClass *pc = nullptr;
			if (!on_grid) {
				c = OFF_GRID;
			} else {
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
						const PlanarDomainVerdict d = CheckPlanarDomain(PlanarDomainInputOf(f.v, points, notes));
						const Readouts ro = readouts(cv->v);
						g_lead_rows_seen += static_cast<uint32_t>(ro.lead.size());
						const bool accept = accepts(*cv, ro);
						if (d.legal) {
							nc.c = accept ? BOTH_ACCEPT : STRICT;
						} else {
							nc.c = accept ? LOOSE : BOTH_REJECT;
						}
						if (nc.c == STRICT) {
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
							nc.key = Join(kept);
						} else if (nc.c == LOOSE) {
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
			for (Tally *x : {&tally, &total}) {
				x->geoms[c]++;
				if (c == OFF_GRID) {
					x->counts[c]++;
					continue;
				}
				if (c == OTHER_SHAPE) {
					if (x->seen.insert(pkey).second) {
						x->counts[c]++;
						if (x->other.size() < 3) x->other.push_back({varied.str(), pc->detail});
					}
					continue;
				}
				Group *grp = c == STRICT ? &x->strict[pc->key] : c == LOOSE ? &x->loose[pc->key] : nullptr;
				if (grp != nullptr) grp->geometries++;
				if (!x->seen.insert(pkey).second) continue;
				x->counts[c]++;
				if (grp != nullptr && grp->count++ < 3) grp->examples.push_back({varied.str(), pc->detail});
			}
			size_t a = 0;
			while (a < cfg.axes.size()) {
				if (++pos[a] < std::get<2>(cfg.axes[a]).size()) break;
				pos[a++] = 0;
			}
			if (a == cfg.axes.size()) break;
		}
		cfg_wall[cfg.name] = std::chrono::duration<double>(std::chrono::steady_clock::now() - c0).count();
	}
	const double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

	std::ofstream out(report_path);
	REQUIRE(out.good());
	out << "Encoding report on " << title << ": our contracted rows (judged on fixed points, no solve) against the domain, one writing per polyline (D432)\n" << header;
	out << "node lead rows over the judged contracted variants: " << g_lead_rows_seen << "\n";
	out << "enumeration wall " << wall << " s\n\n";
	auto write = [&](const std::string &title, const Tally &t) {
		out << "== " << title << " ==\n";
		for (int c = 0; c < 6; c++) out << "  " << CLASS_NAMES[c] << ": " << t.counts[c] << (c == 4 ? "" : " polylines") << " (" << t.geoms[c] << " geometries)\n";
		for (const Example &e : t.other) out << "    other shape, varied: " << e.varied << "\n" << e.detail;
		for (int kind = 0; kind < 2; kind++) {
			const auto &groups = kind == 0 ? t.strict : t.loose;
			out << (kind == 0 ? " too strict, by the kept values of the failing pairs:\n" : " too loose, by domain break rules:\n");
			for (const auto &[key, grp] : groups) {
				out << "  [" << key << "] polylines " << grp.count << " (geometries " << grp.geometries << ")\n";
				for (const Example &e : grp.examples) out << "    varied: " << e.varied << "\n" << e.detail;
			}
		}
		out << "\n";
	};
	for (const Config &cfg : configs) {
		out << "wall " << cfg.name << ": " << cfg_wall[cfg.name] << " s\n";
		write(cfg.name, per[cfg.name]);
	}
	write("total", total);

	out << "== base (strict rules on): failing pairs ==\n";
	{
		const std::vector<Points> base = f.PointsOf(f.base);
		std::set<std::string> kept;
		out << FailingText(f, f.ReadOut(true).keep_apart, base, kept);
	}
	out.close();
	for (const Config &cfg : configs) {
		const Tally &t = per[cfg.name];
		WARN(cfg.name << " (polylines): both accept " << t.counts[0] << ", both reject " << t.counts[1] << ", too strict " << t.counts[2] << ", too loose " << t.counts[3] << ", off grid geometries " << t.counts[4] << ", other shape " << t.counts[5] << " (geometries " << t.geoms[5] << "), wall " << cfg_wall[cfg.name] << " s");
	}
	WARN("enumeration wall " << wall << " s");
}
} // namespace

TEST_CASE("planar draw: keep-apart pairs hold on given points", "[junction][planar][planardomain]")
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

TEST_CASE("planar draw: node lead rows on X1", "[junction][planar][planardomain]")
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

TEST_CASE("planar draw: node lead rows on contracted lines", "[junction][planar][planardomain]")
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

TEST_CASE("planar draw: keep-apart on fixed points agrees with the program on X1", "[junction][planar][planardomain]")
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

TEST_CASE("planar draw: the program on fixed points agrees with the program on X1", "[junction][planar][planardomain]")
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

TEST_CASE("planar draw: the program checked by track sets", "[junction][planar][planardomain]")
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

TEST_CASE("planar draw: end runs stay off a node's switch tile", "[junction][planar][planardomain]")
{
	X1Fixture f;
	/* The first too-loose example's witness (encoding-x1-span_lead.txt, group [head]): the tunnel's end run sits on the split's switch tile (2,2). */
	const std::map<std::string, std::vector<int>> witness = {
		{"AC1", {0, 0, 0, 0, 4}}, {"BD1", {0, 0, 0, 0, 2}}, {"C1", {0, 0, 0, 0, 0, 0, 0, 0, 3, 2}},
		{"B1", {2, 2, 0, 0, 2, 0, 0, 0, 6}}, {"A1", {0, 0, 0, 0, 10, 0, 0, 0, 6}}, {"D1", {0, 0, 0, 0, 0, 0, 1, 0, 0, 8}},
	};
	Geometry g;
	for (const auto &[name, lens] : witness) {
		for (size_t k = 0; k < lens.size(); k++) {
			if (lens[k] != 0) g[name][static_cast<uint32_t>(k)] = lens[k];
		}
	}
	const std::vector<Points> points = f.PointsOf(g);
	const auto has = [](const PlanarProgramCheck &c, const std::string &tag) { return std::ranges::find(c.broken, tag) != c.broken.end(); };

	/* Red: without the option no `end 0 node` row exists, so it cannot break. */
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

TEST_CASE("planar draw: end runs count a diagonal crosser's footprint", "[junction][planar][planardomain]")
{
	X2Fixture f;
	f.options.end_node = true;
	f.options.cross_piece = true;
	/* The first too-loose example's witness (encoding-x2-span_lead_endnode_crosspiece.txt, group [connection]): B1 crosses A1 diagonally at (16, 5), its pieces occupy A1's tiles 7 and 8, but the tunnel's last end run would need tiles 9 and 10 and A1 ends at tile 9. */
	const std::map<std::string, std::vector<int>> witness = {
		{"AC1", {0, 0, 0, 0, 4}}, {"BD1", {0, 0, 0, 0, 2}}, {"C1", {0, 0, 0, 0, 0, 0, 0, 0, 3, 2}},
		{"B1", {0, 0, 0, 0, 2, 0, 0, 1, 2, 2, 0, 0, 0, 0, 0, 0, 0, 1, 4}}, {"A1", {0, 0, 0, 0, 16, 0, 0, 0, 0}}, {"D1", {0, 0, 0, 0, 0, 0, 1, 0, 0, 8}},
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

TEST_CASE("planar draw: a crossing sits on a piece with length", "[junction][planar][planardomain]")
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

TEST_CASE("planar domain: representations of a polyline", "[junction][planar][planardomain]")
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

TEST_CASE("planar domain: a polyline has one contracted writing", "[junction][planar][planardomain]")
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

TEST_CASE("planar domain: our keep-apart against the domain on X1", "[.][planardomainenc]")
{
	const char *report_path = std::getenv("PLANAR_DOMAIN_REPORT");
	REQUIRE(report_path != nullptr);
	const X1Fixture f;
	const std::vector<Config> configs = {
		{"S1", {{"B1", 2, {0, 2, 4}}, {"B1", 3, {0, 1, 2, 3, 4}}, {"B1", 4, {2, 4, 6, 8, 10, 12, 14}}, {"B1", 5, {0, 1, 2, 3, 4}}, {"B1", 6, {4}}}},
		{"S2", {{"A1", 2, {0, 2, 4}}, {"A1", 3, {0, 1, 2, 3, 4}}, {"A1", 4, {2, 4, 6, 8, 10, 12, 14, 16}}, {"A1", 5, {0, 1, 2, 3, 4}}, {"A1", 6, {4}}}},
		{"N1", {{"C1", 0, {0, 1, 2, 3}}, {"C1", 1, {0, 2, 4}}, {"C1", 2, {0, 1, 2, 3}}, {"A1", 0, {0, 2, 4}}, {"A1", 1, {0, 1, 2, 3}}}},
	};
	RunEncoding(f, report_path, "X1", configs, "");
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
	RunEncoding(f, nullptr, "X2", {none}, "", "span+lead+endnode+crosspiece", &verdict);
	CHECK(verdict == 0);
}

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
	RunEncoding(f, report_path, "X2", {c1, c2}, header.str());
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
	RunEncoding(f, nullptr, "M1", {none}, "", "span+lead+endnode+crosspiece", &verdict);
	CHECK(verdict == 0);
}

TEST_CASE("planar domain: K1 loads and its base is legal", "[junction][planar][planardomain]")
{
	const K1Fixture f;
	RequireBaseLegal(f);
	int verdict = 1;
	const Config none{"base", {}};
	RunEncoding(f, nullptr, "K1", {none}, "", "span+lead+endnode+crosspiece", &verdict);
	CHECK(verdict == 0);
}

TEST_CASE("planar domain: our keep-apart against the domain on M1", "[.][planardomainenc]")
{
	const char *report_path = std::getenv("PLANAR_DOMAIN_REPORT_M1");
	REQUIRE(report_path != nullptr);
	const M1Fixture f;
	RequireBaseLegal(f);
	const std::vector<uint32_t> &a1 = f.holders.at("A1");
	const uint32_t a_last = static_cast<uint32_t>(f.heads.at("A1").size()) - 1;
	REQUIRE(a1.size() == 1);
	REQUIRE(a1[0] == a_last);
	/* M1a: B1's pieces varied; A1's last two pieces in {0, 2, 4} each, its first piece the rest of 12 (A1's end stays at (16,5) when its last two pieces run east). One configuration per pair, as the axes are independent. */
	std::vector<Config> configs;
	for (const int x : {0, 2, 4}) {
		for (const int y : {0, 2, 4}) {
			std::ostringstream cname;
			cname << "M1a A1 tail " << x << "," << y;
			Config c{cname.str(), {}};
			AddAxes(c, f, "B1", f.holders.at("B1"), {{2, 4, 6}, {1, 2, 3}}, 0, UINT32_MAX);
			c.axes.emplace_back("A1", 0, std::vector<int>{12 - x - y});
			c.axes.emplace_back("A1", a_last - 1, std::vector<int>{x});
			c.axes.emplace_back("A1", a_last, std::vector<int>{y});
			configs.push_back(c);
		}
	}
	/* M1b: A1's pieces varied (its run holder in {8, 10, 12, 14, 16}, its inserts within two pieces of its end) against the fixed B1. */
	Config b{"M1b", {}};
	AddAxes(b, f, "A1", a1, {{8, 10, 12, 14, 16}}, a_last - 2, UINT32_MAX);
	configs.push_back(b);
	RunEncoding(f, report_path, "M1", configs, FixtureHeader(f, f.holders));
}

TEST_CASE("planar domain: our keep-apart against the domain on K1", "[.][planardomainenc]")
{
	const char *report_path = std::getenv("PLANAR_DOMAIN_REPORT_K1");
	REQUIRE(report_path != nullptr);
	const K1Fixture f;
	RequireBaseLegal(f);
	/* K1a: BC1's total length in {0, 2, 4, 6} (0 is the trident); A1's NE run and C1's SE run in 1..3; the inserts of A1 and C1 within their first three pieces. node_gaps is on (D424). */
	Config c{"K1a", {}};
	c.axes.emplace_back("BC1", f.holders.at("BC1")[0], std::vector<int>{0, 2, 4, 6});
	AddAxes(c, f, "A1", f.holders.at("A1"), {{1, 2, 3}, {}}, 0, 2);
	AddAxes(c, f, "C1", f.holders.at("C1"), {{1, 2, 3}, {}}, 0, 2);
	REQUIRE(f.options.node_gaps);
	RunEncoding(f, report_path, "K1", {c}, "node_gaps: on (D424)\n" + FixtureHeader(f, f.holders));
}
