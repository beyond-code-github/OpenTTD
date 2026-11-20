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
#include <array>
#include <map>
#include <set>
#include <tuple>

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
	int stretch; ///< The index of the stretch along the line.
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
	std::vector<bool> has_box; ///< Per face, whether a ring edge bounds it.
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
		bool box = false;
		std::vector<FaceEntry> entries;
		for (int d : walks[f]) {
			const MapEdge &e = edges[d / 2];
			if (!tails.insert(d % 2 == 0 ? e.u : e.v).second) {
				this->why = fmt::format("face {} is not a simple walk", f);
				return;
			}
			if (e.line < 0) box = true;
			if (e.line >= 0) {
				only_boundary = false;
				entries.push_back({e.line, static_cast<int8_t>(d % 2 == 0 ? -1 : 1), e.stretch});
			}
		}
		if (only_boundary) {
			this->outer = f;
			outers++;
		}
		this->faces.push_back(std::move(entries));
		this->has_box.push_back(box);
	}
	if (outers != 1) this->why = fmt::format("the face map has {} outer faces", outers);
}

/**
 * Reorder the line-level events of a topology to the crossing order of a variant's channels.
 * @param t The topology, whose events follow the unflipped channels.
 * @param channels The variant's channels.
 * @param why Set when the reorder fails.
 * @return The topology with the moved crossings' events in each channel's order.
 */
PlanarTopology EventsInChannelOrder(const PlanarTopology &t, const PlanarChannels &channels, std::string &why)
{
	PlanarTopology out = t;
	const PlanarChannels orig = BuildPlanarChannels(t);
	if (orig.channels.size() != channels.channels.size()) {
		why = "the variant's channels are not the topology's channels";
		return out;
	}
	std::vector<uint32_t> channel_crossing(t.crossings.size(), UINT32_MAX); // line crossing -> channel crossing
	for (uint32_t x = 0; x < orig.crossings.size(); x++) {
		for (uint32_t lc : orig.crossings[x].line_crossings) channel_crossing[lc] = x;
	}
	for (size_t c = 0; c < orig.channels.size(); c++) {
		const std::vector<uint32_t> &before = orig.channels[c].crossings, &after = channels.channels[c].crossings;
		if (before == after) continue;
		if (before.size() != after.size()) {
			why = fmt::format("channel {} has a different number of crossings in the variant", c);
			return out;
		}
		std::set<uint32_t> moved;
		for (size_t k = 0; k < before.size(); k++) {
			if (before[k] != after[k]) {
				moved.insert(before[k]);
				moved.insert(after[k]);
			}
		}
		std::map<uint32_t, size_t> position;
		for (size_t k = 0; k < after.size(); k++) position[after[k]] = k;
		for (uint32_t l : orig.channels[c].lines) {
			std::vector<uint32_t> &events = out.events[l];
			std::vector<size_t> slots;
			for (size_t k = 0; k < events.size(); k++) {
				if (channel_crossing[events[k]] != UINT32_MAX && moved.contains(channel_crossing[events[k]])) slots.push_back(k);
			}
			for (size_t k = 1; k < slots.size(); k++) {
				if (slots[k] != slots[k - 1] + 1) {
					why = fmt::format("line {}: the events of the moved crossings are not contiguous", t.lines[l].name);
					return out;
				}
			}
			std::vector<uint32_t> values;
			for (size_t k : slots) values.push_back(events[k]);
			std::ranges::stable_sort(values, [&](uint32_t a, uint32_t b) { return position[channel_crossing[a]] < position[channel_crossing[b]]; });
			for (size_t i = 0; i < slots.size(); i++) events[slots[i]] = values[i];
		}
	}
	return out;
}

/**
 * The faces of a planar map except the outer one, as runs of walls.
 * @param map The map.
 * @return The faces, each with its index in the map.
 */
std::vector<PlanarFace> FacesOf(const PlanarMap &map)
{
	std::vector<PlanarFace> faces;
	for (size_t f = 0; f < map.faces.size(); f++) {
		if (static_cast<int>(f) == map.outer) continue;
		PlanarFace face{static_cast<int>(f), map.has_box[f], {}};
		for (const FaceEntry &e : map.faces[f]) {
			const bool forward = e.side < 0;
			if (face.runs.empty() || face.runs.back().forward != forward) face.runs.push_back({forward, {}});
			face.runs.back().stretches.emplace_back(e.line, e.stretch);
		}
		if (face.runs.size() > 1 && face.runs.front().forward == face.runs.back().forward) {
			std::vector<std::pair<uint32_t, uint32_t>> &last = face.runs.back().stretches;
			last.insert(last.end(), face.runs.front().stretches.begin(), face.runs.front().stretches.end());
			face.runs.erase(face.runs.begin());
		}
		faces.push_back(std::move(face));
	}
	return faces;
}


/** A topology expanded into tracks (DESIGN-tracks §16): its lines split into parts, with parallel tracks beside them. */
class TrackExpander {
public:
	PlanarTopology t; ///< The tracks are its lines.
	std::vector<PlanarTrack> tracks;
	std::vector<uint32_t> part_of; ///< As PlanarTrackMap::part_of.
	std::vector<RingEnds> ends; ///< Per track, the ring ends it takes for the crossing validation.
	std::vector<std::array<int8_t, 2>> from; ///< Per crossing, the side its a and its b arrive from relative to the other's travel: +1 left, -1 right.
	std::string why;

	/** @return The track that is the line or one of its parts and holds the crossing in its events, or UINT32_MAX. */
	uint32_t Holder(uint32_t line, uint32_t crossing) const
	{
		for (uint32_t i = 0; i < this->tracks.size(); i++) {
			if (this->tracks[i].kind == PlanarTrack::Kind::Line && this->part_of[i] == line && std::ranges::find(this->t.events[i], crossing) != this->t.events[i].end()) return i;
		}
		return UINT32_MAX;
	}

	/**
	 * Add a track beside a track over a range of its events (the one expansion op).
	 * @param p The track.
	 * @param first The index of its first event in the range (its event count for a range with none).
	 * @param last The index of its last event in the range, or SIZE_MAX for a range running to the track's end node.
	 * @param side Where the new track lies: +1 right of p's travel.
	 * @param kind The new track's kind.
	 * @param name The new track's name.
	 * @return The new track, or UINT32_MAX with why set.
	 */
	uint32_t Parallel(uint32_t p, size_t first, size_t last, int side, PlanarTrack::Kind kind, const std::string &name);
};

uint32_t TrackExpander::Parallel(uint32_t p, size_t first, size_t last, int side, PlanarTrack::Kind kind, const std::string &name)
{
	const bool to_end = last == SIZE_MAX;
	const std::vector<uint32_t> old_events = this->t.events[p];
	const size_t stop = to_end ? old_events.size() : last + 1;
	const PlanarEnd old_to = this->t.lines[p].to;
	if (first > stop || stop > old_events.size()) {
		this->why = fmt::format("a range of track {} is outside its events", this->tracks[p].name);
		return UINT32_MAX;
	}
	if ((to_end || this->t.lines[p].from.port) && old_to.port) {
		this->why = fmt::format("a range of track {} ends at a port", this->tracks[p].name);
		return UINT32_MAX;
	}
	if (to_end && this->t.nodes[old_to.index].IsSplit()) {
		this->why = fmt::format("track {} ends at a split node", this->tracks[p].name);
		return UINT32_MAX;
	}
	const uint32_t n = static_cast<uint32_t>(this->t.nodes.size()), n2 = n + 1;
	const uint32_t q = static_cast<uint32_t>(this->t.lines.size()), tr = q + 1, r = q + 2;
	const PlanarEnd leg_end = to_end ? old_to : PlanarEnd{false, n2};
	const auto add = [&](PlanarTrack::Kind k, PlanarEnd start, PlanarEnd end) {
		const PlanarTrack source = this->tracks[p];
		this->t.lines.push_back({source.name, this->t.lines[p].routes, start, end});
		this->tracks.push_back({k, source.line, k == PlanarTrack::Kind::Line ? source.name : name});
		this->part_of.push_back(k == PlanarTrack::Kind::Line ? this->part_of[p] : UINT32_MAX);
		this->ends.push_back(this->ends[p]);
		this->t.events.emplace_back();
	};
	add(PlanarTrack::Kind::Line, {false, n}, leg_end);
	add(kind, {false, n}, leg_end);
	if (!to_end) add(PlanarTrack::Kind::Line, {false, n2}, old_to);
	this->t.lines[p].to = {false, n};
	this->t.events[p].assign(old_events.begin(), old_events.begin() + static_cast<std::ptrdiff_t>(first));
	this->t.events[q].assign(old_events.begin() + static_cast<std::ptrdiff_t>(first), old_events.begin() + static_cast<std::ptrdiff_t>(stop));
	if (!to_end) {
		this->t.events[r].assign(old_events.begin() + static_cast<std::ptrdiff_t>(stop), old_events.end());
		for (uint32_t c : this->t.events[r]) {
			PlanarCrossing &x = this->t.crossings[c];
			(x.a == p ? x.a : x.b) = r;
		}
	}

	/* The new track mirrors the range's crossings; the crosser meets first whichever of the two lies on the side it comes from. */
	for (uint32_t c : this->t.events[q]) {
		PlanarCrossing &x = this->t.crossings[c];
		const size_t which = x.a == p ? 0 : 1;
		(which == 0 ? x.a : x.b) = q;
		const uint32_t other = which == 0 ? x.b : x.a;
		const uint32_t nc = static_cast<uint32_t>(this->t.crossings.size());
		this->t.crossings.push_back({tr, other});
		this->from.push_back({this->from[c][which], this->from[c][1 - which]});
		this->t.events[tr].push_back(nc);
		std::vector<uint32_t> &other_events = this->t.events[other];
		const bool meets_first = this->from[c][1 - which] == -side;
		other_events.insert(std::ranges::find(other_events, c) + (meets_first ? 0 : 1), nc);
	}

	const std::vector<uint32_t> legs = side > 0 ? std::vector<uint32_t>{q, tr} : std::vector<uint32_t>{tr, q};
	this->t.nodes.push_back({{p}, legs, UINT32_MAX});
	if (!to_end) this->t.nodes.push_back({legs, {r}, UINT32_MAX});
	if (to_end || !old_to.port) {
		std::vector<uint32_t> &in = this->t.nodes[old_to.index].in;
		const auto at = std::ranges::find(in, p);
		if (to_end) {
			*at = q;
			in.insert(side > 0 ? at + 1 : at, tr);
		} else {
			*at = r;
		}
	} else {
		*std::ranges::find(this->t.ring, p) = r;
	}
	return tr;
}

/** The lanes S (far to near) and joiners J of a prio join site, as the composer's Join orders them. */
void JoinLanes(const PlanarTopology &t, const PlanarChannels &channels, const std::vector<PlanarResolvedSite> &sites, size_t s, std::vector<uint32_t> &lanes, std::vector<uint32_t> &joiners)
{
	const PlanarResolvedSite &rs = sites[s];
	const std::vector<uint32_t> &sl = channels.channels[rs.straight].lines;
	std::vector<std::tuple<size_t, uint32_t, uint32_t>> legs;
	for (uint32_t nidx : channels.sites[s].nodes) {
		const PlanarNode &node = t.nodes[nidx];
		const bool a_straight = channels.channel_of_line[node.in[0]] == rs.straight;
		const uint32_t sline = a_straight ? node.in[0] : node.in[1], jline = a_straight ? node.in[1] : node.in[0];
		const size_t pos = std::ranges::find(sl, sline) - sl.begin();
		legs.emplace_back(rs.side > 0 ? pos : sl.size() - 1 - pos, sline, jline);
	}
	std::ranges::sort(legs);
	for (const auto &[pos, sline, jline] : legs) {
		lanes.push_back(sline);
		joiners.push_back(jline);
	}
}

/** A line's role at a join or fray site: a mover (joiner, a fray's other leg), a keeper (lane, straight leg), the single line in, or none. */
enum class PlanarRole : uint8_t { None, In, Keeper, Mover };

PlanarRole RoleAt(const PlanarTopology &t, const PlanarChannels &channels, const std::vector<PlanarResolvedSite> &sites, size_t s, uint32_t line)
{
	if (sites[s].straight == UINT32_MAX) return PlanarRole::None;
	const bool merge = channels.sites[s].kind == PlanarSiteKind::Join || channels.sites[s].kind == PlanarSiteKind::LaneMerge;
	for (const uint32_t nidx : channels.sites[s].nodes) {
		const PlanarNode &node = t.nodes[nidx];
		const std::vector<uint32_t> &legs = merge ? node.in : node.out;
		const std::vector<uint32_t> &single = merge ? node.out : node.in;
		if (std::ranges::find(single, line) != single.end()) return PlanarRole::In;
		if (std::ranges::find(legs, line) == legs.end()) continue;
		return channels.channel_of_line[line] == sites[s].straight ? PlanarRole::Keeper : PlanarRole::Mover;
	}
	return PlanarRole::None;
}

/** The side a copy of a line of channel c lies on, as the slot layer's copy rule: the far line's on its outer side, every other line's away from the far line. */
int CopySide(const PlanarChannels &channels, const std::vector<PlanarResolvedSite> &sites, uint32_t c, size_t j)
{
	bool far_left = true;
	for (size_t s = 0; s < channels.sites.size(); s++) {
		if (channels.sites[s].kind != PlanarSiteKind::Fray && (sites[s].straight == c || sites[s].other == c)) {
			if (sites[s].straight == c && sites[s].prio) far_left = sites[s].side > 0;
			break;
		}
	}
	const int away = far_left ? +1 : -1;
	const bool is_far = far_left ? j == 0 : j == channels.channels[c].lines.size() - 1;
	return is_far ? -away : away;
}

} // namespace

PlanarLineSides::PlanarLineSides(const PlanarTopology &topology, const PlanarChannels &channels) : count(topology.lines.size()), table(topology.lines.size() * topology.lines.size(), 0)
{
	const PlanarTopology t = EventsInChannelOrder(topology, channels, this->why);
	if (!this->why.empty()) return;
	this->events = t.events;
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
	this->faces = FacesOf(map);
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

	std::set<std::pair<uint32_t, uint32_t>> pairs;
	for (size_t f = 0; f < map.faces.size(); f++) {
		if (static_cast<int>(f) == map.outer) continue;
		for (const FaceEntry &ep : map.faces[f]) {
			for (const FaceEntry &eq : map.faces[f]) {
				if (ep.line != eq.line && this->table[static_cast<size_t>(ep.line) * n + eq.line] != 0) pairs.emplace(ep.line, eq.line);
			}
		}
	}
	this->face_pairs.assign(pairs.begin(), pairs.end());

	/*
	 * Which side each line arrives at each crossing from. Stretch k of a line ends at its event k and stretch k + 1 starts there. The face
	 * left of the other line's stretch ending at the crossing also holds the line's stretch ending there (it arrives from the left) or
	 * the one starting there (it arrives from the right).
	 */
	for (uint32_t c = 0; c < t.crossings.size(); c++) {
		for (int which = 0; which < 2; which++) {
			const uint32_t line = which == 0 ? t.crossings[c].a : t.crossings[c].b, other = which == 0 ? t.crossings[c].b : t.crossings[c].a;
			const auto index = [&](uint32_t l) { return static_cast<uint32_t>(std::ranges::find(this->events[l], c) - this->events[l].begin()); };
			const uint32_t k_o = index(other), k_l = index(line);
			const auto holds = [](const PlanarFace &face, uint32_t l, uint32_t k, bool forward_only) {
				for (const PlanarWallRun &run : face.runs) {
					if (forward_only && !run.forward) continue;
					if (std::ranges::find(run.stretches, std::pair<uint32_t, uint32_t>{l, k}) != run.stretches.end()) return true;
				}
				return false;
			};
			int8_t side = 0;
			for (const PlanarFace &face : this->faces) {
				if (!holds(face, other, k_o, true)) continue;
				const bool in = holds(face, line, k_l, false), out = holds(face, line, k_l + 1, false);
				if (in != out) side = in ? +1 : -1;
				break;
			}
			if (side == 0 && this->why.empty()) this->why = fmt::format("no face decides the side line {} crosses line {} from", t.lines[line].name, t.lines[other].name);
			this->crossing_from[{c, line}] = side;
		}
	}
}

int8_t PlanarLineSides::CrossingFrom(uint32_t crossing, uint32_t line) const
{
	const auto it = this->crossing_from.find({crossing, line});
	return it == this->crossing_from.end() ? 0 : it->second;
}

std::vector<std::pair<uint32_t, uint32_t>> PlanarLineSides::FacePairs() const
{
	return this->face_pairs;
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

int8_t PlanarTrackMap::Side(uint32_t p, uint32_t ps, uint32_t q, uint32_t qs) const
{
	const auto it = this->side.find({p, ps, q, qs});
	return it == this->side.end() ? 0 : it->second;
}

std::vector<uint32_t> PlanarMovers(const PlanarTopology &topology, const PlanarChannels &channels, const std::vector<PlanarResolvedSite> &sites)
{
	std::vector<uint32_t> out;
	for (size_t s = 0; s < channels.sites.size(); s++) {
		const PlanarSiteKind kind = channels.sites[s].kind;
		if ((kind != PlanarSiteKind::Join && kind != PlanarSiteKind::Fray) || sites[s].straight == UINT32_MAX) continue;
		for (const uint32_t nidx : channels.sites[s].nodes) {
			const PlanarNode &node = topology.nodes[nidx];
			for (const uint32_t line : kind == PlanarSiteKind::Join ? node.in : node.out) {
				if (RoleAt(topology, channels, sites, s, line) == PlanarRole::Mover && std::ranges::find(out, line) == out.end()) out.push_back(line);
			}
		}
	}
	return out;
}

std::vector<uint32_t> PlanarCrossingSeparators(const PlanarTopology &topology, const PlanarChannels &channels, const std::vector<PlanarResolvedSite> &sites, const std::vector<uint32_t> &over, const std::set<uint32_t> &separator_movers, std::string &why)
{
	std::vector<uint32_t> out(topology.crossings.size(), UINT32_MAX);
	std::vector<uint32_t> rules(topology.crossings.size(), 0); ///< Per crossing, how many rules name it.
	const auto name = [&](uint32_t k) { return fmt::format("{} x {}", topology.lines[topology.crossings[k].a].name, topology.lines[topology.crossings[k].b].name); };
	for (size_t i = 0; i < channels.crossings.size() && why.empty(); i++) {
		for (const uint32_t k : channels.crossings[i].line_crossings) {
			const PlanarCrossing &c = topology.crossings[k];
			rules[k]++;
			if (channels.channel_of_line[c.a] == over[i]) {
				out[k] = c.a;
			} else if (channels.channel_of_line[c.b] == over[i]) {
				out[k] = c.b;
			} else {
				why = fmt::format("crossing {} lies on channels neither of which is the over channel", name(k));
				break;
			}
		}
	}
	for (uint32_t g = 0; g < channels.group_crossings.size() && why.empty(); g++) {
		for (const uint32_t k : channels.group_crossings[g]) {
			rules[k]++;
			size_t site = SIZE_MAX;
			for (size_t s = 0; s < channels.sites.size() && site == SIZE_MAX; s++) {
				const PlanarSiteKind kind = channels.sites[s].kind;
				if ((kind == PlanarSiteKind::Join || kind == PlanarSiteKind::Fray) && sites[s].straight != UINT32_MAX && topology.nodes[channels.sites[s].nodes[0]].group == g) site = s;
			}
			if (site == SIZE_MAX) {
				why = fmt::format("crossing {} in node group {} has no join or fray site", name(k), g);
				break;
			}
			const uint32_t la = topology.crossings[k].a, lb = topology.crossings[k].b;
			const PlanarRole ra = RoleAt(topology, channels, sites, site, la), rb = RoleAt(topology, channels, sites, site, lb);
			if (!((ra == PlanarRole::Mover && rb == PlanarRole::Keeper) || (ra == PlanarRole::Keeper && rb == PlanarRole::Mover))) {
				why = fmt::format("crossing {} in the group of site {} is not a mover and a keeper", name(k), site);
				break;
			}
			const uint32_t mover = ra == PlanarRole::Mover ? la : lb, keeper = ra == PlanarRole::Mover ? lb : la;
			out[k] = separator_movers.contains(mover) ? mover : keeper;
		}
	}
	for (uint32_t k = 0; k < rules.size() && why.empty(); k++) {
		if (rules[k] != 1) why = fmt::format("crossing {} is in {} separator rules", name(k), rules[k]);
	}
	return out;
}

size_t PlanarTracksBeside(const PlanarTopology &topology, const PlanarChannels &channels, const std::vector<PlanarResolvedSite> &sites, const std::vector<PlanarStructureRun> &runs, const std::vector<std::vector<uint32_t>> &line_events, uint32_t line, size_t event)
{
	const std::vector<uint32_t> &events = line_events[line];
	size_t tracks = 0;
	/* A row of a prio join's lane starts at the lane's first crossing with a joiner and runs to its end (the rule BuildPlanarTrackMap uses). */
	for (size_t s = 0; s < channels.sites.size(); s++) {
		if (channels.sites[s].kind != PlanarSiteKind::Join || !sites[s].prio || sites[s].straight == UINT32_MAX) continue;
		std::vector<uint32_t> lanes, joiners;
		JoinLanes(topology, channels, sites, s, lanes, joiners);
		if (std::ranges::find(lanes, line) == lanes.end()) continue;
		for (size_t i = 0; i < events.size(); i++) {
			const uint32_t other = topology.crossings[events[i]].a == line ? topology.crossings[events[i]].b : topology.crossings[events[i]].a;
			if (std::ranges::find(joiners, other) == joiners.end()) continue;
			if (i <= event) tracks++;
			break;
		}
	}
	/* A copy beside a run of the line that has one, over the run's first to last crossing. */
	for (const PlanarStructureRun &run : runs) {
		if (run.line != line || !run.copy || run.crossings.empty()) continue;
		const size_t first = static_cast<size_t>(std::ranges::find(events, run.crossings.front()) - events.begin());
		const size_t last = static_cast<size_t>(std::ranges::find(events, run.crossings.back()) - events.begin());
		if (first <= event && event <= last) {
			tracks++;
			break;
		}
	}
	return tracks;
}

std::vector<PlanarStructureRun> PlanarGroupStructures(const PlanarTopology &topology, const PlanarChannels &channels, const std::vector<PlanarResolvedSite> &sites, const PlanarLineSides &sides, const std::vector<uint32_t> &separator, const JunctionSpec &spec)
{
	std::vector<PlanarStructureRun> out;
	/* A prio join's row starts at its lane's first crossing with a joiner. The row's split node lies there, and a bridge cannot hold a node, so a run is cut before that crossing. */
	std::set<std::pair<uint32_t, uint32_t>> row_starts; ///< (lane, crossing).
	for (size_t s = 0; s < channels.sites.size(); s++) {
		if (channels.sites[s].kind != PlanarSiteKind::Join || !sites[s].prio || sites[s].straight == UINT32_MAX) continue;
		std::vector<uint32_t> lanes, joiners;
		JoinLanes(topology, channels, sites, s, lanes, joiners);
		for (const uint32_t lane : lanes) {
			for (const uint32_t k : sides.Events()[lane]) {
				const uint32_t other = topology.crossings[k].a == lane ? topology.crossings[k].b : topology.crossings[k].a;
				if (std::ranges::find(joiners, other) != joiners.end()) {
					row_starts.insert({lane, k});
					break;
				}
			}
		}
	}
	/* The maximal runs of each line. */
	std::vector<std::vector<std::vector<uint32_t>>> maximal(topology.lines.size());
	for (uint32_t line = 0; line < topology.lines.size(); line++) {
		std::vector<uint32_t> run;
		const auto close = [&]() {
			if (!run.empty()) maximal[line].push_back(run);
			run.clear();
		};
		for (const uint32_t k : sides.Events()[line]) {
			if (row_starts.contains({line, k})) close();
			if (separator[k] == line) {
				run.push_back(k);
			} else {
				close();
			}
		}
		close();
	}
	/* A bridge spans every track at the crossing (D371(2); copies and rows are tracks, S2c1 ruling): a crossing's width is 1 plus the tracks Structure places beside its crosser there. */
	std::vector<PlanarStructureRun> maximal_runs;
	for (uint32_t line = 0; line < topology.lines.size(); line++) {
		for (const std::vector<uint32_t> &run : maximal[line]) maximal_runs.push_back({line, run, channels.line_share[line] >= 1.0});
	}
	const auto width = [&](uint32_t line, uint32_t k) {
		const uint32_t crosser = topology.crossings[k].a == line ? topology.crossings[k].b : topology.crossings[k].a;
		const size_t at = static_cast<size_t>(std::ranges::find(sides.Events()[crosser], k) - sides.Events()[crosser].begin());
		return 1 + PlanarTracksBeside(topology, channels, sites, maximal_runs, sides.Events(), crosser, at);
	};
	for (uint32_t line = 0; line < topology.lines.size(); line++) {
		const bool copy = channels.line_share[line] >= 1.0;
		for (const std::vector<uint32_t> &run : maximal[line]) {
			/* Greedy from the start: the fewest pieces whose widths fit the bridge limit and, with a copy, the D228a bound. */
			std::vector<uint32_t> piece;
			size_t sum = 0;
			for (const uint32_t k : run) {
				const size_t w = width(line, k);
				if (!piece.empty() && (sum + w > spec.max_bridge_middle || (copy && sum + w + 2 > 2u * spec.sd + spec.tl))) {
					out.push_back({line, piece, copy});
					piece.clear();
					sum = 0;
				}
				piece.push_back(k);
				sum += w;
			}
			if (!piece.empty()) out.push_back({line, piece, copy});
		}
	}
	return out;
}

PlanarTrackMap BuildPlanarTrackMap(const PlanarTopology &topology, const PlanarChannels &channels, const std::vector<PlanarResolvedSite> &sites, const std::vector<PlanarStructureRun> &runs)
{
	PlanarTrackMap result;
	TrackExpander x;
	x.t = EventsInChannelOrder(topology, channels, result.why);
	if (!result.why.empty()) return result;
	const size_t base = x.t.lines.size();

	/* The side each line arrives from at each crossing, and the ring ends of each line's first path. */
	const PlanarLineSides line_sides(topology, channels);
	for (uint32_t c = 0; c < x.t.crossings.size(); c++) {
		const std::array<int8_t, 2> arrive{line_sides.CrossingFrom(c, x.t.crossings[c].a), line_sides.CrossingFrom(c, x.t.crossings[c].b)};
		if (arrive[0] == 0 || arrive[1] == 0) {
			result.why = fmt::format("no face decides the sides at crossing {}: {}", c, line_sides.Why());
			return result;
		}
		x.from.push_back(arrive);
	}
	std::vector<int> ring_pos(base, -1);
	for (size_t i = 0; i < x.t.ring.size(); i++) ring_pos[x.t.ring[i]] = static_cast<int>(i);
	x.ends.assign(base, {-1, -1});
	for (const PlanarRoute &r : x.t.routes) {
		for (const std::vector<uint32_t> &path : r.paths) {
			for (uint32_t l : path) {
				if (x.ends[l].first < 0) x.ends[l] = {ring_pos[path.front()], ring_pos[path.back()]};
			}
		}
	}
	for (uint32_t l = 0; l < base; l++) {
		if (x.ends[l].first < 0) {
			result.why = fmt::format("no route path runs through line {}", topology.lines[l].name);
			return result;
		}
		x.tracks.push_back({PlanarTrack::Kind::Line, l, topology.lines[l].name});
		x.part_of.push_back(l);
	}

	/* A copy of a line over the given crossings, which one part of it must hold. */
	const auto copy_over = [&](uint32_t line, const std::vector<uint32_t> &crossings, int side) {
		if (crossings.empty() || !x.why.empty()) return;
		const uint32_t holder = x.Holder(line, crossings.front());
		size_t lo = SIZE_MAX, hi = 0;
		for (uint32_t c : crossings) {
			if (x.Holder(line, c) != holder || holder == UINT32_MAX) {
				x.why = fmt::format("the crossings a copy of {} runs over lie in different parts", topology.lines[line].name);
				return;
			}
			const size_t at = std::ranges::find(x.t.events[holder], c) - x.t.events[holder].begin();
			lo = std::min(lo, at);
			hi = std::max(hi, at);
		}
		x.Parallel(holder, lo, hi, side, PlanarTrack::Kind::Copy, fmt::format("{}\"{}", topology.lines[line].name, x.t.events[holder][lo]));
	};

	/* The prio joins' lanes and joiners. */
	struct Join {
		std::vector<uint32_t> lanes, joiners;
		int sigma;
	};
	std::vector<Join> joins;
	for (size_t s = 0; s < channels.sites.size(); s++) {
		if (channels.sites[s].kind != PlanarSiteKind::Join || !sites[s].prio || sites[s].straight == UINT32_MAX) continue;
		Join join{{}, {}, sites[s].side};
		JoinLanes(topology, channels, sites, s, join.lanes, join.joiners);
		joins.push_back(std::move(join));
	}

	/* Rows first, then the join copies, then the channel copies. */
	for (const Join &join : joins) {
		for (uint32_t lane : join.lanes) {
			size_t first = x.t.events[lane].size();
			for (size_t i = 0; i < x.t.events[lane].size() && x.why.empty(); i++) {
				const PlanarCrossing &c = x.t.crossings[x.t.events[lane][i]];
				if (std::ranges::find(join.joiners, c.a == lane ? c.b : c.a) != join.joiners.end()) {
					first = i;
					break;
				}
			}
			if (x.why.empty()) x.Parallel(lane, first, SIZE_MAX, -join.sigma, PlanarTrack::Kind::Row, "prio:" + topology.lines[lane].name);
		}
	}
	/* The side a copy of a run's line lies on at a crossing: the channel's copy side at a channel crossing; in a node group the site's own side, or at a fray the line's channel's copy side. */
	const auto crossing_side = [&](uint32_t k, uint32_t line, int &side) {
		const uint32_t ch = channels.channel_of_line[line];
		const size_t j = static_cast<size_t>(std::ranges::find(channels.channels[ch].lines, line) - channels.channels[ch].lines.begin());
		for (const PlanarChannelCrossing &cc : channels.crossings) {
			if (std::ranges::find(cc.line_crossings, k) != cc.line_crossings.end()) {
				side = CopySide(channels, sites, ch, j);
				return true;
			}
		}
		for (uint32_t g = 0; g < channels.group_crossings.size(); g++) {
			if (std::ranges::find(channels.group_crossings[g], k) == channels.group_crossings[g].end()) continue;
			for (size_t s = 0; s < channels.sites.size(); s++) {
				const PlanarSiteKind kind = channels.sites[s].kind;
				if ((kind != PlanarSiteKind::Join && kind != PlanarSiteKind::Fray) || sites[s].straight == UINT32_MAX || topology.nodes[channels.sites[s].nodes[0]].group != g) continue;
				side = kind == PlanarSiteKind::Fray ? CopySide(channels, sites, ch, j) : sites[s].side;
				return true;
			}
		}
		return false;
	};
	/* One copy per run with a copy, over the run's crossings. */
	for (const PlanarStructureRun &run : runs) {
		if (!run.copy || !x.why.empty()) continue;
		int side = 0;
		bool have = false;
		for (const uint32_t k : run.crossings) {
			int here = 0;
			if (!crossing_side(k, run.line, here)) {
				x.why = fmt::format("no copy side for crossing {} on {}", k, topology.lines[run.line].name);
				break;
			}
			if (have && here != side) {
				x.why = fmt::format("copy sides disagree on {}", topology.lines[run.line].name);
				break;
			}
			side = here;
			have = true;
		}
		if (x.why.empty()) copy_over(run.line, run.crossings, side);
	}
	if (!x.why.empty()) {
		result.why = x.why;
		return result;
	}

	/* Order the tracks: lines, then the parts, then copies and rows; then build the map. */
	std::vector<uint32_t> order(base);
	for (uint32_t i = 0; i < base; i++) order[i] = i;
	for (const bool lines : {true, false}) {
		for (uint32_t i = static_cast<uint32_t>(base); i < x.tracks.size(); i++) {
			if ((x.tracks[i].kind == PlanarTrack::Kind::Line) == lines) order.push_back(i);
		}
	}
	std::vector<uint32_t> index(order.size());
	for (uint32_t i = 0; i < order.size(); i++) index[order[i]] = i;
	PlanarTopology t = x.t;
	std::vector<RingEnds> ends(order.size());
	for (uint32_t i = 0; i < order.size(); i++) {
		t.lines[i] = x.t.lines[order[i]];
		t.events[i] = x.t.events[order[i]];
		ends[i] = x.ends[order[i]];
		result.tracks.push_back(x.tracks[order[i]]);
		result.part_of.push_back(x.part_of[order[i]]);
	}
	for (PlanarNode &node : t.nodes) {
		for (uint32_t &l : node.in) l = index[l];
		for (uint32_t &l : node.out) l = index[l];
	}
	for (PlanarCrossing &c : t.crossings) {
		c.a = index[c.a];
		c.b = index[c.b];
	}
	for (uint32_t &l : t.ring) l = index[l];
	ring_pos.assign(order.size(), -1);
	for (size_t i = 0; i < t.ring.size(); i++) ring_pos[t.ring[i]] = static_cast<int>(i);
	result.events = t.events;
	result.crossings = t.crossings;
	result.expanded = t;
	for (uint32_t r = 0; r < result.expanded.routes.size(); r++) {
		result.expanded.routes[r].paths.clear();
		std::string path_error;
		if (!PlanarRoutePaths(result.expanded, r, result.expanded.routes[r].paths, path_error)) {
			result.why = path_error;
			return result;
		}
	}

	const PlanarMap map(t, ring_pos, ends);
	if (!map.why.empty()) {
		result.why = map.why;
		return result;
	}
	result.faces = FacesOf(map);

	/* Sides per stretch: where two stretches bound a common face, the one lies on the side of the other that face lies on. */
	std::set<std::pair<uint32_t, uint32_t>> crossing, line_crossing;
	for (const PlanarCrossing &c : t.crossings) {
		crossing.insert({c.a, c.b});
		crossing.insert({c.b, c.a});
	}
	for (const PlanarCrossing &c : topology.crossings) {
		line_crossing.insert({c.a, c.b});
		line_crossing.insert({c.b, c.a});
	}
	std::set<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>> opposite;
	for (size_t f = 0; f < map.faces.size(); f++) {
		if (static_cast<int>(f) == map.outer) continue;
		for (const FaceEntry &ep : map.faces[f]) {
			for (const FaceEntry &eq : map.faces[f]) {
				if (ep.line == eq.line) continue;
				const std::tuple<uint32_t, uint32_t, uint32_t, uint32_t> key{ep.line, ep.stretch, eq.line, eq.stretch};
				const auto [it, added] = result.side.emplace(key, eq.side);
				if (!added && it->second != eq.side) opposite.insert(key);
			}
		}
	}
	for (const auto &key : opposite) {
		result.side.erase(key);
		const uint32_t p = std::get<0>(key), q = std::get<2>(key);
		if (crossing.contains({p, q}) || line_crossing.contains({result.tracks[p].line, result.tracks[q].line})) continue;
		result.conflicts.push_back(key);
	}
	return result;
}
