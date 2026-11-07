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
#include "../junction_route.h"
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

/** One candidate of the sweep. */
struct SweepCandidate {
	JunctionCandidateParams params;
	JunctionLayout layout;
};

/** Every candidate that builds, over the sweep of D3a. */
static const std::vector<SweepCandidate> &SweepCandidates(const JunctionSpec &spec)
{
	static const std::vector<SweepCandidate> out = [&spec]() {
	std::vector<SweepCandidate> out;
	const uint8_t g = spec.ml_gap;
	for (uint8_t height = g + 5; height <= g + 20; height++) {
		for (Money crossing_cost : {Money(0), spec.prices.bridge(3)}) {
			for (uint8_t slack = 0; slack <= 3; slack++) {
				const JunctionCandidateParams params{height, crossing_cost, slack};
				std::optional<JunctionLayout> layout = BuildJunctionCandidate(spec, params);
				if (layout.has_value()) out.push_back({params, std::move(*layout)});
			}
		}
	}
	return out;
	}();
	return out;
}

static const SweepCandidate *FirstClean(const JunctionSpec &spec, const std::vector<SweepCandidate> &candidates)
{
	for (const SweepCandidate &c : candidates) {
		if (CheckJunctionRules(c.layout, spec).empty()) return &c;
	}
	return nullptr;
}

TEST_CASE("BuildJunctionCandidate: some candidate is clean", "[junction]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const std::vector<SweepCandidate> &candidates = SweepCandidates(spec);
	const SweepCandidate *clean = FirstClean(spec, candidates);
	REQUIRE(clean != nullptr);
	WARN("first clean: height " << static_cast<int>(clean->params.height) << " crossing_cost " << clean->params.crossing_cost
			<< " joiner_slack " << static_cast<int>(clean->params.joiner_slack) << "\n" << RenderJunctionAscii(clean->layout));
}

TEST_CASE("BuildJunctionCandidate: every candidate is structurally sound", "[junction]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	for (const SweepCandidate &c : SweepCandidates(spec)) {
		INFO("height " << static_cast<int>(c.params.height) << " cost " << c.params.crossing_cost << " slack " << static_cast<int>(c.params.joiner_slack));
		for (const RuleViolation &v : CheckJunctionRules(c.layout, spec)) {
			CHECK(v.rule != JunctionRule::Connectivity);
			CHECK(v.rule != JunctionRule::Geometry);
		}
	}
}

TEST_CASE("BuildJunctionCandidate: the doubled lanes are equal and doubled", "[junction]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const std::vector<SweepCandidate> &candidates = SweepCandidates(spec);
	const SweepCandidate *clean = FirstClean(spec, candidates);
	REQUIRE(clean != nullptr);

	const std::vector<JunctionPort> ports = FindJunctionPorts(clean->layout);
	const JunctionCheckContext ctx = MakeJunctionCheckContext(clean->layout, spec, ports, {{JunctionPortRole::WIn, JunctionPortRole::WOut}});
	std::vector<uint> units;
	for (const JunctionPath &path : ctx.paths) {
		if (path.from != JunctionPortRole::WIn || path.to != JunctionPortRole::WOut) continue;
		uint total = 0;
		for (const JunctionPiece &piece : path.pieces) total += piece.units;
		units.push_back(total);
	}
	REQUIRE(units.size() == 2);
	CHECK(units[0] == units[1]);

	for (const RuleViolation &v : CheckBridges(MakeJunctionCheckContext(clean->layout, spec, ports, {}))) {
		CHECK(v.message.find("not doubled") == std::string::npos);
	}
}
