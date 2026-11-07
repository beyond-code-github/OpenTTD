/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_layout.h Junction layout description, build steps and price estimate. */

#ifndef JUNCTION_LAYOUT_H
#define JUNCTION_LAYOUT_H

#include "map_type.h"
#include "track_type.h"
#include "signal_type.h"
#include "command_type.h"
#include "economy_type.h"
#include "strings_type.h"
#include "tile_type.h"

#include <optional>
#include <variant>
#include <vector>

/** Track pieces on one tile of a junction. */
struct JunctionTrack {
	TileIndexDiffC offset;
	TrackBits tracks;
};

/** A one-way signal; trains travelling along 'facing' see it (SignalAlongTrackdir). */
struct JunctionSignal {
	TileIndexDiffC offset;
	Trackdir facing;
	SignalType type;
	SignalVariant variant;
};

/** A rail bridge between two heads on the same row or column; the bridge type is chosen at placement. */
struct JunctionBridge {
	TileIndexDiffC start;
	TileIndexDiffC end;
};

/** A junction relative to its north corner (0,0); all offsets have x and y >= 0. */
struct JunctionLayout {
	TileIndexDiffC size; ///< Bounding box in tiles.
	std::vector<JunctionTrack> tracks;
	std::vector<JunctionSignal> signals;
	std::vector<JunctionBridge> bridges;
};

/** The ways a junction layout can be transformed. */
enum class JunctionTransform : uint8_t { Rotate, Mirror };

/** Build step: one rail piece. */
struct JunctionRailStep { TileIndex tile; Track track; };
/** Build step: one bridge. */
struct JunctionBridgeStep { TileIndex start; TileIndex end; };
/** Build step: one signal. */
struct JunctionSignalStep { TileIndex tile; Trackdir facing; SignalType type; SignalVariant variant; };
/** Any one build step. */
using JunctionBuildStep = std::variant<JunctionRailStep, JunctionBridgeStep, JunctionSignalStep>;

/** Everything the estimate needs from the game; tests provide a stub. */
class JunctionPricing {
public:
	virtual ~JunctionPricing() = default;
	/** Height of a flat tile on the map, or std::nullopt if off-map or not flat. */
	virtual std::optional<int> GetFlatHeight(TileIndex tile) const = 0;
	/** Test-only run of building one rail piece; the cost includes clearing. */
	virtual CommandCost TestRail(TileIndex tile, Track track) const = 0;
	/** Full price of the cheapest suitable bridge type between two heads, or an error. */
	virtual CommandCost PriceBridge(TileIndex start, TileIndex end) const = 0;
	/** Test-only run of clearing a tile; its cost is what building anything on the tile would pay to clear it first. */
	virtual CommandCost TestClear(TileIndex tile) const = 0;
	/** Price of one signal. */
	virtual Money SignalCost() const = 0;
	/** Money the local company can spend. */
	virtual Money AvailableMoney() const = 0;
};

/** Up-front price and validity of placing a junction. */
struct JunctionEstimate {
	Money total = 0;
	std::vector<TileIndex> bad_tiles; ///< Tiles that are off-map, not flat, at the wrong height, or whose step failed.
	StringID first_error = INVALID_STRING_ID;
	bool affordable = false; ///< bad_tiles is empty and total <= AvailableMoney().
	int height = 0; ///< Ground height of the junction in height levels (the reference height).
};

JunctionLayout TransformJunctionLayout(const JunctionLayout &layout, JunctionTransform transform);
/** Transform a track by a rotation or a mirror. */
Track TransformTrack(Track track, JunctionTransform transform);
/** Transform a trackdir by a rotation or a mirror. */
Trackdir TransformTrackdir(Trackdir trackdir, JunctionTransform transform);
/** Mirror (if set), then rotate 'rotation % 4' quarter turns. */
JunctionLayout ApplyJunctionOrientation(const JunctionLayout &layout, uint8_t rotation, bool mirrored);
JunctionLayout MakeTestJunctionLayout();
std::vector<JunctionBuildStep> MakeJunctionBuildSteps(const JunctionLayout &layout, TileIndex origin);
JunctionEstimate EstimateJunction(const JunctionLayout &layout, TileIndex origin, const JunctionPricing &pricing);

#endif /* JUNCTION_LAYOUT_H */
