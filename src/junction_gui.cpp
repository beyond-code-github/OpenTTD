/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */


/** @file junction_gui.cpp Client-side store and drawing of the junction placement ghost. */

#include "stdafx.h"

#include "junction_gui.h"
#include "junction_ghost_draw.h"
#include "junction_game.h"
#include "junction_generator.h"
#include "junction_hover.h"
#include "junction_line.h"
#include "junction_terraform.h"
#include "junction_topology.h"
#include "junction_rules.h"
#include "querystring_gui.h"
#include "window_gui.h"
#include "gui.h"
#include "error.h"
#include "gfx_func.h"
#include "window_func.h"
#include "picker_gui.h"
#include "hotkeys.h"
#include "sound_func.h"
#include "sound_type.h"
#include "settings_type.h"
#include "strings_func.h"
#include "tilehighlight_func.h"
#include "tilehighlight_type.h"
#include "bridge.h"
#include "vehicle_base.h"
#include "vehicle_gui.h"
#include "company_func.h"
#include "train.h"
#include "engine_base.h"
#include "engine_gui.h"
#include "vehicle_func.h"
#include "zoom_func.h"
#include "settings_type.h"
#include "debug.h"
#include "widgets/junction_widget.h"
#include "map_func.h"
#include "slope_func.h"
#include "slider_func.h"
#include "tile_cmd.h"
#include "viewport_func.h"

#include "table/sprites.h"
#include "table/strings.h"
#include "thread.h"

#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <map>
#include <memory>
#include <thread>
#include <unordered_map>

#include "safeguards.h"

/** The ghost tiles, by tile. */
static std::unordered_map<TileIndex, JunctionGhostTile> _junction_ghost_tiles;
/** Rail type of the ghost. */
static RailType _junction_ghost_railtype = RAILTYPE_RAIL;

/** Bounding box of the stored layout, as set by the last Set(). */
static TileArea _junction_ghost_footprint;
/** Whether the footprint is pinned: drawn even where the ghost has no tile. */
static bool _junction_ghost_pinned = false;

/** Whether the shown junction is valid but too expensive: its whole footprint is outlined red. */
static bool _junction_ghost_unaffordable = false;

/** Mark every stored tile dirty, so it is redrawn. */
static void MarkJunctionGhostDirty()
{
	/* Every footprint tile is outlined, not just the ones with track. */
	if (!_junction_ghost_tiles.empty()) {
		for (TileIndex tile : _junction_ghost_footprint) MarkTileDirtyByTile(tile);
	}
	for (const auto &[tile, ghost] : _junction_ghost_tiles) {
		MarkTileDirtyByTile(tile);
		/* The bridge deck is one level above the ground. */
		if (ghost.span.has_value()) MarkTileDirtyByTile(tile, 1);
	}
}

/**
 * Store a junction ghost, replacing any earlier one.
 * @param layout Layout, already transformed.
 * @param origin North corner of the junction.
 * @param height Ground height of the junction in height levels.
 * @param railtype Rail type to draw.
 * @param bridge_types Chosen bridge type per layout bridge, in order; std::nullopt where none qualifies.
 * @param estimate Estimate of the junction; its bad tiles are drawn red.
 * @param terraform_tiles Tiles whose ground the projection plan changes; outlined blue.
 * @param steps The terraform steps of the projection plan (map coordinates); with levels, the ground of each level tile is the current corner heights moved by them.
 * @param levels The level of every track tile as an offset from origin (the ghost's frame); a tile in it is drawn at its terraformed ground, a bridge deck at its head level + 1.
 */
void JunctionGhost::Set(const JunctionLayout &layout, TileIndex origin, int height, RailType railtype, const std::vector<std::optional<BridgeType>> &bridge_types, const JunctionEstimate &estimate, std::span<const TileIndex> terraform_tiles, std::span<const JunctionTerraformStep> steps, std::span<const std::pair<TileIndexDiffC, int8_t>> levels)
{
	JunctionGhost::Clear();
	_junction_ghost_railtype = railtype;
	_junction_ghost_footprint = TileArea(origin, layout.size.x, layout.size.y);
	_junction_ghost_unaffordable = !estimate.affordable && estimate.bad_tiles.empty();

	for (const JunctionTrack &track : layout.tracks) {
		TileIndex tile = AddTileIndexDiffCWrap(origin, track.offset);
		if (tile == INVALID_TILE) continue;
		_junction_ghost_tiles[tile].tracks.Set(track.tracks);
	}

	/* Station tiles are drawn as the platform track of the block's axis: a distinct station sprite isn't needed in v1. */
	for (const JunctionStationBlock &block : layout.stations) {
		const int w = block.axis == Axis::X ? block.length : block.platforms;
		const int h = block.axis == Axis::X ? block.platforms : block.length;
		for (int y = 0; y < h; y++) {
			for (int x = 0; x < w; x++) {
				TileIndex tile = AddTileIndexDiffCWrap(origin, {static_cast<int16_t>(block.origin.x + x), static_cast<int16_t>(block.origin.y + y)});
				if (tile == INVALID_TILE) continue;
				_junction_ghost_tiles[tile].tracks.Set(block.axis == Axis::X ? Track::X : Track::Y);
			}
		}
	}

	for (const JunctionSignal &signal : layout.signals) {
		TileIndex tile = AddTileIndexDiffCWrap(origin, signal.offset);
		if (tile == INVALID_TILE) continue;
		_junction_ghost_tiles[tile].signals.push_back({tile, signal.facing, signal.type, signal.variant});
	}

	/* The terraformed ground of every level tile: its corners' current heights moved by the steps (each moves a tile's north corner). */
	std::map<std::pair<int, int>, int> moved;
	for (const JunctionTerraformStep &step : steps) moved[{static_cast<int>(TileX(step.tile)), static_cast<int>(TileY(step.tile))}] += step.raise ? 1 : -1;
	/* Store the terraformed ground of a tile as the ghost's slope and height. */
	auto set_ground = [&](TileIndex tile) {
		const int x = static_cast<int>(TileX(tile));
		const int y = static_cast<int>(TileY(tile));
		std::array<int, 4> corners; // W, S, E, N.
		static constexpr std::pair<int, int> CORNER_OFFSET[4] = {{1, 0}, {1, 1}, {0, 1}, {0, 0}};
		for (int i = 0; i < 4; i++) {
			const int cx = x + CORNER_OFFSET[i].first;
			const int cy = y + CORNER_OFFSET[i].second;
			const auto found = moved.find({cx, cy});
			corners[i] = static_cast<int>(TileHeightOutsideMap(cx, cy)) + (found == moved.end() ? 0 : found->second);
		}
		JunctionTerrainTile ground;
		SetJunctionTileCorners(ground, corners);
		JunctionGhostTile &ghost = _junction_ghost_tiles[tile];
		ghost.leveled = true;
		ghost.slope = ground.slope;
		ghost.z = ground.z_min * static_cast<int>(TILE_HEIGHT);
	};

	std::unordered_map<TileIndex, int> level_of;
	for (const auto &[offset, level] : levels) {
		TileIndex tile = AddTileIndexDiffCWrap(origin, offset);
		if (tile == INVALID_TILE) continue;
		level_of[tile] = level;
		set_ground(tile);
	}

	/* A tile the plan terraforms that carries no track shows its planned ground. */
	for (TileIndex tile : terraform_tiles) {
		if (tile == INVALID_TILE || level_of.contains(tile)) continue;
		const auto existing = _junction_ghost_tiles.find(tile);
		if (existing != _junction_ghost_tiles.end() && (existing->second.tracks.Any() || existing->second.leveled)) continue;
		set_ground(tile);
	}

	for (size_t i = 0; i < layout.bridges.size(); i++) {
		TileIndex start = AddTileIndexDiffCWrap(origin, layout.bridges[i].start);
		TileIndex end = AddTileIndexDiffCWrap(origin, layout.bridges[i].end);
		if (start == INVALID_TILE || end == INVALID_TILE) continue;

		std::optional<BridgeType> type = i < bridge_types.size() ? bridge_types[i] : std::nullopt;
		if (!type.has_value()) {
			_junction_ghost_tiles[start].bad = true;
			_junction_ghost_tiles[end].bad = true;
			continue;
		}

		Axis axis = TileX(start) == TileX(end) ? Axis::Y : Axis::X;
		_junction_ghost_tiles[start].head = JunctionGhostBridgeHead{*type, axis == Axis::Y ? DiagDirection::SE : DiagDirection::SW};
		_junction_ghost_tiles[end].head = JunctionGhostBridgeHead{*type, axis == Axis::Y ? DiagDirection::NW : DiagDirection::NE};

		/* Tiles strictly between the heads. */
		uint len = (axis == Axis::Y ? Delta(TileY(start), TileY(end)) : Delta(TileX(start), TileX(end))) - 1;
		/* The deck is one level above the head's level, else above the junction's ground height. */
		const auto head_level = level_of.find(start);
		const int deck_level = (head_level != level_of.end() ? head_level->second : height) + 1;
		for (uint d = 1; d <= len; d++) {
			TileIndex tile = axis == Axis::Y ? TileXY(TileX(start), TileY(start) + d) : TileXY(TileX(start) + d, TileY(start));
			_junction_ghost_tiles[tile].span = JunctionGhostBridgeSpan{*type, axis, static_cast<int>(deck_level * TILE_HEIGHT), d, len + 1 - d};
		}
	}

	for (TileIndex tile : estimate.bad_tiles) {
		if (tile == INVALID_TILE) continue;
		_junction_ghost_tiles[tile].bad = true;
	}

	for (TileIndex tile : terraform_tiles) {
		if (tile == INVALID_TILE) continue;
		_junction_ghost_tiles[tile].terraform = true;
	}

	MarkJunctionGhostDirty();
}

/** Empty the ghost store, and mark the old tiles dirty. */
void JunctionGhost::Clear()
{
	MarkJunctionGhostDirty();
	_junction_ghost_tiles.clear();
	JunctionGhost::SetPinned(false);
}

/**
 * Pin or unpin the footprint of the ghost. While pinned, the footprint is outlined.
 * @param pinned Whether to pin the footprint.
 */
void JunctionGhost::SetPinned(bool pinned)
{
	_junction_ghost_pinned = pinned;
	if (_junction_ghost_footprint.tile == INVALID_TILE) return;
	for (TileIndex tile : _junction_ghost_footprint) MarkTileDirtyByTile(tile);
}

/**
 * Check whether a tile is inside the pinned footprint.
 * @param tile Tile to check.
 * @return True if the footprint is pinned and contains the tile.
 */
bool JunctionGhost::IsInPinnedFootprint(TileIndex tile)
{
	return _junction_ghost_pinned && _junction_ghost_footprint.Contains(tile);
}

/**
 * Get the ghost on a tile.
 * @param tile Tile to look up.
 * @return The ghost, or nullptr if the tile has none.
 */
const JunctionGhostTile *JunctionGhost::Get(TileIndex tile)
{
	auto it = _junction_ghost_tiles.find(tile);
	return it == _junction_ghost_tiles.end() ? nullptr : &it->second;
}

/**
 * Get the rail type of the ghost.
 * @return The rail type given to Set().
 */
RailType JunctionGhost::GetRailType()
{
	return _junction_ghost_railtype;
}

/**
 * Draw the junction ghost's sprites on one tile, if it has any.
 * Called before the tile selection, so the selection outline is drawn over the ghost's ground sprites.
 * The ghost is always drawn opaque in normal colours (D44, D53). A tile with a solved level is drawn on its terraformed ground (slope and height), with the game's own foundation choice; any other tile only when the real ground is flat.
 * @param ti Tile that is being drawn.
 */
void DrawJunctionGhost(TileInfo *ti)
{
	const JunctionGhostTile *ghost = JunctionGhost::Get(ti->tile);
	if (ghost == nullptr || (!ghost->leveled && ti->tileh != SLOPE_FLAT)) return;

	RailType railtype = JunctionGhost::GetRailType();
	if (ghost->leveled) {
		DrawOnCurrentTileAs(ghost->slope, ghost->z, [&](TileInfo *at) {
			if (ghost->tracks.Any()) DrawRailTrackGhost(at, ghost->tracks, railtype);
			if (ghost->head.has_value()) DrawBridgeHeadGhost(at, ghost->head->type, railtype, ghost->head->dir);
			/* A tile with no track is planned ground only: its ground sprite on the planned slope. */
			if (ghost->tracks.None() && !ghost->head.has_value()) DrawRailTrackGhost(at, {}, railtype);
			for (const JunctionSignalStep &signal : ghost->signals) DrawRailSignalGhost(at, railtype, signal.facing, signal.type, signal.variant);
		});
	} else {
		TileInfo copy = *ti;
		if (ghost->tracks.Any()) DrawRailTrackGhost(&copy, ghost->tracks, railtype);
		if (ghost->head.has_value()) DrawBridgeHeadGhost(&copy, ghost->head->type, railtype, ghost->head->dir);
		for (const JunctionSignalStep &signal : ghost->signals) DrawRailSignalGhost(&copy, railtype, signal.facing, signal.type, signal.variant);
	}
	if (ghost->span.has_value()) DrawBridgeMiddleGhost(ti, ghost->span->type, railtype, ghost->span->axis, ghost->span->bridge_z, ghost->span->piece_north, ghost->span->piece_south);
}

/**
 * Draw the junction ghost's outlines on one tile, after the tile selection so they show over it.
 * Bad tiles get a red outline; while a build is pinned, bad tiles are the pieces still
 * to build and get a blue one (D53, D54), and the other built tiles get a white one.
 * Tiles the projection plan terraforms get a blue outline while the ghost is not pinned.
 * @param ti Tile that is being drawn.
 */
void DrawJunctionGhostOutline(const TileInfo *ti)
{
	const JunctionGhostTile *ghost = JunctionGhost::Get(ti->tile);
	/* Only the tiles the junction builds on or terraforms are outlined, while hovering and while building; the empty rest of the footprint stays clear (engineer: the full grid hides the ground when zoomed out). */
	if (ghost == nullptr) return;

	/* Bad tiles are red, or blue for the pieces still to build while a build is pinned (D53); every other ghost tile is white.
	 * Slopes are not bad by themselves: leveled foundations make them usable (D78). */
	if (ghost != nullptr && ghost->bad) {
		DrawTileSelectionRect(ti, _junction_ghost_pinned ? PALETTE_SEL_TILE_BLUE : PALETTE_SEL_TILE_RED);
	} else if (ghost != nullptr && ghost->terraform && !_junction_ghost_pinned) {
		DrawTileSelectionRect(ti, _junction_ghost_unaffordable ? PALETTE_SEL_TILE_RED : PALETTE_SEL_TILE_BLUE);
	} else {
		DrawTileSelectionRect(ti, _junction_ghost_unaffordable && !_junction_ghost_pinned ? PALETTE_SEL_TILE_RED : PAL_NONE);
	}
}

/**
 * Reason shown when a junction can't be placed. Errors come per tile, so instead of the first tile's
 * error a generic message points at the red tiles; "too close to the edge" applies to the whole junction, and the game's own error (the first the estimate met) is named when there is one.
 * @param estimate Estimate with at least one bad tile.
 * @return Message for the tooltip, the cost line and the Shift-click estimate.
 */
static StringID GetPlacementError(const JunctionEstimate &estimate)
{
	if (std::ranges::find(estimate.bad_tiles, INVALID_TILE) != estimate.bad_tiles.end()) return STR_ERROR_TOO_CLOSE_TO_EDGE_OF_MAP;
	if (estimate.first_error != INVALID_STRING_ID && estimate.first_error != STR_ERROR_TOO_CLOSE_TO_EDGE_OF_MAP) return estimate.first_error;
	return STR_JUNCTION_ERROR_TILES_BLOCKED;
}

static constexpr uint MAX_NOTATION_LENGTH = 16; ///< Maximum number of characters of a line notation.

/** Hotkeys of the junction window. */
enum JunctionHotkeys : int {
	JHK_ROTATE, ///< Rotate the junction.
	JHK_MIRROR, ///< Mirror the junction.
};

static bool _junction_picking_train = false; ///< The junction window has the vehicle cursor, to pick a train.

/** @return Whether the junction window is picking a train, so the rail toolbar must keep the junction tool when it loses the cursor. */
bool IsJunctionTrainPickActive()
{
	return _junction_picking_train;
}

static uint8_t _junction_rotation = 0; ///< Quarter turns applied to the junction being placed.
static bool _junction_mirrored = false; ///< Whether the junction being placed is mirrored.
static constexpr bool _junction_projection = true; ///< The hover plans on the projection pipeline with terraform (H2); the earlier generator is no longer reachable from the window.
static std::optional<VehicleID> _junction_picked_train; ///< The last picked design train, remembered between openings of the window (game session only).

/** What a generated junction depends on: the spot, the orientation and the generator inputs. */
struct JunctionGenerateKey {
	TileIndex anchor; ///< The hovered tile, where the hub goes.
	uint8_t rotation; ///< Quarter turns.
	bool mirrored; ///< Effective mirror, including the road side.
	uint8_t ml_gap;
	uint8_t branch_gap;
	uint8_t ml_left; ///< Left tracks of the mainline notation: with the three below, the lane counts the hub topology is built from.
	uint8_t ml_right;
	uint8_t branch_left;
	uint8_t branch_right;
	uint8_t tl;
	uint8_t signal_type;
	uint8_t sd;
	JunctionTrainParams train; ///< The picked train, loaded (D137); default when none is picked.
	uint8_t prio_tiles; ///< Effective prio coverage of the slider (D139); 0 in the library key and before sizing.
	uint8_t lane_pairing; ///< JunctionLanePairing of the hub topology (D221).

	bool operator==(const JunctionGenerateKey &) const = default;
};

/** The hover plan of the projection pipeline for one spot; the layout is canonical, the steps are in map coordinates for the key's orientation. */
struct JunctionProjectionResult {
	std::optional<JunctionLayout> layout; ///< The signalled layout, nullopt when no plan was found.
	std::vector<JunctionTerraformStep> steps; ///< The terraform steps, legal in order.
	Money height_cost = 0; ///< The plan's height cost.
	std::vector<std::pair<TileIndexDiffC, int8_t>> track_levels; ///< The level of every track tile of layout (canonical frame).
	std::string why; ///< The reason when there is no layout.
	TileIndexDiffC hub{}; ///< The layout position of the hovered tile.
	int distance = 0; ///< How far the plan was moved from the hovered spot (D197), 0 at the spot.
};

/** A generation running on a worker thread; owns everything the worker reads, and outlives the window if needed. */
struct JunctionGenerateJob {
	std::atomic<bool> done{false}; ///< Set by the worker when result is final.
	JunctionSpec spec; ///< Spec the worker reads.
	JunctionTerrain terrain; ///< Terrain snapshot the worker reads.
	JunctionNearResult result; ///< Written by the worker.
	std::atomic<bool> cancel{false}; ///< Set when the job is detached; the worker stops searching.
	std::shared_ptr<const std::vector<JunctionLibraryEntry>> library; ///< Library for the spec, or null if not built yet.
	bool projection = false; ///< The job runs the projection pipeline: seed in, projection_result out.
	std::shared_ptr<const JunctionGraphSeed> seed; ///< The T graph seed of the spec, for the projection pipeline.
	JunctionProjectionResult projection_result; ///< Written by the worker of a projection job.
};

/** A library build running on a worker thread, or finished; owns everything the worker reads, and outlives the window if needed. */
struct JunctionLibraryJob {
	std::atomic<bool> sizing_done{false}; ///< Set by the worker when spec.prio_tiles is final: the sized prio, or 0 when the sizing failed.
	std::atomic<bool> done{false}; ///< Set by the worker when library is final.
	JunctionGenerateError sizing_error = JunctionGenerateError::None; ///< Why the sizing failed; written by the worker before sizing_done is set.
	JunctionSpec spec; ///< Spec the worker reads; the worker sets prio_tiles first (D137).
	std::shared_ptr<const std::vector<JunctionLibraryEntry>> library; ///< Written by the worker.
};

static constexpr size_t MAX_JUNCTION_CACHE = 32; ///< Entries in the window's cache of generated junctions.
static constexpr std::chrono::milliseconds JUNCTION_HOVER_REST{300}; ///< How long the hover must rest before generating.

/** Window for choosing the orientation of the junction and showing its ghost. */
struct BuildJunctionWindow : public PickerWindowBase {
	TileIndex hover_tile = INVALID_TILE; ///< Tile the cursor rests on.
	std::chrono::steady_clock::time_point hover_since; ///< When the cursor reached hover_tile.
	std::vector<std::pair<JunctionGenerateKey, JunctionNearResult>> cache; ///< Generated junctions, most recently used first.
	std::vector<std::pair<JunctionGenerateKey, JunctionProjectionResult>> projection_cache; ///< Projection plans, most recently used first.
	std::shared_ptr<const JunctionGraphSeed> seed; ///< The hub graph seed of seed_key, aliasing the topology it was made from, made on the main thread; shared read-only with the jobs.
	std::optional<JunctionGenerateKey> seed_key; ///< The spec key of seed: the key with no anchor, rotation 0 and not mirrored.
	std::string seed_error; ///< Why the seed could not be made; empty when it was.
	std::string hover_why; ///< The plan's reason when hover_status is STR_JUNCTION_STATUS_PLAN_FAILED.
	std::vector<JunctionTerraformStep> shown_terraform; ///< Terraform steps of the shown placement; empty on the earlier path.
	std::optional<Money> shown_height_cost; ///< Height cost of the shown placement; nullopt on the earlier path.
	std::optional<JunctionGenerateKey> pending_key; ///< Key of the running job, if any.
	std::thread pending_thread; ///< Thread of the running job.
	std::shared_ptr<JunctionGenerateJob> pending_job; ///< State of the running job.
	std::shared_ptr<JunctionLibraryJob> library_job; ///< Library for the current spec, or being built.
	std::thread library_thread; ///< Thread of the library build.
	std::optional<JunctionGenerateKey> library_key; ///< MakeKey of the library, with anchor INVALID_TILE and rotation 0, mirrored false.
	bool has_shown = false; ///< The ghost shows a placement, in the three fields below.
	JunctionLayout shown_layout; ///< Oriented layout shown at the cursor.
	uint8_t shown_prio_tiles = 0; ///< Prio coverage of the shown placement, in tiles.
	TileIndex shown_origin = INVALID_TILE; ///< North corner of the shown placement.
	TileIndex shown_tile = INVALID_TILE; ///< Hovered tile the shown placement belongs to.
	int shown_distance = 0; ///< Chebyshev distance in tiles from the hovered tile to the shown placement's anchor.
	StringID hover_status = INVALID_STRING_ID; ///< Status of the hovered spot while there is no placement.
	uint8_t ml_gap = 0; ///< Gap of the mainline notation.
	uint8_t branch_gap = 0; ///< Gap of the branch notation.
	JunctionLineSpec ml_line; ///< The mainline notation's lines.
	JunctionLineSpec branch_line; ///< The branch notation's lines.
	RailType railtype; ///< Rail type of the toolbar that opened this window.
	JunctionLayout build_layout; ///< Layout being built, as it was when the build started.
	TileIndex build_origin = INVALID_TILE; ///< North corner of the junction being built.
	int build_height = 0; ///< Ground height of the junction being built, in height levels.
	std::vector<std::optional<BridgeType>> build_bridge_types; ///< Bridge types chosen for the junction being built, per layout bridge.
	JunctionBuildState last_state = JunctionBuildState::Idle; ///< Sequencer state at the previous tick, to detect a failure.
	Money placement_cost = 0; ///< Cost of the junction at the cursor.
	Money build_quote = 0; ///< The quote (estimate total) of the junction being built.
	bool placement_affordable = false; ///< Whether the local company can pay for the junction at the cursor.
	StringID placement_error = INVALID_STRING_ID; ///< First error if the placement is invalid, else INVALID_STRING_ID.
	bool has_placement = false; ///< The cursor is over the map and the fields above are set.
	StringID generate_error = INVALID_STRING_ID; ///< Why the notations can't be generated.
	QueryString ml_editbox; ///< Editbox of the mainline notation.
	QueryString branch_editbox; ///< Editbox of the branch notation.
	uint8_t tl; ///< Train length in tiles.
	std::optional<JunctionPickedTrain> picked_train; ///< The picked train, if the train length comes from one.
	StringID pick_error = INVALID_STRING_ID; ///< Error of the last pick, shown in the status line.
	uint8_t prio_choice = 0; ///< Slider choice; 0 = the strict maximum (D139).
	EncodedString shown_tooltip; ///< The cursor tooltip as last shown; empty when none is.

	BuildJunctionWindow(WindowDesc &desc, Window *parent, RailType railtype) : PickerWindowBase(desc, parent), railtype(railtype),
		ml_editbox(MAX_NOTATION_LENGTH * MAX_CHAR_LENGTH, MAX_NOTATION_LENGTH), branch_editbox(MAX_NOTATION_LENGTH * MAX_CHAR_LENGTH, MAX_NOTATION_LENGTH),
		tl(Clamp<uint8_t>(_settings_client.gui.junction_tl, 2, 12))
	{
		this->querystrings[WID_BJ_ML_TEXT] = &this->ml_editbox;
		this->querystrings[WID_BJ_BRANCH_TEXT] = &this->branch_editbox;
		this->ml_editbox.text.Assign(_settings_client.gui.junction_ml_notation);
		this->branch_editbox.text.Assign(_settings_client.gui.junction_branch_notation);
		this->InitNested(TransportType::Rail);
		/* Restore the remembered train if it is still one of ours; its data is read afresh. */
		if (_junction_picked_train.has_value()) {
			const Vehicle *v = Vehicle::GetIfValid(*_junction_picked_train);
			if (v != nullptr && v->type == VehicleType::Train && v->IsFrontEngine() && v->owner == _local_company) this->picked_train = ReadJunctionTrain(v);
			if (!this->picked_train.has_value()) _junction_picked_train = std::nullopt;
		}
		/* Else the train saved in the client config: its numbers are all the spec needs. */
		if (!this->picked_train.has_value() && _settings_client.gui.junction_train_mass > 0) {
			JunctionPickedTrain saved;
			saved.loaded.power_hp = _settings_client.gui.junction_train_power;
			saved.loaded.max_te = _settings_client.gui.junction_train_te;
			saved.loaded.mass_t = _settings_client.gui.junction_train_mass;
			saved.loaded.air_drag = _settings_client.gui.junction_train_air_drag;
			saved.loaded.max_speed = _settings_client.gui.junction_train_speed;
			saved.tl = Clamp<uint8_t>(_settings_client.gui.junction_train_tl, 2, 12);
			saved.name = _settings_client.gui.junction_train_name;
			std::string_view rest = _settings_client.gui.junction_train_engines;
			while (!rest.empty()) {
				const size_t comma = rest.find(',');
				const std::string_view item = rest.substr(0, comma);
				rest = comma == std::string_view::npos ? std::string_view{} : rest.substr(comma + 1);
				uint32_t id = 0;
				if (std::from_chars(item.data(), item.data() + item.size(), id).ec != std::errc{}) continue;
				if (Engine::IsValidID(id) && Engine::Get(id)->type == VehicleType::Train) saved.engines.push_back(static_cast<EngineID>(id));
			}
			this->picked_train = std::move(saved);
			this->tl = this->picked_train->tl;
		}
		this->prio_choice = _settings_client.gui.junction_prio;
		this->Regenerate();
	}

	/** @return Whether the generator mirrors for the road side and the window's mirror together. */
	static bool EffectiveMirror()
	{
		return _junction_mirrored != (_settings_game.vehicle.road_side == RoadVehicleDrivingSide::Left);
	}

	/**
	 * Get the key of a generation at a tile.
	 * @param tile The hovered tile.
	 * @return The key.
	 */
	JunctionGenerateKey MakeKey(TileIndex tile) const
	{
		JunctionGenerateKey key = this->MakeBaseKey(tile);
		key.prio_tiles = this->EffectivePrio();
		return key;
	}

	/**
	 * Get the key of a generation at a tile without the slider's coverage. SpecKey uses it, as EffectivePrio itself reads SpecKey.
	 * @param tile The hovered tile.
	 * @return The key, with prio_tiles 0.
	 */
	JunctionGenerateKey MakeBaseKey(TileIndex tile) const
	{
		return {tile, _junction_rotation, EffectiveMirror(), this->ml_gap, this->branch_gap,
			static_cast<uint8_t>(this->ml_line.left_tracks), static_cast<uint8_t>(this->ml_line.tracks - this->ml_line.left_tracks),
			static_cast<uint8_t>(this->branch_line.left_tracks), static_cast<uint8_t>(this->branch_line.tracks - this->branch_line.left_tracks), this->tl,
			static_cast<uint8_t>(_settings_client.gui.junction_signal_type), _settings_client.gui.drag_signals_density,
			this->picked_train.has_value() ? this->picked_train->loaded : JunctionTrainParams{}, 0,
			static_cast<uint8_t>(_settings_client.gui.junction_lane_pairing)};
	}

	/** @return The strict coverage the sizing found for the current spec; 0 before sizing is done or when it failed. */
	uint8_t SizedPrio() const
	{
		if (!this->picked_train.has_value() || this->library_job == nullptr || this->library_key != this->SpecKey() || !this->library_job->sizing_done) return 0;
		return this->library_job->spec.prio_tiles;
	}

	/** @return The coverage the slider selects (D139); 0 before sizing is done or when it failed. */
	uint8_t EffectivePrio() const
	{
		const uint8_t sized = this->SizedPrio();
		return sized == 0 ? 0 : JunctionPrioChoice(this->prio_choice, this->tl, sized).first;
	}

	/** @return The key of the library: the generator inputs only, as the library is flat and doesn't depend on orientation. */
	JunctionGenerateKey SpecKey() const
	{
		JunctionGenerateKey key = this->MakeBaseKey(INVALID_TILE);
		key.rotation = 0;
		key.mirrored = false;
		return key;
	}

	/** Start building the library if the spec changed, and join the build when it is done. */
	void EnsureLibrary()
	{
		if (!this->picked_train.has_value() || _settings_game.vehicle.train_acceleration_model == AccelerationModel::Original) return;
		const JunctionGenerateKey key = this->SpecKey();
		if (this->library_key != key) {
			if (this->library_thread.joinable()) this->library_thread.detach();
			this->library_job.reset();
			this->library_key = key;

			auto job = std::make_shared<JunctionLibraryJob>();
			job->spec = MakeJunctionSpec(this->railtype, key.ml_gap, key.branch_gap, key.tl, this->picked_train->loaded);
			if (!StartNewThread(&this->library_thread, "ottd:junctionlib", [job]() {
				const JunctionPrioSizing sizing = SizeJunctionPrio(job->spec);
				job->spec.prio_tiles = sizing.prio_tiles;
				job->sizing_error = sizing.error;
				job->sizing_done = true;
				job->library = std::make_shared<const std::vector<JunctionLibraryEntry>>(job->spec.prio_tiles == 0 ? std::vector<JunctionLibraryEntry>{} : BuildJunctionLibrary(job->spec));
				job->done = true;
			})) {
				/* No thread: an empty library, so the refine tier alone works. */
				job->library = std::make_shared<const std::vector<JunctionLibraryEntry>>();
				job->sizing_done = true;
				job->done = true;
			}
			this->library_job = std::move(job);
		}

		/* The sizing result changes the prio label and slider as soon as it is final. */
		if (this->library_job != nullptr && !this->library_job->done) {
			this->SetWidgetDirty(WID_BJ_PRIO_LABEL);
			this->SetWidgetDirty(WID_BJ_PRIO_SLIDER);
		}

		if (this->library_job != nullptr && this->library_job->done && this->library_thread.joinable()) {
			this->library_thread.join();
			this->SetWidgetDirty(WID_BJ_PRIO_LABEL);
			this->SetWidgetDirty(WID_BJ_PRIO_SLIDER);
			if (this->hover_tile != INVALID_TILE) this->ShowHover();
		}
	}

	/** @return The sizing status of the current spec: sizing while it runs, too long if it failed, else INVALID_STRING_ID (also with no train picked). */
	StringID SizingStatus() const
	{
		if (!this->picked_train.has_value()) return INVALID_STRING_ID;
		/* D219: the ramp and curve rules are the realistic model's. */
		if (_settings_game.vehicle.train_acceleration_model == AccelerationModel::Original) return STR_JUNCTION_ERROR_NEEDS_REALISTIC;
		if (this->library_job == nullptr || this->library_key != this->SpecKey() || !this->library_job->sizing_done) return STR_JUNCTION_STATUS_SIZING_PRIO;
		if (this->library_job->spec.prio_tiles != 0) return INVALID_STRING_ID;
		switch (this->library_job->sizing_error) {
			case JunctionGenerateError::NoLayout: return STR_JUNCTION_ERROR_NO_LAYOUT;
			case JunctionGenerateError::NeedsMlGap: return STR_JUNCTION_ERROR_NEEDS_ML_GAP;
			default: return STR_JUNCTION_ERROR_PRIO_TOO_LONG;
		}
	}

	/** @return Whether the library for the current spec is built. */
	bool LibraryUsable() const
	{
		return this->library_job != nullptr && this->library_job->done && this->library_key == this->SpecKey() && this->EffectivePrio() == this->SizedPrio();
	}

	/**
	 * Find a cached result, and make it the most recently used.
	 * @param key The key.
	 * @return The result, valid until the cache changes; nullptr if not cached.
	 */
	const JunctionNearResult *FindCached(const JunctionGenerateKey &key)
	{
		auto it = std::ranges::find(this->cache, key, &std::pair<JunctionGenerateKey, JunctionNearResult>::first);
		if (it == this->cache.end()) return nullptr;
		std::rotate(this->cache.begin(), it, it + 1);
		return &this->cache.front().second;
	}

	/**
	 * Add a result to the cache as the most recently used, dropping the oldest beyond the limit.
	 * @param key The key.
	 * @param result The result.
	 */
	void StoreCached(const JunctionGenerateKey &key, JunctionNearResult &&result)
	{
		std::erase_if(this->cache, [&](const auto &entry) { return entry.first == key; });
		this->cache.emplace(this->cache.begin(), key, std::move(result));
		if (this->cache.size() > MAX_JUNCTION_CACHE) this->cache.pop_back();
	}

	/**
	 * Find a cached projection plan, and make it the most recently used.
	 * @param key The key.
	 * @return The plan, valid until the cache changes; nullptr if not cached.
	 */
	const JunctionProjectionResult *FindProjectionCached(const JunctionGenerateKey &key)
	{
		auto it = std::ranges::find(this->projection_cache, key, &std::pair<JunctionGenerateKey, JunctionProjectionResult>::first);
		if (it == this->projection_cache.end()) return nullptr;
		std::rotate(this->projection_cache.begin(), it, it + 1);
		return &this->projection_cache.front().second;
	}

	/** @return Whether a result of the active generator is cached for the key. */
	bool IsCached(const JunctionGenerateKey &key) const
	{
		if (_junction_projection) return std::ranges::find(this->projection_cache, key, &std::pair<JunctionGenerateKey, JunctionProjectionResult>::first) != this->projection_cache.end();
		return std::ranges::find(this->cache, key, &std::pair<JunctionGenerateKey, JunctionNearResult>::first) != this->cache.end();
	}

	/** Let any running job finish on its own; the shared state keeps it alive. */
	void DetachJob()
	{
		if (this->pending_job != nullptr) this->pending_job->cancel = true;
		if (this->pending_thread.joinable()) this->pending_thread.detach();
		this->pending_job.reset();
		this->pending_key = std::nullopt;
	}

	/** Forget the placement shown at the cursor, and the hovered spot. */
	void ResetHover()
	{
		this->hover_tile = INVALID_TILE;
		this->has_shown = false;
		this->hover_status = INVALID_STRING_ID;
		this->hover_why.clear();
	}

	/** Show no placement at the cursor, with a status for the spot. */
	void ShowNoPlacement(StringID status)
	{
		JunctionGhost::Clear();
		this->has_shown = false;
		this->has_placement = false;
		this->hover_status = status;
		this->hover_why.clear();
		_thd.make_square_red = false;
		SetTileSelectSize(1, 1);
		this->SetWidgetDirty(WID_BJ_PRIO_LABEL);
		this->SetWidgetDirty(WID_BJ_PRIO_SLIDER);
	}

	/** Show the placement for the hovered tile: the cached result, or that one is being worked out. */
	void ShowHover()
	{
		if (_junction_projection) {
			const JunctionProjectionResult *projection = this->FindProjectionCached(this->MakeKey(this->hover_tile));
			if (projection != nullptr) {
				this->ShowProjection(*projection);
			} else if (!this->picked_train.has_value()) {
				this->ShowNoPlacement(STR_JUNCTION_STATUS_PICK_TRAIN);
			} else if (this->SizingStatus() != INVALID_STRING_ID) {
				this->ShowNoPlacement(this->SizingStatus());
			} else if (this->ml_gap == 0) {
				this->ShowNoPlacement(STR_JUNCTION_ERROR_NEEDS_ML_GAP);
			} else {
				this->ShowNoPlacement(STR_JUNCTION_STATUS_GENERATING);
			}
			return;
		}

		const JunctionNearResult *result = this->FindCached(this->MakeKey(this->hover_tile));
		if (result != nullptr) {
			this->ShowResult(*result);
		} else if (!this->picked_train.has_value()) {
			this->ShowNoPlacement(STR_JUNCTION_STATUS_PICK_TRAIN);
		} else if (this->SizingStatus() != INVALID_STRING_ID) {
			this->ShowNoPlacement(this->SizingStatus());
		} else if (!this->LibraryUsable()) {
			this->ShowNoPlacement(STR_JUNCTION_STATUS_GENERATING);
		} else if (this->ml_gap == 0) {
			this->ShowNoPlacement(STR_JUNCTION_ERROR_NEEDS_ML_GAP);
		} else {
			/* The instant tier: fit the library around the cursor. The refine tier may still find something better. */
			const bool mirrored = EffectiveMirror();
			const auto t0 = std::chrono::steady_clock::now();
			const std::vector<JunctionLibraryEntry> &lib = *this->library_job->library;
			const JunctionTerrain terrain = MakeJunctionTerrain(this->hover_tile, _junction_rotation, mirrored, lib.empty() ? MakeJunctionFrame(this->library_job->spec) : lib.front().frame, 4);
			const auto t1 = std::chrono::steady_clock::now();
			const JunctionNearResult fit = FitJunctionLibraryNear(lib, terrain, _junction_rotation, mirrored);
			const auto t2 = std::chrono::steady_clock::now();
			Debug(Facility::Misc, Severity::Debug2, "junction instant tier: snapshot {} us, fit {} us",
				std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count(),
				std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count());
			if (fit.result.layout.has_value()) {
				this->ShowResult(fit);
			} else {
				this->ShowNoPlacement(STR_JUNCTION_STATUS_GENERATING);
			}
		}
	}

	/**
	 * Show a generated result at the hovered tile, with its ghost, cost and tooltip.
	 * @param near_result The result generated for the hovered tile and the current settings, with its offset.
	 */
	void ShowResult(const JunctionNearResult &near_result)
	{
		const JunctionGenerateResult &result = near_result.result;
		if (!result.layout.has_value()) {
			StringID status = STR_JUNCTION_ERROR_NO_LAYOUT_HERE;
			if (result.error == JunctionGenerateError::NeedsMlGap) status = STR_JUNCTION_ERROR_NEEDS_ML_GAP;
			if (result.error == JunctionGenerateError::PrioTooLong) status = STR_JUNCTION_ERROR_PRIO_TOO_LONG;
			if (result.error == JunctionGenerateError::PrioDelaysMainline) status = STR_JUNCTION_ERROR_PRIO_DELAYS_MAINLINE;
			if (result.error == JunctionGenerateError::PortBlocked) status = STR_JUNCTION_ERROR_PORT_BLOCKED;
			this->ShowNoPlacement(status);
			return;
		}

		const int tx = static_cast<int>(TileX(this->hover_tile)) + near_result.offset.x;
		const int ty = static_cast<int>(TileY(this->hover_tile)) + near_result.offset.y;
		if (tx < 0 || ty < 0 || tx >= static_cast<int>(Map::SizeX()) || ty >= static_cast<int>(Map::SizeY())) {
			this->ShowNoPlacement(STR_ERROR_TOO_CLOSE_TO_EDGE_OF_MAP);
			return;
		}
		const TileIndex tile = TileXY(tx, ty);
		const bool mirrored = EffectiveMirror();
		JunctionLayout layout = ApplyJunctionOrientation(*result.layout, _junction_rotation, mirrored);
		const TileIndexDiffC offset = OrientJunctionOffset(result.hub, result.layout->size, _junction_rotation, mirrored);
		const int ox = static_cast<int>(TileX(tile)) - offset.x;
		const int oy = static_cast<int>(TileY(tile)) - offset.y;
		if (ox < 0 || oy < 0 || ox >= static_cast<int>(Map::SizeX()) || oy >= static_cast<int>(Map::SizeY())) {
			this->ShowNoPlacement(STR_ERROR_TOO_CLOSE_TO_EDGE_OF_MAP);
			return;
		}
		const TileIndex origin = TileXY(ox, oy);
		this->ShowPlaced(std::move(layout), origin, result.prio_tiles, std::max(std::abs(near_result.offset.x), std::abs(near_result.offset.y)), std::nullopt, {}, {});
	}

	/**
	 * Show a projection plan at the hovered tile, or why there is none.
	 * @param projection The plan made for the hovered tile and the current settings.
	 */
	void ShowProjection(const JunctionProjectionResult &projection)
	{
		if (!projection.layout.has_value()) {
			this->ShowNoPlacement(STR_JUNCTION_STATUS_PLAN_FAILED);
			this->hover_why = projection.why;
			return;
		}

		const bool mirrored = EffectiveMirror();
		const TileIndexDiffC offset = OrientJunctionOffset(projection.hub, projection.layout->size, _junction_rotation, mirrored);
		const int ox = static_cast<int>(TileX(this->hover_tile)) - offset.x;
		const int oy = static_cast<int>(TileY(this->hover_tile)) - offset.y;
		if (ox < 0 || oy < 0 || ox >= static_cast<int>(Map::SizeX()) || oy >= static_cast<int>(Map::SizeY())) {
			this->ShowNoPlacement(STR_ERROR_TOO_CLOSE_TO_EDGE_OF_MAP);
			return;
		}
		std::vector<std::pair<TileIndexDiffC, int8_t>> levels;
		for (const auto &[tile_offset, level] : projection.track_levels) levels.emplace_back(OrientJunctionOffset(tile_offset, projection.layout->size, _junction_rotation, mirrored), level);
		this->ShowPlaced(ApplyJunctionOrientation(*projection.layout, _junction_rotation, mirrored), TileXY(ox, oy), this->EffectivePrio(), projection.distance, projection.height_cost, projection.steps, std::move(levels));
	}

	/**
	 * Show an oriented layout at its origin, with its ghost, cost and tooltip.
	 * @param layout The oriented layout.
	 * @param origin North corner of the layout.
	 * @param prio_tiles Prio coverage of the layout, in tiles.
	 * @param distance Chebyshev distance in tiles from the hovered tile to the layout's anchor.
	 * @param height_cost The height cost of the projection plan; nullopt on the earlier path.
	 * @param steps The terraform steps of the projection plan; empty on the earlier path.
	 * @param levels The track levels of the projection plan, as offsets from origin; empty on the earlier path.
	 */
	void ShowPlaced(JunctionLayout &&layout, TileIndex origin, uint8_t prio_tiles, int distance, std::optional<Money> height_cost, std::vector<JunctionTerraformStep> steps, std::vector<std::pair<TileIndexDiffC, int8_t>> levels)
	{
		JunctionEstimate estimate;
		std::vector<std::optional<BridgeType>> bridge_types;
		Money available;
		this->ComputePlacement(layout, origin, height_cost, steps, estimate, bridge_types, available);
		/* The ghost outlines its own footprint; a big tile selection would add a blue outer row. */
		SetTileSelectSize(1, 1);

		/* A moved corner changes the four tiles around it. */
		std::vector<TileIndex> terraform_tiles;
		for (const JunctionTerraformStep &step : steps) {
			const int x = static_cast<int>(TileX(step.tile));
			const int y = static_cast<int>(TileY(step.tile));
			for (int dy = -1; dy <= 0; dy++) {
				for (int dx = -1; dx <= 0; dx++) {
					if (x + dx >= 0 && y + dy >= 0 && x + dx < static_cast<int>(Map::SizeX()) && y + dy < static_cast<int>(Map::SizeY())) terraform_tiles.push_back(TileXY(x + dx, y + dy));
				}
			}
		}
		std::ranges::sort(terraform_tiles);
		terraform_tiles.erase(std::ranges::unique(terraform_tiles).begin(), terraform_tiles.end());

		JunctionGhost::Set(layout, origin, estimate.height, this->railtype, bridge_types, estimate, terraform_tiles, steps, levels);
		_thd.make_square_red = false;

		this->has_shown = true;
		this->shown_layout = std::move(layout);
		this->shown_prio_tiles = prio_tiles;
		this->SetWidgetDirty(WID_BJ_TRAIN);
		this->shown_origin = origin;
		this->shown_tile = this->hover_tile;
		this->shown_distance = distance;
		this->shown_terraform = std::move(steps);
		this->shown_height_cost = height_cost;
		this->hover_why.clear();
		this->hover_status = INVALID_STRING_ID;
		this->has_placement = true;
		this->placement_cost = estimate.total;
		this->placement_affordable = estimate.affordable;
		this->placement_error = estimate.bad_tiles.empty() ? INVALID_STRING_ID : GetPlacementError(estimate);
		this->SetWidgetDirty(WID_BJ_PRIO_LABEL);
		this->SetWidgetDirty(WID_BJ_PRIO_SLIDER);
	}

	/** Start a job for the hovered tile on the main thread's snapshot, unless one for it is running. */
	void StartJob()
	{
		if (!this->picked_train.has_value() || this->SizingStatus() != INVALID_STRING_ID) return;
		const JunctionGenerateKey key = this->MakeKey(this->hover_tile);
		if (this->pending_key.has_value() && *this->pending_key == key) return;
		/* A job for another spot may still run; it finishes on its own and its result is ignored. */
		this->DetachJob();

		auto job = std::make_shared<JunctionGenerateJob>();
		job->spec = MakeJunctionSpec(this->railtype, key.ml_gap, key.branch_gap, key.tl, this->picked_train->loaded);
		/* The sized prio sets the frame, so it goes in before the snapshot (D137). */
		std::tie(job->spec.prio_tiles, job->spec.prio_strict) = JunctionPrioChoice(this->prio_choice, key.tl, this->library_job->spec.prio_tiles);
		if (_junction_projection) {
			this->StartProjectionJob(std::move(job), key);
			return;
		}
		job->terrain = MakeJunctionTerrain(key.anchor, key.rotation, key.mirrored, MakeJunctionFrame(job->spec), 4);
		if (this->LibraryUsable()) job->library = this->library_job->library;
		if (!StartNewThread(&this->pending_thread, "ottd:junction", [job, key]() {
			job->result = GenerateJunctionNear(job->spec, job->terrain, key.rotation, key.mirrored, job->cancel);
			if (job->library != nullptr && !job->cancel) {
				JunctionNearResult fit = FitJunctionLibraryNear(*job->library, job->terrain, key.rotation, key.mirrored);
				/* Prefer a layout, then the nearer one, then the cheaper; on a full tie the fit, so the ghost doesn't flicker. */
				const JunctionNearResult &full = job->result;
				bool take_fit = true;
				if (full.result.layout.has_value() != fit.result.layout.has_value()) {
					take_fit = fit.result.layout.has_value();
				} else if (full.result.layout.has_value()) {
					const auto distance = [](const JunctionNearResult &r) { return std::max(std::abs(r.offset.x), std::abs(r.offset.y)); };
					const auto rank = [](const JunctionNearResult &r) {
						return std::tuple<int, Money, int>{r.result.score.first, r.result.score.second, r.result.layout->size.x * r.result.layout->size.y};
					};
					if (distance(full) != distance(fit)) {
						take_fit = distance(fit) < distance(full);
					} else {
						take_fit = rank(fit) <= rank(full);
					}
				} else {
					take_fit = false;
				}
				if (take_fit) job->result = std::move(fit);
			}
			job->done = true;
		})) {
			/* No thread: report no layout rather than waiting forever. */
			job->done = true;
		}
		this->pending_job = std::move(job);
		this->pending_key = key;
	}

	/**
	 * Start a projection job: the seed is made here on the main thread, once per spec; the worker plans on the snapshot and makes the terraform steps.
	 * @param job The job, with its spec set.
	 * @param key The key of the job.
	 */
	void StartProjectionJob(std::shared_ptr<JunctionGenerateJob> job, const JunctionGenerateKey &key)
	{
		if (this->ml_gap == 0) return;
		JunctionGenerateKey spec_key = key;
		spec_key.anchor = INVALID_TILE;
		spec_key.rotation = 0;
		spec_key.mirrored = false;
		if (this->seed_key != spec_key) {
			std::string error;
			/* The seed points at its topology: the holder keeps both alive for the jobs that share the seed. */
			auto holder = std::make_shared<std::pair<JunctionTopology, JunctionGraphSeed>>();
			holder->first = MakeJunctionHubTopology(this->ml_line, this->branch_line, static_cast<JunctionLanePairing>(key.lane_pairing));
			holder->second = MakeJunctionGraphSeed(holder->first, job->spec, &error);
			this->seed = std::shared_ptr<const JunctionGraphSeed>(holder, &holder->second);
			this->seed_error = this->seed->routes.empty() ? (error.empty() ? "no seed" : error) : std::string{};
			this->seed_key = spec_key;
		}

		job->projection = true;
		job->seed = this->seed;
		this->pending_key = key;
		if (!this->seed_error.empty()) {
			job->projection_result.why = this->seed_error;
			job->done = true;
			this->pending_job = std::move(job);
			return;
		}

		/* The frame is the seed's box with the hub column in the middle, the box's first row on the anchor row; levels need margin 8, and the near search moves the box by up to its columns or rows. */
		const TileIndexDiffC size = job->seed->placement.size;
		JunctionFrame frame;
		frame.hub_x = size.x / 2;
		frame.width = size.x;
		frame.terrain_origin_y = 0;
		frame.terrain_height = size.y;
		const auto t0 = std::chrono::steady_clock::now();
		job->terrain = MakeJunctionTerrain(key.anchor, key.rotation, key.mirrored, frame, 8 + std::max(JUNCTION_HOVER_NEAR_COLUMNS, JUNCTION_HOVER_NEAR_ROWS));
		Debug(Facility::Misc, Severity::Debug2, "junction projection snapshot: {} us", std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - t0).count());
		const int hub_x = frame.hub_x;
		if (!StartNewThread(&this->pending_thread, "ottd:junctionproj", [job, key, hub_x]() {
			JunctionHoverPlan plan = PlanJunctionHoverNear(*job->seed, job->spec, job->terrain, {0, 0}, &job->cancel);
			JunctionProjectionResult &result = job->projection_result;
			result.why = plan.why;
			result.height_cost = plan.height_cost;
			result.track_levels = plan.track_levels;
			result.hub = {static_cast<int16_t>(hub_x - plan.moved.x), static_cast<int16_t>(-plan.moved.y)};
			result.distance = std::max(std::abs(plan.moved.x), std::abs(plan.moved.y));
			if (plan.layout.has_value()) {
				result.steps = MakeJunctionTerraformSteps(plan.terraform, job->terrain, key.anchor, key.rotation, key.mirrored, hub_x);
				result.layout = std::move(plan.layout);
			}
			job->done = true;
		})) {
			job->projection_result.why = "no thread";
			job->done = true;
		}
		this->pending_job = std::move(job);
	}

	/** Store the running job's result when it is done, and show it if it is for the hovered spot. */
	void PollJob()
	{
		if (this->pending_job == nullptr || !this->pending_job->done) return;
		if (this->pending_thread.joinable()) this->pending_thread.join();
		const JunctionGenerateKey key = *this->pending_key;
		if (this->pending_job->projection) {
			std::erase_if(this->projection_cache, [&](const auto &entry) { return entry.first == key; });
			this->projection_cache.emplace(this->projection_cache.begin(), key, std::move(this->pending_job->projection_result));
			if (this->projection_cache.size() > MAX_JUNCTION_CACHE) this->projection_cache.pop_back();
		} else {
			this->StoreCached(key, std::move(this->pending_job->result));
		}
		this->pending_job.reset();
		this->pending_key = std::nullopt;
		if (this->hover_tile != INVALID_TILE && key == this->MakeKey(this->hover_tile)) this->ShowHover();
	}

	/** Check and remember the notations and the train length as settings. Generation itself happens per hovered spot. */
	void Regenerate()
	{
		this->generate_error = INVALID_STRING_ID;

		const std::string ml_text{this->ml_editbox.text.GetText()};
		const std::string branch_text{this->branch_editbox.text.GetText()};
		const JunctionLineNotation ml = ParseLineNotation(ml_text);
		const JunctionLineNotation branch = ParseLineNotation(branch_text);

		if (ml.error != JunctionNotationError::None || branch.error != JunctionNotationError::None) {
			this->generate_error = STR_JUNCTION_ERROR_NOTATION;
		} else {
			this->ml_gap = ml.spec.gap;
			this->branch_gap = branch.spec.gap;
			this->ml_line = ml.spec;
			this->branch_line = branch.spec;
		}

		_settings_client.gui.junction_ml_notation = ml_text;
		_settings_client.gui.junction_branch_notation = branch_text;
		_settings_client.gui.junction_tl = this->tl;

		this->cache.clear();
		this->projection_cache.clear();
		this->ResetHover();
		this->EnsureLibrary();
		this->SetWidgetDisabledState(WID_BJ_TL_DECREASE, this->tl <= 2);
		this->SetWidgetDisabledState(WID_BJ_TL_INCREASE, this->tl >= 12);
		this->UpdateLanesButton();
		this->SetDirty();
	}

	/** Set the lane pairing button's text from the setting, and disable it when no turning movement is multi-to-multi (D221). */
	void UpdateLanesButton()
	{
		static const StringID labels[] = {STR_JUNCTION_LANES_ANY, STR_JUNCTION_LANES_KEEP, STR_JUNCTION_LANES_DIRECTION};
		this->GetWidget<NWidgetCore>(WID_BJ_LANES)->SetString(labels[std::min<uint8_t>(_settings_client.gui.junction_lane_pairing, 2)]);
		const uint8_t ml_left = this->ml_line.left_tracks, ml_right = this->ml_line.tracks - this->ml_line.left_tracks;
		const uint8_t branch_left = this->branch_line.left_tracks, branch_right = this->branch_line.tracks - this->branch_line.left_tracks;
		/* A turning movement goes between a main group and a branch group, in either direction. */
		const bool multi = this->generate_error == INVALID_STRING_ID && std::max(ml_left, ml_right) > 1 && std::max(branch_left, branch_right) > 1;
		this->SetWidgetDisabledState(WID_BJ_LANES, !multi);
		this->SetWidgetDirty(WID_BJ_LANES);
	}

	void Close(int data = 0) override
	{
		_junction_picking_train = false;
		this->DetachJob();
		if (this->library_thread.joinable()) this->library_thread.detach();
		this->library_job.reset();
		GetJunctionSequencer().Cancel();
		JunctionGhost::Clear();
		JunctionGhost::SetPinned(false);
		SetTileSelectSize(1, 1);
		_thd.make_square_red = false;
		this->PickerWindowBase::Close(data);
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_BJ_RETRY: {
				/* Pieces already built make their tiles fail an estimate, so only check the money. */
				GameJunctionPricing pricing(this->railtype);
				if (pricing.AvailableMoney() > 0) GetJunctionSequencer().Retry();
				break;
			}

			case WID_BJ_CANCEL:
				GetJunctionSequencer().Cancel();
				JunctionGhost::Clear();
				JunctionGhost::SetPinned(false);
				SetTileSelectSize(1, 1);
				this->ResetHover();
				this->SetDirty();
				break;

			case WID_BJ_PICK_TRAIN:
				this->pick_error = INVALID_STRING_ID;
				_junction_picking_train = true;
				SetObjectToPlaceWnd(SPR_CURSOR_CLONE_TRAIN, PAL_NONE, HT_VEHICLE, this);
				SndClickBeep();
				break;

			case WID_BJ_TL_DECREASE:
				if (this->tl > 2) {
					this->tl--;
					this->prio_choice = _settings_client.gui.junction_prio = 0;
					this->Regenerate();
				}
				break;

			case WID_BJ_TL_INCREASE:
				if (this->tl < 12) {
					this->tl++;
					this->prio_choice = _settings_client.gui.junction_prio = 0;
					this->Regenerate();
				}
				break;

			case WID_BJ_PRIO_SLIDER: {
				const uint8_t sized = this->SizedPrio();
				if (sized == 0) break;
				uint8_t value = this->EffectivePrio();
				if (ClickSliderWidget(this->GetWidget<NWidgetBase>(widget)->GetCurrentRect(), pt, std::min(this->tl, sized), sized, 0, value)) {
					const uint8_t choice = value >= sized ? 0 : value;
					if (choice != this->prio_choice) {
						this->prio_choice = _settings_client.gui.junction_prio = choice;
						this->Regenerate();
					}
				}
				if (click_count > 0) this->mouse_capture_widget = widget;
				break;
			}

			case WID_BJ_ROTATE:
				_junction_rotation = (_junction_rotation + 1) % 4;
				this->ResetHover();
				SndClickBeep();
				break;

			case WID_BJ_MIRROR:
				_junction_mirrored = !_junction_mirrored;
				this->ResetHover();
				SndClickBeep();
				break;

			case WID_BJ_LANES:
				_settings_client.gui.junction_lane_pairing = (_settings_client.gui.junction_lane_pairing + 1) % 3;
				this->UpdateLanesButton();
				this->cache.clear();
				this->projection_cache.clear();
				this->ResetHover();
				this->EnsureLibrary();
				SndClickBeep();
				break;
		}
	}

	bool OnVehicleSelect(const Vehicle *v) override
	{
		std::optional<JunctionPickedTrain> picked = ReadJunctionTrain(v);
		if (!picked.has_value()) {
			this->pick_error = STR_JUNCTION_ERROR_NOT_OUR_TRAIN;
			return true;
		}

		this->pick_error = INVALID_STRING_ID;
		this->tl = picked->tl;
		this->picked_train = std::move(picked);
		_junction_picked_train = v->First()->index;
		const JunctionPickedTrain &t = *this->picked_train;
		_settings_client.gui.junction_train_power = t.loaded.power_hp;
		_settings_client.gui.junction_train_te = t.loaded.max_te;
		_settings_client.gui.junction_train_mass = t.loaded.mass_t;
		_settings_client.gui.junction_train_air_drag = t.loaded.air_drag;
		_settings_client.gui.junction_train_speed = t.loaded.max_speed;
		_settings_client.gui.junction_train_tl = t.tl;
		_settings_client.gui.junction_train_name = t.name;
		std::string engines;
		for (const EngineID e : t.engines) engines += fmt::format("{}{}", engines.empty() ? "" : ",", e.base());
		_settings_client.gui.junction_train_engines = std::move(engines);
		this->SetWidgetDirty(WID_BJ_TRAIN_IMAGE);
		this->prio_choice = _settings_client.gui.junction_prio = 0;
		this->Regenerate();
		this->SetWidgetDirty(WID_BJ_TRAIN);
		_junction_picking_train = false;
		RestoreJunctionPlacement();
		return true;
	}

	void OnPlaceObjectAbort() override
	{
		if (!_junction_picking_train) return;
		_junction_picking_train = false;
		RestoreJunctionPlacement();
	}

	void OnEditboxChanged(WidgetID widget) override
	{
		if (widget == WID_BJ_ML_TEXT || widget == WID_BJ_BRANCH_TEXT) this->Regenerate();
	}

	EventState OnHotkey(int hotkey) override
	{
		switch (hotkey) {
			case JHK_ROTATE:
				this->OnClick({}, WID_BJ_ROTATE, 1);
				return EventState::Handled;

			case JHK_MIRROR:
				this->OnClick({}, WID_BJ_MIRROR, 1);
				return EventState::Handled;

			default:
				return this->PickerWindowBase::OnHotkey(hotkey);
		}
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		if (widget == WID_BJ_TRAIN) {
			if (!this->picked_train.has_value()) return GetString(STR_JUNCTION_TRAIN_NONE);
			const JunctionPickedTrain &t = *this->picked_train;
			const uint32_t prio = this->has_shown ? this->shown_prio_tiles : this->SizedPrio();
			if (prio == 0) return GetString(STR_JUNCTION_TRAIN_NO_PRIO, t.name, t.tl);
			return GetString(STR_JUNCTION_TRAIN, t.name, t.tl, prio);
		}
		if (widget == WID_BJ_PRIO_LABEL) {
			if (this->SizedPrio() == 0) return GetString(STR_EMPTY);
			return GetString(STR_JUNCTION_PRIO_LENGTH, this->EffectivePrio(), this->SizedPrio());
		}
		if (widget == WID_BJ_TL_LABEL) return GetString(STR_JUNCTION_TRAIN_LENGTH, this->tl);
		return this->PickerWindowBase::GetWidgetString(widget, stringid);
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget == WID_BJ_TRAIN_IMAGE) {
			/* The remembered train, if it is still there; a longer one is clipped at the widget's end. */
			const Train *t = _junction_picked_train.has_value() && this->picked_train.has_value() ? Train::GetIfValid(*_junction_picked_train) : nullptr;
			const Rect ir = r.Shrink(WidgetDimensions::scaled.framerect);
			if (t != nullptr) {
				DrawTrainImage(t, ir, VehicleID::Invalid(), EngineImageType::InDepot, 0);
			} else if (this->picked_train.has_value()) {
				/* A train saved in the client config: each engine's sprite, left to right. */
				int x = ir.left;
				for (const EngineID e : this->picked_train->engines) {
					const RailVehicleInfo *rvi = RailVehInfo(e);
					const GRFFile *grf = Engine::Get(e)->GetGRF();
					const int ref_width = grf != nullptr && IsCustomVehicleSpriteNum(rvi->image_index) ? grf->traininfo_vehicle_width : TRAININFO_DEFAULT_VEHICLE_WIDTH;
					const int width = ScaleSpriteTrad((VEHICLE_LENGTH - rvi->shorten_factor) * ref_width / VEHICLE_LENGTH);
					DrawTrainEngine(ir.left, ir.right, x + width / 2, CentreBounds(ir.top, ir.bottom, 0), e, GetEnginePalette(e, _local_company), EngineImageType::InDepot);
					x += width;
				}
			}
			return;
		}
		if (widget != WID_BJ_PRIO_SLIDER) return;
		const uint8_t sized = this->SizedPrio();
		if (sized == 0) return; // No value to show: the row stays blank.
		DrawSliderWidget(r, Colours::Mauve, Colours::Yellow, TextColour::Black, std::min(this->tl, sized), sized, 0, this->EffectivePrio(), nullptr);
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		if (widget == WID_BJ_TRAIN_IMAGE) {
			size.height = std::max(size.height, GetVehicleImageCellSize(VehicleType::Train, EngineImageType::InDepot).height + WidgetDimensions::scaled.framerect.Vertical());
			size.width = std::max(size.width, (GetVehicleImageCellSize(VehicleType::Train, EngineImageType::InDepot).extend_left + GetVehicleImageCellSize(VehicleType::Train, EngineImageType::InDepot).extend_right) * 2);
		}
		if (widget == WID_BJ_TRAIN) {
			/* Room for the current text, or for a 12 character name and 2 digit numbers when that is wider. */
			size.width = std::max({size.width, GetStringBoundingBox(this->GetWidgetString(widget, STR_NULL)).width + padding.width,
				GetStringBoundingBox(GetString(STR_JUNCTION_TRAIN, std::string(12, 'W'), GetParamMaxDigits(2), GetParamMaxDigits(2))).width + padding.width});
		}
	}

	/**
	 * Work out the estimate and bridge types for placing an oriented junction.
	 * @param layout The oriented layout.
	 * @param origin North corner of the junction.
	 * @param height_cost The height cost of the projection plan, which replaces the plane check; nullopt on the earlier path.
	 * @param terraform The terraform steps of the projection plan; the estimate judges the tiles they move on the planned ground.
	 * @param[out] estimate The estimate of building it.
	 * @param[out] bridge_types Bridge type chosen per layout bridge.
	 * @param[out] available Money the local company can spend.
	 */
	void ComputePlacement(const JunctionLayout &layout, TileIndex origin, std::optional<Money> height_cost, const std::vector<JunctionTerraformStep> &terraform, JunctionEstimate &estimate, std::vector<std::optional<BridgeType>> &bridge_types, Money &available) const
	{
		GameJunctionPricing pricing(this->railtype);
		estimate = height_cost.has_value() ? EstimateJunctionTerraformed(layout, origin, pricing, *height_cost, terraform) : EstimateJunction(layout, origin, pricing);
		available = pricing.AvailableMoney();

		bridge_types.clear();
		for (const JunctionBridge &bridge : layout.bridges) {
			uint length = abs(bridge.end.x - bridge.start.x) + abs(bridge.end.y - bridge.start.y) - 1;
			bridge_types.push_back(ChooseJunctionBridgeType(this->railtype, length));
		}
	}

	/**
	 * Start building the junction shown at the cursor, if it can be built and paid for.
	 * @param tile The clicked tile; it must be the one the shown placement belongs to.
	 */
	void StartBuild(TileIndex tile)
	{
		if (!this->has_shown || tile != this->shown_tile) return;

		const TileIndex origin = this->shown_origin;
		const JunctionLayout layout = this->shown_layout;
		JunctionEstimate estimate;
		std::vector<std::optional<BridgeType>> bridge_types;
		Money available;
		this->ComputePlacement(layout, origin, this->shown_height_cost, this->shown_terraform, estimate, bridge_types, available);

		/* Shift-click estimates the cost instead of building, as with every other build tool. */
		if (IsShiftPressed()) {
			if (estimate.bad_tiles.empty()) {
				ShowEstimatedCostOrIncome(estimate.total, TileX(tile) * TILE_SIZE, TileY(tile) * TILE_SIZE);
			} else {
				CommandCost cc(GetPlacementError(estimate));
				ShowErrorMessage(GetEncodedString(STR_ERROR_CAN_T_BUILD_RAILROAD_TRACK), TileX(tile) * TILE_SIZE, TileY(tile) * TILE_SIZE, cc);
			}
			return;
		}

		if (!estimate.affordable) return;

		this->build_layout = layout;
		this->build_origin = origin;
		this->build_height = estimate.height;
		this->build_quote = estimate.total;
		this->build_bridge_types = bridge_types;
		this->last_state = JunctionBuildState::Building;
		/* The terraform steps come first, then the rail, bridge and signal steps. */
		std::vector<JunctionBuildStep> steps(this->shown_terraform.begin(), this->shown_terraform.end());
		const std::vector<JunctionBuildStep> build_steps = MakeJunctionBuildSteps(layout, origin);
		steps.insert(steps.end(), build_steps.begin(), build_steps.end());
		GetJunctionSequencer().Start(std::move(steps));
		JunctionGhost::SetPinned(true);
		/* Shrink the cursor box to one tile while the pinned footprint shows the build; 0x0 trips an assert in SetSelectionTilesDirty. */
		SetTileSelectSize(1, 1);
	}

	/** @return The cost line of the placement at the cursor; empty when there is none. */
	std::string CostText() const
	{
		if (!this->has_placement) return {};
		if (this->placement_error != INVALID_STRING_ID) return GetString(STR_JUNCTION_TOOLTIP_INVALID, this->placement_error);
		if (!this->placement_affordable) return GetString(STR_JUNCTION_TOOLTIP_UNAFFORDABLE, this->placement_cost, GameJunctionPricing(this->railtype).AvailableMoney());
		return GetString(STR_JUNCTION_TOOLTIP_COST, this->placement_cost);
	}

	/** @return The status line: the pick error, why nothing is shown, how the shown placement sits, or the build progress. */
	std::string StatusText() const
	{
		const JunctionSequencer &seq = GetJunctionSequencer();
		if (seq.GetState() == JunctionBuildState::Idle) {
			if (this->pick_error != INVALID_STRING_ID && _junction_picking_train) return GetString(this->pick_error);
			if (this->generate_error != INVALID_STRING_ID) return GetString(this->generate_error);
			if (!this->picked_train.has_value()) return GetString(STR_JUNCTION_STATUS_PICK_TRAIN);
			if (this->SizingStatus() != INVALID_STRING_ID) return GetString(this->SizingStatus());
			if (this->hover_status == STR_JUNCTION_STATUS_PLAN_FAILED) return GetString(this->hover_status, this->hover_why);
			if (this->hover_status != INVALID_STRING_ID) return GetString(this->hover_status);
			if (this->has_shown && this->shown_distance != 0) return GetString(STR_JUNCTION_STATUS_NEARBY, this->shown_distance);
		}
		switch (seq.GetState()) {
			case JunctionBuildState::Building: return GetString(STR_JUNCTION_STATUS_BUILDING, seq.GetDone(), seq.GetTotal());
			case JunctionBuildState::Failed: return GetString(STR_JUNCTION_STATUS_FAILED, seq.GetDone(), seq.GetTotal());
			default: return GetString(STR_JUNCTION_STATUS_READY);
		}
	}

	/** Show Retry and Cancel while a build is pinned, else nothing. */
	void ShowBuildButtons(bool show)
	{
		if (this->GetWidget<NWidgetStacked>(WID_BJ_BUILD_SEL)->SetDisplayedPlane(show ? 0 : SZSP_HORIZONTAL)) this->ReInit();
	}

	/** Show the cost and status as a cursor tooltip while the cursor is over the map, and close it when it is not; only a change of text shows it anew. */
	void UpdateTooltip()
	{
		EncodedString text;
		if (GetTileBelowCursor().x != -1) {
			const std::string cost = this->CostText();
			text = GetEncodedString(STR_JUNCTION_TOOLTIP, cost.empty() ? this->StatusText() : cost + "\n" + this->StatusText());
		}
		if (text == this->shown_tooltip) return;
		this->shown_tooltip = text;
		GuiShowTooltips(this, std::move(text), TooltipCloseCondition::ExitViewport);
	}

	void OnRealtimeTick([[maybe_unused]] uint delta_ms) override
	{
		GameJunctionStepPoster poster(this->railtype);
		JunctionSequencer &seq = GetJunctionSequencer();
		seq.Tick(poster);

		if (seq.GetState() == JunctionBuildState::Done) {
			if (_settings_client.sound.confirm) SndPlayTileFx(SND_20_CONSTRUCTION_RAIL, this->build_origin);
			Debug(Facility::Misc, Severity::Debug1, "Junction built: spent {}, quoted {}", seq.spent.base(), this->build_quote.base());
			ShowErrorMessage(GetEncodedString(STR_JUNCTION_STATUS_SPENT, seq.spent, this->build_quote), {}, WarningLevel::Info, TileX(this->build_origin) * TILE_SIZE, TileY(this->build_origin) * TILE_SIZE);
			seq.Cancel();
			/* The map changed, so earlier results no longer hold. */
			this->cache.clear();
			this->projection_cache.clear();
			this->DetachJob();
			JunctionGhost::SetPinned(false);
			SetTileSelectSize(1, 1);
			this->ResetHover();
			this->SetDirty();
		}

		JunctionBuildState state = seq.GetState();
		bool just_failed = state == JunctionBuildState::Failed && this->last_state != JunctionBuildState::Failed;
		this->last_state = state;

		if (state == JunctionBuildState::Building || state == JunctionBuildState::Failed) {
			if (just_failed) {
				/* Show what is left to build. */
				JunctionEstimate estimate;
				for (const JunctionBuildStep &step : seq.GetRemaining()) {
					if (const auto *rail = std::get_if<JunctionRailStep>(&step)) {
						estimate.bad_tiles.push_back(rail->tile);
					} else if (const auto *bridge = std::get_if<JunctionBridgeStep>(&step)) {
						estimate.bad_tiles.push_back(bridge->start);
						estimate.bad_tiles.push_back(bridge->end);
					} else if (const auto *station = std::get_if<JunctionStationStep>(&step)) {
						const int w = station->axis == Axis::X ? station->length : station->platforms;
						const int h = station->axis == Axis::X ? station->platforms : station->length;
						for (int y = 0; y < h; y++) {
							for (int x = 0; x < w; x++) estimate.bad_tiles.push_back(AddTileIndexDiffCWrap(station->tile, {static_cast<int16_t>(x), static_cast<int16_t>(y)}));
						}
					} else if (const auto *terraform = std::get_if<JunctionTerraformStep>(&step)) {
						estimate.bad_tiles.push_back(terraform->tile);
					} else {
						estimate.bad_tiles.push_back(std::get<JunctionSignalStep>(step).tile);
					}
				}
				JunctionGhost::Set(this->build_layout, this->build_origin, this->build_height, this->railtype, this->build_bridge_types, estimate);
				/* Set() clears, which unpins; the footprint stays pinned while the build is paused. */
				JunctionGhost::SetPinned(true);
			}
			this->ShowBuildButtons(true);
			this->SetWidgetDisabledState(WID_BJ_RETRY, state != JunctionBuildState::Failed);
			this->SetWidgetDisabledState(WID_BJ_CANCEL, false);
			this->SetWidgetDirty(WID_BJ_RETRY);
			this->SetWidgetDirty(WID_BJ_CANCEL);
			this->UpdateTooltip();
			return;
		}

		this->ShowBuildButtons(false);
		this->SetWidgetDisabledState(WID_BJ_RETRY, true);
		this->SetWidgetDisabledState(WID_BJ_CANCEL, true);
		this->SetWidgetDirty(WID_BJ_RETRY);
		this->SetWidgetDirty(WID_BJ_CANCEL);

		this->PollJob();
		this->EnsureLibrary();

		if (this->generate_error != INVALID_STRING_ID) {
			if (this->has_placement) {
				JunctionGhost::Clear();
				this->has_placement = false;
			}
			SetTileSelectSize(1, 1);
			this->UpdateTooltip();
			return;
		}

		/* While picking a train there is no placement: no ghost, no hover generation. */
		if (_junction_picking_train) {
			if (this->has_placement || this->has_shown) {
				JunctionGhost::Clear();
				this->ResetHover();
				this->has_placement = false;
			}
			this->UpdateTooltip();
			return;
		}

		Point pt = GetTileBelowCursor();
		if (pt.x == -1) {
			JunctionGhost::Clear();
			this->ResetHover();
			this->has_placement = false;
			this->UpdateTooltip();
			return;
		}

		const auto now = std::chrono::steady_clock::now();
		TileIndex tile = TileVirtXY(pt.x, pt.y);
		if (tile != this->hover_tile) {
			this->hover_tile = tile;
			this->hover_since = now;
				this->shown_tooltip.clear(); // The tooltip is placed where it is shown, so it is shown anew at each tile.
			this->ShowHover();
		}

		if (now - this->hover_since >= JUNCTION_HOVER_REST) {
			const JunctionGenerateKey key = this->MakeKey(tile);
			if (!this->IsCached(key)) this->StartJob();
		}
		this->UpdateTooltip();
	}

	static inline HotkeyList hotkeys{"buildjunction", {
		Hotkey(WKC_PERIOD, "rotate", JHK_ROTATE),
		Hotkey(WKC_COMMA, "mirror", JHK_MIRROR),
	}};
};

/** Nested widget definition of the build junction window. */
static constexpr std::initializer_list<NWidgetPart> _nested_build_junction_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, Colours::DarkGreen),
		NWidget(WWT_CAPTION, Colours::DarkGreen), SetStringTip(STR_JUNCTION_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, Colours::DarkGreen),
		NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_normal, 0), SetPadding(WidgetDimensions::unscaled.picker),
			NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
				NWidget(WWT_TEXT, Colours::Invalid, WID_BJ_ML_LABEL), SetStringTip(STR_JUNCTION_MAINLINE_LABEL, STR_JUNCTION_MAINLINE_TOOLTIP),
				NWidget(WWT_EDITBOX, Colours::Grey, WID_BJ_ML_TEXT), SetToolTip(STR_JUNCTION_MAINLINE_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
				NWidget(WWT_TEXT, Colours::Invalid, WID_BJ_BRANCH_LABEL), SetStringTip(STR_JUNCTION_BRANCH_LABEL, STR_JUNCTION_BRANCH_TOOLTIP),
				NWidget(WWT_EDITBOX, Colours::Grey, WID_BJ_BRANCH_TEXT), SetToolTip(STR_JUNCTION_BRANCH_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
			EndContainer(),
			NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
				NWidget(WWT_PUSHARROWBTN, Colours::Grey, WID_BJ_TL_DECREASE), SetMinimalSize(9, 12), SetArrowWidgetTypeTip(ArrowWidgetType::Decrease, STR_JUNCTION_TRAIN_LENGTH_DECREASE_TOOLTIP),
				NWidget(WWT_LABEL, Colours::Invalid, WID_BJ_TL_LABEL), SetFill(1, 0),
				NWidget(WWT_PUSHARROWBTN, Colours::Grey, WID_BJ_TL_INCREASE), SetMinimalSize(9, 12), SetArrowWidgetTypeTip(ArrowWidgetType::Increase, STR_JUNCTION_TRAIN_LENGTH_INCREASE_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, Colours::Grey, WID_BJ_PICK_TRAIN), SetStringTip(STR_JUNCTION_PICK_TRAIN, STR_JUNCTION_PICK_TRAIN_TOOLTIP), SetFill(1, 0),
				NWidget(WWT_PUSHTXTBTN, Colours::Grey, WID_BJ_ROTATE), SetStringTip(STR_JUNCTION_ROTATE, STR_JUNCTION_ROTATE_TOOLTIP), SetFill(1, 0),
				NWidget(WWT_PUSHTXTBTN, Colours::Grey, WID_BJ_MIRROR), SetStringTip(STR_JUNCTION_MIRROR, STR_JUNCTION_MIRROR_TOOLTIP), SetFill(1, 0),
				NWidget(WWT_PUSHTXTBTN, Colours::Grey, WID_BJ_LANES), SetStringTip(STR_JUNCTION_LANES_ANY, STR_JUNCTION_LANES_TOOLTIP), SetFill(1, 0),
			EndContainer(),
			NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
				NWidget(WWT_EMPTY, Colours::Invalid, WID_BJ_TRAIN_IMAGE), SetFill(1, 0), SetResize(1, 0),
				NWidget(WWT_LABEL, Colours::Invalid, WID_BJ_TRAIN), SetFill(1, 0),
				NWidget(WWT_LABEL, Colours::Invalid, WID_BJ_PRIO_LABEL), SetFill(1, 0),
			EndContainer(),
			NWidget(WWT_EMPTY, Colours::Invalid, WID_BJ_PRIO_SLIDER), SetMinimalTextLines(1, 12 + WidgetDimensions::unscaled.vsep_normal, FontSize::Small), SetFill(1, 0), SetResize(1, 0), SetToolTip(STR_JUNCTION_PRIO_SLIDER_TOOLTIP),
			NWidget(NWID_SELECTION, Colours::Invalid, WID_BJ_BUILD_SEL),
				NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
					NWidget(WWT_PUSHTXTBTN, Colours::Grey, WID_BJ_RETRY), SetStringTip(STR_JUNCTION_RETRY, STR_JUNCTION_RETRY_TOOLTIP), SetFill(1, 0),
					NWidget(WWT_PUSHTXTBTN, Colours::Grey, WID_BJ_CANCEL), SetStringTip(STR_JUNCTION_CANCEL, STR_JUNCTION_CANCEL_TOOLTIP), SetFill(1, 0),
				EndContainer(),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

/** Window definition for the build junction window. */
static WindowDesc _build_junction_desc(
	WindowPosition::Automatic, {}, 0, 0,
	WindowClass::BuildJunction, WindowClass::BuildToolbar,
	WindowDefaultFlag::Construction,
	_nested_build_junction_widgets,
	&BuildJunctionWindow::hotkeys
);

/**
 * Open the build junction window.
 * @param parent The rail toolbar that opens the window.
 * @param railtype Rail type of that toolbar.
 */
void ShowBuildJunctionWindow(Window *parent, RailType railtype)
{
	new BuildJunctionWindow(_build_junction_desc, parent, railtype);
}

/**
 * Handle a click on the map with the junction tool.
 * @param tile The tile that was clicked.
 */
void JunctionToolClick(TileIndex tile)
{
	BuildJunctionWindow *w = dynamic_cast<BuildJunctionWindow *>(FindWindowById(WindowClass::BuildJunction, TransportType::Rail));
	if (w == nullptr || GetJunctionSequencer().GetState() != JunctionBuildState::Idle) return;

	w->StartBuild(tile);
}
