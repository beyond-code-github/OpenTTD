/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file planar_regions.h Which side of each other line a line keeps, read from the topology alone: the planar map's faces, the ring and the legs' order. */

#ifndef PLANAR_REGIONS_H
#define PLANAR_REGIONS_H

#include "../junction_spec.h"
#include "planar_channels.h"
#include "planar_gaps.h"

#include <map>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
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

/** One track of the track map: a topology line, a copy or a prio row. */
struct PlanarTrack {
	enum class Kind : uint8_t { Line, Copy, Row };
	Kind kind;
	uint32_t line; ///< The topology line it is, or runs beside.
	std::string name; ///< The line's name; a copy "<line>\"" plus its first crossing's index; a row "prio:<line>".
};

/** The hub's tracks as one planar map (DESIGN-tracks §16): the skeleton expanded with copies and rows, with sides per stretch. */
struct PlanarTrackMap {
	std::vector<PlanarTrack> tracks; ///< Lines first (same index as the topology's lines), then the parts lines are split into, then copies and rows.
	std::vector<uint32_t> part_of; ///< Per track, the topology line whose part it is (a line part), or UINT32_MAX for copies and rows.
	PlanarTopology expanded; ///< The expanded topology: its lines are the tracks (same order), with their ends, the nodes between them (copy and row splits and merges included), crossings and events.
	std::vector<std::vector<uint32_t>> events; ///< Per track, its crossings (indices into crossings) in travel order.
	std::vector<PlanarCrossing> crossings; ///< The expanded map's crossings (track indices).
	std::vector<PlanarFace> faces; ///< As PlanarLineSides::Faces, over tracks.
	std::string why; ///< Empty, or the expansion or validation failure.
	/** Side of track p's stretch ps relative to track q's stretch qs: +1 right of q's travel, -1 left, 0 when they share no face. */
	int8_t Side(uint32_t p, uint32_t ps, uint32_t q, uint32_t qs) const;
	std::map<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>, int8_t> side; ///< (p, ps, q, qs) -> side, filled from the faces.
	std::vector<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>> conflicts; ///< Stretch pairs two faces give opposite sides.
};

/** Every mover line (D372): every joiner of every join (prio or not) and every other leg of every fray, in site order. */
std::vector<uint32_t> PlanarMovers(const PlanarTopology &topology, const PlanarChannels &channels, const std::vector<PlanarResolvedSite> &sites);

/**
 * A structure as a run of topology crossings on its separator line, in travel order; copy when the line's share is full.
 */
struct PlanarStructureRun {
	uint32_t line; ///< The separator: a topology line.
	std::vector<uint32_t> crossings; ///< Indices into PlanarTopology::crossings, in travel order along the line.
	bool copy; ///< The line's share is full: the run has a copy beside it.
};

/**
 * The line that carries each topology crossing's structure (D369, D372, D364).
 * A crossing of a channel crossing has its line on the over channel; a crossing in a node group has the mover when it is in the set, else the keeper (a prio join's lane or joiner included).
 * @param over Per channel crossing, the channel that is over.
 * @param separator_movers Per mover (D364, D372): the topology lines of the movers that are separators.
 * @param why Set when a crossing has no rule or two (naming it).
 * @return Per topology crossing, its separator line (UINT32_MAX where why is set).
 */
std::vector<uint32_t> PlanarCrossingSeparators(const PlanarTopology &topology, const PlanarChannels &channels, const std::vector<PlanarResolvedSite> &sites, const std::vector<uint32_t> &over, const std::set<uint32_t> &separator_movers, std::string &why);

/**
 * The structures of a variant (D369, D371(2)): on each line, the maximal runs of consecutive crossings whose separator is that line, each split greedily from its start only where the bridge limit or, with a copy, the D228a flow bound forces it.
 * A run is also cut before a prio lane's row start (its first crossing with a joiner): the row's split node lies there, and a bridge cannot hold a node. The limits count line crossings only.
 * @param separator Per topology crossing, its separator line (PlanarCrossingSeparators).
 * @return The runs, line by line, in travel order.
 */
std::vector<PlanarStructureRun> PlanarGroupStructures(const PlanarTopology &topology, const PlanarChannels &channels, const std::vector<PlanarResolvedSite> &sites, const PlanarLineSides &sides, const std::vector<uint32_t> &separator, const JunctionSpec &spec);

/**
 * Build the track map of a variant.
 * @param runs The structures (PlanarGroupStructures): each run with a copy gets exactly one copy track beside its line over its crossings, and nothing else makes copies.
 */
PlanarTrackMap BuildPlanarTrackMap(const PlanarTopology &topology, const PlanarChannels &channels, const std::vector<PlanarResolvedSite> &sites, const std::vector<PlanarStructureRun> &runs);

#endif /* PLANAR_REGIONS_H */
