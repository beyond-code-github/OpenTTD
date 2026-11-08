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

TEST_CASE("Junction train model kick-off")
{
	const JunctionTrainParams p = RefParams();
	JunctionTrainState s;
	for (int i = 0; i < 3 && s.speed == 0; i++) StepJunctionTrain(p, s);
	CHECK(s.speed > 0);
}

TEST_CASE("Junction train model hand-worked tick")
{
	/* Speed 100, mass 200, 1000 hp, air drag 50:
	 *  P = 1000 * 746 = 746000.
	 *  R = axle 10*200 = 2000 + rolling 200 * (15 * 612 / 512 = 17) = 3400 + air 14 * 50 * 100^2 / 1000 = 7000 = 12400.
	 *  F = P * 18 / (100 * 5) = 26856 (below max_te 200000).
	 *  accel = (26856 - 12400) / 800 = 18. subspeed 0 + 18 = 18, no overflow, speed stays 100.
	 *  Progress: 100 * 3 / 4 = 75, below one step (192), so saved. */
	const JunctionTrainParams p = RefParams();
	JunctionTrainState s;
	s.speed = 100;
	StepJunctionTrain(p, s);
	CHECK(s.speed == 100);
	CHECK(s.subspeed == 18);
	CHECK(s.progress == 75);
	CHECK(s.distance == 0);
	CHECK(s.ticks == 1);
}

TEST_CASE("Junction train model terminal speed")
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

TEST_CASE("Junction train model mass monotonic")
{
	const uint64_t ten_tiles = 10 * JunctionProgressPerStraightTile();
	const uint32_t t100 = JunctionTicksToCover(RefParams(100), ten_tiles);
	const uint32_t t200 = JunctionTicksToCover(RefParams(200), ten_tiles);
	const uint32_t t400 = JunctionTicksToCover(RefParams(400), ten_tiles);
	CHECK(t400 > t200);
	CHECK(t200 > t100);
}

TEST_CASE("Junction train model piece lengths")
{
	CHECK(JunctionProgressPerStraightTile() == 3072);
	/* 8 steps per corner piece (vehicle.cpp:3366-3371), 256 each. */
	CHECK(JunctionProgressPerCornerPiece() == 8 * 256);
}
