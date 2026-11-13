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

#include <chrono>
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
		int clash; ///< The index of the JunctionPlacementSystem clash it is, -1 when none.
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

	void AddRow(const Coefs &coefs, double lower, double upper, int relation = -1, int clash = -1)
	{
		Row row{lower, upper, {}, relation, clash};
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
 * @param zero The binary z of every bend that may have size 0 (an S fan's, a rows-sized S-bend's): 1 when it has a size.
 */
static void AddCurveGapCounters(JointModel &model, const JunctionPlacementSystem &system, const JunctionSpec &spec, const std::vector<uint32_t> &x, const std::vector<uint32_t> &y, const std::map<int, uint32_t> &L, const std::map<int, uint32_t> &zero)
{
	for (const JunctionSystemRoute &route : system.routes) {
		const size_t n = route.bends.size();
		std::vector<uint32_t> entry;
		for (size_t i = 0; i < n; i++) entry.push_back(model.Var(-JunctionMilp::INF, JunctionMilp::INF));

		/* scale * q of one raster quantity (q at L = 1, q at L = 2), q = q1 + (L - 1) d. A bend that may have size 0 writes it (q1 - d) z + d L: exact for L >= 1 and 0 at L = 0. */
		auto add_quantity = [&](Expr &e, double scale, const JunctionSystemRouteBend &bend, const std::pair<int, int> &q) {
			const int slope = q.second - q.first;
			if (slope != 0) e.coefs[L.at(bend.bend)] += scale * slope;
			const auto z = bend.bend >= 0 ? zero.find(bend.bend) : zero.end();
			if (z != zero.end()) {
				e.coefs[z->second] += scale * (q.first - slope);
			} else {
				e.constant += scale * (q.first - slope);
			}
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
			/* A vanished bend has no curves: the row is relaxed by M (1 - z), once per such bend in it. */
			double lower = gap.min_gap;
			std::set<int> relaxed;
			for (const size_t bend_index : {bi, bj}) {
				const int id = route.bends[bend_index].bend;
				const auto z = id >= 0 ? zero.find(id) : zero.end();
				if (z == zero.end() || !relaxed.insert(id).second) continue;
				e.coefs[z->second] -= BIG;
				lower -= BIG;
			}
			add_row(e, lower, JunctionMilp::INF);
		}
	}
}

JunctionJointResult SolveJunctionJoint(const JunctionPlacementSystem &system, const JunctionSpec &spec, bool relaxed)
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
	std::set<int> in_edges; ///< The edges a lane path starts on: their fan starts on its port.
	std::set<int> out_edges; ///< The edges a lane path ends on.
	for (const std::vector<int> &path : system.paths) {
		if (path.empty()) continue;
		in_edges.insert(path.front());
		out_edges.insert(path.back());
	}
	for (const JunctionSystemBend &b : system.bends) {
		if (b.source == "port_rows" && in_edges.count(b.edge) != 0 && out_edges.count(b.edge) != 0) {
			result.conflict.push_back("a port fan at both ends of a path");
			return result;
		}
	}
	std::map<int, uint32_t> L, Z, zero; ///< zero: z of every bend that may have size 0 (Z for S fans).
	for (const JunctionSystemBend &b : system.bends) {
		const std::string &src = b.source;
		if (src == "free") {
			if (b.kind == "SFan") {
				L[b.id] = model.Var(0);
				Z[b.id] = model.Var(0, 1);
				model.AddRow({{L[b.id], 1}, {Z[b.id], -1}}, 0, JunctionMilp::INF); // fan: L >= z
				model.AddRow({{L[b.id], 1}, {Z[b.id], -BIG}}, -JunctionMilp::INF, 0); // fan: L <= M z
				zero[b.id] = Z[b.id];
			} else {
				L[b.id] = model.Var(b.floor);
			}
		} else if (src == "corner_rows") {
			L[b.id] = model.Var(k);
		} else if (src == "rows") {
			L[b.id] = model.Var(0);
			zero[b.id] = model.Var(0, 1);
			model.AddRow({{L[b.id], 1}, {zero[b.id], -1}}, 0, JunctionMilp::INF); // rows-sized S-bend: L >= z
			model.AddRow({{L[b.id], 1}, {zero[b.id], -BIG}}, -JunctionMilp::INF, 0); // rows-sized S-bend: L <= M z
		} else if (src == "port_rows") {
			assert(b.item_pt >= 0);
			L[b.id] = model.Var(0);
			zero[b.id] = model.Var(0, 1);
			model.AddRow({{L[b.id], 1}, {zero[b.id], -1}}, 0, JunctionMilp::INF); // port fan: L >= z
			model.AddRow({{L[b.id], 1}, {zero[b.id], -BIG}}, -JunctionMilp::INF, 0); // port fan: L <= M z
		} else { // fixed_k, one_row: held at the recorded size
			L[b.id] = model.Var(b.m, b.m);
		}
	}

	/* The columns of the single bridges over an S-bend (DESIGN-G3 §G3.4a): t in [1, 64], t - t1 L(bend) <= t0, and the tie break EPS t keeps an equal-cost solution at t = 1.
	 * A bridge with an S-bend on both sides of its crossing (DESIGN-G3 §G3.4b) has a binary w, 1 when it passes over the one after:
	 *   t - t1 L(before) - BIG w <= t0 and t - t1_after L(after) + BIG w <= t0_after + BIG. */
	std::vector<uint32_t> T;
	std::vector<int> W; ///< Per over entry, the variable w, -1 when the entry has one alternative.
	for (const JunctionSystemOver &over : system.overs) {
		T.push_back(model.Var(1, 64));
		const bool both = over.bend >= 0 && over.bend_after >= 0;
		W.push_back(both ? static_cast<int>(model.Var(0, 1)) : -1);
		/* The fixed path allows t <= t0 + t1 max(m, 2); this row is stricter at L = 1, so every joint t is accepted by the fixed path. */
		if (over.bend >= 0) {
			Coefs coefs{{T.back(), 1}, {L[over.bend], -over.t1}};
			if (both) coefs[W.back()] -= BIG;
			model.AddRow(coefs, -JunctionMilp::INF, over.t0); // bridge over an S-bend: t <= t0 + t1 L (+ BIG w)
		}
		if (over.bend_after >= 0) {
			Coefs coefs{{T.back(), 1}, {L[over.bend_after], -over.t1_after}};
			if (both) coefs[W.back()] += BIG;
			model.AddRow(coefs, -JunctionMilp::INF, over.t0_after + (both ? BIG : 0)); // bridge over an S-bend: t <= t0_after + t1_after L (+ BIG (1 - w))
		}
		model.cost[T.back()] += EPS;
	}

	/* The recorded relations: coord(v) - coord(u) - a L - d t >= b - d t_recorded (tagged, d from a bridge's column) or >= c. */
	std::set<int> tagged_y;
	for (size_t r = 0; r < system.relations.size(); r++) {
		const JunctionSystemRelation &relation = system.relations[r];
		if (!system.clashes.empty() && relation.reason.starts_with("occupancy")) continue; // Replaced by the clash rows below.
		if (relation.nonlinear && !system.routes.empty() && relation.reason.starts_with("curve gap")) continue; // Replaced by the unit counters below.
		if (relation.order_pin) continue; // The hug's pin to the flat solve's order: the span cost pulls the heads tight instead.
		const std::vector<uint32_t> &axis_coord = *coord[relation.axis];
		Coefs coefs;
		coefs[axis_coord[relation.v]] += 1;
		coefs[axis_coord[relation.u]] -= 1; // v == u: a constant row
		double over_constant = 0; ///< c has + d (t - t_recorded): the row takes - d t and its constant - d t_recorded.
		if (relation.over >= 0) {
			coefs[T[relation.over]] -= relation.d;
			over_constant = -static_cast<double>(relation.d) * system.overs[relation.over].t;
		}
		const int bid = relation.bend;
		double lower = relation.c + over_constant;
		if (relation.zero_branch && bid >= 0 && Z.count(bid) != 0) {
			coefs[Z[bid]] = BIG; // only when the fan is empty
			lower = relation.c;
		} else if (!relation.nonlinear && !relation.zero_branch && bid >= 0) {
			coefs[L[bid]] -= relation.a;
			lower = relation.b + over_constant;
			if (Z.count(bid) != 0) { // an S fan's sized branch: only when the fan is not empty
				coefs[Z[bid]] = -BIG;
				lower = relation.b - BIG;
			}
			if (relation.axis == 1) tagged_y.insert(bid);
		}
		/* A relation of one alternative of a bridge over an S-bend (DESIGN-G3 §G3.4b) holds only when w says so:
		 *   alt 0 (before): row + BIG w >= lower; alt 1 (after): row - BIG w >= lower - BIG. */
		if (relation.alt >= 0 && relation.over >= 0 && W[relation.over] >= 0) {
			if (relation.alt == 0) {
				coefs[W[relation.over]] += BIG;
			} else {
				coefs[W[relation.over]] -= BIG;
				lower -= BIG;
			}
		}
		model.AddRow(coefs, lower, JunctionMilp::INF, static_cast<int>(r));
	}

	/* The clashes: x[b] + c1 L - x[a] - c1 L >= gap - b.c0 + a.c0, relaxed by BIG (1 - z) once per bend that may vanish.
	 * A clash that is not a tie and may be chosen is a choice of order, o binary: b after a (o = 0) holds b.left - a.right >= gap, a after b (o = 1) holds a.left - b.right >= gap_reversed; the other row is relaxed by BIG. */
	for (size_t i = 0; i < system.clashes.size(); i++) {
		const JunctionSystemClash &clash = system.clashes[i];
		const bool choice = clash.choose && !clash.tie;
		const uint32_t order = choice ? model.Var(0, 1) : 0;
		auto add_order_row = [&](const JunctionSystemColumn &later, const JunctionSystemColumn &earlier, int gap, bool b_after_a) {
			Coefs coefs;
			double lower = gap - later.c0 + earlier.c0;
			std::set<int> relaxed_bends;
			for (const auto &[column, sign] : {std::pair{&later, 1}, std::pair{&earlier, -1}}) {
				coefs[x[column->pt]] += sign;
				if (column->c1 == 0) continue;
				const auto length = L.find(column->bend);
				if (length != L.end()) {
					coefs[length->second] += sign * column->c1;
				} else {
					lower -= sign * column->c1 * system.bends[column->bend].m; // No length variable: the recorded size.
				}
				const auto z = zero.find(column->bend);
				if (z == zero.end() || !relaxed_bends.insert(column->bend).second) continue;
				coefs[z->second] -= BIG;
				lower -= BIG;
			}
			if (choice && b_after_a) {
				coefs[order] += BIG; // Relaxed by BIG o: holds when o = 0.
			} else if (choice) {
				coefs[order] -= BIG; // Relaxed by BIG (1 - o): holds when o = 1.
				lower -= BIG;
			}
			model.AddRow(coefs, lower, JunctionMilp::INF, -1, static_cast<int>(i));
		};
		add_order_row(clash.b_left, clash.a_right, clash.gap, true);
		if (choice) add_order_row(clash.a_left, clash.b_right, clash.gap_reversed, false);
	}

	AddCurveGapCounters(model, system, spec, x, y, L, zero);

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
		} else if (b.source == "port_rows") {
			if (b.m == 0) {
				/* Recorded empty: the side is unknown, L >= |port row - y(item)|; the next recording fixes it. */
				model.AddRow({{L[i], 1}, {y[b.item_pt], 1}}, b.port_row, JunctionMilp::INF);
				model.AddRow({{L[i], 1}, {y[b.item_pt], -1}}, -b.port_row, JunctionMilp::INF);
			} else {
				/* y(last) - y(first) goes south iff (hand > 0) == (hin == 1); sigma = sign(port row - y(item)) is that sign, negated when the fan starts on its port (an in edge's). */
				const bool south = (b.hand > 0) == (b.hin == 1);
				const int sigma = (south ? 1 : -1) * (in_edges.count(b.edge) != 0 ? -1 : 1);
				model.AddRow({{y[b.item_pt], -sigma}, {L[i], -1}}, -sigma * b.port_row, -sigma * b.port_row);
			}
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
	for (size_t i = 0; i < model.lo.size(); i++) milp.AddVariable(model.lo[i], model.hi[i], model.cost[i], model.integer[i] && !relaxed);
	std::vector<std::pair<uint32_t, double>> terms;
	for (const JointModel::Row &row : model.rows) {
		terms.assign(row.coefs.begin(), row.coefs.end());
		milp.AddRow(terms, row.lo, row.hi);
	}
	result.status = milp.Solve();
	if (result.status == JunctionMilp::Status::Optimal) {
		result.objective = milp.Objective();
		for (const JunctionSystemBend &b : system.bends) result.length.push_back(static_cast<int>(std::lround(milp.Value(L[b.id]))));
		result.choices.port_offset = system.port_offset;
		for (const uint32_t tokens : system.tokens) {
			result.choices.m.emplace_back(tokens, 0);
			result.choices.corner.emplace_back(tokens, 0);
		}
		for (size_t i = 0; i < system.bends.size(); i++) {
			const JunctionSystemBend &b = system.bends[i];
			if (b.copy != 0) continue;
			if (b.source == "free") result.choices.m[b.edge][b.tok] = result.length[i];
			if (b.source == "corner_rows") result.choices.corner[b.edge][b.tok] = result.length[i];
		}
		if (!system.overs.empty()) {
			bool moved = false; ///< An all-1 column is the empty choice: it stays empty, as the fixed path reads it.
			for (const uint32_t column : T) moved |= std::lround(milp.Value(column)) != 1;
			if (moved) {
				for (const uint32_t tokens : system.tokens) result.choices.over.emplace_back(tokens, 0);
				for (size_t i = 0; i < system.overs.size(); i++) result.choices.over[system.overs[i].edge][system.overs[i].tok] = static_cast<int>(std::lround(milp.Value(T[i]))) - 1;
			}
			bool after = false; ///< An all-0 alternative is the empty choice.
			for (const int w : W) after |= w >= 0 && std::lround(milp.Value(static_cast<uint32_t>(w))) != 0;
			if (after) {
				for (const uint32_t tokens : system.tokens) result.choices.over_after.emplace_back(tokens, 0);
				for (size_t i = 0; i < system.overs.size(); i++) {
					if (W[i] >= 0) result.choices.over_after[system.overs[i].edge][system.overs[i].tok] = static_cast<int>(std::lround(milp.Value(static_cast<uint32_t>(W[i]))));
				}
			}
		}
		for (uint32_t p = 0; p < npts; p++) {
			result.x.push_back(static_cast<int>(std::lround(milp.Value(x[p]))));
			result.y.push_back(static_cast<int>(std::lround(milp.Value(y[p]))));
		}
	} else if (result.status == JunctionMilp::Status::Infeasible) {
		for (uint32_t row : milp.InfeasibleRows()) {
			if (model.rows[row].relation >= 0) result.conflict.push_back(system.relations[model.rows[row].relation].reason);
			if (model.rows[row].clash >= 0) result.conflict.push_back(system.clashes[model.rows[row].clash].reason);
		}
	}
	return result;
}

JunctionJointLoopResult SolveJunctionJointLoop(const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, const JunctionTrackOrder &order, const JunctionTrackStructure &structure, const JunctionTrackPrograms &programs, const JunctionSpec &spec, const JunctionPlacementChoices &choices, int max_rounds)
{
	JunctionJointLoopResult result;
	result.choices = choices;
	/* The state a round starts from: its choices and the point it detects at (empty in round 0). */
	std::vector<std::pair<JunctionPlacementChoices, JunctionSystemPoint>> history;
	std::vector<size_t> added_in_round; ///< Per round, the clashes it kept.
	std::map<std::string, JunctionSystemClash> kept; ///< Every clash seen, by key.
	bool point_joint = false; ///< Whether the point the round detects at is a joint solution's.
	for (int k = 0; k < max_rounds; k++) {
		const auto start = std::chrono::steady_clock::now();
		const JunctionPlacementChoices round_choices = result.choices;
		const JunctionSystemPoint round_point = result.point;
		history.emplace_back(round_choices, round_point);
		result.rounds = k + 1;
		JunctionPlacementSystem system = RecordJunctionPlacementSystem(topology, paths, graph, order, structure, programs, spec, round_choices, nullptr, round_point.x.empty() ? nullptr : &round_point);
		if (system.relations.empty()) {
			result.outcome = "recorded nothing";
			break;
		}
		size_t added = 0;
		for (const JunctionSystemClash &clash : system.clashes) {
			if (kept.count(clash.key) != 0 || kept.count(clash.reversed) != 0) continue;
			JunctionSystemClash &entry = kept.emplace(clash.key, clash).first->second;
			entry.choose = point_joint;
			added++;
		}
		added_in_round.push_back(added);
		system.clashes.clear();
		for (const auto &[key, clash] : kept) system.clashes.push_back(clash);
		const JunctionJointResult joint = SolveJunctionJoint(system, spec);
		int sum = 0, changed = 0;
		for (size_t i = 0; i < joint.length.size(); i++) {
			sum += joint.length[i];
			changed += joint.length[i] != system.bends[i].m;
		}
		int sum_t = 0;
		for (const std::vector<int> &edge : joint.choices.over) {
			for (const int t : edge) sum_t += t;
		}
		std::string failed;
		for (const std::string &text : system.failed) failed += (failed.empty() ? "" : "; ") + text;
		/* The point the next round detects at: the recording's own, else the joint's when this system had X relations (else its x is meaningless), else none. */
		bool has_x = false;
		for (const JunctionSystemRelation &relation : system.relations) has_x |= relation.axis == 0;
		const char *point_kind = !system.solved.x.empty() ? "solved" : (has_x && !joint.x.empty() ? "joint" : "none");
		const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
		result.log.push_back(fmt::format("round {}: status {}, objective {:.3f}, sum L {}, sum t - 1 {}, changed {}, new clashes kept {}, kept total {}, chosen {}, failed [{}], point: {}, {:.1f} ms", k, static_cast<int>(joint.status), joint.objective, sum, sum_t, changed, added, kept.size(), std::count_if(system.clashes.begin(), system.clashes.end(), [](const JunctionSystemClash &c) { return c.choose; }), failed, point_kind, ms));
		if (joint.status == JunctionMilp::Status::Infeasible) {
			std::string chain;
			for (const std::string &reason : joint.conflict) chain += (chain.empty() ? "" : " <- ") + reason;
			result.outcome = "infeasible: " + chain;
			break;
		}
		if (joint.status != JunctionMilp::Status::Optimal) {
			result.outcome = fmt::format("solver status {}", static_cast<int>(joint.status));
			break;
		}
		result.choices = joint.choices;
		/* The canonical point of this round's recording; the joint solution's when the recording's solves failed but had X relations; none otherwise. */
		point_joint = system.solved.x.empty() && has_x;
		if (!system.solved.x.empty()) {
			result.point = system.solved;
		} else if (has_x) {
			result.point = {joint.x, joint.y};
		} else {
			result.point = {};
		}
		if (k > 0 && joint.choices == round_choices && added == 0 && system.solved == round_point) {
			result.outcome = "settled";
			break;
		}
		size_t first = history.size();
		for (size_t j = 0; j < history.size() - 1 && first == history.size(); j++) {
			if (history[j].first != result.choices || history[j].second != result.point) continue;
			size_t added_since = 0; // A repeat is a cycle only when no clash was kept since that round.
			for (size_t later = j + 1; later < added_in_round.size(); later++) added_since += added_in_round[later];
			if (added_since == 0) first = j;
		}
		if (first < history.size()) {
			result.outcome = fmt::format("cycle of period {}", history.size() - first);
			break;
		}
	}
	if (result.outcome.empty()) result.outcome = "round limit";
	return result;
}

JunctionPlacementChoices JunctionFloorChoices(const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, const JunctionTrackOrder &order, const JunctionTrackStructure &structure, const JunctionTrackPrograms &programs, const JunctionSpec &spec)
{
	JunctionPlacementSystem system;
	std::string error;
	PlaceTrackGraph(topology, paths, graph, order, structure, programs, spec, nullptr, &error, {}, {}, nullptr, {0, 0}, true, false, nullptr, nullptr, nullptr, &system);
	JunctionPlacementChoices choices;
	if (system.tokens.empty()) return choices;
	for (const uint32_t tokens : system.tokens) {
		choices.m.emplace_back(tokens, 0);
		choices.corner.emplace_back(tokens, 0);
	}
	for (const JunctionSystemBend &b : system.bends) {
		if (b.copy != 0) continue;
		if (b.source == "free") choices.m[b.edge][b.tok] = b.kind == "SFan" ? 0 : 1;
		if (b.source == "corner_rows") choices.corner[b.edge][b.tok] = system.k;
	}
	choices.port_offset = system.port_offset;
	return choices;
}
