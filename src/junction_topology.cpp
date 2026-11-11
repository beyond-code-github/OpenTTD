/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_topology.cpp The topology of a junction type, its lane-level paths and the chord crossing rule (DESIGN-G1 §1, §2). */

#include "stdafx.h"

#include "junction_topology.h"

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
