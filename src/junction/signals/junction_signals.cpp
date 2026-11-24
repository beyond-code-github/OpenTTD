/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_signals.cpp Signal tokens resolved on the rendered routes of a placement (DESIGN-G1 §12). */

#include "../../stdafx.h"

#include "junction_signals.h"

#include "junction_signal_pass.h"
#include "../junction_spec.h"
#include "../junction_track_geometry.h"
#include "../junction_rules_internal.h"
#include "../placement/junction_render.h"

#include "../../direction_func.h"
#include "../../map_func.h"
#include "../../track_func.h"

#include <algorithm>
#include <climits>
#include <functional>
#include <map>
#include <numeric>
#include <optional>
#include <set>
#include <tuple>

#include "../../safeguards.h"

/**
 * The check context of a layout and the routes drawn in it, built by hand: the pass has no topology, ports or movements, so the routes' pieces are the paths,
 * a route's group is both its slots, and a local topology flags the main groups as prio, which is all CheckPrio and CheckPrioReach read of it.
 * @param topology Receives the local topology; the context points to it, so it must outlive the context.
 */
static JunctionCheckContext MakePassCheckContext(const JunctionLayout &layout, const std::vector<JunctionTrackRoute> &routes, const JunctionSignalNetwork &network, const JunctionSpec &spec, JunctionTopology &topology)
{
	std::vector<JunctionPath> paths;
	for (size_t r = 0; r < routes.size(); r++) {
		const uint16_t slot = static_cast<uint16_t>(network.routes[r].group);
		JunctionPath path;
		path.from = JunctionPortRole::EIn;
		path.to = JunctionPortRole::EOut;
		path.pieces = routes[r].pieces;
		path.from_slot = slot;
		path.to_slot = slot;
		paths.push_back(std::move(path));
		const bool known = std::ranges::any_of(topology.lane_paths, [&](const JunctionLanePath &lane) { return lane.from == slot; });
		if (!known) topology.lane_paths.push_back({slot, slot, 0, network.routes[r].main});
	}
	return JunctionCheckContext{layout, spec, &topology, {}, {}, {}, std::move(paths), {}};
}

std::vector<JunctionTrackSignal> SignalJunctionTrackRoutes(JunctionLayout &layout, const std::vector<JunctionTrackRoute> &routes, const JunctionSignalNetwork &network, const JunctionSpec &spec, std::string *why, std::vector<std::pair<TileIndexDiffC, TileIndexDiffC>> *prio_beside, const std::vector<JunctionPrioSpan> *prio)
{
	using Reason = JunctionSignalReason;
	std::vector<JunctionTrackSignal> tokens;

	/* The pass's own state, read in place of a layout: the tracks of the routes, those drawn on top of them (the prio elements), the bridge heads and the signals placed. */
	std::map<std::pair<int, int>, TrackBits> used;
	std::set<std::pair<int, int>> heads;
	for (const JunctionTrackRoute &route : routes) {
		for (size_t i = 0; i < route.pieces.size(); i++) {
			const JunctionPiece &piece = route.pieces[i];
			if (!piece.on_bridge) {
				used[{piece.tile.x, piece.tile.y}].Set(TrackdirToTrack(piece.trackdir));
				continue;
			}
			if (i == 0 || !route.pieces[i - 1].on_bridge || i + 1 == route.pieces.size() || !route.pieces[i + 1].on_bridge) heads.insert({piece.tile.x, piece.tile.y});
		}
	}
	std::map<std::pair<int, int>, TrackBits> tracks = used;
	std::vector<std::pair<TileIndexDiffC, TrackBits>> drawn; // Written into the layout at the end.
	std::vector<JunctionSignal> signals; // Likewise.
	std::set<std::tuple<int, int, Trackdir>> signalled_at;
	const auto tracks_at = [](const std::map<std::pair<int, int>, TrackBits> &m, TileIndexDiffC tile) {
		const auto it = m.find({tile.x, tile.y});
		return it == m.end() ? TrackBits{} : it->second;
	};
	const auto draw = [&](TileIndexDiffC tile, TrackBits bits) {
		tracks[{tile.x, tile.y}] |= bits;
		drawn.emplace_back(tile, bits);
	};
	const auto add_signal = [&](TileIndexDiffC tile, Trackdir facing, SignalType type) {
		signals.push_back({tile, facing, type, SignalVariant::Electric});
		signalled_at.emplace(tile.x, tile.y, facing);
	};
	const auto is_signalled = [&](const JunctionPiece &piece) { return !piece.on_bridge && signalled_at.contains({piece.tile.x, piece.tile.y, piece.trackdir}); };
	const auto is_legal = [&](const JunctionPiece &piece) {
		if (piece.on_bridge || heads.contains({piece.tile.x, piece.tile.y})) return false;
		if (TracksOverlap(tracks_at(tracks, piece.tile))) return false;
		return !signalled_at.contains({piece.tile.x, piece.tile.y, piece.trackdir}) && !signalled_at.contains({piece.tile.x, piece.tile.y, ReverseTrackdir(piece.trackdir)});
	};

	/** One event of a route: a switch (one index) or a bridge (first to last index). */
	struct Event {
		size_t first;
		size_t last;
		bool bridge;
		bool split;
	};

	/* Phase 0: the prio elements (DESIGN-G1 §12.4). Each draws its own joins, row, row signals and the ML signals inside its coverage; the joiner's Entry follows in phase 4. */
	const int n_prio = static_cast<int>(spec.prio_tiles + (spec.prio_tiles & 1));
	std::vector<std::pair<size_t, size_t>> entries; // The joiner route and the index of its merge piece.
	std::vector<std::tuple<size_t, size_t, size_t, size_t, DiagDirection>> deferred; // Prios the pattern did not fit, searched last: the main route, its merge piece, the joiner route, its merge piece, the row side.
	for (size_t r = 0; r < routes.size(); r++) {
		if (!network.routes[r].main) continue;
		const std::vector<JunctionPiece> &P = routes[r].pieces;
		const std::vector<SwitchEvent> &switches = network.routes[r].switches;
		for (const SwitchEvent &merge : switches) {
			if (merge.first) continue;
			const size_t j = merge.second;
			const JunctionPiece &mp = P[j];
			const TileIndexDiffC M = mp.tile;

			/* 1. The joiner: the merge node n of the event (the last piece of an edge that ends at a node), and the first route of another movement whose lane path has an edge ending at n; k is that route's node piece. */
			const auto merge_node = std::ranges::find(network.routes[r].merge_nodes, j, &std::pair<size_t, uint32_t>::first);
			if (merge_node == network.routes[r].merge_nodes.end()) continue;
			const uint32_t node = merge_node->second;
			std::optional<size_t> joiner, joiner_k;
			for (size_t r2 = 0; r2 < routes.size() && !joiner.has_value(); r2++) {
				if (network.routes[r2].group == network.routes[r].group) continue;
				const auto arrival = std::ranges::find(network.routes[r2].arrivals, node, &std::pair<uint32_t, size_t>::first);
				if (arrival == network.routes[r2].arrivals.end()) continue;
				joiner = r2;
				joiner_k = arrival->second;
			}
			if (!joiner.has_value()) continue;
			if (tracks_at(tracks, M) != tracks_at(used, M)) continue;

			/* 2. The geometry, as GeometryOfPrio. */
			if (!IsDiagonalTrackdir(mp.trackdir)) continue;
			const Track axis = TrackdirToTrack(mp.trackdir);
			const bool along_x = axis == Track::X;
			const DiagDirection exit_edge = TrackdirToExitdir(mp.trackdir);
			const TileIndexDiffC from = routes[*joiner].pieces[*joiner_k - 1].tile;
			const int offset = along_x ? from.y - M.y : from.x - M.x;
			if (offset == 0) continue;
			std::optional<DiagDirection> side;
			for (DiagDirection edge = DiagDirection::Begin; edge < DiagDirection::End; edge++) {
				if (JunctionTrackTouches(axis, edge)) continue;
				const TileIndexDiffC diff = TileIndexDiffCByDiagDir(edge);
				if ((along_x ? diff.y : diff.x) == (offset < 0 ? 1 : -1)) side = edge;
			}
			if (!side.has_value()) continue;
			const TileIndexDiffC dir = TileIndexDiffCByDiagDir(exit_edge);
			const TileIndexDiffC side_diff = TileIndexDiffCByDiagDir(*side);
			const Track h1 = JunctionCornerTrack(*side, exit_edge);
			const Track h2 = JunctionCornerTrack(ReverseDiagDir(*side), ReverseDiagDir(exit_edge));

			/* The caller's spans decide which merges get a prio: a merge without one gets none. */
			const auto span = prio == nullptr ? std::vector<JunctionPrioSpan>::const_iterator{} : std::ranges::find_if(*prio, [&](const JunctionPrioSpan &candidate) { return candidate.merge.x == M.x && candidate.merge.y == M.y; });
			if (prio != nullptr && span == prio->end()) continue;

			/* A merge with a given row is placed by the search on that row, not by the straight pattern (D349, D455). */
			if (prio == nullptr && std::ranges::any_of(network.prio_rows, [&](const std::vector<JunctionPiece> &given_row) {
					return !given_row.empty() && given_row.back().tile.x == M.x + side_diff.x && given_row.back().tile.y == M.y + side_diff.y;
				})) {
				deferred.emplace_back(r, j, *joiner, *joiner_k, *side);
				continue;
			}

			/* 3. The requirements, all before anything is written. */
			const auto ml =[&](int s) { return TileIndexDiffC{static_cast<int16_t>(M.x - s * dir.x), static_cast<int16_t>(M.y - s * dir.y)}; };
			const auto row = [&](int s) { const TileIndexDiffC t = ml(s); return TileIndexDiffC{static_cast<int16_t>(t.x + side_diff.x), static_cast<int16_t>(t.y + side_diff.y)}; };
			bool fits = static_cast<int>(j) >= n_prio;
			for (int s = 1; fits && s <= n_prio; s++) {
				const JunctionPiece &piece = P[j - s];
				const TileIndexDiffC t = ml(s);
				fits = !piece.on_bridge && piece.tile.x == t.x && piece.tile.y == t.y && piece.trackdir == mp.trackdir && std::ranges::none_of(switches, [&](const SwitchEvent &sw) { return sw.second == j - s; });
			}
			for (int s = 0; fits && s <= n_prio; s++) fits = tracks_at(tracks, row(s)) == TrackBits{};
			if (fits) {
				const TrackBits beyond = tracks_at(tracks, row(n_prio + 1));
				for (Track t : beyond) {
					if (JunctionTrackTouches(t, exit_edge)) fits = false;
				}
			}
			if (!fits) {
				if (prio == nullptr) {
					deferred.emplace_back(r, j, *joiner, *joiner_k, *side);
				} else if (why != nullptr && why->empty()) {
					*why = fmt::format("the prio element at {},{} does not fit", M.x, M.y);
				}
				continue;
			}

			/* 4. Write. */
			if (prio != nullptr) {
				for (const auto &[tile, bits] : span->tiles) {
					draw(tile, bits);
					if (prio_beside != nullptr && tile.y != M.y) prio_beside->emplace_back(tile, TileIndexDiffC{tile.x, M.y});
				}
			} else {
				for (int s = 0; s <= n_prio; s++) {
					if (s % 2 == 0) draw(ml(s), TrackBits{h1});
					if (prio_beside != nullptr) prio_beside->emplace_back(row(s), ml(s));
					if (s == 0) {
						draw(row(s), TrackBits{h2});
					} else if (s % 2 == 0) {
						draw(row(s), TrackBits{axis, h2});
					} else {
						draw(row(s), TrackBits{axis});
					}
				}
			}
			for (int s = 1; s < n_prio; s += 2) {
				add_signal(ml(s), mp.trackdir, spec.signal_type);
				tokens.push_back({static_cast<uint16_t>(r), Reason::Prio, ml(s), mp.trackdir});
				const Trackdir back = ReverseTrackdir(mp.trackdir);
				add_signal(row(s), back, s == n_prio - 1 ? SignalType::Exit : SignalType::Combo);
				tokens.push_back({static_cast<uint16_t>(r), Reason::Prio, row(s), back});
			}
			entries.emplace_back(*joiner, *joiner_k);
		}
	}

	for (size_t r = 0; r < routes.size(); r++) {
		const std::vector<JunctionPiece> &P = routes[r].pieces;
		const int64_t n = static_cast<int64_t>(P.size());
		const std::vector<SwitchEvent> &switches = network.routes[r].switches;
		std::vector<Event> events;
		for (const SwitchEvent &sw : switches) events.push_back({sw.second, sw.second, false, sw.first});
		for (size_t i = 0; i < P.size(); i++) {
			if (!P[i].on_bridge) continue;
			size_t last = i;
			while (last + 1 < P.size() && P[last + 1].on_bridge) last++;
			events.push_back({i, last, true, false});
			i = last;
		}
		std::ranges::sort(events, {}, &Event::first);

		auto ok = [&](int64_t i) {
			if (i <= 0 || i >= n - 1) return false;
			const JunctionPiece &piece = P[i];
			if (piece.on_bridge || heads.contains({piece.tile.x, piece.tile.y})) return false;
			if (std::ranges::any_of(switches, [&](const SwitchEvent &sw) { return static_cast<int64_t>(sw.second) == i; })) return false;
			return is_legal(piece) || is_signalled(piece);
		};
		auto put = [&](int64_t i, Reason reason, size_t route) {
			const JunctionPiece &piece = P[i];
			if (!is_signalled(piece)) add_signal(piece.tile, piece.trackdir, spec.signal_type);
			tokens.push_back({static_cast<uint16_t>(route), reason, piece.tile, piece.trackdir});
		};
		/* The largest ok index in (lo, hi), or -1. */
		auto last_ok = [&](int64_t lo, int64_t hi) {
			for (int64_t i = hi - 1; i > lo; i--) if (ok(i)) return i;
			return int64_t{-1};
		};
		/* The smallest ok index in (lo, hi), or -1. */
		auto first_ok = [&](int64_t lo, int64_t hi) {
			for (int64_t i = lo + 1; i < hi; i++) if (ok(i)) return i;
			return int64_t{-1};
		};

		for (size_t e = 0; e < events.size(); e++) {
			const Event &ev = events[e];
			const bool has_prev = e > 0;
			const bool has_next = e + 1 < events.size();
			const int64_t prev = has_prev ? static_cast<int64_t>(events[e - 1].last) : -1;
			const int64_t next = has_next ? static_cast<int64_t>(events[e + 1].first) : n;
			const int64_t before = last_ok(prev, static_cast<int64_t>(ev.first));
			const int64_t after = first_ok(static_cast<int64_t>(ev.last), next);
			if (ev.bridge) {
				if (before >= 0) put(before, has_prev && events[e - 1].bridge ? Reason::SeriesGap : Reason::BeforeBridge, r);
				if (after >= 0) put(after, has_next && events[e + 1].bridge ? Reason::SeriesGap : Reason::AfterBridge, r);
			} else if (ev.split) {
				if (before >= 0) put(before, Reason::BeforeSplit, r);
				if (after >= 0) put(after, Reason::AfterSplit, r);
			} else {
				if (before >= 0) put(before, Reason::BeforeMerge, r);
				if (after >= 0) put(after, Reason::AfterMerge, r);
			}
		}
		if (ok(n - 2)) put(n - 2, Reason::Stub, r);
	}

	/* Phase 2: the whole-train bays before the merges of non-through paths, walked as PlaceBaySignals walks them. */
	const uint bay = 2u * spec.tl;
	for (size_t r = 0; r < routes.size(); r++) {
		if (network.routes[r].main) continue;
		const std::vector<JunctionPiece> &P = routes[r].pieces;
		const std::vector<SwitchEvent> &events = network.routes[r].switches;
		for (const SwitchEvent &event : events) {
			if (event.first) continue;
			const size_t m = event.second;

			size_t s = m;
			for (size_t i = m; i-- > 0;) {
				if (!is_signalled(P[i])) continue;
				s = i;
				break;
			}
			if (s == m) continue;

			size_t u = 0;
			for (const SwitchEvent &other : events) {
				if (other.first && other.second < s) u = std::max(u, other.second);
			}
			uint length = 0;
			for (size_t i = u + 1; i <= s; i++) length += P[i].units;
			if (length < bay) continue;

			size_t prev = s;
			for (;;) {
				std::optional<size_t> cand;
				uint distance = 0;
				for (size_t i = prev; i-- > 0;) {
					distance += P[i + 1].units;
					if (distance < bay) continue;
					const JunctionPiece &piece = P[i];
					if (!is_legal(piece) && !is_signalled(piece)) continue;
					if (piece.on_bridge || heads.contains({piece.tile.x, piece.tile.y})) continue;
					if (std::ranges::any_of(events, [&](const SwitchEvent &e) { return e.second == i; })) continue;
					cand = i;
					break;
				}
				if (!cand.has_value() || *cand <= u) break;
				if (!is_signalled(P[*cand])) add_signal(P[*cand].tile, P[*cand].trackdir, spec.signal_type);
				tokens.push_back({static_cast<uint16_t>(r), Reason::Bay, P[*cand].tile, P[*cand].trackdir});
				prev = *cand;
			}
		}
	}

	/* Phase 3: drag fill. */
	for (size_t r = 0; r < routes.size(); r++) {
		PlaceDragSignals(routes[r].pieces, spec.sd, is_signalled, is_legal, [&](const JunctionPiece &piece) {
			add_signal(piece.tile, piece.trackdir, spec.signal_type);
			tokens.push_back({static_cast<uint16_t>(r), Reason::Fill, piece.tile, piece.trackdir});
		});
	}
	/* Phase 4: the joiner's last signal before each element's merge is an Entry. */
	const auto mark_entry = [&](std::vector<JunctionSignal> &list, size_t r2, size_t k) {
		const std::vector<JunctionPiece> &P = routes[r2].pieces;
		const std::vector<SwitchEvent> &switches = network.routes[r2].switches;
		const auto signalled = [&](const JunctionPiece &piece) {
			return !piece.on_bridge && std::ranges::any_of(list, [&](const JunctionSignal &signal) { return signal.offset.x == piece.tile.x && signal.offset.y == piece.tile.y && signal.facing == piece.trackdir; });
		};
		for (size_t i = k; i-- > 0;) {
			if (std::ranges::any_of(switches, [&](const SwitchEvent &sw) { return sw.second > i && sw.second < k; })) break;
			if (!signalled(P[i])) continue;
			for (JunctionSignal &signal : list) {
				if (signal.offset.x == P[i].tile.x && signal.offset.y == P[i].tile.y && TrackdirToTrack(signal.facing) == TrackdirToTrack(P[i].trackdir)) signal.type = SignalType::Entry;
			}
			break;
		}
	};
	for (const auto &[r2, k] : entries) mark_entry(signals, r2, k);
	for (const auto &[tile, bits] : drawn) AddJunctionTracks(layout, tile, bits);
	layout.signals.insert(layout.signals.end(), signals.begin(), signals.end());

	/*
	 * Phase 5: the prios the pattern did not fit, by search over the finished tracks and signals.
	 * Elements along the main line upstream of the merge, in walk order: the merge-tile join, then pairs of a signal (ML signal and reversed row signal) and a join, ending with a join.
	 * Depth first, fewest elements first, each element on a later piece than the previous, the first candidate at each depth being the pattern's own position.
	 * A leaf is drawn into a scratch copy of the layout and judged by CheckPrio and CheckPrioReach: it is taken when no violation names a tile of this prio.
	 */
	constexpr uint MAX_PRIO_LEAVES = 20000;
	std::set<std::pair<int, int>> placed_merges; // A merge reached by several routes is placed once.
	for (const auto &[r, j, joiner, joiner_k, side] : deferred) {
		const std::vector<JunctionPiece> &P = routes[r].pieces;
		const std::vector<SwitchEvent> &switches = network.routes[r].switches;
		const JunctionPiece &mp = P[j];
		const TileIndexDiffC M = mp.tile;
		if (placed_merges.contains({M.x, M.y})) continue;
		const DiagDirection exit_edge = TrackdirToExitdir(mp.trackdir);
		const DiagDirDiff turn = ChangeDiagDir(exit_edge, DiagDirDiff::Right90) == side ? DiagDirDiff::Right90 : DiagDirDiff::Left90;

		/* The walk: step m is the piece m before the merge piece. The row tile of an axis piece is the tile on its side; a 45 degree piece between two axis pieces turning toward that side has the inner corner tile, which its two neighbours share. */
		const int last_piece = static_cast<int>(j);
		const auto at = [&](int m) -> const JunctionPiece * { return m < 0 || m > last_piece ? nullptr : &P[j - m]; };
		const auto is_axis = [&](int m) { const JunctionPiece *piece = at(m); return piece != nullptr && !piece->on_bridge && IsDiagonalTrackdir(piece->trackdir); };
		const auto side_of = [&](const JunctionPiece &piece) { return ChangeDiagDir(TrackdirToExitdir(piece.trackdir), turn); };
		/* The row the caller gave for this merge: the one whose last piece is on the merge tile's side tile (D349, D455). */
		const std::vector<JunctionPiece> *given = nullptr;
		for (const std::vector<JunctionPiece> &row : network.prio_rows) {
			const TileIndexDiffC diff = TileIndexDiffCByDiagDir(side_of(mp));
			if (!row.empty() && row.back().tile.x == M.x + diff.x && row.back().tile.y == M.y + diff.y) given = &row;
		}
		/* The index in the given row of the piece beside step m: beside an axis piece the row piece on the side tile on the same track, beside a diagonal piece the row piece on the same tile on the opposite half (D353). */
		const auto row_index = [&](int m) -> std::optional<size_t> {
			if (given == nullptr || at(m) == nullptr || at(m)->on_bridge) return std::nullopt;
			const JunctionPiece &piece = *at(m);
			if (!is_axis(m)) {
				const Track ml_half = TrackdirToTrack(piece.trackdir);
				const Track opposite = ml_half == Track::Upper ? Track::Lower : ml_half == Track::Lower ? Track::Upper : ml_half == Track::Left ? Track::Right : Track::Left;
				for (size_t i = 0; i < given->size(); i++) {
					if ((*given)[i].tile.x == piece.tile.x && (*given)[i].tile.y == piece.tile.y && TrackdirToTrack((*given)[i].trackdir) == opposite) return i;
				}
				return std::nullopt;
			}
			const TileIndexDiffC diff = TileIndexDiffCByDiagDir(side_of(piece));
			for (size_t i = 0; i < given->size(); i++) {
				const JunctionPiece &g = (*given)[i];
				if (g.tile.x == piece.tile.x + diff.x && g.tile.y == piece.tile.y + diff.y && TrackdirToTrack(g.trackdir) == TrackdirToTrack(piece.trackdir)) return i;
			}
			return std::nullopt;
		};
		const auto corner =[&](int m) -> std::optional<TileIndexDiffC> {
			const JunctionPiece *piece = at(m);
			if (m < 1 || piece == nullptr || piece->on_bridge || IsDiagonalTrackdir(piece->trackdir) || !is_axis(m - 1) || !is_axis(m + 1)) return std::nullopt;
			const DiagDirection in = TrackdirToExitdir(at(m + 1)->trackdir);
			const DiagDirection out = TrackdirToExitdir(piece->trackdir);
			if (ChangeDiagDir(in, turn) != out) return std::nullopt;
			const TileIndexDiffC to = TileIndexDiffCByDiagDir(out);
			const TileIndexDiffC from = TileIndexDiffCByDiagDir(in);
			return TileIndexDiffC{static_cast<int16_t>(piece->tile.x + to.x - from.x), static_cast<int16_t>(piece->tile.y + to.y - from.y)};
		};
		const auto row_of = [&](int m) -> std::optional<TileIndexDiffC> {
			const JunctionPiece *piece = at(m);
			if (piece == nullptr) return std::nullopt;
			if (given != nullptr) {
				const std::optional<size_t> index = row_index(m);
				if (!index.has_value()) return std::nullopt;
				return (*given)[*index].tile;
			}
			if (!is_axis(m)) return corner(m);
			const TileIndexDiffC diff = TileIndexDiffCByDiagDir(side_of(*piece));
			const TileIndexDiffC tile{static_cast<int16_t>(piece->tile.x + diff.x), static_cast<int16_t>(piece->tile.y + diff.y)};
			for (const int neighbour : {m - 1, m + 1}) {
				const std::optional<TileIndexDiffC> c = corner(neighbour);
				if (c.has_value() && (c->x != tile.x || c->y != tile.y)) return std::nullopt;
			}
			return tile;
		};
		/* An axis piece with a row tile of its own: not beside a bend. */
		const auto plain = [&](int m) { return is_axis(m) && (given != nullptr || (!corner(m - 1).has_value() && !corner(m + 1).has_value())); }; // with a given row, any axis piece has its own row tile
		const auto row_free = [&](int m) {
			if (given != nullptr && !is_axis(m)) return true; // the given row passes it
			const std::optional<TileIndexDiffC> tile = row_of(m);
			return tile.has_value() && tracks_at(tracks, *tile) == TrackBits{} && (m == 0 || std::ranges::none_of(switches, [&](const SwitchEvent &sw) { return sw.second == j - m; }));
		};
		const auto signals_on = [&](const JunctionPiece &piece) {
			std::vector<const JunctionSignal *> found;
			for (const JunctionSignal &signal : layout.signals) {
				if (signal.offset.x == piece.tile.x && signal.offset.y == piece.tile.y && TrackdirToTrack(signal.facing) == TrackdirToTrack(piece.trackdir)) found.push_back(&signal);
			}
			return found;
		};
		const auto is_fill = [&](const JunctionSignal &signal) {
			return std::ranges::any_of(tokens, [&](const JunctionTrackSignal &token) { return token.reason == Reason::Fill && token.tile.x == signal.offset.x && token.tile.y == signal.offset.y && token.facing == signal.facing; });
		};
		const auto candidate = [&](int m, bool join) {
			const JunctionPiece &piece = *at(m);
			const bool diagonal_pair = given != nullptr && !is_axis(m) && row_index(m).has_value(); // a signal pair or a touch point beside a diagonal piece
			if (!(plain(m) || diagonal_pair) || !row_free(m) || heads.contains({piece.tile.x, piece.tile.y})) return false;
			if (m > 0 && tracks_at(tracks, piece.tile) != TrackBits{TrackdirToTrack(piece.trackdir)}) return false;
			for (const JunctionSignal *signal : signals_on(piece)) {
				if (join ? !is_fill(*signal) : signal->facing != piece.trackdir) return false;
			}
			return true;
		};

		/* The ML pieces and row tiles this prio owns: the checker's coverage walk, or as far as the last element. */
		int coverage = 0;
		for (int units = 0; coverage < last_piece && units < 2 * n_prio;) units += at(++coverage)->units;

		std::vector<int> pos; // The pieces of the elements in walk order.
		uint leaves = 0;
		int pairs = 0;
		std::string first_leaf; // The first leaf's violation, for the why.
		const auto try_leaf = [&]() {
			leaves++;
			JunctionLayout scratch = layout;
			std::vector<std::pair<TileIndexDiffC, TrackBits>> drawn_here;
			std::vector<std::tuple<int, int, Trackdir>> removed;
			/* The search owns the range from the merge to the farther of the coverage and its last element: the fill signals there go. */
			for (int m = 0; m <= std::max(coverage, pos.back()); m++) {
				for (const JunctionSignal *signal : signals_on(*at(m))) {
					if (is_fill(*signal)) removed.emplace_back(signal->offset.x, signal->offset.y, signal->facing);
				}
			}
			std::erase_if(scratch.signals, [&](const JunctionSignal &signal) { return std::ranges::find(removed, std::tuple{static_cast<int>(signal.offset.x), static_cast<int>(signal.offset.y), signal.facing}) != removed.end(); });
			if (given != nullptr) {
				/* The given row's beside run from the last element's piece up to the merge tile's join; every join is the chevron below (D455), the merge-tile join holds its half piece alone. */
				const size_t from = *row_index(pos.back()), to = *row_index(0);
				for (size_t i = std::min(from, to); i < std::max(from, to); i++) drawn_here.emplace_back((*given)[i].tile, TrackBits{TrackdirToTrack((*given)[i].trackdir)});
			}
			for (int m = 0; m <= pos.back(); m++) {
				const JunctionPiece &piece = *at(m);
				if (given != nullptr && !is_axis(m)) {
					/* A touch point beside a diagonal piece: one axis piece across the shared tile, from the ML half's exit edge to the row half (a trailing crossover). */
					const size_t e = std::ranges::find(pos, m) - pos.begin();
					if (e < pos.size() && e % 2 == 0) drawn_here.emplace_back(piece.tile, TrackBits{AxisToTrack(DiagDirToAxis(TrackdirToExitdir(piece.trackdir)))});
					continue;
				}
				const TileIndexDiffC row_tile = *row_of(m);
				if (!is_axis(m)) {
					drawn_here.emplace_back(row_tile, TrackBits{TrackdirToTrack(piece.trackdir)});
					continue;
				}
				const size_t e = std::ranges::find(pos, m) - pos.begin();
				const Track piece_axis = TrackdirToTrack(piece.trackdir);
				const DiagDirection piece_exit = TrackdirToExitdir(piece.trackdir);
				if (e < pos.size() && e % 2 == 0) {
					drawn_here.emplace_back(piece.tile, TrackBits{JunctionCornerTrack(side_of(piece), piece_exit)});
					const Track h2 = JunctionCornerTrack(ReverseDiagDir(side_of(piece)), ReverseDiagDir(piece_exit));
					drawn_here.emplace_back(row_tile, (m == 0 || given != nullptr) ? TrackBits{h2} : TrackBits{piece_axis, h2});
				} else if (given == nullptr && plain(m)) {
					drawn_here.emplace_back(row_tile, TrackBits{piece_axis});
				}
			}
			for (const auto &[tile, bits] : drawn_here) AddJunctionTracks(scratch, tile, bits);
			for (size_t e = 1; e < pos.size(); e += 2) {
				const JunctionPiece &piece = *at(pos[e]);
				if (std::ranges::none_of(scratch.signals, [&](const JunctionSignal &signal) { return signal.offset.x == piece.tile.x && signal.offset.y == piece.tile.y && signal.facing == piece.trackdir; })) {
					scratch.signals.push_back({piece.tile, piece.trackdir, spec.signal_type, SignalVariant::Electric});
				}
				const Trackdir row_td = given != nullptr ? (*given)[*row_index(pos[e])].trackdir : piece.trackdir; // the row's own track (the opposite half beside a diagonal piece)
				scratch.signals.push_back({*row_of(pos[e]), ReverseTrackdir(row_td), e + 2 == pos.size() ? SignalType::Exit : SignalType::Combo, SignalVariant::Electric});
			}
			mark_entry(scratch.signals, joiner, joiner_k);

			JunctionTopology topology;
			const JunctionCheckContext ctx = MakePassCheckContext(scratch, routes, network, spec, topology);
			std::vector<RuleViolation> violations = CheckPrio(ctx);
			for (RuleViolation &violation : CheckPrioReach(ctx)) violations.push_back(std::move(violation));
			for (const RuleViolation &violation : violations) {
				bool mine = false;
				for (int m = 0; m <= std::max(coverage, pos.back()) && !mine; m++) {
					const std::optional<TileIndexDiffC> row_tile = row_of(m);
					mine = (violation.tile.x == at(m)->tile.x && violation.tile.y == at(m)->tile.y) || (row_tile.has_value() && violation.tile.x == row_tile->x && violation.tile.y == row_tile->y);
				}
				if (!mine) continue;
				if (leaves == 1) first_leaf = fmt::format("{} at {},{}", violation.message, violation.tile.x, violation.tile.y);
				return false;
			}

			/* Taken: the scratch becomes the layout. */
			for (const auto &[tile, bits] : drawn_here) tracks[{tile.x, tile.y}] |= bits;
			std::erase_if(tokens, [&](const JunctionTrackSignal &token) { return token.reason == Reason::Fill && std::ranges::find(removed, std::tuple{static_cast<int>(token.tile.x), static_cast<int>(token.tile.y), token.facing}) != removed.end(); });
			for (size_t e = 1; e < pos.size(); e += 2) {
				const JunctionPiece &piece = *at(pos[e]);
				tokens.push_back({static_cast<uint16_t>(r), Reason::Prio, piece.tile, piece.trackdir});
				tokens.push_back({static_cast<uint16_t>(r), Reason::Prio, *row_of(pos[e]), ReverseTrackdir(given != nullptr ? (*given)[*row_index(pos[e])].trackdir : piece.trackdir)});
			}
			for (int m = 0; m <= pos.back(); m++) {
				if (prio_beside != nullptr && is_axis(m) && row_of(m).has_value()) prio_beside->emplace_back(*row_of(m), at(m)->tile);
			}
			layout = std::move(scratch);
			return true;
		};
		const std::function<bool(size_t)> search = [&](size_t i) {
			if (i == static_cast<size_t>(2 * pairs + 1)) return try_leaf();
			for (int m = pos.back() + 1; m + static_cast<int>(2 * pairs - i) <= last_piece && row_free(m); m++) {
				if (!candidate(m, i % 2 == 0)) continue;
				pos.push_back(m);
				if (search(i + 1)) return true;
				pos.pop_back();
				if (leaves >= MAX_PRIO_LEAVES) return false;
			}
			return false;
		};

		if (given != nullptr && !row_index(0).has_value()) {
			if (why != nullptr && why->empty()) *why = fmt::format("the given prio row does not reach its merge at {},{}", M.x, M.y);
			continue;
		}
		bool placed = false;
		if (plain(0) && row_free(0)) {
			for (pairs = std::max(n_prio / 2 - 1, 0); pairs < std::max(n_prio, 1) && !placed && leaves < MAX_PRIO_LEAVES; pairs++) {
				pos = {0};
				placed = search(1);
			}
		}
		if (placed) placed_merges.insert({M.x, M.y});
		if (!placed && why != nullptr && why->empty()) {
			*why = given != nullptr ? fmt::format("the given prio row at {},{} fails every leaf; first leaf: {}", M.x, M.y, first_leaf.empty() ? "no violation on its tiles (no leaf drawn)" : first_leaf) : fmt::format("the prio element at {},{} does not fit", M.x, M.y);
		}
	}
	return tokens;
}
