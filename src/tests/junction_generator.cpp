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
#include "../junction_rules_internal.h"
#include "../track_func.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <set>

#include "../safeguards.h"

TEST_CASE("GenerateJunction gives a rule-clean L_R T", "[junction]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	JunctionGenerateResult result = GenerateJunction(spec);
	REQUIRE(result.layout.has_value());
	CHECK(CheckJunctionRules(*result.layout, spec).empty());
}

/** The two W-side measures of J1, measured from the layout's traced paths. */
struct WSideMeasures {
	int plain_before_head = -1; ///< Plain axis tiles between lane B's entry S-bend and its near bridge head.
	int nw_to_lane_merge = -1; ///< Tiles between the N->W merge and the lane merge, along x.
};

static WSideMeasures MeasureWSide(const JunctionLayout &layout, const JunctionSpec &spec)
{
	WSideMeasures out;
	const std::vector<JunctionPort> ports = FindJunctionPorts(layout);
	const JunctionCheckContext ctx = MakeJunctionCheckContext(layout, spec, ports, JunctionTMovements());
	std::vector<const JunctionPath *> lanes;
	const JunctionPath *nw = nullptr;
	for (const JunctionPath &path : ctx.paths) {
		if (path.from == JunctionPortRole::WIn && path.to == JunctionPortRole::WOut) lanes.push_back(&path);
		if (path.from == JunctionPortRole::NIn && path.to == JunctionPortRole::WOut) nw = &path;
	}
	if (lanes.size() != 2 || nw == nullptr) return out;

	/* Lane B is the lane with an entry S-bend: a non-axis piece before its near head. Count the axis pieces after the bend's last piece. */
	for (const JunctionPath *lane : lanes) {
		const auto head = std::ranges::find_if(lane->pieces, [](const JunctionPiece &piece) { return piece.on_bridge; });
		if (head == lane->pieces.end()) continue;
		int plain = 0;
		bool bend = false;
		for (auto it = head; it != lane->pieces.begin();) {
			--it;
			if (!IsDiagonalTrackdir(it->trackdir)) {
				bend = true;
				break;
			}
			plain++;
		}
		if (bend) out.plain_before_head = std::max(out.plain_before_head, plain);
	}

	/* The lane merge: the first tile of the suffix the two lanes share. The N->W merge: the first tile of N->W on that suffix. */
	auto tiles_of = [](const JunctionPath &path) {
		std::set<std::pair<int, int>> tiles;
		for (const JunctionPiece &piece : path.pieces) {
			if (!piece.on_bridge) tiles.insert({piece.tile.x, piece.tile.y});
		}
		return tiles;
	};
	const std::set<std::pair<int, int>> tiles_a = tiles_of(*lanes[0]);
	size_t first_shared = lanes[1]->pieces.size();
	while (first_shared > 0 && !lanes[1]->pieces[first_shared - 1].on_bridge && tiles_a.contains({lanes[1]->pieces[first_shared - 1].tile.x, lanes[1]->pieces[first_shared - 1].tile.y})) first_shared--;
	if (first_shared >= lanes[1]->pieces.size()) return out;
	const TileIndexDiffC lane_merge = lanes[1]->pieces[first_shared].tile;
	std::set<std::pair<int, int>> shared;
	for (size_t i = first_shared; i < lanes[1]->pieces.size(); i++) shared.insert({lanes[1]->pieces[i].tile.x, lanes[1]->pieces[i].tile.y});
	for (const JunctionPiece &piece : nw->pieces) {
		if (piece.on_bridge || !shared.contains({piece.tile.x, piece.tile.y})) continue;
		out.nw_to_lane_merge = std::abs(piece.tile.x - lane_merge.x);
		break;
	}
	return out;
}

/* J1: the W side builds the tight entry (at most one plain tile after lane B's S-bend) and the prio spacing (N->W merges less than 8 tiles from the lane merge). */
TEST_CASE("GenerateJunction: the W side is tight", "[junction]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionGenerateResult result = GenerateJunction(spec);
	REQUIRE(result.layout.has_value());
	CHECK(CheckJunctionRules(*result.layout, spec).empty());
	const WSideMeasures m = MeasureWSide(*result.layout, spec);
	WARN("plain tiles between lane B's S-bend and its near head: " << m.plain_before_head << ", N->W merge to lane merge: " << m.nw_to_lane_merge << ", size " << result.layout->size.x << "x" << result.layout->size.y << ", cost " << ScoreJunctionLayout(*result.layout, spec).first << "\n" << RenderJunctionAscii(*result.layout));
	REQUIRE(m.plain_before_head >= 0);
	REQUIRE(m.nw_to_lane_merge >= 0);
	CHECK(m.plain_before_head <= 1);
	CHECK(m.nw_to_lane_merge < 8);
}

/* D110 3: W->S splits off first; one plain tile, carrying a signal, sits between the SL split and the lane split. */
TEST_CASE("GenerateJunction: the SL splits first with one signal tile before the lane split", "[junction]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionGenerateResult result = GenerateJunction(spec);
	REQUIRE(result.layout.has_value());
	CHECK(CheckJunctionRules(*result.layout, spec).empty());
	const std::vector<JunctionPort> ports = FindJunctionPorts(*result.layout);
	const JunctionCheckContext ctx = MakeJunctionCheckContext(*result.layout, spec, ports, JunctionTMovements());
	const auto used = UsedTracks(ctx);
	int checked = 0;
	for (const JunctionPath &path : ctx.paths) {
		if (path.from != JunctionPortRole::WIn || path.to != JunctionPortRole::WOut) continue;
		std::vector<size_t> splits;
		for (const SwitchEvent &event : SwitchEventsOf(*result.layout, used, path)) {
			if (event.first) splits.push_back(event.second);
		}
		REQUIRE(splits.size() >= 2);
		CHECK(splits[1] - splits[0] == 2);
		CHECK(IsSignalledPiece(*result.layout, path.pieces[splits[0] + 1]));
		checked++;
	}
	CHECK(checked == 2);
	WARN("size " << result.layout->size.x << "x" << result.layout->size.y << "\n" << RenderJunctionAscii(*result.layout));
}

/* J1 / D116: G9c checks the coverage itself. This candidate puts the lane merge just beyond the coverage; it used to be flagged by the "one more signalled segment" extension, and G9b judges it now. */
TEST_CASE("CheckPrio: a lane merge just beyond the coverage is not a split inside it", "[junction]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	JunctionCandidateParams params{13, Money(0), 0, 2};
	params.bridge_margin = 2;
	params.lane_style = 0;
	const std::optional<JunctionLayout> layout = BuildJunctionCandidate(spec, params);
	REQUIRE(layout.has_value());
	for (const RuleViolation &v : CheckJunctionRules(*layout, spec)) {
		INFO(v.message << " at " << v.tile.x << "," << v.tile.y);
		CHECK(v.message.find("split inside prio coverage") == std::string::npos);
	}
}

/* Brief TUNE-footprint: the joiners sit as close as the prio rules allow, so the test junction is no wider than it was with the fixed margins (32). */
TEST_CASE("GenerateJunction: the test junction is no wider than before the margins were dropped", "[junction]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	JunctionGenerateResult result = GenerateJunction(spec);
	REQUIRE(result.layout.has_value());
	CHECK(CheckJunctionRules(*result.layout, spec).empty());
	CHECK(result.layout->size.x <= 32);
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
static std::vector<SweepCandidate> SweepCandidatesOf(const JunctionSpec &spec, uint8_t lane_style)
{
	std::vector<SweepCandidate> out;
	const uint8_t g = spec.ml_gap;
	for (uint8_t height = g + 5; height <= g + 20; height++) {
		for (Money crossing_cost : {Money(0), spec.prices.bridge(3)}) {
			for (uint8_t e_slack = 0; e_slack <= 3; e_slack++) {
				for (uint8_t w_slack = 0; w_slack <= 3; w_slack++) {
					JunctionCandidateParams params{height, crossing_cost, e_slack, w_slack};
					params.lane_style = lane_style;
					std::optional<JunctionLayout> layout = BuildJunctionCandidate(spec, params);
					if (layout.has_value()) out.push_back({params, std::move(*layout)});
				}
			}
		}
	}
	return out;
}

/** Every candidate that builds in the given lane style (margin 3). */
static const std::vector<SweepCandidate> &SweepCandidates(const JunctionSpec &spec, uint8_t lane_style = 0)
{
	static const std::vector<SweepCandidate> outs[2] = {SweepCandidatesOf(spec, 0), SweepCandidatesOf(spec, 1)};
	return outs[lane_style];
}

/** The largest middle-tile count of the layout's bridges that run along x. */
static int MaxXBridgeSpan(const JunctionLayout &layout)
{
	int span = -1;
	for (const JunctionBridge &b : layout.bridges) {
		if (b.start.y != b.end.y) continue;
		span = std::max(span, std::abs(b.end.x - b.start.x) - 1);
	}
	return span;
}

TEST_CASE("GenerateJunction: doubled bridges are no longer than needed", "[junction]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionGenerateResult result = GenerateJunction(spec);
	REQUIRE(result.layout.has_value());

	/* The winner of the margin 3 only sweep. */
	std::optional<JunctionLayout> old_best;
	std::pair<int, Money> old_score{};
	int old_area = 0;
	for (const SweepCandidate &c : SweepCandidates(spec)) {
		if (!CheckJunctionRules(c.layout, spec).empty()) continue;
		const std::pair<int, Money> score = ScoreJunctionLayout(c.layout, spec);
		const int area = c.layout.size.x * c.layout.size.y;
		if (!old_best.has_value() || score < old_score || (score == old_score && area < old_area)) {
			old_best = c.layout;
			old_score = score;
			old_area = area;
		}
	}
	REQUIRE(old_best.has_value());

	const int new_span = MaxXBridgeSpan(*result.layout);
	const int old_span = MaxXBridgeSpan(*old_best);
	WARN("bridge span with the margin sweep " << new_span << ", with margin 3 " << old_span);
	CHECK(new_span >= 0);
	CHECK(new_span < old_span);
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
			<< " e_slack " << static_cast<int>(clean->params.e_slack) << " w_slack " << static_cast<int>(clean->params.w_slack) << "\n" << RenderJunctionAscii(clean->layout));
}

TEST_CASE("BuildJunctionCandidate: every candidate is structurally sound", "[junction]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	for (const SweepCandidate &c : SweepCandidates(spec)) {
		INFO("height " << static_cast<int>(c.params.height) << " cost " << c.params.crossing_cost << " e_slack " << static_cast<int>(c.params.e_slack) << " w_slack " << static_cast<int>(c.params.w_slack));
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

TEST_CASE("BuildJunctionCandidate: the doubled ML lanes are mirror images", "[junction]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const std::vector<SweepCandidate> &candidates = SweepCandidates(spec, 0);
	const SweepCandidate *clean = FirstClean(spec, candidates);
	REQUIRE(clean != nullptr);

	const std::vector<JunctionPort> ports = FindJunctionPorts(clean->layout);
	const JunctionCheckContext ctx = MakeJunctionCheckContext(clean->layout, spec, ports, JunctionTMovements());
	std::vector<std::pair<uint, uint>> shapes; ///< (half pieces, turns) per path.
	for (const JunctionPath &path : ctx.paths) {
		if (path.from != JunctionPortRole::WIn || path.to != JunctionPortRole::WOut) continue;
		uint half = 0;
		uint turns = 0;
		for (size_t i = 0; i < path.pieces.size(); i++) {
			if (!IsDiagonalTrackdir(path.pieces[i].trackdir)) half++;
			if (i > 0 && path.pieces[i].trackdir != path.pieces[i - 1].trackdir) turns++;
		}
		shapes.emplace_back(half, turns);
	}
	REQUIRE(shapes.size() == 2);
	CHECK(shapes[0].first == shapes[1].first);
	CHECK(shapes[0].second == shapes[1].second);
}

/** The half-piece count, turn count and units of each WIn->WOut path. */
static std::vector<std::array<uint, 3>> WPathShapes(const JunctionLayout &layout, const JunctionSpec &spec)
{
	const std::vector<JunctionPort> ports = FindJunctionPorts(layout);
	const JunctionCheckContext ctx = MakeJunctionCheckContext(layout, spec, ports, JunctionTMovements());
	std::vector<std::array<uint, 3>> shapes;
	for (const JunctionPath &path : ctx.paths) {
		if (path.from != JunctionPortRole::WIn || path.to != JunctionPortRole::WOut) continue;
		uint half = 0;
		uint turns = 0;
		uint units = 0;
		for (size_t i = 0; i < path.pieces.size(); i++) {
			if (!IsDiagonalTrackdir(path.pieces[i].trackdir)) half++;
			if (i > 0 && path.pieces[i].trackdir != path.pieces[i - 1].trackdir) turns++;
			units += path.pieces[i].units;
		}
		shapes.push_back({half, turns, units});
	}
	return shapes;
}

/** The first clean candidate of lane style 1 over the full sweep space, including the margin. */
static std::optional<JunctionLayout> CleanParallelogram(const JunctionSpec &spec, int *clean_count = nullptr, int *built_count = nullptr)
{
	std::optional<JunctionLayout> first;
	const uint8_t g = spec.ml_gap;
	for (uint8_t height = g + 5; height <= g + 20; height++) {
		for (Money crossing_cost : {Money(0), spec.prices.bridge(3)}) {
			for (uint8_t e_slack = 0; e_slack <= 3; e_slack++) {
				for (uint8_t w_slack = 0; w_slack <= 3; w_slack++) {
					for (uint8_t margin = 1; margin <= 3; margin++) {
						JunctionCandidateParams params{height, crossing_cost, e_slack, w_slack};
						params.bridge_margin = margin;
						params.lane_style = 1;
						std::optional<JunctionLayout> layout = BuildJunctionCandidate(spec, params);
						if (!layout.has_value()) continue;
						if (built_count != nullptr) (*built_count)++;
						if (!CheckJunctionRules(*layout, spec).empty()) continue;
						if (clean_count != nullptr) (*clean_count)++;
						if (!first.has_value()) first = std::move(layout);
					}
				}
			}
		}
	}
	return first;
}

TEST_CASE("a parallelogram candidate is clean", "[junction]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	int clean1 = 0;
	int built1 = 0;
	const std::optional<JunctionLayout> layout = CleanParallelogram(spec, &clean1, &built1);
	REQUIRE(layout.has_value());
	int clean0 = 0;
	for (const SweepCandidate &c : SweepCandidates(spec, 0)) {
		if (CheckJunctionRules(c.layout, spec).empty()) clean0++;
	}
	int clean1_m3 = 0;
	for (const SweepCandidate &c : SweepCandidates(spec, 1)) {
		if (CheckJunctionRules(c.layout, spec).empty()) clean1_m3++;
	}
	WARN("style 1: " << clean1 << " clean of " << built1 << " built over margins 1-3; at margin 3 " << clean1_m3 << " clean of "
			<< SweepCandidates(spec, 1).size() << " built; style 0 at margin 3: " << clean0 << " clean of " << SweepCandidates(spec, 0).size() << " built");
	CHECK(CheckJunctionRules(*layout, spec).empty());
}

TEST_CASE("parallelogram bridges are adjacent", "[junction]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const std::optional<JunctionLayout> layout = CleanParallelogram(spec);
	REQUIRE(layout.has_value());
	std::vector<int> rows;
	for (const JunctionBridge &b : layout->bridges) {
		if (b.start.y == b.end.y && std::abs(b.end.x - b.start.x) > 1) rows.push_back(b.start.y);
	}
	/* The ML bridges are the doubled pair; the branch bridges, if any, run across them. */
	bool adjacent = false;
	for (size_t i = 0; i < rows.size(); i++) {
		for (size_t j = i + 1; j < rows.size(); j++) {
			if (std::abs(rows[i] - rows[j]) == 1) adjacent = true;
		}
	}
	CHECK(adjacent);
	WARN("\n" << RenderJunctionAscii(*layout));
}

TEST_CASE("BuildJunctionCandidate: the parallelogram lanes have equal half pieces and units", "[junction]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const std::optional<JunctionLayout> layout = CleanParallelogram(spec);
	REQUIRE(layout.has_value());
	const std::vector<std::array<uint, 3>> shapes = WPathShapes(*layout, spec);
	REQUIRE(shapes.size() == 2);
	CHECK(shapes[0][0] == shapes[1][0]);
	CHECK(shapes[0][2] == shapes[1][2]);
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

TEST_CASE("ScoreJunctionLayout prefers the cheaper of two layouts with equal tiles", "[junction]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	JunctionLayout cheap;
	cheap.size = {3, 2};
	cheap.tracks.push_back({{0, 0}, TrackBits{Track::X}});
	cheap.tracks.push_back({{1, 0}, TrackBits{Track::X}});
	cheap.tracks.push_back({{2, 0}, TrackBits{Track::X}});
	JunctionLayout dear = cheap;
	dear.tracks.push_back({{1, 0}, TrackBits{Track::Y}});

	const std::pair<int, Money> a = ScoreJunctionLayout(cheap, spec);
	const std::pair<int, Money> b = ScoreJunctionLayout(dear, spec);
	CHECK(a.second == 300);
	CHECK(b.second == 400);
	CHECK(a < b);
	CHECK(a.first == 3);
	CHECK(b.first == 3);

	/* D114: fewer tiles beat a lower cost. */
	JunctionLayout compact = dear;
	compact.tracks.pop_back();
	compact.tracks.pop_back();
	compact.tracks.pop_back();
	compact.tracks.push_back({{0, 0}, TrackBits{Track::Y}});
	compact.tracks.push_back({{1, 0}, TrackBits{Track::Y}});
	compact.tracks.push_back({{0, 0}, TrackBits{Track::Y}});
	JunctionLayout wide = cheap;
	wide.tracks.push_back({{3, 0}, TrackBits{Track::X}});
	wide.tracks.push_back({{4, 0}, TrackBits{Track::X}});
	CHECK(ScoreJunctionLayout(wide, spec).first == 5);
	CHECK(ScoreJunctionLayout(compact, spec) < ScoreJunctionLayout(wide, spec));
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

TEST_CASE("GenerateJunction: a longer prio covers two train lengths", "[junction]")
{
	const JunctionSpec base_spec = MakeTestJunctionSpec();
	const JunctionGenerateResult base = GenerateJunction(base_spec);
	REQUIRE(base.layout.has_value());

	JunctionSpec spec = base_spec;
	spec.prio_tl = 2;
	const JunctionGenerateResult result = GenerateJunction(spec);
	REQUIRE(result.layout.has_value());
	CHECK(CheckJunctionRules(*result.layout, spec).empty());
	/* The upstream distance is not measured here (it would need a new helper); the layout must be wider instead. */
	CHECK(result.layout->size.x > base.layout->size.x);
}

TEST_CASE("GenerateJunction: prios keep their joins at SD 2", "[junction]")
{
	JunctionSpec spec = MakeTestJunctionSpec();
	spec.sd = 2;
	spec.prio_tl = 1;
	const JunctionGenerateResult result = GenerateJunction(spec);
	REQUIRE(result.layout.has_value());
	const JunctionLayout &layout = *result.layout;

	const std::vector<JunctionPort> ports = FindJunctionPorts(layout);
	const JunctionCheckContext ctx = MakeJunctionCheckContext(layout, spec, ports, JunctionTMovements());
	const auto used = UsedTracks(ctx);
	int prios = 0;
	for (const JunctionPath &ml : ctx.paths) {
		const bool is_ml = (ml.from == JunctionPortRole::EIn && ml.to == JunctionPortRole::EOut) || (ml.from == JunctionPortRole::WIn && ml.to == JunctionPortRole::WOut);
		if (!is_ml) continue;
		for (const SwitchEvent &event : SwitchEventsOf(layout, used, ml)) {
			if (event.first) continue;
			const JunctionPiece &merge = ml.pieces[event.second];
			if (merge.on_bridge || !IsDiagonalTrackdir(merge.trackdir)) continue;

			/* A prio merge: the merge tile holds a track no path uses, the prio's join. */
			const auto merge_used = used.find({merge.tile.x, merge.tile.y});
			const TrackBits merge_bits = TracksAt(layout, merge.tile);
			if (merge_used == used.end() || merge_bits == merge_used->second) continue;
			prios++;

			/* The farthest join upstream: an ML tile with the axis plus a half piece, on the merge's row. */
			const Track axis = TrackdirToTrack(merge.trackdir);
			int farthest = 0;
			for (size_t i = 0; i < event.second; i++) {
				const JunctionPiece &piece = ml.pieces[i];
				if (piece.on_bridge || !IsDiagonalTrackdir(piece.trackdir) || TrackdirToTrack(piece.trackdir) != axis) continue;
				const auto piece_used = used.find({piece.tile.x, piece.tile.y});
				if (piece_used == used.end() || TracksAt(layout, piece.tile) == piece_used->second) continue;
				const int distance = std::abs(piece.tile.x - merge.tile.x) + std::abs(piece.tile.y - merge.tile.y);
				if (piece.tile.x != merge.tile.x && piece.tile.y != merge.tile.y) continue;
				farthest = std::max(farthest, distance);
			}
			CHECK(farthest >= static_cast<int>(spec.tl) - 2);
		}
	}
	CHECK(prios > 0);
}

TEST_CASE("GenerateJunction: an ML gap of 0 has no room for the W prio", "[junction]")
{
	JunctionSpec spec = MakeTestJunctionSpec();
	spec.ml_gap = 0;
	const JunctionGenerateResult result = GenerateJunction(spec);
	CHECK_FALSE(result.layout.has_value());
	CHECK(result.error == JunctionGenerateError::NeedsMlGap);
}

TEST_CASE("GenerateJunction: a short train with a narrow branch fits", "[junction]")
{
	/* At TL3 E->S used to end on the tile above the N stub, leaving N->E no way out (D3d). */
	JunctionSpec spec = MakeTestJunctionSpec();
	spec.branch_gap = 0;
	spec.tl = 3;
	const JunctionGenerateResult result = GenerateJunction(spec);
	REQUIRE(result.layout.has_value());
	CHECK(CheckJunctionRules(*result.layout, spec).empty());
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

/** A flat Buildable terrain at height z covering the default frame. */
static JunctionTerrain MakeFlatTerrain(int z = 0, const JunctionFrame &frame = JunctionFrame{})
{
	JunctionTerrain t;
	t.origin = {0, static_cast<int16_t>(frame.terrain_origin_y)};
	t.width = frame.width;
	t.height = frame.terrain_height;
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

TEST_CASE("the frame grows with prio coverage", "[junction][frame]")
{
	JunctionSpec spec = MakeTestJunctionSpec();
	spec.prio_tl = 1;
	for (const uint8_t tl : {uint8_t(2), uint8_t(4)}) {
		spec.tl = tl;
		const JunctionFrame f1 = MakeJunctionFrame(spec);
		const JunctionFrame def{};
		CHECK(f1.hub_x == def.hub_x);
		CHECK(f1.width == def.width);
		CHECK(f1.reach_east == def.reach_east);
		CHECK(f1.terrain_height == def.terrain_height);
		CHECK(f1.terrain_origin_y == def.terrain_origin_y);
	}
	spec.tl = 4;
	spec.prio_tl = 3;
	const JunctionFrame f3 = MakeJunctionFrame(spec);
	CHECK(f3.hub_x == 38);
	CHECK(f3.width == 80);
	CHECK(f3.reach_east == 24);
	CHECK(f3.terrain_height == 43);
	CHECK(f3.terrain_origin_y == -2);
}

TEST_CASE("library fit uses the library's frame", "[junction][frame][library]")
{
	JunctionSpec spec = MakeTestJunctionSpec();
	spec.prio_tl = 2;
	const std::vector<JunctionLibraryEntry> library = BuildJunctionLibrary(spec);
	if (library.empty()) {
		WARN("library is empty at prio 2");
		return;
	}
	CHECK(library.front().frame.hub_x == MakeJunctionFrame(spec).hub_x);
	const JunctionTerrain flat = MakeFlatTerrain(0, MakeJunctionFrame(spec));
	CHECK(FitJunctionLibrary(library, flat).has_value());
}

TEST_CASE("FitJunctionLibrary on flat terrain is at least as good as GenerateJunction", "[junction][terrain][library]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionTerrain flat = MakeFlatTerrain();

	const auto t0 = std::chrono::steady_clock::now();
	const std::vector<JunctionLibraryEntry> library = BuildJunctionLibrary(spec);
	const auto t1 = std::chrono::steady_clock::now();
	REQUIRE_FALSE(library.empty());

	std::optional<JunctionLibraryFit> fit;
	for (int i = 0; i < 100; i++) fit = FitJunctionLibrary(library, flat);
	const auto t2 = std::chrono::steady_clock::now();
	const double build_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
	const double fit_us = std::chrono::duration<double, std::micro>(t2 - t1).count() / 100;
	WARN("library size " << library.size() << ", build " << build_ms << " ms, fit " << fit_us << " us per call");

	REQUIRE(fit.has_value());
	const JunctionGenerateResult gen = GenerateJunction(spec, &flat);
	REQUIRE(gen.layout.has_value());
	const JunctionLibraryEntry &entry = library[fit->entry];
	const std::pair<int, Money> gen_score = ScoreJunctionLayout(*gen.layout, spec);
	WARN("fit tiles " << entry.score.first << " cost " << (entry.score.second + fit->terrain_cost) << ", gen tiles " << gen_score.first << " cost " << gen_score.second);
	CHECK(std::make_pair(entry.score.first, entry.score.second + fit->terrain_cost) <= gen_score);
}

TEST_CASE("BuildJunctionLibrary is deterministic", "[junction][library]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const std::vector<JunctionLibraryEntry> a = BuildJunctionLibrary(spec);
	const std::vector<JunctionLibraryEntry> b = BuildJunctionLibrary(spec);
	REQUIRE(a.size() == b.size());
	for (size_t i = 0; i < a.size(); i++) {
		CHECK(JunctionLayoutToJson(a[i].layout) == JunctionLayoutToJson(b[i].layout));
		CHECK(a[i].hub.x == b[i].hub.x);
		CHECK(a[i].hub.y == b[i].hub.y);
		CHECK(a[i].score == b[i].score);
		CHECK(a[i].area == b[i].area);
	}
}

/** Whether any track tile or bridge head of the entry lies in the working rectangle. */
static bool EntryTouchesRect(const JunctionLibraryEntry &e, int x0, int x1, int y0, int y1)
{
	auto in = [&](const TileIndexDiffC &t) {
		const int x = t.x - e.hub.x + JunctionFrame{}.hub_x;
		const int y = t.y - e.hub.y;
		return x >= x0 && x <= x1 && y >= y0 && y <= y1;
	};
	for (const JunctionTrack &jt : e.layout.tracks) if (!jt.tracks.None() && in(jt.offset)) return true;
	for (const JunctionBridge &b : e.layout.bridges) if (in(b.start) || in(b.end)) return true;
	return false;
}

TEST_CASE("FitJunctionLibrary avoids blocked tiles", "[junction][terrain][library]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const std::vector<JunctionLibraryEntry> library = BuildJunctionLibrary(spec);
	REQUIRE_FALSE(library.empty());

	JunctionTerrain blocked = MakeFlatTerrain();
	SetTerrainRect(blocked, 18, 27, 4, 10, JunctionTileClass::Blocked);
	const std::optional<JunctionLibraryFit> fit = FitJunctionLibrary(library, blocked);
	if (!fit.has_value()) {
		WARN("house block: no fit");
	} else {
		CHECK_FALSE(EntryTouchesRect(library[fit->entry], 18, 27, 4, 10));
	}

	JunctionTerrain all = MakeFlatTerrain();
	SetTerrainRect(all, 0, JunctionFrame{}.width - 1, JunctionFrame{}.terrain_origin_y, JunctionFrame{}.terrain_origin_y + JunctionFrame{}.terrain_height - 1, JunctionTileClass::Blocked);
	CHECK_FALSE(FitJunctionLibrary(library, all).has_value());
}

TEST_CASE("FitJunctionLibrary picks a plane", "[junction][terrain][library]")
{
	const std::vector<JunctionLibraryEntry> library = BuildJunctionLibrary(MakeTestJunctionSpec());
	REQUIRE_FALSE(library.empty());
	const JunctionTerrain raised = MakeFlatTerrain(2);
	const std::optional<JunctionLibraryFit> fit = FitJunctionLibrary(library, raised);
	REQUIRE(fit.has_value());
	CHECK(fit->plane == 2);
}

TEST_CASE("JunctionTerrainPlanes", "[junction][terrain][library]")
{
	const JunctionTerrain t = MakeFlatTerrain(1);
	CHECK(JunctionTerrainPlanes(t) == std::vector<int>{1});
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
	/* D100: at the crossing the W track is split into lanes on rows w_y - 1 and w_y + 1. */
	const auto lane_ok = [&](int y) { return LayoutTracksAt(layout, result.hub.x, y).Test(Track::X) || UnderBridge(layout, result.hub.x, y); };
	/* D105: or the parallelogram's lanes, on the W row and the row below it. */
	CHECK((LayoutTracksAt(layout, result.hub.x, w_y).Test(Track::X) || (lane_ok(w_y - 1) && lane_ok(w_y + 1)) || (lane_ok(w_y) && lane_ok(w_y + 1))));
}

TEST_CASE("GenerateJunction with a blocked prio row line", "[junction][terrain]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	JunctionTerrain t = MakeFlatTerrain();
	SetTerrainRect(t, 0, JunctionFrame{}.width - 1, -1, -1, JunctionTileClass::Blocked);
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

TEST_CASE("GenerateJunction sets the score of the chosen layout", "[junction]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionGenerateResult result = GenerateJunction(spec);
	REQUIRE(result.layout.has_value());
	CHECK(result.score == ScoreJunctionLayout(*result.layout, spec));
}

TEST_CASE("a bridge may span existing track", "[junction][terrain]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionTerrain flat = MakeFlatTerrain();
	const JunctionGenerateResult base = GenerateJunction(spec, &flat);
	REQUIRE(base.layout.has_value());

	/* The doubled bridges run along x on layout rows hub.y + 1 and hub.y + 3, heads at hub.x - 4 and hub.x + 3; the strip is one column under the middle of the span, over both lane rows and the W row between. */
	const int sx = 29;
	const int sy0 = base.hub.y + 1 - base.hub.y;
	const int sy1 = base.hub.y + 3 - base.hub.y;

	JunctionTerrain blocked = flat;
	SetTerrainRect(blocked, sx, sx, sy0, sy1, JunctionTileClass::Blocked);
	const JunctionGenerateResult red = GenerateJunction(spec, &blocked);
	CHECK((!red.layout.has_value() || !SameLayout(*red.layout, *base.layout)));

	JunctionTerrain spannable = flat;
	SetTerrainRect(spannable, sx, sx, sy0, sy1, JunctionTileClass::Spannable);
	const JunctionGenerateResult result = GenerateJunction(spec, &spannable);
	REQUIRE(result.layout.has_value());
}

TEST_CASE("track may not use a Spannable tile", "[junction][terrain]")
{
	JunctionTerrain t = MakeFlatTerrain();
	SetTerrainRect(t, 20, 20, 5, 5, JunctionTileClass::Spannable);
	CHECK_FALSE(t.UsableAt(20, 5, 0));
	CHECK(t.SpanOk(20, 5, 0));
	CHECK(t.SpanOk(20, 5, 1));
}
