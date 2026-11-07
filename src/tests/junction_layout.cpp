/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_layout.cpp Test functionality from junction_layout. */

#include "../stdafx.h"

#include "../3rdparty/catch2/catch.hpp"

#include "../junction_layout.h"
#include "../map_func.h"
#include "../track_func.h"
#include "../table/strings.h"

#include <algorithm>
#include <map>
#include <set>

#include "../safeguards.h"

/** Pricing stub with configurable failures. */
class StubJunctionPricing : public JunctionPricing {
public:
	std::set<TileIndex> not_flat;
	std::map<TileIndex, int> heights;
	std::map<TileIndex, StringID> rail_fails;
	std::map<TileIndex, Money> clear_costs;
	bool bridge_fails = false;
	Money money = 1000000;

	std::optional<int> GetFlatHeight(TileIndex tile) const override
	{
		if (tile == INVALID_TILE || this->not_flat.contains(tile)) return std::nullopt;
		auto it = this->heights.find(tile);
		return it != this->heights.end() ? it->second : 1;
	}

	CommandCost TestRail(TileIndex tile, [[maybe_unused]] Track track) const override
	{
		auto it = this->rail_fails.find(tile);
		if (it != this->rail_fails.end()) return CommandCost(it->second);
		return CommandCost(ExpensesType::Construction, 100);
	}

	CommandCost PriceBridge([[maybe_unused]] TileIndex start, [[maybe_unused]] TileIndex end) const override
	{
		if (this->bridge_fails) return CommandCost(STR_ERROR_CAN_T_BUILD_BRIDGE_HERE);
		return CommandCost(ExpensesType::Construction, 1000);
	}

	CommandCost TestClear(TileIndex tile) const override
	{
		auto it = this->clear_costs.find(tile);
		return CommandCost(ExpensesType::Construction, it != this->clear_costs.end() ? it->second : Money{0});
	}

	Money SignalCost() const override { return 50; }
	Money AvailableMoney() const override { return this->money; }
};

TEST_CASE("JunctionLayout outer: test junction on flat clear land prices exactly")
{
	Map::Allocate(64, 64);

	StubJunctionPricing pricing;
	JunctionEstimate estimate = EstimateJunction(MakeTestJunctionLayout(), TileXY(10, 10), pricing);

	/* 2000 = 9 rail pieces * 100 + 1 bridge * 1000 + 2 signals * 50. */
	CHECK(estimate.total == 2000);
	CHECK(estimate.bad_tiles.empty());
	CHECK(estimate.affordable);
}

TEST_CASE("JunctionLayout outer: clearing under a bridge over track is counted once")
{
	Map::Allocate(64, 64);

	StubJunctionPricing pricing;
	pricing.clear_costs[TileXY(14, 12)] = 428;
	JunctionEstimate estimate = EstimateJunction(MakeTestJunctionLayout(), TileXY(10, 10), pricing);

	/* The stub's TestRail and PriceBridge don't model clearing, so subtracting 428 shows the overlap rule is applied once. */
	CHECK(estimate.total == 2000 - 428);
	CHECK(estimate.affordable);
	CHECK(estimate.bad_tiles.empty());
}

TEST_CASE("JunctionLayout: clearing elsewhere is not subtracted")
{
	Map::Allocate(64, 64);

	StubJunctionPricing pricing;
	pricing.clear_costs[TileXY(10, 12)] = 428; // Track but no bridge over it.
	pricing.clear_costs[TileXY(14, 11)] = 300; // Bridge span but no track.
	JunctionEstimate estimate = EstimateJunction(MakeTestJunctionLayout(), TileXY(10, 10), pricing);

	CHECK(estimate.total == 2000);
}

TEST_CASE("JunctionLayout: test junction fixture contents")
{
	JunctionLayout layout = MakeTestJunctionLayout();

	CHECK(layout.size.x == 8);
	CHECK(layout.size.y == 6);

	REQUIRE(layout.tracks.size() == 9);
	for (int x = 0; x < 8; x++) {
		CHECK(layout.tracks[x].offset.x == x);
		CHECK(layout.tracks[x].offset.y == 2);
		CHECK(layout.tracks[x].tracks == TrackBits{Track::X});
	}
	CHECK(layout.tracks[8].offset.x == 4);
	CHECK(layout.tracks[8].offset.y == 5);
	CHECK(layout.tracks[8].tracks == TrackBits{Track::Y});

	REQUIRE(layout.bridges.size() == 1);
	CHECK(layout.bridges[0].start.x == 4);
	CHECK(layout.bridges[0].start.y == 0);
	CHECK(layout.bridges[0].end.x == 4);
	CHECK(layout.bridges[0].end.y == 4);

	REQUIRE(layout.signals.size() == 2);
	CHECK(layout.signals[0].offset.x == 1);
	CHECK(layout.signals[0].offset.y == 2);
	CHECK(layout.signals[0].facing == Trackdir::X_SW);
	CHECK(layout.signals[0].type == SignalType::Block);
	CHECK(layout.signals[0].variant == SignalVariant::Electric);
	CHECK(layout.signals[1].offset.x == 6);
	CHECK(layout.signals[1].offset.y == 2);
	CHECK(layout.signals[1].facing == Trackdir::X_NE);
	CHECK(layout.signals[1].type == SignalType::Block);
	CHECK(layout.signals[1].variant == SignalVariant::Electric);
}

TEST_CASE("JunctionLayout: build steps are rail, then bridges, then signals")
{
	Map::Allocate(64, 64);

	std::vector<JunctionBuildStep> steps = MakeJunctionBuildSteps(MakeTestJunctionLayout(), TileXY(10, 10));
	REQUIRE(steps.size() == 12);

	for (int i = 0; i < 8; i++) {
		REQUIRE(std::holds_alternative<JunctionRailStep>(steps[i]));
		const JunctionRailStep &rail = std::get<JunctionRailStep>(steps[i]);
		CHECK(rail.tile == TileXY(10 + i, 12));
		CHECK(rail.track == Track::X);
	}
	REQUIRE(std::holds_alternative<JunctionRailStep>(steps[8]));
	const JunctionRailStep &rail = std::get<JunctionRailStep>(steps[8]);
	CHECK(rail.tile == TileXY(14, 15));
	CHECK(rail.track == Track::Y);

	REQUIRE(std::holds_alternative<JunctionBridgeStep>(steps[9]));
	const JunctionBridgeStep &bridge = std::get<JunctionBridgeStep>(steps[9]);
	CHECK(bridge.start == TileXY(14, 10));
	CHECK(bridge.end == TileXY(14, 14));

	REQUIRE(std::holds_alternative<JunctionSignalStep>(steps[10]));
	const JunctionSignalStep &signal0 = std::get<JunctionSignalStep>(steps[10]);
	CHECK(signal0.tile == TileXY(11, 12));
	CHECK(signal0.facing == Trackdir::X_SW);
	CHECK(signal0.type == SignalType::Block);
	CHECK(signal0.variant == SignalVariant::Electric);

	REQUIRE(std::holds_alternative<JunctionSignalStep>(steps[11]));
	const JunctionSignalStep &signal1 = std::get<JunctionSignalStep>(steps[11]);
	CHECK(signal1.tile == TileXY(16, 12));
	CHECK(signal1.facing == Trackdir::X_NE);
	CHECK(signal1.type == SignalType::Block);
	CHECK(signal1.variant == SignalVariant::Electric);
}

TEST_CASE("JunctionLayout: one rail step per track bit")
{
	Map::Allocate(64, 64);

	JunctionLayout layout;
	layout.size = {1, 1};
	layout.tracks.push_back({{0, 0}, TrackBits{Track::Upper, Track::Lower}});

	std::vector<JunctionBuildStep> steps = MakeJunctionBuildSteps(layout, TileXY(5, 5));
	REQUIRE(steps.size() == 2);

	REQUIRE(std::holds_alternative<JunctionRailStep>(steps[0]));
	CHECK(std::get<JunctionRailStep>(steps[0]).tile == TileXY(5, 5));
	CHECK(std::get<JunctionRailStep>(steps[0]).track == Track::Upper);

	REQUIRE(std::holds_alternative<JunctionRailStep>(steps[1]));
	CHECK(std::get<JunctionRailStep>(steps[1]).tile == TileXY(5, 5));
	CHECK(std::get<JunctionRailStep>(steps[1]).track == Track::Lower);
}

TEST_CASE("JunctionLayout: steps off the map get INVALID_TILE")
{
	Map::Allocate(64, 64);

	std::vector<JunctionBuildStep> steps = MakeJunctionBuildSteps(MakeTestJunctionLayout(), TileXY(60, 60));
	REQUIRE(steps.size() == 12);

	REQUIRE(std::holds_alternative<JunctionRailStep>(steps[0]));
	CHECK(std::get<JunctionRailStep>(steps[0]).tile == TileXY(60, 62));
	REQUIRE(std::holds_alternative<JunctionRailStep>(steps[4]));
	CHECK(std::get<JunctionRailStep>(steps[4]).tile == INVALID_TILE);
	REQUIRE(std::holds_alternative<JunctionBridgeStep>(steps[9]));
	CHECK(std::get<JunctionBridgeStep>(steps[9]).start == INVALID_TILE);
}

TEST_CASE("JunctionLayout outer: one non-flat tile is bad")
{
	Map::Allocate(64, 64);

	StubJunctionPricing pricing;
	pricing.not_flat.insert(TileXY(12, 12));
	JunctionEstimate estimate = EstimateJunction(MakeTestJunctionLayout(), TileXY(10, 10), pricing);

	CHECK(estimate.bad_tiles == std::vector<TileIndex>{TileXY(12, 12)});
	CHECK(!estimate.affordable);
	CHECK(estimate.first_error == STR_ERROR_FLAT_LAND_REQUIRED);
	CHECK(estimate.total == 2000);
}

TEST_CASE("JunctionLayout outer: tile at another height is bad")
{
	Map::Allocate(64, 64);

	StubJunctionPricing pricing;
	pricing.heights[TileXY(16, 12)] = 2;
	JunctionEstimate estimate = EstimateJunction(MakeTestJunctionLayout(), TileXY(10, 10), pricing);

	CHECK(estimate.bad_tiles == std::vector<TileIndex>{TileXY(16, 12)});
	CHECK(!estimate.affordable);
	CHECK(estimate.first_error == STR_ERROR_FLAT_LAND_REQUIRED);
}

TEST_CASE("JunctionLayout outer: a failed rail step marks its tile and the rest are still priced")
{
	Map::Allocate(64, 64);

	StubJunctionPricing pricing;
	pricing.rail_fails[TileXY(11, 12)] = STR_ERROR_CAN_T_BUILD_RAILROAD_TRACK;
	JunctionEstimate estimate = EstimateJunction(MakeTestJunctionLayout(), TileXY(10, 10), pricing);

	CHECK(estimate.bad_tiles == std::vector<TileIndex>{TileXY(11, 12)});
	CHECK(estimate.first_error == STR_ERROR_CAN_T_BUILD_RAILROAD_TRACK);
	CHECK(estimate.total == 1900);
	CHECK(!estimate.affordable);
}

TEST_CASE("JunctionLayout outer: a failed bridge marks both heads")
{
	Map::Allocate(64, 64);

	StubJunctionPricing pricing;
	pricing.bridge_fails = true;
	JunctionEstimate estimate = EstimateJunction(MakeTestJunctionLayout(), TileXY(10, 10), pricing);

	CHECK(estimate.bad_tiles == std::vector<TileIndex>{TileXY(14, 10), TileXY(14, 14)});
	CHECK(estimate.total == 1000);
	CHECK(!estimate.affordable);
}

TEST_CASE("JunctionLayout outer: affordability is total against money")
{
	Map::Allocate(64, 64);

	StubJunctionPricing pricing;
	pricing.money = 1999;
	JunctionEstimate estimate = EstimateJunction(MakeTestJunctionLayout(), TileXY(10, 10), pricing);
	CHECK(!estimate.affordable);
	CHECK(estimate.bad_tiles.empty());

	pricing.money = 2000;
	estimate = EstimateJunction(MakeTestJunctionLayout(), TileXY(10, 10), pricing);
	CHECK(estimate.affordable);
}

TEST_CASE("JunctionLayout outer: layout partly off the map")
{
	Map::Allocate(64, 64);

	StubJunctionPricing pricing;
	JunctionEstimate estimate = EstimateJunction(MakeTestJunctionLayout(), TileXY(60, 60), pricing);

	CHECK(std::count(estimate.bad_tiles.begin(), estimate.bad_tiles.end(), INVALID_TILE) == 1);
	CHECK(estimate.first_error == STR_ERROR_TOO_CLOSE_TO_EDGE_OF_MAP);
	CHECK(!estimate.affordable);
}

/**
 * Compare two layouts field by field.
 * @param a The first layout.
 * @param b The second layout.
 * @return True if the size and every element are equal.
 */
static bool SameLayout(const JunctionLayout &a, const JunctionLayout &b)
{
	if (a.size.x != b.size.x || a.size.y != b.size.y) return false;
	if (a.tracks.size() != b.tracks.size() || a.signals.size() != b.signals.size() || a.bridges.size() != b.bridges.size()) return false;
	for (size_t i = 0; i < a.tracks.size(); i++) {
		if (a.tracks[i].offset.x != b.tracks[i].offset.x || a.tracks[i].offset.y != b.tracks[i].offset.y || a.tracks[i].tracks != b.tracks[i].tracks) return false;
	}
	for (size_t i = 0; i < a.signals.size(); i++) {
		const JunctionSignal &sa = a.signals[i];
		const JunctionSignal &sb = b.signals[i];
		if (sa.offset.x != sb.offset.x || sa.offset.y != sb.offset.y || sa.facing != sb.facing || sa.type != sb.type || sa.variant != sb.variant) return false;
	}
	for (size_t i = 0; i < a.bridges.size(); i++) {
		const JunctionBridge &ba = a.bridges[i];
		const JunctionBridge &bb = b.bridges[i];
		if (ba.start.x != bb.start.x || ba.start.y != bb.start.y || ba.end.x != bb.end.x || ba.end.y != bb.end.y) return false;
	}
	return true;
}

TEST_CASE("JunctionLayout: every track transforms to a valid track")
{
	for (Track track : {Track::X, Track::Y, Track::Upper, Track::Lower, Track::Left, Track::Right}) {
		CHECK(IsValidTrack(TransformTrack(track, JunctionTransform::Rotate)));
		CHECK(IsValidTrack(TransformTrack(track, JunctionTransform::Mirror)));
	}
}

TEST_CASE("JunctionLayout: known track and trackdir cases")
{
	CHECK(TransformTrack(Track::X, JunctionTransform::Rotate) == Track::Y);
	CHECK(TransformTrackdir(Trackdir::X_NE, JunctionTransform::Rotate) == Trackdir::Y_SE);
	CHECK(TransformTrack(Track::Upper, JunctionTransform::Mirror) == Track::Left);
	CHECK(TransformTrackdir(Trackdir::X_NE, JunctionTransform::Mirror) == Trackdir::X_SW);
	CHECK(TransformTrack(Track::Y, JunctionTransform::Mirror) == Track::Y);
}

TEST_CASE("JunctionLayout outer: rotating four times is the identity")
{
	JunctionLayout original = MakeTestJunctionLayout();
	JunctionLayout layout = original;
	for (int i = 0; i < 4; i++) layout = TransformJunctionLayout(layout, JunctionTransform::Rotate);

	CHECK(SameLayout(layout, original));
}

TEST_CASE("JunctionLayout outer: mirroring twice is the identity")
{
	JunctionLayout original = MakeTestJunctionLayout();
	JunctionLayout layout = TransformJunctionLayout(TransformJunctionLayout(original, JunctionTransform::Mirror), JunctionTransform::Mirror);

	CHECK(SameLayout(layout, original));
}

TEST_CASE("JunctionLayout outer: rotation swaps the size")
{
	JunctionLayout layout = TransformJunctionLayout(MakeTestJunctionLayout(), JunctionTransform::Rotate);

	CHECK(layout.size.x == 6);
	CHECK(layout.size.y == 8);
}

TEST_CASE("JunctionLayout outer: a rotated junction still prices exactly")
{
	Map::Allocate(64, 64);

	StubJunctionPricing pricing;
	JunctionEstimate estimate = EstimateJunction(TransformJunctionLayout(MakeTestJunctionLayout(), JunctionTransform::Rotate), TileXY(10, 10), pricing);

	CHECK(estimate.total == 2000);
	CHECK(estimate.bad_tiles.empty());
	CHECK(estimate.affordable);
}

TEST_CASE("JunctionLayout outer: bridge start stays the northern head")
{
	JunctionLayout layout;
	layout.size = {8, 1};
	layout.bridges.push_back({{1, 0}, {5, 0}});

	layout = TransformJunctionLayout(layout, JunctionTransform::Mirror);

	REQUIRE(layout.bridges.size() == 1);
	CHECK(layout.bridges[0].start.x == 2);
	CHECK(layout.bridges[0].start.y == 0);
	CHECK(layout.bridges[0].end.x == 6);
	CHECK(layout.bridges[0].end.y == 0);
}

TEST_CASE("JunctionLayout: orientation applies mirror then rotations")
{
	JunctionLayout layout = MakeTestJunctionLayout();

	CHECK(SameLayout(ApplyJunctionOrientation(layout, 0, false), layout));
	CHECK(SameLayout(ApplyJunctionOrientation(layout, 1, false), TransformJunctionLayout(layout, JunctionTransform::Rotate)));

	JunctionLayout expected = TransformJunctionLayout(layout, JunctionTransform::Mirror);
	for (int i = 0; i < 3; i++) expected = TransformJunctionLayout(expected, JunctionTransform::Rotate);
	CHECK(SameLayout(ApplyJunctionOrientation(layout, 3, true), expected));

	CHECK(SameLayout(ApplyJunctionOrientation(layout, 4, false), ApplyJunctionOrientation(layout, 0, false)));
}

TEST_CASE("JunctionLayout outer: estimate reports the ground height")
{
	Map::Allocate(64, 64);

	StubJunctionPricing pricing;
	CHECK(EstimateJunction(MakeTestJunctionLayout(), TileXY(10, 10), pricing).height == 1);

	/* Every occupied tile: the track row, the extra track piece, the bridge heads and its span. */
	for (int x = 0; x < 8; x++) pricing.heights[TileXY(10 + x, 12)] = 3;
	for (int y : {0, 1, 2, 3, 4, 5}) pricing.heights[TileXY(14, 10 + y)] = 3;
	JunctionEstimate estimate = EstimateJunction(MakeTestJunctionLayout(), TileXY(10, 10), pricing);

	CHECK(estimate.height == 3);
	CHECK(estimate.affordable);
}

TEST_CASE("JunctionLayout orientation: OrientJunctionOffset and OrientJunctionVector match ApplyJunctionOrientation", "[junction]")
{
	JunctionLayout layout;
	layout.size = {5, 3};
	layout.tracks.push_back({{0, 0}, TrackBits{Track::X}});
	layout.tracks.push_back({{3, 1}, TrackBits{Track::Y}});
	layout.tracks.push_back({{4, 2}, TrackBits{Track::Upper}});

	for (uint8_t r = 0; r < 4; r++) {
		for (bool m : {false, true}) {
			const JunctionLayout oriented = ApplyJunctionOrientation(layout, r, m);
			REQUIRE(oriented.tracks.size() == layout.tracks.size());
			for (size_t i = 0; i < layout.tracks.size(); i++) {
				const TileIndexDiffC o = OrientJunctionOffset(layout.tracks[i].offset, layout.size, r, m);
				CHECK(o.x == oriented.tracks[i].offset.x);
				CHECK(o.y == oriented.tracks[i].offset.y);
			}
			for (const JunctionTrack &a : layout.tracks) {
				for (const JunctionTrack &b : layout.tracks) {
					const TileIndexDiffC oa = OrientJunctionOffset(a.offset, layout.size, r, m);
					const TileIndexDiffC ob = OrientJunctionOffset(b.offset, layout.size, r, m);
					const TileIndexDiffC v = OrientJunctionVector({static_cast<int16_t>(a.offset.x - b.offset.x), static_cast<int16_t>(a.offset.y - b.offset.y)}, r, m);
					CHECK(oa.x - ob.x == v.x);
					CHECK(oa.y - ob.y == v.y);
				}
			}
		}
	}
}
