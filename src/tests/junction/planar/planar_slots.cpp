/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file planar_slots.cpp Test the slot layer: the sideways slots of each channel's tracks, from the topology and the variant. */

#include "../../../stdafx.h"

#include "../../../3rdparty/catch2/catch.hpp"

#include "../../../core/format.hpp"
#include "../../../junction/planar/planar_gaps.h"
#include "../../../junction/planar/planar_paths.h"
#include "../../../junction/planar/planar_plan.h"
#include "../../../junction/planar/planar_slots.h"

#include <algorithm>
#include <chrono>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "../../../safeguards.h"

/** A variant, built up to its slots. */
struct SlotsVariant {
	PlanarTopology topology;
	PlanarChannels channels;
	std::vector<PlanarResolvedSite> sites;
	std::vector<uint32_t> over;
	PlanarSlots slots;
};

/** Parse the topology, then flip, resolve and path it as the mapped composer does, and build its slots. */
static SlotsVariant BuildVariant(const std::string &text, uint64_t over_bits, uint64_t ties, uint64_t flips)
{
	SlotsVariant v;
	std::string error;
	v.topology = ParsePlanarTopology(text, &error);
	REQUIRE(error.empty());
	v.channels = FlipPlanarTriangles(BuildPlanarChannels(v.topology), flips);
	std::string why;
	v.sites = ResolvePlanarSites(v.topology, v.channels, ties, why);
	REQUIRE(why.empty());
	REQUIRE(v.sites.size() == v.channels.sites.size());
	const PlanarPaths paths = BuildPlanarPaths(v.topology, v.channels, over_bits);
	REQUIRE(paths.why.empty());
	v.over = paths.over;
	v.slots = BuildPlanarSlots(v.topology, v.channels, v.sites, v.over);
	return v;
}

/** The slot of a line's own track in an interval. */
static int LineSlot(const PlanarSlotInterval &interval, uint32_t line)
{
	for (const PlanarSlotTrack &track : interval.tracks) {
		if (track.kind == PlanarSlotKind::Line && track.line == line) return track.slot;
	}
	FAIL("the line has no track in the interval");
	return 0;
}

/** A line's net step: its steps between consecutive intervals plus the step of the site link into it. */
static int NetStep(const SlotsVariant &v, uint32_t line)
{
	const std::vector<PlanarSlotInterval> &intervals = v.slots.channels[v.channels.channel_of_line[line]];
	int step = LineSlot(intervals.back(), line) - LineSlot(intervals.front(), line);
	for (const PlanarSlotLink &link : v.slots.links) {
		if (link.to == line) step += link.step;
	}
	return step;
}

/** Check the gap change of each pair of neighbouring lines, named by their lines. */
static void CheckGapChanges(const SlotsVariant &v, double objective, const std::vector<std::pair<std::pair<std::string, std::string>, int>> &expected)
{
	INFO(v.slots.why);
	REQUIRE(v.slots.why.empty());
	CHECK(v.slots.objective == Approx(objective));
	REQUIRE(v.slots.channels.size() == v.channels.channels.size());
	size_t pairs = 0;
	for (const PlanarChannel &channel : v.channels.channels) pairs += channel.lines.size() - 1;
	CHECK(expected.size() == pairs);
	for (const auto &[names, gap] : expected) {
		INFO(names.first << "/" << names.second);
		const uint32_t left = PlanarLineByName(v.topology, names.first);
		const uint32_t right = PlanarLineByName(v.topology, names.second);
		REQUIRE(left != UINT32_MAX);
		REQUIRE(right != UINT32_MAX);
		const uint32_t channel = v.channels.channel_of_line[left];
		REQUIRE(v.channels.channel_of_line[right] == channel);
		const std::vector<uint32_t> &lines = v.channels.channels[channel].lines;
		const auto at = std::ranges::find(lines, left);
		REQUIRE(at + 1 != lines.end());
		REQUIRE(*(at + 1) == right);
		CHECK(NetStep(v, right) - NetStep(v, left) == gap);
	}
}

TEST_CASE("planar slots: T2 o1t3f1s0 gap changes", "[junction][planar]")
{
	const SlotsVariant v = BuildVariant(PlanarTText(2), 1, 3, 1);
	CheckGapChanges(v, 4.0041, {
		{{"EF1", "EF2"}, +1}, {{"CB1", "CB2"}, 0}, {{"ED1", "ED2"}, 0}, {{"AB1", "AB2"}, +1},
		{{"CD1", "CD2"}, 0}, {{"AF1", "AF2"}, 0}, {{"A1", "A2"}, 0}, {{"B1", "B2"}, -1},
		{{"C1", "C2"}, 0}, {{"D1", "D2"}, 0}, {{"E1", "E2"}, -1}, {{"F1", "F2"}, 0},
	});
}

TEST_CASE("planar slots: LL² o17t0f1s0 gap changes", "[junction][planar]")
{
	const SlotsVariant v = BuildVariant(PlanarLL2Text(), 17, 0, 1);
	CheckGapChanges(v, 3.0054, {
		{{"EF1", "EF2"}, +1}, {{"WO1", "WO2"}, 0}, {{"EO1", "EO2"}, 0}, {{"AB1", "AB2"}, +1},
		{{"CD1", "CD2"}, +1}, {{"SO1", "SO2"}, 0}, {{"E1", "E2"}, -1}, {{"F1", "F2"}, +2},
		{{"A1", "A2"}, 0}, {{"B1", "B2"}, +1}, {{"C1", "C2"}, 0}, {{"D1", "D2"}, 0},
		{{"EDj1", "EDj2"}, 0}, {{"CBj1", "CBj2"}, 0}, {{"AFj1", "AFj2"}, 0},
	});
}

TEST_CASE("planar slots: a doubled line keeps its slot over consecutive doubled crossings", "[junction][planar]")
{
	size_t pairs = 0;
	for (const SlotsVariant &v : {BuildVariant(PlanarTText(2), 1, 3, 1), BuildVariant(PlanarLL2Text(), 17, 0, 1)}) {
		INFO(v.topology.name);
		REQUIRE(v.slots.why.empty());
		for (const std::vector<PlanarSlotInterval> &intervals : v.slots.channels) {
			for (size_t iv = 0; iv + 1 < intervals.size(); iv++) {
				for (const PlanarSlotTrack &track : intervals[iv].tracks) {
					if (track.kind != PlanarSlotKind::Copy) continue;
					bool next_copy = false;
					for (const PlanarSlotTrack &next : intervals[iv + 1].tracks) next_copy = next_copy || (next.kind == PlanarSlotKind::Copy && next.line == track.line);
					if (!next_copy) continue;
					pairs++;
					INFO(intervals[iv].name << " line " << track.line);
					CHECK(LineSlot(intervals[iv + 1], track.line) == LineSlot(intervals[iv], track.line));
				}
			}
		}
	}
	CHECK(pairs > 0);
}

TEST_CASE("planar slots: the copy runs on between consecutive doubled crossings", "[junction][planar]")
{
	size_t found = 0;
	for (const SlotsVariant &v : {BuildVariant(PlanarTText(2), 1, 3, 1), BuildVariant(PlanarLL2Text(), 17, 0, 1)}) {
		INFO(v.topology.name);
		REQUIRE(v.slots.why.empty());
		for (uint32_t c = 0; c < v.channels.channels.size(); c++) {
			const PlanarChannel &cc = v.channels.channels[c];
			const std::vector<PlanarSlotInterval> &intervals = v.slots.channels[c];
			for (size_t iv = 2; iv + 1 < intervals.size(); iv += 2) {
				if (v.over[cc.crossings[(iv - 1) / 2]] != c || v.over[cc.crossings[(iv + 1) / 2]] != c) continue;
				found++;
				for (uint32_t line : cc.lines) {
					INFO(intervals[iv].name << " line " << line);
					bool has_copy = false;
					for (const PlanarSlotTrack &track : intervals[iv].tracks) has_copy = has_copy || (track.kind == PlanarSlotKind::Copy && track.line == line);
					CHECK(has_copy);
				}
			}
		}
	}
	REQUIRE(found > 0);
}

TEST_CASE("planar slots: a prio join's straight channel keeps the entry gaps", "[junction][planar]")
{
	size_t found = 0;
	for (const SlotsVariant &v : {BuildVariant(PlanarTText(2), 1, 3, 1), BuildVariant(PlanarLL2Text(), 17, 0, 1)}) {
		INFO(v.topology.name);
		REQUIRE(v.slots.why.empty());
		for (size_t s = 0; s < v.channels.sites.size(); s++) {
			if (v.channels.sites[s].kind != PlanarSiteKind::Join || !v.sites[s].prio || v.sites[s].straight == UINT32_MAX) continue;
			found++;
			const uint32_t c = v.sites[s].straight;
			const std::vector<uint32_t> &lines = v.channels.channels[c].lines;
			const int n = static_cast<int>(lines.size());
			const auto e = [](int j) { return j == 0 ? 0 : 2 * j - 1; };
			const bool far_left = v.sites[s].side > 0;
			const PlanarSlotInterval &last = v.slots.channels[c].back();
			for (int j = 1; j < n; j++) {
				INFO("site " << s << " lane " << j);
				CHECK(LineSlot(last, lines[j]) - LineSlot(last, lines[0]) == (far_left ? e(j) : e(n - 1) - e(n - 1 - j)));
			}
		}
	}
	REQUIRE(found > 0);
}

TEST_CASE("planar slots: a prio fray's straight legs step one slot toward the far lane", "[junction][planar]")
{
	size_t found = 0;
	for (const SlotsVariant &v : {BuildVariant(PlanarTText(2), 1, 3, 1), BuildVariant(PlanarLL2Text(), 17, 0, 1)}) {
		INFO(v.topology.name);
		REQUIRE(v.slots.why.empty());
		for (size_t s = 0; s < v.channels.sites.size(); s++) {
			if (v.channels.sites[s].kind != PlanarSiteKind::Fray || !v.sites[s].prio) continue;
			const PlanarChannel &fc = v.channels.channels[v.channels.sites[s].channel];
			const uint32_t far_line = v.sites[s].side > 0 ? fc.lines.front() : fc.lines.back();
			for (const PlanarSlotLink &link : v.slots.links) {
				if (std::ranges::find(fc.lines, link.from) == fc.lines.end() || link.from == far_line) continue;
				found++;
				INFO("site " << s << " from " << link.from);
				CHECK(link.step == (v.sites[s].side > 0 ? -1 : +1));
			}
		}
	}
	REQUIRE(found > 0);
}

TEST_CASE("planar slots: tracks", "[junction][planar]")
{
	const SlotsVariant v = BuildVariant(PlanarTText(2), 1, 3, 1);
	REQUIRE(v.slots.why.empty());

	/* Every channel's least slot is 0 and every interval's slots strictly increase left to right. */
	for (const std::vector<PlanarSlotInterval> &intervals : v.slots.channels) {
		int least = INT32_MAX;
		for (const PlanarSlotInterval &interval : intervals) {
			INFO(interval.name);
			for (size_t i = 0; i < interval.tracks.size(); i++) {
				least = std::min(least, interval.tracks[i].slot);
				if (i > 0) CHECK(interval.tracks[i].slot > interval.tracks[i - 1].slot);
			}
		}
		CHECK(least == 0);
	}

	/* Rows are not tracks yet. */
	for (const std::vector<PlanarSlotInterval> &intervals : v.slots.channels) {
		for (const PlanarSlotInterval &interval : intervals) {
			for (const PlanarSlotTrack &track : interval.tracks) CHECK(track.kind != PlanarSlotKind::Row);
		}
	}

	/* A copy beside each line in each x<k> interval of B where B is over. */
	const uint32_t b = v.channels.channel_of_line[PlanarLineByName(v.topology, "B1")];
	const PlanarChannel &channel = v.channels.channels[b];
	size_t over_intervals = 0;
	for (size_t k = 0; k < channel.crossings.size(); k++) {
		if (v.over[channel.crossings[k]] != b) continue;
		over_intervals++;
		const PlanarSlotInterval &interval = v.slots.channels[b][1 + 2 * k];
		INFO(interval.name);
		CHECK(interval.name == fmt::format("x{}", k));
		for (uint32_t line : channel.lines) {
			size_t copy = SIZE_MAX, own = SIZE_MAX;
			for (size_t i = 0; i < interval.tracks.size(); i++) {
				if (interval.tracks[i].line != line) continue;
				if (interval.tracks[i].kind == PlanarSlotKind::Copy) copy = i;
				if (interval.tracks[i].kind == PlanarSlotKind::Line) own = i;
			}
			REQUIRE(copy != SIZE_MAX);
			REQUIRE(own != SIZE_MAX);
			CHECK((copy + 1 == own || own + 1 == copy));
		}
	}
	CHECK(over_intervals > 0);
}

TEST_CASE("planar slots: fast", "[junction][planar]")
{
	for (const auto &[text, over_bits, ties] : {std::tuple{PlanarTText(2), 1, 3}, std::tuple{PlanarLL2Text(), 17, 0}}) {
		std::string error;
		const PlanarTopology topology = ParsePlanarTopology(text, &error);
		REQUIRE(error.empty());
		const PlanarChannels channels = FlipPlanarTriangles(BuildPlanarChannels(topology), 1);
		std::string why;
		const std::vector<PlanarResolvedSite> sites = ResolvePlanarSites(topology, channels, ties, why);
		REQUIRE(why.empty());
		const PlanarPaths paths = BuildPlanarPaths(topology, channels, over_bits);
		REQUIRE(paths.why.empty());
		const auto start = std::chrono::steady_clock::now();
		const PlanarSlots slots = BuildPlanarSlots(topology, channels, sites, paths.over);
		const double millis = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
		INFO(topology.name << " " << millis << " ms");
		REQUIRE(slots.why.empty());
#ifdef NDEBUG
		CHECK(millis < 20);
#endif
	}
}
