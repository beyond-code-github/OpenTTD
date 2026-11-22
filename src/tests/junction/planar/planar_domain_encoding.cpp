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

struct Example {
	std::string varied;
	std::string detail;
};

struct Group {
	uint32_t count = 0;
	std::vector<Example> examples;
};

struct Tally {
	std::array<uint32_t, 5> counts{}; ///< both accept, both reject, too strict, too loose, off grid.
	std::map<std::string, Group> strict;
	std::map<std::string, Group> loose;
};

const char *const CLASS_NAMES[5] = {"both accept", "both reject", "too strict", "too loose", "off grid"};

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
	REQUIRE(rows.size() == 4);
	std::map<std::string, const PlanarPointRow *> by;
	for (const PlanarPointRow &r : rows) by[r.tag] = &r;
	for (const char *name : {"A1", "C1", "B1", "D1"}) {
		const std::string tag = std::string("lead ") + name + " start";
		REQUIRE(by.count(tag) == 1);
		const PlanarPointRow &r = *by[tag];
		const uint32_t t = f.idx.at(name);
		CHECK(r.p == std::array<uint32_t, 2>{t, 0});
		CHECK(r.q == std::array<uint32_t, 2>{t, 1});
		CHECK(r.least == 2);
		const int h = ((f.heads.at(name)[0] % 8) + 8) % 8;
		CHECK(r.s == std::array<int, 2>{STEPS[h][0], STEPS[h][1]});
	}
	/* The base puts the runs of A1 and B1 on piece 4 (pieces 0 to 3 at 0): their end piece has no lead. */
	const std::vector<Points> base = f.PointsOf(f.base);
	for (const PlanarPointRow &r : rows) {
		INFO(r.tag);
		CHECK(PlanarPointRowHolds(r, base) == (r.tag != "lead A1 start" && r.tag != "lead B1 start"));
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
			enum { BOTH_ACCEPT, BOTH_REJECT, STRICT, LOOSE, OFF_GRID } c;
			bool on_grid = true;
			for (const Points &p : points) on_grid = on_grid && RasterPlanarDomainTrack(p).rule.empty();
			std::string detail;
			std::string key;
			if (!on_grid) {
				c = OFF_GRID;
			} else {
				bool rows_ok = std::ranges::all_of(readout.keep_apart, [&](const PlanarKeepApartPair &p) { return PlanarKeepApartHolds(p, points); });
				std::vector<const PlanarPointRow *> lead_failed;
				for (const PlanarPointRow &r : lead_rows) {
					if (!PlanarPointRowHolds(r, points)) lead_failed.push_back(&r);
				}
				if (!lead_failed.empty()) rows_ok = false;
				std::vector<std::string> notes;
				const PlanarDomainVerdict d = CheckPlanarDomain(PlanarDomainInputOf(f.v, points, notes));
				c = rows_ok ? (d.legal ? BOTH_ACCEPT : LOOSE) : (d.legal ? STRICT : BOTH_REJECT);
				if (c == STRICT) {
					std::set<std::string> kept;
					detail = FailingText(f, readout.keep_apart, points, kept);
					for (const PlanarPointRow *r : lead_failed) {
						kept.insert("lead");
						detail += "      " + r->tag + " fails\n";
					}
					key = Join(kept);
				} else if (c == LOOSE) {
					std::set<std::string> rules;
					std::ostringstream out;
					for (const PlanarDomainBreak &b : d.breaks) {
						rules.insert(b.rule);
						std::string names;
						for (const uint32_t t : b.tracks) names += (names.empty() ? "" : ",") + f.Name(t);
						out << "      break " << b.rule << " tracks " << names << " tile (" << b.tile.x << "," << b.tile.y << ") " << b.why << "\n";
						for (const PlanarKeepApartPair &p : readout.keep_apart) {
							const bool a = std::ranges::count(b.tracks, p.tracks[0]) > 0, bb = std::ranges::count(b.tracks, p.tracks[1]) > 0;
							if (!a || !bb) continue;
							out << "        " << p.tag << " " << f.Name(p.tracks[0]) << "/" << f.Name(p.tracks[1]) << " pieces " << p.pieces[0] << "/" << p.pieces[1] << ": " << (p.skipped.empty() ? "held" : p.skipped) << "\n";
						}
						for (const PlanarPointRow &r : lead_rows) {
							if (std::ranges::count(b.tracks, r.p[0]) > 0) out << "        " << r.tag << ": " << (PlanarPointRowHolds(r, points) ? "holds" : "fails") << "\n";
						}
					}
					detail = out.str();
					key = Join(rules);
				}
			}
			for (Tally *x : {&tally, &total}) {
				x->counts[c]++;
				Group *grp = c == STRICT ? &x->strict[key] : c == LOOSE ? &x->loose[key] : nullptr;
				if (grp != nullptr && grp->count++ < 3) grp->examples.push_back({varied.str(), detail});
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
		for (int c = 0; c < 5; c++) out << "  " << CLASS_NAMES[c] << ": " << t.counts[c] << "\n";
		for (int kind = 0; kind < 2; kind++) {
			const auto &groups = kind == 0 ? t.strict : t.loose;
			out << (kind == 0 ? " too strict, by the kept values of the failing pairs:\n" : " too loose, by domain break rules:\n");
			for (const auto &[key, grp] : groups) {
				out << "  [" << key << "] count " << grp.count << "\n";
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
		WARN(cfg.name << ": both accept " << t.counts[0] << ", both reject " << t.counts[1] << ", too strict " << t.counts[2] << ", too loose " << t.counts[3] << ", off grid " << t.counts[4]);
	}
	WARN("enumeration wall " << wall << " s");
}
