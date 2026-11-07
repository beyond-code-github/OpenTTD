/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_router.cpp Junction route search. */

#include "stdafx.h"

#include "junction_router.h"
#include "direction_func.h"
#include "map_func.h"
#include "track_func.h"

#include <algorithm>
#include <map>
#include <queue>
#include <set>
#include <tuple>

#include "safeguards.h"

namespace {

using TileKey = std::pair<int, int>;

/** One search state: the last piece made and the curve grammar state. */
struct RouterNode {
	int x;
	int y;
	Trackdir trackdir;
	uint u; ///< Units since the last turn, capped.
	uint8_t hand; ///< 0 none, 1 left, 2 right.
	bool prev_gap_ok;
	bool pair;
	bool first; ///< A start piece: nothing has been made yet.
	bool on_bridge; ///< The last piece is a bridge head.
};

using NodeKey = std::tuple<int, int, int, uint, int, bool, bool, bool, bool>;

NodeKey MakeKey(const RouterNode &n)
{
	return {n.x, n.y, static_cast<int>(n.trackdir), n.u, n.hand, n.prev_gap_ok, n.pair, n.first, n.on_bridge};
}

/** A node in the search tree. */
struct TreeNode {
	RouterNode state;
	Money g;
	int parent;
	std::string moves; ///< The moves from the parent to this node.
};

/** The static facts of one search. */
class Router {
public:
	Router(const JunctionLayout &layout, const JunctionRouteRequest &request) : request(request)
	{
		for (const JunctionTrack &jt : layout.tracks) tracks[{jt.offset.x, jt.offset.y}] |= jt.tracks;
		for (const JunctionBridge &b : layout.bridges) {
			bridges.push_back(b);
			heads.insert({b.start.x, b.start.y});
			heads.insert({b.end.x, b.end.y});
		}
		for (const TileIndexDiffC &c : request.crossable) crossable.insert({c.x, c.y});
		for (const JunctionRouteEnd &g : request.goals) {
			goals.insert({g.tile.x, g.tile.y, static_cast<int>(g.trackdir)});
			goal_tiles.insert({g.tile.x, g.tile.y});
		}
		cap = std::max(request.curve_spacing, request.s_bend_window);
	}

	std::optional<JunctionRoute> Run();

private:
	const JunctionRouteRequest &request;
	std::map<TileKey, TrackBits> tracks;
	std::vector<JunctionBridge> bridges;
	std::set<TileKey> heads;
	std::set<TileKey> crossable;
	std::set<std::tuple<int, int, int>> goals;
	std::set<TileKey> goal_tiles;
	uint cap = 0;

	bool InBounds(int x, int y) const { return x >= request.min.x && x <= request.max.x && y >= request.min.y && y <= request.max.y; }
	bool IsGoal(int x, int y, Trackdir td) const { return goals.contains({x, y, static_cast<int>(td)}); }
	bool BetweenBridgeHeads(int x, int y) const;
	bool TileFree(int x, int y) const;
	Money TerrainCost(int x, int y) const;
	bool ConflictsAt(int x, int y, Track t) const;
	bool TurnAllowed(const RouterNode &from, uint8_t hand, bool &set_pair) const;
	bool MakePiece(const RouterNode &from, char move, RouterNode &to, bool &conflict) const;
	Money Heuristic(const RouterNode &n) const;
	void Successors(const RouterNode &from, std::vector<std::pair<RouterNode, std::pair<Money, std::string>>> &out) const;
	bool SelfCrossing(const JunctionRoute &route) const;
};

bool Router::BetweenBridgeHeads(int x, int y) const
{
	for (const JunctionBridge &b : bridges) {
		if (b.start.y == b.end.y && y == b.start.y && x > std::min(b.start.x, b.end.x) && x < std::max(b.start.x, b.end.x)) return true;
		if (b.start.x == b.end.x && x == b.start.x && y > std::min(b.start.y, b.end.y) && y < std::max(b.start.y, b.end.y)) return true;
	}
	return false;
}

/** Whether a tile holds no track of the layout. */
bool Router::TileFree(int x, int y) const
{
	const auto it = tracks.find({x, y});
	return it == tracks.end() || it->second.None();
}

/** The terrain cost of a piece or bridge head on a tile; nothing when the layout already holds track there. */
Money Router::TerrainCost(int x, int y) const
{
	if (request.terrain == nullptr || !TileFree(x, y)) return 0;
	return request.terrain->CostAt(x, y, request.plane);
}

/** Whether a new track t on a tile clashes with the tracks the layout holds there. */
bool Router::ConflictsAt(int x, int y, Track t) const
{
	const auto it = tracks.find({x, y});
	if (it == tracks.end()) return false;
	for (Track e : it->second) {
		const bool parallel = (e == Track::Upper && t == Track::Lower) || (e == Track::Lower && t == Track::Upper) ||
				(e == Track::Left && t == Track::Right) || (e == Track::Right && t == Track::Left);
		if (!parallel) return true;
	}
	return false;
}

/**
 * Whether a new turn of the given hand is allowed from a state.
 * @param from The state before the turn.
 * @param hand The new turn's hand, 1 or 2.
 * @param[out] set_pair Whether the turn closes a tight S-bend.
 */
bool Router::TurnAllowed(const RouterNode &from, uint8_t hand, bool &set_pair) const
{
	set_pair = false;
	if (from.pair) return from.u >= request.s_bend_window;
	if (from.u >= request.curve_spacing) return true;
	if (from.hand != 0 && from.hand != hand && from.prev_gap_ok) {
		set_pair = true;
		return true;
	}
	return false;
}

/**
 * Make one F, L or R piece from a state.
 * @param from The state.
 * @param move F, L or R.
 * @param[out] to The state after the piece.
 * @param[out] conflict Whether the piece fails only the track clash rule (and may be a merge tile piece).
 * @return Whether the piece satisfies every rule except the track clash rule.
 */
bool Router::MakePiece(const RouterNode &from, char move, RouterNode &to, bool &conflict) const
{
	conflict = false;
	const DirDiff diff = move == 'F' ? DirDiff::Same : move == 'L' ? DirDiff::Left45 : DirDiff::Right45;
	Trackdir next;
	if (!JunctionNextTrackdir(from.trackdir, diff, next)) return false;

	to = from;
	if (move != 'F') {
		const uint8_t hand = move == 'L' ? 1 : 2;
		bool set_pair;
		if (!TurnAllowed(from, hand, set_pair)) return false;
		to.prev_gap_ok = from.u >= request.s_bend_window;
		to.hand = hand;
		to.pair = set_pair;
		to.u = 0;
	}
	const TileIndexDiffC step = TileIndexDiffCByDiagDir(TrackdirToExitdir(from.trackdir));
	to.x = from.x + step.x;
	to.y = from.y + step.y;
	to.trackdir = next;
	to.u = std::min<uint>(to.u + (IsDiagonalTrackdir(next) ? 2 : 1), cap);
	to.first = false;
	to.on_bridge = false;

	if (!InBounds(to.x, to.y) || heads.contains({to.x, to.y})) return false;
	if (request.terrain != nullptr && !request.terrain->UsableAt(to.x, to.y, request.plane)) return false;
	if (IsGoal(to.x, to.y, next) || crossable.contains({to.x, to.y})) return true;
	if (from.first) {
		const auto it = tracks.find({to.x, to.y});
		if (it != tracks.end() && it->second.Test(TrackdirToTrack(from.trackdir))) return true;
	}
	if (ConflictsAt(to.x, to.y, TrackdirToTrack(next))) conflict = true;
	return true;
}

Money Router::Heuristic(const RouterNode &n) const
{
	int best = INT32_MAX;
	for (const JunctionRouteEnd &g : request.goals) best = std::min(best, abs(n.x - g.tile.x) + abs(n.y - g.tile.y));
	return best == INT32_MAX ? Money(0) : request.prices.rail_piece * best;
}

void Router::Successors(const RouterNode &from, std::vector<std::pair<RouterNode, std::pair<Money, std::string>>> &out) const
{
	for (char move : {'F', 'L', 'R'}) {
		/* The track beyond a bridge's far head must continue in line with the bridge (G5). */
		if (from.on_bridge && move != 'F') continue;
		RouterNode to;
		bool conflict;
		if (!MakePiece(from, move, to, conflict)) continue;
		if (!conflict) {
			Money cost = request.prices.rail_piece + TerrainCost(to.x, to.y);
			if (crossable.contains({to.x, to.y})) cost += request.crossable_cost;
			out.push_back({to, {cost, std::string(1, move)}});
			continue;
		}
		/* Exception 2: a piece on a merge tile, followed at once by a goal piece. */
		if (!goal_tiles.contains({to.x, to.y})) continue;
		for (char move2 : {'F', 'L', 'R'}) {
			RouterNode to2;
			bool conflict2;
			if (!MakePiece(to, move2, to2, conflict2)) continue;
			if (!IsGoal(to2.x, to2.y, to2.trackdir)) continue;
			out.push_back({to2, {request.prices.rail_piece * 2 + TerrainCost(to.x, to.y) + TerrainCost(to2.x, to2.y), std::string{move, move2}}});
		}
	}

	if (from.on_bridge || !IsDiagonalTrackdir(from.trackdir) || !request.bridge_span_ok) return;
	const TileIndexDiffC step = TileIndexDiffCByDiagDir(TrackdirToExitdir(from.trackdir));
	for (uint n = 1; n <= request.max_bridge_middle; n++) {
		if (!request.bridge_span_ok(n)) continue;
		bool ok = true;
		for (uint k = 1; k <= n + 2 && ok; k++) {
			const int x = from.x + static_cast<int>(k) * step.x;
			const int y = from.y + static_cast<int>(k) * step.y;
			if (!InBounds(x, y) || heads.contains({x, y})) {
				ok = false;
			} else if (k == 1 || k == n + 2) {
				ok = TileFree(x, y) && !crossable.contains({x, y}) && (request.terrain == nullptr || request.terrain->UsableAt(x, y, request.plane));
			} else {
				ok = !BetweenBridgeHeads(x, y) && (request.terrain == nullptr || request.terrain->SpanOk(x, y, request.plane));
			}
		}
		if (!ok) continue;
		RouterNode to = from;
		to.x = from.x + static_cast<int>(n + 2) * step.x;
		to.y = from.y + static_cast<int>(n + 2) * step.y;
		to.u = std::min<uint>(from.u + 2 * (n + 2), cap);
		to.first = false;
		to.on_bridge = true;
		Money bridge_cost = request.prices.bridge(n);
		bridge_cost += TerrainCost(from.x + step.x, from.y + step.y) + TerrainCost(to.x, to.y);
		out.push_back({to, {bridge_cost, fmt::format("B{}", n)}});
	}
}

/** Whether two of the route's own pieces share a tile with tracks that are not a parallel pair. */
bool Router::SelfCrossing(const JunctionRoute &route) const
{
	const JunctionRaster raster = RasterizeJunctionRoute(route);
	if (!raster.error.empty()) return true;
	std::map<TileKey, std::vector<Track>> own;
	for (const JunctionPiece &piece : raster.pieces) {
		if (piece.on_bridge) continue;
		const Track t = TrackdirToTrack(piece.trackdir);
		auto &list = own[{piece.tile.x, piece.tile.y}];
		for (Track e : list) {
			const bool parallel = (e == Track::Upper && t == Track::Lower) || (e == Track::Lower && t == Track::Upper) ||
					(e == Track::Left && t == Track::Right) || (e == Track::Right && t == Track::Left);
			if (!parallel) return true;
		}
		list.push_back(t);
	}
	return false;
}

std::optional<JunctionRoute> Router::Run()
{
	std::vector<TreeNode> tree;
	std::map<NodeKey, Money> best;
	using QueueItem = std::tuple<Money, Money, uint64_t, int>; // f, g, counter, tree index
	std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<QueueItem>> open;
	uint64_t counter = 0;

	auto push = [&](const RouterNode &state, Money g, int parent, std::string moves) {
		const NodeKey key = MakeKey(state);
		const auto it = best.find(key);
		if (it != best.end() && it->second <= g) return;
		best[key] = g;
		tree.push_back({state, g, parent, std::move(moves)});
		const int index = static_cast<int>(tree.size()) - 1;
		open.push({g + Heuristic(state), g, counter++, index});
	};

	for (const JunctionRouteEnd &start : request.starts) {
		push({start.tile.x, start.tile.y, start.trackdir, cap, 0, true, false, true, false}, 0, -1, "");
	}

	uint expansions = 0;
	std::vector<std::pair<RouterNode, std::pair<Money, std::string>>> successors;
	while (!open.empty()) {
		const auto [f, g, order, index] = open.top();
		open.pop();
		const TreeNode node = tree[index];
		if (best[MakeKey(node.state)] < node.g) continue;

		const RouterNode &s = node.state;
		if (!s.first && !s.on_bridge && IsGoal(s.x, s.y, s.trackdir)) {
			std::string moves;
			int i = index;
			for (; tree[i].parent != -1; i = tree[i].parent) moves.insert(0, tree[i].moves);
			JunctionRoute route{{static_cast<int16_t>(tree[i].state.x), static_cast<int16_t>(tree[i].state.y)}, tree[i].state.trackdir, moves};
			if (SelfCrossing(route)) return std::nullopt;
			return route;
		}

		if (++expansions > request.max_expansions) return std::nullopt;
		successors.clear();
		Successors(s, successors);
		for (auto &[to, cost] : successors) push(to, node.g + cost.first, index, std::move(cost.second));
	}
	return std::nullopt;
}

} // namespace

/**
 * Find the cheapest route from a start piece to a goal piece.
 * @param layout What is already laid.
 * @param request What to route.
 * @return The route, or std::nullopt when none is found.
 */
std::optional<JunctionRoute> FindJunctionRoute(const JunctionLayout &layout, const JunctionRouteRequest &request)
{
	Router router(layout, request);
	return router.Run();
}
