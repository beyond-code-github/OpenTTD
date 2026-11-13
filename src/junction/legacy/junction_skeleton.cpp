/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_skeleton.cpp Junction skeleton solver implementation. */

#include "../../stdafx.h"

#include "junction_skeleton.h"

#include <algorithm>
#include <limits>

#include "../../safeguards.h"

namespace {

static constexpr int NO_LOWER = std::numeric_limits<int>::min();
static constexpr int NO_UPPER = std::numeric_limits<int>::max();

/** Rules of the constraints on the cycle that a relaxation of node 'start' lies behind, each once, in cycle order. */
static std::vector<JunctionSkeletonRule> CycleRules(size_t nodes, const std::vector<int> &pred, const std::vector<JunctionSkeletonConstraint> &cs, size_t start)
{
	/* Walk back far enough to be certain to be on the cycle. */
	size_t v = start;
	for (size_t i = 0; i < nodes; i++) v = cs[pred[v]].b;

	std::vector<JunctionSkeletonRule> rules;
	size_t u = v;
	do {
		const JunctionSkeletonConstraint &c = cs[pred[u]];
		if (std::find(rules.begin(), rules.end(), c.rule) == rules.end()) rules.push_back(c.rule);
		u = c.b;
	} while (u != v);
	std::reverse(rules.begin(), rules.end());
	return rules;
}

/** Look for a cycle that makes the constraints infeasible; every node starts at 0 (a virtual source). */
static bool FindCycle(size_t nodes, const std::vector<JunctionSkeletonConstraint> &cs, std::vector<JunctionSkeletonRule> &conflict)
{
	std::vector<int> lo(nodes, 0);
	std::vector<int> pred(nodes, -1);
	for (size_t round = 0; round <= nodes; round++) {
		int changed = -1;
		for (size_t i = 0; i < cs.size(); i++) {
			const JunctionSkeletonConstraint &c = cs[i];
			if (lo[c.b] + c.k > lo[c.a]) {
				lo[c.a] = lo[c.b] + c.k;
				pred[c.a] = static_cast<int>(i);
				changed = c.a;
			}
		}
		if (changed < 0) return false;
		if (round == nodes) {
			conflict = CycleRules(nodes, pred, cs, changed);
			return true;
		}
	}
	return false;
}

/** The bounds of every node from the origin; absent bounds are NO_LOWER and NO_UPPER. Requires feasibility. */
static void Bounds(size_t nodes, const std::vector<JunctionSkeletonConstraint> &cs, std::vector<int> &lo, std::vector<int> &up)
{
	lo.assign(nodes, NO_LOWER);
	up.assign(nodes, NO_UPPER);
	lo[0] = up[0] = 0;
	for (size_t round = 0; round < nodes; round++) {
		bool changed = false;
		for (const JunctionSkeletonConstraint &c : cs) {
			/* x_a >= x_b + k and x_b <= x_a - k. */
			if (lo[c.b] != NO_LOWER && lo[c.b] + c.k > lo[c.a]) {
				lo[c.a] = lo[c.b] + c.k;
				changed = true;
			}
			if (up[c.a] != NO_UPPER && up[c.a] - c.k < up[c.b]) {
				up[c.b] = up[c.a] - c.k;
				changed = true;
			}
		}
		if (!changed) break;
	}
}

}

JunctionSkeletonSolution SolveJunctionSkeleton(size_t variables, const std::vector<std::pair<uint8_t, JunctionSkeletonPush>> &order, const std::vector<JunctionSkeletonConstraint> &constraints)
{
	JunctionSkeletonSolution result;
	const size_t nodes = variables + 1;
	std::vector<JunctionSkeletonConstraint> cs = constraints;
	std::vector<int> value(nodes, 0);
	std::vector<int> lo, up;

	if (FindCycle(nodes, cs, result.conflict)) return result;

	for (const auto &[var, push] : order) {
		Bounds(nodes, cs, lo, up);
		const int bound = push == JunctionSkeletonPush::Down ? lo[var] : up[var];
		if (bound == NO_LOWER || bound == NO_UPPER) {
			result.unbounded = var;
			return result;
		}
		value[var] = bound;
		cs.push_back({var, 0, bound, JunctionSkeletonRule::Extra});
		cs.push_back({0, var, -bound, JunctionSkeletonRule::Extra});
	}

	result.x = std::move(value);
	return result;
}

/** The variables of the W-side builder; 0 is the origin. */
enum SkeletonVar : uint8_t {
	SV_ORIGIN,
	SV_HE_A,
	SV_HE_B,
	SV_HW_A,
	SV_HW_B,
	SV_LS,
	SV_SL,
	SV_LM,
	SV_NW,
	SV_SB,
	SV_COUNT,
};

/** Build and solve the W-side constraints (PLAN-S2 §3). Lane B's heads sit 'stagger' tiles beyond lane A's at both ends (D118), so both bridges have equal length. */
JunctionSkeletonW SolveJunctionSkeletonW(const JunctionSkeletonInputs &in)
{
	using Rule = JunctionSkeletonRule;
	const uint8_t he[2] = {SV_HE_A, SV_HE_B};
	const uint8_t hw[2] = {SV_HW_A, SV_HW_B};

	std::vector<JunctionSkeletonConstraint> cs;
	cs.push_back({SV_SL, SV_LS, in.k_split, Rule::AdjacentSplits});
	for (int l = 0; l < 2; l++) {
		cs.push_back({SV_LS, he[l], in.k_entry[l], Rule::BridgeEntry});
		cs.push_back({he[l], hw[l], 2, Rule::Span});
		cs.push_back({hw[l], he[l], -(in.nmax + 1), Rule::Span});
		cs.push_back({SV_ORIGIN, hw[l], in.k_head - in.zmin[l], Rule::HeadClear});
		cs.push_back({he[l], SV_ORIGIN, in.zmax[l] + in.k_head, Rule::HeadClear});
		cs.push_back({hw[l], SV_LM, in.k_exit[l], Rule::BridgeExit});
	}

	cs.push_back({SV_HE_B, SV_HE_A, in.stagger, Rule::EqualLength});
	cs.push_back({SV_HE_A, SV_HE_B, -in.stagger, Rule::EqualLength});
	cs.push_back({SV_HW_B, SV_HW_A, in.stagger, Rule::EqualLength});
	cs.push_back({SV_HW_A, SV_HW_B, -in.stagger, Rule::EqualLength});

	cs.push_back({SV_LM, SV_NW, in.cover + in.k_w, Rule::PrioCoverage});
	cs.push_back({SV_ORIGIN, SV_NW, -in.x_cap, Rule::NorthWestCap});
	if (in.shift_back) {
		cs.push_back({SV_NW, SV_SB, in.k_sb, Rule::ShiftBack});
		cs.push_back({SV_SB, SV_ORIGIN, in.x_sb_min, Rule::ShiftBack});
	} else {
		cs.push_back({SV_SB, SV_ORIGIN, 0, Rule::ShiftBack});
		cs.push_back({SV_ORIGIN, SV_SB, 0, Rule::ShiftBack});
	}
	cs.insert(cs.end(), in.extra.begin(), in.extra.end());

	using Push = JunctionSkeletonPush;
	const std::vector<std::pair<uint8_t, Push>> order = {
		{SV_HE_A, Push::Down}, {SV_HE_B, Push::Down}, {SV_HW_A, Push::Up}, {SV_HW_B, Push::Up}, {SV_LS, Push::Down},
		{SV_SL, Push::Down}, {SV_LM, Push::Up}, {SV_NW, Push::Up}, {SV_SB, Push::Up},
	};
	const JunctionSkeletonSolution sol = SolveJunctionSkeleton(SV_COUNT - 1, order, cs);

	JunctionSkeletonW w{};
	w.conflict = sol.conflict;
	if (sol.x.empty()) return w;
	w.x_sl = sol.x[SV_SL];
	w.x_ls = sol.x[SV_LS];
	for (int l = 0; l < 2; l++) {
		w.x_he[l] = sol.x[he[l]];
		w.x_hw[l] = sol.x[hw[l]];
	}
	w.x_lm = sol.x[SV_LM];
	w.x_nw = sol.x[SV_NW];
	w.x_sb = sol.x[SV_SB];
	return w;
}
