/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */


/** @file planar_regions.cpp Test the line sides read from the topology: faces, ring and legs' order. */

#include "../../../stdafx.h"

#include "../../../3rdparty/catch2/catch.hpp"

#include "../../../junction/planar/planar_paths.h"
#include "../../../junction/planar/planar_plan.h"
#include "../../../junction/planar/planar_regions.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "../../../safeguards.h"

/** The hubs the sides are checked on: T2, T3 and LL². */
static std::vector<std::pair<std::string, std::string>> Hubs()
{
	return {{"T2", PlanarTText(2)}, {"T3", PlanarTText(3)}, {"LL2", PlanarLL2Text()}};
}

TEST_CASE("planar regions: the face map is valid", "[junction][planarregions]")
{
	for (const auto &[name, text] : Hubs()) {
		std::string error;
		const PlanarTopology topology = ParsePlanarTopology(text, &error);
		REQUIRE(error.empty());
		const PlanarLineSides sides(topology, BuildPlanarChannels(topology));
		INFO(name << ": " << sides.Why());
		CHECK(sides.Why().empty());
	}
}

TEST_CASE("planar regions: every pair of lines sharing a face is decided by it", "[junction][planarregions]")
{
	for (const auto &[name, text] : Hubs()) {
		std::string error;
		const PlanarTopology topology = ParsePlanarTopology(text, &error);
		REQUIRE(error.empty());
		const PlanarLineSides sides(topology, BuildPlanarChannels(topology));
		REQUIRE(sides.Why().empty());
		std::set<std::pair<uint32_t, uint32_t>> crossing;
		for (const PlanarCrossing &c : topology.crossings) {
			crossing.insert({c.a, c.b});
			crossing.insert({c.b, c.a});
		}
		const uint32_t n = static_cast<uint32_t>(topology.lines.size());
		size_t decided = 0;
		for (uint32_t p = 0; p < n; p++) {
			for (uint32_t q = 0; q < n; q++) {
				if (p == q) CHECK(sides.Side(p, q) == 0);
				if (crossing.contains({p, q})) CHECK(sides.Side(p, q) == 0);
				if (sides.Side(p, q) != 0) decided++;
			}
		}
		INFO(name << ": " << decided << " decided ordered pairs");
		CHECK(decided > 0);
		/* The two legs of a split or merge share the face between them: the left leg lies left of the right leg and the right leg right of the left. */
		for (const PlanarNode &node : topology.nodes) {
			const std::vector<uint32_t> &legs = node.IsSplit() ? node.out : node.in;
			for (size_t i = 0; i + 1 < legs.size(); i++) {
				INFO(name << ": legs " << topology.lines[legs[i]].name << " and " << topology.lines[legs[i + 1]].name);
				CHECK(sides.Side(legs[i], legs[i + 1]) == -1);
				CHECK(sides.Side(legs[i + 1], legs[i]) == 1);
			}
		}
	}
}

TEST_CASE("planar regions: known sides", "[junction][planarregions]")
{
	const auto side = [](const std::string &text, std::string_view p, std::string_view q) {
		std::string error;
		const PlanarTopology topology = ParsePlanarTopology(text, &error);
		REQUIRE(error.empty());
		const PlanarLineSides sides(topology, BuildPlanarChannels(topology));
		const uint32_t lp = PlanarLineByName(topology, p), lq = PlanarLineByName(topology, q);
		REQUIRE(lp != UINT32_MAX);
		REQUIRE(lq != UINT32_MAX);
		return static_cast<int>(sides.Side(lp, lq));
	};
	CHECK(side(PlanarLL2Text(), "A1", "A2a") == -1);
	CHECK(side(PlanarLL2Text(), "D1b", "D2") == -1);
	CHECK(side(PlanarTText(2), "C1", "F2") == 0); // no common face
}

TEST_CASE("planar regions: the face map follows the variant's channels", "[junction][planarregions]")
{
	std::string error;
	const PlanarTopology t = ParsePlanarTopology(PlanarTText(2), &error);
	REQUIRE(error.empty());
	const PlanarChannels ch = BuildPlanarChannels(t);
	REQUIRE(!PlanarTriangles(ch).empty());
	const auto max_runs = [](const PlanarLineSides &sides) {
		size_t most = 0;
		for (const PlanarFace &face : sides.Faces()) most = std::max(most, face.runs.size());
		return most;
	};
	const PlanarLineSides plain(t, ch);
	REQUIRE(plain.Why().empty());
	CHECK(max_runs(plain) <= 2);
	const PlanarChannels fl = FlipPlanarTriangles(ch, 1);
	const PlanarLineSides flipped(t, fl);
	INFO(flipped.Why());
	CHECK(flipped.Why().empty());
	CHECK(max_runs(flipped) == 4);
}

TEST_CASE("planar regions: every face is walls in runs", "[junction][planarregions]")
{
	for (const auto &[name, text] : Hubs()) {
		std::string error;
		const PlanarTopology t = ParsePlanarTopology(text, &error);
		REQUIRE(error.empty());
		const PlanarChannels ch = BuildPlanarChannels(t);
		const size_t triangles = PlanarTriangles(ch).size();
		for (uint64_t f = 0; f < (uint64_t{1} << triangles); f++) {
			const PlanarLineSides sides(t, FlipPlanarTriangles(ch, f));
			INFO(name << " flips " << f << ": " << sides.Why());
			REQUIRE(sides.Why().empty());
			std::map<std::pair<uint32_t, uint32_t>, size_t> seen;
			for (const PlanarFace &face : sides.Faces()) {
				REQUIRE(!face.runs.empty());
				if (face.runs.size() > 1) {
					for (size_t i = 0; i < face.runs.size(); i++) CHECK(face.runs[i].forward != face.runs[(i + 1) % face.runs.size()].forward);
				}
				for (const PlanarWallRun &run : face.runs) {
					for (const auto &stretch : run.stretches) seen[stretch]++;
				}
			}
			for (const auto &[stretch, count] : seen) CHECK(count <= 2);
		}
	}
}
