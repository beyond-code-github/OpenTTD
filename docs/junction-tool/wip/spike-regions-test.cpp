/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file planar_regions.cpp SPIKE: the planar map of a topology (faces) against the lazy clash sides. */

#include "../../../stdafx.h"

#include "../../../3rdparty/catch2/catch.hpp"

#include "../../../core/format.hpp"
#include "../../../junction/junction_spec.h"
#include "../../../junction/planar/planar_components.h"
#include "../../../junction/planar/planar_plan.h"
#include "../../../junction/planar/planar_topology.h"

#include <algorithm>
#include <cstdio>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include "../../../safeguards.h"

/* Mirrors the spike struct in planar_components.cpp. */
struct SpikeSep { std::string p; uint32_t pi; int ph; std::string q; uint32_t qi; int qh; int fx; int fy; };
extern std::map<std::string, std::vector<SpikeSep>> g_spike;
extern std::mutex g_spike_mx;

namespace {

struct Edge { int u, v; int line; int stretch; int ring; }; // line -1: boundary edge from ring end `ring` to the next
struct Entry { int line; int stretch; char side; };         // side 'L' or 'R' of the line's travel; line -1 boundary

constexpr int STEPX[8] = {0, 1, 1, 1, 0, -1, -1, -1};
constexpr int STEPY[8] = {-1, -1, 0, 1, 1, 1, 0, -1};

std::string BaseName(const std::string &s)
{
	const size_t k = s.find_first_of("'\"");
	return k == std::string::npos ? s : s.substr(0, k);
}

void Say(const std::string &s) { fmt::print(stderr, "{}\n", s); }

} // namespace

TEST_CASE("planar: regions of a topology against the lazy clash sides", "[.regions]")
{
	const char *hub_env = std::getenv("REGIONS_HUB");
	REQUIRE(hub_env != nullptr);
	const std::string hub = hub_env;
	std::string error;
	const PlanarTopology t = ParsePlanarTopology(hub == "LL2" ? PlanarLL2Text() : PlanarTText(2), &error);
	REQUIRE(error.empty());
	const int nring = static_cast<int>(t.ring.size());
	std::vector<int> ring_pos(t.lines.size(), -1);
	for (int i = 0; i < nring; i++) ring_pos[t.ring[i]] = i;

	/* Vertices: ring ends 0..nring-1, then nodes, then crossings. */
	const int node0 = nring, cross0 = node0 + static_cast<int>(t.nodes.size());
	const int V = cross0 + static_cast<int>(t.crossings.size());
	std::vector<std::string> vname(V);
	for (int i = 0; i < nring; i++) vname[i] = "P:" + t.lines[t.ring[i]].name;
	for (size_t n = 0; n < t.nodes.size(); n++) vname[node0 + n] = fmt::format("N{}", n);
	for (size_t c = 0; c < t.crossings.size(); c++) vname[cross0 + c] = fmt::format("X:{}x{}", t.lines[t.crossings[c].a].name, t.lines[t.crossings[c].b].name);

	/* Path ends of each line (first path through it, as planar_topology.cpp derives the event order). */
	std::vector<std::pair<int, int>> ends(t.lines.size(), {-1, -1});
	for (const PlanarRoute &r : t.routes) {
		for (const auto &path : r.paths) {
			for (uint32_t l : path) {
				if (ends[l].first < 0) ends[l] = {ring_pos[path.front()], ring_pos[path.back()]};
			}
		}
	}

	std::vector<Edge> edges;
	std::vector<std::vector<int>> stretch_edge(t.lines.size()); // per line, the edge ids of its stretches
	for (uint32_t l = 0; l < t.lines.size(); l++) {
		const PlanarLine &line = t.lines[l];
		std::vector<int> vs;
		vs.push_back(line.from.port ? ring_pos[l] : node0 + static_cast<int>(line.from.index));
		for (uint32_t c : t.events[l]) vs.push_back(cross0 + static_cast<int>(c));
		vs.push_back(line.to.port ? ring_pos[l] : node0 + static_cast<int>(line.to.index));
		for (size_t k = 0; k + 1 < vs.size(); k++) {
			stretch_edge[l].push_back(static_cast<int>(edges.size()));
			edges.push_back({vs[k], vs[k + 1], static_cast<int>(l), static_cast<int>(k), -1});
		}
	}
	std::vector<int> bedge(nring);
	for (int i = 0; i < nring; i++) {
		bedge[i] = static_cast<int>(edges.size());
		edges.push_back({i, (i + 1) % nring, -1, -1, i});
	}
	const int E = static_cast<int>(edges.size());
	const auto fwd = [](int e) { return 2 * e; };
	const auto rev = [](int e) { return 2 * e + 1; };

	/* Rotations (darts out of each vertex, clockwise). */
	std::vector<std::vector<int>> rot(V);
	for (int i = 0; i < nring; i++) {
		const uint32_t l = t.ring[i];
		const int line_dart = t.lines[l].from.port ? fwd(stretch_edge[l].front()) : rev(stretch_edge[l].back());
		rot[i] = {rev(bedge[(i + nring - 1) % nring]), fwd(bedge[i]), line_dart}; // prev, next, line
	}
	for (size_t n = 0; n < t.nodes.size(); n++) {
		const PlanarNode &node = t.nodes[n];
		std::vector<int> r;
		if (node.IsSplit()) {
			r.push_back(rev(stretch_edge[node.in[0]].back()));
			for (uint32_t o : node.out) r.push_back(fwd(stretch_edge[o].front()));
		} else {
			r.push_back(fwd(stretch_edge[node.out[0]].front()));
			for (auto it = node.in.rbegin(); it != node.in.rend(); ++it) r.push_back(rev(stretch_edge[*it].back()));
		}
		rot[node0 + n] = r;
	}
	for (size_t c = 0; c < t.crossings.size(); c++) {
		const uint32_t a = t.crossings[c].a, b = t.crossings[c].b;
		const auto pos_in = [&](uint32_t l) { return std::ranges::find(t.events[l], static_cast<uint32_t>(c)) - t.events[l].begin(); };
		const int ja = static_cast<int>(pos_in(a)), jb = static_cast<int>(pos_in(b));
		const int a_in = rev(stretch_edge[a][ja]), a_out = fwd(stretch_edge[a][ja + 1]);
		const int b_in = rev(stretch_edge[b][jb]), b_out = fwd(stretch_edge[b][jb + 1]);
		/* The arc clockwise (increasing ring index) from a's entry to its exit lies to a's left. */
		const int a0 = ends[a].first, a1 = ends[a].second, b0 = ends[b].first, b1 = ends[b].second;
		const auto in_left = [&](int x) { return ((x - a0 + nring) % nring) < ((a1 - a0 + nring) % nring); };
		REQUIRE(in_left(b0) != in_left(b1));
		const bool b_in_left = in_left(b0);
		rot[cross0 + c] = {a_in, b_in_left ? b_in : b_out, a_out, b_in_left ? b_out : b_in};
	}

	/* Face walk: face on the left of each dart. */
	std::vector<int> face_of(2 * E, -1);
	std::vector<std::vector<int>> faces;
	for (int d0 = 0; d0 < 2 * E; d0++) {
		if (face_of[d0] >= 0) continue;
		std::vector<int> walk;
		int d = d0;
		while (face_of[d] < 0) {
			face_of[d] = static_cast<int>(faces.size());
			walk.push_back(d);
			const int e = d / 2;
			const int head = (d % 2 == 0) ? edges[e].v : edges[e].u;
			const int rv = d ^ 1;
			const std::vector<int> &rt = rot[head];
			const size_t idx = std::ranges::find(rt, rv) - rt.begin();
			REQUIRE(idx < rt.size());
			d = rt[(idx + 1) % rt.size()];
		}
		REQUIRE(d == d0);
		faces.push_back(walk);
	}
	const int F = static_cast<int>(faces.size());
	std::string report = fmt::format("HUB {} V={} E={} F={} V-E+F={}", hub, V, E, F, V - E + F);
	Say(report);
	/* Validate. */
	CHECK(V - E + F == 2);
	int outer = -1, outer_count = 0;
	std::vector<Entry> dummy;
	for (int f = 0; f < F; f++) {
		std::set<int> seen;
		bool simple = true;
		bool all_boundary = true;
		for (int d : faces[f]) {
			const int e = d / 2;
			const int tail = (d % 2 == 0) ? edges[e].u : edges[e].v;
			if (!seen.insert(tail).second) simple = false;
			if (edges[e].line >= 0) all_boundary = false;
		}
		if (!simple) Say(fmt::format("face {} is not a simple walk", f));
		if (all_boundary) { outer = f; outer_count++; }
	}
	Say(fmt::format("outer faces (only boundary edges): {}; face of B0 forward = {} ({} darts)", outer_count, face_of[fwd(bedge[0])], faces[face_of[fwd(bedge[0])]].size()));
	CHECK(outer_count == 1);
	CHECK(outer == face_of[fwd(bedge[0])]);

	/* Per face boundary entries. */
	std::vector<std::vector<Entry>> fentries(F);
	for (int f = 0; f < F; f++) {
		std::string s = fmt::format("face {}{}:", f, f == outer ? " (OUTER)" : "");
		for (int d : faces[f]) {
			const Edge &ed = edges[d / 2];
			if (ed.line < 0) {
				s += fmt::format(" ring{}", ed.ring);
				fentries[f].push_back({-1, ed.ring, 'L'});
			} else {
				const char side = d % 2 == 0 ? 'L' : 'R';
				s += fmt::format(" {}#{}{}", t.lines[ed.line].name, ed.stretch, side);
				fentries[f].push_back({ed.line, ed.stretch, side});
			}
		}
		Say(s);
	}

	/* Lazy separations. */
	PlanarComponentLayout layout;
	std::string why;
	const JunctionSpec spec = MakeTestJunctionSpec();
	REQUIRE(SolvePlanarMapped(t, spec, layout, &why));
	Say(fmt::format("variant {} area {}", layout.variant, layout.area));
	std::vector<SpikeSep> seps;
	{
		std::lock_guard<std::mutex> lk(g_spike_mx);
		seps = g_spike[layout.variant];
	}
	std::map<std::string, uint32_t> line_by_name;
	for (uint32_t l = 0; l < t.lines.size(); l++) line_by_name[t.lines[l].name] = l;
	int agree = 0, contradict = 0, notadj = 0, selfcopy = 0, ambiguous = 0, parallel = 0, unknown = 0;
	std::vector<std::string> notadj_list, contra_list, amb_list;
	for (const SpikeSep &s : seps) {
		const std::string bp = BaseName(s.p), bq = BaseName(s.q);
		const std::string tag = fmt::format("{}[{}]h{} | {}[{}]h{} f=({},{})", s.p, s.pi, s.ph, s.q, s.qi, s.qh, s.fx, s.fy);
		if (bp == bq) { selfcopy++; Say("SEP self/copy " + tag); continue; }
		if (!line_by_name.contains(bp) || !line_by_name.contains(bq)) { unknown++; Say("SEP unknown line " + tag); continue; }
		const int lp = static_cast<int>(line_by_name[bp]), lq = static_cast<int>(line_by_name[bq]);
		/* Common faces: the sides of q implied. */
		std::set<char> implied;
		std::vector<int> common;
		for (int f = 0; f < F; f++) {
			bool hp = false;
			char sq = 0;
			for (const Entry &en : fentries[f]) {
				if (en.line == lp) hp = true;
				if (en.line == lq) sq = en.side;
			}
			if (hp && sq != 0 && f != outer) { common.push_back(f); implied.insert(sq); }
		}
		/* Chosen: p is at -f from q; right of q iff (-f).Right(hq) > 0, Right(dx,dy) = (-dy,dx). */
		const int rx = -STEPY[s.qh], ry = STEPX[s.qh];
		const int dotv = -(s.fx * rx + s.fy * ry);
		const char chosen = dotv > 0 ? 'R' : dotv < 0 ? 'L' : '0';
		if (common.empty()) {
			notadj++;
			notadj_list.push_back(fmt::format("{} (chose p {})", tag, chosen));
			Say(fmt::format("SEP not-adjacent {} p-side-of-q={}", tag, chosen));
			continue;
		}
		std::string faces_s;
		for (int f : common) faces_s += fmt::format(" {}", f);
		if (chosen == '0') { parallel++; Say(fmt::format("SEP parallel-f {} faces{}", tag, faces_s)); continue; }
		if (implied.size() > 1) {
			ambiguous++;
			amb_list.push_back(fmt::format("{} chose p {} faces{}", tag, chosen, faces_s));
			Say(fmt::format("SEP ambiguous(both sides across faces) {} chose p {} faces{}", tag, chosen, faces_s));
			continue;
		}
		const char imp = *implied.begin(); // face on q's left means p is on q's left
		if (imp == chosen) {
			agree++;
			Say(fmt::format("SEP agree {} p {} of q faces{}", tag, chosen, faces_s));
		} else {
			contradict++;
			contra_list.push_back(fmt::format("{} face implies p {}, chose {}, faces{}", tag, imp, chosen, faces_s));
			Say(fmt::format("SEP CONTRADICT {} face implies p {} of q, solver chose {} faces{}", tag, imp, chosen, faces_s));
		}
	}
	Say(fmt::format("SUMMARY {} faces={} V={} E={} total_seps={} agree={} contradict={} ambiguous={} not_adjacent={} self_copy={} parallel_f={} unknown={}", hub, F, V, E, seps.size(), agree, contradict, ambiguous, notadj, selfcopy, parallel, unknown));
}
