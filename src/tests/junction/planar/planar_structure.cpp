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
#include <tuple>
#include <utility>
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

/** What wrapping a variant left out or found. */
struct WrapNotes {
	size_t copy_row_crossings = 0; ///< Crossings the map adds for copies and rows, left out of the structures.
	size_t runs = 0; ///< The runs (structures).
	size_t merged = 0; ///< Runs holding both crossings of a channel crossing and crossings in a node group.
	size_t split = 0; ///< Cuts the bridge limit or D228a forced inside a maximal run.
	size_t disagreements = 0; ///< Variants whose map failed with "copy sides disagree".
	size_t other_failures = 0; ///< Variants whose separators or map failed otherwise.
	std::string disagreement_example; ///< The first disagreement, by line name.
	std::string failure_example;
};

/** The track map of a variant, the library's calls in sequence: separators, the sides, the runs, the map. */
static PlanarTrackMap BuildMapOf(const TracksVariant &v, const std::set<uint32_t> &separator_movers)
{
	PlanarTrackMap map;
	const std::vector<uint32_t> separator = PlanarCrossingSeparators(v.topology, v.channels, v.sites, v.over, separator_movers, map.why);
	if (!map.why.empty()) return map;
	const PlanarLineSides sides(v.topology, v.channels);
	return BuildPlanarTrackMap(v.topology, v.channels, v.sites, PlanarGroupStructures(v.topology, v.channels, v.sites, sides, separator, MakeTestJunctionSpec()));
}

/**
 * Wrap a variant with its structures: one per run of the library's grouping, kinds bridge, the map's copy beside the separator when the run has one.
 * Sites are the map's splits and merges.
 */
static PlanarStructureVariant WrapToday(const TracksVariant &v, const std::set<uint32_t> &separator_movers, const std::string &tag, WrapNotes &notes)
{
	PlanarStructureVariant out;
	out.tag = tag;
	out.cost = 0;
	std::string why;
	const std::vector<uint32_t> separator = PlanarCrossingSeparators(v.topology, v.channels, v.sites, v.over, separator_movers, why);
	const PlanarLineSides sides(v.topology, v.channels);
	std::vector<PlanarStructureRun> runs;
	if (why.empty()) runs = PlanarGroupStructures(v.topology, v.channels, v.sites, sides, separator, MakeTestJunctionSpec());
	if (why.empty()) {
		out.map = BuildPlanarTrackMap(v.topology, v.channels, v.sites, runs);
	} else {
		out.map.why = why;
	}
	const PlanarTrackMap &map = out.map;
	if (!map.why.empty()) {
		if (map.why.starts_with("copy sides disagree")) {
			if (notes.disagreements++ == 0) notes.disagreement_example = tag + " " + map.why;
		} else if (notes.other_failures++ == 0) {
			notes.failure_example = tag + " " + map.why;
		}
		return out;
	}
	const auto plain = [&](uint32_t t) { return map.tracks[t].kind == PlanarTrack::Kind::Line; };
	/* The map's crossing between two line tracks that is the topology crossing k. */
	const auto map_crossing = [&](uint32_t k) {
		const PlanarCrossing &c = v.topology.crossings[k];
		for (uint32_t j = 0; j < map.crossings.size(); j++) {
			const uint32_t a = map.crossings[j].a, b = map.crossings[j].b;
			if (!plain(a) || !plain(b)) continue;
			if ((map.part_of[a] == c.a && map.part_of[b] == c.b) || (map.part_of[a] == c.b && map.part_of[b] == c.a)) return j;
		}
		return UINT32_MAX;
	};
	for (uint32_t j = 0; j < map.crossings.size(); j++) {
		if (!plain(map.crossings[j].a) || !plain(map.crossings[j].b)) notes.copy_row_crossings++;
	}
	std::set<uint32_t> channel_crossings;
	for (const PlanarChannelCrossing &cc : v.channels.crossings) channel_crossings.insert(cc.line_crossings.begin(), cc.line_crossings.end());
	for (size_t r = 0; r < runs.size(); r++) {
		const PlanarStructureRun &run = runs[r];
		notes.runs++;
		const size_t in_channel = std::ranges::count_if(run.crossings, [&](uint32_t k) { return channel_crossings.contains(k); });
		if (in_channel != 0 && in_channel != run.crossings.size()) notes.merged++;
		if (r > 0 && runs[r - 1].line == run.line) {
			const std::vector<uint32_t> &events = sides.Events()[run.line];
			if (std::ranges::find(events, runs[r - 1].crossings.back()) + 1 == std::ranges::find(events, run.crossings.front())) notes.split++;
		}
		PlanarStructure st{UINT32_MAX, {}, {}, 1, UINT32_MAX};
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
		out.structures.push_back(st);
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
	h.v = WrapToday(h.tv, {}, "T1o0t0f0", notes);
	REQUIRE(h.v.map.why.empty());
	REQUIRE(!h.v.structures.empty());
	return h;
}

/** @return A structure of one crossing, which a hand mutation can swap sides on. */
static PlanarStructure OneCrossing(const PlanarStructureVariant &v)
{
	for (const PlanarStructure &s : v.structures) {
		if (s.crossings.size() == 1) return s;
	}
	FAIL("no structure of one crossing");
	return {};
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
	 * its crossings: the crossing is then in two structures. No one-field mutation breaks P4 alone; this one breaks only P4, twice, where each structure
	 * is bridge-only over the other (P3 allows it: each track separates one of them).
	 */
	const HandCase h = MakeHandCase();
	PlanarStructureVariant v = h.v;
	const PlanarStructure s = OneCrossing(v);
	PlanarStructure t = s;
	t.separator = s.crossers[0];
	t.crossers = {s.separator};
	t.copy = UINT32_MAX;
	v.structures.push_back(t);
	const std::vector<std::string> ids = Ids(CheckPlanarStructurePromise(h.tv.topology, h.spec, v));
	CHECK(std::ranges::count(ids, "P3") == 0); // Each track separates one of the two structures: allowed since S2a.
	CHECK(std::ranges::count(ids, "P4") == 2);
	CHECK(std::ranges::count(ids, "P4") == static_cast<std::ptrdiff_t>(ids.size()));
}

TEST_CASE("planar structure: P4 a bridge over a tunnel is allowed", "[junction][planar][planarstructure]")
{
	const HandCase h = MakeHandCase();
	PlanarStructureVariant v = h.v;
	const PlanarStructure s = OneCrossing(v);
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
		if (h.v.structures[i].copy != UINT32_MAX && full && with == SIZE_MAX) with = i;
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
	/* The structure lacks its copy, and the copy track is nobody's: two P6 messages. */
	const std::vector<std::string> ids = Ids(CheckPlanarStructurePromise(h.tv.topology, h.spec, v));
	CHECK(ids == std::vector<std::string>{"P6", "P6"});
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
	size_t non_consecutive = 0; ///< Variants with a structure whose crossings are not consecutive events of its separator.
	std::string non_consecutive_example;
	size_t not_maximal_variants = 0; ///< Variants with a "P3: not maximal" message.
	size_t not_maximal_messages = 0; ///< The "P3: not maximal" messages in all variants.
	std::string not_maximal_example;
};

static void Add(Tally &tally, const TracksVariant &tv, const std::set<uint32_t> &separators, const std::string &tag)
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const PlanarStructureVariant v = WrapToday(tv, separators, tag, tally.notes);
	tally.variants++;
	std::set<std::string> seen;
	for (const std::string &m : CheckPlanarStructurePromise(tv.topology, spec, v)) {
		const std::string id = m.substr(0, m.find(':'));
		if (m.find("not maximal") != std::string::npos) {
			if (tally.not_maximal_messages++ == 0) tally.not_maximal_example = tag + " " + m;
			if (seen.insert("P3 not maximal").second) tally.not_maximal_variants++;
		}
		if (m.find("not consecutive") != std::string::npos && seen.insert("P3 non-consecutive").second) {
			tally.non_consecutive++;
			if (tally.non_consecutive_example.empty()) tally.non_consecutive_example = tag + " " + m;
		}
		if (seen.insert(id).second) {
			tally.breaching[id]++;
			if (!tally.first.contains(id)) tally.first[id] = tag + " " + m;
		}
	}
}

static void Print(const std::string &hub, const std::string &form, const Tally &tally)
{
	std::string line = fmt::format("{} {}: {} variants", hub, form, tally.variants);
	for (const char *id : {"P1", "P2", "P3", "P4", "P5", "P6", "P7"}) {
		const auto it = tally.breaching.find(id);
		line += fmt::format(", {}:{}", id, it == tally.breaching.end() ? 0 : it->second);
	}
	line += fmt::format(" | {} runs ({} merged across a channel and a site group, {} P5 cuts), {} copy/row crossings left out, {} copy-side disagreements, {} other map failures", tally.notes.runs, tally.notes.merged, tally.notes.split, tally.notes.copy_row_crossings, tally.notes.disagreements, tally.notes.other_failures);
	WARN(line);
	for (const auto &[id, message] : tally.first) WARN("  first " << message);
	WARN("  P3 not maximal: " << tally.not_maximal_messages << " messages in " << tally.not_maximal_variants << " variants" << (tally.not_maximal_example.empty() ? "" : ", e.g. " + tally.not_maximal_example));
	WARN("  P3 non-consecutive: " << tally.non_consecutive << " variants" << (tally.non_consecutive_example.empty() ? "" : ", e.g. " + tally.non_consecutive_example));
	if (!tally.notes.disagreement_example.empty()) WARN("  copy-side disagreement, e.g. " << tally.notes.disagreement_example);
	if (!tally.notes.failure_example.empty()) WARN("  map failure, e.g. " << tally.notes.failure_example);
}

/** The movers of a variant as a set. */
static std::set<uint32_t> MoverSet(const TracksVariant &tv)
{
	const std::vector<uint32_t> joiners = PlanarMovers(tv.topology, tv.channels, tv.sites);
	return std::set<uint32_t>(joiners.begin(), joiners.end());
}

/** Every variant of a hub: T2, T3 or LL2 (lanes 0). */
template <typename F>
static void ForEachVariant(int lanes, F &&f)
{
	if (lanes != 0) {
		for (uint64_t o = 0; o < 2; o++) {
			for (uint64_t t = 0; t < 4; t++) {
				for (uint64_t fl = 0; fl < 2; fl++) {
					const TracksVariant tv = BuildTracksVariant(PlanarTText(lanes), o, t, fl);
					REQUIRE(tv.ok);
					f(tv, fmt::format("T{}o{}t{}f{}", lanes, o, t, fl));
				}
			}
		}
		return;
	}
	for (uint64_t o = 0; o < 64; o++) {
		for (uint64_t t = 0; t < 8; t++) {
			for (uint64_t fl = 0; fl < 2; fl++) {
				const TracksVariant tv = BuildTracksVariant(PlanarLL2Text(), o, t, fl);
				if (tv.ok) f(tv, fmt::format("LL2o{}t{}f{}", o, t, fl));
			}
		}
	}
}

TEST_CASE("planar structure: today's variants of T2, T3 and LL2 against the promise", "[junction][planar][planarstructure]")
{
	for (const int lanes : {2, 3, 0}) {
		const std::string hub = lanes == 0 ? "LL2" : fmt::format("T{}", lanes);
		Tally none, all, subsets;
		bool first = true;
		ForEachVariant(lanes, [&](const TracksVariant &tv, const std::string &tag) {
			const std::set<uint32_t> joiners = MoverSet(tv);
			Add(none, tv, {}, tag);
			Add(all, tv, joiners, tag);
			if (first) {
				first = false;
				const std::vector<uint32_t> list(joiners.begin(), joiners.end());
				if (list.size() > 8) {
					WARN(hub << ": first variant " << tag << " has " << list.size() << " movers, over 8: subsets not enumerated");
				} else {
					for (uint32_t mask = 0; mask < (1u << list.size()); mask++) {
						std::set<uint32_t> subset;
						for (size_t i = 0; i < list.size(); i++) {
							if ((mask >> i) & 1) subset.insert(list[i]);
						}
						Add(subsets, tv, subset, fmt::format("{}/mask{}", tag, mask));
					}
				}
			}
		});
		REQUIRE(none.variants > 0);
		Print(hub, "separators {}", none);
		Print(hub, "separators all movers", all);
		Print(hub, "first variant, every subset of its movers", subsets);
		for (const Tally *t : {&none, &all, &subsets}) {
			CHECK(t->notes.disagreements == 0);
			CHECK(t->notes.other_failures == 0);
			CHECK(t->breaching.find("P1") == t->breaching.end());
			CHECK(t->not_maximal_messages == 0);
			CHECK(t->breaching.find("P3") == t->breaching.end());
			CHECK(t->breaching.find("P6") == t->breaching.end());
		}
	}
}

TEST_CASE("planar structure: with no separator movers every copy's line has a full share", "[junction][planar][planarstructure]")
{
	for (const int lanes : {2, 3, 0}) {
		size_t copies = 0;
		ForEachVariant(lanes, [&](const TracksVariant &tv, const std::string &tag) {
			const PlanarTrackMap map = BuildMapOf(tv, {});
			REQUIRE(map.why.empty());
			for (const PlanarTrack &track : map.tracks) {
				if (track.kind != PlanarTrack::Kind::Copy) continue;
				copies++;
				INFO(tag << " copy " << track.name);
				CHECK(tv.channels.line_share[track.line] >= 1.0);
			}
		});
		CHECK(copies > 0);
	}
}

TEST_CASE("planar structure: P3 a crossing may be in two structures when each track separates one", "[junction][planar][planarstructure]")
{
	/* The mutation of the P4 test: a second structure over the same crossing with the two tracks swapped; each track is the separator of one, so P3 is silent. */
	const HandCase h = MakeHandCase();
	PlanarStructureVariant v = h.v;
	const PlanarStructure s = OneCrossing(v);
	PlanarStructure t = s;
	t.separator = s.crossers[0];
	t.crossers = {s.separator};
	t.copy = UINT32_MAX;
	t.kinds = 2;
	v.structures.push_back(t);
	const std::vector<std::string> ids = Ids(CheckPlanarStructurePromise(h.tv.topology, h.spec, v));
	CHECK(std::ranges::count(ids, "P3") == 0);
}

TEST_CASE("planar structure: P5 D228a flow bound on a structure with a copy", "[junction][planar][planarstructure]")
{
	HandCase h = MakeHandCase();
	/* The widest structure with a copy has n crossers; a bound of n - 1 crossers (2 * sd + tl - 2 with sd 1) makes it, and only structures as wide, breach. */
	size_t n = 0, as_wide = 0;
	for (const PlanarStructure &s : h.v.structures) {
		if (s.copy != UINT32_MAX) n = std::max(n, s.crossers.size());
	}
	REQUIRE(n >= 2);
	for (const PlanarStructure &s : h.v.structures) {
		if (s.copy != UINT32_MAX && s.crossers.size() == n) as_wide++;
	}
	REQUIRE(as_wide == 1);
	h.spec.sd = 1;
	h.spec.tl = static_cast<uint8_t>(n - 1);
	const std::vector<std::string> messages = CheckPlanarStructurePromise(h.tv.topology, h.spec, h.v);
	size_t d228a = 0;
	for (const std::string &m : messages) {
		if (m.starts_with("P5:") && m.find("D228a") != std::string::npos) d228a++;
	}
	INFO(messages.size() << " messages, first: " << (messages.empty() ? "" : messages[0]));
	CHECK(d228a == 1);
	CHECK(messages.size() == 1);
}

TEST_CASE("planar structure: P6 each copy track is the copy of exactly one structure", "[junction][planar][planarstructure]")
{
	const HandCase h = MakeHandCase();
	PlanarStructureVariant v = h.v;
	/* Two structures naming one copy: find a copy used once, and give it to another structure whose separator's line has the same share class and ends. */
	const PlanarStructure *donor = nullptr;
	for (const PlanarStructure &s : v.structures) {
		if (s.copy != UINT32_MAX) donor = &s;
	}
	REQUIRE(donor != nullptr);
	PlanarStructure twin = *donor;
	twin.crossings.clear();
	twin.crossers.clear();
	v.structures.push_back(twin);
	const std::vector<std::string> ids = Ids(CheckPlanarStructurePromise(h.tv.topology, h.spec, v));
	CHECK(std::ranges::count(ids, "P6") == 1);
}

/** @return How many of the oracle's messages say a structure is not maximal. */
static size_t NotMaximal(const HandCase &h, const PlanarStructureVariant &v)
{
	size_t n = 0;
	for (const std::string &m : CheckPlanarStructurePromise(h.tv.topology, h.spec, v)) n += m.find("not maximal") != std::string::npos ? 1 : 0;
	return n;
}

TEST_CASE("planar structure: P3 two adjacent structures on one separator are not maximal", "[junction][planar][planarstructure]")
{
	/* The first structure of two or more crossings of the hand case is cut into two adjacent ones (they share its copy, a P6 message of its own). */
	const HandCase h = MakeHandCase();
	size_t pick = SIZE_MAX;
	for (size_t i = 0; i < h.v.structures.size() && pick == SIZE_MAX; i++) {
		if (h.v.structures[i].crossings.size() >= 2) pick = i;
	}
	REQUIRE(pick != SIZE_MAX);
	CHECK(NotMaximal(h, h.v) == 0);
	PlanarStructureVariant v = h.v;
	PlanarStructure tail = v.structures[pick];
	PlanarStructure &head = v.structures[pick];
	tail.crossings.assign(head.crossings.begin() + 1, head.crossings.end());
	tail.crossers.assign(head.crossers.begin() + 1, head.crossers.end());
	head.crossings.resize(1);
	head.crossers.resize(1);
	v.structures.push_back(tail);
	CHECK(NotMaximal(h, v) == 1);

	/* Merging would breach the bridge limit: no message. */
	HandCase tight = h;
	tight.spec.max_bridge_middle = static_cast<uint16_t>(h.v.structures[pick].crossings.size() - 1);
	CHECK(NotMaximal(tight, v) == 0);
}
