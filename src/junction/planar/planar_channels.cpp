/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file planar_channels.cpp The channel graph of a planar topology (D305, D306). */

#include "../../stdafx.h"

#include "planar_channels.h"

#include <algorithm>
#include <climits>
#include <functional>
#include <optional>
#include <map>
#include <set>

#include "../../safeguards.h"

namespace {

/** What a line is known by while channels are refined: the ports it comes from and goes to, its depth, its crossed channels in order. */
struct LineKey {
	std::set<uint32_t> up; ///< In ports whose trains reach the line.
	std::set<uint32_t> down; ///< Out ports the line's trains reach.
	uint32_t depth; ///< Lines on the longest path from an in port line (in port lines 0).
	std::vector<uint32_t> crossed; ///< The current channel of each line crossed between channels, in travel order.

	auto operator<=>(const LineKey &) const = default;
};

/** Per line facts derived from the node graph once. */
struct LineFacts {
	const PlanarTopology &t;
	std::vector<std::set<uint32_t>> up, down;
	std::vector<uint32_t> depth;
	std::vector<std::vector<uint32_t>> rank; ///< Left to right order: the leftmost in leg's rank, then the leg position.
	std::vector<bool> done_up, done_down;

	explicit LineFacts(const PlanarTopology &t) : t(t), up(t.lines.size()), down(t.lines.size()), depth(t.lines.size()), rank(t.lines.size()), done_up(t.lines.size()), done_down(t.lines.size())
	{
		for (uint32_t l = 0; l < t.lines.size(); l++) {
			this->Up(l);
			this->Down(l);
		}
	}

	/** The in ports reaching a line, its depth and its rank (memoised over the lines arriving at its start). */
	void Up(uint32_t l)
	{
		if (this->done_up[l]) return;
		this->done_up[l] = true;
		const PlanarEnd &from = this->t.lines[l].from;
		if (from.port) {
			const PlanarPort &port = this->t.ports[from.index];
			this->up[l] = {from.index};
			this->depth[l] = 0;
			this->rank[l] = {static_cast<uint32_t>(std::ranges::find(port.lines, l) - port.lines.begin())};
			return;
		}
		const PlanarNode &node = this->t.nodes[from.index];
		uint32_t d = 0;
		for (uint32_t i : node.in) {
			this->Up(i);
			this->up[l].insert(this->up[i].begin(), this->up[i].end());
			d = std::max(d, this->depth[i] + 1);
		}
		this->depth[l] = d;
		this->rank[l] = this->rank[node.in.front()];
		this->rank[l].push_back(static_cast<uint32_t>(std::ranges::find(node.out, l) - node.out.begin()));
	}

	/** The out ports a line reaches (memoised over the lines leaving its end). */
	void Down(uint32_t l)
	{
		if (this->done_down[l]) return;
		this->done_down[l] = true;
		const PlanarEnd &to = this->t.lines[l].to;
		if (to.port) {
			this->down[l] = {to.index};
			return;
		}
		for (uint32_t o : this->t.nodes[to.index].out) {
			this->Down(o);
			this->down[l].insert(this->down[o].begin(), this->down[o].end());
		}
	}
};

/** A turn from one heading to another in eighths, -4..3; positive is clockwise (to the right). */
int Turn(PlanarHeading from, PlanarHeading to)
{
	return (static_cast<int>(to) - static_cast<int>(from) + 12) % 8 - 4;
}

/** The node group at one end of a line, or UINT32_MAX at a port. */
uint32_t GroupAt(const PlanarTopology &t, const PlanarEnd &e)
{
	return e.port ? UINT32_MAX : t.nodes[e.index].group;
}

} // namespace

/**
 * Group a topology's lines into channels (D305, D306).
 * Lines start in one class per (in ports, out ports, depth); a class splits until its lines cross the same classes in the same
 * order. A crossing whose two lines start or end at the same node group belongs to that group (a fray's or join's own crossings),
 * every other crossing to the two channels.
 * @param topology A parsed topology.
 * @return Its channels, their order and the crossings between them.
 */
PlanarChannels BuildPlanarChannels(const PlanarTopology &topology)
{
	const PlanarTopology &t = topology;
	PlanarChannels out;
	const LineFacts facts(t);

	uint32_t groups = 0;
	for (const PlanarNode &n : t.nodes) groups = std::max(groups, n.group + 1);
	out.group_crossings.resize(groups);
	std::vector<bool> internal(t.crossings.size(), false);
	for (uint32_t c = 0; c < t.crossings.size(); c++) {
		const PlanarLine &a = t.lines[t.crossings[c].a];
		const PlanarLine &b = t.lines[t.crossings[c].b];
		uint32_t g = UINT32_MAX;
		if (GroupAt(t, a.from) != UINT32_MAX && GroupAt(t, a.from) == GroupAt(t, b.from)) g = GroupAt(t, a.from);
		else if (GroupAt(t, a.to) != UINT32_MAX && GroupAt(t, a.to) == GroupAt(t, b.to)) g = GroupAt(t, a.to);
		if (g == UINT32_MAX) continue;
		internal[c] = true;
		out.group_crossings[g].push_back(c);
	}

	/* Refine classes until stable. */
	std::vector<uint32_t> cls(t.lines.size(), 0);
	size_t count = 0;
	for (;;) {
		std::map<LineKey, uint32_t> ids;
		std::vector<uint32_t> next(t.lines.size());
		for (uint32_t l = 0; l < t.lines.size(); l++) {
			LineKey key{facts.up[l], facts.down[l], facts.depth[l], {}};
			for (uint32_t c : t.events[l]) {
				if (internal[c]) continue;
				const uint32_t other = t.crossings[c].a == l ? t.crossings[c].b : t.crossings[c].a;
				key.crossed.push_back(cls[other]);
			}
			auto it = ids.try_emplace(std::move(key), static_cast<uint32_t>(ids.size())).first;
			next[l] = it->second;
		}
		const bool stable = ids.size() == count;
		count = ids.size();
		cls = std::move(next);
		if (stable) break;
	}

	out.channels.resize(count);
	out.channel_of_line = cls;
	for (uint32_t l = 0; l < t.lines.size(); l++) out.channels[cls[l]].lines.push_back(l);
	for (PlanarChannel &ch : out.channels) {
		std::ranges::sort(ch.lines, [&](uint32_t a, uint32_t b) { return facts.rank[a] < facts.rank[b]; });
	}

	/* Channel crossings: the line crossings between each pair of channels, then each channel's in its first line's event order. */
	std::map<std::pair<uint32_t, uint32_t>, uint32_t> pair_index;
	for (uint32_t c = 0; c < t.crossings.size(); c++) {
		if (internal[c]) continue;
		uint32_t a = cls[t.crossings[c].a], b = cls[t.crossings[c].b];
		if (a > b) std::swap(a, b);
		auto [it, added] = pair_index.try_emplace({a, b}, static_cast<uint32_t>(out.crossings.size()));
		if (added) out.crossings.push_back({a, b, {}});
		out.crossings[it->second].line_crossings.push_back(c);
	}
	for (uint32_t ci = 0; ci < out.channels.size(); ci++) {
		PlanarChannel &ch = out.channels[ci];
		ch.up = facts.up[ch.lines.front()];
		ch.down = facts.down[ch.lines.front()];
		for (uint32_t l : ch.lines) {
			for (uint32_t r : t.lines[l].routes) ch.prio |= t.routes[r].prio;
		}
		for (uint32_t c : t.events[ch.lines.front()]) {
			if (internal[c]) continue;
			uint32_t a = cls[t.crossings[c].a], b = cls[t.crossings[c].b];
			if (a > b) std::swap(a, b);
			const uint32_t x = pair_index.at({a, b});
			if (ch.crossings.empty() || ch.crossings.back() != x) ch.crossings.push_back(x);
		}
	}

	/* Sites: the splits leaving one channel, the merges entering one channel. */
	std::map<std::pair<bool, uint32_t>, uint32_t> site_index;
	for (uint32_t n = 0; n < t.nodes.size(); n++) {
		const PlanarNode &node = t.nodes[n];
		const bool split = node.IsSplit();
		const uint32_t key = cls[split ? node.in.front() : node.out.front()];
		auto [it, added] = site_index.try_emplace({!split, key}, static_cast<uint32_t>(out.sites.size()));
		if (added) out.sites.push_back({PlanarSiteKind::Fray, key, {}, {}});
		PlanarSite &site = out.sites[it->second];
		site.nodes.push_back(n);
		for (uint32_t l : split ? node.out : node.in) {
			if (std::ranges::find(site.others, cls[l]) == site.others.end()) site.others.push_back(cls[l]);
		}
	}
	for (PlanarSite &site : out.sites) {
		const bool split = t.nodes[site.nodes.front()].IsSplit();
		const PlanarChannel &key = out.channels[site.channel];
		bool differ = false;
		for (uint32_t o : site.others) differ |= split ? out.channels[o].down != out.channels[site.others.front()].down : out.channels[o].up != out.channels[site.others.front()].up;
		site.kind = split ? (differ ? PlanarSiteKind::Fray : PlanarSiteKind::LaneSplit) : (differ ? PlanarSiteKind::Join : PlanarSiteKind::LaneMerge);
		/* The other channel's turn: from the key channel's in port heading to its out port heading (a split), or from its in port heading to the key channel's out port heading (a merge). */
		const auto turn = [&](uint32_t o) -> std::optional<int> {
			const std::set<uint32_t> &from = split ? key.up : out.channels[o].up;
			const std::set<uint32_t> &to = split ? out.channels[o].down : key.down;
			if (from.size() != 1 || to.size() != 1) return std::nullopt;
			return Turn(t.ports[*from.begin()].heading, t.ports[*to.begin()].heading);
		};
		std::vector<uint32_t> prio;
		for (uint32_t o : site.others) if (out.channels[o].prio) prio.push_back(o);
		if (prio.size() == 1) {
			site.straight = prio.front();
		} else {
			/* Of the prio channels (or of all, with none), the one turning least. */
			uint32_t best = UINT32_MAX;
			int best_turn = INT_MAX;
			bool tie = false;
			for (uint32_t o : prio.empty() ? site.others : prio) {
				const std::optional<int> tr = turn(o);
				if (!tr.has_value()) { tie = true; continue; }
				if (std::abs(*tr) < best_turn) { best = o; best_turn = std::abs(*tr); tie = false; }
				else if (std::abs(*tr) == best_turn) tie = true;
			}
			if (!tie) site.straight = best;
		}
		if (site.straight != UINT32_MAX) {
			site.prio = out.channels[site.straight].prio;
			if (site.others.size() == 2) {
				const uint32_t other = site.others[0] == site.straight ? site.others[1] : site.others[0];
				const std::optional<int> tr = turn(other);
				if (tr.has_value()) site.side = static_cast<int8_t>(*tr > 0 ? 1 : (*tr < 0 ? -1 : 0));
			}
		}
	}

	/* Node legs: a split's straight leg is in its site's straight channel (legs to one port fork); a merge's is its prio line, else the line turning least. */
	const auto line_prio = [&](uint32_t l) {
		for (uint32_t r : t.lines[l].routes) {
			if (t.routes[r].prio) return true;
		}
		return false;
	};
	const auto heading = [&](const std::set<uint32_t> &ports) -> std::optional<PlanarHeading> {
		if (ports.size() != 1) return std::nullopt;
		return t.ports[*ports.begin()].heading;
	};
	out.node_legs.resize(t.nodes.size());
	for (const PlanarSite &site : out.sites) {
		for (uint32_t n : site.nodes) {
			const PlanarNode &node = t.nodes[n];
			PlanarNodeLegs &legs = out.node_legs[n];
			const bool split = node.IsSplit();
			const std::vector<uint32_t> &two = split ? node.out : node.in;
			if (two.size() != 2) continue;
			const auto turn = [&](uint32_t l) -> std::optional<int> {
				const std::optional<PlanarHeading> from = heading(split ? facts.up[node.in.front()] : facts.up[l]);
				const std::optional<PlanarHeading> to = heading(split ? facts.down[l] : facts.down[node.out.front()]);
				if (!from.has_value() || !to.has_value()) return std::nullopt;
				return Turn(*from, *to);
			};
			if (split) {
				if (site.straight != UINT32_MAX) {
					legs.straight = cls[two[0]] == site.straight ? two[0] : two[1];
					legs.side = site.side;
				}
				continue;
			}
			std::vector<uint32_t> prio;
			for (uint32_t l : two) {
				if (line_prio(l)) prio.push_back(l);
			}
			if (prio.size() == 1) {
				legs.straight = prio.front();
			} else {
				const std::optional<int> t0 = turn(two[0]), t1 = turn(two[1]);
				if (t0.has_value() && t1.has_value() && std::abs(*t0) != std::abs(*t1)) legs.straight = std::abs(*t0) < std::abs(*t1) ? two[0] : two[1];
			}
			if (legs.straight == UINT32_MAX) continue;
			const uint32_t other = two[0] == legs.straight ? two[1] : two[0];
			const std::optional<int> to = turn(other);
			if (to.has_value() && *to != 0) {
				legs.side = static_cast<int8_t>(*to > 0 ? 1 : -1);
			} else {
				legs.side = static_cast<int8_t>(other == two[1] ? 1 : -1); // the node lists its lines left to right across travel
			}
		}
	}

	/*
	 * Line shares: an in port line carries a full lane; a fray's legs keep their in line's share, a lane split's k legs
	 * take 1/k each (round robin), a merge's out line the larger of its in lines'. A line of a movement without a prio
	 * route (sideline traffic, when the topology lists prio routes) counts half.
	 */
	std::vector<PlanarSiteKind> node_kind(t.nodes.size(), PlanarSiteKind::Fray);
	for (const PlanarSite &site : out.sites) {
		for (uint32_t n : site.nodes) node_kind[n] = site.kind;
	}
	std::set<char> main_letters;
	bool any_prio = false;
	for (const PlanarRoute &r : t.routes) {
		if (r.prio && !r.name.empty()) main_letters.insert(r.name[0]);
		any_prio |= r.prio;
	}
	out.line_share.assign(t.lines.size(), -1.0);
	std::function<double(uint32_t)> flow = [&](uint32_t l) -> double {
		if (out.line_share[l] >= 0) return out.line_share[l];
		double s = 1.0;
		if (!t.lines[l].from.port) {
			const PlanarNode &node = t.nodes[t.lines[l].from.index];
			if (node.IsSplit()) {
				s = flow(node.in.front());
				if (node_kind[t.lines[l].from.index] == PlanarSiteKind::LaneSplit) s /= static_cast<double>(node.out.size());
			} else {
				s = 0.0;
				for (uint32_t i : node.in) s = std::max(s, flow(i));
			}
		}
		return out.line_share[l] = s;
	};
	for (uint32_t l = 0; l < t.lines.size(); l++) flow(l);
	for (uint32_t l = 0; l < t.lines.size(); l++) {
		bool main = !any_prio;
		for (uint32_t r : t.lines[l].routes) main |= !t.routes[r].name.empty() && main_letters.contains(t.routes[r].name[0]);
		if (!main) out.line_share[l] /= 2.0;
	}
	return out;
}
