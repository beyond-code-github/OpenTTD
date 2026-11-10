/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_organic.h Junction movements routed together by negotiated congestion (DESIGN-O4 revision 3). */

#ifndef JUNCTION_ORGANIC_H
#define JUNCTION_ORGANIC_H

#include "junction_compact.h"
#include "junction_generator.h"
#include "junction_layout.h"
#include "junction_line.h"
#include "junction_rules.h"
#include "junction_terrain.h"

#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

/** Ports of a T on a box of width x height (DESIGN-O4 revision 3): main-line tracks on rows top and top + ml_gap + 1, branch tracks on the bottom row from x = p. */
std::vector<JunctionPort> MakeOrganicTPorts(const JunctionSpec &spec, int width, int height, int p, int top = 0);

/** One candidate structure of the T (DESIGN-O4 revision 5): the forced crossings' over/under, and where each prio window may end. */
struct JunctionTMap {
	std::vector<std::pair<int, int>> over; ///< Forced pairs (over, under), as JunctionNegotiationPrices::over.
	struct Window { int line; int joiner; int8_t side; int end_lo; int end_hi; }; ///< Movement indices (JunctionTMovements() order).
	std::vector<Window> windows; ///< One per merge into a main line.
	int bound = 0; ///< Admissible lower bound on occupied tiles, for ordering.
};

/** The outcome of routing a junction's movements together. */
struct JunctionNegotiation {
	std::optional<JunctionLayout> layout; ///< Set when the negotiation converged.
	std::vector<JunctionLineRoute> routes; ///< Each movement's final route, in the expected order.
	int rounds = 0; ///< Rounds run.
	int routes_run = 0; ///< Router calls made.
	std::vector<int> conflicts_per_round; ///< Conflicting piece pairs after each round.
	int conflicts = 0; ///< Conflicting piece pairs after the last round (0 when converged).
	std::string error; ///< Why it failed, when it did.
	uint max_expansions = 0; ///< The most nodes any router call expanded.
};

/** The extra prices and minimum lengths a negotiation takes (DESIGN-O4 revision 4); the defaults add nothing. */
struct JunctionNegotiationPrices {
	std::unordered_map<uint64_t, Money> *history = nullptr; ///< History costs carried between negotiations; the negotiation reads and adds to them. When null, a negotiation starts with none.
	std::vector<uint> min_units; ///< Per movement (in JunctionTMovements() order), the least track units of its route; empty or 0 for none.
	std::vector<std::pair<int, int>> over; ///< Forced pairs (over, under) by movement index (D167).
	Money ml_turn_cost = 0; ///< Turn cost of the two main-line through movements (D165); 0 means the branch movements' turn cost.
	const JunctionTMap *map = nullptr; ///< When set, the structure to project (DESIGN-O4 revision 5): its over/under replaces `over`, and each main line carries its prio window.
	bool apply_rules = true; ///< When false, no movement rules, no masks and no windows (the O4b1 negotiation).
};

/** The pairs of movements of a T that share no port and whose ports interleave around the box boundary, by movement index (a < b): they must cross (D167). */
std::vector<std::pair<int, int>> JunctionForcedPairs(const std::vector<JunctionPort> &ports, int width, int height);

/** The candidate structures of the T for a box and ports, cheapest bound first. */
std::vector<JunctionTMap> JunctionTMaps(const JunctionSpec &spec, const std::vector<JunctionPort> &ports, int width, int height);

/** Route every movement of a T together by negotiated congestion, the turning movements under the split and merge rules (DESIGN-O4 revision 4, D168). */
JunctionNegotiation NegotiateJunctionMovements(const JunctionSpec &spec, const std::vector<JunctionPort> &ports, int width, int height, const JunctionTerrain *terrain = nullptr, int max_rounds = 16, const JunctionNegotiationPrices &prices = {});

/** One organic T for a box and branch position (DESIGN-O4 revision 4); the layout is set when it is rule-clean and passes the prio timing gate. */
struct JunctionOrganicResult {
	std::optional<JunctionLayout> layout;
	std::vector<RuleViolation> violations; ///< Left after the last case when unclean.
	int p = 0;
	JunctionCompactness compactness{};
	Money cost = 0;
	std::string cases; ///< One line per candidate map tried, the winning one marked with '*' (DESIGN-O4 revision 5).
	std::string error;
	int64_t micros = 0; ///< Wall time of the generation.
	int rounds = 0; ///< Rounds of the winning case's negotiation.
	uint max_expansions = 0; ///< The most nodes any router call expanded, over all attempts.
};

JunctionOrganicResult BuildOrganicT(const JunctionSpec &spec, int width, int height, int p, Money ml_turn_cost = 0);
/** The best rule-clean organic T over the branch positions tried (step 4). */
JunctionOrganicResult GenerateJunctionOrganic(const JunctionSpec &spec, Money ml_turn_cost = 0);

#endif /* JUNCTION_ORGANIC_H */
