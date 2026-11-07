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
	WID_BJ_NAME,   ///< Name of the selected junction.
	WID_BJ_ROTATE, ///< Rotate the junction.
	WID_BJ_MIRROR, ///< Mirror the junction.
	WID_BJ_STATUS, ///< Status line.
	WID_BJ_RETRY,  ///< Retry building after a failure.
	WID_BJ_CANCEL, ///< Cancel a failed or ongoing build.
};

#endif /* WIDGETS_JUNCTION_WIDGET_H */
