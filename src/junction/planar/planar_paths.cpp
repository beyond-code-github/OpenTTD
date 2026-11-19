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

/**
 * The headings of every line of a variant, sites included (DESIGN-tracks §16.7).
 * A line's base list is its channel's path with its channel crossings on that path's crossing pieces. A leg leaving a split
 * node (the other leg of a node with a straight leg; both legs of a fork, the left with side -1, the right +1) starts with one
 * piece, the turn of the in line's last heading by its side, dropping its own first piece when its second is that turn; a leg
 * entering a merge node mirrors this at its end with the turn of the out line's first heading by minus its side, the straight
 * leg keeping its base list. A crossing inside a site's group (both lines starting, or both ending, at the group) has the
 * straight leg's line over, and sits on the first piece of each line that suits counting from the start at a split group,
 * on the last counting back from the end at a merge group: the over line's piece on an axis, the under line's not parallel to it.
 * Each line's event pieces must not decrease. A copy or row track runs beside the parent's pieces of the first and last event
 * it mirrors (a row from its first event, or the parent's last piece with none, to the parent's last piece).
 * @param topology The topology.
 * @param channels Its channel graph, flipped as the variant is.
 * @param paths The variant's channel paths.
 * @param sites The resolved sites.
 * @param events Each line's crossing events in travel order (PlanarLineSides::Events).
 * @param map The variant's track map.
 * @return The lists and the parent ranges, or the first rule that failed in why.
 */
PlanarLineHeadings BuildPlanarLineHeadings(const PlanarTopology &topology, const PlanarChannels &channels, const PlanarPaths &paths, const std::vector<PlanarResolvedSite> &sites, const std::vector<std::vector<uint32_t>> &events, const PlanarTrackMap &map)
{
	const PlanarTopology &t = topology;
	PlanarLineHeadings out;
	out.lines.resize(t.lines.size());
	out.beside.assign(map.tracks.size(), {UINT32_MAX, 0, 0});
	const auto fail = [&](const std::string &why) {
		if (out.why.empty()) out.why = why;
	};

	/* The channel crossing and the group each topology crossing belongs to. */
	std::map<uint32_t, std::pair<uint32_t, uint32_t>> channel_crossing; // crossing -> (channel crossing, line crossing index unused)
	for (uint32_t x = 0; x < channels.crossings.size(); x++) {
		for (const uint32_t c : channels.crossings[x].line_crossings) channel_crossing[c] = {x, 0};
	}
	std::map<uint32_t, uint32_t> group_of;
	for (uint32_t g = 0; g < channels.group_crossings.size(); g++) {
		for (const uint32_t c : channels.group_crossings[g]) group_of[c] = g;
	}

	/* Rule 0: a node's straight leg and the side of the other leg. A site composed per node keeps the node's own legs; any other site has its resolved straight channel and side. */
	struct NodeLegs {
		uint32_t straight;
		int side;
	};
	const auto legs_of = [&](uint32_t n) -> NodeLegs {
		const PlanarNode &node = t.nodes[n];
		for (uint32_t s = 0; s < channels.sites.size(); s++) {
			const PlanarSite &site = channels.sites[s];
			if (std::ranges::find(site.nodes, n) == site.nodes.end()) continue;
			const bool per_node = site.kind == PlanarSiteKind::LaneSplit || site.kind == PlanarSiteKind::LaneMerge || site.others.size() != 2;
			if (per_node) break;
			uint32_t straight = UINT32_MAX;
			for (const uint32_t l : node.IsSplit() ? node.out : node.in) {
				if (channels.channel_of_line[l] == sites[s].straight) straight = l;
			}
			if (straight == UINT32_MAX) fail(fmt::format("node {} has no leg in its site's straight channel", n));
			return {straight, sites[s].side};
		}
		return {channels.node_legs[n].straight, channels.node_legs[n].side};
	};

	/* Rules 1 to 3: each line's list, with the pieces its channel crossings sit on. */
	constexpr uint32_t UNPLACED = UINT32_MAX;
	for (uint32_t l = 0; l < t.lines.size(); l++) {
		const PlanarLine &line = t.lines[l];
		const uint32_t ch = channels.channel_of_line[l];
		std::vector<PlanarHeading> list = paths.paths[ch].headings;
		const uint32_t base_last = static_cast<uint32_t>(list.size()) - 1;

		/* The base piece each channel crossing of the line sits on. */
		std::vector<uint32_t> base_piece;
		for (const uint32_t c : events[l]) {
			const auto it = channel_crossing.find(c);
			if (it == channel_crossing.end()) {
				base_piece.push_back(UNPLACED);
				continue;
			}
			const std::vector<uint32_t> &list_x = channels.channels[ch].crossings;
			const auto k = std::ranges::find(list_x, it->second.first);
			if (k == list_x.end()) {
				fail(fmt::format("crossing of {} is not on its channel", line.name));
				base_piece.push_back(UNPLACED);
				continue;
			}
			base_piece.push_back(paths.paths[ch].crossing_piece[k - list_x.begin()]);
		}
		const auto has_event_on = [&](uint32_t piece) { return std::ranges::find(base_piece, piece) != base_piece.end(); };

		int shift = 0; // list piece = base piece + shift
		if (!line.from.port && t.nodes[line.from.index].IsSplit()) {
			const PlanarNode &node = t.nodes[line.from.index];
			const NodeLegs legs = legs_of(line.from.index);
			if (legs.straight != l) {
				const int side = legs.straight != UINT32_MAX ? legs.side : (node.out[0] == l ? -1 : +1);
				const PlanarHeading h0 = paths.paths[channels.channel_of_line[node.in[0]]].headings.back();
				const PlanarHeading turn = Turned(h0, side);
				if (list.size() >= 2 && list[1] == turn && !has_event_on(0)) {
					list.erase(list.begin());
					shift = -1;
				} else {
					list.insert(list.begin(), turn);
					shift = 1;
				}
			}
		}
		if (!line.to.port && t.nodes[line.to.index].in.size() == 2) {
			const PlanarNode &node = t.nodes[line.to.index];
			const NodeLegs legs = legs_of(line.to.index);
			if (legs.straight != l) {
				const int side = legs.straight != UINT32_MAX ? legs.side : (node.in[0] == l ? -1 : +1);
				const PlanarHeading h1 = paths.paths[channels.channel_of_line[node.out[0]]].headings.front();
				const PlanarHeading turn = Turned(h1, -side);
				if (list.size() >= 2 && list[list.size() - 2] == turn && !has_event_on(base_last)) {
					list.pop_back();
				} else {
					list.push_back(turn);
				}
			}
		}
		out.lines[l].headings = list;
		for (const uint32_t bp : base_piece) {
			if (bp == UNPLACED) {
				out.lines[l].crossing_piece.push_back(UNPLACED);
				continue;
			}
			out.lines[l].crossing_piece.push_back(static_cast<uint32_t>(static_cast<int>(bp) + shift));
		}
	}

	/* Rule 4: the crossings inside a site's group. */
	for (const auto &[c, g] : group_of) {
		const uint32_t a = t.crossings[c].a, b = t.crossings[c].b;
		const PlanarLine &la = t.lines[a], &lb = t.lines[b];
		const bool split = !la.from.port && !lb.from.port && t.nodes[la.from.index].group == g && t.nodes[lb.from.index].group == g;
		const uint32_t na = (split ? la.from : la.to).index, nb = (split ? lb.from : lb.to).index;
		const auto text = [&](const char *what) { return fmt::format("{}: {} x {}", what, la.name, lb.name); };
		uint32_t site = UINT32_MAX;
		for (uint32_t s = 0; s < channels.sites.size() && site == UINT32_MAX; s++) {
			if (std::ranges::find(channels.sites[s].nodes, na) != channels.sites[s].nodes.end()) site = s;
		}
		uint32_t over = UINT32_MAX;
		if (site != UINT32_MAX && sites[site].straight != UINT32_MAX) {
			if (channels.channel_of_line[a] == sites[site].straight) over = a;
			else if (channels.channel_of_line[b] == sites[site].straight) over = b;
		} else if (site != UINT32_MAX) {
			if (legs_of(na).straight == a) over = a;
			else if (legs_of(nb).straight == b) over = b;
		}
		if (over == UINT32_MAX) {
			fail("a site crossing with no straight leg: " + la.name + " x " + lb.name);
			continue;
		}
		const uint32_t under = over == a ? b : a;
		const std::vector<PlanarHeading> &ho = out.lines[over].headings, &hu = out.lines[under].headings;
		const auto first = [&](const std::vector<PlanarHeading> &list, const auto &suits) -> uint32_t {
			for (uint32_t i = 0; i < list.size(); i++) {
				const uint32_t p = split ? i : static_cast<uint32_t>(list.size()) - 1 - i;
				if (suits(list[p])) return p;
			}
			return UNPLACED;
		};
		const uint32_t po = first(ho, [](PlanarHeading h) { return static_cast<int>(h) % 2 == 0; });
		const uint32_t pu = po == UNPLACED ? UNPLACED : first(hu, [&](PlanarHeading h) { return static_cast<int>(h) % 4 != static_cast<int>(ho[po]) % 4; });
		if (po == UNPLACED || pu == UNPLACED) {
			fail(text("no piece suits site crossing"));
			continue;
		}
		for (const auto &[line, piece] : {std::pair{over, po}, std::pair{under, pu}}) {
			const auto k = std::ranges::find(events[line], c);
			if (k != events[line].end()) out.lines[line].crossing_piece[k - events[line].begin()] = piece;
		}
	}

	/* Rule 5: the event pieces do not decrease. */
	for (uint32_t l = 0; l < t.lines.size(); l++) {
		const std::vector<uint32_t> &pieces = out.lines[l].crossing_piece;
		for (size_t k = 1; k < pieces.size(); k++) {
			if (pieces[k] < pieces[k - 1]) {
				fail("events out of order on " + t.lines[l].name);
				break;
			}
		}
	}

	/* Rule 6: where each copy and row runs beside its parent. */
	for (uint32_t i = 0; i < map.tracks.size(); i++) {
		if (map.tracks[i].kind == PlanarTrack::Kind::Line) continue;
		const uint32_t parent = map.tracks[i].line;
		const uint32_t last_piece = static_cast<uint32_t>(out.lines[parent].headings.size()) - 1;
		const auto piece_of = [&](uint32_t crossing) {
			const PlanarCrossing &x = map.crossings[crossing];
			const uint32_t partner = map.tracks[x.a == i ? x.b : x.a].line;
			for (size_t k = 0; k < events[parent].size(); k++) {
				const PlanarCrossing &px = t.crossings[events[parent][k]];
				if ((px.a == parent ? px.b : px.a) == partner) return out.lines[parent].crossing_piece[k];
			}
			fail(fmt::format("{} mirrors no event of {}", map.tracks[i].name, t.lines[parent].name));
			return last_piece;
		};
		const std::vector<uint32_t> &own = map.events[i];
		const uint32_t first = own.empty() ? last_piece : piece_of(own.front());
		const uint32_t last = map.tracks[i].kind == PlanarTrack::Kind::Row || own.empty() ? last_piece : piece_of(own.back());
		out.beside[i] = {parent, first, last};
	}
	return out;
}
