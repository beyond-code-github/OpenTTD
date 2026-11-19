/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file planar_structure.cpp Test the Structure promise oracle on hand variants, then measure today's variants against it. */

#include "../../../stdafx.h"

#include "../../../3rdparty/catch2/catch.hpp"

#include "../../../core/format.hpp"
#include "../../../junction/junction_spec.h"
#include "../../../junction/planar/planar_gaps.h"
#include "../../../junction/planar/planar_model.h"
#include "../../../junction/planar/planar_paths.h"
#include "../../../junction/planar/planar_plan.h"
#include "../../../junction/planar/planar_regions.h"
#include "../../../junction/planar/planar_structure.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "../../../safeguards.h"

/** A variant, built up to what the track map reads. */
struct TracksVariant {
	PlanarTopology topology;
	PlanarChannels channels;
	std::vector<PlanarResolvedSite> sites;
	std::vector<uint32_t> over;
	bool ok = false; ///< The variant's paths were found.
};

/** Parse the topology, then flip, resolve and path it as the mapped composer does; ok is false when the paths fail. */
static TracksVariant BuildTracksVariant(const std::string &text, uint64_t over_bits, uint64_t ties, uint64_t flips)
{
	TracksVariant v;
	std::string error;
	v.topology = ParsePlanarTopology(text, &error);
	REQUIRE(error.empty());
	v.channels = FlipPlanarTriangles(BuildPlanarChannels(v.topology), flips);
	std::string why;
	v.sites = ResolvePlanarSites(v.topology, v.channels, ties, why);
	REQUIRE(why.empty());
	const PlanarPaths paths = BuildPlanarPaths(v.topology, v.channels, over_bits);
	if (!paths.why.empty()) return v;
	v.over = paths.over;
	v.ok = true;
	return v;
}

/** The two ends of a track in the expanded topology are the same as another's. */
static bool SameEnds(const PlanarTrackMap &map, uint32_t a, uint32_t b)
{
	const PlanarLine &x = map.expanded.lines[a], &y = map.expanded.lines[b];
	return x.from.port == y.from.port && x.from.index == y.from.index && x.to.port == y.to.port && x.to.index == y.to.index;
}

/** @return The Copy track beside a track (same line, same ends), or UINT32_MAX. */
static uint32_t CopyBeside(const PlanarTrackMap &map, uint32_t track)
{
	for (uint32_t i = 0; i < map.tracks.size(); i++) {
		if (map.tracks[i].kind == PlanarTrack::Kind::Copy && map.tracks[i].line == map.tracks[track].line && SameEnds(map, i, track)) return i;
	}
	return UINT32_MAX;
}

/** What wrapping a variant left out. */
struct WrapNotes {
	size_t copy_row_crossings = 0; ///< Crossings the map adds for copies and rows, left out of the structures.
	size_t no_separator = 0; ///< Crossings whose over channel is neither track's.
};

/**
 * Wrap a variant with today's implicit grouping: one structure per crossing between two line tracks, the separator the track on the over channel, kinds bridge, the map's copy beside it.
 * Sites are the map's splits and merges.
 */
static PlanarStructureVariant WrapToday(const TracksVariant &v, bool joiner_over, const std::string &tag, WrapNotes &notes)
{
	PlanarStructureVariant out;
	out.tag = tag;
	out.cost = 0;
	out.map = BuildPlanarTrackMap(v.topology, v.channels, v.sites, v.over, joiner_over);
	const PlanarTrackMap &map = out.map;
	if (!map.why.empty()) return out;
	const auto plain = [&](uint32_t t) { return map.tracks[t].kind == PlanarTrack::Kind::Line; };
	for (uint32_t j = 0; j < map.crossings.size(); j++) {
		const uint32_t a = map.crossings[j].a, b = map.crossings[j].b;
		if (!plain(a) || !plain(b)) {
			notes.copy_row_crossings++;
			continue;
		}
		const uint32_t la = map.part_of[a], lb = map.part_of[b];
		uint32_t over_channel = UINT32_MAX;
		for (uint32_t k = 0; k < v.topology.crossings.size() && over_channel == UINT32_MAX; k++) {
			const PlanarCrossing &c = v.topology.crossings[k];
			if (!((c.a == la && c.b == lb) || (c.a == lb && c.b == la))) continue;
			for (uint32_t ch = 0; ch < v.channels.crossings.size(); ch++) {
				if (std::ranges::find(v.channels.crossings[ch].line_crossings, k) != v.channels.crossings[ch].line_crossings.end()) over_channel = v.over[ch];
			}
		}
		uint32_t separator = UINT32_MAX, crosser = UINT32_MAX;
		if (v.channels.channel_of_line[la] == over_channel) {
			separator = a;
			crosser = b;
		} else if (v.channels.channel_of_line[lb] == over_channel) {
			separator = b;
			crosser = a;
		} else {
			notes.no_separator++;
			continue;
		}
		out.structures.push_back({separator, {j}, {crosser}, 1, CopyBeside(map, separator)});
	}
	out.cost = static_cast<int>(out.structures.size());

	const PlanarTopology &e = map.expanded;
	for (uint32_t n = 0; n < e.nodes.size(); n++) {
		const PlanarNode &node = e.nodes[n];
		const bool split = node.IsSplit();
		const std::vector<uint32_t> &legs = split ? node.out : node.in;
		PlanarStructureSite site{n, split, UINT32_MAX, 0};
		if (n >= v.topology.nodes.size()) {
			/* A copy or row node: the line track continues straight, the other leg is on its side. */
			for (size_t i = 0; i < legs.size(); i++) {
				if (plain(legs[i])) {
					site.straight = legs[i];
					site.side = i == 0 ? +1 : -1;
				}
			}
		} else {
			for (size_t s = 0; s < v.channels.sites.size(); s++) {
				if (std::ranges::find(v.channels.sites[s].nodes, n) == v.channels.sites[s].nodes.end() || v.sites[s].straight == UINT32_MAX) continue;
				for (const uint32_t l : legs) {
					if (v.channels.channel_of_line[map.tracks[l].line] == v.sites[s].straight) site.straight = l;
				}
				site.side = v.sites[s].side;
			}
		}
		out.sites.push_back(site);
	}
	return out;
}

/** @return The promise ids of the messages, in order. */
static std::vector<std::string> Ids(const std::vector<std::string> &messages)
{
	std::vector<std::string> ids;
	for (const std::string &m : messages) ids.push_back(m.substr(0, m.find(':')));
	return ids;
}

/** The hand case: the one-lane T's first variant, wrapped, with the crossings of its track map grouped one per structure. */
struct HandCase {
	TracksVariant tv;
	PlanarStructureVariant v;
	JunctionSpec spec = MakeTestJunctionSpec();
};

static HandCase MakeHandCase()
{
	HandCase h;
	h.tv = BuildTracksVariant(PlanarTText(1), 0, 0, 0);
	REQUIRE(h.tv.ok);
	WrapNotes notes;
	h.v = WrapToday(h.tv, false, "T1o0t0f0", notes);
	REQUIRE(h.v.map.why.empty());
	REQUIRE(!h.v.structures.empty());
	return h;
}

/** Assert the oracle's messages are exactly one, starting with the id. */
static void RequireOnly(const HandCase &h, const PlanarStructureVariant &v, const std::string &id)
{
	const std::vector<std::string> messages = CheckPlanarStructurePromise(h.tv.topology, h.spec, v);
	INFO(messages.size() << " messages, first: " << (messages.empty() ? "" : messages[0]));
	REQUIRE(messages.size() == 1);
	CHECK(messages[0].starts_with(id + ":"));
}

TEST_CASE("planar structure: the oracle accepts a valid hand variant", "[junction][planar][planarstructure]")
{
	const HandCase h = MakeHandCase();
	const std::vector<std::string> messages = CheckPlanarStructurePromise(h.tv.topology, h.spec, h.v);
	INFO(messages.size() << " messages, first: " << (messages.empty() ? "" : messages[0]));
	CHECK(messages.empty());
}

TEST_CASE("planar structure: P1 a track listing a crossing twice", "[junction][planar][planarstructure]")
{
	const HandCase h = MakeHandCase();
	PlanarStructureVariant v = h.v;
	/* A copy or row crossing is not the separator's; take the last event of a track that is no separator. */
	std::set<uint32_t> separators;
	for (const PlanarStructure &s : v.structures) separators.insert(s.separator);
	uint32_t track = UINT32_MAX;
	for (uint32_t t = 0; t < v.map.tracks.size() && track == UINT32_MAX; t++) {
		if (!separators.contains(t) && !v.map.events[t].empty()) track = t;
	}
	REQUIRE(track != UINT32_MAX);
	v.map.events[track].push_back(v.map.events[track].back());
	RequireOnly(h, v, "P1");
}

TEST_CASE("planar structure: P2 a side conflict", "[junction][planar][planarstructure]")
{
	const HandCase h = MakeHandCase();
	PlanarStructureVariant v = h.v;
	v.map.conflicts.emplace_back(0, 0, 1, 0);
	RequireOnly(h, v, "P2");
}

TEST_CASE("planar structure: P3 a crossing in no structure", "[junction][planar][planarstructure]")
{
	const HandCase h = MakeHandCase();
	PlanarStructureVariant v = h.v;
	v.structures.back().crossings.clear();
	v.structures.back().crossers.clear();
	RequireOnly(h, v, "P3");
}

TEST_CASE("planar structure: P4 a bridge over a crosser's structure spanning the same crossing", "[junction][planar][planarstructure]")
{
	/*
	 * Under P3 a structure's crossings are adjacent events of its separator, so a crossing of another structure can lie within its span only by being one of
	 * its crossings: the crossing is then in two structures. No one-field mutation breaks P4 alone; this one breaks P3 once and P4 where each structure
	 * is bridge-only over the other.
	 */
	const HandCase h = MakeHandCase();
	PlanarStructureVariant v = h.v;
	const PlanarStructure s = v.structures.front();
	PlanarStructure t = s;
	t.separator = s.crossers[0];
	t.crossers = {s.separator};
	t.copy = UINT32_MAX;
	v.structures.push_back(t);
	const std::vector<std::string> ids = Ids(CheckPlanarStructurePromise(h.tv.topology, h.spec, v));
	CHECK(std::ranges::count(ids, "P3") == 1);
	CHECK(std::ranges::count(ids, "P4") == 2);
	CHECK(std::ranges::count(ids, "P3") + std::ranges::count(ids, "P4") == static_cast<std::ptrdiff_t>(ids.size()));
}

TEST_CASE("planar structure: P4 a bridge over a tunnel is allowed", "[junction][planar][planarstructure]")
{
	const HandCase h = MakeHandCase();
	PlanarStructureVariant v = h.v;
	const PlanarStructure s = v.structures.front();
	PlanarStructure t = s;
	t.separator = s.crossers[0];
	t.crossers = {s.separator};
	t.copy = UINT32_MAX;
	t.kinds = 2;
	v.structures.push_back(t);
	const std::vector<std::string> ids = Ids(CheckPlanarStructurePromise(h.tv.topology, h.spec, v));
	CHECK(std::ranges::count(ids, "P4") == 0);
}

TEST_CASE("planar structure: P5 no kind, and a span over the limit", "[junction][planar][planarstructure]")
{
	const HandCase h = MakeHandCase();
	PlanarStructureVariant v = h.v;
	v.structures.front().kinds = 0;
	RequireOnly(h, v, "P5");

	HandCase low = h;
	low.spec.max_bridge_middle = 0;
	const std::vector<std::string> messages = CheckPlanarStructurePromise(low.tv.topology, low.spec, low.v);
	REQUIRE(!messages.empty());
	CHECK(std::ranges::all_of(Ids(messages), [](const std::string &id) { return id == "P5"; }));
	CHECK(messages.size() == low.v.structures.size());
}

TEST_CASE("planar structure: P6 a copy where the share is not full, and a copy missing", "[junction][planar][planarstructure]")
{
	const HandCase h = MakeHandCase();
	/* A structure without a copy given the copy of another separator's line... take one that has a copy and one that has none. */
	const PlanarChannels channels = BuildPlanarChannels(h.tv.topology);
	size_t with = SIZE_MAX, without = SIZE_MAX;
	for (size_t i = 0; i < h.v.structures.size(); i++) {
		const bool full = channels.line_share[h.v.map.tracks[h.v.structures[i].separator].line] >= 1.0;
		if (h.v.structures[i].copy != UINT32_MAX && full && with == SIZE_MAX) {
			/* Only a structure whose copy another structure also holds leaves no unused copy track behind. */
			for (size_t k = 0; k < h.v.structures.size(); k++) {
				if (k != i && h.v.structures[k].copy == h.v.structures[i].copy) with = i;
			}
		}
		if (h.v.structures[i].copy == UINT32_MAX && !full && without == SIZE_MAX) without = i;
	}
	uint32_t some_copy = UINT32_MAX;
	for (const PlanarStructure &s : h.v.structures) {
		if (s.copy != UINT32_MAX) some_copy = s.copy;
	}
	REQUIRE(some_copy != UINT32_MAX);
	REQUIRE(without != SIZE_MAX);
	PlanarStructureVariant v = h.v;
	v.structures[without].copy = some_copy;
	RequireOnly(h, v, "P6");

	REQUIRE(with != SIZE_MAX);
	v = h.v;
	v.structures[with].copy = UINT32_MAX;
	RequireOnly(h, v, "P6");
}

TEST_CASE("planar structure: P7 a split after a merge on a route path", "[junction][planar][planarstructure]")
{
	const std::string text =
		"topology merge_split\n"
		"port W in E: A1\n"
		"port S in N: B1\n"
		"port E out E: A1a\n"
		"port N out N: B1b\n"
		"node A1, B1 -> AB1\n"
		"node AB1 -> A1a, B1b\n";
	/* The site resolver refuses this topology (its merge and split share one channel), so the variant is the empty map: no tracks, crossings or structures, which every other promise accepts. */
	HandCase h;
	std::string error;
	h.tv.topology = ParsePlanarTopology(text, &error);
	REQUIRE(error.empty());
	REQUIRE(h.tv.topology.crossings.empty());
	/* One message per route path: both routes run through the merged line and split after it. */
	const std::vector<std::string> ids = Ids(CheckPlanarStructurePromise(h.tv.topology, h.spec, h.v));
	CHECK(ids == std::vector<std::string>{"P7", "P7"});
}

/** The tally of one hub in one form. */
struct Tally {
	size_t variants = 0;
	std::map<std::string, size_t> breaching; ///< Variants breaching each promise.
	std::map<std::string, std::string> first; ///< The first message of each promise.
	WrapNotes notes;
};

static void Add(Tally &tally, const TracksVariant &tv, bool joiner_over, const std::string &tag)
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const PlanarStructureVariant v = WrapToday(tv, joiner_over, tag, tally.notes);
	tally.variants++;
	std::set<std::string> seen;
	for (const std::string &m : CheckPlanarStructurePromise(tv.topology, spec, v)) {
		const std::string id = m.substr(0, m.find(':'));
		if (seen.insert(id).second) {
			tally.breaching[id]++;
			if (!tally.first.contains(id)) tally.first[id] = tag + " " + m;
		}
	}
}

static void Print(const std::string &hub, bool joiner_over, const Tally &tally)
{
	std::string line = fmt::format("{} joiner_over={}: {} variants", hub, joiner_over, tally.variants);
	for (const char *id : {"P1", "P2", "P3", "P4", "P5", "P6", "P7"}) {
		const auto it = tally.breaching.find(id);
		line += fmt::format(", {}:{}", id, it == tally.breaching.end() ? 0 : it->second);
	}
	line += fmt::format(" | left out of structures: {} copy/row crossings, {} without separator", tally.notes.copy_row_crossings, tally.notes.no_separator);
	WARN(line);
	for (const auto &[id, message] : tally.first) WARN("  first " << message);
}

TEST_CASE("planar structure: today's variants of T2, T3 and LL2 against the promise", "[junction][planar][planarstructure]")
{
	for (const bool joiner_over : {false, true}) {
		for (const int lanes : {2, 3}) {
			Tally tally;
			size_t built = 0;
			for (uint64_t o = 0; o < 2; o++) {
				for (uint64_t t = 0; t < 4; t++) {
					for (uint64_t f = 0; f < 2; f++) {
						const TracksVariant tv = BuildTracksVariant(PlanarTText(lanes), o, t, f);
						REQUIRE(tv.ok);
						built++;
						Add(tally, tv, joiner_over, fmt::format("T{}o{}t{}f{}", lanes, o, t, f));
					}
				}
			}
			REQUIRE(built == tally.variants);
			Print(fmt::format("T{}", lanes), joiner_over, tally);
		}
		Tally tally;
		for (uint64_t o = 0; o < 64; o++) {
			for (uint64_t t = 0; t < 8; t++) {
				for (uint64_t f = 0; f < 2; f++) {
					const TracksVariant tv = BuildTracksVariant(PlanarLL2Text(), o, t, f);
					if (!tv.ok) continue;
					Add(tally, tv, joiner_over, fmt::format("LL2o{}t{}f{}", o, t, f));
				}
			}
		}
		REQUIRE(tally.variants > 0);
		Print("LL2", joiner_over, tally);
	}
}
