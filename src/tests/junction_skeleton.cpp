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
	return in;
}

TEST_CASE("SolveJunctionSkeletonW: staggered crossing gives staggered heads", "[junction][skeleton]")
{
	const JunctionSkeletonW w = SolveJunctionSkeletonW(StaggerInputs(40, 41, 42, 43));
	CHECK(w.conflict.empty());
	CHECK(w.x_he[0] == 43);
	CHECK(w.x_he[1] == 44);
	CHECK(w.x_hw[0] == 39);
	CHECK(w.x_hw[1] == 40);
	CHECK(w.x_ls == 48);
	CHECK(w.x_sl == 50);
	CHECK(w.x_lm == 35);
	CHECK(w.x_nw == 19);
	CHECK(w.x_sb == 18);
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
