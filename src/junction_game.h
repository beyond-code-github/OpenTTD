/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_game.h Game-side pricing for the junction tool. */

#ifndef JUNCTION_GAME_H
#define JUNCTION_GAME_H

#include "junction_layout.h"
#include "junction_build.h"
#include "rail_type.h"
#include "bridge_type.h"
#include "junction_generator.h"
#include "junction_terrain.h"
#include "junction_train_model.h"

struct Vehicle;

#include <optional>
#include <string>

#include <atomic>

/**
 * Choose the bridge type for a bridge with 'length' middle tiles.
 * A speed-limited rail type gets the cheapest available bridge at least as fast as the rail type (D35).
 * An unlimited rail type gets the cheapest available bridge at least as fast as the fastest buildable train engine that runs on it, or the fastest available bridge if none is (D45).
 */
std::optional<BridgeType> ChooseJunctionBridgeType(RailType railtype, uint length);

/** The speed the junction is designed for: the rail type's limit, or the fastest buildable engine when unlimited (D45, D67). */
uint16_t GetJunctionReferenceSpeed(RailType railtype);
/** What the junction tool takes from a picked train (D101). */
struct JunctionPickedTrain {
	JunctionTrainParams loaded; ///< Loaded mass (sum of GetMaxWeight over parts), effective power, TE and air drag from the caches; max_speed = min(train max, rail type max).
	uint8_t tl = 0; ///< Train length in whole tiles, rounded up: ceil(cached_total_length / TILE_SIZE).
	std::string name; ///< The vehicle's name, as GetString(STR_VEHICLE_NAME, id) gives it.
};

/** Read a train for the junction tool; std::nullopt if v is not a train of the local company. */
std::optional<JunctionPickedTrain> ReadJunctionTrain(const Vehicle *v);

/** Fill a generator spec from the game: settings, rail type, prices and bridges. */
JunctionSpec MakeJunctionSpec(RailType railtype, uint8_t ml_gap, uint8_t branch_gap, uint8_t tl);

/** Snapshot the map around an anchor for the generator, in its working frame (D93): cell (cx, cy) is map tile anchor + OrientJunctionVector({cx - frame.hub_x, cy}, rotation, mirrored). */
JunctionTerrain MakeJunctionTerrain(TileIndex anchor, uint8_t rotation, bool mirrored, const JunctionFrame &frame, int margin = 0);

/** Result of a search for a fitting spot near the anchor. */
struct JunctionNearResult {
	JunctionGenerateResult result;
	TileIndexDiffC offset{}; ///< Map offset from the requested anchor to the anchor that fits.
};
/**
 * Try the anchor, then nearby anchors, using one snapshot taken with a margin of at least the largest offset.
 * Trying map offset d uses the same snapshot with origin shifted by -InverseOrientJunctionVector(d, rotation, mirrored).
 * Stops at the first layout found, or when cancel becomes true.
 */
JunctionNearResult GenerateJunctionNear(const JunctionSpec &spec, const JunctionTerrain &terrain, uint8_t rotation, bool mirrored, const std::atomic<bool> &cancel);

/**
 * The instant tier (D96): fit the library at the anchor, then at the nearby offsets, using the same shifted snapshots as GenerateJunctionNear.
 * Stops at the first offset with a fit.
 * @return The fitted entry's layout as a result (error None, hub and score from the entry, terrain cost added to score.first), with its offset;
 *         or an empty result with error NoLayout and offset (0,0) when nothing fits.
 */
JunctionNearResult FitJunctionLibraryNear(const std::vector<JunctionLibraryEntry> &library, const JunctionTerrain &terrain, uint8_t rotation, bool mirrored);

/** Junction pricing against the real map, commands and prices, for the local company. */
class GameJunctionPricing : public JunctionPricing {
public:
	explicit GameJunctionPricing(RailType railtype) : railtype(railtype) {}
	std::optional<JunctionTileSlope> GetTileSlope(TileIndex tile) const override;
	CommandCost TestRail(TileIndex tile, Track track) const override;
	CommandCost PriceBridge(TileIndex start, TileIndex end) const override;
	CommandCost TestClear(TileIndex tile) const override;
	Money SignalCost() const override;
	Money AvailableMoney() const override;

private:
	RailType railtype; ///< Rail type the junction is built in.
};

/** Posts junction build steps as real commands for the local company. */
class GameJunctionStepPoster : public JunctionStepPoster {
public:
	explicit GameJunctionStepPoster(RailType railtype) : railtype(railtype) {}
	bool Post(const JunctionBuildStep &step) override;

private:
	RailType railtype; ///< Rail type the junction is built in.
};

JunctionSequencer &GetJunctionSequencer();
void CcJunctionStep(Commands cmd, const CommandCost &result, TileIndex tile);

#endif /* JUNCTION_GAME_H */
