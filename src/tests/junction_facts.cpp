/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_facts.cpp Test functionality from junction_facts. */

#include "../stdafx.h"

#include "../3rdparty/catch2/catch.hpp"

#include "../junction_facts.h"
#include "../track_func.h"

#include <algorithm>
#include <chrono>
#include <climits>
#include <random>

#include "../safeguards.h"

static const DiagDirection _x_exit = TrackdirToExitdir(Trackdir::X_SW);
static const DiagDirection _y_exit = TrackdirToExitdir(Trackdir::Y_SE);

static JunctionTerrainTile MakeTile(int8_t z_min, int8_t z_max, DiagDirection incline = DiagDirection::Invalid)
{
	JunctionTerrainTile t;
	t.z_min = z_min;
	t.z_max = z_max;
	t.incline = incline;
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
		const int8_t expect[4][2] = {{1, 1}, {1, 2}, {2, 2}, {2, 2}};
		for (int x = 0; x < 4; x++) {
			int8_t entry = -1, exit = -1;
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
	int8_t entry, exit;
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
	CHECK(entry == 1);
	CHECK(exit == 2);
}

TEST_CASE("facts: blocked and steep tiles are bad", "[junction][facts]")
{
	std::vector<JunctionTerrainTile> tiles(12, MakeTile(0, 0));
	tiles[5].cls = JunctionTileClass::Blocked;
	tiles[9].steep = true;
	JunctionFacts facts(MakeRow(tiles));
	CHECK(facts.NextBad(JunctionFactKind::Straight, Axis::X, 0, 0) == 5);
	CHECK(facts.NextBad(JunctionFactKind::Straight, Axis::X, 0, 5) == 5);
	CHECK(facts.NextBad(JunctionFactKind::Straight, Axis::X, 0, 6) == 9);
	CHECK(facts.NextBad(JunctionFactKind::Switch, Axis::X, 0, 10) == INT_MAX);
	CHECK(facts.PrevBad(JunctionFactKind::Half, Axis::X, 0, 11) == 9);
	CHECK(facts.PrevBad(JunctionFactKind::Half, Axis::X, 0, 8) == 5);
	CHECK(facts.PrevBad(JunctionFactKind::Straight, Axis::X, 0, 4) == INT_MIN);
	CHECK(facts.NextBad(JunctionFactKind::Straight, Axis::X, 3, 0) == 0);
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
					int8_t entry = -1, exit = -1;
					bool expect = level;
					int8_t want_entry = ok ? t->z_max : 0, want_exit = want_entry;
					if (ok && t->incline == out) {
						expect = true;
						want_entry = t->z_min;
						want_exit = t->z_min + 1;
					} else if (ok && t->incline == ReverseDiagDir(out)) {
						expect = true;
						want_exit = t->z_max - 1;
					}
					REQUIRE(facts.Straight(x, y, axis, entry, exit) == expect);
					if (expect) {
						CHECK(entry == want_entry);
						CHECK(exit == want_exit);
					}
				}
			}
		}
		for (int q = 0; q < 1000; q++) {
			const Axis axis = rng() % 2 == 0 ? Axis::X : Axis::Y;
			const int line = static_cast<int>(rng() % 12) - 2 + (axis == Axis::X ? 2 : -3);
			const int a = static_cast<int>(rng() % 22) - 5 + (axis == Axis::X ? -3 : 2);
			const int b = static_cast<int>(rng() % 22) - 5 + (axis == Axis::X ? -3 : 2);
			auto at = [&](int pos, int8_t &entry, int8_t &exit) {
				return axis == Axis::X ? facts.Straight(pos, line, axis, entry, exit) : facts.Straight(line, pos, axis, entry, exit);
			};
			auto good = [&](JunctionFactKind kind, int pos) {
				int8_t entry, exit;
				switch (kind) {
					case JunctionFactKind::Straight: return at(pos, entry, exit);
					case JunctionFactKind::Switch: return axis == Axis::X ? facts.Switch(pos, line) : facts.Switch(line, pos);
					default: return axis == Axis::X ? facts.Half(pos, line) : facts.Half(line, pos);
				}
			};

			bool run = true;
			int8_t prev_exit = 0;
			for (int pos = std::min(a, b); pos <= std::max(a, b) && run; pos++) {
				int8_t entry, exit;
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
