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
#include <chrono>
#include <iostream>
#include <map>
#include <string>

#include "../safeguards.h"

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
