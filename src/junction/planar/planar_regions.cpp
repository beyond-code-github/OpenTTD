/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file planar_regions.cpp Line sides from the planar map's faces, the ring and the legs' order. */

#include "../../stdafx.h"

#include "planar_regions.h"

#include "../../3rdparty/fmt/format.h"

#include <algorithm>
#include <map>
#include <set>

#include "../../safeguards.h"

namespace {

/** An edge of the planar map: a stretch of a line between two events, or one step of the ring boundary. */
struct MapEdge {
	int u; ///< Tail vertex.
	int v; ///< Head vertex.
	int line; ///< The line, or -1 for a boundary edge.
	int stretch; ///< The index of the stretch along the line.
	int ring; ///< For a boundary edge, the ring index it starts at.
};

/** One side of a face's boundary: a line (-1 for the ring) and the side of its travel the face lies on. */
struct FaceEntry {
	int line;
	int8_t side; ///< -1 the face is left of the line's travel, +1 right.
};

/** The clockwise arc from entry to exit of a path, as ring positions. */
using RingEnds = std::pair<int, int>;

/**
 * The faces of the planar map of a topology: vertices are the port ends on the ring, the nodes and the crossings; edges the stretches
 * of every line between events and the ring's steps; each vertex's darts in clockwise order.
 */
class PlanarMap {
public:
	std::vector<std::vector<FaceEntry>> faces; ///< Every face's boundary, the outer face included.
	int outer = -1; ///< The face holding only ring edges.
	std::string why; ///< Empty, or the validation failure.

	PlanarMap(const PlanarTopology &t, const std::vector<int> &ring_pos, const std::vector<RingEnds> &ends);
};

PlanarMap::PlanarMap(const PlanarTopology &t, const std::vector<int> &ring_pos, const std::vector<RingEnds> &ends)
{
	const int nring = static_cast<int>(t.ring.size());
	const int node0 = nring;
	const int cross0 = node0 + static_cast<int>(t.nodes.size());
	const int V = cross0 + static_cast<int>(t.crossings.size());

	std::vector<MapEdge> edges;
	std::vector<std::vector<int>> stretch_edge(t.lines.size()); // per line, the edge of each stretch
	for (uint32_t l = 0; l < t.lines.size(); l++) {
		const PlanarLine &line = t.lines[l];
		if ((line.from.port || line.to.port) && ring_pos[l] < 0) {
			this->why = fmt::format("line {} is a port line but not on the ring", line.name);
			return;
		}
		std::vector<int> vs;
		vs.push_back(line.from.port ? ring_pos[l] : node0 + static_cast<int>(line.from.index));
		for (uint32_t c : t.events[l]) vs.push_back(cross0 + static_cast<int>(c));
		vs.push_back(line.to.port ? ring_pos[l] : node0 + static_cast<int>(line.to.index));
		for (size_t k = 0; k + 1 < vs.size(); k++) {
			stretch_edge[l].push_back(static_cast<int>(edges.size()));
			edges.push_back({vs[k], vs[k + 1], static_cast<int>(l), static_cast<int>(k), -1});
		}
	}
	std::vector<int> boundary(nring);
	for (int i = 0; i < nring; i++) {
		boundary[i] = static_cast<int>(edges.size());
		edges.push_back({i, (i + 1) % nring, -1, -1, i});
	}
	const int E = static_cast<int>(edges.size());
	/* Dart 2e runs along edge e, dart 2e + 1 against it. */
	const auto fwd = [](int e) { return 2 * e; };
	const auto rev = [](int e) { return 2 * e + 1; };

	/* Rotations: the darts out of each vertex, clockwise. */
	std::vector<std::vector<int>> rot(V);
	for (int i = 0; i < nring; i++) {
		const uint32_t l = t.ring[i];
		const int line_dart = t.lines[l].from.port ? fwd(stretch_edge[l].front()) : rev(stretch_edge[l].back());
		rot[i] = {rev(boundary[(i + nring - 1) % nring]), fwd(boundary[i]), line_dart};
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
		const auto at = [&](uint32_t l) { return static_cast<int>(std::ranges::find(t.events[l], static_cast<uint32_t>(c)) - t.events[l].begin()); };
		const int ja = at(a), jb = at(b);
		const int a_in = rev(stretch_edge[a][ja]), a_out = fwd(stretch_edge[a][ja + 1]);
		const int b_in = rev(stretch_edge[b][jb]), b_out = fwd(stretch_edge[b][jb + 1]);
		/* The arc clockwise from a's entry to its exit lies to a's left; b enters from the left when its entry end is on it. */
		const int a0 = ends[a].first, a1 = ends[a].second;
		const auto in_left = [&](int x) { return ((x - a0 + nring) % nring) < ((a1 - a0 + nring) % nring); };
		if (in_left(ends[b].first) == in_left(ends[b].second)) {
			this->why = fmt::format("lines {} and {} cross but their ends do not interleave", t.lines[a].name, t.lines[b].name);
			return;
		}
		const bool b_in_left = in_left(ends[b].first);
		rot[cross0 + c] = {a_in, b_in_left ? b_in : b_out, a_out, b_in_left ? b_out : b_in};
	}

	/* Faces: the walk with the face on the left of each dart. */
	std::vector<int> face_of(2 * E, -1);
	std::vector<std::vector<int>> walks;
	for (int d0 = 0; d0 < 2 * E; d0++) {
		if (face_of[d0] >= 0) continue;
		std::vector<int> walk;
		int d = d0;
		while (face_of[d] < 0) {
			face_of[d] = static_cast<int>(walks.size());
			walk.push_back(d);
			const MapEdge &e = edges[d / 2];
			const std::vector<int> &rt = rot[d % 2 == 0 ? e.v : e.u];
			const size_t idx = std::ranges::find(rt, d ^ 1) - rt.begin();
			if (idx >= rt.size()) {
				this->why = "a dart is missing from its vertex's rotation";
				return;
			}
			d = rt[(idx + 1) % rt.size()];
		}
		if (d != d0) {
			this->why = "a face walk does not close";
			return;
		}
		walks.push_back(std::move(walk));
	}
	const int F = static_cast<int>(walks.size());
	if (V - E + F != 2) {
		this->why = fmt::format("the face map has V={} E={} F={}, V - E + F != 2", V, E, F);
		return;
	}
	int outers = 0;
	for (int f = 0; f < F; f++) {
		std::set<int> tails;
		bool only_boundary = true;
		std::vector<FaceEntry> entries;
		for (int d : walks[f]) {
			const MapEdge &e = edges[d / 2];
			if (!tails.insert(d % 2 == 0 ? e.u : e.v).second) {
				this->why = fmt::format("face {} is not a simple walk", f);
				return;
			}
			if (e.line >= 0) {
				only_boundary = false;
				entries.push_back({e.line, static_cast<int8_t>(d % 2 == 0 ? -1 : 1)});
			}
		}
		if (only_boundary) {
			this->outer = f;
			outers++;
		}
		this->faces.push_back(std::move(entries));
	}
	if (outers != 1) this->why = fmt::format("the face map has {} outer faces", outers);
}

} // namespace

PlanarLineSides::PlanarLineSides(const PlanarTopology &t, const PlanarChannels &) : count(t.lines.size()), table(t.lines.size() * t.lines.size(), 0)
{
	const size_t n = this->count;
	for (uint32_t l = 0; l < n; l++) this->by_name.emplace(t.lines[l].name, l);
	const int nring = static_cast<int>(t.ring.size());
	if (n == 0 || nring == 0) return;
	std::vector<int> ring_pos(n, -1);
	for (int i = 0; i < nring; i++) ring_pos[t.ring[i]] = i;

	/* The ends of each line's first path. */
	std::vector<RingEnds> ends(n, {-1, -1});
	for (const PlanarRoute &r : t.routes) {
		for (const std::vector<uint32_t> &path : r.paths) {
			for (uint32_t l : path) {
				if (ends[l].first < 0) ends[l] = {ring_pos[path.front()], ring_pos[path.back()]};
			}
		}
	}
	for (size_t l = 0; l < n; l++) {
		if (ends[l].first < 0) {
			this->why = fmt::format("no route path runs through line {}", t.lines[l].name);
			return;
		}
	}
	const PlanarMap map(t, ring_pos, ends);
	if (!map.why.empty()) {
		this->why = map.why;
		return;
	}
	std::set<std::pair<uint32_t, uint32_t>> crossing;
	std::set<size_t> conflict;
	for (const PlanarCrossing &c : t.crossings) {
		crossing.insert({c.a, c.b});
		crossing.insert({c.b, c.a});
	}

	/* The faces: where p and q bound a common face, p lies on the side of q that face lies on. Two faces giving a pair opposite sides fail the oracle. */
	for (size_t f = 0; f < map.faces.size(); f++) {
		if (static_cast<int>(f) == map.outer) continue;
		for (const FaceEntry &ep : map.faces[f]) {
			for (const FaceEntry &eq : map.faces[f]) {
				if (ep.line == eq.line) continue;
				const size_t at = static_cast<size_t>(ep.line) * n + eq.line;
				if (this->table[at] != 0 && this->table[at] != eq.side) conflict.insert(at);
				this->table[at] = eq.side;
			}
		}
	}
	for (size_t at : conflict) {
		const uint32_t p = static_cast<uint32_t>(at / n), q = static_cast<uint32_t>(at % n);
		this->table[at] = 0;
		if (crossing.contains({p, q})) continue;
		if (this->why.empty()) this->why = fmt::format("two faces give lines {} and {} opposite sides", t.lines[p].name, t.lines[q].name);
	}
	for (const auto &[p, q] : crossing) this->table[static_cast<size_t>(p) * n + q] = 0;
}

uint32_t PlanarLineSides::LineOfStrand(std::string_view strand) const
{
	const size_t mark = strand.find_last_of("'\"");
	if (mark != std::string_view::npos && strand.find_first_not_of("0123456789", mark + 1) == std::string_view::npos) strand = strand.substr(0, mark);
	const auto it = this->by_name.find(strand);
	return it == this->by_name.end() ? UINT32_MAX : it->second;
}

int8_t PlanarLineSides::Side(uint32_t p, uint32_t q) const
{
	return this->table[p * this->count + q];
}
