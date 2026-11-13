/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_pipeline.cpp The graph pipeline's driver: bridge cut, seed, detours and the terraform entry point (DESIGN-G1 §13). */

#include "../stdafx.h"

#include "junction_pipeline.h"

#include "junction_spec.h"
#include "junction_rules_internal.h"
#include "line/junction_line.h"
#include "placement/junction_levels.h"
#include "placement/junction_render.h"
#include "signals/junction_signals.h"
#include "topology/junction_chain.h"

#include "../direction_func.h"
#include "../map_func.h"
#include "../track_func.h"

#include <algorithm>
#include <climits>
#include <functional>
#include <map>
#include <numeric>
#include <optional>
#include <set>
#include <tuple>

#include "../safeguards.h"

JunctionTrackPlacement PlaceJunctionTrackGraph(const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, const JunctionTrackOrder &order, const JunctionTrackStructure &structure, const JunctionTrackPrograms &programs, const JunctionSpec &spec, const JunctionPlacementChoices &choices, const JunctionFacts &facts, TileIndexDiffC offset, std::string *error, bool heights, bool terraform, const JunctionTerrain *terrain, const JunctionPlacementShared *shared, std::pair<long, long> *incremental_check)
{
	if (terraform && terrain == nullptr) {
		if (error != nullptr) *error = "terraform needs the terrain";
		return {};
	}
	JunctionTrackPlacement placement = PlaceTrackGraph(topology, paths, graph, order, structure, programs, spec, &choices, error, {}, {}, &facts, offset, heights && !terraform, terraform, shared);
	if (!terraform || placement.nodes.empty()) return placement;

	/* The height solve (DESIGN-G1 §15.3) on the render of the placement. */
	std::vector<JunctionTrackRoute> routes;
	std::string text;
	const JunctionLayout rendered = RenderJunctionTrackPlacement(topology, paths, graph, structure, programs, placement, &text, &routes);
	auto fail = [&](const std::string &reason) {
		if (error != nullptr) *error = reason;
		return JunctionTrackPlacement{};
	};
	if (rendered.tracks.empty()) return fail("the placement does not render: " + text);
	/* The prio elements the signal pass draws are track tiles too (H3): the solve takes the signalled layout and the tiles beside the main line. */
	JunctionLayout layout = rendered;
	std::vector<std::pair<TileIndexDiffC, TileIndexDiffC>> prio_beside;
	SignalJunctionTrackRoutes(layout, routes, topology, paths, graph, programs, spec, nullptr, &prio_beside, &placement.prio);
	if (!SolveJunctionLevels(*terrain, facts, offset, JunctionRampLevels(spec), spec.tl, placement.nodes, placement.ports, layout, routes, prio_beside, placement.terraform, placement.height_cost, placement.track_levels, &text, incremental_check)) return fail(text);
	return placement;
}

/**
 * Append the compositions of n into the given number of parts, each 1..JUNCTION_BRIDGE_MAX_CROSSERS, in descending lexicographic order of part sizes.
 * @param n The crossings left to place.
 * @param parts The parts left.
 * @param current The parts so far.
 * @param[out] out Receives the compositions.
 */
static void AppendBridgeCompositions(size_t n, size_t parts, std::vector<size_t> &current, std::vector<std::vector<size_t>> &out)
{
	if (parts == 1) {
		if (n >= 1 && n <= JUNCTION_BRIDGE_MAX_CROSSERS) {
			current.push_back(n);
			out.push_back(current);
			current.pop_back();
		}
		return;
	}
	for (int p = static_cast<int>(std::min(JUNCTION_BRIDGE_MAX_CROSSERS, n - (parts - 1))); p >= 1; p--) {
		if (n - p > (parts - 1) * JUNCTION_BRIDGE_MAX_CROSSERS) continue;
		current.push_back(p);
		AppendBridgeCompositions(n - p, parts - 1, current, out);
		current.pop_back();
	}
}

/**
 * Append the tuples of bridge counts that sum to a total, in ascending lexicographic order.
 * @param low The least count per bridge.
 * @param high The greatest count per bridge.
 * @param remaining The total left to spend.
 * @param current The counts so far.
 * @param[out] out Receives the tuples.
 */
static void AppendBridgeCounts(const std::vector<size_t> &low, const std::vector<size_t> &high, size_t remaining, std::vector<size_t> &current, std::vector<std::vector<size_t>> &out)
{
	const size_t i = current.size();
	if (i == low.size()) {
		if (remaining == 0) out.push_back(current);
		return;
	}
	for (size_t c = low[i]; c <= high[i] && c <= remaining; c++) {
		current.push_back(c);
		AppendBridgeCounts(low, high, remaining - c, current, out);
		current.pop_back();
	}
}

JunctionTrackStructure CutJunctionTrackBridges(const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, const JunctionTrackOrder &order, const JunctionTrackStructure &structure, const JunctionTrackPrograms &programs, const JunctionSpec &spec, JunctionTrackPlacement *placement, std::string *error)
{
	/* The doubled bridges and, per bridge and bridge count, the compositions of its crossing list. */
	std::vector<size_t> low, high;
	std::vector<std::vector<std::vector<std::vector<size_t>>>> compositions;
	size_t low_total = 0, high_total = 0;
	for (const JunctionTrackBridge &bridge : structure.bridges) {
		if (!bridge.doubled) continue;
		const size_t n = bridge.crossed.size();
		low.push_back((n + JUNCTION_BRIDGE_MAX_CROSSERS - 1) / JUNCTION_BRIDGE_MAX_CROSSERS);
		high.push_back(n);
		low_total += low.back();
		high_total += n;
		compositions.emplace_back();
		for (size_t c = low.back(); c <= n; c++) {
			compositions.back().emplace_back();
			std::vector<size_t> current;
			AppendBridgeCompositions(n, c, current, compositions.back().back());
		}
	}

	/* The failure reported when no cut places: the default cut's, the first one tried (the series of at most the cap from the start). */
	std::string default_error;
	bool first_cut = true;
	for (size_t total = low_total; total <= high_total; total++) {
		std::vector<std::vector<size_t>> tuples;
		std::vector<size_t> counts;
		AppendBridgeCounts(low, high, total, counts, tuples);

		bool have = false;
		JunctionTrackStructure best_structure;
		JunctionTrackPlacement best;
		for (const std::vector<size_t> &tuple : tuples) {
			std::vector<size_t> choice(tuple.size(), 0);
			for (;;) {
				/* The cut structure: each doubled bridge replaced in place by its parts. */
				JunctionTrackStructure cut = structure;
				cut.bridges.clear();
				size_t d = 0;
				for (const JunctionTrackBridge &bridge : structure.bridges) {
					if (!bridge.doubled) {
						cut.bridges.push_back(bridge);
						continue;
					}
					size_t first = 0;
					for (size_t part : compositions[d][tuple[d] - low[d]][choice[d]]) {
						cut.bridges.push_back({bridge.edge, std::vector<uint32_t>(bridge.crossed.begin() + first, bridge.crossed.begin() + first + part), true});
						first += part;
					}
					d++;
				}
				std::stable_sort(cut.bridges.begin(), cut.bridges.end(), [](const JunctionTrackBridge &a, const JunctionTrackBridge &b) { return a.edge < b.edge; });

				std::string why;
				JunctionTrackPlacement placed = PlaceJunctionTrackGraph(topology, paths, graph, order, cut, programs, spec, &why);
				const bool is_first = first_cut;
				first_cut = false;
				if (placed.nodes.empty()) {
					if (is_first) default_error = why;
				} else if (!have || placed.size.x < best.size.x || (placed.size.x == best.size.x && placed.size.y < best.size.y)) {
					have = true;
					best = std::move(placed);
					best_structure = std::move(cut);
				}

				/* The next combination of compositions, the last bridge varying fastest. */
				size_t i = choice.size();
				bool more = false;
				while (i > 0 && !more) {
					i--;
					if (++choice[i] < compositions[i][tuple[i] - low[i]].size()) {
						more = true;
					} else {
						choice[i] = 0;
					}
				}
				if (!more) break;
			}
		}
		if (have) {
			if (placement != nullptr) *placement = std::move(best);
			if (error != nullptr) error->clear();
			return best_structure;
		}
	}
	if (placement != nullptr) *placement = JunctionTrackPlacement{};
	if (error != nullptr) *error = default_error;
	return structure;
}

JunctionGraphSeed MakeJunctionGraphSeed(const JunctionTopology &topology, const JunctionSpec &spec, std::string *error)
{
	JunctionGraphSeed seed{};
	seed.topology = &topology;
	std::string reason;
	auto fail = [&]() {
		if (error != nullptr) *error = reason;
		seed.routes.clear();
		return seed;
	};
	seed.paths = ExpandJunctionLanes(topology, &reason);
	if (seed.paths.empty()) return fail();
	seed.graph = BuildJunctionTrackGraph(topology, seed.paths, &reason);
	if (seed.graph.edges.empty()) return fail();
	const JunctionTrackOrder order = OrderJunctionTrackGraph(topology, seed.paths, seed.graph, &reason);
	if (order.rows.empty()) return fail();
	const JunctionTrackStructure structure = StructureJunctionTrackGraph(topology, seed.paths, seed.graph, order, &reason);
	if (structure.stack.empty()) return fail();
	seed.programs = ProgramJunctionTrackGraph(topology, seed.paths, seed.graph, order, structure, &reason);
	if (seed.programs.edges.empty()) return fail();
	seed.structure = CutJunctionTrackBridges(topology, seed.paths, seed.graph, order, structure, seed.programs, spec, &seed.placement, &reason);
	if (seed.placement.nodes.empty()) return fail();
	RenderJunctionTrackPlacement(topology, seed.paths, seed.graph, seed.structure, seed.programs, seed.placement, &reason, &seed.routes);
	if (seed.routes.empty()) return fail();
	return seed;
}

/** Whether a piece's tag is a run piece of (edge, run, copy). */
static bool IsRunPiece(const JunctionTrackPieceTag &tag, const JunctionDetourEdit &edit)
{
	return tag.token == JunctionTrackPieceTag::RUN && tag.edge == edit.edge && tag.run == edit.run && tag.copy == edit.copy;
}

std::vector<JunctionTrackRoute> ApplyJunctionDetours(const std::vector<JunctionTrackRoute> &routes, const std::vector<JunctionDetourEdit> &edits, const JunctionSpec &spec, TileIndexDiffC offset, std::string *why)
{
	std::vector<JunctionTrackRoute> result = routes;
	auto fail = [&](const std::string &reason) {
		if (why != nullptr) *why = reason;
		return std::vector<JunctionTrackRoute>{};
	};
	if (routes.empty()) return fail("no routes");

	for (const JunctionDetourEdit &edit : edits) {
		const std::string name = fmt::format("edit (edge {} run {} copy {} column {} side {} variant {}): ", edit.edge, edit.run, edit.copy, edit.column, edit.side, edit.variant);
		if (edit.side != 1 && edit.side != -1) return fail(name + "not a side");

		/* The carrying routes and each one's run: the first to the last piece tagged with it. */
		struct Carrier {
			size_t route;
			size_t first;
			size_t last;
		};
		std::vector<Carrier> carriers;
		for (size_t r = 0; r < result.size(); r++) {
			const std::vector<JunctionTrackPieceTag> &tags = result[r].tags;
			size_t first = tags.size();
			size_t last = 0;
			for (size_t i = 0; i < tags.size(); i++) {
				if (!IsRunPiece(tags[i], edit)) continue;
				if (first == tags.size()) first = i;
				last = i;
			}
			if (first == tags.size()) continue;
			for (size_t i = first; i <= last; i++) {
				if (!IsRunPiece(tags[i], edit)) return fail(name + "the run is not contiguous");
			}
			carriers.push_back({r, first, last});
		}
		if (carriers.empty()) return fail(name + "no route carries the run");

		/* A straight X run on one row, the same on every carrier, with the piece strictly inside. */
		const std::vector<JunctionPiece> &reference = result[carriers[0].route].pieces;
		const size_t length = carriers[0].last - carriers[0].first + 1;
		for (const Carrier &c : carriers) {
			if (c.last - c.first + 1 != length) return fail(name + "the routes disagree on the run");
			for (size_t i = 0; i < length; i++) {
				const JunctionPiece &piece = result[c.route].pieces[c.first + i];
				const JunctionPiece &ref = reference[carriers[0].first + i];
				if (piece.on_bridge || (piece.trackdir != Trackdir::X_NE && piece.trackdir != Trackdir::X_SW)) return fail(name + "not an X run");
				if (piece.tile.x != ref.tile.x || piece.tile.y != ref.tile.y || piece.trackdir != ref.trackdir || piece.tile.y != reference[carriers[0].first].tile.y) return fail(name + "the routes disagree on the run");
			}
		}
		if (length < 3) return fail(name + "the run has no interior piece");
		const JunctionPiece p = reference[carriers[0].first];
		const JunctionPiece q = reference[carriers[0].last];

		/* The failing column clamped strictly inside the seed run, which is only the reference shape. */
		const int direction = q.tile.x > p.tile.x ? 1 : -1;
		const int along = std::clamp(direction * (edit.column - offset.x - p.tile.x), 1, static_cast<int>(length) - 2);
		const int middle = (static_cast<int>(length) - 1) / 2;

		/* The tiles taken by every other piece: the carriers' run interiors are being replaced. */
		std::set<std::pair<int, int>> taken;
		std::set<std::pair<int, int>> own; ///< The run's own interior tiles: a new piece on one is not a new tile (a bridge may pass over it).
		for (size_t r = 0; r < result.size(); r++) {
			const auto carrier = std::ranges::find_if(carriers, [&](const Carrier &c) { return c.route == r; });
			for (size_t i = 0; i < result[r].pieces.size(); i++) {
				if (carrier != carriers.end() && i > carrier->first && i < carrier->last) {
					own.emplace(result[r].pieces[i].tile.x, result[r].pieces[i].tile.y);
					continue;
				}
				taken.emplace(result[r].pieces[i].tile.x, result[r].pieces[i].tile.y);
			}
		}

		/* A shifted run of one piece has no free link, and the new tiles may meet no other route or bridge head. */
		int skipped = 0;
		auto build = [&](const JunctionRaster &raster, int n2, std::string &reason) {
			if (n2 == 0) {
				reason = "a shifted run of one piece";
				return false;
			}
			for (size_t i = 1; i + 1 < raster.pieces.size(); i++) {
				const TileIndexDiffC tile = raster.pieces[i].tile;
				if (taken.contains({tile.x, tile.y}) && !own.contains({tile.x, tile.y})) {
					reason = fmt::format("collision at {},{}", tile.x, tile.y);
					return false;
				}
			}

			/* The curve gaps of every carrier's edited route hold on its units (rule 2 of the template). */
			for (const Carrier &c : carriers) {
				const std::vector<JunctionPiece> &old = result[c.route].pieces;
				std::vector<JunctionPiece> edited(old.begin(), old.begin() + c.first);
				edited.insert(edited.end(), raster.pieces.begin(), raster.pieces.end());
				edited.insert(edited.end(), old.begin() + c.last + 1, old.end());
				std::vector<JunctionCurveHand> hands;
				std::vector<int> starts;
				int unit = 0;
				for (size_t i = 0; i < edited.size(); i++) {
					if (i > 0) {
						const DirDiff diff = DirDifference(TrackdirToDirection(edited[i - 1].trackdir), TrackdirToDirection(edited[i].trackdir));
						if (diff == DirDiff::Left45 || diff == DirDiff::Right45) {
							hands.push_back(diff == DirDiff::Left45 ? JunctionCurveHand::Left : JunctionCurveHand::Right);
							starts.push_back(unit);
						}
					}
					unit += edited[i].units;
				}
				for (const JunctionCurveGap &gap : JunctionCurveGaps(hands, spec.tl, spec.curve_speed, spec.reference_speed)) {
					if (starts[gap.j] - starts[gap.i] >= gap.min_gap) continue;
					reason = fmt::format("curve gap {} below {}", starts[gap.j] - starts[gap.i], gap.min_gap);
					return false;
				}
			}
			if (skipped < edit.variant) {
				skipped++;
				reason = "skipped candidate";
				return false;
			}
			return true;
		};
		JunctionRaster chosen;
		std::string reason;
		/* From the clamped column toward the run's middle and on to its far interior end: the first column with a candidate. */
		std::string first_reason;
		bool found = false;
		for (int a = along, step = along > middle ? -1 : 1; a >= 1 && a <= static_cast<int>(length) - 2 && !found; a += step) {
			found = ChooseJunctionDetour(p, q, p.tile.x + direction * a, edit.side, build, &chosen, &reason);
			if (!found && reason != "link too short for a detour") first_reason = reason;
		}
		if (!found) return fail(name + (first_reason.empty() ? reason : first_reason));

		/* The run's interior replaced on every carrier, the new pieces tagged as the run. */
		const JunctionTrackPieceTag tag{edit.edge, JunctionTrackPieceTag::RUN, edit.run, edit.copy};
		for (const Carrier &c : carriers) {
			JunctionTrackRoute &route = result[c.route];
			route.pieces.erase(route.pieces.begin() + c.first + 1, route.pieces.begin() + c.last);
			route.tags.erase(route.tags.begin() + c.first + 1, route.tags.begin() + c.last);
			route.pieces.insert(route.pieces.begin() + c.first + 1, chosen.pieces.begin() + 1, chosen.pieces.end() - 1);
			route.tags.insert(route.tags.begin() + c.first + 1, chosen.pieces.size() - 2, tag);
		}
	}
	return result;
}
