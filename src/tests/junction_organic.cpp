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

/** Negotiate the test spec's T on the standard box with the branch at x = p. */
static JunctionNegotiation NegotiateT(const JunctionSpec &spec, int p, int max_rounds = 16)
{
	return NegotiateJunctionMovements(spec, MakeOrganicTPorts(spec, ORGANIC_W, ORGANIC_H, p), ORGANIC_W, ORGANIC_H, nullptr, max_rounds);
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
	CHECK(result.compactness.occupied == compact.occupied);
	CHECK(result.compactness.slivers == compact.slivers);
	CHECK(WinningCase(result.cases) >= 0);
}

TEST_CASE("GenerateJunctionOrganic: the test spec's T is rule-clean", "[junction][organic]")
{
	CheckOrganicClean(MakeTestJunctionSpec());
}

TEST_CASE("GenerateJunctionOrganic: the pinned spec's T is rule-clean", "[junction][organic]")
{
	CheckOrganicClean(PinnedOrganicSpec());
}

TEST_CASE("GenerateJunctionOrganic: a long train with a narrow branch is rule-clean", "[junction][organic]")
{
	CheckOrganicClean(NarrowBranchSpec());
}

TEST_CASE("BuildOrganicT: the first negotiation obeys the rules", "[junction][organic]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionOrganicResult result = BuildOrganicT(spec, ORGANIC_W, ORGANIC_H, 23, MlTurnCostOf(spec));
	INFO("error: " << result.error);
	INFO("violations left:\n" << ViolationText(result.violations));
	INFO("cases:\n" << result.cases);
	REQUIRE(result.layout.has_value());
	CHECK(result.violations.empty());

	/* Negotiate the winning case again (its forced pairs are in its line: [a>b c>d ...]) and count each turning movement's private units from its routes. */
	const size_t open = result.cases.find('[', result.cases.find("*case "));
	REQUIRE(open != std::string::npos);
	std::istringstream label(result.cases.substr(open + 1, result.cases.find(']', open) - open - 1));
	JunctionNegotiationPrices prices;
	prices.ml_turn_cost = MlTurnCostOf(spec);
	for (std::string pair; label >> pair;) prices.over.emplace_back(LeadingInt(pair), LeadingInt(pair.substr(pair.find('>') + 1)));
	const JunctionNegotiation negotiation = NegotiateJunctionMovements(spec, MakeOrganicTPorts(spec, ORGANIC_W, ORGANIC_H, 23), ORGANIC_W, ORGANIC_H, nullptr, 48, prices);
	INFO("negotiation error: " << negotiation.error);
	REQUIRE(negotiation.layout.has_value());
	REQUIRE(negotiation.routes.size() == 6);
	CHECK(negotiation.rounds == result.rounds);

	const auto movements = JunctionTMovements();
	std::vector<std::vector<JunctionPiece>> pieces;
	for (const JunctionLineRoute &route : negotiation.routes) pieces.push_back(RoutePieces(route));
	auto same = [](const JunctionPiece &a, const JunctionPiece &b) { return a.tile.x == b.tile.x && a.tile.y == b.tile.y && a.trackdir == b.trackdir; };
	for (const size_t m : {1, 3, 4, 5}) {
		/* The split: the first piece that parts from the sibling leaving the same port. */
		size_t sibling = movements.size();
		for (size_t o = 0; o < movements.size(); o++) {
			if (o != m && movements[o].first == movements[m].first) sibling = o;
		}
		REQUIRE(sibling < movements.size());
		size_t split = 0;
		while (split < pieces[m].size() && split < pieces[sibling].size() && same(pieces[m][split], pieces[sibling][split])) split++;
		/* The merge: the first piece after it on the route of a movement arriving at the same port. */
		size_t merge = split;
		auto on_arriving = [&](const JunctionPiece &piece) {
			for (size_t o = 0; o < movements.size(); o++) {
				if (o != m && movements[o].second == movements[m].second && std::ranges::any_of(pieces[o], [&](const JunctionPiece &other) { return same(piece, other); })) return true;
			}
			return false;
		};
		while (merge < pieces[m].size() && !on_arriving(pieces[m][merge])) merge++;
		REQUIRE(merge < pieces[m].size());
		unsigned units = 0;
		for (size_t i = split; i < merge; i++) units += pieces[m][i].units;
		INFO("movement " << m << " has " << units << " private units between its split (piece " << split << ") and its merge (piece " << merge << ")");
		CHECK(units >= 2 * spec.tl);
	}
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

/** Time the generation of a spec's organic T (1 warm-up and 5 runs) and print one BENCH line. */
static void BenchGenerate(const char *name, const JunctionSpec &spec)
{
	std::vector<int64_t> times;
	JunctionOrganicResult result = GenerateJunctionOrganic(spec, MlTurnCostOf(spec));
	for (int i = 0; i < 5; i++) {
		result = GenerateJunctionOrganic(spec, MlTurnCostOf(spec));
		times.push_back(result.micros);
	}
	std::sort(times.begin(), times.end());
	std::cout << "BENCH organic_gen " << name << " clean=" << (result.layout.has_value() ? 1 : 0) << " p=" << result.p << " case=" << WinningCase(result.cases) << " rounds=" << result.rounds << " occupied=" << result.compactness.occupied << " slivers=" << result.compactness.slivers << " median_us=" << times[times.size() / 2] << "\n";
}

TEST_CASE("GenerateJunctionOrganic: timing", "[junction][organic][.bench]")
{
	BenchGenerate("test", MakeTestJunctionSpec());
	BenchGenerate("pinned", PinnedOrganicSpec());
	BenchGenerate("narrow", NarrowBranchSpec());

	const JunctionSpec spec = MakeTestJunctionSpec();
	for (int p = 14; p <= 28; p += 2) {
		const JunctionOrganicResult result = BuildOrganicT(spec, ORGANIC_W, ORGANIC_H, p, MlTurnCostOf(spec));
		std::cout << "BENCH organic_p " << p << " clean=" << (result.layout.has_value() ? 1 : 0) << " rounds=" << result.rounds << " us=" << result.micros << "\n";
	}
}
