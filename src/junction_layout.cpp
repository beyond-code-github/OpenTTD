/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_layout.cpp Junction layout description, build steps and price estimate. */

#include "stdafx.h"

#include "junction_layout.h"

#include "safeguards.h"

/**
 * Transform a junction layout by a rotation or a mirror.
 * @param layout The layout to transform.
 * @param transform The transformation to apply.
 * @return The transformed layout.
 */
JunctionLayout TransformJunctionLayout(const JunctionLayout &layout, [[maybe_unused]] JunctionTransform transform)
{
	return layout;
}

/**
 * Make the fixed test layout that exercises every kind of piece.
 * @return The test layout.
 */
JunctionLayout MakeTestJunctionLayout()
{
	return {};
}

/**
 * Make the build steps for placing a layout on the map.
 * @param layout The layout to place.
 * @param origin The tile for the north corner of the layout.
 * @return The steps in build order.
 */
std::vector<JunctionBuildStep> MakeJunctionBuildSteps([[maybe_unused]] const JunctionLayout &layout, [[maybe_unused]] TileIndex origin)
{
	return {};
}

/**
 * Estimate the price and validity of placing a layout.
 * @param layout The layout to place.
 * @param origin The tile for the north corner of the layout.
 * @param pricing Source of tile and price information.
 * @return The estimate.
 */
JunctionEstimate EstimateJunction([[maybe_unused]] const JunctionLayout &layout, [[maybe_unused]] TileIndex origin, [[maybe_unused]] const JunctionPricing &pricing)
{
	return {};
}
