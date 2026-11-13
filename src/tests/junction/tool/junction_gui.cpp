/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */


/** @file junction_gui.cpp Test functionality from junction_gui. */

#include "../../../stdafx.h"

#include "../../../3rdparty/catch2/catch.hpp"

#include "../../../junction/tool/junction_gui.h"
#include "../../../map_func.h"
#include "../../../track_func.h"

#include "../../../safeguards.h"

/** Set the test junction at TileXY(10, 10) as the ghost. */
static void SetTestGhost(const std::vector<std::optional<BridgeType>> &bridge_types, const JunctionEstimate &estimate)
{
	JunctionGhost::Set(MakeTestJunctionLayout(), TileXY(10, 10), 1, RAILTYPE_RAIL, bridge_types, estimate);
}

TEST_CASE("JunctionGhost: test junction tiles")
{
	Map::Allocate(64, 64);
	JunctionGhost::Clear();
	SetTestGhost({BridgeType{0}}, JunctionEstimate{});

	const JunctionGhostTile *west = JunctionGhost::Get(TileXY(10, 12));
	REQUIRE(west != nullptr);
	CHECK(west->tracks == TrackBits{Track::X});

	const JunctionGhostTile *south = JunctionGhost::Get(TileXY(14, 15));
	REQUIRE(south != nullptr);
	CHECK(south->tracks == TrackBits{Track::Y});

	const JunctionGhostTile *deck = JunctionGhost::Get(TileXY(14, 12));
	REQUIRE(deck != nullptr);
	CHECK(deck->tracks.Test(Track::X));
	REQUIRE(deck->span.has_value());
	CHECK(deck->span->axis == Axis::Y);
	CHECK(deck->span->bridge_z == 2 * TILE_HEIGHT);
	CHECK(deck->span->piece_north == 2);
	CHECK(deck->span->piece_south == 2);
	CHECK(deck->span->type == 0);

	const JunctionGhostTile *span = JunctionGhost::Get(TileXY(14, 11));
	REQUIRE(span != nullptr);
	REQUIRE(span->span.has_value());
	CHECK(span->span->piece_north == 1);
	CHECK(span->span->piece_south == 3);
	CHECK(span->tracks.None());

	const JunctionGhostTile *head_north = JunctionGhost::Get(TileXY(14, 10));
	REQUIRE(head_north != nullptr);
	REQUIRE(head_north->head.has_value());
	CHECK(head_north->head->dir == DiagDirection::SE);

	const JunctionGhostTile *head_south = JunctionGhost::Get(TileXY(14, 14));
	REQUIRE(head_south != nullptr);
	REQUIRE(head_south->head.has_value());
	CHECK(head_south->head->dir == DiagDirection::NW);

	const JunctionGhostTile *signal_sw = JunctionGhost::Get(TileXY(11, 12));
	REQUIRE(signal_sw != nullptr);
	REQUIRE(signal_sw->signals.size() == 1);
	CHECK(signal_sw->signals[0].facing == Trackdir::X_SW);

	const JunctionGhostTile *signal_ne = JunctionGhost::Get(TileXY(16, 12));
	REQUIRE(signal_ne != nullptr);
	REQUIRE(signal_ne->signals.size() == 1);
	CHECK(signal_ne->signals[0].facing == Trackdir::X_NE);

	CHECK(JunctionGhost::Get(TileXY(9, 9)) == nullptr);

	for (uint y = 0; y < 64; y++) {
		for (uint x = 0; x < 64; x++) {
			const JunctionGhostTile *ghost = JunctionGhost::Get(TileXY(x, y));
			if (ghost != nullptr) CHECK(!ghost->bad);
		}
	}

	CHECK(JunctionGhost::GetRailType() == RAILTYPE_RAIL);
}

TEST_CASE("JunctionGhost: bad tiles from the estimate")
{
	Map::Allocate(64, 64);
	JunctionGhost::Clear();

	JunctionEstimate estimate;
	estimate.bad_tiles = {TileXY(12, 12), INVALID_TILE};
	SetTestGhost({BridgeType{0}}, estimate);

	const JunctionGhostTile *bad = JunctionGhost::Get(TileXY(12, 12));
	REQUIRE(bad != nullptr);
	CHECK(bad->bad);

	const JunctionGhostTile *good = JunctionGhost::Get(TileXY(13, 12));
	REQUIRE(good != nullptr);
	CHECK(!good->bad);
}

TEST_CASE("JunctionGhost: bridge without a type")
{
	Map::Allocate(64, 64);
	JunctionGhost::Clear();
	SetTestGhost({std::nullopt}, JunctionEstimate{});

	for (TileIndex tile : {TileXY(14, 10), TileXY(14, 14)}) {
		const JunctionGhostTile *ghost = JunctionGhost::Get(tile);
		REQUIRE(ghost != nullptr);
		CHECK(ghost->bad);
		CHECK(!ghost->head.has_value());
	}

	for (TileIndex tile : {TileXY(14, 11), TileXY(14, 12), TileXY(14, 13)}) {
		const JunctionGhostTile *ghost = JunctionGhost::Get(tile);
		if (ghost != nullptr) CHECK(!ghost->span.has_value());
	}

	const JunctionGhostTile *deck = JunctionGhost::Get(TileXY(14, 12));
	REQUIRE(deck != nullptr);
	CHECK(deck->tracks.Test(Track::X));
}

TEST_CASE("JunctionGhost: clear empties the store")
{
	Map::Allocate(64, 64);
	JunctionGhost::Clear();
	SetTestGhost({BridgeType{0}}, JunctionEstimate{});
	JunctionGhost::Clear();

	CHECK(JunctionGhost::Get(TileXY(10, 12)) == nullptr);
}

TEST_CASE("JunctionGhost: pinned footprint covers the layout's bounding box")
{
	Map::Allocate(64, 64);
	JunctionGhost::Clear();
	SetTestGhost({BridgeType{0}}, JunctionEstimate{});
	JunctionGhost::SetPinned(true);

	CHECK(JunctionGhost::IsInPinnedFootprint(TileXY(10, 10)));
	CHECK(JunctionGhost::IsInPinnedFootprint(TileXY(17, 15)));
	CHECK(!JunctionGhost::IsInPinnedFootprint(TileXY(18, 10)));
	CHECK(!JunctionGhost::IsInPinnedFootprint(TileXY(10, 16)));

	JunctionGhost::SetPinned(false);
	CHECK(!JunctionGhost::IsInPinnedFootprint(TileXY(10, 10)));
}

TEST_CASE("JunctionGhost: clear unpins")
{
	Map::Allocate(64, 64);
	JunctionGhost::Clear();
	SetTestGhost({BridgeType{0}}, JunctionEstimate{});
	JunctionGhost::SetPinned(true);
	JunctionGhost::Clear();

	CHECK(!JunctionGhost::IsInPinnedFootprint(TileXY(10, 10)));
}
