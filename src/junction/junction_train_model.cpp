/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_train_model.cpp Pure model of train acceleration and progress on flat straight track. */

#include "../stdafx.h"

#include "junction_train_model.h"
#include "../vehicle_base.h"

#include "../safeguards.h"

/** Movement steps in one straight (X/Y) tile: enter at sub-coordinate 15 (or 0) and leave after 16 one-pixel steps (vehicle.cpp:3366-3392, _vehicle_subcoord, e.g. NE/X enters at x = 15). */
static constexpr uint STEPS_PER_STRAIGHT_TILE = 16;

/** Movement steps in one corner piece (Upper/Lower/Left/Right): e.g. NE-entry Lower starts at (15, 8) moving E (x-1, y+1) and leaves the tile at y = 16, 8 steps (vehicle.cpp:3366-3371). */
static constexpr uint STEPS_PER_CORNER_PIECE = 8;

/** Give-up bound for JunctionTicksToCover. */
static constexpr uint32_t MAX_MODEL_TICKS = 10'000'000;

/** Tick bound for JunctionPrioLength. */
static constexpr uint32_t MAX_MODEL_TICKS_PRIO = 1'000'000;

/** One TrainLocoHandler pass (train_cmd.cpp:4119): UpdateSpeed, then advance. */
static void StepJunctionLocoHandler(const JunctionTrainParams &params, JunctionTrainState &state)
{
	/* GroundVehicle::GetAcceleration, AS_ACCEL, flat, non-maglev (ground_vehicle.cpp:105-168). */
	int64_t speed = state.speed; // ground_vehicle.cpp:110
	int64_t mass = std::max<uint32_t>(params.mass_t, 1); // ground_vehicle.cpp:113
	int64_t power = params.power_hp * 746ll; // ground_vehicle.cpp:120

	/* Axle resistance 10 * weight (ground_vehicle.cpp:94, 139), rolling friction (train.h:309-314),
	 * air drag with area 14 (train.h:274-278; ground_vehicle.cpp:143). No slope resistance on flat track. */
	int64_t resistance = 10 * mass;
	resistance += mass * (15 * (512 + state.speed) / 512);
	resistance += static_cast<int64_t>(14) * params.air_drag * speed * speed / 1000;

	const int max_te = params.max_te; // ground_vehicle.cpp:155
	int64_t force;
	if (speed > 0) {
		force = power * 18 / (speed * 5); // ground_vehicle.cpp:163
		if (force > max_te) force = max_te; // ground_vehicle.cpp:164
	} else {
		/* "Kickoff" acceleration (ground_vehicle.cpp:170-172). */
		force = std::min<int>(max_te, power);
		force = std::max(force, (mass * 8) + resistance);
	}

	int accel; // ground_vehicle.cpp:176-186
	if (force == resistance) {
		accel = 0;
	} else {
		accel = ClampTo<int32_t>((force - resistance) / (mass * 4));
		accel = force < resistance ? std::min(-1, accel) : std::max(1, accel);
	}

	/* GroundVehicle::DoUpdateSpeed with min_speed 2 while accelerating (ground_vehicle.hpp:373-398). */
	const int min_speed = 2;
	const int max_speed = params.max_speed;
	uint spd = state.subspeed + accel;
	state.subspeed = (uint8_t)spd;

	int tempmax = max_speed;
	if (state.speed > max_speed) {
		tempmax = std::max(state.speed - (state.speed / 10) - 1, max_speed);
	}

	state.speed = spd = std::max(std::min(state.speed + ((int)spd >> 8), tempmax), min_speed);

	/* Progress: GetAdvanceSpeed (vehicle_base.h:442), plus saved progress (ground_vehicle.hpp:392-397).
	 * Straight track: every step costs TILE_AXIAL_DISTANCE (vehicle_base.h:454-457). Whole steps are
	 * consumed as in the loop at train_cmd.cpp:4129-4160; the remainder is saved. */
	uint scaled_spd = Vehicle::GetAdvanceSpeed(spd) + state.progress;
	uint steps = scaled_spd / TILE_AXIAL_DISTANCE;
	state.progress = scaled_spd - steps * TILE_AXIAL_DISTANCE;
	state.distance += static_cast<uint64_t>(steps) * TILE_AXIAL_DISTANCE;
}

void StepJunctionTrain(const JunctionTrainParams &params, JunctionTrainState &state)
{
	/* Train::Tick runs TrainLocoHandler twice per game tick (train_cmd.cpp:4213-4215). */
	StepJunctionLocoHandler(params, state);
	StepJunctionLocoHandler(params, state);
	state.ticks++;
}

uint32_t JunctionTicksToCover(const JunctionTrainParams &params, uint64_t distance)
{
	JunctionTrainState state;
	while (state.distance < distance && state.ticks < MAX_MODEL_TICKS) StepJunctionTrain(params, state);
	return state.ticks;
}

uint16_t JunctionTerminalSpeed(const JunctionTrainParams &params)
{
	/* Run from rest until the speed reaches max_speed or stops rising for a while; the run is deterministic, so a later run from rest reaches the same top speed. */
	static constexpr uint32_t SETTLE_TICKS = 5000;
	JunctionTrainState state;
	uint16_t best = 0;
	uint32_t since_rise = 0;
	while (state.ticks < MAX_MODEL_TICKS_PRIO && since_rise < SETTLE_TICKS && best < params.max_speed) {
		StepJunctionTrain(params, state);
		if (state.speed > best) {
			best = state.speed;
			since_rise = 0;
		} else {
			since_rise++;
		}
	}
	return best;
}

uint32_t JunctionPrioLength(const JunctionTrainParams &joiner, uint16_t design_speed, uint8_t tl, uint8_t sd)
{
	/* ML trains are the same picked train, so they can't run faster than its top speed on flat track either (D101). */
	design_speed = std::min({design_speed, joiner.max_speed, JunctionTerminalSpeed(joiner)});
	JunctionTrainParams params = joiner;
	params.max_speed = design_speed;

	/* Two TrainLocoHandler passes per tick, as StepJunctionTrain (train_cmd.cpp:4213-4215). */
	const uint64_t ml_step = 2 * static_cast<uint64_t>(Vehicle::GetAdvanceSpeed(design_speed));
	JunctionTrainState state;
	uint64_t ml_progress = 0;
	int64_t max_deficit = 0;
	while (state.speed < design_speed && state.ticks < MAX_MODEL_TICKS_PRIO) {
		StepJunctionTrain(params, state);
		ml_progress += ml_step;
		max_deficit = std::max(max_deficit, static_cast<int64_t>(ml_progress) - static_cast<int64_t>(state.distance));
	}

	const uint32_t per_tile = JunctionProgressPerStraightTile();
	return static_cast<uint32_t>((max_deficit + per_tile - 1) / per_tile) + tl + sd;
}

uint32_t JunctionProgressPerStraightTile()
{
	return STEPS_PER_STRAIGHT_TILE * TILE_AXIAL_DISTANCE;
}

uint32_t JunctionProgressPerCornerPiece()
{
	/* Corner pieces are traversed in N/E/S/W directions, costing TILE_CORNER_DISTANCE * 2 per step (vehicle_base.h:456). */
	return STEPS_PER_CORNER_PIECE * TILE_CORNER_DISTANCE * 2;
}
