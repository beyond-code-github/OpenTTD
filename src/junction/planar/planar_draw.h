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
#include <utility>
#include <vector>

/** The result of the drawing program. */
struct PlanarDrawResult {
	bool feasible = false; ///< A drawing exists.
	std::string status; ///< "Optimal", "Infeasible", "Error", or "unsupported: <why>" for an input the program does not cover.
	std::vector<std::string> conflict; ///< When infeasible, the tags of the conflicting rows (InfeasibleRows), each once.
	double slack = 0; ///< With elastic: the total slack of the solution.
	std::vector<std::pair<std::string, double>> stretched; ///< With elastic: per row tag whose slack is above 1e-6, the slack, largest first (a tag that names several rows sums them).
	uint32_t rows = 0; ///< The program's rows.
	uint32_t columns = 0; ///< The program's variables.
	uint32_t pairs = 0; ///< Wall pairs separated (pairwise mode).
	uint32_t binaries = 0; ///< Their axis-side binaries.
	double ms = 0; ///< Build and solve time in milliseconds.
	std::vector<std::vector<std::array<int, 2>>> points; ///< When feasible, per track its points (start, then the end of each piece), half tiles, x east and y south.
};

/** What the drawing program writes. */
struct PlanarDrawOptions {
	bool clearance = true; ///< Write the dummies, their landings and the clearance rows; false leaves them out (a variant with prio rows or copies needs the corridor family first, CARD-E5).
	bool clear_rows = true; ///< With clearance: write the clearance rows; false keeps the dummies and their landings but leaves the clearance rows out (a measurement of the landings alone).
	bool pairwise = false; ///< With clearance (D406 probe): no dummies and no landings; a face convex without dummies keeps the clearance rows, every other face gets pairwise separation of its walls (`sep`).
	std::vector<uint32_t> only_faces; ///< Write the face rows (landings and clearance per cell, or in pairwise mode clearance or separation) only for these faces (map face indices); empty is every face. A measurement aid.
	bool flex_sides = true; ///< With clearance: rows by sides (D407): flex inserts join their side and are no target, and each vertex keeps 0 from its cell's dummies (D408); false is the rule before D407 (sides by equal headings only, no dummy rows).
	bool tidy = true; ///< The objective: true minimises the sum of piece lengths (a tidy point); false sets no objective (feasibility only, a measurement).
	bool pocket = false; ///< With flex_sides: the pocket rule (SPEC-shape §8.9, a probe): an adjacent side's vertices are held past a side's last (first) insert and keep only the lines beyond it.
	bool elastic = false; ///< Diagnosis (D409): every row except the piece and box rows gets a slack >= 0 per finite bound with cost 1, the objective is the total slack (piece lengths cost 0), every variable is continuous (the LP relaxation, which an infeasible drawing already fails when it has IIS rows), and the result lists the rows that stretched.
};

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
 * Families: points and pieces (`piece`, `flex`), the box (`box`), ports (`port <side>`), nodes (`node`), crossings (`cross`, `order`), spans (`deck`, `end`, `span`), dummies with their landings (`land <dummy>`) and clearance per convex cell (`clear <cell> <vertex> <wall>`; a structure's span pieces and dummies are no wall, a wall is no target for a vertex on its own straight side, a vertex between two non-track walls has no row, a corner between two crossing tracks is the crossing point, and a vertex inside a span piece has no row), the last three only with `options.clearance` (the clearance rows also only with `options.clear_rows`).
 * With `options.pairwise` (a D406 probe) there are no dummies or landings: a face convex without dummies keeps the clearance rows, every other face separates its non-incident wall pairs (`sep <face> <i> <j>`, one binary per axis side, `sepdir` picks one side of the pair).
 * Not in this step: grid parity (D382), beside tracks as offset curves, node legs and the trident binary, bays, coverage, curve spacing; no clearance against a box edge.
 * @param v The shaped variant.
 * @param seconds The solver's time limit.
 * @param options What to write.
 * @return The result.
 */
PlanarDrawResult CheckPlanarDrawable(const PlanarShapeVariant &v, double seconds, const PlanarDrawOptions &options = {});

#endif /* PLANAR_DRAW_H */
