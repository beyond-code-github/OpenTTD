/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file planar_layout.cpp The planar layout program (DESIGN-planar §2a). */

#include "../../stdafx.h"

#include "planar_layout.h"

#include "../junction_spec.h"
#include "../junction_track_geometry.h"
#include "../placement/junction_milp.h"
#include "../terrain/junction_facts.h"
#include "../topology/junction_chain.h"

#include "../../3rdparty/fmt/format.h"
#include "../../track_func.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <map>
#include <set>

/** Progress lines of the lazy loop on stderr when PLANAR_DEBUG is set (before safeguards: they forbid stdio). */
static void PlanarTrace(const std::string &line)
{
	std::fputs(line.c_str(), stderr);
}

/** Whether PLANAR_DEBUG asks for the lazy loop's trace (before safeguards: they forbid getenv). */
static const char *PlanarEnv(const char *name)
{
	return std::getenv(name);
}

#include "../../safeguards.h"

namespace {

constexpr int DX[8] = {0, 1, 1, 1, 0, -1, -1, -1};
constexpr int DY[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
constexpr uint32_t NONE = UINT32_MAX;

bool IsAxis(int k) { return k % 2 == 0; }

using Terms = std::vector<std::pair<uint32_t, double>>;

/** A point given as an affine expression of a vertex: (vertex, constant offset). */
struct Anchor {
	uint32_t vertex;
	int dx;
	int dy;
};

/** The tile edge a point on a tile's boundary lies on, as a game DiagDirection in the canonical frame (x east = SW, y south = SE). */
DiagDirection EdgeOf(std::array<int, 2> p, TileIndexDiffC tile)
{
	if (p[0] == 2 * tile.x) return DiagDirection::NE;
	if (p[0] == 2 * tile.x + 2) return DiagDirection::SW;
	if (p[1] == 2 * tile.y) return DiagDirection::NW;
	return DiagDirection::SE;
}

int FloorDiv(int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }

/** The piece of a unit step (axis: 2 units) from a point. */
PlanarPiece MakePiece(std::array<int, 2> from, std::array<int, 2> to, uint8_t heading)
{
	PlanarPiece piece;
	piece.from = from;
	piece.to = to;
	piece.heading = heading;
	piece.tile = {static_cast<int16_t>(FloorDiv(from[0] + to[0], 4)), static_cast<int16_t>(FloorDiv(from[1] + to[1], 4))};
	const DiagDirection a = EdgeOf(from, piece.tile);
	const DiagDirection b = EdgeOf(to, piece.tile);
	if ((a == DiagDirection::NE && b == DiagDirection::SW) || (a == DiagDirection::SW && b == DiagDirection::NE)) {
		piece.bits = TrackBits{Track::X};
	} else if ((a == DiagDirection::NW && b == DiagDirection::SE) || (a == DiagDirection::SE && b == DiagDirection::NW)) {
		piece.bits = TrackBits{Track::Y};
	} else {
		piece.bits = TrackBits{JunctionCornerTrack(a, b)};
	}
	return piece;
}

bool Touches(TrackBits bits, DiagDirection edge)
{
	for (Track t : bits) {
		if (JunctionTrackTouches(t, edge)) return true;
	}
	return false;
}

/** The clash rule kinds: two ground pieces (share and touch rules), a head with anything (same tile or touching), two deck middles (same tile). */
enum ClashKind : uint8_t { CLASH_GROUND, CLASH_EXCLUSIVE, CLASH_TILE, CLASH_KINDS };

/** Whether two pieces of unrelated lines clash under a rule kind. */
bool PiecesClash(const PlanarPiece &p, const PlanarPiece &q, ClashKind kind)
{
	const int dx = q.tile.x - p.tile.x;
	const int dy = q.tile.y - p.tile.y;
	if (dx == 0 && dy == 0) return kind != CLASH_GROUND || !JunctionTracksMayShareTile(p.bits, q.bits);
	if (kind == CLASH_TILE || std::abs(dx) + std::abs(dy) != 1) return false;
	const DiagDirection edge = dx == 1 ? DiagDirection::SW : dx == -1 ? DiagDirection::NE : dy == 1 ? DiagDirection::SE : DiagDirection::NW;
	return Touches(p.bits, edge) && Touches(q.bits, ReverseDiagDir(edge));
}

/** The kind of rule between two pieces, or CLASH_KINDS when none applies (a ground piece under a deck middle). */
ClashKind KindOf(const PlanarPiece &p, const PlanarPiece &q)
{
	using K = PlanarPiece::Kind;
	if (p.kind == K::Head || q.kind == K::Head) return CLASH_EXCLUSIVE;
	if (p.kind == K::Middle && q.kind == K::Middle) return CLASH_TILE;
	if (p.kind == K::Middle || q.kind == K::Middle) return CLASH_KINDS;
	return CLASH_GROUND;
}

/** The projection of a point on a sector's axis (axis * 2 + sign): x, y, x + y, x - y, negated for sign 1. */
int Project(int sector, int x, int y)
{
	const int axis = sector / 2;
	const int v = axis == 0 ? x : axis == 1 ? y : axis == 2 ? x + y : x - y;
	return sector % 2 == 0 ? v : -v;
}

/** The gap table: per kind, heading pair and sector, the least projected distance from the first piece's ends to the second's at which they never clash (built once by enumeration). */
struct GapTable {
	int gap[CLASH_KINDS][8][8][8];

	GapTable()
	{
		const auto valid = [](int x, int y, int k) {
			if ((x + y) % 2 == 0) return false;
			if (k == 2 || k == 6) return (x % 2 + 2) % 2 == 0;
			if (k == 0 || k == 4) return (x % 2 + 2) % 2 == 1;
			return true;
		};
		const auto piece_at = [](int x, int y, int k) {
			const int len = IsAxis(k) ? 2 : 1;
			return MakePiece({x, y}, {x + len * DX[k], y + len * DY[k]}, static_cast<uint8_t>(k));
		};
		for (int kind = 0; kind < CLASH_KINDS; kind++) {
			for (int a = 0; a < 8; a++) {
				for (int b = 0; b < 8; b++) {
					for (int s = 0; s < 8; s++) {
						int worst = -100;
						for (int ax = 0; ax <= 1; ax++) {
							const int ay = 1 - ax;
							if (!valid(ax, ay, a)) continue;
							const PlanarPiece p = piece_at(ax, ay, a);
							for (int bx = -8; bx <= 9; bx++) {
								for (int by = -8; by <= 9; by++) {
									if (!valid(bx, by, b)) continue;
									const PlanarPiece q = piece_at(bx, by, b);
									if (!PiecesClash(p, q, static_cast<ClashKind>(kind))) continue;
									const int pmax = std::max(Project(s, p.from[0], p.from[1]), Project(s, p.to[0], p.to[1]));
									const int qmin = std::min(Project(s, q.from[0], q.from[1]), Project(s, q.to[0], q.to[1]));
									worst = std::max(worst, qmin - pmax);
								}
							}
						}
						this->gap[kind][a][b][s] = worst + 1;
					}
				}
			}
		}
	}
};

const GapTable &Gaps()
{
	static const GapTable table;
	return table;
}

/** The least 45-degree turn count from one heading to another. */
int TurnCount(PlanarHeading from, PlanarHeading to)
{
	const int d = (static_cast<int>(to) - static_cast<int>(from) + 8) % 8;
	return std::min(d, 8 - d);
}

/** The program and its lazy loop. */
class Model {
public:
	Model(const PlanarTopology &topology, const JunctionSpec &spec, const PlanarSolveOptions &options) : t(topology), spec(spec), options(options), S(options.bound) {}

	bool Run(const PlanarSolveState *base, PlanarSolveState &state, PlanarGeometry &geometry, std::string *why);

private:
	struct Vertex {
		uint32_t x, y;
		bool track;
		uint32_t m = NONE, h = NONE, p = NONE;
	};
	struct Segment {
		uint32_t line;
		uint32_t from, to;
		std::array<uint32_t, 8> d;
		std::array<uint32_t, 8> w;
		uint32_t u, z;
	};
	struct Turn {
		uint32_t in, out; ///< Global segment ids.
		uint32_t tl, tr;
	};

	const PlanarTopology &t;
	const JunctionSpec &spec;
	const PlanarSolveOptions &options;
	const int S;
	JunctionMilp milp;
	std::vector<uint32_t> binaries;
	uint32_t width = NONE, height = NONE;
	std::vector<Vertex> vertices;
	std::vector<Segment> segments;
	std::vector<Turn> turns;
	std::map<std::pair<uint32_t, uint32_t>, uint32_t> turn_of; ///< (in segment, out segment) to turn.
	std::vector<std::vector<uint32_t>> chain; ///< Per line, vertices.
	std::vector<std::vector<uint32_t>> segs; ///< Per line, global segment ids.
	std::vector<uint32_t> node_vertex;
	std::vector<uint32_t> crossing_vertex;
	std::vector<uint8_t> window; ///< Per line, the headings its segments may take (bit k: heading k).
	std::vector<uint32_t> over; ///< Per crossing, o_c.
	std::vector<std::vector<uint32_t>> crossing_pos; ///< Per line, the chain position of each of its events.
	std::vector<std::pair<uint32_t, uint32_t>> merged_pairs; ///< (line, event index of the first of two consecutive crossings) per merged-deck binary.
	std::vector<uint32_t> merged; ///< Per merged pair, its binary.
	std::vector<std::array<uint32_t, 2>> deck_y; ///< Per crossing, the linearised "a over a diagonal" and "b over a diagonal".
	std::set<std::tuple<int, uint32_t, uint32_t, int>> clash_seen;
	std::vector<double> values;

	uint32_t var_count = 0;

	uint32_t Var(double lo, double hi, double cost, bool integer, bool binary = false)
	{
		const uint32_t v = this->milp.AddVariable(lo, hi, cost, integer);
		this->var_count = v + 1;
		if (binary) this->binaries.push_back(v);
		return v;
	}
	/** Add a row, a variable named twice taking the sum of its coefficients (HiGHS refuses duplicate entries). */
	void Row(const Terms &terms, double lo, double hi)
	{
		std::map<uint32_t, double> merged;
		for (const auto &[v, coef] : terms) merged[v] += coef;
		Terms out;
		for (const auto &[v, coef] : merged) {
			if (coef != 0) out.emplace_back(v, coef);
		}
		this->milp.AddRow(out, lo, hi);
	}
	uint32_t NewVertex(bool track);
	uint32_t NewSegment(uint32_t line, uint32_t from, uint32_t to);
	uint32_t NewTurn(uint32_t in, uint32_t out, bool inherit);
	void Build();
	Terms DiagTerms(uint32_t s, double coef) const
	{
		Terms terms;
		for (int k = 1; k < 8; k += 2) terms.emplace_back(this->segments[s].d[k], coef);
		return terms;
	}
	int Val(uint32_t v) const { return static_cast<int>(std::lround(this->values[v])); }
	int HeadingOf(uint32_t s) const
	{
		for (int k = 0; k < 8; k++) {
			if (this->Val(this->segments[s].d[k]) == 1) return k;
		}
		return 0;
	}
	bool ApplyLazy(const PlanarLazyRow &row, bool fixed);
	void SeparationRows(const std::array<Anchor, 2> &a, const std::array<Anchor, 2> &b, const std::array<int, 8> &gaps, std::array<uint32_t, 2> conditions, int sector);
	std::array<Anchor, 2> SegmentEnds(uint32_t s) const { return {Anchor{this->segments[s].from, 0, 0}, Anchor{this->segments[s].to, 0, 0}}; }
	std::array<Anchor, 2> PrioEnds(uint32_t node, int heading, int side) const;
	int PrioSide(uint32_t node, uint32_t &prio_leg) const;
	void Extract(PlanarGeometry &geometry) const;
	std::vector<PlanarLazyRow> Detect(const PlanarGeometry &geometry, bool fixed) const;
	int ChooseSector(const std::array<Anchor, 2> &a, const std::array<Anchor, 2> &b, const std::array<int, 8> &gaps) const;
	std::array<int, 2> At(const Anchor &a) const { return {this->Val(this->vertices[a.vertex].x) + a.dx, this->Val(this->vertices[a.vertex].y) + a.dy}; }
	int RouteOfLineIsPrio(uint32_t l) const
	{
		for (uint32_t r : this->t.lines[l].routes) {
			if (this->t.routes[r].prio) return 1;
		}
		return 0;
	}
	std::vector<std::vector<uint32_t>> PathSegments() const;
};

uint32_t Model::NewVertex(bool track)
{
	Vertex v;
	v.x = this->Var(0, 2 * S, 0, true);
	v.y = this->Var(0, 2 * S, 0, true);
	v.track = track;
	if (track) {
		v.m = this->Var(-2 * S, 2 * S, 0, true);
		v.h = this->Var(-S, S, 0, true);
		v.p = this->Var(0, 1, 0, true);
		/* X + Y = 2m + 1 (a tile-edge midpoint); X = 2h + p (p = 1: a north or south edge). */
		this->Row({{v.x, 1}, {v.y, 1}, {v.m, -2}}, 1, 1);
		this->Row({{v.x, 1}, {v.h, -2}, {v.p, -1}}, 0, 0);
	}
	/* Family 9: inside the box. */
	this->vertices.push_back(v);
	return static_cast<uint32_t>(this->vertices.size() - 1);
}

uint32_t Model::NewSegment(uint32_t line, uint32_t from, uint32_t to)
{
	Segment s;
	s.line = line;
	s.from = from;
	s.to = to;
	const Money rp = this->spec.prices.rail_piece;
	Terms one, dx, dy, u;
	s.u = this->Var(0, 2 * S, 0, true);
	s.z = this->Var(0, 1, 0, true, true);
	for (int k = 0; k < 8; k++) {
		s.d[k] = this->Var(0, (this->window[line] >> k & 1) ? 1 : 0, 0, true, true);
		/* Objective: rail_piece / 2 per axis unit, rail_piece per diagonal unit (one half piece). */
		s.w[k] = this->Var(0, 2 * S, IsAxis(k) ? static_cast<double>(rp) / 2 : static_cast<double>(rp), true);
		one.emplace_back(s.d[k], 1);
		this->Row({{s.w[k], 1}, {s.d[k], -2.0 * S}}, -JunctionMilp::INF, 0);
		dx.emplace_back(s.w[k], -DX[k]);
		dy.emplace_back(s.w[k], -DY[k]);
		u.emplace_back(s.w[k], -1);
	}
	this->Row(one, 1, 1);
	/* Family 1: exact displacement. */
	dx.emplace_back(this->vertices[to].x, 1);
	dx.emplace_back(this->vertices[from].x, -1);
	this->Row(dx, 0, 0);
	dy.emplace_back(this->vertices[to].y, 1);
	dy.emplace_back(this->vertices[from].y, -1);
	this->Row(dy, 0, 0);
	u.emplace_back(s.u, 1);
	this->Row(u, 0, 0);
	/* z: the segment has length (u >= z, u <= 2S z). */
	this->Row({{s.u, 1}, {s.z, -2.0 * S}}, -JunctionMilp::INF, 0);
	this->Row({{s.u, 1}, {s.z, -1}}, 0, JunctionMilp::INF);
	/* Family 2: parity of the track ends. */
	for (uint32_t v : {from, to}) {
		if (!this->vertices[v].track) continue;
		this->Row({{this->vertices[v].p, 1}, {s.d[2], 1}, {s.d[6], 1}}, -JunctionMilp::INF, 1);
		this->Row({{this->vertices[v].p, 1}, {s.d[0], -1}, {s.d[4], -1}}, 0, JunctionMilp::INF);
	}
	this->segments.push_back(s);
	return static_cast<uint32_t>(this->segments.size() - 1);
}

uint32_t Model::NewTurn(uint32_t in, uint32_t out, bool inherit)
{
	const Segment &a = this->segments[in];
	const Segment &b = this->segments[out];
	Turn turn{in, out, this->Var(0, 1, 0, false), this->Var(0, 1, 0, false)};
	this->Row({{turn.tl, 1}, {turn.tr, 1}}, -JunctionMilp::INF, 1);
	for (int k = 0; k < 8; k++) {
		const int l = (k + 7) % 8, r = (k + 1) % 8;
		/* Family 3: no 90-degree corner. */
		this->Row({{b.d[k], 1}, {a.d[l], -1}, {a.d[k], -1}, {a.d[r], -1}}, -JunctionMilp::INF, 0);
		/* Family 4: a direction change is a left or right curve, exactly. */
		this->Row({{a.d[k], 1}, {b.d[k], -1}, {turn.tl, -1}, {turn.tr, -1}}, -JunctionMilp::INF, 0);
		this->Row({{turn.tl, 1}, {a.d[k], 1}, {b.d[l], -1}}, -JunctionMilp::INF, 1);
		this->Row({{turn.tr, 1}, {a.d[k], 1}, {b.d[r], -1}}, -JunctionMilp::INF, 1);
		/* Family 5: a zero-length segment keeps the direction. */
		if (inherit) {
			this->Row({{b.d[k], 1}, {a.d[k], -1}, {b.z, -1}}, -JunctionMilp::INF, 0);
			this->Row({{a.d[k], 1}, {b.d[k], -1}, {b.z, -1}}, -JunctionMilp::INF, 0);
		}
	}
	this->turns.push_back(turn);
	const uint32_t id = static_cast<uint32_t>(this->turns.size() - 1);
	this->turn_of[{in, out}] = id;
	return id;
}

int Model::PrioSide(uint32_t node, uint32_t &prio_leg) const
{
	/* A merge where exactly one leg carries a prio route (D301): the prio leg is straight and its element lies opposite the joiner (-1: left of travel, +1: right). */
	const PlanarNode &n = this->t.nodes[node];
	prio_leg = NONE;
	if (n.IsSplit()) return 0;
	const int p0 = this->RouteOfLineIsPrio(n.in[0]), p1 = this->RouteOfLineIsPrio(n.in[1]);
	if (p0 == p1) return 0;
	prio_leg = p0 ? 0 : 1;
	return p0 ? -1 : 1;
}

void Model::Build()
{
	const PlanarTopology &t = this->t;
	this->width = this->Var(1, S, static_cast<double>(this->spec.prices.rail_piece), true);
	this->height = this->Var(1, S, static_cast<double>(this->spec.prices.rail_piece), true);
	/* Heading windows: every route path of a line turns from its entry heading to its exit heading the short way; its segments keep to that arc and one heading beyond each end (room for an S-bend). */
	this->window.assign(t.lines.size(), 0xFF);
	for (const PlanarRoute &route : t.routes) {
		for (const auto &path : route.paths) {
			const int h_in = static_cast<int>(t.ports[t.lines[path.front()].from.index].heading);
			const int h_out = static_cast<int>(t.ports[t.lines[path.back()].to.index].heading);
			const int d = (h_out - h_in + 8) % 8;
			const int sign = d <= 4 ? 1 : -1;
			const int r = std::min(d, 8 - d);
			uint8_t mask = 0;
			for (int i = -1; i <= r + 1; i++) mask |= 1 << ((h_in + sign * i + 16) % 8);
			if (d == 4) mask = 0xFF;
			for (uint32_t l : path) {
				this->window[l] &= mask;
			}
		}
	}
	for (size_t n = 0; n < t.nodes.size(); n++) this->node_vertex.push_back(this->NewVertex(true));
	for (size_t c = 0; c < t.crossings.size(); c++) this->crossing_vertex.push_back(this->NewVertex(false));

	/* Chains (DESIGN-planar §2a): start, then per stretch r + 2 bend vertices and the event. */
	std::vector<uint32_t> port_vertex(t.lines.size(), NONE);
	std::vector<std::vector<bool>> is_bend(t.lines.size());
	this->chain.assign(t.lines.size(), {});
	this->segs.assign(t.lines.size(), {});
	this->crossing_pos.assign(t.lines.size(), {});
	for (uint32_t l = 0; l < t.lines.size(); l++) {
		const PlanarLine &line = t.lines[l];
		const auto &path = t.routes[line.routes.front()].paths.front();
		const PlanarHeading h_in = t.ports[t.lines[path.front()].from.index].heading;
		const PlanarHeading h_out = t.ports[t.lines[path.back()].to.index].heading;
		const int bends = TurnCount(h_in, h_out) + 1; // one spare bend per stretch: the first cut §2a names (D297)
		std::vector<uint32_t> &c = this->chain[l];
		if (line.from.port) {
			port_vertex[l] = this->NewVertex(true);
			c.push_back(port_vertex[l]);
		} else {
			c.push_back(this->node_vertex[line.from.index]);
		}
		is_bend[l].push_back(false);
		const size_t stretches = t.events[l].size() + 1;
		for (size_t k = 0; k < stretches; k++) {
			for (int b = 0; b < bends; b++) {
				c.push_back(this->NewVertex(true));
				is_bend[l].push_back(true);
			}
			if (k + 1 < stretches) {
				this->crossing_pos[l].push_back(static_cast<uint32_t>(c.size()));
				c.push_back(this->crossing_vertex[t.events[l][k]]);
			} else if (line.to.port) {
				port_vertex[l] = this->NewVertex(true);
				c.push_back(port_vertex[l]);
			} else {
				c.push_back(this->node_vertex[line.to.index]);
			}
			is_bend[l].push_back(false);
		}
		for (size_t i = 0; i + 1 < c.size(); i++) this->segs[l].push_back(this->NewSegment(l, c[i], c[i + 1]));
		/* Segments at a node, a port or a crossing have length; turns at bend vertices; straight through a crossing. */
		for (size_t i = 0; i < this->segs[l].size(); i++) {
			const uint32_t s = this->segs[l][i];
			if (!is_bend[l][i] || !is_bend[l][i + 1]) this->Row({{this->segments[s].u, 1}}, 1, JunctionMilp::INF);
			if (i == 0) continue;
			const uint32_t prev = this->segs[l][i - 1];
			if (is_bend[l][i]) {
				const uint32_t turn = this->NewTurn(prev, s, true);
				/* A bend vertex that does not turn starts a zero-length segment, unless the next vertex is an event: the spare bends of a stretch gather at its end (a canonical form; no layout is lost). */
				if (is_bend[l][i + 1]) this->Row({{this->segments[s].z, 1}, {this->turns[turn].tl, -1}, {this->turns[turn].tr, -1}}, -JunctionMilp::INF, 0);
			} else {
				for (int k = 0; k < 8; k++) this->Row({{this->segments[prev].d[k], 1}, {this->segments[s].d[k], -1}}, 0, 0);
			}
		}
	}

	/* Family 9: every vertex inside the box. */
	for (const Vertex &v : this->vertices) {
		this->Row({{v.x, 1}, {this->width, -2}}, -JunctionMilp::INF, 0);
		this->Row({{v.y, 1}, {this->height, -2}}, -JunctionMilp::INF, 0);
	}

	/* Family 8: ports. The port line's end segment has the port heading and at least 4 units (the boundary tile and the signal tile); port vertices on their side, a port's lines adjacent, the two ports of a side 2 (gap + 1) apart (the notation). */
	std::map<PlanarSide, std::vector<std::pair<uint32_t, uint32_t>>> side_lines; // (port, line) in reading order
	for (uint32_t p = 0; p < t.ports.size(); p++) {
		const PlanarPort &port = t.ports[p];
		const bool along_x = port.side == PlanarSide::N || port.side == PlanarSide::S;
		const int right = along_x ? -DY[static_cast<int>(port.heading)] : DX[static_cast<int>(port.heading)];
		std::vector<uint32_t> lines = port.lines;
		if (right < 0) std::ranges::reverse(lines);
		for (uint32_t l : lines) side_lines[port.side].emplace_back(p, l);
		for (uint32_t l : port.lines) {
			const uint32_t s = port.in ? this->segs[l].front() : this->segs[l].back();
			this->Row({{this->segments[s].d[static_cast<int>(port.heading)], 1}}, 1, 1);
			this->Row({{this->segments[s].u, 1}}, 4, JunctionMilp::INF);
			const Vertex &v = this->vertices[port_vertex[l]];
			switch (port.side) {
				case PlanarSide::W: this->Row({{v.x, 1}}, 0, 0); break;
				case PlanarSide::E: this->Row({{v.x, 1}, {this->width, -2}}, 0, 0); break;
				case PlanarSide::N: this->Row({{v.y, 1}}, 0, 0); break;
				case PlanarSide::S: this->Row({{v.y, 1}, {this->height, -2}}, 0, 0); break;
			}
		}
	}
	for (const auto &[side, list] : side_lines) {
		const bool along_x = side == PlanarSide::N || side == PlanarSide::S;
		const int gap = along_x ? this->spec.branch_gap : this->spec.ml_gap;
		for (size_t i = 1; i < list.size(); i++) {
			const Vertex &a = this->vertices[port_vertex[list[i - 1].second]];
			const Vertex &b = this->vertices[port_vertex[list[i].second]];
			const int step = list[i - 1].first == list[i].first ? 2 : 2 * (gap + 1);
			this->Row({{along_x ? b.x : b.y, 1}, {along_x ? a.x : a.y, -1}}, step, step);
		}
	}

	/* Family 10: nodes. Legs within 45 degrees of the trunk, the two legs differing, the diverging leg on its embedding side; the stretch next to the node at least 2 units. */
	const auto stretch_terms = [&](uint32_t l, bool first) {
		Terms terms;
		const std::vector<uint32_t> &c = this->chain[l];
		const size_t end = first ? (this->crossing_pos[l].empty() ? c.size() - 1 : this->crossing_pos[l].front()) : c.size() - 1;
		const size_t begin = first ? 0 : (this->crossing_pos[l].empty() ? 0 : this->crossing_pos[l].back());
		for (size_t i = begin; i < end; i++) terms.emplace_back(this->segments[this->segs[l][i]].u, 1);
		return terms;
	};
	const uint32_t n_prio = this->spec.prio_tiles + (this->spec.prio_tiles & 1);
	for (uint32_t n = 0; n < t.nodes.size(); n++) {
		const PlanarNode &node = t.nodes[n];
		std::array<uint32_t, 2> turn_ids;
		if (node.IsSplit()) {
			const uint32_t trunk = this->segs[node.in[0]].back();
			for (int i = 0; i < 2; i++) {
				turn_ids[i] = this->NewTurn(trunk, this->segs[node.out[i]].front(), false);
				this->Row(stretch_terms(node.out[i], true), 2, JunctionMilp::INF);
			}
			this->Row({{this->turns[turn_ids[0]].tr, 1}}, 0, 0);
			this->Row({{this->turns[turn_ids[1]].tl, 1}}, 0, 0);
			this->Row({{this->turns[turn_ids[0]].tl, 1}, {this->turns[turn_ids[1]].tr, 1}}, 1, JunctionMilp::INF);
		} else {
			const uint32_t trunk = this->segs[node.out[0]].front();
			for (int i = 0; i < 2; i++) {
				turn_ids[i] = this->NewTurn(this->segs[node.in[i]].back(), trunk, false);
				this->Row(stretch_terms(node.in[i], false), 2, JunctionMilp::INF);
			}
			/* A leg arriving from the left of travel heads to the right of the trunk and turns left into it; mirrored for the right leg. */
			this->Row({{this->turns[turn_ids[0]].tr, 1}}, 0, 0);
			this->Row({{this->turns[turn_ids[1]].tl, 1}}, 0, 0);
			this->Row({{this->turns[turn_ids[0]].tl, 1}, {this->turns[turn_ids[1]].tr, 1}}, 1, JunctionMilp::INF);
			/* Family 14: the prio leg is straight into the merge, axis, for n_prio + 1 tiles. */
			uint32_t prio_leg;
			if (this->PrioSide(n, prio_leg) != 0) {
				const Turn &turn = this->turns[turn_ids[prio_leg]];
				this->Row({{turn.tl, 1}, {turn.tr, 1}}, 0, 0);
				const uint32_t s = this->segs[node.in[prio_leg]].back();
				this->Row(this->DiagTerms(s, 1), 0, 0);
				this->Row({{this->segments[s].u, 1}}, 2.0 * (n_prio + 1), JunctionMilp::INF);
			}
		}
	}

	/* Families 11 and 12: crossings. o_c picks the over line (D307); consecutive crossings of one line may share a merged deck (binary per pair). */
	this->over.assign(t.crossings.size(), NONE);
	this->deck_y.assign(t.crossings.size(), {NONE, NONE});
	for (uint32_t c = 0; c < t.crossings.size(); c++) this->over[c] = this->Var(0, 1, 0, true, true);
	std::map<std::pair<uint32_t, uint32_t>, uint32_t> merged_before; // (line, event index) to the merged binary of (event - 1, event)
	for (uint32_t l = 0; l < t.lines.size(); l++) {
		for (size_t e = 0; e + 1 < t.events[l].size(); e++) {
			const uint32_t m = this->Var(0, 1, 0, true, true);
			this->merged_pairs.emplace_back(l, static_cast<uint32_t>(e));
			this->merged.push_back(m);
			merged_before[{l, static_cast<uint32_t>(e + 1)}] = m;
		}
	}
	/* The segment of a line into one of its crossings. */
	const auto seg_in = [&](uint32_t l, uint32_t c) {
		const auto &ev = t.events[l];
		const size_t k = std::ranges::find(ev, c) - ev.begin();
		return this->segs[l][this->crossing_pos[l][k] - 1];
	};
	/* Append coef * (line l is over at c): o_c for line a, 1 - o_c for line b; returns the constant part (on the row's left side). */
	const auto over_of = [&](uint32_t l, uint32_t c, double coef, Terms &terms) {
		if (t.crossings[c].a == l) {
			terms.emplace_back(this->over[c], coef);
			return 0.0;
		}
		terms.emplace_back(this->over[c], -coef);
		return coef;
	};
	const Money bridge1 = this->spec.prices.bridge ? this->spec.prices.bridge(1) : Money(0);
	const Money bridge2 = this->spec.prices.bridge ? this->spec.prices.bridge(2) : Money(0);
	const double extra_middle = static_cast<double>(bridge2 - bridge1 - this->spec.prices.rail_piece);
	constexpr double M_HEAD = 8; // the largest head floor (3 + 1 + 2) plus the diagonal term
	for (uint32_t c = 0; c < t.crossings.size(); c++) {
		const uint32_t a = t.crossings[c].a, b = t.crossings[c].b;
		const uint32_t o = this->over[c];
		/* A main line (prio) never goes over: doubling (D309) is not built. */
		const int prio_a = this->RouteOfLineIsPrio(a), prio_b = this->RouteOfLineIsPrio(b);
		if (prio_a && !prio_b) this->Row({{o, 1}}, 0, 0);
		if (prio_b && !prio_a) this->Row({{o, 1}}, 1, 1);
		const uint32_t a_in = seg_in(a, c), b_in = seg_in(b, c);
		/* Over line axis at C; the two lines not parallel (the segments through C are straight, so the in segment stands for both). */
		Terms ta = this->DiagTerms(a_in, 1);
		ta.emplace_back(o, 1);
		this->Row(ta, -JunctionMilp::INF, 1);
		Terms tb = this->DiagTerms(b_in, 1);
		tb.emplace_back(o, -1);
		this->Row(tb, -JunctionMilp::INF, 0);
		for (int k = 0; k < 8; k++) this->Row({{this->segments[a_in].d[k], 1}, {this->segments[b_in].d[k], 1}, {this->segments[b_in].d[(k + 4) % 8], 1}}, -JunctionMilp::INF, 1);
		for (const auto &[l, under_in] : {std::pair(a, b_in), std::pair(b, a_in)}) {
			const auto &ev = t.events[l];
			const uint32_t e = static_cast<uint32_t>(std::ranges::find(ev, c) - ev.begin());
			const uint32_t pos = this->crossing_pos[l][e];
			for (int side = 0; side < 2; side++) {
				/* Head floor: u >= 3 + (under line diagonal) when l is over; the approach side also holds the axis piece the deck starts from (RasterizeJunctionRoute's B follows an axis piece), unless the deck merges from the crossing before. */
				Terms terms{{this->segments[this->segs[l][side == 0 ? pos - 1 : pos]].u, 1}};
				for (auto &[v, coef] : this->DiagTerms(under_in, -1)) terms.emplace_back(v, coef);
				/* u - diag - M over >= 3 - M, over being o or 1 - o (its constant moved right). */
				const double constant = over_of(l, c, -M_HEAD, terms);
				double lo = 3 - M_HEAD - constant;
				if (side == 0) {
					lo += 2;
					const auto it = merged_before.find({l, e});
					if (it != merged_before.end()) terms.emplace_back(it->second, 2);
				}
				this->Row(terms, lo, JunctionMilp::INF);
			}
			/* The under line has length on both sides of C. */
			this->Row({{this->segments[this->segs[l][pos - 1]].u, 1}}, 1, JunctionMilp::INF);
			this->Row({{this->segments[this->segs[l][pos]].u, 1}}, 1, JunctionMilp::INF);
		}
		/* Objective: a deck over a diagonal under line has a second middle tile (y >= over + diag - 1). */
		for (const auto &[l, under_in] : {std::pair(a, b_in), std::pair(b, a_in)}) {
			const uint32_t y = this->Var(0, 1, extra_middle, false);
			Terms terms{{y, 1}};
			for (auto &[v, coef] : this->DiagTerms(under_in, -1)) terms.emplace_back(v, coef);
			const double constant = over_of(l, c, -1, terms);
			this->Row(terms, -1 - constant, JunctionMilp::INF);
			this->deck_y[c][l == a ? 0 : 1] = y;
		}
	}
	for (size_t i = 0; i < this->merged_pairs.size(); i++) {
		const auto [l, e] = this->merged_pairs[i];
		const uint32_t m = this->merged[i];
		const uint32_t c1 = t.events[l][e], c2 = t.events[l][e + 1];
		const uint32_t p1 = this->crossing_pos[l][e], p2 = this->crossing_pos[l][e + 1];
		Terms dist, curves;
		for (uint32_t k = p1; k < p2; k++) dist.emplace_back(this->segments[this->segs[l][k]].u, 1);
		for (uint32_t k = p1 + 1; k < p2; k++) {
			const Turn &turn = this->turns[this->turn_of.at({this->segs[l][k - 1], this->segs[l][k]})];
			curves.emplace_back(turn.tl, 1);
			curves.emplace_back(turn.tr, 1);
		}
		/* m <= over at c1 and at c2. */
		for (uint32_t c : {c1, c2}) {
			Terms terms{{m, 1}};
			const double constant = over_of(l, c, -1, terms);
			this->Row(terms, -JunctionMilp::INF, -constant);
		}
		/* Merged: no curve between. */
		Terms none = curves;
		none.emplace_back(m, 2.0 * (p2 - p1));
		this->Row(none, -JunctionMilp::INF, 2.0 * (p2 - p1));
		/* Both over and not merged: a curve between (curves >= over1 + over2 - 1 - m). */
		Terms some = curves;
		double constant = over_of(l, c1, -1, some);
		constant += over_of(l, c2, -1, some);
		some.emplace_back(m, 1);
		this->Row(some, -1 - constant, JunctionMilp::INF);
		/* Merged deck length: back(c1) + D + front(c2) - 4 <= 2 max_middle, back and front 3 + under diagonal. */
		const auto other = [&](uint32_t c) { return t.crossings[c].a == l ? t.crossings[c].b : t.crossings[c].a; };
		Terms length = dist;
		for (auto &[v, coef] : this->DiagTerms(seg_in(other(c1), c1), 1)) length.emplace_back(v, coef);
		for (auto &[v, coef] : this->DiagTerms(seg_in(other(c2), c2), 1)) length.emplace_back(v, coef);
		const double big = 4.0 * S + 8;
		length.emplace_back(m, big);
		this->Row(length, -JunctionMilp::INF, 2.0 * this->spec.max_bridge_middle - 2 + big);
	}
	/* Family 15: a non-prio route's run from its last split or port to a merge is at least TL tiles. */
	for (const PlanarRoute &route : t.routes) {
		if (route.prio) continue;
		for (const auto &path : route.paths) {
			Terms run;
			for (size_t i = 0; i < path.size(); i++) {
				const PlanarLine &line = t.lines[path[i]];
				if (!line.from.port && t.nodes[line.from.index].IsSplit()) run.clear();
				for (uint32_t s : this->segs[path[i]]) run.emplace_back(this->segments[s].u, 1);
				if (!line.to.port && !t.nodes[line.to.index].IsSplit()) this->Row(run, 2.0 * this->spec.tl, JunctionMilp::INF);
			}
		}
	}
}

std::array<Anchor, 2> Model::PrioEnds(uint32_t node, int heading, int side) const
{
	/* The element's row beside the prio line: from n_prio + 1 tiles before the merge vertex to it, shifted one tile to the element's side. */
	const int n_prio = this->spec.prio_tiles + (this->spec.prio_tiles & 1);
	const int sx = side * -DY[heading], sy = side * DX[heading];
	const uint32_t v = this->node_vertex[node];
	return {Anchor{v, -2 * (n_prio + 1) * DX[heading] + 2 * sx, -2 * (n_prio + 1) * DY[heading] + 2 * sy}, Anchor{v, 2 * sx, 2 * sy}};
}

void Model::SeparationRows(const std::array<Anchor, 2> &a, const std::array<Anchor, 2> &b, const std::array<int, 8> &gaps, std::array<uint32_t, 2> conditions, int sector)
{
	/* proj(b end) - proj(a end) >= gap, for every end pair, in the chosen sector; relaxed when a condition (a segment's heading) no longer holds. */
	const double big = 4.0 * S + 32;
	std::array<uint32_t, 8> pick{};
	if (sector < 0) {
		Terms one;
		for (int s = 0; s < 8; s++) {
			pick[s] = this->Var(0, 1, 0, true, true);
			one.emplace_back(pick[s], 1);
		}
		this->Row(one, 1, 1);
	}
	for (int s = 0; s < 8; s++) {
		if (sector >= 0 && s != sector) continue;
		const int axis = s / 2;
		const double sign = s % 2 == 0 ? 1 : -1;
		const double cx = sign * (axis == 1 ? 0 : 1), cy = sign * (axis == 0 ? 0 : axis == 3 ? -1 : 1);
		for (const Anchor &ea : a) {
			for (const Anchor &eb : b) {
				Terms terms{{this->vertices[eb.vertex].x, cx}, {this->vertices[eb.vertex].y, cy}, {this->vertices[ea.vertex].x, -cx}, {this->vertices[ea.vertex].y, -cy}};
				double lo = gaps[s] - (cx * (eb.dx - ea.dx) + cy * (eb.dy - ea.dy));
				double relax = 0;
				for (uint32_t cond : conditions) {
					if (cond == NONE) continue;
					terms.emplace_back(cond, -big);
					relax += big;
				}
				if (sector < 0) {
					terms.emplace_back(pick[s], -big);
					relax += big;
				}
				this->Row(terms, lo - relax, JunctionMilp::INF);
			}
		}
	}
}

int Model::ChooseSector(const std::array<Anchor, 2> &a, const std::array<Anchor, 2> &b, const std::array<int, 8> &gaps) const
{
	int best = 0, best_need = INT_MAX;
	for (int s = 0; s < 8; s++) {
		int need = INT_MIN;
		for (const Anchor &ea : a) {
			for (const Anchor &eb : b) {
				const auto pa = this->At(ea), pb = this->At(eb);
				need = std::max(need, gaps[s] - (Project(s, pb[0], pb[1]) - Project(s, pa[0], pa[1])));
			}
		}
		if (need < best_need) {
			best_need = need;
			best = s;
		}
	}
	return best;
}

bool Model::ApplyLazy(const PlanarLazyRow &row, bool fixed)
{
	using Kind = PlanarLazyRow::Kind;
	switch (row.kind) {
		case Kind::Clash:
		case Kind::Prio: {
			const uint32_t sb = this->segs[row.line_b][row.segment_b];
			std::array<Anchor, 2> a;
			uint32_t cond_a = NONE;
			const int heading_a = row.heading_a, heading_b = row.heading_b;
			if (row.kind == Kind::Clash) {
				const uint32_t sa = this->segs[row.line_a][row.segment_a];
				a = this->SegmentEnds(sa);
				cond_a = this->segments[sa].d[heading_a];
			} else {
				uint32_t prio_leg;
				const int side = this->PrioSide(row.line_a, prio_leg);
				const uint32_t s = this->segs[this->t.nodes[row.line_a].in[prio_leg]].back();
				a = this->PrioEnds(row.line_a, heading_a, side);
				cond_a = this->segments[s].d[heading_a];
			}
			const ClashKind kind = static_cast<ClashKind>(row.rule);
			std::array<int, 8> gaps;
			for (int s = 0; s < 8; s++) gaps[s] = Gaps().gap[kind][heading_a][heading_b][s];
			int sector = row.sector;
			if (sector < 0 && fixed) sector = this->ChooseSector(a, this->SegmentEnds(sb), gaps);
			this->SeparationRows(a, this->SegmentEnds(sb), gaps, {cond_a, this->segments[sb].d[heading_b]}, sector);
			return true;
		}
		case Kind::Obstacle: {
			const uint32_t s = this->segs[row.line_a][row.segment_a];
			const int heading = row.heading_a;
			const bool along_y = row.sector / 2 == 1;
			const bool low = row.sector % 2 == 0;
			const double big = 4.0 * S + 8;
			const int edge = 2 * (along_y ? row.tile.y : row.tile.x);
			for (uint32_t v : {this->segments[s].from, this->segments[s].to}) {
				const uint32_t coord = along_y ? this->vertices[v].y : this->vertices[v].x;
				if (low) {
					this->Row({{coord, 1}, {this->segments[s].d[heading], big}}, -JunctionMilp::INF, edge + big);
				} else {
					this->Row({{coord, 1}, {this->segments[s].d[heading], -big}}, edge + 2 - big, JunctionMilp::INF);
				}
			}
			return true;
		}
		case Kind::Curve: {
			Terms terms;
			for (const auto &[l, i] : row.span) terms.emplace_back(this->segments[this->segs[l][i]].u, 1);
			for (const auto &[turn, hand] : row.curves) terms.emplace_back(hand == 0 ? this->turns[turn].tl : this->turns[turn].tr, -row.gap);
			this->Row(terms, row.gap * (1.0 - static_cast<double>(row.curves.size())), JunctionMilp::INF);
			return true;
		}
	}
	return false;
}

void Model::Extract(PlanarGeometry &geometry) const
{
	const PlanarTopology &t = this->t;
	geometry = {};
	geometry.width = this->Val(this->width);
	geometry.height = this->Val(this->height);
	for (const Vertex &v : this->vertices) geometry.points.push_back({this->Val(v.x), this->Val(v.y)});
	geometry.chains = this->chain;
	geometry.node_vertex = this->node_vertex;
	for (uint32_t l = 0; l < t.lines.size(); l++) {
		std::vector<uint8_t> h;
		for (uint32_t s : this->segs[l]) h.push_back(static_cast<uint8_t>(this->HeadingOf(s)));
		geometry.headings.push_back(std::move(h));
	}
	for (uint32_t c = 0; c < t.crossings.size(); c++) geometry.a_over.push_back(this->Val(this->over[c]) == 1);
	/* Decks: per line, its over crossings, consecutive ones merged where the merged binary says so. */
	for (uint32_t l = 0; l < t.lines.size(); l++) {
		const auto &events = t.events[l];
		const auto is_over = [&](size_t e) { const uint32_t c = events[e]; return (t.crossings[c].a == l) == geometry.a_over[c]; };
		const auto under_diag = [&](size_t e) {
			const uint32_t c = events[e];
			const uint32_t other = t.crossings[c].a == l ? t.crossings[c].b : t.crossings[c].a;
			const auto &ev = t.events[other];
			const size_t k = std::ranges::find(ev, c) - ev.begin();
			return geometry.headings[other][this->crossing_pos[other][k] - 1] % 2 == 1 ? 1 : 0;
		};
		for (size_t e = 0; e < events.size(); e++) {
			if (!is_over(e)) continue;
			size_t last = e;
			while (last + 1 < events.size()) {
				const auto it = std::ranges::find(this->merged_pairs, std::pair<uint32_t, uint32_t>(l, static_cast<uint32_t>(last)));
				if (it == this->merged_pairs.end() || this->Val(this->merged[it - this->merged_pairs.begin()]) != 1) break;
				last++;
			}
			geometry.decks.push_back({l, this->crossing_pos[l][e], this->crossing_pos[l][last], 3 + under_diag(e), 3 + under_diag(last)});
			e = last;
		}
	}
}

std::vector<std::vector<uint32_t>> Model::PathSegments() const
{
	std::vector<std::vector<uint32_t>> out;
	for (const PlanarRoute &route : this->t.routes) {
		for (const auto &path : route.paths) {
			std::vector<uint32_t> s;
			for (uint32_t l : path) s.insert(s.end(), this->segs[l].begin(), this->segs[l].end());
			out.push_back(std::move(s));
		}
	}
	return out;
}

std::vector<PlanarLazyRow> Model::Detect(const PlanarGeometry &geometry, bool fixed) const
{
	using Kind = PlanarLazyRow::Kind;
	const PlanarTopology &t = this->t;
	std::vector<PlanarLazyRow> out;
	const std::vector<std::vector<PlanarPiece>> raster = RasterPlanarLines(geometry);

	/* Family 13: clashes between pieces of different lines (not two lines meeting at a node vertex both touch). */
	std::map<std::pair<int, int>, std::vector<std::pair<uint32_t, uint32_t>>> by_tile;
	for (uint32_t l = 0; l < raster.size(); l++) {
		for (uint32_t i = 0; i < raster[l].size(); i++) by_tile[{raster[l][i].tile.x, raster[l][i].tile.y}].emplace_back(l, i);
	}
	std::set<uint32_t> node_points;
	for (uint32_t v : geometry.node_vertex) node_points.insert(v);
	const auto at_shared_node = [&](uint32_t la, const PlanarPiece &p, uint32_t lb, const PlanarPiece &q) {
		for (uint32_t n = 0; n < t.nodes.size(); n++) {
			const auto &pt = geometry.points[geometry.node_vertex[n]];
			const auto incident = [&](uint32_t l) { return (!t.lines[l].from.port && t.lines[l].from.index == n) || (!t.lines[l].to.port && t.lines[l].to.index == n); };
			if (!incident(la) || !incident(lb)) continue;
			if ((p.from == pt || p.to == pt) && (q.from == pt || q.to == pt)) return true;
		}
		return false;
	};
	std::set<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>> pairs;
	for (const auto &[tile, list] : by_tile) {
		for (int dx = -1; dx <= 1; dx++) {
			for (int dy = -1; dy <= 1; dy++) {
				if (std::abs(dx) + std::abs(dy) > 1) continue;
				const auto it = by_tile.find({tile.first + dx, tile.second + dy});
				if (it == by_tile.end()) continue;
				for (const auto &[la, ia] : list) {
					for (const auto &[lb, ib] : it->second) {
						if (la >= lb) continue;
						const PlanarPiece &p = raster[la][ia], &q = raster[lb][ib];
						const ClashKind kind = KindOf(p, q);
						if (kind == CLASH_KINDS || !PiecesClash(p, q, kind) || at_shared_node(la, p, lb, q)) continue;
						if (!pairs.insert({la, p.segment, lb, q.segment}).second) continue;
						if (PlanarEnv("PLANAR_DEBUG") != nullptr) {
							const auto ca = geometry.chains[la], cb = geometry.chains[lb];
							const bool shared = ca[p.segment] == cb[q.segment] || ca[p.segment] == cb[q.segment + 1] || ca[p.segment + 1] == cb[q.segment] || ca[p.segment + 1] == cb[q.segment + 1];
							PlanarTrace(fmt::format("  clash {}:{} {} ({},{} k{} h{}) x {}:{} {} ({},{} k{} h{}) rule {}{}\n", t.lines[la].name, p.segment, geometry.headings[la][p.segment], p.tile.x, p.tile.y, static_cast<int>(p.kind), p.heading,
								t.lines[lb].name, q.segment, geometry.headings[lb][q.segment], q.tile.x, q.tile.y, static_cast<int>(q.kind), q.heading, static_cast<int>(kind), shared ? " SHARED" : ""));
						}
						PlanarLazyRow row{};
				row.kind = Kind::Clash;
						row.line_a = la;
						row.segment_a = p.segment;
						row.line_b = lb;
						row.segment_b = q.segment;
						row.rule = kind;
						row.heading_a = geometry.headings[la][p.segment];
						row.heading_b = geometry.headings[lb][q.segment];
						out.push_back(row);
					}
				}
			}
		}
	}

	/* Family 14: the prio element's row tiles hold no other line. */
	const int n_prio = this->spec.prio_tiles + (this->spec.prio_tiles & 1);
	for (uint32_t n = 0; n < t.nodes.size(); n++) {
		uint32_t prio_leg;
		const int side = this->PrioSide(n, prio_leg);
		if (side == 0) continue;
		const uint32_t l = t.nodes[n].in[prio_leg];
		const int heading = geometry.headings[l].back();
		const std::array<int, 2> v = geometry.points[geometry.node_vertex[n]];
		const int sx = side * -DY[heading], sy = side * DX[heading];
		std::set<std::pair<int, int>> row_tiles;
		for (int s = 0; s <= n_prio; s++) {
			const std::array<int, 2> from = {v[0] - 2 * (s + 1) * DX[heading] + 2 * sx, v[1] - 2 * (s + 1) * DY[heading] + 2 * sy};
			const PlanarPiece piece = MakePiece(from, {from[0] + 2 * DX[heading], from[1] + 2 * DY[heading]}, static_cast<uint8_t>(heading));
			row_tiles.insert({piece.tile.x, piece.tile.y});
		}
		for (uint32_t lb = 0; lb < raster.size(); lb++) {
			for (const PlanarPiece &q : raster[lb]) {
				if (q.kind == PlanarPiece::Kind::Middle || !row_tiles.contains({q.tile.x, q.tile.y})) continue;
				if (!pairs.insert({UINT32_MAX - n, 0, lb, q.segment}).second) continue;
				if (PlanarEnv("PLANAR_DEBUG") != nullptr) PlanarTrace(fmt::format("  prio node {} (prio line {} h{} side {}) x {}:{} at {},{} k{}\n", n, t.lines[l].name, heading, side, t.lines[lb].name, q.segment, q.tile.x, q.tile.y, static_cast<int>(q.kind)));
				PlanarLazyRow row{};
				row.kind = Kind::Prio;
				row.line_a = n;
				row.line_b = lb;
				row.segment_b = q.segment;
				row.rule = CLASH_TILE; // the signal pass needs the row tiles free of track, nothing more
				row.heading_a = static_cast<uint8_t>(heading);
				row.heading_b = geometry.headings[lb][q.segment];
				out.push_back(row);
			}
		}
	}

	/* Families 6 and 7: curve gaps along every route path (JunctionCurveGaps on the solved hands). */
	if (this->spec.curve_limit) {
		const auto paths = this->PathSegments();
		for (const auto &path : paths) {
			std::vector<JunctionCurveHand> hands;
			std::vector<int> pos;
			std::vector<std::pair<uint32_t, uint8_t>> curves; // (turn, hand)
			std::vector<size_t> where; // index into path of the segment after the curve
			int at = 0;
			for (size_t i = 0; i < path.size(); i++) {
				if (i > 0) {
					const auto it = this->turn_of.find({path[i - 1], path[i]});
					if (it != this->turn_of.end()) {
						const Turn &turn = this->turns[it->second];
						const int hl = this->Val(turn.tl), hr = this->Val(turn.tr);
						if (hl == 1 || hr == 1) {
							hands.push_back(hl == 1 ? JunctionCurveHand::Left : JunctionCurveHand::Right);
							pos.push_back(at);
							curves.emplace_back(it->second, hl == 1 ? 0 : 1);
							where.push_back(i);
						}
					}
				}
				at += this->Val(this->segments[path[i]].u);
			}
			for (const JunctionCurveGap &gap : JunctionCurveGaps(hands, this->spec.tl, this->spec.curve_speed, this->spec.reference_speed)) {
				if (pos[gap.j] - pos[gap.i] >= gap.min_gap) continue;
				if (PlanarEnv("PLANAR_DEBUG") != nullptr) PlanarTrace(fmt::format("  curve gap {}-{} of {}: {} < {}\n", gap.i, gap.j, hands.size(), pos[gap.j] - pos[gap.i], gap.min_gap));
				PlanarLazyRow row{};
				row.kind = Kind::Curve;
				row.gap = gap.min_gap;
				for (size_t k = where[gap.i]; k < where[gap.j]; k++) {
					const uint32_t s = path[k];
					const uint32_t l = this->segments[s].line;
					row.span.emplace_back(l, static_cast<uint32_t>(std::ranges::find(this->segs[l], s) - this->segs[l].begin()));
				}
				for (size_t k = gap.i; k <= gap.j; k++) row.curves.push_back(curves[k]);
				out.push_back(row);
			}
		}
	}

	/* Family 18: terrain facts on the rasterised result. */
	if (this->options.facts != nullptr) {
		const JunctionFacts &facts = *this->options.facts;
		std::map<std::pair<int, int>, TrackBits> ground;
		for (const auto &pieces : raster) {
			for (const PlanarPiece &p : pieces) {
				if (p.kind == PlanarPiece::Kind::Ground) ground[{p.tile.x, p.tile.y}] |= p.bits;
			}
		}
		std::set<std::tuple<uint32_t, uint32_t, int, int>> seen;
		for (uint32_t l = 0; l < raster.size(); l++) {
			for (const PlanarPiece &p : raster[l]) {
				const int x = p.tile.x + this->options.offset.x, y = p.tile.y + this->options.offset.y;
				bool bad;
				if (p.kind == PlanarPiece::Kind::Ground) {
					bad = !facts.Fits(x, y, ground[{p.tile.x, p.tile.y}]);
				} else if (p.kind == PlanarPiece::Kind::Middle) {
					bad = !facts.Spannable(x, y);
				} else {
					int entry, exit;
					bad = !facts.Straight(x, y, p.bits == TrackBits{Track::X} ? Axis::X : Axis::Y, entry, exit);
				}
				if (!bad || !seen.insert({l, p.segment, p.tile.x, p.tile.y}).second) continue;
				PlanarLazyRow row{};
				row.kind = Kind::Obstacle;
				row.line_a = l;
				row.segment_a = p.segment;
				row.tile = p.tile;
				const int heading = geometry.headings[l][p.segment];
				row.heading_a = static_cast<uint8_t>(heading);
				const bool along_y = heading == 2 || heading == 6;
				const std::array<int, 2> &a = geometry.points[geometry.chains[l][p.segment]];
				const std::array<int, 2> &b = geometry.points[geometry.chains[l][p.segment + 1]];
				const int mid = along_y ? a[1] + b[1] : a[0] + b[0];
				const int centre = 2 * (2 * (along_y ? p.tile.y : p.tile.x) + 1);
				row.sector = static_cast<int8_t>((along_y ? 2 : 0) + (mid <= centre ? 0 : 1));
				out.push_back(row);
			}
		}
	}
	(void)fixed;
	return out;
}

bool Model::Run(const PlanarSolveState *base, PlanarSolveState &state, PlanarGeometry &geometry, std::string *why)
{
	this->Build();
	const bool fixed = this->options.mode == PlanarSolveOptions::Mode::Fixed;
	state = {};
	if (base != nullptr) {
		for (const PlanarLazyRow &row : base->lazy) {
			this->ApplyLazy(row, false);
			state.lazy.push_back(row);
		}
		std::vector<std::pair<uint32_t, double>> start;
		for (uint32_t v : base->binaries) {
			if (v >= base->values.size()) continue;
			if (fixed) {
				this->Row({{v, 1}}, std::round(base->values[v]), std::round(base->values[v]));
			} else {
				start.emplace_back(v, std::round(base->values[v]));
			}
		}
		if (!start.empty()) this->milp.SetStart(std::move(start));
	}
	const bool debug = PlanarEnv("PLANAR_DEBUG") != nullptr;
	for (int round = 0; round < this->options.max_rounds; round++) {
		state.solves++;
		const auto t0 = std::chrono::steady_clock::now();
		const JunctionMilp::Status status = this->milp.Solve();
		if (debug) PlanarTrace(fmt::format("planar round {}: status {}, {:.0f} ms, {} vars, {} binaries, objective {:.0f}\n", round, static_cast<int>(status), std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count(), this->var_count, this->binaries.size(), status == JunctionMilp::Status::Optimal ? this->milp.Objective() : 0.0));
		if (status != JunctionMilp::Status::Optimal) {
			if (why != nullptr) *why = fmt::format("the planar program is {} after {} lazy rows", status == JunctionMilp::Status::Infeasible ? "infeasible" : "unsolved", state.lazy.size());
			return false;
		}
		this->values.clear();
		for (uint32_t v = 0; v < this->var_count; v++) this->values.push_back(this->milp.Value(v));
		this->Extract(geometry);
		std::vector<PlanarLazyRow> found = this->Detect(geometry, fixed);
		if (debug) {
			int kinds[4] = {};
			for (const PlanarLazyRow &row : found) kinds[static_cast<int>(row.kind)]++;
			PlanarTrace(fmt::format("  found clash {} curve {} obstacle {} prio {}; box {} x {}\n", kinds[0], kinds[1], kinds[2], kinds[3], geometry.width, geometry.height));
		}
		if (found.empty()) {
			state.values = this->values;
			state.binaries = this->binaries;
			return true;
		}
		for (PlanarLazyRow &row : found) {
			if (fixed && (row.kind == PlanarLazyRow::Kind::Clash || row.kind == PlanarLazyRow::Kind::Prio)) {
				/* Hover: no new binaries; the sector the geometry is nearest to. */
				this->ApplyLazy(row, true);
				continue;
			}
			this->ApplyLazy(row, false);
			state.lazy.push_back(row);
		}
		if (!fixed) {
			std::vector<std::pair<uint32_t, double>> start;
			for (uint32_t v : this->binaries) {
				if (v < this->values.size()) start.emplace_back(v, std::round(this->values[v]));
			}
			this->milp.SetStart(std::move(start));
		}
	}
	if (why != nullptr) *why = fmt::format("the planar lazy loop did not settle in {} rounds", this->options.max_rounds);
	return false;
}

} // namespace

std::vector<std::vector<PlanarPiece>> RasterPlanarLines(const PlanarGeometry &geometry)
{
	std::vector<std::vector<PlanarPiece>> out(geometry.chains.size());
	for (uint32_t l = 0; l < geometry.chains.size(); l++) {
		const std::vector<uint32_t> &c = geometry.chains[l];
		std::array<int, 2> p = geometry.points[c.front()];
		std::array<int, 2> start = p;
		int units = 0;
		uint32_t piece_segment = 0;
		std::vector<int> unit_at; // per piece, units from the line's start to the piece's start
		std::vector<int> vertex_unit(c.size(), 0);
		int along = 0;
		for (uint32_t s = 0; s + 1 < c.size(); s++) {
			vertex_unit[s] = along;
			const int k = geometry.headings[l][s];
			const std::array<int, 2> &q = geometry.points[c[s + 1]];
			const int len = std::max(std::abs(q[0] - p[0]), std::abs(q[1] - p[1]));
			for (int i = 0; i < len; i++) {
				if (units == 0) piece_segment = s;
				p = {p[0] + DX[k], p[1] + DY[k]};
				units++;
				along++;
				if (((p[0] + p[1]) & 1) == 1) {
					PlanarPiece piece = MakePiece(start, p, static_cast<uint8_t>(k));
					piece.segment = piece_segment;
					unit_at.push_back(along - units);
					out[l].push_back(piece);
					start = p;
					units = 0;
				}
			}
		}
		vertex_unit[c.size() - 1] = along;
		for (const PlanarDeck &deck : geometry.decks) {
			if (deck.line != l) continue;
			const int lo = vertex_unit[deck.first] - deck.back, hi = vertex_unit[deck.last] + deck.front;
			for (size_t i = 0; i < out[l].size(); i++) {
				const int a = unit_at[i];
				if (a < lo || a >= hi) continue;
				out[l][i].kind = (a == lo || a + 2 == hi) ? PlanarPiece::Kind::Head : PlanarPiece::Kind::Middle;
			}
		}
	}
	return out;
}

bool SolvePlanarLayout(const PlanarTopology &topology, const JunctionSpec &spec, const PlanarSolveOptions &options, const PlanarSolveState *base, PlanarSolveState &state, PlanarGeometry &geometry, std::string *why)
{
	Model model(topology, spec, options);
	return model.Run(base, state, geometry, why);
}
