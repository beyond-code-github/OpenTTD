/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file planar_tracks.cpp Test the track map: the skeleton expanded with copies and rows, with sides per stretch. */

#include "../../../stdafx.h"

#include "../../../3rdparty/catch2/catch.hpp"

#include "../../../core/format.hpp"
#include "../../../junction/planar/planar_gaps.h"
#include "../../../junction/planar/planar_paths.h"
#include "../../../junction/planar/planar_plan.h"
#include "../../../junction/planar/planar_regions.h"
#include "../../../junction/planar/planar_slots.h"

#include <algorithm>
#include <chrono>
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

/** Check a variant's track map in both forms; returns the number of failures it found (and puts the first into INFO). */
static bool CheckVariant(const TracksVariant &v, const std::string &tag, int &reported)
{
	bool good = true;
	for (const bool joiner_over : {false, true}) {
		const PlanarTrackMap map = BuildPlanarTrackMap(v.topology, v.channels, v.sites, v.over, joiner_over);
		if (map.why.empty() && map.conflicts.empty()) continue;
		good = false;
		if (reported++ < 3) {
			std::string text = fmt::format("{} joiner_over={} why='{}' conflicts={}", tag, joiner_over, map.why, map.conflicts.size());
			for (size_t i = 0; i < map.conflicts.size() && i < 2; i++) {
				const auto &[p, ps, q, qs] = map.conflicts[i];
				text += fmt::format(" [{}#{} vs {}#{}]", map.tracks[p].name, ps, map.tracks[q].name, qs);
			}
			UNSCOPED_INFO(text);
		}
	}
	return good;
}

TEST_CASE("planar track map: every variant of T2, T3 and LL² validates in both forms", "[junction][planar]")
{
	int reported = 0;
	size_t bad = 0, built = 0;
	for (const int lanes : {2, 3}) {
		for (uint64_t o = 0; o < 2; o++) {
			for (uint64_t t = 0; t < 4; t++) {
				for (uint64_t f = 0; f < 2; f++) {
					const TracksVariant v = BuildTracksVariant(PlanarTText(lanes), o, t, f);
					REQUIRE(v.ok);
					built++;
					if (!CheckVariant(v, fmt::format("T{}o{}t{}f{}", lanes, o, t, f), reported)) bad++;
				}
			}
		}
	}
	for (uint64_t o = 0; o < 64; o++) {
		for (uint64_t t = 0; t < 8; t++) {
			for (uint64_t f = 0; f < 2; f++) {
				const TracksVariant v = BuildTracksVariant(PlanarLL2Text(), o, t, f);
				if (!v.ok) continue;
				built++;
				if (!CheckVariant(v, fmt::format("LL2o{}t{}f{}", o, t, f), reported)) bad++;
			}
		}
	}
	CHECK(built > 16);
	INFO(built << " variants");
	CHECK(bad == 0);
}

/** The Copy runs the slot layer holds in x<k> intervals: per channel and line, maximal runs of consecutive intervals with a copy. */
static size_t SlotCopyRuns(const TracksVariant &v)
{
	const PlanarSlots slots = BuildPlanarSlots(v.topology, v.channels, v.sites, v.over);
	REQUIRE(slots.why.empty());
	size_t runs = 0;
	for (const std::vector<PlanarSlotInterval> &intervals : slots.channels) {
		std::set<uint32_t> lines;
		for (const PlanarSlotInterval &interval : intervals) {
			for (const PlanarSlotTrack &track : interval.tracks) {
				if (track.kind == PlanarSlotKind::Copy) lines.insert(track.line);
			}
		}
		for (const uint32_t line : lines) {
			bool in_run = false;
			for (const PlanarSlotInterval &interval : intervals) {
				const bool has = std::ranges::any_of(interval.tracks, [&](const PlanarSlotTrack &track) { return track.kind == PlanarSlotKind::Copy && track.line == line; });
				if (has && !in_run) runs++;
				in_run = has;
			}
		}
	}
	return runs;
}

/** The track indices of a kind. */
static std::vector<uint32_t> TracksOfKind(const PlanarTrackMap &map, PlanarTrack::Kind kind)
{
	std::vector<uint32_t> out;
	for (uint32_t i = 0; i < map.tracks.size(); i++) {
		if (map.tracks[i].kind == kind) out.push_back(i);
	}
	return out;
}

TEST_CASE("planar track map: tracks", "[junction][planar]")
{
	const TracksVariant v = BuildTracksVariant(PlanarTText(2), 1, 3, 1);
	REQUIRE(v.ok);

	/* The joiners of each prio join: the lines that arrive from the other channel. */
	std::set<uint32_t> joiners;
	for (size_t s = 0; s < v.channels.sites.size(); s++) {
		if (v.channels.sites[s].kind != PlanarSiteKind::Join || !v.sites[s].prio || v.sites[s].straight == UINT32_MAX) continue;
		for (const uint32_t n : v.channels.sites[s].nodes) {
			for (const uint32_t l : v.topology.nodes[n].in) {
				if (v.channels.channel_of_line[l] != v.sites[s].straight) joiners.insert(l);
			}
		}
	}
	REQUIRE(joiners.size() == 4);

	const size_t slot_runs = SlotCopyRuns(v);
	REQUIRE(slot_runs > 0);
	for (const bool joiner_over : {true, false}) {
		INFO("joiner_over " << joiner_over);
		const PlanarTrackMap map = BuildPlanarTrackMap(v.topology, v.channels, v.sites, v.over, joiner_over);
		REQUIRE(map.why.empty());
		const std::vector<uint32_t> rows = TracksOfKind(map, PlanarTrack::Kind::Row);
		CHECK(rows.size() == 4);
		/* The two join forms differ on T2 only by the crossed lane's copy at each prio join (form A: S1 doubles over J0). */
		CHECK(TracksOfKind(map, PlanarTrack::Kind::Copy).size() == slot_runs + (joiner_over ? 0 : 2));
		size_t crossed = 0;
		for (const uint32_t row : rows) {
			CHECK(map.events[row].size() <= 1); // A far lane has no crossing with a joiner, so its row has none.
			crossed += map.events[row].size();
			for (const uint32_t c : map.events[row]) {
				const PlanarCrossing &x = map.crossings[c];
				const uint32_t other = x.a == row ? x.b : x.a;
				CHECK(joiners.contains(map.tracks[other].line));
			}
		}
		CHECK(crossed == 2); // The near lane of each of the two prio joins is crossed by its first joiner.
	}
}

/** The index of a track's event for the crossing with a track, or SIZE_MAX. */
static size_t EventWith(const PlanarTrackMap &map, uint32_t track, uint32_t other)
{
	for (size_t i = 0; i < map.events[track].size(); i++) {
		const PlanarCrossing &x = map.crossings[map.events[track][i]];
		if ((x.a == track && x.b == other) || (x.b == track && x.a == other)) return i;
	}
	return SIZE_MAX;
}

TEST_CASE("planar track map: joiner over at T2's E join gives J0, row, S1, J1", "[junction][planar]")
{
	const TracksVariant v = BuildTracksVariant(PlanarTText(2), 1, 3, 1);
	REQUIRE(v.ok);
	const PlanarTrackMap map = BuildPlanarTrackMap(v.topology, v.channels, v.sites, v.over, true);
	REQUIRE(map.why.empty());
	const auto track_of = [&](const std::string &name, uint32_t holding_with) {
		for (uint32_t i = 0; i < map.tracks.size(); i++) {
			if (map.tracks[i].name == name && (holding_with == UINT32_MAX || EventWith(map, i, holding_with) != SIZE_MAX)) return i;
		}
		FAIL("no such track " << name);
		return UINT32_MAX;
	};
	const uint32_t J0 = track_of("D1", UINT32_MAX);
	const uint32_t R = track_of("prio:E2", UINT32_MAX);
	const uint32_t S1 = track_of("E2", J0); // The part of the near lane that crosses J0.
	const uint32_t J1 = track_of("D2", UINT32_MAX);
	REQUIRE(map.events[R].size() == 1);

	/* Stretch k of a track lies after its event k - 1. R after its crossing with J0, J0 after its crossing with R, S1 after its crossing with J0. */
	const size_t r_after = EventWith(map, R, J0) + 1;
	const size_t j0_after_r = EventWith(map, J0, R) + 1;
	const size_t s1_after = EventWith(map, S1, J0) + 1;
	REQUIRE(r_after == 1);

	/* The E join's joiners arrive from the right of the lanes (sigma +1), so after the crossings the order left to right is J0, R, S1, J1. */
	CHECK(map.Side(J0, j0_after_r, R, r_after) == -1); // J0 lies left of R.
	CHECK(map.Side(R, r_after, J0, j0_after_r) == +1);
	CHECK(map.Side(S1, s1_after, R, r_after) == +1); // S1 lies right of R (the row is on the side away from the joiners, -sigma).
	CHECK(map.Side(R, r_after, S1, s1_after) == -1);
	/* S1's last stretch and J1's last stretch bound one face, J1 on S1's right. */
	const size_t s1_last = map.events[S1].size();
	const size_t j1_last = map.events[J1].size();
	CHECK(map.Side(J1, j1_last, S1, s1_last) == +1);
	CHECK(map.Side(S1, s1_last, J1, j1_last) == -1);
}

TEST_CASE("planar track map: fast", "[junction][planar]")
{
	for (const bool ll2 : {false, true}) {
		const TracksVariant v = ll2 ? BuildTracksVariant(PlanarLL2Text(), 17, 0, 1) : BuildTracksVariant(PlanarTText(2), 1, 3, 1);
		REQUIRE(v.ok);
		const auto start = std::chrono::steady_clock::now();
		const PlanarTrackMap map = BuildPlanarTrackMap(v.topology, v.channels, v.sites, v.over, false);
		const auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
		INFO(v.topology.name << " " << ms << " ms");
		REQUIRE(map.why.empty());
#ifdef NDEBUG
		CHECK(ms < 20);
#endif
	}
}
