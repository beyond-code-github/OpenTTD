/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_signal_adapter.cpp The old pipeline's graph, programs and topology read into the signal pass's network. */

#include "../../stdafx.h"

#include "junction_signal_adapter.h"

#include <algorithm>
#include <optional>

#include "../../safeguards.h"

std::vector<std::pair<bool, size_t>> JunctionRouteEvents(const JunctionTrackGraph &graph, const JunctionTrackPrograms &programs, const std::vector<JunctionTrackRoute> &routes, size_t r)
{
	const JunctionTrackRoute &route = routes[r];
	std::vector<std::pair<bool, size_t>> events;
	/* The node piece at each boundary between consecutive edges: a split's belongs to the out-edge, a merge's to the in-edge. */
	for (size_t i = 1; i < route.tags.size(); i++) {
		if (route.tags[i].edge == route.tags[i - 1].edge) continue;
		const JunctionTrackEdge &in = graph.edges[route.tags[i - 1].edge];
		if (!in.to.node) continue;
		const bool split = graph.nodes[in.to.index].split;
		events.emplace_back(split, split ? i : i - 1);
	}
	/* The lane split and merge of each doubled edge: switches both copies pass, located by the copy that bends there. */
	for (const uint32_t e : graph.path_edges[route.path]) {
		if (programs.copies[e].empty()) continue;
		size_t sibling = routes.size();
		for (size_t s = 0; s < routes.size(); s++) {
			if (routes[s].path == route.path && routes[s].copy != route.copy) sibling = s;
		}
		if (sibling == routes.size()) continue;
		const JunctionTrackRoute &copy0 = route.copy == 0 ? route : routes[sibling];
		const JunctionTrackRoute &copy1 = route.copy == 0 ? routes[sibling] : route;
		const auto with_role = [](const std::vector<JunctionTrackStep> &program, JunctionTrackStep::Role role) {
			for (size_t t = 0; t < program.size(); t++) {
				if (program[t].kind == JunctionTrackStep::Kind::SBend && program[t].role == role) return t;
			}
			return program.size();
		};
		const size_t split_token = with_role(programs.copies[e], JunctionTrackStep::Role::LaneSplit);
		const size_t merge_token = with_role(programs.edges[e], JunctionTrackStep::Role::LaneMerge);
		const auto on_tile = [&](TileIndexDiffC tile) -> std::optional<size_t> {
			for (size_t k = 0; k < route.pieces.size(); k++) {
				if (!route.pieces[k].on_bridge && route.pieces[k].tile.x == tile.x && route.pieces[k].tile.y == tile.y) return k;
			}
			return std::nullopt;
		};
		for (size_t i = 0; i < copy1.tags.size(); i++) {
			if (copy1.tags[i].edge != e || copy1.tags[i].copy != 1 || copy1.tags[i].token != split_token) continue;
			if (const std::optional<size_t> k = on_tile(copy1.pieces[i].tile); k.has_value()) events.emplace_back(true, *k);
			break;
		}
		for (size_t i = copy0.tags.size(); i-- > 0;) {
			if (copy0.tags[i].edge != e || copy0.tags[i].copy != 0 || copy0.tags[i].token != merge_token) continue;
			if (const std::optional<size_t> k = on_tile(copy0.pieces[i].tile); k.has_value()) events.emplace_back(false, *k);
			break;
		}
	}
	std::ranges::stable_sort(events, [](const std::pair<bool, size_t> &a, const std::pair<bool, size_t> &b) { return a.second != b.second ? a.second < b.second : a.first > b.first; });
	events.erase(std::unique(events.begin(), events.end()), events.end());
	return events;
}
JunctionSignalNetwork MakeJunctionSignalNetwork(const std::vector<JunctionTrackRoute> &routes, const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, const JunctionTrackPrograms &programs)
{
	JunctionSignalNetwork network;
	for (size_t r = 0; r < routes.size(); r++) {
		const JunctionTrackRoute &route = routes[r];
		JunctionSignalRoute out;
		const auto &movement = topology.movements[paths[route.path].movement];
		out.main = topology.groups[movement.first].main && topology.groups[movement.second].main;
		out.group = paths[route.path].movement;
		out.switches = JunctionRouteEvents(graph, programs, routes, r);
		for (const auto &[split, j] : out.switches) {
			if (split) continue;
			if (j + 1 >= route.pieces.size() || route.tags[j].edge == route.tags[j + 1].edge || !graph.edges[route.tags[j].edge].to.node) continue;
			out.merge_nodes.emplace_back(j, graph.edges[route.tags[j].edge].to.index);
		}
		for (const uint32_t e : graph.path_edges[route.path]) {
			if (!graph.edges[e].to.node) continue;
			const uint32_t node = graph.edges[e].to.index;
			if (std::ranges::find(out.arrivals, node, &std::pair<uint32_t, size_t>::first) != out.arrivals.end()) continue;
			for (size_t k = route.tags.size(); k-- > 0;) {
				if (route.tags[k].edge != e) continue;
				out.arrivals.emplace_back(node, k);
				break;
			}
		}
		network.routes.push_back(std::move(out));
	}
	return network;
}

std::vector<JunctionTrackSignal> SignalJunctionTrackRoutes(JunctionLayout &layout, const std::vector<JunctionTrackRoute> &routes, const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, const JunctionTrackPrograms &programs, const JunctionSpec &spec, std::string *why, std::vector<std::pair<TileIndexDiffC, TileIndexDiffC>> *prio_beside, const std::vector<JunctionPrioSpan> *prio)
{
	return SignalJunctionTrackRoutes(layout, routes, MakeJunctionSignalNetwork(routes, topology, paths, graph, programs), spec, why, prio_beside, prio);
}
