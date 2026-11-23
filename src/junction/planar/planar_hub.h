/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file planar_hub.h The drawn hub: the contract from Slots to the back end (D452, SPEC-slots-lengths section 4.1). */

#ifndef PLANAR_HUB_H
#define PLANAR_HUB_H

#include "planar_domain.h"
#include "planar_regions.h"
#include "planar_shape_layer.h"
#include "planar_topology.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

/** The drawn hub (D452, SPEC-slots-lengths §4.1): the one hand-off from Slots to Heights, Signals and the judges. Half tiles, x east, y south, every tile >= 0. */
struct PlanarDrawnHub {
	struct Track { std::string name; PlanarTrack::Kind kind; std::vector<std::array<int, 2>> points; };
	struct Node { std::array<int, 2> at; std::vector<uint32_t> in; std::vector<uint32_t> out; bool split; };
	struct Port { PlanarSide side; bool in; std::vector<uint32_t> tracks; };
	struct Structure { uint32_t separator; uint32_t first; uint32_t last; int ends; int limit; bool tunnel; std::vector<uint32_t> crossers; };
	struct Route { std::string name; bool prio; std::vector<std::vector<uint32_t>> paths; };
	std::vector<Track> tracks;
	std::vector<Node> nodes;
	std::vector<Port> ports;
	std::vector<Structure> structures;
	std::vector<Route> routes;
};

/** A v2 drawing (points per track of the variant's track map) as a drawn hub; empty hub and why on failure. */
PlanarDrawnHub PlanarDrawnHubOf(const PlanarShapeVariant &v, const std::vector<std::vector<std::array<int, 2>>> &points, std::vector<std::string> &notes, std::string &why);

/** The contract's oracle (§4.1 promises); one line per broken promise, empty when it holds. */
std::vector<std::string> CheckPlanarDrawnHub(const PlanarDrawnHub &hub);

/** The hub's domain input (a projection: tracks' points, nodes, structures). */
PlanarDomainInput PlanarDomainInputOfHub(const PlanarDrawnHub &hub);

#endif /* PLANAR_HUB_H */
