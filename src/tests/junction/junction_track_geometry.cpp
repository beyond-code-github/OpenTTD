/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_track_geometry.cpp Test functionality from junction_track_geometry. */

#include "../../stdafx.h"

#include "../../3rdparty/catch2/catch.hpp"

#include "../../junction/junction_track_geometry.h"

#include "../../safeguards.h"

TEST_CASE("unrelated pieces share a tile only as a parallel diagonal pair", "[junction][geometry]")
{
	CHECK(JunctionTracksMayShareTile(TrackBits{Track::Upper}, TrackBits{Track::Lower}));
	CHECK(JunctionTracksMayShareTile(TrackBits{Track::Lower}, TrackBits{Track::Upper}));
	CHECK(JunctionTracksMayShareTile(TrackBits{Track::Left}, TrackBits{Track::Right}));
	CHECK(JunctionTracksMayShareTile(TrackBits{Track::Right}, TrackBits{Track::Left}));
	CHECK_FALSE(JunctionTracksMayShareTile(TrackBits{Track::X}, TrackBits{Track::Y}));
	CHECK_FALSE(JunctionTracksMayShareTile(TrackBits{Track::Y}, TrackBits{Track::X}));
	CHECK_FALSE(JunctionTracksMayShareTile(TrackBits{Track::X}, TrackBits{Track::Upper}));
	CHECK_FALSE(JunctionTracksMayShareTile(TrackBits{Track::Upper}, TrackBits{Track::Left}));
	CHECK_FALSE(JunctionTracksMayShareTile(TrackBits{Track::X}, TrackBits{Track::X}));
}
