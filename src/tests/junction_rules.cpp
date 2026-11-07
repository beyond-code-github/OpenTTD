/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_rules.cpp Test functionality from junction_rules. */

#include "../stdafx.h"

#include "../3rdparty/catch2/catch.hpp"

#include "../junction_generator.h"
#include "../junction_route.h"
#include "../junction_rules.h"
#include "../track_func.h"

#include "../safeguards.h"

using Role = JunctionPortRole;
using Movement = std::pair<JunctionPortRole, JunctionPortRole>;

/** Build a normalised layout from route programs; the start tiles must not be negative. */
static JunctionLayout Build(std::initializer_list<JunctionRoute> routes)
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

static JunctionPort Port(Role role, int x, int y, Trackdir td)
{
	return {role, {static_cast<int16_t>(x), static_cast<int16_t>(y)}, td};
}

/** Whether a violation of the rule is at the tile and its message contains the text. */
static bool Has(const std::vector<RuleViolation> &violations, JunctionRule rule, int x, int y, std::string_view text)
{
	return std::ranges::any_of(violations, [&](const RuleViolation &v) {
		return v.rule == rule && v.tile.x == x && v.tile.y == y && v.message.find(text) != std::string::npos;
	});
}

static void AddSignal(JunctionLayout &layout, int x, int y, Trackdir td, SignalType type = SignalType::Block)
{
	layout.signals.push_back({{static_cast<int16_t>(x), static_cast<int16_t>(y)}, td, type, SignalVariant::Electric});
}

TEST_CASE("FindJunctionPorts: the six ports", "[junction][rules]")
{
	/*
	 * 7x6, ports marked P:
	 *   . . . . . . .
	 *   P- . . . . -P      EIn at (0,1), EOut at (6,1)
	 *   P- . . . . -P      WOut at (0,2), WIn at (6,2)
	 *   . . . . . . .
	 *   . . . . . . .
	 *   . . | | . . .      NIn at (2,5), SOut at (3,5)
	 */
	JunctionLayout layout;
	layout.size = {7, 6};
	for (int y : {1, 2}) {
		layout.tracks.push_back({{0, static_cast<int16_t>(y)}, TrackBits{Track::X}});
		layout.tracks.push_back({{6, static_cast<int16_t>(y)}, TrackBits{Track::X}});
	}
	layout.tracks.push_back({{2, 5}, TrackBits{Track::Y}});
	layout.tracks.push_back({{3, 5}, TrackBits{Track::Y}});
	layout.tracks.push_back({{3, 0}, TrackBits{Track::Y}}); // Leaves through the top edge: never a port.

	const std::vector<JunctionPort> ports = FindJunctionPorts(layout);
	REQUIRE(ports.size() == 6);
	auto find = [&](Role role) -> const JunctionPort & {
		auto it = std::ranges::find_if(ports, [&](const JunctionPort &p) { return p.role == role; });
		REQUIRE(it != ports.end());
		return *it;
	};
	CHECK(find(Role::EIn).tile.x == 0);
	CHECK(find(Role::EIn).tile.y == 1);
	CHECK(find(Role::EIn).trackdir == Trackdir::X_SW);
	CHECK(find(Role::WOut).tile.x == 0);
	CHECK(find(Role::WOut).tile.y == 2);
	CHECK(find(Role::WOut).trackdir == Trackdir::X_NE);
	CHECK(find(Role::EOut).tile.x == 6);
	CHECK(find(Role::EOut).tile.y == 1);
	CHECK(find(Role::EOut).trackdir == Trackdir::X_SW);
	CHECK(find(Role::WIn).tile.x == 6);
	CHECK(find(Role::WIn).tile.y == 2);
	CHECK(find(Role::WIn).trackdir == Trackdir::X_NE);
	CHECK(find(Role::NIn).tile.x == 2);
	CHECK(find(Role::NIn).tile.y == 5);
	CHECK(find(Role::NIn).trackdir == Trackdir::Y_NW);
	CHECK(find(Role::SOut).tile.x == 3);
	CHECK(find(Role::SOut).tile.y == 5);
	CHECK(find(Role::SOut).trackdir == Trackdir::Y_SE);
}

TEST_CASE("FindJunctionPorts: mismatches give no ports for the edge", "[junction][rules]")
{
	JunctionLayout layout;
	layout.size = {7, 6};
	layout.tracks.push_back({{0, 1}, TrackBits{Track::X}});
	layout.tracks.push_back({{0, 2}, TrackBits{Track::X}});
	layout.tracks.push_back({{6, 1}, TrackBits{Track::X}});
	layout.tracks.push_back({{6, 2}, TrackBits{Track::X}});
	layout.tracks.push_back({{2, 5}, TrackBits{Track::Y}});
	layout.tracks.push_back({{3, 5}, TrackBits{Track::Y}});
	REQUIRE(FindJunctionPorts(layout).size() == 6);

	/* A third west port tile. */
	JunctionLayout three = layout;
	three.tracks.push_back({{0, 3}, TrackBits{Track::X}});
	CHECK(FindJunctionPorts(three).size() == 4);

	/* A west port that is not the X track. */
	JunctionLayout wrong_track = layout;
	wrong_track.tracks[1].tracks = TrackBits{Track::Upper};
	CHECK(FindJunctionPorts(wrong_track).size() == 4);

	/* One bottom port only. */
	JunctionLayout one = layout;
	one.tracks.pop_back();
	CHECK(FindJunctionPorts(one).size() == 4);
}

TEST_CASE("MakeJunctionCheckContext: tracing a line and a bridge", "[junction][rules]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	/* B1 gives X H = H X; the heads at x = 1 and 3, the span at x = 2. */
	const JunctionLayout layout = Build({{{0, 0}, Trackdir::X_SW, "B1F"}});
	const JunctionCheckContext ctx = MakeJunctionCheckContext(layout, spec,
			{Port(Role::EIn, 0, 0, Trackdir::X_SW), Port(Role::EOut, 4, 0, Trackdir::X_SW)}, {{Role::EIn, Role::EOut}});
	CHECK(ctx.trace_problems.empty());
	REQUIRE(ctx.paths.size() == 1);
	CHECK(ctx.paths[0].from == Role::EIn);
	CHECK(ctx.paths[0].to == Role::EOut);
	REQUIRE(ctx.paths[0].pieces.size() == 5);
	for (size_t i = 0; i < 5; i++) {
		CHECK(ctx.paths[0].pieces[i].tile.x == static_cast<int>(i));
		CHECK(ctx.paths[0].pieces[i].units == 2);
		CHECK(ctx.paths[0].pieces[i].on_bridge == (i >= 1 && i <= 3));
	}
}

TEST_CASE("CheckConnectivity: a straight line passes", "[junction][rules]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionLayout layout = Build({{{0, 0}, Trackdir::X_SW, "FFFF"}});
	const JunctionCheckContext ctx = MakeJunctionCheckContext(layout, spec,
			{Port(Role::EIn, 0, 0, Trackdir::X_SW), Port(Role::EOut, 4, 0, Trackdir::X_SW)}, {{Role::EIn, Role::EOut}});
	CHECK(CheckConnectivity(ctx).empty());
}

TEST_CASE("CheckConnectivity: dead end", "[junction][rules]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	/* "- - . - -": the track stops at (1,0) and (3,0) is not reachable. */
	JunctionLayout layout = Build({{{0, 0}, Trackdir::X_SW, "FFFF"}});
	std::erase_if(layout.tracks, [](const JunctionTrack &jt) { return jt.offset.x == 2; });
	const JunctionCheckContext ctx = MakeJunctionCheckContext(layout, spec,
			{Port(Role::EIn, 0, 0, Trackdir::X_SW), Port(Role::EOut, 4, 0, Trackdir::X_SW)}, {{Role::EIn, Role::EOut}});
	const std::vector<RuleViolation> violations = CheckConnectivity(ctx);
	CHECK(Has(violations, JunctionRule::Connectivity, 1, 0, "dead end"));
	CHECK(Has(violations, JunctionRule::Connectivity, 0, 0, "missing movement"));
}

TEST_CASE("CheckConnectivity: unexpected exit", "[junction][rules]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionLayout layout = Build({{{0, 0}, Trackdir::X_SW, "FFFF"}});
	const JunctionCheckContext ctx = MakeJunctionCheckContext(layout, spec, {Port(Role::EIn, 0, 0, Trackdir::X_SW)}, {});
	const std::vector<RuleViolation> violations = CheckConnectivity(ctx);
	CHECK(Has(violations, JunctionRule::Connectivity, 4, 0, "unexpected exit"));
}

TEST_CASE("CheckConnectivity: cycle", "[junction][rules]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	/*
	 * An entry line joins a ring of eight half-piece turns at (2,0).
	 *   - - # \
	 *   . . | |
	 *   . . \ - /   (the ring's tiles below the entry row; the first piece of the ring is at (2,0))
	 */
	const JunctionLayout layout = Build({{{0, 0}, Trackdir::X_SW, "FF"}, {{2, 0}, Trackdir::X_SW, "LLLLLLLL"}});
	const JunctionCheckContext ctx = MakeJunctionCheckContext(layout, spec, {Port(Role::EIn, 0, 0, Trackdir::X_SW)}, {});
	const std::vector<RuleViolation> violations = CheckConnectivity(ctx);
	CHECK(Has(violations, JunctionRule::Connectivity, 2, 0, "cycle"));
}

TEST_CASE("CheckConnectivity: ambiguous movement", "[junction][rules]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	/*
	 * Two ways from (0,0) to (9,0): straight, or a detour through row 1.
	 *   - - # - - # - - - -
	 *   . . \ - - / . . . .
	 */
	const JunctionLayout layout = Build({{{0, 0}, Trackdir::X_SW, "FFFFFFFFF"}, {{1, 0}, Trackdir::X_SW, "LFRFRFL"}});
	const JunctionCheckContext ctx = MakeJunctionCheckContext(layout, spec,
			{Port(Role::EIn, 0, 0, Trackdir::X_SW), Port(Role::EOut, 9, 0, Trackdir::X_SW)}, {{Role::EIn, Role::EOut}});
	REQUIRE(ctx.paths.size() == 2);
	CHECK(Has(CheckConnectivity(ctx), JunctionRule::Connectivity, 0, 0, "ambiguous movement"));
}

TEST_CASE("CheckConnectivity: unexpected movement and missing port", "[junction][rules]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionLayout layout = Build({{{0, 0}, Trackdir::X_SW, "FFFF"}});
	const std::vector<JunctionPort> ports = {Port(Role::EIn, 0, 0, Trackdir::X_SW), Port(Role::WOut, 4, 0, Trackdir::X_SW)};

	/* The line runs EIn to WOut, but only EIn to EOut is expected: unexpected, and EOut has no port. */
	const JunctionCheckContext ctx = MakeJunctionCheckContext(layout, spec, ports, {{Role::EIn, Role::EOut}});
	const std::vector<RuleViolation> violations = CheckConnectivity(ctx);
	CHECK(Has(violations, JunctionRule::Connectivity, 4, 0, "unexpected movement"));
	CHECK(Has(violations, JunctionRule::Connectivity, 0, 0, "missing port"));
}

TEST_CASE("CheckGeometry: level crossing", "[junction][rules]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const std::vector<JunctionPort> ports = {
		Port(Role::EIn, 0, 1, Trackdir::X_SW), Port(Role::EOut, 6, 1, Trackdir::X_SW),
		Port(Role::NIn, 2, 0, Trackdir::Y_SE), Port(Role::SOut, 2, 2, Trackdir::Y_SE),
	};
	const std::vector<Movement> expected = {{Role::EIn, Role::EOut}, {Role::NIn, Role::SOut}};

	/*
	 * The Y line passes under a bridge: no crossing.
	 *   . . | . . . .
	 *   - H = H - - -
	 *   . . | . . . .
	 */
	const JunctionLayout under = Build({{{0, 1}, Trackdir::X_SW, "B1FFF"}, {{2, 0}, Trackdir::Y_SE, "FF"}});
	const JunctionCheckContext ctx_under = MakeJunctionCheckContext(under, spec, ports, expected);
	REQUIRE(ctx_under.paths.size() == 2);
	CHECK(CheckGeometry(ctx_under).empty());

	/* The same lines on one tile: a level crossing at (2,1). */
	const JunctionLayout crossing = Build({{{0, 1}, Trackdir::X_SW, "FFFFFF"}, {{2, 0}, Trackdir::Y_SE, "FF"}});
	const JunctionCheckContext ctx_cross = MakeJunctionCheckContext(crossing, spec, ports, expected);
	CHECK(Has(CheckGeometry(ctx_cross), JunctionRule::Geometry, 2, 1, "crossing"));
}

/**
 * The main line runs along row 1 from x = 0 to 9. A branch leaves it and a joiner joins it.
 * @param split_x The tile where the branch leaves.
 * @param merge_x The tile where the joiner merges.
 */
static JunctionLayout SplitAndMerge(int split_x, int merge_x)
{
	const std::string branch = std::string("LFR") + std::string(9 - (split_x + 1), 'F');
	return Build({
		{{0, 1}, Trackdir::X_SW, "FFFFFFFFF"},
		{{static_cast<int16_t>(split_x - 1), 1}, Trackdir::X_SW, branch},
		{{static_cast<int16_t>(merge_x - 1), 0}, Trackdir::X_SW, "LFR"},
	});
}

static std::vector<JunctionPort> SplitAndMergePorts(int merge_x)
{
	return {
		Port(Role::EIn, 0, 1, Trackdir::X_SW), Port(Role::EOut, 9, 1, Trackdir::X_SW),
		Port(Role::WIn, merge_x - 1, 0, Trackdir::X_SW), Port(Role::SOut, 9, 2, Trackdir::X_SW),
	};
}

TEST_CASE("CheckOrder: split then merge passes, merge then split fails", "[junction][rules]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const std::vector<Movement> expected = {{Role::EIn, Role::EOut}, {Role::EIn, Role::SOut}, {Role::WIn, Role::EOut}};

	/*
	 * Branch leaves at (2,1), joiner merges at (6,1): splits before merges.
	 *   . . . . . - \ . . .
	 *   - - # - - - # - - -
	 *   . . \ - - - - - - -
	 */
	const JunctionLayout good = SplitAndMerge(2, 6);
	const JunctionCheckContext ctx_good = MakeJunctionCheckContext(good, spec, SplitAndMergePorts(6), expected);
	REQUIRE(ctx_good.paths.size() == 3);
	CHECK(CheckOrder(ctx_good).empty());

	/* Joiner merges at (1,1) and the branch leaves at (6,1): a merge before a split. */
	const JunctionLayout bad = SplitAndMerge(6, 1);
	const JunctionCheckContext ctx_bad = MakeJunctionCheckContext(bad, spec, SplitAndMergePorts(1), expected);
	REQUIRE(ctx_bad.paths.size() == 4);
	CHECK(Has(CheckOrder(ctx_bad), JunctionRule::Order, 6, 1, "split"));
}

/** X H = H X on row 0 with the tracks either side of the heads. */
static JunctionLayout BridgeLayout()
{
	return Build({{{0, 0}, Trackdir::X_SW, "B1F"}});
}

TEST_CASE("CheckBridges: a valid bridge passes", "[junction][rules]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionLayout layout = BridgeLayout();
	const JunctionCheckContext ctx = MakeJunctionCheckContext(layout, spec, {}, {});
	CHECK(CheckBridges(ctx).empty());
}

TEST_CASE("CheckBridges: each clause fails on its own", "[junction][rules]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();

	/* The heads are not on one row. */
	{
		JunctionLayout layout = BridgeLayout();
		layout.size = {5, 2};
		layout.bridges[0].end = {3, 1};
		const JunctionCheckContext ctx = MakeJunctionCheckContext(layout, spec, {}, {});
		CHECK(Has(CheckBridges(ctx), JunctionRule::Bridges, 1, 0, "row or column"));
	}

	/* The heads are the same tile. */
	{
		JunctionLayout layout = BridgeLayout();
		layout.bridges[0].end = layout.bridges[0].start;
		const JunctionCheckContext ctx = MakeJunctionCheckContext(layout, spec, {}, {});
		CHECK(Has(CheckBridges(ctx), JunctionRule::Bridges, 1, 0, "distinct"));
	}

	/* Track on a head. */
	{
		JunctionLayout layout = BridgeLayout();
		layout.tracks.push_back({{1, 0}, TrackBits{Track::X}});
		const JunctionCheckContext ctx = MakeJunctionCheckContext(layout, spec, {}, {});
		CHECK(Has(CheckBridges(ctx), JunctionRule::Bridges, 1, 0, "track on head"));
	}

	/* A signal on a head. */
	{
		JunctionLayout layout = BridgeLayout();
		AddSignal(layout, 3, 0, Trackdir::X_SW);
		const JunctionCheckContext ctx = MakeJunctionCheckContext(layout, spec, {}, {});
		CHECK(Has(CheckBridges(ctx), JunctionRule::Bridges, 3, 0, "signal on head"));
	}

	/* No axis track outside the far head. */
	{
		JunctionLayout layout = BridgeLayout();
		std::erase_if(layout.tracks, [](const JunctionTrack &jt) { return jt.offset.x == 4; });
		const JunctionCheckContext ctx = MakeJunctionCheckContext(layout, spec, {}, {});
		const std::vector<RuleViolation> violations = CheckBridges(ctx);
		CHECK(Has(violations, JunctionRule::Bridges, 3, 0, "approach"));
		CHECK_FALSE(Has(violations, JunctionRule::Bridges, 1, 0, "approach"));
	}

	/* A span length the spec does not accept. */
	{
		JunctionSpec strict = spec;
		strict.bridge_span_ok = [](uint) { return false; };
		const JunctionLayout layout = BridgeLayout();
		const JunctionCheckContext ctx = MakeJunctionCheckContext(layout, strict, {}, {});
		CHECK(Has(CheckBridges(ctx), JunctionRule::Bridges, 1, 0, "span"));
	}

	/*
	 * Two bridges crossing at the span tile (2,2).
	 *   . . | . .
	 *   . . H . .
	 *   - H + H -     the X bridge's span at (2,2) is also the Y bridge's span
	 *   . . H . .
	 *   . . | . .
	 */
	{
		JunctionLayout layout;
		layout.size = {5, 5};
		layout.tracks.push_back({{0, 2}, TrackBits{Track::X}});
		layout.tracks.push_back({{4, 2}, TrackBits{Track::X}});
		layout.tracks.push_back({{2, 0}, TrackBits{Track::Y}});
		layout.tracks.push_back({{2, 4}, TrackBits{Track::Y}});
		layout.bridges.push_back({{1, 2}, {3, 2}});
		layout.bridges.push_back({{2, 1}, {2, 3}});
		const JunctionCheckContext ctx = MakeJunctionCheckContext(layout, spec, {}, {});
		const std::vector<RuleViolation> violations = CheckBridges(ctx);
		CHECK(Has(violations, JunctionRule::Bridges, 2, 2, "shared"));
		CHECK(violations.size() == 1);
	}
}
