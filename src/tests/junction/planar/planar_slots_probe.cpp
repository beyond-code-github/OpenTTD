/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file planar_slots_probe.cpp The G1 probe: Slots' joint solve of lengths and steps with no clash rows, and its clash and curve oracles (SPEC-slots-lengths section 8). */

#include "../../../stdafx.h"

#include "../../../3rdparty/catch2/catch.hpp"

#include "../../../junction/junction_spec.h"
#include "../../../junction/placement/junction_milp.h"
#include "../../../junction/planar/planar_channels.h"
#include "../../../junction/planar/planar_layout.h"
#include "../../../junction/planar/planar_model.h"
#include "../../../junction/planar/planar_plan.h"
#include "../../../junction/planar/planar_shape_layer.h"
#include "../../../junction/planar/planar_structure.h"
#include "../../../junction/planar/planar_topology.h"
#include "../../../junction/topology/junction_chain.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "../../../3rdparty/fmt/format.h"

#include "../../../safeguards.h"

namespace {

/** The answer of one G1 solve and its oracles. */
struct G1Result {
	bool solved; ///< An optimal solution was found.
	std::string why; ///< The status and, when infeasible, the tags of the conflicting rows.
	double ms; ///< The time of the solve.
	int step_lanes; ///< The lanes of all steps, cusps included.
	std::vector<std::string> clashes; ///< The clash oracle's hits, one line each.
	size_t same_channel, face, noface; ///< The hits by kind.
	std::vector<std::string> curve_breaches; ///< The curve oracle's breaches, one line each.
	size_t consistency; ///< Crossings and nodes whose vertices differ after the walk.
	size_t no_chain; ///< Beside tracks and pieces the beside rows skipped.
	int width_tiles, height_tiles; ///< The box in tiles.
	std::vector<std::string> audit; ///< AuditG1's breaches, one line each.
	std::vector<std::string> mod2; ///< G1i: the mod-2 check's report, when g1_mod2 is set.
	std::vector<std::string> forbidden_text; ///< G1k: the step places the grid forbids, one line each.
	std::set<uint32_t> forbidden; ///< G1k: their binaries.
	std::map<uint32_t, int> forced; ///< G1k: the variables whose parity the mod-2 system forces.
	std::vector<std::string> iis; ///< G1k: when tightened and infeasible, the conflicting rows with their terms and the tightened variables in them.
};

/** A linear expression. */
struct Lin {
	std::map<uint32_t, double> terms;
	double constant = 0;

	void Add(uint32_t variable, double coefficient) { this->terms[variable] += coefficient; }

	void Add(const Lin &other, double multiplier = 1)
	{
		for (const auto &[variable, coefficient] : other.terms) this->terms[variable] += coefficient * multiplier;
		this->constant += other.constant * multiplier;
	}
};

/** One element of a track's travel: a part of a piece, or the diagonal or sideways piece of a step. */
struct Elem {
	enum class Kind : uint8_t { Part, StepP, StepM } kind;
	uint32_t var; ///< The part's length, or the step's lane count.
	std::array<int, 2> disp; ///< The displacement per unit of the variable.
	int length; ///< The travel per unit of the variable.
	int heading; ///< The heading the element runs at (mod 8).
	int piece, stretch; ///< The piece and stretch it belongs to.
	bool fixed; ///< A part or a cusp step (a bend of the route's fixed curves), not an optional step.
	uint32_t zp = UINT32_MAX, zm = UINT32_MAX; ///< For an optional step, its two binaries.
};

/** One optional step place. */
struct Place {
	uint32_t stretch, piece;
	uint32_t p, m, zp, zm;
};

/** The elements of one track and the index ranges they fall in. */
struct TrackElems {
	std::vector<Elem> elems;
	std::vector<size_t> stretch_begin, stretch_end; ///< Per stretch, its element range.
	std::vector<size_t> piece_begin, piece_end; ///< Per piece, its element range.
	std::map<std::pair<uint32_t, uint32_t>, std::pair<uint32_t, uint32_t>> cell; ///< (stretch, piece) -> its first and last part variable.
};

/** One span: a separator's deck or a copy's deck. */
struct Span {
	uint32_t track;
	size_t first, last; ///< Event indices on the track.
	bool diag;
	int clear, lim;
};

/** The walk of one solved track. */
struct SolvedTrack {
	std::vector<std::array<int, 2>> vertices;
	std::vector<int> headings; ///< Per segment.
	std::vector<uint32_t> segment_stretch; ///< Per segment.
	std::vector<size_t> event_vertex; ///< Per stretch, the vertex that ends it (the events, then the end).
	std::vector<std::pair<int, int>> runs; ///< (heading, length) of the nonzero segments, same headings merged.
};

/** The rule values a stand-in for Structure's hand-down (SPEC-slots-lengths 8.1). */
struct G1Rules {
	int tl, cl, g, bay, coverage, node_leg, cross;
};

/** The row families the ablation can leave out (SolveG1's `off` bits). */
static const uint32_t G1_FAMILIES = 9;

/** The family bit of a row tag: 1 beside, 2 span clear, 4 deck gap and limit, 8 landing, 16 crossers, 32 node legs, 64 coverage, 128 bays, 256 curves; 0 for every other row. */
static uint32_t G1Family(const std::string &tag)
{
	if (tag.starts_with("beside")) return 1;
	if (tag.starts_with("span ")) {
		if (tag.ends_with(" clear")) return 2;
		if (tag.ends_with(" gap") || tag.ends_with(" middle")) return 4;
		if (tag.ends_with(" cross")) return 16;
	}
	if (tag.starts_with("landing")) return 8;
	if (tag.starts_with("leg ")) return 32;
	if (tag.starts_with("coverage")) return 64;
	if (tag.starts_with("bay ")) return 128;
	if (tag.starts_with("curve")) return 256;
	return 0;
}

/** G1i: one row as SolveG1 wrote it, for the mod-2 check. */
struct Mod2Row {
	std::vector<std::pair<uint32_t, double>> terms;
	double lower, upper;
	std::string tag;
};

/** G1i: one variable's bounds and kind. */
struct Mod2Var {
	double lower, upper;
	bool integer;
};

/** G1i: a row tag's kind, its words up to the first that starts with a digit ("parity step vertex 3 4" -> "parity step vertex"). */
static std::string G1Kind(const std::string &tag)
{
	std::string kind;
	size_t at = 0;
	while (at < tag.size()) {
		size_t end = tag.find(' ', at);
		if (end == std::string::npos) end = tag.size();
		if (std::isdigit(static_cast<unsigned char>(tag[at]))) break;
		if (!kind.empty()) kind += ' ';
		kind += tag.substr(at, end - at);
		at = end + 1;
	}
	return kind;
}

/** G1i: the row kinds SolveG1 leaves out (the kind filter). */
static std::set<std::string> g1_skip_kinds;

/** G1k: variable -> forced parity; when set, SolveG1 moves each such variable's integer bounds inward to that parity. */
static std::map<uint32_t, int> g1_tighten;

/** G1k: variables whose upper bound SolveG1 sets to 0 (step binaries of places the grid forbids). */
static std::set<uint32_t> g1_zero;

/** G1k: the variable names of the last mod-2 run. */
static std::map<uint32_t, std::string> g1_names;

/** G1i: the row kinds a mod-2 run saw. */
static std::set<std::string> g1_kinds_seen;

/** When set, SolveG1 runs the mod-2 check instead of the solve and puts its report in G1Result::mod2. */
static bool g1_mod2 = false;

/**
 * G1i (review F6, L8c): the equality and parity rows over the integers mod 2.
 * Variables with `fixed` values (step binaries) or equal bounds are substituted; rows with the same tag and terms are intersected,
 * and every row (or pair) that pins its expression to one value is an equation mod 2. Rows with a continuous variable or a
 * non-integer coefficient are left out (they can absorb any parity). Gaussian elimination over GF(2) gives a solution or a
 * contradiction; each contradiction is shrunk by a deletion filter to a minimal set of rows, reported, removed, and the
 * elimination is run again, up to `max_certificates`.
 */
static std::vector<std::string> Mod2Check(const std::vector<Mod2Row> &rows, const std::vector<Mod2Var> &vars, const std::map<uint32_t, int> &fixed, const std::map<uint32_t, std::string> &names, size_t max_certificates, std::map<uint32_t, int> *forced = nullptr)
{
	std::vector<std::string> out;
	const auto is_int = [](double d) { return std::abs(d - std::round(d)) < 1e-9; };
	const auto fixed_value = [&](uint32_t v, double &value) {
		auto it = fixed.find(v);
		if (it != fixed.end()) { value = it->second; return true; }
		if (vars[v].lower == vars[v].upper) { value = vars[v].lower; return true; }
		return false;
	};
	/* Substitute and group by (tag, residual terms). */
	struct Group { std::vector<std::pair<uint32_t, double>> terms; double lower, upper; std::string tag; };
	std::map<std::string, Group> groups;
	for (const Mod2Row &r : rows) {
		Group g{{}, r.lower, r.upper, r.tag};
		double shift = 0;
		for (const auto &[v, c] : r.terms) {
			double value;
			if (fixed_value(v, value)) shift += c * value; else g.terms.emplace_back(v, c);
		}
		g.lower -= shift;
		g.upper -= shift;
		std::sort(g.terms.begin(), g.terms.end());
		std::string key = r.tag;
		for (const auto &[v, c] : g.terms) key += fmt::format("|{}:{}", v, c);
		auto [it, fresh] = groups.try_emplace(key, g);
		if (!fresh) {
			it->second.lower = std::max(it->second.lower, g.lower);
			it->second.upper = std::min(it->second.upper, g.upper);
		}
	}
	/* The equations mod 2. */
	struct Eq { std::vector<uint32_t> odd; int rhs; std::string text; };
	std::vector<Eq> eqs;
	size_t skipped = 0, empty_bad = 0;
	std::map<uint32_t, uint32_t> column;
	for (const auto &[key, g] : groups) {
		if (std::abs(g.upper - g.lower) > 1e-9) continue;
		bool ok = is_int(g.lower);
		for (const auto &[v, c] : g.terms) ok = ok && vars[v].integer && is_int(c);
		if (!ok) { skipped++; continue; }
		Eq e{{}, static_cast<int>(((std::llround(g.lower) % 2) + 2) % 2), ""};
		std::string text = g.tag + ":";
		for (const auto &[v, c] : g.terms) {
			if ((std::llabs(std::llround(c)) % 2) == 1) {
				e.odd.push_back(v);
				column.try_emplace(v, static_cast<uint32_t>(column.size()));
			}
			auto n = names.find(v);
			text += fmt::format(" {:+g}*{}", c, n != names.end() ? n->second : fmt::format("v{}", v));
		}
		e.text = text + fmt::format(" = {:g}", g.lower);
		if (e.odd.empty() && e.rhs == 0) continue;
		if (e.odd.empty()) empty_bad++;
		eqs.push_back(std::move(e));
	}
	out.push_back(fmt::format("rows={} groups={} equations={} skipped={} unknowns={} constant_contradictions={}", rows.size(), groups.size(), eqs.size(), skipped, column.size(), empty_bad));
	/* Elimination over a subset of the equations; returns the history (equation indices) of the first contradiction, or empty. */
	const size_t nw = (column.size() + 63) / 64;
	const auto contradiction = [&](const std::vector<size_t> &subset) -> std::vector<size_t> {
		const size_t hw = (subset.size() + 63) / 64;
		std::vector<std::vector<uint64_t>> piv_bits(column.size()), piv_hist(column.size());
		std::vector<int> piv_rhs(column.size(), -1);
		for (size_t s = 0; s < subset.size(); s++) {
			const Eq &e = eqs[subset[s]];
			std::vector<uint64_t> bits(nw, 0), hist(hw, 0);
			for (const uint32_t v : e.odd) { const uint32_t c = column.at(v); bits[c / 64] ^= 1ULL << (c % 64); }
			hist[s / 64] |= 1ULL << (s % 64);
			int rhs = e.rhs;
			bool placed = false;
			for (size_t w = 0; w < nw && !placed; w++) {
				while (bits[w] != 0) {
					const uint32_t c = static_cast<uint32_t>(w * 64 + std::countr_zero(bits[w]));
					if (piv_rhs[c] < 0) {
						piv_bits[c] = bits; piv_hist[c] = hist; piv_rhs[c] = rhs; placed = true;
						break;
					}
					for (size_t i = 0; i < nw; i++) bits[i] ^= piv_bits[c][i];
					for (size_t i = 0; i < hw; i++) hist[i] ^= piv_hist[c][i];
					rhs ^= piv_rhs[c];
				}
			}
			if (!placed && rhs == 1) {
				std::vector<size_t> h;
				for (size_t i = 0; i < subset.size(); i++) if ((hist[i / 64] >> (i % 64)) & 1) h.push_back(subset[i]);
				return h;
			}
		}
		return {};
	};
	std::vector<size_t> live(eqs.size());
	for (size_t i = 0; i < eqs.size(); i++) live[i] = i;
	for (size_t n = 0; n < max_certificates; n++) {
		std::vector<size_t> cert = contradiction(live);
		if (cert.empty()) {
			out.push_back(fmt::format("consistent after {} certificates", n));
			if (forced != nullptr && n == 0) {
				/* The variables whose parity is the same in every solution: their unit vector lies in the row space. Pivot c's lowest bit is c. */
				std::vector<std::vector<uint64_t>> piv(column.size());
				std::vector<int> piv_rhs(column.size(), -1);
				for (const Eq &e : eqs) {
					std::vector<uint64_t> bits(nw, 0);
					for (const uint32_t v : e.odd) { const uint32_t c = column.at(v); bits[c / 64] ^= 1ULL << (c % 64); }
					int rhs = e.rhs;
					for (size_t w = 0; w < nw; w++) {
						bool placed = false;
						while (bits[w] != 0) {
							const uint32_t c = static_cast<uint32_t>(w * 64 + std::countr_zero(bits[w]));
							if (piv_rhs[c] < 0) { piv[c] = bits; piv_rhs[c] = rhs; placed = true; break; }
							for (size_t i = 0; i < nw; i++) bits[i] ^= piv[c][i];
							rhs ^= piv_rhs[c];
						}
						if (placed) break;
					}
				}
				for (const auto &[v, c0] : column) {
					std::vector<uint64_t> bits(nw, 0);
					bits[c0 / 64] = 1ULL << (c0 % 64);
					int rhs = 0;
					bool in_span = true;
					for (size_t w = 0; w < nw && in_span; w++) {
						while (bits[w] != 0) {
							const uint32_t c = static_cast<uint32_t>(w * 64 + std::countr_zero(bits[w]));
							if (piv_rhs[c] < 0) { in_span = false; break; }
							for (size_t i = 0; i < nw; i++) bits[i] ^= piv[c][i];
							rhs ^= piv_rhs[c];
						}
					}
					if (in_span) (*forced)[v] = rhs;
				}
				out.push_back(fmt::format("forced parities: {} of {} unknowns", forced->size(), column.size()));
			}
			break;
		}
		/* Deletion filter: drop each row whose removal keeps the set contradictory. */
		for (size_t i = 0; i < cert.size();) {
			std::vector<size_t> without = cert;
			without.erase(without.begin() + static_cast<std::ptrdiff_t>(i));
			if (!contradiction(without).empty()) cert = without; else i++;
		}
		out.push_back(fmt::format("certificate {}: {} rows", n, cert.size()));
		for (const size_t i : cert) out.push_back(fmt::format("  {}", eqs[i].text));
		std::set<size_t> drop(cert.begin(), cert.end());
		std::erase_if(live, [&](size_t i) { return drop.contains(i); });
	}
	return out;
}

/** The solve's time limit in seconds, and the count of solves that ended neither optimal nor infeasible (the G1h filter reads both). */
static double g1_time_limit = 15.0;
static size_t g1_timeouts = 0;

/** One node stub (D380): the leg, its node's trunk, whether the leg starts at the node (a split), the trunk's heading and the stub variable. */
struct StubRec {
	uint32_t leg, trunk;
	bool split;
	int heading;
	uint32_t var;
};

/** What AuditG1 reads of a solved run. */
struct AuditIn {
	const PlanarTrackMap &map;
	const PlanarShapeVariant &v;
	uint32_t off; ///< The run's ablation bits.
	const std::set<uint32_t> *grid_tracks;
	const std::vector<struct SolvedTrack> &solved;
	const std::vector<std::vector<std::string>> &vertex_kind;
	const std::vector<std::vector<std::array<int, 2>>> &elem_pos; ///< Per track, the walked position before each element (and after the last).
	const std::vector<struct TrackElems> &te;
	const std::vector<struct Span> &spans;
	const std::vector<StubRec> &stubs;
	const std::map<uint32_t, int> &cusp_start, &cusp_end;
	const std::vector<std::tuple<std::string, size_t, int, long>> &beside; ///< Per beside row: track name, piece, need, got.
	const std::vector<bool> &node_split;
	const std::function<int(uint32_t)> &value;
	int node_leg, cross;
};

/**
 * Audit the walked geometry of a solved run against the rows' families that are on in its `off` (BRIEF-G1-joint items 2-9, BRIEF-G1f item 1).
 * @return One line per breach: `off=<bits> family=<name> <track> v<i> <detail>`.
 */
static std::vector<std::string> AuditG1(const AuditIn &in)
{
	using Kind = Elem::Kind;
	const PlanarTrackMap &map = in.map;
	const size_t nt = map.tracks.size();
	std::vector<std::string> out;
	/* Continuous variables (512) and sideways steps (4096) are ablations that rewrite the model itself: there is no geometry to audit against the brief. */
	if ((in.off & (512 | 4096)) != 0) return out;
	const auto breach = [&](const char *family, const std::string &track, size_t vi, const std::string &detail) {
		out.push_back(fmt::format("off={} family={} {} v{} {}", in.off, family, track, vi, detail));
	};
	const auto seg_len = [&](uint32_t t, size_t i) {
		const auto &a = in.solved[t].vertices[i], &b = in.solved[t].vertices[i + 1];
		return std::max(std::abs(b[0] - a[0]), std::abs(b[1] - a[1]));
	};
	const auto path_len = [&](uint32_t t, size_t a, size_t b) {
		int sum = 0;
		for (size_t i = a; i < b; i++) sum += seg_len(t, i);
		return sum;
	};
	const auto stub_of = [&](uint32_t t, bool split) -> const StubRec * {
		for (const StubRec &s : in.stubs) {
			if (s.leg == t && s.split == split) return &s;
		}
		return nullptr;
	};
	const auto vertex_at = [&](uint32_t t, const std::array<int, 2> &pt) -> size_t {
		const auto &vs = in.solved[t].vertices;
		const auto it = std::ranges::find(vs, pt);
		return it == vs.end() ? 0 : static_cast<size_t>(it - vs.begin());
	};

	/* Grid. */
	if ((in.off & 1024) == 0) {
		for (uint32_t t = 0; t < nt; t++) {
			if (in.grid_tracks != nullptr && !in.grid_tracks->contains(t)) continue;
			const SolvedTrack &st = in.solved[t];
			const size_t nv = st.vertices.size();
			const StubRec *ss = stub_of(t, true), *es = stub_of(t, false);
			const int s_stub = ss != nullptr ? in.value(ss->var) : 0, s_h = ss != nullptr ? ss->heading : 0;
			const int e_stub = es != nullptr ? in.value(es->var) : 0, e_h = es != nullptr ? es->heading : 0;
			for (size_t i = 0; i < nv; i++) {
				const std::string &kind = in.vertex_kind[t][i];
				std::array<int, 2> pt = st.vertices[i];
				if (kind == "start") {
					pt = {pt[0] + s_stub * PlanarModel::STEP[s_h][0], pt[1] + s_stub * PlanarModel::STEP[s_h][1]};
				} else if (kind == "end") {
					pt = {pt[0] - e_stub * PlanarModel::STEP[e_h][0], pt[1] - e_stub * PlanarModel::STEP[e_h][1]};
				} else if (kind != "bend" && kind != "step") {
					continue;
				}
				if (((pt[0] + pt[1]) & 1) == 0) breach("grid", map.tracks[t].name, i, fmt::format("x+y even at ({},{}) kind={}", pt[0], pt[1], kind));
			}
			std::vector<size_t> nz;
			for (size_t i = 0; i + 1 < nv; i++) {
				if (seg_len(t, i) > 0) nz.push_back(i);
			}
			for (size_t a = 0; a < nz.size(); a++) {
				const size_t i = nz[a];
				const int h = st.headings[i], len = seg_len(t, i);
				if (h % 2 != 0) continue;
				if ((a == 0 && s_stub != 0 && h == s_h && len == 1) || (a + 1 == nz.size() && e_stub != 0 && h == e_h && len == 1)) continue;
				const int cross_coord = st.vertices[i][h % 4 == 0 ? 0 : 1];
				if ((cross_coord & 1) == 0) breach("grid", map.tracks[t].name, i, fmt::format("axis heading {} cross coordinate {} even", h, cross_coord));
			}
			for (size_t a = 0; a < nz.size();) {
				size_t b = a;
				int len = 0;
				const int h = st.headings[nz[a]];
				while (b < nz.size() && st.headings[nz[b]] == h) len += seg_len(t, nz[b++]);
				if (h % 2 == 0) {
					int adj = len;
					if (a == 0 && s_stub != 0 && h == s_h) adj -= 1;
					if (b == nz.size() && e_stub != 0 && h == e_h) adj -= 1;
					if (adj > 0 && (adj & 1) != 0) breach("grid", map.tracks[t].name, nz[a], fmt::format("axis heading {} run length {} odd", h, adj));
				}
				a = b;
			}
		}
	}

	/* Steps: heading and length of every step segment; cusp length and side. */
	for (uint32_t t = 0; t < nt; t++) {
		const TrackElems &e = in.te[t];
		std::vector<int> cusp_sigma;
		if (in.cusp_start.contains(t)) cusp_sigma.push_back(in.cusp_start.at(t));
		if (in.cusp_end.contains(t)) cusp_sigma.push_back(-in.cusp_end.at(t));
		size_t cusp_index = 0;
		for (size_t i = 0; i < e.elems.size(); i++) {
			const Elem &el = e.elems[i];
			if (el.kind == Kind::Part) continue;
			const int n = in.value(el.var);
			const int sigma = el.fixed && cusp_index < cusp_sigma.size() ? cusp_sigma[cusp_index++] : 0;
			if (n == 0) continue;
			const int ph = ((in.v.tracks[t].pieces[static_cast<size_t>(el.piece)] % 8) + 8) % 8;
			const int expect = el.kind == Kind::StepP ? (ph + 1) % 8 : (ph + 7) % 8;
			const std::array<int, 2> d = {in.elem_pos[t][i + 1][0] - in.elem_pos[t][i][0], in.elem_pos[t][i + 1][1] - in.elem_pos[t][i][1]};
			const int len = std::max(std::abs(d[0]), std::abs(d[1]));
			const size_t vi = vertex_at(t, in.elem_pos[t][i]);
			const char *what = el.fixed ? "cusp step" : "step";
			if (d[0] != len * PlanarModel::STEP[expect][0] || d[1] != len * PlanarModel::STEP[expect][1]) breach("steps", map.tracks[t].name, vi, fmt::format("{} displacement ({},{}) is not heading {} (piece heading {})", what, d[0], d[1], expect, ph));
			if (len != 2 * n) breach("steps", map.tracks[t].name, vi, fmt::format("{} length {} is not 2*{}", what, len, n));
			if (el.fixed) {
				if (len < 2) breach("steps", map.tracks[t].name, vi, fmt::format("cusp step length {} below 2", len));
				if (sigma != 0) {
					const int dot = d[0] * PlanarModel::STEP[(ph + 2) % 8][0] + d[1] * PlanarModel::STEP[(ph + 2) % 8][1];
					if ((dot > 0 ? 1 : -1) != sigma) breach("steps", map.tracks[t].name, vi, fmt::format("cusp step on side {} but the map's side is {}", dot > 0 ? 1 : -1, sigma));
				}
			}
		}
	}

	/* Stubs. */
	for (const StubRec &s : in.stubs) {
		const int stub = in.value(s.var);
		const std::array<int, 2> node_pt = s.split ? in.solved[s.trunk].vertices.back() : in.solved[s.trunk].vertices.front();
		const std::vector<std::array<int, 2>> &ep = in.elem_pos[s.leg];
		const auto &step = PlanarModel::STEP[s.heading];
		const std::array<int, 2> want = {node_pt[0] + (s.split ? 1 : -1) * stub * step[0], node_pt[1] + (s.split ? 1 : -1) * stub * step[1]};
		const std::array<int, 2> have_node = s.split ? ep.front() : ep.back();
		const std::array<int, 2> have = s.split ? ep[1] : ep[ep.size() - 2];
		const size_t vi = s.split ? 0 : in.solved[s.leg].vertices.size() - 1;
		if (have_node != node_pt || have != want) {
			breach("stubs", map.tracks[s.leg].name, vi, fmt::format("{} stub={} node ({},{}) leg's node end ({},{}) leg's stub end ({},{}) expected ({},{})", s.split ? "start" : "end", stub, node_pt[0], node_pt[1], have_node[0], have_node[1], have[0], have[1], want[0], want[1]));
		}
	}

	/* Beside. */
	if ((in.off & 1) == 0) {
		for (const auto &[name, piece, need, got] : in.beside) {
			if (got != need) breach("beside", name, 0, fmt::format("piece {} dot {} need {}", piece, got, need));
		}
	}

	/* Spans, crossers, node legs. */
	for (const Span &sp : in.spans) {
		const uint32_t t = sp.track;
		const SolvedTrack &st = in.solved[t];
		const size_t vf = st.event_vertex[sp.first], vl = st.event_vertex[sp.last];
		if ((in.off & 2) == 0) {
			const int before = vf > 0 ? seg_len(t, vf - 1) : 0, after = vl + 1 < st.vertices.size() ? seg_len(t, vl) : 0;
			if (before < sp.clear) breach("span-clear", map.tracks[t].name, vf, fmt::format("part before the first event {} below clear {}", before, sp.clear));
			if (after < sp.clear) breach("span-clear", map.tracks[t].name, vl, fmt::format("part after the last event {} below clear {}", after, sp.clear));
		}
		if ((in.off & 4) == 0) {
			for (size_t k = sp.first + 1; k <= sp.last; k++) {
				const int gap = path_len(t, st.event_vertex[k - 1], st.event_vertex[k]);
				if (gap < 2) breach("deck-gap", map.tracks[t].name, st.event_vertex[k], fmt::format("gap {} below 2", gap));
			}
			if (sp.lim != INT32_MAX) {
				const long middle = path_len(t, vf, vl), most = 2L * (sp.lim - 1 - (sp.diag ? 1 : 0));
				if (middle > most) breach("deck-gap", map.tracks[t].name, vf, fmt::format("middle {} above {}", middle, most));
			}
		}
		if ((in.off & 16) == 0) {
			for (size_t ev = sp.first; ev <= sp.last; ev++) {
				const uint32_t c = map.events[t][ev];
				const uint32_t x = map.crossings[c].a == t ? map.crossings[c].b : map.crossings[c].a;
				const size_t ec = static_cast<size_t>(std::ranges::find(map.events[x], c) - map.events[x].begin());
				const SolvedTrack &sx = in.solved[x];
				const size_t vi = sx.event_vertex[ec];
				const int before = vi > 0 ? seg_len(x, vi - 1) : 0, after = vi + 1 < sx.vertices.size() ? seg_len(x, vi) : 0;
				if (before < in.cross) breach("crossers", map.tracks[x].name, vi, fmt::format("crosser part before the crossing with {} is {}", map.tracks[t].name, before));
				if (after < in.cross) breach("crossers", map.tracks[x].name, vi, fmt::format("crosser part after the crossing with {} is {}", map.tracks[t].name, after));
			}
		}
	}
	if ((in.off & 32) == 0) {
		for (size_t n = 0; n < map.expanded.nodes.size(); n++) {
			const PlanarNode &node = map.expanded.nodes[n];
			const bool split = in.node_split[n];
			for (const uint32_t leg : split ? node.out : node.in) {
				const SolvedTrack &st = in.solved[leg];
				const size_t ne = map.events[leg].size();
				const size_t a = split ? 0 : (ne > 0 ? st.event_vertex[ne - 1] : 0), b = split ? st.event_vertex[0] : st.event_vertex[ne];
				const int len = path_len(leg, a, b);
				if (len < in.node_leg) breach("node-legs", map.tracks[leg].name, split ? a : b, fmt::format("leg at node {} travels {} below {}", n, len, in.node_leg));
			}
		}
	}
	return out;
}

/**
 * Solve the joint slots and lengths of one variant, then run the oracles.
 * @param off A bit set leaves that family's rows out (G1Family).
 */
static G1Result SolveG1(const PlanarTopology &topology, const JunctionSpec &spec, const PlanarShapeVariant &v, uint32_t off = 0, const std::set<uint32_t> *grid_tracks = nullptr, const std::string *skip_pin = nullptr, const std::set<std::string> *show_names = nullptr, const std::set<std::string> *seed_names = nullptr)
{
	using Kind = Elem::Kind;
	const PlanarTrackMap &map = v.structure.map;
	const size_t nt = map.tracks.size();
	const bool print_geometry = show_names != nullptr;
	G1Result r{};
	JunctionMilp lp;
	const double INF = JunctionMilp::INF;
	std::vector<std::string> tags;
	std::vector<Mod2Row> mod2_rows, all_rows;
	std::vector<Mod2Var> mod2_vars;
	std::map<uint32_t, std::string> mod2_names;
	const auto row = [&](const Lin &e, double lower, double upper, const std::string &tag) {
		if ((G1Family(tag) & off) != 0) return;
		if (skip_pin != nullptr && tag == *skip_pin) return;
		if (!g1_skip_kinds.empty() && g1_skip_kinds.contains(G1Kind(tag))) return;
		if ((off & 1024) != 0 && (tag.starts_with("parity ") || tag.starts_with("even "))) return;
		std::vector<std::pair<uint32_t, double>> terms;
		for (const auto &[variable, coefficient] : e.terms) {
			if (coefficient != 0) terms.emplace_back(variable, coefficient);
		}
		lp.AddRow(terms, lower - e.constant, upper - e.constant);
		tags.push_back(tag);
		if (!g1_tighten.empty()) all_rows.push_back({terms, lower - e.constant, upper - e.constant, tag});
		if (g1_mod2) {
			mod2_rows.push_back({terms, lower - e.constant, upper - e.constant, tag});
			g1_kinds_seen.insert(G1Kind(tag));
		}
	};
	uint32_t var_count = 0;
	std::vector<std::pair<uint32_t, std::string>> tightened;
	const auto var = [&](double lower, double upper, double cost, bool integer = true) {
		if (auto it = g1_tighten.find(var_count); it != g1_tighten.end()) {
			const auto odd = [](double d) { return static_cast<int>(((std::llround(d) % 2) + 2) % 2); };
			const double lo = lower, hi = upper;
			if (lower != -INF && odd(lower) != it->second) lower += 1;
			if (upper != INF && odd(upper) != it->second) upper -= 1;
			if (lo != lower || hi != upper) tightened.emplace_back(var_count, fmt::format("[{:g},{:g}]->[{:g},{:g}]", lo, hi, lower, upper));
		}
		if (g1_zero.contains(var_count)) upper = 0;
		var_count++;
		const uint32_t v = lp.AddVariable(lower, upper, cost, integer && (off & 512) == 0);
		if (g1_mod2) {
			if (mod2_vars.size() <= v) mod2_vars.resize(v + 1);
			mod2_vars[v] = {lower, upper, integer && (off & 512) == 0};
		}
		return v;
	};
	const auto lin = [](std::initializer_list<std::pair<uint32_t, double>> terms) {
		Lin e;
		for (const auto &[variable, coefficient] : terms) e.Add(variable, coefficient);
		return e;
	};

	/* The rule values: a stand-in for Structure's hand-down. */
	G1Rules rules{};
	rules.tl = spec.tl;
	rules.cl = static_cast<int>(DeriveCurveSpacing(spec.reference_speed, spec.curve_speed, spec.tl));
	rules.g = std::min(2 * rules.tl - 1, std::max(2, rules.cl));
	rules.bay = 2 * rules.tl + 4;
	rules.coverage = 2 * spec.prio_tiles + 4;
	rules.node_leg = 2;
	rules.cross = 1;

	/* 1. The box. */
	const uint32_t width_half = var(1, INF, 0), height_half = var(1, INF, 0);
	const uint32_t width = var(2, INF, 1), height = var(2, INF, 1);
	row(lin({{width, 1}, {width_half, -2}}), 0, 0, "box even");
	row(lin({{height, 1}, {height_half, -2}}), 0, 0, "box even");

	/* 2. The points: one per track end and event. */
	std::vector<std::vector<std::pair<uint32_t, uint32_t>>> point(nt);
	for (size_t t = 0; t < nt; t++) {
		for (size_t i = 0; i < map.events[t].size() + 2; i++) {
			const uint32_t x = var(0, INF, 0), y = var(0, INF, 0);
			point[t].emplace_back(x, y);
			mod2_names[x] = fmt::format("x({} {})", map.tracks[t].name, i);
			mod2_names[y] = fmt::format("y({} {})", map.tracks[t].name, i);
			row(lin({{x, 1}, {width, -1}}), -INF, 0, "box");
			row(lin({{y, 1}, {height, -1}}), -INF, 0, "box");
		}
	}

	/* The spans and the stretches inside them. */
	std::set<std::pair<uint32_t, size_t>> in_span;
	std::vector<Span> spans;
	const auto angle_odd = [&](uint32_t crossing) { return (v.angles[crossing] & 1) != 0; };
	for (size_t i = 0; i < v.structure.structures.size(); i++) {
		const PlanarStructure &st = v.structure.structures[i];
		const PlanarShapeStructure &shape = v.structures[i];
		int run = 0, lim = INT32_MAX;
		for (size_t kind = 0; kind < 2; kind++) {
			if (st.limit[kind] < 0) continue;
			run = std::max(run, shape.end_run[kind]);
			lim = std::min(lim, st.limit[kind]);
		}
		const auto add_span = [&](uint32_t track, size_t first, size_t last, uint32_t first_crossing) {
			Span s{track, first, last, angle_odd(first_crossing), 0, lim};
			s.clear = (s.diag ? 2 : 1) + 2 * run;
			spans.push_back(s);
			for (size_t k = first + 1; k <= last; k++) in_span.emplace(track, k);
		};
		if (!st.crossings.empty()) {
			const auto event_of = [&](uint32_t c) { return static_cast<size_t>(std::ranges::find(map.events[st.separator], c) - map.events[st.separator].begin()); };
			add_span(st.separator, event_of(st.crossings.front()), event_of(st.crossings.back()), st.crossings.front());
		}
		if (st.copy != UINT32_MAX && !map.events[st.copy].empty()) add_span(st.copy, 0, map.events[st.copy].size() - 1, map.events[st.copy].front());
	}

	/* 3. Nodes: legs tied to the node point; cusps are found here and become steps below. */
	const auto site_of = [&](size_t n) -> const PlanarStructureSite * {
		const PlanarStructureSite *site = nullptr;
		for (const PlanarStructureSite &candidate : v.structure.sites) {
			if (candidate.node == n) site = &candidate;
		}
		return site;
	};
	const auto is_split = [&](size_t n) {
		const PlanarNode &node = map.expanded.nodes[n];
		const PlanarStructureSite *site = site_of(n);
		return node.in.size() == 1 && (node.out.size() != 1 || (site != nullptr && site->split));
	};
	const auto tie = [&](std::pair<uint32_t, uint32_t> a, std::pair<uint32_t, uint32_t> b, const std::string &tag) {
		row(lin({{a.first, 1}, {b.first, -1}}), 0, 0, tag);
		row(lin({{a.second, 1}, {b.second, -1}}), 0, 0, tag);
	};
	std::map<uint32_t, std::pair<uint32_t, int>> stub_start, stub_end; ///< Per leg track, its stub variable and the trunk heading at the node (D380).
	std::vector<StubRec> stub_recs;
	std::map<uint32_t, int> cusp_start, cusp_end; ///< Per leg track, the sigma of its cusp at a split (stretch 0) or a merge (last stretch).
	for (size_t n = 0; n < map.expanded.nodes.size(); n++) {
		const PlanarNode &node = map.expanded.nodes[n];
		const PlanarStructureSite *site = site_of(n);
		const bool split = is_split(n);
		const std::vector<uint32_t> &trunks = split ? node.in : node.out, &legs = split ? node.out : node.in;
		const uint32_t trunk = trunks[0];
		std::string tag = fmt::format("node {} trunk", n);
		for (const uint32_t t : trunks) tag += " " + map.tracks[t].name;
		tag += " legs";
		for (const uint32_t t : legs) tag += " " + map.tracks[t].name;
		const auto reference = split ? point[trunk].back() : point[trunk].front();
		const int h = (((split ? v.tracks[trunk].pieces.back() : v.tracks[trunk].pieces.front()) % 8) + 8) % 8;
		for (const uint32_t t : trunks) {
			const int ht = (((split ? v.tracks[t].pieces.back() : v.tracks[t].pieces.front()) % 8) + 8) % 8;
			if (ht != h) throw std::runtime_error(fmt::format("STOP node {} has trunks of different headings ({} and {})", n, h, ht));
		}
		for (const uint32_t t : trunks) tie(reference, split ? point[t].back() : point[t].front(), tag);
		for (const uint32_t leg : legs) {
			const uint32_t stub = var(0, 1, 1);
			row(lin({{stub, 1}}), 0, (off & 8192) != 0 ? 0 : 1, fmt::format("stub {} {}", n, map.tracks[leg].name));
			(split ? stub_start : stub_end)[leg] = {stub, h};
			stub_recs.push_back({leg, trunk, split, h, stub});
			tie(reference, split ? point[leg].front() : point[leg].back(), tag);
			const int heading = (((split ? v.tracks[leg].pieces.front() : v.tracks[leg].pieces.back()) % 8) + 8) % 8;
			const bool straight = site != nullptr && leg == site->straight;
			if (straight || heading != h || site == nullptr) continue;
			const bool fork = site->straight == UINT32_MAX && legs.size() == 2;
			const uint32_t stretch = split ? 0 : static_cast<uint32_t>(map.events[leg].size());
			int sigma;
			if (fork) {
				const uint32_t other = legs[0] == leg ? legs[1] : legs[0];
				sigma = map.Side(leg, stretch, other, split ? 0 : static_cast<uint32_t>(map.events[other].size()));
			} else {
				sigma = map.Side(leg, stretch, site->straight, split ? 0 : static_cast<uint32_t>(map.events[site->straight].size()));
				if (sigma == 0) sigma = site->side;
			}
			if (sigma == 0) throw std::runtime_error(fmt::format("STOP cusp with no side: node {} leg {}", n, map.tracks[leg].name));
			if (in_span.contains({leg, stretch})) throw std::runtime_error(fmt::format("STOP cusp leg in a span: node {} leg {}", n, map.tracks[leg].name));
			(split ? cusp_start : cusp_end)[leg] = sigma;
		}
	}

	/* 4. Step places and the elements of every track. */
	std::vector<TrackElems> te(nt);
	std::vector<Place> places;
	int cost_steps = 0;
	(void)cost_steps;
	std::vector<std::vector<Place>> track_places(nt);
	std::vector<std::pair<uint32_t, uint32_t>> cusp_vars; ///< Unused marker, kept empty.
	std::set<uint32_t> step_vars; ///< Every p, m and cusp variable, for the lane count.
	for (uint32_t t = 0; t < nt; t++) {
		const PlanarShapeTrack &s = v.tracks[t];
		const size_t ne = map.events[t].size(), np = s.pieces.size();
		TrackElems &e = te[t];
		e.piece_begin.assign(np, SIZE_MAX);
		e.piece_end.assign(np, 0);
		const PlanarLine &line = map.expanded.lines[t];
		const auto heading_of = [&](size_t j) { return ((s.pieces[j] % 8) + 8) % 8; };
		for (size_t k = 0; k <= ne; k++) {
			const size_t first = k == 0 ? 0 : s.event_piece[k - 1], last = k == ne ? np - 1 : s.event_piece[k];
			const bool has_cusp = (k == 0 && cusp_start.contains(t)) || (k == ne && cusp_end.contains(t));
			int place_piece = -1;
			const bool every_piece = (off & 2048) != 0 && !has_cusp && !in_span.contains({t, k});
			if (!has_cusp && !in_span.contains({t, k})) {
				const bool ends_split = k == ne && !line.to.port && is_split(line.to.index);
				place_piece = static_cast<int>(ends_split ? last : first);
			}
			e.stretch_begin.push_back(e.elems.size());
			const auto add_stub = [&](const std::pair<uint32_t, int> &stub, size_t piece) {
				Elem el{};
				el.kind = Kind::Part;
				el.var = stub.first;
				el.disp = PlanarModel::STEP[stub.second];
				el.length = 1;
				el.heading = stub.second;
				el.piece = static_cast<int>(piece);
				el.stretch = static_cast<int>(k);
				el.fixed = true;
				e.elems.push_back(el);
			};
			if (k == 0 && stub_start.contains(t)) add_stub(stub_start.at(t), 0);
			for (size_t j = first; j <= last; j++) {
				const size_t cell_begin = e.elems.size();
				const int h = heading_of(j);
				const auto add = [&](Kind kind, uint32_t variable, bool fixed) {
					Elem el{};
					el.kind = kind;
					el.var = variable;
					el.piece = static_cast<int>(j);
					el.stretch = static_cast<int>(k);
					el.fixed = fixed;
					if (g1_mod2) mod2_names[variable] = fmt::format("{}({} s{} p{} h{})", kind == Kind::Part ? "len" : (kind == Kind::StepP ? "stepP" : "stepM"), map.tracks[t].name, k, j, h);
					if (kind == Kind::Part) {
						el.disp = PlanarModel::STEP[h];
						el.length = 1;
						el.heading = h;
					} else {
						el.heading = kind == Kind::StepP ? (h + 1) % 8 : (h + 7) % 8;
						el.disp = {2 * PlanarModel::STEP[el.heading][0], 2 * PlanarModel::STEP[el.heading][1]};
						el.length = 2;
						if ((off & 4096) != 0 && !fixed) {
							const int lane = h % 2 == 0 ? 2 : 1, sign = kind == Kind::StepP ? 1 : -1;
							el.disp = {sign * lane * PlanarModel::Right(h)[0], sign * lane * PlanarModel::Right(h)[1]};
						}
					}
					e.elems.push_back(el);
					return e.elems.size() - 1;
				};
				const auto cusp = [&](int sigma, bool split) {
					const uint32_t size = var(1, 8, 1000);
					step_vars.insert(size);
					/* A split's leg steps away from the straight leg to its side; a merge's leg steps back toward it. */
					const bool right = split ? sigma > 0 : sigma < 0;
					add(right ? Kind::StepP : Kind::StepM, size, true);
				};
				if (k == 0 && j == 0 && cusp_start.contains(t)) cusp(cusp_start.at(t), true);
				uint32_t first_part, last_part;
				if (place_piece == static_cast<int>(j) || every_piece) {
					const uint32_t before = var(0, INF, 1), after = var(0, INF, 1);
					const uint32_t p = var(0, 8, 1000), m = var(0, 8, 1000), zp = var(0, 1, 0), zm = var(0, 1, 0);
					step_vars.insert(p);
					step_vars.insert(m);
					row(lin({{p, 1}, {zp, -8}}), -INF, 0, "step");
					row(lin({{p, 1}, {zp, -1}}), 0, INF, "step");
					row(lin({{m, 1}, {zm, -8}}), -INF, 0, "step");
					row(lin({{m, 1}, {zm, -1}}), 0, INF, "step");
					row(lin({{zp, 1}, {zm, 1}}), -INF, 1, "step");
					add(Kind::Part, before, true);
					const size_t ip = add(Kind::StepP, p, false), im = add(Kind::StepM, m, false);
					e.elems[ip].zp = zp;
					e.elems[ip].zm = zm;
					e.elems[im].zp = zp;
					e.elems[im].zm = zm;
					add(Kind::Part, after, true);
					first_part = before;
					last_part = after;
					track_places[t].push_back({static_cast<uint32_t>(k), static_cast<uint32_t>(j), p, m, zp, zm});
				} else {
					const uint32_t part = var(0, INF, 1);
					add(Kind::Part, part, true);
					first_part = last_part = part;
				}
				if (k == ne && j == np - 1 && cusp_end.contains(t)) cusp(cusp_end.at(t), false);
				e.cell[{static_cast<uint32_t>(k), static_cast<uint32_t>(j)}] = {first_part, last_part};
				if (e.piece_begin[j] == SIZE_MAX) e.piece_begin[j] = cell_begin;
				e.piece_end[j] = e.elems.size();
			}
			if (k == ne && stub_end.contains(t)) add_stub(stub_end.at(t), np - 1);
			e.stretch_end.push_back(e.elems.size());
		}
	}
	/* A cusp can sit on a piece whose cell begins with its step: piece_begin must be the cell's first element, which it is. */

	/* The position of an element index on a track, as two expressions. */
	const auto position = [&](uint32_t t, size_t index) {
		std::array<Lin, 2> at;
		at[0].Add(point[t][0].first, 1);
		at[1].Add(point[t][0].second, 1);
		for (size_t i = 0; i < index; i++) {
			const Elem &el = te[t].elems[i];
			at[0].Add(el.var, el.disp[0]);
			at[1].Add(el.var, el.disp[1]);
		}
		return at;
	};
	const auto travel = [&](uint32_t t, size_t from, size_t to) {
		Lin sum;
		for (size_t i = from; i < to; i++) sum.Add(te[t].elems[i].var, te[t].elems[i].length);
		return sum;
	};
	const auto parts = [&](uint32_t t, size_t from, size_t to) {
		Lin sum;
		for (size_t i = from; i < to; i++) {
			if (te[t].elems[i].kind == Kind::Part) sum.Add(te[t].elems[i].var, 1);
		}
		return sum;
	};

	/* The stretch rows, the pieces' least totals, and the parity rows. */
	for (uint32_t t = 0; t < nt; t++) {
		const PlanarShapeTrack &s = v.tracks[t];
		const TrackElems &e = te[t];
		for (size_t k = 0; k <= map.events[t].size(); k++) {
			Lin dx, dy;
			dx.Add(point[t][k + 1].first, 1);
			dx.Add(point[t][k].first, -1);
			dy.Add(point[t][k + 1].second, 1);
			dy.Add(point[t][k].second, -1);
			for (size_t i = e.stretch_begin[k]; i < e.stretch_end[k]; i++) {
				dx.Add(e.elems[i].var, -e.elems[i].disp[0]);
				dy.Add(e.elems[i].var, -e.elems[i].disp[1]);
			}
			row(dx, 0, 0, fmt::format("stretch {} {} x {}", t, k, map.tracks[t].name));
			row(dy, 0, 0, fmt::format("stretch {} {} y {}", t, k, map.tracks[t].name));
		}
		const uint32_t start_aux = var(0, INF, 0);
		const bool grid = grid_tracks == nullptr || grid_tracks->contains(t);
		Lin start_parity = lin({{point[t][0].first, 1}, {point[t][0].second, 1}, {start_aux, -2}});
		if (stub_start.contains(t)) start_parity.Add(stub_start.at(t).first, PlanarModel::STEP[stub_start.at(t).second][0] + PlanarModel::STEP[stub_start.at(t).second][1]);
		if (grid) row(start_parity, 1, 1, fmt::format("parity start {}", map.tracks[t].name));
		for (size_t j = 0; j < s.pieces.size(); j++) {
			Lin total;
			for (size_t i = e.piece_begin[j]; i < e.piece_end[j]; i++) {
				if (e.elems[i].kind == Kind::Part) total.Add(e.elems[i].var, 1);
			}
			const int h = ((s.pieces[j] % 8) + 8) % 8;
			row(total, h % 2 == 0 ? 2 : 1, INF, fmt::format("piece {} {} {}", t, j, map.tracks[t].name));
			if (h % 2 == 0) {
				/* A step place on this piece: the step's first vertex (the end of its before part) has x + y odd when the step is used. */
				for (const Place &pl : track_places[t]) {
					if (pl.piece != j) continue;
					size_t index = 0;
					while (e.elems[index].kind != Kind::StepP || e.elems[index].var != pl.p) index++;
					const std::array<Lin, 2> vertex = position(t, index);
					Lin odd = vertex[0];
					odd.Add(vertex[1], 1);
					const uint32_t odd_aux = var(0, INF, 0);
					odd.Add(odd_aux, -2);
					Lin upper = odd, lower = odd;
					for (const uint32_t z : {pl.zp, pl.zm}) {
						upper.Add(z, 1000);
						lower.Add(z, -1000);
					}
					if (grid) row(upper, -INF, 1001, fmt::format("parity step vertex {} {}", t, j));
					if (grid) row(lower, -999, INF, fmt::format("parity step vertex {} {}", t, j));
				}
				Lin even;
				for (size_t i = e.piece_begin[j]; i < e.piece_end[j]; i++) even.Add(e.elems[i].var, e.elems[i].length);
				const uint32_t even_aux = var(0, INF, 0);
				even.Add(even_aux, -2);
				if (grid) row(even, 0, 0, fmt::format("even {} {}", t, j));
			}
			if (h % 2 != 0) continue;
			std::array<Lin, 2> at = position(t, e.piece_begin[j]);
			Lin cross = at[h % 4 == 0 ? 0 : 1];
			const uint32_t aux = var(0, INF, 0);
			cross.Add(aux, -2);
			if (grid) row(cross, 1, 1, fmt::format("parity piece {} {} {}", t, j, map.tracks[t].name));
		}
		/* A step or cusp step of an axis heading (on a diagonal piece) is an axis piece too: its cross coordinate is odd along it. */
		for (size_t i = 0; i < e.elems.size(); i++) {
			const Elem &el = e.elems[i];
			if (el.kind == Kind::Part || el.heading % 2 != 0) continue;
			const std::array<Lin, 2> at = position(t, i);
			Lin cross = at[el.heading % 4 == 0 ? 0 : 1];
			const uint32_t aux = var(0, INF, 0);
			cross.Add(aux, -2);
			const std::string tag = fmt::format("parity step {} {} {}", t, i, map.tracks[t].name);
			if (el.fixed) {
				if (grid) row(cross, 1, 1, tag);
				continue;
			}
			const uint32_t z = el.kind == Kind::StepP ? el.zp : el.zm;
			Lin upper = cross, lower = cross;
			upper.Add(z, 1000);
			lower.Add(z, -1000);
			if (grid) row(upper, -INF, 1001, tag);
			if (grid) row(lower, -999, INF, tag);
		}
	}

	/* 3 (continued). Crossings and ports. */
	for (uint32_t c = 0; c < map.crossings.size(); c++) {
		const uint32_t a = map.crossings[c].a, b = map.crossings[c].b;
		const auto at = [&](uint32_t t) { return point[t][static_cast<size_t>(std::ranges::find(map.events[t], c) - map.events[t].begin()) + 1]; };
		tie(at(a), at(b), fmt::format("cross {} {} {}", c, map.tracks[a].name, map.tracks[b].name));
	}
	for (uint32_t t = 0; t < nt; t++) {
		const PlanarLine &line = map.expanded.lines[t];
		for (const bool end : {false, true}) {
			const PlanarEnd &e = end ? line.to : line.from;
			if (!e.port) continue;
			const auto [x, y] = end ? point[t].back() : point[t].front();
			const std::string tag = fmt::format("port {} {}", t, map.tracks[t].name);
			switch (map.expanded.ports[e.index].side) {
				case PlanarSide::W: row(lin({{x, 1}}), 0, 0, tag); break;
				case PlanarSide::E: row(lin({{x, 1}, {width, -1}}), 0, 0, tag); break;
				case PlanarSide::N: row(lin({{y, 1}}), 0, 0, tag); break;
				case PlanarSide::S: row(lin({{y, 1}, {height, -1}}), 0, 0, tag); break;
			}
		}
	}
	{
		const std::vector<uint32_t> &ring = map.expanded.ring;
		const auto port_end = [&](uint32_t t) {
			const PlanarLine &line = map.expanded.lines[t];
			return line.from.port ? line.from : line.to;
		};
		for (size_t i = 1; i < ring.size(); i++) {
			const uint32_t t1 = ring[i - 1], t2 = ring[i];
			const PlanarLine &l1 = map.expanded.lines[t1], &l2 = map.expanded.lines[t2];
			const PlanarEnd e1 = port_end(t1), e2 = port_end(t2);
			const PlanarSide side = map.expanded.ports[e1.index].side;
			if (side != map.expanded.ports[e2.index].side) continue;
			const auto p1 = l1.from.port ? point[t1].front() : point[t1].back(), p2 = l2.from.port ? point[t2].front() : point[t2].back();
			const double d = e1.index == e2.index ? 2 : 2.0 * v.structure.side_gap[static_cast<size_t>(side)];
			const std::string tag = fmt::format("pin {} {} {} d={}", static_cast<int>(side), map.tracks[t1].name, map.tracks[t2].name, d);
			switch (side) {
				case PlanarSide::N: row(lin({{p2.first, 1}, {p1.first, -1}}), d, d, tag); break;
				case PlanarSide::E: row(lin({{p2.second, 1}, {p1.second, -1}}), d, d, tag); break;
				case PlanarSide::S: row(lin({{p2.first, 1}, {p1.first, -1}}), -d, -d, tag); break;
				case PlanarSide::W: row(lin({{p2.second, 1}, {p1.second, -1}}), -d, -d, tag); break;
			}
		}
	}

	/* 6. Beside tracks follow their parent chain one lane away. */
	std::vector<std::tuple<std::string, size_t, int, Lin>> beside_rows; ///< Every beside row family 1 would write, for the residual print.
	for (uint32_t b = 0; b < nt; b++) {
		if (map.part_of[b] != UINT32_MAX) continue;
		std::vector<uint32_t> chain;
		if (!ParentChain(map, v.structure.sites, b, chain)) {
			r.no_chain++;
			continue;
		}
		/* Each concatenated chain piece: its track and the piece in it; a node's piece once. */
		std::vector<std::pair<uint32_t, size_t>> chain_piece;
		std::vector<std::pair<uint32_t, size_t>> chain_event; ///< Per concatenated event, its track and event index.
		std::vector<size_t> chain_event_piece; ///< Per concatenated event, its concatenated piece.
		for (size_t i = 0; i < chain.size(); i++) {
			const PlanarShapeTrack &sc = v.tracks[chain[i]];
			const size_t base = chain_piece.size() - (i > 0 ? 1 : 0);
			for (size_t jj = i > 0 ? 1 : 0; jj < sc.pieces.size(); jj++) chain_piece.emplace_back(chain[i], jj);
			for (size_t ev = 0; ev < sc.event_piece.size(); ev++) {
				chain_event.emplace_back(chain[i], ev);
				chain_event_piece.push_back(base + sc.event_piece[ev]);
			}
		}
		const PlanarShapeTrack &sb = v.tracks[b];
		if (sb.pieces.size() != chain_piece.size()) {
			r.no_chain++;
			continue;
		}
		const TrackElems &eb = te[b];
		for (size_t j = 0; j < sb.pieces.size(); j++) {
			const int h = ((sb.pieces[j] % 8) + 8) % 8;
			/* The beside point: an event on the piece, else its end if not the last piece, else its start. */
			std::array<Lin, 2> beside, parent;
			size_t beside_stretch, parent_stretch;
			uint32_t parent_track;
			const auto event_on = std::ranges::find(sb.event_piece, static_cast<uint32_t>(j));
			if (event_on != sb.event_piece.end()) {
				const size_t ev = static_cast<size_t>(event_on - sb.event_piece.begin());
				beside[0].Add(point[b][ev + 1].first, 1);
				beside[1].Add(point[b][ev + 1].second, 1);
				beside_stretch = ev;
				const auto [pt, pev] = chain_event[ev];
				parent_track = pt;
				parent[0].Add(point[pt][pev + 1].first, 1);
				parent[1].Add(point[pt][pev + 1].second, 1);
				parent_stretch = pev;
			} else {
				const bool last_piece = j + 1 == sb.pieces.size();
				if (last_piece && sb.pieces.size() == 1) {
					r.no_chain++;
					continue;
				}
				beside = last_piece ? position(b, eb.piece_begin[j]) : position(b, eb.piece_end[j]);
				beside_stretch = static_cast<size_t>(eb.elems[last_piece ? eb.piece_begin[j] : eb.piece_end[j] - 1].stretch);
				const auto [pt, jj] = chain_piece[j];
				parent_track = pt;
				parent = position(pt, te[pt].piece_begin[jj]);
				parent_stretch = static_cast<size_t>(te[pt].elems[te[pt].piece_begin[jj]].stretch);
			}
			const int s = map.Side(b, static_cast<uint32_t>(beside_stretch), parent_track, static_cast<uint32_t>(parent_stretch));
			if (s == 0) throw std::runtime_error(fmt::format("STOP beside track {} piece {} shares no face with its chain track {}", map.tracks[b].name, j, map.tracks[parent_track].name));
			const auto &normal = PlanarModel::STEP[(h + 2) % 8];
			Lin dot;
			for (size_t axis = 0; axis < 2; axis++) {
				dot.Add(beside[axis], normal[axis]);
				dot.Add(parent[axis], -normal[axis]);
			}
			beside_rows.emplace_back(map.tracks[b].name, j, 2 * s, dot);
			row(dot, 2.0 * s, 2.0 * s, fmt::format("beside {} piece {}", map.tracks[b].name, j));
		}
	}

	/* 7. Spans. */
	std::map<uint32_t, std::vector<size_t>> spans_of;
	for (size_t i = 0; i < spans.size(); i++) spans_of[spans[i].track].push_back(i);
	for (const Span &sp : spans) {
		const uint32_t t = sp.track;
		const std::string tag = fmt::format("span {} {}", map.tracks[t].name, sp.first);
		const TrackElems &e = te[t];
		const PlanarShapeTrack &s = v.tracks[t];
		for (size_t k = sp.first + 1; k <= sp.last; k++) row(parts(t, e.stretch_begin[k], e.stretch_end[k]), 2, INF, tag + " gap");
		if (sp.last > sp.first) row(parts(t, e.stretch_begin[sp.first + 1], e.stretch_end[sp.last]), -INF, 2.0 * (sp.lim - 1 - (sp.diag ? 1 : 0)), tag + " middle");
		const uint32_t before = e.cell.at({static_cast<uint32_t>(sp.first), s.event_piece[sp.first]}).second;
		const uint32_t after = e.cell.at({static_cast<uint32_t>(sp.last + 1), s.event_piece[sp.last]}).first;
		row(lin({{before, 1}}), sp.clear, INF, tag + " clear");
		row(lin({{after, 1}}), sp.clear, INF, tag + " clear");
		/* 8. The crossers' sub-parts beside each crossing. */
		for (size_t ev = sp.first; ev <= sp.last; ev++) {
			const uint32_t c = map.events[t][ev];
			const uint32_t x = map.crossings[c].a == t ? map.crossings[c].b : map.crossings[c].a;
			const size_t ec = static_cast<size_t>(std::ranges::find(map.events[x], c) - map.events[x].begin());
			const uint32_t piece = v.tracks[x].event_piece[ec];
			row(lin({{te[x].cell.at({static_cast<uint32_t>(ec), piece}).second, 1}}), rules.cross, INF, tag + " cross");
			row(lin({{te[x].cell.at({static_cast<uint32_t>(ec + 1), piece}).first, 1}}), rules.cross, INF, tag + " cross");
		}
	}
	for (const auto &[t, list] : spans_of) {
		std::vector<size_t> sorted = list;
		std::ranges::sort(sorted, [&](size_t a, size_t b) { return spans[a].first < spans[b].first; });
		const PlanarShapeTrack &s = v.tracks[t];
		for (size_t i = 1; i < sorted.size(); i++) {
			const Span &a = spans[sorted[i - 1]], &b = spans[sorted[i]];
			if (s.event_piece[a.last] != s.event_piece[b.first]) continue;
			row(travel(t, te[t].stretch_begin[a.last + 1], te[t].stretch_end[b.first]), a.clear + b.clear + 2, INF, fmt::format("landing {} {}", map.tracks[t].name, a.last));
		}
	}

	/* 9. Node legs. */
	for (size_t n = 0; n < map.expanded.nodes.size(); n++) {
		const PlanarNode &node = map.expanded.nodes[n];
		const bool split = is_split(n);
		for (const uint32_t leg : split ? node.out : node.in) {
			const size_t k = split ? 0 : map.events[leg].size();
			row(travel(leg, te[leg].stretch_begin[k], te[leg].stretch_end[k]), rules.node_leg, INF, fmt::format("leg {} {}", n, map.tracks[leg].name));
		}
	}

	/* 10. Coverage of the prio rows. */
	for (uint32_t t = 0; t < nt; t++) {
		if (map.tracks[t].name.starts_with("prio:")) row(travel(t, 0, te[t].elems.size()), rules.coverage, INF, fmt::format("coverage {}", map.tracks[t].name));
	}

	/* 11. Bays: a non-prio route from its last topology split or port to each topology merge. */
	const auto topology_node = [&](size_t n) {
		const PlanarNode &node = map.expanded.nodes[n];
		for (const uint32_t t : node.in) if (map.part_of[t] == UINT32_MAX) return false;
		for (const uint32_t t : node.out) if (map.part_of[t] == UINT32_MAX) return false;
		return true;
	};
	std::set<std::pair<std::vector<std::pair<uint32_t, double>>, double>> seen;
	const auto once_row = [&](const Lin &e, double lower, double upper, const std::string &tag) {
		std::vector<std::pair<uint32_t, double>> key;
		for (const auto &[variable, coefficient] : e.terms) if (coefficient != 0) key.emplace_back(variable, coefficient);
		if (seen.emplace(std::move(key), lower - e.constant).second) row(e, lower, upper, tag);
	};
	for (size_t ri = 0; ri < map.expanded.routes.size(); ri++) {
		const PlanarRoute &route = map.expanded.routes[ri];
		if (route.prio) continue;
		for (const std::vector<uint32_t> &path : route.paths) {
			Lin acc, from_split;
			for (const uint32_t t : path) {
				acc.Add(travel(t, 0, te[t].elems.size()));
				const PlanarEnd &to = map.expanded.lines[t].to;
				if (to.port || !topology_node(to.index)) continue;
				if (is_split(to.index)) {
					from_split = acc;
				} else {
					Lin d = acc;
					d.Add(from_split, -1);
					once_row(d, rules.bay, INF, fmt::format("bay {} {}", route.name, to.index));
				}
			}
		}
	}

	/* 12. Curves along every route path. */
	struct Bend {
		Lin at;
		bool right;
	};
	for (size_t ri = 0; ri < map.expanded.routes.size(); ri++) {
		const PlanarRoute &route = map.expanded.routes[ri];
		for (const std::vector<uint32_t> &path : route.paths) {
			std::vector<Bend> bends;
			struct Marker {
				Lin start;
				uint32_t p, m, zp, zm;
				size_t bends_before;
			};
			std::vector<Marker> markers;
			Lin acc;
			int last_heading = -1;
			for (const uint32_t t : path) {
				for (const Elem &el : te[t].elems) {
					if (!el.fixed) {
						if (el.kind == Kind::StepP) {
							markers.push_back({acc, el.var, UINT32_MAX, el.zp, el.zm, bends.size()});
						} else {
							markers.back().m = el.var;
						}
						acc.Add(el.var, el.length);
						continue;
					}
					if (last_heading != -1 && last_heading != el.heading) {
						const int turn = PlanarModel::Rot(last_heading, el.heading);
						if (std::abs(turn) != 1) {
							std::string detail;
							for (const uint32_t q : path) {
								detail += " " + map.tracks[q].name + "[";
								for (const int h : v.tracks[q].pieces) detail += fmt::format("{} ", h);
								detail += "]";
							}
							throw std::runtime_error(fmt::format("STOP a route turns by {} eighths ({} to {}) at track {}, path{}", turn, last_heading, el.heading, map.tracks[t].name, detail));
						}
						bends.push_back({acc, turn > 0});
					}
					last_heading = el.heading;
					acc.Add(el.var, el.length);
				}
			}
			for (size_t i = 0; i < bends.size(); i++) {
				int mand = 0;
				for (size_t j = i + 1; j < bends.size(); j++) {
					if (mand >= 3) break;
					Lin d = bends[j].at;
					d.Add(bends[i].at, -1);
					const double least = mand >= 1 ? std::min(2 * rules.tl - 1, (mand + 1) * rules.cl) : (bends[i].right != bends[j].right ? 2 : rules.g);
					once_row(d, least, INF, fmt::format("curve {}", route.name));
					mand++;
				}
			}
			for (const Marker &mk : markers) {
				for (const bool use_p : {true, false}) {
					const uint32_t z = use_p ? mk.zp : mk.zm;
					/* The step's first and second bends: p turns right then left, m left then right. */
					Lin first = mk.start, second = mk.start;
					if (use_p) {
						second.Add(mk.p, 2);
					} else {
						first.Add(mk.p, 2);
						second.Add(mk.p, 2);
						second.Add(mk.m, 2);
					}
					const bool first_right = use_p;
					if (mk.bends_before > 0) {
						const Bend &prev = bends[mk.bends_before - 1];
						Lin d = first;
						d.Add(prev.at, -1);
						d.Add(z, -1000);
						once_row(d, (prev.right != first_right ? 2 : rules.g) - 1000, INF, fmt::format("curve step {}", route.name));
					}
					if (mk.bends_before < bends.size()) {
						const Bend &next = bends[mk.bends_before];
						Lin d = next.at;
						d.Add(second, -1);
						d.Add(z, -1000);
						once_row(d, (next.right != !first_right ? 2 : rules.g) - 1000, INF, fmt::format("curve step {}", route.name));
					}
				}
			}
		}
	}

	if (g1_mod2) {
		/* Step existence fixed off: a step's travel is even (2 per lane), so this is the weakest case mod 2; cusps stay (they are fixed). */
		std::map<uint32_t, int> fixed;
		for (const auto &places : track_places) {
			for (const Place &pl : places) {
				for (const uint32_t z : {pl.zp, pl.zm, pl.p, pl.m}) fixed[z] = 0;
			}
		}
		const auto start = std::chrono::steady_clock::now();
		std::map<uint32_t, int> forced;
		r.mod2 = Mod2Check(mod2_rows, mod2_vars, fixed, mod2_names, 8, &forced);
		for (const auto &[v, value] : fixed) forced.erase(v);
		r.forced = forced;
		g1_names = mod2_names;
		/* Per place and side: the grid with only that step on. A certificate means the grid forbids it there. */
		for (uint32_t t = 0; t < nt; t++) {
			for (const Place &pl : track_places[t]) {
				for (int side = 0; side < 2; side++) {
					std::map<uint32_t, int> one = fixed;
					const uint32_t z = side == 0 ? pl.zp : pl.zm, lanes = side == 0 ? pl.p : pl.m;
					one[z] = 1;
					one.erase(lanes);
					const std::vector<std::string> rep = Mod2Check(mod2_rows, mod2_vars, one, mod2_names, 1);
					if (rep.size() < 2 || !rep[1].starts_with("certificate")) continue;
					r.forbidden.insert(z);
					std::string line = fmt::format("{} stretch {} piece {} {}:", map.tracks[t].name, pl.stretch, pl.piece, side == 0 ? "P" : "M");
					for (size_t i = 2; i < rep.size(); i++) line += " |" + rep[i];
					r.forbidden_text.push_back(line);
				}
			}
		}
		size_t places = 0;
		for (const auto &pls : track_places) places += pls.size();
		r.mod2.push_back(fmt::format("step places {} sides {}, forbidden by the grid {}", places, 2 * places, r.forbidden.size()));
		r.ms = static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count()) / 1000.0;
		r.why = "mod2";
		return r;
	}

	lp.SetTimeLimit(g1_time_limit);
	const auto start = std::chrono::steady_clock::now();
	const JunctionMilp::Status status = lp.Solve();
	r.ms = static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count()) / 1000.0;
	r.solved = status == JunctionMilp::Status::Optimal;
	if (status != JunctionMilp::Status::Optimal && status != JunctionMilp::Status::Infeasible) g1_timeouts++;
	r.why = r.solved ? "Optimal" : (status == JunctionMilp::Status::Infeasible ? "Infeasible" : "Error");
	if (status == JunctionMilp::Status::Infeasible) {
		std::set<std::string> unique;
		for (const uint32_t i : lp.InfeasibleRows()) {
			unique.insert(tags[i]);
			if (g1_tighten.empty()) continue;
			const Mod2Row &row_rec = all_rows[i];
			std::string text = row_rec.tag + ":";
			for (const auto &[v, c] : row_rec.terms) {
				auto n = g1_names.find(v);
				std::string mark;
				for (const auto &[tv, what] : tightened) if (tv == v) mark = " TIGHT" + what;
				text += fmt::format(" {:+g}*{}{}", c, n != g1_names.end() ? n->second : fmt::format("v{}", v), mark);
			}
			r.iis.push_back(text + fmt::format(" in [{:g},{:g}]", row_rec.lower, row_rec.upper));
		}
		for (const std::string &t : unique) r.why += " " + t + ";";
	}
	if (!r.solved) return r;

	/* The solution. */
	const auto value = [&](uint32_t variable) { return static_cast<int>(std::lround(lp.Value(variable))); };
	for (const uint32_t sv : step_vars) r.step_lanes += value(sv);
	r.width_tiles = value(width) / 2;
	r.height_tiles = value(height) / 2;

	/* Consistency: walk every track's parts into a vertex list. */
	std::vector<SolvedTrack> solved(nt);
	std::vector<std::vector<std::string>> vertex_kind(nt);
	std::vector<std::vector<std::array<int, 2>>> elem_pos(nt);
	for (uint32_t t = 0; t < nt; t++) {
		SolvedTrack &st = solved[t];
		std::array<int, 2> at = {value(point[t][0].first), value(point[t][0].second)};
		st.vertices.push_back(at);
		vertex_kind[t].push_back("start");
		bool last_part = true;
		elem_pos[t].assign(te[t].elems.size() + 1, at);
		int current = -1, previous = ((v.tracks[t].pieces.front() % 8) + 8) % 8;
		for (size_t k = 0; k <= map.events[t].size(); k++) {
			for (size_t i = te[t].stretch_begin[k]; i < te[t].stretch_end[k]; i++) {
				const Elem &el = te[t].elems[i];
				const int n = value(el.var);
				elem_pos[t][i] = at;
				if (n == 0) continue;
				if (current != -1 && current != el.heading) {
					st.vertices.push_back(at);
					vertex_kind[t].push_back(el.kind != Kind::Part || !last_part ? "step" : "bend");
					st.headings.push_back(current);
					st.segment_stretch.push_back(static_cast<uint32_t>(k));
				}
				last_part = el.kind == Kind::Part;
				current = el.heading;
				at = {at[0] + n * el.disp[0], at[1] + n * el.disp[1]};
				const int length = n * el.length;
				if (!st.runs.empty() && st.runs.back().first == el.heading) {
					st.runs.back().second += length;
				} else {
					st.runs.emplace_back(el.heading, length);
				}
			}
			for (size_t i = te[t].stretch_end[k]; i <= te[t].elems.size(); i++) elem_pos[t][i] = at;
			const std::array<int, 2> expected = {value(point[t][k + 1].first), value(point[t][k + 1].second)};
			if (at != expected) r.consistency++;
			st.vertices.push_back(at);
			vertex_kind[t].push_back(k < map.events[t].size() ? "event" : "end");
			st.headings.push_back(current != -1 ? current : previous);
			st.segment_stretch.push_back(static_cast<uint32_t>(k));
			st.event_vertex.push_back(st.vertices.size() - 1);
			previous = st.headings.back();
			current = -1;
		}
	}
	std::set<uint32_t> shown;
	if (print_geometry) {
		for (const auto &[name, piece, need, dot] : beside_rows) {
			double got = dot.constant;
			for (const auto &[variable, coefficient] : dot.terms) got += coefficient * lp.Value(variable);
			std::cout << fmt::format("G1E BESIDE {} piece={} need={} got={}\n", name, piece, need, std::lround(got));
		}
		std::set<uint32_t> seeds;
		for (uint32_t t = 0; t < nt; t++) {
			const std::string &nm = map.tracks[t].name;
			if (show_names->contains(nm)) shown.insert(t);
			if (seed_names != nullptr && seed_names->contains(nm)) {
				shown.insert(t);
				seeds.insert(t);
			}
		}
		for (const uint32_t s : seeds) {
			for (uint32_t t = 0; t < nt; t++) {
				for (const PlanarEnd &a : {map.expanded.lines[s].from, map.expanded.lines[s].to}) {
					if (a.port) continue;
					for (const PlanarEnd &b : {map.expanded.lines[t].from, map.expanded.lines[t].to}) {
						if (!b.port && b.index == a.index) shown.insert(t);
					}
				}
			}
		}
		for (const uint32_t t : shown) {
			const SolvedTrack &st = solved[t];
			const std::string &nm = map.tracks[t].name;
			const size_t nv = st.vertices.size();
			std::vector<int> len(nv, 0);
			for (size_t i = 0; i + 1 < nv; i++) len[i] = std::max(std::abs(st.vertices[i + 1][0] - st.vertices[i][0]), std::abs(st.vertices[i + 1][1] - st.vertices[i][1]));
			for (size_t i = 0; i < nv; i++) {
				const int x = st.vertices[i][0], y = st.vertices[i][1];
				std::string extra;
				const std::string &kind = vertex_kind[t][i];
				if (kind == "start" || kind == "end") {
					const PlanarEnd &e = kind == "start" ? map.expanded.lines[t].from : map.expanded.lines[t].to;
					extra = fmt::format(" {}={}", e.port ? "port" : "node", e.index);
				} else if (kind == "event") {
					const uint32_t c = map.events[t][st.event_vertex.empty() ? 0 : static_cast<size_t>(std::ranges::find(st.event_vertex, i) - st.event_vertex.begin())];
					extra = fmt::format(" cross={}", map.tracks[map.crossings[c].a == t ? map.crossings[c].b : map.crossings[c].a].name);
				}
				const std::string next = i + 1 < nv ? fmt::format("next_heading={} next_len={}", st.headings[i], len[i]) : std::string("next_heading=- next_len=-");
				std::cout << fmt::format("G1E {} v{} kind={} x={} y={} x%2={} y%2={} (x+y)%2={} {}{}\n", nm, i, kind, x, y, x & 1, y & 1, (x + y) & 1, next, extra);
			}
		}
	}
	for (const PlanarCrossing &c : map.crossings) {
		(void)c;
	}
	for (uint32_t c = 0; c < map.crossings.size(); c++) {
		const uint32_t a = map.crossings[c].a, b = map.crossings[c].b;
		const auto at = [&](uint32_t t) {
			const size_t ev = static_cast<size_t>(std::ranges::find(map.events[t], c) - map.events[t].begin());
			return solved[t].vertices[solved[t].event_vertex[ev]];
		};
		if (at(a) != at(b)) r.consistency++;
	}
	for (size_t n = 0; n < map.expanded.nodes.size(); n++) {
		const PlanarNode &node = map.expanded.nodes[n];
		std::vector<std::array<int, 2>> ends;
		for (const uint32_t t : node.in) ends.push_back(solved[t].vertices.back());
		for (const uint32_t t : node.out) ends.push_back(solved[t].vertices.front());
		for (const auto &end : ends) if (end != ends.front()) { r.consistency++; break; }
	}

	/* The audit. */
	{
		std::vector<bool> node_split;
		for (size_t n = 0; n < map.expanded.nodes.size(); n++) node_split.push_back(is_split(n));
		std::vector<std::tuple<std::string, size_t, int, long>> beside_got;
		for (const auto &[name, piece, need, dot] : beside_rows) {
			double got = dot.constant;
			for (const auto &[variable, coefficient] : dot.terms) got += coefficient * lp.Value(variable);
			beside_got.emplace_back(name, piece, need, std::lround(got));
		}
		const std::function<int(uint32_t)> value_fn = value;
		r.audit = AuditG1({map, v, off, grid_tracks, solved, vertex_kind, elem_pos, te, spans, stub_recs, cusp_start, cusp_end, beside_got, node_split, value_fn, rules.node_leg, rules.cross});
		if (print_geometry) {
			/* The audit's grid checks on the shown tracks, with the grid rows' own bit back on. */
			for (const std::string &line : AuditG1({map, v, off & ~1024U, &shown, solved, vertex_kind, elem_pos, te, spans, stub_recs, cusp_start, cusp_end, beside_got, node_split, value_fn, rules.node_leg, rules.cross})) {
				if (line.find(" family=grid ") != std::string::npos) std::cout << fmt::format("G1E BREAK {}\n", line);
			}
		}
	}

	/* The geometry over the expanded topology. */
	PlanarGeometry g;
	g.width = r.width_tiles;
	g.height = r.height_tiles;
	g.chains.resize(nt);
	g.headings.resize(nt);
	for (uint32_t t = 0; t < nt; t++) {
		for (const auto &p : solved[t].vertices) {
			g.chains[t].push_back(static_cast<uint32_t>(g.points.size()));
			g.points.push_back(p);
		}
		for (const int h : solved[t].headings) g.headings[t].push_back(static_cast<uint8_t>(h));
	}
	g.node_vertex.resize(map.expanded.nodes.size());
	for (size_t n = 0; n < map.expanded.nodes.size(); n++) {
		const PlanarNode &node = map.expanded.nodes[n];
		g.node_vertex[n] = !node.in.empty() ? g.chains[node.in[0]].back() : g.chains[node.out[0]].front();
	}
	for (const Span &sp : spans) {
		const SolvedTrack &st = solved[sp.track];
		g.decks.push_back({sp.track, static_cast<uint32_t>(st.event_vertex[sp.first]), static_cast<uint32_t>(st.event_vertex[sp.last]), sp.clear, sp.clear});
	}

	/* The clash oracle. */
	const PlanarChannels channels = BuildPlanarChannels(topology);
	for (const PlanarClashHit &hit : PlanarGeometryClashes(map.expanded, g)) {
		const uint32_t a = hit.line_a, b = hit.line_b;
		const uint32_t sa = solved[a].segment_stretch[hit.segment_a], sb = solved[b].segment_stretch[hit.segment_b];
		const bool same_channel = channels.channel_of_line[map.tracks[a].line] == channels.channel_of_line[map.tracks[b].line];
		int face = -1;
		for (const PlanarFace &f : map.faces) {
			bool has_a = false, has_b = false;
			for (const PlanarWallRun &run : f.runs) {
				for (const auto &[line, stretch] : run.stretches) {
					if (line == a && stretch == sa) has_a = true;
					if (line == b && stretch == sb) has_b = true;
				}
			}
			if (has_a && has_b) {
				face = f.face;
				break;
			}
		}
		if (same_channel) r.same_channel++; else if (face >= 0) r.face++; else r.noface++;
		r.clashes.push_back(fmt::format("{} s{} h{} x {} s{} h{} tile=({},{}) rule={} face={} same_channel={}", map.tracks[a].name, sa, g.headings[a][hit.segment_a], map.tracks[b].name, sb, g.headings[b][hit.segment_b], hit.tile.x, hit.tile.y, hit.rule,
				face >= 0 ? fmt::format("{}", face) : std::string("none"), same_channel ? 1 : 0));
	}

	/* The curve oracle. */
	for (const PlanarRoute &route : map.expanded.routes) {
		for (size_t pi = 0; pi < route.paths.size(); pi++) {
			std::vector<std::pair<int, int>> runs;
			for (const uint32_t t : route.paths[pi]) {
				for (const auto &[heading, length] : solved[t].runs) {
					if (!runs.empty() && runs.back().first == heading) {
						runs.back().second += length;
					} else {
						runs.emplace_back(heading, length);
					}
				}
			}
			std::vector<JunctionCurveHand> hands;
			std::vector<int> pos;
			int at = 0;
			for (size_t i = 0; i < runs.size(); i++) {
				if (i > 0) {
					const int turn = PlanarModel::Rot(runs[i - 1].first, runs[i].first);
					hands.push_back(turn > 0 ? JunctionCurveHand::Right : JunctionCurveHand::Left);
					pos.push_back(at);
				}
				at += runs[i].second;
			}
			for (const JunctionCurveGap &gap : JunctionCurveGaps(hands, spec.tl, spec.curve_speed, spec.reference_speed)) {
				if (pos[gap.j] - pos[gap.i] >= gap.min_gap) continue;
				r.curve_breaches.push_back(fmt::format("route={} path={} curves {},{} gap {} below {}", route.name, pi, gap.i, gap.j, pos[gap.j] - pos[gap.i], gap.min_gap));
			}
		}
	}
	return r;
}

} // namespace

TEST_CASE("planar probe: G1 joint slots and lengths on T2 T3 LL2", "[junction][planar][planarshape][.probe]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const std::vector<std::pair<std::string, std::string>> hubs = {{"T2", PlanarTText(2)}, {"T3", PlanarTText(3)}, {"LL2", PlanarLL2Text()}};
	for (const auto &[name, text] : hubs) {
		std::string error;
		const PlanarTopology topology = ParsePlanarTopology(text, &error);
		REQUIRE(error.empty());
		const PlanarShapeResult shape = BuildPlanarShape(BuildPlanarStructure(topology, spec, 16));
		REQUIRE(shape.variants.size() >= 3);
		size_t solved = 0, clashes_total = 0, curve_total = 0;
		double ms_max = 0;
		for (size_t i = 0; i < 3; i++) {
			const PlanarShapeVariant &v = shape.variants[i];
			const std::string &tag = v.structure.tag;
			const G1Result r = SolveG1(topology, spec, v);
			if (r.solved) solved++;
			ms_max = std::max(ms_max, r.ms);
			clashes_total += r.clashes.size();
			curve_total += r.curve_breaches.size();
			std::cout << fmt::format("G1 {} {} solved={} ms={:.1f} steps={} W={} H={} clashes={} same_channel={} face={} noface={} curve={} consistency={} no_chain={} why={}\n", name, tag, r.solved ? 1 : 0, r.ms, r.step_lanes, r.width_tiles, r.height_tiles,
					r.clashes.size(), r.same_channel, r.face, r.noface, r.curve_breaches.size(), r.consistency, r.no_chain, r.why);
			for (const std::string &line : r.clashes) std::cout << fmt::format("G1C {} {} {}\n", name, tag, line);
			for (const std::string &line : r.curve_breaches) std::cout << fmt::format("G1V {} {} {}\n", name, tag, line);
			for (const std::string &line : r.audit) std::cout << fmt::format("G1G AUDIT {} {} {}\n", name, tag, line);
			const auto print_a = [&](uint32_t off, const G1Result &a) {
				for (const std::string &line : a.audit) std::cout << fmt::format("G1G AUDIT {} {} {}\n", name, tag, line);
				std::cout << fmt::format("G1A {} {} off={} solved={} ms={:.1f} clashes={} same_channel={} face={} noface={} curve={} why={}\n", name, tag, off, a.solved ? 1 : 0, a.ms, a.clashes.size(), a.same_channel, a.face, a.noface, a.curve_breaches.size(), a.why.substr(0, 300));
				size_t shown[3] = {0, 0, 0};
				for (const std::string &line : a.clashes) {
					const size_t kind = line.ends_with("same_channel=1") ? 0 : (line.find("face=none") != std::string::npos ? 2 : 1);
					if (shown[kind]++ < 3) std::cout << fmt::format("G1C {} {} off={} {}\n", name, tag, off, line);
				}
			};
			if (name == "LL2") {
				const uint32_t base = (1U << G1_FAMILIES) - 1;
				for (const uint32_t extra : {0U, 512U, 1024U, 2048U, 4096U, 1024U | 2048U | 4096U, 512U | 1024U | 2048U | 4096U}) print_a(base | extra, SolveG1(topology, spec, v, base | extra));
			}
			if (name == "T2" && i == 0) {
				for (const uint32_t off : {1U, 2U, (1U << G1_FAMILIES) - 1}) print_a(off, SolveG1(topology, spec, v, off));
			}
			if (r.solved) continue;
			std::vector<uint32_t> offs;
			for (uint32_t b = 0; b < G1_FAMILIES; b++) offs.push_back(1U << b);
			offs.push_back((1U << G1_FAMILIES) - 1);
			for (const uint32_t off : offs) {
				print_a(off, SolveG1(topology, spec, v, off));
			}
		}
		std::cout << fmt::format("G1 {} SUMMARY solved={}/3 ms_max={:.1f} clashes_total={} curve_total={}\n", name, solved, ms_max, clashes_total, curve_total);
	}
}

/** G1h: the deletion filter over the grid rows per track of one variant at off=511, then one solve at off=511|1024 that prints the set's geometry and every grid break on it. */
static void G1hHub(const char *hub, const PlanarTopology &topology, const JunctionSpec &spec, const PlanarShapeVariant &v)
{
	const std::string &tag = v.structure.tag;
	const PlanarTrackMap &map = v.structure.map;
	const uint32_t off = 511;
	const size_t nt = map.tracks.size();
	std::set<uint32_t> all;
	for (uint32_t t = 0; t < nt; t++) all.insert(t);
	if (SolveG1(topology, spec, v, off, &all).solved) {
		std::cout << fmt::format("G1D {} {} full set feasible\n", hub, tag);
		return;
	}
	std::set<uint32_t> keep = all;
	/* Each filter solve has 1 s: a track leaves the set only when its removal is proven infeasible within that. */
	g1_time_limit = 1.0;
	g1_timeouts = 0;
	for (uint32_t t = 0; t < nt; t++) {
		std::set<uint32_t> without = keep;
		without.erase(t);
		if (SolveG1(topology, spec, v, off, &without).why.starts_with("Infeasible")) keep = without;
	}
	g1_time_limit = 15.0;
	std::cout << fmt::format("G1D {} {} filter_timeouts={}\n", hub, tag, g1_timeouts);
	std::string names;
	for (const uint32_t t : keep) names += " " + map.tracks[t].name;
	std::cout << fmt::format("G1D {} {} minimal={} tracks={}\n", hub, tag, keep.size(), names);
	const auto end_text = [&](const PlanarEnd &e) {
		return e.port ? fmt::format("port {}", static_cast<int>(map.expanded.ports[e.index].side)) : fmt::format("node {}", e.index);
	};
	for (const uint32_t t : keep) {
		std::string pieces;
		for (const int p : v.tracks[t].pieces) pieces += fmt::format("{}", ((p % 8) + 8) % 8);
		const std::string part = map.part_of[t] == UINT32_MAX ? std::string("copy/row") : fmt::format("{}", topology.lines[map.part_of[t]].name);
		const PlanarLine &line = map.expanded.lines[t];
		std::cout << fmt::format("G1D TRACK {} part_of={} pieces={} events={} from={} to={}\n", map.tracks[t].name, part, pieces, map.events[t].size(), end_text(line.from), end_text(line.to));
	}
	/* The set plus every track sharing a node or a crossing with it. */
	std::set<uint32_t> shown = keep;
	for (const uint32_t s : keep) {
		for (uint32_t t = 0; t < nt; t++) {
			for (const PlanarEnd &a : {map.expanded.lines[s].from, map.expanded.lines[s].to}) {
				if (a.port) continue;
				for (const PlanarEnd &b : {map.expanded.lines[t].from, map.expanded.lines[t].to}) {
					if (!b.port && b.index == a.index) shown.insert(t);
				}
			}
		}
		for (const PlanarCrossing &c : map.crossings) {
			if (c.a == s) shown.insert(c.b);
			if (c.b == s) shown.insert(c.a);
		}
	}
	std::set<std::string> show;
	for (const uint32_t t : shown) show.insert(map.tracks[t].name);
	std::cout << fmt::format("G1E BLOCK {} {} off=1535\n", hub, tag);
	SolveG1(topology, spec, v, off | 1024U, nullptr, nullptr, &show);
}

TEST_CASE("planar probe: G1d the grid rows that block LL2", "[junction][planar][planarshape][.probe]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	std::string error;
	const PlanarTopology ll2 = ParsePlanarTopology(PlanarLL2Text(), &error);
	REQUIRE(error.empty());
	const PlanarShapeResult shape = BuildPlanarShape(BuildPlanarStructure(ll2, spec, 16));
	REQUIRE(!shape.variants.empty());
	G1hHub("LL2", ll2, spec, shape.variants[0]);
	const PlanarTopology t3 = ParsePlanarTopology(PlanarTText(3), &error);
	REQUIRE(error.empty());
	const PlanarShapeResult shape3 = BuildPlanarShape(BuildPlanarStructure(t3, spec, 16));
	REQUIRE(shape3.variants.size() >= 3);
	for (size_t i = 0; i < 3; i++) {
		const PlanarShapeVariant &v = shape3.variants[i];
		std::set<uint32_t> all;
		for (uint32_t t = 0; t < v.structure.map.tracks.size(); t++) all.insert(t);
		if (!SolveG1(t3, spec, v, 511U, &all).solved) G1hHub("T3", t3, spec, v);
	}
}

TEST_CASE("planar probe: G1e the geometry around LL2's grid conflict", "[junction][planar][planarshape][.probe]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	std::string error;
	const PlanarTopology topology = ParsePlanarTopology(PlanarLL2Text(), &error);
	REQUIRE(error.empty());
	const PlanarShapeResult shape = BuildPlanarShape(BuildPlanarStructure(topology, spec, 16));
	REQUIRE(!shape.variants.empty());
	const std::set<std::string> show = {"EO1", "EO2", "AB1", "AB2"}, seed = {"A2a", "B2\"13"};
	std::cout << "G1E BLOCK LL2 off=1536\n";
	const G1Result r = SolveG1(topology, spec, shape.variants[0], 511U | 1024U, nullptr, nullptr, &show, &seed);
	CHECK(r.solved);
	for (const std::string &line : r.audit) std::cout << fmt::format("G1G AUDIT LL2 {} {}\n", shape.variants[0].structure.tag, line);
	const PlanarTopology t2 = ParsePlanarTopology(PlanarTText(2), &error);
	REQUIRE(error.empty());
	const PlanarShapeResult shape2 = BuildPlanarShape(BuildPlanarStructure(t2, spec, 16));
	REQUIRE(!shape2.variants.empty());
	const std::set<std::string> show2 = {"B1", "prio:B1", "C2", "E2", "prio:E2", "D1"}, seed2 = {"prio:B1", "prio:E2"};
	std::cout << "G1E BLOCK T2 off=1\n";
	const G1Result r2 = SolveG1(t2, spec, shape2.variants[0], 1U, nullptr, nullptr, &show2, &seed2);
	CHECK(r2.solved);
	for (const std::string &line : r2.audit) std::cout << fmt::format("G1G AUDIT T2 {} {}\n", shape2.variants[0].structure.tag, line);
}

TEST_CASE("planar probe: G1i the grid as a mod-2 system", "[junction][planar][planarshape][.probe]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const std::vector<std::pair<std::string, std::string>> hubs = {{"T2", PlanarTText(2)}, {"T3", PlanarTText(3)}, {"LL2", PlanarLL2Text()}};
	g1_mod2 = true;
	for (const auto &[name, text] : hubs) {
		std::string error;
		const PlanarTopology topology = ParsePlanarTopology(text, &error);
		REQUIRE(error.empty());
		const PlanarShapeResult shape = BuildPlanarShape(BuildPlanarStructure(topology, spec, 16));
		REQUIRE(shape.variants.size() >= 3);
		for (size_t i = 0; i < 3; i++) {
			const PlanarShapeVariant &v = shape.variants[i];
			const G1Result r = SolveG1(topology, spec, v, 511U);
			std::cout << fmt::format("G1I {} {} off=511 ms={:.1f}\n", name, v.structure.tag, r.ms);
			for (const std::string &line : r.mod2) std::cout << fmt::format("G1I {} {} {}\n", name, v.structure.tag, line);
			if (name == "LL2" && i == 0) {
				/* The control: stubs off (8192) brings back G1e's node 18 conflict (D380), which the check must name. */
				const G1Result c = SolveG1(topology, spec, v, 511U | 8192U);
				for (const std::string &line : c.mod2) std::cout << fmt::format("G1I CONTROL {} {} off=8703 {}\n", name, v.structure.tag, line);
			}
		}
	}
	g1_mod2 = false;
}

/** G1j: the deletion filter over row kinds at off=511 (5 s per solve; a timeout keeps the kind), on LL2 o0t0f1m1 and T3's failing variant. */
TEST_CASE("planar probe: G1j the row kinds that make the base infeasible", "[junction][planar][planarshape][.probe]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const std::vector<std::pair<std::string, std::string>> hubs = {{"LL2", PlanarLL2Text()}, {"T3", PlanarTText(3)}};
	const double limit = g1_time_limit;
	g1_time_limit = 5.0;
	for (const auto &[name, text] : hubs) {
		std::string error;
		const PlanarTopology topology = ParsePlanarTopology(text, &error);
		REQUIRE(error.empty());
		const PlanarShapeResult shape = BuildPlanarShape(BuildPlanarStructure(topology, spec, 16));
		REQUIRE(shape.variants.size() >= 3);
		for (size_t i = 0; i < 3; i++) {
			const PlanarShapeVariant &v = shape.variants[i];
			const std::string &vt = v.structure.tag;
			if (vt != "o0t0f1m1" && vt != "o7t2f0m3270") continue;
			/* The kinds present: a mod-2 run records every row. */
			g1_mod2 = true;
			std::set<std::string> kinds;
			{
				const G1Result probe = SolveG1(topology, spec, v, 511U);
				(void)probe;
			}
			g1_mod2 = false;
			kinds = g1_kinds_seen;
			const G1Result base = SolveG1(topology, spec, v, 511U);
			std::cout << fmt::format("G1J {} {} base solved={} ms={:.0f} kinds={}\n", name, vt, base.solved ? 1 : 0, base.ms, kinds.size());
			g1_skip_kinds.clear();
			for (const std::string &k : kinds) {
				g1_skip_kinds.insert(k);
				const size_t timeouts = g1_timeouts;
				const G1Result r = SolveG1(topology, spec, v, 511U);
				const bool timed_out = g1_timeouts != timeouts;
				const bool still = !r.solved && !timed_out;
				if (!still) g1_skip_kinds.erase(k);
				std::cout << fmt::format("G1J {} {} drop [{}] -> {} ms={:.0f}\n", name, vt, k, r.solved ? "solved (needed)" : (timed_out ? "timeout (kept)" : "infeasible (dropped)"), r.ms);
			}
			std::string needed;
			for (const std::string &k : kinds) {
				if (!g1_skip_kinds.contains(k)) needed += " [" + k + "]";
			}
			std::cout << fmt::format("G1J {} {} KEPT{}\n", name, vt, needed);
			g1_skip_kinds.clear();
		}
	}
	g1_time_limit = limit;
}

/** G1k: parity-implied bounds (from G1i's forced parities) on the LP relaxation at off=511; an infeasible LP names the conflict exactly. */
TEST_CASE("planar probe: G1k parity-tightened LP names the conflict", "[junction][planar][planarshape][.probe]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const std::vector<std::pair<std::string, std::string>> hubs = {{"T2", PlanarTText(2)}, {"T3", PlanarTText(3)}, {"LL2", PlanarLL2Text()}};
	for (const auto &[name, text] : hubs) {
		std::string error;
		const PlanarTopology topology = ParsePlanarTopology(text, &error);
		REQUIRE(error.empty());
		const PlanarShapeResult shape = BuildPlanarShape(BuildPlanarStructure(topology, spec, 16));
		REQUIRE(shape.variants.size() >= 3);
		for (size_t i = 0; i < 3; i++) {
			const PlanarShapeVariant &v = shape.variants[i];
			const std::string &vt = v.structure.tag;
			g1_mod2 = true;
			const G1Result m = SolveG1(topology, spec, v, 511U);
			g1_mod2 = false;
			const G1Result plain = SolveG1(topology, spec, v, 511U | 512U);
			g1_tighten = m.forced;
			g1_zero = m.forbidden;
			const G1Result r = SolveG1(topology, spec, v, 511U | 512U);
			g1_tighten.clear();
			g1_zero.clear();
			for (const std::string &line : m.mod2) if (line.starts_with("step places")) std::cout << fmt::format("G1K {} {} {}\n", name, vt, line);
			for (const std::string &line : m.forbidden_text) std::cout << fmt::format("G1K FORBID {} {} {}\n", name, vt, line.substr(0, 600));
			std::cout << fmt::format("G1K {} {} forced={} lp_plain={} lp_tight={} ms={:.0f}\n", name, vt, m.forced.size(), plain.solved ? 1 : 0, r.solved ? 1 : 0, r.ms);
			for (const std::string &line : r.iis) std::cout << fmt::format("G1K {} {}   {}\n", name, vt, line);
		}
	}
}
