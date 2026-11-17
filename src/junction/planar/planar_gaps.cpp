/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file planar_gaps.cpp Gap steps from the channel graph. */

#include "../../stdafx.h"

#include "planar_gaps.h"

#include <algorithm>

#include "../../safeguards.h"

/** Whether a site is composed per node: a lane split or merge, or a site with other than two channels. */
static bool SitePerNode(const PlanarSite &site)
{
	return site.kind == PlanarSiteKind::LaneSplit || site.kind == PlanarSiteKind::LaneMerge || site.others.size() != 2;
}

/** The heading of the single port in a set, or -1. */
static int PortHeading(const PlanarTopology &topology, const std::set<uint32_t> &ports)
{
	return ports.size() == 1 ? static_cast<int>(topology.ports[*ports.begin()].heading) : -1;
}

/** A turn in eighths, -4..3, positive clockwise. */
static int TurnOf(int from, int to)
{
	return (to - from + 12) % 8 - 4;
}

/**
 * Decide each site's straight channel, other channel, side and prio, as the mapped composer does.
 * @param topology The topology.
 * @param channels Its channel graph.
 * @param ties Bit i set: the i-th site without a straight channel takes its second other channel as straight.
 * @param why Receives the reason when the result is empty.
 * @return One entry per site of the channel graph, in its order.
 */
std::vector<PlanarResolvedSite> ResolvePlanarSites(const PlanarTopology &topology, const PlanarChannels &channels, uint64_t ties, std::string &why)
{
	std::vector<PlanarResolvedSite> res(channels.sites.size());
	int tie = 0;
	for (size_t s = 0; s < channels.sites.size(); s++) {
		const PlanarSite &site = channels.sites[s];
		if (SitePerNode(site)) continue;
		uint32_t straight = site.straight;
		if (straight == UINT32_MAX) straight = site.others[(ties >> tie++ & 1) != 0 ? 1 : 0];
		const uint32_t other = site.others[0] == straight ? site.others[1] : site.others[0];
		const bool split = site.kind == PlanarSiteKind::Fray;
		const PlanarChannel &key = channels.channels[site.channel];
		const int from = split ? PortHeading(topology, key.up) : PortHeading(topology, channels.channels[other].up);
		const int to = split ? PortHeading(topology, channels.channels[other].down) : PortHeading(topology, key.down);
		if (from < 0 || to < 0) {
			why = "a site's channels have no single port heading";
			return {};
		}
		res[s] = {straight, other, static_cast<int8_t>(TurnOf(from, to) > 0 ? 1 : -1), channels.channels[straight].prio};
	}
	return res;
}

/** Whether a port line is an in line. */
static bool IsInLine(const PlanarTopology &topology, uint32_t line)
{
	return topology.lines[line].from.port;
}

/** The side of the box a port line is on. */
static PlanarSide SideOfLine(const PlanarTopology &topology, uint32_t line)
{
	const PlanarLine &l = topology.lines[line];
	return topology.ports[l.from.port ? l.from.index : l.to.index].side;
}

/**
 * The sideways steps the pinned gaps between neighbouring port lines of different channels need.
 * @param topology The topology.
 * @param channels Its channel graph.
 * @param sites The resolved sites of the channel graph.
 * @param why Receives the reason when the result is empty.
 * @return The steps, sorted by channel then fan (false first), without duplicates.
 */
std::vector<PlanarGapStep> BuildPlanarGapSteps(const PlanarTopology &topology, const PlanarChannels &channels, const std::vector<PlanarResolvedSite> &sites, std::string &why)
{
	std::vector<PlanarGapStep> steps;
	const auto resolved = [&](PlanarSiteKind kind, uint32_t channel) -> const PlanarResolvedSite * {
		for (size_t s = 0; s < channels.sites.size(); s++) {
			if (channels.sites[s].kind == kind && channels.sites[s].channel == channel) return sites[s].straight == UINT32_MAX ? nullptr : &sites[s];
		}
		return nullptr;
	};

	/* Look at the gap from x's line; y's line is its ring successor (successor) or predecessor. Returns false on a refusal. */
	const auto look = [&](uint32_t x_line, uint32_t y_line, bool successor) {
		const bool x_in = IsInLine(topology, x_line);
		const uint32_t x = channels.channel_of_line[x_line], y = channels.channel_of_line[y_line];
		const int toward = (x_in == successor) ? -1 : +1;
		if (x_in) {
			const PlanarResolvedSite *r = resolved(PlanarSiteKind::Fray, x);
			if (r == nullptr || r->side != toward) return true;
			if (r->prio) {
				steps.push_back({x, true, y});
			} else if (!IsInLine(topology, y_line)) {
				steps.push_back({y, false, x});
			} else {
				why = "a plain fray's leg runs toward a neighbouring in port";
				return false;
			}
		} else {
			const PlanarResolvedSite *r = resolved(PlanarSiteKind::Join, x);
			if (r != nullptr && r->side == toward) steps.push_back({x, false, y});
		}
		return true;
	};

	for (size_t i = 0; i + 1 < topology.ring.size(); i++) {
		const uint32_t a = topology.ring[i], b = topology.ring[i + 1];
		if (SideOfLine(topology, a) != SideOfLine(topology, b)) continue;
		if (channels.channel_of_line[a] == channels.channel_of_line[b]) continue;
		if (!look(a, b, true) || !look(b, a, false)) return {};
	}

	std::ranges::stable_sort(steps, [](const PlanarGapStep &p, const PlanarGapStep &q) {
		return p.channel != q.channel ? p.channel < q.channel : p.fan < q.fan;
	});
	const auto dup = std::ranges::unique(steps, [](const PlanarGapStep &p, const PlanarGapStep &q) { return p.channel == q.channel && p.fan == q.fan; });
	steps.erase(dup.begin(), dup.end());
	return steps;
}
