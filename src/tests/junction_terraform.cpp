/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_terraform.cpp Test functionality from junction_terraform. */

#include "../stdafx.h"

#include "../3rdparty/catch2/catch.hpp"

#include "../bridge.h"
#include "../junction_facts.h"
#include "../junction_levels.h"
#include "../junction_nodes.h"
#include "../junction_topology.h"
#include "../rail.h"
#include "../track_func.h"
#include "../junction_terraform.h"
#include "../landscape.h"
#include "../slope_func.h"

#include <algorithm>
#include <cstdlib>
#include <map>
#include <set>
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

TEST_CASE("terraform: a fixed corner is not moved by a propagation", "[junction][terraform]")
{
	/* Raising corner (3,3) to 5 pushes its neighbours to 4 and theirs to 3 on flat 3 ground. A neighbour that is fixed makes the plan fail. */
	const JunctionTerrain flat = MakeTerrainFromCorners(8, 8, [](int, int) { return 3; });
	JunctionTerraform plan;
	plan.corners[{3, 3}] = 5;
	std::string why;
	REQUIRE(PropagateJunctionTerraform(flat, plan, &why));
	CHECK(plan.corners.at({4, 3}) == 4);

	/* A fixed corner the propagation does not reach, or that already has the height it would get from the plan's targets, is fine. */
	const std::set<std::pair<int, int>> far{{7, 7}, {3, 3}};
	JunctionTerraform ok;
	ok.corners[{3, 3}] = 5;
	CHECK(PropagateJunctionTerraform(flat, ok, &why, &far));

	/* A fixed neighbour (at its current height 3) would be moved to 4. */
	const std::set<std::pair<int, int>> near_corner{{4, 3}};
	JunctionTerraform conflict;
	conflict.corners[{3, 3}] = 5;
	CHECK_FALSE(PropagateJunctionTerraform(flat, conflict, &why, &near_corner));
	CHECK(why.find("terraform conflict") != std::string::npos);
	CHECK(why.find("corner (4, 3)") != std::string::npos);
	CHECK(why.find("tile (") != std::string::npos);
}

/** The level solve on a snapshot where the layout is one route through the tiles, each a trackdir along its first track. */
static bool SolveTestLevels(const JunctionTerrain &terrain, const std::vector<std::pair<TileIndexDiffC, TrackBits>> &tiles, JunctionLayout &layout, std::vector<JunctionTrackRoute> &routes, JunctionTerraform &plan, Money &cost, std::string &why)
{
	JunctionTrackRoute route{};
	for (const auto &[offset, bits] : tiles) {
		layout.tracks.push_back({offset, bits});
		route.pieces.push_back({offset, TrackToTrackdir(FindFirstTrack(bits)), 1, false});
		route.tags.push_back({0, JunctionTrackPieceTag::RUN, 0, 0});
	}
	routes = {route};
	const JunctionFacts facts(terrain);
	const JunctionTrackPlacement placement{};
	std::vector<std::pair<TileIndexDiffC, int8_t>> levels;
	return SolveJunctionLevels(terrain, facts, {0, 0}, 1, 1, placement, layout, routes, {}, plan, cost, levels, &why);
}

TEST_CASE("the level solve's foundation is the game's", "[junction][hoverplan]")
{
	/* An X run of three tiles at level 2: tile 1 has three corners raised (a leveled foundation), tile 2 one corner raised (the game builds the track inclined, not flat). */
	constexpr int LEVEL = 2;
	JunctionTerrain terrain = MakeTerrainFromCorners(3, 4, [](int cx, int cy) { return (cx == 3 && cy == 2) || (cx == 3 && cy == 3) || (cx == 2 && cy == 3) ? LEVEL - 1 : LEVEL; });
	terrain.foundation_cost = 5;
	REQUIRE(terrain.At(2, 2)->slope == SLOPE_N);
	REQUIRE(GetRailFoundation(terrain.At(2, 2)->slope, TrackBits{Track::X}) == Foundation::InclinedX);
	REQUIRE(GetRailFoundation(terrain.At(1, 2)->slope, TrackBits{Track::X}) == Foundation::Leveled);

	JunctionLayout layout;
	std::vector<JunctionTrackRoute> routes;
	JunctionTerraform plan;
	Money cost = 0;
	std::string why;
	const std::vector<std::pair<TileIndexDiffC, TrackBits>> tiles = {{{0, 2}, TrackBits{Track::X}}, {{1, 2}, TrackBits{Track::X}}, {{2, 2}, TrackBits{Track::X}}};
	if (SolveTestLevels(terrain, tiles, layout, routes, plan, cost, why)) {
		/* A planned tile is never the lone X on one raised corner (the game's InclinedX), and the heights facts accept the plan. */
		const JunctionTerrain after = ApplyJunctionTerraform(terrain, plan);
		for (int x = 0; x < 3; x++) {
			INFO("tile " << x);
			const Foundation foundation = GetRailFoundation(after.At(x, 2)->slope, TrackBits{Track::X});
			CHECK(foundation != Foundation::InclinedX);
			CHECK(foundation != Foundation::Invalid);
		}
		std::string heights;
		const bool ok = JunctionRoutesHeightsOk(routes, layout, {0, 0}, after, {0, 0}, &heights);
		INFO(heights);
		CHECK(ok);
	} else {
		INFO(why);
		CHECK(!why.empty());
	}
}

TEST_CASE("the level solve offers a leveled foundation exactly where the game builds one", "[junction][hoverplan]")
{
	std::vector<TrackBits> sets = {TrackBits{Track::X}, TrackBits{Track::Y}, TRACK_BIT_HORZ, TRACK_BIT_VERT};
	for (Track t : {Track::Upper, Track::Lower, Track::Left, Track::Right}) {
		sets.push_back(TrackBits{t});
		sets.push_back(TrackBits{Track::X} | TrackBits{t});
		sets.push_back(TrackBits{Track::Y} | TrackBits{t});
	}
	constexpr Money CLEAR = 10;
	constexpr Money FOUNDATION = 5;
	int leveled = 0;
	for (int s = 1; s < 15; s++) {
		const Slope slope = static_cast<Slope>(s);
		for (TrackBits bits : sets) {
			INFO("slope " << s << " bits " << static_cast<int>(bits.base()));
			JunctionTerrain terrain;
			terrain.origin = {0, 0};
			terrain.width = 1;
			terrain.height = 1;
			terrain.terraform_cost = TERRAFORM_COST;
			terrain.foundation_cost = FOUNDATION;
			terrain.tiles.resize(1);
			terrain.tiles[0].clear_cost = CLEAR;
			SetJunctionTileCorners(terrain.tiles[0], {(s & 1) ? 1 : 0, (s & 2) ? 1 : 0, (s & 4) ? 1 : 0, (s & 8) ? 1 : 0});
			REQUIRE(terrain.tiles[0].slope == slope);

			JunctionLayout layout;
			std::vector<JunctionTrackRoute> routes;
			JunctionTerraform plan;
			Money cost = 0;
			std::string why;
			const bool solved = SolveTestLevels(terrain, {{{0, 0}, bits}}, layout, routes, plan, cost, why);
			/* A 1-tile snapshot has no movable corner: the only plan that solves is the foundation. */
			const bool offered = solved && plan.corners.empty() && cost == CLEAR + FOUNDATION;
			const bool game = GetRailFoundation(slope, bits) == Foundation::Leveled;
			CHECK(offered == game);
			leveled += game ? 1 : 0;
		}
	}
	CHECK(leveled > 0);
}

/** The game's z_start of the head of an X bridge on a tile, or INT_MIN when the game refuses its slope (CheckBridgeSlope). */
static int GameHeadZ(const JunctionTerrainTile &tile, bool north)
{
	Slope slope = tile.slope;
	const int z = tile.z_min + static_cast<int>(ApplyFoundationToSlope(GetBridgeFoundation(slope, Axis::X), slope));
	return slope == SLOPE_FLAT || slope == (north ? SLOPE_NE : SLOPE_SW) ? z : INT_MIN;
}

/** The level solve of an X bridge from tile (1, 2) to tile (4, 2) on a 6 x 5 snapshot, with a track tile at each end; the heads' levels are returned. */
static bool SolveTestBridge(const JunctionTerrain &terrain, JunctionTerraform &plan, std::string &why, std::vector<std::pair<TileIndexDiffC, int8_t>> &levels)
{
	JunctionLayout layout;
	layout.tracks.push_back({{0, 2}, TrackBits{Track::X}});
	layout.tracks.push_back({{5, 2}, TrackBits{Track::X}});
	layout.bridges.push_back({{1, 2}, {4, 2}});
	JunctionTrackRoute route{};
	for (int x = 0; x < 6; x++) {
		route.pieces.push_back({{static_cast<int16_t>(x), 2}, Trackdir::X_SW, 1, x >= 1 && x <= 4});
		route.tags.push_back({0, JunctionTrackPieceTag::RUN, 0, 0});
	}
	const std::vector<JunctionTrackRoute> routes = {route};
	const JunctionFacts facts(terrain);
	const JunctionTrackPlacement placement{};
	Money cost = 0;
	return SolveJunctionLevels(terrain, facts, {0, 0}, 1, 1, placement, layout, routes, {}, plan, cost, levels, &why);
}

TEST_CASE("a bridge head may sit on the slope rising to its span", "[junction][hoverplan]")
{
	/* The ground is at 2 for the corners x <= 1 and at 1 beyond: the north head tile (1, 2) has its north-east edge a level above the span side (the game's valid slope for it), the south head is flat at 1. */
	JunctionTerrain terrain = MakeTerrainFromCorners(6, 5, [](int cx, int) { return cx <= 1 ? 2 : 1; });
	terrain.foundation_cost = 5;
	REQUIRE(terrain.At(1, 2)->slope == SLOPE_NE);
	REQUIRE(GameHeadZ(*terrain.At(1, 2), true) == 1);
	REQUIRE(GameHeadZ(*terrain.At(4, 2), false) == 1);

	JunctionTerraform plan;
	std::string why;
	std::vector<std::pair<TileIndexDiffC, int8_t>> levels;
	REQUIRE(SolveTestBridge(terrain, plan, why, levels));
	INFO(why);
	/* The head is not terraformed (no corner of tile (1, 2) moves) and its level is the top of the slope, the height of its outer edge. */
	for (const auto &[corner, height] : plan.corners) {
		const bool on_head = corner.first >= 1 && corner.first <= 2 && corner.second >= 2 && corner.second <= 3;
		CHECK_FALSE(on_head);
	}
	CHECK(plan.corners.empty());
	for (const auto &[tile, level] : levels) {
		if (tile.x == 1 && tile.y == 2) CHECK(level == 2);
		if (tile.x == 4 && tile.y == 2) CHECK(level == 1);
	}
}

TEST_CASE("the ground under a bridge stays at or below its heads", "[junction][hoverplan]")
{
	/* Flat ground at 1 with the first middle tile (2, 2) one level higher: a plan that leaves the heads at 1 would put the span under higher ground. */
	JunctionTerrain terrain = MakeTerrainFromCorners(6, 5, [](int cx, int cy) { return cx >= 2 && cx <= 3 && cy >= 2 && cy <= 3 ? 2 : 1; });
	terrain.foundation_cost = 5;
	JunctionTerraform plan;
	std::string why;
	std::vector<std::pair<TileIndexDiffC, int8_t>> levels;
	if (SolveTestBridge(terrain, plan, why, levels)) {
		const JunctionTerrain after = ApplyJunctionTerraform(terrain, plan);
		const int north = GameHeadZ(*after.At(1, 2), true);
		const int south = GameHeadZ(*after.At(4, 2), false);
		CHECK(north != INT_MIN);
		CHECK(north == south);
		for (int x = 2; x <= 3; x++) {
			INFO("middle tile " << x);
			CHECK(after.At(x, 2)->z_max <= north);
		}
	} else {
		INFO(why);
		CHECK(why.find("bridge") != std::string::npos);
	}
}
