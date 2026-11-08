/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_station_rules.cpp Station rule checker (D106). */

#include "stdafx.h"

#include "junction_station_rules.h"
#include "junction_rules_internal.h"
#include "core/format.hpp"
#include "direction_func.h"
#include "map_func.h"
#include "track_func.h"

#include <algorithm>
#include <set>
#include <tuple>

#include "safeguards.h"

/** A (tile, trackdir) node of the walk. */
using WalkNode = std::tuple<int, int, int>;

static bool SameTile(TileIndexDiffC a, TileIndexDiffC b)
{
	return a.x == b.x && a.y == b.y;
}

static JunctionPiece MakePiece(TileIndexDiffC tile, Trackdir trackdir)
{
	return {tile, trackdir, static_cast<uint8_t>(IsDiagonalTrackdir(trackdir) ? 2 : 1), false};
}

static WalkNode NodeOf(const JunctionPiece &piece)
{
	return {piece.tile.x, piece.tile.y, static_cast<int>(piece.trackdir)};
}

/** The pieces a train on the piece can continue on, over the tracks present; it never reverses. */
static std::vector<JunctionPiece> Successors(const JunctionLayout &layout, const JunctionPiece &cur)
{
	std::vector<JunctionPiece> result;
	const TileIndexDiffC step = TileIndexDiffCByDiagDir(TrackdirToExitdir(cur.trackdir));
	const TileIndexDiffC tile = {static_cast<int16_t>(cur.tile.x + step.x), static_cast<int16_t>(cur.tile.y + step.y)};
	const TrackBits tracks = TracksAt(layout, tile);
	for (DirDiff diff : {DirDiff::Left45, DirDiff::Same, DirDiff::Right45}) {
		Trackdir next;
		if (JunctionNextTrackdir(cur.trackdir, diff, next) && tracks.Test(TrackdirToTrack(next))) result.push_back(MakePiece(tile, next));
	}
	return result;
}

/** All pieces reachable forward from the start, the start included. */
static std::set<WalkNode> Reachable(const JunctionLayout &layout, const JunctionPiece &start, bool *reaches_start_again = nullptr)
{
	std::set<WalkNode> seen;
	std::vector<JunctionPiece> todo;
	if (TracksAt(layout, start.tile).Test(TrackdirToTrack(start.trackdir))) {
		seen.insert(NodeOf(start));
		todo.push_back(start);
	}
	while (!todo.empty()) {
		const JunctionPiece cur = todo.back();
		todo.pop_back();
		for (const JunctionPiece &next : Successors(layout, cur)) {
			if (NodeOf(next) == NodeOf(start) && reaches_start_again != nullptr) *reaches_start_again = true;
			if (seen.insert(NodeOf(next)).second) todo.push_back(next);
		}
	}
	return seen;
}

/** Every walk from the start, ending at a dead end or the end of the layout, never revisiting a piece. */
static void CollectPaths(const JunctionLayout &layout, std::vector<JunctionPiece> &path, std::vector<std::vector<JunctionPiece>> &out)
{
	const std::vector<JunctionPiece> next = Successors(layout, path.back());
	bool extended = false;
	for (const JunctionPiece &piece : next) {
		if (std::ranges::any_of(path, [&](const JunctionPiece &p) { return NodeOf(p) == NodeOf(piece); })) continue;
		extended = true;
		path.push_back(piece);
		CollectPaths(layout, path, out);
		path.pop_back();
	}
	if (!extended) out.push_back(path);
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

static bool InBlock(const JunctionStationBlock &block, TileIndexDiffC tile)
{
	const int width = block.axis == Axis::X ? block.length : block.platforms;
	const int height = block.axis == Axis::X ? block.platforms : block.length;
	return tile.x >= block.origin.x && tile.x < block.origin.x + width && tile.y >= block.origin.y && tile.y < block.origin.y + height;
}

static void AddViolation(std::vector<StationRuleViolation> &out, JunctionStationRule rule, TileIndexDiffC tile, std::string message)
{
	const bool present = std::ranges::any_of(out, [&](const StationRuleViolation &v) {
		return v.rule == rule && SameTile(v.tile, tile) && v.message == message;
	});
	if (!present) out.push_back({rule, tile, std::move(message)});
}

/** S13, S14, D106: one block, TL long, at least two platforms, axis track only. */
static void CheckPlatforms(const JunctionLayout &layout, uint8_t tl, std::vector<StationRuleViolation> &out)
{
	if (layout.stations.size() != 1) {
		AddViolation(out, JunctionStationRule::Platforms, {0, 0}, fmt::format("expected exactly one station block, found {}", layout.stations.size()));
		return;
	}
	const JunctionStationBlock &block = layout.stations.front();
	if (block.length != tl) AddViolation(out, JunctionStationRule::Platforms, block.origin, fmt::format("platform length {} is not TL {}", block.length, tl));
	if (block.platforms < 2) AddViolation(out, JunctionStationRule::Platforms, block.origin, "fewer than two platforms");
	const int width = block.axis == Axis::X ? block.length : block.platforms;
	const int height = block.axis == Axis::X ? block.platforms : block.length;
	const TrackBits axis_track{AxisToTrack(block.axis)};
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			const TileIndexDiffC tile = {static_cast<int16_t>(block.origin.x + x), static_cast<int16_t>(block.origin.y + y)};
			if (TracksAt(layout, tile) != axis_track) AddViolation(out, JunctionStationRule::Platforms, tile, "station tile does not hold exactly the axis track");
		}
	}
}

/** S1: every platform is entered from the in stub and leaves to the out stub; nothing loops back to the in stub. */
static void CheckConnectivity(const JunctionLayout &layout, const JunctionStationPorts &ports, std::vector<StationRuleViolation> &out)
{
	const JunctionStationBlock &block = layout.stations.front();
	const JunctionPiece in = MakePiece(ports.in.tile, ports.in.trackdir);
	if (!TracksAt(layout, in.tile).Test(TrackdirToTrack(in.trackdir))) {
		AddViolation(out, JunctionStationRule::Connectivity, in.tile, "no track at the in port");
		return;
	}
	bool loops = false;
	const std::set<WalkNode> seen = Reachable(layout, in, &loops);
	if (loops) AddViolation(out, JunctionStationRule::Connectivity, in.tile, "a walk leads back to the in port");

	const Track axis_track = AxisToTrack(block.axis);
	const Trackdir forward = TrackToTrackdir(axis_track);
	const Trackdir backward = ReverseTrackdir(forward);
	/* Walking forward means increasing x or y, which is the SW or SE direction. */
	const int length = block.length;
	for (int p = 0; p < block.platforms; p++) {
		auto tile_at = [&](int i) {
			return block.axis == Axis::X
				? TileIndexDiffC{static_cast<int16_t>(block.origin.x + i), static_cast<int16_t>(block.origin.y + p)}
				: TileIndexDiffC{static_cast<int16_t>(block.origin.x + p), static_cast<int16_t>(block.origin.y + i)};
		};
		std::optional<JunctionPiece> far_end;
		for (int i = 0; i < length && !far_end.has_value(); i++) {
			for (Trackdir td : {forward, backward}) {
				if (!seen.contains(NodeOf(MakePiece(tile_at(i), td)))) continue;
				/* The far end is the last tile in the direction of travel. */
				const bool towards_high = TrackdirToExitdir(td) == DiagDirection::SW || TrackdirToExitdir(td) == DiagDirection::SE;
				far_end = MakePiece(tile_at(towards_high ? length - 1 : 0), td);
				break;
			}
		}
		if (!far_end.has_value()) {
			AddViolation(out, JunctionStationRule::Connectivity, tile_at(0), "platform is not reached from the in port");
			continue;
		}
		const JunctionPiece out_piece = MakePiece(ports.out.tile, ports.out.trackdir);
		if (!Reachable(layout, *far_end).contains(NodeOf(out_piece))) AddViolation(out, JunctionStationRule::Connectivity, far_end->tile, "the out port is not reachable from the platform");
	}
}

/** S26, S27, D97: entry, platform exit and out signals, none on station tiles, and the junction spacing rule. */
static void CheckSignals(const JunctionLayout &layout, const JunctionStationPorts &ports, uint8_t sd, std::vector<StationRuleViolation> &out)
{
	const JunctionStationBlock &block = layout.stations.front();
	for (const JunctionSignal &signal : layout.signals) {
		if (InBlock(block, signal.offset)) AddViolation(out, JunctionStationRule::Signals, signal.offset, "signal on a station tile");
	}

	/* A piece a signal could be placed on; station tiles are never legal. */
	auto legal = [&](const JunctionPiece &piece) { return !InBlock(block, piece.tile) && IsLegalSignalPiece(layout, piece); };

	std::vector<JunctionPiece> start = {MakePiece(ports.in.tile, ports.in.trackdir)};
	if (!TracksAt(layout, start.front().tile).Test(TrackdirToTrack(start.front().trackdir))) return;
	std::vector<std::vector<JunctionPiece>> paths;
	CollectPaths(layout, start, paths);

	for (const std::vector<JunctionPiece> &path : paths) {
		/* (a) The last legal piece before the first split. */
		for (size_t k = 0; k + 1 < path.size(); k++) {
			if (Successors(layout, path[k]).size() < 2) continue;
			for (size_t i = k + 1; i-- > 0;) {
				if (!IsSignalledPiece(layout, path[i]) && !legal(path[i])) continue;
				if (!IsSignalledPiece(layout, path[i])) AddViolation(out, JunctionStationRule::Signals, path[i].tile, "no entry signal before the first split");
				break;
			}
			break;
		}

		/* (b) The first legal piece after each platform, before any merge. */
		for (size_t i = 0; i < path.size(); i++) {
			if (!InBlock(block, path[i].tile) || (i + 1 < path.size() && InBlock(block, path[i + 1].tile))) continue;
			for (size_t j = i + 1; j < path.size(); j++) {
				if (j + 1 < path.size() && IsMergePiece(layout, path[j], path[j + 1])) break;
				if (IsSignalledPiece(layout, path[j])) break;
				if (legal(path[j])) {
					AddViolation(out, JunctionStationRule::Signals, path[j].tile, "no signal after the platform");
					break;
				}
			}
		}

		/* (c) The first legal piece after the last merge. */
		size_t last_merge = path.size();
		for (size_t i = 0; i + 1 < path.size(); i++) {
			if (IsMergePiece(layout, path[i], path[i + 1])) last_merge = i;
		}
		for (size_t j = last_merge + 1; j < path.size(); j++) {
			if (IsSignalledPiece(layout, path[j])) break;
			if (legal(path[j])) {
				AddViolation(out, JunctionStationRule::Signals, path[j].tile, "no signal after the last merge");
				break;
			}
		}

		/* (e) Spacing, as for junctions. */
		uint d = 0;
		for (const JunctionPiece &piece : path) {
			if (IsSignalledPiece(layout, piece)) {
				d = 0;
				continue;
			}
			d += JunctionSignalWeight(piece);
			if (d > sd * 192u && legal(piece)) {
				AddViolation(out, JunctionStationRule::Signals, piece.tile, "signals too far apart");
				d = 0;
			}
		}
	}
}

/** S19: at least TL tiles of track without a switch between the in port and the first split. */
static void CheckBraking(const JunctionLayout &layout, const JunctionStationPorts &ports, uint8_t tl, std::vector<StationRuleViolation> &out)
{
	JunctionPiece cur = MakePiece(ports.in.tile, ports.in.trackdir);
	if (!TracksAt(layout, cur.tile).Test(TrackdirToTrack(cur.trackdir))) return;
	std::set<WalkNode> seen = {NodeOf(cur)};
	size_t tiles = 1;
	for (;;) {
		const std::vector<JunctionPiece> next = Successors(layout, cur);
		if (next.size() > 1) {
			if (tiles < tl) AddViolation(out, JunctionStationRule::Braking, cur.tile, fmt::format("{} tiles before the first split, TL is {}", tiles, tl));
			return;
		}
		if (next.empty() || !seen.insert(NodeOf(next.front())).second) return;
		cur = next.front();
		tiles++;
	}
}

std::vector<StationRuleViolation> CheckStationRules(const JunctionLayout &layout, const JunctionStationPorts &ports, uint8_t tl, uint8_t sd)
{
	std::vector<StationRuleViolation> out;
	CheckPlatforms(layout, tl, out);
	if (layout.stations.empty()) return out;
	CheckConnectivity(layout, ports, out);
	CheckSignals(layout, ports, sd, out);
	CheckBraking(layout, ports, tl, out);
	return out;
}
