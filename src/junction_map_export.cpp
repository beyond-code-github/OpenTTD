/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_map_export.cpp Export of a live map area as JSON, and a finder for hubs and big stations. */

#include "stdafx.h"

#include "junction_map_export.h"
#include "bridge_map.h"
#include "depot_map.h"
#include "rail_map.h"
#include "road_map.h"
#include "signal_func.h"
#include "station_base.h"
#include "station_map.h"
#include "tile_map.h"
#include "track_func.h"
#include "tunnelbridge_map.h"
#include "3rdparty/nlohmann/json.hpp"

#include <algorithm>
#include <map>
#include <set>

#include "safeguards.h"

/** Orientation text, as written by the savegame reader. */
static const char *const ORIENTATION_TEXT = "map x increases towards screen SW, y towards screen SE; tile (0,0) is the north corner; edges: NE=x-1, SE=y+1, SW=x+1, NW=y-1";

static const char *const TRACK_NAMES[] = {"X", "Y", "Upper", "Lower", "Left", "Right"};
static const char *const TRACKDIR_NAMES[] = {"X_NE", "Y_SE", "Upper_E", "Lower_E", "Left_S", "Right_S", "", "", "X_SW", "Y_NW", "Upper_W", "Lower_W", "Left_N", "Right_N"};
static const char *const DIRECTION_NAMES[] = {"NE", "SE", "SW", "NW"};
static const char *const SIGNAL_TYPE_NAMES[] = {"block", "entry", "exit", "combo", "path", "path_oneway"};

/**
 * Call a function for every signal on a plain rail tile.
 * A signal is reported once per track and trackdir it faces along, as the savegame reader does.
 * @param tile The rail tile with signals.
 * @param tracks The track bits of the tile.
 * @param visit Called with the track, trackdir, signal type, variant and state.
 */
template <typename Tvisit>
static void ForEachTileSignal(Tile tile, TrackBits tracks, Tvisit &&visit)
{
	for (Track track : tracks) {
		const Trackdir along[] = {TrackToTrackdir(track), ReverseTrackdir(TrackToTrackdir(track))};
		for (Trackdir td : along) {
			if (!HasSignalOnTrackdir(tile, td)) continue;
			visit(track, td, GetSignalType(tile, track), GetSignalVariant(tile, track), GetSignalStateByTrackdir(tile, td));
		}
	}
}

/**
 * Name an owner the way the savegame reader does.
 * @param owner The owner.
 * @return Text for the owner.
 */
static std::string OwnerName(Owner owner)
{
	switch (owner.base()) {
		case OWNER_TOWN.base(): return "town";
		case OWNER_NONE.base(): return "none";
		case OWNER_WATER.base(): return "water";
		default: return fmt::format("company{}", owner.base());
	}
}

/**
 * Whether the signals of a tile cannot be told apart by track.
 * @param tile The rail tile with signals.
 * @param tracks The track bits of the tile.
 * @param claimed Number of signal records produced for the tile.
 * @return True when the signal bits can belong to more than one track.
 */
static bool SignalsAmbiguous(Tile tile, TrackBits tracks, uint claimed)
{
	if (claimed != static_cast<uint>(std::popcount(GetPresentSignals(tile)))) return true;
	if (GetPresentSignals(tile) == 0) return false;
	for (Track a : tracks) {
		for (Track b : tracks) {
			if (a >= b) continue;
			if ((SignalOnTrack(a) & SignalOnTrack(b)) == 0) continue;
			const bool upper_lower = (a == Track::Upper || a == Track::Lower) && (b == Track::Upper || b == Track::Lower);
			const bool left_right = (a == Track::Left || a == Track::Right) && (b == Track::Left || b == Track::Right);
			if (!upper_lower && !left_right) return true;
		}
	}
	return false;
}

/**
 * Describe one tile for the export.
 * @param x X coordinate of the tile.
 * @param y Y coordinate of the tile.
 * @param record Receives the record.
 * @return True when the tile is of an exported kind.
 */
static bool DescribeTile(uint x, uint y, nlohmann::json &record)
{
	const Tile tile = TileXY(x, y);
	record = nlohmann::json::object();
	if (IsTileType(tile, TileType::Railway)) {
		record = {{"x", x}, {"y", y}, {"height", TileHeight(tile)}, {"tile_type", "rail"}, {"owner", OwnerName(GetTileOwner(tile))}, {"rail_type", to_underlying(GetRailType(tile))}};
		if (IsRailDepot(tile)) {
			record["kind"] = "depot";
			record["depot"] = {{"exit_dir", DIRECTION_NAMES[to_underlying(GetRailDepotDirection(tile))]}, {"depot_id", GetDepotIndex(tile).base()}};
			return true;
		}
		const TrackBits tracks = GetTrackBits(tile);
		record["tracks"] = nlohmann::json::array();
		for (Track track : tracks) record["tracks"].push_back(TRACK_NAMES[to_underlying(track)]);
		record["signals"] = nlohmann::json::array();
		if (GetRailTileType(tile) != RailTileType::Signals) {
			record["kind"] = "track";
			return true;
		}
		record["kind"] = "signals";
		uint claimed = 0;
		ForEachTileSignal(tile, tracks, [&](Track track, Trackdir td, SignalType type, SignalVariant variant, SignalState state) {
			record["signals"].push_back({{"track", TRACK_NAMES[to_underlying(track)]}, {"trackdir", TRACKDIR_NAMES[to_underlying(td)]}, {"exit_edge", DIRECTION_NAMES[to_underlying(TrackdirToExitdir(td))]},
					{"type", SIGNAL_TYPE_NAMES[to_underlying(type)]}, {"variant", variant == SignalVariant::Semaphore ? "semaphore" : "electric"}, {"state", state == SignalState::Green ? "green" : "red"}});
			claimed++;
		});
		if (SignalsAmbiguous(tile, tracks, claimed)) record["signals_ambiguous"] = true;
		return true;
	}
	if (IsTileType(tile, TileType::TunnelBridge)) {
		if (GetTunnelBridgeTransportType(tile) != TransportType::Rail) return false;
		const bool bridge = IsBridge(tile);
		record = {{"x", x}, {"y", y}, {"height", TileHeight(tile)}, {"tile_type", "tunnelbridge"}, {"owner", OwnerName(GetTileOwner(tile))}, {"kind", bridge ? "bridge" : "tunnel"},
				{"transport", "rail"}, {"direction", DIRECTION_NAMES[to_underlying(GetTunnelBridgeDirection(tile))]}};
		if (bridge) record["bridge_type"] = GetBridgeType(tile);
		record["rail_type"] = to_underlying(GetRailType(tile));
		const TileIndex far_end = GetOtherTunnelBridgeEnd(tile);
		record["far_end"] = far_end == INVALID_TILE ? nlohmann::json(nullptr) : nlohmann::json{{"x", TileX(far_end)}, {"y", TileY(far_end)}};
		return true;
	}
	if (IsTileType(tile, TileType::Station)) {
		if (!HasStationRail(tile)) return false;
		const StationID sid = GetStationIndex(tile);
		const BaseStation *st = BaseStation::GetIfValid(sid);
		record = {{"x", x}, {"y", y}, {"height", TileHeight(tile)}, {"tile_type", "station"}, {"owner", OwnerName(GetTileOwner(tile))}, {"station_index", sid.base()},
				{"station_name", st != nullptr ? nlohmann::json(st->GetCachedName()) : nlohmann::json(nullptr)}, {"axis", GetRailStationAxis(tile) == Axis::Y ? "Y" : "X"},
				{"waypoint", IsRailWaypoint(tile)}, {"gfx", GetStationGfx(tile)}, {"station_type", IsRailWaypoint(tile) ? "rail_waypoint" : "rail"}, {"rail_type", to_underlying(GetRailType(tile))}};
		return true;
	}
	if (IsLevelCrossingTile(tile)) {
		const bool road_y = GetCrossingRoadAxis(tile) == Axis::Y;
		record = {{"x", x}, {"y", y}, {"height", TileHeight(tile)}, {"tile_type", "road"}, {"owner", OwnerName(GetTileOwner(tile))}, {"kind", "level_crossing"},
				{"road_axis", road_y ? "Y" : "X"}, {"rail_axis", road_y ? "X" : "Y"}, {"rail_type", to_underlying(GetRailType(tile))}};
		return true;
	}
	return false;
}

/**
 * Write a map area as JSON, in the schema of the savegame reader.
 * @param x0 West-most X of the inclusive box.
 * @param y0 North-most Y of the inclusive box.
 * @param x1 East-most X of the inclusive box.
 * @param y1 South-most Y of the inclusive box.
 * @return The JSON text with one record per rail, rail tunnel/bridge, rail station, waypoint and level crossing tile.
 */
std::string JunctionMapExportJson(uint x0, uint y0, uint x1, uint y1)
{
	nlohmann::json tiles = nlohmann::json::array();
	nlohmann::json record;
	for (uint y = y0; y <= y1 && y < Map::SizeY(); y++) {
		for (uint x = x0; x <= x1 && x < Map::SizeX(); x++) {
			if (DescribeTile(x, y, record)) tiles.push_back(record);
		}
	}
	nlohmann::json out = {{"map_size", {{"x", Map::SizeX()}, {"y", Map::SizeY()}}}, {"orientation", ORIENTATION_TEXT}, {"box", {{"x0", x0}, {"y0", y0}, {"x1", x1}, {"y1", y1}}}, {"tiles", tiles}};
	return out.dump(1);
}

/** Counts of one window while ranking, before it becomes a hotspot. */
struct WindowCounts {
	uint presignals = 0;
	uint pbs = 0;
	uint bridge_heads = 0;
	uint station_tiles = 0;
	std::set<StationID> stations;
};

/**
 * Rank map windows by how much hub and station infrastructure they hold.
 * @param top_k Number of hotspots to return.
 * @param window Side length of a window; windows start every window / 2 tiles and are clipped to the map.
 * @return The best windows, by score, ties by smaller y0 then x0.
 */
std::vector<JunctionHotspot> JunctionMapHotspots(uint top_k, uint window)
{
	std::vector<JunctionHotspot> result;
	if (top_k == 0 || window == 0) return result;
	const uint stride = std::max(1U, window / 2);
	std::map<std::pair<uint, uint>, WindowCounts> windows; // Keyed by (window row, window column).

	/* Add one tile's contribution to every window that contains it. */
	auto add = [&](uint x, uint y, auto &&bump) {
		const uint i_lo = x + 1 > window ? (x + 1 - window + stride - 1) / stride : 0;
		const uint j_lo = y + 1 > window ? (y + 1 - window + stride - 1) / stride : 0;
		for (uint j = j_lo; j <= y / stride; j++) {
			for (uint i = i_lo; i <= x / stride; i++) bump(windows[{j, i}]);
		}
	};

	for (uint y = 0; y < Map::SizeY(); y++) {
		for (uint x = 0; x < Map::SizeX(); x++) {
			const Tile tile = TileXY(x, y);
			if (IsTileType(tile, TileType::Railway) && GetRailTileType(tile) == RailTileType::Signals) {
				uint presignals = 0;
				uint pbs = 0;
				ForEachTileSignal(tile, GetTrackBits(tile), [&](Track, Trackdir, SignalType type, SignalVariant, SignalState) {
					if (IsPbsSignal(type)) {
						pbs++;
					} else if (type != SignalType::Block) {
						presignals++;
					}
				});
				if (presignals + pbs > 0) add(x, y, [&](WindowCounts &w) { w.presignals += presignals; w.pbs += pbs; });
			} else if (IsTileType(tile, TileType::TunnelBridge)) {
				if (IsBridge(tile) && GetTunnelBridgeTransportType(tile) == TransportType::Rail) add(x, y, [](WindowCounts &w) { w.bridge_heads++; });
			} else if (IsRailStationTile(tile)) {
				const StationID sid = GetStationIndex(tile);
				add(x, y, [sid](WindowCounts &w) { w.station_tiles++; w.stations.insert(sid); });
			}
		}
	}

	for (const auto &[key, counts] : windows) {
		JunctionHotspot spot;
		spot.x0 = key.second * stride;
		spot.y0 = key.first * stride;
		spot.x1 = std::min(spot.x0 + window, Map::SizeX()) - 1;
		spot.y1 = std::min(spot.y0 + window, Map::SizeY()) - 1;
		spot.presignals = counts.presignals;
		spot.pbs = counts.pbs;
		spot.bridge_heads = counts.bridge_heads;
		spot.station_tiles = counts.station_tiles;
		spot.score = 3 * counts.presignals + 2 * counts.bridge_heads + counts.station_tiles / 4.0 + counts.pbs / 2.0;
		spot.stations.assign(counts.stations.begin(), counts.stations.end());
		result.push_back(std::move(spot));
	}
	std::sort(result.begin(), result.end(), [](const JunctionHotspot &a, const JunctionHotspot &b) {
		if (a.score != b.score) return a.score > b.score;
		if (a.y0 != b.y0) return a.y0 < b.y0;
		return a.x0 < b.x0;
	});
	if (result.size() > top_k) result.resize(top_k);
	return result;
}
