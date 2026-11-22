/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file planar_domain_map.h A drawing of a shaped variant as the domain's input (D428): nodes and structures read from the track map, never from the drawing program's rows. */

#ifndef PLANAR_DOMAIN_MAP_H
#define PLANAR_DOMAIN_MAP_H

#include "planar_domain.h"
#include "planar_shape_layer.h"

#include <array>
#include <string>
#include <vector>

/**
 * The domain input of a drawing.
 * @param v The shaped variant the drawing belongs to.
 * @param points Per track of v's map, its points (as PlanarDrawResult::points).
 * @param notes Receives one line per structure left out, and why.
 * @return The input: every track's points as given; the nodes; the structures.
 */
PlanarDomainInput PlanarDomainInputOf(const PlanarShapeVariant &v, const std::vector<std::vector<std::array<int, 2>>> &points, std::vector<std::string> &notes);

#endif /* PLANAR_DOMAIN_MAP_H */
