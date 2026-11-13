/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_nodes.cpp The node model's position template and its render implementation. */

#include "../../stdafx.h"

#include "junction_nodes.h"

#include "../../direction_func.h"
#include "../topology/junction_chain.h"
#include "../terrain/junction_facts.h"
#include "../junction_rules_internal.h"
#include "../signals/junction_signal_pass.h"
#include "../placement/junction_placement.h"
#include "../signals/junction_signals.h"
#include "../junction_pipeline.h"
#include "../../map_func.h"
#include "../../track_func.h"

#include <algorithm>
#include <array>
#include <climits>
#include <cstdlib>
#include <functional>
#include <map>
#include <set>
#include <tuple>

#include "../../safeguards.h"

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
	int8_t dir; ///< As JunctionNodeLink::dir.
	uint32_t diagonal; ///< The index into JunctionNodeTemplate::diagonals when the link is a free diagonal, else UINT32_MAX.
};

/** Reads the template of an input. */
struct NodeBuilder {
	JunctionNodeTemplate &tpl;
	const JunctionSpec &spec;
	const JunctionNodeInput &input;
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
	/** The port bends' lean per class (D212, DESIGN-G1 §13.3); needs the tags. */
	void ReadLeans();
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
	const size_t path_count = this->input.paths.size();

	this->work.resize(path_count);
	for (size_t pi = 0; pi < path_count; pi++) {
		const std::vector<JunctionPiece> &pieces = this->input.paths[pi].pieces;
		NodePathWork &w = this->work[pi];
		w.events = this->input.events[pi];
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
		const std::vector<JunctionPiece> &pieces = this->input.paths[pi].pieces;
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
		const JunctionPath &path = this->input.paths[pi];
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
			if (added) {
				this->tpl.points.push_back({pieces[i].tile, pieces[i].trackdir, 0, 0});
				if (!this->input.tags.empty()) this->tpl.point_tags.push_back(this->input.tags[pi][i]);
			}
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
				for (const JunctionBridge &bridge : this->input.bridges) {
					if (bridge.start.x == bridge.end.x && pieces[i].tile.x == bridge.start.x && pieces[i].tile.y > std::min(bridge.start.y, bridge.end.y) && pieces[i].tile.y < std::max(bridge.start.y, bridge.end.y)) diagonal = false;
				}
			}
			const bool free = free_x || free_y || diagonal;
			const Axis axis = free_y ? Axis::Y : Axis::X;
			int8_t link_dir = 0;
			if (free && !diagonal) {
				const int step = free_x ? pieces[b].tile.x - pieces[a].tile.x : pieces[b].tile.y - pieces[a].tile.y;
				link_dir = step > 0 ? 1 : (step < 0 ? -1 : 0);
			}
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
				this->tpl.links.push_back({from, to, free, axis, link_dir, diagonal});
				if (!this->input.tags.empty()) {
					JunctionTrackPieceTag tag = this->input.tags[pi][a];
					if (a + 1 < b) {
						tag = this->input.tags[pi][a + 1];
					} else {
						tag.token = JunctionTrackPieceTag::RUN;
					}
					this->tpl.link_tags.push_back(tag);
				}
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
				this->spans.push_back({from, to, axis, link_dir, diagonal_index});
			}
		}
		node_path.dir = plus && !minus ? 1 : (minus && !plus ? -1 : 0);
		node_path.dir_y = plus_y && !minus_y ? 1 : (minus_y && !plus_y ? -1 : 0);
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
	std::set<std::tuple<int, int, Trackdir, int, int>> join_seen;
	for (size_t pi = 0; pi < path_count; pi++) {
		const std::vector<JunctionPiece> &pieces = this->input.paths[pi].pieces;
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

			/* The join to the previous piece (DESIGN-N3 §10): both on the ground and neither strictly inside a free link. */
			if (i > 0 && !pieces[i - 1].on_bridge && !pieces[i].on_bridge && w.link_of[i - 1] < 0 && w.link_of[i] < 0) {
				const JunctionNodePoint &before = this->tpl.points[w.governing[i - 1]];
				const JunctionNodePoint &after = this->tpl.points[w.governing[i]];
				if (before.cls == after.cls && before.ycls == after.ycls) {
					if (join_seen.emplace(pieces[i - 1].tile.x, pieces[i - 1].tile.y, pieces[i - 1].trackdir, pieces[i].tile.x, pieces[i].tile.y).second) {
						this->tpl.joins.push_back({pieces[i - 1].tile, pieces[i - 1].trackdir, pieces[i].tile, before.cls, before.ycls});
					}
				} else {
					/* The two ends of one free link are handled by the link's own rules. */
					const bool ends_of_free_link = std::ranges::any_of(this->tpl.links, [&](const JunctionNodeLink &link) {
						return link.free && link.from == w.governing[i - 1] && link.to == w.governing[i] && before.tile.x == pieces[i - 1].tile.x && before.tile.y == pieces[i - 1].tile.y && before.trackdir == pieces[i - 1].trackdir && after.tile.x == pieces[i].tile.x && after.tile.y == pieces[i].tile.y && after.trackdir == pieces[i].trackdir;
					});
					if (!ends_of_free_link) this->tpl.open_joins++;
				}
			}
			if (w.link_of[i] >= 0) continue;
			const Track track = TrackdirToTrack(pieces[i].trackdir);
			if (!piece_seen.emplace(pieces[i].tile.x, pieces[i].tile.y, track).second) continue;
			this->tpl.pieces.push_back({pieces[i].tile, track, pieces[i].on_bridge, this->tpl.points[w.governing[i]].cls, this->tpl.points[w.governing[i]].ycls});
		}
	}

	/* The bridges with the class of their heads. */
	for (size_t bi = 0; bi < this->input.bridges.size(); bi++) {
		const JunctionBridge &bridge = this->input.bridges[bi];
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
	const size_t path_count = this->input.paths.size();

	/* The switch piece of every event, and whether a merge is a prio merge, as ExtractJunctionChains reads them. */
	std::vector<std::vector<bool>> prio(path_count);
	std::vector<std::vector<int>> joiner_rows(path_count); // Per event, the row of the joiner's piece before a prio merge minus the merge's row.
	for (size_t pi = 0; pi < path_count; pi++) {
		const JunctionPath &path = this->input.paths[pi];
		for (const SwitchEvent &event : this->work[pi].events) {
			bool is_prio = false;
			int joiner_row = 0;
			const JunctionPiece &merge_piece = path.pieces[event.second];
			if (!event.first && this->input.through[pi]) {
				for (size_t qi = 0; qi < path_count && !is_prio; qi++) {
					if (qi == pi) continue;
					for (const SwitchEvent &other : this->work[qi].events) {
						const JunctionPiece &piece = this->input.paths[qi].pieces[other.second];
						if (other.first || piece.tile.x != merge_piece.tile.x || piece.tile.y != merge_piece.tile.y || piece.trackdir == merge_piece.trackdir) continue;
						is_prio = this->input.paths[qi].movement != path.movement;
						if (other.second > 0) joiner_row = this->input.paths[qi].pieces[other.second - 1].tile.y - merge_piece.tile.y;
						break;
					}
				}
			}
			prio[pi].push_back(is_prio);
			joiner_rows[pi].push_back(joiner_row);
		}
	}

	const int cover = 2 * static_cast<int>(this->spec.prio_tiles);
	for (size_t pi = 0; pi < path_count; pi++) {
		const JunctionPath &path = this->input.paths[pi];
		const JunctionNodePath &node_path = this->tpl.paths[pi];
		const NodePathWork &w = this->work[pi];

		/* The rule of §2 between the points p before q with the flat unit difference U (§6): with no X run between them and a Y run or a free diagonal, a Y difference now; with an X run between them, a late gap (an X difference after the Y snap). */
		auto add_gap = [&](uint32_t p, int unit_p, uint32_t q, int unit_q, int g) {
			const int u = unit_q - unit_p;
			const auto ip = std::ranges::find(node_path.points, p) - node_path.points.begin();
			const auto iq = std::ranges::find(node_path.points, q) - node_path.points.begin();
			bool has_x = false;
			bool has_y = false;
			int d_x = 0;
			int d_y = 0;
			bool mixed_x = false;
			bool mixed_y = false;
			const auto add_sign = [](int &sign, bool &mixed, int value) {
				if (value == 0) return;
				if (sign != 0 && sign != value) mixed = true;
				sign = value;
			};
			std::vector<uint32_t> diagonals;
			std::vector<uint32_t> y_links;
			for (auto k = ip; k < iq; k++) {
				const auto link_it = std::ranges::find_if(this->tpl.links, [&](const JunctionNodeLink &l) { return l.from == node_path.points[static_cast<size_t>(k)] && l.to == node_path.points[static_cast<size_t>(k) + 1]; });
				if (link_it == this->tpl.links.end() || !link_it->free) continue;
				if (link_it->diagonal) {
					has_y = true;
					const uint32_t diagonal_index = static_cast<uint32_t>(std::ranges::find_if(this->tpl.diagonals, [&](const JunctionNodeDiagonal &diag) { return diag.link == static_cast<uint32_t>(link_it - this->tpl.links.begin()); }) - this->tpl.diagonals.begin());
					diagonals.push_back(diagonal_index);
					add_sign(d_y, mixed_y, this->tpl.diagonals[diagonal_index].dy);
				} else if (link_it->axis == Axis::X) {
					has_x = true;
					add_sign(d_x, mixed_x, link_it->dir);
				} else {
					has_y = true;
					add_sign(d_y, mixed_y, link_it->dir);
					y_links.push_back(static_cast<uint32_t>(link_it - this->tpl.links.begin()));
				}
			}
			/* A late gap needs only its free X links to agree; the free Y links and diagonals enter per link. */
			if (mixed_x || (!has_x && mixed_y)) {
				this->Fail(fmt::format("path {} points {},{}: moves both ways along {}", pi, p, q, mixed_x ? "x" : "y"));
				return;
			}
			if (has_x) {
				this->tpl.late_gaps.push_back({static_cast<uint32_t>(pi), p, q, g, u, std::move(diagonals), static_cast<int8_t>(d_x), std::move(y_links)});
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
			if (!this->AddDirected(link->dir, this->At(link->from, link->axis), this->At(link->to, link->axis), 1)) this->Fail(fmt::format("path {}: a free link joins one class", pi));
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
			/* The reserved prio row (§8), opposite the joiner; none when the joiner's piece is on the merge's row. */
			const uint32_t merge = w.governing[mi];
			if (joiner_rows[pi][e] != 0 && std::ranges::none_of(this->tpl.prio_rows, [&](const JunctionNodePrioRow &row) { return row.merge == merge; })) {
				const int merge_dx = TileIndexDiffCByDiagDir(TrackdirToExitdir(path.pieces[mi].trackdir)).x;
				if (merge_dx == 0) this->Fail(fmt::format("path {}: prio merge not on an X piece", pi));
				this->tpl.prio_rows.push_back({merge, static_cast<int8_t>(joiner_rows[pi][e] > 0 ? -1 : 1), static_cast<int8_t>(merge_dx), static_cast<uint8_t>(this->spec.prio_tiles)});
			}
			for (const size_t ci : curve_pieces) {
				if (w.start[ci] < w.start[mi]) add_gap(w.governing[ci], w.start[ci], w.governing[mi], w.start[mi], cover);
			}
			for (const SwitchEvent &split : w.events) {
				if (split.first && w.start[split.second] < w.start[mi]) add_gap(w.governing[split.second], w.start[split.second], w.governing[mi], w.start[mi], cover + 1);
			}
		}

		/* A slot is a piece that can take a signal in the input (the checker's legal or already signalled pieces, off the switches). */
		const auto is_slot = [&](size_t i) {
			const JunctionPiece &piece = path.pieces[i];
			if (std::ranges::any_of(w.events, [&](const SwitchEvent &e) { return e.second == i; })) return false;
			return this->input.slots[pi][i];
		};
		/* The greatest slot strictly between pieces a and b, or SIZE_MAX. */
		const auto last_slot = [&](size_t a, size_t b) {
			for (size_t i = b; i-- > a + 1;) {
				if (is_slot(i)) return i;
			}
			return SIZE_MAX;
		};
		/* The slot's end unit is a constant distance from the merge only when no free link lies between the slot and the merge but the slot's own run or free diagonal (the diagonal's end stays at the slot's distance k, see keep_slot). */
		const auto slot_is_fixed = [&](size_t slot, size_t merge) {
			for (size_t i = slot + 1; i < merge; i++) {
				if (w.link_of[i] >= 0 && w.link_of[i] != w.link_of[slot]) return false;
			}
			return true;
		};
		/* The slot stays an interior piece of the free link it lies in: a run keeps one piece; a free diagonal keeps k + 2 halves, k the slot's distance to the diagonal's last half piece (a static Y bound like rule 1's). */
		const auto keep_slot = [&](size_t slot) {
			if (w.link_of[slot] < 0) return;
			const NodeSpan &span = this->spans[static_cast<size_t>(w.link_of[slot])];
			if (span.diagonal == UINT32_MAX) {
				if (!this->AddDirected(span.dir, this->At(span.from, span.axis), this->At(span.to, span.axis), 2)) this->Fail(fmt::format("path {}: a run keeping its last slot joins one class", pi));
				return;
			}
			size_t end = slot + 1;
			while (end < path.pieces.size() && w.link_of[end] == w.link_of[slot]) end++;
			const JunctionNodeDiagonal &diag = this->tpl.diagonals[span.diagonal];
			const int m_min = CeilHalf(static_cast<int>(end - slot) + 2 - diag.halves);
			const NodeCoord cp = this->AtY(span.from);
			const NodeCoord cq = this->AtY(span.to);
			if (!this->AddDirected(diag.dy, cp, cq, diag.dy * (cq.pos - cp.pos) + m_min)) this->Fail(fmt::format("path {}: a free diagonal keeping its last slot joins one class", pi));
		};
		/* The gap g of a unit rule from the switch piece `from` to the merge piece `to` that keeps the units from after `from` to the end of `slot` at least 2 TL. */
		const auto add_room = [&](size_t from, size_t slot, size_t to) {
			if (!slot_is_fixed(slot, to)) {
				this->Fail(fmt::format("path {}: slot {} before piece {} has a free link between it and the merge", pi, slot, to));
				return;
			}
			keep_slot(slot);
			const int g = 2 * static_cast<int>(this->spec.tl) + path.pieces[from].units + (w.start[to] - (w.start[slot] + path.pieces[slot].units));
			add_gap(w.governing[from], w.start[from], w.governing[to], w.start[to], g);
		};

		/* Rule 4, mirroring CheckSignals' switch and bridge isolation: from a switch or a bridge to the next one, the last slot keeps an interior piece when it lies inside a free X or Y run. */
		std::vector<size_t> anchors;
		for (size_t i = 0; i < path.pieces.size(); i++) {
			if (path.pieces[i].on_bridge || std::ranges::any_of(w.events, [&](const SwitchEvent &e) { return e.second == i; })) anchors.push_back(i);
		}
		for (size_t k = 0; k + 1 < anchors.size(); k++) {
			const size_t slot = last_slot(anchors[k], anchors[k + 1]);
			if (slot != SIZE_MAX) keep_slot(slot);
		}

		/* Rule 5, mirroring CheckWaitingRoom :850-880: from the first split to the first merge after it, the units after the split up to the last slot are at least 2 TL. */
		if (this->input.exit[pi]) {
			const auto split = std::ranges::find_if(w.events, [](const SwitchEvent &e) { return e.first; });
			const auto merge = split == w.events.end() ? split : std::find_if(w.events.begin(), w.events.end(), [&](const SwitchEvent &e) { return !e.first && e.second > split->second; });
			if (split != w.events.end() && merge != w.events.end()) {
				const size_t slot = last_slot(split->second, merge->second);
				if (slot != SIZE_MAX) add_room(split->second, slot, merge->second);
			}
		}

		/* Rule 6, mirroring CheckWaitingRoom :890-912: before every queue merge, a bay of 2 TL between the last split before the merge's signal and that signal. */
		if (!this->input.through[pi]) {
			for (const SwitchEvent &event : w.events) {
				if (event.first) continue;
				size_t slot = SIZE_MAX;
				for (size_t i = event.second; i-- > 0;) {
					if (is_slot(i)) {
						slot = i;
						break;
					}
				}
				if (slot == SIZE_MAX) continue;
				std::optional<size_t> upstream;
				for (const SwitchEvent &other : w.events) {
					if (other.first && other.second < slot) upstream = std::max(upstream.value_or(0), other.second);
				}
				if (upstream.has_value()) add_room(*upstream, slot, event.second);
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

		for (size_t qi = 0; qi < this->input.paths.size(); qi++) {
			const std::vector<JunctionPiece> &pieces = this->input.paths[qi].pieces;
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
					const int d = span.dir;
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

JunctionNodeInput TraceJunctionNodeInput(const JunctionLayout &flat, const JunctionSpec &spec, const JunctionTopology &topology)
{
	const JunctionCheckContext ctx = MakeJunctionTopologyCheckContext(flat, spec, topology, FindJunctionPorts(flat, topology));
	JunctionNodeInput input;
	input.paths = ctx.paths;
	const std::map<std::pair<int, int>, TrackBits> used = UsedTracks(ctx);
	input.bridges = flat.bridges;
	for (const JunctionPath &path : ctx.paths) {
		input.through.push_back(IsThroughPath(ctx, path));
		input.exit.push_back(IsExitPath(ctx, path));
		input.events.push_back(SwitchEventsOf(flat, used, path));
		std::vector<bool> slots;
		for (const JunctionPiece &piece : path.pieces) slots.push_back(IsLegalSignalPiece(flat, piece) || IsSignalledPiece(flat, piece));
		input.slots.push_back(std::move(slots));
	}
	return input;
}

/**
 * The lean of each piece's bend on a route (D212, D215): +1 toward the route's travel, -1 against, 0 where the piece is on no leaning bend.
 * A bend is the non-bridge pieces of one emitting token. Kinds: a fan on the route's first edge +1, on its last edge -1; the SBend of a doubled leg's copy 1 +1 (toward its bridge heads); the SBend of its copy 0 -1; any other SBend +1 when no later token of its edge emits on this route but an earlier one does, -1 when an earlier one does not but a later one does. Corners, Over and Under never lean.
 */
static std::vector<int8_t> LeansOfRoute(const JunctionTrackPrograms &programs, const JunctionTrackRoute &route)
{
	using StepKind = JunctionTrackStep::Kind;
	std::vector<int8_t> lean(route.pieces.size(), 0);
	if (route.pieces.empty()) return lean;
	const uint32_t first_edge = route.tags.front().edge;
	const uint32_t last_edge = route.tags.back().edge;
	/* The least and greatest emitting token of each edge on this route. */
	std::map<uint32_t, uint16_t> lo;
	std::map<uint32_t, uint16_t> hi;
	for (const JunctionTrackPieceTag &tag : route.tags) {
		if (tag.token == JunctionTrackPieceTag::RUN) continue;
		const auto [it, fresh] = lo.emplace(tag.edge, tag.token);
		if (!fresh) it->second = std::min(it->second, tag.token);
		const auto [jt, fresh_hi] = hi.emplace(tag.edge, tag.token);
		if (!fresh_hi) jt->second = std::max(jt->second, tag.token);
	}
	for (size_t i = 0; i < route.pieces.size(); i++) {
		const JunctionTrackPieceTag &tag = route.tags[i];
		if (tag.token == JunctionTrackPieceTag::RUN || route.pieces[i].on_bridge) continue;
		const std::vector<JunctionTrackStep> &program = tag.copy == 1 && !programs.copies[tag.edge].empty() ? programs.copies[tag.edge] : programs.edges[tag.edge];
		const JunctionTrackStep &step = program[tag.token];
		int8_t direction = 0;
		if (step.kind == StepKind::Fan) {
			const bool in = tag.edge == first_edge && tag.token == lo[tag.edge];
			const bool out = tag.edge == last_edge && tag.token == hi[tag.edge];
			if (in != out) direction = in ? 1 : -1;
		} else if (step.kind == StepKind::SBend) {
			if (tag.copy == 1 && !programs.copies[tag.edge].empty()) {
				direction = 1;
			} else if (!programs.copies[tag.edge].empty()) {
				direction = -1;
			} else {
				const bool before = lo[tag.edge] < tag.token;
				const bool after = hi[tag.edge] > tag.token;
				if (!after && before) {
					direction = 1;
				} else if (!before && after) {
					direction = -1;
				}
			}
		}
		lean[i] = direction;
	}
	return lean;
}

JunctionNodeInput JunctionNodeInputOfGraph(const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, const JunctionTrackStructure &structure, const JunctionTrackPrograms &programs, const JunctionTrackPlacement &placement, const std::vector<JunctionTrackRoute> &routes)
{
	using StepKind = JunctionTrackStep::Kind;
	const bool is_t = &topology == &JunctionTTopology();
	const std::vector<JunctionPortSlot> port_slots = JunctionPortSlots(topology);
	auto same = [](TileIndexDiffC a, TileIndexDiffC b) { return a.x == b.x && a.y == b.y; };
	JunctionNodeInput input;

	/* Bridges: the runs of bridge pieces of one token, in route-appearance order, near head first by x + y (Q6). */
	for (const JunctionTrackRoute &route : routes) {
		for (size_t i = 0; i < route.pieces.size();) {
			if (!route.pieces[i].on_bridge) {
				i++;
				continue;
			}
			size_t j = i;
			while (j + 1 < route.pieces.size() && route.pieces[j + 1].on_bridge && route.tags[j + 1].edge == route.tags[i].edge && route.tags[j + 1].token == route.tags[i].token) j++;
			JunctionBridge bridge{route.pieces[i].tile, route.pieces[j].tile};
			if (bridge.start.x + bridge.start.y > bridge.end.x + bridge.end.y) std::swap(bridge.start, bridge.end);
			const bool present = std::ranges::any_of(input.bridges, [&](const JunctionBridge &b) { return same(b.start, bridge.start) && same(b.end, bridge.end); });
			if (!present) input.bridges.push_back(bridge);
			i = j + 1;
		}
	}

	/* The tracks of every route per tile, and the bridge heads, for the slot rule (Q5). */
	std::map<std::pair<int, int>, TrackBits> occupied;
	for (const JunctionTrackRoute &route : routes) {
		for (const JunctionPiece &piece : route.pieces) {
			if (!piece.on_bridge) occupied[{piece.tile.x, piece.tile.y}].Set(TrackdirToTrack(piece.trackdir));
		}
	}
	std::vector<TileIndexDiffC> heads;
	for (const std::array<JunctionBridge, 2> &copies : placement.bridges) {
		for (const JunctionBridge &bridge : copies) {
			if (same(bridge.start, bridge.end)) continue;
			heads.push_back(bridge.start);
			heads.push_back(bridge.end);
		}
	}

	/* The checker's order (Q1): the in port's side (W, E, S) and place along it, then the trackdirs from the in port. */
	auto key_of = [&](const JunctionTrackRoute &route) {
		const uint16_t from = paths[route.path].from;
		const JunctionSide side = topology.groups[port_slots[from].group].side;
		const int rank = side == JunctionSide::W ? 0 : side == JunctionSide::E ? 1 : side == JunctionSide::S ? 2 : 3;
		const TileIndexDiffC port = placement.ports[from];
		return std::pair<int, int>{rank, side == JunctionSide::W || side == JunctionSide::E ? port.y : port.x};
	};
	std::vector<size_t> order(routes.size());
	for (size_t i = 0; i < order.size(); i++) order[i] = i;
	std::ranges::stable_sort(order, [&](size_t a, size_t b) {
		const std::pair<int, int> ka = key_of(routes[a]);
		const std::pair<int, int> kb = key_of(routes[b]);
		if (ka != kb) return ka < kb;
		return std::ranges::lexicographical_compare(routes[a].pieces, routes[b].pieces, [](const JunctionPiece &x, const JunctionPiece &y) { return static_cast<int>(x.trackdir) < static_cast<int>(y.trackdir); });
	});

	for (const size_t ri : order) {
		const JunctionTrackRoute &route = routes[ri];
		const JunctionLanePath &lane = paths[route.path];
		input.paths.push_back({is_t ? TRoleOfSlot(lane.from) : JunctionPortRole::EIn, is_t ? TRoleOfSlot(lane.to) : JunctionPortRole::EIn, route.pieces, lane.from, lane.to, lane.movement});
		bool through = false;
		bool exit = false;
		if (lane.movement != UINT16_MAX) {
			const auto &[in, out] = topology.movements[lane.movement];
			through = topology.groups[in].main && topology.groups[out].main;
			exit = topology.groups[in].main && !topology.groups[out].main;
		}
		input.through.push_back(through);
		input.exit.push_back(exit);

		/* Events: from the route's tags (JunctionRouteEvents). */
		std::vector<SwitchEvent> events = JunctionRouteEvents(graph, programs, routes, ri);
		input.events.push_back(std::move(events));

		/* Slots (Q5): not on a bridge, not a bridge head, no overlapping tracks on the tile. */
		std::vector<bool> slots;
		for (const JunctionPiece &piece : route.pieces) {
			const bool head = std::ranges::any_of(heads, [&](TileIndexDiffC h) { return same(h, piece.tile); });
			slots.push_back(!piece.on_bridge && !head && !TracksOverlap(occupied[{piece.tile.x, piece.tile.y}]));
		}
		input.slots.push_back(std::move(slots));
		input.tags.push_back(route.tags);
		input.routes.emplace_back(route.path, route.copy);
		input.lean.push_back(LeansOfRoute(programs, route));
	}
	return input;
}

JunctionNodeInput TraceJunctionNodeInput(const JunctionLayout &flat, const JunctionSpec &spec)
{
	return TraceJunctionNodeInput(flat, spec, JunctionTTopology());
}

void NodeBuilder::ReadLeans()
{
	this->tpl.lean.assign(this->tpl.flat.size(), 0);
	if (this->input.tags.empty() || this->input.lean.empty()) return;

	using BendId = std::tuple<uint32_t, uint16_t, uint8_t>;
	using PieceId = std::tuple<int, int, Trackdir>;
	const auto is_bend_piece = [](const JunctionPiece &piece, const JunctionTrackPieceTag &tag) { return tag.token != JunctionTrackPieceTag::RUN && !piece.on_bridge; };
	const auto straight_axis = [](Trackdir td) { return td == Trackdir::X_NE || td == Trackdir::X_SW || td == Trackdir::Y_SE || td == Trackdir::Y_NW; };

	/* The pieces of each token, and the run pieces of each edge and copy, over every route. */
	std::map<BendId, std::set<PieceId>> bend_pieces;
	std::map<std::pair<uint32_t, uint8_t>, std::set<PieceId>> run_pieces;
	for (size_t pi = 0; pi < this->input.paths.size(); pi++) {
		const std::vector<JunctionPiece> &pieces = this->input.paths[pi].pieces;
		for (size_t i = 0; i < pieces.size(); i++) {
			const JunctionTrackPieceTag &tag = this->input.tags[pi][i];
			if (is_bend_piece(pieces[i], tag)) bend_pieces[{tag.edge, tag.token, tag.copy}].emplace(pieces[i].tile.x, pieces[i].tile.y, pieces[i].trackdir);
			if (tag.token == JunctionTrackPieceTag::RUN && !pieces[i].on_bridge) run_pieces[{tag.edge, tag.copy}].emplace(pieces[i].tile.x, pieces[i].tile.y, pieces[i].trackdir);
		}
	}

	std::vector<int8_t> direction(this->tpl.flat.size(), 0);
	std::vector<bool> barred(this->tpl.flat.size(), false);
	std::vector<std::set<uint16_t>> bends; ///< The classes of each bend.
	for (size_t pi = 0; pi < this->input.paths.size(); pi++) {
		const std::vector<JunctionPiece> &pieces = this->input.paths[pi].pieces;
		const std::vector<JunctionTrackPieceTag> &tags = this->input.tags[pi];
		const std::vector<int8_t> &leans = this->input.lean[pi];
		/* A bend is the pieces of one token on this route with a nonzero lean. */
		std::map<BendId, std::vector<size_t>> routes_bends;
		for (size_t i = 0; i < pieces.size(); i++) {
			if (leans[i] != 0 && is_bend_piece(pieces[i], tags[i])) routes_bends[{tags[i].edge, tags[i].token, tags[i].copy}].push_back(i);
		}
		for (const auto &[tag_id, members] : routes_bends) {
			const auto &[edge, token, copy] = tag_id;
			/* The bend's travel axis: the nearest straight piece, looking back from its first piece, then forward. */
			std::optional<Trackdir> heading;
			for (size_t i = members.front() + 1; i-- > 0 && !heading.has_value();) {
				if (straight_axis(pieces[i].trackdir)) heading = pieces[i].trackdir;
			}
			for (size_t i = members.front(); i < pieces.size() && !heading.has_value(); i++) {
				if (straight_axis(pieces[i].trackdir)) heading = pieces[i].trackdir;
			}
			if (!heading.has_value()) continue;
			const bool x_axis = *heading == Trackdir::X_NE || *heading == Trackdir::X_SW;
			const TileIndexDiffC step = TileIndexDiffCByDir(TrackdirToDirection(*heading));
			const int travel = x_axis ? step.x : step.y;
			const int8_t lean = static_cast<int8_t>(leans[members.front()] * travel);

			/* Every class on the axis that holds a piece of the bend leans. */
			std::set<uint16_t> classes;
			for (const size_t i : members) {
				const JunctionNodePoint &governing = this->tpl.points[this->work[pi].governing[i]];
				classes.insert(x_axis ? governing.cls : governing.ycls);
			}

			/* A node must never slide: every point of each class is a piece of the bend or a run piece of its edge and copy; all-or-nothing per bend. */
			const std::set<PieceId> &bend = bend_pieces.at(tag_id);
			const std::set<PieceId> &run = run_pieces[{edge, copy}];
			for (const uint16_t cls : classes) {
				for (const JunctionNodePoint &point : this->tpl.points) {
					if ((x_axis ? point.cls : point.ycls) != cls) continue;
					const PieceId id{point.tile.x, point.tile.y, point.trackdir};
					if (!bend.contains(id) && !run.contains(id)) barred[cls] = true;
				}
				if (direction[cls] != 0 && direction[cls] != lean) barred[cls] = true;
				direction[cls] = lean;
			}
			bends.push_back(std::move(classes));
		}
	}
	for (const std::set<uint16_t> &classes : bends) {
		if (std::ranges::any_of(classes, [&](uint16_t cls) { return barred[cls]; })) {
			for (const uint16_t cls : classes) barred[cls] = true;
		}
	}
	for (size_t c = 0; c < direction.size(); c++) {
		if (!barred[c]) this->tpl.lean[c] = direction[c];
	}
}

JunctionNodeTemplate BuildJunctionNodeTemplate(const JunctionLayout &flat, const JunctionSpec &spec)
{
	return BuildJunctionNodeTemplate(TraceJunctionNodeInput(flat, spec), spec);
}

JunctionNodeTemplate BuildJunctionNodeTemplate(const JunctionNodeInput &input, const JunctionSpec &spec)
{
	JunctionNodeTemplate tpl{};
	NodeBuilder builder{tpl, spec, input, {}, {}, {}};
	builder.ReadPoints();
	if (!tpl.error.empty()) return tpl;
	builder.ReadLeans();
	builder.AddPathDiffs();
	builder.AddCrossings();
	builder.AddBoxEdges();

	/* Rule 5: the class of the last piece of the first exit path (for the T, EIn->SOut). */
	size_t anchor_index = 0;
	while (anchor_index < input.exit.size() && !input.exit[anchor_index]) anchor_index++;
	if (anchor_index == input.exit.size() || tpl.paths[anchor_index].points.empty()) {
		tpl.anchor = UINT16_MAX;
		tpl.anchor_y = UINT16_MAX;
	} else {
		const JunctionNodePath &anchor = tpl.paths[anchor_index];
		tpl.anchor = tpl.points[anchor.points.back()].cls;
		tpl.anchor_y = tpl.points[anchor.points.front()].ycls;
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

	/* Each late gap: d_x * (x_q - x_p) >= d_x * Fx + ceil((g - U - yfree - 2 sum(m) + 2 d_x sum(dx m)) / 2), yfree the free Y links' 2 s (dy_to - dy_from). */
	for (const JunctionNodeLateGap &gap : tpl.late_gaps) {
		const JunctionNodePoint &p = tpl.points[gap.p];
		const JunctionNodePoint &q = tpl.points[gap.q];
		auto shift_y = [&](uint16_t ycls) { return values[ycls] - tpl.flat[ycls]; };
		const int d_x = gap.dir;
		int yfree = 0;
		for (const uint32_t l : gap.y_links) yfree += 2 * tpl.links[l].dir * (shift_y(tpl.points[tpl.links[l].to].ycls) - shift_y(tpl.points[tpl.links[l].from].ycls));
		int steps = 0;
		int steps_x = 0;
		for (const uint32_t diag : gap.diagonals) {
			const int m = NodeDiagonalSteps(tpl, tpl.diagonals[diag], values);
			steps += m;
			steps_x += tpl.diagonals[diag].dx * m;
		}
		const int k = d_x * (q.tile.x - p.tile.x) + CeilHalf(gap.g - gap.u - yfree - 2 * steps + 2 * d_x * steps_x);
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

bool AddJunctionNodeOccupancy(const JunctionNodeTemplate &tpl, JunctionPositions &positions, const std::vector<int> &values, int &row, std::array<uint16_t, 2> *classes)
{
	constexpr int NO_FLAT_ROW = INT_MIN;
	/** One end of an element: x = values[var] + offset. */
	struct End {
		uint16_t var;
		int offset;
	};
	/** A run of tiles on one current row, with its place in the flat layout. */
	struct Element {
		int flat_row; ///< NO_FLAT_ROW when the element has no flat tile.
		End lo;
		End hi;
		int flat_lo;
		int flat_hi;
		TrackBits bits; ///< The track bits of its tiles (a group: the union).
		std::vector<uint32_t> owners; ///< The free links the element belongs to: its own link for a raster or run tile, the links it is an end point of for a piece; empty for bridge heads (a group: the union).
	};
	std::map<int, std::vector<Element>> rows;
	std::vector<uint32_t> owners;
	auto add = [&](int current_row, int flat_row, End lo, End hi, Track track) {
		rows[current_row].push_back({flat_row, lo, hi, tpl.flat[lo.var] + lo.offset, tpl.flat[hi.var] + hi.offset, TrackBits{track}, owners});
	};
	auto row_of = [&](uint16_t ycls, int y) { return values[ycls] + y - tpl.flat[ycls]; };
	auto at_least = [&](const End &a, const End &b, int k) {
		return NodeAddAtLeast(tpl, positions, a.var, a.offset + tpl.flat[a.var], b.var, b.offset + tpl.flat[b.var], k);
	};

	for (const JunctionNodePiece &piece : tpl.pieces) {
		if (piece.on_bridge) continue;
		const End x{piece.cls, piece.tile.x - tpl.flat[piece.cls]};
		owners.clear();
		for (uint32_t i = 0; i < tpl.links.size(); i++) {
			if (!tpl.links[i].free) continue;
			for (const uint32_t end : {tpl.links[i].from, tpl.links[i].to}) {
				const JunctionNodePoint &point = tpl.points[end];
				if (point.tile.x == piece.tile.x && point.tile.y == piece.tile.y) owners.push_back(i);
			}
		}
		add(row_of(piece.ycls, piece.tile.y), piece.tile.y, x, x, piece.track);
	}
	owners.clear();
	for (const JunctionNodeBridge &node_bridge : tpl.bridges) {
		for (const TileIndexDiffC &head : {node_bridge.bridge.start, node_bridge.bridge.end}) {
			const End x{node_bridge.cls, head.x - tpl.flat[node_bridge.cls]};
			add(row_of(node_bridge.ycls, head.y), head.y, x, x, node_bridge.bridge.start.x == node_bridge.bridge.end.x ? Track::Y : Track::X);
		}
	}
	for (const JunctionNodeLink &link : tpl.links) {
		if (!link.free) continue;
		owners.assign(1, static_cast<uint32_t>(&link - tpl.links.data()));
		const JunctionNodePoint &p = tpl.points[link.from];
		const JunctionNodePoint &q = tpl.points[link.to];
		if (link.diagonal) {
			const JunctionNodeDiagonal &diag = *std::ranges::find_if(tpl.diagonals, [&](const JunctionNodeDiagonal &d) { return &tpl.links[d.link] == &link; });
			const JunctionRaster raster = NodeDiagonalRaster(tpl, diag, diag.halves + 2 * NodeDiagonalSteps(tpl, diag, values));
			const JunctionRaster flat_raster = NodeDiagonalRaster(tpl, diag, diag.halves);
			if (!raster.error.empty() || !flat_raster.error.empty()) return false;
			for (size_t i = 1; i + 1 < raster.pieces.size(); i++) {
				const TileIndexDiffC &tile = raster.pieces[i].tile;
				const End x{p.cls, tile.x - tpl.flat[p.cls]};
				add(row_of(p.ycls, tile.y), i + 1 < flat_raster.pieces.size() ? flat_raster.pieces[i].tile.y : NO_FLAT_ROW, x, x, TrackdirToTrack(raster.pieces[i].trackdir));
			}
		} else if (link.axis == Axis::X) {
			const bool p_first = p.tile.x <= q.tile.x;
			const JunctionNodePoint &first = p_first ? p : q;
			const JunctionNodePoint &last = p_first ? q : p;
			if (first.tile.x + 1 > last.tile.x - 1) continue;
			add(row_of(p.ycls, p.tile.y), p.tile.y, {first.cls, first.tile.x - tpl.flat[first.cls] + 1}, {last.cls, last.tile.x - tpl.flat[last.cls] - 1}, Track::X);
		} else {
			const int sign = q.tile.y > p.tile.y ? 1 : -1;
			const int y_p = row_of(p.ycls, p.tile.y);
			const int y_q = row_of(q.ycls, q.tile.y);
			const int flat_count = std::abs(q.tile.y - p.tile.y) - 1;
			const End x{p.cls, p.tile.x - tpl.flat[p.cls]};
			for (int k = 1; k <= sign * (y_q - y_p) - 1; k++) add(y_p + sign * k, k <= flat_count ? p.tile.y + sign * k : NO_FLAT_ROW, x, x, Track::Y);
		}
	}

	/* The prio rows (§8): the merge's tile and span tiles upstream, on the row beside the merge's. */
	owners.clear();
	for (const JunctionNodePrioRow &prio : tpl.prio_rows) {
		const JunctionNodePoint &merge = tpl.points[prio.merge];
		const End at_merge{merge.cls, merge.tile.x - tpl.flat[merge.cls]};
		const End upstream{merge.cls, merge.tile.x - prio.span * prio.dir - tpl.flat[merge.cls]};
		add(row_of(merge.ycls, merge.tile.y) + prio.side, merge.tile.y + prio.side, prio.dir > 0 ? upstream : at_merge, prio.dir > 0 ? at_merge : upstream, Track::X);
	}

	for (auto &[current_row, elements] : rows) {
		/* Order by the least solution of the X system so far (all classes at Lo: a solution of min-closed constraints), ties by the flat order. */
		const auto now = [&](const End &end) { return positions.Lo(end.var) + end.offset; };
		std::ranges::stable_sort(elements, [&](const Element &a, const Element &b) { return std::make_tuple(now(a.lo), a.flat_lo, a.flat_hi, a.flat_row) < std::make_tuple(now(b.lo), b.flat_lo, b.flat_hi, b.flat_row); });

		/* Elements with one flat tile on one current row form a group: equal x, one entry in the chain. */
		std::vector<Element> chain;
		for (const Element &element : elements) {
			if (!chain.empty()) {
				Element &first = chain.back();
				if (element.flat_row != NO_FLAT_ROW && element.flat_lo == element.flat_hi && first.flat_row == element.flat_row && first.flat_lo == element.flat_lo && first.flat_hi == element.flat_hi) {
					first.bits.Set(element.bits);
					first.owners.insert(first.owners.end(), element.owners.begin(), element.owners.end());
					if (!at_least(element.lo, first.lo, 0) || !at_least(first.lo, element.lo, 0)) {
						row = current_row;
						if (classes != nullptr) *classes = {first.lo.var, element.lo.var};
						return false;
					}
					continue;
				}
			}
			chain.push_back(element);
		}

		for (size_t i = 0; i + 1 < chain.size(); i++) {
			const Element &a = chain[i];
			const Element &b = chain[i + 1];
			if (std::ranges::any_of(a.owners, [&](uint32_t owner) { return std::ranges::find(b.owners, owner) != b.owners.end(); })) continue; // One free link's own tiles and end points: one owner.
			const bool flat_ordered = a.flat_row != NO_FLAT_ROW && a.flat_row == b.flat_row && a.flat_hi < b.flat_lo;
			if (now(a.hi) >= now(b.lo) && !flat_ordered) {
				row = current_row;
				if (classes != nullptr) *classes = {a.lo.var, b.lo.var};
				return false;
			}
			int gap = 1;
			if (a.flat_row != NO_FLAT_ROW && a.flat_row == b.flat_row) {
				gap = std::min(2, b.flat_lo - a.flat_hi);
			} else {
				/* Different flat rows: one empty tile only when the tracks touch across the facing edges. */
				const bool touch = a.bits.Any(DiagdirReachesTracks(ReverseDiagDir(DiagDirection::SW))) && b.bits.Any(DiagdirReachesTracks(ReverseDiagDir(DiagDirection::NE)));
				gap = touch ? 2 : 1;
			}
			if (!at_least(b.lo, a.hi, gap)) {
				row = current_row;
				if (classes != nullptr) *classes = {a.lo.var, b.lo.var};
				return false;
			}
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
	int row = 0;
	if (!AddJunctionNodeLateConstraints(tpl, positions, values) || !AddJunctionNodeOccupancy(tpl, positions, values, row)) return {};
	for (size_t c = 0; c < tpl.x_classes; c++) {
		if (!fix(c)) return {};
	}
	return values;
}

/** The tracks and bridges at the values, in the template's frame (not normalised). */
static JunctionLayout RenderJunctionNodeTracksRaw(const JunctionNodeTemplate &tpl, const std::vector<int> &values)
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
	return layout;
}

JunctionLayout RenderJunctionNodeTracks(const JunctionNodeTemplate &tpl, const std::vector<int> &values)
{
	JunctionLayout layout = RenderJunctionNodeTracksRaw(tpl, values);
	NormaliseJunctionLayout(layout);
	return layout;
}

std::vector<JunctionTrackRoute> RenderJunctionNodeRoutes(const JunctionNodeTemplate &tpl, const JunctionNodeInput &input, const std::vector<int> &values)
{
	auto shift = [&](uint16_t cls) { return values[cls] - tpl.flat[cls]; };
	std::map<std::tuple<int, int, int>, const JunctionNodePiece *> piece_of;
	for (const JunctionNodePiece &piece : tpl.pieces) piece_of.emplace(std::tuple{piece.tile.x, piece.tile.y, static_cast<int>(piece.track)}, &piece);

	std::vector<JunctionTrackRoute> routes;
	for (size_t i = 0; i < tpl.paths.size(); i++) {
		const std::vector<JunctionPiece> &pieces = input.paths[i].pieces;
		const std::vector<JunctionTrackPieceTag> &tags = input.tags[i];
		const JunctionNodePath &node_path = tpl.paths[i];
		JunctionTrackRoute route{input.routes[i].first, input.routes[i].second, {}, {}};

		/* The input piece of each point. */
		std::vector<size_t> at;
		for (const uint32_t point : node_path.points) {
			size_t k = at.empty() ? 0 : at.back() + 1;
			const JunctionNodePoint &p = tpl.points[point];
			while (k < pieces.size() && !(pieces[k].tile.x == p.tile.x && pieces[k].tile.y == p.tile.y && pieces[k].trackdir == p.trackdir)) k++;
			if (k == pieces.size()) return {};
			at.push_back(k);
		}
		if (at.empty()) return {};

		auto emit = [&](JunctionPiece piece, const JunctionTrackPieceTag &tag) {
			route.pieces.push_back(piece);
			route.tags.push_back(tag);
		};
		/* A piece of a rigid run, shifted by its classes (a bridge piece by its bridge's). */
		auto emit_rigid = [&](size_t k) {
			JunctionPiece piece = pieces[k];
			int dx;
			int dy;
			if (piece.on_bridge) {
				const auto it = std::ranges::find_if(tpl.bridges, [&](const JunctionNodeBridge &b) {
					const JunctionBridge &br = b.bridge;
					if (br.start.x == br.end.x) return piece.tile.x == br.start.x && piece.tile.y >= std::min(br.start.y, br.end.y) && piece.tile.y <= std::max(br.start.y, br.end.y);
					return br.start.y == br.end.y && piece.tile.y == br.start.y && piece.tile.x >= std::min(br.start.x, br.end.x) && piece.tile.x <= std::max(br.start.x, br.end.x);
				});
				if (it == tpl.bridges.end()) return false;
				dx = shift(it->cls);
				dy = shift(it->ycls);
			} else {
				const auto it = piece_of.find({piece.tile.x, piece.tile.y, static_cast<int>(TrackdirToTrack(piece.trackdir))});
				if (it == piece_of.end()) return false;
				dx = shift(it->second->cls);
				dy = shift(it->second->ycls);
			}
			piece.tile.x = static_cast<int16_t>(piece.tile.x + dx);
			piece.tile.y = static_cast<int16_t>(piece.tile.y + dy);
			emit(piece, tags[k]);
			return true;
		};

		if (!emit_rigid(at[0])) return {};
		for (size_t j = 0; j + 1 < at.size(); j++) {
			const size_t a = at[j];
			const size_t b = at[j + 1];
			const auto link_it = std::ranges::find_if(tpl.links, [&](const JunctionNodeLink &l) { return l.from == node_path.points[j] && l.to == node_path.points[j + 1]; });
			if (link_it == tpl.links.end()) return {};
			const JunctionNodeLink &link = *link_it;
			if (!link.free) {
				for (size_t k = a + 1; k < b; k++) {
					if (!emit_rigid(k)) return {};
				}
			} else {
				const JunctionNodePoint &from = tpl.points[link.from];
				const JunctionNodePoint &to = tpl.points[link.to];
				JunctionTrackPieceTag tag = tags[a];
				tag.token = JunctionTrackPieceTag::RUN;
				if (a + 1 < b) tag = tags[a + 1];
				if (link.diagonal) {
					const auto diag_it = std::ranges::find_if(tpl.diagonals, [&](const JunctionNodeDiagonal &d) { return &tpl.links[d.link] == &link; });
					if (diag_it == tpl.diagonals.end()) return {};
					const JunctionRaster raster = NodeDiagonalRaster(tpl, *diag_it, diag_it->halves + 2 * NodeDiagonalSteps(tpl, *diag_it, values));
					for (size_t k = 1; k + 1 < raster.pieces.size(); k++) {
						JunctionPiece piece = raster.pieces[k];
						piece.tile.x = static_cast<int16_t>(piece.tile.x + shift(from.cls));
						piece.tile.y = static_cast<int16_t>(piece.tile.y + shift(from.ycls));
						emit(piece, tag);
					}
				} else {
					JunctionPiece piece = pieces[a];
					const int x_from = from.tile.x + shift(from.cls);
					const int y_from = from.tile.y + shift(from.ycls);
					const int x_to = to.tile.x + shift(to.cls);
					const int y_to = to.tile.y + shift(to.ycls);
					if (link.axis == Axis::X) {
						const int step = x_to > x_from ? 1 : -1;
						for (int x = x_from + step; x != x_to; x += step) {
							piece.tile = {static_cast<int16_t>(x), static_cast<int16_t>(y_from)};
							emit(piece, tag);
						}
					} else {
						const int step = y_to > y_from ? 1 : -1;
						for (int y = y_from + step; y != y_to; y += step) {
							piece.tile = {static_cast<int16_t>(x_from), static_cast<int16_t>(y)};
							emit(piece, tag);
						}
					}
				}
			}
			if (!emit_rigid(b)) return {};
		}
		routes.push_back(std::move(route));
	}
	std::ranges::stable_sort(routes, [](const JunctionTrackRoute &a, const JunctionTrackRoute &b) { return std::pair{a.path, a.copy} < std::pair{b.path, b.copy}; });
	return routes;
}

bool JunctionNodeTrackFits(const JunctionFacts &facts, int x, int y, TrackBits bits)
{
	int entry;
	int exit;
	if (bits == TrackBits{Track::X}) return facts.Straight(x, y, Axis::X, entry, exit);
	if (bits == TrackBits{Track::Y}) return facts.Straight(x, y, Axis::Y, entry, exit);
	return facts.Fits(x, y, bits);
}

/** Whether piece a (travelling out of its tile along a_td, its tile built with a_bits) meets the tile b (built with b_bits) at one height: both EdgeZ exist and are equal (DESIGN-N3 §10). */
static bool NodeJoinMeets(const JunctionFacts &facts, int ax, int ay, TrackBits a_bits, Trackdir a_td, int bx, int by, TrackBits b_bits)
{
	const DiagDirection e = TrackdirToExitdir(a_td);
	const int z_out = facts.EdgeZ(ax, ay, a_bits, e);
	const int z_in = facts.EdgeZ(bx, by, b_bits, ReverseDiagDir(e));
	return z_out != INT_MIN && z_in != INT_MIN && z_out == z_in;
}

/** The trackdir of the track that leaves its tile through the edge. */
static Trackdir TrackdirThrough(Track track, DiagDirection edge)
{
	const Trackdir td = TrackToTrackdir(track);
	return TrackdirToExitdir(td) == edge ? td : ReverseTrackdir(td);
}

/** The track that touches both edges. */
static Track NodeCorner(DiagDirection a, DiagDirection b)
{
	for (Track track : {Track::Upper, Track::Lower, Track::Left, Track::Right}) {
		const Trackdir td = TrackToTrackdir(track);
		const DiagDirection e1 = TrackdirToExitdir(td);
		const DiagDirection e2 = TrackdirToExitdir(ReverseTrackdir(td));
		if ((e1 == a || e2 == a) && (e1 == b || e2 == b)) return track;
	}
	NOT_REACHED();
}

/** Add a graph element to a list once. */
static void NodeAddTag(std::vector<JunctionTrackPieceTag> &tags, const JunctionTrackPieceTag &tag)
{
	if (std::ranges::find(tags, tag) == tags.end()) tags.push_back(tag);
}

/** The elements of a free link: its link_tags entry. */
static std::vector<JunctionTrackPieceTag> NodeLinkElements(const JunctionNodeTemplate &tpl, uint32_t link)
{
	std::vector<JunctionTrackPieceTag> tags;
	if (tpl.link_tags.empty()) return tags;
	tags.push_back(tpl.link_tags[link]);
	return tags;
}

/** The tags of the points of a class (X or Y). */
static std::vector<JunctionTrackPieceTag> NodeClassElements(const JunctionNodeTemplate &tpl, uint16_t cls)
{
	std::vector<JunctionTrackPieceTag> tags;
	if (tpl.point_tags.empty()) return tags;
	for (size_t i = 0; i < tpl.points.size(); i++) {
		if (tpl.points[i].cls == cls || tpl.points[i].ycls == cls) NodeAddTag(tags, tpl.point_tags[i]);
	}
	return tags;
}

/** The solve: the template's positions on the facts, rendered, signalled and normalised by `finish`. */
static JunctionNodeSolve SolveJunctionNodesWith(const JunctionNodeTemplate &tpl, const JunctionFacts &facts, TileIndexDiffC offset, const std::function<JunctionLayout(const std::vector<int> &)> &finish)
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
	assert(tpl.lean.size() == variables);
	JunctionPositions positions(variables + 3 * free_links.size(), -RANGE, RANGE);

	/* 2. The template. */
	if (!AddJunctionNodeConstraints(tpl, positions)) {
		result.failure = JunctionNodeFailure{Kind::Template};
		return result;
	}

	/* The tracks of each flat tile: those of the render at the flat values. */
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
	auto bits_at = [&](TileIndexDiffC tile) { return flat_bits.at({tile.x, tile.y}); };
	auto shift_y = [&](uint16_t ycls) { return result.values[ycls] - tpl.flat[ycls]; };

	/* Fix a variable at the nearest value to its flat one that accept takes; false when none is left. */
	result.values.assign(variables, 0);
	auto snap = [&](size_t c, const auto &accept) {
		for (int distance = 0;; distance++) {
			const int low = tpl.flat[c] - distance;
			const int high = tpl.flat[c] + distance;
			if (low < positions.Lo(c) && high > positions.Up(c)) return false;
			for (const int t : {low, high}) {
				if (t < positions.Lo(c) || t > positions.Up(c)) continue;
				const size_t mark = positions.Mark();
				if (positions.RestrictRange(c, t, t)) {
					result.values[c] = positions.Lo(c);
					if (accept(c)) return true;
				}
				positions.Undo(mark);
				if (distance == 0) break;
			}
		}
	};
	/* Fix a leaning class (D212) at its greatest (lean +1) or least value that accept takes, stepping back toward its flat value as far as it; then, or when its lean side is the domain limit, the nearest value to flat as snap; false when none is left. */
	auto snap_lean = [&](size_t c, const auto &accept) {
		const int lean = tpl.lean[c];
		if (lean > 0 ? positions.Up(c) >= RANGE : positions.Lo(c) <= -RANGE) return snap(c, accept);
		for (int t = lean > 0 ? positions.Up(c) : positions.Lo(c); lean > 0 ? t >= tpl.flat[c] : t <= tpl.flat[c]; t -= lean) {
			if (t < positions.Lo(c) || t > positions.Up(c)) continue;
			const size_t mark = positions.Mark();
			if (positions.RestrictRange(c, t, t)) {
				result.values[c] = positions.Lo(c);
				if (accept(c)) return true;
			}
			positions.Undo(mark);
		}
		return snap(c, accept);
	};
	/* snap for a class without a lean, snap_lean for one with. */
	auto snap_class = [&](size_t c, const auto &accept) { return tpl.lean[c] == 0 ? snap(c, accept) : snap_lean(c, accept); };
	/* The classes in [first, last) without a lean in class order, then those with one. */
	auto snap_order = [&](size_t first, size_t last) {
		std::vector<size_t> order;
		for (const bool leaning : {false, true}) {
			for (size_t c = first; c < last; c++) {
				if ((tpl.lean[c] != 0) == leaning) order.push_back(c);
			}
		}
		return order;
	};
	const auto any_value = [](size_t) { return true; };

	/* Whether the join meets with its class at value v (x shift v - flat) and its Y class at its fixed value. */
	auto join_meets = [&](const JunctionNodeJoin &join, int v) {
		const int dx = v - tpl.flat[join.cls] + offset.x;
		const int dy = shift_y(join.ycls) + offset.y;
		return NodeJoinMeets(facts, join.from_tile.x + dx, join.from_tile.y + dy, bits_at(join.from_tile), join.from_td, join.to_tile.x + dx, join.to_tile.y + dy, bits_at(join.to_tile));
	};

	/* Fits and meets that note the working tiles of a failure in bad when it is set. */
	auto fits_into = [&](int x, int y, TrackBits bits, std::vector<TileIndexDiffC> *bad) {
		if (JunctionNodeTrackFits(facts, x, y, bits)) return true;
		if (bad != nullptr) bad->push_back({static_cast<int16_t>(x), static_cast<int16_t>(y)});
		return false;
	};
	auto meets_into = [&](int x1, int y1, TrackBits bits1, Trackdir td, int x2, int y2, TrackBits bits2, std::vector<TileIndexDiffC> *bad) {
		if (NodeJoinMeets(facts, x1, y1, bits1, td, x2, y2, bits2)) return true;
		if (bad != nullptr) {
			bad->push_back({static_cast<int16_t>(x1), static_cast<int16_t>(y1)});
			bad->push_back({static_cast<int16_t>(x2), static_cast<int16_t>(y2)});
		}
		return false;
	};

	/**
	 * Whether the reserved tiles of a prio row (DESIGN-N3 §8) fit the facts and meet, with the merge's X class at value v and its Y class fixed.
	 * PlacePrio lays on the row (M = the merge tile, h1 = Corner(side, exit) on M, h2 = Corner(-side, -exit) on the row):
	 * step 0 (x_M): h2 alone; steps 1..span: the lone X, or X with h2 at a join tile. The joins are at the required blocks, so every step is checked with both sets.
	 * Meets: consecutive row tiles (the upstream lone X into the next tile's bits, step 1 into step 0's h2), and the row tile at x_M into the merge tile built with its flat bits and h1.
	 */
	auto prio_ok = [&](const JunctionNodePrioRow &prio, int v, std::vector<TileIndexDiffC> *bad = nullptr) {
		const JunctionNodePoint &merge = tpl.points[prio.merge];
		const int dx = v - tpl.flat[merge.cls] + offset.x;
		const int dy = shift_y(merge.ycls) + offset.y;
		DiagDirection exit_edge = DiagDirection::Begin;
		DiagDirection side_edge = DiagDirection::Begin;
		for (DiagDirection edge = DiagDirection::Begin; edge < DiagDirection::End; edge++) {
			const TileIndexDiffC diff = TileIndexDiffCByDiagDir(edge);
			if (diff.y == 0 && diff.x == prio.dir) exit_edge = edge;
			if (diff.x == 0 && diff.y == prio.side) side_edge = edge;
		}
		const Track h1 = NodeCorner(side_edge, exit_edge);
		const Track h2 = NodeCorner(ReverseDiagDir(side_edge), ReverseDiagDir(exit_edge));
		const TrackBits lone{Track::X};
		const TrackBits with_half{Track::X, h2};
		const TrackBits half{h2};
		const Trackdir along = TrackdirThrough(Track::X, exit_edge);
		const int row = merge.tile.y + prio.side + dy;
		auto x_at = [&](int step) { return merge.tile.x - step * prio.dir + dx; };
		bool ok = true;
		for (int step = 0; step <= prio.span; step++) {
			if (step == 0) {
				if (!fits_into(x_at(0), row, half, bad)) {
					if (bad == nullptr) return false;
					ok = false;
				}
				continue;
			}
			if (!fits_into(x_at(step), row, lone, bad)) {
				if (bad == nullptr) return false;
				ok = false;
			}
			if (!fits_into(x_at(step), row, with_half, bad)) {
				if (bad == nullptr) return false;
				ok = false;
			}
			if (!meets_into(x_at(step), row, lone, along, x_at(step - 1), row, step == 1 ? half : lone, bad)) {
				if (bad == nullptr) return false;
				ok = false;
			}
		}
		const TrackBits merge_bits = bits_at(merge.tile) | TrackBits{h1};
		return meets_into(x_at(0), row, half, TrackdirThrough(h2, ReverseDiagDir(side_edge)), merge.tile.x + dx, merge.tile.y + dy, merge_bits, bad) && ok;
	};

	/* Whether a free diagonal's interior tiles (the raster at its length now) fit lone tracks and the chain p -> raster[1] -> ... -> raster[n-2] -> q meets, with p's class at value v; q's x is x_p + k as AddJunctionNodeLateConstraints computes it. */
	auto diagonal_ok = [&](const JunctionNodeDiagonal &diag, const JunctionRaster &raster, int v, std::vector<TileIndexDiffC> *bad = nullptr) {
		if (!raster.error.empty()) return false;
		const JunctionNodePoint &p = tpl.points[tpl.links[diag.link].from];
		const JunctionNodePoint &q = tpl.points[tpl.links[diag.link].to];
		const int dx = v - tpl.flat[p.cls] + offset.x;
		const int dy = shift_y(p.ycls) + offset.y;
		int before_x = p.tile.x + dx;
		int before_y = p.tile.y + dy;
		TrackBits before_bits = bits_at(p.tile);
		Trackdir before_td = p.trackdir;
		bool ok = true;
		for (size_t i = 1; i + 1 < raster.pieces.size(); i++) {
			const JunctionPiece &piece = raster.pieces[i];
			const int x = piece.tile.x + dx;
			const int y = piece.tile.y + dy;
			const TrackBits bits{TrackdirToTrack(piece.trackdir)};
			if (!fits_into(x, y, bits, bad)) {
				if (bad == nullptr) return false;
				ok = false;
			}
			if (!meets_into(before_x, before_y, before_bits, before_td, x, y, bits, bad)) {
				if (bad == nullptr) return false;
				ok = false;
			}
			before_x = x;
			before_y = y;
			before_bits = bits;
			before_td = piece.trackdir;
		}
		const int k = (q.tile.x - p.tile.x) + diag.dx * NodeDiagonalSteps(tpl, diag, result.values);
		return meets_into(before_x, before_y, before_bits, before_td, p.tile.x + dx + k, q.tile.y + shift_y(q.ycls) + offset.y, bits_at(q.tile), bad) && ok;
	};

	/* Whether a Y-free link is a column at p's class value v: every interior row is a straight Y piece, consecutive rows connect, and so do p -> first interior and last interior -> q (p -> q when there is none). */
	auto column_ok = [&](const JunctionNodeLink &link, int v, std::vector<TileIndexDiffC> *bad = nullptr) {
		const JunctionNodePoint &p = tpl.points[link.from];
		const JunctionNodePoint &q = tpl.points[link.to];
		const int column = p.tile.x + v - tpl.flat[p.cls] + offset.x;
		const int y_p = p.tile.y + shift_y(p.ycls) + offset.y;
		const int y_q = q.tile.y + shift_y(q.ycls) + offset.y;
		const int lo = std::min(y_p, y_q);
		const int hi = std::max(y_p, y_q);
		int previous_exit = 0;
		bool ok = true;
		for (int r = lo + 1; r < hi; r++) {
			int entry = 0;
			int exit = 0;
			if (!facts.Straight(column, r, Axis::Y, entry, exit)) {
				if (bad == nullptr) return false;
				bad->push_back({static_cast<int16_t>(column), static_cast<int16_t>(r)});
				ok = false;
			} else if (r > lo + 1 && previous_exit != entry) {
				if (bad == nullptr) return false;
				bad->push_back({static_cast<int16_t>(column), static_cast<int16_t>(r)});
				ok = false;
			}
			previous_exit = exit;
		}
		const int step = y_q > y_p ? 1 : -1;
		const TrackBits lone{Track::Y};
		const int first = hi - lo > 1 ? y_p + step : y_q;
		const int last = hi - lo > 1 ? y_q - step : y_p;
		if (!meets_into(column, y_p, bits_at(p.tile), p.trackdir, column, first, hi - lo > 1 ? lone : bits_at(q.tile), bad)) {
			if (bad == nullptr) return false;
			ok = false;
		}
		return (hi - lo <= 1 || meets_into(column, last, lone, p.trackdir, column, y_q, bits_at(q.tile), bad)) && ok;
	};

	/* The x direction of an X-free link on its paths. */
	auto link_direction = [&](const JunctionNodeLink &link) {
		return static_cast<int>(link.dir);
	};

	/* The snapshot's range of values of an X class, within its bounds now. */
	auto x_values = [&](uint16_t c) { return std::pair{std::max(positions.Lo(c), facts.MinX() - offset.x), std::min(positions.Up(c), facts.MaxX() - offset.x)}; };

	/* Whether the Y class c, now fixed with the Y classes before it, leaves a row for everything it carries (DESIGN-N3 §10): its rigid groups, the free diagonals and Y-free links whose two Y classes are fixed. */
	auto y_fits = [&](size_t c) {
		std::set<uint16_t> groups;
		for (const JunctionNodePiece &piece : tpl.pieces) {
			if (piece.ycls == c && !piece.on_bridge) groups.insert(piece.cls);
		}
		for (const uint16_t cx : groups) {
			const auto [lo, hi] = x_values(cx);
			bool any = false;
			for (int v = lo; !any && v <= hi; v++) {
				any = std::ranges::all_of(tpl.pieces, [&](const JunctionNodePiece &piece) {
					if (piece.cls != cx || piece.ycls != c || piece.on_bridge) return true;
					return JunctionNodeTrackFits(facts, piece.tile.x + v - tpl.flat[cx] + offset.x, piece.tile.y + shift_y(piece.ycls) + offset.y, bits_at(piece.tile));
				}) && std::ranges::all_of(tpl.joins, [&](const JunctionNodeJoin &join) { return join.cls != cx || join.ycls != c || join_meets(join, v); })
					&& std::ranges::all_of(tpl.prio_rows, [&](const JunctionNodePrioRow &prio) { const JunctionNodePoint &merge = tpl.points[prio.merge]; return merge.cls != cx || merge.ycls != c || prio_ok(prio, v); });
			}
			if (!any) return false;
		}
		for (const JunctionNodeDiagonal &diag : tpl.diagonals) {
			const JunctionNodePoint &p = tpl.points[tpl.links[diag.link].from];
			const JunctionNodePoint &q = tpl.points[tpl.links[diag.link].to];
			if (p.ycls > c || q.ycls > c || (p.ycls != c && q.ycls != c)) continue;
			const JunctionRaster raster = NodeDiagonalRaster(tpl, diag, diag.halves + 2 * NodeDiagonalSteps(tpl, diag, result.values));
			const auto [lo, hi] = x_values(p.cls);
			bool any = false;
			for (int v = lo; !any && v <= hi; v++) any = diagonal_ok(diag, raster, v);
			if (!any) return false;
		}
		for (const JunctionNodeLink &link : tpl.links) {
			if (!link.free || link.axis != Axis::Y || link.diagonal) continue;
			const JunctionNodePoint &p = tpl.points[link.from];
			const JunctionNodePoint &q = tpl.points[link.to];
			if (p.ycls > c || q.ycls > c || (p.ycls != c && q.ycls != c)) continue;
			const auto [lo, hi] = x_values(p.cls);
			bool any = false;
			for (int v = lo; !any && v <= hi; v++) any = column_ok(link, v);
			if (!any) return false;
		}
		return true;
	};

	/* Where the Y class c, fixed at row, has no x (the first failing part in y_fits' order) and which working tiles fail there; empty when every part has an x. */
	auto height_fault = [&](size_t c, int row) -> std::optional<JunctionNodeHeightFault> {
		JunctionNodeHeightFault fault;
		fault.row = row;
		auto finish = [&](JunctionNodeHeightFault::Part part, uint32_t index, uint16_t cls, auto &&collect) {
			fault.part = part;
			fault.index = index;
			const auto [lo, hi] = x_values(cls);
			fault.x = lo <= hi ? std::clamp(static_cast<int>(tpl.flat[cls]), lo, hi) : static_cast<int>(tpl.flat[cls]);
			if (lo <= hi) collect(fault.x);
			std::vector<TileIndexDiffC> unique;
			for (const TileIndexDiffC &tile : fault.tiles) {
				if (std::ranges::none_of(unique, [&](const TileIndexDiffC &u) { return u.x == tile.x && u.y == tile.y; })) unique.push_back(tile);
			}
			fault.tiles = std::move(unique);
			return fault;
		};
		std::set<uint16_t> groups;
		for (const JunctionNodePiece &piece : tpl.pieces) {
			if (piece.ycls == c && !piece.on_bridge) groups.insert(piece.cls);
		}
		for (const uint16_t cx : groups) {
			const auto [lo, hi] = x_values(cx);
			bool any = false;
			for (int v = lo; !any && v <= hi; v++) {
				any = std::ranges::all_of(tpl.pieces, [&](const JunctionNodePiece &piece) {
					if (piece.cls != cx || piece.ycls != c || piece.on_bridge) return true;
					return JunctionNodeTrackFits(facts, piece.tile.x + v - tpl.flat[cx] + offset.x, piece.tile.y + shift_y(piece.ycls) + offset.y, bits_at(piece.tile));
				}) && std::ranges::all_of(tpl.joins, [&](const JunctionNodeJoin &join) { return join.cls != cx || join.ycls != c || join_meets(join, v); })
					&& std::ranges::all_of(tpl.prio_rows, [&](const JunctionNodePrioRow &prio) { const JunctionNodePoint &merge = tpl.points[prio.merge]; return merge.cls != cx || merge.ycls != c || prio_ok(prio, v); });
			}
			if (any) continue;
			return finish(JunctionNodeHeightFault::Part::Group, cx, cx, [&](int v) {
				for (const JunctionNodePiece &piece : tpl.pieces) {
					if (piece.cls != cx || piece.ycls != c || piece.on_bridge) continue;
					fits_into(piece.tile.x + v - tpl.flat[cx] + offset.x, piece.tile.y + shift_y(piece.ycls) + offset.y, bits_at(piece.tile), &fault.tiles);
				}
				for (const JunctionNodeJoin &join : tpl.joins) {
					if (join.cls != cx || join.ycls != c) continue;
					const int dx = v - tpl.flat[join.cls] + offset.x;
					const int dy = shift_y(join.ycls) + offset.y;
					meets_into(join.from_tile.x + dx, join.from_tile.y + dy, bits_at(join.from_tile), join.from_td, join.to_tile.x + dx, join.to_tile.y + dy, bits_at(join.to_tile), &fault.tiles);
				}
				for (const JunctionNodePrioRow &prio : tpl.prio_rows) {
					const JunctionNodePoint &merge = tpl.points[prio.merge];
					if (merge.cls == cx && merge.ycls == c) prio_ok(prio, v, &fault.tiles);
				}
			});
		}
		for (size_t d = 0; d < tpl.diagonals.size(); d++) {
			const JunctionNodeDiagonal &diag = tpl.diagonals[d];
			const JunctionNodePoint &p = tpl.points[tpl.links[diag.link].from];
			const JunctionNodePoint &q = tpl.points[tpl.links[diag.link].to];
			if (p.ycls > c || q.ycls > c || (p.ycls != c && q.ycls != c)) continue;
			const JunctionRaster raster = NodeDiagonalRaster(tpl, diag, diag.halves + 2 * NodeDiagonalSteps(tpl, diag, result.values));
			const auto [lo, hi] = x_values(p.cls);
			bool any = false;
			for (int v = lo; !any && v <= hi; v++) any = diagonal_ok(diag, raster, v);
			if (any) continue;
			return finish(JunctionNodeHeightFault::Part::Diagonal, static_cast<uint32_t>(d), p.cls, [&](int v) { diagonal_ok(diag, raster, v, &fault.tiles); });
		}
		for (size_t l = 0; l < tpl.links.size(); l++) {
			const JunctionNodeLink &link = tpl.links[l];
			if (!link.free || link.axis != Axis::Y || link.diagonal) continue;
			const JunctionNodePoint &p = tpl.points[link.from];
			const JunctionNodePoint &q = tpl.points[link.to];
			if (p.ycls > c || q.ycls > c || (p.ycls != c && q.ycls != c)) continue;
			const auto [lo, hi] = x_values(p.cls);
			bool any = false;
			for (int v = lo; !any && v <= hi; v++) any = column_ok(link, v);
			if (any) continue;
			return finish(JunctionNodeHeightFault::Part::Column, static_cast<uint32_t>(l), p.cls, [&](int v) { column_ok(link, v, &fault.tiles); });
		}
		return std::nullopt;
	};

	/* 2b. The Y variables: the nearest flat rows whose pieces fit the facts and meet at their heights. */
	for (const size_t c : snap_order(classes, variables)) {
		std::optional<int> first_row;
		if (!snap_class(c, [&](size_t y_class) {
			if (!first_row.has_value()) first_row = positions.Lo(y_class);
			return y_fits(y_class);
		})) {
			result.failure = JunctionNodeFailure{Kind::Height, UINT32_MAX, 0, static_cast<uint16_t>(c)};
			result.failure->elements = NodeClassElements(tpl, static_cast<uint16_t>(c));
			if (first_row.has_value()) {
				const size_t mark = positions.Mark();
				const bool restricted = positions.RestrictRange(c, *first_row, *first_row);
				if (restricted) {
					result.values[c] = positions.Lo(c);
					result.failure->height = height_fault(c, *first_row);
					if (!tpl.point_tags.empty() && result.failure->height.has_value()) {
						const JunctionNodeHeightFault &fault = *result.failure->height;
						std::vector<uint32_t> part; ///< The points of the failing part.
						uint16_t part_cls = UINT16_MAX; ///< The X class that fault.x is a value of.
						if (fault.part == JunctionNodeHeightFault::Part::Group) {
							part_cls = static_cast<uint16_t>(fault.index);
							for (uint32_t i = 0; i < tpl.points.size(); i++) {
								if (tpl.points[i].cls == part_cls && tpl.points[i].ycls == c) part.push_back(i);
							}
						} else {
							const JunctionNodeLink &link = tpl.links[fault.part == JunctionNodeHeightFault::Part::Diagonal ? tpl.diagonals[fault.index].link : fault.index];
							part_cls = tpl.points[link.from].cls;
							part = {link.from, link.to};
						}
						for (const uint32_t i : part) {
							const JunctionNodePoint &point = tpl.points[i];
							if (point.cls != part_cls) continue;
							const int x = point.tile.x + fault.x - tpl.flat[part_cls] + offset.x;
							const int y = point.tile.y + shift_y(point.ycls) + offset.y;
							if (std::ranges::any_of(fault.tiles, [&](const TileIndexDiffC &tile) { return tile.x == x && tile.y == y; })) NodeAddTag(result.failure->elements, tpl.point_tags[i]);
						}
						if (result.failure->elements.empty()) {
							for (const uint32_t i : part) NodeAddTag(result.failure->elements, tpl.point_tags[i]);
						}
					}
				}
				positions.Undo(mark);
			}
			result.values.clear();
			return result;
		}
	}

	/* 2c. The constraints that need the Y values: the free diagonals' x, the late gaps, the late crossings. */
	if (!AddJunctionNodeLateConstraints(tpl, positions, result.values)) {
		result.failure = JunctionNodeFailure{Kind::Template};
		result.values.clear();
		return result;
	}

	/* 2d. The occupancy chain (DESIGN-N3 §7). */
	int occupancy_row = 0;
	std::array<uint16_t, 2> occupancy_classes{UINT16_MAX, UINT16_MAX};
	if (!AddJunctionNodeOccupancy(tpl, positions, result.values, occupancy_row, &occupancy_classes)) {
		result.failure = JunctionNodeFailure{Kind::Occupancy, UINT32_MAX, occupancy_row};
		for (const uint16_t cls : occupancy_classes) {
			if (cls == UINT16_MAX) continue;
			for (const JunctionTrackPieceTag &tag : NodeClassElements(tpl, cls)) NodeAddTag(result.failure->elements, tag);
		}
		result.values.clear();
		return result;
	}

	/* The interior of every free diagonal at its length now, as rasters from the flat tile of its first point. */
	std::vector<JunctionRaster> diagonal_rasters;
	for (const JunctionNodeDiagonal &diag : tpl.diagonals) diagonal_rasters.push_back(NodeDiagonalRaster(tpl, diag, diag.halves + 2 * NodeDiagonalSteps(tpl, diag, result.values)));

	/* 3. The pieces of every class fit the facts, and so do the interior tiles of the free diagonals from their rasters at the known length; a tile's bits are those of the render at the flat values. Every join of the class, free run end, column and diagonal chain of the class meets. */
	for (uint16_t c = 0; c < classes; c++) {
		std::vector<int> allowed;
		for (int v = -RANGE; v <= RANGE; v++) {
			bool fits = std::ranges::all_of(tpl.pieces, [&](const JunctionNodePiece &piece) {
				if (piece.cls != c || piece.on_bridge) return true;
				return JunctionNodeTrackFits(facts, piece.tile.x + v - tpl.flat[c] + offset.x, piece.tile.y + shift_y(piece.ycls) + offset.y, bits_at(piece.tile));
			});
			fits = fits && std::ranges::all_of(tpl.joins, [&](const JunctionNodeJoin &join) { return join.cls != c || join_meets(join, v); });
			fits = fits && std::ranges::all_of(tpl.prio_rows, [&](const JunctionNodePrioRow &prio) { return tpl.points[prio.merge].cls != c || prio_ok(prio, v); });

			/* A Y-free link of this class is a column. */
			for (size_t i = 0; fits && i < tpl.links.size(); i++) {
				const JunctionNodeLink &link = tpl.links[i];
				if (!link.free || link.axis != Axis::Y || link.diagonal || tpl.points[link.from].cls != c) continue;
				fits = column_ok(link, v);
			}

			/* A free X run of this class: its end meets the lone X straight beside it. When the run ends up with no interior this asks a straight X where q's own tile is: conservative. */
			for (size_t k = 0; fits && k < free_links.size(); k++) {
				const JunctionNodeLink &link = tpl.links[free_links[k]];
				const JunctionNodePoint &p = tpl.points[link.from];
				const JunctionNodePoint &q = tpl.points[link.to];
				const int d = link_direction(link);
				const int row = p.tile.y + shift_y(p.ycls) + offset.y;
				const TrackBits lone{Track::X};
				if (p.cls == c) {
					const int x_p = p.tile.x + v - tpl.flat[c] + offset.x;
					fits = NodeJoinMeets(facts, x_p, row, bits_at(p.tile), p.trackdir, x_p + d, row, lone);
				}
				if (fits && q.cls == c) {
					const int x_q = q.tile.x + v - tpl.flat[c] + offset.x;
					fits = NodeJoinMeets(facts, x_q - d, row, lone, p.trackdir, x_q, row, bits_at(q.tile));
				}
			}

			/* A free diagonal starting at this class: every interior tile of its raster at the known length fits a half piece, and the chain meets. */
			for (size_t d = 0; fits && d < tpl.diagonals.size(); d++) {
				if (tpl.points[tpl.links[tpl.diagonals[d].link].from].cls != c) continue;
				fits = diagonal_ok(tpl.diagonals[d], diagonal_rasters[d], v);
			}
			if (fits) allowed.push_back(v);
		}
		if (allowed.empty() || !positions.RestrictDomain(c, allowed)) {
			result.failure = JunctionNodeFailure{Kind::Piece, UINT32_MAX, 0, c};
			result.failure->elements = NodeClassElements(tpl, c);
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
		const int d = link_direction(link);
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
			int entry;
			int exit;
			int next_entry;
			int next_exit;
			if (!facts.Straight(c, row, Axis::X, entry, exit)) {
				if (!(d > 0 ? positions.AddAvoid(u, w, c) : positions.AddAvoid(w, u, c))) {
					result.failure = JunctionNodeFailure{Kind::Straight, link_index, c};
					result.failure->elements = NodeLinkElements(tpl, link_index);
					result.values.clear();
					return result;
				}
			} else if (c + 1 <= facts.MaxX() && facts.Straight(c + 1, row, Axis::X, next_entry, next_exit) && exit != next_entry) {
				if (!(d > 0 ? positions.AddAvoid(u, w2, c) : positions.AddAvoid(w2, u, c + 1))) {
					result.failure = JunctionNodeFailure{Kind::Break, link_index, c};
					result.failure->elements = NodeLinkElements(tpl, link_index);
					result.values.clear();
					return result;
				}
			}
		}
		if (!positions.RestrictRange(u, facts.MinX(), facts.MaxX())) {
			result.failure = JunctionNodeFailure{Kind::Edge, link_index, facts.MinX() - 1};
					result.failure->elements = NodeLinkElements(tpl, link_index);
			result.values.clear();
			return result;
		}
		if (!positions.RestrictRange(w, facts.MinX(), facts.MaxX())) {
			result.failure = JunctionNodeFailure{Kind::Edge, link_index, facts.MaxX() + 1};
					result.failure->elements = NodeLinkElements(tpl, link_index);
			result.values.clear();
			return result;
		}
	}

	/* 5. Snap each X class to the nearest value. */
	for (const size_t c : snap_order(0, classes)) {
		if (!snap_class(c, any_value)) {
			result.failure = JunctionNodeFailure{Kind::Template};
			result.values.clear();
			return result;
		}
	}

	/* 6. Render. */
	result.layout = finish(result.values);
	return result;
}

JunctionNodeSolve SolveJunctionNodes(const JunctionNodeTemplate &tpl, const JunctionSpec &spec, const JunctionFacts &facts, TileIndexDiffC offset)
{
	return SolveJunctionNodesWith(tpl, facts, offset, [&](const std::vector<int> &values) {
		JunctionLayout layout = RenderJunctionNodeTracks(tpl, values);
		PlaceJunctionSignals(layout, spec);
		NormaliseJunctionLayout(layout);
		return layout;
	});
}

JunctionNodeSolve SolveJunctionNodes(const JunctionNodeTemplate &tpl, const JunctionSpec &spec, const JunctionFacts &facts, TileIndexDiffC offset, const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, const JunctionTrackPrograms &programs, const JunctionNodeInput &input)
{
	TileIndexDiffC origin{};
	JunctionNodeSolve result = SolveJunctionNodesWith(tpl, facts, offset, [&](const std::vector<int> &values) {
		JunctionLayout layout = RenderJunctionNodeTracksRaw(tpl, values);
		SignalJunctionTrackRoutes(layout, RenderJunctionNodeRoutes(tpl, input, values), topology, paths, graph, programs, spec);

		/* NormaliseJunctionLayout shifts by the least tile of the tracks and bridges. */
		int min_x = INT16_MAX;
		int min_y = INT16_MAX;
		for (const JunctionTrack &jt : layout.tracks) {
			min_x = std::min<int>(min_x, jt.offset.x);
			min_y = std::min<int>(min_y, jt.offset.y);
		}
		for (const JunctionBridge &bridge : layout.bridges) {
			for (const TileIndexDiffC head : {bridge.start, bridge.end}) {
				min_x = std::min<int>(min_x, head.x);
				min_y = std::min<int>(min_y, head.y);
			}
		}
		origin = {static_cast<int16_t>(min_x), static_cast<int16_t>(min_y)};
		NormaliseJunctionLayout(layout);
		return layout;
	});
	result.origin = origin;
	return result;
}

bool JunctionNodeHeightsOk(const JunctionLayout &layout, const JunctionSpec &spec, const JunctionTerrain &terrain, TileIndexDiffC offset, std::string *why)
{
	const JunctionCheckContext ctx = MakeJunctionCheckContext(layout, spec, FindJunctionPorts(layout), JunctionTMovements());
	const JunctionFacts facts(terrain);
	std::map<std::pair<int, int>, TrackBits> tile_bits; ///< Per layout tile, all the tracks the layout builds there.
	for (const JunctionTrack &jt : layout.tracks) tile_bits[{jt.offset.x, jt.offset.y}] |= jt.tracks;
	for (size_t i = 0; i < ctx.paths.size(); i++) {
		const std::vector<JunctionPiece> &pieces = ctx.paths[i].pieces;
		const JunctionPiece *before = nullptr; ///< The previous piece on the ground (not on a bridge).
		size_t before_index = 0;
		for (size_t j = 0; j < pieces.size(); j++) {
			const JunctionPiece &piece = pieces[j];
			if (piece.on_bridge) {
				before = nullptr;
				continue;
			}
			const int x = piece.tile.x + offset.x;
			const int y = piece.tile.y + offset.y;
			auto fail = [&](const std::string &reason) {
				if (why != nullptr) *why = fmt::format("path {} piece {} at {},{}: {}", i, j, x, y, reason);
				return false;
			};
			const TrackBits bits = tile_bits[{piece.tile.x, piece.tile.y}];
			if (!facts.Fits(x, y, bits)) return fail("the game cannot build these tracks here");
			if (before != nullptr) {
				const TrackBits before_bits = tile_bits[{before->tile.x, before->tile.y}];
				const DiagDirection exit = TrackdirToExitdir(before->trackdir);
				const int z_out = facts.EdgeZ(before->tile.x + offset.x, before->tile.y + offset.y, before_bits, exit);
				const int z_in = facts.EdgeZ(x, y, bits, ReverseDiagDir(exit));
				if (z_out != z_in) return fail(fmt::format("joins piece {} at height {} but meets it at {}", before_index, z_in, z_out));
			}
			before = &piece;
			before_index = j;
		}
	}
	return true;
}

bool JunctionRoutesHeightsOk(const std::vector<JunctionTrackRoute> &routes, const JunctionLayout &layout, TileIndexDiffC origin, const JunctionTerrain &terrain, TileIndexDiffC offset, std::string *why)
{
	const JunctionFacts facts(terrain);
	std::map<std::pair<int, int>, TrackBits> tile_bits; ///< Per template-frame tile, all the tracks the layout builds there.
	for (const JunctionTrack &jt : layout.tracks) tile_bits[{jt.offset.x + origin.x, jt.offset.y + origin.y}] |= jt.tracks;
	for (size_t i = 0; i < routes.size(); i++) {
		const std::vector<JunctionPiece> &pieces = routes[i].pieces;
		const JunctionPiece *before = nullptr; ///< The previous piece on the ground (not on a bridge).
		size_t before_index = 0;
		for (size_t j = 0; j < pieces.size(); j++) {
			const JunctionPiece &piece = pieces[j];
			if (piece.on_bridge) {
				before = nullptr;
				continue;
			}
			const int x = piece.tile.x + offset.x;
			const int y = piece.tile.y + offset.y;
			auto fail = [&](const std::string &reason) {
				if (why != nullptr) *why = fmt::format("route {} piece {} at {},{}: {}", i, j, x, y, reason);
				return false;
			};
			const TrackBits bits = tile_bits[{piece.tile.x, piece.tile.y}];
			if (!facts.Fits(x, y, bits)) return fail("the game cannot build these tracks here");
			if (before != nullptr) {
				const TrackBits before_bits = tile_bits[{before->tile.x, before->tile.y}];
				const DiagDirection exit = TrackdirToExitdir(before->trackdir);
				const int z_out = facts.EdgeZ(before->tile.x + offset.x, before->tile.y + offset.y, before_bits, exit);
				const int z_in = facts.EdgeZ(x, y, bits, ReverseDiagDir(exit));
				if (z_out != z_in) return fail(fmt::format("joins piece {} at height {} but meets it at {}", before_index, z_in, z_out));
			}
			before = &piece;
			before_index = j;
		}
	}

	/* The search's fits loop: every tile of the layout except a bridge head. */
	for (const JunctionTrack &jt : layout.tracks) {
		const bool bridge_head = std::ranges::any_of(layout.bridges, [&](const JunctionBridge &b) { return (b.start.x == jt.offset.x && b.start.y == jt.offset.y) || (b.end.x == jt.offset.x && b.end.y == jt.offset.y); });
		const int x = jt.offset.x + origin.x + offset.x;
		const int y = jt.offset.y + origin.y + offset.y;
		if (bridge_head || JunctionNodeTrackFits(facts, x, y, jt.tracks)) continue;
		if (why != nullptr) *why = fmt::format("fits: tile {},{}", x, y);
		return false;
	}
	return true;
}

bool ChooseJunctionDetour(const JunctionPiece &p, const JunctionPiece &q, int column, int side, const JunctionDetourBuild &build, JunctionRaster *chosen, std::string *why)
{
	auto fail = [&](const std::string &reason) {
		if (why != nullptr) *why = reason;
		return false;
	};
	const int row = p.tile.y;
	const int d = q.tile.x > p.tile.x ? 1 : -1;
	const int length = d * (q.tile.x - p.tile.x);

	/* The hands of the S-bend. */
	char t1 = 'L';
	char t2 = 'R';
	JunctionRaster probe = RasterizeJunctionRoute({p.tile, p.trackdir, "FLFRF"});
	if (!probe.error.empty() || probe.pieces.back().tile.y != row + side) {
		t1 = 'R';
		t2 = 'L';
		probe = RasterizeJunctionRoute({p.tile, p.trackdir, "FRFLF"});
		if (!probe.error.empty() || probe.pieces.back().tile.y != row + side) return fail("no S-bend to that side");
	}

	/* The lengths of the runs: the first (n1, n2, n3) in the order of the rules whose build takes the raster. */
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

			if (build(full, n2, reason)) {
				*chosen = std::move(full);
				return true;
			}
		}
	}
	return fail(reason.empty() ? "link too short for a detour" : "no buildable detour: " + reason);
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
	const int lo = std::min<int>(p.tile.x, q.tile.x) + 1;
	const int hi = std::max<int>(p.tile.x, q.tile.x) - 1;
	if (column < lo || column > hi) return fail("column outside the link");

	/* The layout with the link's interior replaced by a program's raster, or the reason it cannot be had. */
	std::optional<JunctionLayout> result;
	auto build = [&](JunctionRaster raster, int, std::string &reason) {
		JunctionLayout candidate = layout;
		for (JunctionTrack &jt : candidate.tracks) {
			if (jt.offset.y == row && jt.offset.x >= lo && jt.offset.x <= hi) jt.tracks.Reset(Track::X);
		}
		std::erase_if(candidate.tracks, [](const JunctionTrack &jt) { return jt.tracks.None(); });
		raster.pieces.pop_back();
		raster.pieces.erase(raster.pieces.begin());
		raster.bridges.clear();
		for (const JunctionPiece &piece : raster.pieces) {
			const bool taken = std::ranges::any_of(candidate.tracks, [&](const JunctionTrack &jt) { return jt.offset.x == piece.tile.x && jt.offset.y == piece.tile.y; });
			if (taken) {
				reason = fmt::format("collision at {},{}", piece.tile.x, piece.tile.y);
				return false;
			}
		}
		AddRasterToLayout(candidate, raster);

		/* A shifted run of one piece has no free link: the two S-bends become one rigid class and their curve gap cannot stretch, which the template reports as an error. */
		JunctionLayout normalised = candidate;
		NormaliseJunctionLayout(normalised);
		const JunctionNodeTemplate child = BuildJunctionNodeTemplate(normalised, spec);
		if (!child.error.empty()) {
			reason = "template error: " + child.error;
			return false;
		}
		result = std::move(candidate);
		return true;
	};
	JunctionRaster chosen;
	const JunctionPiece p_piece{p.tile, p.trackdir, 0, false};
	const JunctionPiece q_piece{q.tile, q.trackdir, 0, false};
	if (!ChooseJunctionDetour(p_piece, q_piece, column, side, build, &chosen, why)) return std::nullopt;
	return result;
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
			search.offset = layout_offset;
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

/** The cost of routes (D198): rail_piece per piece off a bridge and turn_cost (half a rail piece) per 45 degree turn, summed over all routes. */
static int64_t GraphStateCost(const std::vector<JunctionTrackRoute> &routes, const JunctionSpec &spec)
{
	const int64_t rail_piece = spec.prices.rail_piece.base();
	const int64_t turn_cost = (spec.prices.rail_piece / 2).base();
	int64_t cost = 0;
	for (const JunctionTrackRoute &route : routes) {
		for (size_t i = 0; i < route.pieces.size(); i++) {
			if (route.pieces[i].on_bridge) continue;
			cost += rail_piece;
			if (i > 0 && !route.pieces[i - 1].on_bridge && TrackdirToDirection(route.pieces[i - 1].trackdir) != TrackdirToDirection(route.pieces[i].trackdir)) cost += turn_cost;
		}
	}
	return cost;
}

/** The edits as text for the log. */
static std::string GraphEditsText(const std::vector<JunctionDetourEdit> &edits)
{
	std::string text = "[";
	for (const JunctionDetourEdit &e : edits) text += fmt::format("({} {} {} {} {} {})", e.edge, e.run, e.copy, e.column, e.side, e.variant);
	return text + "]";
}

JunctionGraphSearch SearchJunctionGraph(const JunctionGraphSeed &seed, const JunctionSpec &spec, const JunctionTerrain &terrain, TileIndexDiffC offset, int max_states)
{
	JunctionGraphSearch search;
	const JunctionFacts facts(terrain);
	std::multimap<int64_t, std::vector<JunctionDetourEdit>> queue; ///< By cost; equal costs in insertion order.
	std::set<std::vector<JunctionDetourEdit>> visited;
	if (seed.routes.empty()) {
		search.log = "no seed routes\n";
		return search;
	}
	std::string why;
	queue.emplace(GraphStateCost(seed.routes, spec), std::vector<JunctionDetourEdit>{});
	visited.insert({});

	while (!queue.empty() && search.states < max_states) {
		const int64_t cost = queue.begin()->first;
		const std::vector<JunctionDetourEdit> edits = std::move(queue.begin()->second);
		queue.erase(queue.begin());
		const std::string head = fmt::format("state {} edits {} cost {}: ", search.states + 1, GraphEditsText(edits), cost);

		/* Rebuilt from the seed and the edits. */
		const std::vector<JunctionTrackRoute> routes = ApplyJunctionDetours(seed.routes, edits, spec, offset, &why);
		if (routes.empty()) {
			search.log += head + "refused: " + why + "\n";
			continue;
		}
		const JunctionNodeInput input = JunctionNodeInputOfGraph(*seed.topology, seed.paths, seed.graph, seed.structure, seed.programs, seed.placement, routes);
		const JunctionNodeTemplate tpl = BuildJunctionNodeTemplate(input, spec);
		if (!tpl.error.empty()) {
			search.log += head + "template error: " + tpl.error + "\n";
			continue;
		}
		const JunctionNodeSolve solve = SolveJunctionNodes(tpl, spec, facts, offset, *seed.topology, seed.paths, seed.graph, seed.programs, input);
		search.states++;

		if (solve.layout.has_value()) {
			if (!JunctionRoutesHeightsOk(RenderJunctionNodeRoutes(tpl, input, solve.values), *solve.layout, solve.origin, terrain, offset, &why)) {
				search.log += head + "heights: " + why + "\n";
				continue;
			}
			search.log += head + "ok\n";
			search.layout = solve.layout;
			search.edits = edits;
			return search;
		}

		const JunctionNodeFailure &failure = *solve.failure;
		search.log += head + fmt::format("failure kind {} link {} column {}\n", to_underlying(failure.kind), failure.link, failure.column);
		if ((failure.kind != JunctionNodeFailure::Kind::Straight && failure.kind != JunctionNodeFailure::Kind::Break) || failure.link >= tpl.link_tags.size()) continue;

		/* The failing run, once: its tag (edge, run, copy); the edit keeps the failing working column. */
		const JunctionTrackPieceTag tag = tpl.link_tags[failure.link];
		if (tag.token != JunctionTrackPieceTag::RUN) continue;
		if (std::ranges::any_of(edits, [&](const JunctionDetourEdit &e) { return e.edge == tag.edge && e.run == tag.run && e.copy == tag.copy; })) continue;

		/* One child per side: the first candidate, in the edit's order, whose template builds (as the legacy detour). */
		for (const int side : {-1, 1}) {
			std::string side_why;
			for (uint16_t variant = 0;; variant++) {
				std::vector<JunctionDetourEdit> child = edits;
				child.push_back({tag.edge, tag.run, tag.copy, failure.column, static_cast<int8_t>(side), variant});
				std::ranges::sort(child);
				const std::vector<JunctionTrackRoute> child_routes = ApplyJunctionDetours(seed.routes, child, spec, offset, &why);
				if (child_routes.empty()) {
					search.log += fmt::format("  detour edge {} run {} copy {} column {} side {}: {}\n", tag.edge, tag.run, tag.copy, failure.column, side, side_why.empty() ? why : side_why);
					break;
				}
				const JunctionNodeInput child_input = JunctionNodeInputOfGraph(*seed.topology, seed.paths, seed.graph, seed.structure, seed.programs, seed.placement, child_routes);
				const JunctionNodeTemplate child_tpl = BuildJunctionNodeTemplate(child_input, spec);
				if (!child_tpl.error.empty()) {
					side_why = "template error: " + child_tpl.error;
					continue;
				}
				if (visited.insert(child).second) queue.emplace(GraphStateCost(child_routes, spec), std::move(child));
				break;
			}
		}
	}
	return search;
}

/** A value as text for a difference report. */
template <typename T>
static std::string NodeDiffText(const T &value)
{
	if constexpr (std::is_enum_v<T>) {
		return fmt::format("{}", static_cast<int64_t>(to_underlying(value)));
	} else if constexpr (std::is_same_v<T, bool>) {
		return value ? "true" : "false";
	} else if constexpr (std::is_same_v<T, std::string>) {
		return value;
	} else {
		return fmt::format("{}", static_cast<int64_t>(value));
	}
}

/** "<name>: <a> vs <b>" when the two differ, else empty. */
template <typename T>
static std::string NodeDiffField(const char *name, const T &a, const T &b)
{
	if (a == b) return {};
	return fmt::format("{}: {} vs {}", name, NodeDiffText(a), NodeDiffText(b));
}

/** The first non-empty difference of a list, else empty. */
static std::string NodeDiffFirst(std::initializer_list<std::string> diffs)
{
	for (const std::string &diff : diffs) {
		if (!diff.empty()) return diff;
	}
	return {};
}

/** The difference of two tiles as "<name>.x" or "<name>.y". */
static std::string NodeDiffTile(const char *name, const TileIndexDiffC &a, const TileIndexDiffC &b)
{
	return NodeDiffFirst({NodeDiffField((std::string(name) + ".x").c_str(), a.x, b.x), NodeDiffField((std::string(name) + ".y").c_str(), a.y, b.y)});
}

/** The difference of two vectors of numbers as "<name>: size <a> vs <b>" or "<name>[<i>]: <a> vs <b>". */
template <typename T>
static std::string NodeDiffNumbers(const char *name, const std::vector<T> &a, const std::vector<T> &b)
{
	if (a.size() != b.size()) return fmt::format("{}: size {} vs {}", name, a.size(), b.size());
	for (size_t i = 0; i < a.size(); i++) {
		if (a[i] != b[i]) return fmt::format("{}[{}]: {} vs {}", name, i, NodeDiffText(a[i]), NodeDiffText(b[i]));
	}
	return {};
}

/** The difference of two vectors of structs as "<name>: size <a> vs <b>" or "<name>[<i>].<field>: ...", the field difference coming from `diff`. */
template <typename T, typename F>
static std::string NodeDiffVector(const char *name, const std::vector<T> &a, const std::vector<T> &b, F diff)
{
	if (a.size() != b.size()) return fmt::format("{}: size {} vs {}", name, a.size(), b.size());
	for (size_t i = 0; i < a.size(); i++) {
		const std::string field = diff(a[i], b[i]);
		if (!field.empty()) return fmt::format("{}[{}].{}", name, i, field);
	}
	return {};
}

static_assert(sizeof(JunctionNodePoint) == 10); // Update DiffJunctionNodeTemplates when this struct changes.
static_assert(sizeof(JunctionNodeLink) == 12); // Update DiffJunctionNodeTemplates when this struct changes.
static_assert(sizeof(JunctionNodePath) == 64); // Update DiffJunctionNodeTemplates when this struct changes.
static_assert(sizeof(JunctionNodePiece) == 10); // Update DiffJunctionNodeTemplates when this struct changes.
static_assert(sizeof(JunctionNodeBridge) == 14); // Update DiffJunctionNodeTemplates when this struct changes.
static_assert(sizeof(JunctionNodeJoin) == 14); // Update DiffJunctionNodeTemplates when this struct changes.
static_assert(sizeof(JunctionNodeDiff) == 8); // Update DiffJunctionNodeTemplates when this struct changes.
static_assert(sizeof(JunctionNodeDiagonal) == 8); // Update DiffJunctionNodeTemplates when this struct changes.
static_assert(sizeof(JunctionNodeLateGap) == 80); // Update DiffJunctionNodeTemplates when this struct changes.
static_assert(sizeof(JunctionNodeLateCrossing) == 8); // Update DiffJunctionNodeTemplates when this struct changes.
static_assert(sizeof(JunctionNodePrioRow) == 8); // Update DiffJunctionNodeTemplates when this struct changes.

std::string DiffJunctionNodeTemplates(const JunctionNodeTemplate &a, const JunctionNodeTemplate &b)
{
	return NodeDiffFirst({
		NodeDiffVector("points", a.points, b.points, [](const JunctionNodePoint &x, const JunctionNodePoint &y) {
			return NodeDiffFirst({NodeDiffTile("tile", x.tile, y.tile), NodeDiffField("trackdir", x.trackdir, y.trackdir), NodeDiffField("cls", x.cls, y.cls), NodeDiffField("ycls", x.ycls, y.ycls)});
		}),
		NodeDiffVector("links", a.links, b.links, [](const JunctionNodeLink &x, const JunctionNodeLink &y) {
			return NodeDiffFirst({NodeDiffField("from", x.from, y.from), NodeDiffField("to", x.to, y.to), NodeDiffField("free", x.free, y.free), NodeDiffField("axis", x.axis, y.axis), NodeDiffField("dir", x.dir, y.dir), NodeDiffField("diagonal", x.diagonal, y.diagonal)});
		}),
		NodeDiffVector("paths", a.paths, b.paths, [](const JunctionNodePath &x, const JunctionNodePath &y) {
			return NodeDiffFirst({NodeDiffField("from", x.from, y.from), NodeDiffField("to", x.to, y.to), NodeDiffNumbers("points", x.points, y.points), NodeDiffNumbers("units", x.units, y.units), NodeDiffField("dir", x.dir, y.dir), NodeDiffField("dir_y", x.dir_y, y.dir_y)});
		}),
		NodeDiffVector("pieces", a.pieces, b.pieces, [](const JunctionNodePiece &x, const JunctionNodePiece &y) {
			return NodeDiffFirst({NodeDiffTile("tile", x.tile, y.tile), NodeDiffField("track", x.track, y.track), NodeDiffField("on_bridge", x.on_bridge, y.on_bridge), NodeDiffField("cls", x.cls, y.cls), NodeDiffField("ycls", x.ycls, y.ycls)});
		}),
		NodeDiffVector("bridges", a.bridges, b.bridges, [](const JunctionNodeBridge &x, const JunctionNodeBridge &y) {
			return NodeDiffFirst({NodeDiffTile("bridge.start", x.bridge.start, y.bridge.start), NodeDiffTile("bridge.end", x.bridge.end, y.bridge.end), NodeDiffField("bridge.tunnel", x.bridge.tunnel, y.bridge.tunnel), NodeDiffField("bridge.z", x.bridge.z, y.bridge.z), NodeDiffField("cls", x.cls, y.cls), NodeDiffField("ycls", x.ycls, y.ycls)});
		}),
		NodeDiffVector("joins", a.joins, b.joins, [](const JunctionNodeJoin &x, const JunctionNodeJoin &y) {
			return NodeDiffFirst({NodeDiffTile("from_tile", x.from_tile, y.from_tile), NodeDiffField("from_td", x.from_td, y.from_td), NodeDiffTile("to_tile", x.to_tile, y.to_tile), NodeDiffField("cls", x.cls, y.cls), NodeDiffField("ycls", x.ycls, y.ycls)});
		}),
		NodeDiffField("open_joins", a.open_joins, b.open_joins),
		NodeDiffVector("diffs", a.diffs, b.diffs, [](const JunctionNodeDiff &x, const JunctionNodeDiff &y) {
			return NodeDiffFirst({NodeDiffField("a", x.a, y.a), NodeDiffField("b", x.b, y.b), NodeDiffField("k", x.k, y.k)});
		}),
		NodeDiffVector("diagonals", a.diagonals, b.diagonals, [](const JunctionNodeDiagonal &x, const JunctionNodeDiagonal &y) {
			return NodeDiffFirst({NodeDiffField("link", x.link, y.link), NodeDiffField("dx", x.dx, y.dx), NodeDiffField("dy", x.dy, y.dy), NodeDiffField("halves", x.halves, y.halves)});
		}),
		NodeDiffVector("late_gaps", a.late_gaps, b.late_gaps, [](const JunctionNodeLateGap &x, const JunctionNodeLateGap &y) {
			return NodeDiffFirst({NodeDiffField("path", x.path, y.path), NodeDiffField("p", x.p, y.p), NodeDiffField("q", x.q, y.q), NodeDiffField("g", x.g, y.g), NodeDiffField("u", x.u, y.u), NodeDiffNumbers("diagonals", x.diagonals, y.diagonals), NodeDiffField("dir", x.dir, y.dir), NodeDiffNumbers("y_links", x.y_links, y.y_links)});
		}),
		NodeDiffVector("late_crossings", a.late_crossings, b.late_crossings, [](const JunctionNodeLateCrossing &x, const JunctionNodeLateCrossing &y) {
			return NodeDiffFirst({NodeDiffField("bridge", x.bridge, y.bridge), NodeDiffField("diagonal", x.diagonal, y.diagonal)});
		}),
		NodeDiffVector("prio_rows", a.prio_rows, b.prio_rows, [](const JunctionNodePrioRow &x, const JunctionNodePrioRow &y) {
			return NodeDiffFirst({NodeDiffField("merge", x.merge, y.merge), NodeDiffField("side", x.side, y.side), NodeDiffField("dir", x.dir, y.dir), NodeDiffField("span", x.span, y.span)});
		}),
		NodeDiffNumbers("flat", a.flat, b.flat),
		NodeDiffNumbers("lean", a.lean, b.lean),
		NodeDiffVector("point_tags", a.point_tags, b.point_tags, [](const JunctionTrackPieceTag &x, const JunctionTrackPieceTag &y) {
			return NodeDiffFirst({NodeDiffField("edge", x.edge, y.edge), NodeDiffField("token", x.token, y.token), NodeDiffField("run", x.run, y.run), NodeDiffField("copy", x.copy, y.copy)});
		}),
		NodeDiffVector("link_tags", a.link_tags, b.link_tags, [](const JunctionTrackPieceTag &x, const JunctionTrackPieceTag &y) {
			return NodeDiffFirst({NodeDiffField("edge", x.edge, y.edge), NodeDiffField("token", x.token, y.token), NodeDiffField("run", x.run, y.run), NodeDiffField("copy", x.copy, y.copy)});
		}),
		NodeDiffField("x_classes", a.x_classes, b.x_classes),
		NodeDiffField("anchor", a.anchor, b.anchor),
		NodeDiffField("anchor_y", a.anchor_y, b.anchor_y),
		NodeDiffField("error", a.error, b.error),
	});
}
