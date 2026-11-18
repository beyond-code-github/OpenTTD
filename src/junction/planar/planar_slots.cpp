/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file planar_slots.cpp The slot layer: tracks per channel interval, and their slots from one integer program. */

#include "../../stdafx.h"

#include "planar_slots.h"

#include "../../core/format.hpp"
#include "../placement/junction_milp.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>
#include <optional>
#include <tuple>

#include "../../safeguards.h"

/** The index of the first merge site channel c enters or is straight or other of, as the composer's site_of(c, false). */
static std::optional<size_t> JoinOf(const PlanarChannels &channels, const std::vector<PlanarResolvedSite> &sites, uint32_t c)
{
	for (size_t s = 0; s < channels.sites.size(); s++) {
		if (channels.sites[s].kind != PlanarSiteKind::Fray && (sites[s].straight == c || sites[s].other == c)) return s;
	}
	return std::nullopt;
}

/** The minimum spacing of consecutive lanes in the last interval of channel c: 2 when a prio Fray splits it (composer Fray gap), else 1. */
static int LaneGap(const PlanarChannels &channels, const std::vector<PlanarResolvedSite> &sites, uint32_t c)
{
	for (size_t s = 0; s < channels.sites.size(); s++) {
		if (channels.sites[s].kind == PlanarSiteKind::Fray && channels.sites[s].channel == c && sites[s].prio) return 2;
	}
	return 1;
}

/** Cut each channel into its intervals and list each interval's tracks, left to right; slots are filled by the solve. */
static std::vector<std::vector<PlanarSlotInterval>> BuildIntervals(const PlanarChannels &channels, const std::vector<PlanarResolvedSite> &sites, const std::vector<uint32_t> &over)
{
	std::vector<std::vector<PlanarSlotInterval>> result(channels.channels.size());
	for (uint32_t c = 0; c < channels.channels.size(); c++) {
		const PlanarChannel &cc = channels.channels[c];
		const size_t k = cc.crossings.size();
		std::vector<PlanarSlotInterval> &intervals = result[c];
		intervals.push_back({"pre", {}});
		for (size_t i = 0; i < k; i++) {
			intervals.push_back({fmt::format("x{}", i), {}});
			intervals.push_back({i + 1 < k ? fmt::format("mid{}", i) : "post", {}});
		}
		if (k == 0) intervals.push_back({"post", {}});
		const int n = static_cast<int>(cc.lines.size());
		bool far_left = true;
		if (const auto join = JoinOf(channels, sites, c); join.has_value() && sites[*join].straight == c && sites[*join].prio) far_left = sites[*join].side > 0;
		for (size_t iv = 0; iv < intervals.size(); iv++) {
			const bool crossing_iv = k != 0 && iv % 2 == 1;
			bool is_over = crossing_iv && over[cc.crossings[iv / 2]] == c;
			if (k != 0 && iv % 2 == 0 && iv >= 2 && iv + 1 < intervals.size()) is_over = over[cc.crossings[(iv - 1) / 2]] == c && over[cc.crossings[(iv + 1) / 2]] == c; // Between two doubled crossings the copy runs on.
			std::vector<PlanarSlotTrack> &out = intervals[iv].tracks;
			for (int j = 0; j < n; j++) {
				const uint32_t line = cc.lines[j];
				int copy = 0; // -1 left, +1 right
				if (is_over && channels.line_share[line] >= 1.0) {
					const int away = far_left ? +1 : -1;
					const bool is_far = far_left ? j == 0 : j == n - 1;
					copy = is_far ? -away : away;
				}
				if (copy < 0) out.push_back({PlanarSlotKind::Copy, line, 0});
				out.push_back({PlanarSlotKind::Line, line, 0});
				if (copy > 0) out.push_back({PlanarSlotKind::Copy, line, 0});
			}
		}
	}
	return result;
}

/**
 * Build the slot layer of a variant: an integer program over the slots of every track of every interval that minimises
 * the steps of the lines and site links plus the width of every interval, then normalised per channel.
 * @param topology The topology.
 * @param channels The variant's channels (triangles flipped).
 * @param sites The resolved sites of the variant.
 * @param over Per channel crossing, the channel that is over.
 * @return The slots, with why set when the program has no optimal solution.
 */
PlanarSlots BuildPlanarSlots(const PlanarTopology &topology, const PlanarChannels &channels, const std::vector<PlanarResolvedSite> &sites, const std::vector<uint32_t> &over)
{
	PlanarSlots result;
	result.channels = BuildIntervals(channels, sites, over);
	const std::vector<std::vector<PlanarSlotInterval>> &intervals = result.channels;

	using Key = std::tuple<PlanarSlotKind, uint32_t, uint32_t, uint32_t>; ///< Kind, line, channel, interval.
	JunctionMilp milp;
	std::map<Key, uint32_t> var;
	const auto row = [&](std::vector<std::pair<uint32_t, double>> terms, double lo, double hi) { milp.AddRow(terms, lo, hi); };
	std::vector<uint32_t> o;
	for (uint32_t c = 0; c < channels.channels.size(); c++) o.push_back(milp.AddVariable(-500, 500, 0, true));
	for (uint32_t c = 0; c < channels.channels.size(); c++) {
		for (uint32_t iv = 0; iv < intervals[c].size(); iv++) {
			for (const PlanarSlotTrack &tr : intervals[c][iv].tracks) var[{tr.kind, tr.line, c, iv}] = milp.AddVariable(-500, 500, 0, true);
		}
	}
	const auto sv = [&](PlanarSlotKind k, uint32_t line, uint32_t c, uint32_t iv) { return var.at({k, line, c, iv}); };
	const auto last = [&](uint32_t c) { return static_cast<uint32_t>(intervals[c].size() - 1); };

	for (uint32_t c = 0; c < channels.channels.size(); c++) {
		for (uint32_t iv = 0; iv < intervals[c].size(); iv++) {
			const std::vector<PlanarSlotTrack> &list = intervals[c][iv].tracks;
			for (size_t i = 0; i + 1 < list.size(); i++) {
				const bool fray_gap = iv + 1 == intervals[c].size() && list[i].kind == PlanarSlotKind::Line && list[i + 1].kind == PlanarSlotKind::Line;
				row({{sv(list[i + 1].kind, list[i + 1].line, c, iv), 1}, {sv(list[i].kind, list[i].line, c, iv), -1}}, fray_gap ? LaneGap(channels, sites, c) : 1, JunctionMilp::INF);
			}
			for (size_t i = 0; i < list.size(); i++) {
				if (list[i].kind != PlanarSlotKind::Copy) continue;
				const size_t li = i > 0 && list[i - 1].kind == PlanarSlotKind::Line && list[i - 1].line == list[i].line ? i - 1 : i + 1;
				const double d = li < i ? 1 : -1; // s(copy) - s(line)
				row({{sv(PlanarSlotKind::Copy, list[i].line, c, iv), 1}, {sv(PlanarSlotKind::Line, list[i].line, c, iv), -1}}, d, d);
				if (iv + 1 < intervals[c].size()) {
					const bool next_copy = var.count({PlanarSlotKind::Copy, list[i].line, c, iv + 1}) != 0;
					if (next_copy) {
						row({{sv(PlanarSlotKind::Line, list[i].line, c, iv + 1), 1}, {sv(PlanarSlotKind::Line, list[i].line, c, iv), -1}}, 0, 0); // One parallelogram over consecutive doubled crossings.
					} else {
						const double late = 1 + 0.001 * (static_cast<double>(intervals[c].size()) - 1 - iv); const uint32_t up = milp.AddVariable(0, JunctionMilp::INF, late, true), dn = milp.AddVariable(0, JunctionMilp::INF, late, true); // Probe S4b: a charged step back from the copy's slot.
						row({{sv(PlanarSlotKind::Line, list[i].line, c, iv + 1), 1}, {sv(PlanarSlotKind::Copy, list[i].line, c, iv), -1}, {up, -1}, {dn, 1}}, 0, 0);
					}
				}
			}
		}
		const PlanarChannel &cc = channels.channels[c];
		for (size_t j = 0; j < cc.lines.size(); j++) {
			const PlanarLine &ln = topology.lines[cc.lines[j]];
			if (ln.from.port) row({{sv(PlanarSlotKind::Line, cc.lines[j], c, 0), 1}, {o[c], -1}}, static_cast<double>(j), static_cast<double>(j));
			if (ln.to.port) row({{sv(PlanarSlotKind::Line, cc.lines[j], c, last(c)), 1}, {o[c], -1}}, static_cast<double>(j), static_cast<double>(j));
		}
	}

	/* Probe S4c: a prio join's straight channel keeps the composer's entry gaps in its last interval. */
	for (size_t s = 0; s < channels.sites.size(); s++) {
		if (channels.sites[s].kind != PlanarSiteKind::Join || !sites[s].prio || sites[s].straight == UINT32_MAX) continue;
		const uint32_t c = sites[s].straight;
		const PlanarChannel &cc = channels.channels[c];
		const int n = static_cast<int>(cc.lines.size());
		const bool far_left = sites[s].side > 0;
		const auto e = [](int j) { return j == 0 ? 0 : 2 * j - 1; };
		for (int j = 1; j < n; j++) {
			const double d = far_left ? e(j) : e(n - 1) - e(n - 1 - j);
			row({{sv(PlanarSlotKind::Line, cc.lines[j], c, last(c)), 1}, {sv(PlanarSlotKind::Line, cc.lines[0], c, last(c)), -1}}, d, d);
		}
	}

	/* A straight line across a site is linked by a charged step. */
	struct LinkStep {
		uint32_t from, to, up, dn;
		int fixed = 0; ///< Probe S4a: the step of a fixed link.
	};
	std::vector<LinkStep> link_steps;
	for (size_t si = 0; si < channels.sites.size(); si++) {
		const PlanarSite &site = channels.sites[si];
		if (sites[si].straight == UINT32_MAX) continue;
		const bool split = site.kind == PlanarSiteKind::Fray || site.kind == PlanarSiteKind::LaneSplit;
		for (uint32_t n : site.nodes) {
			const uint32_t sl = channels.node_legs[n].straight;
			if (sl == UINT32_MAX) continue;
			const uint32_t from = split ? topology.nodes[n].in[0] : sl;
			const uint32_t to = split ? sl : topology.nodes[n].out[0];
			const uint32_t cf = channels.channel_of_line[from], ct = channels.channel_of_line[to];
				if (site.kind == PlanarSiteKind::Fray && sites[si].prio) { // Probe S4a: a prio fray's straight leg off a non-far lane doubles one slot toward the far lane.
					const PlanarChannel &fc = channels.channels[site.channel];
					const uint32_t far_line = sites[si].side > 0 ? fc.lines.front() : fc.lines.back();
					if (from != far_line) {
						const double d = sites[si].side > 0 ? -1 : 1;
						row({{sv(PlanarSlotKind::Line, to, ct, 0), 1}, {sv(PlanarSlotKind::Line, from, cf, last(cf)), -1}}, d, d);
						link_steps.push_back({from, to, UINT32_MAX, UINT32_MAX, static_cast<int>(d)});
						continue;
					}
				}
			const uint32_t up = milp.AddVariable(0, JunctionMilp::INF, 1, true), dn = milp.AddVariable(0, JunctionMilp::INF, 1, true);
			row({{sv(PlanarSlotKind::Line, to, ct, 0), 1}, {sv(PlanarSlotKind::Line, from, cf, last(cf)), -1}, {up, -1}, {dn, 1}}, 0, 0);
			link_steps.push_back({from, to, up, dn, 0});
		}
	}

	/* Objective: steps of every line and the width of every interval. */
	for (uint32_t c = 0; c < channels.channels.size(); c++) {
		for (uint32_t line : channels.channels[c].lines) {
			for (uint32_t iv = 0; iv + 1 < intervals[c].size(); iv++) {
				if (var.count({PlanarSlotKind::Copy, line, c, iv}) != 0) continue; // Probe S4e: the bridge's move onto the copy is not charged; the copy rows charge any step back.
				const double late = 1 + 0.001 * (static_cast<double>(intervals[c].size()) - 1 - iv);
				const uint32_t up = milp.AddVariable(0, JunctionMilp::INF, late, true), dn = milp.AddVariable(0, JunctionMilp::INF, late, true);
				row({{sv(PlanarSlotKind::Line, line, c, iv + 1), 1}, {sv(PlanarSlotKind::Line, line, c, iv), -1}, {up, -1}, {dn, 1}}, 0, 0);
			}
		}
		for (uint32_t iv = 0; iv < intervals[c].size(); iv++) {
			const std::vector<PlanarSlotTrack> &list = intervals[c][iv].tracks;
			if (list.size() < 2) continue; // One track: width 0, and a row naming one variable twice is malformed.
			static const double width_weight = std::getenv("PLANAR_S2W") != nullptr ? std::atof(std::getenv("PLANAR_S2W")) : 1.0; // Probe knob.
			const uint32_t w = milp.AddVariable(0, JunctionMilp::INF, width_weight, true);
			row({{w, 1}, {sv(list.back().kind, list.back().line, c, iv), -1}, {sv(list.front().kind, list.front().line, c, iv), 1}}, 0, 0);
		}
	}

	milp.SetTimeLimit(30);
	const JunctionMilp::Status status = milp.Solve();
	if (status != JunctionMilp::Status::Optimal) {
		result.channels.clear();
		result.why = fmt::format("no slots: {}", status == JunctionMilp::Status::Infeasible ? "infeasible" : "error");
		return result;
	}
	result.objective = milp.Objective();
	for (uint32_t c = 0; c < result.channels.size(); c++) {
		int least = INT32_MAX;
		for (uint32_t iv = 0; iv < result.channels[c].size(); iv++) {
			for (PlanarSlotTrack &tr : result.channels[c][iv].tracks) {
				tr.slot = static_cast<int>(std::lround(milp.Value(sv(tr.kind, tr.line, c, iv))));
				least = std::min(least, tr.slot);
			}
		}
		for (PlanarSlotInterval &interval : result.channels[c]) {
			for (PlanarSlotTrack &tr : interval.tracks) tr.slot -= least;
		}
	}
	for (const LinkStep &l : link_steps) result.links.push_back({l.from, l.to, l.up == UINT32_MAX ? l.fixed : static_cast<int>(std::lround(milp.Value(l.up) - milp.Value(l.dn)))});
	return result;
}
