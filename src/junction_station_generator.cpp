/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_station_generator.cpp Station generator (D106). */

#include "stdafx.h"

#include "junction_station_generator.h"
#include "junction_route.h"
#include "junction_router.h"
#include "junction_rules_internal.h"
#include "direction_func.h"
#include "map_func.h"
#include "track_func.h"

#include <algorithm>

#include "safeguards.h"

static TileIndexDiffC Tile(int x, int y)
{
	return {static_cast<int16_t>(x), static_cast<int16_t>(y)};
}

static JunctionPiece MakePiece(TileIndexDiffC tile, Trackdir trackdir)
{
	return {tile, trackdir, static_cast<uint8_t>(IsDiagonalTrackdir(trackdir) ? 2 : 1), false};
}

static bool InBlock(const JunctionStationBlock &block, TileIndexDiffC tile)
{
	return tile.x >= block.origin.x && tile.x < block.origin.x + block.length && tile.y >= block.origin.y && tile.y < block.origin.y + block.platforms;
}

/** The pieces a train on the piece can continue on, over the tracks present. */
static std::vector<JunctionPiece> Successors(const JunctionLayout &layout, const JunctionPiece &cur)
{
	std::vector<JunctionPiece> result;
	const TileIndexDiffC step = TileIndexDiffCByDiagDir(TrackdirToExitdir(cur.trackdir));
	const TileIndexDiffC tile = Tile(cur.tile.x + step.x, cur.tile.y + step.y);
	const TrackBits tracks = TracksAt(layout, tile);
	for (DirDiff diff : {DirDiff::Left45, DirDiff::Same, DirDiff::Right45}) {
		Trackdir next;
		if (JunctionNextTrackdir(cur.trackdir, diff, next) && tracks.Test(TrackdirToTrack(next))) result.push_back(MakePiece(tile, next));
	}
	return result;
}

/** Whether another track on the piece's tile leaves in the same direction and reaches the next piece. */
static bool IsMergePiece(const JunctionLayout &layout, const JunctionPiece &cur, const JunctionPiece &next)
{
	for (Track track : TracksAt(layout, cur.tile)) {
		for (Trackdir other : TrackToTrackdirBits(track)) {
			if (other == cur.trackdir || TrackdirToExitdir(other) != TrackdirToExitdir(cur.trackdir)) continue;
			if (TrackdirReachesTrackdirs(other).Test(next.trackdir)) return true;
		}
	}
	return false;
}

static void AddStraight(JunctionLayout &layout, int x0, int x1, int y)
{
	JunctionRaster raster;
	for (int x = x0; x <= x1; x++) raster.pieces.push_back(MakePiece(Tile(x, y), Trackdir::X_SW));
	AddRasterToLayout(layout, raster);
}

/** The route request fields every fan shares, set as BuildJunctionCandidate's base request sets them. */
static JunctionRouteRequest BaseRequest(const JunctionSpec &spec)
{
	JunctionRouteRequest request{};
	request.curve_spacing = spec.curve_limit ? DeriveCurveSpacing(spec.reference_speed, spec.curve_speed, spec.tl) : 1;
	request.s_bend_window = 2u * spec.tl;
	request.bridge_span_ok = [](uint) { return false; };
	request.max_bridge_middle = 0;
	request.prices = spec.prices;
	request.crossable_cost = 0;
	request.turn_cost = spec.prices.rail_piece / 2;
	return request;
}

/** Route one fan branch, add it to the layout and keep its raster. */
static bool RouteAndAdd(JunctionLayout &layout, const JunctionRouteRequest &request, std::vector<JunctionRaster> &rasters)
{
	const std::optional<JunctionRoute> route = FindJunctionRoute(layout, request);
	if (!route.has_value()) return false;
	JunctionRaster raster = RasterizeJunctionRoute(*route);
	if (!raster.error.empty()) return false;
	AddRasterToLayout(layout, raster);
	rasters.push_back(std::move(raster));
	return true;
}

/**
 * Drag-signal fill (D97) along a path, from an anchor that already holds a signal.
 * Each signal goes where the logical length from the previous one reaches the density exactly, or on the last legal piece before it.
 * @param pieces The path in the direction of travel.
 * @param anchor Index of the piece holding the anchor signal, or of the last piece before a platform when the anchor is virtual.
 * @param upstream Whether to walk towards the start of the path (otherwise towards its end).
 * @param virtual_anchor Whether the anchor holds no signal: the count then starts at its downstream edge and the anchor piece is a candidate itself.
 */
static void FillSignals(JunctionLayout &layout, const JunctionSpec &spec, const std::vector<JunctionPiece> &pieces, size_t anchor, bool upstream, bool virtual_anchor = false)
{
	const JunctionStationBlock block = layout.stations.front();
	const uint density = spec.sd * 192u;
	const ptrdiff_t dir = upstream ? -1 : 1;
	uint count = 0;
	std::optional<ptrdiff_t> last;
	auto legal = [&](const JunctionPiece &piece) { return !InBlock(block, piece.tile) && IsLegalSignalPiece(layout, piece); };
	for (ptrdiff_t j = static_cast<ptrdiff_t>(anchor) + (virtual_anchor ? 0 : dir); j >= 0 && j < static_cast<ptrdiff_t>(pieces.size()); j += dir) {
		if (j != static_cast<ptrdiff_t>(anchor)) count += JunctionSignalWeight(upstream ? pieces[j + 1] : pieces[j]);
		ptrdiff_t at = j;
		if (count == density && legal(pieces[j])) {
			/* Exact hit. */
		} else if (count >= density && last.has_value()) {
			at = *last;
		} else {
			if (count < density && legal(pieces[j])) last = j;
			continue;
		}
		layout.signals.push_back({pieces[at].tile, pieces[at].trackdir, spec.signal_type, SignalVariant::Electric});
		j = at;
		count = 0;
		last.reset();
	}
}

/** A candidate station: the layout and its ports before normalising. */
struct StationCandidate {
	JunctionLayout layout;
	JunctionStationPorts ports;
};

/** Build the station with the platform block at x = p0, or nothing when a fan cannot be routed. */
static std::optional<StationCandidate> BuildStation(const JunctionSpec &spec, uint8_t platforms, int b, int p0)
{
	const int tl = spec.tl;
	const int n = platforms;
	const int east = p0 + tl - 1; ///< The platforms' last x.
	StationCandidate c;
	JunctionLayout &layout = c.layout;
	std::vector<JunctionRaster> rasters;

	/* The in line and the platforms. */
	AddStraight(layout, 0, east, 0);
	for (int y = 1; y < n; y++) AddStraight(layout, p0, east, y);
	layout.stations.push_back({Tile(p0, 0), Axis::X, platforms, spec.tl});

	/* The entry fan. */
	for (int k = n - 1; k >= 1; k--) {
		JunctionRouteRequest request = BaseRequest(spec);
		request.min = Tile(0, 0);
		request.max = Tile(p0, n - 1);
		request.goals = {{Tile(p0, k), Trackdir::X_SW}};
		/* A ladder: the westmost split that routes feeds the farthest platform. */
		bool routed = false;
		for (int x = b; x < p0 && !routed; x++) {
			if (TracksAt(layout, Tile(x, 0)) != TrackBits{Track::X}) continue;
			request.starts = {{Tile(x, 0), Trackdir::X_SW}};
			routed = RouteAndAdd(layout, request, rasters);
		}
		if (!routed) return std::nullopt;
	}

	/* The exit fan, routed over a long row 0 that is cut back to the out stub afterwards. */
	const int far = east + 6 * n + 10;
	JunctionLayout scratch = layout;
	AddStraight(scratch, east + 1, far, 0);
	std::vector<JunctionRaster> exits;
	for (int k = 1; k < n; k++) {
		JunctionRouteRequest request = BaseRequest(spec);
		request.min = Tile(east, 0);
		request.max = Tile(far, n - 1);
		request.starts = {{Tile(east, k), Trackdir::X_SW}};
		for (int x = east + 1; x <= far - 2; x++) request.goals.push_back({Tile(x, 0), Trackdir::X_SW});
		if (!RouteAndAdd(scratch, request, exits)) return std::nullopt;
	}
	int last_goal = east + 1;
	for (const JunctionRaster &raster : exits) {
		AddRasterToLayout(layout, raster);
		last_goal = std::max<int>(last_goal, raster.pieces.back().tile.x);
	}
	AddStraight(layout, east + 1, last_goal + 1, 0); /* The out stub; the last merge tile is last_goal - 1. */

	/* Signals: the entry before the first split, platform exits, and after the last merge. */
	const JunctionStationBlock block = layout.stations.front();
	auto legal = [&](const JunctionPiece &piece) { return !InBlock(block, piece.tile) && IsLegalSignalPiece(layout, piece); };
	auto add_signal = [&](const JunctionPiece &piece) {
		layout.signals.push_back({piece.tile, piece.trackdir, spec.signal_type, SignalVariant::Electric});
	};

	std::vector<JunctionPiece> in_stub = {MakePiece(Tile(0, 0), Trackdir::X_SW)};
	for (;;) {
		const std::vector<JunctionPiece> next = Successors(layout, in_stub.back());
		if (next.size() != 1) break;
		in_stub.push_back(next.front());
	}
	if (Successors(layout, in_stub.back()).size() < 2 || !legal(in_stub.back())) return std::nullopt;
	add_signal(in_stub.back());

	for (int k = 0; k < n; k++) {
		JunctionPiece cur = MakePiece(Tile(east, k), Trackdir::X_SW);
		for (;;) {
			const std::vector<JunctionPiece> next = Successors(layout, cur);
			if (next.empty() || IsMergePiece(layout, cur, next.front())) break;
			if (IsSignalledPiece(layout, next.front())) break;
			if (legal(next.front())) {
				add_signal(next.front());
				break;
			}
			cur = next.front();
		}
	}

	std::vector<JunctionPiece> out_stub;
	for (int x = last_goal; x <= last_goal + 1; x++) out_stub.push_back(MakePiece(Tile(x, 0), Trackdir::X_SW));
	if (!legal(out_stub.front())) return std::nullopt;
	add_signal(out_stub.front());

	FillSignals(layout, spec, in_stub, in_stub.size() - 1, true);
	FillSignals(layout, spec, out_stub, 0, false);

	/* Every entry fan path, from the entry signal to the platform, filled back from the platform entrance. */
	std::vector<std::vector<JunctionPiece>> paths;
	std::vector<JunctionPiece> path = {in_stub.back()};
	auto collect = [&](auto &&self, auto &&at_end) -> void {
		const std::vector<JunctionPiece> next = Successors(layout, path.back());
		if (next.empty() || at_end(path.back())) {
			paths.push_back(path);
			return;
		}
		for (const JunctionPiece &piece : next) {
			path.push_back(piece);
			self(self, at_end);
			path.pop_back();
		}
	};
	collect(collect, [&](const JunctionPiece &piece) { return InBlock(block, piece.tile); });
	for (std::vector<JunctionPiece> &p : paths) {
		while (!p.empty() && InBlock(block, p.back().tile)) p.pop_back();
		if (p.size() > 1) FillSignals(layout, spec, p, p.size() - 1, true, true);
	}

	/* Every exit fan path, from the platform exit signal to the out signal, filled back from the out signal. */
	paths.clear();
	for (int k = 0; k < n; k++) {
		JunctionPiece cur = MakePiece(Tile(east, k), Trackdir::X_SW);
		path = {cur};
		for (;;) {
			const std::vector<JunctionPiece> next = Successors(layout, path.back());
			if (next.empty()) break;
			path.push_back(next.front());
			if (IsSignalledPiece(layout, next.front())) break;
		}
		/* path.back() is the platform exit signal, or the track ran out. */
		std::vector<JunctionPiece> tail = {path.back()};
		auto walk_exit = [&](auto &&self) -> void {
			if (tail.back().tile.x == out_stub.front().tile.x && tail.back().tile.y == 0) {
				paths.push_back(tail);
				return;
			}
			for (const JunctionPiece &piece : Successors(layout, tail.back())) {
				tail.push_back(piece);
				self(self);
				tail.pop_back();
			}
		};
		walk_exit(walk_exit);
	}
	for (const std::vector<JunctionPiece> &p : paths) {
		if (p.size() > 1) FillSignals(layout, spec, p, p.size() - 1, true);
	}

	c.ports.in = {Tile(0, 0), Trackdir::X_SW};
	c.ports.out = {out_stub.back().tile, Trackdir::X_SW};
	return c;
}

JunctionStationResult GenerateStation(const JunctionSpec &spec, uint8_t platforms)
{
	JunctionStationResult result;
	if (platforms < 2 || platforms > 8) return result;
	const int b = spec.tl; ///< Braking space before the first split (S19).
	for (int p0 = b + 2; p0 <= b + 4 * platforms; p0++) {
		std::optional<StationCandidate> c = BuildStation(spec, platforms, b, p0);
		if (!c.has_value()) continue;

		int min_x = INT16_MAX;
		int min_y = INT16_MAX;
		for (const JunctionTrack &jt : c->layout.tracks) {
			min_x = std::min<int>(min_x, jt.offset.x);
			min_y = std::min<int>(min_y, jt.offset.y);
		}
		NormaliseJunctionLayout(c->layout);
		for (JunctionRouteEnd *end : {&c->ports.in, &c->ports.out}) {
			end->tile = Tile(end->tile.x - min_x, end->tile.y - min_y);
		}
		if (!CheckStationRules(c->layout, c->ports, spec.tl, spec.sd).empty()) continue;

		result.layout = std::move(c->layout);
		result.ports = c->ports;
		return result;
	}
	return result;
}
