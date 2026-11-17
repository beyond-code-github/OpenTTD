/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file planar_model.cpp Test the planar model's required pieces and gap-sum curve rows (DESIGN-pieces §2, §4). */

#include "../../../stdafx.h"

#include "../../../3rdparty/catch2/catch.hpp"

#include "../../../junction/planar/planar_model.h"

#include <optional>
#include <string>

#include "../../../safeguards.h"

namespace {

/**
 * Start a strand at a fixed point.
 * @param M The model.
 * @param name The strand.
 * @param u The start's u.
 * @param v The start's v.
 */
void StartAt(PlanarModel::Model &M, const std::string &name, int u, int v)
{
	M.NewStrand(name);
	const auto st = M.strands.at(name).start;
	M.Add(PlanarModel::Eq(st[0], u));
	M.Add(PlanarModel::Eq(st[1], v));
}

} // namespace

TEST_CASE("planar model: a same-hand turn spaces its curves by the gap", "[junction][planarmodel]")
{
	PlanarModel::Model M;
	M.params = PlanarModel::Params{3, 4, 0, 5};
	StartAt(M, "a", 0, 1);
	M.AddPiece("a", PlanarModel::HE, 2);
	const auto se = M.AddPiece("a", PlanarModel::HSE, 1);
	M.AddPiece("a", PlanarModel::HS, 2);
	M.PieceCurves({{"a"}});
	REQUIRE(M.error.empty());
	PlanarModel::Solution sol;
	REQUIRE(M.Solve({}, sol));
	CHECK(sol.val[se.L.index] == 5);
}

TEST_CASE("planar model: curves two apart are spaced by the k=2 rule", "[junction][planarmodel]")
{
	PlanarModel::Model M;
	M.params = PlanarModel::Params{8, 4, 0, 1};
	StartAt(M, "a", 0, 1);
	M.AddPiece("a", PlanarModel::HE, 2);
	const auto se = M.AddPiece("a", PlanarModel::HSE, 1);
	const auto s = M.AddPiece("a", PlanarModel::HS, 2);
	M.AddPiece("a", PlanarModel::HSW, 1);
	M.PieceCurves({{"a"}});
	REQUIRE(M.error.empty());
	PlanarModel::Solution sol;
	REQUIRE(M.Solve({}, sol));
	CHECK(sol.val[se.L.index] + sol.val[s.L.index] >= 8);
}

TEST_CASE("planar model: a diagonal between parallel axis pieces is even, between perpendicular ones odd", "[junction][planarmodel]")
{
	{
		PlanarModel::Model M;
		M.params = PlanarModel::Params{3, 4, 0, 5};
		StartAt(M, "a", 0, 1);
		M.AddPiece("a", PlanarModel::HE, 2);
		const auto ne = M.AddPiece("a", PlanarModel::HNE, 1);
		M.AddPiece("a", PlanarModel::HE, 2);
		M.PieceCurves({{"a"}});
		REQUIRE(M.error.empty());
		PlanarModel::Solution sol;
		REQUIRE(M.Solve({}, sol));
		CHECK(sol.val[ne.L.index] == 2);
	}
	{
		PlanarModel::Model M;
		M.params = PlanarModel::Params{3, 4, 0, 1};
		StartAt(M, "b", 0, 41);
		M.AddPiece("b", PlanarModel::HE, 2);
		const auto se = M.AddPiece("b", PlanarModel::HSE, 1);
		M.AddPiece("b", PlanarModel::HS, 2);
		M.PieceCurves({{"b"}});
		REQUIRE(M.error.empty());
		PlanarModel::Solution sol;
		REQUIRE(M.Solve({}, sol));
		CHECK(sol.val[se.L.index] % 2 == 1);
	}
}

TEST_CASE("planar model: a turn of more than one eighth fails the build", "[junction][planarmodel]")
{
	PlanarModel::Model M;
	M.params = PlanarModel::Params{3, 4, 0, 5};
	StartAt(M, "a", 0, 1);
	M.AddPiece("a", PlanarModel::HE, 2);
	M.AddPiece("a", PlanarModel::HS, 2);
	REQUIRE(!M.error.empty());
	CHECK(M.error.find("more than one eighth") != std::string::npos);
}

TEST_CASE("planar model: a piece shorter than its heading allows fails the build", "[junction][planarmodel]")
{
	{
		PlanarModel::Model M;
		M.params = PlanarModel::Params{3, 4, 0, 5};
		StartAt(M, "a", 0, 1);
		M.AddPiece("a", PlanarModel::HE, 0);
		REQUIRE(!M.error.empty());
		CHECK(M.error.find("shorter than its heading allows") != std::string::npos);
	}
	{
		PlanarModel::Model M;
		M.params = PlanarModel::Params{3, 4, 0, 5};
		StartAt(M, "a", 1, 0);
		M.AddPiece("a", PlanarModel::HSE, 0);
		REQUIRE(!M.error.empty());
		CHECK(M.error.find("shorter than its heading allows") != std::string::npos);
	}
}

TEST_CASE("planar model: a strand starting on a diagonal starts on a tile-edge midpoint", "[junction][planarmodel]")
{
	{
		PlanarModel::Model M;
		M.params = PlanarModel::Params{3, 4, 0, 5};
		StartAt(M, "a", 0, 0);
		M.AddPiece("a", PlanarModel::HSE, 1);
		PlanarModel::Solution sol;
		CHECK(!M.Solve({}, sol));
	}
	{
		PlanarModel::Model M;
		M.params = PlanarModel::Params{3, 4, 0, 5};
		StartAt(M, "a", 1, 0);
		M.AddPiece("a", PlanarModel::HSE, 1);
		PlanarModel::Solution sol;
		CHECK(M.Solve({}, sol));
	}
}

TEST_CASE("planar model: an optional segment on a route of required pieces fails", "[junction][planarmodel]")
{
	PlanarModel::Model M;
	M.params = PlanarModel::Params{3, 4, 0, 5};
	StartAt(M, "a", 0, 1);
	M.AddPiece("a", PlanarModel::HE, 2);
	M.AddSeg("a", PlanarModel::HSE, 1, std::nullopt, 1.0, true);
	M.AddPiece("a", PlanarModel::HS, 2);
	M.PieceCurves({{"a"}});
	REQUIRE(!M.error.empty());
	CHECK(M.error.find("optional segment") != std::string::npos);
}
