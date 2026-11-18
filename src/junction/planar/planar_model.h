/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file planar_model.h The planar mapped composer's integer program: half-tile geometry, linear expressions, strands of segments, decks, crossings and the solve. */

#ifndef PLANAR_MODEL_H
#define PLANAR_MODEL_H

#include "../placement/junction_milp.h"
#include "../../3rdparty/fmt/format.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <optional>
#include <set>
#include <source_location>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace PlanarModel {

/* Half-tile coordinates: u east, v south; track points on tile-edge midpoints (u + v odd). An axis segment of length L moves L (even); a diagonal one moves (L, L). */

constexpr double INF = JunctionMilp::INF;
constexpr double OPTW = 0.01; ///< Tie-break: a bend pair that need not exist does not.
constexpr int OPTMAX = 16; ///< Half-tiles: the longest optional step (a bundle step of 6 lines).
constexpr double SOLVE_LIMIT = 5.0; ///< Seconds per solve.

/** Headings, clockwise from north (y down). */
enum Head : int { HN, HNE, HE, HSE, HS, HSW, HW, HNW };

/** The step of each heading. */
constexpr std::array<std::array<int, 2>, 8> STEP = {{{0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}}};

/**
 * Turn a heading.
 * @param h The heading.
 * @param k Eighth turns, clockwise.
 * @return The new heading.
 */
int Turn(int h, int k);

/**
 * The signed turn from one heading to another, in -3..4.
 * @param a The first heading.
 * @param b The second heading.
 * @return The turn.
 */
int Rot(int a, int b);

/** @return Whether the heading is an axis one. */
bool IsAxis(int d);

/** @return The step to the right of travel at a heading. */
const std::array<int, 2> &Right(int h);

/**
 * The distance a rung holds, in Dot(Right(target), .) units, from the lazy clash rule.
 * The rule (same tile unless opposite corners, or a shared end point; brute force of it): parallel pairs
 * (axis-axis, diag-diag) and any diagonal target need exactly 2 along Right(h); an axis target facing a
 * perpendicular axis end or a diagonal piece leading away needs 1 (the next tile).
 * @param target_d Direction of the target segment.
 * @param own_d Direction of the segment of the point's own run that ends in the point.
 * @return 1 when the target is axis and the own segment is diagonal or perpendicular to it, else 2.
 */
int RungSpacing(int target_d, int own_d);

/** A linear expression: a constant plus a coefficient per variable. Zero coefficients are kept, as the probe's dictionaries keep them. */
struct Ex {
	std::map<uint32_t, double> terms;
	double constant = 0;

	Ex() = default;
	Ex(double c) : constant(c) {}
};

/** A variable of the model: an expression with one term. */
struct Var : Ex {
	uint32_t index;

	explicit Var(uint32_t i) : index(i)
	{
		this->terms[i] = 1;
	}
};

Ex operator+(const Ex &a, const Ex &b);

Ex operator-(const Ex &a);

Ex operator-(const Ex &a, const Ex &b);

Ex operator*(const Ex &a, double k);

/** A constraint: expression >= 0, or == 0. */
struct Con {
	bool equal;
	Ex e;
};

Con Ge(const Ex &a, const Ex &b);

Con Le(const Ex &a, const Ex &b);

Con Eq(const Ex &a, const Ex &b);

/** A row of the program: lower <= sum <= upper. */
struct Row {
	std::vector<std::pair<uint32_t, double>> terms;
	double lower;
	double upper;
	std::string tag; ///< Which construction site made the row, for the conflict printout.
};

/** The tag the rows made now carry; one per thread, since variants build on parallel threads. */
inline thread_local std::string g_tag;

/** Tag the rows made in this scope (the enclosing tag returns after it). */
struct TagScope {
	std::string old;

	explicit TagScope(const std::string &tag) : old(g_tag) { g_tag = tag; }
	~TagScope() { g_tag = this->old; }
	TagScope(const TagScope &) = delete;
	TagScope &operator=(const TagScope &) = delete;
};

/** The tag of a row made outside every scope: its construction line. */
inline std::string Here(const std::source_location &l)
{
	return fmt::format("L{}", l.line());
}

/** A point of two variables. */
struct Point {
	Var x;
	Var y;
	const Var &operator[](size_t i) const { return i == 0 ? this->x : this->y; }
};

/**
 * The dot product of an integer direction and a point of expressions.
 * @param f The direction.
 * @param p The point.
 * @return The expression.
 */
template <class P>
Ex Dot(const std::array<int, 2> &f, const P &p)
{
	return p[0] * f[0] + p[1] * f[1];
}

/** A segment of a strand: a heading, a length variable and two end points; optional segments carry a binary. */
struct Seg {
	std::string strand;
	uint32_t i; ///< Index in the strand.
	int d; ///< Heading.
	Var L; ///< Length in half-tiles.
	Point a;
	Point b;
	std::optional<Var> z; ///< Present for an optional segment.
};

/** A strand: its start point and segments. */
struct Strand {
	Point start;
	std::vector<Seg> segs;
};

/** A deck on a segment: near head's end n and far head's start f, in half-tiles along the segment. */
struct DeckRec {
	Seg over;
	Var n;
	Var f;
};

/** A crossing of an under segment with an over segment's axis run. */
struct CrossRec {
	Seg over;
	Seg under;
	Var a;
	Var b;
};

/** A prio row: a parallel unused track on a side, P tiles before a merge. */
struct PrioRow {
	std::string strand;
	Point m;
	int h;
	int side;
};

/** A copy strand: it replaces segments [first, resume) of its strand. */
struct CopyRec {
	std::string copy;
	std::string strand;
	uint32_t first;
	uint32_t resume;
};

/** A solved vector: rounded variable values and the objective. */
struct Solution {
	std::vector<int64_t> val;
	double objective = 0;
};

/** The facts of a spec the model uses. */
struct Params {
	int tl; ///< Train length in tiles.
	int cl; ///< Curve spacing.
	int p; ///< Prio coverage in tiles.
	int g; ///< The gap between same-hand curves.
	int maxmid = 10; ///< The longest bridge middle in tiles (spec max_bridge_middle).
};

/** The route list of a composition. */
using RouteList = std::vector<std::vector<std::string>>;

/** The integer program of one variant: variables, rows, strands, decks, crossings and copies. */
class Model {
public:
	std::vector<double> lb; ///< Variable lower bounds.
	std::vector<double> ub; ///< Variable upper bounds.
	std::vector<double> cost; ///< Variable costs.
	std::vector<Row> rows; ///< Model rows.
	std::vector<std::pair<uint32_t, int>> parity; ///< (variable, parity) in insertion order.
	std::unordered_map<uint32_t, size_t> parity_at; ///< Position in parity.
	std::vector<std::string> order; ///< Strand names in insertion order.
	std::map<std::string, Strand> strands;
	std::set<std::pair<std::string, std::string>> related; ///< Name pairs (lower first) that meet at a node.
	std::vector<DeckRec> decks;
	std::vector<CrossRec> crossings;
	std::vector<PrioRow> prio_rows;
	std::vector<CopyRec> copies;
	std::vector<std::tuple<Seg, std::string, std::string>> pending; ///< Decks that wait for a later bundle: (segment, first strand, second strand).
	bool last_infeasible = false; ///< Whether the last Solve proved the rows infeasible.
	std::vector<uint32_t> iis_rows; ///< After a proven infeasible Solve: the LP relaxation's conflicting rows (model rows, then the extra rows).
	bool relax = false; ///< Solve the LP relaxation (the conflict reduction).
	std::set<size_t> skip; ///< Rows (model rows, then the extra rows) that Solve replaces by free rows (the conflict reduction).
	std::vector<double> last; ///< The last Optimal solve, all columns.
	std::map<uint32_t, double> obj_extra; ///< The area gradient.
	Var X{0}; ///< Box width, half-tiles.
	Var Y{0}; ///< Box height, half-tiles.
	Ex X0; ///< Box low edge in x (the mapped composer; the constant 0 otherwise).
	Ex Y0; ///< Box low edge in y; the first row is Y0 + 1 (zero otherwise).
	const std::vector<std::vector<std::string>> *routes = nullptr; ///< Route strand lists.
	const std::vector<std::vector<std::string>> *outline_routes = nullptr; ///< The routes of the outline.
	std::string error; ///< The first failure of a construction check.
	Params params{};
	bool deck_parity = false; ///< Deck ends on whole pieces (even half-tiles along their segment), so every deck has its two head pieces.
	std::string opt_group; ///< When set, optional segments share one binary per (group, heading): one switch per channel transition.
	std::map<std::pair<std::string, int>, Var> group_z; ///< The shared binaries.
	std::set<uint32_t> binaries; ///< The optional segments' switches.

	/** Build the empty model with its box variables. */
	Model()
	{
		this->X = this->NewVar();
		this->Y = this->NewVar();
	}

	/**
	 * Add a variable.
	 * @param lower Lower bound.
	 * @param upper Upper bound.
	 * @param c Objective coefficient.
	 * @return The variable.
	 */
	Var NewVar(double lower = 0.0, double upper = INF, double c = 0.0)
	{
		this->lb.push_back(lower);
		this->ub.push_back(upper);
		this->cost.push_back(c);
		return Var(static_cast<uint32_t>(this->lb.size() - 1));
	}

	/** Record the first construction failure. */
	void Fail(const std::string &what)
	{
		if (this->error.empty()) this->error = what;
	}

	/** Make a row of a constraint (zero coefficients dropped). */
	static Row MakeRow(const Con &con)
	{
		Row row;
		for (const auto &[k, v] : con.e.terms) {
			if (v != 0) row.terms.emplace_back(k, v);
		}
		row.lower = -con.e.constant;
		row.upper = con.equal ? -con.e.constant : INF;
		row.tag = g_tag;
		return row;
	}

	/** Add a constraint; one without variables must hold. */
	void Add(const Con &con, std::source_location loc = std::source_location::current())
	{
		Row row = MakeRow(con);
		if (row.tag.empty()) row.tag = Here(loc);
		if (row.terms.empty()) {
			if (con.equal ? con.e.constant != 0 : con.e.constant < 0) this->Fail("a constant constraint does not hold");
			return;
		}
		this->rows.push_back(std::move(row));
	}

	/** Tie a variable to a parity (x = 2k + p). */
	void Parity(const Var &x, int p)
	{
		const auto it = this->parity_at.find(x.index);
		if (it != this->parity_at.end()) {
			this->parity[it->second].second = p;
			return;
		}
		this->parity_at[x.index] = this->parity.size();
		this->parity.emplace_back(x.index, p);
	}

	/** @return A free point. */
	Point NewPoint()
	{
		/* A tiny weight on every coordinate pulls ties toward the north-west corner, so the solver has one best layout to prove, not many (PLANAR_PULL overrides it). */
		static const double pull = std::getenv("PLANAR_PULL") != nullptr ? std::atof(std::getenv("PLANAR_PULL")) : 0.001;
		const Var x = this->NewVar(-INF, INF, pull);
		const Var y = this->NewVar(-INF, INF, pull);
		return Point{x, y};
	}

	/** Make two points equal. */
	template <class A, class B>
	void Tie(const A &p, const B &q, std::source_location loc = std::source_location::current())
	{
		TagScope ts("tie@" + Here(loc));
		this->Add(Eq(p[0], q[0]));
		this->Add(Eq(p[1], q[1]));
	}

	/**
	 * Start a strand.
	 * @param name Its name.
	 * @param start Its start point; a fresh one when absent.
	 */
	void NewStrand(const std::string &name, std::optional<Point> start = std::nullopt)
	{
		Point s = start.has_value() ? *start : this->NewPoint();
		this->strands.erase(name);
		this->strands.emplace(name, Strand{s, {}});
		if (std::ranges::find(this->order, name) == this->order.end()) this->order.push_back(name);
	}

	/** @return The end point of a strand. */
	Point End(const std::string &name) const
	{
		const Strand &s = this->strands.at(name);
		return s.segs.empty() ? s.start : s.segs.back().b;
	}

	/**
	 * Add a segment to a strand.
	 * @param name The strand.
	 * @param d The heading.
	 * @param lo The least length (half-tiles).
	 * @param hi The greatest length, or none.
	 * @param c The cost per half-tile (a diagonal unit costs sqrt 2 times this).
	 * @param opt Whether the segment may vanish (a binary z; length 0 or in [lo, hi]).
	 * @return The segment.
	 */
	Seg AddSeg(const std::string &name, int d, int lo = 0, std::optional<int> hi = std::nullopt, double c = 1.0, bool opt = false)
	{
		TagScope ts(fmt::format("seg {}#{}", name, this->strands.at(name).segs.size()));
		const Point a = this->End(name);
		const Var L = this->NewVar(opt ? 0 : lo, hi.has_value() ? *hi : INF, c * (IsAxis(d) ? 1.0 : std::sqrt(2.0)));
		std::optional<Var> z;
		if (opt && !this->opt_group.empty() && std::getenv("PLANAR_NOTIE") == nullptr) {
			const auto key = std::make_pair(this->opt_group, d);
			auto it = this->group_z.find(key);
			if (it == this->group_z.end()) {
				/* A switch the width profile decides is fixed: on where the width changes ('+'), off where it does not ('-'); jogs ('j') stay free. */
				double lo_z = 0.0, hi_z = 1.0;
				const char kind = this->opt_group.back();
				if (kind == '+') lo_z = 1.0;
				if (kind == '-') hi_z = 0.0;
				it = this->group_z.emplace(key, this->NewVar(lo_z, hi_z, OPTW)).first;
				this->binaries.insert(it->second.index);
			}
			z = it->second;
			this->Add(Ge(L - *z * lo, 0));
			this->Add(Ge(*z * (hi.has_value() ? *hi : OPTMAX) - L, 0));
		} else if (opt) {
			z = this->NewVar(0, 1.0, OPTW);
			this->binaries.insert(z->index);
			this->Add(Ge(L - *z * lo, 0));
			this->Add(Ge(*z * (hi.has_value() ? *hi : OPTMAX) - L, 0));
		}
		if (IsAxis(d)) {
			this->Parity(a[0], STEP[d][1] == 0 ? 0 : 1);
			this->Parity(a[1], STEP[d][1] == 0 ? 1 : 0);
			this->Parity(L, 0);
		}
		const Point b = this->NewPoint();
		this->Add(Eq(b[0] - a[0] - L * STEP[d][0], 0));
		this->Add(Eq(b[1] - a[1] - L * STEP[d][1], 0));
		Strand &s = this->strands.at(name);
		Seg sg{name, static_cast<uint32_t>(s.segs.size()), d, L, a, b, z};
		s.segs.push_back(sg);
		return sg;
	}

	/**
	 * Add a required piece to a strand: it never vanishes; its least length comes from a rule (an axis piece at least a tile, a diagonal at least one piece), it turns at most one eighth from the piece before it, and a strand starting on a diagonal starts on a tile-edge midpoint.
	 * @param name The strand.
	 * @param d The heading.
	 * @param lo The least length (half-tiles).
	 * @param hi The greatest length, or none.
	 * @param c The cost per half-tile.
	 * @return The segment.
	 */
	Seg AddPiece(const std::string &name, int d, int lo, std::optional<int> hi = std::nullopt, double c = 1.0)
	{
		if (lo < (IsAxis(d) ? 2 : 1)) this->Fail(fmt::format("a piece shorter than its heading allows: {} d{} lo {}", name, d, lo));
		const Strand &s = this->strands.at(name);
		if (!s.segs.empty() && std::abs(Rot(s.segs.back().d, d)) > 1) {
			this->Fail(fmt::format("a strand turns by more than one eighth: {}#{} d{} -> d{}", name, s.segs.size(), s.segs.back().d, d));
		}
		if (s.segs.empty() && !IsAxis(d)) {
			const Var w = this->NewVar(-INF, INF);
			this->Add(Eq(w - s.start[0] - s.start[1], 0));
			this->Parity(w, 1);
		}
		return this->AddSeg(name, d, lo, hi, c, false);
	}

	/**
	 * The curve rows of every route over required pieces (DESIGN-pieces §4): between curves k apart along a route, the pieces between them sum to at least min(2 TL - 1, k CL); consecutive curves of opposite hands at least 2, of the same hand G. Every curve exists, so no row depends on a switch.
	 * @param routes The routes' strand names.
	 */
	void PieceCurves(const RouteList &routes);

	/** Mark strands as meeting at a node (clashes between them are allowed). */
	void Node(std::initializer_list<std::string> names)
	{
		this->Node(std::vector<std::string>(names));
	}

	/** Mark strands as meeting at a node (clashes between them are allowed). */
	void Node(const std::vector<std::string> &names)
	{
		for (auto i = names.begin(); i != names.end(); ++i) {
			for (auto j = i + 1; j != names.end(); ++j) this->related.insert(*i < *j ? std::make_pair(*i, *j) : std::make_pair(*j, *i));
		}
	}

	/**
	 * Let an under segment cross an over segment's axis run.
	 * @param over The over segment.
	 * @param under The under segment.
	 * @param head The heads' length along the over segment.
	 * @return The over parameter and the head length.
	 */
	std::pair<Var, int> Cross(const Seg &over, const Seg &under, int head, std::source_location loc = std::source_location::current())
	{
		TagScope ts(fmt::format("cross {}#{}>{}#{}@{}", over.strand, over.i, under.strand, under.i, Here(loc)));
		const Var a = this->NewVar();
		const Var b = this->NewVar();
		this->Add(Ge(b, 1));
		this->Add(Ge(under.L - b, 1));
		const auto &e = STEP[over.d];
		const auto &f = STEP[under.d];
		this->Add(Eq(over.a[0] + a * e[0] - under.a[0] - b * f[0], 0));
		this->Add(Eq(over.a[1] + a * e[1] - under.a[1] - b * f[1], 0));
		this->crossings.push_back({over, under, a, b});
		return {a, head};
	}

	/**
	 * Put a deck on an over segment: heads at both ends of the crossings it spans.
	 * @param over The segment.
	 * @param xs The crossings (parameter, head length).
	 * @param n_exact The exact near head end, if fixed.
	 * @param tail_exact The exact length after the far head start, if fixed.
	 * @param n_min The least near head end.
	 * @param tail_min The least length after the far head start.
	 * @return The near head end n and the far head start f.
	 */
	std::pair<Var, Var> Deck(const Seg &over, const std::vector<std::pair<Var, int>> &xs, std::optional<int> n_exact = std::nullopt, std::optional<int> tail_exact = std::nullopt, int n_min = 2, int tail_min = 2, std::source_location loc = std::source_location::current())
	{
		TagScope ts(fmt::format("deck {}#{}@{}", over.strand, over.i, Here(loc)));
		const Var n = this->NewVar();
		const Var f = this->NewVar();
		if (!n_exact.has_value()) {
			this->Add(Ge(n, n_min));
		} else {
			this->Add(Eq(n, *n_exact));
		}
		if (!tail_exact.has_value()) {
			this->Add(Ge(over.L - f, tail_min));
		} else {
			this->Add(Eq(over.L - f, *tail_exact));
		}
		for (const auto &[a, h] : xs) {
			this->Add(Ge(a - n, h));
			this->Add(Ge(f - a, h));
		}
		if (this->deck_parity) {
			this->Parity(n, 0);
			this->Parity(f, 0);
		}
		this->decks.push_back({over, n, f});
		return {n, f};
	}

	/**
	 * After a Solve with these extra rows proved infeasible: print a minimal conflicting set of rows to stderr, one "IIS <tag> : <row>" line each.
	 * The LP relaxation's conflict set is cut down by deleting rows one by one while the rest stays LP-infeasible, with every row outside it relaxed (a free row).
	 * An infeasibility that is integer only prints "IIS integer-only (LP feasible)".
	 * @param extra The extra rows of that Solve.
	 */
	void ExplainInfeasible(const std::vector<Row> &extra);

	/**
	 * Solve with extra rows, warm-started from the last solution.
	 * @param extra Extra rows.
	 * @param out The solution.
	 * @param deadline The time the solve must end by; its time limit is the time left, at most SOLVE_LIMIT.
	 * @return Whether an optimal solution was found.
	 */
	bool Solve(const std::vector<Row> &extra, Solution &out, std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::time_point::max())
	{
		const auto solve_t0 = std::chrono::steady_clock::now();
		struct Report {
			const std::chrono::steady_clock::time_point t0;
			bool *ok;
			~Report()
			{
				if (std::getenv("PLANAR_TIMES") != nullptr) fmt::print(stderr, "solve {} ms {}\n", std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - this->t0).count(), *this->ok ? "ok" : "no");
			}
		};
		bool report_ok = false;
		const Report report{solve_t0, &report_ok};

		JunctionMilp milp;
		milp.SetTimeLimit(std::clamp(std::chrono::duration<double>(deadline - solve_t0).count(), 0.001, SOLVE_LIMIT));
		const size_t n0 = this->lb.size();
		for (size_t i = 0; i < n0; i++) {
			const auto it = this->obj_extra.find(static_cast<uint32_t>(i));
			milp.AddVariable(this->lb[i], this->ub[i], this->cost[i] + (it == this->obj_extra.end() ? 0.0 : it->second), !this->relax);
		}
		for (size_t k = 0; k < this->parity.size(); k++) milp.AddVariable(-INF, INF, 0.0, !this->relax);
		size_t gi = 0;
		for (const std::vector<Row> *list : std::initializer_list<const std::vector<Row> *>{&this->rows, &extra}) {
			for (const Row &r : *list) {
				if (this->skip.contains(gi++)) {
					milp.AddRow({}, -INF, INF);
				} else {
					milp.AddRow(r.terms, r.lower, r.upper);
				}
			}
		}
		for (size_t k = 0; k < this->parity.size(); k++) {
			const std::vector<std::pair<uint32_t, double>> terms{{this->parity[k].first, 1.0}, {static_cast<uint32_t>(n0 + k), -2.0}};
			milp.AddRow(terms, this->parity[k].second, this->parity[k].second);
		}
		const size_t n = n0 + this->parity.size();
		if (this->last.size() == n) {
			std::vector<std::pair<uint32_t, double>> start;
			for (size_t i = 0; i < n; i++) start.emplace_back(static_cast<uint32_t>(i), this->last[i]);
			milp.SetStart(std::move(start));
		}
		const auto st = milp.Solve();
		this->last_infeasible = st == JunctionMilp::Status::Infeasible;
		this->iis_rows = this->last_infeasible ? milp.InfeasibleRows() : std::vector<uint32_t>{};
		if (st != JunctionMilp::Status::Optimal) return false;
		this->last.resize(n);
		out.val.resize(n);
		for (size_t i = 0; i < n; i++) {
			this->last[i] = milp.Value(static_cast<uint32_t>(i));
			out.val[i] = std::llround(this->last[i]);
		}
		out.objective = milp.Objective();
		report_ok = true;
		return true;
	}
};

/**
 * Evaluate an expression at a solution.
 * @param val The variable values.
 * @param e The expression.
 * @return Its value.
 */
double Ev(const std::vector<int64_t> &val, const Ex &e);

/**
 * Every segment sequence of a route: its strands in order, each doubled stretch by either copy.
 * @param M The model.
 * @param route The route's strand names.
 * @return The sequences, the later strands' choices varying fastest.
 */
std::vector<std::vector<Seg>> RouteSegs(const Model &M, const std::vector<std::string> &route);

} // namespace PlanarModel

#endif /* PLANAR_MODEL_H */
