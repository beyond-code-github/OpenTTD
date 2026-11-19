/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file planar_shape_layer.cpp Test the Shape promise oracle on two hand variants, one mutation per promise (SPEC-shape §7 H1). */

#include "../../../stdafx.h"

#include "../../../3rdparty/catch2/catch.hpp"

#include "../../../junction/junction_spec.h"
#include "../../../junction/planar/planar_shape_layer.h"
#include "../../../junction/planar/planar_structure.h"
#include "../../../junction/planar/planar_topology.h"

#include <string>
#include <utility>
#include <vector>

#include "../../../safeguards.h"

/** A hand case: its topology and spec, and the hand-made shape of Structure's variant 0. */
struct ShapeCase {
	PlanarTopology topology;
	JunctionSpec spec = MakeTestJunctionSpec();
	PlanarShapeVariant v;
};

/**
 * Build Structure's variant 0 of a topology and wrap it with no shape.
 * @param text The topology text.
 * @param tag The tag variant 0 must have.
 * @return The case; its tracks, angles and structures are empty.
 */
static ShapeCase MakeCase(const std::string &text, const std::string &tag)
{
	ShapeCase c;
	std::string error;
	c.topology = ParsePlanarTopology(text, &error);
	REQUIRE(error.empty());
	const PlanarStructureResult r = BuildPlanarStructure(c.topology, c.spec, 1);
	INFO("why: " << r.why);
	REQUIRE(!r.variants.empty());
	REQUIRE(r.variants[0].tag == tag);
	c.v.structure = r.variants[0];
	c.v.cost = 0;
	return c;
}

/** Give the case the hand-made shape: per track index, its pieces and event_piece. */
static void SetTracks(ShapeCase &c, const std::vector<std::pair<std::vector<int>, std::vector<uint32_t>>> &tracks)
{
	REQUIRE(tracks.size() == c.v.structure.map.tracks.size());
	c.v.tracks.clear();
	for (const auto &[pieces, event_piece] : tracks) c.v.tracks.push_back({pieces, event_piece});
}

/** The one-lane T: its tracks are the dump's, track by track. */
static ShapeCase MakeT1()
{
	ShapeCase c = MakeCase(PlanarTText(1), "o3t0f0m0");
	SetTracks(c, {
		{{2}, {}}, {{6}, {}}, {{2}, {}}, {{6}, {}}, {{0}, {}}, {{4}, {}},
		{{5, 4}, {}}, {{6}, {0, 0}}, {{0, -1}, {}}, {{0, 1}, {0, 0}},
		{{2}, {}}, {{3, 4}, {0, 1}}, {{6}, {}}, {{2}, {}}, {{6}, {}}, {{2}, {}},
	});
	c.v.angles = {2, 2, 3};
	REQUIRE(c.v.structure.structures.size() == 2);
	REQUIRE(c.v.structure.structures[0].separator == 9);
	REQUIRE(c.v.structure.structures[1].separator == 11);
	c.v.structures = {{0, {3, -1}, {1, -1}}, {4, {1, -1}, {1, -1}}};
	return c;
}

/** The fray: its tracks are found by name. */
static ShapeCase MakeFray()
{
	ShapeCase c = MakeCase("topology fray\nport W in E: AB1\nport E out E: A1\nport S out S: B1\nnode AB1 -> A1, B1\n", "o0t0f0m0");
	std::vector<std::pair<std::vector<int>, std::vector<uint32_t>>> tracks;
	for (const PlanarTrack &t : c.v.structure.map.tracks) {
		if (t.name == "AB1") {
			tracks.push_back({{2}, {}});
		} else if (t.name == "A1") {
			tracks.push_back({{2}, {}});
		} else if (t.name == "B1") {
			tracks.push_back({{3, 4}, {}});
		} else {
			FAIL("unexpected track " << t.name);
		}
	}
	SetTracks(c, tracks);
	return c;
}

/** @return The index of the fray's track of that name. */
static size_t FrayTrack(const ShapeCase &c, const std::string &name)
{
	for (size_t t = 0; t < c.v.structure.map.tracks.size(); t++) {
		if (c.v.structure.map.tracks[t].name == name) return t;
	}
	FAIL("no track " << name);
	return 0;
}

/** Assert the oracle breaches, and every message starts with the id. */
static void RequireAllOf(const ShapeCase &c, const PlanarShapeVariant &v, const std::string &id)
{
	const std::vector<std::string> messages = CheckPlanarShapePromise(c.topology, c.spec, v);
	INFO(messages.size() << " messages, first: " << (messages.empty() ? "" : messages[0]));
	REQUIRE(!messages.empty());
	for (const std::string &m : messages) {
		INFO(m);
		CHECK(m.starts_with(id + ":"));
	}
}

TEST_CASE("planar shape layer: the oracle accepts the two valid hand variants", "[junction][planar][planarshape]")
{
	for (const ShapeCase &c : {MakeT1(), MakeFray()}) {
		const std::vector<std::string> messages = CheckPlanarShapePromise(c.topology, c.spec, c.v);
		INFO(c.v.structure.tag << ": " << messages.size() << " messages, first: " << (messages.empty() ? "" : messages[0]));
		CHECK(messages.empty());
	}
}

TEST_CASE("planar shape layer: H1 a stretch that turns and turns back", "[junction][planar][planarshape]")
{
	ShapeCase c = MakeFray();
	c.v.tracks[FrayTrack(c, "B1")].pieces = {3, 4, 3, 4};
	RequireAllOf(c, c.v, "H1");
}

TEST_CASE("planar shape layer: H2 a port end off its port's heading", "[junction][planar][planarshape]")
{
	ShapeCase c = MakeFray();
	c.v.tracks[FrayTrack(c, "A1")].pieces = {2, 1};
	RequireAllOf(c, c.v, "H2");
}

TEST_CASE("planar shape layer: H3 a leg that turns two eighths at a node", "[junction][planar][planarshape]")
{
	ShapeCase c = MakeFray();
	c.v.tracks[FrayTrack(c, "B1")].pieces = {4};
	RequireAllOf(c, c.v, "H3");
}

TEST_CASE("planar shape layer: H4 a stored angle that the headings do not give", "[junction][planar][planarshape]")
{
	ShapeCase c = MakeT1();
	c.v.angles[1] = 1;
	RequireAllOf(c, c.v, "H4");
}

TEST_CASE("planar shape layer: H5 a crossing on a diagonal piece of its separator", "[junction][planar][planarshape]")
{
	ShapeCase c = MakeT1();
	c.v.tracks[9].event_piece = {0, 1};
	c.v.angles[2] = 2;
	c.v.structures[0].middles = {2, -1};
	RequireAllOf(c, c.v, "H5");
}

TEST_CASE("planar shape layer: H6 middles that are not the least at the angles", "[junction][planar][planarshape]")
{
	ShapeCase c = MakeT1();
	c.v.structures[0].middles = {5, -1};
	RequireAllOf(c, c.v, "H6");
}

TEST_CASE("planar shape layer: H7 a face that winds twice and a U-turn", "[junction][planar][planarshape]")
{
	ShapeCase c = MakeFray();
	c.v.tracks[FrayTrack(c, "A1")].pieces = {2, 3, 4, 5, 6, 7, 8, 9, 10};
	RequireAllOf(c, c.v, "H7");
}

TEST_CASE("planar shape layer: H8 a row whose pieces differ from its lane's", "[junction][planar][planarshape]")
{
	ShapeCase c = MakeT1();
	c.v.tracks[15].pieces = {2, 3};
	RequireAllOf(c, c.v, "H8");
}

TEST_CASE("planar shape layer: H9 Structure's promise broken under the shape", "[junction][planar][planarshape]")
{
	ShapeCase c = MakeT1();
	c.v.structure.map.conflicts.push_back({0, 0, 0, 0});
	RequireAllOf(c, c.v, "H9");
}
