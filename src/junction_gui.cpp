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
#include "debug.h"
#include "widgets/junction_widget.h"
#include "map_func.h"
#include "slope_func.h"
#include "tile_cmd.h"
#include "viewport_func.h"

#include "table/sprites.h"
#include "table/strings.h"
#include "thread.h"

#include <atomic>
#include <chrono>
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

/** Mark every stored tile dirty, so it is redrawn. */
static void MarkJunctionGhostDirty()
{
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
 */
void JunctionGhost::Set(const JunctionLayout &layout, TileIndex origin, int height, RailType railtype, const std::vector<std::optional<BridgeType>> &bridge_types, const JunctionEstimate &estimate)
{
	JunctionGhost::Clear();
	_junction_ghost_railtype = railtype;
	_junction_ghost_footprint = TileArea(origin, layout.size.x, layout.size.y);

	for (const JunctionTrack &track : layout.tracks) {
		TileIndex tile = AddTileIndexDiffCWrap(origin, track.offset);
		if (tile == INVALID_TILE) continue;
		_junction_ghost_tiles[tile].tracks.Set(track.tracks);
	}

	for (const JunctionSignal &signal : layout.signals) {
		TileIndex tile = AddTileIndexDiffCWrap(origin, signal.offset);
		if (tile == INVALID_TILE) continue;
		_junction_ghost_tiles[tile].signals.push_back({tile, signal.facing, signal.type, signal.variant});
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
		for (uint d = 1; d <= len; d++) {
			TileIndex tile = axis == Axis::Y ? TileXY(TileX(start), TileY(start) + d) : TileXY(TileX(start) + d, TileY(start));
			_junction_ghost_tiles[tile].span = JunctionGhostBridgeSpan{*type, axis, static_cast<int>((height + 1) * TILE_HEIGHT), d, len + 1 - d};
		}
	}

	for (TileIndex tile : estimate.bad_tiles) {
		if (tile == INVALID_TILE) continue;
		_junction_ghost_tiles[tile].bad = true;
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
 * The ghost is always drawn opaque in normal colours (D44, D53); sloped tiles get no sprites.
 * @param ti Tile that is being drawn.
 */
void DrawJunctionGhost(const TileInfo *ti)
{
	const JunctionGhostTile *ghost = JunctionGhost::Get(ti->tile);
	if (ghost == nullptr || ti->tileh != SLOPE_FLAT) return;

	TileInfo copy = *ti;
	RailType railtype = JunctionGhost::GetRailType();
	if (ghost->tracks.Any()) DrawRailTrackGhost(&copy, ghost->tracks, railtype);
	if (ghost->head.has_value()) DrawBridgeHeadGhost(&copy, ghost->head->type, railtype, ghost->head->dir);
	if (ghost->span.has_value()) DrawBridgeMiddleGhost(ti, ghost->span->type, railtype, ghost->span->axis, ghost->span->bridge_z, ghost->span->piece_north, ghost->span->piece_south);
	for (const JunctionSignalStep &signal : ghost->signals) {
		DrawRailSignalGhost(ti->tile, railtype, signal.facing, signal.type, signal.variant);
	}
}

/**
 * Draw the junction ghost's outlines on one tile, after the tile selection so they show over it.
 * Sloped and bad tiles get a red outline; while a build is pinned, bad tiles are the pieces still
 * to build and get a blue one (D53, D54), and the rest of the pinned footprint gets a white one.
 * @param ti Tile that is being drawn.
 */
void DrawJunctionGhostOutline(const TileInfo *ti)
{
	const JunctionGhostTile *ghost = JunctionGhost::Get(ti->tile);
	bool pinned = JunctionGhost::IsInPinnedFootprint(ti->tile);
	if (ghost == nullptr && !pinned) return;

	if (ghost != nullptr && ti->tileh != SLOPE_FLAT) {
		DrawTileSelectionRect(ti, PALETTE_SEL_TILE_RED);
	} else if (ghost != nullptr && ghost->bad) {
		DrawTileSelectionRect(ti, _junction_ghost_pinned ? PALETTE_SEL_TILE_BLUE : PALETTE_SEL_TILE_RED);
	} else if (pinned) {
		DrawTileSelectionRect(ti, PAL_NONE);
	}
}

/**
 * Reason shown when a junction can't be placed. Errors come per tile, so instead of the first tile's
 * error a generic message points at the red tiles; only "too close to the edge" applies to the whole junction.
 * @param estimate Estimate with at least one bad tile.
 * @return Message for the tooltip, the cost line and the Shift-click estimate.
 */
static StringID GetPlacementError(const JunctionEstimate &estimate)
{
	if (std::ranges::find(estimate.bad_tiles, INVALID_TILE) != estimate.bad_tiles.end()) return STR_ERROR_TOO_CLOSE_TO_EDGE_OF_MAP;
	return STR_JUNCTION_ERROR_TILES_BLOCKED;
}

static constexpr uint MAX_NOTATION_LENGTH = 16; ///< Maximum number of characters of a line notation.

/** Hotkeys of the junction window. */
enum JunctionHotkeys : int {
	JHK_ROTATE, ///< Rotate the junction.
	JHK_MIRROR, ///< Mirror the junction.
};

static uint8_t _junction_rotation = 0; ///< Quarter turns applied to the junction being placed.
static bool _junction_mirrored = false; ///< Whether the junction being placed is mirrored.

/** What a generated junction depends on: the spot, the orientation and the generator inputs. */
struct JunctionGenerateKey {
	TileIndex anchor; ///< The hovered tile, where the hub goes.
	uint8_t rotation; ///< Quarter turns.
	bool mirrored; ///< Effective mirror, including the road side.
	uint8_t ml_gap;
	uint8_t branch_gap;
	uint8_t tl;
	uint8_t signal_type;
	uint8_t sd;

	bool operator==(const JunctionGenerateKey &) const = default;
};

/** A generation running on a worker thread; owns everything the worker reads, and outlives the window if needed. */
struct JunctionGenerateJob {
	std::atomic<bool> done{false}; ///< Set by the worker when result is final.
	JunctionSpec spec; ///< Spec the worker reads.
	JunctionTerrain terrain; ///< Terrain snapshot the worker reads.
	JunctionNearResult result; ///< Written by the worker.
	std::atomic<bool> cancel{false}; ///< Set when the job is detached; the worker stops searching.
	std::shared_ptr<const std::vector<JunctionLibraryEntry>> library; ///< Library for the spec, or null if not built yet.
};

/** A library build running on a worker thread, or finished; owns everything the worker reads, and outlives the window if needed. */
struct JunctionLibraryJob {
	std::atomic<bool> done{false}; ///< Set by the worker when library is final.
	JunctionSpec spec; ///< Spec the worker reads.
	std::shared_ptr<const std::vector<JunctionLibraryEntry>> library; ///< Written by the worker.
};

static constexpr size_t MAX_JUNCTION_CACHE = 32; ///< Entries in the window's cache of generated junctions.
static constexpr std::chrono::milliseconds JUNCTION_HOVER_REST{300}; ///< How long the hover must rest before generating.

/** Window for choosing the orientation of the junction and showing its ghost. */
struct BuildJunctionWindow : public PickerWindowBase {
	TileIndex hover_tile = INVALID_TILE; ///< Tile the cursor rests on.
	std::chrono::steady_clock::time_point hover_since; ///< When the cursor reached hover_tile.
	std::vector<std::pair<JunctionGenerateKey, JunctionNearResult>> cache; ///< Generated junctions, most recently used first.
	std::optional<JunctionGenerateKey> pending_key; ///< Key of the running job, if any.
	std::thread pending_thread; ///< Thread of the running job.
	std::shared_ptr<JunctionGenerateJob> pending_job; ///< State of the running job.
	std::shared_ptr<JunctionLibraryJob> library_job; ///< Library for the current spec, or being built.
	std::thread library_thread; ///< Thread of the library build.
	std::optional<JunctionGenerateKey> library_key; ///< MakeKey of the library, with anchor INVALID_TILE and rotation 0, mirrored false.
	bool has_shown = false; ///< The ghost shows a placement, in the three fields below.
	JunctionLayout shown_layout; ///< Oriented layout shown at the cursor.
	TileIndex shown_origin = INVALID_TILE; ///< North corner of the shown placement.
	TileIndex shown_tile = INVALID_TILE; ///< Hovered tile the shown placement belongs to.
	int shown_distance = 0; ///< Chebyshev distance in tiles from the hovered tile to the shown placement's anchor.
	StringID hover_status = INVALID_STRING_ID; ///< Status of the hovered spot while there is no placement.
	uint8_t ml_gap = 0; ///< Gap of the mainline notation.
	uint8_t branch_gap = 0; ///< Gap of the branch notation.
	RailType railtype; ///< Rail type of the toolbar that opened this window.
	JunctionLayout build_layout; ///< Layout being built, as it was when the build started.
	TileIndex build_origin = INVALID_TILE; ///< North corner of the junction being built.
	int build_height = 0; ///< Ground height of the junction being built, in height levels.
	std::vector<std::optional<BridgeType>> build_bridge_types; ///< Bridge types chosen for the junction being built, per layout bridge.
	JunctionBuildState last_state = JunctionBuildState::Idle; ///< Sequencer state at the previous tick, to detect a failure.
	Money placement_cost = 0; ///< Cost of the junction at the cursor.
	bool placement_affordable = false; ///< Whether the local company can pay for the junction at the cursor.
	StringID placement_error = INVALID_STRING_ID; ///< First error if the placement is invalid, else INVALID_STRING_ID.
	bool has_placement = false; ///< The cursor is over the map and the fields above are set.
	StringID generate_error = INVALID_STRING_ID; ///< Why the notations can't be generated.
	QueryString ml_editbox; ///< Editbox of the mainline notation.
	QueryString branch_editbox; ///< Editbox of the branch notation.
	uint8_t tl; ///< Train length in tiles.

	BuildJunctionWindow(WindowDesc &desc, Window *parent, RailType railtype) : PickerWindowBase(desc, parent), railtype(railtype),
		ml_editbox(MAX_NOTATION_LENGTH * MAX_CHAR_LENGTH, MAX_NOTATION_LENGTH), branch_editbox(MAX_NOTATION_LENGTH * MAX_CHAR_LENGTH, MAX_NOTATION_LENGTH),
		tl(Clamp<uint8_t>(_settings_client.gui.junction_tl, 2, 12))
	{
		this->querystrings[WID_BJ_ML_TEXT] = &this->ml_editbox;
		this->querystrings[WID_BJ_BRANCH_TEXT] = &this->branch_editbox;
		this->ml_editbox.text.Assign(_settings_client.gui.junction_ml_notation);
		this->branch_editbox.text.Assign(_settings_client.gui.junction_branch_notation);
		this->InitNested(TransportType::Rail);
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
		return {tile, _junction_rotation, EffectiveMirror(), this->ml_gap, this->branch_gap, this->tl,
			static_cast<uint8_t>(_settings_client.gui.junction_signal_type), _settings_client.gui.drag_signals_density};
	}

	/** @return The key of the library: the generator inputs only, as the library is flat and doesn't depend on orientation. */
	JunctionGenerateKey SpecKey() const
	{
		JunctionGenerateKey key = this->MakeKey(INVALID_TILE);
		key.rotation = 0;
		key.mirrored = false;
		return key;
	}

	/** Start building the library if the spec changed, and join the build when it is done. */
	void EnsureLibrary()
	{
		const JunctionGenerateKey key = this->SpecKey();
		if (this->library_key != key) {
			if (this->library_thread.joinable()) this->library_thread.detach();
			this->library_job.reset();
			this->library_key = key;

			auto job = std::make_shared<JunctionLibraryJob>();
			job->spec = MakeJunctionSpec(this->railtype, key.ml_gap, key.branch_gap, key.tl);
			if (!StartNewThread(&this->library_thread, "ottd:junctionlib", [job]() {
				job->library = std::make_shared<const std::vector<JunctionLibraryEntry>>(BuildJunctionLibrary(job->spec));
				job->done = true;
			})) {
				/* No thread: an empty library, so the refine tier alone works. */
				job->library = std::make_shared<const std::vector<JunctionLibraryEntry>>();
				job->done = true;
			}
			this->library_job = std::move(job);
		}

		if (this->library_job != nullptr && this->library_job->done && this->library_thread.joinable()) {
			this->library_thread.join();
			if (this->hover_tile != INVALID_TILE) this->ShowHover();
		}
	}

	/** @return Whether the library for the current spec is built. */
	bool LibraryUsable() const
	{
		return this->library_job != nullptr && this->library_job->done && this->library_key == this->SpecKey();
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
	}

	/** Show no placement at the cursor, with a status for the spot. */
	void ShowNoPlacement(StringID status)
	{
		JunctionGhost::Clear();
		this->has_shown = false;
		this->has_placement = false;
		this->hover_status = status;
		_thd.make_square_red = false;
		SetTileSelectSize(1, 1);
		this->SetWidgetDirty(WID_BJ_COST);
		this->SetWidgetDirty(WID_BJ_STATUS);
	}

	/** Show the placement for the hovered tile: the cached result, or that one is being worked out. */
	void ShowHover()
	{
		const JunctionNearResult *result = this->FindCached(this->MakeKey(this->hover_tile));
		if (result != nullptr) {
			this->ShowResult(*result);
		} else if (!this->LibraryUsable()) {
			this->ShowNoPlacement(STR_JUNCTION_STATUS_GENERATING);
		} else if (this->ml_gap == 0) {
			this->ShowNoPlacement(STR_JUNCTION_ERROR_NEEDS_ML_GAP);
		} else {
			/* The instant tier: fit the library around the cursor. The refine tier may still find something better. */
			const bool mirrored = EffectiveMirror();
			const auto t0 = std::chrono::steady_clock::now();
			const JunctionTerrain terrain = MakeJunctionTerrain(this->hover_tile, _junction_rotation, mirrored, 4);
			const auto t1 = std::chrono::steady_clock::now();
			const JunctionNearResult fit = FitJunctionLibraryNear(*this->library_job->library, terrain, _junction_rotation, mirrored);
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
			this->ShowNoPlacement(result.error == JunctionGenerateError::NeedsMlGap ? STR_JUNCTION_ERROR_NEEDS_ML_GAP : STR_JUNCTION_ERROR_NO_LAYOUT_HERE);
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

		JunctionEstimate estimate;
		std::vector<std::optional<BridgeType>> bridge_types;
		Money available;
		this->ComputePlacement(layout, origin, estimate, bridge_types, available);
		SetTileSelectBigSize(near_result.offset.x - offset.x, near_result.offset.y - offset.y, layout.size.x, layout.size.y);

		JunctionGhost::Set(layout, origin, estimate.height, this->railtype, bridge_types, estimate);
		/* Blocked tiles are outlined individually; the whole outline goes red only when it is just too expensive. */
		_thd.make_square_red = !estimate.affordable && estimate.bad_tiles.empty();

		this->has_shown = true;
		this->shown_layout = std::move(layout);
		this->shown_origin = origin;
		this->shown_tile = this->hover_tile;
		this->shown_distance = std::max(std::abs(near_result.offset.x), std::abs(near_result.offset.y));
		this->hover_status = INVALID_STRING_ID;
		this->has_placement = true;
		this->placement_cost = estimate.total;
		this->placement_affordable = estimate.affordable;
		this->placement_error = estimate.bad_tiles.empty() ? INVALID_STRING_ID : GetPlacementError(estimate);
		this->SetWidgetDirty(WID_BJ_COST);
		this->SetWidgetDirty(WID_BJ_STATUS);

		EncodedString text;
		if (!estimate.bad_tiles.empty()) {
			text = GetEncodedString(STR_JUNCTION_TOOLTIP_INVALID, GetPlacementError(estimate));
		} else if (!estimate.affordable) {
			text = GetEncodedString(STR_JUNCTION_TOOLTIP_UNAFFORDABLE, estimate.total, available);
		} else {
			text = GetEncodedString(STR_JUNCTION_TOOLTIP_COST, estimate.total);
		}
		GuiShowTooltips(this, std::move(text), TooltipCloseCondition::ExitViewport);
	}

	/** Start a job for the hovered tile on the main thread's snapshot, unless one for it is running. */
	void StartJob()
	{
		const JunctionGenerateKey key = this->MakeKey(this->hover_tile);
		if (this->pending_key.has_value() && *this->pending_key == key) return;
		/* A job for another spot may still run; it finishes on its own and its result is ignored. */
		this->DetachJob();

		auto job = std::make_shared<JunctionGenerateJob>();
		job->spec = MakeJunctionSpec(this->railtype, key.ml_gap, key.branch_gap, key.tl);
		job->terrain = MakeJunctionTerrain(key.anchor, key.rotation, key.mirrored, 4);
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
						return std::tuple<Money, int, int>{r.result.score.first, r.result.score.second, r.result.layout->size.x * r.result.layout->size.y};
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

	/** Store the running job's result when it is done, and show it if it is for the hovered spot. */
	void PollJob()
	{
		if (this->pending_job == nullptr || !this->pending_job->done) return;
		if (this->pending_thread.joinable()) this->pending_thread.join();
		const JunctionGenerateKey key = *this->pending_key;
		this->StoreCached(key, std::move(this->pending_job->result));
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
		const JunctionNotation ml = ParseJunctionNotation(ml_text);
		const JunctionNotation branch = ParseJunctionNotation(branch_text);

		if (ml.error == JunctionNotationError::Invalid || branch.error == JunctionNotationError::Invalid) {
			this->generate_error = STR_JUNCTION_ERROR_NOTATION;
		} else if (ml.error == JunctionNotationError::MultiLane || branch.error == JunctionNotationError::MultiLane) {
			this->generate_error = STR_JUNCTION_ERROR_MULTILANE;
		} else {
			this->ml_gap = ml.gap;
			this->branch_gap = branch.gap;
		}

		_settings_client.gui.junction_ml_notation = ml_text;
		_settings_client.gui.junction_branch_notation = branch_text;
		_settings_client.gui.junction_tl = this->tl;

		this->cache.clear();
		this->ResetHover();
		this->EnsureLibrary();
		this->SetWidgetDisabledState(WID_BJ_TL_DECREASE, this->tl <= 2);
		this->SetWidgetDisabledState(WID_BJ_TL_INCREASE, this->tl >= 12);
		this->SetDirty();
	}

	void Close(int data = 0) override
	{
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

			case WID_BJ_TL_DECREASE:
				if (this->tl > 2) {
					this->tl--;
					this->Regenerate();
				}
				break;

			case WID_BJ_TL_INCREASE:
				if (this->tl < 12) {
					this->tl++;
					this->Regenerate();
				}
				break;

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
		}
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
		if (widget == WID_BJ_NAME) {
			return GetString(STR_JUNCTION_NAME, this->ml_editbox.text.GetText(), this->branch_editbox.text.GetText(), this->tl);
		}
		if (widget == WID_BJ_TL_LABEL) return GetString(STR_JUNCTION_TRAIN_LENGTH, this->tl);
		if (widget == WID_BJ_STATUS) {
			const JunctionSequencer &seq = GetJunctionSequencer();
			if (seq.GetState() == JunctionBuildState::Idle) {
				if (this->generate_error != INVALID_STRING_ID) return GetString(this->generate_error);
				if (this->hover_status != INVALID_STRING_ID) return GetString(this->hover_status);
				if (this->has_shown && this->shown_distance != 0) return GetString(STR_JUNCTION_STATUS_NEARBY, this->shown_distance);
			}
			switch (seq.GetState()) {
				case JunctionBuildState::Building: return GetString(STR_JUNCTION_STATUS_BUILDING, seq.GetDone(), seq.GetTotal());
				case JunctionBuildState::Failed: return GetString(STR_JUNCTION_STATUS_FAILED, seq.GetDone(), seq.GetTotal());
				default: return GetString(STR_JUNCTION_STATUS_READY);
			}
		}
		if (widget == WID_BJ_COST) {
			if (!this->has_placement) return GetString(STR_EMPTY);
			if (this->placement_error != INVALID_STRING_ID) return GetString(STR_JUNCTION_COST_INVALID, this->placement_error);
			if (!this->placement_affordable) return GetString(STR_JUNCTION_COST_UNAFFORDABLE, this->placement_cost);
			return GetString(STR_JUNCTION_COST, this->placement_cost);
		}
		return this->PickerWindowBase::GetWidgetString(widget, stringid);
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		if (widget == WID_BJ_COST) {
			/* Reserve room for late-game amounts of up to 15 digits. */
			size.width = std::max(size.width, std::max(
				GetStringBoundingBox(GetString(STR_JUNCTION_COST, GetParamMaxDigits(15))).width,
				GetStringBoundingBox(GetString(STR_JUNCTION_COST_UNAFFORDABLE, GetParamMaxDigits(15))).width) + padding.width);
		}
	}

	/**
	 * Work out the estimate and bridge types for placing an oriented junction.
	 * @param layout The oriented layout.
	 * @param origin North corner of the junction.
	 * @param[out] estimate The estimate of building it.
	 * @param[out] bridge_types Bridge type chosen per layout bridge.
	 * @param[out] available Money the local company can spend.
	 */
	void ComputePlacement(const JunctionLayout &layout, TileIndex origin, JunctionEstimate &estimate, std::vector<std::optional<BridgeType>> &bridge_types, Money &available) const
	{
		GameJunctionPricing pricing(this->railtype);
		estimate = EstimateJunction(layout, origin, pricing);
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
		this->ComputePlacement(layout, origin, estimate, bridge_types, available);

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
		this->build_bridge_types = bridge_types;
		this->last_state = JunctionBuildState::Building;
		GetJunctionSequencer().Start(MakeJunctionBuildSteps(layout, origin));
		JunctionGhost::SetPinned(true);
		/* Shrink the cursor box to one tile while the pinned footprint shows the build; 0x0 trips an assert in SetSelectionTilesDirty. */
		SetTileSelectSize(1, 1);
	}

	void OnRealtimeTick([[maybe_unused]] uint delta_ms) override
	{
		GameJunctionStepPoster poster(this->railtype);
		JunctionSequencer &seq = GetJunctionSequencer();
		seq.Tick(poster);

		if (seq.GetState() == JunctionBuildState::Done) {
			if (_settings_client.sound.confirm) SndPlayTileFx(SND_20_CONSTRUCTION_RAIL, this->build_origin);
			seq.Cancel();
			/* The map changed, so earlier results no longer hold. */
			this->cache.clear();
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
					} else {
						estimate.bad_tiles.push_back(std::get<JunctionSignalStep>(step).tile);
					}
				}
				JunctionGhost::Set(this->build_layout, this->build_origin, this->build_height, this->railtype, this->build_bridge_types, estimate);
				/* Set() clears, which unpins; the footprint stays pinned while the build is paused. */
				JunctionGhost::SetPinned(true);
			}
			this->SetWidgetDisabledState(WID_BJ_RETRY, state != JunctionBuildState::Failed);
			this->SetWidgetDisabledState(WID_BJ_CANCEL, false);
			this->SetWidgetDirty(WID_BJ_STATUS);
			this->SetWidgetDirty(WID_BJ_RETRY);
			this->SetWidgetDirty(WID_BJ_CANCEL);
			return;
		}

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
				this->SetWidgetDirty(WID_BJ_COST);
			}
			SetTileSelectSize(1, 1);
			this->SetWidgetDirty(WID_BJ_STATUS);
			return;
		}

		Point pt = GetTileBelowCursor();
		if (pt.x == -1) {
			JunctionGhost::Clear();
			this->ResetHover();
			this->has_placement = false;
			this->SetWidgetDirty(WID_BJ_COST);
			this->SetWidgetDirty(WID_BJ_STATUS);
			return;
		}

		const auto now = std::chrono::steady_clock::now();
		TileIndex tile = TileVirtXY(pt.x, pt.y);
		if (tile != this->hover_tile) {
			this->hover_tile = tile;
			this->hover_since = now;
			this->ShowHover();
		}

		if (now - this->hover_since >= JUNCTION_HOVER_REST) {
			const JunctionGenerateKey key = this->MakeKey(tile);
			if (std::ranges::find(this->cache, key, &std::pair<JunctionGenerateKey, JunctionNearResult>::first) == this->cache.end()) this->StartJob();
		}
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
			NWidget(WWT_LABEL, Colours::Invalid, WID_BJ_NAME), SetFill(1, 0),
			NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
				NWidget(WWT_TEXT, Colours::Invalid, WID_BJ_ML_LABEL), SetStringTip(STR_JUNCTION_MAINLINE_LABEL, STR_JUNCTION_MAINLINE_TOOLTIP),
				NWidget(WWT_EDITBOX, Colours::Grey, WID_BJ_ML_TEXT), SetToolTip(STR_JUNCTION_MAINLINE_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
			EndContainer(),
			NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
				NWidget(WWT_TEXT, Colours::Invalid, WID_BJ_BRANCH_LABEL), SetStringTip(STR_JUNCTION_BRANCH_LABEL, STR_JUNCTION_BRANCH_TOOLTIP),
				NWidget(WWT_EDITBOX, Colours::Grey, WID_BJ_BRANCH_TEXT), SetToolTip(STR_JUNCTION_BRANCH_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
			EndContainer(),
			NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
				NWidget(WWT_PUSHARROWBTN, Colours::Grey, WID_BJ_TL_DECREASE), SetMinimalSize(9, 12), SetArrowWidgetTypeTip(ArrowWidgetType::Decrease, STR_JUNCTION_TRAIN_LENGTH_DECREASE_TOOLTIP),
				NWidget(WWT_LABEL, Colours::Invalid, WID_BJ_TL_LABEL), SetFill(1, 0),
				NWidget(WWT_PUSHARROWBTN, Colours::Grey, WID_BJ_TL_INCREASE), SetMinimalSize(9, 12), SetArrowWidgetTypeTip(ArrowWidgetType::Increase, STR_JUNCTION_TRAIN_LENGTH_INCREASE_TOOLTIP),
			EndContainer(),
			NWidget(WWT_LABEL, Colours::Invalid, WID_BJ_COST), SetFill(1, 0),
			NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
				NWidget(WWT_PUSHTXTBTN, Colours::Grey, WID_BJ_ROTATE), SetStringTip(STR_JUNCTION_ROTATE, STR_JUNCTION_ROTATE_TOOLTIP), SetFill(1, 0),
				NWidget(WWT_PUSHTXTBTN, Colours::Grey, WID_BJ_MIRROR), SetStringTip(STR_JUNCTION_MIRROR, STR_JUNCTION_MIRROR_TOOLTIP), SetFill(1, 0),
			EndContainer(),
			NWidget(WWT_LABEL, Colours::Invalid, WID_BJ_STATUS), SetFill(1, 0),
			NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
				NWidget(WWT_PUSHTXTBTN, Colours::Grey, WID_BJ_RETRY), SetStringTip(STR_JUNCTION_RETRY, STR_JUNCTION_RETRY_TOOLTIP), SetFill(1, 0),
				NWidget(WWT_PUSHTXTBTN, Colours::Grey, WID_BJ_CANCEL), SetStringTip(STR_JUNCTION_CANCEL, STR_JUNCTION_CANCEL_TOOLTIP), SetFill(1, 0),
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
