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

#include "../../../junction/block/junction_block.h"
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
/** The fan of an S branch port in a bundle graph: the bundle's half opens west (-1) or east (+1). */
static Step SFan(int hand) { return {Step::Kind::Fan, static_cast<int8_t>(hand), NR, 0}; }
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
		{Fan(0)}, {SFan(-1)}, {Fan(5)}, {},
		{C(+1, NR), U(5, 1), U(5, 0), XS(-1), XC(-1), U(8, 0), XC(+1)},
		{O(8), O(4)},
		{C(-1, NR)},
		{C(-1, 7)},
		{O(4), U(5, 0), U(5, 1), C(+1, 2)},
		{Fan(5)}, {SFan(+1)}, {Fan(0)},
	};
	CHECK(programs.edges == edges);
	Programs copies(12);
	copies[0] = {Fan(1)};
	copies[1] = {SFan(-1)};
	copies[2] = {Fan(4)};
	copies[4] = edges[4];
	copies[5] = {O(8), O(4)};
	copies[6] = {C(-1, NR)};
	copies[7] = {C(-1, 6)};
	copies[8] = {O(4), U(5, 0), U(5, 1), C(+1, 3)};
	copies[9] = {Fan(4)};
	copies[10] = {SFan(+1)};
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
	CHECK(programs.copies[1] == Program{SFan(-1)});
	CHECK(programs.copies[10] == Program{SFan(+1)});
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
	BundleT t(2);
	const JunctionTrackStructure structure = StructureJunctionTrackGraph(t.topology, t.paths, t.graph, t.order, &t.error);
	REQUIRE(!structure.stack.empty());
	const JunctionTrackPrograms programs = ProgramJunctionTrackGraph(t.topology, t.paths, t.graph, t.order, structure, &t.error);
	REQUIRE(programs.nodes.size() == 6);
	const int8_t sides[6] = {+1, -1, -1, -1, -1, +1};
	for (uint32_t v = 0; v < 6; v++) CHECK(JunctionBlockSide(t.graph, t.order, programs, v) == sides[v]);
}

/** A tile moved by (dx, dy). */
static bool Same(TileIndexDiffC a, TileIndexDiffC b) { return a.x == b.x && a.y == b.y; }
static TileIndexDiffC Moved(TileIndexDiffC tile, int dx, int dy) { return {static_cast<int16_t>(tile.x + dx), static_cast<int16_t>(tile.y + dy)}; }

/** The width-2 T's choices of 5a: every form 0, placed through the programs. */
struct PlacedBundleT {
	JunctionTrackPrograms programs;
	JunctionTrackPlacement placement;
	std::string error;
};

static PlacedBundleT PlaceBundleTWithForms(int form_4_3)
{
	PlacedBundleT out;
	JunctionSpec spec = MakeTestJunctionSpec();
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
	const PlacedBundleT placed = PlaceBundleTWithForms(-1);
	const JunctionTrackPlacement &p = placed.placement;
	INFO("error " << placed.error << ", why chain length " << p.why.size());
	REQUIRE(placed.error.empty());

	/* The size, pinned on the first green (step 5a). */
	INFO("size " << p.size.x << " x " << p.size.y);
	CHECK((p.size.x == 86 && p.size.y == 63));
}

TEST_CASE("bundle: the S-bend column form leans as derived", "[junction][bundle]")
{
	CHECK(JunctionSBendColumnForm(2, -1).s == 1);
}

TEST_CASE("bundle: a square crossing waits for stretching blocks", "[junction][bundle]")
{
	const PlacedBundleT placed = PlaceBundleTWithForms(1);
	INFO("why chain length " << placed.placement.why.size());
	CHECK(placed.error == "a square crossing of a bundle is not ruled until blocks stretch");
}

/* The cycle: block attach at nodes 1 and 4, S port spacing, fan and S-bend sizes of edges 1, 4 and 10, the bridge over edge 4's S-bend. */
TEST_CASE("bundle: the joint loop on the bundle T waits for stretching blocks", "[junction][bundle]")
{
	JunctionSpec spec = MakeTestJunctionSpec();
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
	CHECK(loop.outcome.starts_with("infeasible"));
}

TEST_CASE("bundle: the T renders two wide", "[junction][bundle]")
{
	JunctionSpec spec = MakeTestJunctionSpec();
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

	/* Track 1 never crosses track 0: where their tiles coincide they hold two parallel tracks, never X and Y. */
	for (const JunctionTrackRoute &r0 : routes) {
		if (r0.copy != 0) continue;
		for (const JunctionTrackRoute &r1 : routes) {
			if (r1.copy != 1 || r1.tags.front().edge != r0.tags.front().edge) continue;
			for (const JunctionPiece &p1 : r1.pieces) {
				if (p1.on_bridge) continue;
				for (const JunctionPiece &p0 : r0.pieces) {
					if (p0.on_bridge) continue;
					INFO("edge " << r0.tags.front().edge << " tile " << p1.tile.x << "," << p1.tile.y);
					if (!Same(p0.tile, p1.tile)) continue;
					const Track k0 = TrackdirToTrack(p0.trackdir), k1 = TrackdirToTrack(p1.trackdir);
					CHECK(k0 != k1);
					CHECK(!((k0 == Track::X && k1 == Track::Y) || (k0 == Track::Y && k1 == Track::X)));
				}
			}
		}
	}
}

/** The exit block of a node, or the join block, oriented as the node places it. */
struct HubBlocks {
	JunctionBlock exit_block, join_block;
	std::vector<JunctionOrientedBlock> oriented; ///< Per graph node.

	HubBlocks(const BundleT &t, const JunctionTrackPrograms &programs, const JunctionSpec &spec)
	{
		std::string error;
		REQUIRE(BuildJunctionBlock(JunctionExitBlockProgram(), spec, exit_block, &error));
		REQUIRE(BuildJunctionBlock(JunctionJoinBlock227Program(), spec, join_block, &error));
		for (uint32_t v = 0; v < t.graph.nodes.size(); v++) {
			const JunctionBlock &block = t.graph.nodes[v].block == JunctionBlockKind::Exit ? exit_block : join_block;
			REQUIRE((t.graph.nodes[v].block == JunctionBlockKind::Exit || t.graph.nodes[v].block == JunctionBlockKind::Join));
			oriented.push_back(OrientJunctionBlock(block, programs.nodes[v].heading, JunctionBlockSide(t.graph, t.order, programs, v) < 0));
		}
	}
};

TEST_CASE("bundle: the T places with its block templates", "[junction][bundle]")
{
	const PlacedBundleT placed = PlaceBundleTWithForms(-1);
	const JunctionTrackPlacement &p = placed.placement;
	INFO("error " << placed.error << ", why chain length " << p.why.size());
	REQUIRE(placed.error.empty());

	const JunctionSpec spec = MakeTestJunctionSpec();
	BundleT t(2);
	const HubBlocks blocks(t, placed.programs, spec);
	static constexpr int DX[4] = {0, 1, 0, -1};
	static constexpr int DY[4] = {-1, 0, 1, 0};
	REQUIRE(p.block_ends.size() == t.graph.nodes.size());
	for (uint32_t v = 0; v < t.graph.nodes.size(); v++) {
		const JunctionTrackNodeShape &shape = placed.programs.nodes[v];
		const int h = shape.heading;
		const JunctionOrientedBlock &oriented = blocks.oriented[v];
		const bool split = t.graph.nodes[v].split;
		CHECK(Same(p.block_ends[v], Moved(p.nodes[v], oriented.length * DX[h], oriented.length * DY[h])));
		for (uint32_t e = 0; e < t.graph.edges.size(); e++) {
			for (int z = 0; z < 2; z++) {
				const JunctionTrackEnd &end = z == 0 ? t.graph.edges[e].from : t.graph.edges[e].to;
				if (!end.node || end.index != v) continue;
				const bool in_bundle = split ? e == shape.trunk : e != shape.trunk;
				const std::vector<std::vector<JunctionBlockPort>> &bundles = in_bundle ? oriented.in_bundles : oriented.out_bundles;
				const size_t bundle = e == shape.trunk || e == shape.straight ? 0 : 1;
				REQUIRE(bundle < bundles.size());
				for (uint8_t k = 0; k < 2; k++) {
					const JunctionBlockPort &port = bundles[bundle][k];
					const int sign = in_bundle ? -1 : 1;
					INFO("node " << v << " edge " << e << " track " << int(k));
					CHECK(Same(p.attach[e][z][k], Moved(p.nodes[v], port.tile.x + sign * DX[h], port.tile.y + sign * DY[h])));
				}
			}
		}
	}
}

TEST_CASE("bundle: core tracks and block tiles do not clash", "[junction][bundle]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	BundleT t(2);
	const JunctionTrackStructure structure = StructureJunctionTrackGraph(t.topology, t.paths, t.graph, t.order, &t.error);
	REQUIRE(!structure.stack.empty());
	const JunctionTrackPrograms programs = ProgramJunctionTrackGraph(t.topology, t.paths, t.graph, t.order, structure, &t.error);
	REQUIRE(programs.nodes.size() == 6);
	std::string error;
	const JunctionTrackPlacement placement = PlaceJunctionTrackGraph(t.topology, t.paths, t.graph, t.order, structure, programs, spec, &error);
	INFO("placement error " << error);
	REQUIRE(error.empty());
	const JunctionLayout layout = RenderJunctionTrackPlacement(t.topology, t.paths, t.graph, structure, programs, placement, &error);
	REQUIRE(!layout.tracks.empty());
	const HubBlocks blocks(t, programs, spec);

	/* Per tile, its sources: the core (-1) and each block, with the union of their bits. */
	std::map<std::pair<int, int>, std::map<int, TrackBits>> tiles;
	for (const JunctionTrack &jt : layout.tracks) tiles[{jt.offset.x, jt.offset.y}][-1] = jt.tracks;
	for (uint32_t v = 0; v < t.graph.nodes.size(); v++) {
		const JunctionOrientedBlock &oriented = blocks.oriented[v];
		for (const auto *list : {&oriented.tiles, &oriented.prio_tiles}) {
			for (const auto &[offset, bits] : *list) {
				TrackBits &here = tiles[{placement.nodes[v].x + offset.x, placement.nodes[v].y + offset.y}][static_cast<int>(v)];
				for (Track track : bits) here.Set(track);
			}
		}
	}
	for (const auto &[tile, sources] : tiles) {
		for (auto a = sources.begin(); a != sources.end(); ++a) {
			for (auto b = std::next(a); b != sources.end(); ++b) {
				INFO("tile " << tile.first << "," << tile.second << " sources " << a->first << " and " << b->first);
				CHECK(JunctionTracksMayShareTile(a->second, b->second));
			}
		}
	}

	/* Every attach tile holds a core piece. */
	for (uint32_t e = 0; e < t.graph.edges.size(); e++) {
		for (int z = 0; z < 2; z++) {
			for (const TileIndexDiffC &tile : placement.attach[e][z]) {
				if (tile.x < 0) continue;
				INFO("edge " << e << " end " << z << " attach " << tile.x << "," << tile.y);
				CHECK(tiles.contains({tile.x, tile.y}));
				CHECK(tiles[{tile.x, tile.y}].contains(-1));
			}
		}
	}
}

TEST_CASE("bundle: dump the T with its block tiles", "[junction][bundle][.]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	BundleT t(2);
	const JunctionTrackStructure structure = StructureJunctionTrackGraph(t.topology, t.paths, t.graph, t.order, &t.error);
	REQUIRE(!structure.stack.empty());
	const JunctionTrackPrograms programs = ProgramJunctionTrackGraph(t.topology, t.paths, t.graph, t.order, structure, &t.error);
	std::string error;
	const JunctionTrackPlacement placement = PlaceJunctionTrackGraph(t.topology, t.paths, t.graph, t.order, structure, programs, spec, &error);
	REQUIRE(error.empty());
	JunctionLayout layout = RenderJunctionTrackPlacement(t.topology, t.paths, t.graph, structure, programs, placement, &error);
	const HubBlocks blocks(t, programs, spec);
	std::string text = RenderJunctionAscii(layout);
	text += "size " + std::to_string(placement.size.x) + " x " + std::to_string(placement.size.y) + "\n";
	/* The block tiles stamped on a second picture: core bits where the block adds none, the block's own tiles as a letter per node. */
	std::vector<std::string> grid(placement.size.y, std::string(placement.size.x, '.'));
	for (const JunctionTrack &jt : layout.tracks) grid[jt.offset.y][jt.offset.x] = '+';
	for (uint32_t v = 0; v < t.graph.nodes.size(); v++) {
		for (const auto *list : {&blocks.oriented[v].tiles, &blocks.oriented[v].prio_tiles}) {
			for (const auto &[offset, bits] : *list) {
				const int x = placement.nodes[v].x + offset.x, y = placement.nodes[v].y + offset.y;
				if (x >= 0 && y >= 0 && x < placement.size.x && y < placement.size.y) grid[y][x] = static_cast<char>('a' + v);
			}
		}
	}
	text += "block tiles stamped on the core ('+' core, a-f the block of node 0-5):\n";
	for (const std::string &row : grid) text += row + "\n";
	std::ofstream("/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/h1a-t.txt") << text;
}
