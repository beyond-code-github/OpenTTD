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
#include "../junction_route.h"
#include "../junction_router.h"
#include "../junction_rules.h"
#include "../track_func.h"
#include "../core/format.hpp"

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

static void WriteLayoutJson(const JunctionLayout &layout, const char *path, const std::vector<CircuitDepot> &depots = {})
{
	static const char *track_names[] = {"X", "Y", "Upper", "Lower", "Left", "Right"};
	static const char *td_names[] = {"X_NE", "Y_SE", "Upper_E", "Lower_E", "Left_S", "Right_S", "", "", "X_SW", "Y_NW", "Upper_W", "Lower_W", "Left_N", "Right_N"};
	static const char *type_names[] = {"block", "entry", "exit", "combo", "path", "path_oneway"};
	std::ofstream out(path);
	out << "{\"size\":[" << layout.size.x << "," << layout.size.y << "],\n\"tracks\":[";
	bool first = true;
	for (const auto &jt : layout.tracks) {
		out << (first ? "" : ",") << "\n{\"x\":" << jt.offset.x << ",\"y\":" << jt.offset.y << ",\"bits\":[";
		bool fb = true;
		for (Track t : jt.tracks) { out << (fb ? "" : ",") << "\"" << track_names[to_underlying(t)] << "\""; fb = false; }
		out << "]}";
		first = false;
	}
	out << "],\n\"bridges\":[";
	first = true;
	for (const auto &b : layout.bridges) {
		out << (first ? "" : ",") << "{\"start\":[" << b.start.x << "," << b.start.y << "],\"end\":[" << b.end.x << "," << b.end.y << "]}";
		first = false;
	}
	out << "],\n\"signals\":[";
	first = true;
	for (const auto &sg : layout.signals) {
		out << (first ? "" : ",") << "\n{\"x\":" << sg.offset.x << ",\"y\":" << sg.offset.y << ",\"facing\":\"" << td_names[to_underlying(sg.facing)] << "\",\"type\":\"" << type_names[to_underlying(sg.type)] << "\"}";
		first = false;
	}
	out << "]";
	if (!depots.empty()) {
		out << ",\n\"depots\":[";
		first = true;
		for (const auto &d : depots) {
			out << (first ? "" : ",") << "{\"x\":" << d.x << ",\"y\":" << d.y << ",\"dir\":\"" << d.dir << "\"}";
			first = false;
		}
		out << "]";
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
	std::map<std::string, int> histogram;
	int built = 0;
	int best = 1 << 20;
	for (int h = spec.ml_gap + 5; h <= spec.ml_gap + 20; h++) {
		for (Money cc : {Money(0), spec.prices.bridge(3)}) {
			for (int s = 0; s <= 3; s++) {
				auto layout = BuildJunctionCandidate(spec, {static_cast<uint8_t>(h), cc, static_cast<uint8_t>(s)});
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

TEST_CASE("Junction test circuit", "[.circuit]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	JunctionGenerateResult result = GenerateJunction(spec);
	REQUIRE(result.layout.has_value());
	JunctionLayout layout = *result.layout;
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

	/* The depot: on A, the west loop's outermost axis piece with a free outward neighbour D, add the half-piece track joining D's edge and A's exit edge. */
	const JunctionRaster &west = rasters[0];
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
	for (const JunctionPiece &p : west.pieces) {
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
	std::vector<CircuitDepot> depots = {{piece_a->tile.x - 1, piece_a->tile.y, "SW"}};

	/* One-way block signals along each loop. */
	auto track_count = [&](TileIndexDiffC t) {
		for (const auto &jt : layout.tracks) if (jt.offset.x == t.x && jt.offset.y == t.y) return static_cast<int>(jt.tracks.Count());
		return 0;
	};
	for (const JunctionRaster &raster : rasters) {
		uint acc = 0;
		bool first_signal = true;
		for (size_t k = 2; k + 1 < raster.pieces.size(); k++) {
			const JunctionPiece &p = raster.pieces[k];
			acc += p.units;
			if (!first_signal && acc < 2u * spec.sd) continue;
			if (p.on_bridge || track_count(p.tile) != 1) continue;
			layout.signals.push_back({p.tile, p.trackdir, SignalType::Block, SignalVariant::Electric});
			first_signal = false;
			acc = 0;
		}
	}

	int min_x = 1 << 20, min_y = 1 << 20;
	for (const auto &jt : layout.tracks) { min_x = std::min<int>(min_x, jt.offset.x); min_y = std::min<int>(min_y, jt.offset.y); }
	for (const auto &b : layout.bridges) {
		min_x = std::min<int>({min_x, b.start.x, b.end.x});
		min_y = std::min<int>({min_y, b.start.y, b.end.y});
	}
	NormaliseJunctionLayout(layout);
	for (auto &d : depots) { d.x -= min_x; d.y -= min_y; }
	std::cout << RenderJunctionAscii(layout);
	for (const auto &d : depots) std::cout << "depot " << d.x << "," << d.y << " " << d.dir << "\n";
	if (const char *json = std::getenv("JUNCTION_CIRCUIT"); json != nullptr) WriteLayoutJson(layout, json, depots);
}
