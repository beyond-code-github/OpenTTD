/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_signal_pass.cpp Junction signal and prio pass for the generator. */

#include "stdafx.h"

#include "junction_signal_pass.h"

#include "junction_rules_internal.h"
#include "direction_func.h"
#include "map_func.h"
#include "track_func.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <optional>
#include <set>
#include <utility>

#include "safeguards.h"

static bool SameTile(TileIndexDiffC a, TileIndexDiffC b)
{
	return a.x == b.x && a.y == b.y;
}

static bool IsMainline(const JunctionPath &path)
{
	return (path.from == JunctionPortRole::EIn && path.to == JunctionPortRole::EOut) || (path.from == JunctionPortRole::WIn && path.to == JunctionPortRole::WOut);
}

/** The paths in the order the pass walks them: the mainlines first, then the rest, each in context order. */
static std::vector<const JunctionPath *> PathOrder(const JunctionCheckContext &ctx)
{
	std::vector<const JunctionPath *> order;
	for (const JunctionPath &path : ctx.paths) {
		if (IsMainline(path)) order.push_back(&path);
	}
	for (const JunctionPath &path : ctx.paths) {
		if (!IsMainline(path)) order.push_back(&path);
	}
	return order;
}

/** Put a signal on the piece, unless it already has one. */
static void PlaceSignal(JunctionLayout &layout, const JunctionPiece &piece, SignalType type)
{
	if (IsSignalledPiece(layout, piece)) return;
	layout.signals.push_back({piece.tile, piece.trackdir, type, SignalVariant::Electric});
}

/** The nearest piece that has or could take a signal, scanning from 'from' in 'step' direction, or nullptr; the scan of CheckSignals. */
static const JunctionPiece *NearestSignalPiece(const JunctionLayout &layout, const JunctionPath &path, const std::set<size_t> &switches, size_t from, int step)
{
	for (int64_t i = static_cast<int64_t>(from); i >= 0 && i < static_cast<int64_t>(path.pieces.size()); i += step) {
		const JunctionPiece &piece = path.pieces[i];
		if (piece.on_bridge || switches.contains(static_cast<size_t>(i))) return nullptr;
		if (IsSignalledPiece(layout, piece) || IsLegalSignalPiece(layout, piece)) return &piece;
	}
	return nullptr;
}

/** Phase 1: the signals the checker requires, and the joiners' Entry signals. */
static void PlaceRequiredSignals(JunctionLayout &layout, const JunctionSpec &spec, const JunctionCheckContext &ctx)
{
	const auto used = UsedTracks(ctx);
	for (const JunctionPath *path : PathOrder(ctx)) {
		const std::vector<SwitchEvent> events = SwitchEventsOf(layout, used, *path);
		std::set<size_t> switches;
		for (const SwitchEvent &event : events) switches.insert(event.second);

		/* D117 a: a requirement already met by a signal in its stretch places none. */
		auto place = [&](size_t from, int step) {
			if (JunctionStretchHasSignal(layout, *path, switches, from, step)) return;
			const JunctionPiece *piece = NearestSignalPiece(layout, *path, switches, from, step);
			if (piece != nullptr) PlaceSignal(layout, *piece, spec.signal_type);
		};

		for (const auto &[is_split, index] : events) {
			if (index > 0) place(index - 1, -1);
			if (!is_split) place(index + 1, 1);
		}

		for (size_t i = 0; i < path->pieces.size(); i++) {
			if (!path->pieces[i].on_bridge || (i > 0 && path->pieces[i - 1].on_bridge)) continue;
			size_t last = i;
			while (last + 1 < path->pieces.size() && path->pieces[last + 1].on_bridge) last++;
			if (i > 0) place(i - 1, -1);
			if (last + 1 < path->pieces.size()) place(last + 1, 1);
			i = last;
		}
	}

	for (const auto &[tile, trackdir] : JoinerSignals(ctx)) {
		for (JunctionSignal &signal : layout.signals) {
			if (SameTile(signal.offset, tile) && signal.facing == trackdir) signal.type = SignalType::Entry;
		}
	}
}

/** Phase 2: the bay boundaries before every queue merge (D88), walked as CheckWaitingRoom walks them. */
static void PlaceBaySignals(JunctionLayout &layout, const JunctionSpec &spec, const JunctionCheckContext &ctx)
{
	const auto used = UsedTracks(ctx);
	const uint bay = 2u * spec.tl;
	for (const JunctionPath *path : PathOrder(ctx)) {
		if (IsMainline(*path)) continue;
		const std::vector<SwitchEvent> events = SwitchEventsOf(layout, used, *path);
		for (const SwitchEvent &event : events) {
			if (event.first) continue;
			const size_t m = event.second;

			size_t s = m;
			for (size_t i = m; i-- > 0;) {
				if (!IsSignalledPiece(layout, path->pieces[i])) continue;
				s = i;
				break;
			}
			if (s == m) continue;

			size_t u = 0;
			for (const SwitchEvent &other : events) {
				if (other.first && other.second < s) u = std::max(u, other.second);
			}
			uint length = 0;
			for (size_t i = u + 1; i <= s; i++) length += path->pieces[i].units;
			if (length < bay) continue;

			size_t prev = s;
			for (;;) {
				std::optional<size_t> cand;
				uint distance = 0;
				for (size_t i = prev; i-- > 0;) {
					distance += path->pieces[i + 1].units;
					if (distance < bay) continue;
					const JunctionPiece &piece = path->pieces[i];
					if (!IsLegalSignalPiece(layout, piece) && !IsSignalledPiece(layout, piece)) continue;
					if (piece.on_bridge || FindBridgeByHead(layout, piece.tile) != nullptr) continue;
					if (std::ranges::any_of(events, [&](const SwitchEvent &e) { return e.second == i; })) continue;
					cand = i;
					break;
				}
				if (!cand.has_value() || *cand <= u) break;
				PlaceSignal(layout, path->pieces[*cand], spec.signal_type);
				prev = *cand;
			}
		}
	}
}

/** Phase 3: fill the gaps between anchors as drag-signalling would (D97), walking upstream from each downstream anchor. Returns the (tile, trackdir) of the signals it placed. */
static std::set<std::pair<std::pair<int, int>, Trackdir>> FillSignals(JunctionLayout &layout, const JunctionSpec &spec, const JunctionCheckContext &ctx)
{
	std::set<std::pair<std::pair<int, int>, Trackdir>> placed;
	const auto place = [&](const JunctionPiece &piece) {
		if (IsSignalledPiece(layout, piece)) return;
		PlaceSignal(layout, piece, spec.signal_type);
		placed.insert({{piece.tile.x, piece.tile.y}, piece.trackdir});
	};
	const uint density = spec.sd * 192u;
	for (const JunctionPath *path : PathOrder(ctx)) {
		const std::vector<JunctionPiece> &pieces = path->pieces;
		if (pieces.empty()) continue;
		std::vector<size_t> anchors;
		for (size_t i = 0; i < pieces.size(); i++) {
			if (i == 0 || i + 1 == pieces.size() || IsSignalledPiece(layout, pieces[i])) anchors.push_back(i);
		}
		const auto legal = [&](size_t k) { return k != 0 && k + 1 != pieces.size() && IsLegalSignalPiece(layout, pieces[k]); };
		for (size_t ai = 0; ai + 1 < anchors.size(); ai++) {
			const size_t a = anchors[ai];
			size_t last = anchors[ai + 1];
			std::optional<size_t> suitable;
			/* The logical length from piece k (exclusive) up to last (inclusive). */
			const auto dist_to = [&](size_t k) {
				uint d = 0;
				for (size_t m = k + 1; m <= last; m++) d += JunctionSignalWeight(pieces[m]);
				return d;
			};
			for (size_t k = last; k-- > a + 1;) {
				if (dist_to(k) > density && suitable.has_value()) {
					/* Overshoot: use the last good spot. */
					place(pieces[*suitable]);
					last = *suitable;
					suitable.reset();
				}
				const uint dist = dist_to(k);
				if (dist == density) {
					if (legal(k)) {
						place(pieces[k]);
						last = k;
						suitable.reset();
					}
				} else if (legal(k)) {
					suitable = k;
				}
			}
			if (dist_to(a) > density && suitable.has_value()) place(pieces[*suitable]);
		}
	}
	return placed;
}

/** Whether a track touches the given tile edge. */
static bool TrackTouches(Track track, DiagDirection edge)
{
	const Trackdir td = TrackToTrackdir(track);
	return TrackdirToExitdir(td) == edge || TrackdirToExitdir(ReverseTrackdir(td)) == edge;
}

/** The track that touches both edges. */
static Track Corner(DiagDirection a, DiagDirection b)
{
	for (Track track : {Track::Upper, Track::Lower, Track::Left, Track::Right}) {
		if (TrackTouches(track, a) && TrackTouches(track, b)) return track;
	}
	NOT_REACHED();
}

static void AddTracks(JunctionLayout &layout, TileIndexDiffC tile, TrackBits tracks)
{
	for (JunctionTrack &jt : layout.tracks) {
		if (SameTile(jt.offset, tile)) {
			jt.tracks = jt.tracks | tracks;
			return;
		}
	}
	layout.tracks.push_back({tile, tracks});
}

/** The geometry of the prio of one ML merge. */
struct PrioGeometry {
	Track axis;
	DiagDirection exit_edge;
	TileIndexDiffC side_diff; ///< The step to the prio side.
	Track h1; ///< The half piece on the ML tile.
	Track h2; ///< The half piece on the row tile.
};

/** The joiner of an ML merge, found as CheckPrio finds it, with the index of its merge piece; nullptr when there is none. */
static const JunctionPath *FindJoiner(const JunctionCheckContext &ctx, const std::vector<std::vector<SwitchEvent>> &all_events, size_t pi, const SwitchEvent &event, size_t &joiner_index)
{
	const JunctionPiece &merge_piece = ctx.paths[pi].pieces[event.second];
	for (size_t qi = 0; qi < ctx.paths.size(); qi++) {
		if (qi == pi) continue;
		for (const SwitchEvent &other : all_events[qi]) {
			const JunctionPiece &piece = ctx.paths[qi].pieces[other.second];
			if (other.first || !SameTile(piece.tile, merge_piece.tile) || piece.trackdir == merge_piece.trackdir) continue;
			joiner_index = other.second;
			return &ctx.paths[qi];
		}
	}
	return nullptr;
}

/** Whether the merge has a joiner of the same movement, that is, is a lane merge. */
static bool IsLaneMerge(const JunctionCheckContext &ctx, const std::vector<std::vector<SwitchEvent>> &all_events, size_t pi, const SwitchEvent &event)
{
	size_t joiner_index = 0;
	const JunctionPath *joiner = FindJoiner(ctx, all_events, pi, event, joiner_index);
	return joiner != nullptr && joiner->from == ctx.paths[pi].from && joiner->to == ctx.paths[pi].to;
}

/** The prio geometry of a non-lane ML merge: on the side opposite the joiner's approach (D75 and D87). */
static std::optional<PrioGeometry> GeometryOfPrio(const JunctionCheckContext &ctx, const std::vector<std::vector<SwitchEvent>> &all_events, size_t pi, const SwitchEvent &event)
{
	const JunctionPath &ml = ctx.paths[pi];
	const JunctionPiece &merge_piece = ml.pieces[event.second];
	const TileIndexDiffC tile = merge_piece.tile;
	if (!IsDiagonalTrackdir(merge_piece.trackdir)) return std::nullopt;

	size_t joiner_index = 0;
	const JunctionPath *joiner = FindJoiner(ctx, all_events, pi, event, joiner_index);
	if (joiner == nullptr || (joiner->from == ml.from && joiner->to == ml.to) || joiner_index == 0) return std::nullopt;

	const Track axis = TrackdirToTrack(merge_piece.trackdir);
	const bool along_x = axis == Track::X;
	const DiagDirection exit_edge = TrackdirToExitdir(merge_piece.trackdir);
	const TileIndexDiffC from = joiner->pieces[joiner_index - 1].tile;
	const int offset = along_x ? from.y - tile.y : from.x - tile.x;
	if (offset == 0) return std::nullopt;
	std::optional<DiagDirection> side;
	for (DiagDirection edge = DiagDirection::Begin; edge < DiagDirection::End; edge++) {
		if (TrackTouches(axis, edge)) continue;
		const TileIndexDiffC diff = TileIndexDiffCByDiagDir(edge);
		if ((along_x ? diff.y : diff.x) == (offset < 0 ? 1 : -1)) side = edge;
	}
	if (!side.has_value()) return std::nullopt;
	return PrioGeometry{axis, exit_edge, TileIndexDiffCByDiagDir(*side), Corner(*side, exit_edge), Corner(ReverseDiagDir(*side), ReverseDiagDir(exit_edge))};
}

/** Phase 4: the prio for one ML merge; nothing is added when it cannot be built. */
static bool PlacePrio(JunctionLayout &layout, const JunctionCheckContext &ctx, const std::vector<std::vector<SwitchEvent>> &all_events, size_t pi, const SwitchEvent &event)
{
	const JunctionPath &ml = ctx.paths[pi];
	const size_t j = event.second;
	const JunctionPiece &merge_piece = ml.pieces[j];
	const TileIndexDiffC tile = merge_piece.tile;
	const std::optional<PrioGeometry> geometry = GeometryOfPrio(ctx, all_events, pi, event);
	if (!geometry.has_value()) return true;
	const Track axis = geometry->axis;
	const bool along_x = axis == Track::X;
	const TileIndexDiffC dir = TileIndexDiffCByDiagDir(geometry->exit_edge);
	const TileIndexDiffC side_diff = geometry->side_diff;
	const Track h1 = geometry->h1;
	const Track h2 = geometry->h2;

	/* Another lane's pass has done this merge already. Anything below that returns false abandons the prio. */
	if (TracksAt(layout, tile).Test(h1)) return true;

	const auto blocks = ComputeBlocks(layout);
	const auto merge_block = blocks.find({tile.x, tile.y});
	if (merge_block == blocks.end()) return false;

	/* The blocks the checker wants a join in: from the merge upstream along every path of the movement through the merge piece. */
	std::vector<size_t> required;
	std::set<size_t> met;
	for (const JunctionPath &lane : ctx.paths) {
		if (lane.from != ml.from || lane.to != ml.to) continue;
		const auto at = std::ranges::find_if(lane.pieces, [&](const JunctionPiece &piece) { return !piece.on_bridge && SameTile(piece.tile, tile) && piece.trackdir == merge_piece.trackdir; });
		if (at == lane.pieces.end()) continue;
		const size_t merge_at = static_cast<size_t>(at - lane.pieces.begin());
		auto require = [&](size_t i) {
			const std::optional<size_t> block = BlockOfPiece(layout, blocks, lane.pieces[i]);
			if (!block.has_value() || *block == merge_block->second || !met.insert(*block).second) return;
			required.push_back(*block);
		};
		uint covered = 0;
		size_t last_walked = merge_at;
		for (size_t i = merge_at; i-- > 0 && covered < 2u * ctx.spec.prio_tiles;) {
			covered += lane.pieces[i].units;
			last_walked = i;
			require(i);
		}
		/* D107: the segment beyond the coverage is where a held train stands; it needs its join too. */
		if (last_walked < merge_at && last_walked > 0) require(last_walked - 1);
	}

	auto has_signal = [&](TileIndexDiffC t) {
		return std::ranges::any_of(layout.signals, [&](const JunctionSignal &s) { return SameTile(s.offset, t); });
	};

	/* Join tiles: the merge tile, and the first suitable tile of p in each required block. */
	std::vector<TileIndexDiffC> joins;
	for (size_t block : required) {
		bool found = false;
		for (size_t i = j; i-- > 0 && !found;) {
			const JunctionPiece &piece = ml.pieces[i];
			if (piece.on_bridge || BlockOfPiece(layout, blocks, piece) != block) continue;
			if (has_signal(piece.tile)) continue;
			const TileIndexDiffC neighbour = {static_cast<int16_t>(piece.tile.x + side_diff.x), static_cast<int16_t>(piece.tile.y + side_diff.y)};
			const bool reserved = TracksAt(layout, piece.tile) == TrackBits{axis, h1} && TracksAt(layout, neighbour) == TrackBits{axis, h2};
			if (!reserved && (TracksAt(layout, piece.tile) != TrackBits{axis} || TracksAt(layout, neighbour).Any())) continue;
			joins.push_back(piece.tile);
			found = true;
		}
		if (!found) {
			/* A block whose ML tiles are all switches or crossings has nowhere for a join, and is skipped. */
			bool has_tile = false;
			bool joinable = false;
			for (const JunctionPiece &piece : ml.pieces) {
				if (piece.on_bridge || BlockOfPiece(layout, blocks, piece) != block) continue;
				has_tile = true;
				const TileIndexDiffC neighbour = {static_cast<int16_t>(piece.tile.x + side_diff.x), static_cast<int16_t>(piece.tile.y + side_diff.y)};
				if (TracksAt(layout, piece.tile) == TrackBits{axis} || (TracksAt(layout, piece.tile) == TrackBits{axis, h1} && TracksAt(layout, neighbour) == TrackBits{axis, h2})) joinable = true;
			}
			if (joinable || !has_tile) return false;
			continue;
		}
	}

	/* The joins in order of their distance upstream of the merge, which must all be on the merge's row. */
	std::vector<int> steps{0};
	for (const TileIndexDiffC &join : joins) {
		if ((along_x ? join.y != tile.y : join.x != tile.x)) return false;
		const int step = (tile.x - join.x) * dir.x + (tile.y - join.y) * dir.y;
		if (step <= 0) return false;
		steps.push_back(step);
	}
	std::ranges::sort(steps);

	auto row_tile = [&](int step) {
		return TileIndexDiffC{static_cast<int16_t>(tile.x - step * dir.x + side_diff.x), static_cast<int16_t>(tile.y - step * dir.y + side_diff.y)};
	};
	auto main_tile = [&](int step) {
		return TileIndexDiffC{static_cast<int16_t>(tile.x - step * dir.x), static_cast<int16_t>(tile.y - step * dir.y)};
	};

	/* The row: h2 alone beside the merge, axis plus h2 beside the other joins, the axis between. */
	std::vector<std::pair<TileIndexDiffC, TrackBits>> row;
	for (int step = 0; step <= steps.back(); step++) {
		const bool is_join = std::ranges::find(steps, step) != steps.end();
		TrackBits bits{axis};
		if (step == 0) bits = TrackBits{h2};
		else if (is_join) bits = TrackBits{axis, h2};
		const TrackBits present = TracksAt(layout, row_tile(step));
		if (present.Any() && present != bits) return false;
		row.emplace_back(row_tile(step), bits);
	}

	/* One signal beside the downstream join of each pair; adjacent joins cannot have one. */
	std::vector<TileIndexDiffC> signal_tiles;
	for (size_t k = 0; k + 1 < steps.size(); k++) {
		if (steps[k + 1] == steps[k] + 1) return false;
		signal_tiles.push_back(row_tile(steps[k] + 1));
	}

	AddTracks(layout, tile, TrackBits{h1});
	for (int step : steps) {
		if (step > 0) AddTracks(layout, main_tile(step), TrackBits{h1});
	}
	for (const auto &[row_at, bits] : row) AddTracks(layout, row_at, bits);
	const Trackdir upstream = ReverseTrackdir(merge_piece.trackdir);
	for (size_t k = 0; k < signal_tiles.size(); k++) {
		const SignalType type = k + 1 == signal_tiles.size() ? SignalType::Exit : SignalType::Combo;
		layout.signals.push_back({signal_tiles[k], upstream, type, SignalVariant::Electric});
	}
	return true;
}

/** A prio join reserved in phase 0. */
struct ReservedJoin {
	TileIndexDiffC merge; ///< The ML merge tile the join is for.
	TileIndexDiffC tile; ///< The ML tile holding h1.
	TileIndexDiffC neighbour; ///< The row tile holding the axis and h2.
	TrackBits tile_bits;
	TrackBits neighbour_bits;
};

static void RemoveTracks(JunctionLayout &layout, TileIndexDiffC tile, TrackBits tracks)
{
	for (auto it = layout.tracks.begin(); it != layout.tracks.end(); ++it) {
		if (!SameTile(it->offset, tile)) continue;
		for (Track track : tracks) it->tracks.Reset(track);
		if (it->tracks.None()) layout.tracks.erase(it);
		return;
	}
}

/**
 * Phase 0: reserve a prio join in each lane-merge block that the coverage of a later ML merge reaches.
 * The join's half piece makes its tile illegal for signals, so the signals of phase 1 keep clear of it.
 */
static std::vector<ReservedJoin> ReservePrioJoins(JunctionLayout &layout, const JunctionCheckContext &ctx)
{
	std::vector<ReservedJoin> reserved;
	const auto used = UsedTracks(ctx);
	std::vector<std::vector<SwitchEvent>> all_events;
	for (const JunctionPath &path : ctx.paths) all_events.push_back(SwitchEventsOf(layout, used, path));
	for (size_t pi = 0; pi < ctx.paths.size(); pi++) {
		if (!IsMainline(ctx.paths[pi])) continue;
		const JunctionPath &ml = ctx.paths[pi];
		for (const SwitchEvent &lane : all_events[pi]) {
			if (lane.first || !IsLaneMerge(ctx, all_events, pi, lane)) continue;
			for (const SwitchEvent &merge : all_events[pi]) {
				if (merge.first || merge.second <= lane.second || IsLaneMerge(ctx, all_events, pi, merge)) continue;
				const std::optional<PrioGeometry> geometry = GeometryOfPrio(ctx, all_events, pi, merge);
				if (!geometry.has_value()) continue;

				/* Is the lane merge inside the coverage walk back from the merge? */
				uint covered = 0;
				bool inside = false;
				for (size_t i = merge.second; i-- > 0 && covered < 2u * ctx.spec.prio_tiles;) {
					covered += ml.pieces[i].units;
					if (i == lane.second) inside = true;
				}
				if (!inside) continue;

				for (size_t i = lane.second + 1; i < merge.second; i++) {
					const JunctionPiece &piece = ml.pieces[i];
					if (piece.on_bridge) continue;
					const TileIndexDiffC neighbour = {static_cast<int16_t>(piece.tile.x + geometry->side_diff.x), static_cast<int16_t>(piece.tile.y + geometry->side_diff.y)};
					/* Another lane's path has reserved this one already. */
					if (TracksAt(layout, piece.tile) == TrackBits{geometry->axis, geometry->h1} && TracksAt(layout, neighbour) == TrackBits{geometry->axis, geometry->h2}) break;
					if (TracksAt(layout, piece.tile) != TrackBits{geometry->axis} || TracksAt(layout, neighbour).Any()) continue;
					AddTracks(layout, piece.tile, TrackBits{geometry->h1});
					AddTracks(layout, neighbour, TrackBits{geometry->axis, geometry->h2});
					reserved.push_back({ml.pieces[merge.second].tile, piece.tile, neighbour, TrackBits{geometry->h1}, TrackBits{geometry->axis, geometry->h2}});
					break;
				}
			}
		}
	}
	return reserved;
}

/**
 * Phase 3b: a piece that would form a block of its own on one lane, between two signals one piece apart inside the coverage walk
 * of a non-lane ML merge, gets a signal, so that it belongs to no block and the prio need not join it.
 */
static void PlaceLaneBlockSignals(JunctionLayout &layout, const JunctionSpec &spec, const JunctionCheckContext &ctx)
{
	const auto used = UsedTracks(ctx);
	std::vector<std::vector<SwitchEvent>> all_events;
	for (const JunctionPath &path : ctx.paths) all_events.push_back(SwitchEventsOf(layout, used, path));
	for (size_t pi = 0; pi < ctx.paths.size(); pi++) {
		if (!IsMainline(ctx.paths[pi])) continue;
		const JunctionPath &ml = ctx.paths[pi];
		for (const SwitchEvent &merge : all_events[pi]) {
			if (merge.first || IsLaneMerge(ctx, all_events, pi, merge) || !GeometryOfPrio(ctx, all_events, pi, merge).has_value()) continue;
			uint covered = 0;
			for (size_t i = merge.second; i-- > 0 && covered < 2u * ctx.spec.prio_tiles;) {
				covered += ml.pieces[i].units;
				if (i == 0 || i + 1 >= ml.pieces.size()) continue;
				const JunctionPiece &piece = ml.pieces[i];
				if (piece.on_bridge || !IsLegalSignalPiece(layout, piece)) continue;
				if (!IsSignalledPiece(layout, ml.pieces[i - 1]) || !IsSignalledPiece(layout, ml.pieces[i + 1])) continue;
				const bool on_every_lane = std::ranges::all_of(ctx.paths, [&](const JunctionPath &lane) {
					if (lane.from != ml.from || lane.to != ml.to) return true;
					return std::ranges::any_of(lane.pieces, [&](const JunctionPiece &other) { return !other.on_bridge && SameTile(other.tile, piece.tile); });
				});
				if (!on_every_lane) PlaceSignal(layout, piece, spec.signal_type);
			}
		}
	}
}

/**
 * Phase 3c (D107): inside the coverage walk of a non-lane ML merge, and one piece beyond it, two consecutive ML pieces never both carry signals
 * when one of them is a fill signal; the fill signal goes, the upstream one if both are fill. Otherwise the plain-less segment between them has no join.
 */
static void ThinPrioCoverageSignals(JunctionLayout &layout, const JunctionCheckContext &ctx, const std::set<std::pair<std::pair<int, int>, Trackdir>> &fill)
{
	const auto used = UsedTracks(ctx);
	std::vector<std::vector<SwitchEvent>> all_events;
	for (const JunctionPath &path : ctx.paths) all_events.push_back(SwitchEventsOf(layout, used, path));
	auto is_fill = [&](const JunctionPiece &piece) { return fill.contains({{piece.tile.x, piece.tile.y}, piece.trackdir}); };
	for (size_t pi = 0; pi < ctx.paths.size(); pi++) {
		if (!IsMainline(ctx.paths[pi])) continue;
		const JunctionPath &ml = ctx.paths[pi];
		for (const SwitchEvent &merge : all_events[pi]) {
			if (merge.first || IsLaneMerge(ctx, all_events, pi, merge) || !GeometryOfPrio(ctx, all_events, pi, merge).has_value()) continue;
			const JunctionPiece &merge_piece = ml.pieces[merge.second];
			for (const JunctionPath &lane : ctx.paths) {
				if (lane.from != ml.from || lane.to != ml.to) continue;
				const auto at = std::ranges::find_if(lane.pieces, [&](const JunctionPiece &piece) { return !piece.on_bridge && SameTile(piece.tile, merge_piece.tile) && piece.trackdir == merge_piece.trackdir; });
				if (at == lane.pieces.end()) continue;
				const size_t merge_at = static_cast<size_t>(at - lane.pieces.begin());
				uint covered = 0;
				size_t last = merge_at;
				for (size_t i = merge_at; i-- > 0 && covered < 2u * ctx.spec.prio_tiles;) {
					covered += lane.pieces[i].units;
					last = i;
				}
				if (last == merge_at) continue;
				const size_t lowest = last > 0 ? last - 1 : last;
				for (size_t i = merge_at - 1; i-- > lowest;) {
					const JunctionPiece &up = lane.pieces[i];
					const JunctionPiece &down = lane.pieces[i + 1];
					if (!IsSignalledPiece(layout, up) || !IsSignalledPiece(layout, down)) continue;
					const JunctionPiece *drop = nullptr;
					if (is_fill(up)) drop = &up;
					else if (is_fill(down)) drop = &down;
					if (drop == nullptr) continue;
					std::erase_if(layout.signals, [&](const JunctionSignal &signal) { return SameTile(signal.offset, drop->tile) && signal.facing == drop->trackdir; });
				}
			}
		}
	}
}

/** Phase 4: the prios, one for each ML merge that has a joiner from another movement. */
static void PlacePrios(JunctionLayout &layout, const JunctionCheckContext &ctx, const std::vector<ReservedJoin> &reserved)
{
	const auto used = UsedTracks(ctx);
	std::vector<std::vector<SwitchEvent>> all_events;
	for (const JunctionPath &path : ctx.paths) all_events.push_back(SwitchEventsOf(layout, used, path));
	for (size_t pi = 0; pi < ctx.paths.size(); pi++) {
		if (!IsMainline(ctx.paths[pi])) continue;
		for (const SwitchEvent &event : all_events[pi]) {
			if (event.first) continue;
			if (PlacePrio(layout, ctx, all_events, pi, event)) continue;
			/* Abandoned: take back what phase 0 reserved for this merge. */
			const TileIndexDiffC merge_tile = ctx.paths[pi].pieces[event.second].tile;
			for (const ReservedJoin &join : reserved) {
				if (!SameTile(join.merge, merge_tile)) continue;
				RemoveTracks(layout, join.tile, join.tile_bits);
				RemoveTracks(layout, join.neighbour, join.neighbour_bits);
			}
		}
	}
}

/** Remove the signal on a piece; returns it with its index so it can be put back. */
static std::optional<std::pair<size_t, JunctionSignal>> TakeSignal(JunctionLayout &layout, const JunctionPiece &piece)
{
	const auto it = std::ranges::find_if(layout.signals, [&](const JunctionSignal &s) { return SameTile(s.offset, piece.tile) && s.facing == piece.trackdir; });
	if (it == layout.signals.end()) return std::nullopt;
	const std::pair<size_t, JunctionSignal> taken{static_cast<size_t>(it - layout.signals.begin()), *it};
	layout.signals.erase(it);
	return taken;
}

/** Whether every violation of the layout is in 'before' (same rule, tile and message). */
static bool NoNewViolations(const JunctionLayout &layout, const JunctionSpec &spec, const std::vector<RuleViolation> &before)
{
	return std::ranges::all_of(CheckJunctionRules(layout, spec), [&](const RuleViolation &v) {
		return std::ranges::any_of(before, [&](const RuleViolation &b) { return b.rule == v.rule && SameTile(b.tile, v.tile) && b.message == v.message; });
	});
}

void RemoveRedundantJunctionSignals(JunctionLayout &layout, const JunctionSpec &spec)
{
	const std::vector<JunctionPort> ports = FindJunctionPorts(layout);
	for (bool changed = true; changed;) {
		changed = false;
		const JunctionCheckContext ctx = MakeJunctionCheckContext(layout, spec, ports, JunctionTMovements());
		const std::vector<RuleViolation> before = CheckJunctionRules(layout, spec);
		for (const JunctionPath *path : PathOrder(ctx)) {
			for (size_t i = 0; !changed && i + 1 < path->pieces.size(); i++) {
				if (!IsSignalledPiece(layout, path->pieces[i]) || !IsSignalledPiece(layout, path->pieces[i + 1])) continue;
				for (size_t k = i; k <= i + 1 && !changed; k++) {
					const auto taken = TakeSignal(layout, path->pieces[k]);
					if (!taken.has_value()) continue;
					if (NoNewViolations(layout, spec, before)) {
						changed = true;
					} else {
						layout.signals.insert(layout.signals.begin() + taken->first, taken->second);
					}
				}
			}
			if (changed) break;
		}
	}
}

void PlaceJunctionSignals(JunctionLayout &layout, const JunctionSpec &spec)
{
	const std::vector<JunctionPort> ports = FindJunctionPorts(layout);
	auto trace = [&]() { return MakeJunctionCheckContext(layout, spec, ports, JunctionTMovements()); };

	const std::vector<ReservedJoin> reserved = ReservePrioJoins(layout, trace());
	PlaceRequiredSignals(layout, spec, trace());
	PlaceBaySignals(layout, spec, trace());
	const auto fill = FillSignals(layout, spec, trace());
	ThinPrioCoverageSignals(layout, trace(), fill);
	PlaceLaneBlockSignals(layout, spec, trace());
	PlacePrios(layout, trace(), reserved);
}
