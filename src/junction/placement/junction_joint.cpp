/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_joint.cpp Junction joint placement model implementation: the port of the G3P prototype's solve.py. */

#include "../../stdafx.h"

#include "junction_joint.h"

#include "../junction_spec.h"
#include "../topology/junction_chain.h"

#include <cmath>
#include <map>
#include <set>

#include "../../safeguards.h"

static constexpr int VX[] = {0, 1, 0, -1};
static constexpr int VY[] = {-1, 0, 1, 0};
static constexpr double BIG = 100000; ///< The big-M of the S fans' zero-size branch (coordinates stay far below it).
static constexpr double EPS = 1e-3; ///< The tie break: EPS * sum L (the costs are multiples of 50, the sum of L stays below 1000).

/** The terms of a row or an objective: variable to coefficient. */
using Coefs = std::map<uint32_t, double>;

/** The model, kept until it is passed to the solver: the solver takes a variable's cost when it is added, the cost is known only after the rows. */
struct JointModel {
	std::vector<double> lo, hi; ///< Variable bounds.
	std::vector<bool> integer; ///< Whether each variable is integer.
	std::vector<double> cost; ///< Variable objective coefficients.
	struct Row {
		double lo, hi;
		Coefs coefs;
		int relation; ///< The index of the JunctionPlacementSystem relation it is, -1 when none.
	};
	std::vector<Row> rows;

	uint32_t Var(double lower = 0, double upper = JunctionMilp::INF, bool is_integer = true)
	{
		this->lo.push_back(lower);
		this->hi.push_back(upper);
		this->integer.push_back(is_integer);
		this->cost.push_back(0);
		return static_cast<uint32_t>(this->lo.size() - 1);
	}

	void AddRow(const Coefs &coefs, double lower, double upper, int relation = -1)
	{
		Row row{lower, upper, {}, relation};
		for (const auto &[column, coefficient] : coefs) {
			if (coefficient != 0) row.coefs[column] = coefficient;
		}
		this->rows.push_back(std::move(row));
	}

	void AddCost(const Coefs &expr, double weight)
	{
		for (const auto &[column, coefficient] : expr) this->cost[column] += weight * coefficient;
	}
};

/** The side a bend turns to: (hin + hand + 4) % 4. */
static int SideOf(const JunctionSystemBend &bend)
{
	return (bend.hin + bend.hand + 4) % 4;
}

/** A linear expression: its terms and its constant. */
struct Expr {
	Coefs coefs;
	double constant = 0;
};

/**
 * The curve gaps as unit counters (DESIGN-G3 §G3.3a item 1): per route, the units E_i at each bend's entry tile as an integer variable, and each pair of JunctionCurveGaps as one row on the difference of two curves' units.
 * @param model The model to add the rows to.
 * @param system The record, with its routes.
 * @param spec The spec the system was recorded with.
 * @param x The X variable of every point.
 * @param y The Y variable of every point.
 * @param L The length variable of every bend.
 */
static void AddCurveGapCounters(JointModel &model, const JunctionPlacementSystem &system, const JunctionSpec &spec, const std::vector<uint32_t> &x, const std::vector<uint32_t> &y, const std::map<int, uint32_t> &L)
{
	for (const JunctionSystemRoute &route : system.routes) {
		const size_t n = route.bends.size();
		std::vector<uint32_t> entry;
		for (size_t i = 0; i < n; i++) entry.push_back(model.Var(-JunctionMilp::INF, JunctionMilp::INF));

		/* scale * (a + b L) of one raster quantity (q at L = 1, q at L = 2): the quantity is the constant a - b + ... of its affine form. */
		auto add_quantity = [&](Expr &e, double scale, const JunctionSystemRouteBend &bend, const std::pair<int, int> &q) {
			const int slope = q.second - q.first;
			if (slope != 0) e.coefs[L.at(bend.bend)] += scale * slope;
			e.constant += scale * (q.first - slope);
		};
		/* scale * h . (the bend's quantity pair at index dx). */
		auto add_offset = [&](Expr &e, double scale, int heading, const JunctionSystemRouteBend &bend, size_t dx) {
			if (VX[heading] != 0) add_quantity(e, scale * VX[heading], bend, bend.quantity[dx]);
			if (VY[heading] != 0) add_quantity(e, scale * VY[heading], bend, bend.quantity[dx + 1]);
		};
		/* scale * h . point. */
		auto add_point = [&](Expr &e, double scale, int heading, int pt) {
			e.coefs[x[pt]] += scale * VX[heading];
			e.coefs[y[pt]] += scale * VY[heading];
		};
		auto add_row = [&](const Expr &e, double lower, double upper) {
			model.AddRow(e.coefs, lower - e.constant, upper - e.constant);
		};

		for (size_t i = 0; i < n; i++) {
			const JunctionSystemRouteBend &bend = route.bends[i];
			Expr e;
			e.coefs[entry[i]] += 1;
			if (i == 0) {
				/* E_0 = 2 h_start . ((f_0 + before_0) - start) */
				add_point(e, -2, route.start_heading, bend.pt);
				add_point(e, 2, route.start_heading, route.start_pt);
				add_offset(e, -2, route.start_heading, bend, 0);
			} else {
				/* E_i = E_{i-1} + close_units_{i-1} + 2 h_{i-1} . ((f_i + before_i) - (f_{i-1} + close_{i-1})) */
				const JunctionSystemRouteBend &prev = route.bends[i - 1];
				e.coefs[entry[i - 1]] -= 1;
				add_quantity(e, -1, prev, prev.quantity[4]);
				add_point(e, -2, prev.hout, bend.pt);
				add_point(e, 2, prev.hout, prev.pt);
				add_offset(e, -2, prev.hout, bend, 0);
				add_offset(e, 2, prev.hout, prev, 2);
			}
			add_row(e, 0, 0);
		}

		/* The curves in travel order: (bend, curve of the bend). */
		std::vector<std::pair<size_t, size_t>> curves;
		for (size_t i = 0; i < n; i++) {
			for (size_t k = 0; k < route.bends[i].curve_units.size(); k++) curves.emplace_back(i, k);
		}
		assert(curves.size() == route.hands.size());
		std::vector<JunctionCurveHand> hands;
		for (const int8_t hand : route.hands) hands.push_back(hand > 0 ? JunctionCurveHand::Right : JunctionCurveHand::Left);
		for (const JunctionCurveGap &gap : JunctionCurveGaps(hands, spec.tl, spec.curve_speed, spec.reference_speed)) {
			/* U_j - U_i >= min_gap, with U = E_bend + cu_k. */
			Expr e;
			const auto [bi, ki] = curves[gap.i];
			const auto [bj, kj] = curves[gap.j];
			e.coefs[entry[bj]] += 1;
			e.coefs[entry[bi]] -= 1;
			add_quantity(e, 1, route.bends[bj], route.bends[bj].curve_units[kj]);
			add_quantity(e, -1, route.bends[bi], route.bends[bi].curve_units[ki]);
			add_row(e, gap.min_gap, JunctionMilp::INF);
		}
	}
}

JunctionJointResult SolveJunctionJoint(const JunctionPlacementSystem &system, const JunctionSpec &spec)
{
	JunctionJointResult result;
	result.status = JunctionMilp::Status::Error;
	JointModel model;
	const uint32_t npts = system.pt_count;
	std::vector<uint32_t> x, y;
	for (uint32_t p = 0; p < npts; p++) x.push_back(model.Var());
	for (uint32_t p = 0; p < npts; p++) y.push_back(model.Var());
	const std::vector<uint32_t> *coord[2] = {&x, &y};
	model.AddRow({{x[0], 1}}, 0, 0); // origin x
	model.AddRow({{y[0], 1}}, 0, 0); // origin y
	const int k = system.k;
	std::map<int, uint32_t> L, Z;
	for (const JunctionSystemBend &b : system.bends) {
		const std::string &src = b.source;
		if (src == "free") {
			if (b.kind == "SFan") {
				L[b.id] = model.Var(0);
				Z[b.id] = model.Var(0, 1);
				model.AddRow({{L[b.id], 1}, {Z[b.id], -1}}, 0, JunctionMilp::INF); // fan: L >= z
				model.AddRow({{L[b.id], 1}, {Z[b.id], -BIG}}, -JunctionMilp::INF, 0); // fan: L <= M z
			} else {
				L[b.id] = model.Var(b.floor);
			}
		} else if (src == "corner_rows") {
			L[b.id] = model.Var(k);
		} else if (src == "rows") {
			L[b.id] = model.Var(0);
		} else { // port_rows, fixed_k, one_row: held at the recorded size
			L[b.id] = model.Var(b.m, b.m);
		}
	}

	/* The recorded relations: coord(v) - coord(u) - a L >= b (tagged) or >= c. */
	std::set<int> tagged_y;
	for (size_t r = 0; r < system.relations.size(); r++) {
		const JunctionSystemRelation &relation = system.relations[r];
		if (relation.nonlinear && !system.routes.empty() && relation.reason.starts_with("curve gap")) continue; // Replaced by the unit counters below.
		if (relation.order_pin) continue; // The hug's pin to the flat solve's order: the span cost pulls the heads tight instead.
		const std::vector<uint32_t> &axis_coord = *coord[relation.axis];
		Coefs coefs;
		coefs[axis_coord[relation.v]] += 1;
		coefs[axis_coord[relation.u]] -= 1; // v == u: a constant row
		const int bid = relation.bend;
		if (relation.zero_branch && bid >= 0 && Z.count(bid) != 0) {
			coefs[Z[bid]] = BIG; // only when the fan is empty
			model.AddRow(coefs, relation.c, JunctionMilp::INF, static_cast<int>(r));
		} else if (!relation.nonlinear && !relation.zero_branch && bid >= 0) {
			coefs[L[bid]] -= relation.a;
			double lower = relation.b;
			if (Z.count(bid) != 0) { // an S fan's sized branch: only when the fan is not empty
				coefs[Z[bid]] = -BIG;
				lower = relation.b - BIG;
			}
			if (relation.axis == 1) tagged_y.insert(bid);
			model.AddRow(coefs, lower, JunctionMilp::INF, static_cast<int>(r));
		} else {
			model.AddRow(coefs, relation.c, JunctionMilp::INF, static_cast<int>(r));
		}
	}

	AddCurveGapCounters(model, system, spec, x, y, L);

	/* Link rows per diagonal. */
	for (const JunctionSystemBend &b : system.bends) {
		const int i = b.id;
		if (b.source == "free") {
			const int s = VX[SideOf(b)]; // The S-bend size block of the placement: side = (hin + hand + 4) % 4, x sign VX[side].
			const int sv = VY[b.hin]; // y sign VY[hin], rows m - 1 (S-bend size)
			model.AddRow({{x[b.last], 1}, {x[b.first], -1}, {L[i], -s}}, 0, 0);
			if (tagged_y.count(i) == 0) {
				if (Z.count(i) != 0) {
					model.AddRow({{y[b.last], 1}, {y[b.first], -1}, {L[i], -sv}, {Z[i], sv}}, 0, 0);
				} else {
					model.AddRow({{y[b.last], 1}, {y[b.first], -1}, {L[i], -sv}}, -sv, -sv);
				}
			}
		} else if (b.source == "corner_rows") {
			/* The joiner Corner's span is y(first) - y(last) (refresh and the X size: |vy first - vy last|, floor ge(first, last, k)). */
			model.AddRow({{y[b.first], 1}, {y[b.last], -1}, {L[i], -1}}, 0, 0);
		} else if (b.source == "rows") {
			/* A horizontal S-bend's size is |y(item) - y(first)| (refresh); its sign is the side's VY. */
			const int sv = VY[SideOf(b)];
			model.AddRow({{y[b.item_pt], 1}, {y[b.first], -1}, {L[i], -sv}}, 0, 0);
		}
	}

	/* Objective: the flat build cost. */
	const double rail = static_cast<double>(system.rail_piece);
	const double signal = static_cast<double>(system.signal);
	const int sd = system.sd;
	const double bridge_a = static_cast<double>(system.bridge1 - system.bridge0);
	std::map<std::pair<int, int>, Coefs> chain_len; // (edge, copy) -> linear expression of its track pieces (signals)
	const std::vector<JunctionSystemElement> &elements = system.chains;
	for (size_t i = 0; i < elements.size();) {
		size_t end = i;
		while (end < elements.size() && elements[end].edge == elements[i].edge && elements[end].copy == elements[i].copy) end++;
		const int edge = elements[i].edge, copy = elements[i].copy;
		Coefs expr;
		for (size_t j = i; j < end; j++) {
			const JunctionSystemElement &el = elements[j];
			if (el.bend >= 0) expr[L[el.bend]] += 2; // 2 L half pieces (2 L + 1 for a Corner: the constant is dropped)
			if ((el.k == "Heads" || el.k == "HeadsPair") && el.first != el.last) {
				int axis = VX[el.hin] != 0 ? 0 : 1;
				if (el.k == "HeadsPair") axis = 0;
				const uint32_t t = model.Var(0, JunctionMilp::INF, false);
				const uint32_t last = (*coord[axis])[el.last], first = (*coord[axis])[el.first];
				model.AddRow({{t, 1}, {last, -1}, {first, 1}}, 0, JunctionMilp::INF); // cost: span
				model.AddRow({{t, 1}, {last, 1}, {first, -1}}, 0, JunctionMilp::INF); // cost: span
				if (!elements[i].tunnel) model.AddCost({{t, 1}}, bridge_a); // middle tiles = span - 1; the constant is dropped; a tunnel has no price in the spec
			}
			if (j + 1 >= end) continue;
			const JunctionSystemElement &nx = elements[j + 1];
			if (nx.k == "Prio" || el.k == "Prio" || nx.first == el.last) continue;
			const int axis = VX[el.hout] != 0 ? 0 : 1;
			const uint32_t t = model.Var(0, JunctionMilp::INF, false);
			const uint32_t next_first = (*coord[axis])[nx.first], last = (*coord[axis])[el.last];
			model.AddRow({{t, 1}, {next_first, -1}, {last, 1}}, 0, JunctionMilp::INF); // cost: run
			model.AddRow({{t, 1}, {next_first, 1}, {last, -1}}, 0, JunctionMilp::INF); // cost: run
			expr[t] += 1;
		}
		chain_len[{edge, copy}] = expr;
		model.AddCost(expr, rail);
		i = end;
	}
	std::set<int> doubled;
	for (const JunctionSystemElement &el : elements) {
		if (el.copy == 1) doubled.insert(el.edge);
	}
	for (const std::vector<int> &path : system.paths) {
		bool any_doubled = false;
		for (int e : path) any_doubled |= doubled.count(e) != 0;
		for (int c = 0; c < (any_doubled ? 2 : 1); c++) {
			Coefs length;
			for (int e : path) {
				const auto found = chain_len.find({e, doubled.count(e) != 0 ? c : 0});
				if (found == chain_len.end()) continue;
				for (const auto &[column, a] : found->second) length[column] += a;
			}
			const uint32_t n = model.Var(0);
			/* n = floor(len / sd): sd n >= len - (sd - 1), minimised. */
			Coefs row{{n, static_cast<double>(sd)}};
			for (const auto &[column, a] : length) row[column] = -a;
			model.AddRow(row, -(sd - 1), JunctionMilp::INF); // cost: signals
			model.AddCost({{n, 1}}, signal);
		}
	}
	for (const auto &[id, column] : L) model.cost[column] += EPS;

	JunctionMilp milp;
	for (size_t i = 0; i < model.lo.size(); i++) milp.AddVariable(model.lo[i], model.hi[i], model.cost[i], model.integer[i]);
	std::vector<std::pair<uint32_t, double>> terms;
	for (const JointModel::Row &row : model.rows) {
		terms.assign(row.coefs.begin(), row.coefs.end());
		milp.AddRow(terms, row.lo, row.hi);
	}
	result.status = milp.Solve();
	if (result.status == JunctionMilp::Status::Optimal) {
		result.objective = milp.Objective();
		for (const JunctionSystemBend &b : system.bends) result.length.push_back(static_cast<int>(std::lround(milp.Value(L[b.id]))));
	} else if (result.status == JunctionMilp::Status::Infeasible) {
		for (uint32_t row : milp.InfeasibleRows()) {
			if (model.rows[row].relation >= 0) result.conflict.push_back(system.relations[model.rows[row].relation].reason);
		}
	}
	return result;
}
