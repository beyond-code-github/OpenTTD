/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_station_widget.h Types related to the junction station widgets. */

#ifndef WIDGETS_JUNCTION_STATION_WIDGET_H
#define WIDGETS_JUNCTION_STATION_WIDGET_H

/** Widgets of the #BuildJunctionStationWindow class. */
enum BuildJunctionStationWidgets : WidgetID {
	WID_BJS_PLATFORMS_LABEL, ///< Platform count label.
	WID_BJS_PLATFORMS_DECREASE, ///< Decrease the number of platforms.
	WID_BJS_PLATFORMS_INCREASE, ///< Increase the number of platforms.
	WID_BJS_TL_LABEL, ///< Train length label.
	WID_BJS_TL_DECREASE, ///< Decrease the train length.
	WID_BJS_TL_INCREASE, ///< Increase the train length.
	WID_BJS_PICK_TRAIN, ///< Pick a train to size the station for.
	WID_BJS_TRAIN, ///< Info line of the picked train.
	WID_BJS_COST, ///< Cost of building the station at the cursor.
	WID_BJS_ROTATE, ///< Rotate the station.
	WID_BJS_MIRROR, ///< Mirror the station.
	WID_BJS_STATUS, ///< Status line.
	WID_BJS_RETRY, ///< Retry building after a failure.
	WID_BJS_CANCEL, ///< Cancel a failed or ongoing build.
};

#endif /* WIDGETS_JUNCTION_STATION_WIDGET_H */
