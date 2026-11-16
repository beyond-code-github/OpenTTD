/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file planar_paths.h Channel paths: each channel's headings and where its crossings sit, and which channel is over at each crossing (D327). */

#ifndef PLANAR_PATHS_H
#define PLANAR_PATHS_H

#include "planar_channels.h"

/** A channel's path: its pieces' headings in travel order, and the piece each of its crossings sits on. */
struct PlanarChannelPath {
	std::vector<PlanarHeading> headings; ///< One per piece; consecutive pieces differ by 45 degrees.
	std::vector<uint32_t> crossing_piece; ///< Per entry of PlanarChannel::crossings, the piece it sits on.
};

/** Every channel's path for one choice of the open crossings. */
struct PlanarPaths {
	std::vector<PlanarChannelPath> paths; ///< Per channel.
	std::vector<uint32_t> over; ///< Per channel crossing, the channel that is over.
	std::string why; ///< Empty, or why no paths were found.
};

std::vector<uint32_t> PlanarOpenCrossings(const PlanarChannels &channels);
PlanarPaths BuildPlanarPaths(const PlanarTopology &topology, const PlanarChannels &channels, uint64_t choice);

#endif /* PLANAR_PATHS_H */
