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
};

/** One entry of the search tree. */
struct LineTreeEntry {
	LineNode node;
	Money g;
	int parent;
	char move;
	std::vector<std::vector<JunctionPiece>> added; ///< The pieces each track added with this move.
};

static uint64_t PackTile(int x, int y)
{
	return (static_cast<uint64_t>(x + 32768) << 16) | static_cast<uint64_t>(y + 32768);
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

	/** Whether a piece may be built: inside the bounds, not blocked, usable terrain at the plane, and an exact parallel pair at most with the existing tracks (D147). */
	bool PieceUsable(const JunctionPiece &piece) const
	{
		if (!InBounds(piece) || blocked.contains(PackTile(piece.tile.x, piece.tile.y))) return false;
		if (request.terrain != nullptr && !request.terrain->UsableAt(piece.tile.x, piece.tile.y, request.plane)) return false;
		const auto it = existing_bits.find(PackTile(piece.tile.x, piece.tile.y));
		if (it == existing_bits.end()) return true;
		TrackBits bits = it->second;
		const Track track = TrackdirToTrack(piece.trackdir);
		if (bits.Test(track)) return false;
		bits.Set(track);
		return !TracksOverlap(bits);
	}

	/** The terrain cost of building on a piece's tile at the plane, zero without a terrain. */
	Money TerrainCost(const JunctionPiece &piece) const
	{
		return request.terrain != nullptr ? request.terrain->CostAt(piece.tile.x, piece.tile.y, request.plane) : Money{0};
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
		return key;
	}

	Money Heuristic(const LineNode &node) const
	{
		int best = INT32_MAX;
		for (const JunctionRouteEnd &goal : request.goals) {
			best = std::min(best, std::max(std::abs(goal.tile.x - node.last[0].tile.x), std::abs(goal.tile.y - node.last[0].tile.y)));
		}
		return request.use_heuristic ? request.rail_piece * (best * request.line.tracks) : Money{0};
	}

	bool IsGoal(const LineNode &node) const
	{
		const JunctionPiece &piece = node.last[0];
		return std::ranges::any_of(request.goals, [&](const JunctionRouteEnd &goal) {
			return goal.tile.x == piece.tile.x && goal.tile.y == piece.tile.y && goal.trackdir == piece.trackdir;
		});
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

	const std::vector<JunctionRaster> first = RasterizeJunctionLine(request.line, {request.start, request.start_trackdir, ""});
	if (!first[0].error.empty()) return {std::nullopt, first[0].error};

	LineNode root;
	for (const JunctionRaster &raster : first) {
		root.last.push_back(raster.pieces.back());
		root.states.push_back({cap, 0, true, false});
	}
	Money root_cost = 0;
	/* The start pieces must be usable too. */
	{
		std::unordered_map<uint64_t, TrackBits> used;
		for (const JunctionPiece &piece : root.last) {
			if (!PieceUsable(piece)) return {std::nullopt, "no route"};
			if (!used.contains(PackTile(piece.tile.x, piece.tile.y))) root_cost += TerrainCost(piece);
			if (!Occupy(used, piece)) return {std::nullopt, "no route"};
		}
	}

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
	push({std::move(root), root_cost, -1, ' ', {}});

	uint expansions = 0;
	while (!open.empty()) {
		const auto [f, g, order, index] = open.top();
		open.pop();
		(void)f;
		(void)order;
		const LineNode node = tree[index].node;
		if (best[Key(node)] < g) continue; // A cheaper way to this node was found since.

		if (IsGoal(node)) {
			JunctionLineRoute route{request.start, request.start_trackdir, ""};
			for (int i = index; tree[i].parent >= 0; i = tree[i].parent) route.moves.insert(route.moves.begin(), tree[i].move);
			return {route, "", tree[index].g};
		}
		if (++expansions > request.max_expansions) return {std::nullopt, "search limit"};

		/* Everything the line has built so far. */
		std::unordered_map<uint64_t, TrackBits> path_used;
		for (const JunctionRaster &raster : first) Occupy(path_used, raster.pieces[0]);
		for (int i = index; tree[i].parent >= 0; i = tree[i].parent) {
			for (const std::vector<JunctionPiece> &track : tree[i].added) {
				for (const JunctionPiece &piece : track) Occupy(path_used, piece);
			}
		}

		for (char move : {'F', 'L', 'R'}) {
			std::string error;
			std::vector<std::vector<JunctionPiece>> added = ExtendJunctionLine(request.line, node.last, move, error);
			if (added.empty()) continue;

			LineNode next = node;
			std::unordered_map<uint64_t, TrackBits> used = path_used;
			bool legal = true;
			Money cost = move == 'F' ? Money{0} : request.turn_cost;
			for (size_t i = 0; i < added.size() && legal; i++) {
				const JunctionPiece *previous = &node.last[i];
				for (const JunctionPiece &piece : added[i]) {
					if (!PieceUsable(piece)) {
						legal = false;
						break;
					}
					if (!used.contains(PackTile(piece.tile.x, piece.tile.y))) cost += TerrainCost(piece);
					if (!Occupy(used, piece) || !AdvanceState(next.states[i], *previous, piece)) {
						legal = false;
						break;
					}
					cost += request.rail_piece;
					previous = &piece;
				}
				if (legal) next.last[i] = added[i].back();
			}
			if (!legal) continue;
			push({std::move(next), tree[index].g + cost, index, move, std::move(added)});
		}
	}
	return {std::nullopt, "no route"};
}

} // namespace

JunctionLineRouteResult FindJunctionLineRoute(const JunctionLineRouteRequest &request)
{
	return LineRouter(request).Run();
}
