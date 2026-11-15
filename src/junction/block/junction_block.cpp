/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_block.cpp Junction block programs and the generic block generator (DESIGN-blocks). */

#include "../../stdafx.h"

#include "junction_block.h"
#include "../junction_spec.h"
#include "../junction_track_geometry.h"
#include "../placement/junction_positions.h"
#include "../topology/junction_chain.h"
#include "../../track_func.h"

#include <algorithm>
#include <map>
#include <optional>
#include <set>

#include "../../safeguards.h"

/** The PSG 227 join block: two bundles in, two lanes out (DESIGN-blocks §2). */
JunctionBlockProgram JunctionJoinBlock227Program()
{
	using Kind = JunctionBlockSegmentKind;
	enum Point : uint16_t { W, sA, sB, hN, hF, d1, b0, mC0, mC1, e0, e1, mL0, mL1, E };
	enum Node : uint16_t { NsA, NsB, NmC0, NmC1, NmL0, NmL1 };
	enum Edge : uint16_t { fA, L0, cAin, cA0, cA1, cBin, cB1, cB0, c0, c1, fB, L1 };

	auto run = [](uint16_t point) { return JunctionBlockSegment{Kind::Run, 0, point}; };
	auto bend = [](int lines) { return JunctionBlockSegment{Kind::SBend, lines}; };
	auto bridge = [](uint16_t point, uint16_t crosses) { return JunctionBlockSegment{Kind::Bridge, 0, point, crosses}; };
	constexpr bool PORT = false;
	constexpr bool NODE = true;

	JunctionBlockProgram p;
	p.points = {"W", "sA", "sB", "hN", "hF", "d1", "b0", "mC0", "mC1", "e0", "e1", "mL0", "mL1", "E"};
	p.east = E;
	p.in_lines = {1, 2, 5, 6};
	p.out_lines = {1, 6};
	p.nodes = {{true, sA}, {true, sB}, {false, mC0}, {false, mC1}, {false, mL0}, {false, mL1}};
	p.edges.resize(12);
	p.edges[fA] = {{PORT, 0}, {NODE, NmL0}, 1, {run(mL0)}};
	p.edges[L0] = {{NODE, NmL0}, {PORT, 0}, 1, {run(E)}};
	p.edges[cAin] = {{PORT, 1}, {NODE, NsA}, 2, {run(sA)}};
	p.edges[cA0] = {{NODE, NsA}, {NODE, NmC0}, 2, {run(mC0)}};
	p.edges[cA1] = {{NODE, NsA}, {NODE, NmC1}, 2, {bend(+1), run(hN), bridge(hF, cB0), run(d1), bend(+2)}};
	p.edges[cBin] = {{PORT, 2}, {NODE, NsB}, 5, {run(sB)}};
	p.edges[cB1] = {{NODE, NsB}, {NODE, NmC1}, 5, {run(mC1)}};
	p.edges[cB0] = {{NODE, NsB}, {NODE, NmC0}, 5, {bend(-1), run(b0), bend(-2)}};
	p.edges[c0] = {{NODE, NmC0}, {NODE, NmL0}, 2, {run(e0), bend(-1)}};
	p.edges[c1] = {{NODE, NmC1}, {NODE, NmL1}, 5, {run(e1), bend(+1)}};
	p.edges[fB] = {{PORT, 3}, {NODE, NmL1}, 6, {run(mL1)}};
	p.edges[L1] = {{NODE, NmL1}, {PORT, 1}, 6, {run(E)}};
	p.paths = {{fA, L0}, {cAin, cA0, c0, L0}, {cAin, cA1, c1, L1}, {cBin, cB0, c0, L0}, {cBin, cB1, c1, L1}, {fB, L1}};
	p.through = {true, false, false, false, false, true};
	p.prios = {{NmL0, 0}, {NmL1, 7}};
	p.topology = JunctionJoinBlock227Topology();
	return p;
}

/** The topology of the PSG 227 join block: W fA, cA, cB, fB and E L1, L0, one lane each, six movements. */
JunctionTopology JunctionJoinBlock227Topology()
{
	JunctionTopology topology;
	topology.groups = {
		{JunctionSide::W, true, 1, 0, true}, // fA
		{JunctionSide::W, true, 1, 2, true}, // cA
		{JunctionSide::W, true, 1, 3, true}, // cB
		{JunctionSide::W, true, 1, 1, true}, // fB
		{JunctionSide::E, false, 1, 1, true}, // L1
		{JunctionSide::E, false, 1, 0, true}, // L0
	};
	topology.movements = {{0, 5}, {1, 5}, {1, 4}, {2, 5}, {2, 4}, {3, 4}};
	topology.prio_movements = {0, 5}; // fA to L0 and fB to L1
	return topology;
}


namespace {

constexpr int MAX_COLUMN = 255; ///< The largest column a point may take.

/** The rule family a floor belongs to; the number is the reason handed to JunctionPositions. */
enum Rule : uint32_t { R1 = 1, R2, R3, R4, R5, R6, R7, R8, R9, TIE };
constexpr const char *RULE_NAMES[] = {"", "R1", "R2", "R3", "R4", "R5", "R6", "R7", "R8", "R9", "tie"};

/** A column as a point's column plus a constant. */
struct Col {
	uint16_t point;
	int off;
};

/** One half piece of a measured S-bend: its tile from the bend's first tile (columns) and from the axis tile before it (lines). */
struct BendPiece {
	int dx, dy;
	TrackBits bits;
	Trackdir trackdir;
	uint8_t units;
};

/** An S-bend shape measured from its raster. */
struct BendShape {
	int lines; ///< Signed.
	bool first_left;
	std::string moves;
	int width; ///< Columns from the first half piece's tile to the closing axis piece's tile.
	int last_dx; ///< The last half piece's column from the first's.
	int units; ///< Units of all the half pieces.
	std::vector<BendPiece> pieces;
};

/** x_a - x_b >= k. */
struct Floor {
	uint16_t a, b;
	int k;
	uint32_t reason;
};

/** One segment of an edge, with its columns: `start` is the column of its first tile, `end` the column of the next segment's first tile (a Run to the out port ends one past E). */
struct Seg {
	JunctionBlockSegmentKind kind;
	Col start, end;
	int line_before, line_after;
	int shape; ///< Index into the shapes, for an S-bend.
	uint16_t point;
};

struct EdgeGeom {
	std::vector<Seg> segs;
	Col start, end;
	int start_line, end_line;
};

/** Pieces of one edge on one line: a span of equal pieces from `lo` to `hi`, or a single piece. */
struct Item {
	int edge; ///< An edge index, or edges.size() + i for prio i.
	int line;
	Col lo, hi;
	TrackBits lo_bits, hi_bits;
	bool crossed; ///< A piece of an S-bend that a bridge passes over (R4 handles it, not R7).
};

/** A curve of a path: its position is 2 * column(point) + k units. */
struct Curve {
	uint16_t point;
	int k;
	JunctionCurveHand hand;
	int lines; ///< The S-bend it belongs to.
};

int CeilDiv(int a, int b)
{
	return a >= 0 ? (a + b - 1) / b : -((-a) / b);
}

TrackBits BitsOf(Trackdir trackdir)
{
	TrackBits bits;
	bits.Set(TrackdirToTrack(trackdir));
	return bits;
}

/** The least column difference of two unrelated pieces on one line, west piece first. */
int GapNeed(TrackBits low, TrackBits high)
{
	const JunctionTileGap gap = JunctionUnrelatedTileGap(low, high);
	return gap.share ? 0 : gap.apart;
}

class BlockGenerator {
public:
	BlockGenerator(const JunctionBlockProgram &program, const JunctionSpec &spec) : program(program), spec(spec) {}

	bool Run(JunctionBlock &block);
	const std::string &Error() const { return error; }

private:
	const JunctionBlockProgram &program;
	const JunctionSpec &spec;
	std::string error;
	std::string constant_failure;
	std::vector<BendShape> shapes;
	std::vector<EdgeGeom> geoms;
	std::vector<int> node_line;
	std::vector<std::vector<int>> node_edges;
	std::vector<Floor> floors;
	std::vector<Item> items;
	std::vector<std::tuple<int, int, int>> crossed; ///< Edge, segment and line of an S-bend a bridge passes over.

	bool Fail(std::string message)
	{
		error = std::move(message);
		return false;
	}

	int Val(Col col, const std::vector<int> &columns) const { return columns[col.point] + col.off; }
	void AddFloor(Col hi, Col lo, int k, Rule rule);
	void AddTie(Col a, Col b);
	bool Validate();
	int ShapeIndex(int lines);
	bool MeasureShape(int lines);
	bool WalkEdges();
	bool AddStructuralFloors();
	bool AddBridgeFloors();
	bool AddNodeFloors();
	bool AddCurveFloors();
	bool AddPrioFloors();
	void BuildItems();
	bool Solve(const std::vector<Floor> &all, std::vector<int> &columns);
	bool EmitRoute(size_t path, const std::vector<int> &columns, JunctionRoute &route, std::vector<std::pair<uint16_t, size_t>> *spans = nullptr);
	bool BuildOutputs(const JunctionSpec &spec, const std::vector<JunctionRaster> &rasters, const std::vector<std::vector<std::pair<uint16_t, size_t>>> &spans, JunctionBlock &block);
	bool AddBayFloors(const std::vector<int> &columns);
	std::map<int, std::vector<size_t>> Order(const std::vector<int> &columns) const;
	bool Related(const Item &a, const Item &b, const std::vector<int> &columns) const;
	std::vector<std::pair<Item, Item>> UnrelatedPairs(const std::map<int, std::vector<size_t>> &order, const std::vector<int> &columns, bool consecutive_only) const;
};

void BlockGenerator::AddFloor(Col hi, Col lo, int k, Rule rule)
{
	const int need = k - hi.off + lo.off;
	if (hi.point == lo.point) {
		if (need > 0 && constant_failure.empty()) constant_failure = fmt::format("{} floor between {} and {} cannot hold", RULE_NAMES[rule], program.points[hi.point], program.points[lo.point]);
		return;
	}
	floors.push_back({hi.point, lo.point, need, rule});
}

void BlockGenerator::AddTie(Col a, Col b)
{
	AddFloor(a, b, 0, TIE);
	AddFloor(b, a, 0, TIE);
}

bool BlockGenerator::Validate()
{
	const size_t points = program.points.size();
	if (points == 0 || program.east >= points) return Fail("block program has no points or a bad east point");
	if (program.through.size() != program.paths.size()) return Fail("block program: through per path is missing");
	for (const JunctionBlockNode &node : program.nodes) {
		if (node.point >= points) return Fail("block program: node point out of range");
	}
	for (size_t e = 0; e < program.edges.size(); e++) {
		const JunctionBlockEdge &edge = program.edges[e];
		if (edge.segments.empty()) return Fail(fmt::format("block program: edge {} has no segments", e));
		for (const JunctionBlockEnd &end : {edge.from, edge.to}) {
			if (end.node ? end.index >= program.nodes.size() : false) return Fail(fmt::format("block program: edge {} ends at a bad node", e));
		}
		if (!edge.from.node && edge.from.index >= program.in_lines.size()) return Fail(fmt::format("block program: edge {} starts at a bad in port", e));
		if (!edge.to.node && edge.to.index >= program.out_lines.size()) return Fail(fmt::format("block program: edge {} ends at a bad out port", e));
		for (const JunctionBlockSegment &seg : edge.segments) {
			if (seg.kind != JunctionBlockSegmentKind::SBend && seg.point >= points) return Fail(fmt::format("block program: edge {} has a segment to a bad point", e));
			if (seg.kind == JunctionBlockSegmentKind::SBend && seg.lines == 0) return Fail(fmt::format("block program: edge {} has an S-bend of no lines", e));
			if (seg.kind == JunctionBlockSegmentKind::Bridge && seg.crosses >= program.edges.size()) return Fail(fmt::format("block program: edge {} has a bridge over no edge", e));
		}
	}
	for (size_t p = 0; p < program.paths.size(); p++) {
		const std::vector<uint16_t> &path = program.paths[p];
		if (path.empty()) return Fail(fmt::format("block program: path {} is empty", p));
		for (uint16_t e : path) {
			if (e >= program.edges.size()) return Fail(fmt::format("block program: path {} has a bad edge", p));
		}
		if (program.edges[path.front()].from.node || program.edges[path.back()].to.node) return Fail(fmt::format("block program: path {} does not run port to port", p));
		for (size_t j = 0; j + 1 < path.size(); j++) {
			const JunctionBlockEnd &to = program.edges[path[j]].to;
			const JunctionBlockEnd &from = program.edges[path[j + 1]].from;
			if (!to.node || !from.node || to.index != from.index) return Fail(fmt::format("block program: path {} breaks between its edges {} and {}", p, j, j + 1));
		}
	}
	for (const JunctionBlockPrio &prio : program.prios) {
		if (prio.node >= program.nodes.size() || program.nodes[prio.node].split) return Fail("block program: a prio is not at a merge node");
	}
	return true;
}

/** Measure the S-bend of the signed lines from its raster; the hand is the one whose closing piece lands that many lines over. */
bool BlockGenerator::MeasureShape(int lines)
{
	const int halves = 2 * std::abs(lines);
	for (int hand : {1, -1}) {
		const std::string moves = JunctionBendMoves(hand, halves, false);
		const JunctionRaster raster = RasterizeJunctionRoute({{16, 16}, Trackdir::X_SW, moves});
		if (!raster.error.empty() || static_cast<int>(raster.pieces.size()) != halves + 2) continue;
		const JunctionPiece &axis = raster.pieces.front();
		const JunctionPiece &first = raster.pieces[1];
		const JunctionPiece &closing = raster.pieces.back();
		if (closing.trackdir != Trackdir::X_SW || closing.tile.y - axis.tile.y != lines) continue;
		BendShape shape;
		shape.lines = lines;
		shape.first_left = hand > 0;
		shape.moves = moves;
		shape.width = closing.tile.x - first.tile.x;
		shape.last_dx = raster.pieces[halves].tile.x - first.tile.x;
		shape.units = 0;
		for (int i = 1; i <= halves; i++) {
			const JunctionPiece &piece = raster.pieces[i];
			shape.pieces.push_back({piece.tile.x - first.tile.x, piece.tile.y - axis.tile.y, BitsOf(piece.trackdir), piece.trackdir, piece.units});
			shape.units += piece.units;
		}
		shapes.push_back(std::move(shape));
		return true;
	}
	return Fail(fmt::format("no S-bend of {} lines", lines));
}

int BlockGenerator::ShapeIndex(int lines)
{
	for (size_t i = 0; i < shapes.size(); i++) {
		if (shapes[i].lines == lines) return static_cast<int>(i);
	}
	return MeasureShape(lines) ? static_cast<int>(shapes.size()) - 1 : -1;
}

/** Give every segment its columns and lines, and check that the edges meet the nodes and ports on the lines they should. */
bool BlockGenerator::WalkEdges()
{
	using Kind = JunctionBlockSegmentKind;
	geoms.resize(program.edges.size());
	node_edges.assign(program.nodes.size(), {});
	for (size_t e = 0; e < program.edges.size(); e++) {
		const JunctionBlockEdge &edge = program.edges[e];
		EdgeGeom &geom = geoms[e];
		geom.start = edge.from.node ? Col{program.nodes[edge.from.index].point, 0} : Col{0, 0};
		geom.start_line = edge.line;
		if (!edge.from.node && program.in_lines[edge.from.index] != edge.line) return Fail(fmt::format("edge {} does not start on its in port's line", e));
		if (edge.from.node) node_edges[edge.from.index].push_back(static_cast<int>(e));
		if (edge.to.node) node_edges[edge.to.index].push_back(static_cast<int>(e));
		Col cur = geom.start;
		int line = edge.line;
		for (size_t s = 0; s < edge.segments.size(); s++) {
			const JunctionBlockSegment &seg = edge.segments[s];
			const bool last = s + 1 == edge.segments.size();
			Seg out{seg.kind, cur, cur, line, line, -1, seg.point};
			switch (seg.kind) {
				case Kind::Run:
					out.end = {seg.point, 0};
					if (last && edge.to.node && seg.point != program.nodes[edge.to.index].point) return Fail(fmt::format("edge {} does not run to its node", e));
					if (last && !edge.to.node) {
						if (seg.point != program.east) return Fail(fmt::format("edge {} does not run to the east point", e));
						out.end = {seg.point, 1};
					}
					break;

				case Kind::SBend: {
					const int shape = ShapeIndex(seg.lines);
					if (shape < 0) return false;
					out.shape = shape;
					out.end = {cur.point, cur.off + shapes[shape].width};
					out.line_after = line + seg.lines;
					break;
				}

				case Kind::Bridge:
					if (last) return Fail(fmt::format("edge {} ends in a bridge", e));
					out.end = {seg.point, 1};
					break;
			}
			cur = out.end;
			line = out.line_after;
			geom.segs.push_back(out);
		}
		geom.end = cur;
		geom.end_line = line;
		if (!edge.to.node && program.out_lines[edge.to.index] != line) return Fail(fmt::format("edge {} does not end on its out port's line", e));
	}

	node_line.assign(program.nodes.size(), -1);
	for (size_t n = 0; n < program.nodes.size(); n++) {
		const bool split = program.nodes[n].split;
		for (int e : node_edges[n]) {
			const bool incoming = program.edges[e].to.node && program.edges[e].to.index == n;
			const int line = incoming ? geoms[e].end_line : geoms[e].start_line;
			/* A split's tracks all lie on its in track's line at the node; a merge's on its out track's line. */
			const bool defines = split ? incoming : !incoming;
			if (defines) {
				if (node_line[n] >= 0) return Fail(fmt::format("node {} has more than one {} edge", n, split ? "in" : "out"));
				node_line[n] = line;
			}
		}
		if (node_line[n] < 0) return Fail(fmt::format("node {} has no {} edge", n, split ? "in" : "out"));
		for (int e : node_edges[n]) {
			const bool incoming = program.edges[e].to.node && program.edges[e].to.index == n;
			const int line = incoming ? geoms[e].end_line : geoms[e].start_line;
			if (line != node_line[n]) {
				/* A merge's in tracks end on the node's line; a split's out tracks start on it. */
				return Fail(fmt::format("edge {} meets node {} on line {}, not {}", e, n, line, node_line[n]));
			}
		}
	}
	return true;
}

/** R1: port stubs. */
bool BlockGenerator::AddStructuralFloors()
{
	using Kind = JunctionBlockSegmentKind;
	const Col west{0, 0};
	const Col east{program.east, 0};
	for (size_t e = 0; e < program.edges.size(); e++) {
		const JunctionBlockEdge &edge = program.edges[e];
		const EdgeGeom &geom = geoms[e];
		if (!edge.from.node) {
			std::optional<Col> first;
			for (const Seg &seg : geom.segs) {
				if (seg.kind != Kind::Run) {
					first = seg.start;
					break;
				}
			}
			if (!first.has_value() && edge.to.node) first = Col{program.nodes[edge.to.index].point, 0};
			if (first.has_value()) AddFloor(*first, west, 2, R1);
		}
		if (!edge.to.node) {
			std::optional<Col> last;
			for (size_t s = geom.segs.size(); s-- > 0;) {
				if (geom.segs[s].kind != Kind::Run) {
					last = geom.segs[s].end;
					break;
				}
			}
			if (!last.has_value() && edge.from.node) last = Col{program.nodes[edge.from.index].point, 0};
			if (last.has_value()) AddFloor(east, *last, 2, R1);
		}
	}
	return true;
}

/** R2, R3, R4: the room around a bridge and the bridge over a diagonal. */
bool BlockGenerator::AddBridgeFloors()
{
	using Kind = JunctionBlockSegmentKind;
	const TrackBits axis{Track::X};
	for (size_t e = 0; e < program.edges.size(); e++) {
		const EdgeGeom &geom = geoms[e];
		for (size_t s = 0; s < geom.segs.size(); s++) {
			const Seg &bridge = geom.segs[s];
			if (bridge.kind != Kind::Bridge) continue;
			const Col near = bridge.start;
			const Col far{bridge.point, 0};

			size_t before = s;
			while (before > 0 && geom.segs[before - 1].kind == Kind::Run) before--;
			if (before == 0 || geom.segs[before - 1].kind != Kind::SBend) return Fail(fmt::format("edge {}: a bridge needs a bend before it", e));
			AddFloor(near, geom.segs[before - 1].end, 1, R2);

			size_t after = s + 1;
			while (after < geom.segs.size() && geom.segs[after].kind == Kind::Run) after++;
			if (after < geom.segs.size() && geom.segs[after].kind == Kind::SBend) AddFloor(geom.segs[after].start, far, 2, R3);

			const uint16_t crosses = program.edges[e].segments[s].crosses;
			bool found = false;
			for (size_t c = 0; c < geoms[crosses].segs.size(); c++) {
				const Seg &seg = geoms[crosses].segs[c];
				if (seg.kind != Kind::SBend) continue;
				const BendShape &shape = shapes[seg.shape];
				std::optional<BendPiece> west_piece, east_piece;
				for (const BendPiece &piece : shape.pieces) {
					if (seg.line_before + piece.dy != bridge.line_before) continue;
					if (!west_piece.has_value() || piece.dx < west_piece->dx) west_piece = piece;
					if (!east_piece.has_value() || piece.dx > east_piece->dx) east_piece = piece;
				}
				if (!west_piece.has_value()) continue;
				if (found) return Fail(fmt::format("edge {}: its bridge passes over more than one bend of edge {}", e, crosses));
				found = true;
				crossed.emplace_back(crosses, static_cast<int>(c), bridge.line_before);
				AddFloor({seg.start.point, seg.start.off + west_piece->dx}, near, GapNeed(axis, west_piece->bits), R4);
				AddFloor(far, {seg.start.point, seg.start.off + east_piece->dx}, GapNeed(east_piece->bits, axis), R4);
			}
			if (!found) return Fail(fmt::format("edge {}: its bridge has no bend of edge {} under it", e, crosses));
		}
	}
	return true;
}

/** R6: a plain axis tile between a merge or split tile and the next bend of the merged or straight track. */
bool BlockGenerator::AddNodeFloors()
{
	using Kind = JunctionBlockSegmentKind;
	for (size_t n = 0; n < program.nodes.size(); n++) {
		const JunctionBlockNode &node = program.nodes[n];
		for (int e : node_edges[n]) {
			const JunctionBlockEdge &edge = program.edges[e];
			if (!edge.from.node || edge.from.index != n) continue;
			if (node.split && edge.segments.front().kind != Kind::Run) continue; // The diverging leg.
			for (const Seg &seg : geoms[e].segs) {
				if (seg.kind == Kind::Run) continue;
				if (seg.kind == Kind::SBend) AddFloor(seg.start, {node.point, 0}, 2, R6);
				break;
			}
		}
	}
	return true;
}

/** R8: the curve gaps of every path. */
bool BlockGenerator::AddCurveFloors()
{
	using Kind = JunctionBlockSegmentKind;
	for (size_t p = 0; p < program.paths.size(); p++) {
		std::vector<Curve> curves;
		int c = 0;
		for (uint16_t e : program.paths[p]) {
			for (const Seg &seg : geoms[e].segs) {
				if (seg.kind != Kind::SBend) continue;
				const BendShape &shape = shapes[seg.shape];
				const int k = 2 * seg.start.off + c;
				curves.push_back({seg.start.point, k, shape.first_left ? JunctionCurveHand::Left : JunctionCurveHand::Right, shape.lines});
				curves.push_back({seg.start.point, k + shape.units, shape.first_left ? JunctionCurveHand::Right : JunctionCurveHand::Left, shape.lines});
				c += shape.units - 2 * shape.width;
			}
		}
		std::vector<JunctionCurveHand> hands;
		for (const Curve &curve : curves) hands.push_back(curve.hand);
		for (const JunctionCurveGap &gap : JunctionCurveGaps(hands, spec.tl, spec.curve_speed, spec.reference_speed)) {
			const Curve &a = curves[gap.i];
			const Curve &b = curves[gap.j];
			if (a.point == b.point) {
				if (b.k - a.k >= gap.min_gap) continue;
				if (gap.i / 2 == gap.j / 2) return Fail(fmt::format("path {}: the S-bend of {} lines has curves {} units apart, needs {}", p, a.lines, b.k - a.k, gap.min_gap));
				return Fail(fmt::format("path {}: curves {} and {} are fixed {} units apart, need {}", p, gap.i, gap.j, b.k - a.k, gap.min_gap));
			}
			AddFloor({b.point, 0}, {a.point, 0}, CeilDiv(gap.min_gap + a.k - b.k, 2), R8);
		}
	}
	return true;
}

/** R9: prio room on the fixed track. */
bool BlockGenerator::AddPrioFloors()
{
	using Kind = JunctionBlockSegmentKind;
	const int prio_units = (spec.prio_tiles + 1) & ~1;
	for (const JunctionBlockPrio &prio : program.prios) {
		const JunctionBlockNode &node = program.nodes[prio.node];
		int fixed = -1;
		for (int e : node_edges[prio.node]) {
			if (program.edges[e].to.node && program.edges[e].to.index == prio.node && geoms[e].start_line == node_line[prio.node]) fixed = e;
		}
		if (fixed < 0) return Fail(fmt::format("node {} has no fixed track for its prio", prio.node));
		for (const Seg &seg : geoms[fixed].segs) {
			if (seg.kind != Kind::Run) return Fail(fmt::format("the fixed track of node {} has a bend", prio.node));
		}
		const JunctionBlockEdge &edge = program.edges[fixed];
		if (edge.from.node) {
			AddFloor({node.point, 0}, {program.nodes[edge.from.index].point, 0}, prio_units, R9);
		} else {
			AddFloor({node.point, 0}, {0, 0}, prio_units + 2, R9);
		}
	}
	return true;
}

/** Every piece of every edge as items, and the prio rows. */
void BlockGenerator::BuildItems()
{
	using Kind = JunctionBlockSegmentKind;
	const TrackBits axis{Track::X};
	items.clear();
	for (size_t e = 0; e < program.edges.size(); e++) {
		const EdgeGeom &geom = geoms[e];
		for (size_t s = 0; s < geom.segs.size(); s++) {
			const Seg &seg = geom.segs[s];
			const int edge = static_cast<int>(e);
			switch (seg.kind) {
				case Kind::Run:
					items.push_back({edge, seg.line_before, seg.start, {seg.end.point, seg.end.off - 1}, axis, axis, false});
					break;

				case Kind::SBend:
					for (const BendPiece &piece : shapes[seg.shape].pieces) {
						const int line = seg.line_before + piece.dy;
						const bool is_crossed = std::ranges::any_of(crossed, [&](const auto &c) { return std::get<0>(c) == edge && std::get<1>(c) == static_cast<int>(s) && std::get<2>(c) == line; });
						const Col col{seg.start.point, seg.start.off + piece.dx};
						items.push_back({edge, line, col, col, piece.bits, piece.bits, is_crossed});
					}
					break;

				case Kind::Bridge:
					items.push_back({edge, seg.line_before, seg.start, seg.start, axis, axis, false});
					items.push_back({edge, seg.line_before, {seg.end.point, seg.end.off - 1}, {seg.end.point, seg.end.off - 1}, axis, axis, false});
					break;
			}
		}
	}
	const int prio_units = (spec.prio_tiles + 1) & ~1;
	for (size_t i = 0; i < program.prios.size(); i++) {
		const uint16_t point = program.nodes[program.prios[i].node].point;
		items.push_back({static_cast<int>(program.edges.size() + i), program.prios[i].line, {point, -prio_units}, {point, 0}, axis, axis, false});
	}
}

bool BlockGenerator::Solve(const std::vector<Floor> &all, std::vector<int> &columns)
{
	JunctionPositions positions(program.points.size(), 0, MAX_COLUMN);
	auto fail = [&]() {
		std::string reasons;
		for (uint32_t reason : positions.Explain()) {
			if (reason < std::size(RULE_NAMES)) reasons += fmt::format(" {}", RULE_NAMES[reason]);
		}
		return Fail("block floors are infeasible, rules:" + reasons);
	};
	if (!positions.RestrictRange(0, 0, 0)) return fail();
	for (const Floor &floor : all) {
		if (!positions.AddDifference(floor.a, floor.b, floor.k, floor.reason)) return fail();
	}
	columns.resize(program.points.size());
	for (size_t i = 0; i < columns.size(); i++) columns[i] = positions.Lo(i);
	return true;
}

/** The route of a path at the given columns: from its west in-port tile to its east out-port tile. */
bool BlockGenerator::EmitRoute(size_t path, const std::vector<int> &columns, JunctionRoute &route, std::vector<std::pair<uint16_t, size_t>> *spans)
{
	using Kind = JunctionBlockSegmentKind;
	const JunctionBlockEdge &first = program.edges[program.paths[path].front()];
	route.start = {0, static_cast<int16_t>(program.in_lines[first.from.index])};
	route.start_trackdir = Trackdir::X_SW;
	route.moves.clear();
	if (spans != nullptr) spans->clear();
	int last = 0;
	for (uint16_t e : program.paths[path]) {
		if (spans != nullptr) spans->emplace_back(e, route.moves.size());
		for (const Seg &seg : geoms[e].segs) {
			switch (seg.kind) {
				case Kind::Run: {
					const int count = Val(seg.end, columns) - 1 - last;
					if (count < 0) return Fail(fmt::format("path {}: the run of edge {} is too short", path, e));
					route.moves.append(count, 'F');
					last += count;
					break;
				}

				case Kind::SBend:
					if (Val(seg.start, columns) != last + 1) return Fail(fmt::format("path {}: the bend of edge {} does not follow an axis piece", path, e));
					route.moves += shapes[seg.shape].moves;
					last = Val(seg.end, columns);
					break;

				case Kind::Bridge: {
					const int near = Val(seg.start, columns);
					const int middle = Val(seg.end, columns) - 2 - near;
					if (near != last + 1 || middle < 0) return Fail(fmt::format("path {}: the bridge of edge {} does not fit", path, e));
					route.moves += fmt::format("B{}", middle);
					last = near + middle + 1;
					break;
				}
			}
		}
	}
	if (last != columns[program.east]) return Fail(fmt::format("path {} does not end at the east point", path));
	return true;
}

/** The items on each line in west to east order, empty spans left out. */
std::map<int, std::vector<size_t>> BlockGenerator::Order(const std::vector<int> &columns) const
{
	std::map<int, std::vector<size_t>> order;
	for (size_t i = 0; i < items.size(); i++) {
		if (items[i].crossed || Val(items[i].hi, columns) < Val(items[i].lo, columns)) continue;
		order[items[i].line].push_back(i);
	}
	for (auto &[line, list] : order) {
		std::ranges::sort(list, [&](size_t a, size_t b) {
			const int lo_a = Val(items[a].lo, columns), lo_b = Val(items[b].lo, columns);
			if (lo_a != lo_b) return lo_a < lo_b;
			const int hi_a = Val(items[a].hi, columns), hi_b = Val(items[b].hi, columns);
			if (hi_a != hi_b) return hi_a < hi_b;
			return a < b;
		});
	}
	return order;
}

/** Whether the west item a and the east item b are one edge, or two edges meeting at a node, at that node's tile. */
bool BlockGenerator::Related(const Item &a, const Item &b, const std::vector<int> &columns) const
{
	if (a.edge == b.edge) return true;
	const int edges = static_cast<int>(program.edges.size());
	if (a.edge >= edges || b.edge >= edges) return false;
	for (size_t n = 0; n < program.nodes.size(); n++) {
		if (node_line[n] != a.line) continue;
		const bool has_a = std::ranges::find(node_edges[n], a.edge) != node_edges[n].end();
		const bool has_b = std::ranges::find(node_edges[n], b.edge) != node_edges[n].end();
		if (!has_a || !has_b) continue;
		const int tile = columns[program.nodes[n].point];
		if (Val(b.lo, columns) == tile || Val(a.hi, columns) == tile) return true;
	}
	return false;
}

/** The pairs of unrelated items on one line, west item first: consecutive ones, or all of them. */
std::vector<std::pair<Item, Item>> BlockGenerator::UnrelatedPairs(const std::map<int, std::vector<size_t>> &order, const std::vector<int> &columns, bool consecutive_only) const
{
	std::vector<std::pair<Item, Item>> pairs;
	for (const auto &[line, list] : order) {
		for (size_t i = 0; i < list.size(); i++) {
			for (size_t j = i + 1; j < list.size(); j++) {
				if (consecutive_only && j > i + 1) break;
				const Item &a = items[list[i]];
				const Item &b = items[list[j]];
				if (!Related(a, b, columns)) pairs.emplace_back(a, b);
			}
		}
	}
	return pairs;
}

/** R5: from the piece after its last upstream split to the last piece before a merge whose tile holds no other edge, a non-through path has at least 2 TL units. */
bool BlockGenerator::AddBayFloors(const std::vector<int> &columns)
{
	std::map<std::pair<int, int>, std::set<int>> occupied;
	for (const Item &item : items) {
		if (item.edge >= static_cast<int>(program.edges.size())) continue;
		for (int x = Val(item.lo, columns); x <= Val(item.hi, columns); x++) occupied[{x, item.line}].insert(item.edge);
	}
	for (size_t p = 0; p < program.paths.size(); p++) {
		if (program.through[p]) continue;
		JunctionRoute route;
		if (!EmitRoute(p, columns, route)) return false;
		const JunctionRaster raster = RasterizeJunctionRoute(route);
		if (!raster.error.empty()) return Fail(fmt::format("path {}: {}", p, raster.error));
		const std::vector<JunctionPiece> &pieces = raster.pieces;
		std::vector<int> after{pieces[0].units};
		for (size_t i = 1; i < pieces.size(); i++) after.push_back(after.back() + pieces[i].units);
		auto piece_at = [&](uint16_t node) -> std::optional<size_t> {
			for (size_t i = 0; i < pieces.size(); i++) {
				if (pieces[i].tile.x == columns[program.nodes[node].point] && pieces[i].tile.y == node_line[node]) return i;
			}
			return std::nullopt;
		};
		std::optional<uint16_t> split;
		std::optional<size_t> split_piece;
		const std::vector<uint16_t> &path = program.paths[p];
		for (size_t j = 0; j + 1 < path.size(); j++) {
			const uint16_t node = program.edges[path[j]].to.index;
			const std::optional<size_t> index = piece_at(node);
			if (!index.has_value()) return Fail(fmt::format("path {} has no piece at node {}", p, node));
			if (program.nodes[node].split) {
				split = node;
				split_piece = index;
				continue;
			}
			if (!split.has_value()) continue;
			size_t end = *index;
			while (end > *split_piece + 1) {
				end--;
				const JunctionPiece &piece = pieces[end];
				if (piece.on_bridge || occupied[{piece.tile.x, piece.tile.y}].size() != 1) continue;
				break;
			}
			if (end <= *split_piece || pieces[end].on_bridge || occupied[{pieces[end].tile.x, pieces[end].tile.y}].size() != 1) return Fail(fmt::format("path {} has no signal-legal piece before merge {}", p, node));
			const int k_end = after[end] - 2 * columns[program.nodes[node].point];
			const int k_split = after[*split_piece] - 2 * columns[program.nodes[*split].point];
			AddFloor({program.nodes[node].point, 0}, {program.nodes[*split].point, 0}, CeilDiv(2 * spec.tl - k_end + k_split, 2), R5);
		}
	}
	return true;
}

bool BlockGenerator::Run(JunctionBlock &block)
{
	using Kind = JunctionBlockSegmentKind;
	if (!Validate() || !WalkEdges()) return false;

	/* The node ties: an edge that ends in an S-bend at a node ends at the bend's last half piece. */
	for (size_t e = 0; e < program.edges.size(); e++) {
		const JunctionBlockEdge &edge = program.edges[e];
		if (!edge.to.node) continue;
		const Seg &last = geoms[e].segs.back();
		if (last.kind == Kind::SBend) AddTie({program.nodes[edge.to.index].point, 0}, {last.start.point, last.start.off + shapes[last.shape].last_dx});
	}
	if (!AddStructuralFloors() || !AddBridgeFloors() || !AddNodeFloors() || !AddCurveFloors() || !AddPrioFloors()) return false;
	if (!constant_failure.empty()) return Fail(constant_failure);
	BuildItems();

	/* Pass 1: every family but the queue bays and the no-touching floors, which need the pieces. */
	std::vector<int> pass1;
	if (!Solve(floors, pass1)) return false;
	const std::map<int, std::vector<size_t>> order1 = Order(pass1);
	if (!AddBayFloors(pass1)) return false;
	for (const auto &[a, b] : UnrelatedPairs(order1, pass1, true)) AddFloor(b.lo, a.hi, GapNeed(a.hi_bits, b.lo_bits), R7);
	if (!constant_failure.empty()) return Fail(constant_failure);

	/* Pass 2: all floors; the pieces must still stand in the order pass 1 saw, and no unrelated pair may touch. */
	std::vector<int> columns;
	if (!Solve(floors, columns)) return false;
	const std::map<int, std::vector<size_t>> order2 = Order(columns);
	for (const auto &[line, list] : order1) {
		const auto it = order2.find(line);
		if (it == order2.end() || it->second != list) return Fail(fmt::format("block occupancy order changed on line {}", line));
	}
	if (order2.size() != order1.size()) return Fail("block occupancy order changed: a line gained pieces");
	for (const auto &[a, b] : UnrelatedPairs(order2, columns, false)) {
		if (Val(b.lo, columns) - Val(a.hi, columns) < GapNeed(a.hi_bits, b.lo_bits)) return Fail(fmt::format("unrelated tracks touch on line {}", a.line));
	}

	/* Emit. */
	block = JunctionBlock{};
	block.columns = columns;
	std::vector<JunctionRaster> rasters;
	std::vector<std::vector<std::pair<uint16_t, size_t>>> spans(program.paths.size());
	for (size_t p = 0; p < program.paths.size(); p++) {
		JunctionRoute route;
		if (!EmitRoute(p, columns, route, &spans[p])) return false;
		rasters.push_back(RasterizeJunctionRoute(route));
		if (!rasters.back().error.empty()) return Fail(fmt::format("path {}: {}", p, rasters.back().error));
		const JunctionPiece &end = rasters.back().pieces.back();
		const JunctionBlockEdge &last = program.edges[program.paths[p].back()];
		if (end.tile.x != columns[program.east] || end.tile.y != program.out_lines[last.to.index] || end.trackdir != Trackdir::X_SW) return Fail(fmt::format("path {} does not end on its out port", p));
		block.routes.push_back(std::move(route));
	}
	std::set<std::pair<int, int>> tiles;
	int bottom = 0;
	for (const Item &item : items) {
		bottom = std::max(bottom, item.line);
		for (int x = Val(item.lo, columns); x <= Val(item.hi, columns); x++) tiles.insert({x, item.line});
	}
	for (const JunctionRaster &raster : rasters) {
		for (const JunctionPiece &piece : raster.pieces) {
			if (!piece.on_bridge && !tiles.contains({piece.tile.x, piece.tile.y})) return Fail(fmt::format("a route piece at {},{} is outside the program's pieces", piece.tile.x, piece.tile.y));
		}
		AddRasterToLayout(block.layout, raster);
	}
	/* One frame: x the column, y the program line, so the prio rows are the first and last lines; the layout size is the tight bounding box. */
	block.layout.size = {static_cast<int16_t>(columns[program.east] + 1), static_cast<int16_t>(bottom + 1)};
	for (int line : program.in_lines) block.in_ports.push_back({{0, static_cast<int16_t>(line)}, Trackdir::X_SW});
	for (int line : program.out_lines) block.out_ports.push_back({{static_cast<int16_t>(columns[program.east]), static_cast<int16_t>(line)}, Trackdir::X_SW});
	return BuildOutputs(spec, rasters, spans, block);
}

/** The topology, lane paths, track graph, programs, tagged routes and prio spans of the solved block. */
bool BlockGenerator::BuildOutputs(const JunctionSpec &spec, const std::vector<JunctionRaster> &rasters, const std::vector<std::vector<std::pair<uint16_t, size_t>>> &spans, JunctionBlock &block)
{
	using Kind = JunctionBlockSegmentKind;
	block.topology = program.topology;
	std::string why;
	block.paths = ExpandJunctionLanes(block.topology, &why);
	if (block.paths.empty()) return Fail("block topology: " + why);

	/* The in port p is the p-th W slot; the E slots run south to north, so the out port p is the E slot p from the last. */
	const std::vector<JunctionPortSlot> slots = JunctionPortSlots(block.topology);
	std::vector<uint16_t> in_slot, out_slot;
	for (size_t i = 0; i < slots.size(); i++) {
		const JunctionSide side = block.topology.groups[slots[i].group].side;
		if (side == JunctionSide::W) in_slot.push_back(static_cast<uint16_t>(i));
		if (side == JunctionSide::E) out_slot.insert(out_slot.begin(), static_cast<uint16_t>(i));
	}
	bool match = block.paths.size() == program.paths.size() && in_slot.size() == program.in_lines.size() && out_slot.size() == program.out_lines.size();
	for (size_t p = 0; match && p < program.paths.size(); p++) {
		const JunctionBlockEdge &first = program.edges[program.paths[p].front()];
		const JunctionBlockEdge &last = program.edges[program.paths[p].back()];
		match = !first.from.node && !last.to.node && block.paths[p].from == in_slot[first.from.index] && block.paths[p].to == out_slot[last.to.index];
	}
	if (!match) return Fail("block paths do not match the topology");

	/* The graph: nodes and edges as the program has them. */
	JunctionTrackGraph &graph = block.graph;
	for (size_t n = 0; n < program.nodes.size(); n++) {
		JunctionTrackNode node{program.nodes[n].split, 0, 0, JunctionBlockKind::Plain};
		if (node.split) {
			for (const JunctionBlockEdge &edge : program.edges) {
				if (edge.to.node && edge.to.index == n && !edge.from.node) node.slot = in_slot[edge.from.index];
			}
		} else {
			uint16_t at = static_cast<uint16_t>(n);
			for (size_t guard = 0; guard < program.edges.size(); guard++) {
				const auto it = std::ranges::find_if(program.edges, [&](const JunctionBlockEdge &edge) { return edge.from.node && edge.from.index == at; });
				if (it == program.edges.end()) return Fail("block merge feeds no out port");
				if (!it->to.node) {
					node.slot = out_slot[it->to.index];
					break;
				}
				at = it->to.index;
			}
		}
		graph.nodes.push_back(node);
	}
	for (size_t e = 0; e < program.edges.size(); e++) {
		const JunctionBlockEdge &edge = program.edges[e];
		JunctionTrackEdge out;
		if (!edge.from.node && edge.to.node && program.nodes[edge.to.index].split) {
			out.kind = JunctionTrackEdge::Kind::In;
		} else if (edge.from.node && !program.nodes[edge.from.index].split) {
			out.kind = JunctionTrackEdge::Kind::Out;
		} else {
			out.kind = JunctionTrackEdge::Kind::Leg;
		}
		out.from = {edge.from.node, edge.from.node ? edge.from.index : in_slot[edge.from.index]};
		out.to = {edge.to.node, edge.to.node ? edge.to.index : out_slot[edge.to.index]};
		for (size_t p = 0; p < program.paths.size(); p++) {
			if (std::ranges::find(program.paths[p], static_cast<uint16_t>(e)) != program.paths[p].end()) out.paths.push_back(static_cast<uint16_t>(p));
		}
		graph.edges.push_back(std::move(out));
		for (const JunctionBlockSegment &segment : edge.segments) {
			if (segment.kind == Kind::Bridge && segment.crosses != UINT16_MAX) graph.crossings.emplace_back(std::min<uint32_t>(e, segment.crosses), std::max<uint32_t>(e, segment.crosses));
		}
	}
	std::ranges::sort(graph.crossings);
	for (const std::vector<uint16_t> &path : program.paths) graph.path_edges.emplace_back(path.begin(), path.end());
	block.programs.edges.resize(program.edges.size());
	block.programs.copies.resize(program.edges.size());

	/* The tagged routes: each piece belongs to the edge whose moves made it; the start piece to the first edge. */
	for (size_t p = 0; p < program.paths.size(); p++) {
		JunctionTrackRoute route;
		route.path = static_cast<uint16_t>(p);
		route.copy = 0;
		route.pieces = rasters[p].pieces;
		route.tags.push_back({spans[p].front().first, JunctionTrackPieceTag::RUN, 0, 0});
		const std::string &moves = block.routes[p].moves;
		for (size_t s = 0; s < spans[p].size(); s++) {
			const size_t end = s + 1 < spans[p].size() ? spans[p][s + 1].second : moves.size();
			for (size_t i = spans[p][s].second; i < end; i++) {
				if (moves[i] >= '0' && moves[i] <= '9') continue;
				size_t count = 1;
				if (moves[i] == 'B') {
					size_t middle = 0;
					for (size_t j = i + 1; j < end && moves[j] >= '0' && moves[j] <= '9'; j++) middle = middle * 10 + (moves[j] - '0');
					count = middle + 2;
				}
				for (size_t k = 0; k < count; k++) route.tags.push_back({spans[p][s].first, JunctionTrackPieceTag::RUN, 0, 0});
			}
		}
		/* A merge's tile and every piece before it belong to the edge that ends at the merge node, the pieces after it to the edge that leaves (the convention of JunctionRouteEvents); a split's tile belongs to the edge that leaves, as the moves already make it. */
		for (size_t t = 1; t < route.tags.size(); t++) {
			const uint32_t in = route.tags[t - 1].edge, out = route.tags[t].edge;
			if (in == out || !program.edges[in].to.node || program.nodes[program.edges[in].to.index].split) continue;
			const JunctionBlockNode &node = program.nodes[program.edges[in].to.index];
			const size_t from = t >= 3 ? t - 3 : 0;
			const size_t to = std::min(t + 2, route.pieces.size());
			size_t boundary = t;
			for (size_t k = from; k < to; k++) {
				if (!route.pieces[k].on_bridge && route.pieces[k].tile.x == block.columns[node.point] && route.pieces[k].tile.y == program.edges[out].line) boundary = k + 1;
			}
			for (size_t k = std::min(t, boundary); k < std::max(t, boundary); k++) route.tags[k].edge = boundary > t ? in : out;
		}
		if (route.tags.size() != route.pieces.size()) return Fail(fmt::format("path {}: the tags do not cover the pieces", p));
		block.track_routes.push_back(std::move(route));
	}

	/* The prio spans, in the step-bits pattern of the placement. */
	const int even_prio = (spec.prio_tiles + 1) & ~1;
	for (const JunctionBlockPrio &prio : program.prios) {
		const auto fixed = std::ranges::find_if(program.edges, [&](const JunctionBlockEdge &edge) { return edge.to.node && edge.to.index == prio.node && !edge.from.node; });
		if (fixed == program.edges.end()) return Fail("a block prio's merge has no fixed edge");
		JunctionPrioSpan span;
		span.merge = {static_cast<int16_t>(block.columns[program.nodes[prio.node].point]), static_cast<int16_t>(fixed->line)};
		const DiagDirection side = prio.line < fixed->line ? DiagDirection::NW : DiagDirection::SE;
		for (int step = 0; step <= even_prio; step++) {
			const auto [row_bits, lane_bits] = JunctionPrioStepBits(step, side, DiagDirection::SW);
			const int16_t x = static_cast<int16_t>(span.merge.x - step);
			if (lane_bits.Any()) span.tiles.emplace_back(TileIndexDiffC{x, span.merge.y}, lane_bits);
			span.tiles.emplace_back(TileIndexDiffC{x, static_cast<int16_t>(prio.line)}, row_bits);
		}
		block.prio.push_back(std::move(span));
	}
	return true;
}

} // namespace

bool BuildJunctionBlock(const JunctionBlockProgram &program, const JunctionSpec &spec, JunctionBlock &block, std::string *error)
{
	BlockGenerator generator(program, spec);
	const bool built = generator.Run(block);
	if (!built && error != nullptr) *error = generator.Error();
	return built;
}
