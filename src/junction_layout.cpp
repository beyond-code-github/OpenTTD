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
#include "direction_func.h"
#include "track_func.h"
#include "table/strings.h"

#include <algorithm>
#include <array>

#include "safeguards.h"

/**
 * Transform a diagonal direction by a rotation or a mirror.
 * @param dir The direction to transform.
 * @param transform The transformation to apply.
 * @return The transformed direction.
 */
static DiagDirection TransformDiagDirection(DiagDirection dir, JunctionTransform transform)
{
	switch (transform) {
		case JunctionTransform::Rotate: return ChangeDiagDir(dir, DiagDirDiff::Right90);
		case JunctionTransform::Mirror:
			if (dir == DiagDirection::NE) return DiagDirection::SW;
			if (dir == DiagDirection::SW) return DiagDirection::NE;
			return dir;
		default: NOT_REACHED();
	}
}

/**
 * Transform a track by a rotation or a mirror.
 * @param track The track to transform.
 * @param transform The transformation to apply.
 * @return The transformed track.
 */
Track TransformTrack(Track track, JunctionTransform transform)
{
	static const std::array<std::array<DiagDirection, 2>, 6> edges = {{
		{DiagDirection::NE, DiagDirection::SW}, // Track::X
		{DiagDirection::NW, DiagDirection::SE}, // Track::Y
		{DiagDirection::NE, DiagDirection::NW}, // Track::Upper
		{DiagDirection::SE, DiagDirection::SW}, // Track::Lower
		{DiagDirection::SW, DiagDirection::NW}, // Track::Left
		{DiagDirection::NE, DiagDirection::SE}, // Track::Right
	}};

	const auto &old_edges = edges[to_underlying(track)];
	DiagDirection new_first = TransformDiagDirection(old_edges[0], transform);
	DiagDirection new_second = TransformDiagDirection(old_edges[1], transform);
	for (Track t : {Track::X, Track::Y, Track::Upper, Track::Lower, Track::Left, Track::Right}) {
		const auto &candidate = edges[to_underlying(t)];
		if ((candidate[0] == new_first && candidate[1] == new_second) || (candidate[0] == new_second && candidate[1] == new_first)) return t;
	}
	NOT_REACHED();
}

/**
 * Transform a trackdir by a rotation or a mirror.
 * @param trackdir The trackdir to transform.
 * @param transform The transformation to apply.
 * @return The transformed trackdir.
 */
Trackdir TransformTrackdir(Trackdir trackdir, JunctionTransform transform)
{
	return TrackExitdirToTrackdir(TransformTrack(TrackdirToTrack(trackdir), transform), TransformDiagDirection(TrackdirToExitdir(trackdir), transform));
}

/**
 * Transform a tile offset within a layout of the given size.
 * @param offset The offset to transform.
 * @param size The size of the layout before the transformation.
 * @param transform The transformation to apply.
 * @return The transformed offset.
 */
static TileIndexDiffC TransformOffset(TileIndexDiffC offset, TileIndexDiffC size, JunctionTransform transform)
{
	switch (transform) {
		case JunctionTransform::Rotate: return {offset.y, static_cast<int16_t>(size.x - 1 - offset.x)};
		case JunctionTransform::Mirror: return {static_cast<int16_t>(size.x - 1 - offset.x), offset.y};
		default: NOT_REACHED();
	}
}

/**
 * Transform a junction layout by a rotation or a mirror.
 * @param layout The layout to transform.
 * @param transform The transformation to apply.
 * @return The transformed layout.
 */
JunctionLayout TransformJunctionLayout(const JunctionLayout &layout, JunctionTransform transform)
{
	JunctionLayout result;
	result.size = transform == JunctionTransform::Rotate ? TileIndexDiffC{layout.size.y, layout.size.x} : layout.size;
	for (const JunctionTrack &jt : layout.tracks) {
		TrackBits bits{};
		for (Track t : jt.tracks) bits.Set(TransformTrack(t, transform));
		result.tracks.push_back({TransformOffset(jt.offset, layout.size, transform), bits});
	}
	for (const JunctionSignal &signal : layout.signals) {
		result.signals.push_back({TransformOffset(signal.offset, layout.size, transform), TransformTrackdir(signal.facing, transform), signal.type, signal.variant});
	}
	for (const JunctionBridge &bridge : layout.bridges) {
		JunctionBridge new_bridge{TransformOffset(bridge.start, layout.size, transform), TransformOffset(bridge.end, layout.size, transform)};
		if (new_bridge.start.x + new_bridge.start.y > new_bridge.end.x + new_bridge.end.y) std::swap(new_bridge.start, new_bridge.end);
		result.bridges.push_back(new_bridge);
	}
	return result;
}

/**
 * Apply an orientation to a junction layout: mirror (if set), then rotate 'rotation % 4' quarter turns.
 * @param layout The layout to orient.
 * @param rotation Number of quarter turns; taken modulo 4.
 * @param mirrored Whether to mirror before rotating.
 * @return The oriented layout.
 */
JunctionLayout ApplyJunctionOrientation(const JunctionLayout &layout, uint8_t rotation, bool mirrored)
{
	JunctionLayout result = mirrored ? TransformJunctionLayout(layout, JunctionTransform::Mirror) : layout;
	for (uint8_t i = 0; i < rotation % 4; i++) result = TransformJunctionLayout(result, JunctionTransform::Rotate);
	return result;
}

/**
 * Where a layout offset lands after ApplyJunctionOrientation.
 * @param offset The offset in the layout.
 * @param size The size of the layout.
 * @param rotation Number of quarter turns; taken modulo 4.
 * @param mirrored Whether to mirror before rotating.
 * @return The oriented offset.
 */
TileIndexDiffC OrientJunctionOffset(TileIndexDiffC offset, TileIndexDiffC size, uint8_t rotation, bool mirrored)
{
	if (mirrored) offset = TransformOffset(offset, size, JunctionTransform::Mirror);
	for (uint8_t i = 0; i < rotation % 4; i++) {
		offset = TransformOffset(offset, size, JunctionTransform::Rotate);
		size = {size.y, size.x};
	}
	return offset;
}

/**
 * The map offset of a vector in the canonical frame, under an orientation.
 * @param v The vector.
 * @param rotation Number of quarter turns; taken modulo 4.
 * @param mirrored Whether to mirror before rotating.
 * @return The oriented vector.
 */
TileIndexDiffC OrientJunctionVector(TileIndexDiffC v, uint8_t rotation, bool mirrored)
{
	if (mirrored) v = {static_cast<int16_t>(-v.x), v.y};
	for (uint8_t i = 0; i < rotation % 4; i++) v = {v.y, static_cast<int16_t>(-v.x)};
	return v;
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
 * @note 6. Clearing under a bridge over track is counted once (D50).
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
	if (reference_height.has_value()) estimate.height = *reference_height;
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
	std::vector<TileIndex> spans; ///< Span tiles of the bridges that priced successfully.
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
				const TileIndexDiff delta = TileOffsByDiagDir(DiagdirBetweenTiles(bridge->start, bridge->end));
				for (TileIndex t = bridge->start + delta; t != bridge->end; t += delta) spans.push_back(t);
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

	/* The bridge clears its span tiles that have no build-above handler, so a span tile that also has track pays its clearing twice; the real build places the rail first and pays once. */
	std::vector<TileIndex> counted;
	for (const JunctionTrack &jt : layout.tracks) {
		const TileIndex tile = AddTileIndexDiffCWrap(origin, jt.offset);
		if (tile == INVALID_TILE || std::ranges::find(estimate.bad_tiles, tile) != estimate.bad_tiles.end()) continue;
		if (std::ranges::find(spans, tile) == spans.end() || std::ranges::find(counted, tile) != counted.end()) continue;
		counted.push_back(tile);
		CommandCost c = pricing.TestClear(tile);
		if (c.Succeeded()) estimate.total -= c.GetCost();
	}

	estimate.affordable = estimate.bad_tiles.empty() && estimate.total <= pricing.AvailableMoney();
	return estimate;
}
