/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file planar_shape.cpp The shape program: per channel, integer unwrapped headings at its crossings, least total turning (DESIGN-tracks section 12). */

#include "../../stdafx.h"

#include "planar_shape.h"

#include "../../3rdparty/fmt/format.h"
#include "../placement/junction_milp.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>

#include "../../safeguards.h"

namespace {

/** The heading of a single port in a set, if the set has one port. */
std::optional<int> PortHeading(const PlanarTopology &t, const std::set<uint32_t> &ports)
{
	if (ports.size() != 1) return std::nullopt;
	return static_cast<int>(t.ports[*ports.begin()].heading);
}

/** One channel's variables: its unwrapped start, headings at its crossings and end. */
struct ChannelVars {
	uint32_t start;
	std::vector<uint32_t> at; ///< Per entry of PlanarChannel::crossings.
	uint32_t end;
};

} // namespace

/**
 * The turning of a shape: the sum over channels of the 45 degree turns between consecutive pieces (the start and end headings are pieces, so they add none).
 * @param paths The shape.
 * @return The turns.
 */
int PlanarShapeTurns(const std::vector<PlanarChannelPath> &paths)
{
	int turns = 0;
	for (const PlanarChannelPath &p : paths) turns += static_cast<int>(p.headings.size()) - 1;
	return turns;
}

/**
 * The least-turning channel shapes that obey the crossing rules, for one variant. Per channel the unwrapped headings at its
 * start, crossings and end are integers; the cost is the sum of the absolute turns between consecutive ones. A channel starts on
 * its up port's heading, or its in channel's end heading when it starts at a node, and ends on its down port's heading (else its
 * own start heading, mod 8), none turning back. At a channel crossing the over heading is even and the under heading differs
 * from it by s * a, a in 1..3, plus a multiple of 8, where s is the side the topology has the under line cross from (a rule).
 * @param t The topology.
 * @param channels The channel graph of the variant (flipped as the variant is).
 * @param sides The line sides of the topology in the variant's channel order.
 * @param over Per channel crossing, the channel that is over.
 * @param turns The objective, if not null.
 * @param why The reason on failure, if not null.
 * @return One path per channel; empty on failure.
 */
std::vector<PlanarChannelPath> SolvePlanarShape(const PlanarTopology &t, const PlanarChannels &channels, const PlanarLineSides &sides, const std::vector<uint32_t> &over, int *turns, std::string *why)
{
	const auto fail = [&](const std::string &what) -> std::vector<PlanarChannelPath> {
		if (why != nullptr) *why = what;
		return {};
	};
	const size_t n = channels.channels.size();
	if (over.size() != channels.crossings.size()) return fail(fmt::format("{} over channels for {} crossings", over.size(), channels.crossings.size()));
	const auto name = [&](uint32_t ch) { return fmt::format("channel of {}", t.lines[channels.channels[ch].lines.front()].name); };
	constexpr double LO = -16, HI = 23;

	JunctionMilp milp;
	const auto row = [&](std::vector<std::pair<uint32_t, double>> terms, double rhs) { milp.AddRow(terms, rhs, rhs); };
	const auto integer = [&](double lower, double upper) { return milp.AddVariable(lower, upper, 0, true); };

	/* Variables: every channel's start, crossings and end first (a node-start channel's start is its in channel's end). */
	std::vector<ChannelVars> vars(n);
	std::vector<std::optional<int>> from(n), to(n);
	std::vector<uint32_t> in_channel(n, UINT32_MAX);
	for (uint32_t ch = 0; ch < n; ch++) {
		const PlanarChannel &c = channels.channels[ch];
		const PlanarLine &first = t.lines[c.lines.front()];
		from[ch] = PortHeading(t, c.up);
		to[ch] = PortHeading(t, c.down);
		if (!first.from.port) {
			in_channel[ch] = channels.channel_of_line[t.nodes[first.from.index].in.front()];
			from[ch] = std::nullopt;
		} else {
			if (!from[ch].has_value()) from[ch] = to[ch];
			if (!to[ch].has_value()) to[ch] = from[ch];
			if (!from[ch].has_value()) return fail(fmt::format("{} has no single port heading", name(ch)));
			if ((*to[ch] - *from[ch] + 8) % 8 == 4) return fail(fmt::format("{} turns back", name(ch)));
		}
		ChannelVars &v = vars[ch];
		v.start = from[ch].has_value() ? milp.AddVariable(*from[ch], *from[ch], 0, true) : integer(LO, HI);
		for (size_t i = 0; i < c.crossings.size(); i++) v.at.push_back(integer(LO, HI));
		v.end = integer(LO, HI);
	}
	for (uint32_t ch = 0; ch < n; ch++) {
		const ChannelVars &v = vars[ch];
		if (in_channel[ch] != UINT32_MAX) row({{v.start, 1}, {vars[in_channel[ch]].end, -1}}, 0);
		if (to[ch].has_value()) {
			/* End on the down port's heading, unwrapped by whole turns. */
			row({{v.end, 1}, {integer(-3, 3), -8}}, *to[ch]);
			if (in_channel[ch] != UINT32_MAX) {
				/* The end and the in channel's end heading are not opposite: the net turn is -3..3 plus whole turns. */
				row({{v.end, 1}, {v.start, -1}, {integer(-3, 3), -1}, {integer(-3, 3), -8}}, 0);
			}
		} else if (in_channel[ch] != UINT32_MAX) {
			/* No single down port: end on the start heading. */
			row({{v.end, 1}, {v.start, -1}, {integer(-3, 3), -8}}, 0);
		}
	}

	/* The cost: each turn between consecutive points, |t| = p + n with t = next - prev = p - n. */
	for (uint32_t ch = 0; ch < n; ch++) {
		const ChannelVars &v = vars[ch];
		std::vector<uint32_t> points{v.start};
		points.insert(points.end(), v.at.begin(), v.at.end());
		points.push_back(v.end);
		for (size_t i = 1; i < points.size(); i++) {
			const uint32_t p = milp.AddVariable(0, JunctionMilp::INF, 1, false), q = milp.AddVariable(0, JunctionMilp::INF, 1, false);
			row({{points[i], 1}, {points[i - 1], -1}, {p, -1}, {q, 1}}, 0);
		}
	}

	/* The crossing rules at each channel crossing. */
	for (uint32_t x = 0; x < channels.crossings.size(); x++) {
		const PlanarChannelCrossing &cr = channels.crossings[x];
		if (over[x] != cr.a && over[x] != cr.b) return fail(fmt::format("crossing {} has an over channel not on it", x));
		const uint32_t under = over[x] == cr.a ? cr.b : cr.a;
		const auto at = [&](uint32_t ch) {
			const auto &list = channels.channels[ch].crossings;
			return vars[ch].at[std::ranges::find(list, x) - list.begin()];
		};
		const uint32_t ho = at(over[x]), hu = at(under);
		/* The side the under channel crosses from, the same for every line crossing of the pair. */
		int side = 0;
		for (uint32_t lc : cr.line_crossings) {
			const PlanarCrossing &lx = t.crossings[lc];
			const int s = sides.CrossingFrom(lc, channels.channel_of_line[lx.a] == under ? lx.a : lx.b);
			if (s == 0 || (side != 0 && s != side)) return fail(fmt::format("crossing {} has no single side", x));
			side = s;
		}
		row({{ho, 1}, {integer(-8, 11), -2}}, 0);
		row({{hu, 1}, {ho, -1}, {integer(1, 3), -side}, {integer(-5, 5), -8}}, 0);
	}

	milp.SetTimeLimit(15);
	const JunctionMilp::Status status = milp.Solve();
	if (status == JunctionMilp::Status::Infeasible) return fail("the shape program is infeasible");
	if (status != JunctionMilp::Status::Optimal) return fail("the shape program did not solve");

	/* The paths: from each point to the next, one piece a 45 degree step; a crossing sits on the piece at its point. */
	const auto value = [&](uint32_t var) { return static_cast<int>(std::lround(milp.Value(var))); };
	const auto wrap = [](int h) { return static_cast<PlanarHeading>(((h % 8) + 8) % 8); };
	std::vector<PlanarChannelPath> paths(n);
	for (uint32_t ch = 0; ch < n; ch++) {
		const ChannelVars &v = vars[ch];
		PlanarChannelPath &p = paths[ch];
		int cur = value(v.start);
		p.headings.push_back(wrap(cur));
		const auto walk = [&](int target) {
			for (; cur != target; cur += target > cur ? 1 : -1) p.headings.push_back(wrap(cur + (target > cur ? 1 : -1)));
		};
		for (uint32_t var : v.at) {
			walk(value(var));
			p.crossing_piece.push_back(static_cast<uint32_t>(p.headings.size() - 1));
		}
		walk(value(v.end));
	}
	if (turns != nullptr) *turns = PlanarShapeTurns(paths);
	return paths;
}
