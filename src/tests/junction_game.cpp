/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_game.cpp Test functionality from junction_game. */

#include "../stdafx.h"

#include "../3rdparty/catch2/catch.hpp"

#include "../junction_game.h"
#include "../map_func.h"
#include "../tile_map.h"
#include "../rail.h"
#include "../settings_type.h"
#include "../slope_func.h"
#include "../bridge.h"
#include "../economy_func.h"
#include "../timer/timer_game_calendar.h"

#include <chrono>

#include "../safeguards.h"

extern RailTypeInfo _railtypes[RAILTYPE_END];

TEST_CASE("MakeJunctionSpec: reads settings and the rail type")
{
	const uint8_t old_sd = _settings_client.gui.drag_signals_density;
	const uint8_t old_signal = _settings_client.gui.junction_signal_type;
	const AccelerationModel old_model = _settings_game.vehicle.train_acceleration_model;
	const uint8_t old_curve_speed = _railtypes[RAILTYPE_RAIL].curve_speed;
	const uint16_t old_max_speed = _railtypes[RAILTYPE_RAIL].max_speed;

	_settings_client.gui.drag_signals_density = 7;
	_railtypes[RAILTYPE_RAIL].curve_speed = 3;
	_railtypes[RAILTYPE_RAIL].max_speed = 120;

	_settings_client.gui.junction_signal_type = 0;
	_settings_game.vehicle.train_acceleration_model = AccelerationModel::Realistic;
	JunctionSpec spec = MakeJunctionSpec(RAILTYPE_RAIL, 2, 3, 5, MakeTestJunctionSpec().train);
	CHECK(spec.ml_gap == 2);
	CHECK(spec.branch_gap == 3);
	CHECK(spec.tl == 5);
	CHECK(spec.train == MakeTestJunctionSpec().train);
	CHECK(spec.prio_tiles == 0);
	CHECK(spec.sd == 7);
	CHECK(spec.curve_speed == 3);
	CHECK(spec.curve_limit);
	CHECK(spec.signal_type == SignalType::Block);
	CHECK(spec.reference_speed == 120);
	CHECK(GetJunctionReferenceSpeed(RAILTYPE_RAIL) == 120);

	_settings_client.gui.junction_signal_type = 1;
	_settings_game.vehicle.train_acceleration_model = AccelerationModel::Original;
	spec = MakeJunctionSpec(RAILTYPE_RAIL, 1, 1, 4, MakeTestJunctionSpec().train);
	CHECK(spec.signal_type == SignalType::PathOneWay);
	CHECK_FALSE(spec.curve_limit);

	_settings_client.gui.drag_signals_density = old_sd;
	_settings_client.gui.junction_signal_type = old_signal;
	_settings_game.vehicle.train_acceleration_model = old_model;
	_railtypes[RAILTYPE_RAIL].curve_speed = old_curve_speed;
	_railtypes[RAILTYPE_RAIL].max_speed = old_max_speed;
}

TEST_CASE("MakeJunctionSpec: bridge closures are precomputed for 1..10 middle tiles")
{
	const BridgeSpec old_bridge = _bridge[0];
	const uint16_t old_max_speed = _railtypes[RAILTYPE_RAIL].max_speed;
	const uint old_max_length = _settings_game.construction.max_bridge_length;

	_bridge[0].avail_year = TimerGameCalendar::Year{0};
	_bridge[0].min_length = 0;
	_bridge[0].max_length = 50;
	_bridge[0].price = 100;
	_bridge[0].speed = 200;
	_settings_game.construction.max_bridge_length = 50;
	_railtypes[RAILTYPE_RAIL].max_speed = 120;

	JunctionSpec spec = MakeJunctionSpec(RAILTYPE_RAIL, 1, 1, 4, MakeTestJunctionSpec().train);
	const Money huge = std::numeric_limits<int64_t>::max() / 4;
	for (uint n = 1; n <= 10; n++) {
		std::optional<BridgeType> type = ChooseJunctionBridgeType(RAILTYPE_RAIL, n);
		REQUIRE(type.has_value());
		CHECK(spec.bridge_span_ok(n));
		const Money expected = Money(((int64_t)CalcBridgeLenCostFactor(n + 2) * _price[Price::BuildBridge] * GetBridgeSpec(*type)->price) >> 8) + (n + 2) * RailBuildCost(RAILTYPE_RAIL);
		CHECK(spec.prices.bridge(n) == expected);
	}
	/* Outside 1..10 there is no precomputed value: not OK, and a very large price. */
	for (uint n : {0u, 11u, 20u}) {
		CHECK_FALSE(spec.bridge_span_ok(n));
		CHECK(spec.prices.bridge(n) == huge);
	}

	_bridge[0] = old_bridge;
	_railtypes[RAILTYPE_RAIL].max_speed = old_max_speed;
	_settings_game.construction.max_bridge_length = old_max_length;
}

TEST_CASE("MakeJunctionSpec: a bridge slower than the design speed is still usable (D94)")
{
	const BridgeSpec old_bridge = _bridge[0];
	const uint16_t old_max_speed = _railtypes[RAILTYPE_RAIL].max_speed;
	const uint old_max_length = _settings_game.construction.max_bridge_length;

	/* Unlimited rail with only a slow bridge, as in an early-era game: the junction uses the fastest bridge there is. */
	_bridge[0].avail_year = TimerGameCalendar::Year{0};
	_bridge[0].min_length = 0;
	_bridge[0].max_length = 50;
	_bridge[0].price = 100;
	_bridge[0].speed = 40;
	_settings_game.construction.max_bridge_length = 50;
	_railtypes[RAILTYPE_RAIL].max_speed = 0;

	JunctionSpec spec = MakeJunctionSpec(RAILTYPE_RAIL, 1, 1, 4, MakeTestJunctionSpec().train);
	REQUIRE(spec.reference_speed > 40);
	for (uint n = 1; n <= 10; n++) {
		if (!ChooseJunctionBridgeType(RAILTYPE_RAIL, n).has_value()) continue;
		CHECK(spec.bridge_span_ok(n));
	}

	_bridge[0] = old_bridge;
	_railtypes[RAILTYPE_RAIL].max_speed = old_max_speed;
	_settings_game.construction.max_bridge_length = old_max_length;
}

TEST_CASE("GameJunctionPricing: flat tile reports its height")
{
	Map::Allocate(64, 64);
	GameJunctionPricing pricing(RAILTYPE_RAIL);
	CHECK(pricing.GetTileSlope(TileXY(10, 10))->z_max == 0);
	CHECK(pricing.GetTileSlope(TileXY(10, 10))->z_min == 0);
}

TEST_CASE("GameJunctionPricing: raised corner is not flat")
{
	Map::Allocate(64, 64);
	GameJunctionPricing pricing(RAILTYPE_RAIL);
	SetTileHeight(TileXY(11, 11), 1);
	CHECK(pricing.GetTileSlope(TileXY(10, 10))->z_min != pricing.GetTileSlope(TileXY(10, 10))->z_max);
}

TEST_CASE("GameJunctionPricing: raised flat tile reports its height")
{
	Map::Allocate(64, 64);
	GameJunctionPricing pricing(RAILTYPE_RAIL);
	SetTileHeight(TileXY(20, 20), 2);
	SetTileHeight(TileXY(21, 20), 2);
	SetTileHeight(TileXY(20, 21), 2);
	SetTileHeight(TileXY(21, 21), 2);
	CHECK(pricing.GetTileSlope(TileXY(20, 20))->z_min == 2);
	CHECK(pricing.GetTileSlope(TileXY(20, 20))->z_max == 2);
}

TEST_CASE("GameJunctionPricing: off-map tile has no height")
{
	Map::Allocate(64, 64);
	GameJunctionPricing pricing(RAILTYPE_RAIL);
	CHECK(pricing.GetTileSlope(INVALID_TILE) == std::nullopt);
}

TEST_CASE("MakeJunctionTerrain: heights, steepness and class in the working frame")
{
	const JunctionFrame frame{};
	const TileIndex anchor = TileXY(64, 64);
	for (const auto &[rotation, mirrored] : {std::pair<uint8_t, bool>{0, false}, std::pair<uint8_t, bool>{1, true}}) {
		Map::Allocate(128, 128);

		/* The map tile of a working cell. */
		auto tile_of = [&](int wx, int wy) {
			const TileIndexDiffC v = OrientJunctionVector({static_cast<int16_t>(wx - frame.hub_x), static_cast<int16_t>(wy)}, rotation, mirrored);
			return TileXY(TileX(anchor) + v.x, TileY(anchor) + v.y);
		};

		/* Working cell (35, 5): raised flat tile. Cell (38, 5): one corner raised by 1. Cell (41, 5): one corner raised by 2 (steep). */
		const TileIndex raised_tile = tile_of(35, 5);
		for (TileIndexDiffC d : {TileIndexDiffC{0, 0}, {1, 0}, {0, 1}, {1, 1}}) SetTileHeight(TileXY(TileX(raised_tile) + d.x, TileY(raised_tile) + d.y), 2);
		const TileIndex one_corner_tile = tile_of(38, 5);
		const auto [one_slope, one_z] = GetTileSlopeZ(one_corner_tile);
		CHECK(one_slope == SLOPE_FLAT);
		CHECK(one_z == 0);
		SetTileHeight(TileXY(TileX(one_corner_tile) + 1, TileY(one_corner_tile)), 1);
		SetTileHeight(TileXY(TileX(tile_of(41, 5)), TileY(tile_of(41, 5))), 2);

		JunctionTerrain terrain = MakeJunctionTerrain(anchor, rotation, mirrored, frame);
		CHECK(terrain.width == frame.width);
		CHECK(terrain.height == frame.terrain_height);
		CHECK(terrain.origin.x == 0);
		CHECK(terrain.origin.y == frame.terrain_origin_y);
		CHECK(terrain.build_on_slopes == _settings_game.construction.build_on_slopes);
		CHECK(terrain.max_bridge_height == _settings_game.construction.max_bridge_height);

		const JunctionTerrainTile *raised = terrain.At(35, 5);
		REQUIRE(raised != nullptr);
		CHECK(raised->z_min == 2);
		CHECK(raised->z_max == 2);
		CHECK(!raised->steep);
		CHECK(raised->cls == JunctionTileClass::Buildable);

		/* A raised tile corner also lifts the neighbouring tiles that share it, so only check that this one is sloped, not steep and tops at 1. */
		const JunctionTerrainTile *corner = terrain.At(38, 5);
		REQUIRE(corner != nullptr);
		CHECK(corner->z_min == 0);
		CHECK(corner->z_max == 1);
		CHECK(!corner->steep);

		const JunctionTerrainTile *steep = terrain.At(41, 5);
		REQUIRE(steep != nullptr);
		CHECK(steep->steep);
		CHECK(steep->z_max == 2);

		const JunctionTerrainTile *flat = terrain.At(frame.hub_x, 0);
		REQUIRE(flat != nullptr);
		CHECK(flat->z_min == 0);
		CHECK(flat->z_max == 0);
		CHECK(flat->cls == JunctionTileClass::Buildable);
	}

	/* The anchor cell is the anchor tile, and cells off the map are Blocked. */
	JunctionTerrain terrain = MakeJunctionTerrain(TileXY(1, 1), 0, false, frame);
	REQUIRE(terrain.At(frame.hub_x, 0) != nullptr);
	CHECK(terrain.At(frame.hub_x, 0)->cls == JunctionTileClass::Buildable);
	REQUIRE(terrain.At(0, 0) != nullptr);
	CHECK(terrain.At(0, 0)->cls == JunctionTileClass::Blocked);
}

/** A flat Buildable terrain at z0 with margin 4, in the frame of the sized test spec: origin and size as MakeJunctionTerrain gives. */
static JunctionTerrain MakeNearTestTerrain(int margin = 4)
{
	const JunctionFrame frame = MakeJunctionFrame(MakeTestJunctionSpec());
	JunctionTerrain t;
	t.origin = {static_cast<int16_t>(-margin), static_cast<int16_t>(frame.terrain_origin_y - margin)};
	t.width = frame.width + 2 * margin;
	t.height = frame.terrain_height + 2 * margin;
	t.tiles.assign(static_cast<size_t>(t.width) * t.height, JunctionTerrainTile{});
	t.foundation_cost = 250;
	return t;
}

/** Set the class of a rectangle given in working coordinates. */
static void SetNearTestRect(JunctionTerrain &t, int x0, int x1, int y0, int y1, JunctionTileClass cls)
{
	for (int y = y0; y <= y1; y++) {
		for (int x = x0; x <= x1; x++) t.tiles[static_cast<size_t>(y - t.origin.y) * t.width + (x - t.origin.x)].cls = cls;
	}
}

TEST_CASE("GenerateJunctionNear: a clear spot gives offset 0 and the plain layout", "[junction][terrain]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionTerrain terrain = MakeNearTestTerrain();
	const std::atomic<bool> cancel{false};
	const JunctionNearResult near = GenerateJunctionNear(spec, terrain, 0, false, cancel);
	const JunctionTerrain plain = MakeNearTestTerrain(0);
	const JunctionGenerateResult base = GenerateJunction(spec, &plain);
	REQUIRE(near.result.layout.has_value());
	REQUIRE(base.layout.has_value());
	CHECK(near.offset.x == 0);
	CHECK(near.offset.y == 0);
	REQUIRE(near.result.layout->tracks.size() == base.layout->tracks.size());
	for (size_t i = 0; i < base.layout->tracks.size(); i++) {
		CHECK(near.result.layout->tracks[i].offset.x == base.layout->tracks[i].offset.x);
		CHECK(near.result.layout->tracks[i].offset.y == base.layout->tracks[i].offset.y);
		CHECK(near.result.layout->tracks[i].tracks == base.layout->tracks[i].tracks);
	}
}

TEST_CASE("GenerateJunctionNear: a blocked spot gives a nearby fit", "[junction][terrain][.slow]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	JunctionTerrain terrain = MakeNearTestTerrain();
	const int hub_shift = MakeJunctionFrame(spec).hub_x - JunctionFrame{}.hub_x;
	SetNearTestRect(terrain, 27 + hub_shift, 32 + hub_shift, 2, 7, JunctionTileClass::Blocked);
	const std::atomic<bool> cancel{false};
	const JunctionNearResult near = GenerateJunctionNear(spec, terrain, 0, false, cancel);
	REQUIRE(near.result.layout.has_value());
	CHECK((near.offset.x != 0 || near.offset.y != 0));
}

TEST_CASE("GenerateJunctionNear: cancel stops the search", "[junction][terrain]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	JunctionTerrain terrain = MakeNearTestTerrain();
	SetNearTestRect(terrain, -4, MakeJunctionFrame(spec).width + 3, -6, 44, JunctionTileClass::Blocked);
	const std::atomic<bool> cancel{true};
	const auto start = std::chrono::steady_clock::now();
	const JunctionNearResult near = GenerateJunctionNear(spec, terrain, 0, false, cancel);
	const auto elapsed = std::chrono::steady_clock::now() - start;
	CHECK_FALSE(near.result.layout.has_value());
	CHECK(elapsed < std::chrono::seconds(2));
}

TEST_CASE("FitJunctionLibraryNear: clear terrain gives offset 0 and the library's best entry", "[junction][terrain]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const std::vector<JunctionLibraryEntry> library = BuildJunctionLibrary(spec);
	REQUIRE(!library.empty());
	const JunctionTerrain terrain = MakeNearTestTerrain();
	const JunctionNearResult near = FitJunctionLibraryNear(library, terrain, 0, false);
	const JunctionTerrain plain = MakeNearTestTerrain(0);
	const std::optional<JunctionLibraryFit> fit = FitJunctionLibrary(library, plain);
	REQUIRE(fit.has_value());
	REQUIRE(near.result.layout.has_value());
	CHECK(near.result.error == JunctionGenerateError::None);
	CHECK(near.offset.x == 0);
	CHECK(near.offset.y == 0);
	CHECK(JunctionLayoutToJson(*near.result.layout) == JunctionLayoutToJson(library[fit->entry].layout));
}

TEST_CASE("FitJunctionLibraryNear: a blocked block gives a fit whose tiles are buildable, or none", "[junction][terrain]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const std::vector<JunctionLibraryEntry> library = BuildJunctionLibrary(spec);
	JunctionTerrain terrain = MakeNearTestTerrain();
	const int hub_shift = MakeJunctionFrame(spec).hub_x - JunctionFrame{}.hub_x;
	SetNearTestRect(terrain, 27 + hub_shift, 32 + hub_shift, 2, 7, JunctionTileClass::Blocked);
	const JunctionNearResult near = FitJunctionLibraryNear(library, terrain, 0, false);
	WARN("6x6 block: " << (near.result.layout.has_value() ? "fit" : "no fit") << " at offset " << near.offset.x << "," << near.offset.y);
	if (near.result.layout.has_value()) {
		/* At rotation 0 the shifted snapshot reads the unshifted one at the working tile plus the offset. */
		const int ox = MakeJunctionFrame(spec).hub_x - near.result.hub.x + near.offset.x;
		const int oy = -near.result.hub.y + near.offset.y;
		for (const JunctionTrack &jt : near.result.layout->tracks) {
			if (jt.tracks.None()) continue;
			const JunctionTerrainTile *t = terrain.At(jt.offset.x + ox, jt.offset.y + oy);
			REQUIRE(t != nullptr);
			CHECK(t->cls == JunctionTileClass::Buildable);
		}
	}
}

TEST_CASE("FitJunctionLibraryNear: all blocked gives NoLayout at offset 0", "[junction][terrain]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const std::vector<JunctionLibraryEntry> library = BuildJunctionLibrary(spec);
	JunctionTerrain terrain = MakeNearTestTerrain();
	SetNearTestRect(terrain, -4, MakeJunctionFrame(spec).width + 3, -6, 44, JunctionTileClass::Blocked);
	const JunctionNearResult near = FitJunctionLibraryNear(library, terrain, 0, false);
	CHECK_FALSE(near.result.layout.has_value());
	CHECK(near.result.error == JunctionGenerateError::NoLayout);
	CHECK(near.offset.x == 0);
	CHECK(near.offset.y == 0);
}
