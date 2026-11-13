/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_spec.h Junction spec: notation parsing, unit prices, curve spacing and the test spec. */

#ifndef JUNCTION_SPEC_H
#define JUNCTION_SPEC_H

#include "../economy_type.h"
#include "../signal_type.h"
#include "junction_train_model.h"

#include <cstdint>
#include <functional>
#include <string_view>

enum class JunctionNotationError : uint8_t { None, Invalid, MultiLane };
struct JunctionNotation { uint8_t gap = 0; JunctionNotationError error = JunctionNotationError::None; };
JunctionNotation ParseJunctionNotation(std::string_view text);

struct JunctionUnitPrices {
	Money rail_piece;
	Money signal;
	std::function<Money(uint)> bridge; ///< By middle-tile count.
	std::function<Money(uint)> tunnel; ///< One tunnel by middle-tile count, as CmdBuildTunnel charges it without clearing the portals; unset: no tunnels (D155).
};

/** Everything the generator needs; the game fills it (PLAN-1b §5.1). */
struct JunctionSpec {
	uint8_t ml_gap;
	uint8_t branch_gap;
	uint8_t tl; ///< Train length in tiles.
	uint8_t prio_tiles = 0; ///< Prio coverage upstream of a merge, in straight tiles; set by GenerateJunction's sizing (D137), where a non-zero value is the least coverage tried.
	uint8_t sd; ///< Signal spacing in tiles.
	uint16_t reference_speed; ///< Same units as Train::GetCurveSpeedLimit.
	uint8_t curve_speed; ///< RailTypeInfo::curve_speed.
	bool curve_limit; ///< False under the original acceleration model.
	uint8_t slope_steepness = 3; ///< _settings_game.vehicle.train_slope_steepness (%).
	SignalType signal_type; ///< Block or PathOneWay.
	std::function<bool(uint)> bridge_span_ok; ///< By middle-tile count.
	uint16_t max_bridge_middle = 10; ///< The longest middle the game allows (D208): the max_bridge_length setting; bridge_span_ok and prices.bridge are defined up to it.
	JunctionUnitPrices prices;
	uint16_t max_tunnel_length = 64; ///< The game's max_tunnel_length, in middle tiles.
	JunctionTrainParams train{}; ///< The loaded design train (D101, D137); required.
	bool prio_strict = true; ///< Reject layouts whose prio timing delays the ML (D119); false only for tests that pin geometry at a fixed coverage, and for rig comparisons (D137).
};

uint16_t CurveSpeedLimitForSpacing(uint spacing, uint8_t curve_speed);
uint DeriveCurveSpacing(uint16_t speed, uint8_t curve_speed, uint8_t tl);
int JunctionRampLevels(const JunctionSpec &spec); ///< D219: the most levels a route may rise within tl consecutive tiles with no speed loss at reference_speed (realistic model, RECON-slope-physics); tl or more when the slope costs nothing.
JunctionSpec MakeTestJunctionSpec();

#endif /* JUNCTION_SPEC_H */
