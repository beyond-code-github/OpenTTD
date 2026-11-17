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

} // namespace PlanarModel
