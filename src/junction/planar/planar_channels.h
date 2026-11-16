/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file planar_channels.h The channel graph of a planar topology (D305, D306): lines that run together, their order, and the crossings between channels. */

#ifndef PLANAR_CHANNELS_H
#define PLANAR_CHANNELS_H

#include "planar_topology.h"

/**
 * A channel: lines that run together and keep their order. Its lines come from the same in ports, go to the same out ports,
 * lie at the same depth from the in ports and cross the same channels in the same order; a line may end inside the channel
 * (at a split or merge of its own) while the others carry on.
 */
struct PlanarChannel {
	std::vector<uint32_t> lines; ///< Into PlanarTopology::lines, left to right across travel.
	std::vector<uint32_t> crossings; ///< Into PlanarChannels::crossings, in travel order.
};

/** Two channels crossing whole: every line of one crosses every line of the other. */
struct PlanarChannelCrossing {
	uint32_t a; ///< The channel with the lower index.
	uint32_t b;
	std::vector<uint32_t> line_crossings; ///< Into PlanarTopology::crossings.
};

/** The channel graph of a topology. */
struct PlanarChannels {
	std::vector<PlanarChannel> channels;
	std::vector<uint32_t> channel_of_line; ///< Per PlanarTopology line, its channel.
	std::vector<PlanarChannelCrossing> crossings; ///< Crossings between two channels.
	std::vector<std::vector<uint32_t>> group_crossings; ///< Per node group, the topology crossings inside it (its lines share that group as start or end).
};

PlanarChannels BuildPlanarChannels(const PlanarTopology &topology);

#endif /* PLANAR_CHANNELS_H */
