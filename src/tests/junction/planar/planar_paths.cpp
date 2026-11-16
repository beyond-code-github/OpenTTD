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
