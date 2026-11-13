/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_facts.cpp Test functionality from junction_facts. */

#include "../../../stdafx.h"

#include "../../../3rdparty/catch2/catch.hpp"

#include "../../../junction/terrain/junction_facts.h"
#include "../../../landscape.h"
#include "../../../rail.h"
#include "../../../slope_func.h"
#include "../../../track_func.h"

#include <algorithm>
#include <chrono>
#include <climits>
#include <random>

#include "../../../safeguards.h"

static const DiagDirection _x_exit = TrackdirToExitdir(Trackdir::X_SW);
static const DiagDirection _y_exit = TrackdirToExitdir(Trackdir::Y_SE);

static JunctionTerrainTile MakeTile(int8_t z_min, int8_t z_max, DiagDirection incline = DiagDirection::Invalid)
{
	JunctionTerrainTile t;
	t.z_min = z_min;
	t.z_max = z_max;
	t.incline = incline;
	t.slope = incline != DiagDirection::Invalid ? InclinedSlope(incline) : (z_min != z_max ? SLOPE_N : SLOPE_FLAT);
	return t;
}

static JunctionTerrain MakeRow(const std::vector<JunctionTerrainTile> &tiles)
{
	JunctionTerrain terrain;
	terrain.origin = {0, 0};
	terrain.width = static_cast<int>(tiles.size());
	terrain.height = 1;
	terrain.tiles = tiles;
	return terrain;
}

TEST_CASE("facts: a flat tile, a ramp and a foundation", "[junction][facts]")
{
	JunctionTerrain terrain = MakeRow({MakeTile(1, 1), MakeTile(1, 2, _x_exit), MakeTile(2, 2), MakeTile(1, 2, _y_exit)});
	{
		JunctionFacts facts(terrain);
		const int expect[4][2] = {{8, 8}, {8, 16}, {16, 16}, {16, 16}};
		for (int x = 0; x < 4; x++) {
			int entry = -1, exit = -1;
			REQUIRE(facts.Straight(x, 0, Axis::X, entry, exit));
			CHECK(entry == expect[x][0]);
			CHECK(exit == expect[x][1]);
		}
		CHECK(facts.StraightRun(Axis::X, 0, 0, 3));
		CHECK(facts.StraightRun(Axis::X, 0, 3, 0));
	}
	terrain.tiles[2] = MakeTile(3, 3);
	JunctionFacts facts(terrain);
	CHECK_FALSE(facts.StraightRun(Axis::X, 0, 0, 3));
	CHECK(facts.StraightRun(Axis::X, 0, 2, 3) == false);
	CHECK(facts.StraightRun(Axis::X, 0, 0, 1));
}

TEST_CASE("facts: switches and halves are flat or foundation only", "[junction][facts]")
{
	JunctionTerrain terrain = MakeRow({MakeTile(1, 2, _x_exit), MakeTile(1, 1), MakeTile(1, 2, _y_exit)});
	terrain.build_on_slopes = false; // With build_on_slopes the ramp tile would take a foundation like any slope.
	JunctionFacts facts(terrain);
	int entry, exit;
	CHECK(facts.Straight(0, 0, Axis::X, entry, exit));
	CHECK_FALSE(facts.Switch(0, 0));
	CHECK_FALSE(facts.Half(0, 0));
	CHECK(facts.Switch(1, 0));
	CHECK(facts.Half(1, 0));
	CHECK_FALSE(facts.Switch(2, 0));
	CHECK_FALSE(facts.Half(2, 0));
	terrain.build_on_slopes = true;
	const JunctionFacts with_foundation(terrain);
	CHECK(with_foundation.Switch(0, 0));
	CHECK(with_foundation.Half(2, 0));
	CHECK(with_foundation.Straight(0, 0, Axis::X, entry, exit));
	CHECK(entry == 8);
	CHECK(exit == 16);
}

TEST_CASE("facts: blocked tiles are bad and steep tiles follow the game", "[junction][facts]")
{
	std::vector<JunctionTerrainTile> tiles(12, MakeTile(0, 0));
	tiles[5].cls = JunctionTileClass::Blocked;
	tiles[9] = MakeTile(0, 2, GetInclinedSlopeDirection(SLOPE_STEEP_N));
	tiles[9].steep = true;
	tiles[9].slope = SLOPE_STEEP_N;
	JunctionTerrain terrain = MakeRow(tiles);
	const Foundation steep_foundation = GetRailFoundation(SLOPE_STEEP_N, TrackBits{Track::X});
	const bool steep_fits = steep_foundation == Foundation::None || (steep_foundation != Foundation::Invalid && terrain.build_on_slopes);
	JunctionFacts facts(terrain);
	CHECK(facts.NextBad(JunctionFactKind::Straight, Axis::X, 0, 0) == 5);
	CHECK(facts.NextBad(JunctionFactKind::Straight, Axis::X, 0, 5) == 5);
	CHECK(facts.NextBad(JunctionFactKind::Straight, Axis::X, 0, 6) == (steep_fits ? INT_MAX : 9));
	CHECK(facts.NextBad(JunctionFactKind::Switch, Axis::X, 0, 10) == INT_MAX);
	CHECK(facts.PrevBad(JunctionFactKind::Half, Axis::X, 0, 11) == 9);
	CHECK(facts.PrevBad(JunctionFactKind::Half, Axis::X, 0, 8) == 5);
	CHECK(facts.PrevBad(JunctionFactKind::Straight, Axis::X, 0, 4) == INT_MIN);
	CHECK(facts.NextBad(JunctionFactKind::Straight, Axis::X, 3, 0) == 0);
}

TEST_CASE("a span fact is false only on Blocked tiles and outside the snapshot", "[junction][facts]")
{
	std::vector<JunctionTerrainTile> tiles(4, MakeTile(0, 0));
	tiles[1].cls = JunctionTileClass::Water;
	tiles[2].cls = JunctionTileClass::Spannable;
	tiles[3].cls = JunctionTileClass::Blocked;
	const JunctionFacts facts(MakeRow(tiles));
	CHECK(facts.Spannable(0, 0));
	CHECK(facts.Spannable(1, 0));
	CHECK(facts.Spannable(2, 0));
	CHECK(!facts.Spannable(3, 0));
	CHECK(!facts.Spannable(-1, 0));
	CHECK(!facts.Spannable(4, 0));
	CHECK(!facts.Spannable(0, 1));
	CHECK(!facts.Spannable(0, -1));
}

static JunctionTerrain RandomTerrain(std::minstd_rand &rng, int width, int height)
{
	JunctionTerrain terrain;
	terrain.origin = {-3, 2};
	terrain.width = width;
	terrain.height = height;
	terrain.build_on_slopes = rng() % 2 == 0;
	for (int i = 0; i < width * height; i++) {
		JunctionTerrainTile t;
		t.cls = rng() % 100 < 80 ? JunctionTileClass::Buildable : static_cast<JunctionTileClass>(1 + rng() % 3);
		t.steep = rng() % 100 < 5;
		t.z_min = static_cast<int8_t>(rng() % 4);
		t.z_max = static_cast<int8_t>(t.z_min + rng() % 2);
		if (t.z_min != t.z_max && rng() % 2 == 0) t.incline = static_cast<DiagDirection>(rng() % 4);
		t.slope = t.incline != DiagDirection::Invalid ? InclinedSlope(t.incline) : (t.z_min != t.z_max ? SLOPE_N : SLOPE_FLAT);
		terrain.tiles.push_back(t);
	}
	return terrain;
}

TEST_CASE("facts: queries agree with walking", "[junction][facts]")
{
	std::minstd_rand rng(1);
	for (int round = 0; round < 200; round++) {
		const JunctionTerrain terrain = RandomTerrain(rng, 16, 8);
		const JunctionFacts facts(terrain);
		/* The rules, restated from the terrain. */
		for (int y = terrain.origin.y - 1; y <= terrain.origin.y + terrain.height; y++) {
			for (int x = terrain.origin.x - 1; x <= terrain.origin.x + terrain.width; x++) {
				const JunctionTerrainTile *t = terrain.At(x, y);
				const bool ok = t != nullptr && t->cls == JunctionTileClass::Buildable && !t->steep;
				const bool level = ok && terrain.UsableAt(x, y, t->z_max);
				CHECK(facts.Switch(x, y) == level);
				CHECK(facts.Half(x, y) == level);
				for (Axis axis : {Axis::X, Axis::Y}) {
					const DiagDirection out = axis == Axis::X ? _x_exit : _y_exit;
					const TrackBits bits{axis == Axis::X ? Track::X : Track::Y};
					int entry = -1, exit = -1;
					const Foundation foundation = t != nullptr ? GetRailFoundation(t->slope, bits) : Foundation::Invalid;
					const bool expect = t != nullptr && t->cls == JunctionTileClass::Buildable && foundation != Foundation::Invalid && (foundation == Foundation::None || terrain.build_on_slopes);
					REQUIRE(facts.Straight(x, y, axis, entry, exit) == expect);
					if (expect) {
						const auto edge_z = [&](int px, int py) {
							Slope s = t->slope;
							const int lift = static_cast<int>(ApplyPixelFoundationToSlope(foundation, s));
							return t->z_min * static_cast<int>(TILE_HEIGHT) + lift + static_cast<int>(GetPartialPixelZ(px, py, s));
						};
						/* Edge midpoints NE (0, 8), SE (8, 15), SW (15, 8), NW (8, 0); the entry edge is opposite the exit. */
						const std::array<std::pair<int, int>, 4> mid = {{{0, 8}, {8, 15}, {15, 8}, {8, 0}}};
						const auto [ox, oy] = mid[to_underlying(out)];
						const auto [ix, iy] = mid[to_underlying(ReverseDiagDir(out))];
						CHECK(entry == edge_z(ix, iy));
						CHECK(exit == edge_z(ox, oy));
					}
				}
			}
		}
		for (int q = 0; q < 1000; q++) {
			const Axis axis = rng() % 2 == 0 ? Axis::X : Axis::Y;
			const int line = static_cast<int>(rng() % 12) - 2 + (axis == Axis::X ? 2 : -3);
			const int a = static_cast<int>(rng() % 22) - 5 + (axis == Axis::X ? -3 : 2);
			const int b = static_cast<int>(rng() % 22) - 5 + (axis == Axis::X ? -3 : 2);
			auto at = [&](int pos, int &entry, int &exit) {
				return axis == Axis::X ? facts.Straight(pos, line, axis, entry, exit) : facts.Straight(line, pos, axis, entry, exit);
			};
			auto good = [&](JunctionFactKind kind, int pos) {
				int entry, exit;
				switch (kind) {
					case JunctionFactKind::Straight: return at(pos, entry, exit);
					case JunctionFactKind::Switch: return axis == Axis::X ? facts.Switch(pos, line) : facts.Switch(line, pos);
					default: return axis == Axis::X ? facts.Half(pos, line) : facts.Half(line, pos);
				}
			};

			bool run = true;
			int prev_exit = 0;
			for (int pos = std::min(a, b); pos <= std::max(a, b) && run; pos++) {
				int entry, exit;
				if (!at(pos, entry, exit) || (pos > std::min(a, b) && prev_exit != entry)) run = false;
				prev_exit = exit;
			}
			CHECK(facts.StraightRun(axis, line, a, b) == run);

			/* Positions outside the grid are bad themselves; a walk that leaves the grid without finding a bad tile finds none. */
			const int lo = axis == Axis::X ? terrain.origin.x : terrain.origin.y;
			const int hi = lo + (axis == Axis::X ? terrain.width : terrain.height) - 1;
			for (JunctionFactKind kind : {JunctionFactKind::Straight, JunctionFactKind::Switch, JunctionFactKind::Half}) {
				const bool in_grid = good(kind, a) || (a >= lo && a <= hi && (axis == Axis::X ? (line >= terrain.origin.y && line < terrain.origin.y + terrain.height) : (line >= terrain.origin.x && line < terrain.origin.x + terrain.width)));
				int next = a;
				while (next <= hi && good(kind, next)) next++;
				if (next > hi && in_grid) next = INT_MAX;
				CHECK(facts.NextBad(kind, axis, line, a) == next);
				int prev = a;
				while (prev >= lo && good(kind, prev)) prev--;
				if (prev < lo && in_grid) prev = INT_MIN;
				CHECK(facts.PrevBad(kind, axis, line, a) == prev);
			}
		}
	}
}

TEST_CASE("facts: a 128x128 snapshot builds fast", "[junction][facts]")
{
	std::minstd_rand rng(2);
	const JunctionTerrain terrain = RandomTerrain(rng, 128, 128);
	const auto start = std::chrono::steady_clock::now();
	const JunctionFacts facts(terrain);
	const auto us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
	WARN("128x128 build: " << us << " us");
	CHECK(facts.NextBad(JunctionFactKind::Switch, Axis::X, 2, -3) >= -3);
}

/** The slopes a tile can have, steep ones included: every non-flat one with its heights, plus the flat one. */
static const std::vector<Slope> _tile_slopes = {SLOPE_FLAT, SLOPE_W, SLOPE_S, SLOPE_SW, SLOPE_E, SLOPE_EW, SLOPE_SE, SLOPE_WSE, SLOPE_N, SLOPE_NW, SLOPE_NS, SLOPE_NWS, SLOPE_NE, SLOPE_ENW, SLOPE_SEN, SLOPE_STEEP_W, SLOPE_STEEP_S, SLOPE_STEEP_E, SLOPE_STEEP_N};

/** A one-tile terrain at (0, 0) of the given slope, lowest corner at 1. */
static JunctionTerrain MakeSlopeTerrain(Slope slope, bool build_on_slopes)
{
	JunctionTerrainTile t;
	t.z_min = 1;
	t.z_max = static_cast<int8_t>(1 + (slope == SLOPE_FLAT ? 0 : IsSteepSlope(slope) ? 2 : 1));
	t.steep = IsSteepSlope(slope);
	t.incline = GetInclinedSlopeDirection(slope);
	t.slope = slope;
	JunctionTerrain terrain = MakeRow({t});
	terrain.build_on_slopes = build_on_slopes;
	return terrain;
}

TEST_CASE("Fits follows GetRailFoundation", "[junction][facts]")
{
	for (const bool build_on_slopes : {true, false}) {
		for (const Slope slope : _tile_slopes) {
			const JunctionFacts facts(MakeSlopeTerrain(slope, build_on_slopes));
			for (uint8_t raw = 0; raw < 64; raw++) {
				const TrackBits bits{TrackBits::BaseType(raw)};
				const Foundation foundation = GetRailFoundation(slope, bits);
				INFO("slope " << static_cast<int>(slope) << " bits " << static_cast<int>(raw) << " build_on_slopes " << build_on_slopes);
				CHECK(facts.Fits(0, 0, bits) == (build_on_slopes ? foundation != Foundation::Invalid : foundation == Foundation::None));
			}
		}
	}
	/* Off the grid and not buildable never fit. */
	JunctionTerrain terrain = MakeSlopeTerrain(SLOPE_FLAT, true);
	terrain.tiles[0].cls = JunctionTileClass::Blocked;
	CHECK(!JunctionFacts(terrain).Fits(0, 0, TrackBits{Track::X}));
	CHECK(!JunctionFacts(MakeSlopeTerrain(SLOPE_FLAT, true)).Fits(1, 0, TrackBits{Track::X}));
}

TEST_CASE("the canonical slope rotates with the incline", "[junction][facts]")
{
	for (const Slope slope : {SLOPE_NE, SLOPE_SE, SLOPE_SW, SLOPE_NW}) {
		for (uint8_t rotation = 0; rotation < 4; rotation++) {
			for (const bool mirrored : {false, true}) {
				CHECK(JunctionCanonicalSlope(slope, rotation, mirrored) == InclinedSlope(JunctionCanonicalIncline(slope, rotation, mirrored)));
			}
		}
	}
}

TEST_CASE("Straight agrees with EdgeZ for a lone straight", "[junction][facts]")
{
	for (const Slope slope : _tile_slopes) {
		const JunctionFacts facts(MakeSlopeTerrain(slope, true));
		for (const Axis axis : {Axis::X, Axis::Y}) {
			const TrackBits bits{axis == Axis::X ? Track::X : Track::Y};
			const Trackdir travel = axis == Axis::X ? Trackdir::X_SW : Trackdir::Y_SE;
			const DiagDirection out = TrackdirToExitdir(travel);
			int entry = 0, exit = 0;
			const bool straight = facts.Straight(0, 0, axis, entry, exit);
			INFO("slope " << static_cast<int>(slope) << " axis " << static_cast<int>(axis));
			CHECK(straight == facts.Fits(0, 0, bits));
			if (straight && facts.Fits(0, 0, bits)) {
				CHECK(entry == facts.EdgeZ(0, 0, bits, ReverseDiagDir(out)));
				CHECK(exit == facts.EdgeZ(0, 0, bits, out));
			}
		}
	}
}

TEST_CASE("facts: Joins", "[junction][facts]")
{
	/* A row of flat tiles with a step of one level between tiles 2 and 3: they are straight-ok but do not meet. */
	const JunctionFacts facts(MakeRow({MakeTile(1, 1), MakeTile(1, 1), MakeTile(1, 1), MakeTile(2, 2), MakeTile(2, 2)}));
	int entry, exit;
	for (int x = 0; x < 5; x++) CHECK(facts.Straight(x, 0, Axis::X, entry, exit));
	CHECK(facts.Joins(Axis::X, 0, 0));
	CHECK(facts.Joins(Axis::X, 0, 1));
	CHECK_FALSE(facts.Joins(Axis::X, 0, 2));
	CHECK(facts.Joins(Axis::X, 0, 3));
	CHECK_FALSE(facts.Joins(Axis::X, 0, 4)); // The last tile has no neighbour.
}
