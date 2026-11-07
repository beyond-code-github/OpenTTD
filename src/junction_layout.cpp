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
#include "table/strings.h"

#include <algorithm>

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
 * Mark a tile as bad, unless it is already marked.
 * @param estimate The estimate to add the tile to.
 * @param tile The tile to mark.
 */
static void MarkBad(JunctionEstimate &estimate, TileIndex tile)
{
	if (std::ranges::find(estimate.bad_tiles, tile) == estimate.bad_tiles.end()) estimate.bad_tiles.push_back(tile);
}

/**
 * Estimate the price and validity of placing a layout.
 * @param layout The layout to place.
 * @param origin The tile for the north corner of the layout.
 * @param pricing Source of tile and price information.
 * @return The estimate.
 */
JunctionEstimate EstimateJunction(const JunctionLayout &layout, TileIndex origin, const JunctionPricing &pricing)
{
	JunctionEstimate estimate;

	/* Collect the occupied tiles: track tiles, then each bridge's heads and span. */
	std::vector<TileIndex> occupied;
	for (const JunctionTrack &jt : layout.tracks) {
		occupied.push_back(AddTileIndexDiffCWrap(origin, jt.offset));
	}
	for (const JunctionBridge &bridge : layout.bridges) {
		occupied.push_back(AddTileIndexDiffCWrap(origin, bridge.start));
		occupied.push_back(AddTileIndexDiffCWrap(origin, bridge.end));
		if (bridge.start.x == bridge.end.x) {
			for (int16_t y = std::min(bridge.start.y, bridge.end.y) + 1; y < std::max(bridge.start.y, bridge.end.y); y++) {
				occupied.push_back(AddTileIndexDiffCWrap(origin, {bridge.start.x, y}));
			}
		} else {
			for (int16_t x = std::min(bridge.start.x, bridge.end.x) + 1; x < std::max(bridge.start.x, bridge.end.x); x++) {
				occupied.push_back(AddTileIndexDiffCWrap(origin, {x, bridge.start.y}));
			}
		}
	}

	/* Off-map tiles are bad; the others must be flat and at the height of the first flat one. */
	std::optional<int> reference_height;
	for (TileIndex tile : occupied) {
		if (tile == INVALID_TILE) continue;
		reference_height = pricing.GetFlatHeight(tile);
		if (reference_height.has_value()) break;
	}
	for (TileIndex tile : occupied) {
		if (tile == INVALID_TILE) {
			MarkBad(estimate, tile);
			if (estimate.first_error == INVALID_STRING_ID) estimate.first_error = STR_ERROR_TOO_CLOSE_TO_EDGE_OF_MAP;
			continue;
		}
		std::optional<int> height = pricing.GetFlatHeight(tile);
		if (!height.has_value() || height != reference_height) {
			MarkBad(estimate, tile);
			if (estimate.first_error == INVALID_STRING_ID) estimate.first_error = STR_ERROR_FLAT_LAND_REQUIRED;
		}
	}

	/* Price every build step that is entirely on the map. */
	for (const JunctionBuildStep &step : MakeJunctionBuildSteps(layout, origin)) {
		if (const JunctionRailStep *rail = std::get_if<JunctionRailStep>(&step); rail != nullptr) {
			if (rail->tile == INVALID_TILE) {
				MarkBad(estimate, rail->tile);
				if (estimate.first_error == INVALID_STRING_ID) estimate.first_error = STR_ERROR_TOO_CLOSE_TO_EDGE_OF_MAP;
				continue;
			}
			CommandCost result = pricing.TestRail(rail->tile, rail->track);
			if (result.Succeeded()) {
				estimate.total += result.GetCost();
			} else {
				MarkBad(estimate, rail->tile);
				if (estimate.first_error == INVALID_STRING_ID) estimate.first_error = result.GetErrorMessage();
			}
		} else if (const JunctionBridgeStep *bridge = std::get_if<JunctionBridgeStep>(&step); bridge != nullptr) {
			if (bridge->start == INVALID_TILE || bridge->end == INVALID_TILE) {
				MarkBad(estimate, INVALID_TILE);
				if (estimate.first_error == INVALID_STRING_ID) estimate.first_error = STR_ERROR_TOO_CLOSE_TO_EDGE_OF_MAP;
				continue;
			}
			CommandCost result = pricing.PriceBridge(bridge->start, bridge->end);
			if (result.Succeeded()) {
				estimate.total += result.GetCost();
			} else {
				MarkBad(estimate, bridge->start);
				MarkBad(estimate, bridge->end);
				if (estimate.first_error == INVALID_STRING_ID) estimate.first_error = result.GetErrorMessage();
			}
		} else if (const JunctionSignalStep *signal = std::get_if<JunctionSignalStep>(&step); signal != nullptr) {
			if (signal->tile == INVALID_TILE) {
				MarkBad(estimate, signal->tile);
				if (estimate.first_error == INVALID_STRING_ID) estimate.first_error = STR_ERROR_TOO_CLOSE_TO_EDGE_OF_MAP;
				continue;
			}
			estimate.total += pricing.SignalCost();
		}
	}

	estimate.affordable = estimate.bad_tiles.empty() && estimate.total <= pricing.AvailableMoney();
	return estimate;
}
