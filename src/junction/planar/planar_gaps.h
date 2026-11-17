/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file planar_gaps.h Gap steps: the sideways steps the pinned gaps between neighbouring port lines at a box edge need, from the channel graph. */

#ifndef PLANAR_GAPS_H
#define PLANAR_GAPS_H

#include "planar_channels.h"

/** A site with its straight channel decided: the composer's view of a two-channel split or join. */
struct PlanarResolvedSite {
	uint32_t straight = UINT32_MAX; ///< UINT32_MAX for a site composed per node.
	uint32_t other = UINT32_MAX;
	int8_t side = 0; ///< Where the other channel leaves or arrives: +1 right of travel, -1 left (from its turn).
	bool prio = false; ///< The straight channel carries a prio route.
};

/** A sideways step a pinned gap at the box edge needs. */
struct PlanarGapStep {
	uint32_t channel; ///< The channel that steps.
	bool fan; ///< True: on the in channel's fray fan; false: on the out channel's close-up after its join.
	uint32_t neighbour; ///< The channel across the pinned gap.
	int8_t toward = 0; ///< Where the step moves, +1 right of the stepping channel's travel, -1 left: a close-up toward its neighbour, a fan away from it.
};

/** A candidate sideways step (DESIGN-pieces §3 source 5): a straight channel over crossings may step once before its core, away from its in port's neighbour. */
struct PlanarCandidateStep {
	uint32_t channel;
	int8_t toward; ///< +1 right of travel, -1 left.
};

std::vector<PlanarResolvedSite> ResolvePlanarSites(const PlanarTopology &topology, const PlanarChannels &channels, uint64_t ties, std::string &why);
std::vector<PlanarGapStep> BuildPlanarGapSteps(const PlanarTopology &topology, const PlanarChannels &channels, const std::vector<PlanarResolvedSite> &sites, std::string &why);
std::vector<PlanarCandidateStep> PlanarCandidateSteps(const PlanarTopology &topology, const PlanarChannels &channels);

/** The exit step of a one-lane merge onto a port line: +1 when the joiner arrives from the right and the out line's ring successor is on the same box side, else 0. */
int8_t PlanarMergeExitStep(const PlanarTopology &topology, uint32_t out_line, int8_t joiner_side);
/** The exit step of an inner one-lane merge (out line no port line): a width change, +1 when another merge of its site has its out line to the right in the out channel's line order, whatever the joiner side. */
int8_t PlanarInnerMergeExitStep(const PlanarTopology &topology, const PlanarChannels &channels, const PlanarSite &site, uint32_t out_line);

#endif /* PLANAR_GAPS_H */
