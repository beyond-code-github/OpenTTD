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
#include "map_func.h"
#include "track_func.h"

#include <algorithm>
#include <deque>
#include <map>
#include <queue>
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
	uint16_t units = 0; ///< Track 0's units so far, capped at min_units.
	uint8_t phase = 0; ///< With movement rules: 0 on shared track from the In port, 1 private, 2 merged (DESIGN-O4 revision 4).
	uint16_t private_units = 0; ///< With movement rules: units since the split, capped at wait_units.
	uint8_t plain = 0; ///< With movement rules: signal-legal private pieces since the split or the last far head, capped at plain_before_head.
	bool after_head = false; ///< With movement rules: a bridge or tunnel ended since the split.
	uint8_t window_phase = 0; ///< With a prio window: 0 before it, 1 in it, 2 after it (DESIGN-O4 revision 5).
	uint16_t window_units = 0; ///< With a prio window: the units of the window so far, capped at the window's units.
	uint8_t after_window = 0; ///< With a prio window, after it: surface pieces since the window's last piece (the merge piece first), capped at 1 + plain_after.
};

/** One entry of the search tree. */
struct LineTreeEntry {
	LineNode node;
	Money g;
	int parent;
	std::string move;
	std::vector<std::vector<JunctionPiece>> added; ///< The pieces each track added with this move.
	std::vector<JunctionBridge> bridges; ///< The bridges this move added, one per track (D149).
	std::vector<JunctionPiece> companion{}; ///< The prio window's parallel piece this move added, if any (DESIGN-O4 revision 5).
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

/** Whether a tile lies under a tunnel of the list that runs across the given axis at floor height z (D155): the game lets tunnels cross only at different depths. */
static bool UnderCrossingTunnel(int x, int y, int8_t z, bool along_x, const std::vector<JunctionBridge> &bridges)
{
	for (const JunctionBridge &b : bridges) {
		if (!b.tunnel || b.z != z) continue;
		const bool b_along_x = b.start.y == b.end.y;
		if (b_along_x == along_x) continue;
		if (b_along_x && y == b.start.y && x > std::min(b.start.x, b.end.x) && x < std::max(b.start.x, b.end.x)) return true;
		if (!b_along_x && x == b.start.x && y > std::min(b.start.y, b.end.y) && y < std::max(b.start.y, b.end.y)) return true;
	}
	return false;
}

class LineRouter {
public:
	explicit LineRouter(const JunctionLineRouteRequest &request) : request(request), cap(std::max(request.curve_spacing, request.s_bend_window))
	{
		width = std::max(0, request.max.x - request.min.x + 1);
		const int height = std::max(0, request.max.y - request.min.y + 1);
		blocked.assign(static_cast<size_t>(width) * height, false);
		path_used.assign(static_cast<size_t>(width) * height, TrackBits{});
		companion_used.assign(static_cast<size_t>(width) * height, false);
		for (const TileIndexDiffC &tile : request.blocked) Block(tile);
		if (request.existing != nullptr) {
			for (const JunctionTrack &track : request.existing->tracks) existing_bits[PackTile(track.offset.x, track.offset.y)] = track.tracks;
			for (const JunctionBridge &bridge : request.existing->bridges) {
				Block(bridge.start);
				Block(bridge.end);
			}
		}

		/* The least cost one track pays per tile of distance (O4h): a plain piece on the cheapest buildable tile, or the cheapest bridge or tunnel per tile it spans. */
		int64_t terrain_floor = 0;
		if (request.terrain != nullptr) {
			/* CostAt is the tile's clear cost plus a foundation when the track stands above the tile's lowest corner, and a ramp pays the clear cost only, so the clear cost is the least a tile can charge. */
			bool any = false;
			for (int y = request.min.y; y <= request.max.y; y++) {
				for (int x = request.min.x; x <= request.max.x; x++) {
					const JunctionTerrainTile *tile = request.terrain->At(x, y);
					if (tile == nullptr || tile->cls != JunctionTileClass::Buildable || tile->steep) continue;
					terrain_floor = any ? std::min(terrain_floor, tile->clear_cost.base()) : tile->clear_cost.base();
					any = true;
				}
			}
		}
		step_floor = request.rail_piece.base() + terrain_floor;
		track_floor = request.rail_piece.base();
		int64_t span_floor = INT64_MAX;
		if (request.bridge_span_ok && request.bridge_price && request.max_bridge_middle > 0) {
			for (uint n = 0; n <= request.max_bridge_middle; n++) span_floor = std::min<int64_t>(span_floor, request.bridge_price(n).base() / (n + 2));
		}
		if (request.tunnel_price && request.terrain != nullptr) {
			for (uint n = 0; n <= request.max_tunnel_length; n++) span_floor = std::min<int64_t>(span_floor, request.tunnel_price(n).base() / (n + 2));
		}
		step_floor = std::min(step_floor, span_floor);
		track_floor = std::min(track_floor, span_floor);
	}

	JunctionLineRouteResult Run();

private:
	const JunctionLineRouteRequest &request;
	const uint cap;
	int64_t step_floor = 0; ///< The least cost a one-track line pays per tile of distance (O4h): its rail piece plus the cheapest tile's terrain, or its cheapest bridge or tunnel per tile spanned.
	int64_t track_floor = 0; ///< The same for a line of more than one track, without terrain and for one track only (D162): an inner track on a curve travels fewer tiles than track 0, and terrain a track ahead of track 0 already paid is not paid again.
	int width = 0; ///< The bounds' width in tiles, for the grids.
	std::vector<bool> blocked; ///< Per tile of the bounds, row by row: no track or bridge may use it. A tile outside the bounds is never read: every use checks InBounds first.
	std::vector<TrackBits> path_used; ///< Per tile of the bounds: the tracks of the path being expanded. Tiles outside the bounds are not recorded: a new piece is always checked in bounds first, so only a frozen prefix can lie outside and nothing reads it.
	std::vector<size_t> touched; ///< The indices of path_used set for the current expansion, to clear.
	std::vector<bool> companion_used; ///< Per tile of the bounds: the path's prio window has its companion piece here, so no other piece of the line may (DESIGN-O4 revision 5).
	std::vector<size_t> companion_touched; ///< The indices of companion_used set for the current expansion, to clear.
	std::unordered_map<uint64_t, TrackBits> existing_bits; ///< The existing layout's tracks per tile (D147).
	std::deque<LineTreeEntry> tree; ///< A deque, so a reference to an entry stays valid while successors are pushed.

	/** The index of a tile in the grids, or -1 when it lies outside the bounds. */
	ptrdiff_t GridIndex(int x, int y) const
	{
		if (x < request.min.x || x > request.max.x || y < request.min.y || y > request.max.y) return -1;
		return static_cast<ptrdiff_t>(y - request.min.y) * width + (x - request.min.x);
	}

	/** Mark a tile as blocked; a tile outside the bounds is never usable anyway. */
	void Block(const TileIndexDiffC &tile)
	{
		const ptrdiff_t index = GridIndex(tile.x, tile.y);
		if (index >= 0) blocked[index] = true;
	}

	bool IsBlocked(int x, int y) const
	{
		const ptrdiff_t index = GridIndex(x, y);
		return index >= 0 && blocked[index];
	}

	/** The resource cost of a piece, zero when the request has none. */
	Money ResourceCost(const JunctionPiece &piece, int8_t z, JunctionResourceLevel level) const
	{
		return request.resource_cost ? request.resource_cost(piece, z, level) : Money{0};
	}

	/** Clear the path's used grid. */
	void ClearPathUsed()
	{
		for (size_t index : touched) path_used[index] = TrackBits{};
		touched.clear();
		for (size_t index : companion_touched) companion_used[index] = false;
		companion_touched.clear();
	}

	/** Record a companion piece of the path's prio window on its tile; it lies in the bounds, as it was checked when it was added. */
	void MarkCompanion(const JunctionPiece &piece)
	{
		const ptrdiff_t index = GridIndex(piece.tile.x, piece.tile.y);
		if (index < 0 || companion_used[index]) return;
		companion_used[index] = true;
		companion_touched.push_back(static_cast<size_t>(index));
	}

	/** Whether the path's prio window has its companion piece on the piece's tile. */
	bool OnCompanion(const JunctionPiece &piece) const
	{
		const ptrdiff_t index = GridIndex(piece.tile.x, piece.tile.y);
		return index >= 0 && companion_used[index];
	}

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

	/** Whether a tile may be a tunnel portal at floor height z, inclined in the given direction (D155): in bounds, not blocked, Buildable, not steep, its low edge at z. */
	bool PortalUsable(const JunctionPiece &piece, DiagDirection incline, int8_t z) const
	{
		if (!InBounds(piece) || IsBlocked(piece.tile.x, piece.tile.y)) return false;
		const JunctionTerrainTile *tile = request.terrain->At(piece.tile.x, piece.tile.y);
		return tile != nullptr && tile->cls == JunctionTileClass::Buildable && !tile->steep && tile->incline == incline && tile->z_min == z;
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
	if (!InBounds(piece) || IsBlocked(piece.tile.x, piece.tile.y)) return false;
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

	/** Append the bytes of a trivially copyable value to a key. */
	template <typename T>
	static void AppendBytes(std::string &key, const T &value)
	{
		key.append(reinterpret_cast<const char *>(&value), sizeof(T));
	}

	static std::string Key(const LineNode &node)
	{
		std::string key;
		key.reserve(16 + node.states.size() * 8 + node.z.size() + 4);
		const JunctionPiece &first = node.last[0];
		AppendBytes(key, static_cast<int32_t>(first.tile.x));
		AppendBytes(key, static_cast<int32_t>(first.tile.y));
		AppendBytes(key, static_cast<int32_t>(first.trackdir));
		for (const LineTrackState &state : node.states) {
			AppendBytes(key, static_cast<uint32_t>(state.u));
			AppendBytes(key, state.hand);
			AppendBytes(key, static_cast<uint8_t>(state.prev_gap_ok));
			AppendBytes(key, static_cast<uint8_t>(state.pair));
		}
		for (int8_t z : node.z) AppendBytes(key, z);
		AppendBytes(key, static_cast<uint8_t>(node.after_bridge));
		AppendBytes(key, node.passed);
		AppendBytes(key, node.units);
		AppendBytes(key, node.phase);
		AppendBytes(key, node.private_units);
		AppendBytes(key, node.plain);
		AppendBytes(key, static_cast<uint8_t>(node.after_head));
		AppendBytes(key, node.window_phase);
		AppendBytes(key, node.window_units);
		AppendBytes(key, node.after_window);
		return key;
	}

	/** The Manhattan distance between two tiles: track only passes between tiles through their edges, so a track enters at least this many tiles. */
	static int Manhattan(const TileIndexDiffC &a, const TileIndexDiffC &b)
	{
		return std::abs(a.x - b.x) + std::abs(a.y - b.y);
	}

	/** The fewest L or R moves that turn a heading into another: each turns the line by 45 degrees. */
	static int TurnsBetween(Direction from, Direction to)
	{
		const int difference = std::abs(static_cast<int>(from) - static_cast<int>(to));
		return std::min(difference, 8 - difference);
	}

	/**
	 * The cheapest possible cost to finish (O4h, D162): the line pays at least step_floor (one track) or track_floor (more than one track, so the bound stays admissible) per tile of Manhattan distance of track 0 through the remaining waypoints in order, then to a goal, and the line pays turn_cost for each 45 degree turn from its heading to that goal's (D150 4).
	 * The bound is taken per goal and the least is returned; without goals only the waypoints count.
	 */
	Money Heuristic(const LineNode &node) const
	{
		if (!request.use_heuristic) return Money{0};
		const int64_t floor = request.line.tracks == 1 ? step_floor : track_floor;
		const std::vector<TileIndexDiffC> &waypoints = request.waypoints;
		int distance = 0;
		TileIndexDiffC from = node.last[0].tile;
		for (size_t k = node.passed; k < waypoints.size(); k++) {
			distance += Manhattan(from, waypoints[k]);
			from = waypoints[k];
		}
		if (request.goals.empty()) return Money{floor * distance};

		const Direction heading = TrackdirToDirection(node.last[0].trackdir);
		int64_t best = INT64_MAX;
		for (const JunctionRouteEnd &goal : request.goals) {
			const int64_t bound = floor * (distance + Manhattan(from, goal.tile)) + request.turn_cost.base() * TurnsBetween(heading, TrackdirToDirection(goal.trackdir));
			best = std::min(best, bound);
		}
		return Money{best};
	}

	bool IsGoal(const LineNode &node) const
	{
		if (node.passed != request.waypoints.size() || node.units < request.min_units) return false;
		if (request.prio_window.has_value() && node.window_phase != 2) return false;
		if (request.goals.empty()) return !request.waypoints.empty();
		const JunctionPiece &piece = node.last[0];
		/* A goal on a sibling's route towards the Out port is reached by merging into it. */
		if (request.rules.has_value() && request.rules->shares_to && request.rules->shares_to(piece) && node.phase != 2) return false;
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

	/** Whether the movement rules say the piece lies on a sibling's route from the same In port. */
	bool SharesFrom(const JunctionPiece &piece) const
	{
		return request.rules->shares_from && request.rules->shares_from(piece);
	}

	/** Whether the movement rules say the piece lies on a sibling's route to the same Out port. */
	bool SharesTo(const JunctionPiece &piece) const
	{
		return request.rules->shares_to && request.rules->shares_to(piece);
	}

	/** Set the rule state of a start piece (DESIGN-O4 revision 4). */
	void RulesStart(LineNode &node, const JunctionPiece &piece) const
	{
		node.phase = SharesFrom(piece) ? 0 : 1;
		node.private_units = 0;
		node.plain = 0;
		node.after_head = false;
	}

	/**
	 * Advance the rule state over one piece of track 0 that is not a bridge or tunnel middle (DESIGN-O4 revision 4, D168).
	 * @param node The node, updated.
	 * @param piece The piece added.
	 * @return Whether the rules allow the piece.
	 */
	bool RulesStep(LineNode &node, const JunctionPiece &piece) const
	{
		const JunctionMovementRules &rules = *request.rules;
		const bool signal_legal = !piece.on_bridge; // A private piece that is not a bridge head, tunnel portal or middle.
		switch (node.phase) {
			case 0:
				if (SharesFrom(piece)) return true;
				if (rules.may_split && !rules.may_split(piece)) return false;
				node.phase = 1;
				node.private_units = static_cast<uint16_t>(std::min<uint>(piece.units, rules.wait_units));
				node.plain = 0; // The split piece shares its tile with the sibling's other track: no signal can sit on it.
				node.after_head = false;
				return true;

			case 1:
				if (SharesFrom(piece)) return false;
				if (SharesTo(piece)) {
					if (node.private_units < rules.wait_units) return false;
					if (node.after_head && node.plain < rules.plain_before_head) return false;
					if (rules.may_merge && !rules.may_merge(piece)) return false;
					node.phase = 2;
					return true;
				}
				node.private_units = static_cast<uint16_t>(std::min<uint>(node.private_units + piece.units, rules.wait_units));
				if (signal_legal) node.plain = static_cast<uint8_t>(std::min<uint>(node.plain + 1u, rules.plain_before_head));
				return true;

			default:
				return SharesTo(piece);
		}
	}

	/** Whether a bridge or tunnel may start at this near head: after a split, the private stretch before it needs plain_before_head signal-legal pieces (D113). */
	bool RulesHeadAllowed(const LineNode &node, const JunctionPiece &head) const
	{
		if (node.phase == 2 || (node.phase == 0 && SharesFrom(head))) return true;
		return (node.phase == 1 ? node.plain : 0) >= request.rules->plain_before_head; // A head that is itself the split has no piece before it.
	}

	/** After a bridge or tunnel move that ended on private track: the stretch to the next head or the merge starts afresh. */
	static void RulesAfterHead(LineNode &node)
	{
		if (node.phase != 1) return;
		node.plain = 0;
		node.after_head = true;
	}

	/** The along-travel coordinate of an axis piece: x for X-axis travel, y for Y. */
	static int AlongTravel(const JunctionPiece &piece)
	{
		return TrackdirToTrack(piece.trackdir) == Track::X ? piece.tile.x : piece.tile.y;
	}

	/** The piece one tile over on a side (+1 or -1), perpendicular to travel, with the same trackdir: a window piece's companion, or with the opposite side a companion's window piece. */
	static JunctionPiece CompanionOf(const JunctionPiece &piece, int side)
	{
		JunctionPiece companion = piece;
		if (TrackdirToTrack(piece.trackdir) == Track::X) {
			companion.tile.y = static_cast<int16_t>(companion.tile.y + side);
		} else {
			companion.tile.x = static_cast<int16_t>(companion.tile.x + side);
		}
		return companion;
	}

	/** One way a move stands to the prio window: the window's phase and units after it, and whether the piece's companion is laid. */
	struct WindowChoice {
		uint8_t phase;
		uint16_t units;
		bool companion;
		uint8_t after;
	};

	/**
	 * The ways a move may stand to the prio window (DESIGN-O4 revision 5): before it, in it, or after it; the window is a run of straight surface pieces on one axis (phase 1), of at least its units, whose last piece lies in the end range.
	 * @param node The node before the move.
	 * @param piece The first piece the move adds.
	 * @param single Whether the move adds only that piece.
 * @param pieces How many surface pieces the move adds; 0 when it starts with a bridge or tunnel head.
	 * @return The choices; none when the move cannot be made in this phase.
	 */
	std::vector<WindowChoice> WindowChoices(const LineNode &node, const JunctionPiece &piece, bool single, uint pieces) const
	{
		const JunctionPrioWindow &window = *request.prio_window;
		const uint8_t needed = static_cast<uint8_t>(1 + window.plain_after);
		const bool in_window = single && !piece.on_bridge && IsDiagonalTrackdir(piece.trackdir) && (node.window_phase == 0 || piece.trackdir == node.last[0].trackdir);
		std::vector<WindowChoice> choices;
		switch (node.window_phase) {
			case 0:
				choices.push_back({0, 0, false, 0});
				if (in_window) choices.push_back({1, static_cast<uint16_t>(std::min<uint>(piece.units, window.units)), true, 0});
				break;

			case 1: {
				if (in_window) choices.push_back({1, static_cast<uint16_t>(std::min<uint>(node.window_units + piece.units, window.units)), true, 0});
				const int along = AlongTravel(node.last[0]);
				/* The merge piece and plain_after more pieces follow the window before any head. */
				if (node.window_units >= window.units && along >= window.end_lo && along <= window.end_hi && pieces > 0) choices.push_back({2, 0, false, static_cast<uint8_t>(std::min<uint>(pieces, needed))});
				break;
			}

			default:
				if (pieces == 0 && node.after_window < needed) break;
				choices.push_back({2, 0, false, static_cast<uint8_t>(std::min<uint>(node.after_window + pieces, needed))});
				break;
		}
		return choices;
	}

	using Overlay = std::vector<std::pair<uint64_t, TrackBits>>; ///< The tracks the pieces of one successor add on top of the path's used grid.

	/** Record a piece in the path's used grid; false when it clashes with the line's own pieces (anything but an exact parallel pair). A piece outside the bounds is not recorded. */
	bool OccupyBase(const JunctionPiece &piece)
	{
		const ptrdiff_t index = GridIndex(piece.tile.x, piece.tile.y);
		if (index < 0) return true;
		TrackBits &bits = path_used[index];
		if (bits.None()) touched.push_back(index);
		const Track track = TrackdirToTrack(piece.trackdir);
		if (bits.Test(track)) return false;
		bits.Set(track);
		return !TracksOverlap(bits);
	}

	/** Whether the path's grid or the overlay has the piece's tile. */
	bool IsUsed(const Overlay &overlay, const JunctionPiece &piece) const
	{
		const ptrdiff_t index = GridIndex(piece.tile.x, piece.tile.y);
		if (index >= 0 && path_used[index].Any()) return true;
		const uint64_t packed = PackTile(piece.tile.x, piece.tile.y);
		return std::ranges::any_of(overlay, [&](const std::pair<uint64_t, TrackBits> &entry) { return entry.first == packed; });
	}

	/** Record a piece on its tile in the overlay; false when it clashes with the path's grid or the overlay (anything but an exact parallel pair). */
	bool Occupy(Overlay &overlay, const JunctionPiece &piece) const
	{
		const uint64_t packed = PackTile(piece.tile.x, piece.tile.y);
		TrackBits bits;
		const ptrdiff_t index = GridIndex(piece.tile.x, piece.tile.y);
		if (index >= 0) bits = path_used[index];
		auto entry = std::ranges::find_if(overlay, [&](const std::pair<uint64_t, TrackBits> &e) { return e.first == packed; });
		if (entry != overlay.end()) bits = bits | entry->second;
		const Track track = TrackdirToTrack(piece.trackdir);
		if (bits.Test(track)) return false;
		bits.Set(track);
		if (entry == overlay.end()) {
			overlay.emplace_back(packed, TrackBits{track});
		} else {
			entry->second.Set(track);
		}
		return !TracksOverlap(bits);
	}
};

JunctionLineRouteResult LineRouter::Run()
{
	if (request.line.tracks == 0) return {std::nullopt, "a line needs at least one track"};

	if (request.goals.empty() && request.waypoints.empty()) return {std::nullopt, "a route needs a goal or a waypoint"};

	if (request.rules.has_value() && (request.line.tracks != 1 || !request.prefix.empty())) return {std::nullopt, "movement rules need one track and no prefix"};

	if (request.prio_window.has_value() && request.line.tracks != 1) return {std::nullopt, "prio window needs one track"};

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
			if (&raster == &first[0]) {
				uint units = 0;
				for (const JunctionPiece &piece : raster.pieces) units += piece.units;
				root.units = static_cast<uint16_t>(std::min<uint>(units, request.min_units));
			}
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
			for (const JunctionRaster &raster : first) {
				const JunctionPiece &piece = raster.pieces[0];
				int8_t new_z;
				bool ramp;
				if (!PieceUsable(piece, static_cast<int8_t>(request.plane), false, new_z, ramp)) {
					usable = false;
					break;
				}
				if (path_used[GridIndex(piece.tile.x, piece.tile.y)].None()) root_cost += TerrainCost(piece, static_cast<int8_t>(request.plane), false);
				root_cost += ResourceCost(piece, static_cast<int8_t>(request.plane), JunctionResourceLevel::Surface);
				if (!OccupyBase(piece)) {
					usable = false;
					break;
				}
			}
			ClearPathUsed();
		}
		if (!usable) {
			start_error.clear();
			continue;
		}
		if (request.rules.has_value()) RulesStart(root, first[0].pieces[0]);
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
		const auto [it, inserted] = best.try_emplace(Key(entry.node), entry.g.base());
		if (!inserted) {
			if (it->second <= entry.g.base()) return;
			it->second = entry.g.base();
		}
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
		const LineNode &node = tree[index].node;
		if (best[Key(node)] < g) continue; // A cheaper way to this node was found since.

		if (IsGoal(node)) {
			int root_index = index;
			while (tree[root_index].parent >= 0) root_index = tree[root_index].parent;
			const JunctionRouteEnd &used_start = roots[root_of.at(root_index)].start;
			JunctionLineRoute route{used_start.tile, used_start.trackdir, ""};
			for (int i = index; tree[i].parent >= 0; i = tree[i].parent) route.moves.insert(0, tree[i].move);
			route.moves.insert(0, request.prefix);
			size_t prio_end = 0;
			if (request.prio_window.has_value()) {
				for (int i = index; i >= 0; i = tree[i].parent) route.companion.insert(route.companion.begin(), tree[i].companion.begin(), tree[i].companion.end());
				/* The window's last piece is the one beside the last companion. */
				const JunctionPiece &last = route.companion.back();
				const std::vector<JunctionRaster> rasters = RasterizeJunctionLine(request.line, route);
				const JunctionPiece owner = CompanionOf(last, -request.prio_window->side);
				const std::vector<JunctionPiece> &pieces = rasters[0].pieces;
				prio_end = static_cast<size_t>(std::ranges::find_if(pieces, [&](const JunctionPiece &piece) { return piece.tile.x == owner.tile.x && piece.tile.y == owner.tile.y && piece.trackdir == owner.trackdir; }) - pieces.begin());
			}
			return {route, "", tree[index].g, expansions, prio_end};
		}
		if (++expansions > request.max_expansions) return {std::nullopt, "search limit", Money{0}, expansions};

		/* Everything the line has built so far. */
		ClearPathUsed();
		std::vector<JunctionBridge> path_bridges;
		int path_root = index;
		while (tree[path_root].parent >= 0) path_root = tree[path_root].parent;
		for (const JunctionRaster &raster : roots[root_of.at(path_root)].first) OccupyBase(raster.pieces[0]);
		for (int i = index; i >= 0; i = tree[i].parent) {
			for (const JunctionPiece &piece : tree[i].companion) MarkCompanion(piece);
			path_bridges.insert(path_bridges.end(), tree[i].bridges.begin(), tree[i].bridges.end());
			for (const std::vector<JunctionPiece> &track : tree[i].added) {
				for (const JunctionPiece &piece : track) {
					/* D149 6: a bridge's middle tiles stay free. */
					if (!piece.on_bridge || std::ranges::any_of(tree[i].bridges, [&](const JunctionBridge &bridge) {
						return (piece.tile.x == bridge.start.x && piece.tile.y == bridge.start.y) || (piece.tile.x == bridge.end.x && piece.tile.y == bridge.end.y);
					})) {
						OccupyBase(piece);
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
			Overlay overlay;
			bool legal = true;
			Money cost = move == 'F' ? Money{0} : request.turn_cost;
			for (size_t i = 0; i < added.size() && legal; i++) {
				const JunctionPiece *previous = &node.last[i];
				int8_t z = node.z[i];
				for (const JunctionPiece &piece : added[i]) {
					int8_t new_z;
					bool ramp;
					if (!PieceUsable(piece, z, move == 'F', new_z, ramp) || OnCompanion(piece)) {
						legal = false;
						break;
					}
					if (!IsUsed(overlay, piece)) cost += TerrainCost(piece, z, ramp);
					if (ramp) cost += request.slope_piece;
					cost += ResourceCost(piece, z, JunctionResourceLevel::Surface);
					z = new_z;
					if (!Occupy(overlay, piece) || !AdvanceState(next.states[i], *previous, piece)) {
						legal = false;
						break;
					}
					cost += request.rail_piece;
					if (i == 0) next.units = static_cast<uint16_t>(std::min<uint>(next.units + piece.units, request.min_units));
					if (i == 0 && request.rules.has_value() && !RulesStep(next, piece)) {
						legal = false;
						break;
					}
					previous = &piece;
				}
				if (legal) {
					next.last[i] = added[i].back();
					next.z[i] = z;
				}
			}
			if (!legal) continue;
			PassWaypoints(next.passed, added[0], false);
			if (!request.prio_window.has_value()) {
				push({std::move(next), tree[index].g + cost, index, std::string(1, move), std::move(added), {}});
				continue;
			}
			/* The prio window (DESIGN-O4 revision 5): each way the piece may stand to it is a successor; a window piece also lays its companion. */
			for (const WindowChoice &choice : WindowChoices(node, added[0][0], added[0].size() == 1, static_cast<uint>(added[0].size()))) {
				LineNode window_next = next;
				Money window_cost = cost;
				std::vector<JunctionPiece> companion;
				if (choice.companion) {
					const JunctionPiece piece = CompanionOf(added[0][0], request.prio_window->side);
					int8_t new_z;
					bool ramp;
					if (!PieceUsable(piece, node.z[0], false, new_z, ramp) || IsUsed(overlay, piece) || OnCompanion(piece)) continue;
					window_cost += request.rail_piece + ResourceCost(piece, node.z[0], JunctionResourceLevel::Surface);
					companion.push_back(piece);
				}
				window_next.window_phase = choice.phase;
				window_next.window_units = choice.units;
				window_next.after_window = choice.after;
				push({std::move(window_next), tree[index].g + window_cost, index, std::string(1, move), added, {}, std::move(companion)});
			}
		}

		/* D155: a tunnel straight on from axis pieces, as the game's CmdBuildTunnel finds its far end. */
		if (request.tunnel_price && request.terrain != nullptr && !node.after_bridge && IsDiagonalTrackdir(node.last[0].trackdir)) {
			const JunctionPiece &front = node.last[0];
			const DiagDirection heading = TrackdirToExitdir(front.trackdir);
			const TileIndexDiffC step = TileIndexDiffCByDiagDir(heading);
			const bool along_x = heading == DiagDirection::NE || heading == DiagDirection::SW;

			/* The exit portal is the first tile past the entry portal whose lowest corner is at the track's height. */
			int middle = -1;
			for (uint offset = 2; offset <= request.max_tunnel_length + 2; offset++) {
				const JunctionTerrainTile *tile = request.terrain->At(front.tile.x + step.x * static_cast<int>(offset), front.tile.y + step.y * static_cast<int>(offset));
				if (tile == nullptr) break;
				if (tile->z_min == node.z[0]) {
					middle = static_cast<int>(offset) - 2;
					break;
				}
			}

			std::string error;
			std::vector<JunctionBridge> bridges;
			std::vector<std::vector<JunctionPiece>> added;
			if (middle >= 0) added = ExtendJunctionLineBridge(request.line, node.last, static_cast<uint>(middle), bridges, error, true);
			if (!added.empty()) {
				LineNode next = node;
				next.after_bridge = true;
				Overlay overlay;
				bool legal = true;
				Money cost = 0;
				for (size_t i = 0; i < added.size() && legal; i++) {
					const int8_t z = node.z[i];
					const JunctionPiece *previous = &node.last[i];
					for (size_t k = 0; k < added[i].size() && legal; k++) {
						const JunctionPiece &piece = added[i][k];
						if (k == 0 || k + 1 == added[i].size()) {
							const uint64_t packed = PackTile(piece.tile.x, piece.tile.y);
							if (!PortalUsable(piece, k == 0 ? heading : ReverseDiagDir(heading), z) || existing_bits.contains(packed) || IsUsed(overlay, piece) || OnCompanion(piece)) {
								legal = false;
								break;
							}
							cost += request.terrain->At(piece.tile.x, piece.tile.y)->clear_cost;
							cost += ResourceCost(piece, z, JunctionResourceLevel::Head);
							Occupy(overlay, piece);
						} else {
							const JunctionTerrainTile *tile = InBounds(piece) ? request.terrain->At(piece.tile.x, piece.tile.y) : nullptr;
							if (tile == nullptr || tile->z_min == z ||
									(request.existing != nullptr && UnderCrossingTunnel(piece.tile.x, piece.tile.y, z, along_x, request.existing->bridges)) ||
									UnderCrossingTunnel(piece.tile.x, piece.tile.y, z, along_x, path_bridges)) {
								legal = false;
								break;
							}
							cost += ResourceCost(piece, z, JunctionResourceLevel::TunnelBore);
						}
						if (!AdvanceState(next.states[i], *previous, piece)) {
							legal = false;
							break;
						}
						if (i == 0) next.units = static_cast<uint16_t>(std::min<uint>(next.units + piece.units, request.min_units));
						if (i == 0 && request.rules.has_value() && (k == 0 || k + 1 == added[i].size()) && ((k == 0 && !RulesHeadAllowed(next, piece)) || !RulesStep(next, piece))) {
							legal = false;
							break;
						}
						previous = &piece;
					}
					if (legal) {
						cost += request.tunnel_price(static_cast<uint>(middle));
						next.last[i] = added[i].back();
						bridges[i].z = z;
					}
				}
				if (legal) {
					if (request.rules.has_value()) RulesAfterHead(next);
					PassWaypoints(next.passed, added[0], true);
					if (!request.prio_window.has_value()) {
						push({std::move(next), tree[index].g + cost, index, fmt::format("T{}", middle), std::move(added), std::move(bridges)});
					} else {
						for (const WindowChoice &choice : WindowChoices(node, added[0][0], false, 0)) {
							LineNode window_next = next;
							window_next.window_phase = choice.phase;
							window_next.window_units = choice.units;
							window_next.after_window = choice.after;
							push({std::move(window_next), tree[index].g + cost, index, fmt::format("T{}", middle), added, bridges});
						}
					}
				}
			}
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
			Overlay overlay;
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
						if (!PieceUsable(piece, node.z[i], false, new_z, ramp) || existing_bits.contains(packed) || IsUsed(overlay, piece) || OnCompanion(piece)) {
							legal = false;
							break;
						}
						cost += TerrainCost(piece, node.z[i], false);
						cost += ResourceCost(piece, node.z[i], JunctionResourceLevel::Head);
						Occupy(overlay, piece);
					} else if (!InBounds(piece) || IsBlocked(piece.tile.x, piece.tile.y) || is_head(piece) ||
							(request.existing != nullptr && BetweenBridgeHeads(piece.tile.x, piece.tile.y, request.existing->bridges)) ||
							BetweenBridgeHeads(piece.tile.x, piece.tile.y, path_bridges) ||
							(request.terrain != nullptr && !request.terrain->SpanOk(piece.tile.x, piece.tile.y, node.z[i]))) {
						legal = false;
						break;
					} else {
						cost += ResourceCost(piece, node.z[i], JunctionResourceLevel::BridgeSpan);
					}
					if (!AdvanceState(next.states[i], *previous, piece)) {
						legal = false;
						break;
					}
					if (i == 0) next.units = static_cast<uint16_t>(std::min<uint>(next.units + piece.units, request.min_units));
					if (i == 0 && request.rules.has_value() && (k == 0 || k + 1 == added[i].size()) && ((k == 0 && !RulesHeadAllowed(next, piece)) || !RulesStep(next, piece))) {
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
			if (request.rules.has_value()) RulesAfterHead(next);
			PassWaypoints(next.passed, added[0], true);
			if (!request.prio_window.has_value()) {
				push({std::move(next), tree[index].g + cost, index, fmt::format("B{}", n), std::move(added), std::move(bridges)});
				continue;
			}
			for (const WindowChoice &choice : WindowChoices(node, added[0][0], false, 0)) {
				LineNode window_next = next;
				window_next.window_phase = choice.phase;
				window_next.window_units = choice.units;
				window_next.after_window = choice.after;
				push({std::move(window_next), tree[index].g + cost, index, fmt::format("B{}", n), added, bridges});
			}
		}
	}
	return {std::nullopt, "no route", Money{0}, expansions};
}

} // namespace

JunctionLineRouteResult FindJunctionLineRoute(const JunctionLineRouteRequest &request)
{
	return LineRouter(request).Run();
}
