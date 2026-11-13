/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_terrain_json.h Terrain snapshots as JSON, for test fixtures from real maps. */

#ifndef JUNCTION_TERRAIN_JSON_H
#define JUNCTION_TERRAIN_JSON_H

#include "junction_terrain.h"

#include <optional>
#include <string>
#include <string_view>

/** A terrain snapshot as JSON (test fixtures from real maps): every member of JunctionTerrain and each tile's members, tiles as one compact array per row. */
std::string JunctionTerrainToJson(const JunctionTerrain &terrain);

/** The snapshot back; nullopt with the reason in error on a malformed file. */
std::optional<JunctionTerrain> JunctionTerrainFromJson(std::string_view json, std::string *error);

#endif /* JUNCTION_TERRAIN_JSON_H */
