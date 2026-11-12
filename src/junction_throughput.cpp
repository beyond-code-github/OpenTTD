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
#include "track_func.h"

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
	/* The paths-in-movement-order loop is repeated in PrioTimingsOf (junction_generator.cpp). */
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
	out.prio_timings = JunctionPrioTimings(layout, spec, paths, train, out.design_speed, tl);
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

/** Tick cap of one prio timing run. */
static constexpr uint32_t MAX_PRIO_TIMING_TICKS = 20000;

/** Block ids of the virtual track beyond a path's exit start here, clear of the layout's block ids. */
static constexpr size_t VIRTUAL_BLOCK_BASE = SIZE_MAX / 2;

/** Whether two layout offsets are the same tile. */
static bool IsSameTile(TileIndexDiffC a, TileIndexDiffC b)
{
	return a.x == b.x && a.y == b.y;
}

/** The progress at the start of each piece of a path, and the path's total as the last entry. */
static std::vector<int64_t> PieceStarts(const std::vector<JunctionPiece> &pieces)
{
	std::vector<int64_t> out;
	int64_t progress = 0;
	for (const JunctionPiece &piece : pieces) {
		out.push_back(progress);
		progress += PieceProgress(piece);
	}
	out.push_back(progress);
	return out;
}

/**
 * The blocks a train body holds on the joiner's path and the virtual track beyond the exit.
 * Pieces without a block (signal tiles) hold none. The virtual block 0, from the exit to the first virtual signal, is the last block of the ML's path.
 * @param layout The layout.
 * @param blocks The layout's blocks.
 * @param pieces The joiner's pieces.
 * @param starts Progress at the start of each joiner piece, in the ML's coordinates, and the end as the last entry.
 * @param exit The ML's path end in its coordinates.
 * @param last_block The block of the ML path's last piece that has one.
 * @param spacing Progress between virtual signals.
 * @param low Body tail in the ML's coordinates.
 * @param high Body head in the ML's coordinates.
 * @return The blocks overlapped by [low, high).
 */
static std::set<size_t> BodyBlocks(const JunctionLayout &layout, const std::map<std::pair<int, int>, size_t> &blocks, const std::vector<JunctionPiece> &pieces, const std::vector<int64_t> &starts, int64_t exit, std::optional<size_t> last_block, int64_t spacing, int64_t low, int64_t high)
{
	std::set<size_t> out;
	for (size_t i = 0; i < pieces.size(); i++) {
		if (starts[i] >= high || starts[i + 1] <= low) continue;
		const std::optional<size_t> block = BlockOfPiece(layout, blocks, pieces[i]);
		if (block.has_value()) out.insert(*block);
	}
	if (high > exit) {
		for (int64_t v = (std::max(low, exit) - exit) / spacing; v <= (high - 1 - exit) / spacing; v++) {
			out.insert(v == 0 && last_block.has_value() ? *last_block : VIRTUAL_BLOCK_BASE + static_cast<size_t>(v));
		}
	}
	return out;
}

/** The ticks, as [first, last], in which the joiner body holds a block; one entry per disjoint run. */
using HeldIntervals = std::vector<std::pair<uint32_t, uint32_t>>;

/**
 * Simulate one prio merge: the joiner runs from rest at its Entry signal piece, and every ML train start at or beyond the coverage edge is checked against the blocks the joiner holds.
 * The joiner does not depend on the ML train, so it is run once and its held intervals per block recorded; the ML train runs at constant design speed.
 * @param layout The layout.
 * @param blocks The layout's blocks.
 * @param ml The ML path.
 * @param merge_at Index of the merge piece on the ML path.
 * @param joiner The joiner's path.
 * @param joiner_at Index of the joiner's piece on the merge tile.
 * @param entry_at Index of the joiner's Entry signal piece.
 * @param spec The junction spec.
 * @param train The design train.
 * @param design_speed The ML speed.
 * @param tl Train length in tiles.
 * @return The timing, with the movement indices left for the caller.
 */
static JunctionPrioTiming SimulatePrioTiming(const JunctionLayout &layout, const std::map<std::pair<int, int>, size_t> &blocks, const JunctionPath &ml, size_t merge_at, const JunctionPath &joiner, size_t joiner_at, size_t entry_at, const JunctionSpec &spec, const JunctionTrainParams &train, uint16_t design_speed, uint8_t tl)
{
	const int64_t tail = static_cast<int64_t>(tl) * JunctionProgressPerStraightTile();
	const int64_t spacing = static_cast<int64_t>(spec.sd) * JunctionProgressPerStraightTile();
	const int64_t coverage = static_cast<int64_t>(spec.prio_tiles) * JunctionProgressPerStraightTile();

	/* The joiner's progress is mapped to the ML's by the end of the merge piece, where the two paths coincide. */
	const std::vector<int64_t> ml_starts = PieceStarts(ml.pieces);
	std::vector<int64_t> joiner_starts = PieceStarts(joiner.pieces);
	const int64_t merge_end = ml_starts[merge_at + 1];
	const int64_t offset = merge_end - joiner_starts[joiner_at + 1];
	for (int64_t &start : joiner_starts) start += offset;
	const int64_t exit = ml_starts.back();

	std::optional<size_t> last_block;
	for (const JunctionPiece &piece : ml.pieces) {
		const std::optional<size_t> block = BlockOfPiece(layout, blocks, piece);
		if (block.has_value()) last_block = block;
	}

	/* The signals the ML head crosses, each with the block beyond it: the next piece with a block. */
	std::vector<std::pair<int64_t, size_t>> signals;
	for (size_t j = 0; j < ml.pieces.size(); j++) {
		if (!IsSignalledPiece(layout, ml.pieces[j])) continue;
		for (size_t i = j + 1; i < ml.pieces.size(); i++) {
			const std::optional<size_t> block = BlockOfPiece(layout, blocks, ml.pieces[i]);
			if (!block.has_value()) continue;
			signals.emplace_back(ml_starts[j], *block);
			break;
		}
	}

	JunctionPrioTiming out{};
	out.merge_tile = ml.pieces[merge_at].tile;
	out.entry_to_merge = static_cast<uint32_t>(joiner_starts[joiner_at] - joiner_starts[entry_at]);

	JunctionTrainParams params = train;
	params.max_speed = design_speed;
	JunctionTrainState joiner_state;
	auto held_blocks = [&]() {
		const int64_t head = joiner_starts[entry_at] + static_cast<int64_t>(joiner_state.distance);
		return BodyBlocks(layout, blocks, joiner.pieces, joiner_starts, exit, last_block, spacing, std::max(joiner_starts[0], head - tail), head);
	};

	/* Run the joiner once, recording when it holds each block. */
	std::map<size_t, HeldIntervals> held;
	auto record = [&]() {
		for (size_t block : held_blocks()) {
			HeldIntervals &intervals = held[block];
			if (!intervals.empty() && intervals.back().second + 1 == joiner_state.ticks) {
				intervals.back().second = joiner_state.ticks;
			} else {
				intervals.emplace_back(joiner_state.ticks, joiner_state.ticks);
			}
		}
	};
	record();
	bool ended = false;
	while (!ended && joiner_state.ticks < MAX_PRIO_TIMING_TICKS) {
		StepJunctionTrain(params, joiner_state);
		record();
		const int64_t joiner_head = joiner_starts[entry_at] + static_cast<int64_t>(joiner_state.distance);
		ended = joiner_state.speed >= design_speed && joiner_head - tail >= merge_end;
	}
	out.timed_out = !ended;

	/* The ML head's distance after n ticks at constant design speed, to the joiner's end tick. */
	std::vector<int64_t> distance{0};
	JunctionTrainState ml_state;
	ml_state.speed = design_speed;
	while (distance.size() <= joiner_state.ticks) {
		StepJunctionTrain(params, ml_state);
		distance.push_back(static_cast<int64_t>(ml_state.distance));
	}

	/* The virtual signals beyond the exit, for the virtual blocks the joiner held. */
	for (const auto &[block, intervals] : held) {
		if (block >= VIRTUAL_BLOCK_BASE) signals.emplace_back(exit + static_cast<int64_t>(block - VIRTUAL_BLOCK_BASE) * spacing, block);
	}

	/* Per signal and held interval, the ML starts that cross the signal inside it form the range [low, high]; the start x crosses at the first tick n with x + d(n) >= s. */
	const int64_t edge = ml_starts[merge_at] - coverage;
	std::optional<int64_t> worst;
	std::optional<int64_t> gap;
	std::optional<int64_t> farthest;
	for (const auto &[position, block] : signals) {
		const auto it = held.find(block);
		if (it == held.end()) continue;
		for (const auto &[first, last] : it->second) {
			if (last < 1) continue;
			const int64_t low = position - distance[last];
			const int64_t high = position - distance[std::max<uint32_t>(first, 1) - 1] - 1;
			if (high < low) continue;
			farthest = std::min(farthest.value_or(INT64_MAX), low);
			if (low <= edge) {
				worst = std::max(worst.value_or(INT64_MIN), std::min(high, edge));
			} else {
				gap = std::min(gap.value_or(INT64_MAX), low - edge);
			}
		}
	}

	out.ml_delayed = worst.has_value();
	out.worst_start = worst.value_or(edge);
	out.required_distance = farthest.has_value() ? static_cast<uint32_t>(std::max<int64_t>(0, ml_starts[merge_at] - *farthest)) : 0;
	if (out.ml_delayed) {
		out.margin_ticks = 0;
	} else if (gap.has_value()) {
		out.margin_ticks = JunctionTicksAtSpeed(train, design_speed, static_cast<uint64_t>(*gap));
	} else {
		out.margin_ticks = UINT32_MAX;
	}
	return out;
}

/**
 * Worst-case timing of every prio merge on the real layout (PLAN-sim-driven §3.1, ML delay; D119).
 * Each ML train start at or beyond the coverage edge is checked, not only the edge: see SimulatePrioTiming.
 * The merges, their joiners and the joiners' Entry signal pieces are found as CheckPrioReach finds them.
 * Both paths continue beyond the exit as straight track with a signal every sd tiles, from sd tiles after the exit port.
 * @param layout The layout.
 * @param spec The junction spec; sd and prio_tiles set the virtual signals and the coverage.
 * @param paths The movements' paths, in JunctionTMovements() order.
 * @param train The design train, loaded.
 * @param design_speed The ML speed in cur_speed units.
 * @param tl Train length in tiles.
 * @return One timing per prio merge, ordered by ML movement and merge piece.
 */
std::vector<JunctionPrioTiming> JunctionPrioTimings(const JunctionLayout &layout, const JunctionSpec &spec, const std::vector<JunctionPath> &paths, const JunctionTrainParams &train, uint16_t design_speed, uint8_t tl)
{
	const JunctionCheckContext ctx{layout, spec, &JunctionTTopology(), {}, {}, {}, paths, {}};
	const auto used = UsedTracks(ctx);
	const auto blocks = ComputeBlocks(layout);

	std::vector<JunctionPrioTiming> out;
	for (size_t pi = 0; pi < paths.size(); pi++) {
		const JunctionPath &ml = paths[pi];
		if (!IsMlMovement({ml.from, ml.to})) continue;

		for (const SwitchEvent &event : SwitchEventsOf(layout, used, ml)) {
			if (event.first) continue;
			const JunctionPiece &merge_piece = ml.pieces[event.second];
			if (!IsDiagonalTrackdir(merge_piece.trackdir)) continue;

			/* The joiner and its Entry signal, found as CheckPrioReach finds them. */
			const JunctionPath *joiner = nullptr;
			size_t joiner_index = 0;
			size_t joiner_path = 0;
			for (size_t qi = 0; qi < paths.size() && joiner == nullptr; qi++) {
				if (qi == pi) continue;
				for (const SwitchEvent &other : SwitchEventsOf(layout, used, paths[qi])) {
					const JunctionPiece &piece = paths[qi].pieces[other.second];
					if (other.first || !IsSameTile(piece.tile, merge_piece.tile) || piece.trackdir == merge_piece.trackdir) continue;
					joiner = &paths[qi];
					joiner_index = other.second;
					joiner_path = qi;
					break;
				}
			}
			if (joiner == nullptr || (joiner->from == ml.from && joiner->to == ml.to)) continue;
			std::optional<size_t> entry_at;
			for (size_t i = joiner_index; i-- > 0;) {
				if (!IsSignalledPiece(layout, joiner->pieces[i])) continue;
				entry_at = i;
				break;
			}
			if (!entry_at.has_value()) continue;

			/* Only a merge that has a prio: its tile holds an unused track. */
			const auto merge_used = used.find({merge_piece.tile.x, merge_piece.tile.y});
			bool has_prio = false;
			for (Track track : TracksAt(layout, merge_piece.tile)) {
				if (merge_used == used.end() || !merge_used->second.Test(track)) has_prio = true;
			}
			if (!has_prio) continue;

			JunctionPrioTiming timing = SimulatePrioTiming(layout, blocks, ml, event.second, *joiner, joiner_index, *entry_at, spec, train, design_speed, tl);
			timing.ml = pi;
			timing.joiner = joiner_path;
			out.push_back(timing);
		}
	}
	return out;
}
