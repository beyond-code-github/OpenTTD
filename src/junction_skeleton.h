/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_skeleton.h Junction skeleton solver: difference constraints over the W-side positions (PLAN-S2 §3). */

#ifndef JUNCTION_SKELETON_H
#define JUNCTION_SKELETON_H

#include <utility>
#include <vector>

/** Why a skeleton constraint exists (PLAN-S2 §3). */
enum class JunctionSkeletonRule : uint8_t { AdjacentSplits, BridgeEntry, Span, EqualLength, HeadClear, BridgeExit, PrioCoverage, NorthWestCap, ShiftBack, Extra };

/** x[a] - x[b] >= k. Index 0 is the origin, fixed at 0, for bounds. */
struct JunctionSkeletonConstraint {
	uint8_t a;
	uint8_t b;
	int k;
	JunctionSkeletonRule rule;
};

/** Which extreme a variable is fixed at (D144). */
enum class JunctionSkeletonPush : uint8_t { Down, Up };

struct JunctionSkeletonSolution {
	std::vector<int> x; ///< Per variable, index 0 the origin (0); empty when infeasible or unbounded.
	std::vector<JunctionSkeletonRule> conflict; ///< The rules of the constraints on a negative cycle, when infeasible.
	int unbounded = -1; ///< The first variable with no bound in its push direction, or -1.
};

/**
 * Solve difference constraints (D144): variables 1..n are fixed one at a time in 'order', each at its extreme
 * (smallest for Down, largest for Up) consistent with every constraint and the earlier fixes.
 */
JunctionSkeletonSolution SolveJunctionSkeleton(size_t variables, const std::vector<std::pair<uint8_t, JunctionSkeletonPush>> &order, const std::vector<JunctionSkeletonConstraint> &constraints);

/** Inputs of the W-side doubled section (PLAN-S2 §3). x runs along the W row; travel is -x, so upstream is larger x. */
struct JunctionSkeletonInputs {
	int zmin[2], zmax[2]; ///< Crossing set on lane A's row [0] and lane B's row [1].
	int nmax = 10; ///< Largest bridge middle length.
	int cover = 0; ///< Prio coverage in tiles (spec.prio_tiles).
	int x_cap = 0; ///< Largest N->W join x (xI - 2).
	bool shift_back = true; ///< Parallelogram style: has a shift-back x_sb.
	int stagger = 0; ///< Lane B's heads minus lane A's, at both ends (D118: 0 or 1).
	/* Rule offsets: k_split, k_entry, k_exit and k_head are the measured constants of D145; k_w and k_sb are pinned by the caller. */
	int k_split = 2;
	int k_entry[2] = {2, 1};
	int k_exit[2] = {2, 3};
	int k_head = 1;
	int k_w = 1;
	int k_sb = 1;
	int x_sb_min = 2;
	std::vector<JunctionSkeletonConstraint> extra; ///< Learned constraints (S3), added as given.
};

struct JunctionSkeletonW {
	int x_sl, x_ls, x_he[2], x_hw[2], x_lm, x_nw, x_sb; ///< x_sb is 0 when !shift_back.
	std::vector<JunctionSkeletonRule> conflict; ///< Non-empty when infeasible.
};

JunctionSkeletonW SolveJunctionSkeletonW(const JunctionSkeletonInputs &in);

#endif /* JUNCTION_SKELETON_H */
