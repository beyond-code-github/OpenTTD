/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_rules.cpp Junction rule checker. */

#include "stdafx.h"

#include "junction_rules.h"
#include "junction_rules_internal.h"
#include "core/math_func.hpp"
#include "direction_func.h"
#include "map_func.h"
#include "track_func.h"

#include <algorithm>
#include <array>
#include <map>
#include <optional>
#include <set>

#include "safeguards.h"

/** The most pieces a traced path may have before it is given up on. */
static constexpr size_t MAX_PATH_PIECES = 4096;

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

TrackBits TracksAt(const JunctionLayout &layout, TileIndexDiffC tile)
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

const JunctionBridge *FindBridgeByHead(const JunctionLayout &layout, TileIndexDiffC tile)
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

/** The T role of a T topology slot: slot 0 EIn, 1 WOut, 2 NIn, 3 SOut, 4 WIn, 5 EOut. */
static JunctionPortRole TRoleOfSlot(size_t slot)
{
	static const JunctionPortRole roles[] = {JunctionPortRole::EIn, JunctionPortRole::WOut, JunctionPortRole::NIn, JunctionPortRole::SOut, JunctionPortRole::WIn, JunctionPortRole::EOut};
	return slot < lengthof(roles) ? roles[slot] : JunctionPortRole::EIn;
}

/** The group and lane of a port slot; the slots are the groups' lanes in group order (JunctionPortSlots), reversed within a group on the E and N sides. */
static JunctionPortSlot SlotOf(const JunctionTopology &topology, uint16_t slot)
{
	for (size_t g = 0; g < topology.groups.size(); g++) {
		const JunctionLaneGroup &group = topology.groups[g];
		if (slot < group.lanes) {
			const bool reversed = group.side == JunctionSide::E || group.side == JunctionSide::N;
			return {static_cast<uint8_t>(g), static_cast<uint8_t>(reversed ? group.lanes - 1 - slot : slot)};
		}
		slot -= group.lanes;
	}
	NOT_REACHED();
}

/** The name of a port slot in messages: the T's role name on the T, else side letter, lane and direction. */
static std::string PortName(const JunctionCheckContext &ctx, uint16_t slot)
{
	if (ctx.topology == &JunctionTTopology()) return RoleName(TRoleOfSlot(slot));
	const JunctionPortSlot port_slot = SlotOf(*ctx.topology, slot);
	const JunctionLaneGroup &group = ctx.topology->groups[port_slot.group];
	const char side = group.side == JunctionSide::W ? 'W' : group.side == JunctionSide::S ? 'S' : group.side == JunctionSide::E ? 'E' : 'N';
	return fmt::format("{}{} {}", side, port_slot.lane + 1, group.in ? "in" : "out");
}

/** Whether the lane group of a port slot takes trains in. */
static bool IsInSlot(const JunctionTopology &topology, uint16_t slot)
{
	return topology.groups[SlotOf(topology, slot).group].in;
}

bool IsThroughPath(const JunctionCheckContext &ctx, const JunctionPath &path)
{
	if (path.movement == UINT16_MAX) return false;
	const auto &[in, out] = ctx.topology->movements[path.movement];
	return ctx.topology->groups[in].main && ctx.topology->groups[out].main;
}

bool IsExitPath(const JunctionCheckContext &ctx, const JunctionPath &path)
{
	if (path.movement == UINT16_MAX) return false;
	const auto &[in, out] = ctx.topology->movements[path.movement];
	return ctx.topology->groups[in].main && !ctx.topology->groups[out].main;
}

/** Whether two paths run between the same two slots (the routes of one doubled lane path). */
static bool SameLanePath(const JunctionPath &a, const JunctionPath &b)
{
	return a.from_slot == b.from_slot && a.to_slot == b.to_slot;
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
 * An edge whose tile count differs from its slot count, or with a port tile not holding only the axis track, gives no ports.
 * A group on the N side gives no ports: the checker has no N edge.
 * @param layout The layout.
 * @param topology The topology whose port slots the edges are matched to.
 * @return The ports found, edge W, E, S, ascending along each edge.
 */
std::vector<JunctionPort> FindJunctionPorts(const JunctionLayout &layout, const JunctionTopology &topology)
{
	std::vector<JunctionPort> ports;
	const std::vector<JunctionPortSlot> slots = JunctionPortSlots(topology);
	const bool is_t = &topology == &JunctionTTopology();

	auto add_edge = [&](DiagDirection edge, JunctionSide side, Track axis, Trackdir in_td, Trackdir out_td) {
		std::vector<uint16_t> side_slots;
		for (size_t i = 0; i < slots.size(); i++) {
			if (topology.groups[slots[i].group].side == side) side_slots.push_back(static_cast<uint16_t>(i));
		}
		const std::map<int, TrackBits> tiles = EdgeTiles(layout, edge);
		if (tiles.size() != side_slots.size()) return;
		for (const auto &[along, tracks] : tiles) {
			if (tracks != TrackBits{axis}) return;
		}
		if (edge == DiagDirection::SW) std::ranges::reverse(side_slots);
		const bool vertical = edge == DiagDirection::SE; // Along x instead of y.
		size_t index = 0;
		for (const auto &[along, tracks] : tiles) {
			const uint16_t slot = side_slots[index++];
			const TileIndexDiffC tile = vertical
				? TileIndexDiffC{static_cast<int16_t>(along), static_cast<int16_t>(layout.size.y - 1)}
				: TileIndexDiffC{static_cast<int16_t>(edge == DiagDirection::NE ? 0 : layout.size.x - 1), static_cast<int16_t>(along)};
			const bool in = topology.groups[slots[slot].group].in;
			ports.push_back({is_t ? TRoleOfSlot(slot) : JunctionPortRole::EIn, tile, in ? in_td : out_td, slot});
		}
	};

	add_edge(DiagDirection::NE, JunctionSide::W, Track::X, Trackdir::X_SW, Trackdir::X_NE);
	add_edge(DiagDirection::SW, JunctionSide::E, Track::X, Trackdir::X_NE, Trackdir::X_SW);
	add_edge(DiagDirection::SE, JunctionSide::S, Track::Y, Trackdir::Y_NW, Trackdir::Y_SE);
	return ports;
}

std::vector<JunctionPort> FindJunctionPorts(const JunctionLayout &layout)
{
	return FindJunctionPorts(layout, JunctionTTopology());
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
 * @param from The slot of the In port the search started at.
 * @param path The pieces so far; never empty.
 */
static void TraceFrom(JunctionCheckContext &ctx, uint16_t from, std::vector<JunctionPiece> &path)
{
	const JunctionPiece cur = path.back();
	if (path.size() >= MAX_PATH_PIECES) {
		AddTraceProblem(ctx, cur.tile, "path too long");
		return;
	}

	const TileIndexDiffC next = NextTile(cur.tile, cur.trackdir);
	if (!InBox(ctx.layout, next)) {
		for (const JunctionPort &port : ctx.ports) {
			if (IsInSlot(*ctx.topology, port.slot) || !SameTile(port.tile, cur.tile) || port.trackdir != cur.trackdir) continue;
			const auto lane = std::ranges::find_if(ctx.lanes, [&](const JunctionLanePath &l) { return l.from == from && l.to == port.slot; });
			const JunctionPortRole from_role = std::ranges::find_if(ctx.ports, [&](const JunctionPort &p) { return p.slot == from; })->role;
			ctx.paths.push_back({from_role, port.role, path, from, port.slot, lane == ctx.lanes.end() ? static_cast<uint16_t>(UINT16_MAX) : lane->movement});
			return;
		}
		AddTraceProblem(ctx, cur.tile, "unexpected exit");
		return;
	}

	/* A bridge entered from outside, along its axis; the piece before the head may be a corner (a curve straight onto a ramp). */
	if (const JunctionBridge *bridge = FindBridgeByHead(ctx.layout, next); bridge != nullptr) {
		const TileIndexDiffC step = TileIndexDiffCByDiagDir(TrackdirToExitdir(cur.trackdir));
		const bool axis_x = step.x != 0;
		const bool aligned = axis_x ? bridge->start.y == bridge->end.y : bridge->start.x == bridge->end.x;
		const TileIndexDiffC along = {static_cast<int16_t>(SignOf(bridge->end.x - bridge->start.x)), static_cast<int16_t>(SignOf(bridge->end.y - bridge->start.y))};
		const bool from_start = SameTile(bridge->start, next) && step.x == along.x && step.y == along.y;
		const bool from_end = SameTile(bridge->end, next) && step.x == -along.x && step.y == -along.y;
		if (aligned && !SameTile(bridge->start, bridge->end) && (from_start || from_end)) {
			const Trackdir bridge_trackdir = DiagDirToDiagTrackdir(TrackdirToExitdir(cur.trackdir));
			if (IsOnPath(path, next, bridge_trackdir)) {
				AddTraceProblem(ctx, next, "cycle");
				return;
			}
			const TileIndexDiffC far_head = from_start ? bridge->end : bridge->start;
			const size_t old_size = path.size();
			TileIndexDiffC tile = next;
			for (;;) {
				path.push_back({tile, bridge_trackdir, 2, true});
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

/** Trace all paths from the In ports of the context's topology. */
static void TraceAllPaths(JunctionCheckContext &ctx)
{
	for (const JunctionPort &port : ctx.ports) {
		if (!IsInSlot(*ctx.topology, port.slot)) continue;
		std::vector<JunctionPiece> path{{port.tile, port.trackdir, UnitsOf(port.trackdir), false}};
		TraceFrom(ctx, port.slot, path);
	}
}

/**
 * Make a check context and trace all paths from the In ports.
 * @param layout The layout; must outlive the context.
 * @param spec The spec; must outlive the context.
 * @param topology The topology; must outlive the context.
 * @param ports The ports, with their slots.
 * @return The context with paths and trace problems filled in.
 */
JunctionCheckContext MakeJunctionTopologyCheckContext(const JunctionLayout &layout, const JunctionSpec &spec, const JunctionTopology &topology, std::vector<JunctionPort> ports)
{
	JunctionCheckContext ctx{layout, spec, &topology, ExpandJunctionLanes(topology, nullptr), std::move(ports), {}, {}, {}};
	TraceAllPaths(ctx);
	return ctx;
}

/**
 * Make a check context for the T topology and trace all paths from the In ports.
 * @param layout The layout; must outlive the context.
 * @param spec The spec; must outlive the context.
 * @param ports The ports; one without a slot gets the slot of its role.
 * @param expected The movements that must exist.
 * @return The context with paths and trace problems filled in.
 */
JunctionCheckContext MakeJunctionCheckContext(const JunctionLayout &layout, const JunctionSpec &spec, std::vector<JunctionPort> ports, std::vector<std::pair<JunctionPortRole, JunctionPortRole>> expected)
{
	const JunctionTopology &topology = JunctionTTopology();
	auto slot_of = [](JunctionPortRole role) -> uint16_t {
		for (uint16_t slot = 0; slot < 6; slot++) {
			if (TRoleOfSlot(slot) == role) return slot;
		}
		NOT_REACHED();
	};
	for (JunctionPort &port : ports) {
		if (port.slot == UINT16_MAX) port.slot = slot_of(port.role);
	}
	std::vector<JunctionLanePath> lanes;
	for (const auto &[from, to] : expected) {
		const uint16_t from_slot = slot_of(from);
		const uint16_t to_slot = slot_of(to);
		const auto movement = std::ranges::find(topology.movements, std::pair<uint8_t, uint8_t>{static_cast<uint8_t>(from_slot), static_cast<uint8_t>(to_slot)});
		lanes.push_back({from_slot, to_slot, static_cast<uint16_t>(movement == topology.movements.end() ? UINT16_MAX : movement - topology.movements.begin())});
	}
	JunctionCheckContext ctx{layout, spec, &topology, std::move(lanes), std::move(ports), std::move(expected), {}, {}};
	TraceAllPaths(ctx);
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

std::map<std::pair<int, int>, TrackBits> UsedTracks(const JunctionCheckContext &ctx)
{
	std::map<std::pair<int, int>, TrackBits> used;
	for (const auto &[tile, trackdirs] : UsedTrackdirs(ctx)) {
		for (Trackdir td : trackdirs) used[tile].Set(TrackdirToTrack(td));
	}
	return used;
}

std::vector<SwitchEvent> SwitchEventsOf(const JunctionLayout &layout, const std::map<std::pair<int, int>, TrackBits> &used, const JunctionPath &path)
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
				const auto it = used.find({cur.tile.x, cur.tile.y});
				if (it == used.end() || !it->second.Test(TrackdirToTrack(other))) continue;
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

static TileIndexDiffC PortTile(const JunctionCheckContext &ctx, uint16_t slot)
{
	for (const JunctionPort &port : ctx.ports) {
		if (port.slot == slot) return port.tile;
	}
	return {0, 0};
}

/** G1: exactly the expected movements, one path each, with no dead ends, cycles or other exits. */
std::vector<RuleViolation> CheckConnectivity(const JunctionCheckContext &ctx)
{
	std::vector<RuleViolation> out = ctx.trace_problems;

	for (const JunctionLanePath &lane_path : ctx.lanes) {
		const uint16_t from = lane_path.from;
		const uint16_t to = lane_path.to;
		const auto count = std::ranges::count_if(ctx.paths, [&](const JunctionPath &path) { return path.from_slot == from && path.to_slot == to; });
		const std::string movement = fmt::format("{} to {}", PortName(ctx, from), PortName(ctx, to));
		if (count == 0) AddViolation(out, JunctionRule::Connectivity, PortTile(ctx, from), fmt::format("missing movement {}", movement));
		if (count > 1) {
			std::vector<const JunctionPath *> lanes;
			for (const JunctionPath &path : ctx.paths) {
				if (path.from_slot == from && path.to_slot == to) lanes.push_back(&path);
			}
			auto length_of = [](const JunctionPath &path) {
				uint units = 0;
				for (const JunctionPiece &piece : path.pieces) units += piece.units;
				return units;
			};
			auto same_piece = [](const JunctionPiece &a, const JunctionPiece &b) { return SameTile(a.tile, b.tile) && a.trackdir == b.trackdir; };

			bool doubled = true;
			bool equal_length = true;
			for (size_t i = 0; i < lanes.size() && doubled; i++) {
				for (size_t j = i + 1; j < lanes.size() && doubled; j++) {
					const std::vector<JunctionPiece> &a = lanes[i]->pieces;
					const std::vector<JunctionPiece> &b = lanes[j]->pieces;
					size_t prefix = 0;
					while (prefix < a.size() && prefix < b.size() && same_piece(a[prefix], b[prefix])) prefix++;
					size_t suffix = 0;
					while (suffix < a.size() && suffix < b.size() && same_piece(a[a.size() - 1 - suffix], b[b.size() - 1 - suffix])) suffix++;
					if (prefix == 0 || suffix == 0 || prefix + suffix > a.size() || prefix + suffix > b.size()) {
						doubled = false;
						break;
					}
					for (size_t x = prefix; x < a.size() - suffix && doubled; x++) {
						for (size_t y = prefix; y < b.size() - suffix; y++) {
							if (same_piece(a[x], b[y])) {
								doubled = false;
								break;
							}
						}
					}
					if (length_of(*lanes[i]) != length_of(*lanes[j])) equal_length = false;
				}
			}
			if (!doubled) {
				AddViolation(out, JunctionRule::Connectivity, PortTile(ctx, from), fmt::format("ambiguous movement {}", movement));
			} else if (!equal_length) {
				AddViolation(out, JunctionRule::Connectivity, PortTile(ctx, from), "doubled lanes differ in length");
			}
		}
	}

	for (const JunctionPath &path : ctx.paths) {
		if (path.movement != UINT16_MAX) continue;
		AddViolation(out, JunctionRule::Connectivity, path.pieces.back().tile, fmt::format("unexpected movement {} to {}", PortName(ctx, path.from_slot), PortName(ctx, path.to_slot)));
	}

	std::set<uint16_t> named;
	for (const JunctionLanePath &lane_path : ctx.lanes) {
		named.insert(lane_path.from);
		named.insert(lane_path.to);
	}
	for (uint16_t slot : named) {
		const bool present = std::ranges::any_of(ctx.ports, [&](const JunctionPort &port) { return port.slot == slot; });
		if (!present) AddViolation(out, JunctionRule::Connectivity, {0, 0}, fmt::format("missing port {}", PortName(ctx, slot)));
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

/** G4: along a path, a merge must not be followed by a split. D110: two splits on adjacent tiles of a mainline path are wrong. */
std::vector<RuleViolation> CheckOrder(const JunctionCheckContext &ctx)
{
	const auto used = UsedTracks(ctx);
	std::vector<RuleViolation> out;
	for (const JunctionPath &path : ctx.paths) {
		const std::vector<SwitchEvent> events = SwitchEventsOf(ctx.layout, used, path);
		const bool is_ml = IsThroughPath(ctx, path);
		if (is_ml) {
			const SwitchEvent *previous = nullptr;
			for (const SwitchEvent &event : events) {
				if (!event.first) continue;
				if (previous != nullptr && event.second == previous->second + 1) {
					AddViolation(out, JunctionRule::Order, path.pieces[event.second].tile, "adjacent splits: make one three-way split or leave one signal tile");
				}
				previous = &event;
			}
		}
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
		const TileIndexDiffC before = {static_cast<int16_t>(bridge.start.x - step.x), static_cast<int16_t>(bridge.start.y - step.y)};
		const TileIndexDiffC after = AddTiles(bridge.end, step);
		/* D110: the approach tile only needs track reaching the edge it shares with the head; a curve may join a head directly. */
		DiagDirection step_dir = DiagDirection::NE;
		for (DiagDirection dir : {DiagDirection::NE, DiagDirection::SE, DiagDirection::SW, DiagDirection::NW}) {
			if (TileIndexDiffCByDiagDir(dir).x == step.x && TileIndexDiffCByDiagDir(dir).y == step.y) step_dir = dir;
		}
		if (!AnyTrackTouches(TracksAt(layout, before), step_dir)) AddViolation(out, JunctionRule::Bridges, bridge.start, "no approach track outside the head");
		if (!AnyTrackTouches(TracksAt(layout, after), ReverseDiagDir(step_dir))) AddViolation(out, JunctionRule::Bridges, bridge.end, "no approach track outside the head");

		const uint distance = std::abs(bridge.end.x - bridge.start.x) + std::abs(bridge.end.y - bridge.start.y);
		if (!ctx.spec.bridge_span_ok(distance - 1)) AddViolation(out, JunctionRule::Bridges, bridge.start, fmt::format("bridge span of {} tiles not accepted", distance - 1));
	}

	/* Every bridge on a mainline path needs another path of the same movement that avoids it. */
	for (size_t b = 0; b < layout.bridges.size(); b++) {
		const JunctionBridge &bridge = layout.bridges[b];
		auto on_bridge = [&](const JunctionPiece &piece) {
			return piece.on_bridge && std::ranges::any_of(tile_sets[b], [&](TileIndexDiffC tile) { return SameTile(tile, piece.tile); });
		};
		bool doubled = true;
		for (const JunctionPath &ml : ctx.paths) {
			const bool is_ml = IsThroughPath(ctx, ml);
			if (!is_ml || std::ranges::none_of(ml.pieces, on_bridge)) continue;
			const bool has_other = std::ranges::any_of(ctx.paths, [&](const JunctionPath &other) {
				return &other != &ml && SameLanePath(other, ml) && std::ranges::none_of(other.pieces, on_bridge);
			});
			if (!has_other) doubled = false;
		}
		if (!doubled) AddViolation(out, JunctionRule::Bridges, bridge.start, "ML bridge is not doubled");
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

/**
 * The directions of a path's units with n straight units of its first heading before and n of its last heading after, as a train of n parts enters and leaves the path.
 * @param pieces The path's pieces, not empty.
 * @param n The train's part count.
 * @param[out] unit_piece For each entry of the result, the index of the piece it belongs to (the padding counts as the first or last piece's).
 * @param[out] length The number of units of the pieces.
 * @return The padded directions.
 */
static std::vector<Direction> PaddedPathDirections(const std::vector<JunctionPiece> &pieces, size_t n, std::vector<size_t> &unit_piece, size_t &length)
{
	std::vector<Direction> dirs(n, TrackdirToDirection(pieces.front().trackdir));
	unit_piece.assign(n, 0);
	for (size_t i = 0; i < pieces.size(); i++) {
		for (uint8_t u = 0; u < pieces[i].units; u++) {
			dirs.push_back(TrackdirToDirection(pieces[i].trackdir));
			unit_piece.push_back(i);
		}
	}
	length = dirs.size() - n;
	dirs.insert(dirs.end(), n, TrackdirToDirection(pieces.back().trackdir));
	unit_piece.insert(unit_piece.end(), n, pieces.size() - 1);
	return dirs;
}

/** G3: a train of 2*TL parts never has a curve speed limit below the reference speed. */
std::vector<RuleViolation> CheckCurveSpeed(const JunctionCheckContext &ctx)
{
	std::vector<RuleViolation> out;
	const size_t n = 2 * static_cast<size_t>(ctx.spec.tl);
	if (n == 0) return out;

	for (const JunctionPath &path : ctx.paths) {
		std::vector<size_t> unit_piece;
		size_t length;
		const std::vector<Direction> dirs = PaddedPathDirections(path.pieces, n, unit_piece, length);

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

int JunctionMinCurveSpeedOfDirections(const std::vector<Direction> &dirs, uint8_t tl, uint8_t curve_speed)
{
	int least = UINT16_MAX;
	const size_t n = 2 * static_cast<size_t>(tl);
	if (n == 0) return least;

	for (size_t start = 0; start + n <= dirs.size(); start++) {
		const std::vector<Direction> window(dirs.begin() + start, dirs.begin() + start + n);
		least = std::min(least, CurveSpeedLimitOfTrain(window, curve_speed));
	}
	return least;
}

int JunctionMinCurveSpeed(const std::vector<JunctionPiece> &pieces, uint8_t tl, uint8_t curve_speed)
{
	const size_t n = 2 * static_cast<size_t>(tl);
	if (n == 0 || pieces.empty()) return UINT16_MAX;

	std::vector<size_t> unit_piece;
	size_t length;
	return JunctionMinCurveSpeedOfDirections(PaddedPathDirections(pieces, n, unit_piece, length), tl, curve_speed);
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

bool IsSignalledPiece(const JunctionLayout &layout, const JunctionPiece &piece)
{
	return !piece.on_bridge && HasSignalFacing(layout, piece.tile, piece.trackdir);
}

bool IsLegalSignalPiece(const JunctionLayout &layout, const JunctionPiece &piece)
{
	if (piece.on_bridge || FindBridgeByHead(layout, piece.tile) != nullptr) return false;
	if (TracksOverlap(TracksAt(layout, piece.tile))) return false;
	return !HasSignalOnTrack(layout, piece.tile, piece.trackdir);
}

std::vector<std::pair<TileIndexDiffC, Trackdir>> JoinerSignals(const JunctionCheckContext &ctx)
{
	const auto used = UsedTracks(ctx);
	std::vector<std::pair<TileIndexDiffC, Trackdir>> result;
	for (size_t pi = 0; pi < ctx.paths.size(); pi++) {
		const JunctionPath &ml = ctx.paths[pi];
		const bool is_ml = IsThroughPath(ctx, ml);
		if (!is_ml) continue;
		for (const SwitchEvent &event : SwitchEventsOf(ctx.layout, used, ml)) {
			if (event.first) continue;
			const JunctionPiece &merge_piece = ml.pieces[event.second];
			for (size_t qi = 0; qi < ctx.paths.size(); qi++) {
				if (qi == pi) continue;
				const JunctionPath &joiner = ctx.paths[qi];
				for (const SwitchEvent &other : SwitchEventsOf(ctx.layout, used, joiner)) {
					if (other.first) continue;
					const JunctionPiece &joiner_piece = joiner.pieces[other.second];
					if (!SameTile(joiner_piece.tile, merge_piece.tile) || joiner_piece.trackdir == merge_piece.trackdir) continue;
					if (SameLanePath(joiner, ml)) continue; // A lane merge has no prio.
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
bool JunctionStretchHasSignal(const JunctionLayout &layout, const JunctionPath &path, const std::set<size_t> &switches, size_t from, int step)
{
	for (int64_t i = static_cast<int64_t>(from); i >= 0 && i < static_cast<int64_t>(path.pieces.size()); i += step) {
		const JunctionPiece &piece = path.pieces[i];
		if (piece.on_bridge || switches.contains(static_cast<size_t>(i))) return false;
		if (IsSignalledPiece(layout, piece)) return true;
	}
	return false;
}

std::vector<RuleViolation> CheckSignals(const JunctionCheckContext &ctx)
{
	std::vector<RuleViolation> out;
	const JunctionLayout &layout = ctx.layout;
	const std::map<std::pair<int, int>, TrackdirBits> used = UsedTrackdirs(ctx);
	const auto used_tracks = UsedTracks(ctx);
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

	/* 3. Switch isolation (D117 a: any signal in a rule's stretch meets it). */
	for (const JunctionPath &path : ctx.paths) {
		const std::vector<SwitchEvent> events = SwitchEventsOf(layout, used_tracks, path);
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
		/* Whether the scan from 'from' in 'step' direction ends at a switch of the wanted kind before a signal-legal piece (D113). */
		auto blocked_by_switch = [&](size_t from, int step, bool want_split) {
			for (int64_t i = static_cast<int64_t>(from); i >= 0 && i < static_cast<int64_t>(path.pieces.size()); i += step) {
				const JunctionPiece &piece = path.pieces[i];
				if (piece.on_bridge) return false;
				if (switches.contains(static_cast<size_t>(i))) {
					return std::ranges::any_of(events, [&](const SwitchEvent &event) { return event.second == static_cast<size_t>(i) && event.first == want_split; });
				}
				if (IsSignalledPiece(layout, piece) || IsLegalSignalPiece(layout, piece)) return false;
			}
			return false;
		};
		/* D117 a: a rule is met by any signal in its stretch; it is violated when the stretch has a legal piece but no signal. */
		auto require = [&](size_t from, int step, const char *message) {
			const JunctionPiece *piece = nearest(from, step);
			if (piece != nullptr && !JunctionStretchHasSignal(layout, path, switches, from, step)) AddViolation(out, JunctionRule::Signals, piece->tile, message);
		};

		for (const auto &[is_split, index] : events) {
			if (index > 0) require(index - 1, -1, is_split ? "no signal before a split" : "no signal before a merge");
			if (!is_split) require(index + 1, 1, "no signal after a merge");
		}

		/* Every bridge is its own block: a signal before the near head and after the far head. */
		for (size_t i = 0; i < path.pieces.size(); i++) {
			if (!path.pieces[i].on_bridge || (i > 0 && path.pieces[i - 1].on_bridge)) continue;
			size_t last = i;
			while (last + 1 < path.pieces.size() && path.pieces[last + 1].on_bridge) last++;
			if (i > 0) {
				const JunctionPiece *before = nearest(i - 1, -1);
				if (before != nullptr && !JunctionStretchHasSignal(layout, path, switches, i - 1, -1)) AddViolation(out, JunctionRule::Signals, path.pieces[i].tile, "no signal before a bridge");
				if (before == nullptr && blocked_by_switch(i - 1, -1, true)) AddViolation(out, JunctionRule::Signals, path.pieces[i].tile, "split in a bridge's block: no signal between the split and the bridge");
			}
			if (last + 1 < path.pieces.size()) {
				const JunctionPiece *after = nearest(last + 1, 1);
				if (after != nullptr && !JunctionStretchHasSignal(layout, path, switches, last + 1, 1)) AddViolation(out, JunctionRule::Signals, path.pieces[last].tile, "no signal after a bridge");
				if (after == nullptr && blocked_by_switch(last + 1, 1, false)) AddViolation(out, JunctionRule::Signals, path.pieces[last].tile, "split in a bridge's block: no signal between the bridge and the merge");
			}
			i = last;
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
			d += JunctionSignalWeight(piece);
			if (d > ctx.spec.sd * 192u && IsLegalSignalPiece(layout, piece)) {
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
	const auto used = UsedTracks(ctx);
	std::vector<RuleViolation> out;
	for (const JunctionPath &path : ctx.paths) {
		if (!IsExitPath(ctx, path)) continue;

		const std::vector<SwitchEvent> events = SwitchEventsOf(ctx.layout, used, path);
		const auto split = std::ranges::find_if(events, [](const SwitchEvent &e) { return e.first; });
		if (split == events.end()) {
			AddViolation(out, JunctionRule::WaitingRoom, PortTile(ctx, path.from_slot), "no split on the connection");
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

	/* Whole-train bays (D83) before every queue merge. */
	const uint bay = 2u * ctx.spec.tl;
	for (const JunctionPath &path : ctx.paths) {
		const bool is_ml = IsThroughPath(ctx, path);
		if (is_ml) continue;
		const std::vector<SwitchEvent> events = SwitchEventsOf(ctx.layout, used, path);
		for (const SwitchEvent &event : events) {
			if (event.first) continue;
			const size_t m = event.second;
			const TileIndexDiffC merge_tile = path.pieces[m].tile;

			size_t s = m;
			for (size_t i = m; i-- > 0;) {
				if (!IsSignalledPiece(ctx.layout, path.pieces[i])) continue;
				s = i;
				break;
			}
			if (s == m) {
				AddViolation(out, JunctionRule::WaitingRoom, merge_tile, "no signal before the queue merge");
				continue;
			}

			size_t u = 0;
			for (const SwitchEvent &other : events) {
				if (other.first && other.second < s) u = std::max(u, other.second);
			}
			uint length = 0;
			for (size_t i = u + 1; i <= s; i++) length += path.pieces[i].units;
			if (length < bay) {
				AddViolation(out, JunctionRule::WaitingRoom, merge_tile, "less than one whole-train bay before the upstream split");
				continue;
			}

			/* Walk back from the merge signal: each boundary is the first legal piece a bay back from the previous one (D88). */
			size_t prev = s;
			for (uint k = 1;; k++) {
				std::optional<size_t> cand;
				uint distance = 0;
				for (size_t i = prev; i-- > 0;) {
					distance += path.pieces[i + 1].units;
					if (distance < bay) continue;
					const JunctionPiece &piece = path.pieces[i];
					if (!IsLegalSignalPiece(ctx.layout, piece) && !IsSignalledPiece(ctx.layout, piece)) continue;
					if (piece.on_bridge || FindBridgeByHead(ctx.layout, piece.tile) != nullptr) continue;
					if (std::ranges::any_of(events, [&](const SwitchEvent &e) { return e.second == i; })) continue;
					cand = i;
					break;
				}
				if (!cand.has_value() || *cand <= u) break;
				const JunctionPiece &piece = path.pieces[*cand];
				if (!IsSignalledPiece(ctx.layout, piece)) {
					AddViolation(out, JunctionRule::WaitingRoom, piece.tile, fmt::format("no signal at bay boundary {}", k));
				}
				prev = *cand;
			}
		}
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

std::map<std::pair<int, int>, size_t> ComputeBlocks(const JunctionLayout &layout)
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
	const auto used = UsedTracks(ctx);
	std::pair<std::set<std::pair<int, int>>, std::set<std::pair<int, int>>> result;
	for (const JunctionPath &path : ctx.paths) {
		for (const auto &[is_split, index] : SwitchEventsOf(ctx.layout, used, path)) {
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

std::optional<size_t> BlockOfPiece(const JunctionLayout &layout, const std::map<std::pair<int, int>, size_t> &blocks, const JunctionPiece &piece)
{
	if (piece.on_bridge) {
		for (const JunctionBridge &bridge : layout.bridges) {
			if (piece.tile.x < std::min(bridge.start.x, bridge.end.x) || piece.tile.x > std::max(bridge.start.x, bridge.end.x)) continue;
			if (piece.tile.y < std::min(bridge.start.y, bridge.end.y) || piece.tile.y > std::max(bridge.start.y, bridge.end.y)) continue;
			const auto it = blocks.find({bridge.start.x, bridge.start.y});
			if (it != blocks.end()) return it->second;
		}
		return std::nullopt;
	}
	const auto it = blocks.find({piece.tile.x, piece.tile.y});
	if (it == blocks.end()) return std::nullopt;
	return it->second;
}

/**
 * G9: every ML merge has exactly one prio component, shaped and signalled as the rules say.
 * A component is a set of unused track bits; all unused bits of a tile are one node, and nodes are joined
 * across a tile edge when an unused bit on each side touches it.
 */
std::vector<RuleViolation> CheckPrio(const JunctionCheckContext &ctx)
{
	using Key = std::pair<int, int>;
	std::vector<RuleViolation> out;
	const JunctionLayout &layout = ctx.layout;
	const auto used = UsedTracks(ctx);
	const auto blocks = ComputeBlocks(layout);
	auto to_tile = [](Key key) { return TileIndexDiffC{static_cast<int16_t>(key.first), static_cast<int16_t>(key.second)}; };

	std::map<Key, TrackBits> unused;
	for (const JunctionTrack &jt : layout.tracks) {
		const auto it = used.find({jt.offset.x, jt.offset.y});
		for (Track track : jt.tracks) {
			if (it != used.end() && it->second.Test(track)) continue;
			unused[{jt.offset.x, jt.offset.y}].Set(track);
		}
	}

	std::vector<Key> keys;
	std::map<Key, size_t> index_of;
	for (const auto &[key, bits] : unused) {
		index_of[key] = keys.size();
		keys.push_back(key);
	}
	std::vector<size_t> parent(keys.size());
	for (size_t i = 0; i < parent.size(); i++) parent[i] = i;
	for (size_t i = 0; i < keys.size(); i++) {
		for (DiagDirection edge = DiagDirection::Begin; edge < DiagDirection::End; edge++) {
			if (!AnyTrackTouches(unused[keys[i]], edge)) continue;
			const TileIndexDiffC other = AddTiles(to_tile(keys[i]), TileIndexDiffCByDiagDir(edge));
			const auto it = index_of.find({other.x, other.y});
			if (it == index_of.end() || !AnyTrackTouches(unused[{other.x, other.y}], ReverseDiagDir(edge))) continue;
			parent[FindRoot(parent, i)] = FindRoot(parent, it->second);
		}
	}
	auto root_of = [&](Key key) { return FindRoot(parent, index_of.at(key)); };

	std::set<size_t> matched;
	for (size_t pi = 0; pi < ctx.paths.size(); pi++) {
		const JunctionPath &ml = ctx.paths[pi];
		const bool is_ml = IsThroughPath(ctx, ml);
		if (!is_ml) continue;

		for (const SwitchEvent &event : SwitchEventsOf(layout, used, ml)) {
			if (event.first) continue;
			const size_t j = event.second;
			const JunctionPiece &merge_piece = ml.pieces[j];
			const TileIndexDiffC tile = merge_piece.tile;
			const Key tile_key{tile.x, tile.y};

			/* The joiner: the other path entering the switch tile. */
			const JunctionPath *joiner = nullptr;
			size_t joiner_index = 0;
			for (size_t qi = 0; qi < ctx.paths.size() && joiner == nullptr; qi++) {
				if (qi == pi) continue;
				for (const SwitchEvent &other : SwitchEventsOf(layout, used, ctx.paths[qi])) {
					const JunctionPiece &piece = ctx.paths[qi].pieces[other.second];
					if (other.first || !SameTile(piece.tile, tile) || piece.trackdir == merge_piece.trackdir) continue;
					joiner = &ctx.paths[qi];
					joiner_index = other.second;
					break;
				}
			}
			if (joiner != nullptr && SameLanePath(*joiner, ml)) continue; // A lane merge has no prio.
			if (joiner == nullptr) {
				AddViolation(out, JunctionRule::Prio, tile, "ML merge without joiner");
			} else {
				/* 6. The joiner's nearest signal before the merge is Entry. */
				const JunctionPiece *signalled = nullptr;
				for (size_t i = joiner_index; i-- > 0;) {
					if (!IsSignalledPiece(layout, joiner->pieces[i])) continue;
					signalled = &joiner->pieces[i];
					break;
				}
				if (signalled == nullptr) {
					AddViolation(out, JunctionRule::Prio, tile, "no joiner signal before the ML merge");
				} else {
					for (const JunctionSignal &signal : layout.signals) {
						if (SameTile(signal.offset, signalled->tile) && signal.facing == signalled->trackdir && signal.type != SignalType::Entry) {
							AddViolation(out, JunctionRule::Prio, signal.offset, "the joiner's signal before an ML merge must be Entry");
						}
					}
				}
			}

			/* The component sharing a block with the merge's switch tile. */
			const auto merge_block = blocks.find(tile_key);
			std::set<size_t> components;
			if (merge_block != blocks.end()) {
				for (const Key &key : keys) {
					const auto b = blocks.find(key);
					if (b != blocks.end() && b->second == merge_block->second) components.insert(root_of(key));
				}
			}
			matched.insert(components.begin(), components.end());
			if (components.empty()) {
				AddViolation(out, JunctionRule::Prio, tile, "missing prio");
				continue;
			}
			if (components.size() > 1) {
				AddViolation(out, JunctionRule::Prio, tile, "more than one prio component at the ML merge");
				continue;
			}
			if (!IsDiagonalTrackdir(merge_piece.trackdir)) {
				AddViolation(out, JunctionRule::Prio, tile, "ML merge is not on an axis piece");
				continue;
			}

			const size_t component = *components.begin();
			std::set<Key> c_tiles;
			for (const Key &key : keys) {
				if (root_of(key) == component) c_tiles.insert(key);
			}
			std::set<Key> p_tiles;
			for (const JunctionPiece &piece : ml.pieces) {
				if (!piece.on_bridge) p_tiles.insert({piece.tile.x, piece.tile.y});
			}

			const Track axis = TrackdirToTrack(merge_piece.trackdir);
			const Track perp = axis == Track::X ? Track::Y : Track::X;
			const TrackBits axis_bits{axis};
			const TrackBits cross_bits{axis, perp};
			const bool along_x = axis == Track::X;
			auto along = [&](Key key) { return along_x ? key.first : key.second; };
			auto across = [&](Key key) { return along_x ? key.second : key.first; };

			auto piece_index_of = [&](Key key) {
				for (size_t i = 0; i < ml.pieces.size(); i++) {
					if (!ml.pieces[i].on_bridge && ml.pieces[i].tile.x == key.first && ml.pieces[i].tile.y == key.second) return i;
				}
				return ml.pieces.size();
			};

			/*
			 * 1. Shape: streamlined joins on p, and one row of tiles beside p.
			 * A join links the ML tile T on p to the row tile T' across a side edge: T holds the axis and a half piece h1
			 * touching that side and p's exit edge, T' holds the axis and a half piece h2 touching the ML side and p's entry edge.
			 */
			auto corner = [](DiagDirection a, DiagDirection b) {
				for (Track track : {Track::Upper, Track::Lower, Track::Left, Track::Right}) {
					if (TrackTouches(track, a) && TrackTouches(track, b)) return track;
				}
				NOT_REACHED();
			};
			std::vector<Key> join_keys;
			std::map<Key, std::pair<Track, DiagDirection>> join_info; // Per join tile: h1 and p's exit edge.
			std::map<Key, TrackBits> join_rows; // Per row tile of a join: the tracks it must hold.
			std::vector<Key> row;
			for (const Key &key : c_tiles) {
				if (!p_tiles.contains(key)) {
					row.push_back(key);
					continue;
				}
				bool is_join = false;
				const JunctionPiece &p_piece = ml.pieces[piece_index_of(key)];
				if (TrackdirToTrack(p_piece.trackdir) == axis) {
					const DiagDirection exit_edge = TrackdirToExitdir(p_piece.trackdir);
					for (DiagDirection side = DiagDirection::Begin; side < DiagDirection::End && !is_join; side++) {
						if (TrackTouches(axis, side)) continue;
						const Track h1 = corner(side, exit_edge);
						const Track h2 = corner(ReverseDiagDir(side), ReverseDiagDir(exit_edge));
						const TileIndexDiffC other = AddTiles(to_tile(key), TileIndexDiffCByDiagDir(side));
						/* On the merge tile the join also holds the joiner's half piece. */
						TrackBits join_tracks{axis, h1};
						if (key == tile_key && joiner != nullptr) join_tracks.Set(TrackdirToTrack(joiner->pieces[joiner_index].trackdir));
						if (TracksAt(layout, to_tile(key)) != join_tracks || unused[key] != TrackBits{h1}) continue;
						/* On the merge tile nothing continues downstream, so the row tile holds h2 alone; the axis would be a stub. */
						const bool merge_join = key == tile_key;
						const TrackBits row_tracks = merge_join ? TrackBits{h2} : TrackBits{axis, h2};
						const TrackBits other_tracks = TracksAt(layout, other);
						if (!c_tiles.contains({other.x, other.y}) || !(other_tracks == row_tracks || (merge_join && other_tracks == TrackBits{axis, h2}))) continue;
						if (merge_join && other_tracks != row_tracks) AddViolation(out, JunctionRule::Prio, other, "stub on the merge-tile join");
						is_join = true;
						join_keys.push_back(key);
						join_info[key] = {h1, exit_edge};
						join_rows[{other.x, other.y}] = other_tracks;
					}
				}
				if (is_join) continue;
				if (TracksAt(layout, to_tile(key)) == cross_bits && unused[key].Test(perp) && !unused[key].Test(axis)) {
					AddViolation(out, JunctionRule::Prio, to_tile(key), "prio rung must be a streamlined join");
				} else {
					AddViolation(out, JunctionRule::Prio, to_tile(key), "prio track on the ML is not a streamlined join");
				}
			}
			for (const Key &key : row) {
				const TrackBits tracks = TracksAt(layout, to_tile(key));
				if (across(key) != across(row.front()) || std::abs(across(key) - across(tile_key)) != 1) {
					AddViolation(out, JunctionRule::Prio, to_tile(key), "prio tile is not in the single row beside the ML");
				} else if (tracks != axis_bits && !(join_rows.contains(key) && tracks == join_rows[key])) {
					AddViolation(out, JunctionRule::Prio, to_tile(key), "prio tile holds tracks other than the axis and a join");
				}
			}

			/* 2. The component touches nothing used. */
			for (const Key &key : c_tiles) {
				for (Track bit : unused[key]) {
					for (DiagDirection edge = DiagDirection::Begin; edge < DiagDirection::End; edge++) {
						if (!TrackTouches(bit, edge)) continue;
						/* At a join, h1 meets p's track at the exit edge and inside the tile; that contact is the join.
						 * On the merge tile the joiner's piece is also inside the tile, and that contact is the merge. */
						if (const auto join = join_info.find(key); join != join_info.end() && join->second.first == bit && join->second.second == edge) continue;
						const TileIndexDiffC other = AddTiles(to_tile(key), TileIndexDiffCByDiagDir(edge));
						for (Track other_track : TracksAt(layout, other)) {
							if (!TrackTouches(other_track, ReverseDiagDir(edge))) continue;
							const auto it = unused.find({other.x, other.y});
							if (c_tiles.contains({other.x, other.y}) && it != unused.end() && it->second.Test(other_track)) continue;
							AddViolation(out, JunctionRule::Prio, to_tile(key), "prio track touches a used track");
						}
					}
				}
			}

			/* 3. The first (downstream) join is on the merge's switch tile. */
			if (std::ranges::find(join_keys, tile_key) == join_keys.end()) {
				AddViolation(out, JunctionRule::Prio, tile, "the first prio join must be on the merge tile");
			}

			/* 4. Coverage: one join in every block over TL, and one join per block of the row. */
			auto joins_in = [&](size_t block) {
				return std::ranges::count_if(join_keys, [&](const Key &join_key) {
					const auto b = blocks.find(join_key);
					return b != blocks.end() && b->second == block;
				});
			};
			uint covered = 0;
			std::set<size_t> met;
			for (size_t i = j; i-- > 0 && covered < 2u * ctx.spec.prio_tiles;) {
				covered += ml.pieces[i].units;
				const std::optional<size_t> block = BlockOfPiece(layout, blocks, ml.pieces[i]);
				if (!block.has_value() || *block == merge_block->second || !met.insert(*block).second) continue;
				const auto joins = joins_in(*block);
				if (joins == 0) {
					bool has_tile = false;
					bool joinable = false;
					for (const JunctionPiece &piece : ml.pieces) {
						if (piece.on_bridge || BlockOfPiece(layout, blocks, piece) != block) continue;
						has_tile = true;
						if (TracksAt(layout, piece.tile) == axis_bits) joinable = true;
					}
					if (has_tile && !joinable) continue;
				}
				if (joins != 1) AddViolation(out, JunctionRule::Prio, ml.pieces[i].tile, "ML block within the train length needs exactly one join");
			}
			std::set<size_t> row_blocks;
			for (const Key &key : row) {
				const auto b = blocks.find(key);
				if (b == blocks.end() || !row_blocks.insert(b->second).second) continue;
				if (joins_in(b->second) != 1) AddViolation(out, JunctionRule::Prio, to_tile(key), "prio block needs exactly one join");
			}

			/* 4b. G9c (D108, D116): no split of an ML lane inside the coverage itself. Anything beyond it is judged by G9b (the real-semantics prio reach). */
			for (const JunctionPath &lane : ctx.paths) {
				if (!SameLanePath(lane, ml)) continue;
				const auto at = std::ranges::find_if(lane.pieces, [&](const JunctionPiece &piece) { return !piece.on_bridge && SameTile(piece.tile, tile) && piece.trackdir == merge_piece.trackdir; });
				if (at == lane.pieces.end()) continue;
				const size_t merge_at = static_cast<size_t>(at - lane.pieces.begin());
				uint lane_units = 0;
				size_t first = merge_at;
				for (size_t i = merge_at; i-- > 0 && lane_units < 2u * ctx.spec.prio_tiles;) {
					lane_units += lane.pieces[i].units;
					first = i;
				}
				for (const SwitchEvent &other : SwitchEventsOf(layout, used, lane)) {
					if (other.second < first || other.second >= merge_at) continue;
					AddViolation(out, JunctionRule::Prio, lane.pieces[other.second].tile, "split inside prio coverage");
				}
			}

			/* 5. Prio signals face upstream, one between consecutive segments, Exit farthest, Combo elsewhere. */
			const Trackdir upstream = ReverseTrackdir(merge_piece.trackdir);
			std::vector<const JunctionSignal *> prio_signals;
			for (const JunctionSignal &signal : layout.signals) {
				if (std::ranges::find(row, Key{signal.offset.x, signal.offset.y}) != row.end()) prio_signals.push_back(&signal);
			}
			std::vector<int> join_positions;
			for (const Key &join_key : join_keys) join_positions.push_back(along(join_key));
			std::ranges::sort(join_positions);
			int farthest = 0;
			for (const JunctionSignal *signal : prio_signals) farthest = std::max(farthest, std::abs(along({signal->offset.x, signal->offset.y}) - along(tile_key)));
			for (const JunctionSignal *signal : prio_signals) {
				const int pos = along({signal->offset.x, signal->offset.y});
				if (signal->facing != upstream) AddViolation(out, JunctionRule::Prio, signal->offset, "prio signal does not face upstream");
				const SignalType wanted = std::abs(pos - along(tile_key)) == farthest ? SignalType::Exit : SignalType::Combo;
				if (signal->type != wanted) AddViolation(out, JunctionRule::Prio, signal->offset, "the farthest prio signal must be Exit and the others Combo");
				const bool between = join_positions.size() > 1 && pos > join_positions.front() && pos < join_positions.back() &&
						std::ranges::find(join_positions, pos) == join_positions.end();
				if (!between) AddViolation(out, JunctionRule::Prio, signal->offset, "prio signal is not between two segments");
			}
			for (size_t k = 0; k + 1 < join_positions.size(); k++) {
				const auto count = std::ranges::count_if(prio_signals, [&](const JunctionSignal *signal) {
					const int pos = along({signal->offset.x, signal->offset.y});
					return pos > join_positions[k] && pos < join_positions[k + 1];
				});
				if (count != 1) AddViolation(out, JunctionRule::Prio, tile, "there must be exactly one prio signal between consecutive segments");
			}
		}
	}

	for (const Key &key : keys) {
		if (!matched.contains(root_of(key))) {
			AddViolation(out, JunctionRule::Prio, to_tile(key), "stray track");
			matched.insert(root_of(key));
		}
	}
	return out;
}

/** A position a one-tile train stands on: a tile and a track on it. A bridge head is occupied through any track. */
using OccupiedPosition = std::pair<std::pair<int, int>, Track>;

/** The trackdir of the track that leaves the tile through the edge. */
static Trackdir TrackdirExiting(Track track, DiagDirection edge)
{
	const Trackdir td = TrackToTrackdir(track);
	return TrackdirToExitdir(td) == edge ? td : ReverseTrackdir(td);
}

/** The edge of the track that is not the given one. */
static DiagDirection OtherEdgeOf(Track track, DiagDirection edge)
{
	for (DiagDirection dir = DiagDirection::Begin; dir < DiagDirection::End; dir++) {
		if (dir != edge && TrackTouches(track, dir)) return dir;
	}
	NOT_REACHED();
}

/**
 * Settle the signals of the layout with trains on the occupied positions.
 * This mirrors OpenTTD's signal.cpp as docs/junction-tool/tools/sigsim.py ports it: the segment ahead of a signal is explored as
 * ExploreSegment does (whole-tile propagation, per-track detection on a pair of parallel half pieces, a signal tile belonging to
 * both segments it bounds, bridges as wormholes), and the signal is set as UpdateSignalsAroundSegment does (red for a train;
 * an Entry or Combo red when the segment has an Exit and no green Exit). The signals are swept in order until none changes.
 * @return Per signal of the layout, in order, whether it is green.
 */
static std::vector<bool> SettleSignals(const JunctionLayout &layout, const std::set<OccupiedPosition> &occupied)
{
	using Key = std::pair<int, int>;
	using Todo = std::pair<Key, int>; // Tile and the direction entered from; -1 for the wormhole of a bridge.
	constexpr int WORMHOLE = -1;
	auto dir_of = [](int dir) { return static_cast<DiagDirection>(dir); };
	auto reverse = [&](int dir) { return static_cast<int>(ReverseDiagDir(dir_of(dir))); };
	auto step = [&](Key key, int dir) {
		const TileIndexDiffC diff = TileIndexDiffCByDiagDir(dir_of(dir));
		return Key{key.first + diff.x, key.second + diff.y};
	};

	std::map<Key, TrackBits> tracks;
	for (const JunctionTrack &jt : layout.tracks) tracks[{jt.offset.x, jt.offset.y}] = tracks[{jt.offset.x, jt.offset.y}] | jt.tracks;

	/* A head of a bridge: the other head, and the direction a train travels into the bridge. */
	std::map<Key, std::pair<Key, int>> bridges;
	for (const JunctionBridge &bridge : layout.bridges) {
		const Key start{bridge.start.x, bridge.start.y};
		const Key end{bridge.end.x, bridge.end.y};
		const bool along_x = start.second == end.second;
		const DiagDirection forward = along_x ? (end.first > start.first ? DiagDirection::SW : DiagDirection::NE) : (end.second > start.second ? DiagDirection::SE : DiagDirection::NW);
		bridges[start] = {end, static_cast<int>(forward)};
		bridges[end] = {start, static_cast<int>(ReverseDiagDir(forward))};
	}

	std::map<std::pair<Key, Trackdir>, size_t> signal_at;
	std::set<std::pair<Key, Track>> signalled_tracks;
	std::set<Key> signal_tiles;
	std::vector<bool> green(layout.signals.size(), true);
	for (size_t i = 0; i < layout.signals.size(); i++) {
		const JunctionSignal &signal = layout.signals[i];
		const Key key{signal.offset.x, signal.offset.y};
		signal_at[{key, signal.facing}] = i;
		signalled_tracks.insert({key, TrackdirToTrack(signal.facing)});
		signal_tiles.insert(key);
	}

	std::set<Key> occupied_tiles;
	for (const OccupiedPosition &position : occupied) occupied_tiles.insert(position.first);

	/* ExploreSegment: returns whether the segment has a train, an Exit, and a green Exit. */
	auto explore = [&](std::vector<Todo> todo) {
		bool train = false;
		bool exit = false;
		bool exit_green = false;
		std::set<Todo> todo_set(todo.begin(), todo.end());
		std::set<Todo> done;
		/* MaybeAddToTodoSet: a pending (remove) entry is dropped instead of adding (add). */
		auto add = [&](Todo add_entry, Todo remove_entry) {
			if (todo_set.erase(remove_entry) > 0) return;
			if (done.contains(add_entry) || todo_set.contains(add_entry)) return;
			todo.push_back(add_entry);
			todo_set.insert(add_entry);
		};

		while (!todo.empty()) {
			const auto [tile, enter] = todo.back();
			todo.pop_back();
			if (todo_set.erase({tile, enter}) == 0) continue;
			done.insert({tile, enter});

			if (const auto bridge = bridges.find(tile); bridge != bridges.end()) {
				const auto [other, into] = bridge->second;
				if (enter == WORMHOLE) {
					if (occupied_tiles.contains(tile)) train = true;
					const int exit_dir = reverse(into);
					add({step(tile, exit_dir), reverse(exit_dir)}, {tile, exit_dir});
				} else {
					if (reverse(enter) != into) continue;
					if (occupied_tiles.contains(tile)) train = true;
					add({other, WORMHOLE}, {tile, WORMHOLE});
				}
				continue;
			}

			const auto found = tracks.find(tile);
			if (found == tracks.end()) continue;
			TrackBits here = found->second;
			TrackBits masked{};
			for (Track track : here) {
				if (TrackTouches(track, dir_of(enter))) masked = masked | TrackBits{track};
			}
			if (here == TrackBits{Track::Upper, Track::Lower} || here == TrackBits{Track::Left, Track::Right}) {
				here = masked;
				for (Track track : masked) {
					if (occupied.contains({tile, track})) train = true;
				}
			} else {
				if (masked.None()) continue;
				if (occupied_tiles.contains(tile)) train = true;
			}

			if (signal_tiles.contains(tile)) {
				Track track = Track::X;
				for (Track t : masked) {
					track = t;
					break;
				}
				if (signalled_tracks.contains({tile, track})) {
					const DiagDirection leave = OtherEdgeOf(track, dir_of(enter));
					const auto ahead = signal_at.find({tile, TrackdirExiting(track, leave)});
					if (ahead != signal_at.end()) {
						const SignalType type = layout.signals[ahead->second].type;
						if (type == SignalType::Exit || type == SignalType::Combo) {
							exit = true;
							if (green[ahead->second]) exit_green = true;
						}
					}
					continue;
				}
			}

			for (DiagDirection dir = DiagDirection::Begin; dir < DiagDirection::End; dir++) {
				if (static_cast<int>(dir) != enter && AnyTrackTouches(here, dir)) {
					add({step(tile, static_cast<int>(dir)), reverse(static_cast<int>(dir))}, {tile, static_cast<int>(dir)});
				}
			}
		}
		return std::array<bool, 3>{train, exit, exit_green};
	};

	for (int sweep = 0; sweep < 200; sweep++) {
		bool changed = false;
		for (size_t i = 0; i < layout.signals.size(); i++) {
			const JunctionSignal &signal = layout.signals[i];
			const Key key{signal.offset.x, signal.offset.y};
			const int leave = static_cast<int>(TrackdirToExitdir(signal.facing));
			const auto [train, exit, exit_green] = explore({{key, leave}, {step(key, leave), reverse(leave)}});
			/* UpdateSignalsAroundSegment. */
			const bool entry = signal.type == SignalType::Entry || signal.type == SignalType::Combo;
			const bool now_green = !train && !(entry && exit && !exit_green);
			if (now_green != green[i]) {
				green[i] = now_green;
				changed = true;
			}
		}
		if (!changed) break;
	}
	return green;
}

/**
 * G9b (D107): a one-tile train anywhere in the coverage of a prio, or on the next piece upstream when the coverage ends on a signal,
 * must settle the joiner's Entry signal red, under OpenTTD's real pre-signal logic.
 */
std::vector<RuleViolation> CheckPrioReach(const JunctionCheckContext &ctx)
{
	std::vector<RuleViolation> out;
	const JunctionLayout &layout = ctx.layout;
	const auto used = UsedTracks(ctx);
	const uint limit = 2u * ctx.spec.prio_tiles;

	for (size_t pi = 0; pi < ctx.paths.size(); pi++) {
		const JunctionPath &ml = ctx.paths[pi];
		const bool is_ml = IsThroughPath(ctx, ml);
		if (!is_ml) continue;

		for (const SwitchEvent &event : SwitchEventsOf(layout, used, ml)) {
			if (event.first) continue;
			const JunctionPiece &merge_piece = ml.pieces[event.second];
			if (!IsDiagonalTrackdir(merge_piece.trackdir)) continue;

			/* The joiner and its Entry signal, found as CheckPrio finds them. */
			const JunctionPath *joiner = nullptr;
			size_t joiner_index = 0;
			for (size_t qi = 0; qi < ctx.paths.size() && joiner == nullptr; qi++) {
				if (qi == pi) continue;
				for (const SwitchEvent &other : SwitchEventsOf(layout, used, ctx.paths[qi])) {
					const JunctionPiece &piece = ctx.paths[qi].pieces[other.second];
					if (other.first || !SameTile(piece.tile, merge_piece.tile) || piece.trackdir == merge_piece.trackdir) continue;
					joiner = &ctx.paths[qi];
					joiner_index = other.second;
					break;
				}
			}
			if (joiner == nullptr || SameLanePath(*joiner, ml)) continue;
			const JunctionPiece *signalled = nullptr;
			for (size_t i = joiner_index; i-- > 0;) {
				if (!IsSignalledPiece(layout, joiner->pieces[i])) continue;
				signalled = &joiner->pieces[i];
				break;
			}
			if (signalled == nullptr) continue;
			const auto entry = std::ranges::find_if(layout.signals, [&](const JunctionSignal &s) { return SameTile(s.offset, signalled->tile) && s.facing == signalled->trackdir; });
			if (entry == layout.signals.end()) continue;
			const size_t entry_index = static_cast<size_t>(entry - layout.signals.begin());

			/* Only a merge that has a prio: its tile holds an unused track (the first join). */
			const auto merge_used = used.find({merge_piece.tile.x, merge_piece.tile.y});
			bool has_prio = false;
			for (Track track : TracksAt(layout, merge_piece.tile)) {
				if (merge_used == used.end() || !merge_used->second.Test(track)) has_prio = true;
			}
			if (!has_prio) continue;

			/* The covered pieces: the coverage walk along every lane of the movement through the merge piece, and the piece beyond a signalled end. */
			std::vector<const JunctionPiece *> covered;
			for (const JunctionPath &lane : ctx.paths) {
				if (!SameLanePath(lane, ml)) continue;
				const auto at = std::ranges::find_if(lane.pieces, [&](const JunctionPiece &piece) { return !piece.on_bridge && SameTile(piece.tile, merge_piece.tile) && piece.trackdir == merge_piece.trackdir; });
				if (at == lane.pieces.end()) continue;
				const size_t merge_at = static_cast<size_t>(at - lane.pieces.begin());
				uint units = 0;
				size_t last = merge_at;
				for (size_t i = merge_at; i-- > 0 && units < limit;) {
					units += lane.pieces[i].units;
					covered.push_back(&lane.pieces[i]);
					last = i;
				}
				if (last < merge_at && last > 0 && IsSignalledPiece(layout, lane.pieces[last])) covered.push_back(&lane.pieces[last - 1]);
			}

			std::set<OccupiedPosition> tested;
			for (const JunctionPiece *piece : covered) {
				std::pair<int, int> tile{piece->tile.x, piece->tile.y};
				if (const JunctionBridge *bridge = FindBridgeByHead(layout, piece->tile); piece->on_bridge && bridge == nullptr) {
					/* A span tile: any train on the bridge is in the segment of its heads. */
					for (const JunctionBridge &b : layout.bridges) {
						if (piece->tile.x < std::min(b.start.x, b.end.x) || piece->tile.x > std::max(b.start.x, b.end.x)) continue;
						if (piece->tile.y < std::min(b.start.y, b.end.y) || piece->tile.y > std::max(b.start.y, b.end.y)) continue;
						tile = {b.start.x, b.start.y};
						break;
					}
				}
				const OccupiedPosition position{tile, TrackdirToTrack(piece->trackdir)};
				if (!tested.insert(position).second) continue;
				if (SettleSignals(layout, {position})[entry_index]) {
					AddViolation(out, JunctionRule::PrioReach, piece->tile, "ML tile in prio coverage does not hold the joiner");
				}
			}
		}
	}

	/* One violation per tile and message. */
	std::vector<RuleViolation> unique;
	for (const RuleViolation &v : out) {
		if (!std::ranges::any_of(unique, [&](const RuleViolation &u) { return SameTile(u.tile, v.tile); })) unique.push_back(v);
	}
	return unique;
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

const std::vector<std::pair<JunctionPortRole, JunctionPortRole>> &JunctionTMovements()
{
	using Role = JunctionPortRole;
	static const std::vector<std::pair<Role, Role>> movements = {
		{Role::EIn, Role::EOut}, {Role::EIn, Role::SOut}, {Role::WIn, Role::WOut}, {Role::WIn, Role::SOut}, {Role::NIn, Role::WOut}, {Role::NIn, Role::EOut},
	};
	return movements;
}

std::vector<RuleViolation> CheckJunctionRules(const JunctionLayout &layout, const JunctionSpec &spec)
{
	return CheckJunctionRules(layout, spec, JunctionTTopology());
}

std::vector<RuleViolation> CheckJunctionRules(const JunctionLayout &layout, const JunctionSpec &spec, const JunctionTopology &topology)
{
	const JunctionCheckContext ctx = MakeJunctionTopologyCheckContext(layout, spec, topology, FindJunctionPorts(layout, topology));

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
	add(CheckPrio(ctx));
	add(CheckPrioReach(ctx));
	add(CheckFootprint(ctx));
	return out;
}

/**
 * Whether CheckJunctionRules(layout, spec) is empty, checking the cheapest groups first, the prio reach simulation last,
 * and returning at the first violation.
 */
bool JunctionRulesClean(const JunctionLayout &layout, const JunctionSpec &spec)
{
	return JunctionRulesClean(layout, spec, JunctionTTopology());
}

bool JunctionRulesClean(const JunctionLayout &layout, const JunctionSpec &spec, const JunctionTopology &topology)
{
	const JunctionCheckContext ctx = MakeJunctionTopologyCheckContext(layout, spec, topology, FindJunctionPorts(layout, topology));
	return CheckConnectivity(ctx).empty() &&
			CheckGeometry(ctx).empty() &&
			CheckFootprint(ctx).empty() &&
			(!spec.curve_limit || CheckCurveSpeed(ctx).empty()) &&
			CheckOrder(ctx).empty() &&
			CheckBridges(ctx).empty() &&
			CheckSignals(ctx).empty() &&
			CheckWaitingRoom(ctx).empty() &&
			CheckDeadlock(ctx).empty() &&
			CheckPrio(ctx).empty() &&
			CheckPrioReach(ctx).empty();
}
