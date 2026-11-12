/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_terraform.cpp Test functionality from junction_terraform. */

#include "../stdafx.h"

#include "../3rdparty/catch2/catch.hpp"

#include "../junction_facts.h"
#include "../junction_terraform.h"
#include "../slope_func.h"

#include <algorithm>
#include <cstdlib>
#include <map>
#include <string>

#include "../safeguards.h"

static constexpr Money TERRAFORM_COST = 100;
static constexpr Money CLEAR_COST = 10;

/** A w x h snapshot whose corner (cx, cy) has height height_of(cx, cy), every tile Buildable. */
template <typename F>
static JunctionTerrain MakeTerrainFromCorners(int w, int h, F height_of)
{
	JunctionTerrain terrain;
	terrain.origin = {0, 0};
	terrain.width = w;
	terrain.height = h;
	terrain.terraform_cost = TERRAFORM_COST;
	terrain.tiles.resize(static_cast<size_t>(w) * h);
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			JunctionTerrainTile &tile = terrain.tiles[static_cast<size_t>(y) * w + x];
			tile.clear_cost = CLEAR_COST;
			SetJunctionTileCorners(tile, {height_of(x + 1, y), height_of(x + 1, y + 1), height_of(x, y + 1), height_of(x, y)});
		}
	}
	return terrain;
}

/** The game's recursion of TerraformTileHeight (terraform_cmd.cpp:102), on a corner map; returns the number of calls charged. */
static int GameTerraform(std::map<std::pair<int, int>, int> &heights, int x, int y, int height)
{
	heights[{x, y}] = height;
	int charged = 1;
	for (const auto &[dx, dy] : {std::pair{1, 0}, std::pair{0, 1}, std::pair{-1, 0}, std::pair{0, -1}}) {
		const int r = heights.at({x + dx, y + dy});
		int diff = height - r;
		if (std::abs(diff) > 1) {
			diff += (diff < 0 ? 1 : -1);
			charged += GameTerraform(heights, x + dx, y + dy, r + diff);
		}
	}
	return charged;
}

TEST_CASE("terraform: a ramp row cut flat makes a straight run buildable", "[junction][terraform]")
{
	/* A ridge across the run on row 2: corners (4,2) and (5,2) one level above the rest. Without build_on_slopes the sloped tiles 3 to 5 cannot take the run. */
	JunctionTerrain terrain = MakeTerrainFromCorners(9, 5, [](int cx, int cy) { return cy == 2 && (cx == 4 || cx == 5) ? 2 : 1; });
	terrain.build_on_slopes = false;
	CHECK_FALSE(JunctionFacts(terrain).StraightRun(Axis::X, 2, 1, 7));

	/* Target the run's tiles' corners flat at the run's level (1); the corners already there are not moves. */
	JunctionTerraform plan;
	for (int cx = 1; cx <= 8; cx++) {
		for (int cy = 2; cy <= 3; cy++) plan.corners[{cx, cy}] = 1;
	}
	std::string why;
	REQUIRE(PropagateJunctionTerraform(terrain, plan, &why));
	CHECK(plan.corners.size() == 2);

	const JunctionTerrain cut = ApplyJunctionTerraform(terrain, plan);
	CHECK(JunctionFacts(cut).StraightRun(Axis::X, 2, 1, 7));
	/* Two corners one level each, and the 3 x 2 tiles around them. */
	CHECK(plan.cost == 2 * TERRAFORM_COST + 6 * CLEAR_COST);
}

TEST_CASE("terraform: raising one corner by one on flat land changes only that corner", "[junction][terraform]")
{
	const JunctionTerrain terrain = MakeTerrainFromCorners(5, 5, [](int, int) { return 3; });
	JunctionTerraform plan;
	plan.corners[{2, 2}] = 4;
	REQUIRE(PropagateJunctionTerraform(terrain, plan, nullptr));
	CHECK(plan.corners.size() == 1);
	CHECK(plan.cost == TERRAFORM_COST + 4 * CLEAR_COST);

	const JunctionTerrain after = ApplyJunctionTerraform(terrain, plan);
	/* Corner (2,2) is tile (1,1)'s S, (2,1)'s E... as the tile at its offset: (1,1) S, (2,1) E, (1,2) W, (2,2) N. */
	CHECK(after.At(1, 1)->slope == SLOPE_S);
	CHECK(after.At(2, 1)->slope == SLOPE_E);
	CHECK(after.At(1, 2)->slope == SLOPE_W);
	CHECK(after.At(2, 2)->slope == SLOPE_N);
	for (const auto &[x, y] : {std::pair{1, 1}, std::pair{2, 1}, std::pair{1, 2}, std::pair{2, 2}}) {
		CHECK(after.At(x, y)->z_min == 3);
		CHECK(after.At(x, y)->z_max == 4);
		CHECK_FALSE(after.At(x, y)->steep);
	}
	CHECK(after.At(0, 0)->slope == SLOPE_FLAT);
	CHECK(after.At(3, 3)->z_max == 3);
}

TEST_CASE("terraform: raising a corner by two propagates one ring", "[junction][terraform]")
{
	const JunctionTerrain terrain = MakeTerrainFromCorners(7, 7, [](int, int) { return 3; });
	JunctionTerraform plan;
	plan.corners[{3, 3}] = 5;
	REQUIRE(PropagateJunctionTerraform(terrain, plan, nullptr));
	/* The target, and its four orthogonal neighbours raised to 4; the diagonals stay (they differ from the target by one step each way). */
	CHECK(plan.corners.size() == 5);
	CHECK(plan.corners.at({3, 3}) == 5);
	for (const auto &c : {std::pair{4, 3}, std::pair{2, 3}, std::pair{3, 4}, std::pair{3, 2}}) CHECK(plan.corners.at(c) == 4);

	/* The game's own recursion on the same corners, one level per command as the terraform tool does it, agrees corner for corner and in charge. */
	std::map<std::pair<int, int>, int> game;
	for (int cx = 0; cx <= 7; cx++) {
		for (int cy = 0; cy <= 7; cy++) game[{cx, cy}] = 3;
	}
	const int charged = GameTerraform(game, 3, 3, 4) + GameTerraform(game, 3, 3, 5);
	for (const auto &[corner, height] : game) {
		const auto it = plan.corners.find(corner);
		CHECK((it != plan.corners.end() ? it->second : 3) == height);
	}
	CHECK(charged == 6);

	/* Tiles: (3,3) has N at 5 and E, W at 4: steep (a corner two above the lowest), the opposite S staying 3. */
	const JunctionTerrain after = ApplyJunctionTerraform(terrain, plan);
	const JunctionTerrainTile &tile = *after.At(3, 3);
	CHECK(tile.z_min == 3);
	CHECK(tile.z_max == 5);
	CHECK(tile.steep);
	CHECK(tile.slope == SLOPE_STEEP_N);
	/* Tile (3,2) holds corner (3,3) as its E: E 5, N and S... corners (3,2)=4, (4,2)=3, (4,3)=4: steep too. */
	CHECK(after.At(3, 2)->slope == SLOPE_STEEP_E);
	/* Tile (4,4) touches the raised ring only at its N corner (4,4)=3: flat. */
	CHECK(after.At(4, 4)->slope == SLOPE_FLAT);
	CHECK(after.At(4, 3)->z_max == 4);
	CHECK_FALSE(after.At(4, 3)->steep);
	/* Six corner-level moves (the target twice), and the 12 tiles around the five moved corners. */
	CHECK(plan.cost == charged * TERRAFORM_COST + 12 * CLEAR_COST);
}

TEST_CASE("terraform: a plan touching a house, water, rail/road (Spannable) or the snapshot edge is refused", "[junction][terraform]")
{
	for (const JunctionTileClass cls : {JunctionTileClass::Blocked, JunctionTileClass::Water, JunctionTileClass::Spannable}) {
		JunctionTerrain terrain = MakeTerrainFromCorners(5, 5, [](int, int) { return 3; });
		terrain.tiles[2 * 5 + 3].cls = cls; // Tile (3,2).
		JunctionTerraform plan;
		plan.corners[{3, 3}] = 4; // Shared by tiles (2..3, 2..3), so by (3,2).
		std::string why;
		CHECK_FALSE(PropagateJunctionTerraform(terrain, plan, &why));
		CHECK(why.find("tile (3, 2)") != std::string::npos);
	}

	const JunctionTerrain flat = MakeTerrainFromCorners(5, 5, [](int, int) { return 3; });
	JunctionTerraform edge;
	edge.corners[{0, 2}] = 4;
	std::string why;
	CHECK_FALSE(PropagateJunctionTerraform(flat, edge, &why));
	CHECK(why.find("tile (-1, ") != std::string::npos);

	/* Propagation reaching a blocked tile fails too: a rise by two next to a house two corners away. */
	JunctionTerrain house = MakeTerrainFromCorners(7, 7, [](int, int) { return 3; });
	house.tiles[3 * 7 + 5].cls = JunctionTileClass::Blocked; // Tile (5,3) touches corners (5..6, 3..4).
	JunctionTerraform ripple;
	ripple.corners[{4, 3}] = 5; // Pushes corner (5,3) to 4.
	CHECK_FALSE(PropagateJunctionTerraform(house, ripple, &why));
	CHECK(why.find("tile (5, 3)") != std::string::npos);
}

TEST_CASE("terraform: corner heights round-trip", "[junction][terraform]")
{
	static const Slope slopes[] = {
		SLOPE_FLAT, SLOPE_W, SLOPE_S, SLOPE_E, SLOPE_N, SLOPE_NW, SLOPE_SW, SLOPE_SE, SLOPE_NE, SLOPE_EW, SLOPE_NS,
		SLOPE_NWS, SLOPE_WSE, SLOPE_SEN, SLOPE_ENW, SLOPE_STEEP_W, SLOPE_STEEP_S, SLOPE_STEEP_E, SLOPE_STEEP_N,
	};
	for (const Slope slope : slopes) {
		JunctionTerrainTile tile;
		tile.z_min = 2;
		tile.slope = slope;
		tile.steep = IsSteepSlope(slope);
		tile.z_max = static_cast<int8_t>(tile.z_min + (tile.steep ? 2 : (slope == SLOPE_FLAT ? 0 : 1)));
		tile.incline = GetInclinedSlopeDirection(slope);

		const std::array<int, 4> corners = JunctionTileCorners(tile);
		JunctionTerrainTile back;
		SetJunctionTileCorners(back, corners);
		CHECK(back.z_min == tile.z_min);
		CHECK(back.z_max == tile.z_max);
		CHECK(back.slope == tile.slope);
		CHECK(back.steep == tile.steep);
		CHECK(back.incline == tile.incline);
	}
}
