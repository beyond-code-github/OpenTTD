/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/**
 * @file planar_draw.cpp The drawing program: points and pieces, box, ports, nodes, crossings, spans, dummies with their landings and clearance per convex cell (E5-1, E5-2).
 * Not in it yet: grid parity (D382), beside tracks as offset curves, node legs and the trident binary, bays, coverage, curve spacing.
 */

#include "../../stdafx.h"
#include "planar_draw.h"

#include "../../core/format.hpp"
#include "../placement/junction_milp.h"
#include "planar_model.h"

#include <algorithm>
#include <charconv>
#include <climits>
#include <chrono>
#include <cmath>
#include <functional>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include "../../safeguards.h"

std::vector<PlanarClearRow> PlanarCellClearRows(const std::vector<PlanarCellWall> &cell, const std::vector<bool> &insert, bool pocket, std::vector<PlanarAlongRow> *along)
{
	const size_t n = cell.size();
	const auto no_track = [&](size_t i) { return cell[i].track == PLANAR_DUMMY_TRACK || cell[i].track == UINT32_MAX; };
	std::vector<size_t> side(n);
	for (size_t i = 0; i < n; i++) side[i] = i;
	for (size_t i = 0; i < n; i++) {
		const size_t j = (i + 1) % n;
		if (cell[i].heading != cell[j].heading && !insert[i] && !insert[j]) continue;
		const size_t from = side[j], to = side[i];
		for (size_t m = 0; m < n; m++) {
			if (side[m] == from) side[m] = to;
		}
	}
	std::vector<PlanarClearRow> rows;
	for (size_t k = 0; k < n; k++) {
		const size_t next = (k + 1) % n;
		if (no_track(k) && no_track(next)) continue;
		for (size_t e = 0; e < n; e++) {
			if (e == k || e == next || insert[e] || cell[e].track == UINT32_MAX) continue;
			if (side[e] == side[k] || side[e] == side[next]) continue;
			const int h = cell[e].heading;
			int spacing = 0;
			uint32_t room = UINT32_MAX;
			if (cell[e].track != PLANAR_DUMMY_TRACK) {
				if (insert[k] != insert[next]) {
					/* A step of length 0 imposes no spacing (SPEC-shape section 8.10): the insert's heading counts only through the room row. */
					const size_t flex = insert[k] ? k : next, other = insert[k] ? next : k;
					spacing = PlanarModel::RungSpacing(h, cell[other].heading);
					if (PlanarModel::RungSpacing(h, cell[flex].heading) > spacing) room = static_cast<uint32_t>(flex);
				} else {
					spacing = std::max(PlanarModel::RungSpacing(h, cell[k].heading), PlanarModel::RungSpacing(h, cell[next].heading));
				}
			}
			rows.push_back({static_cast<uint32_t>(k), static_cast<uint32_t>(e), spacing, room});
		}
	}
	if (pocket) {
		/* The pocket rule (SPEC-shape section 8.9). */
		const std::set<size_t> distinct(side.begin(), side.end());
		const size_t sides = distinct.size();
		std::set<std::pair<size_t, size_t>> dropped;
		const auto all_track = [&](size_t s) {
			for (size_t i = 0; i < n; i++) {
				if (side[i] == s && no_track(i)) return false;
			}
			return true;
		};
		const auto has_wall = [&](size_t k, size_t s) { return side[k] == s || side[(k + 1) % n] == s; };
		const auto both_walls = [&](size_t k, size_t s) { return side[k] == s && side[(k + 1) % n] == s; };
		for (const size_t s : distinct) {
			if (sides < 3) break;
			/* The side's walls in walk order, from its wall whose predecessor is in another side. */
			size_t begin = n;
			for (size_t i = 0; i < n; i++) {
				if (side[i] == s && side[(i + n - 1) % n] != s) begin = i;
			}
			if (begin == n) continue;
			std::vector<size_t> walls;
			for (size_t i = begin; side[i % n] == s && walls.size() < n; i++) walls.push_back(i % n);
			size_t first_insert = n, last_insert = n, first_plain = n;
			size_t last_position = 0, first_position = 0;
			for (size_t i = 0; i < walls.size(); i++) {
				if (insert[walls[i]]) {
					if (first_insert == n) {
						first_insert = walls[i];
						first_position = i;
					}
					last_insert = walls[i];
					last_position = i;
				} else if (first_plain == n) {
					first_plain = walls[i];
				}
			}
			if (first_insert == n || first_plain == n) continue;
			const int h = cell[first_plain].heading;
			const int m = h % 2 == 0 ? 0 : 2;
			const size_t last_wall = walls.back(), first_wall = walls.front();
			const size_t r = side[(last_wall + 1) % n], p = side[(first_wall + n - 1) % n];
			if (all_track(r)) {
				for (size_t k = 0; k < n; k++) {
					if (!has_wall(k, r) || k == last_wall || (no_track(k) && no_track((k + 1) % n))) continue;
					if (along != nullptr) along->push_back({static_cast<uint32_t>(k), static_cast<uint32_t>(last_insert), h, m});
					if (both_walls(k, r)) {
						for (size_t i = 0; i < last_position; i++) dropped.emplace(k, walls[i]);
					}
				}
			}
			if (all_track(p)) {
				const size_t corner = (first_wall + n - 1) % n;
				for (size_t k = 0; k < n; k++) {
					if (!has_wall(k, p) || k == corner || (no_track(k) && no_track((k + 1) % n))) continue;
					if (along != nullptr) along->push_back({static_cast<uint32_t>(k), static_cast<uint32_t>((first_insert + n - 1) % n), (h + 4) % 8, m});
					if (both_walls(k, p)) {
						for (size_t i = first_position + 1; i < walls.size(); i++) dropped.emplace(k, walls[i]);
					}
				}
			}
		}
		std::erase_if(rows, [&](const PlanarClearRow &row) { return dropped.contains({row.vertex, row.wall}); });
	}
	return rows;
}

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

PlanarDrawPins ParsePlanarDrawPins(const PlanarShapeVariant &v, std::string_view text)
{
	PlanarDrawPins pins;
	const PlanarTrackMap &map = v.structure.map;
	const auto track_of = [&](const std::string &name) {
		for (size_t t = 0; t < map.tracks.size(); t++) {
			if (map.tracks[t].name == name) return static_cast<uint32_t>(t);
		}
		return UINT32_MAX;
	};
	size_t begin = 0;
	while (begin <= text.size()) {
		size_t end = text.find('\n', begin);
		if (end == std::string_view::npos) end = text.size();
		std::string line(text.substr(begin, end - begin));
		begin = end + 1;
		line = line.substr(0, line.find('#'));
		std::istringstream words(line);
		std::vector<std::string> w;
		for (std::string word; words >> word;) w.push_back(word);
		if (w.empty()) continue;
		const auto number = [](const std::string &word, int &out) {
			const char *last = word.data() + word.size();
			const auto [ptr, ec] = std::from_chars(word.data(), last, out);
			return ec == std::errc() && ptr == last;
		};
		std::array<int, 2> at{};
		if (w[0] == "end" && w.size() == 5 && (w[2] == "start" || w[2] == "end") && number(w[3], at[0]) && number(w[4], at[1]) && track_of(w[1]) != UINT32_MAX) {
			pins.pins.push_back({track_of(w[1]), false, w[2] == "start", at});
			continue;
		}
		if (w[0] == "cross" && w.size() == 5 && number(w[3], at[0]) && number(w[4], at[1]) && track_of(w[1]) != UINT32_MAX && track_of(w[2]) != UINT32_MAX) {
			const uint32_t a = track_of(w[1]), b = track_of(w[2]);
			const auto it = std::ranges::find_if(map.crossings, [&](const auto &c) { return (c.a == a && c.b == b) || (c.a == b && c.b == a); });
			if (it != map.crossings.end()) {
				pins.pins.push_back({static_cast<uint32_t>(it - map.crossings.begin()), true, false, at});
				continue;
			}
		}
		pins.unmatched.push_back(line);
	}
	return pins;
}

PlanarDrawResult CheckPlanarDrawable(const PlanarShapeVariant &v, double seconds, const PlanarDrawOptions &options)
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
	std::vector<std::string> slack_tag; ///< Elastic: per slack variable its row's tag.
	std::vector<uint32_t> slack_var; ///< Elastic: the slack variables.
	const auto var = [&](double lower, double upper, double cost, std::string_view group) {
		columns++;
		return milp.AddVariable(lower, upper, cost, !options.elastic && std::ranges::find(options.continuous, group) == options.continuous.end());
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
		if (options.elastic) {
			const std::string family = tag.substr(0, tag.find(' '));
			if (family != "piece" && family != "box" && family != "pin") {
				if (std::isfinite(lower)) {
					merged.emplace_back(milp.AddVariable(0, JunctionMilp::INF, 1, false), 1.0);
					slack_tag.push_back(tag);
					slack_var.push_back(merged.back().first);
				}
				if (std::isfinite(upper)) {
					merged.emplace_back(milp.AddVariable(0, JunctionMilp::INF, 1, false), -1.0);
					slack_tag.push_back(tag);
					slack_var.push_back(merged.back().first);
				}
			}
		}
		milp.AddRow(merged, lower, upper);
		tags.push_back(tag);
	};
	const auto eq = [&](Terms terms, double rhs, const std::string &tag) { row(std::move(terms), rhs, rhs, tag); };

	const uint32_t width = var(1, COORD_MAX, 0, "other");
	const uint32_t height = var(1, COORD_MAX, 0, "other");

	/* Points: per track its start and the end of each piece. */
	std::vector<std::vector<std::array<uint32_t, 2>>> point(nt);
	std::vector<std::vector<Length>> length(nt);
	std::vector<std::vector<bool>> may_zero(nt);
	for (size_t t = 0; t < nt; t++) {
		const std::vector<int> &pieces = v.tracks[t].pieces;
		if (pieces.empty()) return unsupported("track " + map.tracks[t].name + " has no piece");
		for (size_t i = 0; i <= pieces.size(); i++) {
			point[t].push_back({var(0, COORD_MAX, 0, "point"), var(0, COORD_MAX, 0, "point")});
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
		/* D412: a port line with no event before its node is that stretch whole, and its length may be 0 (the node point on the box edge): its pieces are free, and no flex group of it keeps a least length. */
		const bool port_stretch = map.expanded.lines[t].from.port != map.expanded.lines[t].to.port && map.events[t].empty();
		if (port_stretch) free_piece.assign(pieces.size(), true);
		may_zero[t] = free_piece;
		std::set<uint32_t> middles(flex.begin(), flex.end());
		for (size_t i = 0; i < pieces.size(); i++) {
			const bool axis = (pieces[i] % 2) == 0;
			const double lower = free_piece[i] ? 0 : 1;
			length[t].push_back({var(lower, COORD_MAX, options.tidy && !options.elastic ? 1 : 0, axis ? "axis" : "diagonal"), axis ? AXIS_LEAST : 1});
		}
		for (const std::vector<uint32_t> &g : groups) {
			if (port_stretch) break;
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
		size_t on_side = 0, positioned = 0;
		for (const PlanarPort &port : map.expanded.ports) {
			if (static_cast<uint32_t>(port.side) != side) continue;
			on_side++;
			if (!port.at.empty()) positioned++;
		}
		if (positioned != 0 && positioned != on_side) return unsupported(fmt::format("port positions on side {} are partial", SIDE_NAME[side]));
		int previous_at = 0;
		for (uint32_t p = 0; p < map.expanded.ports.size(); p++) {
			const PlanarPort &port = map.expanded.ports[p];
			if (static_cast<uint32_t>(port.side) != side) continue;
			/* Lines enter in reading order when right of travel points along it, else reversed. */
			const int right = PlanarModel::STEP[(static_cast<int>(port.heading) + 2) % 8][along];
			std::vector<uint32_t> lines = port.lines;
			std::vector<int> positions = port.at;
			if (right < 0) {
				std::ranges::reverse(lines);
				std::ranges::reverse(positions);
			}
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
						eq({{edge_var, 1}}, 0, tag + " edge " + map.tracks[line].name);
						break;
					case PlanarSide::S:
						eq({{edge_var, 1}, {height, -1}}, 0, tag + " edge " + map.tracks[line].name);
						break;
					case PlanarSide::E:
						eq({{edge_var, 1}, {width, -1}}, 0, tag + " edge " + map.tracks[line].name);
						break;
				}
				if (!first) {
					int gap = k == 0 ? 2 * v.structure.side_gap[side] : 2;
					if (positioned != 0) {
						gap = 2 * (positions[k] - previous_at);
						if (gap < 2) return unsupported(fmt::format("port positions out of reading order on side {}", SIDE_NAME[side]));
					}
					eq({{at, 1}, {previous, -1}}, gap, tag + " gap " + map.tracks[line].name);
				}
				first = false;
				previous = at;
				if (positioned != 0) previous_at = positions[k];
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
			param[c][side] = var(0, COORD_MAX, 0, "param");
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
	/* The crossing point of each crossing: where the first track's piece reaches it. */
	std::vector<std::array<uint32_t, 2>> cross_point(nc);
	for (size_t c = 0; c < nc; c++) {
		const uint32_t t = map.crossings[c].a;
		const std::array<int, 2> step = PlanarModel::STEP[((v.tracks[t].pieces[piece_of[c][0]] % 8) + 8) % 8];
		for (int axis = 0; axis < 2; axis++) {
			cross_point[c][axis] = var(0, COORD_MAX, 0, "cross");
			eq({{cross_point[c][axis], 1}, {point[t][piece_of[c][0]][axis], -1}, {param[c][0], -step[axis]}}, 0, fmt::format("cross {}", c));
		}
	}
	/* Pins (D393): every pinned point is within the window of its position plus one common shift. */
	if (options.pins != nullptr && !options.pins->pins.empty()) {
		const std::array<uint32_t, 2> shift = {var(-COORD_MAX, COORD_MAX, 0, "other"), var(-COORD_MAX, COORD_MAX, 0, "other")};
		for (const PlanarDrawPins::Pin &pin : options.pins->pins) {
			std::array<uint32_t, 2> p;
			std::string tag;
			if (pin.cross) {
				if (pin.track >= nc) return unsupported("a pin names a crossing the map lacks");
				p = cross_point[pin.track];
				tag = fmt::format("pin cross {} {}", map.tracks[map.crossings[pin.track].a].name, map.tracks[map.crossings[pin.track].b].name);
			} else {
				if (pin.track >= nt) return unsupported("a pin names a track the map lacks");
				p = pin.start ? point[pin.track].front() : point[pin.track].back();
				tag = fmt::format("pin end {} {}", map.tracks[pin.track].name, pin.start ? "start" : "end");
			}
			for (int axis = 0; axis < 2; axis++) row({{p[axis], 1}, {shift[axis], -1}}, pin.at[axis] - options.pins->window, pin.at[axis] + options.pins->window, tag);
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

	/* Dummies: from the bend's vertex along the heading for a length D >= 1 to the landing E. */
	const size_t nd = options.clearance && !options.pairwise ? v.dummies.size() : 0;
	const auto face_listed = [&](uint32_t face) { return options.only_faces.empty() || std::ranges::find(options.only_faces, face) != options.only_faces.end(); };
	std::vector<std::array<uint32_t, 2>> dummy_start(nd), dummy_end(nd);
	std::vector<uint32_t> dummy_length(nd);
	for (size_t d = 0; d < nd; d++) {
		const PlanarShapeDummy &dummy = v.dummies[d];
		if (!face_listed(dummy.face)) continue;
		if (dummy.track >= nt || dummy.piece + 1 >= point[dummy.track].size()) return unsupported(fmt::format("dummy {} starts at a vertex that is not a track's", d));
		dummy_start[d] = point[dummy.track][dummy.piece + 1];
		dummy_length[d] = var(1, COORD_MAX, 0, "other");
		dummy_end[d] = {var(0, COORD_MAX, 0, "other"), var(0, COORD_MAX, 0, "other")};
		const std::string tag = fmt::format("land {}", d);
		row({{dummy_end[d][0], 1}, {width, -1}}, -JunctionMilp::INF, 0, tag);
		row({{dummy_end[d][1], 1}, {height, -1}}, -JunctionMilp::INF, 0, tag);
		const std::array<int, 2> step = PlanarModel::STEP[((dummy.heading % 8) + 8) % 8];
		for (int axis = 0; axis < 2; axis++) eq({{dummy_end[d][axis], 1}, {dummy_start[d][axis], -1}, {dummy_length[d], -step[axis]}}, 0, tag);
	}
	for (size_t d = 0; d < nd; d++) {
		const PlanarShapeDummy &dummy = v.dummies[d];
		if (!face_listed(dummy.face)) continue;
		const std::string tag = fmt::format("land {}", d);
		if (dummy.land_track == UINT32_MAX) {
			if (dummy.land_piece >= 4) return unsupported(fmt::format("dummy {} lands on box side {}", d, dummy.land_piece));
			switch (static_cast<PlanarSide>(dummy.land_piece)) {
				case PlanarSide::N: eq({{dummy_end[d][1], 1}}, 0, tag); break;
				case PlanarSide::S: eq({{dummy_end[d][1], 1}, {height, -1}}, 0, tag); break;
				case PlanarSide::W: eq({{dummy_end[d][0], 1}}, 0, tag); break;
				case PlanarSide::E: eq({{dummy_end[d][0], 1}, {width, -1}}, 0, tag); break;
			}
			continue;
		}
		/* Along a piece or another dummy: E = P + w STEP[h] with 0 <= w <= its length. */
		std::array<uint32_t, 2> from;
		int heading;
		Terms most;
		if (dummy.land_track == PLANAR_DUMMY_TRACK) {
			if (dummy.land_piece >= nd) return unsupported(fmt::format("dummy {} lands on dummy {}", d, dummy.land_piece));
			from = dummy_start[dummy.land_piece];
			heading = v.dummies[dummy.land_piece].heading;
			most = {{dummy_length[dummy.land_piece], -1}};
		} else {
			if (dummy.land_track >= nt || dummy.land_piece >= v.tracks[dummy.land_track].pieces.size()) return unsupported(fmt::format("dummy {} lands on a piece that does not exist", d));
			from = point[dummy.land_track][dummy.land_piece];
			heading = v.tracks[dummy.land_track].pieces[dummy.land_piece];
			const Length &len = length[dummy.land_track][dummy.land_piece];
			most = {{len.variable, -len.coefficient}};
		}
		const uint32_t w = var(0, COORD_MAX, 0, "other");
		most.emplace_back(w, 1);
		row(most, -JunctionMilp::INF, 0, tag);
		const std::array<int, 2> step = PlanarModel::STEP[((heading % 8) + 8) % 8];
		for (int axis = 0; axis < 2; axis++) eq({{dummy_end[d][axis], 1}, {from[axis], -1}, {w, -step[axis]}}, 0, tag);
	}

	/* Clearance: in every convex cell, each vertex keeps its spacing from the line of every other wall. */
	std::map<uint32_t, uint32_t> same_point; // Union-find over the x variable of points that nodes make equal.
	const std::function<uint32_t(uint32_t)> find = [&](uint32_t x) {
		const auto it = same_point.find(x);
		if (it == same_point.end() || it->second == x) return x;
		const uint32_t root = find(it->second);
		same_point[x] = root;
		return root;
	};
	for (const PlanarNode &node : map.expanded.nodes) {
		std::vector<uint32_t> xs;
		for (const uint32_t line : node.in) xs.push_back(point[line].back()[0]);
		for (const uint32_t line : node.out) xs.push_back(point[line].front()[0]);
		for (size_t k = 1; k < xs.size(); k++) same_point[find(xs[k])] = find(xs[0]);
	}
	/* The span pieces: a structure's separator piece at its crossings, and its copy's at the copy's crossings (H5). Their middles are underground or elevated, so ground track lies over or under them anywhere: they are no wall for clearance. */
	std::set<std::pair<uint32_t, uint32_t>> span_piece;
	for (const PlanarStructure &structure : v.structure.structures) {
		if (structure.crossings.empty()) continue;
		for (const uint32_t track : {structure.separator, structure.copy}) {
			if (track >= nt || v.tracks[track].event_piece.size() != map.events[track].size()) continue;
			for (size_t k = 0; k < map.events[track].size(); k++) {
				const uint32_t c = map.events[track][k];
				const bool crosser = std::ranges::find(structure.crossings, c) != structure.crossings.end();
				if (track == structure.separator ? crosser : true) span_piece.emplace(track, v.tracks[track].event_piece[k]);
			}
		}
	}
	const auto is_span = [&](const PlanarCellWall &w) { return w.track < nt && span_piece.contains({w.track, w.piece}); };
	const auto lands_on_span = [&](const PlanarCellWall &w) {
		if (w.track != PLANAR_DUMMY_TRACK || w.piece >= nd) return false;
		const PlanarShapeDummy &d = v.dummies[w.piece];
		return d.land_track < nt && span_piece.contains({d.land_track, d.land_piece});
	};
	const std::vector<std::vector<PlanarCellWall>> cells = options.clearance && options.clear_rows && !options.pairwise ? PlanarShapeCellWalls(v) : std::vector<std::vector<PlanarCellWall>>();
	/* The clearance rows of one convex cell (or, in pairwise mode, of a face convex without dummies); false when a wall is one the variant lacks. */
	const auto clear_walls = [&](const std::vector<PlanarCellWall> &cell, size_t c) {
		const size_t n = cell.size();
		/* Each wall's end points in the walk; a wall a dummy lands on is held only in part. */
		struct Part {
			bool valid = false;
			std::array<uint32_t, 2> a{}, b{};
		};
		std::vector<Part> part(n);
		bool usable = true;
		for (size_t i = 0; i < n && usable; i++) {
			const PlanarCellWall &w = cell[i];
			if (w.track == UINT32_MAX) continue;
			bool forward;
			if (w.track == PLANAR_DUMMY_TRACK) {
				if (w.piece >= nd) {
					usable = false;
					break;
				}
				forward = w.heading == ((v.dummies[w.piece].heading % 8) + 8) % 8;
				part[i] = {true, dummy_start[w.piece], dummy_end[w.piece]};
			} else {
				if (w.track >= nt || w.piece >= v.tracks[w.track].pieces.size()) {
					usable = false;
					break;
				}
				forward = w.heading == ((v.tracks[w.track].pieces[w.piece] % 8) + 8) % 8;
				part[i] = {true, point[w.track][w.piece], point[w.track][w.piece + 1]};
			}
			if (!forward) std::swap(part[i].a, part[i].b);
		}
		if (!usable) return false;
		for (size_t i = 0; i < n; i++) {
			if (!part[i].valid) continue;
			const PlanarCellWall &before = cell[(i + n - 1) % n];
			const PlanarCellWall &after = cell[(i + 1) % n];
			/* A dummy walked from its corner ends on the wall that follows it; one walked back to its corner starts on the wall that precedes it. */
			if (before.track == PLANAR_DUMMY_TRACK && before.heading == ((v.dummies[before.piece].heading % 8) + 8) % 8) part[i].a = dummy_end[before.piece];
			if (after.track == PLANAR_DUMMY_TRACK && after.heading != ((v.dummies[after.piece].heading % 8) + 8) % 8) part[i].b = dummy_end[after.piece];
		}
		/* A corner between pieces of two tracks that cross each other on exactly those pieces is the crossing point; the wall in this cell ends there. */
		for (size_t k = 0; k < n; k++) {
			const size_t next = (k + 1) % n;
			const PlanarCellWall &w0 = cell[k];
			const PlanarCellWall &w1 = cell[next];
			if (w0.track >= nt || w1.track >= nt || w0.track == w1.track) continue;
			for (size_t c = 0; c < nc; c++) {
				const std::array<uint32_t, 2> tracks = {map.crossings[c].a, map.crossings[c].b};
				for (int flip = 0; flip < 2; flip++) {
					if (tracks[flip] != w0.track || tracks[1 - flip] != w1.track) continue;
					if (piece_of[c][flip] != w0.piece || piece_of[c][1 - flip] != w1.piece) continue;
					part[k].b = cross_point[c];
					part[next].a = cross_point[c];
				}
			}
		}
		std::vector<bool> insert(n, false);
		for (size_t i = 0; i < n; i++) {
			if (options.flex_sides && cell[i].track < nt) insert[i] = std::ranges::find(v.tracks[cell[i].track].flex, cell[i].piece) != v.tracks[cell[i].track].flex.end();
		}
		const auto walked_forward = [&](const PlanarCellWall &w) { return w.heading == ((v.dummies[w.piece].heading % 8) + 8) % 8; };
		/* A vertex's point: the end of its wall, else the start of the next; false when it has none. */
		const auto vertex_point = [&](size_t k, std::array<uint32_t, 2> &vertex) {
			if (part[k].valid) {
				vertex = part[k].b;
			} else if (part[(k + 1) % n].valid) {
				vertex = part[(k + 1) % n].a;
			} else {
				return false;
			}
			return true;
		};
		/* A vertex inside a span piece (a dummy's landing on it, or between two parts of it) has no clearance against ground track. */
		const auto inside_span = [&](size_t k) {
			const size_t next = (k + 1) % n;
			return (is_span(cell[k]) && is_span(cell[next])) || (lands_on_span(cell[k]) && walked_forward(cell[k]) && is_span(cell[next])) || (is_span(cell[k]) && lands_on_span(cell[next]) && !walked_forward(cell[next]));
		};
		std::vector<PlanarAlongRow> along;
		for (const PlanarClearRow &r : PlanarCellClearRows(cell, insert, options.pocket && options.flex_sides, &along)) {
			const size_t k = r.vertex, e = r.wall;
			std::array<uint32_t, 2> vertex;
			if (!vertex_point(k, vertex)) continue;
			if (inside_span(k)) continue;
			if (!part[e].valid || is_span(cell[e])) continue;
			if (!options.flex_sides && cell[e].track == PLANAR_DUMMY_TRACK) continue;
			if (find(vertex[0]) == find(part[e].a[0]) || find(vertex[0]) == find(part[e].b[0])) continue;
			const int h = cell[e].heading;
			const std::array<int, 2> normal = PlanarModel::STEP[(h + 6) % 8];
			Terms terms;
			for (int axis = 0; axis < 2; axis++) {
				terms.emplace_back(vertex[axis], normal[axis]);
				terms.emplace_back(part[e].a[axis], -normal[axis]);
			}
			Terms room_terms;
			if (r.room != UINT32_MAX) {
				/* The room row: M * (d - spacing) - length >= 0, so the insert has length only when the vertex keeps more than its base spacing. */
				room_terms = terms;
				for (auto &term : room_terms) term.second *= COORD_MAX;
				const PlanarCellWall &flex = cell[r.room];
				const Length &len = length[flex.track][flex.piece];
				room_terms.emplace_back(len.variable, -len.coefficient);
			}
			row(std::move(terms), r.spacing, JunctionMilp::INF, fmt::format("clear {} {} {}", c, k, e));
			if (r.room != UINT32_MAX) row(std::move(room_terms), COORD_MAX * r.spacing, JunctionMilp::INF, fmt::format("room {} {} {}", c, k, e));
		}
		for (const PlanarAlongRow &r : along) {
			std::array<uint32_t, 2> vertex, anchor;
			if (!vertex_point(r.vertex, vertex) || !vertex_point(r.anchor, anchor) || inside_span(r.vertex)) continue;
			const std::array<int, 2> step = PlanarModel::STEP[r.heading];
			Terms terms;
			for (int axis = 0; axis < 2; axis++) {
				terms.emplace_back(vertex[axis], step[axis]);
				terms.emplace_back(anchor[axis], -step[axis]);
			}
			row(std::move(terms), r.spacing, JunctionMilp::INF, fmt::format("along {} {} {}", c, r.vertex, r.anchor));
		}
		return true;
	};
	const std::vector<uint32_t> cell_faces = cells.empty() ? std::vector<uint32_t>() : PlanarShapeCellFaces(v);
	if (cell_faces.size() != cells.size() && !cells.empty()) return unsupported("the cells and their faces disagree");
	for (size_t c = 0; c < cells.size(); c++) {
		if (!face_listed(cell_faces[c])) continue;
		if (!clear_walls(cells[c], c)) return unsupported(fmt::format("cell {} names a wall the variant lacks", c));
	}

	/* Pairwise mode (D406 probe): a face convex without dummies keeps the clearance rows, every other face separates its non-incident wall pairs. */
	if (options.clearance && options.pairwise) {
		constexpr double M = 4 * COORD_MAX;
		const auto norm = [](int x) { return ((x % 8) + 12) % 8 - 4; };
		if (options.sides_from != nullptr) {
			if (options.sides_from->size() != nt) return unsupported("sides_from does not match the variant");
			for (size_t t = 0; t < nt; t++) {
				if ((*options.sides_from)[t].size() != point[t].size()) return unsupported("sides_from does not match the variant");
			}
		}
		for (const PlanarFaceWalls &f : PlanarShapeFaceWalls(v)) {
			if (!face_listed(f.face)) continue;
			const std::vector<PlanarCellWall> &w = f.walls;
			const size_t n = w.size();
			bool convex = true;
			for (size_t k = 0; k < n; k++) {
				if (norm(w[(k + 1) % n].heading - w[k].heading) > 0) convex = false;
			}
			if (convex) {
				if (!clear_walls(w, f.face)) return unsupported(fmt::format("cell {} names a wall the variant lacks", f.face));
				continue;
			}
			for (size_t i = 0; i < n; i++) {
				for (size_t j = i + 1; j < n; j++) {
					if (j == i + 1 || (i == 0 && j == n - 1)) continue;
					if (w[i].track >= nt || w[j].track >= nt) continue;
					if (is_span(w[i]) || is_span(w[j])) continue;
					if (w[i].track == w[j].track && w[i].piece == w[j].piece) continue;
					if (w[i].piece >= v.tracks[w[i].track].pieces.size() || w[j].piece >= v.tracks[w[j].track].pieces.size()) return unsupported(fmt::format("face {} names a wall the variant lacks", f.face));
					if (w[i].track == w[j].track) {
						const uint32_t lo = std::min(w[i].piece, w[j].piece);
						const uint32_t hi = std::max(w[i].piece, w[j].piece);
						bool all_zero = true;
						for (uint32_t k = lo + 1; k < hi; k++) all_zero = all_zero && may_zero[w[i].track][k];
						if (all_zero) continue;
					}
					const std::array<std::array<uint32_t, 2>, 2> pi = {point[w[i].track][w[i].piece], point[w[i].track][w[i].piece + 1]};
					const std::array<std::array<uint32_t, 2>, 2> pj = {point[w[j].track][w[j].piece], point[w[j].track][w[j].piece + 1]};
					bool shared = false;
					for (const auto &p : pi) {
						for (const auto &q : pj) shared = shared || find(p[0]) == find(q[0]);
					}
					if (shared) continue;
					bool crossing = false;
					for (size_t ki = 0; ki < std::min(map.events[w[i].track].size(), v.tracks[w[i].track].event_piece.size()) && !crossing; ki++) {
						if (v.tracks[w[i].track].event_piece[ki] != w[i].piece) continue;
						for (size_t kj = 0; kj < std::min(map.events[w[j].track].size(), v.tracks[w[j].track].event_piece.size()); kj++) {
							if (v.tracks[w[j].track].event_piece[kj] == w[j].piece && map.events[w[i].track][ki] == map.events[w[j].track][kj]) crossing = true;
						}
					}
					if (crossing) continue;
					result.pairs++;
					struct Axis {
						std::array<int, 2> s;
						int spacing;
					};
					std::vector<Axis> axes = {{PlanarModel::STEP[(w[i].heading + 6) % 8], PlanarModel::RungSpacing(w[i].heading, w[j].heading)}};
					if (w[i].heading % 4 != w[j].heading % 4) {
						axes.push_back({PlanarModel::STEP[(w[j].heading + 6) % 8], PlanarModel::RungSpacing(w[j].heading, w[i].heading)});
					} else {
						axes.push_back({PlanarModel::STEP[w[i].heading % 8], 2});
					}
					if (options.sides_from != nullptr) {
						const auto ref = [&](uint32_t track, uint32_t k) { return (*options.sides_from)[track][k]; };
						const std::array<std::array<uint32_t, 2>, 2> *best_from = nullptr, *best_to = nullptr;
						const Axis *best_axis = nullptr;
						int best_margin = 0;
						const std::array<std::array<uint32_t, 2>, 2> *const ends[2] = {&pi, &pj};
						const std::array<uint32_t, 2> tracks = {w[i].track, w[j].track};
						const std::array<uint32_t, 2> pieces = {w[i].piece, w[j].piece};
						for (const Axis &axis : axes) {
							for (int side = 0; side < 2; side++) {
								const auto *from = ends[side];
								const auto *to = ends[1 - side];
								int margin = INT_MAX;
								for (uint32_t kp = 0; kp < 2; kp++) {
									for (uint32_t kq = 0; kq < 2; kq++) {
										const std::array<int, 2> rp = ref(tracks[side], pieces[side] + kp);
										const std::array<int, 2> rq = ref(tracks[1 - side], pieces[1 - side] + kq);
										margin = std::min(margin, (rq[0] - rp[0]) * axis.s[0] + (rq[1] - rp[1]) * axis.s[1] - axis.spacing);
									}
								}
								if (best_axis == nullptr || margin > best_margin) {
									best_axis = &axis;
									best_from = from;
									best_to = to;
									best_margin = margin;
								}
							}
						}
						if (best_margin < 0) result.short_sides++;
						for (const auto &p : *best_from) {
							for (const auto &q : *best_to) {
								Terms t;
								for (int d = 0; d < 2; d++) {
									t.emplace_back(q[d], best_axis->s[d]);
									t.emplace_back(p[d], -best_axis->s[d]);
								}
								row(std::move(t), best_axis->spacing, JunctionMilp::INF, fmt::format("sep {} {} {}", f.face, i, j));
							}
						}
						continue;
					}
					Terms bs;
					for (const Axis &axis : axes) {
						for (int side = 0; side < 2; side++) {
							const auto &from = side == 0 ? pi : pj;
							const auto &to = side == 0 ? pj : pi;
							const uint32_t b = var(0, 1, 0, "other");
							result.binaries++;
							bs.emplace_back(b, 1);
							for (const auto &p : from) {
								for (const auto &q : to) {
									Terms t{{b, -M}};
									for (int d = 0; d < 2; d++) {
										t.emplace_back(q[d], axis.s[d]);
										t.emplace_back(p[d], -axis.s[d]);
									}
									row(std::move(t), axis.spacing - M, JunctionMilp::INF, fmt::format("sep {} {} {}", f.face, i, j));
								}
							}
						}
					}
					row(std::move(bs), 1, 1, fmt::format("sepdir {} {} {}", f.face, i, j));
				}
			}
		}
	}

	result.rows = static_cast<uint32_t>(tags.size());
	result.columns = columns;
	milp.SetTimeLimit(seconds);
	const JunctionMilp::Status status = milp.Solve();
	if (status == JunctionMilp::Status::Optimal) {
		result.feasible = true;
		result.status = "Optimal";
		if (options.elastic) {
			std::map<std::string, double> per_tag;
			for (size_t i = 0; i < slack_var.size(); i++) {
				const double value = milp.Value(slack_var[i]);
				result.slack += value;
				per_tag[slack_tag[i]] += value;
			}
			for (const auto &[tag, amount] : per_tag) {
				if (amount > 1e-6) result.stretched.emplace_back(tag, amount);
			}
			std::ranges::sort(result.stretched, [](const auto &a, const auto &b) { return a.second != b.second ? a.second > b.second : a.first < b.first; });
		}
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
