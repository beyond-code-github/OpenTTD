/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file planar_slots.h The slot layer: the sideways slot of every track of every channel interval, from the topology and the variant. */

#ifndef PLANAR_SLOTS_H
#define PLANAR_SLOTS_H

#include "planar_channels.h"
#include "planar_gaps.h"

/** What a track in a channel interval is. */
enum class PlanarSlotKind : uint8_t {
	Line, ///< A topology line.
	Copy, ///< A doubled line's copy over a crossing.
	Row, ///< A prio row beside its lane (D349).
};

/** One track of an interval. */
struct PlanarSlotTrack {
	PlanarSlotKind kind;
	uint32_t line; ///< Into PlanarTopology::lines: the line, or the line it copies or protects.
	int slot; ///< Sideways position in lane spacings, left to right across travel; per channel, the least slot is 0.
};

/** One interval of a channel: pre, x<k>, mid<k> or post. */
struct PlanarSlotInterval {
	std::string name;
	std::vector<PlanarSlotTrack> tracks; ///< Left to right.
};

/** A straight line's step across a site. */
struct PlanarSlotLink {
	uint32_t from; ///< The line before the site.
	uint32_t to; ///< The line after it.
	int step; ///< Slot change, after minus before.
};

/** The slot layer of one variant (DESIGN-tracks §15). */
struct PlanarSlots {
	std::vector<std::vector<PlanarSlotInterval>> channels; ///< Per channel, its intervals in travel order.
	std::vector<PlanarSlotLink> links;
	double objective = 0;
	std::string why; ///< Empty, or why no slots were found.
};

PlanarSlots BuildPlanarSlots(const PlanarTopology &topology, const PlanarChannels &channels, const std::vector<PlanarResolvedSite> &sites, const std::vector<uint32_t> &over);

#endif /* PLANAR_SLOTS_H */
