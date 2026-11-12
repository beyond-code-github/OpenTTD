/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_terraform.cpp A terraform plan on a terrain snapshot for the junction generator (D218). */

#include "stdafx.h"

#include "junction_terraform.h"
#include "slope_func.h"

#include <algorithm>
#include <deque>
#include <optional>
#include <set>

#include "safeguards.h"

using JunctionCorner = std::pair<int, int>;

/** Offset of each tile corner (indexed W, S, E, N) from the tile's own coordinates, as the game's CmdTerraformLand names them. */
static constexpr std::pair<int, int> TILE_CORNER_OFFSET[4] = {{1, 0}, {1, 1}, {0, 1}, {0, 0}};

/**
 * The height of a corner from any snapshot tile that has it.
 * @param terrain The snapshot.
 * @param corner The corner.
 * @return The height, or nullopt when no snapshot tile touches the corner.
 */
static std::optional<int> JunctionCornerHeight(const JunctionTerrain &terrain, const JunctionCorner &corner)
{
	for (int i = 0; i < 4; i++) {
		const JunctionTerrainTile *tile = terrain.At(corner.first - TILE_CORNER_OFFSET[i].first, corner.second - TILE_CORNER_OFFSET[i].second);
		if (tile != nullptr) return JunctionTileCorners(*tile)[i];
	}
	return std::nullopt;
}

std::array<int, 4> JunctionTileCorners(const JunctionTerrainTile &tile)
{
	std::array<int, 4> corners;
	int missing = -1;
	for (int i = 0; i < 4; i++) {
		const bool raised = (tile.slope & (1 << i)) != 0;
		corners[i] = tile.z_min + (raised ? 1 : 0);
		if (!raised) missing = i;
	}
	/* A steep slope has three corners raised, the one opposite the missing corner by two levels. */
	if (tile.steep) corners[(missing + 2) % 4] = tile.z_min + 2;
	return corners;
}

void SetJunctionTileCorners(JunctionTerrainTile &tile, const std::array<int, 4> &corners)
{
	const int z_min = *std::min_element(corners.begin(), corners.end());
	const int z_max = *std::max_element(corners.begin(), corners.end());
	Slope slope = z_max > z_min + 1 ? SLOPE_STEEP : SLOPE_FLAT;
	for (int i = 0; i < 4; i++) {
		if (corners[i] > z_min) slope |= static_cast<Slope>(1 << i);
	}
	tile.z_min = static_cast<int8_t>(z_min);
	tile.z_max = static_cast<int8_t>(z_max);
	tile.steep = IsSteepSlope(slope);
	tile.slope = slope;
	tile.incline = GetInclinedSlopeDirection(slope);
}

/**
 * Whether the corner may move: all four tiles around it are in the snapshot and Buildable.
 * @param terrain The snapshot.
 * @param corner The corner.
 * @param why Receives the reason when it may not.
 * @return Whether it may move.
 */
static bool JunctionCornerMovable(const JunctionTerrain &terrain, const JunctionCorner &corner, std::string *why)
{
	static constexpr const char *CLASS_NAME[] = {"Buildable", "Water", "Rail or road", "Blocked"};
	for (int i = 0; i < 4; i++) {
		const int x = corner.first - TILE_CORNER_OFFSET[i].first;
		const int y = corner.second - TILE_CORNER_OFFSET[i].second;
		const JunctionTerrainTile *tile = terrain.At(x, y);
		if (tile == nullptr) {
			if (why != nullptr) *why = fmt::format("corner ({}, {}) touches tile ({}, {}) outside the snapshot", corner.first, corner.second, x, y);
			return false;
		}
		if (tile->cls != JunctionTileClass::Buildable) {
			if (why != nullptr) *why = fmt::format("corner ({}, {}) touches tile ({}, {}) which is {}", corner.first, corner.second, x, y, CLASS_NAME[to_underlying(tile->cls)]);
			return false;
		}
	}
	return true;
}

bool PropagateJunctionTerraform(const JunctionTerrain &terrain, JunctionTerraform &plan, std::string *why, const std::set<std::pair<int, int>> *fixed)
{
	std::map<JunctionCorner, int> heights;
	std::deque<JunctionCorner> work;
	for (const auto &[corner, height] : plan.corners) {
		if (!JunctionCornerMovable(terrain, corner, why)) return false;
		if (JunctionCornerHeight(terrain, corner) == height) continue;
		heights[corner] = height;
		work.push_back(corner);
	}

	static constexpr std::pair<int, int> STEP[4] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
	while (!work.empty()) {
		const JunctionCorner corner = work.front();
		work.pop_front();
		const int height = heights.at(corner);
		for (const auto &[dx, dy] : STEP) {
			const JunctionCorner next{corner.first + dx, corner.second + dy};
			const auto it = heights.find(next);
			const std::optional<int> other = it != heights.end() ? std::optional<int>(it->second) : JunctionCornerHeight(terrain, next);
			if (!other.has_value() || std::abs(height - *other) <= 1) continue;
			if (fixed != nullptr && fixed->contains(next)) {
				if (why != nullptr) *why = fmt::format("terraform conflict: corner ({}, {}) of tile ({}, {}) is fixed at {} and would move to {}", next.first, next.second, next.first, next.second, *other, *other > height ? height + 1 : height - 1);
				return false;
			}
			if (!JunctionCornerMovable(terrain, next, why)) return false;
			heights[next] = *other > height ? height + 1 : height - 1;
			work.push_back(next);
		}
	}

	JunctionTerraform result;
	std::set<std::pair<int, int>> tiles;
	int64_t moves = 0;
	for (const auto &[corner, height] : heights) {
		const int base = *JunctionCornerHeight(terrain, corner);
		if (base == height) continue;
		result.corners[corner] = height;
		moves += std::abs(height - base);
		for (const auto &[dx, dy] : TILE_CORNER_OFFSET) tiles.emplace(corner.first - dx, corner.second - dy);
	}
	result.cost = moves * terrain.terraform_cost;
	for (const auto &[x, y] : tiles) result.cost += terrain.At(x, y)->clear_cost;
	plan = std::move(result);
	return true;
}

JunctionTerrain ApplyJunctionTerraform(const JunctionTerrain &terrain, const JunctionTerraform &plan)
{
	JunctionTerrain result = terrain;
	std::set<std::pair<int, int>> tiles;
	for (const auto &[corner, height] : plan.corners) {
		for (const auto &[dx, dy] : TILE_CORNER_OFFSET) tiles.emplace(corner.first - dx, corner.second - dy);
	}
	for (const auto &[x, y] : tiles) {
		const JunctionTerrainTile *tile = terrain.At(x, y);
		if (tile == nullptr) continue;
		std::array<int, 4> corners = JunctionTileCorners(*tile);
		for (int i = 0; i < 4; i++) {
			const auto it = plan.corners.find({x + TILE_CORNER_OFFSET[i].first, y + TILE_CORNER_OFFSET[i].second});
			if (it != plan.corners.end()) corners[i] = it->second;
		}
		SetJunctionTileCorners(result.tiles[static_cast<size_t>(y - terrain.origin.y) * terrain.width + (x - terrain.origin.x)], corners);
	}
	return result;
}
