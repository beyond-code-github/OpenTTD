/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file planar_topology.cpp Test the planar topology: notation, derived crossings, ring and event order. */

#include "../../../stdafx.h"

#include "../../../3rdparty/catch2/catch.hpp"

#include "../../../junction/planar/planar_topology.h"

#include <algorithm>
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
	REQUIRE(!topology.lines.empty());
	return topology;
}

/** The crossings whose two lines both carry only routes of the given letters. */
static std::set<std::pair<char, char>> CrossingLetterPairs(const PlanarTopology &topology)
{
	std::set<std::pair<char, char>> out;
	for (const PlanarCrossing &c : topology.crossings) {
		const char a = topology.routes[topology.lines[c.a].routes.front()].name[0];
		const char b = topology.routes[topology.lines[c.b].routes.front()].name[0];
		out.insert({std::min(a, b), std::max(a, b)});
	}
	return out;
}

/** Every crossing appears exactly once in the event order of each of its two lines, and nowhere else. */
static void RequireEventsConsistent(const PlanarTopology &topology)
{
	REQUIRE(topology.events.size() == topology.lines.size());
	std::vector<int> seen(topology.crossings.size(), 0);
	for (size_t l = 0; l < topology.events.size(); l++) {
		for (uint32_t c : topology.events[l]) {
			REQUIRE(c < topology.crossings.size());
			REQUIRE((topology.crossings[c].a == l || topology.crossings[c].b == l));
			seen[c]++;
		}
	}
	for (int s : seen) REQUIRE(s == 2);
}

TEST_CASE("planar: the T at one lane derives exactly its three core crossings", "[junction][planar]")
{
	const PlanarTopology t = Parse(PlanarTText(1));
	CHECK(t.routes.size() == 6);
	CHECK(t.nodes.size() == 6);
	const std::vector<std::string> expected = {"B1×D1", "B1×F1", "D1×F1"};
	CHECK(PlanarCrossingNames(t) == expected);
	RequireEventsConsistent(t);
}

TEST_CASE("planar: the T at two lanes derives twelve core and six fray crossings", "[junction][planar]")
{
	const PlanarTopology t = Parse(PlanarTText(2));
	CHECK(t.routes.size() == 12);
	CHECK(t.crossings.size() == 18);
	/* The core: every line of B, D and F crosses both lines of the other two, 12 crossings on single-route lines. */
	size_t core = 0;
	for (const char *a : {"B1", "B2"}) {
		for (const char *b : {"D1", "D2", "F1", "F2"}) {
			const std::vector<std::string> names = PlanarCrossingNames(t);
			if (std::ranges::find(names, std::string(a) + "×" + b) != names.end()) core++;
		}
	}
	for (const char *a : {"D1", "D2"}) {
		for (const char *b : {"F1", "F2"}) {
			const std::vector<std::string> names = PlanarCrossingNames(t);
			if (std::ranges::find(names, std::string(a) + "×" + b) != names.end()) core++;
		}
	}
	CHECK(core == 12);
	/* The fray at each ring: lane 2 of one route crosses lane 1 of the other, on single-route lines; no port line crosses. */
	const std::set<std::pair<char, char>> letters = CrossingLetterPairs(t);
	const std::set<std::pair<char, char>> expected = {{'A', 'B'}, {'A', 'F'}, {'B', 'C'}, {'B', 'D'}, {'B', 'F'}, {'C', 'D'}, {'D', 'E'}, {'D', 'F'}, {'E', 'F'}};
	CHECK(letters == expected);
	for (const PlanarCrossing &c : t.crossings) {
		CHECK(t.lines[c.a].routes.size() == 1);
		CHECK(t.lines[c.b].routes.size() == 1);
	}
	RequireEventsConsistent(t);
	/* Bundles cross whole and keep their order: along B1, D1 and D2 are adjacent, as are F1 and F2. */
	const uint32_t b1 = PlanarLineByName(t, "B1");
	std::vector<std::string> along;
	for (uint32_t c : t.events[b1]) {
		const PlanarCrossing &x = t.crossings[c];
		along.push_back(t.lines[x.a == b1 ? x.b : x.a].name);
	}
	REQUIRE(along.size() == 6); // Four core crossings and the fray at each end (A2, C2).
	const auto pos = [&](const char *n) { return std::ranges::find(along, n) - along.begin(); };
	CHECK(std::abs(pos("D1") - pos("D2")) == 1);
	CHECK(std::abs(pos("F1") - pos("F2")) == 1);
}

TEST_CASE("planar: the ring follows the side orders from travel-left", "[junction][planar]")
{
	const PlanarTopology t = Parse(PlanarTText(2));
	std::vector<std::string> ring;
	for (uint32_t l : t.ring) ring.push_back(t.lines[l].name);
	/* Clockwise from the north-west corner: E side north to south (ED out heading E: left is north), then S east to west, then W south to north. */
	const std::vector<std::string> expected = {"ED1", "ED2", "AB2", "AB1", "AF1", "AF2", "CD2", "CD1", "CB1", "CB2", "EF2", "EF1"};
	CHECK(ring == expected);
}

TEST_CASE("planar: nodes are 1 -> 2 splits and 2 -> 1 merges with legs in authored order", "[junction][planar]")
{
	const PlanarTopology t = Parse(PlanarTText(1));
	for (const PlanarNode &n : t.nodes) {
		CHECK(n.in.size() + n.out.size() == 3);
	}
	const uint32_t ab = PlanarLineByName(t, "AB1");
	const PlanarNode &split = t.nodes[t.lines[ab].to.index];
	REQUIRE(split.IsSplit());
	CHECK(t.lines[split.out[0]].name == "A1");
	CHECK(t.lines[split.out[1]].name == "B1");
	/* Every route has one path from its entry port line to its exit port line. */
	for (const PlanarRoute &r : t.routes) {
		REQUIRE(r.paths.size() == 1);
		CHECK(r.paths[0].size() == 3);
	}
}

TEST_CASE("planar: malformed topologies are refused with a reason", "[junction][planar]")
{
	std::string error;
	CHECK(ParsePlanarTopology("topology x\nport W in E: AB1\nnode AB1 -> A1\n", &error).lines.empty());
	CHECK(error.find("1 -> 2") != std::string::npos);
	error.clear();
	CHECK(ParsePlanarTopology("topology x\nport W in E: A1\nport E out E: A1\nport N out N: A1\n", &error).lines.empty());
	CHECK(error.find("A1") != std::string::npos);
	error.clear();
	CHECK(ParsePlanarTopology("topology x\nport Q in E: A1\n", &error).lines.empty());
	CHECK(!error.empty());
}
