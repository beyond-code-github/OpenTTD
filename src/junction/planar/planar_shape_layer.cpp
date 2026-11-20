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
#include "../placement/junction_milp.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <tuple>
#include <utility>
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

/** The heading each box side is walked along when a face follows the box edge, per side (as the side index counts). */
constexpr int BOX_HEADING[4] = {6, 0, 2, 4};

/** One stretch of a face walk, with the ends it is walked from and to. */
struct WalkedStretch {
	uint32_t track; ///< The track.
	uint32_t stretch; ///< The stretch of the track.
	bool forward; ///< Walked along the track's travel, else against it.
	bool start_port, end_port; ///< Whether the walked start (end) is a port end.
	uint32_t start_side, end_side; ///< The box side of that port end.
};

/** One term of a face's winding sum, between the stretches of a walk. */
struct FaceTerm {
	enum class Kind : uint8_t { Stretch, Vertex, Box };
	Kind kind;
	size_t from; ///< The stretch (Stretch), or the stretch walked before the term (Vertex, Box).
	size_t to; ///< The stretch walked after the term (Vertex, Box).
	uint32_t side_a, side_b; ///< Box: the box sides the face leaves and re-enters by.
	bool lopsided; ///< Vertex: exactly one of the two stretch ends is a port end.
};

/** A face as its stretches in walk order and the terms of its winding sum. */
struct FaceWalk {
	std::vector<WalkedStretch> stretches; ///< In walk order.
	std::vector<FaceTerm> terms; ///< Per stretch: the stretch term, then the vertex or box term to the next stretch.
	bool usable = true; ///< False when a stretch names a track or stretch the map lacks.
};

/**
 * List a face's winding terms (H7).
 * @param map The track map.
 * @param face The face.
 * @return The walk.
 */
FaceWalk WalkFace(const PlanarTrackMap &map, const PlanarFace &face)
{
	FaceWalk walk;
	for (const PlanarWallRun &run : face.runs) {
		for (const auto &[t, k] : run.stretches) {
			if (t >= map.tracks.size() || k > map.events[t].size()) {
				walk.usable = false;
				continue;
			}
			const PlanarLine &line = map.expanded.lines[t];
			const bool at_start = k == 0, at_end = k == map.events[t].size();
			WalkedStretch w;
			if (run.forward) {
				w = {t, k, true, at_start && line.from.port, at_end && line.to.port, 0, 0};
				if (w.start_port) w.start_side = static_cast<uint32_t>(map.expanded.ports[line.from.index].side);
				if (w.end_port) w.end_side = static_cast<uint32_t>(map.expanded.ports[line.to.index].side);
			} else {
				w = {t, k, false, at_end && line.to.port, at_start && line.from.port, 0, 0};
				if (w.start_port) w.start_side = static_cast<uint32_t>(map.expanded.ports[line.to.index].side);
				if (w.end_port) w.end_side = static_cast<uint32_t>(map.expanded.ports[line.from.index].side);
			}
			walk.stretches.push_back(w);
		}
	}
	for (size_t i = 0; i < walk.stretches.size(); i++) {
		const WalkedStretch &w = walk.stretches[i];
		const size_t next = (i + 1) % walk.stretches.size();
		const WalkedStretch &n = walk.stretches[next];
		walk.terms.push_back({FaceTerm::Kind::Stretch, i, i, 0, 0, false});
		if (w.end_port && n.start_port) {
			walk.terms.push_back({FaceTerm::Kind::Box, i, next, w.end_side, n.start_side, false});
		} else {
			walk.terms.push_back({FaceTerm::Kind::Vertex, i, next, 0, 0, w.end_port || n.start_port});
		}
	}
	return walk;
}

/**
 * The parent chain of a beside track (H8): from the straight leg of its start node's site, at each end node the out track that is a part of its line, to the track ending where it ends.
 * @param map The track map.
 * @param sites The variant's sites.
 * @param b The Copy or Row track.
 * @param chain Receives the tracks in order.
 * @return False when there is no (unique) chain.
 */
bool ParentChain(const PlanarTrackMap &map, const std::vector<PlanarStructureSite> &sites, uint32_t b, std::vector<uint32_t> &chain)
{
	chain.clear();
	const PlanarEnd &from = map.expanded.lines[b].from, &end = map.expanded.lines[b].to;
	if (from.port) return false;
	uint32_t current = UINT32_MAX;
	for (const PlanarStructureSite &site : sites) {
		if (site.split && site.node == from.index) current = site.straight;
	}
	while (current < map.tracks.size()) {
		chain.push_back(current);
		const PlanarEnd &to = map.expanded.lines[current].to;
		if (to.port == end.port && to.index == end.index) return true;
		if (to.port || chain.size() > map.tracks.size()) return false;
		uint32_t next = UINT32_MAX;
		for (const uint32_t g : map.expanded.nodes[to.index].out) {
			if (g >= map.part_of.size() || map.part_of[g] != map.tracks[b].line) continue;
			if (next != UINT32_MAX) return false;
			next = g;
		}
		current = next;
	}
	return false;
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
	 * The arrival side of crosser q at the crossing c of separator p (H4).
	 * @param c The crossing.
	 * @param p The separator track.
	 * @param q The crosser track.
	 * @param s Receives the side, +1 or -1.
	 * @return False when the map's sides give no arrival side.
	 */
	bool Arrival(uint32_t c, uint32_t p, uint32_t q, int &s) const
	{
		const size_t k = EventIndex(this->map.events[q], c), j = EventIndex(this->map.events[p], c);
		if (k == SIZE_MAX || j == SIZE_MAX) return false;
		s = -this->map.Side(q, static_cast<uint32_t>(k), p, static_cast<uint32_t>(j));
		if (s == 0) s = -this->map.Side(q, static_cast<uint32_t>(k), p, static_cast<uint32_t>(j + 1));
		return s != 0;
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
		int s;
		if (!this->Arrival(c, p, q, s)) return false;
		const size_t k = EventIndex(this->map.events[q], c), j = EventIndex(this->map.events[p], c);
		a = (((this->EventHeading(q, k) - this->EventHeading(p, j)) * s) % 8 + 8) % 8;
		return true;
	}

	/**
	 * The least middles of a structure at the angles of its crossings, run by run (H6).
	 * @param s The structure.
	 * @param least Receives the least middles.
	 * @return False when a crossing has no angle in 1..3.
	 */
	bool Least(const PlanarStructure &s, int &least) const
	{
		int run = 0, run_angle = -1;
		least = 0;
		const auto flush = [&]() { if (run > 0) least += run_angle == 2 ? run : run + 1; run = 0; };
		for (size_t k = 0; k < s.crossings.size(); k++) {
			int a;
			if (!this->Angle(s.crossings[k], s.separator, s.crossers[k], a) || a < 1 || a > 3) return false;
			if (a != run_angle) flush();
			run_angle = a;
			run++;
		}
		flush();
		return true;
	}

	/** @return The crossings of a beside track's parent chain, concatenated in order; false when it has no chain. */
	bool ChainEvents(uint32_t b, std::vector<uint32_t> &events) const
	{
		std::vector<uint32_t> chain;
		events.clear();
		if (!ParentChain(this->map, this->v.structure.sites, b, chain)) return false;
		for (const uint32_t t : chain) events.insert(events.end(), this->map.events[t].begin(), this->map.events[t].end());
		return true;
	}

	/**
	 * The crossing a crossing mirrors: for a crossing of a beside track b and a track x, the parent's crossing at the same place in its events, when it is with the same x.
	 * @param c The crossing.
	 * @return The original crossing, or UINT32_MAX when c mirrors none.
	 */
	uint32_t MirrorOf(uint32_t c) const
	{
		for (const uint32_t b : {this->map.crossings[c].a, this->map.crossings[c].b}) {
			if (this->map.tracks[b].kind == PlanarTrack::Kind::Line) continue;
			const uint32_t x = this->map.crossings[c].a == b ? this->map.crossings[c].b : this->map.crossings[c].a;
			const size_t k = EventIndex(this->map.events[b], c);
			std::vector<uint32_t> chain_events;
			if (k == SIZE_MAX || !this->ChainEvents(b, chain_events) || k >= chain_events.size()) continue;
			const uint32_t original = chain_events[k];
			const PlanarCrossing &o = this->map.crossings[original];
			if (o.a != b && o.b != b && (o.a == x || o.b == x)) return original;
		}
		return UINT32_MAX;
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
				const uint32_t original = this->MirrorOf(c);
				if (original == UINT32_MAX) {
					this->out.push_back(fmt::format("H4: crossing {} is in no structure and mirrors no crossing", c));
				} else if (c >= this->v.angles.size() || original >= this->v.angles.size() || this->v.angles[c] != this->v.angles[original]) {
					this->out.push_back(fmt::format("H4: mirrored crossing {} has another angle than the crossing {} it mirrors", c, original));
				}
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
	void CheckH5H6()
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
			if (same && piece != UINT32_MAX && (sep.pieces[piece] % 8 + 8) % 8 != shape.axis) this->out.push_back(fmt::format("H5: structure {} has axis {}, its separator's crossing piece heads {}", i, shape.axis, sep.pieces[piece]));
			const std::array<int, 2> end_run = {(s.kinds & 1) != 0 ? 1 : -1, (s.kinds & 2) != 0 ? 2 : -1};
			if (shape.end_run != end_run) this->out.push_back(fmt::format("H5: structure {} has end runs {}, {}; its kinds give {}, {}", i, shape.end_run[0], shape.end_run[1], end_run[0], end_run[1]));

			/* H6: the least middles from the headings, run by run. */
			int least;
			if (!this->Least(s, least)) continue;
			for (int kind = 0; kind < 2; kind++) {
				const bool allowed = (s.kinds & (1 << kind)) != 0;
				if (!allowed) {
					if (shape.middles[kind] != -1) this->out.push_back(fmt::format("H6: structure {} has middles {} for a kind it does not allow", i, shape.middles[kind]));
					continue;
				}
				const char *name = kind == 0 ? "bridge" : "tunnel";
				if (shape.middles[kind] != least) this->out.push_back(fmt::format("H6: structure {} has {} middles {}, the least at its angles is {}", i, name, shape.middles[kind], least));
				const int limit = s.limit[kind];
				if (least > limit) this->out.push_back(fmt::format("H6: structure {} needs {} {} middles, over the limit {}", i, least, name, limit));
			}
		}
	}

	/** @return The walked start heading of a stretch of a face walk. */
	int WalkedStart(const WalkedStretch &w) const
	{
		return w.forward ? this->v.tracks[w.track].pieces[this->First(w.track, w.stretch)] : this->v.tracks[w.track].pieces[this->Last(w.track, w.stretch)] + 4;
	}

	/** @return The walked end heading of a stretch of a face walk. */
	int WalkedEnd(const WalkedStretch &w) const
	{
		return w.forward ? this->v.tracks[w.track].pieces[this->Last(w.track, w.stretch)] : this->v.tracks[w.track].pieces[this->First(w.track, w.stretch)] + 4;
	}

	/** H7: every face winds once (-8 eighths). */
	void CheckH7Faces()
	{
		for (const PlanarFace &face : this->map.faces) {
			const FaceWalk walk = WalkFace(this->map, face);
			if (!walk.usable || walk.stretches.empty()) {
				this->out.push_back(fmt::format("H7: face {} has a stretch the shape lacks", face.face));
				continue;
			}
			int total = 0;
			for (const FaceTerm &term : walk.terms) {
				const WalkedStretch &w = walk.stretches[term.from];
				switch (term.kind) {
					case FaceTerm::Kind::Stretch:
						total += this->WalkedEnd(w) - this->WalkedStart(w);
						break;

					case FaceTerm::Kind::Box: {
						const WalkedStretch &next = walk.stretches[term.to];
						total += Norm(BOX_HEADING[term.side_a] - this->WalkedEnd(w));
						total += -2 * (((static_cast<int>(term.side_a) - static_cast<int>(term.side_b)) % 4 + 4) % 4);
						total += Norm(this->WalkedStart(next) - BOX_HEADING[term.side_b]);
						break;
					}

					case FaceTerm::Kind::Vertex:
						if (term.lopsided) this->out.push_back(fmt::format("H7: face {} leaves or enters the box at a port end only on one side of a vertex", face.face));
						total += Norm(this->WalkedStart(walk.stretches[term.to]) - this->WalkedEnd(w));
						break;
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

	/** H8: beside tracks follow their parent chain. */
	void CheckH8()
	{
		for (uint32_t b = 0; b < this->map.tracks.size(); b++) {
			if (this->map.tracks[b].kind == PlanarTrack::Kind::Line) continue;
			const std::string &name = this->map.tracks[b].name;
			std::vector<uint32_t> chain;
			if (!ParentChain(this->map, this->v.structure.sites, b, chain)) {
				this->out.push_back(fmt::format("H8: no parent chain for {}", name));
				continue;
			}
			/* The chain's pieces concatenated (a node's piece counted once) and its event pieces in that list. */
			std::vector<int> pieces;
			std::vector<uint32_t> event_piece, ep;
			for (size_t i = 0; i < chain.size(); i++) {
				const PlanarShapeTrack &sc = this->v.tracks[chain[i]];
				if (sc.pieces.empty()) break;
				size_t base = pieces.size();
				if (i > 0) {
					base--;
					if (pieces.back() != sc.pieces.front()) this->out.push_back(fmt::format("H8: the parent chain of {} turns at the node before {}", name, this->map.tracks[chain[i]].name));
					pieces.insert(pieces.end(), sc.pieces.begin() + 1, sc.pieces.end());
				} else {
					pieces = sc.pieces;
				}
				for (const uint32_t e : sc.event_piece) event_piece.push_back(static_cast<uint32_t>(base + e));
				ep.insert(ep.end(), this->map.events[chain[i]].begin(), this->map.events[chain[i]].end());
			}
			const PlanarShapeTrack &sb = this->v.tracks[b];
			if (sb.pieces != pieces) this->out.push_back(fmt::format("H8: beside track {} has other pieces than its parent chain", name));
			if (sb.event_piece != event_piece) this->out.push_back(fmt::format("H8: beside track {} has other event pieces than its parent chain", name));
			const std::vector<uint32_t> &eb = this->map.events[b];
			if (eb.size() != ep.size()) {
				this->out.push_back(fmt::format("H8: beside track {} has {} crossings, its parent chain has {}", name, eb.size(), ep.size()));
				continue;
			}
			for (size_t k = 0; k < eb.size(); k++) {
				const PlanarCrossing &cb = this->map.crossings[eb[k]], &cp = this->map.crossings[ep[k]];
				const uint32_t xb = cb.a == b ? cb.b : cb.a, xp = std::ranges::find(chain, cp.a) != chain.end() ? cp.b : cp.a;
				if (xb != xp) {
					this->out.push_back(fmt::format("H8: crossing {} of beside track {} is with track {}, its parent chain's is with {}", k, name, this->map.tracks[xb].name, this->map.tracks[xp].name));
					continue;
				}
				const size_t ib = EventIndex(this->map.events[xb], eb[k]), ip = EventIndex(this->map.events[xb], ep[k]);
				if (ib == SIZE_MAX || ip == SIZE_MAX || this->v.tracks[xb].event_piece[ib] != this->v.tracks[xb].event_piece[ip]) {
					this->out.push_back(fmt::format("H8: crossing {} of beside track {} sits on another piece of {} than its parent chain's", k, name, this->map.tracks[xb].name));
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
	o.CheckH5H6();
	o.CheckH7Faces();
	o.CheckH7Routes();
	o.CheckH8();
	h9();
	return o.out;
}

bool ShapePlanarStructureVariant(const PlanarStructureVariant &v, PlanarShapeVariant &out, std::string &why)
{
	using Terms = std::vector<std::pair<uint32_t, double>>;
	constexpr uint32_t NONE = UINT32_MAX;
	PlanarShapeVariant sv;
	sv.structure = v;
	sv.cost = 0;
	Oracle o(sv);
	const PlanarTrackMap &map = sv.structure.map;
	const size_t nt = map.tracks.size();
	const auto fail = [&](const std::string &reason) {
		why = "no shape: " + reason;
		return false;
	};

	JunctionMilp milp;
	std::vector<std::string> tags; ///< Per row, its kind and object.
	const auto var = [&](double lower, double upper, double cost, bool integer) { return milp.AddVariable(lower, upper, cost, integer); };
	const auto row = [&](Terms terms, double lower, double upper, const std::string &tag) {
		std::ranges::sort(terms);
		Terms merged;
		for (const auto &[variable, coefficient] : terms) {
			if (!merged.empty() && merged.back().first == variable) {
				merged.back().second += coefficient;
			} else {
				merged.emplace_back(variable, coefficient);
			}
		}
		std::erase_if(merged, [](const std::pair<uint32_t, double> &t) { return t.second == 0; });
		milp.AddRow(merged, lower, upper);
		tags.push_back(tag);
	};
	const auto eq = [&](Terms terms, double rhs, const std::string &tag) { row(std::move(terms), rhs, rhs, tag); };

	/* The lifted heading at every point: point 0 the start, point i the event i - 1, point n + 1 the end. */
	std::vector<std::vector<uint32_t>> H(nt);
	for (size_t t = 0; t < nt; t++) {
		if (map.events.size() != nt || map.expanded.lines.size() != nt) return fail("the map's tracks, events and lines disagree");
		for (size_t i = 0; i < map.events[t].size() + 2; i++) H[t].push_back(var(-24, 32, 0, true));
	}

	/* Ports. */
	for (uint32_t t = 0; t < nt; t++) {
		const PlanarLine &line = map.expanded.lines[t];
		if (line.from.port) eq({{H[t].front(), 1}}, static_cast<int>(map.expanded.ports[line.from.index].heading), fmt::format("port {}", t));
		if (line.to.port) eq({{H[t].back(), 1}, {var(-8, 8, 0, true), -8}}, static_cast<int>(map.expanded.ports[line.to.index].heading), fmt::format("port {}", t));
	}

	/* Nodes: the d binary and sigma of each non-straight leg, at the split at its start and at the merge at its end. */
	std::vector<std::pair<uint32_t, int>> split_leg(nt, {NONE, 0}), merge_leg(nt, {NONE, 0});
	for (const PlanarStructureSite &site : sv.structure.sites) {
		if (site.node >= map.expanded.nodes.size()) return fail(fmt::format("a site names node {}, which the map lacks", site.node));
		const PlanarNode &node = map.expanded.nodes[site.node];
		const std::vector<uint32_t> &legs = site.split ? node.out : node.in;
		const std::vector<uint32_t> &single = site.split ? node.in : node.out;
		if (single.size() != 1 || (site.straight == NONE && legs.size() != 2)) return fail(fmt::format("node {} is not a split or merge of the site's shape", site.node));
		const uint32_t one = single[0];
		const std::string tag = fmt::format("{} node {}", site.split ? "split" : "merge", site.node);
		const auto stretch = [&](uint32_t t) { return static_cast<uint32_t>(site.split ? 0 : map.events[t].size()); };
		for (const uint32_t leg : legs) {
			if (leg == site.straight) {
				if (site.split) {
					eq({{H[leg].front(), 1}, {H[one].back(), -1}}, 0, tag);
				} else {
					eq({{H[leg].back(), 1}, {H[one].front(), -1}}, 0, tag);
				}
				continue;
			}
			int sigma;
			if (site.straight == NONE) {
				const uint32_t other = legs[0] == leg ? legs[1] : legs[0];
				sigma = map.Side(leg, stretch(leg), other, stretch(other));
			} else {
				sigma = map.Side(leg, stretch(leg), site.straight, stretch(site.straight));
				if (sigma == 0) sigma = site.side;
			}
			const uint32_t d = var(0, 1, 0, true);
			if (site.split) {
				split_leg[leg] = {d, sigma};
				eq({{H[leg].front(), 1}, {H[one].back(), -1}, {d, -sigma}}, 0, tag);
			} else {
				merge_leg[leg] = {d, sigma};
				eq({{H[leg].back(), 1}, {d, sigma}, {H[one].front(), -1}, {var(-8, 8, 0, true), -8}}, 0, tag);
			}
		}
	}

	/* Crossings: the rule for the separator and, in a stack, for the other separator too. A mirrored crossing of a beside track has no row of its own: it takes its original's angle and keeps its crosser on one piece across both. */
	std::vector<uint32_t> angle(map.crossings.size(), NONE);
	for (uint32_t c = 0; c < map.crossings.size(); c++) {
		const PlanarStructure *first = o.StructureOf(c, nullptr);
		if (first == nullptr) continue;
		const PlanarStructure *second = o.StructureOf(c, first);
		angle[c] = var(1, 3, 0, true);
		for (const PlanarStructure *s : {first, second}) {
			if (s == nullptr) continue;
			const uint32_t p = s->separator;
			const uint32_t q = map.crossings[c].a == p ? map.crossings[c].b : map.crossings[c].a;
			int side;
			if (!o.Arrival(c, p, q, side)) return fail(fmt::format("crossing {} has no arrival side", c));
			const uint32_t hp = H[p][EventIndex(map.events[p], c) + 1], hq = H[q][EventIndex(map.events[q], c) + 1];
			eq({{hq, 1}, {hp, -1}, {angle[c], -side}, {var(-8, 8, 0, true), -8}}, 0, fmt::format("cross {}", c));
			eq({{hp, 1}, {var(-12, 16, 0, true), -2}}, 0, fmt::format("axis {}", c));
		}
	}
	for (uint32_t c = 0; c < map.crossings.size(); c++) {
		if (angle[c] != NONE) continue;
		const uint32_t original = o.MirrorOf(c);
		if (original == UINT32_MAX || angle[original] == NONE) return fail(fmt::format("crossing {} is in no structure and mirrors no crossing", c));
		angle[c] = angle[original];
		/* The crosser is the track the two crossings share. */
		const PlanarCrossing &m = map.crossings[c], &r = map.crossings[original];
		const uint32_t crosser = (m.a == r.a || m.a == r.b) ? m.a : m.b;
		eq({{H[crosser][EventIndex(map.events[crosser], c) + 1], 1}, {H[crosser][EventIndex(map.events[crosser], original) + 1], -1}}, 0, fmt::format("mirror {}", c));
	}

	/* Capacity: per structure and kind, the middles at the chosen angles fit the limit Structure handed down. Crosser k has the angle a_k = b1 + 2 b2 + 3 b3 (one binary set); a run of diagonal crossers at one angle starts at r_k (run starts: r_k >= b1_k - b1_{k-1}, r_k >= b3_k - b3_{k-1}), and n + sum r_k <= limit (H6's width). The binaries serve every kind of the structure. */
	for (size_t i = 0; i < sv.structure.structures.size(); i++) {
		const PlanarStructure &s = sv.structure.structures[i];
		const size_t n = s.crossings.size();
		if (n == 0 || (s.limit[0] < 0 && s.limit[1] < 0)) continue;
		const std::string tag = fmt::format("cap {}", i);
		Terms starts;
		std::vector<uint32_t> prev(2, NONE);
		for (size_t k = 0; k < n; k++) {
			const uint32_t b1 = var(0, 1, 0, true), b2 = var(0, 1, 0, true), b3 = var(0, 1, 0, true), r = var(0, JunctionMilp::INF, 0, false);
			eq({{b1, 1}, {b2, 1}, {b3, 1}}, 1, tag);
			eq({{angle[s.crossings[k]], 1}, {b1, -1}, {b2, -2}, {b3, -3}}, 0, tag);
			if (k == 0) {
				row({{r, 1}, {b1, -1}, {b3, -1}}, 0, JunctionMilp::INF, tag);
			} else {
				row({{r, 1}, {b1, -1}, {prev[0], 1}}, 0, JunctionMilp::INF, tag);
				row({{r, 1}, {b3, -1}, {prev[1], 1}}, 0, JunctionMilp::INF, tag);
			}
			prev = {b1, b3};
			starts.emplace_back(r, 1);
		}
		for (int kind = 0; kind < 2; kind++) {
			if (s.limit[kind] >= 0) row(starts, -JunctionMilp::INF, s.limit[kind] - static_cast<double>(n), tag);
		}
	}

	/* Probe (SPEC-slots Q1, PLANAR_H11): a structure's crossers are parallel at their crossings (headings equal mod 4). */
	if (std::getenv("PLANAR_H11") != nullptr) {
		for (size_t i = 0; i < sv.structure.structures.size(); i++) {
			const PlanarStructure &s = sv.structure.structures[i];
			for (size_t k = 1; k < s.crossings.size(); k++) {
				const size_t e0 = EventIndex(map.events[s.crossers[0]], s.crossings[0]), ek = EventIndex(map.events[s.crossers[k]], s.crossings[k]);
				if (e0 == SIZE_MAX || ek == SIZE_MAX) return fail(fmt::format("structure {} lists a crossing its crosser lacks", i));
				const uint32_t h0 = H[s.crossers[0]][e0 + 1];
				const uint32_t hk = H[s.crossers[k]][ek + 1];
				eq({{hk, 1}, {h0, -1}, {var(-16, 16, 0, true), -4}}, 0, fmt::format("h11 {}", i));
			}
		}
	}

	/* Structures: one axis across the span. */
	for (size_t i = 0; i < sv.structure.structures.size(); i++) {
		const PlanarStructure &s = sv.structure.structures[i];
		for (size_t k = 1; k < s.crossings.size(); k++) {
			const size_t a = EventIndex(map.events[s.separator], s.crossings[k - 1]), b = EventIndex(map.events[s.separator], s.crossings[k]);
			if (a == SIZE_MAX || b == SIZE_MAX) return fail(fmt::format("structure {} lists a crossing its separator lacks", i));
			eq({{H[s.separator][a + 1], 1}, {H[s.separator][b + 1], -1}}, 0, fmt::format("struct {}", i));
		}
	}

	/* Beside tracks follow their parent chain: equal headings at the corresponding points, and the chain turns one way across each stretch of the beside track, as the beside track does. */
	for (uint32_t b = 0; b < nt; b++) {
		if (map.tracks[b].kind == PlanarTrack::Kind::Line) continue;
		const std::string tag = fmt::format("beside {}", b);
		std::vector<uint32_t> chain;
		if (!ParentChain(map, sv.structure.sites, b, chain)) return fail(fmt::format("H8: no parent chain for {}", map.tracks[b].name));
		/* The chain's points (start, each event, end) and its stretches with the number of events before each. */
		std::vector<uint32_t> points{H[chain.front()].front()};
		std::vector<std::tuple<uint32_t, uint32_t, size_t>> stretches; ///< From heading, to heading, events before.
		for (const uint32_t t : chain) {
			const size_t before = points.size() - 1;
			for (size_t i = 0; i + 1 < H[t].size(); i++) stretches.emplace_back(H[t][i], H[t][i + 1], before + i);
			for (size_t i = 1; i + 1 < H[t].size(); i++) points.push_back(H[t][i]);
		}
		points.push_back(H[chain.back()].back());
		if (H[b].size() != points.size()) return fail(fmt::format("beside track {} has other events than its parent chain", b));
		for (size_t i = 0; i < H[b].size(); i++) eq({{H[b][i], 1}, {points[i], -1}}, 0, tag);
		constexpr double M = 64;
		for (size_t m = 0; m + 1 < H[b].size(); m++) {
			const uint32_t u = var(0, 1, 0, true);
			for (const auto &[from, to, before] : stretches) {
				if (before != m) continue;
				row({{to, 1}, {from, -1}, {u, -M}}, -M, JunctionMilp::INF, tag);
				row({{to, 1}, {from, -1}, {u, -M}}, -JunctionMilp::INF, 0, tag);
			}
		}
	}

	/* Faces. */
	for (const PlanarFace &face : map.faces) {
		const FaceWalk walk = WalkFace(map, face);
		if (!walk.usable || walk.stretches.empty()) return fail(fmt::format("face {} has a stretch the map lacks", face.face));
		const std::string tag = fmt::format("face {}", face.face);
		/* A walked heading is a variable plus a constant: a backward stretch walks the heading + 4 from its far end. */
		const auto head = [&](const WalkedStretch &w, bool at_end) -> std::pair<uint32_t, double> {
			if (w.forward) return {H[w.track][at_end ? w.stretch + 1 : w.stretch], 0};
			return {H[w.track][at_end ? w.stretch : w.stretch + 1], 4};
		};
		Terms sum;
		double rhs = -8;
		for (const FaceTerm &term : walk.terms) {
			const WalkedStretch &w = walk.stretches[term.from];
			switch (term.kind) {
				case FaceTerm::Kind::Stretch:
					sum.emplace_back(head(w, true).first, 1);
					sum.emplace_back(head(w, false).first, -1);
					break;

				case FaceTerm::Kind::Vertex: {
					const auto [next_var, next_c] = head(walk.stretches[term.to], false);
					const auto [prev_var, prev_c] = head(w, true);
					const uint32_t vertex = var(-4, 3, 0, true);
					eq({{next_var, 1}, {prev_var, -1}, {vertex, -1}, {var(-8, 8, 0, true), -8}}, prev_c - next_c, tag);
					sum.emplace_back(vertex, 1);
					break;
				}

				case FaceTerm::Kind::Box: {
					const auto [arrive_var, arrive_c] = head(w, true);
					const auto [leave_var, leave_c] = head(walk.stretches[term.to], false);
					const uint32_t v1 = var(-4, 3, 0, true), v2 = var(-4, 3, 0, true);
					eq({{v1, 1}, {arrive_var, 1}, {var(-8, 8, 0, true), 8}}, BOX_HEADING[term.side_a] - arrive_c, tag);
					eq({{v2, 1}, {leave_var, -1}, {var(-8, 8, 0, true), 8}}, leave_c - BOX_HEADING[term.side_b], tag);
					sum.emplace_back(v1, 1);
					sum.emplace_back(v2, 1);
					rhs += 2 * (((static_cast<int>(term.side_a) - static_cast<int>(term.side_b)) % 4 + 4) % 4);
					break;
				}
			}
		}
		eq(sum, rhs, tag);
	}

	/* No U-turn: the cumulative turn along every route path stays within 3 eighths of the entry heading. */
	bool broken = false;
	for (size_t r = 0; r < map.expanded.routes.size() && !broken; r++) {
		const auto carries = [&](uint32_t t) { return std::ranges::find(map.expanded.lines[t].routes, static_cast<uint32_t>(r)) != map.expanded.lines[t].routes.end(); };
		const std::string tag = fmt::format("uturn route {}", r);
		const auto walk = [&](const auto &self, uint32_t t, Terms prefix, size_t depth) -> void {
			if (depth > nt) {
				broken = true;
				return;
			}
			for (size_t i = 0; i < H[t].size(); i++) {
				if (prefix.empty() && i == 0) continue;
				Terms cum = prefix;
				cum.emplace_back(H[t][i], 1);
				cum.emplace_back(H[t].front(), -1);
				row(cum, -3, 3, tag);
			}
			const PlanarEnd &to = map.expanded.lines[t].to;
			if (to.port) return;
			Terms next = prefix;
			next.emplace_back(H[t].back(), 1);
			next.emplace_back(H[t].front(), -1);
			const PlanarNode &node = map.expanded.nodes[to.index];
			for (const uint32_t g : node.out) {
				if (!carries(g)) continue;
				Terms through = next;
				const auto &[d, sigma] = node.out.size() > 1 ? split_leg[g] : merge_leg[t];
				if (d != NONE) through.emplace_back(d, sigma);
				self(self, g, std::move(through), depth + 1);
			}
		};
		for (uint32_t t = 0; t < nt; t++) {
			if (map.expanded.lines[t].from.port && carries(t)) walk(walk, t, {}, 0);
		}
	}
	if (broken) return fail("a route path loops");

	/* The turning of the Line tracks. */
	for (uint32_t t = 0; t < nt; t++) {
		if (map.tracks[t].kind != PlanarTrack::Kind::Line) continue;
		for (size_t k = 0; k + 1 < H[t].size(); k++) {
			const uint32_t u = var(0, JunctionMilp::INF, 1000, false);
			row({{u, 1}, {H[t][k + 1], -1}, {H[t][k], 1}}, 0, JunctionMilp::INF, "obj");
			row({{u, 1}, {H[t][k + 1], 1}, {H[t][k], -1}}, 0, JunctionMilp::INF, "obj");
		}
	}

	milp.SetTimeLimit(5.0);
	const JunctionMilp::Status status = milp.Solve();
	if (status != JunctionMilp::Status::Optimal) {
		std::string reason = status == JunctionMilp::Status::Infeasible ? "Infeasible" : "Error";
		if (status == JunctionMilp::Status::Infeasible) {
			std::string last;
			for (const uint32_t r : milp.InfeasibleRows()) {
				if (r >= tags.size() || tags[r] == last) continue;
				last = tags[r];
				reason += (reason == "Infeasible" ? ": " : ", ") + last;
			}
		}
		return fail(reason);
	}

	/* The pieces: each stretch walks one eighth at a time from its start heading to its end heading. */
	sv.tracks.resize(nt);
	for (uint32_t t = 0; t < nt; t++) {
		PlanarShapeTrack &track = sv.tracks[t];
		const auto heading = [&](size_t i) { return static_cast<int>(std::llround(milp.Value(H[t][i]))); };
		track.pieces.push_back(heading(0));
		for (size_t k = 0; k + 1 < H[t].size(); k++) {
			const int to = heading(k + 1);
			while (track.pieces.back() != to) track.pieces.push_back(track.pieces.back() + (to > track.pieces.back() ? 1 : -1));
			if (k + 2 < H[t].size()) track.event_piece.push_back(static_cast<uint32_t>(track.pieces.size() - 1));
			if (map.tracks[t].kind == PlanarTrack::Kind::Line) sv.cost += std::abs(to - heading(k));
		}
	}
	for (const uint32_t a : angle) sv.angles.push_back(static_cast<uint8_t>(std::llround(milp.Value(a))));
	for (const PlanarStructure &s : sv.structure.structures) {
		PlanarShapeStructure shape{0, {-1, -1}, {(s.kinds & 1) != 0 ? 1 : -1, (s.kinds & 2) != 0 ? 2 : -1}};
		if (!s.crossings.empty()) shape.axis = static_cast<uint8_t>((o.EventHeading(s.separator, EventIndex(map.events[s.separator], s.crossings.front())) % 8 + 8) % 8);
		int least;
		if (!o.Least(s, least)) return fail("a crossing's angle is not in 1..3");
		for (int kind = 0; kind < 2; kind++) {
			if ((s.kinds & (1 << kind)) != 0) shape.middles[kind] = least;
		}
		sv.structures.push_back(shape);
	}
	out = std::move(sv);
	return true;
}

PlanarShapeResult BuildPlanarShape(PlanarStructureResult structure)
{
	PlanarShapeResult result;
	for (PlanarStructureVariant &v : structure.variants) {
		PlanarShapeVariant shaped;
		std::string why;
		if (ShapePlanarStructureVariant(v, shaped, why)) {
			result.variants.push_back(std::move(shaped));
		} else {
			result.unshaped.emplace_back(v.tag, why);
		}
	}
	std::ranges::stable_sort(result.variants, [](const PlanarShapeVariant &a, const PlanarShapeVariant &b) {
		return std::tie(a.structure.cost, a.cost, a.structure.tag) < std::tie(b.structure.cost, b.cost, b.structure.tag);
	});
	if (result.variants.empty()) result.why = structure.why.empty() ? "no variant shaped" : structure.why;
	return result;
}
