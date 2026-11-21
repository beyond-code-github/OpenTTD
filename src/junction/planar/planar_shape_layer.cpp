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
#include "planar_model.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <map>
#include <optional>
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

} // namespace

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

static bool FreeOnePieceStretch(const PlanarShapeVariant &v, uint32_t t, size_t k);

/**
 * The pairs of lines H14 compares: per channel, its whole Line tracks without parts, each later one paired with the first earlier one it is not a leg of one node with, none when every earlier one is such a leg (D388 (a), D403).
 * @param map The track map.
 * @param structure The structure variant (channel of each line, line groups).
 * @return Per pair: channel, first track, other track, the first's and the other's channel-crossing event indices (a node group's own crossings left out), and whether the two cross the same channels in order.
 */
static auto SpinePairs(const PlanarTrackMap &map, const PlanarStructureVariant &structure)
{
	using Pair = std::tuple<uint32_t, uint32_t, uint32_t, std::vector<size_t>, std::vector<size_t>, bool>;
	const std::vector<uint32_t> &channel = structure.channel_of_line;
	const std::vector<std::array<uint32_t, 2>> &groups = structure.line_groups;
	std::map<uint32_t, std::vector<uint32_t>> lines;
	for (uint32_t t = 0; t < map.tracks.size(); t++) {
		const PlanarTrack &track = map.tracks[t];
		if (track.kind != PlanarTrack::Kind::Line || track.line != t || track.line >= channel.size()) continue;
		bool has_parts = false;
		for (uint32_t u = 0; u < map.tracks.size(); u++) {
			has_parts = has_parts || (u != t && map.tracks[u].kind == PlanarTrack::Kind::Line && map.part_of[u] == t);
		}
		if (has_parts) continue;
		lines[channel[track.line]].push_back(t);
	}
	/* The events of track t that cross another channel: not a crossing of two lines sharing a start group or an end group. */
	const auto channel_events = [&](uint32_t t) {
		std::vector<size_t> list;
		for (size_t k = 0; k < map.events[t].size(); k++) {
			const PlanarCrossing &x = map.crossings[map.events[t][k]];
			const uint32_t la = map.tracks[t].line, lb = map.tracks[x.a == t ? x.b : x.a].line;
			bool node_group = false;
			for (size_t end = 0; end < 2; end++) node_group = node_group || (groups[la][end] != UINT32_MAX && groups[la][end] == groups[lb][end]);
			if (!node_group) list.push_back(k);
		}
		return list;
	};
	const auto partner_of = [&](uint32_t t, size_t k) {
		const PlanarCrossing &x = map.crossings[map.events[t][k]];
		return x.a == t ? x.b : x.a;
	};
	/* Two tracks are legs of one node when a node has both among its arriving lines or both among its leaving lines. */
	const auto legs_of_one_node = [&](uint32_t x, uint32_t y) {
		for (const PlanarNode &node : map.expanded.nodes) {
			for (const std::vector<uint32_t> *side : {&node.in, &node.out}) {
				if (std::ranges::find(*side, x) != side->end() && std::ranges::find(*side, y) != side->end()) return true;
			}
		}
		return false;
	};
	std::vector<Pair> pairs;
	for (const auto &[c, members] : lines) {
		for (size_t m = 1; m < members.size(); m++) {
			const uint32_t b = members[m];
			uint32_t a = UINT32_MAX;
			for (size_t u = 0; u < m && a == UINT32_MAX; u++) {
				if (!legs_of_one_node(members[u], b)) a = members[u];
			}
			if (a == UINT32_MAX) continue;
			std::vector<size_t> fa = channel_events(a), fb = channel_events(b);
			bool same = fa.size() == fb.size();
			for (size_t k = 0; same && k < fa.size(); k++) same = channel[map.tracks[partner_of(a, fa[k])].line] == channel[map.tracks[partner_of(b, fb[k])].line];
			pairs.emplace_back(c, a, b, std::move(fa), std::move(fb), same);
		}
	}
	return pairs;
}

namespace {

/** One wall of a cell with the turn at its end corner. */
struct Wall {
	uint32_t track; ///< The track, PLANAR_DUMMY_TRACK for a dummy, or UINT32_MAX for a box edge.
	uint32_t piece; ///< The piece of the track, the dummy's index in the variant's dummies, or the box side.
	int heading; ///< The walked heading.
	int turn; ///< The turn at the corner after this wall.
};

/** D407 (1): cells are refined and checked (H12, H12b) with every flex pair at size 0: a flex insert is no wall of the walk and its bends are no corners. */
static constexpr bool FLEX_CELLS_AT_ZERO = true;

struct Oracle;

/**
 * A face's piece-level walk: its walls in walk order with the turn at each end corner.
 * @param o The oracle (the shape and map it reads).
 * @param walk The face's walk.
 * @param walls Receives the walls.
 * @param piece_total Receives the sum of the turns of the walls.
 * @param h7 Receives the winding the stretch walk gives (H7's sum).
 * @return False when the two totals differ.
 */
bool CellWalls(const Oracle &o, const FaceWalk &walk, std::vector<Wall> &walls, int &piece_total, int &h7);

/**
 * Split one cell of a list at a dummy: the cell holding the dummy's corner and landing wall becomes the cell walked from the corner's outgoing wall to the landing wall and the dummy, and the cell from the landing wall to the corner and the dummy is appended.
 * @param cells The cells of one face.
 * @param d The dummy.
 * @param index The dummy's index in the variant's dummies.
 * @param why Receives the oracle's reason when no cell holds the corner and the landing wall.
 * @return False on failure.
 */
bool SplitCell(std::vector<std::vector<Wall>> &cells, const PlanarShapeDummy &d, uint32_t index, std::string &why)
{
	size_t cell = SIZE_MAX, s = 0, l = 0;
	bool has_corner = false;
	for (size_t c = 0; c < cells.size() && cell == SIZE_MAX; c++) {
		const std::vector<Wall> &w = cells[c];
		size_t found_s = SIZE_MAX, found_l = SIZE_MAX;
		for (size_t i = 0; i < w.size() && found_s == SIZE_MAX; i++) {
			const Wall &a = w[i], &b = w[(i + 1) % w.size()];
			if (a.track == d.track && b.track == d.track && ((a.piece == d.piece && b.piece == d.piece + 1) || (a.piece == d.piece + 1 && b.piece == d.piece))) found_s = i;
		}
		if (found_s == SIZE_MAX) continue;
		has_corner = true;
		for (size_t i = 0; i < w.size() && found_l == SIZE_MAX; i++) {
			if (w[i].track == d.land_track && w[i].piece == d.land_piece) found_l = i;
		}
		if (found_l != SIZE_MAX) {
			cell = c;
			s = found_s;
			l = found_l;
		}
	}
	if (cell == SIZE_MAX) {
		why = has_corner ? "lands on a wall not in its cell" : "does not start at a corner of its cell";
		return false;
	}
	const std::vector<Wall> parent = cells[cell];
	const size_t n = parent.size();
	const size_t count = (l + n - s - 1) % n + 1;
	std::vector<Wall> a, b;
	for (size_t k = 1; k <= count; k++) a.push_back(parent[(s + k) % n]);
	a.back().turn = Norm(d.heading + 4 - a.back().heading);
	a.push_back({PLANAR_DUMMY_TRACK, index, d.heading + 4, Norm(parent[(s + 1) % n].heading - (d.heading + 4))});
	for (size_t k = 0; k <= n - count; k++) b.push_back(parent[(l + k) % n]);
	b.front().turn = parent[l].turn;
	b.back().turn = Norm(d.heading - b.back().heading);
	b.push_back({PLANAR_DUMMY_TRACK, index, d.heading, Norm(b.front().heading - d.heading)});
	cells[cell] = a;
	cells.push_back(b);
	return true;
}

/**
 * A cell walk with every flex insert removed (SPEC-shape §8.8, reduced walk): an insert's turn is added to the wall before it, so the walls either side meet with turn 0.
 * @param v The variant; `PlanarShapeTrack::flex` names the inserts.
 * @param walls The walk, in order, cyclic.
 * @return The walk without inserts.
 */
static std::vector<Wall> ReducedWalls(const PlanarShapeVariant &v, const std::vector<Wall> &walls)
{
	const auto is_insert = [&](const Wall &w) {
		return w.track < v.tracks.size() && std::ranges::find(v.tracks[w.track].flex, w.piece) != v.tracks[w.track].flex.end();
	};
	std::vector<Wall> out;
	int carry = 0; // Turns of inserts seen before any kept wall.
	for (const Wall &w : walls) {
		if (is_insert(w)) {
			if (out.empty()) carry += w.turn; else out.back().turn += w.turn;
		} else {
			out.push_back(w);
		}
	}
	if (out.empty()) return walls;
	out.back().turn += carry;
	return out;
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
	 * The least middles of one span at the angles of its crossings (H6, D396): its width, n + r, where r is the number of runs (maximal groups of consecutive crossers whose headings at their crossings are equal mod 4) with a crosser that crosses at an angle other than 2.
	 * @param deck The deck track (the separator or the copy).
	 * @param crossings The span's crossings.
	 * @param crossers The crossers, one per crossing.
	 * @param least Receives the least middles.
	 * @return False when a crossing has no angle in 1..3.
	 */
	bool Least(uint32_t deck, const std::vector<uint32_t> &crossings, const std::vector<uint32_t> &crossers, int &least) const
	{
		int diagonal_runs = 0, previous = 0;
		bool run_diagonal = false;
		for (size_t k = 0; k < crossings.size(); k++) {
			int a;
			if (!this->Angle(crossings[k], deck, crossers[k], a) || a < 1 || a > 3) return false;
			const int heading = this->EventHeading(crossers[k], EventIndex(this->map.events[crossers[k]], crossings[k]));
			if (k == 0 || ((heading - previous) % 4 + 4) % 4 != 0) {
				diagonal_runs += run_diagonal ? 1 : 0;
				run_diagonal = false;
			}
			if (a != 2) run_diagonal = true;
			previous = heading;
		}
		diagonal_runs += run_diagonal ? 1 : 0;
		least = static_cast<int>(crossings.size()) + diagonal_runs;
		return true;
	}

	/** @return The crossings of the structure's copy span (those of its copy that mirror one of its crossings) in the copy's travel order; empty without a copy. */
	std::vector<uint32_t> CopyCrossings(const PlanarStructure &s) const
	{
		std::vector<uint32_t> crossings;
		if (s.copy == UINT32_MAX) return crossings;
		for (const uint32_t c : this->map.events[s.copy]) {
			if (std::ranges::find(s.crossings, this->MirrorOf(c)) != s.crossings.end()) crossings.push_back(c);
		}
		return crossings;
	}

	/** @return The structure whose copy span holds the crossing, or nullptr. */
	const PlanarStructure *CopyDeckOf(uint32_t c) const
	{
		for (const PlanarStructure &s : this->v.structure.structures) {
			const std::vector<uint32_t> crossings = this->CopyCrossings(s);
			if (std::ranges::find(crossings, c) != crossings.end()) return &s;
		}
		return nullptr;
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

	/** @return Whether piece i of stretch k of track t is a flex pair's middle or its return piece, where the middle lies strictly inside the stretch. */
	bool FlexRemoved(uint32_t t, size_t k, uint32_t i) const
	{
		for (const uint32_t m : this->v.tracks[t].flex) {
			if (this->First(t, k) < m && m < this->Last(t, k) && (i == m || i == m + 1)) return true;
		}
		return false;
	}

	/** H1: pieces, event pieces and monotone stretches; a flex pair is a bend pair on a free stretch and does not count as a turn (D397). */
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
			for (const uint32_t i : s.flex) {
				if (i < 1 || i + 2 > s.pieces.size() || s.pieces[i - 1] != s.pieces[i + 1] || std::abs(s.pieces[i] - s.pieces[i - 1]) != 1) {
					this->out.push_back(fmt::format("H1: track {} flex at piece {} is not a bend pair", name, i));
					continue;
				}
				if (std::ranges::find(s.event_piece, i) != s.event_piece.end()) this->out.push_back(fmt::format("H1: track {} flex at piece {} carries a crossing", name, i));
				bool free = false;
				for (size_t k = 0; k <= s.event_piece.size(); k++) {
					if (this->First(t, k) >= i || i >= this->Last(t, k)) continue;
					size_t kept = 0;
					for (uint32_t j = this->First(t, k); j <= this->Last(t, k); j++) kept += this->FlexRemoved(t, k, j) ? 0 : 1;
					free = kept == 1 && FreeOnePieceStretch(this->v, t, k);
				}
				if (!free) this->out.push_back(fmt::format("H1: track {} flex at piece {} is not on a free stretch", name, i));
			}
			for (size_t k = 0; k <= s.event_piece.size(); k++) {
				bool up = false, down = false;
				std::vector<int> list;
				for (uint32_t i = this->First(t, k); i <= this->Last(t, k); i++) {
					if (!this->FlexRemoved(t, k, i)) list.push_back(s.pieces[i]);
				}
				for (size_t i = 0; i + 1 < list.size(); i++) {
					if (list[i + 1] > list[i]) up = true;
					if (list[i + 1] < list[i]) down = true;
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
				/* A topology node (every track at it a Line) turns its leg always (no cusps, D404); a copy's or row's node may keep the trunk heading (H8). */
				bool topology = true;
				for (const std::vector<uint32_t> *side : {&node.in, &node.out}) {
					for (const uint32_t t : *side) topology = topology && this->map.tracks[t].kind == PlanarTrack::Kind::Line;
				}
				if (topology && SameHeading(heading, h)) {
					this->out.push_back(fmt::format("H3: leg {} at node {} is at the trunk heading (a cusp, D404)", this->map.tracks[leg].name, site.node));
				} else if (!SameHeading(heading, h) && !SameHeading(heading, turned)) {
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
			const PlanarStructure *copy_deck = first == nullptr ? this->CopyDeckOf(c) : nullptr;
			if (first == nullptr && copy_deck == nullptr) {
				const uint32_t original = this->MirrorOf(c);
				if (original == UINT32_MAX) {
					this->out.push_back(fmt::format("H4: crossing {} is in no structure and mirrors no crossing", c));
				} else if (c >= this->v.angles.size() || original >= this->v.angles.size() || this->v.angles[c] != this->v.angles[original]) {
					this->out.push_back(fmt::format("H4: mirrored crossing {} has another angle than the crossing {} it mirrors", c, original));
				}
				continue;
			}
			const PlanarStructure *second = copy_deck != nullptr ? nullptr : this->StructureOf(c, first);
			const uint32_t p = copy_deck != nullptr ? copy_deck->copy : first->separator;
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
			const std::vector<uint32_t> copy_crossings = this->CopyCrossings(s);
			std::vector<uint32_t> copy_crossers;
			for (const uint32_t c : copy_crossings) copy_crossers.push_back(this->map.crossings[c].a == s.copy ? this->map.crossings[c].b : this->map.crossings[c].a);
			for (const bool copy : {false, true}) {
				int least = -1;
				const bool has = !copy || s.copy != UINT32_MAX;
				if (has && !(copy ? this->Least(s.copy, copy_crossings, copy_crossers, least) : this->Least(s.separator, s.crossings, s.crossers, least))) continue;
				const std::array<int, 2> &middles = copy ? shape.copy_middles : shape.middles;
				for (int kind = 0; kind < 2; kind++) {
					const bool allowed = has && (s.kinds & (1 << kind)) != 0;
					const char *deck = copy ? "copy " : "";
					if (!allowed) {
						if (middles[kind] != -1) this->out.push_back(fmt::format("H6: structure {} has {}middles {} for a kind it does not allow", i, deck, middles[kind]));
						continue;
					}
					const char *name = kind == 0 ? "bridge" : "tunnel";
					if (middles[kind] != least) this->out.push_back(fmt::format("H6: structure {} has {}{} middles {}, the least at its angles is {}", i, deck, name, middles[kind], least));
					const int limit = s.limit[kind];
					if (least > limit) this->out.push_back(fmt::format("H6: structure {} needs {} {}{} middles, over the limit {}", i, least, deck, name, limit));
				}
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

	/** H12: every cell of every face turns one way and winds once (SPEC-shape §8.1, D385). */
	void CheckH12()
	{
		const auto text = [&](const Wall &w) {
			static const char *const SIDES[4] = {"N", "E", "S", "W"};
			if (w.track == PLANAR_DUMMY_TRACK) return fmt::format("dummy {}", w.piece);
			if (w.track == UINT32_MAX) return fmt::format("box {}", SIDES[w.piece % 4]);
			return fmt::format("{} piece {}", w.track < this->map.tracks.size() ? this->map.tracks[w.track].name : fmt::format("{}", w.track), w.piece);
		};
		for (const PlanarFace &face : this->map.faces) {
			const FaceWalk walk = WalkFace(this->map, face);
			if (!walk.usable || walk.stretches.empty()) continue;
			std::vector<Wall> first;
			int piece_total, h7;
			if (!CellWalls(*this, walk, first, piece_total, h7)) {
				this->out.push_back(fmt::format("H12: face {} piece walk total {} differs from H7's {}", face.face, piece_total, h7));
				continue;
			}
			if (piece_total != -8) continue; // H7 reports a face that does not wind once.
			if constexpr (FLEX_CELLS_AT_ZERO) first = ReducedWalls(this->v, first);

			std::vector<std::vector<Wall>> cells = {first};
			for (size_t di = 0; di < this->v.dummies.size(); di++) {
				const PlanarShapeDummy &d = this->v.dummies[di];
				if (d.face != static_cast<uint32_t>(face.face)) continue;
				std::string reason;
				if (!SplitCell(cells, d, static_cast<uint32_t>(di), reason)) {
					const std::string start_text = d.track < this->map.tracks.size() ? this->map.tracks[d.track].name : fmt::format("{}", d.track);
					this->out.push_back(fmt::format("H12: face {} dummy at {} piece {} {}", face.face, start_text, d.piece, reason));
				}
			}

			for (size_t c = 0; c < cells.size(); c++) {
				const std::vector<Wall> &w = cells[c];
				int total = 0;
				for (const Wall &x : w) total += x.turn;
				bool breached = false;
				for (size_t i = 0; i < w.size() && !breached; i++) {
					if (w[i].turn <= 0) continue;
					breached = true;
					this->out.push_back(fmt::format("H12: face {} cell {} turns both ways at {} and {}", face.face, c, text(w[i]), text(w[(i + 1) % w.size()])));
				}
				if (!breached && total != -8) this->out.push_back(fmt::format("H12: face {} cell {} winds {} eighths, not -8", face.face, c, total));
			}
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
				if (this->CopyDeckOf(eb[k]) != nullptr) continue;
				const size_t ib = EventIndex(this->map.events[xb], eb[k]), ip = EventIndex(this->map.events[xb], ep[k]);
				if (ib == SIZE_MAX || ip == SIZE_MAX || this->v.tracks[xb].event_piece[ib] != this->v.tracks[xb].event_piece[ip]) {
					this->out.push_back(fmt::format("H8: crossing {} of beside track {} sits on another piece of {} than its parent chain's", k, name, this->map.tracks[xb].name));
				}
			}
		}
	}

	/** @return The pieces from piece `first` to piece `last` of a track, flex pairs removed (H1's helper); a flex middle strictly inside is removed with its return piece. */
	std::vector<int> StretchPieces(uint32_t t, uint32_t first, uint32_t last) const
	{
		std::vector<int> list;
		for (uint32_t i = first; i <= last; i++) {
			bool removed = false;
			for (const uint32_t m : this->v.tracks[t].flex) removed = removed || (first < m && m < last && (i == m || i == m + 1));
			if (!removed) list.push_back(this->v.tracks[t].pieces[i]);
		}
		return list;
	}

	/** H14 (DESIGN-sss-v2 §3.2, D388 (a)): the lines of one channel cross the same channels in order and follow one heading list between their first and last channel crossing; a node group's own crossings are node territory and left out; lines that are legs of one node are exempt (D403). */
	void CheckH14()
	{
		const std::vector<uint32_t> &channel = this->v.structure.channel_of_line;
		for (const auto &[c, a, b, fa, fb, sequences_same] : SpinePairs(this->map, this->v.structure)) {
			{
				const auto partner_of = [&](uint32_t t, size_t k) {
					const PlanarCrossing &x = this->map.crossings[this->map.events[t][k]];
					return x.a == t ? x.b : x.a;
				};
				const bool same = sequences_same && this->v.tracks[a].event_piece.size() == this->map.events[a].size() && this->v.tracks[b].event_piece.size() == this->map.events[b].size();
				if (!same) {
					const auto sequence = [&](uint32_t t, const std::vector<size_t> &f) {
						std::string text;
						for (const size_t k : f) {
							const uint32_t other = partner_of(t, k);
							text += fmt::format("{}{}/{}", text.empty() ? "" : ", ", this->map.tracks[other].name, channel[this->map.tracks[other].line]);
						}
						return text;
					};
					this->out.push_back(fmt::format("H14: channel {} lines {} and {} cross different channels: {}: [{}] vs {}: [{}]", c, this->map.tracks[a].name, this->map.tracks[b].name, this->map.tracks[a].name, sequence(a, fa), this->map.tracks[b].name, sequence(b, fb)));
					continue;
				}
				for (size_t k = 1; k < fa.size(); k++) {
					const auto &pa = this->v.tracks[a].event_piece, &pb = this->v.tracks[b].event_piece;
					if (this->StretchPieces(a, pa[fa[k - 1]], pa[fa[k]]) != this->StretchPieces(b, pb[fb[k - 1]], pb[fb[k]]) || this->EventHeading(a, fa[k - 1]) != this->EventHeading(b, fb[k - 1]) || this->EventHeading(a, fa[k]) != this->EventHeading(b, fb[k])) {
						this->out.push_back(fmt::format("H14: channel {} lines {} and {} differ between crossings {} and {}", c, this->map.tracks[a].name, this->map.tracks[b].name, k - 1, k));
					}
				}
			}
		}
	}
};

bool CellWalls(const Oracle &o, const FaceWalk &walk, std::vector<Wall> &first, int &piece_total, int &h7)
{
	first.clear();
	h7 = 0;
	for (const FaceTerm &term : walk.terms) {
		const WalkedStretch &w = walk.stretches[term.from];
		if (term.kind == FaceTerm::Kind::Stretch) {
			const uint32_t a = o.First(w.track, w.stretch), b = o.Last(w.track, w.stretch);
			const std::vector<int> &pieces = o.v.tracks[w.track].pieces;
			if (b < a || b >= pieces.size()) return false; // Event pieces out of order: the tracks fail H1, there is no cell to walk.
			for (uint32_t k = 0; k <= b - a; k++) {
				const uint32_t p = w.forward ? a + k : b - k;
				first.push_back({w.track, p, w.forward ? pieces[p] : pieces[p] + 4, 0});
				if (k > 0) first[first.size() - 2].turn = first.back().heading - first[first.size() - 2].heading;
			}
			h7 += o.WalkedEnd(w) - o.WalkedStart(w);
			continue;
		}
		const WalkedStretch &next = walk.stretches[term.to];
		if (term.kind == FaceTerm::Kind::Vertex) {
			first.back().turn = Norm(o.WalkedStart(next) - o.WalkedEnd(w));
			h7 += first.back().turn;
			continue;
		}
		first.back().turn = Norm(BOX_HEADING[term.side_a] - o.WalkedEnd(w));
		h7 += first.back().turn;
		const uint32_t count = ((term.side_a + 4 - term.side_b) % 4) + 1;
		for (uint32_t k = 0; k < count; k++) {
			const uint32_t side = (term.side_a + 4 - k) % 4;
			first.push_back({UINT32_MAX, side, BOX_HEADING[side], -2});
			h7 += -2;
		}
		first.back().turn = Norm(o.WalkedStart(next) - BOX_HEADING[term.side_b]);
		h7 += first.back().turn + 2;
	}
	piece_total = 0;
	for (const Wall &w : first) piece_total += w.turn;
	return piece_total == h7;
}

} // namespace

/** Whether CheckPlanarShapePromise runs H14 (on since D403 exempted the legs of one node, E3-S2). */
static constexpr bool H14_IN_PROMISE = true;

/**
 * Whether stretch k of track t is a free stretch (D397, D402): a stretch of a Line track, bounded by crossings, nodes or ports, where the two bounding crossings (if both are crossings) are not both of one structure (or its copy span) and not with tracks of one channel, and the track is in no beside track's parent chain.
 * @param v The variant.
 * @param t The track.
 * @param k The stretch, 0 <= k <= the track's event count (0 from the track's start, the last to its end).
 * @return Whether it is free.
 */
static bool FreeOnePieceStretch(const PlanarShapeVariant &v, uint32_t t, size_t k)
{
	const PlanarTrackMap &map = v.structure.map;
	if (t >= map.tracks.size() || map.tracks[t].kind != PlanarTrack::Kind::Line || k > map.events[t].size()) return false;
	if (k >= 1 && k < map.events[t].size()) {
		const uint32_t c0 = map.events[t][k - 1], c1 = map.events[t][k];
		const auto partner = [&](uint32_t c) { return map.crossings[c].a == t ? map.crossings[c].b : map.crossings[c].a; };
		const Oracle o(v);
		for (const PlanarStructure &s : v.structure.structures) {
			if (std::ranges::find(s.crossings, c0) != s.crossings.end() && std::ranges::find(s.crossings, c1) != s.crossings.end()) return false;
			const std::vector<uint32_t> copy_crossings = o.CopyCrossings(s);
			if (std::ranges::find(copy_crossings, c0) != copy_crossings.end() && std::ranges::find(copy_crossings, c1) != copy_crossings.end()) return false;
		}
		const std::vector<uint32_t> &channel = v.structure.channel_of_line;
		const uint32_t p = map.tracks[partner(c0)].line, q = map.tracks[partner(c1)].line;
		if (p >= channel.size() || q >= channel.size() || channel[p] == channel[q]) return false;
	}
	for (uint32_t b = 0; b < map.tracks.size(); b++) {
		if (map.tracks[b].kind == PlanarTrack::Kind::Line) continue;
		std::vector<uint32_t> chain;
		if (ParentChain(map, v.structure.sites, b, chain) && std::ranges::find(chain, t) != chain.end()) return false;
	}
	return true;
}

bool PlacePlanarShapeFlex(PlanarShapeVariant &v, std::string &why, bool end_pieces)
{
	const PlanarTrackMap &map = v.structure.map;
	if (v.tracks.size() != map.tracks.size()) {
		why = "no flex: the shape's tracks differ from the map's";
		return false;
	}
	for (PlanarShapeTrack &track : v.tracks) track.flex.clear();
	for (uint32_t t = 0; t < map.tracks.size(); t++) {
		PlanarShapeTrack &track = v.tracks[t];
		if (map.tracks[t].kind != PlanarTrack::Kind::Line || track.event_piece.size() != map.events[t].size() || track.pieces.empty()) continue;
		for (size_t k = track.event_piece.size() + 1; k-- > 0;) {
			const size_t events = track.event_piece.size();
			if ((k < events && track.event_piece[k] >= track.pieces.size()) || (k >= 1 && k < events && track.event_piece[k - 1] > track.event_piece[k])) {
				why = fmt::format("no flex: track {} event piece {} is out of range", map.tracks[t].name, k);
				return false;
			}
			/* One piece: the stretch's first and last pieces are the same piece. */
			const size_t first_piece = k == 0 ? 0 : track.event_piece[k - 1];
			const size_t last_piece = k == events ? track.pieces.size() - 1 : track.event_piece[k];
			if (!(first_piece == last_piece || end_pieces) || !FreeOnePieceStretch(v, t, k)) continue;
			/* Insert the pairs after piece p of stretch k. */
			const auto place_at = [&](uint32_t p) {
				const int h = track.pieces[p];
				/* The route cone: the cumulative turning at piece p plus the side, on every path through the track. */
				bool allowed[2] = {true, true};
				for (const PlanarRoute &route : map.expanded.routes) {
					for (const std::vector<uint32_t> &path : route.paths) {
						int cumulative = 0;
						for (size_t n = 0; n < path.size(); n++) {
							const std::vector<int> &pieces = v.tracks[path[n]].pieces;
							if (pieces.empty()) break;
							if (n > 0) cumulative += Norm(pieces.front() - v.tracks[path[n - 1]].pieces.back());
							if (path[n] == t) {
								const int at = cumulative + pieces[p] - pieces.front();
								if (at - 1 < -3) allowed[0] = false;
								if (at + 1 > 3) allowed[1] = false;
							}
							cumulative += pieces.back() - pieces.front();
						}
					}
				}
				std::vector<int> insert;
				if (allowed[0]) insert.insert(insert.end(), {h - 1, h});
				if (allowed[1]) insert.insert(insert.end(), {h + 1, h});
				if (insert.empty()) return;
				const uint32_t count = static_cast<uint32_t>(insert.size());
				track.pieces.insert(track.pieces.begin() + p + 1, insert.begin(), insert.end());
				for (uint32_t &m : track.flex) m += count;
				for (size_t j = 0; j < insert.size(); j += 2) track.flex.push_back(p + 1 + static_cast<uint32_t>(j));
				for (size_t j = k; j < track.event_piece.size(); j++) track.event_piece[j] += count;
			};
			place_at(static_cast<uint32_t>(last_piece));
			if (first_piece != last_piece) place_at(static_cast<uint32_t>(first_piece));
		}
		std::ranges::sort(track.flex);
	}
	return true;
}

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
	o.CheckH12();
	o.CheckH8();
	if constexpr (H14_IN_PROMISE) o.CheckH14();
	h9();
	return o.out;
}

std::vector<std::string> CheckPlanarShapeCells(const PlanarShapeVariant &v)
{
	Oracle o(v);
	o.CheckH12();
	return o.out;
}

std::vector<std::string> CheckPlanarShapeSpines(const PlanarShapeVariant &v)
{
	Oracle o(v);
	o.CheckH14();
	return o.out;
}

bool RefinePlanarShapeCells(PlanarShapeVariant &v, std::string &why, const std::vector<std::vector<std::array<int, 2>>> *landing_from)
{
	v.dummies.clear();
	v.nonmonotone_dummies = 0;
	v.landing_fallbacks = 0;
	std::vector<std::array<double, 2>> dummy_start, dummy_hit; // Per dummy, its start vertex and where its ray first hit in landing_from.
	double box[4] = {0, 0, 0, 0}; // min x, min y, max x, max y of landing_from.
	if (landing_from != nullptr) {
		bool first_point = true;
		for (const auto &line : *landing_from) {
			for (const std::array<int, 2> &pt : line) {
				if (first_point) {
					box[0] = box[2] = pt[0];
					box[1] = box[3] = pt[1];
					first_point = false;
				}
				box[0] = std::min<double>(box[0], pt[0]);
				box[1] = std::min<double>(box[1], pt[1]);
				box[2] = std::max<double>(box[2], pt[0]);
				box[3] = std::max<double>(box[3], pt[1]);
			}
		}
	}
	/* The segment a wall is in landing_from; false for a wall it has no points for. */
	const auto segment = [&](const Wall &x, std::array<double, 2> &a, std::array<double, 2> &b) {
		if (x.track == PLANAR_DUMMY_TRACK) {
			if (x.piece >= dummy_hit.size()) return false;
			a = dummy_start[x.piece];
			b = dummy_hit[x.piece];
			return true;
		}
		if (x.track == UINT32_MAX) {
			switch (x.piece % 4) {
				case 0: a = {box[0], box[1]}; b = {box[2], box[1]}; break;
				case 1: a = {box[2], box[1]}; b = {box[2], box[3]}; break;
				case 2: a = {box[0], box[3]}; b = {box[2], box[3]}; break;
				default: a = {box[0], box[1]}; b = {box[0], box[3]}; break;
			}
			return true;
		}
		if (x.track >= landing_from->size() || x.piece + 1 >= (*landing_from)[x.track].size()) return false;
		const std::array<int, 2> &p0 = (*landing_from)[x.track][x.piece], &p1 = (*landing_from)[x.track][x.piece + 1];
		a = {static_cast<double>(p0[0]), static_cast<double>(p0[1])};
		b = {static_cast<double>(p1[0]), static_cast<double>(p1[1])};
		return true;
	};
	const Oracle o(v);
	const auto text = [&](const Wall &w) {
		static const char *const SIDES[4] = {"N", "E", "S", "W"};
		if (w.track == PLANAR_DUMMY_TRACK) return fmt::format("dummy {}", w.piece);
			if (w.track == UINT32_MAX) return fmt::format("box {}", SIDES[w.piece % 4]);
		return fmt::format("{} piece {}", w.track < o.map.tracks.size() ? o.map.tracks[w.track].name : fmt::format("{}", w.track), w.piece);
	};
	for (const PlanarFace &face : o.map.faces) {
		const FaceWalk walk = WalkFace(o.map, face);
		if (!walk.usable || walk.stretches.empty()) continue;
		std::vector<Wall> first;
		int piece_total, h7;
		if (!CellWalls(o, walk, first, piece_total, h7)) continue;
		if (piece_total != -8) continue; // H7 reports a face that does not wind once.
		if constexpr (FLEX_CELLS_AT_ZERO) first = ReducedWalls(v, first);
		std::vector<std::vector<Wall>> cells = {first};
		for (;;) {
			size_t ci = SIZE_MAX, s = 0;
			for (size_t c = 0; c < cells.size() && ci == SIZE_MAX; c++) {
				for (size_t i = 0; i < cells[c].size(); i++) {
					if (cells[c][i].turn > 0) {
						ci = c;
						s = i;
						break;
					}
				}
			}
			if (ci == SIZE_MAX) break;
			const std::vector<Wall> &w = cells[ci];
			const size_t n = w.size();
			const Wall &in = w[s], &out = w[(s + 1) % n];
			if (in.track >= PLANAR_DUMMY_TRACK || in.track != out.track || (in.piece + 1 != out.piece && out.piece + 1 != in.piece)) {
				why = fmt::format("no cells: face {} reflex corner at {} -> {} is not a bend", face.face, text(in), text(out));
				return false;
			}
			const int h = in.heading;
			bool found = false, wide = false, monotone = false;
			PlanarShapeDummy best{};
			std::vector<std::vector<Wall>> best_cells;
			std::array<double, 2> corner{}, best_hit{};
			if (landing_from != nullptr) {
				const std::array<int, 2> &c = (*landing_from)[in.track][std::max(in.piece, out.piece)];
				corner = {static_cast<double>(c[0]), static_cast<double>(c[1])};
			}
			/* The first wall of the cell, k = 2 .. n - 1, that the ray from the corner along d meets in landing_from. */
			const auto geometric_hit = [&](int d, std::array<double, 2> &hit) {
				const std::array<int, 2> &dir = PlanarModel::STEP[((d % 8) + 8) % 8];
				const double dx = dir[0], dy = dir[1];
				size_t best_k = SIZE_MAX;
				double best_t = 0;
				for (size_t k = 2; k < n; k++) {
					std::array<double, 2> a, b;
					if (!segment(w[(s + k) % n], a, b)) continue;
					const double ex = b[0] - a[0], ey = b[1] - a[1];
					const double denom = dx * ey - dy * ex;
					if (denom == 0) continue;
					const double ax = a[0] - corner[0], ay = a[1] - corner[1];
					const double t = (ax * ey - ay * ex) / denom;
					const double u = (ax * dy - ay * dx) / denom;
					if (t <= 1e-9 || u < -1e-9 || u > 1 + 1e-9) continue;
					if (best_k == SIZE_MAX || t < best_t) {
						best_k = k;
						best_t = t;
					}
				}
				hit = {corner[0] + best_t * dx, corner[1] + best_t * dy};
				if (best_k == SIZE_MAX) hit = corner;
				return best_k;
			};
			bool filter = landing_from != nullptr;
			for (;;) {
				for (const int d : {h, h - 3, h - 1, h - 2}) {
					const Wall *land = nullptr;
					size_t land_k = 0;
					for (size_t k = 2; k < n && land == nullptr; k++) {
						const Wall &x = w[(s + k) % n];
						const int c = Norm(x.heading - d);
						if (c >= -3 && c <= -1) {
							land = &x;
							land_k = k;
						}
					}
					if (land == nullptr) continue;
					std::array<double, 2> hit{};
					if (landing_from != nullptr) {
						const size_t hit_k = geometric_hit(d, hit);
						if (filter) {
							if (hit_k == SIZE_MAX) continue;
							const Wall &g = w[(s + hit_k) % n];
							if (g.track != land->track || g.piece != land->piece) continue;
						}
					}
					/* Monotone (D405): no wall from the corner's outgoing wall to the landing wall moves back along the ray, so the landing is ahead of the corner for every choice of lengths. */
					bool is_monotone = true;
					for (size_t j = 1; j <= land_k; j++) {
						const std::array<int, 2> &a = PlanarModel::STEP[((w[(s + j) % n].heading % 8) + 8) % 8], &b = PlanarModel::STEP[((d % 8) + 8) % 8];
						if (a[0] * b[0] + a[1] * b[1] < 0) is_monotone = false;
					}
					const PlanarShapeDummy dummy{static_cast<uint32_t>(face.face), in.track, std::min(in.piece, out.piece), ((d % 8) + 8) % 8, land->track, land->piece};
					std::vector<std::vector<Wall>> trial = {w};
					std::string reason;
					if (!SplitCell(trial, dummy, static_cast<uint32_t>(v.dummies.size()), reason)) continue;
					/* The corner is the end of the incoming wall in the second cell and the dummy's end in the first. */
					if (trial[0].back().turn > 0 || trial[1][trial[1].size() - 2].turn > 0) continue;
					/* Every corner at a dummy end, the landing included, must be convex: a reflex corner there is no bend, so no later dummy could resolve it (D397). */
					bool reflex = false;
					for (const std::vector<Wall> &cell : trial) {
						for (size_t i = 0; i < cell.size(); i++) {
							const Wall &a = cell[i], &b = cell[(i + 1) % cell.size()];
							const bool at_dummy = a.track == PLANAR_DUMMY_TRACK || b.track == PLANAR_DUMMY_TRACK;
							if (at_dummy && a.turn > 0) reflex = true;
						}
					}
					if (reflex) continue;
					const bool is_wide = trial[0].size() >= 4 && trial[1].size() >= 4;
					if (!found || (is_monotone && !monotone) || (is_monotone == monotone && is_wide && !wide)) {
						found = true;
						monotone = is_monotone;
						wide = is_wide;
						best = dummy;
						best_hit = hit;
						best_cells = std::move(trial);
					}
					if (monotone && wide) break;
				}
				if (found || !filter) break;
				/* No candidate's landing agreed with landing_from: the rule's choice. */
				filter = false;
				v.landing_fallbacks++;
			}
			if (!found) {
				why = fmt::format("no cells: face {} no dummy resolves {} -> {}", face.face, text(in), text(out));
				return false;
			}
			if (!monotone) v.nonmonotone_dummies++;
			v.dummies.push_back(best);
			dummy_start.push_back(corner);
			dummy_hit.push_back(best_hit);
			cells[ci] = std::move(best_cells[0]);
			cells.push_back(std::move(best_cells[1]));
		}
	}
	return true;
}

std::vector<std::vector<PlanarCellWall>> PlanarShapeCellWalls(const PlanarShapeVariant &v)
{
	std::vector<std::vector<PlanarCellWall>> result;
	const Oracle o(v);
	for (const PlanarFace &face : o.map.faces) {
		const FaceWalk walk = WalkFace(o.map, face);
		if (!walk.usable || walk.stretches.empty()) continue;
		std::vector<Wall> first;
		int piece_total, h7;
		if (!CellWalls(o, walk, first, piece_total, h7)) continue;
		if (piece_total != -8) continue;
		std::vector<std::vector<Wall>> cells = {first};
		for (size_t i = 0; i < v.dummies.size(); i++) {
			if (v.dummies[i].face != static_cast<uint32_t>(face.face)) continue;
			std::string reason;
			SplitCell(cells, v.dummies[i], static_cast<uint32_t>(i), reason);
		}
		for (const std::vector<Wall> &cell : cells) {
			std::vector<PlanarCellWall> walls;
			for (const Wall &w : cell) walls.push_back({w.track, w.piece, ((w.heading % 8) + 8) % 8});
			result.push_back(std::move(walls));
		}
	}
	return result;
}

std::vector<uint32_t> PlanarShapeCellFaces(const PlanarShapeVariant &v)
{
	std::vector<uint32_t> result;
	const Oracle o(v);
	for (const PlanarFace &face : o.map.faces) {
		const FaceWalk walk = WalkFace(o.map, face);
		if (!walk.usable || walk.stretches.empty()) continue;
		std::vector<Wall> first;
		int piece_total, h7;
		if (!CellWalls(o, walk, first, piece_total, h7)) continue;
		if (piece_total != -8) continue;
		std::vector<std::vector<Wall>> cells = {first};
		for (size_t i = 0; i < v.dummies.size(); i++) {
			if (v.dummies[i].face != static_cast<uint32_t>(face.face)) continue;
			std::string reason;
			SplitCell(cells, v.dummies[i], static_cast<uint32_t>(i), reason);
		}
		for (size_t i = 0; i < cells.size(); i++) result.push_back(static_cast<uint32_t>(face.face));
	}
	return result;
}

std::vector<PlanarFaceWalls> PlanarShapeFaceWalls(const PlanarShapeVariant &v)
{
	std::vector<PlanarFaceWalls> result;
	const Oracle o(v);
	for (const PlanarFace &face : o.map.faces) {
		const FaceWalk walk = WalkFace(o.map, face);
		if (!walk.usable || walk.stretches.empty()) continue;
		std::vector<Wall> first;
		int piece_total, h7;
		if (!CellWalls(o, walk, first, piece_total, h7)) continue;
		if (piece_total != -8) continue;
		PlanarFaceWalls entry{static_cast<uint32_t>(face.face), {}};
		for (const Wall &w : first) entry.walls.push_back({w.track, w.piece, ((w.heading % 8) + 8) % 8});
		result.push_back(std::move(entry));
	}
	return result;
}

/**
 * Fill a shaped variant's structures from its headings: per structure its axis (its separator's heading at its first crossing), end runs by kind and least middles at its crossings' angles (H5, H6).
 * @param sv The variant, with its tracks set.
 * @param why Receives the reason when a crossing's angle is not in 1..3.
 * @return Whether every structure was filled.
 */
static bool ShapeStructures(PlanarShapeVariant &sv, std::string &why)
{
	const PlanarTrackMap &map = sv.structure.map;
	const Oracle o(sv);
	sv.structures.clear();
	for (const PlanarStructure &s : sv.structure.structures) {
		PlanarShapeStructure shape{0, {-1, -1}, {(s.kinds & 1) != 0 ? 1 : -1, (s.kinds & 2) != 0 ? 2 : -1}};
		if (!s.crossings.empty()) shape.axis = static_cast<uint8_t>((o.EventHeading(s.separator, EventIndex(map.events[s.separator], s.crossings.front())) % 8 + 8) % 8);
		int least;
		if (!o.Least(s.separator, s.crossings, s.crossers, least)) {
			why = "no shape: a crossing's angle is not in 1..3";
			return false;
		}
		for (int kind = 0; kind < 2; kind++) {
			if ((s.kinds & (1 << kind)) != 0) shape.middles[kind] = least;
		}
		if (s.copy != UINT32_MAX) {
			const std::vector<uint32_t> copy_crossings = o.CopyCrossings(s);
			std::vector<uint32_t> copy_crossers;
			for (const uint32_t c : copy_crossings) copy_crossers.push_back(map.crossings[c].a == s.copy ? map.crossings[c].b : map.crossings[c].a);
			if (!o.Least(s.copy, copy_crossings, copy_crossers, least)) {
				why = "no shape: a copy crossing's angle is not in 1..3";
				return false;
			}
			for (int kind = 0; kind < 2; kind++) {
				if ((s.kinds & (1 << kind)) != 0) shape.copy_middles[kind] = least;
			}
		}
		sv.structures.push_back(shape);
	}
	return true;
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

	/* Nodes: the turn (d = 1 at a topology node, else a binary d) and sigma of each non-straight leg, at the split at its start and at the merge at its end. */
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
			/* A topology node turns its leg always (d = 1, no cusps, D404); a copy's or row's node keeps the binary d (its beside track sits on its parent's headings, H8). */
			bool topology = true;
			for (const std::vector<uint32_t> *side : {&node.in, &node.out}) {
				for (const uint32_t t : *side) topology = topology && map.tracks[t].kind == PlanarTrack::Kind::Line;
			}
			const uint32_t d = topology ? NONE : var(0, 1, 0, true);
			if (site.split) {
				split_leg[leg] = {d, sigma};
				if (d == NONE) {
					eq({{H[leg].front(), 1}, {H[one].back(), -1}}, sigma, tag);
				} else {
					eq({{H[leg].front(), 1}, {H[one].back(), -1}, {d, -sigma}}, 0, tag);
				}
			} else {
				merge_leg[leg] = {d, sigma};
				if (d == NONE) {
					eq({{H[leg].back(), 1}, {H[one].front(), -1}, {var(-8, 8, 0, true), -8}}, -sigma, tag);
				} else {
					eq({{H[leg].back(), 1}, {d, sigma}, {H[one].front(), -1}, {var(-8, 8, 0, true), -8}}, 0, tag);
				}
			}
		}
	}

	/* Crossings: the rule for the separator and, in a stack, for the other separator too. A copy-deck crossing has its own angle and the crossing row against the copy (D376); any other mirrored crossing of a beside track has no row of its own: it takes its original's angle and keeps its crosser on one piece across both. */
	std::vector<uint32_t> angle(map.crossings.size(), NONE);
	for (uint32_t c = 0; c < map.crossings.size(); c++) {
		const PlanarStructure *first = o.StructureOf(c, nullptr);
		const PlanarStructure *copy_deck = first == nullptr ? o.CopyDeckOf(c) : nullptr;
		if (first == nullptr && copy_deck == nullptr) continue;
		const PlanarStructure *second = copy_deck != nullptr ? nullptr : o.StructureOf(c, first);
		angle[c] = var(1, 3, 0, true);
		for (const PlanarStructure *s : {first, second, copy_deck}) {
			if (s == nullptr) continue;
			const uint32_t p = s == copy_deck ? s->copy : s->separator;
			const uint32_t q = map.crossings[c].a == p ? map.crossings[c].b : map.crossings[c].a;
			int side;
			if (!o.Arrival(c, p, q, side)) return fail(fmt::format("crossing {} has no arrival side", c));
			const uint32_t hp = H[p][EventIndex(map.events[p], c) + 1], hq = H[q][EventIndex(map.events[q], c) + 1];
			eq({{hq, 1}, {hp, -1}, {angle[c], -side}, {var(-8, 8, 0, true), -8}}, 0, fmt::format("cross {}", c));
			if (s != copy_deck) eq({{hp, 1}, {var(-12, 16, 0, true), -2}}, 0, fmt::format("axis {}", c));
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

	/* Capacity, per span (the separator's deck; the copy's deck when there is a copy): runs per D396, least = n + diagonal runs. Per crossing a binary d >= |a_k - 2|; per neighbour pair a binary p, which forces the two headings equal mod 4 when 1, and e_k >= d_k - p_k, so p_k = 1 only for parallel neighbours and the count is never below the true one. */
	for (size_t i = 0; i < sv.structure.structures.size(); i++) {
		const PlanarStructure &s = sv.structure.structures[i];
		const std::vector<uint32_t> copy_crossings = o.CopyCrossings(s);
		std::vector<uint32_t> copy_crossers;
		for (const uint32_t c : copy_crossings) copy_crossers.push_back(map.crossings[c].a == s.copy ? map.crossings[c].b : map.crossings[c].a);
		for (const bool copy : {false, true}) {
			const std::vector<uint32_t> &crossings = copy ? copy_crossings : s.crossings, &crossers = copy ? copy_crossers : s.crossers;
			const size_t n = crossings.size();
			if (n == 0) continue;
			std::vector<size_t> at(n);
			for (size_t k = 0; k < n; k++) {
				at[k] = EventIndex(map.events[crossers[k]], crossings[k]);
				if (at[k] == SIZE_MAX) return fail(fmt::format("structure {} lists a crossing its crosser lacks", i));
			}
			if (s.limit[0] < 0 && s.limit[1] < 0) continue;
			const std::string cap = fmt::format("cap {}", i), run = fmt::format("run {}", i);
			constexpr double M = 64;
			std::vector<std::pair<uint32_t, double>> total;
			for (size_t k = 0; k < n; k++) {
				const uint32_t d = var(0, 1, 0, true);
				row({{angle[crossings[k]], 1}, {d, -1}}, -JunctionMilp::INF, 2, cap);
				row({{angle[crossings[k]], -1}, {d, -1}}, -JunctionMilp::INF, -2, cap);
				if (k == 0) {
					total.push_back({d, 1});
					continue;
				}
				const uint32_t p = var(0, 1, 0, true), q = var(-16, 16, 0, true), e = var(0, 1, 0, false);
				const uint32_t hk = H[crossers[k]][at[k] + 1], hprev = H[crossers[k - 1]][at[k - 1] + 1];
				row({{hk, 1}, {hprev, -1}, {q, -4}, {p, M}}, -JunctionMilp::INF, M, run);
				row({{hk, 1}, {hprev, -1}, {q, -4}, {p, -M}}, -M, JunctionMilp::INF, run);
				row({{d, 1}, {p, -1}, {e, -1}}, -JunctionMilp::INF, 0, cap);
				total.push_back({e, 1});
			}
			for (int kind = 0; kind < 2; kind++) {
				if (s.limit[kind] >= 0) row(total, -JunctionMilp::INF, s.limit[kind] - static_cast<double>(n), cap);
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

	/* Spines (H14): a channel's whole lines that cross the same channels in order have equal headings at every channel crossing. */
	for (const auto &[c, first, other, first_events, other_events, same] : SpinePairs(map, sv.structure)) {
		if (!same) continue;
		for (size_t k = 0; k < first_events.size(); k++) eq({{H[first][first_events[k] + 1], 1}, {H[other][other_events[k] + 1], -1}}, 0, fmt::format("spine {}", c));
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
		const auto walk = [&](const auto &self, uint32_t t, Terms prefix, int turn, size_t depth) -> void {
			if (depth > nt) {
				broken = true;
				return;
			}
			for (size_t i = 0; i < H[t].size(); i++) {
				if (prefix.empty() && i == 0) continue;
				Terms cum = prefix;
				cum.emplace_back(H[t][i], 1);
				cum.emplace_back(H[t].front(), -1);
				row(cum, -3 - turn, 3 - turn, tag);
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
				self(self, g, std::move(through), d == NONE ? turn + sigma : turn, depth + 1);
			}
		};
		for (uint32_t t = 0; t < nt; t++) {
			if (map.expanded.lines[t].from.port && carries(t)) walk(walk, t, {}, 0, 0);
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
	if (!ShapeStructures(sv, why)) return false;
	out = std::move(sv);
	if (!PlacePlanarShapeFlex(out, why)) return false;
	if (!RefinePlanarShapeCells(out, why)) return false;
	return true;
}

bool LoadPlanarShapeVariant(const PlanarTopology &topology, const PlanarStructureVariant &v, std::string_view text, PlanarShapeVariant &out, std::string &why)
{
	const PlanarTrackMap &map = v.map;
	if (map.tracks.size() != topology.lines.size()) {
		why = "the map has tracks beside its lines (copies, rows or parts)";
		return false;
	}
	std::map<std::string, std::vector<int>, std::less<>> headings;
	std::map<std::string, std::map<std::string, uint32_t, std::less<>>, std::less<>> at;
	while (!text.empty()) {
		const size_t nl = text.find('\n');
		std::string_view line = text.substr(0, nl);
		text.remove_prefix(nl == std::string_view::npos ? text.size() : nl + 1);
		const bool shape = line.starts_with("shape "), place = line.starts_with("at ");
		if (!shape && !place) continue;
		const size_t colon = line.find(':');
		if (colon == std::string_view::npos) {
			why = fmt::format("no ':' in '{}'", line);
			return false;
		}
		const size_t start = shape ? 6 : 3;
		const std::string name(line.substr(start, colon - start));
		if (PlanarLineByName(topology, name) == UINT32_MAX) {
			why = fmt::format("unknown line {}", name);
			return false;
		}
		std::string_view rest = line.substr(colon + 1);
		while (!rest.empty()) {
			const size_t sep = rest.find(shape ? ' ' : ',');
			std::string_view item = rest.substr(0, sep);
			rest.remove_prefix(sep == std::string_view::npos ? rest.size() : sep + 1);
			while (!item.empty() && item.front() == ' ') item.remove_prefix(1);
			while (!item.empty() && item.back() == ' ') item.remove_suffix(1);
			if (item.empty()) continue;
			if (shape) {
				int h = 0;
				std::from_chars(item.data(), item.data() + item.size(), h);
				headings[name].push_back(h);
			} else {
				const size_t atpos = item.find('@');
				uint32_t piece = 0;
				if (atpos == std::string_view::npos || std::from_chars(item.data() + atpos + 1, item.data() + item.size(), piece).ec != std::errc{}) {
					why = fmt::format("bad crossing piece '{}' on {}", item, name);
					return false;
				}
				at[name][std::string(item.substr(0, atpos))] = piece;
			}
		}
	}
	PlanarShapeVariant sv;
	sv.structure = v;
	sv.cost = 0;
	sv.tracks.resize(map.tracks.size());
	/* Lift along each route from the ports: a line from a port keeps its first heading in 0..7; a line from a node starts nearest its node's first in line's last heading. A line without headings (a zero-length line) is one piece at that heading. */
	std::vector<bool> done(topology.lines.size(), false);
	for (size_t pass = 0; pass <= topology.lines.size(); pass++) {
		for (uint32_t l = 0; l < topology.lines.size(); l++) {
			if (done[l]) continue;
			const PlanarLine &line = topology.lines[l];
			std::optional<int> base;
			if (!line.from.port) {
				const uint32_t in = topology.nodes[line.from.index].in[0];
				if (!done[in]) continue;
				base = sv.tracks[in].pieces.back();
			}
			const auto it = headings.find(line.name);
			std::vector<int> h = it != headings.end() ? it->second : std::vector<int>{};
			if (h.empty()) {
				if (!base.has_value()) {
					why = fmt::format("port line {} has no headings", line.name);
					return false;
				}
				h = {*base};
			}
			int first = (h[0] % 8 + 8) % 8;
			if (base.has_value()) {
				while (first - *base > 4) first -= 8;
				while (*base - first > 4) first += 8;
			}
			std::vector<int> &pieces = sv.tracks[l].pieces;
			pieces.push_back(first);
			for (size_t i = 1; i < h.size(); i++) {
				int d = ((h[i] - h[i - 1]) % 8 + 8) % 8;
				if (d > 4) d -= 8;
				pieces.push_back(pieces.back() + d);
				sv.cost += std::abs(d);
			}
			done[l] = true;
		}
	}
	for (uint32_t t = 0; t < map.tracks.size(); t++) {
		if (!done[t]) {
			why = fmt::format("line {} is reached from no port", map.tracks[t].name);
			return false;
		}
		for (const uint32_t c : map.events[t]) {
			const uint32_t other = map.crossings[c].a == t ? map.crossings[c].b : map.crossings[c].a;
			const auto line_at = at.find(map.tracks[t].name);
			const auto it = line_at == at.end() ? decltype(line_at->second.end()){} : line_at->second.find(map.tracks[other].name);
			if (line_at == at.end() || it == line_at->second.end()) {
				why = fmt::format("no piece for crossing {} x {}", map.tracks[t].name, map.tracks[other].name);
				return false;
			}
			if (it->second >= sv.tracks[t].pieces.size()) {
				why = fmt::format("crossing {} x {} on piece {} of {}", map.tracks[t].name, map.tracks[other].name, it->second, sv.tracks[t].pieces.size());
				return false;
			}
			sv.tracks[t].event_piece.push_back(it->second);
		}
	}
	/* Angles from the headings, as the oracle derives them, each at a structure's separator. */
	sv.angles.assign(map.crossings.size(), 0);
	{
		const Oracle o(sv);
		for (const PlanarStructure &s : v.structures) {
			for (size_t k = 0; k < s.crossings.size(); k++) {
				int a;
				if (o.Angle(s.crossings[k], s.separator, s.crossers[k], a) && a >= 1 && a <= 3) sv.angles[s.crossings[k]] = static_cast<uint8_t>(a);
			}
		}
	}
	if (!ShapeStructures(sv, why)) return false;
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
