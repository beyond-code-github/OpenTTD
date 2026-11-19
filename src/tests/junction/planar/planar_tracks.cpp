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
#include "../../../junction/planar/planar_model.h"
#include "../../../junction/planar/planar_paths.h"
#include "../../../junction/planar/planar_plan.h"
#include "../../../junction/planar/planar_regions.h"
#include "../../../junction/planar/planar_slots.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
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

TEST_CASE("planar track map: the expanded topology holds every track with its ends", "[junction][planar]")
{
	const TracksVariant v = BuildTracksVariant(PlanarTText(2), 1, 3, 1);
	REQUIRE(v.ok);
	const PlanarTrackMap map = BuildPlanarTrackMap(v.topology, v.channels, v.sites, v.over, false);
	REQUIRE(map.why.empty());
	const PlanarTopology &e = map.expanded;

	REQUIRE(e.lines.size() == map.tracks.size());
	for (size_t i = 0; i < map.tracks.size(); i++) CHECK(e.lines[i].name == v.topology.lines[map.tracks[i].line].name); // An expanded line keeps its topology line's name (a copy or row too).
	CHECK(e.events == map.events);
	CHECK(e.crossings.size() == map.crossings.size());

	/* Whether a node's list holds a track that is a part of the given line. */
	const auto holds_part = [&](const std::vector<uint32_t> &tracks, uint32_t line) {
		for (const uint32_t t : tracks) {
			if (map.part_of[t] == line) return true;
		}
		return false;
	};
	for (uint32_t i = 0; i < map.tracks.size(); i++) {
		const PlanarTrack &track = map.tracks[i];
		const PlanarLine &l = e.lines[i];
		INFO("track " << i << " " << track.name);
		if (track.kind == PlanarTrack::Kind::Copy) {
			REQUIRE(!l.from.port);
			REQUIRE(!l.to.port);
			CHECK(holds_part(e.nodes[l.from.index].in, track.line));
			CHECK(holds_part(e.nodes[l.to.index].out, track.line));
		} else if (track.kind == PlanarTrack::Kind::Row) {
			REQUIRE(!l.from.port);
			CHECK(holds_part(e.nodes[l.from.index].in, track.line));
			CHECK(!l.to.port);
		}
	}
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

/** A variant with its headings of every line built. */
struct HeadingsVariant {
	TracksVariant base;
	PlanarPaths paths;
	std::vector<std::vector<uint32_t>> events;
	PlanarTrackMap map;
	PlanarLineHeadings headings;
	bool built = false; ///< The paths were found and the track map (form A) built.
};

static HeadingsVariant BuildHeadingsVariant(const std::string &text, uint64_t over_bits, uint64_t ties, uint64_t flips)
{
	HeadingsVariant h;
	h.base = BuildTracksVariant(text, over_bits, ties, flips);
	if (!h.base.ok) return h;
	const TracksVariant &v = h.base;
	h.paths = BuildPlanarPaths(v.topology, v.channels, over_bits);
	h.events = PlanarLineSides(v.topology, v.channels).Events();
	h.map = BuildPlanarTrackMap(v.topology, v.channels, v.sites, v.over, false);
	if (!h.map.why.empty()) return h;
	h.built = true;
	h.headings = BuildPlanarLineHeadings(v.topology, v.channels, h.paths, v.sites, h.events, h.map);
	return h;
}

/** The first rule a variant's headings break among the checks of the outer test, or empty. */
static std::string CheckHeadings(const HeadingsVariant &h)
{
	const PlanarTopology &t = h.base.topology;
	const PlanarChannels &channels = h.base.channels;
	const std::vector<PlanarChannelPath> &lines = h.headings.lines;
	const auto name = [&](uint32_t l) { return t.lines[l].name; };
	if (lines.size() != t.lines.size()) return "one list per line";
	for (uint32_t l = 0; l < t.lines.size(); l++) {
		const std::vector<PlanarHeading> &hs = lines[l].headings;
		if (hs.empty()) return fmt::format("{} has no headings", name(l));
		for (size_t i = 1; i < hs.size(); i++) {
			if (std::abs(PlanarModel::Rot(static_cast<int>(hs[i - 1]), static_cast<int>(hs[i]))) != 1) return fmt::format("{} has consecutive headings not one eighth apart at {}", name(l), i);
		}
		const PlanarLine &line = t.lines[l];
		if (line.from.port && hs.front() != t.ports[line.from.index].heading) return fmt::format("{} does not start on its port's heading", name(l));
		if (line.to.port && hs.back() != t.ports[line.to.index].heading) return fmt::format("{} does not end on its port's heading", name(l));
		if (lines[l].crossing_piece.size() != h.events[l].size()) return fmt::format("{} has {} pieces for {} events", name(l), lines[l].crossing_piece.size(), h.events[l].size());
		for (size_t k = 0; k < lines[l].crossing_piece.size(); k++) {
			if (lines[l].crossing_piece[k] >= hs.size()) return fmt::format("{} has an event piece outside its list", name(l));
			if (k > 0 && lines[l].crossing_piece[k] < lines[l].crossing_piece[k - 1]) return fmt::format("{} has event pieces decreasing", name(l));
		}
	}
	/* Rule 0: a node's straight leg and side come from its site's resolved straight channel unless the site is composed per node. */
	const auto legs_of = [&](uint32_t n) {
		PlanarNodeLegs legs = channels.node_legs[n];
		for (size_t s = 0; s < channels.sites.size(); s++) {
			const PlanarSite &site = channels.sites[s];
			if (std::ranges::find(site.nodes, n) == site.nodes.end()) continue;
			if (site.kind == PlanarSiteKind::LaneSplit || site.kind == PlanarSiteKind::LaneMerge || site.others.size() != 2) break;
			legs.straight = UINT32_MAX;
			for (const uint32_t l : t.nodes[n].IsSplit() ? t.nodes[n].out : t.nodes[n].in) {
				if (channels.channel_of_line[l] == h.base.sites[s].straight) legs.straight = l;
			}
			legs.side = h.base.sites[s].side;
			break;
		}
		return legs;
	};
	for (uint32_t n = 0; n < t.nodes.size(); n++) {
		const PlanarNode &node = t.nodes[n];
		const PlanarNodeLegs legs = legs_of(n);
		if (node.IsSplit()) {
			const int h0 = static_cast<int>(lines[node.in[0]].headings.back());
			for (size_t k = 0; k < 2; k++) {
				const uint32_t leg = node.out[k];
				const int first = static_cast<int>(lines[leg].headings.front());
				if (legs.straight == leg) {
					if (first != h0) return fmt::format("straight leg {} does not start on its in line's heading", name(leg));
				} else {
					const int s = legs.straight == UINT32_MAX ? (k == 0 ? -1 : +1) : legs.side;
					if (first != PlanarModel::Turn(h0, s)) return fmt::format("leg {} does not start on the turn of its in line's heading", name(leg));
				}
			}
		} else if (node.out.size() == 1 && node.in.size() == 2) {
			const int h1 = static_cast<int>(lines[node.out[0]].headings.front());
			for (size_t k = 0; k < 2; k++) {
				const uint32_t leg = node.in[k];
				const int last = static_cast<int>(lines[leg].headings.back());
				if (legs.straight == leg) {
					if (last != h1) return fmt::format("straight leg {} does not end on its out line's heading", name(leg));
				} else {
					const int s = legs.straight == UINT32_MAX ? (k == 0 ? -1 : +1) : legs.side;
					if (last != PlanarModel::Turn(h1, -s)) return fmt::format("leg {} does not end on the turn of its out line's heading", name(leg));
				}
			}
		}
	}
	const auto piece_of = [&](uint32_t l, uint32_t c) {
		const auto it = std::ranges::find(h.events[l], c);
		return static_cast<int>(lines[l].headings[lines[l].crossing_piece[it - h.events[l].begin()]]);
	};
	for (uint32_t c = 0; c < t.crossings.size(); c++) {
		const uint32_t a = t.crossings[c].a, b = t.crossings[c].b;
		uint32_t over = UINT32_MAX;
		for (uint32_t x = 0; x < channels.crossings.size() && over == UINT32_MAX; x++) {
			if (!(std::ranges::find(channels.crossings[x].line_crossings, c) != std::ranges::end(channels.crossings[x].line_crossings))) continue;
			const uint32_t over_channel = h.paths.over[x];
			over = channels.channel_of_line[a] == over_channel ? a : b;
		}
		if (over == UINT32_MAX) {
			for (uint32_t g = 0; g < channels.group_crossings.size() && over == UINT32_MAX; g++) {
				if (!(std::ranges::find(channels.group_crossings[g], c) != std::ranges::end(channels.group_crossings[g]))) continue;
				for (const uint32_t line : {a, b}) {
					const PlanarEnd &end = t.lines[line].from.port || t.nodes[t.lines[line].from.index].group != g ? t.lines[line].to : t.lines[line].from;
					const uint32_t node = end.index;
					for (size_t s = 0; s < channels.sites.size(); s++) {
						if (std::ranges::find(channels.sites[s].nodes, node) == channels.sites[s].nodes.end()) continue;
						const uint32_t straight = h.base.sites[s].straight;
						if (straight != UINT32_MAX ? channels.channel_of_line[line] == straight : channels.node_legs[node].straight == line) over = line;
					}
				}
			}
		}
		if (over == UINT32_MAX) return fmt::format("crossing {} x {} has no over line", name(a), name(b));
		const uint32_t under = over == a ? b : a;
		const int ho = piece_of(over, c), hu = piece_of(under, c);
		if (ho % 2 != 0) return fmt::format("{} is over {} on a diagonal piece", name(over), name(under));
		if (ho % 4 == hu % 4) return fmt::format("{} crosses {} on a parallel piece", name(under), name(over));
	}
	if (h.headings.beside.size() != h.map.tracks.size()) return "one beside per track";
	for (uint32_t i = 0; i < h.map.tracks.size(); i++) {
		const auto &[parent, first, last] = h.headings.beside[i];
		const bool beside = h.map.tracks[i].kind != PlanarTrack::Kind::Line;
		if (!beside) continue;
		if (parent == UINT32_MAX || parent != h.map.tracks[i].line) return fmt::format("{} has no parent", h.map.tracks[i].name);
		if (first > last || last >= lines[parent].headings.size()) return fmt::format("{} has a bad range beside {}", h.map.tracks[i].name, name(parent));
	}
	return {};
}

TEST_CASE("planar line headings: every variant of T2, T3 and LL² by the plain rule", "[junction][planar]")
{
	std::map<std::string, size_t> failures;
	std::map<std::string, std::string> example;
	std::map<std::string, std::pair<size_t, size_t>> counts; // hub -> (ok, variants)
	size_t bad = 0;
	const auto run = [&](const std::string &hub, const std::string &text, uint64_t o, uint64_t t, uint64_t f) {
		const HeadingsVariant h = BuildHeadingsVariant(text, o, t, f);
		if (!h.built) return;
		counts[hub].second++;
		if (!h.headings.why.empty()) {
			failures[hub + ": " + h.headings.why]++;
			return;
		}
		counts[hub].first++;
		const std::string why = CheckHeadings(h);
		if (!why.empty()) {
			bad++;
			failures[hub + " CHECK: " + why]++;
			example[hub + " CHECK: " + why] = fmt::format("{}o{}t{}f{}", hub, o, t, f);
		}
	};
	for (const int lanes : {2, 3}) {
		for (uint64_t o = 0; o < 2; o++) {
			for (uint64_t t = 0; t < 4; t++) {
				for (uint64_t f = 0; f < 2; f++) run(fmt::format("T{}", lanes), PlanarTText(lanes), o, t, f);
			}
		}
	}
	for (uint64_t o = 0; o < 64; o++) {
		for (uint64_t t = 0; t < 8; t++) {
			for (uint64_t f = 0; f < 2; f++) run("LL2", PlanarLL2Text(), o, t, f);
		}
	}
	for (const auto &[hub, c] : counts) WARN(hub << ": " << c.first << " of " << c.second << " variants with empty why");
	for (const auto &[text, n] : failures) WARN(n << " x " << text << (example.contains(text) ? " e.g. " + example[text] : ""));
	CHECK(counts["T2"].first > 0);
	CHECK(counts["T3"].first > 0);
	CHECK(bad == 0);
}

TEST_CASE("planar line headings: T2 o1t3f1s0 straight fray leg and diverging leg", "[junction][planar]")
{
	const HeadingsVariant h = BuildHeadingsVariant(PlanarTText(2), 1, 3, 1);
	REQUIRE(h.built);
	REQUIRE(h.headings.why.empty());
	const PlanarTopology &t = h.base.topology;
	size_t fray_lines = 0;
	for (uint32_t l = 0; l < t.lines.size(); l++) {
		if (!t.lines[l].name.starts_with("EF") || t.lines[l].to.port) continue;
		fray_lines++;
		const uint32_t n = t.lines[l].to.index;
		REQUIRE(t.nodes[n].IsSplit());
		PlanarNodeLegs legs = h.base.channels.node_legs[n];
		for (size_t si = 0; si < h.base.channels.sites.size(); si++) {
			const PlanarSite &site = h.base.channels.sites[si];
			if (std::ranges::find(site.nodes, n) == site.nodes.end() || site.kind == PlanarSiteKind::LaneSplit || site.others.size() != 2) continue;
			legs.straight = UINT32_MAX;
			for (const uint32_t o : t.nodes[n].out) {
				if (h.base.channels.channel_of_line[o] == h.base.sites[si].straight) legs.straight = o;
			}
			legs.side = h.base.sites[si].side;
		}
		const int h0 = static_cast<int>(h.headings.lines[l].headings.back());
		for (const uint32_t leg : t.nodes[n].out) {
			const PlanarHeading first = h.headings.lines[leg].headings.front();
			INFO(t.lines[leg].name);
			if (t.lines[leg].name.starts_with('E')) {
				CHECK(legs.straight == leg);
				CHECK(static_cast<int>(first) == h0);
			} else {
				REQUIRE(t.lines[leg].name.starts_with('F'));
				CHECK(static_cast<int>(first) == PlanarModel::Turn(h0, legs.side));
			}
		}
	}
	CHECK(fray_lines == 2);
}
