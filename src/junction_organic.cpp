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
#include <numeric>
#include <ranges>
#include <set>
#include <unordered_map>
#include <unordered_set>

#include "safeguards.h"

std::vector<JunctionPort> MakeOrganicTPorts(const JunctionSpec &spec, int width, int height, int p)
{
	auto tile = [](int x, int y) { return TileIndexDiffC{static_cast<int16_t>(x), static_cast<int16_t>(y)}; };
	const int row2 = spec.ml_gap + 1;
	return {
		{JunctionPortRole::EIn, tile(0, 0), Trackdir::X_SW},
		{JunctionPortRole::WOut, tile(0, row2), Trackdir::X_NE},
		{JunctionPortRole::EOut, tile(width - 1, 0), Trackdir::X_SW},
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

/** Whether two surface pieces of different movements form a split or a merge: different overlapping bits at the same level that IsSwitch accepts (the O4b1 switch rule). */
static bool FormsSwitch(const OrganicOccupant &a, const OrganicOccupant &b, const std::vector<OrganicMovement> &movements)
{
	if (a.movement == b.movement || a.level != JunctionResourceLevel::Surface || b.level != JunctionResourceLevel::Surface || a.z != b.z || a.bit == b.bit) return false;
	return TracksOverlap(TrackBits{a.bit} | b.bit) && IsSwitch(a, b, movements);
}

/** The occupants of a raster: heads and plain pieces at the surface, bridge middles on the span level. */
static std::vector<OrganicOccupant> MakeOccupants(int movement, const JunctionRaster &raster, int8_t z)
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
	for (const auto &[over, under] : prices.over) {
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

	/* The router request of movement m; others and near must outlive its use. */
	auto build_request = [&](size_t m, const OrganicIndex &others, const std::unordered_set<uint64_t> &near, Money conflict_price, bool use_history) {
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
		const bool main_line = (movements[m].from == JunctionPortRole::EIn && movements[m].to == JunctionPortRole::EOut) || (movements[m].from == JunctionPortRole::WIn && movements[m].to == JunctionPortRole::WOut);
		request.turn_cost = main_line && prices.ml_turn_cost != 0 ? prices.ml_turn_cost : spec.prices.rail_piece / 2;
		request.min_units = m < prices.min_units.size() ? prices.min_units[m] : 0;
		request.bridge_span_ok = spec.bridge_span_ok;
		request.bridge_price = spec.prices.bridge;
		request.max_bridge_middle = MAX_BRIDGE_MIDDLE;
		request.resource_cost = [&, m, conflict_price, use_history](const JunctionPiece &piece, int8_t piece_z, JunctionResourceLevel level) -> Money {
			const OrganicOccupant self{static_cast<int>(m), piece.tile, TrackdirToTrack(piece.trackdir), piece.trackdir, level, piece_z};
			Money cost = 0;
			/* A head on a port tile leaves the checker no approach track outside it: a rule, priced out because the callback can't forbid. */
			if (level == JunctionResourceLevel::Head && std::ranges::any_of(ports, [&](const JunctionPort &port) { return port.tile.x == piece.tile.x && port.tile.y == piece.tile.y; })) cost += Money{unit.base() * 1000};
			if (auto reserved = prices.reserved.find({piece.tile.x, piece.tile.y}); reserved != prices.reserved.end() && reserved->second != static_cast<int>(m)) cost += Money{unit.base() * 1000};
			if (use_history) {
				if (auto it = history.find(ResourceKey(self)); it != history.end()) cost += it->second;
			}
			/* Hug preference (D166 a): a surface piece with no other movement's piece on or beside its tile. */
			if (level == JunctionResourceLevel::Surface && !near.contains(TileKey(piece.tile))) cost += Money{unit.base() / 8};
			const auto penalty = prices.switch_penalties.find({piece.tile.x, piece.tile.y});
			bool switched = false;
			auto [first, last] = others.equal_range(TileKey(piece.tile));
			for (auto other = first; other != last; ++other) {
				if (Conflicts(self, *other->second, movements)) cost += conflict_price;
				if (const int r = role[m][other->second->movement]; r != 0 && other->second->level == JunctionResourceLevel::Surface) {
					/* The forced crossing is decided (D167): at grade, an under movement's span over the over movement, and an over movement's bore under the under movement, are prohibitive. */
					const bool at_grade = level == JunctionResourceLevel::Surface && piece_z == other->second->z && TrackOverlapsTracks(TrackBits{other->second->bit}, self.bit);
					const bool span_over = level == JunctionResourceLevel::BridgeSpan && r < 0;
					const bool bore_under = level == JunctionResourceLevel::TunnelBore && r > 0;
					if (at_grade || span_over || bore_under) cost += Money{unit.base() * 1000};
				}
				if (penalty != prices.switch_penalties.end() && !switched && FormsSwitch(self, *other->second, movements)) {
					cost += penalty->second;
					switched = true;
				}
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
	auto install = [&](size_t m, const JunctionLineRoute &route) {
		std::vector<JunctionRaster> rasters = RasterizeJunctionLine(ORGANIC_LINE, route);
		if (rasters.empty() || !rasters[0].error.empty()) return false;
		movements[m].route = route;
		movements[m].raster = std::move(rasters[0]);
		movements[m].occupants = MakeOccupants(static_cast<int>(m), movements[m].raster, z);
		movements[m].routed = true;
		return true;
	};

	for (int round = 1; round <= max_rounds; round++) {
		const Money present = unit << (round - 1);
		result.rounds = round;

		for (size_t m = 0; m < movements.size(); m++) {
			if (round > 1 && !to_route.contains(static_cast<int>(m))) continue;
			/* The others' current pieces; in round 1 only the movements already routed. */
			OrganicIndex others;
			std::unordered_set<uint64_t> near;
			gather(m, others, near);

			const JunctionLineRouteResult routed = FindJunctionLineRoute(build_request(m, others, near, present, true));
			result.routes_run++;
			if (!routed.route.has_value()) {
				result.error = fmt::format("no route for movement {} (round {}): {}", m, round, routed.error);
				return result;
			}
			if (!install(m, *routed.route)) {
				result.error = fmt::format("movement {} (round {}) does not rasterize", m, round);
				return result;
			}
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
				OrganicIndex others;
				std::unordered_set<uint64_t> near;
				gather(m, others, near);
				const JunctionLineRouteRequest request = build_request(m, others, near, cleanup_price, false);
				const Money old_cost = route_cost(request, movements[m]);
				const JunctionLineRouteResult routed = FindJunctionLineRoute(request);
				result.routes_run++;
				if (!routed.route.has_value() || !(routed.cost < old_cost)) continue;
				std::vector<JunctionRaster> rasters = RasterizeJunctionLine(ORGANIC_LINE, *routed.route);
				if (rasters.empty() || !rasters[0].error.empty()) continue;
				const std::vector<OrganicOccupant> occupants = MakeOccupants(static_cast<int>(m), rasters[0], z);
				const bool clash = std::ranges::any_of(occupants, [&](const OrganicOccupant &occupant) {
					auto [first, last] = others.equal_range(TileKey(occupant.tile));
					return std::any_of(first, last, [&](const auto &other) { return Conflicts(occupant, *other.second, movements); });
				});
				if (clash) continue;
				install(m, *routed.route);
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
	for (const OrganicMovement &movement : movements) AddRasterToLayout(layout, movement.raster);
	result.layout = std::move(layout);
	return result;
}

namespace {

/** The feedback state of one BuildOrganicT: the prices the next negotiation gets. */
struct OrganicFeedbackState {
	std::unordered_map<uint64_t, Money> history; ///< Carried across iterations.
	JunctionNegotiationPrices prices;
};

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
 * @return The finished layout and the shift of its coordinates from the box's.
 */
static OrganicFinished FinishOrganicLayout(JunctionLayout layout, const JunctionSpec &spec)
{
	OrganicFinished finished;
	const int w_row = spec.ml_gap + 1;

	/* Crop the E and W rows to the content, as the old generator does, so no signal falls on the boundary. */
	int min_x = INT32_MAX;
	int max_x = INT32_MIN;
	auto plain_row = [&](const JunctionTrack &jt) { return (jt.offset.y == 0 || jt.offset.y == w_row) && jt.tracks == TrackBits{Track::X}; };
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
		std::erase_if(layout.tracks, [&](const JunctionTrack &jt) { return plain_row(jt) && (jt.offset.x < min_x - 2 || jt.offset.x > max_x + 2); });
		/* The old generator's rows run two tiles past the content on its wider frame; the negotiation box ends at its ports, so a row whose window passes the box continues past it, as plain track, and no signal falls on the boundary. */
		for (const int y : {0, w_row}) {
			for (int x = min_x - 2; x <= max_x + 2; x++) {
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

/** The routes of a negotiation as piece lists, in box coordinates. */
static std::vector<std::vector<JunctionPiece>> OrganicPieces(const JunctionNegotiation &negotiation)
{
	std::vector<std::vector<JunctionPiece>> pieces;
	for (const JunctionLineRoute &route : negotiation.routes) {
		const std::vector<JunctionRaster> rasters = RasterizeJunctionLine(ORGANIC_LINE, route);
		pieces.push_back(rasters.empty() ? std::vector<JunctionPiece>{} : rasters[0].pieces);
	}
	return pieces;
}

static bool OnTile(const JunctionPiece &piece, TileIndexDiffC tile)
{
	return !piece.on_bridge && piece.tile.x == tile.x && piece.tile.y == tile.y;
}

/** A main-line merge: where the main-line movement first shares a tile with its joiner. */
struct OrganicMerge {
	size_t movement; ///< The main-line movement.
	size_t at; ///< Index of the merge piece in its pieces.
	int side_y; ///< The row offset of the prio row: opposite the joiner's approach; 0 when the joiner approaches along the axis (no prio).
};

/** The main-line merge of a main-line movement, when its route has one. */
static std::optional<OrganicMerge> MainLineMerge(const std::vector<std::vector<JunctionPiece>> &pieces, size_t movement)
{
	const auto &movements = JunctionTMovements();
	for (size_t joiner = 0; joiner < movements.size(); joiner++) {
		if (joiner == movement || movements[joiner].second != movements[movement].second) continue;
		for (size_t i = 0; i < pieces[movement].size(); i++) {
			if (pieces[movement][i].on_bridge) continue;
			const auto at = std::ranges::find_if(pieces[joiner], [&](const JunctionPiece &piece) { return OnTile(piece, pieces[movement][i].tile); });
			if (at == pieces[joiner].end()) continue;
			int side_y = 0;
			if (at != pieces[joiner].begin()) {
				const int offset = (at - 1)->tile.y - pieces[movement][i].tile.y;
				side_y = offset < 0 ? 1 : offset > 0 ? -1 : 0;
			}
			return OrganicMerge{movement, i, side_y};
		}
	}
	return std::nullopt;
}

/** The tiles of a main-line movement from its merge upstream, count tiles in all (the merge tile first). */
static std::vector<TileIndexDiffC> UpstreamTiles(const std::vector<JunctionPiece> &pieces, size_t at, int count)
{
	std::vector<TileIndexDiffC> tiles;
	for (size_t i = at + 1; i-- > 0 && static_cast<int>(tiles.size()) < count;) {
		if (pieces[i].on_bridge) continue;
		if (!tiles.empty() && tiles.back().x == pieces[i].tile.x && tiles.back().y == pieces[i].tile.y) continue;
		tiles.push_back(pieces[i].tile);
	}
	return tiles;
}

/** The tiles of a route from piece index 'from' downstream, count tiles in all (the first tile first). */
static std::vector<TileIndexDiffC> DownstreamTiles(const std::vector<JunctionPiece> &pieces, size_t from, int count)
{
	std::vector<TileIndexDiffC> tiles;
	for (size_t i = from; i < pieces.size() && static_cast<int>(tiles.size()) < count; i++) {
		if (pieces[i].on_bridge) continue;
		if (!tiles.empty() && tiles.back().x == pieces[i].tile.x && tiles.back().y == pieces[i].tile.y) continue;
		tiles.push_back(pieces[i].tile);
	}
	return tiles;
}

/**
 * Turn the violations of a finished layout into the next iteration's prices (DESIGN-O4 revision 3, search step 2).
 * @param spec The junction spec.
 * @param violations The violations, in the finished layout's coordinates.
 * @param finished The finished layout (for its shift).
 * @param negotiation The negotiation the layout came from.
 * @param iteration The iteration that found the violations, from 0.
 * @param state The prices to add to.
 * @param counts The feedback counts to add to.
 * @return Whether every violation had feedback.
 */
static bool ApplyOrganicFeedback(const JunctionSpec &spec, const std::vector<RuleViolation> &violations, const OrganicFinished &finished, const JunctionNegotiation &negotiation, int iteration, OrganicFeedbackState &state, JunctionFeedbackCounts &counts)
{
	const std::vector<std::vector<JunctionPiece>> pieces = OrganicPieces(negotiation);
	const auto &movements = JunctionTMovements();
	const int window = spec.prio_tiles + spec.sd + 1;
	const Money unit = spec.prices.rail_piece;
	std::set<size_t> penalised;
	std::set<std::pair<int, int>> bridge_penalised;
	std::set<std::pair<int, int>> merge_penalised;
	std::set<std::pair<int, int>> gap_penalised;
	std::set<std::pair<int, int>> mix_penalised;
	bool all = true;

	/* The main-line movements with a piece on a tile of the negotiation box (a bridge's pieces included: a head or a span tile is a main-line tile too). */
	auto main_line_on = [&](TileIndexDiffC tile) {
		std::vector<size_t> found;
		for (size_t m = 0; m < movements.size(); m++) {
			const bool main_line = (movements[m].first == JunctionPortRole::EIn && movements[m].second == JunctionPortRole::EOut) || (movements[m].first == JunctionPortRole::WIn && movements[m].second == JunctionPortRole::WOut);
			if (main_line && std::ranges::any_of(pieces[m], [&](const JunctionPiece &piece) { return piece.tile.x == tile.x && piece.tile.y == tile.y; })) found.push_back(m);
		}
		return found;
	};

	for (const RuleViolation &violation : violations) {
		const TileIndexDiffC tile = {static_cast<int16_t>(violation.tile.x + finished.shift_x), static_cast<int16_t>(violation.tile.y + finished.shift_y)};
		bool applied = false;
		if (violation.rule == JunctionRule::Prio && violation.message == "missing prio") {
			for (size_t m : main_line_on(tile)) {
				const std::optional<OrganicMerge> merge = MainLineMerge(pieces, m);
				if (!merge.has_value()) continue;
				applied = true;
				/* A merge on a curve piece or an axis approach has no prio geometry (GeometryOfPrio decides none): a switch penalty on the merge tile moves the merge. */
				if (merge->side_y == 0) {
					const TileIndexDiffC merge_tile = pieces[m][merge->at].tile;
					if (merge_penalised.insert({merge_tile.x, merge_tile.y}).second) {
						state.prices.switch_penalties[{merge_tile.x, merge_tile.y}] += Money{unit.base() << iteration};
						counts.coverage_penalties++;
					}
					continue;
				}
				bool added = false;
				for (const TileIndexDiffC &t : UpstreamTiles(pieces[m], merge->at, window)) {
					added |= state.prices.reserved.emplace(std::pair<int, int>{t.x, t.y + merge->side_y}, static_cast<int>(m)).second;
				}
				if (added) counts.prio_rows++;
			}
		} else if ((violation.rule == JunctionRule::Prio && violation.message == "split inside prio coverage") || (violation.rule == JunctionRule::PrioReach && violation.message == "ML tile in prio coverage does not hold the joiner")) {
			for (size_t m : main_line_on(tile)) {
				const std::optional<OrganicMerge> merge = MainLineMerge(pieces, m);
				if (!merge.has_value()) continue;
				applied = true;
				if (!penalised.insert(m).second) continue;
				for (const TileIndexDiffC &t : UpstreamTiles(pieces[m], merge->at, window)) state.prices.switch_penalties[{t.x, t.y}] += Money{unit.base() << iteration};
				counts.coverage_penalties++;
			}
		} else if (violation.rule == JunctionRule::Signals && violation.message.starts_with("split in a bridge's block")) {
			/* D113: a switch penalty on the violation tile, once per tile and iteration. */
			applied = true;
			if (bridge_penalised.insert({tile.x, tile.y}).second) {
				state.prices.switch_penalties[{tile.x, tile.y}] += Money{unit.base() << iteration};
				counts.bridge_block_penalties++;
			}
		} else if ((violation.rule == JunctionRule::Signals && violation.message == "signals too far apart") || (violation.rule == JunctionRule::Deadlock && violation.message == "a signal block holds both a split and a merge")) {
			/* The violation tile and the spec.sd tiles before and after it along each movement through it: a switch there leaves the stretch where the signal pass can't place signals (or a block holds a split and a merge). */
			const bool mix = violation.rule == JunctionRule::Deadlock;
			applied = true;
			if (!(mix ? mix_penalised : gap_penalised).insert({tile.x, tile.y}).second) continue;
			std::set<std::pair<int, int>> stretch;
			for (size_t m = 0; m < movements.size(); m++) {
				const auto here = std::ranges::find_if(pieces[m], [&](const JunctionPiece &piece) { return OnTile(piece, tile); });
				if (here == pieces[m].end()) continue;
				const size_t at = here - pieces[m].begin();
				for (const TileIndexDiffC &t : UpstreamTiles(pieces[m], at, spec.sd + 1)) stretch.insert({t.x, t.y});
				for (const TileIndexDiffC &t : DownstreamTiles(pieces[m], at, spec.sd + 1)) stretch.insert({t.x, t.y});
			}
			for (const auto &[x, y] : stretch) state.prices.switch_penalties[{x, y}] += Money{unit.base() << iteration};
			if (mix) {
				counts.block_mix_penalties++;
			} else {
				counts.signal_gap_penalties++;
			}
		} else if (violation.rule == JunctionRule::WaitingRoom) {
			for (size_t m = 0; m < movements.size(); m++) {
				const auto here = std::ranges::find_if(pieces[m], [&](const JunctionPiece &piece) { return OnTile(piece, tile); });
				if (here == pieces[m].end()) continue;
				/* A merge on the tile: a movement of the same exit port with a piece of a different bit there. */
				const bool merges = [&]() {
					for (size_t o = 0; o < movements.size(); o++) {
						if (o == m || movements[o].second != movements[m].second) continue;
						for (const JunctionPiece &piece : pieces[o]) {
							if (OnTile(piece, tile) && TrackdirToTrack(piece.trackdir) != TrackdirToTrack(here->trackdir)) return true;
						}
					}
					return false;
				}();
				if (!merges) continue;
				/* Its split from the sibling leaving the same entry port: the last tile both routes share before they part. */
				const auto sibling = std::ranges::find_if(std::views::iota(size_t{0}, movements.size()), [&](size_t o) { return o != m && movements[o].first == movements[m].first; });
				if (sibling == std::views::iota(size_t{0}, movements.size()).end()) continue;
				const std::vector<JunctionPiece> &own = pieces[m];
				const std::vector<JunctionPiece> &other = pieces[*sibling];
				size_t shared = 0;
				while (shared + 1 < own.size() && shared + 1 < other.size() && own[shared + 1].tile.x == other[shared + 1].tile.x && own[shared + 1].tile.y == other[shared + 1].tile.y) shared++;
				applied = true;
				/* The split tile and the next 2 x tl tiles of the sibling's route downstream of it can't carry a switch cheaply: the split can only move upstream. */
				const std::vector<TileIndexDiffC> tiles = DownstreamTiles(other, shared, 2 * static_cast<int>(spec.tl) + 1);
				for (const TileIndexDiffC &t : tiles) state.prices.switch_penalties[{t.x, t.y}] += Money{unit.base() << iteration};
				counts.split_moves++;
			}
		}
		if (!applied) all = false;
	}
	return all;
}

}

/** One crossing case's feedback loop: the negotiation with the forced pairs decided, finished and checked until rule-clean. */
static JunctionOrganicResult BuildOrganicCase(const JunctionSpec &spec, int width, int height, int p, int max_feedback, Money ml_turn_cost, const std::vector<std::pair<int, int>> &over, int &rounds)
{
	JunctionOrganicResult result;
	result.p = p;
	rounds = 0;
	const std::vector<JunctionPort> ports = MakeOrganicTPorts(spec, width, height, p);
	OrganicFeedbackState state;
	state.prices.history = &state.history;
	state.prices.ml_turn_cost = ml_turn_cost;
	state.prices.over = over;

	for (int iteration = 0; iteration < max_feedback; iteration++) {
		result.feedback.iterations = iteration + 1;
		const JunctionNegotiation negotiation = NegotiateJunctionMovements(spec, ports, width, height, nullptr, 48, state.prices);
		rounds = negotiation.rounds;
		if (!negotiation.layout.has_value()) {
			result.error = negotiation.error;
			return result;
		}
		OrganicFinished finished = FinishOrganicLayout(*negotiation.layout, spec);
		result.violations = CheckJunctionRules(finished.layout, spec);
		/* The doubled main-line bridge is the D80 move, a later step: no feedback exists and it doesn't make a layout unclean. */
		std::erase_if(result.violations, [](const RuleViolation &violation) { return violation.message == "ML bridge is not doubled"; });

		if (result.violations.empty()) {
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
		if (!ApplyOrganicFeedback(spec, result.violations, finished, negotiation, iteration, state, result.feedback)) {
			result.error = "a violation has no feedback";
			return result;
		}
	}
	result.error = "still unclean after the feedback iterations";
	return result;
}

JunctionOrganicResult BuildOrganicT(const JunctionSpec &spec, int width, int height, int p, int max_feedback, Money ml_turn_cost)
{
	const std::vector<std::pair<int, int>> pairs = JunctionForcedPairs(MakeOrganicTPorts(spec, width, height, p), width, height);
	const auto is_main_line = [](int movement) { return movement == 0 || movement == 2; };

	/* Every assignment of the forced pairs: bit i of a mask set means the lower movement of pair i goes over. Main-line movements over come first (D156), then by mask. */
	std::vector<uint> masks(size_t{1} << pairs.size());
	std::iota(masks.begin(), masks.end(), 0u);
	auto main_line_over = [&](uint mask) {
		int count = 0;
		for (size_t i = 0; i < pairs.size(); i++) count += is_main_line((mask >> i & 1) != 0 ? pairs[i].first : pairs[i].second) ? 1 : 0;
		return count;
	};
	std::ranges::stable_sort(masks, [&](uint a, uint b) { return main_line_over(a) > main_line_over(b); });

	std::string cases;
	std::optional<JunctionOrganicResult> best;
	std::pair<int, Money> best_score{};
	size_t best_case = 0;
	std::optional<JunctionOrganicResult> least_bad;
	for (size_t c = 0; c < masks.size(); c++) {
		std::vector<std::pair<int, int>> over;
		std::string label;
		for (size_t i = 0; i < pairs.size(); i++) {
			const bool first_over = (masks[c] >> i & 1) != 0;
			over.emplace_back(first_over ? pairs[i].first : pairs[i].second, first_over ? pairs[i].second : pairs[i].first);
			label += fmt::format("{}{}>{}", i == 0 ? "" : " ", over.back().first, over.back().second);
		}
		int rounds = 0;
		JunctionOrganicResult attempt = BuildOrganicCase(spec, width, height, p, max_feedback, ml_turn_cost, over, rounds);
		const int bridges = attempt.layout.has_value() ? static_cast<int>(attempt.layout->bridges.size()) : 0;
		cases += fmt::format("case {} [{}]: clean {} iterations {} rounds {} occupied {} slivers {} bridges {}{}\n", c, label, attempt.layout.has_value() ? 1 : 0, attempt.feedback.iterations, rounds, attempt.compactness.occupied, attempt.compactness.slivers, bridges, attempt.layout.has_value() ? "" : fmt::format(" ({})", attempt.error));
		if (!attempt.layout.has_value()) {
			if (!least_bad.has_value() || attempt.violations.size() < least_bad->violations.size()) least_bad = std::move(attempt);
			continue;
		}
		const std::pair<int, Money> score{attempt.compactness.occupied + attempt.compactness.slivers, attempt.cost};
		if (!best.has_value() || score < best_score) {
			best = std::move(attempt);
			best_score = score;
			best_case = c;
		}
	}

	JunctionOrganicResult result;
	if (best.has_value()) {
		result = std::move(*best);
		const std::string mark = fmt::format("case {} [", best_case);
		cases.insert(cases.find(mark), "*");
	} else if (least_bad.has_value()) {
		result = std::move(*least_bad);
	} else {
		result.p = p;
		result.error = "no crossing case built";
	}
	result.cases = std::move(cases);
	return result;
}

JunctionOrganicResult GenerateJunctionOrganic(const JunctionSpec &spec, Money ml_turn_cost)
{
	constexpr int WIDTH = 44;
	constexpr int HEIGHT = 16;
	std::optional<JunctionOrganicResult> best;
	std::pair<int, Money> best_score{};
	std::optional<JunctionOrganicResult> least_bad;
	for (int p = 14; p <= 28; p += 2) {
		JunctionOrganicResult attempt = BuildOrganicT(spec, WIDTH, HEIGHT, p, 8, ml_turn_cost);
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
	if (best.has_value()) return std::move(*best);
	if (least_bad.has_value()) return std::move(*least_bad);
	JunctionOrganicResult none;
	none.error = "no branch position built";
	return none;
}
