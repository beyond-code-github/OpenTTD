/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_layout.cpp Junction layout description, build steps and price estimate. */

#include "stdafx.h"

#include "junction_layout.h"
#include "map_func.h"

#include "safeguards.h"

/**
 * Transform a junction layout by a rotation or a mirror.
 * @param layout The layout to transform.
 * @param transform The transformation to apply.
 * @return The transformed layout.
 */
JunctionLayout TransformJunctionLayout(const JunctionLayout &layout, [[maybe_unused]] JunctionTransform transform)
{
	return layout;
}

/**
 * Make the fixed test layout.
 * Fixed layout used until the generator exists (1b); exercises track, a bridge over track, and signals in both directions.
 * @return The test layout.
 */
JunctionLayout MakeTestJunctionLayout()
{
	JunctionLayout layout;
	layout.size = {8, 6};
	for (int16_t x = 0; x < 8; x++) {
		layout.tracks.push_back({{x, 2}, TrackBits{Track::X}});
	}
	layout.tracks.push_back({{4, 5}, TrackBits{Track::Y}});
	layout.bridges.push_back({{4, 0}, {4, 4}});
	layout.signals.push_back({{1, 2}, Trackdir::X_SW, SignalType::Block, SignalVariant::Electric});
	layout.signals.push_back({{6, 2}, Trackdir::X_NE, SignalType::Block, SignalVariant::Electric});
	return layout;
}

/**
 * Make the build steps for placing a layout on the map.
 * @param layout The layout to place.
 * @param origin The tile for the north corner of the layout.
 * @return The steps in build order.
 */
std::vector<JunctionBuildStep> MakeJunctionBuildSteps(const JunctionLayout &layout, TileIndex origin)
{
	std::vector<JunctionBuildStep> steps;
	for (const JunctionTrack &jt : layout.tracks) {
		for (Track t : jt.tracks) {
			steps.push_back(JunctionRailStep{AddTileIndexDiffCWrap(origin, jt.offset), t});
		}
	}
	for (const JunctionBridge &bridge : layout.bridges) {
		steps.push_back(JunctionBridgeStep{AddTileIndexDiffCWrap(origin, bridge.start), AddTileIndexDiffCWrap(origin, bridge.end)});
	}
	for (const JunctionSignal &signal : layout.signals) {
		steps.push_back(JunctionSignalStep{AddTileIndexDiffCWrap(origin, signal.offset), signal.facing, signal.type, signal.variant});
	}
	return steps;
}

/**
 * Estimate the price and validity of placing a layout.
 * @param layout The layout to place.
 * @param origin The tile for the north corner of the layout.
 * @param pricing Source of tile and price information.
 * @return The estimate.
 */
JunctionEstimate EstimateJunction([[maybe_unused]] const JunctionLayout &layout, [[maybe_unused]] TileIndex origin, [[maybe_unused]] const JunctionPricing &pricing)
{
	return {};
}
