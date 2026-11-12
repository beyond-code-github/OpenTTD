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
#include "direction_func.h"
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
	std::vector<SnapshotTile> preds; ///< The rampable tiles whose route goes on to this one.
};

/**
 * The joint level solve (RECON-joint-levels; DESIGN-G1 §15.3): one level per track tile, in a JunctionPositions.
 * The level of a tile is its entry level in the direction of travel. A tile that is not rampable continues at its own level; a rampable one at most one level either way.
 * The ramp rule (D219) is L(t + tl) - L(t) <= k per route, a crosser under a bridge middle stays at or below both heads, a corner shared by two track tiles is one height
 * (a rampable tile's exit-side corners are the next tile's level) and track corners within Manhattan distance d differ by at most d.
 * The start is each tile at its cheapest flat level; a coordinate descent over the tiles tied by equality then lowers the cost.
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
 * @return The empty string on success, else the reason.
 */
std::string SolveTileLevels(const Context &c, const std::vector<std::vector<Visit>> &route_visits, const std::set<SnapshotTile> &heads, const std::set<SnapshotTile> &fixed_tiles, const std::vector<std::pair<SnapshotTile, SnapshotTile>> &ties, const JunctionLayout &layout, TileIndexDiffC offset, int ramp_levels, int train_length, std::map<SnapshotTile, Chosen> &chosen)
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
		const int kinds = lt.rampable && probe.dir != DiagDirection::Invalid ? 3 : 1;
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

	for (const std::vector<Visit> &visits : route_visits) {
		for (size_t i = 0; i + 1 < visits.size(); i++) {
			if (visits[i + 1].break_before) continue;
			const SnapshotTile a = visits[i].tile;
			const SnapshotTile b = visits[i + 1].tile;
			LevelTile &la = tiles[a];
			la.has_next = true;
			la.next = b;
			if (la.rampable) {
				tiles[b].preds.push_back(a);
				diff(var[b], var[a], -1, {0, a.first, a.second, b.first, b.second, 0});
				diff(var[a], var[b], -1, {1, a.first, a.second, b.first, b.second, 0});
			} else {
				equal(var[a], var[b], {2, a.first, a.second, b.first, b.second, 0});
			}
		}
		/* The ramp rule: L(t + tl) - L(t) <= k, not across a bridge middle. */
		if (ramp_levels < train_length) {
			for (size_t i = 0; i + train_length < visits.size(); i++) {
				bool broken = false;
				for (size_t j = i + 1; j <= i + train_length; j++) broken = broken || visits[j].break_before;
				const SnapshotTile a = visits[i].tile;
				const SnapshotTile b = visits[i + train_length].tile;
				if (broken || a == b) continue;
				diff(var[a], var[b], -ramp_levels, {3, a.first, a.second, b.first, b.second, ramp_levels});
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
				tiles[lm.next].preds.push_back(prio);
			}
		}
		equal(var[prio], var[beside], {4, prio.first, prio.second, beside.first, beside.second, 0});
	}
	for (auto &[tile, lt] : tiles) {
		std::ranges::sort(lt.preds);
		lt.preds.erase(std::unique(lt.preds.begin(), lt.preds.end()), lt.preds.end());
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
	/* Track corners within Manhattan distance d differ by at most d (the terraform slope limit; the propagation moves anything steeper). */
	constexpr int SLOPE_RANGE = 2;
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

	/* The choice of tile i at the levels lv: flat, or the ramp its exit level makes. */
	auto choice_at = [&](size_t i, const std::vector<int> &lv) {
		const LevelTile &lt = tiles[order_tiles[i]];
		int kind = 0;
		if (lt.rampable && lt.has_next) {
			const int d = lv[var[lt.next]] - lv[i];
			kind = d > 0 ? 1 : d < 0 ? 2 : 0;
		}
		return Evaluate(c, lt.visit, kind, lv[i]);
	};
	constexpr Money NO_CHOICE = 1000000000000LL;
	/* The cost of the tiles affected at the levels lv: their choices, and the corner mismatch against every adjacent track tile. */
	auto local_cost = [&](const std::set<size_t> &affected, const std::vector<int> &lv) {
		Money total = 0;
		std::map<size_t, std::optional<Choice>> memo;
		auto get = [&](size_t i) -> const std::optional<Choice> & {
			auto it = memo.find(i);
			if (it == memo.end()) it = memo.emplace(i, choice_at(i, lv)).first;
			return it->second;
		};
		for (size_t i : affected) {
			const std::optional<Choice> &mine = get(i);
			total += mine.has_value() ? mine->cost : NO_CHOICE;
			if (!mine.has_value()) continue;
			const SnapshotTile &t = order_tiles[i];
			for (int dx = -1; dx <= 1; dx++) {
				for (int dy = -1; dy <= 1; dy++) {
					const auto f = var.find({t.first + dx, t.second + dy});
					if ((dx == 0 && dy == 0) || f == var.end()) continue;
					const size_t j = f->second;
					if (affected.contains(j) && j < i) continue; // Counted from j.
					const std::optional<Choice> &other = get(j);
					if (!other.has_value()) continue;
					for (int ci = 0; ci < 4; ci++) {
						for (int cj = 0; cj < 4; cj++) {
							if (f->first.first + CORNER_OFFSET[cj].first != t.first + CORNER_OFFSET[ci].first || f->first.second + CORNER_OFFSET[cj].second != t.second + CORNER_OFFSET[ci].second) continue;
							total += c.terrain.terraform_cost * std::abs(mine->corners[ci] - other->corners[cj]);
						}
					}
				}
			}
		}
		return total;
	};

	/* Coordinate descent on the groups: each in turn takes the level in its feasible range (the rest held at their levels) that costs least. */
	constexpr int MAX_ROUNDS = 20;
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
			std::set<size_t> affected(member_set);
			for (size_t m : members) {
				for (const SnapshotTile &p : tiles[order_tiles[m]].preds) affected.insert(var[p]);
			}
			const int current = level[members.front()];
			Money best_cost = local_cost(affected, level);
			int best = current;
			std::vector<int> trial = level;
			for (int l = lo + base; l <= up + base; l++) {
				if (l == current) continue;
				for (size_t m : members) trial[m] = l;
				const Money cost_here = local_cost(affected, trial);
				if (cost_here < best_cost) {
					best_cost = cost_here;
					best = l;
				}
			}
			if (best != current) {
				for (size_t m : members) level[m] = best;
				moved = true;
			}
		}
		if (!moved) break;
	}

	for (size_t i = 0; i < n; i++) {
		const std::optional<Choice> choice = choice_at(i, level);
		if (!choice.has_value()) return fmt::format("no choice for the tile {}", name(order_tiles[i]));
		const Visit &v = tiles[order_tiles[i]].visit;
		chosen[order_tiles[i]] = {*choice, v.edge, v.copy};
	}
	return "";
}

} // namespace

bool SolveJunctionLevels(const JunctionTerrain &terrain, const JunctionFacts &facts, TileIndexDiffC offset, int ramp_levels, int train_length, const JunctionTrackPlacement &placement, const JunctionLayout &layout, const std::vector<JunctionTrackRoute> &routes, const std::vector<std::pair<TileIndexDiffC, TileIndexDiffC>> &prio_beside, JunctionTerraform &plan, Money &cost, std::vector<std::pair<TileIndexDiffC, int8_t>> &levels, std::string *why)
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
	for (const JunctionBridge &b : layout.bridges) {
		heads.insert(world(b.start));
		heads.insert(world(b.end));
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
	const std::string levels_why = SolveTileLevels(ctx, route_visits, heads, fixed_tiles, ties, layout, offset, ramp_levels, train_length, chosen);
	if (!levels_why.empty()) return fail(levels_why);

	/* The plan: every corner a tile demands, merged over the tiles; two tiles that demand different heights of one corner conflict. A tile on a foundation takes any corner height from its ground up to its level. */
	std::map<MapCorner, std::pair<int, SnapshotTile>> demanded;
	std::set<MapCorner> fixed_corners;
	JunctionTerraform result;
	for (const auto &[tile, item] : chosen) {
		const JunctionTerrainTile *ground = terrain.At(tile.first, tile.second);
		const std::array<int, 4> current = JunctionTileCorners(*ground);
		for (int i = 0; i < 4; i++) {
			const MapCorner corner{tile.first + CORNER_OFFSET[i].first, tile.second + CORNER_OFFSET[i].second};
			fixed_corners.insert(corner);
			if (item.choice.foundation) continue;
			const auto [found, inserted] = demanded.emplace(corner, std::make_pair(item.choice.corners[i], tile));
			if (!inserted && found->second.first != item.choice.corners[i]) {
				const Chosen &other = chosen.at(found->second.second);
				return fail(fmt::format("terraform conflict: corner ({}, {}) is wanted at {} by tile ({}, {}) (edge {} copy {}) and at {} by tile ({}, {}) (edge {} copy {})", corner.first, corner.second, found->second.first, found->second.second.first, found->second.second.second, other.edge, other.copy, item.choice.corners[i], tile.first, tile.second, item.edge, item.copy));
			}
			if (item.choice.corners[i] != current[i]) result.corners[corner] = item.choice.corners[i];
		}
	}
	for (const auto &[tile, item] : chosen) {
		if (!item.choice.foundation) continue;
		const JunctionTerrainTile *ground = terrain.At(tile.first, tile.second);
		const std::array<int, 4> current = JunctionTileCorners(*ground);
		for (int i = 0; i < 4; i++) {
			const MapCorner corner{tile.first + CORNER_OFFSET[i].first, tile.second + CORNER_OFFSET[i].second};
			const auto found = demanded.find(corner);
			if (found == demanded.end()) continue;
			const int wanted = found->second.first;
			if (wanted < current[i] || wanted > ground->z_max) return fail(fmt::format("terraform conflict: corner ({}, {}) is wanted at {} by tile ({}, {}) but the foundation tile ({}, {}) takes {} to {}", corner.first, corner.second, wanted, found->second.second.first, found->second.second.second, tile.first, tile.second, current[i], ground->z_max));
			if (wanted != current[i]) result.corners[corner] = wanted;
		}
	}
	std::string propagate_why;
	if (!PropagateJunctionTerraform(terrain, result, &propagate_why, &fixed_corners)) return fail(propagate_why);

	/* Total: the plan, plus foundations and clearing of the track tiles it does not touch (the plan clears the tiles it moves). */
	std::set<SnapshotTile> dirty;
	for (const auto &[corner, height] : result.corners) {
		for (const auto &[dx, dy] : CORNER_OFFSET) dirty.emplace(corner.first - dx, corner.second - dy);
	}
	Money total = result.cost;
	for (const auto &[tile, item] : chosen) {
		if (!dirty.contains(tile)) total += item.choice.cost;
	}
	plan = std::move(result);
	cost = total;
	levels.clear();
	for (const auto &[tile, item] : chosen) levels.emplace_back(TileIndexDiffC{static_cast<int16_t>(tile.first - offset.x), static_cast<int16_t>(tile.second - offset.y)}, static_cast<int8_t>(item.choice.level));
	return true;
}
