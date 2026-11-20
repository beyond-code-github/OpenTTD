/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/**
 * @file planar_draw.cpp The drawing program: points and pieces, box, ports, nodes, crossings and spans (E5-1).
 * Not in it yet: grid parity (D382), beside tracks as offset curves, node legs and the trident binary, bays, coverage, curve spacing, cells, dummies, clearance.
 */

#include "../../stdafx.h"
#include "planar_draw.h"

#include "../../core/format.hpp"
#include "../placement/junction_milp.h"
#include "planar_model.h"

#include <algorithm>
#include <chrono>
#include <set>
#include <utility>

#include "../../safeguards.h"

namespace {

using Terms = std::vector<std::pair<uint32_t, double>>;

constexpr double COORD_MAX = 4096; ///< The largest coordinate, length and parameter, half tiles.
constexpr std::array<const char *, 4> SIDE_NAME = {"N", "E", "S", "W"};

/** The least length of an axis piece, half tiles; a diagonal piece's is 1. */
constexpr double AXIS_LEAST = 2;

/** One piece's length as coefficient times variable: 2 k for an axis piece, a plain count for a diagonal one. */
struct Length {
	uint32_t variable = 0;
	double coefficient = 1;
};

} // namespace

PlanarDrawResult CheckPlanarDrawable(const PlanarShapeVariant &v, double seconds)
{
	const auto start = std::chrono::steady_clock::now();
	PlanarDrawResult result;
	const PlanarTrackMap &map = v.structure.map;
	const PlanarRuleFields &rules = v.structure.rules;
	const size_t nt = map.tracks.size();
	const auto unsupported = [&](const std::string &why) {
		result.status = "unsupported: " + why;
		return result;
	};
	if (v.tracks.size() != nt || map.events.size() != nt || map.expanded.lines.size() != nt) return unsupported("the shape's tracks, the map's events and its lines disagree");
	if (v.structures.size() != v.structure.structures.size()) return unsupported("the variant's structures and their derived values disagree");

	JunctionMilp milp;
	std::vector<std::string> tags;
	uint32_t columns = 0;
	const auto var = [&](double lower, double upper, double cost) {
		columns++;
		return milp.AddVariable(lower, upper, cost, true);
	};
	const auto row = [&](Terms terms, double lower, double upper, const std::string &tag) {
		std::ranges::sort(terms);
		Terms merged;
		for (const auto &[variable, coefficient] : terms) {
			if (!merged.empty() && merged.back().first == variable) {
				merged.back().second += coefficient;
			} else {
				merged.emplace_back(variable, coefficient);
			}
		}
		std::erase_if(merged, [](const std::pair<uint32_t, double> &t) { return t.second == 0; });
		milp.AddRow(merged, lower, upper);
		tags.push_back(tag);
	};
	const auto eq = [&](Terms terms, double rhs, const std::string &tag) { row(std::move(terms), rhs, rhs, tag); };

	const uint32_t width = var(1, COORD_MAX, 0);
	const uint32_t height = var(1, COORD_MAX, 0);

	/* Points: per track its start and the end of each piece. */
	std::vector<std::vector<std::array<uint32_t, 2>>> point(nt);
	std::vector<std::vector<Length>> length(nt);
	for (size_t t = 0; t < nt; t++) {
		const std::vector<int> &pieces = v.tracks[t].pieces;
		if (pieces.empty()) return unsupported("track " + map.tracks[t].name + " has no piece");
		for (size_t i = 0; i <= pieces.size(); i++) {
			point[t].push_back({var(0, COORD_MAX, 0), var(0, COORD_MAX, 0)});
			row({{point[t][i][0], 1}, {width, -1}}, -JunctionMilp::INF, 0, "box");
			row({{point[t][i][1], 1}, {height, -1}}, -JunctionMilp::INF, 0, "box");
		}

		/* Flex groups: the h pieces around consecutive flex middles may be 0 each, together keeping the original piece's least length. */
		std::vector<bool> free_piece(pieces.size(), false);
		std::vector<std::vector<uint32_t>> groups;
		const std::vector<uint32_t> &flex = v.tracks[t].flex;
		for (size_t f = 0; f < flex.size(); f++) {
			const uint32_t m = flex[f];
			if (m == 0 || m + 1 >= pieces.size()) return unsupported("a flex middle of track " + map.tracks[t].name + " is out of range");
			if (f == 0 || flex[f - 1] + 2 != m) groups.push_back({m - 1});
			groups.back().push_back(m);
			groups.back().push_back(m + 1);
		}
		for (const std::vector<uint32_t> &g : groups) {
			for (const uint32_t i : g) free_piece[i] = true;
		}
		std::set<uint32_t> middles(flex.begin(), flex.end());
		for (size_t i = 0; i < pieces.size(); i++) {
			const bool axis = (pieces[i] % 2) == 0;
			const double lower = free_piece[i] ? 0 : 1;
			length[t].push_back({var(lower, COORD_MAX, 1), axis ? AXIS_LEAST : 1});
		}
		for (const std::vector<uint32_t> &g : groups) {
			Terms sum;
			for (const uint32_t i : g) {
				if (!middles.contains(i)) sum.emplace_back(length[t][i].variable, length[t][i].coefficient);
			}
			const double least = (pieces[g.front()] % 2) == 0 ? AXIS_LEAST : 1;
			row(sum, least, JunctionMilp::INF, fmt::format("flex {} {}", map.tracks[t].name, g.front()));
		}
		for (size_t i = 0; i < pieces.size(); i++) {
			const std::array<int, 2> step = PlanarModel::STEP[((pieces[i] % 8) + 8) % 8];
			const std::string tag = fmt::format("piece {} {}", map.tracks[t].name, i);
			for (int axis = 0; axis < 2; axis++) {
				eq({{point[t][i + 1][axis], 1}, {point[t][i][axis], -1}, {length[t][i].variable, -step[axis] * length[t][i].coefficient}}, 0, tag);
			}
		}
	}

	/* The box edge of a side and the order of the port lines along it. */
	for (uint32_t side = 0; side < 4; side++) {
		const std::string tag = fmt::format("port {}", SIDE_NAME[side]);
		const bool along_x = side == static_cast<uint32_t>(PlanarSide::N) || side == static_cast<uint32_t>(PlanarSide::S);
		const int along = along_x ? 0 : 1;
		bool first = true;
		uint32_t previous = 0;
		for (uint32_t p = 0; p < map.expanded.ports.size(); p++) {
			const PlanarPort &port = map.expanded.ports[p];
			if (static_cast<uint32_t>(port.side) != side) continue;
			/* Lines enter in reading order when right of travel points along it, else reversed. */
			const int right = PlanarModel::STEP[(static_cast<int>(port.heading) + 2) % 8][along];
			std::vector<uint32_t> lines = port.lines;
			if (right < 0) std::ranges::reverse(lines);
			for (size_t k = 0; k < lines.size(); k++) {
				const uint32_t line = lines[k];
				const PlanarEnd &from = map.expanded.lines[line].from;
				const PlanarEnd &to = map.expanded.lines[line].to;
				uint32_t at;
				if (from.port && from.index == p) {
					at = point[line].front()[along];
				} else if (to.port && to.index == p) {
					at = point[line].back()[along];
				} else {
					return unsupported("line " + map.tracks[line].name + " does not end at its port");
				}
				const uint32_t edge_at = along_x ? 1 : 0;
				const uint32_t edge_var = (from.port && from.index == p) ? point[line].front()[edge_at] : point[line].back()[edge_at];
				switch (static_cast<PlanarSide>(side)) {
					case PlanarSide::N:
					case PlanarSide::W:
						eq({{edge_var, 1}}, 0, tag);
						break;
					case PlanarSide::S:
						eq({{edge_var, 1}, {height, -1}}, 0, tag);
						break;
					case PlanarSide::E:
						eq({{edge_var, 1}, {width, -1}}, 0, tag);
						break;
				}
				if (!first) {
					const int gap = k == 0 ? 2 * v.structure.side_gap[side] : 2;
					eq({{at, 1}, {previous, -1}}, gap, tag);
				}
				first = false;
				previous = at;
			}
		}
	}

	/* Nodes: every line's end at a node is the node's one point. */
	for (size_t n = 0; n < map.expanded.nodes.size(); n++) {
		const PlanarNode &node = map.expanded.nodes[n];
		std::vector<std::array<uint32_t, 2>> ends;
		for (const uint32_t line : node.in) ends.push_back(point[line].back());
		for (const uint32_t line : node.out) ends.push_back(point[line].front());
		for (size_t k = 1; k < ends.size(); k++) {
			for (int axis = 0; axis < 2; axis++) eq({{ends[k][axis], 1}, {ends[0][axis], -1}}, 0, fmt::format("node {}", n));
		}
	}

	/* Crossings: per crossing the parameter along each of its two pieces. */
	const size_t nc = map.crossings.size();
	std::vector<std::array<uint32_t, 2>> param(nc);
	std::vector<std::array<uint32_t, 2>> piece_of(nc);
	for (size_t c = 0; c < nc; c++) {
		const std::array<uint32_t, 2> tracks = {map.crossings[c].a, map.crossings[c].b};
		for (int side = 0; side < 2; side++) {
			const uint32_t t = tracks[side];
			const auto at = std::ranges::find(map.events[t], static_cast<uint32_t>(c));
			if (at == map.events[t].end() || v.tracks[t].event_piece.size() != map.events[t].size()) return unsupported(fmt::format("crossing {} is not an event of track {}", c, map.tracks[t].name));
			const uint32_t piece = v.tracks[t].event_piece[at - map.events[t].begin()];
			if (piece >= v.tracks[t].pieces.size()) return unsupported(fmt::format("crossing {} sits on piece {} of track {}, which has {}", c, piece, map.tracks[t].name, v.tracks[t].pieces.size()));
			piece_of[c][side] = piece;
			param[c][side] = var(0, COORD_MAX, 0);
			const Length &len = length[t][piece];
			const std::string tag = fmt::format("cross {}", c);
			row({{param[c][side], 1}, {len.variable, -len.coefficient}}, -JunctionMilp::INF, -rules.cross, tag);
			row({{param[c][side], 1}}, rules.cross, JunctionMilp::INF, tag);
		}
		for (int axis = 0; axis < 2; axis++) {
			Terms terms;
			for (int side = 0; side < 2; side++) {
				const uint32_t t = tracks[side];
				const std::array<int, 2> step = PlanarModel::STEP[((v.tracks[t].pieces[piece_of[c][side]] % 8) + 8) % 8];
				const double sign = side == 0 ? 1 : -1;
				terms.emplace_back(point[t][piece_of[c][side]][axis], sign);
				terms.emplace_back(param[c][side], sign * step[axis]);
			}
			eq(terms, 0, fmt::format("cross {}", c));
		}
	}
	/* Crossings on one piece of one track keep their event order. */
	for (size_t t = 0; t < nt; t++) {
		for (size_t k = 0; k + 1 < map.events[t].size(); k++) {
			const uint32_t c0 = map.events[t][k];
			const uint32_t c1 = map.events[t][k + 1];
			const int s0 = map.crossings[c0].a == t ? 0 : 1;
			const int s1 = map.crossings[c1].a == t ? 0 : 1;
			if (piece_of[c0][s0] != piece_of[c1][s1]) continue;
			row({{param[c1][s1], 1}, {param[c0][s0], -1}}, 0, JunctionMilp::INF, fmt::format("order {} {}", map.tracks[t].name, piece_of[c0][s0]));
		}
	}

	/* Spans: the separator's crossings, their gaps, end runs and reach. */
	for (size_t s = 0; s < v.structure.structures.size(); s++) {
		const PlanarStructure &structure = v.structure.structures[s];
		const PlanarShapeStructure &shape = v.structures[s];
		if (structure.crossings.empty()) continue;
		std::vector<uint32_t> u;
		uint32_t piece = UINT32_MAX;
		for (const uint32_t c : structure.crossings) {
			if (c >= nc) return unsupported(fmt::format("structure {} names crossing {}", s, c));
			const int side = map.crossings[c].a == structure.separator ? 0 : 1;
			if (piece != UINT32_MAX && piece != piece_of[c][side]) return unsupported(fmt::format("structure {}'s crossings sit on different pieces of its separator", s));
			piece = piece_of[c][side];
			u.push_back(param[c][side]);
		}
		int least_run = -1;
		int most_limit = -1;
		for (int kind = 0; kind < 2; kind++) {
			if (structure.limit[kind] < 0) continue;
			least_run = least_run < 0 ? shape.end_run[kind] : std::min(least_run, shape.end_run[kind]);
			most_limit = std::max(most_limit, structure.limit[kind]);
		}
		for (size_t k = 0; k + 1 < u.size(); k++) row({{u[k + 1], 1}, {u[k], -1}}, rules.deck_gap, JunctionMilp::INF, fmt::format("deck {}", s));
		const Length &len = length[structure.separator][piece];
		if (least_run > 0) {
			row({{u.front(), 1}}, 2 * least_run, JunctionMilp::INF, fmt::format("end {}", s));
			row({{len.variable, len.coefficient}, {u.back(), -1}}, 2 * least_run, JunctionMilp::INF, fmt::format("end {}", s));
		}
		if (most_limit >= 0) row({{u.back(), 1}, {u.front(), -1}}, -JunctionMilp::INF, 2 * (most_limit - 1), fmt::format("span {}", s));
	}

	result.rows = static_cast<uint32_t>(tags.size());
	result.columns = columns;
	milp.SetTimeLimit(seconds);
	const JunctionMilp::Status status = milp.Solve();
	if (status == JunctionMilp::Status::Optimal) {
		result.feasible = true;
		result.status = "Optimal";
		result.points.resize(nt);
		for (size_t t = 0; t < nt; t++) {
			for (const std::array<uint32_t, 2> &p : point[t]) result.points[t].push_back({static_cast<int>(std::lround(milp.Value(p[0]))), static_cast<int>(std::lround(milp.Value(p[1])))});
		}
	} else if (status == JunctionMilp::Status::Infeasible) {
		result.status = "Infeasible";
		for (const uint32_t r : milp.InfeasibleRows()) {
			if (r < tags.size() && std::ranges::find(result.conflict, tags[r]) == result.conflict.end()) result.conflict.push_back(tags[r]);
		}
	} else {
		result.status = "Error";
	}
	result.ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
	return result;
}
