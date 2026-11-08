/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_throughput.h Junction throughput evaluator: running times, block occupation and the bottleneck. */

#ifndef JUNCTION_THROUGHPUT_H
#define JUNCTION_THROUGHPUT_H

#include "junction_generator.h"
#include "junction_layout.h"
#include "junction_route.h"
#include "junction_rules.h"
#include "junction_train_model.h"

#include <optional>
#include <utility>
#include <vector>

/** A block a movement holds, in ticks from the train's arrival at the path start (PLAN-sim-driven §3.1). */
struct JunctionBlockOccupation {
	size_t block;
	uint32_t start; ///< The head reaches the block.
	uint32_t end; ///< The tail leaves the block.
};

/** One movement through the junction at design speed. */
struct JunctionMovementTiming {
	JunctionPortRole from;
	JunctionPortRole to;
	uint64_t length; ///< Path progress.
	uint32_t ticks; ///< Head from path start to path end.
	std::vector<JunctionBlockOccupation> occupations; ///< In path order.
};

/** Relative traffic per movement; weights need not sum to 1. */
struct JunctionTrafficMix {
	std::vector<std::pair<std::pair<JunctionPortRole, JunctionPortRole>, double>> weights;
};

/** A held train of one movement blocking another movement (PLAN-sim-driven §3.1, queue spillback). */
struct JunctionQueueConflict {
	size_t queued; ///< Index of the held train's movement.
	uint64_t stop; ///< Progress along the queued movement's path where its head is held.
	bool at_exit; ///< Held beyond the exit port rather than at a signal inside the junction.
	size_t blocked; ///< Index of the movement that is blocked.
	size_t block; ///< The block they share.
	bool ml_blocked; ///< The blocked movement is an ML movement.
};

/** What the evaluator reports. */
struct JunctionThroughput {
	uint16_t design_speed;
	std::vector<JunctionMovementTiming> movements; ///< In JunctionTMovements() order.
	std::vector<std::vector<uint32_t>> headway; ///< headway[i][j]: the least ticks between a train of movement i and a following train of movement j.
	size_t bottleneck_block;
	double bottleneck_load; ///< Ticks per train of the mix.
	std::vector<size_t> bottleneck_movements; ///< Indices into movements.
	double trains_per_1000_ticks;
	std::vector<JunctionQueueConflict> queue_conflicts; ///< Held trains that block other movements.
};

uint32_t JunctionTicksAtSpeed(const JunctionTrainParams &train, uint16_t speed, uint64_t distance);
JunctionTrafficMix MakeDefaultJunctionMix();
std::optional<JunctionThroughput> EvaluateJunctionThroughput(const JunctionLayout &layout, const JunctionSpec &spec, const JunctionTrainParams &train, uint8_t tl, const JunctionTrafficMix &mix);

std::vector<JunctionBlockOccupation> JunctionPathOccupations(const JunctionLayout &layout, const std::vector<JunctionPiece> &pieces, const JunctionTrainParams &train, uint16_t speed, uint8_t tl);
std::vector<std::vector<uint32_t>> JunctionHeadways(const std::vector<JunctionMovementTiming> &movements);
std::vector<JunctionQueueConflict> JunctionQueueConflicts(const JunctionLayout &layout, const std::vector<JunctionPath> &paths, const std::vector<bool> &is_ml, uint8_t tl);
void JunctionFindBottleneck(const std::vector<JunctionMovementTiming> &movements, const std::vector<double> &weights, JunctionThroughput &out);

#endif /* JUNCTION_THROUGHPUT_H */
