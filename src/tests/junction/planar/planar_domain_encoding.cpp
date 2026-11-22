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
	std::array<uint32_t, 7> counts{}; ///< Polylines: both accept, both reject, too strict, too loose, undecided; then off grid geometries, then other shape.
	std::array<uint32_t, 7> geoms{}; ///< The enumerated geometries behind each.
	std::set<std::string> seen; ///< The polylines counted.
	std::map<std::string, double> undecided; ///< Per undecided polyline, its representation count product.
	std::map<std::string, Group> strict;
	std::map<std::string, Group> loose;
	std::vector<Example> other; ///< The first other shape polylines.
};

std::set<std::string> g_undetermined_seen; ///< The undetermined families seen over the run.

std::string FamilyOf(const std::string &tag)
{
	return tag.substr(0, tag.find(' '));
}

const char *const CLASS_NAMES[7] = {"both accept", "both reject", "too strict", "too loose", "undecided", "off grid (geometries)", "other shape"};

/** What the rows say of a polyline: 0 an accepted representation exists, 1 none, 2 undecided; the witness is the accepted representation per track. */
struct Judged {
	int verdict = 1;
	std::vector<Lengths> witness;
	std::vector<std::string> undetermined; ///< The witness's undetermined families of the program check.
};

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
	return SegmentsMeetInOnePoint(a[c.pa], a[c.pa + 1], b[c.pb], b[c.pb + 1]);
}

/**
 * Arc consistency over the binary constraints (D428): a representation of track a survives against track b when some representation of b makes every constraint over exactly {a, b} hold
 * (keep-apart pairs, point rows with those two tracks, program rows by the partial check with only a and b placed). Prunes reps (and lens in step) until no list changes.
 * Returns 0 consistent, 1 a list emptied (rejection), 2 more than cap pair evaluations (undecided); evals counts the pair evaluations.
 */
int ArcConsistency(uint32_t n, std::vector<std::vector<Points>> &reps, std::vector<std::vector<Lengths>> *lens, const std::vector<PlanarKeepApartPair> &keeps, const std::vector<PlanarPointRow> &rows, const PlanarProgram &program, uint64_t cap, uint64_t &evals, const std::vector<Conformity> *conform = nullptr)
{
	std::vector<std::vector<std::vector<const PlanarKeepApartPair *>>> keep_of(n, std::vector<std::vector<const PlanarKeepApartPair *>>(n));
	std::vector<std::vector<std::vector<const PlanarPointRow *>>> row_of(n, std::vector<std::vector<const PlanarPointRow *>>(n));
	for (const PlanarKeepApartPair &p : keeps) {
		if (p.tracks[0] >= n || p.tracks[1] >= n || p.tracks[0] == p.tracks[1]) continue;
		keep_of[p.tracks[0]][p.tracks[1]].push_back(&p);
		keep_of[p.tracks[1]][p.tracks[0]].push_back(&p);
	}
	for (const PlanarPointRow &r : rows) {
		std::set<uint32_t> set;
		for (const PlanarPointRow::Term &t : r.terms) set.insert(t.point[0]);
		if (set.size() != 2) continue;
		const uint32_t a = *set.begin(), b = *set.rbegin();
		row_of[a][b].push_back(&r);
		row_of[b][a].push_back(&r);
	}
	std::vector<std::vector<std::vector<const Conformity *>>> conf_of(n, std::vector<std::vector<const Conformity *>>(n));
	if (conform != nullptr) {
		for (const Conformity &c : *conform) {
			conf_of[c.a][c.b].push_back(&c);
			conf_of[c.b][c.a].push_back(&c);
		}
	}
	std::vector<Points> cur(n);
	/* Whether a at ra and b at rb satisfy every binary constraint over {a, b}. */
	auto holds = [&](uint32_t a, const Points &ra, uint32_t b, const Points &rb) {
		evals++;
		cur[a] = ra;
		cur[b] = rb;
		if (!std::ranges::all_of(conf_of[a][b], [&](const Conformity *c) { return ConformityHolds(*c, cur); })) return false;
		if (!std::ranges::all_of(keep_of[a][b], [&](const PlanarKeepApartPair *p) { return PlanarKeepApartHolds(*p, cur); })) return false;
		if (!std::ranges::all_of(row_of[a][b], [&](const PlanarPointRow *r) { return PlanarPointRowHolds(*r, cur); })) return false;
		std::vector<bool> placed(n, false);
		placed[a] = placed[b] = true;
		return CheckPlanarProgramOnPoints(program, cur, placed).broken.empty();
	};
	std::vector<std::pair<uint32_t, uint32_t>> queue;
	std::set<std::pair<uint32_t, uint32_t>> queued;
	for (uint32_t a = 0; a < n; a++) {
		for (uint32_t b = 0; b < n; b++) {
			if (a != b) {
				queue.emplace_back(a, b);
				queued.insert({a, b});
			}
		}
	}
	while (!queue.empty()) {
		const auto [a, b] = queue.back();
		queue.pop_back();
		queued.erase({a, b});
		bool changed = false;
		for (size_t r = 0; r < reps[a].size();) {
			bool supported = false;
			for (size_t s = 0; s < reps[b].size() && !supported; s++) {
				if (evals > cap) return 2;
				supported = holds(a, reps[a][r], b, reps[b][s]);
			}
			if (supported) {
				r++;
				continue;
			}
			reps[a].erase(reps[a].begin() + static_cast<std::ptrdiff_t>(r));
			if (lens != nullptr) (*lens)[a].erase((*lens)[a].begin() + static_cast<std::ptrdiff_t>(r));
			changed = true;
		}
		if (reps[a].empty()) return 1;
		if (!changed) continue;
		for (uint32_t c = 0; c < n; c++) {
			if (c != a && c != b && queued.insert({c, a}).second) queue.emplace_back(c, a);
		}
		if (queued.insert({b, a}).second) queue.emplace_back(b, a);
	}
	return 0;
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

/** Whether a polyline (the geometry's runs per track) is of another shape than the variant's: arc consistency over the conformance constraints alone empties a list (D428). */
bool IsOtherShape(const X1Fixture &f, const Geometry &g)
{
	std::vector<std::vector<Points>> reps(f.n);
	for (const auto &[name, h] : f.heads) {
		Lengths lens(h.size(), 0);
		const auto it = g.find(name);
		if (it != g.end()) {
			for (const auto &[k, len] : it->second) lens[k] = len;
		}
		const std::vector<Lengths> all = Representations(h, PolylineRuns(h, lens), 20001);
		if (all.size() > 20000) return false;
		const uint32_t t = f.idx.at(name);
		for (const Lengths &l : all) {
			Geometry one;
			for (size_t k = 0; k < l.size(); k++) {
				if (l[k] != 0) one[name][static_cast<uint32_t>(k)] = l[k];
			}
			reps[t].push_back(f.PointsOf(one)[t]);
		}
	}
	const std::vector<Conformity> conform = ConformitiesOf(f);
	uint64_t evals = 0;
	return ArcConsistency(f.n, reps, nullptr, {}, {}, PlanarProgram{}, 5000000, evals, &conform) == 1;
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

TEST_CASE("planar domain: arc consistency prunes and proves", "[junction][planar][planardomain]")
{
	const X1Fixture f;
	const PlanarProgram no_program; /* Only the hand constraints. */
	auto lists = [&]() {
		std::vector<std::vector<Points>> reps(f.n);
		for (const auto &[name, h] : f.heads) {
			Lengths l(h.size(), 0);
			for (const auto &[k, len] : f.base.at(name)) l[k] = len;
			for (const Lengths &r : Representations(h, PolylineRuns(h, l), 20000)) {
				Geometry one;
				for (size_t k = 0; k < r.size(); k++) {
					if (r[k] != 0) one[name][static_cast<uint32_t>(k)] = r[k];
				}
				reps[f.idx.at(name)].push_back(f.PointsOf(one)[f.idx.at(name)]);
			}
		}
		return reps;
	};
	const uint32_t a1 = f.idx.at("A1"), b1 = f.idx.at("B1");
	const std::vector<PlanarKeepApartPair> no_pairs;

	/* A binary constraint no pair satisfies (0 >= 1 over A1 and B1): a list empties, the polyline is rejected. */
	{
		std::vector<std::vector<Points>> reps = lists();
		const PlanarPointRow never{"never", {{{a1, 0}, {0, 0}}, {{b1, 0}, {0, 0}}}, 1};
		uint64_t evals = 0;
		CHECK(ArcConsistency(f.n, reps, nullptr, no_pairs, {never}, no_program, 5000000, evals) == 1);
		CHECK(evals > 0);
	}

	/* A binary constraint only one representation of A1 satisfies, with any B1: A1's list is pruned to it. */
	{
		std::vector<std::vector<Points>> reps = lists();
		const size_t before = reps[a1].size();
		REQUIRE(before > 1);
		PlanarPointRow only;
		bool found = false;
		for (uint32_t k = 0; k < reps[a1][0].size() && !found; k++) {
			for (int axis = 0; axis < 2 && !found; axis++) {
				int best = INT_MIN;
				size_t count = 0;
				for (const Points &p : reps[a1]) {
					if (p[k][axis] > best) {
						best = p[k][axis];
						count = 0;
					}
					if (p[k][axis] == best) count++;
				}
				if (count != 1) continue;
				found = true;
				only = PlanarPointRow{"only", {{{a1, k}, axis == 0 ? std::array<int, 2>{1, 0} : std::array<int, 2>{0, 1}}, {{b1, 0}, {0, 0}}}, best};
			}
		}
		REQUIRE(found);
		uint64_t evals = 0;
		CHECK(ArcConsistency(f.n, reps, nullptr, no_pairs, {only}, no_program, 5000000, evals) == 0);
		CHECK(reps[a1].size() == 1);
		CHECK(!reps[b1].empty());
	}
}

TEST_CASE("planar domain: other shape is set apart", "[junction][planar][planardomain]")
{
	const X1Fixture f;
	CHECK(!IsOtherShape(f, f.base));
	Geometry g = f.base;
	g["B1"] = {{2, 4}, {3, 2}, {4, 2}, {5, 2}, {6, 4}, {8, 4}};
	CHECK(IsOtherShape(f, g));
}

TEST_CASE("planar domain: our keep-apart against the domain on X1", "[.][planardomainenc]")
{
	const char *report_path = std::getenv("PLANAR_DOMAIN_REPORT");
	REQUIRE(report_path != nullptr);
	const X1Fixture f;
	const std::vector<Conformity> conform = ConformitiesOf(f);

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
		const PlanarDrawResult program_out = f.ProgramOut();
		REQUIRE(program_out.status == "built");
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
			std::vector<std::string> witness_undetermined;
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
					/* The program rows that depend on this track alone. */
					if (ok) {
						std::vector<bool> only(f.n, false);
						only[t] = true;
						ok = CheckPlanarProgramOnPoints(program_out.program, pts, only).broken.empty();
					}
					if (ok) {
					reps[t].push_back(pts[t]);
					replens[t].push_back(l);
				}
				}
			}
			bool run_search = false;
			uint64_t evals = 0;
			if (verdict != 2 && !std::ranges::any_of(reps, [](const std::vector<Points> &r) { return r.empty(); })) {
				/* Arc consistency first: an emptied list rejects the polyline, more than 5,000,000 pair evaluations leave it undecided. */
				const int ac = ArcConsistency(f.n, reps, &replens, readout.keep_apart, lead_rows, program_out.program, 5000000, evals, &conform);
				if (ac == 0) run_search = true; else verdict = ac;
			}
			nodes_used = static_cast<double>(evals);
			if (run_search) {
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
				std::vector<bool> placed(f.n, false);
				uint64_t nodes = 0;
				auto search = [&](auto &&self, uint32_t d) -> bool {
					if (d == f.n) return true;
					for (size_t ri = 0; ri < reps[order[d]].size(); ri++) {
						const Points &rp = reps[order[d]][ri];
						if (++nodes > 50000) return false;
						cur[order[d]] = rp;
						chosen[order[d]] = ri;
						placed[order[d]] = true;
						const bool ok = std::ranges::all_of(at_depth[d], [&](const PlanarKeepApartPair *p) { return PlanarKeepApartHolds(*p, cur); }) && std::ranges::all_of(conform, [&](const Conformity &c) { return !placed[c.a] || !placed[c.b] || ConformityHolds(c, cur); }) && CheckPlanarProgramOnPoints(program_out.program, cur, placed).broken.empty();
						if (ok && self(self, d + 1)) return true;
						if (nodes > 50000) return false;
					}
					placed[order[d]] = false;
					return false;
				};
				const bool found = search(search, 0);
				nodes_used = static_cast<double>(nodes + evals);
				verdict = found ? 0 : (nodes > 50000 ? 2 : 1);
				if (found) {
					witness.resize(f.n);
					for (uint32_t t = 0; t < f.n; t++) witness[t] = replens[t][chosen[t]];
					witness_undetermined = CheckPlanarProgramOnPoints(program_out.program, cur).undetermined;
					g_undetermined_seen.insert(witness_undetermined.begin(), witness_undetermined.end());
				}
			}
			rep_products[pkey] = nodes_used;
			Judged &j = verdicts[pkey];
			j.verdict = verdict;
			j.witness = witness;
			j.undetermined = witness_undetermined;
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
			enum { BOTH_ACCEPT, BOTH_REJECT, STRICT, LOOSE, UNDECIDED, OFF_GRID, OTHER_SHAPE } c;
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
					const bool other = IsOtherShape(f, g);
					const Judged &j = other ? verdicts[pkey] : judge(pkey, g);
					PolyClass nc;
					if (other) {
						nc.c = OTHER_SHAPE;
						nc.detail = "      polyline " + pkey + "\n";
					} else if (j.verdict == 2) {
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
						const PlanarProgramCheck pc_check = CheckPlanarProgramOnPoints(program_out.program, points);
						for (const std::string &b : pc_check.broken) {
							kept.insert(FamilyOf(b));
							nc.detail += "      program row broken: " + b + "\n";
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
						if (!j.undetermined.empty()) {
							std::set<std::string> und(j.undetermined.begin(), j.undetermined.end());
							nc.key += " / undetermined " + Join(und);
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
		for (int c = 0; c < 7; c++) out << "  " << CLASS_NAMES[c] << ": " << t.counts[c] << (c == 5 ? "" : " polylines") << " (" << t.geoms[c] << " geometries)\n";
		for (const Example &e : t.other) out << "    other shape, varied: " << e.varied << "\n" << e.detail;
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

	out << "undetermined families seen: " << Join(g_undetermined_seen) << "\n\n";
	out << "== base (strict rules on): failing pairs ==\n";
	{
		const std::vector<Points> base = f.PointsOf(f.base);
		std::set<std::string> kept;
		out << FailingText(f, f.ReadOut(true).keep_apart, base, kept);
	}
	out.close();
	for (const Config &cfg : configs) {
		const Tally &t = per[cfg.name];
		WARN(cfg.name << " (polylines): both accept " << t.counts[0] << ", both reject " << t.counts[1] << ", too strict " << t.counts[2] << ", too loose " << t.counts[3] << ", undecided " << t.counts[4] << ", off grid geometries " << t.counts[5] << ", other shape " << t.counts[6] << " (geometries " << t.geoms[6] << ")");
	}
	WARN("undetermined families seen: " << Join(g_undetermined_seen));
	WARN("enumeration wall " << wall << " s");
}
