/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_router.cpp Junction route search. */

#include "../../stdafx.h"

#include "junction_router.h"
#include "../../direction_func.h"
#include "../../map_func.h"
#include "../../track_func.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <map>
#include <queue>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

#include "../../safeguards.h"

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

/** Pack a search state into one key. */
uint64_t PackKey(const RouterNode &n)
{
	assert(n.u < 256);
	assert(n.x >= -32768 && n.x < 32768);
	assert(n.y >= -32768 && n.y < 32768);
	uint64_t key = static_cast<uint64_t>(n.x + 32768);
	key |= static_cast<uint64_t>(n.y + 32768) << 16;
	key |= static_cast<uint64_t>(n.trackdir) << 32;
	key |= static_cast<uint64_t>(n.u) << 36;
	key |= static_cast<uint64_t>(n.hand) << 44;
	key |= static_cast<uint64_t>(n.prev_gap_ok) << 46;
	key |= static_cast<uint64_t>(n.pair) << 47;
	key |= static_cast<uint64_t>(n.first) << 48;
	key |= static_cast<uint64_t>(n.on_bridge) << 49;
	return key;
}

/** The cheapest g found per packed search state: linear probing, power-of-two capacity, grows at half full. */
class BestCostTable {
public:
	explicit BestCostTable(size_t capacity) : keys(capacity, EMPTY), values(capacity), mask(capacity - 1)
	{
		assert(capacity != 0 && (capacity & (capacity - 1)) == 0);
	}

	/**
	 * Insert key with g, or lower its g.
	 * @return Whether the state is new or improved (false when the stored g is <= g).
	 */
	bool Improve(uint64_t key, Money g)
	{
		if (this->count * 2 >= this->keys.size()) this->Grow();
		size_t slot = this->Slot(key);
		while (this->keys[slot] != EMPTY) {
			if (this->keys[slot] == key) {
				if (this->values[slot] <= g) return false;
				this->values[slot] = g;
				return true;
			}
			slot = (slot + 1) & this->mask;
		}
		this->keys[slot] = key;
		this->values[slot] = g;
		this->count++;
		return true;
	}

	/** The stored g; the key must be present. */
	Money Get(uint64_t key) const
	{
		size_t slot = this->Slot(key);
		while (this->keys[slot] != key) {
			assert(this->keys[slot] != EMPTY);
			slot = (slot + 1) & this->mask;
		}
		return this->values[slot];
	}

private:
	static constexpr uint64_t EMPTY = UINT64_MAX; ///< PackKey uses at most 50 bits, so never a real key.
	std::vector<uint64_t> keys;
	std::vector<Money> values;
	size_t count = 0;
	size_t mask = 0;

	/** The home slot of a key: the high half of a 64-bit multiply mix folded onto the low half, then masked. */
	size_t Slot(uint64_t key) const
	{
		const uint64_t x = key * 0x9E3779B97F4A7C15ull;
		return static_cast<size_t>(x ^ (x >> 32)) & this->mask;
	}

	/** Double the capacity and reinsert every key. */
	void Grow()
	{
		std::vector<uint64_t> old_keys(this->keys.size() * 2, EMPTY);
		std::vector<Money> old_values(this->keys.size() * 2);
		old_keys.swap(this->keys);
		old_values.swap(this->values);
		this->mask = this->keys.size() - 1;
		for (size_t i = 0; i < old_keys.size(); i++) {
			if (old_keys[i] == EMPTY) continue;
			size_t slot = this->Slot(old_keys[i]);
			while (this->keys[slot] != EMPTY) slot = (slot + 1) & this->mask;
			this->keys[slot] = old_keys[i];
			this->values[slot] = old_values[i];
		}
	}
};

/** Pack a tile position into one key. */
static uint32_t PackTile(int x, int y)
{
	return static_cast<uint32_t>(x + 32768) | (static_cast<uint32_t>(y + 32768) << 16);
}

/** The text of the moves of one search step: F, L, R, B<n>, or a pair of moves. */
struct MoveText {
	std::array<char, 4> c{};
	uint8_t n = 0;

	void Add(char ch)
	{
		assert(this->n < 3);
		this->c[this->n++] = ch;
	}
};

/** A node in the search tree. */
struct TreeNode {
	RouterNode state;
	Money g;
	int parent;
	MoveText moves; ///< The moves from the parent to this node.
};

/** The static facts of one search. */
class Router {
public:
	Router(const JunctionLayout &layout, const JunctionRouteRequest &request) : request(request)
	{
		this->grid_min_x = request.min.x - 12;
		this->grid_min_y = request.min.y - 12;
		this->grid_w = request.max.x + 12 - this->grid_min_x + 1;
		this->grid_h = request.max.y + 12 - this->grid_min_y + 1;
		this->track_grid.assign(static_cast<size_t>(this->grid_w) * this->grid_h, TrackBits{});
		this->flag_grid.assign(static_cast<size_t>(this->grid_w) * this->grid_h, 0);

		for (const JunctionTrack &jt : layout.tracks) {
			const int gi = this->GridIndex(jt.offset.x, jt.offset.y);
			if (gi >= 0) {
				this->track_grid[gi] |= jt.tracks;
			} else {
				tracks[PackTile(jt.offset.x, jt.offset.y)] |= jt.tracks;
			}
		}
		for (const JunctionBridge &b : layout.bridges) {
			bridges.push_back(b);
			const int gs = this->GridIndex(b.start.x, b.start.y);
			if (gs >= 0) {
				this->flag_grid[gs] |= 1;
			} else {
				heads.insert(PackTile(b.start.x, b.start.y));
			}
			const int ge = this->GridIndex(b.end.x, b.end.y);
			if (ge >= 0) {
				this->flag_grid[ge] |= 1;
			} else {
				heads.insert(PackTile(b.end.x, b.end.y));
			}
		}
		for (const TileIndexDiffC &c : request.crossable) {
			const int gi = this->GridIndex(c.x, c.y);
			if (gi >= 0) {
				this->flag_grid[gi] |= 2;
			} else {
				crossable.insert(PackTile(c.x, c.y));
			}
		}
		for (const JunctionRouteEnd &g : request.goals) {
			goals.insert(PackTile(g.tile.x, g.tile.y) | (uint64_t(g.trackdir) << 32));
			const int gi = this->GridIndex(g.tile.x, g.tile.y);
			if (gi >= 0) {
				this->flag_grid[gi] |= 4;
			} else {
				goal_tiles.insert(PackTile(g.tile.x, g.tile.y));
			}
		}
		cap = std::max(request.curve_spacing, request.s_bend_window);
	}

	std::optional<JunctionRoute> Run();

private:
	const JunctionRouteRequest &request;
	std::unordered_map<uint32_t, TrackBits> tracks;
	std::vector<JunctionBridge> bridges;
	std::unordered_set<uint32_t> heads;
	std::unordered_set<uint32_t> crossable;
	std::unordered_set<uint64_t> goals;
	std::unordered_set<uint32_t> goal_tiles;
	uint cap = 0;
	int grid_min_x = 0;
	int grid_min_y = 0;
	int grid_w = 0;
	int grid_h = 0;
	std::vector<TrackBits> track_grid;
	std::vector<uint8_t> flag_grid; ///< 1 head, 2 crossable, 4 goal tile.

	int GridIndex(int x, int y) const
	{
		const int gx = x - this->grid_min_x;
		const int gy = y - this->grid_min_y;
		if (gx < 0 || gy < 0 || gx >= this->grid_w || gy >= this->grid_h) return -1;
		return gy * this->grid_w + gx;
	}

	bool InBounds(int x, int y) const { return x >= request.min.x && x <= request.max.x && y >= request.min.y && y <= request.max.y; }
	bool IsGoal(int x, int y, Trackdir td) const { return goals.contains(PackTile(x, y) | (uint64_t(td) << 32)); }
	bool BetweenBridgeHeads(int x, int y) const;
	bool TileFree(int x, int y) const;
	Money TerrainCost(int x, int y) const;
	bool ConflictsAt(int x, int y, Track t) const;
	bool TurnAllowed(const RouterNode &from, uint8_t hand, bool &set_pair) const;
	bool MakePiece(const RouterNode &from, char move, RouterNode &to, bool &conflict) const;
	Money Heuristic(const RouterNode &n) const;
	void Successors(const RouterNode &from, std::vector<std::pair<RouterNode, std::pair<Money, MoveText>>> &out) const;
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
	const int gi = this->GridIndex(x, y);
	if (gi >= 0) return this->track_grid[gi].None();
	const auto it = tracks.find(PackTile(x, y));
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
	TrackBits held;
	const int gi = this->GridIndex(x, y);
	if (gi >= 0) {
		held = this->track_grid[gi];
	} else {
		const auto it = tracks.find(PackTile(x, y));
		if (it == tracks.end()) return false;
		held = it->second;
	}
	for (Track e : held) {
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

	const int gi = this->GridIndex(to.x, to.y);
	if (!InBounds(to.x, to.y) || (gi >= 0 ? (this->flag_grid[gi] & 1) != 0 : heads.contains(PackTile(to.x, to.y)))) return false;
	if (request.terrain != nullptr && !request.terrain->UsableAt(to.x, to.y, request.plane)) return false;
	if (IsGoal(to.x, to.y, next) || (gi >= 0 ? (this->flag_grid[gi] & 2) != 0 : crossable.contains(PackTile(to.x, to.y)))) return true;
	if (from.first) {
		if (gi >= 0) {
			if (this->track_grid[gi].Test(TrackdirToTrack(from.trackdir))) return true;
		} else {
			const auto it = tracks.find(PackTile(to.x, to.y));
			if (it != tracks.end() && it->second.Test(TrackdirToTrack(from.trackdir))) return true;
		}
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

void Router::Successors(const RouterNode &from, std::vector<std::pair<RouterNode, std::pair<Money, MoveText>>> &out) const
{
	for (char move : {'F', 'L', 'R'}) {
		/* The track beyond a bridge's far head must continue in line with the bridge (G5). */
		if (from.on_bridge && move != 'F') continue;
		RouterNode to;
		bool conflict;
		if (!MakePiece(from, move, to, conflict)) continue;
		if (!conflict) {
			Money cost = request.prices.rail_piece + TerrainCost(to.x, to.y);
			const int gi = this->GridIndex(to.x, to.y);
			if (gi >= 0 ? (this->flag_grid[gi] & 2) != 0 : crossable.contains(PackTile(to.x, to.y))) cost += request.crossable_cost;
			if (move != 'F') cost += request.turn_cost;
			MoveText text;
			text.Add(move);
			out.push_back({to, {cost, text}});
			continue;
		}
		/* Exception 2: a piece on a merge tile, followed at once by a goal piece. */
		const int gt = this->GridIndex(to.x, to.y);
		if (!(gt >= 0 ? (this->flag_grid[gt] & 4) != 0 : goal_tiles.contains(PackTile(to.x, to.y)))) continue;
		for (char move2 : {'F', 'L', 'R'}) {
			RouterNode to2;
			bool conflict2;
			if (!MakePiece(to, move2, to2, conflict2)) continue;
			if (!IsGoal(to2.x, to2.y, to2.trackdir)) continue;
			Money cost2 = request.prices.rail_piece * 2 + TerrainCost(to.x, to.y) + TerrainCost(to2.x, to2.y);
			if (move != 'F') cost2 += request.turn_cost;
			if (move2 != 'F') cost2 += request.turn_cost;
			MoveText text;
			text.Add(move);
			text.Add(move2);
			out.push_back({to2, {cost2, text}});
		}
	}

	/* D113: a start piece is where the route splits off its track; a bridge directly after it would put the split in the bridge's block. */
	if (from.first || from.on_bridge || !IsDiagonalTrackdir(from.trackdir) || !request.bridge_span_ok) return;
	const TileIndexDiffC step = TileIndexDiffCByDiagDir(TrackdirToExitdir(from.trackdir));
	for (uint n = 1; n <= request.max_bridge_middle; n++) {
		if (!request.bridge_span_ok(n)) continue;
		bool ok = true;
		for (uint k = 1; k <= n + 2 && ok; k++) {
			const int x = from.x + static_cast<int>(k) * step.x;
			const int y = from.y + static_cast<int>(k) * step.y;
			const int gi = this->GridIndex(x, y);
			if (!InBounds(x, y) || (gi >= 0 ? (this->flag_grid[gi] & 1) != 0 : heads.contains(PackTile(x, y)))) {
				ok = false;
			} else if (k == 1 || k == n + 2) {
				ok = TileFree(x, y) && !(gi >= 0 ? (this->flag_grid[gi] & 2) != 0 : crossable.contains(PackTile(x, y))) && (request.terrain == nullptr || request.terrain->UsableAt(x, y, request.plane));
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
		MoveText text;
		text.Add('B');
		if (n >= 10) text.Add(static_cast<char>('0' + n / 10));
		text.Add(static_cast<char>('0' + n % 10));
		out.push_back({to, {bridge_cost, text}});
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
	tree.reserve(1024);
	BestCostTable best(4096);
	using QueueItem = std::tuple<int64_t, int64_t, uint64_t, int>; // f, g, counter, tree index
	std::vector<QueueItem> open_storage;
	open_storage.reserve(1024);
	std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<QueueItem>> open(std::greater<QueueItem>(), std::move(open_storage));
	uint64_t counter = 0;

	auto push = [&](const RouterNode &state, Money g, int parent, MoveText moves) {
		if (!best.Improve(PackKey(state), g)) return;
		tree.push_back({state, g, parent, moves});
		const int index = static_cast<int>(tree.size()) - 1;
		open.push({(g + Heuristic(state)).base(), g.base(), counter++, index});
	};

	for (const JunctionRouteEnd &start : request.starts) {
		push({start.tile.x, start.tile.y, start.trackdir, cap, 0, true, false, true, false}, 0, -1, MoveText{});
	}

	uint expansions = 0;
	std::vector<std::pair<RouterNode, std::pair<Money, MoveText>>> successors;
	while (!open.empty()) {
		const auto [f, g, order, index] = open.top();
		open.pop();
		const RouterNode s = tree[index].state;
		const Money node_g = tree[index].g;
		if (best.Get(PackKey(s)) < node_g) continue;

		if (!s.first && !s.on_bridge && IsGoal(s.x, s.y, s.trackdir)) {
			std::vector<int> chain;
			int i = index;
			for (; tree[i].parent != -1; i = tree[i].parent) chain.push_back(i);
			std::string moves;
			for (auto it = chain.rbegin(); it != chain.rend(); ++it) moves.append(tree[*it].moves.c.data(), tree[*it].moves.n);
			JunctionRoute route{{static_cast<int16_t>(tree[i].state.x), static_cast<int16_t>(tree[i].state.y)}, tree[i].state.trackdir, moves};
			if (SelfCrossing(route)) return std::nullopt;
			return route;
		}

		if (++expansions > request.max_expansions) return std::nullopt;
		successors.clear();
		Successors(s, successors);
		for (auto &[to, cost] : successors) push(to, node_g + cost.first, index, std::move(cost.second));
	}
	return std::nullopt;
}

} // namespace

/** Every input the router reads, flattened; bridge_span_ok, prices and terrain are fixed per cache scope. */
static std::vector<int32_t> RouteCacheKey(const JunctionLayout &layout, const JunctionRouteRequest &request)
{
	std::vector<int32_t> key;
	auto add_money = [&](Money m) {
		key.push_back(static_cast<int32_t>(static_cast<uint64_t>(m.base()) >> 32));
		key.push_back(static_cast<int32_t>(static_cast<uint64_t>(m.base()) & 0xFFFFFFFFu));
	};
	auto add_ends = [&](const std::vector<JunctionRouteEnd> &ends) {
		key.push_back(static_cast<int32_t>(ends.size()));
		for (const JunctionRouteEnd &end : ends) {
			key.push_back(end.tile.x);
			key.push_back(end.tile.y);
			key.push_back(static_cast<int32_t>(end.trackdir));
		}
	};
	add_ends(request.starts);
	add_ends(request.goals);
	for (const int32_t v : {request.min.x, request.min.y, request.max.x, request.max.y}) key.push_back(v);
	key.push_back(static_cast<int32_t>(request.curve_spacing));
	key.push_back(static_cast<int32_t>(request.s_bend_window));
	key.push_back(static_cast<int32_t>(request.max_bridge_middle));
	key.push_back(static_cast<int32_t>(request.max_expansions));
	key.push_back(request.plane);
	key.push_back(static_cast<int32_t>(request.crossable.size()));
	for (const TileIndexDiffC &t : request.crossable) {
		key.push_back(t.x);
		key.push_back(t.y);
	}
	add_money(request.crossable_cost);
	add_money(request.turn_cost);
	key.push_back(static_cast<int32_t>(layout.tracks.size()));
	for (const JunctionTrack &jt : layout.tracks) {
		key.push_back(jt.offset.x);
		key.push_back(jt.offset.y);
		key.push_back(static_cast<int32_t>(jt.tracks.base()));
	}
	key.push_back(static_cast<int32_t>(layout.bridges.size()));
	for (const JunctionBridge &b : layout.bridges) {
		key.push_back(b.start.x);
		key.push_back(b.start.y);
		key.push_back(b.end.x);
		key.push_back(b.end.y);
	}
	return key;
}

bool JunctionRouteCache::Lookup(const std::vector<int32_t> &key, std::optional<JunctionRoute> &result)
{
	std::lock_guard<std::mutex> lock(this->mutex);
	const auto it = this->results.find(key);
	if (it == this->results.end()) {
		this->misses++;
		return false;
	}
	result = it->second;
	this->hits++;
	return true;
}

void JunctionRouteCache::Store(std::vector<int32_t> key, const std::optional<JunctionRoute> &result)
{
	std::lock_guard<std::mutex> lock(this->mutex);
	this->results.try_emplace(std::move(key), result);
}

/**
 * Find the cheapest route from a start piece to a goal piece.
 * @param layout What is already laid.
 * @param request What to route.
 * @param cache When set, results are looked up and stored here.
 * @return The route, or std::nullopt when none is found.
 */
std::optional<JunctionRoute> FindJunctionRoute(const JunctionLayout &layout, const JunctionRouteRequest &request, JunctionRouteCache *cache)
{
	std::vector<int32_t> key;
	if (cache != nullptr) {
		key = RouteCacheKey(layout, request);
		std::optional<JunctionRoute> cached;
		if (cache->Lookup(key, cached)) return cached;
	}
	Router router(layout, request);
	std::optional<JunctionRoute> route = router.Run();
	if (cache != nullptr) cache->Store(std::move(key), route);
	return route;
}
