/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_generator.cpp Junction generator: spec, notation parsing and curve spacing. */

#include "stdafx.h"

#include "junction_generator.h"
#include "junction_rules.h"
#include "junction_rules_internal.h"
#include "core/math_func.hpp"
#include "junction_line_router.h"
#include "junction_route.h"
#include "junction_router.h"
#include "junction_signal_pass.h"
#include "junction_skeleton.h"
#include "junction_throughput.h"
#include "track_func.h"
#include "thread.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <tuple>

#include "safeguards.h"

/**
 * Parse a junction notation such as "L_R": left lane(s), gap, right lane(s).
 * @param text The notation.
 * @return The gap between the lanes, or why the notation is not usable.
 */
JunctionNotation ParseJunctionNotation(std::string_view text)
{
	size_t lefts = 0;
	while (lefts < text.size() && text[lefts] == 'L') lefts++;
	size_t gap = 0;
	while (lefts + gap < text.size() && text[lefts + gap] == '_') gap++;
	size_t rights = 0;
	while (lefts + gap + rights < text.size() && text[lefts + gap + rights] == 'R') rights++;

	if (lefts > 0 && rights > 0 && lefts + gap + rights == text.size()) {
		if (lefts > 1 || rights > 1) return {0, JunctionNotationError::MultiLane};
		return {static_cast<uint8_t>(gap), JunctionNotationError::None};
	}

	/* Multi-lane forms with interleaved gaps, such as L_L__R_R, are recognised but unsupported. */
	const bool only_lr_gap = text.find_first_not_of("L_R") == std::string_view::npos;
	if (only_lr_gap && !text.empty() && text.front() == 'L' && text.back() == 'R' &&
			std::ranges::count(text, 'L') > 1 && std::ranges::count(text, 'R') > 1) {
		return {0, JunctionNotationError::MultiLane};
	}
	return {0, JunctionNotationError::Invalid};
}

/**
 * The curve speed limit of a train whose curves are 'spacing' tiles apart, as Train::GetCurveSpeedLimit.
 * @param spacing The spacing in tiles; clamped to 1..12.
 * @param curve_speed The rail type's curve speed advantage.
 * @return The speed limit.
 */
uint16_t CurveSpeedLimitForSpacing(uint spacing, uint8_t curve_speed)
{
	const int clamped = Clamp<int>(spacing, 1, 12);
	int speed = 232 - (13 - clamped) * (13 - clamped);
	speed += (speed / 2) * curve_speed;
	return static_cast<uint16_t>(speed);
}

/**
 * The smallest curve spacing at which a train of the given speed is not slowed down.
 * @param speed The wanted speed.
 * @param curve_speed The rail type's curve speed advantage.
 * @param tl The train length in tiles.
 * @return The spacing in tiles, at most twice the train length.
 */
uint DeriveCurveSpacing(uint16_t speed, uint8_t curve_speed, uint8_t tl)
{
	uint spacing = 2 * tl;
	for (uint s = 1; s <= 12; s++) {
		if (CurveSpeedLimitForSpacing(s, curve_speed) >= speed) {
			spacing = s;
			break;
		}
	}
	return std::min<uint>(spacing, 2 * tl);
}

JunctionSpec MakeTestJunctionSpec()
{
	JunctionSpec spec{};
	spec.ml_gap = 1;
	spec.branch_gap = 1;
	spec.tl = 4;
	spec.prio_tiles = 14; /* Measured: SizeJunctionPrio of this spec with prio_tiles 0 (see the test "SizeJunctionPrio sizes the test spec"). */
	/* The D138 design train: Buborough's main-line consist, loaded. */
	spec.train.power_hp = 14194;
	spec.train.max_te = 1090000;
	spec.train.mass_t = 561;
	spec.train.air_drag = 25;
	spec.train.max_speed = 196;
	spec.sd = 2;
	spec.reference_speed = 160;
	spec.curve_speed = 0;
	spec.curve_limit = true;
	spec.signal_type = SignalType::Block;
	spec.bridge_span_ok = [](uint n) { return n >= 1 && n <= 10; };
	spec.prices.rail_piece = 100;
	spec.prices.signal = 50;
	spec.prices.bridge = [](uint n) { return Money(500 + 200 * n); };
	return spec;
}

namespace {

/** The tracks of the tile at (x, y) in a layout, or none. */
TrackBits TracksAt(const JunctionLayout &layout, int x, int y)
{
	TrackBits bits{};
	for (const JunctionTrack &jt : layout.tracks) {
		if (jt.offset.x == x && jt.offset.y == y) bits = bits | jt.tracks;
	}
	return bits;
}

/** Whether a tile holds exactly the axis track X. */
bool IsPlainX(const JunctionLayout &layout, int x, int y)
{
	return TracksAt(layout, x, y) == TrackBits{Track::X};
}

/** The plain pieces of a row for x in min_x..max_x, travelling in the given direction. */
std::vector<JunctionRouteEnd> PlainRowPieces(const JunctionLayout &layout, int y, Trackdir trackdir, int min_x, int max_x)
{
	std::vector<JunctionRouteEnd> ends;
	for (int x = min_x; x <= max_x; x++) {
		if (IsPlainX(layout, x, y)) ends.push_back({{static_cast<int16_t>(x), static_cast<int16_t>(y)}, trackdir});
	}
	return ends;
}

/**
 * The plain pieces of a routed raster for x in min_x..max_x, travelling in the given direction, on any row.
 * @param layout The layout the raster was added to; a piece is plain when its tile holds exactly the axis track X there.
 * @param raster The raster whose pieces are the candidates.
 * @param trackdir The direction the pieces are travelled in.
 * @param min_x The first column.
 * @param max_x The last column.
 * @return The ends, in the raster's order.
 */
static std::vector<JunctionRouteEnd> PlainRasterPieces(const JunctionLayout &layout, const JunctionRaster &raster, Trackdir trackdir, int min_x, int max_x)
{
	std::vector<JunctionRouteEnd> ends;
	for (const JunctionPiece &piece : raster.pieces) {
		if (piece.tile.x < min_x || piece.tile.x > max_x) continue;
		if (IsPlainX(layout, piece.tile.x, piece.tile.y)) ends.push_back({piece.tile, trackdir});
	}
	return ends;
}

/** Route, rasterize and add to the layout. */
std::optional<JunctionRaster> RouteAndAdd(JunctionLayout &layout, JunctionRouteRequest request, JunctionRouteCache *cache)
{
	auto clip = [&](std::vector<JunctionRouteEnd> &ends) {
		std::erase_if(ends, [&](const JunctionRouteEnd &end) {
			return end.tile.x < request.min.x || end.tile.x > request.max.x || end.tile.y < request.min.y || end.tile.y > request.max.y;
		});
	};
	clip(request.starts);
	clip(request.goals);
	const std::optional<JunctionRoute> route = FindJunctionRoute(layout, request, cache);
	if (!route.has_value()) return std::nullopt;
	JunctionRaster raster = RasterizeJunctionRoute(*route);
	if (!raster.error.empty()) return std::nullopt;
	AddRasterToLayout(layout, raster);
	return raster;
}

/** Route against a copy of the layout with blocker tracks on the given empty tiles, then add the raster to the real layout. */
std::optional<JunctionRaster> RouteFencedAndAdd(JunctionLayout &layout, JunctionRouteRequest request, const std::vector<TileIndexDiffC> &fence, JunctionRouteCache *cache)
{
	JunctionLayout fenced = layout;
	for (const TileIndexDiffC &t : fence) {
		if (TracksAt(layout, t.x, t.y).None()) fenced.tracks.push_back({t, TrackBits{Track::Y}});
	}
	std::optional<JunctionRaster> raster = RouteAndAdd(fenced, request, cache);
	if (raster.has_value()) AddRasterToLayout(layout, *raster);
	return raster;
}

/** A straight route of F moves. */
JunctionRoute StraightRoute(TileIndexDiffC start, Trackdir trackdir, int length)
{
	return {start, trackdir, std::string(length, 'F')};
}

uint RasterUnits(const JunctionRaster &raster)
{
	uint units = 0;
	for (const JunctionPiece &piece : raster.pieces) units += piece.units;
	return units;
}

} // namespace

/**
 * The frame for a spec: it grows with the prio coverage beyond one train length.
 * @param spec The junction spec.
 * @return The frame; the defaults when the coverage is at most one train length.
 */
JunctionFrame MakeJunctionFrame(const JunctionSpec &spec)
{
	const int dC = std::max(0, spec.prio_tiles - spec.tl);
	JunctionFrame frame{};
	frame.hub_x += dC;
	frame.width += 2 * dC;
	frame.reach_east += dC;
	return frame;
}

/**
 * Tiles between the lane merge and the N->W join beyond the prio coverage, in the parallelogram style (D145).
 * Pinned by the test "Parallelogram solver constants are minimal (D145)": one less leaves no clean candidate (G9c).
 */
static constexpr int JUNCTION_PRIO_AFTER_MERGE = 1;

/**
 * Tiles between the N->W join and the shift-back S-bend, in the parallelogram style (D145).
 * Pinned by the test "Parallelogram solver constants are minimal (D145)".
 */
static constexpr int JUNCTION_SHIFT_BACK_TILES = 4;

/** D148: the N->E goal window starts this far past the E->S split, beyond the prio coverage: one tile so the split lies outside the join's coverage (G9c, G9b), one because the goal piece is the plain tile after the merge tile. One more tile (e_slack 1) when the prio pass can't place non-adjacent joins over the coverage (G9b, D148a). */
static constexpr int JUNCTION_E_PRIO_AFTER_SPLIT = 2;

/**
 * Build the two lanes of the parallelogram style from the solver positions (D118, D145).
 * Lane A runs straight on the W row, through its bridge, then S-bends onto the b row; lane B S-bends onto the b row at the
 * lane split, bridges on the b row, and shifts back to the W row at the shift-back. Each count is found by rasterizing in increasing order.
 * @param pos The solved positions.
 * @param width The frame width; the lanes start at x = width - 1.
 * @param w_row The W row.
 * @param b_row The row below it.
 * @param error Receives the first unmet target when nothing is returned.
 * @return The rasters of lanes A and B, or std::nullopt.
 */
static std::optional<std::pair<JunctionRaster, JunctionRaster>> BuildParallelogramLanes(const JunctionSkeletonW &pos, int width, int w_row, int b_row, std::string *error)
{
	auto start = [&](const std::string &moves) {
		return JunctionRoute{{static_cast<int16_t>(width - 1), static_cast<int16_t>(w_row)}, Trackdir::X_NE, moves};
	};
	auto fail = [&](const std::string &what) -> std::optional<std::pair<JunctionRaster, JunctionRaster>> {
		if (error != nullptr) *error = what;
		return std::nullopt;
	};
	/* The first and last bridge piece of a raster; size() and 0 when it has none. */
	auto bridge_ends = [](const JunctionRaster &raster) {
		size_t near_head = raster.pieces.size();
		size_t far_head = 0;
		for (size_t i = 0; i < raster.pieces.size(); i++) {
			if (!raster.pieces[i].on_bridge) continue;
			near_head = std::min(near_head, i);
			far_head = i;
		}
		return std::pair{near_head, far_head};
	};
	auto at = [](const JunctionPiece &piece, int x, int y) { return piece.tile.x == x && piece.tile.y == y; };

	/* Lane A: F^k B<n> F^g RFL. */
	const std::string span_a = fmt::format("B{}", pos.x_he[0] - pos.x_hw[0] - 1);
	std::optional<int> k_a;
	for (int k = 0; k <= width - 1 && !k_a.has_value(); k++) {
		const JunctionRaster raster = RasterizeJunctionRoute(start(std::string(k, 'F') + span_a));
		if (!raster.error.empty()) continue;
		const auto [near_head, far_head] = bridge_ends(raster);
		if (near_head >= raster.pieces.size()) continue;
		if (at(raster.pieces[near_head], pos.x_he[0], w_row) && at(raster.pieces[far_head], pos.x_hw[0], w_row)) k_a = k;
	}
	if (!k_a.has_value()) return fail(fmt::format("lane A heads at x {} and {} on row {}", pos.x_he[0], pos.x_hw[0], w_row));
	std::optional<JunctionRaster> lane_a;
	for (int g = 0; g <= width - 1 && !lane_a.has_value(); g++) {
		const JunctionRaster raster = RasterizeJunctionRoute(start(std::string(*k_a, 'F') + span_a + std::string(g, 'F') + "RFL"));
		if (raster.error.empty() && at(raster.pieces.back(), pos.x_lm, b_row)) lane_a = raster;
	}
	if (!lane_a.has_value()) return fail(fmt::format("lane A last piece at x {} on row {}", pos.x_lm, b_row));

	/* Lane B: F^p RFL F^q B<n> F^t LFR F^s; q = -1 is RF then the bridge, whose near head closes the S-bend (D110). */
	const std::string span_b = fmt::format("B{}", pos.x_he[1] - pos.x_hw[1] - 1);
	std::optional<int> p_b;
	for (int p = 0; p <= width - 1 && !p_b.has_value(); p++) {
		const JunctionRaster raster = RasterizeJunctionRoute(start(std::string(p, 'F') + "RF"));
		if (raster.error.empty() && raster.pieces.size() > static_cast<size_t>(p) + 1 && raster.pieces[p + 1].tile.x == pos.x_ls) p_b = p;
	}
	if (!p_b.has_value()) return fail(fmt::format("lane B split at x {}", pos.x_ls));
	std::optional<std::string> head_moves;
	for (int q = -1; q <= width - 1 && !head_moves.has_value(); q++) {
		const std::string moves = std::string(*p_b, 'F') + (q < 0 ? "RF" : "RFL" + std::string(q, 'F')) + span_b;
		const JunctionRaster raster = RasterizeJunctionRoute(start(moves));
		if (!raster.error.empty()) continue;
		const auto [near_head, far_head] = bridge_ends(raster);
		if (near_head >= raster.pieces.size()) continue;
		if (at(raster.pieces[near_head], pos.x_he[1], b_row) && at(raster.pieces[far_head], pos.x_hw[1], b_row)) head_moves = moves;
	}
	if (!head_moves.has_value()) return fail(fmt::format("lane B heads at x {} and {} on row {}", pos.x_he[1], pos.x_hw[1], b_row));
	std::optional<std::string> to_sb;
	for (int t = 0; t <= width - 1 && !to_sb.has_value(); t++) {
		const JunctionRaster raster = RasterizeJunctionRoute(start(*head_moves + std::string(t, 'F')));
		if (raster.error.empty() && at(raster.pieces.back(), pos.x_sb + 1, b_row)) to_sb = *head_moves + std::string(t, 'F');
	}
	if (!to_sb.has_value()) return fail(fmt::format("lane B last piece before the shift-back at x {} on row {}", pos.x_sb + 1, b_row));
	std::optional<JunctionRaster> lane_b;
	for (int s = 0; s <= width - 1 && !lane_b.has_value(); s++) {
		const JunctionRaster raster = RasterizeJunctionRoute(start(*to_sb + "LFR" + std::string(s, 'F')));
		if (raster.error.empty() && at(raster.pieces.back(), 0, w_row)) lane_b = raster;
	}
	if (!lane_b.has_value()) return fail(fmt::format("lane B end at x 0 on row {}", w_row));

	/* Lane A's RFL must land on a b row tile where lane B has an axis piece. */
	const bool merges = std::ranges::any_of(lane_b->pieces, [&](const JunctionPiece &piece) {
		return !piece.on_bridge && at(piece, pos.x_lm, b_row) && IsDiagonalTrackdir(piece.trackdir);
	});
	if (!merges) return fail(fmt::format("lane B axis piece at the lane merge x {}", pos.x_lm));
	return std::pair{std::move(*lane_a), std::move(*lane_b)};
}

/**
 * Crop, check the terrain, normalise and place signals: the common end of every candidate.
 * @param fl The routed layout.
 * @param spec The junction spec.
 * @param terrain Optional terrain snapshot in the generator's working coordinates.
 * @param plane The level the junction is built on; only used with a terrain.
 * @param w_row The W row.
 * @param hub_x The x of the hub in working coordinates (the frame's hub_x).
 * @param cost_out When set together with terrain, receives the terrain cost.
 * @param hub_out When set, receives the hub offset after normalising.
 * @return The finished layout, or std::nullopt when it cannot be finished.
 */
static std::optional<JunctionLayout> FinishJunctionCandidate(JunctionLayout fl, const JunctionSpec &spec, const JunctionTerrain *terrain, int plane, int w_row, int hub_x, Money *cost_out, TileIndexDiffC *hub_out)
{
	/* Crop the E and W rows to the content. */
	int min_x = INT32_MAX;
	int max_x = INT32_MIN;
	for (const JunctionTrack &jt : fl.tracks) {
		const bool plain_row = (jt.offset.y == 0 || jt.offset.y == w_row) && jt.tracks == TrackBits{Track::X};
		if (plain_row) continue;
		min_x = std::min<int>(min_x, jt.offset.x);
		max_x = std::max<int>(max_x, jt.offset.x);
	}
	for (const JunctionBridge &bridge : fl.bridges) {
		for (const TileIndexDiffC &head : {bridge.start, bridge.end}) {
			min_x = std::min<int>(min_x, head.x);
			max_x = std::max<int>(max_x, head.x);
		}
	}
	if (max_x < min_x) return std::nullopt;
	std::erase_if(fl.tracks, [&](const JunctionTrack &jt) {
		const bool plain_row = (jt.offset.y == 0 || jt.offset.y == w_row) && jt.tracks == TrackBits{Track::X};
		return plain_row && (jt.offset.x < min_x - 2 || jt.offset.x > max_x + 2);
	});

	if (terrain != nullptr) {
		std::set<std::pair<int, int>> occupied;
		for (const JunctionTrack &jt : fl.tracks) {
			if (jt.tracks.None()) continue;
			if (!terrain->UsableAt(jt.offset.x, jt.offset.y, plane)) return std::nullopt;
			occupied.insert({jt.offset.x, jt.offset.y});
		}
		for (const JunctionBridge &bridge : fl.bridges) {
			for (const TileIndexDiffC &head : {bridge.start, bridge.end}) {
				if (!terrain->UsableAt(head.x, head.y, plane)) return std::nullopt;
				occupied.insert({head.x, head.y});
			}
			const int dx = (bridge.end.x > bridge.start.x) - (bridge.end.x < bridge.start.x);
			const int dy = (bridge.end.y > bridge.start.y) - (bridge.end.y < bridge.start.y);
			for (int x = bridge.start.x + dx, y = bridge.start.y + dy; x != bridge.end.x || y != bridge.end.y; x += dx, y += dy) {
				if (!terrain->SpanOk(x, y, plane)) return std::nullopt;
			}
		}
		if (cost_out != nullptr) {
			Money cost = 0;
			for (const auto &[x, y] : occupied) cost += terrain->CostAt(x, y, plane);
			*cost_out = cost;
		}
	}

	/* The shift NormaliseJunctionLayout applies: the minimum of the track and bridge offsets. */
	int shift_x = 0;
	int shift_y = 0;
	auto normalise = [&]() {
		int min_x = INT32_MAX;
		int min_y = INT32_MAX;
		for (const JunctionTrack &jt : fl.tracks) {
			min_x = std::min<int>(min_x, jt.offset.x);
			min_y = std::min<int>(min_y, jt.offset.y);
		}
		for (const JunctionBridge &bridge : fl.bridges) {
			for (const TileIndexDiffC &head : {bridge.start, bridge.end}) {
				min_x = std::min<int>(min_x, head.x);
				min_y = std::min<int>(min_y, head.y);
			}
		}
		if (min_x != INT32_MAX) {
			shift_x += min_x;
			shift_y += min_y;
		}
		NormaliseJunctionLayout(fl);
	};
	normalise();
	PlaceJunctionSignals(fl, spec);
	normalise();

	if (terrain != nullptr) {
		/* The prio rows exist only now; every track tile must be usable, in working coordinates. */
		for (const JunctionTrack &jt : fl.tracks) {
			if (jt.tracks.None()) continue;
			if (!terrain->UsableAt(jt.offset.x + shift_x, jt.offset.y + shift_y, plane)) return std::nullopt;
		}
	}
	if (hub_out != nullptr) *hub_out = {static_cast<int16_t>(hub_x - shift_x), static_cast<int16_t>(0 - shift_y)};
	return fl;
}

/**
 * Build one generator candidate with a fixed lane gap: route the branch movements, double the mainline bridges, crop, and place signals.
 * @param spec The junction spec.
 * @param params The swept inputs.
 * @param terrain Optional terrain snapshot in the generator's working coordinates (see the header).
 * @param terrain_cost Receives the terrain cost when terrain is set.
 * @param gap 0 puts lane A's RFL right after its far head and lane B's near head right after its S-bend; 1 adds one plain tile to each (D110).
 * @param sb_back Unused: the parallelogram style takes its positions from the skeleton solver (D145), and its parameters bridge_margin, w_slack, gap and sb_back are ignored.
 * @return The layout, or std::nullopt when this candidate cannot be built.
 */
static std::optional<JunctionLayout> BuildJunctionCandidateWithGap(const JunctionSpec &spec, const JunctionCandidateParams &params, const JunctionTerrain *terrain, Money *terrain_cost, TileIndexDiffC *hub, int gap, [[maybe_unused]] int sb_back)
{
	const JunctionFrame frame = MakeJunctionFrame(spec);
	const int g = spec.ml_gap;
	const int xI = frame.hub_x;
	const int xO = xI + spec.branch_gap + 1;
	const int yb = static_cast<int>(params.height) - 1;
	if (yb < g + 4) return std::nullopt;
	const int w_row = g + 1;
	const int b_row = g + 2;

	auto tile = [](int x, int y) { return TileIndexDiffC{static_cast<int16_t>(x), static_cast<int16_t>(y)}; };

	JunctionRouteRequest base{};
	base.min = tile(1, 0); // N->W joins as far left as x = 2 (its window shrinks with TL), so the box must reach it.
	base.max = tile(xO + frame.reach_east, yb);
	base.max_expansions = 50000 * frame.width / 64;
	base.curve_spacing = spec.curve_limit ? DeriveCurveSpacing(spec.reference_speed, spec.curve_speed, spec.tl) : 1;
	base.s_bend_window = 2u * spec.tl;
	base.bridge_span_ok = spec.bridge_span_ok;
	base.max_bridge_middle = 10;
	base.prices = spec.prices;
	base.crossable_cost = 0;
	base.turn_cost = spec.prices.rail_piece / 2;
	base.terrain = terrain;
	base.plane = params.plane;

	/*
	 * Short straight branch legs: E->S joins the top of the S stub and W->S merges into its side; N->E leaves the top of the N stub and N->W splits from its side.
	 * D115/D115a: a stub is the entry row, then, below the edge-most merge or split, the signal tile and the boundary tile.
	 */
	constexpr int STUB = 2;
	const int ys = yb - STUB;
	const JunctionRouteEnd goal_s{tile(xO, ys), Trackdir::Y_SE};
	const JunctionRouteEnd start_n{tile(xI, ys), Trackdir::Y_NW};

	auto base_layout = [&](JunctionLayout &layout) {
		AddRasterToLayout(layout, RasterizeJunctionRoute(StraightRoute(tile(0, 0), Trackdir::X_SW, frame.width - 1)));
		AddRasterToLayout(layout, RasterizeJunctionRoute(StraightRoute(tile(xO, ys), Trackdir::Y_SE, STUB)));
		AddRasterToLayout(layout, RasterizeJunctionRoute(StraightRoute(tile(xI, yb), Trackdir::Y_NW, STUB)));
	};
	auto stub_pieces = [&](int x, Trackdir td, int first_y) {
		std::vector<JunctionRouteEnd> ends;
		for (int y = first_y; y < yb; y++) ends.push_back({tile(x, y), td});
		return ends;
	};

	/* Pass 1: find the crossing zone. */
	JunctionLayout layout;
	base_layout(layout);
	AddRasterToLayout(layout, RasterizeJunctionRoute(StraightRoute(tile(frame.width - 1, w_row), Trackdir::X_NE, frame.width - 1)));
	std::vector<int> zone; ///< The x of every W row tile that a crossing movement (E->S, N->E) uses.
	std::vector<int> zone_b; ///< The same for the b row (lane B's row in the parallelogram style).
	auto note_crossings = [&](const JunctionRaster &raster) {
		for (const JunctionPiece &piece : raster.pieces) {
			if (piece.tile.y == w_row) zone.push_back(piece.tile.x);
			if (piece.tile.y == b_row) zone_b.push_back(piece.tile.x);
		}
	};
	/*
	 * Route the four movements in the order E->S, W->S, N->E, N->W.
	 * Fences: E->S may not touch column xI above row g+3, so it crosses the N column low down and N->E bridges straight
	 * over it. W->S and N->W stay on the branch side: they may not use the ML gap rows or the ground under ML bridges.
	 */
	auto route_movements = [&](JunctionLayout &l, bool crossable_w, const std::vector<TileIndexDiffC> &under_spans, int nw_max_x, int lane_b_max_x, bool note, int nw_row, int nw_min_x, int sl_x) -> bool {
		std::vector<TileIndexDiffC> w_tiles;
		if (crossable_w) for (int x = 0; x <= frame.width - 1; x++) w_tiles.push_back(tile(x, w_row));
		std::vector<TileIndexDiffC> column_fence;
		for (int y = 0; y <= g + 3; y++) column_fence.push_back(tile(xI, y));
		/* Keep the tiles above the N stub free: N->E needs them for its first piece and its bridge head (D3d). */
		column_fence.push_back(tile(xI, ys - 1));
		/* D113: N->E starts with a plain piece (it carries the signal), then its bridge head: keep the second tile free too. */
		column_fence.push_back(tile(xI, ys - 2));
		std::vector<TileIndexDiffC> branch_fence = under_spans;
		for (int y = 1; y < w_row; y++) for (int x = 0; x <= frame.width - 1; x++) branch_fence.push_back(tile(x, y));

		JunctionRouteRequest request = base;
		request.crossable = w_tiles;
		request.crossable_cost = params.crossing_cost;
		for (int x = 2; x <= xI - 2; x++) {
			if (TracksAt(l, x, 0) == TrackBits{Track::X}) request.starts.push_back({tile(x, 0), Trackdir::X_SW});
		}
		request.goals = {goal_s};
		const std::optional<JunctionRaster> e_to_s = RouteFencedAndAdd(l, request, column_fence, params.route_cache);
		if (!e_to_s.has_value()) return false;
		if (note) note_crossings(*e_to_s);

		request = base;
		if (sl_x < 0) {
			request.starts = PlainRowPieces(l, w_row, Trackdir::X_NE, xO + 2, frame.width - 3);
			for (const JunctionRouteEnd &end : PlainRowPieces(l, b_row, Trackdir::X_NE, xO + 2, lane_b_max_x)) request.starts.push_back(end);
		} else {
			/* D110: with the lanes fixed, W->S splits off the single W track first, one plain tile (the signal tile) before the lane split. */
			request.starts = PlainRowPieces(l, w_row, Trackdir::X_NE, sl_x, sl_x);
			if (sl_x < xO + 2 || request.starts.empty()) return false;
		}
		/* The W->S merge needs its goal piece on the next row (router exception 2), so its goals include the entry row ys; the N->W starts below keep ys + 1. */
		request.goals = stub_pieces(xO, Trackdir::Y_SE, ys);
		if (!RouteFencedAndAdd(l, request, branch_fence, params.route_cache).has_value()) return false;

		request = base;
		request.crossable = w_tiles;
		request.crossable_cost = params.crossing_cost;
		request.starts = {start_n};
		/* D108: the N->E merge window starts beyond the prio coverage past the E->S split; the checker rejects unsafe positions. */
		const int x_s = e_to_s->pieces.front().tile.x;
		const int cover = static_cast<int>(spec.prio_tiles);
		const int e_after = params.lane_style == 1 ? JUNCTION_E_PRIO_AFTER_SPLIT + params.e_slack + params.k_e_delta : 1 + params.e_slack;
		request.goals = PlainRowPieces(l, 0, Trackdir::X_SW, std::max(xO, x_s + cover + e_after), frame.width - 3);
		const std::optional<JunctionRaster> n_to_e = RouteAndAdd(l, request, params.route_cache);
		if (!n_to_e.has_value()) return false;
		if (note) note_crossings(*n_to_e);

		request = base;
		request.starts = stub_pieces(xI, Trackdir::Y_NW, ys + 1);
		request.goals = PlainRowPieces(l, nw_row, Trackdir::X_NE, nw_min_x, nw_max_x);
		return RouteFencedAndAdd(l, request, branch_fence, params.route_cache).has_value();
	};
	if (!route_movements(layout, true, {}, xI - 2, frame.width - 3, true, w_row, 2, -1)) return std::nullopt;

	auto finish = [&](JunctionLayout fl, Money *cost_out, TileIndexDiffC *hub_out) {
		return FinishJunctionCandidate(std::move(fl), spec, terrain, params.plane, w_row, frame.hub_x, cost_out, hub_out);
	};

	if (!zone.empty() && params.lane_style == 1) {
		/* D145: the parallelogram style takes its positions from the skeleton solver, for both staggers (D118). */
		JunctionSkeletonInputs in{};
		in.zmin[0] = *std::ranges::min_element(zone);
		in.zmax[0] = *std::ranges::max_element(zone);
		in.zmin[1] = zone_b.empty() ? in.zmin[0] : *std::ranges::min_element(zone_b);
		in.zmax[1] = zone_b.empty() ? in.zmax[0] : *std::ranges::max_element(zone_b);
		in.nmax = 0;
		for (int n = 10; n >= 1 && in.nmax == 0; n--) {
			if (spec.bridge_span_ok(n)) in.nmax = n;
		}
		if (in.nmax == 0) return std::nullopt;
		in.cover = spec.prio_tiles;
		in.x_cap = xI - 2;
		in.shift_back = true;
		in.k_w = JUNCTION_PRIO_AFTER_MERGE + params.k_w_delta;
		in.k_sb = JUNCTION_SHIFT_BACK_TILES + params.k_sb_delta;
		in.x_sb_min = 2;

		std::optional<JunctionLayout> best;
		bool best_clean = false;
		std::pair<int, Money> best_score{};
		Money best_cost = 0;
		TileIndexDiffC best_hub{};
		for (int stagger = 0; stagger <= 1; stagger++) {
			in.stagger = stagger;
			const JunctionSkeletonW pos = SolveJunctionSkeletonW(in);
			if (!pos.conflict.empty() || pos.x_ls <= 0) continue;
			if (std::min(pos.x_hw[0], pos.x_hw[1]) < 0 || std::max(pos.x_he[0], pos.x_he[1]) + 1 > frame.width - 1) continue;
			if (!spec.bridge_span_ok(pos.x_he[0] - pos.x_hw[0] - 1) || !spec.bridge_span_ok(pos.x_he[1] - pos.x_hw[1] - 1)) continue;
			std::string error;
			const auto lanes = BuildParallelogramLanes(pos, frame.width, w_row, b_row, &error);
			if (!lanes.has_value()) continue;

			/* Pass 2: fixed doubled lanes. */
			JunctionLayout fixed;
			base_layout(fixed);
			AddRasterToLayout(fixed, lanes->first);
			AddRasterToLayout(fixed, lanes->second);
			std::vector<TileIndexDiffC> under_spans;
			for (int l = 0; l < 2; l++) {
				for (int x = pos.x_hw[l] + 1; x < pos.x_he[l]; x++) under_spans.push_back(tile(x, l == 0 ? w_row : b_row));
			}
			if (!route_movements(fixed, false, under_spans, pos.x_nw, frame.width - 3, false, b_row, pos.x_sb + 1, pos.x_sl + 1)) continue;

			Money cost = 0;
			TileIndexDiffC cand_hub{};
			std::optional<JunctionLayout> built = finish(std::move(fixed), &cost, &cand_hub);
			if (!built.has_value()) continue;
			const bool clean = JunctionRulesClean(*built, spec);
			const std::pair<int, Money> score = ScoreJunctionLayout(*built, spec);
			/* Clean beats unclean, then the lower score; stagger 1 wins a tie. */
			if (!best.has_value() || (clean && !best_clean) || (clean == best_clean && score <= best_score)) {
				best = std::move(built);
				best_clean = clean;
				best_score = score;
				best_cost = cost;
				best_hub = cand_hub;
			}
		}
		if (!best.has_value()) return std::nullopt;
		if (terrain_cost != nullptr) *terrain_cost = best_cost;
		if (hub != nullptr) *hub = best_hub;
		return best;
	}

	if (!zone.empty()) {
		const int h_w = *std::ranges::min_element(zone) - params.bridge_margin;
		const int h_e = *std::ranges::max_element(zone) + params.bridge_margin;
		const int n = h_e - h_w - 1;
		if (!spec.bridge_span_ok(n)) return std::nullopt;
		if (h_w < 0 || h_e + 1 > frame.width - 1) return std::nullopt;

		/* Pass 2: fixed doubled lanes. */
		layout = JunctionLayout{};
		base_layout(layout);

		const std::string span = fmt::format("B{}", n);
		auto nw_max_for = [&](int merge) { return std::min(xI - 2, merge - static_cast<int>(spec.prio_tiles) - params.w_slack); };
		JunctionRaster raster_b;
		std::optional<int> merge_x;
		int nw_max_x = 0;
		/* Lane B: find p, q, r and s by rasterizing and checking the piece positions. */
		std::optional<JunctionRoute> lane_b;
		/* D110: q counts the plain tiles after the S-bend; try them in increasing order, so q = 0 (one real plain tile after the S-bend) comes first with gap 0. */
		for (int q = gap; q <= frame.width - 1 && !lane_b.has_value(); q++) {
			for (int p = 0; p <= frame.width - 1 && !lane_b.has_value(); p++) {
				const JunctionRaster head_raster = RasterizeJunctionRoute({tile(frame.width - 1, w_row), Trackdir::X_NE, std::string(p, 'F') + "RFL" + std::string(q, 'F') + span});
				if (!head_raster.error.empty()) continue;
				/* Pieces after the end of RFL (index p + 3) up to the near head. */
				size_t head = p + 4;
				while (head < head_raster.pieces.size() && !head_raster.pieces[head].on_bridge) head++;
				if (head >= head_raster.pieces.size() || head_raster.pieces[head].tile.x != h_e || head_raster.pieces[head].tile.y != b_row) continue;

				for (int r = 1; r <= 1 && !lane_b.has_value(); r++) {
					const std::string head_moves = std::string(p, 'F') + "RFL" + std::string(q, 'F') + span;
					const JunctionRaster far_raster = RasterizeJunctionRoute({tile(frame.width - 1, w_row), Trackdir::X_NE, head_moves + std::string(r, 'F')});
					if (!far_raster.error.empty()) continue;
					/* Plain axis pieces between the far head and the start of LFR: the r pieces after the head. */
					for (int s = 0; s <= frame.width - 1; s++) {
						const JunctionRoute candidate{tile(frame.width - 1, w_row), Trackdir::X_NE, head_moves + std::string(r, 'F') + "LFR" + std::string(s, 'F')};
						const JunctionRaster raster = RasterizeJunctionRoute(candidate);
						if (!raster.error.empty()) continue;
						if (raster.pieces.back().tile.x == 0) {
							lane_b = candidate;
							break;
						}
					}
				}
			}
		}
		if (!lane_b.has_value()) return std::nullopt;
		raster_b = RasterizeJunctionRoute(*lane_b);

		{
			/* Lane A is lane B's mirror: the same program with every L and R swapped. */
			JunctionRoute lane_a = *lane_b;
			for (char &c : lane_a.moves) {
				if (c == 'L') {
					c = 'R';
				} else if (c == 'R') {
					c = 'L';
				}
			}
			const JunctionRaster raster_a = RasterizeJunctionRoute(lane_a);
			if (!raster_a.error.empty()) return std::nullopt;
			size_t near_head_a = raster_a.pieces.size();
			size_t far_head_a = 0;
			for (size_t i = 0; i < raster_a.pieces.size(); i++) {
				if (!raster_a.pieces[i].on_bridge) continue;
				near_head_a = std::min(near_head_a, i);
				far_head_a = i;
			}
			if (near_head_a >= raster_a.pieces.size()) return std::nullopt;
			if (raster_a.pieces[near_head_a].tile.x != h_e || raster_a.pieces[near_head_a].tile.y != w_row - 1) return std::nullopt;
			if (raster_a.pieces[far_head_a].tile.x != h_w || raster_a.pieces[far_head_a].tile.y != w_row - 1) return std::nullopt;
			if (raster_a.pieces.back().tile.x != 0 || raster_a.pieces.back().tile.y != w_row) return std::nullopt;
			if (RasterUnits(raster_a) != RasterUnits(raster_b)) return std::nullopt;
			AddRasterToLayout(layout, raster_a);
			AddRasterToLayout(layout, raster_b);

			/* The lane merge tile: the W row tile where lane B's LFR puts a half piece. */
			size_t far_head = 0;
			for (size_t i = 0; i < raster_b.pieces.size(); i++) {
				if (raster_b.pieces[i].on_bridge) far_head = i;
			}
			for (size_t i = far_head + 1; i < raster_b.pieces.size(); i++) {
				const JunctionPiece &piece = raster_b.pieces[i];
				if (piece.tile.y == w_row && !IsDiagonalTrackdir(piece.trackdir)) {
					merge_x = piece.tile.x;
					break;
				}
			}
			if (!merge_x.has_value()) return std::nullopt;
			std::optional<int> merge_x_a;
			for (size_t i = far_head_a + 1; i < raster_a.pieces.size(); i++) {
				const JunctionPiece &piece = raster_a.pieces[i];
				if (piece.tile.y == w_row && !IsDiagonalTrackdir(piece.trackdir)) {
					merge_x_a = piece.tile.x;
					break;
				}
			}
			if (merge_x_a != merge_x) return std::nullopt;

		}
		/* Re-route the four movements with the lanes fixed and no crossable tiles. */
		std::vector<TileIndexDiffC> under_spans;
		for (int x = h_w + 1; x < h_e; x++) {
			under_spans.push_back(tile(x, w_row - 1));
			under_spans.push_back(tile(x, b_row));
		}
		/* N->W may join only a train length after the lane merge, so the prio's coverage stays on single track (D85). */
		/* D110: the lane split is where lane B first leaves the W row's axis piece. */
		int x_ls = -1;
		for (const JunctionPiece &piece : raster_b.pieces) {
			if (!IsDiagonalTrackdir(piece.trackdir)) {
				x_ls = piece.tile.x;
				break;
			}
		}
		if (x_ls < 0) return std::nullopt;
		nw_max_x = nw_max_for(*merge_x);
		if (!route_movements(layout, false, under_spans, nw_max_x, frame.width - 3, false, w_row, 2, x_ls + 3)) return std::nullopt;
	}

	return finish(std::move(layout), terrain_cost, hub);
}

/**
 * Build one generator candidate: the tight variant first (D110, gap 0); when that is not rule-clean, the variant with one more plain tile
 * after lane B's S-bend and before lane A's S-bend (gap 1) is built once and returned instead.
 * The parallelogram style is built once, from the skeleton solver (D145).
 */
std::optional<JunctionLayout> BuildJunctionCandidate(const JunctionSpec &spec, const JunctionCandidateParams &params, const JunctionTerrain *terrain, Money *terrain_cost, TileIndexDiffC *hub, bool *out_clean)
{
	bool clean = false;
	auto attempt = [&](int sb_back) -> std::optional<JunctionLayout> {
		clean = false;
		std::optional<JunctionLayout> tight = BuildJunctionCandidateWithGap(spec, params, terrain, terrain_cost, hub, 0, sb_back);
		if (tight.has_value() && JunctionRulesClean(*tight, spec)) {
			clean = true;
			return tight;
		}
		Money loose_cost = 0;
		TileIndexDiffC loose_hub{};
		std::optional<JunctionLayout> loose = BuildJunctionCandidateWithGap(spec, params, terrain, &loose_cost, &loose_hub, 1, sb_back);
		if (!loose.has_value()) return tight;
		if (terrain_cost != nullptr) *terrain_cost = loose_cost;
		if (hub != nullptr) *hub = loose_hub;
		clean = JunctionRulesClean(*loose, spec);
		return loose;
	};
	if (params.lane_style == 1) {
		/* D145: one build, from the solver; the gap, w_slack, margin and shift-back parameters play no part. */
		std::optional<JunctionLayout> result = BuildJunctionCandidateWithGap(spec, params, terrain, terrain_cost, hub, 0, 0);
		if (out_clean != nullptr) *out_clean = result.has_value() && JunctionRulesClean(*result, spec);
		return result;
	}
	std::optional<JunctionLayout> single = attempt(6);
	if (out_clean != nullptr) *out_clean = clean;
	return single;
}

uint8_t JunctionEWindowPhase(int prio_tiles)
{
	return (prio_tiles >= 5 && prio_tiles % 2 == 1) ? 1 : 0;
}

int JunctionTMapFirstHeight(const JunctionSpec &spec)
{
	return spec.ml_gap + 10;
}

JunctionTMapResult ConstructJunctionTMap(const JunctionSpec &spec, const JunctionTerrain *terrain, std::optional<int> first_height, bool line_router)
{
	const auto time_start = std::chrono::steady_clock::now();
	JunctionTMapResult result;
	result.e_phase = JunctionEWindowPhase(spec.prio_tiles);

	/*
	 * D173: "Flat empty ground is the relaxation of every terrain: terrain only removes options. So the height at which the T map builds on flat ground
	 * (old router) is an exact lower bound for projection; the line router starts there."
	 * The flat map ignores the terrain by design.
	 */
	int x_w = 0; ///< A's west port, frame column on row 0 (line router only).
	int x_e = 0; ///< A's east port.
	if (line_router) {
		const auto flat_start = std::chrono::steady_clock::now();
		JunctionTMapResult flat = ConstructJunctionTMap(spec, nullptr, std::nullopt, false);
		result.micros_flat = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - flat_start).count();
		if (!flat.layout.has_value()) {
			std::string prefixed;
			for (size_t from = 0; from < flat.log.size();) {
				const size_t end = flat.log.find('\n', from);
				const size_t stop = end == std::string::npos ? flat.log.size() : end + 1;
				prefixed += "flat map: " + flat.log.substr(from, stop - from);
				from = stop;
			}
			flat.log = std::move(prefixed);
			flat.micros_flat = result.micros_flat;
			flat.micros = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - time_start).count();
			return flat;
		}
		if (!first_height.has_value()) first_height = flat.height;
		/* D179: the ports come from the flat T, whose crop leaves the main line content +-2 (FinishJunctionCandidate). */
		x_w = MakeJunctionFrame(spec).hub_x - flat.hub.x;
		x_e = x_w + flat.layout->size.x - 1;
	}

	const JunctionFrame frame = MakeJunctionFrame(spec);
	const int g = spec.ml_gap;
	const int xI = frame.hub_x;
	const int xO = xI + spec.branch_gap + 1;
	const int w_row = g + 1;
	const int b_row = g + 2;
	const int first = first_height.value_or(JunctionTMapFirstHeight(spec));
	constexpr int STUB = 2;
	auto tile = [](int x, int y) { return TileIndexDiffC{static_cast<int16_t>(x), static_cast<int16_t>(y)}; };
	JunctionRouteCache route_cache;
	using Clock = std::chrono::steady_clock;
	auto since = [](Clock::time_point from) { return std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - from).count(); };
	/* Adds the wall time of one router call to micros_route. */
	auto timed_route = [&](auto &&route) {
		const auto from = Clock::now();
		auto routed = route();
		result.micros_route += since(from);
		return routed;
	};
	Clock::time_point height_start;
	/**
	 * The frame's last row at the current height: the checker's footprint rule forbids signals, switches and heads on the layout's boundary, and routed track gets signals,
	 * so the boundary ring belongs to the ports. Every line-router edge request blocks it except the two stub tiles.
	 */
	std::vector<TileIndexDiffC> last_row;

	/**
	 * One edge on the line router: one track, from the request's windows, against the layout so far, then added to it.
	 * @param layout The layout so far; its track and heads are obstacles, and the route is added to it.
	 * @param base The request's bounds, spacing and prices.
	 * @param blocked Tiles no track or bridge may use.
	 * @param w_row_heads_barred Whether a bridge head on the W row is prohibited (the old router's crossable rule).
	 * @return The route's raster, or none when it does not route.
	 */
	auto route_line = [&](JunctionLayout &layout, const JunctionRouteRequest &base, const std::vector<TileIndexDiffC> &blocked, bool w_row_heads_barred) -> std::optional<JunctionRaster> {
		JunctionLineRouteRequest line_request;
		line_request.line = JunctionLineSpec{1, 0, 1};
		auto clip = [&](const std::vector<JunctionRouteEnd> &ends) {
			std::vector<JunctionRouteEnd> clipped;
			for (const JunctionRouteEnd &end : ends) {
				if (end.tile.x >= base.min.x && end.tile.x <= base.max.x && end.tile.y >= base.min.y && end.tile.y <= base.max.y) clipped.push_back(end);
			}
			return clipped;
		};
		line_request.starts = clip(base.starts);
		line_request.goals = clip(base.goals);
		line_request.min = base.min;
		line_request.max = base.max;
		line_request.blocked = blocked;
		line_request.blocked.insert(line_request.blocked.end(), last_row.begin(), last_row.end());
		line_request.terrain = terrain;
		line_request.plane = base.plane;
		line_request.existing = &layout;
		line_request.curve_spacing = base.curve_spacing;
		line_request.s_bend_window = base.s_bend_window;
		line_request.rail_piece = spec.prices.rail_piece;
		line_request.turn_cost = base.turn_cost;
		line_request.bridge_span_ok = spec.bridge_span_ok;
		line_request.bridge_price = spec.prices.bridge;
		line_request.max_bridge_middle = base.max_bridge_middle;
		if (spec.prices.tunnel) {
			/* D175: towns and industries are passed around or under, so a priced tunnel lets the T map's edges go under hills; D174: these are branch (SL) lines, whose tunnels are single at basic throughput. */
			line_request.tunnel_price = spec.prices.tunnel;
			line_request.max_tunnel_length = spec.max_tunnel_length;
		}
		line_request.max_expansions = base.max_expansions;
		if (w_row_heads_barred) {
			/* A bridge head may not sit on the W row, which is the old router's crossable row (D156). */
			line_request.resource_cost = [&](const JunctionPiece &piece, int8_t, JunctionResourceLevel level) {
				return (level == JunctionResourceLevel::Head && piece.tile.y == w_row) ? Money(100 * 100 * spec.prices.rail_piece.base()) : Money(0);
			};
		}
		const JunctionLineRouteResult routed = FindJunctionLineRoute(line_request);
		result.line_router_trace += fmt::format("{} {} {}\n", routed.route.has_value() ? routed.route->moves : std::string("-"), routed.cost.base(), routed.expansions);
		if (!routed.route.has_value()) return std::nullopt;
		std::vector<JunctionRaster> rasters = RasterizeJunctionLine(line_request.line, *routed.route);
		if (rasters.empty() || !rasters[0].error.empty()) return std::nullopt;
		AddRasterToLayout(layout, rasters[0]);
		return std::move(rasters[0]);
	};

	/*
	 * D170a: the box grows until the map fits. Without a terrain that is the flat cap. With one, heights grow while the frame (rows 0 .. height - 1, its columns)
	 * lies inside the snapshot, which is the edge of what projection can see.
	 */
	int height_limit = g + 13;
	if (terrain != nullptr) {
		auto box_fits = [&](int rows) {
			for (int y = 0; y < rows; y++) {
				for (int x = 0; x <= frame.width - 1; x++) {
					if (terrain->At(x, y) == nullptr) return false;
				}
			}
			return true;
		};
		height_limit = 0;
		while (box_fits(height_limit + 1)) height_limit++;
	}

	/*
	 * D177: "the T map's plane is the ground height (z_max) of the hub tile, working (hub_x, 0), when that tile is Buildable and not steep; otherwise no layout."
	 * Without a terrain everything stays at plane 0.
	 */
	int plane = 0;
	if (terrain != nullptr) {
		const JunctionTerrainTile *hub_tile = terrain->At(xI, 0);
		if (hub_tile == nullptr || hub_tile->cls != JunctionTileClass::Buildable || hub_tile->steep) {
			result.log += "hub tile not buildable\n";
			result.micros = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - time_start).count();
			return result;
		}
		plane = hub_tile->z_max;
	}

	/*
	 * D179: "Before any routing, every port's approach (the port tile plus the straight pieces the rules require before it, D115a) must be usable at the ground height there;
	 * a failure returns at once with the blocked port and tile; the tool says to move the cursor." Every port is checked at D177's hub plane here;
	 * a port at its own ground height is a later step.
	 */
	auto ports_blocked = [&] {
		if (!line_router || terrain == nullptr) return false;
		for (const int x : {x_w, x_w + 1, x_w + STUB, x_e - STUB, x_e - 1, x_e}) {
			if (terrain->UsableAt(x, 0, plane)) continue;
			result.log += fmt::format("port approach blocked at {},0\n", x);
			result.port_blocked = true;
			result.micros = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - time_start).count();
			return true;
		}
		return false;
	};
	if (ports_blocked()) return result;

	bool redo_height = false; ///< The ports widened: the same height is routed again, and not counted again.
	for (int height = first; height <= height_limit; height++) {
		if (redo_height) {
			redo_height = false;
		} else {
			if (result.heights_tried > 0) result.micros_per_height.push_back(since(height_start));
			height_start = Clock::now();
			result.heights_tried++;
		}
		const int yb = height - 1;
		last_row.clear();
		for (int x = 0; x <= frame.width - 1; x++) {
			if (x != xI && x != xO) last_row.push_back(tile(x, yb));
		}
		auto say = [&](const std::string &text) { result.log += fmt::format("height {}: {}\n", height, text); };

		JunctionRouteRequest base{};
		base.min = tile(1, 0);
		base.max = tile(xO + frame.reach_east, yb);
		base.max_expansions = 50000 * frame.width / 64;
		base.curve_spacing = spec.curve_limit ? DeriveCurveSpacing(spec.reference_speed, spec.curve_speed, spec.tl) : 1;
		base.s_bend_window = 2u * spec.tl;
		base.bridge_span_ok = spec.bridge_span_ok;
		base.max_bridge_middle = 10;
		base.prices = spec.prices;
		base.crossable_cost = 0;
		base.turn_cost = spec.prices.rail_piece / 2;
		base.terrain = terrain;
		base.plane = plane;

		const int ys = yb - STUB;
		const JunctionRouteEnd goal_s{tile(xO, ys), Trackdir::Y_SE};
		const JunctionRouteEnd start_n{tile(xI, ys), Trackdir::Y_NW};
		/* The line router routes A (E1) and the port edges (E2, D178), so only the old router's layouts hold row 0 and the stubs as fixed track. */
		auto base_layout = [&](JunctionLayout &layout) {
			if (line_router) return;
			AddRasterToLayout(layout, RasterizeJunctionRoute(StraightRoute(tile(0, 0), Trackdir::X_SW, frame.width - 1)));
			AddRasterToLayout(layout, RasterizeJunctionRoute(StraightRoute(tile(xO, ys), Trackdir::Y_SE, STUB)));
			AddRasterToLayout(layout, RasterizeJunctionRoute(StraightRoute(tile(xI, yb), Trackdir::Y_NW, STUB)));
		};
		/* The old router's stub pieces; the line router takes them from the port edges' rasters (D178). */
		auto stub_pieces = [&](const JunctionLayout &layout, const std::optional<JunctionRaster> &port_edge, int x, Trackdir td, int first_y) {
			std::vector<JunctionRouteEnd> ends;
			if (!port_edge.has_value()) {
				for (int y = first_y; y < yb; y++) ends.push_back({tile(x, y), td});
				return ends;
			}
			for (const JunctionPiece &piece : port_edge->pieces) {
				if (piece.tile.y < first_y || piece.tile.y >= yb) continue;
				if (TracksAt(layout, piece.tile.x, piece.tile.y) == TrackBits{Track::Y}) ends.push_back({piece.tile, td});
			}
			return ends;
		};
		if (yb < g + 4) {
			say("frame too short");
			continue;
		}

		/*
		 * The fixed parts (B's lanes on the W row; with the old router also A's main line on row 0 and the stubs) are not routed yet, so an obstacle on them cannot be worked around.
		 * A taller box does not move row 0 or the W row: report it and stop (routing B is a later step). D178: the stubs are routed port edges,
		 * so an obstacle on one is passed by growing the box (D170a) and is not checked here.
		 */
		if (terrain != nullptr) {
			JunctionLayout fixed_parts;
			base_layout(fixed_parts);
			AddRasterToLayout(fixed_parts, RasterizeJunctionRoute(StraightRoute(tile(frame.width - 1, w_row), Trackdir::X_NE, frame.width - 1)));
			for (const JunctionTrack &jt : fixed_parts.tracks) {
				/* E1: A is routed, so row 0 is not checked here; B's W row is checked only between the same ports until E3 routes B. */
				if (line_router && jt.offset.y == w_row && (jt.offset.x < x_w || jt.offset.x > x_e)) continue;
				if (jt.tracks.None() || terrain->UsableAt(jt.offset.x, jt.offset.y, plane)) continue;
				say(fmt::format("fixed track on unusable tile {},{}", jt.offset.x, jt.offset.y));
				result.micros_per_height.push_back(since(height_start));
				result.micros = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - time_start).count();
				return result;
			}
		}

		/* A->S and N->A: final routes; B's row is crossable at cost 0 because B goes over (D156). */
		JunctionLayout pass1;
		base_layout(pass1);
		/*
		 * E2, D178: the S and N stubs are port edges, routed like every other edge, before A. The S port edge carries A->S and B->S, the N port edge N->A and N->B.
		 * Their ends are the same straight piece sequences the old router's stubs were; the boundary row is not blocked at the port tiles.
		 */
		std::optional<JunctionRaster> s_port_raster;
		std::optional<JunctionRaster> n_port_raster;
		if (line_router) {
			JunctionRouteRequest s_request = base;
			s_request.min = tile(xO, ys);
			s_request.max = tile(xO, yb);
			s_request.starts = {{tile(xO, ys), Trackdir::Y_SE}};
			s_request.goals = {{tile(xO, yb), Trackdir::Y_SE}};
			s_port_raster = timed_route([&] { return route_line(pass1, s_request, {}, false); });
			if (!s_port_raster.has_value()) {
				say("S port edge failed");
				continue;
			}
			JunctionRouteRequest n_request = base;
			n_request.min = tile(xI, ys);
			n_request.max = tile(xI, yb);
			n_request.starts = {{tile(xI, yb), Trackdir::Y_NW}};
			n_request.goals = {{tile(xI, ys), Trackdir::Y_NW}};
			n_port_raster = timed_route([&] { return route_line(pass1, n_request, {}, false); });
			if (!n_port_raster.has_value()) {
				say("N port edge failed");
				continue;
			}
		}
		/*
		 * E1, D178: A is an edge of the map, routed between its two ports like the branch edges. Its band runs from row 0 to the row above B's W row and may not cross B;
		 * leaving the band (a detour around B's rows) is a later step.
		 */
		std::optional<JunctionRaster> a_raster;
		if (line_router) {
			JunctionRouteRequest a_request = base;
			a_request.min = tile(x_w, 0);
			a_request.max = tile(x_e, g);
			a_request.starts = {{tile(x_w, 0), Trackdir::X_SW}};
			a_request.goals = {{tile(x_e, 0), Trackdir::X_SW}};
			a_raster = timed_route([&] { return route_line(pass1, a_request, {}, false); });
			if (!a_raster.has_value()) {
				say("A failed");
				continue;
			}
		}
		/* The line router sees no W row: the old router's crossable row is empty ground to it. */
		if (!line_router) AddRasterToLayout(pass1, RasterizeJunctionRoute(StraightRoute(tile(frame.width - 1, w_row), Trackdir::X_NE, frame.width - 1)));
		std::vector<TileIndexDiffC> w_tiles;
		for (int x = 0; x <= frame.width - 1; x++) w_tiles.push_back(tile(x, w_row));
		std::vector<TileIndexDiffC> column_fence;
		for (int y = 0; y <= g + 3; y++) column_fence.push_back(tile(xI, y));
		column_fence.push_back(tile(xI, ys - 1));
		column_fence.push_back(tile(xI, ys - 2));

		JunctionRouteRequest request = base;
		request.crossable = w_tiles;
		if (line_router) {
			request.starts = PlainRasterPieces(pass1, *a_raster, Trackdir::X_SW, 2, xI - 2);
		} else {
			for (int x = 2; x <= xI - 2; x++) {
				if (TracksAt(pass1, x, 0) == TrackBits{Track::X}) request.starts.push_back({tile(x, 0), Trackdir::X_SW});
			}
		}
		request.goals = {goal_s};
		const std::optional<JunctionRaster> a_to_s = timed_route([&] {
			return line_router ? route_line(pass1, request, column_fence, true) : RouteFencedAndAdd(pass1, request, column_fence, &route_cache);
		});
		if (!a_to_s.has_value()) {
			say("A->S failed");
			continue;
		}
		result.a_to_s_tiles.clear();
		for (const JunctionPiece &piece : a_to_s->pieces) result.a_to_s_tiles.push_back(piece.tile);

		/*
		 * D172: "each crossing is a node with a tile envelope derived from its realisation (X_B: D = nmax - 1 - 2 * k_head, from the doubled bridge's span);
		 * every edge passing the node gets the window [hi - D, lo + D] from the crossers already placed, blocked outside; the first crosser is unbounded and checked after."
		 * A->S is X_B's first crosser; N->A gets the window. The old router's column fence is its envelope, so this is the line router's only.
		 */
		std::vector<TileIndexDiffC> n_to_a_blocked;
		if (line_router) {
			int lo = std::numeric_limits<int>::max();
			int hi = std::numeric_limits<int>::min();
			for (const JunctionPiece &piece : a_to_s->pieces) {
				if (piece.tile.y != w_row && piece.tile.y != b_row) continue;
				lo = std::min<int>(lo, piece.tile.x);
				hi = std::max<int>(hi, piece.tile.x);
			}
			int nmax = 0;
			for (int n = 10; n >= 1 && nmax == 0; n--) {
				if (spec.bridge_span_ok(n)) nmax = n;
			}
			constexpr int K_HEAD = 1;
			const int window = nmax - 1 - 2 * K_HEAD;
			if (lo <= hi) {
				if (hi - lo > window) {
					say("A->S crossing wider than the doubled bridge");
					continue;
				}
				for (const int row : {w_row, b_row}) {
					for (int x = 0; x <= frame.width - 1; x++) {
						if (x < hi - window || x > lo + window) n_to_a_blocked.push_back(tile(x, row));
					}
				}
			}
		}

		request = base;
		request.crossable = w_tiles;
		request.starts = {start_n};
		const int x_s = a_to_s->pieces.front().tile.x;
		const int n_to_a_min_x = std::max(xO, x_s + static_cast<int>(spec.prio_tiles) + JUNCTION_E_PRIO_AFTER_SPLIT + result.e_phase);
		request.goals = line_router ? PlainRasterPieces(pass1, *a_raster, Trackdir::X_SW, n_to_a_min_x, std::min(frame.width - 3, x_e)) : PlainRowPieces(pass1, 0, Trackdir::X_SW, n_to_a_min_x, frame.width - 3);
		const std::optional<JunctionRaster> n_to_a = timed_route([&] {
			return line_router ? route_line(pass1, request, n_to_a_blocked, true) : RouteAndAdd(pass1, request, &route_cache);
		});
		if (!n_to_a.has_value()) {
			say("N->A failed");
			continue;
		}
		result.n_to_a_tiles.clear();
		for (const JunctionPiece &piece : n_to_a->pieces) result.n_to_a_tiles.push_back(piece.tile);

		/* The crossing set, and the skeleton from it. */
		const auto skeleton_start = Clock::now();
		auto skeleton_done = [&] { result.micros_skeleton += since(skeleton_start); };
		std::vector<int> zone;
		std::vector<int> zone_b;
		for (const JunctionRaster *raster : {&*a_to_s, &*n_to_a}) {
			for (const JunctionPiece &piece : raster->pieces) {
				if (piece.tile.y == w_row) zone.push_back(piece.tile.x);
				if (piece.tile.y == b_row) zone_b.push_back(piece.tile.x);
			}
		}
		if (zone.empty()) {
			skeleton_done();
			say("no crossing of the W row");
			continue;
		}
		JunctionSkeletonInputs in{};
		in.zmin[0] = *std::ranges::min_element(zone);
		in.zmax[0] = *std::ranges::max_element(zone);
		in.zmin[1] = zone_b.empty() ? in.zmin[0] : *std::ranges::min_element(zone_b);
		in.zmax[1] = zone_b.empty() ? in.zmax[0] : *std::ranges::max_element(zone_b);
		in.nmax = 0;
		for (int n = 10; n >= 1 && in.nmax == 0; n--) {
			if (spec.bridge_span_ok(n)) in.nmax = n;
		}
		if (in.nmax == 0) {
			skeleton_done();
			say("no bridge span is ok");
			continue;
		}
		in.cover = spec.prio_tiles;
		in.x_cap = xI - 2;
		in.shift_back = true;
		in.k_w = JUNCTION_PRIO_AFTER_MERGE;
		in.k_sb = JUNCTION_SHIFT_BACK_TILES;
		in.x_sb_min = 2;

		std::optional<JunctionSkeletonW> chosen;
		int chosen_stagger = 0;
		for (int stagger = 0; stagger <= 1; stagger++) {
			in.stagger = stagger;
			const JunctionSkeletonW pos = SolveJunctionSkeletonW(in);
			if (!pos.conflict.empty() || pos.x_ls <= 0) continue;
			if (std::min(pos.x_hw[0], pos.x_hw[1]) < 0 || std::max(pos.x_he[0], pos.x_he[1]) + 1 > frame.width - 1) continue;
			if (!spec.bridge_span_ok(pos.x_he[0] - pos.x_hw[0] - 1) || !spec.bridge_span_ok(pos.x_he[1] - pos.x_hw[1] - 1)) continue;
			/* The smaller bridge span wins; stagger 1 wins a tie. */
			if (!chosen.has_value() || pos.x_he[0] - pos.x_hw[0] <= chosen->x_he[0] - chosen->x_hw[0]) {
				chosen = pos;
				chosen_stagger = stagger;
			}
		}
		if (!chosen.has_value()) {
			skeleton_done();
			say("no valid skeleton for either stagger");
			continue;
		}
		const JunctionSkeletonW pos = *chosen;
		std::string error;
		const auto lanes = BuildParallelogramLanes(pos, frame.width, w_row, b_row, &error);
		if (!lanes.has_value()) {
			skeleton_done();
			say(fmt::format("lanes: {}", error));
			continue;
		}

		skeleton_done();

		/* The doubled lanes, then B->S and N->B. */
		JunctionLayout fixed;
		if (line_router) {
			AddRasterToLayout(fixed, *a_raster);
			AddRasterToLayout(fixed, *s_port_raster);
			AddRasterToLayout(fixed, *n_port_raster);
		}
		base_layout(fixed);
		AddRasterToLayout(fixed, *a_to_s);
		AddRasterToLayout(fixed, *n_to_a);
		AddRasterToLayout(fixed, lanes->first);
		AddRasterToLayout(fixed, lanes->second);
		std::vector<TileIndexDiffC> branch_fence;
		for (int l = 0; l < 2; l++) {
			for (int x = pos.x_hw[l] + 1; x < pos.x_he[l]; x++) branch_fence.push_back(tile(x, l == 0 ? w_row : b_row));
		}
		for (int y = 1; y < w_row; y++) for (int x = 0; x <= frame.width - 1; x++) branch_fence.push_back(tile(x, y));

		const int sl_x = pos.x_sl + 1;
		request = base;
		request.starts = PlainRowPieces(fixed, w_row, Trackdir::X_NE, sl_x, sl_x);
		if (sl_x < xO + 2 || request.starts.empty()) {
			say("B->S has no start");
			continue;
		}
		request.goals = stub_pieces(fixed, s_port_raster, xO, Trackdir::Y_SE, ys);
		if (!timed_route([&] {
			return line_router ? route_line(fixed, request, branch_fence, false) : RouteFencedAndAdd(fixed, request, branch_fence, &route_cache);
		}).has_value()) {
			say("B->S failed");
			continue;
		}
		request = base;
		request.starts = stub_pieces(fixed, n_port_raster, xI, Trackdir::Y_NW, ys + 1);
		request.goals = PlainRowPieces(fixed, b_row, Trackdir::X_NE, pos.x_sb + 1, pos.x_nw);
		if (!timed_route([&] {
			return line_router ? route_line(fixed, request, branch_fence, false) : RouteFencedAndAdd(fixed, request, branch_fence, &route_cache);
		}).has_value()) {
			say("N->B failed");
			continue;
		}

		const auto finish_start = Clock::now();
		/*
		 * No terrain here: the line router has already checked every routed piece against the terrain (heights, ramps, foundations), and the fixed parts
		 * are checked at the hub's plane (D177) before routing (G3b). FinishJunctionCandidate's flat-plane test would reject every ramp.
		 */
		std::optional<JunctionLayout> built = FinishJunctionCandidate(std::move(fixed), spec, nullptr, 0, w_row, frame.hub_x, nullptr, &result.hub);
		result.micros_finish += since(finish_start);
		if (!built.has_value()) {
			say("finish failed");
			continue;
		}
		if (line_router) {
			/*
			 * D170a (the box grows sideways) and D179: the flat T's ports are where A starts, not limits. When this height's crop reaches past them
			 * (A->S splits or N->A merges outside the ports), A starts at the wider ports and the same height is routed again.
			 * Ports only widen, within the frame, so this ends.
			 */
			const int fx_min = frame.hub_x - result.hub.x;
			const int fx_max = fx_min + built->size.x - 1;
			if (fx_min < x_w || fx_max > x_e) {
				if (fx_min < 0 || fx_max > frame.width - 1) {
					say("ports outside the frame");
					continue;
				}
				x_w = std::min(x_w, fx_min);
				x_e = std::max(x_e, fx_max);
				say(fmt::format("ports widened to {}..{}", x_w, x_e));
				if (ports_blocked()) return result;
				redo_height = true;
				height--;
				continue;
			}
		}
		say("built");
		result.height = height;
		result.stagger = static_cast<uint8_t>(chosen_stagger);
		result.layouts_built++;
		const auto check_start = Clock::now();
		result.clean = JunctionRulesClean(*built, spec);
		if (!result.clean) result.violations = CheckJunctionRules(*built, spec);
		result.micros_check += since(check_start);
		result.layout = std::move(built);
		break;
	}
	if (result.heights_tried > 0) result.micros_per_height.push_back(since(height_start));
	result.micros = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - time_start).count();
	return result;
}

/** The number of middle tiles of a bridge. */
static uint BridgeMiddleTiles(const JunctionBridge &bridge)
{
	return static_cast<uint>(std::abs(bridge.end.x - bridge.start.x) + std::abs(bridge.end.y - bridge.start.y)) - 1;
}

/** Generator score of a layout: the count of tiles it builds on or over, then the build cost (D114). Lower is better. */
std::pair<int, Money> ScoreJunctionLayout(const JunctionLayout &layout, const JunctionSpec &spec)
{
	std::set<std::pair<int, int>> tiles;
	int track_bits = 0;
	for (const JunctionTrack &jt : layout.tracks) {
		track_bits += static_cast<int>(jt.tracks.Count());
		if (!jt.tracks.None()) tiles.insert({jt.offset.x, jt.offset.y});
	}
	Money cost = spec.prices.rail_piece * track_bits + spec.prices.signal * static_cast<int>(layout.signals.size());
	for (const JunctionBridge &bridge : layout.bridges) {
		cost += spec.prices.bridge(BridgeMiddleTiles(bridge));
		const int dx = (bridge.end.x > bridge.start.x) - (bridge.end.x < bridge.start.x);
		const int dy = (bridge.end.y > bridge.start.y) - (bridge.end.y < bridge.start.y);
		for (int x = bridge.start.x, y = bridge.start.y;; x += dx, y += dy) {
			tiles.insert({x, y});
			if (x == bridge.end.x && y == bridge.end.y) break;
		}
	}
	return {static_cast<int>(tiles.size()), cost};
}

/** The candidate planes for a terrain: the most common z_max values of Buildable tiles around the hub, at most 3. */
std::vector<int> JunctionTerrainPlanes(const JunctionTerrain &terrain)
{
	std::map<int, int> counts;
	for (int y = -4; y <= 4; y++) {
		for (int x = 26; x <= 34; x++) {
			const JunctionTerrainTile *t = terrain.At(x, y);
			if (t != nullptr && t->cls == JunctionTileClass::Buildable) counts[t->z_max]++;
		}
	}
	std::vector<int> planes;
	for (const auto &[z, n] : counts) planes.push_back(z);
	std::ranges::stable_sort(planes, [&](int a, int b) { return counts[a] > counts[b]; });
	if (planes.size() > 3) planes.resize(3);
	return planes;
}

/** Run fn(i) for i in [0, n) on up to max_threads threads (including the caller); returns when all are done. */
static void JunctionParallelFor(size_t n, const std::function<void(size_t)> &fn)
{
	const size_t max_threads = std::max(1u, std::min(8u, std::thread::hardware_concurrency()));
	std::atomic<size_t> next{0};
	auto work = [&]() {
		for (size_t i = next.fetch_add(1); i < n; i = next.fetch_add(1)) fn(i);
	};

	std::vector<std::thread> threads;
	for (size_t t = 1; t < std::min(max_threads, n); t++) {
		std::thread thr;
		if (!StartNewThread(&thr, "ottd:junction", [&work]() { work(); })) break;
		threads.push_back(std::move(thr));
	}
	work();
	for (std::thread &thr : threads) thr.join();
}

/** The search bound of the prio sizing, in tiles (D137). */
static constexpr uint8_t MAX_PRIO_TILES = 32;

/**
 * The prio timings of a layout for a spec's design train, with the paths and the design speed obtained as EvaluateJunctionThroughput obtains them.
 * @param layout The layout.
 * @param spec The junction spec; train and prio_tiles are used.
 * @return One timing per prio merge; none when a movement has no path.
 */
static std::vector<JunctionPrioTiming> PrioTimingsOf(const JunctionLayout &layout, const JunctionSpec &spec)
{
	const JunctionCheckContext ctx = MakeJunctionCheckContext(layout, spec, FindJunctionPorts(layout), JunctionTMovements());
	std::vector<JunctionPath> paths;
	/* The same paths-in-movement-order loop as in EvaluateJunctionThroughput. */
	for (const auto &movement : JunctionTMovements()) {
		const auto path = std::ranges::find_if(ctx.paths, [&](const JunctionPath &p) { return p.from == movement.first && p.to == movement.second; });
		if (path == ctx.paths.end()) return {};
		paths.push_back(*path);
	}
	return JunctionPrioTimings(layout, spec, paths, spec.train, JunctionTerminalSpeed(spec.train), spec.tl);
}

/** The candidates of one height (D146): the symmetric style over the full sweep, the parallelogram style once per crossing cost and E-side phase (D146, D148, D148a). */
static void AddJunctionSweepParams(std::vector<JunctionCandidateParams> &out, const JunctionSpec &spec, int height, int plane, JunctionRouteCache *cache)
{
	for (Money crossing_cost : {Money(0), spec.prices.bridge(3)}) {
		for (uint8_t e_slack = 0; e_slack <= 3; e_slack++) {
			for (uint8_t w_slack = 0; w_slack <= 3; w_slack++) {
				for (uint8_t margin = 1; margin <= 3; margin++) {
					JunctionCandidateParams params{static_cast<uint8_t>(height), crossing_cost, e_slack, w_slack};
					params.plane = plane;
					params.bridge_margin = margin;
					params.lane_style = 0;
					params.route_cache = cache;
					out.push_back(params);
				}
			}
		}
		for (uint8_t e_slack = 0; e_slack <= 1; e_slack++) {
			JunctionCandidateParams params{static_cast<uint8_t>(height), crossing_cost, e_slack, 0};
			params.plane = plane;
			params.lane_style = 1;
			params.route_cache = cache;
			out.push_back(params);
		}
	}
}

/**
 * Sweep candidates, keep the rule-clean ones and pick the best by score, without checking the prio timing.
 * @param spec The junction spec; prio_tiles is used as given.
 * @param terrain Optional terrain snapshot.
 * @return The best layout, or NoLayout when no candidate is clean.
 */
static JunctionGenerateResult GenerateJunctionUnchecked(const JunctionSpec &spec, const JunctionTerrain *terrain)
{
	/* With no gap row the W prio has nowhere to go: the row inside the W track is the E track, and the joiner blocks the outside. */
	if (spec.ml_gap == 0) return {std::nullopt, JunctionGenerateError::NeedsMlGap};

	std::vector<int> planes{0};
	if (terrain != nullptr) {
		planes = JunctionTerrainPlanes(*terrain);
		if (planes.empty()) return {std::nullopt, JunctionGenerateError::NoLayout};
	}

	std::optional<JunctionLayout> best;
	std::pair<int, Money> best_score{};
	int best_area = 0;
	TileIndexDiffC best_hub{};

	const int first = spec.ml_gap + 5;
	const int last = spec.ml_gap + 20;
	JunctionRouteCache route_cache;
	for (const int plane : planes) {
		int stop_after = INT32_MAX;
		for (int height = first; height <= last && height <= stop_after; height++) {
			bool clean_here = false;
			std::vector<JunctionCandidateParams> all_params;
			AddJunctionSweepParams(all_params, spec, height, plane, &route_cache);
			struct Result {
				std::optional<JunctionLayout> layout;
				Money terrain_cost = 0;
				TileIndexDiffC hub{};
				bool clean = false;
			};
			std::vector<Result> results(all_params.size());
			JunctionParallelFor(all_params.size(), [&](size_t i) {
				Result &r = results[i];
				r.layout = BuildJunctionCandidate(spec, all_params[i], terrain, &r.terrain_cost, &r.hub, &r.clean);
			});
			for (Result &r : results) {
				if (!r.clean) continue;
				std::optional<JunctionLayout> &layout = r.layout;
				const TileIndexDiffC hub = r.hub;
				clean_here = true;
				std::pair<int, Money> score = ScoreJunctionLayout(*layout, spec);
				score.second += r.terrain_cost;
				const int area = layout->size.x * layout->size.y;
				if (!best.has_value() || score < best_score || (score == best_score && area < best_area)) {
					best = std::move(layout);
					best_hub = hub;
					best_score = score;
					best_area = area;
				}
			}
			if (clean_here && stop_after == INT32_MAX) stop_after = height + 2;
		}
	}
	if (!best.has_value()) return {std::nullopt, JunctionGenerateError::NoLayout};
	RemoveRedundantJunctionSignals(*best, spec);
	best_score = ScoreJunctionLayout(*best, spec);
	return {std::move(best), JunctionGenerateError::None, best_hub, best_score};
}

/**
 * Generate a junction: sweep candidates, keep the rule-clean ones and pick the best by score.
 * @param spec The junction spec; prio_tiles must be set, see SizeJunctionPrio.
 * @param terrain Optional terrain snapshot.
 * @return The best layout and spec.prio_tiles; NoLayout when no candidate is clean; PrioDelaysMainline when the best layout's prio delays the ML (unless spec.prio_strict is false).
 */
JunctionGenerateResult GenerateJunction(const JunctionSpec &spec, const JunctionTerrain *terrain)
{
	assert(spec.prio_tiles > 0);
	JunctionGenerateResult result = GenerateJunctionUnchecked(spec, terrain);
	if (!result.layout.has_value()) return result;

	if (spec.prio_strict && std::ranges::any_of(PrioTimingsOf(*result.layout, spec), [](const JunctionPrioTiming &t) { return t.ml_delayed || t.timed_out; })) {
		return {std::nullopt, JunctionGenerateError::PrioDelaysMainline};
	}
	result.prio_tiles = spec.prio_tiles;
	return result;
}

/**
 * The least prio coverage, in tiles, that does not delay the ML on the flat generated junction for the spec's design train (D137).
 * The coverage starts at max(1, spec.prio_tiles); each step jumps to the farthest delaying ML start of the exact timing (S1c2).
 * @param spec The junction spec; train is required, prio_tiles is the least coverage tried.
 * @return The coverage; or {0, the generation's error} when no flat layout exists, or {0, PrioTooLong} when it would exceed the search bound.
 */
JunctionPrioSizing SizeJunctionPrio(const JunctionSpec &spec)
{
	JunctionSpec copy = spec;
	copy.prio_tiles = std::max<uint8_t>(1, spec.prio_tiles);
	for (;;) {
		const JunctionGenerateResult result = GenerateJunctionUnchecked(copy, nullptr);
		if (!result.layout.has_value()) return {0, result.error};

		bool delayed = false;
		uint next = 0;
		/* A timed out timing steps the coverage by one: its required_distance is 0 or small. */
		for (const JunctionPrioTiming &timing : PrioTimingsOf(*result.layout, copy)) {
			if (!timing.ml_delayed && !timing.timed_out) continue;
			delayed = true;
			next = std::max(next, timing.required_distance / JunctionProgressPerStraightTile() + 1);
		}
		if (!delayed) return {copy.prio_tiles, JunctionGenerateError::None};

		const uint wanted = std::max<uint>(copy.prio_tiles + 1u, next);
		if (wanted > MAX_PRIO_TILES) return {0, JunctionGenerateError::PrioTooLong};
		copy.prio_tiles = static_cast<uint8_t>(wanted);
	}
}

std::vector<JunctionLibraryEntry> BuildJunctionLibrary(const JunctionSpec &spec)
{
	std::vector<JunctionLibraryEntry> library;
	if (spec.ml_gap == 0) return library;

	std::set<std::string> seen;
	const int first = spec.ml_gap + 5;
	const int last = spec.ml_gap + 20;
	JunctionRouteCache route_cache;
	std::vector<JunctionCandidateParams> all_params;
	for (int height = first; height <= last; height++) {
		AddJunctionSweepParams(all_params, spec, height, 0, &route_cache);
	}
	struct Result {
		std::optional<JunctionLayout> layout;
		TileIndexDiffC hub{};
		bool clean = false;
	};
	std::vector<Result> results(all_params.size());
	JunctionParallelFor(all_params.size(), [&](size_t i) {
		Result &r = results[i];
		r.layout = BuildJunctionCandidate(spec, all_params[i], nullptr, nullptr, &r.hub, &r.clean);
	});
	for (Result &r : results) {
		if (!r.clean) continue;
		std::optional<JunctionLayout> &layout = r.layout;
		if (!seen.insert(JunctionLayoutToJson(*layout)).second) continue;
		if (spec.prio_strict && std::ranges::any_of(PrioTimingsOf(*layout, spec), [](const JunctionPrioTiming &t) { return t.ml_delayed || t.timed_out; })) continue;
		JunctionLibraryEntry entry;
		entry.hub = r.hub;
		entry.frame = MakeJunctionFrame(spec);
		RemoveRedundantJunctionSignals(*layout, spec);
		entry.score = ScoreJunctionLayout(*layout, spec);
		entry.area = layout->size.x * layout->size.y;
		entry.prio_tiles = spec.prio_tiles;
		entry.layout = std::move(*layout);
		library.push_back(std::move(entry));
	}
	return library;
}

std::optional<JunctionLibraryFit> FitJunctionLibrary(const std::vector<JunctionLibraryEntry> &library, const JunctionTerrain &terrain)
{
	const std::vector<int> planes = JunctionTerrainPlanes(terrain);
	std::optional<JunctionLibraryFit> best;
	std::tuple<int, Money, int> best_key{};
	for (size_t i = 0; i < library.size(); i++) {
		const JunctionLibraryEntry &e = library[i];
		const int ox = e.frame.hub_x - e.hub.x;
		const int oy = -e.hub.y;
		for (const int plane : planes) {
			bool ok = true;
			std::vector<std::pair<int, int>> occupied;
			for (const JunctionTrack &jt : e.layout.tracks) {
				if (jt.tracks.None()) continue;
				const int x = jt.offset.x + ox;
				const int y = jt.offset.y + oy;
				if (!terrain.UsableAt(x, y, plane)) { ok = false; break; }
				occupied.emplace_back(x, y);
			}
			for (size_t b = 0; ok && b < e.layout.bridges.size(); b++) {
				const JunctionBridge &bridge = e.layout.bridges[b];
				for (const TileIndexDiffC &head : {bridge.start, bridge.end}) {
					const int x = head.x + ox;
					const int y = head.y + oy;
					if (!terrain.UsableAt(x, y, plane)) { ok = false; break; }
					occupied.emplace_back(x, y);
				}
				if (!ok) break;
				const int dx = (bridge.end.x > bridge.start.x) - (bridge.end.x < bridge.start.x);
				const int dy = (bridge.end.y > bridge.start.y) - (bridge.end.y < bridge.start.y);
				for (int x = bridge.start.x + dx, y = bridge.start.y + dy; x != bridge.end.x || y != bridge.end.y; x += dx, y += dy) {
					if (!terrain.SpanOk(x + ox, y + oy, plane)) { ok = false; break; }
				}
			}
			if (!ok) continue;
			std::ranges::sort(occupied);
			occupied.erase(std::unique(occupied.begin(), occupied.end()), occupied.end());
			Money cost = 0;
			for (const auto &[x, y] : occupied) cost += terrain.CostAt(x, y, plane);
			const std::tuple<int, Money, int> key{e.score.first, e.score.second + cost, e.area};
			if (!best.has_value() || key < best_key) {
				best = JunctionLibraryFit{i, plane, cost};
				best_key = key;
			}
		}
	}
	return best;
}
