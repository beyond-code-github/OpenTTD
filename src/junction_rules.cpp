/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_rules.cpp Junction rule checker. */

#include "stdafx.h"

#include "junction_rules.h"
#include "core/math_func.hpp"
#include "direction_func.h"
#include "map_func.h"
#include "track_func.h"

#include <algorithm>
#include <map>
#include <set>

#include "safeguards.h"

/** The most pieces a traced path may have before it is given up on. */
static constexpr size_t MAX_PATH_PIECES = 4096;

/** One movement through the junction. */
using Movement = std::pair<JunctionPortRole, JunctionPortRole>;

/** A switch on a path: whether it is a split, and the index of the switch tile's piece. */
using SwitchEvent = std::pair<bool, size_t>;

static bool SameTile(TileIndexDiffC a, TileIndexDiffC b)
{
	return a.x == b.x && a.y == b.y;
}

static TileIndexDiffC AddTiles(TileIndexDiffC a, TileIndexDiffC b)
{
	return {static_cast<int16_t>(a.x + b.x), static_cast<int16_t>(a.y + b.y)};
}

static bool InBox(const JunctionLayout &layout, TileIndexDiffC tile)
{
	return tile.x >= 0 && tile.y >= 0 && tile.x < layout.size.x && tile.y < layout.size.y;
}

/** The track bits of all layout entries on a tile. */
static TrackBits TracksAt(const JunctionLayout &layout, TileIndexDiffC tile)
{
	TrackBits tracks{};
	for (const JunctionTrack &jt : layout.tracks) {
		if (SameTile(jt.offset, tile)) tracks = tracks | jt.tracks;
	}
	return tracks;
}

/** Both trackdirs of every track in the bits. */
static TrackdirBits TrackdirsOf(TrackBits tracks)
{
	TrackdirBits bits{};
	for (Track track : tracks) bits = bits | TrackToTrackdirBits(track);
	return bits;
}

/** Whether a track touches the given tile edge, that is, one of its trackdirs exits through it. */
static bool TrackTouches(Track track, DiagDirection edge)
{
	const Trackdir td = TrackToTrackdir(track);
	return TrackdirToExitdir(td) == edge || TrackdirToExitdir(ReverseTrackdir(td)) == edge;
}

/** Whether any of the tracks touches the given tile edge. */
static bool AnyTrackTouches(TrackBits tracks, DiagDirection edge)
{
	for (Track track : tracks) {
		if (TrackTouches(track, edge)) return true;
	}
	return false;
}

static TileIndexDiffC NextTile(TileIndexDiffC tile, Trackdir td)
{
	return AddTiles(tile, TileIndexDiffCByDiagDir(TrackdirToExitdir(td)));
}

static uint8_t UnitsOf(Trackdir td)
{
	return IsDiagonalTrackdir(td) ? 2 : 1;
}

/** The bridge that has the tile as one of its heads, or nullptr. */
static const JunctionBridge *FindBridgeByHead(const JunctionLayout &layout, TileIndexDiffC tile)
{
	for (const JunctionBridge &bridge : layout.bridges) {
		if (SameTile(bridge.start, tile) || SameTile(bridge.end, tile)) return &bridge;
	}
	return nullptr;
}

static int SignOf(int value)
{
	return value < 0 ? -1 : value > 0 ? 1 : 0;
}

static const char *RoleName(JunctionPortRole role)
{
	switch (role) {
		case JunctionPortRole::EIn: return "EIn";
		case JunctionPortRole::EOut: return "EOut";
		case JunctionPortRole::WIn: return "WIn";
		case JunctionPortRole::WOut: return "WOut";
		case JunctionPortRole::NIn: return "NIn";
		case JunctionPortRole::SOut: return "SOut";
		default: NOT_REACHED();
	}
}

static bool IsInRole(JunctionPortRole role)
{
	return role == JunctionPortRole::EIn || role == JunctionPortRole::WIn || role == JunctionPortRole::NIn;
}

/**
 * Collect the tiles on one edge of the box that hold tracks touching that edge.
 * @param layout The layout.
 * @param edge The edge to look at: NE is x = 0, SW is x = max, SE is y = max.
 * @return Map from the coordinate along the edge to the tracks touching it.
 */
static std::map<int, TrackBits> EdgeTiles(const JunctionLayout &layout, DiagDirection edge)
{
	std::map<int, TrackBits> result;
	for (const JunctionTrack &jt : layout.tracks) {
		bool on_edge = false;
		int along = 0;
		switch (edge) {
			case DiagDirection::NE: on_edge = jt.offset.x == 0; along = jt.offset.y; break;
			case DiagDirection::SW: on_edge = jt.offset.x == layout.size.x - 1; along = jt.offset.y; break;
			case DiagDirection::SE: on_edge = jt.offset.y == layout.size.y - 1; along = jt.offset.x; break;
			default: NOT_REACHED();
		}
		if (!on_edge) continue;
		for (Track track : jt.tracks) {
			if (TrackTouches(track, edge)) result[along].Set(track);
		}
	}
	return result;
}

/**
 * Find the ports of a layout in the canonical right-hand frame.
 * An edge that does not have exactly two port tiles, each holding only the expected axis track, gives no ports.
 * @param layout The layout.
 * @return The ports found.
 */
std::vector<JunctionPort> FindJunctionPorts(const JunctionLayout &layout)
{
	std::vector<JunctionPort> ports;

	auto add_edge = [&](DiagDirection edge, Track axis, JunctionPortRole first, Trackdir first_td, JunctionPortRole second, Trackdir second_td) {
		const std::map<int, TrackBits> tiles = EdgeTiles(layout, edge);
		if (tiles.size() != 2) return;
		for (const auto &[along, tracks] : tiles) {
			if (tracks != TrackBits{axis}) return;
		}
		auto it = tiles.begin();
		const bool vertical = edge == DiagDirection::SE; // Along x instead of y.
		auto tile_at = [&](int along) -> TileIndexDiffC {
			if (vertical) return {static_cast<int16_t>(along), static_cast<int16_t>(layout.size.y - 1)};
			return {static_cast<int16_t>(edge == DiagDirection::NE ? 0 : layout.size.x - 1), static_cast<int16_t>(along)};
		};
		ports.push_back({first, tile_at(it->first), first_td});
		++it;
		ports.push_back({second, tile_at(it->first), second_td});
	};

	add_edge(DiagDirection::NE, Track::X, JunctionPortRole::EIn, Trackdir::X_SW, JunctionPortRole::WOut, Trackdir::X_NE);
	add_edge(DiagDirection::SW, Track::X, JunctionPortRole::EOut, Trackdir::X_SW, JunctionPortRole::WIn, Trackdir::X_NE);
	add_edge(DiagDirection::SE, Track::Y, JunctionPortRole::NIn, Trackdir::Y_NW, JunctionPortRole::SOut, Trackdir::Y_SE);
	return ports;
}

static void AddTraceProblem(JunctionCheckContext &ctx, TileIndexDiffC tile, std::string message)
{
	const bool present = std::ranges::any_of(ctx.trace_problems, [&](const RuleViolation &v) {
		return SameTile(v.tile, tile) && v.message == message;
	});
	if (!present) ctx.trace_problems.push_back({JunctionRule::Connectivity, tile, std::move(message)});
}

static bool IsOnPath(const std::vector<JunctionPiece> &path, TileIndexDiffC tile, Trackdir td)
{
	return std::ranges::any_of(path, [&](const JunctionPiece &piece) { return SameTile(piece.tile, tile) && piece.trackdir == td; });
}

/**
 * Depth-first search for all paths from the last piece of the given path.
 * @param ctx The context to add paths and problems to.
 * @param from The role of the In port the search started at.
 * @param path The pieces so far; never empty.
 */
static void TraceFrom(JunctionCheckContext &ctx, JunctionPortRole from, std::vector<JunctionPiece> &path)
{
	const JunctionPiece cur = path.back();
	if (path.size() >= MAX_PATH_PIECES) {
		AddTraceProblem(ctx, cur.tile, "path too long");
		return;
	}

	const TileIndexDiffC next = NextTile(cur.tile, cur.trackdir);
	if (!InBox(ctx.layout, next)) {
		for (const JunctionPort &port : ctx.ports) {
			if (IsInRole(port.role) || !SameTile(port.tile, cur.tile) || port.trackdir != cur.trackdir) continue;
			ctx.paths.push_back({from, port.role, path});
			return;
		}
		AddTraceProblem(ctx, cur.tile, "unexpected exit");
		return;
	}

	/* A bridge entered from outside, along its axis. */
	if (const JunctionBridge *bridge = FindBridgeByHead(ctx.layout, next); bridge != nullptr && IsDiagonalTrackdir(cur.trackdir)) {
		const bool axis_x = TrackdirToTrack(cur.trackdir) == Track::X;
		const bool aligned = axis_x ? bridge->start.y == bridge->end.y : bridge->start.x == bridge->end.x;
		const TileIndexDiffC step = TileIndexDiffCByDiagDir(TrackdirToExitdir(cur.trackdir));
		const TileIndexDiffC along = {static_cast<int16_t>(SignOf(bridge->end.x - bridge->start.x)), static_cast<int16_t>(SignOf(bridge->end.y - bridge->start.y))};
		const bool from_start = SameTile(bridge->start, next) && step.x == along.x && step.y == along.y;
		const bool from_end = SameTile(bridge->end, next) && step.x == -along.x && step.y == -along.y;
		if (aligned && !SameTile(bridge->start, bridge->end) && (from_start || from_end)) {
			if (IsOnPath(path, next, cur.trackdir)) {
				AddTraceProblem(ctx, next, "cycle");
				return;
			}
			const TileIndexDiffC far_head = from_start ? bridge->end : bridge->start;
			const size_t old_size = path.size();
			TileIndexDiffC tile = next;
			for (;;) {
				path.push_back({tile, cur.trackdir, 2, true});
				if (SameTile(tile, far_head)) break;
				tile = AddTiles(tile, step);
			}
			TraceFrom(ctx, from, path);
			path.resize(old_size);
			return;
		}
	}

	const TrackdirBits candidates = TrackdirReachesTrackdirs(cur.trackdir) & TrackdirsOf(TracksAt(ctx.layout, next));
	if (candidates.None()) {
		AddTraceProblem(ctx, cur.tile, "dead end");
		return;
	}
	for (Trackdir td : candidates) {
		if (IsOnPath(path, next, td)) {
			AddTraceProblem(ctx, next, "cycle");
			continue;
		}
		path.push_back({next, td, UnitsOf(td), false});
		TraceFrom(ctx, from, path);
		path.pop_back();
	}
}

/**
 * Make a check context and trace all paths from the In ports.
 * @param layout The layout; must outlive the context.
 * @param spec The spec; must outlive the context.
 * @param ports The ports.
 * @param expected The movements that must exist.
 * @return The context with paths and trace problems filled in.
 */
JunctionCheckContext MakeJunctionCheckContext(const JunctionLayout &layout, const JunctionSpec &spec, std::vector<JunctionPort> ports, std::vector<std::pair<JunctionPortRole, JunctionPortRole>> expected)
{
	JunctionCheckContext ctx{layout, spec, std::move(ports), std::move(expected), {}, {}};
	for (const JunctionPort &port : ctx.ports) {
		if (!IsInRole(port.role)) continue;
		std::vector<JunctionPiece> path{{port.tile, port.trackdir, UnitsOf(port.trackdir), false}};
		TraceFrom(ctx, port.role, path);
	}
	return ctx;
}

/** The trackdirs used by the paths, per tile; bridge pieces carry no track. */
static std::map<std::pair<int, int>, TrackdirBits> UsedTrackdirs(const JunctionCheckContext &ctx)
{
	std::map<std::pair<int, int>, TrackdirBits> used;
	for (const JunctionPath &path : ctx.paths) {
		for (const JunctionPiece &piece : path.pieces) {
			if (piece.on_bridge) continue;
			used[{piece.tile.x, piece.tile.y}].Set(piece.trackdir);
		}
	}
	return used;
}

/** The tracks used by the paths, per tile. */
static std::map<std::pair<int, int>, TrackBits> UsedTracks(const JunctionCheckContext &ctx)
{
	std::map<std::pair<int, int>, TrackBits> used;
	for (const auto &[tile, trackdirs] : UsedTrackdirs(ctx)) {
		for (Trackdir td : trackdirs) used[tile].Set(TrackdirToTrack(td));
	}
	return used;
}

/**
 * The switches along a path, ordered by the index of their switch tile's piece.
 * A split is at the piece where more than one continuation exists; a merge is at the piece
 * where another track on the same tile leaves in the same direction and reaches the next piece.
 * Bridge pieces are never switches.
 */
static std::vector<SwitchEvent> SwitchEventsOf(const JunctionLayout &layout, const JunctionPath &path)
{
	std::vector<SwitchEvent> events;
	const std::vector<JunctionPiece> &pieces = path.pieces;
	for (size_t k = 0; k + 1 < pieces.size(); k++) {
		const JunctionPiece &cur = pieces[k];
		const JunctionPiece &next = pieces[k + 1];
		if (!next.on_bridge) {
			const TrackdirBits candidates = TrackdirReachesTrackdirs(cur.trackdir) & TrackdirsOf(TracksAt(layout, next.tile));
			if (candidates.Count() > 1) events.emplace_back(true, k + 1);
		}
		if (!cur.on_bridge) {
			for (Trackdir other : TrackdirsOf(TracksAt(layout, cur.tile))) {
				if (other == cur.trackdir || TrackdirToExitdir(other) != TrackdirToExitdir(cur.trackdir)) continue;
				if (!TrackdirReachesTrackdirs(other).Test(next.trackdir)) continue;
				events.emplace_back(false, k);
				break;
			}
		}
	}
	std::ranges::stable_sort(events, [](const SwitchEvent &a, const SwitchEvent &b) { return a.second < b.second; });
	return events;
}

static void AddViolation(std::vector<RuleViolation> &out, JunctionRule rule, TileIndexDiffC tile, std::string message)
{
	const bool present = std::ranges::any_of(out, [&](const RuleViolation &v) {
		return v.rule == rule && SameTile(v.tile, tile) && v.message == message;
	});
	if (!present) out.push_back({rule, tile, std::move(message)});
}

static TileIndexDiffC PortTile(const JunctionCheckContext &ctx, JunctionPortRole role)
{
	for (const JunctionPort &port : ctx.ports) {
		if (port.role == role) return port.tile;
	}
	return {0, 0};
}

/** G1: exactly the expected movements, one path each, with no dead ends, cycles or other exits. */
std::vector<RuleViolation> CheckConnectivity(const JunctionCheckContext &ctx)
{
	std::vector<RuleViolation> out = ctx.trace_problems;

	for (const auto &[from, to] : ctx.expected) {
		const auto count = std::ranges::count_if(ctx.paths, [&](const JunctionPath &path) { return path.from == from && path.to == to; });
		const std::string movement = fmt::format("{} to {}", RoleName(from), RoleName(to));
		if (count == 0) AddViolation(out, JunctionRule::Connectivity, PortTile(ctx, from), fmt::format("missing movement {}", movement));
		if (count > 1) AddViolation(out, JunctionRule::Connectivity, PortTile(ctx, from), fmt::format("ambiguous movement {}", movement));
	}

	for (const JunctionPath &path : ctx.paths) {
		if (std::ranges::find(ctx.expected, Movement{path.from, path.to}) != ctx.expected.end()) continue;
		AddViolation(out, JunctionRule::Connectivity, path.pieces.back().tile, fmt::format("unexpected movement {} to {}", RoleName(path.from), RoleName(path.to)));
	}

	std::set<JunctionPortRole> roles;
	for (const auto &[from, to] : ctx.expected) {
		roles.insert(from);
		roles.insert(to);
	}
	for (JunctionPortRole role : roles) {
		const bool present = std::ranges::any_of(ctx.ports, [&](const JunctionPort &port) { return port.role == role; });
		if (!present) AddViolation(out, JunctionRule::Connectivity, {0, 0}, fmt::format("missing port {}", RoleName(role)));
	}
	return out;
}

/** G2: no tile where paths use both X and Y, which would be a level crossing. */
std::vector<RuleViolation> CheckGeometry(const JunctionCheckContext &ctx)
{
	std::vector<RuleViolation> out;
	for (const auto &[tile, tracks] : UsedTracks(ctx)) {
		if (!tracks.Test(Track::X) || !tracks.Test(Track::Y)) continue;
		AddViolation(out, JunctionRule::Geometry, {static_cast<int16_t>(tile.first), static_cast<int16_t>(tile.second)}, "level crossing");
	}
	return out;
}

/** G4: along a path, a merge must not be followed by a split. */
std::vector<RuleViolation> CheckOrder(const JunctionCheckContext &ctx)
{
	std::vector<RuleViolation> out;
	for (const JunctionPath &path : ctx.paths) {
		const std::vector<SwitchEvent> events = SwitchEventsOf(ctx.layout, path);
		const auto first_merge = std::ranges::find_if(events, [](const SwitchEvent &e) { return !e.first; });
		if (first_merge == events.end()) continue;
		for (const SwitchEvent &event : events) {
			if (!event.first || event.second <= first_merge->second) continue;
			AddViolation(out, JunctionRule::Order, path.pieces[event.second].tile, "split after a merge");
			break;
		}
	}
	return out;
}

/** The tiles of a bridge from head to head; only valid for aligned heads. */
static std::vector<TileIndexDiffC> BridgeTiles(const JunctionBridge &bridge)
{
	std::vector<TileIndexDiffC> tiles;
	const TileIndexDiffC step = {static_cast<int16_t>(SignOf(bridge.end.x - bridge.start.x)), static_cast<int16_t>(SignOf(bridge.end.y - bridge.start.y))};
	for (TileIndexDiffC tile = bridge.start;; tile = AddTiles(tile, step)) {
		tiles.push_back(tile);
		if (SameTile(tile, bridge.end)) break;
	}
	return tiles;
}

/** G5: bridge heads, approaches, span lengths and overlaps. */
std::vector<RuleViolation> CheckBridges(const JunctionCheckContext &ctx)
{
	std::vector<RuleViolation> out;
	const JunctionLayout &layout = ctx.layout;
	std::vector<std::vector<TileIndexDiffC>> tile_sets;

	for (const JunctionBridge &bridge : layout.bridges) {
		const bool same_row = bridge.start.y == bridge.end.y;
		const bool same_column = bridge.start.x == bridge.end.x;
		const bool aligned = same_row != same_column; // Equal in both is the same tile.
		if (SameTile(bridge.start, bridge.end)) AddViolation(out, JunctionRule::Bridges, bridge.start, "bridge heads must be distinct");
		if (!same_row && !same_column) AddViolation(out, JunctionRule::Bridges, bridge.start, "bridge heads must be on one row or column");
		tile_sets.push_back(aligned ? BridgeTiles(bridge) : std::vector<TileIndexDiffC>{bridge.start, bridge.end});

		for (TileIndexDiffC head : {bridge.start, bridge.end}) {
			if (TracksAt(layout, head).Any()) AddViolation(out, JunctionRule::Bridges, head, "track on head");
			if (std::ranges::any_of(layout.signals, [&](const JunctionSignal &s) { return SameTile(s.offset, head); })) {
				AddViolation(out, JunctionRule::Bridges, head, "signal on head");
			}
		}
		if (!aligned) continue;

		const TileIndexDiffC step = {static_cast<int16_t>(SignOf(bridge.end.x - bridge.start.x)), static_cast<int16_t>(SignOf(bridge.end.y - bridge.start.y))};
		const Track axis = same_row ? Track::X : Track::Y;
		const TileIndexDiffC before = {static_cast<int16_t>(bridge.start.x - step.x), static_cast<int16_t>(bridge.start.y - step.y)};
		const TileIndexDiffC after = AddTiles(bridge.end, step);
		if (!TracksAt(layout, before).Test(axis)) AddViolation(out, JunctionRule::Bridges, bridge.start, "no approach track outside the head");
		if (!TracksAt(layout, after).Test(axis)) AddViolation(out, JunctionRule::Bridges, bridge.end, "no approach track outside the head");

		const uint distance = std::abs(bridge.end.x - bridge.start.x) + std::abs(bridge.end.y - bridge.start.y);
		if (!ctx.spec.bridge_span_ok(distance - 1)) AddViolation(out, JunctionRule::Bridges, bridge.start, fmt::format("bridge span of {} tiles not accepted", distance - 1));
	}

	for (size_t i = 0; i < tile_sets.size(); i++) {
		for (size_t j = i + 1; j < tile_sets.size(); j++) {
			for (TileIndexDiffC tile : tile_sets[i]) {
				if (std::ranges::none_of(tile_sets[j], [&](TileIndexDiffC other) { return SameTile(tile, other); })) continue;
				AddViolation(out, JunctionRule::Bridges, tile, "tile shared with another bridge");
				break;
			}
		}
	}
	return out;
}

/**
 * The curve speed limit of a train whose parts head in the given directions, as Train::GetCurveSpeedLimit.
 * Every part is 1 long and the parts are walked from the front, which is the last entry.
 * @param dirs The direction of each part in travel order.
 * @param curve_speed The rail type's curve speed advantage.
 * @return The limit; UINT16_MAX when unlimited.
 */
static int CurveSpeedLimitOfTrain(const std::vector<Direction> &dirs, uint8_t curve_speed)
{
	static constexpr int absolute_max_speed = UINT16_MAX;
	static constexpr int vehicle_length = 1;
	int max_speed = absolute_max_speed;

	int curvecount[2] = {0, 0};
	int numcurve = 0;
	int sum = 0;
	int pos = 0;
	int lastpos = -1;
	const size_t n = dirs.size();
	for (size_t i = 0; i + 1 < n; i++, pos += vehicle_length) {
		const Direction this_dir = dirs[n - 1 - i];
		const Direction next_dir = dirs[n - 2 - i];

		const DirDiff dirdiff = DirDifference(this_dir, next_dir);
		if (dirdiff == DirDiff::Same) continue;

		if (dirdiff == DirDiff::Left45) curvecount[0]++;
		if (dirdiff == DirDiff::Right45) curvecount[1]++;
		if (dirdiff == DirDiff::Left45 || dirdiff == DirDiff::Right45) {
			if (lastpos != -1) {
				numcurve++;
				sum += pos - lastpos;
				if (pos - lastpos <= vehicle_length && max_speed > 88) max_speed = 88;
			}
			lastpos = pos;
		}

		if (dirdiff == DirDiff::Left90 || dirdiff == DirDiff::Right90) max_speed = 61;
	}

	if (numcurve > 0 && max_speed > 88) {
		if (curvecount[0] == 1 && curvecount[1] == 1) {
			max_speed = absolute_max_speed;
		} else {
			sum = CeilDiv(sum, vehicle_length);
			sum /= numcurve;
			max_speed = 232 - (13 - Clamp(sum, 1, 12)) * (13 - Clamp(sum, 1, 12));
		}
	}

	if (max_speed != absolute_max_speed) max_speed += (max_speed / 2) * curve_speed;
	return max_speed;
}

/** G3: a train of 2*TL parts never has a curve speed limit below the reference speed. */
std::vector<RuleViolation> CheckCurveSpeed(const JunctionCheckContext &ctx)
{
	std::vector<RuleViolation> out;
	const size_t n = 2 * static_cast<size_t>(ctx.spec.tl);
	if (n == 0) return out;

	for (const JunctionPath &path : ctx.paths) {
		std::vector<Direction> dirs(n, TrackdirToDirection(path.pieces.front().trackdir));
		std::vector<size_t> unit_piece(n, 0);
		for (size_t i = 0; i < path.pieces.size(); i++) {
			for (uint8_t u = 0; u < path.pieces[i].units; u++) {
				dirs.push_back(TrackdirToDirection(path.pieces[i].trackdir));
				unit_piece.push_back(i);
			}
		}
		const size_t length = dirs.size() - n;
		dirs.insert(dirs.end(), n, TrackdirToDirection(path.pieces.back().trackdir));

		for (size_t start = 0; start + n <= dirs.size(); start++) {
			const std::vector<Direction> window(dirs.begin() + start, dirs.begin() + start + n);
			const int limit = CurveSpeedLimitOfTrain(window, ctx.spec.curve_speed);
			if (limit >= ctx.spec.reference_speed) continue;

			/* The unit under the front, clamped to the path: the padding stands for straight legs. */
			const size_t front = start + n - 1;
			const size_t unit = front < n ? 0 : std::min(front - n, length - 1);
			AddViolation(out, JunctionRule::CurveSpeed, path.pieces[unit_piece[n + unit]].tile, fmt::format("curve speed limit {} below {}", limit, ctx.spec.reference_speed));
			break;
		}
	}
	return out;
}

/** Whether the layout has a signal on the tile for travel along the trackdir. */
static bool HasSignalFacing(const JunctionLayout &layout, TileIndexDiffC tile, Trackdir td)
{
	return std::ranges::any_of(layout.signals, [&](const JunctionSignal &s) { return SameTile(s.offset, tile) && s.facing == td; });
}

/** Whether the layout has a signal in either direction on the track of the trackdir. */
static bool HasSignalOnTrack(const JunctionLayout &layout, TileIndexDiffC tile, Trackdir td)
{
	return std::ranges::any_of(layout.signals, [&](const JunctionSignal &s) { return SameTile(s.offset, tile) && TrackdirToTrack(s.facing) == TrackdirToTrack(td); });
}

static bool IsSignalledPiece(const JunctionLayout &layout, const JunctionPiece &piece)
{
	return !piece.on_bridge && HasSignalFacing(layout, piece.tile, piece.trackdir);
}

/** Whether a signal could be placed on the piece. */
static bool IsLegalSignalPiece(const JunctionLayout &layout, const JunctionPiece &piece)
{
	if (piece.on_bridge || FindBridgeByHead(layout, piece.tile) != nullptr) return false;
	if (TracksOverlap(TracksAt(layout, piece.tile))) return false;
	return !HasSignalOnTrack(layout, piece.tile, piece.trackdir);
}

/** The signals that are the joiner's nearest signal before an ML merge. */
static std::vector<std::pair<TileIndexDiffC, Trackdir>> JoinerSignals(const JunctionCheckContext &ctx)
{
	std::vector<std::pair<TileIndexDiffC, Trackdir>> result;
	for (size_t pi = 0; pi < ctx.paths.size(); pi++) {
		const JunctionPath &ml = ctx.paths[pi];
		const bool is_ml = (ml.from == JunctionPortRole::EIn && ml.to == JunctionPortRole::EOut) || (ml.from == JunctionPortRole::WIn && ml.to == JunctionPortRole::WOut);
		if (!is_ml) continue;
		for (const SwitchEvent &event : SwitchEventsOf(ctx.layout, ml)) {
			if (event.first) continue;
			const JunctionPiece &merge_piece = ml.pieces[event.second];
			for (size_t qi = 0; qi < ctx.paths.size(); qi++) {
				if (qi == pi) continue;
				const JunctionPath &joiner = ctx.paths[qi];
				for (const SwitchEvent &other : SwitchEventsOf(ctx.layout, joiner)) {
					if (other.first) continue;
					const JunctionPiece &joiner_piece = joiner.pieces[other.second];
					if (!SameTile(joiner_piece.tile, merge_piece.tile) || joiner_piece.trackdir == merge_piece.trackdir) continue;
					for (size_t i = other.second; i-- > 0;) {
						if (!IsSignalledPiece(ctx.layout, joiner.pieces[i])) continue;
						result.emplace_back(joiner.pieces[i].tile, joiner.pieces[i].trackdir);
						break;
					}
				}
			}
		}
	}
	return result;
}

/** G6: signal legality, facing, isolation of switches, spacing and types. */
std::vector<RuleViolation> CheckSignals(const JunctionCheckContext &ctx)
{
	std::vector<RuleViolation> out;
	const JunctionLayout &layout = ctx.layout;
	const std::map<std::pair<int, int>, TrackdirBits> used = UsedTrackdirs(ctx);
	auto used_at = [&](TileIndexDiffC tile) {
		const auto it = used.find({tile.x, tile.y});
		return it == used.end() ? TrackdirBits{} : it->second;
	};

	/* 1. Legality and 2. facing. */
	for (const JunctionSignal &signal : layout.signals) {
		const TrackBits tracks = TracksAt(layout, signal.offset);
		if (FindBridgeByHead(layout, signal.offset) != nullptr) {
			AddViolation(out, JunctionRule::Signals, signal.offset, "signal on a bridge head");
		} else if (!tracks.Test(TrackdirToTrack(signal.facing))) {
			AddViolation(out, JunctionRule::Signals, signal.offset, "signal without its track");
		} else if (TracksOverlap(tracks)) {
			AddViolation(out, JunctionRule::Signals, signal.offset, "signal on overlapping tracks");
		}

		const TrackdirBits used_here = used_at(signal.offset);
		if ((used_here & TrackToTrackdirBits(TrackdirToTrack(signal.facing))).Any() && !used_here.Test(signal.facing)) {
			AddViolation(out, JunctionRule::Signals, signal.offset, "signal faces against the traffic");
		}
	}

	/* 3. Switch isolation. */
	for (const JunctionPath &path : ctx.paths) {
		const std::vector<SwitchEvent> events = SwitchEventsOf(layout, path);
		std::set<size_t> switches;
		for (const SwitchEvent &event : events) switches.insert(event.second);

		/* The nearest piece that has or could take a signal, scanning from 'from' in 'step' direction, or nullptr. */
		auto nearest = [&](size_t from, int step) -> const JunctionPiece * {
			for (int64_t i = static_cast<int64_t>(from); i >= 0 && i < static_cast<int64_t>(path.pieces.size()); i += step) {
				const JunctionPiece &piece = path.pieces[i];
				if (piece.on_bridge || switches.contains(static_cast<size_t>(i))) return nullptr;
				if (IsSignalledPiece(layout, piece) || IsLegalSignalPiece(layout, piece)) return &piece;
			}
			return nullptr;
		};
		auto require = [&](const JunctionPiece *piece, const char *message) {
			if (piece != nullptr && !IsSignalledPiece(layout, *piece)) AddViolation(out, JunctionRule::Signals, piece->tile, message);
		};

		for (const auto &[is_split, index] : events) {
			if (index > 0) require(nearest(index - 1, -1), is_split ? "no signal before a split" : "no signal before a merge");
			if (!is_split) require(nearest(index + 1, 1), "no signal after a merge");
		}
	}

	/* 4. Spacing. */
	for (const JunctionPath &path : ctx.paths) {
		uint d = 0;
		for (const JunctionPiece &piece : path.pieces) {
			if (IsSignalledPiece(layout, piece)) {
				d = 0;
				continue;
			}
			d += piece.units;
			if (d > 2u * ctx.spec.sd && IsLegalSignalPiece(layout, piece)) {
				AddViolation(out, JunctionRule::Signals, piece.tile, "signals too far apart");
				d = 0;
			}
		}
	}

	/* 5. Types, except for the joiner's signal at an ML merge (G9 checks that one). */
	const auto joiner_signals = JoinerSignals(ctx);
	for (const JunctionSignal &signal : layout.signals) {
		if (!used_at(signal.offset).Test(signal.facing) || signal.type == ctx.spec.signal_type) continue;
		const bool joiner = std::ranges::any_of(joiner_signals, [&](const auto &js) { return SameTile(js.first, signal.offset) && js.second == signal.facing; });
		if (!joiner) AddViolation(out, JunctionRule::Signals, signal.offset, "signal type does not match the setting");
	}
	return out;
}

/** G7: the track from where a connection leaves the ML to its last signal before the merge holds a train. */
std::vector<RuleViolation> CheckWaitingRoom(const JunctionCheckContext &ctx)
{
	std::vector<RuleViolation> out;
	for (const JunctionPath &path : ctx.paths) {
		if ((path.from != JunctionPortRole::EIn && path.from != JunctionPortRole::WIn) || path.to != JunctionPortRole::SOut) continue;

		const std::vector<SwitchEvent> events = SwitchEventsOf(ctx.layout, path);
		const auto split = std::ranges::find_if(events, [](const SwitchEvent &e) { return e.first; });
		if (split == events.end()) {
			AddViolation(out, JunctionRule::WaitingRoom, PortTile(ctx, path.from), "no split on the connection");
			continue;
		}
		const TileIndexDiffC split_tile = path.pieces[split->second].tile;
		const auto merge = std::ranges::find_if(events, [&](const SwitchEvent &e) { return !e.first && e.second > split->second; });
		if (merge == events.end()) {
			AddViolation(out, JunctionRule::WaitingRoom, split_tile, "no merge after the split");
			continue;
		}
		const TileIndexDiffC merge_tile = path.pieces[merge->second].tile;

		size_t last_signal = 0;
		for (size_t i = merge->second; i-- > split->second + 1;) {
			if (!IsSignalledPiece(ctx.layout, path.pieces[i])) continue;
			last_signal = i;
			break;
		}
		if (last_signal == 0) {
			AddViolation(out, JunctionRule::WaitingRoom, merge_tile, "no signal between the split and the merge");
			continue;
		}

		uint units = 0;
		for (size_t i = split->second + 1; i <= last_signal; i++) units += path.pieces[i].units;
		if (units < 2u * ctx.spec.tl) AddViolation(out, JunctionRule::WaitingRoom, merge_tile, fmt::format("waiting room of {} units, needs {}", units, 2u * ctx.spec.tl));
	}
	return out;
}

/** Union-find root. */
static size_t FindRoot(std::vector<size_t> &parent, size_t i)
{
	while (parent[i] != i) {
		parent[i] = parent[parent[i]];
		i = parent[i];
	}
	return i;
}

/**
 * Compute the signal blocks. Tiles with a signal belong to no block; all track bits of a tile are one unit;
 * a bridge joins the tile before its near head and the tile after its far head, and its heads belong to that block.
 * @return Map from tile to block id.
 */
static std::map<std::pair<int, int>, size_t> ComputeBlocks(const JunctionLayout &layout)
{
	std::map<std::pair<int, int>, size_t> node_of;
	std::vector<TileIndexDiffC> tiles;
	for (const JunctionTrack &jt : layout.tracks) {
		if (node_of.contains({jt.offset.x, jt.offset.y})) continue;
		if (std::ranges::any_of(layout.signals, [&](const JunctionSignal &s) { return SameTile(s.offset, jt.offset); })) continue;
		node_of[{jt.offset.x, jt.offset.y}] = tiles.size();
		tiles.push_back(jt.offset);
	}
	const size_t bridge_base = tiles.size();
	std::vector<size_t> parent(tiles.size() + layout.bridges.size());
	for (size_t i = 0; i < parent.size(); i++) parent[i] = i;

	auto join = [&](size_t a, size_t b) { parent[FindRoot(parent, a)] = FindRoot(parent, b); };

	for (size_t i = 0; i < tiles.size(); i++) {
		const TrackBits tracks = TracksAt(layout, tiles[i]);
		for (DiagDirection edge = DiagDirection::Begin; edge < DiagDirection::End; edge++) {
			if (!AnyTrackTouches(tracks, edge)) continue;
			const TileIndexDiffC other = AddTiles(tiles[i], TileIndexDiffCByDiagDir(edge));
			const auto it = node_of.find({other.x, other.y});
			if (it == node_of.end()) continue;
			const DiagDirection opposite = ReverseDiagDir(edge);
			if (AnyTrackTouches(TracksAt(layout, other), opposite)) join(i, it->second);
		}
	}

	for (size_t b = 0; b < layout.bridges.size(); b++) {
		const JunctionBridge &bridge = layout.bridges[b];
		const bool aligned = (bridge.start.x == bridge.end.x) != (bridge.start.y == bridge.end.y);
		if (!aligned) continue;
		const TileIndexDiffC step = {static_cast<int16_t>(SignOf(bridge.end.x - bridge.start.x)), static_cast<int16_t>(SignOf(bridge.end.y - bridge.start.y))};
		const TileIndexDiffC before = {static_cast<int16_t>(bridge.start.x - step.x), static_cast<int16_t>(bridge.start.y - step.y)};
		const TileIndexDiffC after = AddTiles(bridge.end, step);
		for (TileIndexDiffC side : {before, after}) {
			const auto it = node_of.find({side.x, side.y});
			if (it != node_of.end()) join(bridge_base + b, it->second);
		}
	}

	std::map<std::pair<int, int>, size_t> blocks;
	for (size_t i = 0; i < tiles.size(); i++) blocks[{tiles[i].x, tiles[i].y}] = FindRoot(parent, i);
	for (size_t b = 0; b < layout.bridges.size(); b++) {
		for (TileIndexDiffC head : {layout.bridges[b].start, layout.bridges[b].end}) blocks[{head.x, head.y}] = FindRoot(parent, bridge_base + b);
	}
	return blocks;
}

/** The tiles of the switches on all paths: splits first, merges second. */
static std::pair<std::set<std::pair<int, int>>, std::set<std::pair<int, int>>> SwitchTiles(const JunctionCheckContext &ctx)
{
	std::pair<std::set<std::pair<int, int>>, std::set<std::pair<int, int>>> result;
	for (const JunctionPath &path : ctx.paths) {
		for (const auto &[is_split, index] : SwitchEventsOf(ctx.layout, path)) {
			const TileIndexDiffC tile = path.pieces[index].tile;
			(is_split ? result.first : result.second).insert({tile.x, tile.y});
		}
	}
	return result;
}

/** G8: no signal block holds both a split and a merge. */
std::vector<RuleViolation> CheckDeadlock(const JunctionCheckContext &ctx)
{
	std::vector<RuleViolation> out;
	const auto blocks = ComputeBlocks(ctx.layout);
	const auto [splits, merges] = SwitchTiles(ctx);

	std::set<size_t> split_blocks;
	for (const auto &tile : splits) {
		if (const auto it = blocks.find(tile); it != blocks.end()) split_blocks.insert(it->second);
	}
	std::set<size_t> reported;
	for (const auto &tile : merges) {
		const auto it = blocks.find(tile);
		if (it == blocks.end() || !split_blocks.contains(it->second) || !reported.insert(it->second).second) continue;
		AddViolation(out, JunctionRule::Deadlock, {static_cast<int16_t>(tile.first), static_cast<int16_t>(tile.second)}, "a signal block holds both a split and a merge");
	}
	return out;
}

/** G10: size is the bounding box and the west, east and bottom edges are free of signals, switches, bridge heads and unused track. */
std::vector<RuleViolation> CheckFootprint(const JunctionCheckContext &ctx)
{
	std::vector<RuleViolation> out;
	const JunctionLayout &layout = ctx.layout;

	int min_x = INT16_MAX;
	int min_y = INT16_MAX;
	int max_x = INT16_MIN;
	int max_y = INT16_MIN;
	auto include = [&](TileIndexDiffC tile) {
		min_x = std::min<int>(min_x, tile.x);
		min_y = std::min<int>(min_y, tile.y);
		max_x = std::max<int>(max_x, tile.x);
		max_y = std::max<int>(max_y, tile.y);
	};
	for (const JunctionTrack &jt : layout.tracks) include(jt.offset);
	for (const JunctionBridge &bridge : layout.bridges) {
		include(bridge.start);
		include(bridge.end);
	}
	if (max_x < min_x) return out;

	if (min_x != 0 || min_y != 0) {
		AddViolation(out, JunctionRule::Footprint, {static_cast<int16_t>(min_x), static_cast<int16_t>(min_y)}, "the minimum offsets must be 0");
	}
	if (layout.size.x != max_x + 1 || layout.size.y != max_y + 1) {
		AddViolation(out, JunctionRule::Footprint, {0, 0}, fmt::format("size {}x{} is not the bounding box {}x{}", layout.size.x, layout.size.y, max_x + 1, max_y + 1));
	}

	auto on_boundary = [&](TileIndexDiffC tile) { return tile.x == 0 || tile.x == layout.size.x - 1 || tile.y == layout.size.y - 1; };
	const auto used = UsedTracks(ctx);
	const auto [splits, merges] = SwitchTiles(ctx);

	for (const JunctionSignal &signal : layout.signals) {
		if (on_boundary(signal.offset)) AddViolation(out, JunctionRule::Footprint, signal.offset, "signal on the boundary");
	}
	for (const JunctionBridge &bridge : layout.bridges) {
		for (TileIndexDiffC head : {bridge.start, bridge.end}) {
			if (on_boundary(head)) AddViolation(out, JunctionRule::Footprint, head, "bridge head on the boundary");
		}
	}
	for (const JunctionTrack &jt : layout.tracks) {
		if (!on_boundary(jt.offset)) continue;
		const std::pair<int, int> key{jt.offset.x, jt.offset.y};
		if (splits.contains(key) || merges.contains(key)) AddViolation(out, JunctionRule::Footprint, jt.offset, "switch on the boundary");
		const auto it = used.find(key);
		for (Track track : jt.tracks) {
			if (it == used.end() || !it->second.Test(track)) AddViolation(out, JunctionRule::Footprint, jt.offset, "unused track on the boundary");
		}
	}
	return out;
}

std::vector<RuleViolation> CheckJunctionRules(const JunctionLayout &layout, const JunctionSpec &spec)
{
	using Role = JunctionPortRole;
	const JunctionCheckContext ctx = MakeJunctionCheckContext(layout, spec, FindJunctionPorts(layout), {
		{Role::EIn, Role::EOut}, {Role::EIn, Role::SOut}, {Role::WIn, Role::WOut}, {Role::WIn, Role::SOut}, {Role::NIn, Role::WOut}, {Role::NIn, Role::EOut},
	});

	std::vector<RuleViolation> out;
	auto add = [&](const std::vector<RuleViolation> &group) { out.insert(out.end(), group.begin(), group.end()); };
	add(CheckConnectivity(ctx));
	add(CheckGeometry(ctx));
	if (spec.curve_limit) add(CheckCurveSpeed(ctx));
	add(CheckOrder(ctx));
	add(CheckBridges(ctx));
	add(CheckSignals(ctx));
	add(CheckWaitingRoom(ctx));
	add(CheckDeadlock(ctx));
	add(CheckFootprint(ctx));
	return out;
}
