/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file planar_components.cpp The planar component solver implementation (DESIGN-components §6, translated from wip/planar-probe8.py). */

#include "../../stdafx.h"

#include "planar_components.h"

#include "../junction_spec.h"
#include "../placement/junction_milp.h"
#include "../../3rdparty/fmt/format.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <unordered_map>

#include "../../safeguards.h"

namespace {

/* Half-tile coordinates: u east, v south; track points on tile-edge midpoints (u + v odd). An axis segment of length L moves L (even); a diagonal one moves (L, L). */

constexpr double INF = JunctionMilp::INF;
constexpr int STUB = 4; ///< Half-tiles of a port stub.
constexpr double FANW = 1.5; ///< Cost per half-tile of a fanned (spread) stretch; the tight stub costs 1.
constexpr double OPTW = 0.01; ///< Tie-break: a bend pair that need not exist does not.
constexpr int OPTMAX = 16; ///< Half-tiles: the longest optional step (a bundle step of 6 lines).
constexpr double AREA_W = 10; ///< Objective weight per tile of area inside the outline.
constexpr double BUDGET = 15.0; ///< Seconds per variant for the lazy loop.
constexpr double SOLVE_LIMIT = 5.0; ///< Seconds per solve.
constexpr int LAZY_ROUNDS = 40; ///< Rounds of the lazy clash loop.
constexpr int AREA_ROUNDS = 6; ///< Rounds of the area linearisation.

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
int Turn(int h, int k)
{
	return ((h + k) % 8 + 8) % 8;
}

/**
 * The signed turn from one heading to another, in -3..4.
 * @param a The first heading.
 * @param b The second heading.
 * @return The turn.
 */
int Rot(int a, int b)
{
	const int d = ((b - a) % 8 + 8) % 8;
	return d > 4 ? d - 8 : d;
}

/** @return Whether the heading is an axis one. */
bool IsAxis(int d)
{
	return d % 2 == 0;
}

/** @return The step to the right of travel at a heading. */
const std::array<int, 2> &Right(int h)
{
	return STEP[Turn(h, 2)];
}

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

Ex operator+(const Ex &a, const Ex &b)
{
	Ex r = a;
	for (const auto &[k, v] : b.terms) r.terms[k] += v;
	r.constant += b.constant;
	return r;
}

Ex operator-(const Ex &a)
{
	Ex r;
	for (const auto &[k, v] : a.terms) r.terms[k] = -v;
	r.constant = -a.constant;
	return r;
}

Ex operator-(const Ex &a, const Ex &b)
{
	return a + (-b);
}

Ex operator*(const Ex &a, double k)
{
	Ex r;
	for (const auto &[i, v] : a.terms) r.terms[i] = v * k;
	r.constant = a.constant * k;
	return r;
}

/** A constraint: expression >= 0, or == 0. */
struct Con {
	bool equal;
	Ex e;
};

Con Ge(const Ex &a, const Ex &b) { return {false, a - b}; }
Con Le(const Ex &a, const Ex &b) { return {false, b - a}; }
Con Eq(const Ex &a, const Ex &b) { return {true, a - b}; }

/** A row of the program: lower <= sum <= upper. */
struct Row {
	std::vector<std::pair<uint32_t, double>> terms;
	double lower;
	double upper;
};

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
};

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
	std::vector<double> last; ///< The last Optimal solve, all columns.
	std::map<uint32_t, double> obj_extra; ///< The area gradient.
	Var X{0}; ///< Box width, half-tiles.
	Var Y{0}; ///< Box height, half-tiles.
	const std::vector<std::vector<std::string>> *routes = nullptr; ///< Route strand lists.
	const std::vector<std::vector<std::string>> *outline_routes = nullptr; ///< The routes of the outline.
	std::string error; ///< The first failure of a construction check.
	Params params{};

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
		return row;
	}

	/** Add a constraint; one without variables must hold. */
	void Add(const Con &con)
	{
		Row row = MakeRow(con);
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
		const Var x = this->NewVar(-INF);
		const Var y = this->NewVar(-INF);
		return Point{x, y};
	}

	/** Make two points equal. */
	template <class A, class B>
	void Tie(const A &p, const B &q)
	{
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
		const Point a = this->End(name);
		const Var L = this->NewVar(opt ? 0 : lo, hi.has_value() ? *hi : INF, c * (IsAxis(d) ? 1.0 : std::sqrt(2.0)));
		std::optional<Var> z;
		if (opt) {
			z = this->NewVar(0, 1.0, OPTW);
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

	/** Mark strands as meeting at a node (clashes between them are allowed). */
	void Node(std::initializer_list<std::string> names)
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
	std::pair<Var, int> Cross(const Seg &over, const Seg &under, int head)
	{
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
	std::pair<Var, Var> Deck(const Seg &over, const std::vector<std::pair<Var, int>> &xs, std::optional<int> n_exact = std::nullopt, std::optional<int> tail_exact = std::nullopt, int n_min = 2, int tail_min = 2)
	{
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
		this->decks.push_back({over, n, f});
		return {n, f};
	}

	/**
	 * Solve with extra rows, warm-started from the last solution.
	 * @param extra Extra rows.
	 * @param out The solution.
	 * @return Whether an optimal solution was found.
	 */
	bool Solve(const std::vector<Row> &extra, Solution &out)
	{
		JunctionMilp milp;
		milp.SetTimeLimit(SOLVE_LIMIT);
		const size_t n0 = this->lb.size();
		for (size_t i = 0; i < n0; i++) {
			const auto it = this->obj_extra.find(static_cast<uint32_t>(i));
			milp.AddVariable(this->lb[i], this->ub[i], this->cost[i] + (it == this->obj_extra.end() ? 0.0 : it->second), true);
		}
		for (size_t k = 0; k < this->parity.size(); k++) milp.AddVariable(-INF, INF, 0.0, true);
		for (const Row &r : this->rows) milp.AddRow(r.terms, r.lower, r.upper);
		for (const Row &r : extra) milp.AddRow(r.terms, r.lower, r.upper);
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
		if (milp.Solve() != JunctionMilp::Status::Optimal) return false;
		this->last.resize(n);
		out.val.resize(n);
		for (size_t i = 0; i < n; i++) {
			this->last[i] = milp.Value(static_cast<uint32_t>(i));
			out.val[i] = std::llround(this->last[i]);
		}
		out.objective = milp.Objective();
		return true;
	}
};

/**
 * Evaluate an expression at a solution.
 * @param val The variable values.
 * @param e The expression.
 * @return Its value.
 */
double Ev(const std::vector<int64_t> &val, const Ex &e)
{
	double r = e.constant;
	for (const auto &[k, v] : e.terms) r += static_cast<double>(val[k]) * v;
	return r;
}

/** The kind of a track piece on its tile: an axis step or a diagonal half step by its corner. */
enum class PieceKind : uint8_t { X, Y, NW, NE, SW, SE };

/** One piece of a segment as the probe's `pieces` gives it. */
struct Pc {
	int tx; ///< Tile x.
	int ty; ///< Tile y.
	PieceKind kind;
	std::array<int, 2> a; ///< Start point.
	std::array<int, 2> b; ///< End point.
	int t; ///< Offset along the segment in half-tiles.
};

/** Floor division by a positive number. */
int FloorDiv(int x, int d)
{
	return x >= 0 ? x / d : -((-x + d - 1) / d);
}

/**
 * The pieces of a segment.
 * @param u Start u.
 * @param v Start v.
 * @param L Length in half-tiles.
 * @param d Heading.
 * @return The pieces in travel order.
 */
std::vector<Pc> Pieces(int u, int v, int L, int d)
{
	const int dx = STEP[d][0], dy = STEP[d][1];
	std::vector<Pc> out;
	if (IsAxis(d)) {
		for (int t = 0; t < L; t += 2) {
			const std::array<int, 2> a{u + dx * t, v + dy * t};
			const std::array<int, 2> b{u + dx * (t + 2), v + dy * (t + 2)};
			out.push_back({FloorDiv(a[0] + b[0], 4), FloorDiv(a[1] + b[1], 4), dy == 0 ? PieceKind::X : PieceKind::Y, a, b, t});
		}
	} else {
		for (int t = 0; t < L; t++) {
			const std::array<int, 2> a{u + dx * t, v + dy * t};
			const std::array<int, 2> b{u + dx * (t + 1), v + dy * (t + 1)};
			const int tx = FloorDiv(a[0] + b[0], 4), ty = FloorDiv(a[1] + b[1], 4);
			const bool north = a[1] + b[1] - 4 * ty < 2;
			const bool west = a[0] + b[0] - 4 * tx < 2;
			out.push_back({tx, ty, north ? (west ? PieceKind::NW : PieceKind::NE) : (west ? PieceKind::SW : PieceKind::SE), a, b, t});
		}
	}
	return out;
}

/** The route list of a composition. */
using RouteList = std::vector<std::vector<std::string>>;

/** The routes of LL² as the probe lists them: strand names are the topology's line names. */
const RouteList &LL2Routes()
{
	static const RouteList routes = {
		{"EF1", "E1", "EO1"}, {"EF2", "E2", "E2a", "EDj1", "EO1"}, {"EF2", "E2", "E2b", "EDj2", "EO2"},
		{"CD1", "D1", "D1a", "EDj1", "EO1"}, {"CD1", "D1", "D1b", "EDj2", "EO2"}, {"CD2", "D2", "EO2"},
		{"EF1", "F1", "F1a", "AFj1", "SO1"}, {"EF1", "F1", "F1b", "AFj2", "SO2"}, {"EF2", "F2", "SO2"},
		{"AB1", "A1", "SO1"}, {"AB2", "A2", "A2a", "AFj1", "SO1"}, {"AB2", "A2", "A2b", "AFj2", "SO2"},
		{"AB1", "B1", "B1a", "CBj1", "WO1"}, {"AB1", "B1", "B1b", "CBj2", "WO2"}, {"AB2", "B2", "WO2"},
		{"CD1", "C1", "WO1"}, {"CD2", "C2", "C2a", "CBj1", "WO1"}, {"CD2", "C2", "C2b", "CBj2", "WO2"}};
	return routes;
}

/** The outline routes of LL². */
const RouteList &LL2Outline()
{
	static const RouteList routes = {{"EF1", "E1", "EO1"}, {"AB1", "A1", "SO1"}, {"CD1", "C1", "WO1"}};
	return routes;
}

/** The routes of T2. */
const RouteList &T2Routes()
{
	static const RouteList routes = [] {
		RouteList r;
		for (const char k : {'1', '2'}) {
			const std::string lane(1, k);
			for (const auto &[a, b, c] : {std::array<const char *, 3>{"EF", "E", "ED"}, {"EF", "F", "AF"}, {"AB", "B", "CB"}, {"AB", "A", "AF"}, {"CD", "C", "CB"}, {"CD", "D", "ED"}}) {
				r.push_back({a + lane, b + lane, c + lane});
			}
		}
		return r;
	}();
	return routes;
}

/** The outline routes of T2. */
const RouteList &T2Outline()
{
	static const RouteList routes = {{"EF1", "E1", "ED1"}, {"AB1", "A1", "AF1"}, {"CD1", "C1", "CB1"}};
	return routes;
}

/** A bundle: lanes left to right, a heading list, and per lane its segments. */
struct Bundle {
	std::vector<std::string> lanes;
	std::map<std::string, std::vector<Seg>> segs;
};

/** One variant of a composition. */
struct Variant {
	int fray_s; ///< The S fray's side.
	int other; ///< LL²: the diagonal west fray (0 or 1); T2: the S join's side (+1 or -1).
};

/** Builds the model of one variant of T2 or LL² (the probe's build_t2 with F over D and the flipped crossing triangle). */
class Composer {
public:
	Model M;

	/**
	 * Build the model.
	 * @param params The spec facts.
	 * @param t2 Whether the topology is T2, else LL².
	 * @param variant The variant.
	 */
	Composer(const Params &params, bool t2, const Variant &variant) : t2(t2), P(params.p), TL(params.tl), fray_s(variant.fray_s), wdiag(t2 ? 0 : variant.other), join_s(t2 ? variant.other : 1)
	{
		this->M.params = params;
		this->Build();
	}

private:
	bool t2;
	int P;
	int TL;
	int fray_s;
	int wdiag;
	int join_s;
	std::vector<std::pair<std::string, Bundle>> bundles; ///< In insertion order.

	/** @return The 'u' of a deck's near head end. */
	static Ex HeadU(const Seg &seg, const Var &n)
	{
		return seg.a[0] + n * STEP[seg.d][0];
	}

	/** A port line at the box boundary. */
	void PortIn(const std::string &name, char side)
	{
		this->M.NewStrand(name);
		const Point u = this->M.strands.at(name).start;
		if (side == 'W') this->M.Add(Eq(u[0], 0));
		if (side == 'E') this->M.Add(Eq(u[0] - this->M.X, 0));
		if (side == 'S') this->M.Add(Eq(u[1] - this->M.Y, 0));
	}

	/** Start a copy strand of a lane at a point. */
	std::string StartCopy(const std::string &lane, const char *mark)
	{
		const Point cs = this->M.End(lane);
		const std::string c = lane + mark;
		this->M.NewStrand(c, cs);
		this->M.Node({lane, c});
		return c;
	}

	/**
	 * A doubled over stretch on a lane (the parallelogram, copy toward sigma, shift back; D145/D314 tight heads, D118 equal bridges and stagger).
	 * @param xs_of The crossings of a segment's deck.
	 */
	template <class F>
	void Doubled(const std::string &l, int h, int sigma, F xs_of)
	{
		Model &M = this->M;
		M.AddSeg(l, h, 2);
		const std::string c = this->StartCopy(l, "\"");
		const Seg d0 = M.AddSeg(l, h, 2);
		M.AddSeg(l, Turn(h, sigma), 2, 2);
		M.AddSeg(c, Turn(h, sigma), 2, 2);
		const Seg d1 = M.AddSeg(c, h, 2);
		M.Tie(M.End(c), M.End(l));
		M.copies.push_back({c, l, d0.i, d0.i + 2});
		M.AddSeg(l, h, 2);
		M.AddSeg(l, Turn(h, -sigma), 2, 2);
		const auto x0 = xs_of(d0);
		const auto [n0, f0] = M.Deck(d0, x0, 4, 0);
		const auto x1 = xs_of(d1);
		const auto [n1, f1] = M.Deck(d1, x1, 0, 4);
		M.Add(Eq(f0 - n0 - (f1 - n1), 0));
		M.Add(Eq(Dot(STEP[h], d0.a) + n0 - Dot(STEP[h], d1.a) - n1, 2));
	}

	/** The PSG 227 join block (DESIGN-blocks 2): left bundle (fixed fA, choice cA) on lines 1, 2; right bundle (choice cB, fixed fB) on 5, 6. */
	void Block227(int h, const std::string &fA, const std::string &cA, const std::string &cB, const std::string &fB, const std::string &cA0, const std::string &cA1, const std::string &cB0, const std::string &cB1, const std::string &c0, const std::string &c1, const std::string &L0, const std::string &L1)
	{
		Model &M = this->M;
		const auto &r = Right(h);
		const int R = Turn(h, +1), Lt = Turn(h, -1);
		M.Add(Eq(Dot(r, M.End(cB)) - Dot(r, M.End(cA)), 6)); // lines 3 and 4 carry the crossing legs
		M.AddSeg(fA, h, 2 * this->P + 2); // R9: the fixed lanes straight through the prio coverage
		M.AddSeg(fB, h, 2 * this->P + 2);
		M.AddSeg(cA, h, 2);
		const Point sA = M.End(cA);
		M.AddSeg(cB, h, 2);
		const Point sB = M.End(cB);
		M.NewStrand(cA0, sA);
		M.NewStrand(cA1, sA);
		M.NewStrand(cB0, sB);
		M.NewStrand(cB1, sB);
		M.Node({cA, cA0, cA1});
		M.Node({cB, cB0, cB1});
		M.AddSeg(cA1, R, 2, 2); // cA1: up to line 3, bridge, down two lines onto line 5
		const Seg dA = M.AddSeg(cA1, h, 2);
		M.AddSeg(cA1, R, 4, 4);
		M.AddSeg(cB0, Lt, 2, 2); // cB0: up to line 4, run, down two lines under cA1's span onto line 2
		M.AddSeg(cB0, h, 2);
		const Seg xB = M.AddSeg(cB0, Lt, 4, 4);
		const std::pair<Var, int> x = M.Cross(dA, xB, 4);
		M.Deck(dA, {x});
		M.AddSeg(cA0, h, 2);
		M.Tie(M.End(cA0), M.End(cB0));
		const Point mC0 = M.End(cA0);
		M.AddSeg(cB1, h, 2);
		M.Tie(M.End(cB1), M.End(cA1));
		const Point mC1 = M.End(cB1);
		for (const std::string &leg : {cA0, cB1}) { // R5: split tile, a whole-train bay (2 TL) to the bay signal, then the merge tile
			Ex sum;
			for (const Seg &sg : M.strands.at(leg).segs) sum = sum + sg.L;
			M.Add(Ge(sum, 2 * this->TL + 4));
		}
		M.NewStrand(c0, mC0);
		M.Node({cA0, cB0, c0});
		M.NewStrand(c1, mC1);
		M.Node({cA1, cB1, c1});
		M.AddSeg(c0, h, 2);
		M.AddSeg(c0, Lt, 2, 2);
		M.Tie(M.End(c0), M.End(fA));
		const Point mL0 = M.End(fA);
		M.AddSeg(c1, h, 2);
		M.AddSeg(c1, R, 2, 2);
		M.Tie(M.End(c1), M.End(fB));
		const Point mL1 = M.End(fB);
		M.NewStrand(L0, mL0);
		M.Node({fA, c0, L0});
		M.NewStrand(L1, mL1);
		M.Node({c1, fB, L1});
		M.prio_rows.push_back({fA, mL0, h, -1});
		M.prio_rows.push_back({fB, mL1, h, +1});
		/* Out: the inside lane (L1, toward the arm's in port) runs straight to its port; the outside lane closes up beside it straight after its merge. */
		M.AddSeg(L1, h, 2 + STUB);
		/* The outside lane closes up as early as it can (D317, D320): up to two steps, each optional. */
		M.AddSeg(L0, h, 2, std::nullopt, FANW); // merge signal room
		M.AddSeg(L0, R, 2, std::nullopt, 1.0, true);
		M.AddSeg(L0, h, 0, std::nullopt, FANW);
		M.AddSeg(L0, R, 2, std::nullopt, 1.0, true);
		M.AddSeg(L0, h, STUB);
		M.Add(Eq(Dot(r, M.End(L1)) - Dot(r, M.End(L0)), 2));
	}

	/**
	 * The D320 fray. Each lane: port stub, an optional step left (D313 fans left), a run (spread costs FANW), the split. The near lane's straight leg may take the step after its split instead (split before the fan); the diverging legs leave on the sigma diagonal and continue into their bundle (its first axis piece optional). dform: the diverging legs leave on the far side of the straight near leg and both cross it under one deck (the diagonal fray).
	 */
	void Fray(int h, int sigma, const std::string &far, const std::string &near, const std::string &S_far, const std::string &S_near, const std::string &V_far, const std::string &V_near, bool prio, bool dform = false)
	{
		Model &M = this->M;
		const int Lt = Turn(h, -1);
		const auto &r = Right(h);
		for (const std::string &x : {far, near}) {
			M.AddSeg(x, h, STUB);
			M.AddSeg(x, Lt, 2, std::nullopt, 1.0, true);
			M.AddSeg(x, h, 0, std::nullopt, FANW);
		}
		const Point sf = M.End(far), sn = M.End(near);
		M.NewStrand(S_far, sf);
		M.NewStrand(V_far, sf);
		M.Node({far, S_far, V_far});
		M.NewStrand(S_near, sn);
		M.NewStrand(V_near, sn);
		M.Node({near, S_near, V_near});
		if (dform) {
			if (Lt != Turn(h, -sigma) || prio) M.Fail("the diagonal fray needs its diverging side on the left and no prio");
			M.Add(Eq(Dot(r, sn) - Dot(r, sf), 4 * sigma)); // a gap line between the lanes for the near diverging leg
			M.AddSeg(V_near, Lt, 2, 2); // into the gap line
			M.AddSeg(V_near, h, 2);
			M.AddSeg(S_far, h, 2);
			const Seg dn = M.AddSeg(S_near, h, 2); // its deck spans both diverging diagonals (added after V's bundle)
			M.AddSeg(S_near, h, 2);
			M.pending.emplace_back(dn, V_far, V_near);
			return;
		}
		M.Add(Ge((Dot(r, sn) - Dot(r, sf)) * sigma, prio ? 4 : 2));
		M.AddSeg(S_far, h, 2);
		const Seg vf = M.AddSeg(V_far, Turn(h, sigma), 2);
		M.AddSeg(V_near, Turn(h, sigma), 2);
		if (prio) { // near lane on line 2: the parallelogram steps inward to line 1 (V_far's diagonal meets line 1 first, D118)
			M.AddSeg(S_near, h, 4); // split tile, one signal tile, then the copy split
			const std::string c = this->StartCopy(S_near, "'");
			const Seg d0 = M.AddSeg(S_near, h, 2); // copy 0 deck (line 2)
			M.AddSeg(S_near, Turn(h, -sigma), 2, 2); // copy 0 bends after its bridge
			M.AddSeg(c, Turn(h, -sigma), 2, 2); // copy 1 bends before its bridge
			const Seg d1 = M.AddSeg(c, h, 2); // copy 1 deck (line 1), runs to the merge
			M.Tie(M.End(c), M.End(S_near));
			M.copies.push_back({c, S_near, d0.i, d0.i + 2});
			M.AddSeg(S_near, h, 2); // merge signal room; now beside S_far, no shift back
			const auto x0 = M.Cross(d0, vf, 4);
			const auto [n0, f0] = M.Deck(d0, {x0}, 4, 0); // D145 tight heads at both ends
			const auto x1 = M.Cross(d1, vf, 4);
			const auto [n1, f1] = M.Deck(d1, {x1}, 0, 4);
			M.Add(Eq(f0 - n0 - (f1 - n1), 0)); // D118: equal bridges
			M.Add(Eq(Dot(STEP[h], d0.a) + n0 - Dot(STEP[h], d1.a) - n1, 2)); // natural D118 stagger
		} else {
			M.AddSeg(S_near, h, 2);
			M.AddSeg(S_near, Lt, 2, std::nullopt, 1.0, true); // the near lane's step after its split (split before the fan)
			const Seg d0 = M.AddSeg(S_near, h, 2);
			const auto x0 = M.Cross(d0, vf, 4);
			M.Deck(d0, {x0}, std::nullopt, std::nullopt, 2); // split or bend, one signal tile, then the head
		}
	}

	/** Straight bundle S, joining bundle J from side sigma. Ends the in-strands; starts the out-strands. */
	void Join(int h, int sigma, const std::string &S_far, const std::string &S_near, const std::string &J_far, const std::string &J_near, const std::string &O_far, const std::string &O_near, bool prio)
	{
		Model &M = this->M;
		const auto &r = Right(h);
		/* The far fixed lane runs to its merge; J_far's diagonal lands on it. */
		M.AddSeg(S_far, h, prio ? 2 * this->P + 2 : 2);
		M.AddSeg(J_far, h, 2);
		const Seg jf = M.AddSeg(J_far, Turn(h, -sigma), 6, 6);
		const Point mf = M.End(S_far);
		M.Tie(M.End(J_far), mf);
		if (prio) {
			M.AddSeg(S_near, h, 2);
			const std::string c = this->StartCopy(S_near, "''");
			const Seg d0 = M.AddSeg(S_near, h, 2);
			M.AddSeg(S_near, Turn(h, sigma), 2, 2);
			M.AddSeg(c, Turn(h, sigma), 2, 2);
			const Seg d1 = M.AddSeg(c, h, 2);
			M.Tie(M.End(c), M.End(S_near));
			M.copies.push_back({c, S_near, d0.i, d0.i + 2});
			M.AddSeg(S_near, h, 2 * this->P + 2); // R9: no bend in the prio coverage
			const auto x0 = M.Cross(d0, jf, 4);
			const auto [n0, f0] = M.Deck(d0, {x0}, 4, 0); // D145 tight heads at both ends
			const auto x1 = M.Cross(d1, jf, 4);
			const auto [n1, f1] = M.Deck(d1, {x1}, 0, 4);
			M.Add(Eq(f0 - n0 - (f1 - n1), 0)); // D118: equal bridges
			M.Add(Eq(Dot(STEP[h], d0.a) + n0 - Dot(STEP[h], d1.a) - n1, 2)); // natural D118 stagger
			M.AddSeg(J_near, h, 2);
			M.AddSeg(J_near, Turn(h, -sigma), 4, 4);
		} else {
			const Seg d0 = M.AddSeg(S_near, h, 2);
			const auto x0 = M.Cross(d0, jf, 4);
			M.Deck(d0, {x0}, std::nullopt, std::nullopt, 2, 4); // the far head, one signal tile, then the merge tile
			M.AddSeg(J_near, h, 2);
			M.AddSeg(J_near, Turn(h, -sigma), 6, 6);
		}
		const Point mn = M.End(S_near);
		M.Tie(M.End(J_near), mn);
		M.NewStrand(O_far, mf);
		M.Node({S_far, J_far, O_far});
		M.NewStrand(O_near, mn);
		M.Node({S_near, J_near, O_near});
		if (prio) {
			M.prio_rows.push_back({S_far, mf, h, -sigma});
			M.prio_rows.push_back({S_near, mn, h, -sigma});
			M.AddSeg(O_near, h, 2);
			M.AddSeg(O_near, Turn(h, -sigma), 2, 2);
		}
		std::map<std::string, Seg> st;
		for (const std::string &o : {O_far, O_near}) { // D320: each out lane steps toward the arm's other direction on its own, optional
			M.AddSeg(o, h, 2, std::nullopt, FANW);
			st.emplace(o, M.AddSeg(o, Turn(h, +1), 2, std::nullopt, 1.0, true));
			M.AddSeg(o, h, STUB);
		}
		const bool far_first = Turn(h, +1) == Turn(h, sigma); // the lane stepping toward the other steps no earlier
		const Seg &a = far_first ? st.at(O_far) : st.at(O_near);
		const Seg &b = far_first ? st.at(O_near) : st.at(O_far);
		M.Add(Ge(Dot(STEP[h], a.a) - Dot(STEP[h], b.a), 0));
		M.Add(Eq(Dot(r, M.End(O_near)) - Dot(r, M.End(O_far)), 2 * sigma));
	}

	/** A run with an optional S-bend either way (D320): per lane free lengths; offset-parallel on its last piece. */
	void Flex(const std::string &name, const std::vector<std::string> &lanes, int h)
	{
		Model &M = this->M;
		Bundle b;
		b.lanes = lanes;
		for (const std::string &l : lanes) {
			std::vector<Seg> s;
			s.push_back(M.AddSeg(l, h, 0));
			s.push_back(M.AddSeg(l, Turn(h, -1), 2, std::nullopt, std::sqrt(2.0), true));
			s.push_back(M.AddSeg(l, h, 0));
			s.push_back(M.AddSeg(l, Turn(h, +1), 2, std::nullopt, std::sqrt(2.0), true));
			s.push_back(M.AddSeg(l, h, 0));
			b.segs[l] = std::move(s);
		}
		const auto &r = Right(h);
		for (size_t j = 1; j < lanes.size(); j++) {
			for (const bool end : {false, true}) {
				const Seg &sj = b.segs[lanes[j]][4];
				const Seg &s0 = b.segs[lanes[0]][4];
				M.Add(Eq(Dot(r, end ? sj.b : sj.a) - Dot(r, end ? s0.b : s0.a), 2 * static_cast<int>(j)));
			}
		}
		this->bundles.emplace_back(name, std::move(b));
	}

	/** A bundle: lanes left to right; each lane continues with one segment per heading; offset-parallel. */
	const std::map<std::string, std::vector<Seg>> &MakeBundle(const std::string &name, const std::vector<std::string> &lanes, const std::vector<int> &heads, bool opt0 = false)
	{
		Model &M = this->M;
		Bundle b;
		b.lanes = lanes;
		for (const std::string &l : lanes) b.segs[l] = {};
		for (size_t k = 0; k < heads.size(); k++) {
			const int h = heads[k];
			const auto &r = Right(h);
			for (const std::string &l : lanes) b.segs[l].push_back(M.AddSeg(l, h, 0, std::nullopt, IsAxis(h) ? 1.0 : std::sqrt(2.0), opt0 && k == 0)); // a turn's diagonal only as long as it must be
			for (size_t j = 1; j < lanes.size(); j++) {
				const Seg &sj = b.segs[lanes[j]][k];
				const Seg &s0 = b.segs[lanes[0]][k];
				M.Add(Eq(Dot(r, sj.a) - Dot(r, s0.a), 2 * static_cast<int>(j)));
				M.Add(Eq(Dot(r, sj.b) - Dot(r, s0.b), 2 * static_cast<int>(j)));
			}
		}
		this->bundles.emplace_back(name, std::move(b));
		return this->bundles.back().second.segs;
	}

	/** @return The first segment of the bundle that holds a lane. */
	const Seg &FirstSeg(const std::string &lane) const
	{
		for (const auto &[name, b] : this->bundles) {
			if (b.segs.contains(lane)) return b.segs.at(lane)[0];
		}
		NOT_REACHED();
	}

	/** The core crossings of a deck: the F lines' last segments and the D lines' third segment, each as a list. */
	std::pair<std::vector<std::pair<Var, int>>, std::vector<std::pair<Var, int>>> CoreXs(const Seg &dk, const std::map<std::string, std::vector<Seg>> &sF, const std::map<std::string, std::vector<Seg>> &sD)
	{
		std::vector<std::pair<Var, int>> xs_F, xs_D;
		for (const char *f : {"F1", "F2"}) xs_F.push_back(this->M.Cross(dk, sF.at(f).back(), 3));
		for (const char *d : {"D1", "D2"}) xs_D.push_back(this->M.Cross(dk, sD.at(d)[2], 3));
		return {xs_F, xs_D};
	}

	/** The core decks of an over run: F's crossings with the near head free and the far tail fixed, D's the other way round. */
	std::array<Ex, 4> CoreDecks(const Seg &dk, std::optional<int> first, std::optional<int> last, const std::map<std::string, std::vector<Seg>> &sF, const std::map<std::string, std::vector<Seg>> &sD)
	{
		Model &M = this->M;
		const auto [xs_F, xs_D] = this->CoreXs(dk, sF, sD);
		const auto [nF, fF] = M.Deck(dk, xs_F, std::nullopt, last);
		const auto [nD, fD] = M.Deck(dk, xs_D, first, std::nullopt);
		M.Add(Ge(nF - fD, 2));
		return {fF - nF, fD - nD, HeadU(dk, nF), HeadU(dk, nD)};
	}

	/** A pair of doubled lanes' decks in the core: the copy-0 and copy-1 decks, equal bridges, copy 1 one tile upstream. */
	void CoreDouble(const Seg &d0, const Seg &d1, const std::map<std::string, std::vector<Seg>> &sF, const std::map<std::string, std::vector<Seg>> &sD)
	{
		const auto sp0 = this->CoreDecks(d0, 4, 0, sF, sD);
		const auto sp1 = this->CoreDecks(d1, 0, 4, sF, sD);
		this->M.Add(Eq(sp0[0] - sp1[0], 0));
		this->M.Add(Eq(sp0[1] - sp1[1], 0));
		this->M.Add(Eq(sp0[2] - sp1[2], -2));
		this->M.Add(Eq(sp0[3] - sp1[3], -2));
	}

	/** A lane of B doubled outward: the parallelogram with its core decks. */
	void DoubledB(const std::string &l, int sg, const std::map<std::string, std::vector<Seg>> &sF, const std::map<std::string, std::vector<Seg>> &sD)
	{
		Model &M = this->M;
		const int h = HW;
		M.AddSeg(l, h, 2);
		const std::string c = this->StartCopy(l, "\"");
		const Seg d0 = M.AddSeg(l, h, 2);
		M.AddSeg(l, Turn(h, sg), 2, 2);
		M.AddSeg(c, Turn(h, sg), 2, 2);
		const Seg d1 = M.AddSeg(c, h, 2);
		M.Tie(M.End(c), M.End(l));
		M.copies.push_back({c, l, d0.i, d0.i + 2});
		M.AddSeg(l, h, 2);
		M.AddSeg(l, Turn(h, -sg), 2, 2);
		this->CoreDouble(d0, d1, sF, sD);
	}

	/** Build the model of the variant. */
	void Build()
	{
		Model &M = this->M;
		const Params &pr = M.params;
		const bool t2 = this->t2;
		const double tuck = std::sqrt(2.0);

		for (const char *n : {"EF1", "EF2"}) this->PortIn(n, 'W');
		for (const char *n : {"AB1", "AB2"}) this->PortIn(n, 'E');
		for (const char *n : {"CD1", "CD2"}) this->PortIn(n, 'S');
		const std::map<char, std::string> ON = t2 ? std::map<char, std::string>{{'E', "ED"}, {'W', "CB"}, {'S', "AF"}} : std::map<char, std::string>{{'E', "EO"}, {'W', "WO"}, {'S', "SO"}};
		std::vector<std::pair<std::string, char>> outs;
		for (const char k : {'E', 'W', 'S'}) {
			for (const char n : {'1', '2'}) outs.emplace_back(ON.at(k) + n, k);
		}

		/* Frays. */
		this->Fray(HE, +1, "EF1", "EF2", "E1", "E2", "F1", "F2", t2, this->wdiag != 0);
		this->Fray(HW, -1, "AB2", "AB1", "B2", "B1", "A2", "A1", t2);
		if (this->fray_s > 0) {
			this->Fray(HN, +1, "CD1", "CD2", "C1", "C2", "D1", "D2", false);
		} else {
			this->Fray(HN, -1, "CD2", "CD1", "D2", "D1", "C2", "C1", false);
		}

		/* Bundles (lanes left to right of travel). A diverging bundle's first axis piece is optional (it may stay on the fray's diagonal). */
		this->Flex("E", {"E1", "E2"}, HE);
		const std::map<std::string, std::vector<Seg>> sF = this->wdiag != 0 ? this->MakeBundle("F", {"F1", "F2"}, {HSE, HS}) : this->MakeBundle("F", {"F1", "F2"}, {HE, HSE, HS}, true);
		const std::map<std::string, std::vector<Seg>> sD = this->MakeBundle("D", {"D1", "D2"}, {HN, HNE, HN, HNE, HE}, this->fray_s > 0);
		this->MakeBundle("C", {"C1", "C2"}, {HN, HNW, HW}, this->fray_s < 0);
		this->MakeBundle("A", {"A1", "A2"}, {HW, HSW, HS}, true);
		for (const auto &[dn, va, vb] : M.pending) {
			std::vector<std::pair<Var, int>> xs;
			for (const std::string &v : {va, vb}) xs.push_back(M.Cross(dn, this->FirstSeg(v), 4));
			M.Deck(dn, xs, std::nullopt, std::nullopt, 4);
		}
		/* B: run; B2 (prio) doubled outward, B1 (choice) single (D299); run. */
		this->Flex("B", {"B1", "B2"}, HW);
		if (t2) { // B: both lanes carry the prio route B, each doubled (copy outward, the parallelogram); mirrored pairs (D312)
			this->DoubledB("B1", -1, sF, sD);
			this->DoubledB("B2", +1, sF, sD);
			const Seg r1 = M.AddSeg("B1", HW, 0);
			const Seg r2 = M.AddSeg("B2", HW, 0);
			M.Add(Eq(Dot(Right(HW), r2.a) - Dot(Right(HW), r1.a), 2));
			/* F's southbound run over D's NE diagonal (S-bend form). */
			for (const char *f : {"F1", "F2"}) {
				const Seg &over = sF.at(f).back();
				std::vector<std::pair<Var, int>> xs;
				for (const char *d : {"D1", "D2"}) xs.push_back(M.Cross(over, sD.at(d)[1], 4));
				M.Deck(over, xs);
			}
			this->Join(HE, +1, "E1", "E2", "D1", "D2", "ED1", "ED2", true);
			this->Join(HW, -1, "B2", "B1", "C2", "C1", "CB2", "CB1", true);
			if (this->join_s > 0) {
				this->Join(HS, +1, "A1", "A2", "F1", "F2", "AF1", "AF2", false);
			} else {
				this->Join(HS, -1, "F2", "F1", "A2", "A1", "AF2", "AF1", false);
			}
		} else {
			const Seg b1 = M.AddSeg("B1", HW, 2);
			this->CoreDecks(b1, std::nullopt, std::nullopt, sF, sD);
			this->DoubledB("B2", +1, sF, sD);
			const Seg r1 = M.AddSeg("B1", HW, 0);
			const Seg r2 = M.AddSeg("B2", HW, 0);
			M.Add(Eq(Dot(Right(HW), r2.a) - Dot(Right(HW), r1.a), 2));
			/* F's southbound run over D's NE diagonal; F2 (prio) doubled outward. */
			const auto xs_of = [&](const Seg &dk) {
				std::vector<std::pair<Var, int>> xs;
				for (const char *d : {"D1", "D2"}) xs.push_back(M.Cross(dk, sD.at(d)[1], 4));
				return xs;
			};
			const Seg f1 = M.AddSeg("F1", HS, 2);
			const auto xf1 = xs_of(f1);
			M.Deck(f1, xf1);
			this->Doubled("F2", HS, +1, xs_of);
			const Seg fr1 = M.AddSeg("F1", HS, 0);
			const Seg fr2 = M.AddSeg("F2", HS, 0);
			M.Add(Eq(Dot(Right(HS), fr2.a) - Dot(Right(HS), fr1.a), 2));
			/* Joins: the 227 block at every exit (fixed lanes outside, choice lanes inside). */
			this->Block227(HE, "E1", "E2", "D1", "D2", "E2a", "E2b", "D1a", "D1b", "EDj1", "EDj2", "EO1", "EO2");
			this->Block227(HW, "C1", "C2", "B1", "B2", "C2a", "C2b", "B1a", "B1b", "CBj1", "CBj2", "WO1", "WO2");
			this->Block227(HS, "A1", "A2", "F1", "F2", "A2a", "A2b", "F1a", "F1b", "AFj1", "AFj2", "SO1", "SO2");
		}

		/* Out ports on the boundary. */
		for (const auto &[n, side] : outs) {
			const Point u = M.End(n);
			if (side == 'W') M.Add(Eq(u[0], 0));
			if (side == 'E') M.Add(Eq(u[0] - M.X, 0));
			if (side == 'S') M.Add(Eq(u[1] - M.Y, 0));
		}
		/* Port order per side (reading order), adjacent within a port. */
		const auto vv = [&](const std::string &n, bool start) { return start ? M.strands.at(n).start : M.End(n); };
		using Seq = std::vector<std::pair<std::string, bool>>;
		const std::vector<std::pair<char, Seq>> order = {
			{'W', Seq{{"EF1", true}, {"EF2", true}, {ON.at('W') + "2", false}, {ON.at('W') + "1", false}}},
			{'E', Seq{{ON.at('E') + "1", false}, {ON.at('E') + "2", false}, {"AB2", true}, {"AB1", true}}},
			{'S', Seq{{"CD1", true}, {"CD2", true}, {ON.at('S') + "2", false}, {ON.at('S') + "1", false}}}};
		for (const auto &[side, seq] : order) {
			const size_t k = side == 'S' ? 0 : 1;
			for (size_t idx = 0; idx + 1 < seq.size(); idx++) {
				const Point pa = vv(seq[idx].first, seq[idx].second);
				const Point pb = vv(seq[idx + 1].first, seq[idx + 1].second);
				M.Add(Eq(pb[k] - pa[k], idx == 1 ? 4 : 2)); // LL_RR: lanes adjacent, one tile between the directions
				if (idx == 1) { // between the arm's two directions: a gap variable the probe keeps, at no cost
					const Var g = M.NewVar(0.0, INF, 0.0);
					M.Add(Eq(g - (pb[k] - pa[k]), 0));
				}
			}
		}
		/* Box. */
		for (const std::string &name : M.order) {
			const Strand &s = M.strands.at(name);
			std::vector<Point> pts{s.start};
			for (const Seg &sg : s.segs) pts.push_back(sg.b);
			for (const Point &p : pts) {
				M.Add(Ge(p[0], 0));
				M.Add(Ge(M.X - p[0], 0));
				M.Add(Ge(p[1], 1));
				M.Add(Ge(M.Y - p[1], 0));
			}
		}
		/* Prio rows (parallel unused track on side, P tiles before the merge): inside the box, unrelated to all. */
		for (const PrioRow &pr_row : std::vector<PrioRow>(M.prio_rows)) {
			const auto &r = Right(pr_row.h);
			const int dx = STEP[pr_row.h][0], dy = STEP[pr_row.h][1];
			const std::array<Ex, 2> start = {pr_row.m[0] - dx * 2 * this->P + pr_row.side * r[0] * 2, pr_row.m[1] - dy * 2 * this->P + pr_row.side * r[1] * 2};
			const std::string nm = "prio:" + pr_row.strand;
			const Point p0 = M.NewPoint();
			M.NewStrand(nm, p0);
			M.Tie(M.strands.at(nm).start, start);
			M.AddSeg(nm, pr_row.h, 2 * this->P, 2 * this->P, 0);
			for (const Point &p : {M.strands.at(nm).start, M.End(nm)}) {
				M.Add(Ge(p[0], 0));
				M.Add(Ge(M.X - p[0], 0));
				M.Add(Ge(p[1], 1));
				M.Add(Ge(M.Y - p[1], 0));
			}
		}
		M.routes = t2 ? &T2Routes() : &LL2Routes();
		M.outline_routes = t2 ? &T2Outline() : &LL2Outline();
		this->RouteCurves(*M.routes, pr, tuck);
	}

	/**
	 * JunctionCurveGaps on every route: curves k apart at least min(2TL-1, k*CL), consecutive opposite hands 2, same hand G. A curve at an optional segment exists only with it (z); a row between two curves holds only when both exist, and the consecutive form tightens to the k=2 form when an optional curve between them exists.
	 */
	void RouteCurves(const RouteList &routes, const Params &pr, double)
	{
		Model &M = this->M;
		std::set<std::pair<std::vector<std::pair<uint32_t, double>>, double>> seen;
		const int K2 = std::min(2 * pr.tl - 1, 2 * pr.cl);
		const auto addrow = [&](const Ex &e) {
			std::vector<std::pair<uint32_t, double>> key(e.terms.begin(), e.terms.end());
			if (seen.emplace(std::move(key), e.constant).second) M.Add(Ge(e, 0));
		};
		for (const auto &route : routes) {
			for (const std::vector<Seg> &seq : RouteSegs(M, route)) {
				struct Curve {
					Ex at;
					bool left_hand;
					std::optional<Var> z;
				};
				std::vector<Curve> curves;
				Ex acc;
				for (size_t k = 0; k + 1 < seq.size(); k++) {
					const Seg &a = seq[k], &b = seq[k + 1];
					acc = acc + a.L;
					if (a.d != b.d) {
						if (std::abs(Rot(a.d, b.d)) != 1) M.Fail("a route turns by more than one eighth");
						if (a.z.has_value() && b.z.has_value()) M.Fail("adjacent optional segments");
						curves.push_back({acc, Rot(a.d, b.d) > 0, a.z.has_value() ? a.z : b.z});
					}
				}
				for (size_t i = 0; i < curves.size(); i++) {
					int mand = 0;
					std::vector<Var> opts;
					for (size_t j = i + 1; j < curves.size(); j++) {
						if (mand >= 3) break;
						const Ex e = curves[j].at - curves[i].at; // a sum of lengths, never negative: a row lapses with a relaxation of its own gap
						const auto relaxed = [&](double gap) {
							Ex out = e - gap;
							for (const auto *z : {&curves[i].z, &curves[j].z}) {
								if (z->has_value()) out = out + (Ex(1) - **z) * gap;
							}
							return out;
						};
						if (mand >= 1) {
							addrow(relaxed(std::min(2 * pr.tl - 1, (mand + 1) * pr.cl)));
						} else {
							const int g = curves[i].left_hand != curves[j].left_hand ? 2 : pr.g;
							addrow(relaxed(g));
							for (const Var &zm : opts) addrow(relaxed(K2) + (Ex(1) - zm) * (K2 - g));
						}
						if (!curves[j].z.has_value()) {
							mand++;
						} else {
							opts.push_back(*curves[j].z);
						}
					}
				}
			}
		}
	}

public:
	/**
	 * Every segment sequence of a route: its strands in order, each doubled stretch by either copy.
	 * @param M The model.
	 * @param route The route's strand names.
	 * @return The sequences, the later strands' choices varying fastest.
	 */
	static std::vector<std::vector<Seg>> RouteSegs(const Model &M, const std::vector<std::string> &route)
	{
		std::vector<std::vector<Seg>> variants{{}};
		for (const std::string &st : route) {
			std::vector<const CopyRec *> cps;
			for (const CopyRec &c : M.copies) {
				if (c.strand == st) cps.push_back(&c);
			}
			std::ranges::sort(cps, [](const CopyRec *x, const CopyRec *y) { return std::tie(x->first, x->resume, x->copy) < std::tie(y->first, y->resume, y->copy); });
			std::vector<std::vector<Seg>> opts;
			const std::vector<Seg> &segs = M.strands.at(st).segs;
			for (uint32_t mask = 0; mask < (1u << cps.size()); mask++) {
				std::map<uint32_t, const CopyRec *> chosen;
				for (size_t b = 0; b < cps.size(); b++) {
					if (mask >> b & 1) chosen[cps[b]->first] = cps[b];
				}
				std::vector<Seg> seq;
				size_t i = 0;
				while (i < segs.size()) {
					const auto it = chosen.find(static_cast<uint32_t>(i));
					if (it != chosen.end()) {
						const std::vector<Seg> &part = M.strands.at(it->second->copy).segs;
						seq.insert(seq.end(), part.begin(), part.end());
						i = it->second->resume;
					} else {
						seq.push_back(segs[i]);
						i++;
					}
				}
				opts.push_back(std::move(seq));
			}
			std::vector<std::vector<Seg>> next;
			for (const auto &v : variants) {
				for (const auto &o : opts) {
					std::vector<Seg> joined = v;
					joined.insert(joined.end(), o.begin(), o.end());
					next.push_back(std::move(joined));
				}
			}
			variants = std::move(next);
		}
		return variants;
	}
};

/** One segment's geometry at a solution: start point, length, heading. */
struct Geo {
	int u;
	int v;
	int L;
	int d;
};

/** A segment's identity: strand name and index. */
using SegKey = std::pair<std::string, uint32_t>;

/** A deck at a solution: its segment, near head end and far head start. */
struct DeckAt {
	SegKey key;
	int n;
	int f;
};

/** The geometry of every segment and deck at a solution. */
struct GeoSet {
	std::map<SegKey, Geo> geo;
	std::vector<DeckAt> decks;
};

/** @return The geometry of a solution. */
GeoSet GeometryOf(const Model &M, const std::vector<int64_t> &val)
{
	GeoSet g;
	for (const std::string &name : M.order) {
		for (const Seg &s : M.strands.at(name).segs) {
			g.geo[{name, s.i}] = Geo{static_cast<int>(std::llround(Ev(val, s.a[0]))), static_cast<int>(std::llround(Ev(val, s.a[1]))), static_cast<int>(std::llround(Ev(val, s.L))), s.d};
		}
	}
	for (const DeckRec &d : M.decks) g.decks.push_back({{d.over.strand, d.over.i}, static_cast<int>(std::llround(Ev(val, d.n))), static_cast<int>(std::llround(Ev(val, d.f)))});
	return g;
}

/** A piece of a segment as the clash check sees it. */
struct Occ {
	std::string name;
	uint32_t i;
	PieceKind kind;
	std::array<int, 2> a;
	std::array<int, 2> b;
};

/** The pairs of segments that clash, sorted. */
using ClashList = std::vector<std::pair<SegKey, SegKey>>;

/**
 * The clashes of unrelated strands at a solution: two pieces on one tile unless they are the opposite diagonals of one tile, or two pieces sharing a point. Deck middles over ground are free, heads are track.
 * @param M The model.
 * @param g The geometry.
 * @return The clashing pairs.
 */
ClashList Clashes(const Model &M, const GeoSet &g)
{
	std::map<SegKey, std::vector<std::pair<int, int>>> dk;
	for (const DeckAt &d : g.decks) dk[d.key].emplace_back(d.n, d.f);
	std::map<std::pair<int, int>, std::vector<Occ>> occ;
	for (const auto &[key, geo] : g.geo) {
		const auto decks = dk.find(key);
		for (const Pc &pc : Pieces(geo.u, geo.v, geo.L, geo.d)) {
			bool inside = false, is_head = false;
			if (decks != dk.end()) {
				for (const auto &[n, f] : decks->second) {
					if (n <= pc.t && pc.t + 2 <= f) inside = true;
					if (n <= pc.t && pc.t + 2 <= f && (pc.t == n || pc.t + 2 == f)) is_head = true;
				}
			}
			if (inside) {
				for (const auto &[n, f] : decks->second) is_head = is_head || pc.t == n || pc.t + 2 == f;
				if (!is_head) continue;
			}
			occ[{pc.tx, pc.ty}].push_back({key.first, key.second, pc.kind, pc.a, pc.b});
		}
	}
	const auto unrelated = [&](const std::string &p, const std::string &q) {
		if (p == q) return false;
		return !M.related.contains(p < q ? std::make_pair(p, q) : std::make_pair(q, p));
	};
	const auto ok_pair = [](PieceKind x, PieceKind y) {
		return (x == PieceKind::NW && y == PieceKind::SE) || (x == PieceKind::SE && y == PieceKind::NW) || (x == PieceKind::NE && y == PieceKind::SW) || (x == PieceKind::SW && y == PieceKind::NE);
	};
	std::set<std::pair<SegKey, SegKey>> bad;
	std::map<std::pair<int, int>, std::set<SegKey>> pts;
	for (const auto &[tile, lst] : occ) {
		for (size_t x = 0; x < lst.size(); x++) {
			for (size_t y = x + 1; y < lst.size(); y++) {
				const Occ &o1 = lst[x], &o2 = lst[y];
				if (!unrelated(o1.name, o2.name)) continue;
				if (ok_pair(o1.kind, o2.kind)) continue;
				SegKey k1{o1.name, o1.i}, k2{o2.name, o2.i};
				bad.insert(k2 < k1 ? std::make_pair(k2, k1) : std::make_pair(k1, k2));
			}
		}
		for (const Occ &o : lst) {
			for (const auto &pt : {o.a, o.b}) pts[{pt[0], pt[1]}].insert({o.name, o.i});
		}
	}
	for (const auto &[pt, s] : pts) {
		for (auto x = s.begin(); x != s.end(); ++x) {
			for (auto y = std::next(x); y != s.end(); ++y) {
				if (unrelated(x->first, y->first)) bad.insert({*x, *y});
			}
		}
	}
	return ClashList(bad.begin(), bad.end());
}

/** The eight separating directions in the probe's order. */
constexpr std::array<std::array<int, 2>, 8> DIRS8 = {{{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {-1, -1}, {1, -1}, {-1, 1}}};

/**
 * The ways to separate two clashing segments, best first: along a direction, every end of one at least 2 beyond every end of the other.
 * @param M The model.
 * @param g The geometry.
 * @param pr The clashing pair.
 * @return The (direction, rows) options in order.
 */
std::vector<std::vector<Row>> SepOptions(const Model &M, const GeoSet &g, const std::pair<SegKey, SegKey> &pr)
{
	const Seg &sp = M.strands.at(pr.first.first).segs[pr.first.second];
	const Seg &sq = M.strands.at(pr.second.first).segs[pr.second.second];
	const auto ends = [&](const SegKey &key) {
		const Geo &geo = g.geo.at(key);
		return std::array<std::array<int, 2>, 2>{{{geo.u, geo.v}, {geo.u + STEP[geo.d][0] * geo.L, geo.v + STEP[geo.d][1] * geo.L}}};
	};
	const auto ep = ends(pr.first), eq = ends(pr.second);
	const auto dot = [](const std::array<int, 2> &f, const std::array<int, 2> &p) { return f[0] * p[0] + f[1] * p[1]; };
	std::vector<std::tuple<int, int, int>> opts; // (-cur, fx, fy)
	for (const auto &f : DIRS8) {
		const int cur = std::min(dot(f, eq[0]), dot(f, eq[1])) - std::max(dot(f, ep[0]), dot(f, ep[1]));
		opts.emplace_back(-cur, f[0], f[1]);
	}
	std::ranges::sort(opts);
	std::vector<std::vector<Row>> out;
	for (const auto &[c, fx, fy] : opts) {
		const std::array<int, 2> f{fx, fy};
		std::vector<Row> rows;
		for (const Point *a : {&sp.a, &sp.b}) {
			for (const Point *b : {&sq.a, &sq.b}) rows.push_back(Model::MakeRow(Ge(Dot(f, *b) - Dot(f, *a), 2)));
		}
		out.push_back(std::move(rows));
	}
	return out;
}

/** The outline's points: the outer route between each pair of neighbouring arms, joined along the box edges. */
std::vector<Point> Outline(const Model &M)
{
	std::vector<Point> pts;
	for (const auto &route : *M.outline_routes) {
		const std::vector<Seg> seq = Composer::RouteSegs(M, route)[0];
		pts.push_back(seq[0].a);
		for (const Seg &sg : seq) pts.push_back(sg.b);
	}
	return pts;
}

/** @return The signed shoelace area of points, in half-tile squares. */
double AreaOf(const std::vector<int64_t> &val, const std::vector<Point> &pts)
{
	const size_t n = pts.size();
	double sum = 0;
	for (size_t i = 0; i < n; i++) {
		const Point &p = pts[i], &q = pts[(i + 1) % n];
		sum += Ev(val, p[0]) * Ev(val, q[1]) - Ev(val, q[0]) * Ev(val, p[1]);
	}
	return 0.5 * sum;
}

/** Set the objective coefficients of the area's gradient at a solution (tiles; shoelace, sign from the current orientation). */
void LineariseArea(Model &M, const std::vector<int64_t> &val)
{
	const std::vector<Point> pts = Outline(M);
	const size_t n = pts.size();
	std::vector<std::array<double, 2>> P_;
	for (const Point &p : pts) P_.push_back({Ev(val, p[0]), Ev(val, p[1])});
	const double sgn = AreaOf(val, pts) >= 0 ? 1 : -1;
	std::map<uint32_t, double> coef;
	for (size_t i = 0; i < n; i++) {
		const auto &next = P_[(i + 1) % n], &prev = P_[(i + n - 1) % n];
		const double gx = 0.5 * (next[1] - prev[1]);
		const double gy = 0.5 * (prev[0] - next[0]);
		for (const auto &[e, g] : {std::make_pair(&pts[i][0], gx), std::make_pair(&pts[i][1], gy)}) {
			for (const auto &[k, v] : e->terms) coef[k] += sgn * g * v * AREA_W / 4;
		}
	}
	M.obj_extra = std::move(coef);
}

/** What one lazy solve finds. */
struct LazyResult {
	Solution sol;
	GeoSet geo;
	std::vector<Row> seps;
};

using Clock = std::chrono::steady_clock;

/**
 * Solve with clashes separated lazily.
 * @param M The model.
 * @param t0 The variant's start, for the budget.
 * @param seps0 The separation rows found so far.
 * @param out The result.
 * @param why The reason on failure.
 * @return Whether a clash-free solution was found.
 */
bool SolveLazy1(Model &M, Clock::time_point t0, const std::vector<Row> &seps0, LazyResult &out, std::string &why)
{
	std::vector<Row> seps = seps0;
	Solution r;
	if (!M.Solve(seps, r)) {
		why = "base infeasible";
		return false;
	}
	for (int rnd = 0; rnd < LAZY_ROUNDS; rnd++) {
		const GeoSet g = GeometryOf(M, r.val);
		const ClashList bad = Clashes(M, g);
		/* An under track is at ground where it is crossed: not inside a deck (heads included) of its own segment. */
		std::vector<std::array<Row, 2>> under_bad;
		for (const CrossRec &c : M.crossings) {
			const double bv = Ev(r.val, c.b);
			for (const DeckRec &d : M.decks) {
				if (d.over.strand == c.under.strand && d.over.i == c.under.i && Ev(r.val, d.n) - 3 < bv && bv < Ev(r.val, d.f) + 3) {
					under_bad.push_back({Model::MakeRow(Le(c.b, d.n - 3)), Model::MakeRow(Ge(c.b, d.f + 3))});
				}
			}
		}
		if (bad.empty() && under_bad.empty()) {
			out = LazyResult{r, g, seps};
			return true;
		}
		if (!under_bad.empty()) {
			bool done = false;
			for (const Row &row : under_bad[0]) {
				std::vector<Row> trial = seps;
				trial.push_back(row);
				Solution probe;
				if (M.Solve(trial, probe)) {
					seps.push_back(row);
					done = true;
					break;
				}
			}
			if (!done) {
				why = "under-deck conflict not separable";
				return false;
			}
			if (!M.Solve(seps, r)) {
				why = "infeasible after separation";
				return false;
			}
			continue;
		}
		if (std::chrono::duration<double>(Clock::now() - t0).count() > BUDGET) {
			why = fmt::format("budget at round {}, {} clashes", rnd, bad.size());
			return false;
		}
		const std::vector<std::vector<Row>> options = SepOptions(M, g, bad[0]);
		bool separated = false;
		for (size_t k = 0; k < 4; k++) {
			std::vector<Row> trial = seps;
			trial.insert(trial.end(), options[k].begin(), options[k].end());
			Solution probe;
			if (M.Solve(trial, probe)) {
				seps = std::move(trial);
				separated = true;
				break;
			}
		}
		if (!separated) {
			why = "a clash is not separable";
			return false;
		}
		if (!M.Solve(seps, r)) {
			why = "infeasible after separation";
			return false;
		}
	}
	why = "rounds exhausted";
	return false;
}

/**
 * Area inside the outline by sequential linearisation around the lazy-separated solution.
 * @param M The model.
 * @param t0 The variant's start.
 * @param out The best result.
 * @param area The best area in tiles.
 * @param why The reason on failure.
 * @return Whether a solution was found.
 */
bool SolveLazy(Model &M, Clock::time_point t0, LazyResult &out, double &area, std::string &why)
{
	M.obj_extra.clear();
	if (!SolveLazy1(M, t0, {}, out, why)) return false;
	double best = std::abs(AreaOf(out.sol.val, Outline(M)));
	std::vector<Row> seps = out.seps;
	for (int it = 0; it < AREA_ROUNDS; it++) {
		LineariseArea(M, out.sol.val);
		LazyResult r2;
		std::string why2;
		if (!SolveLazy1(M, t0, seps, r2, why2)) break;
		const double a2 = std::abs(AreaOf(r2.sol.val, Outline(M)));
		if (a2 >= best - 1) break;
		best = a2;
		out = std::move(r2);
		seps = out.seps;
	}
	area = best / 4;
	return true;
}

/** What one variant gives. */
struct VariantOutcome {
	bool ok = false;
	std::string why;
	std::string tag;
	double area = 0;
	int width = 0;
	int height = 0;
	std::vector<PlanarRouteProgram> routes;
};

/**
 * Export the route programs of a solved variant: per route, per path, per copy choice, its pieces and copy switches.
 * @param M The model.
 * @param g The geometry.
 * @param topology The topology the programs index into.
 * @param routes The programs.
 * @param why The reason on failure.
 * @return Whether every route and path was found in the topology.
 */
bool ExportRoutes(const Model &M, const GeoSet &g, const PlanarTopology &topology, std::vector<PlanarRouteProgram> &routes, std::string &why)
{
	std::map<SegKey, std::vector<std::pair<int, int>>> deck_of;
	for (const DeckAt &d : g.decks) deck_of[d.key].emplace_back(d.n, d.f);
	for (const auto &route : *M.routes) {
		const std::string &name = route[1];
		uint32_t r = 0;
		while (r < topology.routes.size() && topology.routes[r].name != name) r++;
		std::string joined_route;
		for (const std::string &s : route) joined_route += (joined_route.empty() ? "" : ",") + s;
		if (r == topology.routes.size()) {
			why = "route " + name + " is not in the topology";
			return false;
		}
		uint32_t pi = 0;
		for (; pi < topology.routes[r].paths.size(); pi++) {
			std::string joined;
			for (const uint32_t l : topology.routes[r].paths[pi]) joined += (joined.empty() ? "" : ",") + topology.lines[l].name;
			if (joined == joined_route) break;
		}
		if (pi == topology.routes[r].paths.size()) {
			why = "path " + joined_route + " is not in the topology";
			return false;
		}
		/* Per route line, its choices of copy with the copy regions. */
		using Seq = std::vector<std::pair<Seg, uint8_t>>;
		using Regions = std::vector<std::pair<size_t, size_t>>;
		std::vector<std::vector<std::pair<Seq, Regions>>> per_line;
		for (size_t li = 0; li < route.size(); li++) {
			const std::string &st = route[li];
			std::vector<const CopyRec *> cps;
			for (const CopyRec &c : M.copies) {
				if (c.strand == st) cps.push_back(&c);
			}
			std::ranges::sort(cps, [](const CopyRec *x, const CopyRec *y) { return std::tie(x->first, x->resume, x->copy) < std::tie(y->first, y->resume, y->copy); });
			const std::vector<Seg> &segs = M.strands.at(st).segs;
			std::vector<std::pair<Seq, Regions>> opts;
			for (uint32_t mask = 0; mask < (1u << cps.size()); mask++) {
				std::set<uint32_t> chosen;
				std::map<uint32_t, const CopyRec *> region_at;
				for (size_t b = 0; b < cps.size(); b++) {
					if (mask >> b & 1) chosen.insert(cps[b]->first);
					region_at[cps[b]->first] = cps[b];
				}
				Seq seq;
				Regions regions;
				size_t i = 0;
				while (i < segs.size()) {
					const auto it = region_at.find(static_cast<uint32_t>(i));
					if (it != region_at.end()) {
						const CopyRec &c = *it->second;
						std::vector<Seg> part;
						if (chosen.contains(c.first)) {
							part = M.strands.at(c.copy).segs;
						} else {
							part.assign(segs.begin() + c.first, segs.begin() + c.resume);
						}
						regions.emplace_back(seq.size(), seq.size() + part.size() - 1);
						for (const Seg &s : part) seq.emplace_back(s, static_cast<uint8_t>(li));
						i = c.resume;
					} else {
						seq.emplace_back(segs[i], static_cast<uint8_t>(li));
						i++;
					}
				}
				opts.emplace_back(std::move(seq), std::move(regions));
			}
			per_line.push_back(std::move(opts));
		}
		std::vector<std::pair<Seq, Regions>> variants{{}};
		for (const auto &opts : per_line) {
			std::vector<std::pair<Seq, Regions>> next;
			for (const auto &v : variants) {
				for (const auto &o : opts) {
					Seq seq = v.first;
					seq.insert(seq.end(), o.first.begin(), o.first.end());
					Regions regions = v.second;
					for (const auto &[a, b] : o.second) regions.emplace_back(a + v.first.size(), b + v.first.size());
					next.emplace_back(std::move(seq), std::move(regions));
				}
			}
			variants = std::move(next);
		}
		if (variants.size() > 256) {
			why = "route " + name + " has more than 256 copy choices";
			return false;
		}
		for (size_t copy = 0; copy < variants.size(); copy++) {
			const auto &[seq, regions] = variants[copy];
			PlanarRouteProgram program;
			program.route = r;
			program.path = pi;
			program.copy = static_cast<uint8_t>(copy);
			std::vector<size_t> seg_first, seg_last;
			for (const auto &[s, li] : seq) {
				const Geo &geo = g.geo.at({s.strand, s.i});
				const auto dk = deck_of.find({s.strand, s.i});
				seg_first.push_back(program.pieces.size());
				for (const Pc &pc : Pieces(geo.u, geo.v, geo.L, geo.d)) {
					PlanarPiece::Kind kind = PlanarPiece::Kind::Ground;
					if (dk != deck_of.end()) {
						for (const auto &[n, f] : dk->second) {
							if (n <= pc.t && pc.t + 2 <= f) kind = (pc.t == n || pc.t + 2 == f) ? PlanarPiece::Kind::Head : PlanarPiece::Kind::Middle;
						}
					}
					program.pieces.push_back({{static_cast<int16_t>(pc.tx), static_cast<int16_t>(pc.ty)}, static_cast<uint8_t>(geo.d), kind, li});
				}
				seg_last.push_back(program.pieces.size() - 1);
			}
			for (const auto &[a, b] : regions) {
				program.copy_switches.emplace_back(true, static_cast<uint32_t>(seg_first[a]));
				program.copy_switches.emplace_back(false, static_cast<uint32_t>(seg_last[b]));
			}
			routes.push_back(std::move(program));
		}
	}
	return true;
}

/**
 * Build, solve and export one variant.
 * @param topology The topology.
 * @param params The spec facts.
 * @param t2 Whether T2, else LL².
 * @param variant The variant.
 * @return The outcome.
 */
VariantOutcome SolveVariant(const PlanarTopology &topology, const Params &params, bool t2, const Variant &variant)
{
	VariantOutcome out;
	out.tag = fmt::format("{:+d}F1w{}", variant.fray_s, variant.other);
	const Clock::time_point t0 = Clock::now();
	Composer composer(params, t2, variant);
	Model &M = composer.M;
	if (!M.error.empty()) {
		out.why = M.error;
		return out;
	}
	LazyResult res;
	if (!SolveLazy(M, t0, res, out.area, out.why)) return out;
	out.width = static_cast<int>(std::llround(Ev(res.sol.val, M.X))) / 2 + 1;
	out.height = static_cast<int>(std::llround(Ev(res.sol.val, M.Y))) / 2 + 1;
	if (!M.error.empty()) {
		out.why = M.error;
		return out;
	}
	if (!ExportRoutes(M, res.geo, topology, out.routes, out.why)) return out;
	out.ok = true;
	return out;
}

} // namespace

bool SolvePlanarComponents(const PlanarTopology &topology, const JunctionSpec &spec, PlanarComponentLayout &layout, std::string *why)
{
	const Clock::time_point t0 = Clock::now();
	const auto fail = [&](const std::string &what) {
		if (why != nullptr) *why = what;
		return false;
	};
	const bool t2 = topology.name == "T2";
	if (!t2 && topology.name != "LL2") return fail("no component composition for topology " + topology.name);
	for (const auto &route : t2 ? T2Routes() : LL2Routes()) {
		for (const std::string &line : route) {
			if (PlanarLineByName(topology, line) == UINT32_MAX) return fail("line " + line + " is not in the topology");
		}
	}
	Params params;
	params.tl = spec.tl;
	params.cl = static_cast<int>(DeriveCurveSpacing(spec.reference_speed, spec.curve_speed, spec.tl));
	params.p = spec.prio_tiles;
	params.g = std::min(2 * params.tl - 1, std::max(2, params.cl));
	std::vector<Variant> variants;
	if (t2) {
		variants = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
	} else {
		variants = {{1, 0}, {1, 1}, {-1, 0}, {-1, 1}};
	}
	std::vector<VariantOutcome> outcomes(variants.size());
	std::vector<std::thread> threads;
	for (size_t i = 0; i < variants.size(); i++) {
		threads.emplace_back([&, i] { outcomes[i] = SolveVariant(topology, params, t2, variants[i]); });
	}
	for (std::thread &thread : threads) thread.join();
	const VariantOutcome *best = nullptr;
	std::string reasons;
	for (const VariantOutcome &o : outcomes) {
		if (!o.ok) {
			reasons += " " + o.tag + ": " + o.why + ";";
			continue;
		}
		if (best == nullptr || o.area < best->area) best = &o;
	}
	if (best == nullptr) return fail("no variant solves:" + reasons);
	layout.width = best->width;
	layout.height = best->height;
	layout.area = best->area;
	layout.variant = best->tag;
	layout.routes = best->routes;
	layout.micros = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - t0).count();
	return true;
}
