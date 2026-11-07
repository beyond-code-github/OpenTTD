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

/** A line of n X tiles along row 0. */
static JunctionLayout Line(int n)
{
	return Build({{{0, 0}, Trackdir::X_SW, std::string(n - 1, 'F')}});
}

static std::vector<JunctionPort> LinePorts(int n, Role to = Role::EOut)
{
	return {Port(Role::EIn, 0, 0, Trackdir::X_SW), Port(to, n - 1, 0, Trackdir::X_SW)};
}

/** Check one route from X_SW at the start of a path of its own, with the given reference speed. */
static std::vector<RuleViolation> CurveViolations(const char *moves, uint16_t reference_speed)
{
	JunctionSpec spec = MakeTestJunctionSpec();
	spec.reference_speed = reference_speed;
	const JunctionRaster raster = RasterizeJunctionRoute({{0, 0}, Trackdir::X_SW, moves});
	REQUIRE(raster.error.empty());
	JunctionLayout layout;
	AddRasterToLayout(layout, raster);
	NormaliseJunctionLayout(layout);
	const JunctionPiece &last = raster.pieces.back();
	const JunctionCheckContext ctx = MakeJunctionCheckContext(layout, spec,
			{Port(Role::EIn, 0, 0, Trackdir::X_SW), Port(Role::EOut, last.tile.x, last.tile.y, last.trackdir)}, {{Role::EIn, Role::EOut}});
	REQUIRE(ctx.paths.size() == 1);
	return CheckCurveSpeed(ctx);
}

TEST_CASE("CheckCurveSpeed: reference cases at TL 4", "[junction][rules]")
{
	/* A straight path never fails. */
	CHECK(CurveViolations("FFFFFFFF", 1000).empty());

	/* "LL": bends one unit apart, limit 88. */
	CHECK_FALSE(CurveViolations("LL", 89).empty());
	CHECK(CurveViolations("LL", 88).empty());

	/* "LFFL": bends three units apart, limit 132. */
	CHECK_FALSE(CurveViolations("LFFL", 140).empty());
	CHECK(CurveViolations("LFFL", 132).empty());

	/* "LFR": one S-bend alone is exempt. */
	CHECK(CurveViolations("LFR", 60000).empty());

	/* "LFRLFR": two S-bends, curvecount 2 and 2, numcurve 3, average spacing 2: limit 111. */
	CHECK_FALSE(CurveViolations("LFRLFR", 200).empty());
	CHECK_FALSE(CurveViolations("LFRLFR", 112).empty());
	CHECK(CurveViolations("LFRLFR", 111).empty());
}

TEST_CASE("CheckCurveSpeed: violation tile and the curve bonus", "[junction][rules]")
{
	/* The first failing window has its front on the first piece after the bends: (1,1) for "LL". */
	const std::vector<RuleViolation> violations = CurveViolations("LL", 89);
	REQUIRE(violations.size() == 1);
	CHECK(violations[0].rule == JunctionRule::CurveSpeed);

	/* With curve_speed 1 the limit of "LL" is 88 + 44 = 132. */
	JunctionSpec spec = MakeTestJunctionSpec();
	spec.curve_speed = 1;
	spec.reference_speed = 132;
	const JunctionLayout layout = Build({{{0, 0}, Trackdir::X_SW, "LL"}});
	JunctionCheckContext ctx = MakeJunctionCheckContext(layout, spec, {Port(Role::EIn, 0, 0, Trackdir::X_SW), Port(Role::EOut, 1, 1, Trackdir::Y_SE)}, {{Role::EIn, Role::EOut}});
	REQUIRE(ctx.paths.size() == 1);
	CHECK(CheckCurveSpeed(ctx).empty());
	spec.reference_speed = 133;
	CHECK_FALSE(CheckCurveSpeed(ctx).empty());
}

/**
 * A main line on row 0 with a branch leaving at (4,0).
 *   - - - - # - - - - -
 *   . . . . \ - - - - -
 */
static JunctionLayout SplitLayout()
{
	JunctionLayout layout = Build({{{0, 0}, Trackdir::X_SW, "FFFFFFFFF"}, {{3, 0}, Trackdir::X_SW, "LFRFFFF"}});
	for (int x : {1, 3, 5, 7}) AddSignal(layout, x, 0, Trackdir::X_SW);
	for (int x : {5, 7}) AddSignal(layout, x, 1, Trackdir::X_SW);
	return layout;
}

static std::vector<JunctionPort> SplitPorts()
{
	return {Port(Role::EIn, 0, 0, Trackdir::X_SW), Port(Role::EOut, 9, 0, Trackdir::X_SW), Port(Role::SOut, 9, 1, Trackdir::X_SW)};
}

/**
 * A main line on row 1 with a joiner merging at (5,1).
 *   . . . . - / . . . .
 *   - - - - - # - - - -
 */
static JunctionLayout MergeLayout()
{
	/* The joiner is the second route: (4,0) (5,0) (5,1) (6,1). */
	JunctionLayout layout = Build({{{0, 1}, Trackdir::X_SW, "FFFFFFFFF"}, {{4, 0}, Trackdir::X_SW, "LFR"}});
	for (int x : {2, 4, 6, 8}) AddSignal(layout, x, 1, Trackdir::X_SW);
	AddSignal(layout, 5, 0, Trackdir::Right_S, SignalType::Entry);
	return layout;
}

static std::vector<JunctionPort> MergePorts()
{
	return {Port(Role::EIn, 0, 1, Trackdir::X_SW), Port(Role::EOut, 9, 1, Trackdir::X_SW), Port(Role::WIn, 4, 0, Trackdir::X_SW)};
}

static const std::vector<Movement> MERGE_MOVEMENTS = {{Role::EIn, Role::EOut}, {Role::WIn, Role::EOut}};

static void RemoveSignal(JunctionLayout &layout, int x, int y)
{
	std::erase_if(layout.signals, [&](const JunctionSignal &s) { return s.offset.x == x && s.offset.y == y; });
}

TEST_CASE("CheckSignals: passing fixtures", "[junction][rules]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();

	const JunctionLayout line = [] { JunctionLayout l = Line(5); AddSignal(l, 1, 0, Trackdir::X_SW); AddSignal(l, 3, 0, Trackdir::X_SW); return l; }();
	CHECK(CheckSignals(MakeJunctionCheckContext(line, spec, LinePorts(5), {{Role::EIn, Role::EOut}})).empty());

	const JunctionLayout split = SplitLayout();
	const JunctionCheckContext split_ctx = MakeJunctionCheckContext(split, spec, SplitPorts(), {{Role::EIn, Role::EOut}, {Role::EIn, Role::SOut}});
	REQUIRE(split_ctx.paths.size() == 2);
	CHECK(CheckSignals(split_ctx).empty());

	/* The joiner's Entry signal at an ML merge is not a type violation here. */
	const JunctionLayout merge = MergeLayout();
	const JunctionCheckContext merge_ctx = MakeJunctionCheckContext(merge, spec, MergePorts(), MERGE_MOVEMENTS);
	REQUIRE(merge_ctx.paths.size() == 2);
	CHECK(CheckSignals(merge_ctx).empty());
}

TEST_CASE("CheckSignals: legality", "[junction][rules]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();

	/* A signal for a track the tile does not have. */
	{
		JunctionLayout layout = Line(5);
		AddSignal(layout, 2, 0, Trackdir::Y_SE);
		CHECK(Has(CheckSignals(MakeJunctionCheckContext(layout, spec, LinePorts(5), {{Role::EIn, Role::EOut}})), JunctionRule::Signals, 2, 0, "without its track"));
	}

	/* A signal on a tile with crossing tracks. */
	{
		JunctionLayout layout = Line(5);
		layout.tracks[2].tracks = TRACK_BIT_CROSS;
		AddSignal(layout, 2, 0, Trackdir::X_SW);
		CHECK(Has(CheckSignals(MakeJunctionCheckContext(layout, spec, LinePorts(5), {{Role::EIn, Role::EOut}})), JunctionRule::Signals, 2, 0, "overlapping"));
	}

	/* A signal on a bridge head. */
	{
		JunctionLayout layout = BridgeLayout();
		AddSignal(layout, 1, 0, Trackdir::X_SW);
		CHECK(Has(CheckSignals(MakeJunctionCheckContext(layout, spec, {}, {})), JunctionRule::Signals, 1, 0, "bridge head"));
	}
}

TEST_CASE("CheckSignals: facing", "[junction][rules]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	JunctionLayout layout = Line(5);
	AddSignal(layout, 1, 0, Trackdir::X_NE);
	AddSignal(layout, 3, 0, Trackdir::X_SW);
	CHECK(Has(CheckSignals(MakeJunctionCheckContext(layout, spec, LinePorts(5), {{Role::EIn, Role::EOut}})), JunctionRule::Signals, 1, 0, "against"));
}

TEST_CASE("CheckSignals: switch isolation", "[junction][rules]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();

	/* Before a split. */
	{
		JunctionLayout layout = SplitLayout();
		RemoveSignal(layout, 3, 0);
		const std::vector<RuleViolation> violations = CheckSignals(MakeJunctionCheckContext(layout, spec, SplitPorts(), {{Role::EIn, Role::EOut}, {Role::EIn, Role::SOut}}));
		CHECK(Has(violations, JunctionRule::Signals, 3, 0, "before a split"));
	}

	/* Before a merge, on the ML. */
	{
		JunctionLayout layout = MergeLayout();
		RemoveSignal(layout, 4, 1);
		const std::vector<RuleViolation> violations = CheckSignals(MakeJunctionCheckContext(layout, spec, MergePorts(), MERGE_MOVEMENTS));
		CHECK(Has(violations, JunctionRule::Signals, 4, 1, "before a merge"));
		CHECK(violations.size() == 1);
	}

	/* Before a merge, on the joiner. */
	{
		JunctionLayout layout = MergeLayout();
		RemoveSignal(layout, 5, 0);
		const std::vector<RuleViolation> violations = CheckSignals(MakeJunctionCheckContext(layout, spec, MergePorts(), MERGE_MOVEMENTS));
		CHECK(Has(violations, JunctionRule::Signals, 5, 0, "before a merge"));
		CHECK(violations.size() == 1);
	}

	/* After a merge. */
	{
		JunctionLayout layout = MergeLayout();
		RemoveSignal(layout, 6, 1);
		const std::vector<RuleViolation> violations = CheckSignals(MakeJunctionCheckContext(layout, spec, MergePorts(), MERGE_MOVEMENTS));
		CHECK(Has(violations, JunctionRule::Signals, 6, 1, "after a merge"));
	}
}

TEST_CASE("CheckSignals: spacing", "[junction][rules]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	/* Five tiles without signals: the third tile is 6 units from the start, more than 2 * SD = 4. */
	const JunctionLayout layout = Line(5);
	const std::vector<RuleViolation> violations = CheckSignals(MakeJunctionCheckContext(layout, spec, LinePorts(5), {{Role::EIn, Role::EOut}}));
	CHECK(Has(violations, JunctionRule::Signals, 2, 0, "too far apart"));
	CHECK(violations.size() == 1);
}

TEST_CASE("CheckSignals: types", "[junction][rules]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();

	/* A path signal where the setting says block. */
	{
		JunctionLayout layout = MergeLayout();
		layout.signals[0].type = SignalType::PathOneWay;
		const std::vector<RuleViolation> violations = CheckSignals(MakeJunctionCheckContext(layout, spec, MergePorts(), MERGE_MOVEMENTS));
		CHECK(Has(violations, JunctionRule::Signals, 2, 1, "type"));
		CHECK(violations.size() == 1);
	}

	/* An Entry signal that is not the joiner's. */
	{
		JunctionLayout layout = MergeLayout();
		layout.signals[1].type = SignalType::Entry;
		CHECK(Has(CheckSignals(MakeJunctionCheckContext(layout, spec, MergePorts(), MERGE_MOVEMENTS)), JunctionRule::Signals, 4, 1, "type"));
	}
}

/**
 * E to S leaves the ML at (3,0) and merges with the joiner at (9,1).
 *   - - - # - - - - - - - -
 *   . . . \ - - - - - - # - -
 *   . . . . . . . . - / . .     (joiner from (8,2))
 */
static JunctionLayout WaitingRoomLayout()
{
	return Build({{{0, 0}, Trackdir::X_SW, "FFFFFFFFFFF"}, {{2, 0}, Trackdir::X_SW, "LFRFFFFFFF"}, {{8, 2}, Trackdir::X_SW, "RFL"}});
}

static std::vector<JunctionPort> WaitingRoomPorts()
{
	return {Port(Role::EIn, 0, 0, Trackdir::X_SW), Port(Role::EOut, 11, 0, Trackdir::X_SW), Port(Role::NIn, 8, 2, Trackdir::X_SW), Port(Role::SOut, 11, 1, Trackdir::X_SW)};
}

static const std::vector<Movement> WAITING_ROOM_MOVEMENTS = {{Role::EIn, Role::EOut}, {Role::EIn, Role::SOut}, {Role::NIn, Role::SOut}};

TEST_CASE("CheckWaitingRoom", "[junction][rules]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();

	/* A signal at (8,1): 1 + 5 * 2 = 11 units after the split switch piece, at least 2 * TL = 8. */
	{
		JunctionLayout layout = WaitingRoomLayout();
		AddSignal(layout, 8, 1, Trackdir::X_SW);
		const JunctionCheckContext ctx = MakeJunctionCheckContext(layout, spec, WaitingRoomPorts(), WAITING_ROOM_MOVEMENTS);
		REQUIRE(ctx.paths.size() == 3);
		CHECK(CheckWaitingRoom(ctx).empty());
	}

	/* A signal at (6,1) leaves 1 + 3 * 2 = 7 units. */
	{
		JunctionLayout layout = WaitingRoomLayout();
		AddSignal(layout, 6, 1, Trackdir::X_SW);
		const JunctionCheckContext ctx = MakeJunctionCheckContext(layout, spec, WaitingRoomPorts(), WAITING_ROOM_MOVEMENTS);
		CHECK(Has(CheckWaitingRoom(ctx), JunctionRule::WaitingRoom, 9, 1, "waiting room"));
	}

	/* No signal between the split and the merge. */
	{
		const JunctionLayout layout = WaitingRoomLayout();
		const JunctionCheckContext ctx = MakeJunctionCheckContext(layout, spec, WaitingRoomPorts(), WAITING_ROOM_MOVEMENTS);
		CHECK(Has(CheckWaitingRoom(ctx), JunctionRule::WaitingRoom, 9, 1, "no signal"));
	}

	/* A split without a merge. */
	{
		const JunctionLayout layout = SplitLayout();
		const JunctionCheckContext ctx = MakeJunctionCheckContext(layout, spec, SplitPorts(), {{Role::EIn, Role::EOut}, {Role::EIn, Role::SOut}});
		CHECK(Has(CheckWaitingRoom(ctx), JunctionRule::WaitingRoom, 4, 0, "no merge"));
	}

	/* No split at all. */
	{
		const JunctionLayout layout = Line(5);
		const JunctionCheckContext ctx = MakeJunctionCheckContext(layout, spec, LinePorts(5, Role::SOut), {{Role::EIn, Role::SOut}});
		CHECK(Has(CheckWaitingRoom(ctx), JunctionRule::WaitingRoom, 0, 0, "no split"));
	}
}

TEST_CASE("CheckDeadlock", "[junction][rules]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();

	/* Without signals the whole junction is one block, with the split at (3,0) and the merge at (9,1). */
	{
		const JunctionLayout layout = WaitingRoomLayout();
		const JunctionCheckContext ctx = MakeJunctionCheckContext(layout, spec, WaitingRoomPorts(), WAITING_ROOM_MOVEMENTS);
		const std::vector<RuleViolation> violations = CheckDeadlock(ctx);
		CHECK(Has(violations, JunctionRule::Deadlock, 9, 1, "split and a merge"));
		CHECK(violations.size() == 1);
	}

	/* A signal at (8,1) puts the merge in a block of its own. */
	{
		JunctionLayout layout = WaitingRoomLayout();
		AddSignal(layout, 8, 1, Trackdir::X_SW);
		const JunctionCheckContext ctx = MakeJunctionCheckContext(layout, spec, WaitingRoomPorts(), WAITING_ROOM_MOVEMENTS);
		CHECK(CheckDeadlock(ctx).empty());
	}
}

/** Shift a layout and ports one tile towards +x, keeping the size. */
static void ShiftRight(JunctionLayout &layout, std::vector<JunctionPort> &ports)
{
	for (JunctionTrack &jt : layout.tracks) jt.offset.x++;
	for (JunctionSignal &s : layout.signals) s.offset.x++;
	for (JunctionPort &p : ports) p.tile.x++;
}

TEST_CASE("CheckFootprint", "[junction][rules]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();

	/* The waiting room layout is a tight box with clear west, east and bottom edges. */
	{
		JunctionLayout layout = WaitingRoomLayout();
		AddSignal(layout, 8, 1, Trackdir::X_SW);
		const JunctionCheckContext ctx = MakeJunctionCheckContext(layout, spec, WaitingRoomPorts(), WAITING_ROOM_MOVEMENTS);
		CHECK(CheckFootprint(ctx).empty());
	}

	/* The minimum offset is 1, not 0. */
	{
		JunctionLayout layout = WaitingRoomLayout();
		std::vector<JunctionPort> ports = WaitingRoomPorts();
		ShiftRight(layout, ports);
		layout.size.x++;
		const JunctionCheckContext ctx = MakeJunctionCheckContext(layout, spec, ports, WAITING_ROOM_MOVEMENTS);
		CHECK(Has(CheckFootprint(ctx), JunctionRule::Footprint, 1, 0, "minimum offsets"));
	}

	/* The size is bigger than the bounding box. */
	{
		JunctionLayout layout = WaitingRoomLayout();
		layout.size.y++;
		const JunctionCheckContext ctx = MakeJunctionCheckContext(layout, spec, WaitingRoomPorts(), WAITING_ROOM_MOVEMENTS);
		CHECK(Has(CheckFootprint(ctx), JunctionRule::Footprint, 0, 0, "bounding box"));
	}

	/* A signal on the east edge. */
	{
		JunctionLayout layout = WaitingRoomLayout();
		AddSignal(layout, 11, 0, Trackdir::X_SW);
		const JunctionCheckContext ctx = MakeJunctionCheckContext(layout, spec, WaitingRoomPorts(), WAITING_ROOM_MOVEMENTS);
		CHECK(Has(CheckFootprint(ctx), JunctionRule::Footprint, 11, 0, "signal"));
	}

	/*
	 * A switch on the bottom edge: the branch leaves row 1 at (3,1).
	 *   . . . \ - - -
	 *   - - - # . . .   wait: the branch goes up to row 0
	 */
	{
		const JunctionLayout layout = Build({{{0, 1}, Trackdir::X_SW, "FFFFFF"}, {{2, 1}, Trackdir::X_SW, "RFLFF"}});
		const JunctionCheckContext ctx = MakeJunctionCheckContext(layout, spec,
				{Port(Role::EIn, 0, 1, Trackdir::X_SW), Port(Role::EOut, 6, 1, Trackdir::X_SW), Port(Role::SOut, 6, 0, Trackdir::X_SW)}, {{Role::EIn, Role::EOut}, {Role::EIn, Role::SOut}});
		REQUIRE(ctx.paths.size() == 2);
		CHECK(Has(CheckFootprint(ctx), JunctionRule::Footprint, 3, 1, "switch"));
	}

	/* An unused track bit on the west edge. */
	{
		JunctionLayout layout = WaitingRoomLayout();
		layout.tracks.push_back({{0, 2}, TrackBits{Track::Y}});
		const JunctionCheckContext ctx = MakeJunctionCheckContext(layout, spec, WaitingRoomPorts(), WAITING_ROOM_MOVEMENTS);
		CHECK(Has(CheckFootprint(ctx), JunctionRule::Footprint, 0, 2, "unused"));
	}

	/* Bridge heads on the bottom edge: the bridge layout is one row high. */
	{
		const JunctionLayout layout = BridgeLayout();
		const JunctionCheckContext ctx = MakeJunctionCheckContext(layout, spec,
				{Port(Role::EIn, 0, 0, Trackdir::X_SW), Port(Role::EOut, 4, 0, Trackdir::X_SW)}, {{Role::EIn, Role::EOut}});
		const std::vector<RuleViolation> violations = CheckFootprint(ctx);
		CHECK(Has(violations, JunctionRule::Footprint, 1, 0, "bridge head"));
		CHECK(Has(violations, JunctionRule::Footprint, 3, 0, "bridge head"));
	}
}

TEST_CASE("CheckJunctionRules: runs the groups and honours curve_limit", "[junction][rules]")
{
	JunctionSpec spec = MakeTestJunctionSpec();

	const std::vector<RuleViolation> empty = CheckJunctionRules(JunctionLayout{}, spec);
	CHECK(Has(empty, JunctionRule::Connectivity, 0, 0, "missing port"));
	CHECK(std::ranges::none_of(empty, [](const RuleViolation &v) { return v.rule == JunctionRule::NotImplemented; }));

	/*
	 * EIn at (0,2) runs up to EOut at (4,0) through a tight S-bend; WIn at (4,3) runs straight to WOut at (0,3).
	 *   . . . - -
	 *   . \ . . .
	 *   - # . . .   (the bend: half pieces and a Y piece in column 1)
	 *   - - - - -
	 */
	JunctionLayout layout = Build({{{0, 2}, Trackdir::X_SW, "RRLLFF"}, {{0, 3}, Trackdir::X_SW, "FFFF"}});
	REQUIRE(layout.size.x == 5);
	REQUIRE(layout.size.y == 4);
	const std::vector<JunctionPort> ports = FindJunctionPorts(layout);
	REQUIRE(ports.size() == 4);

	auto has_curve = [&](const std::vector<RuleViolation> &violations) {
		return std::ranges::any_of(violations, [](const RuleViolation &v) { return v.rule == JunctionRule::CurveSpeed; });
	};
	CHECK(has_curve(CheckJunctionRules(layout, spec)));
	spec.curve_limit = false;
	CHECK_FALSE(has_curve(CheckJunctionRules(layout, spec)));
}

static void SetTracks(JunctionLayout &layout, int x, int y, TrackBits bits)
{
	for (JunctionTrack &jt : layout.tracks) {
		if (jt.offset.x == x && jt.offset.y == y) {
			jt.tracks = bits;
			return;
		}
	}
	layout.tracks.push_back({{static_cast<int16_t>(x), static_cast<int16_t>(y)}, bits});
}

static void RemoveTracks(JunctionLayout &layout, int x, int y)
{
	std::erase_if(layout.tracks, [&](const JunctionTrack &jt) { return jt.offset.x == x && jt.offset.y == y; });
}

/**
 * The ML runs along row 1, the joiner merges at (5,1), and the prio row is on row 2 with joins at x = 2 and 5 (the merge tile).
 * A join is the axis plus Lower on the ML tile and the axis plus Upper on the prio tile (prio on +y of +x traffic).
 * The join on the merge tile also holds the joiner's half piece.
 * With 'extra' there is a third join at x = 0 and a second prio signal.
 *   . . . . - / . . . . . .      the joiner: (4,0) (5,0), merging at (5,1)
 *   - >+ - >- # . >- . . . .     ML signals at x = 1, 3, 7; joins at x = 2 and 5 (the merge tile)
 *   . . + - < + . . . . . .      prio row x = 2..5, Exit signal at (4,2) facing upstream
 *
 * Blocks: {(0,1)}, {(2,1) (2,2) (3,2)}, {(4,1) (5,1) (6,1) (5,2) (4,2)}; the joiner's Entry signal is at (5,0).
 */
static const TrackBits JOIN_ML{Track::X, Track::Lower};
static const TrackBits JOIN_ROW{Track::X, Track::Upper};

/** Add or remove h1 (Lower) on the merge tile (5,1), which also holds the axis and the joiner's half piece. */
static void SetMergeJoin(JunctionLayout &layout, bool join)
{
	for (JunctionTrack &jt : layout.tracks) {
		if (jt.offset.x != 5 || jt.offset.y != 1) continue;
		if (join) {
			jt.tracks.Set(Track::Lower);
		} else {
			jt.tracks.Reset(Track::Lower);
		}
	}
}

static JunctionLayout PrioLayout(bool extra = false)
{
	JunctionLayout layout = Build({{{0, 1}, Trackdir::X_SW, "FFFFFFFFFFF"}, {{4, 0}, Trackdir::X_SW, "LFR"}});
	layout.size.y = 3;
	SetTracks(layout, 2, 1, JOIN_ML);
	SetMergeJoin(layout, true);
	for (int x = 2; x <= 5; x++) SetTracks(layout, x, 2, x == 2 || x == 5 ? JOIN_ROW : TrackBits{Track::X});
	for (int x : {1, 3, 7}) AddSignal(layout, x, 1, Trackdir::X_SW);
	AddSignal(layout, 5, 0, Trackdir::Right_S, SignalType::Entry);
	AddSignal(layout, 4, 2, Trackdir::X_NE, SignalType::Exit);
	if (extra) {
		SetTracks(layout, 0, 1, JOIN_ML);
		SetTracks(layout, 0, 2, JOIN_ROW);
		SetTracks(layout, 1, 2, TrackBits{Track::X});
		AddSignal(layout, 1, 2, Trackdir::X_NE, SignalType::Exit);
		layout.signals[4].type = SignalType::Combo;
	}
	return layout;
}

static std::vector<RuleViolation> PrioViolations(const JunctionLayout &layout, bool joiner_port = true)
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	std::vector<JunctionPort> ports = MergePorts();
	ports[1].tile.x = 11;
	if (!joiner_port) ports.pop_back();
	const JunctionCheckContext ctx = MakeJunctionCheckContext(layout, spec, ports, MERGE_MOVEMENTS);
	return CheckPrio(ctx);
}

static void ChangeSignal(JunctionLayout &layout, int x, int y, Trackdir facing, SignalType type)
{
	for (JunctionSignal &s : layout.signals) {
		if (s.offset.x == x && s.offset.y == y) {
			s.facing = facing;
			s.type = type;
		}
	}
}

TEST_CASE("CheckPrio: a correct prio passes", "[junction][rules]")
{
	CHECK(PrioViolations(PrioLayout()).empty());
	CHECK(PrioViolations(PrioLayout(true)).empty());
}

TEST_CASE("CheckPrio: stray track and missing prio", "[junction][rules]")
{
	JunctionLayout stray = PrioLayout();
	SetTracks(stray, 9, 2, TrackBits{Track::X});
	CHECK(Has(PrioViolations(stray), JunctionRule::Prio, 9, 2, "stray"));

	JunctionLayout missing = PrioLayout();
	for (int x = 2; x <= 5; x++) RemoveTracks(missing, x, 2);
	SetTracks(missing, 2, 1, TrackBits{Track::X});
	SetMergeJoin(missing, false);
	CHECK(Has(PrioViolations(missing), JunctionRule::Prio, 5, 1, "missing prio"));
}

TEST_CASE("CheckPrio: more than one component at the merge", "[junction][rules]")
{
	JunctionLayout layout = PrioLayout();
	SetTracks(layout, 4, 1, TRACK_BIT_CROSS); // An unused Y bit on the ML in the merge block, joined to nothing.
	CHECK(Has(PrioViolations(layout), JunctionRule::Prio, 5, 1, "more than one"));
}

TEST_CASE("CheckPrio: clause 1, shape", "[junction][rules]")
{
	JunctionLayout layout = PrioLayout();
	SetTracks(layout, 4, 2, TRACK_BIT_CROSS); // The row tile's Y leads to a tile outside the row.
	SetTracks(layout, 4, 3, TrackBits{Track::Y});
	layout.size.y = 4;
	const std::vector<RuleViolation> violations = PrioViolations(layout);
	CHECK(Has(violations, JunctionRule::Prio, 4, 3, "single row"));
	CHECK(Has(violations, JunctionRule::Prio, 4, 2, "other than the axis and a join"));
}

TEST_CASE("CheckPrio: clause 2, touches nothing used", "[junction][rules]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	/* A used line starts at (6,2) and runs away from the prio row's end tile (5,2), but its track touches that tile's edge. */
	JunctionLayout layout = PrioLayout();
	JunctionLayout extra = Build({{{6, 2}, Trackdir::X_SW, "FFFFF"}});
	for (const JunctionTrack &jt : extra.tracks) layout.tracks.push_back({{static_cast<int16_t>(jt.offset.x + 6), static_cast<int16_t>(jt.offset.y + 2)}, jt.tracks});
	std::vector<JunctionPort> ports = MergePorts();
	ports[1].tile.x = 11;
	ports.push_back(Port(Role::NIn, 6, 2, Trackdir::X_SW));
	ports.push_back(Port(Role::SOut, 11, 2, Trackdir::X_SW));
	std::vector<Movement> expected = MERGE_MOVEMENTS;
	expected.emplace_back(Role::NIn, Role::SOut);
	const JunctionCheckContext ctx = MakeJunctionCheckContext(layout, spec, ports, expected);
	REQUIRE(ctx.paths.size() == 3);
	CHECK(Has(CheckPrio(ctx), JunctionRule::Prio, 5, 2, "touches a used track"));
}

TEST_CASE("CheckPrio: clause 3, the first join is on the merge tile", "[junction][rules]")
{
	/* The only join in the merge block is upstream of the merge, at (4,1). */
	{
		JunctionLayout layout = PrioLayout();
		SetMergeJoin(layout, false);
		RemoveTracks(layout, 5, 2);
		SetTracks(layout, 4, 1, JOIN_ML);
		SetTracks(layout, 4, 2, JOIN_ROW);
		layout.signals.pop_back();
		AddSignal(layout, 3, 2, Trackdir::X_NE, SignalType::Exit);
		CHECK(Has(PrioViolations(layout), JunctionRule::Prio, 5, 1, "the first prio join must be on the merge tile"));
	}

	/* The first join is one tile after the merge, at (6,1). */
	{
		JunctionLayout layout = PrioLayout();
		SetMergeJoin(layout, false);
		SetTracks(layout, 5, 2, TrackBits{Track::X});
		SetTracks(layout, 6, 1, JOIN_ML);
		SetTracks(layout, 6, 2, JOIN_ROW);
		CHECK(Has(PrioViolations(layout), JunctionRule::Prio, 5, 1, "the first prio join must be on the merge tile"));
	}
}

TEST_CASE("CheckPrio: clause 1, the merge tile join holds nothing else", "[junction][rules]")
{
	JunctionLayout layout = PrioLayout();
	for (JunctionTrack &jt : layout.tracks) {
		if (jt.offset.x == 5 && jt.offset.y == 1) jt.tracks.Set(Track::Upper);
	}
	const std::vector<RuleViolation> violations = PrioViolations(layout);
	CHECK(Has(violations, JunctionRule::Prio, 5, 1, "not a streamlined join"));
	CHECK(Has(violations, JunctionRule::Prio, 5, 1, "the first prio join must be on the merge tile"));
}

TEST_CASE("CheckPrio: clause 4, coverage", "[junction][rules]")
{
	/* The block (2,1) over the train length has no join. */
	{
		JunctionLayout layout = PrioLayout();
		SetTracks(layout, 2, 1, TrackBits{Track::X});
		SetTracks(layout, 2, 2, TrackBits{Track::X});
		CHECK(Has(PrioViolations(layout), JunctionRule::Prio, 2, 1, "exactly one join"));
	}

	/* Two joins in the merge block: (4,1) as well as the merge tile (5,1). */
	{
		JunctionLayout layout = PrioLayout();
		SetTracks(layout, 4, 1, JOIN_ML);
		SetTracks(layout, 4, 2, JOIN_ROW);
		CHECK(Has(PrioViolations(layout), JunctionRule::Prio, 5, 2, "exactly one join"));
	}
}

TEST_CASE("CheckPrio: clause 5, prio signals", "[junction][rules]")
{
	/* Facing the merge, not upstream. */
	{
		JunctionLayout layout = PrioLayout();
		ChangeSignal(layout, 4, 2, Trackdir::X_SW, SignalType::Exit);
		CHECK(Has(PrioViolations(layout), JunctionRule::Prio, 4, 2, "face upstream"));
	}

	/* The only, and so the farthest, signal is Combo. */
	{
		JunctionLayout layout = PrioLayout();
		ChangeSignal(layout, 4, 2, Trackdir::X_NE, SignalType::Combo);
		CHECK(Has(PrioViolations(layout), JunctionRule::Prio, 4, 2, "Exit"));
	}

	/* No signal between the segments. */
	{
		JunctionLayout layout = PrioLayout();
		layout.signals.pop_back();
		CHECK(Has(PrioViolations(layout), JunctionRule::Prio, 5, 1, "exactly one prio signal"));
	}

	/* With three segments, the nearer signal must be Combo. */
	{
		JunctionLayout layout = PrioLayout(true);
		ChangeSignal(layout, 4, 2, Trackdir::X_NE, SignalType::Exit);
		CHECK(Has(PrioViolations(layout), JunctionRule::Prio, 4, 2, "Combo"));
	}
}

TEST_CASE("CheckPrio: clause 6, the joiner's signal", "[junction][rules]")
{
	JunctionLayout wrong_type = PrioLayout();
	ChangeSignal(wrong_type, 5, 0, Trackdir::Right_S, SignalType::Block);
	CHECK(Has(PrioViolations(wrong_type), JunctionRule::Prio, 5, 0, "Entry"));

	JunctionLayout no_signal = PrioLayout();
	RemoveSignal(no_signal, 5, 0);
	CHECK(Has(PrioViolations(no_signal), JunctionRule::Prio, 5, 1, "no joiner signal"));

	/* Without the joiner path there is no ML merge at all, as an unused track never makes one; the prio is then stray. */
	const std::vector<RuleViolation> no_joiner = PrioViolations(PrioLayout(), false);
	CHECK(Has(no_joiner, JunctionRule::Prio, 2, 1, "stray"));
}

TEST_CASE("CheckPrio: clause 1, a join facing forward or an X+Y rung", "[junction][rules]")
{
	/* The ML tile's half piece touches the entry edge, so ML trains could take it. */
	{
		JunctionLayout layout = PrioLayout();
		SetTracks(layout, 2, 1, TrackBits{Track::X, Track::Right});
		CHECK(Has(PrioViolations(layout), JunctionRule::Prio, 2, 1, "not a streamlined join"));
	}

	/* A 90 degree X+Y rung is not a join. */
	{
		JunctionLayout layout = PrioLayout();
		SetTracks(layout, 2, 1, TRACK_BIT_CROSS);
		SetTracks(layout, 2, 2, TRACK_BIT_CROSS);
		CHECK(Has(PrioViolations(layout), JunctionRule::Prio, 2, 1, "must be a streamlined join"));
	}
}

TEST_CASE("CheckPrio: joins for the four traffic and prio side combinations pass", "[junction][rules]")
{
	/* The base layout is +x traffic with the prio on +y; the others are its mirror and 180 degree rotation. */
	const JunctionSpec spec = MakeTestJunctionSpec();
	for (const bool mirrored : {false, true}) {
		for (const uint8_t rotation : {0, 2}) {
			const JunctionLayout base = PrioLayout();
			std::vector<JunctionPort> ports = MergePorts();
			ports[1].tile.x = 11;
			for (JunctionPort &port : ports) {
				if (mirrored) {
					port.tile.x = static_cast<int16_t>(base.size.x - 1 - port.tile.x);
					port.trackdir = TransformTrackdir(port.trackdir, JunctionTransform::Mirror);
				}
				if (rotation == 2) {
					port.tile = {static_cast<int16_t>(base.size.x - 1 - port.tile.x), static_cast<int16_t>(base.size.y - 1 - port.tile.y)};
					for (int i = 0; i < 2; i++) port.trackdir = TransformTrackdir(port.trackdir, JunctionTransform::Rotate);
				}
			}
			const JunctionLayout layout = ApplyJunctionOrientation(base, rotation, mirrored);
			const JunctionCheckContext ctx = MakeJunctionCheckContext(layout, spec, ports, MERGE_MOVEMENTS);
			INFO("mirrored " << mirrored << " rotation " << static_cast<int>(rotation));
			CHECK(ctx.paths.size() == 2);
			CHECK(CheckPrio(ctx).empty());
		}
	}
}
