/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file planar_shape.h The shape program: the least-turning channel shapes that obey the crossing rules. */

#ifndef PLANAR_SHAPE_H
#define PLANAR_SHAPE_H

#include "planar_paths.h"
#include "planar_regions.h"

#include <string>
#include <vector>

std::vector<PlanarChannelPath> SolvePlanarShape(const PlanarTopology &t, const PlanarChannels &channels, const PlanarLineSides &sides, const std::vector<uint32_t> &over, int *turns, std::string *why);
int PlanarShapeTurns(const std::vector<PlanarChannelPath> &paths);

#endif /* PLANAR_SHAPE_H */
