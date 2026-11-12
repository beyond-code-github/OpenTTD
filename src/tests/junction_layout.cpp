/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_layout.cpp Test functionality from junction_layout. */

#include "../stdafx.h"

#include "../3rdparty/catch2/catch.hpp"

#include "../junction_game.h"
#include "../junction_generator.h"
#include "../junction_gui.h"
#include "../junction_hover.h"
#include "../junction_layout.h"
#include "../junction_terraform.h"
#include "../junction_terrain_json.h"
#include "../junction_topology.h"
#include "../map_func.h"
#include "../tile_map.h"
#include "../track_func.h"
#include "../table/strings.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <set>
#include <tuple>

#include "../safeguards.h"

/** Pricing stub with configurable failures. */
class StubJunctionPricing : public JunctionPricing {
public:
	std::set<TileIndex> not_flat;
	std::map<TileIndex, int> heights;
	std::map<TileIndex, StringID> rail_fails;
	std::map<TileIndex, Money> clear_costs;
	bool bridge_fails = false;
	bool tunnel_fails = false;
	Money money = 1000000;

	std::map<TileIndex, std::pair<int, int>> slope_zs; ///< Tile to (z_min, z_max), overriding the flat height.
	std::set<TileIndex> steep;
	bool build_on_slopes = true;

	std::optional<JunctionTileSlope> GetTileSlope(TileIndex tile) const override
	{
		if (tile == INVALID_TILE) return std::nullopt;
		auto it = this->heights.find(tile);
		int z_min = it != this->heights.end() ? it->second : 1;
		int z_max = z_min;
		if (auto sit = this->slope_zs.find(tile); sit != this->slope_zs.end()) std::tie(z_min, z_max) = sit->second;
		return JunctionTileSlope{z_min, z_max, this->not_flat.contains(tile) || this->steep.contains(tile), this->build_on_slopes};
	}

	CommandCost TestRail(TileIndex tile, [[maybe_unused]] Track track) const override
	{
		auto it = this->rail_fails.find(tile);
		if (it != this->rail_fails.end()) return CommandCost(it->second);
		return CommandCost(ExpensesType::Construction, 100);
	}

	CommandCost TestStation(TileIndex tile, [[maybe_unused]] Axis axis, [[maybe_unused]] uint8_t platforms, [[maybe_unused]] uint8_t length) const override
	{
		auto it = this->rail_fails.find(tile);
		if (it != this->rail_fails.end()) return CommandCost(it->second);
		return CommandCost(ExpensesType::Construction, 500);
	}

	CommandCost PriceBridge([[maybe_unused]] TileIndex start, [[maybe_unused]] TileIndex end) const override
	{
		if (this->bridge_fails) return CommandCost(STR_ERROR_CAN_T_BUILD_BRIDGE_HERE);
		return CommandCost(ExpensesType::Construction, 1000);
	}

	Money RailPrice([[maybe_unused]] Track track, bool foundation) const override { return foundation ? 130 : 100; }
	Money BridgePrice([[maybe_unused]] TileIndex start, [[maybe_unused]] TileIndex end) const override { return 1000; }

	CommandCost PriceTunnel([[maybe_unused]] TileIndex start, [[maybe_unused]] TileIndex end) const override
	{
		if (this->tunnel_fails) return CommandCost(STR_ERROR_CAN_T_BUILD_TUNNEL_HERE);
		return CommandCost(ExpensesType::Construction, 700);
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

TEST_CASE("JunctionLayout outer: plane rule with slopes")
{
	Map::Allocate(64, 64);
	const TileIndex t = TileXY(12, 12); ///< A track tile of the test layout.

	/* Flat at 0 everywhere. */
	{
		StubJunctionPricing pricing;
		for (int x = 0; x < 64; x++) for (int y = 0; y < 64; y++) pricing.heights[TileXY(x, y)] = 0;
		JunctionEstimate estimate = EstimateJunction(MakeTestJunctionLayout(), TileXY(10, 10), pricing);
		CHECK(estimate.bad_tiles.empty());
		CHECK(estimate.height == 0);
	}

	/* One tile z 0..1 among tiles flat at 1: accepted with build_on_slopes, rejected without. */
	{
		StubJunctionPricing pricing;
		pricing.slope_zs[t] = {0, 1};
		JunctionEstimate estimate = EstimateJunction(MakeTestJunctionLayout(), TileXY(10, 10), pricing);
		CHECK(estimate.bad_tiles.empty());
		CHECK(estimate.height == 1);

		pricing.build_on_slopes = false;
		estimate = EstimateJunction(MakeTestJunctionLayout(), TileXY(10, 10), pricing);
		CHECK(estimate.bad_tiles == std::vector<TileIndex>{t});
		CHECK(estimate.first_error == STR_ERROR_FLAT_LAND_REQUIRED);
	}

	/* A steep tile. */
	{
		StubJunctionPricing pricing;
		pricing.slope_zs[t] = {0, 1};
		pricing.steep.insert(t);
		JunctionEstimate estimate = EstimateJunction(MakeTestJunctionLayout(), TileXY(10, 10), pricing);
		CHECK(estimate.bad_tiles == std::vector<TileIndex>{t});
	}

	/* Mixed planes: a tile flat at 0 while H = 1. */
	{
		StubJunctionPricing pricing;
		pricing.heights[t] = 0;
		JunctionEstimate estimate = EstimateJunction(MakeTestJunctionLayout(), TileXY(10, 10), pricing);
		CHECK(estimate.bad_tiles == std::vector<TileIndex>{t});
		CHECK(estimate.first_error == STR_ERROR_FLAT_LAND_REQUIRED);
	}
}

TEST_CASE("JunctionLayoutToJson matches the rig layout format", "[junction]")
{
	CHECK(JunctionLayoutToJson(MakeTestJunctionLayout()) ==
		"{\"size\":[8,6],\n\"tracks\":["
		"\n{\"x\":0,\"y\":2,\"bits\":[\"X\"]},\n{\"x\":1,\"y\":2,\"bits\":[\"X\"]},"
		"\n{\"x\":2,\"y\":2,\"bits\":[\"X\"]},\n{\"x\":3,\"y\":2,\"bits\":[\"X\"]},"
		"\n{\"x\":4,\"y\":2,\"bits\":[\"X\"]},\n{\"x\":5,\"y\":2,\"bits\":[\"X\"]},"
		"\n{\"x\":6,\"y\":2,\"bits\":[\"X\"]},\n{\"x\":7,\"y\":2,\"bits\":[\"X\"]},"
		"\n{\"x\":4,\"y\":5,\"bits\":[\"Y\"]}],\n"
		"\"bridges\":[{\"start\":[4,0],\"end\":[4,4]}],\n"
		"\"signals\":["
		"\n{\"x\":1,\"y\":2,\"facing\":\"X_SW\",\"type\":\"block\"},"
		"\n{\"x\":6,\"y\":2,\"facing\":\"X_NE\",\"type\":\"block\"}]}\n");
}

TEST_CASE("JunctionLayoutToJson writes station blocks", "[junction]")
{
	JunctionLayout layout = MakeTestJunctionLayout();
	layout.stations.push_back({{1, 2}, Axis::Y, 3, 5});
	const std::string json = JunctionLayoutToJson(layout);
	CHECK(json.find("\"stations\":[{\"x\":1,\"y\":2,\"axis\":\"Y\",\"platforms\":3,\"length\":5}]") != std::string::npos);
	CHECK(JunctionLayoutToJson(MakeTestJunctionLayout()).find("stations") == std::string::npos);
}

TEST_CASE("ApplyJunctionOrientation rotates and mirrors station blocks", "[junction]")
{
	JunctionLayout layout = MakeTestJunctionLayout(); // size 8x6
	layout.stations.push_back({{1, 2}, Axis::X, 3, 4}); // covers x 1..4, y 2..4
	const JunctionLayout rotated = ApplyJunctionOrientation(layout, 1, false);
	REQUIRE(rotated.stations.size() == 1);
	CHECK(rotated.stations[0].axis == Axis::Y);
	CHECK(rotated.stations[0].platforms == 3);
	CHECK(rotated.stations[0].length == 4);
	/* Tracks rotate (x,y) -> (y, 7 - x): the block's corners (1,2) and (4,4) land at (2,6) and (4,3), so it covers x 2..4, y 3..6. */
	CHECK(rotated.stations[0].origin.x == 2);
	CHECK(rotated.stations[0].origin.y == 3);
	const JunctionLayout mirrored = ApplyJunctionOrientation(layout, 0, true);
	CHECK(mirrored.stations[0].axis == Axis::X);
	CHECK(mirrored.stations[0].origin.x == 3);
	CHECK(mirrored.stations[0].origin.y == 2);
}

TEST_CASE("InverseOrientJunctionVector undoes OrientJunctionVector", "[junction]")
{
	const TileIndexDiffC samples[] = {{0, 0}, {2, 0}, {-4, 2}, {3, -5}};
	for (const TileIndexDiffC &v : samples) {
		for (uint8_t r = 0; r < 4; r++) {
			for (bool m : {false, true}) {
				const TileIndexDiffC back = InverseOrientJunctionVector(OrientJunctionVector(v, r, m), r, m);
				CHECK(back.x == v.x);
				CHECK(back.y == v.y);
			}
		}
	}
}

/** A layout of one X line with a tunnel from (1,0) to (5,0). */
static JunctionLayout MakeTunnelLayout()
{
	JunctionLayout layout;
	layout.size = {7, 1};
	layout.tracks.push_back({{0, 0}, TrackBits{Track::X}});
	layout.tracks.push_back({{6, 0}, TrackBits{Track::X}});
	layout.bridges.push_back({{1, 0}, {5, 0}, true});
	return layout;
}

TEST_CASE("JunctionLayout: a tunnel gives a tunnel build step", "[junction]")
{
	Map::Allocate(64, 64);

	const std::vector<JunctionBuildStep> steps = MakeJunctionBuildSteps(MakeTunnelLayout(), TileXY(10, 10));
	REQUIRE(steps.size() == 3);
	REQUIRE(std::holds_alternative<JunctionBridgeStep>(steps[2]));
	const JunctionBridgeStep &step = std::get<JunctionBridgeStep>(steps[2]);
	CHECK(step.start == TileXY(11, 10));
	CHECK(step.end == TileXY(15, 10));
	CHECK(step.tunnel);

	const std::vector<JunctionBuildStep> bridge_steps = MakeJunctionBuildSteps(MakeTestJunctionLayout(), TileXY(10, 10));
	CHECK_FALSE(std::get<JunctionBridgeStep>(bridge_steps[9]).tunnel);
}

TEST_CASE("JunctionLayout: orientation keeps the tunnel flag", "[junction]")
{
	for (int rotations = 0; rotations < 4; rotations++) {
		for (bool mirror : {false, true}) {
			const JunctionLayout result = ApplyJunctionOrientation(MakeTunnelLayout(), rotations, mirror);
			REQUIRE(result.bridges.size() == 1);
			CHECK(result.bridges[0].tunnel);
		}
	}
}

TEST_CASE("JunctionLayoutToJson marks tunnels", "[junction]")
{
	const std::string json = JunctionLayoutToJson(MakeTunnelLayout());
	CHECK(json.find("\"bridges\":[{\"start\":[1,0],\"end\":[5,0],\"tunnel\":true}]") != std::string::npos);
	CHECK(JunctionLayoutToJson(MakeTestJunctionLayout()).find("tunnel") == std::string::npos);
}

TEST_CASE("JunctionLayout: a tunnel is priced as a tunnel, a bridge as a bridge", "[junction]")
{
	Map::Allocate(64, 64);

	StubJunctionPricing pricing;
	JunctionEstimate estimate = EstimateJunction(MakeTunnelLayout(), TileXY(10, 10), pricing);
	CHECK(estimate.total == 2 * 100 + 700);
	CHECK(estimate.bad_tiles.empty());

	pricing.tunnel_fails = true;
	estimate = EstimateJunction(MakeTunnelLayout(), TileXY(10, 10), pricing);
	CHECK(estimate.bad_tiles == std::vector<TileIndex>{TileXY(11, 10), TileXY(15, 10)});
	CHECK(estimate.first_error == STR_ERROR_CAN_T_BUILD_TUNNEL_HERE);

	JunctionLayout both = MakeTunnelLayout();
	both.size = {7, 3};
	both.bridges.push_back({{1, 2}, {5, 2}});
	pricing.tunnel_fails = false;
	CHECK(EstimateJunction(both, TileXY(10, 10), pricing).total == 2 * 100 + 700 + 1000);
}

/** The corner offsets of a tile from its own coordinates: W, S, E, N. */
static constexpr std::pair<int, int> TERRAFORM_CORNER_OFFSET[4] = {{1, 0}, {1, 1}, {0, 1}, {0, 0}};

/**
 * The terrain in a map frame: each cell's tile is anchor + O(cell); the lattice map is the affine map that sends the cell's square to that tile's square.
 * @return The corner height of each map lattice point the snapshot covers.
 */
static std::map<std::pair<int, int>, int> TerrainToMapLattice(const JunctionTerrain &t, TileIndex anchor, int hub_x, uint8_t rotation, bool mirror)
{
	std::map<std::pair<int, int>, int> heights;
	int min_x = 2, min_y = 2;
	for (int u = 0; u < 4; u++) {
		const TileIndexDiffC o = OrientJunctionVector({static_cast<int16_t>(u & 1), static_cast<int16_t>(u >> 1)}, rotation, mirror);
		min_x = std::min<int>(min_x, o.x);
		min_y = std::min<int>(min_y, o.y);
	}
	for (int y = t.origin.y; y < t.origin.y + t.height; y++) {
		for (int x = t.origin.x; x < t.origin.x + t.width; x++) {
			const TileIndexDiffC image = OrientJunctionVector({static_cast<int16_t>(x - hub_x), static_cast<int16_t>(y)}, rotation, mirror);
			const int tx = static_cast<int>(TileX(anchor)) + image.x;
			const int ty = static_cast<int>(TileY(anchor)) + image.y;
			const std::array<int, 4> corners = JunctionTileCorners(*t.At(x, y));
			for (int i = 0; i < 4; i++) {
				const TileIndexDiffC o = OrientJunctionVector({static_cast<int16_t>(TERRAFORM_CORNER_OFFSET[i].first), static_cast<int16_t>(TERRAFORM_CORNER_OFFSET[i].second)}, rotation, mirror);
				const std::pair<int, int> lattice{tx + o.x - min_x, ty + o.y - min_y};
				const auto [it, inserted] = heights.emplace(lattice, corners[i]);
				if (!inserted) REQUIRE(it->second == corners[i]);
			}
		}
	}
	return heights;
}

/** Apply terraform steps corner by corner to a lattice map; each step moves one corner one level, and the game's propagation must then have nothing to move. */
static void SimulateTerraformSteps(std::map<std::pair<int, int>, int> &map_heights, const std::vector<JunctionTerraformStep> &steps)
{
	for (const JunctionTerraformStep &step : steps) {
		const std::pair<int, int> lattice{static_cast<int>(TileX(step.tile)) + TERRAFORM_CORNER_OFFSET[to_underlying(step.corner)].first, static_cast<int>(TileY(step.tile)) + TERRAFORM_CORNER_OFFSET[to_underlying(step.corner)].second};
		REQUIRE(map_heights.contains(lattice));
		const int h = map_heights[lattice] + (step.raise ? 1 : -1);
		map_heights[lattice] = h;
		for (const auto &[dx, dy] : {std::pair{1, 0}, std::pair{0, 1}, std::pair{-1, 0}, std::pair{0, -1}}) {
			const auto it = map_heights.find({lattice.first + dx, lattice.second + dy});
			if (it != map_heights.end()) REQUIRE(std::abs(it->second - h) <= 1);
		}
	}
}

TEST_CASE("terraform steps: every orientation", "[junction][hoverplan]")
{
	Map::Allocate(64, 64);

	/* A 16 x 14 snapshot, ground rising along x from corner column 8, every tile Buildable. */
	auto ground = [](int cx) { return 3 + std::clamp(cx - 8, 0, 2); };
	JunctionTerrain terrain;
	terrain.origin = {-3, -2};
	terrain.width = 16;
	terrain.height = 14;
	terrain.terraform_cost = 100;
	terrain.tiles.resize(static_cast<size_t>(terrain.width) * terrain.height);
	for (int y = terrain.origin.y; y < terrain.origin.y + terrain.height; y++) {
		for (int x = terrain.origin.x; x < terrain.origin.x + terrain.width; x++) {
			SetJunctionTileCorners(terrain.tiles[static_cast<size_t>(y - terrain.origin.y) * terrain.width + (x - terrain.origin.x)], {ground(x + 1), ground(x + 1), ground(x), ground(x)});
		}
	}
	JunctionTerraform plan;
	plan.corners[{5, 5}] = 5; // Two up on flat ground.
	plan.corners[{9, 6}] = 2; // Two down on the slope.
	std::string why;
	REQUIRE(PropagateJunctionTerraform(terrain, plan, &why));
	INFO(why);
	REQUIRE(!plan.corners.empty());
	const JunctionTerrain after = ApplyJunctionTerraform(terrain, plan);

	constexpr int HUB_X = 8;
	const TileIndex anchor = TileXY(30, 30);
	for (int mirror = 0; mirror < 2; mirror++) {
		for (uint8_t rotation = 0; rotation < 4; rotation++) {
			INFO("rotation " << int(rotation) << " mirrored " << mirror);
			std::map<std::pair<int, int>, int> map_heights = TerrainToMapLattice(terrain, anchor, HUB_X, rotation, mirror != 0);
			const std::map<std::pair<int, int>, int> expected = TerrainToMapLattice(after, anchor, HUB_X, rotation, mirror != 0);

			const std::vector<JunctionTerraformStep> steps = MakeJunctionTerraformSteps(plan, terrain, anchor, rotation, mirror != 0, HUB_X);
			int moves = 0;
			for (const auto &[corner, target] : plan.corners) moves += std::abs(target - JunctionTileCorners(*terrain.At(corner.first, corner.second))[3]);
			CHECK(static_cast<int>(steps.size()) == moves);

			SimulateTerraformSteps(map_heights, steps);
			CHECK(map_heights == expected);
		}
	}
}

TEST_CASE("hover job: snapshot, plan and steps on real terrain", "[junction][hoverplan]")
{
	namespace fs = std::filesystem;
	Map::Allocate(256, 256);

	/* The hover job's frame (H2): the seed's box with the hub column in the middle, the plan at the snapshot's canonical (0, 0). */
	const JunctionSpec spec = MakeTestJunctionSpec();
	std::string error;
	const JunctionTopology &t = JunctionTTopology();
	const JunctionGraphSeed seed = MakeJunctionGraphSeed(t, spec, &error);
	INFO(error);
	REQUIRE(!seed.routes.empty());
	const int hub_x = seed.placement.size.x / 2;
	const TileIndex anchor = TileXY(120, 120);

	std::vector<fs::path> files;
	for (const fs::directory_entry &entry : fs::directory_iterator(JUNCTION_TERRAIN_DIR)) {
		if (entry.path().extension() == ".json") files.push_back(entry.path());
	}
	std::ranges::sort(files);

	int checked = 0;
	for (const fs::path &file : files) {
		if (checked == 3) break;
		std::ifstream in(file);
		std::stringstream text;
		text << in.rdbuf();
		std::string load_error;
		std::optional<JunctionTerrain> loaded = JunctionTerrainFromJson(text.str(), &load_error);
		REQUIRE(loaded.has_value());
		/* The fixture as the job's snapshot: canonical (0, 0) is the box's first tile, so the origin shifts to where the fixture centres the box. */
		JunctionTerrain terrain = std::move(*loaded);
		const TileIndexDiffC centred = {static_cast<int16_t>((terrain.width - seed.placement.size.x) / 2), static_cast<int16_t>((terrain.height - seed.placement.size.y) / 2)};
		terrain.origin = {static_cast<int16_t>(-centred.x), static_cast<int16_t>(-centred.y)};
		const JunctionHoverPlan plan = PlanJunctionHover(seed, spec, terrain, {0, 0});
		if (!plan.layout.has_value() || plan.terraform.corners.empty()) continue;
		checked++;
		INFO(file.filename().string());
		const JunctionTerrain after = ApplyJunctionTerraform(terrain, plan.terraform);

		for (int mirror = 0; mirror < 2; mirror++) {
			for (uint8_t rotation = 0; rotation < 4; rotation++) {
				INFO("rotation " << int(rotation) << " mirrored " << mirror);
				std::map<std::pair<int, int>, int> map_heights = TerrainToMapLattice(terrain, anchor, hub_x, rotation, mirror != 0);
				const std::map<std::pair<int, int>, int> expected = TerrainToMapLattice(after, anchor, hub_x, rotation, mirror != 0);
				const std::vector<JunctionTerraformStep> steps = MakeJunctionTerraformSteps(plan.terraform, terrain, anchor, rotation, mirror != 0, hub_x);
				CHECK(!steps.empty());
				/* Every step's corner lies in the snapshot (the simulation requires it), and the result is the plan. */
				SimulateTerraformSteps(map_heights, steps);
				CHECK(map_heights == expected);

				/* The ghost's frame: the layout, oriented and placed at its origin, lies on the same tiles as the canonical layout through the snapshot's mapping. */
				const JunctionLayout oriented = ApplyJunctionOrientation(*plan.layout, rotation, mirror != 0);
				const TileIndexDiffC hub = {static_cast<int16_t>(hub_x), 0};
				const TileIndexDiffC offset = OrientJunctionOffset(hub, plan.layout->size, rotation, mirror != 0);
				const TileIndexDiffC origin = {static_cast<int16_t>(static_cast<int>(TileX(anchor)) - offset.x), static_cast<int16_t>(static_cast<int>(TileY(anchor)) - offset.y)};
				std::set<std::pair<int, int>> ghost_tiles;
				for (const JunctionTrack &track : oriented.tracks) ghost_tiles.insert({origin.x + track.offset.x, origin.y + track.offset.y});
				std::set<std::pair<int, int>> mapped_tiles;
				for (const JunctionTrack &track : plan.layout->tracks) {
					const TileIndexDiffC image = OrientJunctionVector({static_cast<int16_t>(track.offset.x - hub_x), static_cast<int16_t>(track.offset.y)}, rotation, mirror != 0);
					mapped_tiles.insert({static_cast<int>(TileX(anchor)) + image.x, static_cast<int>(TileY(anchor)) + image.y});
				}
				CHECK(ghost_tiles == mapped_tiles);
			}
		}
	}
	CHECK(checked == 3);
}

TEST_CASE("the hover plan carries a level for every track tile", "[junction][hoverplan]")
{
	namespace fs = std::filesystem;
	const JunctionSpec spec = MakeTestJunctionSpec();
	std::string error;
	const JunctionGraphSeed seed = MakeJunctionGraphSeed(JunctionTTopology(), spec, &error);
	INFO(error);
	REQUIRE(!seed.routes.empty());

	std::vector<fs::path> files;
	for (const fs::directory_entry &entry : fs::directory_iterator(JUNCTION_TERRAIN_DIR)) {
		if (entry.path().extension() == ".json") files.push_back(entry.path());
	}
	std::ranges::sort(files);

	int checked = 0;
	for (const fs::path &file : files) {
		if (checked == 3) break;
		std::ifstream in(file);
		std::stringstream text;
		text << in.rdbuf();
		std::string load_error;
		std::optional<JunctionTerrain> loaded = JunctionTerrainFromJson(text.str(), &load_error);
		REQUIRE(loaded.has_value());
		JunctionTerrain terrain = std::move(*loaded);
		const TileIndexDiffC centred = {static_cast<int16_t>((terrain.width - seed.placement.size.x) / 2), static_cast<int16_t>((terrain.height - seed.placement.size.y) / 2)};
		terrain.origin = {static_cast<int16_t>(-centred.x), static_cast<int16_t>(-centred.y)};
		const JunctionHoverPlan plan = PlanJunctionHover(seed, spec, terrain, {0, 0});
		if (!plan.layout.has_value() || plan.terraform.corners.empty()) continue;
		checked++;
		INFO(file.filename().string());
		const JunctionTerrain after = ApplyJunctionTerraform(terrain, plan.terraform);

		/* Every track tile of the layout (bridge heads included) has exactly one level. */
		std::map<std::pair<int, int>, TrackBits> tiles;
		for (const JunctionTrack &track : plan.layout->tracks) tiles[{track.offset.x, track.offset.y}] |= track.tracks;
		for (const JunctionBridge &bridge : plan.layout->bridges) {
			const TrackBits axis_track{bridge.start.y == bridge.end.y ? Track::X : Track::Y};
			tiles[{bridge.start.x, bridge.start.y}] |= axis_track;
			tiles[{bridge.end.x, bridge.end.y}] |= axis_track;
		}
		std::map<std::pair<int, int>, int> levels;
		for (const auto &[offset, level] : plan.track_levels) CHECK(levels.emplace(std::make_pair<int, int>(offset.x, offset.y), level).second);
		CHECK(levels.size() == plan.track_levels.size());
		for (const auto &[tile, bits] : tiles) {
			INFO("tile " << tile.first << ", " << tile.second);
			const auto found = levels.find(tile);
			REQUIRE(found != levels.end());
			const int level = found->second;
			const JunctionTerrainTile *ground = after.At(tile.first, tile.second);
			REQUIRE(ground != nullptr);
			if (ground->z_min == ground->z_max) {
				CHECK(ground->z_min == level);
			} else if (ground->z_max == level) {
				/* A foundation on a slope, or a ramp that ends at the level. */
				CHECK(ground->z_max - ground->z_min <= 2);
			} else {
				/* A ramp that starts at the level: one level up, inclined along the piece. */
				CHECK(ground->z_min == level);
				CHECK(ground->z_max == level + 1);
				CHECK(!ground->steep);
				const bool along = bits == TrackBits{Track::X} ? (ground->incline == DiagDirection::NE || ground->incline == DiagDirection::SW) : (bits == TrackBits{Track::Y} && (ground->incline == DiagDirection::NW || ground->incline == DiagDirection::SE));
				CHECK(along);
			}
		}
		for (const auto &[offset, level] : levels) CHECK(tiles.contains(offset));
	}
	CHECK(checked == 3);
}

TEST_CASE("the ghost has the level and ground it draws signals and planned ground at", "[junction][hoverplan]")
{
	namespace fs = std::filesystem;
	Map::Allocate(256, 256);
	const JunctionSpec spec = MakeTestJunctionSpec();
	std::string error;
	const JunctionGraphSeed seed = MakeJunctionGraphSeed(JunctionTTopology(), spec, &error);
	INFO(error);
	REQUIRE(!seed.routes.empty());
	const int hub_x = seed.placement.size.x / 2;
	const TileIndex anchor = TileXY(120, 120);

	std::vector<fs::path> files;
	for (const fs::directory_entry &entry : fs::directory_iterator(JUNCTION_TERRAIN_DIR)) {
		if (entry.path().extension() == ".json") files.push_back(entry.path());
	}
	std::ranges::sort(files);

	int checked = 0;
	for (const fs::path &file : files) {
		if (checked == 3) break;
		std::ifstream in(file);
		std::stringstream text;
		text << in.rdbuf();
		std::string load_error;
		std::optional<JunctionTerrain> loaded = JunctionTerrainFromJson(text.str(), &load_error);
		REQUIRE(loaded.has_value());
		JunctionTerrain terrain = std::move(*loaded);
		const TileIndexDiffC centred = {static_cast<int16_t>((terrain.width - seed.placement.size.x) / 2), static_cast<int16_t>((terrain.height - seed.placement.size.y) / 2)};
		terrain.origin = {static_cast<int16_t>(-centred.x), static_cast<int16_t>(-centred.y)};
		const JunctionHoverPlan plan = PlanJunctionHover(seed, spec, terrain, {0, 0});
		if (!plan.layout.has_value() || plan.terraform.corners.empty()) continue;
		checked++;
		INFO(file.filename().string());
		const JunctionTerrain after = ApplyJunctionTerraform(terrain, plan.terraform);

		/* The map holds the snapshot's heights, and the ghost is set with the steps the hover would give it (no rotation, no mirror). */
		for (const auto &[lattice, height] : TerrainToMapLattice(terrain, anchor, hub_x, 0, false)) SetTileHeight(TileXY(lattice.first, lattice.second), height);
		const std::vector<JunctionTerraformStep> steps = MakeJunctionTerraformSteps(plan.terraform, terrain, anchor, 0, false, hub_x);
		std::vector<TileIndex> terraform_tiles;
		for (const JunctionTerraformStep &step : steps) {
			for (int dy = -1; dy <= 0; dy++) {
				for (int dx = -1; dx <= 0; dx++) terraform_tiles.push_back(TileXY(static_cast<int>(TileX(step.tile)) + dx, static_cast<int>(TileY(step.tile)) + dy));
			}
		}
		std::ranges::sort(terraform_tiles);
		terraform_tiles.erase(std::ranges::unique(terraform_tiles).begin(), terraform_tiles.end());
		REQUIRE(!terraform_tiles.empty());

		const TileIndexDiffC origin = {static_cast<int16_t>(static_cast<int>(TileX(anchor)) - hub_x), static_cast<int16_t>(TileY(anchor))};
		const TileIndex origin_tile = TileXY(origin.x, origin.y);
		JunctionGhost::Set(*plan.layout, origin_tile, 0, RAILTYPE_RAIL, {}, JunctionEstimate{}, terraform_tiles, steps, plan.track_levels);

		/* Every signal lies on a tile that has a level, and the ghost keeps the signals and the planned ground to draw them on. */
		std::set<std::pair<int, int>> level_tiles;
		for (const auto &[offset, level] : plan.track_levels) level_tiles.insert({offset.x, offset.y});
		CHECK(!plan.layout->signals.empty());
		for (const JunctionSignal &signal : plan.layout->signals) {
			INFO("signal " << signal.offset.x << ", " << signal.offset.y);
			CHECK(level_tiles.contains({signal.offset.x, signal.offset.y}));
			const JunctionGhostTile *ghost = JunctionGhost::Get(TileXY(origin.x + signal.offset.x, origin.y + signal.offset.y));
			REQUIRE(ghost != nullptr);
			CHECK(ghost->leveled);
			CHECK(!ghost->signals.empty());
		}

		/* Every tile with a planned ground change and no track has the plan's slope and height. */
		int ground_only = 0;
		for (TileIndex tile : terraform_tiles) {
			const JunctionGhostTile *ghost = JunctionGhost::Get(tile);
			REQUIRE(ghost != nullptr);
			if (ghost->tracks.Any() || ghost->head.has_value()) continue;
			ground_only++;
			INFO("ground tile " << TileX(tile) << ", " << TileY(tile));
			const JunctionTerrainTile *planned = after.At(static_cast<int>(TileX(tile)) - origin.x, static_cast<int>(TileY(tile)) - origin.y);
			REQUIRE(planned != nullptr);
			CHECK(ghost->leveled);
			CHECK(ghost->slope == planned->slope);
			CHECK(ghost->z == planned->z_min * static_cast<int>(TILE_HEIGHT));
		}
		CHECK(ground_only > 0);
		JunctionGhost::Clear();
	}
	CHECK(checked == 3);
}

TEST_CASE("the terraformed estimate adds the height cost", "[junction][hoverplan]")
{
	Map::Allocate(64, 64);

	StubJunctionPricing pricing;
	const JunctionLayout layout = MakeTestJunctionLayout();
	const JunctionEstimate flat = EstimateJunction(layout, TileXY(10, 10), pricing);

	/* Ground that is not in one plane: the plane check marks it bad, the terraformed estimate does not (the plan levels it). */
	pricing.slope_zs[TileXY(12, 12)] = {1, 2};
	pricing.heights[TileXY(11, 12)] = 3;
	CHECK(!EstimateJunction(layout, TileXY(10, 10), pricing).bad_tiles.empty());
	const JunctionEstimate terraformed = EstimateJunctionTerraformed(layout, TileXY(10, 10), pricing, 777, {});
	CHECK(terraformed.bad_tiles.empty());
	CHECK(terraformed.total == flat.total + 777);
	CHECK(terraformed.affordable);

	pricing.money = flat.total + 776;
	CHECK(!EstimateJunctionTerraformed(layout, TileXY(10, 10), pricing, 777, {}).affordable);
}

TEST_CASE("the cone reject is sound", "[junction][hoverplan]")
{
	namespace fs = std::filesystem;
	const JunctionSpec spec = MakeTestJunctionSpec();
	std::string error;
	const JunctionTopology &t = JunctionTTopology();
	const JunctionGraphSeed seed = MakeJunctionGraphSeed(t, spec, &error);
	INFO(error);
	REQUIRE(!seed.routes.empty());

	/* Two offsets the sweep rejects by the cone: a tile with no option that keeps the ground the terraform must keep, and tile levels that cannot meet. */
	struct Case {
		const char *file;
		int dx; ///< Offset from the centred placement.
		int dy;
		const char *why; ///< The start of the reason.
	};
	const Case cases[] = {
		{"arctic-hilly-12.json", -2, 3, "tile (159, 54) cannot reach the ground the terraform must keep"},
		{"arctic-hilly-14.json", 4, 2, "the tiles around (28, 231) cannot meet: they cannot reach the ground the terraform must keep"},
	};
	for (const Case &test : cases) {
		std::ifstream in(fs::path(JUNCTION_TERRAIN_DIR) / test.file);
		std::stringstream text;
		text << in.rdbuf();
		std::string load_error;
		const std::optional<JunctionTerrain> terrain = JunctionTerrainFromJson(text.str(), &load_error);
		REQUIRE(terrain.has_value());
		const TileIndexDiffC centred = {static_cast<int16_t>(terrain->origin.x + (terrain->width - seed.placement.size.x) / 2 + test.dx), static_cast<int16_t>(terrain->origin.y + (terrain->height - seed.placement.size.y) / 2 + test.dy)};
		const JunctionHoverPlan plan = PlanJunctionHover(seed, spec, *terrain, centred);
		INFO(test.file << ": " << plan.why);
		CHECK(!plan.layout.has_value());
		CHECK(plan.why.find(test.why) != std::string::npos);
	}
}

TEST_CASE("the hover moves the box only when the cursor spot fails", "[junction][hoverplan]")
{
	namespace fs = std::filesystem;
	const JunctionSpec spec = MakeTestJunctionSpec();
	std::string error;
	const JunctionTopology &t = JunctionTTopology();
	const JunctionGraphSeed seed = MakeJunctionGraphSeed(t, spec, &error);
	INFO(error);
	REQUIRE(!seed.routes.empty());

	const auto load = [](const char *file) {
		std::ifstream in(fs::path(JUNCTION_TERRAIN_DIR) / file);
		std::stringstream text;
		text << in.rdbuf();
		std::string load_error;
		std::optional<JunctionTerrain> terrain = JunctionTerrainFromJson(text.str(), &load_error);
		REQUIRE(terrain.has_value());
		return *terrain;
	};
	const auto centred = [&](const JunctionTerrain &terrain) {
		return TileIndexDiffC{static_cast<int16_t>(terrain.origin.x + (terrain.width - seed.placement.size.x) / 2), static_cast<int16_t>(terrain.origin.y + (terrain.height - seed.placement.size.y) / 2)};
	};
	const auto same_layout = [](const JunctionLayout &a, const JunctionLayout &b) {
		return std::ranges::equal(a.tracks, b.tracks, [](const JunctionTrack &x, const JunctionTrack &y) { return x.offset.x == y.offset.x && x.offset.y == y.offset.y && x.tracks == y.tracks; }) &&
				std::ranges::equal(a.signals, b.signals, [](const JunctionSignal &x, const JunctionSignal &y) { return x.offset.x == y.offset.x && x.offset.y == y.offset.y && x.facing == y.facing && x.type == y.type && x.variant == y.variant; });
	};

	/* (a) A fixture that plans at its offset: no move, the same layout as the plain hover. */
	{
		const JunctionTerrain terrain = load("arctic-hilly-2.json");
		const TileIndexDiffC offset = centred(terrain);
		const JunctionHoverPlan plain = PlanJunctionHover(seed, spec, terrain, offset);
		REQUIRE(plain.layout.has_value());
		const JunctionHoverPlan near = PlanJunctionHoverNear(seed, spec, terrain, offset);
		REQUIRE(near.layout.has_value());
		CHECK(near.moved.x == 0);
		CHECK(near.moved.y == 0);
		CHECK(same_layout(*near.layout, *plain.layout));
	}

	/* (b) A fixture that fails at its offset: it moves, the plain hover at the moved offset gives the same layout, and every earlier offset of the search order fails. */
	{
		const JunctionTerrain terrain = load("arctic-hilly-1.json");
		const TileIndexDiffC offset = centred(terrain);
		REQUIRE(!PlanJunctionHover(seed, spec, terrain, offset).layout.has_value());
		const JunctionHoverPlan near = PlanJunctionHoverNear(seed, spec, terrain, offset);
		REQUIRE(near.layout.has_value());
		CHECK((near.moved.x != 0 || near.moved.y != 0));
		const JunctionHoverPlan there = PlanJunctionHover(seed, spec, terrain, {static_cast<int16_t>(offset.x + near.moved.x), static_cast<int16_t>(offset.y + near.moved.y)});
		REQUIRE(there.layout.has_value());
		CHECK(same_layout(*near.layout, *there.layout));

		/* The search order: |dx| + |dy|, then |dy|, then dx, then dy. */
		const auto before = [&](int dx, int dy) {
			const int a = std::abs(dx) + std::abs(dy), b = std::abs(near.moved.x) + std::abs(near.moved.y);
			if (a != b) return a < b;
			if (std::abs(dy) != std::abs(near.moved.y)) return std::abs(dy) < std::abs(near.moved.y);
			if (dx != near.moved.x) return dx < near.moved.x;
			return dy < near.moved.y;
		};
		int earlier = 0;
		for (int dy = -JUNCTION_HOVER_NEAR_ROWS; dy <= JUNCTION_HOVER_NEAR_ROWS; dy++) {
			for (int dx = -JUNCTION_HOVER_NEAR_COLUMNS; dx <= JUNCTION_HOVER_NEAR_COLUMNS; dx++) {
				if ((dx == 0 && dy == 0) || !before(dx, dy)) continue;
				earlier++;
				INFO("offset " << dx << "," << dy);
				CHECK(!PlanJunctionHover(seed, spec, terrain, {static_cast<int16_t>(offset.x + dx), static_cast<int16_t>(offset.y + dy)}).layout.has_value());
			}
		}
		CHECK(earlier > 0);
	}
}

TEST_CASE("a cancelled near search stops at once", "[junction][hoverplan]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	std::string error;
	const JunctionGraphSeed seed = MakeJunctionGraphSeed(JunctionTTopology(), spec, &error);
	INFO(error);
	REQUIRE(!seed.routes.empty());
	std::ifstream in(std::filesystem::path(JUNCTION_TERRAIN_DIR) / "arctic-hilly-1.json");
	std::stringstream text;
	text << in.rdbuf();
	std::string load_error;
	std::optional<JunctionTerrain> terrain = JunctionTerrainFromJson(text.str(), &load_error);
	REQUIRE(terrain.has_value());
	const TileIndexDiffC offset = {static_cast<int16_t>(terrain->origin.x + (terrain->width - seed.placement.size.x) / 2), static_cast<int16_t>(terrain->origin.y + (terrain->height - seed.placement.size.y) / 2)};
	REQUIRE(!PlanJunctionHover(seed, spec, *terrain, offset).layout.has_value()); // It fails at its offset, so the near search runs.

	const std::atomic<bool> cancel{true};
	const auto start = std::chrono::steady_clock::now();
	const JunctionHoverPlan plan = PlanJunctionHoverNear(seed, spec, *terrain, offset, &cancel);
	const auto elapsed = std::chrono::steady_clock::now() - start;
	CHECK(!plan.layout.has_value());
	CHECK(plan.why == "cancelled");
#ifdef NDEBUG
	CHECK(elapsed < std::chrono::milliseconds(1));
#endif
	(void)elapsed;
}

TEST_CASE("the terraform plan is the game's", "[junction][hoverplan]")
{
	namespace fs = std::filesystem;
	Map::Allocate(256, 256);
	const JunctionSpec spec = MakeTestJunctionSpec();
	std::string error;
	const JunctionGraphSeed seed = MakeJunctionGraphSeed(JunctionTTopology(), spec, &error);
	INFO(error);
	REQUIRE(!seed.routes.empty());
	const int hub_x = seed.placement.size.x / 2;
	const TileIndex anchor = TileXY(120, 120);

	std::vector<fs::path> files;
	for (const fs::directory_entry &entry : fs::directory_iterator(JUNCTION_TERRAIN_DIR)) {
		if (entry.path().extension() == ".json") files.push_back(entry.path());
	}
	std::ranges::sort(files);

	/* Every fixture that plans at its centred offset: the steps the game would run, corner by corner, end at exactly the plan's corners. */
	int planned = 0;
	for (const fs::path &file : files) {
		std::ifstream in(file);
		std::stringstream text;
		text << in.rdbuf();
		std::string load_error;
		std::optional<JunctionTerrain> loaded = JunctionTerrainFromJson(text.str(), &load_error);
		REQUIRE(loaded.has_value());
		JunctionTerrain terrain = std::move(*loaded);
		const TileIndexDiffC centred = {static_cast<int16_t>((terrain.width - seed.placement.size.x) / 2), static_cast<int16_t>((terrain.height - seed.placement.size.y) / 2)};
		terrain.origin = {static_cast<int16_t>(-centred.x), static_cast<int16_t>(-centred.y)};
		const JunctionHoverPlan plan = PlanJunctionHover(seed, spec, terrain, {0, 0});
		if (!plan.layout.has_value()) continue;
		planned++;
		INFO(file.filename().string());
		const JunctionTerrain after = ApplyJunctionTerraform(terrain, plan.terraform);
		for (const auto &[rotation, mirror] : {std::pair<uint8_t, bool>{0, false}, std::pair<uint8_t, bool>{3, true}}) {
			INFO("rotation " << int(rotation) << " mirrored " << mirror);
			std::map<std::pair<int, int>, int> map_heights = TerrainToMapLattice(terrain, anchor, hub_x, rotation, mirror);
			const std::map<std::pair<int, int>, int> expected = TerrainToMapLattice(after, anchor, hub_x, rotation, mirror);
			const std::vector<JunctionTerraformStep> steps = MakeJunctionTerraformSteps(plan.terraform, terrain, anchor, rotation, mirror, hub_x);
			SimulateTerraformSteps(map_heights, steps);
			CHECK(map_heights == expected);
		}
	}
	CHECK(planned >= 19);
}

TEST_CASE("the clamp equals propagation where propagation succeeds", "[junction][terraform]")
{
	namespace fs = std::filesystem;
	Map::Allocate(256, 256);
	const JunctionSpec spec = MakeTestJunctionSpec();
	std::string error;
	const JunctionGraphSeed seed = MakeJunctionGraphSeed(JunctionTTopology(), spec, &error);
	INFO(error);
	REQUIRE(!seed.routes.empty());

	std::vector<fs::path> files;
	for (const fs::directory_entry &entry : fs::directory_iterator(JUNCTION_TERRAIN_DIR)) {
		if (entry.path().extension() == ".json") files.push_back(entry.path());
	}
	std::ranges::sort(files);

	int checked = 0;
	for (const fs::path &file : files) {
		if (checked == 3) break;
		std::ifstream in(file);
		std::stringstream text;
		text << in.rdbuf();
		std::string load_error;
		std::optional<JunctionTerrain> loaded = JunctionTerrainFromJson(text.str(), &load_error);
		REQUIRE(loaded.has_value());
		JunctionTerrain terrain = std::move(*loaded);
		const TileIndexDiffC centred = {static_cast<int16_t>((terrain.width - seed.placement.size.x) / 2), static_cast<int16_t>((terrain.height - seed.placement.size.y) / 2)};
		terrain.origin = {static_cast<int16_t>(-centred.x), static_cast<int16_t>(-centred.y)};
		const JunctionHoverPlan plan = PlanJunctionHover(seed, spec, terrain, {0, 0});
		if (!plan.layout.has_value() || plan.terraform.corners.empty()) continue;
		INFO(file.filename().string());
		const JunctionTerrain after = ApplyJunctionTerraform(terrain, plan.terraform);

		/* The corners of every track tile after the plan are fixed; propagation of those demands must move what the clamp moved, at the same cost. */
		std::set<std::pair<int, int>> tiles;
		for (const JunctionTrack &track : plan.layout->tracks) tiles.insert({track.offset.x, track.offset.y});
		for (const JunctionBridge &bridge : plan.layout->bridges) {
			if (bridge.tunnel) {
				tiles.insert({bridge.start.x, bridge.start.y});
				tiles.insert({bridge.end.x, bridge.end.y});
				continue;
			}
			/* The span's ground is held by the clamp too (IG3): the middle tiles' corners are demands as well. */
			const int dx = (bridge.end.x > bridge.start.x) - (bridge.end.x < bridge.start.x);
			const int dy = (bridge.end.y > bridge.start.y) - (bridge.end.y < bridge.start.y);
			for (TileIndexDiffC t = bridge.start; t.x != bridge.end.x + dx || t.y != bridge.end.y + dy; t = {static_cast<int16_t>(t.x + dx), static_cast<int16_t>(t.y + dy)}) tiles.insert({t.x, t.y});
		}
		JunctionTerraform demand;
		std::set<std::pair<int, int>> fixed;
		for (const auto &[x, y] : tiles) {
			const std::array<int, 4> g = JunctionTileCorners(*terrain.At(x, y));
			const std::array<int, 4> a = JunctionTileCorners(*after.At(x, y));
			for (int i = 0; i < 4; i++) {
				const std::pair<int, int> corner{x + TERRAFORM_CORNER_OFFSET[i].first, y + TERRAFORM_CORNER_OFFSET[i].second};
				fixed.insert(corner);
				if (a[i] != g[i]) demand.corners[corner] = a[i];
			}
		}
		std::string why;
		if (!PropagateJunctionTerraform(terrain, demand, &why, &fixed)) continue;
		checked++;
		INFO(why);
		CHECK(demand.corners == plan.terraform.corners);
		CHECK(demand.cost == plan.terraform.cost);
	}
	CHECK(checked == 3);
}
