/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_topology.cpp The topology of a junction type, its lane-level paths and the chord crossing rule (DESIGN-G1 §1, §2). */

#include "stdafx.h"

#include "junction_topology.h"

#include "junction_generator.h"
#include "junction_route.h"

#include <algorithm>

#include "safeguards.h"

std::vector<JunctionPortSlot> JunctionPortSlots(const JunctionTopology &topology)
{
	/* Counter-clockwise from the north-west corner: W, S, E, N. */
	static const JunctionSide order[] = {JunctionSide::W, JunctionSide::S, JunctionSide::E, JunctionSide::N};
	std::vector<JunctionPortSlot> slots;
	size_t side_index = 0;
	for (size_t g = 0; g < topology.groups.size(); g++) {
		const JunctionLaneGroup &group = topology.groups[g];
		while (side_index < 4 && order[side_index] != group.side) side_index++;
		if (side_index == 4) return {};
		const bool reversed = group.side == JunctionSide::E || group.side == JunctionSide::N;
		for (uint8_t i = 0; i < group.lanes; i++) {
			slots.push_back({static_cast<uint8_t>(g), static_cast<uint8_t>(reversed ? group.lanes - 1 - i : i)});
		}
	}
	return slots;
}

std::vector<JunctionLanePath> ExpandJunctionLanes(const JunctionTopology &topology, std::string *error)
{
	std::vector<JunctionPortSlot> slots = JunctionPortSlots(topology);
	auto fail = [&](const char *reason) {
		if (error != nullptr) *error = reason;
		return std::vector<JunctionLanePath>();
	};
	if (slots.empty()) return fail("groups are not in slot order");

	auto slot_of = [&](uint8_t group, uint8_t lane) -> uint16_t {
		for (size_t s = 0; s < slots.size(); s++) {
			if (slots[s].group == group && slots[s].lane == lane) return static_cast<uint16_t>(s);
		}
		return 0;
	};

	std::vector<JunctionLanePath> paths;
	for (size_t m = 0; m < topology.movements.size(); m++) {
		const JunctionLaneGroup &in = topology.groups[topology.movements[m].first];
		const JunctionLaneGroup &out = topology.groups[topology.movements[m].second];
		const uint8_t in_group = topology.movements[m].first;
		const uint8_t out_group = topology.movements[m].second;
		if (in.main && out.main && in.line == out.line) {
			if (in.lanes != out.lanes) return fail("a through movement needs equal lane counts");
			for (uint8_t i = 0; i < in.lanes; i++) {
				paths.push_back({slot_of(in_group, i), slot_of(out_group, i), static_cast<uint16_t>(m)});
			}
		} else {
			if (in.lanes > 1 && out.lanes > 1) return fail("multi-to-multi movements are not ruled yet");
			for (uint8_t i = 0; i < in.lanes; i++) {
				for (uint8_t o = 0; o < out.lanes; o++) {
					paths.push_back({slot_of(in_group, i), slot_of(out_group, o), static_cast<uint16_t>(m)});
				}
			}
		}
	}
	return paths;
}

std::vector<std::pair<uint32_t, uint32_t>> JunctionChordCrossings(const std::vector<JunctionLanePath> &paths)
{
	std::vector<std::pair<uint32_t, uint32_t>> crossings;
	for (uint32_t i = 0; i < paths.size(); i++) {
		const uint16_t a0 = std::min(paths[i].from, paths[i].to);
		const uint16_t a1 = std::max(paths[i].from, paths[i].to);
		for (uint32_t j = i + 1; j < paths.size(); j++) {
			const uint16_t b0 = paths[j].from;
			const uint16_t b1 = paths[j].to;
			if (b0 == a0 || b0 == a1 || b1 == a0 || b1 == a1) continue;
			const bool in0 = a0 < b0 && b0 < a1;
			const bool in1 = a0 < b1 && b1 < a1;
			if (in0 != in1) crossings.emplace_back(i, j);
		}
	}
	return crossings;
}

std::vector<int> JunctionSlotRows(const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, std::string *error)
{
	const std::vector<JunctionPortSlot> slots = JunctionPortSlots(topology);
	const int n = static_cast<int>(slots.size());
	auto group_of = [&](int slot) -> const JunctionLaneGroup & { return topology.groups[slots[slot].group]; };
	std::vector<int> row_of(n, -1);
	int rows = 0;
	for (int s = 0; s < n; s++) {
		if (group_of(s).main && group_of(s).side == JunctionSide::W) row_of[s] = rows++;
	}
	for (int s = 0; s < n; s++) {
		if (!group_of(s).main || row_of[s] >= 0) continue;
		for (const JunctionLanePath &path : paths) {
			const int other = path.from == s ? path.to : (path.to == s ? path.from : -1);
			if (other >= 0 && group_of(other).main && group_of(other).side == JunctionSide::W) {
				row_of[s] = row_of[other];
				break;
			}
		}
		if (row_of[s] < 0) {
			if (error != nullptr) *error = "a main slot without a through partner is not ruled yet";
			return {};
		}
	}
	return row_of;
}

/** Builds the split or merge trees of a track graph (DESIGN-G1 §7). */
struct JunctionTreeBuilder {
	const std::vector<JunctionPortSlot> &slots;
	const std::vector<JunctionLanePath> &paths;
	bool split; ///< Building split trees, else merge trees.
	std::vector<JunctionTrackNode> nodes;
	std::vector<JunctionTrackEdge> edges;
	std::vector<JunctionTrackEnd> leaf; ///< Per path, its in-leaf or out-leaf in the trees built.
	std::string error;

	uint16_t Far(uint16_t p) const { return split ? paths[p].to : paths[p].from; }

	/** Splits a set of paths into keys ordered by their least counter-clockwise distance from slot s. */
	std::vector<std::vector<uint16_t>> Keys(const std::vector<uint16_t> &set, uint16_t s, bool by_slot) const
	{
		const size_t n = slots.size();
		std::vector<std::pair<std::pair<size_t, size_t>, std::vector<uint16_t>>> keyed; // ((least distance, key), paths)
		for (uint16_t p : set) {
			const size_t key = by_slot ? Far(p) : slots[Far(p)].group;
			const size_t distance = (Far(p) + n - s) % n;
			auto it = std::find_if(keyed.begin(), keyed.end(), [&](const auto &k) { return k.first.second == key; });
			if (it == keyed.end()) {
				keyed.push_back({{distance, key}, {p}});
			} else {
				it->first.first = std::min(it->first.first, distance);
				it->second.push_back(p);
			}
		}
		std::sort(keyed.begin(), keyed.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
		std::vector<std::vector<uint16_t>> result;
		for (auto &k : keyed) result.push_back(std::move(k.second));
		return result;
	}

	bool Partition(const std::vector<uint16_t> &set, bool by_slot, JunctionTrackEnd parent, uint16_t s)
	{
		std::vector<std::vector<uint16_t>> keys = this->Keys(set, s, by_slot);
		if (keys.size() == 1) {
			if (by_slot) return false; // A key of one slot holds one path.
			return this->Partition(set, true, parent, s);
		}
		if (keys.size() > 2) {
			this->error = this->split ? "more than two legs at one split are not ruled yet" : "more than two legs at one merge are not ruled yet";
			return false;
		}
		const JunctionTrackEnd node{true, static_cast<uint16_t>(this->nodes.size())};
		this->nodes.push_back({this->split, s, 0});
		if (this->split) {
			this->edges.push_back({JunctionTrackEdge::Kind::In, parent, node, set});
		} else {
			this->edges.push_back({JunctionTrackEdge::Kind::Out, node, parent, set});
		}
		for (const auto &key : keys) {
			if (key.size() == 1) {
				this->leaf[key[0]] = node;
			} else if (!this->Partition(key, by_slot, node, s)) {
				return false;
			}
		}
		return true;
	}

	/** Builds the trees of every slot ascending. */
	bool Build()
	{
		this->leaf.assign(paths.size(), {});
		for (uint16_t s = 0; s < slots.size(); s++) {
			std::vector<uint16_t> set;
			for (uint16_t p = 0; p < paths.size(); p++) {
				if ((this->split ? paths[p].from : paths[p].to) == s) set.push_back(p);
			}
			const JunctionTrackEnd port{false, s};
			if (set.size() == 1) {
				this->leaf[set[0]] = port;
			} else if (set.size() > 1 && !this->Partition(set, false, port, s)) {
				return false;
			}
		}
		return true;
	}
};

JunctionTrackGraph BuildJunctionTrackGraph(const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, std::string *error)
{
	const std::vector<JunctionPortSlot> slots = JunctionPortSlots(topology);
	const int n = static_cast<int>(slots.size());
	auto group_of = [&](int slot) -> const JunctionLaneGroup & { return topology.groups[slots[slot].group]; };
	for (int s = 0; s < n; s++) {
		if (!group_of(s).main && group_of(s).side != JunctionSide::S) {
			if (error != nullptr) *error = "branch ports off the S side are not ruled yet";
			return {};
		}
	}
	std::string rows_error;
	const std::vector<int> row_of = JunctionSlotRows(topology, paths, &rows_error);
	if (!rows_error.empty()) {
		if (error != nullptr) *error = rows_error;
		return {};
	}
	int rows = 0;
	for (int s = 0; s < n; s++) {
		if (group_of(s).main && group_of(s).side == JunctionSide::W) rows++;
	}
	JunctionTreeBuilder in_trees{slots, paths, true, {}, {}, {}, {}};
	JunctionTreeBuilder out_trees{slots, paths, false, {}, {}, {}, {}};
	if (!in_trees.Build() || !out_trees.Build()) {
		if (error != nullptr) *error = in_trees.error.empty() ? out_trees.error : in_trees.error;
		return {};
	}

	JunctionTrackGraph graph;
	graph.nodes = in_trees.nodes;
	const uint16_t out_node_base = static_cast<uint16_t>(in_trees.nodes.size());
	graph.nodes.insert(graph.nodes.end(), out_trees.nodes.begin(), out_trees.nodes.end());
	auto shift = [&](JunctionTrackEnd end) {
		if (end.node) end.index += out_node_base;
		return end;
	};

	graph.edges = in_trees.edges;
	for (uint16_t p = 0; p < paths.size(); p++) {
		graph.edges.push_back({JunctionTrackEdge::Kind::Leg, in_trees.leaf[p], shift(out_trees.leaf[p]), {p}});
	}
	for (JunctionTrackEdge edge : out_trees.edges) {
		edge.from = shift(edge.from);
		edge.to = shift(edge.to);
		graph.edges.push_back(std::move(edge));
	}

	auto carries = [](const JunctionTrackEdge &edge, uint16_t p) {
		return std::find(edge.paths.begin(), edge.paths.end(), p) != edge.paths.end();
	};
	graph.path_edges.resize(paths.size());
	for (uint16_t p = 0; p < paths.size(); p++) {
		std::vector<uint32_t> &list = graph.path_edges[p];
		for (uint32_t e = 0; e < graph.edges.size(); e++) {
			if (graph.edges[e].kind == JunctionTrackEdge::Kind::In && carries(graph.edges[e], p)) list.push_back(e);
		}
		list.push_back(static_cast<uint32_t>(in_trees.edges.size() + p));
		for (uint32_t e = static_cast<uint32_t>(graph.edges.size()); e-- > 0;) {
			if (graph.edges[e].kind == JunctionTrackEdge::Kind::Out && carries(graph.edges[e], p)) list.push_back(e);
		}
	}

	/* Levels (D204, D205). */
	for (uint32_t i = 0; i < graph.nodes.size(); i++) {
		JunctionTrackNode &node = graph.nodes[i];
		if (group_of(node.slot).main) {
			node.level = 2 * row_of[node.slot] + 1;
			continue;
		}
		if (!node.split) {
			node.level = 2 * rows;
			continue;
		}
		const JunctionTrackEdge *best = nullptr;
		for (const JunctionTrackEdge &edge : graph.edges) {
			if (((edge.from.node && edge.from.index == i) || (edge.to.node && edge.to.index == i)) && (best == nullptr || edge.paths.size() > best->paths.size())) best = &edge;
		}
		const uint8_t group = slots[paths[best->paths[0]].to].group;
		bool one_group = true;
		for (uint16_t p : best->paths) one_group = one_group && slots[paths[p].to].group == group;
		if (!one_group) {
			node.level = 2 * rows;
			continue;
		}
		int largest = 0;
		for (int s = 0; s < n; s++) {
			if (slots[s].group == group) largest = std::max(largest, row_of[s]);
		}
		node.level = 2 * (1 + largest);
	}
	auto end_level = [&](const JunctionTrackEnd &end) {
		if (end.node) return graph.nodes[end.index].level;
		return group_of(end.index).main ? 2 * row_of[end.index] + 1 : 2 * rows + 1;
	};
	auto meet = [&](uint32_t i, uint32_t j) {
		const int ai = end_level(graph.edges[i].from);
		const int bi = end_level(graph.edges[i].to);
		const int aj = end_level(graph.edges[j].from);
		const int bj = end_level(graph.edges[j].to);
		const bool row_i = ai == bi && ai % 2 == 1;
		const bool row_j = aj == bj && aj % 2 == 1;
		const int lo_i = std::min(ai, bi);
		const int hi_i = std::max(ai, bi);
		const int lo_j = std::min(aj, bj);
		const int hi_j = std::max(aj, bj);
		if (row_i && row_j) return false;
		if (row_i) return lo_j < ai && ai < hi_j;
		if (row_j) return lo_i < aj && aj < hi_i;
		const int low = std::max(lo_i, lo_j);
		return low + low % 2 <= std::min(hi_i, hi_j);
	};

	/* Crossings: the biggest edge pairs first, each chord pair covered once. */
	const std::vector<std::pair<uint32_t, uint32_t>> chords = JunctionChordCrossings(paths);
	auto is_chord = [&](uint16_t p, uint16_t q) {
		return std::binary_search(chords.begin(), chords.end(), std::make_pair<uint32_t, uint32_t>(std::min(p, q), std::max(p, q)));
	};
	std::vector<std::pair<uint32_t, uint32_t>> candidates;
	for (uint32_t i = 0; i < graph.edges.size(); i++) {
		for (uint32_t j = i + 1; j < graph.edges.size(); j++) {
			bool all = true;
			for (uint16_t p : graph.edges[i].paths) {
				for (uint16_t q : graph.edges[j].paths) all = all && p != q && is_chord(p, q);
			}
			if (all && meet(i, j)) candidates.emplace_back(i, j);
		}
	}
	auto weight = [&](const std::pair<uint32_t, uint32_t> &c) { return graph.edges[c.first].paths.size() * graph.edges[c.second].paths.size(); };
	std::sort(candidates.begin(), candidates.end(), [&](const auto &a, const auto &b) {
		if (weight(a) != weight(b)) return weight(a) > weight(b);
		return a < b;
	});
	std::vector<std::vector<bool>> covered(paths.size(), std::vector<bool>(paths.size(), false));
	for (const auto &c : candidates) {
		bool free = true;
		for (uint16_t p : graph.edges[c.first].paths) {
			for (uint16_t q : graph.edges[c.second].paths) free = free && !covered[std::min(p, q)][std::max(p, q)];
		}
		if (!free) continue;
		for (uint16_t p : graph.edges[c.first].paths) {
			for (uint16_t q : graph.edges[c.second].paths) covered[std::min(p, q)][std::max(p, q)] = true;
		}
		graph.crossings.push_back(c);
	}
	std::sort(graph.crossings.begin(), graph.crossings.end());
	return graph;
}

JunctionTrackOrder OrderJunctionTrackGraph(const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, std::string *error)
{
	auto fail = [&](const char *reason) {
		if (error != nullptr) *error = reason;
		return JunctionTrackOrder{};
	};
	const std::vector<JunctionPortSlot> slots = JunctionPortSlots(topology);
	const int n = static_cast<int>(slots.size());
	auto group_of = [&](int slot) -> const JunctionLaneGroup & { return topology.groups[slots[slot].group]; };

	/* Rows and levels. */
	const std::vector<int> row_of = JunctionSlotRows(topology, paths, error);
	if (row_of.empty()) return JunctionTrackOrder{};
	int rows = 0;
	for (int s = 0; s < n; s++) {
		if (group_of(s).main && group_of(s).side == JunctionSide::W) rows++;
	}
	const int south_level = 2 * rows + 1;
	std::vector<int> node_level;
	for (const JunctionTrackNode &node : graph.nodes) node_level.push_back(node.level);
	auto end_level = [&](const JunctionTrackEnd &end) {
		if (end.node) return node_level[end.index];
		return group_of(end.index).main ? 2 * row_of[end.index] + 1 : south_level;
	};

	/* Row edges and crossing edges. */
	const uint32_t edge_count = static_cast<uint32_t>(graph.edges.size());
	auto is_row_edge = [&](uint32_t e) {
		const int level = end_level(graph.edges[e].from);
		return level == end_level(graph.edges[e].to) && level % 2 == 1;
	};

	/* Band crossings. */
	std::vector<std::vector<std::pair<uint32_t, uint32_t>>> band_crossings(2 * rows + 1);
	for (const auto &[i, j] : graph.crossings) {
		if (is_row_edge(i) || is_row_edge(j)) continue;
		int north = 0;
		int south = south_level;
		for (uint32_t e : {i, j}) {
			const int a = end_level(graph.edges[e].from);
			const int b = end_level(graph.edges[e].to);
			north = std::max(north, std::min(a, b));
			south = std::min(south, std::max(a, b));
		}
		int level = south - south % 2;
		if (level >= north && level <= 2 * rows) band_crossings[level].emplace_back(i, j);
	}

	/* The place of an end along a row: a port is before the first item on the W side, else after the last. */
	auto row_position = [&](const JunctionTrackEnd &end, const std::vector<JunctionTrackRowItem> &row_items) {
		if (!end.node) return group_of(end.index).side == JunctionSide::W ? -1 : static_cast<int>(row_items.size());
		const auto it = std::find(row_items.begin(), row_items.end(), JunctionTrackRowItem{true, end.index});
		return static_cast<int>(it - row_items.begin());
	};

	/* Sweep. */
	JunctionTrackOrder order;
	order.node_level = node_level;
	order.rows.resize(rows);
	order.bands.resize(rows + 1);
	std::vector<std::vector<uint32_t>> swept(edge_count);
	std::vector<bool> south_is_to(edge_count, false); // The end the sweep entered the edge by.
	auto north_end = [&](uint32_t e) { return south_is_to[e] ? graph.edges[e].from : graph.edges[e].to; };
	std::vector<uint32_t> items;
	std::vector<int> layers; // Beside items: the layer of each item inside the band.
	for (int s = 0; s < n; s++) {
		if (group_of(s).main) continue;
		for (uint32_t e = 0; e < edge_count; e++) {
			const JunctionTrackEdge &edge = graph.edges[e];
			if ((!edge.from.node && edge.from.index == s) || (!edge.to.node && edge.to.index == s)) {
				south_is_to[e] = !edge.to.node && edge.to.index == s;
				items.push_back(e);
				layers.push_back(0);
				break;
			}
		}
	}
	for (int level = 2 * rows; level >= 0; level--) {
		if (level % 2 == 0) {
			std::vector<std::pair<uint32_t, uint32_t>> pending = band_crossings[level];
			std::fill(layers.begin(), layers.end(), 0);
			for (;;) {
				bool swapped = false;
				for (auto p = pending.begin(); p != pending.end(); ++p) {
					const auto pi = std::find(items.begin(), items.end(), p->first);
					const auto pj = std::find(items.begin(), items.end(), p->second);
					if (pi == items.end() || pj == items.end() || std::abs(pi - pj) != 1) continue;
					int &layer_i = layers[pi - items.begin()];
					int &layer_j = layers[pj - items.begin()];
					const int layer = 1 + std::max(layer_i, layer_j);
					layer_i = layer_j = layer;
					order.bands[level / 2].push_back({false, p->first, p->second, layer});
					std::iter_swap(pi, pj);
					swept[p->first].push_back(p->second);
					swept[p->second].push_back(p->first);
					pending.erase(p);
					swapped = true;
					break;
				}
				if (swapped) continue;
				auto it = std::find_if(items.begin(), items.end(), [&](uint32_t e) {
					const JunctionTrackEnd end = north_end(e);
					return end.node && node_level[end.index] == level;
				});
				if (it == items.end()) break;
				const uint32_t item = *it;
				const int layer = 1 + layers[it - items.begin()];
				order.bands[level / 2].push_back({true, north_end(item).index, 0, layer});
				const uint16_t node_index = north_end(item).index;
				const JunctionTrackNode &node = graph.nodes[node_index];
				std::vector<std::pair<int, uint32_t>> others; // (key, edge)
				for (uint32_t f = 0; f < edge_count; f++) {
					const JunctionTrackEdge &edge = graph.edges[f];
					const bool at_from = edge.from.node && edge.from.index == node_index;
					const bool at_to = edge.to.node && edge.to.index == node_index;
					if (f == item || (!at_from && !at_to)) continue;
					int key = n;
					for (uint16_t p : edge.paths) {
						const int far = node.split ? paths[p].to : paths[p].from;
						key = std::min(key, (far - node.slot + n) % n);
					}
					others.emplace_back(key, f);
					south_is_to[f] = at_to;
				}
				std::stable_sort(others.begin(), others.end(), [](const auto &a, const auto &b) { return a.first > b.first; });
				const size_t at = it - items.begin();
				items.erase(it);
				layers.erase(layers.begin() + at);
				size_t pos = at;
				for (const auto &other : others) {
					items.insert(items.begin() + pos, other.second);
					layers.insert(layers.begin() + pos, layer);
					pos++;
				}
			}
			if (!pending.empty()) return fail("a crossing between edges that are not adjacent is not ruled yet");
		} else {
			const int row = (level - 1) / 2;
			std::vector<JunctionTrackRowItem> &row_items = order.rows[row];
			std::vector<uint32_t> kept;
			for (uint32_t e : items) {
				const JunctionTrackEnd end = north_end(e);
				if (end.node && node_level[end.index] == level) {
					row_items.push_back({true, end.index});
				} else {
					row_items.push_back({false, e});
					kept.push_back(e);
				}
			}
			items = kept;
			layers.assign(items.size(), 0);
		}
		if (level % 2 == 1) {
			/* The crossing items record the row edges they cross (positions need the finished row). */
			const std::vector<JunctionTrackRowItem> &row_items = order.rows[(level - 1) / 2];
			for (uint32_t f = 0; f < edge_count; f++) {
				if (!is_row_edge(f) || end_level(graph.edges[f].from) != level) continue;
				const int lo = row_position(graph.edges[f].from, row_items);
				const int hi = row_position(graph.edges[f].to, row_items);
				for (int k = std::min(lo, hi) + 1; k < std::max(lo, hi); k++) {
					if (!row_items[k].node) swept[row_items[k].index].push_back(f);
				}
			}
		}
	}
	if (!items.empty()) return fail("the order does not realise the crossings");

	/* Crossings per edge. */
	order.edge_crossings.resize(edge_count);
	for (uint32_t e = 0; e < edge_count; e++) {
		if (!is_row_edge(e)) {
			order.edge_crossings[e] = swept[e];
			if (south_is_to[e]) std::reverse(order.edge_crossings[e].begin(), order.edge_crossings[e].end());
			continue;
		}
		const int row = (end_level(graph.edges[e].from) - 1) / 2;
		const std::vector<JunctionTrackRowItem> &row_items = order.rows[row];
		const int from = row_position(graph.edges[e].from, row_items);
		const int to = row_position(graph.edges[e].to, row_items);
		const int step = from < to ? 1 : -1;
		for (int k = from + step; k != to; k += step) {
			if (!row_items[k].node) order.edge_crossings[e].push_back(row_items[k].index);
		}
	}

	/* Check. */
	std::vector<std::pair<uint32_t, uint32_t>> realised;
	for (uint32_t e = 0; e < edge_count; e++) {
		for (uint32_t f : order.edge_crossings[e]) realised.emplace_back(std::min(e, f), std::max(e, f));
	}
	std::sort(realised.begin(), realised.end());
	realised.erase(std::unique(realised.begin(), realised.end()), realised.end());
	if (realised != graph.crossings) return fail("the order does not realise the crossings");
	return order;
}

JunctionTrackStructure StructureJunctionTrackGraph(const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, const JunctionTrackOrder &order, std::string *error)
{
	auto fail = [&](const char *reason) {
		if (error != nullptr) *error = reason;
		return JunctionTrackStructure{};
	};
	const std::vector<JunctionPortSlot> slots = JunctionPortSlots(topology);
	const int n = static_cast<int>(slots.size());
	auto group_of = [&](int slot) -> const JunctionLaneGroup & { return topology.groups[slots[slot].group]; };

	/* The rows of the main slots. */
	const std::vector<int> row_of = JunctionSlotRows(topology, paths, error);
	if (row_of.empty()) return JunctionTrackStructure{};
	const int rows = static_cast<int>(order.rows.size());
	auto end_level = [&](const JunctionTrackEnd &end) {
		if (end.node) return order.node_level[end.index];
		return group_of(end.index).main ? 2 * row_of[end.index] + 1 : 2 * rows + 1;
	};
	const uint32_t edge_count = static_cast<uint32_t>(graph.edges.size());
	auto is_row_edge = [&](uint32_t e) {
		const int level = end_level(graph.edges[e].from);
		return level == end_level(graph.edges[e].to) && level % 2 == 1;
	};

	/* Joiners, prio and copy. */
	JunctionTrackStructure structure;
	for (int r = 0; r < rows; r++) {
		const int level = 2 * r + 1;
		int joiners = 0;
		bool approach_south = false;
		for (uint32_t e = 0; e < edge_count; e++) {
			if (is_row_edge(e)) continue;
			const JunctionTrackEdge &edge = graph.edges[e];
			const bool at_from = edge.from.node && order.node_level[edge.from.index] == level && !graph.nodes[edge.from.index].split;
			const bool at_to = edge.to.node && order.node_level[edge.to.index] == level && !graph.nodes[edge.to.index].split;
			if (!at_from && !at_to) continue;
			joiners++;
			approach_south = end_level(at_from ? edge.to : edge.from) > level;
		}
		if (joiners > 1) return fail("more than one joiner on a lane is not ruled yet");
		bool crossed = false;
		for (uint32_t e = 0; e < edge_count; e++) {
			if (is_row_edge(e) && end_level(graph.edges[e].from) == level && !order.edge_crossings[e].empty()) crossed = true;
		}
		const bool has_joiner = joiners == 1;
		/* Per side: [north, south]. */
		bool prio[2] = {false, false};
		bool approach[2] = {false, false};
		bool copy[2] = {false, false};
		if (has_joiner) {
			approach[approach_south ? 1 : 0] = true;
			prio[approach_south ? 0 : 1] = true;
		}
		if (crossed) {
			if (has_joiner) {
				copy[approach_south ? 0 : 1] = true;
			} else {
				copy[0] = true;
			}
		}
		auto side_item = [&](int side, std::vector<JunctionTrackStackItem> &out) {
			using Kind = JunctionTrackStackItem::Kind;
			if (prio[side] && copy[side]) {
				out.push_back({static_cast<uint8_t>(r), Kind::PrioAndCopy});
			} else if (prio[side]) {
				out.push_back({static_cast<uint8_t>(r), Kind::Prio});
			} else if (copy[side]) {
				out.push_back({static_cast<uint8_t>(r), Kind::Copy});
			} else if (approach[side]) {
				out.push_back({static_cast<uint8_t>(r), Kind::Approach});
			}
		};
		side_item(0, structure.stack);
		structure.stack.push_back({static_cast<uint8_t>(r), JunctionTrackStackItem::Kind::Lane});
		side_item(1, structure.stack);
	}

	/* Bridges. */
	for (uint32_t e = 0; e < edge_count; e++) {
		if (is_row_edge(e) && !order.edge_crossings[e].empty()) structure.bridges.push_back({e, order.edge_crossings[e], true});
	}
	auto from_branch = [&](uint32_t e) {
		for (uint16_t p : graph.edges[e].paths) {
			if (group_of(paths[p].from).main) return false;
		}
		return true;
	};
	for (const auto &[i, j] : graph.crossings) {
		if (is_row_edge(i) || is_row_edge(j)) continue;
		const bool bi = from_branch(i);
		const bool bj = from_branch(j);
		if (bi == bj) return fail("a branch crossing without one joiner is not ruled yet");
		structure.bridges.push_back({bi ? i : j, {bi ? j : i}, false});
	}
	std::stable_sort(structure.bridges.begin(), structure.bridges.end(), [](const JunctionTrackBridge &a, const JunctionTrackBridge &b) { return a.edge < b.edge; });

	/* Turns: clockwise quarter turns from the in heading to the out heading. Headings: 0 north, 1 east, 2 south, 3 west. */
	auto in_heading = [](JunctionSide side) { return side == JunctionSide::W ? 1 : (side == JunctionSide::E ? 3 : (side == JunctionSide::S ? 0 : 2)); };
	auto out_heading = [](JunctionSide side) { return side == JunctionSide::W ? 3 : (side == JunctionSide::E ? 1 : (side == JunctionSide::S ? 2 : 0)); };
	for (const JunctionLanePath &path : paths) {
		int turn = (out_heading(group_of(path.to).side) - in_heading(group_of(path.from).side) + 4) % 4;
		if (turn == 3) turn = -1;
		if (turn == 2) return fail("a reversing path is not ruled yet");
		structure.turns.push_back(static_cast<int8_t>(turn));
	}
	return structure;
}

JunctionTrackPrograms ProgramJunctionTrackGraph(const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, const JunctionTrackOrder &order, const JunctionTrackStructure &structure, std::string *error)
{
	using Step = JunctionTrackStep;
	using StepKind = JunctionTrackStep::Kind;
	auto fail = [&](const char *reason) {
		if (error != nullptr) *error = reason;
		return JunctionTrackPrograms{};
	};
	if (graph.edges.empty()) return fail("a track graph without edges has no programs");
	const std::vector<JunctionPortSlot> slots = JunctionPortSlots(topology);
	const int n = static_cast<int>(slots.size());
	auto group_of = [&](int slot) -> const JunctionLaneGroup & { return topology.groups[slots[slot].group]; };
	auto step = [](StepKind kind, int hand, uint32_t index, uint8_t copy = 0) { return Step{kind, static_cast<int8_t>(hand), index, copy}; };

	/* Rows and levels. */
	const std::vector<int> row_of = JunctionSlotRows(topology, paths, error);
	if (row_of.empty()) return JunctionTrackPrograms{};
	const int rows = static_cast<int>(order.rows.size());
	auto end_level = [&](const JunctionTrackEnd &end) {
		if (end.node) return order.node_level[end.index];
		return group_of(end.index).main ? 2 * row_of[end.index] + 1 : 2 * rows + 1;
	};
	const uint32_t edge_count = static_cast<uint32_t>(graph.edges.size());
	auto is_row_edge = [&](uint32_t e) {
		const int level = end_level(graph.edges[e].from);
		return level == end_level(graph.edges[e].to) && level % 2 == 1;
	};
	auto is_row_node = [&](const JunctionTrackEnd &end) { return end.node && order.node_level[end.index] % 2 == 1; };
	auto is_band_node = [&](const JunctionTrackEnd &end, bool split) { return end.node && order.node_level[end.index] % 2 == 0 && graph.nodes[end.index].split == split; };

	/* The heading of every row: east when its in slot is on the W side. */
	std::vector<bool> east(rows, true);
	for (int s = 0; s < n; s++) {
		if (group_of(s).main && group_of(s).in) east[row_of[s]] = group_of(s).side == JunctionSide::W;
	}

	/* The stack items of every row. */
	std::vector<uint32_t> lane_item(rows, Step::NO_ROW);
	std::vector<uint32_t> north_item(rows, Step::NO_ROW);
	std::vector<uint32_t> approach_item(rows, Step::NO_ROW);
	for (uint32_t i = 0; i < structure.stack.size(); i++) {
		const JunctionTrackStackItem &item = structure.stack[i];
		if (item.kind == JunctionTrackStackItem::Kind::Lane) {
			lane_item[item.row] = i;
			if (i > 0 && structure.stack[i - 1].row == item.row && structure.stack[i - 1].kind != JunctionTrackStackItem::Kind::Approach) north_item[item.row] = i - 1;
		} else if (item.kind == JunctionTrackStackItem::Kind::Approach) {
			approach_item[item.row] = i;
		}
	}

	/* The shape of every row: in, one leg, out. */
	std::vector<int> in_count(rows, 0), leg_count(rows, 0), out_count(rows, 0), row_total(rows, 0);
	std::vector<uint32_t> leg_of(rows, 0);
	for (uint32_t e = 0; e < edge_count; e++) {
		const JunctionTrackEdge &edge = graph.edges[e];
		if (!is_row_edge(e)) {
			if (is_row_node(edge.from) && is_row_node(edge.to)) return fail("an edge between two rows is not ruled yet");
			continue;
		}
		const int r = (end_level(edge.from) - 1) / 2;
		row_total[r]++;
		if (!edge.from.node && edge.to.node) {
			in_count[r]++;
		} else if (edge.from.node && !edge.to.node) {
			out_count[r]++;
		} else if (edge.from.node && edge.to.node) {
			leg_count[r]++;
			leg_of[r] = e;
		}
	}
	for (int r = 0; r < rows; r++) {
		if (row_total[r] != 3 || in_count[r] != 1 || leg_count[r] != 1 || out_count[r] != 1) return fail("a lane other than in, one leg, out is not ruled yet");
	}

	/* Bridges. */
	auto doubled_bridge = [&](uint32_t e) {
		for (const JunctionTrackBridge &bridge : structure.bridges) {
			if (bridge.edge == e && bridge.doubled) return true;
		}
		return false;
	};
	auto bridge_over = [&](uint32_t e, uint32_t x) {
		for (const JunctionTrackBridge &bridge : structure.bridges) {
			if (bridge.edge == e && std::find(bridge.crossed.begin(), bridge.crossed.end(), x) != bridge.crossed.end()) return true;
		}
		return false;
	};
	std::vector<bool> row_doubled(rows, false);
	for (uint32_t e = 0; e < edge_count; e++) {
		if (!doubled_bridge(e)) continue;
		if (!is_row_edge(e)) continue;
		const int r = (end_level(graph.edges[e].from) - 1) / 2;
		if (e != leg_of[r]) return fail("a crossed lane in or out edge is not ruled yet");
		row_doubled[r] = true;
	}

	/* Band nodes: the siblings west to east, the straight child and the side of the other. */
	auto siblings = [&](uint16_t node_index) {
		const JunctionTrackNode &node = graph.nodes[node_index];
		std::vector<std::pair<int, uint32_t>> others; // (key, edge)
		for (uint32_t f = 0; f < edge_count; f++) {
			const JunctionTrackEdge &edge = graph.edges[f];
			const bool trunk = node.split ? (edge.to.node && edge.to.index == node_index) : (edge.from.node && edge.from.index == node_index);
			const bool at = node.split ? (edge.from.node && edge.from.index == node_index) : (edge.to.node && edge.to.index == node_index);
			if (trunk || !at) continue;
			int key = n;
			for (uint16_t p : edge.paths) {
				const int far = node.split ? paths[p].to : paths[p].from;
				key = std::min(key, (far - node.slot + n) % n);
			}
			others.emplace_back(key, f);
		}
		std::stable_sort(others.begin(), others.end(), [](const auto &a, const auto &b) { return a.first > b.first; });
		std::vector<uint32_t> result;
		for (const auto &other : others) result.push_back(other.second);
		return result;
	};
	/* Whether e diverges at the band node, and the hand of its side. */
	auto diverges = [&](uint32_t e, uint16_t node_index, int &side_hand) {
		const JunctionTrackNode &node = graph.nodes[node_index];
		const std::vector<uint32_t> sibs = siblings(node_index);
		if (sibs.size() != 2) return false;
		auto far_level = [&](uint32_t f) { return end_level(node.split ? graph.edges[f].to : graph.edges[f].from); };
		const uint32_t straight = far_level(sibs[1]) < far_level(sibs[0]) ? sibs[1] : sibs[0];
		if (e == straight) return false;
		side_hand = e == sibs[0] ? -1 : +1;
		return true;
	};
	/* The slot of an edge's first path that is not on the main line. */
	auto s_side_slot = [&](uint32_t e) {
		const JunctionLanePath &path = paths[graph.edges[e].paths[0]];
		return group_of(path.from).main ? path.to : path.from;
	};

	/* The programs. */
	JunctionTrackPrograms programs;
	programs.edges.resize(edge_count);
	programs.copies.resize(edge_count);
	for (uint32_t e = 0; e < edge_count; e++) {
		const JunctionTrackEdge &edge = graph.edges[e];
		std::vector<Step> &program = programs.edges[e];
		const std::vector<uint32_t> &crossings = order.edge_crossings[e];
		if (is_row_edge(e)) {
			const int r = (end_level(edge.from) - 1) / 2;
			if (e == leg_of[r]) {
				if (!row_doubled[r]) continue;
				if (lane_item[r] == Step::NO_ROW) return fail("a row without its side tracks is not ruled yet");
				const Step bend = step(StepKind::SBend, east[r] ? +1 : -1, lane_item[r]);
				for (uint32_t x : crossings) program.push_back(step(StepKind::Over, 0, x));
				program.push_back(bend);
				programs.copies[e].push_back(bend);
				for (uint32_t x : crossings) programs.copies[e].push_back(step(StepKind::Over, 0, x));
			} else if (edge.to.node) {
				const uint32_t item = row_doubled[r] ? north_item[r] : lane_item[r];
				if (item == Step::NO_ROW) return fail("a row without its side tracks is not ruled yet");
				program.push_back(step(StepKind::Fan, 0, item));
			} else {
				if (lane_item[r] == Step::NO_ROW) return fail("a row without its side tracks is not ruled yet");
				program.push_back(step(StepKind::Fan, 0, lane_item[r]));
			}
			continue;
		}

		const bool heads_south = end_level(edge.to) > end_level(edge.from);
		bool corner_placed = false;
		auto joiner_corner = [&]() {
			const int r = (order.node_level[edge.to.index] - 1) / 2;
			return step(StepKind::Corner, east[r] ? +1 : -1, approach_item[r]);
		};

		/* a. Start. */
		int side_hand = 0;
		if (is_band_node(edge.from, true) && diverges(e, edge.from.index, side_hand)) {
			if (is_row_node(edge.to)) {
				if (!crossings.empty()) return fail("a diverging joiner that crosses is not ruled yet");
				const Step corner = joiner_corner();
				if (corner.index == Step::NO_ROW) return fail("a row without its side tracks is not ruled yet");
				if (corner.hand != side_hand) return fail("a diverging corner on the wrong side is not ruled yet");
				program.push_back(corner);
				corner_placed = true;
			} else {
				program.push_back(step(StepKind::SBend, side_hand, Step::NO_ROW));
			}
		} else if (is_row_node(edge.from)) {
			const int r = (order.node_level[edge.from.index] - 1) / 2;
			program.push_back(step(StepKind::Corner, east[r] ? +1 : -1, Step::NO_ROW));
		}

		/* b. Crossings. */
		for (uint32_t x : crossings) {
			if (bridge_over(e, x)) {
				program.push_back(step(StepKind::Over, 0, x));
			} else if (doubled_bridge(x)) {
				program.push_back(step(StepKind::Under, 0, x, heads_south ? 0 : 1));
				program.push_back(step(StepKind::Under, 0, x, heads_south ? 1 : 0));
			} else if (bridge_over(x, e)) {
				program.push_back(step(StepKind::SBend, s_side_slot(e) < s_side_slot(x) ? +1 : -1, Step::NO_ROW));
				program.push_back(step(StepKind::Under, 0, x));
			} else {
				return fail("a crossing without a bridge is not ruled yet");
			}
		}

		/* c. Arrival on a row. */
		if (is_row_node(edge.to)) {
			const int r = (order.node_level[edge.to.index] - 1) / 2;
			if (lane_item[r] == Step::NO_ROW) return fail("a row without its side tracks is not ruled yet");
			if (!corner_placed) {
				const Step corner = joiner_corner();
				if (corner.index == Step::NO_ROW) return fail("a row without its side tracks is not ruled yet");
				program.push_back(corner);
			}
			program.push_back(step(StepKind::SBend, east[r] ? -1 : +1, lane_item[r]));
		}

		/* d. End. */
		if (is_band_node(edge.to, false) && diverges(e, edge.to.index, side_hand)) {
			program.push_back(step(StepKind::SBend, side_hand, Step::NO_ROW));
		}
	}
	return programs;
}

/** The axis piece of each heading: 0 north, 1 east, 2 south, 3 west. */
static constexpr Trackdir JUNCTION_AXIS_TRACKDIR[4] = {Trackdir::Y_NW, Trackdir::X_SW, Trackdir::Y_SE, Trackdir::X_NE};

/** The moves of a bend from its axis piece: the first turn, the halves after the first, the closing turn (DESIGN-G1 §11.8). A hand of +1 is a clockwise turn. */
static std::string JunctionBendMoves(int hand, int halves, bool corner)
{
	std::string moves(1, hand > 0 ? 'L' : 'R');
	moves.append(halves - 1, 'F');
	moves += ((corner ? hand : -hand) > 0) ? 'L' : 'R';
	return moves;
}

JunctionTrackPlacement PlaceJunctionTrackGraph(const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, const JunctionTrackOrder &order, const JunctionTrackStructure &structure, const JunctionTrackPrograms &programs, const JunctionSpec &spec, std::string *error)
{
	auto str = [](int v) { return fmt::format("{}", v); };
	using Step = JunctionTrackStep;
	using StepKind = JunctionTrackStep::Kind;
	auto fail = [&](const std::string &reason) {
		if (error != nullptr) *error = reason;
		return JunctionTrackPlacement{};
	};
	if (graph.edges.empty() || programs.edges.size() != graph.edges.size() || programs.copies.size() != graph.edges.size()) return fail("a placement needs the programs of a track graph");
	const std::vector<JunctionPortSlot> slots = JunctionPortSlots(topology);
	const int n = static_cast<int>(slots.size());
	auto group_of = [&](int slot) -> const JunctionLaneGroup & { return topology.groups[slots[slot].group]; };
	const std::vector<int> row_of = JunctionSlotRows(topology, paths, error);
	if (row_of.empty()) return JunctionTrackPlacement{};
	const int rows = static_cast<int>(order.rows.size());
	const uint32_t edge_count = static_cast<uint32_t>(graph.edges.size());
	const uint32_t NO_ROW = Step::NO_ROW;
	auto end_level = [&](const JunctionTrackEnd &end) {
		if (end.node) return order.node_level[end.index];
		return group_of(end.index).main ? 2 * row_of[end.index] + 1 : 2 * rows + 1;
	};
	auto is_row_edge = [&](uint32_t e) {
		const int level = end_level(graph.edges[e].from);
		return level == end_level(graph.edges[e].to) && level % 2 == 1;
	};
	auto is_row_node = [&](const JunctionTrackEnd &end) { return end.node && order.node_level[end.index] % 2 == 1; };
	auto row_of_node = [&](uint32_t node) { return (order.node_level[node] - 1) / 2; };

	/* Headings: 0 north, 1 east, 2 south, 3 west. */
	static constexpr int VX[4] = {0, 1, 0, -1};
	static constexpr int VY[4] = {-1, 0, 1, 0};
	std::vector<bool> east(rows, true);
	for (int s = 0; s < n; s++) {
		if (group_of(s).main && group_of(s).in) east[row_of[s]] = group_of(s).side == JunctionSide::W;
	}

	/* Stack items and rows. */
	const int items = static_cast<int>(structure.stack.size());
	std::vector<uint32_t> lane_item(rows, NO_ROW), north_item(rows, NO_ROW), approach_item(rows, NO_ROW);
	std::vector<int> row_first(rows, -1), row_last(rows, -1);
	for (int i = 0; i < items; i++) {
		const JunctionTrackStackItem &item = structure.stack[i];
		if (row_first[item.row] < 0) row_first[item.row] = i;
		row_last[item.row] = i;
		if (item.kind == JunctionTrackStackItem::Kind::Lane) {
			lane_item[item.row] = i;
			if (i > 0 && structure.stack[i - 1].row == item.row && structure.stack[i - 1].kind != JunctionTrackStackItem::Kind::Approach) north_item[item.row] = i - 1;
		} else if (item.kind == JunctionTrackStackItem::Kind::Approach) {
			approach_item[item.row] = i;
		}
	}

	/* The shape of every row: its in edge, leg and out edge. */
	std::vector<uint32_t> in_edge(rows, NO_ROW), leg_edge(rows, NO_ROW), out_edge(rows, NO_ROW);
	for (uint32_t e = 0; e < edge_count; e++) {
		if (!is_row_edge(e)) continue;
		const JunctionTrackEdge &edge = graph.edges[e];
		const int r = (end_level(edge.from) - 1) / 2;
		if (!edge.from.node && edge.to.node) {
			in_edge[r] = e;
		} else if (edge.from.node && !edge.to.node) {
			out_edge[r] = e;
		} else if (edge.from.node && edge.to.node) {
			leg_edge[r] = e;
		}
	}
	for (int r = 0; r < rows; r++) {
		if (in_edge[r] == NO_ROW || leg_edge[r] == NO_ROW || out_edge[r] == NO_ROW || lane_item[r] == NO_ROW) return fail("a lane other than in, one leg, out is not ruled yet");
	}
	std::vector<std::vector<int>> bridges_of(edge_count);
	for (size_t b = 0; b < structure.bridges.size(); b++) bridges_of[structure.bridges[b].edge].push_back(static_cast<int>(b));
	auto doubled_row = [&](int r) { return !bridges_of[leg_edge[r]].empty() && structure.bridges[bridges_of[leg_edge[r]][0]].doubled; };

	/* The curve size and the rule constants. */
	const int cl = spec.curve_limit ? static_cast<int>(DeriveCurveSpacing(spec.reference_speed, spec.curve_speed, spec.tl)) : 2;
	int h = std::min(2 * static_cast<int>(spec.tl) - 1, std::max(2, cl));
	if (h % 2 == 0) h++;
	const int k = (h - 1) / 2;
	const int prio = spec.prio_tiles;
	/* The generator's JUNCTION_SHIFT_BACK_TILES (junction_generator.cpp, D145), repeated here because that one is file-local. */
	static constexpr int JUNCTION_SHIFT_BACK_TILES = 4;
	/* Not checked here, left to G1c(ii-b), whose render tests (no conflicting track bits) catch them: the band-layer slices (DESIGN-G1 §10.8) and a vertical edge meeting a side track. */
	static constexpr int STUB = 2; // D206.

	/* The raster of a bend: its half pieces' tiles relative to the first. A hand of +1 is a clockwise turn. */
	auto bend_tiles = [&](int heading, int hand, int halves, bool corner) {
		const JunctionRaster raster = RasterizeJunctionRoute({{0, 0}, JUNCTION_AXIS_TRACKDIR[heading], JunctionBendMoves(hand, halves, corner)});
		std::vector<TileIndexDiffC> result;
		if (!raster.error.empty() || static_cast<int>(raster.pieces.size()) != halves + 2) return result;
		for (int i = 1; i <= halves; i++) {
			result.push_back({static_cast<int16_t>(raster.pieces[i].tile.x - raster.pieces[1].tile.x), static_cast<int16_t>(raster.pieces[i].tile.y - raster.pieces[1].tile.y)});
		}
		return result;
	};

	/* Points: a variable in the X system and in the Y system. Point 0 is the box's north-west corner. */
	int pt_count = 1;
	auto new_pt = [&]() { return pt_count++; };
	std::vector<int> item_pt(items), node_pt(graph.nodes.size()), port_pt(n);
	for (int &p : item_pt) p = new_pt();
	for (int &p : node_pt) p = new_pt();
	for (int &p : port_pt) p = new_pt();
	const int xe_pt = new_pt(); // The E ports' column.
	const int se_pt = new_pt(); // The S edge's row.

	/* The chains of the edges that are not rows: elements in travel order. */
	struct Elem {
		enum class K : uint8_t { Tile, Corner, SBend, Heads } k = K::Tile;
		int first = 0;
		int last = 0;
		int tok = -1;
		int hin = 0;
		int hout = 0;
		int hand = 0;
		bool joiner_div = false; ///< A Corner from a band split: its size comes from the rows (F4).
		bool free_m = false; ///< A vertical S-bend: its size comes from the columns.
		int m = 1;
		int cross = -1; ///< Heads: the crossed edge.
		uint32_t index = NO_ROW; ///< The stack item the token ends on.
		std::vector<TileIndexDiffC> offs;
	};
	std::vector<std::vector<Elem>> chain(edge_count);
	for (uint32_t e = 0; e < edge_count; e++) {
		if (is_row_edge(e)) continue;
		const JunctionTrackEdge &edge = graph.edges[e];
		const std::vector<Step> &program = programs.edges[e];
		const int start = edge.from.node ? node_pt[edge.from.index] : port_pt[edge.from.index];
		const int stop = edge.to.node ? node_pt[edge.to.index] : port_pt[edge.to.index];
		int cur = edge.kind == JunctionTrackEdge::Kind::In ? 0 : (edge.kind == JunctionTrackEdge::Kind::Out ? 2 : (is_row_node(edge.from) ? (east[row_of_node(edge.from.index)] ? 1 : 3) : 0));
		const bool from_band_split = edge.from.node && order.node_level[edge.from.index] % 2 == 0 && graph.nodes[edge.from.index].split;
		const bool to_merge = edge.to.node && !graph.nodes[edge.to.index].split;
		std::vector<Elem> &elems = chain[e];
		Elem begin;
		begin.first = begin.last = start;
		begin.hin = begin.hout = cur;
		elems.push_back(begin);
		for (size_t t = 0; t < program.size(); t++) {
			const Step &step = program[t];
			Elem el;
			el.tok = static_cast<int>(t);
			el.hin = cur;
			if (step.kind == StepKind::Corner || step.kind == StepKind::SBend) {
				el.k = step.kind == StepKind::Corner ? Elem::K::Corner : Elem::K::SBend;
				el.hand = step.hand;
				el.index = step.index;
				el.hout = step.kind == StepKind::Corner ? (cur + step.hand + 4) % 4 : cur;
				el.first = t == 0 && edge.from.node ? start : new_pt();
				el.last = t + 1 == program.size() && edge.to.node && (is_row_node(edge.to) || to_merge) && step.kind == StepKind::SBend ? stop : new_pt();
				el.joiner_div = step.kind == StepKind::Corner && t == 0 && from_band_split;
				el.free_m = step.kind == StepKind::SBend && (cur == 0 || cur == 2);
				if (step.kind == StepKind::SBend && t == 0 && edge.from.node && !from_band_split) el.first = new_pt();
				cur = el.hout;
			} else if (step.kind == StepKind::Over) {
				if (bridges_of[e].size() != 1 || structure.bridges[bridges_of[e][0]].doubled || structure.bridges[bridges_of[e][0]].crossed.size() != 1) return fail("a bridge over several band crossings is not ruled yet");
				const int b = bridges_of[e][0];
				el.k = Elem::K::Heads;
				el.first = new_pt();
				el.last = new_pt();
				el.hout = cur;
				el.cross = static_cast<int>(structure.bridges[b].crossed[0]);
			} else {
				continue;
			}
			elems.push_back(el);
		}
		Elem finish;
		finish.first = finish.last = stop;
		finish.hin = finish.hout = cur;
		elems.push_back(finish);
	}

	/* The solutions. */
	struct Cons {
		int v;
		int u;
		int k;
	};
	std::vector<Cons> cons;
	int phase = 0; // 0: X, 1: Y.
	auto ge = [&](int axis, int v, int u, int c) { if (axis == phase) cons.push_back({v, u, c}); };
	auto eq = [&](int axis, int v, int u, int c) { ge(axis, v, u, c); ge(axis, u, v, -c); };
	std::vector<int> vx, vy;
	std::string build_why;
	bool rows_known = false; // False in the first pass, which only gives the S-bends' first sizes.
	auto solve = [&](std::vector<int> &val) {
		val.assign(pt_count, 0);
		for (int round = 0; round <= pt_count; round++) {
			bool changed = false;
			for (const Cons &c : cons) {
				if (val[c.u] + c.k > val[c.v]) {
					val[c.v] = val[c.u] + c.k;
					changed = true;
				}
			}
			if (!changed) return val[0] == 0;
		}
		/* Still relaxing after one round per point: a positive cycle. */
		if (error != nullptr) *error = "the placement rules contradict each other";
		return false;
	};
	/* A relation along a heading: coord(next) - coord(prev) in the heading's sense >= d (= d when exact), no relation across. */
	auto along = [&](int heading, int next, int prev, int d, bool exact) {
		const int axis = VX[heading] != 0 ? 0 : 1;
		const int sign = VX[heading] + VY[heading];
		if (sign > 0) {
			if (exact) eq(axis, next, prev, d); else ge(axis, next, prev, d);
		} else {
			if (exact) eq(axis, prev, next, d); else ge(axis, prev, next, d);
		}
	};
	auto run = [&](int heading, int next, int prev, int d, bool exact) {
		along(heading, next, prev, d, exact);
		eq(VX[heading] != 0 ? 1 : 0, next, prev, 0);
	};

	/* The size of every bend, refreshed from the rows and columns. */
	auto refresh = [&](bool rows_known) {
		for (uint32_t e = 0; e < edge_count; e++) {
			for (Elem &el : chain[e]) {
				if (el.k == Elem::K::Corner) {
					const int kk = el.joiner_div && rows_known ? std::abs(vy[el.first] - vy[el.last]) : k;
					el.offs = bend_tiles(el.hin, el.hand, 2 * kk + 1, true);
				} else if (el.k == Elem::K::SBend) {
					el.offs = bend_tiles(el.hin, el.hand, 2 * el.m, false);
				}
			}
		}
	};

	/* The notation rows of the W and E sides. */
	std::vector<int> notation(n, -1);
	for (const JunctionSide side : {JunctionSide::W, JunctionSide::E}) {
		std::vector<int> side_slots;
		for (int s = 0; s < n; s++) {
			if (group_of(s).side == side) side_slots.push_back(s);
		}
		if (side == JunctionSide::E) std::reverse(side_slots.begin(), side_slots.end());
		int row = 0;
		for (size_t i = 0; i < side_slots.size(); i++) {
			if (i > 0 && slots[side_slots[i]].group != slots[side_slots[i - 1]].group) row++;
			notation[side_slots[i]] = row++;
		}
	}
	std::vector<int> port_row(n, 0);
	std::vector<int> fan_m(edge_count, 0);
	auto fan_item = [&](uint32_t e) -> uint32_t { return programs.edges[e].size() == 1 && programs.edges[e][0].kind == StepKind::Fan ? programs.edges[e][0].index : NO_ROW; };
	auto edge_port = [&](uint32_t e) { return graph.edges[e].from.node ? graph.edges[e].to.index : graph.edges[e].from.index; };

	/* Row chains' points. */
	struct RowPts {
		int fif = 0, fil = 0, fof = 0, fol = 0; ///< In and out fan.
		int lane_split = 0, sb1_last = 0, sb0_first = 0, lane_merge = 0;
		std::vector<int> nh0, fh0, nh1, fh1; ///< The heads, one point each per bridge of the row's leg.
	};
	std::vector<RowPts> row_pts(rows);
	for (int r = 0; r < rows; r++) {
		RowPts &rp = row_pts[r];
		rp.fif = new_pt(); rp.fil = new_pt(); rp.fof = new_pt(); rp.fol = new_pt();
		rp.lane_split = new_pt(); rp.sb1_last = new_pt(); rp.sb0_first = new_pt(); rp.lane_merge = new_pt();
		if (doubled_row(r)) {
			for (size_t k = 0; k < bridges_of[leg_edge[r]].size(); k++) {
				rp.nh0.push_back(new_pt()); rp.fh0.push_back(new_pt()); rp.nh1.push_back(new_pt()); rp.fh1.push_back(new_pt());
			}
		}
	}

	/* Symbolic tiles of an edge: an anchor point, a column offset, a row; in travel order. */
	struct SymTile {
		int pt;
		int dx;
		int y;
	};
	auto edge_tiles = [&](uint32_t e) {
		std::vector<SymTile> tiles;
		const std::vector<Elem> &elems = chain[e];
		for (size_t i = 0; i < elems.size(); i++) {
			const Elem &el = elems[i];
			if (el.k == Elem::K::Tile) {
				tiles.push_back({el.first, 0, vy[el.first]});
			} else if (el.k == Elem::K::Heads) {
				const int step = vy[el.last] > vy[el.first] ? 1 : -1;
				for (int y = vy[el.first]; y != vy[el.last] + step; y += step) tiles.push_back({el.first, 0, y});
			} else {
				for (const TileIndexDiffC &o : el.offs) tiles.push_back({el.first, o.x, vy[el.first] + o.y});
			}
			if (i + 1 < elems.size() && elems[i + 1].first != el.last && VX[el.hout] == 0) {
				const int d = VY[el.hout];
				for (int y = vy[el.last] + d; d > 0 ? y < vy[elems[i + 1].first] : y > vy[elems[i + 1].first]; y += d) tiles.push_back({el.last, 0, y});
			}
		}
		return tiles;
	};

	/* One round: the constraints of a phase. */
	auto build = [&]() {
		cons.clear();
		const bool x_phase = phase == 0;

		/* Stack rows. */
		eq(1, item_pt[0], 0, 0);
		for (int i = 1; i < items; i++) {
			if (structure.stack[i].row == structure.stack[i - 1].row) eq(1, item_pt[i], item_pt[i - 1], 1); else ge(1, item_pt[i], item_pt[i - 1], 1);
		}
		std::vector<int> layer1;
		for (uint32_t i = 0; i < graph.nodes.size(); i++) {
			const int level = order.node_level[i];
			if (level % 2 != 0) continue;
			const int b = level / 2;
			if (b >= 1) ge(1, node_pt[i], item_pt[row_last[b - 1]], 1);
			if (b < rows) ge(1, item_pt[row_first[b]], node_pt[i], 1);
		}
		for (const JunctionTrackBandEvent &event : order.bands[rows]) {
			if (event.node && event.layer == 1) layer1.push_back(static_cast<int>(event.a));
		}
		for (size_t i = 0; i < layer1.size(); i++) {
			if (i > 0) eq(1, node_pt[layer1[i]], node_pt[layer1[0]], 0);
			eq(1, se_pt, node_pt[layer1[i]], STUB);
		}
		for (int s = 0; s < n; s++) {
			if (!group_of(s).main) eq(1, port_pt[s], se_pt, 0);
		}

		/* Ports. */
		std::vector<int> south;
		for (int s = 0; s < n; s++) {
			if (group_of(s).side == JunctionSide::W) eq(0, port_pt[s], 0, 0);
			if (group_of(s).side == JunctionSide::E) eq(0, port_pt[s], xe_pt, 0);
			if (group_of(s).side == JunctionSide::S) south.push_back(s);
		}
		int column = 0;
		for (size_t i = 1; i < south.size(); i++) {
			column += 1 + (slots[south[i]].group != slots[south[i - 1]].group ? 1 : 0);
			eq(0, port_pt[south[i]], port_pt[south[0]], column);
		}

		/* The rows. */
		for (int r = 0; r < rows; r++) {
			const bool doubled = doubled_row(r);
			const RowPts &rp = row_pts[r];
			const uint32_t exit_item = doubled ? north_item[r] : lane_item[r];
			const JunctionTrackEdge &in = graph.edges[in_edge[r]];
			const JunctionTrackEdge &out = graph.edges[out_edge[r]];
			const int nex = node_pt[in.to.index];
			const int njm = node_pt[out.from.index];
			eq(1, nex, item_pt[exit_item], 0);
			eq(1, njm, item_pt[lane_item[r]], 0);
			const int s = east[r] ? 1 : -1;
			const int heading = east[r] ? 1 : 3;
			const int pin = port_pt[in.from.index];
			const int pout = port_pt[out.to.index];
			if (doubled) {
				eq(1, rp.lane_split, item_pt[north_item[r]], 0);
				eq(1, rp.sb1_last, item_pt[lane_item[r]], 0);
				for (size_t k = 0; k < rp.nh0.size(); k++) {
					eq(1, rp.nh0[k], item_pt[north_item[r]], 0);
					eq(1, rp.fh0[k], item_pt[north_item[r]], 0);
					eq(1, rp.nh1[k], item_pt[lane_item[r]], 0);
					eq(1, rp.fh1[k], item_pt[lane_item[r]], 0);
				}
				eq(1, rp.sb0_first, item_pt[north_item[r]], 0);
				eq(1, rp.lane_merge, item_pt[lane_item[r]], 0);
			}
			if (!x_phase) continue;

			/* In: port, fan, exit split. */
			const int m_in = fan_m[in_edge[r]];
			if (m_in == 0) {
				eq(0, rp.fif, pin, 0);
				eq(0, rp.fil, pin, 0);
			} else {
				along(heading, rp.fif, pin, 2, false);
				along(heading, rp.fil, rp.fif, m_in - 1, true);
			}
			along(heading, nex, rp.fil, 2, false);
			int after = nex; // The last point of the lane chain before the joiner merge.
			if (!doubled) {
				along(heading, njm, nex, prio + 1, false);
			} else {
				/* The runs from the exit split to the lane split and on to the first heads (and from the last heads to the lane merge) are rule minima, not exact: series bridges need the slack (DESIGN-G1 §11.9b). */
				along(heading, rp.lane_split, nex, 2, false);
				const size_t g = rp.nh0.size();
				along(heading, rp.nh0[0], rp.lane_split, 2, false);
				eq(0, rp.sb1_last, rp.lane_split, 0);
				along(heading, rp.nh1[0], rp.sb1_last, 1, false);
				for (size_t k = 0; k < g; k++) {
					along(heading, rp.nh0[k], rp.nh1[k], 1, true);
					along(heading, rp.fh0[k], rp.fh1[k], 1, true);
					if (k > 0) along(heading, rp.nh0[k], rp.fh0[k - 1], 2, false);
				}
				along(heading, rp.sb0_first, rp.fh0[g - 1], 1, false);
				eq(0, rp.lane_merge, rp.sb0_first, 0);
				along(heading, rp.lane_merge, rp.fh1[g - 1], 2, false);
				along(heading, njm, rp.lane_merge, prio + 1, false);
				after = rp.lane_merge;

				/* Every crosser's tiles on the copy row and the lane row lie between its bridge's heads, and the heads hug the outermost tiles (D80, D118, D145). */
				const int u = -s; // The upstream step.
				for (size_t k = 0; k < g; k++) {
					const JunctionTrackBridge &bridge = structure.bridges[bridges_of[leg_edge[r]][k]];
					bool have = false;
					int low_pt = 0, low_dx = 0, high_pt = 0, high_dx = 0, low_value = 0, high_value = 0;
					for (uint32_t x : bridge.crossed) {
						if (!rows_known) break;
						const std::vector<SymTile> tiles = edge_tiles(x);
						for (int pass = 0; pass < 2; pass++) {
							const int y = vy[item_pt[pass == 0 ? north_item[r] : lane_item[r]]];
							bool any = false;
							for (const SymTile &z : tiles) {
								if (z.y != y) continue;
								any = true;
								const int near = pass == 0 ? rp.nh0[k] : rp.nh1[k];
								const int far = pass == 0 ? rp.fh0[k] : rp.fh1[k];
								if (s > 0) {
									ge(0, z.pt, near, 1 - z.dx);
									ge(0, far, z.pt, 1 + z.dx);
								} else {
									ge(0, near, z.pt, 1 + z.dx);
									ge(0, z.pt, far, 1 - z.dx);
								}
								const int dx = pass == 0 ? z.dx : z.dx - u;
								const int value = vx[z.pt] + dx;
								if (!have || value < low_value) { low_value = value; low_pt = z.pt; low_dx = dx; }
								if (!have || value > high_value) { high_value = value; high_pt = z.pt; high_dx = dx; }
								have = true;
							}
							if (!any) { build_why = "Z row " + str(y) + " edge " + str(x) + " row " + str(r); return false; }
						}
					}
					if (have) {
						const int west = s > 0 ? rp.nh0[k] : rp.fh0[k];
						const int east_head = s > 0 ? rp.fh0[k] : rp.nh0[k];
						eq(0, west, low_pt, low_dx - 1);
						eq(0, east_head, high_pt, high_dx + 1);
					}
				}
			}
			(void)after;

			/* Out: joiner merge, fan, port. */
			const int m_out = fan_m[out_edge[r]];
			if (m_out == 0) {
				eq(0, rp.fof, pout, 0);
				eq(0, rp.fol, pout, 0);
				along(heading, pout, njm, 2, false);
			} else {
				along(heading, rp.fof, njm, JUNCTION_SHIFT_BACK_TILES, true);
				along(heading, rp.fol, rp.fof, m_out - 1, true);
				along(heading, pout, rp.fol, 2, true);
			}
		}

		/* The chains of the other edges. */
		for (uint32_t e = 0; e < edge_count; e++) {
			const std::vector<Elem> &elems = chain[e];
			const JunctionTrackEdge &edge = graph.edges[e];
			for (size_t i = 0; i < elems.size(); i++) {
				const Elem &el = elems[i];
				if (el.k == Elem::K::Corner) {
					const int kk = k;
					const int dx = kk * (VX[el.hin] + VX[el.hout]);
					const int dy = kk * (VY[el.hin] + VY[el.hout]);
					if (el.joiner_div) {
						ge(1, el.first, el.last, k);
						if (x_phase) {
							const int kp = rows_known ? std::abs(vy[el.first] - vy[el.last]) : k;
							eq(0, el.last, el.first, kp * (VX[el.hin] + VX[el.hout]));
						}
					} else {
						eq(0, el.last, el.first, dx);
						eq(1, el.last, el.first, dy);
					}
					if (el.index != NO_ROW) eq(1, el.last, item_pt[el.index], 0);
				} else if (el.k == Elem::K::SBend) {
					const int side = (el.hin + el.hand + 4) % 4;
					if (el.hin == 0 || el.hin == 2) {
						eq(1, el.last, el.first, VY[el.hin] * (el.m - 1));
						if (VX[side] > 0) ge(0, el.last, el.first, el.m); else ge(0, el.first, el.last, el.m);
					} else {
						eq(1, el.last, el.first, VY[side]);
						eq(0, el.last, el.first, VX[el.hin] * 0);
					}
					if (el.index != NO_ROW) eq(1, el.last, item_pt[el.index], 0);
				} else if (el.k == Elem::K::Heads) {
					eq(VX[el.hin] != 0 ? 1 : 0, el.last, el.first, 0);
				}
				if (i + 1 >= elems.size()) continue;
				const Elem &next = elems[i + 1];
				if (next.first == el.last) continue;
				bool exact = false;
				const int dist = 2;
				if (next.k == Elem::K::Tile && i + 2 == elems.size()) {
					const bool band_merge = edge.to.node && !graph.nodes[edge.to.index].split && order.node_level[edge.to.index] % 2 == 0;
										if (!edge.to.node) exact = true;
				}
				if (el.k == Elem::K::Tile && i == 0 && !edge.from.node) exact = true;
				run(el.hout, next.first, el.last, dist, exact);
			}

			/* A single bridge passes over the S-bend it crosses, in the column next to the S-bend's start (D113, F4). */
			for (const Elem &el : elems) {
				if (el.k != Elem::K::Heads) continue;
				const std::vector<Elem> &other = chain[el.cross];
				const Elem *crossed = nullptr;
				for (const Elem &o : other) {
					if (o.k == Elem::K::SBend && o.tok >= 0 && static_cast<size_t>(o.tok) + 1 < programs.edges[el.cross].size() && programs.edges[el.cross][o.tok + 1].kind == StepKind::Under && programs.edges[el.cross][o.tok + 1].index == e) crossed = &o;
				}
				if (crossed == nullptr) { build_why = "no crossed sbend"; return false; }
				const int side = (crossed->hin + crossed->hand + 4) % 4;
				const std::vector<TileIndexDiffC> tiles = bend_tiles(crossed->hin, crossed->hand, 2 * std::max(crossed->m, 2), false);
				int lo = 1000, hi = -1000;
				for (const TileIndexDiffC &t : tiles) {
					if (t.x * VX[side] + t.y * VY[side] != 1) continue;
					lo = std::min<int>(lo, t.y);
					hi = std::max<int>(hi, t.y);
				}
				if (lo > hi) { build_why = "no column tile"; return false; }
				eq(0, el.first, crossed->first, VX[side]);
				const bool north = el.hin == 0;
				eq(1, el.first, crossed->first, north ? hi + 1 : lo - 1);
				eq(1, el.last, crossed->first, north ? lo - 1 : hi + 1);
			}
		}

		if (x_phase) {
			for (int p = 1; p < pt_count; p++) {
				if (p != xe_pt) ge(0, xe_pt, p, 0);
			}
		} else {
			for (int p = 1; p < pt_count; p++) {
				if (p != se_pt) ge(1, se_pt, p, 0);
			}
		}
		return true;
	};

	/* The rounds: a first X pass without rows gives the S-bends' sizes, then Y and X alternate until the sizes hold. */
	phase = 0;
	refresh(false);
	if (!build()) return fail("the placement rules are not complete for this topology " + build_why);
	if (!solve(vx)) return JunctionTrackPlacement{};
	for (uint32_t e = 0; e < edge_count; e++) {
		for (Elem &el : chain[e]) {
			if (el.free_m) el.m = std::max(1, std::abs(vx[el.last] - vx[el.first]));
		}
	}
	rows_known = true;
	for (int round = 0; round < 5; round++) {
		phase = 1;
		refresh(false);
		if (!build()) return fail("the placement rules are not complete for this topology " + build_why);
		if (!solve(vy)) return JunctionTrackPlacement{};

		/* The port rows: the offset that minimises the largest fan (ties: the smaller). */
		int best = -1, best_o = 0;
		for (int o = 0; o <= vy[item_pt[items - 1]]; o++) {
			int worst = 0;
			for (uint32_t e = 0; e < edge_count; e++) {
				const uint32_t item = fan_item(e);
				if (item == NO_ROW) continue;
				worst = std::max(worst, std::abs(o + notation[edge_port(e)] - vy[item_pt[item]]));
			}
			if (best < 0 || worst < best) {
				best = worst;
				best_o = o;
			}
		}
		for (int s = 0; s < n; s++) {
			if (notation[s] >= 0) port_row[s] = best_o + notation[s];
		}
		for (uint32_t e = 0; e < edge_count; e++) {
			const uint32_t item = fan_item(e);
			if (item != NO_ROW) fan_m[e] = std::abs(port_row[edge_port(e)] - vy[item_pt[item]]);
		}

		phase = 0;
		refresh(true);
		if (!build()) return fail("the placement rules are not complete for this topology " + build_why);
		if (!solve(vx)) return JunctionTrackPlacement{};

		bool changed = false;
		for (uint32_t e = 0; e < edge_count; e++) {
			for (Elem &el : chain[e]) {
				if (!el.free_m) continue;
				const int m = std::abs(vx[el.last] - vx[el.first]);
				if (m != el.m) {
					el.m = m;
					changed = true;
				}
			}
		}
		if (!changed) break;
		if (round == 4) return fail("the placement does not settle");
	}

	/* The fans' and ports' rows. */
	for (int s = 0; s < n; s++) {
		if (notation[s] >= 0) vy[port_pt[s]] = port_row[s];
	}
	for (int r = 0; r < rows; r++) {
		RowPts &rp = row_pts[r];
		const int in_port = graph.edges[in_edge[r]].from.index;
		const int out_port = graph.edges[out_edge[r]].to.index;
		vy[rp.fif] = port_row[in_port];
		vy[rp.fil] = vy[item_pt[fan_item(in_edge[r])]];
		vy[rp.fof] = vy[item_pt[fan_item(out_edge[r])]];
		vy[rp.fol] = port_row[out_port];
	}
	auto tile = [&](int pt) { return TileIndexDiffC{static_cast<int16_t>(vx[pt]), static_cast<int16_t>(vy[pt])}; };

	JunctionTrackPlacement result;
	for (int i = 0; i < items; i++) result.item_rows.push_back(vy[item_pt[i]]);
	for (int s = 0; s < n; s++) result.ports.push_back(tile(port_pt[s]));
	for (uint32_t i = 0; i < graph.nodes.size(); i++) result.nodes.push_back(tile(node_pt[i]));
	result.size = {static_cast<int16_t>(vx[xe_pt] + 1), static_cast<int16_t>(vy[se_pt] + 1)};

	/* The heads of a bridge: its near and far point, and the carrier's travel direction along the axis. */
	struct Heads {
		int near;
		int far;
		bool row; ///< On a row (else in a column).
	};
	auto heads_of = [&](int b, int copy) {
		const uint32_t edge = structure.bridges[b].edge;
		for (int r = 0; r < rows; r++) {
			if (leg_edge[r] != edge || !doubled_row(r)) continue;
			const size_t k = std::find(bridges_of[edge].begin(), bridges_of[edge].end(), b) - bridges_of[edge].begin();
			return Heads{copy == 0 ? row_pts[r].nh0[k] : row_pts[r].nh1[k], copy == 0 ? row_pts[r].fh0[k] : row_pts[r].fh1[k], true};
		}
		for (const Elem &el : chain[edge]) {
			if (el.k == Elem::K::Heads) return Heads{el.first, el.last, false};
		}
		return Heads{0, 0, false};
	};
	/* The bridge of an edge whose crossed set holds another edge, or -1. */
	auto bridge_over = [&](uint32_t edge, uint32_t crossed) {
		for (int b : bridges_of[edge]) {
			const std::vector<uint32_t> &set = structure.bridges[b].crossed;
			if (std::find(set.begin(), set.end(), crossed) != set.end()) return b;
		}
		return -1;
	};
	/* The tiles of an edge strictly between a bridge's heads on a row (or in a column), in the edge's order. */
	auto tiles_under = [&](uint32_t edge, const Heads &heads, int line) {
		std::vector<TileIndexDiffC> found;
		for (const SymTile &t : edge_tiles(edge)) {
			const int x = vx[t.pt] + t.dx;
			if (heads.row) {
				if (t.y != line) continue;
				if (x > std::min(vx[heads.near], vx[heads.far]) && x < std::max(vx[heads.near], vx[heads.far])) found.push_back({static_cast<int16_t>(x), static_cast<int16_t>(t.y)});
			} else {
				if (x != line) continue;
				if (t.y > std::min(vy[heads.near], vy[heads.far]) && t.y < std::max(vy[heads.near], vy[heads.far])) found.push_back({static_cast<int16_t>(x), static_cast<int16_t>(t.y)});
			}
		}
		return found;
	};

	result.edges.resize(edge_count);
	result.copies.resize(edge_count);
	for (uint32_t e = 0; e < edge_count; e++) {
		const std::vector<Step> &program = programs.edges[e];
		if (is_row_edge(e)) {
			const int r = (end_level(graph.edges[e].from) - 1) / 2;
			const RowPts &rp = row_pts[r];
			const int s = east[r] ? 1 : -1;
			if (e == in_edge[r] && !program.empty()) {
				result.edges[e].push_back({tile(rp.fif), tile(rp.fil)});
			} else if (e == out_edge[r] && !program.empty()) {
				result.edges[e].push_back({tile(rp.fof), tile(rp.fol)});
			} else if (e == leg_edge[r] && doubled_row(r)) {
				for (int copy = 0; copy < 2; copy++) {
					const std::vector<Step> &tokens = copy == 0 ? program : programs.copies[e];
					for (const Step &step : tokens) {
						if (step.kind == StepKind::SBend) {
							(copy == 0 ? result.edges[e] : result.copies[e]).push_back(copy == 0 ? JunctionTrackSpan{tile(rp.sb0_first), tile(rp.lane_merge)} : JunctionTrackSpan{tile(rp.lane_split), tile(rp.sb1_last)});
						} else {
							const int over = bridge_over(e, step.index);
							if (over < 0) return fail("a crossing edge misses its bridge");
							const Heads heads = heads_of(over, copy);
							std::vector<TileIndexDiffC> found = tiles_under(step.index, heads, vy[heads.near]);
							if (found.empty()) return fail("a crossing edge misses its bridge");
							std::stable_sort(found.begin(), found.end(), [&](const TileIndexDiffC &a, const TileIndexDiffC &b) { return s > 0 ? a.x < b.x : a.x > b.x; });
							(copy == 0 ? result.edges[e] : result.copies[e]).push_back({found.front(), found.back()});
						}
					}
				}
			}
			continue;
		}
		const std::vector<Elem> &elems = chain[e];
		for (size_t t = 0; t < program.size(); t++) {
			const Step &step = program[t];
			if (step.kind == StepKind::Corner || step.kind == StepKind::SBend) {
				for (const Elem &el : elems) {
					if (el.tok == static_cast<int>(t)) {
						const TileIndexDiffC first = tile(el.first);
						const TileIndexDiffC last = {static_cast<int16_t>(vx[el.first] + el.offs.back().x), static_cast<int16_t>(vy[el.first] + el.offs.back().y)};
						result.edges[e].push_back({first, last});
					}
				}
			} else if (step.kind == StepKind::Over) {
				if (bridges_of[e].empty()) return fail("a crossing edge misses its bridge");
				const Heads heads = heads_of(bridges_of[e][0], 0);
				std::vector<TileIndexDiffC> found = tiles_under(step.index, heads, vx[heads.near]);
				if (found.empty()) return fail("a crossing edge misses its bridge");
				const bool north = vy[heads.far] < vy[heads.near];
				std::stable_sort(found.begin(), found.end(), [&](const TileIndexDiffC &a, const TileIndexDiffC &b) { return north ? a.y > b.y : a.y < b.y; });
				result.edges[e].push_back({found.front(), found.back()});
			} else {
				/* Under: this edge's tiles beneath the overhead edge's bridge. */
				const uint32_t x = step.index;
				const int bx = bridge_over(x, e);
				if (bx < 0) return fail("a crossing edge misses its bridge");
				std::vector<TileIndexDiffC> found;
				if (structure.bridges[bx].doubled) {
					const Heads heads = heads_of(bx, step.copy);
					int r = 0;
					while (leg_edge[r] != x) r++;
					found = tiles_under(e, heads, vy[item_pt[step.copy == 0 ? north_item[r] : lane_item[r]]]);
				} else {
					const Heads heads = heads_of(bx, 0);
					found = tiles_under(e, heads, vx[heads.near]);
				}
				if (found.empty()) return fail("a crossing edge misses its bridge");
				result.edges[e].push_back({found.front(), found.back()});
			}
		}
	}

	/* The bridges. */
	for (size_t b = 0; b < structure.bridges.size(); b++) {
		const JunctionTrackBridge &bridge = structure.bridges[b];
		std::array<JunctionBridge, 2> heads{};
		for (int copy = 0; copy < (bridge.doubled ? 2 : 1); copy++) {
			const Heads hd = heads_of(static_cast<int>(b), copy);
			heads[copy] = JunctionBridge{tile(hd.near), tile(hd.far)};
			if (bridge.doubled) {
				const int middle = std::abs(vx[hd.near] - vx[hd.far]) - 1;
				if (copy == 0 && middle > spec.max_bridge_middle) return fail(fmt::format("bridge {} on edge {} needs {} middle tiles, more than {}", b, bridge.edge, middle, spec.max_bridge_middle));
			}
		}
		result.bridges.push_back(heads);
	}
	return result;
}

JunctionLayout RenderJunctionTrackPlacement(const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, const JunctionTrackStructure &structure, const JunctionTrackPrograms &programs, const JunctionTrackPlacement &placement, std::string *error)
{
	using StepKind = JunctionTrackStep::Kind;
	auto fail = [&](const std::string &reason) {
		if (error != nullptr) *error = reason;
		return JunctionLayout{};
	};
	if (placement.nodes.empty() || graph.path_edges.size() != paths.size()) return fail("a render needs a placement of the track graph");
	const std::vector<JunctionPortSlot> slots = JunctionPortSlots(topology);

	/* Unit steps by heading: 0 north, 1 east, 2 south, 3 west. */
	static constexpr int VX[4] = {0, 1, 0, -1};
	static constexpr int VY[4] = {-1, 0, 1, 0};
	auto doubled_leg = [&](uint32_t e) {
		return std::ranges::any_of(structure.bridges, [&](const JunctionTrackBridge &b) { return b.edge == e && b.doubled; });
	};
	auto bridge_over = [&](uint32_t e, uint32_t crossed) {
		for (size_t b = 0; b < structure.bridges.size(); b++) {
			const JunctionTrackBridge &bridge = structure.bridges[b];
			if (bridge.edge == e && std::ranges::find(bridge.crossed, crossed) != bridge.crossed.end()) return static_cast<int>(b);
		}
		return -1;
	};

	JunctionLayout layout;
	layout.size = placement.size;
	for (size_t p = 0; p < paths.size(); p++) {
		const std::vector<uint32_t> &edges = graph.path_edges[p];
		const bool has_doubled = std::ranges::any_of(edges, doubled_leg);
		for (int c = 0; c < (has_doubled ? 2 : 1); c++) {
			auto fail_at = [&](const std::string &where, const std::string &reason) { return fail(fmt::format("path {} copy {}{}: {}", p, c, where, reason)); };
			const JunctionSide side = topology.groups[slots[paths[p].from].group].side;
			if (side == JunctionSide::N) return fail_at("", "an N port is not ruled yet");
			int h = side == JunctionSide::W ? 1 : side == JunctionSide::E ? 3 : 0;
			TileIndexDiffC cur = placement.ports[paths[p].from];
			JunctionRoute route{cur, JUNCTION_AXIS_TRACKDIR[h], ""};
			int last_over = -1;

			/* The number of steps from cur to a tile along the heading, or -1. */
			auto run = [&](const TileIndexDiffC &to) {
				const int dx = to.x - cur.x;
				const int dy = to.y - cur.y;
				if (VX[h] != 0) return dy == 0 && dx * VX[h] >= 0 ? dx * VX[h] : -1;
				return dx == 0 && dy * VY[h] >= 0 ? dy * VY[h] : -1;
			};

			for (uint32_t e : edges) {
				const bool copy1 = c == 1 && doubled_leg(e);
				const std::vector<JunctionTrackStep> &tokens = copy1 ? programs.copies[e] : programs.edges[e];
				const std::vector<JunctionTrackSpan> &spans = copy1 ? placement.copies[e] : placement.edges[e];
				if (spans.size() != tokens.size()) return fail_at(fmt::format(" edge {}", e), "the placement does not match the program");
				for (size_t t = 0; t < tokens.size(); t++) {
					const JunctionTrackStep &token = tokens[t];
					const std::string where = fmt::format(" edge {} token {}", e, t);
					if (token.kind == StepKind::Under) continue;
					if (token.kind == StepKind::Over) {
						const int b = bridge_over(e, token.index);
						if (b < 0) return fail_at(where, "an Over without a bridge");
						if (b == last_over) continue;
						last_over = b;
						const JunctionBridge &heads = placement.bridges[b][doubled_leg(e) ? c : 0];
						if (cur.x == heads.start.x && cur.y == heads.start.y && !route.moves.empty() && (route.moves.back() == 'L' || route.moves.back() == 'R')) {
							/* D110: the bridge follows the bend directly and completes it; the near head stands where the closing axis piece would. */
							route.moves.pop_back();
						} else {
							const int before = run({static_cast<int16_t>(heads.start.x - VX[h]), static_cast<int16_t>(heads.start.y - VY[h])});
							if (before < 0) return fail_at(where, "negative or off-line run");
							route.moves.append(before, 'F');
						}
						const int middle = std::abs(heads.end.x - heads.start.x) + std::abs(heads.end.y - heads.start.y) - 1;
						route.moves += fmt::format("B{}", middle);
						cur = heads.end;
						continue;
					}
					const TileIndexDiffC first = spans[t].first;
					const TileIndexDiffC last = spans[t].second;
					if (token.kind == StepKind::Fan) {
						if (first.x == last.x && first.y == last.y && first.x == cur.x && first.y == cur.y) continue;
						const bool off_line = VX[h] != 0 ? (first.y != cur.y || last.y != cur.y) : (first.x != cur.x || last.x != cur.x);
						if (!off_line) continue;
					}
					int matches = 0;
					int match_hand = 0;
					int match_halves = 0;
					JunctionRaster match_raster;
					for (int hand : {-1, 1}) {
						if (token.kind != StepKind::Fan && hand != token.hand) continue;
						for (int halves = 1; halves <= 64; halves++) {
							const JunctionRaster raster = RasterizeJunctionRoute({{0, 0}, JUNCTION_AXIS_TRACKDIR[h], JunctionBendMoves(hand, halves, token.kind == StepKind::Corner)});
							if (!raster.error.empty() || static_cast<int>(raster.pieces.size()) != halves + 2) continue;
							const int o1x = raster.pieces[1].tile.x - raster.pieces[0].tile.x;
							const int o1y = raster.pieces[1].tile.y - raster.pieces[0].tile.y;
							const int olx = raster.pieces[halves].tile.x - raster.pieces[0].tile.x;
							const int oly = raster.pieces[halves].tile.y - raster.pieces[0].tile.y;
							if (last.x - first.x != olx - o1x || last.y - first.y != oly - o1y) continue;
							matches++;
							match_hand = hand;
							match_halves = halves;
							match_raster = raster;
						}
					}
					if (matches != 1) return fail_at(where, fmt::format("{} bend matches", matches));
					const JunctionPiece &p0 = match_raster.pieces[0];
					const TileIndexDiffC axis_tile = {static_cast<int16_t>(first.x - (match_raster.pieces[1].tile.x - p0.tile.x)), static_cast<int16_t>(first.y - (match_raster.pieces[1].tile.y - p0.tile.y))};
					const int before = run(axis_tile);
					if (before < 0) return fail_at(where, "negative or off-line run");
					route.moves.append(before, 'F');
					route.moves += JunctionBendMoves(match_hand, match_halves, token.kind == StepKind::Corner);
					const JunctionPiece &closing = match_raster.pieces.back();
					cur = {static_cast<int16_t>(axis_tile.x + closing.tile.x - p0.tile.x), static_cast<int16_t>(axis_tile.y + closing.tile.y - p0.tile.y)};
					int heading = -1;
					for (int i = 0; i < 4; i++) {
						if (JUNCTION_AXIS_TRACKDIR[i] == closing.trackdir) heading = i;
					}
					if (heading < 0) return fail_at(where, "a bend does not end on an axis piece");
					h = heading;
				}
			}
			const TileIndexDiffC out = placement.ports[paths[p].to];
			const int tail = run(out);
			if (tail < 0) return fail_at("", "negative or off-line run to the out port");
			route.moves.append(tail, 'F');

			const JunctionRaster raster = RasterizeJunctionRoute(route);
			if (!raster.error.empty()) return fail_at("", raster.error);
			if (raster.pieces.back().tile.x != out.x || raster.pieces.back().tile.y != out.y) return fail_at("", "misses its out port");
			AddRasterToLayout(layout, raster);
		}
	}
	return layout;
}

const JunctionTopology &JunctionTTopology()
{
	static const JunctionTopology topology = {
		{
			{JunctionSide::W, true, 1, 0, true}, // EIn
			{JunctionSide::W, false, 1, 0, true}, // WOut
			{JunctionSide::S, true, 1, 1, false}, // NIn
			{JunctionSide::S, false, 1, 1, false}, // SOut
			{JunctionSide::E, true, 1, 0, true}, // WIn
			{JunctionSide::E, false, 1, 0, true}, // EOut
		},
		/* EIn->EOut, EIn->SOut, WIn->WOut, WIn->SOut, NIn->WOut, NIn->EOut. */
		{{0, 5}, {0, 3}, {4, 1}, {4, 3}, {2, 1}, {2, 5}},
	};
	return topology;
}

const JunctionTopology &JunctionLLRRSidelineTopology()
{
	static const JunctionTopology topology = {
		{
			{JunctionSide::W, true, 2, 0, true}, // e-in
			{JunctionSide::W, false, 2, 0, true}, // w-out
			{JunctionSide::S, true, 1, 1, false}, // branch-in
			{JunctionSide::S, false, 1, 1, false}, // branch-out
			{JunctionSide::E, true, 2, 0, true}, // w-in
			{JunctionSide::E, false, 2, 0, true}, // e-out
		},
		/* e through, w through, e exit, w exit, join to e-out, join to w-out. */
		{{0, 5}, {4, 1}, {0, 3}, {4, 3}, {2, 5}, {2, 1}},
	};
	return topology;
}

/**
 * Append the compositions of n into the given number of parts, each 1..JUNCTION_BRIDGE_MAX_CROSSERS, in descending lexicographic order of part sizes.
 * @param n The crossings left to place.
 * @param parts The parts left.
 * @param current The parts so far.
 * @param[out] out Receives the compositions.
 */
static void AppendBridgeCompositions(size_t n, size_t parts, std::vector<size_t> &current, std::vector<std::vector<size_t>> &out)
{
	if (parts == 1) {
		if (n >= 1 && n <= JUNCTION_BRIDGE_MAX_CROSSERS) {
			current.push_back(n);
			out.push_back(current);
			current.pop_back();
		}
		return;
	}
	for (int p = static_cast<int>(std::min(JUNCTION_BRIDGE_MAX_CROSSERS, n - (parts - 1))); p >= 1; p--) {
		if (n - p > (parts - 1) * JUNCTION_BRIDGE_MAX_CROSSERS) continue;
		current.push_back(p);
		AppendBridgeCompositions(n - p, parts - 1, current, out);
		current.pop_back();
	}
}

/**
 * Append the tuples of bridge counts that sum to a total, in ascending lexicographic order.
 * @param low The least count per bridge.
 * @param high The greatest count per bridge.
 * @param remaining The total left to spend.
 * @param current The counts so far.
 * @param[out] out Receives the tuples.
 */
static void AppendBridgeCounts(const std::vector<size_t> &low, const std::vector<size_t> &high, size_t remaining, std::vector<size_t> &current, std::vector<std::vector<size_t>> &out)
{
	const size_t i = current.size();
	if (i == low.size()) {
		if (remaining == 0) out.push_back(current);
		return;
	}
	for (size_t c = low[i]; c <= high[i] && c <= remaining; c++) {
		current.push_back(c);
		AppendBridgeCounts(low, high, remaining - c, current, out);
		current.pop_back();
	}
}

JunctionTrackStructure CutJunctionTrackBridges(const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, const JunctionTrackOrder &order, const JunctionTrackStructure &structure, const JunctionTrackPrograms &programs, const JunctionSpec &spec, JunctionTrackPlacement *placement, std::string *error)
{
	/* The doubled bridges and, per bridge and bridge count, the compositions of its crossing list. */
	std::vector<size_t> low, high;
	std::vector<std::vector<std::vector<std::vector<size_t>>>> compositions;
	size_t low_total = 0, high_total = 0;
	for (const JunctionTrackBridge &bridge : structure.bridges) {
		if (!bridge.doubled) continue;
		const size_t n = bridge.crossed.size();
		low.push_back((n + JUNCTION_BRIDGE_MAX_CROSSERS - 1) / JUNCTION_BRIDGE_MAX_CROSSERS);
		high.push_back(n);
		low_total += low.back();
		high_total += n;
		compositions.emplace_back();
		for (size_t c = low.back(); c <= n; c++) {
			compositions.back().emplace_back();
			std::vector<size_t> current;
			AppendBridgeCompositions(n, c, current, compositions.back().back());
		}
	}

	std::string last_error;
	for (size_t total = low_total; total <= high_total; total++) {
		std::vector<std::vector<size_t>> tuples;
		std::vector<size_t> counts;
		AppendBridgeCounts(low, high, total, counts, tuples);

		bool have = false;
		JunctionTrackStructure best_structure;
		JunctionTrackPlacement best;
		for (const std::vector<size_t> &tuple : tuples) {
			std::vector<size_t> choice(tuple.size(), 0);
			for (;;) {
				/* The cut structure: each doubled bridge replaced in place by its parts. */
				JunctionTrackStructure cut = structure;
				cut.bridges.clear();
				size_t d = 0;
				for (const JunctionTrackBridge &bridge : structure.bridges) {
					if (!bridge.doubled) {
						cut.bridges.push_back(bridge);
						continue;
					}
					size_t first = 0;
					for (size_t part : compositions[d][tuple[d] - low[d]][choice[d]]) {
						cut.bridges.push_back({bridge.edge, std::vector<uint32_t>(bridge.crossed.begin() + first, bridge.crossed.begin() + first + part), true});
						first += part;
					}
					d++;
				}
				std::stable_sort(cut.bridges.begin(), cut.bridges.end(), [](const JunctionTrackBridge &a, const JunctionTrackBridge &b) { return a.edge < b.edge; });

				std::string why;
				JunctionTrackPlacement placed = PlaceJunctionTrackGraph(topology, paths, graph, order, cut, programs, spec, &why);
				if (placed.nodes.empty()) {
					last_error = why;
				} else if (!have || placed.size.x < best.size.x || (placed.size.x == best.size.x && placed.size.y < best.size.y)) {
					have = true;
					best = std::move(placed);
					best_structure = std::move(cut);
				}

				/* The next combination of compositions, the last bridge varying fastest. */
				size_t i = choice.size();
				bool more = false;
				while (i > 0 && !more) {
					i--;
					if (++choice[i] < compositions[i][tuple[i] - low[i]].size()) {
						more = true;
					} else {
						choice[i] = 0;
					}
				}
				if (!more) break;
			}
		}
		if (have) {
			if (placement != nullptr) *placement = std::move(best);
			if (error != nullptr) error->clear();
			return best_structure;
		}
	}
	if (placement != nullptr) *placement = JunctionTrackPlacement{};
	if (error != nullptr) *error = last_error;
	return structure;
}
