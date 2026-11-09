/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_skeleton.cpp Test functionality from junction_skeleton. */

#include "../stdafx.h"

#include "../3rdparty/catch2/catch.hpp"

#include "../junction_skeleton.h"

#include <algorithm>

#include "../safeguards.h"

using Rule = JunctionSkeletonRule;
using Push = JunctionSkeletonPush;

static JunctionSkeletonInputs StaggerInputs(int zmin0, int zmin1, int zmax0, int zmax1)
{
	JunctionSkeletonInputs in;
	in.zmin[0] = zmin0;
	in.zmin[1] = zmin1;
	in.zmax[0] = zmax0;
	in.zmax[1] = zmax1;
	in.nmax = 10;
	in.cover = 15;
	in.x_cap = 28;
	in.stagger = 1;
	
	return in;
}

/*
 * By hand, stagger 1, zmin 40/41, zmax 42/43, nmax 10, cover 15, k_w 1, k_sb 1, D145 offsets:
 * x_heA >= 42 + 1 = 43 and x_heB >= 43 + 1 = 44, x_heB = x_heA + 1, so Down gives 43 and 44.
 * x_hwA <= 40 - 1 = 39 and x_hwB <= 41 - 1 = 40, x_hwB = x_hwA + 1, so Up gives 39 and 40 (spans 3, within 11).
 * x_ls >= x_heA + 2 = 45 and >= x_heB + 1 = 45, so 45; x_sl >= x_ls + 2 = 47.
 * x_lm <= x_hwA - 2 = 37 and <= x_hwB - 3 = 37, so 37; x_nw <= 37 - (15 + 1) = 21 (cap 28); x_sb <= 21 - 1 = 20.
 */
TEST_CASE("SolveJunctionSkeletonW: staggered crossing gives staggered heads", "[junction][skeleton]")
{
	const JunctionSkeletonW w = SolveJunctionSkeletonW(StaggerInputs(40, 41, 42, 43));
	CHECK(w.conflict.empty());
	CHECK(w.x_he[0] == 43);
	CHECK(w.x_he[1] == 44);
	CHECK(w.x_hw[0] == 39);
	CHECK(w.x_hw[1] == 40);
	CHECK(w.x_ls == 45);
	CHECK(w.x_sl == 47);
	CHECK(w.x_lm == 37);
	CHECK(w.x_nw == 21);
	CHECK(w.x_sb == 20);
}

TEST_CASE("SolveJunctionSkeletonW: a span over the limit names the span", "[junction][skeleton]")
{
	const JunctionSkeletonW w = SolveJunctionSkeletonW(StaggerInputs(40, 40, 52, 52));
	REQUIRE(!w.conflict.empty());
	CHECK(std::find(w.conflict.begin(), w.conflict.end(), Rule::Span) != w.conflict.end());
}

TEST_CASE("SolveJunctionSkeleton: Down and Up pick the extremes", "[junction][skeleton]")
{
	const std::vector<JunctionSkeletonConstraint> c = {
		{1, 0, 3, Rule::Extra}, {0, 1, -9, Rule::Extra}, {2, 1, 2, Rule::Extra}, {0, 2, -20, Rule::Extra},
	};
	const JunctionSkeletonSolution a = SolveJunctionSkeleton(2, {{1, Push::Down}, {2, Push::Up}}, c);
	REQUIRE(a.x.size() == 3);
	CHECK(a.x[1] == 3);
	CHECK(a.x[2] == 20);
	const JunctionSkeletonSolution b = SolveJunctionSkeleton(2, {{2, Push::Down}, {1, Push::Up}}, c);
	REQUIRE(b.x.size() == 3);
	CHECK(b.x[2] == 5);
	CHECK(b.x[1] == 3);
}

TEST_CASE("SolveJunctionSkeleton: negative cycle reports its rules", "[junction][skeleton]")
{
	const JunctionSkeletonSolution s = SolveJunctionSkeleton(2, {{1, Push::Down}, {2, Push::Down}}, {{1, 2, 1, Rule::Span}, {2, 1, 1, Rule::HeadClear}});
	CHECK(s.x.empty());
	std::vector<Rule> got = s.conflict;
	std::sort(got.begin(), got.end());
	CHECK(got == std::vector<Rule>{Rule::Span, Rule::HeadClear});
}

TEST_CASE("SolveJunctionSkeleton: unbounded variable", "[junction][skeleton]")
{
	const JunctionSkeletonSolution s = SolveJunctionSkeleton(1, {{1, Push::Up}}, {{1, 0, 3, Rule::Extra}});
	CHECK(s.x.empty());
	CHECK(s.unbounded == 1);
}
