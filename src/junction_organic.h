/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_organic.h Junction movements routed together by negotiated congestion (DESIGN-O4 revision 3). */

#ifndef JUNCTION_ORGANIC_H
#define JUNCTION_ORGANIC_H

#include "junction_generator.h"
#include "junction_layout.h"
#include "junction_line.h"
#include "junction_rules.h"
#include "junction_terrain.h"

#include <optional>
#include <string>
#include <vector>

/** Ports of a T on a box of width x height (DESIGN-O4 revision 3): main-line tracks on rows 0 and ml_gap + 1, branch tracks on the bottom row from x = p. */
std::vector<JunctionPort> MakeOrganicTPorts(const JunctionSpec &spec, int width, int height, int p);

/** The outcome of routing a junction's movements together. */
struct JunctionNegotiation {
	std::optional<JunctionLayout> layout; ///< Set when the negotiation converged.
	std::vector<JunctionLineRoute> routes; ///< Each movement's final route, in the expected order.
	int rounds = 0; ///< Rounds run.
	int routes_run = 0; ///< Router calls made.
	std::vector<int> conflicts_per_round; ///< Conflicting piece pairs after each round.
	int conflicts = 0; ///< Conflicting piece pairs after the last round (0 when converged).
	std::string error; ///< Why it failed, when it did.
};

/** Route every movement of a T together by negotiated congestion (DESIGN-O4 revision 3, search step 1; no prios, signals or rule feedback). */
JunctionNegotiation NegotiateJunctionMovements(const JunctionSpec &spec, const std::vector<JunctionPort> &ports, int width, int height, const JunctionTerrain *terrain = nullptr, int max_rounds = 16);

#endif /* JUNCTION_ORGANIC_H */
