/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_throughput.cpp Test functionality from junction_throughput. */

#include "../stdafx.h"

#include "../3rdparty/catch2/catch.hpp"

#include "../junction_generator.h"
#include "../junction_route.h"
#include "../junction_rules.h"
#include "../junction_rules_internal.h"
#include "../junction_throughput.h"
#include "../junction_train_model.h"

#include "../safeguards.h"

using Role = JunctionPortRole;

/** Build a normalised layout from route programs; the start tiles must not be negative. */
static JunctionLayout Build(std::initializer_list<JunctionRoute> routes)
{
	JunctionLayout layout;
	for (const JunctionRoute &route : routes) {
		const JunctionRaster raster = RasterizeJunctionRoute(route);
		REQUIRE(raster.error.empty());
		AddRasterToLayout(layout, raster);
	}
	NormaliseJunctionLayout(layout);
	return layout;
}

static void AddSignal(JunctionLayout &layout, int x, int y, Trackdir td, SignalType type = SignalType::Block)
{
	layout.signals.push_back({{static_cast<int16_t>(x), static_cast<int16_t>(y)}, td, type, SignalVariant::Electric});
}

/** The train fixture: a strong loaded train, capped at the spec's reference speed. */
static JunctionTrainParams FixtureTrain(const JunctionSpec &spec)
{
	JunctionTrainParams p;
	p.power_hp = 4000;
	p.max_te = 300000;
	p.mass_t = 500;
	p.air_drag = 0;
	p.max_speed = spec.reference_speed;
	return p;
}

/** A hand-made movement timing with the given occupations. */
static JunctionMovementTiming Timing(std::initializer_list<JunctionBlockOccupation> occupations)
{
	JunctionMovementTiming timing{Role::EIn, Role::EOut, 0, 0, occupations};
	return timing;
}

TEST_CASE("EvaluateJunctionThroughput on the generated test junction", "[junction][throughput]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionGenerateResult result = GenerateJunction(spec);
	REQUIRE(result.layout.has_value());

	const JunctionTrainParams train = FixtureTrain(spec);
	const std::optional<JunctionThroughput> out = EvaluateJunctionThroughput(*result.layout, spec, train, spec.tl, MakeDefaultJunctionMix());
	REQUIRE(out.has_value());
	REQUIRE(out->movements.size() == JunctionTMovements().size());
	for (size_t i = 0; i < out->movements.size(); i++) {
		const JunctionMovementTiming &m = out->movements[i];
		CHECK(m.from == JunctionTMovements()[i].first);
		CHECK(m.to == JunctionTMovements()[i].second);
		REQUIRE_FALSE(m.occupations.empty());
		uint32_t prev_start = 0;
		for (const JunctionBlockOccupation &o : m.occupations) {
			CHECK(o.start >= prev_start);
			CHECK(o.end > o.start);
			prev_start = o.start;
		}
	}
	CHECK(out->trains_per_1000_ticks > 0);
	const bool ml_in_bottleneck = std::ranges::any_of(out->bottleneck_movements, [&](size_t i) {
		const auto &mv = JunctionTMovements()[i];
		return (mv.first == Role::EIn && mv.second == Role::EOut) || (mv.first == Role::WIn && mv.second == Role::WOut);
	});
	CHECK(ml_in_bottleneck);
	CHECK(std::ranges::none_of(out->queue_conflicts, [](const JunctionQueueConflict &c) { return c.ml_blocked; }));
}

TEST_CASE("JunctionTicksAtSpeed matches stepping", "[junction][throughput]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	JunctionTrainParams train = FixtureTrain(spec);
	const uint16_t speed = JunctionTerminalSpeed(train);
	REQUIRE(speed > 0);

	const uint64_t distance = 10ull * JunctionProgressPerStraightTile();
	train.max_speed = speed;
	JunctionTrainState state;
	state.speed = speed;
	while (state.distance < distance) StepJunctionTrain(train, state);
	CHECK(state.ticks > 0);
	CHECK(JunctionTicksAtSpeed(FixtureTrain(spec), speed, distance) == state.ticks);
	CHECK(JunctionTicksAtSpeed(FixtureTrain(spec), speed, 0) == 0);
}

TEST_CASE("Occupation on a straight signalled line", "[junction][throughput]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionTrainParams train = FixtureTrain(spec);
	const uint16_t speed = JunctionTerminalSpeed(train);
	const uint8_t tl = 2;

	/*
	 * "FFFFFFFFFFFF" is the start tile plus 12 moves: 13 straight tiles, x = 0..12 on row 0, P progress each.
	 * Block signals at x = 3 and x = 7 face the travel direction; signal tiles are in no block.
	 *   block A: x 0..2   progress [0, 3P]
	 *   (signal x = 3)
	 *   block B: x 4..6   progress [4P, 7P]
	 *   (signal x = 7)
	 *   block C: x 8..12  progress [8P, 13P]
	 * Each block is held from t(entry) to t(exit + tl * P).
	 */
	JunctionLayout layout = Build({{{0, 0}, Trackdir::X_SW, "FFFFFFFFFFFF"}});
	AddSignal(layout, 3, 0, Trackdir::X_SW);
	AddSignal(layout, 7, 0, Trackdir::X_SW);
	const JunctionRaster raster = RasterizeJunctionRoute({{0, 0}, Trackdir::X_SW, "FFFFFFFFFFFF"});
	REQUIRE(raster.pieces.size() == 13);

	const std::vector<JunctionBlockOccupation> occ = JunctionPathOccupations(layout, raster.pieces, train, speed, tl);
	const uint64_t P = JunctionProgressPerStraightTile();
	const uint64_t tail = tl * P;
	REQUIRE(occ.size() == 3);
	CHECK(occ[0].start == JunctionTicksAtSpeed(train, speed, 0));
	CHECK(occ[0].end == JunctionTicksAtSpeed(train, speed, 3 * P + tail));
	CHECK(occ[1].start == JunctionTicksAtSpeed(train, speed, 4 * P));
	CHECK(occ[1].end == JunctionTicksAtSpeed(train, speed, 7 * P + tail));
	CHECK(occ[2].start == JunctionTicksAtSpeed(train, speed, 8 * P));
	CHECK(occ[2].end == JunctionTicksAtSpeed(train, speed, 13 * P + tail));
	CHECK(occ[0].block != occ[1].block);
	CHECK(occ[1].block != occ[2].block);
	CHECK(occ[0].block != occ[2].block);
	CHECK(occ[0].start == 0);
}

TEST_CASE("Headway from shared blocks", "[junction][throughput]")
{
	const std::vector<JunctionMovementTiming> m = {
		Timing({{1, 0, 100}, {2, 100, 200}}), // 0
		Timing({{1, 10, 60}, {2, 50, 150}}), // 1: shares both blocks with 0
		Timing({{3, 0, 50}}), // 2: shares nothing
		Timing({{1, 500, 600}}), // 3: starts long after 0 has left
		Timing({{1, 10, 20}, {1, 40, 90}}), // 4: uses block 1 twice
	};
	const auto h = JunctionHeadways(m);
	REQUIRE(h.size() == 5);
	for (const auto &row : h) REQUIRE(row.size() == 5);

	/* Shared blocks: H[0][1] = max(100 - 10, 200 - 50) = 150; H[1][0] = max(60 - 0, 150 - 100) = 60. */
	CHECK(h[0][1] == 150);
	CHECK(h[1][0] == 60);
	/* No shared block. */
	CHECK(h[0][2] == 0);
	CHECK(h[2][0] == 0);
	/* Negative clamps to 0: 100 - 500 < 0; the other way 600 - 0. */
	CHECK(h[0][3] == 0);
	CHECK(h[3][0] == 600);
	/* Twice-used block: earliest start 10, latest end 90. */
	CHECK(h[4][0] == 90);
	CHECK(h[0][4] == 90);
	/* A movement against itself: block 1 gives 100 - 0, block 2 gives 200 - 100. */
	CHECK(h[0][0] == 100);
}

TEST_CASE("Bottleneck load with a mix", "[junction][throughput]")
{
	const std::vector<JunctionMovementTiming> m = {
		Timing({{1, 0, 100}, {2, 100, 160}}), // 0
		Timing({{1, 20, 60}, {3, 60, 300}}), // 1
		Timing({{2, 0, 200}, {3, 10, 50}}), // 2
	};
	/* Weights 2, 1, 1 normalise to 0.5, 0.25, 0.25. Loads:
	 *  block 1: 0.5 * 100 + 0.25 * 40 = 60
	 *  block 2: 0.5 * 60 + 0.25 * 200 = 80
	 *  block 3: 0.25 * 240 + 0.25 * 40 = 70
	 * Bottleneck block 2, used by movements 0 and 2; 1000 / 80 = 12.5 trains. */
	JunctionThroughput out{};
	JunctionFindBottleneck(m, {2, 1, 1}, out);
	CHECK(out.bottleneck_block == 2);
	CHECK(out.bottleneck_load == 80.0);
	CHECK(out.bottleneck_movements == std::vector<size_t>{0, 2});
	CHECK(out.trains_per_1000_ticks == 12.5);
}

/** A path from the pieces of a prefix route and a branch route; the branch route starts on the last prefix piece's successor. */
static JunctionPath MakePath(Role from, Role to, std::vector<JunctionPiece> prefix, const JunctionRaster &route)
{
	prefix.insert(prefix.end(), route.pieces.begin(), route.pieces.end());
	return {from, to, std::move(prefix)};
}

TEST_CASE("Queue spillback: a split in a bridge's block blocks the ML", "[junction][throughput]")
{
	/*
	 * The main line on row 0 is "FFB1FFFF" from (0,0): pieces x = 0, 1, 2, bridge heads x = 3 and 5 with the span at 4, then x = 6..9.
	 * The branch leaves it on the tile after branch_x, runs on row 1 and has no signal of its own.
	 *
	 * Failing case (branch_x = 1, signals at x = 0, 6, 8): the split is on x = 2, directly before the bridge head, so the block
	 * holding the split, the bridge and the whole branch is x = 1..5 plus row 1. The branch's only held positions are the signal
	 * at x = 0 (progress 0, nothing behind it) and its path end beyond the exit port. A branch train held at the end has its body
	 * in the last tl tiles of row 1, which is that block, so it blocks the ML; the other exit port keeps the ML from being exempt.
	 */
	const JunctionSpec spec = MakeTestJunctionSpec();
	const uint8_t tl = 2;
	const JunctionRaster ml = RasterizeJunctionRoute({{0, 0}, Trackdir::X_SW, "FFB1FFFF"});
	REQUIRE(ml.error.empty());
	const std::vector<JunctionPiece> head = {ml.pieces[0]};

	auto conflicts_for = [&](int branch_x, std::initializer_list<int> signals, const std::vector<JunctionPiece> &prefix, bool branch_signal) {
		const std::string branch_moves = "LFR" + std::string(7 - branch_x, 'F');
		JunctionLayout layout = Build({{{0, 0}, Trackdir::X_SW, "FFB1FFFF"}, {{static_cast<int16_t>(branch_x), 0}, Trackdir::X_SW, branch_moves}});
		for (int x : signals) AddSignal(layout, x, 0, Trackdir::X_SW);
		/* The branch's first signal-legal piece after the split is the corner at (branch_x + 1, 1), which faces south. */
		if (branch_signal) AddSignal(layout, branch_x + 1, 1, Trackdir::Left_S);
		const JunctionRaster branch = RasterizeJunctionRoute({{static_cast<int16_t>(branch_x), 0}, Trackdir::X_SW, branch_moves});
		REQUIRE(branch.error.empty());
		const std::vector<JunctionPath> paths = {{Role::EIn, Role::EOut, ml.pieces}, MakePath(Role::EIn, Role::SOut, prefix, branch)};
		const auto blocks = ComputeBlocks(layout);
		const size_t bridge_block = *BlockOfPiece(layout, blocks, ml.pieces[3]);
		return std::make_pair(JunctionQueueConflicts(layout, paths, {true, false}, tl), bridge_block);
	};

	const auto [bad, bad_block] = conflicts_for(1, {0, 6, 8}, head, false);
	CHECK(std::ranges::any_of(bad, [&](const JunctionQueueConflict &c) { return c.queued == 1 && c.at_exit && c.blocked == 0 && c.ml_blocked && c.block == bad_block; }));

	/*
	 * The passing case of the rules test: a signal on x = 2 splits the bridge block from the split's, and a real layout has a signal on the branch after the split (D24).
	 * The branch is then its own block, so a branch train held at its end blocks no ML movement.
	 */
	const auto [good, good_block] = conflicts_for(0, {2, 6, 8}, {}, true);
	CHECK(std::ranges::none_of(good, [](const JunctionQueueConflict &c) { return c.ml_blocked; }));
}

TEST_CASE("Queue spillback: a whole-train bay holds its train", "[junction][throughput]")
{
	/*
	 * A straight line "FFFFFFFFFFFF" with signals at x = 3 and x = 9; a second path leaves it at x = 2 and runs on row 1 with no signal.
	 * Neither line signal is a merge signal: the branch uses only the split's block (x = 0..2 and row 1), not the blocks x = 4..8 and x = 10..12
	 * after them. So the line's only hold point is its path end, held inside the blocks after the signals, which the branch does not use,
	 * and no tl changes that. The branch's only hold point is its path end, with its body in the split's block that the line also starts in.
	 */
	const JunctionRaster line = RasterizeJunctionRoute({{0, 0}, Trackdir::X_SW, "FFFFFFFFFFFF"});
	const JunctionRaster branch = RasterizeJunctionRoute({{1, 0}, Trackdir::X_SW, "LFRFFF"});
	REQUIRE(line.error.empty());
	REQUIRE(branch.error.empty());
	JunctionLayout layout = Build({{{0, 0}, Trackdir::X_SW, "FFFFFFFFFFFF"}, {{1, 0}, Trackdir::X_SW, "LFRFFF"}});
	AddSignal(layout, 3, 0, Trackdir::X_SW);
	AddSignal(layout, 9, 0, Trackdir::X_SW);
	const std::vector<JunctionPath> paths = {{Role::EIn, Role::EOut, line.pieces}, MakePath(Role::EIn, Role::SOut, {line.pieces[0]}, branch)};
	const auto blocks = ComputeBlocks(layout);
	const size_t split_block = *BlockOfPiece(layout, blocks, line.pieces[0]);
	uint64_t branch_end = 0;
	for (const JunctionPiece &piece : paths[1].pieces) branch_end += (piece.on_bridge || piece.units == 2) ? JunctionProgressPerStraightTile() : JunctionProgressPerCornerPiece();

	for (uint8_t tl : {6, 7}) {
		const auto conflicts = JunctionQueueConflicts(layout, paths, {false, false}, tl);
		REQUIRE(conflicts.size() == 1);
		CHECK(conflicts[0].queued == 1);
		CHECK(conflicts[0].stop == branch_end);
		CHECK(conflicts[0].at_exit);
		CHECK(conflicts[0].blocked == 0);
		CHECK(conflicts[0].block == split_block);
		CHECK_FALSE(conflicts[0].ml_blocked);
	}
}

TEST_CASE("Queue spillback: same-signal followers are exempt", "[junction][throughput]")
{
	/* Both paths pass the signal at x = 5 and share the exit role, so each is queued behind the other's stops; nothing blocks. */
	const JunctionRaster line = RasterizeJunctionRoute({{0, 0}, Trackdir::X_SW, "FFFFFFFFFFFF"});
	const JunctionRaster branch = RasterizeJunctionRoute({{6, 0}, Trackdir::X_SW, "LFRFFF"});
	REQUIRE(line.error.empty());
	REQUIRE(branch.error.empty());
	JunctionLayout layout = Build({{{0, 0}, Trackdir::X_SW, "FFFFFFFFFFFF"}, {{6, 0}, Trackdir::X_SW, "LFRFFF"}});
	AddSignal(layout, 5, 0, Trackdir::X_SW);
	const std::vector<JunctionPiece> prefix(line.pieces.begin(), line.pieces.begin() + 6);
	const std::vector<JunctionPath> paths = {{Role::EIn, Role::EOut, line.pieces}, MakePath(Role::EIn, Role::EOut, prefix, branch)};
	CHECK(JunctionQueueConflicts(layout, paths, {false, false}, 2).empty());
}

TEST_CASE("Queue spillback: only hold points stop trains", "[junction][throughput]")
{
	/*
	 * The ML is row 0 from x = 0 to x = 14. A branch leaves it at x = 1, runs on row 1 and rejoins it at x = 12, so both end on the ML exit.
	 * The branch carries block signals at (3,1) and (11,1):
	 *   - (3,1) has the branch's own block (x = 4..10 on row 1) as its next block, which no other movement uses: not a merge signal.
	 *   - (11,1) has the ML's block (the rest of the layout) as its next block; the ML does not use the branch block before it: a merge signal.
	 * A branch train held at (3,1) would have its body in the ML's block (the split's corners), which would block the ML.
	 * That train stands there only if the one ahead of it is held at (11,1); a train held there has its tl = 2 body inside x = 4..10, so the ML is never blocked.
	 * The path ends are exempt: both movements exit at the same port.
	 */
	const uint8_t tl = 2;
	const JunctionRaster ml = RasterizeJunctionRoute({{0, 0}, Trackdir::X_SW, "FFFFFFFFFFFFFF"});
	const JunctionRaster branch = RasterizeJunctionRoute({{1, 0}, Trackdir::X_SW, "LFRFFFFFFFFRFLFF"});
	REQUIRE(ml.error.empty());
	REQUIRE(branch.error.empty());
	JunctionLayout layout = Build({{{0, 0}, Trackdir::X_SW, "FFFFFFFFFFFFFF"}, {{1, 0}, Trackdir::X_SW, "LFRFFFFFFFFRFLFF"}});
	AddSignal(layout, 3, 1, Trackdir::X_SW);
	AddSignal(layout, 11, 1, Trackdir::X_SW);
	const std::vector<JunctionPath> paths = {{Role::EIn, Role::EOut, ml.pieces}, MakePath(Role::EIn, Role::EOut, {ml.pieces[0]}, branch)};
	CHECK(JunctionQueueConflicts(layout, paths, {true, false}, tl).empty());
}
