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
#include "junction_route.h"
#include "junction_router.h"
#include "junction_signal_pass.h"
#include "junction_throughput.h"
#include "track_func.h"
#include "thread.h"

#include <algorithm>
#include <atomic>
#include <functional>
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
 * Build one generator candidate with a fixed lane gap: route the branch movements, double the mainline bridges, crop, and place signals.
 * @param spec The junction spec.
 * @param params The swept inputs.
 * @param terrain Optional terrain snapshot in the generator's working coordinates (see the header).
 * @param terrain_cost Receives the terrain cost when terrain is set.
 * @param gap 0 puts lane A's RFL right after its far head and lane B's near head right after its S-bend; 1 adds one plain tile to each (D110).
 * @param sb_back For the parallelogram style, the shift-back S-bend starts this many tiles upstream of the N->W window's far end (D110 4).
 * @return The layout, or std::nullopt when this candidate cannot be built.
 */
static std::optional<JunctionLayout> BuildJunctionCandidateWithGap(const JunctionSpec &spec, const JunctionCandidateParams &params, const JunctionTerrain *terrain, Money *terrain_cost, TileIndexDiffC *hub, int gap, int sb_back)
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
		request.goals = PlainRowPieces(l, 0, Trackdir::X_SW, std::max(xO, x_s + cover + 1 + params.e_slack), frame.width - 3);
		const std::optional<JunctionRaster> n_to_e = RouteAndAdd(l, request, params.route_cache);
		if (!n_to_e.has_value()) return false;
		if (note) note_crossings(*n_to_e);

		request = base;
		request.starts = stub_pieces(xI, Trackdir::Y_NW, ys + 1);
		request.goals = PlainRowPieces(l, nw_row, Trackdir::X_NE, nw_min_x, nw_max_x);
		return RouteFencedAndAdd(l, request, branch_fence, params.route_cache).has_value();
	};
	if (!route_movements(layout, true, {}, xI - 2, frame.width - 3, true, w_row, 2, -1)) return std::nullopt;

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
		const int nw_row = params.lane_style == 1 ? b_row : w_row;
		auto nw_max_for = [&](int merge) { return std::min(xI - 2, merge - static_cast<int>(spec.prio_tiles) - params.w_slack); };
		JunctionRaster raster_a_style1;
		JunctionRaster raster_b;
		std::optional<int> merge_x;
		int x_sb = 0;
		int nw_max_x = 0;
		if (params.lane_style == 1) {
			/* Parallelogram (D105): lane A runs straight on the W row through its bridge, then S-bends onto the B row. */
			std::optional<JunctionRaster> found_a;
			for (int k = 0; k <= frame.width - 1 && !found_a.has_value(); k++) {
				const JunctionRaster raster = RasterizeJunctionRoute({tile(frame.width - 1, w_row), Trackdir::X_NE, std::string(k, 'F') + span + (gap == 0 ? "" : "F") + "RFL"});
				if (!raster.error.empty()) continue;
				size_t near_head = raster.pieces.size();
				size_t far_head = 0;
				for (size_t i = 0; i < raster.pieces.size(); i++) {
					if (!raster.pieces[i].on_bridge) continue;
					near_head = std::min(near_head, i);
					far_head = i;
				}
				if (near_head >= raster.pieces.size()) continue;
				if (raster.pieces[near_head].tile.x != h_e || raster.pieces[near_head].tile.y != w_row) continue;
				if (raster.pieces[far_head].tile.x != h_w || raster.pieces[far_head].tile.y != w_row) continue;
				if (raster.pieces.back().tile.y != b_row) continue;
				found_a = raster;
			}
			if (!found_a.has_value()) return std::nullopt;
			raster_a_style1 = std::move(*found_a);
			merge_x = raster_a_style1.pieces.back().tile.x;
			nw_max_x = nw_max_for(*merge_x);
			x_sb = nw_max_x - sb_back;
			if (x_sb < 2) return std::nullopt;
		}
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

				if (params.lane_style == 1) {
					/* Straight on the B row to the tile at x_sb + 1, then LFR up to the W row and straight on to x = 0. */
					const std::string head_moves = std::string(p, 'F') + "RFL" + std::string(q, 'F') + span;
					for (int t = 0; t <= frame.width - 1 && !lane_b.has_value(); t++) {
						const JunctionRaster to_sb = RasterizeJunctionRoute({tile(frame.width - 1, w_row), Trackdir::X_NE, head_moves + std::string(t, 'F')});
						if (!to_sb.error.empty()) continue;
						if (to_sb.pieces.back().tile.x != x_sb + 1 || to_sb.pieces.back().tile.y != b_row) continue;
						for (int s = 0; s <= frame.width - 1; s++) {
							const JunctionRoute candidate{tile(frame.width - 1, w_row), Trackdir::X_NE, head_moves + std::string(t, 'F') + "LFR" + std::string(s, 'F')};
							const JunctionRaster raster = RasterizeJunctionRoute(candidate);
							if (!raster.error.empty()) continue;
							if (raster.pieces.back().tile.x == 0 && raster.pieces.back().tile.y == w_row) {
								lane_b = candidate;
								break;
							}
						}
					}
					continue;
				}
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

		if (params.lane_style == 0) {
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
		if (params.lane_style == 1) {
			/* Lane A's RFL must land on a B row tile where lane B has an axis piece. */
			const bool merges = std::ranges::any_of(raster_b.pieces, [&](const JunctionPiece &piece) {
				return !piece.on_bridge && piece.tile.x == *merge_x && piece.tile.y == b_row && IsDiagonalTrackdir(piece.trackdir);
			});
			if (!merges) return std::nullopt;
			AddRasterToLayout(layout, raster_a_style1);
			AddRasterToLayout(layout, raster_b);
		}
		/* Re-route the four movements with the lanes fixed and no crossable tiles. */
		std::vector<TileIndexDiffC> under_spans;
		for (int x = h_w + 1; x < h_e; x++) {
			under_spans.push_back(tile(x, params.lane_style == 1 ? w_row : w_row - 1));
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
		if (params.lane_style == 0) nw_max_x = nw_max_for(*merge_x);
		if (!route_movements(layout, false, under_spans, nw_max_x, frame.width - 3, false, nw_row, params.lane_style == 1 ? x_sb + 1 : 2, x_ls + 3)) return std::nullopt;
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
	if (hub != nullptr) *hub = {static_cast<int16_t>(frame.hub_x - shift_x), static_cast<int16_t>(0 - shift_y)};
	return layout;
}

/**
 * Build one generator candidate: the tight variant first (D110, gap 0); when that is not rule-clean, the variant with one more plain tile
 * after lane B's S-bend and before lane A's S-bend (gap 1) is built once and returned instead.
 * For the parallelogram style the shift-back is tried tightest first (D110 4): the first rule-clean shift-back wins.
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
	if (params.lane_style != 1) {
		std::optional<JunctionLayout> single = attempt(6);
		if (out_clean != nullptr) *out_clean = clean;
		return single;
	}
	std::optional<JunctionLayout> result;
	for (int sb_back = 1; sb_back <= 6; sb_back++) {
		result = attempt(sb_back);
		if (clean) break;
	}
	if (out_clean != nullptr) *out_clean = clean;
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
			for (Money crossing_cost : {Money(0), spec.prices.bridge(3)}) {
				for (uint8_t e_slack = 0; e_slack <= 3; e_slack++) {
					for (uint8_t w_slack = 0; w_slack <= 3; w_slack++) {
						for (uint8_t margin = 1; margin <= 3; margin++) {
							for (uint8_t style = 0; style <= 1; style++) {
								JunctionCandidateParams params{static_cast<uint8_t>(height), crossing_cost, e_slack, w_slack};
								params.plane = plane;
								params.route_cache = &route_cache;
								params.bridge_margin = margin;
								params.lane_style = style;
								all_params.push_back(params);
							}
						}
					}
				}
			}
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
		for (Money crossing_cost : {Money(0), spec.prices.bridge(3)}) {
			for (uint8_t e_slack = 0; e_slack <= 3; e_slack++) {
				for (uint8_t w_slack = 0; w_slack <= 3; w_slack++) {
					for (uint8_t margin = 1; margin <= 3; margin++) {
						for (uint8_t style = 0; style <= 1; style++) {
							JunctionCandidateParams params{static_cast<uint8_t>(height), crossing_cost, e_slack, w_slack};
							params.bridge_margin = margin;
							params.lane_style = style;
							params.route_cache = &route_cache;
							all_params.push_back(params);
						}
					}
				}
			}
		}
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
