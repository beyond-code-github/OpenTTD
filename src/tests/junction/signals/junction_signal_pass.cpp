/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_signal_pass.cpp Test functionality from junction_signal_pass. */

#include "../../../stdafx.h"

#include "../../../3rdparty/catch2/catch.hpp"

#include "../../../junction/legacy/junction_generator.h"
#include "../../../junction/junction_route.h"
#include "../../../junction/junction_rules.h"
#include "../../../junction/junction_rules_internal.h"
#include "../../../core/format.hpp"
#include "../../../junction/signals/junction_signal_pass.h"
#include "../../../track_func.h"

#include <algorithm>
#include <span>
#include <string>

#include "../../../safeguards.h"

using Role = JunctionPortRole;

static JunctionRoute R(int x, int y, Trackdir td, std::string moves)
{
	return {{static_cast<int16_t>(x), static_cast<int16_t>(y)}, td, std::move(moves)};
}

/** Rasterize routes into a normalised layout without signals. */
static JunctionLayout BuildRoutes(const std::vector<JunctionRoute> &routes)
{
	JunctionLayout layout;
	for (const JunctionRoute &route : routes) {
		const JunctionRaster raster = RasterizeJunctionRoute(route);
		REQUIRE(raster.error.empty());
		AddRasterToLayout(layout, raster);
	}
	NormaliseJunctionLayout(layout);
	return layout;
}

static std::string Repeat(char c, int n)
{
	return std::string(n, c);
}

/**
 * A frame of the two mainlines and the bottom ports: EIn to EOut on row 0, WIn to WOut on row 3, both 24 tiles wide.
 * The W to S branch splits off row 3 at x = 18 and ends at (18, bottom); the N to W joiner merges into row 3 at x = 15 from (15, bottom).
 * With split or merge false the port tile stands alone instead.
 */
static std::vector<JunctionRoute> Frame(int bottom, bool split, bool merge, std::string row0 = Repeat('F', 23))
{
	std::vector<JunctionRoute> routes;
	routes.push_back(R(0, 0, Trackdir::X_SW, row0));
	routes.push_back(R(23, 3, Trackdir::X_NE, Repeat('F', 23)));
	if (split) {
		routes.push_back(R(23, 3, Trackdir::X_NE, "FFFFRR" + Repeat('F', bottom - 4)));
	} else {
		routes.push_back(R(18, bottom, Trackdir::Y_SE, ""));
	}
	if (merge) {
		routes.push_back(R(15, bottom, Trackdir::Y_NW, Repeat('F', bottom - 4) + "RR" + Repeat('F', 14)));
	} else {
		routes.push_back(R(15, bottom, Trackdir::Y_NW, ""));
	}
	return routes;
}

static const JunctionSignal *FindSignal(const JunctionLayout &layout, int x, int y, Trackdir td)
{
	for (const JunctionSignal &s : layout.signals) {
		if (s.offset.x == x && s.offset.y == y && s.facing == td) return &s;
	}
	return nullptr;
}

static std::string Report(const JunctionLayout &layout, const std::vector<RuleViolation> &violations)
{
	std::string out = RenderJunctionAscii(layout);
	for (const RuleViolation &v : violations) out += fmt::format("\n{} ({},{}): {}", static_cast<int>(v.rule), v.tile.x, v.tile.y, v.message);
	return out;
}

TEST_CASE("PlaceJunctionSignals: the doubled reference keeps its non-prio rules (D100, D107)", "[junction][signals]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	/* The route lines of layouts/reference-LR-TL4-doubled.design. */
	JunctionLayout layout = BuildRoutes({
		R(6, 0, Trackdir::X_SW, "FFFFFFFFFFFFFFFFFFFFFFFFFFF"),
		R(6, 0, Trackdir::X_SW, "FFFFFFFFFLFFFFLFFFFFRFFFFFFFLFFF"),
		R(33, 2, Trackdir::X_NE, "FFFFFFFFFFB5FFFFFFFFFF"),
		R(33, 2, Trackdir::X_NE, "FFRFLFFFFFFFB3FFLFRFFFFFFF"),
		R(33, 2, Trackdir::X_NE, "FFRFLFFFFRFFFFRFFFFFFFFFF"),
		R(20, 16, Trackdir::Y_NW, "FFFB2FFFFFFLFFFFLFFFFFFFFFF"),
		R(20, 16, Trackdir::Y_NW, "FRFFFFFFFFFFFFFLFFRFFFFRFFFF"),
	});
	PlaceJunctionSignals(layout, spec);
	NormaliseJunctionLayout(layout);
	const std::vector<RuleViolation> violations = CheckJunctionRules(layout, spec);
	INFO(Report(layout, violations));
	/*
	 * This hand-routed reference predates D100 (it still has the old asymmetric lanes) and D107 (its N to W merge has no margin after the lane merge),
	 * so it is no longer fully rule-clean: the prio rules and the signal after the lane merge give way. Everything else must hold.
	 */
	for (const RuleViolation &v : violations) {
		if (v.rule == JunctionRule::Prio || v.rule == JunctionRule::PrioReach) continue;
		CHECK(v.rule == JunctionRule::Signals);
		CHECK(v.message == "no signal after a merge");
	}
	INFO(RenderJunctionAscii(layout));
}

TEST_CASE("PlaceJunctionSignals: signals before splits and merges and after merges", "[junction][signals]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	JunctionLayout layout = BuildRoutes(Frame(5, true, true));
	PlaceJunctionSignals(layout, spec);
	INFO(RenderJunctionAscii(layout));

	/* The W to S split at (18,3), the N to W merge at (15,3). */
	CHECK(FindSignal(layout, 19, 3, Trackdir::X_NE) != nullptr); // Before the split.
	CHECK(FindSignal(layout, 16, 3, Trackdir::X_NE) != nullptr); // Before the merge on the ML.
	CHECK(FindSignal(layout, 15, 4, Trackdir::Y_NW) != nullptr); // Before the merge on the joiner.
	CHECK(FindSignal(layout, 14, 3, Trackdir::X_NE) != nullptr); // After the merge.
}

TEST_CASE("PlaceJunctionSignals: signals around a bridge", "[junction][signals]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	/* Heads at x = 3 and x = 5 on row 0. */
	JunctionLayout layout = BuildRoutes(Frame(5, false, false, "FFB1" + Repeat('F', 18)));
	REQUIRE(layout.bridges.size() == 1);
	PlaceJunctionSignals(layout, spec);
	INFO(RenderJunctionAscii(layout));
	CHECK(FindSignal(layout, 2, 0, Trackdir::X_SW) != nullptr);
	CHECK(FindSignal(layout, 6, 0, Trackdir::X_SW) != nullptr);
}

TEST_CASE("PlaceJunctionSignals: the joiner's signal at an ML merge is Entry, a lane merge's is not", "[junction][signals]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	/* Lane B of the north row jogs to row 1 and back, so its rejoin is a lane merge. */
	std::vector<JunctionRoute> routes = Frame(5, false, true);
	routes.push_back(R(0, 0, Trackdir::X_SW, "FFFLFR" + Repeat('F', 8) + "RFL" + Repeat('F', 8)));
	JunctionLayout layout = BuildRoutes(routes);
	PlaceJunctionSignals(layout, spec);
	INFO(RenderJunctionAscii(layout));

	const JunctionSignal *joiner = FindSignal(layout, 15, 4, Trackdir::Y_NW);
	REQUIRE(joiner != nullptr);
	CHECK(joiner->type == SignalType::Entry);
	CHECK(std::ranges::count_if(layout.signals, [](const JunctionSignal &s) { return s.type == SignalType::Entry; }) == 1);
}

TEST_CASE("PlaceJunctionSignals: a queue longer than two bays gets bay signals", "[junction][signals]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	/* The joiner climbs from (15,14) to the merge at (15,3); the queue merge signal is at (15,4), and bays are 8 units. */
	JunctionLayout layout = BuildRoutes(Frame(14, false, true));
	PlaceJunctionSignals(layout, spec);
	INFO(RenderJunctionAscii(layout));
	CHECK(FindSignal(layout, 15, 8, Trackdir::Y_NW) != nullptr);
	CHECK(FindSignal(layout, 15, 12, Trackdir::Y_NW) != nullptr);

	const JunctionCheckContext ctx = MakeJunctionCheckContext(layout, spec, FindJunctionPorts(layout), JunctionTMovements());
	for (const RuleViolation &v : CheckWaitingRoom(ctx)) CHECK(v.message.find("bay boundary") == std::string::npos);
}

/** The x of every signalled piece on the path, by walking it. */
static std::vector<int> SignalledXs(const JunctionLayout &layout, const JunctionPath &path)
{
	std::vector<int> xs;
	for (const JunctionPiece &piece : path.pieces) {
		if (IsSignalledPiece(layout, piece)) xs.push_back(piece.tile.x);
	}
	return xs;
}

TEST_CASE("PlaceJunctionSignals: fill pitch matches drag-signalling on a straight", "[junction][signals]")
{
	JunctionSpec spec = MakeTestJunctionSpec();
	for (const uint sd : {2u, 4u}) {
		spec.sd = sd;
		JunctionLayout layout = BuildRoutes(Frame(5, false, false));
		PlaceJunctionSignals(layout, spec);
		INFO(RenderJunctionAscii(layout));

		const JunctionCheckContext ctx = MakeJunctionCheckContext(layout, spec, FindJunctionPorts(layout), JunctionTMovements());
		const auto path = std::ranges::find_if(ctx.paths, [](const JunctionPath &p) { return p.from == Role::EIn && p.to == Role::EOut; });
		REQUIRE(path != ctx.paths.end());

		/* Walking upstream from the last piece (x = 23), a signal goes where the count reaches SD axis pieces. */
		std::vector<int> expected;
		for (int x = 23 - static_cast<int>(sd); x >= 1; x -= static_cast<int>(sd)) expected.push_back(x);
		std::ranges::reverse(expected);
		for (const int x : expected) CHECK(FindSignal(layout, x, 0, Trackdir::X_SW) != nullptr);
		CHECK(SignalledXs(layout, *path) == expected);
	}
}

TEST_CASE("PlaceJunctionSignals: fill on a diagonal uses 128 per half piece", "[junction][signals]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	/* Row 0 rises into a run of 12 diagonal half pieces, runs back down in another 12, and ends on row 0 at x = 23. */
	JunctionLayout layout = BuildRoutes(Frame(5, false, false, "FFFR" + Repeat('F', 11) + "LL" + Repeat('F', 11) + "R" + Repeat('F', 6)));
	PlaceJunctionSignals(layout, spec);
	INFO(RenderJunctionAscii(layout));

	const JunctionCheckContext ctx = MakeJunctionCheckContext(layout, spec, FindJunctionPorts(layout), JunctionTMovements());
	const auto path = std::ranges::find_if(ctx.paths, [](const JunctionPath &p) { return p.from == Role::EIn && p.to == Role::EOut; });
	REQUIRE(path != ctx.paths.end());

	/* Consecutive signals with only half pieces between them (the apex between the two runs is an axis piece) are 3 pieces apart. */
	std::vector<size_t> signalled;
	for (size_t i = 0; i < path->pieces.size(); i++) {
		if (IsSignalledPiece(layout, path->pieces[i])) signalled.push_back(i);
	}
	uint pairs = 0;
	for (size_t j = 1; j < signalled.size(); j++) {
		const auto between = std::span(path->pieces).subspan(signalled[j - 1], signalled[j] - signalled[j - 1] + 1);
		if (!std::ranges::all_of(between, [](const JunctionPiece &p) { return p.units == 1; })) continue;
		CHECK(signalled[j] - signalled[j - 1] == 3);
		pairs++;
	}
	CHECK(pairs >= 4);
}

TEST_CASE("CheckJunctionRules: signals too far apart", "[junction][signals]")
{
	JunctionSpec spec = MakeTestJunctionSpec();
	spec.sd = 2;
	/* Signals on the E row, with a gap of n axis pieces between them (n pieces unsignalled). */
	const auto too_far = [&](int gap) {
		JunctionLayout layout = BuildRoutes(Frame(5, false, false));
		std::vector<int> xs = {1, 3, 5};
		for (int x = 6 + gap; x <= 21; x += 2) xs.push_back(x);
		if (xs.back() != 21) xs.push_back(21);
		for (const int x : xs) {
			layout.signals.push_back({{static_cast<int16_t>(x), 0}, Trackdir::X_SW, spec.signal_type, SignalVariant::Electric});
		}
		for (const RuleViolation &v : CheckJunctionRules(layout, spec)) {
			if (v.message == "signals too far apart" && v.tile.y == 0) return true;
		}
		return false;
	};
	CHECK(!too_far(2));
	CHECK(too_far(3));
}

/** The track bits of a tile, as the pass left them. */
static TrackBits Bits(const JunctionLayout &layout, int x, int y)
{
	return TracksAt(layout, {static_cast<int16_t>(x), static_cast<int16_t>(y)});
}

/*
 * D75's table rows for an ML merge: the prio goes opposite the joiner's approach. The rows with the prio on +y need a joiner
 * that approaches from -y, which needs bridged joiners; the checker fixtures cover those.
 */
TEST_CASE("PlaceJunctionSignals: prio side and join pieces", "[junction][signals]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();

	/* An ML going -x on row 3, the joiner from +y at (15,3): prio on -y, h1 Upper on the merge tile, h2 Lower beside it. */
	{
		JunctionLayout layout = BuildRoutes(Frame(5, false, true));
		PlaceJunctionSignals(layout, spec);
		INFO(RenderJunctionAscii(layout));
		CHECK(Bits(layout, 15, 3).Test(Track::Upper));
		CHECK(Bits(layout, 15, 2) == TrackBits{Track::Lower}); // D81: h2 alone beside the merge tile.
	}

	/* An ML going +x on row 0, the joiner from +y over a bridge at (15,0): prio on -y, h1 Left, h2 Right. */
	{
		std::vector<JunctionRoute> routes = Frame(5, false, false);
		routes.push_back(R(15, 5, Trackdir::Y_NW, "B1FLL" + Repeat('F', 7)));
		JunctionLayout layout = BuildRoutes(routes);
		REQUIRE(layout.bridges.size() == 1);
		PlaceJunctionSignals(layout, spec);
		INFO(RenderJunctionAscii(layout));
		CHECK(Bits(layout, 15, 0).Test(Track::Left));
		CHECK(Bits(layout, 15, -1) == TrackBits{Track::Right});
	}
}
