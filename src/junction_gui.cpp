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
#include "widgets/junction_widget.h"
#include "map_func.h"
#include "slope_func.h"
#include "tile_cmd.h"
#include "viewport_func.h"

#include "table/sprites.h"
#include "table/strings.h"

#include <unordered_map>

#include "safeguards.h"

/** The ghost tiles, by tile. */
static std::unordered_map<TileIndex, JunctionGhostTile> _junction_ghost_tiles;
/** Rail type of the ghost. */
static RailType _junction_ghost_railtype = RAILTYPE_RAIL;

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
 * Draw the junction ghost on one tile, if it has any.
 * Valid ghosts are drawn opaque in normal colours (D44); invalid ones are remapped to red.
 * @param ti Tile that is being drawn.
 */
void DrawJunctionGhost(const TileInfo *ti)
{
	const JunctionGhostTile *ghost = JunctionGhost::Get(ti->tile);
	if (ghost == nullptr) return;

	/* Ghost tiles that are not flat can't be built on; outline them in red. */
	if (ti->tileh != SLOPE_FLAT) {
		SetViewportGhostPalette(PALETTE_SEL_TILE_RED);
		DrawTileSelectionRect(ti, PALETTE_SEL_TILE_RED);
		SetViewportGhostPalette(PAL_NONE);
		return;
	}

	SetViewportGhostPalette(ghost->bad ? PALETTE_TO_STRUCT_RED : PAL_NONE);
	TileInfo copy = *ti;
	RailType railtype = JunctionGhost::GetRailType();
	if (ghost->tracks.Any()) DrawRailTrackGhost(&copy, ghost->tracks, railtype);
	if (ghost->head.has_value()) DrawBridgeHeadGhost(&copy, ghost->head->type, railtype, ghost->head->dir);
	if (ghost->span.has_value()) DrawBridgeMiddleGhost(ti, ghost->span->type, railtype, ghost->span->axis, ghost->span->bridge_z, ghost->span->piece_north, ghost->span->piece_south);
	for (const JunctionSignalStep &signal : ghost->signals) {
		DrawRailSignalGhost(ti->tile, railtype, signal.facing, signal.type, signal.variant);
	}
	SetViewportGhostPalette(PAL_NONE);
}

/** Hotkeys of the junction window. */
enum JunctionHotkeys : int {
	JHK_ROTATE, ///< Rotate the junction.
	JHK_MIRROR, ///< Mirror the junction.
};

static uint8_t _junction_rotation = 0; ///< Quarter turns applied to the junction being placed.
static bool _junction_mirrored = false; ///< Whether the junction being placed is mirrored.

/**
 * Get the layout being placed, with the current orientation applied.
 * @return The oriented layout.
 */
static JunctionLayout GetPlacementLayout()
{
	return ApplyJunctionOrientation(MakeTestJunctionLayout(), _junction_rotation, _junction_mirrored);
}

/** Window for choosing the orientation of the junction and showing its ghost. */
struct BuildJunctionWindow : public PickerWindowBase {
	TileIndex last_tile; ///< Tile the ghost was last placed at.
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

	BuildJunctionWindow(WindowDesc &desc, Window *parent, RailType railtype) : PickerWindowBase(desc, parent), railtype(railtype)
	{
		this->InitNested(TransportType::Rail);
		this->last_tile = INVALID_TILE;
	}

	void Close(int data = 0) override
	{
		GetJunctionSequencer().Cancel();
		JunctionGhost::Clear();
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
				this->last_tile = INVALID_TILE;
				this->SetDirty();
				break;

			case WID_BJ_ROTATE:
				_junction_rotation = (_junction_rotation + 1) % 4;
				this->last_tile = INVALID_TILE;
				SndClickBeep();
				break;

			case WID_BJ_MIRROR:
				_junction_mirrored = !_junction_mirrored;
				this->last_tile = INVALID_TILE;
				SndClickBeep();
				break;
		}
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
		if (widget == WID_BJ_STATUS) {
			const JunctionSequencer &seq = GetJunctionSequencer();
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
	 * Work out the layout, estimate and bridge types for placing the junction at a tile.
	 * @param tile North corner of the junction.
	 * @param[out] layout The oriented layout.
	 * @param[out] estimate The estimate of building it.
	 * @param[out] bridge_types Bridge type chosen per layout bridge.
	 * @param[out] available Money the local company can spend.
	 */
	void ComputePlacement(TileIndex tile, JunctionLayout &layout, JunctionEstimate &estimate, std::vector<std::optional<BridgeType>> &bridge_types, Money &available) const
	{
		layout = GetPlacementLayout();

		GameJunctionPricing pricing(this->railtype);
		estimate = EstimateJunction(layout, tile, pricing);
		available = pricing.AvailableMoney();

		bridge_types.clear();
		for (const JunctionBridge &bridge : layout.bridges) {
			uint length = abs(bridge.end.x - bridge.start.x) + abs(bridge.end.y - bridge.start.y) - 1;
			bridge_types.push_back(ChooseJunctionBridgeType(this->railtype, length));
		}
	}

	/**
	 * Start building the junction at a tile, if it can be built and paid for.
	 * @param tile North corner of the junction.
	 */
	void StartBuild(TileIndex tile)
	{
		JunctionLayout layout;
		JunctionEstimate estimate;
		std::vector<std::optional<BridgeType>> bridge_types;
		Money available;
		this->ComputePlacement(tile, layout, estimate, bridge_types, available);

		/* Shift-click estimates the cost instead of building, as with every other build tool. */
		if (IsShiftPressed()) {
			if (estimate.bad_tiles.empty()) {
				ShowEstimatedCostOrIncome(estimate.total, TileX(tile) * TILE_SIZE, TileY(tile) * TILE_SIZE);
			} else {
				CommandCost cc(estimate.first_error);
				ShowErrorMessage(GetEncodedString(STR_ERROR_CAN_T_BUILD_RAILROAD_TRACK), TileX(tile) * TILE_SIZE, TileY(tile) * TILE_SIZE, cc);
			}
			return;
		}

		if (!estimate.affordable) return;

		this->build_layout = layout;
		this->build_origin = tile;
		this->build_height = estimate.height;
		this->build_bridge_types = bridge_types;
		this->last_state = JunctionBuildState::Building;
		GetJunctionSequencer().Start(MakeJunctionBuildSteps(layout, tile));
	}

	void OnRealtimeTick([[maybe_unused]] uint delta_ms) override
	{
		GameJunctionStepPoster poster(this->railtype);
		JunctionSequencer &seq = GetJunctionSequencer();
		seq.Tick(poster);

		if (seq.GetState() == JunctionBuildState::Done) {
			if (_settings_client.sound.confirm) SndPlayTileFx(SND_20_CONSTRUCTION_RAIL, this->build_origin);
			seq.Cancel();
			this->last_tile = INVALID_TILE;
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

		Point pt = GetTileBelowCursor();
		if (pt.x == -1) {
			JunctionGhost::Clear();
			this->last_tile = INVALID_TILE;
			this->has_placement = false;
			this->SetWidgetDirty(WID_BJ_COST);
			return;
		}

		TileIndex tile = TileVirtXY(pt.x, pt.y);
		if (tile == this->last_tile) return;
		this->last_tile = tile;

		JunctionLayout layout;
		JunctionEstimate estimate;
		std::vector<std::optional<BridgeType>> bridge_types;
		Money available;
		this->ComputePlacement(tile, layout, estimate, bridge_types, available);
		SetTileSelectSize(layout.size.x, layout.size.y);

		JunctionGhost::Set(layout, tile, estimate.height, this->railtype, bridge_types, estimate);
		_thd.make_square_red = !estimate.affordable;

		this->has_placement = true;
		this->placement_cost = estimate.total;
		this->placement_affordable = estimate.affordable;
		this->placement_error = estimate.bad_tiles.empty() ? INVALID_STRING_ID : estimate.first_error;
		this->SetWidgetDirty(WID_BJ_COST);

		EncodedString text;
		if (!estimate.bad_tiles.empty()) {
			text = GetEncodedString(STR_JUNCTION_TOOLTIP_INVALID, estimate.first_error);
		} else if (!estimate.affordable) {
			text = GetEncodedString(STR_JUNCTION_TOOLTIP_UNAFFORDABLE, estimate.total, available);
		} else {
			text = GetEncodedString(STR_JUNCTION_TOOLTIP_COST, estimate.total);
		}
		GuiShowTooltips(this, std::move(text), TooltipCloseCondition::ExitViewport);
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
			NWidget(WWT_LABEL, Colours::Invalid, WID_BJ_NAME), SetStringTip(STR_JUNCTION_NAME_TEST), SetFill(1, 0),
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
