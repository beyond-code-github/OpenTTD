/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file planar_gaps.cpp Test gap steps: the sideways steps pinned port gaps need, from the channel graph. */

#include "../../../stdafx.h"

#include "../../../3rdparty/catch2/catch.hpp"

#include "../../../junction/planar/planar_gaps.h"
#include "../../../junction/planar/planar_paths.h"

#include <algorithm>
#include <string>
#include <vector>

#include "../../../safeguards.h"

/** The channel holding the named line. */
static uint32_t ChannelIndex(const PlanarTopology &topology, const PlanarChannels &channels, std::string_view line)
{
	return channels.channel_of_line[PlanarLineByName(topology, line)];
}

static void CheckTSteps(int n, uint64_t ties)
{
	std::string error;
	const PlanarTopology topology = ParsePlanarTopology(PlanarTText(n), &error);
	REQUIRE(error.empty());
	const PlanarChannels channels = FlipPlanarTriangles(BuildPlanarChannels(topology), 1);
	std::string why;
	const std::vector<PlanarResolvedSite> sites = ResolvePlanarSites(topology, channels, ties, why);
	REQUIRE(why.empty());
	REQUIRE(sites.size() == channels.sites.size());
	const std::vector<PlanarGapStep> steps = BuildPlanarGapSteps(topology, channels, sites, why);
	REQUIRE(why.empty());

	std::vector<PlanarGapStep> expected = {
		{ChannelIndex(topology, channels, "EF1"), true, ChannelIndex(topology, channels, "CB1"), -1},
		{ChannelIndex(topology, channels, "AF1"), false, ChannelIndex(topology, channels, "CD1"), +1},
		{ChannelIndex(topology, channels, "ED1"), false, ChannelIndex(topology, channels, "AB1"), +1},
	};
	std::ranges::sort(expected, [](const PlanarGapStep &a, const PlanarGapStep &b) {
		return a.channel != b.channel ? a.channel < b.channel : a.fan < b.fan;
	});
	REQUIRE(steps.size() == expected.size());
	for (size_t i = 0; i < steps.size(); i++) {
		INFO(i);
		CHECK(steps[i].channel == expected[i].channel);
		CHECK(steps[i].fan == expected[i].fan);
		CHECK(steps[i].neighbour == expected[i].neighbour);
		CHECK(steps[i].toward == expected[i].toward);
	}
}

TEST_CASE("planar gaps: T3 o0t1f1 steps", "[junction][planargaps]")
{
	CheckTSteps(3, 1);
}

TEST_CASE("planar gaps: T2 o0t2f1 steps", "[junction][planargaps]")
{
	CheckTSteps(2, 2);
}

static void CheckTCandidates(int n)
{
	std::string error;
	const PlanarTopology topology = ParsePlanarTopology(PlanarTText(n), &error);
	REQUIRE(error.empty());
	const PlanarChannels channels = FlipPlanarTriangles(BuildPlanarChannels(topology), 1);
	const std::vector<PlanarCandidateStep> cands = PlanarCandidateSteps(topology, channels);
	REQUIRE(cands.size() == 1);
	CHECK(cands[0].channel == ChannelIndex(topology, channels, "B1"));
	CHECK(cands[0].toward == -1);
}

TEST_CASE("planar gaps: the T's candidate step", "[junction][planargaps]")
{
	CheckTCandidates(3);
	CheckTCandidates(2);
}

TEST_CASE("planar gaps: resolved sites of the T", "[junction][planargaps]")
{
	std::string error;
	const PlanarTopology topology = ParsePlanarTopology(PlanarTText(3), &error);
	REQUIRE(error.empty());
	const PlanarChannels channels = FlipPlanarTriangles(BuildPlanarChannels(topology), 1);
	std::string why;
	const std::vector<PlanarResolvedSite> sites = ResolvePlanarSites(topology, channels, 1, why);
	REQUIRE(why.empty());
	REQUIRE(sites.size() == channels.sites.size());
	bool saw_fray = false, saw_join = false;
	for (size_t s = 0; s < sites.size(); s++) {
		const PlanarSite &site = channels.sites[s];
		const PlanarResolvedSite &r = sites[s];
		if (site.kind != PlanarSiteKind::Fray && site.kind != PlanarSiteKind::Join) continue;
		if (site.others.size() != 2) continue;
		INFO(s);
		CHECK(((r.straight == site.others[0] && r.other == site.others[1]) || (r.straight == site.others[1] && r.other == site.others[0])));
		CHECK((r.side == 1 || r.side == -1));
		if (site.kind == PlanarSiteKind::Fray && site.channel == ChannelIndex(topology, channels, "EF1")) {
			saw_fray = true;
			CHECK(r.straight == ChannelIndex(topology, channels, "E1"));
			CHECK(r.prio);
		}
		if (site.kind == PlanarSiteKind::Join && site.channel == ChannelIndex(topology, channels, "ED1")) {
			saw_join = true;
			CHECK(r.other == ChannelIndex(topology, channels, "D1"));
			CHECK(r.side == 1);
		}
	}
	CHECK(saw_fray);
	CHECK(saw_join);
}
