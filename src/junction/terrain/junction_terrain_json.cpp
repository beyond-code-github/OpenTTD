/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_terrain_json.cpp Terrain snapshots as JSON, for test fixtures from real maps. */

#include "../../stdafx.h"

#include "junction_terrain_json.h"
#include "../../3rdparty/nlohmann/json.hpp"

#include "../../safeguards.h"

/**
 * The members of one tile, in the order of its integer array:
 * 0 z_min, 1 z_max, 2 steep (0 or 1), 3 incline (DiagDirection, 255 for Invalid), 4 slope, 5 class (JunctionTileClass), 6 clear_cost.
 */
static constexpr size_t TILE_MEMBERS = 7;

std::string JunctionTerrainToJson(const JunctionTerrain &terrain)
{
	nlohmann::json rows = nlohmann::json::array();
	for (int y = 0; y < terrain.height; y++) {
		nlohmann::json row = nlohmann::json::array();
		for (int x = 0; x < terrain.width; x++) {
			const JunctionTerrainTile &t = terrain.tiles[static_cast<size_t>(y) * terrain.width + x];
			row.push_back(nlohmann::json::array({t.z_min, t.z_max, t.steep ? 1 : 0, to_underlying(t.incline), to_underlying(t.slope), to_underlying(t.cls), static_cast<int64_t>(t.clear_cost)}));
		}
		rows.push_back(std::move(row));
	}
	const nlohmann::json out = {
		{"origin", {terrain.origin.x, terrain.origin.y}},
		{"width", terrain.width},
		{"height", terrain.height},
		{"foundation_cost", static_cast<int64_t>(terrain.foundation_cost)},
		{"terraform_cost", static_cast<int64_t>(terrain.terraform_cost)},
		{"build_on_slopes", terrain.build_on_slopes},
		{"max_bridge_height", terrain.max_bridge_height},
		{"max_height", terrain.max_height},
		{"tiles", rows},
	};
	return out.dump();
}

std::optional<JunctionTerrain> JunctionTerrainFromJson(std::string_view json, std::string *error)
{
	const auto fail = [&](const std::string &reason) -> std::optional<JunctionTerrain> {
		if (error != nullptr) *error = reason;
		return std::nullopt;
	};
	try {
		const nlohmann::json in = nlohmann::json::parse(json);
		JunctionTerrain terrain;
		const auto &origin = in.at("origin");
		if (!origin.is_array() || origin.size() != 2) return fail("origin is not a pair");
		terrain.origin = {origin[0].get<int16_t>(), origin[1].get<int16_t>()};
		terrain.width = in.at("width").get<int>();
		terrain.height = in.at("height").get<int>();
		if (terrain.width < 0 || terrain.height < 0) return fail("negative size");
		terrain.foundation_cost = static_cast<Money>(in.at("foundation_cost").get<int64_t>());
		terrain.terraform_cost = static_cast<Money>(in.at("terraform_cost").get<int64_t>());
		terrain.build_on_slopes = in.at("build_on_slopes").get<bool>();
		terrain.max_bridge_height = in.at("max_bridge_height").get<int>();
		if (in.contains("max_height")) terrain.max_height = in.at("max_height").get<int>();
		const auto &rows = in.at("tiles");
		if (!rows.is_array() || rows.size() != static_cast<size_t>(terrain.height)) return fail("tile rows do not match the height");
		terrain.tiles.reserve(static_cast<size_t>(terrain.width) * terrain.height);
		for (const auto &row : rows) {
			if (!row.is_array() || row.size() != static_cast<size_t>(terrain.width)) return fail("a tile row does not match the width");
			for (const auto &entry : row) {
				if (!entry.is_array() || entry.size() != TILE_MEMBERS) return fail("a tile is not an array of 7 integers");
				JunctionTerrainTile t;
				t.z_min = entry[0].get<int8_t>();
				t.z_max = entry[1].get<int8_t>();
				t.steep = entry[2].get<int>() != 0;
				t.incline = static_cast<DiagDirection>(entry[3].get<uint8_t>());
				t.slope = static_cast<Slope>(entry[4].get<uint8_t>());
				const int cls = entry[5].get<int>();
				if (cls < 0 || cls > to_underlying(JunctionTileClass::Blocked)) return fail("tile class out of range");
				t.cls = static_cast<JunctionTileClass>(cls);
				t.clear_cost = static_cast<Money>(entry[6].get<int64_t>());
				terrain.tiles.push_back(t);
			}
		}
		return terrain;
	} catch (const nlohmann::json::exception &e) {
		return fail(e.what());
	}
}
