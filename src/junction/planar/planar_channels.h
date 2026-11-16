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

#include <set>

/**
 * A channel: lines that run together and keep their order. Its lines come from the same in ports, go to the same out ports,
 * lie at the same depth from the in ports and cross the same channels in the same order; a line may end inside the channel
 * (at a split or merge of its own) while the others carry on.
 */
struct PlanarChannel {
	std::vector<uint32_t> lines; ///< Into PlanarTopology::lines, left to right across travel.
	std::vector<uint32_t> crossings; ///< Into PlanarChannels::crossings, in travel order.
	std::set<uint32_t> up; ///< The in ports whose trains reach it.
	std::set<uint32_t> down; ///< The out ports its trains reach.
	bool prio = false; ///< One of its lines carries a prio route.
};

/** Two channels crossing whole: every line of one crosses every line of the other. */
struct PlanarChannelCrossing {
	uint32_t a; ///< The channel with the lower index.
	uint32_t b;
	std::vector<uint32_t> line_crossings; ///< Into PlanarTopology::crossings.
};

/** What happens at a site. */
enum class PlanarSiteKind : uint8_t {
	Fray, ///< Splits whose legs go to different out ports (DESIGN-components C3).
	LaneSplit, ///< Splits whose legs go to the same out ports (a choice of lane, C2).
	Join, ///< Merges of lines from different in ports (C4).
	LaneMerge, ///< Merges of lines from the same in ports (legs bound for one lane merging first, D224).
};

/** A site: the splits leaving one channel, or the merges entering one channel. */
struct PlanarSite {
	PlanarSiteKind kind;
	uint32_t channel; ///< The channel split (a split site) or merged into (a merge site).
	std::vector<uint32_t> nodes; ///< Into PlanarTopology::nodes.
	std::vector<uint32_t> others; ///< The channels leaving (a split site) or arriving (a merge site), by index.
	uint32_t straight = UINT32_MAX; ///< Of others, the channel continuing straight; UINT32_MAX when two tie (a variant).
	int8_t side = 0; ///< Where the other channel leaves or arrives: +1 right of travel, -1 left, 0 with no straight channel.
	bool prio = false; ///< The straight channel carries a prio route.
};

/** One split or merge node's legs: which runs straight and where the other is. */
struct PlanarNodeLegs {
	uint32_t straight = UINT32_MAX; ///< The straight line (a split's out line, a merge's in line); UINT32_MAX when the legs tie (a fork).
	int8_t side = 0; ///< Where the other leg leaves or arrives: +1 right of travel, -1 left, 0 at a fork.
};

/** The channel graph of a topology. */
struct PlanarChannels {
	std::vector<PlanarChannel> channels;
	std::vector<uint32_t> channel_of_line; ///< Per PlanarTopology line, its channel.
	std::vector<PlanarChannelCrossing> crossings; ///< Crossings between two channels.
	std::vector<std::vector<uint32_t>> group_crossings; ///< Per node group, the topology crossings inside it (its lines share that group as start or end).
	std::vector<PlanarSite> sites; ///< In channel order of their key channel, splits before merges.
	std::vector<PlanarNodeLegs> node_legs; ///< Per PlanarTopology node.
	std::vector<double> line_share; ///< Per PlanarTopology line, its share of a full lane's flow: a bridge on a line with a full share is doubled.
};

PlanarChannels BuildPlanarChannels(const PlanarTopology &topology);

#endif /* PLANAR_CHANNELS_H */
