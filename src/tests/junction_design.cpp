/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_design.cpp Hidden design harness: builds a layout from a text description and runs the rule checker. */

#include "../stdafx.h"

#include "../3rdparty/catch2/catch.hpp"

#include "../junction_generator.h"
#include "../junction_layout.h"
#include "../junction_nodes.h"
#include "../junction_route.h"
#include "../junction_router.h"
#include "../junction_topology.h"
#include "../junction_rules.h"
#include "../junction_rules_internal.h"
#include "../slope_func.h"
#include "../track_func.h"
#include "../core/format.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>

#include "../safeguards.h"

static Trackdir ParseTrackdir(const std::string &s)
{
	static const std::pair<const char *, Trackdir> names[] = {
		{"X_NE", Trackdir::X_NE}, {"Y_SE", Trackdir::Y_SE}, {"Upper_E", Trackdir::Upper_E}, {"Lower_E", Trackdir::Lower_E},
		{"Left_S", Trackdir::Left_S}, {"Right_S", Trackdir::Right_S}, {"X_SW", Trackdir::X_SW}, {"Y_NW", Trackdir::Y_NW},
		{"Upper_W", Trackdir::Upper_W}, {"Lower_W", Trackdir::Lower_W}, {"Left_N", Trackdir::Left_N}, {"Right_N", Trackdir::Right_N},
	};
	for (const auto &[n, td] : names) if (s == n) return td;
	FAIL("unknown trackdir " << s);
	return Trackdir::X_NE;
}

static Track ParseTrack(const std::string &s)
{
	static const std::pair<const char *, Track> names[] = {
		{"X", Track::X}, {"Y", Track::Y}, {"Upper", Track::Upper}, {"Lower", Track::Lower}, {"Left", Track::Left}, {"Right", Track::Right},
	};
	for (const auto &[n, t] : names) if (s == n) return t;
	FAIL("unknown track " << s);
	return Track::X;
}

static SignalType ParseSignalType(const std::string &s)
{
	if (s == "block") return SignalType::Block;
	if (s == "entry") return SignalType::Entry;
	if (s == "exit") return SignalType::Exit;
	if (s == "combo") return SignalType::Combo;
	if (s == "path") return SignalType::Path;
	if (s == "pathoneway") return SignalType::PathOneWay;
	FAIL("unknown signal type " << s);
	return SignalType::Block;
}

struct CircuitDepot { int x; int y; const char *dir; };
struct CircuitWaypoint { const char *name; int x; int y; const char *axis; };
struct CircuitExtras { std::vector<CircuitWaypoint> waypoints; int box[4]; };

static void WriteLayoutJson(const JunctionLayout &layout, const char *path, const std::vector<CircuitDepot> &depots = {}, const CircuitExtras *extras = nullptr)
{
	std::string json = JunctionLayoutToJson(layout);
	std::ofstream out(path);
	json.resize(json.size() - 2); /* Drop the closing "}\n" so depots can follow. */
	out << json;
	if (!depots.empty()) {
		out << ",\n\"depots\":[";
		bool first = true;
		for (const auto &d : depots) {
			out << (first ? "" : ",") << "{\"x\":" << d.x << ",\"y\":" << d.y << ",\"dir\":\"" << d.dir << "\"}";
			first = false;
		}
		out << "]";
	}
	if (extras != nullptr) {
		out << ",\n\"waypoints\":[";
		bool first = true;
		for (const auto &wp : extras->waypoints) {
			out << (first ? "" : ",") << "{\"name\":\"" << wp.name << "\",\"x\":" << wp.x << ",\"y\":" << wp.y << ",\"axis\":\"" << wp.axis << "\"}";
			first = false;
		}
		out << "],\n\"junction\":[" << extras->box[0] << "," << extras->box[1] << "," << extras->box[2] << "," << extras->box[3] << "]";
	}
	out << "}\n";
}

TEST_CASE("Junction design harness", "[.design]")
{
	const char *path = std::getenv("JUNCTION_DESIGN");
	REQUIRE(path != nullptr);
	std::ifstream in(path);
	REQUIRE(in.good());

	JunctionSpec spec = MakeTestJunctionSpec();
	JunctionLayout layout{};
	std::vector<JunctionRaster> rasters;
	std::string line;
	while (std::getline(in, line)) {
		std::istringstream ls(line);
		std::string kind;
		if (!(ls >> kind) || kind[0] == '#') continue;
		if (kind == "spec") {
			int tl, sd, speed;
			ls >> tl >> sd >> speed;
			spec.tl = tl;
			spec.sd = sd;
			spec.reference_speed = speed;
		} else if (kind == "route") {
			int16_t x, y;
			std::string td, moves;
			{ int a, b; ls >> a >> b >> td >> moves; x = a; y = b; }
			JunctionRaster r = RasterizeJunctionRoute({{x, y}, ParseTrackdir(td), moves});
			if (!r.error.empty()) std::cout << "ROUTE ERROR (" << line << "): " << r.error << "\n";
			std::cout << "route " << td << " " << moves << " ends at " << r.pieces.back().tile.x << "," << r.pieces.back().tile.y << "\n";
			AddRasterToLayout(layout, r);
			if (std::getenv("JUNCTION_PIECES") != nullptr) {
				std::cout << "  pieces of route " << rasters.size() << ":";
				for (size_t i = 0; i < r.pieces.size(); i++) std::cout << " " << i << "@" << r.pieces[i].tile.x << "," << r.pieces[i].tile.y << (r.pieces[i].on_bridge ? "b" : "");
				std::cout << "\n";
			}
			rasters.push_back(r);
		} else if (kind == "rsig") {
			int ri, pi;
			std::string type;
			ls >> ri >> pi >> type;
			const JunctionPiece &pc = rasters.at(ri).pieces.at(pi);
			layout.signals.push_back({pc.tile, pc.trackdir, ParseSignalType(type), SignalVariant::Electric});
		} else if (kind == "track") {
			int16_t x, y;
			std::string t;
			{ int a, b; ls >> a >> b; x = a; y = b; }
			TrackBits bits{};
			while (ls >> t) bits.Set(ParseTrack(t));
			bool found = false;
			for (auto &jt : layout.tracks) {
				if (jt.offset.x == x && jt.offset.y == y) { jt.tracks |= bits; found = true; }
			}
			if (!found) layout.tracks.push_back({{x, y}, bits});
		} else if (kind == "signal") {
			int16_t x, y;
			std::string td, type;
			{ int a, b; ls >> a >> b >> td >> type; x = a; y = b; }
			layout.signals.push_back({{x, y}, ParseTrackdir(td), ParseSignalType(type), SignalVariant::Electric});
		}
	}
	int min_x = 1 << 20, min_y = 1 << 20;
	for (const auto &jt : layout.tracks) { min_x = std::min<int>(min_x, jt.offset.x); min_y = std::min<int>(min_y, jt.offset.y); }
	NormaliseJunctionLayout(layout);
	std::cout << "shift " << -min_x << "," << -min_y << " size " << layout.size.x << "x" << layout.size.y << "\n";
	std::cout << RenderJunctionAscii(layout);
	if (std::getenv("JUNCTION_MIRROR") != nullptr) layout = TransformJunctionLayout(layout, JunctionTransform::Mirror);
	if (const char *json = std::getenv("JUNCTION_JSON"); json != nullptr) WriteLayoutJson(layout, json);
	auto v = CheckJunctionRules(layout, spec);
	std::cout << v.size() << " violations\n";
	for (const auto &rv : v) std::cout << "  rule " << static_cast<int>(rv.rule) << " @" << rv.tile.x << "," << rv.tile.y << " (orig " << rv.tile.x + min_x << "," << rv.tile.y + min_y << "): " << rv.message << "\n";
}

TEST_CASE("Junction generator candidate sweep", "[.gen]")
{
	JunctionSpec spec = MakeTestJunctionSpec();
	if (const char *e = std::getenv("JUNCTION_GEN_TL")) spec.tl = static_cast<uint8_t>(e[0] - '0');
	if (const char *e = std::getenv("JUNCTION_GEN_SD")) spec.sd = static_cast<uint8_t>(e[0] - '0');
	if (const char *e = std::getenv("JUNCTION_GEN_G")) spec.ml_gap = static_cast<uint8_t>(e[0] - '0');
	if (const char *e = std::getenv("JUNCTION_GEN_B")) spec.branch_gap = static_cast<uint8_t>(e[0] - '0');
	if (const char *e = std::getenv("JUNCTION_GEN_PRIO")) spec.prio_tiles = static_cast<uint8_t>((e[0] - '0') * spec.tl);
	std::map<std::string, int> histogram;
	int built = 0;
	int best = 1 << 20;
	for (int h = spec.ml_gap + 5; h <= spec.ml_gap + 20; h++) {
		for (Money cc : {Money(0), spec.prices.bridge(3)}) {
			for (int s = 0; s <= 3; s++) {
				auto layout = BuildJunctionCandidate(spec, {static_cast<uint8_t>(h), cc, static_cast<uint8_t>(s), static_cast<uint8_t>(s)});
				if (!layout.has_value()) continue;
				auto v = CheckJunctionRules(*layout, spec);
				built++;
				for (const auto &rv : v) histogram[fmt::format("{} {}", static_cast<int>(rv.rule), rv.message)]++;
				std::cout << "h" << h << " cc" << cc.base() << " s" << s << ": " << v.size() << " violations\n";
				if (static_cast<int>(v.size()) < best) {
					best = static_cast<int>(v.size());
					std::cout << RenderJunctionAscii(*layout);
					for (const auto &rv : v) std::cout << "  rule " << static_cast<int>(rv.rule) << " @" << rv.tile.x << "," << rv.tile.y << ": " << rv.message << "\n";
				}
			}
		}
	}
	std::cout << "built " << built << " candidates; histogram:\n";
	for (const auto &[msg, n] : histogram) std::cout << "  " << n << "x " << msg << "\n";
	if (std::getenv("JUNCTION_GEN_SKIP") != nullptr) return;

	JunctionGenerateResult result = GenerateJunction(spec);
	REQUIRE(result.layout.has_value());
	JunctionLayout layout = *result.layout;
	std::cout << RenderJunctionAscii(layout);
	if (std::getenv("JUNCTION_MIRROR") != nullptr) layout = TransformJunctionLayout(layout, JunctionTransform::Mirror);
	if (const char *json = std::getenv("JUNCTION_JSON"); json != nullptr) WriteLayoutJson(layout, json);
}

/** The depot on the given loop: on A, its outermost axis piece with a free outward neighbour D, the half-piece track joining D's edge and A's exit edge is added to the layout. */
static CircuitDepot AddCircuitDepot(JunctionLayout &layout, const JunctionRaster &loop)
{
	auto tile = [](int x, int y) { return TileIndexDiffC{static_cast<int16_t>(x), static_cast<int16_t>(y)}; };
	/* The depot: on A, the loop's outermost axis piece with a free outward neighbour D, add the half-piece track joining D's edge and A's exit edge. */
	auto track_at = [&](TileIndexDiffC t) {
		for (const auto &jt : layout.tracks) if (jt.offset.x == t.x && jt.offset.y == t.y) return jt.tracks;
		return TrackBits{};
	};
	auto joining_track = [](DiagDirection e1, DiagDirection e2) -> std::optional<Track> {
		for (Track t : {Track::X, Track::Y, Track::Upper, Track::Lower, Track::Left, Track::Right}) {
			const Trackdir td = TrackToTrackdir(t);
			const DiagDirection a = TrackdirToExitdir(td);
			const DiagDirection b = TrackdirToExitdir(ReverseTrackdir(td));
			if ((a == e1 && b == e2) || (a == e2 && b == e1)) return t;
		}
		return std::nullopt;
	};
	const JunctionPiece *piece_a = nullptr;
	Track a_track = Track::X;
	for (const JunctionPiece &p : loop.pieces) {
		const Track axis = TrackdirToTrack(p.trackdir);
		if (p.on_bridge || (axis != Track::X && axis != Track::Y)) continue;
		if (piece_a != nullptr && p.tile.x >= piece_a->tile.x) continue;
		const TileIndexDiffC d = tile(p.tile.x - 1, p.tile.y);
		if (track_at(d).Any()) continue;
		const std::optional<Track> joint = joining_track(DiagDirection::NE, TrackdirToExitdir(p.trackdir));
		if (!joint.has_value()) continue;
		piece_a = &p;
		a_track = *joint;
	}
	if (piece_a == nullptr) FAIL("no depot site on the west loop");
	for (auto &jt : layout.tracks) {
		if (jt.offset.x == piece_a->tile.x && jt.offset.y == piece_a->tile.y) jt.tracks.Set(a_track);
	}
	return {piece_a->tile.x - 1, piece_a->tile.y, "SW"};
}

/** One-way block signals along each loop, spaced as drag-signalling with minimise gaps would (D97). */
static void AddCircuitLoopSignals(JunctionLayout &layout, const std::vector<JunctionRaster> &rasters, const JunctionSpec &spec)
{
	auto track_at = [&](TileIndexDiffC t) {
		for (const auto &jt : layout.tracks) if (jt.offset.x == t.x && jt.offset.y == t.y) return jt.tracks;
		return TrackBits{};
	};
	const uint density = spec.sd * 192u;
	for (const JunctionRaster &raster : rasters) {
		auto legal = [&](const JunctionPiece &p) { return !p.on_bridge && !TracksOverlap(track_at(p.tile)); };
		auto place = [&](const JunctionPiece &p) { layout.signals.push_back({p.tile, p.trackdir, SignalType::Block, SignalVariant::Electric}); };
		std::optional<size_t> suitable;
		uint suitable_dist = 0;
		uint dist = 0; // Logical length from the last signal to the current piece.
		bool placed_any = false;
		/* The loop's pieces, then straight on into the junction from its entry port up to the junction's first signal,
		 * so the grid carries on across both seams. */
		std::vector<JunctionPiece> seq = raster.pieces;
		{
			const JunctionPiece &last = raster.pieces.back();
			const JunctionRaster ahead = RasterizeJunctionRoute({last.tile, last.trackdir, std::string(8, 'F')});
			for (size_t a = 1; a < ahead.pieces.size(); a++) {
				const JunctionPiece &q = ahead.pieces[a];
				if (!track_at(q.tile).Test(TrackdirToTrack(q.trackdir))) break;
				seq.push_back(q);
			}
		}
		auto signalled = [&](const JunctionPiece &p) {
			return std::ranges::any_of(layout.signals, [&](const JunctionSignal &js) { return js.offset.x == p.tile.x && js.offset.y == p.tile.y && js.facing == p.trackdir; });
		};
		/* From the loop's first piece (the junction's exit port tile). */
		for (size_t k = 0; k < seq.size(); k++) {
			const JunctionPiece &p = seq[k];
			if (k >= raster.pieces.size() && signalled(p)) {
				/* The junction's own first signal ends the drag: close the last gap if it is too long. */
				if (dist + JunctionSignalWeight(seq[k - 1]) > density && suitable.has_value()) place(seq[*suitable]);
				break;
			}
			if (!placed_any) {
				/* The first legal piece gets a signal, as the drag's first tile does. */
				if (legal(p)) {
					place(p);
					placed_any = true;
					dist = 0;
				}
				continue;
			}
			dist += JunctionSignalWeight(seq[k - 1]);
			if (dist > density && suitable.has_value()) {
				place(seq[*suitable]);
				dist -= suitable_dist;
				suitable.reset();
			}
			if (dist == density) {
				if (legal(p)) {
					place(p);
					dist = 0;
					suitable.reset();
				}
			} else if (legal(p)) {
				suitable = k;
				suitable_dist = dist;
			}
		}
	}
}

TEST_CASE("Junction test circuit", "[.circuit]")
{
	JunctionSpec spec = MakeTestJunctionSpec();
	if (const char *e = std::getenv("JUNCTION_GEN_PRIO")) spec.prio_tiles = static_cast<uint8_t>((e[0] - '0') * spec.tl);
	/* JUNCTION_TMAP: wrap the T map's layout (DESIGN-O4 "G1") instead of the template generator's. */
	std::optional<JunctionLayout> generated = std::getenv("JUNCTION_TMAP") != nullptr ? ConstructJunctionTMap(spec).layout : GenerateJunction(spec).layout;
	REQUIRE(generated.has_value());
	JunctionLayout layout = *generated;
	const int w = layout.size.x;
	const int h = layout.size.y;
	const std::vector<JunctionPort> ports = FindJunctionPorts(layout);
	auto port = [&](JunctionPortRole role) -> JunctionPort {
		for (const auto &p : ports) if (p.role == role) return p;
		FAIL("missing port");
		return {};
	};

	JunctionRouteRequest base{};
	base.max_expansions = 200000;
	base.curve_spacing = spec.curve_limit ? DeriveCurveSpacing(spec.reference_speed, spec.curve_speed, spec.tl) : 1;
	base.s_bend_window = 2u * spec.tl;
	base.bridge_span_ok = spec.bridge_span_ok;
	base.max_bridge_middle = 10;
	base.prices = spec.prices;
	base.crossable_cost = 0;

	auto tile = [](int x, int y) { return TileIndexDiffC{static_cast<int16_t>(x), static_cast<int16_t>(y)}; };
	constexpr int DEPTH = 24;
	struct Loop { const char *name; JunctionPortRole out; JunctionPortRole in; TileIndexDiffC min; TileIndexDiffC max; };
	const Loop loops[] = {
		{"west", JunctionPortRole::WOut, JunctionPortRole::EIn, tile(-DEPTH, -DEPTH), tile(0, h - 1 + DEPTH)},
		{"east", JunctionPortRole::EOut, JunctionPortRole::WIn, tile(w - 1, -DEPTH), tile(w - 1 + DEPTH, h - 1 + DEPTH)},
		{"south", JunctionPortRole::SOut, JunctionPortRole::NIn, tile(-DEPTH, h - 1), tile(w - 1 + DEPTH, h - 1 + DEPTH)},
	};
	std::vector<JunctionRaster> rasters;
	for (const Loop &loop : loops) {
		const JunctionPort o = port(loop.out);
		const JunctionPort i = port(loop.in);
		JunctionRouteRequest request = base;
		request.starts = {{o.tile, o.trackdir}};
		request.goals = {{i.tile, i.trackdir}};
		request.min = loop.min;
		request.max = loop.max;
		const std::optional<JunctionRoute> route = FindJunctionRoute(layout, request);
		if (!route.has_value()) FAIL("no loop route for " << loop.name);
		JunctionRaster raster = RasterizeJunctionRoute(*route);
		if (!raster.error.empty()) FAIL("bad loop route for " << loop.name << ": " << raster.error);
		AddRasterToLayout(layout, raster);
		rasters.push_back(raster);
	}

	std::vector<CircuitDepot> depots = {AddCircuitDepot(layout, rasters[0])};

	AddCircuitLoopSignals(layout, rasters, spec);

	auto track_at = [&](TileIndexDiffC t) {
		for (const auto &jt : layout.tracks) if (jt.offset.x == t.x && jt.offset.y == t.y) return jt.tracks;
		return TrackBits{};
	};
	/* One waypoint per loop: the axis piece nearest the raster's middle, alone on its tile, with axis pieces (signalled or not) on both sides. */
	auto has_signal = [&](TileIndexDiffC t) {
		for (const auto &sg : layout.signals) if (sg.offset.x == t.x && sg.offset.y == t.y) return true;
		return false;
	};
	auto axis_piece = [&](const JunctionPiece &p) {
		const Track axis = TrackdirToTrack(p.trackdir);
		return !p.on_bridge && (axis == Track::X || axis == Track::Y);
	};
	CircuitExtras extras{};
	for (size_t li = 0; li < rasters.size(); li++) {
		const auto &pcs = rasters[li].pieces;
		const size_t mid = pcs.size() / 2;
		std::optional<size_t> best;
		for (size_t k = 1; k + 1 < pcs.size(); k++) {
			if (!axis_piece(pcs[k]) || has_signal(pcs[k].tile) || !axis_piece(pcs[k - 1]) || !axis_piece(pcs[k + 1])) continue;
			if (track_at(pcs[k].tile).Count() != 1) continue;
			const size_t dk = k > mid ? k - mid : mid - k;
			if (!best.has_value() || dk < (*best > mid ? *best - mid : mid - *best)) best = k;
		}
		if (!best.has_value()) FAIL("no waypoint site on the " << loops[li].name << " loop");
		const JunctionPiece &p = pcs[*best];
		extras.waypoints.push_back({loops[li].name, p.tile.x, p.tile.y, TrackdirToTrack(p.trackdir) == Track::X ? "X" : "Y"});
	}
	extras.box[0] = 0; extras.box[1] = 0; extras.box[2] = w; extras.box[3] = h;

	int min_x = 1 << 20, min_y = 1 << 20;
	for (const auto &jt : layout.tracks) { min_x = std::min<int>(min_x, jt.offset.x); min_y = std::min<int>(min_y, jt.offset.y); }
	for (const auto &b : layout.bridges) {
		min_x = std::min<int>({min_x, b.start.x, b.end.x});
		min_y = std::min<int>({min_y, b.start.y, b.end.y});
	}
	NormaliseJunctionLayout(layout);
	for (auto &d : depots) { d.x -= min_x; d.y -= min_y; }
	for (auto &wp : extras.waypoints) { wp.x -= min_x; wp.y -= min_y; }
	extras.box[0] -= min_x; extras.box[1] -= min_y;
	std::cout << RenderJunctionAscii(layout);
	for (const auto &d : depots) std::cout << "depot " << d.x << "," << d.y << " " << d.dir << "\n";
	for (const auto &wp : extras.waypoints) std::cout << "waypoint " << wp.name << " " << wp.x << "," << wp.y << " " << wp.axis << "\n";
	std::cout << "junction " << extras.box[0] << "," << extras.box[1] << "," << extras.box[2] << "," << extras.box[3] << "\n";
	if (const char *json = std::getenv("JUNCTION_CIRCUIT"); json != nullptr) WriteLayoutJson(layout, json, depots, &extras);
}

TEST_CASE("Junction LL_RR test circuit", "[.circuitllrr]")
{
	JunctionSpec spec = MakeTestJunctionSpec();
	spec.max_bridge_middle = 10;
	const JunctionTopology &topology = JunctionLLRRSidelineTopology();

	std::string ignored;
	const std::vector<JunctionLanePath> paths = ExpandJunctionLanes(topology, &ignored);
	const JunctionTrackGraph graph = BuildJunctionTrackGraph(topology, paths, &ignored);
	REQUIRE(!graph.edges.empty());
	const JunctionTrackOrder order = OrderJunctionTrackGraph(topology, paths, graph, &ignored);
	REQUIRE(!order.rows.empty());
	const JunctionTrackStructure structure = StructureJunctionTrackGraph(topology, paths, graph, order, &ignored);
	REQUIRE(!structure.stack.empty());
	const JunctionTrackPrograms programs = ProgramJunctionTrackGraph(topology, paths, graph, order, structure, &ignored);
	REQUIRE(!programs.edges.empty());
	JunctionTrackPlacement placement;
	const JunctionTrackStructure cut = CutJunctionTrackBridges(topology, paths, graph, order, structure, programs, spec, &placement, &ignored);
	REQUIRE(!placement.nodes.empty());
	std::vector<JunctionTrackRoute> routes;
	const JunctionLayout seed = RenderJunctionTrackPlacement(topology, paths, graph, cut, programs, placement, &ignored, &routes);
	REQUIRE(!seed.tracks.empty());

	/* The layout is the graph solved on flat ground (the port fans lean toward their nodes, D212): terrain around the seed's box with a margin of 8. */
	constexpr int MARGIN = 8;
	JunctionTerrain terrain;
	terrain.origin = {static_cast<int16_t>(-MARGIN), static_cast<int16_t>(-MARGIN)};
	terrain.width = seed.size.x + 2 * MARGIN;
	terrain.height = seed.size.y + 2 * MARGIN;
	terrain.tiles.assign(static_cast<size_t>(terrain.width) * terrain.height, JunctionTerrainTile{});
	terrain.foundation_cost = 250;
	const JunctionFacts facts(terrain);
	const JunctionNodeInput input = JunctionNodeInputOfGraph(topology, paths, graph, cut, programs, placement, routes);
	const JunctionNodeTemplate tpl = BuildJunctionNodeTemplate(input, spec);
	REQUIRE(tpl.error.empty());
	const JunctionNodeSolve solve = SolveJunctionNodes(tpl, spec, facts, {0, 0}, topology, paths, graph, programs, input);
	REQUIRE(solve.layout.has_value());
	JunctionLayout layout = *solve.layout;
	const std::vector<RuleViolation> violations = CheckJunctionRules(layout, spec, topology);
	for (const RuleViolation &v : violations) std::cout << "violation " << v.tile.x << "," << v.tile.y << " " << v.message << "\n";
	REQUIRE(violations.empty());
	const int w = layout.size.x;
	const int h = layout.size.y;

	const std::vector<JunctionPort> ports = FindJunctionPorts(layout, topology);
	REQUIRE(ports.size() == 10);
	const std::vector<JunctionPortSlot> slots = JunctionPortSlots(topology);

	JunctionRouteRequest base{};
	base.max_expansions = 200000;
	base.curve_spacing = spec.curve_limit ? DeriveCurveSpacing(spec.reference_speed, spec.curve_speed, spec.tl) : 1;
	base.s_bend_window = 2u * spec.tl;
	base.bridge_span_ok = spec.bridge_span_ok;
	base.max_bridge_middle = 10;
	base.prices = spec.prices;
	base.crossable_cost = 0;

	auto tile = [](int x, int y) { return TileIndexDiffC{static_cast<int16_t>(x), static_cast<int16_t>(y)}; };
	constexpr int DEPTH = 24;
	struct Side { const char *name; JunctionSide side; TileIndexDiffC min; TileIndexDiffC max; };
	const Side sides[] = {
		{"W", JunctionSide::W, tile(-DEPTH, -DEPTH), tile(0, h - 1 + DEPTH)},
		{"E", JunctionSide::E, tile(w - 1, -DEPTH), tile(w - 1 + DEPTH, h - 1 + DEPTH)},
		{"S", JunctionSide::S, tile(-DEPTH, h - 1), tile(w - 1 + DEPTH, h - 1 + DEPTH)},
	};
	/* The distance along the box edge: the port's row on the W and E sides, its column on the S side. */
	auto along = [&](const JunctionPort &p, JunctionSide side) { return side == JunctionSide::S ? p.tile.x : p.tile.y; };

	std::vector<JunctionRaster> rasters;
	std::optional<size_t> outer_west;
	for (const Side &side : sides) {
		std::vector<JunctionPort> ins;
		std::vector<JunctionPort> outs;
		for (const JunctionPort &p : ports) {
			const JunctionLaneGroup &group = topology.groups[slots[p.slot].group];
			if (group.side != side.side) continue;
			(group.in ? ins : outs).push_back(p);
		}
		/* Nearest first by the distance to the nearest port of the other kind; ties by position. */
		auto sort_by_gap = [&](std::vector<JunctionPort> &mine, const std::vector<JunctionPort> &other) {
			auto gap = [&](const JunctionPort &p) {
				int least = 1 << 20;
				for (const JunctionPort &o : other) least = std::min(least, std::abs(along(p, side.side) - along(o, side.side)));
				return least;
			};
			std::ranges::stable_sort(mine, [&](const JunctionPort &a, const JunctionPort &b) {
				const int ga = gap(a);
				const int gb = gap(b);
				return ga != gb ? ga < gb : along(a, side.side) < along(b, side.side);
			});
		};
		sort_by_gap(ins, outs);
		sort_by_gap(outs, ins);
		REQUIRE(ins.size() == outs.size());
		for (size_t i = 0; i < ins.size(); i++) {
			JunctionRouteRequest request = base;
			request.starts = {{outs[i].tile, outs[i].trackdir}};
			request.goals = {{ins[i].tile, ins[i].trackdir}};
			request.min = side.min;
			request.max = side.max;
			const std::optional<JunctionRoute> route = FindJunctionRoute(layout, request);
			if (!route.has_value()) FAIL("no loop route for side " << side.name << " pair " << i);
			JunctionRaster raster = RasterizeJunctionRoute(*route);
			if (!raster.error.empty()) FAIL("bad loop route for side " << side.name << " pair " << i << ": " << raster.error);
			AddRasterToLayout(layout, raster);
			rasters.push_back(raster);
			if (side.side == JunctionSide::W) outer_west = rasters.size() - 1;
		}
	}
	REQUIRE(outer_west.has_value());

	AddCircuitLoopSignals(layout, rasters, spec);
	std::vector<CircuitDepot> depots = {AddCircuitDepot(layout, rasters[*outer_west])};

	CircuitExtras extras{};
	extras.box[0] = 0; extras.box[1] = 0; extras.box[2] = w; extras.box[3] = h;

	int min_x = 1 << 20, min_y = 1 << 20;
	for (const auto &jt : layout.tracks) { min_x = std::min<int>(min_x, jt.offset.x); min_y = std::min<int>(min_y, jt.offset.y); }
	for (const auto &b : layout.bridges) {
		min_x = std::min<int>({min_x, b.start.x, b.end.x});
		min_y = std::min<int>({min_y, b.start.y, b.end.y});
	}
	NormaliseJunctionLayout(layout);
	for (auto &d : depots) { d.x -= min_x; d.y -= min_y; }
	extras.box[0] -= min_x; extras.box[1] -= min_y;
	std::cout << RenderJunctionAscii(layout);
	for (const auto &d : depots) std::cout << "depot " << d.x << "," << d.y << " " << d.dir << "\n";
	std::cout << "junction " << extras.box[0] << "," << extras.box[1] << "," << extras.box[2] << "," << extras.box[3] << "\n";
	std::cout << "loops " << rasters.size() << " size " << layout.size.x << "x" << layout.size.y << "\n";
	if (const char *json = std::getenv("JUNCTION_CIRCUIT"); json != nullptr) WriteLayoutJson(layout, json, depots, &extras);
}

TEST_CASE("Junction T map export", "[.tmapjson]")
{
	const JunctionTMapResult result = ConstructJunctionTMap(MakeTestJunctionSpec());
	REQUIRE(result.layout.has_value());
	CHECK(result.clean);
	JunctionLayout layout = *result.layout;
	std::cout << RenderJunctionAscii(layout);
	if (std::getenv("JUNCTION_MIRROR") != nullptr) layout = TransformJunctionLayout(layout, JunctionTransform::Mirror);
	if (const char *json = std::getenv("JUNCTION_JSON"); json != nullptr) WriteLayoutJson(layout, json);
}

TEST_CASE("Junction T map over a ridge export", "[.ridgejson]")
{
	/* The terrain of the ridge test in junction_map.cpp (G3c): flat at 0, rising over row R = b + 6, level at 1 over R + 1 and R + 2, falling over R + 3. */
	const JunctionSpec spec = MakeTestJunctionSpec();
	const JunctionFrame frame = MakeJunctionFrame(spec);
	constexpr int MARGIN = 4;
	JunctionTerrain terrain;
	terrain.origin = {static_cast<int16_t>(-MARGIN), static_cast<int16_t>(frame.terrain_origin_y - MARGIN)};
	terrain.width = frame.width + 2 * MARGIN;
	terrain.height = frame.terrain_height + 2 * MARGIN;
	terrain.tiles.assign(static_cast<size_t>(terrain.width) * terrain.height, JunctionTerrainTile{});
	terrain.foundation_cost = 250;
	terrain.build_on_slopes = true;
	const int ridge = spec.ml_gap + 2 + 6;
	auto set_row = [&](int y, int z_min, int z_max, DiagDirection incline) {
		for (int i = 0; i < terrain.width; i++) {
			JunctionTerrainTile &t = terrain.tiles[static_cast<size_t>(y - terrain.origin.y) * terrain.width + i];
			t.z_min = static_cast<int8_t>(z_min);
			t.z_max = static_cast<int8_t>(z_max);
			t.incline = incline;
			t.slope = incline == DiagDirection::Invalid ? SLOPE_FLAT : InclinedSlope(incline);
		}
	};
	set_row(ridge, 0, 1, DiagDirection::SE);
	set_row(ridge + 1, 1, 1, DiagDirection::Invalid);
	set_row(ridge + 2, 1, 1, DiagDirection::Invalid);
	set_row(ridge + 3, 0, 1, DiagDirection::NW);

	const JunctionTMapResult flat = ConstructJunctionTMap(spec, nullptr, std::nullopt, true);
	REQUIRE(flat.layout.has_value());
	const JunctionTMapResult result = ConstructJunctionTMap(spec, &terrain, flat.height, true);
	INFO(result.log);
	REQUIRE(result.layout.has_value());
	CHECK(result.clean);
	JunctionLayout layout = *result.layout;

	/* Frame to layout shift, from the S stub's last tile: frame (xO, height - 1) is the bottom row's Y track with the larger x. */
	const int x_o = frame.hub_x + spec.branch_gap + 1;
	int bottom = -1, s_x = -1;
	for (const auto &jt : layout.tracks) bottom = std::max<int>(bottom, jt.offset.y);
	for (const auto &jt : layout.tracks) if (jt.offset.y == bottom && jt.tracks.Test(Track::Y)) s_x = std::max<int>(s_x, jt.offset.x);
	const int dx = s_x - x_o;
	const int dy = bottom - (result.height - 1);
	std::cout << RenderJunctionAscii(layout);
	std::cout << "height " << result.height << " shift " << dx << "," << dy << " size " << layout.size.x << "x" << layout.size.y << "\n";
	std::cout << "raise_rows " << ridge + 1 + dy << " " << ridge + 2 + dy << "\n";
	if (const char *json = std::getenv("JUNCTION_JSON"); json != nullptr) WriteLayoutJson(layout, json);
}
