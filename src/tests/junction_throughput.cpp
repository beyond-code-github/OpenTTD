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
