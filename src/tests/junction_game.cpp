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
	JunctionSpec spec = MakeJunctionSpec(RAILTYPE_RAIL, 2, 3, 5);
	CHECK(spec.ml_gap == 2);
	CHECK(spec.branch_gap == 3);
	CHECK(spec.tl == 5);
	CHECK(spec.sd == 7);
	CHECK(spec.curve_speed == 3);
	CHECK(spec.curve_limit);
	CHECK(spec.signal_type == SignalType::Block);
	CHECK(spec.reference_speed == 120);
	CHECK(GetJunctionReferenceSpeed(RAILTYPE_RAIL) == 120);

	_settings_client.gui.junction_signal_type = 1;
	_settings_game.vehicle.train_acceleration_model = AccelerationModel::Original;
	spec = MakeJunctionSpec(RAILTYPE_RAIL, 1, 1, 4);
	CHECK(spec.signal_type == SignalType::PathOneWay);
	CHECK_FALSE(spec.curve_limit);

	_settings_client.gui.drag_signals_density = old_sd;
	_settings_client.gui.junction_signal_type = old_signal;
	_settings_game.vehicle.train_acceleration_model = old_model;
	_railtypes[RAILTYPE_RAIL].curve_speed = old_curve_speed;
	_railtypes[RAILTYPE_RAIL].max_speed = old_max_speed;
}

TEST_CASE("GameJunctionPricing: flat tile reports its height")
{
	Map::Allocate(64, 64);
	GameJunctionPricing pricing(RAILTYPE_RAIL);
	CHECK(pricing.GetFlatHeight(TileXY(10, 10)) == 0);
}

TEST_CASE("GameJunctionPricing: raised corner is not flat")
{
	Map::Allocate(64, 64);
	GameJunctionPricing pricing(RAILTYPE_RAIL);
	SetTileHeight(TileXY(11, 11), 1);
	CHECK(pricing.GetFlatHeight(TileXY(10, 10)) == std::nullopt);
}

TEST_CASE("GameJunctionPricing: raised flat tile reports its height")
{
	Map::Allocate(64, 64);
	GameJunctionPricing pricing(RAILTYPE_RAIL);
	SetTileHeight(TileXY(20, 20), 2);
	SetTileHeight(TileXY(21, 20), 2);
	SetTileHeight(TileXY(20, 21), 2);
	SetTileHeight(TileXY(21, 21), 2);
	CHECK(pricing.GetFlatHeight(TileXY(20, 20)) == 2);
}

TEST_CASE("GameJunctionPricing: off-map tile has no height")
{
	Map::Allocate(64, 64);
	GameJunctionPricing pricing(RAILTYPE_RAIL);
	CHECK(pricing.GetFlatHeight(INVALID_TILE) == std::nullopt);
}
