/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_bundle.cpp Test the bundle graph's structure and programs (width 2). */

#include "../../../stdafx.h"

#include "../../../3rdparty/catch2/catch.hpp"

#include <array>
#include <cstdlib>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "../../../junction/junction_spec.h"
#include "../../../junction/topology/junction_topology.h"
#include "../../../junction/junction_track_geometry.h"
#include "../../../junction/placement/junction_placement.h"
#include "../../../junction/placement/junction_joint.h"
#include "../../../junction/placement/junction_render.h"
#include "../../../junction/junction_route.h"
#include "../../../track_func.h"

using Item = JunctionTrackStackItem;
using ItemKind = JunctionTrackStackItem::Kind;
using Bridge = JunctionTrackBridge;
using Step = JunctionTrackStep;
using Program = std::vector<Step>;
using Programs = std::vector<Program>;
static constexpr uint32_t NR = JunctionTrackStep::NO_ROW;
static Step Fan(uint32_t i) { return {Step::Kind::Fan, 0, i, 0}; }
static Step C(int h, uint32_t i) { return {Step::Kind::Corner, static_cast<int8_t>(h), i, 0}; }
static Step XS(int h) { return {Step::Kind::SBend, static_cast<int8_t>(h), NR, 0, Step::Role::Cross}; }
static Step XC(int h) { return {Step::Kind::Corner, static_cast<int8_t>(h), NR, 0, Step::Role::Cross}; }
static Step O(uint32_t x) { return {Step::Kind::Over, 0, x, 0}; }
static Step U(uint32_t x, uint8_t c) { return {Step::Kind::Under, 0, x, c}; }

/** The T's one-lane paths, and its bundle graph with every group `lanes` wide, in the numbering of the T's track graph. */
struct BundleT {
	const JunctionTopology &topology = JunctionTTopology();
	std::vector<JunctionLanePath> paths;
	JunctionTrackGraph graph;
	JunctionTrackOrder order;
	std::string error;

	explicit BundleT(uint8_t lanes)
	{
		JunctionTopology wide = JunctionTTopology();
		for (JunctionLaneGroup &group : wide.groups) group.lanes = lanes;
		graph = BuildJunctionBundleGraph(wide, &error);
		REQUIRE(!graph.edges.empty());
		paths = ExpandJunctionLanes(topology, &error);
		order = OrderJunctionTrackGraph(topology, paths, graph, &error);
		REQUIRE(!order.rows.empty());
	}
};

TEST_CASE("bundle: a bundle wider than two tracks is refused", "[junction][bundle]")
{
	BundleT t(3);
	std::string error;
	const JunctionTrackStructure structure = StructureJunctionTrackGraph(t.topology, t.paths, t.graph, t.order, &error);
	CHECK(error == "a bundle wider than two tracks is not ruled yet");
	CHECK(structure.stack.empty());
}

TEST_CASE("bundle: the T's structure two wide", "[junction][bundle]")
{
	BundleT t(2);
	const JunctionTrackStructure structure = StructureJunctionTrackGraph(t.topology, t.paths, t.graph, t.order, &t.error);
	CHECK(structure.stack == std::vector<Item>{
		{0, ItemKind::Lane, 0}, {0, ItemKind::Copy, 1}, {0, ItemKind::BlockSide, 0}, {0, ItemKind::BlockSide, 1},
		{1, ItemKind::Copy, 1}, {1, ItemKind::Lane, 0}, {1, ItemKind::BlockSide, 1}, {1, ItemKind::BlockSide, 0},
	});
	CHECK(structure.bridges == std::vector<Bridge>{{5, {8, 4}, true}, {8, {4}, false}});
}

TEST_CASE("bundle: the T's programs two wide", "[junction][bundle]")
{
	BundleT t(2);
	const JunctionTrackStructure structure = StructureJunctionTrackGraph(t.topology, t.paths, t.graph, t.order, &t.error);
	REQUIRE(!structure.stack.empty());
	const JunctionTrackPrograms programs = ProgramJunctionTrackGraph(t.topology, t.paths, t.graph, t.order, structure, &t.error);
	REQUIRE(programs.edges.size() == 12);
	REQUIRE(programs.copies.size() == 12);
	const Programs edges = {
		{Fan(0)}, {}, {Fan(5)}, {},
		{C(+1, NR), U(5, 1), U(5, 0), XS(-1), XC(-1), U(8, 0), XC(+1)},
		{O(8), O(4)},
		{C(-1, NR)},
		{C(-1, 7)},
		{O(4), U(5, 0), U(5, 1), C(+1, 2)},
		{Fan(5)}, {}, {Fan(0)},
	};
	CHECK(programs.edges == edges);
	Programs copies(12);
	copies[0] = {Fan(1)};
	copies[2] = {Fan(4)};
	copies[4] = edges[4];
	copies[5] = {O(8), O(4)};
	copies[6] = {C(-1, NR)};
	copies[7] = {C(-1, 6)};
	copies[8] = {O(4), U(5, 0), U(5, 1), C(+1, 3)};
	copies[9] = {Fan(4)};
	copies[11] = {Fan(1)};
	CHECK(programs.copies == copies);
	REQUIRE(programs.ends.size() == 12);
	for (size_t e = 0; e < t.graph.edges.size(); e++) {
		if (t.graph.edges[e].from.node) CHECK(programs.ends[e].first.token == JunctionTrackEdgeEnd::NO_TOKEN);
		if (t.graph.edges[e].to.node) CHECK(programs.ends[e].second.token == JunctionTrackEdgeEnd::NO_TOKEN);
	}
	REQUIRE(programs.nodes.size() == 6);
	const uint32_t items[6] = {0, NR, 5, 5, NR, 0};
	for (size_t v = 0; v < 6; v++) CHECK(programs.nodes[v].item == items[v]);
}

TEST_CASE("bundle: edge tracks", "[junction][bundle]")
{
	for (uint8_t lanes : {1, 2}) {
		BundleT t(lanes);
		const JunctionTrackStructure structure = StructureJunctionTrackGraph(t.topology, t.paths, t.graph, t.order, &t.error);
		REQUIRE(!structure.stack.empty());
		const JunctionTrackPrograms programs = ProgramJunctionTrackGraph(t.topology, t.paths, t.graph, t.order, structure, &t.error);
		REQUIRE(programs.edges.size() == 12);
		for (uint32_t e = 0; e < 12; e++) CHECK(JunctionEdgeTracks(t.graph, programs, e) == (lanes == 2 || e == 5 ? 2 : 1));
	}
}

TEST_CASE("bundle: the T's arrangement reads track 0", "[junction][bundle]")
{
	BundleT t(2);
	const JunctionTrackStructure structure = StructureJunctionTrackGraph(t.topology, t.paths, t.graph, t.order, &t.error);
	REQUIRE(!structure.stack.empty());
	const JunctionTrackPrograms programs = ProgramJunctionTrackGraph(t.topology, t.paths, t.graph, t.order, structure, &t.error);
	REQUIRE(programs.edges.size() == 12);
	const JunctionTrackArrangement arrangement = ArrangeJunctionTrackGraph(t.graph, t.order, programs);
	std::vector<std::array<uint32_t, 3>> unders;
	for (const JunctionArrangementUnder &under : arrangement.unders) unders.push_back({under.edge, under.token, programs.edges[under.edge][under.token].copy});
	CHECK(unders == std::vector<std::array<uint32_t, 3>>{{4, 1, 1}, {4, 2, 0}, {8, 1, 0}, {8, 2, 1}});
}

TEST_CASE("bundle: band track 1 is track 0 with its own items", "[junction][bundle]")
{
	BundleT t(2);
	const JunctionTrackStructure structure = StructureJunctionTrackGraph(t.topology, t.paths, t.graph, t.order, &t.error);
	REQUIRE(!structure.stack.empty());
	const JunctionTrackPrograms programs = ProgramJunctionTrackGraph(t.topology, t.paths, t.graph, t.order, structure, &t.error);
	REQUIRE(programs.copies.size() == 12);
	CHECK(programs.copies[4] == programs.edges[4]);
	CHECK(programs.copies[6] == Program{C(-1, NR)});
	CHECK(programs.copies[7] == Program{C(-1, 6)});
	CHECK(programs.copies[8] == Program{O(4), U(5, 0), U(5, 1), C(+1, 3)});
	CHECK(programs.copies[1].empty());
	CHECK(programs.copies[10].empty());
	for (uint32_t e : {4, 6, 7, 8}) {
		REQUIRE(programs.copies[e].size() == programs.edges[e].size());
		for (size_t i = 0; i < programs.edges[e].size(); i++) {
			Step a = programs.edges[e][i], b = programs.copies[e][i];
			if (a.kind == Step::Kind::Corner) a.index = b.index = 0;
			CHECK(a == b);
		}
	}
}

TEST_CASE("bundle: block geometry", "[junction][bundle]")
{
	JunctionSpec spec = MakeTestJunctionSpec();
	spec.exit_block_along = spec.join_block_along = 4;
	CHECK(JunctionBlockFootprintOf(JunctionBlockKind::Exit, 2, spec) == JunctionBlockFootprint{4, 4});
	CHECK(JunctionBlockFootprintOf(JunctionBlockKind::Join, 2, spec) == JunctionBlockFootprint{4, 4});
	CHECK(JunctionBlockFootprintOf(JunctionBlockKind::Plain, 2, spec) == JunctionBlockFootprint{1, 1});

	BundleT t(2);
	const JunctionTrackStructure structure = StructureJunctionTrackGraph(t.topology, t.paths, t.graph, t.order, &t.error);
	REQUIRE(!structure.stack.empty());
	const JunctionTrackPrograms programs = ProgramJunctionTrackGraph(t.topology, t.paths, t.graph, t.order, structure, &t.error);
	REQUIRE(programs.nodes.size() == 6);
	const int8_t sides[6] = {+1, -1, -1, -1, -1, +1};
	for (uint32_t v = 0; v < 6; v++) CHECK(JunctionBlockSide(t.graph, t.order, programs, v) == sides[v]);

	/* Per node, its three edges and their attach (along, across) at track 0; the across of track k adds k. */
	struct Row { uint32_t node; int8_t side; uint32_t edge; int along; int across; };
	const Row rows[] = {
		{0, +1, 0, -1, 0}, {0, +1, 3, 4, 0}, {0, +1, 4, 4, 2},
		{2, -1, 2, -1, 0}, {2, -1, 5, 4, 0}, {2, -1, 6, 4, -2},
		{5, +1, 3, -1, 0}, {5, +1, 8, -1, 2}, {5, +1, 11, 4, 0},
		{3, -1, 5, -1, 0}, {3, -1, 7, -1, -2}, {3, -1, 9, 4, 0},
		{1, -1, 1, -1, 0}, {1, -1, 8, 4, 0}, {1, -1, 7, 4, -2},
		{4, -1, 4, -1, 0}, {4, -1, 6, -1, -2}, {4, -1, 10, 4, 0},
	};
	for (const Row &row : rows) {
		const JunctionBlockFootprint footprint = JunctionBlockFootprintOf(t.graph.nodes[row.node].block, 2, spec);
		REQUIRE(footprint == JunctionBlockFootprint{4, 4});
		for (uint8_t k = 0; k < 2; k++) {
			INFO("node " << row.node << " edge " << row.edge << " track " << int(k));
			CHECK(JunctionBlockAttachOf(t.graph, programs, row.node, row.side, footprint, row.edge, k) == JunctionBlockAttach{row.along, row.across + k});
		}
	}
}

/** The width-2 T through placement with the given block lengths; the placement's error is in `error`. */
static JunctionTrackPlacement PlaceBundleT(uint8_t along, std::string *error)
{
	JunctionSpec spec = MakeTestJunctionSpec();
	spec.exit_block_along = spec.join_block_along = along;
	BundleT t(2);
	const JunctionTrackStructure structure = StructureJunctionTrackGraph(t.topology, t.paths, t.graph, t.order, &t.error);
	REQUIRE(!structure.stack.empty());
	const JunctionTrackPrograms programs = ProgramJunctionTrackGraph(t.topology, t.paths, t.graph, t.order, structure, &t.error);
	REQUIRE(programs.nodes.size() == 6);
	return PlaceJunctionTrackGraph(t.topology, t.paths, t.graph, t.order, structure, programs, spec, error);
}

/** A tile moved by (dx, dy). */
static bool Same(TileIndexDiffC a, TileIndexDiffC b) { return a.x == b.x && a.y == b.y; }
static TileIndexDiffC Moved(TileIndexDiffC tile, int dx, int dy) { return {static_cast<int16_t>(tile.x + dx), static_cast<int16_t>(tile.y + dy)}; }

/** The width-2 T's choices of 5a: every form 0, placed through the programs with the given block length. */
struct PlacedBundleT {
	JunctionTrackPrograms programs;
	JunctionTrackPlacement placement;
	std::string error;
};

static PlacedBundleT PlaceBundleTWithForms(uint8_t along, int form_4_3)
{
	PlacedBundleT out;
	JunctionSpec spec = MakeTestJunctionSpec();
	spec.exit_block_along = spec.join_block_along = along;
	BundleT t(2);
	const JunctionTrackStructure structure = StructureJunctionTrackGraph(t.topology, t.paths, t.graph, t.order, &t.error);
	REQUIRE(!structure.stack.empty());
	out.programs = ProgramJunctionTrackGraph(t.topology, t.paths, t.graph, t.order, structure, &t.error);
	REQUIRE(out.programs.nodes.size() == 6);
	if (form_4_3 < 0) {
		out.placement = PlaceJunctionTrackGraph(t.topology, t.paths, t.graph, t.order, structure, out.programs, spec, &out.error);
		return out;
	}
	/* The choices of the first placement, with edge 4's token 3 given the square form. */
	JunctionPlacementChoices choices = PlaceJunctionTrackGraph(t.topology, t.paths, t.graph, t.order, structure, out.programs, spec, &out.error).choices;
	REQUIRE(out.error.empty());
	choices.form.resize(12);
	for (size_t e = 0; e < 12; e++) choices.form[e].assign(out.programs.edges[e].size(), 0);
	choices.form[4][3] = form_4_3;
	out.error.clear();
	out.placement = PlaceJunctionTrackGraph(t.topology, t.paths, t.graph, t.order, structure, out.programs, spec, choices, &out.error);
	return out;
}

TEST_CASE("bundle: the T places two wide on flat ground", "[junction][bundle]")
{
	const PlacedBundleT placed = PlaceBundleTWithForms(4, -1);
	const JunctionTrackPlacement &p = placed.placement;
	INFO("error " << placed.error << ", why chain length " << p.why.size());
	REQUIRE(placed.error.empty());

	/* The size, pinned on the first green (step 5a). */
	INFO("size " << p.size.x << " x " << p.size.y);
	CHECK((p.size.x == 37 && p.size.y == 21));
	/* Track 1 of every band edge bend is track 0's span moved by D (DESIGN-width 5a.2). */
	struct Move { uint32_t edge; size_t token; int dx, dy; };
	const Move moves[] = {{4, 0, -1, +1}, {4, 3, -1, 0}, {6, 0, -1, -1}, {7, 0, +1, -1}, {8, 3, +1, +1}};
	for (const Move &m : moves) {
		INFO("edge " << m.edge << " token " << m.token);
		REQUIRE(p.edges[m.edge].size() > m.token);
		REQUIRE(p.copies[m.edge].size() > m.token);
		CHECK(Same(p.copies[m.edge][m.token].first, Moved(p.edges[m.edge][m.token].first, m.dx, m.dy)));
		CHECK(Same(p.copies[m.edge][m.token].second, Moved(p.edges[m.edge][m.token].second, m.dx, m.dy)));
	}
	/* The bridge pairs: bridge 1 (edge 8 over edge 4) on the column axis, bridge 0 (row 1). */
	REQUIRE(p.bridges.size() == 2);
	CHECK(Same(p.bridges[1][1].start, Moved(p.bridges[1][0].start, +1, +1)));
	CHECK(Same(p.bridges[1][1].end, Moved(p.bridges[1][0].end, +1, +1)));
	CHECK(Same(p.bridges[0][1].start, Moved(p.bridges[0][0].start, +1, -1)));
	CHECK(Same(p.bridges[0][1].end, Moved(p.bridges[0][0].end, +1, -1)));

	/* The attach tiles are the anchor plus the 4a offsets, per track; the block ends follow the footprint. */
	static constexpr int DX[4] = {0, 1, 0, -1};
	static constexpr int DY[4] = {-1, 0, 1, 0};
	JunctionSpec spec = MakeTestJunctionSpec();
	spec.exit_block_along = spec.join_block_along = 4;
	BundleT t(2);
	REQUIRE(p.block_ends.size() == t.graph.nodes.size());
	for (uint32_t v = 0; v < t.graph.nodes.size(); v++) {
		const int h = placed.programs.nodes[v].heading, r = (h + 1) % 4;
		const JunctionBlockFootprint footprint = JunctionBlockFootprintOf(t.graph.nodes[v].block, 2, spec);
		const int8_t side = JunctionBlockSide(t.graph, t.order, placed.programs, v);
		CHECK(Same(p.block_ends[v], Moved(p.nodes[v], (footprint.along - 1) * DX[h], (footprint.along - 1) * DY[h])));
		for (uint32_t e = 0; e < t.graph.edges.size(); e++) {
			for (int z = 0; z < 2; z++) {
				const JunctionTrackEnd &end = z == 0 ? t.graph.edges[e].from : t.graph.edges[e].to;
				if (!end.node || end.index != v) continue;
				for (uint8_t k = 0; k < 2; k++) {
					const JunctionBlockAttach a = JunctionBlockAttachOf(t.graph, placed.programs, v, side, footprint, e, k);
					INFO("node " << v << " edge " << e << " track " << int(k));
					CHECK(Same(p.attach[e][z][k], Moved(p.nodes[v], a.along * DX[h] + a.across * DX[r], a.along * DY[h] + a.across * DY[r])));
				}
			}
		}
	}
}

TEST_CASE("bundle: the S-bend column form leans as derived", "[junction][bundle]")
{
	CHECK(JunctionSBendColumnForm(2, -1).s == 1);
}

TEST_CASE("bundle: a square crossing of a bundle is refused", "[junction][bundle]")
{
	const PlacedBundleT placed = PlaceBundleTWithForms(4, 1);
	INFO("why chain length " << placed.placement.why.size());
	CHECK(placed.error == "a square crossing of a bundle is not ruled yet");
}

TEST_CASE("bundle: the joint loop settles on the bundle T", "[junction][bundle]")
{
	JunctionSpec spec = MakeTestJunctionSpec();
	spec.exit_block_along = spec.join_block_along = 4;
	BundleT t(2);
	const JunctionTrackStructure structure = StructureJunctionTrackGraph(t.topology, t.paths, t.graph, t.order, &t.error);
	REQUIRE(!structure.stack.empty());
	const JunctionTrackPrograms programs = ProgramJunctionTrackGraph(t.topology, t.paths, t.graph, t.order, structure, &t.error);
	REQUIRE(programs.nodes.size() == 6);
	std::string error;
	const JunctionPlacementChoices start = PlaceJunctionTrackGraph(t.topology, t.paths, t.graph, t.order, structure, programs, spec, &error).choices;
	REQUIRE(error.empty());
	const JunctionJointLoopResult loop = SolveJunctionJointLoop(t.topology, t.paths, t.graph, t.order, structure, programs, spec, start);
	INFO("outcome " << loop.outcome);
	CHECK(loop.outcome == "settled");
	const JunctionTrackPlacement placed = PlaceJunctionTrackGraph(t.topology, t.paths, t.graph, t.order, structure, programs, spec, loop.choices, &error);
	INFO("error " << error);
	CHECK(!placed.nodes.empty());
}

TEST_CASE("bundle: the T renders two wide", "[junction][bundle]")
{
	JunctionSpec spec = MakeTestJunctionSpec();
	spec.exit_block_along = spec.join_block_along = 4;
	BundleT t(2);
	const JunctionTrackStructure structure = StructureJunctionTrackGraph(t.topology, t.paths, t.graph, t.order, &t.error);
	REQUIRE(!structure.stack.empty());
	const JunctionTrackPrograms programs = ProgramJunctionTrackGraph(t.topology, t.paths, t.graph, t.order, structure, &t.error);
	REQUIRE(programs.nodes.size() == 6);
	std::string error;
	const JunctionTrackPlacement placement = PlaceJunctionTrackGraph(t.topology, t.paths, t.graph, t.order, structure, programs, spec, &error);
	REQUIRE(error.empty());

	std::vector<JunctionTrackRoute> routes;
	const JunctionLayout layout = RenderJunctionTrackPlacement(t.topology, t.paths, t.graph, structure, programs, placement, &error, &routes);
	INFO("render error " << error);
	REQUIRE(!layout.tracks.empty());
	if (std::getenv("JUNCTION_DUMP_BUNDLE") != nullptr) {
		std::ofstream("/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/bundle-t.txt") << RenderJunctionAscii(layout);
	}

	/* One route per (edge, track), from the track's start tile to its end tile. */
	REQUIRE(routes.size() == 24);
	for (const JunctionTrackRoute &route : routes) {
		REQUIRE(!route.pieces.empty());
		const uint32_t e = route.tags.front().edge;
		const JunctionTrackEdge &edge = t.graph.edges[e];
		const TileIndexDiffC start = edge.from.node ? placement.attach[e][0][route.copy] : placement.track_ports[edge.from.index][route.copy];
		const TileIndexDiffC end = edge.to.node ? placement.attach[e][1][route.copy] : placement.track_ports[edge.to.index][route.copy];
		INFO("edge " << e << " track " << int(route.copy));
		CHECK(Same(route.pieces.front().tile, start));
		CHECK(Same(route.pieces.back().tile, end));
	}

	/* Two bridge pairs (the layout normalises a bridge's ends), each pair's track 1 heading one tile upstream of track 0's along the span. */
	CHECK(layout.bridges.size() == 4);
	REQUIRE(placement.bridges.size() == 2);
	for (size_t b = 0; b < 2; b++) {
		for (int c = 0; c < 2; c++) {
			const JunctionBridge &heads = placement.bridges[b][c];
			CHECK(std::ranges::any_of(layout.bridges, [&](const JunctionBridge &l) { return (Same(l.start, heads.start) && Same(l.end, heads.end)) || (Same(l.start, heads.end) && Same(l.end, heads.start)); }));
		}
		const JunctionBridge &a = placement.bridges[b][0];
		const int dx = (a.end.x > a.start.x) - (a.end.x < a.start.x);
		const int dy = (a.end.y > a.start.y) - (a.end.y < a.start.y);
		CHECK((placement.bridges[b][1].start.x - a.start.x) * dx + (placement.bridges[b][1].start.y - a.start.y) * dy == -1);
	}

	/* A tile holds pieces of two routes only when their tracks may share it. */
	std::map<std::pair<int, int>, std::vector<std::pair<size_t, Track>>> tiles;
	for (size_t i = 0; i < routes.size(); i++) {
		for (const JunctionPiece &piece : routes[i].pieces) {
			if (piece.on_bridge) continue;
			auto &here = tiles[{piece.tile.x, piece.tile.y}];
			const Track track = TrackdirToTrack(piece.trackdir);
			if (std::ranges::none_of(here, [&](const std::pair<size_t, Track> &o) { return o.first == i && o.second == track; })) here.push_back({i, track});
		}
	}
	for (const auto &[tile, here] : tiles) {
		for (size_t a = 0; a < here.size(); a++) {
			for (size_t b = a + 1; b < here.size(); b++) {
				if (here[a].first == here[b].first) continue;
				INFO("tile " << tile.first << "," << tile.second << " routes " << here[a].first << " and " << here[b].first);
				CHECK(here[a].second != here[b].second);
				CHECK(JunctionTracksMayShareTile(TrackBits{here[a].second}, TrackBits{here[b].second}));
			}
		}
	}

	/* No piece lies inside a block footprint: its anchor to its downstream end along the heading, 4 lines across. */
	static constexpr int DX[4] = {0, 1, 0, -1};
	static constexpr int DY[4] = {-1, 0, 1, 0};
	for (uint32_t v = 0; v < t.graph.nodes.size(); v++) {
		const int h = programs.nodes[v].heading, r = (h + 1) % 4;
		const int8_t side = JunctionBlockSide(t.graph, t.order, programs, v);
		const TileIndexDiffC anchor = placement.nodes[v];
		const int length = (placement.block_ends[v].x - anchor.x) * DX[h] + (placement.block_ends[v].y - anchor.y) * DY[h];
		for (const JunctionTrackRoute &route : routes) {
			for (const JunctionPiece &piece : route.pieces) {
				if (piece.on_bridge) continue;
				const int along = (piece.tile.x - anchor.x) * DX[h] + (piece.tile.y - anchor.y) * DY[h];
				const int across = (piece.tile.x - anchor.x) * DX[r] + (piece.tile.y - anchor.y) * DY[r];
				const bool inside = along >= 0 && along <= length && (side > 0 ? across >= 0 && across <= 3 : across >= -2 && across <= 1);
				INFO("node " << v << " piece " << piece.tile.x << "," << piece.tile.y);
				CHECK(!inside);
			}
		}
	}
}

TEST_CASE("bundle: a block without its footprint is refused", "[junction][bundle]")
{
	std::string error;
	const JunctionTrackPlacement placement = PlaceBundleT(0, &error);
	CHECK(error == "a block without its footprint is not ruled yet");
}
