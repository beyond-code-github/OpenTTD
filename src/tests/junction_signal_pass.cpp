/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_signal_pass.cpp Test functionality from junction_signal_pass. */

#include "../stdafx.h"

#include "../3rdparty/catch2/catch.hpp"

#include "../junction_generator.h"
#include "../junction_route.h"
#include "../junction_rules.h"
#include "../junction_rules_internal.h"
#include "../core/format.hpp"
#include "../junction_signal_pass.h"
#include "../track_func.h"

#include <algorithm>
#include <string>

#include "../safeguards.h"

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

TEST_CASE("PlaceJunctionSignals: the doubled reference comes out rule-clean", "[junction][signals]")
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
	REQUIRE(violations.empty());
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

TEST_CASE("PlaceJunctionSignals: fill keeps gaps within twice the spacing", "[junction][signals]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	JunctionLayout layout = BuildRoutes(Frame(5, false, false));
	PlaceJunctionSignals(layout, spec);
	INFO(RenderJunctionAscii(layout));

	const JunctionCheckContext ctx = MakeJunctionCheckContext(layout, spec, FindJunctionPorts(layout), JunctionTMovements());
	const auto path = std::ranges::find_if(ctx.paths, [](const JunctionPath &p) { return p.from == Role::EIn && p.to == Role::EOut; });
	REQUIRE(path != ctx.paths.end());
	size_t anchor = 0;
	uint count = 0;
	for (size_t i = 1; i < path->pieces.size(); i++) {
		if (!IsSignalledPiece(layout, path->pieces[i]) && i + 1 != path->pieces.size()) continue;
		uint gap = 0;
		for (size_t k = anchor + 1; k <= i; k++) gap += path->pieces[k].units;
		CHECK(gap <= 2u * spec.sd);
		CHECK(gap >= spec.sd);
		anchor = i;
		count++;
	}
	CHECK(count > 5);
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
