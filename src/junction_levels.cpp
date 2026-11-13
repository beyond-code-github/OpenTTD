/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_levels.cpp The height solve of a junction placement (DESIGN-G1 §15.1-§15.3; D218, D219). */

#include "stdafx.h"

#include "junction_levels.h"
#include "junction_positions.h"
#include "bridge.h"
#include "direction_func.h"
#include "landscape.h"
#include "rail.h"
#include "track_func.h"

#include <algorithm>
#include <climits>
#include <map>
#include <optional>
#include <set>
#include <tuple>

#include "safeguards.h"

namespace {

using SnapshotTile = std::pair<int, int>; ///< A snapshot tile.
using MapCorner = std::pair<int, int>;

constexpr Money INFINITE_COST = Money(INT64_MAX / 4);

/** Offsets of the corners W, S, E, N from the tile's coordinates. */
constexpr std::pair<int, int> CORNER_OFFSET[4] = {{1, 0}, {1, 1}, {0, 1}, {0, 0}};

/** A tile of an edge in travel order: consecutive pieces on one tile are one visit. */
struct Visit {
	SnapshotTile tile;
	TrackBits bits; ///< Every track the layout builds on the tile.
	bool fixed = false; ///< A node, port, bridge head or route end tile: flat at its own level.
	bool head = false; ///< A bridge head: dir is then the direction into the span, and the tile's level is the height of its outer edge.
	bool tunnel = false; ///< A tunnel portal: dir is then the direction into the tunnel, and the tile's level is the tunnel's level L, the height of its outer edge.
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
	int level = 0; ///< The tile's level: its entry level in the direction of travel.
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

/** What the game's bridge command makes of a head tile (CheckBridgeSlope, tunnelbridge_cmd.cpp). */
struct BridgeHeadGround {
	int z; ///< The head's z (the game's z_start): the tile's lowest corner plus the foundation's rise.
	bool inclined; ///< The slope on top of the foundation is the valid inclined one (the outer edge one level above the span side).
	bool foundation; ///< The game charges a foundation.
};

/**
 * The game's bridge head test.
 * @param slope The tile's slope.
 * @param z_min The tile's lowest corner.
 * @param inward The direction from the outer edge into the span.
 * @return The head, or nullopt when the game refuses the slope.
 */
std::optional<BridgeHeadGround> GetBridgeHeadGround(Slope slope, int z_min, DiagDirection inward)
{
	const Axis axis = inward == DiagDirection::SW || inward == DiagDirection::NE ? Axis::X : Axis::Y;
	const Slope valid = inward == DiagDirection::SW ? SLOPE_NE : inward == DiagDirection::SE ? SLOPE_NW : inward == DiagDirection::NE ? SLOPE_SW : SLOPE_SE;
	const Foundation f = GetBridgeFoundation(slope, axis);
	const int z = z_min + static_cast<int>(ApplyFoundationToSlope(f, slope));
	if (slope != SLOPE_FLAT && slope != valid) return std::nullopt;
	return BridgeHeadGround{z, slope != SLOPE_FLAT, f != Foundation::None};
}

/**
 * A bridge head tile built as the game takes it: kind 0 flat at the level (the span side and the outer edge both at level), kind 2 on the valid inclined slope
 * (the outer edge at level, the span side at level - 1), or on a foundation the game builds there for that. The head's z (the game's z_start) is level - (kind == 2 ? 1 : 0).
 */
std::optional<Choice> EvaluateHead(const Context &c, const Visit &v, int kind, int level)
{
	if (kind == 1) return std::nullopt;
	const JunctionTerrainTile *tile = c.terrain.At(v.tile.first, v.tile.second);
	if (tile == nullptr || tile->cls != JunctionTileClass::Buildable) return std::nullopt;
	std::array<int, 4> target;
	target.fill(level);
	if (kind == 2) {
		for (int i : ExitCorners(v.dir)) target[i] = level - 1;
	}
	const std::array<int, 4> current = JunctionTileCorners(*tile);
	Choice choice;
	choice.level = level;
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
	if (c.terrain.build_on_slopes) {
		const std::optional<BridgeHeadGround> ground = GetBridgeHeadGround(tile->slope, tile->z_min, v.dir);
		if (ground.has_value() && ground->foundation && ground->inclined == (kind == 2) && ground->z == level - (kind == 2 ? 1 : 0)) {
			const Money foundation = tile->clear_cost + c.terrain.foundation_cost;
			if (foundation <= choice.cost) {
				choice.cost = foundation;
				choice.corners = current;
				choice.moves = false;
				choice.foundation = true;
			}
		}
	}
	return choice;
}

/**
 * A tunnel portal built as the game takes it (CmdBuildTunnel): the raw inclined slope rising into the tunnel, its outer edge at the tunnel's level and its tunnel-side edge one above;
 * the game takes no foundation there, so this is the portal's only option.
 */
std::optional<Choice> EvaluateTunnelPortal(const Context &c, const Visit &v, int level)
{
	const JunctionTerrainTile *tile = c.terrain.At(v.tile.first, v.tile.second);
	if (tile == nullptr || tile->cls != JunctionTileClass::Buildable) return std::nullopt;
	std::array<int, 4> target;
	target.fill(level);
	for (int i : ExitCorners(v.dir)) target[i] = level + 1;
	const std::array<int, 4> current = JunctionTileCorners(*tile);
	Choice choice;
	choice.level = level;
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
	return choice;
}

/**
 * The tile built as a shape at a level (D218 cost of a tile at a shape).
 * @param kind 0 flat at level, 1 a ramp up from level to level + 1, 2 a ramp down from level to level - 1 (entry level first).
 */
std::optional<Choice> Evaluate(const Context &c, const Visit &v, int kind, int level)
{
	if (v.tunnel) return kind == 0 ? EvaluateTunnelPortal(c, v, level) : std::nullopt;
	if (v.head) return EvaluateHead(c, v, kind, level);
	const JunctionTerrainTile *tile = c.terrain.At(v.tile.first, v.tile.second);
	if (tile == nullptr || tile->cls != JunctionTileClass::Buildable) return std::nullopt;
	std::array<int, 4> target;
	target.fill(level);
	if (kind != 0) {
		for (int i : ExitCorners(v.dir)) target[i] = level + (kind == 1 ? 1 : -1);
	}
	const std::array<int, 4> current = JunctionTileCorners(*tile);
	Choice choice;
	choice.level = level;
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
	/* A leveled foundation: flat at the top of the slope, where the game builds exactly that for these tracks (GetRailFoundation, rail_cmd.cpp; a lone axis piece on one raised corner or along an incline is built inclined, not flat). */
	if (kind == 0 && tile->z_max == level && c.terrain.build_on_slopes && GetRailFoundation(tile->slope, v.bits) == Foundation::Leveled && c.facts.Fits(v.tile.first, v.tile.second, v.bits)) {
		const Money foundation = tile->clear_cost + c.terrain.foundation_cost;
		if (foundation <= choice.cost) {
			choice.cost = foundation;
			choice.corners = current;
			choice.moves = false;
			choice.foundation = true;
		}
	}
	return choice;
}

/** The heights the corners of the snapshot may end at (the cone of the ground a terraform must keep). */
struct CornerCone {
	int x0 = 0; ///< Snapshot x of the first corner.
	int y0 = 0; ///< Snapshot y of the first corner.
	int stride = 0; ///< Corners per row.
	std::vector<int> lower; ///< Per corner the lowest height it can end at.
	std::vector<int> upper; ///< Per corner the highest height it can end at.

	/** The index of a corner. */
	size_t Index(int cx, int cy) const { return static_cast<size_t>(cy - this->y0) * this->stride + (cx - this->x0); }
};

/**
 * The cone of a snapshot. A corner that no terraform can move (a tile around it is not Buildable, or lies outside the snapshot) ends at its ground height g.
 * A finished terraform leaves every moved corner within 1 of each neighbour, and a corner it does not move is within 1 of them by the ground, so every corner ends within d of g
 * when d is its Manhattan distance to the unmovable corner: two passes of an L1 distance transform.
 * @param terrain The snapshot.
 * @return The bounds of every corner.
 */
CornerCone MakeCornerCone(const JunctionTerrain &terrain)
{
	constexpr int UNBOUNDED = 1000000;
	CornerCone cone;
	cone.x0 = terrain.origin.x;
	cone.y0 = terrain.origin.y;
	cone.stride = terrain.width + 1;
	const int rows = terrain.height + 1;
	cone.lower.assign(static_cast<size_t>(cone.stride) * rows, -UNBOUNDED);
	cone.upper.assign(static_cast<size_t>(cone.stride) * rows, UNBOUNDED);
	for (int y = 0; y < rows; y++) {
		for (int x = 0; x < cone.stride; x++) {
			bool movable = true;
			std::optional<int> ground;
			for (int i = 0; i < 4; i++) {
				const JunctionTerrainTile *tile = terrain.At(cone.x0 + x - CORNER_OFFSET[i].first, cone.y0 + y - CORNER_OFFSET[i].second);
				if (tile == nullptr) {
					movable = false;
					continue;
				}
				movable = movable && tile->cls == JunctionTileClass::Buildable;
				if (!ground.has_value()) ground = JunctionTileCorners(*tile)[i];
			}
			if (movable || !ground.has_value()) continue;
			cone.lower[cone.Index(cone.x0 + x, cone.y0 + y)] = *ground;
			cone.upper[cone.Index(cone.x0 + x, cone.y0 + y)] = *ground;
		}
	}
	for (int y = 0; y < rows; y++) {
		for (int x = 0; x < cone.stride; x++) {
			const size_t k = static_cast<size_t>(y) * cone.stride + x;
			if (x > 0) {
				cone.lower[k] = std::max(cone.lower[k], cone.lower[k - 1] - 1);
				cone.upper[k] = std::min(cone.upper[k], cone.upper[k - 1] + 1);
			}
			if (y > 0) {
				cone.lower[k] = std::max(cone.lower[k], cone.lower[k - cone.stride] - 1);
				cone.upper[k] = std::min(cone.upper[k], cone.upper[k - cone.stride] + 1);
			}
		}
	}
	for (int y = rows - 1; y >= 0; y--) {
		for (int x = cone.stride - 1; x >= 0; x--) {
			const size_t k = static_cast<size_t>(y) * cone.stride + x;
			if (x + 1 < cone.stride) {
				cone.lower[k] = std::max(cone.lower[k], cone.lower[k + 1] - 1);
				cone.upper[k] = std::min(cone.upper[k], cone.upper[k + 1] + 1);
			}
			if (y + 1 < rows) {
				cone.lower[k] = std::max(cone.lower[k], cone.lower[k + cone.stride] - 1);
				cone.upper[k] = std::min(cone.upper[k], cone.upper[k + cone.stride] + 1);
			}
		}
	}
	return cone;
}

/** The choice of a tile and the edge that routes through it first (for the reasons). */
struct Chosen {
	Choice choice;
	uint32_t edge;
	uint8_t copy;
};

/** One track tile of the joint solve. */
struct LevelTile {
	Visit visit; ///< The first visit of the tile.
	bool rampable = true; ///< Only a lone axis track, every route through it the same way, not a node, port, head or route end.
	bool has_next = false;
	SnapshotTile next; ///< The tile after this one on its route (when has_next).
};

/**
 * The joint level solve (RECON-joint-levels; DESIGN-G1 §15.3): one level per track tile, in a JunctionPositions.
 * The level of a tile is its entry level in the direction of travel. A tile that is not rampable continues at its own level; a rampable one at most one level either way.
 * The ramp rule (D219) is L(t + tl) - L(t) <= k per route, a crosser under a bridge middle stays at or below both heads, a corner shared by two track tiles is one height
 * (a rampable tile's exit-side corners are the next tile's level) and track corners within Manhattan distance d differ by at most d.
 * A bridge head's level is the height of its outer edge: flat, or on the game's inclined slope (the head a level above the other), or on the game's bridge foundation; the two heads share one z,
 * and the corners of the span's ground stay within [z - max_bridge_height + 1, z] in the clamp.
 * A tunnel's two portals are tied to one level L and each has one option, the game's raw slope rising into the tunnel (outer edge at L, tunnel-side edge at L + 1, no foundation);
 * the corners of the ground over the tunnel stay at L + 1 or more in the clamp (a lower tile would end the tunnel early), so a crosser's track tile there is raised with it.
 * For the ramp rule a portal is no climb (the game puts a train on a portal at the tile's z), and each middle tile counts as a tile at L.
 * The start is each tile at its cheapest flat level; a coordinate descent over the tiles tied by equality then lowers the cost, which is the exact cost of the finished terraform:
 * the heights the tracks fix at their corners, and every other corner the ground clamped to the cones of those (clamp(g, L, U)), with the foundation tiles' corners free.
 * @param c The terrain and facts.
 * @param route_visits The tiles of every route in travel order.
 * @param heads The bridge head tiles.
 * @param fixed_tiles The head, node and port tiles.
 * @param ties (prio tile, main-line tile beside it) pairs: one level; the prio tiles are in route_visits as one-tile routes.
 * @param layout The render.
 * @param offset The snapshot tile of the placement's (0, 0).
 * @param ramp_levels JunctionRampLevels(spec).
 * @param train_length Tiles of the train.
 * @param[out] chosen The choice of every track tile.
 * @param[out] terraform The corners the clamp moves, and the cost of moving them and clearing the tiles they touch.
 * @param[out] total The cost of the whole plan: the terraform, plus the tiles it does not touch.
 * @return The empty string on success, else the reason.
 */
std::string SolveTileLevels(const Context &c, const std::vector<std::vector<Visit>> &route_visits, const std::set<SnapshotTile> &heads, const std::set<SnapshotTile> &fixed_tiles, const std::vector<std::pair<SnapshotTile, SnapshotTile>> &ties, const JunctionLayout &layout, TileIndexDiffC offset, int ramp_levels, int train_length, std::map<SnapshotTile, Chosen> &chosen, JunctionTerraform &terraform, Money &total, std::pair<long, long> *incremental_check)
{
	std::map<SnapshotTile, LevelTile> tiles;
	std::map<SnapshotTile, std::set<DiagDirection>> directions;
	for (const std::vector<Visit> &visits : route_visits) {
		for (const Visit &v : visits) {
			const auto [slot, fresh] = tiles.try_emplace(v.tile);
			if (fresh) slot->second.visit = v;
			directions[v.tile].insert(v.dir);
			slot->second.rampable = slot->second.rampable && v.axis && !v.fixed;
		}
	}
	if (tiles.empty()) return "";

	int lowest = INT_MAX;
	int highest = INT_MIN;
	for (auto &[tile, lt] : tiles) {
		const JunctionTerrainTile *ground = c.terrain.At(tile.first, tile.second);
		if (ground == nullptr || ground->cls != JunctionTileClass::Buildable) return fmt::format("tile ({}, {}) cannot be built on", tile.first, tile.second);
		lt.rampable = lt.rampable && directions[tile].size() == 1;
		lowest = std::min<int>(lowest, ground->z_min);
		highest = std::max<int>(highest, ground->z_max);
	}
	/* Levels run from 3 below the lowest ground to 3 above the highest; the variable v is the level v + base. */
	const int base = lowest - 3;
	std::vector<SnapshotTile> order_tiles;
	std::map<SnapshotTile, size_t> var;
	for (const auto &[tile, lt] : tiles) {
		var[tile] = order_tiles.size();
		order_tiles.push_back(tile);
	}
	const size_t n = order_tiles.size();
	const int top = highest + 3 - base;
	JunctionPositions pos(n, 0, top);

	/* The cone filter: a tile's option (shape and level) that is not a foundation and moves a corner out of the cone can be in no successful plan,
	 * so the tile's levels shrink to those with a surviving option; a tile with none cannot be built. */
	const CornerCone cone = MakeCornerCone(c.terrain);

	/* The bridges: the two heads share one z, and every corner of the span's ground stays within [z - max_bridge_height + 1, z] (CmdBuildBridge). */
	struct Span {
		size_t a, e; ///< The head variables.
		bool tunnel; ///< The portals of a tunnel: the ground over it only has a lower bound.
		SnapshotTile start, end; ///< The head tiles.
		std::vector<std::pair<SnapshotTile, size_t>> corners; ///< The middle tile of each corner and its index in the cone.
	};
	std::vector<Span> spans;
	std::vector<size_t> partner(n, SIZE_MAX); ///< Per head variable, the other head's.
	for (const JunctionBridge &b : layout.bridges) {
		const SnapshotTile a{b.start.x + offset.x, b.start.y + offset.y};
		const SnapshotTile e{b.end.x + offset.x, b.end.y + offset.y};
		if (!var.contains(a) || !var.contains(e) || !(b.tunnel ? tiles[a].visit.tunnel && tiles[e].visit.tunnel : tiles[a].visit.head && tiles[e].visit.head)) continue;
		Span span{var[a], var[e], b.tunnel, a, e, {}};
		if (!b.tunnel) {
			partner[var[a]] = var[e];
			partner[var[e]] = var[a];
		}
		const int dx = (e.first > a.first) - (e.first < a.first);
		const int dy = (e.second > a.second) - (e.second < a.second);
		std::set<size_t> seen_corners;
		for (SnapshotTile t{a.first + dx, a.second + dy}; t != e; t = {t.first + dx, t.second + dy}) {
			if (c.terrain.At(t.first, t.second) == nullptr) continue;
			for (const auto &[ox, oy] : CORNER_OFFSET) {
				const size_t k = cone.Index(t.first + ox, t.second + oy);
				if (seen_corners.insert(k).second) span.corners.emplace_back(t, k);
			}
		}
		spans.push_back(std::move(span));
	}
	std::vector<std::pair<int, int>> reach(n, {0, top}); ///< Per tile the lowest and highest variable with a surviving option.
	std::vector<bool> cut(n, false); ///< Whether the tile's levels were shrunk.
	for (size_t i = 0; i < n; i++) {
		const SnapshotTile &tile = order_tiles[i];
		const LevelTile &lt = tiles[tile];
		bool inside = true; ///< Every level of the tile keeps the corners in the cone.
		for (int ci = 0; ci < 4; ci++) {
			const size_t k = cone.Index(tile.first + CORNER_OFFSET[ci].first, tile.second + CORNER_OFFSET[ci].second);
			inside = inside && cone.lower[k] <= base && cone.upper[k] >= base + top;
		}
		if (inside) continue;
		const std::array<int, 4> ground = JunctionTileCorners(*c.terrain.At(tile.first, tile.second));
		/* A prio tile that is a lone axis piece ramps as the main-line tile beside it. */
		Visit probe = lt.visit;
		if (probe.dir == DiagDirection::Invalid) {
			for (const auto &[prio, beside] : ties) {
				if (prio == tile && tiles.contains(beside)) probe.dir = tiles.at(beside).visit.dir;
			}
		}
		const int kinds = (lt.rampable && probe.dir != DiagDirection::Invalid) || probe.head ? 3 : 1;
		int lowest_ok = INT_MAX;
		int highest_ok = INT_MIN;
		for (int l = 0; l <= top; l++) {
			for (int kind = 0; kind < kinds; kind++) {
				const std::optional<Choice> choice = Evaluate(c, probe, kind, l + base);
				if (!choice.has_value()) continue;
				bool survives = true;
				for (int ci = 0; ci < 4 && survives && !choice->foundation; ci++) {
					if (choice->corners[ci] == ground[ci]) continue;
					const size_t k = cone.Index(tile.first + CORNER_OFFSET[ci].first, tile.second + CORNER_OFFSET[ci].second);
					survives = choice->corners[ci] >= cone.lower[k] && choice->corners[ci] <= cone.upper[k];
				}
				if (!survives) continue;
				lowest_ok = std::min(lowest_ok, l);
				highest_ok = std::max(highest_ok, l);
				break;
			}
		}
		if (lowest_ok > highest_ok) return fmt::format("tile ({}, {}) cannot reach the ground the terraform must keep", tile.first, tile.second);
		reach[i] = {lowest_ok, highest_ok};
		cut[i] = true;
	}

	/* A reason is kept as numbers and formatted only when a failure reports it. */
	struct Why {
		int kind; ///< The sentence: the order of why_text's cases.
		int a0, a1, b0, b1; ///< Two tiles or corners.
		int k; ///< The distance or the levels.
	};
	std::vector<Why> reasons;
	auto reason = [&](const Why &why) {
		reasons.push_back(why);
		return static_cast<uint32_t>(reasons.size() - 1);
	};
	auto name = [](const SnapshotTile &t) { return fmt::format("({}, {})", t.first, t.second); };
	auto why_text = [&](const Why &w) -> std::string {
		const SnapshotTile a{w.a0, w.a1};
		const SnapshotTile b{w.b0, w.b1};
		switch (w.kind) {
			case 0: return "ramp tile " + name(a) + " to " + name(b) + " drops at most one level";
			case 1: return "ramp tile " + name(a) + " to " + name(b) + " climbs at most one level";
			case 2: return "flat tile " + name(a) + " continues at its level into " + name(b);
			case 3: return fmt::format("ramp rule: {} to {} climbs at most {}", name(a), name(b), w.k);
			case 4: return "prio tile " + name(a) + " is at the level of " + name(b);
			case 5: return "crosser " + name(b) + " under the head " + name(a);
			case 6: return fmt::format("the corner ({}, {}) shared by track tiles is one height", w.a0, w.a1);
			case 7: return fmt::format("track corners ({}, {}) and ({}, {}) differ by at most {}", w.a0, w.a1, w.b0, w.b1, w.k);
			case 9: return "the heads " + name(a) + " and " + name(b) + " of a bridge or tunnel are at the same z";
			default: return "start level of " + name(a);
		}
	};
	std::set<std::tuple<size_t, size_t, int>> seen;
	std::vector<std::vector<std::pair<size_t, int>>> lower(n); ///< Per variable a, the (b, k) of its constraints x_a - x_b >= k.
	std::vector<std::vector<std::pair<size_t, int>>> upper(n); ///< Per variable b, the (a, k) of its constraints x_a - x_b >= k.
	std::set<std::pair<size_t, size_t>> equal_pairs;
	bool feasible = true;
	auto diff = [&](size_t a, size_t b, int k, const Why &why) { ///< x_a - x_b >= k.
		if (!seen.emplace(a, b, k).second) return;
		lower[a].emplace_back(b, k);
		upper[b].emplace_back(a, k);
		feasible = pos.AddDifference(a, b, k, reason(why)) && feasible;
	};
	auto equal = [&](size_t a, size_t b, const Why &why) {
		diff(a, b, 0, why);
		diff(b, a, 0, why);
		equal_pairs.emplace(std::min(a, b), std::max(a, b));
	};

	std::vector<std::tuple<size_t, bool, size_t, bool>> deck_windows; ///< The windows of the ramp rule with an end at a bridge's deck: (variable, at the deck of that span's head), twice.
	for (const std::vector<Visit> &visits : route_visits) {
		for (size_t i = 0; i + 1 < visits.size(); i++) {
			if (visits[i + 1].break_before) continue;
			const SnapshotTile a = visits[i].tile;
			const SnapshotTile b = visits[i + 1].tile;
			LevelTile &la = tiles[a];
			la.has_next = true;
			la.next = b;
			if (la.rampable) {
				diff(var[b], var[a], -1, {0, a.first, a.second, b.first, b.second, 0});
				diff(var[a], var[b], -1, {1, a.first, a.second, b.first, b.second, 0});
			} else {
				equal(var[a], var[b], {2, a.first, a.second, b.first, b.second, 0});
			}
		}
		/* The ramp rule: L(t + tl) - L(t) <= k over the entries the train sees: a bridge's middle tiles and its far head are entered at the deck (a flat head climbs onto it, an inclined one is at it). */
		if (ramp_levels < train_length) {
			std::vector<std::pair<size_t, bool>> entries; ///< Per tile the variable of its level, or of the near head of the span whose deck it is entered at.
			for (size_t i = 0; i < visits.size(); i++) {
				const size_t v = var[visits[i].tile];
				if (visits[i].break_before && i > 0 && tiles.at(visits[i - 1].tile).visit.tunnel) {
					/* A tunnel's middle tiles are level at L, the level of its portals (a portal is no climb): each counts as a tile at the near portal's level. */
					const SnapshotTile &p = visits[i - 1].tile;
					const SnapshotTile &q = visits[i].tile;
					const int middles = std::abs(q.first - p.first) + std::abs(q.second - p.second) - 1;
					for (int m = 0; m < middles; m++) entries.emplace_back(var[p], false);
					entries.emplace_back(v, false);
				} else if (visits[i].break_before && i > 0 && partner[var[visits[i - 1].tile]] != SIZE_MAX) {
					const size_t near = var[visits[i - 1].tile];
					const SnapshotTile &p = visits[i - 1].tile;
					const SnapshotTile &q = visits[i].tile;
					const int middles = std::abs(q.first - p.first) + std::abs(q.second - p.second) - 1;
					for (int m = 0; m < middles; m++) entries.emplace_back(near, true);
					entries.emplace_back(near, true);
				} else {
					entries.emplace_back(v, false);
				}
			}
			for (size_t i = 0; i + train_length < entries.size(); i++) {
				const auto [sa, deck_a] = entries[i];
				const auto [sb, deck_b] = entries[i + train_length];
				if (deck_a && deck_b) continue;
				if (deck_a || deck_b) {
					deck_windows.emplace_back(sa, deck_a, sb, deck_b);
					/* The deck is at least the higher head: a necessary part of the rule as a difference. */
					if (deck_b && sa != sb && sa != partner[sb]) {
						diff(sa, sb, -ramp_levels, {3, order_tiles[sa].first, order_tiles[sa].second, order_tiles[sb].first, order_tiles[sb].second, ramp_levels});
						diff(sa, partner[sb], -ramp_levels, {3, order_tiles[sa].first, order_tiles[sa].second, order_tiles[partner[sb]].first, order_tiles[partner[sb]].second, ramp_levels});
					}
					continue;
				}
				if (sa == sb) continue;
				diff(sa, sb, -ramp_levels, {3, order_tiles[sa].first, order_tiles[sa].second, order_tiles[sb].first, order_tiles[sb].second, ramp_levels});
			}
		}
	}
	for (const auto &[prio, beside] : ties) {
		if (!var.contains(beside)) return fmt::format("the prio tile {} has no main-line tile {} beside it", name(prio), name(beside));
		/* A prio tile that is a lone axis piece (a visit of its own, no direction yet) ramps exactly as the main-line tile beside it: the same entry and exit levels. */
		LevelTile &lp = tiles[prio];
		const LevelTile &lm = tiles[beside];
		if (lp.rampable && lp.visit.dir == DiagDirection::Invalid) {
			lp.rampable = lm.rampable && lm.has_next;
			if (lp.rampable) {
				lp.visit.dir = lm.visit.dir;
				lp.has_next = true;
				lp.next = lm.next;
			}
		}
		equal(var[prio], var[beside], {4, prio.first, prio.second, beside.first, beside.second, 0});
	}
	/* The heads of a bridge are at the same z: their outer edges differ by at most one level (the head on the inclined slope is the higher); the portals of a tunnel are at one level. */
	for (const Span &span : spans) {
		if (span.tunnel) {
			equal(span.a, span.e, {9, span.start.first, span.start.second, span.end.first, span.end.second, 0});
			continue;
		}
		diff(span.a, span.e, -1, {9, span.start.first, span.start.second, span.end.first, span.end.second, 0});
		diff(span.e, span.a, -1, {9, span.end.first, span.end.second, span.start.first, span.start.second, 0});
	}
	/* A crosser under a bridge middle stays at or below both heads. */
	for (const JunctionBridge &b : layout.bridges) {
		if (b.tunnel) continue;
		const SnapshotTile a{b.start.x + offset.x, b.start.y + offset.y};
		const SnapshotTile e{b.end.x + offset.x, b.end.y + offset.y};
		if (!var.contains(a) || !var.contains(e)) continue;
		const int dx = (e.first > a.first) - (e.first < a.first);
		const int dy = (e.second > a.second) - (e.second < a.second);
		for (SnapshotTile t{a.first + dx, a.second + dy}; t != e; t = {t.first + dx, t.second + dy}) {
			if (!var.contains(t)) continue;
			diff(var[a], var[t], 0, {5, a.first, a.second, t.first, t.second, 0});
			diff(var[e], var[t], 0, {5, e.first, e.second, t.first, t.second, 0});
		}
	}

	/* The shrunk levels of the tiles must meet under the constraints so far. */
	const size_t cone_mark = pos.Mark();
	for (size_t i = 0; i < n && feasible; i++) {
		if (!cut[i]) continue;
		if (!pos.RestrictRange(i, reach[i].first, reach[i].second, reason({8, order_tiles[i].first, order_tiles[i].second, 0, 0, 0}))) {
			return fmt::format("the tiles around ({}, {}) cannot meet: they cannot reach the ground the terraform must keep", order_tiles[i].first, order_tiles[i].second);
		}
	}
	pos.Undo(cone_mark);

	/* The variable of each corner of a track tile: a rampable tile's exit-side corners are the next tile's level. */
	std::map<MapCorner, std::vector<size_t>> corner_vars;
	for (const auto &[tile, lt] : tiles) {
		for (int ci = 0; ci < 4; ci++) {
			size_t v = var[tile];
			if (lt.rampable && lt.has_next) {
				const std::array<int, 2> exit = ExitCorners(lt.visit.dir);
				if (ci == exit[0] || ci == exit[1]) v = var[lt.next];
			}
			corner_vars[{tile.first + CORNER_OFFSET[ci].first, tile.second + CORNER_OFFSET[ci].second}].push_back(v);
		}
	}
	/* A corner shared by two track tiles is one height. */
	for (const auto &[corner, vars] : corner_vars) {
		for (size_t i = 1; i < vars.size(); i++) {
			if (vars[i] != vars[0]) equal(vars[0], vars[i], {6, corner.first, corner.second, 0, 0, 0});
		}
	}
	/* Track corners within Manhattan distance d differ by at most d (the terraform slope limit; the clamp cannot join anything steeper). */
	constexpr int SLOPE_RANGE = 1;
	for (const auto &[corner, vars] : corner_vars) {
		for (int dx = -SLOPE_RANGE; dx <= SLOPE_RANGE; dx++) {
			for (int dy = -SLOPE_RANGE; dy <= SLOPE_RANGE; dy++) {
				const int d = std::abs(dx) + std::abs(dy);
				if (d == 0 || d > SLOPE_RANGE) continue;
				const auto other = corner_vars.find({corner.first + dx, corner.second + dy});
				if (other == corner_vars.end() || other->second[0] == vars[0]) continue;
				diff(vars[0], other->second[0], -d, {7, corner.first, corner.second, other->first.first, other->first.second, d});
			}
		}
	}

	auto explain = [&]() {
		std::string text = "no levels:";
		size_t count = 0;
		for (uint32_t r : pos.Explain()) {
			if (r >= reasons.size()) continue;
			text += (count++ == 0 ? " " : "; ") + why_text(reasons[r]);
			if (count >= 3) break;
		}
		return text;
	};
	if (!feasible) return explain();

	/* Groups of variables tied by equality. */
	std::vector<size_t> parent(n);
	for (size_t i = 0; i < n; i++) parent[i] = i;
	auto find = [&](size_t x) {
		while (parent[x] != x) x = parent[x] = parent[parent[x]];
		return x;
	};
	for (const auto &[a, b] : equal_pairs) parent[find(a)] = find(b);
	std::map<size_t, std::vector<size_t>> groups;
	for (size_t i = 0; i < n; i++) groups[find(i)].push_back(i);

	/* The start: each tile at its cheapest flat level within what the others leave it, heads first, then nodes and ports, then the rest. */
	std::vector<int> level(n, 0);
	const size_t start_mark = pos.Mark();
	for (int rank = 0; rank < 3; rank++) {
		for (size_t i = 0; i < n; i++) {
			const SnapshotTile &t = order_tiles[i];
			if ((heads.contains(t) ? 0 : fixed_tiles.contains(t) ? 1 : 2) != rank) continue;
			int best = INT_MIN;
			Money best_cost = INFINITE_COST;
			for (int l = pos.Lo(i); l <= pos.Up(i); l++) {
				const std::optional<Choice> choice = Evaluate(c, tiles[t].visit, 0, l + base);
				if (choice.has_value() && choice->cost < best_cost) {
					best_cost = choice->cost;
					best = l;
				}
			}
			if (best == INT_MIN) return fmt::format("no level for the tile {}", name(t));
			level[i] = best + base;
			if (!pos.RestrictRange(i, best, best, reason({8, t.first, t.second, 0, 0, 0}))) return explain();
		}
	}
	pos.Undo(start_mark);

	/* The choice of tile i at the levels lv: flat, or the ramp its exit level makes; memoised per tile, kind and level. */
	std::vector<size_t> next_var(n, SIZE_MAX);
	for (size_t i = 0; i < n; i++) {
		const LevelTile &lt = tiles[order_tiles[i]];
		if (lt.rampable && lt.has_next) next_var[i] = var[lt.next];
	}
	std::vector<int8_t> memo_state(n * 3 * (top + 1), 0); ///< 0 unknown, 1 has a choice, 2 none.
	std::vector<Choice> memo_choice(n * 3 * (top + 1));
	auto choice_at = [&](size_t i, const std::vector<int> &lv) -> const Choice * {
		int kind = 0;
		if (partner[i] != SIZE_MAX) {
			kind = lv[i] > lv[partner[i]] ? 2 : 0; /* the higher head of a bridge is on the inclined slope */
		} else if (next_var[i] != SIZE_MAX) {
			const int d = lv[next_var[i]] - lv[i];
			kind = d > 0 ? 1 : d < 0 ? 2 : 0;
		}
		const size_t slot = (i * 3 + kind) * (top + 1) + (lv[i] - base);
		if (memo_state[slot] == 0) {
			const std::optional<Choice> e = Evaluate(c, tiles[order_tiles[i]].visit, kind, lv[i]);
			memo_state[slot] = e.has_value() ? 1 : 2;
			if (e.has_value()) memo_choice[slot] = *e;
		}
		return memo_state[slot] == 1 ? &memo_choice[slot] : nullptr;
	};

	/* The exact objective: the finished terraform of the choices at the levels lv. The corners of the non-foundation tiles are fixed at the heights the choices demand;
	 * every other corner ends at its ground clamped to [L, U], the L1 cones (one level per step) of the raised and lowered fixed corners and of the corners nothing can move.
	 * Only the corners the cones reach are visited. */
	const int GW = c.terrain.width;
	const int GH = c.terrain.height;
	const int stride = cone.stride;
	const size_t rows = static_cast<size_t>(GH) + 1;
	const size_t corner_count = static_cast<size_t>(stride) * rows;
	std::vector<int> ground_h(corner_count, 0);
	for (int ty = 0; ty < GH; ty++) {
		for (int tx = 0; tx < GW; tx++) {
			const std::array<int, 4> cs = JunctionTileCorners(c.terrain.tiles[static_cast<size_t>(ty) * GW + tx]);
			for (int ci = 0; ci < 4; ci++) ground_h[cone.Index(cone.x0 + tx + CORNER_OFFSET[ci].first, cone.y0 + ty + CORNER_OFFSET[ci].second)] = cs[ci];
		}
	}
	constexpr int NO_TARGET = INT_MIN;
	constexpr int BIG = 1000000;
	constexpr Money VIOLATION = 1000000000LL; ///< Per level of violation: an infeasible assignment costs more than any feasible one and still has a gradient.
	constexpr Money NO_CHOICE = 1000000000000LL;
	/* The ground of a span's middle corners: a bridge's within [z - max_bridge_height + 1, z]; a tunnel's at L + 1 or more (a lower tile ends the tunnel early) with no upper bound. */
	auto span_lo = [&](const Span &span, int z) { return span.tunnel ? z + 1 : z - c.terrain.max_bridge_height + 1; };
	auto span_hi = [&](const Span &span, int z) { return span.tunnel ? BIG : z; };
	std::vector<int> ex_target(corner_count, NO_TARGET), ex_l(corner_count, -BIG), ex_u(corner_count, BIG);
	std::vector<int> ex_h = ground_h; ///< The heights after the terraform: the ground, except at the touched corners.
	std::vector<char> ex_dirty(static_cast<size_t>(GW) * GH, 0);
	std::vector<size_t> t_set, t_touch, t_dirty, t_queue;
	std::vector<std::array<int, 3>> q_raised, q_lowered; ///< The raised and the lowered targets as (x, y, target).
	std::vector<std::pair<size_t, int>> t_span_lo, t_span_hi; ///< The span corners the ground is below the least or above the greatest height of.
	std::vector<std::pair<size_t, int>> ex_moved; ///< The corners the last evaluation moved, as (index, height).
	std::vector<const Choice *> ex_ch(n);
	long ex_violation = 0; ///< The violation of the last evaluation.
	long ex_ramp_violation = 0; ///< The part of it that is the ramp rule at a bridge's deck.
	auto exact_cost = [&](const std::vector<int> &lv) -> Money {
		/* Reset what the last evaluation touched. */
		for (size_t k : t_set) ex_target[k] = NO_TARGET;
		for (size_t k : t_touch) {
			ex_l[k] = -BIG;
			ex_u[k] = BIG;
			ex_h[k] = ground_h[k];
		}
		for (size_t d : t_dirty) ex_dirty[d] = 0;
		t_set.clear();
		t_touch.clear();
		t_dirty.clear();
		ex_moved.clear();
		Money nochoice = 0;
		for (size_t i = 0; i < n; i++) {
			ex_ch[i] = choice_at(i, lv);
			if (ex_ch[i] == nullptr) nochoice += NO_CHOICE;
		}
		if (nochoice != 0) return nochoice;
		long viol = 0;
		for (size_t i = 0; i < n; i++) {
			if (ex_ch[i]->foundation) continue;
			const SnapshotTile &t = order_tiles[i];
			for (int ci = 0; ci < 4; ci++) {
				const size_t k = cone.Index(t.first + CORNER_OFFSET[ci].first, t.second + CORNER_OFFSET[ci].second);
				const int target = ex_ch[i]->corners[ci];
				if (ex_target[k] == NO_TARGET) {
					ex_target[k] = target;
					t_set.push_back(k);
				} else if (ex_target[k] != target) {
					viol += std::abs(ex_target[k] - target);
				}
			}
		}
		auto touch = [&](size_t k) {
			if (ex_l[k] == -BIG && ex_u[k] == BIG) t_touch.push_back(k);
		};
		/* The span's ground: at or below the heads' z and above z - max_bridge_height, as a bound on each corner of the middle tiles. */
		t_span_lo.clear();
		t_span_hi.clear();
		for (const Span &span : spans) {
			const int z = std::min(lv[span.a], lv[span.e]);
			for (const auto &[tile, k] : span.corners) {
				if (span_hi(span, z) < ground_h[k]) t_span_hi.emplace_back(k, span_hi(span, z));
				if (span_lo(span, z) > ground_h[k]) t_span_lo.emplace_back(k, span_lo(span, z));
			}
		}
		/* Raised targets spread L = t - d while it exceeds the ground; lowered ones spread U = t + d while it is below. */
		for (int pass = 0; pass < 2; pass++) {
			const bool up = pass == 0;
			std::vector<int> &bound = up ? ex_l : ex_u;
			t_queue.clear();
			for (size_t k : t_set) {
				if (up ? ex_target[k] > ground_h[k] : ex_target[k] < ground_h[k]) {
					touch(k);
					bound[k] = ex_target[k];
					t_queue.push_back(k);
				}
			}
			for (const auto &[k, value] : up ? t_span_lo : t_span_hi) {
				if (up ? value > bound[k] : value < bound[k]) {
					touch(k);
					bound[k] = value;
					t_queue.push_back(k);
				}
			}
			for (size_t qi = 0; qi < t_queue.size(); qi++) {
				const size_t k = t_queue[qi];
				const int v = up ? bound[k] - 1 : bound[k] + 1;
				const size_t x = k % stride;
				const size_t y = k / stride;
				const size_t nb[4] = {x > 0 ? k - 1 : SIZE_MAX, x + 1 < static_cast<size_t>(stride) ? k + 1 : SIZE_MAX, y > 0 ? k - stride : SIZE_MAX, y + 1 < rows ? k + stride : SIZE_MAX};
				for (size_t m : nb) {
					if (m == SIZE_MAX) continue;
					if (up ? (v > ground_h[m] && v > bound[m]) : (v < ground_h[m] && v < bound[m])) {
						touch(m);
						bound[m] = v;
						t_queue.push_back(m);
					}
				}
			}
		}
		/* A raised target against a lowered one, where no cone binds in between: they differ by at most their distance. */
		/* Only a raised target at least two above a lowered one can break the rule (they are at least one apart): collect the two kinds with their coordinates, and pair them. */
		q_raised.clear();
		q_lowered.clear();
		for (size_t k : t_set) {
			if (ex_target[k] > ground_h[k]) q_raised.push_back({static_cast<int>(k % stride), static_cast<int>(k / stride), ex_target[k]});
			else if (ex_target[k] < ground_h[k]) q_lowered.push_back({static_cast<int>(k % stride), static_cast<int>(k / stride), ex_target[k]});
		}
		if (!q_raised.empty() && !q_lowered.empty()) {
			int max_raised = INT_MIN, min_lowered = INT_MAX;
			for (const auto &r : q_raised) max_raised = std::max(max_raised, r[2]);
			for (const auto &l : q_lowered) min_lowered = std::min(min_lowered, l[2]);
			if (max_raised - min_lowered >= 2) {
				for (const auto &r : q_raised) {
					for (const auto &l : q_lowered) {
						const int dt = r[2] - l[2];
						if (dt < 2) continue;
						const int d = std::abs(r[0] - l[0]) + std::abs(r[1] - l[1]);
						if (d < dt) viol += dt - d;
					}
				}
			}
		}
		int64_t moves = 0;
		for (size_t k : t_touch) {
			const bool lo_bind = ex_l[k] > ground_h[k];
			const bool hi_bind = ex_u[k] < ground_h[k];
			int h = ground_h[k];
			if (lo_bind && hi_bind) viol += ex_l[k] - ex_u[k];
			else if (lo_bind) h = ex_l[k];
			else if (hi_bind) h = ex_u[k];
			/* The corners nothing can move (the base cone). */
			if (h > cone.upper[k]) viol += h - cone.upper[k];
			if (h < cone.lower[k]) viol += cone.lower[k] - h;
			ex_h[k] = h;
			if (ex_target[k] != NO_TARGET) viol += std::abs(h - ex_target[k]);
			if (h == ground_h[k]) continue;
			ex_moved.emplace_back(k, h);
			moves += std::abs(h - ground_h[k]);
			const int cx = static_cast<int>(k % stride);
			const int cy = static_cast<int>(k / stride);
			for (const auto &[dx, dy] : CORNER_OFFSET) {
				const int tx = cx - dx;
				const int ty = cy - dy;
				if (tx < 0 || ty < 0 || tx >= GW || ty >= GH) continue;
				const size_t d = static_cast<size_t>(ty) * GW + tx;
				if (!ex_dirty[d]) {
					ex_dirty[d] = 1;
					t_dirty.push_back(d);
				}
			}
		}
		for (const Span &span : spans) {
			const int z = std::min(lv[span.a], lv[span.e]);
			for (const auto &[tile, k] : span.corners) {
				viol += std::max(0, ex_h[k] - span_hi(span, z)) + std::max(0, span_lo(span, z) - ex_h[k]);
			}
		}
		Money cost = c.terrain.terraform_cost * moves;
		for (size_t d : t_dirty) cost += c.terrain.tiles[d].clear_cost;
		for (size_t i = 0; i < n; i++) {
			const SnapshotTile &t = order_tiles[i];
			const size_t di = static_cast<size_t>(t.second - cone.y0) * GW + (t.first - cone.x0);
			if (!ex_dirty[di]) {
				cost += ex_ch[i]->cost;
				continue;
			}
			if (!ex_ch[i]->foundation) continue;
			/* A foundation tile whose corners moved: flat at its level, or still a leveled foundation at that level (else infeasible). */
			std::array<int, 4> f;
			bool flat = true;
			for (int ci = 0; ci < 4; ci++) {
				f[ci] = ex_h[cone.Index(t.first + CORNER_OFFSET[ci].first, t.second + CORNER_OFFSET[ci].second)];
				flat = flat && f[ci] == ex_ch[i]->level;
			}
			if (tiles[t].visit.head) {
				/* A bridge head on the planned ground: the game's head test must give the head's z and slope. */
				const bool inclined = lv[i] > lv[partner[i]];
				JunctionTerrainTile moved = c.terrain.tiles[di];
				SetJunctionTileCorners(moved, f);
				/* Corners that spread two levels over other than three raised corners are no slope of the game (GetHighestSlopeCorner aborts on it): a violation. */
				if (IsSteepSlope(moved.slope) && !IsSlopeWithOneCornerRaised(ComplementSlope(static_cast<Slope>(moved.slope & ~SLOPE_STEEP)))) { viol++; continue; }
				const std::optional<BridgeHeadGround> ground = GetBridgeHeadGround(moved.slope, moved.z_min, tiles[t].visit.dir);
				if (!ground.has_value() || ground->inclined != inclined || ground->z != lv[i] - (inclined ? 1 : 0)) viol++;
				else if (ground->foundation) cost += c.terrain.foundation_cost;
				continue;
			}
			if (flat) continue;
			cost += c.terrain.foundation_cost;
			JunctionTerrainTile moved = c.terrain.tiles[di];
			SetJunctionTileCorners(moved, f);
			if (IsSteepSlope(moved.slope) && !IsSlopeWithOneCornerRaised(ComplementSlope(static_cast<Slope>(moved.slope & ~SLOPE_STEEP)))) { viol++; continue; }
			if (moved.z_max != ex_ch[i]->level || GetRailFoundation(moved.slope, tiles[t].visit.bits) != Foundation::Leveled) viol++;
		}
		/* The ramp rule at a bridge's deck: a flat head is entered at its level and the deck is one above it, an inclined head (the higher of the two) is at the deck. */
		ex_ramp_violation = 0;
		for (const auto &[sa, deck_a, sb, deck_b] : deck_windows) {
			auto level_of = [&](size_t v, bool deck) { return deck ? std::max(lv[v], lv[partner[v]]) + (lv[v] == lv[partner[v]] ? 1 : 0) : lv[v]; };
			ex_ramp_violation += std::max(0, level_of(sb, deck_b) - level_of(sa, deck_a) - ramp_levels);
		}
		viol += ex_ramp_violation;
		ex_violation = viol;
		return cost + viol * VIOLATION;
	};

	/* The incremental exact cost (P4). The state below is exact_cost(I_lv) kept up to date as one group's level changes: only what the change can reach is recomputed.
	 * Invariant: after every inc_apply the state equals what exact_cost computes at I_lv, because each stored quantity is a function of the choices (targets), of the
	 * corner bounds L and U (the fixed point of the cone propagation: the maximum over the raised seeds of seed - path length, the path staying above the ground, and the dual for U)
	 * and of the held heights; a quantity is recomputed whenever one of its inputs changed, and an input that did not change cannot change it:
	 *   - a tile's choice depends on I_lv of itself, its partner head and its next tile (the group's dependants);
	 *   - a corner's effective target and conflict violation on the choices of the tiles that touch it (at most four);
	 *   - L at a corner changes only if a seed whose cone (single-source reach, a superset: the multi-source reach of max(old, new) at the changed seeds) contains it changed,
	 *     and then it is recomputed inside that reach from the seeds there and from the unchanged bound of the neighbours outside it;
	 *   - the raised-against-lowered violation changes only in the pairs that contain a corner whose target changed;
	 *   - a tile's foundation cost depends on its choice, whether it is dirty and the heights of its corners (the corners that changed height are visited). */
	struct IncCont {
		int tile;
		int ci;
	};
	std::vector<std::array<IncCont, 4>> inc_cont(corner_count);
	std::vector<uint8_t> inc_cont_n(corner_count, 0);
	std::vector<int> tile_of_cell(static_cast<size_t>(GW) * GH, -1);
	std::vector<size_t> tile_cell(n);
	std::vector<std::array<size_t, 4>> tile_corner(n);
	for (size_t i = 0; i < n; i++) {
		const SnapshotTile &t = order_tiles[i];
		tile_cell[i] = static_cast<size_t>(t.second - cone.y0) * GW + (t.first - cone.x0);
		tile_of_cell[tile_cell[i]] = static_cast<int>(i);
		for (int ci = 0; ci < 4; ci++) {
			const size_t k = cone.Index(t.first + CORNER_OFFSET[ci].first, t.second + CORNER_OFFSET[ci].second);
			tile_corner[i][ci] = k;
			inc_cont[k][inc_cont_n[k]++] = {static_cast<int>(i), ci};
		}
	}
	std::vector<int> sp_first(corner_count, -1);
	std::vector<std::array<int, 2>> sp_ent; ///< (span, next entry) lists of the spans a corner belongs to.
	for (size_t si = 0; si < spans.size(); si++) {
		for (const auto &[tile, k] : spans[si].corners) {
			sp_ent.push_back({static_cast<int>(si), sp_first[k]});
			sp_first[k] = static_cast<int>(sp_ent.size() - 1);
		}
	}
	const int T_OFF = base - 1; ///< Targets lie in [base - 1, base + top + 1]: a level in [base, base + top], a corner within one of it.
	std::vector<int> I_lv = level;
	std::vector<const Choice *> I_ch(n, nullptr);
	std::vector<int> I_tgt(corner_count, NO_TARGET), I_cviol(corner_count, 0);
	std::vector<int> I_seed_up(corner_count, -BIG), I_seed_dn(corner_count, BIG), I_sp_up(corner_count, -BIG), I_sp_dn(corner_count, BIG);
	std::vector<int> I_L(corner_count, -BIG), I_U(corner_count, BIG), I_h = ground_h, I_mv(corner_count, 0);
	std::vector<long> I_tv(corner_count, 0);
	std::vector<char> I_moved(corner_count, 0);
	std::vector<int> I_dcnt(static_cast<size_t>(GW) * GH, 0);
	std::vector<Money> I_tcost(n, 0);
	std::vector<long> I_tviol(n, 0);
	std::vector<int> I_span_z(spans.size(), INT_MIN);
	std::vector<int> I_hist_r(static_cast<size_t>(top) + 4, 0), I_hist_l(static_cast<size_t>(top) + 4, 0);
	std::vector<size_t> I_rl, I_ll;
	std::vector<int> I_pos(corner_count, -1);
	Money I_tiles = 0, I_clear = 0;
	long I_moves = 0, I_viol_tgt = 0, I_viol_touch = 0, I_viol_tile = 0, I_pairs = 0;
	std::vector<uint32_t> st_kt(corner_count, 0), st_seed(corner_count, 0), st_up(corner_count, 0), st_dn(corner_count, 0), st_term(corner_count, 0), st_tile(n, 0), st_cell(static_cast<size_t>(GW) * GH, 0);
	std::vector<int> rv(corner_count, 0);
	uint32_t epoch = 0;
	std::vector<size_t> l_kt, l_kc, l_seed, l_up, l_dn, l_term, l_tile, l_queue, l_spk;
	std::vector<size_t> l_changed;
	std::vector<std::pair<size_t, int>> seeds_up, seeds_dn;
	std::vector<const Choice *> new_ch;

	auto for_nb = [&](size_t k, auto &&f) {
		const size_t x = k % stride;
		const size_t y = k / stride;
		if (x > 0) f(k - 1);
		if (x + 1 < static_cast<size_t>(stride)) f(k + 1);
		if (y > 0) f(k - stride);
		if (y + 1 < rows) f(k + stride);
	};

	/* The raised-against-lowered violation: the sum over pairs (r, l) of dt - d where dt = target_r - target_l >= 2 and d < dt (d the Manhattan distance). */
	auto pair_scan = [&](size_t k, int tk, bool raised) -> long {
		const std::vector<int> &hist = raised ? I_hist_l : I_hist_r;
		const std::vector<size_t> &list = raised ? I_ll : I_rl;
		if (list.empty()) return 0;
		int rho = 0;
		if (raised) {
			size_t lo = 0;
			while (hist[lo] == 0) lo++;
			rho = tk - (static_cast<int>(lo) + T_OFF) - 1;
		} else {
			size_t hi = hist.size() - 1;
			while (hist[hi] == 0) hi--;
			rho = (static_cast<int>(hi) + T_OFF) - tk - 1;
		}
		if (rho < 1) return 0;
		long sum = 0;
		const int cx = static_cast<int>(k % stride);
		const int cy = static_cast<int>(k / stride);
		if (static_cast<size_t>(2 * rho * rho + 2 * rho + 1) < list.size()) {
			for (int dy = -rho; dy <= rho; dy++) {
				const int y = cy + dy;
				if (y < 0 || y >= static_cast<int>(rows)) continue;
				const int r2 = rho - std::abs(dy);
				for (int dx = -r2; dx <= r2; dx++) {
					const int x = cx + dx;
					if (x < 0 || x >= stride || (dx == 0 && dy == 0)) continue;
					const size_t m = static_cast<size_t>(y) * stride + x;
					const int tm = I_tgt[m];
					if (tm == NO_TARGET || (raised ? tm >= ground_h[m] : tm <= ground_h[m])) continue;
					const int dt = raised ? tk - tm : tm - tk;
					const int d = std::abs(dx) + std::abs(dy);
					if (dt >= 2 && d < dt) sum += dt - d;
				}
			}
		} else {
			for (size_t m : list) {
				const int dt = raised ? tk - I_tgt[m] : I_tgt[m] - tk;
				const int d = std::abs(static_cast<int>(m % stride) - cx) + std::abs(static_cast<int>(m / stride) - cy);
				if (dt >= 2 && d < dt) sum += dt - d;
			}
		}
		return sum;
	};
	auto pair_register = [&](size_t k, int tk, bool add) {
		const int g = ground_h[k];
		if (tk == NO_TARGET || tk == g) return;
		const bool raised = tk > g;
		std::vector<size_t> &list = raised ? I_rl : I_ll;
		std::vector<int> &hist = raised ? I_hist_r : I_hist_l;
		const long s = pair_scan(k, tk, raised);
		if (add) {
			I_pairs += s;
			I_pos[k] = static_cast<int>(list.size());
			list.push_back(k);
			hist[static_cast<size_t>(tk - T_OFF)]++;
		} else {
			I_pairs -= s;
			const int p = I_pos[k];
			list[p] = list.back();
			I_pos[list[p]] = p;
			list.pop_back();
			hist[static_cast<size_t>(tk - T_OFF)]--;
		}
	};

	/* The effective target of a corner (the first non-foundation tile's, in tile order) and the violation of the later tiles that differ. */
	auto corner_target = [&](size_t k, int &eff, long &viol) {
		eff = NO_TARGET;
		viol = 0;
		for (int j = 0; j < inc_cont_n[k]; j++) {
			const IncCont &cc = inc_cont[k][j];
			const Choice *ch = I_ch[cc.tile];
			if (ch->foundation) continue;
			const int target = ch->corners[cc.ci];
			if (eff == NO_TARGET) eff = target;
			else if (eff != target) viol += std::abs(eff - target);
		}
	};

	/* The foundation part of a tile: its cost and violation given its choice, whether it is dirty and its corner heights. */
	auto tile_eval = [&](size_t i, Money &cost_out, long &viol_out) {
		const SnapshotTile &t = order_tiles[i];
		const size_t di = tile_cell[i];
		const Choice *ch = I_ch[i];
		cost_out = 0;
		viol_out = 0;
		if (!I_dcnt[di]) {
			cost_out = ch->cost;
			return;
		}
		if (!ch->foundation) return;
		std::array<int, 4> f;
		bool flat = true;
		for (int ci = 0; ci < 4; ci++) {
			f[ci] = I_h[tile_corner[i][ci]];
			flat = flat && f[ci] == ch->level;
		}
		if (tiles[t].visit.head) {
			const bool inclined = I_lv[i] > I_lv[partner[i]];
			JunctionTerrainTile moved = c.terrain.tiles[di];
			SetJunctionTileCorners(moved, f);
			if (IsSteepSlope(moved.slope) && !IsSlopeWithOneCornerRaised(ComplementSlope(static_cast<Slope>(moved.slope & ~SLOPE_STEEP)))) { viol_out++; return; }
			const std::optional<BridgeHeadGround> ground = GetBridgeHeadGround(moved.slope, moved.z_min, tiles[t].visit.dir);
			if (!ground.has_value() || ground->inclined != inclined || ground->z != I_lv[i] - (inclined ? 1 : 0)) viol_out++;
			else if (ground->foundation) cost_out += c.terrain.foundation_cost;
			return;
		}
		if (flat) return;
		cost_out += c.terrain.foundation_cost;
		JunctionTerrainTile moved = c.terrain.tiles[di];
		SetJunctionTileCorners(moved, f);
		if (IsSteepSlope(moved.slope) && !IsSlopeWithOneCornerRaised(ComplementSlope(static_cast<Slope>(moved.slope & ~SLOPE_STEEP)))) { viol_out++; return; }
		if (moved.z_max != ch->level || GetRailFoundation(moved.slope, tiles[t].visit.bits) != Foundation::Leveled) viol_out++;
	};

	/* Reaches (support of the max-propagation from the seeds) and the bounds inside them. */
	auto reach_and_bound = [&](bool up, const std::vector<std::pair<size_t, int>> &seeds, std::vector<uint32_t> &stamp, std::vector<size_t> &list) {
		list.clear();
		l_queue.clear();
		for (const auto &[k, v] : seeds) {
			if (stamp[k] != epoch) {
				stamp[k] = epoch;
				list.push_back(k);
				rv[k] = v;
				l_queue.push_back(k);
			} else if (up ? v > rv[k] : v < rv[k]) {
				rv[k] = v;
				l_queue.push_back(k);
			}
		}
		for (size_t qi = 0; qi < l_queue.size(); qi++) {
			const size_t k = l_queue[qi];
			const int v = up ? rv[k] - 1 : rv[k] + 1;
			for_nb(k, [&](size_t m) {
				if (!(up ? v > ground_h[m] : v < ground_h[m])) return;
				if (stamp[m] != epoch) {
					stamp[m] = epoch;
					list.push_back(m);
					rv[m] = v;
					l_queue.push_back(m);
				} else if (up ? v > rv[m] : v < rv[m]) {
					rv[m] = v;
					l_queue.push_back(m);
				}
			});
		}
		std::vector<int> &bound = up ? I_L : I_U;
		const std::vector<int> &seed = up ? I_seed_up : I_seed_dn;
		for (size_t k : list) bound[k] = up ? -BIG : BIG;
		l_queue.clear();
		for (size_t k : list) {
			int v = seed[k];
			for_nb(k, [&](size_t m) {
				if (stamp[m] == epoch) return;
				const int bm = bound[m];
				if (up ? bm == -BIG : bm == BIG) return;
				const int w = up ? bm - 1 : bm + 1;
				if (up ? (w > ground_h[k] && w > v) : (w < ground_h[k] && w < v)) v = w;
			});
			bound[k] = v;
			if (up ? v > -BIG : v < BIG) l_queue.push_back(k);
		}
		for (size_t qi = 0; qi < l_queue.size(); qi++) {
			const size_t k = l_queue[qi];
			const int v = up ? bound[k] - 1 : bound[k] + 1;
			for_nb(k, [&](size_t m) {
				if (stamp[m] != epoch) return;
				if (up ? (v > ground_h[m] && v > bound[m]) : (v < ground_h[m] && v < bound[m])) {
					bound[m] = v;
					l_queue.push_back(m);
				}
			});
		}
	};

	long inc_viol_last = 0;
	/** Bring the state to I_lv, re-evaluating the tiles of dep; false (and the cost of the missing choices in cost) when a tile has no choice there (the state is then unchanged). */
	auto inc_apply = [&](const std::vector<size_t> &dep, Money &cost) -> bool {
		new_ch.clear();
		Money nochoice = 0;
		for (size_t i : dep) {
			const Choice *ch = choice_at(i, I_lv);
			new_ch.push_back(ch);
			if (ch == nullptr) nochoice += NO_CHOICE;
		}
		if (nochoice != 0) {
			cost = nochoice;
			return false;
		}
		epoch++;
		l_changed.clear();
		l_kt.clear();
		l_kc.clear();
		l_seed.clear();
		l_term.clear();
		l_tile.clear();
		l_spk.clear();
		seeds_up.clear();
		seeds_dn.clear();
		/* 1. the changed choices and the corners they touch */
		for (size_t j = 0; j < dep.size(); j++) {
			const size_t i = dep[j];
			if (I_ch[i] == new_ch[j]) continue;
			I_ch[i] = new_ch[j];
			l_changed.push_back(i);
			for (int ci = 0; ci < 4; ci++) {
				const size_t k = tile_corner[i][ci];
				if (st_kt[k] != epoch) {
					st_kt[k] = epoch;
					l_kt.push_back(k);
				}
			}
		}
		/* 2. effective targets, conflicts, and the pair violation */
		for (size_t k : l_kt) {
			int eff;
			long viol;
			corner_target(k, eff, viol);
			I_viol_tgt += viol - I_cviol[k];
			I_cviol[k] = viol;
			if (eff != I_tgt[k]) {
				pair_register(k, I_tgt[k], false);
				I_tgt[k] = eff;
				pair_register(k, eff, true);
				l_kc.push_back(k);
			}
		}
		/* 3. the span bounds: the spans whose z changed */
		for (size_t si = 0; si < spans.size(); si++) {
			const int z = std::min(I_lv[spans[si].a], I_lv[spans[si].e]);
			if (z == I_span_z[si]) continue;
			I_span_z[si] = z;
			for (const auto &[tile, k] : spans[si].corners) l_spk.push_back(k);
		}
		for (size_t k : l_spk) {
			int up = -BIG, dn = BIG;
			for (int e = sp_first[k]; e >= 0; e = sp_ent[e][1]) {
				const Span &span = spans[sp_ent[e][0]];
				const int z = I_span_z[sp_ent[e][0]];
				if (span_hi(span, z) < ground_h[k]) dn = std::min(dn, span_hi(span, z));
				if (span_lo(span, z) > ground_h[k]) up = std::max(up, span_lo(span, z));
			}
			I_sp_up[k] = up;
			I_sp_dn[k] = dn;
			if (st_seed[k] != epoch) {
				st_seed[k] = epoch;
				l_seed.push_back(k);
			}
		}
		for (size_t k : l_kc) {
			if (st_seed[k] != epoch) {
				st_seed[k] = epoch;
				l_seed.push_back(k);
			}
		}
		/* 4. the seeds that changed and the bounds in their reach */
		for (size_t k : l_seed) {
			const int g = ground_h[k];
			const int up = std::max(I_tgt[k] != NO_TARGET && I_tgt[k] > g ? I_tgt[k] : -BIG, I_sp_up[k]);
			const int dn = std::min(I_tgt[k] != NO_TARGET && I_tgt[k] < g ? I_tgt[k] : BIG, I_sp_dn[k]);
			if (up != I_seed_up[k]) {
				seeds_up.emplace_back(k, std::max(up, I_seed_up[k]));
				I_seed_up[k] = up;
			}
			if (dn != I_seed_dn[k]) {
				seeds_dn.emplace_back(k, std::min(dn, I_seed_dn[k]));
				I_seed_dn[k] = dn;
			}
		}
		reach_and_bound(true, seeds_up, st_up, l_up);
		reach_and_bound(false, seeds_dn, st_dn, l_dn);
		/* 5. the corner terms where a bound or a target changed */
		for (const std::vector<size_t> *lst : {&l_up, &l_dn, &l_kc}) {
			for (size_t k : *lst) {
				if (st_term[k] != epoch) {
					st_term[k] = epoch;
					l_term.push_back(k);
				}
			}
		}
		for (size_t k : l_term) {
			const int g = ground_h[k];
			const bool lo = I_L[k] > g;
			const bool hi = I_U[k] < g;
			long v = 0;
			int h = g;
			if (lo || hi) {
				if (lo && hi) v += I_L[k] - I_U[k];
				else if (lo) h = I_L[k];
				else h = I_U[k];
				if (h > cone.upper[k]) v += h - cone.upper[k];
				if (h < cone.lower[k]) v += cone.lower[k] - h;
				if (I_tgt[k] != NO_TARGET) v += std::abs(h - I_tgt[k]);
			}
			I_viol_touch += v - I_tv[k];
			I_tv[k] = v;
			const int mv = std::abs(h - g);
			I_moves += mv - I_mv[k];
			I_mv[k] = mv;
			if (h == I_h[k]) continue;
			I_h[k] = h;
			const bool moved = h != g;
			const int cx = static_cast<int>(k % stride);
			const int cy = static_cast<int>(k / stride);
			for (const auto &[dx, dy] : CORNER_OFFSET) {
				const int tx = cx - dx;
				const int ty = cy - dy;
				if (tx < 0 || ty < 0 || tx >= GW || ty >= GH) continue;
				const size_t d = static_cast<size_t>(ty) * GW + tx;
				if (moved != static_cast<bool>(I_moved[k])) {
					if (moved) {
						if (I_dcnt[d]++ == 0) I_clear += c.terrain.tiles[d].clear_cost;
					} else {
						if (--I_dcnt[d] == 0) I_clear -= c.terrain.tiles[d].clear_cost;
					}
				}
				if (tile_of_cell[d] >= 0 && st_tile[tile_of_cell[d]] != epoch) {
					st_tile[tile_of_cell[d]] = epoch;
					l_tile.push_back(static_cast<size_t>(tile_of_cell[d]));
				}
			}
			I_moved[k] = moved;
		}
		/* 6. the tiles whose choice, dirtiness or corner heights changed */
		for (size_t i : l_changed) {
			if (st_tile[i] != epoch) {
				st_tile[i] = epoch;
				l_tile.push_back(i);
			}
		}
		for (size_t i : l_tile) {
			Money tc;
			long tv;
			tile_eval(i, tc, tv);
			I_tiles += tc - I_tcost[i];
			I_tcost[i] = tc;
			I_viol_tile += tv - I_tviol[i];
			I_tviol[i] = tv;
		}
		/* 7. the span's ground and the ramp rule: small, recomputed */
		long viol = I_viol_tgt + I_pairs + I_viol_touch + I_viol_tile;
		for (size_t si = 0; si < spans.size(); si++) {
			const int z = I_span_z[si];
			for (const auto &[tile, k] : spans[si].corners) viol += std::max(0, I_h[k] - span_hi(spans[si], z)) + std::max(0, span_lo(spans[si], z) - I_h[k]);
		}
		for (const auto &[sa, deck_a, sb, deck_b] : deck_windows) {
			auto level_of = [&](size_t v, bool deck) { return deck ? std::max(I_lv[v], I_lv[partner[v]]) + (I_lv[v] == I_lv[partner[v]] ? 1 : 0) : I_lv[v]; };
			viol += std::max(0, level_of(sb, deck_b) - level_of(sa, deck_a) - ramp_levels);
		}
		inc_viol_last = viol;
		cost = c.terrain.terraform_cost * I_moves + I_clear + I_tiles + viol * VIOLATION;
		return true;
	};

	/* Coordinate descent on the groups: each in turn takes the level in its feasible range (the rest held at their levels) that costs least. */
	constexpr int MAX_ROUNDS = 20;
	constexpr bool P4_INCREMENTAL = true;
	Money level_cost = 0;
	bool level_cost_known = false;
	std::vector<std::vector<size_t>> who_next(n);
	for (size_t i = 0; i < n; i++) {
		if (next_var[i] != SIZE_MAX) who_next[next_var[i]].push_back(i);
	}
	/* The incremental state is built at the start levels when every tile has a choice there; else the descent evaluates every trial in full. */
	bool inc_ok = false;
	if constexpr (P4_INCREMENTAL) {
		bool all = true;
		for (size_t i = 0; i < n && all; i++) all = choice_at(i, level) != nullptr;
		if (all) {
			std::vector<size_t> everyone(n);
			for (size_t i = 0; i < n; i++) everyone[i] = i;
			Money cost0 = 0;
			inc_ok = inc_apply(everyone, cost0);
			level_cost = cost0;
			level_cost_known = inc_ok;
			if (incremental_check != nullptr) {
				incremental_check->first++;
				if (cost0 != exact_cost(level) || inc_viol_last != ex_violation) incremental_check->second++;
			}
		}
	}
	std::vector<size_t> dep;
	for (int round = 0; round < MAX_ROUNDS; round++) {
		bool moved = false;
		for (const auto &[key, members] : groups) {
			const std::set<size_t> member_set(members.begin(), members.end());
			/* The range the constraints to the held outside tiles leave the group (all have k <= 0, so no propagation through the others is needed). */
			int lo = 0;
			int up = top;
			for (size_t m : members) {
				for (const auto &[o, k] : lower[m]) {
					if (!member_set.contains(o)) lo = std::max(lo, level[o] - base + k);
				}
				for (const auto &[a, k] : upper[m]) {
					if (!member_set.contains(a)) up = std::min(up, level[a] - base - k);
				}
			}
			if (lo >= up) continue;
			const int current = level[members.front()];
			/* The cost of the held levels is the same until a group moves: it is not evaluated again for every group. */
			if (!level_cost_known) {
				level_cost = exact_cost(level);
				level_cost_known = true;
			}
			Money best_cost = level_cost;
			int best = current;
			if (inc_ok) {
				/* The tiles whose choice depends on the group: its members, their partner heads and the tiles that exit into a member. */
				dep.clear();
				for (size_t m : members) {
					dep.push_back(m);
					if (partner[m] != SIZE_MAX) dep.push_back(partner[m]);
					for (size_t i : who_next[m]) dep.push_back(i);
				}
				std::sort(dep.begin(), dep.end());
				dep.erase(std::unique(dep.begin(), dep.end()), dep.end());
				int applied = current;
				for (int l = lo + base; l <= up + base; l++) {
					if (l == current) continue;
					for (size_t m : members) I_lv[m] = l;
					Money cost_here = 0;
					if (inc_apply(dep, cost_here)) applied = l;
					if (incremental_check != nullptr) {
						const Money full = exact_cost(I_lv);
						incremental_check->first++;
						if (full != cost_here || (applied == l && inc_viol_last != ex_violation)) incremental_check->second++;
					}
					if (cost_here < best_cost) {
						best_cost = cost_here;
						best = l;
					}
				}
				if (applied != best) {
					for (size_t m : members) I_lv[m] = best;
					Money back = 0;
					if (!inc_apply(dep, back)) std::abort();
				}
			} else {
				std::vector<int> trial = level;
				for (int l = lo + base; l <= up + base; l++) {
					if (l == current) continue;
					for (size_t m : members) trial[m] = l;
					const Money cost_here = exact_cost(trial);
					if (cost_here < best_cost) {
						best_cost = cost_here;
						best = l;
					}
				}
			}
			if (best != current) {
				for (size_t m : members) level[m] = best;
				level_cost = best_cost;
				moved = true;
			}
		}
		if (!moved) break;
	}

	/* The finished terraform: the clamp of the final levels. */
	const Money final_cost = exact_cost(level);
	for (size_t i = 0; i < n; i++) {
		if (ex_ch[i] == nullptr) return fmt::format("no choice for the tile {}", name(order_tiles[i]));
	}
	if (ex_violation > 0) {
		for (const Span &span : spans) {
			const int z = std::min(level[span.a], level[span.e]);
			for (const auto &[tile, k] : span.corners) {
				if (ex_h[k] > span_hi(span, z) || ex_h[k] < span_lo(span, z)) {
					if (span.tunnel) return fmt::format("the tunnel from {} to {} cannot keep the ground of tile {} above {}: its corner ends at {}", name(span.start), name(span.end), name(tile), z, ex_h[k]);
					return fmt::format("the bridge from {} to {} cannot keep the ground of tile {} at most {} and above {}: its corner ends at {}", name(span.start), name(span.end), name(tile), z, z - c.terrain.max_bridge_height, ex_h[k]);
				}
			}
		}
	}
	if (ex_ramp_violation > 0) return fmt::format("ramp rule: a route climbs more than {} levels within {} tiles onto or across a bridge", ramp_levels, train_length);
	if (ex_violation > 0) return fmt::format("terraform conflict: the corners the tracks fix cannot be joined by one terraform (violation {})", ex_violation);
	for (size_t i = 0; i < n; i++) {
		const Visit &v = tiles[order_tiles[i]].visit;
		chosen[order_tiles[i]] = {*ex_ch[i], v.edge, v.copy};
	}
	int64_t moves = 0;
	std::set<SnapshotTile> dirty;
	for (const auto &[k, h] : ex_moved) {
		const MapCorner corner{cone.x0 + static_cast<int>(k % stride), cone.y0 + static_cast<int>(k / stride)};
		terraform.corners[corner] = h;
		moves += std::abs(h - ground_h[k]);
		for (const auto &[dx, dy] : CORNER_OFFSET) dirty.emplace(corner.first - dx, corner.second - dy);
	}
	terraform.cost = c.terrain.terraform_cost * moves;
	for (const SnapshotTile &t : dirty) {
		if (const JunctionTerrainTile *tile = c.terrain.At(t.first, t.second); tile != nullptr) terraform.cost += tile->clear_cost;
	}
	total = final_cost;
	return "";
}

} // namespace

bool SolveJunctionLevels(const JunctionTerrain &terrain, const JunctionFacts &facts, TileIndexDiffC offset, int ramp_levels, int train_length, const JunctionTrackPlacement &placement, const JunctionLayout &layout, const std::vector<JunctionTrackRoute> &routes, const std::vector<std::pair<TileIndexDiffC, TileIndexDiffC>> &prio_beside, JunctionTerraform &plan, Money &cost, std::vector<std::pair<TileIndexDiffC, int8_t>> &levels, std::string *why, std::pair<long, long> *incremental_check)
{
	const Context ctx{terrain, facts};
	auto fail = [&](const std::string &reason) {
		if (why != nullptr) *why = reason;
		return false;
	};
	auto world = [&](TileIndexDiffC t) { return SnapshotTile{t.x + offset.x, t.y + offset.y}; };

	std::map<SnapshotTile, TrackBits> tile_bits;
	for (const JunctionTrack &jt : layout.tracks) tile_bits[world(jt.offset)] |= jt.tracks;
	std::set<SnapshotTile> heads;
	std::map<SnapshotTile, DiagDirection> into_span; ///< Per head of a bridge or portal of a tunnel, the direction from its outer edge into the span.
	std::set<SnapshotTile> portals; ///< The portals of the tunnels.
	for (const JunctionBridge &b : layout.bridges) {
		heads.insert(world(b.start));
		heads.insert(world(b.end));
		const bool along_x = b.start.y == b.end.y;
		const bool start_first = along_x ? b.start.x < b.end.x : b.start.y < b.end.y;
		into_span[world(b.start)] = along_x ? (start_first ? DiagDirection::SW : DiagDirection::NE) : (start_first ? DiagDirection::SE : DiagDirection::NW);
		into_span[world(b.end)] = along_x ? (start_first ? DiagDirection::NE : DiagDirection::SW) : (start_first ? DiagDirection::NW : DiagDirection::SE);
		if (b.tunnel) {
			portals.insert(world(b.start));
			portals.insert(world(b.end));
		}
		/* No track is laid on a head: it is judged as the bridge's axis track (flat; its incline is kept out of this step). */
		const TrackBits axis_track{b.start.y == b.end.y ? Track::X : Track::Y};
		tile_bits[world(b.start)] |= axis_track;
		tile_bits[world(b.end)] |= axis_track;
	}
	std::set<SnapshotTile> fixed_tiles = heads;
	for (const TileIndexDiffC &t : placement.nodes) fixed_tiles.insert(world(t));
	for (const TileIndexDiffC &t : placement.ports) fixed_tiles.insert(world(t));

	/* The tiles of every route in travel order, one visit per tile. */
	std::vector<std::vector<Visit>> route_visits;
	for (const JunctionTrackRoute &route : routes) {
		std::vector<Visit> visits;
		bool broken = false;
		for (size_t j = 0; j < route.pieces.size(); j++) {
			const JunctionPiece &piece = route.pieces[j];
			const SnapshotTile tile = world(piece.tile);
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
			if (const auto head = into_span.find(tile); head != into_span.end()) {
				v.head = !portals.contains(tile);
				v.tunnel = portals.contains(tile);
				v.dir = head->second;
			}
			visits.push_back(v);
		}
		route_visits.push_back(std::move(visits));
	}

	/* The prio elements: each tile one flat visit of its own, tied to the main-line tile beside it. */
	std::vector<std::pair<SnapshotTile, SnapshotTile>> ties;
	std::set<SnapshotTile> routed;
	for (const std::vector<Visit> &visits : route_visits) {
		for (const Visit &v : visits) routed.insert(v.tile);
	}
	for (const auto &[prio, beside] : prio_beside) {
		const SnapshotTile tile = world(prio);
		ties.emplace_back(tile, world(beside));
		if (routed.contains(tile)) continue;
		routed.insert(tile);
		Visit v;
		v.tile = tile;
		v.bits = tile_bits.at(tile);
		v.axis = v.bits == TrackBits{Track::X} || v.bits == TrackBits{Track::Y};
		route_visits.push_back({v});
	}

	std::map<SnapshotTile, Chosen> chosen;
	JunctionTerraform result;
	Money total = 0;
	const std::string levels_why = SolveTileLevels(ctx, route_visits, heads, fixed_tiles, ties, layout, offset, ramp_levels, train_length, chosen, result, total, incremental_check);
	if (!levels_why.empty()) return fail(levels_why);

	plan = std::move(result);
	cost = total;
	levels.clear();
	for (const auto &[tile, item] : chosen) levels.emplace_back(TileIndexDiffC{static_cast<int16_t>(tile.first - offset.x), static_cast<int16_t>(tile.second - offset.y)}, static_cast<int8_t>(item.choice.level));
	return true;
}
