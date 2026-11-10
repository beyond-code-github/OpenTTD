/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_organic.cpp Junction movements routed together by negotiated congestion (DESIGN-O4 revision 3). */

#include "stdafx.h"

#include "junction_organic.h"

#include "junction_line_router.h"
#include "junction_rules_internal.h"
#include "junction_route.h"
#include "junction_signal_pass.h"
#include "junction_throughput.h"
#include "junction_train_model.h"
#include "track_func.h"

#include "3rdparty/fmt/format.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <map>
#include <numeric>
#include <ranges>
#include <set>
#include <unordered_map>
#include <unordered_set>

#include "safeguards.h"

std::vector<JunctionPort> MakeOrganicTPorts(const JunctionSpec &spec, int width, int height, int p, int top)
{
	auto tile = [](int x, int y) { return TileIndexDiffC{static_cast<int16_t>(x), static_cast<int16_t>(y)}; };
	const int row2 = spec.ml_gap + 1 + top;
	return {
		{JunctionPortRole::EIn, tile(0, top), Trackdir::X_SW},
		{JunctionPortRole::WOut, tile(0, row2), Trackdir::X_NE},
		{JunctionPortRole::EOut, tile(width - 1, top), Trackdir::X_SW},
		{JunctionPortRole::WIn, tile(width - 1, row2), Trackdir::X_NE},
		{JunctionPortRole::NIn, tile(p, height - 1), Trackdir::Y_NW},
		{JunctionPortRole::SOut, tile(p + spec.branch_gap + 1, height - 1), Trackdir::Y_SE},
	};
}


namespace {

/** One routed piece and the resource it occupies. */
struct OrganicOccupant {
	int movement; ///< Index of the movement in the expected list.
	TileIndexDiffC tile;
	Track bit;
	Trackdir trackdir;
	JunctionResourceLevel level;
	int8_t z;
};

/** One movement: its ports, its route and the resources the route occupies. */
struct OrganicMovement {
	JunctionPortRole from;
	JunctionPortRole to;
	JunctionLineRoute route{};
	JunctionRaster raster;
	std::vector<OrganicOccupant> occupants;
	bool routed = false;
};

/** Pieces by tile, level and z, for finding what a piece conflicts with. */
using OrganicIndex = std::unordered_multimap<uint64_t, const OrganicOccupant *>;

constexpr JunctionLineSpec ORGANIC_LINE{1, 1, 1};

static uint64_t TileKey(TileIndexDiffC tile)
{
	return (static_cast<uint64_t>(static_cast<uint16_t>(tile.x)) << 16) | static_cast<uint64_t>(static_cast<uint16_t>(tile.y));
}

static uint64_t ResourceKey(const OrganicOccupant &occupant)
{
	return (TileKey(occupant.tile) << 16) | (static_cast<uint64_t>(occupant.level) << 11) | (static_cast<uint64_t>(static_cast<uint8_t>(occupant.z)) << 3) | static_cast<uint64_t>(occupant.bit);
}

/** Whether two pieces of different movements share their resource for free: same bit, trackdir, level and z, and a common entry or exit port. */
static bool Compatible(const OrganicOccupant &a, const OrganicOccupant &b, const std::vector<OrganicMovement> &movements)
{
	if (a.bit != b.bit || a.trackdir != b.trackdir || a.level != b.level || a.z != b.z) return false;
	const OrganicMovement &ma = movements[a.movement];
	const OrganicMovement &mb = movements[b.movement];
	return ma.from == mb.from || ma.to == mb.to;
}

/** Whether two plain pieces of different bits form a switch: a split (same entry port, both enter through the same edge) or a merge (same exit port, both leave through the same edge). */
static bool IsSwitch(const OrganicOccupant &a, const OrganicOccupant &b, const std::vector<OrganicMovement> &movements)
{
	const OrganicMovement &ma = movements[a.movement];
	const OrganicMovement &mb = movements[b.movement];
	if (ma.from == mb.from && TrackdirToExitdir(ReverseTrackdir(a.trackdir)) == TrackdirToExitdir(ReverseTrackdir(b.trackdir))) return true;
	return ma.to == mb.to && TrackdirToExitdir(a.trackdir) == TrackdirToExitdir(b.trackdir);
}

/** Whether two pieces on the same tile conflict (a and b are of different movements). A head is exclusive but for the same head of a compatible movement. */
static bool Conflicts(const OrganicOccupant &a, const OrganicOccupant &b, const std::vector<OrganicMovement> &movements)
{
	if (a.movement == b.movement || a.tile.x != b.tile.x || a.tile.y != b.tile.y) return false;
	if (a.level == JunctionResourceLevel::Head || b.level == JunctionResourceLevel::Head) return !Compatible(a, b, movements);
	if (a.level != b.level || a.z != b.z) return false;
	if (a.level != JunctionResourceLevel::Surface) return true;
	if (a.bit == b.bit) return !Compatible(a, b, movements);
	if (!TracksOverlap(TrackBits{a.bit} | b.bit)) return false;
	return !IsSwitch(a, b, movements);
}

/** The occupants of a raster: heads and plain pieces at the surface, bridge middles on the span level, and the companion pieces of a prio window (the line's own track at the surface). */
static std::vector<OrganicOccupant> MakeOccupants(int movement, const JunctionRaster &raster, const std::vector<JunctionPiece> &companion, int8_t z)
{
	std::vector<OrganicOccupant> result;
	for (const JunctionPiece &piece : raster.pieces) {
		JunctionResourceLevel level = JunctionResourceLevel::Surface;
		if (piece.on_bridge) {
			const bool head = std::ranges::any_of(raster.bridges, [&](const JunctionBridge &bridge) {
				return (piece.tile.x == bridge.start.x && piece.tile.y == bridge.start.y) || (piece.tile.x == bridge.end.x && piece.tile.y == bridge.end.y);
			});
			level = head ? JunctionResourceLevel::Head : JunctionResourceLevel::BridgeSpan;
		}
		result.push_back({movement, piece.tile, TrackdirToTrack(piece.trackdir), piece.trackdir, level, z});
	}
	for (const JunctionPiece &piece : companion) result.push_back({movement, piece.tile, TrackdirToTrack(piece.trackdir), piece.trackdir, JunctionResourceLevel::Surface, z});
	return result;
}

static const JunctionPort *FindPort(const std::vector<JunctionPort> &ports, JunctionPortRole role)
{
	auto it = std::ranges::find(ports, role, &JunctionPort::role);
	return it == ports.end() ? nullptr : &*it;
}

/**
 * Count the conflicting piece pairs among all routed movements and collect the resources in conflict.
 * @param movements The movements.
 * @param resources Receives the resource keys of every piece in a conflict.
 * @param involved Receives the movements with a piece in a conflict.
 * @return The number of conflicting piece pairs.
 */
static int CountConflicts(const std::vector<OrganicMovement> &movements, std::set<uint64_t> &resources, std::set<int> &involved)
{
	OrganicIndex index;
	for (const OrganicMovement &movement : movements) {
		for (const OrganicOccupant &occupant : movement.occupants) index.emplace(TileKey(occupant.tile), &occupant);
	}
	int pairs = 0;
	for (const OrganicMovement &movement : movements) {
		for (const OrganicOccupant &occupant : movement.occupants) {
			auto [first, last] = index.equal_range(TileKey(occupant.tile));
			for (auto it = first; it != last; ++it) {
				/* Count each pair once, from the lower movement. */
				if (it->second->movement <= occupant.movement || !Conflicts(occupant, *it->second, movements)) continue;
				pairs++;
				resources.insert(ResourceKey(occupant));
				resources.insert(ResourceKey(*it->second));
				involved.insert(occupant.movement);
				involved.insert(it->second->movement);
			}
		}
	}
	return pairs;
}

/** The key of a piece: its tile and trackdir. */
static uint64_t PieceKey(const JunctionPiece &piece)
{
	return (TileKey(piece.tile) << 8) | static_cast<uint64_t>(piece.trackdir);
}

/** Whether a movement is one of the two main-line through movements of a T. */
static bool IsMainLineMovement(JunctionPortRole from, JunctionPortRole to)
{
	return (from == JunctionPortRole::EIn && to == JunctionPortRole::EOut) || (from == JunctionPortRole::WIn && to == JunctionPortRole::WOut);
}

/** The tiles of the (up to) window pieces of a main line's route upstream of its piece at, in route order. */
static std::vector<TileIndexDiffC> WindowTiles(const std::vector<JunctionPiece> &line, size_t at, size_t window)
{
	std::vector<TileIndexDiffC> tiles;
	for (size_t i = at > window ? at - window : 0; i < at; i++) tiles.push_back(line[i].tile);
	return tiles;
}

}

std::vector<std::pair<int, int>> JunctionForcedPairs(const std::vector<JunctionPort> &ports, int width, int height)
{
	/* Clockwise position of a boundary tile: along the top row, down the right column, back along the bottom row, up the left column. */
	auto position = [&](JunctionPortRole role) {
		const JunctionPort *port = FindPort(ports, role);
		if (port == nullptr) return -1;
		const int x = port->tile.x;
		const int y = port->tile.y;
		if (y == 0) return x;
		if (x == width - 1) return width - 1 + y;
		if (y == height - 1) return (width - 1) + (height - 1) + (width - 1 - x);
		return 2 * (width - 1) + (height - 1) + (height - 1 - y);
	};

	const auto &movements = JunctionTMovements();
	std::vector<std::pair<int, int>> pairs;
	for (size_t a = 0; a < movements.size(); a++) {
		for (size_t b = a + 1; b < movements.size(); b++) {
			const auto &[a_from, a_to] = movements[a];
			const auto &[b_from, b_to] = movements[b];
			if (a_from == b_from || a_from == b_to || a_to == b_from || a_to == b_to) continue;
			const int lo = std::min(position(a_from), position(a_to));
			const int hi = std::max(position(a_from), position(a_to));
			int between = 0;
			for (const int q : {position(b_from), position(b_to)}) between += lo < q && q < hi ? 1 : 0;
			if (between == 1) pairs.emplace_back(static_cast<int>(a), static_cast<int>(b));
		}
	}
	return pairs;
}

std::vector<JunctionTMap> JunctionTMaps(const JunctionSpec &spec, const std::vector<JunctionPort> &ports, int width, int height)
{
	const auto &movements = JunctionTMovements();
	auto index_of = [&](JunctionPortRole from, JunctionPortRole to) {
		return static_cast<int>(std::ranges::find(movements, std::make_pair(from, to)) - movements.begin());
	};
	const std::vector<std::pair<int, int>> pairs = JunctionForcedPairs(ports, width, height);

	/* Every assignment of the forced pairs: bit i of a mask set means the lower movement of pair i goes over. Main-line movements over come first (D156), then by mask. */
	std::vector<uint> masks(size_t{1} << pairs.size());
	std::iota(masks.begin(), masks.end(), 0u);
	auto main_line_over = [&](uint mask) {
		int count = 0;
		for (size_t i = 0; i < pairs.size(); i++) {
			const int over = (mask >> i & 1) != 0 ? pairs[i].first : pairs[i].second;
			count += IsMainLineMovement(movements[over].first, movements[over].second) ? 1 : 0;
		}
		return count;
	};
	std::ranges::stable_sort(masks, [&](uint a, uint b) { return main_line_over(a) > main_line_over(b); });

	/* The merges into a main line, the N->W one first: its window may end in any chunk of 6 tiles of the span from its own length in from the entry port to the last column before the exit port. */
	struct Merge {
		int line;
		int joiner;
		int row; ///< The main line's row.
		std::vector<std::pair<int, int>> chunks;
	};
	const int window = spec.prio_tiles + spec.sd + 1;
	std::vector<Merge> merges;
	for (const auto &[from, to] : {std::make_pair(JunctionPortRole::WIn, JunctionPortRole::WOut), std::make_pair(JunctionPortRole::EIn, JunctionPortRole::EOut)}) {
		const JunctionPort *entry = FindPort(ports, from);
		const JunctionPort *exit = FindPort(ports, to);
		if (entry == nullptr || exit == nullptr) return {};
		const int dir = exit->tile.x > entry->tile.x ? 1 : -1;
		const int first = entry->tile.x + dir * window;
		const int last = exit->tile.x - dir;
		Merge merge{index_of(from, to), index_of(JunctionPortRole::NIn, to), entry->tile.y, {}};
		for (int start = std::min(first, last); start <= std::max(first, last); start += 6) merge.chunks.emplace_back(start, std::min(start + 5, std::max(first, last)));
		merges.push_back(std::move(merge));
	}

	const JunctionPort *n_in = FindPort(ports, JunctionPortRole::NIn);
	if (n_in == nullptr) return {};
	auto distance = [](TileIndexDiffC a, TileIndexDiffC b) { return std::abs(a.x - b.x) + std::abs(a.y - b.y); };

	std::vector<JunctionTMap> result;
	for (const uint mask : masks) {
		std::vector<std::pair<int, int>> over;
		for (size_t i = 0; i < pairs.size(); i++) {
			const bool first_over = (mask >> i & 1) != 0;
			over.emplace_back(first_over ? pairs[i].first : pairs[i].second, first_over ? pairs[i].second : pairs[i].first);
		}
		for (const auto &chunk_w : merges[0].chunks) {
			for (const auto &chunk_e : merges[1].chunks) {
				JunctionTMap map;
				map.over = over;
				const std::pair<int, int> chunks[] = {chunk_w, chunk_e};
				for (size_t k = 0; k < merges.size(); k++) map.windows.push_back({merges[k].line, merges[k].joiner, -1, chunks[k].first, chunks[k].second});

				/* The bound: every movement's Manhattan distance between its ports, but a joiner's is at least the distance to the nearest end of its window's chunk. */
				for (size_t m = 0; m < movements.size(); m++) {
					const JunctionPort *from = FindPort(ports, movements[m].first);
					const JunctionPort *to = FindPort(ports, movements[m].second);
					if (from == nullptr || to == nullptr) return {};
					int length = distance(from->tile, to->tile);
					for (size_t k = 0; k < merges.size(); k++) {
						if (merges[k].joiner != static_cast<int>(m)) continue;
						const int near = std::min(distance(n_in->tile, {static_cast<int16_t>(chunks[k].first), static_cast<int16_t>(merges[k].row)}), distance(n_in->tile, {static_cast<int16_t>(chunks[k].second), static_cast<int16_t>(merges[k].row)}));
						length = std::max(length, near);
					}
					map.bound += length;
				}
				result.push_back(std::move(map));
			}
		}
	}
	std::ranges::stable_sort(result, [](const JunctionTMap &a, const JunctionTMap &b) { return a.bound < b.bound; });
	return result;
}

JunctionNegotiation NegotiateJunctionMovements(const JunctionSpec &spec, const std::vector<JunctionPort> &ports, int width, int height, const JunctionTerrain *terrain, int max_rounds, const JunctionNegotiationPrices &prices)
{
	JunctionNegotiation result;

	std::vector<OrganicMovement> movements;
	for (const auto &[from, to] : JunctionTMovements()) {
		if (FindPort(ports, from) == nullptr || FindPort(ports, to) == nullptr) {
			result.error = "missing port";
			return result;
		}
		OrganicMovement movement;
		movement.from = from;
		movement.to = to;
		movements.push_back(std::move(movement));
	}

	const Money unit = spec.prices.rail_piece;
	const int8_t z = 0; // Flat ground: every piece is at the plane.
	std::unordered_map<uint64_t, Money> local_history;
	std::unordered_map<uint64_t, Money> &history = prices.history != nullptr ? *prices.history : local_history;
	std::set<int> to_route; // Movements to route this round.

	/* The box boundary holds only ports: the bottom row and the side columns carry no track and no bridge span but at the port tiles. */
	std::vector<TileIndexDiffC> blocked;
	auto is_port_tile = [&](int x, int y) { return std::ranges::any_of(ports, [&](const JunctionPort &port) { return port.tile.x == x && port.tile.y == y; }); };
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			if ((y == height - 1 || x == 0 || x == width - 1) && !is_port_tile(x, y)) blocked.push_back({static_cast<int16_t>(x), static_cast<int16_t>(y)});
		}
	}

	constexpr uint MAX_BRIDGE_MIDDLE = 10;
	const Money cleanup_price = Money{unit.base() * 1000};

	/* Forced pairs (D167): role[m][o] is 1 when m goes over o, -1 when m goes under o, 0 when the two are not a forced pair. */
	std::vector<std::vector<int>> role(movements.size(), std::vector<int>(movements.size(), 0));
	for (const auto &[over, under] : prices.map != nullptr ? prices.map->over : prices.over) {
		role[over][under] = 1;
		role[under][over] = -1;
	}

	/* The pieces of the other movements and the tiles on or beside them (D166 a): the prices of movement m's routes are measured against these. */
	auto gather = [&](size_t m, OrganicIndex &others, std::unordered_set<uint64_t> &near) {
		for (size_t o = 0; o < movements.size(); o++) {
			if (o == m || !movements[o].routed) continue;
			for (const OrganicOccupant &occupant : movements[o].occupants) {
				others.emplace(TileKey(occupant.tile), &occupant);
				for (int dy = -1; dy <= 1; dy++) {
					for (int dx = -1; dx <= 1; dx++) near.insert(TileKey({static_cast<int16_t>(occupant.tile.x + dx), static_cast<int16_t>(occupant.tile.y + dy)}));
				}
			}
		}
	};

	const size_t window = static_cast<size_t>(spec.prio_tiles + spec.sd + 1); // W (D108, D148): the tiles of a merge's prio window.

	/* The prio windows of the map (DESIGN-O4 revision 5), and where each main line's window ended in its current route: the merge piece is the next one. */
	const std::vector<JunctionTMap::Window> no_windows;
	const std::vector<JunctionTMap::Window> &windows = prices.apply_rules && prices.map != nullptr ? prices.map->windows : no_windows;
	std::vector<size_t> prio_end(movements.size(), 0);
	auto window_of_line = [&](size_t m) -> const JunctionTMap::Window * {
		const auto it = std::ranges::find(windows, static_cast<int>(m), &JunctionTMap::Window::line);
		return it == windows.end() ? nullptr : &*it;
	};
	auto window_of_joiner = [&](size_t m) -> const JunctionTMap::Window * {
		const auto it = std::ranges::find(windows, static_cast<int>(m), &JunctionTMap::Window::joiner);
		return it == windows.end() ? nullptr : &*it;
	};

	/* The rule masks of the movement being routed (rebuilt for each request from the others' current routes). */
	std::unordered_set<uint64_t> shares_from_keys;
	std::unordered_set<uint64_t> shares_to_keys;
	std::unordered_set<uint64_t> split_blocked;

	/* The movement rules of a turning movement m, from the other movements' current routes (DESIGN-O4 revision 4, D168). */
	auto make_rules = [&](size_t m) {
		shares_from_keys.clear();
		shares_to_keys.clear();
		split_blocked.clear();
		for (size_t o = 0; o < movements.size(); o++) {
			if (o == m || !movements[o].routed) continue;
			for (const JunctionPiece &piece : movements[o].raster.pieces) {
				if (movements[o].from == movements[m].from) shares_from_keys.insert(PieceKey(piece));
				if (movements[o].to == movements[m].to) shares_to_keys.insert(PieceKey(piece));
			}
		}

		JunctionMovementRules rules;
		rules.shares_from = [&](const JunctionPiece &piece) { return shares_from_keys.contains(PieceKey(piece)); };
		rules.shares_to = [&](const JunctionPiece &piece) { return shares_to_keys.contains(PieceKey(piece)); };
		rules.wait_units = 2 * spec.tl;
		rules.plain_before_head = 1;

		for (size_t ml = 0; ml < movements.size(); ml++) {
			if (!IsMainLineMovement(movements[ml].from, movements[ml].to) || !movements[ml].routed || movements[ml].from != movements[m].from) continue;
			/* m splits off this main line: no split on the main line within W tiles upstream of the merge that follows its prio window (D108). */
			const std::vector<JunctionPiece> &line = movements[ml].raster.pieces;
			if (window_of_line(ml) != nullptr && prio_end[ml] + 1 < line.size()) {
				for (const TileIndexDiffC &tile : WindowTiles(line, prio_end[ml] + 1, window)) split_blocked.insert(TileKey(tile));
				split_blocked.insert(TileKey(line[prio_end[ml] + 1].tile));
			}
			rules.may_split = [&](const JunctionPiece &piece) { return !split_blocked.contains(TileKey(piece.tile)); };
		}
		return rules;
	};

	/* The router request of movement m, its conflicting pieces priced by pf (the clean-up pass: prohibitively, without history); others and near must outlive its use. */
	auto build_request = [&](size_t m, const OrganicIndex &others, const std::unordered_set<uint64_t> &near, double pf, bool cleanup) {
		const JunctionPort &from = *FindPort(ports, movements[m].from);
		const JunctionPort &to = *FindPort(ports, movements[m].to);

		JunctionLineRouteRequest request;
		request.line = ORGANIC_LINE;
		request.start = from.tile;
		request.start_trackdir = from.trackdir;
		request.goals = {{to.tile, to.trackdir}};
		request.min = {0, 0};
		request.max = {static_cast<int16_t>(width - 1), static_cast<int16_t>(height - 1)};
		request.blocked = blocked;
		request.terrain = terrain;
		request.plane = z;
		request.curve_spacing = spec.curve_limit ? DeriveCurveSpacing(spec.reference_speed, spec.curve_speed, spec.tl) : 1;
		request.s_bend_window = 2u * spec.tl;
		request.rail_piece = spec.prices.rail_piece;
		const bool main_line = IsMainLineMovement(movements[m].from, movements[m].to);
		request.turn_cost = main_line && prices.ml_turn_cost != 0 ? prices.ml_turn_cost : spec.prices.rail_piece / 2;
		request.min_units = m < prices.min_units.size() ? prices.min_units[m] : 0;
		request.bridge_span_ok = spec.bridge_span_ok;
		request.bridge_price = spec.prices.bridge;
		request.max_bridge_middle = MAX_BRIDGE_MIDDLE;
		if (!main_line && prices.apply_rules) request.rules = make_rules(m);
		if (const JunctionTMap::Window *line_window = window_of_line(m); line_window != nullptr) {
			/* The main line carries its prio with it (DESIGN-O4 revision 5). */
			request.prio_window = JunctionPrioWindow{static_cast<uint>(2 * window), line_window->side, line_window->end_lo, line_window->end_hi};
		}
		if (const JunctionTMap::Window *joiner_window = window_of_joiner(m); joiner_window != nullptr && movements[joiner_window->line].routed) {
			/* The joiner ends at the merge piece, the one after its main line's window. */
			const std::vector<JunctionPiece> &line = movements[joiner_window->line].raster.pieces;
			if (prio_end[joiner_window->line] + 1 < line.size()) request.goals = {{line[prio_end[joiner_window->line] + 1].tile, line[prio_end[joiner_window->line] + 1].trackdir}};
		}
		request.resource_cost = [&, m, pf, cleanup](const JunctionPiece &piece, int8_t piece_z, JunctionResourceLevel level) -> Money {
			const OrganicOccupant self{static_cast<int>(m), piece.tile, TrackdirToTrack(piece.trackdir), piece.trackdir, level, piece_z};
			Money cost = 0;
			/* A head on a port tile leaves the checker no approach track outside it: a rule, priced out because the callback can't forbid. */
			if (level == JunctionResourceLevel::Head && std::ranges::any_of(ports, [&](const JunctionPort &port) { return port.tile.x == piece.tile.x && port.tile.y == piece.tile.y; })) cost += Money{unit.base() * 1000};
			Money paid = unit; // What a piece of this resource costs before any clash: U plus its history (PathFinder prices).
			if (!cleanup) {
				if (auto it = history.find(ResourceKey(self)); it != history.end()) paid += it->second;
			}
			/* Hug preference (D166 a): a surface piece with no other movement's piece on or beside its tile. */
			if (level == JunctionResourceLevel::Surface && !near.contains(TileKey(piece.tile))) cost += Money{unit.base() / 8};
			int clashes = 0;
			auto [first, last] = others.equal_range(TileKey(piece.tile));
			for (auto other = first; other != last; ++other) {
				if (Conflicts(self, *other->second, movements)) clashes++;
				if (const int r = role[m][other->second->movement]; r != 0 && other->second->level == JunctionResourceLevel::Surface) {
					/* The forced crossing is decided (D167): at grade, an under movement's span over the over movement, and an over movement's bore under the under movement, are prohibitive. */
					const bool at_grade = level == JunctionResourceLevel::Surface && piece_z == other->second->z && TrackOverlapsTracks(TrackBits{other->second->bit}, self.bit);
					const bool span_over = level == JunctionResourceLevel::BridgeSpan && r < 0;
					const bool bore_under = level == JunctionResourceLevel::TunnelBore && r > 0;
					if (at_grade || span_over || bore_under) cost += Money{unit.base() * 1000};
				}
			}
			if (cleanup) {
				cost += Money{cleanup_price.base() * clashes};
			} else {
				/* (U + h) x (1 + pf x k) - U: the router adds U for the piece itself. */
				cost += Money{static_cast<int64_t>(std::llround(static_cast<double>(paid.base()) * (1.0 + pf * clashes)))} - unit;
			}
			return cost;
		};
		return request;
	};

	/* The cost of a movement's present route as the router would price it under a request (for D166 b). */
	auto route_cost = [&](const JunctionLineRouteRequest &request, const OrganicMovement &movement) {
		Money cost = 0;
		for (char move : movement.route.moves) {
			if (move == 'L' || move == 'R') cost += request.turn_cost;
		}
		const std::vector<JunctionPiece> &pieces = movement.raster.pieces;
		for (size_t k = 0; k < pieces.size(); k++) {
			JunctionResourceLevel level = JunctionResourceLevel::Surface;
			if (pieces[k].on_bridge) {
				const bool head = std::ranges::any_of(movement.raster.bridges, [&](const JunctionBridge &bridge) {
					return (pieces[k].tile.x == bridge.start.x && pieces[k].tile.y == bridge.start.y) || (pieces[k].tile.x == bridge.end.x && pieces[k].tile.y == bridge.end.y);
				});
				level = head ? JunctionResourceLevel::Head : JunctionResourceLevel::BridgeSpan;
			} else if (k > 0) {
				cost += request.rail_piece;
			}
			cost += request.resource_cost(pieces[k], z, level);
		}
		for (const JunctionBridge &bridge : movement.raster.bridges) {
			const int middle = std::max(std::abs(bridge.end.x - bridge.start.x), std::abs(bridge.end.y - bridge.start.y)) - 1;
			cost += request.bridge_price(static_cast<uint>(middle));
		}
		return cost;
	};

	/* Route movement m under a request; false when it has no route or does not rasterize. */
	auto install = [&](size_t m, const JunctionLineRoute &route, size_t window_end) {
		std::vector<JunctionRaster> rasters = RasterizeJunctionLine(ORGANIC_LINE, route);
		if (rasters.empty() || !rasters[0].error.empty()) return false;
		movements[m].route = route;
		movements[m].raster = std::move(rasters[0]);
		movements[m].occupants = MakeOccupants(static_cast<int>(m), movements[m].raster, route.companion, z);
		prio_end[m] = window_end;
		movements[m].routed = true;
		return true;
	};

	/* The two main-line through movements are routed first, then the turning movements (DESIGN-O4 revision 4). */
	std::vector<size_t> order;
	for (const bool main_line : {true, false}) {
		for (size_t m = 0; m < movements.size(); m++) {
			if (IsMainLineMovement(movements[m].from, movements[m].to) == main_line) order.push_back(m);
		}
	}

	double pf = 0.5; // PathFinder's present-sharing factor: 0.5 in round 1, x1.5 each round.
	for (int round = 1; round <= max_rounds; round++, pf *= 1.5) {
		result.rounds = round;

		std::set<int> refresh; // Joiners whose main line was routed again this round: their goal is its new merge piece.
		for (const size_t m : order) {
			if (round > 1 && !to_route.contains(static_cast<int>(m)) && !refresh.contains(static_cast<int>(m))) continue;
			/* The others' current pieces; in round 1 only the movements already routed. */
			OrganicIndex others;
			std::unordered_set<uint64_t> near;
			gather(m, others, near);

			const JunctionLineRouteResult routed = FindJunctionLineRoute(build_request(m, others, near, pf, false));
			result.routes_run++;
			if (!routed.route.has_value()) {
				result.error = fmt::format("no route for movement {} (round {}): {}", m, round, routed.error);
				return result;
			}
			if (!install(m, *routed.route, routed.prio_end)) {
				result.error = fmt::format("movement {} (round {}) does not rasterize", m, round);
				return result;
			}
			if (const JunctionTMap::Window *line_window = window_of_line(m); line_window != nullptr) refresh.insert(line_window->joiner);
		}

		std::set<uint64_t> contested;
		to_route.clear();
		result.conflicts = CountConflicts(movements, contested, to_route);
		result.conflicts_per_round.push_back(result.conflicts);

		if (result.conflicts == 0) break;
		for (uint64_t key : contested) history[key] += unit;
	}

	/* Clean-up (D166 b): reroute each movement without history costs and with every conflict prohibitive, keeping a cheaper conflict-free route, to drop detours the negotiation's history left behind. */
	if (result.conflicts == 0) {
		for (int pass = 0; pass < 3; pass++) {
			bool changed = false;
			for (size_t m = 0; m < movements.size(); m++) {
				/* A main line with a prio window stays: its joiner ends at the merge piece after the window. */
				if (window_of_line(m) != nullptr) continue;
				OrganicIndex others;
				std::unordered_set<uint64_t> near;
				gather(m, others, near);
				const JunctionLineRouteRequest request = build_request(m, others, near, 0.0, true);
				const Money old_cost = route_cost(request, movements[m]);
				const JunctionLineRouteResult routed = FindJunctionLineRoute(request);
				result.routes_run++;
				if (!routed.route.has_value() || !(routed.cost < old_cost)) continue;
				std::vector<JunctionRaster> rasters = RasterizeJunctionLine(ORGANIC_LINE, *routed.route);
				if (rasters.empty() || !rasters[0].error.empty()) continue;
				const std::vector<OrganicOccupant> occupants = MakeOccupants(static_cast<int>(m), rasters[0], routed.route->companion, z);
				const bool clash = std::ranges::any_of(occupants, [&](const OrganicOccupant &occupant) {
					auto [first, last] = others.equal_range(TileKey(occupant.tile));
					return std::any_of(first, last, [&](const auto &other) { return Conflicts(occupant, *other.second, movements); });
				});
				if (clash) continue;
				install(m, *routed.route, routed.prio_end);
				changed = true;
			}
			if (!changed) break;
		}
	}

	for (const OrganicMovement &movement : movements) result.routes.push_back(movement.route);
	if (result.conflicts != 0) {
		result.error = fmt::format("{} conflicts left after {} rounds", result.conflicts, result.rounds);
		return result;
	}

	JunctionLayout layout;
	layout.size = {static_cast<int16_t>(width), static_cast<int16_t>(height)};
	for (const OrganicMovement &movement : movements) {
		AddRasterToLayout(layout, movement.raster);
		/* The prio window's companion track is the prio row: unused by any movement. */
		JunctionRaster companion;
		companion.pieces = movement.route.companion;
		AddRasterToLayout(layout, companion);
	}
	result.layout = std::move(layout);
	return result;
}

namespace {

/** A layout finished as the old generator finishes a candidate, and the shift from the negotiation box to its coordinates. */
struct OrganicFinished {
	JunctionLayout layout;
	int shift_x = 0; ///< Box x = layout x + shift_x.
	int shift_y = 0;
};

/**
 * The common end of the old generator (BuildJunctionCandidateWithGap's finish): crop the E and W rows to the content, normalise, place the signals, normalise.
 * @param layout The negotiated layout, in box coordinates.
 * @param spec The junction spec.
 * @param e_row The E main line's row; the prio rows beside the main lines are content, so the crop keeps them (DESIGN-O4 revision 5).
 * @return The finished layout and the shift of its coordinates from the box's.
 */
static OrganicFinished FinishOrganicLayout(JunctionLayout layout, const JunctionSpec &spec, int e_row)
{
	OrganicFinished finished;
	const int w_row = e_row + spec.ml_gap + 1;

	/* Crop the E and W rows to the content, as the old generator does, so no signal falls on the boundary. */
	int min_x = INT32_MAX;
	int max_x = INT32_MIN;
	auto plain_row = [&](const JunctionTrack &jt) { return (jt.offset.y == e_row || jt.offset.y == w_row) && jt.tracks == TrackBits{Track::X}; };
	for (const JunctionTrack &jt : layout.tracks) {
		if (plain_row(jt)) continue;
		min_x = std::min<int>(min_x, jt.offset.x);
		max_x = std::max<int>(max_x, jt.offset.x);
	}
	for (const JunctionBridge &bridge : layout.bridges) {
		for (const TileIndexDiffC &head : {bridge.start, bridge.end}) {
			min_x = std::min<int>(min_x, head.x);
			max_x = std::max<int>(max_x, head.x);
		}
	}
	if (max_x >= min_x) {
		/* A main-line row keeps 2 tiles beyond the content, which includes the prio rows beside the main lines. */
		auto row_lo = [&](int) { return min_x - 2; };
		auto row_hi = [&](int) { return max_x + 2; };
		std::erase_if(layout.tracks, [&](const JunctionTrack &jt) { return plain_row(jt) && (jt.offset.x < row_lo(jt.offset.y) || jt.offset.x > row_hi(jt.offset.y)); });
		/* The old generator's rows run two tiles past the content on its wider frame; the negotiation box ends at its ports, so a row whose window passes the box continues past it, as plain track, and no signal falls on the boundary. */
		for (const int y : {e_row, w_row}) {
			for (int x = row_lo(y); x <= row_hi(y); x++) {
				if (x >= 0 && x < layout.size.x) continue;
				const int edge = x < 0 ? 0 : layout.size.x - 1;
				const bool has_edge = std::ranges::any_of(layout.tracks, [&](const JunctionTrack &jt) { return jt.offset.x == edge && jt.offset.y == y; });
				if (has_edge) layout.tracks.push_back({{static_cast<int16_t>(x), static_cast<int16_t>(y)}, TrackBits{Track::X}});
			}
		}
	}

	/* The branch ports sit on the bottom row of the box: each continues one plain tile past it, so no signal falls on the boundary (the old generator's stem has no such end, so this is the one tile the signal pass needs). */
	{
		std::vector<int> columns;
		for (const JunctionTrack &jt : layout.tracks) {
			if (jt.offset.y == layout.size.y - 1 && jt.tracks == TrackBits{Track::Y}) columns.push_back(jt.offset.x);
		}
		for (const int x : columns) layout.tracks.push_back({{static_cast<int16_t>(x), static_cast<int16_t>(layout.size.y)}, TrackBits{Track::Y}});
	}

	/* NormaliseJunctionLayout shifts by the minimum of the track and bridge offsets; the shift is tracked. */
	auto normalise = [&]() {
		int lo_x = INT32_MAX;
		int lo_y = INT32_MAX;
		for (const JunctionTrack &jt : layout.tracks) {
			lo_x = std::min<int>(lo_x, jt.offset.x);
			lo_y = std::min<int>(lo_y, jt.offset.y);
		}
		for (const JunctionBridge &bridge : layout.bridges) {
			for (const TileIndexDiffC &head : {bridge.start, bridge.end}) {
				lo_x = std::min<int>(lo_x, head.x);
				lo_y = std::min<int>(lo_y, head.y);
			}
		}
		if (lo_x != INT32_MAX) {
			finished.shift_x += lo_x;
			finished.shift_y += lo_y;
		}
		NormaliseJunctionLayout(layout);
	};
	normalise();
	PlaceJunctionSignals(layout, spec);
	normalise();
	finished.layout = std::move(layout);
	return finished;
}

/** The prio timings of a layout, as PrioTimingsOf in the old generator obtains them. */
static std::vector<JunctionPrioTiming> OrganicPrioTimings(const JunctionLayout &layout, const JunctionSpec &spec)
{
	const JunctionCheckContext ctx = MakeJunctionCheckContext(layout, spec, FindJunctionPorts(layout), JunctionTMovements());
	std::vector<JunctionPath> paths;
	for (const auto &movement : JunctionTMovements()) {
		const auto path = std::ranges::find_if(ctx.paths, [&](const JunctionPath &p) { return p.from == movement.first && p.to == movement.second; });
		if (path == ctx.paths.end()) return {};
		paths.push_back(*path);
	}
	return JunctionPrioTimings(layout, spec, paths, spec.train, JunctionTerminalSpeed(spec.train), spec.tl);
}

}

/** One candidate: the negotiation projecting a map, finished, and confirmed by the checker. */
static JunctionOrganicResult BuildOrganicCase(const JunctionSpec &spec, int width, int height, int p, Money ml_turn_cost, const JunctionTMap &map, int &rounds)
{
	JunctionOrganicResult result;
	result.p = p;
	rounds = 0;
	constexpr int TOP = 1; // The E main line's row: the row above it holds its prio.
	const std::vector<JunctionPort> ports = MakeOrganicTPorts(spec, width, height, p, TOP);
	JunctionNegotiationPrices prices;
	prices.ml_turn_cost = ml_turn_cost;
	prices.map = &map;

	const JunctionNegotiation negotiation = NegotiateJunctionMovements(spec, ports, width, height, nullptr, 48, prices);
	rounds = negotiation.rounds;
	if (!negotiation.layout.has_value()) {
		result.error = negotiation.error;
		return result;
	}

	OrganicFinished finished = FinishOrganicLayout(*negotiation.layout, spec, TOP);
	result.violations = CheckJunctionRules(finished.layout, spec);
	/* The doubled main-line bridge is the D80 move, a later step: it doesn't make a layout unclean. */
	std::erase_if(result.violations, [](const RuleViolation &violation) { return violation.message == "ML bridge is not doubled"; });
	if (!result.violations.empty()) {
		result.error = fmt::format("the checker finds violations, the first rule {}: {}", static_cast<int>(result.violations.front().rule), result.violations.front().message);
		return result;
	}

	/* The judge: the prio timing gate as the old generator applies it. */
	if (spec.prio_strict && std::ranges::any_of(OrganicPrioTimings(finished.layout, spec), [](const JunctionPrioTiming &t) { return t.ml_delayed || t.timed_out; })) {
		result.error = "the prio delays the main line";
		return result;
	}
	RemoveRedundantJunctionSignals(finished.layout, spec);
	result.compactness = MeasureJunctionCompactness(finished.layout);
	result.cost = ScoreJunctionLayout(finished.layout, spec).second;
	result.layout = std::move(finished.layout);
	return result;
}

JunctionOrganicResult BuildOrganicT(const JunctionSpec &spec, int width, int height, int p, Money ml_turn_cost)
{
	const auto started = std::chrono::steady_clock::now();
	constexpr int MAX_GROWTHS = 4; // The map is made to fit: the box grows (width + 4, height + 2) up to this many times (D170a).

	/* The index of a window's range among the ranges its joiner's windows take over a list of maps. */
	auto chunk_index = [](const std::vector<JunctionTMap> &maps, const JunctionTMap::Window &window) {
		int least = window.end_lo;
		for (const JunctionTMap &map : maps) {
			for (const JunctionTMap::Window &other : map.windows) {
				if (other.joiner == window.joiner) least = std::min(least, other.end_lo);
			}
		}
		return (window.end_lo - least) / 6;
	};

	/* The first map is projected; when it does not fit, the same map (over pairs and chunk indices) on a larger box. */
	std::string cases;
	JunctionOrganicResult result;
	result.p = p;
	result.error = "no map to project";
	std::vector<std::pair<int, int>> over;
	std::vector<int> chunks;
	for (int step = 0; step <= MAX_GROWTHS; step++) {
		const int box_width = width + 4 * step;
		const int box_height = height + 1 + 2 * step; // One row more than the frame: the ports move one row in from the top, so the E line's prio row fits above it.
		const std::vector<JunctionTMap> maps = JunctionTMaps(spec, MakeOrganicTPorts(spec, box_width, box_height, p, 1), box_width, box_height);
		const JunctionTMap *map = nullptr;
		if (step == 0 && !maps.empty()) {
			map = &maps[0];
			over = map->over;
			for (const JunctionTMap::Window &window : map->windows) chunks.push_back(chunk_index(maps, window));
		} else {
			for (const JunctionTMap &candidate : maps) {
				bool same = candidate.over == over && candidate.windows.size() == chunks.size();
				for (size_t k = 0; same && k < chunks.size(); k++) same = chunk_index(maps, candidate.windows[k]) == chunks[k];
				if (same) {
					map = &candidate;
					break;
				}
			}
		}
		if (map == nullptr) {
			cases += fmt::format("case {} box {}x{}: no such map\n", step, box_width, box_height);
			result.error = "no map to project";
			break;
		}

		std::string over_text;
		for (const auto &[first, second] : map->over) over_text += fmt::format("{}{}>{}", over_text.empty() ? "" : " ", first, second);
		std::string chunk_text;
		for (const JunctionTMap::Window &window : map->windows) chunk_text += fmt::format("{}{}:{}-{}", chunk_text.empty() ? "" : " ", window.joiner, window.end_lo, window.end_hi);
		int rounds = 0;
		JunctionOrganicResult attempt = BuildOrganicCase(spec, box_width, box_height, p, ml_turn_cost, *map, rounds);
		const bool clean = attempt.layout.has_value();
		const int bridges = clean ? static_cast<int>(attempt.layout->bridges.size()) : 0;
		cases += fmt::format("{}case {} box {}x{} bound {} over [{}] windows [{}]: {}\n", clean ? "*" : "", step, box_width, box_height, map->bound, over_text, chunk_text, clean ? fmt::format("clean rounds {} occupied {} slivers {} bridges {}", rounds, attempt.compactness.occupied, attempt.compactness.slivers, bridges) : attempt.error);
		attempt.rounds = rounds;
		result = std::move(attempt);
		if (clean) break;
	}
	result.cases = std::move(cases);
	result.micros = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - started).count();
	return result;
}

JunctionOrganicResult GenerateJunctionOrganic(const JunctionSpec &spec, Money ml_turn_cost)
{
	const auto started = std::chrono::steady_clock::now();
	constexpr int WIDTH = 44;
	constexpr int HEIGHT = 16;
	std::optional<JunctionOrganicResult> best;
	std::pair<int, Money> best_score{};
	std::optional<JunctionOrganicResult> least_bad;
	for (int p = 14; p <= 28; p += 2) {
		JunctionOrganicResult attempt = BuildOrganicT(spec, WIDTH, HEIGHT, p, ml_turn_cost);
		if (!attempt.layout.has_value()) {
			if (!least_bad.has_value() || attempt.violations.size() < least_bad->violations.size()) least_bad = std::move(attempt);
			continue;
		}
		const std::pair<int, Money> score{attempt.compactness.occupied + attempt.compactness.slivers, attempt.cost};
		if (!best.has_value() || score < best_score) {
			best = std::move(attempt);
			best_score = score;
		}
	}
	JunctionOrganicResult result;
	if (best.has_value()) {
		result = std::move(*best);
	} else if (least_bad.has_value()) {
		result = std::move(*least_bad);
	} else {
		result.error = "no branch position built";
	}
	result.micros = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - started).count();
	return result;
}
