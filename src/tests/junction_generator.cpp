/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_generator.cpp Test functionality from junction_generator. */

#include "../stdafx.h"

#include "../3rdparty/catch2/catch.hpp"

#include "../junction_generator.h"
#include "../junction_rules.h"

#include "../safeguards.h"

TEST_CASE("GenerateJunction gives a rule-clean L_R T", "[junction][!mayfail]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	JunctionGenerateResult result = GenerateJunction(spec);
	REQUIRE(result.layout.has_value());
	CHECK(CheckJunctionRules(*result.layout, spec).empty());
}

TEST_CASE("ParseJunctionNotation", "[junction]")
{
	CHECK(ParseJunctionNotation("L_R").error == JunctionNotationError::None);
	CHECK(ParseJunctionNotation("L_R").gap == 1);
	CHECK(ParseJunctionNotation("LR").error == JunctionNotationError::None);
	CHECK(ParseJunctionNotation("LR").gap == 0);
	CHECK(ParseJunctionNotation("L___R").error == JunctionNotationError::None);
	CHECK(ParseJunctionNotation("L___R").gap == 3);

	CHECK(ParseJunctionNotation("LL_RR").error == JunctionNotationError::MultiLane);
	CHECK(ParseJunctionNotation("L_L__R_R").error == JunctionNotationError::MultiLane);

	for (std::string_view text : {"", "R_L", "L_X_R", "L_", "_R"}) {
		INFO(text);
		CHECK(ParseJunctionNotation(text).error == JunctionNotationError::Invalid);
	}
}

TEST_CASE("CurveSpeedLimitForSpacing", "[junction]")
{
	const uint spacings[] = {1, 2, 3, 4, 6, 8, 10, 12};
	const uint16_t expected[] = {88, 111, 132, 151, 183, 207, 223, 231};
	for (size_t i = 0; i < std::size(spacings); i++) {
		INFO(spacings[i]);
		CHECK(CurveSpeedLimitForSpacing(spacings[i], 0) == expected[i]);
	}
	CHECK(CurveSpeedLimitForSpacing(13, 0) == 231);
	CHECK(CurveSpeedLimitForSpacing(2, 1) == 166);
	CHECK(CurveSpeedLimitForSpacing(4, 2) == 301);
}

TEST_CASE("DeriveCurveSpacing", "[junction]")
{
	CHECK(DeriveCurveSpacing(88, 0, 10) == 1);
	CHECK(DeriveCurveSpacing(111, 0, 10) == 2);
	CHECK(DeriveCurveSpacing(160, 0, 10) == 5);
	CHECK(DeriveCurveSpacing(231, 0, 10) == 12);
	CHECK(DeriveCurveSpacing(232, 0, 10) == 20);
	CHECK(DeriveCurveSpacing(160, 0, 2) == 4);
	CHECK(DeriveCurveSpacing(160, 1, 10) == 2);
}
