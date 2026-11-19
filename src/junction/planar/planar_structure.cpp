/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file planar_structure.cpp The Structure layer: the ranked variant list and the promise oracle. */

#include "../../stdafx.h"

#include "planar_structure.h"

#include "../../core/format.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <memory>
#include <set>
#include <tuple>

#include "../../safeguards.h"

namespace {

/** @return The position of a crossing in a track's events, or SIZE_MAX. */
size_t EventIndex(const std::vector<uint32_t> &events, uint32_t crossing)
{
	const auto it = std::ranges::find(events, crossing);
	return it == events.end() ? SIZE_MAX : static_cast<size_t>(it - events.begin());
}

/** @return Whether two tracks of the expanded topology have the same two ends. */
bool SameEnds(const PlanarTrackMap &map, uint32_t a, uint32_t b)
{
	const PlanarLine &x = map.expanded.lines[a], &y = map.expanded.lines[b];
	return x.from.port == y.from.port && x.from.index == y.from.index && x.to.port == y.to.port && x.to.index == y.to.index;
}

/** @return The Copy track beside a track (same line, same ends), or UINT32_MAX. */
uint32_t CopyBeside(const PlanarTrackMap &map, uint32_t track)
{
	for (uint32_t i = 0; i < map.tracks.size(); i++) {
		if (map.tracks[i].kind == PlanarTrack::Kind::Copy && map.tracks[i].line == map.tracks[track].line && SameEnds(map, i, track)) return i;
	}
	return UINT32_MAX;
}

} // namespace

std::vector<uint32_t> PlanarChannelOver(const PlanarChannels &channels, uint64_t bits)
{
	std::vector<uint32_t> over(channels.crossings.size());
	for (uint32_t x = 0; x < channels.crossings.size(); x++) {
		const PlanarChannelCrossing &c = channels.crossings[x];
		const bool a_prio = channels.channels[c.a].prio, b_prio = channels.channels[c.b].prio;
		const uint32_t first = a_prio != b_prio ? (a_prio ? c.a : c.b) : c.a;
		const uint32_t other = first == c.a ? c.b : c.a;
		over[x] = (bits >> x & 1) != 0 ? other : first;
	}
	return over;
}

/**
 * The most middle tiles a structure may span per kind.
 * @param spec The spec.
 * @param s The structure: its kinds and whether it has a copy.
 * @return Bridge then tunnel, -1 for a kind not allowed.
 */
static std::array<int, 2> StructureLimit(const JunctionSpec &spec, const PlanarStructure &s)
{
	const int flow = 2 * spec.sd + spec.tl - 2;
	std::array<int, 2> limit{-1, -1};
	if ((s.kinds & 1) != 0) limit[0] = s.copy != UINT32_MAX ? std::min<int>(spec.max_bridge_middle, flow) : spec.max_bridge_middle;
	if ((s.kinds & 2) != 0) limit[1] = s.copy != UINT32_MAX ? std::min<int>(spec.max_tunnel_length, flow) : spec.max_tunnel_length;
	return limit;
}

std::vector<PlanarStructure> PlanarBuildStructures(const PlanarTopology &topology, const std::vector<PlanarStructureRun> &runs, const PlanarTrackMap &map, const JunctionSpec &spec)
{
	std::vector<PlanarStructure> out;
	const auto plain = [&](uint32_t t) { return map.tracks[t].kind == PlanarTrack::Kind::Line; };
	/* The map's crossing between two line tracks that is the topology crossing k. */
	const auto map_crossing = [&](uint32_t k) {
		const PlanarCrossing &c = topology.crossings[k];
		for (uint32_t j = 0; j < map.crossings.size(); j++) {
			const uint32_t a = map.crossings[j].a, b = map.crossings[j].b;
			if (!plain(a) || !plain(b)) continue;
			if ((map.part_of[a] == c.a && map.part_of[b] == c.b) || (map.part_of[a] == c.b && map.part_of[b] == c.a)) return j;
		}
		return UINT32_MAX;
	};
	for (const PlanarStructureRun &run : runs) {
		PlanarStructure st{UINT32_MAX, {}, {}, 1, UINT32_MAX, {-1, -1}};
		uint32_t first = UINT32_MAX, last = UINT32_MAX;
		for (const uint32_t k : run.crossings) {
			const uint32_t j = map_crossing(k);
			if (j == UINT32_MAX) continue;
			const uint32_t a = map.crossings[j].a, b = map.crossings[j].b;
			if (st.separator == UINT32_MAX) {
				st.separator = map.part_of[a] == run.line ? a : b;
				first = j;
			}
			last = j;
		}
		/* Every crossing of the separator's events from the run's first to its last belongs to the structure: line crossings and crossings with a row or another structure's copy. Its own copy's crossings follow it. */
		if (st.separator != UINT32_MAX) {
			const std::vector<uint32_t> &events = map.events[st.separator];
			const auto lo = std::ranges::find(events, first), hi = std::ranges::find(events, last);
			for (auto it = lo; it != events.end() && it <= hi; ++it) {
				st.crossings.push_back(*it);
				st.crossers.push_back(map.crossings[*it].a == st.separator ? map.crossings[*it].b : map.crossings[*it].a);
			}
		}
		if (st.separator == UINT32_MAX) continue;
		if (run.copy) st.copy = CopyBeside(map, st.separator);
		st.limit = StructureLimit(spec, st);
		out.push_back(st);
	}
	return out;
}

std::vector<PlanarStructureSite> PlanarBuildStructureSites(const PlanarTopology &topology, const PlanarChannels &channels, const std::vector<PlanarResolvedSite> &sites, const PlanarTrackMap &map)
{
	std::vector<PlanarStructureSite> out;
	const PlanarTopology &e = map.expanded;
	for (uint32_t n = 0; n < e.nodes.size(); n++) {
		const PlanarNode &node = e.nodes[n];
		const bool split = node.IsSplit();
		const std::vector<uint32_t> &legs = split ? node.out : node.in;
		PlanarStructureSite site{n, split, UINT32_MAX, 0};
		if (n >= topology.nodes.size()) {
			/* A copy or row node: the line track continues straight, the other leg is on its side. */
			for (size_t i = 0; i < legs.size(); i++) {
				if (map.tracks[legs[i]].kind == PlanarTrack::Kind::Line) {
					site.straight = legs[i];
					site.side = i == 0 ? +1 : -1;
				}
			}
		} else {
			for (size_t s = 0; s < channels.sites.size(); s++) {
				if (std::ranges::find(channels.sites[s].nodes, n) == channels.sites[s].nodes.end() || sites[s].straight == UINT32_MAX) continue;
				for (const uint32_t l : legs) {
					if (channels.channel_of_line[map.tracks[l].line] == sites[s].straight) site.straight = l;
				}
				site.side = sites[s].side;
			}
		}
		out.push_back(site);
	}
	return out;
}

namespace {

/** @return How many tie bits a channel graph has (a duplicate of CountMappedBits' tie rule in planar_components.cpp, which is the composer's and not Structure's to call, D366): a site whose straight channel is open, counted per node for the lane and odd sites and per site for the rest (as the mapped composer counts them). */
size_t CountTieBits(const PlanarTopology &topology, const PlanarChannels &channels)
{
	size_t ties = 0;
	for (const PlanarSite &site : channels.sites) {
		const bool per_node = site.kind == PlanarSiteKind::LaneSplit || site.kind == PlanarSiteKind::LaneMerge || site.others.size() != 2;
		if (!per_node) {
			ties += site.straight == UINT32_MAX ? 1 : 0;
			continue;
		}
		for (const uint32_t n : site.nodes) ties += !topology.nodes[n].IsSplit() && channels.node_legs[n].straight == UINT32_MAX ? 1 : 0;
	}
	return ties;
}

/** What one (flip, ties) combination fixes: its channels, the sides, the resolved sites and the movers. */
struct FlipTies {
	PlanarChannels channels;
	std::vector<PlanarResolvedSite> sites;
	std::vector<uint32_t> movers;
	bool ok = false; ///< The sites resolved.
};

/** One evaluated variant. */
struct Evaluated {
	std::vector<PlanarStructureRun> runs;
	int cost = 0;
	std::string tag;
	bool ok = false; ///< The separators were found.
};

using Key = std::tuple<uint64_t, uint64_t, uint64_t, uint64_t>; ///< (over, ties, flips, movers).

} // namespace

PlanarStructureResult BuildPlanarStructure(const PlanarTopology &topology, const JunctionSpec &spec, size_t keep)
{
	PlanarStructureResult result;
	const PlanarChannels base = BuildPlanarChannels(topology);
	const size_t over_bits = base.crossings.size(), tie_bits = CountTieBits(topology, base), flip_bits = PlanarTriangles(base).size();
	if (over_bits > 62 || tie_bits > 62 || flip_bits > 62) {
		result.why = "more variant bits than a 64-bit mask holds";
		return result;
	}

	/* Per flip, the flipped channels and the sides; per (flip, ties), the resolved sites and movers; each built once. */
	std::map<uint64_t, PlanarChannels> flipped;
	std::map<uint64_t, std::unique_ptr<PlanarLineSides>> sides;
	std::map<std::pair<uint64_t, uint64_t>, FlipTies> combos;
	const auto flip_of = [&](uint64_t f) -> const PlanarChannels & {
		auto it = flipped.find(f);
		if (it == flipped.end()) {
			it = flipped.emplace(f, FlipPlanarTriangles(base, f)).first;
			sides.emplace(f, std::make_unique<PlanarLineSides>(topology, it->second));
		}
		return it->second;
	};
	const auto combo_of = [&](uint64_t f, uint64_t t) -> FlipTies & {
		const auto found = combos.find({f, t});
		if (found != combos.end()) return found->second;
		FlipTies &c = combos[{f, t}];
		c.channels = flip_of(f);
		std::string why;
		c.sites = ResolvePlanarSites(topology, c.channels, t, why);
		c.ok = why.empty();
		if (c.ok) {
			c.movers = PlanarMovers(topology, c.channels, c.sites);
		} else {
			result.skipped["sites: " + why]++;
		}
		return c;
	};

	std::map<Key, Evaluated> pool;
	const auto evaluate = [&](const Key &key) -> const Evaluated & {
		const auto found = pool.find(key);
		if (found != pool.end()) return found->second;
		const auto [o, t, f, m] = key;
		Evaluated &e = pool[key];
		e.tag = fmt::format("o{}t{}f{}m{}", o, t, f, m);
		FlipTies &c = combo_of(f, t);
		if (!c.ok) return e;
		result.evaluated++;
		std::set<uint32_t> separator_movers;
		for (size_t i = 0; i < c.movers.size(); i++) {
			if ((m >> i & 1) != 0) separator_movers.insert(c.movers[i]);
		}
		std::string why;
		const std::vector<uint32_t> separator = PlanarCrossingSeparators(topology, c.channels, c.sites, PlanarChannelOver(c.channels, o), separator_movers, why);
		if (!why.empty()) {
			result.skipped[why]++;
			return e;
		}
		e.runs = PlanarGroupStructures(topology, c.channels, c.sites, *sides.at(f), separator, spec);
		e.cost = static_cast<int>(e.runs.size() + std::ranges::count_if(e.runs, [](const PlanarStructureRun &r) { return r.copy; }));
		e.ok = true;
		return e;
	};

	/* Exhaustive when the four kinds' bits fit in 16, the movers counted at their largest over the tie combinations of flip 0. */
	size_t max_movers = 0;
	bool small = over_bits + tie_bits + flip_bits <= 16;
	for (uint64_t t = 0; small && t < (uint64_t{1} << tie_bits); t++) {
		const FlipTies &c = combo_of(0, t);
		if (c.ok) max_movers = std::max(max_movers, c.movers.size());
	}
	result.exhaustive = small && over_bits + tie_bits + flip_bits + max_movers <= 16;
	if (result.exhaustive) {
		for (uint64_t f = 0; f < (uint64_t{1} << flip_bits); f++) {
			for (uint64_t t = 0; t < (uint64_t{1} << tie_bits); t++) {
				const FlipTies &c = combo_of(f, t);
				if (!c.ok) continue;
				for (uint64_t m = 0; m < (uint64_t{1} << c.movers.size()); m++) {
					for (uint64_t o = 0; o < (uint64_t{1} << over_bits); o++) evaluate({o, t, f, m});
				}
			}
		}
	} else {
		Key current{0, 0, 0, 0};
		const Evaluated *best = &evaluate(current);
		for (;;) {
			const auto [o, t, f, m] = current;
			const FlipTies &c = combo_of(f, t);
			std::vector<Key> neighbours;
			for (size_t i = 0; i < over_bits; i++) neighbours.push_back({o ^ (uint64_t{1} << i), t, f, m});
			for (size_t i = 0; i < tie_bits; i++) neighbours.push_back({o, t ^ (uint64_t{1} << i), f, m});
			for (size_t i = 0; i < flip_bits; i++) neighbours.push_back({o, t, f ^ (uint64_t{1} << i), m});
			for (size_t i = 0; c.ok && i < c.movers.size(); i++) neighbours.push_back({o, t, f, m ^ (uint64_t{1} << i)});
			const Key *next = nullptr;
			const Evaluated *next_eval = nullptr;
			for (const Key &k : neighbours) {
				const Evaluated &e = evaluate(k);
				if (!e.ok || (best->ok && e.cost >= best->cost)) continue;
				if (next_eval == nullptr || std::tie(e.cost, e.tag) < std::tie(next_eval->cost, next_eval->tag)) {
					next = &k;
					next_eval = &e;
				}
			}
			if (next == nullptr) break;
			current = *next;
			best = next_eval;
		}
	}

	std::vector<std::pair<std::pair<int, std::string>, Key>> ranked;
	for (const auto &[key, e] : pool) {
		if (e.ok) ranked.push_back({{e.cost, e.tag}, key});
	}
	std::ranges::sort(ranked);
	for (const auto &[rank, key] : ranked) {
		if (result.variants.size() >= keep) break;
		const auto &[o, t, f, m] = key;
		const Evaluated &e = pool.at(key);
		const FlipTies &c = combos.at({f, t});
		PlanarStructureVariant v;
		v.tag = e.tag;
		v.cost = e.cost;
		v.map = BuildPlanarTrackMap(topology, c.channels, c.sites, e.runs);
		if (!v.map.why.empty()) {
			result.skipped["map: " + v.map.why]++;
			continue;
		}
		v.structures = PlanarBuildStructures(topology, e.runs, v.map, spec);
		v.sites = PlanarBuildStructureSites(topology, c.channels, c.sites, v.map);
		result.variants.push_back(std::move(v));
	}
	if (result.variants.empty()) result.why = "no variant survives";
	return result;
}

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

	/* P3 maximal: two structures on one separator whose crossings are adjacent events of it would be one, unless merging breaches P5. */
	for (size_t i = 0; i < v.structures.size(); i++) {
		const PlanarStructure &s = v.structures[i];
		if (s.separator >= map.tracks.size() || s.crossings.empty()) continue;
		for (size_t m = i + 1; m < v.structures.size(); m++) {
			const PlanarStructure &t = v.structures[m];
			if (t.separator != s.separator || t.crossings.empty()) continue;
			const size_t s_first = EventIndex(map.events[s.separator], s.crossings.front()), s_last = EventIndex(map.events[s.separator], s.crossings.back());
			const size_t t_first = EventIndex(map.events[s.separator], t.crossings.front()), t_last = EventIndex(map.events[s.separator], t.crossings.back());
			if (s_first == SIZE_MAX || s_last == SIZE_MAX || t_first == SIZE_MAX || t_last == SIZE_MAX) continue;
			if (s_last + 1 != t_first && t_last + 1 != s_first) continue;
			const size_t merged = s.crossings.size() + t.crossings.size();
			if (((s.kinds | t.kinds) & 1) != 0 && merged > spec.max_bridge_middle) continue;
			if ((s.copy != UINT32_MAX || t.copy != UINT32_MAX) && merged + 2 > 2u * spec.sd + spec.tl) continue;
			out.push_back(fmt::format("P3: not maximal: structures {} and {} on {} are adjacent events and could be one", i, m, map.tracks[s.separator].name));
		}
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
		if (s.limit != StructureLimit(spec, s)) {
			const std::array<int, 2> want = StructureLimit(spec, s);
			out.push_back(fmt::format("P5: structure {} has limit {}/{}, the spec gives {}/{}", i, s.limit[0], s.limit[1], want[0], want[1]));
		}
		const std::string flow = s.copy != UINT32_MAX ? " (with the D228a flow bound 2*sd+tl-2)" : "";
		if ((s.kinds & 1) != 0 && static_cast<int>(s.crossers.size()) > s.limit[0]) out.push_back(fmt::format("P5: structure {} spans {} tracks, over the bridge limit {}{}", i, s.crossers.size(), s.limit[0], flow));
		if ((s.kinds & 2) != 0 && static_cast<int>(s.crossers.size()) > s.limit[1]) out.push_back(fmt::format("P5: structure {} spans {} tracks, over the tunnel limit {}{}", i, s.crossers.size(), s.limit[1], flow));
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
