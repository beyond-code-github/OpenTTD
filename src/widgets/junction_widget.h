/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_widget.h Types related to the junction widgets. */

#ifndef WIDGETS_JUNCTION_WIDGET_H
#define WIDGETS_JUNCTION_WIDGET_H

/** Widgets of the #BuildJunctionWindow class. */
enum BuildJunctionWidgets : WidgetID {
	WID_BJ_PRESET, ///< Dropdown of the topology presets (D222).
	WID_BJ_TOPOLOGY, ///< Open the Topology window.
	WID_BJ_TL_LABEL, ///< Train length label.
	WID_BJ_TL_DECREASE, ///< Decrease the train length.
	WID_BJ_TL_INCREASE, ///< Increase the train length.
	WID_BJ_PICK_TRAIN, ///< Pick a train to size the junction for.
	WID_BJ_TRAIN_IMAGE, ///< Picture of the picked train.
	WID_BJ_TRAIN, ///< Info line of the picked train.
	WID_BJ_PRIO_LABEL, ///< Label of the prio length.
	WID_BJ_PRIO_SLIDER, ///< Slider of the prio length.
	WID_BJ_ROTATE, ///< Rotate the junction.
	WID_BJ_MIRROR, ///< Mirror the junction.
	WID_BJ_BUILD_SEL, ///< Selection of the Retry and Cancel row: shown while a build is pinned.
	WID_BJ_RETRY,  ///< Retry building after a failure.
	WID_BJ_CANCEL, ///< Cancel a failed or ongoing build.
};

/**
 * Widgets of the topology window. Each stepper is a decrease button, a label and an increase button in that order, and the six steppers
 * are main line left lanes, right lanes, gap, then the same for the branch.
 */
enum JunctionTopologyWidgets : WidgetID {
	WID_JT_ML_LEFT_DECREASE, ///< Decrease the main line's left lanes.
	WID_JT_ML_LEFT_LABEL, ///< Label of the main line's left lanes.
	WID_JT_ML_LEFT_INCREASE, ///< Increase the main line's left lanes.
	WID_JT_ML_RIGHT_DECREASE, ///< Decrease the main line's right lanes.
	WID_JT_ML_RIGHT_LABEL, ///< Label of the main line's right lanes.
	WID_JT_ML_RIGHT_INCREASE, ///< Increase the main line's right lanes.
	WID_JT_ML_GAP_DECREASE, ///< Decrease the main line's gap.
	WID_JT_ML_GAP_LABEL, ///< Label of the main line's gap.
	WID_JT_ML_GAP_INCREASE, ///< Increase the main line's gap.
	WID_JT_BRANCH_LEFT_DECREASE, ///< Decrease the branch's left lanes.
	WID_JT_BRANCH_LEFT_LABEL, ///< Label of the branch's left lanes.
	WID_JT_BRANCH_LEFT_INCREASE, ///< Increase the branch's left lanes.
	WID_JT_BRANCH_RIGHT_DECREASE, ///< Decrease the branch's right lanes.
	WID_JT_BRANCH_RIGHT_LABEL, ///< Label of the branch's right lanes.
	WID_JT_BRANCH_RIGHT_INCREASE, ///< Increase the branch's right lanes.
	WID_JT_BRANCH_GAP_DECREASE, ///< Decrease the branch's gap.
	WID_JT_BRANCH_GAP_LABEL, ///< Label of the branch's gap.
	WID_JT_BRANCH_GAP_INCREASE, ///< Increase the branch's gap.
	WID_JT_PAIRING_LABEL, ///< Label of the lane pairing.
	WID_JT_PAIRING, ///< Dropdown of the lane pairing (D221).
	WID_JT_SUMMARY, ///< The notations as text.
};

#endif /* WIDGETS_JUNCTION_WIDGET_H */
