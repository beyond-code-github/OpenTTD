/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_layout.cpp Junction layout description, build steps and price estimate. */

#include "../stdafx.h"

#include "junction_layout.h"
#include "../map_func.h"
#include "../direction_func.h"
#include "../track_func.h"
#include "../rail.h"
#include "../tile_map.h"
#include "../slope_func.h"
#include "../core/format.hpp"
#include "../table/strings.h"

#include <algorithm>
#include <array>
#include <map>
#include <set>

#include "../safeguards.h"

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
		JunctionBridge new_bridge{TransformOffset(bridge.start, layout.size, transform), TransformOffset(bridge.end, layout.size, transform), bridge.tunnel, bridge.z};
		if (new_bridge.start.x + new_bridge.start.y > new_bridge.end.x + new_bridge.end.y) std::swap(new_bridge.start, new_bridge.end);
		result.bridges.push_back(new_bridge);
	}
	for (const JunctionStationBlock &block : layout.stations) {
		const int16_t w = block.axis == Axis::X ? block.length : block.platforms;
		JunctionStationBlock new_block = block;
		if (transform == JunctionTransform::Rotate) {
			new_block.origin = {block.origin.y, static_cast<int16_t>(layout.size.x - block.origin.x - w)};
			new_block.axis = block.axis == Axis::X ? Axis::Y : Axis::X;
		} else {
			new_block.origin = {static_cast<int16_t>(layout.size.x - block.origin.x - w), block.origin.y};
		}
		result.stations.push_back(new_block);
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
 * The layout as the JSON object the test rig builds from.
 * @param layout The layout.
 * @return The JSON text, ending in "}\n".
 */
std::string JunctionLayoutToJson(const JunctionLayout &layout)
{
	static const char *track_names[] = {"X", "Y", "Upper", "Lower", "Left", "Right"};
	static const char *td_names[] = {"X_NE", "Y_SE", "Upper_E", "Lower_E", "Left_S", "Right_S", "", "", "X_SW", "Y_NW", "Upper_W", "Lower_W", "Left_N", "Right_N"};
	static const char *type_names[] = {"block", "entry", "exit", "combo", "path", "path_oneway"};
	std::string out = fmt::format("{{\"size\":[{},{}],\n\"tracks\":[", layout.size.x, layout.size.y);
	bool first = true;
	for (const auto &jt : layout.tracks) {
		out += fmt::format("{}\n{{\"x\":{},\"y\":{},\"bits\":[", first ? "" : ",", jt.offset.x, jt.offset.y);
		bool fb = true;
		for (Track t : jt.tracks) {
			out += fmt::format("{}\"{}\"", fb ? "" : ",", track_names[to_underlying(t)]);
			fb = false;
		}
		out += "]}";
		first = false;
	}
	out += "],\n\"bridges\":[";
	first = true;
	for (const auto &b : layout.bridges) {
		out += fmt::format("{}{{\"start\":[{},{}],\"end\":[{},{}]{}{}}}", first ? "" : ",", b.start.x, b.start.y, b.end.x, b.end.y, b.tunnel ? ",\"tunnel\":true" : "", b.tunnel && b.z != 0 ? fmt::format(",\"z\":{}", b.z) : std::string());
		first = false;
	}
	out += "],\n\"signals\":[";
	first = true;
	for (const auto &sg : layout.signals) {
		out += fmt::format("{}\n{{\"x\":{},\"y\":{},\"facing\":\"{}\",\"type\":\"{}\"}}", first ? "" : ",", sg.offset.x, sg.offset.y, td_names[to_underlying(sg.facing)], type_names[to_underlying(sg.type)]);
		first = false;
	}
	out += "]";
	if (!layout.stations.empty()) {
		out += ",\n\"stations\":[";
		first = true;
		for (const auto &st : layout.stations) {
			out += fmt::format("{}{{\"x\":{},\"y\":{},\"axis\":\"{}\",\"platforms\":{},\"length\":{}}}", first ? "" : ",", st.origin.x, st.origin.y, st.axis == Axis::X ? "X" : "Y", st.platforms, st.length);
			first = false;
		}
		out += "]";
	}
	out += "}\n";
	return out;
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
 * Inverse of OrientJunctionVector.
 * @param v The oriented vector.
 * @param rotation Number of quarter turns; taken modulo 4.
 * @param mirrored Whether the orientation mirrors.
 * @return The vector in the canonical frame.
 */
TileIndexDiffC InverseOrientJunctionVector(TileIndexDiffC v, uint8_t rotation, bool mirrored)
{
	for (uint8_t i = 0; i < rotation % 4; i++) v = {static_cast<int16_t>(-v.y), v.x};
	if (mirrored) v = {static_cast<int16_t>(-v.x), v.y};
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
 * Check whether a layout offset is on a station tile.
 * @param layout The layout.
 * @param offset The offset to check.
 * @return True if a station block of the layout covers the offset.
 */
static bool InStationBlock(const JunctionLayout &layout, TileIndexDiffC offset)
{
	for (const JunctionStationBlock &block : layout.stations) {
		const int w = block.axis == Axis::X ? block.length : block.platforms;
		const int h = block.axis == Axis::X ? block.platforms : block.length;
		if (offset.x >= block.origin.x && offset.x < block.origin.x + w && offset.y >= block.origin.y && offset.y < block.origin.y + h) return true;
	}
	return false;
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
	for (const JunctionStationBlock &block : layout.stations) {
		steps.push_back(JunctionStationStep{AddTileIndexDiffCWrap(origin, block.origin), block.axis, block.platforms, block.length});
	}
	for (const JunctionTrack &jt : layout.tracks) {
		if (InStationBlock(layout, jt.offset)) continue; // Station tiles carry their own track.
		for (Track t : jt.tracks) {
			steps.push_back(JunctionRailStep{AddTileIndexDiffCWrap(origin, jt.offset), t});
		}
	}
	for (const JunctionBridge &bridge : layout.bridges) {
		steps.push_back(JunctionBridgeStep{AddTileIndexDiffCWrap(origin, bridge.start), AddTileIndexDiffCWrap(origin, bridge.end), bridge.tunnel});
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

/** The net level change of the moved map corners; the key (x, y) is the north corner of tile (x, y). */
using JunctionCornerMoves = std::map<std::pair<int, int>, int>;

/**
 * The slope a tile has once the terraform steps are applied: its corners' current heights moved by the steps (each step moves a tile's north corner).
 * @param tile The tile.
 * @param moved The net level change of each moved corner.
 * @return The planned slope.
 */
static Slope JunctionPlannedSlope(TileIndex tile, const JunctionCornerMoves &moved)
{
	static constexpr std::pair<int, int> CORNER_OFFSET[4] = {{1, 0}, {1, 1}, {0, 1}, {0, 0}}; // W, S, E, N.
	const int x = static_cast<int>(TileX(tile));
	const int y = static_cast<int>(TileY(tile));
	std::array<int, 4> corners;
	for (int i = 0; i < 4; i++) {
		const int cx = x + CORNER_OFFSET[i].first;
		const int cy = y + CORNER_OFFSET[i].second;
		const auto found = moved.find({cx, cy});
		corners[i] = static_cast<int>(TileHeightOutsideMap(cx, cy)) + (found == moved.end() ? 0 : found->second);
	}
	JunctionTerrainTile ground;
	SetJunctionTileCorners(ground, corners);
	return ground.slope;
}

/**
 * Estimate the price and validity of placing a layout.
 * @param layout The layout to place.
 * @param origin The tile for the north corner of the layout.
 * @param pricing Source of tile and price information.
 * @param single_plane Whether every occupied tile must lie in one plane (flat ground); false when a terraform plan makes the ground fit (H1).
 * @param steps The terraform plan's steps; the tiles they move are judged on the planned ground, not tested with the commands.
 * @return The estimate.
 * @note 6. Clearing under a bridge over track is counted once (D50).
 */
static JunctionEstimate EstimateJunctionImpl(const JunctionLayout &layout, TileIndex origin, const JunctionPricing &pricing, bool single_plane, const std::vector<JunctionTerraformStep> &steps)
{
	JunctionEstimate estimate;

	/* A tile is planned when the steps move any of its corners; each step moves the north corner of its tile, shared by four tiles. */
	JunctionCornerMoves moved;
	std::set<TileIndex> planned;
	for (const JunctionTerraformStep &step : steps) {
		const int x = static_cast<int>(TileX(step.tile));
		const int y = static_cast<int>(TileY(step.tile));
		moved[{x, y}] += step.raise ? 1 : -1;
		for (int dy = -1; dy <= 0; dy++) {
			for (int dx = -1; dx <= 0; dx++) {
				if (x + dx >= 0 && y + dy >= 0) planned.insert(TileXY(x + dx, y + dy));
			}
		}
	}
	/* Every track piece of each tile, for the foundation the planned ground needs. */
	std::map<TileIndex, TrackBits> tile_bits;
	for (const JunctionTrack &jt : layout.tracks) {
		const TileIndex tile = AddTileIndexDiffCWrap(origin, jt.offset);
		if (tile != INVALID_TILE) tile_bits[tile].Set(jt.tracks);
	}

	/* Collect the occupied tiles: track tiles and bridge heads, and separately each bridge's span. */
	std::vector<TileIndex> occupied;
	std::vector<TileIndex> span_tiles;
	for (const JunctionTrack &jt : layout.tracks) {
		occupied.push_back(AddTileIndexDiffCWrap(origin, jt.offset));
	}
	for (const JunctionBridge &bridge : layout.bridges) {
		occupied.push_back(AddTileIndexDiffCWrap(origin, bridge.start));
		occupied.push_back(AddTileIndexDiffCWrap(origin, bridge.end));
		if (bridge.tunnel) continue; // A tunnel has no span tiles to clear.
		if (bridge.start.x == bridge.end.x) {
			for (int16_t y = std::min(bridge.start.y, bridge.end.y) + 1; y < std::max(bridge.start.y, bridge.end.y); y++) {
				span_tiles.push_back(AddTileIndexDiffCWrap(origin, {bridge.start.x, y}));
			}
		} else {
			for (int16_t x = std::min(bridge.start.x, bridge.end.x) + 1; x < std::max(bridge.start.x, bridge.end.x); x++) {
				span_tiles.push_back(AddTileIndexDiffCWrap(origin, {x, bridge.start.y}));
			}
		}
	}

	/* Off-map tiles are bad; the others must lie in one plane H, the top of the first occupied tile: non-steep, top at H, and flat or built on a slope. Span tiles are validated by PriceBridge. */
	std::optional<int> plane;
	for (TileIndex tile : occupied) {
		if (!single_plane) break;
		if (tile == INVALID_TILE) continue;
		std::optional<JunctionTileSlope> slope = pricing.GetTileSlope(tile);
		if (slope.has_value()) {
			plane = slope->z_max;
			break;
		}
	}
	if (plane.has_value()) estimate.height = *plane;
	for (TileIndex tile : occupied) {
		if (tile == INVALID_TILE) {
			MarkBad(estimate, tile);
			if (estimate.first_error == INVALID_STRING_ID) estimate.first_error = STR_ERROR_TOO_CLOSE_TO_EDGE_OF_MAP;
			continue;
		}
		if (!single_plane) continue;
		std::optional<JunctionTileSlope> slope = pricing.GetTileSlope(tile);
		if (!slope.has_value() || slope->steep || slope->z_max != plane || (slope->z_min != slope->z_max && !slope->build_on_slopes)) {
			MarkBad(estimate, tile);
			if (estimate.first_error == INVALID_STRING_ID) estimate.first_error = STR_ERROR_FLAT_LAND_REQUIRED;
		}
	}
	for (TileIndex tile : span_tiles) {
		if (tile == INVALID_TILE) {
			MarkBad(estimate, tile);
			if (estimate.first_error == INVALID_STRING_ID) estimate.first_error = STR_ERROR_TOO_CLOSE_TO_EDGE_OF_MAP;
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
			if (planned.contains(rail->tile)) {
				/* The planner proved this piece on the planned ground (T3): no command test, which would judge the current ground. */
				const Foundation foundation = GetRailFoundation(JunctionPlannedSlope(rail->tile, moved), tile_bits[rail->tile]);
				if (foundation == Foundation::Invalid) {
					MarkBad(estimate, rail->tile);
					if (estimate.first_error == INVALID_STRING_ID) estimate.first_error = STR_ERROR_LAND_SLOPED_IN_WRONG_DIRECTION;
				} else {
					estimate.total += pricing.RailPrice(rail->track, foundation != Foundation::None);
				}
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
			if (!bridge->tunnel) {
				bool on_planned = planned.contains(bridge->start) || planned.contains(bridge->end);
				const TileIndexDiff delta = TileOffsByDiagDir(DiagdirBetweenTiles(bridge->start, bridge->end));
				for (TileIndex t = bridge->start + delta; t != bridge->end && !on_planned; t += delta) on_planned = planned.contains(t);
				if (on_planned) {
					/* The heads and span are judged on the planned ground: no command test, and the span's clearing is not priced, so nothing is taken back for it below. */
					estimate.total += pricing.BridgePrice(bridge->start, bridge->end);
					continue;
				}
			}
			CommandCost result = bridge->tunnel ? pricing.PriceTunnel(bridge->start, bridge->end) : pricing.PriceBridge(bridge->start, bridge->end);
			if (result.Succeeded()) {
				estimate.total += result.GetCost();
				if (!bridge->tunnel) {
					const TileIndexDiff delta = TileOffsByDiagDir(DiagdirBetweenTiles(bridge->start, bridge->end));
					for (TileIndex t = bridge->start + delta; t != bridge->end; t += delta) spans.push_back(t);
				}
			} else {
				MarkBad(estimate, bridge->start);
				MarkBad(estimate, bridge->end);
				if (estimate.first_error == INVALID_STRING_ID) estimate.first_error = result.GetErrorMessage();
			}
		} else if (const JunctionStationStep *station = std::get_if<JunctionStationStep>(&step); station != nullptr) {
			if (station->tile == INVALID_TILE) {
				MarkBad(estimate, station->tile);
				if (estimate.first_error == INVALID_STRING_ID) estimate.first_error = STR_ERROR_TOO_CLOSE_TO_EDGE_OF_MAP;
				continue;
			}
			CommandCost result = pricing.TestStation(station->tile, station->axis, station->platforms, station->length);
			if (result.Succeeded()) {
				estimate.total += result.GetCost();
			} else {
				/* The command reports one error for the block, so mark every tile of it. */
				const int w = station->axis == Axis::X ? station->length : station->platforms;
				const int h = station->axis == Axis::X ? station->platforms : station->length;
				for (int y = 0; y < h; y++) {
					for (int x = 0; x < w; x++) MarkBad(estimate, AddTileIndexDiffCWrap(station->tile, {static_cast<int16_t>(x), static_cast<int16_t>(y)}));
				}
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

JunctionEstimate EstimateJunction(const JunctionLayout &layout, TileIndex origin, const JunctionPricing &pricing)
{
	return EstimateJunctionImpl(layout, origin, pricing, true, {});
}

JunctionEstimate EstimateJunctionTerraformed(const JunctionLayout &layout, TileIndex origin, const JunctionPricing &pricing, Money height_cost, const std::vector<JunctionTerraformStep> &steps)
{
	JunctionEstimate estimate = EstimateJunctionImpl(layout, origin, pricing, false, steps);
	estimate.total += height_cost;
	estimate.affordable = estimate.bad_tiles.empty() && estimate.total <= pricing.AvailableMoney();
	return estimate;
}

/** A corner of the canonical lattice. */
using TerraformLattice = std::pair<int, int>;

std::vector<JunctionTerraformStep> MakeJunctionTerraformSteps(const JunctionTerraform &plan, const JunctionTerrain &terrain, TileIndex anchor, uint8_t rotation, bool mirrored, int frame_hub_x)
{
	/* Offset of each tile corner (W, S, E, N) from the tile's own coordinates. */
	static constexpr std::pair<int, int> CORNER_OFFSET[4] = {{1, 0}, {1, 1}, {0, 1}, {0, 0}};

	/* The current height of a canonical corner, from any snapshot tile that has it. */
	auto current = [&](const TerraformLattice &corner) -> int {
		for (int i = 0; i < 4; i++) {
			const JunctionTerrainTile *tile = terrain.At(corner.first - CORNER_OFFSET[i].first, corner.second - CORNER_OFFSET[i].second);
			if (tile != nullptr) return JunctionTileCorners(*tile)[i];
		}
		return 0;
	};

	/* Cells map to tiles by OrientJunctionVector, which is linear, about the tile centres; so a corner's image is the cell's image tile centre plus the oriented half-diagonal. In half units the north corner of canonical cell c is at 2 * image(c) + (1, 1) + O((-1, -1)); the result is even per axis and is the north corner of map tile (X, Y). */
	struct Move { TerraformLattice canonical; TileIndex tile; int from; int to; };
	std::vector<Move> moves;
	const TileIndexDiffC half = OrientJunctionVector({-1, -1}, rotation, mirrored);
	for (const auto &[corner, target] : plan.corners) {
		const int from = current(corner);
		if (from == target) continue;
		const TileIndexDiffC image = OrientJunctionVector({static_cast<int16_t>(corner.first - frame_hub_x), static_cast<int16_t>(corner.second)}, rotation, mirrored);
		const int x = static_cast<int>(TileX(anchor)) + image.x + (1 + half.x) / 2;
		const int y = static_cast<int>(TileY(anchor)) + image.y + (1 + half.y) / 2;
		moves.push_back({corner, TileXY(x, y), from, target});
	}

	/* Order in the canonical frame (the rotation and mirror keep adjacency): raises lowest corner first, then lowers highest corner first. Each such corner is legal when it moves (RECON-hover-switch; no neighbour is more than one level beyond it). */
	std::vector<JunctionTerraformStep> steps;
	std::map<TerraformLattice, int> height;
	for (const Move &move : moves) height[move.canonical] = move.from;
	for (const bool raise : {true, false}) {
		for (;;) {
			const Move *best = nullptr;
			for (const Move &move : moves) {
				const int h = height[move.canonical];
				if (raise ? h >= move.to : h <= move.to) continue;
				if (best == nullptr) { best = &move; continue; }
				const int bh = height[best->canonical];
				if (raise ? h < bh : h > bh) best = &move;
			}
			if (best == nullptr) break;
			height[best->canonical] += raise ? 1 : -1;
			steps.push_back({best->tile, Corner::N, raise});
		}
	}
	return steps;
}
