/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_placement.cpp Horizontal placement of a programmed track graph (DESIGN-G1 §11.8, §14). */

#include "../../stdafx.h"

#include "junction_placement.h"

#include "../junction_spec.h"
#include "../junction_track_geometry.h"
#include "../junction_route.h"
#include "../junction_rules_internal.h"
#include "../topology/junction_chain.h"
#include "../line/junction_line.h"
#include "junction_positions.h"

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

/** The kinds of element a placement run lies between (D217, DESIGN-G1 §16.1). */
enum class FloorKind : uint8_t { Port, Fan, Tile, Corner, SBend, Heads, Node, PrioNode, Prio, LaneSplit, LaneMerge };

/**
 * The tracks a prio element draws at one of its steps (DESIGN-G1 12.4), the one pattern occupancy and the placement's JunctionPrioSpan share.
 * @param step The step upstream of the joiner merge (0 at the merge's column).
 * @param side The side of the main line the element's row lies on.
 * @param exit_edge The edge the main line leaves its tiles by (SW eastbound, NE westbound).
 * @return The bits on the element's row and those on the main line (lane row) tile of the same column.
 */
static std::pair<TrackBits, TrackBits> JunctionPrioStepBits(int step, DiagDirection side, DiagDirection exit_edge)
{
	const Track h1 = JunctionCornerTrack(side, exit_edge);
	const Track h2 = JunctionCornerTrack(ReverseDiagDir(side), ReverseDiagDir(exit_edge));
	TrackBits row;
	if (step == 0) {
		row = TrackBits{h2};
	} else if (step % 2 == 0) {
		row = TrackBits{Track::X, h2};
	} else {
		row = TrackBits{Track::X};
	}
	return {row, step % 2 == 0 ? TrackBits{h1} : TrackBits{}};
}
/**
 * The least run in tiles between two neighbouring elements: the one table of every run floor in placement (D217, DESIGN-G1 §16.1). A pair not listed has the chain run.
 * @param before The kind of the element the run starts at.
 * @param after The kind of the element the run ends at.
 * @param spec The junction spec (the waiting room reads the train length).
 * @return The least run.
 */
static int JunctionRunFloor(FloorKind before, FloorKind after, const JunctionSpec &spec)
{
	using K = FloorKind;
	if (before == K::Port && after == K::Fan) return 2; // The fan starts 2 tiles inside the port: the signal tile and the boundary tile (D115a, D206).
	if (before == K::Fan && after == K::Node) return 2; // The run from a fan to the exit split is the chain run (D217).
	if (before == K::Fan && after == K::Port) return 2; // The last fan tile to its port: the port stub (D206, D115a).
	if (before == K::Node && after == K::Port) return 2; // A branch port's stub below its node (D206, D115a).
	if (before == K::PrioNode && after == K::Fan) return 2; // The joiner merge to the out fan: the chain run (D217).
	if (before == K::PrioNode && after == K::Port) return 2; // The joiner merge to an unfanned port: the chain run and port stub (D217, D206).
	if (before == K::Node && after == K::Prio) return 1; // The exit split stays outside the prio element's coverage; its row ends one step on (DESIGN-G1 §12.4, RECON-prio-element §3).
	if (before == K::LaneMerge && after == K::Prio) return 2; // The element's row must not meet the lane merge's S-bend (RECON-prio-element §3-4).
	if (before == K::Node && after == K::LaneSplit) return 2; // The exit split to the lane split: the chain run, a minimum so series bridges have slack (D217, DESIGN-G1 §11.9b).
	if (before == K::LaneSplit && after == K::Heads) return 2; // Shared split tile, one plain piece, then the near head (D145, D113, D117, D118).
	if (before == K::Heads && after == K::Heads) return 2; // Series bridges: the next near head 2 tiles past the last far head (DESIGN-G1 §11.9b).
	if (before == K::Heads && after == K::LaneMerge) return 2; // The last far head to the lane merge (D145: x_hwA - x_lm = 2).
	if (before == K::Node && after == K::Node) return static_cast<int>(spec.tl) + 1; // The waiting room of an exit path, from its first split to the first merge: 2 dy - 1 >= 2 tl units (rule WaitingRoom).
	return 2; // The chain run: a signal tile and a boundary tile between elements (D217).
}

JunctionTrackPlacement PlaceTrackGraph(const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, const JunctionTrackOrder &order, const JunctionTrackStructure &structure, const JunctionTrackPrograms &programs, const JunctionSpec &spec, const JunctionPlacementChoices *fixed, std::string *error, const std::vector<std::pair<int, int>> &x_floors, const std::vector<std::pair<int, int>> &y_floors, const JunctionFacts *facts, TileIndexDiffC offset, bool heights, bool classes_only, const JunctionPlacementShared *shared, JunctionPlacementShared *make_shared, JunctionPlacementShared *record, JunctionPlacementSystem *system, const JunctionBendSizes *sizes, const JunctionSystemPoint *at)
{
	auto str = [](int v) { return fmt::format("{}", v); };
	using Step = JunctionTrackStep;
	using StepKind = JunctionTrackStep::Kind;
	auto fail = [&](const std::string &reason) {
		if (error != nullptr) *error = reason;
		return JunctionTrackPlacement{};
	};
	if (graph.edges.empty() || programs.edges.size() != graph.edges.size() || programs.copies.size() != graph.edges.size()) return fail("a placement needs the programs of a track graph");
	if (fixed != nullptr) {
		if (fixed->m.size() != graph.edges.size() || fixed->corner.size() != graph.edges.size()) return fail("the choices do not fit the programs of this track graph");
		for (size_t e = 0; e < graph.edges.size(); e++) {
			if (fixed->m[e].size() != programs.edges[e].size() || fixed->corner[e].size() != programs.edges[e].size()) return fail("the choices do not fit the programs of this track graph");
		}
		for (const std::vector<std::vector<int>> *shaped : {&fixed->over, &fixed->over_after}) {
			if (shaped->empty()) continue;
			if (shaped->size() != graph.edges.size()) return fail("the choices do not fit the programs of this track graph");
			for (size_t e = 0; e < graph.edges.size(); e++) {
				if ((*shaped)[e].size() != programs.edges[e].size()) return fail("the choices do not fit the programs of this track graph");
			}
		}
	}
	/* A recording given a point detects the lazy families there, not at its flat solves (DESIGN-G3 §G3.6, R3). */
	const bool at_mode = at != nullptr && fixed != nullptr && system != nullptr;
	std::string at_error; ///< Receives the text of a failed build or solve when the caller gave no error.
	if (at_mode && error == nullptr) error = &at_error;
	std::vector<std::string> at_failed; ///< The failed builds and solves of a recording at a point, for JunctionPlacementSystem::failed.
	const std::vector<JunctionPortSlot> slots = JunctionPortSlots(topology);
	const int n = static_cast<int>(slots.size());
	auto group_of = [&](int slot) -> const JunctionLaneGroup & { return topology.groups[slots[slot].group]; };
	const std::vector<int> row_of = JunctionSlotRows(topology, paths, error);
	if (row_of.empty()) return JunctionTrackPlacement{};
	const int rows = static_cast<int>(order.rows.size());
	const uint32_t edge_count = static_cast<uint32_t>(graph.edges.size());
	const uint32_t NO_ROW = Step::NO_ROW;
	auto end_level = [&](const JunctionTrackEnd &end) {
		if (end.node) return order.node_level[end.index];
		return group_of(end.index).main ? 2 * row_of[end.index] + 1 : 2 * rows + 1;
	};
	auto is_row_edge = [&](uint32_t e) {
		const int level = end_level(graph.edges[e].from);
		return level == end_level(graph.edges[e].to) && level % 2 == 1;
	};

	/* Headings: 0 north, 1 east, 2 south, 3 west. */
	static constexpr int VX[4] = {0, 1, 0, -1};
	static constexpr int VY[4] = {-1, 0, 1, 0};
	std::vector<bool> east(rows, true);
	for (int s = 0; s < n; s++) {
		if (group_of(s).main && group_of(s).in) east[row_of[s]] = group_of(s).side == JunctionSide::W;
	}

	/* Stack items and rows. */
	const int items = static_cast<int>(structure.stack.size());
	std::vector<RowItems> row_items;
	for (int r = 0; r < rows; r++) row_items.push_back(FindRowItems(structure.stack, r));
	std::vector<int> row_first(rows, -1), row_last(rows, -1);
	for (int i = 0; i < items; i++) {
		const JunctionTrackStackItem &item = structure.stack[i];
		if (row_first[item.row] < 0) row_first[item.row] = i;
		row_last[item.row] = i;
	}

	/* The roles of the edges, from the graph and the programs (not from a row index). */
	auto IsInEdge = [&](uint32_t e) { return JunctionIsInEdge(graph, order, programs, e); };
	auto IsOutEdge = [&](uint32_t e) { return JunctionIsOutEdge(graph, order, programs, e); };
	auto IsLegEdge = [&](uint32_t e) { return JunctionIsLegEdge(graph, order, e); };
	auto IsDoubledEdge = [&](uint32_t e) { return JunctionIsDoubledEdge(programs, e); };

	/* The shape of every row: its in edge, leg and out edge, a lookup filled here from the roles. */
	std::vector<uint32_t> in_edge(rows, NO_ROW), leg_edge(rows, NO_ROW), out_edge(rows, NO_ROW);
	for (uint32_t e = 0; e < edge_count; e++) {
		const JunctionTrackEdge &edge = graph.edges[e];
		if (IsInEdge(e)) {
			if (in_edge[(order.node_level[edge.to.index] - 1) / 2] != NO_ROW) return fail("a lane other than in, one leg, out is not ruled yet");
			in_edge[(order.node_level[edge.to.index] - 1) / 2] = e;
		} else if (IsOutEdge(e)) {
			if (out_edge[(order.node_level[edge.from.index] - 1) / 2] != NO_ROW) return fail("a lane other than in, one leg, out is not ruled yet");
			out_edge[(order.node_level[edge.from.index] - 1) / 2] = e;
		} else if (IsLegEdge(e)) {
			if (leg_edge[(order.node_level[edge.from.index] - 1) / 2] != NO_ROW) return fail("a lane other than in, one leg, out is not ruled yet");
			leg_edge[(order.node_level[edge.from.index] - 1) / 2] = e;
		}
	}
	for (int r = 0; r < rows; r++) {
		if (in_edge[r] == NO_ROW || leg_edge[r] == NO_ROW || out_edge[r] == NO_ROW || row_items[r].lane == NO_ROW) return fail("a lane other than in, one leg, out is not ruled yet");
	}
	std::vector<std::vector<int>> bridges_of(edge_count);
	for (size_t b = 0; b < structure.bridges.size(); b++) bridges_of[structure.bridges[b].edge].push_back(static_cast<int>(b));

	/* The curve size and the rule constants. */
	const int cl = spec.curve_limit ? static_cast<int>(DeriveCurveSpacing(spec.reference_speed, spec.curve_speed, spec.tl)) : 2;
	int h = std::min(2 * static_cast<int>(spec.tl) - 1, std::max(2, cl));
	if (h % 2 == 0) h++;
	const int k = (h - 1) / 2;
	const int prio = spec.prio_tiles;
	/* The prio element's coverage in tiles: joins at even offsets 0..even_prio upstream of the joiner merge (DESIGN-G1 §12.4, RECON-prio-element §3). */
	const int even_prio = prio + (prio & 1);
	/* Not checked here, left to G1c(ii-b), whose render tests (no conflicting track bits) catch them: the band-layer slices (DESIGN-G1 §10.8) and a vertical edge meeting a side track. */

	/* The raster of a bend: its half pieces' tiles relative to the first. A hand of +1 is a clockwise turn. */
	auto bend_tiles = [&](int heading, int hand, int halves, bool corner) {
		const JunctionRaster raster = RasterizeJunctionRoute({{0, 0}, JUNCTION_AXIS_TRACKDIR[heading], JunctionBendMoves(hand, halves, corner)});
		std::vector<TileIndexDiffC> result;
		if (!raster.error.empty() || static_cast<int>(raster.pieces.size()) != halves + 2) return result;
		for (int i = 1; i <= halves; i++) {
			result.push_back({static_cast<int16_t>(raster.pieces[i].tile.x - raster.pieces[1].tile.x), static_cast<int16_t>(raster.pieces[i].tile.y - raster.pieces[1].tile.y)});
		}
		return result;
	};

	/* Points: a variable in the X system and in the Y system. Point 0 is the box's north-west corner. */
	int pt_count = 1;
	auto new_pt = [&]() { return pt_count++; };
	std::vector<int> item_pt(items), node_pt(graph.nodes.size()), port_pt(n);
	for (int &p : item_pt) p = new_pt();
	for (int &p : node_pt) p = new_pt();
	for (int &p : port_pt) p = new_pt();
	const int xe_pt = new_pt(); // The E ports' column.
	const int se_pt = new_pt(); // The S edge's row.

	/* The chains of the edges that are not rows: elements in travel order. */
	struct Elem {
		/**
		 * Tile: one tile (a node, a port or an edge's own point). Corner: a quarter turn. SBend: a sideways step, vertical (size from the columns) or, on a row edge, horizontal (size |item row - its first row| from the rows, rule of the lane change).
		 * Heads: a Y-axis bridge's two heads. Fan (R2): the port stub's S-bend between the port row and an item row, size |port row - item row| from the port rows, 0 allowed (no tiles, first and last at one column).
		 * HeadsPair (R2): the near and far head of a doubled row bridge on one copy's row; copy 1's heads are copy 0's plus the stagger u (D118, D145), a rule of the row relations, not of the element.
		 * Prio (RW6): the prio element on the row of its Prio item (index): a straight run of tiles from first to last (the joiner merge's column, which it ends on), even_prio steps long. It is the last element of the leg's chain, after the end tile, and no track of the chain: the walks along the chain stop before it.
		 */
		enum class K : uint8_t { Tile, Corner, SBend, Heads, Fan, HeadsPair, Prio } k = K::Tile;
		int first = 0;
		int last = 0;
		int tok = -1;
		int hin = 0;
		int hout = 0;
		int hand = 0;
		bool joiner_div = false; ///< A Corner from a band split: its size comes from the rows (F4).
		bool free_m = false; ///< A vertical S-bend: its size comes from the columns.
		bool fan = false; ///< A band edge's fan at an S branch port (D230): a vertical S-bend sized from the columns that may be empty (m 0, no tiles).
		int m = 1;
		int cross = -1; ///< Heads: the crossed edge.
		uint32_t index = NO_ROW; ///< The stack item the token ends on.
		int slot = -1; ///< Row edges: the port slot of a port Tile or a Fan.
		bool port_first = false; ///< A Fan: it starts on the port row (an in edge's), else it ends on it.
		std::vector<TileIndexDiffC> offs;
	};
	std::vector<std::vector<Elem>> chain(edge_count);
	std::vector<std::vector<Elem>> chain_copy(edge_count); ///< Copy 1 of a doubled row leg: SBend, then its heads.
	/* A doubled leg's elements by their tokens (R9, R11): the k-th bridge's heads of a chain, and its S-bend of a lane role. */
	auto nth_heads = [](const std::vector<Elem> &elems, size_t k) -> const Elem & {
		for (const Elem &el : elems) {
			if (el.k == Elem::K::HeadsPair && k-- == 0) return el;
		}
		return elems.front();
	};
	auto lane_bend = [&](uint32_t e, const std::vector<Elem> &elems, Step::Role role) -> const Elem & {
		const std::vector<Step> &program = &elems == &chain[e] ? programs.edges[e] : programs.copies[e];
		for (const Elem &el : elems) {
			if (el.k == Elem::K::SBend && program[el.tok].role == role) return el;
		}
		return elems.front();
	};
	for (uint32_t e = 0; e < edge_count; e++) {
		if (is_row_edge(e)) continue;
		const JunctionTrackEdge &edge = graph.edges[e];
		const std::vector<Step> &program = programs.edges[e];
		const int start = edge.from.node ? node_pt[edge.from.index] : port_pt[edge.from.index];
		const int stop = edge.to.node ? node_pt[edge.to.index] : port_pt[edge.to.index];
		/* A band edge carries exit paths or joiner paths, never both; its start heading comes from the programs' edge ends (a row's heading at a row node, else south for exit paths from the main line and north for joiner paths from the S side). */
		for (uint16_t p : edge.paths) {
			if (group_of(paths[p].from).main != group_of(paths[edge.paths[0]].from).main) return fail("a band edge carrying exit and joiner paths is not ruled yet");
		}
		int cur = programs.ends[e].first.heading;
		const bool from_band_split = edge.from.node && order.node_level[edge.from.index] % 2 == 0 && graph.nodes[edge.from.index].split;
		std::vector<Elem> &elems = chain[e];
		Elem begin;
		begin.first = begin.last = start;
		begin.hin = begin.hout = cur;
		elems.push_back(begin);
		for (size_t t = 0; t < program.size(); t++) {
			const Step &step = program[t];
			Elem el;
			el.tok = static_cast<int>(t);
			el.hin = cur;
			if (step.kind == StepKind::Corner || step.kind == StepKind::SBend || step.kind == StepKind::Fan) {
				el.k = step.kind == StepKind::Corner ? Elem::K::Corner : Elem::K::SBend;
				el.fan = step.kind == StepKind::Fan;
				if (el.fan) el.m = 0;
				el.hand = step.hand;
				el.index = step.index;
				el.hout = step.kind == StepKind::Corner ? (cur + step.hand + 4) % 4 : cur;
				/* A bend is pinned to the node it serves when the programs' edge end names its token, never by its position (DESIGN-G2 §G2.6). */
				el.first = edge.from.node && programs.ends[e].first.token == static_cast<uint32_t>(t) ? start : new_pt();
				el.last = edge.to.node && programs.ends[e].second.token == static_cast<uint32_t>(t) ? stop : new_pt();
				el.joiner_div = step.kind == StepKind::Corner && t == 0 && from_band_split;
				el.free_m = step.kind != StepKind::Corner && (cur == 0 || cur == 2);
				cur = el.hout;
			} else if (step.kind == StepKind::Over || step.kind == StepKind::Tunnel) {
				/* A tunnel's portals are the heads of a single bridge (D220). */
				const auto bridge_it = std::ranges::find_if(bridges_of[e], [&](int b) { return std::ranges::find(structure.bridges[b].crossed, step.index) != structure.bridges[b].crossed.end(); });
				if (bridge_it == bridges_of[e].end() || structure.bridges[*bridge_it].doubled || structure.bridges[*bridge_it].crossed.size() != 1) return fail("a bridge over several band crossings is not ruled yet");
				el.k = Elem::K::Heads;
				el.first = new_pt();
				el.last = new_pt();
				el.hout = cur;
				el.cross = static_cast<int>(step.index);
			} else {
				continue;
			}
			elems.push_back(el);
		}
		Elem finish;
		finish.first = finish.last = stop;
		finish.hin = finish.hout = cur;
		elems.push_back(finish);
	}

	/* The solutions. */
	struct Cons {
		int v;
		int u;
		int k;
		uint32_t reason; ///< Index into 'reasons'.
		bool record_only = false; ///< Recorded in the system but never solved (the other alternative of a bridge over an S-bend).
	};
	std::vector<Cons> cons;
	std::vector<JunctionPlacementReason> reasons;
	/* A terrain fact of the X phase: a domain of columns for a point, or a run from point v to point u that must not cover both the column columns[0] and the column columns[1]. */
	struct FactCons {
		int v;
		int u; ///< -1 for a domain.
		std::vector<int> columns;
		uint32_t reason; ///< Index into 'reasons'.
	};
	std::vector<FactCons> fact_cons;
	std::map<uint32_t, std::string> notes; ///< Per reason of a fact, the tiles it names, for the error text.
	std::vector<std::array<int, 6>> group_tiles; ///< Per lazy note of kind 3: the row and the first (up to four) blocked tiles' columns of a group, count first: (count, y, x0..x3).
	std::vector<std::array<int, 3>> lazy_notes; ///< Per reason index, (kind, x, y) of a note formatted only when a failure reports it: 1 a blocked tile, 2 a height step.
	JunctionPlacementReason why_now; ///< The reason of the relations added now (DESIGN-G1 §14.4).
	auto why = [&](const char *rule, uint32_t edge = UINT32_MAX, uint32_t node = UINT32_MAX, int row = -1) { why_now = JunctionPlacementReason{rule, edge, node, static_cast<int32_t>(row)}; };
	int phase = 0; // 0: X, 1: Y.
	/** What the constant of the relations added now depends on (DESIGN-G3 §G3.3): c = a * m + b of the bend, or a site that depends on the sizes non-linearly or by a zero-size branch. */
	struct SystemTag {
		const Elem *bend = nullptr;
		int a = 0;
		int b = 0;
		bool nonlinear = false;
		bool zero_branch = false;
		bool order_pin = false; ///< The hug's equalities: the joint model drops them.
		int over = -1, d = 0; ///< When over >= 0: c also has + d * (t(over) - t at the recording) (DESIGN-G3 §G3.4a).
		int alt = -1; ///< When >= 0: the relation holds only when its over entry's alternative is alt (DESIGN-G3 §G3.4b).
	};
	SystemTag tag; ///< The tag of the relations added now.
	bool tag_negated = false; ///< The second relation of an equality: its constant is the negation.
	bool record_only_now = false; ///< The relations added now are recorded but never solved.
	std::vector<SystemTag> cons_tag; ///< Per relation of 'cons', its tag.
	/* Relations pushed to 'cons' without ge() carry a tag of their own, given here. */
	auto pad_tags = [&](bool nonlinear) {
		while (cons_tag.size() < cons.size()) cons_tag.push_back(SystemTag{nullptr, 0, 0, nonlinear, false});
	};
	auto ge = [&](int axis, int v, int u, int c) {
		if (axis != phase) return;
		assert(why_now.rule != nullptr);
		if (reasons.empty() || reasons.back().rule != why_now.rule || reasons.back().edge != why_now.edge || reasons.back().node != why_now.node || reasons.back().row != why_now.row) reasons.push_back(why_now);
		pad_tags(false);
		cons.push_back({v, u, c, static_cast<uint32_t>(reasons.size() - 1), record_only_now});
		SystemTag t = tag;
		if (tag_negated) {
			t.a = -t.a;
			t.b = -t.b;
			t.d = -t.d;
		}
		cons_tag.push_back(t);
	};
	auto eq = [&](int axis, int v, int u, int c) {
		ge(axis, v, u, c);
		tag_negated = !tag_negated;
		ge(axis, u, v, -c);
		tag_negated = !tag_negated;
	};
	std::vector<int> vx, vy;
	std::string build_why;
	bool rows_known = false; // False in the first pass, which only gives the S-bends' first sizes.
	/** A doubled bridge's crosser tiles on its two rows, as (point, column offset) in the copy-0 heads' frame: the heads hug the outermost by the flat solve's order (D217). */
	struct HugCandidate {
		int west;
		int east;
		uint32_t edge;
		int row;
		std::vector<std::pair<int, int>> tiles;
	};
	std::vector<HugCandidate> hugs;
	/* Modes of a build (D217): the flat state has no floors, no clearance and, in X, only the relations; clearance relations of the Y phase come from the flat state; an illegal occupancy group is a placement failure. */
	bool floors_off = false, relations_only = false, skip_clearance = false, occupancy_failed = false;
	/** One bridge's middle line: a Y-axis bridge's column variable and its two heads' points, or an X-axis bridge's row variable and its heads' columns. */
	struct BridgeLine {
		int bridge;
		uint32_t edge;
		bool x_axis;
		int a, b; ///< The heads' points.
		int line; ///< The point whose column (Y-axis) or row (X-axis) is the line.
	};
	struct HeadRef {
		int bridge;
		uint32_t edge;
		int xpt, ypt; ///< The points whose column and row are the head's.
	};
	struct ClearanceY {
		int v, u;
		uint32_t edge_b, edge_h;
	};
	std::vector<ClearanceY> clear_y;
	std::vector<JunctionPlacementReason> failure; ///< The explanation chain of the last failed solve.
	std::vector<int> fan_m(edge_count, 0);
	std::vector<int> port_row(n, 0);
	int port_offset = 0;
	/** A row change of kinds 1-4 (DESIGN-G1 §14.5, D215): the point its bend starts at, whether its feature lies toward larger columns, and for a fan the edge whose fan size must be nonzero. */
	struct RowChange {
		int anchor;
		int feature; ///< The point of the feature it serves.
		bool larger;
		uint32_t fan_edge;
		uint32_t edge;
		int row;
		int token; ///< The token of the edge's program (copy's program for copy 1) the change belongs to; with edge and copy it is the change's key (R2).
		int copy;
	};
	std::vector<RowChange> row_changes;
	/* The relation system of the last X and the last Y solve, for the caller that asked to record it (DESIGN-G3 §G3.3). */
	JunctionPlacementSystem recorded[2]; ///< Per axis (0 X, 1 Y), the system of its last solve.
	bool recorded_axis[2] = {false, false};
	std::vector<JunctionSystemRoute> recorded_routes; ///< The curve-gap routes of the X build now (DESIGN-G3 §G3.3a), taken by the X record.
	std::vector<std::array<int, 5>> recorded_route_curves; ///< Their curve forms (for tests).
	std::vector<JunctionSystemClash> recorded_clashes; ///< The occupancy clashes of the X build now (DESIGN-G3 §G3.3a, b-6), taken by the X record; the reason is set there.
	std::vector<JunctionSystemOver> recorded_overs; ///< The single bridges over an S-bend of the build now, in order (DESIGN-G3 §G3.4a); record_system sets the bend from over_elems.
	std::vector<const Elem *> over_elems; ///< Per recorded over, the crossed S-bend's element before the crossing (nullptr when none).
	std::vector<const Elem *> over_elems_after; ///< Per recorded over, the crossed S-bend's element after the crossing (nullptr when none).
	std::vector<uint32_t> recorded_clash_reasons; ///< Per clash, its index into 'reasons'.
	auto record_system = [&]() {
		pad_tags(false);
		JunctionPlacementSystem &now = recorded[phase];
		now = JunctionPlacementSystem{};
		now.pt_count = pt_count;
		now.k = k;
		now.rows_known = rows_known;
		now.item_pt = item_pt;
		now.port_row = port_row;
		now.port_offset = port_offset;
		now.rail_piece = spec.prices.rail_piece.base();
		now.signal = spec.prices.signal.base();
		now.bridge0 = spec.prices.bridge ? spec.prices.bridge(0).base() : 0;
		now.bridge1 = spec.prices.bridge ? spec.prices.bridge(1).base() : 0;
		now.sd = spec.sd;
		for (uint32_t e = 0; e < edge_count; e++) now.tokens.push_back(static_cast<uint32_t>(programs.edges[e].size()));
		if (phase == 0) {
			now.routes = recorded_routes;
			now.route_curves = recorded_route_curves;
			now.clashes = recorded_clashes;
		}
		for (size_t p = 0; p < paths.size(); p++) now.paths.emplace_back(graph.path_edges[p].begin(), graph.path_edges[p].end());
		std::map<const Elem *, int> bend_id;
		for (uint32_t e = 0; e < edge_count; e++) {
			for (int c = 0; c < 2; c++) {
				const std::vector<Elem> &elems = c == 0 ? chain[e] : chain_copy[e];
				for (const Elem &el : elems) {
					int id = -1;
					if (el.k == Elem::K::Corner || el.k == Elem::K::SBend || el.k == Elem::K::Fan) {
						id = static_cast<int>(bend_id.size());
						bend_id[&el] = id;
						const char *kind = el.k == Elem::K::Corner ? "Corner" : el.k == Elem::K::Fan ? "Fan" : (el.fan ? "SFan" : "SBend");
						const char *source = el.free_m ? "free" : el.k == Elem::K::Fan ? "port_rows" : el.k == Elem::K::Corner ? (el.joiner_div ? "corner_rows" : "fixed_k") : (el.index != NO_ROW ? "rows" : "one_row");
						const int floor = el.free_m ? (el.fan ? 0 : 1) : el.k == Elem::K::Corner ? k : (el.k == Elem::K::Fan ? 0 : (el.index != NO_ROW ? 0 : 1));
						const bool span_known = el.joiner_div && rows_known && static_cast<size_t>(std::max(el.first, el.last)) < vy.size();
						const int size = el.k == Elem::K::Corner ? (span_known ? std::abs(vy[el.first] - vy[el.last]) : k) : el.m;
						now.bends.push_back({id, static_cast<int>(e), c, el.tok, kind, source, el.hand, el.hin, el.first, el.last, el.index != NO_ROW ? item_pt[el.index] : -1, size, floor, el.k == Elem::K::Fan ? port_row[el.slot] : 0});
					}
					const char *kk = el.k == Elem::K::Tile ? "Tile" : el.k == Elem::K::Corner ? "Corner" : el.k == Elem::K::SBend ? "SBend" : el.k == Elem::K::Heads ? "Heads" : el.k == Elem::K::Fan ? "Fan" : el.k == Elem::K::HeadsPair ? "HeadsPair" : "Prio";
					const bool tunnel = !bridges_of[e].empty() && structure.bridges[bridges_of[e][0]].tunnel;
					now.chains.push_back({static_cast<int>(e), c, tunnel, kk, el.first, el.last, el.hin, el.hout, id});
				}
			}
		}
		now.overs = recorded_overs;
		for (size_t i = 0; i < now.overs.size(); i++) {
			now.overs[i].bend = over_elems[i] != nullptr ? bend_id.at(over_elems[i]) : -1;
			now.overs[i].bend_after = over_elems_after[i] != nullptr ? bend_id.at(over_elems_after[i]) : -1;
		}
		std::vector<std::string> reason_text(reasons.size());
		for (size_t i = 0; i < reasons.size(); i++) {
			const JunctionPlacementReason &reason = reasons[i];
			std::string parts;
			if (reason.edge != UINT32_MAX) parts += fmt::format("edge {}", reason.edge);
			if (reason.node != UINT32_MAX) parts += fmt::format("{}node {}", parts.empty() ? "" : ", ", reason.node);
			if (reason.row >= 0) parts += fmt::format("{}row {}", parts.empty() ? "" : ", ", reason.row);
			reason_text[i] = fmt::format("{}{}", reason.rule != nullptr ? reason.rule : "?", parts.empty() ? "" : " (" + parts + ")");
		}
		for (size_t i = 0; i < now.clashes.size(); i++) {
			JunctionSystemClash &clash = now.clashes[i];
			clash.reason = reason_text[recorded_clash_reasons[i]];
			for (const JunctionSystemColumn *column : {&clash.a_right, &clash.b_left}) {
				if (column->c1 == 0) continue;
				const std::string &source = now.bends[column->bend].source;
				clash.held |= source != "free" && source != "corner_rows" && source != "rows";
			}
		}
		for (size_t i = 0; i < cons.size(); i++) {
			const SystemTag &t = cons_tag[i];
			const int bid = t.bend != nullptr && bend_id.count(t.bend) ? bend_id[t.bend] : -1;
			now.relations.push_back({static_cast<uint8_t>(phase), cons[i].v, cons[i].u, cons[i].k, bid, t.a, t.b, t.nonlinear, t.zero_branch, t.order_pin, reason_text[cons[i].reason]});
			now.relations.back().over = t.over;
			now.relations.back().d = t.d;
			now.relations.back().alt = t.alt;
		}
		recorded_axis[phase] = true;

		/* The record: the chains, paths and prices of the X system (the Y system when X never solved), the bends of the X system once its rows were known (else the Y system), and both axes' relations. */
		const JunctionPlacementSystem &any = recorded_axis[0] ? recorded[0] : recorded[1];
		const JunctionPlacementSystem &bends_from = recorded_axis[0] && recorded[0].rows_known ? recorded[0] : (recorded_axis[1] ? recorded[1] : recorded[0]);
		*system = any;
		system->bends = bends_from.bends;
		system->rows_known = bends_from.rows_known;
		system->failed = at_failed;
		system->relations.clear();
		for (int axis = 0; axis < 2; axis++) {
			if (recorded_axis[axis]) system->relations.insert(system->relations.end(), recorded[axis].relations.begin(), recorded[axis].relations.end());
		}
	};
	auto solve = [&](std::vector<int> &val) {
		if (system != nullptr) record_system();
		/* A least solution is a longest path, so no point lies beyond the sum of the positive constants; a larger lo is a positive cycle. */
		int bound = 1;
		for (const Cons &cn : cons) bound += cn.record_only ? 0 : std::max(cn.k, 0);
		/* Facts can push points across the whole snapshot (DESIGN-G1 §14.4). */
		if (facts != nullptr && phase == 0) bound = std::max(bound, facts->MaxX() - offset.x + 1);
		JunctionPositions positions(pt_count, 0, bound);
		reasons.push_back(JunctionPlacementReason{"origin"});
		bool ok = positions.RestrictRange(0, 0, 0, static_cast<uint32_t>(reasons.size() - 1));
		for (const Cons &cn : cons) {
			if (!ok) break;
			if (cn.record_only) continue;
			ok = positions.AddDifference(cn.v, cn.u, cn.k, cn.reason);
		}
		for (const FactCons &fc : fact_cons) {
			if (!ok) break;
			ok = fc.u < 0 ? positions.RestrictDomain(fc.v, fc.columns, fc.reason) : positions.AddAvoid(fc.v, fc.u, fc.columns[0], fc.columns[1], fc.reason);
		}
		if (ok && phase == 0 && rows_known) {
			/* The D215 objective (DESIGN-G1 §14.5): with every other point at its least column, each row change whose feature lies toward larger columns moves as far as the relations and facts allow. The tokens' rigid groups are the points tied by exact relations. */
			std::map<std::pair<int, int>, int> most;
			for (const Cons &cn : cons) {
				if (cn.record_only) continue;
				const auto [found, added] = most.emplace(std::make_pair(cn.v, cn.u), cn.k);
				if (!added) found->second = std::max(found->second, cn.k);
			}
			std::vector<int> group(pt_count);
			std::iota(group.begin(), group.end(), 0);
			auto root = [&](int p) {
				while (group[p] != p) p = group[p] = group[group[p]];
				return p;
			};
			for (const auto &[pair, k] : most) {
				const auto back = most.find({pair.second, pair.first});
				if (back != most.end() && back->second == -k) group[root(pair.first)] = root(pair.second);
			}
			/* A group may not hold a port, the origin or the box's edges; it may hold a node only when that node is the feature being pushed. */
			std::vector<bool> fixed_point(pt_count, false), node_point(pt_count, false);
			fixed_point[0] = fixed_point[xe_pt] = fixed_point[se_pt] = true;
			for (const int p : port_pt) fixed_point[p] = true;
			for (const int p : node_pt) node_point[p] = true;
			/* First the tokens whose feature lies toward larger columns move to it; then, for the others, the feature's group moves toward the token (an eastbound out fan stays at its port and its joiner merge comes to it). */
			/* A token held back by another still short of its own feature goes on in the next round; columns only grow, so the rounds end. */
			for (bool pushed = true; pushed;) {
			pushed = false;
			for (const bool push_feature : {false, true}) {
				for (const RowChange &change : row_changes) {
					if (change.larger == push_feature || (change.fan_edge != UINT32_MAX && fan_m[change.fan_edge] == 0)) continue;
					const int moved = push_feature ? change.feature : change.anchor;
					const int moved_root = root(moved);
					bool tied_to_fixed = false;
					for (int p = 0; p < pt_count; p++) tied_to_fixed |= root(p) == moved_root && (fixed_point[p] || (node_point[p] && !(push_feature && p == change.feature)));
					if (tied_to_fixed) continue;
					const size_t mark = positions.Mark();
					bool held = true;
					for (int p = 0; p < pt_count && held; p++) {
						if (root(p) != moved_root) held = positions.RestrictRange(p, positions.Lo(p), positions.Lo(p));
					}
					const int up = held ? positions.Up(moved) : positions.Lo(moved);
					positions.Undo(mark);
					if (up == positions.Lo(moved)) continue;
					reasons.push_back(JunctionPlacementReason{"next to feature", change.edge, UINT32_MAX, change.row});
					const size_t before = positions.Mark();
					if (!positions.RestrictRange(moved, up, up, static_cast<uint32_t>(reasons.size() - 1))) positions.Undo(before); else pushed = true;
				}
			}
			}
		}
		if (!ok) {
			std::string chain_text;
			failure.clear();
			for (uint32_t index : positions.Explain()) {
				const JunctionPlacementReason &reason = reasons[index];
				failure.push_back(reason);
				std::string parts;
				if (reason.edge != UINT32_MAX) parts += fmt::format("edge {}", reason.edge);
				if (reason.node != UINT32_MAX) parts += fmt::format("{}node {}", parts.empty() ? "" : ", ", reason.node);
				if (reason.row >= 0) parts += fmt::format("{}row {}", parts.empty() ? "" : ", ", reason.row);
				const auto note = notes.find(index);
				std::string note_text = note == notes.end() ? std::string() : " " + note->second;
				if (note == notes.end() && index < lazy_notes.size() && lazy_notes[index][0] == 1) note_text = fmt::format(" [blocked tile ({},{})]", lazy_notes[index][1], lazy_notes[index][2]);
				if (note == notes.end() && index < lazy_notes.size() && lazy_notes[index][0] == 3) {
					const std::array<int, 6> &named = group_tiles[lazy_notes[index][1]];
					std::string tiles;
					for (int i = 0; i < named[0]; i++) tiles += fmt::format("{}({},{})", tiles.empty() ? "" : " ", named[2 + i], named[1]);
					note_text = " [blocked tile " + tiles + "]";
				}
				if (note == notes.end() && index < lazy_notes.size() && lazy_notes[index][0] == 2) note_text = fmt::format(" [height step ({},{}) to ({},{})]", lazy_notes[index][1], lazy_notes[index][2], lazy_notes[index][1] + 1, lazy_notes[index][2]);
				chain_text += fmt::format("{}{}{}{}", chain_text.empty() ? "" : " <- ", reason.rule, parts.empty() ? "" : " (" + parts + ")", note_text);
			}
			if (error != nullptr) *error = "the placement rules contradict each other: " + chain_text;
			return false;
		}
		val.resize(pt_count);
		for (int i = 0; i < pt_count; i++) val[i] = positions.Lo(i);
		return true;
	};
	auto solve_failed = [&]() {
		JunctionTrackPlacement result;
		result.why = failure;
		return result;
	};
	/* A relation along a heading: coord(next) - coord(prev) in the heading's sense >= d (= d when exact), no relation across. */
	auto along = [&](int heading, int next, int prev, int d, bool exact) {
		const int axis = VX[heading] != 0 ? 0 : 1;
		const int sign = VX[heading] + VY[heading];
		if (sign > 0) {
			if (exact) eq(axis, next, prev, d); else ge(axis, next, prev, d);
		} else {
			if (exact) eq(axis, prev, next, d); else ge(axis, prev, next, d);
		}
	};
	auto run = [&](int heading, int next, int prev, int d, bool exact) {
		along(heading, next, prev, d, exact);
		eq(VX[heading] != 0 ? 1 : 0, next, prev, 0);
	};

	/* The sizes of the bends that are read from a solution: the one place that reads vy, port_row and fan_m for them (DESIGN-G3 §G3.6, R1). */
	auto measure = [&](bool rows_known) {
		JunctionBendSizes sizes_now;
		for (uint32_t e = 0; e < edge_count; e++) {
			for (std::vector<Elem> *elems : {&chain[e], &chain_copy[e]}) {
				for (Elem &el : *elems) {
					if (el.k != Elem::K::Corner && el.k != Elem::K::SBend && el.k != Elem::K::Fan) continue;
					int m = el.m;
					int hand = el.hand;
					if (el.k == Elem::K::Fan) {
						/* The fan's size is the distance of the port row to its item row, 0 until the port rows are set (the rows are known then). */
						m = fan_m[e];
						if (m != 0) {
							const int y_item = vy[item_pt[el.index]];
							const int y_port = port_row[el.slot];
							hand = (((el.port_first ? y_item - y_port : y_port - y_item) > 0) == (el.hin == 1)) ? 1 : -1;
						}
					} else if (el.k == Elem::K::SBend) {
						/* A horizontal S-bend, of a row edge or a joiner's arrival, is sized by the rows it joins (rule of the lane change); known with the rows. */
						if (is_row_edge(e)) {
							m = rows_known ? std::abs(vy[item_pt[el.index]] - vy[el.first]) : 0;
						} else if (VX[el.hin] != 0 && el.index != NO_ROW) {
							m = rows_known ? std::abs(vy[item_pt[el.index]] - vy[el.first]) : 1; // A bend of a chain that is no row keeps one row until the rows are known.
						}
					}
					sizes_now.m.push_back(m);
					sizes_now.hand.push_back(static_cast<int8_t>(hand));
				}
			}
		}
		return sizes_now;
	};

	/* The size of every bend, applied from the sizes it is given. */
	auto refresh = [&](bool rows_known, const JunctionBendSizes &given) {
		size_t id = 0;
		for (uint32_t e = 0; e < edge_count; e++) {
			for (std::vector<Elem> *elems : {&chain[e], &chain_copy[e]}) {
				for (Elem &el : *elems) {
					if (el.k != Elem::K::Corner && el.k != Elem::K::SBend && el.k != Elem::K::Fan) continue;
					const size_t bend = id++;
					if (el.k == Elem::K::Fan) {
						el.m = given.m[bend];
						el.offs.clear();
						if (el.m != 0) {
							el.hand = given.hand[bend];
							el.offs = bend_tiles(el.hin, el.hand, 2 * el.m, false);
						}
					} else if (el.k == Elem::K::Corner) {
						const int kk = el.joiner_div && rows_known ? std::abs(vy[el.first] - vy[el.last]) : k;
						el.offs = bend_tiles(el.hin, el.hand, 2 * kk + 1, true);
					} else {
						if (is_row_edge(e) || (VX[el.hin] != 0 && el.index != NO_ROW)) el.m = given.m[bend];
						el.offs.clear();
						if (el.m != 0) el.offs = bend_tiles(el.hin, el.hand, 2 * el.m, false);
					}
				}
			}
		}
	};

	/* The notation rows of the W and E sides. */
	std::vector<int> notation(n, -1);
	for (const JunctionSide side : {JunctionSide::W, JunctionSide::E}) {
		std::vector<int> side_slots;
		for (int s = 0; s < n; s++) {
			if (group_of(s).side == side) side_slots.push_back(s);
		}
		if (side == JunctionSide::E) std::reverse(side_slots.begin(), side_slots.end());
		int row = 0;
		for (size_t i = 0; i < side_slots.size(); i++) {
			if (i > 0 && slots[side_slots[i]].group != slots[side_slots[i - 1]].group) row++;
			notation[side_slots[i]] = row++;
		}
	}
	/* The stack item of an edge's fan: the first Fan token of an in edge's program, the last of an out edge's (R3). */
	auto fan_item = [&](uint32_t e) -> uint32_t {
		if (!IsInEdge(e) && !IsOutEdge(e)) return NO_ROW;
		return programs.edges[e][FanTokenOf(programs, e, IsInEdge(e))].index;
	};
	auto edge_port = [&](uint32_t e) { return graph.edges[e].from.node ? graph.edges[e].to.index : graph.edges[e].from.index; };

	/* The row edges' chains, one builder for every row edge (RW4a): a chain is its start tile, one element per token of its program in order (a Fan token a Fan, an S-bend token an S-bend, an Over token the heads of that bridge), its end tile. A doubled leg's second chain is built from copy 1's tokens the same way. Every element takes its points from new_pt() as it is built, in chain order. */
	for (uint32_t e = 0; e < edge_count; e++) {
		if (!is_row_edge(e)) continue;
		const JunctionTrackEdge &edge = graph.edges[e];
		const int r = (end_level(edge.from) - 1) / 2;
		const int heading = east[r] ? 1 : 3;
		auto tile_elem = [&](int pt, int slot) {
			Elem el;
			el.first = el.last = pt;
			el.hin = el.hout = heading;
			el.slot = slot;
			return el;
		};
		const bool doubled = IsDoubledEdge(e);
		if (doubled) {
			/* R11 reads the lane split and merge S-bends by their role, so a doubled leg without them is still refused. */
			const auto has_role = [](const std::vector<Step> &tokens, Step::Role role) {
				return std::ranges::any_of(tokens, [&](const Step &token) { return token.kind == StepKind::SBend && token.role == role; });
			};
			if (!has_role(programs.edges[e], Step::Role::LaneMerge) || !has_role(programs.copies[e], Step::Role::LaneSplit)) return fail("a doubled leg without its lane split and merge is not ruled yet");
		}
		for (int c = 0; c < (doubled ? 2 : 1); c++) {
			const std::vector<Step> &program = c == 0 ? programs.edges[e] : programs.copies[e];
			std::vector<Elem> &elems = c == 0 ? chain[e] : chain_copy[e];
			elems.push_back(edge.from.node ? tile_elem(node_pt[edge.from.index], -1) : tile_elem(port_pt[edge.from.index], edge.from.index));
			bool fan_built = false;
			uint32_t row_item = NO_ROW; ///< The stack item the line is on after the elements so far; NO_ROW before the first fan or S-bend.
			for (size_t t = 0; t < program.size(); t++) {
				const Step &step = program[t];
				Elem el;
				el.hin = el.hout = heading;
				el.tok = static_cast<int>(t);
				el.index = step.index;
				if (step.kind == StepKind::Fan && !fan_built) {
					/* The fan is at the end of the edge that is a port: first if the edge starts at the port, else last. */
					fan_built = true;
					el.k = Elem::K::Fan;
					el.port_first = !edge.from.node;
					el.slot = el.port_first ? edge.from.index : edge.to.index;
					el.first = new_pt();
					el.last = new_pt();
					row_item = step.index;
				} else if (step.kind == StepKind::SBend) {
					el.k = Elem::K::SBend;
					el.hand = step.hand;
					el.first = new_pt();
					el.last = new_pt();
					row_item = step.index;
				} else if (step.kind == StepKind::Over && doubled) {
					/* A bridge crosses one or several edges, one Over token each; its heads are one element, at the token of its first crossed edge, and its other Over tokens add none. */
					const auto bridge_it = std::ranges::find_if(bridges_of[e], [&](int b) { return structure.bridges[b].crossed[0] == step.index; });
					if (bridge_it == bridges_of[e].end()) {
						if (std::ranges::any_of(bridges_of[e], [&](int b) { return std::ranges::find(structure.bridges[b].crossed, step.index) != structure.bridges[b].crossed.end(); })) continue;
						return fail("an Over token of a row edge without a bridge is not ruled yet");
					}
					el.k = Elem::K::HeadsPair;
					el.first = new_pt();
					el.last = new_pt();
					/* The row the heads lie on: the item of the last fan or S-bend before them, or, with none before, the row the start node is on, which no token of this chain names (the item the in edge ends on: RW3's in edge of the row). */
					el.index = row_item != NO_ROW ? row_item : programs.nodes[graph.edges[in_edge[r]].to.index].item;
				} else {
					return fail("a token of a row edge other than its fan, S-bends and bridges is not ruled yet");
				}
				elems.push_back(el);
			}
			elems.push_back(edge.to.node ? tile_elem(node_pt[edge.to.index], -1) : tile_elem(port_pt[edge.to.index], edge.to.index));
			/* The leg ends at the joiner merge, upstream of which the prio element lies (D75, D87: on the Prio item's row, opposite the joiner). It is the leg's, not the out edge's: its tiles precede the merge along the heading. */
			if (c == 0 && leg_edge[r] == e) {
				for (int i = 0; i < items; i++) {
					const JunctionTrackStackItem &item = structure.stack[i];
					if (item.row != r || (item.kind != JunctionTrackStackItem::Kind::Prio && item.kind != JunctionTrackStackItem::Kind::PrioAndCopy)) continue;
					Elem prio_el;
					prio_el.k = Elem::K::Prio;
					prio_el.hin = prio_el.hout = heading;
					prio_el.index = static_cast<uint32_t>(i);
					prio_el.first = new_pt();
					prio_el.last = new_pt();
					elems.push_back(prio_el);
				}
			}
		}
	}

	/* The row changes of kinds 1-4 (briefs/P3b-every-row-change-leans.md, "The rule") in the objective's order: edges ascending, an edge's tokens ascending, its copies after them. Each leans toward the feature it serves; only one whose feature lies toward larger columns can move. */
	for (uint32_t e = 0; e < edge_count; e++) {
		if (is_row_edge(e)) {
			const int r = (end_level(graph.edges[e].from) - 1) / 2;
			const bool eastward = east[r];
			for (int c = 0; c < (IsDoubledEdge(e) ? 2 : 1); c++) {
				const std::vector<Elem> &elems = c == 0 ? chain[e] : chain_copy[e];
				const std::vector<Step> &program = c == 0 ? programs.edges[e] : programs.copies[e];
				for (size_t i = 0; i < elems.size(); i++) {
					const Elem &el = elems[i];
					if (el.k == Elem::K::Fan) {
						/* Kind 1: a fan at the edge's start leans toward the node it ends at, with the travel; at its end, toward the node it starts at, against it. */
						row_changes.push_back({el.first, node_pt[el.port_first ? graph.edges[e].to.index : graph.edges[e].from.index], el.port_first ? eastward : !eastward, e, e, r, el.tok, c});
					} else if (el.k == Elem::K::SBend && program[el.tok].role == Step::Role::LaneMerge) {
						row_changes.push_back({el.first, elems[i - 1].last, !eastward, UINT32_MAX, e, r, el.tok, c}); // Kind 3: the lane merge leans toward the element before it, against the travel.
					} else if (el.k == Elem::K::SBend && program[el.tok].role == Step::Role::LaneSplit) {
						row_changes.push_back({el.first, chain[e][1].first, eastward, UINT32_MAX, e, r, el.tok, c}); // Kind 2: the lane split leans toward the first element after the start of copy 0, with the travel.
					}
				}
			}
			continue;
		}
		if (!programs.copies[e].empty()) continue;
		const std::vector<Step> &program = programs.edges[e];
		for (const Elem &el : chain[e]) {
			if (el.k != Elem::K::SBend || VX[el.hin] == 0) continue;
			/* Kind 4: toward the node at the end of the edge when no emitting token follows, toward the one at its start when none precedes. */
			bool before = false, after = false;
			for (size_t t = 0; t < program.size(); t++) {
				if (program[t].kind == StepKind::Under || program[t].kind == StepKind::Above || static_cast<int>(t) == el.tok) continue;
				(static_cast<int>(t) < el.tok ? before : after) = true;
			}
			if (before == after) continue;
			const JunctionTrackEnd &end = before ? graph.edges[e].to : graph.edges[e].from;
			row_changes.push_back({el.first, end.node ? node_pt[end.index] : port_pt[end.index], VX[el.hin] * (before ? 1 : -1) > 0, UINT32_MAX, e, -1, el.tok, 0});
		}
	}


	/* The objective's order is the graph's, not the points' (R2): by (edge, copy, token). */
	std::ranges::stable_sort(row_changes, [](const RowChange &a, const RowChange &b) { return std::array<int, 3>{static_cast<int>(a.edge), a.copy, a.token} < std::array<int, 3>{static_cast<int>(b.edge), b.copy, b.token}; });

	/* Symbolic tiles of an edge: an anchor point, a column offset, a row; in travel order. */
	struct SymTile {
		int pt;
		int dx;
		int y;
		bool bend = false; ///< Made by a bend's raster offsets (a Fan or another bend), so its dx depends on the bend's length and rows.
	};
	auto edge_tiles_of = [&](const std::vector<Elem> &elems) {
		std::vector<SymTile> tiles;
		for (size_t i = 0; i < elems.size(); i++) {
			const Elem &el = elems[i];
			if (el.k == Elem::K::Tile) {
				tiles.push_back({el.first, 0, el.slot >= 0 ? port_row[el.slot] : vy[el.first]});
			} else if (el.k == Elem::K::HeadsPair) {
				tiles.push_back({el.first, 0, vy[el.first]});
				tiles.push_back({el.last, 0, vy[el.last]});
			} else if (el.k == Elem::K::Fan) {
				/* The fan starts on the port row (an in edge's) or on its item row (an out edge's). */
				const int y = el.port_first ? port_row[el.slot] : vy[item_pt[el.index]];
				for (const TileIndexDiffC &o : el.offs) tiles.push_back({el.first, o.x, y + o.y, true});
			} else if (el.k == Elem::K::Heads) {
				const int step = vy[el.last] > vy[el.first] ? 1 : -1;
				for (int y = vy[el.first]; y != vy[el.last] + step; y += step) tiles.push_back({el.first, 0, y});
			} else {
				for (const TileIndexDiffC &o : el.offs) tiles.push_back({el.first, o.x, vy[el.first] + o.y, true});
			}
			if (i + 1 < elems.size() && elems[i + 1].first != el.last && VX[el.hout] == 0) {
				const int d = VY[el.hout];
				for (int y = vy[el.last] + d; d > 0 ? y < vy[elems[i + 1].first] : y > vy[elems[i + 1].first]; y += d) tiles.push_back({el.last, 0, y});
			}
		}
		return tiles;
	};
	auto edge_tiles = [&](uint32_t e) { return edge_tiles_of(chain[e]); };

	/* The curves of every route (D217): per lane path and copy, in travel order, each curve as its bend's anchor point (the bend's first half tile), its hand and its element, and the route's units up to it in two forms. In the X form (rows known, one horizontal heading s per route) the units are xoff + 2 s (x[anchor] - x[the route's start]): along a horizontal stretch of that heading every column adds 2 units, a vertical stretch adds the fixed units of its rows, and a bend adds what its own pieces do. In the Y form (a segment of curves with only vertical runs between them, vertical heading sv) the units from the segment's first curve are yoff + 2 sv (y[anchor] - y[first anchor]). Row bends (fans, lane S-bends) lie between horizontal runs, so they have no segment (seg -1) and their curves pair in the X form only. */
	struct SymCurve {
		int pt;
		int xoff;
		int yoff;
		int seg;
		JunctionCurveHand hand;
		uint32_t edge;
		int row;
	};
	struct SymRoute {
		size_t path = 0;
		int copy = 0;
		int start = 0; ///< The route's start point.
		int sign = 1; ///< s.
		std::vector<int> seg_sign; ///< Per segment, sv.
		std::vector<SymCurve> curves;
		JunctionSystemRoute rec; ///< The route as the curve-gap counters read it (X form builds of a recording call only).
		std::vector<const Elem *> rec_elems; ///< Per bend of 'rec', the element it is.
	};
	auto doubled_edge = [&](uint32_t e) {
		return std::ranges::any_of(bridges_of[e], [&](int b) { return structure.bridges[b].doubled; });
	};
	/* With numeric_y false (the Y phase) the rows are unknown: the X form is not made and the row bends are left out. */
	std::map<std::tuple<int, int, int, bool>, JunctionRaster> bend_rasters; ///< The rasters of the bends the walk meets, by heading, hand, half pieces and corner.
	auto sym_routes = [&](std::vector<SymRoute> &routes, bool numeric_y) {
		/* A bend of the route: the anchor point and row of its first half tile, the heading it starts on, its hand, its half pieces. */
		struct Bend {
			int pt, y, heading, hand, halves;
			bool corner;
			uint32_t edge;
			int row;
			bool on_row;
			const Elem *el;
		};
		routes.clear();
		for (size_t p = 0; p < paths.size(); p++) {
			const std::vector<uint32_t> &edges = graph.path_edges[p];
			const JunctionSide side = group_of(paths[p].from).side;
			if (side == JunctionSide::N) continue; // Not ruled yet (RenderJunctionTrackPlacement fails on it).
			const int start_heading = side == JunctionSide::W ? 1 : side == JunctionSide::E ? 3 : 0;
			const int start_y = !numeric_y ? 0 : (side == JunctionSide::S ? vy[port_pt[paths[p].from]] : port_row[paths[p].from]);
			const bool has_doubled = std::ranges::any_of(edges, doubled_edge);
			for (int c = 0; c < (has_doubled ? 2 : 1); c++) {
				std::vector<Bend> bends;
				std::vector<int> row_heading; ///< Per bend, the heading of the row edge crossed just before it, or -1.
				int pending_row = -1;
				for (uint32_t e : edges) {
					const std::vector<Step> &tokens = c == 1 && doubled_edge(e) ? programs.copies[e] : programs.edges[e];
					if (is_row_edge(e)) {
						const int r = (end_level(graph.edges[e].from) - 1) / 2;
						const int heading = east[r] ? 1 : 3;
						pending_row = heading;
						if (!numeric_y) continue;
						row_heading.resize(bends.size(), -1);
						/* Every bend token of the edge's chain, in travel order (R4, R5): the fan, an S-bend, a doubled leg's lane split and merge (copy 1's are in its own chain); a bridge's heads are no bend. */
						for (size_t t = 0; t < tokens.size(); t++) {
							for (const Elem &el : c == 1 && doubled_edge(e) ? chain_copy[e] : chain[e]) {
								if (el.tok != static_cast<int>(t) || (el.k != Elem::K::Fan && el.k != Elem::K::SBend)) continue;
								row_heading.resize(bends.size(), -1);
								if (el.k == Elem::K::Fan) {
									if (el.m == 0) continue;
									const int y_fan = el.port_first ? port_row[el.slot] : vy[item_pt[el.index]];
									bends.push_back({el.first, y_fan, heading, el.hand, 2 * el.m, false, e, r, true, &el});
								} else {
									if (el.m == 0) continue; // An S-bend sized to 0 columns is no bend.
									bends.push_back({el.first, vy[el.first], heading, el.hand, 2 * el.m, false, e, r, true, &el});
								}
							}
						}
						continue;
					}
					for (size_t t = 0; t < tokens.size(); t++) {
						if (tokens[t].kind != StepKind::Corner && tokens[t].kind != StepKind::SBend && tokens[t].kind != StepKind::Fan) continue;
						for (const Elem &el : chain[e]) {
							if (el.tok != static_cast<int>(t) || (el.fan && el.m == 0) || el.offs.empty()) continue;
							row_heading.resize(bends.size(), -1);
							row_heading.push_back(pending_row);
							pending_row = -1;
							bends.push_back({el.first, numeric_y ? vy[el.first] : 0, el.hin, el.hand, static_cast<int>(el.offs.size()), el.k == Elem::K::Corner, e, -1, false, &el});
						}
					}
				}
				row_heading.resize(bends.size(), -1);

				/* Each bend's pieces: its curves with the units before them, the offsets of the axis piece before it and the closing axis piece from the first half tile, and the heading it leaves on. */
				struct Pieces {
					std::vector<std::pair<int, JunctionCurveHand>> curves; ///< Units from the start of the axis piece before the bend, hand.
					int before_dx, before_dy, close_dx, close_dy, close_units, heading;
				};
				std::vector<Pieces> pieces;
				/* The pieces of a bend of this many half pieces, from its raster. */
				auto read_pieces = [&](const Bend &b, int halves, Pieces &q) {
					auto cached = bend_rasters.find({b.heading, b.hand, halves, b.corner});
					if (cached == bend_rasters.end()) cached = bend_rasters.emplace(std::make_tuple(b.heading, b.hand, halves, b.corner), RasterizeJunctionRoute({{0, 0}, JUNCTION_AXIS_TRACKDIR[b.heading], JunctionBendMoves(b.hand, halves, b.corner)})).first;
					const JunctionRaster &raster = cached->second;
					if (!raster.error.empty() || static_cast<int>(raster.pieces.size()) != halves + 2) { build_why = "a bend of edge " + str(b.edge) + " has no raster"; return false; }
					q = Pieces{{}, raster.pieces[0].tile.x - raster.pieces[1].tile.x, raster.pieces[0].tile.y - raster.pieces[1].tile.y, raster.pieces.back().tile.x - raster.pieces[1].tile.x, raster.pieces.back().tile.y - raster.pieces[1].tile.y, 0, -1};
					for (size_t i = 0; i < raster.pieces.size(); i++) {
						if (i > 0) {
							const DirDiff diff = DirDifference(TrackdirToDirection(raster.pieces[i - 1].trackdir), TrackdirToDirection(raster.pieces[i].trackdir));
							if (diff == DirDiff::Left45 || diff == DirDiff::Right45) q.curves.emplace_back(q.close_units, diff == DirDiff::Left45 ? JunctionCurveHand::Left : JunctionCurveHand::Right);
						}
						if (i + 1 < raster.pieces.size()) q.close_units += raster.pieces[i].units;
					}
					for (int i = 0; i < 4; i++) {
						if (JUNCTION_AXIS_TRACKDIR[i] == raster.pieces.back().trackdir) q.heading = i;
					}
					if (q.heading < 0) { build_why = "a bend of edge " + str(b.edge) + " does not end on an axis piece"; return false; }
					return true;
				};
				for (const Bend &b : bends) {
					Pieces q;
					if (!read_pieces(b, b.halves, q)) return false;
					pieces.push_back(std::move(q));
				}

				/* The one horizontal heading of the route up to its last bend (the X form's s). */
				SymRoute route;
				route.path = p;
				route.copy = c;
				route.start = port_pt[paths[p].from];
				route.sign = 0;
				auto note = [&](int heading) { return VX[heading] == 0 || (route.sign == 0 ? (route.sign = VX[heading]) != 0 : route.sign == VX[heading]); };
				bool one_heading = note(start_heading);
				for (size_t i = 0; i + 1 < pieces.size(); i++) one_heading = one_heading && note(pieces[i].heading);
				if (!one_heading) { build_why = "path " + str(static_cast<int>(p)) + " has two horizontal headings"; return false; }
				if (route.sign == 0) route.sign = 1;

				/* The segments: a bend on a row, or one that follows a horizontal run, starts one; a bend that follows a vertical run joins it. Its vertical heading sv is that of its first vertical run. */
				std::vector<int> seg_of(bends.size(), -1);
				{
					int h = start_heading, cur = -1;
					for (size_t i = 0; i < bends.size(); i++) {
						const int in_heading = row_heading[i] >= 0 ? row_heading[i] : h;
						if (bends[i].on_row) {
							cur = -1;
						} else {
							if (cur < 0 || VX[in_heading] != 0) {
								route.seg_sign.push_back(0);
								cur = static_cast<int>(route.seg_sign.size()) - 1;
							} else if (route.seg_sign[cur] == 0) {
								route.seg_sign[cur] = VY[in_heading];
							} else if (route.seg_sign[cur] != VY[in_heading]) {
								build_why = "a segment of path " + str(static_cast<int>(p)) + " has two vertical headings";
								return false;
							}
							seg_of[i] = cur;
						}
						h = pieces[i].heading;
						if (!bends[i].on_row && VX[h] != 0) cur = -1;
					}
				}

				/* w: the units at the route's current piece less 2 s times its columns from the start (X form). v: the same for the segment, less 2 sv times its rows (Y form, with the anchors' rows symbolic). */
				int w = 0, y = start_y, heading = start_heading, v = 0, last_seg = -2;
				for (size_t i = 0; i < bends.size(); i++) {
					const Bend &b = bends[i];
					const Pieces &q = pieces[i];
					const int in_heading = row_heading[i] >= 0 ? row_heading[i] : heading;
					if (numeric_y && VX[in_heading] == 0) {
						const int run = (b.y + q.before_dy - y) * VY[in_heading];
						if (run < 0) { build_why = "a vertical run before edge " + str(b.edge) + " is negative"; return false; }
						w += 2 * run;
					}
					if (seg_of[i] != last_seg) v = 0;
					last_seg = seg_of[i];
					const int ysign = seg_of[i] < 0 ? 0 : (route.seg_sign[seg_of[i]] != 0 ? route.seg_sign[seg_of[i]] : 1);
					for (const auto &[units, hand] : q.curves) route.curves.push_back({b.pt, numeric_y ? w + 2 * route.sign * q.before_dx + units : 0, v + 2 * ysign * q.before_dy + units, seg_of[i], hand, b.edge, b.row});
					w += 2 * route.sign * (q.before_dx - q.close_dx) + q.close_units;
					v += 2 * ysign * (q.before_dy - q.close_dy) + q.close_units;
					y = b.y + q.close_dy;
					heading = q.heading;
				}
				if (system != nullptr && numeric_y) {
					/* The counters' record: each bend's raster quantities as (q at L = 1, q at L = 2) when its halves are 2L (a joiner Corner's 2L + 1), else the constant of its one raster. */
					route.rec.start_pt = route.start;
					route.rec.start_heading = start_heading;
					for (size_t i = 0; i < bends.size(); i++) {
						const Bend &b = bends[i];
						const bool sized = !b.corner || b.el->joiner_div;
						Pieces q1 = pieces[i], q2 = pieces[i];
						if (sized && (!read_pieces(b, b.corner ? 3 : 2, q1) || !read_pieces(b, b.corner ? 5 : 4, q2))) return false;
						if (q1.curves.size() != q2.curves.size() || q1.heading != q2.heading) { build_why = "a bend of edge " + str(b.edge) + " is not affine in its length"; return false; }
						JunctionSystemRouteBend rb{b.pt, sized ? -2 : -1, b.heading, q1.heading, b.hand, b.corner, {{{q1.before_dx, q2.before_dx}, {q1.before_dy, q2.before_dy}, {q1.close_dx, q2.close_dx}, {q1.close_dy, q2.close_dy}, {q1.close_units, q2.close_units}}}, {}};
						for (size_t k = 0; k < q1.curves.size(); k++) {
							rb.curve_units.emplace_back(q1.curves[k].first, q2.curves[k].first);
							route.rec.hands.push_back(q1.curves[k].second == JunctionCurveHand::Right ? 1 : -1);
						}
						route.rec.bends.push_back(std::move(rb));
						route.rec_elems.push_back(b.el);
					}
				}
				routes.push_back(std::move(route));
			}
		}
		return true;
	};

	/**
	 * One piece of a row's chains in the walk of the occupancy and the facts (R5): a tile (a port's, a bridge head), a run of straight tiles strictly between two points, or a bend at a point. The row of a piece is a number (a port's row and a fan's ends are not points' rows).
	 * copy: 0 or 1 for a doubled leg's copies, -1 for the part both share.
	 */
	struct RowPiece {
		enum class T : uint8_t { Tile, Run, Bend } t;
		int a, b; ///< Tile, Bend: the anchor point a. Run: from a to b.
		int y;
		int copy;
		uint32_t edge;
		int hand, halves;
		bool port; ///< A port's tile (it has no fact: the port's column is fixed).
		bool fact; ///< A bridge head's tile (a straight tile fact).
		const Elem *bend = nullptr; ///< Bend: its chain element.
	};
	/* Row r's pieces in order: the in edge, the leg, the out edge, each read from its chain(s). */
	auto row_pieces = [&](int r) {
		std::vector<RowPiece> pieces;
		/* An edge's chains read in travel order, element by element: its tiles at the ports, its fan, S-bends and heads, the runs between its elements (an empty fan has no run to its port). The copies of a doubled leg share one lane before the lane split and after the lane merge (copy -1): the run from the start tile to the split is shared and copy 0's run begins at the split; the run after the merge is shared and copy 1's run ends at the merge. */
		auto walk = [&](uint32_t e) {
			const int n_chains = IsDoubledEdge(e) ? 2 : 1;
			const auto role_of = [&](int c, const Elem &el) { return el.k == Elem::K::SBend ? (c == 0 ? programs.edges[e] : programs.copies[e])[el.tok].role : Step::Role::None; };
			const auto find_role = [&](int c, Step::Role role) -> const Elem * {
				for (const Elem &el : c == 0 ? chain[e] : chain_copy[e]) {
					if (role_of(c, el) == role) return &el;
				}
				return nullptr;
			};
			const Elem *split = n_chains == 2 ? find_role(1, Step::Role::LaneSplit) : nullptr;
			const Elem *merge = n_chains == 2 ? find_role(0, Step::Role::LaneMerge) : nullptr;
			for (int c = 0; c < n_chains; c++) {
				const std::vector<Elem> &elems = c == 0 ? chain[e] : chain_copy[e];
				for (size_t i = 0; i < elems.size(); i++) {
					const Elem &el = elems[i];
					int y_end = vy[el.last];
					if (el.k == Elem::K::Tile) {
						if (el.slot >= 0) {
							y_end = port_row[el.slot];
							pieces.push_back({RowPiece::T::Tile, el.first, 0, y_end, 0, e, 0, 0, true, false});
						}
					} else if (el.k == Elem::K::Fan) {
						const int y_item = vy[item_pt[el.index]];
						const int y_port = port_row[el.slot];
						y_end = el.port_first ? y_item : y_port;
						if (el.m != 0) pieces.push_back({RowPiece::T::Bend, el.first, 0, el.port_first ? y_port : y_item, 0, e, el.hand, static_cast<int>(el.offs.size()), false, false, &el});
					} else if (el.k == Elem::K::SBend) {
						if (!el.offs.empty()) pieces.push_back({RowPiece::T::Bend, el.first, 0, vy[el.first], c, e, el.hand, static_cast<int>(el.offs.size()), false, false, &el});
					} else if (el.k == Elem::K::HeadsPair) {
						pieces.push_back({RowPiece::T::Tile, el.first, 0, vy[el.first], c, e, 0, 0, false, true});
						pieces.push_back({RowPiece::T::Tile, el.last, 0, vy[el.first], c, e, 0, 0, false, true});
					}
					if (i + 1 >= elems.size()) continue;
					const Elem &next = elems[i + 1];
					if (next.k == Elem::K::Prio) continue; // The prio element is no track of the chain (its tiles are read where the walk is used).
					int a = el.last, b = next.first, copy = c;
					if (split != nullptr && i == 0) {
						if (c == 0) a = split->first;
						else copy = -1;
					}
					if (merge != nullptr && c == 0 && el.k == Elem::K::SBend && &el == merge) copy = -1;
					if (merge != nullptr && c == 1 && i + 2 == elems.size()) b = merge->last;
					if (a == b) continue;
					if ((el.k == Elem::K::Tile && el.slot >= 0 && next.k == Elem::K::Fan && next.m == 0) || (el.k == Elem::K::Fan && el.m == 0 && next.k == Elem::K::Tile && next.slot >= 0)) continue;
					pieces.push_back({RowPiece::T::Run, a, b, y_end, copy, e, 0, 0, false, false});
				}
			}
		};
		walk(in_edge[r]);
		walk(leg_edge[r]);
		walk(out_edge[r]);
		return pieces;
	};

	/* The tracks the routes build on each node's tile, and the headings of the routes that go straight through it (JunctionRoutesHeightsOk reads the union per tile): the piece of a route on its node's tile is the half that leaves or joins there, or an axis piece. */
	std::vector<TrackBits> node_bits(graph.nodes.size());
	std::vector<std::vector<int>> node_straight(graph.nodes.size());
	auto compute_node_tracks = [&]() {
		std::ranges::fill(node_bits, TrackBits{});
		for (std::vector<int> &headings : node_straight) headings.clear();
		std::map<std::pair<const Elem *, bool>, TrackBits> half_cache;
		auto bend_half = [&](const Elem &el, bool last) {
			const auto cached = half_cache.find({&el, last});
			if (cached != half_cache.end()) return cached->second;
			const size_t halves = el.offs.size();
			if (halves == 0) return TrackBits{}; // Sized to 0 columns: no bend, no half.
			const JunctionRaster raster = RasterizeJunctionRoute({{0, 0}, JUNCTION_AXIS_TRACKDIR[el.hin], JunctionBendMoves(el.hand, static_cast<int>(halves), el.k == Elem::K::Corner)});
			const TrackBits half = !raster.error.empty() || raster.pieces.size() != halves + 2 ? TrackBits{} : TrackBits{TrackdirToTrack(raster.pieces[last ? halves : 1].trackdir)};
			half_cache[{&el, last}] = half;
			return half;
		};
		for (const std::vector<uint32_t> &edges : graph.path_edges) {
			for (size_t i = 0; i + 1 < edges.size(); i++) {
				if (!graph.edges[edges[i]].to.node) continue;
				const uint32_t node = graph.edges[edges[i]].to.index;
				const uint32_t e_in = edges[i];
				const uint32_t e_out = edges[i + 1];
				const Elem *leaving = nullptr;
				const Elem *joining = nullptr;
				if (!is_row_edge(e_out) && chain[e_out].size() > 2 && chain[e_out][1].first == chain[e_out][0].first && chain[e_out][1].k != Elem::K::Heads) leaving = &chain[e_out][1];
				if (!is_row_edge(e_in) && chain[e_in].size() > 2 && chain[e_in][chain[e_in].size() - 2].last == chain[e_in].back().first && chain[e_in][chain[e_in].size() - 2].k != Elem::K::Heads) joining = &chain[e_in][chain[e_in].size() - 2];
				if (leaving != nullptr) {
					node_bits[node] = node_bits[node] | bend_half(*leaving, false);
				} else if (joining != nullptr) {
					node_bits[node] = node_bits[node] | bend_half(*joining, true);
				} else {
					const int heading = is_row_edge(e_out) ? (east[(end_level(graph.edges[e_out].from) - 1) / 2] ? 1 : 3) : chain[e_out][0].hout;
					node_bits[node] = node_bits[node] | TrackBits{TrackdirToTrack(JUNCTION_AXIS_TRACKDIR[heading])};
					if (std::ranges::find(node_straight[node], heading) == node_straight[node].end()) node_straight[node].push_back(heading);
				}
			}
		}
	};

	/* The flat solve (D217): the least columns of the relations added so far, with no facts and no floors; false when they contradict each other. */
	auto flat_solve = [&](std::vector<int> &val) {
		int bound = 1;
		for (const Cons &cn : cons) bound += cn.record_only ? 0 : std::max(cn.k, 0);
		JunctionPositions positions(pt_count, 0, bound);
		bool ok = positions.RestrictRange(0, 0, 0);
		for (const Cons &cn : cons) {
			if (!ok) break;
			if (cn.record_only) continue;
			ok = positions.AddDifference(cn.v, cn.u, cn.k);
		}
		if (!ok) return false;
		val.resize(pt_count);
		for (int i = 0; i < pt_count; i++) val[i] = positions.Lo(i);
		return true;
	};

	/** The half pieces of a bend as track bits, one per half tile as bend_tiles lists them; empty when the bend has no raster. */
	auto bend_bits = [&](int heading, int hand, int halves, bool corner) {
		auto cached = bend_rasters.find({heading, hand, halves, corner});
		if (cached == bend_rasters.end()) cached = bend_rasters.emplace(std::make_tuple(heading, hand, halves, corner), RasterizeJunctionRoute({{0, 0}, JUNCTION_AXIS_TRACKDIR[heading], JunctionBendMoves(hand, halves, corner)})).first;
		const JunctionRaster &raster = cached->second;
		std::vector<TrackBits> bits;
		if (!raster.error.empty() || static_cast<int>(raster.pieces.size()) != halves + 2) return bits;
		for (int i = 1; i <= halves; i++) bits.push_back(TrackBits{TrackdirToTrack(raster.pieces[i].trackdir)});
		return bits;
	};

	/* The occupancy family (D217): unrelated tracks never share or touch a tile; two unrelated pieces on one tile are legal only as Upper|Lower or Left|Right (JunctionTracksMayShareTile). */
	auto add_occupancy = [&](const std::vector<int> &flat, bool legality) -> bool {
		/** One occupant of a row: the columns [left, right], each end a point and an offset, with the tracks of its tiles and what it belongs to. */
		struct Occupant {
			int row;
			int lp, ldx, rp, rdx;
			TrackBits bits;
			uint32_t edge; ///< The graph edge, UINT32_MAX for a node's tile.
			int copy; ///< 0 or 1, -1 for the part both copies share.
			uint32_t node; ///< The node of a node's tile, else UINT32_MAX.
			int footprint = 0; ///< A prio element's footprint: +1 when it lies downstream of its tile column (columns grow), -1 when downstream is lower; 0 for tracks.
			JunctionSystemColumn left, right; ///< The ends as columns of the joint model, filled when recording.
		};
		const bool record = system != nullptr && !legality;
		std::map<const Elem *, int> bend_ids; ///< The system's bend id of each chain bend (the order record_system gives them), when recording.
		if (record) {
			for (uint32_t e = 0; e < edge_count; e++) {
				for (const std::vector<Elem> *elems : {&chain[e], &chain_copy[e]}) {
					for (const Elem &el : *elems) {
						if (el.k == Elem::K::Corner || el.k == Elem::K::SBend || el.k == Elem::K::Fan) bend_ids.emplace(&el, static_cast<int>(bend_ids.size()));
					}
				}
			}
		}
		std::map<int, std::vector<Occupant>> by_row;
		auto flat_left = [&](const Occupant &o) { return flat[o.lp] + o.ldx; };
		auto flat_right = [&](const Occupant &o) { return flat[o.rp] + o.rdx; };
		auto add_tile = [&](int pt, int dx, int y, TrackBits bits, uint32_t edge, int copy, const JunctionSystemColumn *column = nullptr) {
			std::vector<Occupant> &row = by_row[y];
			if (!row.empty() && row.back().edge == edge && row.back().copy == copy && row.back().node == UINT32_MAX && row.back().lp == pt && row.back().ldx == dx && row.back().rp == pt && row.back().rdx == dx) {
				row.back().bits = row.back().bits | bits;
				return;
			}
			const JunctionSystemColumn at = column != nullptr ? *column : JunctionSystemColumn{pt, dx};
			row.push_back({y, pt, dx, pt, dx, bits, edge, copy, UINT32_MAX, 0, at, at});
		};
		/* The tiles strictly between points a and b on row y, a before b in the sense sgn; a run with none at the flat solve is an empty interval (right = left - 1), which keeps its place in the order for the solves with floors. */
		auto add_run = [&](int a, int b, int y, int sgn, uint32_t edge, int copy) {
			by_row[y].push_back({y, sgn > 0 ? a : b, 1, sgn > 0 ? b : a, -1, TrackBits{Track::X}, edge, copy, UINT32_MAX, 0, {sgn > 0 ? a : b, 1}, {sgn > 0 ? b : a, -1}});
		};
		auto add_bend = [&](int pt, int y, int heading, int hand, int halves, bool corner, uint32_t edge, int copy, bool skip_first, bool skip_last, const Elem *bend) {
			if (halves < 1) return; // A bend sized to 0 columns is no bend.
			const std::vector<TileIndexDiffC> offs = bend_tiles(heading, hand, halves, corner);
			const std::vector<TrackBits> bits = bend_bits(heading, hand, halves, corner);
			if (bits.size() != offs.size()) return;
			std::vector<JunctionBendTileForm> forms;
			int bend_id = -1;
			if (record) {
				const int length = corner ? (halves - 1) / 2 : halves / 2;
				assert((corner ? 2 * length + 1 : 2 * length) == halves);
				forms = JunctionBendTileForms(heading, hand, length, corner);
				assert(forms.size() == offs.size());
				assert(bend_ids.count(bend) != 0);
				bend_id = bend_ids.at(bend);
			}
			for (size_t j = 0; j < offs.size(); j++) {
				if ((skip_first && j == 0) || (skip_last && j + 1 == offs.size())) continue;
				if (record) {
					const JunctionSystemColumn column{pt, forms[j].dx0, forms[j].dx1, forms[j].dx1 != 0 ? bend_id : -1};
					add_tile(pt, offs[j].x, y + offs[j].y, bits[j], edge, copy, &column);
				} else {
					add_tile(pt, offs[j].x, y + offs[j].y, bits[j], edge, copy);
				}
			}
		};
		std::vector<uint32_t> node_of_pt(pt_count, UINT32_MAX);
		for (uint32_t i = 0; i < node_pt.size(); i++) node_of_pt[node_pt[i]] = i;
		for (uint32_t i = 0; i < node_pt.size(); i++) by_row[vy[node_pt[i]]].push_back({vy[node_pt[i]], node_pt[i], 0, node_pt[i], 0, node_bits[i], UINT32_MAX, 0, i, 0, {node_pt[i]}, {node_pt[i]}});

		/* The chains of the edges that are not rows. */
		for (uint32_t e = 0; e < edge_count; e++) {
			if (is_row_edge(e)) continue;
			const std::vector<Elem> &elems = chain[e];
			for (size_t i = 0; i < elems.size(); i++) {
				const Elem &el = elems[i];
				if (el.k == Elem::K::Tile) {
					if (node_of_pt[el.first] == UINT32_MAX) add_tile(el.first, 0, vy[el.first], VX[el.hin] != 0 ? TrackBits{Track::X} : TrackBits{Track::Y}, e, 0);
				} else if (el.k == Elem::K::Heads) {
					const TrackBits head = VX[el.hin] != 0 ? TrackBits{Track::X} : TrackBits{Track::Y};
					add_tile(el.first, 0, vy[el.first], head, e, 0);
					add_tile(el.last, 0, vy[el.last], head, e, 0);
				} else {
					add_bend(el.first, vy[el.first], el.hin, el.hand, static_cast<int>(el.offs.size()), el.k == Elem::K::Corner, e, 0, node_of_pt[el.first] != UINT32_MAX, node_of_pt[el.last] != UINT32_MAX, &el);
				}
				if (i + 1 >= elems.size() || elems[i + 1].first == el.last) continue;
				const Elem &next = elems[i + 1];
				if (VX[el.hout] == 0) {
					const int d = VY[el.hout];
					for (int y = vy[el.last] + d; d > 0 ? y < vy[next.first] : y > vy[next.first]; y += d) add_tile(el.last, 0, y, TrackBits{Track::Y}, e, 0);
				} else {
					add_run(el.last, next.first, vy[el.last], VX[el.hout], e, 0);
				}
			}
		}

		/* The rows' chains: in edge, leg (both copies of a doubled one), out edge. */
		for (int r = 0; r < rows; r++) {
			const int sgn = east[r] ? 1 : -1;
			const TrackBits axis{Track::X};
			for (const RowPiece &piece : row_pieces(r)) {
				switch (piece.t) {
					case RowPiece::T::Tile: add_tile(piece.a, 0, piece.y, axis, piece.edge, piece.copy); break;
					case RowPiece::T::Run: add_run(piece.a, piece.b, piece.y, sgn, piece.edge, piece.copy); break;
					case RowPiece::T::Bend: add_bend(piece.a, piece.y, east[r] ? 1 : 3, piece.hand, piece.halves, false, piece.edge, piece.copy, false, false, piece.bend); break;
				}
			}
		}

		/* The prio element's footprint (DESIGN-G1 12.4): on the Prio item's row, opposite the joiner's approach, the row tiles from the joiner merge's column to even_prio upstream, from the leg chain's Prio element; unrelated tracks may not stand on them nor touch the row's open far end. */
		for (int r = 0; r < rows; r++) {
			const Elem &prio_el = chain[leg_edge[r]].back();
			if (prio_el.k != Elem::K::Prio) continue;
			const uint32_t njm_node = graph.edges[out_edge[r]].from.index;
			const int sgn = east[r] ? 1 : -1;
			const int y_row = vy[prio_el.first];
			const int y_lane = vy[item_pt[row_items[r].lane]];
			const DiagDirection side = prio_el.index < row_items[r].lane ? DiagDirection::NW : DiagDirection::SE;
			const DiagDirection exit_edge = east[r] ? DiagDirection::SW : DiagDirection::NE;
			for (int step = 0; step <= even_prio; step++) {
				const auto [row_bits, lane_bits] = JunctionPrioStepBits(step, side, exit_edge);
				by_row[y_row].push_back({y_row, prio_el.last, -step * sgn, prio_el.last, -step * sgn, row_bits, UINT32_MAX, 0, njm_node, sgn, {prio_el.last, -step * sgn}, {prio_el.last, -step * sgn}});
				if (lane_bits.Any()) by_row[y_lane].push_back({y_lane, prio_el.last, -step * sgn, prio_el.last, -step * sgn, lane_bits, UINT32_MAX, 0, njm_node, sgn, {prio_el.last, -step * sgn}, {prio_el.last, -step * sgn}});
			}
		}

		/* Related occupants never constrain each other: the same edge and copy, a node's tile and an edge that starts or ends at the node. */
		auto at_node = [&](uint32_t edge, uint32_t node) { return edge != UINT32_MAX && ((graph.edges[edge].from.node && graph.edges[edge].from.index == node) || (graph.edges[edge].to.node && graph.edges[edge].to.index == node)); };
		auto related = [&](const Occupant &a, const Occupant &b) {
			if (a.edge != UINT32_MAX && a.edge == b.edge) return a.copy == b.copy || a.copy < 0 || b.copy < 0;
			if (a.edge == UINT32_MAX && b.edge == UINT32_MAX && a.node != UINT32_MAX && a.node == b.node) return true; // The two parts of one prio footprint.
			return (a.node != UINT32_MAX && at_node(b.edge, a.node)) || (b.node != UINT32_MAX && at_node(a.edge, b.node));
		};
		auto label = [&](const Occupant &o) { return o.node != UINT32_MAX ? fmt::format("node {}", o.node) : fmt::format("edge {}", o.edge); };
		for (auto &[y, occupants] : by_row) {
			/* On equal columns a footprint comes first in its travel direction: the unrelated track stands downstream of it. */
			std::ranges::stable_sort(occupants, [&](const Occupant &a, const Occupant &b) { return std::make_tuple(flat_left(a), flat_right(a), -a.footprint) < std::make_tuple(flat_left(b), flat_right(b), -b.footprint); });
			auto add_relation = [&](const Occupant &a, const Occupant &b, bool tie) -> bool {
				/* At a given point, pieces that may not share a tile are separated, not tied (DESIGN-G3 §G3.6 F1). */
				if (at_mode && tie && a.footprint == 0 && b.footprint == 0 && !JunctionTracksMayShareTile(a.bits, b.bits)) tie = false;
				reasons.push_back(JunctionPlacementReason{"occupancy", b.edge, b.node, -1});
				const uint32_t reason = static_cast<uint32_t>(reasons.size() - 1);
				notes[reason] = fmt::format("[{} and {} on row {}]", label(a), label(b), y);
				const int apart = flat_left(b) - flat_right(a);
				if (record) {
					const bool touch = !tie && a.bits.Any(DiagdirReachesTracks(ReverseDiagDir(DiagDirection::SW))) && b.bits.Any(DiagdirReachesTracks(ReverseDiagDir(DiagDirection::NE)));
					const bool touch_reversed = !tie && b.bits.Any(DiagdirReachesTracks(ReverseDiagDir(DiagDirection::SW))) && a.bits.Any(DiagdirReachesTracks(ReverseDiagDir(DiagDirection::NE)));
					auto occupant_key = [&](const Occupant &o) {
						return fmt::format("{}/{}/{}/{},{},{},{}/{},{},{},{}", static_cast<int>(o.edge), o.copy, static_cast<int>(o.node), o.left.pt, o.left.c0, o.left.c1, o.left.bend, o.right.pt, o.right.c0, o.right.c1, o.right.bend);
					};
					recorded_clashes.push_back({fmt::format("{}|{}|{}", occupant_key(a), occupant_key(b), tie), fmt::format("{}|{}|{}", occupant_key(b), occupant_key(a), tie), a.right, b.left, a.left, b.right, tie ? apart : (touch ? 2 : 1), tie ? apart : (touch_reversed ? 2 : 1), false, tie, false, ""});
					recorded_clash_reasons.push_back(reason);
				}
				if (tie) {
					cons.push_back({b.lp, a.rp, apart - b.ldx + a.rdx, reason});
					cons.push_back({a.rp, b.lp, -(apart - b.ldx + a.rdx), reason});
				} else {
					const bool touch = a.bits.Any(DiagdirReachesTracks(ReverseDiagDir(DiagDirection::SW))) && b.bits.Any(DiagdirReachesTracks(ReverseDiagDir(DiagDirection::NE)));
					cons.push_back({b.lp, a.rp, (touch ? 2 : 1) - b.ldx + a.rdx, reason});
				}
				pad_tags(true); // The tile extents of the occupants' pieces (bend rasters of 2m halves, rows) and the pair order from the flat solve.
				return true;
			};
			/* Legality per pair of occupants: two on one tile are legal when the graph relates them, or when they stand as Upper|Lower or Left|Right (D182, D217; a footprint is not a track: its pairs are ordered below); checked once, on the settled placement. */
			for (size_t i = 0; legality && i < occupants.size(); i++) {
				if (occupants[i].footprint != 0 || flat_left(occupants[i]) > flat_right(occupants[i])) continue;
				for (size_t j = i + 1; j < occupants.size() && flat_left(occupants[j]) <= flat_right(occupants[i]); j++) {
					if (occupants[j].footprint != 0 || flat_left(occupants[j]) > flat_right(occupants[j]) || related(occupants[i], occupants[j])) continue;
					if (!JunctionTracksMayShareTile(occupants[i].bits, occupants[j].bits)) {
						reasons.push_back(JunctionPlacementReason{"occupancy", occupants[j].edge, occupants[j].node, -1});
						const uint32_t reason = static_cast<uint32_t>(reasons.size() - 1);
						notes[reason] = fmt::format("[{} and {} on row {}]", label(occupants[i]), label(occupants[j]), y);
						occupancy_failed = true;
						failure.assign(1, reasons.back());
						if (error != nullptr) *error = "the placement rules contradict each other: occupancy (edge " + (occupants[i].edge != UINT32_MAX ? str(static_cast<int>(occupants[i].edge)) : std::string("-")) + ") " + notes[reason];
						return false;
					}
				}
			}
			if (legality) continue;
			/* Occupants on one flat tile form a group, tied to each other; groups are neighbours in the order. */
			std::vector<std::pair<size_t, size_t>> groups;
			for (size_t i = 0; i < occupants.size(); i++) {
				if (!groups.empty() && occupants[i].footprint == 0 && flat_left(occupants[i]) <= flat_right(occupants[i]) && occupants[groups.back().second].footprint == 0 && flat_left(occupants[groups.back().first]) == flat_left(occupants[i]) && flat_right(occupants[groups.back().first]) == flat_right(occupants[i])) {
					groups.back().second = i;
				} else {
					groups.emplace_back(i, i);
				}
			}
			for (size_t gi = 0; gi < groups.size(); gi++) {
				for (size_t i = groups[gi].first; i < groups[gi].second; i++) {
					if (!related(occupants[i], occupants[i + 1]) && !add_relation(occupants[i], occupants[i + 1], true)) return false;
				}
				if (gi + 1 == groups.size()) continue;
				bool any_related = false;
				TrackBits left_bits, right_bits;
				for (size_t i = groups[gi].first; i <= groups[gi].second; i++) {
					left_bits = left_bits | occupants[i].bits;
					for (size_t j = groups[gi + 1].first; j <= groups[gi + 1].second; j++) any_related = any_related || related(occupants[i], occupants[j]);
				}
				for (size_t j = groups[gi + 1].first; j <= groups[gi + 1].second; j++) right_bits = right_bits | occupants[j].bits;
				if (any_related) continue;
				Occupant a = occupants[groups[gi].second];
				Occupant b = occupants[groups[gi + 1].first];
				a.bits = left_bits;
				b.bits = right_bits;
				if (!add_relation(a, b, flat_left(b) <= flat_right(a) && a.footprint == 0 && b.footprint == 0)) return false;
			}
		}
		return true;
	};

	/* The heads of a doubled bridge hug its outermost crosser tiles by the flat solve's order (D80, D118, D217). */
	auto hug_heads = [&](const std::vector<int> &flat) {
		for (const HugCandidate &hug : hugs) {
			if (hug.tiles.empty()) continue;
			auto value = [&](const std::pair<int, int> &tile) { return flat[tile.first] + tile.second; };
			const std::pair<int, int> *low = &hug.tiles[0];
			const std::pair<int, int> *high = &hug.tiles[0];
			for (const std::pair<int, int> &tile : hug.tiles) {
				if (value(tile) < value(*low)) low = &tile;
				if (value(tile) > value(*high)) high = &tile;
			}
			why("bridge heads hug crossers", hug.edge, UINT32_MAX, hug.row);
			tag = SystemTag{nullptr, 0, 0, true, false, true}; // The crosser tile offset dx comes from the bend rasters of 2m halves and the rows; the outermost tile from the flat solve.
			eq(0, hug.west, low->first, low->second - 1);
			eq(0, hug.east, high->first, high->second + 1);
			tag = SystemTag{};
		}
	};


	/* The bridges' middle lines and heads (D217): a Y-axis bridge's column and rows, an X-axis (doubled) bridge's row and columns. */
	auto collect_bridges = [&](std::vector<BridgeLine> &lines, std::vector<HeadRef> &heads) {
		lines.clear();
		heads.clear();
		for (uint32_t e = 0; e < edge_count; e++) {
			if (is_row_edge(e)) continue;
			for (const Elem &el : chain[e]) {
				if (el.k != Elem::K::Heads || VX[el.hin] != 0 || bridges_of[e].empty()) continue;
				const int b = *std::ranges::find_if(bridges_of[e], [&](int bb) { return structure.bridges[bb].crossed[0] == static_cast<uint32_t>(el.cross); });
				/* A tunnel has no deck above the ground to keep clear (D220); its portals still count as heads. */
				if (!structure.bridges[b].tunnel) lines.push_back({b, e, false, el.first, el.last, el.first});
				heads.push_back({b, e, el.first, el.first});
				heads.push_back({b, e, el.last, el.last});
			}
		}
		/* A doubled leg's bridges: the HeadsPair elements of its two chains, one pair per bridge in travel order; the line of a copy's heads is the row item its chain lies on. */
		for (int r = 0; r < rows; r++) {
			if (!IsDoubledEdge(leg_edge[r])) continue;
			const uint32_t leg = leg_edge[r];
			for (size_t k = 0; k < bridges_of[leg].size(); k++) {
				const int b = bridges_of[leg][k];
				for (const Elem *h : {&nth_heads(chain[leg], k), &nth_heads(chain_copy[leg], k)}) {
					const int line = item_pt[h->index];
					lines.push_back({b, leg, true, h->first, h->last, line});
					heads.push_back({b, leg, h->first, line});
					heads.push_back({b, leg, h->last, line});
				}
			}
		}
	};

	/* Bridge middle clearance in X (D217): a head whose row lies strictly within a Y-axis bridge's rows keeps its flat side of the bridge's column by one. */
	auto add_clearance_x = [&](const std::vector<int> &flat) {
		std::vector<BridgeLine> lines;
		std::vector<HeadRef> heads;
		collect_bridges(lines, heads);
		for (const BridgeLine &line : lines) {
			if (line.x_axis) continue;
			const int top = std::min(vy[line.a], vy[line.b]);
			const int bottom = std::max(vy[line.a], vy[line.b]);
			for (const HeadRef &head : heads) {
				if (head.bridge == line.bridge || vy[head.ypt] <= top || vy[head.ypt] >= bottom) continue;
				const int side = flat[head.xpt] - flat[line.line];
				if (side == 0) continue;
				reasons.push_back(JunctionPlacementReason{"bridge middle clearance", line.edge, UINT32_MAX, -1});
				const uint32_t reason = static_cast<uint32_t>(reasons.size() - 1);
				notes[reason] = fmt::format("[edge {} and the head of edge {}]", line.edge, head.edge);
				if (side > 0) cons.push_back({head.xpt, line.line, 1, reason}); else cons.push_back({line.line, head.xpt, 1, reason});
				pad_tags(false); // Bridge middle clearance: the side comes from the flat solve, the constant 1 has no m.
			}
		}
	};

	/* The families that read columns (DESIGN-G3 §G3.6, R2): at columns x, the occupancy, the hug pins and the X clearance; false when the occupancy refuses. */
	auto detect = [&](const std::vector<int> &x) -> bool {
		if (!add_occupancy(x, false)) return false;
		hug_heads(x);
		if (!skip_clearance) add_clearance_x(x);
		return true;
	};

	/* One round: the constraints of a phase. */
	auto build = [&]() {
		cons.clear();
		cons_tag.clear();
		recorded_overs.clear();
		over_elems.clear();
		over_elems_after.clear();
		reasons.clear();
		fact_cons.clear();
		notes.clear();
		lazy_notes.clear();
		group_tiles.clear();
		why_now = JunctionPlacementReason{};
		const bool x_phase = phase == 0;
		if (x_phase) hugs.clear();

		/* Stack rows. */
		why("stack origin");
		eq(1, item_pt[0], 0, 0);
		for (int i = 1; i < items; i++) {
			why("stack rows", UINT32_MAX, UINT32_MAX, static_cast<int>(structure.stack[i].row));
			if (structure.stack[i].row == structure.stack[i - 1].row) eq(1, item_pt[i], item_pt[i - 1], 1); else ge(1, item_pt[i], item_pt[i - 1], 1);
		}
		std::vector<int> layer1;
		for (uint32_t i = 0; i < graph.nodes.size(); i++) {
			const int level = order.node_level[i];
			if (level % 2 != 0) continue;
			const int b = level / 2;
			why("band gap", UINT32_MAX, i);
			if (b >= 1) ge(1, node_pt[i], item_pt[row_last[b - 1]], 1);
			if (b < rows) ge(1, item_pt[row_first[b]], node_pt[i], 1);
		}
		for (const JunctionTrackBandEvent &event : order.bands[rows]) {
			if (event.node && event.layer == 1) layer1.push_back(static_cast<int>(event.a));
		}
		for (size_t i = 0; i < layer1.size(); i++) {
			why("S edge", UINT32_MAX, static_cast<uint32_t>(layer1[i]));
			/* Every layer-1 node of band R lies at least a port stub above the S edge; a node behind a fan lies further, by its chain (D115a, D230). */
			ge(1, se_pt, node_pt[layer1[i]], JunctionRunFloor(FloorKind::Node, FloorKind::Port, spec));
		}
		why("S port row");
		for (int s = 0; s < n; s++) {
			if (!group_of(s).main) eq(1, port_pt[s], se_pt, 0);
		}

		/* Ports. */
		std::vector<int> south;
		why("port columns");
		for (int s = 0; s < n; s++) {
			if (group_of(s).side == JunctionSide::W) eq(0, port_pt[s], 0, 0);
			if (group_of(s).side == JunctionSide::E) eq(0, port_pt[s], xe_pt, 0);
			if (group_of(s).side == JunctionSide::S) south.push_back(s);
		}
		int column = 0;
		why("S port spacing");
		for (size_t i = 1; i < south.size(); i++) {
			column += 1 + (slots[south[i]].group != slots[south[i - 1]].group ? 1 : 0);
			eq(0, port_pt[south[i]], port_pt[south[0]], column);
		}

		why("row nodes");
		/* The rows' nodes: the exit split lies on the exit item's row, the joiner merge on the lane item's. */
		for (int r = 0; r < rows; r++) {
			const uint32_t exit_item = programs.nodes[graph.edges[in_edge[r]].to.index].item; // The exit split's item, from the programs' node shape: the in edge's fan ends on it.
			const JunctionTrackEdge &in = graph.edges[in_edge[r]];
			const JunctionTrackEdge &out = graph.edges[out_edge[r]];
			eq(1, node_pt[in.to.index], item_pt[exit_item], 0);
			eq(1, node_pt[out.from.index], item_pt[programs.nodes[graph.edges[out_edge[r]].from.index].item], 0); // The joiner merge's item, from the programs' node shape: the out edge's fan starts on it.
		}

		if (!x_phase) {
			/* The waiting room of an exit path, from its first split to the first merge after it: 2 dy - 1 >= 2 tl units, so dy >= tl + 1 tiles (rule WaitingRoom). */
			for (size_t p = 0; p < paths.size(); p++) {
				if (!group_of(paths[p].from).main || group_of(paths[p].to).main) continue;
				uint32_t cur_edge = edge_count;
				for (uint32_t e = 0; e < edge_count; e++) {
					if (!graph.edges[e].from.node && graph.edges[e].from.index == paths[p].from && std::ranges::count(graph.edges[e].paths, static_cast<uint16_t>(p)) > 0) cur_edge = e;
				}
				int split_node = -1;
				for (int guard = 0; guard < 64 && cur_edge < edge_count && graph.edges[cur_edge].to.node; guard++) {
					const int nd = graph.edges[cur_edge].to.index;
					if (split_node < 0 && graph.nodes[nd].split) {
						split_node = nd;
					} else if (split_node >= 0 && !graph.nodes[nd].split) {
						why("waiting room", cur_edge, static_cast<uint32_t>(nd));
						ge(1, node_pt[nd], node_pt[split_node], JunctionRunFloor(FloorKind::Node, FloorKind::Node, spec));
						break;
					}
					uint32_t next_edge = edge_count;
					for (uint32_t e = 0; e < edge_count; e++) {
						if (graph.edges[e].from.node && graph.edges[e].from.index == nd && std::ranges::count(graph.edges[e].paths, static_cast<uint16_t>(p)) > 0) next_edge = e;
					}
					cur_edge = next_edge;
				}
			}
		}

		/* The chains of the other edges. */
		for (uint32_t e = 0; e < edge_count; e++) {
			const std::vector<Elem> &elems = chain[e];
			const JunctionTrackEdge &edge = graph.edges[e];
			/* The prio element (RW6): on its Prio item's row, ending in the column of the joiner merge, even_prio steps long. Its run from the element before it is added with that element, below. */
			const Elem *prio_el = !elems.empty() && elems.back().k == Elem::K::Prio ? &elems.back() : nullptr;
			if (prio_el != nullptr) {
				why("prio element", e, edge.to.index, (end_level(edge.from) - 1) / 2);
				eq(1, prio_el->first, item_pt[prio_el->index], 0);
				eq(1, prio_el->last, item_pt[prio_el->index], 0);
				eq(0, prio_el->last, node_pt[edge.to.index], 0);
				along(prio_el->hout, prio_el->last, prio_el->first, even_prio, true);
			}
			if (is_row_edge(e) && doubled_edge(e)) {
				/* A doubled leg: copy 0's chain holds its HeadsPairs and its lane merge S-bend, copy 1's its lane split S-bend and its HeadsPairs, each between its two Tiles. The lane pair joins the two (copy 1's S-bend starts at the lane split, copy 0's ends at the lane merge) and the heads of copy 1 are copy 0's one tile upstream (D118). */
				const int r = (end_level(edge.from) - 1) / 2;
				const std::vector<Elem> &copy = chain_copy[e];
				const size_t g = std::ranges::count(elems, Elem::K::HeadsPair, &Elem::k);
				const int nex = elems.front().first;
				const Elem &bend0 = lane_bend(e, elems, Step::Role::LaneMerge);
				const Elem &bend1 = lane_bend(e, copy, Step::Role::LaneSplit);
				auto heads0 = [&](size_t b) -> const Elem & { return nth_heads(elems, b); };
				auto heads1 = [&](size_t b) -> const Elem & { return nth_heads(copy, b); };
				why("row items", e, UINT32_MAX, r);
				/* The leg's S-bends start on the exit item, where the in edge's fan ends (the exit split lies there), and end on their own token's item. */
				const uint32_t exit_item = programs.nodes[graph.edges[in_edge[r]].to.index].item;
				eq(1, bend1.first, item_pt[exit_item], 0);
				eq(1, bend1.last, item_pt[bend1.index], 0);
				for (size_t b = 0; b < g; b++) {
					for (const Elem *h : {&heads0(b), &heads1(b)}) {
						eq(1, h->first, item_pt[h->index], 0);
						eq(1, h->last, item_pt[h->index], 0);
					}
				}
				eq(1, bend0.first, item_pt[exit_item], 0);
				eq(1, bend0.last, item_pt[bend0.index], 0);
				if (!x_phase) continue;

				const int s = east[r] ? 1 : -1;
				const int heading = east[r] ? 1 : 3;
				/* The lane split sits next to its bridge (D215); the run from the exit split to the lane split, and from the last heads to the lane merge, are rule minima, not exact: series bridges need the slack (DESIGN-G1 §11.9b). */
				why("lane split run", e, edge.from.index, r);
				along(heading, bend1.first, nex, JunctionRunFloor(FloorKind::Node, FloorKind::LaneSplit, spec), false);
				along(heading, heads0(0).first, bend1.first, JunctionRunFloor(FloorKind::LaneSplit, FloorKind::Heads, spec), true);
				eq(0, bend1.last, bend1.first, 0);
				along(heading, heads1(0).first, bend1.last, 1, true); // A shape, not a floor: copy 1 sits one tile nearer than copy 0, the head stagger (D118, D145).
				for (size_t b = 0; b < g; b++) {
					along(heading, heads0(b).first, heads1(b).first, 1, true); // Shape: the head stagger, copy 0 one past copy 1 (D118).
					along(heading, heads0(b).last, heads1(b).last, 1, true); // Shape: the head stagger (D118).
					if (b > 0) along(heading, heads0(b).first, heads0(b - 1).last, JunctionRunFloor(FloorKind::Heads, FloorKind::Heads, spec), false);
				}
				why("series bridge heads", e, UINT32_MAX, r);
				along(heading, bend0.first, heads0(g - 1).last, 1, false); // Shape: one tile past copy 0's far head, the stagger (D118, D145); the floor is the lane merge's, next.
				eq(0, bend0.last, bend0.first, 0);
				along(heading, bend0.last, heads1(g - 1).last, JunctionRunFloor(FloorKind::Heads, FloorKind::LaneMerge, spec), false);
				/* The element's row ends at step even_prio + 1, which must not meet the lane merge's S-bend (RECON-prio-element §3-4). */
				if (prio_el != nullptr) {
					why("prio element", e, graph.edges[out_edge[r]].from.index, r);
					along(heading, prio_el->first, bend0.last, JunctionRunFloor(FloorKind::LaneMerge, FloorKind::Prio, spec), false);
				}

				/* Every crosser's tiles on the copy row and the lane row lie between its bridge's heads; the heads hug the outermost tiles by the flat solve's order, below (D80, D118, D145, D217). */
				const int u = -s; // The upstream step.
				why("bridge heads hug crossers", e, UINT32_MAX, r);
				for (size_t b = 0; b < g; b++) {
					const JunctionTrackBridge &bridge = structure.bridges[bridges_of[e][b]];
					HugCandidate hug{s > 0 ? heads0(b).first : heads0(b).last, s > 0 ? heads0(b).last : heads0(b).first, e, r, {}};
					for (uint32_t x : bridge.crossed) {
						if (!rows_known) break;
						const std::vector<SymTile> tiles = edge_tiles(x);
						for (int pass = 0; pass < 2; pass++) {
							const Elem &near_far = pass == 0 ? heads0(b) : heads1(b);
							const int y = vy[near_far.first];
							bool any = false;
							for (const SymTile &z : tiles) {
								if (z.y != y) continue;
								any = true;
								tag = SystemTag{nullptr, 0, 0, z.bend, false}; // A bend tile's offset dx comes from the bend rasters of 2m halves and the rows; a straight tile's is exact (0).
								if (s > 0) {
									ge(0, z.pt, near_far.first, 1 - z.dx);
									ge(0, near_far.last, z.pt, 1 + z.dx);
								} else {
									ge(0, near_far.first, z.pt, 1 + z.dx);
									ge(0, z.pt, near_far.last, 1 - z.dx);
								}
								hug.tiles.emplace_back(z.pt, pass == 0 ? z.dx : z.dx - u);
							}
							tag = SystemTag{};
							if (!any) { build_why = "Z row " + str(y) + " edge " + str(x) + " row " + str(r); return false; }
						}
					}
					hugs.push_back(std::move(hug));
				}
				continue;
			}
			for (size_t i = 0; i < elems.size(); i++) {
				const Elem &el = elems[i];
				if (el.k == Elem::K::Fan) {
					/* The fan is its port row's S-bend: m - 1 columns across, first at the port row and last at its item row; empty, it lies on the port's column (R3). */
					why("fan size", e, UINT32_MAX, (end_level(edge.from) - 1) / 2);
					const int port = el.port_first ? elems[i - 1].first : elems[i + 1].first;
					if (el.m == 0) {
						tag = SystemTag{&el, 0, 0, false, true}; // An empty fan lies on its port.
						eq(0, el.first, port, 0);
						eq(0, el.last, port, 0);
					} else {
						tag = SystemTag{&el, 1, -1, false, false};
						along(el.hin, el.last, el.first, el.m - 1, true);
					}
					tag = SystemTag{};
					eq(1, el.port_first ? el.last : el.first, item_pt[el.index], 0);
				} else if (el.k == Elem::K::Corner) {
					const int kk = k;
					const int dx = kk * (VX[el.hin] + VX[el.hout]);
					const int dy = kk * (VY[el.hin] + VY[el.hout]);
					if (el.joiner_div) {
						why("joiner_div Corner floor", e);
						ge(1, el.first, el.last, k);
						if (fixed != nullptr) {
							why("joiner_div Corner choice", e);
							tag = SystemTag{&el, 1, 0, false, false};
							eq(1, el.first, el.last, fixed->corner[e][el.tok]);
							tag = SystemTag{};
						}
						if (x_phase) {
							const int kp = fixed != nullptr ? fixed->corner[e][el.tok] : (rows_known ? std::abs(vy[el.first] - vy[el.last]) : k);
							tag = SystemTag{&el, VX[el.hin] + VX[el.hout], 0, false, false};
							eq(0, el.last, el.first, kp * (VX[el.hin] + VX[el.hout]));
							tag = SystemTag{};
						}
					} else {
						why("Corner size", e);
						eq(0, el.last, el.first, dx);
						eq(1, el.last, el.first, dy);
					}
					if (el.index != NO_ROW) eq(1, el.last, item_pt[el.index], 0);
				} else if (el.k == Elem::K::SBend) {
					const int side = (el.hin + el.hand + 4) % 4;
					why("S-bend size", e);
					if (el.fan && (el.hin == 0 || el.hin == 2)) {
						/* A fan at an S branch port: any size toward its side, 0 allowed (D230). */
						tag = el.m == 0 ? SystemTag{&el, 0, 0, false, true} : SystemTag{&el, VY[el.hin], -VY[el.hin], false, false}; // An empty S fan has no rows.
						if (el.m == 0) eq(1, el.last, el.first, 0); else eq(1, el.last, el.first, VY[el.hin] * (el.m - 1));
						tag = SystemTag{&el, 1, 0, false, false};
						if (fixed != nullptr) {
							if (VX[side] > 0) eq(0, el.last, el.first, el.m); else eq(0, el.first, el.last, el.m);
						} else {
							if (VX[side] > 0) ge(0, el.last, el.first, el.m); else ge(0, el.first, el.last, el.m);
						}
						tag = SystemTag{};
					} else if (el.hin == 0 || el.hin == 2) {
						tag = SystemTag{&el, VY[el.hin], -VY[el.hin], false, false};
						eq(1, el.last, el.first, VY[el.hin] * (el.m - 1));
						tag = SystemTag{&el, 1, 0, false, false};
						if (fixed != nullptr) {
							if (VX[side] > 0) eq(0, el.last, el.first, el.m); else eq(0, el.first, el.last, el.m);
						} else {
							if (VX[side] > 0) ge(0, el.last, el.first, el.m); else ge(0, el.first, el.last, el.m);
						}
						tag = SystemTag{};
					} else {
						if (el.index == NO_ROW) eq(1, el.last, el.first, VY[side]); // No item pins it: one row.
						eq(0, el.last, el.first, VX[el.hin] * 0);
					}
					if (el.index != NO_ROW) eq(1, el.last, item_pt[el.index], 0);
				} else if (el.k == Elem::K::Heads) {
					why("bridge heads", e);
					eq(VX[el.hin] != 0 ? 1 : 0, el.last, el.first, 0);
				}
				if (i + 1 >= elems.size()) continue;
				const Elem &next = elems[i + 1];
				if (next.k == Elem::K::Prio) continue; // The prio element is no track of the chain: nothing runs to it from the end tile.
				if (next.first == el.last) continue;
				/* An empty fan joins its port tile; the run on its node side stays (D230). */
				if ((i == 0 && !edge.from.node && next.fan && next.m == 0) || (el.fan && el.m == 0 && !edge.to.node && i + 2 == elems.size())) {
					why("fan size", e);
					tag = SystemTag{next.fan ? &next : &el, 0, 0, false, true}; // An empty S fan joins its port tile.
					eq(0, next.first, el.last, 0);
					eq(1, next.first, el.last, 0);
					tag = SystemTag{};
					continue;
				}
				bool exact = false;
				const bool in_out = IsInEdge(e) || IsOutEdge(e);
				/* A row edge's tiles name their kind: a port, the exit split of an in edge (Node), the joiner merge of an out edge (PrioNode), a fan (R3). */
				const auto floor_kind = [&](const Elem &o) {
					if (o.k == Elem::K::Corner) return FloorKind::Corner;
					if (o.k == Elem::K::SBend) return FloorKind::SBend;
					if (o.k == Elem::K::Heads) return FloorKind::Heads;
					if (o.k == Elem::K::Fan) return FloorKind::Fan;
					if (is_row_edge(e) && !in_out) return &o == &elems.front() ? FloorKind::Node : FloorKind::PrioNode; // An undoubled leg: the exit split to the joiner merge.
					if (!in_out) return FloorKind::Tile;
					return o.slot >= 0 ? FloorKind::Port : (edge.to.node ? FloorKind::Node : FloorKind::PrioNode);
				};
				/* The element before the end tile of a leg with a prio element: its run to the prio element's first tile is the floor, the run on to the merge only keeps the lane row (the element's span is exact). */
				const bool before_prio = prio_el != nullptr && i + 3 == elems.size();
				const int dist = before_prio ? 0 : JunctionRunFloor(floor_kind(el), floor_kind(next), spec);
				if (before_prio) {
					why("prio element", e, edge.to.index, (end_level(graph.edges[e].from) - 1) / 2); // The prio element's coverage is a floor on its first tile (D217).
					along(el.hout, prio_el->first, el.last, JunctionRunFloor(floor_kind(el), FloorKind::Prio, spec), false);
				} else {
					why("chain run", e);
				}
				if (next.k == Elem::K::Tile && i + 2 == elems.size() && !edge.to.node) exact = true;
				if (el.k == Elem::K::Tile && i == 0 && !edge.from.node && !in_out) exact = true;
				const auto port_tile = [](const Elem &o) { return o.k == Elem::K::Tile && o.slot >= 0; };
				if (in_out && (port_tile(el) || port_tile(next))) {
					/* A run to a port is along the heading only: the port's row is a number, not a point (R3); an empty fan lies on its port already. */
					const Elem &fan = el.k == Elem::K::Fan ? el : next;
					if (fan.k == Elem::K::Fan && fan.m == 0) continue;
					along(el.hout, next.first, el.last, dist, exact);
					continue;
				}
				run(el.hout, next.first, el.last, dist, exact);
			}

			/* A single bridge passes over an S-bend of the edge it crosses beside its Under (or Above) token, the one before it or the one after it as a choice (default before), in the column t of the S-bend, 1 unless a choice says otherwise (D113, F4, DESIGN-G3 §G3.4a, §G3.4b). */
			for (const Elem &el : elems) {
				if (el.k != Elem::K::Heads) continue;
				const std::vector<Elem> &other = chain[el.cross];
				const std::vector<JunctionTrackStep> &crossed_program = programs.edges[el.cross];
				const auto is_crossing = [&](int tok) { return tok >= 0 && static_cast<size_t>(tok) < crossed_program.size() && (crossed_program[tok].kind == StepKind::Under || crossed_program[tok].kind == StepKind::Above) && crossed_program[tok].index == e; };
				const Elem *before = nullptr;
				const Elem *after = nullptr;
				for (const Elem &o : other) {
					if (o.k != Elem::K::SBend || o.fan || o.tok < 0) continue;
					if (is_crossing(o.tok + 1)) before = &o;
					if (is_crossing(o.tok - 1)) after = &o;
				}
				if (before == nullptr && after == nullptr) { build_why = "no crossed sbend"; return false; }
				const int chosen = fixed != nullptr && !fixed->over_after.empty() ? fixed->over_after[e][el.tok] : 0;
				const Elem *chosen_elem = chosen != 0 ? after : before;
				if (chosen_elem == nullptr) { build_why = "a bridge over an S-bend that is not there"; return false; }
				/* The column t counted from the start: 1 + the choice, 1 when empty or when not fixed. */
				const int t = 1 + (fixed != nullptr && !fixed->over.empty() ? fixed->over[e][el.tok] : 0);
				const bool both = before != nullptr && after != nullptr;
				why("bridge over S-bend", e);
				recorded_overs.push_back({static_cast<int>(e), el.tok, -1, t, 0, 0, -1, 0, 0, chosen});
				over_elems.push_back(before);
				over_elems_after.push_back(after);
				const int over_index = static_cast<int>(recorded_overs.size()) - 1;
				for (int alternative = 0; alternative < 2; alternative++) {
					const Elem *crossed = alternative == 0 ? before : after;
					if (crossed == nullptr) continue;
					const bool is_chosen = crossed == chosen_elem;
					if (!is_chosen && system == nullptr) continue;
					const int side = (crossed->hin + crossed->hand + 4) % 4;
					const JunctionSBendColumns form = JunctionSBendColumnForm(crossed->hin, crossed->hand);
					(alternative == 0 ? recorded_overs.back().t0 : recorded_overs.back().t0_after) = form.t0;
					(alternative == 0 ? recorded_overs.back().t1 : recorded_overs.back().t1_after) = form.t1;
					if (is_chosen && t > form.t0 + form.t1 * std::max(crossed->m, 2)) { build_why = "a bridge over an S-bend beyond its diagonal"; return false; }
					const int lo = form.lo + form.s * (t - 1);
					const int hi = form.hi + form.s * (t - 1);
					const bool north = el.hin == 0;
					const int alt = both ? alternative : -1;
					record_only_now = !is_chosen;
					/* The rows of column t follow the diagonal (premise P3): the relations are linear in t, so a recording tags them with it. */
					tag = SystemTag{nullptr, 0, 0, false, false, false, over_index, VX[side], alt};
					eq(0, el.first, crossed->first, VX[side] * t);
					tag = SystemTag{crossed, 0, north ? hi + 1 : lo - 1, false, false, false, over_index, form.s, alt};
					eq(1, el.first, crossed->first, north ? hi + 1 : lo - 1);
					tag.b = north ? lo - 1 : hi + 1;
					eq(1, el.last, crossed->first, north ? lo - 1 : hi + 1);
					tag = SystemTag{};
					record_only_now = false;
				}
			}
		}

		if (!x_phase && !floors_off) {
			why("y floor");
			for (const std::pair<int, int> &floor : y_floors) ge(1, floor.first, 0, floor.second);
		}
		if (!x_phase && !skip_clearance) {
			for (const ClearanceY &c : clear_y) {
				reasons.push_back(JunctionPlacementReason{"bridge middle clearance", c.edge_b, UINT32_MAX, -1});
				notes[static_cast<uint32_t>(reasons.size() - 1)] = fmt::format("[edge {} and the head of edge {}]", c.edge_b, c.edge_h);
				cons.push_back({c.v, c.u, 1, static_cast<uint32_t>(reasons.size() - 1)});
				pad_tags(false); // The side comes from the flat state, the constant 1 has no m.
			}
		}

		if (x_phase) {
			recorded_routes.clear();
			recorded_route_curves.clear();
			recorded_clashes.clear();
			recorded_clash_reasons.clear();
		}
		if ((!x_phase || rows_known) && spec.curve_limit) {
			/* The curve-gap family (D217): every pair JunctionCurveGaps returns for a route's curves. A pair in one segment (only vertical runs and fixed spans between the curves) is a difference of its anchors' rows in the Y phase; every other pair is a difference of its anchors' columns in the X phase. Pairs on one anchor are constants. */
			std::vector<SymRoute> routes;
			if (!sym_routes(routes, x_phase)) return false;
			if (x_phase && system != nullptr) {
				/* The routes of the counters, with each sized bend's length variable named by the system's bend id (the order record_system gives them). */
				std::map<const Elem *, int> ids;
				for (uint32_t e = 0; e < edge_count; e++) {
					for (const std::vector<Elem> *elems : {&chain[e], &chain_copy[e]}) {
						for (const Elem &el : *elems) {
							if (el.k == Elem::K::Corner || el.k == Elem::K::SBend || el.k == Elem::K::Fan) ids.emplace(&el, static_cast<int>(ids.size()));
						}
					}
				}
				for (SymRoute &route : routes) {
					for (size_t i = 0; i < route.rec.bends.size(); i++) {
						if (route.rec.bends[i].bend != -2) continue;
						const auto found = ids.find(route.rec_elems[i]);
						if (found == ids.end()) { build_why = "a sized bend has no system bend"; return false; }
						route.rec.bends[i].bend = found->second;
					}
					const int index = static_cast<int>(recorded_routes.size());
					for (size_t k = 0; k < route.curves.size(); k++) recorded_route_curves.push_back({index, static_cast<int>(k), route.curves[k].xoff, route.curves[k].yoff, route.curves[k].seg});
					recorded_routes.push_back(route.rec);
				}
			}
			/* One relation per (v, u): the largest of the pairs'. */
			struct Relation {
				int v, u, least;
				uint32_t edge;
				int row;
			};
			std::vector<Relation> relations;
			std::map<std::pair<int, int>, size_t> at;
			for (const SymRoute &route : routes) {
				std::vector<JunctionCurveHand> hands;
				for (const SymCurve &curve : route.curves) hands.push_back(curve.hand);
				for (const JunctionCurveGap &gap : JunctionCurveGaps(hands, spec.tl, spec.curve_speed, spec.reference_speed)) {
					const SymCurve &ci = route.curves[gap.i];
					const SymCurve &cj = route.curves[gap.j];
					const bool vertical = ci.seg >= 0 && ci.seg == cj.seg;
					if (vertical == x_phase) continue;
					/* The units from curve i to curve j are off_j - off_i + 2 sg (a_j - a_i), at least min_gap; sg is s (columns) or sv (rows). */
					const int sg = vertical ? (route.seg_sign[ci.seg] != 0 ? route.seg_sign[ci.seg] : 1) : route.sign;
					const int delta = vertical ? cj.yoff - ci.yoff : cj.xoff - ci.xoff;
					const int need = gap.min_gap - delta;
					const int least = need > 0 ? (need + 1) / 2 : -(-need / 2);
					if (ci.pt == cj.pt) {
						if (least > 0) { build_why = "curve gap " + str(gap.min_gap) + " between edge " + str(ci.edge) + " and edge " + str(cj.edge) + " is not kept: " + str(delta); return false; }
						continue;
					}
					const int v = sg > 0 ? cj.pt : ci.pt;
					const int u = v == cj.pt ? ci.pt : cj.pt;
					const auto [found, added] = at.emplace(std::make_pair(v, u), relations.size());
					if (added) {
						relations.push_back({v, u, least, cj.edge, cj.row});
					} else if (relations[found->second].least < least) {
						relations[found->second] = {v, u, least, cj.edge, cj.row};
					}
				}
			}
			for (const Relation &relation : relations) {
				why("curve gap", relation.edge, UINT32_MAX, relation.row);
				tag = SystemTag{nullptr, 0, 0, true, false}; // The unit offsets come from the bend rasters of 2m halves (and, in X, the rows): ceil(need / 2), the largest pair per (v, u).
				ge(x_phase ? 0 : 1, relation.v, relation.u, relation.least);
				tag = SystemTag{};
			}
		}

		if (x_phase && rows_known && !relations_only) {
			/* The column-reading families (detect) follow the order of the flat solve of the relations above (D217); when the relations contradict each other the solve below explains it. R3 hands detect another point. */
			compute_node_tracks();
			if (at_mode) {
				if (!detect(at->x)) return false;
			} else {
				std::vector<int> flat;
				if (flat_solve(flat) && !detect(flat)) return false;
			}
		}

		if (x_phase && !floors_off) {
			why("x floor");
			for (const std::pair<int, int> &floor : x_floors) ge(0, floor.first, 0, floor.second);
		}

		if (x_phase && facts != nullptr && !relations_only) {
			/* The terrain's facts (DESIGN-G1 §14.4): every group of tiles gets the columns where its tile is buildable, every straight run on a row must be buildable on each tile it covers. Bridge middles are not checked: the node path checks no bridge piece either (JunctionNodeHeightsOk skips on_bridge). */
			const int high = facts->MaxX() - offset.x;
			std::vector<uint32_t> node_of_pt(pt_count, UINT32_MAX);
			for (uint32_t i = 0; i < node_pt.size(); i++) node_of_pt[node_pt[i]] = i;
			/* The height facts are the costly ones: they wait until every other fact of the block is in, and then look only at the columns a point can take under the relations and those facts (least and most, below). */
			std::vector<int> least(pt_count, 0);
			std::vector<int> most(pt_count, INT_MAX / 2);
			std::vector<std::function<void()>> pending;
			auto fact_reason = [&](uint32_t e, uint32_t node, int row, const std::string &note, const char *rule = "blocked tile") {
				reasons.push_back(JunctionPlacementReason{rule, e, node, static_cast<int32_t>(row)});
				if (!note.empty()) notes[static_cast<uint32_t>(reasons.size() - 1)] = note;
				return static_cast<uint32_t>(reasons.size() - 1);
			};
			auto lazy_note = [&](uint32_t reason, int kind, int x, int y) {
				if (lazy_notes.size() <= reason) lazy_notes.resize(reason + 1, {0, 0, 0});
				lazy_notes[reason] = {kind, x, y};
			};
			auto holds = [&](JunctionFactKind kind, Axis axis, int x, int y) {
				int entry, exit;
				if (classes_only) return facts->Buildable(x, y);
				switch (kind) {
					case JunctionFactKind::Straight: return facts->Straight(x, y, axis, entry, exit);
					case JunctionFactKind::Switch: return facts->Switch(x, y);
					default: return facts->Half(x, y);
				}
			};
			/* Per (fact, axis, row), whether the fact holds at each column of the snapshot: every group on that row asks for it. */
			std::map<std::tuple<int, int, int>, std::vector<bool>> holding;
			auto holds_on_row = [&](JunctionFactKind kind, Axis axis, int y) -> const std::vector<bool> & {
				const auto key = classes_only ? std::make_tuple(0, 0, y) : std::make_tuple(static_cast<int>(kind), static_cast<int>(axis), y);
				auto found = holding.find(key);
				if (found != holding.end()) return found->second;
				std::vector<bool> row;
				for (int x = facts->MinX(); x <= facts->MaxX(); x++) row.push_back(holds(kind, axis, x, y));
				return holding.emplace(key, std::move(row)).first->second;
			};
			/* A group tile: anchor point pt, column offset dx, row y; its anchor's columns where the fact holds. */
			auto group = [&](int pt, int dx, int y, JunctionFactKind kind, Axis axis, uint32_t e, uint32_t node, int row) {
				if (record != nullptr) record->near_groups.push_back({pt, dx, y});
				std::vector<int> allowed;
				std::array<int, 6> named{0, y + offset.y, 0, 0, 0, 0};
				const std::vector<bool> &here = holds_on_row(kind, axis, y + offset.y);
				for (int c = 0; c <= high; c++) {
					const int x = c + dx + offset.x;
					const bool inside = x >= facts->MinX() && x <= facts->MaxX();
					if (inside && here[x - facts->MinX()]) {
						allowed.push_back(c);
					} else if (inside && named[0] < 4) {
						named[2 + named[0]++] = x;
					}
				}
				/* The note names the first blocked tiles; it is formatted only when a failure reports the reason. */
				const uint32_t reason = fact_reason(e, node, row, std::string());
				if (named[0] > 0) {
					lazy_note(reason, 3, static_cast<int>(group_tiles.size()), 0);
					group_tiles.push_back(named);
				}
				fact_cons.push_back({pt, -1, std::move(allowed), reason});
			};
			/* Per row, the columns of the snapshot where a straight X piece cannot sit, and the columns whose straight X piece does not meet the next one's height. */
			std::map<int, std::pair<std::vector<int>, std::vector<int>>> row_facts;
			auto row_facts_of = [&](int y) -> const std::pair<std::vector<int>, std::vector<int>> & {
				auto found = row_facts.find(y);
				if (found != row_facts.end()) return found->second;
				std::pair<std::vector<int>, std::vector<int>> bad;
				int entry, exit;
				bool before = false;
				for (int x = facts->MinX(); x <= facts->MaxX(); x++) {
					const bool straight = classes_only ? facts->Buildable(x, y) : facts->Straight(x, y, Axis::X, entry, exit);
					if (!straight) bad.first.push_back(x);
					if (x > facts->MinX() && before && straight && !facts->Joins(Axis::X, y, x - 1)) bad.second.push_back(x - 1);
					before = straight;
				}
				return row_facts.emplace(y, std::move(bad)).first->second;
			};
			/* A straight run on row y from point a to point b, in the travel sense sgn (+1 east). */
			auto row_run = [&](int a, int b, int y, int sgn, uint32_t e, int row) {
				if (record != nullptr) record->near_runs.push_back({a, b, y, sgn});
				const auto &[blocked, steps] = row_facts_of(y + offset.y);
				for (int x : blocked) {
					const uint32_t reason = fact_reason(e, UINT32_MAX, row, std::string());
					lazy_note(reason, 1, x, y + offset.y);
					fact_cons.push_back({sgn > 0 ? a : b, sgn > 0 ? b : a, {x - offset.x, x - offset.x}, reason});
				}
				/* Two straight tiles side by side that do not meet at one height: the run must not cover both. */
				if (!heights) return;
				for (int x : steps) {
					const uint32_t reason = fact_reason(e, UINT32_MAX, row, std::string(), "height step");
					lazy_note(reason, 2, x, y + offset.y);
					fact_cons.push_back({sgn > 0 ? a : b, sgn > 0 ? b : a, {x - offset.x, x + 1 - offset.x}, reason});
				}
			};
			/* A vertical run of straight tiles at column offset 0 of point pt, rows from y_a to y_b (either order): the columns where every two neighbours meet at one height. */
			auto column_run_now = [&](int pt, int y_a, int y_b, uint32_t e) {
				const int y_lo = std::min(y_a, y_b);
				const int y_hi = std::max(y_a, y_b);
				if (y_lo == y_hi) return;
				std::vector<int> allowed;
				std::string tiles;
				for (int c = std::max(0, least[pt]); c <= std::min(high, most[pt]); c++) {
					int bad = INT_MIN;
					for (int y = y_lo; y < y_hi && bad == INT_MIN; y++) {
						if (!facts->Joins(Axis::Y, c + offset.x, y + offset.y)) bad = y;
					}
					if (bad == INT_MIN) {
						allowed.push_back(c);
					} else if (c + offset.x >= facts->MinX() && c + offset.x <= facts->MaxX() && tiles.size() < 40) {
						tiles += fmt::format("{}({},{})", tiles.empty() ? "" : " ", c + offset.x, bad + offset.y);
					}
				}
				const uint32_t reason = fact_reason(e, UINT32_MAX, -1, tiles.empty() ? std::string() : "[height step " + tiles + "]", "height step");
				fact_cons.push_back({pt, -1, std::move(allowed), reason});
			};
			auto column_run = [&](int pt, int y_a, int y_b, uint32_t e) { if (heights) pending.push_back([&column_run_now, pt, y_a, y_b, e] { column_run_now(pt, y_a, y_b, e); }); };
			/* A node's tile: the columns where all its tracks fit (and the axis pieces on both sides of a route that goes straight through), and of those where the route meets them at one height. */
			auto node_checks = [&]() {
				for (uint32_t node = 0; node < graph.nodes.size(); node++) {
					const int y = vy[node_pt[node]] + offset.y;
					std::vector<int> fits_ok, joins_ok;
					std::string fit_tiles, join_tiles;
					for (int c = std::max(0, least[node_pt[node]]); c <= std::min(high, most[node_pt[node]]); c++) {
						const int x = c + offset.x;
						bool fits = facts->Fits(x, y, node_bits[node]);
						bool joins = true;
						for (int heading : node_straight[node]) {
							const DiagDirection exit = TrackdirToExitdir(JUNCTION_AXIS_TRACKDIR[heading]);
							const TrackBits lone{TrackdirToTrack(JUNCTION_AXIS_TRACKDIR[heading])};
							/* The neighbours beyond the snapshot's columns carry no facts. */
							const bool before_inside = x - VX[heading] >= facts->MinX() && x - VX[heading] <= facts->MaxX();
							const bool after_inside = x + VX[heading] >= facts->MinX() && x + VX[heading] <= facts->MaxX();
							const int before = before_inside ? facts->EdgeZ(x - VX[heading], y - VY[heading], lone, exit) : 0;
							const int after = after_inside ? facts->EdgeZ(x + VX[heading], y + VY[heading], lone, ReverseDiagDir(exit)) : 0;
							const int in = facts->EdgeZ(x, y, node_bits[node], ReverseDiagDir(exit));
							const int out = facts->EdgeZ(x, y, node_bits[node], exit);
							fits = fits && before != INT_MIN && after != INT_MIN && in != INT_MIN && out != INT_MIN;
							joins = joins && (!before_inside || before == in) && (!after_inside || out == after);
						}
						if (fits) fits_ok.push_back(c);
						if (fits && joins) joins_ok.push_back(c);
						if (x >= facts->MinX() && x <= facts->MaxX() && fit_tiles.size() < 40 && !fits) fit_tiles += fmt::format("{}({},{})", fit_tiles.empty() ? "" : " ", x, y);
						if (x >= facts->MinX() && x <= facts->MaxX() && join_tiles.size() < 40 && fits && !joins) join_tiles += fmt::format("{}({},{})", join_tiles.empty() ? "" : " ", x, y);
					}
					const size_t evaluated = static_cast<size_t>(std::max(0, std::min(high, most[node_pt[node]]) + 1 - std::max(0, least[node_pt[node]])));
					if (fits_ok.size() != evaluated) fact_cons.push_back({node_pt[node], -1, std::move(fits_ok), fact_reason(UINT32_MAX, node, -1, fit_tiles.empty() ? std::string() : "[blocked tile " + fit_tiles + "]")});
					if (joins_ok.size() != evaluated) fact_cons.push_back({node_pt[node], -1, std::move(joins_ok), fact_reason(UINT32_MAX, node, -1, join_tiles.empty() ? std::string() : "[height join " + join_tiles + "]", "height join")});
				}
			};
			/* A bend of 'halves' half pieces whose first sits at row y of point pt: the columns where each piece fits with its own track, each join inside it meets at one height, and its two joins with the axis pieces just outside it do too (DESIGN-G1 §14.3). */
			auto bend_heights_now = [&](int pt, int y, int heading, int hand, int halves, bool corner, uint32_t e, int row, uint32_t node_first, uint32_t node_last) {
				if (halves < 1) return; // A bend sized to 0 columns is no bend.
				const JunctionRaster raster = RasterizeJunctionRoute({{0, 0}, JUNCTION_AXIS_TRACKDIR[heading], JunctionBendMoves(hand, halves, corner)});
				if (!raster.error.empty() || static_cast<int>(raster.pieces.size()) != halves + 2) return;
				const std::vector<JunctionPiece> &pieces = raster.pieces;
				/* Per piece: its tile relative to the first half, its track (a piece on a node's tile sits with every other track the routes build there), the edge it leaves by. */
				const size_t count = pieces.size();
				std::vector<int> dx(count), dy(count);
				std::vector<TrackBits> bits(count);
				std::vector<DiagDirection> exit(count);
				for (size_t i = 0; i < count; i++) {
					dx[i] = pieces[i].tile.x - pieces[1].tile.x;
					dy[i] = y + pieces[i].tile.y - pieces[1].tile.y + offset.y;
					bits[i] = TrackBits{TrackdirToTrack(pieces[i].trackdir)};
					exit[i] = TrackdirToExitdir(pieces[i].trackdir);
				}
				if (node_first != UINT32_MAX) bits[1] = node_bits[node_first];
				if (node_last != UINT32_MAX) bits[halves] = node_bits[node_last];
				/* Two domains: where the tracks cannot be built (a blocked tile, a slope without a foundation) and, of the rest, where the pieces do not meet at one height. */
				std::vector<int> fits_ok, joins_ok;
				std::string fit_tiles, join_tiles;
				for (int c = std::max(0, least[pt]); c <= std::min(high, most[pt]); c++) {
					size_t bad_fit = count, bad_join = count;
					for (size_t i = 0; bad_fit == count && i < count; i++) {
						const int x = c + dx[i] + offset.x;
						/* Columns beyond the snapshot carry no facts (as in the blocked-tile facts above): a join with such a tile is not checked. */
						if (i + 1 < count && (x < facts->MinX() || x > facts->MaxX() || c + dx[i + 1] + offset.x < facts->MinX() || c + dx[i + 1] + offset.x > facts->MaxX())) continue;
						if (i + 1 == count || (dx[i] == dx[i + 1] && dy[i] == dy[i + 1])) {
							if (i >= 1 && i <= static_cast<size_t>(halves) && !facts->Fits(x, dy[i], bits[i])) bad_fit = i;
							continue;
						}
						/* The join to the next piece; EdgeZ is INT_MIN where the tracks do not fit. */
						const int z_out = facts->EdgeZ(x, dy[i], bits[i], exit[i]);
						const int z_in = facts->EdgeZ(c + dx[i + 1] + offset.x, dy[i + 1], bits[i + 1], ReverseDiagDir(exit[i]));
						if (z_out == INT_MIN || z_in == INT_MIN) {
							bad_fit = i;
						} else if (z_out != z_in && bad_join == count) {
							bad_join = i;
						}
					}
					const auto note = [&](std::string &text, size_t bad) {
						if (c + dx[bad] + offset.x >= facts->MinX() && c + dx[bad] + offset.x <= facts->MaxX() && text.size() < 40) text += fmt::format("{}({},{})", text.empty() ? "" : " ", c + dx[bad] + offset.x, dy[bad]);
					};
					if (bad_fit == count) fits_ok.push_back(c); else note(fit_tiles, bad_fit);
					if (bad_join == count) joins_ok.push_back(c); else if (bad_fit == count) note(join_tiles, bad_join);
				}
				/* A domain that keeps every column is no fact. */
				const size_t evaluated = static_cast<size_t>(std::max(0, std::min(high, most[pt]) + 1 - std::max(0, least[pt])));
				if (fits_ok.size() != evaluated) fact_cons.push_back({pt, -1, std::move(fits_ok), fact_reason(e, UINT32_MAX, row, fit_tiles.empty() ? std::string() : "[blocked tile " + fit_tiles + "]")});
				if (joins_ok.size() != evaluated) fact_cons.push_back({pt, -1, std::move(joins_ok), fact_reason(e, UINT32_MAX, row, "[height join " + join_tiles + "]", "height join")});
			};

			auto bend_heights = [&](int pt, int y, int heading, int hand, int halves, bool corner, uint32_t e, int row, uint32_t node_first = UINT32_MAX, uint32_t node_last = UINT32_MAX) {
				if (heights) pending.push_back([&bend_heights_now, pt, y, heading, hand, halves, corner, e, row, node_first, node_last] { bend_heights_now(pt, y, heading, hand, halves, corner, e, row, node_first, node_last); });
			};

			/* The chains of the edges that are not rows. */
			for (uint32_t e = 0; e < edge_count; e++) {
				if (is_row_edge(e)) continue;
				const std::vector<Elem> &elems = chain[e];
				for (size_t i = 0; i < elems.size(); i++) {
					const Elem &el = elems[i];
					if (el.k == Elem::K::Tile) {
						const uint32_t node = node_of_pt[el.first];
						if (node != UINT32_MAX) {
							group(el.first, 0, vy[el.first], JunctionFactKind::Switch, Axis::X, e, node, -1);
						} else {
							group(el.first, 0, vy[el.first], JunctionFactKind::Straight, VX[el.hin] != 0 ? Axis::X : Axis::Y, e, UINT32_MAX, -1);
						}
					} else if (el.k == Elem::K::Heads) {
						const Axis axis = VX[el.hin] != 0 ? Axis::X : Axis::Y;
						group(el.first, 0, vy[el.first], JunctionFactKind::Straight, axis, e, UINT32_MAX, -1);
						group(el.last, 0, vy[el.last], JunctionFactKind::Straight, axis, e, UINT32_MAX, -1);
					} else if (!(el.fan && el.m == 0)) {
						bend_heights(el.first, vy[el.first], el.hin, el.hand, static_cast<int>(el.offs.size()), el.k == Elem::K::Corner, e, -1, node_of_pt[el.first], node_of_pt[el.last]);
						for (size_t j = 0; j < el.offs.size(); j++) {
							const TileIndexDiffC &o = el.offs[j];
							const bool at_first = j == 0 && node_of_pt[el.first] != UINT32_MAX;
							const bool at_last = j + 1 == el.offs.size() && node_of_pt[el.last] != UINT32_MAX;
							const uint32_t node = at_first ? node_of_pt[el.first] : (at_last ? node_of_pt[el.last] : UINT32_MAX);
							group(el.first, o.x, vy[el.first] + o.y, node != UINT32_MAX ? JunctionFactKind::Switch : JunctionFactKind::Half, Axis::X, e, node, -1);
						}
					}
					if (i + 1 >= elems.size() || elems[i + 1].first == el.last) continue;
					const Elem &next = elems[i + 1];
					if (VX[el.hout] == 0) {
						const int d = VY[el.hout];
						for (int y = vy[el.last] + d; d > 0 ? y < vy[next.first] : y > vy[next.first]; y += d) group(el.last, 0, y, JunctionFactKind::Straight, Axis::Y, e, UINT32_MAX, -1);
						/* The run's tiles, and a port tile at either end, which is an axis piece like them. */
						int y_a = vy[el.last] + d;
						int y_b = vy[next.first] - d;
						if (d > 0 ? y_a <= y_b : y_a >= y_b) {
							if (el.k == Elem::K::Tile && node_of_pt[el.last] == UINT32_MAX) y_a = vy[el.last];
							if (next.k == Elem::K::Tile && node_of_pt[next.first] == UINT32_MAX) y_b = vy[next.first];
							column_run(el.last, y_a, y_b, e);
						}
					} else {
						row_run(el.last, next.first, vy[el.last], VX[el.hout], e, -1);
					}
				}
			}

			/* The rows' chains: in edge, leg (both copies of a doubled one), out edge. */
			for (int r = 0; r < rows; r++) {
				const int sgn = east[r] ? 1 : -1;
				const int heading = east[r] ? 1 : 3;
				for (const RowPiece &piece : row_pieces(r)) {
					switch (piece.t) {
						case RowPiece::T::Tile:
							if (piece.fact) group(piece.a, 0, piece.y, JunctionFactKind::Straight, Axis::X, piece.edge, UINT32_MAX, r);
							break;
						case RowPiece::T::Run: row_run(piece.a, piece.b, piece.y, sgn, piece.edge, r); break;
						case RowPiece::T::Bend:
							bend_heights(piece.a, piece.y, heading, piece.hand, piece.halves, false, piece.edge, r);
							for (const TileIndexDiffC &o : bend_tiles(heading, piece.hand, piece.halves, false)) group(piece.a, o.x, piece.y + o.y, JunctionFactKind::Half, Axis::X, piece.edge, UINT32_MAX, r);
							break;
					}
				}
				/* The prio element's tiles (RW6): a straight run on its row, the tile at either end and the run between them. */
				const Elem &prio_el = chain[leg_edge[r]].back();
				if (prio_el.k == Elem::K::Prio) {
					group(prio_el.first, 0, vy[prio_el.first], JunctionFactKind::Straight, Axis::X, leg_edge[r], UINT32_MAX, r);
					group(prio_el.last, 0, vy[prio_el.last], JunctionFactKind::Straight, Axis::X, leg_edge[r], UINT32_MAX, r);
					row_run(prio_el.first, prio_el.last, vy[prio_el.first], sgn, leg_edge[r], r);
				}
			}

			if (record != nullptr) {
				record->near_cons.clear();
				for (const Cons &cn : cons) {
					if (!cn.record_only) record->near_cons.push_back({cn.v, cn.u, cn.k});
				}
				record->near_pts = pt_count;
			}
			/* The columns each point can take: the least and the greatest solution of the relations (a longest path; x_v - x_u >= k) within the domains the facts above gave. */
			for (const FactCons &fc : fact_cons) {
				if (fc.u >= 0) continue;
				least[fc.v] = std::max(least[fc.v], fc.columns.empty() ? INT_MAX / 2 : fc.columns.front());
				most[fc.v] = std::min(most[fc.v], fc.columns.empty() ? -1 : fc.columns.back());
			}
			for (int round = 0; round <= pt_count; round++) {
				bool changed = false;
				for (const Cons &cn : cons) {
					if (cn.record_only) continue;
					if (least[cn.u] + cn.k > least[cn.v]) {
						least[cn.v] = least[cn.u] + cn.k;
						changed = true;
					}
					if (most[cn.v] - cn.k < most[cn.u]) {
						most[cn.u] = most[cn.v] - cn.k;
						changed = true;
					}
				}
				if (!changed) break;
			}
			if (heights) node_checks();
			for (const std::function<void()> &job : pending) job();
		}

		why("box bound");
		if (x_phase) {
			for (int p = 1; p < pt_count; p++) {
				if (p != xe_pt) ge(0, xe_pt, p, 0);
			}
		} else {
			for (int p = 1; p < pt_count; p++) {
				if (p != se_pt) ge(1, se_pt, p, 0);
			}
		}
		return true;
	};

	/* The port rows' offset: the one that minimises the largest fan (ties: the smaller). */
	auto best_offset = [&]() {
		int best = -1, best_o = 0;
		for (int o = 0; o <= vy[item_pt[items - 1]]; o++) {
			int worst = 0;
			for (uint32_t e = 0; e < edge_count; e++) {
				const uint32_t item = fan_item(e);
				if (item == NO_ROW) continue;
				worst = std::max(worst, std::abs(o + notation[edge_port(e)] - vy[item_pt[item]]));
			}
			if (best < 0 || worst < best) {
				best = worst;
				best_o = o;
			}
		}
		return best_o;
	};
	auto set_port_rows = [&](int offset) {
		for (int s = 0; s < n; s++) {
			if (notation[s] >= 0) port_row[s] = offset + notation[s];
		}
		for (uint32_t e = 0; e < edge_count; e++) {
			const uint32_t item = fan_item(e);
			if (item != NO_ROW) fan_m[e] = std::abs(port_row[edge_port(e)] - vy[item_pt[item]]);
		}
	};

	/* The Y-phase clearance of the X-axis bridges at a point (DESIGN-G3 §G3.6, R2): a head whose column lies strictly within an X-axis bridge keeps its side of the bridge's row by one; on the row itself, both sides (as today). */
	auto detect_y = [&](const std::vector<int> &x, const std::vector<int> &y) {
		clear_y.clear();
		std::vector<BridgeLine> lines;
		std::vector<HeadRef> heads;
		collect_bridges(lines, heads);
		for (const BridgeLine &line : lines) {
			if (!line.x_axis) continue;
			const int left = std::min(x[line.a], x[line.b]);
			const int right = std::max(x[line.a], x[line.b]);
			for (const HeadRef &head : heads) {
				if (head.bridge == line.bridge || x[head.xpt] <= left || x[head.xpt] >= right) continue;
				const int side = y[head.ypt] - y[line.line];
				/* Flat side 0: the head lies on the bridge's row inside its span, a contradiction of the family (both sides are demanded). */
				if (side == 0) {
					clear_y.push_back({head.ypt, line.line, line.edge, head.edge});
					clear_y.push_back({line.line, head.ypt, line.edge, head.edge});
					continue;
				}
				if (side > 0) clear_y.push_back({head.ypt, line.line, line.edge, head.edge}); else clear_y.push_back({line.line, head.ypt, line.edge, head.edge});
			}
		}
	};

	/* The Y-phase clearance of the X-axis bridges (D217): from the flat state, the Y and the X solve with no floors and only the relations, a head whose column lies strictly within an X-axis bridge keeps its flat side of the bridge's row by one. */
	auto plan_clearance = [&](int fixed_offset) {
		clear_y.clear();
		const std::vector<int> saved_vx = vx, saved_vy = vy, saved_port_row = port_row, saved_fan_m = fan_m;
		const int saved_phase = phase;
		floors_off = relations_only = skip_clearance = true;
		std::vector<int> flat_y, flat_x;
		phase = 1;
		refresh(false, measure(false));
		bool ok = build() && flat_solve(flat_y);
		if (ok) {
			vy = flat_y;
			set_port_rows(fixed_offset >= 0 ? fixed_offset : best_offset());
			phase = 0;
			refresh(true, measure(true));
			ok = build() && flat_solve(flat_x);
			if (ok) {
				vx = flat_x;
			}
		}
		floors_off = relations_only = skip_clearance = false;
		phase = saved_phase;
		vx = saved_vx;
		vy = saved_vy;
		port_row = saved_port_row;
		fan_m = saved_fan_m;
		if (!ok) return;
		detect_y(flat_x, flat_y);
	};
	auto build_failed = [&]() {
		if (occupancy_failed) return solve_failed();
		return fail("the placement rules are not complete for this topology " + build_why);
	};
	JunctionPlacementChoices choices;

	if (fixed != nullptr) {
		/* One Y solve and one X solve with the choices fixed (DESIGN-G1 §14.2). */
		for (uint32_t e = 0; e < edge_count; e++) {
			for (Elem &el : chain[e]) {
				if (el.free_m) el.m = el.fan ? fixed->m[e][el.tok] : std::max(1, fixed->m[e][el.tok]);
			}
		}
		if (at_mode && (at->x.size() != static_cast<size_t>(pt_count) || at->y.size() != static_cast<size_t>(pt_count))) return fail("the given point does not fit the placement points");
		rows_known = true;
		/* The box's first row bounds the port rows as it bounds every solved point. */
		for (int s = 0; s < n; s++) {
			if (notation[s] < 0 || fixed->port_offset + notation[s] >= 0) continue;
			failure.assign(1, JunctionPlacementReason{"box", UINT32_MAX, UINT32_MAX, fixed->port_offset + notation[s]});
			if (error != nullptr) *error = fmt::format("the placement rules contradict each other: box (port slot {} would lie on row {})", s, fixed->port_offset + notation[s]);
			return solve_failed();
		}
		bool y_solved = true; ///< Whether a recording at a point solved its Y rows (else they stand at the point).
		if (shared != nullptr && shared->ok) {
			/* The offset-independent stages were solved once by MakeJunctionPlacementShared: the Y solution is theirs. */
			phase = 1;
			refresh(false, measure(false));
			vy = shared->vy;
		} else {
			if (at_mode) {
				phase = 1;
				/* The clearance is detected at the given point; a failed build or solve is recorded and the Y rows stand at the point. */
				detect_y(at->x, at->y);
				refresh(false, measure(false));
				error->clear();
				build_why.clear();
				if (!build()) {
					at_failed.push_back("Y build: " + (error->empty() ? build_why : *error));
					vy = at->y;
					y_solved = false;
				} else if (!solve(vy)) {
					at_failed.push_back("Y solve: " + *error);
					vy = at->y;
					y_solved = false;
				}
			} else {
				plan_clearance(fixed->port_offset);
				phase = 1;
				refresh(false, measure(false));
				if (!build()) return build_failed();
				if (!solve(vy)) return solve_failed();
			}
		}
		if (make_shared != nullptr) {
			make_shared->vy = vy;
			make_shared->ok = true;
			return JunctionTrackPlacement{};
		}
		port_offset = fixed->port_offset;
		set_port_rows(port_offset);
		if (sizes != nullptr) {
			size_t bend_count = 0;
			for (uint32_t e = 0; e < edge_count; e++) {
				for (const std::vector<Elem> *elems : {&chain[e], &chain_copy[e]}) {
					for (const Elem &el : *elems) bend_count += el.k == Elem::K::Corner || el.k == Elem::K::SBend || el.k == Elem::K::Fan;
				}
			}
			if (sizes->m.size() != bend_count) return fail("the given sizes do not fit the bends");
		}
		phase = 0;
		refresh(true, sizes != nullptr ? *sizes : measure(true));
		if (at_mode) {
			error->clear();
			build_why.clear();
			if (!build()) {
				system->failed = at_failed;
				system->failed.push_back("X build: " + (error->empty() ? build_why : *error));
				return build_failed();
			}
			if (!solve(vx)) {
				at_failed.push_back("X solve: " + *error);
				system->failed = at_failed;
				return solve_failed();
			}
			system->failed = at_failed;
			if (y_solved) system->solved = {vx, vy};
		} else {
			if (!build()) return build_failed();
			if (record != nullptr) return JunctionTrackPlacement{};
			if (!solve(vx)) return solve_failed();
			if (system != nullptr) system->solved = {vx, vy};
		}
		choices = *fixed;
	} else {
		/* The rounds: a first X pass without rows gives the S-bends' sizes, then Y and X alternate until the sizes hold. */
		phase = 0;
		refresh(false, measure(false));
		if (!build()) return build_failed();
		if (!solve(vx)) return solve_failed();
		for (uint32_t e = 0; e < edge_count; e++) {
			for (Elem &el : chain[e]) {
				if (el.free_m) el.m = el.fan ? std::abs(vx[el.last] - vx[el.first]) : std::max(1, std::abs(vx[el.last] - vx[el.first]));
			}
		}
		rows_known = true;
		for (int round = 0; round < 5; round++) {
			plan_clearance(-1);
			phase = 1;
			refresh(false, measure(false));
			if (!build()) return build_failed();
			if (!solve(vy)) return solve_failed();

			/* The port rows. */
			port_offset = best_offset();
			set_port_rows(port_offset);

			phase = 0;
			refresh(true, measure(true));
			if (!build()) return build_failed();
			if (!solve(vx)) return solve_failed();

			bool changed = false;
			for (uint32_t e = 0; e < edge_count; e++) {
				for (Elem &el : chain[e]) {
					if (!el.free_m) continue;
					const int m = std::abs(vx[el.last] - vx[el.first]);
					if (m != el.m) {
						el.m = m;
						changed = true;
					}
				}
			}
			if (!changed) break;
			if (round == 4) return fail("the placement does not settle");
		}

		/* The choices the rounds settled on. */
		choices.port_offset = port_offset;
		choices.m.resize(edge_count);
		choices.corner.resize(edge_count);
		for (uint32_t e = 0; e < edge_count; e++) {
			choices.m[e].assign(programs.edges[e].size(), 0);
			choices.corner[e].assign(programs.edges[e].size(), 0);
			for (const Elem &el : chain[e]) {
				if (el.free_m) choices.m[e][el.tok] = el.m;
				if (el.joiner_div) choices.corner[e][el.tok] = vy[el.first] - vy[el.last];
			}
		}
	}

	/* The legality of the settled placement (the rounds' states may overlap unrelated pieces while the S-bend sizes settle). */
	if (rows_known && !relations_only) {
		compute_node_tracks();
		if (!add_occupancy(vx, true)) return solve_failed();
	}

	/* The fans' and ports' rows. */
	for (int s = 0; s < n; s++) {
		if (notation[s] >= 0) vy[port_pt[s]] = port_row[s];
	}
	/* A fan's two ends: the port row and its item's row. */
	for (uint32_t e = 0; e < edge_count; e++) {
		for (const Elem &el : chain[e]) {
			if (el.k != Elem::K::Fan) continue;
			vy[el.port_first ? el.first : el.last] = port_row[el.slot];
			vy[el.port_first ? el.last : el.first] = vy[item_pt[el.index]];
		}
	}
	auto tile = [&](int pt) { return TileIndexDiffC{static_cast<int16_t>(vx[pt]), static_cast<int16_t>(vy[pt])}; };

	JunctionTrackPlacement result;
	for (int i = 0; i < items; i++) result.item_rows.push_back(vy[item_pt[i]]);
	for (int s = 0; s < n; s++) result.ports.push_back(tile(port_pt[s]));
	for (uint32_t i = 0; i < graph.nodes.size(); i++) result.nodes.push_back(tile(node_pt[i]));
	result.size = {static_cast<int16_t>(vx[xe_pt] + 1), static_cast<int16_t>(vy[se_pt] + 1)};
	result.choices = choices;
	result.columns.assign(vx.begin(), vx.end());
	/* The prio elements (RW6b): per element, its tiles and drawn bits, the one source of occupancy, the signal pass and the level solve. */
	for (int r = 0; r < rows; r++) {
		const Elem &prio_el = chain[leg_edge[r]].back();
		if (prio_el.k != Elem::K::Prio) continue;
		const int sgn = east[r] ? 1 : -1;
		const DiagDirection side = prio_el.index < row_items[r].lane ? DiagDirection::NW : DiagDirection::SE;
		const DiagDirection exit_edge = east[r] ? DiagDirection::SW : DiagDirection::NE;
		JunctionPrioSpan span;
		span.merge = {static_cast<int16_t>(vx[prio_el.last]), static_cast<int16_t>(vy[item_pt[row_items[r].lane]])};
		for (int step = 0; step <= even_prio; step++) {
			const auto [row_bits, lane_bits] = JunctionPrioStepBits(step, side, exit_edge);
			const int x = vx[prio_el.last] - step * sgn;
			if (lane_bits.Any()) span.tiles.emplace_back(TileIndexDiffC{static_cast<int16_t>(x), span.merge.y}, lane_bits);
			span.tiles.emplace_back(TileIndexDiffC{static_cast<int16_t>(x), static_cast<int16_t>(vy[prio_el.first])}, row_bits);
		}
		result.prio.push_back(std::move(span));
	}

	/* The heads of a bridge: its near and far point, and the carrier's travel direction along the axis. */
	struct Heads {
		int near;
		int far;
		bool row; ///< On a row (else in a column).
	};
	auto heads_of = [&](int b, int copy) {
		const uint32_t edge = structure.bridges[b].edge;
		if (structure.bridges[b].doubled) {
			const size_t k = std::find(bridges_of[edge].begin(), bridges_of[edge].end(), b) - bridges_of[edge].begin();
			const Elem &el = nth_heads(copy == 0 ? chain[edge] : chain_copy[edge], k);
			return Heads{el.first, el.last, true};
		}
		for (const Elem &el : chain[edge]) {
			if (el.k == Elem::K::Heads && el.cross == static_cast<int>(structure.bridges[b].crossed[0])) return Heads{el.first, el.last, false};
		}
		return Heads{0, 0, false};
	};
	/* The bridge of an edge whose crossed set holds another edge, or -1. */
	auto bridge_over = [&](uint32_t edge, uint32_t crossed) {
		for (int b : bridges_of[edge]) {
			const std::vector<uint32_t> &set = structure.bridges[b].crossed;
			if (std::find(set.begin(), set.end(), crossed) != set.end()) return b;
		}
		return -1;
	};
	/* The tiles of an edge strictly between a bridge's heads on a row (or in a column), in the edge's order. */
	auto tiles_under = [&](uint32_t edge, const Heads &heads, int line) {
		std::vector<TileIndexDiffC> found;
		for (const SymTile &t : edge_tiles(edge)) {
			const int x = vx[t.pt] + t.dx;
			if (heads.row) {
				if (t.y != line) continue;
				if (x > std::min(vx[heads.near], vx[heads.far]) && x < std::max(vx[heads.near], vx[heads.far])) found.push_back({static_cast<int16_t>(x), static_cast<int16_t>(t.y)});
			} else {
				if (x != line) continue;
				if (t.y > std::min(vy[heads.near], vy[heads.far]) && t.y < std::max(vy[heads.near], vy[heads.far])) found.push_back({static_cast<int16_t>(x), static_cast<int16_t>(t.y)});
			}
		}
		return found;
	};

	result.edges.resize(edge_count);
	result.copies.resize(edge_count);
	for (uint32_t e = 0; e < edge_count; e++) {
		const std::vector<Step> &program = programs.edges[e];
		if (is_row_edge(e) && doubled_edge(e)) {
			/* A doubled leg: each copy's S-bend spans its chain's S-bend, a bridge the crosser's tiles between that copy's heads. */
			const int s = east[(end_level(graph.edges[e].from) - 1) / 2] ? 1 : -1;
			for (int copy = 0; copy < 2; copy++) {
				const std::vector<Step> &tokens = copy == 0 ? program : programs.copies[e];
				for (size_t t = 0; t < tokens.size(); t++) {
					const Step &step = tokens[t];
					if (step.kind == StepKind::SBend) {
						const std::vector<Elem> &elems = copy == 0 ? chain[e] : chain_copy[e];
						const Elem &bend = *std::ranges::find_if(elems, [&](const Elem &el) { return el.k == Elem::K::SBend && el.tok == static_cast<int>(t); });
						(copy == 0 ? result.edges[e] : result.copies[e]).push_back({tile(bend.first), tile(bend.last)});
					} else {
						const int over = bridge_over(e, step.index);
						if (over < 0) return fail("a crossing edge misses its bridge");
						const Heads heads = heads_of(over, copy);
						std::vector<TileIndexDiffC> found = tiles_under(step.index, heads, vy[heads.near]);
						if (found.empty()) return fail("a crossing edge misses its bridge");
						std::stable_sort(found.begin(), found.end(), [&](const TileIndexDiffC &a, const TileIndexDiffC &b) { return s > 0 ? a.x < b.x : a.x > b.x; });
						(copy == 0 ? result.edges[e] : result.copies[e]).push_back({found.front(), found.back()});
					}
				}
			}
			continue;
		}
		const std::vector<Elem> &elems = chain[e];
		for (size_t t = 0; t < program.size(); t++) {
			const Step &step = program[t];
			if (step.kind == StepKind::Fan) {
				for (const Elem &el : elems) {
					if (el.tok == static_cast<int>(t)) result.edges[e].push_back({tile(el.first), tile(el.last)});
				}
			} else if (step.kind == StepKind::Corner || step.kind == StepKind::SBend) {
				for (const Elem &el : elems) {
					if (el.tok == static_cast<int>(t)) {
						const TileIndexDiffC first = tile(el.first);
						const TileIndexDiffC last = {static_cast<int16_t>(vx[el.first] + el.offs.back().x), static_cast<int16_t>(vy[el.first] + el.offs.back().y)};
						result.edges[e].push_back({first, last});
					}
				}
			} else if (step.kind == StepKind::Over || step.kind == StepKind::Tunnel) {
				if (bridges_of[e].empty()) return fail("a crossing edge misses its bridge");
				const auto over_it = std::ranges::find_if(bridges_of[e], [&](int b) { return structure.bridges[b].crossed[0] == step.index; });
				if (over_it == bridges_of[e].end()) return fail("a crossing edge misses its bridge");
				const Heads heads = heads_of(*over_it, 0);
				std::vector<TileIndexDiffC> found = tiles_under(step.index, heads, vx[heads.near]);
				if (found.empty()) return fail("a crossing edge misses its bridge");
				const bool north = vy[heads.far] < vy[heads.near];
				std::stable_sort(found.begin(), found.end(), [&](const TileIndexDiffC &a, const TileIndexDiffC &b) { return north ? a.y > b.y : a.y < b.y; });
				result.edges[e].push_back({found.front(), found.back()});
			} else {
				/* Under, Above: this edge's tiles beneath the overhead edge's bridge. */
				const uint32_t x = step.index;
				const int bx = bridge_over(x, e);
				if (bx < 0) return fail("a crossing edge misses its bridge");
				std::vector<TileIndexDiffC> found;
				if (structure.bridges[bx].doubled) {
					const Heads heads = heads_of(bx, step.copy);
					found = tiles_under(e, heads, vy[heads.near]);
				} else {
					const Heads heads = heads_of(bx, 0);
					found = tiles_under(e, heads, vx[heads.near]);
				}
				if (found.empty()) return fail("a crossing edge misses its bridge");
				result.edges[e].push_back({found.front(), found.back()});
			}
		}
	}

	/* The bridges. */
	for (size_t b = 0; b < structure.bridges.size(); b++) {
		const JunctionTrackBridge &bridge = structure.bridges[b];
		std::array<JunctionBridge, 2> heads{};
		for (int copy = 0; copy < (bridge.doubled ? 2 : 1); copy++) {
			const Heads hd = heads_of(static_cast<int>(b), copy);
			heads[copy] = JunctionBridge{tile(hd.near), tile(hd.far)};
			if (bridge.doubled) {
				const int middle = std::abs(vx[hd.near] - vx[hd.far]) - 1;
				if (copy == 0 && middle > spec.max_bridge_middle) return fail(fmt::format("bridge {} on edge {} needs {} middle tiles, more than {}", b, bridge.edge, middle, spec.max_bridge_middle));
			}
		}
		if (bridge.tunnel) {
			/* A tunnel's length is capped by the game's max_tunnel_length (D220), not by the bridge span. */
			const int middle = std::abs(heads[0].start.x - heads[0].end.x) + std::abs(heads[0].start.y - heads[0].end.y) - 1;
			if (middle > spec.max_tunnel_length) return fail(fmt::format("tunnel {} on edge {} needs {} middle tiles, more than {}", b, bridge.edge, middle, spec.max_tunnel_length));
		}
		result.bridges.push_back(heads);
	}
	/* A bridge middle never spans a Blocked tile (D175, HH2); tunnels pass under anything. Each head pair lies on one row or column. */
	if (facts != nullptr) {
		for (size_t b = 0; b < structure.bridges.size(); b++) {
			if (structure.bridges[b].tunnel) continue;
			for (int copy = 0; copy < (structure.bridges[b].doubled ? 2 : 1); copy++) {
				const JunctionBridge &span = result.bridges[b][copy];
				const int sx = (span.end.x > span.start.x) - (span.end.x < span.start.x);
				const int sy = (span.end.y > span.start.y) - (span.end.y < span.start.y);
				for (int x = span.start.x + sx, y = span.start.y + sy; x != span.end.x || y != span.end.y; x += sx, y += sy) {
					if (facts->Spannable(x + offset.x, y + offset.y)) continue;
					/* As a house under a track tile reports: the blocked tile first, then the bridge that spans it, each naming its edge. */
					const uint32_t edge = structure.bridges[b].edge;
					JunctionTrackPlacement refused;
					refused.why = {JunctionPlacementReason{"blocked tile", edge}, JunctionPlacementReason{"bridge span over a blocked tile", edge}};
					if (error != nullptr) *error = fmt::format("the placement rules contradict each other: blocked tile (edge {}) [blocked tile ({},{})] <- bridge span over a blocked tile (edge {})", edge, x, y, edge);
					return refused;
				}
			}
		}
	}

	/* The curves at the solved columns (for tests): paths first, then the copies in path order. */
	std::vector<SymRoute> sym;
	if (!sym_routes(sym, true)) return fail("the curves of the routes are not ruled yet " + build_why);
	for (int copy = 0; copy < 2; copy++) {
		for (const SymRoute &route : sym) {
			if (route.copy != copy) continue;
			std::vector<std::pair<int8_t, int>> curves;
			for (const SymCurve &curve : route.curves) curves.emplace_back(curve.hand == JunctionCurveHand::Right ? 1 : -1, curve.xoff + 2 * route.sign * (vx[curve.pt] - vx[route.start]));
			result.curves.push_back(std::move(curves));
		}
	}
	return result;
}

JunctionTrackPlacement PlaceJunctionTrackGraph(const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, const JunctionTrackOrder &order, const JunctionTrackStructure &structure, const JunctionTrackPrograms &programs, const JunctionSpec &spec, std::string *error)
{
	return PlaceTrackGraph(topology, paths, graph, order, structure, programs, spec, nullptr, error, {}, {});
}

JunctionTrackPlacement PlaceJunctionTrackGraph(const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, const JunctionTrackOrder &order, const JunctionTrackStructure &structure, const JunctionTrackPrograms &programs, const JunctionSpec &spec, const JunctionPlacementChoices &choices, std::string *error, const std::vector<std::pair<int, int>> &x_floors, const std::vector<std::pair<int, int>> &y_floors)
{
	return PlaceTrackGraph(topology, paths, graph, order, structure, programs, spec, &choices, error, x_floors, y_floors);
}

JunctionPlacementSystem RecordJunctionPlacementSystem(const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, const JunctionTrackOrder &order, const JunctionTrackStructure &structure, const JunctionTrackPrograms &programs, const JunctionSpec &spec, const JunctionPlacementChoices &choices, const JunctionBendSizes *sizes, const JunctionSystemPoint *at)
{
	JunctionPlacementSystem system;
	PlaceTrackGraph(topology, paths, graph, order, structure, programs, spec, &choices, nullptr, {}, {}, nullptr, {0, 0}, true, false, nullptr, nullptr, nullptr, &system, sizes, at);
	return system;
}


JunctionPlacementShared MakeJunctionPlacementShared(const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, const JunctionTrackOrder &order, const JunctionTrackStructure &structure, const JunctionTrackPrograms &programs, const JunctionSpec &spec, const JunctionPlacementChoices &choices)
{
	JunctionPlacementShared shared;
	PlaceTrackGraph(topology, paths, graph, order, structure, programs, spec, &choices, nullptr, {}, {}, nullptr, {0, 0}, true, false, nullptr, &shared);
	if (!shared.ok) return shared;
	/* Record the shape of the X phase with all-buildable facts (the groups, runs and relations do not depend on the terrain or the offset): the near search's early reject reads it. */
	JunctionTerrain flat;
	flat.origin = {0, 0};
	flat.width = 512;
	flat.height = 128;
	flat.tiles.assign(static_cast<size_t>(flat.width) * flat.height, JunctionTerrainTile{});
	const JunctionFacts dummy(flat);
	shared.near_groups.clear();
	shared.near_runs.clear();
	shared.near_cons.clear();
	std::string recorded_error;
	PlaceTrackGraph(topology, paths, graph, order, structure, programs, spec, &choices, &recorded_error, {}, {}, &dummy, {64, 8}, false, true, &shared, nullptr, &shared);
	if (shared.near_pts > 0) {
		shared.near_lo.assign(shared.near_pts, 0);
		for (int round = 0; round <= shared.near_pts; round++) {
			bool changed = false;
			for (const auto &c : shared.near_cons) {
				if (shared.near_lo[c[1]] + c[2] > shared.near_lo[c[0]]) {
					shared.near_lo[c[0]] = shared.near_lo[c[1]] + c[2];
					changed = true;
				}
			}
			if (!changed) break;
		}
		shared.near_ok = true;
	}
	return shared;
}

bool JunctionPlacementNearReject(const JunctionPlacementShared &shared, const JunctionFacts &facts, TileIndexDiffC offset)
{
	if (!shared.near_ok) return false;
	const int pts = shared.near_pts;
	const int high = facts.MaxX() - offset.x;
	constexpr int INF = INT_MAX / 4;
	std::vector<int> lo = shared.near_lo;
	std::vector<int> hi(pts, INF);
	auto next = [&](const std::array<int, 3> &g, int c) {
		for (; c <= high; c++) {
			if (facts.Buildable(c + g[1] + offset.x, g[2] + offset.y)) return c;
		}
		return INF;
	};
	auto prev = [&](const std::array<int, 3> &g, int c) {
		for (c = std::min(c, high); c >= 0; c--) {
			if (facts.Buildable(c + g[1] + offset.x, g[2] + offset.y)) return c;
		}
		return -1;
	};
	/* The blocked columns (relative to the offset) of each run's row. */
	std::map<int, std::vector<int>> blocked_of_row;
	std::vector<const std::vector<int> *> blocked;
	for (const auto &r : shared.near_runs) {
		auto found = blocked_of_row.find(r[2]);
		if (found == blocked_of_row.end()) {
			std::vector<int> list;
			for (int x = facts.MinX(); x <= facts.MaxX(); x++) {
				if (!facts.Buildable(x, r[2] + offset.y)) list.push_back(x - offset.x);
			}
			found = blocked_of_row.emplace(r[2], std::move(list)).first;
		}
		blocked.push_back(&found->second);
	}
	for (const auto &g : shared.near_groups) {
		const int c = next(g, std::max(0, lo[g[0]]));
		if (c == INF) return true;
		lo[g[0]] = c;
	}
	for (int round = 0; round <= pts * 4; round++) {
		bool changed = false;
		for (const auto &c : shared.near_cons) {
			if (lo[c[1]] + c[2] > lo[c[0]]) { lo[c[0]] = lo[c[1]] + c[2]; changed = true; }
			if (hi[c[0]] < INF && hi[c[0]] - c[2] < hi[c[1]]) { hi[c[1]] = hi[c[0]] - c[2]; changed = true; }
		}
		for (const auto &g : shared.near_groups) {
			const int p = g[0];
			if (lo[p] > high || lo[p] > hi[p]) return true;
			const int c = next(g, lo[p]);
			if (c == INF) return true;
			if (c != lo[p]) { lo[p] = c; changed = true; }
			const int d = prev(g, hi[p]);
			if (d < lo[p]) return true;
			if (d != hi[p]) { hi[p] = d; changed = true; }
		}
		for (size_t i = 0; i < shared.near_runs.size(); i++) {
			const auto &r = shared.near_runs[i];
			const int west = r[3] > 0 ? r[0] : r[1];
			const int east = r[3] > 0 ? r[1] : r[0];
			for (const int c : *blocked[i]) {
				/* Not (x_west <= c <= x_east). */
				if (lo[east] >= c && lo[west] < c + 1) { lo[west] = c + 1; changed = true; }
				if (hi[west] <= c && hi[east] > c - 1) { hi[east] = c - 1; changed = true; }
			}
			if (lo[west] > hi[west] || lo[east] > hi[east]) return true;
		}
		if (!changed) break;
	}
	return false;
}
