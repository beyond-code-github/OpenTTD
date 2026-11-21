/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file planar_shape_layer.h The Shape layer's hand-off: headings, crossing angles and structure axes, and the promise oracle (SPEC-shape §6, DESIGN-tracks §17.2). */

#ifndef PLANAR_SHAPE_LAYER_H
#define PLANAR_SHAPE_LAYER_H

#include "../junction_spec.h"
#include "planar_structure.h"
#include "planar_topology.h"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/** One track's shape: its heading list and the piece each crossing sits on (CLAUDE Terms: Shape). */
struct PlanarShapeTrack {
	std::vector<int> pieces; ///< One lifted heading per piece (eighths clockwise from north, not reduced mod 8); consecutive pieces are one eighth apart. The heading at a point is the heading of the piece it sits on.
	std::vector<uint32_t> event_piece; ///< Per entry of the map's events of this track, the piece that crossing sits on. Stretch k runs from piece event_piece[k-1] (0 for k = 0) to event_piece[k] (the last piece for the last stretch).
	std::vector<uint32_t> flex; ///< The middle piece of each flex pair (h, h +- 1, h), ascending; Slots sizes each pair >= 0, 0 being no step (D397).
};

/** What Shape derives for one structure. */
struct PlanarShapeStructure {
	uint8_t axis; ///< The axis heading (eighths, even) its separator holds from the first to the last crossing.
	std::array<int, 2> middles; ///< The least middle tiles per kind at the chosen angles (index 0 bridge, 1 tunnel; -1 for a kind the structure does not allow).
	std::array<int, 2> end_run; ///< The straight end-run tiles per kind (bridge: the head; tunnel: portal and ramp; -1 for a kind the structure does not allow).
	std::array<int, 2> copy_middles = {-1, -1}; ///< The least middle tiles of the copy's deck per kind at its chosen angles (-1 for a kind not allowed, or no copy).
};

/** The land_track of a dummy that lands on another dummy: its land_piece is that dummy's index; also the track of a dummy wall of a cell. */
constexpr uint32_t PLANAR_DUMMY_TRACK = UINT32_MAX - 1;

/** A dummy edge that refines a face into convex cells (SPEC-shape §8.1, D385, H12). */
struct PlanarShapeDummy {
	uint32_t face; ///< The face it lies in (index into the map's faces).
	uint32_t track; ///< The track whose bend it starts at.
	uint32_t piece; ///< The bend: the vertex between this piece and the next of the track.
	int heading; ///< Its heading, 0..7.
	uint32_t land_track; ///< The track of the wall it ends on, UINT32_MAX for a box edge, or PLANAR_DUMMY_TRACK for a dummy (then land_piece is that dummy's index).
	uint32_t land_piece; ///< The piece of that track it ends on, the PlanarSide of the box edge, or the index of the dummy it ends on.
};

/** One variant of Shape's output: Structure's variant refined with headings. */
struct PlanarShapeVariant {
	PlanarStructureVariant structure; ///< Structure's variant, unchanged (map, sites, structures, kinds, cost).
	std::vector<PlanarShapeTrack> tracks; ///< Per track of the map, beside tracks included, its shape.
	std::vector<uint8_t> angles; ///< Per crossing of the map, its angle in eighths (1 to 3).
	std::vector<PlanarShapeStructure> structures; ///< Per structure of Structure's variant, in the same order.
	std::vector<PlanarShapeDummy> dummies; ///< The dummy edges that make every face's cells convex (H12).
	int cost; ///< Shape's cost: the turning (sum of |turn| over stretches, D340).
	uint32_t nonmonotone_dummies = 0; ///< The dummies H12b placed with no monotone candidate: their landing may need lengths the drawing program must meet (D405).
};

/** Shape's output: the variants ranked by Structure cost, then Shape cost (D373). */
struct PlanarShapeResult {
	std::vector<PlanarShapeVariant> variants; ///< Ranked: Structure cost, then Shape cost.
	std::string why; ///< Empty, or why no variant exists.
	std::vector<std::pair<std::string, std::string>> unshaped; ///< Variants handed down that got no shape: each is a bug (SPEC-shape H10). Tag and why.
};

/**
 * The parent chain of a beside track (H8): from the straight leg of its start node's site, at each end node the out track that is a part of its line, to the track ending where it ends.
 * @param map The track map.
 * @param sites The variant's sites.
 * @param b The Copy or Row track.
 * @param chain Receives the tracks in order.
 * @return False when there is no (unique) chain.
 */
bool ParentChain(const PlanarTrackMap &map, const std::vector<PlanarStructureSite> &sites, uint32_t b, std::vector<uint32_t> &chain);

/**
 * The promise H1 to H9 (H11, per run since D396, is held by H6's least middles) of SPEC-shape §4 as a test oracle (D367).
 * The topology and spec serve H9 (P1 to P8) and H6's limits; the oracle is a test tool, not a layer.
 * @param topology The topology the variant was made from.
 * @param spec The spec: the bridge and tunnel limits and the flow bound.
 * @param v The variant.
 * @return One message per breach, each starting with its promise id ("H3: ..."); empty when the promise is kept.
 */
std::vector<std::string> CheckPlanarShapePromise(const PlanarTopology &topology, const JunctionSpec &spec, const PlanarShapeVariant &v);

/**
 * H12 (SPEC-shape 8.1, D385): every inner face's cells, split by v.dummies, turn one way and wind once; part of CheckPlanarShapePromise.
 * Faces that do not wind once are skipped (H7 reports them).
 * A dummy is accepted only when every corner at a dummy end, its landing included, is convex (D397).
 * @param v The variant; its tracks must pass H1.
 * @return One message per breach, each starting with "H12:"; empty when every cell is convex.
 */
std::vector<std::string> CheckPlanarShapeCells(const PlanarShapeVariant &v);

/**
 * H14 (DESIGN-sss-v2 §3.2, D388 (a)): every channel's lines cross the same channels in order and follow one heading list between their first and last crossing (flex pairs removed); measured, not yet in the promise.
 * @param v The variant; its tracks must pass H1.
 * @return One message per channel and line pair that differs.
 */
std::vector<std::string> CheckPlanarShapeSpines(const PlanarShapeVariant &v);

/**
 * H12b (SPEC-shape 8.7, D385, D386): refine every inner face into convex cells by dummy edges from each reflex bend, by rule.
 * Candidate headings h, h - 3, h - 1, h - 2 (h the incoming wall's lifted heading), the landing the first facing wall after the corner; a monotone landing first (every wall from the corner's outgoing wall to the landing wall has a non-negative dot product with the ray, D405), then the first candidate whose two cells both have 4 or more walls, else the first valid one.
 * Dummies placed with no monotone candidate are counted in v.nonmonotone_dummies.
 * Faces that do not wind once are skipped (H7 reports them).
 * @param v The variant; its tracks must pass H1. Its dummies are replaced.
 * @param why Receives the reason on failure ("no cells: ...").
 * @return False when a reflex corner is not a bend inside one track, or no candidate resolves it.
 */
bool RefinePlanarShapeCells(PlanarShapeVariant &v, std::string &why);

/** One wall of a convex cell, walked with the cell on its left. */
struct PlanarCellWall {
	uint32_t track; ///< A track of the map, PLANAR_DUMMY_TRACK for a dummy, or UINT32_MAX for a box edge.
	uint32_t piece; ///< The piece of the track, the dummy's index in the variant's dummies, or the PlanarSide of the box edge.
	int heading; ///< The walked heading, 0..7.
};

/**
 * The convex cells of every inner face as RefinePlanarShapeCells splits them: each face's walk split by the stored dummies in order (D385).
 * Faces that H12 skips (not usable, not winding once) have no cells; a dummy that does not split its face's cells (H12 reports it) is ignored.
 * @param v The variant; its tracks must pass H1.
 * @return One list of walls per cell, in walk order with the cell on the left; a face's cells together, faces in the map's order.
 */
std::vector<std::vector<PlanarCellWall>> PlanarShapeCellWalls(const PlanarShapeVariant &v);

/** Per cell of PlanarShapeCellWalls, in the same order, the face it lies in. */
std::vector<uint32_t> PlanarShapeCellFaces(const PlanarShapeVariant &v);

/** One inner face's walk, unsplit by dummies. */
struct PlanarFaceWalls {
	uint32_t face; ///< The face's index in the map's faces.
	std::vector<PlanarCellWall> walls; ///< Its walls in walk order, the face on the left.
};

/**
 * Every inner face's walls as PlanarShapeCellWalls walks them, without the dummies' splits: the same faces (usable, winding -8), in the map's order.
 * @param v The variant; its tracks must pass H1.
 * @return One entry per face.
 */
std::vector<PlanarFaceWalls> PlanarShapeFaceWalls(const PlanarShapeVariant &v);

/**
 * Flex points on free one-piece stretches (D397, steps on spines): on every free one-piece stretch (a Line track's stretch between two crossings that are not of one structure and not with two tracks of one channel, the track in no beside track's parent chain) on piece p at heading h, insert after p the left pair h-1, h then the right pair h+1, h, each side only where every route through it stays within 3 eighths of its entry heading (H7's cone).
 * Stretches are processed last to first; later event pieces shift; PlanarShapeTrack::flex lists the middles.
 * Its dummies are not touched: refine after.
 * @param v The shaped variant; every track's flex is replaced.
 * @param why Receives "no flex: ..." on an internal inconsistency.
 * @return False only on an internal inconsistency; the rule has no failure.
 */
bool PlacePlanarShapeFlex(PlanarShapeVariant &v, std::string &why);

/**
 * Shape one Structure variant: the heading program of SPEC-shape §2 and §2.1 (every rule of SPEC-shape §2.1).
 * One integer program over the lifted heading at every point of every track (its ends and its events): port headings; the node rule at every site; the crossing rule with the arrival side, the separator on an axis and the crossing angle in 1..3 eighths; one axis across each structure; beside tracks equal to their parent; every face winding once; no route turning more than 3 eighths from its entry heading; per structure and kind the capacity rows (n crossers plus one tile per run of diagonal crossers at one angle, at most the structure's limit). A stretch is generated monotone from its end headings. It minimises the turning of the Line tracks (no angle preference).
 * Flex points are placed by rule (PlacePlanarShapeFlex, D397) before the cells are refined.
 * @param v Structure's variant.
 * @param out Receives the shaped variant when the result is true: the pieces, event pieces, crossing angles, structure axes, least middles and end runs, and the turning cost.
 * @param why Receives "no shape: <status>" when the program has no optimal solution, with the tags of the infeasible rows for an infeasible one.
 * @return Whether the variant was shaped.
 */
bool ShapePlanarStructureVariant(const PlanarStructureVariant &v, PlanarShapeVariant &out, std::string &why);

/**
 * A Shape output written down (D389): per-line headings and crossing pieces loaded into a shaped variant of a Structure variant, with the crossing angles, structure axes, end runs and least middles derived from the headings as ShapePlanarStructureVariant derives them; no dummies.
 * Text, one statement per line: `shape <line>: <h0> <h1> ...` (headings in eighths clockwise from north; lifted here along each route from the ports: a line from a port keeps its first heading in 0..7, a line from a node starts nearest its node's first in line's last heading, a line with no headings is one piece at that heading) and `at <line>: <other line>@<piece>, ...` (the piece each crossing sits on). Other lines are ignored.
 * @param topology The topology.
 * @param v The Structure variant; its map's tracks must be the topology's lines (no copies, rows or parts).
 * @param text The shape.
 * @param out Receives the variant when the result is true.
 * @param why Receives the reason otherwise.
 * @return Whether the shape loaded.
 */
bool LoadPlanarShapeVariant(const PlanarTopology &topology, const PlanarStructureVariant &v, std::string_view text, PlanarShapeVariant &out, std::string &why);

/**
 * Shape every variant Structure handed down (SPEC-shape §6).
 * @param structure Structure's output.
 * @return The shaped variants ranked by (Structure cost, Shape cost, Structure tag); the variants with no shape in `unshaped`; why is set only when none shaped.
 */
PlanarShapeResult BuildPlanarShape(PlanarStructureResult structure);

#endif /* PLANAR_SHAPE_LAYER_H */
