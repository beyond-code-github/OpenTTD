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
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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
	uint32_t walk_differs = 0; ///< With sides_by_walk: fixed pairs where the rule's candidate is not sides_from's best.
	double ms = 0; ///< Build and solve time in milliseconds.
	std::vector<std::vector<std::array<int, 2>>> points; ///< When feasible, per track its points (start, then the end of each piece), half tiles, x east and y south.
	size_t start_values = 0; ///< With start_lengths or start_points: how many variables the MIP start gave.
	std::vector<std::vector<double>> lengths; ///< When feasible, per track each piece's length in half-tile units along its heading (unrounded, so an LP's fractional lengths show).
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
	bool curves = true; ///< On by default since D420 (agrees with V's PSG 197); the curve spacing rows (SPEC-slots-lengths §8.1, §8.2, D420): Shape's bends along every route path keep curve_same, curve_opposite and curve_window; flex and step inserts count as travel, not bends (§10.3), except corridor steps and committed flex inserts (below).
	bool node_gaps = false; ///< Node gaps (SPEC-slots-lengths §8.1 node_leg, D400): a track from a node to a node with no event travels at least rules.node_leg, or 0 (a trident) where node_gap_from says so (tags `leg`, `leg zero`).
	const std::vector<std::vector<double>> *node_gap_from = nullptr; ///< With node_gaps: a drawing's lengths (as PlanarDrawResult::lengths); a node-to-node track whose travel there is below 1 half tile is held at 0, every other at least rules.node_leg. Null: every one at least rules.node_leg.
	bool corridor = false; ///< The corridor family (D421): beside tracks (copies, rows) as offset curves of their parent chain one lane to their side, with an exact one-lane opening and closing step at their nodes.
	bool sibling_lanes = false; ///< A probe (D388 spine, SPEC-slots-lengths §11): sibling lanes of one channel are offset curves at one lane between their first and last channel crossing (tag lane), and keep-apart skips their wall pairs there.
	const std::vector<std::vector<double>> *curve_flex_from = nullptr; ///< Flex commitment (D422 probe; Gemini digest 2026-11-23): per flex middle, its length in this reference (half tiles, as PlanarDrawResult::lengths); at or above curve_flex_above the insert is committed (length >= its least, a real bend pair in the curve rows), below it the insert is held at 0.
	double curve_flex_above = 0.25; ///< With curve_flex_from.
	const std::vector<std::vector<double>> *curve_flex_soft = nullptr; ///< Scheme A (a probe): per track and piece, a slack price; when set, every flex insert is a real bend in the curve rows (its length stays free), and its rows (tag "curve flex <name> <i>") are elastic at that price whatever elastic_families says (price 0: no row).
	bool curve_flex_free = false; ///< With curve_flex_from (a measurement, the additions pass): an insert below curve_flex_above is free (priced by flex_weights, no curve rows, as in discovery) instead of held at 0.
	const std::vector<std::vector<double>> *flex_weights = nullptr; ///< A measurement (D422 discovery, IRL1): per track and piece, the extra objective cost per half tile of that flex insert's length; when set it replaces flex_price.
	double box_price = 0; ///< A measurement (D422 discovery): objective cost per half tile of width and of height.
	double flex_price = 0; ///< A measurement (D422 discovery): extra objective cost per half tile of flex insert length, so a drawing uses inserts only where they buy separation.
	bool pairwise_flex_only = false; ///< With pairwise (a D416 count probe): in the faces that get separation rows, separate only pairs where at least one wall is in a flex group (a flex middle piece or the piece just before or after it on its track); other pairs get no row. A relaxation, for counting and timing.
	std::vector<uint32_t> only_faces; ///< Write the face rows (landings and clearance per cell, or in pairwise mode clearance or separation) only for these faces (map face indices); empty is every face. A measurement aid.
	std::vector<std::string> skip_rows; ///< A measurement aid: rows whose tag equals one of these, or starts with one ending in a space, are not written (a deletion filter).
	std::vector<std::string> skip_pairs; ///< A measurement aid: in pairwise mode these pair tags ("sep <face> <i> <j>") get no separation rows (a deletion filter).
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
	int sides_pad = 0; ///< With pairwise: every separation's spacing is raised by this many half tiles (a direction-finding round, D418; never a rule).
	const std::vector<std::vector<double>> *flex_zero_from = nullptr; ///< A measurement (D418): a flex insert whose length in this reference (`PlanarDrawResult::lengths`) is below flex_zero_below is held at 0.
	double flex_zero_below = 0; ///< With flex_zero_from.
	bool sides_by_walk = false; ///< With pairwise and sides_from (a measurement): a pair whose face walk is monotone towards one wall's inward normal takes that candidate (the rule); other pairs take sides_from's choice.
	bool facing_sides = false; ///< D426 probe: with pairwise and sides_from, a pair whose face walk is monotone both ways (i to j towards i's inward normal, j to i towards j's) takes its side from the walk: anti-parallel walls the common normal, other angles the better of the two inward normals on the reference; other pairs keep sides_from's choice.
	bool walk_strict = false; ///< With sides_by_walk (D417): the rule fixes a pair only when the walk's guaranteed projection onto the wall's inward normal (the walls strictly between, each at its least length) is at least the pair's spacing.
	std::vector<std::string> continuous; ///< A measurement (D409 diagnosis): variable groups written continuous in an integer drawing: "point" (track points), "axis" (axis piece lengths), "diagonal" (diagonal piece lengths), "param" (crossing parameters), "cross" (crossing points), "other" (every other variable: box, pin shift, dummies, binaries).
	const std::vector<std::vector<std::array<int, 2>>> *fix_points = nullptr; ///< A measurement: every track point is held equal to this drawing's (PlanarDrawResult::points of the same variant; hard rows, tag "fix"; a track with an empty point list is not fixed), so elastic slack on the remaining rows is their violation on that drawing.
	int fix_window = 0; ///< With fix_points: each point is held within this many half tiles of the reference per axis (absorbs rounding of an LP reference).
	const std::vector<std::vector<double>> *start_lengths = nullptr; ///< A measurement: a partial MIP start, every piece length (half tiles, as `PlanarDrawResult::lengths`) of this drawing that is whole in its variable's units; the solver completes the rest.
	bool grid = false; ///< A measurement (D382 probe): every point of an axis run of positive length has an odd coordinate across it (N/S on odd x, E/W on odd y); the parities this and the integer equalities force are propagated, each forced variable is written 2 h + p, and every all-integer row is divided by its gcd with its bounds rounded inward (whole tiles). Not with elastic.
	const std::vector<std::vector<std::array<int, 2>>> *start_points = nullptr; ///< A measurement: a MIP start of every track point from this drawing (as `PlanarDrawResult::points`).
	std::string export_path; ///< A measurement (D418): when not empty, the built program is written there as JSON before solving (variables, rows with their tags, the objective), for offline solvers.
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
	bool final_tidy = true; ///< The integer final minimises the sum of piece lengths; false is feasibility only (a measurement).
	bool final_grid = false; ///< A measurement (D382 probe): the integer final writes the grid rows (PlanarDrawOptions::grid).
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
	bool additions = false; ///< A probe: after the commit and prune, one additions pass (rounds with the committed inserts kept and the others free under IRL1, the inserts it uses committed, then the commit rounds and the prunes again).
};

/** What the drawing pipeline did, and its drawing. */
struct PlanarDrawPipelineResult {
	PlanarDrawResult final; ///< The integer final (or the last result before the pipeline stopped).
	std::vector<std::vector<double>> committed; ///< The commitment reference handed to the final (as curve_flex_from).
	size_t committed_count = 0; ///< Inserts committed before the prune.
	size_t pruned = 0; ///< Inserts the prune held at 0.
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
