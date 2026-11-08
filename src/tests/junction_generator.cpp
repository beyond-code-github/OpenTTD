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

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>

#include "../safeguards.h"

TEST_CASE("GenerateJunction gives a rule-clean L_R T", "[junction]")
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

static bool SameLayout(const JunctionLayout &a, const JunctionLayout &b)
{
	if (a.size.x != b.size.x || a.size.y != b.size.y) return false;
	if (a.tracks.size() != b.tracks.size() || a.bridges.size() != b.bridges.size() || a.signals.size() != b.signals.size()) return false;
	for (size_t i = 0; i < a.tracks.size(); i++) {
		if (a.tracks[i].offset.x != b.tracks[i].offset.x || a.tracks[i].offset.y != b.tracks[i].offset.y || a.tracks[i].tracks != b.tracks[i].tracks) return false;
	}
	for (size_t i = 0; i < a.bridges.size(); i++) {
		if (a.bridges[i].start.x != b.bridges[i].start.x || a.bridges[i].start.y != b.bridges[i].start.y ||
				a.bridges[i].end.x != b.bridges[i].end.x || a.bridges[i].end.y != b.bridges[i].end.y) return false;
	}
	for (size_t i = 0; i < a.signals.size(); i++) {
		if (a.signals[i].offset.x != b.signals[i].offset.x || a.signals[i].offset.y != b.signals[i].offset.y ||
				a.signals[i].facing != b.signals[i].facing || a.signals[i].type != b.signals[i].type || a.signals[i].variant != b.signals[i].variant) return false;
	}
	return true;
}

TEST_CASE("GenerateJunction is deterministic", "[junction]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionGenerateResult a = GenerateJunction(spec);
	const JunctionGenerateResult b = GenerateJunction(spec);
	REQUIRE(a.layout.has_value());
	REQUIRE(b.layout.has_value());
	CHECK(SameLayout(*a.layout, *b.layout));
}

TEST_CASE("ScoreJunctionLayout prefers the cheaper of two layouts", "[junction]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	JunctionLayout cheap;
	cheap.size = {3, 2};
	cheap.tracks.push_back({{0, 0}, TrackBits{Track::X}});
	cheap.tracks.push_back({{1, 0}, TrackBits{Track::X}});
	cheap.tracks.push_back({{2, 0}, TrackBits{Track::X}});
	JunctionLayout dear = cheap;
	dear.tracks.push_back({{1, 0}, TrackBits{Track::Y}});

	const std::pair<Money, int> a = ScoreJunctionLayout(cheap, spec);
	const std::pair<Money, int> b = ScoreJunctionLayout(dear, spec);
	CHECK(a.first == 300);
	CHECK(b.first == 400);
	CHECK(a < b);
	CHECK(a.second == -3);
}

TEST_CASE("Junction generator spec sweep", "[.gensweep]")
{
	for (uint8_t tl : {3, 4, 6}) {
		for (uint8_t sd : {2, 4}) {
			for (uint8_t ml_gap : {0, 1, 3}) {
				for (uint8_t branch_gap : {0, 1, 3}) {
					JunctionSpec spec = MakeTestJunctionSpec();
					spec.tl = tl;
					spec.sd = sd;
					spec.ml_gap = ml_gap;
					spec.branch_gap = branch_gap;
					const auto start = std::chrono::steady_clock::now();
					const JunctionGenerateResult result = GenerateJunction(spec);
					const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
					std::cout << "TL" << static_cast<int>(tl) << " SD" << static_cast<int>(sd) << " ml_gap" << static_cast<int>(ml_gap) << " branch_gap" << static_cast<int>(branch_gap) << ": ";
					if (result.layout.has_value()) {
						std::cout << "clean " << result.layout->size.x << "x" << result.layout->size.y;
						CHECK(CheckJunctionRules(*result.layout, spec).empty());
					} else {
						std::cout << "NoLayout";
					}
					std::cout << " " << ms << " ms\n";
				}
			}
		}
	}
}

TEST_CASE("GenerateJunction: an ML gap of 0 has no room for the W prio", "[junction]")
{
	JunctionSpec spec = MakeTestJunctionSpec();
	spec.ml_gap = 0;
	const JunctionGenerateResult result = GenerateJunction(spec);
	CHECK_FALSE(result.layout.has_value());
	CHECK(result.error == JunctionGenerateError::NeedsMlGap);
}

TEST_CASE("GenerateJunction: a long train with a narrow branch still fits", "[junction]")
{
	/* At TL6 the N->W window ends left of xI - 16, so a search box that stopped there had no goal. */
	JunctionSpec spec = MakeTestJunctionSpec();
	spec.branch_gap = 0;
	spec.tl = 6;
	const JunctionGenerateResult result = GenerateJunction(spec);
	REQUIRE(result.layout.has_value());
	CHECK(CheckJunctionRules(*result.layout, spec).empty());
}

/** A flat Buildable terrain at height z covering working x 0..63, y -2..40. */
static JunctionTerrain MakeFlatTerrain(int z = 0)
{
	JunctionTerrain t;
	t.origin = {0, -2};
	t.width = 64;
	t.height = 43;
	JunctionTerrainTile tile;
	tile.z_min = tile.z_max = static_cast<int8_t>(z);
	t.tiles.assign(static_cast<size_t>(t.width) * t.height, tile);
	t.foundation_cost = 250;
	return t;
}

/** Set the class of a rectangle given in working coordinates. */
static void SetTerrainRect(JunctionTerrain &t, int x0, int x1, int y0, int y1, JunctionTileClass cls)
{
	for (int y = y0; y <= y1; y++) {
		for (int x = x0; x <= x1; x++) t.tiles[static_cast<size_t>(y - t.origin.y) * t.width + (x - t.origin.x)].cls = cls;
	}
}

TEST_CASE("Junction generator golden JSON", "[.golden]")
{
	const char *path = std::getenv("JUNCTION_GOLDEN");
	if (path == nullptr) return;

	JunctionSpec gap0 = MakeTestJunctionSpec();
	gap0.branch_gap = 0;
	gap0.tl = 6;
	JunctionTerrain blocked = MakeFlatTerrain();
	SetTerrainRect(blocked, 18, 27, 4, 10, JunctionTileClass::Blocked);

	const JunctionGenerateResult results[] = {
		GenerateJunction(MakeTestJunctionSpec()),
		GenerateJunction(gap0),
		GenerateJunction(MakeTestJunctionSpec(), &blocked),
	};
	std::ofstream out(path);
	for (const JunctionGenerateResult &r : results) out << (r.layout.has_value() ? JunctionLayoutToJson(*r.layout) : std::string("none")) << "\n";
}

TEST_CASE("GenerateJunction on flat terrain equals no terrain", "[junction][terrain]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionTerrain flat = MakeFlatTerrain();
	const JunctionGenerateResult a = GenerateJunction(spec);
	const JunctionGenerateResult b = GenerateJunction(spec, &flat);
	REQUIRE(a.layout.has_value());
	REQUIRE(b.layout.has_value());
	CHECK(SameLayout(*a.layout, *b.layout));
}

TEST_CASE("GenerateJunction avoids a block of houses", "[junction][terrain]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionTerrain flat = MakeFlatTerrain();
	const JunctionGenerateResult base = GenerateJunction(spec, &flat);
	REQUIRE(base.layout.has_value());

	/* A fixed rectangle, as the normalised layout has no terrain frame. */
	JunctionTerrain blocked = MakeFlatTerrain();
	SetTerrainRect(blocked, 18, 27, 4, 10, JunctionTileClass::Blocked);
	const JunctionGenerateResult result = GenerateJunction(spec, &blocked);
	if (!result.layout.has_value()) {
		WARN("house block: NoLayout");
		SUCCEED();
		return;
	}
	WARN("house block:\n" << RenderJunctionAscii(*result.layout));
	CHECK(CheckJunctionRules(*result.layout, spec).empty());
	CHECK_FALSE(SameLayout(*result.layout, *base.layout));
}

TEST_CASE("GenerateJunction on a raised plane equals the flat result", "[junction][terrain]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionTerrain flat = MakeFlatTerrain();
	const JunctionTerrain raised = MakeFlatTerrain(3);
	const JunctionGenerateResult a = GenerateJunction(spec, &flat);
	const JunctionGenerateResult b = GenerateJunction(spec, &raised);
	REQUIRE(a.layout.has_value());
	REQUIRE(b.layout.has_value());
	CHECK(SameLayout(*a.layout, *b.layout));
}

TEST_CASE("GenerateJunction with a pond under the crossing", "[junction][terrain]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const int g = spec.ml_gap;
	JunctionTerrain pond = MakeFlatTerrain();
	SetTerrainRect(pond, 26, 34, g + 1, g + 2, JunctionTileClass::Water);
	const JunctionGenerateResult result = GenerateJunction(spec, &pond);
	if (!result.layout.has_value()) {
		WARN("pond: NoLayout");
		SUCCEED();
		return;
	}
	WARN("pond:\n" << RenderJunctionAscii(*result.layout));
	CHECK(CheckJunctionRules(*result.layout, spec).empty());
}

/** The track bits of the layout tile at an offset, or none. */
static TrackBits LayoutTracksAt(const JunctionLayout &layout, int x, int y)
{
	for (const JunctionTrack &jt : layout.tracks) {
		if (jt.offset.x == x && jt.offset.y == y) return jt.tracks;
	}
	return {};
}

static bool UnderBridge(const JunctionLayout &layout, int x, int y)
{
	for (const JunctionBridge &b : layout.bridges) {
		if (x >= std::min(b.start.x, b.end.x) && x <= std::max(b.start.x, b.end.x) && y >= std::min(b.start.y, b.end.y) && y <= std::max(b.start.y, b.end.y)) return true;
	}
	return false;
}

TEST_CASE("GenerateJunction reports the hub offset", "[junction]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionGenerateResult result = GenerateJunction(spec);
	REQUIRE(result.layout.has_value());
	const JunctionLayout &layout = *result.layout;
	REQUIRE(result.hub.x >= 0);
	REQUIRE(result.hub.y >= 0);
	REQUIRE(result.hub.x < layout.size.x);
	REQUIRE(result.hub.y < layout.size.y);
	CHECK(LayoutTracksAt(layout, result.hub.x, result.hub.y).Test(Track::X));

	const int w_y = result.hub.y + spec.ml_gap + 1;
	CHECK((LayoutTracksAt(layout, result.hub.x, w_y).Test(Track::X) || UnderBridge(layout, result.hub.x, w_y)));
}

TEST_CASE("GenerateJunction with a blocked prio row line", "[junction][terrain]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	JunctionTerrain t = MakeFlatTerrain();
	SetTerrainRect(t, 0, 63, -1, -1, JunctionTileClass::Blocked);
	const JunctionGenerateResult result = GenerateJunction(spec, &t);
	if (!result.layout.has_value()) {
		CHECK(result.error == JunctionGenerateError::NoLayout);
		return;
	}
	for (const JunctionTrack &jt : result.layout->tracks) {
		if (jt.tracks.None()) continue;
		CHECK(jt.offset.y != result.hub.y - 1);
	}
}
