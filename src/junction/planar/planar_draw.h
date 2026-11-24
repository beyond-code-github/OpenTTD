/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file planar_draw.h The drawing program: Shape's shape as an integer program, a feasibility oracle (E5, D388 (b)). */

#ifndef PLANAR_DRAW_H
#define PLANAR_DRAW_H

#include "planar_shape_layer.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/** One keep-apart pair of the pairwise block as written (D428 read-out): its skip reason, or its candidates with the rows each writes. */
struct PlanarKeepApartPair {
	/**
	 * One row of a candidate: s . (q - p) >= spacing, p and q given as (track, point index).
	 * A wall the face cuts at a crossing ends there (D431): the end is then the crossing point, named by p_cross / q_cross as (track a, piece of a, track b, piece of b) of the crossing, UINT32_MAX when the end is the piece's point.
	 */
	struct Row {
		std::array<uint32_t, 2> p;
		std::array<uint32_t, 2> q;
		std::array<uint32_t, 4> p_cross = {UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX};
		std::array<uint32_t, 4> q_cross = {UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX};
	};
	/** One side of one axis; the pair holds when every row of one candidate holds. */
	struct Candidate {
		std::array<int, 2> s;
		int spacing;
		std::vector<Row> rows;
	};
	std::string tag; ///< "sep <face> <i> <j>".
	std::array<uint32_t, 2> tracks; ///< Wall i's track, wall j's track.
	std::array<uint32_t, 2> pieces; ///< Wall i's piece, wall j's piece.
	std::string skipped; ///< Empty when rows were written, else the skip reason (the dump's wording).
	std::string kept; ///< When a strict rule kept a pair the plain rules skip: "strict span", or the strict_zero text the dump writes to `kept`; else empty.
	std::vector<Candidate> candidates;
	int walk = -1; ///< With keep_apart_out: the candidate (axis * 2 + side) the sides_by_walk rule forces, or -1. Informational.
	int walk_strict = -1; ///< With keep_apart_out: the candidate walk_strict (D417) forces, or -1.
	int facing = -1; ///< With keep_apart_out: the candidate facing_sides (D426) forces (anti-parallel facing walls: 0), or -1; facing walls at another angle choose between the two inward candidates on a reference and force none.
	uint32_t inward = 0; ///< With keep_apart_out: bit (axis * 2 + side) set for each candidate inward_sides allows.
};

/** The end a row names on given points (as PlanarDrawResult::points): the row's p (q false) or q (q true), the crossing point where the row names a cut end. */
std::array<int, 2> PlanarKeepApartEnd(const PlanarKeepApartPair::Row &row, bool q, const std::vector<std::vector<std::array<int, 2>>> &points);

/** Whether a pair holds on given points (as PlanarDrawResult::points): it is skipped, or every row of one of its candidates holds. */
bool PlanarKeepApartHolds(const PlanarKeepApartPair &pair, const std::vector<std::vector<std::array<int, 2>>> &points);

/** A row on fixed points: sum over terms of c . point(track, index) >= least. */
struct PlanarPointRow {
	struct Term {
		std::array<uint32_t, 2> point; ///< (track, point index).
		std::array<int, 2> c;
	};
	std::string tag;
	std::vector<Term> terms;
	int least;
};

/** Whether a point row holds on given points (as PlanarDrawResult::points). */
bool PlanarPointRowHolds(const PlanarPointRow &row, const std::vector<std::vector<std::array<int, 2>>> &points);

/**
 * The node lead rows (rule A, D400, D429, D430): if a leg's S-bend toward its sibling leg has length, the leg travels from the node before it.
 * Per track that is a leg of a split at its start: walk its flex middles m ascending while m = 1 + 2k; an insert m steps to side d = sign(pieces[m] - pieces[m - 1]) (+1 right of travel); every insert with d on the sibling side (site.side for the straight leg, its negation for the other, either side at a fork or a node with no site) gets a row when the insert's heading (mod 8) equals a sibling leg's heading at the node (its first piece at a split, its last at a merge; it would run over it), kept as is at a fork or a node with no site. With h0 = pieces[0], T = STEP[h0] . (point (t, m) - point (t, 0)) the travel from the node to the insert along the end heading and I = STEP[pieces[m]] . (point (t, m + 1) - point (t, m)) the insert length (>= 0), the row is `64 * T - I >= 0`: with integer points, I > 0 forces T >= 1 (C = 64). A diagonal end heading gives T >= 1 per half-tile unit, so the guarantee there is half a piece (the suite measures whether that matters). A leg of a merge at its end: the mirror, m descending while m = n - 2 - 2k, d = sign(pieces[m] - pieces[m + 1]), T = STEP[pieces[n - 1]] . (point (t, n) - point (t, m + 1)), I the same insert length (pieces run forward, so I >= 0 for a used insert). On contracted lines (D432) the S-bends are `steps`; a toward step at the node with no node-side flank is rejected. Trunks (a split's in line, a merge's out line) get none. Tag `lead <track name> start|end <m>`. D412 exemption: a track that starts at a port, ends at a node and has no events (its stretch from the port to its first node may have total length 0) gets none; the program's own port_stretch is the same (D412 covers only an in-port line).
 */
std::vector<PlanarPointRow> PlanarNodeLeadRows(const PlanarShapeVariant &v);

/** A cusp at a topology node (D451): a non-straight Line leg at the trunk heading, and its node-end S-bend insert on the turned heading. */
struct PlanarNodeCusp {
	uint32_t track = 0; ///< The leg.
	uint32_t node = 0; ///< The site's node.
	uint32_t insert = UINT32_MAX; ///< The insert's piece, UINT32_MAX when missing.
	std::vector<uint32_t> held; ///< The pieces between the insert and the node, held at 0.
	std::string missing; ///< Why there is no insert (empty when there is one).
};

/** Every cusp of the variant's sites, in site order (D451). */
std::vector<PlanarNodeCusp> PlanarNodeCusps(const PlanarShapeVariant &v);

struct PlanarProgramPlan;

/** The drawing program as written (D428 read-out): its variables and rows, and which variables are the track points. */
struct PlanarProgram {
	struct Row {
		std::vector<std::pair<uint32_t, double>> terms;
		double lower;
		double upper;
		std::string tag;
	};
	std::vector<double> lower; ///< Per variable.
	std::vector<double> upper;
	std::vector<bool> integer;
	std::vector<std::string> group; ///< Per variable, its `var` group ("point", "param", "cross", "other", ...); lengths as their group.
	std::vector<Row> rows;
	std::vector<std::vector<std::array<uint32_t, 2>>> point; ///< Per track and point, its x and y variable.
	mutable std::shared_ptr<PlanarProgramPlan> plan; ///< The elimination order, built by the first CheckPlanarProgramOnPoints call (it depends only on which variables are points); a program edited after that call must reset it.
};

/** What a program says about fixed points. */
struct PlanarProgramCheck {
	std::vector<std::string> broken; ///< Tags of rows that fail (each once, in row order), and "integer <group>" for a determined integer variable off an integer.
	std::vector<std::string> undetermined; ///< Families (tag up to the first space) of rows skipped because a variable stayed unknown, each once.
};

/** Fix the points, solve the variables the equality rows determine, and check every row whose variables are all known (tolerance 1e-6). */
PlanarProgramCheck CheckPlanarProgramOnPoints(const PlanarProgram &program, const std::vector<std::vector<std::array<int, 2>>> &points);

/** The same with only some tracks placed: only the steps and rows whose track sets (the tracks of the points that reach them) lie within `placed` (per track) are evaluated, the rest are skipped silently. The form above is this with every track placed. */
PlanarProgramCheck CheckPlanarProgramOnPoints(const PlanarProgram &program, const std::vector<std::vector<std::array<int, 2>>> &points, const std::vector<bool> &placed);

/** The result of the drawing program. */
struct PlanarDrawResult {
	bool feasible = false; ///< A drawing exists.
	std::string status; ///< "Optimal", "Infeasible", "Error", or "unsupported: <why>" for an input the program does not cover.
	std::vector<std::string> conflict; ///< When infeasible, the tags of the conflicting rows (InfeasibleRows), each once.
	double slack = 0; ///< With elastic: the total slack of the solution.
	std::vector<std::pair<std::string, double>> stretched; ///< With elastic: per row tag whose slack is above 1e-6, the slack, largest first (a tag that names several rows sums them).
	std::vector<std::pair<std::string, std::string>> sep_sides; ///< A read-out: per written sep row, its tag and the chosen candidate (axis and side, as text).
	std::vector<std::pair<std::string, std::string>> sep_margins; ///< A read-out (margin_tags): per pair tag, each candidate as "c<k> s(x,y) spacing <n> from i|j margin <m>", joined by "; ".
	uint32_t rows = 0; ///< The program's rows.
	size_t flank_rows = 0; ///< With flank_turn: the flank rows written.
	size_t cusps_forced = 0; ///< With node_cusp: the cusps whose insert was forced.
	std::vector<std::string> cusps_missing; ///< With node_cusp: "track node: reason" per cusp with no insert.
	uint32_t columns = 0; ///< The program's variables.
	size_t grid_forced = 0; ///< With grid: the variables whose parity the integer equalities force (written 2 h + p).
	size_t grid_tightened = 0; ///< With grid: the rows whose bounds rounding inward moved, once divided by their coefficients' gcd.
	size_t grid_conflicts = 0; ///< With grid: integer equalities whose parity the forced parities break (the grid is then infeasible).
	uint32_t pairs = 0; ///< Wall pairs separated (pairwise mode).
	uint32_t binaries = 0; ///< Their axis-side binaries.
	uint32_t short_sides = 0; ///< With sides_from: the pairs whose chosen side the reference keeps by less than its spacing.
	uint32_t sides_free = 0; ///< With sides_from and sides_free_below: pairs left with binaries.
	uint32_t walk_fixed = 0; ///< With sides_by_walk: pairs the rule fixed.
	uint32_t lane_pairs = 0; ///< With sibling_lanes: the sibling pairs the lane rows were written for.
	uint32_t lane_sep_skipped = 0; ///< With sibling_lanes: wall pairs the keep-apart skipped inside a used sibling pair.
	std::vector<std::string> lane_skipped; ///< With sibling_lanes: "<a> <b>: <reason>" per sibling pair the rule skipped.
	uint32_t walk_short = 0; ///< With sides_by_walk: fixed pairs whose rule candidate the sides_from reference keeps by less than its spacing.
	uint32_t facing_fixed = 0; ///< With facing_sides: anti-parallel facing pairs fixed.
	uint32_t facing_choice = 0; ///< With facing_sides: facing pairs at another angle.
	uint32_t inward_short = 0; ///< With inward_sides and sides_from: pairs whose best inward candidate's margin on the reference is negative.
	std::vector<std::string> inward_short_tags; ///< The first 40 such tags, each with ` best inward margin <m>, best overall margin <m'>`.
	uint32_t walk_differs = 0; ///< With sides_by_walk: fixed pairs where the rule's candidate is not sides_from's best.
	double ms = 0; ///< Build and solve time in milliseconds.
	std::vector<std::vector<std::array<int, 2>>> points; ///< When feasible, per track its points (start, then the end of each piece), half tiles, x east and y south.
	size_t start_values = 0; ///< With start_lengths or start_points: how many variables the MIP start gave.
	std::vector<std::vector<double>> lengths; ///< When feasible, per track each piece's length in half-tile units along its heading (unrounded, so an LP's fractional lengths show).
	std::vector<PlanarKeepApartPair> keep_apart; ///< With keep_apart_out: every pair the pairwise block considers, in order.
	PlanarProgram program; ///< With program_out: the program as written.
};

/** Points of an answer key (D393): positions the drawing must keep within `window` half tiles, up to one common shift. */
struct PlanarDrawPins {
	struct Pin {
		uint32_t track; ///< The track (end pins) or the crossing's index in map.crossings (cross pins).
		bool cross; ///< A crossing pin.
		bool start; ///< For an end pin: the track's first point, else its last.
		std::array<int, 2> at; ///< Half tiles, map frame.
	};
	std::vector<Pin> pins;
	int window = 2; ///< Half tiles, per axis.
	std::vector<std::string> unmatched; ///< Lines of the text that named no track or crossing.
};

/** Read pins (format in docs/junction-tool/briefs/E5-FSh-pins.md section 2) against a variant's map; unknown names go to `unmatched`. */
PlanarDrawPins ParsePlanarDrawPins(const PlanarShapeVariant &v, std::string_view text);

/** What the drawing program writes. */
struct PlanarDrawOptions {
	double coord_max = 4096; ///< The largest coordinate, length and parameter (half tiles); the pairwise big-M is 4 x this. A tighter bound tightens the binary rows (a measurement aid for exact solves).
	bool clearance = true; ///< Write the dummies, their landings and the clearance rows; false leaves them out (a variant with prio rows or copies needs the corridor family first, CARD-E5).
	bool clear_rows = true; ///< With clearance: write the clearance rows; false keeps the dummies and their landings but leaves the clearance rows out (a measurement of the landings alone).
	bool pairwise = false; ///< With clearance (D406 probe): no dummies and no landings; a face convex without dummies keeps the clearance rows, every other face gets pairwise separation of its walls (`sep`).
	bool pairwise_all_faces = false; ///< With pairwise: convex faces get pairwise separation like every other face instead of the cell clearance rows.
	bool sep_extremal = false; ///< With pairwise and a fixed side (a measurement): one row per pair, between the endpoints whose projections on the side's axis can bind (fixed headings, lengths >= 0), instead of all four endpoint pairs; exact.
	bool bays = true; ///< On by default since D420 (agrees with V's PSG 197); the bay rows (SPEC-slots-lengths §8.1, D420): on every non-prio route path, the travel from its last topology split or port to each topology merge is at least rules.bay.
	bool coverage = false; ///< A probe (D384): per Row track, its travel along its path (half tiles, as the bay rows) is exactly rules.coverage (tag `coverage <row>`).
	bool curves = true; ///< On by default since D420 (agrees with V's PSG 197); the curve spacing rows (SPEC-slots-lengths §8.1, §8.2, D420): Shape's bends along every route path keep curve_same, curve_opposite and curve_window; flex and step inserts count as travel, not bends (§10.3), except corridor steps and committed flex inserts (below).
	bool end_node = true; ///< On by default (D435: the domain suite exact on X1, X2, M1, K1, L1, Y1, P1, BR1); false is the old behaviour. A structure's end run stays off a node's switch tile (tag `end <s> node`): the travel from a node at the separator's start to its first crossing in the structure is at least the end run plus one tile; mirrored at its end.
	bool cross_piece = true; ///< On by default (D435: the domain suite exact on X1, X2, M1, K1, L1, Y1, P1, BR1); false is the old behaviour. Per crossing and side, the piece the crossing sits on has length >= 1 half tile (diagonal) or >= 2 (axis) (tag `cross <c> piece`); a zero-length crossing piece crosses at another angle.
	bool node_lead = true; ///< On by default (D435: the domain suite exact on X1, X2, M1, K1, L1, Y1, P1, BR1); false is the old behaviour. Rule A (D400, engineer 2026-11-23): write PlanarNodeLeadRows as hard rows (tag lead).
	bool node_flank = false; ///< A probe (2026-11-24): every track end at a node (not a port; not a D412 port stretch or a zero leg) keeps its end piece at its least length, never 0, so a leg's node turn cannot vanish (V's PSG 197 keeps all 36 node-side flanks).
	bool flank_turn = false; ///< D448 (1): per flex insert and side, when a piece in the zero-able run beyond that flank (up to the first piece with a least length) or the node neighbour's end heading is more than one eighth from the insert's heading, write `flank_k * flank - insert >= 0` (tag `flank <track> <m> before|after`), so a used insert keeps its flank.
	bool node_cusp = false; ///< A probe (D451): a non-straight Line leg at the trunk heading at a topology node (a cusp) gets its node-end flex insert on the turned heading forced (length >= 1) with every piece between it and the node held at 0, so the leg arrives on the turned heading flush at the node, as the game's prio join does; a cusp with no such insert is reported (cusps_missing).
	double flank_k = 64; ///< With flank_turn: the rows' constant K (as D430's lead rows' 64); a small K keeps the relaxation tight (K = coord_max = 4096 made stage A 40x slower on T1).
	bool grid_conditional = false; ///< Probe (with grid): stage A: an axis run that may be zero is odd across when it has length (tags `grid run`, `grid cond`).
	bool node_gaps = false; ///< Node gaps (SPEC-slots-lengths §8.1 node_leg, D400): a track from a node to a node with no event travels at least rules.node_leg, or 0 (a trident) where node_gap_from says so (tags `leg`, `leg zero`).
	const std::vector<std::vector<double>> *node_gap_from = nullptr; ///< With node_gaps: a drawing's lengths (as PlanarDrawResult::lengths); a node-to-node track whose travel there is below 1 half tile is held at 0, every other at least rules.node_leg. Null: every one at least rules.node_leg.
	const std::vector<std::vector<double>> *along_from = nullptr; ///< D438: per track (a crosser of a TUNNEL), per index into that track's events (PlanarTrackMap::events), 1 when that crossing is in along mode (the crosser has pieces of the separator's heading mod 4 immediately after its crossing piece, lying on the separator's line), else 0; other tracks and events 0. Null: no along mode anywhere. In along mode the program holds the along pieces' endpoints on the separator's line (tag `along <c> on`) and the end run clear of the span piece's ends (`along <c> clear`), and the keep-apart pairs of the span piece with those pieces and the leaving piece are skipped (reason `along`).
	bool port_zero = false; ///< A probe (D434): a D412 port line (in-port, no event, to a node) is 0 or an ordinary line, the branch from port_zero_from (tag `portzero`).
	const std::vector<std::vector<double>> *port_zero_from = nullptr; ///< With port_zero: a drawing's lengths (as PlanarDrawResult::lengths); a D412 line whose travel there is below 1 half tile is held at 0 (its pieces free), every other is an ordinary line. Null: every one ordinary.
	bool corridor = false; ///< The corridor family (D421): beside tracks (copies, rows) as offset curves of their parent chain one lane to their side, with an exact one-lane opening and closing step at their nodes.
	bool sibling_lanes = false; ///< A probe (D388 spine, SPEC-slots-lengths §11): sibling lanes of one channel are offset curves at one lane between their first and last channel crossing (tag lane), and keep-apart skips their wall pairs there.
	const std::vector<std::vector<double>> *curve_flex_from = nullptr; ///< Flex commitment (D422 probe; Gemini digest 2026-11-23): per flex middle, its length in this reference (half tiles, as PlanarDrawResult::lengths); at or above curve_flex_above the insert is committed (length >= its least, a real bend pair in the curve rows), below it the insert is held at 0.
	double curve_flex_above = 0.25; ///< With curve_flex_from.
	const std::vector<std::vector<double>> *curve_flex_soft = nullptr; ///< Scheme A (a probe): per track and piece, a slack price; when set, every flex insert is a real bend in the curve rows (its length stays free), and its rows (tag "curve flex <name> <i>") are elastic at that price whatever elastic_families says (price 0: no row).
	std::vector<std::string> flex_free_tracks; ///< With curve_flex_from (a measurement, neighbourhood repair): the flex inserts of these tracks (by name) are free (lower bound 0, not held), whatever the reference says.
	bool curve_flex_free = false; ///< With curve_flex_from (a measurement, the additions pass): an insert below curve_flex_above is free (priced by flex_weights, no curve rows, as in discovery) instead of held at 0.
	const std::vector<std::vector<double>> *flex_weights = nullptr; ///< A measurement (D422 discovery, IRL1): per track and piece, the extra objective cost per half tile of that flex insert's length; when set it replaces flex_price.
	double box_price = 0; ///< A measurement (D422 discovery): objective cost per half tile of width and of height.
	double flex_price = 0; ///< A measurement (D422 discovery): extra objective cost per half tile of flex insert length, so a drawing uses inserts only where they buy separation.
	bool pairwise_flex_only = false; ///< With pairwise (a D416 count probe): in the faces that get separation rows, separate only pairs where at least one wall is in a flex group (a flex middle piece or the piece just before or after it on its track); other pairs get no row. A relaxation, for counting and timing.
	std::vector<uint32_t> only_faces; ///< Write the face rows (landings and clearance per cell, or in pairwise mode clearance or separation) only for these faces (map face indices); empty is every face. A measurement aid.
	std::vector<std::string> skip_rows; ///< A measurement aid: rows whose tag equals one of these, or starts with one ending in a space, are not written (a deletion filter).
	std::vector<std::string> skip_pairs; ///< A measurement aid: in pairwise mode these pair tags ("sep <face> <i> <j>") get no separation rows (a deletion filter).
	std::vector<std::string> only_pairs; ///< D444 trial: see only_pairs_on.
	bool only_pairs_on = false; ///< D444 trial: in pairwise mode, when only_pairs_on, only pairs whose tag is in only_pairs get separation rows (an empty list writes none); every pair is still listed in keep_apart_out.
	std::vector<std::pair<std::string, int>> fixed_pairs; ///< D444 trial: these pair tags take the given candidate (axis*2+side) as one plain row, no binary; with only_pairs_on, a pair listed here gets rows even if not in only_pairs.
	bool flex_sides = true; ///< With clearance: rows by sides (D407): flex inserts join their side and are no target, and each vertex keeps 0 from its cell's dummies (D408); false is the rule before D407 (sides by equal headings only, no dummy rows).
	bool tidy = true; ///< The objective: true minimises the sum of piece lengths (a tidy point); false sets no objective (feasibility only, a measurement).
	bool pocket = false; ///< With flex_sides: the pocket rule (SPEC-shape §8.9, a probe): an adjacent side's vertices are held past a side's last (first) insert and keep only the lines beyond it.
	bool elastic = false; ///< Diagnosis (D409): every row except the piece, box, pin, fix and grid rows (game rules) gets a slack >= 0 per finite bound with cost 1, the objective is the total slack (piece lengths cost 0), every variable is continuous (the LP relaxation, which an infeasible drawing already fails when it has IIS rows), and the result lists the rows that stretched.
	double elastic_tidy = 0; ///< With elastic and tidy: each piece length also costs this (a tie-break below the slack's cost 1), so an elastic drawing stays compact.
	double slack_price = 1; ///< With elastic: the objective cost of each slack per half tile.
	std::vector<std::string> elastic_families; ///< With elastic: when not empty, only rows of these families (tag up to the first space) get slack; every other row is hard (a measurement: which rows of one family V's geometry needs stretched).
	bool elastic_integer = false; ///< With elastic (a measurement): the variables keep their integrality; only the slacks are continuous.
	const PlanarDrawPins *pins = nullptr; ///< Answer-key pins (D393): every pinned point is within window of its position plus one common shift (two free integer variables).
	const std::vector<std::vector<std::array<int, 2>>> *sides_from = nullptr; ///< With pairwise: a drawing of this variant (`PlanarDrawResult::points`); each separated pair takes the axis side these points keep best as a plain row, with no binary and no `sepdir` (an LP when every other variable is continuous).
	std::vector<std::string> free_sides; ///< A probe: with sides_from, these sep pair tags ("sep <face> <i> <j>") get the binary side choice instead of a fixed side.
	std::vector<uint32_t> free_faces; ///< A probe: with sides_from, every sep pair of these faces gets the binary side choice, as free_sides does for single pairs.
	std::vector<std::pair<std::string, int>> forced_sides; ///< A probe: with sides_from, these sep pair tags ("sep <face> <i> <j>") take the given candidate (axis index * 2 + side) instead of the best one, written as one hard row tagged "sephard <face> <i> <j>" (sep_extremal endpoints).
	std::vector<std::string> margin_tags; ///< A read-out: with sides_from, for these sep pair tags every candidate's margin on the reference is listed in PlanarDrawResult::sep_margins.
	double sides_free_below = -1e30; ///< With sides_from: a pair whose best candidate's margin on the reference is below this keeps its binaries (all candidates, one sepdir row) instead of a fixed direction.
	int least_extra = 0; ///< A measurement aid (domain suite bases): every piece with a least length gets its length variable's lower bound raised from 1 to 1 + least_extra (an axis piece in whole tiles, a diagonal in half tiles), so a tidy drawing has room. Free pieces (flex, steps, D412) keep 0.
	int sides_pad = 0; ///< With pairwise: every separation's spacing is raised by this many half tiles (a direction-finding round, D418; never a rule).
	const std::vector<std::vector<double>> *flex_zero_from = nullptr; ///< A measurement (D418): a flex insert whose length in this reference (`PlanarDrawResult::lengths`) is below flex_zero_below is held at 0.
	double flex_zero_below = 0; ///< With flex_zero_from.
	bool sides_by_walk = false; ///< With pairwise and sides_from (a measurement): a pair whose face walk is monotone towards one wall's inward normal takes that candidate (the rule); other pairs take sides_from's choice.
	bool facing_sides = false; ///< D426 probe: with pairwise and sides_from, a pair whose face walk is monotone both ways (i to j towards i's inward normal, j to i towards j's) takes its side from the walk: anti-parallel walls the common normal, other angles the better of the two inward normals on the reference; other pairs keep sides_from's choice.
	bool strict_span = false; ///< D427 probe: a span piece is exempt from keep-apart only against the crossing piece of a track that crosses it on that piece.
	bool strict_zero = false; ///< D427 probe: a pair of different tracks the zero contraction would skip is kept (same-track pairs stay skipped): skipped only where the walls cross at the touch point; otherwise each touching end is exempt against the other wall and the remaining endpoint pairs keep full spacing, unless both leave the touch point in the same direction (then every endpoint pair).
	bool crossing_flanks = false; ///< D427 probe: a structure's crosser whose crossing piece is a flank of an S-bend keeps the crossing 1 half tile off that S-bend's side of the piece when the S-bend's insert is parallel to the other track's crossing piece, so it never runs along that track from the crossing point.
	bool inward_sides = false; ///< A probe: in pairwise mode every pair's candidates are only the inward ones: wall i's inward normal from i to j, and, when the walls differ mod 4, wall j's inward normal from j to i (with sides_from: the better of these on the reference; without: binaries over these only).
	bool walk_strict = false; ///< With sides_by_walk (D417): the rule fixes a pair only when the walk's guaranteed projection onto the wall's inward normal (the walls strictly between, each at its least length) is at least the pair's spacing.
	std::vector<std::string> continuous; ///< A measurement (D409 diagnosis): variable groups written continuous in an integer drawing: "point" (track points), "axis" (axis piece lengths), "diagonal" (diagonal piece lengths), "param" (crossing parameters), "cross" (crossing points), "other" (every other variable: box, pin shift, dummies, binaries).
	const std::vector<std::vector<std::array<int, 2>>> *fix_points = nullptr; ///< A measurement: every track point is held equal to this drawing's (PlanarDrawResult::points of the same variant; hard rows, tag "fix"; a track with an empty point list is not fixed), so elastic slack on the remaining rows is their violation on that drawing.
	int fix_window = 0; ///< With fix_points: each point is held within this many half tiles of the reference per axis (absorbs rounding of an LP reference).
	const std::vector<std::vector<double>> *start_lengths = nullptr; ///< A measurement: a partial MIP start, every piece length (half tiles, as `PlanarDrawResult::lengths`) of this drawing that is whole in its variable's units; the solver completes the rest.
	bool grid = false; ///< A measurement (D382 probe): every point of an axis run of positive length has an odd coordinate across it (N/S on odd x, E/W on odd y); the parities this and the integer equalities force are propagated, each forced variable is written 2 h + p, and every all-integer row is divided by its gcd with its bounds rounded inward (whole tiles). Not with elastic.
	const std::vector<double> *start_all = nullptr; ///< A measurement: a complete MIP start, one value per variable in creation order (as an export lists them, e.g. from an offline solve); ignored unless its size equals the variable count.
	const std::vector<std::vector<std::array<int, 2>>> *start_points = nullptr; ///< A measurement: a MIP start of every track point from this drawing (as `PlanarDrawResult::points`).
	std::string export_path; ///< A measurement (D418): when not empty, the built program is written there as JSON before solving (variables, rows with their tags, the objective), for offline solvers.
	bool keep_apart_out = false; ///< A read-out (D428): fill PlanarDrawResult::keep_apart; binary mode only (no sides_from, sides_by_walk or facing_sides; else unsupported).
	bool program_out = false; ///< A read-out (D428): fill PlanarDrawResult::program with every variable and row as written, before any solve; works with build_only.
	bool build_only = false; ///< A measurement (D428): build the program and return before the solve, status "built".
	std::string dump_path; ///< A measurement aid: when not empty, the drawing is written there as JSON after the solve (tracks, faces, rows that stretched, every keep-apart pair with its candidates and their margins on the drawing).
};

/**
 * The opening step of a beside track (D421): from its from-node's point to its first point, an exact one-lane step.
 * @param heading The beside track's first piece heading (any integer, taken mod 8).
 * @param side Its side relative to its parent chain, +1 right of travel, -1 left.
 * @return 2 * PlanarModel::STEP[(heading + side) mod 8], in half tiles.
 */
std::array<int, 2> PlanarBesideOpening(int heading, int side);

/**
 * The closing step of a beside track (D421): from its last point to its to-node's point, an exact one-lane step.
 * @param heading The beside track's last piece heading (any integer, taken mod 8).
 * @param side Its side relative to its parent chain, +1 right of travel, -1 left.
 * @return 2 * PlanarModel::STEP[(heading - side) mod 8], in half tiles.
 */
std::array<int, 2> PlanarBesideClosing(int heading, int side);

/** One clearance row of a cell (SPEC-shape §8.8): the vertex after wall `vertex` keeps `spacing` from the line of wall `wall`, on the cell's side. */
struct PlanarClearRow {
	uint32_t vertex; ///< The vertex: the corner after this wall in the cell's walk.
	uint32_t wall; ///< The wall whose line it keeps from.
	int spacing; ///< The least distance along the wall's left normal (`RungSpacing` units); 0 for a dummy.
	uint32_t room = UINT32_MAX; ///< A flex insert at this vertex whose heading needs more spacing than `spacing` (SPEC-shape §8.10): its length must be 0 unless the vertex keeps more than `spacing`; UINT32_MAX for none.
};

/** An along row of a cell (SPEC-shape §8.9): Dot(V[vertex] - V[anchor], STEP[heading]) >= spacing, where V[k] is the corner after wall k. */
struct PlanarAlongRow {
	uint32_t vertex; ///< The vertex held.
	uint32_t anchor; ///< The vertex it is held past: the end of the side's last insert, or the start of its first.
	int heading; ///< The direction along which it is held: the side's walked heading, or its reverse.
	int spacing; ///< 0 along an axis heading, 2 along a diagonal one.
};

/**
 * The clearance rows of one cell, by sides (D407 (2), D408), before the variant's own exemptions (span pieces, shared node points, crossing points).
 * A side is a maximal run of walls joined where two consecutive walls have equal headings or one of them is a flex insert.
 * Each vertex k (between walls k and k + 1) has rows against every wall e that is not k or k + 1, not an insert, not a box edge, and not on the side of wall k or of wall k + 1:
 * a track wall with spacing max(RungSpacing(h_e, h_k), RungSpacing(h_e, h_k+1)), a dummy with spacing 0. A vertex between two walls that are no track (dummies, box edges) has no rows.
 * @param cell The cell's walls in walk order, the cell on the left.
 * With `pocket` (SPEC-shape §8.9, a probe), for each side S with an insert whose next side R (previous side P) is all track walls and the cell has at least 3 sides, every vertex of R (P) except the corner with S gets an along row past S's last (first) insert, and a vertex with both walls in R (P) drops its line rows against S's walls before the last (after the first) insert.
 * A step of length 0 imposes no spacing (SPEC-shape §8.10): at a vertex of exactly one insert the spacing is that of the other wall, and a `room` names the insert when its own heading would need more.
 * With `pocket` false the rows are exactly those above and `along` is untouched.
 * @param cell The cell's walls in walk order, the cell on the left.
 * @param insert Per wall, whether it is a flex insert.
 * @param pocket Whether to apply the pocket rule.
 * @param along When pocket, receives the along rows (may be null).
 * @return The rows.
 */
std::vector<PlanarClearRow> PlanarCellClearRows(const std::vector<PlanarCellWall> &cell, const std::vector<bool> &insert, bool pocket = false, std::vector<PlanarAlongRow> *along = nullptr);

/**
 * Draw Shape's shape on the half-tile grid with every Slots rule of the families below kept, as one integer program (E5-1).
 * A test oracle (D388 (b)): an infeasible result names the conflicting rows, which names a missing upstream promise.
 * Units are half tiles; a heading h moves STEP[h mod 8] per unit (an axis unit (0, 1) or (1, 0), a diagonal unit (1, 1)). Every row has a tag kept beside the program.
 * Families: points and pieces (`piece`, `flex`), the box (`box`), ports (`port <side>`), nodes (`node`), crossings (`cross`, `order`), spans (`deck`, `end`, `span`), dummies with their landings (`land <dummy>`) and clearance per convex cell (`clear <cell> <vertex> <wall>`; a structure's span pieces and dummies are no wall, a wall is no target for a vertex on its own straight side, a vertex between two non-track walls has no row, a corner between two crossing tracks is the crossing point, and a vertex inside a span piece has no row), the last three only with `options.clearance` (the clearance rows also only with `options.clear_rows`), corridors (`corridor`, option) and sibling lanes (`lane`, option `sibling_lanes`).
 * With `options.pairwise` (a D406 probe) there are no dummies or landings: a face convex without dummies keeps the clearance rows, every other face separates its non-incident wall pairs (`sep <face> <i> <j>`, one binary per axis side, `sepdir` picks one side of the pair); with `options.sides_from` each pair takes the side that drawing keeps best as plain rows, no binaries.
 * Not in this step: grid parity (D382), node legs and the trident binary, coverage; no clearance against a box edge.
 * @param v The shaped variant.
 * @param seconds The solver's time limit.
 * @param options What to write.
 * @return The result.
 */
PlanarDrawResult CheckPlanarDrawable(const PlanarShapeVariant &v, double seconds, const PlanarDrawOptions &options = {});

/** The drawing pipeline's settings (D422 defaults). */
struct PlanarDrawPipelineOptions {
	const PlanarDrawPins *pins = nullptr; ///< Answer-key pins, or none.
	double round_seconds = 30.0; ///< Time limit of each LP round.
	double final_seconds = 60.0; ///< Time limit of the integer final.
	double first_flex_price = 0.05; ///< Insert price per half tile in the first discovery round.
	double irl1 = 0.1; ///< From the second discovery round, an insert's price is irl1 / (its previous length + 0.5).
	double box_price = 0.01; ///< Price per half tile of width and of height, in every round.
	double slack_price = 1; ///< The slack price of every elastic round (PlanarDrawOptions::slack_price); it should be far above box_price, or the rounds may trade a broken rule for a smaller box.
	double discovery_slack_price = 1; ///< The slack price of the seed and discovery rounds; slack_price applies from the commit on.
	bool corridor = true; ///< Beside tracks as offset curves (D421) in every stage.
	bool sibling_lanes = false; ///< A probe (D388 spine, SPEC-slots-lengths §11): PlanarDrawOptions::sibling_lanes in every stage.
	bool facing_sides = false; ///< A probe (D426): PlanarDrawOptions::facing_sides in every stage.
	bool strict_span = false; ///< A probe (D427): PlanarDrawOptions::strict_span in every stage.
	bool node_lead = true; ///< On by default (D428): PlanarDrawOptions::node_lead in every stage.
	bool end_node = true; ///< On by default (D428): PlanarDrawOptions::end_node in every stage.
	bool cross_piece = true; ///< On by default (D428): PlanarDrawOptions::cross_piece in every stage.
	bool strict_zero = false; ///< A probe (D427): PlanarDrawOptions::strict_zero in every stage.
	bool crossing_flanks = false; ///< A probe (D427): PlanarDrawOptions::crossing_flanks in every stage.
	std::vector<uint32_t> final_free_faces; ///< A measurement (D427): the integer final gives every keep-apart pair of these faces the binary side choice (PlanarDrawOptions::free_faces) instead of the side read from the last round.
	bool inward_sides = false; ///< A probe: PlanarDrawOptions::inward_sides in every stage.
	bool final_tidy = true; ///< The integer final minimises the sum of piece lengths; false is feasibility only (a measurement).
	bool final_grid = true; ///< On by default (D435): the integer final writes the grid rows (PlanarDrawOptions::grid).
	bool final_soft_sep = false; ///< A measurement: the integer final's sep rows get slack (cost 1, piece lengths cost 0.01), every variable stays integer; the result lists the rows that stretched.
	bool free_stretched_sides = false; ///< A probe: after the last LP round, one round with the stretched sep pairs' sides free (binaries), lengths continuous; logged as "free sides".
	bool side_probe = false; ///< A probe: after the last LP round, per stretched sep pair and per candidate side, one LP like the last round with that pair's side hard (forced_sides) and every other row as in the last round; logged as "side probe".
	bool final_soft_all = false; ///< A measurement: every row of the integer final except piece, box, pin, fix and grid rows gets slack (cost 1, piece lengths cost 0), every variable stays integer; the result lists the rows that stretched.
	bool final_start = false; ///< A measurement: the integer final starts from the last round's whole lengths (a partial MIP start, JunctionMilp::SetStart).
	bool node_gaps = true; ///< On by default (D424). Node gaps in every stage after the seed, each stage's reference its input drawing (PlanarDrawOptions::node_gaps).
	bool keep_apart = true; ///< Pairwise keep-apart (D418) in every stage; false writes no clearance rows (clearance = false), a measurement for hubs whose keep-apart is not yet drawable.
	double commit_above = 1.0; ///< An insert at or above this length (half tiles) in the last discovery drawing is committed.
	int discovery_rounds = 8; ///< At most this many discovery rounds.
	int commit_rounds = 4; ///< At most this many rounds after the commit, and again after the prune.
	int prunes = 1; ///< At most this many prunes, each holding at 0 the committed inserts whose curve flex rows still stretch, each followed by up to commit_rounds rounds; stops early when a prune holds none (D422: 1).
	bool curve_discovery = true; ///< On by default (D425). Scheme A: from discovery round 2, insert curve rows priced by a damped memory of each insert's length (CARD-E5-schemeA); at most 4 discovery rounds after round 1.
	std::string dump_dir; ///< A measurement aid: when not empty, each stage's drawing is dumped there as <stage>-<n>.json (seed, round, commit, prune, final).
	bool final_contract = false; ///< A measurement (D427): before the integer final, every flex insert the commit held at 0 is removed with its trailing flank (one straight piece remains), every committed insert becomes a step (PlanarShapeTrack::steps, a fixed bend pair), and the final runs on that variant with the last drawing's points and lengths mapped onto it.
	bool additions = false; ///< A probe: after the commit and prune, one additions pass (rounds with the committed inserts kept and the others free under IRL1, the inserts it uses committed, then the commit rounds and the prunes again).
};

/** What the drawing pipeline did, and its drawing. */
struct PlanarDrawPipelineResult {
	PlanarDrawResult final; ///< The integer final (or the last result before the pipeline stopped).
	std::vector<std::vector<double>> committed; ///< The commitment reference handed to the final (as curve_flex_from).
	size_t committed_count = 0; ///< Inserts committed before the prune.
	size_t pruned = 0; ///< Inserts the prune held at 0.
	uint32_t final_pairs = 0; ///< The final's keep-apart pairs, from its result's pairs.
	uint32_t contracted = 0; ///< With final_contract: erased flex pairs.
	uint32_t contract_steps = 0; ///< With final_contract: committed inserts kept as steps.
	PlanarShapeVariant final_variant; ///< With final_contract: the contracted variant the final ran on.
	std::vector<std::vector<std::array<int, 2>>> final_sides; ///< With final_contract: the mapped reference the final read its sides from.
	size_t added = 0; ///< With additions: inserts the additions pass committed.
	std::vector<std::string> log; ///< One line per round: stage, round, status, total slack, curve slack, box, length.
	std::string stopped; ///< Empty, or the stage that ended without an Optimal round.
	std::vector<PlanarDrawResult> rounds; ///< A read-out: every round's result in order (seed, discovery, commit, prune).
	PlanarDrawResult last_round; ///< The last Optimal LP round (a read-out: its drawing and the rows that stretched).
	std::vector<std::vector<std::array<int, 2>>> last_input; ///< The input drawing of the last Optimal round, from which the final took its sides (a read-out).
};

/**
 * Draw Shape's shape with optional flex by the D422 pipeline: free LP discovery rounds with insert prices, a commit of the inserts used, one prune, and an integer final on the sides the last round proved.
 * @param v The shaped variant.
 * @param options The settings.
 * @return The result.
 */
PlanarDrawPipelineResult DrawPlanarPipeline(const PlanarShapeVariant &v, const PlanarDrawPipelineOptions &options);

#endif /* PLANAR_DRAW_H */
