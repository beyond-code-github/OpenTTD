/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_hub.cpp Compose a hub from the core's placement and render and its oriented blocks (DESIGN-blocks §7.3). */

#include "../../stdafx.h"

#include "junction_hub.h"

#include "junction_block.h"
#include "../junction_spec.h"

#include <algorithm>
#include <array>
#include <functional>
#include <map>
#include <set>

#include "../../safeguards.h"

namespace {

/** One block instance of the hub: the template, its orientation and its port tables. */
struct HubBlock {
	const JunctionBlock *block = nullptr;
	JunctionOrientedBlock oriented;
	bool mirrored = false;
	std::vector<uint16_t> in_slot, out_slot; ///< Per port, its slot in the block topology (in port p is the p-th W slot, out port p the p-th E slot from the last).
	std::array<std::array<uint32_t, 2>, 2> bundle_edge{{{UINT32_MAX, UINT32_MAX}, {UINT32_MAX, UINT32_MAX}}}; ///< [in bundle ? 1 : 0][bundle]: the core edge attached there.

	/** The port number of a block topology slot, or -1. */
	static int PortOf(const std::vector<uint16_t> &slots, uint16_t slot)
	{
		const auto it = std::ranges::find(slots, slot);
		return it == slots.end() ? -1 : static_cast<int>(it - slots.begin());
	}

	/** The (bundle, track left to right of travel in the hub) of a port, or (-1, -1). */
	std::pair<int, int> BundleOfPort(const std::vector<std::vector<uint8_t>> &bundles, int port) const
	{
		for (size_t b = 0; b < bundles.size(); b++) {
			const auto it = std::ranges::find(bundles[b], port);
			if (it == bundles[b].end()) continue;
			const int pos = static_cast<int>(it - bundles[b].begin());
			return {static_cast<int>(b), mirrored ? static_cast<int>(bundles[b].size()) - 1 - pos : pos};
		}
		return {-1, -1};
	}

	/** The port of (bundle, track), or -1. */
	int PortOfTrack(const std::vector<std::vector<uint8_t>> &bundles, size_t bundle, size_t track) const
	{
		if (bundle >= bundles.size() || track >= bundles[bundle].size()) return -1;
		return bundles[bundle][mirrored ? bundles[bundle].size() - 1 - track : track];
	}
};

/** One piece of a hub lane path's walk: a core track, or a path of a block instance. */
struct HubStage {
	bool core;
	uint32_t a; ///< Core: the edge. Block: the node.
	uint32_t b; ///< Core: the track. Block: the block path.
};

/** A segment of a hub edge: a core track (core, edge, track) or a block edge (not core, node, block edge). */
using HubSegment = std::array<uint32_t, 3>;

/** The hub edges, their segments, and where each segment lies. */
struct HubEdge {
	std::vector<HubSegment> segments;
};

} // namespace

bool ComposeJunctionHub(const JunctionTopology &hub_topology, const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, const JunctionTrackOrder &order, const JunctionTrackPrograms &programs, const JunctionTrackPlacement &placement, const JunctionLayout &core_layout, const std::vector<JunctionTrackRoute> &core_routes, const JunctionSpec &spec, JunctionHub &hub, std::string *error)
{
	auto fail = [&](const std::string &message) {
		if (error != nullptr) *error = "hub: " + message;
		return false;
	};
	hub = JunctionHub{};
	(void)paths;
	const uint32_t width = JunctionGraphWidth(graph);
	if (width != 2) return fail("the core is not a bundle graph of width 2");
	const std::vector<JunctionPortSlot> slots1 = JunctionPortSlots(topology);
	const std::vector<JunctionPortSlot> slots2 = JunctionPortSlots(hub_topology);
	if (slots1.empty() || slots2.size() != slots1.size() * width) return fail("the hub topology is not the one-lane topology at the bundle width");
	if (placement.track_ports.size() != slots1.size() || placement.nodes.size() != graph.nodes.size()) return fail("the placement is not that of this graph");
	for (const JunctionLaneGroup &group : hub_topology.groups) {
		if (group.lanes != width) return fail("a hub group is not at the bundle width");
	}

	/* The blocks, as placement builds them. */
	JunctionBlock exit_block, join_block;
	std::string block_error;
	if (!BuildJunctionBlock(JunctionExitBlockProgram(), spec, exit_block, &block_error)) return fail("the exit block: " + block_error);
	if (!BuildJunctionBlock(JunctionJoinBlock227Program(), spec, join_block, &block_error)) return fail("the join block: " + block_error);
	std::vector<HubBlock> blocks(graph.nodes.size());
	for (uint32_t v = 0; v < graph.nodes.size(); v++) {
		const JunctionBlockKind kind = graph.nodes[v].block;
		if (kind != JunctionBlockKind::Exit && kind != JunctionBlockKind::Join) return fail("a block other than an exit or a join is not ruled yet");
		HubBlock &hb = blocks[v];
		hb.block = kind == JunctionBlockKind::Exit ? &exit_block : &join_block;
		hb.mirrored = JunctionBlockSide(graph, order, programs, v) < 0;
		hb.oriented = OrientJunctionBlock(*hb.block, programs.nodes[v].heading, hb.mirrored);
		const std::vector<JunctionPortSlot> block_slots = JunctionPortSlots(hb.block->topology);
		for (size_t i = 0; i < block_slots.size(); i++) {
			const JunctionSide side = hb.block->topology.groups[block_slots[i].group].side;
			if (side == JunctionSide::W) hb.in_slot.push_back(static_cast<uint16_t>(i));
			if (side == JunctionSide::E) hb.out_slot.insert(hb.out_slot.begin(), static_cast<uint16_t>(i));
		}
		for (uint32_t e = 0; e < graph.edges.size(); e++) {
			for (const JunctionTrackEnd &end : {graph.edges[e].from, graph.edges[e].to}) {
				if (!end.node || end.index != v) continue;
				bool in_bundle;
				const int bundle = JunctionBlockBundleOf(graph, programs, v, e, in_bundle);
				hb.bundle_edge[in_bundle ? 1 : 0][bundle] = e;
			}
		}
	}

	/* Hub slots: (one-lane slot, track) to the hub slot of its rank by port tile (W y ascending, S x ascending, E y descending). */
	std::vector<std::vector<uint16_t>> group_slots(hub_topology.groups.size());
	for (size_t h = 0; h < slots2.size(); h++) group_slots[slots2[h].group].push_back(static_cast<uint16_t>(h));
	std::vector<std::vector<uint16_t>> hub_slot(slots1.size(), std::vector<uint16_t>(width));
	std::vector<std::pair<uint16_t, uint16_t>> slot_track(slots2.size(), {UINT16_MAX, UINT16_MAX}); ///< Per hub slot: (one-lane slot, track).
	for (uint16_t s = 0; s < slots1.size(); s++) {
		const uint8_t group = slots1[s].group;
		const JunctionSide side = topology.groups[group].side;
		if (side != JunctionSide::W && side != JunctionSide::S && side != JunctionSide::E) return fail("an N port is not ruled yet");
		if (group_slots[group].size() != width || placement.track_ports[s].size() != width) return fail("a group's slots do not match the bundle width");
		auto key = [&](uint32_t k) {
			const TileIndexDiffC tile = placement.track_ports[s][k];
			return side == JunctionSide::W ? tile.y : side == JunctionSide::S ? tile.x : -tile.y;
		};
		std::vector<uint32_t> ranked(width);
		for (uint32_t k = 0; k < width; k++) ranked[k] = k;
		std::ranges::stable_sort(ranked, [&](uint32_t x, uint32_t y) { return key(x) < key(y); });
		for (uint32_t r = 0; r < width; r++) {
			hub_slot[s][ranked[r]] = group_slots[group][r];
			slot_track[group_slots[group][r]] = {s, static_cast<uint16_t>(ranked[r])};
		}
	}

	/* The core's routes by (edge, track). */
	std::map<std::pair<uint32_t, uint8_t>, size_t> core_route_of;
	for (size_t r = 0; r < core_routes.size(); r++) {
		if (core_routes[r].tags.empty()) return fail("a core route has no tags");
		core_route_of[{core_routes[r].tags.front().edge, core_routes[r].copy}] = r;
	}

	/* Lane paths by the walk (D301). */
	struct HubPath {
		JunctionLanePath lane;
		std::vector<HubStage> stages;
	};
	std::vector<HubPath> hub_paths;
	std::string walk_error;
	std::vector<HubStage> stages;
	std::function<bool(uint32_t, uint32_t, uint16_t)> walk = [&](uint32_t e, uint32_t t, uint16_t from) {
		stages.push_back({true, e, t});
		const JunctionTrackEdge &edge = graph.edges[e];
		if (!edge.to.node) {
			const uint16_t to = hub_slot[edge.to.index][t];
			const uint8_t in_group = slots2[from].group, out_group = slots2[to].group;
			const auto movement = std::ranges::find(hub_topology.movements, std::pair<uint8_t, uint8_t>{in_group, out_group});
			if (movement == hub_topology.movements.end()) {
				walk_error = "a walk ends in a group pair that is no movement";
				return false;
			}
			bool prio = false;
			for (const HubStage &stage : stages) {
				if (stage.core || graph.nodes[stage.a].block != JunctionBlockKind::Join) continue;
				const JunctionBlock &jb = *blocks[stage.a].block;
				prio = std::ranges::count(jb.topology.prio_movements, static_cast<uint8_t>(jb.paths[stage.b].movement)) > 0;
			}
			hub_paths.push_back({{from, to, static_cast<uint16_t>(movement - hub_topology.movements.begin()), prio}, stages});
			stages.pop_back();
			return true;
		}
		const uint32_t w = edge.to.index;
		HubBlock &hb = blocks[w];
		bool in_bundle;
		const int bundle = JunctionBlockBundleOf(graph, programs, w, e, in_bundle);
		const int port = in_bundle ? hb.PortOfTrack(hb.block->in_bundles, bundle, t) : -1;
		if (port < 0) {
			walk_error = "a core track meets no in port of its block";
			return false;
		}
		for (uint32_t i = 0; i < hb.block->paths.size(); i++) {
			if (HubBlock::PortOf(hb.in_slot, hb.block->paths[i].from) != port) continue;
			const int out_port = HubBlock::PortOf(hb.out_slot, hb.block->paths[i].to);
			const auto [out_bundle, out_track] = hb.BundleOfPort(hb.block->out_bundles, out_port);
			if (out_bundle < 0) {
				walk_error = "a block path ends at no out bundle";
				return false;
			}
			const uint32_t next = hb.bundle_edge[0][out_bundle];
			if (next == UINT32_MAX || !(graph.edges[next].from.node && graph.edges[next].from.index == w)) {
				walk_error = "an out bundle has no core edge";
				return false;
			}
			stages.push_back({false, w, i});
			const bool ok = walk(next, static_cast<uint32_t>(out_track), from);
			stages.pop_back();
			if (!ok) return false;
		}
		stages.pop_back();
		return true;
	};
	for (uint16_t h = 0; h < slots2.size(); h++) {
		if (!hub_topology.groups[slots2[h].group].in) continue;
		const auto [s, k] = slot_track[h];
		uint32_t e = UINT32_MAX;
		for (uint32_t i = 0; i < graph.edges.size(); i++) {
			if (!graph.edges[i].from.node && graph.edges[i].from.index == s) e = i;
		}
		if (e == UINT32_MAX) return fail("an in slot has no core edge");
		if (!walk(e, k, h)) return fail(walk_error);
	}

	hub.topology = hub_topology;
	for (const HubPath &hp : hub_paths) hub.topology.lane_paths.push_back(hp.lane);
	std::string expand_error;
	hub.paths = ExpandJunctionLanes(hub.topology, &expand_error);
	if (hub.paths.empty()) return fail("the lane paths: " + expand_error);

	/* The signal graph: segments of each path, grouped into chains (a core track joins the block edge it meets; two block edges meet at a node). */
	std::vector<HubEdge> hub_edges;
	std::map<HubSegment, std::pair<uint32_t, uint32_t>> segment_at; ///< Segment to (hub edge, tokens before it in the hub edge).
	std::map<HubSegment, uint32_t> chain_of; ///< The first segment of a chain to its hub edge.
	std::vector<std::vector<uint32_t>> path_edges;
	for (const HubPath &hp : hub_paths) {
		std::vector<HubSegment> seq;
		for (const HubStage &stage : hp.stages) {
			if (stage.core) {
				seq.push_back({1, stage.a, stage.b});
				continue;
			}
			for (uint16_t be : blocks[stage.a].block->graph.path_edges[stage.b]) seq.push_back({0, stage.a, be});
		}
		std::vector<uint32_t> edges;
		for (size_t i = 0; i < seq.size();) {
			size_t j = i + 1;
			while (j < seq.size() && (seq[j][0] == 1 || seq[j - 1][0] == 1)) j++;
			const std::vector<HubSegment> chain(seq.begin() + i, seq.begin() + j);
			const auto found = chain_of.find(chain.front());
			uint32_t he;
			if (found == chain_of.end()) {
				he = static_cast<uint32_t>(hub_edges.size());
				hub_edges.push_back({chain});
				chain_of[chain.front()] = he;
				uint32_t tokens = 0;
				for (const HubSegment &seg : chain) {
					segment_at[seg] = {he, tokens};
					if (seg[0] == 0) tokens += static_cast<uint32_t>(blocks[seg[1]].block->programs.edges[seg[2]].size());
				}
			} else {
				he = found->second;
				if (hub_edges[he].segments != chain) return fail("a chain of segments differs between two paths");
			}
			edges.push_back(he);
			i = j;
		}
		path_edges.push_back(std::move(edges));
	}

	JunctionTrackGraph &sg = hub.graph;
	std::vector<uint32_t> node_base(graph.nodes.size());
	for (uint32_t v = 0; v < graph.nodes.size(); v++) {
		node_base[v] = static_cast<uint32_t>(sg.nodes.size());
		for (const JunctionTrackNode &bn : blocks[v].block->graph.nodes) sg.nodes.push_back({bn.split, UINT16_MAX, 0, JunctionBlockKind::Plain});
	}
	for (const HubEdge &he : hub_edges) {
		JunctionTrackEdge edge;
		const HubSegment &first = he.segments.front();
		const HubSegment &last = he.segments.back();
		if (first[0] == 1) {
			const JunctionTrackEdge &ce = graph.edges[first[1]];
			if (ce.from.node) return fail("a chain starts at a core track that starts at a node");
			edge.from = {false, hub_slot[ce.from.index][first[2]]};
		} else {
			const JunctionTrackEdge &be = blocks[first[1]].block->graph.edges[first[2]];
			if (!be.from.node) return fail("a chain starts at a block port");
			edge.from = {true, static_cast<uint16_t>(node_base[first[1]] + be.from.index)};
		}
		if (last[0] == 1) {
			const JunctionTrackEdge &ce = graph.edges[last[1]];
			if (ce.to.node) return fail("a chain ends at a core track that ends at a node");
			edge.to = {false, hub_slot[ce.to.index][last[2]]};
		} else {
			const JunctionTrackEdge &be = blocks[last[1]].block->graph.edges[last[2]];
			if (!be.to.node) return fail("a chain ends at a block port");
			edge.to = {true, static_cast<uint16_t>(node_base[last[1]] + be.to.index)};
		}
		edge.kind = !edge.from.node ? JunctionTrackEdge::Kind::In : !edge.to.node ? JunctionTrackEdge::Kind::Out : JunctionTrackEdge::Kind::Leg;
		sg.edges.push_back(edge);
	}
	sg.path_edges = path_edges;
	for (size_t p = 0; p < path_edges.size(); p++) {
		for (uint32_t he : path_edges[p]) {
			std::vector<uint16_t> &on = sg.edges[he].paths;
			if (on.empty() || on.back() != p) on.push_back(static_cast<uint16_t>(p));
		}
		for (uint32_t he : path_edges[p]) {
			for (const JunctionTrackEnd &end : {sg.edges[he].from, sg.edges[he].to}) {
				if (!end.node) continue;
				JunctionTrackNode &node = sg.nodes[end.index];
				if (node.slot == UINT16_MAX) node.slot = node.split ? hub_paths[p].lane.from : hub_paths[p].lane.to;
			}
		}
	}
	for (const JunctionTrackNode &node : sg.nodes) {
		if (node.slot == UINT16_MAX) return fail("a block node is on no path");
	}

	/* Every core track and block edge is on a hub edge; the crossings are mapped. */
	auto edge_of = [&](const HubSegment &segment, uint32_t &he) {
		const auto it = segment_at.find(segment);
		if (it == segment_at.end()) return false;
		he = it->second.first;
		return true;
	};
	for (uint32_t e = 0; e < graph.edges.size(); e++) {
		for (uint32_t t = 0; t < width; t++) {
			uint32_t he;
			if (!edge_of({1, e, t}, he)) return fail("a core track is on no path");
		}
	}
	for (uint32_t v = 0; v < graph.nodes.size(); v++) {
		for (uint32_t be = 0; be < blocks[v].block->graph.edges.size(); be++) {
			uint32_t he;
			if (!edge_of({0, v, be}, he)) return fail("a block edge is on no path");
		}
	}
	for (uint32_t v = 0; v < graph.nodes.size(); v++) {
		for (const auto &[a, b] : blocks[v].block->graph.crossings) {
			uint32_t ha, hb;
			if (!edge_of({0, v, a}, ha) || !edge_of({0, v, b}, hb)) return fail("a block crossing has no hub edge");
			sg.crossings.emplace_back(std::min(ha, hb), std::max(ha, hb));
		}
	}
	for (const auto &[x, y] : graph.crossings) {
		for (uint32_t a = 0; a < width; a++) {
			for (uint32_t b = 0; b < width; b++) {
				uint32_t ha, hb;
				if (!edge_of({1, x, a}, ha) || !edge_of({1, y, b}, hb)) return fail("a core crossing has no hub edge");
				sg.crossings.emplace_back(std::min(ha, hb), std::max(ha, hb));
			}
		}
	}
	std::ranges::sort(sg.crossings);
	sg.crossings.erase(std::unique(sg.crossings.begin(), sg.crossings.end()), sg.crossings.end());

	/* The tokens of the hub edges that hold a doubled block edge. */
	hub.programs.edges.resize(sg.edges.size());
	hub.programs.copies.resize(sg.edges.size());
	for (size_t he = 0; he < hub_edges.size(); he++) {
		for (const HubSegment &seg : hub_edges[he].segments) {
			if (seg[0] != 0) continue;
			const JunctionTrackPrograms &bp = blocks[seg[1]].block->programs;
			hub.programs.edges[he].insert(hub.programs.edges[he].end(), bp.edges[seg[2]].begin(), bp.edges[seg[2]].end());
			hub.programs.copies[he].insert(hub.programs.copies[he].end(), bp.copies[seg[2]].begin(), bp.copies[seg[2]].end());
		}
	}

	/* The routes: per hub path and copy, the core tracks' pieces and the blocks' oriented routes joined at the attach tiles. */
	auto block_raster = [&](uint32_t v, size_t j, JunctionRaster &raster) {
		const JunctionRoute &route = blocks[v].oriented.routes[j];
		const TileIndexDiffC anchor = placement.nodes[v];
		raster = RasterizeJunctionRoute({{static_cast<int16_t>(anchor.x + route.start.x), static_cast<int16_t>(anchor.y + route.start.y)}, route.start_trackdir, route.moves});
		return raster.error.empty();
	};
	for (size_t p = 0; p < hub_paths.size(); p++) {
		const HubPath &hp = hub_paths[p];
		std::vector<std::vector<size_t>> stage_routes(hp.stages.size()); ///< Per block stage, the indices of its routes in the block (copy 0 first).
		size_t copies = 1;
		for (size_t i = 0; i < hp.stages.size(); i++) {
			if (hp.stages[i].core) continue;
			const JunctionBlock &jb = *blocks[hp.stages[i].a].block;
			for (size_t j = 0; j < jb.track_routes.size(); j++) {
				if (jb.track_routes[j].path == hp.stages[i].b) stage_routes[i].push_back(j);
			}
			if (stage_routes[i].empty() || stage_routes[i].size() > 2) return fail("a block path has no route or more than two");
			if (stage_routes[i].size() == 2) {
				if (copies == 2) return fail("a hub path passes more than one doubled block edge");
				copies = 2;
			}
		}
		for (uint32_t c = 0; c < copies; c++) {
			JunctionTrackRoute route;
			route.path = static_cast<uint16_t>(p);
			route.copy = static_cast<uint8_t>(c);
			for (size_t i = 0; i < hp.stages.size(); i++) {
				const HubStage &stage = hp.stages[i];
				if (stage.core) {
					const auto it = core_route_of.find({stage.a, static_cast<uint8_t>(stage.b)});
					if (it == core_route_of.end()) return fail("a core track has no route");
					const JunctionTrackRoute &core = core_routes[it->second];
					const auto [he, tokens] = segment_at.at({1, stage.a, stage.b});
					for (const JunctionPiece &piece : core.pieces) {
						route.pieces.push_back(piece);
						route.tags.push_back({he, JunctionTrackPieceTag::RUN, static_cast<uint16_t>(tokens), static_cast<uint8_t>(c)});
					}
					continue;
				}
				const std::vector<size_t> &rs = stage_routes[i];
				const size_t j = rs[rs.size() == 2 ? c : 0];
				const JunctionTrackRoute &br = blocks[stage.a].block->track_routes[j];
				JunctionRaster raster;
				if (!block_raster(stage.a, j, raster)) return fail("a block route does not rasterise: " + raster.error);
				if (raster.pieces.size() != br.tags.size()) return fail("a block route's pieces do not match its tags");
				for (size_t k = 0; k < raster.pieces.size(); k++) {
					const auto [he, tokens] = segment_at.at({0, stage.a, br.tags[k].edge});
					route.pieces.push_back(raster.pieces[k]);
					const bool token = br.tags[k].token != JunctionTrackPieceTag::RUN;
					route.tags.push_back({he, token ? static_cast<uint16_t>(br.tags[k].token + tokens) : JunctionTrackPieceTag::RUN, static_cast<uint16_t>(br.tags[k].run + tokens), static_cast<uint8_t>(c)});
				}
			}
			hub.routes.push_back(std::move(route));
		}
	}

	/* The layout: the core's with every block route's raster added; the box stays the placement's. */
	hub.layout = core_layout;
	for (uint32_t v = 0; v < graph.nodes.size(); v++) {
		for (size_t j = 0; j < blocks[v].oriented.routes.size(); j++) {
			JunctionRaster raster;
			if (!block_raster(v, j, raster)) return fail("a block route does not rasterise: " + raster.error);
			AddRasterToLayout(hub.layout, raster);
		}
	}
	auto inside = [&](TileIndexDiffC tile) { return tile.x >= 0 && tile.y >= 0 && tile.x < hub.layout.size.x && tile.y < hub.layout.size.y; };
	for (const JunctionTrack &jt : hub.layout.tracks) {
		if (!inside(jt.offset)) return fail("a block tile lies outside the box");
	}
	for (const JunctionBridge &bridge : hub.layout.bridges) {
		if (!inside(bridge.start) || !inside(bridge.end)) return fail("a block bridge lies outside the box");
	}

	/* The prio spans of every block, at the hub's tiles. */
	for (uint32_t v = 0; v < graph.nodes.size(); v++) {
		const TileIndexDiffC anchor = placement.nodes[v];
		for (const JunctionPrioSpan &span : blocks[v].oriented.prio) {
			JunctionPrioSpan out;
			out.merge = {static_cast<int16_t>(anchor.x + span.merge.x), static_cast<int16_t>(anchor.y + span.merge.y)};
			for (const auto &[tile, bits] : span.tiles) out.tiles.emplace_back(TileIndexDiffC{static_cast<int16_t>(anchor.x + tile.x), static_cast<int16_t>(anchor.y + tile.y)}, bits);
			hub.prio.push_back(std::move(out));
		}
	}
	return true;
}
