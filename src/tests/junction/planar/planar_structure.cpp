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
#include <chrono>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
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
 * Sites are the map's splits and merges. The notes count what the wrap left out or found.
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
	}
	out.structures = PlanarBuildStructures(v.topology, runs, map, MakeTestJunctionSpec());
	out.side_gap = PlanarSideGaps(MakeTestJunctionSpec());
	out.rules = PlanarRuleFieldsOf(MakeTestJunctionSpec());
	out.cost = static_cast<int>(out.structures.size());
	out.sites = PlanarBuildStructureSites(v.topology, v.channels, v.sites, map);
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

/** @return The limits of a structure for the spec, by the formula of the limit's doc (a hand mutation of its kinds or copy changes them). */
static std::array<int, 2> LimitFor(const JunctionSpec &spec, const PlanarStructure &s)
{
	const int flow = 2 * spec.sd + spec.tl - 2;
	std::array<int, 2> limit{-1, -1};
	if ((s.kinds & 1) != 0) limit[0] = s.copy != UINT32_MAX ? std::min<int>(spec.max_bridge_middle, flow) : spec.max_bridge_middle;
	if ((s.kinds & 2) != 0) limit[1] = s.copy != UINT32_MAX ? std::min<int>(spec.max_tunnel_length, flow) : spec.max_tunnel_length;
	return limit;
}

/** Rebuild a hand case's structures' limits for its (changed) spec. */
static void RefreshLimits(HandCase &h)
{
	for (PlanarStructure &s : h.v.structures) s.limit = LimitFor(h.spec, s);
	h.v.rules = PlanarRuleFieldsOf(h.spec);
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

TEST_CASE("planar structure: P4 a tunnel under a tunnel is allowed (D390)", "[junction][planar][planarstructure]")
{
	const HandCase h = MakeHandCase();
	PlanarStructureVariant v = h.v;
	const auto one = std::ranges::find_if(v.structures, [](const PlanarStructure &x) { return x.crossings.size() == 1; });
	REQUIRE(one != v.structures.end());
	one->kinds = 2;
	const PlanarStructure s = *one;
	PlanarStructure t = s;
	t.separator = s.crossers[0];
	t.crossers = {s.separator};
	t.copy = UINT32_MAX;
	v.structures.push_back(t);
	const std::vector<std::string> ids = Ids(CheckPlanarStructurePromise(h.tv.topology, h.spec, v));
	CHECK(std::ranges::count(ids, "P4") == 0);
}

TEST_CASE("planar structure: P5 no kind, and a span over the limit", "[junction][planar][planarstructure]")
{
	const HandCase h = MakeHandCase();
	PlanarStructureVariant v = h.v;
	v.structures.front().kinds = 0;
	v.structures.front().limit = LimitFor(h.spec, v.structures.front());
	RequireOnly(h, v, "P5");

	HandCase low = h;
	low.spec.max_bridge_middle = 0;
	RefreshLimits(low);
	const std::vector<std::string> messages = CheckPlanarStructurePromise(low.tv.topology, low.spec, low.v);
	REQUIRE(!messages.empty());
	CHECK(std::ranges::all_of(Ids(messages), [](const std::string &id) { return id == "P5"; }));
	CHECK(messages.size() == low.v.structures.size());
}

TEST_CASE("planar structure: P5 a structure's limit that is not the spec's", "[junction][planar][planarstructure]")
{
	const HandCase h = MakeHandCase();
	PlanarStructureVariant v = h.v;
	v.structures.front().limit[0] += 1;
	RequireOnly(h, v, "P5");
	CHECK(CheckPlanarStructurePromise(h.tv.topology, h.spec, v)[0].find("limit") != std::string::npos);
}

TEST_CASE("planar structure: P8 a side gap that is not the spec's", "[junction][planar][planarstructure]")
{
	const HandCase h = MakeHandCase();
	PlanarStructureVariant v = h.v;
	v.side_gap[static_cast<size_t>(PlanarSide::E)] += 1;
	RequireOnly(h, v, "P8");
	CHECK(CheckPlanarStructurePromise(h.tv.topology, h.spec, v)[0].find("side gap") != std::string::npos);
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
	v.structures[without].limit = LimitFor(h.spec, v.structures[without]);
	RequireOnly(h, v, "P6");

	REQUIRE(with != SIZE_MAX);
	v = h.v;
	v.structures[with].copy = UINT32_MAX;
	v.structures[with].limit = LimitFor(h.spec, v.structures[with]);
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
	h.v.side_gap = PlanarSideGaps(h.spec);
	h.v.rules = PlanarRuleFieldsOf(h.spec);
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
	RefreshLimits(h);
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

/** @return The tags of a hub's variants, for messages. */
static std::string Describe(const PlanarStructureResult &r)
{
	std::string out;
	for (const auto &[reason, n] : r.skipped) out += fmt::format("[{} x {}] ", n, reason);
	return out.empty() ? "none" : out;
}

TEST_CASE("planar structure: BuildPlanarStructure on T2, T3 and LL2 hands down ranked variants that keep the promise", "[junction][planar][planarstructure]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	for (const int lanes : {2, 3, 0}) {
		const std::string hub = lanes == 0 ? "LL2" : fmt::format("T{}", lanes);
		std::string error;
		const PlanarTopology topology = ParsePlanarTopology(lanes == 0 ? PlanarLL2Text() : PlanarTText(lanes), &error);
		REQUIRE(error.empty());
		const PlanarChannels channels = BuildPlanarChannels(topology);
		const auto t0 = std::chrono::steady_clock::now();
		const PlanarStructureResult result = BuildPlanarStructure(topology, spec, 16);
		const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - t0).count();
		INFO(hub << " why: " << result.why);
		REQUIRE(!result.variants.empty());
		size_t breaches = 0;
		std::string first_breach;
		for (size_t i = 0; i < result.variants.size(); i++) {
			const PlanarStructureVariant &v = result.variants[i];
			const std::vector<std::string> messages = CheckPlanarStructurePromise(topology, spec, v);
			breaches += messages.size();
			if (!messages.empty() && first_breach.empty()) first_breach = v.tag + " " + messages[0];
			if (i > 0) CHECK(result.variants[i - 1].cost <= v.cost);
		}
		INFO(first_breach);
		CHECK(breaches == 0);
		size_t ties = 0;
		for (const PlanarSite &site : channels.sites) {
			if (site.kind != PlanarSiteKind::LaneSplit && site.kind != PlanarSiteKind::LaneMerge && site.others.size() == 2) {
				ties += site.straight == UINT32_MAX ? 1 : 0;
				continue;
			}
			for (const uint32_t n : site.nodes) ties += !topology.nodes[n].IsSplit() && channels.node_legs[n].straight == UINT32_MAX ? 1 : 0;
		}
		std::string tie_why;
		const std::vector<PlanarResolvedSite> sites0 = ResolvePlanarSites(topology, channels, 0, tie_why);
		const size_t movers0 = tie_why.empty() ? PlanarMovers(topology, channels, sites0).size() : 0;
		WARN(hub << ": bits over " << channels.crossings.size() << " ties " << ties << " flips " << PlanarTriangles(channels).size() << " movers(t0f0) " << movers0 << ", " << (result.exhaustive ? "exhaustive" : "best-improvement") << ", evaluated " << result.evaluated << ", skipped " << Describe(result) << ", kept " << result.variants.size() << ", best cost " << result.variants.front().cost << " (" << result.variants.front().tag << "), worst cost " << result.variants.back().cost << ", " << micros << " us");
		CHECK(micros < 5'000'000);
	}
}

TEST_CASE("planar structure: PlanarChannelOver bit 0 is today's default, a set bit flips exactly its crossing", "[junction][planar][planarstructure]")
{
	std::string error;
	const PlanarTopology topology = ParsePlanarTopology(PlanarTText(2), &error);
	REQUIRE(error.empty());
	const PlanarChannels channels = BuildPlanarChannels(topology);
	REQUIRE(!channels.crossings.empty());
	/* The oracle of today's default, in the test only. */
	CHECK(PlanarChannelOver(channels, 0) == BuildPlanarPaths(topology, channels, 0).over);
	const std::vector<uint32_t> base = PlanarChannelOver(channels, 0);
	for (uint32_t x = 0; x < channels.crossings.size(); x++) {
		const std::vector<uint32_t> one = PlanarChannelOver(channels, uint64_t{1} << x);
		for (uint32_t y = 0; y < channels.crossings.size(); y++) CHECK((one[y] != base[y]) == (x == y));
	}
}

TEST_CASE("planar structure: the cost of T2 o0t0f0m0 is its runs plus its copies", "[junction][planar][planarstructure]")
{
	std::string error;
	const PlanarTopology topology = ParsePlanarTopology(PlanarTText(2), &error);
	REQUIRE(error.empty());
	const PlanarStructureResult result = BuildPlanarStructure(topology, MakeTestJunctionSpec(), 100000);
	const PlanarStructureVariant *found = nullptr;
	for (const PlanarStructureVariant &v : result.variants) {
		if (v.tag == "o0t0f0m0") found = &v;
	}
	REQUIRE(found != nullptr);
	int copies = 0;
	for (const PlanarStructure &s : found->structures) copies += s.copy != UINT32_MAX ? 1 : 0;
	CHECK(found->cost == static_cast<int>(found->structures.size()) + copies);
}

TEST_CASE("planar structure: T3 o3t0f0m3294 splits the structure on D1 so every piece keeps P5", "[junction][planar][planarstructure]")
{
	TracksVariant tv = BuildTracksVariant(PlanarTText(3), 0, 0, 0);
	REQUIRE(tv.ok);
	tv.over = PlanarChannelOver(tv.channels, 3);
	const std::vector<uint32_t> movers = PlanarMovers(tv.topology, tv.channels, tv.sites);
	std::set<uint32_t> separators;
	for (size_t i = 0; i < movers.size(); i++) {
		if ((3294 >> i & 1) != 0) separators.insert(movers[i]);
	}
	WrapNotes notes;
	const PlanarStructureVariant v = WrapToday(tv, separators, "T3o3t0f0m3294", notes);
	REQUIRE(v.map.why.empty());
	size_t on_d1 = 0;
	for (const PlanarStructure &s : v.structures) on_d1 += v.map.tracks[s.separator].name == "D1" ? 1 : 0;
	CHECK(on_d1 >= 2);
	const std::vector<std::string> messages = CheckPlanarStructurePromise(tv.topology, MakeTestJunctionSpec(), v);
	INFO(messages.size() << " messages, first: " << (messages.empty() ? std::string() : messages[0]));
	CHECK(messages.empty());
}

/** Parse and require success. */
static PlanarTopology Parse(const std::string &text)
{
	std::string error;
	PlanarTopology topology = ParsePlanarTopology(text, &error);
	INFO(error);
	REQUIRE(error.empty());
	REQUIRE(!topology.lines.empty());
	return topology;
}

/** The text with the given `cross` statements appended. */
static std::string WithCross(const std::string &text, const std::vector<std::pair<std::string, std::vector<std::string>>> &statements)
{
	std::string out = text + "\n";
	for (const auto &[name, partners] : statements) {
		out += "cross " + name + ":";
		for (size_t i = 0; i < partners.size(); i++) out += (i == 0 ? " " : ", ") + partners[i];
		out += "\n";
	}
	return out;
}

/** The text plus, for every line with crossings, the `cross` statement of its derived event order. */
static std::string AuthoredText(const std::string &text)
{
	const PlanarTopology t = Parse(text);
	const PlanarLineSides sides(t, BuildPlanarChannels(t));
	std::vector<std::pair<std::string, std::vector<std::string>>> statements;
	for (uint32_t l = 0; l < t.lines.size(); l++) {
		if (t.events[l].empty()) continue;
		std::vector<std::string> partners;
		for (uint32_t c : t.events[l]) {
			const uint32_t m = t.crossings[c].a == l ? t.crossings[c].b : t.crossings[c].a;
			partners.push_back(t.lines[m].name + (sides.CrossingFrom(c, m) == +1 ? ">" : "<"));
		}
		statements.push_back({t.lines[l].name, std::move(partners)});
	}
	return WithCross(text, statements);
}

/** @return The whole text of a file of the topologies directory. */
static std::string ReadTopologyFile(const std::string &name)
{
	std::ifstream file(std::string(JUNCTION_TOPOLOGY_DIR "/") + name);
	REQUIRE(file.good());
	std::stringstream buffer;
	buffer << file.rdbuf();
	return buffer.str();
}

TEST_CASE("planar structure: P9 a rule field that is not the spec's", "[junction][planar][planarstructure]")
{
	const HandCase h = MakeHandCase();
	PlanarStructureVariant v = h.v;
	v.rules.bay += 1;
	RequireOnly(h, v, "P9");
	CHECK(CheckPlanarStructurePromise(h.tv.topology, h.spec, v)[0].find("bay") != std::string::npos);
}

TEST_CASE("planar structure: Structure hands down the spec's rule fields", "[junction][planar][planarstructure]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	std::string error;
	const PlanarTopology topology = ParsePlanarTopology(PlanarTText(2), &error);
	REQUIRE(error.empty());
	const PlanarStructureResult r = BuildPlanarStructure(topology, spec, 16);
	REQUIRE(!r.variants.empty());
	const int cl = static_cast<int>(DeriveCurveSpacing(spec.reference_speed, spec.curve_speed, spec.tl));
	for (const PlanarStructureVariant &v : r.variants) {
		CHECK(v.rules == PlanarRuleFieldsOf(spec));
		CHECK(v.rules.tl == 4);
		CHECK(v.rules.bay == 12);
		CHECK(v.rules.coverage == 32);
		CHECK(v.rules.curve_same == std::min(7, std::max(2, cl)));
	}
}

TEST_CASE("planar structure: PSG 197's pinned variant, gaps and promise", "[junction][planar][planarstructure]")
{
	std::string error;
	const PlanarTopology topology = ParsePlanarTopology(ReadTopologyFile("psg197.txt"), &error);
	REQUIRE(error.empty());
	const JunctionSpec spec = MakeTestJunctionSpec();
	std::vector<std::string> report;
	const PlanarStructureVariant v = LoadPlanarStructureVariant(topology, ReadTopologyFile("psg197.structure.txt"), spec, report, &error);
	INFO(error);
	REQUIRE(error.empty());
	size_t gaps = 0, differs = 0;
	for (const std::string &line : report) {
		WARN(line);
		(line.starts_with("gap:") ? gaps : differs)++;
	}
	const std::vector<std::string> promise = CheckPlanarStructurePromise(topology, spec, v);
	for (const std::string &line : promise) WARN(line);
	WARN("tracks " << v.map.tracks.size() << ", structures " << v.structures.size() << ", sites " << v.sites.size() << ", gap lines " << gaps << ", differs lines " << differs << ", promise breaches " << promise.size());
}

TEST_CASE("planar structure: PSG 197's pinned variant keeps the promise", "[.][planarpsg197]")
{
	std::string error;
	const PlanarTopology topology = ParsePlanarTopology(ReadTopologyFile("psg197.txt"), &error);
	REQUIRE(error.empty());
	const JunctionSpec spec = MakeTestJunctionSpec();
	std::vector<std::string> report;
	const PlanarStructureVariant v = LoadPlanarStructureVariant(topology, ReadTopologyFile("psg197.structure.txt"), spec, report, &error);
	REQUIRE(error.empty());
	for (const std::string &line : report) CHECK(!line.starts_with("gap:"));
	CHECK(v.rules == PlanarRuleFieldsOf(spec));
	const std::vector<std::string> promise = CheckPlanarStructurePromise(topology, spec, v);
	INFO(promise.size() << " breaches, first: " << (promise.empty() ? std::string() : promise[0]));
	CHECK(promise.empty());
}

/** @return A variant's decisions as text: a tunnel per structure, a straight per topology node. */
static std::string DecisionsText(const PlanarTopology &topology, const PlanarStructureVariant &v)
{
	const auto name = [&](uint32_t track) { return topology.lines[v.map.tracks[track].line].name; };
	std::string out;
	for (size_t i = 0; i < v.structures.size(); i++) {
		const PlanarStructure &st = v.structures[i];
		out += fmt::format("tunnel T{} {}:", i + 1, name(st.separator));
		for (size_t k = 0; k < st.crossers.size(); k++) out += fmt::format("{} {}", k == 0 ? "" : ",", name(st.crossers[k]));
		out += "\n";
		if (st.copy != UINT32_MAX) out += fmt::format("copy {} over T{}\n", name(st.separator), i + 1);
	}
	for (uint32_t n = 0; n < topology.nodes.size(); n++) {
		const PlanarNode &node = topology.nodes[n];
		out += fmt::format("straight {}: {}\n", topology.lines[node.IsSplit() ? node.in[0] : node.out[0]].name, v.sites[n].straight == UINT32_MAX ? std::string("none") : name(v.sites[n].straight));
	}
	return out;
}

TEST_CASE("planar structure: a pinned variant equal to Structure's own gives its structures and sites", "[junction][planar][planarstructure]")
{
	const PlanarTopology topology = Parse(AuthoredText(PlanarTText(1)));
	const JunctionSpec spec = MakeTestJunctionSpec();
	const PlanarStructureResult result = BuildPlanarStructure(topology, spec, 1);
	REQUIRE(!result.variants.empty());
	const PlanarStructureVariant &v0 = result.variants[0];
	std::vector<std::string> report;
	std::string error;
	const PlanarStructureVariant v = LoadPlanarStructureVariant(topology, DecisionsText(topology, v0), spec, report, &error);
	INFO(error);
	REQUIRE(error.empty());
	INFO("report: " << (report.empty() ? std::string() : report[0]));
	CHECK(report.empty());
	REQUIRE(v.structures.size() == v0.structures.size());
	const auto line = [&](const PlanarStructureVariant &x, uint32_t track) { return x.map.tracks[track].line; };
	for (size_t i = 0; i < v0.structures.size(); i++) {
		const PlanarStructure &a = v0.structures[i], &b = v.structures[i];
		CHECK(line(v0, a.separator) == line(v, b.separator));
		REQUIRE(a.crossers.size() == b.crossers.size());
		for (size_t k = 0; k < a.crossers.size(); k++) CHECK(line(v0, a.crossers[k]) == line(v, b.crossers[k]));
		CHECK((a.copy != UINT32_MAX) == (b.copy != UINT32_MAX));
		CHECK(b.kinds == 2);
	}
	REQUIRE(v.sites.size() == v0.sites.size());
	for (size_t n = 0; n < v0.sites.size(); n++) {
		CHECK(v.sites[n].straight == v0.sites[n].straight);
		CHECK(v.sites[n].side == v0.sites[n].side);
	}
}

TEST_CASE("planar structure: pinned decisions are refused with a reason", "[junction][planar][planarstructure]")
{
	const PlanarTopology topology = Parse(AuthoredText(PlanarTText(1)));
	const JunctionSpec spec = MakeTestJunctionSpec();
	const PlanarStructureResult result = BuildPlanarStructure(topology, spec, 1);
	REQUIRE(!result.variants.empty());
	const std::string text = DecisionsText(topology, result.variants[0]);
	const auto error_of = [&](const PlanarTopology &t, const std::string &decisions) {
		std::vector<std::string> report;
		std::string error;
		LoadPlanarStructureVariant(t, decisions, spec, report, &error);
		return error;
	};
	SECTION("the topology is not authored") {
		CHECK(error_of(Parse(PlanarTText(1)), text).find("not authored") != std::string::npos);
	}
	SECTION("an unknown statement") {
		CHECK(error_of(topology, text + "bridge B1 A1\n").find("unknown") != std::string::npos);
	}
	SECTION("a tunnel naming a non-crossing pair") {
		REQUIRE(!topology.lines.empty());
		CHECK(error_of(topology, text + "tunnel T99 " + topology.lines[0].name + ": " + topology.lines[0].name + "\n").find("does not cross") != std::string::npos);
	}
	SECTION("a straight statement removed") {
		const size_t at = text.rfind("straight ");
		REQUIRE(at != std::string::npos);
		CHECK(error_of(topology, text.substr(0, at)).find("has no straight statement") != std::string::npos);
	}
}
