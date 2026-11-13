/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_spec.cpp Junction spec: notation parsing, unit prices, curve spacing and the test spec. */

#include "../stdafx.h"

#include "junction_spec.h"
#include "junction_train_model.h"
#include "../core/math_func.hpp"

#include <algorithm>

#include "../safeguards.h"

/**
 * Parse a junction notation such as "L_R": left lane(s), gap, right lane(s).
 * @param text The notation.
 * @return The gap between the lanes, or why the notation is not usable.
 */
JunctionNotation ParseJunctionNotation(std::string_view text)
{
	size_t lefts = 0;
	while (lefts < text.size() && text[lefts] == 'L') lefts++;
	size_t gap = 0;
	while (lefts + gap < text.size() && text[lefts + gap] == '_') gap++;
	size_t rights = 0;
	while (lefts + gap + rights < text.size() && text[lefts + gap + rights] == 'R') rights++;

	if (lefts > 0 && rights > 0 && lefts + gap + rights == text.size()) {
		if (lefts > 1 || rights > 1) return {0, JunctionNotationError::MultiLane};
		return {static_cast<uint8_t>(gap), JunctionNotationError::None};
	}

	/* Multi-lane forms with interleaved gaps, such as L_L__R_R, are recognised but unsupported. */
	const bool only_lr_gap = text.find_first_not_of("L_R") == std::string_view::npos;
	if (only_lr_gap && !text.empty() && text.front() == 'L' && text.back() == 'R' &&
			std::ranges::count(text, 'L') > 1 && std::ranges::count(text, 'R') > 1) {
		return {0, JunctionNotationError::MultiLane};
	}
	return {0, JunctionNotationError::Invalid};
}

/**
 * The curve speed limit of a train whose curves are 'spacing' tiles apart, as Train::GetCurveSpeedLimit.
 * @param spacing The spacing in tiles; clamped to 1..12.
 * @param curve_speed The rail type's curve speed advantage.
 * @return The speed limit.
 */
uint16_t CurveSpeedLimitForSpacing(uint spacing, uint8_t curve_speed)
{
	const int clamped = Clamp<int>(spacing, 1, 12);
	int speed = 232 - (13 - clamped) * (13 - clamped);
	speed += (speed / 2) * curve_speed;
	return static_cast<uint16_t>(speed);
}

/**
 * The smallest curve spacing at which a train of the given speed is not slowed down.
 * @param speed The wanted speed.
 * @param curve_speed The rail type's curve speed advantage.
 * @param tl The train length in tiles.
 * @return The spacing in tiles, at most twice the train length.
 */
uint DeriveCurveSpacing(uint16_t speed, uint8_t curve_speed, uint8_t tl)
{
	uint spacing = 2 * tl;
	for (uint s = 1; s <= 12; s++) {
		if (CurveSpeedLimitForSpacing(s, curve_speed) >= speed) {
			spacing = s;
			break;
		}
	}
	return std::min<uint>(spacing, 2 * tl);
}

/**
 * The most levels a route may rise within tl consecutive tiles with no speed loss at the reference speed (D219).
 * The slope force on the train is m x s x 100 N times the share of its parts on up tiles, one level per tile, so within tl tiles the train holds its speed while tl x S(v) >= levels x m x s x 100, S(v) being the tractive force at v less the flat resistances, as GroundVehicle::GetAcceleration computes them.
 * @param spec The spec; train, tl, reference_speed and slope_steepness are read.
 * @return floor(tl x S(v) / (m x s x 100)), 0 when the train cannot hold the speed on the level; tl when the steepness is 0.
 */
int JunctionRampLevels(const JunctionSpec &spec)
{
	const int64_t mass = spec.train.mass_t;
	const int64_t slope = mass * spec.slope_steepness * 100;
	if (slope == 0) return spec.tl;
	const int64_t speed = spec.reference_speed;
	const int64_t power = static_cast<int64_t>(spec.train.power_hp) * 746;
	int64_t force = speed > 0 ? power * 18 / (speed * 5) : spec.train.max_te;
	force = std::min<int64_t>(force, spec.train.max_te);
	/* Axle, rolling (the factor is an integer in the game) and air resistance (area 14). */
	const int64_t resistance = 10 * mass + mass * (15 * (512 + speed) / 512) + 14 * static_cast<int64_t>(spec.train.air_drag) * speed * speed / 1000;
	const int64_t spare = force - resistance;
	if (spare <= 0) return 0;
	return static_cast<int>(spec.tl * spare / slope);
}

uint8_t JunctionPrioTiles(const JunctionSpec &spec)
{
	const uint32_t length = JunctionPrioLength(spec.train, spec.reference_speed, spec.tl, spec.sd);
	return length > JUNCTION_MAX_PRIO_TILES ? 0 : static_cast<uint8_t>(length);
}

JunctionSpec MakeTestJunctionSpec()
{
	JunctionSpec spec{};
	spec.ml_gap = 1;
	spec.branch_gap = 1;
	spec.tl = 4;
	spec.prio_tiles = 14; /* A fixed fixture coverage (the old sizing's value at this spec), as a slider choice. */
	/* The D138 design train: Buborough's main-line consist, loaded. */
	spec.train.power_hp = 14194;
	spec.train.max_te = 1090000;
	spec.train.mass_t = 561;
	spec.train.air_drag = 25;
	spec.train.max_speed = 196;
	spec.sd = 2;
	spec.reference_speed = 160;
	spec.curve_speed = 0;
	spec.curve_limit = true;
	spec.signal_type = SignalType::Block;
	spec.bridge_span_ok = [](uint n) { return n >= 1 && n <= 10; };
	spec.prices.rail_piece = 100;
	spec.prices.signal = 50;
	spec.prices.bridge = [](uint n) { return Money(500 + 200 * n); };
	return spec;
}
