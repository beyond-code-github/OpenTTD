/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_train_model.h Pure model of train acceleration and progress on flat straight track. */

#ifndef JUNCTION_TRAIN_MODEL_H
#define JUNCTION_TRAIN_MODEL_H

/** Effective train properties, as the game caches them (GroundVehicleCache), for flat track. */
struct JunctionTrainParams {
	uint32_t power_hp = 0; ///< gcache.cached_power (sum over parts, hp).
	uint32_t max_te = 0; ///< gcache.cached_max_te (N).
	uint32_t mass_t = 1; ///< gcache.cached_weight (t), at least 1.
	uint32_t air_drag = 0; ///< gcache.cached_air_drag.
	uint16_t max_speed = 0; ///< Speed cap in cur_speed units (the lowest of engine, rail type and curve limits).
};

/** The state the game advances each tick. */
struct JunctionTrainState {
	uint16_t speed = 0; ///< cur_speed.
	uint8_t subspeed = 0;
	uint32_t progress = 0; ///< Saved progress, below the next step's cost.
	uint64_t distance = 0; ///< Progress units travelled in total.
	uint32_t ticks = 0;
};

/** Advance one tick on flat straight track: GetAcceleration (AS_ACCEL), DoUpdateSpeed, then progress, exactly as the game's integer code. */
void StepJunctionTrain(const JunctionTrainParams &params, JunctionTrainState &state);

/** Ticks for a train starting at speed 0 to travel the given progress distance on straight track. */
uint32_t JunctionTicksToCover(const JunctionTrainParams &params, uint64_t distance);

/** The highest speed the train reaches from rest on flat straight track, capped by its max_speed. */
uint16_t JunctionTerminalSpeed(const JunctionTrainParams &params);

/**
 * Shortest prio coverage, in whole tiles of straight track, so that an ML train arriving at design_speed never comes within one block of a joiner that starts from rest at the merge signal (D102).
 * @param joiner The joining train, loaded (D101).
 * @param design_speed ML speed in cur_speed units: min(train max, rail type max) (D101).
 * @param tl Train length in tiles.
 * @param sd Signal spacing in tiles (one block).
 * @return L = ceil(max over t of (ML distance − joiner distance), in tiles) + tl + sd.
 */
uint32_t JunctionPrioLength(const JunctionTrainParams &joiner, uint16_t design_speed, uint8_t tl, uint8_t sd);

/** Progress units for one straight (X/Y) tile and for one corner (half) piece, from the game's step data. */
uint32_t JunctionProgressPerStraightTile();
uint32_t JunctionProgressPerCornerPiece();

#endif /* JUNCTION_TRAIN_MODEL_H */
