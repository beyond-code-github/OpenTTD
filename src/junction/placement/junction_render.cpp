/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_render.cpp Render of a placement to routes and a track-only layout (DESIGN-G1 §11.10). */

#include "../../stdafx.h"

#include "junction_render.h"

#include "../junction_spec.h"
#include "../junction_track_geometry.h"
#include "../junction_rules_internal.h"

#include "../../direction_func.h"
#include "../../map_func.h"
#include "../../track_func.h"

#include <algorithm>
#include <climits>
#include <functional>
#include <map>
#include <numeric>
#include <optional>
#include <set>
#include <tuple>

#include "../../safeguards.h"

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
					if (token.kind == StepKind::Under || token.kind == StepKind::Above) continue;
					if (token.kind == StepKind::Over || token.kind == StepKind::Tunnel) {
						const int b = bridge_over(e, token.index);
						if (b < 0) return fail_at(where, "an Over without a bridge");
						if (b == last_over) continue;
						last_over = b;
						const JunctionBridge &heads = placement.bridges[b][doubled_leg(e) ? c : 0];
						if (!structure.bridges[b].tunnel && cur.x == heads.start.x && cur.y == heads.start.y && !route.moves.empty() && (route.moves.back() == 'L' || route.moves.back() == 'R')) {
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
						route.moves += fmt::format("{}{}", structure.bridges[b].tunnel ? 'T' : 'B', middle);
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

/** Add tracks to a tile of the layout. */
void AddJunctionTracks(JunctionLayout &layout, TileIndexDiffC tile, TrackBits tracks)
{
	for (JunctionTrack &jt : layout.tracks) {
		if (jt.offset.x == tile.x && jt.offset.y == tile.y) {
			jt.tracks = jt.tracks | tracks;
			return;
		}
	}
	layout.tracks.push_back({tile, tracks});
}
