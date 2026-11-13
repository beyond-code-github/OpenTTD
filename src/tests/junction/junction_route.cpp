/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_route.cpp Test functionality from junction_route. */

#include "../../stdafx.h"

#include "../../3rdparty/catch2/catch.hpp"

#include "../../direction_func.h"
#include "../../junction/junction_route.h"
#include "../../track_func.h"

#include "../../safeguards.h"

TEST_CASE("TrackdirToDirection", "[junction]")
{
	CHECK(TrackdirToDirection(Trackdir::X_NE) == Direction::NE);
	CHECK(TrackdirToDirection(Trackdir::X_SW) == Direction::SW);
	CHECK(TrackdirToDirection(Trackdir::Y_NW) == Direction::NW);
	CHECK(TrackdirToDirection(Trackdir::Y_SE) == Direction::SE);
	CHECK(TrackdirToDirection(Trackdir::Upper_E) == Direction::E);
	CHECK(TrackdirToDirection(Trackdir::Lower_E) == Direction::E);
	CHECK(TrackdirToDirection(Trackdir::Upper_W) == Direction::W);
	CHECK(TrackdirToDirection(Trackdir::Lower_W) == Direction::W);
	CHECK(TrackdirToDirection(Trackdir::Left_N) == Direction::N);
	CHECK(TrackdirToDirection(Trackdir::Right_N) == Direction::N);
	CHECK(TrackdirToDirection(Trackdir::Left_S) == Direction::S);
	CHECK(TrackdirToDirection(Trackdir::Right_S) == Direction::S);
}

static JunctionRaster Raster(Trackdir td, const char *moves)
{
	return RasterizeJunctionRoute({{0, 0}, td, moves});
}

TEST_CASE("RasterizeJunctionRoute: straight and turns", "[junction]")
{
	JunctionRaster r = Raster(Trackdir::X_SW, "FF");
	REQUIRE(r.error.empty());
	REQUIRE(r.pieces.size() == 3);
	for (int i = 0; i < 3; i++) {
		CHECK(r.pieces[i].tile.x == i);
		CHECK(r.pieces[i].tile.y == 0);
		CHECK(r.pieces[i].trackdir == Trackdir::X_SW);
		CHECK(r.pieces[i].units == 2);
		CHECK_FALSE(r.pieces[i].on_bridge);
	}

	for (auto [moves, diff] : {std::pair{"L", DirDiff::Left45}, std::pair{"R", DirDiff::Right45}}) {
		r = Raster(Trackdir::X_SW, moves);
		REQUIRE(r.error.empty());
		REQUIRE(r.pieces.size() == 2);
		CHECK(TrackdirToDirection(r.pieces[1].trackdir) == ChangeDir(Direction::SW, diff));
		CHECK(r.pieces[1].tile.x == 1);
		CHECK(r.pieces[1].tile.y == 0);
		CHECK(r.pieces[1].units == 1);
	}
}

TEST_CASE("RasterizeJunctionRoute: half pieces", "[junction]")
{
	JunctionRaster r = Raster(Trackdir::X_SW, "LFFF");
	REQUIRE(r.error.empty());
	REQUIRE(r.pieces.size() == 5);
	for (size_t i = 1; i < r.pieces.size(); i++) {
		CHECK(r.pieces[i].units == 1);
		CHECK(TrackdirToDirection(r.pieces[i].trackdir) == Direction::S);
		if (i > 1) CHECK(r.pieces[i].trackdir != r.pieces[i - 1].trackdir);
		CHECK((r.pieces[i].tile.x != r.pieces[i - 1].tile.x || r.pieces[i].tile.y != r.pieces[i - 1].tile.y));
	}

	r = Raster(Trackdir::X_SW, "LFR");
	REQUIRE(r.error.empty());
	REQUIRE(r.pieces.size() == 4);
	CHECK(r.pieces[1].trackdir == Trackdir::Right_S);
	CHECK(r.pieces[2].trackdir == Trackdir::Left_S);
	CHECK(r.pieces[3].trackdir == Trackdir::X_SW);

	CHECK_FALSE(Raster(Trackdir::X_SW, "LR").error.empty());
}

TEST_CASE("RasterizeJunctionRoute: bridges and errors", "[junction]")
{
	JunctionRaster r = Raster(Trackdir::X_SW, "B3F");
	REQUIRE(r.error.empty());
	REQUIRE(r.pieces.size() == 7);
	CHECK_FALSE(r.pieces[0].on_bridge);
	for (int x = 1; x <= 5; x++) {
		CHECK(r.pieces[x].on_bridge);
		CHECK(r.pieces[x].tile.x == x);
		CHECK(r.pieces[x].units == 2);
	}
	REQUIRE(r.bridges.size() == 1);
	CHECK(r.bridges[0].start.x == 1);
	CHECK(r.bridges[0].start.y == 0);
	CHECK(r.bridges[0].end.x == 5);
	CHECK(r.bridges[0].end.y == 0);
	CHECK(r.pieces[6].tile.x == 6);
	CHECK(r.pieces[6].tile.y == 0);
	CHECK(r.pieces[6].trackdir == Trackdir::X_SW);
	CHECK_FALSE(r.pieces[6].on_bridge);

	/* Heading NE the far head has the smaller coordinate; start must still be the smaller one. */
	r = Raster(Trackdir::X_NE, "B1");
	REQUIRE(r.error.empty());
	REQUIRE(r.bridges.size() == 1);
	CHECK(r.bridges[0].start.x + r.bridges[0].start.y < r.bridges[0].end.x + r.bridges[0].end.y);

	CHECK_FALSE(Raster(Trackdir::X_SW, "LB2").error.empty());
	CHECK_FALSE(Raster(Trackdir::X_SW, "FX").error.empty());
	CHECK_FALSE(Raster(Trackdir::X_SW, "B").error.empty());
}

TEST_CASE("RasterizeJunctionRoute: a bridge may close an S-bend (D110)", "[junction]")
{
	const JunctionRaster closed = Raster(Trackdir::X_NE, "RFL");
	const JunctionRaster b = Raster(Trackdir::X_NE, "RFB2");
	REQUIRE(closed.error.empty());
	REQUIRE(b.error.empty());
	REQUIRE(b.pieces.size() == 7);
	CHECK(b.pieces[3].tile.x == closed.pieces[3].tile.x);
	CHECK(b.pieces[3].tile.y == closed.pieces[3].tile.y);
	CHECK(b.pieces[3].trackdir == closed.pieces[3].trackdir);
	CHECK(b.pieces[3].on_bridge);
	CHECK(b.pieces[6].tile.x == closed.pieces[3].tile.x - 3);
	CHECK(b.pieces[6].tile.y == closed.pieces[3].tile.y);
	REQUIRE(b.bridges.size() == 1);

	/* Unchanged: a bridge after the closing piece, and after a straight start. */
	const JunctionRaster after = Raster(Trackdir::X_NE, "RFLB2");
	REQUIRE(after.error.empty());
	CHECK(after.pieces[4].tile.x == closed.pieces[3].tile.x - 1);
	CHECK(Raster(Trackdir::X_NE, "FB2").error.empty());
}

TEST_CASE("AddRasterToLayout and NormaliseJunctionLayout", "[junction]")
{
	JunctionLayout layout;
	AddRasterToLayout(layout, Raster(Trackdir::X_SW, "F"));
	AddRasterToLayout(layout, RasterizeJunctionRoute({{1, 0}, Trackdir::Y_SE, ""}));
	REQUIRE(layout.tracks.size() == 2);
	for (const JunctionTrack &jt : layout.tracks) {
		if (jt.offset.x == 1) {
			CHECK(jt.tracks == TrackBits{Track::X, Track::Y});
		} else {
			CHECK(jt.tracks == TrackBits{Track::X});
		}
	}

	JunctionLayout bridged;
	JunctionRaster b = Raster(Trackdir::X_SW, "B1");
	AddRasterToLayout(bridged, b);
	AddRasterToLayout(bridged, b);
	REQUIRE(bridged.tracks.size() == 1); // Only the start piece; bridge tiles add no track.
	CHECK(bridged.bridges.size() == 1);

	JunctionLayout neg;
	neg.tracks.push_back({{-2, -1}, TrackBits{Track::X}});
	neg.tracks.push_back({{1, 3}, TrackBits{Track::Y}});
	neg.signals.push_back({{-2, -1}, Trackdir::X_SW, SignalType::Block, SignalVariant::Electric});
	neg.bridges.push_back({{-2, -1}, {1, -1}});
	NormaliseJunctionLayout(neg);
	CHECK(neg.size.x == 4);
	CHECK(neg.size.y == 5);
	CHECK(neg.tracks[0].offset.x == 0);
	CHECK(neg.tracks[0].offset.y == 0);
	CHECK(neg.tracks[1].offset.x == 3);
	CHECK(neg.tracks[1].offset.y == 4);
	CHECK(neg.signals[0].offset.x == 0);
	CHECK(neg.signals[0].offset.y == 0);
	CHECK(neg.bridges[0].start.x == 0);
	CHECK(neg.bridges[0].end.x == 3);
	CHECK(neg.bridges[0].end.y == 0);
}

TEST_CASE("RenderJunctionAscii", "[junction]")
{
	JunctionLayout layout;
	layout.size = {5, 3};
	layout.bridges.push_back({{0, 0}, {3, 0}});
	layout.tracks.push_back({{4, 0}, TRACK_BIT_CROSS});
	layout.tracks.push_back({{0, 1}, TrackBits{Track::X}});
	layout.tracks.push_back({{1, 1}, TrackBits{Track::Y}});
	layout.tracks.push_back({{2, 1}, TrackBits{Track::Lower}});
	layout.tracks.push_back({{3, 1}, TrackBits{Track::Right}});
	layout.tracks.push_back({{4, 1}, TrackBits{Track::X, Track::Upper}});
	layout.tracks.push_back({{1, 2}, TrackBits{Track::Upper}});
	layout.tracks.push_back({{2, 2}, TrackBits{Track::Left}});
	auto sig = [&](int x, int y, Trackdir td) { layout.signals.push_back({{static_cast<int16_t>(x), static_cast<int16_t>(y)}, td, SignalType::Block, SignalVariant::Electric}); };
	sig(4, 0, Trackdir::X_NE);
	sig(0, 1, Trackdir::X_SW);
	sig(1, 1, Trackdir::Y_NW);
	sig(2, 1, Trackdir::Y_SE);
	sig(3, 1, Trackdir::Left_N);
	sig(4, 1, Trackdir::X_NE);
	sig(4, 1, Trackdir::X_SW);
	sig(0, 2, Trackdir::Left_S);
	sig(1, 2, Trackdir::Upper_E);
	sig(2, 2, Trackdir::Upper_W);

	CHECK(RenderJunctionAscii(layout) ==
		"H = = H +<\n"
		"->|^/v\\n#*\n"
		".s/e\\w. . \n");
}

TEST_CASE("RenderJunctionAscii draws tunnel portals as O", "[junction]")
{
	JunctionLayout layout;
	layout.size = {5, 2};
	layout.bridges.push_back({{0, 0}, {3, 0}, true});
	layout.bridges.push_back({{0, 1}, {3, 1}});
	layout.tracks.push_back({{1, 0}, TrackBits{Track::Y}});
	CHECK(RenderJunctionAscii(layout) ==
		"O | . O . \n"
		"H = = H . \n");
}
