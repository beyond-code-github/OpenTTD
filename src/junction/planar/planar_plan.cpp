/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file planar_plan.cpp The planar model's back end (DESIGN-planar §3, §4). */

#include "../../stdafx.h"

#include "planar_plan.h"

#include "../junction_spec.h"
#include "../placement/junction_levels.h"
#include "../terrain/junction_facts.h"

#include "../../3rdparty/fmt/format.h"
#include "../../track_func.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <map>
#include <mutex>


#include "../../safeguards.h"

namespace {

/** The cache key of a seed: the text and the spec's fields the program reads. */
std::string SeedKey(const std::string &text, const JunctionSpec &spec)
{
	const Money bridge1 = spec.prices.bridge ? spec.prices.bridge(1) : Money(0);
	const Money bridge2 = spec.prices.bridge ? spec.prices.bridge(2) : Money(0);
	return fmt::format("{}|{},{},{},{},{},{},{},{},{},{},{},{}", text, spec.ml_gap, spec.branch_gap, spec.tl, spec.prio_tiles, spec.reference_speed, spec.curve_speed, spec.curve_limit, spec.max_bridge_middle,
		static_cast<int64_t>(spec.prices.rail_piece), static_cast<int64_t>(bridge1), static_cast<int64_t>(bridge2), spec.sd);
}

/** The trackdir of an axis heading (canonical frame: E = +x = X_SW, S = +y = Y_SE). */
Trackdir AxisTrackdir(int heading)
{
	switch (heading) {
		case 2: return Trackdir::X_SW;
		case 4: return Trackdir::Y_SE;
		case 6: return Trackdir::X_NE;
		default: return Trackdir::Y_NW;
	}
}

/** Every route path in route order: (route, path index within the route). */
std::vector<std::pair<uint32_t, uint32_t>> RoutePaths(const PlanarTopology &topology)
{
	std::vector<std::pair<uint32_t, uint32_t>> out;
	for (uint32_t r = 0; r < topology.routes.size(); r++) {
		for (uint32_t p = 0; p < topology.routes[r].paths.size(); p++) out.emplace_back(r, p);
	}
	return out;
}

/** A port's lines in reading order (W, E: north to south; N, S: west to east). */
std::vector<uint32_t> ReadingOrder(const PlanarPort &port)
{
	static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
	static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
	const bool along_x = port.side == PlanarSide::N || port.side == PlanarSide::S;
	const int h = static_cast<int>(port.heading);
	const int right = along_x ? -dy[h] : dx[h];
	std::vector<uint32_t> lines = port.lines;
	if (right < 0) std::ranges::reverse(lines);
	return lines;
}

}

std::shared_ptr<const PlanarSeed> PlanarSeedFor(const std::string &text, const JunctionSpec &spec)
{
	static std::mutex mutex;
	static std::map<std::string, std::shared_ptr<const PlanarSeed>> cache;
	const std::string key = SeedKey(text, spec);
	{
		std::lock_guard<std::mutex> lock(mutex);
		const auto it = cache.find(key);
		if (it != cache.end()) return it->second;
	}
	auto seed = std::make_shared<PlanarSeed>();
	seed->text = text;
	const auto start = std::chrono::steady_clock::now();
	seed->topology = ParsePlanarTopology(text, &seed->why);
	if (seed->why.empty()) {
		/* The box bound S: the smallest of 64, 128 tiles that solves (the big-M values follow it). */
		for (int bound : {64, 128}) {
			PlanarSolveOptions options;
			options.bound = bound;
			seed->why.clear();
			seed->bound = bound;
			if (SolvePlanarLayout(seed->topology, spec, options, nullptr, seed->state, seed->geometry, &seed->why)) break;
		}
	}
	seed->micros = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
	std::lock_guard<std::mutex> lock(mutex);
	return cache.emplace(key, std::move(seed)).first->second;
}

PlanarRender RenderPlanarGeometry(const PlanarTopology &topology, const PlanarGeometry &geometry)
{
	PlanarRender out;
	const std::vector<std::vector<PlanarPiece>> raster = RasterPlanarLines(geometry);
	out.layout.size = {static_cast<int16_t>(geometry.width), static_cast<int16_t>(geometry.height)};
	const auto paths = RoutePaths(topology);
	for (uint16_t index = 0; index < paths.size(); index++) {
		const auto [r, p] = paths[index];
		const PlanarRoute &route = topology.routes[r];
		const std::vector<uint32_t> &path = route.paths[p];
		std::vector<PlanarPiece> pieces;
		std::vector<uint32_t> line_of;
		std::vector<size_t> first_piece; // per path line, its first piece index
		for (uint32_t l : path) {
			first_piece.push_back(pieces.size());
			pieces.insert(pieces.end(), raster[l].begin(), raster[l].end());
			line_of.insert(line_of.end(), raster[l].size(), l);
		}
		first_piece.push_back(pieces.size());
		if (pieces.empty() || pieces.front().heading % 2 != 0) {
			out.why = fmt::format("route {} does not start on an axis piece", route.name);
			return out;
		}
		/* Moves: F, L or R per piece against the previous one; a deck from its near head to its far head is B<middle>. */
		JunctionRoute program{pieces.front().tile, AxisTrackdir(pieces.front().heading), {}};
		for (size_t i = 1; i < pieces.size(); i++) {
			if (pieces[i].kind == PlanarPiece::Kind::Head) {
				size_t far = i + 1;
				while (far < pieces.size() && pieces[far].kind == PlanarPiece::Kind::Middle) far++;
				if (far >= pieces.size() || pieces[far].kind != PlanarPiece::Kind::Head) {
					out.why = fmt::format("route {} has a deck without a far head", route.name);
					return out;
				}
				program.moves += fmt::format("B{}", far - i - 1);
				i = far;
				continue;
			}
			const int turn = (pieces[i].heading - pieces[i - 1].heading + 8) % 8;
			program.moves += turn == 0 ? 'F' : turn == 1 ? 'R' : 'L';
		}
		const JunctionRaster rendered = RasterizeJunctionRoute(program);
		if (!rendered.error.empty() || rendered.pieces.size() != pieces.size()) {
			out.why = fmt::format("route {} does not rasterise: {}", route.name, rendered.error.empty() ? "piece count" : rendered.error);
			return out;
		}
		JunctionTrackRoute track{index, 0, rendered.pieces, {}};
		for (size_t i = 0; i < pieces.size(); i++) {
			if (rendered.pieces[i].tile.x != pieces[i].tile.x || rendered.pieces[i].tile.y != pieces[i].tile.y) {
				out.why = fmt::format("route {} piece {} renders at {},{}, planned {},{}", route.name, i, rendered.pieces[i].tile.x, rendered.pieces[i].tile.y, pieces[i].tile.x, pieces[i].tile.y);
				return out;
			}
			track.tags.push_back({line_of[i], JunctionTrackPieceTag::RUN, 0, 0});
		}
		AddRasterToLayout(out.layout, rendered);

		/* The signal pass's view (D301: the fixed route holds prio; a joiner is a route of another letter). */
		JunctionSignalRoute signal;
		signal.main = route.prio;
		signal.group = static_cast<uint32_t>(route.name.front());
		for (size_t i = 0; i + 1 < path.size(); i++) {
			const uint32_t node = topology.lines[path[i]].to.index;
			const size_t last = first_piece[i + 1] - 1;
			signal.arrivals.emplace_back(node, last);
			if (topology.nodes[node].IsSplit()) {
				signal.switches.emplace_back(true, first_piece[i + 1]);
			} else {
				signal.switches.emplace_back(false, last);
				signal.merge_nodes.emplace_back(last, node);
			}
		}
		out.network.routes.push_back(std::move(signal));
		out.routes.push_back(std::move(track));
	}
	/* Node tiles: a split's is its legs' first piece, a merge's their last; port tiles: a port line's piece at the box edge. */
	for (const PlanarNode &node : topology.nodes) {
		const uint32_t leg = node.IsSplit() ? node.out[0] : node.in[0];
		out.node_tiles.push_back(node.IsSplit() ? raster[leg].front().tile : raster[leg].back().tile);
	}
	for (const PlanarPort &port : topology.ports) {
		for (uint32_t l : port.lines) out.port_tiles.push_back(port.in ? raster[l].front().tile : raster[l].back().tile);
	}
	return out;
}

PlanarPlan PlanPlanarJunction(const PlanarSeed &seed, const JunctionSpec &spec, const JunctionTerrain *terrain, TileIndexDiffC offset, bool hover)
{
	PlanarPlan plan;
	if (!seed.why.empty()) {
		plan.why = seed.why;
		return plan;
	}
	PlanarGeometry solved;
	const PlanarGeometry *geometry = &seed.geometry;
	std::optional<JunctionFacts> facts;
	if (terrain != nullptr) {
		facts.emplace(*terrain);
		PlanarSolveOptions options;
		options.bound = seed.bound;
		options.facts = &*facts;
		options.offset = offset;
		options.mode = hover ? PlanarSolveOptions::Mode::Fixed : PlanarSolveOptions::Mode::Free;
		PlanarSolveState state;
		if (!SolvePlanarLayout(seed.topology, spec, options, &seed.state, state, solved, &plan.why)) return plan;
		geometry = &solved;
	}
	PlanarRender render = RenderPlanarGeometry(seed.topology, *geometry);
	if (!render.why.empty()) {
		plan.why = render.why;
		return plan;
	}
	JunctionLayout layout = render.layout;
	std::vector<std::pair<TileIndexDiffC, TileIndexDiffC>> prio_beside;
	std::string why;
	SignalJunctionTrackRoutes(layout, render.routes, render.network, spec, &why, &prio_beside);
	if (!why.empty()) {
		plan.why = why;
		return plan;
	}
	if (terrain != nullptr) {
		if (!SolveJunctionLevels(*terrain, *facts, offset, JunctionRampLevels(spec), spec.tl, render.node_tiles, render.port_tiles, layout, render.routes, prio_beside, plan.terraform, plan.height_cost, plan.track_levels, &why)) {
			plan.why = why;
			return plan;
		}
	}
	plan.layout = std::move(layout);
	plan.routes = std::move(render.routes);
	return plan;
}

JunctionHoverPlan PlanPlanarHoverNear(const PlanarSeed &seed, const JunctionSpec &spec, const JunctionTerrain &terrain, TileIndexDiffC offset, const std::atomic<bool> *cancel)
{
	const auto at = [&](TileIndexDiffC o) {
		JunctionHoverPlan hover;
		PlanarPlan plan = PlanPlanarJunction(seed, spec, &terrain, o, true);
		hover.why = plan.why;
		hover.layout = std::move(plan.layout);
		hover.terraform = std::move(plan.terraform);
		hover.height_cost = plan.height_cost;
		hover.track_levels = std::move(plan.track_levels);
		return hover;
	};
	const auto cancelled = []() {
		JunctionHoverPlan plan;
		plan.why = "cancelled";
		return plan;
	};
	if (cancel != nullptr && cancel->load()) return cancelled();
	JunctionHoverPlan cursor = at(offset);
	if (cursor.layout.has_value()) return cursor;
	std::vector<TileIndexDiffC> moves;
	for (int y = -JUNCTION_HOVER_NEAR_ROWS; y <= JUNCTION_HOVER_NEAR_ROWS; y++) {
		for (int x = -JUNCTION_HOVER_NEAR_COLUMNS; x <= JUNCTION_HOVER_NEAR_COLUMNS; x++) {
			if (x != 0 || y != 0) moves.push_back({static_cast<int16_t>(x), static_cast<int16_t>(y)});
		}
	}
	std::ranges::sort(moves, [](const TileIndexDiffC &a, const TileIndexDiffC &b) {
		const int da = std::abs(a.x) + std::abs(a.y), db = std::abs(b.x) + std::abs(b.y);
		if (da != db) return da < db;
		if (std::abs(a.y) != std::abs(b.y)) return std::abs(a.y) < std::abs(b.y);
		if (a.x != b.x) return a.x < b.x;
		return a.y < b.y;
	});
	for (const TileIndexDiffC &move : moves) {
		if (cancel != nullptr && cancel->load()) return cancelled();
		JunctionHoverPlan plan = at({static_cast<int16_t>(offset.x + move.x), static_cast<int16_t>(offset.y + move.y)});
		if (!plan.layout.has_value()) continue;
		plan.moved = move;
		return plan;
	}
	return cursor;
}

JunctionTopology PlanarCheckerTopology(const PlanarTopology &topology)
{
	JunctionTopology out;
	/* Groups in slot order: W ports in reading order, then S, then E and N reversed (JunctionPortSlots). */
	std::vector<uint32_t> order;
	for (PlanarSide side : {PlanarSide::W, PlanarSide::S, PlanarSide::E, PlanarSide::N}) {
		std::vector<uint32_t> ports;
		for (uint32_t p = 0; p < topology.ports.size(); p++) {
			if (topology.ports[p].side == side) ports.push_back(p);
		}
		if (side == PlanarSide::E || side == PlanarSide::N) std::ranges::reverse(ports);
		order.insert(order.end(), ports.begin(), ports.end());
	}
	std::vector<uint8_t> group_of(topology.ports.size());
	for (uint32_t p : order) {
		const PlanarPort &port = topology.ports[p];
		const bool main = port.side == PlanarSide::W || port.side == PlanarSide::E;
		group_of[p] = static_cast<uint8_t>(out.groups.size());
		out.groups.push_back({static_cast<JunctionSide>(port.side), port.in, static_cast<uint8_t>(port.lines.size()), static_cast<uint8_t>(main ? 0 : 1), main});
	}
	const std::vector<JunctionPortSlot> slots = JunctionPortSlots(out);
	const auto slot_of = [&](uint32_t line, bool entry) {
		const PlanarEnd end = entry ? topology.lines[line].from : topology.lines[line].to;
		const std::vector<uint32_t> reading = ReadingOrder(topology.ports[end.index]);
		const uint8_t lane = static_cast<uint8_t>(std::ranges::find(reading, line) - reading.begin());
		for (uint16_t s = 0; s < slots.size(); s++) {
			if (slots[s].group == group_of[end.index] && slots[s].lane == lane) return s;
		}
		return uint16_t{0};
	};
	for (const auto &[r, p] : RoutePaths(topology)) {
		const auto &path = topology.routes[r].paths[p];
		const uint8_t from = group_of[topology.lines[path.front()].from.index], to = group_of[topology.lines[path.back()].to.index];
		auto it = std::ranges::find(out.movements, std::pair<uint8_t, uint8_t>(from, to));
		if (it == out.movements.end()) {
			out.movements.emplace_back(from, to);
			it = out.movements.end() - 1;
		}
		out.lane_paths.push_back({slot_of(path.front(), true), slot_of(path.back(), false), static_cast<uint16_t>(it - out.movements.begin()), topology.routes[r].prio});
	}
	return out;
}

std::string PlanarHubText(uint8_t ml_lanes, uint8_t branch_lanes, JunctionLanePairing pairing)
{
	if (ml_lanes != branch_lanes) return {};
	if (ml_lanes == 1) return PlanarTText(1);
	if (ml_lanes == 2) return pairing == JunctionLanePairing::AnyToAny ? PlanarLL2Text() : PlanarTText(2);
	return {};
}

std::string PlanarLL2Text()
{
	/*
	 * The PSG 227 pattern at two lanes (D287: his graph, our prio element), frame as the T (main W-E, branch S).
	 * Routes are the composed hub's lane paths (planar lane 1 is left of travel): per movement a fixed route holding prio (D301) and a choice route.
	 * Entry: each in lane splits by direction (the bundles fray). Exit: each choice route splits by lane, the two choice legs of a lane merge first, then join the fixed lane under its prio.
	 */
	return
		"topology LL2\n"
		"port W in E: EF1, EF2\n"
		"port W out W: WO1=C1+B1+C2, WO2=B1+B2+C2\n"
		"port E out E: EO1=E1+E2+D1, EO2=E2+D1+D2\n"
		"port E in W: AB1, AB2\n"
		"port S in N: CD1, CD2\n"
		"port S out S: SO1=A1+A2+F1, SO2=F1+F2+A2\n"
		"node EF1, EF2 -> E1, F1, E2, F2\n"
		"node AB1, AB2 -> A1, B1, A2, B2\n"
		"node CD1, CD2 -> C1, D1, C2, D2\n"
		"node E2 -> E2a, E2b\n"
		"node D1 -> D1a, D1b\n"
		"node E2a, D1a -> EDj1=E2+D1\n"
		"node E2b, D1b -> EDj2=E2+D1\n"
		"node E1, EDj1 -> EO1\n"
		"node EDj2, D2 -> EO2\n"
		"node C2 -> C2a, C2b\n"
		"node B1 -> B1a, B1b\n"
		"node C2a, B1a -> CBj1=C2+B1\n"
		"node C2b, B1b -> CBj2=C2+B1\n"
		"node C1, CBj1 -> WO1\n"
		"node CBj2, B2 -> WO2\n"
		"node A2 -> A2a, A2b\n"
		"node F1 -> F1a, F1b\n"
		"node A2a, F1a -> AFj1=A2+F1\n"
		"node A2b, F1b -> AFj2=A2+F1\n"
		"node A1, AFj1 -> SO1\n"
		"node AFj2, F2 -> SO2\n"
		"prio E1, F2, A1, B2, C1, D2\n";
}
