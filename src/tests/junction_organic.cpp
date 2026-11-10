/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_organic.cpp Test functionality from junction_organic. */

#include "../stdafx.h"

#include "../3rdparty/catch2/catch.hpp"

#include "../junction_compact.h"
#include "../junction_generator.h"
#include "../junction_organic.h"
#include "../junction_route.h"
#include "../junction_rules.h"
#include "../junction_rules_internal.h"
#include "../track_func.h"

#include "../3rdparty/fmt/format.h"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

#include "../safeguards.h"

/** The test spec at its old coverage of one train length and without the ML delay check (a copy of PinnedTestSpec in the generator tests). */
static JunctionSpec PinnedOrganicSpec()
{
	JunctionSpec spec = MakeTestJunctionSpec();
	spec.prio_tiles = spec.tl;
	spec.prio_strict = false;
	return spec;
}

/** The spec of the generator test "a long train with a narrow branch still fits". */
static JunctionSpec NarrowBranchSpec()
{
	JunctionSpec spec = PinnedOrganicSpec();
	spec.branch_gap = 0;
	spec.tl = 6;
	return spec;
}

/** The main-line turn cost the organic tests use (D165): a turn weighed against one train length of rail. */
static Money MlTurnCostOf(const JunctionSpec &spec)
{
	return Money{spec.prices.rail_piece.base() * spec.tl};
}

static constexpr int ORGANIC_W = 44;
static constexpr int ORGANIC_H = 16;
static constexpr int ORGANIC_P = 20;

/** Negotiate the test spec's T on the standard box with the branch at x = p, without movement rules, masks or windows (the O4b1 negotiation). */
static JunctionNegotiation NegotiateT(const JunctionSpec &spec, int p, int max_rounds = 16)
{
	JunctionNegotiationPrices prices;
	prices.apply_rules = false;
	return NegotiateJunctionMovements(spec, MakeOrganicTPorts(spec, ORGANIC_W, ORGANIC_H, p), ORGANIC_W, ORGANIC_H, nullptr, max_rounds, prices);
}

/** The pieces of a route, from the start. */
static std::vector<JunctionPiece> RoutePieces(const JunctionLineRoute &route)
{
	const std::vector<JunctionRaster> rasters = RasterizeJunctionLine({1, 1, 1}, route);
	REQUIRE(rasters.size() == 1);
	REQUIRE(rasters[0].error.empty());
	return rasters[0].pieces;
}

TEST_CASE("NegotiateJunctionMovements: a T on flat ground converges", "[junction][organic]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionNegotiation negotiation = NegotiateT(spec, ORGANIC_P);
	INFO("error: " << negotiation.error);
	INFO("rounds: " << negotiation.rounds);
	std::string rounds_text;
	for (int c : negotiation.conflicts_per_round) rounds_text += fmt::format(" {}", c);
	INFO("conflicts after each round:" << rounds_text << ", routes run " << negotiation.routes_run);
	std::string moves_text;
	for (const JunctionLineRoute &route : negotiation.routes) moves_text += fmt::format("\n  start {},{} {}: {}", route.start.x, route.start.y, static_cast<int>(route.start_trackdir), route.moves);
	INFO("moves:" << moves_text);
	REQUIRE(negotiation.layout.has_value());
	CHECK(negotiation.conflicts == 0);

	const JunctionLayout &layout = *negotiation.layout;
	const JunctionCompactness compact = MeasureJunctionCompactness(layout);
	INFO("occupied " << compact.occupied << " slivers " << compact.slivers);
	INFO("\n" << RenderJunctionAscii(layout));

	const JunctionCheckContext ctx = MakeJunctionCheckContext(layout, spec, FindJunctionPorts(layout), JunctionTMovements());
	std::vector<RuleViolation> violations = CheckConnectivity(ctx);
	for (const auto &group : {CheckGeometry(ctx), CheckBridges(ctx)}) violations.insert(violations.end(), group.begin(), group.end());
	/* The doubled main-line bridge is the D80 move, which O4b2 adds. */
	const size_t before = violations.size();
	std::erase_if(violations, [](const RuleViolation &violation) { return violation.message == "ML bridge is not doubled"; });
	INFO("ignored 'ML bridge is not doubled': " << before - violations.size());
	std::string text;
	for (const RuleViolation &violation : violations) text += fmt::format("violation (rule {}) at {},{}: {}\n", static_cast<int>(violation.rule), violation.tile.x, violation.tile.y, violation.message);
	INFO(text);
	CHECK(violations.empty());
}

TEST_CASE("NegotiateJunctionMovements: movements from one port share their first pieces", "[junction][organic]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionNegotiation negotiation = NegotiateT(spec, ORGANIC_P);
	REQUIRE(negotiation.layout.has_value());
	REQUIRE(negotiation.routes.size() == 6);

	std::vector<std::vector<JunctionPiece>> pieces;
	for (const JunctionLineRoute &route : negotiation.routes) pieces.push_back(RoutePieces(route));

	auto same = [](const JunctionPiece &a, const JunctionPiece &b) { return a.tile.x == b.tile.x && a.tile.y == b.tile.y && a.trackdir == b.trackdir; };
	/* In JunctionTMovements() order: (EIn, EOut), (EIn, SOut), (WIn, WOut), (WIn, SOut), (NIn, WOut), (NIn, EOut). */
	CHECK(same(pieces[0].front(), pieces[1].front()));
	CHECK(same(pieces[2].front(), pieces[3].front()));
	CHECK(same(pieces[4].front(), pieces[5].front()));
	/* Arriving: SOut by movements 1 and 3, WOut by 2 and 4, EOut by 0 and 5. */
	CHECK(same(pieces[1].back(), pieces[3].back()));
	CHECK(same(pieces[2].back(), pieces[4].back()));
	CHECK(same(pieces[0].back(), pieces[5].back()));
}

TEST_CASE("NegotiateJunctionMovements: crossings go over or under", "[junction][organic]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionNegotiation negotiation = NegotiateT(spec, ORGANIC_P);
	REQUIRE(negotiation.layout.has_value());
	const JunctionLayout &layout = *negotiation.layout;

	/* Recheck from the routes: two pieces of different movements on one tile at grade may hold different overlapping bits only as a switch. */
	const auto movements = JunctionTMovements();
	REQUIRE(negotiation.routes.size() == movements.size());
	struct Piece { size_t movement; JunctionPiece piece; };
	std::map<std::pair<int, int>, std::vector<Piece>> by_tile;
	for (size_t m = 0; m < negotiation.routes.size(); m++) {
		for (const JunctionPiece &piece : RoutePieces(negotiation.routes[m])) {
			if (!piece.on_bridge) by_tile[{piece.tile.x, piece.tile.y}].push_back({m, piece});
		}
	}
	int overlaps = 0;
	for (const auto &[tile, list] : by_tile) {
		for (size_t i = 0; i < list.size(); i++) {
			for (size_t j = i + 1; j < list.size(); j++) {
				if (list[i].movement == list[j].movement) continue;
				const Track bit_a = TrackdirToTrack(list[i].piece.trackdir);
				const Track bit_b = TrackdirToTrack(list[j].piece.trackdir);
				if (bit_a == bit_b) {
					if (list[i].piece.trackdir != list[j].piece.trackdir) overlaps++;
					continue;
				}
				if (!TracksOverlap(TrackBits{bit_a} | bit_b)) continue;
				const bool split = movements[list[i].movement].first == movements[list[j].movement].first && TrackdirToExitdir(ReverseTrackdir(list[i].piece.trackdir)) == TrackdirToExitdir(ReverseTrackdir(list[j].piece.trackdir));
				const bool merge = movements[list[i].movement].second == movements[list[j].movement].second && TrackdirToExitdir(list[i].piece.trackdir) == TrackdirToExitdir(list[j].piece.trackdir);
				if (split || merge) continue;
				INFO("overlap at " << tile.first << "," << tile.second);
				overlaps++;
			}
		}
	}
	CHECK(overlaps == 0);
	CHECK(!layout.bridges.empty());
}

TEST_CASE("MakeOrganicTPorts: the old layout's arrangement", "[junction][organic]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionGenerateResult generated = GenerateJunction(spec);
	REQUIRE(generated.layout.has_value());
	const std::vector<JunctionPort> old_ports = FindJunctionPorts(*generated.layout);
	const std::vector<JunctionPort> ports = MakeOrganicTPorts(spec, ORGANIC_W, ORGANIC_H, ORGANIC_P);
	REQUIRE(old_ports.size() == 6);
	REQUIRE(ports.size() == 6);

	auto find = [](const std::vector<JunctionPort> &list, JunctionPortRole role) {
		auto it = std::ranges::find(list, role, &JunctionPort::role);
		REQUIRE(it != list.end());
		return *it;
	};
	/* Which edge a port sits on: 0 = x 0, 1 = x max, 2 = y max. */
	auto edge = [](const JunctionPort &port, int w, int h) { return port.tile.x == 0 ? 0 : port.tile.x == w - 1 ? 1 : port.tile.y == h - 1 ? 2 : -1; };

	using Role = JunctionPortRole;
	for (Role role : {Role::EIn, Role::EOut, Role::WIn, Role::WOut, Role::NIn, Role::SOut}) {
		const JunctionPort a = find(old_ports, role);
		const JunctionPort b = find(ports, role);
		CHECK(a.trackdir == b.trackdir);
		CHECK(edge(a, generated.layout->size.x, generated.layout->size.y) == edge(b, ORGANIC_W, ORGANIC_H));
	}
	auto dy = [&](const std::vector<JunctionPort> &list, Role a, Role b) { return find(list, a).tile.y - find(list, b).tile.y; };
	auto dx = [&](const std::vector<JunctionPort> &list, Role a, Role b) { return find(list, a).tile.x - find(list, b).tile.x; };
	CHECK(dy(old_ports, Role::WOut, Role::EIn) == dy(ports, Role::WOut, Role::EIn));
	CHECK(dy(old_ports, Role::EOut, Role::EIn) == dy(ports, Role::EOut, Role::EIn));
	CHECK(dy(old_ports, Role::WIn, Role::EIn) == dy(ports, Role::WIn, Role::EIn));
	CHECK(dx(old_ports, Role::SOut, Role::NIn) == dx(ports, Role::SOut, Role::NIn));
	CHECK(dy(ports, Role::WOut, Role::EIn) == spec.ml_gap + 1);
	CHECK(dx(ports, Role::SOut, Role::NIn) == spec.branch_gap + 1);
}

/** Time the negotiation of the test spec's T with the branch at x = p and print one BENCH line. */
static void BenchOrganic(const char *name, int p)
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	std::vector<long long> times;
	JunctionNegotiation negotiation = NegotiateT(spec, p);
	for (int i = 0; i < 11; i++) {
		const auto begin = std::chrono::steady_clock::now();
		negotiation = NegotiateT(spec, p);
		const auto end = std::chrono::steady_clock::now();
		times.push_back(std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count());
	}
	std::sort(times.begin(), times.end());
	JunctionCompactness compact;
	if (negotiation.layout.has_value()) compact = MeasureJunctionCompactness(*negotiation.layout);
	std::cout << "BENCH " << name << " rounds=" << negotiation.rounds << " converged=" << (negotiation.layout.has_value() ? 1 : 0) << " occupied=" << compact.occupied << " slivers=" << compact.slivers << " median_us=" << times[times.size() / 2] << " max_us=" << times.back() << "\n";
}

TEST_CASE("NegotiateJunctionMovements: timing", "[junction][organic][.bench]")
{
	BenchOrganic("organic_t", ORGANIC_P);
	BenchOrganic("organic_t_p14", 14);
	BenchOrganic("organic_t_p26", 26);
}

/** The violations of a layout as text, one per line. */
static std::string ViolationText(const std::vector<RuleViolation> &violations)
{
	std::string text;
	for (const RuleViolation &violation : violations) text += fmt::format("violation (rule {}) at {},{}: {}\n", static_cast<int>(violation.rule), violation.tile.x, violation.tile.y, violation.message);
	return text;
}

/** The integer a text starts with. */
static int LeadingInt(const std::string &text)
{
	int value = 0;
	std::from_chars(text.data(), text.data() + text.size(), value);
	return value;
}

/** The number of the winning crossing case in a result's case lines (the line marked '*'), or -1. */
static int WinningCase(const std::string &cases)
{
	const size_t star = cases.find("*case ");
	return star == std::string::npos ? -1 : LeadingInt(cases.substr(star + 6));
}

/** Generate a spec's organic T and check it is rule-clean, reporting the numbers beside the old generator's. */
static void CheckOrganicClean(const JunctionSpec &spec)
{
	const JunctionOrganicResult result = GenerateJunctionOrganic(spec, MlTurnCostOf(spec));
	INFO("error: " << result.error);
	INFO("last violations:\n" << ViolationText(result.violations));
	REQUIRE(result.layout.has_value());
	const JunctionCompactness compact = MeasureJunctionCompactness(*result.layout);
	INFO("p " << result.p << " rounds " << result.rounds << " micros " << result.micros << " occupied " << compact.occupied << " slivers " << compact.slivers << " cost " << result.cost.base());
	INFO("cases at the winning p:\n" << result.cases);
	INFO("bridges " << result.layout->bridges.size());
	INFO("\n" << RenderJunctionAscii(*result.layout));
	const JunctionGenerateResult old = GenerateJunction(spec);
	if (old.layout.has_value()) {
		const JunctionCompactness old_compact = MeasureJunctionCompactness(*old.layout);
		INFO("old generator: occupied " << old_compact.occupied << " slivers " << old_compact.slivers);
	}
	std::vector<RuleViolation> violations = CheckJunctionRules(*result.layout, spec);
	const size_t before = violations.size();
	/* The doubled main-line bridge is the D80 move, a later step. */
	std::erase_if(violations, [](const RuleViolation &violation) { return violation.message == "ML bridge is not doubled"; });
	INFO("ignored 'ML bridge is not doubled': " << before - violations.size());
	INFO(ViolationText(violations));
	CHECK(violations.empty());
	CHECK(JunctionRulesClean(*result.layout, spec));
	CHECK(result.compactness.occupied == compact.occupied);
	CHECK(result.compactness.slivers == compact.slivers);
	CHECK(WinningCase(result.cases) >= 0);
}

TEST_CASE("GenerateJunctionOrganic: the map-driven organic T is rule-clean", "[junction][organic]")
{
	CheckOrganicClean(MakeTestJunctionSpec());
}

TEST_CASE("GenerateJunctionOrganic: the map-driven organic T of the pinned spec is rule-clean", "[junction][organic]")
{
	CheckOrganicClean(PinnedOrganicSpec());
}

TEST_CASE("GenerateJunctionOrganic: the map-driven organic T with a narrow branch is rule-clean", "[junction][organic]")
{
	CheckOrganicClean(NarrowBranchSpec());
}

TEST_CASE("BuildOrganicT: the prio row travels with the main line", "[junction][organic]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionOrganicResult generated = GenerateJunctionOrganic(spec, MlTurnCostOf(spec));
	REQUIRE(generated.layout.has_value());
	const int p = generated.p;
	const JunctionOrganicResult result = BuildOrganicT(spec, ORGANIC_W, ORGANIC_H, p, MlTurnCostOf(spec));
	INFO("p " << p << " cases:\n" << result.cases);
	REQUIRE(result.layout.has_value());

	/* The checker's prio component is empty for the finished layout. */
	const JunctionCheckContext ctx = MakeJunctionCheckContext(*result.layout, spec, FindJunctionPorts(*result.layout), JunctionTMovements());
	const std::vector<RuleViolation> prio = CheckPrio(ctx);
	INFO(ViolationText(prio));
	CHECK(prio.empty());

	/* The winning candidate is the map of the case marked '*': project it again; each main line's companion tiles carry track that no movement's route uses. */
	const int winner = WinningCase(result.cases);
	REQUIRE(winner >= 0);
	const int box_width = ORGANIC_W + 4 * winner;
	const int box_height = ORGANIC_H + 1 + 2 * winner;
	auto maps_of = [&](int w, int h) { return JunctionTMaps(spec, MakeOrganicTPorts(spec, w, h, p, 1), w, h); };
	auto chunk_of = [](const std::vector<JunctionTMap> &list, const JunctionTMap::Window &window) {
		int least = window.end_lo;
		for (const JunctionTMap &map : list) {
			for (const JunctionTMap::Window &other : map.windows) {
				if (other.joiner == window.joiner) least = std::min(least, other.end_lo);
			}
		}
		return (window.end_lo - least) / 6;
	};
	const std::vector<JunctionTMap> first_maps = maps_of(ORGANIC_W, ORGANIC_H + 1);
	const std::vector<JunctionTMap> maps = maps_of(box_width, box_height);
	const JunctionTMap *winning = nullptr;
	for (const JunctionTMap &map : maps) {
		bool same = map.over == first_maps[0].over;
		for (size_t k = 0; same && k < map.windows.size(); k++) same = chunk_of(maps, map.windows[k]) == chunk_of(first_maps, first_maps[0].windows[k]);
		if (same) {
			winning = &map;
			break;
		}
	}
	REQUIRE(winning != nullptr);
	const std::vector<JunctionPort> ports = MakeOrganicTPorts(spec, box_width, box_height, p, 1);
	JunctionNegotiationPrices prices;
	prices.ml_turn_cost = MlTurnCostOf(spec);
	prices.map = winning;
	const JunctionNegotiation negotiation = NegotiateJunctionMovements(spec, ports, box_width, box_height, nullptr, 48, prices);
	INFO("negotiation error: " << negotiation.error);
	REQUIRE(negotiation.layout.has_value());
	REQUIRE(negotiation.routes.size() == 6);
	REQUIRE(winning->windows.size() == 2);
	for (const JunctionTMap::Window &window : winning->windows) {
		const std::vector<JunctionPiece> &companion = negotiation.routes[window.line].companion;
		INFO("window of movement " << window.line);
		REQUIRE_FALSE(companion.empty());
		for (const JunctionPiece &piece : companion) {
			CHECK(std::ranges::any_of(negotiation.layout->tracks, [&](const JunctionTrack &jt) { return jt.offset.x == piece.tile.x && jt.offset.y == piece.tile.y && jt.tracks.Test(TrackdirToTrack(piece.trackdir)); }));
			for (const JunctionLineRoute &route : negotiation.routes) {
				for (const JunctionPiece &used : RoutePieces(route)) CHECK_FALSE((used.tile.x == piece.tile.x && used.tile.y == piece.tile.y));
			}
		}
	}
}

/** The violations of the checker's signal and waiting-room groups for a built T, from its layout when clean, else from the last case's violations. */
static std::vector<RuleViolation> SignalAndWaitingViolations(const JunctionSpec &spec, const JunctionOrganicResult &result)
{
	std::vector<RuleViolation> violations;
	if (result.layout.has_value()) {
		const JunctionCheckContext ctx = MakeJunctionCheckContext(*result.layout, spec, FindJunctionPorts(*result.layout), JunctionTMovements());
		for (const auto &group : {CheckSignals(ctx), CheckWaitingRoom(ctx)}) violations.insert(violations.end(), group.begin(), group.end());
	} else {
		violations = result.violations;
	}
	return violations;
}

TEST_CASE("BuildOrganicT: router waiting room agrees with the checker", "[junction][organic]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionOrganicResult result = BuildOrganicT(spec, ORGANIC_W, ORGANIC_H, 16, MlTurnCostOf(spec));
	INFO("cases:\n" << result.cases);
	REQUIRE(result.layout.has_value());
	const JunctionCheckContext ctx = MakeJunctionCheckContext(*result.layout, spec, FindJunctionPorts(*result.layout), JunctionTMovements());
	const std::vector<RuleViolation> waiting = CheckWaitingRoom(ctx);
	INFO(ViolationText(waiting));
	CHECK(waiting.empty());
}

TEST_CASE("BuildOrganicT: the waiting room needs exactly the checker's minimum", "[junction][organic]")
{
	/* The checker's minimum is 2 x tl units; one train length less, the same and more all pass without padding. */
	for (const uint tl : {3u, 4u, 5u}) {
		JunctionSpec spec = MakeTestJunctionSpec();
		spec.tl = tl;
		const JunctionOrganicResult result = BuildOrganicT(spec, ORGANIC_W, ORGANIC_H, 16, MlTurnCostOf(spec));
		INFO("tl " << tl << " cases:\n" << result.cases);
		REQUIRE(result.layout.has_value());
		const JunctionCheckContext ctx = MakeJunctionCheckContext(*result.layout, spec, FindJunctionPorts(*result.layout), JunctionTMovements());
		const std::vector<RuleViolation> waiting = CheckWaitingRoom(ctx);
		INFO(ViolationText(waiting));
		CHECK(waiting.empty());
	}
}

TEST_CASE("BuildOrganicT: bridges keep a signal-legal piece from every split and merge", "[junction][organic]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	for (const int p : {20, 22, 24}) {
		const JunctionOrganicResult result = BuildOrganicT(spec, ORGANIC_W, ORGANIC_H, p, MlTurnCostOf(spec));
		INFO("p " << p << " cases:\n" << result.cases);
		const std::vector<RuleViolation> violations = SignalAndWaitingViolations(spec, result);
		INFO(ViolationText(violations));
		for (const RuleViolation &violation : violations) {
			CHECK(violation.message.find("split in a bridge's block") == std::string::npos);
			CHECK(violation.message.find("signals too far apart") == std::string::npos);
		}
		CHECK(result.layout.has_value());
	}
}

TEST_CASE("JunctionTMaps: maps are ordered by bound", "[junction][organic]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const int box_height = ORGANIC_H + 1;
	const std::vector<JunctionPort> ports = MakeOrganicTPorts(spec, ORGANIC_W, box_height, 20, 1);
	const std::vector<JunctionTMap> maps = JunctionTMaps(spec, ports, ORGANIC_W, box_height);
	REQUIRE(maps.size() >= 8);
	for (size_t i = 1; i < maps.size(); i++) CHECK(maps[i - 1].bound <= maps[i].bound);

	const auto movements = JunctionTMovements();
	const std::vector<std::pair<int, int>> pairs = JunctionForcedPairs(ports, ORGANIC_W, box_height);
	REQUIRE(maps[0].over.size() == pairs.size());
	/* Where a main line is in a forced pair, it goes over (the pair of the two turning movements has none). */
	auto is_main_line = [&](int movement) { return (movements[movement].first == JunctionPortRole::EIn && movements[movement].second == JunctionPortRole::EOut) || (movements[movement].first == JunctionPortRole::WIn && movements[movement].second == JunctionPortRole::WOut); };
	for (const auto &[over, under] : maps[0].over) {
		if (is_main_line(over) || is_main_line(under)) CHECK(is_main_line(over));
	}
	CHECK(maps[0].windows.size() == 2);
}

TEST_CASE("BuildOrganicT: every bridge crosses something", "[junction][organic]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const int p = GenerateJunctionOrganic(spec, MlTurnCostOf(spec)).p;
	const JunctionOrganicResult result = BuildOrganicT(spec, ORGANIC_W, ORGANIC_H, p, MlTurnCostOf(spec));
	INFO("p " << p);
	INFO("error: " << result.error);
	INFO("cases:\n" << result.cases);
	REQUIRE(result.layout.has_value());
	INFO("\n" << RenderJunctionAscii(*result.layout));
	for (const JunctionBridge &bridge : result.layout->bridges) {
		int crossed = 0;
		for (int x = std::min(bridge.start.x, bridge.end.x); x <= std::max(bridge.start.x, bridge.end.x); x++) {
			for (int y = std::min(bridge.start.y, bridge.end.y); y <= std::max(bridge.start.y, bridge.end.y); y++) {
				const bool head = (x == bridge.start.x && y == bridge.start.y) || (x == bridge.end.x && y == bridge.end.y);
				if (head) continue;
				if (std::ranges::any_of(result.layout->tracks, [&](const JunctionTrack &jt) { return jt.offset.x == x && jt.offset.y == y; })) crossed++;
			}
		}
		INFO("bridge " << bridge.start.x << "," << bridge.start.y << " to " << bridge.end.x << "," << bridge.end.y << " crosses " << crossed << " tiles of track");
		CHECK(crossed > 0);
	}
}

TEST_CASE("JunctionForcedPairs: the forced pairs of a T", "[junction][organic]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const std::vector<std::pair<int, int>> pairs = JunctionForcedPairs(MakeOrganicTPorts(spec, ORGANIC_W, ORGANIC_H, ORGANIC_P), ORGANIC_W, ORGANIC_H);
	/* In JunctionTMovements() order: 1 = E->S, 2 = W main line, 5 = N->E. */
	const std::vector<std::pair<int, int>> expected = {{1, 2}, {1, 5}, {2, 5}};
	CHECK(pairs == expected);
}

/** Print one BENCH line and the winning cases block for a spec's organic T at each branch position p tried. */
static void BenchSpec(const char *name, const JunctionSpec &spec)
{
	for (int p = 14; p <= 28; p += 2) {
		const JunctionOrganicResult result = BuildOrganicT(spec, ORGANIC_W, ORGANIC_H, p, MlTurnCostOf(spec));
		const size_t candidates = static_cast<size_t>(std::ranges::count(result.cases, '\n')) - (result.cases.find("stopped after") != std::string::npos ? 1 : 0);
		std::cout << "BENCH spec=" << name << " p=" << p << " candidates=" << candidates << " rounds=" << result.rounds << " micros=" << result.micros << " max_expansions_seen=" << result.max_expansions << " occupied=" << result.compactness.occupied << " slivers=" << result.compactness.slivers << " clean=" << (result.layout.has_value() ? 1 : 0) << "\n";
		std::cout << result.cases;
	}
}

TEST_CASE("GenerateJunctionOrganic: timing", "[junction][organic][.bench]")
{
	BenchSpec("test", MakeTestJunctionSpec());
	BenchSpec("pinned", PinnedOrganicSpec());
	BenchSpec("narrow", NarrowBranchSpec());
}
