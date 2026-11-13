/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_station_gui.cpp The station generator window: ghost, cost and build (D106). */

#include "../../stdafx.h"

#include "../tool/junction_gui.h"
#include "../game/junction_game.h"
#include "junction_station_generator.h"
#include "../../window_gui.h"
#include "../../gui.h"
#include "../../error.h"
#include "../../gfx_func.h"
#include "../../window_func.h"
#include "../../picker_gui.h"
#include "../../hotkeys.h"
#include "../../sound_func.h"
#include "../../sound_type.h"
#include "../../settings_type.h"
#include "../../strings_func.h"
#include "../../tilehighlight_func.h"
#include "../../tilehighlight_type.h"
#include "../../vehicle_base.h"
#include "../../widgets/junction_station_widget.h"
#include "../../map_func.h"
#include "../../viewport_func.h"

#include "../../table/sprites.h"
#include "../../table/strings.h"

#include "../../safeguards.h"

/** Hotkeys of the junction station window. */
enum JunctionStationHotkeys : int {
	JSHK_ROTATE, ///< Rotate the station.
	JSHK_MIRROR, ///< Mirror the station.
};

static bool _junction_station_picking_train = false; ///< The junction station window has the vehicle cursor, to pick a train.

/** @return Whether the junction station window is picking a train, so the rail toolbar must keep the station tool when it loses the cursor. */
bool IsJunctionStationTrainPickActive()
{
	return _junction_station_picking_train;
}

static uint8_t _junction_station_rotation = 0; ///< Quarter turns applied to the station being placed.
static bool _junction_station_mirrored = false; ///< Whether the station being placed is mirrored.

/**
 * Reason shown when a station can't be placed. Errors come per tile, so a generic message points at the red tiles;
 * only "too close to the edge" applies to the whole station.
 * @param estimate Estimate with at least one bad tile.
 * @return Message for the tooltip, the cost line and the Shift-click estimate.
 */
static StringID GetStationPlacementError(const JunctionEstimate &estimate)
{
	if (std::ranges::find(estimate.bad_tiles, INVALID_TILE) != estimate.bad_tiles.end()) return STR_ERROR_TOO_CLOSE_TO_EDGE_OF_MAP;
	return STR_JUNCTION_ERROR_TILES_BLOCKED;
}

/** Window for choosing the orientation of the station and showing its ghost. */
struct BuildJunctionStationWindow : public PickerWindowBase {
	TileIndex hover_tile = INVALID_TILE; ///< Tile the cursor rests on.
	bool has_shown = false; ///< The ghost shows a placement, in the three fields below.
	JunctionLayout shown_layout; ///< Oriented layout shown at the cursor.
	TileIndex shown_origin = INVALID_TILE; ///< North corner of the shown placement.
	TileIndex shown_tile = INVALID_TILE; ///< Hovered tile the shown placement belongs to.
	StringID hover_status = INVALID_STRING_ID; ///< Status of the hovered spot while there is no placement.
	RailType railtype; ///< Rail type of the toolbar that opened this window.
	JunctionLayout build_layout; ///< Layout being built, as it was when the build started.
	TileIndex build_origin = INVALID_TILE; ///< North corner of the station being built.
	int build_height = 0; ///< Ground height of the station being built, in height levels.
	JunctionBuildState last_state = JunctionBuildState::Idle; ///< Sequencer state at the previous tick, to detect a failure.
	Money placement_cost = 0; ///< Cost of the station at the cursor.
	bool placement_affordable = false; ///< Whether the local company can pay for the station at the cursor.
	StringID placement_error = INVALID_STRING_ID; ///< First error if the placement is invalid, else INVALID_STRING_ID.
	bool has_placement = false; ///< The cursor is over the map and the fields above are set.
	StringID generate_error = INVALID_STRING_ID; ///< Why no station could be generated.
	JunctionStationResult station; ///< The generated station for the current settings, in its normalised frame.
	uint8_t tl; ///< Train length in tiles; also the platform length.
	uint8_t platforms; ///< Number of platforms.
	std::optional<JunctionPickedTrain> picked_train; ///< The picked train, if the train length comes from one.
	StringID pick_error = INVALID_STRING_ID; ///< Error of the last pick, shown in the status line.

	BuildJunctionStationWindow(WindowDesc &desc, Window *parent, RailType railtype) : PickerWindowBase(desc, parent), railtype(railtype),
		tl(Clamp<uint8_t>(_settings_client.gui.junction_tl, 2, 12)),
		platforms(Clamp<uint8_t>(_settings_client.gui.junction_station_platforms, 2, 8))
	{
		this->InitNested(TransportType::Rail);
		this->Regenerate();
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
		this->SetWidgetDirty(WID_BJS_COST);
		this->SetWidgetDirty(WID_BJS_STATUS);
	}

	/**
	 * Work out the estimate for placing an oriented station.
	 * @param layout The oriented layout.
	 * @param origin North corner of the station.
	 * @param[out] estimate The estimate of building it.
	 * @param[out] available Money the local company can spend.
	 */
	void ComputePlacement(const JunctionLayout &layout, TileIndex origin, JunctionEstimate &estimate, Money &available) const
	{
		GameJunctionPricing pricing(this->railtype);
		estimate = EstimateJunction(layout, origin, pricing);
		available = pricing.AvailableMoney();
	}

	/** Show the station for the hovered tile, with its ghost, cost and tooltip. The in-port tile of the station goes under the cursor. */
	void ShowHover()
	{
		if (!this->station.layout.has_value()) {
			this->ShowNoPlacement(STR_JUNCTION_STATION_ERROR_NO_LAYOUT);
			return;
		}

		const JunctionLayout &base = *this->station.layout;
		const JunctionLayout layout = ApplyJunctionOrientation(base, _junction_station_rotation, _junction_station_mirrored);
		const TileIndexDiffC offset = OrientJunctionOffset(this->station.ports.in.tile, base.size, _junction_station_rotation, _junction_station_mirrored);
		const int ox = static_cast<int>(TileX(this->hover_tile)) - offset.x;
		const int oy = static_cast<int>(TileY(this->hover_tile)) - offset.y;
		if (ox < 0 || oy < 0 || ox >= static_cast<int>(Map::SizeX()) || oy >= static_cast<int>(Map::SizeY())) {
			this->ShowNoPlacement(STR_ERROR_TOO_CLOSE_TO_EDGE_OF_MAP);
			return;
		}
		const TileIndex origin = TileXY(ox, oy);

		JunctionEstimate estimate;
		Money available;
		this->ComputePlacement(layout, origin, estimate, available);
		/* The ghost outlines its own footprint; a big tile selection would add a blue outer row. */
		SetTileSelectSize(1, 1);

		JunctionGhost::Set(layout, origin, estimate.height, this->railtype, {}, estimate);
		_thd.make_square_red = false;

		this->has_shown = true;
		this->shown_layout = layout;
		this->shown_origin = origin;
		this->shown_tile = this->hover_tile;
		this->hover_status = INVALID_STRING_ID;
		this->has_placement = true;
		this->placement_cost = estimate.total;
		this->placement_affordable = estimate.affordable;
		this->placement_error = estimate.bad_tiles.empty() ? INVALID_STRING_ID : GetStationPlacementError(estimate);
		this->SetWidgetDirty(WID_BJS_COST);
		this->SetWidgetDirty(WID_BJS_STATUS);

		EncodedString text;
		if (!estimate.bad_tiles.empty()) {
			text = GetEncodedString(STR_JUNCTION_TOOLTIP_INVALID, GetStationPlacementError(estimate));
		} else if (!estimate.affordable) {
			text = GetEncodedString(STR_JUNCTION_TOOLTIP_UNAFFORDABLE, estimate.total, available);
		} else {
			text = GetEncodedString(STR_JUNCTION_TOOLTIP_COST, estimate.total);
		}
		GuiShowTooltips(this, std::move(text), TooltipCloseCondition::ExitViewport);
	}

	/** Remember the settings, and generate the station for them. Generation is fast, so it runs here on spec change. */
	void Regenerate()
	{
		_settings_client.gui.junction_tl = this->tl;
		_settings_client.gui.junction_station_platforms = this->platforms;

		/* Stations do not size a prio (D137): the picked train is passed on when there is one. */
		this->station = GenerateStation(MakeJunctionSpec(this->railtype, 1, 1, this->tl, this->picked_train.has_value() ? this->picked_train->loaded : JunctionTrainParams{}), this->platforms);
		this->generate_error = this->station.layout.has_value() ? INVALID_STRING_ID : STR_JUNCTION_STATION_ERROR_NO_LAYOUT;

		this->ResetHover();
		this->SetWidgetDisabledState(WID_BJS_PLATFORMS_DECREASE, this->platforms <= 2);
		this->SetWidgetDisabledState(WID_BJS_PLATFORMS_INCREASE, this->platforms >= 8);
		this->SetWidgetDisabledState(WID_BJS_TL_DECREASE, this->tl <= 2);
		this->SetWidgetDisabledState(WID_BJS_TL_INCREASE, this->tl >= 12);
		this->SetDirty();
	}

	void Close(int data = 0) override
	{
		_junction_station_picking_train = false;
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
			case WID_BJS_RETRY: {
				/* Pieces already built make their tiles fail an estimate, so only check the money. */
				GameJunctionPricing pricing(this->railtype);
				if (pricing.AvailableMoney() > 0) GetJunctionSequencer().Retry();
				break;
			}

			case WID_BJS_CANCEL:
				GetJunctionSequencer().Cancel();
				JunctionGhost::Clear();
				JunctionGhost::SetPinned(false);
				SetTileSelectSize(1, 1);
				this->ResetHover();
				this->SetDirty();
				break;

			case WID_BJS_PICK_TRAIN:
				this->pick_error = INVALID_STRING_ID;
				_junction_station_picking_train = true;
				SetObjectToPlaceWnd(SPR_CURSOR_CLONE_TRAIN, PAL_NONE, HT_VEHICLE, this);
				this->SetWidgetDirty(WID_BJS_STATUS);
				SndClickBeep();
				break;

			case WID_BJS_PLATFORMS_DECREASE:
				if (this->platforms > 2) {
					this->platforms--;
					this->Regenerate();
				}
				break;

			case WID_BJS_PLATFORMS_INCREASE:
				if (this->platforms < 8) {
					this->platforms++;
					this->Regenerate();
				}
				break;

			case WID_BJS_TL_DECREASE:
				if (this->tl > 2) {
					this->picked_train = std::nullopt;
					this->tl--;
					this->Regenerate();
				}
				break;

			case WID_BJS_TL_INCREASE:
				if (this->tl < 12) {
					this->picked_train = std::nullopt;
					this->tl++;
					this->Regenerate();
				}
				break;

			case WID_BJS_ROTATE:
				_junction_station_rotation = (_junction_station_rotation + 1) % 4;
				this->ResetHover();
				SndClickBeep();
				break;

			case WID_BJS_MIRROR:
				_junction_station_mirrored = !_junction_station_mirrored;
				this->ResetHover();
				SndClickBeep();
				break;
		}
	}

	bool OnVehicleSelect(const Vehicle *v) override
	{
		std::optional<JunctionPickedTrain> picked = ReadJunctionTrain(v);
		if (!picked.has_value()) {
			this->pick_error = STR_JUNCTION_ERROR_NOT_OUR_TRAIN;
			this->SetWidgetDirty(WID_BJS_STATUS);
			return true;
		}

		this->pick_error = INVALID_STRING_ID;
		this->tl = picked->tl;
		this->picked_train = std::move(picked);
		this->Regenerate();
		this->SetWidgetDirty(WID_BJS_TRAIN);
		_junction_station_picking_train = false;
		RestoreJunctionPlacement();
		return true;
	}

	void OnPlaceObjectAbort() override
	{
		if (!_junction_station_picking_train) return;
		_junction_station_picking_train = false;
		RestoreJunctionPlacement();
	}

	EventState OnHotkey(int hotkey) override
	{
		switch (hotkey) {
			case JSHK_ROTATE:
				this->OnClick({}, WID_BJS_ROTATE, 1);
				return EventState::Handled;

			case JSHK_MIRROR:
				this->OnClick({}, WID_BJS_MIRROR, 1);
				return EventState::Handled;

			default:
				return this->PickerWindowBase::OnHotkey(hotkey);
		}
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		if (widget == WID_BJS_TRAIN) {
			if (!this->picked_train.has_value()) return GetString(STR_JUNCTION_TRAIN_NONE);
			return GetString(STR_JUNCTION_STATION_TRAIN, this->picked_train->name, this->picked_train->tl);
		}
		if (widget == WID_BJS_PLATFORMS_LABEL) return GetString(STR_JUNCTION_STATION_PLATFORMS, this->platforms);
		if (widget == WID_BJS_TL_LABEL) return GetString(STR_JUNCTION_TRAIN_LENGTH, this->tl);
		if (widget == WID_BJS_STATUS) {
			const JunctionSequencer &seq = GetJunctionSequencer();
			if (seq.GetState() == JunctionBuildState::Idle) {
				if (this->pick_error != INVALID_STRING_ID && _junction_station_picking_train) return GetString(this->pick_error);
				if (this->generate_error != INVALID_STRING_ID) return GetString(this->generate_error);
				if (this->hover_status != INVALID_STRING_ID) return GetString(this->hover_status);
			}
			switch (seq.GetState()) {
				case JunctionBuildState::Building: return GetString(STR_JUNCTION_STATUS_BUILDING, seq.GetDone(), seq.GetTotal());
				case JunctionBuildState::Failed: return GetString(STR_JUNCTION_STATUS_FAILED, seq.GetDone(), seq.GetTotal());
				default: return GetString(STR_JUNCTION_STATUS_READY);
			}
		}
		if (widget == WID_BJS_COST) {
			if (!this->has_placement) return GetString(STR_EMPTY);
			if (this->placement_error != INVALID_STRING_ID) return GetString(STR_JUNCTION_COST_INVALID, this->placement_error);
			if (!this->placement_affordable) return GetString(STR_JUNCTION_COST_UNAFFORDABLE, this->placement_cost);
			return GetString(STR_JUNCTION_COST, this->placement_cost);
		}
		return this->PickerWindowBase::GetWidgetString(widget, stringid);
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		if (widget == WID_BJS_COST) {
			/* Reserve room for late-game amounts of up to 15 digits. */
			size.width = std::max(size.width, std::max(
				GetStringBoundingBox(GetString(STR_JUNCTION_COST, GetParamMaxDigits(15))).width,
				GetStringBoundingBox(GetString(STR_JUNCTION_COST_UNAFFORDABLE, GetParamMaxDigits(15))).width) + padding.width);
		}
	}

	/**
	 * Start building the station shown at the cursor, if it can be built and paid for.
	 * @param tile The clicked tile; it must be the one the shown placement belongs to.
	 */
	void StartBuild(TileIndex tile)
	{
		if (!this->has_shown || tile != this->shown_tile) return;

		const TileIndex origin = this->shown_origin;
		const JunctionLayout layout = this->shown_layout;
		JunctionEstimate estimate;
		Money available;
		this->ComputePlacement(layout, origin, estimate, available);

		/* Shift-click estimates the cost instead of building, as with every other build tool. */
		if (IsShiftPressed()) {
			if (estimate.bad_tiles.empty()) {
				ShowEstimatedCostOrIncome(estimate.total, TileX(tile) * TILE_SIZE, TileY(tile) * TILE_SIZE);
			} else {
				CommandCost cc(GetStationPlacementError(estimate));
				ShowErrorMessage(GetEncodedString(STR_ERROR_CAN_T_BUILD_RAILROAD_STATION), TileX(tile) * TILE_SIZE, TileY(tile) * TILE_SIZE, cc);
			}
			return;
		}

		if (!estimate.affordable) return;

		this->build_layout = layout;
		this->build_origin = origin;
		this->build_height = estimate.height;
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
					} else if (const auto *station = std::get_if<JunctionStationStep>(&step)) {
						const int w = station->axis == Axis::X ? station->length : station->platforms;
						const int h = station->axis == Axis::X ? station->platforms : station->length;
						for (int y = 0; y < h; y++) {
							for (int x = 0; x < w; x++) estimate.bad_tiles.push_back(AddTileIndexDiffCWrap(station->tile, {static_cast<int16_t>(x), static_cast<int16_t>(y)}));
						}
					} else if (const auto *signal = std::get_if<JunctionSignalStep>(&step)) {
						estimate.bad_tiles.push_back(signal->tile);
					}
				}
				JunctionGhost::Set(this->build_layout, this->build_origin, this->build_height, this->railtype, {}, estimate);
				/* Set() clears, which unpins; the footprint stays pinned while the build is paused. */
				JunctionGhost::SetPinned(true);
			}
			this->SetWidgetDisabledState(WID_BJS_RETRY, state != JunctionBuildState::Failed);
			this->SetWidgetDisabledState(WID_BJS_CANCEL, false);
			this->SetWidgetDirty(WID_BJS_STATUS);
			this->SetWidgetDirty(WID_BJS_RETRY);
			this->SetWidgetDirty(WID_BJS_CANCEL);
			return;
		}

		this->SetWidgetDisabledState(WID_BJS_RETRY, true);
		this->SetWidgetDisabledState(WID_BJS_CANCEL, true);
		this->SetWidgetDirty(WID_BJS_RETRY);
		this->SetWidgetDirty(WID_BJS_CANCEL);

		if (this->generate_error != INVALID_STRING_ID) {
			if (this->has_placement) {
				JunctionGhost::Clear();
				this->has_placement = false;
				this->SetWidgetDirty(WID_BJS_COST);
			}
			SetTileSelectSize(1, 1);
			this->SetWidgetDirty(WID_BJS_STATUS);
			return;
		}

		/* While picking a train there is no placement: no ghost, no hover. */
		if (_junction_station_picking_train) {
			if (this->has_placement || this->has_shown) {
				JunctionGhost::Clear();
				this->ResetHover();
				this->has_placement = false;
				this->SetWidgetDirty(WID_BJS_COST);
			}
			return;
		}

		Point pt = GetTileBelowCursor();
		if (pt.x == -1) {
			JunctionGhost::Clear();
			this->ResetHover();
			this->has_placement = false;
			this->SetWidgetDirty(WID_BJS_COST);
			this->SetWidgetDirty(WID_BJS_STATUS);
			return;
		}

		TileIndex tile = TileVirtXY(pt.x, pt.y);
		if (tile != this->hover_tile) {
			this->hover_tile = tile;
			this->ShowHover();
		}
	}

	static inline HotkeyList hotkeys{"buildjunctionstation", {
		Hotkey(WKC_PERIOD, "rotate", JSHK_ROTATE),
		Hotkey(WKC_COMMA, "mirror", JSHK_MIRROR),
	}};
};

/** Nested widget definition of the build junction station window. */
static constexpr std::initializer_list<NWidgetPart> _nested_build_junction_station_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, Colours::DarkGreen),
		NWidget(WWT_CAPTION, Colours::DarkGreen), SetStringTip(STR_JUNCTION_STATION_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, Colours::DarkGreen),
		NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_normal, 0), SetPadding(WidgetDimensions::unscaled.picker),
			NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
				NWidget(WWT_PUSHARROWBTN, Colours::Grey, WID_BJS_PLATFORMS_DECREASE), SetMinimalSize(9, 12), SetArrowWidgetTypeTip(ArrowWidgetType::Decrease, STR_JUNCTION_STATION_PLATFORMS_DECREASE_TOOLTIP),
				NWidget(WWT_LABEL, Colours::Invalid, WID_BJS_PLATFORMS_LABEL), SetFill(1, 0),
				NWidget(WWT_PUSHARROWBTN, Colours::Grey, WID_BJS_PLATFORMS_INCREASE), SetMinimalSize(9, 12), SetArrowWidgetTypeTip(ArrowWidgetType::Increase, STR_JUNCTION_STATION_PLATFORMS_INCREASE_TOOLTIP),
			EndContainer(),
			NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
				NWidget(WWT_PUSHARROWBTN, Colours::Grey, WID_BJS_TL_DECREASE), SetMinimalSize(9, 12), SetArrowWidgetTypeTip(ArrowWidgetType::Decrease, STR_JUNCTION_STATION_TRAIN_LENGTH_DECREASE_TOOLTIP),
				NWidget(WWT_LABEL, Colours::Invalid, WID_BJS_TL_LABEL), SetFill(1, 0),
				NWidget(WWT_PUSHARROWBTN, Colours::Grey, WID_BJS_TL_INCREASE), SetMinimalSize(9, 12), SetArrowWidgetTypeTip(ArrowWidgetType::Increase, STR_JUNCTION_STATION_TRAIN_LENGTH_INCREASE_TOOLTIP),
			EndContainer(),
			NWidget(WWT_PUSHTXTBTN, Colours::Grey, WID_BJS_PICK_TRAIN), SetStringTip(STR_JUNCTION_PICK_TRAIN, STR_JUNCTION_STATION_PICK_TRAIN_TOOLTIP), SetFill(1, 0),
			NWidget(WWT_LABEL, Colours::Invalid, WID_BJS_TRAIN), SetFill(1, 0),
			NWidget(WWT_LABEL, Colours::Invalid, WID_BJS_COST), SetFill(1, 0),
			NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
				NWidget(WWT_PUSHTXTBTN, Colours::Grey, WID_BJS_ROTATE), SetStringTip(STR_JUNCTION_ROTATE, STR_JUNCTION_STATION_ROTATE_TOOLTIP), SetFill(1, 0),
				NWidget(WWT_PUSHTXTBTN, Colours::Grey, WID_BJS_MIRROR), SetStringTip(STR_JUNCTION_MIRROR, STR_JUNCTION_STATION_MIRROR_TOOLTIP), SetFill(1, 0),
			EndContainer(),
			NWidget(WWT_LABEL, Colours::Invalid, WID_BJS_STATUS), SetFill(1, 0),
			NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
				NWidget(WWT_PUSHTXTBTN, Colours::Grey, WID_BJS_RETRY), SetStringTip(STR_JUNCTION_RETRY, STR_JUNCTION_RETRY_TOOLTIP), SetFill(1, 0),
				NWidget(WWT_PUSHTXTBTN, Colours::Grey, WID_BJS_CANCEL), SetStringTip(STR_JUNCTION_CANCEL, STR_JUNCTION_CANCEL_TOOLTIP), SetFill(1, 0),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

/** Window definition for the build junction station window. */
static WindowDesc _build_junction_station_desc(
	WindowPosition::Automatic, {}, 0, 0,
	WindowClass::BuildJunctionStation, WindowClass::BuildToolbar,
	WindowDefaultFlag::Construction,
	_nested_build_junction_station_widgets,
	&BuildJunctionStationWindow::hotkeys
);

/**
 * Open the build junction station window.
 * @param parent The rail toolbar that opens the window.
 * @param railtype Rail type of that toolbar.
 */
void ShowBuildJunctionStationWindow(Window *parent, RailType railtype)
{
	new BuildJunctionStationWindow(_build_junction_station_desc, parent, railtype);
}

/**
 * Handle a click on the map with the junction station tool.
 * @param tile The tile that was clicked.
 */
void JunctionStationToolClick(TileIndex tile)
{
	BuildJunctionStationWindow *w = dynamic_cast<BuildJunctionStationWindow *>(FindWindowById(WindowClass::BuildJunctionStation, TransportType::Rail));
	if (w == nullptr || GetJunctionSequencer().GetState() != JunctionBuildState::Idle) return;

	w->StartBuild(tile);
}
