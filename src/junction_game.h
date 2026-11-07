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

/**
 * Choose the bridge type for a bridge with 'length' middle tiles.
 * A speed-limited rail type gets the cheapest available bridge at least as fast as the rail type (D35).
 * An unlimited rail type gets the cheapest available bridge at least as fast as the fastest buildable train engine that runs on it, or the fastest available bridge if none is (D45).
 */
std::optional<BridgeType> ChooseJunctionBridgeType(RailType railtype, uint length);

/** Junction pricing against the real map, commands and prices, for the local company. */
class GameJunctionPricing : public JunctionPricing {
public:
	explicit GameJunctionPricing(RailType railtype) : railtype(railtype) {}
	std::optional<int> GetFlatHeight(TileIndex tile) const override;
	CommandCost TestRail(TileIndex tile, Track track) const override;
	CommandCost PriceBridge(TileIndex start, TileIndex end) const override;
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
