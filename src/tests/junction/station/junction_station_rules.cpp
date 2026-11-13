/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_station_rules.cpp Test functionality from junction_station_rules. */

#include "../../../stdafx.h"

#include "../../../3rdparty/catch2/catch.hpp"

#include "../../../junction/junction_route.h"
#include "../../../junction/station/junction_station_rules.h"
#include "../../../core/format.hpp"
#include "../../../track_func.h"

#include <algorithm>
#include <string>

#include "../../../safeguards.h"

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

/** The sd used by the fixture tests. */
static constexpr uint8_t FIXTURE_SD = 4;

/** A ro-ro drop station layout with the ports its rules are checked against. */
struct StationFixture {
	JunctionLayout layout;
	JunctionStationPorts ports;
};

static void AddSignal(JunctionLayout &layout, int x, int y)
{
	layout.signals.push_back({{static_cast<int16_t>(x), static_cast<int16_t>(y)}, Trackdir::X_SW, SignalType::Entry, SignalVariant::Electric});
}

static void RemoveSignal(JunctionLayout &layout, int x, int y)
{
	std::erase_if(layout.signals, [&](const JunctionSignal &s) { return s.offset.x == x && s.offset.y == y; });
}

/**
 * A two platform ro-ro drop station along row 0 and row 1, trains travelling towards +x.
 * An in stub of 4 tiles (x = 0..3), a split at x = 4 (row 1 leaves by an S-bend), the platforms at x = 5..4+tl,
 * the mirrored merge at x = 6+tl, and an out stub of 2 tiles (x = 7+tl, 8+tl).
 */
static StationFixture MakeRoRoDropFixture(uint8_t tl = 4, uint8_t platforms = 2)
{
	REQUIRE(platforms == 2);
	StationFixture f;
	f.layout = BuildRoutes({
		R(0, 0, Trackdir::X_SW, std::string(8 + tl, 'F')),
		R(3, 0, Trackdir::X_SW, "LFR" + std::string(tl, 'F') + "RFL"),
	});
	f.layout.stations.push_back({{5, 0}, Axis::X, platforms, tl});
	AddSignal(f.layout, 3, 0); /* Entry, before the split. */
	AddSignal(f.layout, 5 + tl, 0); /* Platform exits. */
	AddSignal(f.layout, 5 + tl, 1);
	AddSignal(f.layout, 7 + tl, 0); /* After the merge. */
	f.ports.in = {{0, 0}, Trackdir::X_SW};
	f.ports.out = {{static_cast<int16_t>(8 + tl), 0}, Trackdir::X_SW};

	const JunctionStationBlock &block = f.layout.stations.front();
	WARN(fmt::format("ro-ro drop fixture (tl {}), station block rect x {}..{}, y {}..{}:\n{}", tl, block.origin.x, block.origin.x + block.length - 1, block.origin.y, block.origin.y + block.platforms - 1, RenderJunctionAscii(f.layout)));
	return f;
}

/** Check that the rules fire and that the violations are all of the expected rule. */
static void RequireOnly(const StationFixture &f, JunctionStationRule rule)
{
	const auto violations = CheckStationRules(f.layout, f.ports, 4, FIXTURE_SD);
	CHECK(!violations.empty());
	for (const StationRuleViolation &v : violations) {
		INFO(fmt::format("({}, {}) {}", v.tile.x, v.tile.y, v.message));
		CHECK(v.rule == rule);
	}
}

TEST_CASE("CheckStationRules: a correct ro-ro drop passes", "[junction][station]")
{
	const StationFixture f = MakeRoRoDropFixture();
	const auto violations = CheckStationRules(f.layout, f.ports, 4, FIXTURE_SD);
	for (const StationRuleViolation &v : violations) WARN(fmt::format("({}, {}) {}", v.tile.x, v.tile.y, v.message));
	CHECK(violations.empty());
}

TEST_CASE("CheckStationRules: platforms of the wrong length", "[junction][station]")
{
	/* A fixture built for TL 3, checked against TL 4. */
	RequireOnly(MakeRoRoDropFixture(3), JunctionStationRule::Platforms);
}

TEST_CASE("CheckStationRules: a disconnected platform row", "[junction][station]")
{
	StationFixture f = MakeRoRoDropFixture();
	/* Remove the merge track of row 1. */
	std::erase_if(f.layout.tracks, [](const JunctionTrack &jt) { return jt.offset.x == 10 && jt.offset.y == 1; });
	for (JunctionTrack &jt : f.layout.tracks) {
		if (jt.offset.x == 10 && jt.offset.y == 0) jt.tracks.Reset(Track::Lower);
	}
	RequireOnly(f, JunctionStationRule::Connectivity);
}

TEST_CASE("CheckStationRules: no entry signal", "[junction][station]")
{
	StationFixture f = MakeRoRoDropFixture();
	RemoveSignal(f.layout, 3, 0);
	RequireOnly(f, JunctionStationRule::Signals);
}

TEST_CASE("CheckStationRules: a platform without an exit signal", "[junction][station]")
{
	StationFixture f = MakeRoRoDropFixture();
	RemoveSignal(f.layout, 9, 1);
	RequireOnly(f, JunctionStationRule::Signals);
}

TEST_CASE("CheckStationRules: a signal on a station tile", "[junction][station]")
{
	StationFixture f = MakeRoRoDropFixture();
	AddSignal(f.layout, 6, 0);
	RequireOnly(f, JunctionStationRule::Signals);
}

TEST_CASE("CheckStationRules: too little braking space before the split", "[junction][station]")
{
	StationFixture f = MakeRoRoDropFixture();
	f.ports.in = {{2, 0}, Trackdir::X_SW}; /* An in stub of 2 tiles. */
	RequireOnly(f, JunctionStationRule::Braking);
}
