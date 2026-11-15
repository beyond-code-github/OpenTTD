/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_block.cpp Test the junction block programs and the PSG 227 join block (DESIGN-blocks B2a). */

#include "../../../stdafx.h"

#include "../../../3rdparty/catch2/catch.hpp"

#include <algorithm>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "../../../direction_func.h"
#include "../../../junction/block/junction_block.h"
#include "../../../junction/junction_route.h"
#include "../../../junction/junction_rules.h"
#include "../../../junction/junction_spec.h"
#include "../../../junction/junction_track_geometry.h"
#include "../../../junction/signals/junction_signals.h"
#include "../../../junction/topology/junction_chain.h"
#include "../../../track_func.h"

#include "../../../safeguards.h"

/** The solved 227 block with what the tests read from it. */
struct BlockFixture {
	JunctionSpec spec = MakeTestJunctionSpec();
	JunctionBlockProgram program = JunctionJoinBlock227Program();
	JunctionTopology topology = JunctionJoinBlock227Topology();
	JunctionBlock block;
	std::string error;

	BlockFixture()
	{
		const bool built = BuildJunctionBlock(program, spec, block, &error);
		INFO(error);
		REQUIRE(built);
	}

	int Column(std::string_view name) const
	{
		const auto it = std::ranges::find(program.points, name);
		REQUIRE(it != program.points.end());
		return block.columns[it - program.points.begin()];
	}

	/** A program line minus its layout row (zero: the frame starts at line 0); route pieces are already in layout rows. */
	int Top() const { return program.in_lines[0] - block.in_ports[0].tile.y; }

	/** The tile of a node in layout coordinates: its column, and the line it sits on. */
	TileIndexDiffC NodeTile(uint16_t node) const
	{
		int line = -1;
		for (const JunctionBlockEdge &edge : program.edges) {
			if (program.nodes[node].split ? (edge.to.node && edge.to.index == node) : (edge.from.node && edge.from.index == node)) line = edge.line;
		}
		return {static_cast<int16_t>(block.columns[program.nodes[node].point]), static_cast<int16_t>(line - Top())};
	}

	int EvenPrio() const { return (spec.prio_tiles + 1) & ~1; }
};

static TrackBits BitsAt(const JunctionLayout &layout, int x, int y)
{
	for (const JunctionTrack &jt : layout.tracks) {
		if (jt.offset.x == x && jt.offset.y == y) return jt.tracks;
	}
	return {};
}

static int CountBits(TrackBits bits)
{
	int count = 0;
	for ([[maybe_unused]] Track track : bits) count++;
	return count;
}

TEST_CASE("block: the 227 join block reaches exactly its six movements", "[junction][block]")
{
	BlockFixture f;
	const std::vector<JunctionPort> ports = FindJunctionPorts(f.block.layout, f.topology);
	CHECK(ports.size() == 6);
	CHECK(std::ranges::count_if(ports, [&](const JunctionPort &p) { return p.tile.x == 0; }) == 4);
	CHECK(std::ranges::count_if(ports, [&](const JunctionPort &p) { return p.tile.x == f.block.layout.size.x - 1; }) == 2);
	const JunctionCheckContext ctx = MakeJunctionTopologyCheckContext(f.block.layout, f.spec, f.topology, ports);
	CHECK(ctx.lanes.size() == 6);
	CHECK(CheckConnectivity(ctx).empty());
	CHECK(ctx.paths.size() == 6);
}

TEST_CASE("block: an S-bend of k lines", "[junction][block]")
{
	for (int k : {-2, -1, 1, 2}) {
		int reaching = 0;
		for (int hand : {-1, 1}) {
			const JunctionRaster raster = RasterizeJunctionRoute({{16, 16}, Trackdir::X_SW, JunctionBendMoves(hand, 2 * std::abs(k), false)});
			REQUIRE(raster.error.empty());
			const JunctionPiece &closing = raster.pieces.back();
			if (closing.tile.y - raster.pieces[0].tile.y == k) {
				reaching++;
				CHECK(closing.trackdir == Trackdir::X_SW);
			}
		}
		CHECK(reaching == 1);
	}
}

TEST_CASE("block: node tiles", "[junction][block]")
{
	BlockFixture f;
	std::vector<TileIndexDiffC> nodes;
	for (uint16_t n = 0; n < f.program.nodes.size(); n++) {
		const TileIndexDiffC tile = f.NodeTile(n);
		nodes.push_back(tile);
		CHECK(CountBits(BitsAt(f.block.layout, tile.x, tile.y)) == 2);
	}
	for (const JunctionTrack &jt : f.block.layout.tracks) {
		if (std::ranges::any_of(nodes, [&](TileIndexDiffC t) { return t.x == jt.offset.x && t.y == jt.offset.y; })) continue;
		const int count = CountBits(jt.tracks);
		if (count == 1) continue;
		REQUIRE(count == 2);
		TrackBits a, b;
		for (Track track : jt.tracks) (a.None() ? a : b).Set(track);
		CHECK(JunctionTracksMayShareTile(a, b));
	}
}

TEST_CASE("block: the floors hold", "[junction][block]")
{
	BlockFixture f;
	const JunctionLayout &layout = f.block.layout;
	const int top = 0; // The routes are in layout rows.
	const int east = f.Column("E");
	const TrackBits axis{Track::X};
	std::vector<JunctionRaster> rasters;
	for (const JunctionRoute &route : f.block.routes) {
		rasters.push_back(RasterizeJunctionRoute(route));
		REQUIRE(rasters.back().error.empty());
	}
	REQUIRE(rasters.size() == 6);

	/* R1: the first two columns and the last two are plain axis tiles on the ports' lines. */
	for (const JunctionTrack &jt : layout.tracks) {
		if (jt.offset.x <= 1 || jt.offset.x >= east - 1) CHECK(jt.tracks == axis);
	}
	CHECK(std::ranges::count_if(layout.tracks, [](const JunctionTrack &jt) { return jt.offset.x == 0; }) == 4);
	CHECK(std::ranges::count_if(layout.tracks, [&](const JunctionTrack &jt) { return jt.offset.x == east; }) == 2);

	/* R2, R3: a plain axis tile before the near head and after the far head of cA1's bridge. */
	const std::vector<JunctionPiece> &ca1 = rasters[2].pieces;
	const auto near_it = std::ranges::find_if(ca1, [](const JunctionPiece &p) { return p.on_bridge; });
	const auto far_it = std::ranges::find_if(ca1.rbegin(), ca1.rend(), [](const JunctionPiece &p) { return p.on_bridge; });
	REQUIRE(near_it != ca1.end());
	REQUIRE(far_it != ca1.rend());
	const JunctionPiece &near_head = *near_it;
	const JunctionPiece &far_head = *far_it;
	const JunctionPiece &before = *(near_it - 1);
	const JunctionPiece &after = *(far_it.base());
	for (const JunctionPiece *p : {&before, &after}) {
		CHECK(p->trackdir == Trackdir::X_SW);
		CHECK(BitsAt(layout, p->tile.x, p->tile.y - top) == axis);
	}

	/* R4: the pieces of cB0 on the bridge's line lie between the heads and keep their gap from each. */
	int on_line = 0;
	for (const JunctionPiece &p : rasters[3].pieces) {
		if (p.tile.y != near_head.tile.y) continue;
		on_line++;
		TrackBits bits;
		bits.Set(TrackdirToTrack(p.trackdir));
		const JunctionTileGap near_gap = JunctionUnrelatedTileGap(axis, bits);
		const JunctionTileGap far_gap = JunctionUnrelatedTileGap(bits, axis);
		CHECK(p.tile.x - near_head.tile.x >= (near_gap.share ? 0 : near_gap.apart));
		CHECK(far_head.tile.x - p.tile.x >= (far_gap.share ? 0 : far_gap.apart));
		CHECK(p.tile.x > near_head.tile.x);
		CHECK(p.tile.x < far_head.tile.x);
	}
	CHECK(on_line > 0);

	/* R5: every non-through path's bay, counted on its pieces, is at least two train lengths. */
	for (size_t i = 0; i < f.program.paths.size(); i++) {
		if (f.program.through[i]) continue;
		const std::vector<JunctionPiece> &pieces = rasters[i].pieces;
		auto piece_at = [&](TileIndexDiffC tile) {
			const auto it = std::ranges::find_if(pieces, [&](const JunctionPiece &p) { return p.tile.x == tile.x && p.tile.y - top == tile.y; });
			REQUIRE(it != pieces.end());
			return static_cast<size_t>(it - pieces.begin());
		};
		std::optional<size_t> split;
		const std::vector<uint16_t> &path = f.program.paths[i];
		for (size_t j = 0; j + 1 < path.size(); j++) {
			const uint16_t node = f.program.edges[path[j]].to.index;
			const size_t index = piece_at(f.NodeTile(node));
			if (f.program.nodes[node].split) {
				split = index;
				continue;
			}
			REQUIRE(split.has_value());
			size_t end = index - 1;
			while (pieces[end].on_bridge || CountBits(BitsAt(layout, pieces[end].tile.x, pieces[end].tile.y - top)) != 1) end--;
			REQUIRE(end > *split);
			uint units = 0;
			for (size_t k = *split + 1; k <= end; k++) units += pieces[k].units;
			INFO("path " << i << " merge " << node);
			CHECK(units >= 2u * f.spec.tl);
		}
	}

	/* R8: the curve gaps hold on each route's bends. */
	for (size_t i = 0; i < rasters.size(); i++) {
		const std::vector<JunctionPiece> &pieces = rasters[i].pieces;
		std::vector<JunctionCurveHand> hands;
		std::vector<int> positions;
		int units = pieces[0].units;
		for (size_t k = 1; k < pieces.size(); k++) {
			const Direction before_dir = TrackdirToDirection(pieces[k - 1].trackdir);
			const Direction dir = TrackdirToDirection(pieces[k].trackdir);
			if (dir != before_dir) {
				hands.push_back(ChangeDir(before_dir, DirDiff::Left45) == dir ? JunctionCurveHand::Left : JunctionCurveHand::Right);
				positions.push_back(units);
			}
			units += pieces[k].units;
		}
		INFO("route " << i);
		CHECK(JunctionCurveGapsHold(JunctionCurveGaps(hands, f.spec.tl, f.spec.curve_speed, f.spec.reference_speed), positions));
	}

	/* R9: the prio rows hold no track, and the fixed tracks run straight through the prio's coverage. */
	for (const JunctionRaster &raster : rasters) {
		for (const JunctionPiece &p : raster.pieces) {
			CHECK(p.tile.y >= 1);
			CHECK(p.tile.y <= 6);
		}
	}
	CHECK(f.block.routes[0].moves.find_first_not_of('F') == std::string::npos);
	CHECK(f.block.routes[5].moves.substr(0, f.Column("mL1")).find_first_not_of('F') == std::string::npos); // fB runs straight to its merge; L1 then fans in to line 2.
	for (const auto &[name, line] : {std::pair<std::string_view, int>{"mL0", 1}, std::pair<std::string_view, int>{"mL1", 6}}) {
		const int merge = f.Column(name);
		CHECK(merge >= f.EvenPrio() + 2);
		for (int x = merge - f.EvenPrio(); x < merge; x++) CHECK(CountBits(BitsAt(layout, x, line - f.Top())) == 1);
	}
}

/** The block with the signal pass run on it. */
struct SignalledBlock : BlockFixture {
	std::string why;
	std::vector<std::pair<TileIndexDiffC, TileIndexDiffC>> beside;
	std::vector<JunctionTrackSignal> signals;

	SignalledBlock()
	{
		signals = SignalJunctionTrackRoutes(block.layout, block.track_routes, block.topology, block.paths, block.graph, block.programs, spec, &why, &beside, &block.prio);
	}
};

TEST_CASE("block: the 227 join block signals and prios", "[junction][block]")
{
	SignalledBlock f;
	INFO(f.why);
	CHECK(f.why.empty());
	REQUIRE(f.block.track_routes.size() == 6);
	for (size_t r = 0; r < f.block.track_routes.size(); r++) {
		INFO("route " << r);
		CHECK(std::ranges::any_of(f.signals, [&](const JunctionTrackSignal &signal) { return signal.route == r; }));
	}
	REQUIRE(f.block.prio.size() == 2);
	for (const JunctionPrioSpan &span : f.block.prio) {
		CHECK(!span.tiles.empty());
		for (const auto &[tile, bits] : span.tiles) {
			INFO("tile " << tile.x << "," << tile.y);
			CHECK(BitsAt(f.block.layout, tile.x, tile.y).Any());
			CHECK((BitsAt(f.block.layout, tile.x, tile.y) & bits) == bits);
		}
	}
}

TEST_CASE("block: the 227 join block is checker clean", "[junction][block]")
{
	SignalledBlock f;
	const std::vector<RuleViolation> violations = CheckJunctionRules(f.block.layout, f.spec, f.topology);
	for (const RuleViolation &v : violations) UNSCOPED_INFO("rule " << static_cast<int>(v.rule) << " at " << v.tile.x << "," << v.tile.y << ": " << v.message);
	CHECK(violations.empty());
}

TEST_CASE("block: graph matches the routes", "[junction][block]")
{
	BlockFixture f;
	REQUIRE(f.block.track_routes.size() == f.block.paths.size());
	for (const JunctionTrackRoute &route : f.block.track_routes) {
		const std::vector<uint32_t> &path_edges = f.block.graph.path_edges[route.path];
		REQUIRE(route.tags.size() == route.pieces.size());
		size_t at = 0;
		for (const JunctionTrackPieceTag &tag : route.tags) {
			const auto it = std::ranges::find(path_edges.begin() + at, path_edges.end(), tag.edge);
			INFO("path " << route.path << " edge " << tag.edge);
			REQUIRE(it != path_edges.end());
			at = it - path_edges.begin();
		}
		CHECK(at + 1 == path_edges.size());
	}
	const std::vector<std::pair<uint32_t, uint32_t>> expected{{4, 7}};
	CHECK(f.block.graph.crossings == expected);
}

/** The exit block, built and signalled. */
struct ExitBlockFixture {
	JunctionSpec spec = MakeTestJunctionSpec();
	JunctionBlockProgram program = JunctionExitBlockProgram();
	JunctionTopology topology = JunctionExitBlockTopology();
	JunctionBlock block;
	std::string error;
	std::string why;
	std::vector<std::pair<TileIndexDiffC, TileIndexDiffC>> beside;
	std::vector<JunctionTrackSignal> signals;

	ExitBlockFixture()
	{
		const bool built = BuildJunctionBlock(program, spec, block, &error);
		INFO(error);
		REQUIRE(built);
		signals = SignalJunctionTrackRoutes(block.layout, block.track_routes, block.topology, block.paths, block.graph, block.programs, spec, &why, &beside, &block.prio);
	}
};

TEST_CASE("block: the exit block is checker clean", "[junction][block]")
{
	ExitBlockFixture f;
	INFO(f.why);
	CHECK(f.why.empty());
	const std::vector<RuleViolation> violations = CheckJunctionRules(f.block.layout, f.spec, f.topology);
	for (const RuleViolation &v : violations) UNSCOPED_INFO("rule " << static_cast<int>(v.rule) << " at " << v.tile.x << "," << v.tile.y << ": " << v.message);
	CHECK(violations.empty());
}

TEST_CASE("block: the exit block reaches exactly its four movements", "[junction][block]")
{
	ExitBlockFixture f;
	const std::vector<JunctionPort> ports = FindJunctionPorts(f.block.layout, f.topology);
	CHECK(ports.size() == 6);
	CHECK(std::ranges::count_if(ports, [&](const JunctionPort &p) { return p.tile.x == 0; }) == 2);
	CHECK(std::ranges::count_if(ports, [&](const JunctionPort &p) { return p.tile.x == f.block.layout.size.x - 1; }) == 4);
	const JunctionCheckContext ctx = MakeJunctionTopologyCheckContext(f.block.layout, f.spec, f.topology, ports);
	CHECK(ctx.lanes.size() == 4);
	CHECK(CheckConnectivity(ctx).empty());
	CHECK(ctx.paths.size() == 5); // l0 to l0b has one path per copy.
}

TEST_CASE("block: a doubled edge's copies", "[junction][block]")
{
	ExitBlockFixture f;
	REQUIRE(f.block.paths.size() == 4);
	std::vector<size_t> copies; // The routes of l0 to l0b (path 1), copy 0 then copy 1.
	for (size_t r = 0; r < f.block.track_routes.size(); r++) {
		if (f.block.track_routes[r].path == 1) copies.push_back(r);
	}
	REQUIRE(copies.size() == 2);
	CHECK(f.block.track_routes.size() == 5);
	CHECK(f.block.track_routes[copies[0]].copy == 0);
	CHECK(f.block.track_routes[copies[1]].copy == 1);

	auto units = [&](size_t r) {
		uint total = 0;
		for (const JunctionPiece &piece : f.block.track_routes[r].pieces) total += piece.units;
		return total;
	};
	CHECK(units(copies[0]) == units(copies[1]));

	for (size_t r : copies) {
		INFO("route " << r);
		const std::vector<std::pair<bool, size_t>> events = JunctionRouteEvents(f.block.graph, f.block.programs, f.block.track_routes, r);
		CHECK(std::ranges::count_if(events, [](const std::pair<bool, size_t> &e) { return e.first; }) == 2); // s0 and the lane split.
		CHECK(std::ranges::count_if(events, [](const std::pair<bool, size_t> &e) { return !e.first; }) == 1); // The lane merge.
	}

	/* R4 per deck: the heads hug l1a's pieces on their own line. */
	const uint32_t l1a = f.block.graph.path_edges[2].back();
	const JunctionTrackRoute &l1a_route = f.block.track_routes[std::ranges::find_if(f.block.track_routes, [](const JunctionTrackRoute &r) { return r.path == 2; }) - f.block.track_routes.begin()];
	const TrackBits axis{Track::X};
	for (size_t r : copies) {
		const std::vector<JunctionPiece> &pieces = f.block.track_routes[r].pieces;
		const auto near_it = std::ranges::find_if(pieces, [](const JunctionPiece &p) { return p.on_bridge; });
		const auto far_it = std::ranges::find_if(pieces.rbegin(), pieces.rend(), [](const JunctionPiece &p) { return p.on_bridge; });
		REQUIRE(near_it != pieces.end());
		REQUIRE(far_it != pieces.rend());
		int hugged = 0;
		for (size_t i = 0; i < l1a_route.pieces.size(); i++) {
			const JunctionPiece &p = l1a_route.pieces[i];
			if (l1a_route.tags[i].edge != l1a || p.on_bridge || p.tile.y != near_it->tile.y) continue;
			hugged++;
			TrackBits bits;
			bits.Set(TrackdirToTrack(p.trackdir));
			const JunctionTileGap near_gap = JunctionUnrelatedTileGap(axis, bits);
			const JunctionTileGap far_gap = JunctionUnrelatedTileGap(bits, axis);
			CHECK(p.tile.x - near_it->tile.x >= (near_gap.share ? 0 : near_gap.apart));
			CHECK(far_it->tile.x - p.tile.x >= (far_gap.share ? 0 : far_gap.apart));
		}
		CHECK(hugged > 0);
	}
}

TEST_CASE("block: an oriented block's routes rasterise to its oriented tiles", "[junction][block]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	for (const JunctionBlockProgram &program : {JunctionJoinBlock227Program(), JunctionExitBlockProgram()}) {
		JunctionBlock block;
		std::string error;
		REQUIRE(BuildJunctionBlock(program, spec, block, &error));
		INFO(error);
		for (uint8_t heading = 0; heading < 4; heading++) {
			for (bool mirrored : {false, true}) {
				INFO("heading " << static_cast<int>(heading) << " mirrored " << mirrored);
				const JunctionOrientedBlock oriented = OrientJunctionBlock(block, heading, mirrored);
				REQUIRE(oriented.routes.size() == block.routes.size());

				std::map<std::pair<int, int>, int> from_routes;
				for (const JunctionRoute &route : oriented.routes) {
					const JunctionRaster raster = RasterizeJunctionRoute({{static_cast<int16_t>(route.start.x + 40), static_cast<int16_t>(route.start.y + 40)}, route.start_trackdir, route.moves});
					REQUIRE(raster.error.empty());
					for (const JunctionPiece &piece : raster.pieces) {
						if (piece.on_bridge) continue;
						from_routes[{piece.tile.x - 40, piece.tile.y - 40}] |= 1 << to_underlying(TrackdirToTrack(piece.trackdir));
					}
				}
				std::map<std::pair<int, int>, int> from_tiles;
				for (const auto &[tile, bits] : oriented.tiles) {
					int mask = 0;
					for (Track track : bits) mask |= 1 << to_underlying(track);
					from_tiles[{tile.x, tile.y}] |= mask;
				}
				CHECK(from_routes == from_tiles);

				if (heading == 1) {
					const std::vector<uint8_t> &first = block.in_bundles.front();
					const int16_t anchor_line = block.in_ports[mirrored ? first.back() : first.front()].tile.y;
					std::map<std::pair<int, int>, int> expected;
					for (const JunctionTrack &jt : block.layout.tracks) {
						int mask = 0;
						for (Track track : jt.tracks) mask |= 1 << to_underlying(mirrored ? TransformTrack(TransformTrack(TransformTrack(track, JunctionTransform::Mirror), JunctionTransform::Rotate), JunctionTransform::Rotate) : track);
						const int dy = jt.offset.y - anchor_line;
						expected[{jt.offset.x, mirrored ? -dy : dy}] |= mask;
					}
					CHECK(from_tiles == expected);
				}

				/* The ports of each bundle are in track order, left to right of travel. */
				static const TileIndexDiffC rights[] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}}; // R(h): right of travel for N, E, S, W.
				const TileIndexDiffC right = rights[heading];
				for (const auto *bundles : {&oriented.in_bundles, &oriented.out_bundles}) {
					for (const auto &bundle : *bundles) {
						for (size_t k = 1; k < bundle.size(); k++) {
							const int across = (bundle[k].tile.x - bundle[k - 1].tile.x) * right.x + (bundle[k].tile.y - bundle[k - 1].tile.y) * right.y;
							CHECK(across > 0);
						}
					}
				}
				CHECK(oriented.in_bundles.front().front().tile.x == 0);
				CHECK(oriented.in_bundles.front().front().tile.y == 0);
				CHECK(oriented.length == block.out_ports.front().tile.x);
			}
		}
	}
}

TEST_CASE("block: the 227 join block's lanes leave side by side", "[junction][block]")
{
	BlockFixture f;
	REQUIRE(f.block.out_ports.size() == 2);
	CHECK(f.block.out_ports[0].tile.y == 1);
	CHECK(f.block.out_ports[1].tile.y == 2);
}

TEST_CASE("block: dump", "[junction][block][.]")
{
	SignalledBlock f;
	std::ofstream out("/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/b2b-block.txt");
	for (size_t i = 0; i < f.program.points.size(); i++) out << f.program.points[i] << "=" << f.block.columns[i] << " ";
	out << "\nsize " << f.block.layout.size.x << " x " << f.block.layout.size.y << "\n";
	out << RenderJunctionAscii(f.block.layout);

	ExitBlockFixture exit_block;
	std::ofstream exit_out("/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/b3-exit.txt");
	for (size_t i = 0; i < exit_block.program.points.size(); i++) exit_out << exit_block.program.points[i] << "=" << exit_block.block.columns[i] << " ";
	exit_out << "\nsize " << exit_block.block.layout.size.x << " x " << exit_block.block.layout.size.y << "\nwhy: " << exit_block.why << "\n";
	exit_out << RenderJunctionAscii(exit_block.block.layout);
}

TEST_CASE("block: oriented prio spans", "[junction][block]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	JunctionBlock block;
	std::string error;
	REQUIRE(BuildJunctionBlock(JunctionJoinBlock227Program(), spec, block, &error));
	REQUIRE(block.prio.size() == 2);
	for (uint8_t heading = 0; heading < 4; heading++) {
		for (bool mirrored : {false, true}) {
			INFO("heading " << static_cast<int>(heading) << " mirrored " << mirrored);
			const JunctionOrientedBlock oriented = OrientJunctionBlock(block, heading, mirrored);
			REQUIRE(oriented.prio.size() == block.prio.size());
			size_t tiles = 0;
			std::map<std::pair<int, int>, int> in_prio_tiles;
			for (const auto &[tile, bits] : oriented.prio_tiles) {
				int mask = 0;
				for (Track track : bits) mask |= 1 << to_underlying(track);
				in_prio_tiles[{tile.x, tile.y}] |= mask;
			}
			for (size_t s = 0; s < block.prio.size(); s++) {
				REQUIRE(oriented.prio[s].tiles.size() == block.prio[s].tiles.size());
				tiles += oriented.prio[s].tiles.size();
				for (const auto &[tile, bits] : oriented.prio[s].tiles) {
					int mask = 0;
					for (Track track : bits) mask |= 1 << to_underlying(track);
					CHECK((in_prio_tiles[{tile.x, tile.y}] & mask) == mask);
				}
			}
			size_t all = 0;
			for (const JunctionPrioSpan &span : block.prio) all += span.tiles.size();
			CHECK(tiles == all);
			CHECK(oriented.prio_tiles.size() == all);

			if (heading == 1 && !mirrored) {
				const std::vector<uint8_t> &first = block.in_bundles.front();
				const TileIndexDiffC anchor = block.in_ports[first.front()].tile;
				for (size_t s = 0; s < block.prio.size(); s++) {
					CHECK(oriented.prio[s].merge.x == block.prio[s].merge.x - anchor.x);
					CHECK(oriented.prio[s].merge.y == block.prio[s].merge.y - anchor.y);
					for (size_t i = 0; i < block.prio[s].tiles.size(); i++) {
						CHECK(oriented.prio[s].tiles[i].first.x == block.prio[s].tiles[i].first.x - anchor.x);
						CHECK(oriented.prio[s].tiles[i].first.y == block.prio[s].tiles[i].first.y - anchor.y);
						CHECK(oriented.prio[s].tiles[i].second == block.prio[s].tiles[i].second);
					}
				}
			}
		}
	}
}
