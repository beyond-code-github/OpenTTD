/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file planar_components.cpp The planar component solver implementation (DESIGN-components §6, translated from wip/planar-probe8.py). */

#include "../../stdafx.h"

#include "planar_components.h"
#include "planar_model.h"
#include "planar_channels.h"
#include "planar_gaps.h"
#include "planar_paths.h"
#include "planar_regions.h"

#include "../junction_spec.h"
#include "../placement/junction_milp.h"
#include "../../3rdparty/fmt/format.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <cmath>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <unordered_map>

#include "../../safeguards.h"

namespace {

using namespace PlanarModel;

constexpr int STUB = 4; ///< Half-tiles of a port stub.
constexpr double FANW = 1.5; ///< Cost per half-tile of a fanned (spread) stretch; the tight stub costs 1.
constexpr double AREA_W = 10; ///< Objective weight per tile of area inside the outline.
constexpr double BUDGET = 15.0; ///< Seconds per variant for the lazy loop.
constexpr int LAZY_ROUNDS = 40; ///< Rounds of the lazy clash loop.
constexpr int AREA_ROUNDS = 6; ///< Rounds of the area linearisation under PLANAR_OBJ=loop; by default one round (D329: the same areas as the loop on T3).

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
		RouteCurves(M, *M.routes, pr, tuck);
	}

public:
	/**
	 * JunctionCurveGaps on every route: curves k apart at least min(2TL-1, k*CL), consecutive opposite hands 2, same hand G. A curve at an optional segment exists only with it (z); a row between two curves holds only when both exist, and the consecutive form tightens to the k=2 form when an optional curve between them exists.
	 */
	static void RouteCurves(Model &M, const RouteList &routes, const Params &pr, double)
	{
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
						if (std::abs(Rot(a.d, b.d)) != 1) M.Fail(fmt::format("a route turns by more than one eighth {}#{} d{} -> {}#{} d{}", a.strand, a.i, a.d, b.strand, b.i, b.d));
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
};

/** Whether a site is composed per node: a lane split or merge, or a site with other than two channels. */
static bool PlanarSitePerNode(const PlanarSite &site)
{
	return site.kind == PlanarSiteKind::LaneSplit || site.kind == PlanarSiteKind::LaneMerge || site.others.size() != 2;
}

/** One variant of a mapped composition: which channel is over at each open crossing, and which channel is straight at each tie site. */
struct MappedVariant {
	uint64_t over = 0; ///< Bit i set: the i-th open crossing has its higher channel over.
	uint64_t ties = 0; ///< Bit i set: the i-th site without a straight channel takes its second other channel as straight.
	uint64_t flips = 0; ///< Bit i set: the i-th crossing triangle sits the other way round (FlipPlanarTriangles).
	uint64_t steps = 0; ///< Bit i set: the i-th candidate step (PlanarCandidateSteps) is taken.
};

/**
 * Builds the model of one variant from the topology's channel graph (D327): probe 9's n-lane fray, join, doubled core and
 * bundles, placed by the channels, sites and paths instead of by a hand composition.
 */
class MappedComposer {
public:
	Model M;

	/**
	 * Build the model.
	 * @param topology The topology.
	 * @param channels Its channel graph.
	 * @param params The spec facts.
	 * @param variant The variant.
	 */
	MappedComposer(const PlanarTopology &topology, const PlanarChannels &channels, const Params &params, const MappedVariant &variant) : t(topology), ch(channels), P(params.p), variant(variant), sides(topology, channels)
	{
		this->M.params = params;
		this->M.deck_parity = true;
		this->Build();
	}

private:
	const PlanarTopology &t;
	const PlanarChannels &ch;
	int P;
	MappedVariant variant;
public:
	PlanarLineSides sides; ///< Which side of each other two lines keep, from the topology.

private:
	PlanarPaths paths;
	std::vector<std::map<std::string, std::vector<Seg>>> piece_segs; ///< Per channel, per lane, its segment for each path piece (bundled channels).
	RouteList routes; ///< Every route path as line names.
	RouteList outline; ///< The outline routes.
	std::map<size_t, uint32_t> cross_x; ///< Index into Model::crossings -> its channel crossing.
	std::map<size_t, uint32_t> deck_x; ///< Index into Model::decks -> the first channel crossing it spans.
	std::map<std::pair<uint32_t, uint32_t>, std::map<std::string, std::pair<Seg, Seg>>> dbl_decks; ///< Per (bundled channel, piece) over a crossing with doubled lanes, per lane its copy-0 and copy-1 decks.
	std::map<std::pair<uint32_t, uint32_t>, std::map<std::string, Seg>> piece_tails; ///< Per (channel, piece) holding its own decks, per lane its run after them (the run before them is the piece's segment).

	/** The names of a channel's lanes, left to right. */
	std::vector<std::string> Lanes(uint32_t c) const
	{
		std::vector<std::string> out;
		for (uint32_t l : this->ch.channels[c].lines) out.push_back(this->t.lines[l].name);
		return out;
	}

	/** Whether a prio route runs on a line. */
	bool LinePrio(uint32_t l) const
	{
		for (uint32_t r : this->t.lines[l].routes) {
			if (this->t.routes[r].prio) return true;
		}
		return false;
	}

	/** Whether a line carries a full lane's flow, so its bridges are doubled. */
	bool FullShare(uint32_t l) const
	{
		return this->ch.line_share[l] >= 1.0;
	}

	/** How many of a channel's lanes carry a full share. */
	int FullLanes(uint32_t c) const
	{
		int n = 0;
		for (uint32_t l : this->ch.channels[c].lines) n += this->FullShare(l) ? 1 : 0;
		return n;
	}

	/** A fork: a lane split's legs leave its in line's end, each with an optional outward diagonal, at least a line apart after it (which leg runs straight is the solver's). */
	void Fork(int h, const std::string &in, const std::string &left, const std::string &right)
	{
		Model &M = this->M;
		const Point p = M.End(in);
		M.NewStrand(left, p);
		M.NewStrand(right, p);
		M.Node({in, left, right});
		const Seg a = M.AddSeg(left, Turn(h, -1), 2, std::nullopt, 1.0, true);
		const Seg b = M.AddSeg(right, Turn(h, +1), 2, std::nullopt, 1.0, true);
		const TagScope ts("fork gap");
		M.Add(Ge(Dot(Right(h), b.b) - Dot(Right(h), a.b), 2));
	}

	/** The heading of the single port in a set, or -1. */
	int PortHeading(const std::set<uint32_t> &ports) const
	{
		return ports.size() == 1 ? static_cast<int>(this->t.ports[*ports.begin()].heading) : -1;
	}

	/** A turn in eighths, -4..3, positive clockwise. */
	static int TurnOf(int from, int to)
	{
		return (to - from + 12) % 8 - 4;
	}

	/** A port line at the box boundary. */
	void PortIn(const std::string &name, PlanarSide side)
	{
		this->M.NewStrand(name);
		const TagScope ts("port in");
		const Point u = this->M.strands.at(name).start;
		if (side == PlanarSide::W) this->M.Add(Eq(u[0], 0));
		if (side == PlanarSide::E) this->M.Add(Eq(u[0] - this->M.X, 0));
		if (side == PlanarSide::S) this->M.Add(Eq(u[1] - this->M.Y, 0));
		if (side == PlanarSide::N) this->M.Add(Eq(u[1], 1));
	}

	/** Start a copy strand of a lane at its end. */
	std::string StartCopy(const std::string &lane, const std::string &mark)
	{
		const Point cs = this->M.End(lane);
		const std::string c = lane + mark;
		this->M.NewStrand(c, cs);
		this->M.Node({lane, c});
		return c;
	}

	/** The parallelogram on a lane (copy toward sg): lane deck, step sg; copy steps sg, deck; the lane ends on its copy's line. */
	std::pair<Seg, Seg> Double(const std::string &l, int h, int sg, const std::string &suffix, int first)
	{
		Model &M = this->M;
		M.AddSeg(l, h, first);
		const std::string c = this->StartCopy(l, suffix);
		const Seg d0 = M.AddSeg(l, h, 2);
		M.AddSeg(l, Turn(h, sg), 2, 2);
		M.AddSeg(c, Turn(h, sg), 2, 2);
		const Seg d1 = M.AddSeg(c, h, 2);
		M.Tie(M.End(c), M.End(l));
		M.copies.push_back({c, l, d0.i, d0.i + 2});
		return {d0, d1};
	}

	/** Copy 0 and copy 1 decks over the same crossers: tight heads, equal bridges, copy 1 one tile upstream (D118, D145). */
	void PairDecks(const Seg &d0, const Seg &d1, const std::vector<Seg> &crossers, int h)
	{
		Model &M = this->M;
		const TagScope ts("pair deck");
		std::vector<std::pair<Var, int>> x0, x1;
		for (const Seg &v : crossers) {
			x0.push_back(M.Cross(d0, v, 4));
			x1.push_back(M.Cross(d1, v, 4));
		}
		const auto [n0, f0] = M.Deck(d0, x0, 4, 0);
		const auto [n1, f1] = M.Deck(d1, x1, 0, 4);
		M.Add(Eq(f0 - n0 - (f1 - n1), 0));
		M.Add(Eq(Dot(STEP[h], d0.a) + n0 - Dot(STEP[h], d1.a) - n1, 2));
	}

	/** n-lane fray (probe 9): lanes far..near; V_k crosses every nearer S_j; each crossed S_j decks over all farther V. */
	void Fray(int h, int sigma, const std::vector<std::string> &ins, const std::vector<std::string> &S, const std::vector<std::string> &V, bool prio)
	{
		Model &M = this->M;
		const int Lt = Turn(h, -1);
		const auto &r = Right(h);
		const size_t n = ins.size();
		for (const std::string &x : ins) {
			M.AddSeg(x, h, STUB);
			if (prio) M.AddSeg(x, Lt, 2, std::nullopt, 1.0);
			M.AddSeg(x, h, 0, std::nullopt, FANW);
		}
		std::vector<Point> sp;
		for (size_t k = 0; k < n; k++) {
			sp.push_back(M.End(ins[k]));
			M.NewStrand(S[k], sp.back());
			M.NewStrand(V[k], sp.back());
			M.Node({ins[k], S[k], V[k]});
		}
		for (size_t k = 1; k < n; k++) {
			const TagScope ts("fray gap");
			M.Add(Ge((Dot(r, sp[k]) - Dot(r, sp[k - 1])) * sigma, prio ? 4 : 2));
		}
		M.AddSeg(S[0], h, 2);
		std::vector<Seg> vs;
		for (const std::string &v : V) vs.push_back(M.AddSeg(v, Turn(h, sigma), 2));
		for (size_t j = 1; j < n; j++) {
			const std::vector<Seg> far_v(vs.begin(), vs.begin() + j);
			if (prio) {
				const auto [d0, d1] = this->Double(S[j], h, -sigma, "'", 4);
				M.AddSeg(S[j], h, 2);
				this->PairDecks(d0, d1, far_v, h);
			} else {
				M.AddSeg(S[j], h, 2);
				const Seg d0 = M.AddSeg(S[j], h, 2);
				std::vector<std::pair<Var, int>> xs;
				for (const Seg &v : far_v) xs.push_back(M.Cross(d0, v, 4));
				M.Deck(d0, xs, std::nullopt, std::nullopt, 2);
			}
		}
	}

	/** n-lane join (probe 9): J_k lands on S_k from side sigma, crossing every nearer S_j (lanes far..near). */
	void Join(int h, int sigma, const std::vector<std::string> &S, const std::vector<std::string> &J, const std::vector<std::string> &O, bool prio, std::optional<bool> exit_step, int lo0 = 6)
	{
		Model &M = this->M;
		const auto &r = Right(h);
		const size_t n = S.size();
		M.AddSeg(S[0], h, prio ? 2 * this->P + 2 : 2);
		std::vector<Seg> js;
		for (size_t k = 0; k < n; k++) {
			M.AddSeg(J[k], h, 2);
			const int lo = (prio && k > 0) ? 4 : (k == 0 ? lo0 : 6);
			js.push_back(M.AddSeg(J[k], Turn(h, -sigma), lo, n == 2 ? std::optional<int>(lo) : std::nullopt));
		}
		M.Tie(M.End(J[0]), M.End(S[0]));
		for (size_t j = 1; j < n; j++) {
			if (prio) {
				/* Joiners far from the merge cross before this lane's coverage, each P tiles apart: one doubled stretch per crossed joiner in series (D322). */
				for (size_t k = 0; k + 1 < j; k++) {
					const auto [d0, d1] = this->Double(S[j], h, sigma, "''" + std::string(k + 1, '\''), 2);
					M.AddSeg(S[j], h, 2);
					M.AddSeg(S[j], Turn(h, -sigma), 2, 2);
					this->PairDecks(d0, d1, {js[k]}, h);
				}
				const auto [d0, d1] = this->Double(S[j], h, sigma, "''", 2);
				M.AddSeg(S[j], h, 2 * this->P + 2);
				this->PairDecks(d0, d1, {js[j - 1]}, h);
			} else {
				const Seg d0 = M.AddSeg(S[j], h, 2);
				std::vector<std::pair<Var, int>> xs;
				for (size_t k = 0; k < j; k++) xs.push_back(M.Cross(d0, js[k], 4));
				M.Deck(d0, xs, std::nullopt, std::nullopt, 2, 4);
			}
			M.Tie(M.End(J[j]), M.End(S[j]));
		}
		std::vector<Point> ms;
		for (size_t k = 0; k < n; k++) ms.push_back(M.End(S[k]));
		for (size_t k = 0; k < n; k++) {
			M.NewStrand(O[k], ms[k]);
			M.Node({S[k], J[k], O[k]});
		}
		if (prio) {
			for (size_t k = 0; k < n; k++) M.prio_rows.push_back({S[k], ms[k], h, -sigma});
			for (size_t j = 1; j < n; j++) {
				M.AddSeg(O[j], h, 2);
				M.AddSeg(O[j], Turn(h, -sigma), 2, 2 * static_cast<int>(j));
			}
		}
		std::vector<Seg> st;
		for (const std::string &o : O) {
			M.AddSeg(o, h, 2, std::nullopt, FANW);
			if (!exit_step.has_value()) {
				st.push_back(M.AddSeg(o, Turn(h, +1), 2, std::nullopt, 1.0, true));
			} else if (*exit_step) {
				st.push_back(M.AddSeg(o, Turn(h, +1), 2, std::nullopt, 1.0));
			}
			M.AddSeg(o, h, STUB);
		}
		for (size_t k = 0; k + 1 < n; k++) {
			const TagScope ts("join exit");
			if (!st.empty()) {
				const bool f_first = Turn(h, +1) == Turn(h, sigma);
				const Seg &a = f_first ? st[k] : st[k + 1];
				const Seg &b = f_first ? st[k + 1] : st[k];
				M.Add(Ge(Dot(STEP[h], a.a) - Dot(STEP[h], b.a), 0));
			}
			M.Add(Eq(Dot(r, M.End(O[k + 1])) - Dot(r, M.End(O[k])), 2 * sigma));
		}
	}

	/** A run with an optional sideways step (a required diagonal when jog is not 0); offset-parallel on its last piece at the given lines, which it returns. */
	std::vector<Seg> Flex(const std::vector<std::string> &lanes, int h, const std::vector<int> &offs, int jog)
	{
		Model &M = this->M;
		std::vector<Seg> last, diag;
		const TagScope ts("flex");
		for (const std::string &l : lanes) {
			M.AddSeg(l, h, 0);
			if (jog == 0) {
				last.push_back(M.AddSeg(l, h, 0));
				continue;
			}
			diag.push_back(M.AddSeg(l, Turn(h, jog), 2, std::nullopt, std::sqrt(2.0)));
			last.push_back(M.AddSeg(l, h, 0));
		}
		for (size_t j = 1; j < diag.size(); j++) M.Add(Eq(diag[j].L - diag[0].L, 0));
		const auto &r = Right(h);
		for (size_t j = 1; j < lanes.size(); j++) {
			M.Add(Eq(Dot(r, last[j].a) - Dot(r, last[0].a), 2 * offs[j]));
			M.Add(Eq(Dot(r, last[j].b) - Dot(r, last[0].b), 2 * offs[j]));
		}
		return last;
	}

	/**
	 * A bundle on a channel's path: one segment per heading per lane, offset-parallel.
	 * On a doubled piece each lane runs, takes the parallelogram outward (the left lane's copy left, the right lane's right), steps back and runs on.
	 */
	void Bundle(uint32_t c, const std::vector<int> &heads, const std::set<uint32_t> &doubled)
	{
		Model &M = this->M;
		const std::vector<std::string> lanes = this->Lanes(c);
		const int n = static_cast<int>(lanes.size());
		for (size_t k = 0; k < heads.size(); k++) {
			const int h = heads[k];
			const auto &r = Right(h);
			std::vector<Seg> tails;
			for (int j = 0; j < n; j++) {
				const std::string &l = lanes[j];
				const Seg lead = M.AddSeg(l, h, 0, std::nullopt, IsAxis(h) ? 1.0 : std::sqrt(2.0));
				this->piece_segs[c][l].push_back(lead);
				if (!doubled.contains(static_cast<uint32_t>(k))) {
					tails.push_back(lead);
					continue;
				}
				const int sg = j == 0 ? -1 : +1;
				const std::string cp = this->StartCopy(l, fmt::format("\"{}", k));
				const Seg d0 = M.AddSeg(l, h, 2);
				M.AddSeg(l, Turn(h, sg), 2, 2);
				M.AddSeg(cp, Turn(h, sg), 2, 2);
				const Seg d1 = M.AddSeg(cp, h, 2);
				M.Tie(M.End(cp), M.End(l));
				M.copies.push_back({cp, l, d0.i, d0.i + 2});
				M.AddSeg(l, h, 2);
				M.AddSeg(l, Turn(h, -sg), 2, 2);
				tails.push_back(M.AddSeg(l, h, 0));
				this->dbl_decks[{c, static_cast<uint32_t>(k)}].insert_or_assign(l, std::pair<Seg, Seg>{d0, d1});
				this->piece_tails[{c, static_cast<uint32_t>(k)}].insert_or_assign(l, tails.back());
			}
			for (int j = 1; j < n; j++) {
				const TagScope ts("bundle offset");
				M.Add(Eq(Dot(r, this->piece_segs[c][lanes[j]][k].a) - Dot(r, this->piece_segs[c][lanes[0]][k].a), 2 * j));
				M.Add(Eq(Dot(r, tails[j].b) - Dot(r, tails[0].b), 2 * j));
			}
		}
	}

	/**
	 * The under lanes' segments of a crossing on an under channel's piece; empty (after a failure) when that piece has none.
	 * On a piece holding its own decks the crossing is on its run before them when it comes before the piece's first over crossing in travel order, else on its run after.
	 */
	std::vector<Seg> UnderSegs(uint32_t u, uint32_t x)
	{
		const auto &ul = this->ch.channels[u].crossings;
		const size_t at = std::ranges::find(ul, x) - ul.begin();
		const uint32_t upiece = this->paths.paths[u].crossing_piece[at];
		bool after = false;
		const auto tails = this->piece_tails.find({u, upiece});
		if (tails != this->piece_tails.end()) {
			for (size_t i = 0; i < ul.size(); i++) {
				if (this->paths.over[ul[i]] == u && this->paths.paths[u].crossing_piece[i] == upiece) {
					after = i < at;
					break;
				}
			}
		}
		std::vector<Seg> out;
		for (const std::string &ln : this->Lanes(u)) {
			if (this->piece_segs[u][ln].size() <= upiece) {
				this->M.Fail("a crossing's under channel has no bundled piece there");
				return {};
			}
			out.push_back(after ? tails->second.at(ln) : this->piece_segs[u][ln][upiece]);
		}
		return out;
	}

	/** A prio join's straight lanes arrive on these lines from its far lane: every crossed lane steps one line out (D324). */
	static std::vector<int> JoinEntry(int n)
	{
		std::vector<int> e{0};
		for (int j = 1; j < n; j++) e.push_back(2 * j - 1);
		return e;
	}

	void Build();
};

/** Build the model of the variant. */
void MappedComposer::Build()
{
	Model &M = this->M;
	const PlanarTopology &t = this->t;
	const PlanarChannels &ch = this->ch;
	if (!this->sides.Why().empty()) {
		M.Fail(this->sides.Why());
		return;
	}
	this->paths = BuildPlanarPaths(t, ch, this->variant.over);
	if (!this->paths.why.empty()) {
		M.Fail(this->paths.why);
		return;
	}
	this->piece_segs.resize(ch.channels.size());

	/* Sites: the straight channel and side, the tie sites taking theirs from the variant; then the gap steps they imply. */
	std::string why;
	const std::vector<PlanarResolvedSite> res = ResolvePlanarSites(t, ch, this->variant.ties, why);
	if (!why.empty()) {
		M.Fail(why);
		return;
	}
	int tie = 0;
	for (size_t s = 0; s < ch.sites.size(); s++) {
		if (!PlanarSitePerNode(ch.sites[s]) && ch.sites[s].straight == UINT32_MAX) tie++;
	}
	const std::vector<PlanarGapStep> gap_steps = BuildPlanarGapSteps(t, ch, res, why);
	if (!why.empty()) {
		M.Fail(why);
		return;
	}
	const std::vector<PlanarCandidateStep> cand_steps = PlanarCandidateSteps(t, ch);
	std::set<uint32_t> cores_built;
	const auto site_of = [&](uint32_t c, bool split) -> std::optional<size_t> {
		for (size_t s = 0; s < ch.sites.size(); s++) {
			const bool is_split = ch.sites[s].kind == PlanarSiteKind::Fray;
			if (is_split == split && (res[s].straight == c || res[s].other == c)) return s;
		}
		return std::nullopt;
	};

	/* In ports. */
	for (const PlanarPort &port : t.ports) {
		if (!port.in) continue;
		for (uint32_t l : port.lines) this->PortIn(t.lines[l].name, port.side);
	}

	/* Depth order: a site is built once its in lines exist and starts the lines one deeper; a channel is built after the site that starts it. Crossings are wired at the end. */
	std::vector<int> depth(t.lines.size(), -1);
	std::function<int(uint32_t)> depth_of = [&](uint32_t l) -> int {
		if (depth[l] >= 0) return depth[l];
		int d = 0;
		if (!t.lines[l].from.port) {
			for (uint32_t i : t.nodes[t.lines[l].from.index].in) d = std::max(d, depth_of(i) + 1);
		}
		return depth[l] = d;
	};
	int max_depth = 0;
	for (uint32_t l = 0; l < t.lines.size(); l++) max_depth = std::max(max_depth, depth_of(l));
	std::vector<std::vector<size_t>> sites_at(max_depth + 1);
	for (size_t s = 0; s < ch.sites.size(); s++) {
		const PlanarNode &node = t.nodes[ch.sites[s].nodes.front()];
		sites_at[depth_of(node.out.front())].push_back(s);
	}
	std::vector<std::vector<uint32_t>> channels_at(max_depth + 1);
	for (uint32_t c = 0; c < ch.channels.size(); c++) {
		const uint32_t first = ch.channels[c].lines.front();
		if (t.lines[first].from.port || t.lines[first].to.port) continue;
		channels_at[depth_of(first)].push_back(c);
	}
	std::vector<std::function<bool()>> wire;

	/* Frays: lanes far..near (far away from the diverging side), each in line's legs matched by node. */
	const auto build_fray = [&](size_t s) {
		const PlanarSite &site = ch.sites[s];
		const PlanarResolvedSite &rs = res[s];
		const int h = this->PortHeading(ch.channels[site.channel].up);
		std::vector<uint32_t> ins = ch.channels[site.channel].lines;
		if (rs.side < 0) std::ranges::reverse(ins);
		std::vector<std::string> in_names, S, V;
		for (uint32_t x : ins) {
			const PlanarNode &node = t.nodes[t.lines[x].to.index];
			const uint32_t a = node.out[0], b = node.out[1];
			const bool a_straight = ch.channel_of_line[a] == rs.straight;
			in_names.push_back(t.lines[x].name);
			S.push_back(t.lines[a_straight ? a : b].name);
			V.push_back(t.lines[a_straight ? b : a].name);
		}
		this->Fray(h, rs.side, in_names, S, V, rs.prio);
	};

	/* Movement channels: a bundle on their path. */
	std::vector<uint32_t> cores;
	const auto build_bundle = [&](uint32_t c) -> bool {
		const PlanarChannelPath &path = this->paths.paths[c];
		if (path.headings.size() == 1) {
			cores.push_back(c);
			return true;
		}
		std::vector<int> heads;
		for (PlanarHeading hd : path.headings) heads.push_back(static_cast<int>(hd));
		/* Its pieces over a crossing double every lane when the lanes carry a full share. */
		std::set<uint32_t> doubled;
		const int full = this->FullLanes(c);
		const auto &cl = ch.channels[c].crossings;
		for (size_t i = 0; i < cl.size(); i++) {
			if (this->paths.over[cl[i]] != c || full == 0) continue;
			if (full != 2 || ch.channels[c].lines.size() != 2) {
				M.Fail("a turning channel over a crossing with doubled lanes is composed for two full-share lanes only");
				return false;
			}
			if (!IsAxis(heads[path.crossing_piece[i]])) {
				M.Fail("a doubled bridge on a diagonal piece");
				return false;
			}
			doubled.insert(path.crossing_piece[i]);
		}
		this->Bundle(c, heads, doubled);
		return true;
	};

	/* Straight channels: a run to the join-entry lines; a prio channel over crossings doubles each lane over them all (the core). */
	const auto build_core = [&](uint32_t c) -> bool {
		const PlanarChannel &cc = ch.channels[c];
		const std::vector<std::string> lanes = this->Lanes(c);
		const int n = static_cast<int>(lanes.size());
		const int h = static_cast<int>(this->paths.paths[c].headings.front());
		const std::optional<size_t> join = site_of(c, false);
		std::vector<int> offs(n);
		for (int j = 0; j < n; j++) offs[j] = j;
		bool far_left = true;
		if (join.has_value() && res[*join].straight == c && res[*join].prio) {
			far_left = res[*join].side > 0;
			const std::vector<int> e = JoinEntry(n);
			for (int j = 0; j < n; j++) offs[j] = far_left ? e[j] : e[n - 1] - e[n - 1 - j];
		}
		cores_built.insert(c);
		int jog = 0;
		for (size_t k = 0; k < cand_steps.size(); k++) {
			if (cand_steps[k].channel == c && (this->variant.steps >> k & 1) != 0) jog = cand_steps[k].toward;
		}
		const std::vector<Seg> lead = this->Flex(lanes, h, offs, jog);
		/* Its one piece for crossings under it: the run before its decks. */
		for (int j = 0; j < n; j++) this->piece_segs[c][lanes[j]].push_back(lead[j]);
		std::vector<uint32_t> overs;
		for (uint32_t x : cc.crossings) {
			if (this->paths.over[x] == c) overs.push_back(x);
		}
		if (overs.empty()) return true;
		const int full = this->FullLanes(c);
		if (full != 0 && full != n) {
			M.Fail("a straight channel over a crossing with doubled and single lanes is not composed yet");
			return false;
		}
		/* One deck per crossing on each deck segment, in travel order; with a copy, the first copy-0 deck has its near head tight and the last its tail, mirrored on copy 1, equal bridges, copy 1 one tile upstream. Wired once every under piece exists. */
		const auto wire_decks = [this, &M, &ch, c, h, overs](const Seg &d0, std::optional<Seg> d1) -> bool {
			const TagScope ts("deck chain");
			std::optional<Var> prev_f0, prev_f1;
			for (size_t k = 0; k < overs.size(); k++) {
				const PlanarChannelCrossing &xc = ch.crossings[overs[k]];
				const std::vector<Seg> under = this->UnderSegs(xc.a == c ? xc.b : xc.a, overs[k]);
				if (under.empty()) return false;
				std::vector<std::pair<Var, int>> x0, x1;
				for (const Seg &v : under) {
					x0.push_back(M.Cross(d0, v, d1.has_value() ? 3 : 4));
					this->cross_x[M.crossings.size() - 1] = overs[k];
					if (!d1.has_value()) continue;
					x1.push_back(M.Cross(*d1, v, 3));
					this->cross_x[M.crossings.size() - 1] = overs[k];
				}
				if (!d1.has_value()) {
					const auto [n0, f0] = M.Deck(d0, x0);
					if (prev_f0.has_value()) M.Add(Ge(n0 - *prev_f0, 2));
					prev_f0 = f0;
					continue;
				}
				const bool first_k = k == 0, last_k = k + 1 == overs.size();
				const auto [n0, f0] = M.Deck(d0, x0, first_k ? std::optional<int>(4) : std::nullopt, last_k ? std::optional<int>(0) : std::nullopt);
				const auto [n1, f1] = M.Deck(*d1, x1, first_k ? std::optional<int>(0) : std::nullopt, last_k ? std::optional<int>(4) : std::nullopt);
				if (prev_f0.has_value()) {
					M.Add(Ge(n0 - *prev_f0, 2));
					M.Add(Ge(n1 - *prev_f1, 2));
				}
				prev_f0 = f0;
				prev_f1 = f1;
				M.Add(Eq(f0 - n0 - (f1 - n1), 0));
				M.Add(Eq(Dot(STEP[h], d0.a) + n0 - Dot(STEP[h], d1->a) - n1, 2));
			}
			return true;
		};
		if (full == 0) {
			/* Lanes carrying a half share or less cross single. */
			for (int j = 0; j < n; j++) {
				const Seg d0 = M.AddSeg(lanes[j], h, 2);
				wire.push_back([wire_decks, d0]() { return wire_decks(d0, std::nullopt); });
			}
		} else {
			/* Copy sides (D312): the far lane copies outward, every other lane away from the far lane. */
			const int away = far_left ? +1 : -1;
			for (int j = 0; j < n; j++) {
				const bool is_far = far_left ? j == 0 : j == n - 1;
				const int sg = is_far ? -away : away;
				const auto [d0, d1] = this->Double(lanes[j], h, sg, "\"", 2);
				M.AddSeg(lanes[j], h, 2);
				M.AddSeg(lanes[j], Turn(h, -sg), 2, 2);
				wire.push_back([wire_decks, d0, d1]() { return wire_decks(d0, d1); });
			}
		}
		std::vector<Seg> run;
		for (const std::string &l : lanes) run.push_back(M.AddSeg(l, h, 0));
		const TagScope ts("run offset");
		for (int j = 1; j < n; j++) M.Add(Eq(Dot(Right(h), run[j].a) - Dot(Right(h), run[0].a), 2 * offs[j]));
		for (int j = 0; j < n; j++) this->piece_tails[{c, 0}].insert_or_assign(lanes[j], run[j]);
		return true;
	};

	/* Joins: lanes far..near (far away from the joining side), each merge's legs matched by node. */
	const auto build_join = [&](size_t s) -> bool {
		const PlanarSite &site = ch.sites[s];
		const PlanarResolvedSite &rs = res[s];
		const int h = this->PortHeading(ch.channels[site.channel].down);
		if (static_cast<int>(this->paths.paths[rs.other].headings.back()) != h || static_cast<int>(this->paths.paths[rs.straight].headings.back()) != h) {
			M.Fail("a join's channels do not arrive on its heading");
			return false;
		}
		std::vector<std::tuple<size_t, std::string, std::string, std::string>> legs;
		const std::vector<uint32_t> &sl = ch.channels[rs.straight].lines;
		for (uint32_t nidx : site.nodes) {
			const PlanarNode &node = t.nodes[nidx];
			const uint32_t a = node.in[0], b = node.in[1];
			const bool a_straight = ch.channel_of_line[a] == rs.straight;
			const uint32_t sline = a_straight ? a : b, jline = a_straight ? b : a;
			const size_t pos = std::ranges::find(sl, sline) - sl.begin();
			legs.emplace_back(rs.side > 0 ? pos : sl.size() - 1 - pos, t.lines[sline].name, t.lines[jline].name, t.lines[node.out[0]].name);
		}
		std::ranges::sort(legs);
		std::vector<std::string> S, J, O;
		for (const auto &[pos, sn, jn, on] : legs) {
			S.push_back(sn);
			J.push_back(jn);
			O.push_back(on);
		}
		bool exit_step = false;
		for (const PlanarGapStep &gs : gap_steps) {
			if (gs.channel != site.channel || gs.fan) continue;
			exit_step = true;
			if (gs.toward != +1) {
				M.Fail("an exit step to the left is not composed");
				return false;
			}
		}
		this->Join(h, rs.side, S, J, O, rs.prio, exit_step);

		return true;
	};

	/* Sites whose nodes do not share one straight and one other channel: per node, a fork (a lane split) or a one-lane join. */
	const auto build_nodes = [&](size_t s) -> bool {
		const PlanarSite &site = ch.sites[s];
		for (uint32_t n : site.nodes) {
			const PlanarNode &node = t.nodes[n];
			if (node.IsSplit()) {
				if (ch.node_legs[n].straight != UINT32_MAX) {
					M.Fail("a split with a straight leg in a mixed site is not composed yet");
					return false;
				}
				const PlanarChannelPath &kp = this->paths.paths[ch.channel_of_line[node.in.front()]];
				this->Fork(static_cast<int>(kp.headings.back()), t.lines[node.in.front()].name, t.lines[node.out[0]].name, t.lines[node.out[1]].name);
				continue;
			}
			uint32_t straight = ch.node_legs[n].straight;
			int side = ch.node_legs[n].side;
			if (straight == UINT32_MAX) {
				straight = node.in[(this->variant.ties >> tie++ & 1) != 0 ? 1 : 0];
				side = straight == node.in[0] ? 1 : -1;
			}
			const uint32_t joiner = node.in[0] == straight ? node.in[1] : node.in[0];
			const int h = this->PortHeading(ch.channels[ch.channel_of_line[node.out.front()]].down);
			if (h < 0 || static_cast<int>(this->paths.paths[ch.channel_of_line[straight]].headings.back()) != h || static_cast<int>(this->paths.paths[ch.channel_of_line[joiner]].headings.back()) != h) {
				M.Fail("a merge's legs do not arrive on its heading");
				return false;
			}
			M.opt_group = fmt::format("merge{}j", n);
			this->Join(h, side, {t.lines[straight].name}, {t.lines[joiner].name}, {t.lines[node.out.front()].name}, this->LinePrio(straight), std::nullopt, 2);
			M.opt_group.clear();
		}
		return true;
	};

	for (int d = 1; d <= max_depth; d++) {
		for (size_t s : sites_at[d]) {
			if (PlanarSitePerNode(ch.sites[s])) {
				if (!build_nodes(s)) return;
				continue;
			}
			if (ch.sites[s].kind == PlanarSiteKind::Fray) build_fray(s);
			if (ch.sites[s].kind == PlanarSiteKind::Join && !build_join(s)) return;
		}
		cores.clear();
		for (uint32_t c : channels_at[d]) {
			if (!build_bundle(c)) return;
		}
		for (uint32_t c : cores) {
			if (!build_core(c)) return;
		}
	}
	for (size_t k = 0; k < cand_steps.size(); k++) {
		if ((this->variant.steps >> k & 1) != 0 && !cores_built.contains(cand_steps[k].channel)) {
			M.Fail("a candidate step on a channel that is not a core");
			return;
		}
	}
	for (const auto &w : wire) {
		if (!w()) return;
	}

	/* Crossings between bundled channels: the over lanes' piece decks over all the under lanes' piece. */
	std::map<std::pair<uint32_t, uint32_t>, std::vector<uint32_t>> deck_groups;
	for (uint32_t x = 0; x < ch.crossings.size(); x++) {
		const uint32_t o = this->paths.over[x];
		if (this->paths.paths[o].headings.size() == 1) continue;
		const auto &ol = ch.channels[o].crossings;
		deck_groups[{o, this->paths.paths[o].crossing_piece[std::ranges::find(ol, x) - ol.begin()]}].push_back(x);
	}
	for (const auto &[key, xs] : deck_groups) {
		const auto [o, opiece] = key;
		std::vector<Seg> under;
		for (uint32_t x : xs) {
			const PlanarChannelCrossing &xc = ch.crossings[x];
			const std::vector<Seg> u = this->UnderSegs(xc.a == o ? xc.b : xc.a, x);
			if (u.empty()) return;
			for (const Seg &v : u) under.push_back(v);
		}
		const auto cross_all = [&](const Seg &over, int head) {
			std::vector<std::pair<Var, int>> cr;
			size_t i = 0;
			for (uint32_t x : xs) {
				const PlanarChannelCrossing &xc = ch.crossings[x];
				const size_t nu = ch.channels[xc.a == o ? xc.b : xc.a].lines.size();
				for (size_t j = 0; j < nu; j++, i++) {
					cr.push_back(M.Cross(over, under[i], head));
					this->cross_x[M.crossings.size() - 1] = x;
				}
			}
			return cr;
		};
		const auto dbl = this->dbl_decks.find(key);
		for (const std::string &ol : this->Lanes(o)) {
			if (dbl == this->dbl_decks.end()) {
				M.Deck(this->piece_segs[o][ol][opiece], cross_all(this->piece_segs[o][ol][opiece], 4));
				this->deck_x[M.decks.size() - 1] = xs.front();
				continue;
			}
			/* A doubled lane: copy-0 deck near head tight, copy-1 tail tight, equal bridges, copy 1 one tile upstream (D118, D145). */
			const TagScope ts("doubled deck");
			const auto &[d0, d1] = dbl->second.at(ol);
			const int h = d0.d;
			const auto [n0, f0] = M.Deck(d0, cross_all(d0, 4), 4, 0);
			this->deck_x[M.decks.size() - 1] = xs.front();
			const auto [n1, f1] = M.Deck(d1, cross_all(d1, 4), 0, 4);
			this->deck_x[M.decks.size() - 1] = xs.front();
			M.Add(Eq(f0 - n0 - (f1 - n1), 0));
			M.Add(Eq(Dot(STEP[h], d0.a) + n0 - Dot(STEP[h], d1.a) - n1, 2));
		}
	}

	/* An under track is at ground where it is crossed: a piece that is both over (its deck) and under (another channel's deck) keeps the crossing outside its own deck, on the side its channel's crossing order gives. */
	if (std::getenv("PLANAR_NOUNDER") == nullptr) {
		for (const auto &[ci, xu] : this->cross_x) {
			const CrossRec &c = M.crossings[ci];
			for (const auto &[di, xd] : this->deck_x) {
				const TagScope ts("under-deck");
				const DeckRec &d = M.decks[di];
				if (d.over.strand != c.under.strand || d.over.i != c.under.i) continue;
				const uint32_t line = PlanarLineByName(t, c.under.strand);
				if (line == UINT32_MAX) continue;
				const auto &list = ch.channels[ch.channel_of_line[line]].crossings;
				const auto pu = std::ranges::find(list, xu) - list.begin(), pd = std::ranges::find(list, xd) - list.begin();
				if (pu < pd) {
					M.Add(Le(c.b, d.n - 3));
				} else {
					M.Add(Ge(c.b, d.f + 3));
				}
			}
		}
	}

	/* Clash sides from the channel graph, given up front. */
	const bool rules = std::getenv("PLANAR_NORULES") == nullptr;
	const auto sep = [&](const Seg &sp, const Seg &sq, const std::array<int, 2> &f) { // sq lies at least 2 beyond sp along f
		if (!rules) return;
		const TagScope ts(fmt::format("{} {}#{} {}#{}", g_tag, sp.strand, sp.i, sq.strand, sq.i));
		for (const Point &a : {sp.a, sp.b}) {
			for (const Point &b : {sq.a, sq.b}) M.Add(Ge(Dot(f, b) - Dot(f, a), 2));
		}
	};
	const auto legs_of = [&](uint32_t line) {
		std::vector<Seg> out;
		const PlanarEnd &e = t.lines[line].to;
		if (e.port) return out;
		for (uint32_t o : t.nodes[e.index].out) {
			const std::vector<Seg> &segs = M.strands.at(t.lines[o].name).segs;
			for (size_t k = 0; k < std::min<size_t>(2, segs.size()); k++) out.push_back(segs[k]);
		}
		return out;
	};
	/* An in port's fan: a lane right of another keeps its fan and what follows right of and ahead of the other's stub and fan. */
	for (const PlanarPort &port : t.ports) {
		const TagScope ts("fan rule");
		if (!port.in) continue;
		const int h = static_cast<int>(port.heading);
		const std::array<int, 2> f = {Right(h)[0] + STEP[h][0], Right(h)[1] + STEP[h][1]};
		for (size_t i = 0; i < port.lines.size(); i++) {
			const std::vector<Seg> &ps = M.strands.at(t.lines[port.lines[i]].name).segs;
			for (size_t j = i + 1; j < port.lines.size(); j++) {
				const std::vector<Seg> &qs = M.strands.at(t.lines[port.lines[j]].name).segs;
				const std::vector<Seg> legs = legs_of(port.lines[j]);
				for (size_t k = 1; k < qs.size(); k++) sep(ps[0], qs[k], f);
				for (const Seg &sq : legs) sep(ps[0], sq, f);
				if (ps.size() < 2 || ps[1].d == ps[0].d) continue; // a plain fray's lane has no fan
				for (size_t k = 2; k < qs.size(); k++) sep(ps[1], qs[k], f);
				for (const Seg &sq : legs) sep(ps[1], sq, f);
			}
		}
	}
	/* Ports on one side: a joiner arriving from the side of the neighbouring in port keeps its last run on its own side of that port. */
	for (size_t s = 0; s < ch.sites.size(); s++) {
		const TagScope ts("side rule");
		if (ch.sites[s].kind != PlanarSiteKind::Join) continue;
		const PlanarChannel &key = ch.channels[ch.sites[s].channel];
		if (key.down.size() != 1) continue;
		const uint32_t op = *key.down.begin();
		const PlanarPort &out_port = t.ports[op];
		const int h = static_cast<int>(out_port.heading);
		const std::array<int, 2> toward = {Right(h)[0] * res[s].side, Right(h)[1] * res[s].side}; // where the joiner comes from
		for (uint32_t ip = 0; ip < t.ports.size(); ip++) {
			const PlanarPort &in_port = t.ports[ip];
			if (!in_port.in || in_port.side != out_port.side) continue;
			/* The in port lies on the joiner's side of the out port along the box edge (a side's ports are in reading order: N and S west to east, W and E north to south). */
			const bool along_y = out_port.side == PlanarSide::W || out_port.side == PlanarSide::E;
			const int dir = along_y ? toward[1] : toward[0];
			if (dir == 0) continue;
			const bool in_after = ip > op;
			if ((dir > 0) != in_after) continue;
			const std::array<int, 2> f = along_y ? std::array<int, 2>{0, dir} : std::array<int, 2>{dir, 0};
			for (const std::string &jl : this->Lanes(res[s].other)) {
				const std::vector<Seg> &js = M.strands.at(jl).segs; // its last two: the join's own approach
				for (uint32_t l : in_port.lines) {
					for (const Seg &sp : M.strands.at(t.lines[l].name).segs) {
						for (size_t k = js.size() >= 2 ? js.size() - 2 : 0; k < js.size(); k++) sep(js[k], sp, f);
					}
				}
			}
		}
	}

	/* Experiment (PLANAR_FACESEPS): every pair of lines bounding a common face keeps its side, segment against segment; a segment pair sharing an end point is left out (it meets there). */
	if (std::getenv("PLANAR_FACESEPS") != nullptr) {
		for (const auto &[lp, lq] : this->sides.FacePairs()) {
			if (lp >= lq) continue;
			const int8_t side = this->sides.Side(lp, lq);
			for (const Seg &sp : M.strands.at(t.lines[lp].name).segs) {
				for (const Seg &sq : M.strands.at(t.lines[lq].name).segs) {
					const std::array<int, 2> f = {-Right(sq.d)[0] * side, -Right(sq.d)[1] * side};
					std::vector<Con> cons;
					bool shared = false;
					for (const Point &x : {sp.a, sp.b}) {
						for (const Point &y : {sq.a, sq.b}) {
							cons.push_back(Ge(Dot(f, y) - Dot(f, x), 2));
							if (Model::MakeRow(cons.back()).terms.empty()) shared = true;
						}
					}
					if (shared) continue;
					const TagScope ts(fmt::format("face sep {}#{} {}#{}", sp.strand, sp.i, sq.strand, sq.i));
					for (const Con &c : cons) M.Add(c);
				}
			}
		}
	}

	/* Out ports on the boundary. */
	for (const PlanarPort &port : t.ports) {
		if (port.in) continue;
		for (uint32_t l : port.lines) {
			const TagScope ts("port out");
			const Point u = M.End(t.lines[l].name);
			if (port.side == PlanarSide::W) M.Add(Eq(u[0], 0));
			if (port.side == PlanarSide::E) M.Add(Eq(u[0] - M.X, 0));
			if (port.side == PlanarSide::S) M.Add(Eq(u[1] - M.Y, 0));
			if (port.side == PlanarSide::N) M.Add(Eq(u[1], 1));
		}
	}
	/* Port order per side in reading order: lanes of a port adjacent, one tile between ports. */
	for (const PlanarSide side : {PlanarSide::N, PlanarSide::E, PlanarSide::S, PlanarSide::W}) {
		const bool across_y = side == PlanarSide::W || side == PlanarSide::E;
		const std::array<int, 2> read = across_y ? std::array<int, 2>{0, 1} : std::array<int, 2>{1, 0};
		std::vector<std::pair<Point, bool>> seq; // point, first line of a new port
		for (const PlanarPort &port : t.ports) {
			if (port.side != side) continue;
			std::vector<uint32_t> lines = port.lines;
			const auto &r = Right(static_cast<int>(port.heading));
			if (r[0] * read[0] + r[1] * read[1] < 0) std::ranges::reverse(lines);
			bool first = true;
			for (uint32_t l : lines) {
				const std::string &nm = t.lines[l].name;
				seq.emplace_back(port.in ? M.strands.at(nm).start : M.End(nm), first);
				first = false;
			}
		}
		const TagScope ts("port order");
		const size_t k = across_y ? 1 : 0;
		for (size_t i = 1; i < seq.size(); i++) M.Add(Eq(seq[i].first[k] - seq[i - 1].first[k], seq[i].second ? 4 : 2));
	}
	/* Experiment (PLANAR_ALIGN): a prio route between opposite sides enters and leaves on the same row (D316). */
	if (std::getenv("PLANAR_ALIGN") != nullptr) {
		const TagScope ts("align");
		for (const PlanarRoute &route : t.routes) {
			if (!route.prio || route.paths.empty() || route.paths[0].empty()) continue;
			const PlanarLine &first = t.lines[route.paths[0].front()], &last = t.lines[route.paths[0].back()];
			if (!first.from.port || !last.to.port) continue;
			const PlanarSide a = t.ports[first.from.index].side, b = t.ports[last.to.index].side;
			const bool we = (a == PlanarSide::W && b == PlanarSide::E) || (a == PlanarSide::E && b == PlanarSide::W);
			const bool ns = (a == PlanarSide::N && b == PlanarSide::S) || (a == PlanarSide::S && b == PlanarSide::N);
			if (!we && !ns) continue;
			if (std::string(std::getenv("PLANAR_ALIGN")) == "W" && a != PlanarSide::W) continue; // only routes entering from the west
			const size_t k = we ? 1 : 0;
			M.Add(Eq(M.End(last.name)[k] - M.strands.at(first.name).start[k], 0));
		}
	}
	/* Box. */
	for (const std::string &name : M.order) {
		const TagScope ts("box");
		const Strand &st = M.strands.at(name);
		std::vector<Point> pts{st.start};
		for (const Seg &sg : st.segs) pts.push_back(sg.b);
		for (const Point &p : pts) {
			M.Add(Ge(p[0], 0));
			M.Add(Ge(M.X - p[0], 0));
			M.Add(Ge(p[1], 1));
			M.Add(Ge(M.Y - p[1], 0));
		}
	}
	/* Prio rows: inside the box, unrelated to all. */
	for (const PrioRow &row : std::vector<PrioRow>(M.prio_rows)) {
		const TagScope ts("prio row");
		const auto &r = Right(row.h);
		const int dx = STEP[row.h][0], dy = STEP[row.h][1];
		const std::array<Ex, 2> start = {row.m[0] - dx * 2 * this->P + row.side * r[0] * 2, row.m[1] - dy * 2 * this->P + row.side * r[1] * 2};
		const std::string nm = "prio:" + row.strand;
		M.NewStrand(nm, M.NewPoint());
		M.Tie(M.strands.at(nm).start, start);
		M.AddSeg(nm, row.h, 2 * this->P, 2 * this->P, 0);
		for (const Point &p : {M.strands.at(nm).start, M.End(nm)}) {
			M.Add(Ge(p[0], 0));
			M.Add(Ge(M.X - p[0], 0));
			M.Add(Ge(p[1], 1));
			M.Add(Ge(M.Y - p[1], 0));
		}
	}
	/* A deck is two heads and at most the longest middle. */
	for (const DeckRec &d : std::vector<DeckRec>(M.decks)) {
		const TagScope ts("deck max");
		M.Add(Le(d.f - d.n, 2 * (M.params.maxmid + 2)));
	}

	/* Routes: every route path; the outline: from each in port's left lane, the path turning furthest left. */
	for (const PlanarRoute &route : t.routes) {
		for (const auto &path : route.paths) {
			std::vector<std::string> names;
			for (uint32_t l : path) names.push_back(t.lines[l].name);
			this->routes.push_back(std::move(names));
		}
	}
	for (const PlanarPort &port : t.ports) {
		if (!port.in) continue;
		const uint32_t lane = port.lines.front();
		const std::vector<std::string> *best = nullptr;
		int best_turn = INT_MAX;
		for (const auto &rp : this->routes) {
			if (rp.front() != t.lines[lane].name) continue;
			const PlanarLine &last = t.lines[PlanarLineByName(t, rp.back())];
			const int turn = TurnOf(static_cast<int>(port.heading), static_cast<int>(t.ports[last.to.index].heading));
			if (turn < best_turn) {
				best_turn = turn;
				best = &rp;
			}
		}
		if (best != nullptr) this->outline.push_back(*best);
	}
	/* R5: a non-prio route runs at least 2 TL units from its last split (or its port) to each merge. */
	const auto starts_at = [&](const std::string &name, size_t legs_in) {
		const PlanarEnd &e = t.lines[PlanarLineByName(t, name)].from;
		return !e.port && t.nodes[e.index].in.size() == legs_in;
	};
	std::set<std::vector<std::string>> bays;
	for (const PlanarRoute &route : t.routes) {
		if (route.prio) continue;
		for (const auto &path : route.paths) {
			std::vector<std::string> names;
			for (uint32_t l : path) names.push_back(t.lines[l].name);
			for (size_t i = 0; i + 1 < names.size(); i++) {
				if (!starts_at(names[i + 1], 2)) continue;
				size_t j = i;
				while (j > 0 && !starts_at(names[j], 1)) j--;
				std::vector<std::string> stretch(names.begin() + j, names.begin() + i + 1);
				if (!bays.insert(stretch).second) continue;
				const TagScope ts("bay length");
				Ex len;
				for (const std::string &nm : stretch) {
					for (const Seg &sg : M.strands.at(nm).segs) len = len + sg.L;
				}
				M.Add(Ge(len, 2 * M.params.tl + 4));
			}
		}
	}
	M.routes = &this->routes;
	M.outline_routes = &this->outline;
	Composer::RouteCurves(M, this->routes, M.params, std::sqrt(2.0));
}

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
std::vector<std::pair<std::array<int, 2>, std::vector<Row>>> SepOptions(const Model &M, const GeoSet &g, const std::pair<SegKey, SegKey> &pr)
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
	std::vector<std::pair<std::array<int, 2>, std::vector<Row>>> out;
	for (const auto &[c, fx, fy] : opts) {
		const std::array<int, 2> f{fx, fy};
		std::vector<Row> rows;
		const TagScope ts(fmt::format("sep {}#{} {}#{} dir({},{})", pr.first.first, pr.first.second, pr.second.first, pr.second.second, fx, fy));
		for (const Point *a : {&sp.a, &sp.b}) {
			for (const Point *b : {&sq.a, &sq.b}) rows.push_back(Model::MakeRow(Ge(Dot(f, *b) - Dot(f, *a), 2)));
		}
		out.emplace_back(f, std::move(rows));
	}
	return out;
}

/** The outline's points: the outer route between each pair of neighbouring arms, joined along the box edges. */
std::vector<Point> Outline(const Model &M)
{
	std::vector<Point> pts;
	for (const auto &route : *M.outline_routes) {
		const std::vector<Seg> seq = RouteSegs(M, route)[0];
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
 * @param sides The topology's line sides, or null (a hand composition).
 * @return Whether a clash-free solution was found.
 */
bool SolveLazy1(Model &M, Clock::time_point t0, const std::vector<Row> &seps0, LazyResult &out, std::string &why, const PlanarLineSides *sides)
{
	std::vector<Row> seps = seps0;
	Solution r;
	/* PLANAR_IIS: a proven infeasible solve names its conflicting rows (the solve that has just failed must be the last one made). */
	const bool explain = std::getenv("PLANAR_IIS") != nullptr;
	std::vector<Row> tried; // the rows of the last failed trial, kept only to explain it
	const auto explain_last = [&](const std::vector<Row> &extra) {
		if (explain && M.last_infeasible) M.ExplainInfeasible(extra);
	};
	if (!M.Solve(seps, r)) {
		explain_last(seps);
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
		if (std::getenv("PLANAR_TIMES") != nullptr) fmt::print(stderr, "round {}: {} clashes, {} under-deck\n", rnd, bad.size(), under_bad.size());
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
				if (explain) tried = std::move(trial);
			}
			if (!done) {
				explain_last(tried);
				why = "under-deck conflict not separable";
				return false;
			}
			if (!M.Solve(seps, r)) {
				explain_last(seps);
				why = "infeasible after separation";
				return false;
			}
			continue;
		}
		if (std::chrono::duration<double>(Clock::now() - t0).count() > BUDGET) {
			why = fmt::format("budget at round {}, {} clashes", rnd, bad.size());
			return false;
		}
		/* A clash between two lines the topology orders is separated on the side it gives: q at least 2 beyond p along a direction with p on q's -f side. */
		if (sides != nullptr) {
			std::vector<Row> rows;
			size_t decided = 0;
			for (const auto &clash : bad) {
				const uint32_t lp = sides->LineOfStrand(clash.first.first), lq = sides->LineOfStrand(clash.second.first);
				if (lp == UINT32_MAX || lq == UINT32_MAX || lp == lq || sides->Side(lp, lq) == 0) continue;
				const auto &right = Right(g.geo.at(clash.second).d);
				for (auto &[f, opt_rows] : SepOptions(M, g, clash)) {
					if ((f[0] * right[0] + f[1] * right[1]) * sides->Side(lp, lq) >= 0) continue;
					rows.insert(rows.end(), opt_rows.begin(), opt_rows.end());
					decided++;
					break;
				}
			}
			if (std::getenv("PLANAR_TIMES") != nullptr) fmt::print(stderr, "round {}: {} decided by sides, {} by trial\n", rnd, decided, decided == 0 ? 1 : 0);
			if (decided > 0) {
				seps.insert(seps.end(), rows.begin(), rows.end());
				if (!M.Solve(seps, r)) {
					explain_last(seps);
					why = "infeasible with topology sides";
					return false;
				}
				continue;
			}
		}
		const std::vector<std::pair<std::array<int, 2>, std::vector<Row>>> options = SepOptions(M, g, bad[0]);
		bool separated = false;
		for (size_t k = 0; k < 4; k++) {
			std::vector<Row> trial = seps;
			trial.insert(trial.end(), options[k].second.begin(), options[k].second.end());
			Solution probe;
			if (M.Solve(trial, probe)) {
				seps = std::move(trial);
				separated = true;
				break;
			}
			if (explain) tried = std::move(trial);
		}
		if (!separated) {
			explain_last(tried);
			why = "a clash is not separable";
			return false;
		}
		if (!M.Solve(seps, r)) {
			explain_last(seps);
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
 * @param sides The topology's line sides, or null (a hand composition).
 * @return Whether a solution was found.
 */
bool SolveLazy(Model &M, Clock::time_point t0, LazyResult &out, double &area, std::string &why, const PlanarLineSides *sides)
{
	M.obj_extra.clear();
	/* Experiment (PLANAR_OBJ): a linear score instead of the area step: len (track only), box (width + height), routes (outline route length), loop (AREA_ROUNDS linearisations). */
	const char *obj_env = std::getenv("PLANAR_OBJ");
	const std::string obj = obj_env != nullptr ? obj_env : "";
	const double w = std::getenv("PLANAR_OBJW") != nullptr ? std::atof(std::getenv("PLANAR_OBJW")) : 10.0;
	if (obj == "box") {
		M.obj_extra[M.X.index] += w;
		M.obj_extra[M.Y.index] += w;
	} else if (obj == "routes") {
		std::set<std::pair<std::string, uint32_t>> seen;
		for (const auto &route : *M.outline_routes) {
			for (const std::string &st : route) {
				for (const Seg &sg : M.strands.at(st).segs) {
					if (seen.insert({st, sg.i}).second) M.obj_extra[sg.L.index] += w;
				}
			}
		}
	}
	if (!SolveLazy1(M, t0, {}, out, why, sides)) return false;
	double best = std::abs(AreaOf(out.sol.val, Outline(M)));
	if (obj == "len" || obj == "box" || obj == "routes") {
		area = best / 4;
		return true;
	}
	std::vector<Row> seps = out.seps;
	for (int it = 0; it < (obj == "loop" ? AREA_ROUNDS : 1); it++) {
		LineariseArea(M, out.sol.val);
		LazyResult r2;
		std::string why2;
		if (!SolveLazy1(M, t0, seps, r2, why2, sides)) break;
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
	double track = 0;
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
	if (!SolveLazy(M, t0, res, out.area, out.why, nullptr)) return out;
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

/**
 * Build, solve and export one variant of the mapped composition.
 * @param topology The topology.
 * @param channels Its channel graph.
 * @param params The spec facts.
 * @param variant The variant.
 * @return The outcome.
 */
VariantOutcome SolveMappedVariant(const PlanarTopology &topology, const PlanarChannels &channels, const Params &params, const MappedVariant &variant)
{
	VariantOutcome out;
	out.tag = fmt::format("o{}t{}f{}s{}", variant.over, variant.ties, variant.flips, variant.steps);
	const Clock::time_point t0 = Clock::now();
	MappedComposer composer(topology, channels, params, variant);
	Model &M = composer.M;
	if (std::getenv("PLANAR_DRY") != nullptr) { // debug: build every variant without solving; print its over choices and its construction failure
		const PlanarPaths pp = BuildPlanarPaths(topology, channels, variant.over);
		std::string overs;
		for (uint32_t x = 0; x < channels.crossings.size() && x < pp.over.size(); x++) {
			const auto nm = [&](uint32_t c) { return topology.lines[channels.channels[c].lines.front()].name; };
			const uint32_t o = pp.over[x], u = channels.crossings[x].a == o ? channels.crossings[x].b : channels.crossings[x].a;
			overs += " " + nm(o) + ">" + nm(u);
		}
		fmt::print(stderr, "DRY {} |{} | {}\n", out.tag, overs, M.error.empty() ? std::string("built") : M.error);
		out.why = "dry";
		return out;
	}
	if (!M.error.empty()) {
		out.why = M.error;
		return out;
	}
	LazyResult res;
	if (!SolveLazy(M, t0, res, out.area, out.why, &composer.sides)) return out;
	for (const auto &[key, geo] : res.geo.geo) out.track += geo.L * (IsAxis(geo.d) ? 0.5 : std::sqrt(2.0) / 2);
	out.width = static_cast<int>(std::llround(Ev(res.sol.val, M.X))) / 2 + 1;
	out.height = static_cast<int>(std::llround(Ev(res.sol.val, M.Y))) / 2 + 1;
	if (!M.error.empty()) {
		out.why = M.error;
		return out;
	}
	if (!ExportRoutes(M, res.geo, topology, out.routes, out.why)) return out;
	if (const char *dir = std::getenv("PLANAR_MAPPED_DUMP"); dir != nullptr) {
		/* Debug: each strand's crossings in travel order (the other strand and over or under), for diffing against a hand composition. */
		std::map<std::string, std::vector<std::pair<double, std::string>>> along;
		const auto offset = [&](const Seg &sg) {
			double o = 0;
			for (uint32_t i = 0; i < sg.i; i++) o += Ev(res.sol.val, M.strands.at(sg.strand).segs[i].L);
			return o;
		};
		for (const CrossRec &c : M.crossings) {
			along[c.over.strand].emplace_back(offset(c.over) + Ev(res.sol.val, c.a), "over " + c.under.strand);
			along[c.under.strand].emplace_back(offset(c.under) + Ev(res.sol.val, c.b), "under " + c.over.strand);
		}
		std::ofstream f(fmt::format("{}/{}.txt", dir, out.tag));
		f << "area " << out.area << "\n";
		for (const auto &[key, z] : M.group_z) f << "switch " << key.first << " h" << key.second << " " << std::llround(Ev(res.sol.val, z)) << "\n";
		for (const std::string &name : M.order) {
			f << "seq " << name << ":";
			for (const Seg &sg : M.strands.at(name).segs) {
				const int64_t len = std::llround(Ev(res.sol.val, sg.L));
				if (len != 0) f << " " << sg.d << ":" << len;
			}
			f << "\n";
		}
		for (auto &[name, xs] : along) {
			std::ranges::sort(xs);
			f << name << ":";
			for (const auto &[pos, what] : xs) f << " " << what;
			f << "\n";
		}
	}
	out.ok = true;
	return out;
}

/** The spec facts the model uses. */
Params MakeParams(const JunctionSpec &spec)
{
	Params params;
	params.tl = spec.tl;
	params.cl = static_cast<int>(DeriveCurveSpacing(spec.reference_speed, spec.curve_speed, spec.tl));
	params.p = spec.prio_tiles;
	params.g = std::min(2 * params.tl - 1, std::max(2, params.cl));
	params.maxmid = spec.max_bridge_middle;
	return params;
}

/** The least-area outcome, or the reasons none solved. */
bool PickBest(const std::vector<VariantOutcome> &outcomes, Clock::time_point t0, PlanarComponentLayout &layout, std::string *why)
{
	const VariantOutcome *best = nullptr;
	std::string reasons;
	for (const VariantOutcome &o : outcomes) {
		if (!o.ok) {
			reasons += " " + o.tag + ": " + o.why + ";";
			continue;
		}
		if (best == nullptr || o.area < best->area) best = &o;
	}
	if (best == nullptr) {
		if (why != nullptr) *why = "no variant solves:" + reasons;
		return false;
	}
	layout.width = best->width;
	layout.height = best->height;
	layout.area = best->area;
	layout.track = best->track;
	layout.variant = best->tag;
	layout.routes = best->routes;
	layout.micros = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - t0).count();
	return true;
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
	if (topology.name == "T1") return fail("the one-lane T keeps the §2a layout (its mapped layout breaks the bay rules)");
	if (!t2 && topology.name != "LL2") return SolvePlanarMapped(topology, spec, layout, why);
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
	layout.track = best->track;
	layout.variant = best->tag;
	layout.routes = best->routes;
	layout.micros = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - t0).count();
	return true;
}

/**
 * Solve a topology's component layout from its channel graph (D327): every variant of its open crossings and tie sites, each
 * on its own thread; the least area inside the outline wins.
 * @param topology The topology.
 * @param spec The spec.
 * @param layout The layout found.
 * @param why The reason on failure.
 * @return Whether a layout was found.
 */
bool SolvePlanarMapped(const PlanarTopology &topology, const JunctionSpec &spec, PlanarComponentLayout &layout, std::string *why)
{
	const Clock::time_point t0 = Clock::now();
	const PlanarChannels channels = BuildPlanarChannels(topology);
	const Params params = MakeParams(spec);
	const size_t open = PlanarOpenCrossings(channels).size();
	size_t ties = 0;
	for (const PlanarSite &site : channels.sites) {
		if (!PlanarSitePerNode(site)) {
			ties += site.straight == UINT32_MAX ? 1 : 0;
			continue;
		}
		for (uint32_t n : site.nodes) ties += !topology.nodes[n].IsSplit() && channels.node_legs[n].straight == UINT32_MAX ? 1 : 0;
	}
	const size_t triangles = PlanarTriangles(channels).size();
	const size_t steps = PlanarCandidateSteps(topology, channels).size();
	if (open + ties + triangles + steps > 6 && std::getenv("PLANAR_ONLY") == nullptr && std::getenv("PLANAR_DRY") == nullptr) {
		if (why != nullptr) *why = fmt::format("{} open crossings, {} tie sites, {} triangles and {} candidate steps are too many variants", open, ties, triangles, steps);
		return false;
	}
	std::vector<PlanarChannels> flipped;
	for (uint64_t f = 0; f < (uint64_t{1} << triangles); f++) flipped.push_back(FlipPlanarTriangles(channels, f));
	std::vector<MappedVariant> variants;
	for (uint64_t f = 0; f < (uint64_t{1} << triangles); f++) {
		for (uint64_t o = 0; o < (uint64_t{1} << open); o++) {
			for (uint64_t k = 0; k < (uint64_t{1} << ties); k++) {
				for (uint64_t st = 0; st < (uint64_t{1} << steps); st++) variants.push_back({o, k, f, st});
			}
		}
	}
	if (const char *only = std::getenv("PLANAR_ONLY"); only != nullptr) { // debug: one variant by tag
		std::erase_if(variants, [&](const MappedVariant &v) { return fmt::format("o{}t{}f{}s{}", v.over, v.ties, v.flips, v.steps) != only; });
	}
	std::vector<VariantOutcome> outcomes(variants.size());
	std::vector<std::thread> threads;
	for (size_t i = 0; i < variants.size(); i++) {
		threads.emplace_back([&, i] { outcomes[i] = SolveMappedVariant(topology, flipped[variants[i].flips], params, variants[i]); });
	}
	for (std::thread &thread : threads) thread.join();
	return PickBest(outcomes, t0, layout, why);
}
