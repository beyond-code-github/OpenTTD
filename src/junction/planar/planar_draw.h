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
#include <vector>

/** The result of the drawing program. */
struct PlanarDrawResult {
	bool feasible = false; ///< A drawing exists.
	std::string status; ///< "Optimal", "Infeasible", "Error", or "unsupported: <why>" for an input the program does not cover.
	std::vector<std::string> conflict; ///< When infeasible, the tags of the conflicting rows (InfeasibleRows), each once.
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
};

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
