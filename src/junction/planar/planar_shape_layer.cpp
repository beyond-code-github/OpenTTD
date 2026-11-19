/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file planar_shape_layer.cpp The Shape layer's promise oracle. */

#include "../../stdafx.h"
#include "planar_shape_layer.h"

#include "../../core/format.hpp"

#include <algorithm>
#include <cstdlib>
#include <vector>

#include "../../safeguards.h"

namespace {

/** @return x reduced into [-4, 3] (eighths). */
int Norm(int x)
{
	x %= 8;
	if (x < -4) x += 8;
	if (x > 3) x -= 8;
	return x;
}

/** @return Whether a and b are the same heading mod 8. */
bool SameHeading(int a, int b)
{
	return (a - b) % 8 == 0;
}

/** @return The position of a crossing in a track's events, or SIZE_MAX. */
size_t EventIndex(const std::vector<uint32_t> &events, uint32_t crossing)
{
	const auto it = std::ranges::find(events, crossing);
	return it == events.end() ? SIZE_MAX : static_cast<size_t>(it - events.begin());
}

/** The oracle's view of one variant: the map, the shape and the stretch arithmetic. */
struct Oracle {
	const PlanarShapeVariant &v;
	const PlanarTrackMap &map;
	std::vector<std::string> out;

	explicit Oracle(const PlanarShapeVariant &variant) : v(variant), map(variant.structure.map) {}

	/** @return The first piece of stretch k of a track. */
	uint32_t First(uint32_t t, size_t k) const { return k == 0 ? 0 : this->v.tracks[t].event_piece[k - 1]; }

	/** @return The last piece of stretch k of a track. */
	uint32_t Last(uint32_t t, size_t k) const
	{
		const PlanarShapeTrack &s = this->v.tracks[t];
		return k == s.event_piece.size() ? static_cast<uint32_t>(s.pieces.size() - 1) : s.event_piece[k];
	}

	/** @return The heading at the k-th event of a track. */
	int EventHeading(uint32_t t, size_t k) const { return this->v.tracks[t].pieces[this->v.tracks[t].event_piece[k]]; }

	/** @return The structure that lists the crossing, or nullptr; `skip` is a structure to pass over. */
	const PlanarStructure *StructureOf(uint32_t c, const PlanarStructure *skip) const
	{
		for (const PlanarStructure &s : this->v.structure.structures) {
			if (&s != skip && std::ranges::find(s.crossings, c) != s.crossings.end()) return &s;
		}
		return nullptr;
	}

	/**
	 * The arrival angle of crosser q at the crossing c of separator p (H4).
	 * @param c The crossing.
	 * @param p The separator track.
	 * @param q The crosser track.
	 * @param a Receives the angle in 0..7.
	 * @return False when the map's sides give no arrival side.
	 */
	bool Angle(uint32_t c, uint32_t p, uint32_t q, int &a) const
	{
		const size_t k = EventIndex(this->map.events[q], c), j = EventIndex(this->map.events[p], c);
		if (k == SIZE_MAX || j == SIZE_MAX) return false;
		int s = -this->map.Side(q, static_cast<uint32_t>(k), p, static_cast<uint32_t>(j));
		if (s == 0) s = -this->map.Side(q, static_cast<uint32_t>(k), p, static_cast<uint32_t>(j + 1));
		if (s == 0) return false;
		a = (((this->EventHeading(q, k) - this->EventHeading(p, j)) * s) % 8 + 8) % 8;
		return true;
	}

	/** H1: pieces, event pieces and monotone stretches. */
	void CheckH1()
	{
		for (uint32_t t = 0; t < this->map.tracks.size(); t++) {
			const PlanarShapeTrack &s = this->v.tracks[t];
			const std::string &name = this->map.tracks[t].name;
			bool usable = !s.pieces.empty();
			if (!usable) this->out.push_back(fmt::format("H1: track {} has no pieces", name));
			for (size_t i = 1; i < s.pieces.size(); i++) {
				if (std::abs(s.pieces[i] - s.pieces[i - 1]) != 1) {
					this->out.push_back(fmt::format("H1: track {} pieces {} and {} do not differ by one eighth", name, i - 1, i));
					break;
				}
			}
			if (s.event_piece.size() != this->map.events[t].size()) {
				this->out.push_back(fmt::format("H1: track {} has {} event pieces for {} events", name, s.event_piece.size(), this->map.events[t].size()));
				usable = false;
			}
			for (size_t k = 0; usable && k < s.event_piece.size(); k++) {
				if (s.event_piece[k] >= s.pieces.size() || (k > 0 && s.event_piece[k] < s.event_piece[k - 1])) {
					this->out.push_back(fmt::format("H1: track {} event piece {} is out of range or before the one before", name, k));
					usable = false;
				}
			}
			if (!usable) continue;
			for (size_t k = 0; k <= s.event_piece.size(); k++) {
				bool up = false, down = false;
				for (uint32_t i = this->First(t, k); i < this->Last(t, k); i++) {
					if (s.pieces[i + 1] > s.pieces[i]) up = true;
					if (s.pieces[i + 1] < s.pieces[i]) down = true;
				}
				if (up && down) this->out.push_back(fmt::format("H1: track {} stretch {} turns both ways", name, k));
			}
		}
	}

	/** H2: port headings. */
	void CheckH2()
	{
		for (uint32_t t = 0; t < this->map.tracks.size(); t++) {
			const PlanarLine &line = this->map.expanded.lines[t];
			const PlanarShapeTrack &s = this->v.tracks[t];
			if (s.pieces.empty()) continue;
			if (line.from.port && !SameHeading(s.pieces.front(), static_cast<int>(this->map.expanded.ports[line.from.index].heading))) {
				this->out.push_back(fmt::format("H2: track {} starts at heading {}, the port's is {}", this->map.tracks[t].name, s.pieces.front(), static_cast<int>(this->map.expanded.ports[line.from.index].heading)));
			}
			if (line.to.port && !SameHeading(s.pieces.back(), static_cast<int>(this->map.expanded.ports[line.to.index].heading))) {
				this->out.push_back(fmt::format("H2: track {} ends at heading {}, the port's is {}", this->map.tracks[t].name, s.pieces.back(), static_cast<int>(this->map.expanded.ports[line.to.index].heading)));
			}
		}
	}

	/** H3: the node rule at every site. */
	void CheckH3()
	{
		for (const PlanarStructureSite &site : this->v.structure.sites) {
			if (site.node >= this->map.expanded.nodes.size()) {
				this->out.push_back(fmt::format("H3: a site names node {}, which the map lacks", site.node));
				continue;
			}
			const PlanarNode &node = this->map.expanded.nodes[site.node];
			const std::vector<uint32_t> &legs = site.split ? node.out : node.in;
			const std::vector<uint32_t> &single = site.split ? node.in : node.out;
			if (single.size() != 1) {
				this->out.push_back(fmt::format("H3: node {} has {} tracks on its single side", site.node, single.size()));
				continue;
			}
			const uint32_t one = single[0];
			const int h = site.split ? this->v.tracks[one].pieces.back() : this->v.tracks[one].pieces.front();
			/* The stretch of a leg at the node: the first for a leg of a split, the last for a leg of a merge. */
			const auto stretch = [&](uint32_t t) { return static_cast<uint32_t>(site.split ? 0 : this->map.events[t].size()); };
			if (site.straight == UINT32_MAX && legs.size() != 2) {
				this->out.push_back(fmt::format("H3: node {} is a fork with {} legs", site.node, legs.size()));
				continue;
			}
			for (const uint32_t leg : legs) {
				const int heading = site.split ? this->v.tracks[leg].pieces.front() : this->v.tracks[leg].pieces.back();
				if (leg == site.straight) {
					if (!SameHeading(heading, h)) this->out.push_back(fmt::format("H3: the straight leg {} at node {} is at heading {}, not {}", this->map.tracks[leg].name, site.node, heading, h));
					continue;
				}
				int sigma;
				if (site.straight == UINT32_MAX) {
					const uint32_t other = legs[0] == leg ? legs[1] : legs[0];
					sigma = this->map.Side(leg, stretch(leg), other, stretch(other));
				} else {
					sigma = this->map.Side(leg, stretch(leg), site.straight, stretch(site.straight));
					if (sigma == 0) sigma = site.side;
				}
				const int turned = site.split ? h + sigma : h - sigma;
				if (!SameHeading(heading, h) && !SameHeading(heading, turned)) {
					this->out.push_back(fmt::format("H3: leg {} at node {} is at heading {}, expected {} or {}", this->map.tracks[leg].name, site.node, heading, h, turned));
				}
			}
		}
	}

	/** H4: the crossing rule. */
	void CheckH4()
	{
		for (uint32_t c = 0; c < this->map.crossings.size(); c++) {
			const PlanarStructure *first = this->StructureOf(c, nullptr);
			if (first == nullptr) {
				this->out.push_back(fmt::format("H4: crossing {} is in no structure, so it has no separator", c));
				continue;
			}
			const PlanarStructure *second = this->StructureOf(c, first);
			const uint32_t p = first->separator;
			const uint32_t q = this->map.crossings[c].a == p ? this->map.crossings[c].b : this->map.crossings[c].a;
			int a;
			if (!this->Angle(c, p, q, a)) {
				this->out.push_back(fmt::format("H4: crossing {} has no side: the map gives no arrival side", c));
				continue;
			}
			if (a < 1 || a > 3) {
				this->out.push_back(fmt::format("H4: crossing {} has angle {} from the headings, not in 1..3", c, a));
			} else if (c >= this->v.angles.size() || this->v.angles[c] != a) {
				this->out.push_back(fmt::format("H4: crossing {} has stored angle {}, the headings give {}", c, c < this->v.angles.size() ? static_cast<int>(this->v.angles[c]) : -1, a));
			}
			if (second != nullptr && a != 2) this->out.push_back(fmt::format("H4: crossing {} of two separators is at angle {}, not 2", c, a));
		}
	}

	/** H5 and H6: each structure's axis piece, end runs and least middles. */
	void CheckH5H6(const JunctionSpec &spec)
	{
		const std::vector<PlanarStructure> &structures = this->v.structure.structures;
		for (size_t i = 0; i < structures.size(); i++) {
			const PlanarStructure &s = structures[i];
			const PlanarShapeTrack &sep = this->v.tracks[s.separator];
			bool same = true;
			uint32_t piece = UINT32_MAX;
			for (const uint32_t c : s.crossings) {
				const size_t j = EventIndex(this->map.events[s.separator], c);
				if (j == SIZE_MAX) continue;
				if (piece == UINT32_MAX) piece = sep.event_piece[j];
				if (sep.event_piece[j] != piece) same = false;
			}
			if (!same) this->out.push_back(fmt::format("H5: structure {} has crossings on different pieces of its separator", i));
			if (same && piece != UINT32_MAX && sep.pieces[piece] % 2 != 0) this->out.push_back(fmt::format("H5: structure {} crosses on a diagonal piece (heading {})", i, sep.pieces[piece]));
			if (i >= this->v.structures.size()) {
				this->out.push_back(fmt::format("H5: structure {} has no shape entry", i));
				continue;
			}
			const PlanarShapeStructure &shape = this->v.structures[i];
			const std::array<int, 2> end_run = {(s.kinds & 1) != 0 ? 1 : -1, (s.kinds & 2) != 0 ? 2 : -1};
			if (shape.end_run != end_run) this->out.push_back(fmt::format("H5: structure {} has end runs {}, {}; its kinds give {}, {}", i, shape.end_run[0], shape.end_run[1], end_run[0], end_run[1]));

			/* H6: the least middles from the headings, run by run. */
			int least = 0, run = 0, run_angle = -1;
			bool known = true;
			const auto flush = [&]() { if (run > 0) least += run_angle == 2 ? run : run + 1; run = 0; };
			for (size_t k = 0; k < s.crossings.size() && known; k++) {
				int a;
				if (!this->Angle(s.crossings[k], s.separator, s.crossers[k], a) || a < 1 || a > 3) {
					known = false;
					break;
				}
				if (a != run_angle) flush();
				run_angle = a;
				run++;
			}
			flush();
			if (!known) continue;
			for (int kind = 0; kind < 2; kind++) {
				const bool allowed = (s.kinds & (1 << kind)) != 0;
				if (!allowed) {
					if (shape.middles[kind] != -1) this->out.push_back(fmt::format("H6: structure {} has middles {} for a kind it does not allow", i, shape.middles[kind]));
					continue;
				}
				const char *name = kind == 0 ? "bridge" : "tunnel";
				if (shape.middles[kind] != least) this->out.push_back(fmt::format("H6: structure {} has {} middles {}, the least at its angles is {}", i, name, shape.middles[kind], least));
				const int limit = kind == 0 ? spec.max_bridge_middle : spec.max_tunnel_length;
				if (least > limit) this->out.push_back(fmt::format("H6: structure {} needs {} {} middles, over the limit {}", i, least, name, limit));
				if (s.copy != UINT32_MAX && least + 2 > 2 * spec.sd + spec.tl) this->out.push_back(fmt::format("H6: structure {} has a copy and needs {} {} middles, over the flow bound 2*sd+tl-2 = {}", i, least, name, 2 * spec.sd + spec.tl - 2));
			}
		}
	}

	/** H7: every face winds once (-8 eighths). */
	void CheckH7Faces()
	{
		static const int BOX_HEADING[4] = {6, 0, 2, 4};
		for (const PlanarFace &face : this->map.faces) {
			/* The walked stretches in order, each with its walked start and end heading and its walked ends. */
			struct Walked {
				int start, end;
				bool start_port, end_port;
				uint32_t start_side, end_side;
			};
			std::vector<Walked> walk;
			bool usable = true;
			for (const PlanarWallRun &run : face.runs) {
				for (const auto &[t, k] : run.stretches) {
					if (t >= this->map.tracks.size() || k > this->v.tracks[t].event_piece.size()) {
						usable = false;
						continue;
					}
					const PlanarShapeTrack &s = this->v.tracks[t];
					const PlanarLine &line = this->map.expanded.lines[t];
					const int first = s.pieces[this->First(t, k)], last = s.pieces[this->Last(t, k)];
					const bool at_start = k == 0, at_end = k == s.event_piece.size();
					Walked w;
					if (run.forward) {
						w = {first, last, at_start && line.from.port, at_end && line.to.port, 0, 0};
						if (w.start_port) w.start_side = static_cast<uint32_t>(this->map.expanded.ports[line.from.index].side);
						if (w.end_port) w.end_side = static_cast<uint32_t>(this->map.expanded.ports[line.to.index].side);
					} else {
						w = {last + 4, first + 4, at_end && line.to.port, at_start && line.from.port, 0, 0};
						if (w.start_port) w.start_side = static_cast<uint32_t>(this->map.expanded.ports[line.to.index].side);
						if (w.end_port) w.end_side = static_cast<uint32_t>(this->map.expanded.ports[line.from.index].side);
					}
					walk.push_back(w);
				}
			}
			if (!usable || walk.empty()) {
				this->out.push_back(fmt::format("H7: face {} has a stretch the shape lacks", face.face));
				continue;
			}
			int total = 0;
			for (size_t i = 0; i < walk.size(); i++) {
				const Walked &w = walk[i], &next = walk[(i + 1) % walk.size()];
				total += w.end - w.start;
				if (w.end_port && next.start_port) {
					const int a = static_cast<int>(w.end_side), b = static_cast<int>(next.start_side);
					total += Norm(BOX_HEADING[a] - w.end);
					total += -2 * (((a - b) % 4 + 4) % 4);
					total += Norm(next.start - BOX_HEADING[b]);
				} else if (w.end_port || next.start_port) {
					this->out.push_back(fmt::format("H7: face {} leaves or enters the box at a port end only on one side of a vertex", face.face));
					total += Norm(next.start - w.end);
				} else {
					total += Norm(next.start - w.end);
				}
			}
			if (total != -8) this->out.push_back(fmt::format("H7: face {} winds {} eighths, not -8", face.face, total));
		}
	}

	/** H7: no U-turn on any route path. */
	void CheckH7Routes()
	{
		for (const PlanarRoute &route : this->map.expanded.routes) {
			for (const std::vector<uint32_t> &path : route.paths) {
				int cumulative = 0;
				bool breached = false;
				for (size_t n = 0; n < path.size() && !breached; n++) {
					const std::vector<int> &pieces = this->v.tracks[path[n]].pieces;
					if (pieces.empty()) break;
					if (n > 0) cumulative += Norm(pieces.front() - this->v.tracks[path[n - 1]].pieces.back());
					if (cumulative < -3 || cumulative > 3) breached = true;
					for (size_t i = 1; i < pieces.size() && !breached; i++) {
						cumulative += pieces[i] - pieces[i - 1];
						if (cumulative < -3 || cumulative > 3) breached = true;
					}
				}
				if (breached) this->out.push_back(fmt::format("H7: route {} turns more than 3 eighths from its entry heading", route.name));
			}
		}
	}

	/** H8: beside tracks follow their parent. */
	void CheckH8()
	{
		for (uint32_t b = 0; b < this->map.tracks.size(); b++) {
			if (this->map.tracks[b].kind == PlanarTrack::Kind::Line) continue;
			const std::string &name = this->map.tracks[b].name;
			const PlanarEnd &from = this->map.expanded.lines[b].from;
			uint32_t parent = UINT32_MAX;
			if (!from.port) {
				for (const PlanarStructureSite &site : this->v.structure.sites) {
					if (site.split && site.node == from.index) parent = site.straight;
				}
			}
			if (parent == UINT32_MAX || parent >= this->map.tracks.size()) {
				this->out.push_back(fmt::format("H8: beside track {} has no parent: its split node has no straight leg", name));
				continue;
			}
			const PlanarShapeTrack &sb = this->v.tracks[b], &sp = this->v.tracks[parent];
			if (sb.pieces != sp.pieces) this->out.push_back(fmt::format("H8: beside track {} has other pieces than its parent {}", name, this->map.tracks[parent].name));
			if (sb.event_piece != sp.event_piece) this->out.push_back(fmt::format("H8: beside track {} has other event pieces than its parent {}", name, this->map.tracks[parent].name));
			const std::vector<uint32_t> &eb = this->map.events[b], &ep = this->map.events[parent];
			if (eb.size() != ep.size()) {
				this->out.push_back(fmt::format("H8: beside track {} has {} crossings, its parent {} has {}", name, eb.size(), this->map.tracks[parent].name, ep.size()));
				continue;
			}
			for (size_t k = 0; k < eb.size(); k++) {
				const PlanarCrossing &cb = this->map.crossings[eb[k]], &cp = this->map.crossings[ep[k]];
				const uint32_t xb = cb.a == b ? cb.b : cb.a, xp = cp.a == parent ? cp.b : cp.a;
				if (xb != xp) {
					this->out.push_back(fmt::format("H8: crossing {} of beside track {} is with track {}, its parent's is with {}", k, name, this->map.tracks[xb].name, this->map.tracks[xp].name));
					continue;
				}
				const size_t ib = EventIndex(this->map.events[xb], eb[k]), ip = EventIndex(this->map.events[xb], ep[k]);
				if (ib == SIZE_MAX || ip == SIZE_MAX || this->v.tracks[xb].event_piece[ib] != this->v.tracks[xb].event_piece[ip]) {
					this->out.push_back(fmt::format("H8: crossing {} of beside track {} sits on another piece of {} than its parent's", k, name, this->map.tracks[xb].name));
				}
			}
		}
	}
};

} // namespace

std::vector<std::string> CheckPlanarShapePromise(const PlanarTopology &topology, const JunctionSpec &spec, const PlanarShapeVariant &v)
{
	Oracle o(v);
	const auto h9 = [&]() {
		for (const std::string &m : CheckPlanarStructurePromise(topology, spec, v.structure)) o.out.push_back("H9: " + m);
	};
	const size_t n = o.map.tracks.size();
	if (v.tracks.size() != n || o.map.events.size() != n || o.map.expanded.lines.size() != n) {
		o.out.push_back(fmt::format("H1: the shape has {} tracks, the map {}", v.tracks.size(), n));
		h9();
		return o.out;
	}
	const size_t before = o.out.size();
	o.CheckH1();
	if (o.out.size() != before) {
		/* The later promises read pieces and event pieces; a broken H1 leaves them nothing safe to read. */
		h9();
		return o.out;
	}
	o.CheckH2();
	o.CheckH3();
	o.CheckH4();
	o.CheckH5H6(spec);
	o.CheckH7Faces();
	o.CheckH7Routes();
	o.CheckH8();
	h9();
	return o.out;
}
