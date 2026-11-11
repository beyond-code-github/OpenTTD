/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_chain.h Junction curve speed as pairwise gap constraints between curve positions (DESIGN-nodes 4a). */

#ifndef JUNCTION_CHAIN_H
#define JUNCTION_CHAIN_H

#include "junction_generator.h"
#include "junction_layout.h"
#include "junction_rules.h"

#include <string>
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

/** A 45-degree curve on a chain: where it is along the path, which way, and the tile of the piece after the direction change. */
struct JunctionChainCurve {
	uint16_t unit; ///< Units from the path's start to the first unit in the new direction (same position rule as PaddedPathDirections, without padding).
	JunctionCurveHand hand;
	TileIndexDiffC tile;
};
/** A switch on a chain. */
struct JunctionChainSwitch {
	uint16_t unit; ///< Units from the path's start to the switch tile's piece.
	bool split; ///< SwitchEvent::first.
	bool prio; ///< A merge on a main line whose joiner is another movement (CheckPrio's rule); false for splits and lane merges.
	TileIndexDiffC tile;
};
/** One movement of a layout as the node model reads it (DESIGN-nodes 4b). */
struct JunctionChain {
	JunctionPortRole from;
	JunctionPortRole to;
	uint16_t units; ///< The path's length in units.
	std::vector<JunctionChainCurve> curves; ///< In path order.
	std::vector<JunctionChainSwitch> switches; ///< In path order.
	uint16_t steps90 = 0; ///< 90-degree direction changes, which are not curves of this model.
};
/** Read every traced path of a layout as a chain, in the checker's path order. */
std::vector<JunctionChain> ExtractJunctionChains(const JunctionLayout &layout, const JunctionSpec &spec);
/**
 * The node model's constraints on chains, checked on given chains (the template the position solve will use): curve gaps (JunctionCurveGaps, DESIGN-nodes 4a) per chain,
 * and for every prio merge on a main-line chain: no curve and no split in the 2 * prio_tiles units before the merge's unit (D108, G9c coverage).
 * @return One line per violated constraint ("chain <i> curves <a>,<b> gap <g> below <min>", "chain <i> merge at unit <u>: curve at unit <c> inside the prio coverage", "... split at unit <s> inside ..."), and "90-degree step at unit u" lines; empty when all hold.
 */
std::vector<std::string> JunctionChainViolations(const std::vector<JunctionChain> &chains, const JunctionSpec &spec);
/** One line per chain for logs: "<from>-><to> units <n>: C<unit><L|R>@x,y ... S<unit>@x,y M<unit>[p]@x,y ...", curves and switches merged in unit order. */
std::string FormatJunctionChain(const JunctionChain &chain);

#endif /* JUNCTION_CHAIN_H */
