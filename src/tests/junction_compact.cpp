/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_compact.cpp Test functionality from junction_compact. */

#include "../stdafx.h"

#include "../3rdparty/catch2/catch.hpp"

#include "../junction_compact.h"
#include "../junction_generator.h"
#include "../track_func.h"

#include "../safeguards.h"

static void AddLineX(JunctionLayout &layout, int16_t y, int16_t x0, int16_t x1)
{
	for (int16_t x = x0; x <= x1; x++) layout.tracks.push_back({{x, y}, TrackBits{Track::X}});
}

static void AddLineY(JunctionLayout &layout, int16_t x, int16_t y0, int16_t y1)
{
	for (int16_t y = y0; y <= y1; y++) layout.tracks.push_back({{x, y}, TrackBits{Track::Y}});
}

TEST_CASE("Compactness: parallel lines with a 1-tile gap", "[junction][compact]")
{
	JunctionLayout layout;
	layout.size = {10, 3};
	AddLineX(layout, 0, 0, 9);
	AddLineX(layout, 2, 0, 9);
	const JunctionCompactness c = MeasureJunctionCompactness(layout);
	CHECK(c.occupied == 20);
	CHECK(c.slivers == 10);
	CHECK(c.enclosed == 0);
}

TEST_CASE("Compactness: parallel lines with a 2-tile gap", "[junction][compact]")
{
	JunctionLayout layout;
	layout.size = {10, 4};
	AddLineX(layout, 0, 0, 9);
	AddLineX(layout, 3, 0, 9);
	const JunctionCompactness c = MeasureJunctionCompactness(layout);
	CHECK(c.occupied == 20);
	CHECK(c.slivers == 0);
	CHECK(c.enclosed == 0);
}

TEST_CASE("Compactness: closed ring encloses its interior", "[junction][compact]")
{
	JunctionLayout layout;
	layout.size = {6, 6};
	AddLineX(layout, 0, 0, 5);
	AddLineX(layout, 5, 0, 5);
	AddLineY(layout, 0, 1, 4);
	AddLineY(layout, 5, 1, 4);
	const JunctionCompactness c = MeasureJunctionCompactness(layout);
	CHECK(c.occupied == 20);
	CHECK(c.enclosed == 16);
}

TEST_CASE("Compactness: tiles under a bridge span are not occupied", "[junction][compact]")
{
	JunctionLayout layout;
	layout.size = {7, 5};
	layout.tracks.push_back({{1, 2}, TrackBits{Track::X}});
	layout.tracks.push_back({{5, 2}, TrackBits{Track::X}});
	layout.bridges.push_back({{1, 2}, {5, 2}});
	AddLineY(layout, 3, 0, 4);
	const JunctionCompactness c = MeasureJunctionCompactness(layout);
	CHECK(c.occupied == 7); // Two heads plus the crossing line; the span tiles (2,2) and (4,2) are free, (3,2) holds the crossing track.
	CHECK(c.enclosed == 0);
}

TEST_CASE("Compactness: today's generator layout", "[junction][compact]")
{
	const JunctionGenerateResult result = GenerateJunction(MakeTestJunctionSpec());
	REQUIRE(result.layout.has_value());
	const JunctionCompactness c = MeasureJunctionCompactness(*result.layout);
	CHECK(c.occupied == 190);
	CHECK(c.enclosed == 34);
	CHECK(c.slivers == 53);
}
