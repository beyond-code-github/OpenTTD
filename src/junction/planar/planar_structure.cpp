/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file planar_structure.cpp The Structure layer's promise oracle. */

#include "../../stdafx.h"

#include "planar_structure.h"

#include "../../core/format.hpp"

#include <algorithm>
#include <map>
#include <set>

#include "../../safeguards.h"

namespace {

/** @return The position of a crossing in a track's events, or SIZE_MAX. */
size_t EventIndex(const std::vector<uint32_t> &events, uint32_t crossing)
{
	const auto it = std::ranges::find(events, crossing);
	return it == events.end() ? SIZE_MAX : static_cast<size_t>(it - events.begin());
}

} // namespace

std::vector<std::string> CheckPlanarStructurePromise(const PlanarTopology &topology, const JunctionSpec &spec, const PlanarStructureVariant &v)
{
	std::vector<std::string> out;
	const PlanarTrackMap &map = v.map;

	/* P7 reads the topology alone: on a route path no split node follows a merge node. */
	for (const PlanarRoute &route : topology.routes) {
		for (const std::vector<uint32_t> &path : route.paths) {
			bool merged = false;
			for (const uint32_t line : path) {
				const PlanarEnd &to = topology.lines[line].to;
				if (to.port) continue;
				if (!topology.nodes[to.index].IsSplit()) {
					merged = true;
				} else if (merged) {
					out.push_back(fmt::format("P7: route {} splits at the end of line {} after a merge", route.name, topology.lines[line].name));
					break;
				}
			}
		}
	}

	/* P1: the map is valid, every authored crossing appears once, no track lists a crossing twice. */
	if (!map.why.empty()) {
		out.push_back(fmt::format("P1: the track map is invalid: {}", map.why));
		return out;
	}
	if (map.events.size() != map.tracks.size() || map.part_of.size() != map.tracks.size() || map.expanded.lines.size() != map.tracks.size()) {
		out.push_back("P1: the track map's tracks, events and expanded lines differ in count");
		return out;
	}
	const auto plain = [&](uint32_t t) { return map.tracks[t].kind == PlanarTrack::Kind::Line; };
	const auto crossing_ok = [&](uint32_t j) { return j < map.crossings.size() && map.crossings[j].a < map.tracks.size() && map.crossings[j].b < map.tracks.size(); };
	for (uint32_t k = 0; k < topology.crossings.size(); k++) {
		const PlanarCrossing &c = topology.crossings[k];
		size_t found = 0;
		for (uint32_t j = 0; j < map.crossings.size(); j++) {
			if (!crossing_ok(j)) continue;
			const uint32_t a = map.crossings[j].a, b = map.crossings[j].b;
			if (!plain(a) || !plain(b)) continue;
			if ((map.part_of[a] == c.a && map.part_of[b] == c.b) || (map.part_of[a] == c.b && map.part_of[b] == c.a)) found++;
		}
		if (found != 1) out.push_back(fmt::format("P1: crossing {} x {} appears {} times in the track map", topology.lines[c.a].name, topology.lines[c.b].name, found));
	}
	for (uint32_t t = 0; t < map.tracks.size(); t++) {
		std::set<uint32_t> seen;
		for (const uint32_t j : map.events[t]) {
			if (!seen.insert(j).second) out.push_back(fmt::format("P1: track {} lists crossing {} twice", map.tracks[t].name, j));
		}
	}

	/* P2. */
	if (!map.conflicts.empty()) out.push_back(fmt::format("P2: {} stretch pairs have conflicting sides", map.conflicts.size()));

	/* P3: every crossing between two line tracks is in exactly one structure; a structure's crossings are consecutive events of its separator. */
	std::vector<std::vector<uint32_t>> owners(map.crossings.size()); ///< Per crossing, the separator of each structure holding it.
	for (size_t i = 0; i < v.structures.size(); i++) {
		const PlanarStructure &s = v.structures[i];
		if (s.separator >= map.tracks.size() || s.crossings.size() != s.crossers.size()) {
			out.push_back(fmt::format("P3: structure {} has a bad separator or crossers", i));
			continue;
		}
		size_t previous = SIZE_MAX;
		for (size_t k = 0; k < s.crossings.size(); k++) {
			const uint32_t j = s.crossings[k];
			if (!crossing_ok(j)) {
				out.push_back(fmt::format("P3: structure {} lists crossing {} which the map lacks", i, j));
				continue;
			}
			owners[j].push_back(s.separator);
			const uint32_t a = map.crossings[j].a, b = map.crossings[j].b;
			if (!plain(a) || !plain(b)) out.push_back(fmt::format("P3: structure {} holds crossing {} of a copy or row track", i, j));
			if (a != s.separator && b != s.separator) {
				out.push_back(fmt::format("P3: crossing {} of structure {} does not involve its separator {}", j, i, map.tracks[s.separator].name));
				continue;
			}
			if (s.crossers[k] != (a == s.separator ? b : a)) out.push_back(fmt::format("P3: crosser {} of structure {} is not the other track of crossing {}", k, i, j));
			const size_t at = EventIndex(map.events[s.separator], j);
			if (at == SIZE_MAX) {
				out.push_back(fmt::format("P3: crossing {} of structure {} is not an event of its separator", j, i));
			} else if (previous != SIZE_MAX && at != previous + 1) {
				out.push_back(fmt::format("P3: crossings of structure {} are not consecutive events of its separator", i));
			}
			previous = at;
		}
	}
	for (uint32_t j = 0; j < map.crossings.size(); j++) {
		if (!plain(map.crossings[j].a) || !plain(map.crossings[j].b)) continue;
		/* Exactly one structure, or two when each of the crossing's tracks is the separator of one of them. */
		const uint32_t a = map.crossings[j].a, b = map.crossings[j].b;
		const bool two_ok = owners[j].size() == 2 && ((owners[j][0] == a && owners[j][1] == b) || (owners[j][0] == b && owners[j][1] == a));
		if (owners[j].size() != 1 && !two_ok) out.push_back(fmt::format("P3: crossing {} ({} x {}) is in {} structures", j, map.tracks[a].name, map.tracks[b].name, owners[j].size()));
	}

	/* P4: a crossing within the span of its crosser's own structure needs one bridge and one tunnel. */
	for (size_t i = 0; i < v.structures.size(); i++) {
		const PlanarStructure &s = v.structures[i];
		if (s.separator >= map.tracks.size() || s.crossings.size() != s.crossers.size()) continue;
		for (size_t k = 0; k < s.crossings.size(); k++) {
			const uint32_t y = s.crossers[k];
			if (y >= map.tracks.size()) continue;
			const size_t at = EventIndex(map.events[y], s.crossings[k]);
			for (size_t m = 0; m < v.structures.size(); m++) {
				const PlanarStructure &t = v.structures[m];
				if (m == i || t.separator != y || t.crossings.empty()) continue;
				const size_t first = EventIndex(map.events[y], t.crossings.front()), last = EventIndex(map.events[y], t.crossings.back());
				if (at == SIZE_MAX || first == SIZE_MAX || last == SIZE_MAX || at < std::min(first, last) || at > std::max(first, last)) continue;
				const bool one_each = ((s.kinds & 1) != 0 && (t.kinds & 2) != 0) || ((s.kinds & 2) != 0 && (t.kinds & 1) != 0);
				if (!one_each) out.push_back(fmt::format("P4: crossing {} of structure {} lies within the span of structure {} on its crosser, and they cannot be a bridge and a tunnel", s.crossings[k], i, m));
			}
		}
	}

	/* P5: a kind exists and every kind fits the limit. */
	for (size_t i = 0; i < v.structures.size(); i++) {
		const PlanarStructure &s = v.structures[i];
		if ((s.kinds & 3) == 0) out.push_back(fmt::format("P5: structure {} allows no kind", i));
		if ((s.kinds & 1) != 0 && s.crossers.size() > spec.max_bridge_middle) out.push_back(fmt::format("P5: structure {} spans {} tracks, over the bridge limit {}", i, s.crossers.size(), spec.max_bridge_middle));
		if ((s.kinds & 2) != 0 && s.crossers.size() > spec.max_tunnel_length) out.push_back(fmt::format("P5: structure {} spans {} tracks, over the tunnel limit {}", i, s.crossers.size(), spec.max_tunnel_length));
		if (s.copy != UINT32_MAX && s.crossers.size() + 2 > 2u * spec.sd + spec.tl) out.push_back(fmt::format("P5: structure {} has a copy and spans {} tracks, over the D228a flow bound 2*sd+tl-2 = {}", i, s.crossers.size(), 2 * spec.sd + spec.tl - 2));
	}

	/* P6: copies follow the share, rows follow the prio joins. */
	const PlanarChannels channels = BuildPlanarChannels(topology);
	std::map<uint32_t, size_t> used_copies; ///< Per copy track, the structures naming it.
	for (size_t i = 0; i < v.structures.size(); i++) {
		const PlanarStructure &s = v.structures[i];
		if (s.separator >= map.tracks.size()) continue;
		const bool full = channels.line_share[map.tracks[s.separator].line] >= 1.0;
		if (!full) {
			if (s.copy != UINT32_MAX) out.push_back(fmt::format("P6: structure {} has a copy but its separator {} has not a full share", i, map.tracks[s.separator].name));
			continue;
		}
		const PlanarLine *copy_line = s.copy < map.tracks.size() ? &map.expanded.lines[s.copy] : nullptr;
		const PlanarLine &sep_line = map.expanded.lines[s.separator];
		if (copy_line == nullptr) {
			out.push_back(fmt::format("P6: structure {} has no copy but its separator {} has a full share", i, map.tracks[s.separator].name));
		} else if (map.tracks[s.copy].kind != PlanarTrack::Kind::Copy || map.tracks[s.copy].line != map.tracks[s.separator].line ||
				copy_line->from.port != sep_line.from.port || copy_line->from.index != sep_line.from.index || copy_line->to.port != sep_line.to.port || copy_line->to.index != sep_line.to.index) {
			out.push_back(fmt::format("P6: the copy of structure {} is not a copy track beside its separator {}", i, map.tracks[s.separator].name));
		} else {
			used_copies[s.copy]++;
		}
	}
	for (uint32_t t = 0; t < map.tracks.size(); t++) {
		if (map.tracks[t].kind != PlanarTrack::Kind::Copy) continue;
		const auto it = used_copies.find(t);
		if (it == used_copies.end()) {
			out.push_back(fmt::format("P6: copy track {} is no structure's copy", map.tracks[t].name));
		} else if (it->second > 1) {
			out.push_back(fmt::format("P6: copy track {} is the copy of {} structures", map.tracks[t].name, it->second));
		}
	}
	std::set<uint32_t> join_nodes, lanes;
	for (const PlanarSite &site : channels.sites) {
		if (site.kind == PlanarSiteKind::Join) join_nodes.insert(site.nodes.begin(), site.nodes.end());
	}
	for (const PlanarStructureSite &site : v.sites) {
		if (site.split || site.node >= topology.nodes.size() || !join_nodes.contains(site.node) || site.straight >= map.tracks.size()) continue;
		const uint32_t line = map.tracks[site.straight].line;
		for (const uint32_t r : topology.lines[line].routes) {
			if (topology.routes[r].prio) lanes.insert(line);
		}
	}
	for (uint32_t t = 0; t < map.tracks.size(); t++) {
		if (map.tracks[t].kind == PlanarTrack::Kind::Row && !lanes.contains(map.tracks[t].line)) out.push_back(fmt::format("P6: row track {} is not beside a lane of a prio join", map.tracks[t].name));
	}
	return out;
}
