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
#include "track_func.h"

#include "3rdparty/fmt/format.h"

#include <algorithm>
#include <set>
#include <unordered_map>

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

JunctionNegotiation NegotiateJunctionMovements(const JunctionSpec &spec, const std::vector<JunctionPort> &ports, int width, int height, const JunctionTerrain *terrain, int max_rounds)
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
	std::unordered_map<uint64_t, Money> history;
	std::set<int> to_route; // Movements to route this round.

	for (int round = 1; round <= max_rounds; round++) {
		const Money present = unit << (round - 1);
		result.rounds = round;

		for (size_t m = 0; m < movements.size(); m++) {
			if (round > 1 && !to_route.contains(static_cast<int>(m))) continue;
			/* The others' current pieces; in round 1 only the movements already routed. */
			OrganicIndex others;
			for (const OrganicMovement &other : movements) {
				if (&other == &movements[m] || !other.routed) continue;
				for (const OrganicOccupant &occupant : other.occupants) others.emplace(TileKey(occupant.tile), &occupant);
			}

			const JunctionPort &from = *FindPort(ports, movements[m].from);
			const JunctionPort &to = *FindPort(ports, movements[m].to);

			JunctionLineRouteRequest request;
			request.line = ORGANIC_LINE;
			request.start = from.tile;
			request.start_trackdir = from.trackdir;
			request.goals = {{to.tile, to.trackdir}};
			request.min = {0, 0};
			request.max = {static_cast<int16_t>(width - 1), static_cast<int16_t>(height - 1)};
			request.terrain = terrain;
			request.plane = z;
			request.curve_spacing = spec.curve_limit ? DeriveCurveSpacing(spec.reference_speed, spec.curve_speed, spec.tl) : 1;
			request.s_bend_window = 2u * spec.tl;
			request.rail_piece = spec.prices.rail_piece;
			request.turn_cost = spec.prices.rail_piece / 2;
			request.bridge_span_ok = spec.bridge_span_ok;
			request.bridge_price = spec.prices.bridge;
			request.max_bridge_middle = 10;
			request.resource_cost = [&](const JunctionPiece &piece, int8_t piece_z, JunctionResourceLevel level) -> Money {
				const OrganicOccupant self{static_cast<int>(m), piece.tile, TrackdirToTrack(piece.trackdir), piece.trackdir, level, piece_z};
				Money cost = 0;
				/* A head on a port tile leaves the checker no approach track outside it: a rule, priced out because the callback can't forbid. */
				if (level == JunctionResourceLevel::Head && std::ranges::any_of(ports, [&](const JunctionPort &port) { return port.tile.x == piece.tile.x && port.tile.y == piece.tile.y; })) cost += Money{unit.base() * 1000};
				if (auto it = history.find(ResourceKey(self)); it != history.end()) cost += it->second;
				auto [first, last] = others.equal_range(TileKey(piece.tile));
				for (auto other = first; other != last; ++other) {
					if (Conflicts(self, *other->second, movements)) cost += present;
				}
				return cost;
			};

			const JunctionLineRouteResult routed = FindJunctionLineRoute(request);
			result.routes_run++;
			if (!routed.route.has_value()) {
				result.error = fmt::format("no route for movement {} (round {}): {}", m, round, routed.error);
				return result;
			}
			std::vector<JunctionRaster> rasters = RasterizeJunctionLine(ORGANIC_LINE, *routed.route);
			if (rasters.empty() || !rasters[0].error.empty()) {
				result.error = fmt::format("movement {} (round {}) does not rasterize", m, round);
				return result;
			}
			movements[m].route = *routed.route;
			movements[m].raster = std::move(rasters[0]);
			movements[m].occupants = MakeOccupants(static_cast<int>(m), movements[m].raster, z);
			movements[m].routed = true;
		}

		std::set<uint64_t> contested;
		to_route.clear();
		result.conflicts = CountConflicts(movements, contested, to_route);
		result.conflicts_per_round.push_back(result.conflicts);
		if (result.conflicts == 0) break;
		for (uint64_t key : contested) history[key] += unit;
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
