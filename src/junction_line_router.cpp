/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_line_router.cpp Router for a multi-track line at one plane (LP4, LP5). */

#include "stdafx.h"

#include "junction_line_router.h"
#include "direction_func.h"
#include "track_func.h"

#include <algorithm>
#include <map>
#include <queue>
#include <set>
#include <tuple>
#include <unordered_map>

#include "safeguards.h"

namespace {

/** The curve grammar state of one track, as RouterNode in junction_router.cpp. */
struct LineTrackState {
	uint u; ///< Units since the last turn, capped.
	uint8_t hand; ///< 0 none, 1 left, 2 right.
	bool prev_gap_ok;
	bool pair;
};

/** One search node: the last piece and curve state of every track. */
struct LineNode {
	std::vector<JunctionPiece> last;
	std::vector<LineTrackState> states;
	std::vector<int8_t> z; ///< Each track's height after its last piece (D152).
	bool after_bridge = false; ///< The last move was a bridge (D149 5).
	uint8_t passed = 0; ///< Waypoints passed so far (D150).
};

/** One entry of the search tree. */
struct LineTreeEntry {
	LineNode node;
	Money g;
	int parent;
	std::string move;
	std::vector<std::vector<JunctionPiece>> added; ///< The pieces each track added with this move.
	std::vector<JunctionBridge> bridges; ///< The bridges this move added, one per track (D149).
};

static uint64_t PackTile(int x, int y)
{
	return (static_cast<uint64_t>(x + 32768) << 16) | static_cast<uint64_t>(y + 32768);
}

/** Whether a tile lies strictly between the heads of any of the bridges; as Router::BetweenBridgeHeads (junction_router.cpp). */
static bool BetweenBridgeHeads(int x, int y, const std::vector<JunctionBridge> &bridges)
{
	for (const JunctionBridge &b : bridges) {
		if (b.start.y == b.end.y && y == b.start.y && x > std::min(b.start.x, b.end.x) && x < std::max(b.start.x, b.end.x)) return true;
		if (b.start.x == b.end.x && x == b.start.x && y > std::min(b.start.y, b.end.y) && y < std::max(b.start.y, b.end.y)) return true;
	}
	return false;
}

class LineRouter {
public:
	explicit LineRouter(const JunctionLineRouteRequest &request) : request(request), cap(std::max(request.curve_spacing, request.s_bend_window))
	{
		for (const TileIndexDiffC &tile : request.blocked) blocked.insert(PackTile(tile.x, tile.y));
		if (request.existing != nullptr) {
			for (const JunctionTrack &track : request.existing->tracks) existing_bits[PackTile(track.offset.x, track.offset.y)] = track.tracks;
			for (const JunctionBridge &bridge : request.existing->bridges) {
				blocked.insert(PackTile(bridge.start.x, bridge.start.y));
				blocked.insert(PackTile(bridge.end.x, bridge.end.y));
			}
		}
	}

	JunctionLineRouteResult Run();

private:
	const JunctionLineRouteRequest &request;
	const uint cap;
	std::set<uint64_t> blocked;
	std::unordered_map<uint64_t, TrackBits> existing_bits; ///< The existing layout's tracks per tile (D147).
	std::vector<LineTreeEntry> tree;

	bool InBounds(const JunctionPiece &piece) const
	{
		return piece.tile.x >= request.min.x && piece.tile.x <= request.max.x && piece.tile.y >= request.min.y && piece.tile.y <= request.max.y;
	}

	/**
	 * The height step of a piece (D152, D152b): an axis piece on a tile inclined along it is a ramp only, as the game builds it; every other piece is level, usable at z.
	 * @param piece The piece.
	 * @param z The track's height before the piece.
	 * @param allow_ramp Whether the piece may be a ramp.
	 * @param[out] new_z The track's height after the piece.
	 * @param[out] ramp Whether the piece is a ramp.
	 * @return Whether the piece is usable.
	 */
	bool HeightStep(const JunctionPiece &piece, int8_t z, bool allow_ramp, int8_t &new_z, bool &ramp) const
	{
		new_z = z;
		ramp = false;
		if (request.terrain == nullptr) return true;
		const JunctionTerrainTile *tile = request.terrain->At(piece.tile.x, piece.tile.y);
		if (tile != nullptr && IsDiagonalTrackdir(piece.trackdir) && tile->incline != DiagDirection::Invalid) {
			const DiagDirection exit = TrackdirToExitdir(piece.trackdir);
			if (tile->incline == exit || tile->incline == ReverseDiagDir(exit)) {
				if (!allow_ramp || !RampStep(piece, z, new_z)) return false;
				ramp = true;
				return true;
			}
		}
		return request.terrain->UsableAt(piece.tile.x, piece.tile.y, z);
	}

	/** Whether an axis piece on a Buildable, non-steep tile inclined along it is a ramp from height z (up or down); new_z is the height after. */
	bool RampStep(const JunctionPiece &piece, int8_t z, int8_t &new_z) const
	{
		if (!IsDiagonalTrackdir(piece.trackdir)) return false;
		const JunctionTerrainTile *tile = request.terrain->At(piece.tile.x, piece.tile.y);
		if (tile == nullptr || tile->cls != JunctionTileClass::Buildable || tile->steep || tile->incline == DiagDirection::Invalid) return false;
		const DiagDirection exit = TrackdirToExitdir(piece.trackdir);
		if (tile->incline == exit && tile->z_min == z) {
			new_z = static_cast<int8_t>(z + 1);
		} else if (tile->incline == ReverseDiagDir(exit) && tile->z_max == z) {
			new_z = static_cast<int8_t>(z - 1);
		} else {
			return false;
		}
		return true;
	}

	/** Whether a piece may be built: inside the bounds, not blocked, its height step usable, and an exact parallel pair at most with the existing tracks (D147). */
	bool PieceUsable(const JunctionPiece &piece, int8_t z, bool allow_ramp, int8_t &new_z, bool &ramp) const
	{
	if (!InBounds(piece) || blocked.contains(PackTile(piece.tile.x, piece.tile.y))) return false;
	if (!HeightStep(piece, z, allow_ramp, new_z, ramp)) return false;
		const auto it = existing_bits.find(PackTile(piece.tile.x, piece.tile.y));
		if (it == existing_bits.end()) return true;
		TrackBits bits = it->second;
		const Track track = TrackdirToTrack(piece.trackdir);
		if (bits.Test(track)) return false;
		bits.Set(track);
		return !TracksOverlap(bits);
	}

	/** The terrain cost of building on a piece's tile at height z, zero without a terrain; a ramp pays the tile's clear cost only. */
	Money TerrainCost(const JunctionPiece &piece, int8_t z, bool ramp) const
	{
	if (request.terrain == nullptr) return Money{0};
	if (ramp) return request.terrain->At(piece.tile.x, piece.tile.y)->clear_cost;
	return request.terrain->CostAt(piece.tile.x, piece.tile.y, z);
	}

	/**
	 * Whether a new turn of the given hand is allowed from a state; as Router::TurnAllowed (junction_router.cpp 308-318).
	 * @param from The state before the turn.
	 * @param hand The new turn's hand, 1 or 2.
	 * @param[out] set_pair Whether the turn closes a tight S-bend.
	 */
	bool TurnAllowed(const LineTrackState &from, uint8_t hand, bool &set_pair) const
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
	 * Advance a track's curve state over one piece; as Router::MakePiece (junction_router.cpp 331-349).
	 * @param state The state, updated.
	 * @param previous The piece before.
	 * @param piece The piece made.
	 * @return Whether the piece is a legal F, L or R after the previous one.
	 */
	bool AdvanceState(LineTrackState &state, const JunctionPiece &previous, const JunctionPiece &piece) const
	{
		const Direction before = TrackdirToDirection(previous.trackdir);
		const Direction after = TrackdirToDirection(piece.trackdir);
		if (after != before) {
			const bool left = after == ChangeDir(before, DirDiff::Left45);
			if (!left && after != ChangeDir(before, DirDiff::Right45)) return false;
			const uint8_t hand = left ? 1 : 2;
			bool set_pair;
			if (!TurnAllowed(state, hand, set_pair)) return false;
			state.prev_gap_ok = state.u >= request.s_bend_window;
			state.hand = hand;
			state.pair = set_pair;
			state.u = 0;
		}
		state.u = std::min<uint>(state.u + (IsDiagonalTrackdir(piece.trackdir) ? 2 : 1), cap);
		return true;
	}

	static std::string Key(const LineNode &node)
	{
		std::string key;
		const JunctionPiece &first = node.last[0];
		key += fmt::format("{},{},{}", first.tile.x, first.tile.y, static_cast<int>(first.trackdir));
		for (const LineTrackState &state : node.states) key += fmt::format(";{},{},{},{}", state.u, state.hand, state.prev_gap_ok, state.pair);
		for (int8_t z : node.z) key += fmt::format(";{}", z);
		key += node.after_bridge ? ";b" : ";";
		key += fmt::format("{}", node.passed);
		return key;
	}

	/** The Chebyshev distance between two tiles. */
	static int Chebyshev(const TileIndexDiffC &a, const TileIndexDiffC &b)
	{
		return std::max(std::abs(a.x - b.x), std::abs(a.y - b.y));
	}

	/** The cheapest possible cost to finish: every track covers at least the Chebyshev distance of track 0 through the remaining waypoints in order, then to the nearest goal when there are goals (D150 4). */
	Money Heuristic(const LineNode &node) const
	{
		if (!request.use_heuristic) return Money{0};
		const std::vector<TileIndexDiffC> &waypoints = request.waypoints;
		int distance = 0;
		TileIndexDiffC from = node.last[0].tile;
		for (size_t k = node.passed; k < waypoints.size(); k++) {
			distance += Chebyshev(from, waypoints[k]);
			from = waypoints[k];
		}
		if (!request.goals.empty()) {
			int best = INT32_MAX;
			for (const JunctionRouteEnd &goal : request.goals) best = std::min(best, Chebyshev(from, goal.tile));
			distance += best;
		}
		return request.rail_piece * (distance * request.line.tracks);
	}

	bool IsGoal(const LineNode &node) const
	{
		if (node.passed != request.waypoints.size()) return false;
		if (request.goals.empty()) return !request.waypoints.empty();
		const JunctionPiece &piece = node.last[0];
		return std::ranges::any_of(request.goals, [&](const JunctionRouteEnd &goal) {
			return goal.tile.x == piece.tile.x && goal.tile.y == piece.tile.y && goal.trackdir == piece.trackdir;
		});
	}

	/**
	 * Count the waypoints that track 0's pieces added by a move pass, in order (D150 1).
	 * @param passed The waypoints passed so far, updated.
	 * @param pieces Track 0's added pieces.
	 * @param bridge Whether the move is a bridge, whose middle tiles do not pass a waypoint.
	 */
	void PassWaypoints(uint8_t &passed, const std::vector<JunctionPiece> &pieces, bool bridge) const
	{
		for (size_t k = 0; k < pieces.size(); k++) {
			if (bridge && k != 0 && k + 1 != pieces.size()) continue;
			while (passed < request.waypoints.size() && pieces[k].tile.x == request.waypoints[passed].x && pieces[k].tile.y == request.waypoints[passed].y) passed++;
		}
	}

	/** Record a piece on its tile; false when it clashes with the line's own pieces (anything but an exact parallel pair). */
	static bool Occupy(std::unordered_map<uint64_t, TrackBits> &used, const JunctionPiece &piece)
	{
		TrackBits &bits = used[PackTile(piece.tile.x, piece.tile.y)];
		const Track track = TrackdirToTrack(piece.trackdir);
		if (bits.Test(track)) return false;
		bits.Set(track);
		return !TracksOverlap(bits);
	}
};

JunctionLineRouteResult LineRouter::Run()
{
	if (request.line.tracks == 0) return {std::nullopt, "a line needs at least one track"};

	if (request.goals.empty() && request.waypoints.empty()) return {std::nullopt, "a route needs a goal or a waypoint"};

	std::vector<JunctionRouteEnd> starts = request.starts;
	if (starts.empty()) starts.push_back({request.start, request.start_trackdir});
	if (starts.size() > 1 && !request.prefix.empty()) return {std::nullopt, "a prefix needs a single start"};

	/* One root per usable start; its first pieces are occupied for its own subtree only. */
	struct Root {
		JunctionRouteEnd start;
		std::vector<JunctionRaster> first;
		LineTreeEntry entry;
	};
	std::vector<Root> roots;
	std::string start_error;
	for (const JunctionRouteEnd &start : starts) {
		std::vector<JunctionRaster> first = RasterizeJunctionLine(request.line, {start.tile, start.trackdir, request.prefix});
		if (!first[0].error.empty()) {
			start_error = first[0].error;
			continue;
		}

		LineNode root;
		LineTreeEntry root_entry;
		bool prefix_ok = true;
		for (const JunctionRaster &raster : first) {
			LineTrackState state{cap, 0, true, false};
			for (size_t k = 1; k < raster.pieces.size() && prefix_ok; k++) {
				if (!AdvanceState(state, raster.pieces[k - 1], raster.pieces[k])) prefix_ok = false;
			}
			if (!prefix_ok) break;
			root.last.push_back(raster.pieces.back());
			root.states.push_back(state);
			root.z.push_back(static_cast<int8_t>(request.plane));
			root_entry.added.emplace_back(raster.pieces.begin() + 1, raster.pieces.end());
			root_entry.bridges.insert(root_entry.bridges.end(), raster.bridges.begin(), raster.bridges.end());
		}
		if (!prefix_ok) {
			start_error = "prefix breaks the curve rules";
			continue;
		}
		/* The prefix ends in a bridge when its last move is a B<n>. */
		{
			size_t end = request.prefix.size();
			while (end > 0 && request.prefix[end - 1] >= '0' && request.prefix[end - 1] <= '9') end--;
			root.after_bridge = end > 0 && end < request.prefix.size() && request.prefix[end - 1] == 'B';
		}
		Money root_cost = 0;
		/* The start pieces must be usable too. */
		bool usable = true;
		{
			std::unordered_map<uint64_t, TrackBits> used;
			for (const JunctionRaster &raster : first) {
				const JunctionPiece &piece = raster.pieces[0];
				int8_t new_z;
				bool ramp;
				if (!PieceUsable(piece, static_cast<int8_t>(request.plane), false, new_z, ramp)) {
					usable = false;
					break;
				}
				if (!used.contains(PackTile(piece.tile.x, piece.tile.y))) root_cost += TerrainCost(piece, static_cast<int8_t>(request.plane), false);
				if (!Occupy(used, piece)) {
					usable = false;
					break;
				}
			}
		}
		if (!usable) {
			start_error.clear();
			continue;
		}
		root_entry.node = std::move(root);
		root_entry.g = root_cost;
		root_entry.parent = -1;
		roots.push_back({start, std::move(first), std::move(root_entry)});
	}
	if (roots.empty()) return {std::nullopt, start_error.empty() ? "no route" : start_error};

	using OpenItem = std::tuple<int64_t, int64_t, uint, int>; // f, g, counter, tree index.
	auto later = [](const OpenItem &a, const OpenItem &b) { return std::get<0>(a) != std::get<0>(b) ? std::get<0>(a) > std::get<0>(b) : std::get<2>(a) > std::get<2>(b); };
	std::priority_queue<OpenItem, std::vector<OpenItem>, decltype(later)> open(later);
	std::unordered_map<std::string, int64_t> best;
	uint counter = 0;

	auto push = [&](LineTreeEntry &&entry) {
		const std::string key = Key(entry.node);
		const auto it = best.find(key);
		if (it != best.end() && it->second <= entry.g.base()) return;
		best[key] = entry.g.base();
		const int64_t f = entry.g.base() + Heuristic(entry.node).base();
		const int64_t g = entry.g.base();
		tree.push_back(std::move(entry));
		open.push({f, g, counter++, static_cast<int>(tree.size()) - 1});
	};
	std::map<int, size_t> root_of; ///< Tree index of each root to its index in roots.
	for (size_t i = 0; i < roots.size(); i++) {
		const size_t before = tree.size();
		push(std::move(roots[i].entry));
		if (tree.size() != before) root_of[static_cast<int>(before)] = i;
	}

	uint expansions = 0;
	while (!open.empty()) {
		const auto [f, g, order, index] = open.top();
		open.pop();
		(void)f;
		(void)order;
		const LineNode node = tree[index].node;
		if (best[Key(node)] < g) continue; // A cheaper way to this node was found since.

		if (IsGoal(node)) {
			int root_index = index;
			while (tree[root_index].parent >= 0) root_index = tree[root_index].parent;
			const JunctionRouteEnd &used_start = roots[root_of.at(root_index)].start;
			JunctionLineRoute route{used_start.tile, used_start.trackdir, ""};
			for (int i = index; tree[i].parent >= 0; i = tree[i].parent) route.moves.insert(0, tree[i].move);
			route.moves.insert(0, request.prefix);
			return {route, "", tree[index].g};
		}
		if (++expansions > request.max_expansions) return {std::nullopt, "search limit"};

		/* Everything the line has built so far. */
		std::unordered_map<uint64_t, TrackBits> path_used;
		std::vector<JunctionBridge> path_bridges;
		int path_root = index;
		while (tree[path_root].parent >= 0) path_root = tree[path_root].parent;
		for (const JunctionRaster &raster : roots[root_of.at(path_root)].first) Occupy(path_used, raster.pieces[0]);
		for (int i = index; i >= 0; i = tree[i].parent) {
			path_bridges.insert(path_bridges.end(), tree[i].bridges.begin(), tree[i].bridges.end());
			for (const std::vector<JunctionPiece> &track : tree[i].added) {
				for (const JunctionPiece &piece : track) {
					/* D149 6: a bridge's middle tiles stay free. */
					if (!piece.on_bridge || std::ranges::any_of(tree[i].bridges, [&](const JunctionBridge &bridge) {
						return (piece.tile.x == bridge.start.x && piece.tile.y == bridge.start.y) || (piece.tile.x == bridge.end.x && piece.tile.y == bridge.end.y);
					})) {
						Occupy(path_used, piece);
					}
				}
			}
		}

		for (char move : {'F', 'L', 'R'}) {
			std::string error;
			std::vector<std::vector<JunctionPiece>> added = ExtendJunctionLine(request.line, node.last, move, error);
			if (added.empty()) continue;

			LineNode next = node;
			next.after_bridge = false;
			std::unordered_map<uint64_t, TrackBits> used = path_used;
			bool legal = true;
			Money cost = move == 'F' ? Money{0} : request.turn_cost;
			for (size_t i = 0; i < added.size() && legal; i++) {
				const JunctionPiece *previous = &node.last[i];
				int8_t z = node.z[i];
				for (const JunctionPiece &piece : added[i]) {
					int8_t new_z;
					bool ramp;
					if (!PieceUsable(piece, z, move == 'F', new_z, ramp)) {
						legal = false;
						break;
					}
					if (!used.contains(PackTile(piece.tile.x, piece.tile.y))) cost += TerrainCost(piece, z, ramp);
					if (ramp) cost += request.slope_piece;
					z = new_z;
					if (!Occupy(used, piece) || !AdvanceState(next.states[i], *previous, piece)) {
						legal = false;
						break;
					}
					cost += request.rail_piece;
					previous = &piece;
				}
				if (legal) {
					next.last[i] = added[i].back();
					next.z[i] = z;
				}
			}
			if (!legal) continue;
			PassWaypoints(next.passed, added[0], false);
			push({std::move(next), tree[index].g + cost, index, std::string(1, move), std::move(added), {}});
		}

		if (!request.bridge_span_ok || !request.bridge_price || request.max_bridge_middle == 0 || node.after_bridge) continue;
		for (uint n = 1; n <= request.max_bridge_middle; n++) {
			if (!request.bridge_span_ok(n)) continue;
			std::string error;
			std::vector<JunctionBridge> bridges;
			std::vector<std::vector<JunctionPiece>> added = ExtendJunctionLineBridge(request.line, node.last, n, bridges, error);
			if (added.empty()) continue;

			std::vector<JunctionBridge> all_bridges = path_bridges;
			all_bridges.insert(all_bridges.end(), bridges.begin(), bridges.end());
			auto is_head = [&](const JunctionPiece &piece) {
				return std::ranges::any_of(all_bridges, [&](const JunctionBridge &bridge) {
					return (piece.tile.x == bridge.start.x && piece.tile.y == bridge.start.y) || (piece.tile.x == bridge.end.x && piece.tile.y == bridge.end.y);
				});
			};

			LineNode next = node;
			next.after_bridge = true;
			std::unordered_map<uint64_t, TrackBits> used = path_used;
			bool legal = true;
			Money cost = 0;
			for (size_t i = 0; i < added.size() && legal; i++) {
				const JunctionPiece *previous = &node.last[i];
				for (size_t k = 0; k < added[i].size() && legal; k++) {
					const JunctionPiece &piece = added[i][k];
					if (k == 0 || k + 1 == added[i].size()) {
						const uint64_t packed = PackTile(piece.tile.x, piece.tile.y);
						int8_t new_z;
						bool ramp;
						if (!PieceUsable(piece, node.z[i], false, new_z, ramp) || existing_bits.contains(packed) || used.contains(packed)) {
							legal = false;
							break;
						}
						cost += TerrainCost(piece, node.z[i], false);
						Occupy(used, piece);
					} else if (!InBounds(piece) || blocked.contains(PackTile(piece.tile.x, piece.tile.y)) || is_head(piece) ||
							(request.existing != nullptr && BetweenBridgeHeads(piece.tile.x, piece.tile.y, request.existing->bridges)) ||
							BetweenBridgeHeads(piece.tile.x, piece.tile.y, path_bridges) ||
							(request.terrain != nullptr && !request.terrain->SpanOk(piece.tile.x, piece.tile.y, node.z[i]))) {
						legal = false;
						break;
					}
					if (!AdvanceState(next.states[i], *previous, piece)) {
						legal = false;
						break;
					}
					previous = &piece;
				}
				if (legal) {
					cost += request.bridge_price(n);
					next.last[i] = added[i].back();
				}
			}
			if (!legal) continue;
			PassWaypoints(next.passed, added[0], true);
			push({std::move(next), tree[index].g + cost, index, fmt::format("B{}", n), std::move(added), std::move(bridges)});
		}
	}
	return {std::nullopt, "no route"};
}

} // namespace

JunctionLineRouteResult FindJunctionLineRoute(const JunctionLineRouteRequest &request)
{
	return LineRouter(request).Run();
}
