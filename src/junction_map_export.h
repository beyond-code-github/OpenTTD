/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_map_export.h Export of a live map area as JSON, and a finder for hubs and big stations. */

#ifndef JUNCTION_MAP_EXPORT_H
#define JUNCTION_MAP_EXPORT_H

#include "station_type.h"

#include <string>
#include <vector>

/** One ranked map window for finding hubs and big stations. */
struct JunctionHotspot {
	uint x0, y0, x1, y1;
	uint presignals; ///< Entry, exit and combo signals.
	uint pbs; ///< Path signals, both kinds.
	uint bridge_heads; ///< Rail bridge heads.
	uint station_tiles; ///< Rail station tiles (waypoints excluded).
	double score;
	std::vector<StationID> stations; ///< Rail stations with tiles inside.
};

std::string JunctionMapExportJson(uint x0, uint y0, uint x1, uint y1);
std::vector<JunctionHotspot> JunctionMapHotspots(uint top_k, uint window);

#endif /* JUNCTION_MAP_EXPORT_H */
