/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_milp.h Junction mixed-integer linear programs: the one seam to the solver (HiGHS). */

#ifndef JUNCTION_MILP_H
#define JUNCTION_MILP_H

#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <utility>
#include <vector>

/** A mixed-integer linear program and its solve: the junction code's one seam to the solver (HiGHS). */
class JunctionMilp {
public:
	static constexpr double INF = std::numeric_limits<double>::infinity();
	enum class Status : uint8_t {
		Optimal, ///< Solved to proven optimality.
		Infeasible, ///< No solution exists; InfeasibleRows names a conflicting set.
		Error, ///< Anything else (unbounded, solver error, limits).
	};
	JunctionMilp();
	~JunctionMilp();
	uint32_t AddVariable(double lower, double upper, double cost, bool integer); ///< Returns the variable's index.
	uint32_t AddRow(std::span<const std::pair<uint32_t, double>> terms, double lower, double upper); ///< lower <= sum(coef * x) <= upper; returns the row's index.
	Status Solve(); ///< Minimises the objective.
	double Value(uint32_t variable) const; ///< After Optimal.
	double Objective() const; ///< After Optimal.
	std::vector<uint32_t> InfeasibleRows() const; ///< After Infeasible: an irreducible infeasible set of the LP relaxation, sorted.
private:
	struct Impl;
	std::unique_ptr<Impl> impl;
};

#endif /* JUNCTION_MILP_H */
