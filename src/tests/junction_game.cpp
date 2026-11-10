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
#include "../junction_rules.h"
#include "../map_func.h"
#include "../tile_map.h"
#include "../rail.h"
#include "../settings_type.h"
#include "../slope_func.h"
#include "../bridge.h"
#include "../economy_func.h"
#include "../timer/timer_game_calendar.h"

#include <chrono>
#include <iostream>

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

TEST_CASE("MakeJunctionSpec caps the reference speed at the picked train", "[junction][game]")
{
	const uint16_t old_max_speed = _railtypes[RAILTYPE_RAIL].max_speed;
	_railtypes[RAILTYPE_RAIL].max_speed = 120;

	JunctionTrainParams train = MakeTestJunctionSpec().train;
	train.max_speed = 100;
	CHECK(MakeJunctionSpec(RAILTYPE_RAIL, 2, 3, 5, train).reference_speed == 100);
	CHECK(MakeJunctionSpec(RAILTYPE_RAIL, 2, 3, 5, JunctionTrainParams{}).reference_speed == GetJunctionReferenceSpeed(RAILTYPE_RAIL));

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

TEST_CASE("JunctionTunnelPrice follows the tunnel command's cost", "[junction][tunnelprice]")
{
	/*
	 * CmdBuildTunnel with build_tunnel = 1000 and rail = 100, tiles_coef 3 (4 from the 25th middle tile on), tiles_bump 25:
	 * middle 0: no loop; the entrance 1000 and the exit 1000, and (0 + 2) * 100 rail = 200: 2200.
	 * middle 1: the loop adds 1000, then 1000 >> 3 = 125: 1125; plus 2000 and (1 + 2) * 100 = 300: 3425.
	 * Middle 25 is the first tile with tiles == tiles_bump, so its multiplier is >> 4.
	 */
	CHECK(JunctionTunnelPrice(0, Money(1000), Money(100)) == Money(2200));
	CHECK(JunctionTunnelPrice(1, Money(1000), Money(100)) == Money(3425));
	CHECK(JunctionTunnelPrice(2, Money(1000), Money(100)) == Money(4790));
	CHECK(JunctionTunnelPrice(24, Money(1000), Money(100)) == Money(147557));
	CHECK(JunctionTunnelPrice(25, Money(1000), Money(100)) == Money(157654));
	CHECK(JunctionTunnelPrice(26, Money(1000), Money(100)) == Money(168376));
}

TEST_CASE("MakeJunctionSpec: the tunnel price and length come from the game", "[junction][tunnelprice]")
{
	const uint16_t old_max_tunnel = _settings_game.construction.max_tunnel_length;
	_settings_game.construction.max_tunnel_length = 40;

	JunctionSpec spec = MakeJunctionSpec(RAILTYPE_RAIL, 1, 1, 4, MakeTestJunctionSpec().train);
	REQUIRE(static_cast<bool>(spec.prices.tunnel));
	for (const uint n : {0u, 3u}) {
		CHECK(spec.prices.tunnel(n) == JunctionTunnelPrice(n, _price[Price::BuildTunnel], RailBuildCost(RAILTYPE_RAIL)));
	}
	CHECK(spec.max_tunnel_length == _settings_game.construction.max_tunnel_length);

	_settings_game.construction.max_tunnel_length = old_max_tunnel;
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

TEST_CASE("MakeJunctionTerrainRect: cells are map tiles", "[terrainrect]")
{
	Map::Allocate(128, 128);
	const TileIndex raised_tile = TileXY(32, 46);
	for (TileIndexDiffC d : {TileIndexDiffC{0, 0}, {1, 0}, {0, 1}, {1, 1}}) SetTileHeight(TileXY(TileX(raised_tile) + d.x, TileY(raised_tile) + d.y), 2);
	const TileIndex one_corner_tile = TileXY(35, 48);
	SetTileHeight(TileXY(36, 48), 1);
	SetTileHeight(TileXY(38, 49), 2);

	const auto start = std::chrono::steady_clock::now();
	const JunctionTerrain terrain = MakeJunctionTerrainRect(TileXY(40, 50), TileXY(30, 45), 2);
	const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
	CHECK(terrain.origin.x == 28);
	CHECK(terrain.origin.y == 43);
	CHECK(terrain.width == 15);
	CHECK(terrain.height == 10);
	CHECK(terrain.build_on_slopes == _settings_game.construction.build_on_slopes);
	CHECK(terrain.max_bridge_height == _settings_game.construction.max_bridge_height);

	const JunctionTerrainTile *raised = terrain.At(TileX(raised_tile), TileY(raised_tile));
	REQUIRE(raised != nullptr);
	CHECK(raised->z_min == 2);
	CHECK(raised->z_max == 2);
	CHECK(!raised->steep);

	const JunctionTerrainTile *corner = terrain.At(TileX(one_corner_tile), TileY(one_corner_tile));
	REQUIRE(corner != nullptr);
	CHECK(corner->z_min == 0);
	CHECK(corner->z_max == 1);
	CHECK(!corner->steep);

	const JunctionTerrainTile *steep = terrain.At(38, 49);
	REQUIRE(steep != nullptr);
	CHECK(steep->steep);
	CHECK(steep->z_max == 2);

	const JunctionTerrainTile *flat = terrain.At(30, 45);
	REQUIRE(flat != nullptr);
	CHECK(flat->z_min == 0);
	CHECK(flat->z_max == 0);
	CHECK(flat->cls == JunctionTileClass::Buildable);

	/* Timing of a 64x64 snapshot. */
	const auto bench_start = std::chrono::steady_clock::now();
	const JunctionTerrain big = MakeJunctionTerrainRect(TileXY(10, 10), TileXY(73, 73), 0);
	const auto bench_micros = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - bench_start).count();
	CHECK(big.width == 64);
	CHECK(big.height == 64);
	std::cout << "BENCH terrainrect 64x64 micros=" << bench_micros << " (small rect micros=" << micros << ")\n";
}

TEST_CASE("MakeJunctionTerrainRect: agrees with MakeJunctionTerrain at rotation 0", "[terrainrect]")
{
	Map::Allocate(128, 128);
	const JunctionFrame frame{};
	/* Relies on OrientJunctionVector(v, 0, false) == v: no mirror, no quarter turns. Working cell (x, y) is then map tile (64 + x - hub_x, 64 + y). */
	REQUIRE(OrientJunctionVector({7, -3}, 0, false).x == 7);
	REQUIRE(OrientJunctionVector({7, -3}, 0, false).y == -3);
	SetTileHeight(TileXY(60, 66), 2);
	SetTileHeight(TileXY(70, 70), 1);

	const JunctionTerrain snapshot = MakeJunctionTerrain(TileXY(64, 64), 0, false, frame);
	const int x0 = 64 - frame.hub_x;
	const JunctionTerrain rect = MakeJunctionTerrainRect(TileXY(x0, 64 + frame.terrain_origin_y), TileXY(x0 + frame.width - 1, 64 + frame.terrain_origin_y + frame.terrain_height - 1), 0);
	CHECK(rect.foundation_cost == snapshot.foundation_cost);
	CHECK(rect.build_on_slopes == snapshot.build_on_slopes);
	CHECK(rect.max_bridge_height == snapshot.max_bridge_height);
	int compared = 0;
	for (int wy = snapshot.origin.y; wy < snapshot.origin.y + snapshot.height; wy++) {
		for (int wx = snapshot.origin.x; wx < snapshot.origin.x + snapshot.width; wx++) {
			const JunctionTerrainTile *a = snapshot.At(wx, wy);
			const JunctionTerrainTile *b = rect.At(64 + wx - frame.hub_x, 64 + wy);
			if (b == nullptr) continue;
			REQUIRE(a != nullptr);
			compared++;
			CHECK(a->z_min == b->z_min);
			CHECK(a->z_max == b->z_max);
			CHECK(a->steep == b->steep);
			CHECK(a->incline == b->incline);
			CHECK(a->cls == b->cls);
			CHECK(a->clear_cost == b->clear_cost);
		}
	}
	CHECK(compared == frame.width * frame.terrain_height);
}

TEST_CASE("MakeJunctionTerrainRect: off-map cells are Blocked", "[terrainrect]")
{
	Map::Allocate(128, 128);
	const JunctionTerrain terrain = MakeJunctionTerrainRect(TileXY(0, 0), TileXY(3, 3), 2);
	CHECK(terrain.origin.x == -2);
	CHECK(terrain.origin.y == -2);
	REQUIRE(terrain.At(-1, -1) != nullptr);
	CHECK(terrain.At(-1, -1)->cls == JunctionTileClass::Blocked);
	REQUIRE(terrain.At(-2, 3) != nullptr);
	CHECK(terrain.At(-2, 3)->cls == JunctionTileClass::Blocked);
	REQUIRE(terrain.At(0, 0) != nullptr);
	CHECK(terrain.At(0, 0)->cls == MakeJunctionTerrain(TileXY(1, 1), 0, false, JunctionFrame{}).At(JunctionFrame{}.hub_x, 0)->cls);
	CHECK(terrain.At(0, 0)->cls == JunctionTileClass::Buildable);
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

TEST_CASE("GenerateJunctionNear: a clear spot gives offset 0 and the plain layout", "[junction][terrain][slow]")
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

TEST_CASE("FitJunctionLibraryNear: clear terrain gives offset 0 and the library's best entry", "[junction][terrain][slow]")
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

TEST_CASE("FitJunctionLibraryNear: a blocked block gives a fit whose tiles are buildable, or none", "[junction][terrain][slow]")
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

TEST_CASE("FitJunctionLibraryNear: all blocked gives NoLayout at offset 0", "[junction][terrain][slow]")
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

TEST_CASE("JunctionPrioChoice follows the slider rules", "[junction][game]")
{
	using R = std::pair<uint8_t, bool>;
	CHECK(JunctionPrioChoice(0, 5, 14) == R{14, true});
	CHECK(JunctionPrioChoice(20, 5, 14) == R{14, true});
	CHECK(JunctionPrioChoice(14, 5, 14) == R{14, true});
	CHECK(JunctionPrioChoice(7, 5, 14) == R{7, false});
	CHECK(JunctionPrioChoice(3, 5, 14) == R{5, false});
	CHECK(JunctionPrioChoice(0, 5, 3) == R{3, true});
	CHECK(JunctionPrioChoice(2, 5, 3) == R{3, true});
}

/** Set every tile of row y of the terrain: heights and incline (a copy of SetRampRow in junction_map.cpp). */
static void SetNearRampRow(JunctionTerrain &terrain, int y, int z_min, int z_max, DiagDirection incline)
{
	for (int x = terrain.origin.x; x < terrain.origin.x + terrain.width; x++) {
		JunctionTerrainTile &tile = terrain.tiles[static_cast<size_t>(y - terrain.origin.y) * terrain.width + (x - terrain.origin.x)];
		tile.z_min = static_cast<int8_t>(z_min);
		tile.z_max = static_cast<int8_t>(z_max);
		tile.incline = incline;
	}
}

TEST_CASE("GenerateJunctionNear builds the projected T map over a ridge", "[junction][tmapnear]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	JunctionTerrain terrain = MakeNearTestTerrain();
	terrain.build_on_slopes = true;
	const int ridge = spec.ml_gap + 2 + 6;
	SetNearRampRow(terrain, ridge, 0, 1, DiagDirection::SE);
	SetNearRampRow(terrain, ridge + 1, 1, 1, DiagDirection::Invalid);
	SetNearRampRow(terrain, ridge + 2, 1, 1, DiagDirection::Invalid);
	SetNearRampRow(terrain, ridge + 3, 0, 1, DiagDirection::NW);
	const JunctionTMapResult direct = ConstructJunctionTMap(spec, &terrain, std::nullopt, true);
	REQUIRE(direct.layout.has_value());
	const std::atomic<bool> cancel{false};
	const auto start = std::chrono::steady_clock::now();
	const JunctionNearResult near = GenerateJunctionNear(spec, terrain, 0, false, cancel);
	const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
	std::cout << "BENCH tmapnear GenerateJunctionNear micros=" << micros << "\n";
	CHECK(near.offset.x == 0);
	CHECK(near.offset.y == 0);
	CHECK(near.result.error == JunctionGenerateError::None);
	CHECK(near.result.hub.x == direct.hub.x);
	CHECK(near.result.hub.y == direct.hub.y);
	REQUIRE(near.result.layout.has_value());
	const JunctionLayout &a = *near.result.layout;
	const JunctionLayout &b = *direct.layout;
	REQUIRE(a.tracks.size() == b.tracks.size());
	for (size_t i = 0; i < a.tracks.size(); i++) {
		CHECK(a.tracks[i].offset.x == b.tracks[i].offset.x);
		CHECK(a.tracks[i].offset.y == b.tracks[i].offset.y);
		CHECK(a.tracks[i].tracks == b.tracks[i].tracks);
	}
	REQUIRE(a.bridges.size() == b.bridges.size());
	for (size_t i = 0; i < a.bridges.size(); i++) {
		CHECK(a.bridges[i].start.x == b.bridges[i].start.x);
		CHECK(a.bridges[i].start.y == b.bridges[i].start.y);
		CHECK(a.bridges[i].end.x == b.bridges[i].end.x);
		CHECK(a.bridges[i].end.y == b.bridges[i].end.y);
		CHECK(a.bridges[i].tunnel == b.bridges[i].tunnel);
	}
	REQUIRE(a.signals.size() == b.signals.size());
	for (size_t i = 0; i < a.signals.size(); i++) {
		CHECK(a.signals[i].offset.x == b.signals[i].offset.x);
		CHECK(a.signals[i].offset.y == b.signals[i].offset.y);
		CHECK(a.signals[i].facing == b.signals[i].facing);
		CHECK(a.signals[i].type == b.signals[i].type);
		CHECK(a.signals[i].variant == b.signals[i].variant);
	}
}

TEST_CASE("GenerateJunctionNear's score includes the terrain cost", "[junction][tmapnear]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	JunctionTerrain terrain = MakeNearTestTerrain();
	terrain.build_on_slopes = true;
	terrain.foundation_cost = 250;
	for (JunctionTerrainTile &tile : terrain.tiles) tile.clear_cost = 30;
	const int ridge = spec.ml_gap + 2 + 6;
	SetNearRampRow(terrain, ridge, 0, 1, DiagDirection::SE);
	SetNearRampRow(terrain, ridge + 1, 1, 1, DiagDirection::Invalid);
	SetNearRampRow(terrain, ridge + 2, 1, 1, DiagDirection::Invalid);
	SetNearRampRow(terrain, ridge + 3, 0, 1, DiagDirection::NW);
	const JunctionTMapResult tmap = ConstructJunctionTMap(spec, &terrain, std::nullopt, true);
	REQUIRE(tmap.layout.has_value());
	const std::atomic<bool> cancel{false};
	const JunctionNearResult near = GenerateJunctionNear(spec, terrain, 0, false, cancel);
	REQUIRE(near.result.layout.has_value());
	CHECK(near.offset.x == 0);
	CHECK(near.offset.y == 0);
	CHECK(tmap.terrain_cost > 0);
	CHECK(near.result.score.second == ScoreJunctionLayout(*near.result.layout, spec).second + tmap.terrain_cost);
}

TEST_CASE("GenerateJunctionNear runs the flat map once", "[junction][tmapnear]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const auto since = [](std::chrono::steady_clock::time_point start) {
		return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
	};
	const auto flat_start = std::chrono::steady_clock::now();
	const JunctionTMapResult flat = ConstructJunctionTMap(spec, nullptr, std::nullopt, false);
	const int64_t flat_micros = since(flat_start);
	REQUIRE(flat.layout.has_value());
	const int x_w = MakeJunctionFrame(spec).hub_x - flat.hub.x;
	JunctionTerrain terrain = MakeNearTestTerrain();
	/* The west port's approach is blocked at the anchor and at the offsets above it; the first offset below clears it and builds. */
	SetNearTestRect(terrain, x_w - 4, x_w + 6, -4, 1, JunctionTileClass::Blocked);
	const std::atomic<bool> cancel{false};
	const auto start = std::chrono::steady_clock::now();
	const JunctionNearResult near = GenerateJunctionNear(spec, terrain, 0, false, cancel);
	const int64_t micros = since(start);
	/* The result equals the one the T map gives at that offset on its own. */
	REQUIRE(near.result.layout.has_value());
	CHECK(near.offset.x == -2);
	CHECK(near.offset.y == 2);
	JunctionTerrain shifted = terrain;
	shifted.origin = {static_cast<int16_t>(terrain.origin.x - near.offset.x), static_cast<int16_t>(terrain.origin.y - near.offset.y)};
	const JunctionTMapResult direct = ConstructJunctionTMap(spec, &shifted, std::nullopt, true);
	REQUIRE(direct.layout.has_value());
	CHECK(near.result.hub.x == direct.hub.x);
	CHECK(near.result.hub.y == direct.hub.y);
	CHECK(near.result.score == std::make_pair(ScoreJunctionLayout(*direct.layout, spec).first, ScoreJunctionLayout(*direct.layout, spec).second + direct.terrain_cost));
	REQUIRE(near.result.layout->tracks.size() == direct.layout->tracks.size());
	for (size_t i = 0; i < direct.layout->tracks.size(); i++) {
		CHECK(near.result.layout->tracks[i].offset.x == direct.layout->tracks[i].offset.x);
		CHECK(near.result.layout->tracks[i].offset.y == direct.layout->tracks[i].offset.y);
		CHECK(near.result.layout->tracks[i].tracks == direct.layout->tracks[i].tracks);
	}
	/* The offsets tried: the anchor, then d = 2 and d = 4 by rows then columns, up to the one found. */
	int tried = 1;
	bool reached = near.offset.x == 0 && near.offset.y == 0;
	for (int d : {2, 4}) {
		for (int dy = -d; dy <= d && !reached; dy += d) {
			for (int dx = -d; dx <= d && !reached; dx += d) {
				if (dx == 0 && dy == 0) continue;
				tried++;
				reached = dx == near.offset.x && dy == near.offset.y;
			}
		}
	}
	REQUIRE(reached);
	INFO("offsets tried " << tried << ", flat map " << flat_micros << " us, GenerateJunctionNear " << micros << " us");
	CHECK(micros < flat_micros * tried / 2);
}

TEST_CASE("GenerateJunctionNear reports a blocked port", "[junction][tmapnear]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionTMapResult flat = ConstructJunctionTMap(spec, nullptr, std::nullopt, true);
	REQUIRE(flat.layout.has_value());
	constexpr int STUB = 2;
	const int x_w = MakeJunctionFrame(spec).hub_x - flat.hub.x;
	JunctionTerrain terrain = MakeNearTestTerrain();
	/* The west port's approach at every near offset's position. */
	SetNearTestRect(terrain, x_w - 4, x_w + STUB + 4, -4, 4, JunctionTileClass::Blocked);
	const std::atomic<bool> cancel{false};
	const JunctionNearResult near = GenerateJunctionNear(spec, terrain, 0, false, cancel);
	CHECK(near.result.error == JunctionGenerateError::PortBlocked);
	CHECK(!near.result.layout.has_value());
}
