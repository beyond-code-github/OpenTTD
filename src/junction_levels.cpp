/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_levels.cpp The height solve of a junction placement (DESIGN-G1 §15.1-§15.3; D218, D219). */

#include "stdafx.h"

#include "junction_levels.h"
#include "direction_func.h"
#include "track_func.h"

#include <algorithm>
#include <climits>
#include <map>
#include <optional>
#include <set>

#include "safeguards.h"

namespace {

using Tile = std::pair<int, int>; ///< A snapshot tile.
using MapCorner = std::pair<int, int>;

constexpr Money INFINITE_COST = Money(INT64_MAX / 4);

/** Offsets of the corners W, S, E, N from the tile's coordinates. */
constexpr std::pair<int, int> CORNER_OFFSET[4] = {{1, 0}, {1, 1}, {0, 1}, {0, 0}};

/** A tile of an edge in travel order: consecutive pieces on one tile are one visit. */
struct Visit {
	Tile tile;
	TrackBits bits; ///< Every track the layout builds on the tile.
	bool fixed = false; ///< A node, port or bridge head tile: flat at its own level.
	bool axis = false; ///< Only a lone axis track: the tile may be a ramp.
	DiagDirection dir = DiagDirection::Invalid; ///< The direction of travel.
	uint32_t edge = 0;
	uint8_t copy = 0;
	bool break_before = false; ///< A bridge middle lies between this tile and the one before.
};

/** What a tile is built as: the corner heights it demands and its price. */
struct Choice {
	Money cost = INFINITE_COST; ///< Clearing plus foundation or terraform moves of this tile alone.
	std::array<int, 4> corners{}; ///< W, S, E, N heights the tile demands: its target, or its current ones when the ground fits.
	bool moves = false; ///< Some corner differs from the ground.
	bool foundation = false; ///< Leveled by a foundation on a slope.
};

struct Context {
	const JunctionTerrain &terrain;
	const JunctionFacts &facts;
};

/** The corners of a tile at the exit side of a travel direction (indices into W, S, E, N). */
std::array<int, 2> ExitCorners(DiagDirection dir)
{
	switch (dir) {
		case DiagDirection::SW: return {0, 1};
		case DiagDirection::NE: return {3, 2};
		case DiagDirection::SE: return {2, 1};
		default: return {3, 0};
	}
}

/**
 * The tile built as a shape at a level (D218 cost of a tile at a shape).
 * @param kind 0 flat at level, 1 a ramp up from level to level + 1, 2 a ramp down from level to level - 1 (entry level first).
 */
std::optional<Choice> Evaluate(const Context &c, const Visit &v, int kind, int level)
{
	const JunctionTerrainTile *tile = c.terrain.At(v.tile.first, v.tile.second);
	if (tile == nullptr || tile->cls != JunctionTileClass::Buildable) return std::nullopt;
	std::array<int, 4> target;
	target.fill(level);
	if (kind != 0) {
		for (int i : ExitCorners(v.dir)) target[i] = level + (kind == 1 ? 1 : -1);
	}
	const std::array<int, 4> current = JunctionTileCorners(*tile);
	Choice choice;
	if (current == target) {
		choice.cost = tile->clear_cost;
		choice.corners = current;
		return choice;
	}
	int steps = 0;
	for (int i = 0; i < 4; i++) steps += std::abs(target[i] - current[i]);
	choice.cost = tile->clear_cost + c.terrain.terraform_cost * steps;
	choice.corners = target;
	choice.moves = true;
	/* A leveled foundation: flat at the top of a gentle slope, but not for an axis piece along its own incline (D152b: that is a ramp). */
	if (kind == 0 && !tile->steep && tile->z_min < tile->z_max && tile->z_max == level && c.terrain.build_on_slopes && c.facts.Fits(v.tile.first, v.tile.second, v.bits)) {
		const bool along = v.axis && tile->incline != DiagDirection::Invalid && (tile->incline == v.dir || tile->incline == ReverseDiagDir(v.dir));
		const Money foundation = tile->clear_cost + c.terrain.foundation_cost;
		if (!along && foundation <= choice.cost) {
			choice.cost = foundation;
			choice.corners = current;
			choice.moves = false;
			choice.foundation = true;
		}
	}
	return choice;
}

} // namespace

bool SolveJunctionLevels(const JunctionTerrain &terrain, const JunctionFacts &facts, TileIndexDiffC offset, int ramp_levels, int train_length, const JunctionTrackPlacement &placement, const JunctionLayout &layout, const std::vector<JunctionTrackRoute> &routes, JunctionTerraform &plan, Money &cost, std::string *why)
{
	const Context ctx{terrain, facts};
	auto fail = [&](const std::string &reason) {
		if (why != nullptr) *why = reason;
		return false;
	};
	auto world = [&](TileIndexDiffC t) { return Tile{t.x + offset.x, t.y + offset.y}; };

	std::map<Tile, TrackBits> tile_bits;
	for (const JunctionTrack &jt : layout.tracks) tile_bits[world(jt.offset)] |= jt.tracks;
	std::set<Tile> heads;
	for (const JunctionBridge &b : layout.bridges) {
		heads.insert(world(b.start));
		heads.insert(world(b.end));
		/* No track is laid on a head: it is judged as the bridge's axis track (flat; its incline is kept out of this step). */
		const TrackBits axis_track{b.start.y == b.end.y ? Track::X : Track::Y};
		tile_bits[world(b.start)] |= axis_track;
		tile_bits[world(b.end)] |= axis_track;
	}
	std::set<Tile> fixed_tiles = heads;
	for (const TileIndexDiffC &t : placement.nodes) fixed_tiles.insert(world(t));
	for (const TileIndexDiffC &t : placement.ports) fixed_tiles.insert(world(t));

	/* The tiles of every route in travel order, one visit per tile. */
	std::vector<std::vector<Visit>> route_visits;
	for (const JunctionTrackRoute &route : routes) {
		std::vector<Visit> visits;
		bool broken = false;
		for (size_t j = 0; j < route.pieces.size(); j++) {
			const JunctionPiece &piece = route.pieces[j];
			const Tile tile = world(piece.tile);
			if (piece.on_bridge && !heads.contains(tile)) {
				broken = true;
				continue;
			}
			if (!visits.empty() && visits.back().tile == tile && !broken) continue;
			Visit v;
			v.tile = tile;
			const auto bits = tile_bits.find(tile);
			if (bits == tile_bits.end()) return fail(fmt::format("tile ({}, {}) of a route has no track", tile.first, tile.second));
			v.bits = bits->second;
			v.fixed = fixed_tiles.contains(tile) || j == 0 || j + 1 == route.pieces.size();
			v.axis = v.bits == TrackBits{Track::X} || v.bits == TrackBits{Track::Y};
			v.dir = TrackdirToExitdir(piece.trackdir);
			v.edge = route.tags[j].edge;
			v.copy = route.tags[j].copy;
			v.break_before = broken;
			broken = false;
			visits.push_back(v);
		}
		route_visits.push_back(std::move(visits));
	}

	/* The edges' tiles in travel order, cut at the fixed tiles; a bridge's middle cuts too (its heads are fixed). */
	std::vector<std::vector<Visit>> segments;
	std::set<std::vector<Tile>> seen;
	std::map<Tile, Visit> visit_of;
	for (const std::vector<Visit> &visits : route_visits) {
		std::vector<Visit> current;
		for (const Visit &v : visits) {
			if (v.fixed) {
				if (!current.empty() && !v.break_before) {
					current.push_back(v);
					std::vector<Tile> key;
					for (const Visit &u : current) key.push_back(u.tile);
					if (current.size() >= 2 && seen.insert(key).second) segments.push_back(current);
				}
				current = {v};
				visit_of.emplace(v.tile, v);
			} else if (!current.empty()) {
				current.push_back(v);
			}
		}
	}
	for (const std::vector<Visit> &segment : segments) {
		for (const Visit &v : segment) visit_of.emplace(v.tile, v);
	}

	/* The levels of the fixed tiles: the cheapest flat level of the tile alone (ties: the lower); heads first, then a crosser under a bridge middle may not exceed its bridge's lower head. */
	std::map<Tile, int> fixed_level;
	std::map<Tile, int> cap;
	auto cheapest = [&](const Visit &v, int limit) -> std::optional<int> {
		const JunctionTerrainTile *tile = terrain.At(v.tile.first, v.tile.second);
		if (tile == nullptr) return std::nullopt;
		std::optional<int> best;
		Money best_cost = INFINITE_COST;
		for (int level = tile->z_min - 2; level <= std::min<int>(tile->z_max + 2, limit); level++) {
			const std::optional<Choice> choice = Evaluate(ctx, v, 0, level);
			if (choice.has_value() && choice->cost < best_cost) {
				best_cost = choice->cost;
				best = level;
			}
		}
		return best;
	};
	auto place_fixed = [&](const Tile &tile) {
		const auto limit = cap.find(tile);
		const std::optional<int> level = cheapest(visit_of.at(tile), limit == cap.end() ? INT_MAX : limit->second);
		if (!level.has_value()) return false;
		fixed_level[tile] = *level;
		return true;
	};
	for (const Tile &tile : heads) {
		if (visit_of.contains(tile) && !place_fixed(tile)) return fail(fmt::format("no level for the bridge head ({}, {})", tile.first, tile.second));
	}
	for (const JunctionBridge &b : layout.bridges) {
		if (b.tunnel) continue;
		const Tile a = world(b.start);
		const Tile e = world(b.end);
		if (!fixed_level.contains(a) || !fixed_level.contains(e)) continue;
		const int head = std::min(fixed_level.at(a), fixed_level.at(e));
		const int dx = (e.first > a.first) - (e.first < a.first);
		const int dy = (e.second > a.second) - (e.second < a.second);
		for (Tile t{a.first + dx, a.second + dy}; t != e; t = {t.first + dx, t.second + dy}) {
			const auto found = cap.find(t);
			cap[t] = found == cap.end() ? head : std::min(found->second, head);
		}
	}
	for (const auto &[tile, visit] : visit_of) {
		if (visit.fixed && !fixed_level.contains(tile) && !place_fixed(tile)) return fail(fmt::format("no level for the node or port tile ({}, {})", tile.first, tile.second));
	}

	/* The ramp rule (D219; stricter than the window rule, simple): with k < tl levels allowed per tl tiles, up-ramp tiles are at least d apart, d the least integer with floor((tl - 1) / d) + 1 <= k; down-ramps are free. */
	const bool up_allowed = ramp_levels > 0;
	int spacing = 1;
	if (up_allowed && ramp_levels < train_length) {
		while ((train_length - 1) / spacing + 1 > ramp_levels) spacing++;
	}

	/* Each tile's choice, with the edge that made it. */
	struct Chosen {
		Choice choice;
		uint32_t edge;
		uint8_t copy;
	};
	std::map<Tile, Chosen> chosen;
	auto record = [&](const Visit &v, const Choice &choice) {
		const auto found = chosen.find(v.tile);
		if (found == chosen.end()) {
			chosen[v.tile] = {choice, v.edge, v.copy};
			return true;
		}
		return found->second.choice.corners == choice.corners;
	};
	for (const auto &[tile, level] : fixed_level) {
		const Visit &v = visit_of.at(tile);
		chosen[tile] = {*Evaluate(ctx, v, 0, level), v.edge, v.copy};
	}

	struct Cell {
		Money cost = INFINITE_COST;
		int prev_level = 0;
		int prev_since = 0;
		int kind = 0;
	};
	for (const std::vector<Visit> &segment : segments) {
		const size_t n = segment.size();
		const int start_level = fixed_level.at(segment.front().tile);
		const int end_level = fixed_level.at(segment.back().tile);
		int low = std::min(start_level, end_level);
		int high = std::max(start_level, end_level);
		for (const Visit &v : segment) {
			const JunctionTerrainTile *tile = terrain.At(v.tile.first, v.tile.second);
			if (tile == nullptr) continue;
			low = std::min<int>(low, tile->z_min - 2);
			high = std::max<int>(high, tile->z_max + 2);
		}
		const int levels = high - low + 1;
		auto at = [&](int level, int since) { return static_cast<size_t>(level - low) * spacing + (since - 1); };
		std::vector<std::vector<Cell>> layer(n, std::vector<Cell>(static_cast<size_t>(levels) * spacing));
		layer[0][at(start_level, spacing)].cost = 0;
		for (size_t i = 1; i + 1 < n; i++) {
			const Visit &v = segment[i];
			const auto limit = cap.find(v.tile);
			const int top = limit == cap.end() ? INT_MAX : limit->second;
			for (int level = low; level <= high; level++) {
				for (int since = 1; since <= spacing; since++) {
					const Cell &from = layer[i - 1][at(level, since)];
					if (from.cost >= INFINITE_COST) continue;
					auto relax = [&](int kind, int to_level, int to_since) {
						if (to_level < low || to_level > high || level > top || to_level > top) return;
						const std::optional<Choice> choice = Evaluate(ctx, v, kind, level);
						if (!choice.has_value()) return;
						Cell &to = layer[i][at(to_level, to_since)];
						if (from.cost + choice->cost < to.cost) to = {from.cost + choice->cost, level, since, kind};
					};
					relax(0, level, std::min(since + 1, spacing));
					if (v.axis) {
						if (up_allowed && since == spacing) relax(1, level + 1, 1);
						relax(2, level - 1, std::min(since + 1, spacing));
					}
				}
			}
		}
		/* The last interior tile must exit at the end tile's level. */
		Money best = INFINITE_COST;
		int best_since = 0;
		if (n == 2) {
			if (start_level != end_level) return fail(fmt::format("no levels between the adjacent fixed tiles ({}, {}) at {} and ({}, {}) at {}", segment[0].tile.first, segment[0].tile.second, start_level, segment[1].tile.first, segment[1].tile.second, end_level));
			continue;
		}
		for (int since = 1; since <= spacing; since++) {
			const Cell &cell = layer[n - 2][at(end_level, since)];
			if (cell.cost < best) {
				best = cell.cost;
				best_since = since;
			}
		}
		if (best >= INFINITE_COST) return fail(fmt::format("no levels for the edge {} copy {} from ({}, {}) at {} to ({}, {}) at {}", segment[1].edge, segment[1].copy, segment.front().tile.first, segment.front().tile.second, start_level, segment.back().tile.first, segment.back().tile.second, end_level));
		int level = end_level;
		int since = best_since;
		for (size_t i = n - 2; i >= 1; i--) {
			const Cell &cell = layer[i][at(level, since)];
			const std::optional<Choice> choice = Evaluate(ctx, segment[i], cell.kind, cell.prev_level);
			if (!record(segment[i], *choice)) {
				const Chosen &other = chosen.at(segment[i].tile);
				return fail(fmt::format("terraform conflict: tile ({}, {}) is built differently by edge {} copy {} and edge {} copy {}", segment[i].tile.first, segment[i].tile.second, other.edge, other.copy, segment[i].edge, segment[i].copy));
			}
			level = cell.prev_level;
			since = cell.prev_since;
		}
	}

	/* The plan: every corner a tile demands, merged over the tiles; two tiles that demand different heights of one corner conflict. */
	std::map<MapCorner, std::pair<int, Tile>> demanded;
	std::set<MapCorner> fixed_corners;
	JunctionTerraform result;
	for (const auto &[tile, item] : chosen) {
		const JunctionTerrainTile *ground = terrain.At(tile.first, tile.second);
		const std::array<int, 4> current = JunctionTileCorners(*ground);
		for (int i = 0; i < 4; i++) {
			const MapCorner corner{tile.first + CORNER_OFFSET[i].first, tile.second + CORNER_OFFSET[i].second};
			fixed_corners.insert(corner);
			const auto [found, inserted] = demanded.emplace(corner, std::make_pair(item.choice.corners[i], tile));
			if (!inserted && found->second.first != item.choice.corners[i]) {
				const Chosen &other = chosen.at(found->second.second);
				return fail(fmt::format("terraform conflict: corner ({}, {}) is wanted at {} by tile ({}, {}) (edge {} copy {}) and at {} by tile ({}, {}) (edge {} copy {})", corner.first, corner.second, found->second.first, found->second.second.first, found->second.second.second, other.edge, other.copy, item.choice.corners[i], tile.first, tile.second, item.edge, item.copy));
			}
			if (item.choice.corners[i] != current[i]) result.corners[corner] = item.choice.corners[i];
		}
	}
	std::string propagate_why;
	if (!PropagateJunctionTerraform(terrain, result, &propagate_why, &fixed_corners)) return fail(propagate_why);

	/* Total: the plan, plus foundations and clearing of the track tiles it does not touch (the plan clears the tiles it moves). */
	std::set<Tile> dirty;
	for (const auto &[corner, height] : result.corners) {
		for (const auto &[dx, dy] : CORNER_OFFSET) dirty.emplace(corner.first - dx, corner.second - dy);
	}
	Money total = result.cost;
	for (const auto &[tile, item] : chosen) {
		if (!dirty.contains(tile)) total += item.choice.cost;
	}
	plan = std::move(result);
	cost = total;
	return true;
}
