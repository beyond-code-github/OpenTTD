/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_throughput.cpp Junction throughput evaluator: running times, block occupation and the bottleneck. */

#include "stdafx.h"

#include "junction_throughput.h"
#include "junction_rules_internal.h"

#include <algorithm>
#include <map>

#include "safeguards.h"

/** Give-up bound for JunctionTicksAtSpeed. */
static constexpr uint32_t MAX_THROUGHPUT_TICKS = 10'000'000;

/**
 * Ticks for a train held at a constant speed to cover a progress distance.
 * The train starts at the given speed with its cap set to that speed, and is stepped with StepJunctionTrain until the distance is reached.
 * @param train The train.
 * @param speed The design speed in cur_speed units.
 * @param distance Progress units to cover.
 * @return The ticks the head needs; 0 for no distance.
 */
uint32_t JunctionTicksAtSpeed(const JunctionTrainParams &train, uint16_t speed, uint64_t distance)
{
	JunctionTrainParams params = train;
	params.max_speed = speed;
	JunctionTrainState state;
	state.speed = speed;
	while (state.distance < distance && state.ticks < MAX_THROUGHPUT_TICKS) StepJunctionTrain(params, state);
	return state.ticks;
}

/** The default traffic mix: weight 3 for each ML movement, 1 for each branch movement. */
JunctionTrafficMix MakeDefaultJunctionMix()
{
	JunctionTrafficMix mix;
	for (const auto &movement : JunctionTMovements()) {
		const bool is_ml = (movement.first == JunctionPortRole::EIn && movement.second == JunctionPortRole::EOut) || (movement.first == JunctionPortRole::WIn && movement.second == JunctionPortRole::WOut);
		mix.weights.push_back({movement, is_ml ? 3.0 : 1.0});
	}
	return mix;
}

/**
 * Evaluate the throughput of a layout at design speed.
 * @param layout The layout.
 * @param spec The junction spec.
 * @param train The design train.
 * @param tl Train length in tiles.
 * @param mix The traffic mix.
 * @return The result, or nullopt when a movement has no path or a signal is not block, entry, exit or combo.
 */
std::optional<JunctionThroughput> EvaluateJunctionThroughput(const JunctionLayout &layout, const JunctionSpec &spec, const JunctionTrainParams &train, uint8_t tl, const JunctionTrafficMix &mix)
{
	for (const JunctionSignal &signal : layout.signals) {
		if (signal.type != SignalType::Block && signal.type != SignalType::Entry && signal.type != SignalType::Exit && signal.type != SignalType::Combo) return std::nullopt;
	}

	const JunctionCheckContext ctx = MakeJunctionCheckContext(layout, spec, FindJunctionPorts(layout), JunctionTMovements());

	JunctionThroughput out{};
	out.design_speed = JunctionTerminalSpeed(train);
	std::vector<double> weights;
	for (const auto &movement : JunctionTMovements()) {
		const auto path = std::ranges::find_if(ctx.paths, [&](const JunctionPath &p) { return p.from == movement.first && p.to == movement.second; });
		if (path == ctx.paths.end()) return std::nullopt;

		JunctionMovementTiming timing{movement.first, movement.second, 0, 0, {}};
		for (const JunctionPiece &piece : path->pieces) {
			timing.length += (piece.on_bridge || piece.units == 2) ? JunctionProgressPerStraightTile() : JunctionProgressPerCornerPiece();
		}
		timing.ticks = JunctionTicksAtSpeed(train, out.design_speed, timing.length);
		timing.occupations = JunctionPathOccupations(layout, path->pieces, train, out.design_speed, tl);
		out.movements.push_back(std::move(timing));

		const auto weight = std::ranges::find_if(mix.weights, [&](const auto &w) { return w.first == movement; });
		weights.push_back(weight == mix.weights.end() ? 0.0 : weight->second);
	}

	out.headway = JunctionHeadways(out.movements);
	JunctionFindBottleneck(out.movements, weights, out);
	return out;
}

/**
 * The blocks a path holds, from its pieces in order.
 * A maximal run of consecutive pieces in one block is one occupation, held from the head reaching the run's start
 * to the tail leaving its end (the end plus the train length). Pieces without a block belong to no occupation.
 * @param layout The layout.
 * @param pieces The path's pieces.
 * @param train The design train.
 * @param speed The design speed.
 * @param tl Train length in tiles.
 * @return The occupations in path order.
 */
std::vector<JunctionBlockOccupation> JunctionPathOccupations(const JunctionLayout &layout, const std::vector<JunctionPiece> &pieces, const JunctionTrainParams &train, uint16_t speed, uint8_t tl)
{
	const auto blocks = ComputeBlocks(layout);
	const uint64_t tail = static_cast<uint64_t>(tl) * JunctionProgressPerStraightTile();

	std::vector<JunctionBlockOccupation> out;
	std::optional<size_t> run_block;
	uint64_t run_start = 0;
	uint64_t progress = 0;
	auto close_run = [&]() {
		if (!run_block.has_value()) return;
		out.push_back({*run_block, JunctionTicksAtSpeed(train, speed, run_start), JunctionTicksAtSpeed(train, speed, progress + tail)});
		run_block.reset();
	};
	for (const JunctionPiece &piece : pieces) {
		const std::optional<size_t> block = BlockOfPiece(layout, blocks, piece);
		if (block != run_block) {
			close_run();
			run_block = block;
			run_start = progress;
		}
		progress += (piece.on_bridge || piece.units == 2) ? JunctionProgressPerStraightTile() : JunctionProgressPerCornerPiece();
	}
	close_run();
	return out;
}

/** The earliest start and latest end a movement has in a block, or nullopt when it does not use the block. */
static std::optional<std::pair<uint32_t, uint32_t>> SpanInBlock(const JunctionMovementTiming &movement, size_t block)
{
	std::optional<std::pair<uint32_t, uint32_t>> span;
	for (const JunctionBlockOccupation &o : movement.occupations) {
		if (o.block != block) continue;
		if (!span.has_value()) {
			span = {o.start, o.end};
		} else {
			span->first = std::min(span->first, o.start);
			span->second = std::max(span->second, o.end);
		}
	}
	return span;
}

/**
 * The headway matrix: H[i][j] is the most, over the blocks both movements use, of i's end minus j's start, and at least 0.
 * @param movements The movements, each timed from its own path start.
 * @return The matrix.
 */
std::vector<std::vector<uint32_t>> JunctionHeadways(const std::vector<JunctionMovementTiming> &movements)
{
	std::vector<std::vector<uint32_t>> out(movements.size(), std::vector<uint32_t>(movements.size(), 0));
	for (size_t i = 0; i < movements.size(); i++) {
		for (size_t j = 0; j < movements.size(); j++) {
			int64_t best = 0;
			for (const JunctionBlockOccupation &o : movements[i].occupations) {
				const auto span_i = SpanInBlock(movements[i], o.block);
				const auto span_j = SpanInBlock(movements[j], o.block);
				if (!span_i.has_value() || !span_j.has_value()) continue;
				best = std::max(best, static_cast<int64_t>(span_i->second) - static_cast<int64_t>(span_j->first));
			}
			out[i][j] = static_cast<uint32_t>(best);
		}
	}
	return out;
}

/**
 * Find the block with the largest load under a mix, and the throughput it allows.
 * A block's load is the sum over movements of weight * (latest end - earliest start) in it, with the weights normalised to sum 1.
 * Ties go to the lowest block id.
 * @param movements The movements.
 * @param weights One weight per movement.
 * @param[out] out Receives the bottleneck block, load, movements and trains per 1000 ticks.
 */
void JunctionFindBottleneck(const std::vector<JunctionMovementTiming> &movements, const std::vector<double> &weights, JunctionThroughput &out)
{
	double total = 0;
	for (double w : weights) total += w;

	std::map<size_t, double> load;
	for (size_t m = 0; m < movements.size(); m++) {
		const double weight = total > 0 ? weights[m] / total : 0.0;
		for (const JunctionBlockOccupation &o : movements[m].occupations) {
			load.try_emplace(o.block, 0.0);
		}
		for (auto &[block, block_load] : load) {
			const auto span = SpanInBlock(movements[m], block);
			if (span.has_value()) block_load += weight * (static_cast<double>(span->second) - static_cast<double>(span->first));
		}
	}

	out.bottleneck_block = 0;
	out.bottleneck_load = 0;
	out.bottleneck_movements.clear();
	bool found = false;
	for (const auto &[block, block_load] : load) {
		if (found && block_load <= out.bottleneck_load) continue;
		found = true;
		out.bottleneck_block = block;
		out.bottleneck_load = block_load;
	}
	if (found) {
		for (size_t m = 0; m < movements.size(); m++) {
			if (SpanInBlock(movements[m], out.bottleneck_block).has_value()) out.bottleneck_movements.push_back(m);
		}
	}
	out.trains_per_1000_ticks = out.bottleneck_load > 0 ? 1000.0 / out.bottleneck_load : 0.0;
}
