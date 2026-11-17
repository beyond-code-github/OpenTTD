/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file planar_model.cpp The planar mapped composer's integer program: half-tile geometry, linear expressions, strands of segments, decks, crossings and the solve. */

#include "../../stdafx.h"

#include "planar_model.h"

#include <algorithm>
#include <set>

#include "../../safeguards.h"

namespace PlanarModel {

int Turn(int h, int k)
{
	return ((h + k) % 8 + 8) % 8;
}

int Rot(int a, int b)
{
	const int d = ((b - a) % 8 + 8) % 8;
	return d > 4 ? d - 8 : d;
}

bool IsAxis(int d)
{
	return d % 2 == 0;
}

const std::array<int, 2> &Right(int h)
{
	return STEP[Turn(h, 2)];
}

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

Con Ge(const Ex &a, const Ex &b) { return {false, a - b}; }

Con Le(const Ex &a, const Ex &b) { return {false, b - a}; }

Con Eq(const Ex &a, const Ex &b) { return {true, a - b}; }

double Ev(const std::vector<int64_t> &val, const Ex &e)
{
	double r = e.constant;
	for (const auto &[k, v] : e.terms) r += static_cast<double>(val[k]) * v;
	return r;
}

void Model::PieceCurves(const RouteList &routes)
{
	const Params &pr = this->params;
	TagScope ts_curve("curve");
	std::set<std::pair<std::vector<std::pair<uint32_t, double>>, double>> seen;
	const auto addrow = [&](const Ex &e) {
		std::vector<std::pair<uint32_t, double>> key(e.terms.begin(), e.terms.end());
		if (seen.emplace(std::move(key), e.constant).second) this->Add(Ge(e, 0));
	};
	for (const auto &route : routes) {
		for (const std::vector<Seg> &seq : RouteSegs(*this, route)) {
			for (const Seg &seg : seq) {
				if (seg.z.has_value()) this->Fail(fmt::format("an optional segment on a route of required pieces: {}#{}", seg.strand, seg.i));
			}
			struct Curve {
				Ex at;
				bool left_hand;
			};
			std::vector<Curve> curves;
			Ex acc;
			for (size_t k = 0; k + 1 < seq.size(); k++) {
				const Seg &a = seq[k], &b = seq[k + 1];
				acc = acc + a.L;
				if (a.d != b.d) {
					if (std::abs(Rot(a.d, b.d)) != 1) this->Fail(fmt::format("a route turns by more than one eighth {}#{} d{} -> {}#{} d{}", a.strand, a.i, a.d, b.strand, b.i, b.d));
					curves.push_back({acc, Rot(a.d, b.d) > 0});
				}
			}
			for (size_t i = 0; i < curves.size(); i++) {
				int mand = 0;
				for (size_t j = i + 1; j < curves.size(); j++) {
					if (mand >= 3) break;
					const Ex e = curves[j].at - curves[i].at;
					if (mand >= 1) {
						addrow(e - std::min(2 * pr.tl - 1, (mand + 1) * pr.cl));
					} else {
						addrow(e - (curves[i].left_hand != curves[j].left_hand ? 2 : pr.g));
					}
					mand++;
				}
			}
		}
	}
}

std::vector<std::vector<Seg>> RouteSegs(const Model &M, const std::vector<std::string> &route)
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

void Model::ExplainInfeasible(const std::vector<Row> &extra)
{
	const size_t nr = this->rows.size();
	const size_t total = nr + extra.size();
	const auto row_at = [&](size_t r) -> const Row & { return r < nr ? this->rows[r] : extra[r - nr]; };
	std::vector<size_t> core;
	for (const uint32_t r : this->iis_rows) core.push_back(r);
	if (core.empty()) {
		fmt::print(stderr, "IIS integer-only (LP feasible)\n");
		return;
	}

	/* Every row outside the set is relaxed, then the set is cut down: a row whose removal leaves the LP infeasible is dropped for good. */
	const std::vector<double> keep_last = this->last;
	std::set<size_t> dropped;
	for (size_t r = 0; r < total; r++) {
		if (std::ranges::find(core, r) == core.end()) dropped.insert(r);
	}
	this->relax = true;
	for (size_t k = 0; k < core.size();) {
		if (core[k] >= total) { // a parity row is not a row of the LP: it stays
			k++;
			continue;
		}
		this->skip = dropped;
		this->skip.insert(core[k]);
		Solution q;
		if (!this->Solve(extra, q) && this->last_infeasible) {
			dropped.insert(core[k]);
			core.erase(core.begin() + k);
		} else {
			k++;
		}
	}
	this->skip = dropped;
	Solution q;
	const bool alone = !this->Solve(extra, q) && this->last_infeasible;
	this->relax = false;
	this->skip.clear();
	this->last = keep_last;
	if (!alone) fmt::print(stderr, "IIS core is not LP-infeasible alone (a solver limit); the rows below are not a proven conflict\n");

	std::map<uint32_t, std::string> names;
	const auto name_var = [&](uint32_t index, const std::string &name) {
		std::string &n = names[index];
		n += (n.empty() ? "" : "=") + name;
	};
	for (const auto &[name, st] : this->strands) {
		name_var(st.start[0].index, name + ".start.x");
		name_var(st.start[1].index, name + ".start.y");
		for (const Seg &sg : st.segs) {
			name_var(sg.L.index, fmt::format("{}#{}.L", name, sg.i));
			name_var(sg.b[0].index, fmt::format("{}#{}.end.x", name, sg.i));
			name_var(sg.b[1].index, fmt::format("{}#{}.end.y", name, sg.i));
		}
	}
	name_var(this->X.index, "X");
	name_var(this->Y.index, "Y");
	for (const size_t r : core) {
		if (r >= total) {
			fmt::print(stderr, "IIS parity\n");
			continue;
		}
		const Row &row = row_at(r);
		std::string text;
		for (const auto &[index, coef] : row.terms) text += fmt::format(" {:+g}*{}", coef, names.contains(index) ? names[index] : fmt::format("v{}", index));
		if (row.lower == row.upper) {
			text += fmt::format(" = {:g}", row.lower + 0.0);
		} else {
			if (row.lower > -INF) text += fmt::format(" >= {:g}", row.lower + 0.0);
			if (row.upper < INF) text += fmt::format(" <= {:g}", row.upper + 0.0);
		}
		fmt::print(stderr, "IIS {}{} :{}\n", r >= nr ? "(extra) " : "", row.tag, text);
	}
}

} // namespace PlanarModel
