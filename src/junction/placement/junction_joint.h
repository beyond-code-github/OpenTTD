/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_joint.h Junction joint placement model: both axes and every bend length in one integer program (DESIGN-G3 §G3.3). */

#ifndef JUNCTION_JOINT_H
#define JUNCTION_JOINT_H

#include "junction_milp.h"
#include "junction_placement.h"

#include <string>
#include <vector>

struct JunctionSpec;

/** The joint ILP's result (DESIGN-G3 §G3.3). */
struct JunctionJointResult {
	JunctionMilp::Status status;
	std::vector<int> length; ///< Per JunctionPlacementSystem bend, its L.
	std::vector<int> x, y; ///< Per system point, its column and row at the optimum.
	JunctionPlacementChoices choices; ///< The lengths as the fixed path's choices.
	double objective = 0;
	std::vector<std::string> conflict; ///< When infeasible: the reasons of the infeasible set's relation rows, in row order.
};

/**
 * Solve the joint model of a recorded placement system (DESIGN-G3 §G3.3): one integer position per point and axis, one integer length per bend, the relations with their length-dependent constants, and the flat build cost as the objective.
 * @param system The record of the last X and Y rounds of a placement.
 * @param spec The spec the system was recorded with.
 * @param relaxed Whether to add every variable as continuous (the LP relaxation; a scale probe).
 * @return The status, and for Optimal each bend's length and the objective.
 */
JunctionJointResult SolveJunctionJoint(const JunctionPlacementSystem &system, const JunctionSpec &spec, bool relaxed = false);

/** The joint loop's result (DESIGN-G3 §G3.6, R3). */
struct JunctionJointLoopResult {
	JunctionPlacementChoices choices; ///< The last joint solution's lengths as choices.
	JunctionSystemPoint point; ///< The last canonical point.
	int rounds = 0;
	std::string outcome; ///< "settled", or the reason it stopped: "infeasible: <conflict chain>", "solver status <n>", "cycle of period <p>", "round limit".
	std::vector<std::string> log; ///< One line per round: lengths changed, clashes kept, new clashes, the recording's failures.
};

/**
 * The joint loop (DESIGN-G3 §G3.6, R3): record the rules at the choices and the point, solve the joint model with every clash kept so far, take the recording's canonical point (or the joint solution's when the recording's solves fail), and repeat until a round keeps no new clash and changes no length.
 * @param choices The first round's choices (a seed's, or all floors).
 * @param max_rounds The round limit.
 */
JunctionJointLoopResult SolveJunctionJointLoop(const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, const JunctionTrackOrder &order, const JunctionTrackStructure &structure, const JunctionTrackPrograms &programs, const JunctionSpec &spec, const JunctionPlacementChoices &choices, int max_rounds = 12);

/**
 * The joint loop's first choices when no placement exists (DESIGN-G3 §G3.6, R4): every free S-bend 1, every S fan 0, every joiner Corner k, read from a recording of the rounds.
 * @return The choices, or empty choices (no m) when the rounds recorded no bends.
 */
JunctionPlacementChoices JunctionFloorChoices(const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, const JunctionTrackOrder &order, const JunctionTrackStructure &structure, const JunctionTrackPrograms &programs, const JunctionSpec &spec);

#endif /* JUNCTION_JOINT_H */
