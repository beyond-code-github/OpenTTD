/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_chain.cpp Test functionality from junction_chain. */

#include "../stdafx.h"
#include "../3rdparty/fmt/format.h"

#include "../3rdparty/catch2/catch.hpp"

#include "../direction_func.h"
#include "../junction_chain.h"
#include "../junction_generator.h"
#include "../junction_rules.h"
#include "../junction_rules_internal.h"

#include <algorithm>
#include <random>
#include <set>
#include <string>

#include "../safeguards.h"

using Hand = JunctionCurveHand;

static bool SameGap(const JunctionCurveGap &g, uint16_t i, uint16_t j, int16_t gap)
{
	return g.i == i && g.j == j && g.min_gap == gap;
}

TEST_CASE("curve gaps of the test spec", "[junction][nodes]")
{
	const auto gaps = [](const std::vector<Hand> &h) { return JunctionCurveGaps(h, 4, 0, 160); };

	auto g = gaps({Hand::Left, Hand::Right});
	REQUIRE(g.size() == 1);
	CHECK(SameGap(g[0], 0, 1, 2));

	g = gaps({Hand::Left, Hand::Left});
	REQUIRE(g.size() == 1);
	CHECK(SameGap(g[0], 0, 1, 5));

	g = gaps({Hand::Left, Hand::Right, Hand::Left});
	REQUIRE(g.size() == 3);
	CHECK(SameGap(g[0], 0, 1, 2));
	CHECK(SameGap(g[1], 0, 2, 7));
	CHECK(SameGap(g[2], 1, 2, 2));

	CHECK(gaps({Hand::Left}).empty());
	CHECK(gaps({}).empty());
}

TEST_CASE("curve gaps agree with the checker", "[junction][nodes]")
{
	std::minstd_rand rng(1);
	static constexpr uint8_t tls[] = {2, 3, 4, 6, 8};
	uint conservative[std::size(tls)] = {};

	for (int chain = 0; chain < 5000; chain++) {
		const size_t tli = rng() % std::size(tls);
		const uint8_t tl = tls[tli];
		const int n = 2 * tl;
		const size_t k = 1 + rng() % 6;

		std::vector<Hand> hands;
		std::vector<int> pos;
		for (size_t c = 0; c < k; c++) {
			hands.push_back(rng() % 2 == 0 ? Hand::Left : Hand::Right);
			pos.push_back(c == 0 ? n : pos.back() + 1 + static_cast<int>(rng() % 16));
		}

		std::vector<Direction> dirs;
		Direction cur = Direction::NE;
		for (size_t c = 0; c < k; c++) {
			while (static_cast<int>(dirs.size()) < pos[c]) dirs.push_back(cur);
			cur = ChangeDir(cur, hands[c] == Hand::Left ? DirDiff::Left45 : DirDiff::Right45);
		}
		while (static_cast<int>(dirs.size()) < pos.back() + n) dirs.push_back(cur);

		const bool model = JunctionCurveGapsHold(JunctionCurveGaps(hands, tl, 0, 160), pos);
		const bool oracle = JunctionMinCurveSpeedOfDirections(dirs, tl, 0) >= 160;
		INFO("tl " << static_cast<int>(tl) << " curves " << k << " chain " << chain);
		REQUIRE(!(model && !oracle));
		if (!model && oracle) conservative[tli]++;
	}

	for (size_t i = 0; i < std::size(tls); i++) {
		if (tls[i] == 4) {
			CHECK(conservative[i] == 0);
		} else {
			WARN("tl " << static_cast<int>(tls[i]) << " conservative " << conservative[i]);
		}
	}
}

/** The flat line-router T of the test spec, built once. */
static const JunctionTMapResult &FlatT()
{
	static const JunctionTMapResult flat = ConstructJunctionTMap(MakeTestJunctionSpec(), nullptr, std::nullopt, true);
	return flat;
}

static std::vector<JunctionChain> FlatChains()
{
	REQUIRE(FlatT().layout.has_value());
	return ExtractJunctionChains(*FlatT().layout, MakeTestJunctionSpec());
}

/** The tiles of the chain's prio merges. */
static std::set<std::pair<int, int>> PrioMergeTiles(const JunctionChain &chain)
{
	std::set<std::pair<int, int>> tiles;
	for (const JunctionChainSwitch &s : chain.switches) {
		if (s.prio) tiles.insert({s.tile.x, s.tile.y});
	}
	return tiles;
}

TEST_CASE("chains of a hand-built S-bend", "[junction][nodes]")
{
	JunctionLayout layout;
	for (const JunctionRoute &route : {JunctionRoute{{0, 0}, Trackdir::X_SW, "FFFLFRFFFF"}, JunctionRoute{{9, 3}, Trackdir::X_NE, "FFFFFFFFF"}}) {
		const JunctionRaster raster = RasterizeJunctionRoute(route);
		REQUIRE(raster.error.empty());
		AddRasterToLayout(layout, raster);
	}
	NormaliseJunctionLayout(layout);
	WARN(RenderJunctionAscii(layout));

	const std::vector<JunctionChain> chains = ExtractJunctionChains(layout, MakeTestJunctionSpec());
	REQUIRE(chains.size() >= 1);
	WARN(FormatJunctionChain(chains[0]));
	REQUIRE(chains[0].curves.size() == 2);
	CHECK(chains[0].curves[0].hand != chains[0].curves[1].hand);
	CHECK(chains[0].curves[1].unit - chains[0].curves[0].unit == 2);
}

TEST_CASE("the flat T reads as chains", "[junction][nodes]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionLayout &layout = *FlatT().layout;
	const std::vector<JunctionChain> chains = FlatChains();
	CHECK(chains.size() == MakeJunctionCheckContext(layout, spec, FindJunctionPorts(layout), JunctionTMovements()).paths.size());

	/* Lane B has two traced paths that both pass the one prio merge, so the merges are counted by tile. */
	std::set<std::pair<int, int>> a_prio;
	std::set<std::pair<int, int>> b_prio;
	for (const JunctionChain &chain : chains) {
		WARN(FormatJunctionChain(chain));
		CHECK(chain.steps90 == 0);
		if (chain.from == JunctionPortRole::EIn && chain.to == JunctionPortRole::EOut) {
			CHECK(chain.curves.empty());
			a_prio.merge(PrioMergeTiles(chain));
		} else if (chain.from == JunctionPortRole::WIn && chain.to == JunctionPortRole::WOut) {
			b_prio.merge(PrioMergeTiles(chain));
		} else {
			CHECK(PrioMergeTiles(chain).empty());
		}
	}
	CHECK(a_prio.size() == 1);
	CHECK(b_prio.size() == 1);
}

TEST_CASE("the flat T meets the chain constraints", "[junction][nodes]")
{
	const std::vector<std::string> lines = JunctionChainViolations(FlatChains(), MakeTestJunctionSpec());
	for (const std::string &line : lines) INFO(line);
	CHECK(lines.empty());
}

TEST_CASE("the chain constraints catch a short prio coverage", "[junction][nodes]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	std::vector<JunctionChain> chains = FlatChains();
	const auto a = std::ranges::find_if(chains, [](const JunctionChain &c) { return c.from == JunctionPortRole::EIn && c.to == JunctionPortRole::EOut; });
	REQUIRE(a != chains.end());
	const auto merge = std::ranges::find_if(a->switches, [](const JunctionChainSwitch &s) { return s.prio; });
	REQUIRE(merge != a->switches.end());
	REQUIRE(merge->unit >= 4);

	a->curves.push_back({static_cast<uint16_t>(merge->unit - 4), Hand::Left, merge->tile});
	const size_t index = static_cast<size_t>(a - chains.begin());
	const std::vector<std::string> lines = JunctionChainViolations(chains, spec);
	REQUIRE(lines.size() == 1);
	CHECK(lines[0] == fmt::format("chain {} merge at unit {}: curve at unit {} inside the prio coverage", index, merge->unit, merge->unit - 4));
}
