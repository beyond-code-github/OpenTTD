/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/**
 * @file planar_draw.cpp The drawing program: points and pieces, box, ports, nodes, crossings, spans, dummies with their landings and clearance per convex cell (E5-1, E5-2), bays (option), curve spacing (option; with corridors, a beside track's opening and closing steps are bends, and committed flex inserts are real bends), corridors (option).
 * Not in it yet: grid parity (D382), node legs and the trident binary, coverage.
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
#include <fstream>
#include <functional>
#include <map>
#include <numeric>
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

std::array<int, 2> PlanarBesideOpening(int heading, int side)
{
	const std::array<int, 2> step = PlanarModel::STEP[(((heading % 8) + 8) % 8 + side + 8) % 8];
	return {2 * step[0], 2 * step[1]};
}

std::array<int, 2> PlanarBesideClosing(int heading, int side)
{
	const std::array<int, 2> step = PlanarModel::STEP[(((heading % 8) + 8) % 8 - side + 8) % 8];
	return {2 * step[0], 2 * step[1]};
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
	const bool exporting = !options.export_path.empty();
	std::string export_variables; ///< Exporting: the variables as JSON objects, in index order.
	std::string export_rows; ///< Exporting: the rows as JSON objects, in row order.
	double export_big_m = 0; ///< Exporting: the pairwise block's big-M constant.
	const auto json_number = [](double x) { return std::isfinite(x) ? fmt::format("{}", x) : std::string("null"); };
	std::vector<bool> integral;
	std::vector<double> var_lower, var_upper;
	struct Pending {
		Terms terms;
		double lower;
		double upper;
		std::string tag;
	};
	std::vector<Pending> pending;
	const auto add_variable = [&](double lower, double upper, double cost, bool integer) {
		integral.push_back(integer);
		var_lower.push_back(lower);
		var_upper.push_back(upper);
		if (exporting) export_variables += fmt::format("{}{{\"lb\":{},\"ub\":{},\"cost\":{},\"integer\":{}}}", export_variables.empty() ? "" : ",", json_number(lower), json_number(upper), json_number(cost), integer ? "true" : "false");
		return milp.AddVariable(lower, upper, cost, integer);
	};
	const auto var = [&](double lower, double upper, double cost, std::string_view group) {
		columns++;
		return add_variable(lower, upper, cost, (!options.elastic || options.elastic_integer) && std::ranges::find(options.continuous, group) == options.continuous.end());
	};
	std::map<std::string, double> soft_price; ///< Scheme A: per "curve flex" tag its slack price.
	const auto emit = [&](Terms merged, double lower, double upper, const std::string &tag) {
		if (options.elastic) {
			const std::string family = tag.substr(0, tag.find(' '));
			const auto soft = soft_price.find(tag);
			const double price = soft != soft_price.end() ? soft->second : options.slack_price;
			if (soft != soft_price.end() || (family != "piece" && family != "box" && family != "pin" && family != "fix" && family != "grid" && (options.elastic_families.empty() || std::ranges::find(options.elastic_families, family) != options.elastic_families.end()))) {
				if (std::isfinite(lower)) {
					merged.emplace_back(add_variable(0, JunctionMilp::INF, price, false), 1.0);
					slack_tag.push_back(tag);
					slack_var.push_back(merged.back().first);
				}
				if (std::isfinite(upper)) {
					merged.emplace_back(add_variable(0, JunctionMilp::INF, price, false), -1.0);
					slack_tag.push_back(tag);
					slack_var.push_back(merged.back().first);
				}
			}
		}
		milp.AddRow(merged, lower, upper);
		tags.push_back(tag);
		if (exporting) {
			std::string terms_json;
			for (const auto &[variable, coefficient] : merged) terms_json += fmt::format("{}[{},{}]", terms_json.empty() ? "" : ",", variable, json_number(coefficient));
			std::string tag_json;
			for (const char c : tag) {
				if (c == '"' || c == '\\') tag_json += '\\';
				tag_json += c;
			}
			export_rows += fmt::format("{}{{\"terms\":[{}],\"lb\":{},\"ub\":{},\"tag\":\"{}\"}}", export_rows.empty() ? "" : ",", terms_json, json_number(lower), json_number(upper), tag_json);
		}
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
		if (options.grid && !options.elastic) {
			pending.push_back({std::move(merged), lower, upper, tag});
			return;
		}
		emit(std::move(merged), lower, upper, tag);
	};
	const auto eq = [&](Terms terms, double rhs, const std::string &tag) { row(std::move(terms), rhs, rhs, tag); };

	const uint32_t width = var(1, COORD_MAX, options.box_price, "other");
	const uint32_t height = var(1, COORD_MAX, options.box_price, "other");

	/* Points: per track its start and the end of each piece. */
	if (options.fix_points != nullptr) {
		if (options.fix_points->size() != nt) return unsupported("fix_points does not match the variant");
		for (size_t t = 0; t < nt; t++) {
			if (!(*options.fix_points)[t].empty() && (*options.fix_points)[t].size() != v.tracks[t].pieces.size() + 1) return unsupported("fix_points does not match the variant");
		}
	}
	std::vector<std::vector<std::array<uint32_t, 2>>> point(nt);
	std::vector<std::vector<Length>> length(nt);
	std::vector<std::vector<bool>> may_zero(nt);
	std::map<uint32_t, int> grid_seed; ///< Grid (D382 probe): point variables whose parity the grid fixes.
	for (size_t t = 0; t < nt; t++) {
		const std::vector<int> &pieces = v.tracks[t].pieces;
		if (pieces.empty()) return unsupported("track " + map.tracks[t].name + " has no piece");
		for (size_t i = 0; i <= pieces.size(); i++) {
			point[t].push_back({var(0, COORD_MAX, 0, "point"), var(0, COORD_MAX, 0, "point")});
			row({{point[t][i][0], 1}, {width, -1}}, -JunctionMilp::INF, 0, "box");
			row({{point[t][i][1], 1}, {height, -1}}, -JunctionMilp::INF, 0, "box");
			if (options.fix_points != nullptr && !(*options.fix_points)[t].empty()) {
				for (int axis = 0; axis < 2; axis++) row({{point[t][i][axis], 1}}, (*options.fix_points)[t][i][axis] - options.fix_window, (*options.fix_points)[t][i][axis] + options.fix_window, "fix");
			}
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
		/* Option A probe: the flanks of each used step may be 0 like a flex group's, the insert keeps its lower bound. */
		const size_t flex_groups = groups.size();
		std::vector<bool> flex_piece = free_piece;
		const std::vector<uint32_t> &steps = v.tracks[t].steps;
		for (size_t f = 0; f < steps.size(); f++) {
			const uint32_t m = steps[f];
			if (m == 0 || m + 1 >= pieces.size()) return unsupported("a step middle of track " + map.tracks[t].name + " is out of range");
			if (flex_piece[m - 1] || flex_piece[m] || flex_piece[m + 1]) return unsupported("a step of track " + map.tracks[t].name + " overlaps a flex group");
			if (f == 0 || steps[f - 1] + 2 != m) groups.push_back({m - 1});
			groups.back().push_back(m);
			groups.back().push_back(m + 1);
		}
		for (size_t g = flex_groups; g < groups.size(); g++) {
			for (const uint32_t i : groups[g]) free_piece[i] = true;
		}
		for (const uint32_t m : steps) free_piece[m] = false;
		/* D412: a port line with no event before its node is that stretch whole, and its length may be 0 (the node point on the box edge): its pieces are free, and no flex group of it keeps a least length. */
		const bool port_stretch = map.expanded.lines[t].from.port != map.expanded.lines[t].to.port && map.events[t].empty();
		if (port_stretch) free_piece.assign(pieces.size(), true);
		may_zero[t] = free_piece;
		std::set<uint32_t> middles(flex.begin(), flex.end());
		middles.insert(steps.begin(), steps.end());
		std::vector<bool> positive; ///< Per piece: its least length is above 0.
		std::vector<bool> capped; ///< Per piece: its upper bound is 0 (an insert held at 0).
		for (size_t i = 0; i < pieces.size(); i++) {
			const bool axis = (pieces[i] % 2) == 0;
			const double lower = free_piece[i] ? 0 : 1;
			const bool zero = options.flex_zero_from != nullptr && free_piece[i] && std::ranges::find(flex, static_cast<uint32_t>(i)) != flex.end() && t < options.flex_zero_from->size() && i < (*options.flex_zero_from)[t].size() && (*options.flex_zero_from)[t][i] < options.flex_zero_below;
			const bool in_flex = std::ranges::find(flex, static_cast<uint32_t>(i)) != flex.end();
			const bool reference = options.curve_flex_from != nullptr && in_flex && t < options.curve_flex_from->size() && i < (*options.curve_flex_from)[t].size() && (*options.curve_flex_from)[t][i] >= options.curve_flex_above;
			const bool held = options.curve_flex_from != nullptr && in_flex && !reference && !options.curve_flex_free;
			positive.push_back(reference || lower > 0);
			capped.push_back(zero || held);
			length[t].push_back({var(reference ? 1 : lower, (zero || held) ? 0 : COORD_MAX, (options.elastic ? (options.tidy ? options.elastic_tidy : 0) : (options.tidy ? 1 : 0)) + (in_flex ? (options.flex_weights != nullptr && t < options.flex_weights->size() && i < (*options.flex_weights)[t].size() ? (*options.flex_weights)[t][i] : options.flex_price) * (axis ? AXIS_LEAST : 1) : 0), axis ? "axis" : "diagonal"), axis ? AXIS_LEAST : 1});
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
		/* Grid (D382 probe): every point of an axis run of positive length lies on the run's tile-centre line, N/S on odd x and E/W on odd y, so it is odd there (a seed of the parity pass before the solve). A run is the pieces of one heading joined through inserts held at 0; it is positive when a piece of it has a least length, or when it holds a whole flex group whose inserts are all held at 0 (the group's flex row). */
		if (options.grid) {
			std::vector<bool> run_positive = positive;
			if (!port_stretch) {
				for (const std::vector<uint32_t> &g : groups) {
					if (!std::ranges::all_of(g, [&](uint32_t i) { return !middles.contains(i) || capped[i]; })) continue;
					for (const uint32_t i : g) {
						if (!middles.contains(i)) run_positive[i] = true;
					}
				}
			}
			std::set<std::pair<size_t, int>> gridded;
			for (size_t i = 0; i < pieces.size();) {
				if ((pieces[i] % 2) != 0 || capped[i]) {
					i++;
					continue;
				}
				size_t last = i;
				bool known = run_positive[i];
				for (size_t j = i + 1; j < pieces.size(); j++) {
					if (capped[j]) continue;
					if (pieces[j] != pieces[i]) break;
					last = j;
					known = known || run_positive[j];
				}
				if (known) {
					const int across = (((pieces[i] % 8) + 8) % 8) % 4 == 0 ? 0 : 1;
					for (size_t at = i; at <= last + 1; at++) {
						if (!gridded.insert({at, across}).second) continue;
						grid_seed.emplace(point[t][at][across], 1);
					}
				}
				i = last + 1;
			}
		}
	}

	/* Node gaps (SPEC-slots-lengths §8.1 node_leg, D400): a track between two nodes with no event travels 0 (a trident) or at least node_leg. */
	if (options.node_gaps) {
		for (size_t t = 0; t < nt; t++) {
			const auto &line = map.expanded.lines[t];
			if (line.from.port || line.to.port || !map.events[t].empty()) continue;
			Terms travel;
			for (const Length &len : length[t]) travel.emplace_back(len.variable, len.coefficient);
			double reference = 2 * rules.node_leg;
			if (options.node_gap_from != nullptr && t < options.node_gap_from->size()) {
				reference = 0;
				for (const double l : (*options.node_gap_from)[t]) reference += l;
			}
			if (reference < 1) {
				row(std::move(travel), -JunctionMilp::INF, 0, fmt::format("leg zero {}", map.tracks[t].name));
			} else {
				row(std::move(travel), rules.node_leg, JunctionMilp::INF, fmt::format("leg {}", map.tracks[t].name));
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

	/* Corridors (D421): per beside track its parent chain, side, and its opening and closing steps. */
	struct Beside {
		uint32_t track;
		std::vector<uint32_t> chain;
		int side;
		std::array<int, 2> opening;
		std::array<int, 2> closing;
	};
	std::vector<Beside> besides;
	std::vector<bool> is_beside(nt, false);
	if (options.corridor) {
		for (uint32_t b = 0; b < nt; b++) {
			if (map.part_of[b] != UINT32_MAX) continue;
			const std::string &name = map.tracks[b].name;
			Beside beside{b, {}, 0, {}, {}};
			if (!ParentChain(map, v.structure.sites, b, beside.chain)) return unsupported("corridor: no parent chain for " + name);
			std::vector<int> chain_pieces;
			for (const uint32_t t : beside.chain) {
				if (!v.tracks[t].flex.empty() || !v.tracks[t].steps.empty()) return unsupported("corridor: parent chain track " + map.tracks[t].name + " of " + name + " has flex or steps");
				/* A node's piece once (H8): a join at equal headings is one piece, as the chain's points drop the node point there. */
				const std::vector<int> &tp = v.tracks[t].pieces;
				const bool merge = !chain_pieces.empty() && !tp.empty() && ((chain_pieces.back() - tp.front()) % 8 + 8) % 8 == 0;
				chain_pieces.insert(chain_pieces.end(), tp.begin() + (merge ? 1 : 0), tp.end());
			}
			if (!v.tracks[b].flex.empty() || !v.tracks[b].steps.empty()) return unsupported("corridor: " + name + " has flex or steps");
			const std::vector<int> &pieces = v.tracks[b].pieces;
			bool same = pieces.size() == chain_pieces.size();
			for (size_t i = 0; same && i < pieces.size(); i++) same = (((pieces[i] - chain_pieces[i]) % 8) + 8) % 8 == 0;
			if (!same) {
				std::string own_text, chain_text;
				for (const int h : pieces) own_text += fmt::format("{}{}", own_text.empty() ? "" : " ", h);
				for (const int h : chain_pieces) chain_text += fmt::format("{}{}", chain_text.empty() ? "" : " ", h);
				return unsupported("corridor: " + name + " does not follow its parent chain's pieces: own [" + own_text + "] chain [" + chain_text + "]");
			}
			beside.side = map.Side(b, 0, beside.chain[0], 0);
			if (beside.side == 0) return unsupported("corridor: " + name + " has no side to its parent chain");
			beside.opening = PlanarBesideOpening(pieces.front(), beside.side);
			beside.closing = PlanarBesideClosing(pieces.back(), beside.side);
			besides.push_back(std::move(beside));
			is_beside[b] = true;
		}
	}

	/* Nodes: every line's end at a node is the node's one point; a corridor's beside track ends one lane step from it. */
	for (size_t n = 0; n < map.expanded.nodes.size(); n++) {
		const PlanarNode &node = map.expanded.nodes[n];
		std::vector<std::array<uint32_t, 2>> ends;
		for (const uint32_t line : node.in) {
			if (!is_beside[line]) ends.push_back(point[line].back());
		}
		for (const uint32_t line : node.out) {
			if (!is_beside[line]) ends.push_back(point[line].front());
		}
		const bool any_beside = std::ranges::any_of(node.in, [&](uint32_t l) { return is_beside[l]; }) || std::ranges::any_of(node.out, [&](uint32_t l) { return is_beside[l]; });
		if (any_beside && ends.empty()) return unsupported(fmt::format("corridor: every line at node {} is a beside track", n));
		for (size_t k = 1; k < ends.size(); k++) {
			for (int axis = 0; axis < 2; axis++) eq({{ends[k][axis], 1}, {ends[0][axis], -1}}, 0, fmt::format("node {}", n));
		}
		for (const Beside &beside : besides) {
			const std::string &name = map.tracks[beside.track].name;
			if (std::ranges::find(node.out, beside.track) != node.out.end()) {
				for (int axis = 0; axis < 2; axis++) eq({{point[beside.track].front()[axis], 1}, {ends[0][axis], -1}}, beside.opening[axis], fmt::format("corridor {} open", name));
			}
			if (std::ranges::find(node.in, beside.track) != node.in.end()) {
				for (int axis = 0; axis < 2; axis++) eq({{ends[0][axis], 1}, {point[beside.track].back()[axis], -1}}, beside.closing[axis], fmt::format("corridor {} close", name));
			}
		}
	}

	/* Corridors: each interior vertex of a beside track keeps one lane (the miter) from its parent chain's vertex, for both headings meeting there. */
	for (const Beside &beside : besides) {
		const uint32_t b = beside.track;
		std::vector<std::array<uint32_t, 2>> chain_point;
		std::vector<int> joined;
		for (const uint32_t t : beside.chain) {
			const std::vector<int> &tp = v.tracks[t].pieces;
			if (!chain_point.empty() && !joined.empty() && !tp.empty() && ((joined.back() - tp.front()) % 8 + 8) % 8 == 0) chain_point.pop_back();
			chain_point.insert(chain_point.end(), point[t].begin() + (chain_point.empty() ? 0 : 1), point[t].end());
			joined.insert(joined.end(), tp.begin(), tp.end());
		}
		const std::vector<int> &pieces = v.tracks[b].pieces;
		for (size_t k = 1; k < pieces.size(); k++) {
			std::set<int> headings = {((pieces[k - 1] % 8) + 8) % 8, ((pieces[k] % 8) + 8) % 8};
			for (const int h : headings) {
				const std::array<int, 2> normal = PlanarModel::STEP[(h + 2) % 8];
				Terms terms;
				for (int axis = 0; axis < 2; axis++) {
					terms.emplace_back(point[b][k][axis], normal[axis]);
					terms.emplace_back(chain_point[k][axis], -normal[axis]);
				}
				eq(terms, 2 * beside.side, fmt::format("corridor {} {}", map.tracks[b].name, k));
			}
		}
	}

	/* Sibling lanes (D388 spine, SPEC-slots-lengths §11): per used sibling pair, one lane between the two tracks at every vertex of the range between their first and last channel crossing, and at its ends. */
	struct LanePair {
		uint32_t a, b;
		uint32_t pa0, pa1, pb0, pb1;
	};
	std::vector<LanePair> lane_pairs;
	if (options.sibling_lanes) {
		const auto mod8 = [](int h) { return ((h % 8) + 8) % 8; };
		for (const auto &[c, a, b, fa, fb, same] : PlanarSpinePairs(map, v.structure, true)) {
			if (!same) continue;
			const std::string pair_name = fmt::format("{} {}", map.tracks[a].name, map.tracks[b].name);
			const auto skip = [&](const char *why) { result.lane_skipped.push_back(fmt::format("{}: {}", pair_name, why)); };
			if (fa.size() < 2) {
				skip("one channel crossing");
				continue;
			}
			if (fa.back() - fa.front() + 1 != fa.size() || fb.back() - fb.front() + 1 != fb.size()) {
				skip("other events inside");
				continue;
			}
			int s = 0;
			bool no_side = false, differ = false;
			for (size_t j = 1; j < fa.size(); j++) {
				const int side = map.Side(b, fb.front() + j, a, fa.front() + j);
				if (side == 0) no_side = true;
				if (j == 1) s = side;
				else if (side != s) differ = true;
			}
			if (no_side) {
				skip("no side");
				continue;
			}
			if (differ) {
				skip("sides differ");
				continue;
			}
			const std::vector<uint32_t> &ea = v.tracks[a].event_piece, &eb = v.tracks[b].event_piece;
			const uint32_t pa0 = ea[fa.front()], pa1 = ea[fa.back()], pb0 = eb[fb.front()], pb1 = eb[fb.back()];
			bool pieces_same = pa1 - pa0 == pb1 - pb0;
			for (uint32_t i = 0; pieces_same && i <= pa1 - pa0; i++) {
				pieces_same = mod8(v.tracks[a].pieces[pa0 + i]) == mod8(v.tracks[b].pieces[pb0 + i]) && (std::ranges::find(v.tracks[a].flex, pa0 + i) != v.tracks[a].flex.end()) == (std::ranges::find(v.tracks[b].flex, pb0 + i) != v.tracks[b].flex.end());
			}
			if (!pieces_same) {
				skip("pieces differ");
				continue;
			}
			lane_pairs.push_back({a, b, pa0, pa1, pb0, pb1});
			result.lane_pairs++;
			const auto lane_row = [&](uint32_t ka, uint32_t kb, int heading, uint32_t index) {
				const std::array<int, 2> normal = PlanarModel::STEP[(mod8(heading) + 2) % 8];
				Terms terms;
				for (int axis = 0; axis < 2; axis++) {
					terms.emplace_back(point[b][kb][axis], normal[axis]);
					terms.emplace_back(point[a][ka][axis], -normal[axis]);
				}
				eq(terms, 2 * s, fmt::format("lane {} {} {}", map.tracks[a].name, map.tracks[b].name, index));
			};
			const std::vector<int> &pieces = v.tracks[a].pieces;
			lane_row(pa0, pb0, pieces[pa0], 0);
			for (uint32_t i = 1; i <= pa1 - pa0; i++) {
				for (const int h : std::set<int>{mod8(pieces[pa0 + i - 1]), mod8(pieces[pa0 + i])}) lane_row(pa0 + i, pb0 + i, h, i);
			}
			lane_row(pa1 + 1, pb1 + 1, pieces[pa1], pa1 - pa0 + 1);
		}
	}

	/* Crossings: per crossing the parameter along each of its two pieces. */
	const size_t nc = map.crossings.size();
	std::vector<std::array<uint32_t, 2>> param(nc);
	std::vector<std::array<uint32_t, 2>> piece_of(nc);
	std::vector<std::array<bool, 2>> is_separator(nc, {false, false}); // Per side, whether its track is the separator of a structure holding the crossing (D414).
	std::vector<bool> in_structure(nc, false);
	for (const PlanarStructure &structure : v.structure.structures) {
		for (const uint32_t c : structure.crossings) {
			if (c >= nc) continue;
			in_structure[c] = true;
			if (map.crossings[c].a == structure.separator) is_separator[c][0] = true;
			if (map.crossings[c].b == structure.separator) is_separator[c][1] = true;
		}
	}
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
			/* A structure's crosser may cross at its piece's ends (D414); the separator and every other crossing keep rules.cross. */
			const bool crosser = in_structure[c] && !is_separator[c][side];
			row({{param[c][side], 1}, {len.variable, -len.coefficient}}, -JunctionMilp::INF, crosser ? 0 : -rules.cross, tag);
			if (!crosser) row({{param[c][side], 1}}, rules.cross, JunctionMilp::INF, tag);
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
	/* Rows that two route paths share are written once. */
	std::set<std::pair<Terms, double>> seen;
	const auto once_row = [&](Terms terms, double lower, const std::string &tag) {
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
		if (const auto soft = soft_price.find(tag); soft != soft_price.end() && soft->second <= 0) return;
		if (seen.emplace(merged, lower).second) row(std::move(merged), lower, JunctionMilp::INF, tag);
	};
	/* Bays (D420): a non-prio route path's travel from its last topology split or port to each topology merge is at least rules.bay. */
	if (options.bays) {
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
		const auto topology_node = [&](size_t n) {
			const PlanarNode &node = map.expanded.nodes[n];
			for (const uint32_t t : node.in) if (map.part_of[t] == UINT32_MAX) return false;
			for (const uint32_t t : node.out) if (map.part_of[t] == UINT32_MAX) return false;
			return true;
		};
		for (const PlanarRoute &route : map.expanded.routes) {
			if (route.prio) continue;
			for (const std::vector<uint32_t> &path : route.paths) {
				Terms acc;
				Terms from_split;
				for (const uint32_t t : path) {
					for (const Length &len : length[t]) acc.emplace_back(len.variable, len.coefficient);
					const PlanarEnd &to = map.expanded.lines[t].to;
					if (to.port || !topology_node(to.index)) continue;
					if (is_split(to.index)) {
						from_split = acc;
					} else {
						Terms d = acc;
						for (const auto &[variable, coefficient] : from_split) d.emplace_back(variable, -coefficient);
						once_row(std::move(d), rules.bay, fmt::format("bay {} {}", route.name, to.index));
					}
				}
			}
		}
	}
	/* Curve spacing (D420, SPEC-slots-lengths §8.2): Shape's bends along every route path; flex and step middles count as travel only (§10.3). */
	if (options.curves) {
		struct Bend {
			Terms at;
			double constant = 0;
			bool right;
			std::string flex;
			std::string where;
		};
		const auto committed = [&](uint32_t t, size_t i) {
			return options.curve_flex_from != nullptr && std::ranges::find(v.tracks[t].flex, static_cast<uint32_t>(i)) != v.tracks[t].flex.end() && t < options.curve_flex_from->size() && i < (*options.curve_flex_from)[t].size() && (*options.curve_flex_from)[t][i] >= options.curve_flex_above;
		};
		for (const PlanarRoute &route : map.expanded.routes) {
			for (const std::vector<uint32_t> &path : route.paths) {
				std::vector<Bend> bends;
				Terms acc;
				double constant = 0;
				int last_heading = -1;
				std::string why;
				std::string last_flex; // The committed insert the previous piece was, if any.
				const auto walk = [&](int heading, const std::string &flex_tag, const std::string &where) -> bool {
					if (last_heading != -1 && last_heading != heading) {
						const int turn = PlanarModel::Rot(last_heading, heading);
						if (std::abs(turn) != 1) {
							why = fmt::format("route {} turns by {} eighths at {}", route.name, turn, where);
							return false;
						}
						bends.push_back({acc, constant, turn > 0, flex_tag.empty() ? last_flex : flex_tag, where});
					}
					last_heading = heading;
					last_flex = flex_tag;
					return true;
				};
				for (const uint32_t t : path) {
					const std::vector<uint32_t> &flex = v.tracks[t].flex;
					const std::vector<uint32_t> &steps = v.tracks[t].steps;
					const std::vector<int> &pieces = v.tracks[t].pieces;
					const Beside *beside = nullptr;
					for (const Beside &b : besides) {
						if (b.track == t) beside = &b;
					}
					if (beside != nullptr) {
						if (!walk((((pieces.front() + beside->side) % 8) + 8) % 8, "", map.tracks[t].name)) return unsupported(why);
						constant += 2;
					}
					for (size_t i = 0; i < pieces.size(); i++) {
						const Length &len = length[t][i];
						const bool soft_insert = options.curve_flex_soft != nullptr && std::ranges::find(flex, static_cast<uint32_t>(i)) != flex.end() && t < options.curve_flex_soft->size() && i < (*options.curve_flex_soft)[t].size();
						const bool real = committed(t, i) || soft_insert;
						if (soft_insert && !committed(t, i)) soft_price[fmt::format("curve flex {} {}", map.tracks[t].name, i)] = (*options.curve_flex_soft)[t][i];
						if (real || (std::ranges::find(flex, i) == flex.end() && std::ranges::find(steps, i) == steps.end())) {
							/* Lifted headings may differ by whole turns from one track to the next. */
							if (!walk(((pieces[i] % 8) + 8) % 8, real ? fmt::format("curve flex {} {}", map.tracks[t].name, i) : "", fmt::format("track {}#{}", map.tracks[t].name, i))) return unsupported(why);
						}
						acc.emplace_back(len.variable, len.coefficient);
					}
					if (beside != nullptr) {
						if (!walk((((pieces.back() - beside->side) % 8) + 8) % 8, "", map.tracks[t].name)) return unsupported(why);
						constant += 2;
					}
				}
				for (size_t i = 0; i < bends.size(); i++) {
					int mand = 0;
					for (size_t j = i + 1; j < bends.size(); j++) {
						if (mand >= 3) break;
						Terms d = bends[j].at;
						for (const auto &[variable, coefficient] : bends[i].at) d.emplace_back(variable, -coefficient);
						const double least = mand >= 1 ? rules.curve_window[mand - 1] : (bends[i].right != bends[j].right ? rules.curve_opposite : rules.curve_same);
						const std::string &flex_tag = !bends[i].flex.empty() ? bends[i].flex : bends[j].flex;
						once_row(std::move(d), least - (bends[j].constant - bends[i].constant), flex_tag.empty() ? fmt::format("curve {} {} {}", route.name, bends[i].where, bends[j].where) : flex_tag);
						mand++;
					}
				}
			}
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
		std::vector<int> crosser_heading; // The crosser's piece heading at each crossing, 0..7.
		uint32_t piece = UINT32_MAX;
		for (const uint32_t c : structure.crossings) {
			if (c >= nc) return unsupported(fmt::format("structure {} names crossing {}", s, c));
			const int side = map.crossings[c].a == structure.separator ? 0 : 1;
			if (piece != UINT32_MAX && piece != piece_of[c][side]) return unsupported(fmt::format("structure {}'s crossings sit on different pieces of its separator", s));
			piece = piece_of[c][side];
			u.push_back(param[c][side]);
			const uint32_t crosser = map.crossings[c].a == structure.separator ? map.crossings[c].b : map.crossings[c].a;
			crosser_heading.push_back(((v.tracks[crosser].pieces[piece_of[c][1 - side]] % 8) + 8) % 8);
		}
		int least_run = -1;
		int most_limit = -1;
		for (int kind = 0; kind < 2; kind++) {
			if (structure.limit[kind] < 0) continue;
			least_run = least_run < 0 ? shape.end_run[kind] : std::min(least_run, shape.end_run[kind]);
			most_limit = std::max(most_limit, structure.limit[kind]);
		}
		/* Parallel crossers keep the deck gap; an axis and a diagonal crosser may be one half tile apart (D415). */
		for (size_t k = 0; k + 1 < u.size(); k++) row({{u[k + 1], 1}, {u[k], -1}}, crosser_heading[k] % 4 == crosser_heading[k + 1] % 4 ? rules.deck_gap : 1, JunctionMilp::INF, fmt::format("deck {}", s));
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
		export_big_m = M;
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
			if (convex && !options.pairwise_all_faces) {
				if (!clear_walls(w, f.face)) return unsupported(fmt::format("cell {} names a wall the variant lacks", f.face));
				continue;
			}
			for (size_t i = 0; i < n; i++) {
				for (size_t j = i + 1; j < n; j++) {
					if (j == i + 1 || (i == 0 && j == n - 1)) continue;
					if (w[i].track >= nt || w[j].track >= nt) continue;
					if (is_span(w[i]) || is_span(w[j])) continue;
					if (options.sibling_lanes) {
						bool in_lane = false;
						for (const LanePair &lp : lane_pairs) {
							const auto inside = [&](const PlanarCellWall &x, const PlanarCellWall &y) { return x.track == lp.a && y.track == lp.b && x.piece >= lp.pa0 && x.piece <= lp.pa1 && y.piece >= lp.pb0 && y.piece <= lp.pb1; };
							in_lane = in_lane || inside(w[i], w[j]) || inside(w[j], w[i]);
						}
						if (in_lane) {
							result.lane_sep_skipped++;
							continue;
						}
					}
					if (w[i].track == w[j].track && w[i].piece == w[j].piece) continue;
					if (w[i].piece >= v.tracks[w[i].track].pieces.size() || w[j].piece >= v.tracks[w[j].track].pieces.size()) return unsupported(fmt::format("face {} names a wall the variant lacks", f.face));
						/* Zero contraction: every may-be-zero piece is a point, so a wall touches the pieces of its track from lo - 1 to hi + 1, the nodes its track reaches through such pieces, and the crossings on pieces from lo to hi; two walls that touch are kept apart by no row. */
						struct Touch {
							uint32_t lo, hi;
							std::vector<uint32_t> nodes;
							std::vector<size_t> events;
						};
						const auto touch = [&](const PlanarCellWall &wall) {
							Touch r{wall.piece, wall.piece, {}, {}};
							const size_t last = v.tracks[wall.track].pieces.size() - 1;
							while (r.lo > 0 && may_zero[wall.track][r.lo - 1]) r.lo--;
							while (r.hi < last && may_zero[wall.track][r.hi + 1]) r.hi++;
							const auto &line = map.expanded.lines[wall.track];
							if (r.lo == 0 && !line.from.port) r.nodes.push_back(line.from.index);
							if (r.hi == last && !line.to.port) r.nodes.push_back(line.to.index);
							for (size_t k = 0; k < std::min(map.events[wall.track].size(), v.tracks[wall.track].event_piece.size()); k++) {
								const uint32_t e = v.tracks[wall.track].event_piece[k];
								if (e >= r.lo && e <= r.hi) r.events.push_back(k);
							}
							return r;
						};
						{
							const Touch ti = touch(w[i]), tj = touch(w[j]);
							bool touching = false;
							if (w[i].track == w[j].track) touching = w[j].piece + 1 >= ti.lo && w[j].piece <= ti.hi + 1;
							for (const uint32_t a : ti.nodes) {
								for (const uint32_t b : tj.nodes) touching = touching || (a == b);
							}
							for (const size_t a : ti.events) {
								for (const size_t b : tj.events) touching = touching || (map.events[w[i].track][a] == map.events[w[j].track][b]);
							}
							if (touching) continue;
						}
					const std::array<std::array<uint32_t, 2>, 2> pi ={point[w[i].track][w[i].piece], point[w[i].track][w[i].piece + 1]};
					const std::array<std::array<uint32_t, 2>, 2> pj = {point[w[j].track][w[j].piece], point[w[j].track][w[j].piece + 1]};
					bool shared = false;
					for (const auto &p : pi) {
						for (const auto &q : pj) shared = shared || find(p[0]) == find(q[0]);
					}
					if (shared) continue;
					if (options.pairwise_flex_only) {
						const auto in_flex_group = [&](const PlanarCellWall &wall) {
							for (const uint32_t m : v.tracks[wall.track].flex) {
								if (wall.piece + 1 >= m && wall.piece <= m + 1) return true;
							}
							return false;
						};
						if (!in_flex_group(w[i]) && !in_flex_group(w[j])) continue;
					}
					result.pairs++;
					struct Axis {
						std::array<int, 2> s;
						int spacing;
					};
					std::vector<Axis> axes = {{PlanarModel::STEP[(w[i].heading + 6) % 8], PlanarModel::RungSpacing(w[i].heading, w[j].heading) + options.sides_pad}};
					if (w[i].heading % 4 != w[j].heading % 4) {
						axes.push_back({PlanarModel::STEP[(w[j].heading + 6) % 8], PlanarModel::RungSpacing(w[j].heading, w[i].heading) + options.sides_pad});
					} else {
						axes.push_back({PlanarModel::STEP[w[i].heading % 8], 2 + options.sides_pad});
					}
					int forced_axis = -1, forced_side = 0;
					const auto monotone = [&](size_t from, size_t to, const std::array<int, 2> &normal) {
						for (size_t k = (from + 1) % n;; k = (k + 1) % n) {
							const std::array<int, 2> step = PlanarModel::STEP[w[k].heading % 8];
							if (step[0] * normal[0] + step[1] * normal[1] < 0) return false;
							if (k == to) return true;
						}
					};
					/* D426 probe: 0 not facing, 1 facing with anti-parallel walls, 2 facing at another angle. */
					int facing_kind = 0;
					if (options.facing_sides && options.sides_from != nullptr && w[i].heading % 8 != w[j].heading % 8 && monotone(i, j, axes[0].s) && monotone(j, i, PlanarModel::STEP[(w[j].heading + 6) % 8])) {
						facing_kind = (((w[j].heading - w[i].heading) % 8) + 8) % 8 == 4 ? 1 : 2;
					}
					if (options.sides_by_walk && facing_kind == 0) {
						/* D417: the guaranteed projection of the walls strictly between, each at its least length, onto the normal. */
						const auto guaranteed = [&](size_t from, size_t to, const std::array<int, 2> &normal) {
							int sum = 0;
							for (size_t k = (from + 1) % n; k != to; k = (k + 1) % n) {
								if (w[k].track >= nt || w[k].piece >= may_zero[w[k].track].size() || may_zero[w[k].track][w[k].piece]) continue;
								const std::array<int, 2> step = PlanarModel::STEP[w[k].heading % 8];
								sum += static_cast<int>(length[w[k].track][w[k].piece].coefficient) * (step[0] * normal[0] + step[1] * normal[1]);
							}
							return sum;
						};
						const std::array<int, 2> nj = PlanarModel::STEP[(w[j].heading + 6) % 8];
						if (monotone(i, j, axes[0].s) && (!options.walk_strict || guaranteed(i, j, axes[0].s) >= axes[0].spacing)) {
							forced_axis = 0;
							forced_side = 0;
						} else if (w[i].heading % 4 != w[j].heading % 4) {
							if (monotone(j, i, nj) && (!options.walk_strict || guaranteed(j, i, nj) >= axes[1].spacing)) {
								forced_axis = 1;
								forced_side = 1;
							}
						} else if (monotone(j, i, nj) && (!options.walk_strict || guaranteed(j, i, nj) >= axes[0].spacing)) {
							forced_axis = 0;
							forced_side = w[i].heading % 8 == w[j].heading % 8 ? 1 : 0;
						}
					}
					if (options.sides_from == nullptr && forced_axis >= 0) {
						result.walk_fixed++;
						const Axis &axis = axes[forced_axis];
						for (const auto &pp : forced_side == 0 ? pi : pj) {
							for (const auto &q : forced_side == 0 ? pj : pi) {
								Terms t;
								for (int d = 0; d < 2; d++) {
									t.emplace_back(q[d], axis.s[d]);
									t.emplace_back(pp[d], -axis.s[d]);
								}
								row(std::move(t), axis.spacing, JunctionMilp::INF, fmt::format("sep {} {} {}", f.face, i, j));
							}
						}
						continue;
					}
					if (options.sides_from != nullptr) {
						const auto ref = [&](uint32_t track, uint32_t k) { return (*options.sides_from)[track][k]; };
						const std::array<std::array<uint32_t, 2>, 2> *best_from = nullptr, *best_to = nullptr;
						const Axis *best_axis = nullptr;
						int best_margin = 0;
						const std::array<std::array<uint32_t, 2>, 2> *const ends[2] = {&pi, &pj};
						const std::array<uint32_t, 2> tracks = {w[i].track, w[j].track};
						const std::array<uint32_t, 2> pieces = {w[i].piece, w[j].piece};
						int forced_margin = 0;
						bool have_forced = false;
						const std::array<std::array<uint32_t, 2>, 2> *forced_from = nullptr, *forced_to = nullptr;
						int axis_index = -1;
						const std::string pair_tag = fmt::format("sep {} {} {}", f.face, i, j);
						const bool want_margins = std::ranges::find(options.margin_tags, pair_tag) != options.margin_tags.end();
						std::string margin_text;
						int probe_candidate = -1;
						for (const auto &[forced_tag, forced_c] : options.forced_sides) {
							if (forced_tag == pair_tag) probe_candidate = forced_c;
						}
						const Axis *probe_axis = nullptr;
						const std::array<std::array<uint32_t, 2>, 2> *probe_from = nullptr, *probe_to = nullptr;
						for (const Axis &axis : axes) {
							axis_index++;
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
								if (want_margins) margin_text += fmt::format("{}c{} s({},{}) spacing {} from {} margin {}", margin_text.empty() ? "" : "; ", axis_index * 2 + side, axis.s[0], axis.s[1], axis.spacing, from == &pi ? "i" : "j", margin);
								if (axis_index * 2 + side == probe_candidate) {
									probe_axis = &axis;
									probe_from = from;
									probe_to = to;
								}
								if (axis_index == forced_axis && side == forced_side) {
									have_forced = true;
									forced_margin = margin;
									forced_from = from;
									forced_to = to;
								}
								const bool allowed = facing_kind == 0 || (facing_kind == 1 && axis_index == 0 && side == 0) || (facing_kind == 2 && axis_index == side);
								if (allowed && (best_axis == nullptr || margin > best_margin)) {
									best_axis = &axis;
									best_from = from;
									best_to = to;
									best_margin = margin;
								}
							}
						}
						if (want_margins) result.sep_margins.emplace_back(pair_tag, margin_text);
						if (have_forced) {
							result.walk_fixed++;
							if (forced_margin < 0) result.walk_short++;
							if (forced_margin < best_margin) result.walk_differs++;
							best_axis = &axes[forced_axis];
							best_from = forced_from;
							best_to = forced_to;
							best_margin = forced_margin;
						}
						/* sep_extremal: a piece's projection on s grows from start to end when its heading's step has a positive dot with s, so only from's highest and to's lowest endpoint can bind. */
						const auto along = [&](const std::array<std::array<uint32_t, 2>, 2> *ends_of, const Axis *axis) {
							const size_t side = ends_of == &pi ? 0 : 1;
							const std::array<int, 2> step = PlanarModel::STEP[((v.tracks[tracks[side]].pieces[pieces[side]] % 8) + 8) % 8];
							return step[0] * axis->s[0] + step[1] * axis->s[1];
						};
						if (probe_candidate >= 0) {
							if (probe_axis == nullptr) {
								result.sep_sides.emplace_back(pair_tag, "candidate out of range");
								continue;
							}
							result.sep_sides.emplace_back(pair_tag, fmt::format("s ({},{}) spacing {} from {}", probe_axis->s[0], probe_axis->s[1], probe_axis->spacing, probe_from == &pi ? "i" : "j"));
							const std::array<uint32_t, 2> &p = (*probe_from)[along(probe_from, probe_axis) > 0 ? 1 : 0];
							const std::array<uint32_t, 2> &q = (*probe_to)[along(probe_to, probe_axis) > 0 ? 0 : 1];
							Terms t;
							for (int d = 0; d < 2; d++) {
								t.emplace_back(q[d], probe_axis->s[d]);
								t.emplace_back(p[d], -probe_axis->s[d]);
							}
							row(std::move(t), probe_axis->spacing, JunctionMilp::INF, fmt::format("sephard {} {} {}", f.face, i, j));
							continue;
						}
						if (best_margin < options.sides_free_below || std::ranges::find(options.free_sides, fmt::format("sep {} {} {}", f.face, i, j)) != options.free_sides.end() || std::ranges::find(options.free_faces, f.face) != options.free_faces.end()) {
							result.sides_free++;
							Terms bs;
							for (const Axis &axis : axes) {
								for (int side = 0; side < 2; side++) {
									const auto &from = side == 0 ? pi : pj;
									const auto &to = side == 0 ? pj : pi;
									const uint32_t b = var(0, 1, 0, "other");
									result.binaries++;
									bs.emplace_back(b, 1);
									std::vector<std::array<uint32_t, 2>> from_ends(from.begin(), from.end());
									std::vector<std::array<uint32_t, 2>> to_ends(to.begin(), to.end());
									if (options.sep_extremal) {
										from_ends = {from[along(&from, &axis) > 0 ? 1 : 0]};
										to_ends = {to[along(&to, &axis) > 0 ? 0 : 1]};
									}
									for (const auto &p : from_ends) {
										for (const auto &q : to_ends) {
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
							continue;
						}
						if (best_margin < 0) result.short_sides++;
						std::vector<std::array<uint32_t, 2>> from_ends(best_from->begin(), best_from->end());
						std::vector<std::array<uint32_t, 2>> to_ends(best_to->begin(), best_to->end());
						if (facing_kind == 1) result.facing_fixed++;
						if (facing_kind == 2) result.facing_choice++;
						result.sep_sides.emplace_back(fmt::format("sep {} {} {}", f.face, i, j), fmt::format("s ({},{}) spacing {} from {}{}", best_axis->s[0], best_axis->s[1], best_axis->spacing, best_from == &pi ? "i" : "j", facing_kind == 1 ? " facing" : facing_kind == 2 ? " facing2" : ""));
						if (options.sep_extremal) {
							from_ends = {(*best_from)[along(best_from, best_axis) > 0 ? 1 : 0]};
							to_ends = {(*best_to)[along(best_to, best_axis) > 0 ? 0 : 1]};
						}
						for (const auto &p : from_ends) {
							for (const auto &q : to_ends) {
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

	/* Grid in whole tiles (D382 probe): the parities the integer equalities force, propagated from the grid's seeds (an integer equality with one odd-coefficient unknown fixes it); each forced variable becomes 2 h + p (h a new integer, linked by a `grid link` row), and every row over integers with integer coefficients is divided by their gcd with its bounds rounded inward. */
	if (options.grid && !options.elastic) {
		const auto whole = [](double c) { return std::abs(c - std::round(c)) < 1e-9; };
		const auto integer_row = [&](const Pending &p) {
			return std::ranges::all_of(p.terms, [&](const std::pair<uint32_t, double> &term) { return integral[term.first] && whole(term.second); });
		};
		std::map<uint32_t, int> parity = grid_seed;
		/* One pass: per integer equality, the parity of its right-hand side minus the known odd terms, and its odd-coefficient unknowns. */
		const auto scan = [&](const Pending &p, int &sum, uint32_t &unknown) {
			sum = static_cast<int>(((std::llround(p.lower) % 2) + 2) % 2);
			int unknowns = 0;
			for (const auto &[v, c] : p.terms) {
				if ((std::llabs(std::llround(c)) & 1) == 0) continue;
				const auto it = parity.find(v);
				if (it != parity.end()) {
					sum ^= it->second;
				} else {
					unknown = v;
					unknowns++;
				}
			}
			return unknowns;
		};
		const auto parity_row = [&](const Pending &p) { return p.lower == p.upper && std::isfinite(p.lower) && whole(p.lower) && integer_row(p); };
		for (bool changed = true; changed;) {
			changed = false;
			for (const Pending &p : pending) {
				if (!parity_row(p)) continue;
				int sum = 0;
				uint32_t unknown = UINT32_MAX;
				if (scan(p, sum, unknown) == 1) {
					parity[unknown] = sum;
					changed = true;
				}
			}
		}
		for (const Pending &p : pending) {
			int sum = 0;
			uint32_t unknown = UINT32_MAX;
			if (parity_row(p) && scan(p, sum, unknown) == 0 && sum != 0) result.grid_conflicts++;
		}
		std::map<uint32_t, uint32_t> half; ///< Forced variable -> its h.
		for (const auto &[v, p] : parity) {
			const double lower = std::isfinite(var_lower[v]) ? std::ceil((var_lower[v] - p) / 2) : -JunctionMilp::INF;
			const double upper = std::isfinite(var_upper[v]) ? std::floor((var_upper[v] - p) / 2) : JunctionMilp::INF;
			half[v] = add_variable(lower, upper, 0, true);
			columns++;
		}
		result.grid_forced = half.size();
		for (Pending &p : pending) {
			Terms terms;
			double shift = 0;
			for (const auto &[v, c] : p.terms) {
				const auto it = half.find(v);
				if (it == half.end()) {
					terms.emplace_back(v, c);
					continue;
				}
				terms.emplace_back(it->second, 2 * c);
				shift += c * parity.at(v);
			}
			p.terms = std::move(terms);
			p.lower -= shift;
			p.upper -= shift;
			if (!p.terms.empty() && integer_row(p)) {
				long long g = 0;
				for (const auto &[v, c] : p.terms) g = std::gcd(g, std::llabs(std::llround(c)));
				const double lower = std::isfinite(p.lower) ? std::ceil(p.lower / g - 1e-9) : p.lower;
				const double upper = std::isfinite(p.upper) ? std::floor(p.upper / g + 1e-9) : p.upper;
				if ((std::isfinite(p.lower) && lower * g > p.lower + 1e-9) || (std::isfinite(p.upper) && upper * g < p.upper - 1e-9)) result.grid_tightened++;
				for (auto &term : p.terms) term.second /= static_cast<double>(g);
				p.lower = lower;
				p.upper = upper;
			}
		}
		for (Pending &p : pending) emit(std::move(p.terms), p.lower, p.upper, p.tag);
		pending.clear();
		for (const auto &[v, p] : parity) emit({{v, 1}, {half.at(v), -2}}, p, p, "grid link");
	}

	result.rows = static_cast<uint32_t>(tags.size());
	result.columns = columns;
	if (exporting) {
		std::ofstream out(options.export_path);
		out << fmt::format("{{\"variables\": [{}], \"rows\": [{}], \"sense\": \"min\", \"big_m\": {}}}\n", export_variables, export_rows, json_number(export_big_m));
		out.close();
		if (!out) return unsupported("cannot write export");
	}
	milp.SetTimeLimit(seconds);
	if (options.start_lengths != nullptr || options.start_points != nullptr) {
		std::vector<std::pair<uint32_t, double>> start;
		for (size_t t = 0; t < nt; t++) {
			if (options.start_lengths != nullptr && t < options.start_lengths->size() && (*options.start_lengths)[t].size() == length[t].size()) {
				for (size_t i = 0; i < length[t].size(); i++) {
					const double value = (*options.start_lengths)[t][i] / length[t][i].coefficient;
					if (std::abs(value - std::round(value)) < 1e-6) start.emplace_back(length[t][i].variable, std::round(value));
				}
			}
			if (options.start_points != nullptr && t < options.start_points->size() && (*options.start_points)[t].size() == point[t].size()) {
				for (size_t i = 0; i < point[t].size(); i++) {
					for (int axis = 0; axis < 2; axis++) start.emplace_back(point[t][i][axis], (*options.start_points)[t][i][axis]);
				}
			}
		}
		result.start_values = start.size();
		milp.SetStart(std::move(start));
	}
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
		result.lengths.resize(nt);
		for (size_t t = 0; t < nt; t++) {
			for (const Length &len : length[t]) result.lengths[t].push_back(milp.Value(len.variable) * len.coefficient);
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

PlanarDrawPipelineResult DrawPlanarPipeline(const PlanarShapeVariant &v, const PlanarDrawPipelineOptions &options)
{
	PlanarDrawPipelineResult out;

	/* One log line: the stage, the round, the status, the slacks, the drawing's box and its total length. */
	const auto log_round = [&](const std::string &stage, int k, const PlanarDrawResult &r) {
		out.rounds.push_back(r);
		double curve_slack = 0;
		for (const auto &[row, slack] : r.stretched) curve_slack += row.starts_with("curve") ? slack : 0;
		long total = 0;
		int x0 = INT_MAX, x1 = INT_MIN, y0 = INT_MAX, y1 = INT_MIN;
		for (const auto &track : r.points) {
			for (size_t i = 0; i < track.size(); i++) {
				x0 = std::min(x0, track[i][0]); x1 = std::max(x1, track[i][0]);
				y0 = std::min(y0, track[i][1]); y1 = std::max(y1, track[i][1]);
				if (i > 0) total += std::max(std::abs(track[i][0] - track[i - 1][0]), std::abs(track[i][1] - track[i - 1][1]));
			}
		}
		out.log.push_back(fmt::format("{} {}: {}, total slack {:.3f}, curve slack {:.3f}, box {} x {}, length {}", stage, k, r.status, r.slack, curve_slack, x0 > x1 ? 0 : x1 - x0, x0 > x1 ? 0 : y1 - y0, total));
	};

	/* The seed. */
	PlanarDrawResult previous = CheckPlanarDrawable(v, options.round_seconds, {.clearance = options.keep_apart, .corridor = options.corridor, .sibling_lanes = options.sibling_lanes, .facing_sides = options.facing_sides, .elastic = true, .elastic_tidy = 0.001, .slack_price = options.discovery_slack_price, .pins = options.pins});
	out.final = previous;
	if (!previous.feasible) {
		out.stopped = "seed";
		return out;
	}
	std::vector<std::vector<std::array<int, 2>>> last_input = previous.points; // The input drawing of the last Optimal round.
	std::vector<std::vector<double>> last_input_lengths = previous.lengths; // Its lengths.

	/* Discovery. */
	std::vector<std::vector<double>> weights;
	PlanarDrawResult last = previous;
	std::vector<std::vector<double>> memory, soft; // Scheme A (curve_discovery): per insert the damped memory of its length and its slack price.
	const int rounds = options.curve_discovery ? std::min(options.discovery_rounds, 5) : options.discovery_rounds;
	for (int k = 1; k <= rounds; k++) {
		const bool previous_zero = k > 1 && previous.slack < 1e-6;
		PlanarDrawResult r = CheckPlanarDrawable(v, options.round_seconds, {.clearance = options.keep_apart, .clear_rows = true, .pairwise = true, .pairwise_all_faces = true, .sep_extremal = true, .node_gaps = options.node_gaps, .node_gap_from = &previous.lengths, .corridor = options.corridor, .sibling_lanes = options.sibling_lanes, .facing_sides = options.facing_sides, .flex_weights = k == 1 ? nullptr : &weights, .box_price = options.box_price, .flex_price = k == 1 ? options.first_flex_price : 0, .elastic = true, .elastic_tidy = 0.001, .slack_price = options.discovery_slack_price, .elastic_families = {"sep"}, .curve_flex_soft = options.curve_discovery && k >= 2 ? &soft : nullptr, .pins = options.pins, .sides_from = &previous.points});
		log_round("discovery", k, r);
		if (r.status != "Optimal") {
			out.final = r;
			out.stopped = "discovery";
			return out;
		}
		last_input = previous.points;
		last_input_lengths = previous.lengths;
		weights.assign(v.tracks.size(), {});
		for (size_t t = 0; t < v.tracks.size() && t < r.lengths.size(); t++) {
			weights[t].assign(r.lengths[t].size(), 0.0);
			for (size_t i = 0; i < r.lengths[t].size(); i++) weights[t][i] = options.irl1 / (r.lengths[t][i] + 0.5);
		}
		if (options.curve_discovery) {
			memory.resize(r.lengths.size());
			soft.resize(r.lengths.size());
			for (size_t t = 0; t < r.lengths.size(); t++) {
				memory[t].resize(r.lengths[t].size(), 0.0);
				soft[t].assign(r.lengths[t].size(), 0.0);
				for (size_t i = 0; i < r.lengths[t].size(); i++) {
					memory[t][i] = std::max(r.lengths[t][i], 0.5 * memory[t][i]);
					soft[t][i] = options.discovery_slack_price * std::min(1.0, memory[t][i] / 1.0);
				}
			}
		}
		const bool zero_twice = r.slack < 1e-6 && previous_zero;
		previous = r;
		last = std::move(r);
		if (zero_twice) break;
	}

	/* Commit. */
	out.committed = last.lengths;
	const auto count_committed = [&]() {
		size_t n = 0;
		for (size_t t = 0; t < v.tracks.size() && t < out.committed.size(); t++) {
			for (const uint32_t m : v.tracks[t].flex) n += m < out.committed[t].size() && out.committed[t][m] >= options.commit_above ? 1 : 0;
		}
		return n;
	};
	out.committed_count = count_committed();
	/* At most commit_rounds rounds from `from` with the commitment; the last round's result goes to `last`. False when a round is not Optimal. */
	const auto commit_rounds = [&](const PlanarDrawResult &from, const std::string &stage) {
		PlanarDrawResult prev = from;
		for (int k = 1; k <= options.commit_rounds; k++) {
			PlanarDrawResult r = CheckPlanarDrawable(v, options.round_seconds, {.clearance = options.keep_apart, .clear_rows = true, .pairwise = true, .pairwise_all_faces = true, .sep_extremal = true, .node_gaps = options.node_gaps, .node_gap_from = &prev.lengths, .corridor = options.corridor, .sibling_lanes = options.sibling_lanes, .facing_sides = options.facing_sides, .curve_flex_from = &out.committed, .curve_flex_above = options.commit_above, .box_price = options.box_price, .elastic = true, .elastic_tidy = 0.001, .slack_price = options.slack_price, .elastic_families = {"sep", "curve"}, .pins = options.pins, .sides_from = &prev.points});
			log_round(stage, k, r);
			if (r.status != "Optimal") {
				out.final = r;
				return false;
			}
			last_input = prev.points;
			last_input_lengths = prev.lengths;
			const bool done = r.slack < 1e-6 || (k > 1 && prev.slack - r.slack <= 0.5);
			prev = r;
			last = std::move(r);
			if (done) break;
		}
		return true;
	};
	if (!commit_rounds(last, "commit")) {
		out.stopped = "commit";
		return out;
	}

	/* Prune: a committed insert whose curve rows still stretch is held at 0, then the rounds again; at most `prunes` times, stopping when none is held. */
	const auto prune_all = [&]() -> bool {
	for (int p = 1; p <= options.prunes; p++) {
		size_t held = 0;
		const auto before_prune = out.committed;
		const auto last_before_prune = last;
		const auto last_input_before_prune = last_input;
		const auto last_input_lengths_before_prune = last_input_lengths;
		for (const auto &[row, slack] : last.stretched) {
			if (!row.starts_with("curve flex ")) continue;
			std::istringstream rs(row.substr(11));
			std::string name;
			uint32_t piece = 0;
			rs >> name >> piece;
			for (size_t t = 0; t < v.tracks.size(); t++) {
				if (v.structure.map.tracks[t].name != name || piece >= out.committed[t].size()) continue;
				out.committed[t][piece] = 0;
				held++;
			}
		}
		if (held == 0) break;
		out.pruned += held;
		const PlanarDrawResult committed_last = last;
		if (!commit_rounds(committed_last, p == 1 ? "prune" : fmt::format("prune{}", p))) {
			/* A prune whose rounds are not Optimal is undone: its held inserts come back and the pipeline goes on from the drawing before it. */
			out.committed = before_prune;
			out.pruned -= held;
			last = last_before_prune;
			last_input = last_input_before_prune;
			last_input_lengths = last_input_lengths_before_prune;
			out.log.push_back(fmt::format("prune {} undone: {} held inserts restored", p, held));
			break;
		}
	}
	return true;
	};
	if (!prune_all()) return out;

	/* Additions (a probe): the committed inserts stay, the others are free under IRL1; the inserts the pass uses are committed, then the commit rounds and the prunes again. */
	if (options.additions) {
		std::vector<std::vector<double>> add_weights(v.tracks.size());
		for (size_t t = 0; t < v.tracks.size() && t < last.lengths.size(); t++) {
			add_weights[t].assign(last.lengths[t].size(), 0.0);
			for (size_t i = 0; i < last.lengths[t].size(); i++) add_weights[t][i] = options.irl1 / (last.lengths[t][i] + 0.5);
		}
		PlanarDrawResult prev = last;
		for (int k = 1; k <= options.discovery_rounds; k++) {
			const bool previous_zero = k > 1 && prev.slack < 1e-6;
			PlanarDrawResult r = CheckPlanarDrawable(v, options.round_seconds, {.clearance = options.keep_apart, .clear_rows = true, .pairwise = true, .pairwise_all_faces = true, .sep_extremal = true, .node_gaps = options.node_gaps, .node_gap_from = &prev.lengths, .corridor = options.corridor, .sibling_lanes = options.sibling_lanes, .facing_sides = options.facing_sides, .curve_flex_from = &out.committed, .curve_flex_above = options.commit_above, .curve_flex_free = true, .flex_weights = &add_weights, .box_price = options.box_price, .elastic = true, .elastic_tidy = 0.001, .slack_price = options.slack_price, .elastic_families = {"sep", "curve"}, .pins = options.pins, .sides_from = &prev.points});
			log_round("additions", k, r);
			if (r.status != "Optimal") {
				out.final = r;
				out.stopped = "additions";
				return out;
			}
			last_input = prev.points;
			last_input_lengths = prev.lengths;
			for (size_t t = 0; t < v.tracks.size() && t < r.lengths.size(); t++) {
				for (size_t i = 0; i < r.lengths[t].size() && i < add_weights[t].size(); i++) add_weights[t][i] = options.irl1 / (r.lengths[t][i] + 0.5);
			}
			const bool zero_twice = r.slack < 1e-6 && previous_zero;
			prev = r;
			last = std::move(r);
			if (zero_twice) break;
		}
		size_t added = 0;
		for (size_t t = 0; t < v.tracks.size() && t < out.committed.size() && t < last.lengths.size(); t++) {
			for (const uint32_t m : v.tracks[t].flex) {
				if (m >= out.committed[t].size() || m >= last.lengths[t].size() || out.committed[t][m] >= options.commit_above || last.lengths[t][m] < options.commit_above) continue;
				out.committed[t][m] = last.lengths[t][m];
				added++;
			}
		}
		out.added = added;
		if (!commit_rounds(last, "add commit")) {
			out.stopped = "add commit";
			return out;
		}
		if (!prune_all()) return out;
	}

	if (options.free_stretched_sides) {
		std::vector<std::string> tags;
		for (const auto &[row, slack] : last.stretched) {
			if (row.starts_with("sep ")) tags.push_back(row);
		}
		PlanarDrawResult r = CheckPlanarDrawable(v, 15.0, {.clearance = options.keep_apart, .clear_rows = true, .pairwise = true, .pairwise_all_faces = true, .sep_extremal = true, .node_gaps = options.node_gaps, .node_gap_from = &last.lengths, .corridor = options.corridor, .sibling_lanes = options.sibling_lanes, .facing_sides = options.facing_sides, .curve_flex_from = &out.committed, .curve_flex_above = options.commit_above, .box_price = options.box_price, .elastic = true, .elastic_integer = true, .elastic_tidy = 0.001, .slack_price = options.slack_price, .elastic_families = {"sep", "curve"}, .continuous = {"point", "axis", "diagonal", "param", "cross"}, .pins = options.pins, .sides_from = &last.points, .free_sides = tags});
		log_round("free sides", 1, r);
	}

	if (options.side_probe) {
		std::vector<std::string> tags;
		for (const auto &[row, slack] : last.stretched) {
			if (row.starts_with("sep ") && std::ranges::find(tags, row) == tags.end()) tags.push_back(row);
		}
		const auto commit_options = [&]() {
			return PlanarDrawOptions{.clearance = options.keep_apart, .clear_rows = true, .pairwise = true, .pairwise_all_faces = true, .sep_extremal = true, .node_gaps = options.node_gaps, .corridor = options.corridor, .sibling_lanes = options.sibling_lanes, .facing_sides = options.facing_sides, .curve_flex_from = &out.committed, .curve_flex_above = options.commit_above, .box_price = options.box_price, .elastic = true, .elastic_tidy = 0.001, .slack_price = options.slack_price, .elastic_families = {"sep", "curve"}, .pins = options.pins};
		};
		const auto conflict_line = [](const std::string &prefix, const PlanarDrawResult &r) {
			std::string text;
			for (size_t k = 0; k < r.conflict.size() && k < 40; k++) text += (k == 0 ? "" : " | ") + r.conflict[k];
			if (r.conflict.size() > 40) text += fmt::format(" (+{} more)", r.conflict.size() - 40);
			return fmt::format("{} conflict: {}", prefix, text);
		};
		PlanarDrawOptions margins = commit_options();
		margins.node_gap_from = &out.rounds[0].lengths;
		margins.sides_from = &out.rounds[0].points;
		margins.margin_tags = tags;
		const PlanarDrawResult seed_margins = CheckPlanarDrawable(v, 15.0, margins);
		for (const auto &[tag, text] : seed_margins.sep_margins) out.log.push_back(fmt::format("side probe seed margins {}: {}", tag, text));
		for (const std::string &tag : tags) {
			for (int c = 0; c < 4; c++) {
				PlanarDrawOptions probe = commit_options();
				probe.node_gap_from = &last_input_lengths;
				probe.sides_from = &last_input;
				probe.forced_sides = {{tag, c}};
				probe.margin_tags = {tag};
				const PlanarDrawResult r = CheckPlanarDrawable(v, 15.0, probe);
				double sep_slack = 0, curve_slack = 0;
				for (const auto &[row, slack] : r.stretched) {
					if (row.starts_with("sep")) sep_slack += slack;
					if (row.starts_with("curve")) curve_slack += slack;
				}
				if (c == 0) {
					for (const auto &[t, text] : r.sep_margins) out.log.push_back(fmt::format("side probe last margins {}: {}", t, text));
				}
				out.log.push_back(fmt::format("side probe {} c{}: {} slack {:.3f} sep {:.3f} curve {:.3f} ms {:.0f}", tag, c, r.status, r.slack, sep_slack, curve_slack, r.ms));
				if (r.status != "Optimal") out.log.push_back(conflict_line(fmt::format("side probe {} c{}", tag, c), r));
			}
		}
		for (const std::string &tag : tags) {
			for (int c = 0; c < 4; c++) {
				PlanarDrawOptions probe = commit_options();
				probe.curve_flex_from = nullptr;
				probe.node_gap_from = &last_input_lengths;
				probe.sides_from = &last_input;
				probe.forced_sides = {{tag, c}};
				probe.margin_tags = {tag};
				const PlanarDrawResult r = CheckPlanarDrawable(v, 15.0, probe);
				double sep_slack = 0, curve_slack = 0;
				for (const auto &[row, slack] : r.stretched) {
					if (row.starts_with("sep")) sep_slack += slack;
					if (row.starts_with("curve")) curve_slack += slack;
				}
				out.log.push_back(fmt::format("side probe free {} c{}: {} slack {:.3f} sep {:.3f} curve {:.3f} ms {:.0f}", tag, c, r.status, r.slack, sep_slack, curve_slack, r.ms));
				if (r.status != "Optimal") out.log.push_back(conflict_line(fmt::format("side probe free {} c{}", tag, c), r));
			}
		}
		PlanarDrawResult joint_free;
		for (const bool free_inserts : {false, true}) {
			PlanarDrawOptions joint = commit_options();
			if (free_inserts) joint.curve_flex_from = nullptr;
			joint.node_gap_from = &last_input_lengths;
			joint.sides_from = &last_input;
			joint.free_sides = tags;
			joint.elastic_integer = true;
			joint.continuous = {"point", "axis", "diagonal", "param", "cross"};
			const PlanarDrawResult r = CheckPlanarDrawable(v, 15.0, joint);
			if (free_inserts) joint_free = r;
			const char *mode = free_inserts ? "free" : "committed";
			double sep_slack = 0, curve_slack = 0;
			for (const auto &[row, slack] : r.stretched) {
				if (row.starts_with("sep")) sep_slack += slack;
				if (row.starts_with("curve")) curve_slack += slack;
			}
			out.log.push_back(fmt::format("side probe joint {}: {} slack {:.3f} sep {:.3f} curve {:.3f} binaries {} sides_free {} ms {:.0f}", mode, r.status, r.slack, sep_slack, curve_slack, r.binaries, r.sides_free, r.ms));
			for (const auto &[row, slack] : r.stretched) out.log.push_back(fmt::format("side probe joint {} stretched {} {:.3f}", mode, row, slack));
			if (r.status != "Optimal") out.log.push_back(conflict_line(fmt::format("side probe joint {}", mode), r));
		}
		{
			const auto run_hard = [&](const PlanarDrawResult &source, const char *label) {
				PlanarDrawOptions hard = commit_options();
				hard.curve_flex_from = nullptr;
				hard.node_gap_from = &source.lengths;
				hard.sides_from = &source.points;
				hard.elastic_families = {"curve"};
				const PlanarDrawResult h = CheckPlanarDrawable(v, 15.0, hard);
				out.log.push_back(fmt::format("side probe {}: {} slack {:.3f} ms {:.0f}", label, h.status, h.slack, h.ms));
				if (h.status != "Optimal") {
					const PlanarTrackMap &map = v.structure.map;
					const std::vector<PlanarFaceWalls> face_walls = PlanarShapeFaceWalls(v);
					const auto piece_text = [&](const PlanarCellWall &w) {
						if (w.track >= v.tracks.size()) return std::string("box");
						return fmt::format("{}#{} h{}", map.tracks[w.track].name, w.piece, ((v.tracks[w.track].pieces[w.piece] % 8) + 8) % 8);
					};
					for (const std::string &tag : h.conflict) {
						std::string line = fmt::format("side probe {} conflict {}", label, tag);
						std::istringstream words(tag);
						std::string family;
						uint32_t face = 0, i = 0, j = 0;
						words >> family;
						if (family == "sep" && (words >> face >> i >> j)) {
							for (const PlanarFaceWalls &f : face_walls) {
								if (f.face != face || i >= f.walls.size() || j >= f.walls.size()) continue;
								line += fmt::format(" = {} | {}", piece_text(f.walls[i]), piece_text(f.walls[j]));
							}
						}
						out.log.push_back(line);
					}
				}
			};
			/* Whole-face binaries cost 15 s per hub and found nothing. */
			if (std::getenv("PLANAR_PROBE_NO_FACES") == nullptr) {
				std::vector<uint32_t> faces;
				for (const std::string &tag : tags) {
					std::istringstream words(tag);
					std::string family;
					uint32_t face = 0;
					if (words >> family >> face && std::ranges::find(faces, face) == faces.end()) faces.push_back(face);
				}
				std::string face_list;
				for (const uint32_t face : faces) face_list += fmt::format(" {}", face);
				PlanarDrawOptions open = commit_options();
				open.curve_flex_from = nullptr;
				open.node_gap_from = &last_input_lengths;
				open.sides_from = &last_input;
				open.free_sides = tags;
				open.free_faces = faces;
				open.elastic_integer = true;
				open.continuous = {"point", "axis", "diagonal", "param", "cross"};
				const PlanarDrawResult r = CheckPlanarDrawable(v, 15.0, open);
				double sep_slack = 0, curve_slack = 0;
				for (const auto &[row, slack] : r.stretched) {
					if (row.starts_with("sep")) sep_slack += slack;
					if (row.starts_with("curve")) curve_slack += slack;
				}
				out.log.push_back(fmt::format("side probe faces free: {} slack {:.3f} sep {:.3f} curve {:.3f} binaries {} sides_free {} faces{} ms {:.0f}", r.status, r.slack, sep_slack, curve_slack, r.binaries, r.sides_free, face_list, r.ms));
				for (const auto &[row, slack] : r.stretched) out.log.push_back(fmt::format("side probe faces free stretched {} {:.3f}", row, slack));
				if (r.status == "Optimal") run_hard(r, "faces hard");
			}
			if (joint_free.status == "Optimal") run_hard(joint_free, "joint hard");
		}
	}

	/* Final: the integer drawing on the sides the last round proved. */
	PlanarDrawOptions final_options{.clearance = options.keep_apart, .clear_rows = true, .pairwise = true, .pairwise_all_faces = true, .sep_extremal = true, .node_gaps = options.node_gaps, .node_gap_from = &last.lengths, .corridor = options.corridor, .sibling_lanes = options.sibling_lanes, .facing_sides = options.facing_sides, .curve_flex_from = &out.committed, .curve_flex_above = options.commit_above, .tidy = options.final_tidy, .pins = options.pins, .sides_from = &last_input, .start_lengths = options.final_start ? &last.lengths : nullptr, .grid = options.final_grid};
	if (options.final_soft_sep) {
		final_options.elastic = true;
		final_options.elastic_integer = true;
		final_options.elastic_families = {"sep"};
		final_options.elastic_tidy = 0.01;
	}
	if (options.final_soft_all) {
		final_options.elastic = true;
		final_options.elastic_integer = true;
		final_options.elastic_families.clear();
		final_options.elastic_tidy = 0;
	}
	final_options.slack_price = options.slack_price;
	out.last_round = last;
	out.last_input = last_input;
	out.final = CheckPlanarDrawable(v, options.final_seconds, final_options);
	log_round("final", 1, out.final);
	if (out.final.status != "Optimal") out.stopped = "final";
	return out;
}
