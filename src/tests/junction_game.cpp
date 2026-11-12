/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_game.cpp Test functionality from junction_game. */

#include "../stdafx.h"

#include "../3rdparty/catch2/catch.hpp"

#include "../junction_facts.h"
#include "../junction_game.h"
#include "../junction_generator.h"
#include "../junction_hover.h"
#include "../junction_layout.h"
#include "../junction_terraform.h"
#include "../junction_terrain_json.h"
#include "../junction_nodes.h"
#include "../junction_rules.h"
#include "../junction_topology.h"
#include "../landscape.h"
#include "../map_func.h"
#include "../rail_map.h"
#include "../tile_map.h"
#include "../track_func.h"
#include "../rail.h"
#include "../settings_type.h"
#include "../slope_func.h"
#include "../bridge.h"
#include "../economy_func.h"
#include "../timer/timer_game_calendar.h"
#include "../company_base.h"
#include "../company_func.h"
#include "../command_func.h"
#include "../clear_map.h"
#include "../water_map.h"
#include "../tunnelbridge.h"
#include "../terraform_cmd.h"
#include "../table/strings.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <random>
#include <set>
#include <tuple>

#include "../safeguards.h"

extern RailTypeInfo _railtypes[RAILTYPE_END];

TEST_CASE("MakeJunctionSpec: reads settings and the rail type", "[junction][game]")
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

TEST_CASE("MakeJunctionSpec: bridge closures are precomputed up to the game's max bridge length", "[junction][game]")
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
	CHECK(spec.max_bridge_middle == 50);
	for (uint n = 1; n <= 50; n++) {
		std::optional<BridgeType> type = ChooseJunctionBridgeType(RAILTYPE_RAIL, n);
		REQUIRE(type.has_value());
		CHECK(spec.bridge_span_ok(n));
		const Money expected = Money(((int64_t)CalcBridgeLenCostFactor(n + 2) * _price[Price::BuildBridge] * GetBridgeSpec(*type)->price) >> 8) + (n + 2) * RailBuildCost(RAILTYPE_RAIL);
		CHECK(spec.prices.bridge(n) == expected);
	}
	/* Outside 1..50 there is no precomputed value: not OK, and a very large price. */
	for (uint n : {0u, 51u, 60u}) {
		CHECK_FALSE(spec.bridge_span_ok(n));
		CHECK(spec.prices.bridge(n) == huge);
	}

	_bridge[0] = old_bridge;
	_railtypes[RAILTYPE_RAIL].max_speed = old_max_speed;
	_settings_game.construction.max_bridge_length = old_max_length;
}

TEST_CASE("MakeJunctionSpec: a bridge type's own max length bounds the span", "[junction][game]")
{
	std::array<BridgeSpec, MAX_BRIDGES> old_bridges;
	std::copy(std::begin(_bridge), std::end(_bridge), old_bridges.begin());
	const uint16_t old_max_speed = _railtypes[RAILTYPE_RAIL].max_speed;
	const uint old_max_length = _settings_game.construction.max_bridge_length;

	for (BridgeType type = 0; type < MAX_BRIDGES; type++) _bridge[type].avail_year = TimerGameCalendar::Year{INT32_MAX};
	_bridge[0].avail_year = TimerGameCalendar::Year{0};
	_bridge[0].min_length = 0;
	_bridge[0].max_length = 12;
	_bridge[0].price = 100;
	_bridge[0].speed = 200;
	_settings_game.construction.max_bridge_length = 50;
	_railtypes[RAILTYPE_RAIL].max_speed = 120;

	JunctionSpec spec = MakeJunctionSpec(RAILTYPE_RAIL, 1, 1, 4, MakeTestJunctionSpec().train);
	CHECK(spec.bridge_span_ok(12));
	CHECK_FALSE(spec.bridge_span_ok(13));

	std::copy(old_bridges.begin(), old_bridges.end(), std::begin(_bridge));
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

TEST_CASE("MakeJunctionSpec: a bridge slower than the design speed is still usable (D94)", "[junction][game]")
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

TEST_CASE("GameJunctionPricing: flat tile reports its height", "[junction][game]")
{
	Map::Allocate(64, 64);
	GameJunctionPricing pricing(RAILTYPE_RAIL);
	CHECK(pricing.GetTileSlope(TileXY(10, 10))->z_max == 0);
	CHECK(pricing.GetTileSlope(TileXY(10, 10))->z_min == 0);
}

TEST_CASE("GameJunctionPricing: raised corner is not flat", "[junction][game]")
{
	Map::Allocate(64, 64);
	GameJunctionPricing pricing(RAILTYPE_RAIL);
	SetTileHeight(TileXY(11, 11), 1);
	CHECK(pricing.GetTileSlope(TileXY(10, 10))->z_min != pricing.GetTileSlope(TileXY(10, 10))->z_max);
}

TEST_CASE("GameJunctionPricing: raised flat tile reports its height", "[junction][game]")
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

TEST_CASE("GameJunctionPricing: off-map tile has no height", "[junction][game]")
{
	Map::Allocate(64, 64);
	GameJunctionPricing pricing(RAILTYPE_RAIL);
	CHECK(pricing.GetTileSlope(INVALID_TILE) == std::nullopt);
}

TEST_CASE("MakeJunctionTerrain: heights, steepness and class in the working frame", "[junction][game]")
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

TEST_CASE("the hover snapshot: the projection frame costs at most 3 times the earlier one", "[junction][game]")
{
	Map::Allocate(256, 256);
	const JunctionSpec spec = MakeTestJunctionSpec();
	std::string error;
	const JunctionGraphSeed seed = MakeJunctionGraphSeed(JunctionTTopology(), spec, &error);
	INFO(error);
	REQUIRE(!seed.routes.empty());
	JunctionFrame frame;
	frame.hub_x = seed.placement.size.x / 2;
	frame.width = seed.placement.size.x;
	frame.terrain_origin_y = 0;
	frame.terrain_height = seed.placement.size.y;

	/* The best of five: the first run pays for cold caches. */
	auto best_micros = [](auto &&make) {
		int64_t best = INT64_MAX;
		for (int i = 0; i < 5; i++) {
			const auto start = std::chrono::steady_clock::now();
			const JunctionTerrain terrain = make();
			best = std::min<int64_t>(best, std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count());
			REQUIRE(!terrain.tiles.empty());
		}
		return best;
	};
	const int64_t old_us = best_micros([&]() { return MakeJunctionTerrain(TileXY(128, 128), 0, false, MakeJunctionFrame(spec), 4); });
	const int64_t new_us = best_micros([&]() { return MakeJunctionTerrain(TileXY(128, 128), 0, false, frame, 8); });
	std::cout << "HOVERSNAPSHOT old_us=" << old_us << " new_us=" << new_us << " box=" << seed.placement.size.x << "x" << seed.placement.size.y << "\n";
	CHECK(new_us <= 3 * old_us);
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

TEST_CASE("GenerateJunctionNear: a clear spot gives offset 0 and a rule-clean layout", "[junction][terrain][slow]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionTerrain terrain = MakeNearTestTerrain();
	const std::atomic<bool> cancel{false};
	const JunctionNearResult near = GenerateJunctionNear(spec, terrain, 0, false, cancel);
	REQUIRE(near.result.layout.has_value());
	CHECK(near.result.error == JunctionGenerateError::None);
	CHECK(near.offset.x == 0);
	CHECK(near.offset.y == 0);
	const std::vector<RuleViolation> violations = CheckJunctionRules(*near.result.layout, spec);
	for (const RuleViolation &violation : violations) INFO("rule " << to_underlying(violation.rule) << " at " << violation.tile.x << "," << violation.tile.y << ": " << violation.message);
	CHECK(violations.empty());
}

TEST_CASE("GenerateJunctionNear: a blocked spot gives a nearby fit", "[junction][terrain][slow]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	JunctionTerrain terrain = MakeNearTestTerrain();
	const int hub_shift = MakeJunctionFrame(spec).hub_x - JunctionFrame{}.hub_x;
	SetNearTestRect(terrain, 27 + hub_shift, 32 + hub_shift, 2, 7, JunctionTileClass::Blocked);
	const std::atomic<bool> cancel{false};
	const JunctionNearResult near = GenerateJunctionNear(spec, terrain, 0, false, cancel);
	REQUIRE(near.result.layout.has_value());
	CHECK((near.offset.x != 0 || near.offset.y != 0));
	WARN("blocked spot: offset " << near.offset.x << "," << near.offset.y);
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
		tile.slope = incline == DiagDirection::Invalid ? SLOPE_FLAT : InclinedSlope(incline);
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
	const std::atomic<bool> cancel{false};
	const auto start = std::chrono::steady_clock::now();
	const JunctionNearResult near = GenerateJunctionNear(spec, terrain, 0, false, cancel);
	const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
	std::cout << "BENCH tmapnear GenerateJunctionNear micros=" << micros << "\n";
	/* (0,0) needs a structure change: path 4's free diagonal must cross the ramp row (HF1b finding); HF1b makes the solve report it as a Height failure. */
	WARN("ridge: offset " << near.offset.x << "," << near.offset.y);
	CHECK(near.result.error == JunctionGenerateError::None);
	REQUIRE(near.result.layout.has_value());
	const std::vector<RuleViolation> violations = CheckJunctionRules(*near.result.layout, spec);
	for (const RuleViolation &violation : violations) INFO("rule " << to_underlying(violation.rule) << " at " << violation.tile.x << "," << violation.tile.y << ": " << violation.message);
	CHECK(violations.empty());
}

/** Anchor-offset 0 on flat land at the 196 km/h design speed, with its sized prio coverage. */
TEST_CASE("GenerateJunctionNear builds on flat land at 196 km/h", "[junction][tmapnear]")
{
	JunctionSpec spec = MakeTestJunctionSpec();
	spec.train.max_speed = 196;
	spec.reference_speed = 196;
	spec.prio_tiles = SizeJunctionPrio(spec).prio_tiles;
	const JunctionTerrain terrain = MakeNearTestTerrain();
	const std::atomic<bool> cancel{false};
	const auto start = std::chrono::steady_clock::now();
	const JunctionNearResult near = GenerateJunctionNear(spec, terrain, 0, false, cancel);
	const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
	std::cout << "BENCH tmapnear GenerateJunctionNear 196 micros=" << micros << "\n";
	CHECK(near.result.error == JunctionGenerateError::None);
	CHECK(near.offset.x == 0);
	CHECK(near.offset.y == 0);
	REQUIRE(near.result.layout.has_value());
	const std::vector<RuleViolation> violations = CheckJunctionRules(*near.result.layout, spec);
	for (const RuleViolation &violation : violations) INFO("rule " << to_underlying(violation.rule) << " at " << violation.tile.x << "," << violation.tile.y << ": " << violation.message);
	CHECK(violations.empty());
}

TEST_CASE("the node search's layouts pass the rule checker on random terrain", "[junction][nodeoracle]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionTMapResult &topo = JunctionNodeTopologyT();
	REQUIRE(topo.layout.has_value());
	const JunctionFrame frame = MakeJunctionFrame(spec);
	const TileIndexDiffC start_offset = {static_cast<int16_t>(frame.hub_x - topo.hub.x), static_cast<int16_t>(-topo.hub.y)};
	const int x_lo = start_offset.x;
	const int x_hi = start_offset.x + topo.layout->size.x - 1;

	int n = 0;
	int skipped = 0;
	int accepted = 0;
	int64_t total_search_micros = 0;
	int heights = 0;
	int fits = 0;
	const auto run = [&](const JunctionTerrain &terrain, int seed) {
		n++;
		if (JunctionBlockedPortApproach(terrain, x_lo, x_hi).has_value()) {
			skipped++;
			return;
		}
		const auto start = std::chrono::steady_clock::now();
		const JunctionNodeSearch search = SearchJunctionNodes(*topo.layout, spec, terrain, start_offset, 16);
		total_search_micros += std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
		for (size_t at = search.log.find("heights:"); at != std::string::npos; at = search.log.find("heights:", at + 1)) heights++;
		for (size_t at = search.log.find("fits:"); at != std::string::npos; at = search.log.find("fits:", at + 1)) fits++;
		if (!search.layout.has_value()) return;
		accepted++;
		const std::vector<RuleViolation> violations = CheckJunctionRules(*search.layout, spec);
		INFO("seed " << seed);
		for (const RuleViolation &violation : violations) INFO("rule " << to_underlying(violation.rule) << " at " << violation.tile.x << "," << violation.tile.y << ": " << violation.message);
		CHECK(violations.empty());
	};

	for (int seed = 1; seed <= 200; seed++) {
		std::mt19937 rng(seed);
		JunctionTerrain terrain = MakeNearTestTerrain();
		const int count = std::uniform_int_distribution<int>(1, 3)(rng);
		for (int i = 0; i < count; i++) {
			const int w = std::uniform_int_distribution<int>(1, 3)(rng);
			const int h = std::uniform_int_distribution<int>(1, 3)(rng);
			const int x0 = std::uniform_int_distribution<int>(start_offset.x, start_offset.x + topo.layout->size.x - 1)(rng);
			const int y0 = std::uniform_int_distribution<int>(0, topo.layout->size.y - 1)(rng);
			SetNearTestRect(terrain, x0, x0 + w - 1, y0, y0 + h - 1, JunctionTileClass::Blocked);
		}
		run(terrain, seed);
	}
	for (int seed = 1001; seed <= 1050; seed++) {
		std::mt19937 rng(seed);
		JunctionTerrain terrain = MakeNearTestTerrain();
		terrain.build_on_slopes = true;
		const int r = std::uniform_int_distribution<int>(2, topo.layout->size.y - 4)(rng);
		SetNearRampRow(terrain, r, 0, 1, DiagDirection::SE);
		SetNearRampRow(terrain, r + 1, 1, 1, DiagDirection::Invalid);
		SetNearRampRow(terrain, r + 2, 1, 1, DiagDirection::Invalid);
		SetNearRampRow(terrain, r + 3, 0, 1, DiagDirection::NW);
		run(terrain, seed);
	}
	std::cout << "ORACLE terrains=" << n << " skipped=" << skipped << " accepted=" << accepted << " heights=" << heights << " fits=" << fits << " micros=" << total_search_micros << "\n";
	CHECK(accepted > 0);
	CHECK(heights == 0);
	CHECK(fits == 0);
}

/** The nodeoracle's 250 terrains (same seeds, generators and skip rule): each is passed to run with its seed. */
template <typename F>
static void ForEachOracleTerrain(const JunctionLayout &t_layout, TileIndexDiffC start_offset, F &&run)
{
	for (int seed = 1; seed <= 200; seed++) {
		std::mt19937 rng(seed);
		JunctionTerrain terrain = MakeNearTestTerrain();
		const int count = std::uniform_int_distribution<int>(1, 3)(rng);
		for (int i = 0; i < count; i++) {
			const int w = std::uniform_int_distribution<int>(1, 3)(rng);
			const int h = std::uniform_int_distribution<int>(1, 3)(rng);
			const int x0 = std::uniform_int_distribution<int>(start_offset.x, start_offset.x + t_layout.size.x - 1)(rng);
			const int y0 = std::uniform_int_distribution<int>(0, t_layout.size.y - 1)(rng);
			SetNearTestRect(terrain, x0, x0 + w - 1, y0, y0 + h - 1, JunctionTileClass::Blocked);
		}
		run(terrain, seed);
	}
	for (int seed = 1001; seed <= 1050; seed++) {
		std::mt19937 rng(seed);
		JunctionTerrain terrain = MakeNearTestTerrain();
		terrain.build_on_slopes = true;
		const int r = std::uniform_int_distribution<int>(2, t_layout.size.y - 4)(rng);
		SetNearRampRow(terrain, r, 0, 1, DiagDirection::SE);
		SetNearRampRow(terrain, r + 1, 1, 1, DiagDirection::Invalid);
		SetNearRampRow(terrain, r + 2, 1, 1, DiagDirection::Invalid);
		SetNearRampRow(terrain, r + 3, 0, 1, DiagDirection::NW);
		run(terrain, seed);
	}
}

TEST_CASE("placement on facts is buildable on random terrain", "[junction][placeoracle]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionTMapResult &topo = JunctionNodeTopologyT();
	REQUIRE(topo.layout.has_value());
	const JunctionFrame frame = MakeJunctionFrame(spec);
	/* The graph seed of the T renders the traced T's tiles (checked below), so its hub is the traced T's: the node test's start offset. */
	const TileIndexDiffC offset = {static_cast<int16_t>(frame.hub_x - topo.hub.x), static_cast<int16_t>(-topo.hub.y)};
	std::string error;
	const JunctionTopology &t = JunctionTTopology();
	const JunctionGraphSeed seed = MakeJunctionGraphSeed(t, spec, &error);
	INFO(error);
	REQUIRE(!seed.routes.empty());
	const JunctionTrackOrder order = OrderJunctionTrackGraph(t, seed.paths, seed.graph, &error);
	INFO(error);
	{
		/* The two layouts differ inside (the seed's bridges stagger otherwise) but have the same ports, so one frame and one hub. */
		const JunctionLayout seed_layout = RenderJunctionTrackPlacement(t, seed.paths, seed.graph, seed.structure, seed.programs, seed.placement, &error);
		auto port_set = [](const JunctionLayout &layout) {
			std::set<std::tuple<int, int, int>> ports;
			for (const JunctionPort &port : FindJunctionPorts(layout)) ports.emplace(port.tile.x, port.tile.y, static_cast<int>(port.trackdir));
			return ports;
		};
		CHECK((seed_layout.size.x == topo.layout->size.x && seed_layout.size.y == topo.layout->size.y));
		CHECK(!port_set(seed_layout).empty());
		CHECK(port_set(seed_layout) == port_set(*topo.layout));
	}

	int n = 0, skipped = 0, solved = 0, heights = 0, violations = 0;
	int64_t micros = 0;
	ForEachOracleTerrain(*topo.layout, offset, [&](const JunctionTerrain &terrain, int terrain_seed) {
		n++;
		if (JunctionBlockedPortApproach(terrain, offset.x, offset.x + topo.layout->size.x - 1).has_value()) {
			skipped++;
			return;
		}
		const JunctionFacts facts(terrain);
		std::string place_error;
		const auto start = std::chrono::steady_clock::now();
		const JunctionTrackPlacement placement = PlaceJunctionTrackGraph(t, seed.paths, seed.graph, order, seed.structure, seed.programs, spec, seed.placement.choices, facts, offset, &place_error);
		micros += std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
		if (placement.nodes.empty()) return;
		solved++;
		std::vector<JunctionTrackRoute> routes;
		std::string render_error;
		JunctionLayout layout = RenderJunctionTrackPlacement(t, seed.paths, seed.graph, seed.structure, seed.programs, placement, &render_error, &routes);
		INFO("seed " << terrain_seed << " " << render_error);
		REQUIRE(!layout.tracks.empty());
		std::string why;
		const bool ok = JunctionRoutesHeightsOk(routes, layout, {0, 0}, terrain, offset, &why);
		INFO("heights: " << why);
		CHECK(ok);
		if (!ok) heights++;
		SignalJunctionTrackRoutes(layout, routes, t, seed.paths, seed.graph, seed.programs, spec);
		for (const RuleViolation &violation : CheckJunctionRules(layout, spec, t)) {
			INFO("rule " << to_underlying(violation.rule) << " at " << violation.tile.x << "," << violation.tile.y << ": " << violation.message);
			CHECK(false);
			violations++;
		}
	});
	std::cout << "PLACEORACLE terrains=" << n << " skipped=" << skipped << " solved=" << solved << " heights=" << heights << " violations=" << violations << " micros=" << micros << "\n";
	CHECK(solved > 0);
	CHECK(heights == 0);
	CHECK(violations == 0);
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
	const std::atomic<bool> cancel{false};
	const JunctionNearResult near = GenerateJunctionNear(spec, terrain, 0, false, cancel);
	REQUIRE(near.result.layout.has_value());
	WARN("cost: offset " << near.offset.x << "," << near.offset.y);
	/* The terrain as GenerateJunctionNear shifts it at rotation 0, and the layout-to-frame offset from the hub. */
	JunctionTerrain shifted = terrain;
	shifted.origin = {static_cast<int16_t>(terrain.origin.x - near.offset.x), static_cast<int16_t>(terrain.origin.y - near.offset.y)};
	const Money cost = JunctionTMapTerrainCost(*near.result.layout, shifted, MakeJunctionFrame(spec).hub_x - near.result.hub.x, -near.result.hub.y);
	CHECK(cost > 0);
	CHECK(near.result.score.second == ScoreJunctionLayout(*near.result.layout, spec).second + cost);
}

TEST_CASE("GenerateJunctionNear reports a blocked port", "[junction][tmapnear]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionTMapResult flat = ConstructJunctionTMap(spec, nullptr, std::nullopt, true);
	REQUIRE(flat.layout.has_value());
	constexpr int STUB = 2;
	const int x_w = MakeJunctionFrame(spec).hub_x - flat.hub.x;
	JunctionTerrain terrain = MakeNearTestTerrain(8);
	/* The west port's approach at every near offset's position (rings out to 8 tiles, D195). */
	SetNearTestRect(terrain, x_w - 8, x_w + STUB + 8, -8, 8, JunctionTileClass::Blocked);
	const std::atomic<bool> cancel{false};
	const JunctionNearResult near = GenerateJunctionNear(spec, terrain, 0, false, cancel);
	CHECK(near.result.error == JunctionGenerateError::PortBlocked);
	CHECK(!near.result.layout.has_value());
}

TEST_CASE("EdgeZ matches the game's track surface on a real tile", "[junction][facts]")
{
	Map::Allocate(64, 64);
	const bool old_build_on_slopes = _settings_game.construction.build_on_slopes;
	_settings_game.construction.build_on_slopes = true;
	const TileIndex tile = TileXY(10, 10);
	const int base = 1;
	for (const Slope slope : {SLOPE_NE, SLOPE_SW, SLOPE_N, SLOPE_NWS, SLOPE_STEEP_N}) {
		for (const TrackBits bits : {TrackBits{Track::X}, TrackBits{Track::Y}, TrackBits{Track::Y, Track::Upper}, TrackBits{Track::X, Track::Lower}, TrackBits{Track::Upper}, TrackBits{Track::Lower}, TrackBits{Track::Left}, TrackBits{Track::Right}, TrackBits{Track::Upper, Track::Lower}}) {
			if (GetRailFoundation(slope, bits) == Foundation::Invalid) continue;
			INFO("slope " << static_cast<int>(slope) << " bits " << static_cast<int>(bits.base()));

			/* Corners N (0,0), E (0,1), S (1,1), W (1,0) of the tile, raised as the slope says; a steep slope has its highest corner raised once more. */
			const Corner highest = IsSteepSlope(slope) ? GetHighestSlopeCorner(slope) : Corner::Invalid;
			for (const auto &[corner, dx, dy] : {std::tuple{Corner::N, 0, 0}, {Corner::E, 0, 1}, {Corner::S, 1, 1}, {Corner::W, 1, 0}}) {
				const int raised = (slope & SlopeWithOneCornerRaised(corner)) != SLOPE_FLAT ? 1 : 0;
				SetTileHeight(TileXY(TileX(tile) + dx, TileY(tile) + dy), base + raised + (corner == highest ? 1 : 0));
			}
			REQUIRE(GetTileSlopeZ(tile) == std::tuple{slope, base});

			const JunctionTerrain terrain = MakeJunctionTerrainRect(tile, tile, 0);
			const JunctionFacts facts(terrain);
			REQUIRE(facts.Fits(TileX(tile), TileY(tile), bits));
			MakeRailNormal(tile, Owner{0}, bits, RAILTYPE_RAIL);

			static constexpr std::array<std::pair<int, int>, 4> MIDPOINT = {{{0, 8}, {8, 15}, {15, 8}, {8, 0}}}; // NE, SE, SW, NW.
			for (DiagDirection edge = DiagDirection::Begin; edge != DiagDirection::End; edge++) {
				bool touched = false;
				for (const Track track : bits) {
					const Trackdir td = TrackToTrackdir(track);
					touched |= TrackdirToExitdir(td) == edge || TrackdirToExitdir(ReverseTrackdir(td)) == edge;
				}
				if (!touched) continue;
				const auto [mx, my] = MIDPOINT[to_underlying(edge)];
				CHECK(facts.EdgeZ(TileX(tile), TileY(tile), bits, edge) == GetSlopePixelZ(TileX(tile) * TILE_SIZE + mx, TileY(tile) * TILE_SIZE + my));
			}
			Map::Allocate(64, 64);
		}
	}
	_settings_game.construction.build_on_slopes = old_build_on_slopes;
}

namespace {

/** The game state the commands need to price and run junction steps: a company with unlimited money and limits, the settings of a typical game, the vanilla bridges and rail types. Restores what it changes. */
struct JunctionGameWorld {
	bool created = false;
	CompanyID old_current = _current_company;
	CompanyID old_local = _local_company;
	GameSettings old_settings = _settings_game;

	JunctionGameWorld()
	{
		_settings_game.construction.build_on_slopes = true;
		_settings_game.construction.max_bridge_height = 12;
		_settings_game.construction.max_bridge_length = 16;
		_settings_game.construction.map_height_limit = 15;
		_settings_game.construction.extra_dynamite = false;
		ResetBridges();
		ResetRailTypes();
		InitRailTypes();
		TimerGameCalendar::year = TimerGameCalendar::Year{2000};
		RecomputePrices();
		Map::Allocate(256, 256);
		if (!Company::IsValidID(CompanyID::Begin())) {
			Company *c = Company::CreateAtIndex(CompanyID::Begin(), STR_SV_UNNAMED, false);
			c->avail_railtypes = RailTypes{RAILTYPE_RAIL};
			this->created = true;
		}
		_current_company = _local_company = CompanyID::Begin();
		Company *c = Company::Get(CompanyID::Begin());
		c->money = c->current_loan = INT64_MAX / 8;
		c->terraform_limit = c->clear_limit = c->tree_limit = c->build_object_limit = UINT32_MAX;
	}

	~JunctionGameWorld()
	{
		if (this->created) delete Company::Get(CompanyID::Begin());
		_current_company = this->old_current;
		_local_company = this->old_local;
		_settings_game = this->old_settings;
	}
};

/** Make every tile of the map clear grass of density 0, which the game clears for nothing. */
void MakeBareGrass(TileIndex first, uint size)
{
	for (uint y = 0; y < size; y++) {
		for (uint x = 0; x < size; x++) MakeClear(first + TileDiffXY(x, y), ClearGround::Grass, 0);
	}
}

}

TEST_CASE("RailPrice is what TestRail charges on a flat and on a foundation tile", "[junction][game][junctionprice]")
{
	JunctionGameWorld world;
	GameJunctionPricing pricing(RAILTYPE_RAIL);
	MakeBareGrass(TileXY(8, 8), 8);

	const TileIndex flat = TileXY(10, 10);
	for (Track track : {Track::X, Track::Y, Track::Upper}) {
		CommandCost result = pricing.TestRail(flat, track);
		REQUIRE(result.Succeeded());
		CHECK(result.GetCost() == pricing.RailPrice(track, false));
	}

	/* One raised corner (the tile's north corner) and a piece that needs a foundation on it. */
	const TileIndex sloped = TileXY(12, 12);
	SetTileHeight(sloped, 1);
	const Slope slope = GetTileSlope(sloped);
	std::optional<Track> found;
	for (Track track : {Track::X, Track::Y, Track::Upper, Track::Lower, Track::Left, Track::Right}) {
		const Foundation f = GetRailFoundation(slope, TrackBits{track});
		if (f != Foundation::Invalid && f != Foundation::None) { found = track; break; }
	}
	REQUIRE(found.has_value());
	CommandCost result = pricing.TestRail(sloped, *found);
	REQUIRE(result.Succeeded());
	CHECK(result.GetCost() == pricing.RailPrice(*found, true));
	CHECK(pricing.RailPrice(*found, true) > pricing.RailPrice(*found, false));
}

TEST_CASE("BridgePrice is what PriceBridge charges on flat ground", "[junction][game][junctionprice]")
{
	JunctionGameWorld world;
	GameJunctionPricing pricing(RAILTYPE_RAIL);
	MakeBareGrass(TileXY(8, 8), 12);
	/* Raise the span tiles? No: a bridge over flat ground needs the heads one level above nothing; flat is enough for a bridge over bare ground. */
	for (uint middle : {1, 3, 6}) {
		const TileIndex start = TileXY(10, 10);
		const TileIndex end = TileXY(10 + middle + 1, 10);
		CommandCost result = pricing.PriceBridge(start, end);
		REQUIRE(result.Succeeded());
		CHECK(result.GetCost() == pricing.BridgePrice(start, end));
		CHECK(pricing.BridgePrice(start, end) > 0);
	}
}

namespace {

/** What the estimate of one fixture's plan and the commands after its terraform say. */
struct IG2Counts {
	int planned = 0;
	int bad_before = 0; ///< Bad tiles of the estimate.
	int rail_bad_before = 0; ///< Of those, tiles that are no bridge head.
	int bad_not_failing_bridge_head = 0;
	int bridges_failing_after = 0;
	int bridges_failing_after_too_low = 0;
	int rail_bad_after = 0;
	Money worst_diff = 0;
	std::string worst_diff_fixture;
};

}

TEST_CASE("the in-game estimate of a terraformed plan has no red tile the terraform fixes", "[junction][junctiongame]")
{
	namespace fs = std::filesystem;
	JunctionGameWorld world;
	JunctionSpec spec = MakeTestJunctionSpec();
	spec.slope_steepness = 3;
	std::string error;
	const JunctionGraphSeed seed = MakeJunctionGraphSeed(JunctionTTopology(), spec, &error);
	REQUIRE(!seed.routes.empty());
	const int hub_x = seed.placement.size.x / 2;

	std::vector<fs::path> files;
	for (const fs::directory_entry &entry : fs::directory_iterator(JUNCTION_TERRAIN_DIR)) if (entry.path().extension() == ".json") files.push_back(entry.path());
	std::ranges::sort(files);

	IG2Counts counts;
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
		const JunctionHoverPlan plan = PlanJunctionHoverNear(seed, spec, terrain, {0, 0});
		if (!plan.layout.has_value()) continue;
		counts.planned++;
		INFO("fixture " << file.filename().string());

		/* The fixture as a game map: cell (cx, cy) is map tile anchor + (cx - hub_x, cy). */
		Map::Allocate(256, 256);
		const TileIndex anchor = TileXY(100, 100);
		auto tile_of = [&](int cx, int cy) { return TileXY(static_cast<int>(TileX(anchor)) + cx - hub_x, static_cast<int>(TileY(anchor)) + cy); };
		for (int fy = 0; fy < terrain.height; fy++) {
			for (int fx = 0; fx < terrain.width; fx++) {
				const TileIndex t = tile_of(terrain.origin.x + fx, terrain.origin.y + fy);
				const std::array<int, 4> c = JunctionTileCorners(terrain.tiles[static_cast<size_t>(fy) * terrain.width + fx]);
				SetTileHeight(TileXY(TileX(t) + 1, TileY(t)), c[0]);
				SetTileHeight(TileXY(TileX(t) + 1, TileY(t) + 1), c[1]);
				SetTileHeight(TileXY(TileX(t), TileY(t) + 1), c[2]);
				SetTileHeight(TileXY(TileX(t), TileY(t)), c[3]);
			}
		}
		for (int fy = 0; fy < terrain.height; fy++) {
			for (int fx = 0; fx < terrain.width; fx++) {
				const JunctionTerrainTile &cell = terrain.tiles[static_cast<size_t>(fy) * terrain.width + fx];
				const TileIndex t = tile_of(terrain.origin.x + fx, terrain.origin.y + fy);
				switch (cell.cls) {
					case JunctionTileClass::Buildable: MakeClear(t, ClearGround::Grass, 0); break;
					case JunctionTileClass::Water: if (cell.z_max == 0) MakeSea(t); else MakeRiver(t, 0); break;
					case JunctionTileClass::Spannable: MakeRailNormal(t, OWNER_NONE, TrackBits{Track::X}, RAILTYPE_RAIL); break;
					case JunctionTileClass::Blocked: MakeRailDepot(t, OWNER_NONE, DepotID::Begin(), DiagDirection::NE, RAILTYPE_RAIL); break;
				}
			}
		}

		/* As ShowProjection and ShowPlaced do. */
		const std::vector<JunctionTerraformStep> steps = MakeJunctionTerraformSteps(plan.terraform, terrain, anchor, 0, false, hub_x);
		const TileIndexDiffC hub = {static_cast<int16_t>(hub_x - plan.moved.x), static_cast<int16_t>(-plan.moved.y)};
		const TileIndexDiffC offset = OrientJunctionOffset(hub, plan.layout->size, 0, false);
		const TileIndex origin = TileXY(static_cast<int>(TileX(anchor)) - offset.x, static_cast<int>(TileY(anchor)) - offset.y);
		const JunctionLayout layout = ApplyJunctionOrientation(*plan.layout, 0, false);
		GameJunctionPricing pricing(RAILTYPE_RAIL);

		/* The estimate the GUI shows, before the terraform. */
		const JunctionEstimate estimate = EstimateJunctionTerraformed(layout, origin, pricing, plan.height_cost, steps);

		/* The commands after the terraform. */
		for (const JunctionTerraformStep &step : steps) {
			auto [result, cost, tile] = Command<Commands::TerraformLand>::Do(DoCommandFlag::Execute, step.tile, SlopeWithOneCornerRaised(step.corner), step.raise);
			REQUIRE(result.Succeeded());
		}
		std::set<TileIndex> failing_heads;
		Money commands = 0;
		Money signals = 0;
		for (const JunctionBuildStep &step : MakeJunctionBuildSteps(layout, origin)) {
			if (const JunctionRailStep *rail = std::get_if<JunctionRailStep>(&step); rail != nullptr) {
				const CommandCost result = pricing.TestRail(rail->tile, rail->track);
				if (result.Failed()) {
					counts.rail_bad_after++;
					INFO("rail tile " << rail->tile.base() << " track " << to_underlying(rail->track));
					CHECK(result.Succeeded());
				} else {
					commands += result.GetCost();
				}
			} else if (const JunctionBridgeStep *bridge = std::get_if<JunctionBridgeStep>(&step); bridge != nullptr) {
				const CommandCost result = bridge->tunnel ? pricing.PriceTunnel(bridge->start, bridge->end) : pricing.PriceBridge(bridge->start, bridge->end);
				if (result.Failed()) {
					counts.bridges_failing_after++;
					if (result.GetErrorMessage() == STR_ERROR_BRIDGE_TOO_LOW_FOR_TERRAIN) counts.bridges_failing_after_too_low++;
					failing_heads.insert(bridge->start);
					failing_heads.insert(bridge->end);
					commands += pricing.BridgePrice(bridge->start, bridge->end);
				} else {
					commands += result.GetCost();
				}
			} else if (std::holds_alternative<JunctionSignalStep>(step)) {
				signals += pricing.SignalCost();
			}
		}

		/* No bad tile except a head of a bridge the game still refuses after the terraform. */
		for (TileIndex bad : estimate.bad_tiles) {
			if (!failing_heads.contains(bad)) {
				counts.bad_not_failing_bridge_head++;
				INFO("bad tile " << TileX(bad) << "," << TileY(bad));
				CHECK(failing_heads.contains(bad));
			}
		}
		counts.bad_before += static_cast<int>(estimate.bad_tiles.size());
		std::set<TileIndex> heads;
		for (const JunctionBridge &bridge : layout.bridges) {
			heads.insert(AddTileIndexDiffCWrap(origin, bridge.start));
			heads.insert(AddTileIndexDiffCWrap(origin, bridge.end));
		}
		for (TileIndex bad : estimate.bad_tiles) {
			if (!heads.contains(bad)) counts.rail_bad_before++;
		}

		/* The total is the commands' sum, apart from the foundation price (the estimate charges it on every piece of a tile that needs one, the commands per piece that changes it). */
		const Money diff = estimate.total - plan.height_cost - signals - commands;
		if (std::abs(diff.base()) > std::abs(counts.worst_diff.base())) {
			counts.worst_diff = diff;
			counts.worst_diff_fixture = file.filename().string();
		}
		const Money foundation_price = _price[Price::BuildFoundation];
		CHECK(diff.base() % foundation_price.base() == 0);
	}
	std::cout << "IG2: planned " << counts.planned << ", estimate bad tiles " << counts.bad_before << " (rail: " << counts.rail_bad_before << "; not heads of a bridge failing after the terraform: " << counts.bad_not_failing_bridge_head << "), rail pieces failing after the terraform " << counts.rail_bad_after
		<< ", bridges failing after the terraform " << counts.bridges_failing_after << " (BRIDGE_TOO_LOW " << counts.bridges_failing_after_too_low << "), worst total difference " << counts.worst_diff << " in " << counts.worst_diff_fixture << std::endl;
	CHECK(counts.planned == 31);
	CHECK(counts.rail_bad_before == 0);
	CHECK(counts.bad_not_failing_bridge_head == 0);
	CHECK(counts.rail_bad_after == 0);
}
