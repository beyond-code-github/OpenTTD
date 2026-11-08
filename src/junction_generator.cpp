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
#include "core/math_func.hpp"
#include "junction_route.h"
#include "junction_router.h"
#include "junction_signal_pass.h"
#include "track_func.h"

#include <algorithm>
#include <map>
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

/** Route, rasterize and add to the layout. */
std::optional<JunctionRaster> RouteAndAdd(JunctionLayout &layout, JunctionRouteRequest request)
{
	auto clip = [&](std::vector<JunctionRouteEnd> &ends) {
		std::erase_if(ends, [&](const JunctionRouteEnd &end) {
			return end.tile.x < request.min.x || end.tile.x > request.max.x || end.tile.y < request.min.y || end.tile.y > request.max.y;
		});
	};
	clip(request.starts);
	clip(request.goals);
	const std::optional<JunctionRoute> route = FindJunctionRoute(layout, request);
	if (!route.has_value()) return std::nullopt;
	JunctionRaster raster = RasterizeJunctionRoute(*route);
	if (!raster.error.empty()) return std::nullopt;
	AddRasterToLayout(layout, raster);
	return raster;
}

/** Route against a copy of the layout with blocker tracks on the given empty tiles, then add the raster to the real layout. */
std::optional<JunctionRaster> RouteFencedAndAdd(JunctionLayout &layout, JunctionRouteRequest request, const std::vector<TileIndexDiffC> &fence)
{
	JunctionLayout fenced = layout;
	for (const TileIndexDiffC &t : fence) {
		if (TracksAt(layout, t.x, t.y).None()) fenced.tracks.push_back({t, TrackBits{Track::Y}});
	}
	std::optional<JunctionRaster> raster = RouteAndAdd(fenced, request);
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
 * Build one generator candidate: route the branch movements, double the mainline bridges, crop, and place signals.
 * @param spec The junction spec.
 * @param params The swept inputs.
 * @param terrain Optional terrain snapshot in the generator's working coordinates (see the header).
 * @param terrain_cost Receives the terrain cost when terrain is set.
 * @return The layout, or std::nullopt when this candidate cannot be built.
 */
std::optional<JunctionLayout> BuildJunctionCandidate(const JunctionSpec &spec, const JunctionCandidateParams &params, const JunctionTerrain *terrain, Money *terrain_cost, TileIndexDiffC *hub)
{
	const int g = spec.ml_gap;
	const int xI = 30;
	const int xO = xI + spec.branch_gap + 1;
	const int yb = static_cast<int>(params.height) - 1;
	if (yb < g + 4) return std::nullopt;
	const int w_row = g + 1;
	const int b_row = g + 2;

	auto tile = [](int x, int y) { return TileIndexDiffC{static_cast<int16_t>(x), static_cast<int16_t>(y)}; };

	JunctionRouteRequest base{};
	base.min = tile(1, 0); // N->W joins as far left as x = 2 (its window shrinks with TL), so the box must reach it.
	base.max = tile(xO + 16, yb);
	base.max_expansions = 50000;
	base.curve_spacing = spec.curve_limit ? DeriveCurveSpacing(spec.reference_speed, spec.curve_speed, spec.tl) : 1;
	base.s_bend_window = 2u * spec.tl;
	base.bridge_span_ok = spec.bridge_span_ok;
	base.max_bridge_middle = 10;
	base.prices = spec.prices;
	base.crossable_cost = 0;
	base.turn_cost = spec.prices.rail_piece / 2;
	base.terrain = terrain;
	base.plane = params.plane;

	/* Short straight branch legs: E->S joins the top of the S stub and W->S merges into its side; N->E leaves the top of the N stub and N->W splits from its side. */
	constexpr int STUB = 3;
	const int ys = yb - STUB;
	const JunctionRouteEnd goal_s{tile(xO, ys), Trackdir::Y_SE};
	const JunctionRouteEnd start_n{tile(xI, ys), Trackdir::Y_NW};

	auto base_layout = [&](JunctionLayout &layout) {
		AddRasterToLayout(layout, RasterizeJunctionRoute(StraightRoute(tile(0, 0), Trackdir::X_SW, 63)));
		AddRasterToLayout(layout, RasterizeJunctionRoute(StraightRoute(tile(xO, ys), Trackdir::Y_SE, STUB)));
		AddRasterToLayout(layout, RasterizeJunctionRoute(StraightRoute(tile(xI, yb), Trackdir::Y_NW, STUB)));
	};
	auto stub_pieces = [&](int x, Trackdir td) {
		std::vector<JunctionRouteEnd> ends;
		for (int y = ys + 1; y < yb; y++) ends.push_back({tile(x, y), td});
		return ends;
	};

	/* Pass 1: find the crossing zone. */
	JunctionLayout layout;
	base_layout(layout);
	AddRasterToLayout(layout, RasterizeJunctionRoute(StraightRoute(tile(63, w_row), Trackdir::X_NE, 63)));
	std::vector<int> zone; ///< The x of every W row tile that a crossing movement (E->S, N->E) uses.
	auto note_crossings = [&](const JunctionRaster &raster) {
		for (const JunctionPiece &piece : raster.pieces) {
			if (piece.tile.y == w_row) zone.push_back(piece.tile.x);
		}
	};
	/*
	 * Route the four movements in the order E->S, W->S, N->E, N->W.
	 * Fences: E->S may not touch column xI above row g+3, so it crosses the N column low down and N->E bridges straight
	 * over it. W->S and N->W stay on the branch side: they may not use the ML gap rows or the ground under ML bridges.
	 */
	auto route_movements = [&](JunctionLayout &l, bool crossable_w, const std::vector<TileIndexDiffC> &under_spans, int nw_max_x, int lane_b_max_x, bool note) -> bool {
		std::vector<TileIndexDiffC> w_tiles;
		if (crossable_w) for (int x = 0; x <= 63; x++) w_tiles.push_back(tile(x, w_row));
		std::vector<TileIndexDiffC> column_fence;
		for (int y = 0; y <= g + 3; y++) column_fence.push_back(tile(xI, y));
		std::vector<TileIndexDiffC> branch_fence = under_spans;
		for (int y = 1; y < w_row; y++) for (int x = 0; x <= 63; x++) branch_fence.push_back(tile(x, y));

		JunctionRouteRequest request = base;
		request.crossable = w_tiles;
		request.crossable_cost = params.crossing_cost;
		for (int x = 2; x <= xI - 2; x++) {
			if (TracksAt(l, x, 0) == TrackBits{Track::X}) request.starts.push_back({tile(x, 0), Trackdir::X_SW});
		}
		request.goals = {goal_s};
		const std::optional<JunctionRaster> e_to_s = RouteFencedAndAdd(l, request, column_fence);
		if (!e_to_s.has_value()) return false;
		if (note) note_crossings(*e_to_s);

		request = base;
		request.starts = PlainRowPieces(l, w_row, Trackdir::X_NE, xO + 2, 61);
		for (const JunctionRouteEnd &end : PlainRowPieces(l, b_row, Trackdir::X_NE, xO + 2, lane_b_max_x)) request.starts.push_back(end);
		request.goals = stub_pieces(xO, Trackdir::Y_SE);
		if (!RouteFencedAndAdd(l, request, branch_fence).has_value()) return false;

		request = base;
		request.crossable = w_tiles;
		request.crossable_cost = params.crossing_cost;
		request.starts = {start_n};
		request.goals = PlainRowPieces(l, 0, Trackdir::X_SW, xO, 61);
		const std::optional<JunctionRaster> n_to_e = RouteAndAdd(l, request);
		if (!n_to_e.has_value()) return false;
		if (note) note_crossings(*n_to_e);

		request = base;
		request.starts = stub_pieces(xI, Trackdir::Y_NW);
		request.goals = PlainRowPieces(l, w_row, Trackdir::X_NE, 2, nw_max_x);
		return RouteFencedAndAdd(l, request, branch_fence).has_value();
	};
	if (!route_movements(layout, true, {}, xI - 2, 61, true)) return std::nullopt;

	if (!zone.empty()) {
		const int h_w = *std::ranges::min_element(zone) - 3;
		const int h_e = *std::ranges::max_element(zone) + 3;
		const int n = h_e - h_w - 1;
		if (!spec.bridge_span_ok(n)) return std::nullopt;
		if (h_w < 0 || h_e + 1 > 63) return std::nullopt;

		/* Pass 2: fixed doubled lanes. */
		layout = JunctionLayout{};
		base_layout(layout);

		const std::string span = fmt::format("B{}", n);
		const JunctionRoute lane_a{tile(63, w_row), Trackdir::X_NE, std::string(62 - h_e, 'F') + span + std::string(h_w, 'F')};
		const JunctionRaster raster_a = RasterizeJunctionRoute(lane_a);
		if (!raster_a.error.empty()) return std::nullopt;

		/* Lane B: find p, q, r and s by rasterizing and checking the piece positions. */
		std::optional<JunctionRoute> lane_b;
		for (int p = 0; p <= 63 && !lane_b.has_value(); p++) {
			for (int q = 1; q <= 63 && !lane_b.has_value(); q++) {
				const JunctionRaster head_raster = RasterizeJunctionRoute({tile(63, w_row), Trackdir::X_NE, std::string(p, 'F') + "RFL" + std::string(q, 'F') + span});
				if (!head_raster.error.empty()) continue;
				/* Pieces after the end of RFL (index p + 3) up to the near head. */
				size_t head = p + 4;
				while (head < head_raster.pieces.size() && !head_raster.pieces[head].on_bridge) head++;
				if (head >= head_raster.pieces.size() || head_raster.pieces[head].tile.x != h_e || head_raster.pieces[head].tile.y != b_row) continue;
				uint plain_axis = 0;
				for (size_t i = p + 4; i < head; i++) {
					if (IsDiagonalTrackdir(head_raster.pieces[i].trackdir)) plain_axis++;
				}
				if (plain_axis != 1) continue;

				for (int r = 1; r <= 1 && !lane_b.has_value(); r++) {
					const std::string head_moves = std::string(p, 'F') + "RFL" + std::string(q, 'F') + span;
					const JunctionRaster far_raster = RasterizeJunctionRoute({tile(63, w_row), Trackdir::X_NE, head_moves + std::string(r, 'F')});
					if (!far_raster.error.empty()) continue;
					/* Plain axis pieces between the far head and the start of LFR: the r pieces after the head. */
					for (int s = 0; s <= 63; s++) {
						const JunctionRoute candidate{tile(63, w_row), Trackdir::X_NE, head_moves + std::string(r, 'F') + "LFR" + std::string(s, 'F')};
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
		const JunctionRaster raster_b = RasterizeJunctionRoute(*lane_b);
		if (RasterUnits(raster_a) != RasterUnits(raster_b)) return std::nullopt;
		AddRasterToLayout(layout, raster_a);
		AddRasterToLayout(layout, raster_b);

		/* The lane merge tile: the W row tile where lane B's LFR puts a half piece. */
		std::optional<int> merge_x;
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

		/* Re-route the four movements with the lanes fixed and no crossable tiles. */
		std::vector<TileIndexDiffC> under_spans;
		for (int x = h_w + 1; x < h_e; x++) {
			under_spans.push_back(tile(x, w_row));
			under_spans.push_back(tile(x, b_row));
		}
		/*
		 * W->S may leave lane B only a train length after lane B's S-bend, so its first turn is clear of the S-bend's turns (G3).
		 * N->W may join only a train length after the lane merge, so the prio's coverage stays on single track (D85).
		 */
		int lane_b_axis_x = -1;
		for (const JunctionPiece &piece : raster_b.pieces) {
			if (piece.tile.y == b_row && IsDiagonalTrackdir(piece.trackdir)) {
				lane_b_axis_x = piece.tile.x;
				break;
			}
		}
		const int lane_b_max_x = lane_b_axis_x - static_cast<int>(spec.tl);
		const int nw_max_x = std::min(xI - 2, *merge_x - 1 - static_cast<int>(spec.tl) - params.joiner_slack);
		if (!route_movements(layout, false, under_spans, nw_max_x, lane_b_max_x, false)) return std::nullopt;
	}

	/* Crop the E and W rows to the content. */
	int min_x = INT32_MAX;
	int max_x = INT32_MIN;
	for (const JunctionTrack &jt : layout.tracks) {
		const bool plain_row = (jt.offset.y == 0 || jt.offset.y == w_row) && jt.tracks == TrackBits{Track::X};
		if (plain_row) continue;
		min_x = std::min<int>(min_x, jt.offset.x);
		max_x = std::max<int>(max_x, jt.offset.x);
	}
	for (const JunctionBridge &bridge : layout.bridges) {
		for (const TileIndexDiffC &head : {bridge.start, bridge.end}) {
			min_x = std::min<int>(min_x, head.x);
			max_x = std::max<int>(max_x, head.x);
		}
	}
	if (max_x < min_x) return std::nullopt;
	std::erase_if(layout.tracks, [&](const JunctionTrack &jt) {
		const bool plain_row = (jt.offset.y == 0 || jt.offset.y == w_row) && jt.tracks == TrackBits{Track::X};
		return plain_row && (jt.offset.x < min_x - 2 || jt.offset.x > max_x + 2);
	});

	if (terrain != nullptr) {
		const int plane = params.plane;
		std::set<std::pair<int, int>> occupied;
		for (const JunctionTrack &jt : layout.tracks) {
			if (jt.tracks.None()) continue;
			if (!terrain->UsableAt(jt.offset.x, jt.offset.y, plane)) return std::nullopt;
			occupied.insert({jt.offset.x, jt.offset.y});
		}
		for (const JunctionBridge &bridge : layout.bridges) {
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
		if (terrain_cost != nullptr) {
			Money cost = 0;
			for (const auto &[x, y] : occupied) cost += terrain->CostAt(x, y, plane);
			*terrain_cost = cost;
		}
	}

	/* The shift NormaliseJunctionLayout applies: the minimum of the track and bridge offsets. */
	int shift_x = 0;
	int shift_y = 0;
	auto normalise = [&]() {
		int min_x = INT32_MAX;
		int min_y = INT32_MAX;
		for (const JunctionTrack &jt : layout.tracks) {
			min_x = std::min<int>(min_x, jt.offset.x);
			min_y = std::min<int>(min_y, jt.offset.y);
		}
		for (const JunctionBridge &bridge : layout.bridges) {
			for (const TileIndexDiffC &head : {bridge.start, bridge.end}) {
				min_x = std::min<int>(min_x, head.x);
				min_y = std::min<int>(min_y, head.y);
			}
		}
		if (min_x != INT32_MAX) {
			shift_x += min_x;
			shift_y += min_y;
		}
		NormaliseJunctionLayout(layout);
	};
	normalise();
	PlaceJunctionSignals(layout, spec);
	normalise();

	if (terrain != nullptr) {
		/* The prio rows exist only now; every track tile must be usable, in working coordinates. */
		for (const JunctionTrack &jt : layout.tracks) {
			if (jt.tracks.None()) continue;
			if (!terrain->UsableAt(jt.offset.x + shift_x, jt.offset.y + shift_y, params.plane)) return std::nullopt;
		}
	}
	if (hub != nullptr) *hub = {static_cast<int16_t>(30 - shift_x), static_cast<int16_t>(0 - shift_y)};
	return layout;
}

/** The number of middle tiles of a bridge. */
static uint BridgeMiddleTiles(const JunctionBridge &bridge)
{
	return static_cast<uint>(std::abs(bridge.end.x - bridge.start.x) + std::abs(bridge.end.y - bridge.start.y)) - 1;
}

/** The area of the largest axis-aligned rectangle of empty tiles in the layout's bounding box (D76). */
static int LargestOpenRectangle(const JunctionLayout &layout)
{
	const int w = layout.size.x;
	const int h = layout.size.y;
	if (w <= 0 || h <= 0) return 0;
	std::vector<uint8_t> used(static_cast<size_t>(w) * h, 0);
	auto mark = [&](int x, int y) {
		if (x >= 0 && x < w && y >= 0 && y < h) used[static_cast<size_t>(y) * w + x] = 1;
	};
	for (const JunctionTrack &jt : layout.tracks) {
		if (!jt.tracks.None()) mark(jt.offset.x, jt.offset.y);
	}
	for (const JunctionBridge &bridge : layout.bridges) {
		const int x0 = std::min<int>(bridge.start.x, bridge.end.x);
		const int x1 = std::max<int>(bridge.start.x, bridge.end.x);
		const int y0 = std::min<int>(bridge.start.y, bridge.end.y);
		const int y1 = std::max<int>(bridge.start.y, bridge.end.y);
		for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) mark(x, y);
	}

	int best = 0;
	std::vector<int> heights(w, 0);
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) heights[x] = used[static_cast<size_t>(y) * w + x] ? 0 : heights[x] + 1;
		std::vector<int> stack; ///< Column indices with increasing heights.
		for (int x = 0; x <= w; x++) {
			const int cur = x < w ? heights[x] : 0;
			while (!stack.empty() && heights[stack.back()] >= cur) {
				const int height = heights[stack.back()];
				stack.pop_back();
				const int left = stack.empty() ? -1 : stack.back();
				best = std::max(best, height * (x - left - 1));
			}
			stack.push_back(x);
		}
	}
	return best;
}

/** Generator score of a layout: cost, then open space (D76). Lower is better. */
std::pair<Money, int> ScoreJunctionLayout(const JunctionLayout &layout, const JunctionSpec &spec)
{
	int track_bits = 0;
	for (const JunctionTrack &jt : layout.tracks) track_bits += static_cast<int>(jt.tracks.Count());
	Money cost = spec.prices.rail_piece * track_bits + spec.prices.signal * static_cast<int>(layout.signals.size());
	for (const JunctionBridge &bridge : layout.bridges) cost += spec.prices.bridge(BridgeMiddleTiles(bridge));
	return {cost, -LargestOpenRectangle(layout)};
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

/**
 * Generate a junction: sweep candidates, keep the rule-clean ones and pick the best by score.
 * @param spec The junction spec.
 * @return The best layout, or NoLayout when no candidate is clean.
 */
JunctionGenerateResult GenerateJunction(const JunctionSpec &spec, const JunctionTerrain *terrain)
{
	/* With no gap row the W prio has nowhere to go: the row inside the W track is the E track, and the joiner blocks the outside. */
	if (spec.ml_gap == 0) return {std::nullopt, JunctionGenerateError::NeedsMlGap};

	std::vector<int> planes{0};
	if (terrain != nullptr) {
		planes = JunctionTerrainPlanes(*terrain);
		if (planes.empty()) return {std::nullopt, JunctionGenerateError::NoLayout};
	}

	std::optional<JunctionLayout> best;
	std::pair<Money, int> best_score{};
	int best_area = 0;
	TileIndexDiffC best_hub{};

	const int first = spec.ml_gap + 5;
	const int last = spec.ml_gap + 20;
	for (const int plane : planes) {
		int stop_after = INT32_MAX;
		for (int height = first; height <= last && height <= stop_after; height++) {
			bool clean_here = false;
			for (Money crossing_cost : {Money(0), spec.prices.bridge(3)}) {
				for (uint8_t slack = 0; slack <= 3; slack++) {
					JunctionCandidateParams params{static_cast<uint8_t>(height), crossing_cost, slack};
					params.plane = plane;
					Money terrain_cost = 0;
					TileIndexDiffC hub{};
					std::optional<JunctionLayout> layout = BuildJunctionCandidate(spec, params, terrain, &terrain_cost, &hub);
					if (!layout.has_value() || !CheckJunctionRules(*layout, spec).empty()) continue;
					clean_here = true;
					std::pair<Money, int> score = ScoreJunctionLayout(*layout, spec);
					score.first += terrain_cost;
					const int area = layout->size.x * layout->size.y;
					if (!best.has_value() || score < best_score || (score == best_score && area < best_area)) {
						best = std::move(layout);
						best_hub = hub;
						best_score = score;
						best_area = area;
					}
				}
			}
			if (clean_here && stop_after == INT32_MAX) stop_after = height + 2;
		}
	}
	if (!best.has_value()) return {std::nullopt, JunctionGenerateError::NoLayout};
	return {std::move(best), JunctionGenerateError::None, best_hub};
}

std::vector<JunctionLibraryEntry> BuildJunctionLibrary(const JunctionSpec &spec)
{
	std::vector<JunctionLibraryEntry> library;
	if (spec.ml_gap == 0) return library;

	std::set<std::string> seen;
	const int first = spec.ml_gap + 5;
	const int last = spec.ml_gap + 20;
	for (int height = first; height <= last; height++) {
		for (Money crossing_cost : {Money(0), spec.prices.bridge(3)}) {
			for (uint8_t slack = 0; slack <= 3; slack++) {
				const JunctionCandidateParams params{static_cast<uint8_t>(height), crossing_cost, slack};
				TileIndexDiffC hub{};
				std::optional<JunctionLayout> layout = BuildJunctionCandidate(spec, params, nullptr, nullptr, &hub);
				if (!layout.has_value() || !CheckJunctionRules(*layout, spec).empty()) continue;
				if (!seen.insert(JunctionLayoutToJson(*layout)).second) continue;
				JunctionLibraryEntry entry;
				entry.hub = hub;
				entry.score = ScoreJunctionLayout(*layout, spec);
				entry.area = layout->size.x * layout->size.y;
				entry.layout = std::move(*layout);
				library.push_back(std::move(entry));
			}
		}
	}
	return library;
}

std::optional<JunctionLibraryFit> FitJunctionLibrary(const std::vector<JunctionLibraryEntry> &library, const JunctionTerrain &terrain)
{
	const std::vector<int> planes = JunctionTerrainPlanes(terrain);
	std::optional<JunctionLibraryFit> best;
	std::tuple<Money, int, int> best_key{};
	for (size_t i = 0; i < library.size(); i++) {
		const JunctionLibraryEntry &e = library[i];
		const int ox = 30 - e.hub.x;
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
			const std::tuple<Money, int, int> key{e.score.first + cost, e.score.second, e.area};
			if (!best.has_value() || key < best_key) {
				best = JunctionLibraryFit{i, plane, cost};
				best_key = key;
			}
		}
	}
	return best;
}
