/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_generator.cpp Junction generator: spec, notation parsing and curve spacing. */

#include "stdafx.h"

#include "junction_generator.h"
#include "core/math_func.hpp"
#include "junction_route.h"
#include "junction_router.h"
#include "junction_signal_pass.h"
#include "track_func.h"

#include <algorithm>

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

JunctionGenerateResult GenerateJunction([[maybe_unused]] const JunctionSpec &spec)
{
	return {std::nullopt, JunctionGenerateError::NoLayout};
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
 * @return The layout, or std::nullopt when this candidate cannot be built.
 */
std::optional<JunctionLayout> BuildJunctionCandidate(const JunctionSpec &spec, const JunctionCandidateParams &params)
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
	base.min = tile(xI - 16, 0);
	base.max = tile(xO + 16, yb);
	base.max_expansions = 50000;
	base.curve_spacing = spec.curve_limit ? DeriveCurveSpacing(spec.reference_speed, spec.curve_speed, spec.tl) : 1;
	base.s_bend_window = 2u * spec.tl;
	base.bridge_span_ok = spec.bridge_span_ok;
	base.max_bridge_middle = 10;
	base.prices = spec.prices;
	base.crossable_cost = 0;

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
	auto route_movements = [&](JunctionLayout &l, bool crossable_w, const std::vector<TileIndexDiffC> &under_spans, int nw_max_x, bool note) -> bool {
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
		for (const JunctionRouteEnd &end : PlainRowPieces(l, b_row, Trackdir::X_NE, xO + 2, 61)) request.starts.push_back(end);
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
	if (!route_movements(layout, true, {}, xI - 2, true)) return std::nullopt;

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
		if (!route_movements(layout, false, under_spans, std::min(xI - 2, *merge_x - 1 - params.joiner_slack), false)) return std::nullopt;
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

	NormaliseJunctionLayout(layout);
	PlaceJunctionSignals(layout, spec);
	NormaliseJunctionLayout(layout);
	return layout;
}
