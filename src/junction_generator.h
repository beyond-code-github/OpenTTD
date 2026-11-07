/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_generator.h Junction generator: spec, notation parsing and curve spacing. */

#ifndef JUNCTION_GENERATOR_H
#define JUNCTION_GENERATOR_H

#include "economy_type.h"
#include "junction_layout.h"
#include "junction_terrain.h"
#include "signal_type.h"

#include <functional>
#include <optional>
#include <string_view>
#include <utility>

enum class JunctionNotationError : uint8_t { None, Invalid, MultiLane };
struct JunctionNotation { uint8_t gap = 0; JunctionNotationError error = JunctionNotationError::None; };
JunctionNotation ParseJunctionNotation(std::string_view text);

struct JunctionUnitPrices {
	Money rail_piece;
	Money signal;
	std::function<Money(uint)> bridge; ///< By middle-tile count.
};

/** Everything the generator needs; the game fills it (PLAN-1b §5.1). */
struct JunctionSpec {
	uint8_t ml_gap;
	uint8_t branch_gap;
	uint8_t tl; ///< Train length in tiles.
	uint8_t sd; ///< Signal spacing in tiles.
	uint16_t reference_speed; ///< Same units as Train::GetCurveSpeedLimit.
	uint8_t curve_speed; ///< RailTypeInfo::curve_speed.
	bool curve_limit; ///< False under the original acceleration model.
	SignalType signal_type; ///< Block or PathOneWay.
	std::function<bool(uint)> bridge_span_ok; ///< By middle-tile count.
	JunctionUnitPrices prices;
};

enum class JunctionGenerateError : uint8_t { None, InvalidNotation, MultiLane, NoLayout, NeedsMlGap };
struct JunctionGenerateResult {
	std::optional<JunctionLayout> layout;
	JunctionGenerateError error = JunctionGenerateError::None;
	TileIndexDiffC hub{}; ///< The offset, in the returned layout, of working coordinate (30, 0).
};

uint16_t CurveSpeedLimitForSpacing(uint spacing, uint8_t curve_speed);
uint DeriveCurveSpacing(uint16_t speed, uint8_t curve_speed, uint8_t tl);
JunctionSpec MakeTestJunctionSpec();
JunctionGenerateResult GenerateJunction(const JunctionSpec &spec, const JunctionTerrain *terrain = nullptr);

/** The swept inputs of one candidate. */
struct JunctionCandidateParams {
	uint8_t height; ///< H: the branch edge is row H-1; the E track is row 0.
	Money crossing_cost; ///< crossable_cost when a route crosses the W track.
	uint8_t joiner_slack; ///< Extra tiles kept between the lane merge and where N->W may join.
	int plane = 0; ///< The level the junction is built on; only used with a terrain.
};

/**
 * Build one generator candidate.
 * @param spec The junction spec.
 * @param params The swept inputs.
 * @param terrain Optional terrain snapshot, in the generator's working coordinates before cropping and normalising (x 0..63, rows from 0).
 *                The anchor is the hub point (30, 0): the terrain's cell (30 - origin.x, 0 - origin.y) must be the hovered tile.
 * @param terrain_cost When set together with terrain, receives the sum of CostAt over occupied track tiles and bridge heads, once per tile.
 * @return The layout, or std::nullopt when this candidate cannot be built.
 */
std::optional<JunctionLayout> BuildJunctionCandidate(const JunctionSpec &spec, const JunctionCandidateParams &params, const JunctionTerrain *terrain = nullptr, Money *terrain_cost = nullptr, TileIndexDiffC *hub = nullptr);

/** Generator score of a layout: cost, then open space (D76). Lower is better. */
std::pair<Money, int> ScoreJunctionLayout(const JunctionLayout &layout, const JunctionSpec &spec); // second = -open rectangle area

#endif /* JUNCTION_GENERATOR_H */
