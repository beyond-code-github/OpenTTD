/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file planar_domain_map.cpp Test a drawing's domain input from its variant (D428). */

#include "../../../stdafx.h"

#include "../../../3rdparty/catch2/catch.hpp"

#include "../../../junction/junction_spec.h"
#include "../../../junction/planar/planar_domain.h"
#include "../../../junction/planar/planar_domain_map.h"
#include "../../../junction/planar/planar_shape_layer.h"
#include "../../../junction/planar/planar_structure.h"
#include "../../../junction/planar/planar_topology.h"

#include <array>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "../../../safeguards.h"

using Point = std::array<int, 2>;
using Points = std::vector<Point>;

/** The loaded X1 variant. */
static PlanarShapeVariant LoadX1()
{
	const std::string topo = "topology X1\nport W in E: AC1\nport S in N: BD1\nport N out N: C1, B1\nport E out E: A1, D1\nnode AC1 -> C1, A1\nnode BD1 -> B1, D1\ncross A1: B1<\ncross B1: A1>\n";
	std::string error;
	const PlanarTopology t = ParsePlanarTopology(topo, &error);
	REQUIRE(error.empty());
	const JunctionSpec spec = MakeTestJunctionSpec();
	std::vector<std::string> report;
	const PlanarStructureVariant s = LoadPlanarStructureVariant(t, "tunnel T1 A1: B1\nstraight AC1: A1\nstraight BD1: B1\n", spec, report, &error);
	REQUIRE(error.empty());
	PlanarShapeVariant v;
	std::string why;
	REQUIRE(LoadPlanarShapeVariant(t, s, "shape AC1: 2\nshape A1: 2\nshape C1: 1 0\nshape BD1: 0\nshape B1: 0\nshape D1: 1 2\nat A1: B1@0\nat B1: A1@0\n", v, why));
	return v;
}

/** The set of track indices whose names are given. */
static std::set<uint32_t> Named(const PlanarShapeVariant &v, std::initializer_list<const char *> names)
{
	std::set<uint32_t> out;
	for (const char *n : names) {
		for (uint32_t t = 0; t < v.structure.map.expanded.lines.size(); t++) {
			if (v.structure.map.expanded.lines[t].name == n) out.insert(t);
		}
	}
	return out;
}

static uint32_t Index(const PlanarShapeVariant &v, const char *name) { return *Named(v, {name}).begin(); }

static std::set<uint32_t> AsSet(const std::vector<uint32_t> &x) { return {x.begin(), x.end()}; }

TEST_CASE("planar domain: a drawing's input from its variant", "[junction][planar][planardomain]")
{
	const PlanarShapeVariant v = LoadX1();
	REQUIRE(v.structure.map.expanded.lines.size() == 6);
	std::map<std::string, Points> hand = {
		{"AC1", {{0, 5}, {4, 5}}},
		{"A1", {{4, 5}, {20, 5}}},
		{"C1", {{4, 5}, {7, 2}, {7, 0}}},
		{"BD1", {{11, 12}, {11, 10}}},
		{"B1", {{11, 10}, {11, 0}}},
		{"D1", {{11, 10}, {12, 9}, {20, 9}}},
	};
	auto draw = [&](int shift) {
		std::vector<Points> points(6);
		for (uint32_t t = 0; t < 6; t++) {
			const std::string &name = v.structure.map.expanded.lines[t].name;
			points[t] = hand[name];
			if (shift != 0 && (name == "B1" || name == "BD1" || name == "D1")) {
				for (Point &p : points[t]) p[0] += shift;
			}
		}
		return points;
	};

	SECTION("the hand drawing") {
		const std::vector<Points> points = draw(0);
		std::vector<std::string> notes;
		const PlanarDomainInput in = PlanarDomainInputOf(v, points, notes);
		REQUIRE(in.tracks.size() == 6);
		for (uint32_t t = 0; t < 6; t++) CHECK(in.tracks[t] == points[t]);
		REQUIRE(in.nodes.size() == 2);
		CHECK(in.nodes[0].at == Point{4, 5});
		CHECK(AsSet(in.nodes[0].in) == Named(v, {"AC1"}));
		CHECK(AsSet(in.nodes[0].out) == Named(v, {"C1", "A1"}));
		CHECK(in.nodes[1].at == Point{11, 10});
		CHECK(AsSet(in.nodes[1].in) == Named(v, {"BD1"}));
		CHECK(AsSet(in.nodes[1].out) == Named(v, {"B1", "D1"}));
		REQUIRE(in.structures.size() == 1);
		CHECK(in.structures[0].separator == Index(v, "A1"));
		CHECK(AsSet(in.structures[0].crossers) == Named(v, {"B1"}));
		CHECK(in.structures[0].tunnel);
		/* A tunnel's end run is the portal alone, 1 piece (D437): the span runs one piece further in at each end than with portal and ramp. */
		CHECK(in.structures[0].ends == 1);
		CHECK(in.structures[0].first == 2);
		CHECK(in.structures[0].last == 4);
		CHECK(in.structures[0].limit == v.structure.structures[0].limit[1]);
		CHECK(notes.empty());
		const PlanarDomainVerdict d = CheckPlanarDomain(in);
		CHECK(d.legal);
		CHECK(d.breaks.empty());
	}

	SECTION("points that do not match the variant give an empty input and a note") {
		std::vector<Points> points = draw(0);
		points.pop_back();
		std::vector<std::string> notes;
		const PlanarDomainInput in = PlanarDomainInputOf(v, points, notes);
		CHECK(in.tracks.empty());
		CHECK(in.nodes.empty());
		CHECK(in.structures.empty());
		CHECK(notes == std::vector<std::string>{"points do not match the variant"});
	}

	SECTION("a crossing on the separator's first piece leaves the structure out") {
		const std::vector<Points> points = draw(-6);
		std::vector<std::string> notes;
		const PlanarDomainInput in = PlanarDomainInputOf(v, points, notes);
		REQUIRE(notes.size() == 1);
		CHECK(notes[0].find("no room for its end runs") != std::string::npos);
		const PlanarDomainVerdict d = CheckPlanarDomain(in);
		std::set<std::string> rules;
		for (const PlanarDomainBreak &b : d.breaks) rules.insert(b.rule);
		CHECK(rules.count("diamond") == 1);
	}
}

TEST_CASE("planar domain: a node tile the separator and its crosser share is not a crossing", "[junction][planar][planardomain]")
{
	/* X1's A1 and C1 leave one split (AC1). C1 steps south-east off the node's tile, runs east and comes back north-east, passing A1's tunnel mid-run (tiles (5,2) and (6,2)); both tracks lie on the node's tile (2,2) at piece 0. That shared tile is where the two meet, not a crossing: the structure stays, with its end runs outside the real crossing (V's PSG 197 L4/L5 at game (81..82,173), L69/L70 at (104,180)). */
	PlanarShapeVariant v = LoadX1();
	const uint32_t a1 = Index(v, "A1"), c1 = Index(v, "C1");
	REQUIRE(v.structure.structures.size() == 1);
	v.structure.structures[0].crossers = {c1};
	std::vector<Points> points(6);
	points[Index(v, "AC1")] = {{0, 5}, {4, 5}};
	points[a1] = {{4, 5}, {20, 5}};
	points[c1] = {{4, 5}, {6, 7}, {10, 7}, {14, 3}};
	points[Index(v, "BD1")] = {{41, 12}, {41, 10}};
	points[Index(v, "B1")] = {{41, 10}, {41, 0}};
	points[Index(v, "D1")] = {{41, 10}, {42, 9}, {50, 9}};
	std::vector<std::string> notes;
	const PlanarDomainInput in = PlanarDomainInputOf(v, points, notes);
	CHECK(notes.empty());
	REQUIRE(in.structures.size() == 1);
	CHECK(in.structures[0].separator == a1);
	/* End run 1 (D437): the span starts and ends one piece further in than with the old portal and ramp. */
	CHECK(in.structures[0].first == 2);
	CHECK(in.structures[0].last == 5);
	const PlanarDomainVerdict d = CheckPlanarDomain(in);
	std::string text;
	for (const PlanarDomainBreak &b : d.breaks) text += b.rule + ": " + b.why + "\n";
	INFO(text);
	CHECK(d.legal);
}
