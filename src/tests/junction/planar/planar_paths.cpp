/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file planar_paths.cpp Test channel paths: headings, crossing pieces and over channels from the topology alone. */

#include "../../../stdafx.h"

#include "../../../3rdparty/catch2/catch.hpp"

#include "../../../junction/planar/planar_paths.h"
#include "../../../junction/planar/planar_plan.h"
#include "../../../junction/planar/planar_topology.h"

#include <algorithm>
#include <string>
#include <vector>

#include "../../../safeguards.h"

static bool IsAxisHeading(PlanarHeading h) { return static_cast<int>(h) % 2 == 0; }

/** The channel holding the named line. */
static uint32_t ChannelIndex(const PlanarTopology &topology, const PlanarChannels &channels, std::string_view line)
{
	return channels.channel_of_line[PlanarLineByName(topology, line)];
}

/** Every path is valid: 45-degree steps, crossings on nondecreasing pieces, the over piece axis, the under piece not parallel to it. */
static void CheckValid(const PlanarChannels &channels, const PlanarPaths &paths)
{
	REQUIRE(paths.why.empty());
	REQUIRE(paths.paths.size() == channels.channels.size());
	REQUIRE(paths.over.size() == channels.crossings.size());
	for (uint32_t c = 0; c < channels.channels.size(); c++) {
		const PlanarChannelPath &p = paths.paths[c];
		REQUIRE(!p.headings.empty());
		for (size_t i = 1; i < p.headings.size(); i++) {
			const int d = (static_cast<int>(p.headings[i]) - static_cast<int>(p.headings[i - 1]) + 8) % 8;
			CHECK((d == 1 || d == 7));
		}
		REQUIRE(p.crossing_piece.size() == channels.channels[c].crossings.size());
		for (size_t i = 1; i < p.crossing_piece.size(); i++) CHECK(p.crossing_piece[i] >= p.crossing_piece[i - 1]);
	}
	for (uint32_t x = 0; x < channels.crossings.size(); x++) {
		const PlanarChannelCrossing &cr = channels.crossings[x];
		const uint32_t over = paths.over[x];
		REQUIRE((over == cr.a || over == cr.b));
		const uint32_t under = over == cr.a ? cr.b : cr.a;
		const auto piece = [&](uint32_t ch) {
			const auto &list = channels.channels[ch].crossings;
			const size_t i = std::ranges::find(list, x) - list.begin();
			REQUIRE(i < list.size());
			return paths.paths[ch].headings[paths.paths[ch].crossing_piece[i]];
		};
		const PlanarHeading ho = piece(over), hu = piece(under);
		CHECK(IsAxisHeading(ho));
		CHECK(static_cast<int>(ho) % 4 != static_cast<int>(hu) % 4);
	}
}

TEST_CASE("planar paths: the T", "[planar]")
{
	for (int n = 1; n <= 3; n++) {
		INFO(n);
		std::string error;
		const PlanarTopology topology = ParsePlanarTopology(PlanarTText(n), &error);
		REQUIRE(error.empty());
		const PlanarChannels channels = BuildPlanarChannels(topology);
		const uint32_t b = ChannelIndex(topology, channels, "B1"), d = ChannelIndex(topology, channels, "D1"), f = ChannelIndex(topology, channels, "F1");
		/* B (prio) is over D and F; D x F is the one open crossing. */
		const std::vector<uint32_t> open = PlanarOpenCrossings(channels);
		REQUIRE(open.size() == 1);
		const PlanarChannelCrossing &df = channels.crossings[open.front()];
		CHECK(((df.a == d && df.b == f) || (df.a == f && df.b == d)));
		for (uint64_t choice = 0; choice < 2; choice++) {
			INFO(choice);
			const PlanarPaths paths = BuildPlanarPaths(topology, channels, choice);
			CheckValid(channels, paths);
			for (uint32_t x = 0; x < channels.crossings.size(); x++) {
				if (x != open.front()) CHECK(paths.over[x] == b);
			}
			/* Shortest turns: B and E straight, F E->S, D N->E, C N->W, A W->S, each 45 degrees a piece. */
			CHECK(paths.paths[b].headings == std::vector<PlanarHeading>{PlanarHeading::W});
			CHECK(paths.paths[ChannelIndex(topology, channels, "E1")].headings == std::vector<PlanarHeading>{PlanarHeading::E});
			CHECK(paths.paths[ChannelIndex(topology, channels, "C1")].headings == std::vector<PlanarHeading>{PlanarHeading::N, PlanarHeading::NW, PlanarHeading::W});
			CHECK(paths.paths[ChannelIndex(topology, channels, "A1")].headings == std::vector<PlanarHeading>{PlanarHeading::W, PlanarHeading::SW, PlanarHeading::S});
		}
		CHECK(BuildPlanarPaths(topology, channels, 0).over[open.front()] != BuildPlanarPaths(topology, channels, 1).over[open.front()]);
	}
}

TEST_CASE("planar paths: the T's crossing triangle flips", "[planar]")
{
	for (int n = 1; n <= 3; n++) {
		INFO(n);
		std::string error;
		const PlanarTopology topology = ParsePlanarTopology(PlanarTText(n), &error);
		REQUIRE(error.empty());
		const PlanarChannels channels = BuildPlanarChannels(topology);
		/* B, D and F cross pairwise: one empty triangle. */
		const auto triangles = PlanarTriangles(channels);
		REQUIRE(triangles.size() == 1);
		const PlanarChannels flipped = FlipPlanarTriangles(channels, 1);
		for (const char *ln : {"B1", "D1", "F1"}) {
			INFO(ln);
			const uint32_t c = ChannelIndex(topology, channels, ln);
			std::vector<uint32_t> before = channels.channels[c].crossings, after = flipped.channels[c].crossings;
			REQUIRE(before.size() == 2);
			std::ranges::reverse(before);
			CHECK(after == before);
		}
		/* Unflipped is the topology's order; both give valid paths. */
		CHECK(FlipPlanarTriangles(channels, 0).channels[ChannelIndex(topology, channels, "B1")].crossings == channels.channels[ChannelIndex(topology, channels, "B1")].crossings);
		for (uint64_t choice = 0; choice < 2; choice++) CheckValid(flipped, BuildPlanarPaths(topology, flipped, choice));
	}
}

static bool SamePaths(const std::vector<PlanarChannelPath> &a, const std::vector<PlanarChannelPath> &b)
{
	if (a.size() != b.size()) return false;
	for (size_t i = 0; i < a.size(); i++) {
		if (a[i].headings != b[i].headings || a[i].crossing_piece != b[i].crossing_piece) return false;
	}
	return true;
}

TEST_CASE("planar paths: every variant's own shape passes the shape check", "[planar]")
{
	const std::vector<std::string> texts = {PlanarTText(2), PlanarTText(3), PlanarLL2Text()};
	for (const std::string &text : texts) {
		std::string error;
		const PlanarTopology topology = ParsePlanarTopology(text, &error);
		REQUIRE(error.empty());
		const PlanarChannels channels = BuildPlanarChannels(topology);
		const size_t open = PlanarOpenCrossings(channels).size();
		const size_t triangles = PlanarTriangles(channels).size();
		size_t built = 0;
		for (uint64_t f = 0; f < (uint64_t{1} << triangles); f++) {
			const PlanarChannels flipped = FlipPlanarTriangles(channels, f);
			for (uint64_t o = 0; o < (uint64_t{1} << open); o++) {
				INFO(topology.name << " o" << o << " f" << f);
				const PlanarPaths p = BuildPlanarPaths(topology, flipped, o);
				if (!p.why.empty()) continue;
				built++;
				CHECK(CheckPlanarShape(topology, flipped, p.over, p.paths).empty());
				const PlanarPaths given = BuildPlanarPaths(topology, flipped, o, &p.paths);
				CHECK(given.why.empty());
				CHECK(given.over == p.over);
				CHECK(SamePaths(given.paths, p.paths));
			}
		}
		CHECK(built > 0);
	}
}

TEST_CASE("planar paths: a shape breaking a rule is refused with its reason", "[planar]")
{
	std::string error;
	const PlanarTopology topology = ParsePlanarTopology(PlanarTText(2), &error);
	REQUIRE(error.empty());
	const PlanarChannels channels = BuildPlanarChannels(topology);
	const PlanarPaths p = BuildPlanarPaths(topology, channels, 0);
	REQUIRE(p.why.empty());
	REQUIRE(CheckPlanarShape(topology, channels, p.over, p.paths).empty());

	/* (a) An over crossing's piece on a diagonal heading. */
	{
		std::vector<PlanarChannelPath> paths = p.paths;
		bool done = false;
		for (uint32_t x = 0; x < channels.crossings.size() && !done; x++) {
			const uint32_t o = p.over[x];
			const auto &list = channels.channels[o].crossings;
			const size_t i = std::ranges::find(list, x) - list.begin();
			PlanarChannelPath &path = paths[o];
			const uint32_t piece = path.crossing_piece[i];
			/* A single-piece channel would break its port heading too; pick a path with a diagonal piece to move onto. */
			for (uint32_t k = 0; k < path.headings.size() && !done; k++) {
				if (IsAxisHeading(path.headings[k])) continue;
				path.crossing_piece[i] = k;
				done = true;
			}
			if (!done) path.crossing_piece[i] = piece;
		}
		REQUIRE(done);
		const std::string why = CheckPlanarShape(topology, channels, p.over, paths);
		CHECK(why.find("over") != std::string::npos);
		CHECK(BuildPlanarPaths(topology, channels, 0, &paths).why == why);
	}
	/* (b) Two consecutive headings 90 degrees apart. */
	{
		std::vector<PlanarChannelPath> paths = p.paths;
		bool done = false;
		for (PlanarChannelPath &path : paths) {
			if (path.headings.size() < 2) continue;
			path.headings[1] = static_cast<PlanarHeading>((static_cast<int>(path.headings[0]) + 2) % 8);
			done = true;
			break;
		}
		REQUIRE(done);
		CHECK(CheckPlanarShape(topology, channels, p.over, paths).find("45") != std::string::npos);
	}
	/* (c) A crossing piece index out of range. */
	{
		std::vector<PlanarChannelPath> paths = p.paths;
		uint32_t named = UINT32_MAX;
		for (uint32_t c = 0; c < paths.size(); c++) {
			if (paths[c].crossing_piece.empty()) continue;
			paths[c].crossing_piece[0] = static_cast<uint32_t>(paths[c].headings.size());
			named = c;
			break;
		}
		REQUIRE(named != UINT32_MAX);
		const std::string line = topology.lines[channels.channels[named].lines.front()].name;
		CHECK(CheckPlanarShape(topology, channels, p.over, paths).find(line) != std::string::npos);
	}
}

TEST_CASE("planar paths: a shape crossing from the wrong side is refused", "[planar]")
{
	std::string error;
	const PlanarTopology topology = ParsePlanarTopology(PlanarTText(2), &error);
	REQUIRE(error.empty());
	const PlanarChannels base = BuildPlanarChannels(topology);
	const size_t open = PlanarOpenCrossings(base).size(), triangles = PlanarTriangles(base).size();
	/* Move an under channel's crossing onto a piece heading to the mirrored side of the over piece (still not parallel, still in travel order). */
	bool done = false;
	for (uint64_t f = 0; f < (uint64_t{1} << triangles) && !done; f++) {
		const PlanarChannels channels = FlipPlanarTriangles(base, f);
		for (uint64_t o = 0; o < (uint64_t{1} << open) && !done; o++) {
			const PlanarPaths p = BuildPlanarPaths(topology, channels, o);
			if (!p.why.empty()) continue;
			REQUIRE(CheckPlanarShape(topology, channels, p.over, p.paths).empty());
			for (uint32_t x = 0; x < channels.crossings.size() && !done; x++) {
				const PlanarChannelCrossing &cr = channels.crossings[x];
				const uint32_t over = p.over[x], under = over == cr.a ? cr.b : cr.a;
				const auto index = [&](uint32_t ch) { return static_cast<size_t>(std::ranges::find(channels.channels[ch].crossings, x) - channels.channels[ch].crossings.begin()); };
				const int ho = static_cast<int>(p.paths[over].headings[p.paths[over].crossing_piece[index(over)]]);
				const PlanarChannelPath &up = p.paths[under];
				const int hu = static_cast<int>(up.headings[up.crossing_piece[index(under)]]);
				const int mirrored = ((2 * ho - hu) % 8 + 8) % 8;
				if (index(under) + 1 != channels.channels[under].crossings.size() || mirrored % 4 == ho % 4) continue;
				/* The path up to the previous crossing's piece, then walking to the mirrored heading and on to the end, 45 degrees a piece; the crossing sits on the mirrored piece. */
				std::vector<PlanarChannelPath> paths = p.paths;
				PlanarChannelPath &path = paths[under];
				const int end = static_cast<int>(up.headings.back());
				const size_t keep = index(under) > 0 ? up.crossing_piece[index(under) - 1] : 0;
				path.headings.assign(up.headings.begin(), up.headings.begin() + keep + 1);
				int cur = static_cast<int>(path.headings.back());
				const auto walk = [&](int target) {
					while (cur != target) {
						cur = (cur + ((target - cur + 8) % 8 <= 4 ? 1 : 7)) % 8;
						path.headings.push_back(static_cast<PlanarHeading>(cur));
					}
				};
				walk(mirrored);
				path.crossing_piece.back() = static_cast<uint32_t>(path.headings.size() - 1);
				walk(end);
				const std::string why = CheckPlanarShape(topology, channels, p.over, paths);
				if (why.find("side") == std::string::npos) continue;
				CHECK(BuildPlanarPaths(topology, channels, o, &paths).why == why);
				done = true;
			}
		}
	}
	CHECK(done);
}
