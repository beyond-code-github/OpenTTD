/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file planar_domain.cpp The domain predicate: the game's tile rules, shared with no solver (D428). */

#include "../../stdafx.h"

#include "planar_domain.h"

#include "../junction_track_geometry.h"

#include "../../core/format.hpp"
#include "../../direction_func.h"
#include "../../track_func.h"

#include <algorithm>
#include <cstdlib>
#include <map>
#include <set>

#include "../../safeguards.h"

namespace {

using Point = std::array<int, 2>;

const int HEADING_DX[8] = {0, 1, 1, 1, 0, -1, -1, -1};
const int HEADING_DY[8] = {-1, -1, 0, 1, 1, 1, 0, -1};

/** floor(v / 4). */
int FloorQuarter(int v)
{
	return v >= 0 ? v / 4 : -((-v + 3) / 4);
}

/** floor(v / 2). */
int FloorHalf(int v)
{
	return v >= 0 ? v / 2 : -((-v + 1) / 2);
}

bool IsMidpoint(const Point &p)
{
	return ((p[0] + p[1]) & 1) == 1;
}

/** The edge of a tile that the point is the midpoint of, if any. */
bool EdgeOf(const Point &p, TileIndexDiffC tile, DiagDirection &edge)
{
	const int x0 = 2 * tile.x, y0 = 2 * tile.y;
	if (p[0] == x0 && p[1] == y0 + 1) edge = DiagDirection::NE;
	else if (p[0] == x0 + 2 && p[1] == y0 + 1) edge = DiagDirection::SW;
	else if (p[0] == x0 + 1 && p[1] == y0) edge = DiagDirection::NW;
	else if (p[0] == x0 + 1 && p[1] == y0 + 2) edge = DiagDirection::SE;
	else return false;
	return true;
}

int HeadingOf(int dx, int dy)
{
	for (int h = 0; h < 8; h++) {
		if (HEADING_DX[h] == (dx > 0) - (dx < 0) && HEADING_DY[h] == (dy > 0) - (dy < 0)) return h;
	}
	return 0;
}

/** Whether two headings are within one eighth of each other. */
bool HeadingsClose(int a, int b)
{
	const int d = (a - b + 8) % 8;
	return d == 0 || d == 1 || d == 7;
}

bool SameTile(TileIndexDiffC a, TileIndexDiffC b)
{
	return a.x == b.x && a.y == b.y;
}

} // namespace

PlanarDomainRaster RasterPlanarDomainTrack(const std::vector<std::array<int, 2>> &points)
{
	PlanarDomainRaster out;
	const auto fail = [&](const char *rule, const std::string &why) {
		out.rule = rule;
		out.why = why;
		return out;
	};

	std::vector<Point> pts;
	for (const Point &p : points) {
		if (pts.empty() || pts.back() != p) pts.push_back(p);
	}
	if (pts.empty()) return out;
	if (!IsMidpoint(pts[0])) return fail("grid", fmt::format("the first point ({}, {}) is not an edge midpoint", pts[0][0], pts[0][1]));

	Point at = pts[0];
	Point start = at;
	int steps = 0;
	int pending = -1;
	for (size_t s = 0; s + 1 < pts.size(); s++) {
		const int dx = pts[s + 1][0] - at[0];
		const int dy = pts[s + 1][1] - at[1];
		if (dx != 0 && dy != 0 && std::abs(dx) != std::abs(dy)) return fail("grid", fmt::format("the segment to ({}, {}) is not axis or diagonal", pts[s + 1][0], pts[s + 1][1]));
		const int heading = HeadingOf(dx, dy);
		if (steps > 0 && pending != heading) return fail("grid", fmt::format("the heading changes at ({}, {}), between edge midpoints", at[0], at[1]));
		const int length = std::max(std::abs(dx), std::abs(dy));
		for (int i = 0; i < length; i++) {
			at = {at[0] + HEADING_DX[heading], at[1] + HEADING_DY[heading]};
			steps++;
			pending = heading;
			if (!IsMidpoint(at)) continue;

			PlanarDomainPiece piece;
			piece.from = start;
			piece.to = at;
			piece.heading = heading;
			piece.tile = {static_cast<int16_t>(FloorQuarter(start[0] + at[0])), static_cast<int16_t>(FloorQuarter(start[1] + at[1]))};
			DiagDirection a, b;
			if (!EdgeOf(start, piece.tile, a) || !EdgeOf(at, piece.tile, b) || a == b) return fail("grid", fmt::format("the piece to ({}, {}) does not join two edges of one tile", at[0], at[1]));
			if (heading % 2 == 0) {
				if (b != ReverseDiagDir(a)) return fail("grid", fmt::format("the axis piece to ({}, {}) does not join opposite edges", at[0], at[1]));
				piece.track = (a == DiagDirection::NE || a == DiagDirection::SW) ? Track::X : Track::Y;
			} else {
				if (b == ReverseDiagDir(a)) return fail("grid", fmt::format("the diagonal piece to ({}, {}) joins opposite edges", at[0], at[1]));
				piece.track = JunctionCornerTrack(a, b);
			}
			if (!out.pieces.empty()) {
				const PlanarDomainPiece &prev = out.pieces.back();
				if (!HeadingsClose(prev.heading, heading)) return fail("turn", fmt::format("the heading turns more than one eighth at ({}, {})", start[0], start[1]));
				if (SameTile(prev.tile, piece.tile)) return fail("turn", fmt::format("two consecutive pieces lie on tile ({}, {})", piece.tile.x, piece.tile.y));
			}
			out.pieces.push_back(piece);
			start = at;
			steps = 0;
			pending = -1;
		}
	}
	if (steps > 0) return fail("grid", fmt::format("the last point ({}, {}) is not an edge midpoint", at[0], at[1]));
	return out;
}

namespace {

enum class Kind : uint8_t { Ground, Head, Middle };

/** One piece of one track. */
struct PieceRef {
	uint32_t track;
	uint32_t index;
};

/** One end of a ground or head piece at an edge midpoint. */
struct End {
	uint32_t track;
	uint32_t index;
	bool at_from; ///< The piece's `from` end; else its `to` end.
};

/** The collected breaks, one per (rule, tracks, tile). */
struct Breaks {
	std::vector<PlanarDomainBreak> list;

	void Add(const char *rule, std::vector<uint32_t> tracks, TileIndexDiffC tile, const std::string &why)
	{
		std::sort(tracks.begin(), tracks.end());
		tracks.erase(std::unique(tracks.begin(), tracks.end()), tracks.end());
		for (const PlanarDomainBreak &b : list) {
			if (b.rule == rule && b.tracks == tracks && SameTile(b.tile, tile)) return;
		}
		list.push_back({rule, tracks, tile, why});
	}
};

TileIndexDiffC TileOfPoint(const Point &p)
{
	return {static_cast<int16_t>(FloorHalf(p[0])), static_cast<int16_t>(FloorHalf(p[1]))};
}

bool Contains(const std::vector<uint32_t> &list, uint32_t value)
{
	return std::find(list.begin(), list.end(), value) != list.end();
}

} // namespace

PlanarDomainVerdict CheckPlanarDomain(const PlanarDomainInput &input)
{
	Breaks breaks;
	const uint32_t track_count = static_cast<uint32_t>(input.tracks.size());

	/* 1. The tracks. */
	std::vector<PlanarDomainRaster> rasters(track_count);
	std::vector<bool> valid(track_count, false);
	for (uint32_t t = 0; t < track_count; t++) {
		rasters[t] = RasterPlanarDomainTrack(input.tracks[t]);
		valid[t] = rasters[t].rule.empty();
		if (!valid[t]) {
			const TileIndexDiffC tile = rasters[t].pieces.empty() ? TileIndexDiffC{0, 0} : rasters[t].pieces.back().tile;
			breaks.Add(rasters[t].rule.c_str(), {t}, tile, rasters[t].why);
		}
	}
	const auto usable = [&](uint32_t t) { return t < track_count && valid[t]; };

	/* 2. The structures: span, and which pieces are heads and middles. */
	std::vector<std::vector<Kind>> kinds(track_count);
	for (uint32_t t = 0; t < track_count; t++) kinds[t].assign(rasters[t].pieces.size(), Kind::Ground);
	std::vector<const PlanarDomainStructure *> structures;
	for (const PlanarDomainStructure &st : input.structures) {
		if (!usable(st.separator)) continue;
		const std::vector<PlanarDomainPiece> &pieces = rasters[st.separator].pieces;
		bool ok = st.first < st.last && st.last < pieces.size() && st.last - st.first >= 2;
		if (ok) {
			for (uint32_t i = st.first; i <= st.last; i++) ok = ok && pieces[i].heading == pieces[st.first].heading;
			ok = ok && pieces[st.first].heading % 2 == 0;
		}
		if (ok && st.limit >= 0 && static_cast<int>(st.last - st.first) - 1 > st.limit) ok = false;
		if (!ok) {
			const TileIndexDiffC tile = st.first < pieces.size() ? pieces[st.first].tile : TileIndexDiffC{0, 0};
			breaks.Add("span", {st.separator}, tile, fmt::format("the structure on pieces {}..{} is not a straight run of 1 to {} middles", st.first, st.last, st.limit));
			continue;
		}
		structures.push_back(&st);
		for (uint32_t i = st.first + 1; i < st.last; i++) {
			if (kinds[st.separator][i] == Kind::Ground) kinds[st.separator][i] = Kind::Middle;
		}
		kinds[st.separator][st.first] = Kind::Head;
		kinds[st.separator][st.last] = Kind::Head;
	}

	/* The pieces by tile, and each middle's structure. */
	std::map<std::pair<int, int>, std::vector<PieceRef>> by_tile;
	for (uint32_t t = 0; t < track_count; t++) {
		if (!valid[t]) continue;
		for (uint32_t i = 0; i < rasters[t].pieces.size(); i++) {
			by_tile[{rasters[t].pieces[i].tile.x, rasters[t].pieces[i].tile.y}].push_back({t, i});
		}
	}
	const auto kind_of = [&](const PieceRef &r) { return kinds[r.track][r.index]; };
	const auto piece_of = [&](const PieceRef &r) -> const PlanarDomainPiece & { return rasters[r.track].pieces[r.index]; };
	/* The structure whose middle is the piece, or nullptr. */
	const auto middle_of = [&](const PieceRef &r) -> const PlanarDomainStructure * {
		for (const PlanarDomainStructure *st : structures) {
			if (st->separator == r.track && r.index > st->first && r.index < st->last) return st;
		}
		return nullptr;
	};

	/* 3. Connections and nodes. */
	std::map<Point, std::vector<End>> ends;
	for (uint32_t t = 0; t < track_count; t++) {
		if (!valid[t]) continue;
		for (uint32_t i = 0; i < rasters[t].pieces.size(); i++) {
			if (kinds[t][i] == Kind::Middle) continue;
			ends[rasters[t].pieces[i].from].push_back({t, i, true});
			ends[rasters[t].pieces[i].to].push_back({t, i, false});
		}
	}
	std::set<Point> node_points;
	for (const PlanarDomainNode &node : input.nodes) {
		node_points.insert(node.at);
		const auto tracks_at = [&](const Point &p) {
			std::vector<uint32_t> tracks;
			const auto it = ends.find(p);
			if (it != ends.end()) {
				for (const End &e : it->second) tracks.push_back(e.track);
			}
			return tracks;
		};
		const auto it = ends.find(node.at);
		if (it != ends.end()) {
			for (const End &e : it->second) {
				const bool listed_in = Contains(node.in, e.track) && !e.at_from && e.index + 1 == rasters[e.track].pieces.size();
				const bool listed_out = Contains(node.out, e.track) && e.at_from && e.index == 0;
				if (listed_in || listed_out) continue;
				breaks.Add("connection", tracks_at(node.at), TileOfPoint(node.at), fmt::format("a track ends at the node ({}, {}) without being listed there", node.at[0], node.at[1]));
				break;
			}
		}
		for (uint32_t t : node.in) {
			if (usable(t) && !rasters[t].pieces.empty() && rasters[t].pieces.back().to == node.at) continue;
			if (t < track_count) breaks.Add("node", {t}, TileOfPoint(node.at), fmt::format("track {} is listed in `in` but does not end at ({}, {})", t, node.at[0], node.at[1]));
		}
		for (uint32_t t : node.out) {
			if (usable(t) && !rasters[t].pieces.empty() && rasters[t].pieces.front().from == node.at) continue;
			if (t < track_count) breaks.Add("node", {t}, TileOfPoint(node.at), fmt::format("track {} is listed in `out` but does not start at ({}, {})", t, node.at[0], node.at[1]));
		}
	}
	for (const auto &[point, list] : ends) {
		if (node_points.contains(point) || list.size() < 2) continue;
		bool through = list.size() == 2 && list[0].track == list[1].track;
		if (through) {
			const End &lo = list[0].index < list[1].index ? list[0] : list[1];
			const End &hi = list[0].index < list[1].index ? list[1] : list[0];
			through = hi.index == lo.index + 1 && !lo.at_from && hi.at_from;
		}
		if (through) continue;
		std::vector<uint32_t> tracks;
		for (const End &e : list) tracks.push_back(e.track);
		breaks.Add("connection", tracks, TileOfPoint(point), fmt::format("{} track ends meet at ({}, {}) with no node", list.size(), point[0], point[1]));
	}

	/* 4. The node's turns and its switch. */
	for (const PlanarDomainNode &node : input.nodes) {
		std::vector<PieceRef> in_pieces, out_pieces;
		for (uint32_t t : node.in) {
			if (usable(t) && !rasters[t].pieces.empty() && rasters[t].pieces.back().to == node.at) in_pieces.push_back({t, static_cast<uint32_t>(rasters[t].pieces.size() - 1)});
		}
		for (uint32_t t : node.out) {
			if (usable(t) && !rasters[t].pieces.empty() && rasters[t].pieces.front().from == node.at) out_pieces.push_back({t, 0});
		}
		for (const PieceRef &a : in_pieces) {
			for (const PieceRef &b : out_pieces) {
				if (HeadingsClose(piece_of(a).heading, piece_of(b).heading) && !SameTile(piece_of(a).tile, piece_of(b).tile)) continue;
				breaks.Add("turn", {a.track, b.track}, piece_of(b).tile, fmt::format("tracks {} and {} do not continue each other at ({}, {})", a.track, b.track, node.at[0], node.at[1]));
			}
		}
		for (const std::vector<PieceRef> *side : {&in_pieces, &out_pieces}) {
			if (side->size() < 2) continue;
			bool one_tile = true;
			std::vector<uint32_t> tracks;
			for (const PieceRef &r : *side) {
				one_tile = one_tile && SameTile(piece_of(r).tile, piece_of(side->front()).tile);
				tracks.push_back(r.track);
			}
			if (!one_tile) breaks.Add("node", tracks, piece_of(side->front()).tile, fmt::format("the pieces on one side of the node ({}, {}) do not share a tile", node.at[0], node.at[1]));
		}
	}

	/* 5. Tiles. */
	for (const auto &[key, list] : by_tile) {
		const TileIndexDiffC tile{static_cast<int16_t>(key.first), static_cast<int16_t>(key.second)};
		std::vector<uint32_t> tracks_here;
		for (const PieceRef &r : list) tracks_here.push_back(r.track);
		for (size_t a = 0; a < list.size(); a++) {
			const PieceRef &p = list[a];
			if (kind_of(p) == Kind::Head && list.size() > 1) breaks.Add("head", tracks_here, tile, fmt::format("a head on tile ({}, {}) shares it with another piece", tile.x, tile.y));
			const PlanarDomainStructure *deck = middle_of(p);
			for (size_t b = a + 1; b < list.size(); b++) {
				const PieceRef &q = list[b];
				if (kind_of(p) == Kind::Ground && kind_of(q) == Kind::Ground && (p.track != q.track || q.index > p.index + 1)) {
					const Track tp = piece_of(p).track, tq = piece_of(q).track;
					if ((tp == Track::X && tq == Track::Y) || (tp == Track::Y && tq == Track::X)) breaks.Add("diamond", {p.track, q.track}, tile, fmt::format("an X and a Y piece cross at grade on tile ({}, {})", tile.x, tile.y));
				}
				const PlanarDomainStructure *other = middle_of(q);
				if (deck != nullptr && other != nullptr && deck != other && !deck->tunnel && !other->tunnel) breaks.Add("decks", {p.track, q.track}, tile, fmt::format("two bridge decks share tile ({}, {})", tile.x, tile.y));
			}
			if (kind_of(p) != Kind::Ground) continue;
			for (const PieceRef &q : list) {
				const PlanarDomainStructure *st = middle_of(q);
				if (st != nullptr && !Contains(st->crossers, p.track)) breaks.Add("under", {p.track, st->separator}, tile, fmt::format("track {} passes under the structure on track {} at tile ({}, {}) without being listed", p.track, st->separator, tile.x, tile.y));
			}
		}
	}

	/* 6. Missing crossings. */
	for (const PlanarDomainStructure *st : structures) {
		for (uint32_t c : st->crossers) {
			if (!usable(c)) continue;
			bool crosses = false;
			for (uint32_t i = 0; i < rasters[c].pieces.size() && !crosses; i++) {
				if (kinds[c][i] != Kind::Ground) continue;
				for (uint32_t m = st->first + 1; m < st->last && !crosses; m++) {
					crosses = SameTile(rasters[c].pieces[i].tile, rasters[st->separator].pieces[m].tile);
				}
			}
			if (!crosses) breaks.Add("missing crossing", {st->separator, c}, rasters[st->separator].pieces[st->first + 1].tile, fmt::format("track {} is listed under the structure on track {} but never passes under a middle", c, st->separator));
		}
	}

	PlanarDomainVerdict verdict;
	verdict.breaks = std::move(breaks.list);
	verdict.legal = verdict.breaks.empty();
	return verdict;
}
