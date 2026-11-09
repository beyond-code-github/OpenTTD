/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_train_model.cpp Test functionality from junction_train_model. */

#include "../stdafx.h"

#include "../3rdparty/catch2/catch.hpp"

#include "../junction_train_model.h"

#include "../safeguards.h"

static JunctionTrainParams RefParams(uint32_t mass = 200)
{
	JunctionTrainParams p;
	p.power_hp = 1000;
	p.max_te = 200000;
	p.mass_t = mass;
	p.air_drag = 50;
	p.max_speed = 160;
	return p;
}

TEST_CASE("Junction train model kick-off", "[junction]")
{
	const JunctionTrainParams p = RefParams();
	JunctionTrainState s;
	for (int i = 0; i < 3 && s.speed == 0; i++) StepJunctionTrain(p, s);
	CHECK(s.speed > 0);
}

TEST_CASE("Junction train model hand-worked tick", "[junction]")
{
	/* Speed 100, mass 200, 1000 hp, air drag 50:
	 *  P = 1000 * 746 = 746000.
	 *  R = axle 10*200 = 2000 + rolling 200 * (15 * 612 / 512 = 17) = 3400 + air 14 * 50 * 100^2 / 1000 = 7000 = 12400.
	 *  F = P * 18 / (100 * 5) = 26856 (below max_te 200000).
	 *  accel = (26856 - 12400) / 800 = 18. subspeed 0 + 18 = 18, no overflow, speed stays 100.
	 *  Progress: 100 * 3 / 4 = 75, below one step (192), so saved.
	 * One tick is two loco-handler passes (train_cmd.cpp:4213-4215), so the values below are the pass result applied twice: subspeed 36, progress 150. */
	const JunctionTrainParams p = RefParams();
	JunctionTrainState s;
	s.speed = 100;
	StepJunctionTrain(p, s);
	CHECK(s.speed == 100);
	CHECK(s.subspeed == 36);
	CHECK(s.progress == 150);
	CHECK(s.distance == 0);
	CHECK(s.ticks == 1);
}

TEST_CASE("Junction train model terminal speed", "[junction]")
{
	const JunctionTrainParams p = RefParams();
	JunctionTrainState s;
	uint16_t max_seen = 0;
	uint16_t before = 0;
	for (int i = 0; i < 20000; i++) {
		if (i == 19000) before = s.speed;
		StepJunctionTrain(p, s);
		max_seen = std::max(max_seen, s.speed);
	}
	CHECK(max_seen <= p.max_speed);
	/* Air drag beats power below 160 here, so it settles where F ~ R, below the cap. */
	CHECK(s.speed < p.max_speed);
	CHECK(s.speed > 100);
	CHECK(s.speed == before);
}

TEST_CASE("Junction train model mass monotonic", "[junction]")
{
	const uint64_t ten_tiles = 10 * JunctionProgressPerStraightTile();
	const uint32_t t100 = JunctionTicksToCover(RefParams(100), ten_tiles);
	const uint32_t t200 = JunctionTicksToCover(RefParams(200), ten_tiles);
	const uint32_t t400 = JunctionTicksToCover(RefParams(400), ten_tiles);
	CHECK(t400 > t200);
	CHECK(t200 > t100);
}

TEST_CASE("Junction train model piece lengths", "[junction]")
{
	CHECK(JunctionProgressPerStraightTile() == 3072);
	/* 8 steps per corner piece (vehicle.cpp:3366-3371), 256 each. */
	CHECK(JunctionProgressPerCornerPiece() == 8 * 256);
}

TEST_CASE("Junction prio length mass monotonic", "[junction]")
{
	const uint32_t l100 = JunctionPrioLength(RefParams(100), 160, 4, 2);
	const uint32_t l200 = JunctionPrioLength(RefParams(200), 160, 4, 2);
	const uint32_t l400 = JunctionPrioLength(RefParams(400), 160, 4, 2);
	CHECK(l400 > l200);
	CHECK(l200 > l100);
}

TEST_CASE("Junction prio length design speed monotonic", "[junction]")
{
	const uint32_t l60 = JunctionPrioLength(RefParams(200), 60, 4, 2);
	const uint32_t l100 = JunctionPrioLength(RefParams(200), 100, 4, 2);
	const uint32_t l160 = JunctionPrioLength(RefParams(200), 160, 4, 2);
	CHECK(l160 > l100);
	CHECK(l100 > l60);
}

TEST_CASE("Junction prio length floor", "[junction]")
{
	JunctionTrainParams p = RefParams(50);
	p.power_hp = 100000;
	p.max_te = 10000000;
	const uint32_t l = JunctionPrioLength(p, 160, 4, 2);
	CHECK(l >= 6);
	CHECK(l <= 7);
}

TEST_CASE("Junction prio length hand-check", "[junction]")
{
	const uint32_t l = JunctionPrioLength(RefParams(), 160, 4, 2);
	WARN("JunctionPrioLength(ref, 160, tl 4, sd 2) = " << l);
	CHECK(l >= 6);
}

TEST_CASE("Junction prio length when the train can't reach the design speed", "[junction]")
{
	/* The reference train settles below 160 on flat track; ML trains of the same type can't go faster either (D101). */
	const JunctionTrainParams ref = RefParams();
	const uint16_t terminal = JunctionTerminalSpeed(ref);
	CHECK(terminal < 160);
	CHECK(terminal > 100);
	const uint32_t l = JunctionPrioLength(ref, 160, 4, 2);
	CHECK(l == JunctionPrioLength(ref, terminal, 4, 2));
	CHECK(l < 200);
}

TEST_CASE("StepJunctionTrain: one tick is two loco-handler passes", "[junction][train]")
{
	/* The D138 design train; the rig took 64 game ticks from 61 to 190 km/h (R2). */
	JunctionTrainParams p;
	p.power_hp = 14194;
	p.max_te = 1090000;
	p.mass_t = 280;
	p.air_drag = 25;
	p.max_speed = 196;
	JunctionTrainState s;
	s.speed = 61;
	s.subspeed = 0;
	uint32_t calls = 0;
	while (s.speed < 190 && calls < 1000) {
		StepJunctionTrain(p, s);
		calls++;
	}
	/* The rig samples once per tick, so the 190 km/h crossing can land on either side of the model's (R2). */
	CHECK(calls >= 63);
	CHECK(calls <= 65);
}
