/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file planar_channels.cpp Test the channel graph: grouping, order and channel crossings from the topology alone. */

#include "../../../stdafx.h"

#include "../../../3rdparty/catch2/catch.hpp"

#include "../../../core/format.hpp"
#include "../../../junction/planar/planar_channels.h"
#include "../../../junction/planar/planar_plan.h"
#include "../../../junction/planar/planar_topology.h"

#include <set>
#include <string>
#include <vector>

#include "../../../safeguards.h"

/** Parse and require success. */
static PlanarTopology Parse(const std::string &text)
{
	std::string error;
	PlanarTopology topology = ParsePlanarTopology(text, &error);
	INFO(error);
	REQUIRE(error.empty());
	return topology;
}

/** The names of a channel's lines, left to right. */
static std::vector<std::string> Names(const PlanarTopology &topology, const PlanarChannel &channel)
{
	std::vector<std::string> names;
	for (uint32_t l : channel.lines) names.push_back(topology.lines[l].name);
	return names;
}

/** The channel holding the named line. */
static const PlanarChannel &ChannelOf(const PlanarTopology &topology, const PlanarChannels &channels, std::string_view line)
{
	const uint32_t l = PlanarLineByName(topology, line);
	REQUIRE(l < channels.channel_of_line.size());
	return channels.channels[channels.channel_of_line[l]];
}

/** The letter pairs of the channel crossings, each channel named by its first line's first route letter. */
static std::set<std::pair<char, char>> CrossingLetters(const PlanarTopology &topology, const PlanarChannels &channels)
{
	std::set<std::pair<char, char>> out;
	for (const PlanarChannelCrossing &c : channels.crossings) {
		char a = topology.lines[channels.channels[c.a].lines.front()].name[0];
		char b = topology.lines[channels.channels[c.b].lines.front()].name[0];
		if (a > b) std::swap(a, b);
		out.insert({a, b});
	}
	return out;
}

/** The T at n lanes: twelve channels (six port channels, six movement channels), three lanes each in lane order, three whole channel crossings. */
static void CheckT(int n)
{
	const PlanarTopology topology = Parse(PlanarTText(n));
	const PlanarChannels channels = BuildPlanarChannels(topology);
	CHECK(channels.channel_of_line.size() == topology.lines.size());
	CHECK(channels.channels.size() == 12);
	for (const std::string letters : {"EF", "AB", "CD", "ED", "CB", "AF", "A", "B", "C", "D", "E", "F"}) {
		INFO(letters);
		std::vector<std::string> expected;
		for (int k = 1; k <= n; k++) expected.push_back(fmt::format("{}{}", letters, k));
		CHECK(Names(topology, ChannelOf(topology, channels, letters + "1")) == expected);
	}
	CHECK(CrossingLetters(topology, channels) == std::set<std::pair<char, char>>{{'B', 'D'}, {'B', 'F'}, {'D', 'F'}});
	for (const PlanarChannelCrossing &c : channels.crossings) {
		CHECK(c.line_crossings.size() == static_cast<size_t>(n * n));
	}
	/* B crosses D and F; D crosses B and F; E crosses nothing. */
	CHECK(ChannelOf(topology, channels, "B1").crossings.size() == 2);
	CHECK(ChannelOf(topology, channels, "D1").crossings.size() == 2);
	CHECK(ChannelOf(topology, channels, "E1").crossings.empty());
	/* Every topology crossing is either a channel crossing or inside one node group. */
	size_t covered = 0;
	for (const PlanarChannelCrossing &c : channels.crossings) covered += c.line_crossings.size();
	for (const auto &g : channels.group_crossings) covered += g.size();
	CHECK(covered == topology.crossings.size());
}

TEST_CASE("planar channels: the T at one, two and three lanes", "[planar]")
{
	CheckT(1);
	CheckT(2);
	CheckT(3);
}

TEST_CASE("planar channels: LL2's choice legs are their own channels", "[planar]")
{
	const PlanarTopology topology = Parse(PlanarLL2Text());
	const PlanarChannels channels = BuildPlanarChannels(topology);
	CHECK(channels.channel_of_line.size() == topology.lines.size());
	/* The fixed lane E1 and the choice lane E2 leave the fray together; E2's choice leg E2b crosses D1's leg D1a, so it is a channel of its own. */
	CHECK(Names(topology, ChannelOf(topology, channels, "E2b")) == std::vector<std::string>{"E2b"});
	CHECK(Names(topology, ChannelOf(topology, channels, "D1a")) == std::vector<std::string>{"D1a"});
	CHECK(Names(topology, ChannelOf(topology, channels, "E1")) == std::vector<std::string>{"E1", "E2"});
	CHECK(Names(topology, ChannelOf(topology, channels, "EO1")) == std::vector<std::string>{"EO1", "EO2"});
	size_t covered = 0;
	for (const PlanarChannelCrossing &c : channels.crossings) covered += c.line_crossings.size();
	for (const auto &g : channels.group_crossings) covered += g.size();
	CHECK(covered == topology.crossings.size());
}

/** The site splitting or merging into the channel holding the named line. */
static const PlanarSite &SiteAt(const PlanarTopology &topology, const PlanarChannels &channels, std::string_view line)
{
	const uint32_t ch = channels.channel_of_line[PlanarLineByName(topology, line)];
	for (const PlanarSite &s : channels.sites) {
		if (s.channel == ch) return s;
	}
	FAIL("no site at " << line);
	return channels.sites.front();
}

/** The first line's name of a site's straight channel, or "tie". */
static std::string StraightName(const PlanarTopology &topology, const PlanarChannels &channels, const PlanarSite &site)
{
	if (site.straight == UINT32_MAX) return "tie";
	return topology.lines[channels.channels[site.straight].lines.front()].name;
}

TEST_CASE("planar channels: the T's sites, straight channel and side from the topology", "[planar]")
{
	for (int n = 1; n <= 3; n++) {
		INFO(n);
		const PlanarTopology topology = Parse(PlanarTText(n));
		const PlanarChannels channels = BuildPlanarChannels(topology);
		CHECK(channels.sites.size() == 6);
		struct Expect { const char *line; PlanarSiteKind kind; const char *straight; int side; bool prio; };
		for (const Expect &e : {
				Expect{"EF1", PlanarSiteKind::Fray, "E1", +1, true},   // F leaves right of E
				Expect{"AB1", PlanarSiteKind::Fray, "B1", -1, true},   // A leaves left of B
				Expect{"CD1", PlanarSiteKind::Fray, "tie", 0, false},  // C left 90, D right 90: the S fray side variant
				Expect{"ED1", PlanarSiteKind::Join, "E1", +1, true},   // D joins from the right
				Expect{"CB1", PlanarSiteKind::Join, "B1", -1, true},   // C joins from the left
				Expect{"AF1", PlanarSiteKind::Join, "tie", 0, false}}) {  // A and F both turn 90: the S join side variant
			INFO(e.line);
			const PlanarSite &site = SiteAt(topology, channels, e.line);
			CHECK(site.kind == e.kind);
			CHECK(site.nodes.size() == static_cast<size_t>(n));
			CHECK(site.others.size() == 2);
			CHECK(StraightName(topology, channels, site) == e.straight);
			CHECK(site.side == e.side);
			CHECK(site.prio == e.prio);
		}
	}
}

TEST_CASE("planar channels: LL2's sites", "[planar]")
{
	const PlanarTopology topology = Parse(PlanarLL2Text());
	const PlanarChannels channels = BuildPlanarChannels(topology);
	/* Three frays; per exit the two choice lanes' splits, the choice legs' merges and the exit's merges. */
	CHECK(channels.sites.size() == 15);
	CHECK(SiteAt(topology, channels, "EF1").kind == PlanarSiteKind::Fray);
	CHECK(SiteAt(topology, channels, "E1").kind == PlanarSiteKind::LaneSplit);
	CHECK(SiteAt(topology, channels, "EDj1").kind == PlanarSiteKind::Join);
	CHECK(SiteAt(topology, channels, "EO1").kind == PlanarSiteKind::Join);
	CHECK(SiteAt(topology, channels, "EO1").nodes.size() == 2);
}

TEST_CASE("planar channels: of several prio channels the one turning least runs straight", "[planar]")
{
	const PlanarTopology topology = Parse(PlanarLL2Text());
	const PlanarChannels channels = BuildPlanarChannels(topology);
	/* EF's lanes fray into E (straight on, prio lane E1) and F (turning south, prio lane F2): both prio, E turns least. */
	const PlanarSite &fray = SiteAt(topology, channels, "EF1");
	CHECK(fray.straight == channels.channel_of_line[PlanarLineByName(topology, "E1")]);
	CHECK(fray.side == 1);
	CHECK(fray.prio);
}

/** The legs at the node a line ends at (a split: its in line) or starts from (a merge: its out line). */
static const PlanarNodeLegs &LegsAt(const PlanarTopology &topology, const PlanarChannels &channels, std::string_view line, bool split)
{
	const PlanarLine &l = topology.lines[PlanarLineByName(topology, line)];
	const PlanarEnd &e = split ? l.to : l.from;
	REQUIRE(!e.port);
	return channels.node_legs[e.index];
}

/** A line's name, or "fork" for no straight line. */
static std::string LineName(const PlanarTopology &topology, uint32_t line)
{
	return line == UINT32_MAX ? "fork" : topology.lines[line].name;
}

TEST_CASE("planar channels: LL2's legs per node, prio per line", "[planar]")
{
	const PlanarTopology topology = Parse(PlanarLL2Text());
	const PlanarChannels channels = BuildPlanarChannels(topology);
	/* The exit merges: the prio lane runs straight, the merged choice lane joins from inside. */
	CHECK(LineName(topology, LegsAt(topology, channels, "EO1", false).straight) == "E1");
	CHECK(LegsAt(topology, channels, "EO1", false).side == 1);
	CHECK(LineName(topology, LegsAt(topology, channels, "EO2", false).straight) == "D2");
	CHECK(LegsAt(topology, channels, "EO2", false).side == -1);
	/* The choice merge: no prio, the leg turning least (E2a, straight on) runs straight; D1a comes up from the south, the right. */
	CHECK(LineName(topology, LegsAt(topology, channels, "EDj1", false).straight) == "E2a");
	CHECK(LegsAt(topology, channels, "EDj1", false).side == 1);
	/* A lane split: both legs go to one port, a fork; E2a is the left leg. */
	CHECK(LineName(topology, LegsAt(topology, channels, "E2", true).straight) == "fork");
	/* The entry fray's lanes: E straight, F leaving right. */
	CHECK(LineName(topology, LegsAt(topology, channels, "EF2", true).straight) == "E2");
	CHECK(LegsAt(topology, channels, "EF2", true).side == 1);
}

/** A line's share of a full lane's flow. */
static double ShareOf(const PlanarTopology &topology, const PlanarChannels &channels, std::string_view line)
{
	return channels.line_share[PlanarLineByName(topology, line)];
}

TEST_CASE("planar channels: LL2's line shares, full after a fray, half after a lane choice", "[planar]")
{
	const PlanarTopology topology = Parse(PlanarLL2Text());
	const PlanarChannels channels = BuildPlanarChannels(topology);
	/* Every movement holds a prio lane, so all traffic is main: the fray keeps the full share, a fork halves it. */
	for (const char *l : {"EF1", "E1", "E2", "F1", "F2", "B1", "B2", "D1", "D2", "EO1"}) {
		INFO(l);
		CHECK(ShareOf(topology, channels, l) == 1.0);
	}
	for (const char *l : {"E2a", "E2b", "D1a", "D1b", "F1a", "B1b"}) {
		INFO(l);
		CHECK(ShareOf(topology, channels, l) == 0.5);
	}
	/* The merged choice legs carry no more than their busiest feeder. */
	CHECK(ShareOf(topology, channels, "EDj1") == 0.5);
}

TEST_CASE("planar channels: the T's line shares, sideline movements half", "[planar]")
{
	const PlanarTopology topology = Parse(PlanarTText(2));
	const PlanarChannels channels = BuildPlanarChannels(topology);
	/* The T is a sideline hub: B and E hold prio, the S port's movements (A, C, D, F) are sideline traffic and cross single. */
	for (const char *l : {"EF1", "AB2", "B1", "E2", "ED1", "CB2"}) {
		INFO(l);
		CHECK(ShareOf(topology, channels, l) == 1.0);
	}
	for (const char *l : {"CD1", "D1", "F2", "A1", "C2", "AF1"}) {
		INFO(l);
		CHECK(ShareOf(topology, channels, l) == 0.5);
	}
}
