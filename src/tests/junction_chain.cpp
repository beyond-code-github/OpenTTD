/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_chain.cpp Test functionality from junction_chain. */

#include "../stdafx.h"

#include "../3rdparty/catch2/catch.hpp"

#include "../direction_func.h"
#include "../junction_chain.h"
#include "../junction_rules.h"

#include <random>

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
