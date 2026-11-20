/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file planar_topology.cpp Test the planar topology: notation, derived crossings, ring and event order. */

#include "../../../stdafx.h"

#include "../../../3rdparty/catch2/catch.hpp"

#include "../../../junction/planar/planar_plan.h"
#include "../../../junction/planar/planar_regions.h"
#include "../../../junction/planar/planar_topology.h"

#include <algorithm>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
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

/** The `cross` statements of a text: line name and its partners in travel order. */
static std::vector<std::pair<std::string, std::vector<std::string>>> CrossStatements(const std::string &text)
{
	const auto trim = [](std::string s) {
		const size_t a = s.find_first_not_of(" \t\r");
		const size_t b = s.find_last_not_of(" \t\r");
		return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
	};
	std::vector<std::pair<std::string, std::vector<std::string>>> out;
	std::istringstream in(text);
	std::string line;
	while (std::getline(in, line)) {
		line = trim(line.substr(0, line.find('#')));
		if (line.rfind("cross ", 0) != 0) continue;
		const size_t colon = line.find(':');
		REQUIRE(colon != std::string::npos);
		std::pair<std::string, std::vector<std::string>> statement{trim(line.substr(6, colon - 6)), {}};
		std::istringstream rest(line.substr(colon + 1));
		std::string name;
		while (std::getline(rest, name, ',')) statement.second.push_back(trim(name));
		out.push_back(std::move(statement));
	}
	return out;
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

TEST_CASE("planar: PSG 197 parses with its authored crossings", "[junction][planar]")
{
	std::ifstream file(JUNCTION_TOPOLOGY_DIR "/psg197.txt");
	REQUIRE(file.good());
	std::stringstream buffer;
	buffer << file.rdbuf();
	const std::string text = buffer.str();
	const PlanarTopology t = Parse(text);
	CHECK(t.crossings_authored);
	CHECK(t.lines.size() == 101);
	CHECK(t.crossings.size() == 122);
	for (const auto &[name, partners] : CrossStatements(text)) {
		const uint32_t l = PlanarLineByName(t, name);
		REQUIRE(l != UINT32_MAX);
		std::vector<std::string> along;
		for (uint32_t c : t.events[l]) along.push_back(t.lines[t.crossings[c].a == l ? t.crossings[c].b : t.crossings[c].a].name);
		std::vector<std::string> bare = partners;
		for (std::string &m : bare) m.pop_back();
		CHECK(along == bare);
	}
}

TEST_CASE("planar: authored crossings equal to the derived ones parse to the same topology", "[junction][planar]")
{
	for (const std::string text : {PlanarTText(1), PlanarTText(2), PlanarLL2Text()}) {
		const PlanarTopology derived = Parse(text);
		const PlanarTopology authored = Parse(AuthoredText(text));
		CHECK(authored.crossings_authored);
		CHECK(!derived.crossings_authored);
		REQUIRE(authored.crossings.size() == derived.crossings.size());
		for (size_t i = 0; i < derived.crossings.size(); i++) {
			CHECK(authored.crossings[i].a == derived.crossings[i].a);
			CHECK(authored.crossings[i].b == derived.crossings[i].b);
		}
		CHECK(authored.events == derived.events);
		const PlanarLineSides derived_sides(derived, BuildPlanarChannels(derived));
		const PlanarLineSides authored_sides(authored, BuildPlanarChannels(authored));
		REQUIRE(authored.b_from_left.size() == authored.crossings.size());
		for (uint32_t c = 0; c < derived.crossings.size(); c++) {
			CHECK(authored.b_from_left[c] == (derived_sides.CrossingFrom(c, derived.crossings[c].b) == +1));
			for (uint32_t line : {derived.crossings[c].a, derived.crossings[c].b}) {
				CHECK(authored_sides.CrossingFrom(c, line) == derived_sides.CrossingFrom(c, line));
			}
		}
		CHECK(authored_sides.Faces().size() == derived_sides.Faces().size());
	}
}

TEST_CASE("planar: authored crossings are refused with a reason", "[junction][planar]")
{
	const std::string base = PlanarTText(1);
	const PlanarTopology derived = Parse(base);
	const auto statements = CrossStatements(AuthoredText(base));
	REQUIRE(statements.size() >= 2);
	const auto error_of = [&](const std::vector<std::pair<std::string, std::vector<std::string>>> &s) {
		std::string error;
		ParsePlanarTopology(WithCross(base, s), &error);
		return error;
	};
	SECTION("a name that is no line") {
		auto s = statements;
		s[0].first = "Nope";
		CHECK(error_of(s).find("cross names unknown line Nope") != std::string::npos);
	}
	SECTION("a second statement for a line") {
		auto s = statements;
		s.push_back(s[0]);
		CHECK(error_of(s).find("has two cross statements") != std::string::npos);
	}
	SECTION("a line listing itself") {
		auto s = statements;
		s[0].second.push_back(s[0].first + ">");
		CHECK(error_of(s).find("crosses itself") != std::string::npos);
	}
	SECTION("a line listing one partner twice") {
		auto s = statements;
		s[0].second.push_back(s[0].second[0]);
		CHECK(error_of(s).find("twice") != std::string::npos);
	}
	SECTION("one partner removed from one line's list only") {
		auto s = statements;
		s[0].second.erase(s[0].second.begin());
		CHECK(error_of(s).find("is listed by") != std::string::npos);
	}
	SECTION("a partner without a mark") {
		auto s = statements;
		s[0].second[0].pop_back();
		CHECK(error_of(s).find("no direction mark") != std::string::npos);
	}
	SECTION("a pair with the same mark on both sides") {
		auto s = statements;
		const std::string a = s[0].first, b = s[0].second[0].substr(0, s[0].second[0].size() - 1);
		const char mark = s[0].second[0].back();
		for (auto &[name, partners] : s) {
			if (name != b) continue;
			for (std::string &m : partners) if (m.substr(0, m.size() - 1) == a) m.back() = mark;
		}
		CHECK(error_of(s).find("same direction") != std::string::npos);
	}
	SECTION("one pair removed from both lines' lists") {
		auto s = statements;
		const std::string a = s[0].first, b = s[0].second[0]; // b keeps its mark
		s[0].second.erase(s[0].second.begin());
		for (auto &[name, partners] : s) {
			if (name == b.substr(0, b.size() - 1)) partners.erase(std::ranges::find_if(partners, [&](const std::string &m) { return m.substr(0, m.size() - 1) == a; }));
		}
		CHECK(error_of(s).find("times but their ends interleave") != std::string::npos);
	}
	SECTION("one pair added to both lists where the derived topology has no crossing") {
		const auto port_line = [&](uint32_t l) { return derived.lines[l].from.port || derived.lines[l].to.port; };
		bool found = false;
		for (uint32_t a = 0; a < derived.lines.size() && !found; a++) {
			for (uint32_t b = a + 1; b < derived.lines.size() && !found; b++) {
				if (port_line(a) || port_line(b)) continue;
				const bool crossed = std::ranges::any_of(derived.crossings, [&](const PlanarCrossing &c) { return c.a == a && c.b == b; });
				if (crossed) continue;
				auto s = statements;
				for (const auto &[x, y, mark] : {std::tuple(a, b, ">"), std::tuple(b, a, "<")}) {
					const std::string &name = derived.lines[x].name;
					auto it = std::ranges::find_if(s, [&](const auto &st) { return st.first == name; });
					if (it == s.end()) {
						s.push_back({name, {}});
						it = s.end() - 1;
					}
					it->second.push_back(derived.lines[y].name + mark);
				}
				found = error_of(s).find("do not interleave") != std::string::npos;
			}
		}
		CHECK(found);
	}
}

TEST_CASE("planar: a topology without cross statements is not authored", "[junction][planar]")
{
	const PlanarTopology t = Parse(PlanarLL2Text());
	CHECK(!t.crossings_authored);
	CHECK(t.crossings.size() == 18);
}
