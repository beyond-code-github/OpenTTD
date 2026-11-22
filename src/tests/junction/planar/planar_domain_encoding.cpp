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
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

#include "../../../safeguards.h"

namespace {

using Point = std::array<int, 2>;
using Points = std::vector<Point>;

/** The loaded X1 variant with its flex pairs placed. */
PlanarShapeVariant LoadX1Enc()
{
	const std::string topo = "topology X1\nport W in E: AC1\nport S in N: BD1\nport N out N: C1, B1\nport E out E: A1, D1\nnode AC1 -> C1, A1\nnode BD1 -> B1, D1\ncross A1: B1<\ncross B1: A1>\n";
	std::string error;
	const PlanarTopology t = ParsePlanarTopology(topo, &error);
	REQUIRE(error.empty());
	const JunctionSpec spec = MakeTestJunctionSpec();
	std::vector<std::string> report;
	const PlanarStructureVariant s = LoadPlanarStructureVariant(t, "tunnel T1 A1: B1\nstraight AC1: A1\nstraight BD1: B1\n", spec, report, &error);
	REQUIRE(error.empty());
	PlanarShapeVariant v;
	std::string why;
	REQUIRE(LoadPlanarShapeVariant(t, s, "shape AC1: 2\nshape A1: 2\nshape C1: 1 0\nshape BD1: 0\nshape B1: 0\nshape D1: 1 2\nat A1: B1@0\nat B1: A1@0\n", v, why));
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

	X1Fixture() : v(LoadX1Enc())
	{
		n = static_cast<uint32_t>(v.structure.map.expanded.lines.size());
		REQUIRE(n == 6);
		for (uint32_t t = 0; t < n; t++) idx[v.structure.map.expanded.lines[t].name] = t;
		heads = {
			{"AC1", {2, 1, 2, 3, 2}},
			{"A1", {2, 1, 2, 3, 2, 1, 2, 3, 2}},
			{"C1", {1, 0, 1, 2, 1, 0, -1, 0, 1, 0}},
			{"BD1", {0, -1, 0, 1, 0}},
			{"B1", {0, -1, 0, 1, 0, -1, 0, 1, 0}},
			{"D1", {1, 0, 1, 2, 1, 2, 1, 2, 3, 2}},
		};
		for (const auto &[name, h] : heads) {
			REQUIRE(idx.count(name) == 1);
			REQUIRE(v.tracks[idx[name]].pieces == h);
		}
		starts = {{"AC1", {0, 5}}, {"A1", {4, 5}}, {"C1", {4, 5}}, {"BD1", {11, 12}}, {"B1", {11, 10}}, {"D1", {11, 10}}};
		base = {
			{"AC1", {{0, 4}}},
			{"A1", {{4, 12}, {8, 4}}},
			{"C1", {{0, 3}, {9, 2}}},
			{"BD1", {{0, 2}}},
			{"B1", {{4, 6}, {8, 4}}},
			{"D1", {{0, 1}, {9, 8}}},
		};
		options.clearance = options.clear_rows = options.pairwise = options.pairwise_all_faces = true;
		options.strict_span = options.strict_zero = options.crossing_flanks = true;
		options.node_gaps = true;
		options.bays = options.curves = false;
		options.skip_rows = {"port "};
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

	const std::string &Name(uint32_t t) const { return v.structure.map.expanded.lines[t].name; }
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

/** Every piece-length assignment on the pieces whose polyline is exactly runs (at most cap; capped says it stopped there). */
std::vector<Lengths> Representations(const std::vector<int> &pieces, const Runs &runs, size_t cap, bool *capped = nullptr)
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
		/* Stay in the current run (any amount from 0), or move on to the next run once this one is used up (at least one unit). */
		if (!runs.empty() && runs[r].first == h) {
			for (int a = 0; a <= left; a += unit) {
				cur[k] = a;
				self(self, k + 1, r, left - a);
			}
			cur[k] = 0;
		} else {
			self(self, k + 1, r, left);
		}
		if (!runs.empty() && left == 0 && r + 1 < runs.size() && runs[r + 1].first == h) {
			for (int a = unit; a <= runs[r + 1].second; a += unit) {
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
	std::array<uint32_t, 6> counts{}; ///< Polylines: both accept, both reject, too strict, too loose, undecided; the last is off grid geometries.
	std::array<uint32_t, 6> geoms{}; ///< The enumerated geometries behind each.
	std::set<std::string> seen; ///< The polylines counted.
	std::map<std::string, double> undecided; ///< Per undecided polyline, its representation count product.
	std::map<std::string, Group> strict;
	std::map<std::string, Group> loose;
};

const char *const CLASS_NAMES[6] = {"both accept", "both reject", "too strict", "too loose", "undecided", "off grid (geometries)"};

/** What the rows say of a polyline: 0 an accepted representation exists, 1 none, 2 undecided; the witness is the accepted representation per track. */
struct Judged {
	int verdict = 1;
	std::vector<Lengths> witness;
};

/** A polyline's class and its example text, from the first geometry that had it. */
struct PolyClass {
	int c = 0;
	std::string key;
	std::string detail;
};

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

TEST_CASE("planar domain: our keep-apart against the domain on X1", "[.][planardomainenc]")
{
	const char *report_path = std::getenv("PLANAR_DOMAIN_REPORT");
	REQUIRE(report_path != nullptr);
	const X1Fixture f;

	const auto t0 = std::chrono::steady_clock::now();
	std::map<std::string, Tally> per;
	Tally total;

	struct Config {
		std::string name;
		std::vector<std::tuple<std::string, uint32_t, std::vector<int>>> axes;
	};
	const std::vector<Config> configs = {
		{"S1", {{"B1", 2, {0, 2, 4}}, {"B1", 3, {0, 1, 2, 3, 4}}, {"B1", 4, {2, 4, 6, 8, 10, 12, 14}}, {"B1", 5, {0, 1, 2, 3, 4}}, {"B1", 6, {4}}}},
		{"S2", {{"A1", 2, {0, 2, 4}}, {"A1", 3, {0, 1, 2, 3, 4}}, {"A1", 4, {2, 4, 6, 8, 10, 12, 14, 16}}, {"A1", 5, {0, 1, 2, 3, 4}}, {"A1", 6, {4}}}},
		{"N1", {{"C1", 0, {0, 1, 2, 3}}, {"C1", 1, {0, 2, 4}}, {"C1", 2, {0, 1, 2, 3}}, {"A1", 0, {0, 2, 4}}, {"A1", 1, {0, 1, 2, 3}}}},
	};

	std::map<std::string, Judged> verdicts; ///< Per polyline key.
		std::map<std::string, PolyClass> classes;
	std::map<std::string, double> rep_products;

	for (const Config &cfg : configs) {
		Tally &tally = per[cfg.name];
		/* PLANAR_DOMAIN_RULES (a measurement): plain, span, zero or all (default) strict keep-apart rules. */
		const char *rules_env = std::getenv("PLANAR_DOMAIN_RULES");
		std::string rules = rules_env == nullptr ? "all" : rules_env;
		const bool lead = rules.ends_with("+lead");
		if (lead) rules.resize(rules.size() - 5);
		const std::vector<PlanarPointRow> lead_rows = lead ? PlanarNodeLeadRows(f.v) : std::vector<PlanarPointRow>{};
		PlanarDrawResult readout = f.ReadOut(rules == "all");
		if (rules == "span" || rules == "zero") {
			X1Fixture g = f;
			g.options.strict_span = rules == "span";
			g.options.strict_zero = rules == "zero";
			g.options.crossing_flanks = false;
			readout = g.ReadOut(true);
		}
		REQUIRE(readout.status == "built");
		auto lengths_of = [&](const Geometry &g, const std::string &name) {
			Lengths l(f.heads.at(name).size(), 0);
			const auto it = g.find(name);
			if (it != g.end()) {
				for (const auto &[k, len] : it->second) l[k] = len;
			}
			return l;
		};
		/* Judge a polyline by all its representations (cached per polyline key). */
		auto judge = [&](const std::string &pkey, const Geometry &g) -> const Judged & {
			const auto cached = verdicts.find(pkey);
			if (cached != verdicts.end()) return cached->second;
			int verdict = 1;
			std::vector<std::vector<Lengths>> replens(f.n);
			std::vector<Lengths> witness;
			double nodes_used = 0;
			/* Per track (by track index): its representations as points, after the unary filter. */
			std::vector<std::vector<Points>> reps(f.n);
			for (const auto &[name, h] : f.heads) {
				const uint32_t t = f.idx.at(name);
				const std::vector<Lengths> all = Representations(h, PolylineRuns(h, lengths_of(g, name)), 20001);
				if (all.size() > 20000) verdict = 2;
				if (verdict == 2) continue;
				for (const Lengths &l : all) {
					Geometry one;
					for (size_t k = 0; k < l.size(); k++) {
						if (l[k] != 0) one[name][static_cast<uint32_t>(k)] = l[k];
					}
					std::vector<Points> pts(f.n);
					pts[t] = f.PointsOf(one)[t];
					bool ok = true;
					for (const PlanarKeepApartPair &p : readout.keep_apart) {
						if (p.tracks[0] == t && p.tracks[1] == t && !PlanarKeepApartHolds(p, pts)) ok = false;
					}
					for (const PlanarPointRow &r : lead_rows) {
						if (r.terms.front().point[0] == t && !PlanarPointRowHolds(r, pts)) ok = false;
					}
					if (ok) {
					reps[t].push_back(pts[t]);
					replens[t].push_back(l);
				}
				}
			}
			if (verdict != 2 && !std::ranges::any_of(reps, [](const std::vector<Points> &r) { return r.empty(); })) {
				std::vector<uint32_t> order(f.n);
				for (uint32_t t = 0; t < f.n; t++) order[t] = t;
				std::ranges::stable_sort(order, [&](uint32_t a, uint32_t b) { return reps[a].size() < reps[b].size(); });
				std::vector<uint32_t> depth_of(f.n);
				for (uint32_t d = 0; d < f.n; d++) depth_of[order[d]] = d;
				std::vector<std::vector<const PlanarKeepApartPair *>> at_depth(f.n);
				for (const PlanarKeepApartPair &p : readout.keep_apart) {
					if (p.tracks[0] >= f.n || p.tracks[1] >= f.n || p.tracks[0] == p.tracks[1]) continue;
					at_depth[std::max(depth_of[p.tracks[0]], depth_of[p.tracks[1]])].push_back(&p);
				}
				std::vector<Points> cur(f.n);
				std::vector<size_t> chosen(f.n, 0);
				uint64_t nodes = 0;
				auto search = [&](auto &&self, uint32_t d) -> bool {
					if (d == f.n) return true;
					for (size_t ri = 0; ri < reps[order[d]].size(); ri++) {
						const Points &rp = reps[order[d]][ri];
						if (++nodes > 50000) return false;
						cur[order[d]] = rp;
						chosen[order[d]] = ri;
						const bool ok = std::ranges::all_of(at_depth[d], [&](const PlanarKeepApartPair *p) { return PlanarKeepApartHolds(*p, cur); });
						if (ok && self(self, d + 1)) return true;
						if (nodes > 50000) return false;
					}
					return false;
				};
				const bool found = search(search, 0);
				nodes_used = static_cast<double>(nodes);
				verdict = found ? 0 : (nodes > 50000 ? 2 : 1);
				if (found) {
					witness.resize(f.n);
					for (uint32_t t = 0; t < f.n; t++) witness[t] = replens[t][chosen[t]];
				}
			}
			rep_products[pkey] = nodes_used;
			Judged &j = verdicts[pkey];
			j.verdict = verdict;
			j.witness = witness;
			return j;
		};
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
			enum { BOTH_ACCEPT, BOTH_REJECT, STRICT, LOOSE, UNDECIDED, OFF_GRID } c;
			bool on_grid = true;
			for (const Points &p : points) on_grid = on_grid && RasterPlanarDomainTrack(p).rule.empty();
			std::string pkey;
			const PolyClass *pc = nullptr;
			if (!on_grid) {
				c = OFF_GRID;
			} else {
				for (const auto &[name, h] : f.heads) {
					pkey += name + ":";
					std::ostringstream run_text;
					for (const auto &[hh, len] : PolylineRuns(h, lengths_of(g, name))) run_text << hh << "*" << len << ",";
					pkey += run_text.str() + ";";
				}
				const auto known = classes.find(pkey);
				if (known == classes.end()) {
					std::vector<std::string> notes;
					const PlanarDomainVerdict d = CheckPlanarDomain(PlanarDomainInputOf(f.v, points, notes));
					const Judged &j = judge(pkey, g);
					PolyClass nc;
					if (j.verdict == 2) {
						nc.c = UNDECIDED;
					} else if (d.legal) {
						nc.c = j.verdict == 0 ? BOTH_ACCEPT : STRICT;
					} else {
						nc.c = j.verdict == 0 ? LOOSE : BOTH_REJECT;
					}
					if (nc.c == STRICT) {
						std::set<std::string> kept;
						nc.detail = "      polyline " + pkey + "\n" + FailingText(f, readout.keep_apart, points, kept);
						for (const PlanarPointRow &r : lead_rows) {
							if (PlanarPointRowHolds(r, points)) continue;
							kept.insert("lead");
							nc.detail += "      " + r.tag + " fails\n";
						}
						nc.key = Join(kept);
					} else if (nc.c == LOOSE) {
						std::set<std::string> rules;
						std::ostringstream out;
						out << "      polyline " << pkey << "\n      witness piece lengths:";
						for (uint32_t t = 0; t < f.n; t++) {
							out << " " << f.Name(t) << "[";
							for (size_t k = 0; k < j.witness[t].size(); k++) out << (k == 0 ? "" : ",") << j.witness[t][k];
							out << "]";
						}
						out << "\n";
						for (const PlanarDomainBreak &b : d.breaks) {
							rules.insert(b.rule);
							std::string names;
							for (const uint32_t t : b.tracks) names += (names.empty() ? "" : ",") + f.Name(t);
							out << "      break " << b.rule << " tracks " << names << " tile (" << b.tile.x << "," << b.tile.y << ") " << b.why << "\n";
							for (const PlanarKeepApartPair &p : readout.keep_apart) {
								const bool pa = std::ranges::count(b.tracks, p.tracks[0]) > 0, pb = std::ranges::count(b.tracks, p.tracks[1]) > 0;
								if (!pa || !pb) continue;
								out << "        " << p.tag << " " << f.Name(p.tracks[0]) << "/" << f.Name(p.tracks[1]) << " pieces " << p.pieces[0] << "/" << p.pieces[1] << ": " << (p.skipped.empty() ? "held" : p.skipped) << "\n";
							}
							for (const PlanarPointRow &r : lead_rows) {
								if (std::ranges::count(b.tracks, r.terms.front().point[0]) > 0) out << "        " << r.tag << ": holds on the witness\n";
							}
						}
						nc.detail = out.str();
						nc.key = Join(rules);
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
				Group *grp = c == STRICT ? &x->strict[pc->key] : c == LOOSE ? &x->loose[pc->key] : nullptr;
				if (grp != nullptr) grp->geometries++;
				if (!x->seen.insert(pkey).second) continue;
				x->counts[c]++;
				if (c == UNDECIDED) x->undecided[pkey] = rep_products[pkey];
				if (grp != nullptr && grp->count++ < 3) grp->examples.push_back({varied.str(), pc->detail});
			}
			size_t a = 0;
			while (a < cfg.axes.size()) {
				if (++pos[a] < std::get<2>(cfg.axes[a]).size()) break;
				pos[a++] = 0;
			}
			if (a == cfg.axes.size()) break;
		}
	}
	const double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

	std::ofstream out(report_path);
	REQUIRE(out.good());
	out << "Encoding report on X1: our keep-apart rows (judged on fixed points, no solve) against the domain\n";
	out << "enumeration wall " << wall << " s\n\n";
	auto write = [&](const std::string &title, const Tally &t) {
		out << "== " << title << " ==\n";
		for (int c = 0; c < 6; c++) out << "  " << CLASS_NAMES[c] << ": " << t.counts[c] << (c == 5 ? "" : " polylines") << " (" << t.geoms[c] << " geometries)\n";
		for (const auto &[pk, prod] : t.undecided) out << "    undecided polyline " << pk << " search nodes " << prod << "\n";
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
	for (const Config &cfg : configs) write(cfg.name, per[cfg.name]);
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
		WARN(cfg.name << " (polylines): both accept " << t.counts[0] << ", both reject " << t.counts[1] << ", too strict " << t.counts[2] << ", too loose " << t.counts[3] << ", undecided " << t.counts[4] << ", off grid geometries " << t.counts[5]);
	}
	WARN("enumeration wall " << wall << " s");
}
