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
#include "planar_regions.h"

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
 * Check a shape against the rules BuildPlanarPaths builds to: one path per channel; every path has headings, and consecutive
 * headings are 45 degrees apart; a channel's first heading is its up port's, or its in channel's last heading when it starts at
 * a node (falling back to its down port's when it has neither); its last heading is its down port's when that is a single port
 * (else its first), and the two are not opposite; every channel has one crossing piece per crossing entry, each on a piece of
 * the path and in travel order; at each crossing the over channel's piece is on an axis heading and the under channel's piece
 * is not parallel to it, and it arrives from the side of the over channel the topology has it cross from.
 * @param topology The topology.
 * @param channels The channel graph the paths are for (flipped as the variant is).
 * @param over Per channel crossing, the channel that is over.
 * @param paths The shape: one path per channel.
 * @return Empty, or the first rule it breaks.
 */
std::string CheckPlanarShape(const PlanarTopology &topology, const PlanarChannels &channels, const std::vector<uint32_t> &over, const std::vector<PlanarChannelPath> &paths)
{
	const PlanarTopology &t = topology;
	const size_t n = channels.channels.size();
	if (paths.size() != n) return fmt::format("{} paths for {} channels", paths.size(), n);
	if (over.size() != channels.crossings.size()) return fmt::format("{} over channels for {} crossings", over.size(), channels.crossings.size());
	const auto name = [&](uint32_t ch) { return fmt::format("channel of {}", t.lines[channels.channels[ch].lines.front()].name); };
	for (uint32_t ch = 0; ch < n; ch++) {
		const PlanarChannelPath &p = paths[ch];
		if (p.headings.empty()) return fmt::format("{} has no headings", name(ch));
		for (size_t i = 1; i < p.headings.size(); i++) {
			const int d = (static_cast<int>(p.headings[i]) - static_cast<int>(p.headings[i - 1]) + 8) % 8;
			if (d != 1 && d != 7) return fmt::format("{} has consecutive headings not 45 degrees apart at piece {}", name(ch), i);
		}
	}
	for (uint32_t ch = 0; ch < n; ch++) {
		const PlanarChannel &c = channels.channels[ch];
		const PlanarChannelPath &p = paths[ch];
		const PlanarLine &first = t.lines[c.lines.front()];
		std::optional<PlanarHeading> from = PortHeading(t, c.up), to = PortHeading(t, c.down);
		if (!first.from.port) from = paths[channels.channel_of_line[t.nodes[first.from.index].in.front()]].headings.back();
		if (!from.has_value()) from = to;
		if (!to.has_value()) to = from;
		if (!from.has_value()) return fmt::format("{} has no single port heading", name(ch));
		if ((static_cast<int>(*to) - static_cast<int>(*from) + 8) % 8 == 4) return fmt::format("{} turns back", name(ch));
		if (p.headings.front() != *from) return fmt::format("{} does not start on its start heading", name(ch));
		if (p.headings.back() != *to) return fmt::format("{} does not end on its end heading", name(ch));
		if (p.crossing_piece.size() != c.crossings.size()) return fmt::format("{} has {} crossing pieces for {} crossings", name(ch), p.crossing_piece.size(), c.crossings.size());
		for (size_t k = 0; k < p.crossing_piece.size(); k++) {
			if (p.crossing_piece[k] >= p.headings.size()) return fmt::format("{} has crossing piece {} out of range", name(ch), k);
			if (k > 0 && p.crossing_piece[k] < p.crossing_piece[k - 1]) return fmt::format("{} has its crossing pieces out of travel order at {}", name(ch), k);
		}
	}
	for (uint32_t x = 0; x < channels.crossings.size(); x++) {
		const PlanarChannelCrossing &cr = channels.crossings[x];
		if (over[x] != cr.a && over[x] != cr.b) return fmt::format("crossing {} has an over channel not on it", x);
		const uint32_t under = over[x] == cr.a ? cr.b : cr.a;
		const auto heading = [&](uint32_t ch) {
			const auto &list = channels.channels[ch].crossings;
			return static_cast<int>(paths[ch].headings[paths[ch].crossing_piece[std::ranges::find(list, x) - list.begin()]]);
		};
		const int ho = heading(over[x]), hu = heading(under);
		if (ho % 2 != 0) return fmt::format("{} is over a crossing on a diagonal piece", name(over[x]));
		if (ho % 4 == hu % 4) return fmt::format("{} crosses {} on a parallel piece", name(under), name(over[x]));
	}
	const PlanarLineSides sides(t, channels);
	if (!sides.Why().empty()) return sides.Why();
	for (uint32_t x = 0; x < channels.crossings.size(); x++) {
		const PlanarChannelCrossing &cr = channels.crossings[x];
		const uint32_t under = over[x] == cr.a ? cr.b : cr.a;
		const auto heading = [&](uint32_t ch) {
			const auto &list = channels.channels[ch].crossings;
			return static_cast<int>(paths[ch].headings[paths[ch].crossing_piece[std::ranges::find(list, x) - list.begin()]]);
		};
		const int d = (heading(under) - heading(over[x]) + 8) % 8;
		const int shape_side = d >= 1 && d <= 3 ? +1 : -1;
		for (uint32_t lc : cr.line_crossings) {
			const PlanarCrossing &lx = t.crossings[lc];
			if (sides.CrossingFrom(lc, channels.channel_of_line[lx.a] == under ? lx.a : lx.b) != shape_side) return fmt::format("{} crosses {} from the wrong side", name(under), name(over[x]));
		}
	}
	return {};
}

/**
 * Every channel's path for one choice of the open crossings.
 * A channel turns the short way from its in port's heading to its out port's (a channel reaching several ports takes the
 * one it has a single one of), 45 degrees a piece. Its crossings sit, in its event order, on the first piece at or after the
 * last one that suits: axis where it is over, not parallel to the over piece where it is under; when none suits, the path
 * steps back and forth once (a detour) and tries again. The pieces settle jointly: a piece suits given the other channel's current piece, and passes repeat until none changes.
 * @param topology The topology.
 * @param channels Its channel graph.
 * @param choice Bit i set: the open crossing i has its higher channel over, else its lower.
 * @param shape Null, or the paths to use instead of deciding them: they are checked by CheckPlanarShape and returned as given.
 * @return The paths, or a reason in why.
 */
PlanarPaths BuildPlanarPaths(const PlanarTopology &topology, const PlanarChannels &channels, uint64_t choice, const std::vector<PlanarChannelPath> *shape)
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

	if (shape != nullptr) {
		out.paths = *shape;
		out.why = CheckPlanarShape(t, channels, out.over, out.paths);
		return out;
	}

	/* Headings: the short turn from start to end; a channel starting at a node starts on the heading its in channel ends on. */
	std::vector<bool> headed(n, false);
	std::function<bool(uint32_t)> head = [&](uint32_t ch) -> bool {
		if (headed[ch]) return true;
		headed[ch] = true;
		const PlanarChannel &c = channels.channels[ch];
		const PlanarLine &first = t.lines[c.lines.front()];
		std::optional<PlanarHeading> from = PortHeading(t, c.up), to = PortHeading(t, c.down);
		if (!first.from.port) {
			const uint32_t in = channels.channel_of_line[t.nodes[first.from.index].in.front()];
			if (!head(in)) return false;
			from = out.paths[in].headings.back();
		}
		if (!from.has_value()) from = to;
		if (!to.has_value()) to = from;
		if (!from.has_value()) {
			out.why = fmt::format("channel of {} has no single port heading", first.name);
			return false;
		}
		const int turn = (static_cast<int>(*to) - static_cast<int>(*from) + 12) % 8 - 4;
		if (turn == -4) {
			out.why = fmt::format("channel of {} turns back", first.name);
			return false;
		}
		const int step = turn > 0 ? 1 : -1;
		out.paths[ch].headings.push_back(*from);
		for (int k = 0; k != turn; k += step) out.paths[ch].headings.push_back(Turned(*from, k + step));
		return true;
	};
	for (uint32_t ch = 0; ch < n; ch++) {
		if (!head(ch)) return out;
	}

	/* A fork leg's side: -1 for the left leg, +1 for the right, 0 for a channel not starting at a fork. */
	const auto fork_side = [&](const PlanarChannel &c) -> int {
		const PlanarLine &first = t.lines[c.lines.front()];
		if (first.from.port) return 0;
		const PlanarNode &node = t.nodes[first.from.index];
		if (!node.IsSplit() || channels.node_legs[first.from.index].straight != UINT32_MAX) return 0;
		return node.out[0] == c.lines.front() ? -1 : +1;
	};

	/* Visit over channels before the channels they cross where the overs have no cycle. */
	std::vector<uint32_t> order;
	std::vector<uint8_t> state(n, 0);
	const std::function<void(uint32_t)> visit = [&](uint32_t ch) {
		if (state[ch] != 0) return;
		state[ch] = 1;
		for (uint32_t x : channels.channels[ch].crossings) {
			if (out.over[x] != ch) visit(out.over[x]);
		}
		state[ch] = 2;
		order.push_back(ch);
	};
	for (uint32_t ch = 0; ch < n; ch++) visit(ch);

	/*
	 * Crossing pieces, settled jointly: each channel walks its crossings in event order and takes the first piece at or after
	 * the last that suits, given the other channel's current piece; passes repeat until no piece changes.
	 */
	std::vector<std::vector<int>> piece(n);
	for (uint32_t ch = 0; ch < n; ch++) piece[ch].assign(channels.channels[ch].crossings.size(), -1);
	const auto partner_heading = [&](uint32_t x, uint32_t ch) -> std::optional<int> {
		const PlanarChannelCrossing &xc = channels.crossings[x];
		const uint32_t o = xc.a == ch ? xc.b : xc.a;
		const auto &list = channels.channels[o].crossings;
		const int pk = piece[o][std::ranges::find(list, x) - list.begin()];
		if (pk < 0) return std::nullopt;
		return static_cast<int>(out.paths[o].headings[pk]);
	};
	/* While the other channel's piece is not chosen, a piece suits only when one of the other's pieces could cross it. */
	const auto partner_could = [&](uint32_t x, uint32_t ch, PlanarHeading h) {
		const PlanarChannelCrossing &xc = channels.crossings[x];
		const uint32_t o = xc.a == ch ? xc.b : xc.a;
		const bool other_over = out.over[x] == o;
		std::vector<PlanarHeading> could = out.paths[o].headings;
		if (could.size() == 1 && fork_side(channels.channels[o]) != 0) could.push_back(Turned(could[0], fork_side(channels.channels[o]))); // its jog
		for (PlanarHeading g : could) {
			if (other_over && static_cast<int>(g) % 2 != 0) continue;
			if (static_cast<int>(g) % 4 != static_cast<int>(h) % 4) return true;
		}
		return false;
	};
	bool settled = false;
	for (int pass = 0; pass < 6 && !settled; pass++) {
		settled = true;
		for (uint32_t ch : order) {
			const PlanarChannel &c = channels.channels[ch];
			PlanarChannelPath &p = out.paths[ch];
			uint32_t at = 0;
			for (size_t k = 0; k < c.crossings.size(); k++) {
				const uint32_t x = c.crossings[k];
				const bool over = out.over[x] == ch;
				const std::optional<int> other = partner_heading(x, ch);
				bool lenient = false; // no piece the other channel could cross: the first piece that suits this channel alone
				const auto suits = [&](PlanarHeading h) {
					if (over && static_cast<int>(h) % 2 != 0) return false;
					if (other.has_value()) return static_cast<int>(h) % 4 != *other % 4;
					return lenient || partner_could(x, ch, h);
				};
				if (!other.has_value()) {
					bool any = false;
					for (uint32_t i = at; i < p.headings.size() && !any; i++) any = suits(p.headings[i]);
					lenient = !any;
				}
				bool placed = false;
				/* Under a prio (doubled) channel, cross at right angles where a back-and-forth allows it (the hand T3's D path). */
				if (!over && other.has_value() && channels.channels[out.over[x]].prio) {
					const auto perp = [&](PlanarHeading h) { return static_cast<int>(h) % 4 == (*other + 2) % 4; };
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
					if (placed) break;
					if (at == 0) {
						/* A straight fork leg jogs toward its own side to cross (the left leg left, the right leg right). */
						const int side = fork_side(c);
						if (side == 0 || p.headings.size() != 1) break;
						p.headings = {p.headings[0], Turned(p.headings[0], side), p.headings[0]};
						continue;
					}
					p.headings.insert(p.headings.begin() + at + 1, {p.headings[at - 1], p.headings[at]});
				}
				if (!placed) {
					out.why = fmt::format("no piece of the channel of {} suits its crossing", t.lines[c.lines.front()].name);
					return out;
				}
				if (piece[ch][k] != static_cast<int>(at)) settled = false;
				piece[ch][k] = static_cast<int>(at);
			}
		}
	}
	if (!settled) {
		out.why = "the crossing pieces do not settle";
		return out;
	}
	for (uint32_t ch = 0; ch < n; ch++) {
		for (int pk : piece[ch]) out.paths[ch].crossing_piece.push_back(static_cast<uint32_t>(pk));
	}
	return out;
}
