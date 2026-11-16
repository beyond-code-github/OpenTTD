/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file planar_paths.cpp Channel paths (D327). */

#include "../../stdafx.h"

#include "planar_paths.h"

#include "../../3rdparty/fmt/format.h"

#include <cstdlib>
#include <functional>
#include <map>
#include <optional>

#include "../../safeguards.h"

namespace {

/** A heading turned by k eighths clockwise. */
PlanarHeading Turned(PlanarHeading h, int k)
{
	return static_cast<PlanarHeading>((static_cast<int>(h) + k + 8) % 8);
}

/** The heading of a single port in a set, if the set has one port. */
std::optional<PlanarHeading> PortHeading(const PlanarTopology &t, const std::set<uint32_t> &ports)
{
	if (ports.size() != 1) return std::nullopt;
	return t.ports[*ports.begin()].heading;
}

} // namespace

/**
 * The channel crossings whose over channel is a choice: neither or both channels carry a prio route.
 * A prio channel is over a non-prio one, so the main line stays straight (D328).
 * @param channels The channel graph.
 * @return Indices into PlanarChannels::crossings; bit i of a choice picks the over channel of the i-th.
 */
std::vector<uint32_t> PlanarOpenCrossings(const PlanarChannels &channels)
{
	std::vector<uint32_t> open;
	for (uint32_t x = 0; x < channels.crossings.size(); x++) {
		const PlanarChannelCrossing &c = channels.crossings[x];
		if (channels.channels[c.a].prio == channels.channels[c.b].prio) open.push_back(x);
	}
	return open;
}

/**
 * The empty triangles of the channel graph: three channels crossing pairwise, each with its two crossings of the others next
 * to each other along it. Each can sit either way round (both are planar and carry the same routes); the topology's event order
 * gives one, a flip the other.
 * @param channels The channel graph.
 * @return Per triangle its three channel crossings.
 */
std::vector<std::array<uint32_t, 3>> PlanarTriangles(const PlanarChannels &channels)
{
	std::map<std::pair<uint32_t, uint32_t>, uint32_t> pair;
	for (uint32_t x = 0; x < channels.crossings.size(); x++) pair[{channels.crossings[x].a, channels.crossings[x].b}] = x;
	const auto adjacent = [&](uint32_t c, uint32_t x, uint32_t y) {
		const auto &list = channels.channels[c].crossings;
		const auto i = std::ranges::find(list, x) - list.begin(), j = std::ranges::find(list, y) - list.begin();
		return std::abs(i - j) == 1;
	};
	std::vector<std::array<uint32_t, 3>> out;
	const uint32_t n = static_cast<uint32_t>(channels.channels.size());
	for (uint32_t a = 0; a < n; a++) {
		for (uint32_t b = a + 1; b < n; b++) {
			const auto ab = pair.find({a, b});
			if (ab == pair.end()) continue;
			for (uint32_t c = b + 1; c < n; c++) {
				const auto ac = pair.find({a, c}), bc = pair.find({b, c});
				if (ac == pair.end() || bc == pair.end()) continue;
				if (adjacent(a, ab->second, ac->second) && adjacent(b, ab->second, bc->second) && adjacent(c, ac->second, bc->second)) out.push_back({ab->second, bc->second, ac->second});
			}
		}
	}
	return out;
}

/**
 * The channel graph with some triangles flipped: on each of a flipped triangle's three channels its two crossings swap order.
 * @param channels The channel graph.
 * @param flips Bit i set: the i-th triangle of PlanarTriangles is flipped.
 * @return The flipped copy.
 */
PlanarChannels FlipPlanarTriangles(const PlanarChannels &channels, uint64_t flips)
{
	PlanarChannels out = channels;
	const std::vector<std::array<uint32_t, 3>> triangles = PlanarTriangles(channels);
	for (size_t i = 0; i < triangles.size(); i++) {
		if ((flips >> i & 1) == 0) continue;
		const std::array<uint32_t, 3> &tri = triangles[i];
		for (PlanarChannel &c : out.channels) {
			std::vector<size_t> at;
			for (size_t k = 0; k < c.crossings.size(); k++) {
				if (std::ranges::find(tri, c.crossings[k]) != tri.end()) at.push_back(k);
			}
			if (at.size() == 2) std::swap(c.crossings[at[0]], c.crossings[at[1]]);
		}
	}
	return out;
}

/**
 * Every channel's path for one choice of the open crossings.
 * A channel turns the short way from its in port's heading to its out port's (a channel reaching several ports takes the
 * one it has a single one of), 45 degrees a piece. Its crossings sit, in its event order, on the first piece at or after the
 * last one that suits: axis where it is over, not parallel to the over piece where it is under; when none suits, the path
 * steps back and forth once (a detour) and tries again. Over channels are placed before the channels they cross.
 * @param topology The topology.
 * @param channels Its channel graph.
 * @param choice Bit i set: the open crossing i has its higher channel over, else its lower.
 * @return The paths, or a reason in why.
 */
PlanarPaths BuildPlanarPaths(const PlanarTopology &topology, const PlanarChannels &channels, uint64_t choice)
{
	const PlanarTopology &t = topology;
	PlanarPaths out;
	const size_t n = channels.channels.size();
	out.paths.resize(n);
	out.over.resize(channels.crossings.size());
	const std::vector<uint32_t> open = PlanarOpenCrossings(channels);
	for (uint32_t x = 0; x < channels.crossings.size(); x++) {
		const PlanarChannelCrossing &c = channels.crossings[x];
		const auto it = std::ranges::find(open, x);
		if (it == open.end()) {
			out.over[x] = channels.channels[c.a].prio ? c.a : c.b;
		} else {
			out.over[x] = (choice >> (it - open.begin()) & 1) != 0 ? c.b : c.a;
		}
	}

	/* Headings: the short turn from start to end. */
	for (uint32_t ch = 0; ch < n; ch++) {
		const PlanarChannel &c = channels.channels[ch];
		const std::optional<PlanarHeading> up = PortHeading(t, c.up), down = PortHeading(t, c.down);
		const std::optional<PlanarHeading> from = up.has_value() ? up : down, to = down.has_value() ? down : up;
		if (!from.has_value()) {
			out.why = fmt::format("channel of {} has no single port heading", t.lines[c.lines.front()].name);
			return out;
		}
		const int turn = (static_cast<int>(*to) - static_cast<int>(*from) + 12) % 8 - 4;
		if (turn == -4) {
			out.why = fmt::format("channel of {} turns back", t.lines[c.lines.front()].name);
			return out;
		}
		const int step = turn > 0 ? 1 : -1;
		out.paths[ch].headings.push_back(*from);
		for (int k = 0; k != turn; k += step) out.paths[ch].headings.push_back(Turned(*from, k + step));
	}

	/* Place over channels before the channels they cross. */
	std::vector<uint32_t> order;
	std::vector<uint8_t> state(n, 0);
	std::string cycle;
	const std::function<void(uint32_t)> visit = [&](uint32_t ch) {
		if (state[ch] == 2) return;
		if (state[ch] == 1) {
			cycle = t.lines[channels.channels[ch].lines.front()].name;
			return;
		}
		state[ch] = 1;
		for (uint32_t x : channels.channels[ch].crossings) {
			if (out.over[x] != ch) visit(out.over[x]);
		}
		state[ch] = 2;
		order.push_back(ch);
	};
	for (uint32_t ch = 0; ch < n; ch++) visit(ch);
	if (!cycle.empty()) {
		out.why = fmt::format("channels cross over each other in a cycle at {}", cycle);
		return out;
	}

	for (uint32_t ch : order) {
		const PlanarChannel &c = channels.channels[ch];
		PlanarChannelPath &p = out.paths[ch];
		uint32_t at = 0;
		for (uint32_t x : c.crossings) {
			const bool over = out.over[x] == ch;
			std::optional<int> over_axis;
			if (!over) {
				const uint32_t o = out.over[x];
				const auto &list = channels.channels[o].crossings;
				const size_t i = std::ranges::find(list, x) - list.begin();
				over_axis = static_cast<int>(out.paths[o].headings[out.paths[o].crossing_piece[i]]) % 4;
			}
			const auto suits = [&](PlanarHeading h) { return over ? static_cast<int>(h) % 2 == 0 : static_cast<int>(h) % 4 != *over_axis; };
			bool placed = false;
			/* Experiment (PLANAR_PERP): under a prio (doubled) channel, cross at right angles where a back-and-forth allows it. */
			if (!over && channels.channels[out.over[x]].prio && std::getenv("PLANAR_PERP") != nullptr) {
				const auto perp = [&](PlanarHeading h) { return static_cast<int>(h) % 4 == (*over_axis + 2) % 4; };
				for (int detour = 0; detour < 2 && !placed; detour++) {
					for (uint32_t i = at; i < p.headings.size(); i++) {
						if (perp(p.headings[i])) {
							at = i;
							placed = true;
							break;
						}
					}
					if (placed || at == 0) break;
					p.headings.insert(p.headings.begin() + at + 1, {p.headings[at - 1], p.headings[at]});
				}
			}
			for (int detour = 0; detour < 3 && !placed; detour++) {
				for (uint32_t i = at; i < p.headings.size(); i++) {
					if (suits(p.headings[i])) {
						at = i;
						placed = true;
						break;
					}
				}
				if (placed || at == 0) break;
				p.headings.insert(p.headings.begin() + at + 1, {p.headings[at - 1], p.headings[at]});
			}
			if (!placed) {
				out.why = fmt::format("no piece of the channel of {} suits its crossing", t.lines[c.lines.front()].name);
				return out;
			}
			p.crossing_piece.push_back(at);
		}
	}
	return out;
}
