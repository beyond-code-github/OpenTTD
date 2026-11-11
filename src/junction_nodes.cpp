/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_nodes.cpp The node model's position template and its render implementation. */

#include "stdafx.h"

#include "junction_nodes.h"

#include "direction_func.h"
#include "junction_chain.h"
#include "junction_rules_internal.h"
#include "junction_signal_pass.h"
#include "map_func.h"
#include "track_func.h"

#include <algorithm>
#include <map>
#include <set>
#include <tuple>

#include "safeguards.h"

/** A coordinate (x or y) in the flat layout and the variable that moves it: pos' = value(var) + pos - flat[var]. */
struct NodeCoord {
	int pos;
	uint16_t var;
};

/** What the builder keeps per path beyond the template's JunctionNodePath. */
struct NodePathWork {
	std::vector<uint16_t> start; ///< Per piece, the unit at which it starts.
	std::vector<SwitchEvent> events;
	std::vector<uint32_t> governing; ///< Per piece, the latest point at or before it.
	std::vector<int> link_of; ///< Per piece, the index into `spans` when the piece is strictly inside a free link, else -1.
};

/** A free link of one path: the points and the paths' pieces between. */
struct NodeSpan {
	uint32_t from;
	uint32_t to;
	Axis axis;
	uint32_t diagonal; ///< The index into JunctionNodeTemplate::diagonals when the link is a free diagonal, else UINT32_MAX.
};

/** Reads the template of a flat layout. */
struct NodeBuilder {
	JunctionNodeTemplate &tpl;
	const JunctionSpec &spec;
	const JunctionCheckContext &ctx;
	std::vector<NodePathWork> work;
	std::vector<NodeSpan> spans;
	std::set<std::tuple<uint16_t, uint16_t, int>> seen_diffs;

	/** The point's x with its X class. */
	NodeCoord AtX(uint32_t point) const { return {tpl.points[point].tile.x, tpl.points[point].cls}; }

	/** The point's y with its Y class. */
	NodeCoord AtY(uint32_t point) const { return {tpl.points[point].tile.y, tpl.points[point].ycls}; }

	/** The point's coordinate along an axis with its variable. */
	NodeCoord At(uint32_t point, Axis axis) const { return axis == Axis::X ? this->AtX(point) : this->AtY(point); }

	/** Fail with the first error only. */
	void Fail(const std::string &message)
	{
		if (this->tpl.error.empty()) this->tpl.error = message;
	}

	/** a - b >= k as a diff over variables; false when both are one variable and the constant does not hold. */
	bool Add(NodeCoord a, NodeCoord b, int k)
	{
		const int off_a = a.pos - this->tpl.flat[a.var];
		const int off_b = b.pos - this->tpl.flat[b.var];
		if (a.var == b.var) return off_a - off_b >= k;
		const int rhs = k - off_a + off_b;
		if (this->seen_diffs.emplace(a.var, b.var, rhs).second) this->tpl.diffs.push_back({a.var, b.var, rhs});
		return true;
	}

	/** d * (b - a) >= k. */
	bool AddDirected(int d, NodeCoord a, NodeCoord b, int k)
	{
		return d > 0 ? this->Add(b, a, k) : this->Add(a, b, k);
	}

	/** The points, links, classes and pieces. */
	void ReadPoints();
	/** DESIGN-N3 §2 rules 1-3, per path. */
	void AddPathDiffs();
	/** DESIGN-N3 §2 rule 4, for bridges on either axis (DESIGN-N3 §5). */
	void AddCrossings();
	/** Rule 6: the path end points at the extreme flat x share their x, and the same for y. */
	void AddBoxEdges();
};

/** The least integer not below n / 2. */
static int CeilHalf(int n)
{
	return n >= 0 ? (n + 1) / 2 : -((-n) / 2);
}

/** Whether a path is a main line, as CheckPrio reads it. */
static bool IsMainLinePath(const JunctionPath &path)
{
	return (path.from == JunctionPortRole::EIn && path.to == JunctionPortRole::EOut) || (path.from == JunctionPortRole::WIn && path.to == JunctionPortRole::WOut);
}

/** Union-find over point indices. */
struct NodeUnion {
	std::vector<uint32_t> parent;

	explicit NodeUnion(size_t n) : parent(n)
	{
		for (size_t i = 0; i < n; i++) this->parent[i] = static_cast<uint32_t>(i);
	}

	uint32_t Find(uint32_t i)
	{
		while (this->parent[i] != i) {
			this->parent[i] = this->parent[this->parent[i]];
			i = this->parent[i];
		}
		return i;
	}

	void Join(uint32_t a, uint32_t b) { this->parent[this->Find(a)] = this->Find(b); }
};

void NodeBuilder::ReadPoints()
{
	const JunctionLayout &layout = this->ctx.layout;
	const auto used = UsedTracks(this->ctx);
	const size_t path_count = this->ctx.paths.size();

	this->work.resize(path_count);
	for (size_t pi = 0; pi < path_count; pi++) {
		const std::vector<JunctionPiece> &pieces = this->ctx.paths[pi].pieces;
		NodePathWork &w = this->work[pi];
		w.events = SwitchEventsOf(layout, used, this->ctx.paths[pi]);
		w.start.resize(pieces.size());
		uint16_t unit = 0;
		for (size_t i = 0; i < pieces.size(); i++) {
			w.start[i] = unit;
			unit += pieces[i].units;
		}
	}

	/* A piece that is a point on one path is a point wherever another path passes it. */
	using PieceId = std::tuple<int, int, Trackdir>;
	std::set<PieceId> point_ids;
	for (size_t pi = 0; pi < path_count; pi++) {
		const std::vector<JunctionPiece> &pieces = this->ctx.paths[pi].pieces;
		std::vector<bool> is_point(pieces.size(), false);
		for (size_t i = 0; i < pieces.size(); i++) {
			const bool first_of_run = i == 0 || TrackdirToDirection(pieces[i - 1].trackdir) != TrackdirToDirection(pieces[i].trackdir);
			const bool last_of_run = i + 1 == pieces.size() || TrackdirToDirection(pieces[i + 1].trackdir) != TrackdirToDirection(pieces[i].trackdir);
			const bool first_on_bridge = pieces[i].on_bridge && (i == 0 || !pieces[i - 1].on_bridge);
			const bool last_on_bridge = pieces[i].on_bridge && (i + 1 == pieces.size() || !pieces[i + 1].on_bridge);
			is_point[i] = first_of_run || last_of_run || first_on_bridge || last_on_bridge;
		}
		for (const SwitchEvent &event : this->work[pi].events) is_point[event.second] = true;
		for (size_t i = 0; i < pieces.size(); i++) {
			if (is_point[i]) point_ids.emplace(pieces[i].tile.x, pieces[i].tile.y, pieces[i].trackdir);
		}
	}

	/* The points in order of first appearance, and each path's points. */
	std::map<PieceId, uint32_t> index_of;
	std::set<std::pair<uint32_t, uint32_t>> link_seen;
	std::map<std::pair<uint32_t, uint32_t>, uint32_t> diagonal_of; ///< Per diagonal link (from, to), its index into tpl.diagonals.
	for (size_t pi = 0; pi < path_count; pi++) {
		const JunctionPath &path = this->ctx.paths[pi];
		const std::vector<JunctionPiece> &pieces = path.pieces;
		NodePathWork &w = this->work[pi];
		JunctionNodePath node_path{path.from, path.to, {}, {}, 0, 0};
		w.governing.assign(pieces.size(), 0);
		w.link_of.assign(pieces.size(), -1);
		std::vector<size_t> point_pieces;
		for (size_t i = 0; i < pieces.size(); i++) {
			const PieceId id{pieces[i].tile.x, pieces[i].tile.y, pieces[i].trackdir};
			if (!point_ids.contains(id)) continue;
			auto [it, added] = index_of.emplace(id, static_cast<uint32_t>(this->tpl.points.size()));
			if (added) this->tpl.points.push_back({pieces[i].tile, pieces[i].trackdir, 0, 0});
			node_path.points.push_back(it->second);
			node_path.units.push_back(w.start[i]);
			point_pieces.push_back(i);
		}

		/* Links between consecutive points. */
		bool plus = false;
		bool minus = false;
		bool plus_y = false;
		bool minus_y = false;
		for (size_t k = 0; k + 1 < node_path.points.size(); k++) {
			const size_t a = point_pieces[k];
			const size_t b = point_pieces[k + 1];
			bool free_x = true;
			bool free_y = true;
			for (size_t i = a; i <= b; i++) {
				const Trackdir td = pieces[i].trackdir;
				if (pieces[i].on_bridge || (td != Trackdir::X_NE && td != Trackdir::X_SW) || pieces[i].tile.y != pieces[a].tile.y) free_x = false;
				if (pieces[i].on_bridge || (td != Trackdir::Y_SE && td != Trackdir::Y_NW) || pieces[i].tile.x != pieces[a].tile.x) free_y = false;
			}

			/* A free diagonal (DESIGN-N3 §6): one whole run of at least 3 half pieces, none on a bridge, none under a Y-axis bridge. */
			bool diagonal = b - a + 1 >= 3 && (a == 0 || TrackdirToDirection(pieces[a - 1].trackdir) != TrackdirToDirection(pieces[a].trackdir)) && (b + 1 == pieces.size() || TrackdirToDirection(pieces[b + 1].trackdir) != TrackdirToDirection(pieces[b].trackdir));
			for (size_t i = a; diagonal && i <= b; i++) {
				if (pieces[i].on_bridge || IsDiagonalTrackdir(pieces[i].trackdir) || TrackdirToDirection(pieces[i].trackdir) != TrackdirToDirection(pieces[a].trackdir)) diagonal = false;
				for (const JunctionBridge &bridge : layout.bridges) {
					if (bridge.start.x == bridge.end.x && pieces[i].tile.x == bridge.start.x && pieces[i].tile.y > std::min(bridge.start.y, bridge.end.y) && pieces[i].tile.y < std::max(bridge.start.y, bridge.end.y)) diagonal = false;
				}
			}
			const bool free = free_x || free_y || diagonal;
			const Axis axis = free_y ? Axis::Y : Axis::X;
			const uint32_t from = node_path.points[k];
			const uint32_t to = node_path.points[k + 1];
			uint32_t diagonal_index = UINT32_MAX;
			if (link_seen.emplace(from, to).second) {
				if (diagonal) {
					const TileIndexDiffC step = TileIndexDiffCByDir(TrackdirToDirection(pieces[a].trackdir));
					diagonal_index = static_cast<uint32_t>(this->tpl.diagonals.size());
					this->tpl.diagonals.push_back({static_cast<uint32_t>(this->tpl.links.size()), static_cast<int8_t>(step.x), static_cast<int8_t>(step.y), static_cast<uint16_t>(b - a + 1)});
					diagonal_of[{from, to}] = diagonal_index;
				}
				this->tpl.links.push_back({from, to, free, axis, diagonal});
			} else if (diagonal) {
				diagonal_index = diagonal_of.at({from, to});
			}
			if (free) {
				if (diagonal) {
					const JunctionNodeDiagonal &diag = this->tpl.diagonals[diagonal_index];
					if (diag.dx > 0) plus = true;
					if (diag.dx < 0) minus = true;
					if (diag.dy > 0) plus_y = true;
					if (diag.dy < 0) minus_y = true;
				} else if (free_x) {
					const int dx = pieces[b].tile.x - pieces[a].tile.x;
					if (dx > 0) plus = true;
					if (dx < 0) minus = true;
				} else {
					const int dy = pieces[b].tile.y - pieces[a].tile.y;
					if (dy > 0) plus_y = true;
					if (dy < 0) minus_y = true;
				}
				for (size_t i = a + 1; i < b; i++) w.link_of[i] = static_cast<int>(this->spans.size());
				this->spans.push_back({from, to, axis, diagonal_index});
			}
		}
		if (plus && minus) this->Fail(fmt::format("path {} moves both ways along x", pi));
		if (plus_y && minus_y) this->Fail(fmt::format("path {} moves both ways along y", pi));
		node_path.dir = plus ? 1 : (minus ? -1 : 0);
		node_path.dir_y = plus_y ? 1 : (minus_y ? -1 : 0);
		this->tpl.paths.push_back(std::move(node_path));
	}

	/* Classes: X classes join rigid and Y-free links, Y classes join rigid and X-free links; each numbered by its first point, X classes first. */
	NodeUnion x_union(this->tpl.points.size());
	NodeUnion y_union(this->tpl.points.size());
	for (const JunctionNodeLink &link : this->tpl.links) {
		if (link.diagonal) continue;
		if (!link.free || link.axis == Axis::Y) x_union.Join(link.from, link.to);
		if (!link.free || link.axis == Axis::X) y_union.Join(link.from, link.to);
	}
	std::map<uint32_t, uint16_t> x_of_root;
	for (uint32_t p = 0; p < this->tpl.points.size(); p++) {
		auto [it, added] = x_of_root.emplace(x_union.Find(p), static_cast<uint16_t>(x_of_root.size()));
		if (added) this->tpl.flat.push_back(this->tpl.points[p].tile.x);
		this->tpl.points[p].cls = it->second;
	}
	this->tpl.x_classes = static_cast<uint16_t>(x_of_root.size());
	std::map<uint32_t, uint16_t> y_of_root;
	for (uint32_t p = 0; p < this->tpl.points.size(); p++) {
		auto [it, added] = y_of_root.emplace(y_union.Find(p), static_cast<uint16_t>(this->tpl.x_classes + y_of_root.size()));
		if (added) this->tpl.flat.push_back(this->tpl.points[p].tile.y);
		this->tpl.points[p].ycls = it->second;
	}

	/* The pieces that are not strictly inside a free link, with the class of the latest point at or before them. */
	std::set<std::tuple<int, int, Track>> piece_seen;
	for (size_t pi = 0; pi < path_count; pi++) {
		const std::vector<JunctionPiece> &pieces = this->ctx.paths[pi].pieces;
		NodePathWork &w = this->work[pi];
		const JunctionNodePath &node_path = this->tpl.paths[pi];
		size_t next = 0;
		for (size_t i = 0; i < pieces.size(); i++) {
			const PieceId id{pieces[i].tile.x, pieces[i].tile.y, pieces[i].trackdir};
			if (next < node_path.points.size() && index_of.contains(id) && index_of.at(id) == node_path.points[next]) {
				w.governing[i] = node_path.points[next];
				next++;
			} else {
				w.governing[i] = w.governing[i - 1];
			}
			if (w.link_of[i] >= 0) continue;
			const Track track = TrackdirToTrack(pieces[i].trackdir);
			if (!piece_seen.emplace(pieces[i].tile.x, pieces[i].tile.y, track).second) continue;
			this->tpl.pieces.push_back({pieces[i].tile, track, pieces[i].on_bridge, this->tpl.points[w.governing[i]].cls, this->tpl.points[w.governing[i]].ycls});
		}
	}

	/* The bridges with the class of their heads. */
	for (size_t bi = 0; bi < layout.bridges.size(); bi++) {
		const JunctionBridge &bridge = layout.bridges[bi];
		std::optional<std::pair<uint16_t, uint16_t>> cls[2];
		const TileIndexDiffC heads[2] = {bridge.start, bridge.end};
		for (size_t h = 0; h < 2; h++) {
			for (const JunctionNodePiece &piece : this->tpl.pieces) {
				if (piece.on_bridge && piece.tile.x == heads[h].x && piece.tile.y == heads[h].y) {
					cls[h] = std::make_pair(piece.cls, piece.ycls);
					break;
				}
			}
		}
		if (!cls[0].has_value() || !cls[1].has_value()) {
			this->Fail(fmt::format("bridge {}: a head is not a piece of any path", bi));
			this->tpl.bridges.push_back({bridge, 0, this->tpl.x_classes});
		} else {
			if (*cls[0] != *cls[1]) this->Fail(fmt::format("bridge {}: heads in different classes", bi));
			this->tpl.bridges.push_back({bridge, cls[0]->first, cls[0]->second});
		}
	}
}

void NodeBuilder::AddPathDiffs()
{
	const size_t path_count = this->ctx.paths.size();

	/* The switch piece of every event, and whether a merge is a prio merge, as ExtractJunctionChains reads them. */
	std::vector<std::vector<bool>> prio(path_count);
	for (size_t pi = 0; pi < path_count; pi++) {
		const JunctionPath &path = this->ctx.paths[pi];
		for (const SwitchEvent &event : this->work[pi].events) {
			bool is_prio = false;
			const JunctionPiece &merge_piece = path.pieces[event.second];
			if (!event.first && IsMainLinePath(path)) {
				for (size_t qi = 0; qi < path_count && !is_prio; qi++) {
					if (qi == pi) continue;
					for (const SwitchEvent &other : this->work[qi].events) {
						const JunctionPiece &piece = this->ctx.paths[qi].pieces[other.second];
						if (other.first || piece.tile.x != merge_piece.tile.x || piece.tile.y != merge_piece.tile.y || piece.trackdir == merge_piece.trackdir) continue;
						is_prio = this->ctx.paths[qi].from != path.from || this->ctx.paths[qi].to != path.to;
						break;
					}
				}
			}
			prio[pi].push_back(is_prio);
		}
	}

	const int cover = 2 * static_cast<int>(this->spec.prio_tiles);
	for (size_t pi = 0; pi < path_count; pi++) {
		const JunctionPath &path = this->ctx.paths[pi];
		const JunctionNodePath &node_path = this->tpl.paths[pi];
		const NodePathWork &w = this->work[pi];
		const int d = node_path.dir;
		const int d_y = node_path.dir_y;

		/* The rule of §2 between the points p before q with the flat unit difference U (§6): with no X run between them and a Y run or a free diagonal, a Y difference now; with an X run between them, a late gap (an X difference after the Y snap). */
		auto add_gap = [&](uint32_t p, int unit_p, uint32_t q, int unit_q, int g) {
			const int u = unit_q - unit_p;
			const auto ip = std::ranges::find(node_path.points, p) - node_path.points.begin();
			const auto iq = std::ranges::find(node_path.points, q) - node_path.points.begin();
			bool has_x = false;
			bool has_y = false;
			std::vector<uint32_t> diagonals;
			for (auto k = ip; k < iq; k++) {
				const auto link_it = std::ranges::find_if(this->tpl.links, [&](const JunctionNodeLink &l) { return l.from == node_path.points[static_cast<size_t>(k)] && l.to == node_path.points[static_cast<size_t>(k) + 1]; });
				if (link_it == this->tpl.links.end() || !link_it->free) continue;
				if (link_it->diagonal) {
					has_y = true;
					diagonals.push_back(static_cast<uint32_t>(std::ranges::find_if(this->tpl.diagonals, [&](const JunctionNodeDiagonal &diag) { return diag.link == static_cast<uint32_t>(link_it - this->tpl.links.begin()); }) - this->tpl.diagonals.begin()));
				} else if (link_it->axis == Axis::X) {
					has_x = true;
				} else {
					has_y = true;
				}
			}
			if (has_x) {
				this->tpl.late_gaps.push_back({static_cast<uint32_t>(pi), p, q, g, u, std::move(diagonals)});
				return;
			}
			if (!has_y || d_y == 0) {
				if (u < g) this->Fail(fmt::format("path {} points {},{}: gap {} below {}", pi, p, q, u, g));
				return;
			}
			const NodeCoord cp = this->AtY(p);
			const NodeCoord cq = this->AtY(q);
			if (!this->AddDirected(d_y, cp, cq, d_y * (cq.pos - cp.pos) + CeilHalf(g - u))) this->Fail(fmt::format("path {} points {},{}: gap {} below {}", pi, p, q, u, g));
		};

		/* Rule 1, along each free link's axis; a free diagonal's length stays at least two half pieces: m >= m_min (§6). */
		for (size_t k = 0; k + 1 < node_path.points.size(); k++) {
			const auto link = std::ranges::find_if(this->tpl.links, [&](const JunctionNodeLink &l) { return l.from == node_path.points[k] && l.to == node_path.points[k + 1]; });
			if (link == this->tpl.links.end() || !link->free) continue;
			if (link->diagonal) {
				const uint32_t link_index = static_cast<uint32_t>(link - this->tpl.links.begin());
				const JunctionNodeDiagonal &diag = *std::ranges::find_if(this->tpl.diagonals, [&](const JunctionNodeDiagonal &c) { return c.link == link_index; });
				const NodeCoord cp = this->AtY(link->from);
				const NodeCoord cq = this->AtY(link->to);
				const int m_min = -((diag.halves - 2) / 2);
				if (!this->AddDirected(diag.dy, cp, cq, diag.dy * (cq.pos - cp.pos) + m_min)) this->Fail(fmt::format("path {}: a free diagonal joins one class", pi));
				continue;
			}
			if (!this->AddDirected(link->axis == Axis::X ? d : d_y, this->At(link->from, link->axis), this->At(link->to, link->axis), 1)) this->Fail(fmt::format("path {}: a free link joins one class", pi));
		}

		/* Rule 2: the curves of the path are the pieces where the direction changes by 45 degrees. */
		std::vector<JunctionCurveHand> hands;
		std::vector<size_t> curve_pieces;
		for (size_t i = 1; i < path.pieces.size(); i++) {
			const DirDiff diff = DirDifference(TrackdirToDirection(path.pieces[i - 1].trackdir), TrackdirToDirection(path.pieces[i].trackdir));
			if (diff == DirDiff::Left45) {
				hands.push_back(JunctionCurveHand::Left);
				curve_pieces.push_back(i);
			} else if (diff == DirDiff::Right45) {
				hands.push_back(JunctionCurveHand::Right);
				curve_pieces.push_back(i);
			}
		}
		for (const JunctionCurveGap &gap : JunctionCurveGaps(hands, this->spec.tl, this->spec.curve_speed, this->spec.reference_speed)) {
			const size_t ci = curve_pieces[gap.i];
			const size_t cj = curve_pieces[gap.j];
			add_gap(w.governing[ci], w.start[ci], w.governing[cj], w.start[cj], gap.min_gap);
		}

		/* Rule 3: for every prio merge, the curves and splits before it. */
		for (size_t e = 0; e < w.events.size(); e++) {
			if (!prio[pi][e]) continue;
			const size_t mi = w.events[e].second;
			for (const size_t ci : curve_pieces) {
				if (w.start[ci] < w.start[mi]) add_gap(w.governing[ci], w.start[ci], w.governing[mi], w.start[mi], cover);
			}
			for (const SwitchEvent &split : w.events) {
				if (split.first && w.start[split.second] < w.start[mi]) add_gap(w.governing[split.second], w.start[split.second], w.governing[mi], w.start[mi], cover + 1);
			}
		}
	}
}

void NodeBuilder::AddCrossings()
{
	std::set<std::pair<size_t, uint32_t>> late_seen; ///< The (bridge, diagonal) pairs already added as late crossings.
	for (size_t bi = 0; bi < this->tpl.bridges.size(); bi++) {
		const JunctionBridge &bridge = this->tpl.bridges[bi].bridge;
		const NodeCoord head_x{bridge.start.x, this->tpl.bridges[bi].cls};
		const NodeCoord head_y{bridge.start.y, this->tpl.bridges[bi].ycls};
		const Axis along = bridge.start.y == bridge.end.y ? Axis::X : Axis::Y; ///< The bridge's axis A; B is the other.
		const Axis across = along == Axis::X ? Axis::Y : Axis::X;
		const auto pos = [](TileIndexDiffC tile, Axis axis) { return axis == Axis::X ? tile.x : tile.y; };
		const int lo = std::min(pos(bridge.start, along), pos(bridge.end, along));
		const int hi = std::max(pos(bridge.start, along), pos(bridge.end, along));
		const int line = pos(bridge.start, across);
		const NodeCoord head_along = along == Axis::X ? head_x : head_y;
		const NodeCoord head_across = along == Axis::X ? head_y : head_x;

		for (size_t qi = 0; qi < this->ctx.paths.size(); qi++) {
			const std::vector<JunctionPiece> &pieces = this->ctx.paths[qi].pieces;
			const NodePathWork &w = this->work[qi];
			const bool uses = std::ranges::any_of(pieces, [&](const JunctionPiece &p) {
				return p.on_bridge && ((p.tile.x == bridge.start.x && p.tile.y == bridge.start.y) || (p.tile.x == bridge.end.x && p.tile.y == bridge.end.y));
			});
			if (uses) continue;

			for (size_t i = 0; i < pieces.size(); i++) {
				const TileIndexDiffC tile = pieces[i].tile;
				if (pos(tile, across) != line || pos(tile, along) <= lo || pos(tile, along) >= hi) continue;
				if (w.link_of[i] >= 0) {
					const NodeSpan &span = this->spans[static_cast<size_t>(w.link_of[i])];
					if (span.diagonal != UINT32_MAX) {
						/* §6: an X-axis bridge over a free diagonal: its rows now (the bridge row within the diagonal's rows), its tiles after the Y snap. */
						if (along != Axis::X) {
							this->Fail(fmt::format("bridge {}: path {} has a free diagonal under it at {},{}", bi, qi, tile.x, tile.y));
						} else if (late_seen.emplace(bi, span.diagonal).second) {
							const JunctionNodeDiagonal &diag = this->tpl.diagonals[span.diagonal];
							const NodeCoord row{line, head_across.var};
							this->tpl.late_crossings.push_back({static_cast<uint32_t>(bi), span.diagonal});
							if (!this->AddDirected(diag.dy, this->AtY(span.from), row, 0) || !this->AddDirected(diag.dy, row, this->AtY(span.to), 0)) {
								this->Fail(fmt::format("bridge {}: path {} diagonal under it at {},{} is not on the bridge's rows", bi, qi, tile.x, tile.y));
							}
						}
						continue;
					}
					if (span.axis == along) {
						this->Fail(fmt::format("bridge {}: path {} has a free link under it at {},{}", bi, qi, tile.x, tile.y));
						continue;
					}
					const int d = across == Axis::X ? this->tpl.paths[qi].dir : this->tpl.paths[qi].dir_y;
					if (!this->AddDirected(d, this->At(span.from, across), head_across, 1) || !this->AddDirected(d, head_across, this->At(span.to, across), 1)) {
						this->Fail(fmt::format("bridge {}: path {} link under it at {},{} joins the bridge's class", bi, qi, tile.x, tile.y));
					}
					/* The link keeps its line along the bridge's axis: that line stays strictly between the heads. */
					const NodeCoord link_along{pos(tile, along), this->At(span.from, along).var};
					if (!this->Add(link_along, NodeCoord{lo, head_along.var}, 1) || !this->Add(NodeCoord{hi, head_along.var}, link_along, 1)) {
						this->Fail(fmt::format("bridge {}: path {} link under it at {},{} is not between the heads", bi, qi, tile.x, tile.y));
					}
				} else {
					const NodeCoord piece_x{tile.x, this->tpl.points[w.governing[i]].cls};
					const NodeCoord piece_y{tile.y, this->tpl.points[w.governing[i]].ycls};
					const NodeCoord piece_along = along == Axis::X ? piece_x : piece_y;
					const NodeCoord piece_across = along == Axis::X ? piece_y : piece_x;
					const NodeCoord h1{lo, head_along.var};
					const NodeCoord h2{hi, head_along.var};
					if (!this->Add(piece_along, h1, 1) || !this->Add(h2, piece_along, 1)) this->Fail(fmt::format("bridge {}: path {} piece at {},{} is not between the heads", bi, qi, tile.x, tile.y));
					if (!this->Add(piece_across, head_across, 0) || !this->Add(head_across, piece_across, 0)) this->Fail(fmt::format("bridge {}: path {} piece at {},{} is not under the heads", bi, qi, tile.x, tile.y));
				}
			}
		}
	}
}

void NodeBuilder::AddBoxEdges()
{
	std::set<uint32_t> ends;
	for (const JunctionNodePath &path : this->tpl.paths) {
		if (path.points.empty()) continue;
		ends.insert(path.points.front());
		ends.insert(path.points.back());
	}
	if (ends.empty()) return;
	for (const Axis axis : {Axis::X, Axis::Y}) {
		const auto less = [&](uint32_t a, uint32_t b) { return this->At(a, axis).pos < this->At(b, axis).pos; };
		const auto [min_it, max_it] = std::ranges::minmax_element(ends, less);
		for (const int edge : {this->At(*min_it, axis).pos, this->At(*max_it, axis).pos}) {
			std::optional<uint32_t> previous;
			for (const uint32_t point : ends) {
				if (this->At(point, axis).pos != edge) continue;
				if (previous.has_value() && this->At(*previous, axis).var != this->At(point, axis).var) {
					this->Add(this->At(*previous, axis), this->At(point, axis), 0);
					this->Add(this->At(point, axis), this->At(*previous, axis), 0);
				}
				previous = point;
			}
		}
	}
}

JunctionNodeTemplate BuildJunctionNodeTemplate(const JunctionLayout &flat, const JunctionSpec &spec)
{
	JunctionNodeTemplate tpl{};
	const JunctionCheckContext ctx = MakeJunctionCheckContext(flat, spec, FindJunctionPorts(flat), JunctionTMovements());
	NodeBuilder builder{tpl, spec, ctx, {}, {}, {}};
	builder.ReadPoints();
	if (!tpl.error.empty()) return tpl;
	builder.AddPathDiffs();
	builder.AddCrossings();
	builder.AddBoxEdges();

	/* Rule 5: the class of the last piece of the EIn->SOut path. */
	const auto anchor = std::ranges::find_if(tpl.paths, [](const JunctionNodePath &p) { return p.from == JunctionPortRole::EIn && p.to == JunctionPortRole::SOut; });
	if (anchor == tpl.paths.end() || anchor->points.empty()) {
		tpl.anchor = UINT16_MAX;
		tpl.anchor_y = UINT16_MAX;
	} else {
		tpl.anchor = tpl.points[anchor->points.back()].cls;
		tpl.anchor_y = tpl.points[anchor->points.front()].ycls;
	}
	return tpl;
}

bool AddJunctionNodeConstraints(const JunctionNodeTemplate &tpl, JunctionPositions &positions)
{
	if (tpl.flat.empty() || positions.Size() < tpl.flat.size()) return false;
	for (const JunctionNodeDiff &diff : tpl.diffs) {
		if (!positions.AddDifference(diff.a, diff.b, diff.k)) return false;
	}
	if (tpl.anchor == UINT16_MAX) return true;
	return positions.RestrictRange(tpl.anchor, tpl.flat[tpl.anchor], tpl.flat[tpl.anchor]) && positions.RestrictRange(tpl.anchor_y, tpl.flat[tpl.anchor_y], tpl.flat[tpl.anchor_y]);
}

/**
 * x_a - x_b >= k for two coordinates x_c = value(var_c) + pos_c - flat[var_c].
 * @return A difference over the variables, or the constant check when both are one variable; false when infeasible.
 */
static bool NodeAddAtLeast(const JunctionNodeTemplate &tpl, JunctionPositions &positions, uint16_t var_a, int pos_a, uint16_t var_b, int pos_b, int k)
{
	const int off_a = pos_a - tpl.flat[var_a];
	const int off_b = pos_b - tpl.flat[var_b];
	if (var_a == var_b) return off_a - off_b >= k;
	return positions.AddDifference(var_a, var_b, k - off_a + off_b);
}

/** The extra map steps m of a free diagonal at the Y values: its flat halves plus 2 m (DESIGN-N3 §6). */
static int NodeDiagonalSteps(const JunctionNodeTemplate &tpl, const JunctionNodeDiagonal &diag, const std::vector<int> &values)
{
	const JunctionNodePoint &p = tpl.points[tpl.links[diag.link].from];
	const JunctionNodePoint &q = tpl.points[tpl.links[diag.link].to];
	return diag.dy * ((values[q.ycls] - tpl.flat[q.ycls]) - (values[p.ycls] - tpl.flat[p.ycls]));
}

/** The raster of a free diagonal with the given number of half pieces, from its first point's flat tile. */
static JunctionRaster NodeDiagonalRaster(const JunctionNodeTemplate &tpl, const JunctionNodeDiagonal &diag, int halves)
{
	const JunctionNodePoint &p = tpl.points[tpl.links[diag.link].from];
	return RasterizeJunctionRoute({p.tile, p.trackdir, std::string(static_cast<size_t>(std::max(halves - 1, 0)), 'F')});
}

bool AddJunctionNodeLateConstraints(const JunctionNodeTemplate &tpl, JunctionPositions &positions, const std::vector<int> &values)
{
	auto y_of = [&](uint32_t point) { return values[tpl.points[point].ycls] + tpl.points[point].tile.y - tpl.flat[tpl.points[point].ycls]; };

	/* Each free diagonal: x_q - x_p = flat + dx m. */
	for (const JunctionNodeDiagonal &diag : tpl.diagonals) {
		const JunctionNodePoint &p = tpl.points[tpl.links[diag.link].from];
		const JunctionNodePoint &q = tpl.points[tpl.links[diag.link].to];
		const int k = (q.tile.x - p.tile.x) + diag.dx * NodeDiagonalSteps(tpl, diag, values);
		if (!NodeAddAtLeast(tpl, positions, q.cls, q.tile.x, p.cls, p.tile.x, k) || !NodeAddAtLeast(tpl, positions, p.cls, p.tile.x, q.cls, q.tile.x, -k)) return false;
	}

	/* Each late gap: d_x * (x_q - x_p) >= d_x * Fx + ceil((g - U - 2 d_y dy + 2 sum(m)) / 2). */
	for (const JunctionNodeLateGap &gap : tpl.late_gaps) {
		const JunctionNodePoint &p = tpl.points[gap.p];
		const JunctionNodePoint &q = tpl.points[gap.q];
		const int d_x = tpl.paths[gap.path].dir;
		const int d_y = tpl.paths[gap.path].dir_y;
		const int delta_y = (y_of(gap.q) - y_of(gap.p)) - (q.tile.y - p.tile.y);
		int steps = 0;
		for (const uint32_t diag : gap.diagonals) steps += NodeDiagonalSteps(tpl, tpl.diagonals[diag], values);
		const int k = d_x * (q.tile.x - p.tile.x) + CeilHalf(gap.g - gap.u - 2 * d_y * delta_y + 2 * steps);
		if (!(d_x > 0 ? NodeAddAtLeast(tpl, positions, q.cls, q.tile.x, p.cls, p.tile.x, k) : NodeAddAtLeast(tpl, positions, p.cls, p.tile.x, q.cls, q.tile.x, k))) return false;
	}

	/* Each late crossing: the diagonal's tiles on the bridge row, from its raster at the known length, strictly between the heads. */
	for (const JunctionNodeLateCrossing &crossing : tpl.late_crossings) {
		const JunctionNodeBridge &node_bridge = tpl.bridges[crossing.bridge];
		const JunctionNodeDiagonal &diag = tpl.diagonals[crossing.diagonal];
		const uint32_t from = tpl.links[diag.link].from;
		const JunctionNodePoint &p = tpl.points[from];
		const JunctionRaster raster = NodeDiagonalRaster(tpl, diag, diag.halves + 2 * NodeDiagonalSteps(tpl, diag, values));
		if (!raster.error.empty()) return false;
		const int row = values[node_bridge.ycls] + node_bridge.bridge.start.y - tpl.flat[node_bridge.ycls];
		const int lo = std::min(node_bridge.bridge.start.x, node_bridge.bridge.end.x);
		const int hi = std::max(node_bridge.bridge.start.x, node_bridge.bridge.end.x);
		for (size_t i = 1; i + 1 < raster.pieces.size(); i++) {
			if (raster.pieces[i].tile.y - p.tile.y != row - y_of(from)) continue;
			const int x = raster.pieces[i].tile.x;
			if (!NodeAddAtLeast(tpl, positions, p.cls, x, node_bridge.cls, lo, 1) || !NodeAddAtLeast(tpl, positions, node_bridge.cls, hi, p.cls, x, 1)) return false;
		}
	}
	return true;
}

std::vector<int> JunctionNodeSnap(const JunctionNodeTemplate &tpl, JunctionPositions &positions)
{
	std::vector<int> values(tpl.flat.size(), 0);
	auto fix = [&](size_t c) {
		values[c] = std::clamp(tpl.flat[c], positions.Lo(c), positions.Up(c));
		return positions.RestrictRange(c, values[c], values[c]);
	};
	for (size_t c = tpl.x_classes; c < tpl.flat.size(); c++) {
		if (!fix(c)) return {};
	}
	if (!AddJunctionNodeLateConstraints(tpl, positions, values)) return {};
	for (size_t c = 0; c < tpl.x_classes; c++) {
		if (!fix(c)) return {};
	}
	return values;
}

JunctionLayout RenderJunctionNodeTracks(const JunctionNodeTemplate &tpl, const std::vector<int> &values)
{
	std::map<std::pair<int, int>, TrackBits> tiles;
	auto shift = [&](uint16_t cls) { return values[cls] - tpl.flat[cls]; };

	for (const JunctionNodePiece &piece : tpl.pieces) {
		if (piece.on_bridge) continue;
		tiles[{piece.tile.x + shift(piece.cls), piece.tile.y + shift(piece.ycls)}].Set(piece.track);
	}
	for (const JunctionNodeDiagonal &diag : tpl.diagonals) {
		const JunctionNodePoint &from = tpl.points[tpl.links[diag.link].from];
		const JunctionRaster raster = NodeDiagonalRaster(tpl, diag, diag.halves + 2 * NodeDiagonalSteps(tpl, diag, values));
		for (size_t i = 1; i + 1 < raster.pieces.size(); i++) {
			tiles[{raster.pieces[i].tile.x + shift(from.cls), raster.pieces[i].tile.y + shift(from.ycls)}].Set(TrackdirToTrack(raster.pieces[i].trackdir));
		}
	}
	for (const JunctionNodeLink &link : tpl.links) {
		if (!link.free || link.diagonal) continue;
		const JunctionNodePoint &from = tpl.points[link.from];
		const JunctionNodePoint &to = tpl.points[link.to];
		if (link.axis == Axis::X) {
			const int row = from.tile.y + shift(from.ycls);
			const int x_from = from.tile.x + shift(from.cls);
			const int x_to = to.tile.x + shift(to.cls);
			for (int x = std::min(x_from, x_to) + 1; x < std::max(x_from, x_to); x++) tiles[{x, row}].Set(Track::X);
		} else {
			const int column = from.tile.x + shift(from.cls);
			const int y_from = from.tile.y + shift(from.ycls);
			const int y_to = to.tile.y + shift(to.ycls);
			for (int y = std::min(y_from, y_to) + 1; y < std::max(y_from, y_to); y++) tiles[{column, y}].Set(Track::Y);
		}
	}

	JunctionLayout layout{};
	for (const auto &[tile, bits] : tiles) layout.tracks.push_back({{static_cast<int16_t>(tile.first), static_cast<int16_t>(tile.second)}, bits});
	for (const JunctionNodeBridge &node_bridge : tpl.bridges) {
		JunctionBridge moved = node_bridge.bridge;
		moved.start.x = static_cast<int16_t>(moved.start.x + shift(node_bridge.cls));
		moved.end.x = static_cast<int16_t>(moved.end.x + shift(node_bridge.cls));
		moved.start.y = static_cast<int16_t>(moved.start.y + shift(node_bridge.ycls));
		moved.end.y = static_cast<int16_t>(moved.end.y + shift(node_bridge.ycls));
		layout.bridges.push_back(moved);
	}
	NormaliseJunctionLayout(layout);
	return layout;
}

bool JunctionNodeTrackFits(const JunctionFacts &facts, int x, int y, TrackBits bits)
{
	if (bits.Count() >= 2) return facts.Switch(x, y);
	int8_t entry;
	int8_t exit;
	if (bits.Test(Track::X)) return facts.Straight(x, y, Axis::X, entry, exit);
	if (bits.Test(Track::Y)) return facts.Straight(x, y, Axis::Y, entry, exit);
	return bits.Any() && facts.Half(x, y);
}

JunctionNodeSolve SolveJunctionNodes(const JunctionNodeTemplate &tpl, const JunctionSpec &spec, const JunctionFacts &facts, TileIndexDiffC offset)
{
	JunctionNodeSolve result;
	using Kind = JunctionNodeFailure::Kind;
	constexpr int RANGE = 1024;

	std::vector<uint32_t> free_links; ///< The X-free links (X runs; the free diagonals are not among them).
	for (uint32_t i = 0; i < tpl.links.size(); i++) {
		if (tpl.links[i].free && !tpl.links[i].diagonal && tpl.links[i].axis == Axis::X) free_links.push_back(i);
	}
	const size_t variables = tpl.flat.size();
	const size_t classes = tpl.x_classes;
	JunctionPositions positions(variables + 3 * free_links.size(), -RANGE, RANGE);

	/* 2. The template. */
	if (!AddJunctionNodeConstraints(tpl, positions)) {
		result.failure = JunctionNodeFailure{Kind::Template};
		return result;
	}

	/* Fix a variable at the nearest value to its flat one; false when none is left. */
	result.values.assign(variables, 0);
	auto snap = [&](size_t c) {
		for (int distance = 0;; distance++) {
			const int low = tpl.flat[c] - distance;
			const int high = tpl.flat[c] + distance;
			if (low < positions.Lo(c) && high > positions.Up(c)) return false;
			for (const int t : {low, high}) {
				if (t < positions.Lo(c) || t > positions.Up(c)) continue;
				const size_t mark = positions.Mark();
				if (positions.RestrictRange(c, t, t)) {
					result.values[c] = positions.Lo(c);
					return true;
				}
				positions.Undo(mark);
				if (distance == 0) break;
			}
		}
	};

	/* 2b. The Y variables: snapped to the flat rows, no facts. */
	for (size_t c = classes; c < variables; c++) {
		if (!snap(c)) {
			result.failure = JunctionNodeFailure{Kind::Template};
			result.values.clear();
			return result;
		}
	}
	auto shift_y = [&](uint16_t ycls) { return result.values[ycls] - tpl.flat[ycls]; };

	/* 2c. The constraints that need the Y values: the free diagonals' x, the late gaps, the late crossings. */
	if (!AddJunctionNodeLateConstraints(tpl, positions, result.values)) {
		result.failure = JunctionNodeFailure{Kind::Template};
		result.values.clear();
		return result;
	}

	/* The interior of every free diagonal at its length now, as rasters from the flat tile of its first point. */
	std::vector<JunctionRaster> diagonal_rasters;
	for (const JunctionNodeDiagonal &diag : tpl.diagonals) diagonal_rasters.push_back(NodeDiagonalRaster(tpl, diag, diag.halves + 2 * NodeDiagonalSteps(tpl, diag, result.values)));

	/* 3. The pieces of every class fit the facts; a tile's bits are those of the render at the flat values. */
	std::map<std::pair<int, int>, TrackBits> flat_bits;
	for (const JunctionNodePiece &piece : tpl.pieces) {
		if (!piece.on_bridge) flat_bits[{piece.tile.x, piece.tile.y}].Set(piece.track);
	}
	for (const JunctionNodeLink &link : tpl.links) {
		if (!link.free || link.diagonal) continue;
		const JunctionNodePoint &from = tpl.points[link.from];
		const JunctionNodePoint &to = tpl.points[link.to];
		if (link.axis == Axis::X) {
			for (int x = std::min(from.tile.x, to.tile.x) + 1; x < std::max(from.tile.x, to.tile.x); x++) flat_bits[{x, from.tile.y}].Set(Track::X);
		} else {
			for (int y = std::min(from.tile.y, to.tile.y) + 1; y < std::max(from.tile.y, to.tile.y); y++) flat_bits[{from.tile.x, y}].Set(Track::Y);
		}
	}
	for (uint16_t c = 0; c < classes; c++) {
		std::vector<int> allowed;
		for (int v = -RANGE; v <= RANGE; v++) {
			bool fits = std::ranges::all_of(tpl.pieces, [&](const JunctionNodePiece &piece) {
				if (piece.cls != c || piece.on_bridge) return true;
				return JunctionNodeTrackFits(facts, piece.tile.x + v - tpl.flat[c] + offset.x, piece.tile.y + shift_y(piece.ycls) + offset.y, flat_bits.at({piece.tile.x, piece.tile.y}));
			});

			/* A Y-free link of this class is a column: every interior row is a straight Y piece and consecutive rows connect. */
			for (size_t i = 0; fits && i < tpl.links.size(); i++) {
				const JunctionNodeLink &link = tpl.links[i];
				const JunctionNodePoint &p = tpl.points[link.from];
				const JunctionNodePoint &q = tpl.points[link.to];
				if (!link.free || link.axis != Axis::Y || p.cls != c) continue;
				const int column = p.tile.x + v - tpl.flat[c] + offset.x;
				const int y_p = p.tile.y + shift_y(p.ycls) + offset.y;
				const int y_q = q.tile.y + shift_y(q.ycls) + offset.y;
				int8_t previous_exit = 0;
				for (int r = std::min(y_p, y_q) + 1; fits && r < std::max(y_p, y_q); r++) {
					int8_t entry;
					int8_t exit;
					if (!facts.Straight(column, r, Axis::Y, entry, exit) || (r > std::min(y_p, y_q) + 1 && previous_exit != entry)) fits = false;
					previous_exit = exit;
				}
			}
			if (fits) allowed.push_back(v);
		}
		if (allowed.empty() || !positions.RestrictDomain(c, allowed)) {
			result.failure = JunctionNodeFailure{Kind::Piece, UINT32_MAX, 0, c};
			result.values.clear();
			return result;
		}
	}

	/* 4. The X-free links. */
	for (size_t k = 0; k < free_links.size(); k++) {
		const uint32_t link_index = free_links[k];
		const JunctionNodeLink &link = tpl.links[link_index];
		const JunctionNodePoint &p = tpl.points[link.from];
		const JunctionNodePoint &q = tpl.points[link.to];
		int d = 0;
		for (const JunctionNodePath &path : tpl.paths) {
			for (size_t i = 0; i + 1 < path.points.size() && d == 0; i++) {
				if (path.points[i] == link.from && path.points[i + 1] == link.to) d = path.dir;
			}
		}
		const size_t u = variables + 3 * k;
		const size_t w = u + 1;
		const size_t w2 = u + 2;
		const int row = p.tile.y + shift_y(p.ycls) + offset.y;

		/* aux = x_cls + shift, in working x, as two difference constraints. */
		auto tie = [&](size_t aux, const JunctionNodePoint &point, int shift) {
			const int constant = point.tile.x - tpl.flat[point.cls] + offset.x + shift;
			return positions.AddDifference(aux, point.cls, constant) && positions.AddDifference(point.cls, aux, -constant);
		};
		if (!tie(u, p, d) || !tie(w, q, -d) || !tie(w2, q, -2 * d)) {
			result.failure = JunctionNodeFailure{Kind::Template};
			result.values.clear();
			return result;
		}

		for (int c = facts.MinX(); c <= facts.MaxX(); c++) {
			int8_t entry;
			int8_t exit;
			int8_t next_entry;
			int8_t next_exit;
			if (!facts.Straight(c, row, Axis::X, entry, exit)) {
				if (!(d > 0 ? positions.AddAvoid(u, w, c) : positions.AddAvoid(w, u, c))) {
					result.failure = JunctionNodeFailure{Kind::Straight, link_index, c};
					result.values.clear();
					return result;
				}
			} else if (c + 1 <= facts.MaxX() && facts.Straight(c + 1, row, Axis::X, next_entry, next_exit) && exit != next_entry) {
				if (!(d > 0 ? positions.AddAvoid(u, w2, c) : positions.AddAvoid(w2, u, c + 1))) {
					result.failure = JunctionNodeFailure{Kind::Break, link_index, c};
					result.values.clear();
					return result;
				}
			}
		}
		if (!positions.RestrictRange(u, facts.MinX(), facts.MaxX())) {
			result.failure = JunctionNodeFailure{Kind::Edge, link_index, facts.MinX() - 1};
			result.values.clear();
			return result;
		}
		if (!positions.RestrictRange(w, facts.MinX(), facts.MaxX())) {
			result.failure = JunctionNodeFailure{Kind::Edge, link_index, facts.MaxX() + 1};
			result.values.clear();
			return result;
		}
	}

	/* 5. Snap each X class to the nearest value. */
	for (size_t c = 0; c < classes; c++) {
		if (!snap(c)) {
			result.failure = JunctionNodeFailure{Kind::Template};
			result.values.clear();
			return result;
		}
	}

	/* 6. Render. */
	JunctionLayout layout = RenderJunctionNodeTracks(tpl, result.values);
	PlaceJunctionSignals(layout, spec);
	NormaliseJunctionLayout(layout);
	result.layout = std::move(layout);
	return result;
}

/**
 * Whether an axis piece on a Buildable, non-steep tile inclined along its direction is a ramp, and from which height (mirrors HeightStep and RampStep in junction_line_router.cpp).
 * @param tile The terrain tile of the piece, or nullptr.
 * @param trackdir The piece's trackdir.
 * @param[out] up Whether the ramp rises along the piece's direction.
 * @return Whether the piece is inclined along its direction (a ramp, or unusable when the tile cannot take one).
 */
static bool NodeInclinedAlong(const JunctionTerrainTile *tile, Trackdir trackdir, bool &up)
{
	if (tile == nullptr || !IsDiagonalTrackdir(trackdir) || tile->incline == DiagDirection::Invalid) return false;
	const DiagDirection exit = TrackdirToExitdir(trackdir);
	up = tile->incline == exit;
	return up || tile->incline == ReverseDiagDir(exit);
}

/**
 * The height step of one piece as the line router's HeightStep reads it with ramps allowed (RampStep for a ramp, UsableAt otherwise).
 * @param[out] new_z The height after the piece.
 * @return Whether the piece is usable at z.
 */
static bool NodeHeightStep(const JunctionTerrain &terrain, int x, int y, Trackdir trackdir, int8_t z, int8_t &new_z)
{
	new_z = z;
	const JunctionTerrainTile *tile = terrain.At(x, y);
	bool up = false;
	if (NodeInclinedAlong(tile, trackdir, up)) {
		if (tile->cls != JunctionTileClass::Buildable || tile->steep) return false;
		if (up && tile->z_min == z) {
			new_z = static_cast<int8_t>(z + 1);
		} else if (!up && tile->z_max == z) {
			new_z = static_cast<int8_t>(z - 1);
		} else {
			return false;
		}
		return true;
	}
	return terrain.UsableAt(x, y, z);
}

bool JunctionNodeHeightsOk(const JunctionLayout &layout, const JunctionSpec &spec, const JunctionTerrain &terrain, TileIndexDiffC offset, std::string *why)
{
	const JunctionCheckContext ctx = MakeJunctionCheckContext(layout, spec, FindJunctionPorts(layout), JunctionTMovements());
	std::map<std::tuple<int, int, Track>, int8_t> levels; ///< Per (working tile, track), the lower height of the piece.
	for (size_t i = 0; i < ctx.paths.size(); i++) {
		const std::vector<JunctionPiece> &pieces = ctx.paths[i].pieces;
		int8_t z = 0;
		for (size_t j = 0; j < pieces.size(); j++) {
			const JunctionPiece &piece = pieces[j];
			const int x = piece.tile.x + offset.x;
			const int y = piece.tile.y + offset.y;
			auto fail = [&](const std::string &reason) {
				if (why != nullptr) *why = fmt::format("path {} piece {} at {},{}: {}", i, j, x, y, reason);
				return false;
			};
			const JunctionTerrainTile *tile = terrain.At(x, y);
			if (j == 0) {
				bool up = false;
				if (NodeInclinedAlong(tile, piece.trackdir, up) && tile->cls == JunctionTileClass::Buildable && !tile->steep) {
					z = up ? tile->z_min : tile->z_max;
				} else {
					z = tile != nullptr ? tile->z_max : static_cast<int8_t>(0);
				}
			}
			if (piece.on_bridge) continue;
			int8_t new_z = z;
			if (!NodeHeightStep(terrain, x, y, piece.trackdir, z, new_z)) return fail(fmt::format("not usable at height {}", z));
			const auto key = std::make_tuple(x, y, TrackdirToTrack(piece.trackdir));
			const int8_t level = std::min(z, new_z);
			const auto [it, inserted] = levels.emplace(key, level);
			if (!inserted && it->second != level) return fail(fmt::format("height {} but another path has it at {}", level, it->second));
			z = new_z;
		}
	}
	return true;
}

std::optional<JunctionLayout> InsertJunctionDetour(const JunctionLayout &layout, const JunctionNodeTemplate &tpl, uint32_t link, int column, int side, const JunctionSpec &spec, std::string *why)
{
	auto fail = [&](const std::string &reason) {
		if (why != nullptr) *why = reason;
		return std::optional<JunctionLayout>{};
	};
	if (link >= tpl.links.size() || !tpl.links[link].free || tpl.links[link].diagonal || tpl.links[link].axis != Axis::X || (side != 1 && side != -1)) return fail("not a free link or side");

	/* 1. The link and its interior. */
	const JunctionNodePoint &p = tpl.points[tpl.links[link].from];
	const JunctionNodePoint &q = tpl.points[tpl.links[link].to];
	const int row = p.tile.y;
	const int d = q.tile.x > p.tile.x ? 1 : -1;
	const int length = d * (q.tile.x - p.tile.x);
	const int lo = std::min<int>(p.tile.x, q.tile.x) + 1;
	const int hi = std::max<int>(p.tile.x, q.tile.x) - 1;
	if (column < lo || column > hi) return fail("column outside the link");

	/* 2. The hands of the S-bend. */
	char t1 = 'L';
	char t2 = 'R';
	JunctionRaster probe = RasterizeJunctionRoute({p.tile, p.trackdir, "FLFRF"});
	if (!probe.error.empty() || probe.pieces.back().tile.y != row + side) {
		t1 = 'R';
		t2 = 'L';
		probe = RasterizeJunctionRoute({p.tile, p.trackdir, "FRFLF"});
		if (!probe.error.empty() || probe.pieces.back().tile.y != row + side) return fail("no S-bend to that side");
	}

	/* The layout with the link's interior replaced by a program's raster, or the reason it cannot be had. */
	auto build = [&](JunctionRaster raster, std::string &reason) -> std::optional<JunctionLayout> {
		JunctionLayout result = layout;
		for (JunctionTrack &jt : result.tracks) {
			if (jt.offset.y == row && jt.offset.x >= lo && jt.offset.x <= hi) jt.tracks.Reset(Track::X);
		}
		std::erase_if(result.tracks, [](const JunctionTrack &jt) { return jt.tracks.None(); });
		raster.pieces.pop_back();
		raster.pieces.erase(raster.pieces.begin());
		raster.bridges.clear();
		for (const JunctionPiece &piece : raster.pieces) {
			const bool taken = std::ranges::any_of(result.tracks, [&](const JunctionTrack &jt) { return jt.offset.x == piece.tile.x && jt.offset.y == piece.tile.y; });
			if (taken) {
				reason = fmt::format("collision at {},{}", piece.tile.x, piece.tile.y);
				return std::nullopt;
			}
		}
		AddRasterToLayout(result, raster);

		/* A shifted run of one piece has no free link: the two S-bends become one rigid class and their curve gap cannot stretch, which the template reports as an error. */
		JunctionLayout normalised = result;
		NormaliseJunctionLayout(normalised);
		const JunctionNodeTemplate child = BuildJunctionNodeTemplate(normalised, spec);
		if (!child.error.empty()) {
			reason = "template error: " + child.error;
			return std::nullopt;
		}
		return result;
	};

	/* 3. The lengths of the runs: the first (n1, n2, n3) in the order of the rules whose layout has no collision and a template. */
	auto program = [&](int n1, int n2, int n3) {
		return std::string(static_cast<size_t>(n1), 'F') + t1 + 'F' + t2 + std::string(static_cast<size_t>(n2), 'F') + t2 + 'F' + t1 + std::string(static_cast<size_t>(n3), 'F');
	};
	std::string reason;
	for (int n1 = length; n1 >= 0; n1--) {
		for (int n2 = 0; n2 <= length; n2++) {
			const JunctionRaster raster = RasterizeJunctionRoute({p.tile, p.trackdir, program(n1, n2, 0)});
			if (!raster.error.empty()) continue;
			const size_t first_shifted = static_cast<size_t>(n1) + 3;
			const size_t last_shifted = first_shifted + static_cast<size_t>(n2);
			if (last_shifted + 3 >= raster.pieces.size()) continue;

			/* Nothing on the link's row reaches the column before the second S-bend, and the shifted run covers it. */
			bool clear = true;
			bool covers = false;
			for (size_t i = 0; i <= last_shifted; i++) {
				const JunctionPiece &piece = raster.pieces[i];
				if (piece.tile.y == row && d * (piece.tile.x - column) >= 0) clear = false;
				if (i >= first_shifted && piece.tile.y == row + side && piece.tile.x == column) covers = true;
			}
			if (!clear || !covers) continue;

			/* The closing run ends on q. */
			const JunctionPiece &end = raster.pieces.back();
			if (end.tile.y != row || end.trackdir != q.trackdir) continue;
			const int n3 = d * (q.tile.x - end.tile.x);
			if (n3 < 0) continue;
			JunctionRaster full = RasterizeJunctionRoute({p.tile, p.trackdir, program(n1, n2, n3)});
			if (!full.error.empty() || full.pieces.back().tile.x != q.tile.x || full.pieces.back().tile.y != q.tile.y || full.pieces.back().trackdir != q.trackdir) continue;

			std::optional<JunctionLayout> result = build(std::move(full), reason);
			if (result.has_value()) return result;
		}
	}
	return fail(reason.empty() ? "link too short for a detour" : "no buildable detour: " + reason);
}

/** A state of the structure search: a layout (the flat T with detours) and its working offset (working = layout + offset). */
struct NodeState {
	JunctionLayout layout;
	TileIndexDiffC offset;
	int detours;
};

/** The sorted track list of a layout, as the search's visited key. */
static std::vector<std::tuple<int, int, uint>> NodeStateKey(const JunctionLayout &layout)
{
	std::vector<std::tuple<int, int, uint>> key;
	for (const JunctionTrack &jt : layout.tracks) key.emplace_back(jt.offset.x, jt.offset.y, static_cast<uint>(jt.tracks.base()));
	std::ranges::sort(key);
	return key;
}

/** Where the anchor pieces of a rendered layout lie relative to their tiles in the template's layout (layout minus template: x from the last piece of EIn->SOut, y from its first), or nullopt without an EIn->SOut path. */
static std::optional<TileIndexDiffC> NodeAnchorShift(const JunctionNodeTemplate &tpl, const JunctionLayout &layout, const JunctionSpec &spec)
{
	const auto anchor = std::ranges::find_if(tpl.paths, [](const JunctionNodePath &path) { return path.from == JunctionPortRole::EIn && path.to == JunctionPortRole::SOut; });
	const JunctionCheckContext ctx = MakeJunctionCheckContext(layout, spec, FindJunctionPorts(layout), JunctionTMovements());
	const auto ctx_path = std::ranges::find_if(ctx.paths, [](const JunctionPath &path) { return path.from == JunctionPortRole::EIn && path.to == JunctionPortRole::SOut; });
	if (tpl.anchor == UINT16_MAX || anchor == tpl.paths.end() || anchor->points.empty() || ctx_path == ctx.paths.end()) return std::nullopt;
	const TileIndexDiffC anchor_flat_x = tpl.points[anchor->points.back()].tile;
	const TileIndexDiffC anchor_flat_y = tpl.points[anchor->points.front()].tile;
	return TileIndexDiffC{static_cast<int16_t>(ctx_path->pieces.back().tile.x - anchor_flat_x.x), static_cast<int16_t>(ctx_path->pieces.front().tile.y - anchor_flat_y.y)};
}

JunctionNodeSearch SearchJunctionNodes(const JunctionLayout &flat, const JunctionSpec &spec, const JunctionTerrain &terrain, TileIndexDiffC offset, int max_states)
{
	JunctionNodeSearch search;
	const JunctionFacts facts(terrain);
	std::multimap<int, NodeState> queue; ///< By detour count; equal counts in insertion order.
	std::set<std::vector<std::tuple<int, int, uint>>> visited;

	/* A state is track-only: the finished flat layout's signals and prio rows are not path pieces of its template. */
	const JunctionNodeTemplate flat_tpl = BuildJunctionNodeTemplate(flat, spec);
	if (!flat_tpl.error.empty()) {
		search.log = "template error: " + flat_tpl.error + "\n";
		return search;
	}
	JunctionLayout start = RenderJunctionNodeTracks(flat_tpl, flat_tpl.flat);
	const std::optional<TileIndexDiffC> start_shift = NodeAnchorShift(flat_tpl, start, spec);
	if (!start_shift.has_value()) {
		search.log = "no anchor path\n";
		return search;
	}
	const TileIndexDiffC start_offset = {static_cast<int16_t>(offset.x - start_shift->x), static_cast<int16_t>(offset.y - start_shift->y)};
	visited.insert(NodeStateKey(start));
	queue.emplace(0, NodeState{std::move(start), start_offset, 0});

	while (!queue.empty() && search.states < max_states) {
		const NodeState state = std::move(queue.begin()->second);
		queue.erase(queue.begin());

		const JunctionNodeTemplate tpl = BuildJunctionNodeTemplate(state.layout, spec);
		if (!tpl.error.empty()) {
			search.log += fmt::format("state {} detours {}: template error: {}\n", search.states + 1, state.detours, tpl.error);
			continue;
		}
		const JunctionNodeSolve solve = SolveJunctionNodes(tpl, spec, facts, state.offset);
		search.states++;
		const std::string head = fmt::format("state {} detours {}: ", search.states, state.detours);

		if (solve.layout.has_value()) {
			const JunctionLayout &layout = *solve.layout;

			/* The shift of the normalisation: the anchor class is fixed, so its piece sits at its state tile plus the state offset. */
			const std::optional<TileIndexDiffC> shift = NodeAnchorShift(tpl, layout, spec);
			if (!shift.has_value()) {
				search.log += head + "no anchor path\n";
				continue;
			}
			const TileIndexDiffC layout_offset = {static_cast<int16_t>(state.offset.x - shift->x), static_cast<int16_t>(state.offset.y - shift->y)};

			const std::vector<RuleViolation> violations = CheckJunctionRules(layout, spec);
			if (!violations.empty()) {
				search.log += head + fmt::format("rules: rule {} at {},{}: {}\n", to_underlying(violations.front().rule), violations.front().tile.x, violations.front().tile.y, violations.front().message);
				continue;
			}
			std::string why;
			if (!JunctionNodeHeightsOk(layout, spec, terrain, layout_offset, &why)) {
				search.log += head + "heights: " + why + "\n";
				continue;
			}
			bool fits = true;
			for (const JunctionTrack &jt : layout.tracks) {
				const bool bridge_head = std::ranges::any_of(layout.bridges, [&](const JunctionBridge &b) { return (b.start.x == jt.offset.x && b.start.y == jt.offset.y) || (b.end.x == jt.offset.x && b.end.y == jt.offset.y); });
				if (bridge_head || JunctionNodeTrackFits(facts, jt.offset.x + layout_offset.x, jt.offset.y + layout_offset.y, jt.tracks)) continue;
				search.log += head + fmt::format("fits: tile {},{}\n", jt.offset.x + layout_offset.x, jt.offset.y + layout_offset.y);
				fits = false;
				break;
			}
			if (!fits) continue;
			search.log += head + "ok\n";
			search.layout = layout;
			return search;
		}

		const JunctionNodeFailure &failure = *solve.failure;
		search.log += head + fmt::format("failure kind {} link {} column {}\n", to_underlying(failure.kind), failure.link, failure.column);
		if (failure.kind != JunctionNodeFailure::Kind::Straight && failure.kind != JunctionNodeFailure::Kind::Break) continue;

		for (const int side : {-1, 1}) {
			std::string why;
			std::optional<JunctionLayout> child = InsertJunctionDetour(state.layout, tpl, failure.link, failure.column - state.offset.x, side, spec, &why);
			if (!child.has_value()) {
				search.log += fmt::format("  detour link {} column {} side {}: {}\n", failure.link, failure.column, side, why);
				continue;
			}
			int min_x = INT32_MAX;
			int min_y = INT32_MAX;
			for (const JunctionTrack &jt : child->tracks) {
				min_x = std::min<int>(min_x, jt.offset.x);
				min_y = std::min<int>(min_y, jt.offset.y);
			}
			for (const JunctionBridge &bridge : child->bridges) {
				for (const TileIndexDiffC &bridge_head : {bridge.start, bridge.end}) {
					min_x = std::min<int>(min_x, bridge_head.x);
					min_y = std::min<int>(min_y, bridge_head.y);
				}
			}
			NormaliseJunctionLayout(*child);
			if (!visited.insert(NodeStateKey(*child)).second) continue;
			const TileIndexDiffC child_offset = {static_cast<int16_t>(state.offset.x + min_x), static_cast<int16_t>(state.offset.y + min_y)};
			queue.emplace(state.detours + 1, NodeState{std::move(*child), child_offset, state.detours + 1});
		}
	}
	return search;
}
