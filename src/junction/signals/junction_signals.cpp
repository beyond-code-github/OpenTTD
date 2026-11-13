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

std::vector<std::pair<bool, size_t>> JunctionRouteEvents(const JunctionTrackGraph &graph, const JunctionTrackPrograms &programs, const std::vector<JunctionTrackRoute> &routes, size_t r)
{
	const JunctionTrackRoute &route = routes[r];
	std::vector<std::pair<bool, size_t>> events;
	/* The node piece at each boundary between consecutive edges: a split's belongs to the out-edge, a merge's to the in-edge. */
	for (size_t i = 1; i < route.tags.size(); i++) {
		if (route.tags[i].edge == route.tags[i - 1].edge) continue;
		const JunctionTrackEdge &in = graph.edges[route.tags[i - 1].edge];
		if (!in.to.node) continue;
		const bool split = graph.nodes[in.to.index].split;
		events.emplace_back(split, split ? i : i - 1);
	}
	/* The lane split and merge of each doubled edge: switches both copies pass, located by the copy that bends there. */
	for (const uint32_t e : graph.path_edges[route.path]) {
		if (programs.copies[e].empty()) continue;
		size_t sibling = routes.size();
		for (size_t s = 0; s < routes.size(); s++) {
			if (routes[s].path == route.path && routes[s].copy != route.copy) sibling = s;
		}
		if (sibling == routes.size()) continue;
		const JunctionTrackRoute &copy0 = route.copy == 0 ? route : routes[sibling];
		const JunctionTrackRoute &copy1 = route.copy == 0 ? routes[sibling] : route;
		const auto with_role = [](const std::vector<JunctionTrackStep> &program, JunctionTrackStep::Role role) {
			for (size_t t = 0; t < program.size(); t++) {
				if (program[t].kind == JunctionTrackStep::Kind::SBend && program[t].role == role) return t;
			}
			return program.size();
		};
		const size_t split_token = with_role(programs.copies[e], JunctionTrackStep::Role::LaneSplit);
		const size_t merge_token = with_role(programs.edges[e], JunctionTrackStep::Role::LaneMerge);
		const auto on_tile = [&](TileIndexDiffC tile) -> std::optional<size_t> {
			for (size_t k = 0; k < route.pieces.size(); k++) {
				if (!route.pieces[k].on_bridge && route.pieces[k].tile.x == tile.x && route.pieces[k].tile.y == tile.y) return k;
			}
			return std::nullopt;
		};
		for (size_t i = 0; i < copy1.tags.size(); i++) {
			if (copy1.tags[i].edge != e || copy1.tags[i].copy != 1 || copy1.tags[i].token != split_token) continue;
			if (const std::optional<size_t> k = on_tile(copy1.pieces[i].tile); k.has_value()) events.emplace_back(true, *k);
			break;
		}
		for (size_t i = copy0.tags.size(); i-- > 0;) {
			if (copy0.tags[i].edge != e || copy0.tags[i].copy != 0 || copy0.tags[i].token != merge_token) continue;
			if (const std::optional<size_t> k = on_tile(copy0.pieces[i].tile); k.has_value()) events.emplace_back(false, *k);
			break;
		}
	}
	std::ranges::stable_sort(events, [](const std::pair<bool, size_t> &a, const std::pair<bool, size_t> &b) { return a.second != b.second ? a.second < b.second : a.first > b.first; });
	events.erase(std::unique(events.begin(), events.end()), events.end());
	return events;
}

std::vector<JunctionTrackSignal> SignalJunctionTrackRoutes(JunctionLayout &layout, const std::vector<JunctionTrackRoute> &routes, const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, const JunctionTrackPrograms &programs, const JunctionSpec &spec, std::string *why, std::vector<std::pair<TileIndexDiffC, TileIndexDiffC>> *prio_beside, const std::vector<JunctionPrioSpan> *prio)
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
	for (size_t r = 0; r < routes.size(); r++) {
		const auto &movement = topology.movements[paths[routes[r].path].movement];
		if (!topology.groups[movement.first].main || !topology.groups[movement.second].main) continue;
		const std::vector<JunctionPiece> &P = routes[r].pieces;
		const std::vector<SwitchEvent> switches = JunctionRouteEvents(graph, programs, routes, r);
		for (const SwitchEvent &merge : switches) {
			if (merge.first) continue;
			const size_t j = merge.second;
			const JunctionPiece &mp = P[j];
			const TileIndexDiffC M = mp.tile;

			/* 1. The joiner: the merge node n of the event (the last piece of an edge that ends at a node), and the first route of another movement whose lane path has an edge ending at n; k is that route's node piece. */
			if (j + 1 >= P.size() || routes[r].tags[j].edge == routes[r].tags[j + 1].edge || !graph.edges[routes[r].tags[j].edge].to.node) continue;
			const uint16_t node = graph.edges[routes[r].tags[j].edge].to.index;
			std::optional<size_t> joiner, joiner_k;
			for (size_t r2 = 0; r2 < routes.size() && !joiner.has_value(); r2++) {
				if (paths[routes[r2].path].movement == paths[routes[r].path].movement) continue;
				for (const uint32_t e : graph.path_edges[routes[r2].path]) {
					if (!graph.edges[e].to.node || graph.edges[e].to.index != node) continue;
					for (size_t k = routes[r2].tags.size(); k-- > 0;) {
						if (routes[r2].tags[k].edge != e) continue;
						joiner = r2;
						joiner_k = k;
						break;
					}
					break;
				}
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

			/* 3. The requirements, all before anything is written. */
			const auto ml = [&](int s) { return TileIndexDiffC{static_cast<int16_t>(M.x - s * dir.x), static_cast<int16_t>(M.y - s * dir.y)}; };
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
				if (why != nullptr && why->empty()) *why = fmt::format("the prio element at {},{} does not fit", M.x, M.y);
				continue;
			}

			/* 4. Write. */
			if (prio != nullptr) {
				const auto span = std::ranges::find_if(*prio, [&](const JunctionPrioSpan &candidate) { return candidate.merge.x == M.x && candidate.merge.y == M.y; });
				if (span == prio->end()) continue;
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
		const std::vector<SwitchEvent> switches = JunctionRouteEvents(graph, programs, routes, r);
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
		const auto &movement = topology.movements[paths[routes[r].path].movement];
		if (topology.groups[movement.first].main && topology.groups[movement.second].main) continue;
		const std::vector<JunctionPiece> &P = routes[r].pieces;
		const std::vector<SwitchEvent> events = JunctionRouteEvents(graph, programs, routes, r);
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
	for (const auto &[r2, k] : entries) {
		const std::vector<JunctionPiece> &P = routes[r2].pieces;
		const std::vector<SwitchEvent> switches = JunctionRouteEvents(graph, programs, routes, r2);
		for (size_t i = k; i-- > 0;) {
			if (std::ranges::any_of(switches, [&](const SwitchEvent &sw) { return sw.second > i && sw.second < k; })) break;
			if (!is_signalled(P[i])) continue;
			for (JunctionSignal &signal : signals) {
				if (signal.offset.x == P[i].tile.x && signal.offset.y == P[i].tile.y && TrackdirToTrack(signal.facing) == TrackdirToTrack(P[i].trackdir)) signal.type = SignalType::Entry;
			}
			break;
		}
	}
	for (const auto &[tile, bits] : drawn) AddJunctionTracks(layout, tile, bits);
	layout.signals.insert(layout.signals.end(), signals.begin(), signals.end());
	return tokens;
}
