/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_milp.cpp Test functionality from junction_milp. */

#include "../../../stdafx.h"

#include "../../../3rdparty/catch2/catch.hpp"

#include "../../../junction/placement/junction_milp.h"

#include "../../../safeguards.h"

using Terms = std::vector<std::pair<uint32_t, double>>;

TEST_CASE("milp: integer optimum differs from the relaxation", "[junction][milp]")
{
	for (bool integer : {true, false}) {
		JunctionMilp m;
		uint32_t x = m.AddVariable(0, 10, 1, integer);
		uint32_t y = m.AddVariable(0, 10, 1, integer);
		Terms terms{{x, 1}, {y, 2}};
		m.AddRow(terms, 3.5, JunctionMilp::INF);
		REQUIRE(m.Solve() == JunctionMilp::Status::Optimal);
		if (integer) {
			CHECK(m.Objective() == Approx(2));
			CHECK(m.Value(x) == Approx(0));
			CHECK(m.Value(y) == Approx(2));
		} else {
			CHECK(m.Objective() == Approx(1.75));
		}
	}
}

TEST_CASE("milp: difference constraints solve to the least solution", "[junction][milp]")
{
	JunctionMilp m;
	uint32_t x0 = m.AddVariable(0, 100, 0, true);
	uint32_t x1 = m.AddVariable(0, 100, 0, true);
	uint32_t x2 = m.AddVariable(0, 100, 1, true);
	Terms a{{x1, 1}, {x0, -1}};
	Terms b{{x2, 1}, {x1, -1}};
	m.AddRow(a, 2, JunctionMilp::INF);
	m.AddRow(b, 3, JunctionMilp::INF);
	REQUIRE(m.Solve() == JunctionMilp::Status::Optimal);
	CHECK(m.Value(x2) == Approx(5));
}

TEST_CASE("milp: an infeasible model names its conflicting rows", "[junction][milp]")
{
	JunctionMilp m;
	uint32_t x = m.AddVariable(0, 100, 0, false);
	uint32_t y = m.AddVariable(0, 100, 0, false);
	Terms r0{{x, 1}};
	Terms r1{{y, 1}};
	Terms r2{{x, 1}};
	m.AddRow(r0, 3, JunctionMilp::INF);
	m.AddRow(r1, 1, JunctionMilp::INF);
	m.AddRow(r2, -JunctionMilp::INF, 1);
	REQUIRE(m.Solve() == JunctionMilp::Status::Infeasible);
	CHECK(m.InfeasibleRows() == std::vector<uint32_t>{0, 2});
}
