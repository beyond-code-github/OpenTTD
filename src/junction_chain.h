/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_chain.h Junction curve speed as pairwise gap constraints between curve positions (DESIGN-nodes 4a). */

#ifndef JUNCTION_CHAIN_H
#define JUNCTION_CHAIN_H

#include <vector>

/** A 45-degree direction change along a path. */
enum class JunctionCurveHand : uint8_t { Left, Right };

/** pos[j] - pos[i] >= min_gap between curve i and curve j (DESIGN-nodes 4a). */
struct JunctionCurveGap {
	uint16_t i;
	uint16_t j;
	int16_t min_gap;
};

/**
 * The gap constraints between the curves of a path so that no window of a train of 2*tl parts has a curve speed limit below the reference speed.
 * They follow Train::GetCurveSpeedLimit (DESIGN-nodes 4a). The set is sufficient, and exact when a window holds at most two curves.
 * @param hands The hand of each curve in path order.
 * @param tl The train length in tiles.
 * @param curve_speed The rail type's curve speed advantage.
 * @param reference_speed The speed that must not be slowed down.
 * @return The constraints, ordered by i then j.
 */
std::vector<JunctionCurveGap> JunctionCurveGaps(const std::vector<JunctionCurveHand> &hands, uint8_t tl, uint8_t curve_speed, uint16_t reference_speed);

/**
 * Whether the positions satisfy every gap.
 * @param gaps The constraints.
 * @param pos The position of each curve.
 * @return True when all hold.
 */
bool JunctionCurveGapsHold(const std::vector<JunctionCurveGap> &gaps, const std::vector<int> &pos);

#endif /* JUNCTION_CHAIN_H */
