/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_map_export.cpp Test functionality from junction_map_export. */

#include "../../../stdafx.h"

#include "../../../3rdparty/catch2/catch.hpp"
#include "../../../3rdparty/nlohmann/json.hpp"

#include "../../../bridge_map.h"
#include "../../../junction/game/junction_map_export.h"
#include "../../../map_func.h"
#include "../../../rail_map.h"
#include "../../../road_map.h"
#include "../../../signal_func.h"
#include "../../../station_map.h"

#include "../../../safeguards.h"

/**
 * Lay a straight X rail tile owned by company 0.
 * @param x X coordinate.
 * @param y Y coordinate.
 */
static void LayRailX(uint x, uint y)
{
	MakeRailNormal(TileXY(x, y), Owner{0}, TrackBits{Track::X}, RAILTYPE_RAIL);
}

/**
 * Put one signal on an X rail tile.
 * @param x X coordinate.
 * @param y Y coordinate.
 * @param td Trackdir the signal faces along.
 * @param type Signal type.
 * @param green Whether the signal is green.
 */
static void PlaceSignalX(uint x, uint y, Trackdir td, SignalType type, bool green)
{
	const Tile tile = TileXY(x, y);
	SetHasSignals(tile, true);
	SetPresentSignals(tile, SignalAlongTrackdir(td));
	SetSignalType(tile, Track::X, type);
	SetSignalVariant(tile, Track::X, SignalVariant::Electric);
	SetSignalStates(tile, green ? SignalAlongTrackdir(td) : 0);
}

/**
 * Find the record of a tile in the exported JSON.
 * @param tiles The "tiles" array.
 * @param x X coordinate.
 * @param y Y coordinate.
 * @return The record, or null when the tile was not exported.
 */
static nlohmann::json FindRecord(const nlohmann::json &tiles, uint x, uint y)
{
	for (const auto &record : tiles) {
		if (record["x"] == x && record["y"] == y) return record;
	}
	return nullptr;
}

TEST_CASE("map_export JSON of a hand-built area", "[junction][mapexport]")
{
	Map::Allocate(64, 64);
	for (uint x = 5; x <= 8; x++) LayRailX(x, 10);
	PlaceSignalX(6, 10, Trackdir::X_NE, SignalType::Block, false);
	PlaceSignalX(7, 10, Trackdir::X_SW, SignalType::PathOneWay, true);
	MakeRailBridgeRamp(TileXY(10, 10), Owner{0}, 0, DiagDirection::SW, RAILTYPE_RAIL);
	MakeRailBridgeRamp(TileXY(14, 10), Owner{0}, 0, DiagDirection::NE, RAILTYPE_RAIL);
	MakeRoadCrossing(TileXY(20, 10), OWNER_NONE, OWNER_NONE, Owner{0}, Axis::Y, RAILTYPE_RAIL, ROADTYPE_ROAD, INVALID_ROADTYPE, TownID{0});

	const nlohmann::json out = nlohmann::json::parse(JunctionMapExportJson(4, 9, 21, 11));
	CHECK(out["map_size"]["x"] == 64);
	CHECK(out["box"]["x1"] == 21);
	REQUIRE(out["tiles"].size() == 7);

	const auto &tiles = out["tiles"];
	const nlohmann::json plain = FindRecord(tiles, 5, 10);
	REQUIRE(!plain.is_null());
	CHECK(plain["tile_type"] == "rail");
	CHECK(plain["kind"] == "track");
	CHECK(plain["tracks"] == nlohmann::json::array({"X"}));
	CHECK(plain["owner"] == "company0");

	const nlohmann::json block = FindRecord(tiles, 6, 10);
	REQUIRE(!block.is_null());
	CHECK(block["kind"] == "signals");
	REQUIRE(block["signals"].size() == 1);
	CHECK(block["signals"][0]["track"] == "X");
	CHECK(block["signals"][0]["type"] == "block");
	CHECK(block["signals"][0]["trackdir"] == "X_NE");
	CHECK(block["signals"][0]["exit_edge"] == "NE");
	CHECK(block["signals"][0]["variant"] == "electric");
	CHECK(block["signals"][0]["state"] == "red");

	const nlohmann::json pbs = FindRecord(tiles, 7, 10);
	REQUIRE(!pbs.is_null());
	REQUIRE(pbs["signals"].size() == 1);
	CHECK(pbs["signals"][0]["type"] == "path_oneway");
	CHECK(pbs["signals"][0]["trackdir"] == "X_SW");
	CHECK(pbs["signals"][0]["exit_edge"] == "SW");
	CHECK(pbs["signals"][0]["state"] == "green");

	const nlohmann::json head = FindRecord(tiles, 10, 10);
	REQUIRE(!head.is_null());
	CHECK(head["kind"] == "bridge");
	CHECK(head["transport"] == "rail");
	CHECK(head["direction"] == "SW");
	CHECK(head["far_end"]["x"] == 14);
	CHECK(head["far_end"]["y"] == 10);
	CHECK(FindRecord(tiles, 14, 10)["far_end"]["x"] == 10);

	const nlohmann::json crossing = FindRecord(tiles, 20, 10);
	REQUIRE(!crossing.is_null());
	CHECK(crossing["tile_type"] == "road");
	CHECK(crossing["kind"] == "level_crossing");
	CHECK(crossing["road_axis"] == "Y");
	CHECK(crossing["rail_axis"] == "X");
}

TEST_CASE("Hotspots rank windows by score", "[junction][mapexport]")
{
	Map::Allocate(64, 64);
	/* The quiet area: an entry signal, a path signal and a bridge. */
	LayRailX(4, 4);
	PlaceSignalX(4, 4, Trackdir::X_NE, SignalType::Entry, false);
	LayRailX(5, 4);
	PlaceSignalX(5, 4, Trackdir::X_NE, SignalType::Path, false);
	MakeRailBridgeRamp(TileXY(6, 4), Owner{0}, 0, DiagDirection::SW, RAILTYPE_RAIL);
	MakeRailBridgeRamp(TileXY(10, 4), Owner{0}, 0, DiagDirection::NE, RAILTYPE_RAIL);
	/* The busy area: three pre-signals and a station tile. */
	const SignalType busy[] = {SignalType::Entry, SignalType::Exit, SignalType::Combo};
	for (uint i = 0; i < 3; i++) {
		LayRailX(40 + i, 40);
		PlaceSignalX(40 + i, 40, Trackdir::X_NE, busy[i], false);
	}
	MakeRailStation(TileXY(43, 40), Owner{0}, StationID{7}, Axis::X, 0, RAILTYPE_RAIL);

	const std::vector<JunctionHotspot> spots = JunctionMapHotspots(5, 16);
	REQUIRE(spots.size() == 5);

	CHECK(spots[0].x0 == 32);
	CHECK(spots[0].y0 == 32);
	CHECK(spots[0].x1 == 47);
	CHECK(spots[0].y1 == 47);
	CHECK(spots[0].presignals == 3);
	CHECK(spots[0].pbs == 0);
	CHECK(spots[0].bridge_heads == 0);
	CHECK(spots[0].station_tiles == 1);
	CHECK(spots[0].score == 9.25);
	CHECK(spots[0].stations == std::vector<StationID>{StationID{7}});

	/* The three other windows holding the busy area tie with the first and follow it by y0, then x0. */
	CHECK(spots[1].x0 == 40);
	CHECK(spots[1].y0 == 32);
	CHECK(spots[2].x0 == 32);
	CHECK(spots[2].y0 == 40);
	CHECK(spots[3].x0 == 40);
	CHECK(spots[3].y0 == 40);
	CHECK(spots[3].score == 9.25);

	CHECK(spots[4].x0 == 0);
	CHECK(spots[4].y0 == 0);
	CHECK(spots[4].presignals == 1);
	CHECK(spots[4].pbs == 1);
	CHECK(spots[4].bridge_heads == 2);
	CHECK(spots[4].station_tiles == 0);
	CHECK(spots[4].score == 7.5);
	CHECK(spots[4].stations.empty());
}
