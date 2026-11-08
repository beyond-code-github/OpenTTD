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
#include <set>

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

/** Whether a movement is an ML movement, by role. */
static bool IsMlMovement(const std::pair<JunctionPortRole, JunctionPortRole> &movement)
{
	return (movement.first == JunctionPortRole::EIn && movement.second == JunctionPortRole::EOut) || (movement.first == JunctionPortRole::WIn && movement.second == JunctionPortRole::WOut);
}

/** The default traffic mix: weight 3 for each ML movement, 1 for each branch movement. */
JunctionTrafficMix MakeDefaultJunctionMix()
{
	JunctionTrafficMix mix;
	for (const auto &movement : JunctionTMovements()) {
		mix.weights.push_back({movement, IsMlMovement(movement) ? 3.0 : 1.0});
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
	std::vector<JunctionPath> paths;
	std::vector<bool> is_ml;
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
		paths.push_back(*path);
		is_ml.push_back(IsMlMovement(movement));

		const auto weight = std::ranges::find_if(mix.weights, [&](const auto &w) { return w.first == movement; });
		weights.push_back(weight == mix.weights.end() ? 0.0 : weight->second);
	}

	out.headway = JunctionHeadways(out.movements);
	JunctionFindBottleneck(out.movements, weights, out);
	out.queue_conflicts = JunctionQueueConflicts(layout, paths, is_ml, tl);
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

/** Path progress of one piece, as in the movement timings. */
static uint64_t PieceProgress(const JunctionPiece &piece)
{
	return (piece.on_bridge || piece.units == 2) ? JunctionProgressPerStraightTile() : JunctionProgressPerCornerPiece();
}

/** Whether the signal piece j of path m is a merge signal: the block after it is used by a movement that does not use the block before it. */
static bool IsMergeSignal(const std::vector<std::vector<std::optional<size_t>>> &held_block, size_t m, size_t j)
{
	const auto &blocks = held_block[m];
	std::optional<size_t> before;
	for (size_t i = j; i-- > 0 && !before.has_value();) before = blocks[i];
	std::optional<size_t> next;
	for (size_t i = j + 1; i < blocks.size() && !next.has_value(); i++) next = blocks[i];
	if (!before.has_value() || !next.has_value()) return false;
	for (size_t k = 0; k < held_block.size(); k++) {
		if (k == m) continue;
		if (std::ranges::find(held_block[k], next) != held_block[k].end() && std::ranges::find(held_block[k], before) == held_block[k].end()) return true;
	}
	return false;
}

/**
 * The queue spillback conflicts: held trains whose bodies lie in a block another movement needs.
 * A train of movement m is held with its head at the hold points (the path end, outside the layout at the first signal
 * beyond the exit port, and the merge signals on the path). Its body covers the tl tiles of progress behind the head; the
 * blocks of the non-signal pieces in that range are held. Every other movement using one of them is blocked, except
 * one that also passes the held signal (it queues behind it anyway), or, for the path end, one with the same exit port.
 * @param layout The layout.
 * @param paths The movements' paths.
 * @param is_ml Whether each movement is an ML movement.
 * @param tl Train length in tiles.
 * @return The conflicts in order of queued movement, stop, blocked movement; one per such triple, with the lowest block.
 */
std::vector<JunctionQueueConflict> JunctionQueueConflicts(const JunctionLayout &layout, const std::vector<JunctionPath> &paths, const std::vector<bool> &is_ml, uint8_t tl)
{
	const auto blocks = ComputeBlocks(layout);
	const uint64_t tail = static_cast<uint64_t>(tl) * JunctionProgressPerStraightTile();

	/* The block of each piece of each path; signal pieces and pieces without a block get none. */
	std::vector<std::vector<std::optional<size_t>>> held_block(paths.size());
	std::vector<std::set<size_t>> used(paths.size());
	for (size_t i = 0; i < paths.size(); i++) {
		for (const JunctionPiece &piece : paths[i].pieces) {
			const std::optional<size_t> block = BlockOfPiece(layout, blocks, piece);
			held_block[i].push_back(IsSignalledPiece(layout, piece) ? std::nullopt : block);
			if (block.has_value()) used[i].insert(*block);
		}
	}

	std::vector<JunctionQueueConflict> out;
	for (size_t m = 0; m < paths.size(); m++) {
		/* The stops: the start of each merge signal piece, then the path end. */
		struct Stop {
			uint64_t progress;
			std::optional<size_t> piece; ///< The signal piece, or nullopt for the path end.
		};
		std::vector<Stop> stops;
		std::vector<uint64_t> start;
		uint64_t progress = 0;
		for (size_t j = 0; j < paths[m].pieces.size(); j++) {
			start.push_back(progress);
			if (IsSignalledPiece(layout, paths[m].pieces[j]) && IsMergeSignal(held_block, m, j)) stops.push_back({progress, j});
			progress += PieceProgress(paths[m].pieces[j]);
		}
		stops.push_back({progress, std::nullopt});

		for (const Stop &stop : stops) {
			const uint64_t low = stop.progress > tail ? stop.progress - tail : 0;
			std::set<size_t> held;
			for (size_t j = 0; j < paths[m].pieces.size(); j++) {
				if (start[j] < stop.progress && start[j] + PieceProgress(paths[m].pieces[j]) > low && held_block[m][j].has_value()) held.insert(*held_block[m][j]);
			}

			for (size_t k = 0; k < paths.size(); k++) {
				if (k == m) continue;
				if (stop.piece.has_value()) {
					const JunctionPiece &signal = paths[m].pieces[*stop.piece];
					if (std::ranges::any_of(paths[k].pieces, [&](const JunctionPiece &p) { return p.tile.x == signal.tile.x && p.tile.y == signal.tile.y && p.trackdir == signal.trackdir; })) continue;
				} else if (paths[k].to == paths[m].to) {
					continue;
				}
				const auto shared = std::ranges::find_if(held, [&](size_t block) { return used[k].contains(block); });
				if (shared == held.end()) continue;
				out.push_back({m, stop.progress, !stop.piece.has_value(), k, *shared, is_ml[k]});
			}
		}
	}
	return out;
}
