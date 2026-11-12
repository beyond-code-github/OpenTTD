/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_topology.cpp The topology of a junction type, its lane-level paths and the chord crossing rule (DESIGN-G1 §1, §2). */

#include "stdafx.h"

#include "junction_topology.h"

#include "junction_chain.h"
#include "junction_generator.h"
#include "junction_levels.h"
#include "junction_positions.h"
#include "junction_route.h"
#include "junction_rules_internal.h"
#include "junction_signal_pass.h"
#include "direction_func.h"
#include "map_func.h"
#include "track_func.h"

#include <algorithm>
#include <climits>
#include <functional>
#include <map>
#include <numeric>
#include <optional>
#include <set>
#include <tuple>

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
	auto step = [](StepKind kind, int hand, uint32_t index, uint8_t copy = 0, Step::Role role = Step::Role::None) { return Step{kind, static_cast<int8_t>(hand), index, copy, role}; };

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

	/* The roles of every row's edges (graph structure, not token shape: any program may sit on them): one in edge from a main port, one leg between two nodes, one out edge to a main port. */
	std::vector<int> in_count(rows, 0), leg_count(rows, 0), out_count(rows, 0);
	std::vector<uint32_t> leg_of(rows, 0);
	for (uint32_t e = 0; e < edge_count; e++) {
		const JunctionTrackEdge &edge = graph.edges[e];
		if (!is_row_edge(e)) {
			if (is_row_node(edge.from) && is_row_node(edge.to)) return fail("an edge between two rows is not ruled yet");
			continue;
		}
		const int r = (end_level(edge.from) - 1) / 2;
		if (!edge.from.node && edge.to.node) {
			in_count[r]++;
		} else if (edge.from.node && !edge.to.node) {
			out_count[r]++;
		} else if (edge.from.node && edge.to.node) {
			leg_count[r]++;
			leg_of[r] = e;
		} else {
			return fail("a lane other than in, one leg, out is not ruled yet");
		}
	}
	for (int r = 0; r < rows; r++) {
		if (in_count[r] != 1 || leg_count[r] != 1 || out_count[r] != 1) return fail("a lane other than in, one leg, out is not ruled yet");
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
				const Step merge = step(StepKind::SBend, east[r] ? +1 : -1, lane_item[r], 0, Step::Role::LaneMerge);
				const Step split = step(StepKind::SBend, east[r] ? +1 : -1, lane_item[r], 0, Step::Role::LaneSplit);
				for (uint32_t x : crossings) program.push_back(step(StepKind::Over, 0, x));
				program.push_back(merge);
				programs.copies[e].push_back(split);
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

/** The kinds of element a placement run lies between (D217, DESIGN-G1 §16.1). */
enum class FloorKind : uint8_t { Port, Fan, Tile, Corner, SBend, Heads, Node, PrioNode, LaneSplit, LaneMerge };

static Track JunctionCornerTrack(DiagDirection a, DiagDirection b);
/**
 * The least run in tiles between two neighbouring elements: the one table of every run floor in placement (D217, DESIGN-G1 §16.1). A pair not listed has the chain run.
 * @param before The kind of the element the run starts at.
 * @param after The kind of the element the run ends at.
 * @param spec The junction spec (the waiting room reads the train length).
 * @param even_prio The prio element's coverage in tiles, rounded up to even.
 * @return The least run.
 */
static int JunctionRunFloor(FloorKind before, FloorKind after, const JunctionSpec &spec, int even_prio)
{
	using K = FloorKind;
	if (before == K::Port && after == K::Fan) return 2; // The fan starts 2 tiles inside the port: the signal tile and the boundary tile (D115a, D206).
	if (before == K::Fan && after == K::Node) return 2; // The run from a fan to the exit split is the chain run (D217).
	if (before == K::Fan && after == K::Port) return 2; // The last fan tile to its port: the port stub (D206, D115a).
	if (before == K::Node && after == K::Port) return 2; // A branch port's stub below its node (D206, D115a).
	if (before == K::PrioNode && after == K::Fan) return 2; // The joiner merge to the out fan: the chain run (D217).
	if (before == K::PrioNode && after == K::Port) return 2; // The joiner merge to an unfanned port: the chain run and port stub (D217, D206).
	if (before == K::Node && after == K::PrioNode) return even_prio + 1; // The exit split stays outside the prio element's coverage; its row ends one step on (DESIGN-G1 §12.4, RECON-prio-element §3).
	if (before == K::LaneMerge && after == K::PrioNode) return even_prio + 2; // The element's row must not meet the lane merge's S-bend (RECON-prio-element §3-4).
	if (before == K::Node && after == K::LaneSplit) return 2; // The exit split to the lane split: the chain run, a minimum so series bridges have slack (D217, DESIGN-G1 §11.9b).
	if (before == K::LaneSplit && after == K::Heads) return 2; // Shared split tile, one plain piece, then the near head (D145, D113, D117, D118).
	if (before == K::Heads && after == K::Heads) return 2; // Series bridges: the next near head 2 tiles past the last far head (DESIGN-G1 §11.9b).
	if (before == K::Heads && after == K::LaneMerge) return 2; // The last far head to the lane merge (D145: x_hwA - x_lm = 2).
	if (before == K::Node && after == K::Node) return static_cast<int>(spec.tl) + 1; // The waiting room of an exit path, from its first split to the first merge: 2 dy - 1 >= 2 tl units (rule WaitingRoom).
	return 2; // The chain run: a signal tile and a boundary tile between elements (D217).
}

/**
 * The placement of a programmed track graph (DESIGN-G1 §11.8, §14.2).
 * @param fixed The choices to solve with (one Y and one X solve), or nullptr for the rounds that find them.
 * @param x_floors (point, least column) pairs for the X phase (see the public overload).
 * @param y_floors (point, least row) pairs for the Y phase (see the public overload).
 * @param facts The target terrain's facts for the X phase of a fixed solve (DESIGN-G1 §14.4), or nullptr.
 * @param offset The working tile of the placement's (0, 0); only with facts.
 * @param heights False: only the class facts (D218): the height steps, joins and element fits are skipped.
 * @param classes_only True (the height solve's X phase): every tile check is JunctionFacts::Buildable, the game's slope rules are the level solve's.
 */
static JunctionTrackPlacement PlaceTrackGraph(const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, const JunctionTrackOrder &order, const JunctionTrackStructure &structure, const JunctionTrackPrograms &programs, const JunctionSpec &spec, const JunctionPlacementChoices *fixed, std::string *error, const std::vector<std::pair<int, int>> &x_floors, const std::vector<std::pair<int, int>> &y_floors, const JunctionFacts *facts = nullptr, TileIndexDiffC offset = {0, 0}, bool heights = true, bool classes_only = false)
{
	auto str = [](int v) { return fmt::format("{}", v); };
	using Step = JunctionTrackStep;
	using StepKind = JunctionTrackStep::Kind;
	auto fail = [&](const std::string &reason) {
		if (error != nullptr) *error = reason;
		return JunctionTrackPlacement{};
	};
	if (graph.edges.empty() || programs.edges.size() != graph.edges.size() || programs.copies.size() != graph.edges.size()) return fail("a placement needs the programs of a track graph");
	if (fixed != nullptr) {
		if (fixed->m.size() != graph.edges.size() || fixed->corner.size() != graph.edges.size()) return fail("the choices do not fit the programs of this track graph");
		for (size_t e = 0; e < graph.edges.size(); e++) {
			if (fixed->m[e].size() != programs.edges[e].size() || fixed->corner[e].size() != programs.edges[e].size()) return fail("the choices do not fit the programs of this track graph");
		}
	}
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
	/* A row edge that starts or ends at a port: the in and out edges of a row, whose chains carry their own relations (R3); the legs keep the row points. */
	auto is_in_out_edge = [&](uint32_t e) { return is_row_edge(e) && !(graph.edges[e].from.node && graph.edges[e].to.node); };
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
	/* The prio element's coverage in tiles: joins at even offsets 0..even_prio upstream of the joiner merge (DESIGN-G1 §12.4, RECON-prio-element §3). */
	const int even_prio = prio + (prio & 1);
	/* Not checked here, left to G1c(ii-b), whose render tests (no conflicting track bits) catch them: the band-layer slices (DESIGN-G1 §10.8) and a vertical edge meeting a side track. */

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
		/**
		 * Tile: one tile (a node, a port or an edge's own point). Corner: a quarter turn. SBend: a sideways step, vertical (size from the columns) or, on a row edge, horizontal (size |item row - its first row| from the rows, rule of the lane change).
		 * Heads: a Y-axis bridge's two heads. Fan (R2): the port stub's S-bend between the port row and an item row, size |port row - item row| from the port rows, 0 allowed (no tiles, first and last at one column).
		 * HeadsPair (R2): the near and far head of a doubled row bridge on one copy's row; copy 1's heads are copy 0's plus the stagger u (D118, D145), a rule of the row relations, not of the element.
		 */
		enum class K : uint8_t { Tile, Corner, SBend, Heads, Fan, HeadsPair } k = K::Tile;
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
		int slot = -1; ///< Row edges: the port slot of a port Tile or a Fan.
		bool port_first = false; ///< A Fan: it starts on the port row (an in edge's), else it ends on it.
		std::vector<TileIndexDiffC> offs;
	};
	std::vector<std::vector<Elem>> chain(edge_count);
	std::vector<std::vector<Elem>> chain_copy(edge_count); ///< Copy 1 of a doubled row leg: SBend, then its heads.
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
				el.last = t + 1 == program.size() && edge.to.node && to_merge && step.kind == StepKind::SBend ? stop : new_pt();
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
		uint32_t reason; ///< Index into 'reasons'.
	};
	std::vector<Cons> cons;
	std::vector<JunctionPlacementReason> reasons;
	/* A terrain fact of the X phase: a domain of columns for a point, or a run from point v to point u that must not cover both the column columns[0] and the column columns[1]. */
	struct FactCons {
		int v;
		int u; ///< -1 for a domain.
		std::vector<int> columns;
		uint32_t reason; ///< Index into 'reasons'.
	};
	std::vector<FactCons> fact_cons;
	std::map<uint32_t, std::string> notes; ///< Per reason of a fact, the tiles it names, for the error text.
	JunctionPlacementReason why_now; ///< The reason of the relations added now (DESIGN-G1 §14.4).
	auto why = [&](const char *rule, uint32_t edge = UINT32_MAX, uint32_t node = UINT32_MAX, int row = -1) { why_now = JunctionPlacementReason{rule, edge, node, static_cast<int32_t>(row)}; };
	int phase = 0; // 0: X, 1: Y.
	auto ge = [&](int axis, int v, int u, int c) {
		if (axis != phase) return;
		assert(why_now.rule != nullptr);
		if (reasons.empty() || reasons.back().rule != why_now.rule || reasons.back().edge != why_now.edge || reasons.back().node != why_now.node || reasons.back().row != why_now.row) reasons.push_back(why_now);
		cons.push_back({v, u, c, static_cast<uint32_t>(reasons.size() - 1)});
	};
	auto eq = [&](int axis, int v, int u, int c) { ge(axis, v, u, c); ge(axis, u, v, -c); };
	std::vector<int> vx, vy;
	std::string build_why;
	bool rows_known = false; // False in the first pass, which only gives the S-bends' first sizes.
	/** A doubled bridge's crosser tiles on its two rows, as (point, column offset) in the copy-0 heads' frame: the heads hug the outermost by the flat solve's order (D217). */
	struct HugCandidate {
		int west;
		int east;
		uint32_t edge;
		int row;
		std::vector<std::pair<int, int>> tiles;
	};
	std::vector<HugCandidate> hugs;
	/* Modes of a build (D217): the flat state has no floors, no clearance and, in X, only the relations; clearance relations of the Y phase come from the flat state; an illegal occupancy group is a placement failure. */
	bool floors_off = false, relations_only = false, skip_clearance = false, occupancy_failed = false, collecting = false;
	std::set<std::pair<uint32_t, uint32_t>> ref_shares; ///< The edge pairs that share a tile in the reference solve (the state's flat solve without floors).
	/** One bridge's middle line: a Y-axis bridge's column variable and its two heads' points, or an X-axis bridge's row variable and its heads' columns. */
	struct BridgeLine {
		int bridge;
		uint32_t edge;
		bool x_axis;
		int a, b; ///< The heads' points.
		int line; ///< The point whose column (Y-axis) or row (X-axis) is the line.
	};
	struct HeadRef {
		int bridge;
		uint32_t edge;
		int xpt, ypt; ///< The points whose column and row are the head's.
	};
	struct ClearanceY {
		int v, u;
		uint32_t edge_b, edge_h;
	};
	std::vector<ClearanceY> clear_y;
	std::vector<JunctionPlacementReason> failure; ///< The explanation chain of the last failed solve.
	std::vector<int> fan_m(edge_count, 0);
	std::vector<int> port_row(n, 0);
	/** A row change of kinds 1-4 (DESIGN-G1 §14.5, D215): the point its bend starts at, whether its feature lies toward larger columns, and for a fan the edge whose fan size must be nonzero. */
	struct RowChange {
		int anchor;
		int feature; ///< The point of the feature it serves.
		bool larger;
		uint32_t fan_edge;
		uint32_t edge;
		int row;
		int token; ///< The token of the edge's program (copy's program for copy 1) the change belongs to; with edge and copy it is the change's key (R2).
		int copy;
	};
	std::vector<RowChange> row_changes;
	auto solve = [&](std::vector<int> &val) {
		/* A least solution is a longest path, so no point lies beyond the sum of the positive constants; a larger lo is a positive cycle. */
		int bound = 1;
		for (const Cons &cn : cons) bound += std::max(cn.k, 0);
		/* Facts can push points across the whole snapshot (DESIGN-G1 §14.4). */
		if (facts != nullptr && phase == 0) bound = std::max(bound, facts->MaxX() - offset.x + 1);
		JunctionPositions positions(pt_count, 0, bound);
		reasons.push_back(JunctionPlacementReason{"origin"});
		bool ok = positions.RestrictRange(0, 0, 0, static_cast<uint32_t>(reasons.size() - 1));
		for (const Cons &cn : cons) {
			if (!ok) break;
			ok = positions.AddDifference(cn.v, cn.u, cn.k, cn.reason);
		}
		for (const FactCons &fc : fact_cons) {
			if (!ok) break;
			ok = fc.u < 0 ? positions.RestrictDomain(fc.v, fc.columns, fc.reason) : positions.AddAvoid(fc.v, fc.u, fc.columns[0], fc.columns[1], fc.reason);
		}
		if (ok && phase == 0 && rows_known) {
			/* The D215 objective (DESIGN-G1 §14.5): with every other point at its least column, each row change whose feature lies toward larger columns moves as far as the relations and facts allow. The tokens' rigid groups are the points tied by exact relations. */
			std::map<std::pair<int, int>, int> most;
			for (const Cons &cn : cons) {
				const auto [found, added] = most.emplace(std::make_pair(cn.v, cn.u), cn.k);
				if (!added) found->second = std::max(found->second, cn.k);
			}
			std::vector<int> group(pt_count);
			std::iota(group.begin(), group.end(), 0);
			auto root = [&](int p) {
				while (group[p] != p) p = group[p] = group[group[p]];
				return p;
			};
			for (const auto &[pair, k] : most) {
				const auto back = most.find({pair.second, pair.first});
				if (back != most.end() && back->second == -k) group[root(pair.first)] = root(pair.second);
			}
			/* A group may not hold a port, the origin or the box's edges; it may hold a node only when that node is the feature being pushed. */
			std::vector<bool> fixed_point(pt_count, false), node_point(pt_count, false);
			fixed_point[0] = fixed_point[xe_pt] = fixed_point[se_pt] = true;
			for (const int p : port_pt) fixed_point[p] = true;
			for (const int p : node_pt) node_point[p] = true;
			/* First the tokens whose feature lies toward larger columns move to it; then, for the others, the feature's group moves toward the token (an eastbound out fan stays at its port and its joiner merge comes to it). */
			/* A token held back by another still short of its own feature goes on in the next round; columns only grow, so the rounds end. */
			for (bool pushed = true; pushed;) {
			pushed = false;
			for (const bool push_feature : {false, true}) {
				for (const RowChange &change : row_changes) {
					if (change.larger == push_feature || (change.fan_edge != UINT32_MAX && fan_m[change.fan_edge] == 0)) continue;
					const int moved = push_feature ? change.feature : change.anchor;
					const int moved_root = root(moved);
					bool tied_to_fixed = false;
					for (int p = 0; p < pt_count; p++) tied_to_fixed |= root(p) == moved_root && (fixed_point[p] || (node_point[p] && !(push_feature && p == change.feature)));
					if (tied_to_fixed) continue;
					const size_t mark = positions.Mark();
					bool held = true;
					for (int p = 0; p < pt_count && held; p++) {
						if (root(p) != moved_root) held = positions.RestrictRange(p, positions.Lo(p), positions.Lo(p));
					}
					const int up = held ? positions.Up(moved) : positions.Lo(moved);
					positions.Undo(mark);
					if (up == positions.Lo(moved)) continue;
					reasons.push_back(JunctionPlacementReason{"next to feature", change.edge, UINT32_MAX, change.row});
					const size_t before = positions.Mark();
					if (!positions.RestrictRange(moved, up, up, static_cast<uint32_t>(reasons.size() - 1))) positions.Undo(before); else pushed = true;
				}
			}
			}
		}
		if (!ok) {
			std::string chain_text;
			failure.clear();
			for (uint32_t index : positions.Explain()) {
				const JunctionPlacementReason &reason = reasons[index];
				failure.push_back(reason);
				std::string parts;
				if (reason.edge != UINT32_MAX) parts += fmt::format("edge {}", reason.edge);
				if (reason.node != UINT32_MAX) parts += fmt::format("{}node {}", parts.empty() ? "" : ", ", reason.node);
				if (reason.row >= 0) parts += fmt::format("{}row {}", parts.empty() ? "" : ", ", reason.row);
				const auto note = notes.find(index);
				chain_text += fmt::format("{}{}{}{}", chain_text.empty() ? "" : " <- ", reason.rule, parts.empty() ? "" : " (" + parts + ")", note == notes.end() ? "" : " " + note->second);
			}
			if (error != nullptr) *error = "the placement rules contradict each other: " + chain_text;
			return false;
		}
		val.resize(pt_count);
		for (int i = 0; i < pt_count; i++) val[i] = positions.Lo(i);
		return true;
	};
	auto solve_failed = [&]() {
		JunctionTrackPlacement result;
		result.why = failure;
		return result;
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
			for (std::vector<Elem> *elems : {&chain[e], &chain_copy[e]}) {
				for (Elem &el : *elems) {
					if (el.k == Elem::K::Fan) {
						/* The fan's size is the distance of the port row to its item row, 0 until the port rows are set (the rows are known then). */
						el.m = fan_m[e];
						el.offs.clear();
						if (el.m != 0) {
							const int y_item = vy[item_pt[el.index]];
							const int y_port = port_row[el.slot];
							el.hand = (((el.port_first ? y_item - y_port : y_port - y_item) > 0) == (el.hin == 1)) ? 1 : -1;
							el.offs = bend_tiles(el.hin, el.hand, 2 * el.m, false);
						}
					} else if (el.k == Elem::K::Corner) {
						const int kk = el.joiner_div && rows_known ? std::abs(vy[el.first] - vy[el.last]) : k;
						el.offs = bend_tiles(el.hin, el.hand, 2 * kk + 1, true);
					} else if (el.k == Elem::K::SBend) {
						/* A horizontal S-bend of a row edge is sized by the rows it joins (rule of the lane change); known with the rows. */
						if (is_row_edge(e)) el.m = rows_known ? std::abs(vy[item_pt[el.index]] - vy[el.first]) : 0;
						el.offs.clear();
						if (el.m != 0) el.offs = bend_tiles(el.hin, el.hand, 2 * el.m, false);
					}
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
	/* The stack item of an edge's fan: its program's first token on an in edge, its last on an out edge, when that token is a Fan (R3). */
	auto fan_item = [&](uint32_t e) -> uint32_t {
		if (!is_in_out_edge(e) || programs.edges[e].empty()) return NO_ROW;
		const Step &token = graph.edges[e].from.node ? programs.edges[e].back() : programs.edges[e].front();
		return token.kind == StepKind::Fan ? token.index : NO_ROW;
	};
	auto edge_port = [&](uint32_t e) { return graph.edges[e].from.node ? graph.edges[e].to.index : graph.edges[e].from.index; };

	/* The row chains' points: allocated here, in this order, so the point ids of the reports stay; the chains' elements alias them and nothing else reads them. */
	struct RowPts {
		int fif = 0, fil = 0, fof = 0, fol = 0; ///< In and out fan: the points of the in and out chains' Fan elements.
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

	/* The row edges' chains. Their elements use the points of row_pts (the fan, lane split and merge, head points allocated above, so the point ids of the reports do not move); a doubled leg's second chain holds copy 1: the lane split's S-bend, then its heads. */
	const int base_pts = pt_count; ///< The points before the row chains' S-bends; the columns reported are these.
	for (uint32_t e = 0; e < edge_count; e++) {
		if (!is_row_edge(e)) continue;
		const JunctionTrackEdge &edge = graph.edges[e];
		const int r = (end_level(edge.from) - 1) / 2;
		const RowPts &rp = row_pts[r];
		const int heading = east[r] ? 1 : 3;
		auto tile_elem = [&](int pt, int slot) {
			Elem el;
			el.first = el.last = pt;
			el.hin = el.hout = heading;
			el.slot = slot;
			return el;
		};
		std::vector<Elem> &elems = chain[e];
		if (e == in_edge[r] || e == out_edge[r]) {
			/* The program's tokens in travel order: the fan (the first token of an in edge, the last of an out edge, on the fan's own points: their row points are the chain's, R3) and S-bends beside it. */
			const bool in = e == in_edge[r];
			const std::vector<Step> &program = programs.edges[e];
			elems.push_back(in ? tile_elem(port_pt[edge.from.index], edge.from.index) : tile_elem(node_pt[edge.from.index], -1));
			for (size_t t = 0; t < program.size(); t++) {
				const Step &step = program[t];
				Elem el;
				el.hin = el.hout = heading;
				el.tok = static_cast<int>(t);
				el.index = step.index;
				if (step.kind == StepKind::Fan && t == (in ? 0 : program.size() - 1)) {
					el.k = Elem::K::Fan;
					el.slot = in ? edge.from.index : edge.to.index;
					el.port_first = in;
					el.first = in ? rp.fif : rp.fof;
					el.last = in ? rp.fil : rp.fol;
				} else if (step.kind == StepKind::SBend) {
					el.k = Elem::K::SBend;
					el.hand = step.hand;
					el.first = new_pt();
					el.last = new_pt();
				} else {
					return fail("a token of an in or out edge other than its fan and S-bends is not ruled yet");
				}
				elems.push_back(el);
			}
			elems.push_back(in ? tile_elem(node_pt[edge.to.index], -1) : tile_elem(port_pt[edge.to.index], edge.to.index));
		} else if (!doubled_row(r)) {
			elems.push_back(tile_elem(node_pt[edge.from.index], -1));
			elems.push_back(tile_elem(node_pt[edge.to.index], -1));
		} else {
			const std::vector<Step> &program = programs.edges[e];
			std::vector<Elem> &copy = chain_copy[e];
			const auto with_role = [](const std::vector<Step> &tokens, Step::Role role) {
				size_t t = 0;
				while (t < tokens.size() && !(tokens[t].kind == StepKind::SBend && tokens[t].role == role)) t++;
				return t;
			};
			const size_t merge_token = with_role(program, Step::Role::LaneMerge);
			const size_t split_token = with_role(programs.copies[e], Step::Role::LaneSplit);
			if (merge_token == program.size() || split_token == programs.copies[e].size()) return fail("a doubled leg without its lane split and merge is not ruled yet");
			Elem bend0, bend1;
			bend0.k = bend1.k = Elem::K::SBend;
			bend0.hin = bend0.hout = bend1.hin = bend1.hout = heading;
			bend0.hand = program[merge_token].hand;
			bend1.hand = programs.copies[e][split_token].hand;
			bend0.index = program[merge_token].index;
			bend1.index = programs.copies[e][split_token].index;
			bend0.tok = static_cast<int>(merge_token);
			bend1.tok = static_cast<int>(split_token);
			bend0.first = rp.sb0_first;
			bend0.last = rp.lane_merge;
			bend1.first = rp.lane_split;
			bend1.last = rp.sb1_last;
			elems.push_back(tile_elem(node_pt[edge.from.index], -1));
			copy.push_back(tile_elem(node_pt[edge.from.index], -1));
			copy.push_back(bend1);
			for (size_t b = 0; b < bridges_of[e].size(); b++) {
				const JunctionTrackBridge &bridge = structure.bridges[bridges_of[e][b]];
				Elem h0 = tile_elem(0, -1), h1 = tile_elem(0, -1);
				h0.k = h1.k = Elem::K::HeadsPair;
				for (size_t t = 0; t < program.size(); t++) {
					if (program[t].kind == StepKind::Over && program[t].index == bridge.crossed[0]) h0.tok = static_cast<int>(t);
				}
				for (size_t t = 0; t < programs.copies[e].size(); t++) {
					if (programs.copies[e][t].kind == StepKind::Over && programs.copies[e][t].index == bridge.crossed[0]) h1.tok = static_cast<int>(t);
				}
				h0.first = rp.nh0[b];
				h0.last = rp.fh0[b];
				h1.first = rp.nh1[b];
				h1.last = rp.fh1[b];
				h0.index = north_item[r]; // The rows the heads lie on: the exit item (copy 0) and the lane item (copy 1).
				h1.index = lane_item[r];
				elems.push_back(h0);
				copy.push_back(h1);
			}
			elems.push_back(bend0);
			elems.push_back(tile_elem(node_pt[edge.to.index], -1));
			copy.push_back(tile_elem(node_pt[edge.to.index], -1));
		}
	}

	/* The row changes of kinds 1-4 (briefs/P3b-every-row-change-leans.md, "The rule") in the objective's order: edges ascending, an edge's tokens ascending, its copies after them. Each leans toward the feature it serves; only one whose feature lies toward larger columns can move. */
	for (uint32_t e = 0; e < edge_count; e++) {
		if (is_row_edge(e)) {
			const int r = (end_level(graph.edges[e].from) - 1) / 2;
			const bool eastward = east[r];
			const Elem *fan = nullptr; // The chain's Fan element, if the edge has one.
			for (const Elem &el : chain[e]) {
				if (el.k == Elem::K::Fan) fan = &el;
			}
			if (e == in_edge[r] && fan != nullptr) {
				row_changes.push_back({fan->first, node_pt[graph.edges[e].to.index], eastward, e, e, r, fan->tok, 0}); // Kind 1, first edge: with the travel, toward its node.
			} else if (e == out_edge[r] && fan != nullptr) {
				row_changes.push_back({fan->first, node_pt[graph.edges[e].from.index], !eastward, e, e, r, fan->tok, 0}); // Kind 1, last edge: against the travel.
			} else if (e == leg_edge[r] && doubled_row(r)) {
				/* A doubled leg: copy 0's chain is [Tile, HeadsPair.., SBend, Tile], copy 1's [Tile, SBend, HeadsPair.., Tile]. */
				const Elem &merge_bend = chain[e][chain[e].size() - 2];
				const Elem &last_heads = chain[e][chain[e].size() - 3];
				const Elem &split_bend = chain_copy[e][1];
				row_changes.push_back({merge_bend.first, last_heads.last, !eastward, UINT32_MAX, e, r, merge_bend.tok, 0}); // Kind 3: the lane merge, against the travel.
				row_changes.push_back({split_bend.first, chain[e][1].first, eastward, UINT32_MAX, e, r, split_bend.tok, 1}); // Kind 2: the lane split, with the travel.
			}
			continue;
		}
		if (!programs.copies[e].empty()) continue;
		const std::vector<Step> &program = programs.edges[e];
		for (const Elem &el : chain[e]) {
			if (el.k != Elem::K::SBend || VX[el.hin] == 0) continue;
			/* Kind 4: toward the node at the end of the edge when no emitting token follows, toward the one at its start when none precedes. */
			bool before = false, after = false;
			for (size_t t = 0; t < program.size(); t++) {
				if (program[t].kind == StepKind::Under || static_cast<int>(t) == el.tok) continue;
				(static_cast<int>(t) < el.tok ? before : after) = true;
			}
			if (before == after) continue;
			const JunctionTrackEnd &end = before ? graph.edges[e].to : graph.edges[e].from;
			row_changes.push_back({el.first, end.node ? node_pt[end.index] : port_pt[end.index], VX[el.hin] * (before ? 1 : -1) > 0, UINT32_MAX, e, -1, el.tok, 0});
		}
	}

	/* The objective's order is the graph's, not the points' (R2): by (edge, copy, token). */
	std::ranges::stable_sort(row_changes, [](const RowChange &a, const RowChange &b) { return std::array<int, 3>{static_cast<int>(a.edge), a.copy, a.token} < std::array<int, 3>{static_cast<int>(b.edge), b.copy, b.token}; });

	/* Symbolic tiles of an edge: an anchor point, a column offset, a row; in travel order. */
	struct SymTile {
		int pt;
		int dx;
		int y;
	};
	auto edge_tiles_of = [&](const std::vector<Elem> &elems) {
		std::vector<SymTile> tiles;
		for (size_t i = 0; i < elems.size(); i++) {
			const Elem &el = elems[i];
			if (el.k == Elem::K::Tile) {
				tiles.push_back({el.first, 0, el.slot >= 0 ? port_row[el.slot] : vy[el.first]});
			} else if (el.k == Elem::K::HeadsPair) {
				tiles.push_back({el.first, 0, vy[el.first]});
				tiles.push_back({el.last, 0, vy[el.last]});
			} else if (el.k == Elem::K::Fan) {
				/* The fan starts on the port row (an in edge's) or on its item row (an out edge's). */
				const int y = el.port_first ? port_row[el.slot] : vy[item_pt[el.index]];
				for (const TileIndexDiffC &o : el.offs) tiles.push_back({el.first, o.x, y + o.y});
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
	auto edge_tiles = [&](uint32_t e) { return edge_tiles_of(chain[e]); };

	/* The curves of every route (D217): per lane path and copy, in travel order, each curve as its bend's anchor point (the bend's first half tile), its hand and its element, and the route's units up to it in two forms. In the X form (rows known, one horizontal heading s per route) the units are xoff + 2 s (x[anchor] - x[the route's start]): along a horizontal stretch of that heading every column adds 2 units, a vertical stretch adds the fixed units of its rows, and a bend adds what its own pieces do. In the Y form (a segment of curves with only vertical runs between them, vertical heading sv) the units from the segment's first curve are yoff + 2 sv (y[anchor] - y[first anchor]). Row bends (fans, lane S-bends) lie between horizontal runs, so they have no segment (seg -1) and their curves pair in the X form only. */
	struct SymCurve {
		int pt;
		int xoff;
		int yoff;
		int seg;
		JunctionCurveHand hand;
		uint32_t edge;
		int row;
	};
	struct SymRoute {
		size_t path = 0;
		int copy = 0;
		int start = 0; ///< The route's start point.
		int sign = 1; ///< s.
		std::vector<int> seg_sign; ///< Per segment, sv.
		std::vector<SymCurve> curves;
	};
	auto doubled_edge = [&](uint32_t e) {
		return std::ranges::any_of(bridges_of[e], [&](int b) { return structure.bridges[b].doubled; });
	};
	/* With numeric_y false (the Y phase) the rows are unknown: the X form is not made and the row bends are left out. */
	std::map<std::tuple<int, int, int, bool>, JunctionRaster> bend_rasters; ///< The rasters of the bends the walk meets, by heading, hand, half pieces and corner.
	auto sym_routes = [&](std::vector<SymRoute> &routes, bool numeric_y) {
		/* A bend of the route: the anchor point and row of its first half tile, the heading it starts on, its hand, its half pieces. */
		struct Bend {
			int pt, y, heading, hand, halves;
			bool corner;
			uint32_t edge;
			int row;
			bool on_row;
		};
		routes.clear();
		for (size_t p = 0; p < paths.size(); p++) {
			const std::vector<uint32_t> &edges = graph.path_edges[p];
			const JunctionSide side = group_of(paths[p].from).side;
			if (side == JunctionSide::N) continue; // Not ruled yet (RenderJunctionTrackPlacement fails on it).
			const int start_heading = side == JunctionSide::W ? 1 : side == JunctionSide::E ? 3 : 0;
			const int start_y = !numeric_y ? 0 : (side == JunctionSide::S ? vy[port_pt[paths[p].from]] : port_row[paths[p].from]);
			const bool has_doubled = std::ranges::any_of(edges, doubled_edge);
			for (int c = 0; c < (has_doubled ? 2 : 1); c++) {
				std::vector<Bend> bends;
				std::vector<int> row_heading; ///< Per bend, the heading of the row edge crossed just before it, or -1.
				int pending_row = -1;
				for (uint32_t e : edges) {
					const std::vector<Step> &tokens = c == 1 && doubled_edge(e) ? programs.copies[e] : programs.edges[e];
					if (is_row_edge(e)) {
						const int r = (end_level(graph.edges[e].from) - 1) / 2;
						const int heading = east[r] ? 1 : 3;
						pending_row = heading;
						if (!numeric_y) continue;
						row_heading.resize(bends.size(), -1);
						/* Every bend token of the edge's chain, in travel order (R4, R5): the fan, an S-bend, a doubled leg's lane split and merge (copy 1's are in its own chain); a bridge's heads are no bend. */
						for (size_t t = 0; t < tokens.size(); t++) {
							for (const Elem &el : c == 1 && doubled_edge(e) ? chain_copy[e] : chain[e]) {
								if (el.tok != static_cast<int>(t) || (el.k != Elem::K::Fan && el.k != Elem::K::SBend)) continue;
								row_heading.resize(bends.size(), -1);
								if (el.k == Elem::K::Fan) {
									if (fan_m[e] == 0) continue;
									const int y_fan = el.port_first ? port_row[el.slot] : vy[item_pt[el.index]];
									const int y_far = el.port_first ? vy[item_pt[el.index]] : port_row[el.slot];
									bends.push_back({el.first, y_fan, heading, ((y_far > y_fan) == (heading == 1)) ? 1 : -1, 2 * fan_m[e], false, e, r, true});
								} else {
									bends.push_back({el.first, vy[el.first], heading, el.hand, 2 * std::abs(vy[item_pt[el.index]] - vy[el.first]), false, e, r, true});
								}
							}
						}
						continue;
					}
					for (size_t t = 0; t < tokens.size(); t++) {
						if (tokens[t].kind != StepKind::Corner && tokens[t].kind != StepKind::SBend) continue;
						for (const Elem &el : chain[e]) {
							if (el.tok != static_cast<int>(t)) continue;
							row_heading.resize(bends.size(), -1);
							row_heading.push_back(pending_row);
							pending_row = -1;
							bends.push_back({el.first, numeric_y ? vy[el.first] : 0, el.hin, el.hand, static_cast<int>(el.offs.size()), el.k == Elem::K::Corner, e, -1, false});
						}
					}
				}
				row_heading.resize(bends.size(), -1);

				/* Each bend's pieces: its curves with the units before them, the offsets of the axis piece before it and the closing axis piece from the first half tile, and the heading it leaves on. */
				struct Pieces {
					std::vector<std::pair<int, JunctionCurveHand>> curves; ///< Units from the start of the axis piece before the bend, hand.
					int before_dx, before_dy, close_dx, close_dy, close_units, heading;
				};
				std::vector<Pieces> pieces;
				for (const Bend &b : bends) {
					auto cached = bend_rasters.find({b.heading, b.hand, b.halves, b.corner});
					if (cached == bend_rasters.end()) cached = bend_rasters.emplace(std::make_tuple(b.heading, b.hand, b.halves, b.corner), RasterizeJunctionRoute({{0, 0}, JUNCTION_AXIS_TRACKDIR[b.heading], JunctionBendMoves(b.hand, b.halves, b.corner)})).first;
					const JunctionRaster &raster = cached->second;
					if (!raster.error.empty() || static_cast<int>(raster.pieces.size()) != b.halves + 2) { build_why = "a bend of edge " + str(b.edge) + " has no raster"; return false; }
					Pieces q{{}, raster.pieces[0].tile.x - raster.pieces[1].tile.x, raster.pieces[0].tile.y - raster.pieces[1].tile.y, raster.pieces.back().tile.x - raster.pieces[1].tile.x, raster.pieces.back().tile.y - raster.pieces[1].tile.y, 0, -1};
					for (size_t i = 0; i < raster.pieces.size(); i++) {
						if (i > 0) {
							const DirDiff diff = DirDifference(TrackdirToDirection(raster.pieces[i - 1].trackdir), TrackdirToDirection(raster.pieces[i].trackdir));
							if (diff == DirDiff::Left45 || diff == DirDiff::Right45) q.curves.emplace_back(q.close_units, diff == DirDiff::Left45 ? JunctionCurveHand::Left : JunctionCurveHand::Right);
						}
						if (i + 1 < raster.pieces.size()) q.close_units += raster.pieces[i].units;
					}
					for (int i = 0; i < 4; i++) {
						if (JUNCTION_AXIS_TRACKDIR[i] == raster.pieces.back().trackdir) q.heading = i;
					}
					if (q.heading < 0) { build_why = "a bend of edge " + str(b.edge) + " does not end on an axis piece"; return false; }
					pieces.push_back(std::move(q));
				}

				/* The one horizontal heading of the route up to its last bend (the X form's s). */
				SymRoute route;
				route.path = p;
				route.copy = c;
				route.start = port_pt[paths[p].from];
				route.sign = 0;
				auto note = [&](int heading) { return VX[heading] == 0 || (route.sign == 0 ? (route.sign = VX[heading]) != 0 : route.sign == VX[heading]); };
				bool one_heading = note(start_heading);
				for (size_t i = 0; i + 1 < pieces.size(); i++) one_heading = one_heading && note(pieces[i].heading);
				if (!one_heading) { build_why = "path " + str(static_cast<int>(p)) + " has two horizontal headings"; return false; }
				if (route.sign == 0) route.sign = 1;

				/* The segments: a bend on a row, or one that follows a horizontal run, starts one; a bend that follows a vertical run joins it. Its vertical heading sv is that of its first vertical run. */
				std::vector<int> seg_of(bends.size(), -1);
				{
					int h = start_heading, cur = -1;
					for (size_t i = 0; i < bends.size(); i++) {
						const int in_heading = row_heading[i] >= 0 ? row_heading[i] : h;
						if (bends[i].on_row) {
							cur = -1;
						} else {
							if (cur < 0 || VX[in_heading] != 0) {
								route.seg_sign.push_back(0);
								cur = static_cast<int>(route.seg_sign.size()) - 1;
							} else if (route.seg_sign[cur] == 0) {
								route.seg_sign[cur] = VY[in_heading];
							} else if (route.seg_sign[cur] != VY[in_heading]) {
								build_why = "a segment of path " + str(static_cast<int>(p)) + " has two vertical headings";
								return false;
							}
							seg_of[i] = cur;
						}
						h = pieces[i].heading;
						if (!bends[i].on_row && VX[h] != 0) cur = -1;
					}
				}

				/* w: the units at the route's current piece less 2 s times its columns from the start (X form). v: the same for the segment, less 2 sv times its rows (Y form, with the anchors' rows symbolic). */
				int w = 0, y = start_y, heading = start_heading, v = 0, last_seg = -2;
				for (size_t i = 0; i < bends.size(); i++) {
					const Bend &b = bends[i];
					const Pieces &q = pieces[i];
					const int in_heading = row_heading[i] >= 0 ? row_heading[i] : heading;
					if (numeric_y && VX[in_heading] == 0) {
						const int run = (b.y + q.before_dy - y) * VY[in_heading];
						if (run < 0) { build_why = "a vertical run before edge " + str(b.edge) + " is negative"; return false; }
						w += 2 * run;
					}
					if (seg_of[i] != last_seg) v = 0;
					last_seg = seg_of[i];
					const int ysign = seg_of[i] < 0 ? 0 : (route.seg_sign[seg_of[i]] != 0 ? route.seg_sign[seg_of[i]] : 1);
					for (const auto &[units, hand] : q.curves) route.curves.push_back({b.pt, numeric_y ? w + 2 * route.sign * q.before_dx + units : 0, v + 2 * ysign * q.before_dy + units, seg_of[i], hand, b.edge, b.row});
					w += 2 * route.sign * (q.before_dx - q.close_dx) + q.close_units;
					v += 2 * ysign * (q.before_dy - q.close_dy) + q.close_units;
					y = b.y + q.close_dy;
					heading = q.heading;
				}
				routes.push_back(std::move(route));
			}
		}
		return true;
	};

	/**
	 * One piece of a row's chains in the walk of the occupancy and the facts (R5): a tile (a port's, a bridge head), a run of straight tiles strictly between two points, or a bend at a point. The row of a piece is a number (a port's row and a fan's ends are not points' rows).
	 * copy: 0 or 1 for a doubled leg's copies, -1 for the part both share.
	 */
	struct RowPiece {
		enum class T : uint8_t { Tile, Run, Bend } t;
		int a, b; ///< Tile, Bend: the anchor point a. Run: from a to b.
		int y;
		int copy;
		uint32_t edge;
		int hand, halves;
		bool port; ///< A port's tile (it has no fact: the port's column is fixed).
		bool fact; ///< A bridge head's tile (a straight tile fact).
	};
	/* Row r's pieces in order: the in edge, the leg, the out edge, each read from its chain(s). */
	auto row_pieces = [&](int r) {
		std::vector<RowPiece> pieces;
		/* An edge's chain read in travel order: its tiles at the ports, its fan and S-bends, the runs between its elements (an empty fan has no run to its port). */
		auto walk = [&](uint32_t e) {
			const std::vector<Elem> &elems = chain[e];
			for (size_t i = 0; i < elems.size(); i++) {
				const Elem &el = elems[i];
				int y_end = vy[el.last];
				if (el.k == Elem::K::Tile) {
					if (el.slot >= 0) {
						y_end = port_row[el.slot];
						pieces.push_back({RowPiece::T::Tile, el.first, 0, y_end, 0, e, 0, 0, true, false});
					}
				} else if (el.k == Elem::K::Fan) {
					const int y_item = vy[item_pt[el.index]];
					const int y_port = port_row[el.slot];
					y_end = el.port_first ? y_item : y_port;
					if (el.m != 0) pieces.push_back({RowPiece::T::Bend, el.first, 0, el.port_first ? y_port : y_item, 0, e, el.hand, static_cast<int>(el.offs.size()), false, false});
				} else if (el.k == Elem::K::SBend) {
					pieces.push_back({RowPiece::T::Bend, el.first, 0, vy[el.first], 0, e, el.hand, static_cast<int>(el.offs.size()), false, false});
				}
				if (i + 1 >= elems.size() || elems[i + 1].first == el.last) continue;
				const Elem &next = elems[i + 1];
				if ((el.k == Elem::K::Tile && el.slot >= 0 && next.k == Elem::K::Fan && next.m == 0) || (el.k == Elem::K::Fan && el.m == 0 && next.k == Elem::K::Tile && next.slot >= 0)) continue;
				pieces.push_back({RowPiece::T::Run, el.last, next.first, y_end, 0, e, 0, 0, false, false});
			}
		};
		walk(in_edge[r]);
		const uint32_t leg = leg_edge[r];
		if (!doubled_row(r)) {
			walk(leg);
		} else {
			/* A doubled leg: copy 0's chain is [Tile, HeadsPair.., SBend, Tile], copy 1's [Tile, SBend, HeadsPair.., Tile]; before the lane split and after the lane merge the two copies share one lane (copy -1). */
			const std::vector<Elem> &c0 = chain[leg];
			const std::vector<Elem> &c1 = chain_copy[leg];
			const size_t g = c0.size() - 3;
			const int nex = c0.front().first, njm = c0.back().first;
			const Elem &bend0 = c0[g + 1];
			const Elem &bend1 = c1[1];
			const int y_exit = vy[nex], y_lane = vy[njm];
			auto run = [&](int a, int b, int y, int copy) { pieces.push_back({RowPiece::T::Run, a, b, y, copy, leg, 0, 0, false, false}); };
			auto head = [&](int pt, int y, int copy) { pieces.push_back({RowPiece::T::Tile, pt, 0, y, copy, leg, 0, 0, false, true}); };
			run(nex, bend1.first, y_exit, -1);
			run(bend1.first, c0[1].first, y_exit, 0);
			for (size_t k = 0; k < g; k++) {
				head(c0[1 + k].first, y_exit, 0);
				head(c0[1 + k].last, y_exit, 0);
				head(c1[2 + k].first, y_lane, 1);
				head(c1[2 + k].last, y_lane, 1);
				if (k + 1 < g) {
					run(c0[1 + k].last, c0[2 + k].first, y_exit, 0);
					run(c1[2 + k].last, c1[3 + k].first, y_lane, 1);
				}
			}
			run(c0[g].last, bend0.first, y_exit, 0);
			run(bend1.last, c1[2].first, y_lane, 1);
			run(c1[1 + g].last, bend0.last, y_lane, 1);
			run(bend0.last, njm, y_lane, -1);
			for (const Elem *bend : {&bend0, &bend1}) pieces.push_back({RowPiece::T::Bend, bend->first, 0, y_exit, bend == &bend0 ? 0 : 1, leg, bend->hand, static_cast<int>(bend->offs.size()), false, false});
		}
		walk(out_edge[r]);
		return pieces;
	};

	/* The tracks the routes build on each node's tile, and the headings of the routes that go straight through it (JunctionRoutesHeightsOk reads the union per tile): the piece of a route on its node's tile is the half that leaves or joins there, or an axis piece. */
	std::vector<TrackBits> node_bits(graph.nodes.size());
	std::vector<std::vector<int>> node_straight(graph.nodes.size());
	auto compute_node_tracks = [&]() {
		std::ranges::fill(node_bits, TrackBits{});
		for (std::vector<int> &headings : node_straight) headings.clear();
		std::map<std::pair<const Elem *, bool>, TrackBits> half_cache;
		auto bend_half = [&](const Elem &el, bool last) {
			const auto cached = half_cache.find({&el, last});
			if (cached != half_cache.end()) return cached->second;
			const size_t halves = el.offs.size();
			const JunctionRaster raster = RasterizeJunctionRoute({{0, 0}, JUNCTION_AXIS_TRACKDIR[el.hin], JunctionBendMoves(el.hand, static_cast<int>(halves), el.k == Elem::K::Corner)});
			const TrackBits half = !raster.error.empty() || raster.pieces.size() != halves + 2 ? TrackBits{} : TrackBits{TrackdirToTrack(raster.pieces[last ? halves : 1].trackdir)};
			half_cache[{&el, last}] = half;
			return half;
		};
		for (const std::vector<uint32_t> &edges : graph.path_edges) {
			for (size_t i = 0; i + 1 < edges.size(); i++) {
				if (!graph.edges[edges[i]].to.node) continue;
				const uint32_t node = graph.edges[edges[i]].to.index;
				const uint32_t e_in = edges[i];
				const uint32_t e_out = edges[i + 1];
				const Elem *leaving = nullptr;
				const Elem *joining = nullptr;
				if (!is_row_edge(e_out) && chain[e_out].size() > 2 && chain[e_out][1].first == chain[e_out][0].first && chain[e_out][1].k != Elem::K::Heads) leaving = &chain[e_out][1];
				if (!is_row_edge(e_in) && chain[e_in].size() > 2 && chain[e_in][chain[e_in].size() - 2].last == chain[e_in].back().first && chain[e_in][chain[e_in].size() - 2].k != Elem::K::Heads) joining = &chain[e_in][chain[e_in].size() - 2];
				if (leaving != nullptr) {
					node_bits[node] = node_bits[node] | bend_half(*leaving, false);
				} else if (joining != nullptr) {
					node_bits[node] = node_bits[node] | bend_half(*joining, true);
				} else {
					const int heading = is_row_edge(e_out) ? (east[(end_level(graph.edges[e_out].from) - 1) / 2] ? 1 : 3) : chain[e_out][0].hout;
					node_bits[node] = node_bits[node] | TrackBits{TrackdirToTrack(JUNCTION_AXIS_TRACKDIR[heading])};
					if (std::ranges::find(node_straight[node], heading) == node_straight[node].end()) node_straight[node].push_back(heading);
				}
			}
		}
	};

	/* The flat solve (D217): the least columns of the relations added so far, with no facts and no floors; false when they contradict each other. */
	auto flat_solve = [&](std::vector<int> &val) {
		int bound = 1;
		for (const Cons &cn : cons) bound += std::max(cn.k, 0);
		JunctionPositions positions(pt_count, 0, bound);
		bool ok = positions.RestrictRange(0, 0, 0);
		for (const Cons &cn : cons) {
			if (!ok) break;
			ok = positions.AddDifference(cn.v, cn.u, cn.k);
		}
		if (!ok) return false;
		val.resize(pt_count);
		for (int i = 0; i < pt_count; i++) val[i] = positions.Lo(i);
		return true;
	};

	/** The half pieces of a bend as track bits, one per half tile as bend_tiles lists them; empty when the bend has no raster. */
	auto bend_bits = [&](int heading, int hand, int halves, bool corner) {
		auto cached = bend_rasters.find({heading, hand, halves, corner});
		if (cached == bend_rasters.end()) cached = bend_rasters.emplace(std::make_tuple(heading, hand, halves, corner), RasterizeJunctionRoute({{0, 0}, JUNCTION_AXIS_TRACKDIR[heading], JunctionBendMoves(hand, halves, corner)})).first;
		const JunctionRaster &raster = cached->second;
		std::vector<TrackBits> bits;
		if (!raster.error.empty() || static_cast<int>(raster.pieces.size()) != halves + 2) return bits;
		for (int i = 1; i <= halves; i++) bits.push_back(TrackBits{TrackdirToTrack(raster.pieces[i].trackdir)});
		return bits;
	};

	/* The occupancy family (D217): unrelated tracks never share or touch a tile. */
	auto add_occupancy = [&](const std::vector<int> &flat) -> bool {
		/** One occupant of a row: the columns [left, right], each end a point and an offset, with the tracks of its tiles and what it belongs to. */
		struct Occupant {
			int row;
			int lp, ldx, rp, rdx;
			TrackBits bits;
			uint32_t edge; ///< The graph edge, UINT32_MAX for a node's tile.
			int copy; ///< 0 or 1, -1 for the part both copies share.
			uint32_t node; ///< The node of a node's tile, else UINT32_MAX.
			int footprint = 0; ///< A prio element's footprint: +1 when it lies downstream of its tile column (columns grow), -1 when downstream is lower; 0 for tracks.
		};
		std::map<int, std::vector<Occupant>> by_row;
		auto flat_left = [&](const Occupant &o) { return flat[o.lp] + o.ldx; };
		auto flat_right = [&](const Occupant &o) { return flat[o.rp] + o.rdx; };
		auto add_tile = [&](int pt, int dx, int y, TrackBits bits, uint32_t edge, int copy) {
			std::vector<Occupant> &row = by_row[y];
			if (!row.empty() && row.back().edge == edge && row.back().copy == copy && row.back().node == UINT32_MAX && row.back().lp == pt && row.back().ldx == dx && row.back().rp == pt && row.back().rdx == dx) {
				row.back().bits = row.back().bits | bits;
				return;
			}
			row.push_back({y, pt, dx, pt, dx, bits, edge, copy, UINT32_MAX});
		};
		/* The tiles strictly between points a and b on row y, a before b in the sense sgn; nothing when there are none at the flat solve. */
		auto add_run = [&](int a, int b, int y, int sgn, uint32_t edge, int copy) {
			Occupant run{y, sgn > 0 ? a : b, 1, sgn > 0 ? b : a, -1, TrackBits{Track::X}, edge, copy, UINT32_MAX};
			if (flat_left(run) > flat_right(run)) return;
			by_row[y].push_back(run);
		};
		auto add_bend = [&](int pt, int y, int heading, int hand, int halves, bool corner, uint32_t edge, int copy, bool skip_first, bool skip_last) {
			const std::vector<TileIndexDiffC> offs = bend_tiles(heading, hand, halves, corner);
			const std::vector<TrackBits> bits = bend_bits(heading, hand, halves, corner);
			if (bits.size() != offs.size()) return;
			for (size_t j = 0; j < offs.size(); j++) {
				if ((skip_first && j == 0) || (skip_last && j + 1 == offs.size())) continue;
				add_tile(pt, offs[j].x, y + offs[j].y, bits[j], edge, copy);
			}
		};
		std::vector<uint32_t> node_of_pt(pt_count, UINT32_MAX);
		for (uint32_t i = 0; i < node_pt.size(); i++) node_of_pt[node_pt[i]] = i;
		for (uint32_t i = 0; i < node_pt.size(); i++) by_row[vy[node_pt[i]]].push_back({vy[node_pt[i]], node_pt[i], 0, node_pt[i], 0, node_bits[i], UINT32_MAX, 0, i});

		/* The chains of the edges that are not rows. */
		for (uint32_t e = 0; e < edge_count; e++) {
			if (is_row_edge(e)) continue;
			const std::vector<Elem> &elems = chain[e];
			for (size_t i = 0; i < elems.size(); i++) {
				const Elem &el = elems[i];
				if (el.k == Elem::K::Tile) {
					if (node_of_pt[el.first] == UINT32_MAX) add_tile(el.first, 0, vy[el.first], VX[el.hin] != 0 ? TrackBits{Track::X} : TrackBits{Track::Y}, e, 0);
				} else if (el.k == Elem::K::Heads) {
					const TrackBits head = VX[el.hin] != 0 ? TrackBits{Track::X} : TrackBits{Track::Y};
					add_tile(el.first, 0, vy[el.first], head, e, 0);
					add_tile(el.last, 0, vy[el.last], head, e, 0);
				} else {
					add_bend(el.first, vy[el.first], el.hin, el.hand, static_cast<int>(el.offs.size()), el.k == Elem::K::Corner, e, 0, node_of_pt[el.first] != UINT32_MAX, node_of_pt[el.last] != UINT32_MAX);
				}
				if (i + 1 >= elems.size() || elems[i + 1].first == el.last) continue;
				const Elem &next = elems[i + 1];
				if (VX[el.hout] == 0) {
					const int d = VY[el.hout];
					for (int y = vy[el.last] + d; d > 0 ? y < vy[next.first] : y > vy[next.first]; y += d) add_tile(el.last, 0, y, TrackBits{Track::Y}, e, 0);
				} else {
					add_run(el.last, next.first, vy[el.last], VX[el.hout], e, 0);
				}
			}
		}

		/* The rows' chains: in edge, leg (both copies of a doubled one), out edge. */
		for (int r = 0; r < rows; r++) {
			const int sgn = east[r] ? 1 : -1;
			const TrackBits axis{Track::X};
			for (const RowPiece &piece : row_pieces(r)) {
				switch (piece.t) {
					case RowPiece::T::Tile: add_tile(piece.a, 0, piece.y, axis, piece.edge, piece.copy); break;
					case RowPiece::T::Run: add_run(piece.a, piece.b, piece.y, sgn, piece.edge, piece.copy); break;
					case RowPiece::T::Bend: add_bend(piece.a, piece.y, east[r] ? 1 : 3, piece.hand, piece.halves, false, piece.edge, piece.copy, false, false); break;
				}
			}
		}

		/* The prio element's footprint (DESIGN-G1 12.4): on the side of the ML row opposite the joiner's approach, the row tiles from the joiner merge's tile to even_prio upstream; unrelated tracks may not stand on them nor touch the row's open far end. */
		for (int r = 0; r < rows; r++) {
			const uint32_t njm_node = graph.edges[out_edge[r]].from.index;
			const int njm = node_pt[njm_node];
			bool main_path = false;
			for (size_t pi = 0; pi < graph.path_edges.size() && !main_path; pi++) {
				if (std::ranges::find(graph.path_edges[pi], leg_edge[r]) == graph.path_edges[pi].end()) continue;
				const auto &mv = topology.movements[paths[pi].movement];
				main_path = topology.groups[mv.first].main && topology.groups[mv.second].main;
			}
			if (!main_path || !node_bits[njm_node].Test(Track::X)) continue;
			bool nw = false, se = false;
			for (Track t : node_bits[njm_node]) {
				if (t == Track::Upper || t == Track::Left) nw = true;
				if (t == Track::Lower || t == Track::Right) se = true;
			}
			if (nw == se) continue;
			const int sgn = east[r] ? 1 : -1;
			const int y_row = vy[item_pt[lane_item[r]]] + (nw ? 1 : -1);
			const DiagDirection side = nw ? DiagDirection::SE : DiagDirection::NW;
			const DiagDirection exit_edge = east[r] ? DiagDirection::SW : DiagDirection::NE;
			const TrackBits h2{JunctionCornerTrack(ReverseDiagDir(side), ReverseDiagDir(exit_edge))};
			by_row[y_row].push_back({y_row, njm, 0, njm, 0, h2, UINT32_MAX, 0, njm_node, sgn});
			by_row[y_row].push_back(sgn > 0 ? Occupant{y_row, njm, -even_prio, njm, -1, TrackBits{Track::X}, UINT32_MAX, 0, njm_node, sgn} : Occupant{y_row, njm, 1, njm, even_prio, TrackBits{Track::X}, UINT32_MAX, 0, njm_node, sgn});
		}

		/* Related occupants never constrain each other: the same edge and copy, a node's tile and an edge that starts or ends at the node. */
		auto at_node = [&](uint32_t edge, uint32_t node) { return edge != UINT32_MAX && ((graph.edges[edge].from.node && graph.edges[edge].from.index == node) || (graph.edges[edge].to.node && graph.edges[edge].to.index == node)); };
		auto related = [&](const Occupant &a, const Occupant &b) {
			if (a.edge != UINT32_MAX && a.edge == b.edge) return a.copy == b.copy || a.copy < 0 || b.copy < 0;
			if (a.edge == UINT32_MAX && b.edge == UINT32_MAX && a.node != UINT32_MAX && a.node == b.node) return true; // The two parts of one prio footprint.
			return (a.node != UINT32_MAX && at_node(b.edge, a.node)) || (b.node != UINT32_MAX && at_node(a.edge, b.node));
		};
		auto label = [&](const Occupant &o) { return o.node != UINT32_MAX ? fmt::format("node {}", o.node) : fmt::format("edge {}", o.edge); };
		for (auto &[y, occupants] : by_row) {
			/* On equal columns a footprint comes first in its travel direction: the unrelated track stands downstream of it. */
			std::ranges::stable_sort(occupants, [&](const Occupant &a, const Occupant &b) { return std::make_tuple(flat_left(a), flat_right(a), -a.footprint) < std::make_tuple(flat_left(b), flat_right(b), -b.footprint); });
			auto add_relation = [&](const Occupant &a, const Occupant &b, bool tie) -> bool {
				if (collecting) {
					if (tie) ref_shares.emplace(std::min(a.edge, b.edge), std::max(a.edge, b.edge));
					return true;
				}
				reasons.push_back(JunctionPlacementReason{"occupancy", b.edge, b.node, -1});
				const uint32_t reason = static_cast<uint32_t>(reasons.size() - 1);
				notes[reason] = fmt::format("[{} and {} on row {}]", label(a), label(b), y);
				const int apart = flat_left(b) - flat_right(a);
				if (tie) {
					/* A group is legal when the same edges share a tile in the reference: the state's flat solve without floors (checker-clean by the sweeps; when row edits arrive the reference becomes the cached root seed). One that appears only with floors is legal only when no two of its unrelated pieces share a track end on a tile edge. */
					const bool in_reference = ref_shares.contains({std::min(a.edge, b.edge), std::max(a.edge, b.edge)});
					for (const DiagDirection d : {DiagDirection::NE, DiagDirection::SE, DiagDirection::SW, DiagDirection::NW}) {
						if (!in_reference && a.bits.Any(DiagdirReachesTracks(d)) && b.bits.Any(DiagdirReachesTracks(d))) {
							occupancy_failed = true;
							failure.assign(1, reasons.back());
							if (error != nullptr) *error = "the placement rules contradict each other: occupancy (edge " + (a.edge != UINT32_MAX ? str(static_cast<int>(a.edge)) : std::string("-")) + ") " + notes[reason];
							return false;
						}
					}
					cons.push_back({b.lp, a.rp, apart - b.ldx + a.rdx, reason});
					cons.push_back({a.rp, b.lp, -(apart - b.ldx + a.rdx), reason});
				} else {
					const bool touch = a.bits.Any(DiagdirReachesTracks(ReverseDiagDir(DiagDirection::SW))) && b.bits.Any(DiagdirReachesTracks(ReverseDiagDir(DiagDirection::NE)));
					cons.push_back({b.lp, a.rp, (touch ? 2 : 1) - b.ldx + a.rdx, reason});
				}
				return true;
			};
			/* Occupants on one flat tile form a group, tied to each other; groups are neighbours in the order. */
			std::vector<std::pair<size_t, size_t>> groups;
			for (size_t i = 0; i < occupants.size(); i++) {
				if (!groups.empty() && occupants[i].footprint == 0 && occupants[groups.back().second].footprint == 0 && flat_left(occupants[groups.back().first]) == flat_left(occupants[i]) && flat_right(occupants[groups.back().first]) == flat_right(occupants[i])) {
					groups.back().second = i;
				} else {
					groups.emplace_back(i, i);
				}
			}
			for (size_t gi = 0; gi < groups.size(); gi++) {
				for (size_t i = groups[gi].first; i < groups[gi].second; i++) {
					if (!related(occupants[i], occupants[i + 1]) && !add_relation(occupants[i], occupants[i + 1], true)) return false;
				}
				if (gi + 1 == groups.size()) continue;
				bool any_related = false;
				TrackBits left_bits, right_bits;
				for (size_t i = groups[gi].first; i <= groups[gi].second; i++) {
					left_bits = left_bits | occupants[i].bits;
					for (size_t j = groups[gi + 1].first; j <= groups[gi + 1].second; j++) any_related = any_related || related(occupants[i], occupants[j]);
				}
				for (size_t j = groups[gi + 1].first; j <= groups[gi + 1].second; j++) right_bits = right_bits | occupants[j].bits;
				if (any_related) continue;
				Occupant a = occupants[groups[gi].second];
				Occupant b = occupants[groups[gi + 1].first];
				a.bits = left_bits;
				b.bits = right_bits;
				if (!add_relation(a, b, flat_left(b) <= flat_right(a) && a.footprint == 0 && b.footprint == 0)) return false;
			}
		}
		return true;
	};

	/* The heads of a doubled bridge hug its outermost crosser tiles by the flat solve's order (D80, D118, D217). */
	auto hug_heads = [&](const std::vector<int> &flat) {
		for (const HugCandidate &hug : hugs) {
			if (hug.tiles.empty()) continue;
			auto value = [&](const std::pair<int, int> &tile) { return flat[tile.first] + tile.second; };
			const std::pair<int, int> *low = &hug.tiles[0];
			const std::pair<int, int> *high = &hug.tiles[0];
			for (const std::pair<int, int> &tile : hug.tiles) {
				if (value(tile) < value(*low)) low = &tile;
				if (value(tile) > value(*high)) high = &tile;
			}
			why("bridge heads hug crossers", hug.edge, UINT32_MAX, hug.row);
			eq(0, hug.west, low->first, low->second - 1);
			eq(0, hug.east, high->first, high->second + 1);
		}
	};


	/* The bridges' middle lines and heads (D217): a Y-axis bridge's column and rows, an X-axis (doubled) bridge's row and columns. */
	auto collect_bridges = [&](std::vector<BridgeLine> &lines, std::vector<HeadRef> &heads) {
		lines.clear();
		heads.clear();
		for (uint32_t e = 0; e < edge_count; e++) {
			if (is_row_edge(e)) continue;
			for (const Elem &el : chain[e]) {
				if (el.k != Elem::K::Heads || VX[el.hin] != 0 || bridges_of[e].empty()) continue;
				const int b = bridges_of[e][0];
				lines.push_back({b, e, false, el.first, el.last, el.first});
				heads.push_back({b, e, el.first, el.first});
				heads.push_back({b, e, el.last, el.last});
			}
		}
		/* A doubled leg's bridges: the HeadsPair elements of its two chains, one pair per bridge in travel order; the line of a copy's heads is the row item its chain lies on. */
		for (int r = 0; r < rows; r++) {
			if (!doubled_row(r)) continue;
			const uint32_t leg = leg_edge[r];
			for (size_t k = 0; k < bridges_of[leg].size(); k++) {
				const int b = bridges_of[leg][k];
				for (const Elem *h : {&chain[leg][1 + k], &chain_copy[leg][2 + k]}) {
					const int line = item_pt[h->index];
					lines.push_back({b, leg, true, h->first, h->last, line});
					heads.push_back({b, leg, h->first, line});
					heads.push_back({b, leg, h->last, line});
				}
			}
		}
	};

	/* Bridge middle clearance in X (D217): a head whose row lies strictly within a Y-axis bridge's rows keeps its flat side of the bridge's column by one. */
	auto add_clearance_x = [&](const std::vector<int> &flat) {
		std::vector<BridgeLine> lines;
		std::vector<HeadRef> heads;
		collect_bridges(lines, heads);
		for (const BridgeLine &line : lines) {
			if (line.x_axis) continue;
			const int top = std::min(vy[line.a], vy[line.b]);
			const int bottom = std::max(vy[line.a], vy[line.b]);
			for (const HeadRef &head : heads) {
				if (head.bridge == line.bridge || vy[head.ypt] <= top || vy[head.ypt] >= bottom) continue;
				const int side = flat[head.xpt] - flat[line.line];
				if (side == 0) continue;
				reasons.push_back(JunctionPlacementReason{"bridge middle clearance", line.edge, UINT32_MAX, -1});
				const uint32_t reason = static_cast<uint32_t>(reasons.size() - 1);
				notes[reason] = fmt::format("[edge {} and the head of edge {}]", line.edge, head.edge);
				if (side > 0) cons.push_back({head.xpt, line.line, 1, reason}); else cons.push_back({line.line, head.xpt, 1, reason});
			}
		}
	};

	/* One round: the constraints of a phase. */
	auto build = [&]() {
		cons.clear();
		reasons.clear();
		fact_cons.clear();
		notes.clear();
		why_now = JunctionPlacementReason{};
		const bool x_phase = phase == 0;
		if (x_phase) hugs.clear();

		/* Stack rows. */
		why("stack origin");
		eq(1, item_pt[0], 0, 0);
		for (int i = 1; i < items; i++) {
			why("stack rows", UINT32_MAX, UINT32_MAX, static_cast<int>(structure.stack[i].row));
			if (structure.stack[i].row == structure.stack[i - 1].row) eq(1, item_pt[i], item_pt[i - 1], 1); else ge(1, item_pt[i], item_pt[i - 1], 1);
		}
		std::vector<int> layer1;
		for (uint32_t i = 0; i < graph.nodes.size(); i++) {
			const int level = order.node_level[i];
			if (level % 2 != 0) continue;
			const int b = level / 2;
			why("band gap", UINT32_MAX, i);
			if (b >= 1) ge(1, node_pt[i], item_pt[row_last[b - 1]], 1);
			if (b < rows) ge(1, item_pt[row_first[b]], node_pt[i], 1);
		}
		for (const JunctionTrackBandEvent &event : order.bands[rows]) {
			if (event.node && event.layer == 1) layer1.push_back(static_cast<int>(event.a));
		}
		for (size_t i = 0; i < layer1.size(); i++) {
			why("S edge", UINT32_MAX, static_cast<uint32_t>(layer1[i]));
			if (i > 0) eq(1, node_pt[layer1[i]], node_pt[layer1[0]], 0);
			eq(1, se_pt, node_pt[layer1[i]], JunctionRunFloor(FloorKind::Node, FloorKind::Port, spec, even_prio));
		}
		why("S port row");
		for (int s = 0; s < n; s++) {
			if (!group_of(s).main) eq(1, port_pt[s], se_pt, 0);
		}

		/* Ports. */
		std::vector<int> south;
		why("port columns");
		for (int s = 0; s < n; s++) {
			if (group_of(s).side == JunctionSide::W) eq(0, port_pt[s], 0, 0);
			if (group_of(s).side == JunctionSide::E) eq(0, port_pt[s], xe_pt, 0);
			if (group_of(s).side == JunctionSide::S) south.push_back(s);
		}
		int column = 0;
		why("S port spacing");
		for (size_t i = 1; i < south.size(); i++) {
			column += 1 + (slots[south[i]].group != slots[south[i - 1]].group ? 1 : 0);
			eq(0, port_pt[south[i]], port_pt[south[0]], column);
		}

		/* The rows' nodes: the exit split lies on the exit item's row, the joiner merge on the lane item's. */
		for (int r = 0; r < rows; r++) {
			const uint32_t exit_item = doubled_row(r) ? north_item[r] : lane_item[r];
			const JunctionTrackEdge &in = graph.edges[in_edge[r]];
			const JunctionTrackEdge &out = graph.edges[out_edge[r]];
			eq(1, node_pt[in.to.index], item_pt[exit_item], 0);
			eq(1, node_pt[out.from.index], item_pt[lane_item[r]], 0);
		}

		if (!x_phase) {
			/* The waiting room of an exit path, from its first split to the first merge after it: 2 dy - 1 >= 2 tl units, so dy >= tl + 1 tiles (rule WaitingRoom). */
			for (size_t p = 0; p < paths.size(); p++) {
				if (!group_of(paths[p].from).main || group_of(paths[p].to).main) continue;
				uint32_t cur_edge = edge_count;
				for (uint32_t e = 0; e < edge_count; e++) {
					if (!graph.edges[e].from.node && graph.edges[e].from.index == paths[p].from && std::ranges::count(graph.edges[e].paths, static_cast<uint16_t>(p)) > 0) cur_edge = e;
				}
				int split_node = -1;
				for (int guard = 0; guard < 64 && cur_edge < edge_count && graph.edges[cur_edge].to.node; guard++) {
					const int nd = graph.edges[cur_edge].to.index;
					if (split_node < 0 && graph.nodes[nd].split) {
						split_node = nd;
					} else if (split_node >= 0 && !graph.nodes[nd].split) {
						why("waiting room", cur_edge, static_cast<uint32_t>(nd));
						ge(1, node_pt[nd], node_pt[split_node], JunctionRunFloor(FloorKind::Node, FloorKind::Node, spec, even_prio));
						break;
					}
					uint32_t next_edge = edge_count;
					for (uint32_t e = 0; e < edge_count; e++) {
						if (graph.edges[e].from.node && graph.edges[e].from.index == nd && std::ranges::count(graph.edges[e].paths, static_cast<uint16_t>(p)) > 0) next_edge = e;
					}
					cur_edge = next_edge;
				}
			}
		}

		/* The chains of the other edges. */
		for (uint32_t e = 0; e < edge_count; e++) {
			const std::vector<Elem> &elems = chain[e];
			const JunctionTrackEdge &edge = graph.edges[e];
			if (is_row_edge(e) && doubled_edge(e)) {
				/* A doubled leg: copy 0's chain is [Tile, HeadsPair.., SBend, Tile], copy 1's [Tile, SBend, HeadsPair.., Tile]. The lane pair joins the two (copy 1's S-bend starts at the lane split, copy 0's ends at the lane merge) and the heads of copy 1 are copy 0's one tile upstream (D118). */
				const int r = (end_level(edge.from) - 1) / 2;
				const std::vector<Elem> &copy = chain_copy[e];
				const size_t g = elems.size() - 3;
				const int nex = elems.front().first, njm = elems.back().first;
				const Elem &bend0 = elems[g + 1];
				const Elem &bend1 = copy[1];
				auto heads0 = [&](size_t b) -> const Elem & { return elems[1 + b]; };
				auto heads1 = [&](size_t b) -> const Elem & { return copy[2 + b]; };
				why("row items", e, UINT32_MAX, r);
				eq(1, bend1.first, item_pt[north_item[r]], 0);
				eq(1, bend1.last, item_pt[lane_item[r]], 0);
				for (size_t b = 0; b < g; b++) {
					for (const Elem *h : {&heads0(b), &heads1(b)}) {
						eq(1, h->first, item_pt[h->index], 0);
						eq(1, h->last, item_pt[h->index], 0);
					}
				}
				eq(1, bend0.first, item_pt[north_item[r]], 0);
				eq(1, bend0.last, item_pt[lane_item[r]], 0);
				if (!x_phase) continue;

				const int s = east[r] ? 1 : -1;
				const int heading = east[r] ? 1 : 3;
				/* The lane split sits next to its bridge (D215); the run from the exit split to the lane split, and from the last heads to the lane merge, are rule minima, not exact: series bridges need the slack (DESIGN-G1 §11.9b). */
				why("lane split run", e, edge.from.index, r);
				along(heading, bend1.first, nex, JunctionRunFloor(FloorKind::Node, FloorKind::LaneSplit, spec, even_prio), false);
				along(heading, heads0(0).first, bend1.first, JunctionRunFloor(FloorKind::LaneSplit, FloorKind::Heads, spec, even_prio), true);
				eq(0, bend1.last, bend1.first, 0);
				along(heading, heads1(0).first, bend1.last, 1, true); // A shape, not a floor: copy 1 sits one tile nearer than copy 0, the head stagger (D118, D145).
				for (size_t b = 0; b < g; b++) {
					along(heading, heads0(b).first, heads1(b).first, 1, true); // Shape: the head stagger, copy 0 one past copy 1 (D118).
					along(heading, heads0(b).last, heads1(b).last, 1, true); // Shape: the head stagger (D118).
					if (b > 0) along(heading, heads0(b).first, heads0(b - 1).last, JunctionRunFloor(FloorKind::Heads, FloorKind::Heads, spec, even_prio), false);
				}
				why("series bridge heads", e, UINT32_MAX, r);
				along(heading, bend0.first, heads0(g - 1).last, 1, false); // Shape: one tile past copy 0's far head, the stagger (D118, D145); the floor is the lane merge's, next.
				eq(0, bend0.last, bend0.first, 0);
				along(heading, bend0.last, heads1(g - 1).last, JunctionRunFloor(FloorKind::Heads, FloorKind::LaneMerge, spec, even_prio), false);
				/* The element's row ends at step even_prio + 1, which must not meet the lane merge's S-bend (RECON-prio-element §3-4). */
				why("prio element", e, graph.edges[out_edge[r]].from.index, r);
				along(heading, njm, bend0.last, JunctionRunFloor(FloorKind::LaneMerge, FloorKind::PrioNode, spec, even_prio), false);

				/* Every crosser's tiles on the copy row and the lane row lie between its bridge's heads; the heads hug the outermost tiles by the flat solve's order, below (D80, D118, D145, D217). */
				const int u = -s; // The upstream step.
				why("bridge heads hug crossers", e, UINT32_MAX, r);
				for (size_t b = 0; b < g; b++) {
					const JunctionTrackBridge &bridge = structure.bridges[bridges_of[e][b]];
					HugCandidate hug{s > 0 ? heads0(b).first : heads0(b).last, s > 0 ? heads0(b).last : heads0(b).first, e, r, {}};
					for (uint32_t x : bridge.crossed) {
						if (!rows_known) break;
						const std::vector<SymTile> tiles = edge_tiles(x);
						for (int pass = 0; pass < 2; pass++) {
							const Elem &near_far = pass == 0 ? heads0(b) : heads1(b);
							const int y = vy[near_far.first];
							bool any = false;
							for (const SymTile &z : tiles) {
								if (z.y != y) continue;
								any = true;
								if (s > 0) {
									ge(0, z.pt, near_far.first, 1 - z.dx);
									ge(0, near_far.last, z.pt, 1 + z.dx);
								} else {
									ge(0, near_far.first, z.pt, 1 + z.dx);
									ge(0, z.pt, near_far.last, 1 - z.dx);
								}
								hug.tiles.emplace_back(z.pt, pass == 0 ? z.dx : z.dx - u);
							}
							if (!any) { build_why = "Z row " + str(y) + " edge " + str(x) + " row " + str(r); return false; }
						}
					}
					hugs.push_back(std::move(hug));
				}
				continue;
			}
			for (size_t i = 0; i < elems.size(); i++) {
				const Elem &el = elems[i];
				if (el.k == Elem::K::Fan) {
					/* The fan is its port row's S-bend: m - 1 columns across, first at the port row and last at its item row; empty, it lies on the port's column (R3). */
					why("fan size", e, UINT32_MAX, (end_level(edge.from) - 1) / 2);
					const int port = el.port_first ? elems[i - 1].first : elems[i + 1].first;
					if (el.m == 0) {
						eq(0, el.first, port, 0);
						eq(0, el.last, port, 0);
					} else {
						along(el.hin, el.last, el.first, el.m - 1, true);
					}
					eq(1, el.port_first ? el.last : el.first, item_pt[el.index], 0);
				} else if (el.k == Elem::K::Corner) {
					const int kk = k;
					const int dx = kk * (VX[el.hin] + VX[el.hout]);
					const int dy = kk * (VY[el.hin] + VY[el.hout]);
					if (el.joiner_div) {
						why("joiner_div Corner floor", e);
						ge(1, el.first, el.last, k);
						if (fixed != nullptr) {
							why("joiner_div Corner choice", e);
							eq(1, el.first, el.last, fixed->corner[e][el.tok]);
						}
						if (x_phase) {
							const int kp = fixed != nullptr ? fixed->corner[e][el.tok] : (rows_known ? std::abs(vy[el.first] - vy[el.last]) : k);
							eq(0, el.last, el.first, kp * (VX[el.hin] + VX[el.hout]));
						}
					} else {
						why("Corner size", e);
						eq(0, el.last, el.first, dx);
						eq(1, el.last, el.first, dy);
					}
					if (el.index != NO_ROW) eq(1, el.last, item_pt[el.index], 0);
				} else if (el.k == Elem::K::SBend) {
					const int side = (el.hin + el.hand + 4) % 4;
					why("S-bend size", e);
					if (el.hin == 0 || el.hin == 2) {
						eq(1, el.last, el.first, VY[el.hin] * (el.m - 1));
						if (fixed != nullptr) {
							if (VX[side] > 0) eq(0, el.last, el.first, el.m); else eq(0, el.first, el.last, el.m);
						} else {
							if (VX[side] > 0) ge(0, el.last, el.first, el.m); else ge(0, el.first, el.last, el.m);
						}
					} else {
						eq(1, el.last, el.first, VY[side]);
						eq(0, el.last, el.first, VX[el.hin] * 0);
					}
					if (el.index != NO_ROW) eq(1, el.last, item_pt[el.index], 0);
				} else if (el.k == Elem::K::Heads) {
					why("bridge heads", e);
					eq(VX[el.hin] != 0 ? 1 : 0, el.last, el.first, 0);
				}
				if (i + 1 >= elems.size()) continue;
				const Elem &next = elems[i + 1];
				if (next.first == el.last) continue;
				bool exact = false;
				const bool in_out = is_in_out_edge(e);
				/* A row edge's tiles name their kind: a port, the exit split of an in edge (Node), the joiner merge of an out edge (PrioNode), a fan (R3). */
				const auto floor_kind = [&](const Elem &o) {
					if (o.k == Elem::K::Corner) return FloorKind::Corner;
					if (o.k == Elem::K::SBend) return FloorKind::SBend;
					if (o.k == Elem::K::Heads) return FloorKind::Heads;
					if (o.k == Elem::K::Fan) return FloorKind::Fan;
					if (is_row_edge(e) && !in_out) return &o == &elems.front() ? FloorKind::Node : FloorKind::PrioNode; // An undoubled leg: the exit split to the joiner merge.
					if (!in_out) return FloorKind::Tile;
					return o.slot >= 0 ? FloorKind::Port : (edge.to.node ? FloorKind::Node : FloorKind::PrioNode);
				};
				const int dist = JunctionRunFloor(floor_kind(el), floor_kind(next), spec, even_prio);
				if (floor_kind(next) == FloorKind::PrioNode) {
					why("prio element", e, edge.to.index, (end_level(graph.edges[e].from) - 1) / 2); // The prio element's coverage is a floor on the joiner merge (D217).
				} else {
					why("chain run", e);
				}
				if (next.k == Elem::K::Tile && i + 2 == elems.size() && !edge.to.node) exact = true;
				if (el.k == Elem::K::Tile && i == 0 && !edge.from.node && !in_out) exact = true;
				const auto port_tile = [](const Elem &o) { return o.k == Elem::K::Tile && o.slot >= 0; };
				if (in_out && (port_tile(el) || port_tile(next))) {
					/* A run to a port is along the heading only: the port's row is a number, not a point (R3); an empty fan lies on its port already. */
					const Elem &fan = el.k == Elem::K::Fan ? el : next;
					if (fan.k == Elem::K::Fan && fan.m == 0) continue;
					along(el.hout, next.first, el.last, dist, exact);
					continue;
				}
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
				why("bridge over S-bend", e);
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

		if (!x_phase && !floors_off) {
			why("y floor");
			for (const std::pair<int, int> &floor : y_floors) ge(1, floor.first, 0, floor.second);
		}
		if (!x_phase && !skip_clearance) {
			for (const ClearanceY &c : clear_y) {
				reasons.push_back(JunctionPlacementReason{"bridge middle clearance", c.edge_b, UINT32_MAX, -1});
				notes[static_cast<uint32_t>(reasons.size() - 1)] = fmt::format("[edge {} and the head of edge {}]", c.edge_b, c.edge_h);
				cons.push_back({c.v, c.u, 1, static_cast<uint32_t>(reasons.size() - 1)});
			}
		}

		if ((!x_phase || rows_known) && spec.curve_limit) {
			/* The curve-gap family (D217): every pair JunctionCurveGaps returns for a route's curves. A pair in one segment (only vertical runs and fixed spans between the curves) is a difference of its anchors' rows in the Y phase; every other pair is a difference of its anchors' columns in the X phase. Pairs on one anchor are constants. */
			std::vector<SymRoute> routes;
			if (!sym_routes(routes, x_phase)) return false;
			/* One relation per (v, u): the largest of the pairs'. */
			struct Relation {
				int v, u, least;
				uint32_t edge;
				int row;
			};
			std::vector<Relation> relations;
			std::map<std::pair<int, int>, size_t> at;
			for (const SymRoute &route : routes) {
				std::vector<JunctionCurveHand> hands;
				for (const SymCurve &curve : route.curves) hands.push_back(curve.hand);
				for (const JunctionCurveGap &gap : JunctionCurveGaps(hands, spec.tl, spec.curve_speed, spec.reference_speed)) {
					const SymCurve &ci = route.curves[gap.i];
					const SymCurve &cj = route.curves[gap.j];
					const bool vertical = ci.seg >= 0 && ci.seg == cj.seg;
					if (vertical == x_phase) continue;
					/* The units from curve i to curve j are off_j - off_i + 2 sg (a_j - a_i), at least min_gap; sg is s (columns) or sv (rows). */
					const int sg = vertical ? (route.seg_sign[ci.seg] != 0 ? route.seg_sign[ci.seg] : 1) : route.sign;
					const int delta = vertical ? cj.yoff - ci.yoff : cj.xoff - ci.xoff;
					const int need = gap.min_gap - delta;
					const int least = need > 0 ? (need + 1) / 2 : -(-need / 2);
					if (ci.pt == cj.pt) {
						if (least > 0) { build_why = "curve gap " + str(gap.min_gap) + " between edge " + str(ci.edge) + " and edge " + str(cj.edge) + " is not kept: " + str(delta); return false; }
						continue;
					}
					const int v = sg > 0 ? cj.pt : ci.pt;
					const int u = v == cj.pt ? ci.pt : cj.pt;
					const auto [found, added] = at.emplace(std::make_pair(v, u), relations.size());
					if (added) {
						relations.push_back({v, u, least, cj.edge, cj.row});
					} else if (relations[found->second].least < least) {
						relations[found->second] = {v, u, least, cj.edge, cj.row};
					}
				}
			}
			for (const Relation &relation : relations) {
				why("curve gap", relation.edge, UINT32_MAX, relation.row);
				ge(x_phase ? 0 : 1, relation.v, relation.u, relation.least);
			}
		}

		if (x_phase && rows_known && !relations_only) {
			/* The occupancy family and the bridge heads follow the order of the flat solve of the relations above (D217); when the relations contradict each other the solve below explains it. */
			compute_node_tracks();
			std::vector<int> flat;
			if (flat_solve(flat)) {
				if (!add_occupancy(flat)) return false;
				hug_heads(flat);
				if (!skip_clearance) add_clearance_x(flat);
			}
		}

		if (x_phase && !floors_off) {
			why("x floor");
			for (const std::pair<int, int> &floor : x_floors) ge(0, floor.first, 0, floor.second);
		}

		if (x_phase && facts != nullptr && !relations_only) {
			/* The terrain's facts (DESIGN-G1 §14.4): every group of tiles gets the columns where its tile is buildable, every straight run on a row must be buildable on each tile it covers. Bridge middles are not checked: the node path checks no bridge piece either (JunctionNodeHeightsOk skips on_bridge). */
			const int high = facts->MaxX() - offset.x;
			std::vector<uint32_t> node_of_pt(pt_count, UINT32_MAX);
			for (uint32_t i = 0; i < node_pt.size(); i++) node_of_pt[node_pt[i]] = i;
			/* The height facts are the costly ones: they wait until every other fact of the block is in, and then look only at the columns a point can take under the relations and those facts (least and most, below). */
			std::vector<int> least(pt_count, 0);
			std::vector<int> most(pt_count, INT_MAX / 2);
			std::vector<std::function<void()>> pending;
			auto fact_reason = [&](uint32_t e, uint32_t node, int row, const std::string &note, const char *rule = "blocked tile") {
				reasons.push_back(JunctionPlacementReason{rule, e, node, static_cast<int32_t>(row)});
				if (!note.empty()) notes[static_cast<uint32_t>(reasons.size() - 1)] = note;
				return static_cast<uint32_t>(reasons.size() - 1);
			};
			auto holds = [&](JunctionFactKind kind, Axis axis, int x, int y) {
				int entry, exit;
				if (classes_only) return facts->Buildable(x, y);
				switch (kind) {
					case JunctionFactKind::Straight: return facts->Straight(x, y, axis, entry, exit);
					case JunctionFactKind::Switch: return facts->Switch(x, y);
					default: return facts->Half(x, y);
				}
			};
			/* Per (fact, axis, row), whether the fact holds at each column of the snapshot: every group on that row asks for it. */
			std::map<std::tuple<int, int, int>, std::vector<bool>> holding;
			auto holds_on_row = [&](JunctionFactKind kind, Axis axis, int y) -> const std::vector<bool> & {
				const auto key = std::make_tuple(static_cast<int>(kind), static_cast<int>(axis), y);
				auto found = holding.find(key);
				if (found != holding.end()) return found->second;
				std::vector<bool> row;
				for (int x = facts->MinX(); x <= facts->MaxX(); x++) row.push_back(holds(kind, axis, x, y));
				return holding.emplace(key, std::move(row)).first->second;
			};
			/* A group tile: anchor point pt, column offset dx, row y; its anchor's columns where the fact holds. */
			auto group = [&](int pt, int dx, int y, JunctionFactKind kind, Axis axis, uint32_t e, uint32_t node, int row) {
				std::vector<int> allowed;
				std::string tiles;
				int named = 0;
				const std::vector<bool> &here = holds_on_row(kind, axis, y + offset.y);
				for (int c = 0; c <= high; c++) {
					const int x = c + dx + offset.x;
					const bool inside = x >= facts->MinX() && x <= facts->MaxX();
					if (inside && here[x - facts->MinX()]) {
						allowed.push_back(c);
					} else if (inside && named++ < 4) {
						tiles += fmt::format("{}({},{})", tiles.empty() ? "" : " ", x, y + offset.y);
					}
				}
				const uint32_t reason = fact_reason(e, node, row, tiles.empty() ? std::string() : "[blocked tile " + tiles + "]");
				fact_cons.push_back({pt, -1, std::move(allowed), reason});
			};
			/* Per row, the columns of the snapshot where a straight X piece cannot sit, and the columns whose straight X piece does not meet the next one's height. */
			std::map<int, std::pair<std::vector<int>, std::vector<int>>> row_facts;
			auto row_facts_of = [&](int y) -> const std::pair<std::vector<int>, std::vector<int>> & {
				auto found = row_facts.find(y);
				if (found != row_facts.end()) return found->second;
				std::pair<std::vector<int>, std::vector<int>> bad;
				int entry, exit;
				bool before = false;
				for (int x = facts->MinX(); x <= facts->MaxX(); x++) {
					const bool straight = classes_only ? facts->Buildable(x, y) : facts->Straight(x, y, Axis::X, entry, exit);
					if (!straight) bad.first.push_back(x);
					if (x > facts->MinX() && before && straight && !facts->Joins(Axis::X, y, x - 1)) bad.second.push_back(x - 1);
					before = straight;
				}
				return row_facts.emplace(y, std::move(bad)).first->second;
			};
			/* A straight run on row y from point a to point b, in the travel sense sgn (+1 east). */
			auto row_run = [&](int a, int b, int y, int sgn, uint32_t e, int row) {
				const auto &[blocked, steps] = row_facts_of(y + offset.y);
				for (int x : blocked) {
					const uint32_t reason = fact_reason(e, UINT32_MAX, row, fmt::format("[blocked tile ({},{})]", x, y + offset.y));
					fact_cons.push_back({sgn > 0 ? a : b, sgn > 0 ? b : a, {x - offset.x, x - offset.x}, reason});
				}
				/* Two straight tiles side by side that do not meet at one height: the run must not cover both. */
				if (!heights) return;
				for (int x : steps) {
					const uint32_t reason = fact_reason(e, UINT32_MAX, row, fmt::format("[height step ({},{}) to ({},{})]", x, y + offset.y, x + 1, y + offset.y), "height step");
					fact_cons.push_back({sgn > 0 ? a : b, sgn > 0 ? b : a, {x - offset.x, x + 1 - offset.x}, reason});
				}
			};
			/* A vertical run of straight tiles at column offset 0 of point pt, rows from y_a to y_b (either order): the columns where every two neighbours meet at one height. */
			auto column_run_now = [&](int pt, int y_a, int y_b, uint32_t e) {
				const int y_lo = std::min(y_a, y_b);
				const int y_hi = std::max(y_a, y_b);
				if (y_lo == y_hi) return;
				std::vector<int> allowed;
				std::string tiles;
				for (int c = std::max(0, least[pt]); c <= std::min(high, most[pt]); c++) {
					int bad = INT_MIN;
					for (int y = y_lo; y < y_hi && bad == INT_MIN; y++) {
						if (!facts->Joins(Axis::Y, c + offset.x, y + offset.y)) bad = y;
					}
					if (bad == INT_MIN) {
						allowed.push_back(c);
					} else if (c + offset.x >= facts->MinX() && c + offset.x <= facts->MaxX() && tiles.size() < 40) {
						tiles += fmt::format("{}({},{})", tiles.empty() ? "" : " ", c + offset.x, bad + offset.y);
					}
				}
				const uint32_t reason = fact_reason(e, UINT32_MAX, -1, tiles.empty() ? std::string() : "[height step " + tiles + "]", "height step");
				fact_cons.push_back({pt, -1, std::move(allowed), reason});
			};
			auto column_run = [&](int pt, int y_a, int y_b, uint32_t e) { if (heights) pending.push_back([&column_run_now, pt, y_a, y_b, e] { column_run_now(pt, y_a, y_b, e); }); };
			/* A node's tile: the columns where all its tracks fit (and the axis pieces on both sides of a route that goes straight through), and of those where the route meets them at one height. */
			auto node_checks = [&]() {
				for (uint32_t node = 0; node < graph.nodes.size(); node++) {
					const int y = vy[node_pt[node]] + offset.y;
					std::vector<int> fits_ok, joins_ok;
					std::string fit_tiles, join_tiles;
					for (int c = std::max(0, least[node_pt[node]]); c <= std::min(high, most[node_pt[node]]); c++) {
						const int x = c + offset.x;
						bool fits = facts->Fits(x, y, node_bits[node]);
						bool joins = true;
						for (int heading : node_straight[node]) {
							const DiagDirection exit = TrackdirToExitdir(JUNCTION_AXIS_TRACKDIR[heading]);
							const TrackBits lone{TrackdirToTrack(JUNCTION_AXIS_TRACKDIR[heading])};
							/* The neighbours beyond the snapshot's columns carry no facts. */
							const bool before_inside = x - VX[heading] >= facts->MinX() && x - VX[heading] <= facts->MaxX();
							const bool after_inside = x + VX[heading] >= facts->MinX() && x + VX[heading] <= facts->MaxX();
							const int before = before_inside ? facts->EdgeZ(x - VX[heading], y - VY[heading], lone, exit) : 0;
							const int after = after_inside ? facts->EdgeZ(x + VX[heading], y + VY[heading], lone, ReverseDiagDir(exit)) : 0;
							const int in = facts->EdgeZ(x, y, node_bits[node], ReverseDiagDir(exit));
							const int out = facts->EdgeZ(x, y, node_bits[node], exit);
							fits = fits && before != INT_MIN && after != INT_MIN && in != INT_MIN && out != INT_MIN;
							joins = joins && (!before_inside || before == in) && (!after_inside || out == after);
						}
						if (fits) fits_ok.push_back(c);
						if (fits && joins) joins_ok.push_back(c);
						if (x >= facts->MinX() && x <= facts->MaxX() && fit_tiles.size() < 40 && !fits) fit_tiles += fmt::format("{}({},{})", fit_tiles.empty() ? "" : " ", x, y);
						if (x >= facts->MinX() && x <= facts->MaxX() && join_tiles.size() < 40 && fits && !joins) join_tiles += fmt::format("{}({},{})", join_tiles.empty() ? "" : " ", x, y);
					}
					const size_t evaluated = static_cast<size_t>(std::max(0, std::min(high, most[node_pt[node]]) + 1 - std::max(0, least[node_pt[node]])));
					if (fits_ok.size() != evaluated) fact_cons.push_back({node_pt[node], -1, std::move(fits_ok), fact_reason(UINT32_MAX, node, -1, fit_tiles.empty() ? std::string() : "[blocked tile " + fit_tiles + "]")});
					if (joins_ok.size() != evaluated) fact_cons.push_back({node_pt[node], -1, std::move(joins_ok), fact_reason(UINT32_MAX, node, -1, join_tiles.empty() ? std::string() : "[height join " + join_tiles + "]", "height join")});
				}
			};
			/* A bend of 'halves' half pieces whose first sits at row y of point pt: the columns where each piece fits with its own track, each join inside it meets at one height, and its two joins with the axis pieces just outside it do too (DESIGN-G1 §14.3). */
			auto bend_heights_now = [&](int pt, int y, int heading, int hand, int halves, bool corner, uint32_t e, int row, uint32_t node_first, uint32_t node_last) {
				const JunctionRaster raster = RasterizeJunctionRoute({{0, 0}, JUNCTION_AXIS_TRACKDIR[heading], JunctionBendMoves(hand, halves, corner)});
				if (!raster.error.empty() || static_cast<int>(raster.pieces.size()) != halves + 2) return;
				const std::vector<JunctionPiece> &pieces = raster.pieces;
				/* Per piece: its tile relative to the first half, its track (a piece on a node's tile sits with every other track the routes build there), the edge it leaves by. */
				const size_t count = pieces.size();
				std::vector<int> dx(count), dy(count);
				std::vector<TrackBits> bits(count);
				std::vector<DiagDirection> exit(count);
				for (size_t i = 0; i < count; i++) {
					dx[i] = pieces[i].tile.x - pieces[1].tile.x;
					dy[i] = y + pieces[i].tile.y - pieces[1].tile.y + offset.y;
					bits[i] = TrackBits{TrackdirToTrack(pieces[i].trackdir)};
					exit[i] = TrackdirToExitdir(pieces[i].trackdir);
				}
				if (node_first != UINT32_MAX) bits[1] = node_bits[node_first];
				if (node_last != UINT32_MAX) bits[halves] = node_bits[node_last];
				/* Two domains: where the tracks cannot be built (a blocked tile, a slope without a foundation) and, of the rest, where the pieces do not meet at one height. */
				std::vector<int> fits_ok, joins_ok;
				std::string fit_tiles, join_tiles;
				for (int c = std::max(0, least[pt]); c <= std::min(high, most[pt]); c++) {
					size_t bad_fit = count, bad_join = count;
					for (size_t i = 0; bad_fit == count && i < count; i++) {
						const int x = c + dx[i] + offset.x;
						/* Columns beyond the snapshot carry no facts (as in the blocked-tile facts above): a join with such a tile is not checked. */
						if (i + 1 < count && (x < facts->MinX() || x > facts->MaxX() || c + dx[i + 1] + offset.x < facts->MinX() || c + dx[i + 1] + offset.x > facts->MaxX())) continue;
						if (i + 1 == count || (dx[i] == dx[i + 1] && dy[i] == dy[i + 1])) {
							if (i >= 1 && i <= static_cast<size_t>(halves) && !facts->Fits(x, dy[i], bits[i])) bad_fit = i;
							continue;
						}
						/* The join to the next piece; EdgeZ is INT_MIN where the tracks do not fit. */
						const int z_out = facts->EdgeZ(x, dy[i], bits[i], exit[i]);
						const int z_in = facts->EdgeZ(c + dx[i + 1] + offset.x, dy[i + 1], bits[i + 1], ReverseDiagDir(exit[i]));
						if (z_out == INT_MIN || z_in == INT_MIN) {
							bad_fit = i;
						} else if (z_out != z_in && bad_join == count) {
							bad_join = i;
						}
					}
					const auto note = [&](std::string &text, size_t bad) {
						if (c + dx[bad] + offset.x >= facts->MinX() && c + dx[bad] + offset.x <= facts->MaxX() && text.size() < 40) text += fmt::format("{}({},{})", text.empty() ? "" : " ", c + dx[bad] + offset.x, dy[bad]);
					};
					if (bad_fit == count) fits_ok.push_back(c); else note(fit_tiles, bad_fit);
					if (bad_join == count) joins_ok.push_back(c); else if (bad_fit == count) note(join_tiles, bad_join);
				}
				/* A domain that keeps every column is no fact. */
				const size_t evaluated = static_cast<size_t>(std::max(0, std::min(high, most[pt]) + 1 - std::max(0, least[pt])));
				if (fits_ok.size() != evaluated) fact_cons.push_back({pt, -1, std::move(fits_ok), fact_reason(e, UINT32_MAX, row, fit_tiles.empty() ? std::string() : "[blocked tile " + fit_tiles + "]")});
				if (joins_ok.size() != evaluated) fact_cons.push_back({pt, -1, std::move(joins_ok), fact_reason(e, UINT32_MAX, row, "[height join " + join_tiles + "]", "height join")});
			};

			auto bend_heights = [&](int pt, int y, int heading, int hand, int halves, bool corner, uint32_t e, int row, uint32_t node_first = UINT32_MAX, uint32_t node_last = UINT32_MAX) {
				if (heights) pending.push_back([&bend_heights_now, pt, y, heading, hand, halves, corner, e, row, node_first, node_last] { bend_heights_now(pt, y, heading, hand, halves, corner, e, row, node_first, node_last); });
			};

			/* The chains of the edges that are not rows. */
			for (uint32_t e = 0; e < edge_count; e++) {
				if (is_row_edge(e)) continue;
				const std::vector<Elem> &elems = chain[e];
				for (size_t i = 0; i < elems.size(); i++) {
					const Elem &el = elems[i];
					if (el.k == Elem::K::Tile) {
						const uint32_t node = node_of_pt[el.first];
						if (node != UINT32_MAX) {
							group(el.first, 0, vy[el.first], JunctionFactKind::Switch, Axis::X, e, node, -1);
						} else {
							group(el.first, 0, vy[el.first], JunctionFactKind::Straight, VX[el.hin] != 0 ? Axis::X : Axis::Y, e, UINT32_MAX, -1);
						}
					} else if (el.k == Elem::K::Heads) {
						const Axis axis = VX[el.hin] != 0 ? Axis::X : Axis::Y;
						group(el.first, 0, vy[el.first], JunctionFactKind::Straight, axis, e, UINT32_MAX, -1);
						group(el.last, 0, vy[el.last], JunctionFactKind::Straight, axis, e, UINT32_MAX, -1);
					} else {
						bend_heights(el.first, vy[el.first], el.hin, el.hand, static_cast<int>(el.offs.size()), el.k == Elem::K::Corner, e, -1, node_of_pt[el.first], node_of_pt[el.last]);
						for (size_t j = 0; j < el.offs.size(); j++) {
							const TileIndexDiffC &o = el.offs[j];
							const bool at_first = j == 0 && node_of_pt[el.first] != UINT32_MAX;
							const bool at_last = j + 1 == el.offs.size() && node_of_pt[el.last] != UINT32_MAX;
							const uint32_t node = at_first ? node_of_pt[el.first] : (at_last ? node_of_pt[el.last] : UINT32_MAX);
							group(el.first, o.x, vy[el.first] + o.y, node != UINT32_MAX ? JunctionFactKind::Switch : JunctionFactKind::Half, Axis::X, e, node, -1);
						}
					}
					if (i + 1 >= elems.size() || elems[i + 1].first == el.last) continue;
					const Elem &next = elems[i + 1];
					if (VX[el.hout] == 0) {
						const int d = VY[el.hout];
						for (int y = vy[el.last] + d; d > 0 ? y < vy[next.first] : y > vy[next.first]; y += d) group(el.last, 0, y, JunctionFactKind::Straight, Axis::Y, e, UINT32_MAX, -1);
						/* The run's tiles, and a port tile at either end, which is an axis piece like them. */
						int y_a = vy[el.last] + d;
						int y_b = vy[next.first] - d;
						if (d > 0 ? y_a <= y_b : y_a >= y_b) {
							if (el.k == Elem::K::Tile && node_of_pt[el.last] == UINT32_MAX) y_a = vy[el.last];
							if (next.k == Elem::K::Tile && node_of_pt[next.first] == UINT32_MAX) y_b = vy[next.first];
							column_run(el.last, y_a, y_b, e);
						}
					} else {
						row_run(el.last, next.first, vy[el.last], VX[el.hout], e, -1);
					}
				}
			}

			/* The rows' chains: in edge, leg (both copies of a doubled one), out edge. */
			for (int r = 0; r < rows; r++) {
				const int sgn = east[r] ? 1 : -1;
				const int heading = east[r] ? 1 : 3;
				for (const RowPiece &piece : row_pieces(r)) {
					switch (piece.t) {
						case RowPiece::T::Tile:
							if (piece.fact) group(piece.a, 0, piece.y, JunctionFactKind::Straight, Axis::X, piece.edge, UINT32_MAX, r);
							break;
						case RowPiece::T::Run: row_run(piece.a, piece.b, piece.y, sgn, piece.edge, r); break;
						case RowPiece::T::Bend:
							bend_heights(piece.a, piece.y, heading, piece.hand, piece.halves, false, piece.edge, r);
							for (const TileIndexDiffC &o : bend_tiles(heading, piece.hand, piece.halves, false)) group(piece.a, o.x, piece.y + o.y, JunctionFactKind::Half, Axis::X, piece.edge, UINT32_MAX, r);
							break;
					}
				}
			}

			/* The columns each point can take: the least and the greatest solution of the relations (a longest path; x_v - x_u >= k) within the domains the facts above gave. */
			for (const FactCons &fc : fact_cons) {
				if (fc.u >= 0) continue;
				least[fc.v] = std::max(least[fc.v], fc.columns.empty() ? INT_MAX / 2 : fc.columns.front());
				most[fc.v] = std::min(most[fc.v], fc.columns.empty() ? -1 : fc.columns.back());
			}
			for (int round = 0; round <= pt_count; round++) {
				bool changed = false;
				for (const Cons &cn : cons) {
					if (least[cn.u] + cn.k > least[cn.v]) {
						least[cn.v] = least[cn.u] + cn.k;
						changed = true;
					}
					if (most[cn.v] - cn.k < most[cn.u]) {
						most[cn.u] = most[cn.v] - cn.k;
						changed = true;
					}
				}
				if (!changed) break;
			}
			if (heights) node_checks();
			for (const std::function<void()> &job : pending) job();
		}

		why("box bound");
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

	/* The port rows' offset: the one that minimises the largest fan (ties: the smaller). */
	auto best_offset = [&]() {
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
		return best_o;
	};
	auto set_port_rows = [&](int offset) {
		for (int s = 0; s < n; s++) {
			if (notation[s] >= 0) port_row[s] = offset + notation[s];
		}
		for (uint32_t e = 0; e < edge_count; e++) {
			const uint32_t item = fan_item(e);
			if (item != NO_ROW) fan_m[e] = std::abs(port_row[edge_port(e)] - vy[item_pt[item]]);
		}
	};

	/* The Y-phase clearance of the X-axis bridges (D217): from the flat state, the Y and the X solve with no floors and only the relations, a head whose column lies strictly within an X-axis bridge keeps its flat side of the bridge's row by one. */
	auto plan_clearance = [&](int fixed_offset) {
		clear_y.clear();
		ref_shares.clear();
		std::vector<BridgeLine> lines;
		std::vector<HeadRef> heads;
		collect_bridges(lines, heads);
		const std::vector<int> saved_vx = vx, saved_vy = vy, saved_port_row = port_row, saved_fan_m = fan_m;
		const int saved_phase = phase;
		floors_off = relations_only = skip_clearance = true;
		std::vector<int> flat_y, flat_x;
		phase = 1;
		refresh(false);
		bool ok = build() && flat_solve(flat_y);
		if (ok) {
			vy = flat_y;
			set_port_rows(fixed_offset >= 0 ? fixed_offset : best_offset());
			phase = 0;
			refresh(true);
			ok = build() && flat_solve(flat_x);
			if (ok) {
				vx = flat_x;
				compute_node_tracks();
				collecting = true;
				add_occupancy(flat_x);
				collecting = false;
			}
		}
		floors_off = relations_only = skip_clearance = false;
		phase = saved_phase;
		vx = saved_vx;
		vy = saved_vy;
		port_row = saved_port_row;
		fan_m = saved_fan_m;
		if (!ok) return;
		for (const BridgeLine &line : lines) {
			if (!line.x_axis) continue;
			const int left = std::min(flat_x[line.a], flat_x[line.b]);
			const int right = std::max(flat_x[line.a], flat_x[line.b]);
			for (const HeadRef &head : heads) {
				if (head.bridge == line.bridge || flat_x[head.xpt] <= left || flat_x[head.xpt] >= right) continue;
				const int side = flat_y[head.ypt] - flat_y[line.line];
				if (side == 0) continue;
				if (side > 0) clear_y.push_back({head.ypt, line.line, line.edge, head.edge}); else clear_y.push_back({line.line, head.ypt, line.edge, head.edge});
			}
		}
	};
	auto build_failed = [&]() {
		if (occupancy_failed) return solve_failed();
		return fail("the placement rules are not complete for this topology " + build_why);
	};
	JunctionPlacementChoices choices;
	int port_offset = 0;

	if (fixed != nullptr) {
		/* One Y solve and one X solve with the choices fixed (DESIGN-G1 §14.2). */
		for (uint32_t e = 0; e < edge_count; e++) {
			for (Elem &el : chain[e]) {
				if (el.free_m) el.m = std::max(1, fixed->m[e][el.tok]);
			}
		}
		rows_known = true;
		plan_clearance(fixed->port_offset);
		phase = 1;
		refresh(false);
		if (!build()) return build_failed();
		if (!solve(vy)) return solve_failed();
		port_offset = fixed->port_offset;
		set_port_rows(port_offset);
		phase = 0;
		refresh(true);
		if (!build()) return build_failed();
		if (!solve(vx)) return solve_failed();
		choices = *fixed;
	} else {
		/* The rounds: a first X pass without rows gives the S-bends' sizes, then Y and X alternate until the sizes hold. */
		phase = 0;
		refresh(false);
		if (!build()) return build_failed();
		if (!solve(vx)) return solve_failed();
		for (uint32_t e = 0; e < edge_count; e++) {
			for (Elem &el : chain[e]) {
				if (el.free_m) el.m = std::max(1, std::abs(vx[el.last] - vx[el.first]));
			}
		}
		rows_known = true;
		for (int round = 0; round < 5; round++) {
			plan_clearance(-1);
			phase = 1;
			refresh(false);
			if (!build()) return build_failed();
			if (!solve(vy)) return solve_failed();

			/* The port rows. */
			port_offset = best_offset();
			set_port_rows(port_offset);

			phase = 0;
			refresh(true);
			if (!build()) return build_failed();
			if (!solve(vx)) return solve_failed();

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

		/* The choices the rounds settled on. */
		choices.port_offset = port_offset;
		choices.m.resize(edge_count);
		choices.corner.resize(edge_count);
		for (uint32_t e = 0; e < edge_count; e++) {
			choices.m[e].assign(programs.edges[e].size(), 0);
			choices.corner[e].assign(programs.edges[e].size(), 0);
			for (const Elem &el : chain[e]) {
				if (el.free_m) choices.m[e][el.tok] = el.m;
				if (el.joiner_div) choices.corner[e][el.tok] = vy[el.first] - vy[el.last];
			}
		}
	}

	/* The fans' and ports' rows. */
	for (int s = 0; s < n; s++) {
		if (notation[s] >= 0) vy[port_pt[s]] = port_row[s];
	}
	/* A fan's two ends: the port row and its item's row. */
	for (uint32_t e = 0; e < edge_count; e++) {
		for (const Elem &el : chain[e]) {
			if (el.k != Elem::K::Fan) continue;
			vy[el.port_first ? el.first : el.last] = port_row[el.slot];
			vy[el.port_first ? el.last : el.first] = vy[item_pt[el.index]];
		}
	}
	auto tile = [&](int pt) { return TileIndexDiffC{static_cast<int16_t>(vx[pt]), static_cast<int16_t>(vy[pt])}; };

	JunctionTrackPlacement result;
	for (int i = 0; i < items; i++) result.item_rows.push_back(vy[item_pt[i]]);
	for (int s = 0; s < n; s++) result.ports.push_back(tile(port_pt[s]));
	for (uint32_t i = 0; i < graph.nodes.size(); i++) result.nodes.push_back(tile(node_pt[i]));
	result.size = {static_cast<int16_t>(vx[xe_pt] + 1), static_cast<int16_t>(vy[se_pt] + 1)};
	result.choices = choices;
	result.columns.assign(vx.begin(), vx.begin() + base_pts); // The points of the row chains' S-bends are not reported.

	/* The heads of a bridge: its near and far point, and the carrier's travel direction along the axis. */
	struct Heads {
		int near;
		int far;
		bool row; ///< On a row (else in a column).
	};
	auto heads_of = [&](int b, int copy) {
		const uint32_t edge = structure.bridges[b].edge;
		if (structure.bridges[b].doubled) {
			const size_t k = std::find(bridges_of[edge].begin(), bridges_of[edge].end(), b) - bridges_of[edge].begin();
			const Elem &el = copy == 0 ? chain[edge][1 + k] : chain_copy[edge][2 + k];
			return Heads{el.first, el.last, true};
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
		if (is_row_edge(e) && doubled_edge(e)) {
			/* A doubled leg: each copy's S-bend spans its chain's S-bend, a bridge the crosser's tiles between that copy's heads. */
			const int s = east[(end_level(graph.edges[e].from) - 1) / 2] ? 1 : -1;
			for (int copy = 0; copy < 2; copy++) {
				const std::vector<Step> &tokens = copy == 0 ? program : programs.copies[e];
				for (size_t t = 0; t < tokens.size(); t++) {
					const Step &step = tokens[t];
					if (step.kind == StepKind::SBend) {
						const std::vector<Elem> &elems = copy == 0 ? chain[e] : chain_copy[e];
						const Elem &bend = *std::ranges::find_if(elems, [&](const Elem &el) { return el.k == Elem::K::SBend && el.tok == static_cast<int>(t); });
						(copy == 0 ? result.edges[e] : result.copies[e]).push_back({tile(bend.first), tile(bend.last)});
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
			continue;
		}
		const std::vector<Elem> &elems = chain[e];
		for (size_t t = 0; t < program.size(); t++) {
			const Step &step = program[t];
			if (step.kind == StepKind::Fan) {
				for (const Elem &el : elems) {
					if (el.tok == static_cast<int>(t)) result.edges[e].push_back({tile(el.first), tile(el.last)});
				}
			} else if (step.kind == StepKind::Corner || step.kind == StepKind::SBend) {
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
					found = tiles_under(e, heads, vy[heads.near]);
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

	/* The curves at the solved columns (for tests): paths first, then the copies in path order. */
	std::vector<SymRoute> sym;
	if (!sym_routes(sym, true)) return fail("the curves of the routes are not ruled yet " + build_why);
	for (int copy = 0; copy < 2; copy++) {
		for (const SymRoute &route : sym) {
			if (route.copy != copy) continue;
			std::vector<std::pair<int8_t, int>> curves;
			for (const SymCurve &curve : route.curves) curves.emplace_back(curve.hand == JunctionCurveHand::Right ? 1 : -1, curve.xoff + 2 * route.sign * (vx[curve.pt] - vx[route.start]));
			result.curves.push_back(std::move(curves));
		}
	}
	return result;
}

JunctionTrackPlacement PlaceJunctionTrackGraph(const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, const JunctionTrackOrder &order, const JunctionTrackStructure &structure, const JunctionTrackPrograms &programs, const JunctionSpec &spec, std::string *error)
{
	return PlaceTrackGraph(topology, paths, graph, order, structure, programs, spec, nullptr, error, {}, {});
}

JunctionTrackPlacement PlaceJunctionTrackGraph(const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, const JunctionTrackOrder &order, const JunctionTrackStructure &structure, const JunctionTrackPrograms &programs, const JunctionSpec &spec, const JunctionPlacementChoices &choices, std::string *error, const std::vector<std::pair<int, int>> &x_floors, const std::vector<std::pair<int, int>> &y_floors)
{
	return PlaceTrackGraph(topology, paths, graph, order, structure, programs, spec, &choices, error, x_floors, y_floors);
}

JunctionTrackPlacement PlaceJunctionTrackGraph(const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, const JunctionTrackOrder &order, const JunctionTrackStructure &structure, const JunctionTrackPrograms &programs, const JunctionSpec &spec, const JunctionPlacementChoices &choices, const JunctionFacts &facts, TileIndexDiffC offset, std::string *error, bool heights, bool terraform, const JunctionTerrain *terrain)
{
	if (terraform && terrain == nullptr) {
		if (error != nullptr) *error = "terraform needs the terrain";
		return {};
	}
	JunctionTrackPlacement placement = PlaceTrackGraph(topology, paths, graph, order, structure, programs, spec, &choices, error, {}, {}, &facts, offset, heights && !terraform, terraform);
	if (!terraform || placement.nodes.empty()) return placement;

	/* The height solve (DESIGN-G1 §15.3) on the render of the placement. */
	std::vector<JunctionTrackRoute> routes;
	std::string text;
	const JunctionLayout layout = RenderJunctionTrackPlacement(topology, paths, graph, structure, programs, placement, &text, &routes);
	auto fail = [&](const std::string &reason) {
		if (error != nullptr) *error = reason;
		return JunctionTrackPlacement{};
	};
	if (layout.tracks.empty()) return fail("the placement does not render: " + text);
	if (!SolveJunctionLevels(*terrain, facts, offset, JunctionRampLevels(spec), spec.tl, placement, layout, routes, placement.terraform, placement.height_cost, &text)) return fail(text);
	return placement;
}

JunctionLayout RenderJunctionTrackPlacement(const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, const JunctionTrackStructure &structure, const JunctionTrackPrograms &programs, const JunctionTrackPlacement &placement, std::string *error, std::vector<JunctionTrackRoute> *routes)
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
	if (routes != nullptr) routes->clear();
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
			/* One tag per piece the moves have made so far (piece 0 is the start piece). */
			std::vector<JunctionTrackPieceTag> tags{{edges.front(), JunctionTrackPieceTag::RUN, 0, static_cast<uint8_t>(c)}};
			auto tag_pieces = [&](size_t count, uint32_t edge, uint16_t token, size_t run) {
				tags.insert(tags.end(), count, JunctionTrackPieceTag{edge, token, static_cast<uint16_t>(run), static_cast<uint8_t>(c)});
			};

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
							tags.pop_back();
						} else {
							const int before = run({static_cast<int16_t>(heads.start.x - VX[h]), static_cast<int16_t>(heads.start.y - VY[h])});
							if (before < 0) return fail_at(where, "negative or off-line run");
							route.moves.append(before, 'F');
							tag_pieces(before, e, JunctionTrackPieceTag::RUN, t);
						}
						const int middle = std::abs(heads.end.x - heads.start.x) + std::abs(heads.end.y - heads.start.y) - 1;
						route.moves += fmt::format("B{}", middle);
						tag_pieces(middle + 2, e, static_cast<uint16_t>(t), t);
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
					tag_pieces(before, e, JunctionTrackPieceTag::RUN, t);
					route.moves += JunctionBendMoves(match_hand, match_halves, token.kind == StepKind::Corner);
					tag_pieces(match_halves, e, static_cast<uint16_t>(t), t);
					tag_pieces(1, e, JunctionTrackPieceTag::RUN, t + 1);
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
			const uint32_t last_edge = edges.back();
			tag_pieces(tail, last_edge, JunctionTrackPieceTag::RUN, (c == 1 && doubled_leg(last_edge) ? programs.copies : programs.edges)[last_edge].size());

			const JunctionRaster raster = RasterizeJunctionRoute(route);
			if (!raster.error.empty()) return fail_at("", raster.error);
			if (tags.size() != raster.pieces.size()) return fail_at("", "the piece tags do not match the pieces");
			if (raster.pieces.back().tile.x != out.x || raster.pieces.back().tile.y != out.y) return fail_at("", "misses its out port");

			/* Each piece lies on the edge it is on: consecutive edges meet at their shared node's piece, which a merge leaves to the first edge and a split gives to the next. */
			size_t block_start = 0;
			uint32_t block_edge = edges.front();
			size_t edge_index = 0;
			bool token_moved = false;
			auto set_block = [&](size_t end) {
				for (size_t i = block_start; i < end; i++) {
					if (tags[i].token != JunctionTrackPieceTag::RUN && tags[i].edge != block_edge) token_moved = true;
					tags[i].edge = block_edge;
				}
			};
			for (size_t k = 0; k + 1 < edges.size(); k++) {
				const JunctionTrackEnd &shared = graph.edges[edges[k]].to;
				if (!shared.node || !(shared == graph.edges[edges[k + 1]].from)) return fail_at("", "consecutive edges do not share a node");
				const TileIndexDiffC node_tile = placement.nodes[shared.index];
				size_t at = 0;
				while (at < raster.pieces.size() && (raster.pieces[at].on_bridge || raster.pieces[at].tile.x != node_tile.x || raster.pieces[at].tile.y != node_tile.y)) at++;
				if (at == raster.pieces.size()) return fail_at("", "a node has no piece on the route");
				const size_t next_start = graph.nodes[shared.index].split ? at : at + 1;
				if (next_start < block_start) return fail_at("", "the edge blocks are out of order");
				set_block(next_start);
				block_start = next_start;
				block_edge = edges[++edge_index];
			}
			set_block(tags.size());
			if (token_moved) return fail_at("", "a token piece lies outside its edge's block");
			/* Copy 1 only on a doubled edge; run counts the edge's tokens before the piece, from its own token tags. */
			for (size_t i = 0; i < tags.size(); i++) {
				tags[i].copy = c == 1 && doubled_leg(tags[i].edge) ? 1 : 0;
				if (tags[i].token != JunctionTrackPieceTag::RUN) {
					tags[i].run = tags[i].token;
					continue;
				}
				tags[i].run = 0;
				for (size_t j = i; j-- > 0;) {
					if (tags[j].edge != tags[i].edge) break;
					if (tags[j].token != JunctionTrackPieceTag::RUN) {
						tags[i].run = tags[j].token + 1;
						break;
					}
				}
			}
			AddRasterToLayout(layout, raster);
			if (routes != nullptr) routes->push_back({static_cast<uint16_t>(p), static_cast<uint8_t>(c), raster.pieces, std::move(tags)});
		}
	}
	return layout;
}

/** Whether a track touches the given tile edge. */
static bool JunctionTrackTouches(Track track, DiagDirection edge)
{
	const Trackdir td = TrackToTrackdir(track);
	return TrackdirToExitdir(td) == edge || TrackdirToExitdir(ReverseTrackdir(td)) == edge;
}

/** The track that touches both edges. */
static Track JunctionCornerTrack(DiagDirection a, DiagDirection b)
{
	for (Track track : {Track::Upper, Track::Lower, Track::Left, Track::Right}) {
		if (JunctionTrackTouches(track, a) && JunctionTrackTouches(track, b)) return track;
	}
	NOT_REACHED();
}

/** Add tracks to a tile of the layout. */
static void AddJunctionTracks(JunctionLayout &layout, TileIndexDiffC tile, TrackBits tracks)
{
	for (JunctionTrack &jt : layout.tracks) {
		if (jt.offset.x == tile.x && jt.offset.y == tile.y) {
			jt.tracks = jt.tracks | tracks;
			return;
		}
	}
	layout.tracks.push_back({tile, tracks});
}

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

std::vector<JunctionTrackSignal> SignalJunctionTrackRoutes(JunctionLayout &layout, const std::vector<JunctionTrackRoute> &routes, const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, const JunctionTrackPrograms &programs, const JunctionSpec &spec, std::string *why)
{
	using Reason = JunctionSignalReason;
	std::vector<JunctionTrackSignal> tokens;

	/* The pass's own state, read in place of a layout: the tracks of the routes, those drawn on top of them (the prio elements), the bridge heads and the signals placed. */
	std::map<std::pair<int, int>, TrackBits> used;
	std::set<std::pair<int, int>> heads;
	for (const JunctionTrackRoute &route : routes) {
		for (size_t i = 0; i < route.pieces.size(); i++) {
			const JunctionPiece &piece = route.pieces[i];
			if (!piece.on_bridge) {
				used[{piece.tile.x, piece.tile.y}].Set(TrackdirToTrack(piece.trackdir));
				continue;
			}
			if (i == 0 || !route.pieces[i - 1].on_bridge || i + 1 == route.pieces.size() || !route.pieces[i + 1].on_bridge) heads.insert({piece.tile.x, piece.tile.y});
		}
	}
	std::map<std::pair<int, int>, TrackBits> tracks = used;
	std::vector<std::pair<TileIndexDiffC, TrackBits>> drawn; // Written into the layout at the end.
	std::vector<JunctionSignal> signals; // Likewise.
	std::set<std::tuple<int, int, Trackdir>> signalled_at;
	const auto tracks_at = [](const std::map<std::pair<int, int>, TrackBits> &m, TileIndexDiffC tile) {
		const auto it = m.find({tile.x, tile.y});
		return it == m.end() ? TrackBits{} : it->second;
	};
	const auto draw = [&](TileIndexDiffC tile, TrackBits bits) {
		tracks[{tile.x, tile.y}] |= bits;
		drawn.emplace_back(tile, bits);
	};
	const auto add_signal = [&](TileIndexDiffC tile, Trackdir facing, SignalType type) {
		signals.push_back({tile, facing, type, SignalVariant::Electric});
		signalled_at.emplace(tile.x, tile.y, facing);
	};
	const auto is_signalled = [&](const JunctionPiece &piece) { return !piece.on_bridge && signalled_at.contains({piece.tile.x, piece.tile.y, piece.trackdir}); };
	const auto is_legal = [&](const JunctionPiece &piece) {
		if (piece.on_bridge || heads.contains({piece.tile.x, piece.tile.y})) return false;
		if (TracksOverlap(tracks_at(tracks, piece.tile))) return false;
		return !signalled_at.contains({piece.tile.x, piece.tile.y, piece.trackdir}) && !signalled_at.contains({piece.tile.x, piece.tile.y, ReverseTrackdir(piece.trackdir)});
	};

	/** One event of a route: a switch (one index) or a bridge (first to last index). */
	struct Event {
		size_t first;
		size_t last;
		bool bridge;
		bool split;
	};

	/* Phase 0: the prio elements (DESIGN-G1 §12.4). Each draws its own joins, row, row signals and the ML signals inside its coverage; the joiner's Entry follows in phase 4. */
	const int n_prio = static_cast<int>(spec.prio_tiles + (spec.prio_tiles & 1));
	std::vector<std::pair<size_t, size_t>> entries; // The joiner route and the index of its merge piece.
	for (size_t r = 0; r < routes.size(); r++) {
		const auto &movement = topology.movements[paths[routes[r].path].movement];
		if (!topology.groups[movement.first].main || !topology.groups[movement.second].main) continue;
		const std::vector<JunctionPiece> &P = routes[r].pieces;
		const std::vector<SwitchEvent> switches = JunctionRouteEvents(graph, programs, routes, r);
		for (const SwitchEvent &merge : switches) {
			if (merge.first) continue;
			const size_t j = merge.second;
			const JunctionPiece &mp = P[j];
			const TileIndexDiffC M = mp.tile;

			/* 1. The joiner: the merge node n of the event (the last piece of an edge that ends at a node), and the first route of another movement whose lane path has an edge ending at n; k is that route's node piece. */
			if (j + 1 >= P.size() || routes[r].tags[j].edge == routes[r].tags[j + 1].edge || !graph.edges[routes[r].tags[j].edge].to.node) continue;
			const uint16_t node = graph.edges[routes[r].tags[j].edge].to.index;
			std::optional<size_t> joiner, joiner_k;
			for (size_t r2 = 0; r2 < routes.size() && !joiner.has_value(); r2++) {
				if (paths[routes[r2].path].movement == paths[routes[r].path].movement) continue;
				for (const uint32_t e : graph.path_edges[routes[r2].path]) {
					if (!graph.edges[e].to.node || graph.edges[e].to.index != node) continue;
					for (size_t k = routes[r2].tags.size(); k-- > 0;) {
						if (routes[r2].tags[k].edge != e) continue;
						joiner = r2;
						joiner_k = k;
						break;
					}
					break;
				}
			}
			if (!joiner.has_value()) continue;
			if (tracks_at(tracks, M) != tracks_at(used, M)) continue;

			/* 2. The geometry, as GeometryOfPrio. */
			if (!IsDiagonalTrackdir(mp.trackdir)) continue;
			const Track axis = TrackdirToTrack(mp.trackdir);
			const bool along_x = axis == Track::X;
			const DiagDirection exit_edge = TrackdirToExitdir(mp.trackdir);
			const TileIndexDiffC from = routes[*joiner].pieces[*joiner_k - 1].tile;
			const int offset = along_x ? from.y - M.y : from.x - M.x;
			if (offset == 0) continue;
			std::optional<DiagDirection> side;
			for (DiagDirection edge = DiagDirection::Begin; edge < DiagDirection::End; edge++) {
				if (JunctionTrackTouches(axis, edge)) continue;
				const TileIndexDiffC diff = TileIndexDiffCByDiagDir(edge);
				if ((along_x ? diff.y : diff.x) == (offset < 0 ? 1 : -1)) side = edge;
			}
			if (!side.has_value()) continue;
			const TileIndexDiffC dir = TileIndexDiffCByDiagDir(exit_edge);
			const TileIndexDiffC side_diff = TileIndexDiffCByDiagDir(*side);
			const Track h1 = JunctionCornerTrack(*side, exit_edge);
			const Track h2 = JunctionCornerTrack(ReverseDiagDir(*side), ReverseDiagDir(exit_edge));

			/* 3. The requirements, all before anything is written. */
			const auto ml = [&](int s) { return TileIndexDiffC{static_cast<int16_t>(M.x - s * dir.x), static_cast<int16_t>(M.y - s * dir.y)}; };
			const auto row = [&](int s) { const TileIndexDiffC t = ml(s); return TileIndexDiffC{static_cast<int16_t>(t.x + side_diff.x), static_cast<int16_t>(t.y + side_diff.y)}; };
			bool fits = static_cast<int>(j) >= n_prio;
			for (int s = 1; fits && s <= n_prio; s++) {
				const JunctionPiece &piece = P[j - s];
				const TileIndexDiffC t = ml(s);
				fits = !piece.on_bridge && piece.tile.x == t.x && piece.tile.y == t.y && piece.trackdir == mp.trackdir && std::ranges::none_of(switches, [&](const SwitchEvent &sw) { return sw.second == j - s; });
			}
			for (int s = 0; fits && s <= n_prio; s++) fits = tracks_at(tracks, row(s)) == TrackBits{};
			if (fits) {
				const TrackBits beyond = tracks_at(tracks, row(n_prio + 1));
				for (Track t : beyond) {
					if (JunctionTrackTouches(t, exit_edge)) fits = false;
				}
			}
			if (!fits) {
				if (why != nullptr && why->empty()) *why = fmt::format("the prio element at {},{} does not fit", M.x, M.y);
				continue;
			}

			/* 4. Write. */
			for (int s = 0; s <= n_prio; s++) {
				if (s % 2 == 0) draw(ml(s), TrackBits{h1});
				if (s == 0) {
					draw(row(s), TrackBits{h2});
				} else if (s % 2 == 0) {
					draw(row(s), TrackBits{axis, h2});
				} else {
					draw(row(s), TrackBits{axis});
				}
			}
			for (int s = 1; s < n_prio; s += 2) {
				add_signal(ml(s), mp.trackdir, spec.signal_type);
				tokens.push_back({static_cast<uint16_t>(r), Reason::Prio, ml(s), mp.trackdir});
				const Trackdir back = ReverseTrackdir(mp.trackdir);
				add_signal(row(s), back, s == n_prio - 1 ? SignalType::Exit : SignalType::Combo);
				tokens.push_back({static_cast<uint16_t>(r), Reason::Prio, row(s), back});
			}
			entries.emplace_back(*joiner, *joiner_k);
		}
	}

	for (size_t r = 0; r < routes.size(); r++) {
		const std::vector<JunctionPiece> &P = routes[r].pieces;
		const int64_t n = static_cast<int64_t>(P.size());
		const std::vector<SwitchEvent> switches = JunctionRouteEvents(graph, programs, routes, r);
		std::vector<Event> events;
		for (const SwitchEvent &sw : switches) events.push_back({sw.second, sw.second, false, sw.first});
		for (size_t i = 0; i < P.size(); i++) {
			if (!P[i].on_bridge) continue;
			size_t last = i;
			while (last + 1 < P.size() && P[last + 1].on_bridge) last++;
			events.push_back({i, last, true, false});
			i = last;
		}
		std::ranges::sort(events, {}, &Event::first);

		auto ok = [&](int64_t i) {
			if (i <= 0 || i >= n - 1) return false;
			const JunctionPiece &piece = P[i];
			if (piece.on_bridge || heads.contains({piece.tile.x, piece.tile.y})) return false;
			if (std::ranges::any_of(switches, [&](const SwitchEvent &sw) { return static_cast<int64_t>(sw.second) == i; })) return false;
			return is_legal(piece) || is_signalled(piece);
		};
		auto put = [&](int64_t i, Reason reason, size_t route) {
			const JunctionPiece &piece = P[i];
			if (!is_signalled(piece)) add_signal(piece.tile, piece.trackdir, spec.signal_type);
			tokens.push_back({static_cast<uint16_t>(route), reason, piece.tile, piece.trackdir});
		};
		/* The largest ok index in (lo, hi), or -1. */
		auto last_ok = [&](int64_t lo, int64_t hi) {
			for (int64_t i = hi - 1; i > lo; i--) if (ok(i)) return i;
			return int64_t{-1};
		};
		/* The smallest ok index in (lo, hi), or -1. */
		auto first_ok = [&](int64_t lo, int64_t hi) {
			for (int64_t i = lo + 1; i < hi; i++) if (ok(i)) return i;
			return int64_t{-1};
		};

		for (size_t e = 0; e < events.size(); e++) {
			const Event &ev = events[e];
			const bool has_prev = e > 0;
			const bool has_next = e + 1 < events.size();
			const int64_t prev = has_prev ? static_cast<int64_t>(events[e - 1].last) : -1;
			const int64_t next = has_next ? static_cast<int64_t>(events[e + 1].first) : n;
			const int64_t before = last_ok(prev, static_cast<int64_t>(ev.first));
			const int64_t after = first_ok(static_cast<int64_t>(ev.last), next);
			if (ev.bridge) {
				if (before >= 0) put(before, has_prev && events[e - 1].bridge ? Reason::SeriesGap : Reason::BeforeBridge, r);
				if (after >= 0) put(after, has_next && events[e + 1].bridge ? Reason::SeriesGap : Reason::AfterBridge, r);
			} else if (ev.split) {
				if (before >= 0) put(before, Reason::BeforeSplit, r);
				if (after >= 0) put(after, Reason::AfterSplit, r);
			} else {
				if (before >= 0) put(before, Reason::BeforeMerge, r);
				if (after >= 0) put(after, Reason::AfterMerge, r);
			}
		}
		if (ok(n - 2)) put(n - 2, Reason::Stub, r);
	}

	/* Phase 2: the whole-train bays before the merges of non-through paths, walked as PlaceBaySignals walks them. */
	const uint bay = 2u * spec.tl;
	for (size_t r = 0; r < routes.size(); r++) {
		const auto &movement = topology.movements[paths[routes[r].path].movement];
		if (topology.groups[movement.first].main && topology.groups[movement.second].main) continue;
		const std::vector<JunctionPiece> &P = routes[r].pieces;
		const std::vector<SwitchEvent> events = JunctionRouteEvents(graph, programs, routes, r);
		for (const SwitchEvent &event : events) {
			if (event.first) continue;
			const size_t m = event.second;

			size_t s = m;
			for (size_t i = m; i-- > 0;) {
				if (!is_signalled(P[i])) continue;
				s = i;
				break;
			}
			if (s == m) continue;

			size_t u = 0;
			for (const SwitchEvent &other : events) {
				if (other.first && other.second < s) u = std::max(u, other.second);
			}
			uint length = 0;
			for (size_t i = u + 1; i <= s; i++) length += P[i].units;
			if (length < bay) continue;

			size_t prev = s;
			for (;;) {
				std::optional<size_t> cand;
				uint distance = 0;
				for (size_t i = prev; i-- > 0;) {
					distance += P[i + 1].units;
					if (distance < bay) continue;
					const JunctionPiece &piece = P[i];
					if (!is_legal(piece) && !is_signalled(piece)) continue;
					if (piece.on_bridge || heads.contains({piece.tile.x, piece.tile.y})) continue;
					if (std::ranges::any_of(events, [&](const SwitchEvent &e) { return e.second == i; })) continue;
					cand = i;
					break;
				}
				if (!cand.has_value() || *cand <= u) break;
				if (!is_signalled(P[*cand])) add_signal(P[*cand].tile, P[*cand].trackdir, spec.signal_type);
				tokens.push_back({static_cast<uint16_t>(r), Reason::Bay, P[*cand].tile, P[*cand].trackdir});
				prev = *cand;
			}
		}
	}

	/* Phase 3: drag fill. */
	for (size_t r = 0; r < routes.size(); r++) {
		PlaceDragSignals(routes[r].pieces, spec.sd, is_signalled, is_legal, [&](const JunctionPiece &piece) {
			add_signal(piece.tile, piece.trackdir, spec.signal_type);
			tokens.push_back({static_cast<uint16_t>(r), Reason::Fill, piece.tile, piece.trackdir});
		});
	}
	/* Phase 4: the joiner's last signal before each element's merge is an Entry. */
	for (const auto &[r2, k] : entries) {
		const std::vector<JunctionPiece> &P = routes[r2].pieces;
		const std::vector<SwitchEvent> switches = JunctionRouteEvents(graph, programs, routes, r2);
		for (size_t i = k; i-- > 0;) {
			if (std::ranges::any_of(switches, [&](const SwitchEvent &sw) { return sw.second > i && sw.second < k; })) break;
			if (!is_signalled(P[i])) continue;
			for (JunctionSignal &signal : signals) {
				if (signal.offset.x == P[i].tile.x && signal.offset.y == P[i].tile.y && TrackdirToTrack(signal.facing) == TrackdirToTrack(P[i].trackdir)) signal.type = SignalType::Entry;
			}
			break;
		}
	}
	for (const auto &[tile, bits] : drawn) AddJunctionTracks(layout, tile, bits);
	layout.signals.insert(layout.signals.end(), signals.begin(), signals.end());
	return tokens;
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

JunctionGraphSeed MakeJunctionGraphSeed(const JunctionTopology &topology, const JunctionSpec &spec, std::string *error)
{
	JunctionGraphSeed seed{};
	seed.topology = &topology;
	std::string reason;
	auto fail = [&]() {
		if (error != nullptr) *error = reason;
		seed.routes.clear();
		return seed;
	};
	seed.paths = ExpandJunctionLanes(topology, &reason);
	if (seed.paths.empty()) return fail();
	seed.graph = BuildJunctionTrackGraph(topology, seed.paths, &reason);
	if (seed.graph.edges.empty()) return fail();
	const JunctionTrackOrder order = OrderJunctionTrackGraph(topology, seed.paths, seed.graph, &reason);
	if (order.rows.empty()) return fail();
	const JunctionTrackStructure structure = StructureJunctionTrackGraph(topology, seed.paths, seed.graph, order, &reason);
	if (structure.stack.empty()) return fail();
	seed.programs = ProgramJunctionTrackGraph(topology, seed.paths, seed.graph, order, structure, &reason);
	if (seed.programs.edges.empty()) return fail();
	seed.structure = CutJunctionTrackBridges(topology, seed.paths, seed.graph, order, structure, seed.programs, spec, &seed.placement, &reason);
	if (seed.placement.nodes.empty()) return fail();
	RenderJunctionTrackPlacement(topology, seed.paths, seed.graph, seed.structure, seed.programs, seed.placement, &reason, &seed.routes);
	if (seed.routes.empty()) return fail();
	return seed;
}

/** Whether a piece's tag is a run piece of (edge, run, copy). */
static bool IsRunPiece(const JunctionTrackPieceTag &tag, const JunctionDetourEdit &edit)
{
	return tag.token == JunctionTrackPieceTag::RUN && tag.edge == edit.edge && tag.run == edit.run && tag.copy == edit.copy;
}

std::vector<JunctionTrackRoute> ApplyJunctionDetours(const std::vector<JunctionTrackRoute> &routes, const std::vector<JunctionDetourEdit> &edits, const JunctionSpec &spec, TileIndexDiffC offset, std::string *why)
{
	std::vector<JunctionTrackRoute> result = routes;
	auto fail = [&](const std::string &reason) {
		if (why != nullptr) *why = reason;
		return std::vector<JunctionTrackRoute>{};
	};
	if (routes.empty()) return fail("no routes");

	for (const JunctionDetourEdit &edit : edits) {
		const std::string name = fmt::format("edit (edge {} run {} copy {} column {} side {} variant {}): ", edit.edge, edit.run, edit.copy, edit.column, edit.side, edit.variant);
		if (edit.side != 1 && edit.side != -1) return fail(name + "not a side");

		/* The carrying routes and each one's run: the first to the last piece tagged with it. */
		struct Carrier {
			size_t route;
			size_t first;
			size_t last;
		};
		std::vector<Carrier> carriers;
		for (size_t r = 0; r < result.size(); r++) {
			const std::vector<JunctionTrackPieceTag> &tags = result[r].tags;
			size_t first = tags.size();
			size_t last = 0;
			for (size_t i = 0; i < tags.size(); i++) {
				if (!IsRunPiece(tags[i], edit)) continue;
				if (first == tags.size()) first = i;
				last = i;
			}
			if (first == tags.size()) continue;
			for (size_t i = first; i <= last; i++) {
				if (!IsRunPiece(tags[i], edit)) return fail(name + "the run is not contiguous");
			}
			carriers.push_back({r, first, last});
		}
		if (carriers.empty()) return fail(name + "no route carries the run");

		/* A straight X run on one row, the same on every carrier, with the piece strictly inside. */
		const std::vector<JunctionPiece> &reference = result[carriers[0].route].pieces;
		const size_t length = carriers[0].last - carriers[0].first + 1;
		for (const Carrier &c : carriers) {
			if (c.last - c.first + 1 != length) return fail(name + "the routes disagree on the run");
			for (size_t i = 0; i < length; i++) {
				const JunctionPiece &piece = result[c.route].pieces[c.first + i];
				const JunctionPiece &ref = reference[carriers[0].first + i];
				if (piece.on_bridge || (piece.trackdir != Trackdir::X_NE && piece.trackdir != Trackdir::X_SW)) return fail(name + "not an X run");
				if (piece.tile.x != ref.tile.x || piece.tile.y != ref.tile.y || piece.trackdir != ref.trackdir || piece.tile.y != reference[carriers[0].first].tile.y) return fail(name + "the routes disagree on the run");
			}
		}
		if (length < 3) return fail(name + "the run has no interior piece");
		const JunctionPiece p = reference[carriers[0].first];
		const JunctionPiece q = reference[carriers[0].last];

		/* The failing column clamped strictly inside the seed run, which is only the reference shape. */
		const int direction = q.tile.x > p.tile.x ? 1 : -1;
		const int along = std::clamp(direction * (edit.column - offset.x - p.tile.x), 1, static_cast<int>(length) - 2);
		const int middle = (static_cast<int>(length) - 1) / 2;

		/* The tiles taken by every other piece: the carriers' run interiors are being replaced. */
		std::set<std::pair<int, int>> taken;
		std::set<std::pair<int, int>> own; ///< The run's own interior tiles: a new piece on one is not a new tile (a bridge may pass over it).
		for (size_t r = 0; r < result.size(); r++) {
			const auto carrier = std::ranges::find_if(carriers, [&](const Carrier &c) { return c.route == r; });
			for (size_t i = 0; i < result[r].pieces.size(); i++) {
				if (carrier != carriers.end() && i > carrier->first && i < carrier->last) {
					own.emplace(result[r].pieces[i].tile.x, result[r].pieces[i].tile.y);
					continue;
				}
				taken.emplace(result[r].pieces[i].tile.x, result[r].pieces[i].tile.y);
			}
		}

		/* A shifted run of one piece has no free link, and the new tiles may meet no other route or bridge head. */
		int skipped = 0;
		auto build = [&](const JunctionRaster &raster, int n2, std::string &reason) {
			if (n2 == 0) {
				reason = "a shifted run of one piece";
				return false;
			}
			for (size_t i = 1; i + 1 < raster.pieces.size(); i++) {
				const TileIndexDiffC tile = raster.pieces[i].tile;
				if (taken.contains({tile.x, tile.y}) && !own.contains({tile.x, tile.y})) {
					reason = fmt::format("collision at {},{}", tile.x, tile.y);
					return false;
				}
			}

			/* The curve gaps of every carrier's edited route hold on its units (rule 2 of the template). */
			for (const Carrier &c : carriers) {
				const std::vector<JunctionPiece> &old = result[c.route].pieces;
				std::vector<JunctionPiece> edited(old.begin(), old.begin() + c.first);
				edited.insert(edited.end(), raster.pieces.begin(), raster.pieces.end());
				edited.insert(edited.end(), old.begin() + c.last + 1, old.end());
				std::vector<JunctionCurveHand> hands;
				std::vector<int> starts;
				int unit = 0;
				for (size_t i = 0; i < edited.size(); i++) {
					if (i > 0) {
						const DirDiff diff = DirDifference(TrackdirToDirection(edited[i - 1].trackdir), TrackdirToDirection(edited[i].trackdir));
						if (diff == DirDiff::Left45 || diff == DirDiff::Right45) {
							hands.push_back(diff == DirDiff::Left45 ? JunctionCurveHand::Left : JunctionCurveHand::Right);
							starts.push_back(unit);
						}
					}
					unit += edited[i].units;
				}
				for (const JunctionCurveGap &gap : JunctionCurveGaps(hands, spec.tl, spec.curve_speed, spec.reference_speed)) {
					if (starts[gap.j] - starts[gap.i] >= gap.min_gap) continue;
					reason = fmt::format("curve gap {} below {}", starts[gap.j] - starts[gap.i], gap.min_gap);
					return false;
				}
			}
			if (skipped < edit.variant) {
				skipped++;
				reason = "skipped candidate";
				return false;
			}
			return true;
		};
		JunctionRaster chosen;
		std::string reason;
		/* From the clamped column toward the run's middle and on to its far interior end: the first column with a candidate. */
		std::string first_reason;
		bool found = false;
		for (int a = along, step = along > middle ? -1 : 1; a >= 1 && a <= static_cast<int>(length) - 2 && !found; a += step) {
			found = ChooseJunctionDetour(p, q, p.tile.x + direction * a, edit.side, build, &chosen, &reason);
			if (!found && reason != "link too short for a detour") first_reason = reason;
		}
		if (!found) return fail(name + (first_reason.empty() ? reason : first_reason));

		/* The run's interior replaced on every carrier, the new pieces tagged as the run. */
		const JunctionTrackPieceTag tag{edit.edge, JunctionTrackPieceTag::RUN, edit.run, edit.copy};
		for (const Carrier &c : carriers) {
			JunctionTrackRoute &route = result[c.route];
			route.pieces.erase(route.pieces.begin() + c.first + 1, route.pieces.begin() + c.last);
			route.tags.erase(route.tags.begin() + c.first + 1, route.tags.begin() + c.last);
			route.pieces.insert(route.pieces.begin() + c.first + 1, chosen.pieces.begin() + 1, chosen.pieces.end() - 1);
			route.tags.insert(route.tags.begin() + c.first + 1, chosen.pieces.size() - 2, tag);
		}
	}
	return result;
}
