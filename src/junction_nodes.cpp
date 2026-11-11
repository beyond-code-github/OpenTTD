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
#include "track_func.h"

#include <algorithm>
#include <map>
#include <set>
#include <tuple>

#include "safeguards.h"

/** An x coordinate in the flat layout and the class that moves it: x = value(cls) + x - flat[cls]. */
struct NodeX {
	int x;
	uint16_t cls;
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
};

/** Reads the template of a flat layout. */
struct NodeBuilder {
	JunctionNodeTemplate &tpl;
	const JunctionSpec &spec;
	const JunctionCheckContext &ctx;
	std::vector<NodePathWork> work;
	std::vector<NodeSpan> spans;
	std::set<std::tuple<uint16_t, uint16_t, int>> seen_diffs;

	/** The point's x with its class. */
	NodeX At(uint32_t point) const { return {tpl.points[point].tile.x, tpl.points[point].cls}; }

	/** Fail with the first error only. */
	void Fail(const std::string &message)
	{
		if (this->tpl.error.empty()) this->tpl.error = message;
	}

	/** x_a - x_b >= k as a diff over classes; false when both are in one class and the constant does not hold. */
	bool AddX(NodeX a, NodeX b, int k)
	{
		const int off_a = a.x - this->tpl.flat[a.cls];
		const int off_b = b.x - this->tpl.flat[b.cls];
		if (a.cls == b.cls) return off_a - off_b >= k;
		const int rhs = k - off_a + off_b;
		if (this->seen_diffs.emplace(a.cls, b.cls, rhs).second) this->tpl.diffs.push_back({a.cls, b.cls, rhs});
		return true;
	}

	/** d * (x_b - x_a) >= k. */
	bool AddDirected(int d, NodeX a, NodeX b, int k)
	{
		return d > 0 ? this->AddX(b, a, k) : this->AddX(a, b, k);
	}

	/** The points, links, classes and pieces. */
	void ReadPoints();
	/** DESIGN-N3 §2 rules 1-3, per path. */
	void AddPathDiffs();
	/** DESIGN-N3 §2 rule 4. */
	void AddCrossings();
	/** Rule 6: the path end points at the extreme flat x share their x. */
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
	for (size_t pi = 0; pi < path_count; pi++) {
		const JunctionPath &path = this->ctx.paths[pi];
		const std::vector<JunctionPiece> &pieces = path.pieces;
		NodePathWork &w = this->work[pi];
		JunctionNodePath node_path{path.from, path.to, {}, {}, 0};
		w.governing.assign(pieces.size(), 0);
		w.link_of.assign(pieces.size(), -1);
		std::vector<size_t> point_pieces;
		for (size_t i = 0; i < pieces.size(); i++) {
			const PieceId id{pieces[i].tile.x, pieces[i].tile.y, pieces[i].trackdir};
			if (!point_ids.contains(id)) continue;
			auto [it, added] = index_of.emplace(id, static_cast<uint32_t>(this->tpl.points.size()));
			if (added) this->tpl.points.push_back({pieces[i].tile, pieces[i].trackdir, 0});
			node_path.points.push_back(it->second);
			node_path.units.push_back(w.start[i]);
			point_pieces.push_back(i);
		}

		/* Links between consecutive points. */
		bool plus = false;
		bool minus = false;
		for (size_t k = 0; k + 1 < node_path.points.size(); k++) {
			const size_t a = point_pieces[k];
			const size_t b = point_pieces[k + 1];
			bool free = true;
			for (size_t i = a; i <= b; i++) {
				const Trackdir td = pieces[i].trackdir;
				if (pieces[i].on_bridge || (td != Trackdir::X_NE && td != Trackdir::X_SW) || pieces[i].tile.y != pieces[a].tile.y) free = false;
			}
			const uint32_t from = node_path.points[k];
			const uint32_t to = node_path.points[k + 1];
			if (free) {
				const int dx = pieces[b].tile.x - pieces[a].tile.x;
				if (dx > 0) plus = true;
				if (dx < 0) minus = true;
				for (size_t i = a + 1; i < b; i++) w.link_of[i] = static_cast<int>(this->spans.size());
				this->spans.push_back({from, to});
			}
			if (link_seen.emplace(from, to).second) this->tpl.links.push_back({from, to, free});
		}
		if (plus && minus) this->Fail(fmt::format("path {} has free links both ways", pi));
		node_path.dir = plus ? 1 : (minus ? -1 : 0);
		this->tpl.paths.push_back(std::move(node_path));
	}

	/* Classes: union-find over rigid links, numbered by their first point. */
	NodeUnion classes(this->tpl.points.size());
	for (const JunctionNodeLink &link : this->tpl.links) {
		if (!link.free) classes.Join(link.from, link.to);
	}
	std::map<uint32_t, uint16_t> class_of_root;
	for (uint32_t p = 0; p < this->tpl.points.size(); p++) {
		const uint32_t root = classes.Find(p);
		auto [it, added] = class_of_root.emplace(root, static_cast<uint16_t>(class_of_root.size()));
		if (added) this->tpl.flat.push_back(this->tpl.points[p].tile.x);
		this->tpl.points[p].cls = it->second;
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
			this->tpl.pieces.push_back({pieces[i].tile, track, pieces[i].on_bridge, this->tpl.points[w.governing[i]].cls});
		}
	}

	/* The bridges with the class of their heads. */
	for (size_t bi = 0; bi < layout.bridges.size(); bi++) {
		const JunctionBridge &bridge = layout.bridges[bi];
		std::optional<uint16_t> cls[2];
		const TileIndexDiffC heads[2] = {bridge.start, bridge.end};
		for (size_t h = 0; h < 2; h++) {
			for (const JunctionNodePiece &piece : this->tpl.pieces) {
				if (piece.on_bridge && piece.tile.x == heads[h].x && piece.tile.y == heads[h].y) {
					cls[h] = piece.cls;
					break;
				}
			}
		}
		if (!cls[0].has_value() || !cls[1].has_value()) {
			this->Fail(fmt::format("bridge {}: a head is not a piece of any path", bi));
			this->tpl.bridges.emplace_back(bridge, 0);
		} else {
			if (*cls[0] != *cls[1]) this->Fail(fmt::format("bridge {}: heads in different classes", bi));
			this->tpl.bridges.emplace_back(bridge, *cls[0]);
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

		/* The rule of §2: d * (x_q - x_p) >= d * F + ceil((g - U) / 2) for the points p before q with the flat unit difference U. */
		auto add_gap = [&](uint32_t p, int unit_p, uint32_t q, int unit_q, int g) {
			const int u = unit_q - unit_p;
			if (this->tpl.points[p].cls == this->tpl.points[q].cls || d == 0) {
				if (u < g) this->Fail(fmt::format("path {} points {},{}: gap {} below {}", pi, p, q, u, g));
				return;
			}
			const int f = this->tpl.points[q].tile.x - this->tpl.points[p].tile.x;
			this->AddDirected(d, this->At(p), this->At(q), d * f + CeilHalf(g - u));
		};

		/* Rule 1. */
		for (size_t k = 0; k + 1 < node_path.points.size(); k++) {
			const auto link = std::ranges::find_if(this->tpl.links, [&](const JunctionNodeLink &l) { return l.from == node_path.points[k] && l.to == node_path.points[k + 1]; });
			if (link == this->tpl.links.end() || !link->free) continue;
			if (!this->AddDirected(d, this->At(link->from), this->At(link->to), 1)) this->Fail(fmt::format("path {}: a free link joins one class", pi));
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
	for (size_t bi = 0; bi < this->tpl.bridges.size(); bi++) {
		const JunctionBridge &bridge = this->tpl.bridges[bi].first;
		const uint16_t head_cls = this->tpl.bridges[bi].second;
		const bool x_axis = bridge.start.y == bridge.end.y;

		for (size_t qi = 0; qi < this->ctx.paths.size(); qi++) {
			const std::vector<JunctionPiece> &pieces = this->ctx.paths[qi].pieces;
			const NodePathWork &w = this->work[qi];
			const bool uses = std::ranges::any_of(pieces, [&](const JunctionPiece &p) {
				return p.on_bridge && ((p.tile.x == bridge.start.x && p.tile.y == bridge.start.y) || (p.tile.x == bridge.end.x && p.tile.y == bridge.end.y));
			});
			if (uses) continue;

			for (size_t i = 0; i < pieces.size(); i++) {
				const TileIndexDiffC tile = pieces[i].tile;
				if (x_axis) {
					const int lo = std::min(bridge.start.x, bridge.end.x);
					const int hi = std::max(bridge.start.x, bridge.end.x);
					if (tile.y != bridge.start.y || tile.x <= lo || tile.x >= hi) continue;
					if (w.link_of[i] >= 0) {
						this->Fail(fmt::format("bridge {}: path {} has a free link under it at {},{}", bi, qi, tile.x, tile.y));
						continue;
					}
					const NodeX piece{tile.x, this->tpl.points[w.governing[i]].cls};
					const NodeX h1{lo, head_cls};
					const NodeX h2{hi, head_cls};
					if (!this->AddX(piece, h1, 1) || !this->AddX(h2, piece, 1)) this->Fail(fmt::format("bridge {}: path {} piece at {},{} is not between the heads", bi, qi, tile.x, tile.y));
				} else {
					const int lo = std::min(bridge.start.y, bridge.end.y);
					const int hi = std::max(bridge.start.y, bridge.end.y);
					if (tile.x != bridge.start.x || tile.y <= lo || tile.y >= hi) continue;
					const NodeX head{bridge.start.x, head_cls};
					if (w.link_of[i] >= 0) {
						const NodeSpan &span = this->spans[static_cast<size_t>(w.link_of[i])];
						const int d = this->tpl.paths[qi].dir;
						if (!this->AddDirected(d, this->At(span.from), head, 1) || !this->AddDirected(d, head, this->At(span.to), 1)) {
							this->Fail(fmt::format("bridge {}: path {} link under it at {},{} joins the bridge's class", bi, qi, tile.x, tile.y));
						}
					} else {
						const NodeX piece{tile.x, this->tpl.points[w.governing[i]].cls};
						if (!this->AddX(piece, head, 0) || !this->AddX(head, piece, 0)) this->Fail(fmt::format("bridge {}: path {} piece at {},{} is not under the heads", bi, qi, tile.x, tile.y));
					}
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
	const auto [min_it, max_it] = std::ranges::minmax_element(ends, [&](uint32_t a, uint32_t b) { return this->tpl.points[a].tile.x < this->tpl.points[b].tile.x; });
	for (const int edge : {this->tpl.points[*min_it].tile.x, this->tpl.points[*max_it].tile.x}) {
		std::optional<uint32_t> previous;
		for (const uint32_t point : ends) {
			if (this->tpl.points[point].tile.x != edge) continue;
			if (previous.has_value() && this->tpl.points[*previous].cls != this->tpl.points[point].cls) {
				this->AddX(this->At(*previous), this->At(point), 0);
				this->AddX(this->At(point), this->At(*previous), 0);
			}
			previous = point;
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
	} else {
		tpl.anchor = tpl.points[anchor->points.back()].cls;
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
	return positions.RestrictRange(tpl.anchor, tpl.flat[tpl.anchor], tpl.flat[tpl.anchor]);
}

std::vector<int> JunctionNodeSnap(const JunctionNodeTemplate &tpl, JunctionPositions &positions)
{
	std::vector<int> values;
	for (size_t c = 0; c < tpl.flat.size(); c++) {
		const int value = std::clamp(tpl.flat[c], positions.Lo(c), positions.Up(c));
		if (!positions.RestrictRange(c, value, value)) return {};
		values.push_back(value);
	}
	return values;
}

JunctionLayout RenderJunctionNodeTracks(const JunctionNodeTemplate &tpl, const std::vector<int> &values)
{
	std::map<std::pair<int, int>, TrackBits> tiles;
	auto shift = [&](uint16_t cls) { return values[cls] - tpl.flat[cls]; };

	for (const JunctionNodePiece &piece : tpl.pieces) {
		if (piece.on_bridge) continue;
		tiles[{piece.tile.x + shift(piece.cls), piece.tile.y}].Set(piece.track);
	}
	for (const JunctionNodeLink &link : tpl.links) {
		if (!link.free) continue;
		const JunctionNodePoint &from = tpl.points[link.from];
		const JunctionNodePoint &to = tpl.points[link.to];
		const int x_from = from.tile.x + shift(from.cls);
		const int x_to = to.tile.x + shift(to.cls);
		for (int x = std::min(x_from, x_to) + 1; x < std::max(x_from, x_to); x++) tiles[{x, from.tile.y}].Set(Track::X);
	}

	JunctionLayout layout{};
	for (const auto &[tile, bits] : tiles) layout.tracks.push_back({{static_cast<int16_t>(tile.first), static_cast<int16_t>(tile.second)}, bits});
	for (const auto &[bridge, cls] : tpl.bridges) {
		JunctionBridge moved = bridge;
		moved.start.x = static_cast<int16_t>(moved.start.x + shift(cls));
		moved.end.x = static_cast<int16_t>(moved.end.x + shift(cls));
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

	std::vector<uint32_t> free_links;
	for (uint32_t i = 0; i < tpl.links.size(); i++) {
		if (tpl.links[i].free) free_links.push_back(i);
	}
	const size_t classes = tpl.flat.size();
	JunctionPositions positions(classes + 3 * free_links.size(), -RANGE, RANGE);

	/* 2. The template. */
	if (!AddJunctionNodeConstraints(tpl, positions)) {
		result.failure = JunctionNodeFailure{Kind::Template};
		return result;
	}

	/* 3. The pieces of every class fit the facts; a tile's bits are those of the render at the flat values. */
	std::map<std::pair<int, int>, TrackBits> flat_bits;
	for (const JunctionNodePiece &piece : tpl.pieces) {
		if (!piece.on_bridge) flat_bits[{piece.tile.x, piece.tile.y}].Set(piece.track);
	}
	for (const JunctionNodeLink &link : tpl.links) {
		if (!link.free) continue;
		const int x_from = tpl.points[link.from].tile.x;
		const int x_to = tpl.points[link.to].tile.x;
		for (int x = std::min(x_from, x_to) + 1; x < std::max(x_from, x_to); x++) flat_bits[{x, tpl.points[link.from].tile.y}].Set(Track::X);
	}
	for (uint16_t c = 0; c < classes; c++) {
		std::vector<int> allowed;
		for (int v = -RANGE; v <= RANGE; v++) {
			const bool fits = std::ranges::all_of(tpl.pieces, [&](const JunctionNodePiece &piece) {
				if (piece.cls != c || piece.on_bridge) return true;
				return JunctionNodeTrackFits(facts, piece.tile.x + v - tpl.flat[c] + offset.x, piece.tile.y + offset.y, flat_bits.at({piece.tile.x, piece.tile.y}));
			});
			if (fits) allowed.push_back(v);
		}
		if (allowed.empty() || !positions.RestrictDomain(c, allowed)) {
			result.failure = JunctionNodeFailure{Kind::Piece, UINT32_MAX, 0, c};
			return result;
		}
	}

	/* 4. The free links. */
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
		const size_t u = classes + 3 * k;
		const size_t w = u + 1;
		const size_t w2 = u + 2;
		const int row = p.tile.y + offset.y;

		/* aux = x_cls + shift, in working x, as two difference constraints. */
		auto tie = [&](size_t aux, const JunctionNodePoint &point, int shift) {
			const int constant = point.tile.x - tpl.flat[point.cls] + offset.x + shift;
			return positions.AddDifference(aux, point.cls, constant) && positions.AddDifference(point.cls, aux, -constant);
		};
		if (!tie(u, p, d) || !tie(w, q, -d) || !tie(w2, q, -2 * d)) {
			result.failure = JunctionNodeFailure{Kind::Template};
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
					return result;
				}
			} else if (c + 1 <= facts.MaxX() && facts.Straight(c + 1, row, Axis::X, next_entry, next_exit) && exit != next_entry) {
				if (!(d > 0 ? positions.AddAvoid(u, w2, c) : positions.AddAvoid(w2, u, c + 1))) {
					result.failure = JunctionNodeFailure{Kind::Break, link_index, c};
					return result;
				}
			}
		}
		if (!positions.RestrictRange(u, facts.MinX(), facts.MaxX())) {
			result.failure = JunctionNodeFailure{Kind::Edge, link_index, facts.MinX() - 1};
			return result;
		}
		if (!positions.RestrictRange(w, facts.MinX(), facts.MaxX())) {
			result.failure = JunctionNodeFailure{Kind::Edge, link_index, facts.MaxX() + 1};
			return result;
		}
	}

	/* 5. Snap each class to the nearest value. */
	for (size_t c = 0; c < classes; c++) {
		bool done = false;
		for (int distance = 0; !done; distance++) {
			const int low = tpl.flat[c] - distance;
			const int high = tpl.flat[c] + distance;
			if (low < positions.Lo(c) && high > positions.Up(c)) break;
			for (const int t : {low, high}) {
				if (done || t < positions.Lo(c) || t > positions.Up(c)) continue;
				const size_t mark = positions.Mark();
				if (positions.RestrictRange(c, t, t)) {
					done = true;
				} else {
					positions.Undo(mark);
				}
				if (distance == 0) break;
			}
		}
		if (!done) {
			result.failure = JunctionNodeFailure{Kind::Template};
			result.values.clear();
			return result;
		}
		result.values.push_back(positions.Lo(c));
	}

	/* 6. Render. */
	JunctionLayout layout = RenderJunctionNodeTracks(tpl, result.values);
	PlaceJunctionSignals(layout, spec);
	NormaliseJunctionLayout(layout);
	result.layout = std::move(layout);
	return result;
}
