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

#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
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

/** The extra prices and minimum lengths the rule feedback gives a negotiation (DESIGN-O4 revision 3, search step 2); the defaults add nothing. */
struct JunctionNegotiationPrices {
	std::unordered_map<uint64_t, Money> *history = nullptr; ///< History costs carried between negotiations; the negotiation reads and adds to them. When null, a negotiation starts with none.
	std::map<std::pair<int, int>, int> reserved; ///< Reserved tiles (a prio row): any piece of any movement but the one whose index is given costs 1000 x U.
	std::map<std::pair<int, int>, Money> switch_penalties; ///< Added to a piece on the tile that forms a split or merge with another movement's piece there.
	std::vector<uint> min_units; ///< Per movement (in JunctionTMovements() order), the least track units of its route; empty or 0 for none.
};

/** Route every movement of a T together by negotiated congestion (DESIGN-O4 revision 3, search step 1; no prios, signals or rule feedback of its own). */
JunctionNegotiation NegotiateJunctionMovements(const JunctionSpec &spec, const std::vector<JunctionPort> &ports, int width, int height, const JunctionTerrain *terrain = nullptr, int max_rounds = 16, const JunctionNegotiationPrices &prices = {});

/** Counts of the feedback the search applied, per rule (DESIGN-O4 revision 3, step 2). */
struct JunctionFeedbackCounts {
	int prio_rows = 0; ///< Prio rows reserved.
	int coverage_penalties = 0; ///< Coverage windows penalised for switches.
	int bridge_block_penalties = 0; ///< Splits in a bridge's block penalised (D113).
	int split_moves = 0; ///< Splits of waiting-room merges pushed upstream by a penalty.
	int iterations = 0; ///< Feedback iterations run.
};

/** One organic T for a box and branch position (steps 1-4); the layout is set when it is rule-clean and passes the prio timing gate. */
struct JunctionOrganicResult {
	std::optional<JunctionLayout> layout;
	std::vector<RuleViolation> violations; ///< Left after the last iteration when unclean.
	JunctionFeedbackCounts feedback;
	int p = 0;
	JunctionCompactness compactness{};
	Money cost = 0;
	std::string error;
};

JunctionOrganicResult BuildOrganicT(const JunctionSpec &spec, int width, int height, int p, int max_feedback = 8);
/** The best rule-clean organic T over the branch positions tried (step 4). */
JunctionOrganicResult GenerateJunctionOrganic(const JunctionSpec &spec);

#endif /* JUNCTION_ORGANIC_H */
