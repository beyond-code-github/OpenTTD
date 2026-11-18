/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file planar_regions.h Which side of each other line a line keeps, read from the topology alone: the planar map's faces, the ring and the legs' order. */

#ifndef PLANAR_REGIONS_H
#define PLANAR_REGIONS_H

#include "planar_channels.h"

#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/** A face's wall made by one line or by consecutive lines, walked in one direction. */
struct PlanarWallRun {
	bool forward; ///< The run walks the face with forward darts: the face lies left of the line's travel.
	std::vector<std::pair<uint32_t, uint32_t>> stretches; ///< (line, stretch index), in walk order.
};

/** A face of the planar map: its walls as runs, in walk order. */
struct PlanarFace {
	int face; ///< The face's index in the planar map.
	bool box; ///< A ring edge bounds the face.
	std::vector<PlanarWallRun> runs; ///< The maximal runs of one dart direction; the last is merged into the first when they match.
};

/** Which side of each other two lines keep, read from the topology alone (DESIGN-tracks §3). */
class PlanarLineSides {
public:
	PlanarLineSides(const PlanarTopology &topology, const PlanarChannels &channels);

	/**
	 * Where line p lies relative to line q's travel.
	 * @param p The line.
	 * @param q The line it is seen from.
	 * @return +1 right, -1 left, 0 undecided (or p == q, or the lines cross).
	 */
	int8_t Side(uint32_t p, uint32_t q) const;

	/**
	 * Which side of `other` the line arrives at a crossing from.
	 * @param crossing The line-level crossing, an index into PlanarTopology::crossings.
	 * @param line One of the crossing's two lines; the other is `other`.
	 * @return +1 when `line` arrives from the left of the other line's travel, -1 from its right, 0 when undecided (Why() says why).
	 */
	int8_t CrossingFrom(uint32_t crossing, uint32_t line) const;

	/**
	 * The line a composer strand belongs to: the strand's name without a trailing copy mark (' or " and any digits after it).
	 * @param strand The strand name.
	 * @return The line, or UINT32_MAX when no line has that name.
	 */
	uint32_t LineOfStrand(std::string_view strand) const;

	/** Every ordered pair of distinct lines (p, q) bounding a common face with Side(p, q) != 0, once each. */
	std::vector<std::pair<uint32_t, uint32_t>> FacePairs() const;

	/** @return Every face except the outer one, with its walls as runs; the face map follows the variant's channel crossing order. */
	const std::vector<PlanarFace> &Faces() const { return this->faces; }

	/** @return Each line's crossing events in the variant's channel order; the stretch indices of `Faces()` count these. */
	const std::vector<std::vector<uint32_t>> &Events() const { return this->events; }

	/** @return Empty, or why the face map failed validation (V - E + F != 2, a face not a simple walk) or two sources disagreed. */
	const std::string &Why() const { return this->why; }

private:
	size_t count = 0; ///< The number of lines.
	std::vector<int8_t> table; ///< Side(p, q) at p * count + q.
	std::string why;
	std::vector<PlanarFace> faces; ///< Every face but the outer, as runs of walls.
	std::vector<std::vector<uint32_t>> events; ///< Each line's crossing events in the variant's channel order.
	std::map<std::pair<uint32_t, uint32_t>, int8_t> crossing_from; ///< CrossingFrom by (crossing, line).
	std::vector<std::pair<uint32_t, uint32_t>> face_pairs; ///< The ordered pairs sharing a face with a side.
	std::map<std::string, uint32_t, std::less<>> by_name; ///< Line index by name.
};

#endif /* PLANAR_REGIONS_H */
